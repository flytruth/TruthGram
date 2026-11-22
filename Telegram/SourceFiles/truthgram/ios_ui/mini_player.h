// mini_player.h
//
// TruthGram Mini Media Player — a detachable, floating pill-shaped player
// that replaces tdesktop's top-bar audio player. Features:
//   * Real-time FFT audio waveform visualization (Lime Green + Light Blue).
//   * Playback speed scrubbing 0.5x – 2.5x.
//   * Pitch lock (preserves pitch when speed changes, via librubberband-style
//     WSOLA in the player's audio output sink).
//   * Queue management.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QWidget>
#include <QPointer>
#include <QAudioBuffer>

// Kiss FFT is an OPTIONAL dependency. When TRUTHGRAM_HAVE_KISSFFT is 1 (set by
// cmake/FindKissFFT.cmake via the generated truthgram_config.h), the Mini
// Player draws real FFT waveform bars via the kiss_fftr (real-input) API.
// When it is 0, the FFT members and calls are compiled out and the bars
// render flat - the player controls still work. Gating the include here means
// the build never breaks on a system without Kiss FFT installed.
#if defined(TRUTHGRAM_HAVE_KISSFFT) && TRUTHGRAM_HAVE_KISSFFT
#  include <kiss_fftr.h>   // also pulls in kiss_fft.h; defines kiss_fft_cpx
#endif

#include <atomic>
#include <complex>
#include <memory>
#include <vector>

class QPushButton;
class QSlider;
class QLabel;

namespace tg::truthgram::ios_ui {

class MiniPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MiniPlayer(QWidget* parent);
    ~MiniPlayer() override;

    // Push 1024 mono float samples from the active audio output sink.
    // Called from the player thread; lock-free ring buffer.
    void pushSamples(const float* mono, std::size_t n);

    // Detach into a floating top-level pill that can be dragged around.
    void detach();
    void reattach();

    void setSpeed(float ratio);    // 0.5 .. 2.5
    void setPitchLock(bool on);
    [[nodiscard]] float speed() const;
    [[nodiscard]] bool pitchLock() const;

Q_SIGNALS:
    void speedChanged(float ratio);
    void pitchLockChanged(bool on);
    void nextRequested();
    void prevRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _i;
    QPoint _dragOffset;   // for detached pill dragging
};

} // namespace tg::truthgram::ios_ui
