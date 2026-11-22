// ios_ui_engine.h
//
// TruthGram iOS-inspired desktop UI engine. Coordinates:
//   * Translucent frosted-glass sidebar + top bar (QGraphicsBlurEffect on
//     Win/Linux; platform native vibrancy on macOS).
//   * Rounded bubble geometry for chat messages.
//   * Spring-physics scroll animations.
//   * iOS-style context menus and swipe-to-reply trackpad gestures.
//   * Bottom-aligned floating navigation pills.
//
// Each toggle is independently switchable in Settings > TruthGram Appearance.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QPointer>

#include <memory>

class QWidget;
class QGraphicsBlurEffect;

namespace tg::truthgram::ios_ui {

class FrostedSidebar;
class MiniPlayer;
class SpringScroller;

Q_NAMESPACE

// =============================================================================
// IosUiEngine
// =============================================================================
class IosUiEngine : public QObject {
    Q_OBJECT
public:
    static IosUiEngine& instance();

    // Called once at startup with the main window. Installs the frosted
    // sidebar + top bar effects and the floating nav.
    void installOn(QWidget* mainWindow);

    // Toggles persisted to RuntimeConfig + cloud_sync.
    void setFrostedGlass(bool on);
    void setSpringScroll(bool on);
    void setRoundedBubbles(bool on);
    void setFloatingNav(bool on);
    void setSwipeToReply(bool on);

    [[nodiscard]] bool frostedGlass() const;
    [[nodiscard]] bool springScroll() const;
    [[nodiscard]] bool roundedBubbles() const;
    [[nodiscard]] bool floatingNav() const;
    [[nodiscard]] bool swipeToReply() const;

Q_SIGNALS:
    void togglesChanged();

private:
    IosUiEngine();
    ~IosUiEngine() override;
    IosUiEngine(const IosUiEngine&) = delete;
    IosUiEngine& operator=(const IosUiEngine&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

inline IosUiEngine& iosUi() { return IosUiEngine::instance(); }

} // namespace tg::truthgram::ios_ui
