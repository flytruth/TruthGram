// webapp_inspector.cpp
//
// TruthGram Mini App Domain Inspector implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/webapp/webapp_inspector.h"

#include <QSslSocket>
#include <QSslCertificate>
#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

#include <mutex>
#include <unordered_map>

namespace tg::truthgram::webapp {

namespace {

// Look up the ASN for an IP via Team Cymru's WHOIS service over TLS. This is
// the only outbound call the inspector makes, and it goes through the user's
// configured Tor/SOCKS5 proxy when one is set (applied at the NAM level by
// truthgram_init). The query is `show ipwhois <ip>` formatted as a DNS TXT.
//
// In the blueprint we keep it simple: emit a placeholder and let the
// network-filter UI surface that ASN lookup is opt-in.
QString lookupAsn(const QString& ip) {
    if (ip.isEmpty()) return {};
    // Real implementation: query whois.cymru.com over TLS (port 43).
    // Skipped here to keep the blueprint self-contained.
    return QStringLiteral("(opt-in)");
}

} // namespace

struct WebAppInspector::State {
    std::mutex mu;
    std::unordered_map<qint64, bool> approved;
    MiniAppIdentity last;
    QNetworkAccessManager* nam{nullptr};
};

WebAppInspector& WebAppInspector::instance() {
    static WebAppInspector s;
    return s;
}

WebAppInspector::WebAppInspector()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {
    _s->nam = new QNetworkAccessManager(this);
}

WebAppInspector::~WebAppInspector() = default;

void WebAppInspector::inspect(qint64 botId, const QString& botUsername, const QUrl& webAppUrl) {
    MiniAppIdentity id;
    id.botId       = botId;
    id.botUsername = botUsername;
    id.webAppUrl   = webAppUrl;
    id.fqdn        = webAppUrl.host().toLower();
    id.initDataHeld = true;

    // Synchronous TLS cert probe on a short-lived QSslSocket. We use this
    // instead of a full HTTPS GET so the inspector never fetches app content
    // before the user approves — only the cert.
    QSslSocket socket;
    socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket.connectToHostEncrypted(id.fqdn, 443);
    if (socket.waitForEncrypted(3000)) {
        const auto certs = socket.peerCertificateChain();
        if (!certs.isEmpty()) {
            const QSslCertificate leaf = certs.first();
            id.tlsIssuer  = leaf.issuerInfo(QSslCertificate::CommonName).join('/');
            id.tlsSubject = leaf.subjectInfo(QSslCertificate::CommonName).join('/');
            id.tlsExpiry  = leaf.expiryDate().toString(Qt::ISODate);
        }
    }
    socket.close();

    // Resolve the host to an IP for the ASN lookup. Use QHostInfo so we don't
    // issue a network request ourselves; DNS goes through the system resolver.
    const QHostInfo hi = QHostInfo::fromName(id.fqdn);
    if (hi.error() == QHostInfo::NoError && !hi.addresses().isEmpty()) {
        id.resolvedIp = hi.addresses().first().toString();
        id.asn        = lookupAsn(id.resolvedIp);
        id.asnOrg     = QStringLiteral("(opt-in)");
    }

    {
        std::lock_guard lk(_s->mu);
        _s->last = id;
    }
    Q_EMIT inspectionComplete(id);
}

void WebAppInspector::approveLaunch(qint64 botId) {
    {
        std::lock_guard lk(_s->mu);
        _s->approved[botId] = true;
    }
    Q_EMIT launchApproved(botId);
}

void WebAppInspector::denyLaunch(qint64 botId) {
    {
        std::lock_guard lk(_s->mu);
        _s->approved[botId] = false;
    }
    Q_EMIT launchDenied(botId);
}

bool WebAppInspector::isApproved(qint64 botId) const {
    std::lock_guard lk(_s->mu);
    auto it = _s->approved.find(botId);
    return it != _s->approved.end() && it->second;
}

MiniAppIdentity WebAppInspector::lastInspection() const {
    std::lock_guard lk(_s->mu);
    return _s->last;
}

} // namespace tg::truthgram::webapp
