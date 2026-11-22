# Building TruthGram

TruthGram extends `telegramdesktop/tdesktop` with a `tg_truthgram` static
library target that is linked into the main `Telegram` executable. The TruthGram
build is driven by the root `CMakeLists.txt`; it expects the tdesktop submodule
to be checked out at `tdesktop/` (or `third_party/tdesktop/`).

## Common prerequisites

- **CMake** ≥ 3.25
- **Ninja** (recommended) or Make
- **C++20** toolchain
- **Qt** 6.6+ (Modules: Core, Gui, Widgets, Network, Svg, Concurrent,
  Multimedia, OpenGL, OpenGLWidgets, Quick, QuickWidgets)
- **tg_owt** (with TruthGram voice-changer patch — see below)
- **lib_tl**, **lib_ui**, **lib_base** (from tdesktop `lib_*` subtrees)
- **OpenSSL** 3.x, **FFmpeg** ≥ 6, **libopus**, **rnnoise**
- **SQLCipher** 4.x (SQLite + AES-256 encryption)
- **Python** ≥ 3.10 (for codegen + packaging scripts)

## Windows (MSVC + Ninja)

```powershell
# From packaging/windows/
.\build.ps1 -BuildType Release -Portable      # produces TruthGram-x.y.z-portable.zip
.\build.ps1 -BuildType Release -Installer      # runs Inno Setup -> TruthGram-x.y.z-setup.exe
```

`build.ps1` calls `vswhere` to locate Visual Studio 2022, sets up the MSVC +
Windows SDK environment, runs `cmake --preset windows-msvc-ninja`, builds
`truthgram_app`, then either zips a portable tree or invokes ISCC on
`TruthGram.iss`.

Voice-changer note: the build patches `tg_owt` with
`patches/tg_owt/0001-truthgram-audio-hook.patch` which exposes the
`AudioProcessing::ProcessCaptureFrameEx` hook point used by `voice_changer.cpp`.

## Debian / Ubuntu (.deb + AppImage)

```bash
cd packaging/debian
./build-deb.sh --release        # -> ../dist/truthgram_x.y.z_amd64.deb
sudo dpkg -i ../dist/truthgram_*.deb
```

`build-deb.sh` installs build-deps from `control`, runs the CMake build with
`-DTDESKTOP_USE_PACKAGED=ON`, then assembles the `.deb` using
`dpkg-deb --build` against a staged `DEBIAN/` tree.

AppImage:

```bash
cd packaging/appimage
sudo ./build-appimage.sh        # uses linuxdeploy + AppImageBuilder.yml
# -> TruthGram-x.y.z-x86_64.AppImage
```

## Arch (AUR PKGBUILD)

```bash
cd packaging/arch
makepkg -si                     # builds truthgram-git from AUR layout
```

The `PKGBUILD` clones tdesktop at a pinned tag, applies TruthGram as an overlay
(symlinks `Telegram/SourceFiles/truthgram/` into the upstream tree), runs the
CMake build against system Qt6 / tg_owt / OpenSSL, and packages the desktop
file, hicolor icons, and the bundled Cyber-Truth theme.

## macOS (Universal2 .dmg)

```bash
cd packaging/macos
./build-dmg.sh --universal      # arm64 + x86_64 lipo'd binary, create-dmg -> TruthGram-x.y.z.dmg
```

`build-dmg.sh` runs two CMake builds (`-DCMAKE_OSX_ARCHITECTURES=arm64` and
`=x86_64`), `lipo`-creates a fat binary, signs and notarizes if
`APPLE_DEVELOPER_ID` is set, then builds the `.dmg` with `create-dmg`. The
Info.plist declares `NSAppTransportSecurity` to force MTProto endpoints only.

## CMake presets

TruthGram ships `CMakePresets.json` with `configure`/`build` presets:

| Preset | Toolchain | Notes |
| --- | --- | --- |
| `windows-msvc-ninja` | MSVC 2022 | `/permissive- /Zc:__cxaApiExport`, ASAN optional |
| `linux-gcc-ninja` | GCC 13+ | `-Wall -Wextra -Wpedantic`, LTO |
| `linux-clang-ninja` | Clang 17+ | libc++ optional |
| `macos-arm64-clang` | Apple Clang | arm64 |
| `macos-x86_64-clang` | Apple Clang | x86_64 |

## Disabling TruthGram modules at compile time

Every TruthGram module is gated by a CMake option so distributors can ship a
slimmed binary:

```bash
cmake -S . -B build \
  -DTRUTHGRAM_ENABLE_OSINT=OFF \
  -DTRUTHGRAM_ENABLE_VOICE_CHANGER=OFF \
  -DTRUTHGRAM_ENABLE_CHESS=OFF
```

See `truthgram_config.h` for the macro that each option maps to. When a module
is compiled out, its UI entry points are hidden and the corresponding
`tg::truthgram::Init*` call is a no-op.
