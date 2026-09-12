# License & Compliance Notes - Nyra Player

This project links against VLC media player's public libvlc library
(https://github.com/videolan/vlc). This document must be reviewed by
someone with actual authority to make license decisions for your
organization before distribution - it is a starting checklist, not legal
advice.

## What changed vs the previous version of this document

The previous design compiled Nyra's own modules directly into VLC's
`libvlccore` process image (as internal "control" plugins living in
`modules/control/nyra/`), which meant those files needed to be
GPL/LGPL-compatible because they shared a process image with GPL-licensed
VLC modules. That design has been dropped (see `AUDIT.md` Finding #1):
Nyra no longer compiles anything into VLC's own tree. It is a fully
separate process (`nyra.exe`) that links **only** the public, stable,
LGPLv2.1-or-later `libvlc.dll` API - the same one third-party embedders
like `libvlcpp`/`QtAV` use, and the same one VLC's own documentation
describes as the supported embedding surface.

## VLC's own licensing (unchanged)

- **libvlc / libvlccore** (the public API surface Nyra links): LGPLv2.1 or later.
- VLC's internal **modules** (codecs, demuxers, vout, etc., shipped as
  `plugins/*.dll` alongside `libvlc.dll`) are individually LGPL or GPLv2+.
  Nyra does not modify or link directly against any of these - it only
  loads them at runtime through libvlc, exactly as any libvlc-based
  application does.

## What this means for Nyra Player

1. **`nyra.exe`** links only `libvlc.dll` (public API) plus Qt6 and Win32
   system libraries. Because it does not statically or directly link any
   GPL-licensed VLC module or reuse VLC's own Qt GUI source
   (`modules/gui/qt/`), it can be distributed under license terms of your
   choosing, subject to LGPLv2.1's dynamic-linking conditions for the
   libvlc.dll dependency itself (ship it as an unmodified separate DLL,
   which the CMake install step already does, and don't strip its
   ability to be replaced by the end user with a compatible version).
2. Do not copy code from VLC's own Qt UI (`modules/gui/qt/`, GPLv2+) into
   this project's `src/app/` - doing so would make this GPLv2+ obligated.
   Nothing in this delivery does that; keep it that way.
3. **Any FFmpeg/contrib code paths inside the VLC SDK you distribute**
   (avcodec, x264/x265 if enabled, dav1d, etc.) carry FFmpeg's own
   licensing for that specific build configuration - this is entirely
   VideoLAN's build's concern, not something Nyra's own source changes,
   but it does affect what you must include when redistributing the SDK's
   DLLs/plugins alongside `nyra.exe`.

## Required for any binary distribution

- [ ] Ship `COPYING.LIB` (LGPL) as found in the VLC SDK you download,
      unmodified, alongside your package.
- [ ] Ship a `THIRD-PARTY-NOTICES.txt` enumerating every contrib library
      actually present in the VLC SDK build you used (FFmpeg, libass,
      libdvdnav, x264/x265 if enabled, dav1d, opus, etc.) - generate this
      from the SDK's own notices, don't hand-write a guess.
- [ ] Provide either the complete corresponding source code, or a written
      offer valid for the period required by the applicable license
      (LGPLv2.1 §6), for the LGPL/GPL components you redistribute in
      binary form (the VLC SDK's DLLs/plugins).
- [ ] If you use the "VLC" name/logo in a way that could imply official
      endorsement, check VideoLAN's trademark policy separately - the
      LGPL grant covering the *code* does not automatically grant
      trademark rights to the *name*, which is why this project uses the
      name "Nyra Player" rather than "VLC Pro" or similar.

## Qt

`src/app/` links Qt6 Widgets. Qt is available under LGPLv3 (or GPLv3, or a
commercial license). Using the LGPLv3 build of Qt is compatible with
distributing `nyra.exe` under the model described above, **provided** you
meet LGPLv3's dynamic-linking requirements (ship Qt as separate DLLs -
which `windeployqt`, invoked from `src/app/CMakeLists.txt`'s install step,
already does - and allow the end user to replace those DLLs with a
compatible version). Do not statically link Qt into `nyra.exe` without
re-checking this section; static linking changes the compliance obligations.

## Explicitly NOT covered by this document

Patent licensing for codecs (H.264/HEVC/AV1 hardware or software decode)
is a separate question from copyright/OSS licensing and is out of scope
here - consult counsel if you plan wide commercial distribution.
