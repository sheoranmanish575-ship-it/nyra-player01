# Nyra Player - Release Notes (TEMPLATE)

**This file is a template.** No version of Nyra Player has actually been
built, run, or tested yet - that only happens once someone executes
`BUILD.md` on a real Windows machine or via CI. Fill in every `[ ]` and
`TODO` below with real data from that build/test pass before calling this
a release. Do not ship this file with placeholder numbers still in it.

## Version: v0.1.0-TODO

### Build info (fill in from your actual CI run or local build)
- Qt version: `TODO` (workflow currently pins 6.7.x - confirm actual)
- VLC SDK version/URL used: `TODO`
- Toolchain: `TODO` (MinGW-w64 via windows-latest + install-qt-action, or your local setup)
- Build date: `TODO`
- Architecture: x86_64

### What's new vs stock VLC
- **Playback Mode Manager**: Maximum Quality / Balanced / Battery Saver /
  Custom presets, each setting real, verifiable libvlc per-media options
  (see `src/app/PlaybackModeManagerBridge.cpp` and `AUDIT.md` Finding #3
  for what was cut from the original, unimplementable design).
- **Hardware capability probing**: startup DXGI/D3D11 capability detection
  with a disk cache that now actually skips the expensive enumeration pass
  on a cache hit (see `AUDIT.md` Finding #4) - TODO: report the actual
  cold-start time delta this produces, measured via `benchmarks/benchmark.ps1`.
- **Idle/power management**: event-driven high-res-timer discipline and
  stats-overlay throttling when paused/minimized (`src/app/PowerManager.cpp`).
- **Crash-safe resume**: minidump capture + resume prompt on next launch,
  now actually wired into the app (`AUDIT.md` Finding #6).
- New Qt6 UI shell: `src/app/`.

### Benchmark results (REQUIRED before this is a real release)
Run `benchmarks/benchmark.ps1` against both this build and an equivalent
stock VLC build, same hardware, same files, then paste the actual CSV
summary here:

| Metric | Stock VLC | Nyra | Delta |
|---|---|---|---|
| Avg CPU % (1080p H.264) | TODO | TODO | TODO |
| Avg CPU % (4K HEVC10 HDR) | TODO | TODO | TODO |
| Avg RAM (MB) | TODO | TODO | TODO |
| Cold start (ms) | TODO | TODO | TODO |
| Battery drain (%/hr, unplugged) | TODO | TODO | TODO |

If any row shows Nyra worse than stock VLC, that is a real, expected
possible outcome for a v0.1 - do not omit or round away an unfavorable
number.

### Known limitations
- Settings dialog, playlist save/load, subtitle delay/sync, audio
  equalizer, crop/aspect-ratio controls: not implemented yet.
- Dolby Vision: not implemented (HDR10 is).
- `[ TODO: anything found during your actual QA pass ]`

### License/compliance
See `docs/LICENSE-NOTICES.md`. Confirm the `THIRD-PARTY-NOTICES.txt`
shipped with this build actually matches whatever VLC SDK build you
linked - generate it from the SDK you downloaded, don't hand-copy this
template.
