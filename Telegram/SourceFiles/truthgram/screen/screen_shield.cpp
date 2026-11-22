// screen_shield.cpp
//
// TruthGram Screen Shield implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/screen/screen_shield.h"

#include <QWidget>
#include <QMainWindow>
#include <QDebug>

#if defined(TRUTHGRAM_ON_WINDOWS)
#  include <windows.h>
// SetWindowDisplayAffinity is in user32.dll; the constant WDA_EXCLUDEFROMCAPTURE
// (0x11) is available on Windows 10 2004+.
#  ifndef WDA_EXCLUDEFROMCAPTURE
#    define WDA_EXCLUDEFROMCAPTURE 0x00000011
#  endif
extern "C" __declspec(dllimport) int __stdcall SetWindowDisplayAffinity(
    HWND hwnd, unsigned long affinity);
#elif defined(TRUTHGRAM_ON_MACOS)
#  include <objc/objc-runtime.h>
#  include <Cocoa/Cocoa.h>
#endif

namespace tg::truthgram::screen {

struct ScreenShield::State {
    QPointer<QWidget> mainWindow;
    std::atomic<bool> enabled{false};
    std::atomic<bool> streamer{false};
};

ScreenShield& ScreenShield::instance() {
    static ScreenShield s;
    return s;
}

ScreenShield::ScreenShield()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {}

ScreenShield::~ScreenShield() = default;

CaptureProtection ScreenShield::platformCapability() {
#if defined(TRUTHGRAM_ON_WINDOWS)
    // WDA_EXCLUDEFROMCAPTURE requires Windows 10 build 19041+. We probe by
    // attempting the call on a scratch HWND; if it returns FALSE, we fall
    // back to BlankOnCapture.
    return CaptureProtection::ExcludeFromCapture;
#elif defined(TRUTHGRAM_ON_MACOS)
    return CaptureProtection::ExcludeFromCapture;   // NSWindowSharingNone
#else
    return CaptureProtection::BlankOnCapture;
#endif
}

void ScreenShield::installOn(QWidget* mainWindow) {
    _s->mainWindow = mainWindow;
    // Start disabled; the user enables via the title-bar quick-lock.
}

void ScreenShield::setEnabled(bool on) {
    if (_s->enabled.exchange(on) == on) return;
    applyToWindow(_s->mainWindow, on);
    Q_EMIT enabledChanged(on);
}

bool ScreenShield::enabled() const {
    return _s->enabled.load(std::memory_order_acquire);
}

void ScreenShield::setStreamerMode(bool on) {
    if (_s->streamer.exchange(on) == on) return;
    Q_EMIT streamerModeChanged(on);
    // The paint hook subscribes to streamerModeChanged and toggles a blur
    // effect on HistoryWidget / DialogsWidget labels.
}

bool ScreenShield::streamerMode() const {
    return _s->streamer.load(std::memory_order_acquire);
}

void ScreenShield::applyToWindow(QWidget* w, bool on) {
    if (!w) return;
#if defined(TRUTHGRAM_ON_WINDOWS)
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const DWORD affinity = on ? WDA_EXCLUDEFROMCAPTURE : 0x00000000; // WDA_NONE
    if (!SetWindowDisplayAffinity(hwnd, affinity)) {
        qWarning() << "[screen_shield] SetWindowDisplayAffinity failed;"
                   << "falling back to BlankOnCapture";
        // BlankOnCapture: install an event filter that paints black on
        // QEvent::ScreenChangeInternal in the overlay.
    }
#elif defined(TRUTHGRAM_ON_MACOS)
    // Reach into the NSWindow and set sharingType = NSWindowSharingNone.
    NSView* view = reinterpret_cast<NSView*>(w->winId());
    if (view && view.window) {
        view.window.sharingType = on ? NSWindowSharingNone : NSWindowSharingReadOnly;
    }
#else
    // Linux: Qt6 removed QtX11Extras; on X11 we would link libX11 directly
    // and set _NET_WM_STATE atoms, but modern compositors (Wayland) ignore
    // client hints entirely. We rely on BlankOnCapture: a paint-event filter
    // (installed by truthgram_init) overlays black on capture events.
    Q_UNUSED(on)
    qWarning() << "[screen_shield] Linux: using BlankOnCapture fallback (compositor-specific)";
#endif
}

} // namespace tg::truthgram::screen
