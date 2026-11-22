// floating_action_bar.cpp
//
// TruthGram shared floating pill widget with spring-physics show/hide.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/ui/floating_action_bar.h"
#include "truthgram/theme/truthgram_theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimerEvent>

namespace tg::truthgram::ui {

// A simple spring animation: integrate a damped spring toward the target
// each tick (~16 ms). We use QPropertyAnimation on a custom 'springValue'
// property, but implement the spring here for a snappier iOS feel.
class SpringAnimation : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
public:
    SpringAnimation(QObject* parent, qreal stiffness, qreal damping)
        : QObject(parent), _k(stiffness), _c(damping) {
        _timer.setInterval(16);
        connect(&_timer, &QTimer::timeout, this, &SpringAnimation::tick);
    }

    void animateTo(qreal target) {
        _target = target;
        _timer.start();
    }
    qreal value() const { return _v; }
    void setValue(qreal v) { _v = v; Q_EMIT valueChanged(v); }

Q_SIGNALS:
    void valueChanged(qreal v);

private:
    void tick() {
        const qreal dt = 0.016;
        const qreal a = -_k * (_v - _target) - _c * _vel;
        _vel += a * dt;
        _v   += _vel * dt;
        Q_EMIT valueChanged(_v);
        if (std::fabs(_v - _target) < 0.001f && std::fabs(_vel) < 0.001f) {
            _timer.stop();
        }
    }

    QTimer _timer;
    qreal _v{0.0};
    qreal _vel{0.0};
    qreal _target{0.0};
    qreal _k;
    qreal _c;
};

struct FloatingActionBar::Impl {
    QHBoxLayout* buttonRow{nullptr};
    QLabel* caption{nullptr};
    QGraphicsDropShadowEffect* shadow{nullptr};
    SpringAnimation* spring{nullptr};
    qreal restY = 0;     // y position when fully shown
    qreal hiddenY = 0;   // y position when hidden (below parent bottom)
};

FloatingActionBar::FloatingActionBar(QWidget* parent)
    : QWidget(parent)
    , _i(std::make_unique<Impl>()) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(4);

    _i->buttonRow = new QHBoxLayout();
    _i->buttonRow->setSpacing(8);
    layout->addLayout(_i->buttonRow);

    _i->caption = new QLabel(this);
    layout->addWidget(_i->caption, 0, Qt::AlignHCenter);

    _i->shadow = new QGraphicsDropShadowEffect(this);
    _i->shadow->setBlurRadius(24);
    _i->shadow->setColor(QColor(0, 0, 0, 160));
    _i->shadow->setOffset(0, 4);
    setGraphicsEffect(_i->shadow);

    // Spring constants come from the active theme extras.
    const auto& extras = theme::theme().extras();
    _i->spring = new SpringAnimation(this,
        extras.springStiffness, extras.springDamping);
    connect(_i->spring, &SpringAnimation::valueChanged, this, [this](qreal v) {
        move(x(), int(v));
    });

    hide();
}

FloatingActionBar::~FloatingActionBar() = default;

void FloatingActionBar::setButtons(const std::vector<QPushButton*>& buttons) {
    // Clear any previously-added buttons. (There used to be a stray
    // `QLayout* old = ... ? ... : nullptr;` here that took an item and then
    // was never read - it tripped -Wunused-variable under -Wall. The while
    // loop below already drains the whole row, so the dead line is gone.)
    while (_i->buttonRow->count()) {
        QLayoutItem* it = _i->buttonRow->takeAt(0);
        delete it;
    }
    for (auto* b : buttons) {
        b->setParent(this);
        _i->buttonRow->addWidget(b);
    }
    adjustSize();
    reposition();
}

void FloatingActionBar::setCaption(QLabel* caption) {
    _i->caption = caption;
    _i->caption->setParent(this);
    if (_i->caption->text().isEmpty())
        _i->caption->hide();
    else
        _i->caption->show();
    adjustSize();
    reposition();
}

void FloatingActionBar::animateIn() {
    if (!parentWidget()) return;
    const int parentH = parentWidget()->height();
    _i->restY = parentH - height() - 16;
    _i->hiddenY = parentH + 16;
    move((parentWidget()->width() - width()) / 2, int(_i->hiddenY));
    show();
    raise();
    _i->spring->setValue(_i->hiddenY);
    _i->spring->animateTo(_i->restY);
}

void FloatingActionBar::animateOut() {
    if (!parentWidget()) return;
    _i->spring->animateTo(double(parentWidget()->height() + height()));
    // After the spring settles, hide. We poll via a one-shot timer.
    QTimer::singleShot(400, this, [this] { if (std::fabs(_i->spring->value() - _i->hiddenY) > 1.0) hide(); });
}

void FloatingActionBar::reposition() {
    if (!parentWidget()) return;
    _i->restY = parentWidget()->height() - height() - 16;
    move((parentWidget()->width() - width()) / 2, int(_i->restY));
}

void FloatingActionBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& extras = theme::theme().extras();
    const qreal radius = extras.bubbleRadius;
    const QRectF r = rect().adjusted(2, 2, -2, -2);

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p.fillPath(path, theme::theme().windowBg());
    // Subtle accent ring (lime 1px).
    QPen pen(theme::theme().accent());
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawPath(path);
}

void FloatingActionBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    reposition();
}

} // namespace tg::truthgram::ui

#include "floating_action_bar.moc"
