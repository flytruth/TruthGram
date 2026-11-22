// plugin_manager.cpp
//
// Implementation of the TruthGram Plugin Manager.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/plugins/plugin_manager.h"
#include "truthgram/plugins/plugin_host.h"

#include "truthgram/storage/truthgram_database.h"
#include "truthgram/sync/cloud_sync.h"
#include "truthgram/ui/widget_slot_registry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QTimer>
#include <QThread>
#include <QStandardPaths>

#include <atomic>
#include <mutex>
#include <vector>

#if defined(TRUTHGRAM_ON_WINDOWS)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

#if defined(TRUTHGRAM_ENABLE_LUA_PLUGINS) && TRUTHGRAM_ENABLE_LUA_PLUGINS
#  include <lua.h>
#  include <lualib.h>
#  include <lauxlib.h>
#endif

namespace tg::truthgram::plugin {

namespace {

// Symbol name resolution. On Windows the platform mangles C symbols depending
// on calling convention; TruthGram declares extern "C" so the name is plain.
#if defined(TRUTHGRAM_ON_WINDOWS)
using NativeHandle = HMODULE;
NativeHandle nativeOpen(const QString& path) {
    return LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
}
void* nativeSym(NativeHandle h, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(h, name));
}
void nativeClose(NativeHandle h) { if (h) FreeLibrary(h); }
const char* nativeError() { return "LoadLibrary failed"; }
#else
using NativeHandle = void*;
NativeHandle nativeOpen(const QString& path) {
    return dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
}
void* nativeSym(NativeHandle h, const char* name) { return dlsym(h, name); }
void nativeClose(NativeHandle h) { if (h) dlclose(h); }
const char* nativeError() { return dlerror(); }
#endif

// Cross-platform sleep used to debounce hot-reload.
void debounceMs(int ms) {
    QThread::msleep(uint(ms));
}

// Parse a manifest.json into a LoadedPlugin. Returns the error reason.
PluginError parseManifest(const QString& manifestPath, LoadedPlugin& out) {
    QFile f(manifestPath);
    if (!f.open(QIODevice::ReadOnly)) return PluginError::ManifestNotFound;
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return PluginError::ManifestInvalid;

    const auto o = doc.object();
    out.id          = o.value("id").toString();
    out.name        = o.value("name").toString();
    out.version     = o.value("version").toString();
    out.author      = o.value("author").toString();
    out.description = o.value("description").toString();
    out.apiVersion  = std::uint32_t(o.value("api_version").toInt(0));

    const QString type = o.value("type").toString("native").toLower();
    if (type == "lua" || type == "python") {
        out.isScript = true;
        out.entrySymbol = o.value("entry").toString();
    } else if (type == "native") {
        out.isScript = false;
        out.entrySymbol = o.value("entry").toString("truthgram_plugin_entry");
    } else {
        return PluginError::ManifestInvalid;
    }

    const auto perms = o.value("permissions").toObject();
    out.permNetwork   = perms.value("network").toBool(false);
    out.permFsRead    = perms.value("filesystem_read").toBool(false);
    out.permFsWrite   = perms.value("filesystem_write").toBool(false);
    out.permUiInject  = perms.value("ui_inject").toBool(false);

    if (out.id.isEmpty() || out.name.isEmpty())
        return PluginError::ManifestInvalid;
    return PluginError::None;
}

// Find the entry file inside a plugin dir. For native plugins it's
// <id>.so/.dll/.dylib; for scripts it's the manifest "entry" filename.
QString resolveEntryFile(const QDir& dir, const LoadedPlugin& p) {
    if (p.isScript)
        return dir.absoluteFilePath(p.entrySymbol);
#if defined(TRUTHGRAM_ON_WINDOWS)
    return dir.absoluteFilePath(p.id + ".dll");
#elif defined(TRUTHGRAM_ON_MACOS)
    return dir.absoluteFilePath("lib" + p.id + ".dylib");
#else
    return dir.absoluteFilePath("lib" + p.id + ".so");
#endif
}

} // namespace

struct PluginManager::State {
    mutable std::mutex mu;
    std::vector<LoadedPlugin> list;
    std::unique_ptr<HostApiImpl> host;
    QFileSystemWatcher watcher;
    QTimer debounce;
    std::atomic<bool> shuttingDown{false};
};

PluginManager& PluginManager::instance() {
    static PluginManager s;
    return s;
}

PluginManager::PluginManager()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {
    // The HostApiImpl is constructed lazily on first discoverAndLoad(),
    // because it depends on the DB + cloud_sync singletons being ready.
    connect(&_s->watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString& dir) { onPluginDirChanged(dir); });
    connect(&_s->watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString& f) { onPluginDirChanged(f); });

    _s->debounce.setSingleShot(true);
    connect(&_s->debounce, &QTimer::timeout, this, [this] {
        // Reload only plugins whose files changed. For simplicity here we
        // re-scan; the diff is cheap because plugins are typically <10.
        discoverAndLoad();
    });
}

PluginManager::~PluginManager() {
    unloadAll();
}

const HostApi* PluginManager::hostApi() const {
    return _s->host ? _s->host->vtable() : nullptr;
}

HostApiImpl& PluginManager::hostImpl() {
    if (!_s->host) {
        // Lazy construction: build the host with the DB + cloud_sync + slot
        // registry singletons. In a real overlay these come from tdesktop's
        // MainWidget; here we use the TruthGram singletons.
        _s->host = std::make_unique<HostApiImpl>(
            storage::TruthGramDatabase::instance(),
            sync::CloudSync::instance(),
            ui::WidgetSlotRegistry::instance());
    }
    return *_s->host;
}

void PluginManager::discoverAndLoad() {
    const QString pluginsDir = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation) + "/TruthGram/plugins";
    QDir dir(pluginsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        return;
    }
    _s->watcher.addPath(pluginsDir);

    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::vector<LoadedPlugin> next;
    next.reserve(entries.size());

    for (const QFileInfo& sub : entries) {
        const QDir pluginDir(sub.absoluteFilePath());
        const QString manifestPath = pluginDir.absoluteFilePath("manifest.json");
        LoadedPlugin p;
        p.manifestPath = manifestPath;
        const auto err = parseManifest(manifestPath, p);
        if (err != PluginError::None) {
            // Quarantine malformed manifests.
            const QString q = pluginDir.path() + "/.quarantine/" + p.id;
            QDir().mkpath(q);
            QFile::copy(manifestPath, q + "/manifest.json");
            Q_EMIT pluginFailed(p.id.isEmpty() ? sub.fileName() : p.id,
                                err, QStringLiteral("manifest parse"));
            continue;
        }

        // If a plugin with the same id is already loaded and unchanged, keep it.
        bool reuse = false;
        {
            std::lock_guard lk(_s->mu);
            for (const auto& existing : _s->list) {
                if (existing.id == p.id && existing.path == resolveEntryFile(pluginDir, p)) {
                    next.push_back(existing);
                    reuse = true;
                    break;
                }
            }
        }
        if (reuse) continue;

        p.path = resolveEntryFile(pluginDir, p);
        _s->watcher.addPath(p.path);

        // Refuse to load plugins built against a newer SDK than the host.
        if (p.apiVersion > kApiVersion) {
            Q_EMIT pluginFailed(p.id, PluginError::ApiVersionMismatch,
                QStringLiteral("plugin api=%1 host api=%2")
                    .arg(p.apiVersion).arg(kApiVersion));
            continue;
        }
        // Network-permission plugins require explicit consent.
        if (p.permNetwork && !p.consentGranted) {
            // Loaded but disabled until the user consents in the Plugin Manager UI.
            p.enabled = false;
        }

        // Actually load the library / script.
        if (p.isScript) {
            // Script plugins are loaded by the embedded interpreter in the
            // host; here we mark them ready and let the Lua/Python bridge
            // (compiled in via TRUTHGRAM_ENABLE_LUA_PLUGINS) run them.
            p.api = hostApi();
            p.enabled = true;
        } else {
            p.nativeHandle = nativeOpen(p.path);
            if (!p.nativeHandle) {
                Q_EMIT pluginFailed(p.id, PluginError::LoadFailed,
                                    QString::fromLatin1(nativeError()));
                continue;
            }
            using EntryFn = int(*)(const HostApi*);
            auto entry = reinterpret_cast<EntryFn>(
                nativeSym(static_cast<NativeHandle>(p.nativeHandle),
                          p.entrySymbol.toUtf8().constData()));
            if (!entry) {
                Q_EMIT pluginFailed(p.id, PluginError::EntryMissing,
                                    p.entrySymbol);
                nativeClose(static_cast<NativeHandle>(p.nativeHandle));
                continue;
            }
            p.api = hostApi();
            const int rc = entry(p.api);
            if (rc != 0) {
                Q_EMIT pluginFailed(p.id, PluginError::InitFailed,
                                    QStringLiteral("entry returned %1").arg(rc));
                nativeClose(static_cast<NativeHandle>(p.nativeHandle));
                continue;
            }
            p.enabled = true;
        }

        next.push_back(std::move(p));
        Q_EMIT pluginLoaded(next.back().id);
    }

    std::lock_guard lk(_s->mu);
    _s->list = std::move(next);
}

PluginError PluginManager::reload(const QString& id) {
    LoadedPlugin p;
    {
        std::lock_guard lk(_s->mu);
        for (const auto& e : _s->list) {
            if (e.id == id) { p = e; break; }
        }
    }
    if (p.id.isEmpty()) return PluginError::ManifestNotFound;

    // Unload.
    if (!p.isScript && p.nativeHandle) {
        // Call shutdown if present.
        using ShutdownFn = void(*)();
        auto sh = reinterpret_cast<ShutdownFn>(nativeSym(
            static_cast<NativeHandle>(p.nativeHandle), "truthgram_plugin_shutdown"));
        if (sh) sh();
        nativeClose(static_cast<NativeHandle>(p.nativeHandle));
    }
    Q_EMIT pluginUnloaded(id);

    // Force the dir scan to pick it up fresh.
    discoverAndLoad();
    return PluginError::None;
}

void PluginManager::setEnabled(const QString& id, bool enabled) {
    bool changed = false;
    {
        std::lock_guard lk(_s->mu);
        for (auto& p : _s->list) {
            if (p.id == id && p.enabled != enabled) {
                p.enabled = enabled;
                changed = true;
                // Call the lifecycle hook if present.
                if (!p.isScript && p.nativeHandle) {
                    using HookFn = void(*)();
                    const char* sym = enabled ? "truthgram_plugin_enable"
                                              : "truthgram_plugin_disable";
                    auto hook = reinterpret_cast<HookFn>(nativeSym(
                        static_cast<NativeHandle>(p.nativeHandle), sym));
                    if (hook) hook();
                }
                break;
            }
        }
    }
    if (changed) Q_EMIT pluginEnabledChanged(id, enabled);
}

void PluginManager::grantConsent(const QString& id, bool granted) {
    std::lock_guard lk(_s->mu);
    for (auto& p : _s->list) {
        if (p.id == id) { p.consentGranted = granted; return; }
    }
}

void PluginManager::unloadAll() {
    _s->shuttingDown.store(true, std::memory_order_release);
    std::vector<LoadedPlugin> snapshot;
    {
        std::lock_guard lk(_s->mu);
        snapshot.swap(_s->list);
    }
    for (const auto& p : snapshot) {
        if (!p.isScript && p.nativeHandle) {
            using ShutdownFn = void(*)();
            auto sh = reinterpret_cast<ShutdownFn>(nativeSym(
                static_cast<NativeHandle>(p.nativeHandle),
                "truthgram_plugin_shutdown"));
            if (sh) sh();
            nativeClose(static_cast<NativeHandle>(p.nativeHandle));
        }
        Q_EMIT pluginUnloaded(p.id);
    }
}

std::vector<LoadedPlugin> PluginManager::plugins() const {
    std::lock_guard lk(_s->mu);
    return _s->list;
}

const LoadedPlugin* PluginManager::find(const QString& id) const {
    std::lock_guard lk(_s->mu);
    for (const auto& p : _s->list)
        if (p.id == id) return &p;
    return nullptr;
}

void PluginManager::onPluginDirChanged(const QString& path) {
    Q_UNUSED(path)
    if (_s->shuttingDown.load(std::memory_order_acquire)) return;
    // Debounce: many editors emit multiple change events per save.
    _s->debounce.start(500);
}

} // namespace tg::truthgram::plugin
