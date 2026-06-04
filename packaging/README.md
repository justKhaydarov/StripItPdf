# Packaging & distribution

How to produce and ship StripItPdf for each desktop OS.

> **A Windows `.exe` and a macOS `.app` cannot be built from Linux** (no MinGW
> Qt/MuPDF, no macOS toolchain). They are produced on native runners by the
> GitHub Actions workflow in [`.github/workflows/build.yml`](../.github/workflows/build.yml)
> (push a `v*` tag, or run it manually), or built locally on each OS. Only the
> **Linux** artifacts in `dist/` were built and tested here.

## Artifacts

| Platform | Artifact | How it's made |
|---|---|---|
| Debian/Ubuntu | `stripitpdf_<ver>_amd64.deb` | `packaging/make_deb.sh` ✅ built here |
| Any Linux + Qt6 | `stripitpdf-<ver>-linux-x86_64.tar.gz` | `packaging/make_tarball.sh` ✅ built here |
| Windows | `stripitpdf.exe` + Qt DLLs (zip) | CI `windows` job (MSYS2 + windeployqt) |
| macOS | `stripitpdf.app` (zip/dmg) | CI `macos` job (brew + macdeployqt) |

## Install / run

**Debian/Ubuntu** — the native variant:
```bash
sudo apt install ./stripitpdf_1.0.0_amd64.deb     # pulls Qt6 deps automatically
stripitpdf                                         # or launch "StripItPdf" from the menu
```
**Other Linux** — unpack and run (needs the Qt6 runtime installed):
```bash
tar xzf stripitpdf-1.0.0-linux-x86_64.tar.gz && ./stripitpdf-1.0.0-linux-x86_64/run.sh
```
**Windows** — unzip, double-click `stripitpdf.exe` (Qt DLLs sit beside it).
**macOS** — unzip, drag `stripitpdf.app` to Applications. Unsigned, so the first
launch is right-click → **Open** (or `xattr -dr com.apple.quarantine stripitpdf.app`).

## Building locally on each OS

```bash
# Linux  (Debian/Ubuntu)
sudo apt install cmake g++ qt6-base-dev libmupdf-dev
cmake -S app -B build && cmake --build build -j

# Windows (MSYS2 MINGW64 shell)
pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,qt6-base,mupdf}
cmake -S app -B build -G Ninja && cmake --build build -j
windeployqt6 build/stripitpdf.exe

# macOS
brew install qt@6 mupdf cmake
cmake -S app -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix mupdf)"
cmake --build build -j && macdeployqt build/stripitpdf.app
```

## Do you need an installer?

**No — not for any platform.** This is a single self-contained GUI tool, so a
plain package is enough:

- **Linux:** the **`.deb` is the "installer"** the Debian world expects — it
  registers the app, icon and PDF file-association and lets `apt` resolve the Qt
  dependencies. Nothing else is needed. (For distro-independent "just download
  and run", an **AppImage** is the alternative — also CI-buildable — and needs no
  install at all.)
- **Windows:** ship the **`windeployqt` folder as a `.zip`** — the user unzips and
  runs `stripitpdf.exe`. An installer (**Inno Setup** / **NSIS**) is *optional
  polish* if you want a Start-Menu shortcut + uninstaller; it isn't required for
  the app to work.
- **macOS:** ship the **`.app` in a `.zip` or `.dmg`** (drag-to-Applications) —
  that *is* the macOS convention; Mac apps don't use installers. The real
  follow-up there isn't an installer but **code-signing + notarization** (needs a
  paid Apple Developer ID) so Gatekeeper doesn't warn on first launch.

Bottom line: `.deb` for Debian/Ubuntu, a zipped `.app`/`.dmg` for macOS, and a
zipped exe folder for Windows cover everyone. Add an Inno Setup installer later
only if you want Windows Start-Menu integration.
