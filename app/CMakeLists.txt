# StripItPdf — standard build for machines with system Qt6 + MuPDF dev packages.
#
#   Linux:   sudo apt install qt6-base-dev libmupdf-dev cmake g++
#   Windows: install Qt6 (MSVC), vcpkg install mupdf, then configure with the
#            Qt6_DIR / CMAKE_TOOLCHAIN_FILE pointing at them.
#
#   cmake -S app -B build && cmake --build build -j
#
# If you have no root (cannot install -dev packages), use the bundled-SDK path
# instead:  scripts/bootstrap_sdk.sh  &&  make -C app   (see app/Makefile).

cmake_minimum_required(VERSION 3.16)
project(StripItPdf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)          # compile the .qrc automatically

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---- locate MuPDF ----------------------------------------------------------
find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h)
find_library(MUPDF_LIB mupdf)
find_library(MUPDF_THIRD_LIB mupdf-third)
if(NOT MUPDF_INCLUDE_DIR OR NOT MUPDF_LIB)
    message(FATAL_ERROR "MuPDF not found. Install libmupdf-dev (Linux) or set MUPDF_* vars.")
endif()

# WIN32 -> no console window on Windows; MACOSX_BUNDLE -> a .app on macOS.
add_executable(stripitpdf WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.cpp
    src/PageView.cpp
    src/PdfCore.cpp
    resources/resources.qrc
)
target_include_directories(stripitpdf PRIVATE ${MUPDF_INCLUDE_DIR})
target_link_libraries(stripitpdf PRIVATE Qt6::Widgets ${MUPDF_LIB})

# MuPDF's third-party dependencies (Debian unbundles them; on other distros
# libmupdf-third.a may satisfy most of these — link whatever exists).
if(MUPDF_THIRD_LIB)
    target_link_libraries(stripitpdf PRIVATE ${MUPDF_THIRD_LIB})
endif()
# Link whichever of MuPDF's (possibly unbundled) third-party deps are present.
foreach(dep freetype harfbuzz jbig2dec openjp2 openjpeg jpeg turbojpeg
            mujs gumbo tesseract leptonica zlib z m)
    find_library(LIB_${dep} ${dep})
    if(LIB_${dep})
        target_link_libraries(stripitpdf PRIVATE ${LIB_${dep}})
    endif()
endforeach()

install(TARGETS stripitpdf RUNTIME DESTINATION bin)
