// truthgram_database.cpp
//
// TruthGram local encrypted storage (SQLCipher).
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/storage/truthgram_database.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QDebug>

#include <sqlite3.h>
#include <cstring>

namespace tg::truthgram::storage {

namespace {

// Quote an identifier for use in DDL. We only allow [A-Za-z0-9_] in table
// names; this guards against SQL injection from a malicious plugin id.
bool safeIdent(const QString& s, QByteArray* out) {
    if (s.isEmpty()) return false;
    for (const QChar c : s) {
        if (!(c.isLetterOrNumber() || c == '_')) return false;
    }
    *out = s.toUtf8();
    return true;
}

void traceError(sqlite3* db, int rc, const char* ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        qWarning() << "[truthgram_db]" << ctx << "rc=" << rc
                   << (db ? sqlite3_errmsg(db) : "(no db)");
    }
}

} // namespace

struct TruthGramDatabase::State {
    mutable std::mutex mu;
    sqlite3* db{nullptr};
    bool inMemory{false};
};

TruthGramDatabase& TruthGramDatabase::instance() {
    static TruthGramDatabase s;
    return s;
}

TruthGramDatabase::TruthGramDatabase()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {}

TruthGramDatabase::~TruthGramDatabase() { close(); }

bool TruthGramDatabase::openInMemory() {
    close();
    std::lock_guard lk(_s->mu);
    const int rc = sqlite3_open(":memory:", &_s->db);
    if (rc != SQLITE_OK) { traceError(_s->db, rc, "open in-memory"); return false; }
    _s->inMemory = true;
    // Create the core key/value table used by TruthGram itself.
    sqlite3_exec(_s->db,
        "CREATE TABLE IF NOT EXISTS tg_kv("
        "  k TEXT PRIMARY KEY,"
        "  v BLOB NOT NULL) WITHOUT ROWID;", nullptr, nullptr, nullptr);
    return true;
}

bool TruthGramDatabase::open(const QString& path, const unsigned char key[32]) {
    close();
    QDir().mkpath(QFileInfo(path).absolutePath());

    std::lock_guard lk(_s->mu);
    const QByteArray pathUtf8 = path.toUtf8();
    int rc = sqlite3_open(pathUtf8.constData(), &_s->db);
    if (rc != SQLITE_OK) { traceError(_s->db, rc, "open"); return false; }

    // SQLCipher key pragma: hex-encoded 32-byte key.
    QByteArray hex(64, '\0');
    static const char kHex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        hex[2*i]     = kHex[(key[i] >> 4) & 0xF];
        hex[2*i + 1] = kHex[key[i] & 0xF];
    }
    const QByteArray pragma = "PRAGMA key = \"x'" + hex + "'\";";
    rc = sqlite3_exec(_s->db, pragma.constData(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) { traceError(_s->db, rc, "PRAGMA key"); close(); return false; }

    // Verify the key actually decrypts the database.
    rc = sqlite3_exec(_s->db, "SELECT count(*) FROM sqlite_master;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) { traceError(_s->db, rc, "key verify"); close(); return false; }

    // Core TruthGram tables.
    static constexpr const char* kSchema =
        "CREATE TABLE IF NOT EXISTS tg_kv("
        "  k TEXT PRIMARY KEY,"
        "  v BLOB NOT NULL) WITHOUT ROWID;"
        "CREATE TABLE IF NOT EXISTS tg_audit("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ts INTEGER NOT NULL,"
        "  module TEXT NOT NULL,"
        "  action TEXT NOT NULL,"
        "  detail TEXT);"
        "CREATE INDEX IF NOT EXISTS tg_audit_ts ON tg_audit(ts);";
    rc = sqlite3_exec(_s->db, kSchema, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) { traceError(_s->db, rc, "schema"); close(); return false; }
    return true;
}

void TruthGramDatabase::close() {
    std::lock_guard lk(_s->mu);
    if (_s->db) {
        sqlite3_close_v2(_s->db);
        _s->db = nullptr;
    }
    _s->inMemory = false;
}

bool TruthGramDatabase::isOpen() const {
    std::lock_guard lk(_s->mu);
    return _s->db != nullptr;
}

bool TruthGramDatabase::exec(const QString& sql) {
    std::lock_guard lk(_s->mu);
    if (!_s->db) return false;
    const QByteArray s = sql.toUtf8();
    const int rc = sqlite3_exec(_s->db, s.constData(), nullptr, nullptr, nullptr);
    traceError(_s->db, rc, "exec");
    return rc == SQLITE_OK;
}

bool TruthGramDatabase::ensurePluginTable(const QString& pluginId) {
    QByteArray ident;
    if (!safeIdent(pluginId, &ident)) return false;
    const QByteArray tbl = "plugin_" + ident;
    const QByteArray sql = "CREATE TABLE IF NOT EXISTS " + tbl +
        "(k TEXT PRIMARY KEY, v BLOB NOT NULL) WITHOUT ROWID;";
    return exec(QString::fromLatin1(sql));
}

bool TruthGramDatabase::put(const QString& table, const QString& key, const QByteArray& value) {
    std::lock_guard lk(_s->mu);
    if (!_s->db) return false;
    QByteArray ident;
    if (!safeIdent(table, &ident)) return false;
    const QByteArray tbl = (table == "tg_kv") ? QByteArray("tg_kv")
                                              : (QByteArray("plugin_") + ident);
    const QByteArray sql = "INSERT OR REPLACE INTO " + tbl + "(k,v) VALUES(?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_s->db, sql.constData(), -1, &stmt, nullptr) != SQLITE_OK) {
        traceError(_s->db, SQLITE_ERROR, "put prepare");
        return false;
    }
    const QByteArray k = key.toUtf8();
    sqlite3_bind_text(stmt, 1, k.constData(), int(k.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, value.constData(), int(value.size()), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    traceError(_s->db, rc, "put step");
    return rc == SQLITE_DONE;
}

QByteArray TruthGramDatabase::get(const QString& table, const QString& key) const {
    std::lock_guard lk(_s->mu);
    if (!_s->db) return {};
    QByteArray ident;
    if (!safeIdent(table, &ident)) return {};
    const QByteArray tbl = (table == "tg_kv") ? QByteArray("tg_kv")
                                              : (QByteArray("plugin_") + ident);
    const QByteArray sql = "SELECT v FROM " + tbl + " WHERE k=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_s->db, sql.constData(), -1, &stmt, nullptr) != SQLITE_OK)
        return {};
    const QByteArray k = key.toUtf8();
    sqlite3_bind_text(stmt, 1, k.constData(), int(k.size()), SQLITE_TRANSIENT);
    QByteArray out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        const int sz = sqlite3_column_bytes(stmt, 0);
        out = QByteArray(reinterpret_cast<const char*>(blob), sz);
    }
    sqlite3_finalize(stmt);
    return out;
}

bool TruthGramDatabase::del(const QString& table, const QString& key) {
    std::lock_guard lk(_s->mu);
    if (!_s->db) return false;
    QByteArray ident;
    if (!safeIdent(table, &ident)) return false;
    const QByteArray tbl = (table == "tg_kv") ? QByteArray("tg_kv")
                                              : (QByteArray("plugin_") + ident);
    const QByteArray sql = "DELETE FROM " + tbl + " WHERE k=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_s->db, sql.constData(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    const QByteArray k = key.toUtf8();
    sqlite3_bind_text(stmt, 1, k.constData(), int(k.size()), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<QString> TruthGramDatabase::keys(const QString& table) const {
    std::lock_guard lk(_s->mu);
    std::vector<QString> out;
    if (!_s->db) return out;
    QByteArray ident;
    if (!safeIdent(table, &ident)) return out;
    const QByteArray tbl = (table == "tg_kv") ? QByteArray("tg_kv")
                                              : (QByteArray("plugin_") + ident);
    const QByteArray sql = "SELECT k FROM " + tbl + ";";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_s->db, sql.constData(), -1, &stmt, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* k = sqlite3_column_text(stmt, 0);
        if (k) out.emplace_back(QString::fromUtf8(reinterpret_cast<const char*>(k)));
    }
    sqlite3_finalize(stmt);
    return out;
}

// ---- Transaction -----------------------------------------------------------
Transaction::Transaction(TruthGramDatabase& db) : _db(db) {
    _db.exec("BEGIN IMMEDIATE;");
}
Transaction::~Transaction() { if (!_done) _db.exec("ROLLBACK;"); }
void Transaction::commit()   { if (!_done) { _db.exec("COMMIT;"); _done = true; } }
void Transaction::rollback() { if (!_done) { _db.exec("ROLLBACK;"); _done = true; } }

} // namespace tg::truthgram::storage
