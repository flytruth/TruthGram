// phishing_guard.h
//
// TruthGram Link Guardian — heuristic phishing detection on link clicks.
//
//   * Punycode/IDN homograph detection (Cyrillic spoofing of latin glyphs).
//   * URL unshortening via HEAD requests through the user's Tor/SOCKS5 proxy.
//   * Lookalike Telegram domain checks (teIegrarn.org, te1egram.app, ...).
//   * Optional local VirusTotal / URLhaus API key integration.
//
// All network egress is opt-in and proxied; the analyzer never leaks queries
// to any TruthGram server (there is none).
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QSet>

#include <memory>
#include <vector>

namespace tg::truthgram::phishing {

enum class RiskLevel {
    Safe,
    Suspicious,
    Dangerous,
};

enum class RiskReason : std::uint16_t {
    None                = 0,
    IdnHomograph        = 1,
    LookalikeTelegram   = 2,
    PunycodeMixedScript = 3,
    ShortenerUnresolved = 4,
    ShortenerLoop       = 5,
    VirusTotalPositive  = 6,
    UrlhausListed       = 7,
    ExcessiveSubdomain  = 8,
    NonHttps            = 9,
    IpLiteralHost       = 10,
};

struct Analysis {
    RiskLevel level = RiskLevel::Safe;
    std::vector<RiskReason> reasons;
    QString detail;          // human-readable summary shown in the modal
    QUrl    resolvedUrl;     // after unshortening
    QUrl    originalUrl;
};

Q_NAMESPACE

// =============================================================================
// PhishingGuard
// =============================================================================
class PhishingGuard : public QObject {
    Q_OBJECT
public:
    static PhishingGuard& instance();

    // Synchronous heuristic pass (no network). Returns quickly enough to gate
    // the link-click modal without a round trip.
    [[nodiscard]] Analysis analyzeHeuristic(const QUrl& url) const;

    // Asynchronous deep analysis: follows shortener redirects (HEAD only,
    // max 5 hops) through the configured proxy, optionally queries
    // VirusTotal / URLhaus if API keys are present.
    void analyzeDeep(const QUrl& url);

    void setVirusTotalKey(const QString& key);
    void setUrlhausEnabled(bool on);
    void setProxyUrl(const QString& socksOrHttpProxy);   // e.g. socks5://127.0.0.1:9050

    // Did the user opt in to the confirmation modal at all? Reads RuntimeConfig.
    [[nodiscard]] bool confirmModalEnabled() const;

Q_SIGNALS:
    void deepAnalysisComplete(const Analysis& result);

private:
    PhishingGuard();
    ~PhishingGuard() override;
    PhishingGuard(const PhishingGuard&) = delete;
    PhishingGuard& operator=(const PhishingGuard&) = delete;

    struct State;
    std::unique_ptr<State> _s;

    // ---- Heuristic helpers (all synchronous, no network) ----
    [[nodiscard]] bool isIdnHomograph(const QString& host) const;
    [[nodiscard]] bool isLookalikeTelegram(const QString& host) const;
    [[nodiscard]] bool isMixedScript(const QString& host) const;
    [[nodiscard]] bool isExcessiveSubdomain(const QString& host) const;
    [[nodiscard]] bool isIpLiteral(const QString& host) const;

    // Confusable-character table for the Telegram brand. Returns the
    // normalized (ASCII-folded) form so "teIegrarn" -> "telegram".
    [[nodiscard]] QString foldConfusables(const QString& host) const;
};

inline PhishingGuard& linkGuardian() { return PhishingGuard::instance(); }

} // namespace tg::truthgram::phishing
