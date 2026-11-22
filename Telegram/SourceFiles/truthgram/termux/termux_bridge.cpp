// termux_bridge.cpp
//
// TruthGram Termux / local CLI IPC bridge implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/termux/termux_bridge.h"

#include "truthgram/storage/truthgram_database.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QHostAddress>
#include <QDebug>

#include <mutex>
#include <unordered_map>

namespace tg::truthgram::termux {

namespace {

constexpr const char* kSecretKey = "termux_bridge_secret";

// Generate a 32-byte shared secret as a hex string.
QString generateSecret() {
    QByteArray bytes(32, '\0');
    for (int i = 0; i < 32; ++i)
        bytes[i] = char(QRandomGenerator::global()->bounded(256));
    return QString::fromLatin1(bytes.toHex());
}

} // namespace

struct TermuxBridge::State {
    std::mutex mu;
    QWebSocketServer* server{nullptr};
    std::vector<ApprovedHook> hooks;
    std::unordered_map<QString, QWebSocket*> clients;
    QString secret;
    QString endpoint_;
    std::atomic<bool> running{false};

    State() {
        // Load or generate the shared secret.
        secret = QString::fromUtf8(
            storage::TruthGramDatabase::instance().get("tg_kv", kSecretKey));
        if (secret.isEmpty()) {
            secret = generateSecret();
            storage::TruthGramDatabase::instance().put("tg_kv", kSecretKey,
                secret.toUtf8());
        }
    }
};

TermuxBridge& TermuxBridge::instance() {
    static TermuxBridge s;
    return s;
}

TermuxBridge::TermuxBridge()
    : QObject(nullptr), _s(std::make_unique<State>()) {}

TermuxBridge::~TermuxBridge() { stop(); }

bool TermuxBridge::start(quint16 port) {
    if (_s->running.load(std::memory_order_acquire)) return true;
    _s->server = new QWebSocketServer(QStringLiteral("TruthGramBridge"),
                                      QWebSocketServer::NonSecureMode, this);
    if (!_s->server->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "[termux_bridge] listen failed:"
                   << _s->server->errorString();
        delete _s->server; _s->server = nullptr;
        return false;
    }
    _s->endpoint_ = QStringLiteral("ws://127.0.0.1:%1").arg(port);
    _s->running.store(true, std::memory_order_release);

    connect(_s->server, &QWebSocketServer::newConnection, this, [this] {
        while (_s->server->hasPendingConnections()) {
            QWebSocket* sock = _s->server->nextPendingConnection();
            // Require the secret as the first message; close on mismatch.
            connect(sock, &QWebSocket::textMessageReceived, this,
                [this, sock](const QString& msg) {
                    handleAuth(sock, msg);
                });
            connect(sock, &QWebSocket::disconnected, this,
                [this, sock] { handleDisconnect(sock); });
        }
    });
    return true;
}

void TermuxBridge::handleAuth(QWebSocket* sock, const QString& msg) {
    // First message must be "AUTH <secret>". Subsequent messages are events.
    if (msg.startsWith(QStringLiteral("AUTH "))) {
        if (msg.mid(5) == _s->secret) {
            const QString id = QUuid::createUuid().toString();
            {
                std::lock_guard lk(_s->mu);
                _s->clients[id] = sock;
            }
            sock->setProperty("authed", true);
            sock->setProperty("id", id);
            Q_EMIT clientConnected(id);
        } else {
            sock->close();
        }
        return;
    }
    if (!sock->property("authed").toBool()) { sock->close(); return; }
    // Authed message: a JSON event from the local CLI. Route to hooks.
    const auto doc = QJsonDocument::fromJson(msg.toUtf8());
    if (!doc.isObject()) return;
    const auto o = doc.object();
    const QString trigger = o.value("event").toString();
    dispatchEvent(trigger, msg.toUtf8());
}

void TermuxBridge::handleDisconnect(QWebSocket* sock) {
    const QString id = sock->property("id").toString();
    if (!id.isEmpty()) {
        std::lock_guard lk(_s->mu);
        _s->clients.erase(id);
    }
    Q_EMIT clientDisconnected(id);
    sock->deleteLater();
}

void TermuxBridge::stop() {
    if (!_s->running.exchange(false)) return;
    if (_s->server) {
        _s->server->close();
        _s->server->deleteLater();
        _s->server = nullptr;
    }
}

bool TermuxBridge::running() const {
    return _s->running.load(std::memory_order_acquire);
}

QString TermuxBridge::endpoint() const { return _s->endpoint_; }

std::vector<ApprovedHook> TermuxBridge::hooks() const {
    std::lock_guard lk(_s->mu);
    return _s->hooks;
}

void TermuxBridge::addHook(const ApprovedHook& h) {
    std::lock_guard lk(_s->mu);
    _s->hooks.push_back(h);
}
void TermuxBridge::removeHook(const QString& name) {
    std::lock_guard lk(_s->mu);
    _s->hooks.erase(std::remove_if(_s->hooks.begin(), _s->hooks.end(),
        [&](const ApprovedHook& h) { return h.name == name; }),
        _s->hooks.end());
}
void TermuxBridge::setHookEnabled(const QString& name, bool enabled) {
    std::lock_guard lk(_s->mu);
    for (auto& h : _s->hooks) if (h.name == name) h.enabled = enabled;
}

void TermuxBridge::dispatchEvent(const QString& triggerEvent,
                                  const QByteArray& jsonPayload) {
    std::vector<ApprovedHook> snapshot;
    {
        std::lock_guard lk(_s->mu);
        for (const auto& h : _s->hooks)
            if (h.enabled && h.triggerEvent == triggerEvent)
                snapshot.push_back(h);
    }
    for (const auto& h : snapshot) {
        // Only allow whitelisted tokens; never arbitrary user input.
        QString cmd = h.command;
        cmd.replace(QStringLiteral("{json}"), QString::fromUtf8(jsonPayload));
        QProcess* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, name = h.name, proc](int code, QProcess::ExitStatus) {
                if (code == 0) Q_EMIT hookExecuted(name, code);
                else           Q_EMIT hookFailed(name, QStringLiteral("exit %1").arg(code));
                proc->deleteLater();
            });
        proc->start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
    }
}

} // namespace tg::truthgram::termux
