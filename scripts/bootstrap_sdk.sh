#!/usr/bin/env bash
# Bootstrap a local Qt6 + MuPDF SDK WITHOUT root, by downloading the Debian/Ubuntu
# -dev packages with `apt-get download` (needs no sudo) and extracting them into
# ./.sdk.  Use this when you cannot `sudo apt install` the dev packages.
#
#   scripts/bootstrap_sdk.sh
#   make -C app                 # the Makefile auto-detects ./.sdk
#
# Requires: g++, make, apt-get, dpkg-deb  (all present on a stock Ubuntu).
set -euo pipefail
cd "$(dirname "$0")/.."
SDK=".sdk"
mkdir -p "$SDK/debs"

PKGS=(
  # Qt6 dev headers + tools (moc/rcc/uic) — matched to the installed runtime
  qt6-base-dev qt6-base-dev-tools
  libgl-dev libglx-dev libopengl-dev libglvnd-dev libpthread-stubs0-dev libvulkan-dev
  libx11-dev libxext-dev libxau-dev libxcb1-dev libxdmcp-dev
  x11proto-core-dev x11proto-dev xtrans-dev
  # MuPDF + its (Debian-unbundled) third-party deps
  libmupdf-dev libmujs-dev libgumbo-dev
  libfreetype-dev libharfbuzz-dev libharfbuzz0b
  libjbig2dec0-dev libopenjp2-7-dev
  libjpeg-turbo8-dev libjpeg-turbo8
  zlib1g-dev libbz2-dev libpng-dev libbrotli-dev
)

echo ">> downloading $(echo ${PKGS[@]} | wc -w) packages into $SDK/debs ..."
( cd "$SDK/debs"
  for p in "${PKGS[@]}"; do
    apt-get download "$p" >/dev/null 2>&1 && echo "   ok  $p" || echo "   --  $p (skipped)"
  done )

echo ">> extracting ..."
rm -rf "$SDK/root"; mkdir -p "$SDK/root"
for d in "$SDK"/debs/*.deb; do dpkg-deb -x "$d" "$SDK/root"; done

echo ">> creating dev symlinks for the installed Qt runtime ..."
SYS=/usr/lib/x86_64-linux-gnu
mkdir -p "$SDK/linklib"
for m in Core Gui Widgets DBus Network OpenGL OpenGLWidgets PrintSupport; do
  src=$(ls "$SYS/libQt6$m.so.6" 2>/dev/null | head -1)
  [ -n "$src" ] && ln -sf "$src" "$SDK/linklib/libQt6$m.so"
done

echo ">> done.  Now build with:  make -C app"
