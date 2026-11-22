// webapp_inspector.h
//
// TruthGram Mini App Domain Inspector. Intercepts messages.requestWebView /
// Mini App launches and displays a persistent security badge in the Mini App
// title bar showing the exact underlying FQDN, the SSL/TLS certificate issuer,
// and the IP/ASN of the resolved host — BEFORE any initData token is released
// to the WebView.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QPointer>

#include <memory>

namespace tg::truthgram::webapp {

struct MiniAppIdentity {
    qint64   botId{0};
    QString  botUsername;
    QUrl     webAppUrl;
    QString  fqdn;            // resolved FQDN after redirects
    QString  tlsIssuer;       // CN of the leaf certificate issuer
    QString  tlsSubject;      // CN of the leaf cert subject
    QString  tlsExpiry;       // ISO 8601
    QString  resolvedIp;
    QString  asn;             // "AS12345"
    QString  asnOrg;
    bool     initDataHeld{true};   // we hold initData until the user approves
};

Q_NAMESPACE

// =============================================================================
// WebAppInspector
// =============================================================================
class WebAppInspector : public QObject {
    Q_OBJECT
public:
    static WebAppInspector& instance();

    // Called from the tdesktop hook BEFORE messages.requestWebView is sent.
    // Resolves the URL, fetches the TLS cert + ASN, and emits
    // inspectionComplete. The caller must NOT send initData until the user
    // taps "Approve" in the badge UI (gated by approveLaunch).
    void inspect(qint64 botId, const QString& botUsername, const QUrl& webAppUrl);

    // Called when the user explicitly approves (or the badge is dismissed).
    void approveLaunch(qint64 botId);
    void denyLaunch(qint64 botId);

    [[nodiscard]] bool isApproved(qint64 botId) const;

    // The most recent inspection result, for the badge widget.
    [[nodiscard]] MiniAppIdentity lastInspection() const;

Q_SIGNALS:
    void inspectionComplete(const MiniAppIdentity& id);
    void launchApproved(qint64 botId);
    void launchDenied(qint64 botId);

private:
    WebAppInspector();
    ~WebAppInspector() override;
    WebAppInspector(const WebAppInspector&) = delete;
    WebAppInspector& operator=(const WebAppInspector&) = delete;

    struct State;
    std::unique_ptr<State> _s;

    // Resolve FQDN by following redirects; fetch TLS cert details via QSslSocket.
    void resolveAsync(const QUrl& url, std::function<void(MiniAppIdentity)> done);
};

inline WebAppInspector& webappInspector() { return WebAppInspector::instance(); }

} // namespace tg::truthgram::webapp
