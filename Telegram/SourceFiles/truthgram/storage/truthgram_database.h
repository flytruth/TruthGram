// truthgram_database.h
//
// TruthGram local encrypted storage wrapper around SQLCipher.
// Every TruthGram module that needs persistence goes through this singleton.
// Plugin private tables are namespaced as "plugin_<id>".
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QByteArray>

#include <memory>
#include <mutex>
#include <vector>

struct sqlite3;

namespace tg::truthgram::storage {

// Scoped transaction. Rolls back on destruction unless commit() was called.
class Transaction {
public:
    explicit Transaction(class TruthGramDatabase& db);
    ~Transaction();
    void commit();
    void rollback();
private:
    TruthGramDatabase& _db;
    bool _done{false};
};

// =============================================================================
// TruthGramDatabase
// =============================================================================
class TruthGramDatabase : public QObject {
    Q_OBJECT
public:
    static TruthGramDatabase& instance();

    // Open (or create) the encrypted database at the standard TruthGram data
    // path. The key is the 32-byte AES key derived from the user's passphrase
    // / MTProto auth key. Returns false on open failure.
    bool open(const QString& path, const unsigned char key[32]);
    bool openInMemory();   // for tests
    void close();

    [[nodiscard]] bool isOpen() const;

    // Key/value helpers used by cloud_sync, theme, plugins. Each call is a
    // prepared statement; safe to call from any thread (the mutex serializes).
    bool put(const QString& table, const QString& key, const QByteArray& value);
    [[nodiscard]] QByteArray get(const QString& table, const QString& key) const;
    bool del(const QString& table, const QString& key);
    [[nodiscard]] std::vector<QString> keys(const QString& table) const;

    // Create an isolated table for a plugin. Idempotent.
    bool ensurePluginTable(const QString& pluginId);

    // Low-level exec for migrations.
    bool exec(const QString& sql);

private:
    TruthGramDatabase();
    ~TruthGramDatabase() override;
    TruthGramDatabase(const TruthGramDatabase&) = delete;
    TruthGramDatabase& operator=(const TruthGramDatabase&) = delete;

    struct State;
    std::unique_ptr<State> _s;
    friend class Transaction;
};

inline TruthGramDatabase& db() { return TruthGramDatabase::instance(); }

} // namespace tg::truthgram::storage
