# TruthGram — Architecture Blueprint

> **TruthGram** is a fully open-source (GPL-3.0), privacy-first, zero-telemetry
> Telegram Desktop client. It is a fork of `telegramdesktop/tdesktop` built on
> C++20, Qt 6, `lib_ui`, `lib_tl`, `lib_base`, and `tg_owt`. This document is
> the authoritative engineering blueprint: every module described here has a
> matching header / source file under `Telegram/SourceFiles/truthgram/`.

## 1. Design Pillars

| Pillar | Mechanism |
| --- | --- |
| **Zero telemetry** | No analytics SDKs, no closed blobs. All network traffic is MTProto-only, except user-triggered opt-in OSINT / phishing checks routed through Tor/SOCKS5. |
| **Local-first storage** | SQLite + SQLCipher for plugin configs, settings, keys. No external cloud. |
| **Cross-device sync without a cloud** | Encrypted JSON/protobuf sync packets pushed through the user's own **Saved Messages**, reparsed by every TruthGram instance logged into the same account. |
| **Hot-swappable everything** | Themes, plugins (native `.so`/`.dll`/`.dylib` + sandboxed Lua/Python), and DSP voice presets all hot-load without restart. |
| **Upstream compatibility** | TruthGram remains a strict superset of tdesktop. Every upstream feature works unchanged; TruthGram only adds optional modules gated behind `tg::truthgram::Config`. |

## 2. Repository & Directory Structure

```
TruthGram/
├── README.md
├── ARCHITECTURE.md                 # this file
├── LICENSE                         # GPL-3.0
├── CMakeLists.txt                  # root build, adds tg_truthgram target
├── cmake/
│   ├── TruthGramConfig.cmake       # TruthGramTargets export
│   ├── Platform.cmake              # WIN32 / LINUX / APPLE guards, helper macros
│   ├── Dependencies.cmake          # Qt6, tg_owt, lib_tl, lib_ui, SQLCipher, etc.
│   └── CompilerFlags.cmake         # C++20, /permissive-, -Wall -Wextra, LTO
├── docs/
│   ├── BUILD.md                    # per-OS build instructions
│   ├── PLUGIN_API.md               # plugin SDK reference
│   └── SYNC_PROTOCOL.md            # Saved Messages packet format
├── resources/
│   └── themes/
│       └── cyber-truth.tdesktop-theme   # built-in Cyber-Truth palette
├── packaging/
│   ├── windows/
│   │   ├── build.ps1               # MSVC + Ninja portable + installer
│   │   ├── TruthGram.iss           # Inno Setup installer script
│   │   └── portable.ps1
│   ├── debian/
│   │   ├── control                 # debian/control template
│   │   └── build-deb.sh            # dpkg-deb packaging
│   ├── arch/
│   │   └── PKGBUILD                # AUR: truthgram-git
│   ├── macos/
│   │   ├── Info.plist
│   │   └── build-dmg.sh            # universal2 .dmg via create-dmg
│   └── appimage/
│       └── AppImageBuilder.yml     # linuxdeploy + AppImageBuilder
├── plugins/
│   └── hello_plugin/
│       ├── manifest.json
│       ├── hello_plugin.cpp
│       └── CMakeLists.txt
└── Telegram/
    └── SourceFiles/
        └── truthgram/              # all TruthGram modules live here
            ├── truthgram_config.h      # compile-time + runtime feature flags
            ├── truthgram_pch.h         # precompiled header
            ├── truthgram_init.h/.cpp   # bootstraps every subsystem on launch
            ├── theme/
            │   ├── truthgram_theme.h/.cpp     # Cyber-Truth palette + QPalette override
            ├── ios_ui/
            │   ├── ios_ui_engine.h/.cpp       # frosted glass, springs, floating nav
            │   ├── frosted_sidebar.h/.cpp
            │   └── mini_player.h/.cpp         # detachable FFT pill player
            ├── plugins/
            │   ├── plugin_sdk.h               # public SDK header
            │   ├── plugin_host.h               # ABI stable host interface
            │   └── plugin_manager.h/.cpp       # discovery, load, sandbox, Lua/Python
            ├── sync/
            │   └── cloud_sync.h/.cpp           # Saved Messages encrypted sync packets
            ├── voice/
            │   └── voice_changer.h/.cpp        # tg_owt / Media::Capture DSP hook
            ├── ghost/
            │   └── ghost_controls.h/.cpp       # read/unread floating action bar
            ├── phishing/
            │   └── phishing_guard.h/.cpp       # IDN homograph + unshorten + VirusTotal
            ├── webapp/
            │   └── webapp_inspector.h/.cpp     # Mini App domain + TLS + ASN badge
            ├── osint/
            │   └── osint_runner.h/.cpp          # Sherlock-style username recon via Tor
            ├── screen/
            │   └── screen_shield.h/.cpp         # WDA_EXCLUDEFROMCAPTURE / NSWindow / Wayland
            ├── chess/
            │   └── tg_chess.h/.cpp              # serverless P2P chess over chat messages
            ├── botstudio/
            │   └── botfather_studio.h/.cpp      # visual BotFather command builder
            ├── termux/
            │   └── termux_bridge.h/.cpp         # encrypted local WebSocket/UDS IPC
            ├── storage/
            │   └── truthgram_database.h/.cpp    # SQLite + SQLCipher wrapper
            └── ui/
                └── floating_action_bar.h/.cpp   # shared floating pill widget
```

## 3. Module Interaction Diagram

```
 ┌─────────────────────────────────────────────────────────────────────┐
 │                       TruthGram Application Layer                   │
 │  (extends tdesktop MainWidget / SessionController / Window::Widget) │
 └───────────────▲───────────────────────────────▲─────────────────────┘
                 │                                 │
        truthgram_init.cpp bootstraps             │
                 │                                 │
   ┌─────────────┴──────────────┐    ┌────────────┴──────────────┐
   │  UI / Theme subsystem      │    │  Plugin subsystem          │
   │  truthgram_theme           │    │  plugin_manager            │
   │  ios_ui_engine             │    │  plugin_sdk (ABI stable)   │
   │  frosted_sidebar           │◄──►│  plugin_host               │
   │  mini_player (FFT)         │    │  Lua/Python sandbox        │
   └─────────────┬──────────────┘    └────────────┬──────────────┘
                 │                                 │
                 │           ┌─────────────────────┴──────────────┐
                 │           │  cloud_sync  ◄──► Saved Messages   │
                 │           │  (encrypted JSON/protobuf packets) │
                 │           └────────────────────────────────────┘
                 │                                 │
   ┌─────────────┴─────────────────────────────────┴──────────────┐
   │                    truthgram_database (SQLCipher)             │
   └─────────────▲───────────────────────────────────▲────────────┘
                 │                                   │
   ┌─────────────┴──────────┐    ┌───────────────────┴────────────┐
   │  Privacy modules       │    │  Native integration modules     │
   │  ghost_controls        │    │  voice_changer (tg_owt hook)    │
   │  phishing_guard        │    │  screen_shield (OS WDA / NSWin) │
   │  webapp_inspector      │    │  termux_bridge (WS/UDS IPC)     │
   │  osint_runner (Tor)    │    │  tg_chess (chat-serialized)     │
   └────────────────────────┘    │  botfather_studio               │
                                 └─────────────────────────────────┘
```

## 4. Threat Model & Telemetry Boundary

1. **Outbound network allow-list** — enforced in `truthgram_network_filter` (a
   `QNetworkAccessManager` subclass) — only `*.telegram.org`, `*.t.me`, and the
   MTProto endpoints configured by `MTProto::Config` are permitted by default.
   Everything else requires an explicit, per-call user opt-in captured in the
   `cloud_sync` audit log.
2. **OSINT / phishing checks** run through a user-supplied Tor/SOCKS5 proxy and
   are scoped to a single short-lived session; queries are never cached server
   side and never proxied through TruthGram infrastructure (there is none).
3. **Plugin sandbox** — native plugins are loaded with `dlopen`/`LoadLibrary`
   but receive only the `plugin_sdk.h` ABI surface. Lua/Python scripts run in a
   restricted interpreter with `io`, `os.execute`, `socket`, and `subprocess`
   removed; network access is mediated by the host.

## 5. Sync Packet Format (Saved Messages)

The `cloud_sync` module serializes state into a compact envelope, encrypts it
with a key derived from the user's MTProto auth_key (X3DH-derived 256-bit
symmetric key, AES-256-GCM), and posts it to Saved Messages as a hidden
structured message. Other TruthGram instances detect the magic header
`TGSYNC:v1:` and re-import.

```
TGSYNC:v1:<base64( AES-256-GCM( protobuf(SyncPacket), nonce, tag ) )>
```

`SyncPacket` carries: enabled plugins + manifests, theme accent override, ghost
mode toggles, voice changer preset, screen shield state. See
`docs/SYNC_PROTOCOL.md` for the full schema.

## 6. Build Targets

| Target | Output |
| --- | --- |
| `tg_truthgram` | static library linked into the main `Telegram` executable |
| `truthgram_theme` | theme resource bundle (qrc) |
| `hello_plugin` | example native plugin `.so`/`.dll`/`.dylib` |
| `truthgram_app` | final `TruthGram` executable (renamed `Telegram`) |
