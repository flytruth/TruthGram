// ios_ui_engine.cpp
//
// TruthGram iOS-inspired desktop UI engine implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/ios_ui/ios_ui_engine.h"
#include "truthgram/ios_ui/frosted_sidebar.h"
#include "truthgram/ios_ui/mini_player.h"
#include "truthgram/theme/truthgram_theme.h"
#include "truthgram/sync/cloud_sync.h"

#include <QWidget>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QGestureEvent>
#include <QSwipeGesture>
#include <QDebug>

#include <atomic>

namespace tg::truthgram::ios_ui {

// A QScrollArea subclass that animates its vertical scrollbar with a spring
// toward the target value when the user releases the wheel/trackpad.
class SpringScroller : public QScrollArea {
    Q_OBJECT
public:
    explicit SpringScroller(QWidget* parent) : QScrollArea(parent) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        grabGesture(Qt::SwipeGesture);
    }

Q_SIGNALS:
    void swipedLeft();
    void swipedRight();

protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::Gesture)
            return gestureEvent(static_cast<QGestureEvent*>(e));
        return QScrollArea::event(e);
    }

    void wheelEvent(QWheelEvent* e) override {
        if (!_springEnabled) { QScrollArea::wheelEvent(e); return; }
        const int delta = e->angleDelta().y();
        const int target = verticalScrollBar()->value() - delta;
        animateTo(target);
        e->accept();
    }

private:
    bool gestureEvent(QGestureEvent* e) {
        if (auto* s = qobject_cast<QSwipeGesture*>(e->gesture(Qt::SwipeGesture))) {
            if (s->horizontalDirection() == QSwipeGesture::Left)  Q_EMIT swipedLeft();
            if (s->horizontalDirection() == QSwipeGesture::Right) Q_EMIT swipedRight();
        }
        return true;
    }

    void animateTo(int target) {
        auto* bar = verticalScrollBar();
        target = std::clamp(target, bar->minimum(), bar->maximum());
        auto* anim = new QPropertyAnimation(bar, "value", this);
        anim->setDuration(450);
        anim->setEasingCurve(QEasingCurve(QEasingCurve::OutBack,
                                          QEasingCurve::Amplitude(0.85),
                                          QEasingCurve::Period(0.45)));
        anim->setStartValue(bar->value());
        anim->setEndValue(target);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    bool _springEnabled = true;
};

struct IosUiEngine::State {
    QPointer<QWidget> mainWindow;
    FrostedSidebar* sidebar{nullptr};
    MiniPlayer* miniPlayer{nullptr};
    SpringScroller* scroller{nullptr};
    std::atomic<bool> frosted{true};
    std::atomic<bool> spring{true};
    std::atomic<bool> rounded{true};
    std::atomic<bool> floatingNav{true};
    std::atomic<bool> swipe{true};
};

IosUiEngine& IosUiEngine::instance() {
    static IosUiEngine s;
    return s;
}

IosUiEngine::IosUiEngine()
    : QObject(nullptr), _s(std::make_unique<State>()) {}
IosUiEngine::~IosUiEngine() = default;

void IosUiEngine::installOn(QWidget* mainWindow) {
    _s->mainWindow = mainWindow;
    _s->sidebar = new FrostedSidebar(mainWindow);
    _s->sidebar->show();

    _s->miniPlayer = new MiniPlayer(mainWindow);
    _s->miniPlayer->lower();
    _s->miniPlayer->move(mainWindow->width() / 2 - 200,
                         mainWindow->height() - 64);
    _s->miniPlayer->show();
}

void IosUiEngine::setFrostedGlass(bool on) {
    _s->frosted.store(on, std::memory_order_release);
    if (_s->sidebar) _s->sidebar->setFrost(on);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void IosUiEngine::setSpringScroll(bool on) {
    _s->spring.store(on, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void IosUiEngine::setRoundedBubbles(bool on) {
    _s->rounded.store(on, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void IosUiEngine::setFloatingNav(bool on) {
    _s->floatingNav.store(on, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void IosUiEngine::setSwipeToReply(bool on) {
    _s->swipe.store(on, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}

bool IosUiEngine::frostedGlass() const  { return _s->frosted.load(std::memory_order_acquire); }
bool IosUiEngine::springScroll() const  { return _s->spring.load(std::memory_order_acquire); }
bool IosUiEngine::roundedBubbles() const{ return _s->rounded.load(std::memory_order_acquire); }
bool IosUiEngine::floatingNav() const   { return _s->floatingNav.load(std::memory_order_acquire); }
bool IosUiEngine::swipeToReply() const  { return _s->swipe.load(std::memory_order_acquire); }

} // namespace tg::truthgram::ios_ui

#include "ios_ui_engine.moc"
