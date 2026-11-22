# TruthGram Plugin API

TruthGram ships a sandboxed, hot-loadable plugin engine. A plugin is one of:

1. **Native plugin** — a single shared library (`.so` / `.dll` / `.dylib`) plus a
   `manifest.json`, dropped into `~/.local/share/TruthGram/plugins/<name>/`.
2. **Script plugin** — a `.lua` or `.py` file plus `manifest.json`, run in a
   restricted interpreter with `io`, `os.execute`, `socket`, `subprocess`, and
   `ctypes` removed. Script plugins receive the same host API surface through a
   generated binding.

Both kinds speak the same host ABI declared in
[`Telegram/SourceFiles/truthgram/plugins/plugin_sdk.h`](../Telegram/SourceFiles/truthgram/plugins/plugin_sdk.h).

## manifest.json

```json
{
  "id": "hello_plugin",
  "name": "Hello World",
  "version": "1.0.0",
  "author": "TruthGram Contributors",
  "min_host_version": "1.0.0",
  "api_version": 1,
  "type": "native",
  "entry": "hello_plugin",
  "permissions": {
    "network": false,
    "filesystem_read": false,
    "filesystem_write": false,
    "ui_inject": true
  },
  "description": "Adds a friendly greeting to the chat header."
}
```

- `type`: `"native"` | `"lua"` | `"python"`.
- `entry`: for native, the exported C symbol named in
  `truthgram_plugin_entry` (see `plugin_sdk.h`); for scripts, the entry
  filename.
- `permissions`: the host enforces these. A plugin that requests
  `network: true` is **not** loaded silently — the Plugin Manager UI shows a
  consent dialog. Even with consent, all plugin network traffic is routed
  through the user's Tor/SOCKS5 proxy when one is configured, and is
  allow-listed against the TruthGram network filter.

## Host lifecycle

```cpp
// plugin_sdk.h (excerpt)
extern "C" {
  // One-time, called on load. Return 0 to succeed.
  int truthgram_plugin_entry(const TruthGram::Plugin::HostApi* api);

  // Called when the plugin is enabled/disabled at runtime.
  void truthgram_plugin_enable();
  void truthgram_plugin_disable();

  // Called on TruthGram shutdown or hot-unload.
  void truthgram_plugin_shutdown();
}
```

`HostApi` is a vtable-stable struct that exposes:

- `register_setting(Setting)` — add a row in `Settings > TruthGram > Plugins`.
- `inject_widget(WidgetSlot, QWidget*)` — place a widget into a reserved slot
  (ChatHeaderLeft, ChatHeaderRight, SidebarFooter, MiniPlayerSlot, etc.).
- `subscribe(EventType, Callback)` — listen for `ChatOpened`,
  `MessageReceived`, `LinkClicked`, `VoiceCaptureStarted`, `WebAppLaunched`,
  `ThemeChanged`.
- `local_storage()` — read/write to the plugin's namespaced SQLCipher table.
- `sync_publish(SyncPacket)` — push a small encrypted blob through Saved
  Messages sync (capped at 4 KiB per packet; larger state is chunked).
- `log(LogLevel, message)` — write to the TruthGram debug log (never to disk
  outside the local profile).

## Example

See [`plugins/hello_plugin/hello_plugin.cpp`](../plugins/hello_plugin/hello_plugin.cpp).

## Distribution

- Plugins are loaded at startup and hot-reloaded on file change (debounced
  500 ms via `QFileSystemWatcher`).
- A plugin may declare a `min_host_version`; mismatched plugins are listed but
  disabled with a visible reason.
- Malformed manifests are quarantined under `~/.local/share/TruthGram/plugins/
  .quarantine/<id>/` and the user is notified.

## Safety guarantees

- Native plugins run in the same process (no IPC), but the host ABI is
  versioned: `api_version` mismatches cause a graceful skip, not a crash.
- Script plugins are fully sandboxed: no direct filesystem, no subprocess, no
  raw sockets. The only network path is `host.fetch_url(url)` which goes
  through the proxy + allow-list.
- Plugins cannot read the user's MTProto auth key, message history, or
  contacts directly; they receive only the events and metadata explicitly
  declared in `HostApi`.
