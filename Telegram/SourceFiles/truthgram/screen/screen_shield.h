// screen_shield.h
//
// TruthGram Screen Shield — OS-level screenshot / screen-capture protection.
//
//   * Windows:  SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE) on the
//               main window. Captures show a black rectangle.
//   * macOS:    NSWindow.sharingType = NSWindowSharingNone.
//   * Linux:    Wayland layer-surface "exclusive" hint + X11 _NET_WM state
//               override_redirect toggling (best-effort; compositors vary).
//   * Streamer / Anonymize Mode: blurs usernames, phone numbers, and avatars
//     on the fly via a paint hook on HistoryWidget / DialogsWidget.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QPointer>

#include <atomic>
#include <memory>

class QWidget;
class QMainWindow;

namespace tg::truthgram::screen {

enum class CaptureProtection {
    None,
    ExcludeFromCapture,   // Windows WDA_EXCLUDFROMCAPTURE / macOS NSWindowSharingNone
    BlankOnCapture,       // fallback: paint black when capture is detected
};

Q_NAMESPACE

// =============================================================================
// ScreenShield
// =============================================================================
class ScreenShield : public QObject {
    Q_OBJECT
public:
    static ScreenShield& instance();

    // Install the OS-level capture protection on the main window. Called once
    // at startup with the QApplication's active window.
    void installOn(QWidget* mainWindow);

    // Toggle capture protection at runtime (the title-bar quick-lock icon).
    void setEnabled(bool on);
    [[nodiscard]] bool enabled() const;

    // Streamer / Anonymize Mode: blur names/phones/avatars on paint.
    void setStreamerMode(bool on);
    [[nodiscard]] bool streamerMode() const;

    // Whether the OS supports WDA_EXCLUDEFROMCAPTURE (Win10 2004+) or
    // NSWindowSharingNone. On Linux this returns false and we fall back to
    // BlankOnCapture.
    [[nodiscard]] static CaptureProtection platformCapability();

Q_SIGNALS:
    void enabledChanged(bool on);
    void streamerModeChanged(bool on);

private:
    ScreenShield();
    ~ScreenShield() override;
    ScreenShield(const ScreenShield&) = delete;
    ScreenShield& operator=(const ScreenShield&) = delete;

    struct State;
    std::unique_ptr<State> _s;

    void applyToWindow(QWidget* w, bool on);
};

inline ScreenShield& shield() { return ScreenShield::instance(); }

} // namespace tg::truthgram::screen
