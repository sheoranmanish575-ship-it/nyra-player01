# Nyra Player - Audit Findings and Fixes

Full inspection of the uploaded ZIP (`nyra-player__1_.zip`, ~1,800 lines
across 21 files). Each finding below: what was wrong, why it mattered, and
what changed. This document is the answer to "what was wrong / what did
you change" from the original request.

## Finding #1 (architectural, most important): two incompatible object models

`src/modules/pmm/playback_mode_manager.c` and `src/modules/power/idle_manager.c`
were written as **internal VLC core "interface" plugins** - they used
`vlc_player_t`, `vlc_playlist_t`, `intf_thread_t`, `vlc_object_t`, and
`var_SetString()` on core objects. That is VLC's *internal* object model,
used by code compiled directly into `libvlccore`/a VLC front-end (like
`vlc.exe` itself), never by an external application.

Meanwhile `src/ui/nyra_shell/MainWindow.cpp` (correctly, per the brief's
own architecture diagram) uses the **public libvlc client API**:
`libvlc_new()`, `libvlc_media_player_new()`, `libvlc_media_new_path()`,
etc. A libvlc client app does not automatically get its player wired into
whatever internal `vlc_player_t`/`vlc_playlist_t` an "interface" module
attaches to - those are two separate object graphs. Concretely:
`idle_manager.c`'s `Open()` calls `vlc_intf_GetMainPlayer(intf)`, which
returns VLC's own core playlist's player, not the `libvlc_media_player_t`
that `MainWindow` created. Even if `automake_nyra_fragment.am` had
successfully built this file into a working VLC plugin, and even granting
the (real, but narrow) public `libvlc_add_intf()` bridge, its callbacks
would fire for a playback pipeline the Qt UI never drives. As written, the
whole idle/power and playback-mode-manager subsystem could not have
affected anything the user sees in Nyra's own window.

Also: `idle_manager.c` calls `vlc_player_GetHWND(player)` - its own
comment admits this is an "illustrative accessor name," i.e. it is not a
real function in VLC's headers. This file would not have compiled as
written, independent of the architecture problem above.

**Fix:** Nyra no longer forks or patches VLC's source tree at all. VLC is
used as an unmodified, official prebuilt SDK (see `BUILD.md`). All
Nyra-specific logic - hardware-capability probing, playback-mode presets,
idle/power management, crash handling - now runs inside `nyra.exe` itself
and only calls the public libvlc API, real Win32 APIs, and Qt. This is a
smaller feature set than the original design implied (no core-level
variable injection into arbitrary decoder/vout objects) but everything in
it actually runs, in the actual process the user launches.

## Finding #2: wrong link target and library

`src/ui/nyra_shell/CMakeLists.txt` built a target named `NyraShell`
(-> `NyraShell.exe`, not the required `nyra.exe`) and linked
`Qt6::Widgets vlc vlccore` - `vlc` and `vlccore` are VLC's private internal
libraries, explicitly the wrong thing to link per the brief's own
architecture requirement ("do NOT link the Qt application directly against
VLC's private internal core libraries"). There was also no top-level
`CMakeLists.txt` at all, so this was the only build target in the whole
project, and none of the C modules were wired into any build.

**Fix:** New `src/app/CMakeLists.txt` builds a target literally named
`nyra`, links only the public `libvlc` import library, and compiles the
(now-repurposed) `HwCaps.c`/`CrashHandler.c` straight into the same
executable with the Win32 libraries they actually need (`d3d11`, `dxgi`,
`dxguid`, `dbghelp`, `winmm`) - none of which were linked anywhere in the
original project. Added a top-level `CMakeLists.txt` to tie it together.

## Finding #3: two fabricated libvlc options

`PlaybackModeManagerBridge.cpp` and `playback_mode_manager.c` both set
`:vout-scale-mode=...` and `:gpu-power-hint=...`. These are **not real VLC
options** - they do not exist in any VLC release. The original comment
admitted this outright: implementing them would require "a handful of
small, documented patches to the existing d3d11 vout and avcodec glue,"
which do not exist anywhere in the delivered ZIP. As written, three of the
four playback modes (Maximum Quality, Balanced, Battery Saver) would have
behaved identically at runtime for these two settings, silently - exactly
the "features described in documentation but not actually implemented"
failure mode the brief called out.

**Fix:** `PlaybackModeManagerBridge.cpp` now only emits options that are
real, existing, documented libvlc per-media options:
`:avcodec-hw`, `:video-filter=deinterlace` + `:deinterlace-mode`, and
`:avcodec-threads`. This is a smaller feature set - genuinely applying
fewer distinct levers per mode than originally claimed - but every option
now does something real and verifiable against `vlc/vlc.h` and VLC's
`deinterlace`/`avcodec` module documentation.

## Finding #4: cache that never actually cached anything

`hwcaps.c`'s header comment claimed the on-disk cache means the DXGI/D3D11
probe "is not re-probed on every launch." But `nyra_hwcaps_get()`
unconditionally called the **full** `nyra_hwcaps_probe()` (D3D11 device
creation + video-decoder-profile enumeration for six codecs) just to
compare the driver revision against the cache - meaning the "cheap" path
did 100% of the expensive work every time, plus extra file I/O. Separately,
`nyra_hwcaps_load_cache()` wrote six per-codec booleans to disk but never
parsed them back (the comment admitted this: "intentionally omitted...
for brevity"), so even a valid cache hit could never actually answer "does
this GPU support HW HEVC."

**Fix (`HwCaps.c`):** added a genuinely cheap `probe_identity_only()`
(DXGI adapter enumeration only, no D3D11 device, no decoder-profile
enumeration) used for the revision check; the full probe now only runs on
a cache miss or an actual driver-revision change. The cache parser now
reads all six codec booleans back, and fails closed (treats the cache as a
miss) if it can't find any of them, so an old-schema cache file can't be
silently trusted with stale/zeroed codec data.

## Finding #5: screenshot filename never gets a real path

`onScreenshot()` passed a path containing a literal `%d` to
`libvlc_video_take_snapshot()`. That function takes an exact output file
path - it does not do `printf`-style substitution - so every screenshot
would have been written to the same literal filename `...%d.png`, silently
overwriting the previous one.

**Fix:** builds a real, unique filename from the current timestamp before
calling the API.

## Finding #6: two disconnected "did we crash" mechanisms

`crash_handler.c` (minidump + its own `resume-state.txt` file) existed but
was never called by anything in the Qt shell. Instead, `MainWindow`
tracked its own, separate `cleanExit` boolean in `QSettings`, with no
relationship to `crash_handler.c`'s state file - two independent crash
markers that could disagree, and the actual minidump-writing code was
dead: nothing in the delivered project ever called
`nyra_crash_handler_install()`.

**Fix:** `CrashHandler.c`/`.h` (moved from `src/modules/crash/`) is now
installed at startup in `MainWindow`'s constructor, updated periodically
with the current file/position, and checked/cleared via its own real API
(`nyra_crash_handler_check_previous_crash` /
`nyra_crash_handler_mark_clean_exit`) - the `QSettings` flag is gone.

## Finding #7: A-B repeat was a documented no-op

`onSetAbRepeatPoint()`'s second click reset state instead of capturing an
end point, and `onRefreshStatsOverlay()`'s loop-check block was an empty
comment ("Intentionally left as a documented no-op stub"). The feature
listed in `STATUS.md` as "source-complete" did not function.

**Fix:** implemented for real: first click latches a start time, second
click latches an end time (validated to be after start), and the 1 Hz
stats callback seeks back to the start time once playback passes the end
time.

## Finding #8: CI workflow never built the Qt app at all

The original `.github/workflows/build-windows.yml` cloned upstream VLC,
ran VLC's own `build.sh`, located `vlc.exe`, smoke-tested `vlc.exe`, and
uploaded `vlc.exe` - it never invoked CMake, never touched Qt, and never
compiled anything under `src/ui/nyra_shell/`. This is exactly the failure
mode the brief explicitly forbade: "Do NOT create a workflow whose only
useful output is vlc.exe."

**Fix:** rewritten to run natively on `windows-latest` (no WSL2 needed for
this simpler architecture), install Qt6, download VLC's official SDK,
configure/build **this repo's** CMake project, and hard-fail (not
`|| true`) if `nyra.exe` or any required DLL/plugins folder is missing
from the packaged output. See `.github/workflows/build-windows.yml`
header comment for detail. This has been written carefully but **not
executed** - see `STATUS.md`.

## Finding #9: installer/registry files pointed at the wrong exe name

`installer/nsis-file-association.nsh` and `installer/file-association.reg`
both referenced `NyraShell.exe` throughout, and the NSIS file was a
*fragment* meant to be spliced into a VLC-generated installer script that
only exists if VLC is rebuilt from source (Finding #1 removed that step
entirely, so the base template it depended on no longer exists).

**Fix:** all references changed to `nyra.exe`; `nyra-installer.nsi` is now
a complete, standalone NSIS script runnable directly with `makensis`,
not a fragment.

## Not fixed / explicitly out of scope

- No actual Windows compile, link, or launch has been performed by
  whoever last edited this repository - see `STATUS.md` for exactly why
  and what would need to happen (a real Windows machine or the CI
  workflow) to get that verification.
- No real benchmark numbers exist. `benchmarks/benchmark.ps1` is
  functional but has not been run against real builds of Nyra and stock
  VLC on real hardware.
- Settings dialog, playlist save/load, subtitle delay/sync controls,
  audio equalizer, and crop/aspect-ratio controls remain unwritten -
  these were already flagged as scaffolded/not-started in the original
  `STATUS.md` and this pass did not add net-new UI surface area, only
  fixed correctness bugs in what already existed.
