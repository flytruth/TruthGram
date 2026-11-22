// plugin_sdk.h
//
// Public Plugin SDK ABI for TruthGram. This header is the ONLY TruthGram
// surface a third-party plugin may #include. It is kept ABI-stable across
// TruthGram releases: forward compatibility is achieved by versioning the
// HostApi vtable and adding new function pointers only at the end.
//
// Plugins export four C-linkage symbols (declared at the bottom of this file).
// The host dlopen()s the library, resolves those symbols, and calls
// truthgram_plugin_entry(api) once with a populated HostApi.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include <cstdint>
#include <cstddef>

// Forward-declare Qt types used across the ABI boundary. Plugins must link Qt
// themselves; the ABI passes QWidget* as an opaque pointer in the C signature
// but typed in the C++ convenience layer.
class QWidget;
class QString;

namespace tg::truthgram::plugin {

// ---------------------------------------------------------------------------
// Versioning
// ---------------------------------------------------------------------------
inline constexpr std::uint32_t kApiVersion = 1;

// ---------------------------------------------------------------------------
// Log levels
// ---------------------------------------------------------------------------
enum class LogLevel : std::uint8_t {
    Verbose = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
};

// ---------------------------------------------------------------------------
// Reserved widget slots the plugin may inject into. Adding new slots here is
// ABI-safe because the host treats unknown values as "no-op".
// ---------------------------------------------------------------------------
enum class WidgetSlot : std::uint8_t {
    ChatHeaderLeft    = 0,
    ChatHeaderRight   = 1,
    SidebarFooter     = 2,
    MiniPlayerSlot    = 3,
    SettingsPage      = 4,
    ComposerLeft      = 5,
    ComposerRight     = 6,
    UserInfoPanel     = 7,
};

// ---------------------------------------------------------------------------
// Event types the plugin can subscribe to. Reserved ids < 64 are host-defined;
// plugins use ids >= 64 for inter-plugin events.
// ---------------------------------------------------------------------------
enum class EventType : std::uint16_t {
    ChatOpened          = 1,
    ChatClosed          = 2,
    MessageReceived     = 3,
    MessageSent         = 4,
    LinkClicked         = 5,
    VoiceCaptureStarted= 6,
    VoiceCaptureStopped= 7,
    WebAppLaunched      = 8,
    ThemeChanged        = 9,
    GhostToggleChanged  = 10,
    PluginLoaded        = 11,
    PluginUnloaded      = 12,
};

// Opaque payload passed with events. Plugins must NOT retain the pointer past
// the callback; copy what they need.
struct EventPayload {
    EventType type{};
    std::uint64_t peerId{};       // 0 if not applicable
    std::uint64_t messageId{};    // 0 if not applicable
    const char*  text{};          // UTF-8, may be null
    const void*  blob{};          // type-specific
    std::size_t  blobLen{};
};

// ---------------------------------------------------------------------------
// Settings row a plugin can register into Settings > TruthGram > Plugins.
// ---------------------------------------------------------------------------
struct Setting {
    const char* key;             // unique within the plugin, UTF-8
    const char* label;           // human-readable
    const char* tooltip;         // optional, may be null
    enum class Kind : std::uint8_t { Toggle, Slider, Text, Button } kind;
    int    intValue{};           // current value for Toggle/Slider
    int    intMin{};
    int    intMax{};
    const char* textValue{};     // current value for Text
};

// ---------------------------------------------------------------------------
// Sync packet (pushed through Saved Messages by cloud_sync). Capped at 4 KiB
// per call; the host chunks larger state automatically.
// ---------------------------------------------------------------------------
struct SyncPacket {
    const char* key;             // logical name, e.g. "hello_plugin.state"
    const void* data;
    std::size_t length;
};

// ---------------------------------------------------------------------------
// HostApi — the vtable the host populates and hands to the plugin.
// ---------------------------------------------------------------------------
// This is a pure struct of function pointers; adding new pointers at the end
// is ABI-safe. Plugins compiled against an older header simply do not see the
// newer pointers.
struct HostApi {
    std::uint32_t api_version;   // == kApiVersion at the time of build

    // --- Logging (writes to TruthGram's local debug log only) ---
    void (*log)(LogLevel level, const char* message);

    // --- Settings registration ---
    void (*register_setting)(const Setting* setting);
    void (*get_setting_int)(const char* key, int* out);
    void (*set_setting_int)(const char* key, int value);
    void (*get_setting_text)(const char* key, const char** out);
    void (*set_setting_text)(const char* key, const char* value);

    // --- UI injection ---
    // The host takes ownership of the QWidget* parenting; the plugin still
    // owns the underlying C++ object until truthgram_plugin_shutdown.
    void (*inject_widget)(WidgetSlot slot, QWidget* widget);
    void (*remove_widget)(QWidget* widget);

    // --- Event subscription ---
    // callback receives an opaque `subscriber` pointer the plugin must pass
    // back when unsubscribing.
    using EventCallback = void (*)(void* subscriber, const EventPayload* payload);
    void* (*subscribe)(EventType type, EventCallback cb, void* subscriber);
    void  (*unsubscribe)(void* token);

    // --- Local storage (namespaced SQLCipher table) ---
    // Each plugin gets an isolated table: "plugin_<id>". The plugin never
    // sees the raw DB handle.
    void (*storage_put)(const char* key, const void* data, std::size_t len);
    bool (*storage_get)(const char* key, void* out, std::size_t cap,
                        std::size_t* written);
    void (*storage_del)(const char* key);

    // --- Cloud sync publish (encrypted through Saved Messages) ---
    void (*sync_publish)(const SyncPacket* packet);

    // --- Network (gated by user consent + Tor/SOCKS5 + allow-list) ---
    // fetch_url performs a GET and returns the body as a heap-allocated
    // buffer the plugin must free with free_response.
    struct UrlResponse {
        int         status;
        const char* body;     // UTF-8, null-terminated
        std::size_t length;
    };
    UrlResponse (*fetch_url)(const char* url);
    void        (*free_response)(const UrlResponse* resp);

    // --- Host queries ---
    const char* (*host_version)();        // "1.0.0"
    std::uint32_t (*host_api_version)();  // == kApiVersion
    const char* (*data_dir)();            // plugin's private data dir
};

// ---------------------------------------------------------------------------
// Plugin-side C entry points. The host resolves these by name from the shared
// library. Only truthgram_plugin_entry is mandatory; the others are optional
// and may be omitted (the host treats their absence as a no-op).
// ---------------------------------------------------------------------------
extern "C" {

// Called once on load with a populated HostApi. Return 0 on success.
int truthgram_plugin_entry(const tg::truthgram::plugin::HostApi* api);

// Optional lifecycle hooks.
void truthgram_plugin_enable();
void truthgram_plugin_disable();
void truthgram_plugin_shutdown();

// Optional: declare the plugin's API version at compile time so the host can
// refuse to load a plugin built against a newer SDK than itself.
std::uint32_t truthgram_plugin_api_version();

} // extern "C"

} // namespace tg::truthgram::plugin
