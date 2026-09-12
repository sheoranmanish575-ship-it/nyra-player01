# Nyra Player - top-level build
#
# FIXED vs the original project:
#   - There was no top-level CMakeLists.txt at all; only
#     src/ui/nyra_shell/CMakeLists.txt existed, and it built a target
#     literally named "NyraShell" (-> NyraShell.exe), not nyra.exe.
#   - The old modules/pmm, modules/power and modules/crash/*.c files were
#     written as *internal VLC core plugins* (vlc_player_t, intf_thread_t,
#     var_SetString on core objects) meant to be compiled INTO a forked
#     VLC source tree via scripts/automake_nyra_fragment.am. That is a
#     completely different, incompatible object model from the Qt shell,
#     which talks to VLC only through the public libvlc client API
#     (libvlc_instance_t / libvlc_media_player_t). A libvlc client app
#     does not automatically get an "interface" module's callbacks wired
#     to the player it created itself - these two halves of the project
#     could never have talked to each other as designed. See AUDIT.md
#     ("Finding #1") for the full explanation.
#
# NEW architecture (deliberate, and simpler/more buildable):
#   nyra.exe (this CMake target) links ONLY the public libvlc.dll import
#   library + Qt6::Widgets + a few Win32 system libs (d3d11/dxgi/dbghelp).
#   VLC itself is used as a prebuilt, unmodified redistributable SDK - we
#   never patch or recompile VLC's own source tree. All Nyra-specific
#   logic (hardware-capability probing, playback-mode presets, idle/power
#   management, crash handling) now lives inside this one process and
#   calls only documented, existing APIs (public libvlc + Win32 + Qt).
#   This removes the entire "fork VLC's Makefile.am and hope the
#   AM_CONDITIONAL name matches this VLC revision" risk described in the
#   original scripts/automake_nyra_fragment.am.

cmake_minimum_required(VERSION 3.21)
project(NyraPlayer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

option(NYRA_BUILD_TESTS "Build the CTest smoke-test targets" ON)

add_subdirectory(src/app)

if(NYRA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
