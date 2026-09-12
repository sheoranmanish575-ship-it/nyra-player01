# Verifies a packaged Nyra folder actually has what it needs to run on a
# clean machine: nyra.exe, the Qt runtime DLLs, libvlc, and the plugins
# tree. Fails loudly (non-zero exit) rather than silently passing when a
# file is missing - this is the check items #3-6 in the brief's Phase 11
# ("nyra.exe exists / all required DLLs exist / Qt plugins exist / VLC
# plugins exist") map to.

if(NOT NYRA_DIST_DIR)
    message(FATAL_ERROR "NYRA_DIST_DIR not set")
endif()

set(REQUIRED_FILES
    nyra.exe
    libvlc.dll
    Qt6Core.dll
    Qt6Widgets.dll
    Qt6Gui.dll
)

set(MISSING "")
foreach(f ${REQUIRED_FILES})
    if(NOT EXISTS "${NYRA_DIST_DIR}/${f}")
        list(APPEND MISSING "${f}")
    endif()
endforeach()

if(NOT EXISTS "${NYRA_DIST_DIR}/plugins")
    list(APPEND MISSING "plugins/")
endif()

if(MISSING)
    message(FATAL_ERROR "Missing from package (${NYRA_DIST_DIR}): ${MISSING}")
endif()

message(STATUS "Package at ${NYRA_DIST_DIR} contains all required files.")
