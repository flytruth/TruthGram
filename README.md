# TruthGram

**A privacy-first, zero-telemetry, fully open-source Telegram Desktop client.**
Fork of [`telegramdesktop/tdesktop`](https://github.com/telegramdesktop/tdesktop).
GPL-3.0.

TruthGram adds a curated set of privacy, UX, and power-user modules on top of
upstream tdesktop **without** introducing any external analytics, tracking, or
proprietary blobs. All sync state travels through your own Telegram Saved
Messages, encrypted with AES-256-GCM keyed off your MTProto auth key.


## Highlights

- **Cyber-Truth theme** — Deep Obsidian Black background, Electric Lime Green
  accents, Ice Light Blue links. 100% hot-swappable with `.tdesktop-theme`.
- **iOS-inspired desktop UI** — frosted-glass sidebar, spring-physics scroll,
  rounded bubbles, floating navigation pills, swipe-to-reply gestures.
- **Detachable Mini Media Player** — floating pill with real-time FFT
  waveform (Lime/Blue), 0.5×–2.5× speed scrubbing, pitch lock, queue.
- **Plugin Engine** — native `.so`/`.dll`/`.dylib` + sandboxed Lua/Python,
  hot-loaded from `~/.local/share/TruthGram/plugins/`.
- **Voice Changer** — hooks `tg_owt` / `Media::Capture` pre-Opus: Deep,
  Pitch Shift, Vocoder, Radio, Reverb, Custom EQ, with local Test Mic loopback.
- **Ghost Mode** — floating `[✓ Mark as Read]` / `[🕶 Keep Unread / Ghost]`
  bar; suppresses `messages.readHistory`, `readMessageContents`, `setTyping`,
  and presence.
- **Screen Shield** — `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` on
  Windows, `NSWindowSharingNone` on macOS, Wayland/X11 hints on Linux, plus a
  Streamer Mode that blurs names/phones/avatars.
- **OSINT Inspector** — raw MTProto metadata (User ID, DC ID, account-age
  estimate) and a local Sherlock-style username recon runner over Tor/SOCKS5.
- **WebApp Auditor** — Mini Apps show their FQDN, TLS issuer, and IP/ASN before
  any `initData` token is released.
- **Link Guardian** — IDN/Punycode homograph detection, URL unshortening over
  proxy, lookalike Telegram domain checks, optional local VirusTotal/URLhaus.
- **BotFather Studio** — visual builder that emits `/newbot`, `/setcommands`,
  `/setmenubutton`, `/setdescription` and ready-to-copy webhook templates.
- **P2P Chess** — serverless board serialized as algebraic strings inside chat.
- **Termux Bridge** — encrypted local WebSocket / Unix Domain Socket IPC to
  Termux on Android or local shell automation.

## Repository layout

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full blueprint. Source lives in
`Telegram/SourceFiles/truthgram/`. Packaging is in `packaging/`. Example plugin
in `plugins/hello_plugin/`.

## Build

See [docs/BUILD.md](docs/BUILD.md) for Windows (MSVC/Ninja), Debian/Ubuntu
(`.deb` + AppImage), Arch (`PKGBUILD`), and macOS (universal `.dmg`).

Quick start (Linux, Ninja):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTDESKTOP_USE_PACKAGED=ON
cmake --build build --target truthgram_app
```

## Plugin SDK

Write your own plugin in ~60 lines. See
[docs/PLUGIN_API.md](docs/PLUGIN_API.md) and the example in
[`plugins/hello_plugin`](plugins/hello_plugin).

## License

GPL-3.0. See [LICENSE](LICENSE). The canonical full text is at
<https://www.gnu.org/licenses/gpl-3.0.txt>. Upstream tdesktop retains its own
license; TruthGram's additions are GPL-3.0.

## Disclaimer

TruthGram is not affiliated with Telegram FZ-LLC or Telegram Messenger Inc.
OSINT modules check only **public** username availability and must be used in
compliance with the terms of each platform you query. The voice changer is
intended for privacy, accessibility, and creative use; do not use it to
impersonate or defraud.
