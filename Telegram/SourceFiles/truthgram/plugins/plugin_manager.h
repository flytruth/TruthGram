// plugin_manager.h
//
// TruthGram Plugin Manager: discovers, loads, sandboxes, and hot-reloads
// native (.so/.dll/.dylib) and script (.lua/.py) plugins from
// ~/.local/share/TruthGram/plugins/<id>/.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram/plugins/plugin_sdk.h"
#include "truthgram/plugins/plugin_host.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QPointer>
#include <QFileSystemWatcher>

#include <atomic>
#include <memory>
#include <vector>

namespace tg::truthgram::plugin {

// One loaded plugin instance.
struct LoadedPlugin {
    QString id;             // matches manifest "id"
    QString name;
    QString version;
    QString author;
    QString description;
    QString entrySymbol;    // for native: "truthgram_plugin_entry"
    QString path;           // .so/.dll/.dylib or .lua/.py file
    QString manifestPath;
    std::uint32_t apiVersion{};
    bool isScript{};
    bool enabled{};
    bool consentGranted{};  // required if permissions.network == true

    // Native handle (dlopen returned pointer) — nullptr for scripts.
    void* nativeHandle{};
    // Pointer into the loaded library's lifetime; valid until unload().
    const HostApi* api{};

    // Permissions from the manifest.
    bool permNetwork{};
    bool permFsRead{};
    bool permFsWrite{};
    bool permUiInject{};
};

enum class PluginError {
    None,
    ManifestNotFound,
    ManifestInvalid,
    ApiVersionMismatch,
    EntryMissing,
    LoadFailed,
    InitFailed,
    Quarantined,
};

Q_NAMESPACE

// =============================================================================
// PluginManager
// =============================================================================
class PluginManager : public QObject {
    Q_OBJECT
public:
    static PluginManager& instance();

    // Scan the user plugin dir and load every plugin with a valid manifest.
    // Plugins that fail are quarantined and surfaced via pluginFailed.
    void discoverAndLoad();

    // Reload a single plugin (e.g. after the user edits it). Safe to call
    // while TruthGram is running; the plugin is unloaded then reloaded.
    PluginError reload(const QString& id);

    // Enable / disable at runtime without unloading.
    void setEnabled(const QString& id, bool enabled);

    // Toggle user consent for a plugin that requests network access.
    void grantConsent(const QString& id, bool granted);

    // Unload everything (called on shutdown).
    void unloadAll();

    [[nodiscard]] std::vector<LoadedPlugin> plugins() const;
    [[nodiscard]] const LoadedPlugin* find(const QString& id) const;

    // The host API vtable handed to plugins.
    [[nodiscard]] const HostApi* hostApi() const;

    // The HostApiImpl backing the vtable. TruthGram modules dispatch events
    // through it.
    HostApiImpl& hostImpl();

Q_SIGNALS:
    void pluginLoaded(const QString& id);
    void pluginUnloaded(const QString& id);
    void pluginFailed(const QString& id, PluginError reason, const QString& detail);
    void pluginEnabledChanged(const QString& id, bool enabled);

private:
    PluginManager();
    ~PluginManager() override;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    struct State;
    std::unique_ptr<State> _s;

    // File-watcher callback for hot reload.
    void onPluginDirChanged(const QString& path);
};

inline PluginManager& plugins() { return PluginManager::instance(); }

} // namespace tg::truthgram::plugin
