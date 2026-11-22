// termux_bridge.h
//
// TruthGram Termux & Local CLI Bridge. Provides an encrypted local WebSocket /
// Unix Domain Socket IPC bridge on 127.0.0.1 (or over an SSH tunnel) allowing
// users to connect TruthGram Desktop to Termux on Android or local shell
// scripts to trigger notifications, pipe logs, or execute user-approved
// automation hooks.
//
// Security: the bridge binds to loopback only, requires a per-install shared
// secret (32 random bytes stored in the SQLCipher DB), and only executes
// hooks the user has explicitly approved in Settings. No remote network
// exposure is possible.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QByteArray>

#include <atomic>
#include <memory>
#include <vector>

class QWebSocketServer;
class QWebSocket;

namespace tg::truthgram::termux {

struct ApprovedHook {
    QString name;          // unique id
    QString command;       // shell command template, {json} substituted
    QString triggerEvent;  // e.g. "message_received", "notification_posted"
    bool    enabled{true};
};

Q_NAMESPACE

// =============================================================================
// TermuxBridge
// =============================================================================
class TermuxBridge : public QObject {
    Q_OBJECT
public:
    static TermuxBridge& instance();

    // Start the IPC server. Binds to 127.0.0.1:<port> (default 7354 = "TGBR")
    // or, on POSIX, a Unix Domain Socket at $XDG_RUNTIME_DIR/truthgram.sock.
    // The shared secret is generated if missing and persisted to SQLCipher.
    bool start(quint16 port = 7354);
    void stop();
    [[nodiscard]] bool running() const;
    [[nodiscard]] QString endpoint() const;

    // Approved-hook catalog. Persisted to SQLCipher + cloud_sync.
    [[nodiscard]] std::vector<ApprovedHook> hooks() const;
    void addHook(const ApprovedHook& h);
    void removeHook(const QString& name);
    void setHookEnabled(const QString& name, bool enabled);

    // Dispatch an event to all approved hooks matching triggerEvent.
    // The JSON payload is substituted into {json} in the hook command.
    void dispatchEvent(const QString& triggerEvent, const QByteArray& jsonPayload);

Q_SIGNALS:
    void clientConnected(const QString& remoteId);
    void clientDisconnected(const QString& remoteId);
    void hookExecuted(const QString& name, int exitCode);
    void hookFailed(const QString& name, const QString& reason);

private:
    TermuxBridge();
    ~TermuxBridge() override;
    TermuxBridge(const TermuxBridge&) = delete;
    TermuxBridge& operator=(const TermuxBridge&) = delete;

    struct State;
    std::unique_ptr<State> _s;

    // WebSocket auth + disconnect handlers (called from lambdas in start()).
    void handleAuth(QWebSocket* sock, const QString& msg);
    void handleDisconnect(QWebSocket* sock);
};

inline TermuxBridge& termux() { return TermuxBridge::instance(); }

} // namespace tg::truthgram::termux
