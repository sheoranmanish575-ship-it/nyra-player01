# Builds the actual nyra.exe.
#
# FIXED vs original src/ui/nyra_shell/CMakeLists.txt:
#   - Target renamed NyraShell -> nyra (so the executable is nyra.exe,
#     which the brief requires by name; NyraShell.exe was never the
#     deliverable name).
#   - `target_link_libraries(... vlc vlccore)` REMOVED. "vlc" and
#     "vlccore" are VLC's *internal* core libraries (private ABI, changes
#     between releases without notice). The public, stable, redistributable
#     import library is libvlc (libvlc.dll / libvlc.dll.a on MinGW,
#     libvlc.lib on MSVC) - that is the only one linked now. See
#     ../../AUDIT.md Finding #1 / #2.
#   - Added the C sources (HwCaps.c, CrashHandler.c) that used to be
#     separate, unwired "VLC plugin" files; they are compiled straight
#     into nyra.exe now.
#   - Added the Win32 system libraries those C files actually call into
#     (d3d11, dxgi, dxguid, dbghelp, winmm) - the original project asked
#     for these only in an automake fragment that was never connected to
#     this CMake target at all, so hwcaps.c / crash_handler.c could not
#     have linked even if compiled.
#   - WIN32_EXECUTABLE + windeployqt packaging note kept/expanded.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Point these at the VLC SDK you are building against. The recommended,
# lowest-risk source for this SDK is VideoLAN's own published Windows
# "dev" build (see BUILD.md, Option A) - it already contains public
# headers + import libs + all plugins, with no need to compile VLC
# yourself. If you build VLC from source instead (BUILD.md, Option B),
# point these at that build's include/ and lib/ output instead.
set(VLC_SDK_DIR "" CACHE PATH
    "Root of the VLC Windows SDK: expects <dir>/include/vlc/vlc.h and <dir>/lib/libvlc.lib (or .dll.a)")

if(VLC_SDK_DIR STREQUAL "")
    message(FATAL_ERROR
        "VLC_SDK_DIR is not set. Configure with -DVLC_SDK_DIR=<path>. "
        "See BUILD.md for how to obtain the SDK - configure will not "
        "silently proceed with a guessed path.")
endif()

if(NOT EXISTS "${VLC_SDK_DIR}/include/vlc/vlc.h")
    message(FATAL_ERROR "VLC_SDK_DIR (${VLC_SDK_DIR}) does not contain include/vlc/vlc.h - wrong path?")
endif()

add_executable(nyra
    main.cpp
    MainWindow.cpp
    MainWindow.h
    PlaybackModeManagerBridge.cpp
    PlaybackModeManagerBridge.h
    PowerManager.cpp
    PowerManager.h
)

target_include_directories(nyra PRIVATE "${VLC_SDK_DIR}/include")
target_link_directories(nyra PRIVATE "${VLC_SDK_DIR}/lib")
target_link_libraries(nyra PRIVATE Qt6::Widgets libvlc)

if(WIN32)
    target_sources(nyra PRIVATE HwCaps.c HwCaps.h CrashHandler.c CrashHandler.h)
    target_link_libraries(nyra PRIVATE d3d11 dxgi dxguid dbghelp winmm)
    set_target_properties(nyra PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(nyra PRIVATE NYRA_HAVE_HWCAPS=1 NYRA_HAVE_CRASH_HANDLER=1)
else()
    message(WARNING
        "HwCaps.c and CrashHandler.c are Windows-only (DXGI/D3D11/DbgHelp) "
        "and are excluded on this platform. nyra will still build and run "
        "for local Qt/libvlc UI development, but without HW-capability "
        "probing or minidump crash handling. Nyra Player is a Windows x64 "
        "product; non-Windows is dev-only.")
endif()

# --- packaging (Windows only) ----------------------------------------
# Copies the Qt runtime DLLs/plugins next to nyra.exe via windeployqt, and
# copies the entire VLC SDK plugins/ tree + libvlc*.dll next to it too, so
# `cmake --install` produces one self-contained, runnable folder rather
# than an exe that only works next to the developer's build tree.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

    install(TARGETS nyra RUNTIME DESTINATION .)

    if(EXISTS "${VLC_SDK_DIR}/plugins")
        install(DIRECTORY "${VLC_SDK_DIR}/plugins" DESTINATION .)
    endif()
    file(GLOB VLC_RUNTIME_DLLS "${VLC_SDK_DIR}/libvlc.dll" "${VLC_SDK_DIR}/libvlccore.dll" "${VLC_SDK_DIR}/*.dll")
    if(VLC_RUNTIME_DLLS)
        install(FILES ${VLC_RUNTIME_DLLS} DESTINATION .)
    endif()

    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                --release --no-translations --no-system-d3d-compiler
                \"\${CMAKE_INSTALL_PREFIX}/nyra.exe\")
        ")
    else()
        message(WARNING "windeployqt not found - Qt runtime DLLs will NOT be "
                         "copied automatically; `cmake --install` output will "
                         "be missing Qt6Core.dll etc. unless windeployqt is on PATH.")
    endif()
endif()
