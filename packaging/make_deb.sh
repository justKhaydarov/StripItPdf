#!/usr/bin/env bash
# Build a Debian package (.deb) for StripItPdf from an already-built binary.
#   make -C app          # produce app/build/stripitpdf first
#   packaging/make_deb.sh # -> packaging/dist/stripitpdf_<ver>_amd64.deb
set -euo pipefail
cd "$(dirname "$0")/.."

VER="${1:-1.0.0}"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
BIN="${PHX_BIN:-app/build/stripitpdf}"
[ -x "$BIN" ] || { echo "build the app first:  make -C app"; exit 1; }

ROOT="packaging/build/deb"
rm -rf "$ROOT"
install -Dm755 "$BIN"                      "$ROOT/usr/bin/stripitpdf"
install -Dm644 app/resources/logo_256.png  "$ROOT/usr/share/icons/hicolor/256x256/apps/stripitpdf.png"
install -Dm644 app/README.md               "$ROOT/usr/share/doc/stripitpdf/README.md"

# desktop entry
install -Dm644 /dev/stdin "$ROOT/usr/share/applications/stripitpdf.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=StripItPdf
GenericName=PDF Layer Editor
Comment=Remove watermark layers from PDFs, keep the content
Exec=stripitpdf %f
Icon=stripitpdf
Terminal=false
Categories=Graphics;
MimeType=application/pdf;
EOF

# control — Qt6 pulls its own transitive deps (icu/xcb/…); list the leaves.
SIZE=$(du -ks "$ROOT/usr" | cut -f1)
install -Dm644 /dev/stdin "$ROOT/DEBIAN/control" <<EOF
Package: stripitpdf
Version: $VER
Section: graphics
Priority: optional
Architecture: $ARCH
Installed-Size: $SIZE
Maintainer: StripItPdf <noujackec@gmail.com>
Depends: libc6, libstdc++6, libgcc-s1,
 libqt6widgets6, libqt6gui6, libqt6core6, libqt6dbus6, qt6-qpa-plugins,
 libfreetype6, libharfbuzz0b, libjbig2dec0, libopenjp2-7, zlib1g,
 libjpeg-turbo8 | libjpeg62-turbo | libjpeg8
Description: PDF watermark / layer editor
 StripItPdf loads a PDF and removes the "useless" layers stacked on top of the
 page image — diagonal watermarks, tiled stamps, logos, optional-content groups
 and full-page transparent watermark sheets — while keeping the content image
 and the real, selectable text. Pick what to delete in a clean Qt interface.
EOF

mkdir -p packaging/dist
OUT="packaging/dist/stripitpdf_${VER}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$ROOT" "$OUT"
echo ">> built $OUT"
dpkg-deb --info "$OUT" | sed 's/^/   /'
