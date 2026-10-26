@echo off
setlocal
title RE1 Mod Helper
echo RE1 Mod Helper - check and package supported translation files
echo This helper does not rewrite incompatible mods or supply missing fonts.
echo Start with docs/PORTING_PC_MODS.md in the source or helper download.
echo.
where node >nul 2>&1
if errorlevel 1 (
  echo Install Node.js LTS from https://nodejs.org/ and open a new window.
  pause
  exit /b 1
)
if "%~1"=="" (
  echo Drag an EXTRACTED supported translation folder onto this file.
  echo Extract all ZIP/RAR/7z archives first, including inner mod archives.
  echo Check for manifest.txt, encoding.xml and the matching Data/PBFONT0.tim.
  echo Model and sound replacement ZIPs do not need this translation check.
  echo Texture creation commands are explained in docs/ANDROID_MODS.md.
  pause
  exit /b 1
)
set "RE1_PACK_TOOL=%~dp0pack.js"
if not exist "%RE1_PACK_TOOL%" set "RE1_PACK_TOOL=%~dp0tools\mods\pack.js"
node "%RE1_PACK_TOOL%" rebirth "%~f1"
set "RE1_HELPER_RESULT=%ERRORLEVEL%"
if not "%RE1_HELPER_RESULT%"=="0" echo Checks failed. Read the error and the guide before trying again.
pause
exit /b %RE1_HELPER_RESULT%
