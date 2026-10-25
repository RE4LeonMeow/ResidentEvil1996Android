@echo off
setlocal

rem Locate MSBuild through vswhere instead of hardcoding the Visual Studio
rem edition/version path (Community/Professional/Enterprise, 17/18/...).
rem vswhere ships with every VS 2017+ installer at this fixed location.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MSBUILD="

if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"

rem Fall back to a Developer Command Prompt, where MSBuild is already on PATH.
if not defined MSBUILD (
    where msbuild >nul 2>&1 && set "MSBUILD=msbuild"
)

if not defined MSBUILD (
    echo error: MSBuild not found. Install Visual Studio with the MSBuild component,
    echo or run this from a Developer Command Prompt.
    exit /b 1
)

rem Game.vcxproj pins PlatformToolset v145 / SDK 10.0.26100.0 (VS 2026). An
rem older VS can override them, e.g.  set RE1_TOOLSET=v143 && set RE1_SDK=10.0
set "RE1_EXTRA="
if defined RE1_TOOLSET set "RE1_EXTRA=%RE1_EXTRA% /p:PlatformToolset=%RE1_TOOLSET%"
if defined RE1_SDK set "RE1_EXTRA=%RE1_EXTRA% /p:WindowsTargetPlatformVersion=%RE1_SDK%"

"%MSBUILD%" "Game.sln" /p:Configuration=Debug /p:Platform=Win32 %RE1_EXTRA% /t:Build /v:minimal
