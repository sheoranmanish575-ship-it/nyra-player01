# Nyra Player - VLC-derived, power-aware Windows media player

## Read this first

This is a real, fixed-up source tree for a Qt6 + libVLC media player - not
a demo, not a renamed VLC. It has **not** been compiled or run by whoever
last edited it: that was done in a Linux sandbox with no network access and
no Windows toolchain, which makes producing a real Windows `.exe` there
physically impossible. See `AUDIT.md` for the full list of bugs found and
fixed in the source, and `STATUS.md` for what is and isn't verified.

**Nyra Player will not decode video "100x faster" than VLC or anything
close to it.** It uses VLC's own decode/demux/output engine (libVLC) - the
same, mature, heavily-optimized code VLC itself uses. What Nyra actually
adds is a different UI and a small set of real, documented playback-mode
presets and idle/power behaviors on top of that engine. Any specific
efficiency claim belongs in `docs/RELEASE_NOTES.md` only after
`benchmarks/benchmark.ps1` has actually been run and produced numbers -
never as an upfront promise.

## Getting a real .exe

See `BUILD.md`. Short version: push this repo to GitHub and let
`.github/workflows/build-windows.yml` build it on a real Windows runner -
that is the only way anyone (including the author of this document) has
actually verified `nyra.exe` gets produced.

## Architecture

```
                    +------------------+
                    |    nyra.exe      |   Qt6 C++ UI + PowerManager +
                    |  (this repo)     |   HwCaps probe + crash handler
                    +---------+--------+
                              |  public libvlc API only
                              v
                    +------------------+
                    |   libvlc.dll     |   unmodified, official VLC build
                    +---------+--------+
                              |
                              v
                    +------------------+
                    | VLC plugins/     |   codecs, demuxers, vout, etc.
                    +------------------+
```

VLC itself is never patched or recompiled. See `AUDIT.md` Finding #1 for
why the previous design (compiling Nyra modules into VLC's own source
tree as internal "interface" plugins) was replaced with this simpler
single-process design.

## Layout

```
nyra-player/
├── AUDIT.md                    - bugs found in the original project + fixes applied
├── STATUS.md                   - requirement-by-requirement status, honestly
├── BUILD.md                    - exact build steps
├── CMakeLists.txt / src/app/   - the actual application (target: nyra.exe)
├── .github/workflows/          - CI that builds, verifies, and packages a real .exe
├── tests/                      - CTest smoke tests (package completeness, launch check)
├── installer/                  - NSIS installer + file-association registry template
├── benchmarks/benchmark.ps1    - Nyra vs stock VLC harness (produces no numbers until run)
└── docs/                       - license notices, release-notes template
```

## Features implemented in `src/app/`

Open file/URL, drag & drop, play/pause/stop, seek, volume, playback speed,
fullscreen, audio/subtitle track selection, screenshot, A-B repeat,
playlist, four playback-mode presets (Maximum Quality / Balanced / Battery
Saver / Custom), GPU/codec capability probing at startup, minidump crash
capture with resume-state on next launch, and event-driven (non-polling)
idle/power management. See `AUDIT.md` for exactly what was broken in each
of these in the original project and what the fix was.
