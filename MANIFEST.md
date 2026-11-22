# TruthGram — File Manifest

Generated for TruthGram v1.0.0. 68 files, ~8,700 lines of C++20 / CMake / docs.

## Documentation (5)
| File | Purpose |
| --- | --- |
| `README.md` | Project overview, highlights, build quickstart |
| `ARCHITECTURE.md` | Full engineering blueprint: pillars, directory tree, module interaction diagram, threat model, sync protocol, build targets |
| `LICENSE` | GPL-3.0 header + canonical URL reference |
| `docs/BUILD.md` | Per-OS build instructions (Windows MSVC/Ninja, Debian .deb, Arch PKGBUILD, macOS .dmg, AppImage, CMake presets, compile-time module toggles) |
| `docs/PLUGIN_API.md` | Plugin SDK reference: manifest.json schema, host lifecycle, ABI, sandbox guarantees |
| `docs/SYNC_PROTOCOL.md` | Saved Messages sync packet envelope, protobuf schema, conflict resolution, throttling/chunking, privacy |

## Build system (8)
| File | Purpose |
| --- | --- |
| `CMakeLists.txt` | Root build: `tg_truthgram` static lib + `truthgram_app` executable; 13 module on/off options; AUTOMOC/RCC/UIC |
| `CMakePresets.json` | 6 configure+build presets: windows-msvc-ninja, linux-gcc/clang-ninja, macos-arm64/x86_64-clang, debug-asan |
| `cmake/Platform.cmake` | WIN32/LINUX/APPLE guards, output dir helpers, plugin suffix helper |
| `cmake/CompilerFlags.cmake` | C++20 flags: `/permissive- /W4` (MSVC), `-Wall -Wextra -Wpedantic` (GCC/Clang); LTO, ASAN, option→define mapping |
| `cmake/Dependencies.cmake` | Qt6, OpenSSL 3, SQLCipher 4, FFmpeg 6, rnnoise, LuaJIT, tg_owt, Wayland/X11, AppKit |
| `cmake/FindSQLCipher.cmake` | find_package shim exposing `SQLCipher::SQLCipher` |
| `cmake/TruthGramConfig.cmake` | Export `TruthGram::Host` for downstream plugin toolchains |
| `Telegram/SourceFiles/truthgram/truthgram_version.h.in` | CMake configure_file template → `truthgram_version.h` |

## Packaging (8)
| File | Purpose |
| --- | --- |
| `packaging/windows/build.ps1` | MSVC+Ninja build → staging → windeployqt; `-Portable` / `-Installer` modes |
| `packaging/windows/TruthGram.iss` | Inno Setup installer script (LZMA2, arm64+x64) |
| `packaging/windows/portable.ps1` | Self-contained portable `.zip` with `--portable --workdir` launcher |
| `packaging/debian/control` | debian/control with full Build-Depends + Recommends/Suggests (tor) |
| `packaging/debian/build-deb.sh` | dpkg-deb packaging: stage tree, postinst/prerm hooks, desktop file |
| `packaging/arch/PKGBUILD` | AUR `truthgram-git`: overlays TruthGram onto pinned tdesktop tag, system Qt6 |
| `packaging/macos/Info.plist` | Universal2 app bundle; ATS forces MTProto endpoints only; mic/camera/screen usage strings |
| `packaging/macos/build-dmg.sh` | arm64+x86_64 lipo, macdeployqt, optional codesign+notarize, create-dmg/hdiutil |
| `packaging/appimage/AppImageBuilder.yml` | linuxdeploy + AppImageBuilder recipe, bundled Qt6/SQLCipher/FFmpeg |

## Resources (1)
| File | Purpose |
| --- | --- |
| `resources/themes/cyber-truth.tdesktop-theme` | Built-in Cyber-Truth palette: Obsidian Black `#0A0C0E`, Lime `#39FF14`, Ice `#38BDF8`; 70+ role keys + TruthGram `tg_*` extras |

## Core C++ modules (Telegram/SourceFiles/truthgram/)

### Bootstrap & config (5)
| File | Purpose |
| --- | --- |
| `truthgram_config.h` | Compile-time module flags (mirror CMake), `RgbColor` + brand constants, `RuntimeConfig` struct, path helpers |
| `truthgram_pch.h` | Precompiled header: std lib + Qt Core/Gui/Widgets + config |
| `truthgram_init.h` / `.cpp` | `initialize()` / `shutdown()`: HKDF key derivation → SQLCipher open → cloud_sync → theme → all singletons wired in order |
| `truthgram_version.h.in` | Version constants (rendered by CMake) |

### Theme engine (2)
| File | Purpose |
| --- | --- |
| `theme/truthgram_theme.h` / `.cpp` | `TruthGramTheme` singleton: parses `.tdesktop-theme`, builds `QPalette` override, live accent customizer (`limeFromHue`/`iceFromHue`), `tg_*` extras struct, hot-swap signals |

### iOS-inspired UI (6)
| File | Purpose |
| --- | --- |
| `ios_ui/ios_ui_engine.h` / `.cpp` | `IosUiEngine`: frosted glass, spring scroll, rounded bubbles, floating nav, swipe-to-reply toggles; `SpringScroller` QScrollArea subclass with `OutBack` easing |
| `ios_ui/frosted_sidebar.h` / `.cpp` | `FrostedSidebar`: translucent panel, 15fps backdrop capture, cheap scale-down/up blur, translucent tint |
| `ios_ui/mini_player.h` / `.cpp` | `MiniPlayer`: detachable pill, 512-pt kiss_fft, Hann window, 48 Lime→Ice interpolated bars, 0.5–2.5× speed, pitch lock, drag when detached |

### Plugin engine (4)
| File | Purpose |
| --- | --- |
| `plugins/plugin_sdk.h` | Public ABI: `HostApi` vtable, `kApiVersion=1`, `WidgetSlot`/`EventType`/`Setting`/`SyncPacket`, 4 C entry points |
| `plugins/plugin_host.h` | `HostApiImpl`: backs the vtable with DB/cloud/slots; static C callbacks recover the instance |
| `plugins/plugin_manager.h` / `.cpp` | `PluginManager`: manifest parse, dlopen/LoadLibrary, API-version gate, network-consent gate, quarantine, `QFileSystemWatcher` hot-reload (500ms debounce), Lua/Python hooks |

### Cloud sync (2)
| File | Purpose |
| --- | --- |
| `sync/cloud_sync.h` / `.cpp` | `CloudSync`: AES-256-GCM (OpenSSL EVP) of JSON wire packet, 12-byte device-id/seq/type header, 30s debounce, last-writer-wins import, foreign-device echo skip |

### Storage (2)
| File | Purpose |
| --- | --- |
| `storage/truthgram_database.h` / `.cpp` | `TruthGramDatabase`: SQLCipher wrapper, `PRAGMA key` hex, `tg_kv` + per-plugin `plugin_<id>` tables, `Transaction` RAII |

### UI shared (2)
| File | Purpose |
| --- | --- |
| `ui/floating_action_bar.h` / `.cpp` | `FloatingActionBar`: rounded pill, drop shadow, `SpringAnimation` (damped spring integrator), animateIn/Out from parent bottom |
| `ui/widget_slot_registry.h` | Header-only `WidgetSlotRegistry`: binds `WidgetSlot`→container, plugin `inject_widget` reparents |

### Voice changer (2)
| File | Purpose |
| --- | --- |
| `voice/voice_changer.h` / `.cpp` | `VoiceChanger`: real-time DSP hook (10ms/480-sample frames). Stages: input gain → 5-band biquad EQ → formant warp → PSOLA-lite pitch → vocoder → radio bandpass+softclip → Schroeder reverb. 8 presets + loopback |

### Ghost Mode (2)
| File | Purpose |
| --- | --- |
| `ghost/ghost_controls.h` / `.cpp` | `GhostController`: suppress `readHistory`/`readContents`/`setTyping`/presence; one-shot read allowance per peer. `FloatingGhostBar`: Lime "Mark as Read" + Ice "Keep Unread / Ghost" pill |

### Phishing Guard (2)
| File | Purpose |
| --- | --- |
| `phishing/phishing_guard.h` / `.cpp` | `PhishingGuard`: IDN/Punycode homograph, Cyrillic→Latin confusable fold (16-char table), Levenshtein lookalike-Telegram check, shortener HEAD unshorten (5-hop, proxied), VirusTotal/URLhaus opt-in |

### WebApp Inspector (2)
| File | Purpose |
| --- | --- |
| `webapp/webapp_inspector.h` / `.cpp` | `WebAppInspector`: `QSslSocket` cert probe (issuer/subject/expiry), `QHostInfo` IP resolve, holds `initData` until user approves; per-bot approval gate |

### OSINT Runner (2)
| File | Purpose |
| --- | --- |
| `osint/osint_runner.h` / `.cpp` | `OsintRunner`: Telegram DC table (5 DCs), user-id→creation-epoch estimate, 15-platform Sherlock catalog, `QNetworkAccessManager`+proxy availability checks, concurrency cap |

### Screen Shield (2)
| File | Purpose |
| --- | --- |
| `screen/screen_shield.h` / `.cpp` | `ScreenShield`: Windows `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`, macOS `NSWindow.sharingType=NSWindowSharingNone`, Linux BlankOnCapture fallback; Streamer Mode toggle |

### Chess (2)
| File | Purpose |
| --- | --- |
| `chess/tg_chess.h` / `.cpp` | `TgChess`: 8x8 board, pseudo-legal move gen (sliding/knight/king/pawn+promo), visible `TGchess:` + zero-width steganographic wire format, FEN serialize/parse |

### BotFather Studio (2)
| File | Purpose |
| --- | --- |
| `botstudio/botfather_studio.h` / `.cpp` | `BotFatherStudio`: prompt→spec heuristic (weather/reminder/poll/todo/news), `/newbot`+`/setcommands`+`/setmenubutton`+`/setdescription` emitter, python/node/json webhook templates |

### Termux Bridge (2)
| File | Purpose |
| --- | --- |
| `termux/termux_bridge.h` / `.cpp` | `TermuxBridge`: loopback `QWebSocketServer` (port 7354), 32-byte shared secret (SQLCipher), approved-hook catalog, `{json}` substitution, `QProcess` execution |

## Plugin SDK example (3)
| File | Purpose |
| --- | --- |
| `plugins/hello_plugin/hello_plugin.cpp` | ~120-line example: registers 1 Setting, subscribes to ChatOpened, injects blinking Lime pill into ChatHeaderLeft; exports all 4 C entry points |
| `plugins/hello_plugin/manifest.json` | Sample manifest: `api_version:1`, `type:native`, `ui_inject:true` |
| `plugins/hello_plugin/CMakeLists.txt` | Standalone plugin build: Qt6 Widgets + SDK header include; cross-platform `.so`/`.dll`/`.dylib` suffix; install into `~/.local/share/TruthGram/plugins/hello_plugin/` |
