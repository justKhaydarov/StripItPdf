# Dev helper: env + flags for the bundled-SDK build (source from repo root).
#   source scripts/devflags.sh
SDK="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.sdk" && pwd)"
SYS=/usr/lib/x86_64-linux-gnu
QT_INC="$SDK/root/usr/include/x86_64-linux-gnu/qt6"
QT_CXX="-I$QT_INC -I$QT_INC/QtCore -I$QT_INC/QtGui -I$QT_INC/QtWidgets"
QT_LINK="-L$SDK/linklib -lQt6Widgets -lQt6Gui -lQt6Core -Wl,-rpath,$SYS"
MUPDF_CXX="-I$SDK/root/usr/include"
MUPDF_LINK="-Wl,--start-group \
  $SDK/root/usr/lib/libmupdf.a $SDK/root/usr/lib/libmupdf-third.a \
  $SDK/root/usr/lib/x86_64-linux-gnu/libmujs.a $SDK/root/usr/lib/x86_64-linux-gnu/libgumbo.a \
  -Wl,--end-group -L$SYS \
  -l:libfreetype.so.6 -l:libharfbuzz.so.0 -l:libjbig2dec.so.0 -l:libopenjp2.so.2.5.0 -l:libjpeg.so.8 \
  -lz -lm"
RUN_ENV="LD_LIBRARY_PATH=$SYS QT_QPA_PLATFORM=offscreen QT_PLUGIN_PATH=$SYS/qt6/plugins"
