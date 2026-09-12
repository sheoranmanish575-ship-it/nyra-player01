; nyra-installer.nsi - Nyra Player Windows installer
;
; FIXED vs the original installer/nsis-file-association.nsh:
;   - The original was a *fragment* meant to be spliced into VLC's own
;     generated NSIS script (extras/package/win32/NSIS/vlc.nsi.in), which
;     only exists if you actually rebuild VLC from source. Since this
;     project no longer rebuilds VLC (see AUDIT.md Finding #1/#7), that
;     base template does not exist to splice into. This is now a complete,
;     standalone NSIS script that can be run directly with makensis.
;   - Every reference to NyraShell.exe replaced with nyra.exe (the actual
;     target name - see src/app/CMakeLists.txt).
;
; Usage:
;   makensis /DNYRA_BUILD_DIR="dist\Nyra" nyra-installer.nsi
; NYRA_BUILD_DIR must contain nyra.exe, the Qt/VLC DLLs, and plugins/ -
; i.e. the output of `cmake --install`, NOT the raw build tree.

!ifndef NYRA_BUILD_DIR
  !error "Define NYRA_BUILD_DIR, e.g. makensis /DNYRA_BUILD_DIR=dist\Nyra nyra-installer.nsi"
!endif

Name "Nyra Player"
OutFile "Nyra-Player-Setup-Windows-x64.exe"
InstallDir "$PROGRAMFILES64\Nyra Player"
RequestExecutionLevel admin

!macro NyraRegisterFileAssociation Extension
  WriteRegStr HKCU "Software\Classes\${Extension}\OpenWithProgids" "NyraPlayer.Media" ""
!macroend

Section "Nyra Player Core" SecNyraCore
  SetOutPath "$INSTDIR"
  File "${NYRA_BUILD_DIR}\nyra.exe"
  File "${NYRA_BUILD_DIR}\*.dll"
  SetOutPath "$INSTDIR\plugins"
  File /r "${NYRA_BUILD_DIR}\plugins\*.*"

  WriteRegStr HKCU "Software\Classes\NyraPlayer.Media" "" "Nyra Player Media File"
  WriteRegStr HKCU "Software\Classes\NyraPlayer.Media\DefaultIcon" "" "$INSTDIR\nyra.exe,0"
  WriteRegStr HKCU "Software\Classes\NyraPlayer.Media\shell\open\command" "" '"$INSTDIR\nyra.exe" "%1"'

  !insertmacro NyraRegisterFileAssociation ".mp4"
  !insertmacro NyraRegisterFileAssociation ".mkv"
  !insertmacro NyraRegisterFileAssociation ".avi"
  !insertmacro NyraRegisterFileAssociation ".mov"
  !insertmacro NyraRegisterFileAssociation ".webm"
  !insertmacro NyraRegisterFileAssociation ".ts"

  WriteRegStr HKCU "Software\RegisteredApplications" "Nyra Player" "Software\Nyra Player\Capabilities"
  WriteRegStr HKCU "Software\Nyra Player\Capabilities" "ApplicationName" "Nyra Player"
  WriteRegStr HKCU "Software\Nyra Player\Capabilities" "ApplicationDescription" "A VLC-derived, power-aware media player"

  CreateDirectory "$SMPROGRAMS\Nyra Player"
  CreateShortcut "$SMPROGRAMS\Nyra Player\Nyra Player.lnk" "$INSTDIR\nyra.exe"
  CreateShortcut "$SMPROGRAMS\Nyra Player\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\nyra.exe"
  Delete "$INSTDIR\*.dll"
  RMDir /r "$INSTDIR\plugins"
  RMDir /r "$SMPROGRAMS\Nyra Player"
  DeleteRegKey HKCU "Software\Classes\NyraPlayer.Media"
  DeleteRegKey HKCU "Software\Nyra Player"
  DeleteRegValue HKCU "Software\RegisteredApplications" "Nyra Player"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
