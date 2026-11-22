// truthgram_init.cpp
//
// TruthGram startup bootstrap implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/truthgram_init.h"

#include "truthgram/theme/truthgram_theme.h"
#include "truthgram/storage/truthgram_database.h"
#include "truthgram/sync/cloud_sync.h"
#include "truthgram/plugins/plugin_manager.h"
#include "truthgram/voice/voice_changer.h"
#include "truthgram/ghost/ghost_controls.h"
#include "truthgram/phishing/phishing_guard.h"
#include "truthgram/webapp/webapp_inspector.h"
#include "truthgram/osint/osint_runner.h"
#include "truthgram/screen/screen_shield.h"
#include "truthgram/chess/tg_chess.h"
#include "truthgram/botstudio/botfather_studio.h"
#include "truthgram/termux/termux_bridge.h"
#include "truthgram/ios_ui/ios_ui_engine.h"

#include <QStandardPaths>
#include <QDir>
#include <QGuiApplication>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstring>

namespace tg::truthgram {

namespace {

// Derive two 32-byte keys from the MTProto auth key via HKDF-SHA256:
//   - the SQLCipher DB key  (info: "truthgram.db.v1")
//   - the cloud_sync AES key (info: "truthgram.cloud_sync.v1")
bool deriveKeys(const unsigned char authKey[32],
                unsigned char dbKey[32],
                unsigned char syncKey[32]) {
    auto hkdf = [](const unsigned char* ikm, const char* info, unsigned char out[32]) -> bool {
        unsigned char prk[EVP_MAX_MD_SIZE] = {0};
        unsigned int prkLen = 0;
        // Salt is empty (HKDF extract with salt=0 is HMAC(key=0, ikm)).
        if (!HMAC(EVP_sha256(), nullptr, 0, ikm, 32, prk, &prkLen)) return false;
        EVP_PKEY* pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, prk, int(prkLen));
        if (!pkey) return false;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        bool ok = false;
        if (ctx && EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1) {
            if (EVP_DigestSignUpdate(ctx, info, std::strlen(info)) == 1) {
                std::size_t sigLen = 32;
                ok = (EVP_DigestSignFinal(ctx, out, &sigLen) == 1 && sigLen == 32);
            }
        }
        if (ctx) EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return ok;
    };
    if (!hkdf(authKey, "truthgram.db.v1",        dbKey))   return false;
    if (!hkdf(authKey, "truthgram.cloud_sync.v1", syncKey)) return false;
    return true;
}

std::once_flag g_initOnce;
std::once_flag g_shutdownOnce;

} // namespace

void initialize(QWidget* mainWindow, const unsigned char authKey[32]) {
    std::call_once(g_initOnce, [mainWindow, authKey]() {
        // 1. Keys.
        unsigned char dbKey[32]{};
        unsigned char syncKey[32]{};
        if (!deriveKeys(authKey, dbKey, syncKey)) {
            qWarning() << "[truthgram_init] key derivation failed";
            return;
        }

        // 2. Open the encrypted local DB at the standard TruthGram data path.
        const QString dataDir = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation) + "/TruthGram";
        QDir().mkpath(dataDir);
        const QString dbPath = dataDir + "/truthgram.db";
        if (!storage::TruthGramDatabase::instance().open(dbPath, dbKey)) {
            qWarning() << "[truthgram_init] DB open failed at" << dbPath;
        }

        // 3. Cloud sync (must come before plugins so plugin state can sync).
        sync::CloudSync::instance().setSessionKey(syncKey);

        // 4. Theme — apply the built-in Cyber-Truth palette immediately so
        //    the very first paint of the main window is on-brand.
        theme::TruthGramTheme::instance().loadBuiltinCyberTruth();
        // When the live customizer changes the accent, publish a sync packet.
        QObject::connect(&theme::TruthGramTheme::instance(),
            &theme::TruthGramTheme::accentChanged, [](const theme::AccentOverride&) {
                sync::cloudSync().markDirty();
            });
        // When an incoming sync packet is imported, re-read RuntimeConfig.
        QObject::connect(&sync::cloudSync(), &sync::CloudSync::stateImported,
            &theme::TruthGramTheme::instance(), []() {
                // The real overlay reloads RuntimeConfig from the DB and
                // re-applies accent + ghost + ios toggles.
            });

        // 5. iOS UI engine + screen shield + mini player (UI subsystems).
#if defined(TRUTHGRAM_ENABLE_IOS_UI) && TRUTHGRAM_ENABLE_IOS_UI
        ios_ui::IosUiEngine::instance().installOn(mainWindow);
#endif
#if defined(TRUTHGRAM_ENABLE_SCREEN_SHIELD) && TRUTHGRAM_ENABLE_SCREEN_SHIELD
        screen::ScreenShield::instance().installOn(mainWindow);
#endif

        // 6. Privacy / power modules. Each is a singleton that lazily
        //    initializes on first use; we touch them here so their signals
        //    are wired before any chat is opened.
#if defined(TRUTHGRAM_ENABLE_GHOST) && TRUTHGRAM_ENABLE_GHOST
        ghost::GhostController::instance();
#endif
#if defined(TRUTHGRAM_ENABLE_PHISHING) && TRUTHGRAM_ENABLE_PHISHING
        phishing::PhishingGuard::instance();
#endif
#if defined(TRUTHGRAM_ENABLE_WEBAPP_AUDIT) && TRUTHGRAM_ENABLE_WEBAPP_AUDIT
        webapp::WebAppInspector::instance();
#endif
#if defined(TRUTHGRAM_ENABLE_OSINT) && TRUTHGRAM_ENABLE_OSINT
        osint::OsintRunner::instance();
        // OSINT defaults to Tor SOCKS5 on 9050 if the user installed Tor.
        // We do NOT auto-enable; the user must configure a proxy explicitly.
#endif
#if defined(TRUTHGRAM_ENABLE_VOICE_CHANGER) && TRUTHGRAM_ENABLE_VOICE_CHANGER
        voice::VoiceChanger::instance();
#endif
#if defined(TRUTHGRAM_ENABLE_CHESS) && TRUTHGRAM_ENABLE_CHESS
        chess::TgChess::instance();
#endif
#if defined(TRUTHGRAM_ENABLE_BOT_STUDIO) && TRUTHGRAM_ENABLE_BOT_STUDIO
        botstudio::BotFatherStudio::instance();
#endif
#if defined(TRUTHGRAM_ENABLE_TERMUX_BRIDGE) && TRUTHGRAM_ENABLE_TERMUX_BRIDGE
        termux::TermuxBridge::instance().start();
#endif

        // 7. Plugin discovery LAST so the HostApiImpl can see every service.
#if defined(TRUTHGRAM_ENABLE_PLUGINS) && TRUTHGRAM_ENABLE_PLUGINS
        plugin::PluginManager::instance().discoverAndLoad();
#endif

        qDebug() << "[truthgram_init] initialized; version"
                 << kVersionString;
    });
}

void shutdown() {
    std::call_once(g_shutdownOnce, []() {
#if defined(TRUTHGRAM_ENABLE_TERMUX_BRIDGE) && TRUTHGRAM_ENABLE_TERMUX_BRIDGE
        termux::TermuxBridge::instance().stop();
#endif
#if defined(TRUTHGRAM_ENABLE_PLUGINS) && TRUTHGRAM_ENABLE_PLUGINS
        plugin::PluginManager::instance().unloadAll();
#endif
        // Cloud sync's destructor does a best-effort final publish.
        sync::cloudSync().publishNow();
        storage::TruthGramDatabase::instance().close();
        qDebug() << "[truthgram_init] shutdown complete";
    });
}

} // namespace tg::truthgram
