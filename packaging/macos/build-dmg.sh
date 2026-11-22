#!/usr/bin/env bash
# =============================================================================
# TruthGram macOS Universal2 .dmg builder
# -----------------------------------------------------------------------------
#   ./build-dmg.sh --universal          # arm64 + x86_64 -> TruthGram-x.y.z.dmg
#   ./build-dmg.sh --universal --sign   # sign + notarize (needs APPLE_* env)
# =============================================================================
set -euo pipefail

ARCHS="arm64"
SIGN=0
TDESKTOP_DIR="${TDESKTOP_DIR:-$(pwd)/../../tdesktop}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --universal) ARCHS="arm64 x86_64"; shift;;
        --arm64)     ARCHS="arm64";       shift;;
        --x86_64)    ARCHS="x86_64";      shift;;
        --sign)      SIGN=1;              shift;;
        *) echo "unknown arg: $1" >&2; exit 2;;
    esac
done

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="$(grep -m1 'project(TruthGram' "$REPO_ROOT/CMakeLists.txt" \
    | grep -oE 'VERSION [0-9.]+' | awk '{print $2}')"
OUT_DIR="$(pwd)/../dist"
STAGING="$OUT_DIR/staging"
APP_BUNDLE="TruthGram.app"
mkdir -p "$OUT_DIR" "$STAGING"

build_one() {
    local arch="$1"
    local bdir="$REPO_ROOT/build/macos-$arch"
    echo "==> CMake configure ($arch)"
    cmake -S "$REPO_ROOT" -B "$bdir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="$arch" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
        -DTRUTHGRAM_TDESKTOP_DIR="$TDESKTOP_DIR"
    echo "==> CMake build ($arch)"
    cmake --build "$bdir" --target truthgram_app
    echo "$bdir/bin/TruthGram"
}

# Build each slice.
BUILT=()
for a in $ARCHS; do BUILT+=( "$(build_one "$a")" ); done

# Assemble .app
APP_ROOT="$STAGING/$APP_BUNDLE"
rm -rf "$APP_ROOT"
mkdir -p "$APP_ROOT/Contents/MacOS" "$APP_ROOT/Contents/Resources"

if [[ "${#BUILT[@]}" -gt 1 ]]; then
    echo "==> lipo -create -> universal binary"
    lipo -create -output "$APP_ROOT/Contents/MacOS/TruthGram" "${BUILT[@]}"
else
    cp "${BUILT[0]}" "$APP_ROOT/Contents/MacOS/TruthGram"
fi
chmod +x "$APP_ROOT/Contents/MacOS/TruthGram"

cp "$REPO_ROOT/packaging/macos/Info.plist" "$APP_ROOT/Contents/Info.plist"
cp "$REPO_ROOT/resources/themes/cyber-truth.tdesktop-theme" \
   "$APP_ROOT/Contents/Resources/cyber-truth.tdesktop-theme"
cp "$REPO_ROOT/LICENSE" "$APP_ROOT/Contents/Resources/LICENSE"

# macdeployqt to bundle Qt frameworks and fix @rpaths.
macdeployqt "$APP_ROOT" -always-overwrite

# Sign + notarize.
if [[ $SIGN -eq 1 ]]; then
    if [[ -z "${APPLE_DEVELOPER_ID:-}" || -z "${APPLE_ID:-}" || -z "${APPLE_APP_SPECIFIC_PASSWORD:-}" || -z "${APPLE_TEAM_ID:-}" ]]; then
        echo "APPLE_DEVELOPER_ID/APPLE_ID/APPLE_APP_SPECIFIC_PASSWORD/APPLE_TEAM_ID must be set for --sign" >&2
        exit 2
    fi
    echo "==> codesign"
    /usr/bin/codesign --force --deep --options runtime \
        --entitlements "$REPO_ROOT/packaging/macos/TruthGram.entitlements" \
        --sign "$APPLE_DEVELOPER_ID" "$APP_ROOT"

    echo "==> notarize"
    DMG_UNSIGNED="$OUT_DIR/TruthGram-$VERSION-unsigned.dmg"
    hdiutil create -volname TruthGram -srcfolder "$APP_ROOT" \
        -ov -format UDZO "$DMG_UNSIGNED"
    xcrun notarytool submit "$DMG_UNSIGNED" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_APP_SPECIFIC_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" --wait
    xcrun stapler staple "$DMG_UNSIGNED"
fi

# Build the final .dmg via create-dmg if available, else hdiutil.
DMG="$OUT_DIR/TruthGram-$VERSION.dmg"
rm -f "$DMG"
if command -v create-dmg >/dev/null 2>&1; then
    create-dmg --volname "TruthGram" \
        --window-size 600 400 --icon-size 96 \
        --app-drop-link 425 200 \
        --no-internet-enable "$DMG" "$STAGING"
else
    hdiutil create -volname TruthGram -srcfolder "$STAGING" \
        -ov -format UDZO "$DMG"
fi
echo "==> $DMG"
