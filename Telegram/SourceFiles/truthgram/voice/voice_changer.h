// voice_changer.h
//
// TruthGram Voice FX — real-time DSP hook into tg_owt / Media::Capture audio
// input buffers, applied BEFORE Opus encoding. Presets:
//   Deep / Anonymous, Pitch Shift (+/- semitones), Robotic / Vocoder,
//   Radio / Walkie-Talkie, Reverb, Custom EQ.
// Plus a local "Test Mic" loopback toggle for monitoring.
//
// The hook patches into tg_owt's AudioProcessing::ProcessCaptureFrameEx
// (see patches/tg_owt/0001-truthgram-audio-hook.patch). TruthGram installs a
// VoiceChanger::processFrame callback that tg_owt calls per 10 ms frame.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QAudioFormat>
#include <QIODevice>
#include <QPointer>

#include <atomic>
#include <complex>
#include <memory>
#include <span>
#include <vector>
#include <mutex>

namespace tg::truthgram::voice {

// Real-time-safe parameters for the DSP chain. All fields are POD; the
// audio thread reads them under a single std::atomic snapshot.
struct alignas(64) VoiceParams {
    float pitchSemitones = 0.0f;   // -12..+12
    float formantShift   = 1.0f;   // 0.5..2.0 (timbre without pitch)
    float reverbMix      = 0.0f;   // 0..1
    float reverbRoom     = 0.5f;   // 0..1 (decay)
    float vocoderMix     = 0.0f;   // 0..1 (robotic)
    float radioBandwidth = 1.0f;   // 0..1 (1=full, 0.1=walkie)
    // Custom EQ: 5-band gains in dB (-12..+12).
    float eqBand[5] = {0, 0, 0, 0, 0};
    float inputGainDb = 0.0f;      // -20..+20
    bool  bypass = true;           // start in bypass
};

enum class Preset {
    Bypass,
    DeepAnonymous,
    PitchUp,
    PitchDown,
    Robotic,
    Radio,
    Reverb,
    Custom,
};

Q_NAMESPACE

// =============================================================================
// VoiceChanger
// =============================================================================
// Singleton. The tg_owt patch calls processFrame() from the audio capture
// thread; everything in processFrame() must be allocation-free and lock-free.
class VoiceChanger : public QObject {
    Q_OBJECT
public:
    static VoiceChanger& instance();

    // Apply a named preset. Copies the params under a mutex and publishes a
    // cloud_sync dirty marker.
    void applyPreset(Preset p);

    // Live parameter tweak (used by the Voice FX popover sliders).
    void setParams(const VoiceParams& p);
    [[nodiscard]] VoiceParams params() const;

    [[nodiscard]] Preset currentPreset() const;

    // The hot-path entry point called by tg_owt per 10 ms frame.
    //   frame: interleaved int16 samples (mono, 48 kHz, 480 samples).
    // Thread-safe, allocation-free, takes no Qt locks.
    void processFrame(std::span<std::int16_t> frame);

    // ---- Loopback / Test Mic ----
    // Connects the capture sink to the default output device so the user can
    // hear their own voice through the active DSP chain. Toggled by the
    // Voice FX popover.
    void setLoopback(bool on);
    [[nodiscard]] bool loopback() const;

Q_SIGNALS:
    void paramsChanged(const VoiceParams& p);
    void presetChanged(Preset p);
    void loopbackChanged(bool on);

private:
    VoiceChanger();
    ~VoiceChanger() override;
    VoiceChanger(const VoiceChanger&) = delete;
    VoiceChanger& operator=(const VoiceChanger&) = delete;

    struct Dsp;
    struct State;
    std::unique_ptr<State> _s;

    // Per-stage DSP primitives (real-time safe).
    void applyInputGain(std::span<float> f, float db);
    void applyEq(std::span<float> f);          // 5-band biquad
    void applyPitch(std::span<float> f);        // PSOLA-lite overlap-add
    void applyVocoder(std::span<float> f);      // carrier-modulated robotic
    void applyRadio(std::span<float> f);        // bandpass + soft clip
    void applyReverb(std::span<float> f);       // feedback comb network
    void applyFormant(std::span<float> f);      // simple spectral envelope warp

    // Convert the int16 frame to float, run the chain, convert back.
    void processFloat(std::span<float> f);

    Preset _lastPreset{Preset::Bypass};
};

inline VoiceChanger& voice() { return VoiceChanger::instance(); }

} // namespace tg::truthgram::voice
