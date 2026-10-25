# Build (and optionally package) the RE1 Asset Migrator.
#
#   powershell -ExecutionPolicy Bypass -File tools/asset_migrator/build.ps1
#   powershell -ExecutionPolicy Bypass -File tools/asset_migrator/build.ps1 -Package
#
# Qt 6.8 MSVC 2022 64-bit is expected at -QtDir (or $env:QTDIR). ffmpeg is a
# runtime dependency and is not bundled. The Visual Studio generator is taken
# from cmake's own default (the newest Visual Studio installed); pass
# -Generator to override it.
param(
    [string]$QtDir = $(if ($env:QTDIR) { $env:QTDIR } else { "C:\Qt\6.8.3\msvc2022_64" }),
    [string]$Config = "Release",
    [string]$Generator = "",
    [switch]$Package
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root "build"

function Find-CMake {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -property installationPath
        if ($vs) {
            $c = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $c) { return $c }
        }
    }
    throw "cmake not found (install CMake or Visual Studio's C++ CMake tools)"
}

# "Visual Studio 18 2026" on VS 2026, "Visual Studio 17 2022" on VS 2022, and
# so on: cmake marks the generator it would pick by default with "*" in
# "cmake --help", which keeps the name in step with whichever Visual Studio
# the machine has rather than with a version baked in here. cmake only offers
# a Visual Studio default when it found an instance, so a miss means the C++
# toolset is missing and is worth failing on instead of silently falling back
# to a generator that cannot compile.
function Find-Generator($cmake) {
    if ($Generator) { return $Generator }
    $default = & $cmake --help |
        Select-String -Pattern "^\*\s+(Visual Studio .+?)\s+=" |
        ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -First 1
    if (-not $default) {
        throw "no Visual Studio instance with the C++ toolset (pass -Generator to pick one)"
    }
    return $default
}

if (-not (Test-Path (Join-Path $QtDir "lib\cmake\Qt6\Qt6Config.cmake"))) {
    throw "Qt 6 not found at $QtDir (pass -QtDir or set QTDIR)"
}

$cmake = Find-CMake
$gen = Find-Generator $cmake
Write-Host "cmake     : $cmake"
Write-Host "generator : $gen"
Write-Host "Qt        : $QtDir"

$genArgs = @("-S", $root, "-B", $build, "-G", $gen,
    "-DCMAKE_PREFIX_PATH=$($QtDir -replace '\\','/')")
if ($gen -like "Visual Studio*") {
    $genArgs += @("-A", "x64")
} else {
    # Single-config generators ignore --config, and leaving CMAKE_BUILD_TYPE
    # unset would quietly build an unoptimized tool.
    $genArgs += "-DCMAKE_BUILD_TYPE=$Config"
}

& $cmake @genArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& $cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { throw "build failed" }

# The Visual Studio generators are multi-config and drop the binaries in a
# per-config subdirectory; single-config generators (Ninja) write them beside
# the build system, and $Config reached them as CMAKE_BUILD_TYPE.
$out = if ($gen -like "Visual Studio*") { Join-Path $build $Config } else { $build }
$exe = Join-Path $out "re1_asset_migrator.exe"
if (-not (Test-Path $exe)) { throw "no exe at $exe" }
Write-Host "built : $exe"

if ($Package) {
    $windeploy = Join-Path $QtDir "bin\windeployqt.exe"
    if (-not (Test-Path $windeploy)) { throw "windeployqt not found at $windeploy" }
    & $windeploy --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

    $dist = Join-Path $root "dist\re1_asset_migrator"
    if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
    New-Item -ItemType Directory -Force -Path $dist | Out-Null
    Copy-Item (Join-Path $out "*") $dist -Recurse -Force
    # The import library / linker artifacts are build outputs, not runtime.
    Remove-Item (Join-Path $dist "*.lib"), (Join-Path $dist "*.exp"),
        (Join-Path $dist "*.pdb") -Force -ErrorAction SilentlyContinue
    Write-Host "packaged: $dist"
}
