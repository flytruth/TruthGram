// osint_runner.h
//
// TruthGram OSINT / Sherlock-style local username recon runner.
//
//   * Extends the User/Channel Info panel with raw MTProto metadata: exact
//     User ID, DC ID (Data Center), account creation date estimate (via the
//     Telegram ID range lookup table below), and common groups.
//   * A modular Sherlock-style username availability checker that runs
//     STRICTLY from the user's local machine over a Tor/SOCKS5 proxy. No
//     query ever reaches a TruthGram server (there is none).
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QList>
#include <QHash>

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

namespace tg::truthgram::osint {

// Telegram data-center table. The DC ID encodes which cluster a user lives on.
// Account-creation date estimation comes from the well-known Telegram bot API
// user-id -> timestamp ranges (approximate; not authoritative).
struct DcInfo {
    int id{};
    const char* name{};        // e.g. "Miami, FL"
    const char* ipv4{};
};

// Account-age estimate from a Telegram user id. The id encodes the
// (process_id, shard, time) triple; we use the public epoch table.
struct AccountAgeEstimate {
    qint64 userId{};
    qint64 createdApproxMs{};   // 0 if unknown
    QString createdApproxIso;   // empty if unknown
};

// One platform definition the Sherlock-style runner checks.
struct Platform {
    QString name;               // e.g. "GitHub"
    QString checkUrlTemplate;   // e.g. "https://github.com/{u}"
    QHash<QString, QString> headers;   // optional
    // The HTTP status that indicates "username TAKEN" (typically 200).
    int takenStatus{200};
    // The HTTP status that indicates "username AVAILABLE" (typically 404).
    int availableStatus{404};
};

enum class Availability {
    Taken,
    Available,
    Unknown,
    RateLimited,
    Error,
};

struct ReconResult {
    QString platform;
    Availability availability = Availability::Unknown;
    QUrl profileUrl;
    QString detail;
};

struct MtprotoMetadata {
    qint64 userId{};
    int dcId{};
    AccountAgeEstimate age;
    QStringList commonGroups;
};

Q_NAMESPACE

// =============================================================================
// OsintRunner
// =============================================================================
class OsintRunner : public QObject {
    Q_OBJECT
public:
    static OsintRunner& instance();

    // ---- MTProto metadata for the User/Channel Info panel ----
    [[nodiscard]] MtprotoMetadata metadataFor(qint64 userId) const;
    [[nodiscard]] static DcInfo dcInfo(int dcId);
    [[nodiscard]] static AccountAgeEstimate estimateAccountAge(qint64 userId);

    // ---- Sherlock-style username recon ----
    // Returns the platform catalog. TruthGram ships a curated default list;
    // users may extend it from Settings (persisted to SQLCipher + cloud_sync).
    [[nodiscard]] QList<Platform> platforms() const;
    void setPlatforms(const QList<Platform>& p);
    void addPlatform(const Platform& p);
    void removePlatform(const QString& name);

    // Run availability checks for a username across all enabled platforms.
    // All requests go through the user's Tor/SOCKS5 proxy when one is set.
    void runRecon(const QString& username);

    void setProxyUrl(const QString& socksOrHttpProxy);
    [[nodiscard]] QString proxyUrl() const;

    // Cap concurrent in-flight checks to be polite to public platforms.
    void setMaxConcurrent(int n);

Q_SIGNALS:
    void reconResult(const ReconResult& r);
    void reconComplete(const QString& username, int checked, int taken, int available);

private:
    OsintRunner();
    ~OsintRunner() override;
    OsintRunner(const OsintRunner&) = delete;
    OsintRunner& operator=(const OsintRunner&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

inline OsintRunner& osint() { return OsintRunner::instance(); }

} // namespace tg::truthgram::osint
