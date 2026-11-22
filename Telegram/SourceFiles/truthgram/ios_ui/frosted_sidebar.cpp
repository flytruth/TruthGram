// frosted_sidebar.cpp
//
// TruthGram frosted-glass sidebar implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/ios_ui/frosted_sidebar.h"
#include "truthgram/theme/truthgram_theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QGraphicsBlurEffect>
#include <QPixmap>
#include <QTimer>
#include <QDebug>

namespace tg::truthgram::ios_ui {

struct FrostedSidebar::Impl {
    bool frost{true};
    QPixmap backdrop;             // last captured parent snapshot
    QGraphicsBlurEffect* blur{nullptr};
    QTimer* captureTimer{nullptr};
};

FrostedSidebar::FrostedSidebar(QWidget* parent)
    : QWidget(parent)
    , _i(std::make_unique<Impl>()) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    _i->blur = new QGraphicsBlurEffect(this);
    _i->blur->setBlurRadius(theme::theme().extras().frostBlurRadius);
    _i->blur->setEnabled(false);
    setGraphicsEffect(_i->blur);

    // Re-capture the parent backdrop at ~15 fps so the frost tracks window
    // movement / video playing behind the sidebar.
    _i->captureTimer = new QTimer(this);
    _i->captureTimer->setInterval(66);
    connect(_i->captureTimer, &QTimer::timeout, this, [this] {
        if (!_i->frost || !parentWidget()) return;
        const QRect r = geometry().translated(parentWidget()->mapToParent(QPoint(0,0)));
        _i->backdrop = parentWidget()->grab(r);
        update();
    });
    _i->captureTimer->start();
}

FrostedSidebar::~FrostedSidebar() = default;

void FrostedSidebar::setFrost(bool on) {
    _i->frost = on;
    _i->blur->setEnabled(on);
    if (on) _i->captureTimer->start();
    else    _i->captureTimer->stop();
    update();
}

bool FrostedSidebar::frost() const { return _i->frost; }

void FrostedSidebar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

void FrostedSidebar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& extras = theme::theme().extras();
    const QRectF r = rect();

    if (_i->frost && !_i->backdrop.isNull()) {
        // Draw the blurred backdrop, then a translucent tint on top.
        QPixmap blurred = _i->backdrop.scaled(size(), Qt::IgnoreAspectRatio,
                                              Qt::SmoothTransformation);
        QPainter bp(&blurred);
        bp.setRenderHint(QPainter::Antialiasing, true);
        // Cheap blur: scale down then up.
        const QPixmap small = blurred.scaled(size() / 4, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
        bp.drawPixmap(rect(), small, small.rect());
        bp.end();
        p.drawPixmap(0, 0, blurred);
        p.fillRect(r, QColor(extras.frostBlurColor));
    } else {
        QPainterPath path;
        path.addRoundedRect(r, 0, 0);
        p.fillPath(path, theme::theme().windowBg());
    }

    // Subtle right-edge accent line.
    QPen pen(theme::theme().accent());
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawLine(r.right() - 1, r.top() + 8, r.right() - 1, r.bottom() - 8);
}

} // namespace tg::truthgram::ios_ui
