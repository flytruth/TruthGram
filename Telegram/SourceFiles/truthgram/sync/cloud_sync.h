// cloud_sync.h
//
// TruthGram cross-device sync over Saved Messages. Serializes per-device
// state (enabled plugins, theme accent, ghost toggles, voice preset, screen
// shield state) into an encrypted envelope and posts it to the user's own
// Saved Messages peer. Other TruthGram instances detect the magic header
// and re-import the state.
//
// See docs/SYNC_PROTOCOL.md for the full packet schema and conflict rules.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace tg::truthgram {

struct RuntimeConfig;

namespace sync {

// Magic ASCII prefix that lets every TruthGram instance recognize a sync
// packet in Saved Messages without decrypting it.
inline constexpr const char* kMagic = "TGSYNC:v1:";

// The 256-bit AES key is derived from the user's MTProto auth_key via
// HKDF-SHA256 with this info string. We never store the derived key on disk;
// it lives in memory only while TruthGram is running.
inline constexpr const char* kKeyInfo = "truthgram.cloud_sync.v1";

// What to do with an incoming packet, decided by the conflict resolver.
enum class ImportResult {
    Applied,
    Merged,
    SkippedOlder,
    SkippedMalformed,
    SkippedForeignDevice,
};

// The sync state we publish. Mirrors RuntimeConfig but is the wire format.
struct PublishRequest {
    const RuntimeConfig* runtimeConfig{};
};

// =============================================================================
// CloudSync
// =============================================================================
class CloudSync : public QObject {
    Q_OBJECT
public:
    static CloudSync& instance();

    // Set the 256-bit AES key derived from the MTProto auth_key (HKDF-SHA256,
    // info=kKeyInfo). Called once during session bootstrap. Until this is set,
    // publish() is a no-op and incoming packets are dropped.
    void setSessionKey(const unsigned char key[32]);

    // Mark local state as dirty. Debounced 30s publish to Saved Messages.
    void markDirty();

    // Force an immediate publish (used on shutdown / explicit user trigger).
    void publishNow();

    // Called by the incoming-message hook when a Saved Messages text starts
    // with kMagic. Verifies the GCM tag, decrypts, and applies.
    ImportResult importEnvelope(const QString& envelope);

    // Whether sync is enabled. Reads RuntimeConfig.cloudSyncEnabled.
    [[nodiscard]] bool enabled() const;

    // Throttle window, configurable for tests.
    void setThrottleSeconds(std::chrono::seconds s);

Q_SIGNALS:
    // Fired after a successful import so modules can refresh their UI.
    void stateImported();
    void publishQueued();
    void publishSent(qint64 messageId);
    void importFailed(const QString& reason);

private:
    CloudSync();
    ~CloudSync() override;
    CloudSync(const CloudSync&) = delete;
    CloudSync& operator=(const CloudSync&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

inline CloudSync& cloudSync() { return CloudSync::instance(); }

} // namespace sync
} // namespace tg::truthgram
