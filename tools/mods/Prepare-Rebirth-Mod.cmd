@echo off
setlocal
title Prepare Classic REbirth mod for Android
where node >nul 2>&1
if errorlevel 1 (
  echo Install Node.js LTS from https://nodejs.org/ and open a new window.
  pause
  exit /b 1
)
if "%~1"=="" (
  echo Drag the EXTRACTED translation folder onto this file.
  echo Extract ZIP/RAR/7z archives, including any inner mod archive, first.
  echo Keep the original manifest.txt, encoding.xml, Data, text and file folders.
  pause
  exit /b 1
)
set "RE1_PACK_TOOL=%~dp0pack.js"
if not exist "%RE1_PACK_TOOL%" set "RE1_PACK_TOOL=%~dp0tools\mods\pack.js"
node "%RE1_PACK_TOOL%" rebirth "%~f1"
set "RE1_PREPARE_RESULT=%ERRORLEVEL%"
if not "%RE1_PREPARE_RESULT%"=="0" echo Preparation failed. Read the message above and check the guide.
pause
exit /b %RE1_PREPARE_RESULT%
