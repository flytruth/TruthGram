// ghost_controls.cpp
//
// TruthGram Ghost Mode — controller + floating action bar widget.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/ghost/ghost_controls.h"
#include "truthgram/theme/truthgram_theme.h"
#include "truthgram/sync/cloud_sync.h"
#include "truthgram/ui/floating_action_bar.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QGraphicsDropShadowEffect>

#include <unordered_map>
#include <mutex>

namespace tg::truthgram::ghost {

struct GhostController::State {
    mutable std::mutex mu;
    std::unordered_map<qint64, bool> oneShotRead;   // peerId -> allowance
    std::atomic<bool> suppressReadHistory{true};
    std::atomic<bool> suppressReadContents{true};
    std::atomic<bool> suppressTyping{true};
    std::atomic<bool> suppressPresence{true};
};

GhostController& GhostController::instance() {
    static GhostController s;
    return s;
}

GhostController::GhostController()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {}

GhostController::~GhostController() = default;

void GhostController::setSuppressReadHistory(bool v) {
    _s->suppressReadHistory.store(v, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void GhostController::setSuppressReadContents(bool v) {
    _s->suppressReadContents.store(v, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void GhostController::setSuppressTyping(bool v) {
    _s->suppressTyping.store(v, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}
void GhostController::setSuppressPresence(bool v) {
    _s->suppressPresence.store(v, std::memory_order_release);
    Q_EMIT togglesChanged();
    sync::cloudSync().markDirty();
}

bool GhostController::suppressReadHistory() const {
    return _s->suppressReadHistory.load(std::memory_order_acquire);
}
bool GhostController::suppressReadContents() const {
    return _s->suppressReadContents.load(std::memory_order_acquire);
}
bool GhostController::suppressTyping() const {
    return _s->suppressTyping.load(std::memory_order_acquire);
}
bool GhostController::suppressPresence() const {
    return _s->suppressPresence.load(std::memory_order_acquire);
}

void GhostController::allowOneReadHistory(qint64 peerId) {
    {
        std::lock_guard lk(_s->mu);
        _s->oneShotRead[peerId] = true;
    }
    Q_EMIT ghostBarDismissed(peerId);
}

bool GhostController::consumeReadHistoryAllowance(qint64 peerId) {
    std::lock_guard lk(_s->mu);
    auto it = _s->oneShotRead.find(peerId);
    if (it == _s->oneShotRead.end()) return false;
    const bool granted = it->second;
    _s->oneShotRead.erase(it);
    return granted;
}

// ---------------------------------------------------------------------------
// FloatingGhostBar
// ---------------------------------------------------------------------------
struct FloatingGhostBar::Impl {
    QWidget* container{nullptr};
    qint64 peerId{0};
    ui::FloatingActionBar* bar{nullptr};
};

FloatingGhostBar::FloatingGhostBar(QWidget* chatContainer, qint64 peerId)
    : QObject(chatContainer)
    , _i(std::make_unique<Impl>()) {
    _i->container = chatContainer;
    _i->peerId = peerId;

    // The shared floating-action-bar widget (truthgram/ui/floating_action_bar)
    // gives us the rounded pill geometry + drop shadow + theme colors.
    _i->bar = new ui::FloatingActionBar(chatContainer);

    auto* markRead = new QPushButton(QStringLiteral("\u2713  Mark as Read"), _i->bar);
    auto* keepGhost = new QPushButton(QStringLiteral("\U0001F575  Keep Unread / Ghost"), _i->bar);
    auto* info = new QLabel(QStringLiteral("Ghost Mode is active."), _i->bar);

    const QColor accent = theme::theme().accent();
    const QColor bg = theme::theme().windowBg();
    const QColor link = theme::theme().link();

    // Mark-as-read uses the lime accent; Keep-Unread uses the ice blue.
    markRead->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;color:%2;border:none;border-radius:10px;"
        "padding:8px 16px;font-weight:600;}"
        "QPushButton:hover{background:%3;}")
        .arg(accent.name(), bg.name(), theme::theme().accentDim().name()));
    keepGhost->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;color:%2;border:none;border-radius:10px;"
        "padding:8px 16px;font-weight:600;}"
        "QPushButton:hover{background:%3;}")
        .arg(link.name(), bg.name(), theme::theme().linkDim().name()));
    info->setStyleSheet(QStringLiteral("color:%1;").arg(
        theme::theme().subTextFg().name()));

    _i->bar->setButtons({markRead, keepGhost});
    _i->bar->setCaption(info);

    connect(markRead, &QPushButton::clicked, this, [this] {
        ghost().allowOneReadHistory(_i->peerId);
        Q_EMIT markAsRead(_i->peerId);
    });
    connect(keepGhost, &QPushButton::clicked, this, [this] {
        // Explicit "stay unread" — clear any pending allowance and dismiss.
        ghost().consumeReadHistoryAllowance(_i->peerId);
        Q_EMIT keepUnread(_i->peerId);
        hide();
    });
}

FloatingGhostBar::~FloatingGhostBar() = default;

void FloatingGhostBar::show() {
    if (_i && _i->bar) {
        _i->bar->raise();
        _i->bar->show();
        _i->bar->animateIn();
    }
}

void FloatingGhostBar::hide() {
    if (_i && _i->bar) {
        _i->bar->animateOut();
    }
}

} // namespace tg::truthgram::ghost
