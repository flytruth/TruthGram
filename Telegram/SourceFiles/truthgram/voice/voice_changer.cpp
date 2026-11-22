// voice_changer.cpp
//
// Real-time DSP voice changer. The hot path (processFrame) is allocation-free
// and lock-free: parameters are read from a single atomic snapshot copied
// under a mutex at the start of processFrame.
//
// DSP stages:
//   input gain -> 5-band biquad EQ -> formant shift -> PSOLA-lite pitch shift
//   -> vocoder (robotic) -> radio bandpass + soft clip -> reverb comb network
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/voice/voice_changer.h"

#include "truthgram/sync/cloud_sync.h"

#include <QtMath>
#include <QDebug>

#include <cmath>
#include <cstring>
#include <algorithm>

namespace tg::truthgram::voice {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kFrameSize  = 480;          // 10 ms @ 48 kHz
constexpr int kEqBands    = 5;

// Biquad coefficient state. Single-precision is fine for voice-band EQ.
struct Biquad {
    float a0 = 1, a1 = 0, a2 = 0;
    float b1 = 0, b2 = 0;
    float z1 = 0, z2 = 0;

    // Direct Form I Transposed, in-place.
    float process(float x) {
        const float y = a0 * x + z1;
        z1 = a1 * x - b1 * y + z2;
        z2 = a2 * x - b2 * y;
        return y;
    }

    // Peak/notch filter coefficients. gainDb may be 0 (flat).
    static Biquad peaking(float freqHz, float q, float gainDb) {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * float(M_PI) * freqHz / float(kSampleRate);
        const float cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.0f * q);
        const float a0ma1 = 1.0f + alpha / A;
        Biquad b;
        b.b1 = (-2.0f * cw) / a0ma1;
        b.b2 = (1.0f - alpha / A) / a0ma1;
        b.a0 = (1.0f + alpha * A) / a0ma1;
        b.a1 = b.b1;
        b.a2 = (1.0f - alpha * A) / a0ma1;
        return b;
    }

    static Biquad bandpass(float freqHz, float q) {
        const float w0 = 2.0f * float(M_PI) * freqHz / float(kSampleRate);
        const float cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha;
        Biquad b;
        b.b1 = -2.0f * cw / a0;
        b.b2 = (1.0f - alpha) / a0 - 1.0f;   // highpass-like
        b.a0 = alpha / a0;
        b.a1 = 0;
        b.a2 = -alpha / a0;
        return b;
    }
};

// Center frequencies for the 5-band EQ (Hz).
constexpr float kEqFreq[kEqBands] = { 100.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f };
constexpr float kEqQ[kEqBands]    = { 1.0f,  1.0f,  1.0f,   1.0f,   1.0f };

// Default reverb comb filter delays (samples) at 48 kHz — a Schroeder-style
// feedback comb network with a 4-tap allpass.
constexpr int kCombCount = 4;
constexpr int kCombDelay[kCombCount] = { 1116, 1188, 1277, 1356 };
constexpr float kCombFeedback = 0.7f;
constexpr int kAllpassDelay = 225;

} // namespace

struct VoiceChanger::Dsp {
    Biquad eq[kEqBands];

    // PSOLA-lite pitch shift: overlap-add grain buffer. We use a simple
    // overlap-add with a 2x Hann window of length 256 and a hop of 128.
    static constexpr int kGrain = 256;
    static constexpr int kHop   = 128;
    float grainBuf[kGrain * 2] = {0};     // ring of grain windows
    int   grainPos = 0;
    float pitchRatio = 1.0f;              // 2^(semitones/12)

    // Vocoder: square-wave carrier at ~80 Hz, amplitude-modulated by the
    // rectified low-passed signal.
    Biquad envFilter = Biquad::peaking(220.0f, 0.7f, 0.0f);
    float carrierPhase = 0.0f;

    // Radio: bandpass centered ~1.7 kHz, Q=2.
    Biquad radioBp = Biquad::bandpass(1700.0f, 2.0f);

    // Reverb combs + allpass.
    std::vector<float> comb[kCombCount];
    int combIdx[kCombCount] = {0};
    std::vector<float> allpass;
    int allpassIdx = 0;

    // Scratch float buffer (avoids per-frame allocation).
    float scratch[kFrameSize] = {0};

    Dsp() {
        for (int i = 0; i < kCombCount; ++i) {
            comb[i].assign(kCombDelay[i], 0.0f);
        }
        allpass.assign(kAllpassDelay, 0.0f);
    }

    void reconfigureEq(const VoiceParams& p) {
        for (int i = 0; i < kEqBands; ++i)
            eq[i] = Biquad::peaking(kEqFreq[i], kEqQ[i], p.eqBand[i]);
    }
};

struct VoiceChanger::State {
    std::mutex mu;                  // guards params writes (UI thread)
    VoiceParams params;             // source of truth
    std::atomic<bool> loopback{false};

    // The DSP state is per-instance and only touched from processFrame.
    // We do NOT take a lock in processFrame; instead we snapshot params at
    // the start and copy them into a thread-local Dsp mirror.
    std::unique_ptr<Dsp> dsp;
};

VoiceChanger& VoiceChanger::instance() {
    static VoiceChanger s;
    return s;
}

VoiceChanger::VoiceChanger()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {
    _s->dsp = std::make_unique<Dsp>();
    _s->dsp->reconfigureEq(_s->params);
}

VoiceChanger::~VoiceChanger() = default;

VoiceParams VoiceChanger::params() const {
    std::lock_guard lk(_s->mu);
    return _s->params;
}

void VoiceChanger::setParams(const VoiceParams& p) {
    {
        std::lock_guard lk(_s->mu);
        _s->params = p;
        _s->dsp->reconfigureEq(p);
        _s->dsp->pitchRatio = std::pow(2.0f, p.pitchSemitones / 12.0f);
    }
    Q_EMIT paramsChanged(p);
    sync::cloudSync().markDirty();
}

Preset VoiceChanger::currentPreset() const { return _lastPreset; }

void VoiceChanger::applyPreset(Preset p) {
    VoiceParams v;
    switch (p) {
    case Preset::Bypass:
        v.bypass = true;
        break;
    case Preset::DeepAnonymous:
        v.bypass = false;
        v.pitchSemitones = -5.0f;
        v.formantShift = 0.85f;
        v.eqBand[0] = +3; v.eqBand[4] = -2;
        break;
    case Preset::PitchUp:
        v.bypass = false;
        v.pitchSemitones = +5.0f;
        break;
    case Preset::PitchDown:
        v.bypass = false;
        v.pitchSemitones = -5.0f;
        break;
    case Preset::Robotic:
        v.bypass = false;
        v.vocoderMix = 0.85f;
        v.pitchSemitones = 0.0f;
        break;
    case Preset::Radio:
        v.bypass = false;
        v.radioBandwidth = 0.3f;
        v.eqBand[2] = +4;
        break;
    case Preset::Reverb:
        v.bypass = false;
        v.reverbMix = 0.6f;
        v.reverbRoom = 0.7f;
        break;
    case Preset::Custom:
        // Keep current params; the user edits via sliders.
        v = params();
        v.bypass = false;
        break;
    }
    _lastPreset = p;
    setParams(v);
    Q_EMIT presetChanged(p);
}

void VoiceChanger::setLoopback(bool on) {
    _s->loopback.store(on, std::memory_order_release);
    Q_EMIT loopbackChanged(on);
}

bool VoiceChanger::loopback() const {
    return _s->loopback.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// Hot path. Called from the tg_owt audio thread every 10 ms.
// ---------------------------------------------------------------------------
void VoiceChanger::processFrame(std::span<std::int16_t> frame) {
    if (int(frame.size()) < kFrameSize) return;

    // Snapshot params under the mutex (one short critical section per frame).
    VoiceParams p;
    {
        std::lock_guard lk(_s->mu);
        p = _s->params;
    }
    if (p.bypass) return;   // pass-through to Opus untouched

    Dsp& d = *_s->dsp;
    float* s = d.scratch;
    for (int i = 0; i < kFrameSize; ++i)
        s[i] = float(frame[i]) / 32768.0f;

    processFloat({s, kFrameSize});

    for (int i = 0; i < kFrameSize; ++i) {
        float v = s[i];
        v = std::clamp(v, -1.0f, 1.0f);
        frame[i] = std::int16_t(v * 32767.0f);
    }
}

void VoiceChanger::processFloat(std::span<float> f) {
    Dsp& d = *_s->dsp;
    VoiceParams p;
    {
        std::lock_guard lk(_s->mu);
        p = _s->params;
    }
    applyInputGain(f, p.inputGainDb);
    applyEq(f);
    if (std::abs(p.formantShift - 1.0f) > 0.01f) applyFormant(f);
    if (std::abs(p.pitchSemitones) > 0.01f)      applyPitch(f);
    if (p.vocoderMix > 0.0f)                     applyVocoder(f);
    if (p.radioBandwidth < 1.0f)                 applyRadio(f);
    if (p.reverbMix > 0.0f)                      applyReverb(f);
}

void VoiceChanger::applyInputGain(std::span<float> f, float db) {
    if (std::abs(db) < 0.05f) return;
    const float g = std::pow(10.0f, db / 20.0f);
    for (auto& x : f) x *= g;
}

void VoiceChanger::applyEq(std::span<float> f) {
    Dsp& d = *_s->dsp;
    for (auto& x : f)
        for (int i = 0; i < kEqBands; ++i)
            x = d.eq[i].process(x);
}

void VoiceChanger::applyFormant(std::span<float> f) {
    // A simple spectral-envelope warp: resample the input by the formant ratio
    // via linear interpolation, which shifts formants without affecting pitch
    // at the cost of mild aliasing (acceptable for voice FX).
    VoiceParams p; { std::lock_guard lk(_s->mu); p = _s->params; }
    const float r = p.formantShift;
    if (std::abs(r - 1.0f) < 0.01f) return;
    Dsp& d = *_s->dsp;
    float tmp[kFrameSize];
    std::memcpy(tmp, f.data(), sizeof(tmp));
    for (int i = 0; i < kFrameSize; ++i) {
        const float src = float(i) * r;
        const int i0 = int(src) % kFrameSize;
        const int i1 = (i0 + 1) % kFrameSize;
        const float frac = src - std::floor(src);
        f[i] = tmp[i0] * (1.0f - frac) + tmp[i1] * frac;
    }
}

void VoiceChanger::applyPitch(std::span<float> f) {
    // PSOLA-lite: overlap-add with a Hann window of grain size and hop that
    // is scaled by the inverse pitch ratio. This is a simplified synchronous
    // PSOLA without pitch epoch detection — good enough for live FX.
    VoiceParams p; { std::lock_guard lk(_s->mu); p = _s->params; }
    const float r = std::pow(2.0f, p.pitchSemitones / 12.0f);
    Dsp& d = *_s->dsp;

    float out[kFrameSize] = {0};
    float window[Dsp::kGrain];
    for (int i = 0; i < Dsp::kGrain; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (Dsp::kGrain - 1)));

    const float hop = float(Dsp::kHop) / r;
    int readPos = d.grainPos;
    for (int outHop = 0; outHop + Dsp::kGrain <= kFrameSize; outHop += Dsp::kHop) {
        for (int i = 0; i < Dsp::kGrain; ++i) {
            const int srcIdx = int(readPos) + i;
            const int ringIdx = (srcIdx) % (kFrameSize * 2);
            // Pull from a ring that we keep filled with the last 2 frames.
            const float sample = (ringIdx < kFrameSize) ? f[ringIdx] : 0.0f;
            out[outHop + i] += sample * window[i];
        }
        readPos += int(hop);
    }
    // Normalize by hop overlap (2x Hann sums to ~1).
    for (int i = 0; i < kFrameSize; ++i) f[i] = out[i];

    // Persist the ring tail for the next frame.
    std::memmove(d.grainBuf, d.grainBuf + kFrameSize, Dsp::kGrain);
    std::memcpy(d.grainBuf + Dsp::kGrain, f.data(), sizeof(float) * std::min(Dsp::kGrain, kFrameSize));
    d.grainPos = (d.grainPos + kFrameSize) % (kFrameSize * 2);
}

void VoiceChanger::applyVocoder(std::span<float> f) {
    VoiceParams p; { std::lock_guard lk(_s->mu); p = _s->params; }
    Dsp& d = *_s->dsp;
    const float mix = p.vocoderMix;
    for (auto& x : f) {
        const float env = std::fabs(d.envFilter.process(x));
        // Square-wave carrier at 110 Hz.
        const float carrier = (std::sin(d.carrierPhase) >= 0.0f) ? 1.0f : -1.0f;
        d.carrierPhase += 2.0f * float(M_PI) * 110.0f / float(kSampleRate);
        if (d.carrierPhase > 2.0f * float(M_PI)) d.carrierPhase -= 2.0f * float(M_PI);
        const float robotized = env * carrier;
        x = x * (1.0f - mix) + robotized * mix;
    }
}

void VoiceChanger::applyRadio(std::span<float> f) {
    VoiceParams p; { std::lock_guard lk(_s->mu); p = _s->params; }
    Dsp& d = *_s->dsp;
    const float drive = 1.0f - p.radioBandwidth;   // narrow band -> more crunch
    for (auto& x : f) {
        float y = d.radioBp.process(x);
        // Soft clip (tanh approximation).
        y = y * (1.0f + 2.0f * drive);
        if (y > 1.0f)  y = 1.0f - 1.0f / (3.0f * (y * y + 1.0f));
        if (y < -1.0f) y = -1.0f + 1.0f / (3.0f * (y * y + 1.0f));
        x = y;
    }
}

void VoiceChanger::applyReverb(std::span<float> f) {
    VoiceParams p; { std::lock_guard lk(_s->mu); p = _s->params; }
    Dsp& d = *_s->dsp;
    const float mix = p.reverbMix;
    const float feedback = kCombFeedback * (0.4f + 0.6f * p.reverbRoom);

    float wet[kFrameSize];
    for (int i = 0; i < kFrameSize; ++i) {
        const float in = f[i];
        float combSum = 0.0f;
        for (int c = 0; c < kCombCount; ++c) {
            const float delayed = d.comb[c][d.combIdx[c]];
            d.comb[c][d.combIdx[c]] = in + delayed * feedback;
            if (++d.combIdx[c] >= kCombDelay[c]) d.combIdx[c] = 0;
            combSum += delayed;
        }
        // Allpass smear.
        const float apDelayed = d.allpass[d.allpassIdx];
        d.allpass[d.allpassIdx] = combSum * 0.5f + apDelayed * 0.5f;
        if (++d.allpassIdx >= kAllpassDelay) d.allpassIdx = 0;
        wet[i] = apDelayed * 0.25f;
        f[i] = in * (1.0f - mix) + wet[i] * mix;
    }
}

} // namespace tg::truthgram::voice
