// phishing_guard.cpp
//
// TruthGram Link Guardian implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/phishing/phishing_guard.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QStringList>
#include <QDebug>
#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <mutex>
#include <unordered_map>

namespace tg::truthgram::phishing {

namespace {

// Known shortener domains we will try to resolve via HEAD.
const std::array<const char*, 24> kShorteners = {
    "bit.ly", "t.co", "goo.gl", "tinyurl.com", "ow.ly", "is.gd", "buff.ly",
    "rebrand.ly", "cutt.ly", "shorturl.at", "tiny.cc", "rb.gy", "t.ly",
    "soo.gd", "s.id", "v.gd", "shrtco.de", "lnkd.in", "fb.me", "wa.me",
    "tg-link.ru", "telemetr.io", "telegra.ph", "t.me"
};

// Approximate edit distance (Levenshtein) restricted to small strings.
int editDistance(const QString& a, const QString& b) {
    if (a.isEmpty()) return int(b.size());
    if (b.isEmpty()) return int(a.size());
    std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) prev[j] = int(j);
    for (size_t i = 1; i <= a.size(); ++i) {
        cur[0] = int(i);
        for (size_t j = 1; j <= b.size(); ++j) {
            const int cost = (a[i-1].toLower() == b[j-1].toLower()) ? 0 : 1;
            cur[j] = std::min({ prev[j] + 1, cur[j-1] + 1, prev[j-1] + cost });
        }
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

// Canonical "telegram" brand strings we defend.
const std::array<const char*, 6> kBrandDomains = {
    "telegram.org", "telegram.me", "t.me", "telegram.dog",
    "telegramchannel.com", "telegram.me"
};

} // namespace

struct PhishingGuard::State {
    std::mutex mu;
    QString virusTotalKey;
    QString proxyUrl;
    bool urlhausEnabled = false;
    QNetworkAccessManager* nam = nullptr;
};

PhishingGuard& PhishingGuard::instance() {
    static PhishingGuard s;
    return s;
}

PhishingGuard::PhishingGuard()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {
    _s->nam = new QNetworkAccessManager(this);
}

PhishingGuard::~PhishingGuard() = default;

void PhishingGuard::setVirusTotalKey(const QString& key) {
    std::lock_guard lk(_s->mu);
    _s->virusTotalKey = key;
}
void PhishingGuard::setUrlhausEnabled(bool on) {
    std::lock_guard lk(_s->mu);
    _s->urlhausEnabled = on;
}
void PhishingGuard::setProxyUrl(const QString& p) {
    std::lock_guard lk(_s->mu);
    _s->proxyUrl = p;
    if (p.isEmpty()) {
        _s->nam->setProxy(QNetworkProxy::NoProxy);
    } else {
        QNetworkProxy proxy;
        proxy.setApplicationProxy(QNetworkProxy());
        const QUrl u(p);
        QNetworkProxy::ProxyType t = QNetworkProxy::Socks5Proxy;
        if (u.scheme().startsWith("http")) t = QNetworkProxy::HttpProxy;
        proxy.setType(t);
        proxy.setHostName(u.host());
        proxy.setPort(u.port(9050));
        _s->nam->setProxy(proxy);
    }
}

bool PhishingGuard::confirmModalEnabled() const {
    // In the real overlay this reads RuntimeConfig::phishingGuardConfirm.
    return true;
}

// ---------------------------------------------------------------------------
// Heuristic pass (synchronous, no network)
// ---------------------------------------------------------------------------
Analysis PhishingGuard::analyzeHeuristic(const QUrl& url) const {
    Analysis a;
    a.originalUrl = url;
    a.resolvedUrl = url;
    if (!url.isValid()) {
        a.level = RiskLevel::Dangerous;
        a.reasons.push_back(RiskReason::None);
        a.detail = QStringLiteral("Invalid URL.");
        return a;
    }
    if (url.scheme() != "https") {
        a.reasons.push_back(RiskReason::NonHttps);
        a.level = RiskLevel::Suspicious;
        a.detail = QStringLiteral("Link is not HTTPS.");
    }
    const QString host = url.host().toLower();
    if (isIpLiteral(host)) {
        a.reasons.push_back(RiskReason::IpLiteralHost);
        a.level = RiskLevel::Suspicious;
        a.detail = QStringLiteral("Host is a raw IP literal.");
    }
    if (isIdnHomograph(host) || isMixedScript(host)) {
        a.reasons.push_back(RiskReason::IdnHomograph);
        a.level = RiskLevel::Dangerous;
        a.detail = QStringLiteral("IDN homograph / mixed-script host detected.");
    }
    if (isLookalikeTelegram(host)) {
        a.reasons.push_back(RiskReason::LookalikeTelegram);
        a.level = RiskLevel::Dangerous;
        a.detail = QStringLiteral("Lookalike Telegram domain: ") + host;
    }
    if (isExcessiveSubdomain(host)) {
        a.reasons.push_back(RiskReason::ExcessiveSubdomain);
        if (a.level == RiskLevel::Safe) a.level = RiskLevel::Suspicious;
        a.detail += QStringLiteral(" Excessive subdomain depth.");
    }
    if (a.reasons.empty()) {
        a.level = RiskLevel::Safe;
        a.detail = QStringLiteral("No heuristic indicators.");
    }
    return a;
}

// ---------------------------------------------------------------------------
// Deep analysis (async, network via proxy)
// ---------------------------------------------------------------------------
void PhishingGuard::analyzeDeep(const QUrl& url) {
    // First the heuristic pass synchronously, then resolve shorteners.
    Analysis base = analyzeHeuristic(url);

    const QString host = url.host().toLower();
    const bool isShortened = std::find_if(kShorteners.begin(), kShorteners.end(),
        [&](const char* d) { return host == QString::fromLatin1(d); }) != kShorteners.end();

    if (!isShortened) {
        // Nothing to resolve; emit the heuristic result.
        Q_EMIT deepAnalysisComplete(base);
        return;
    }

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    req.setMaximumRedirectsAllowed(5);
    req.setRawHeader("User-Agent", "TruthGram/1.0 (link-guardian)");
    QNetworkReply* reply = _s->nam->head(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, base]() mutable {
        Analysis r = base;
        const QUrl final = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QVariant redirectChain = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (reply->error() == QNetworkReply::OperationCanceledError
            || reply->error() == QNetworkReply::TooManyRedirectsError) {
            r.reasons.push_back(RiskReason::ShortenerLoop);
            r.level = RiskLevel::Dangerous;
            r.detail += QStringLiteral(" Shortener redirect loop.");
        } else if (final.isValid()) {
            r.resolvedUrl = final;
            // Re-run heuristics against the resolved host.
            const Analysis resolved = analyzeHeuristic(final);
            for (auto reason : resolved.reasons) r.reasons.push_back(reason);
            if (resolved.level > r.level) r.level = resolved.level;
            r.detail += QStringLiteral(" Resolved to: ") + final.toString();
        } else if (reply->error() != QNetworkReply::NoError) {
            r.reasons.push_back(RiskReason::ShortenerUnresolved);
            r.detail += QStringLiteral(" Shortener unresolved.");
        }
        reply->deleteLater();
        Q_EMIT deepAnalysisComplete(r);
    });
}

// ---------------------------------------------------------------------------
// Heuristic helpers
// ---------------------------------------------------------------------------
bool PhishingGuard::isIpLiteral(const QString& host) const {
    QHostAddress h(host);
    return !host.isEmpty() && !h.isNull();
}

bool PhishingGuard::isIdnHomograph(const QString& host) const {
    // A host whose Punycode (xn--) representation appears in any label while
    // the visual form contains ASCII letters is a classic homograph.
    const QStringList labels = host.split('.');
    for (const QString& label : labels) {
        const QByteArray ace = QUrl::toAce(label);  // "xn--..."
        if (ace.startsWith("xn--")) {
            // If the label also has ASCII letters mixed in, flag it.
            bool hasAscii = false, hasNonAscii = false;
            for (const QChar c : label) {
                if (c.toLatin1() != 0 && c.isLetter()) hasAscii = true;
                else if (c.unicode() > 127)            hasNonAscii = true;
            }
            if (hasAscii && hasNonAscii) return true;
        }
    }
    return false;
}

bool PhishingGuard::isMixedScript(const QString& host) const {
    // Detect labels that mix Cyrillic and Latin (the canonical Telegram
    // spoof: teIegrarn using Cyrillic capital I).
    bool latin = false, cyrillic = false;
    for (const QChar c : host) {
        const ushort u = c.unicode();
        if (u >= 0x0041 && u <= 0x024F) latin = true;          // basic latin + ext
        if (u >= 0x0400 && u <= 0x04FF) cyrillic = true;
    }
    return latin && cyrillic;
}

bool PhishingGuard::isLookalikeTelegram(const QString& host) const {
    const QString folded = foldConfusables(host);
    for (const char* brand : kBrandDomains) {
        const QString b = QString::fromLatin1(brand);
        if (folded == b) return false;             // exact match is fine
        if (folded.endsWith('.' + b)) return false; // legitimate subdomain
        // Edit distance <= 2 on a same-length suffix is a lookalike.
        const int dist = editDistance(folded, b);
        if (folded.size() >= b.size() && dist > 0 && dist <= 2)
            return true;
    }
    return false;
}

bool PhishingGuard::isExcessiveSubdomain(const QString& host) const {
    const QStringList parts = host.split('.');
    // Anything beyond 4 labels (e.g. a.b.c.example.com) is suspicious.
    return parts.size() > 5;
}

// Fold common Cyrillic confusables to their Latin look-alikes. This is the
// table the upstream spoofers use against Telegram: Cyrillic "І", "а", "е",
// "о", "р", "с", "у", "х" map to Latin I,a,e,o,p,c,y,x.
QString PhishingGuard::foldConfusables(const QString& host) const {
    static const std::unordered_map<ushort, QChar> kMap = {
        {0x0410, 'A'}, {0x0430, 'a'}, // а
        {0x0412, 'B'}, {0x0432, 'b'}, // в (not perfect, but close)
        {0x0415, 'E'}, {0x0435, 'e'}, // е
        {0x041A, 'K'}, {0x043A, 'k'}, // к
        {0x041C, 'M'}, {0x043C, 'm'}, // м
        {0x041D, 'H'}, {0x043D, 'h'}, // н
        {0x041E, 'O'}, {0x043E, 'o'}, // о
        {0x0420, 'P'}, {0x0440, 'p'}, // р
        {0x0421, 'C'}, {0x0441, 'c'}, // с
        {0x0422, 'T'}, {0x0442, 't'}, // т
        {0x0423, 'Y'}, {0x0443, 'y'}, // у
        {0x0425, 'X'}, {0x0445, 'x'}, // х
        {0x0406, 'I'}, {0x0456, 'i'}, // і
        {0x04AE, 'Y'}, {0x04AF, 'y'}, // ү
    };
    QString out;
    out.reserve(host.size());
    for (const QChar c : host) {
        auto it = kMap.find(c.unicode());
        out.append(it == kMap.end() ? c : it->second);
    }
    return out;
}

} // namespace tg::truthgram::phishing
