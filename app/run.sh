#!/usr/bin/env bash
# Build (if needed) and launch StripItPdf.
#   ./app/run.sh            # open the editor
#   ./app/run.sh 444.pdf    # open with a file loaded
set -euo pipefail
cd "$(dirname "$0")"

# Bootstrap the no-root SDK on first run if system Qt6 dev isn't available.
if [ ! -d ../.sdk/root ] && ! pkg-config --exists Qt6Widgets 2>/dev/null; then
    echo ">> No Qt6 dev found; bootstrapping a local SDK (no sudo needed)…"
    ../scripts/bootstrap_sdk.sh
fi

make >/dev/null
SYS=/usr/lib/x86_64-linux-gnu
LD_LIBRARY_PATH=$SYS QT_PLUGIN_PATH=$SYS/qt6/plugins exec ./build/stripitpdf "$@"
