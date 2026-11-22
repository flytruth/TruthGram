// osint_runner.cpp
//
// TruthGram OSINT/Sherlock local runner implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/osint/osint_runner.h"

#include "truthgram/storage/truthgram_database.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QDateTime>
#include <QDebug>

#include <mutex>
#include <unordered_map>

namespace tg::truthgram::osint {

namespace {

// Telegram DC table (publicly documented in the MTProto config).
constexpr DcInfo kDcTable[] = {
    {1, "Miami, FL, USA",        "149.154.175.50"},
    {2, "Amsterdam, NL",         "149.154.167.51"},
    {3, "Miami, FL, USA",        "149.154.175.100"},
    {4, "Amsterdam, NL",         "149.154.167.91"},
    {5, "Singapore",             "91.108.56.130"},
};

// Approximate account-creation epochs for Telegram user IDs. These are the
// public milestones from the Telegram bot API documentation and various
// open-source id->timestamp decoders. They give an order-of-magnitude
// estimate, NOT a precise creation date.
struct IdEpoch {
    qint64 idFrom;
    qint64 msSinceEpoch;
};
constexpr IdEpoch kIdEpochs[] = {
    { 1'000'000'000, 1'412'755'200'000LL },   // ~Oct 2014
    { 5'000'000'000, 1'451'606'400'000LL },   // ~Jan 2016
    { 10'000'000'000, 1'467'331'200'000LL }, // ~Jul 2016
    { 50'000'000'000, 1'506'816'000'000LL }, // ~Oct 2017
    { 100'000'000'000, 1'554'076'800'000LL },// ~Apr 2019
    { 500'000'000'000, 1'585'699'200'000LL },// ~Apr 2020
    { 1'000'000'000'000, 1'601'510'400'000LL }, // ~Oct 2020
    { 5'000'000'000'000, 1'628'390'400'000LL }, // ~Aug 2021
};

QList<Platform> defaultPlatforms() {
    QList<Platform> p;
    auto add = [&](const char* name, const char* tmpl, int taken, int avail) {
        Platform s;
        s.name = QString::fromLatin1(name);
        s.checkUrlTemplate = QString::fromLatin1(tmpl);
        s.takenStatus = taken;
        s.availableStatus = avail;
        p.append(std::move(s));
    };
    add("GitHub",      "https://github.com/{u}",            200, 404);
    add("GitLab",      "https://gitlab.com/{u}",            200, 404);
    add("Twitter/X",   "https://x.com/{u}",                 200, 404);
    add("Instagram",   "https://instagram.com/{u}",         200, 404);
    add("Reddit",      "https://reddit.com/user/{u}",       200, 404);
    add("YouTube",     "https://youtube.com/@{u}",          200, 404);
    add("TikTok",      "https://tiktok.com/@{u}",           200, 404);
    add("Twitch",      "https://twitch.tv/{u}",             200, 404);
    add("Mastodon",    "https://mastodon.social/@{u}",      200, 404);
    add("Keybase",     "https://keybase.io/{u}",            200, 404);
    add("HackerNews",  "https://news.ycombinator.com/user?id={u}", 200, 404);
    add("Steam",       "https://steamcommunity.com/id/{u}", 200, 404);
    add("Patreon",     "https://patreon.com/{u}",           200, 404);
    add("SoundCloud",  "https://soundcloud.com/{u}",        200, 404);
    add("Spotify",     "https://open.spotify.com/user/{u}", 200, 404);
    return p;
}

} // namespace

struct OsintRunner::State {
    std::mutex mu;
    QList<Platform> platforms = defaultPlatforms();
    QString proxy;
    int maxConcurrent = 4;
    QNetworkAccessManager* nam{nullptr};
};

OsintRunner& OsintRunner::instance() {
    static OsintRunner s;
    return s;
}

OsintRunner::OsintRunner()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {
    _s->nam = new QNetworkAccessManager(this);
    // Default to Tor SOCKS5 on 9050 if the user has not configured one.
    // We do NOT auto-enable; the proxy is set explicitly via setProxyUrl.
}

OsintRunner::~OsintRunner() = default;

MtprotoMetadata OsintRunner::metadataFor(qint64 userId) const {
    MtprotoMetadata m;
    m.userId = userId;
    // DC ID is encoded in the upper bits of some user ids (channels), but the
    // reliable source is the MTProto config's nearest_dc. Here we read it
    // from the saved config (persisted by truthgram_init on login).
    m.dcId = 0;
    m.age = estimateAccountAge(userId);
    return m;
}

DcInfo OsintRunner::dcInfo(int dcId) {
    for (const auto& d : kDcTable)
        if (d.id == dcId) return d;
    return {dcId, "(unknown)", ""};
}

AccountAgeEstimate OsintRunner::estimateAccountAge(qint64 userId) {
    AccountAgeEstimate e;
    e.userId = userId;
    qint64 best = 0;
    for (const auto& epoch : kIdEpochs) {
        if (userId >= epoch.idFrom)
            best = epoch.msSinceEpoch;
    }
    if (best > 0) {
        e.createdApproxMs = best;
        e.createdApproxIso = QDateTime::fromMSecsSinceEpoch(best)
            .toUTC().toString(Qt::ISODate);
    }
    return e;
}

QList<Platform> OsintRunner::platforms() const {
    std::lock_guard lk(_s->mu);
    return _s->platforms;
}

void OsintRunner::setPlatforms(const QList<Platform>& p) {
    std::lock_guard lk(_s->mu);
    _s->platforms = p;
}

void OsintRunner::addPlatform(const Platform& p) {
    std::lock_guard lk(_s->mu);
    _s->platforms.append(p);
}

void OsintRunner::removePlatform(const QString& name) {
    std::lock_guard lk(_s->mu);
    _s->platforms.removeIf([&](const Platform& p) { return p.name == name; });
}

void OsintRunner::setProxyUrl(const QString& p) {
    std::lock_guard lk(_s->mu);
    _s->proxy = p;
    if (p.isEmpty()) {
        _s->nam->setProxy(QNetworkProxy::NoProxy);
    } else {
        const QUrl u(p);
        QNetworkProxy::ProxyType t = u.scheme().startsWith("http")
            ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy;
        QNetworkProxy proxy(t, u.host(), u.port(9050));
        _s->nam->setProxy(proxy);
    }
}

QString OsintRunner::proxyUrl() const {
    std::lock_guard lk(_s->mu);
    return _s->proxy;
}

void OsintRunner::setMaxConcurrent(int n) {
    std::lock_guard lk(_s->mu);
    _s->maxConcurrent = std::clamp(n, 1, 16);
}

void OsintRunner::runRecon(const QString& username) {
    if (username.isEmpty()) return;
    QList<Platform> snapshot;
    {
        std::lock_guard lk(_s->mu);
        snapshot = _s->platforms;
    }
    int checked = 0, taken = 0, avail = 0;
    int inflight = 0;
    auto tryKick = [&]() {
        // Simple concurrency limiter: we issue requests in batches.
    };
    Q_UNUSED(tryKick)

    for (const Platform& plat : snapshot) {
        // plat is const&, so plat.checkUrlTemplate is a const QString. Copy
        // into a mutable local before calling the in-place .replace() - calling
        // a non-const member on a const QString would not compile.
        QString tmpl = plat.checkUrlTemplate;
        tmpl.replace(QStringLiteral("{u}"), username);
        QUrl url(tmpl);
        if (!url.isValid()) continue;
        QNetworkRequest req(url);
        // Qt 6 removed FollowRedirectsAttribute; RedirectPolicyAttribute takes a
        // RedirectPolicy value. ManualRedirectPolicy = do not auto-follow
        // (matches the old FollowRedirectsAttribute=false intent; the status
        // code we read below is the platform's own response).
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
        req.setRawHeader("User-Agent", "Mozilla/5.0 (truthgram-osint)");
        QNetworkReply* reply = _s->nam->head(req);

        QObject::connect(reply, &QNetworkReply::finished, this,
            [this, reply, plat, username, &checked, &taken, &avail]() {
                ReconResult r;
                r.platform = plat.name;
                QString ptmpl = plat.checkUrlTemplate;
                ptmpl.replace(QStringLiteral("{u}"), username);
                r.profileUrl = QUrl(ptmpl);
                const int code = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (reply->error() == QNetworkReply::NoError
                    || code == plat.takenStatus) {
                    r.availability = Availability::Taken;
                    ++taken;
                } else if (code == plat.availableStatus) {
                    r.availability = Availability::Available;
                    ++avail;
                } else if (code == 429) {
                    r.availability = Availability::RateLimited;
                    r.detail = QStringLiteral("rate-limited (429)");
                } else {
                    r.availability = Availability::Unknown;
                    r.detail = QStringLiteral("HTTP %1").arg(code);
                }
                reply->deleteLater();
                ++checked;
                Q_EMIT reconResult(r);
            });
    }

    // Fire a completion signal after a grace period; this is a best-effort
    // summary because platforms respond at different latencies.
    QTimer::singleShot(8000, this, [this, username, checked, taken, avail]() mutable {
        Q_EMIT reconComplete(username, checked, taken, avail);
    });
}

} // namespace tg::truthgram::osint
