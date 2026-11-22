// floating_action_bar.h
//
// Shared TruthGram floating pill widget: rounded background, drop shadow,
// spring-physics show/hide animation. Used by Ghost Mode, the Mini Player,
// the OSINT inspector badges, and the Voice FX popover.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QWidget>
#include <QPointer>
#include <vector>

class QPushButton;
class QLabel;
class QGraphicsDropShadowEffect;

namespace tg::truthgram::ui {

class SpringAnimation;

class FloatingActionBar : public QWidget {
    Q_OBJECT
public:
    explicit FloatingActionBar(QWidget* parent);
    ~FloatingActionBar() override;

    // Takes ownership of button lifetime via Qt parenting. Buttons are laid
    // out left-to-right inside the pill.
    void setButtons(const std::vector<QPushButton*>& buttons);
    void setCaption(QLabel* caption);

    // Animate in/out from the bottom of the parent widget. The animation uses
    // the spring constants from the active TruthGram theme.
    void animateIn();
    void animateOut();

    // Reposition relative to the parent's bottom edge; called on resize.
    void reposition();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _i;
};

} // namespace tg::truthgram::ui
