// ghost_controls.h
//
// TruthGram Ghost Mode — granular read-receipt controls.
//
// When the user opens a chat, TruthGram suppresses the upstream
// messages.readHistory / messages.readMessageContents packets. Instead it
// injects a floating action bar at the bottom of the chat with two explicit
// buttons: [✓ Mark as Read] and [🕶 Keep Unread / Ghost]. Toggles for hiding
// typing status (messages.setTyping) and online presence are also surfaced
// here and in Settings.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QPointer>

#include <atomic>
#include <memory>

namespace tg::truthgram::ghost {

// =============================================================================
// GhostController
// =============================================================================
// Decides whether a given MTProto packet should be suppressed. The tdesktop
// ApiWrap / MTP::send hooks consult GhostController before each send.
class GhostController : public QObject {
    Q_OBJECT
public:
    static GhostController& instance();

    // Set by truthgram_init from RuntimeConfig + cloud_sync imports.
    void setSuppressReadHistory(bool v);
    void setSuppressReadContents(bool v);
    void setSuppressTyping(bool v);
    void setSuppressPresence(bool v);

    [[nodiscard]] bool suppressReadHistory() const;
    [[nodiscard]] bool suppressReadContents() const;
    [[nodiscard]] bool suppressTyping() const;
    [[nodiscard]] bool suppressPresence() const;

    // The floating action bar calls these. When the user taps [✓ Mark as Read],
    // we ALLOW exactly one readHistory for the given peer and then return to
    // suppression. This one-shot gate is the core of Ghost Mode.
    void allowOneReadHistory(qint64 peerId);
    [[nodiscard]] bool consumeReadHistoryAllowance(qint64 peerId);

Q_SIGNALS:
    void togglesChanged();
    void ghostBarRequested(qint64 peerId);   // the UI shows the floating bar
    void ghostBarDismissed(qint64 peerId);

private:
    GhostController();
    ~GhostController() override;
    GhostController(const GhostController&) = delete;
    GhostController& operator=(const GhostController&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

inline GhostController& ghost() { return GhostController::instance(); }

// =============================================================================
// FloatingGhostBar
// =============================================================================
// The actual floating pill widget drawn at the bottom of an unread chat.
// Two buttons + an info label. Implementation is in ghost_controls.cpp.
class FloatingGhostBar : public QObject {
    Q_OBJECT
public:
    // Creates the bar for the given chat container. Reparents into container.
    explicit FloatingGhostBar(QWidget* chatContainer, qint64 peerId);
    ~FloatingGhostBar() override;

    void show();
    void hide();

Q_SIGNALS:
    void markAsRead(qint64 peerId);
    void keepUnread(qint64 peerId);

private:
    struct Impl;
    std::unique_ptr<Impl> _i;
};

} // namespace tg::truthgram::ghost
