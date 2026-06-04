#!/usr/bin/env bash
# Build a portable Linux tarball: the binary + a launcher + icon + README.
# Runs on any Linux that has the Qt6 runtime installed (no root / no install).
#   packaging/make_tarball.sh  -> packaging/dist/stripitpdf-<ver>-linux-x86_64.tar.gz
set -euo pipefail
cd "$(dirname "$0")/.."

VER="${1:-1.0.0}"
BIN="${PHX_BIN:-app/build/stripitpdf}"
[ -x "$BIN" ] || { echo "build the app first:  make -C app"; exit 1; }

DIR="packaging/build/stripitpdf-$VER-linux-x86_64"
rm -rf "$DIR"; mkdir -p "$DIR"
cp "$BIN" "$DIR/stripitpdf"
cp app/resources/logo_256.png "$DIR/stripitpdf.png"
cp app/README.md "$DIR/README.md"

cat > "$DIR/run.sh" <<'EOF'
#!/usr/bin/env bash
# Launch StripItPdf. Needs the Qt6 runtime (libqt6widgets6 + qt6-qpa-plugins).
cd "$(dirname "$0")"
if ! ldd ./stripitpdf 2>/dev/null | grep -q libQt6Widgets; then :; fi
exec ./stripitpdf "$@"
EOF
chmod +x "$DIR/run.sh" "$DIR/stripitpdf"

mkdir -p packaging/dist
OUT="packaging/dist/stripitpdf-$VER-linux-x86_64.tar.gz"
tar -C packaging/build -czf "$OUT" "stripitpdf-$VER-linux-x86_64"
echo ">> built $OUT"
tar -tzf "$OUT" | sed 's/^/   /'
