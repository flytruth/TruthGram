// plugin_host.h
//
// Host-side glue between the Plugin Manager and the ABI declared in
// plugin_sdk.h. The HostApiImpl populates the HostApi vtable with real
// implementations backed by TruthGram services (logging, SQLCipher, cloud_sync,
// the network allow-list, the UI slot registry).
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram/plugins/plugin_sdk.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QByteArray>
#include <functional>
#include <memory>
#include <vector>

namespace tg::truthgram::plugin {

// Forward declarations of TruthGram services that back the host API.
namespace storage { class TruthGramDatabase; }
namespace sync     { class CloudSync; }
namespace ui       { class WidgetSlotRegistry; }

// One subscriber registered for an event type.
struct Subscriber {
    EventType    type{};
    EventCallback cb{};
    void*        user{};
};

// Implementation of HostApi. Lifetime is tied to the PluginManager singleton.
// Constructed once at TruthGram startup; the same HostApi struct is handed to
// every loaded plugin.
class HostApiImpl {
public:
    HostApiImpl(storage::TruthGramDatabase& db,
                sync::CloudSync&            cloudSync,
                ui::WidgetSlotRegistry&     slots);
    ~HostApiImpl();

    // Returns a pointer to the vtable that gets passed to plugins. The
    // pointer is stable for the lifetime of HostApiImpl.
    const HostApi* vtable() const { return &_api; }

    // Dispatch an event to every subscriber of the given type. Called by
    // TruthGram modules (e.g. ghost_controls emits GhostToggleChanged).
    void dispatchEvent(const EventPayload& ev);

    // Register / lookup settings registered by plugins so the Settings UI
    // can render them.
    struct SettingRow {
        QString pluginId;
        Setting spec;
    };
    [[nodiscard]] std::vector<SettingRow> allSettings() const;

private:
    storage::TruthGramDatabase& _db;
    sync::CloudSync&            _cloud;
    ui::WidgetSlotRegistry&     _slots;

    HostApi _api;   // populated in the constructor; passed to plugins.

    // Subscribers keyed by EventType.
    mutable std::mutex _subMu;
    std::vector<Subscriber> _subscribers;

    // Settings rows (per-plugin). Kept here so the Settings UI can iterate
    // without reaching into each plugin's storage.
    mutable std::mutex _setMu;
    std::vector<SettingRow> _settings;

    // Plugin private data dirs, created lazily.
    QString pluginDataDir(const QString& pluginId) const;

    // ---- C-style callbacks that fill the HostApi vtable. Each one recovers
    // the HostApiImpl via a per-instance context pointer stored in a side map.
    static void cb_log(LogLevel level, const char* msg);
    static void cb_register_setting(const Setting* s);
    static void cb_get_setting_int(const char* key, int* out);
    static void cb_set_setting_int(const char* key, int value);
    static void cb_get_setting_text(const char* key, const char** out);
    static void cb_set_setting_text(const char* key, const char* value);
    static void cb_inject_widget(WidgetSlot slot, QWidget* widget);
    static void cb_remove_widget(QWidget* widget);
    static void* cb_subscribe(EventType type, EventCallback cb, void* user);
    static void  cb_unsubscribe(void* token);
    static void cb_storage_put(const char* key, const void* data, std::size_t len);
    static bool cb_storage_get(const char* key, void* out, std::size_t cap, std::size_t* written);
    static void cb_storage_del(const char* key);
    static void cb_sync_publish(const SyncPacket* packet);
    static HostApi::UrlResponse cb_fetch_url(const char* url);
    static void cb_free_response(const HostApi::UrlResponse* resp);
    static const char* cb_host_version();
    static std::uint32_t cb_host_api_version();
    static const char* cb_data_dir();

    // Side map: HostApi* -> HostApiImpl*. Used by the static C callbacks to
    // recover the instance. In practice there is exactly one HostApiImpl per
    // process; the map exists so plugins can theoretically be re-hosted.
    static std::mutex s_hostMapMu;
    static std::vector<HostApiImpl*> s_hosts;
};

} // namespace tg::truthgram::plugin
