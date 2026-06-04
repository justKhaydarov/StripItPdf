# StripItPdf — Layer Editor

A clean, native **C++/Qt6** desktop app that loads a PDF, shows what each page is
built from, and **deletes the useless layers stacked on top of the main image**
— diagonal watermarks, tiled stamps, centred logos, and optional-content
("watermark") layers — then saves a clean copy. Unlike a flatten/rasterise, it
removes the overlays *in place* and **keeps the real, selectable text**.

It is styled with a clean, vibrant-blue design system (`#2563EB`, Outfit +
Plus Jakarta Sans, soft rounding and shadows).

![editor](docs/screenshot.png)

## What it does

- **Loads & renders** every page (MuPDF) with a thumbnail navigator.
- **Detects layers** per page and classifies them:
  - the **biggest image** = the real content → **kept** (green outline).
  - everything stacked on top → **overlay** → **deleted** (red outline):
    small/tiled images, logos, optional-content groups, and **diagonal/rotated
    watermark text** (kept text that is horizontal).
- **Lets you choose** — tick/untick any element; *Apply to every page* mirrors
  your choice across a uniform document; *Reset to auto-detect* restores the
  smart defaults.
- **Preview** the cleaned result live with the Original / Cleaned toggle.
- **Saves** a stripped PDF (`*_cleaned.pdf`).

## How the stripping works

Each page is a large content image with junk painted over it. The engine
(`src/PdfCore.cpp`, MuPDF `pdf_filter_page_contents`) runs three filters:

| Overlay kind | How it is removed |
|---|---|
| Tiled / small / logo **images** | `image_filter` drops any image whose page coverage is below the keep threshold (or the covers you selected) |
| **Full-page transparent watermark** images (stripes, "DIGITAL X SAT", "Watermarkly", baked-in @handles) | a full-cover image that is *mostly transparent* (mean opacity below ~50%) is an overlay, not content — the opaque page scan is kept, the see-through watermark sheet is dropped |
| **Diagonal text** watermark | `text_filter` drops glyphs whose baseline is rotated more than ~8° (horizontal body text is kept) |
| **Optional-content** (OCG) layers | every form XObject marked `/OC` is emptied |

The biggest **opaque** image and the horizontal text survive untouched. Opacity
is what lets the editor tell an opaque content scan apart from a same-size
transparent watermark drawn on top of it — both show up at 100% page coverage,
so the inspector labels them "opaque" vs "7% opaque overlay".

## Build

### Linux — recommended (system packages)

```bash
sudo apt install qt6-base-dev libmupdf-dev cmake g++
cmake -S app -B build && cmake --build build -j
./build/stripitpdf            # or: ./build/stripitpdf 444.pdf
```

### Linux — no root (bundled SDK)

If you can't install `-dev` packages, fetch them locally with `apt-get download`
(no sudo) and build with the auto-detecting Makefile:

```bash
scripts/bootstrap_sdk.sh      # downloads Qt6 + MuPDF headers into ./.sdk
make -C app                   # -> app/build/stripitpdf
./app/run.sh 444.pdf          # builds if needed, then launches
```

### Windows

Cross-platform by construction (Qt6 + MuPDF, no platform code).

1. Install **Qt 6** (MSVC) and **CMake**.
2. Get MuPDF dev libs (e.g. `vcpkg install mupdf`).
3. Configure & build:
   ```bat
   cmake -S app -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ^
         -DCMAKE_PREFIX_PATH=<Qt>/6.x/msvc2022_64
   cmake --build build --config Release
   ```
   The fonts, icons, logo and stylesheet are compiled into the binary via Qt
   resources, so the `.exe` is self-contained (ship it next to the Qt runtime
   DLLs with `windeployqt`).

## Layout

```
app/
  src/PdfCore.{h,cpp}     MuPDF engine: render, analyse, strip   (no Qt — reusable)
  src/PageView.{h,cpp}    canvas widget: page + keep/delete highlights
  src/MainWindow.{h,cpp}  the app shell (sidebar / viewer / inspector)
  src/main.cpp            entry: loads fonts + stylesheet
  src/cli_strip.cpp       headless CLI driver for PdfCore
  resources/style.qss     app theme   ·  resources/*.ttf fonts  ·  logo/icons
  CMakeLists.txt          standard build   ·  Makefile  bundled-SDK build
```

There is also a standalone CLI: `build/cli_strip in.pdf out.pdf`.
