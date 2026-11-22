#!/usr/bin/env bash
# =============================================================================
# TruthGram Debian (.deb) packaging
# -----------------------------------------------------------------------------
#   ./build-deb.sh --release            # -> ../dist/truthgram_x.y.z_amd64.deb
#   ./build-deb.sh --release --arm64    # cross-build for arm64
# =============================================================================
set -euo pipefail

BUILD_TYPE=Release
ARCH=$(dpkg --print-architecture)
TDESKTOP_DIR="${TDESKTOP_DIR:-$(pwd)/../../tdesktop}"
OUT_DIR="$(pwd)/../dist"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release) BUILD_TYPE=Release; shift;;
        --debug)   BUILD_TYPE=Debug;   shift;;
        --arm64)   ARCH=arm64;         shift;;
        --amd64)   ARCH=amd64;         shift;;
        --out)     OUT_DIR="$2"; shift 2;;
        *) echo "unknown arg: $1" >&2; exit 2;;
    esac
done

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="$(grep -m1 'project(TruthGram' "$REPO_ROOT/CMakeLists.txt" \
    | grep -oP 'VERSION \K[0-9.]+')"
PKG="truthgram_${VERSION}_${ARCH}"

echo "==> Building TruthGram $VERSION ($ARCH) [$BUILD_TYPE]"
BUILD_DIR="$REPO_ROOT/build/deb-$ARCH"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="/usr" \
    -DTDESKTOP_USE_PACKAGED=ON \
    -DTRUTHGRAM_TDESKTOP_DIR="$TDESKTOP_DIR"
cmake --build "$BUILD_DIR" --target truthgram_app

# Stage the Debian package tree.
STAGE="$(pwd)/stage-$PKG"
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" \
         "$STAGE/usr/bin" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/icons/hicolor/512x512/apps" \
         "$STAGE/usr/share/truthgram/themes" \
         "$STAGE/usr/share/doc/truthgram"

cp "$BUILD_DIR/bin/TruthGram" "$STAGE/usr/bin/truthgram"
cp "$REPO_ROOT/resources/themes/cyber-truth.tdesktop-theme" \
   "$STAGE/usr/share/truthgram/themes/"
cp "$REPO_ROOT/LICENSE" "$STAGE/usr/share/doc/truthgram/copyright"
cp "$REPO_ROOT/README.md" "$STAGE/usr/share/doc/truthgram/README"

cat > "$STAGE/usr/share/applications/truthgram.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=TruthGram
Comment=Privacy-first Telegram Desktop client
Exec=truthgram -- %u
Icon=truthgram
Categories=Network;InstantMessaging;Chat;
MimeType=x-scheme-handler/tg;x-scheme-handler/ton;
Keywords=telegram;truthgram;chat;im;
StartupWMClass=TruthGram
EOF

# (Placeholder icon would be a real PNG in a full checkout.)
# cp "$REPO_ROOT/resources/icons/truthgram-512.png" "$STAGE/usr/share/icons/hicolor/512x512/apps/truthgram.png"

# control file
mkdir -p "$STAGE/DEBIAN"
awk -v v="$VERSION" -v a="$ARCH" '
    /^Version:/ {print "Version: " v; next}
    /^Architecture:/ {print "Architecture: " a; next}
    {print}
' "$REPO_ROOT/packaging/debian/control" > "$STAGE/DEBIAN/control"

cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
update-desktop-database -q /usr/share/applications || true
gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
EOF
chmod 0755 "$STAGE/DEBIAN/postinst"

cat > "$STAGE/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    rm -rf "$HOME/.local/share/TruthGram/plugins/.lock" 2>/dev/null || true
fi
EOF
chmod 0755 "$STAGE/DEBIAN/prerm"

mkdir -p "$OUT_DIR"
dpkg-deb --build --root-owner-group "$STAGE" "$OUT_DIR/$PKG.deb"
echo "==> $OUT_DIR/$PKG.deb"
rm -rf "$STAGE"
