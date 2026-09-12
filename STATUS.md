# STATUS - requirement-by-requirement, as of this delivery

Legend: **Source-complete** = real, non-placeholder, self-consistent code
exists and is wired into the one build target (`nyra`). **Compiled** =
actually built and confirmed to produce `nyra.exe` (only true if you or CI
have run `BUILD.md` - see the bottom of this table). **Scaffolded** = a
real starting point that needs extension. **Not started** = genuinely
absent.

| # | Requirement | Status | Where |
|---|---|---|---|
| 1 | Build produces `nyra.exe`, not `vlc.exe` | Source-complete, **not yet compiled by anyone** | `src/app/CMakeLists.txt` |
| 2 | Real playback via public libvlc API | Source-complete | `src/app/MainWindow.cpp` |
| 3 | Modern Windows UI | Source-complete (Qt6 Widgets) | `src/app/` |
| 4 | Playback Mode UI (4 modes) | Source-complete, using only real libvlc options (see AUDIT.md #3) | `MainWindow.cpp` + `PlaybackModeManagerBridge.cpp` |
| 5 | Hardware acceleration detection | Source-complete, cache bug fixed (AUDIT.md #4) | `src/app/HwCaps.c` |
| 6 | Efficient hardware decoding | Reused from VLC unmodified (correct call - not reimplemented) | VLC's own `d3d11va` module, selected via `:avcodec-hw=any` |
| 7 | Power-aware playback | Source-complete, moved out of a disconnected VLC-plugin design (AUDIT.md #1) | `src/app/PowerManager.cpp` |
| 8 | Playback statistics | Source-complete | `MainWindow::onRefreshStatsOverlay()` |
| 9 | Playlist | Source-complete (basic, in-memory) | `QListWidget` dock; no save/load file yet |
| 10 | Subtitles | Source-complete (track selection) | no delay/sync controls yet |
| 11 | Audio controls | Source-complete (volume, track select) | no equalizer yet |
| 12 | Fullscreen | Source-complete | `onToggleFullscreen()` |
| 13 | Drag & drop | Source-complete | `dragEnterEvent`/`dropEvent` |
| 14 | A-B repeat | Source-complete (was a no-op stub - AUDIT.md #7) | `onSetAbRepeatPoint()` |
| 15 | Screenshot | Source-complete (filename bug fixed - AUDIT.md #5) | `onScreenshot()` |
| 16 | Crash-safe behavior | Source-complete, actually wired up now (AUDIT.md #6) | `CrashHandler.c` + `MainWindow.cpp` |
| 17 | File association | Source-complete, correct exe name (AUDIT.md #9) | `installer/` |
| 18 | Settings dialog | Not started | mode combo exists; no full dialog |
| 19 | CI that builds the real app | Source-complete, **not yet executed** (AUDIT.md #8) | `.github/workflows/build-windows.yml` |
| 20 | Windows x64 Release build, actually compiled and launched | **Not done.** Requires a Windows machine or a GitHub Actions run - neither is available in the Linux sandbox this fix-up pass ran in. No network egress either, so even downloading Qt6/the VLC SDK to attempt a cross-compile here was not possible. | - |

## What "not yet compiled" actually means, concretely

Whoever/whatever produced this revision of the repository:
- Read every file in the original ZIP (21 files, ~1,800 lines).
- Found and fixed 9 distinct, real bugs/architecture problems (see
  `AUDIT.md`) by reasoning about the code and cross-checking libvlc/VLC's
  actual public API surface from training knowledge - **not** by running a
  compiler against it, because no compiler for this target exists in the
  authoring environment.
- Did **not** run `cmake`, `qmake`, `mingw32-make`, `windeployqt`,
  `makensis`, or `nyra.exe` itself. Any claim that this project "compiles
  cleanly" or "runs correctly" would be unverified and should not be
  trusted until you or CI actually do one of those things.

## The actual next step

Push this repository to GitHub and either let
`.github/workflows/build-windows.yml` run on push, or trigger it manually
from the Actions tab. Read its log. If it fails, the failure will point at
a specific step (Qt install, VLC SDK download/layout, CMake configure,
compile, link, packaging, or the launch smoke test) - fix that step and
re-run, the same iterate-until-green loop the original brief asked for,
just executed on a machine that can actually run a Windows compiler.

## Honest gap list

- No automated GUI test harness (e.g. WinAppDriver) - CI's smoke test only
  proves `nyra.exe --version` launches and exits 0, not that pixels are
  correct or that a specific video file plays back correctly frame-by-frame.
- No real benchmark numbers exist anywhere in this repo.
  `benchmarks/benchmark.ps1` is functional but has not been run. **Do not
  treat any number anywhere in this repo as measured. None are, by
  design, until a real run on real hardware produces them.**
- Settings dialog, playlist save/load, subtitle delay/sync, audio
  equalizer, crop/aspect-ratio controls: not implemented.
- VLC's own `libvlc_video_get_spu_description` /
  `libvlc_audio_get_track_description` signatures have shifted slightly
  across VLC 3.x vs the still-evolving VLC 4.x track API. `MainWindow.cpp`
  is written against the 3.x signatures (matching the SDK `BUILD.md`
  Option A currently pins); if you build against a VLC 4.x SDK instead,
  diff these two calls against that SDK's actual header first.
