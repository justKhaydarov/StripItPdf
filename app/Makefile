# StripItPdf — Makefile.
#
# Auto-detects how to build:
#   * if ../.sdk/root exists  -> use the bundled no-root SDK (scripts/bootstrap_sdk.sh)
#   * else if pkg-config has Qt6Widgets -> use system Qt6 + system MuPDF
#
#   make            # build  -> build/stripitpdf
#   make run        # build and launch
#   make clean
#
# (CMake is the recommended path on a normal dev machine — see CMakeLists.txt.)

APP      := build/stripitpdf
SRC      := src/main.cpp src/MainWindow.cpp src/PageView.cpp src/PdfCore.cpp
QRC      := resources/resources.qrc
GEN_QRC  := build/qrc_resources.cpp
CXXFLAGS := -std=c++17 -O2 -fPIC
SYS      := /usr/lib/x86_64-linux-gnu

SDK := $(abspath ../.sdk)

ifneq ($(wildcard $(SDK)/root),)
  # ---- bundled no-root SDK -------------------------------------------------
  MODE     := bundled SDK ($(SDK))
  QT_INC   := $(SDK)/root/usr/include/x86_64-linux-gnu/qt6
  RCC      := $(SDK)/root/usr/lib/qt6/libexec/rcc
  INCS     := -I$(QT_INC) -I$(QT_INC)/QtCore -I$(QT_INC)/QtGui -I$(QT_INC)/QtWidgets \
              -I$(SDK)/root/usr/include
  LIBS     := -L$(SDK)/linklib -lQt6Widgets -lQt6Gui -lQt6Core -Wl,-rpath,$(SYS) \
              -Wl,--start-group \
                $(SDK)/root/usr/lib/libmupdf.a $(SDK)/root/usr/lib/libmupdf-third.a \
                $(SDK)/root/usr/lib/x86_64-linux-gnu/libmujs.a \
                $(SDK)/root/usr/lib/x86_64-linux-gnu/libgumbo.a \
              -Wl,--end-group -L$(SYS) \
              -l:libfreetype.so.6 -l:libharfbuzz.so.0 -l:libjbig2dec.so.0 \
              -l:libopenjp2.so.2.5.0 -l:libjpeg.so.8 -lz -lm
  RUNENV   := LD_LIBRARY_PATH=$(SYS) QT_PLUGIN_PATH=$(SYS)/qt6/plugins
else
  # ---- system packages -----------------------------------------------------
  MODE     := system Qt6 + system MuPDF
  RCC      := $(shell which rcc || echo /usr/lib/qt6/libexec/rcc)
  INCS     := $(shell pkg-config --cflags Qt6Widgets) -I/usr/include
  LIBS     := $(shell pkg-config --libs Qt6Widgets) \
              -lmupdf -lmupdf-third -lfreetype -lharfbuzz -ljbig2dec -lopenjp2 \
              -ljpeg -lmujs -lgumbo -lz -lm
  RUNENV   :=
endif

$(APP): $(SRC) src/*.h $(GEN_QRC) | build
	@echo ">> building ($(MODE))"
	g++ $(CXXFLAGS) $(SRC) $(GEN_QRC) -o $@ $(INCS) $(LIBS)

$(GEN_QRC): $(QRC) | build
	$(RCC) $(QRC) -o $@

build:
	mkdir -p build

run: $(APP)
	$(RUNENV) ./$(APP)

clean:
	rm -rf build

.PHONY: run clean
