# Building Nyra Player for Windows x64

## What changed vs the previous BUILD.md

The previous approach rebuilt VLC itself from source via autotools/WSL2 and
tried to inject Nyra's modules into VLC's own build (`modules/control/`).
That has been dropped - see `AUDIT.md` Finding #1 for the full reasoning.
**Nyra no longer touches VLC's source tree at all.** It builds against
VLC's own official, prebuilt Windows SDK (public headers + import libs +
plugins), which VideoLAN already publishes for exactly this purpose. This
is simpler, faster (minutes, not 1-2 hours), and does not depend on VLC's
internal build system staying compatible with a hand-maintained Makefile
fragment.

## Option A (recommended): official VLC Windows SDK, no VLC compile at all

1. Download a VLC Windows 64-bit build from https://get.videolan.org/vlc/
   (e.g. `vlc-3.0.20-win64.7z`, or a newer 3.x/4.x release once you've
   checked its SDK layout matches). Extract it. Under the extracted
   folder you need:
   - `sdk/include/vlc/vlc.h` (and the rest of `sdk/include/vlc/`)
   - `sdk/lib/libvlc.lib` (MSVC) or `libvlc.dll.a` (MinGW)
   - `plugins/` (the whole tree - required at runtime, not just build time)
   - `libvlc.dll`, `libvlccore.dll`, and VLC's own third-party DLLs (avcodec
     etc. if present) in the top-level extracted folder

   Arrange them as:
   ```
   vlc-sdk/
     include/vlc/vlc.h
     lib/libvlc.lib (or libvlc.dll.a)
     libvlc.dll
     libvlccore.dll
     plugins/...
   ```
   (`.github/workflows/build-windows.yml` does this extraction/normalization
   step automatically - read it if your SDK zip's internal layout differs
   from what it expects, and adjust.)

2. Install Qt6 (Widgets module), e.g. via the Qt online installer, or
   `aqtinstall`, or `winget install --id=Qt...` - any Qt6 6.5+ with the
   `mingw` or `msvc` desktop kit matching your compiler.

3. Install a compiler: MinGW-w64 (matches `win64_mingw` Qt kit) or MSVC
   (matches `win64_msvc2019_64` Qt kit) + CMake 3.21+.

4. Configure and build:
   ```powershell
   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DVLC_SDK_DIR=C:\path\to\vlc-sdk
   cmake --build build --config Release -j
   ```

5. Package a runnable folder:
   ```powershell
   cmake --install build --config Release --prefix dist\Nyra
   ```
   This copies `nyra.exe`, runs `windeployqt` to add the Qt runtime DLLs,
   and copies `libvlc.dll` + `plugins/` next to it. Verify with:
   ```powershell
   cmake -DNYRA_DIST_DIR=dist\Nyra -P tests\check_package_contents.cmake
   ```

6. Run it: `dist\Nyra\nyra.exe`.

## Option B: build VLC from source yourself first

If you specifically need a VLC revision with no official Windows SDK zip
(e.g. testing against `master`), build VLC per VLC's own current
`doc/BUILD-win32.md` (via WSL2/autotools, ~1-2 hours including contribs),
then point `-DVLC_SDK_DIR` at that build's output
(`<build>/win64/vlc-<ver>/sdk` typically has the same `include/`/`lib/`
layout Option A expects). Nyra's CMake does not care whether the SDK came
from a download or a from-source build - only the resulting folder layout
matters.

## CI (the actual, verified path to a real .exe)

Push this repository to GitHub. `.github/workflows/build-windows.yml` runs
Option A end-to-end on a hosted Windows runner: installs Qt6, downloads the
VLC SDK, configures/builds/installs with CMake, verifies `nyra.exe` and
every required DLL/plugins folder actually exist, launches `nyra.exe
--version` headlessly to confirm the process starts and exits cleanly,
packages `Nyra-Player-Windows-x64.zip`, and (best-effort) builds
`Nyra-Player-Setup-Windows-x64.exe` with NSIS. Download the artifacts from
the Actions run page.

This workflow has been written carefully but **has not been executed** by
whoever/whatever wrote this document - that requires an actual Windows
runner. Run it and read its log before trusting the artifact; if a step
fails, the log tells you exactly which one and why (deliberately no
`|| true` anywhere that would hide a real failure - see `AUDIT.md`).

## Verifying real media plays

Headless CI can prove the process launches; it cannot prove your specific
video card decodes 4K HEVC correctly or that subtitles render right. After
downloading a built package:
1. Run `nyra.exe`, open a local MP4/MKV file, confirm video+audio play,
   seeking works, and the mode dropdown doesn't crash anything.
2. Check the status bar - it should show your GPU name (from the HW-probe)
   within a second or two of startup.
3. Try each of the four playback modes on the same file and confirm
   playback keeps working in all four (this proves the per-mode options in
   `PlaybackModeManagerBridge.cpp` don't break `libvlc_media_player_play`).

Neither of us (you reading this, and whoever last edited this repo) has
skipped this step and called it "verified" - do not do that either.
