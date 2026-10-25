> Historical desktop-focused README retained from this fork before the Android README rewrite.
> For the Android build, installation, current features and validation, see [the main README](../README.md) and [Android documentation](ANDROID.md).
> Statements below describe the inherited desktop project, not Android test coverage. The current Android launcher also includes separately credited launcher artwork; see its bundled notices.

# Resident Evil 1 for PC Decompilation

## Introduction

This is a decompilation Resident Evil 1 for PC released in 1997. The original game code, reverse-engineered from the Ghidra decompilation of the 1997 executable, is rebuilt on a modern rendering layer, for **two platforms**:

| Platform | Renderer | Audio | FMV |
|---|---|---|---|
| Windows (Win32) | Direct3D 11 | XAudio2 | MCI |
| Linux (SDL2 + OpenGL 3.3) | OpenGL core | SDL2 mixer | ffmpeg (Cinepak AVI) |

Both builds share every line of game logic; only the platform layer and the renderer backend differ (`src/marni/MarniDX.cpp` vs `MarniDX_GL.cpp`, `src/platform/win32/` vs `src/platform/linux/`). The GL backend is written against the GLES-3-compatible subset so the same code can be repointed at Android/Switch later.

**The port is functional-complete**: the entire original game is playable —
every room, enemy, cutscene, FMV, menu and ending. Extensive playtesting
confirms it behaves like the original release.

The DirectX 5.0 wrapper of the original (Capcom's *Marni System*, built on
DirectDraw/DirectSound) is re-implemented on top of those backends
(`src/marni/`)

## Completion status

Measured against the Ghidra project: **2393 functions in the original binary**
(+161 DLL imports provided by the loader). Every function has been triaged:

| Stream | Count | Status |
|---|---|---|
| Game logic implemented in `src\` | 1716 of 1717 | done |
| Marni System DirectX internals → DX11 / OpenGL backends | 84 of 84 | done |
| CRT / MSVC runtime (provided by toolchain) | 290 | out of scope |
| Compiler SEH / static-init glue (absorbed by real C++ ctors/dtors) | 177 | out of scope |
| Raw D3D5 API paths (replaced by MarniDX) | 108 | out of scope |
| Software-FMV shared-memory player (replaced by MCI / ffmpeg) | 11 | out of scope |
| Import thunks (loader-provided) | 6 | out of scope |

## How to build

### Windows

Requirements:
- Windows
- Visual Studio with C++ toolset and MSBuild (`Game.sln`, Win32 platform)

Build:
```
build.bat
```
This invokes MSBuild on `Game.sln` (Release / Win32) and produces
`bin\Release\residentevil.exe`. For a Debug build use:
```
build_debug.bat
```
This produces `bin\Debug\residentevil.exe`.

Both scripts find MSBuild with `vswhere.exe`, so any Visual Studio edition and
version (2017 or newer) works without editing a path. The solution itself pins
`PlatformToolset=v145` / SDK `10.0.26100.0`; on an older installation override
them with the `RE1_TOOLSET` and `RE1_SDK` environment variables, e.g.
```
set RE1_TOOLSET=v143
set RE1_SDK=10.0
build.bat
```

> **Note:** Release intentionally builds with `WholeProgramOptimization`
> disabled — `/GL`+`/LTCG` miscompiles the task scheduler's naked-assembly
> stack switching (cross-TU inlining assumes standard prologues), which caused
> intermittent crashes at room load. Don't re-enable it.

To build from a developer command prompt instead:
```
msbuild Game.sln /p:Configuration=Release /p:Platform=Win32 /t:Build
```

### Linux

A native 32-bit binary (`-m32`) built with CMake. The game logic and the Marni
layer are shared with Windows; the platform layer, the OpenGL backend and the
ffmpeg FMV decoder are Linux-only files (`src/platform/linux/`,
`src/marni/MarniDX_GL.cpp`).

Requirements (64-bit host building 32-bit).

Debian/Ubuntu:
```
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install build-essential cmake g++-multilib libc6-dev-i386 \
                 libsdl2-dev:i386 \
                 libavformat-dev:i386 libavcodec-dev:i386 \
                 libavutil-dev:i386 libswresample-dev:i386
```

On Ubuntu 24.04 (noble) `libsdl2-dev:i386` cannot be resolved: it pulls
`libpulse-dev:i386` and `libibus-1.0-dev:i386`, whose `libglib2.0-dev:i386`
dependency has no i386 build there (the glib dev utilities are amd64-only).
None of those headers are used, so install the rest and force the SDL2 dev
package past its dependency check:
```
sudo apt install -y --no-install-recommends libgl-dev libsdl2-2.0-0:i386 \
     libavformat-dev:i386 libavcodec-dev:i386 \
     libavutil-dev:i386 libswresample-dev:i386
cd /tmp && apt-get download libsdl2-dev:i386
sudo dpkg -i --force-depends --force-overwrite libsdl2-dev_*_i386.deb
```

Arch/CachyOS (with `[multilib]` enabled in `/etc/pacman.conf`):
```
sudo pacman -S --needed base-devel cmake lib32-glibc lib32-gcc-libs \
                        sdl2 lib32-sdl2 ffmpeg lib32-ffmpeg lib32-mesa
```

The 32-bit packages are the point: the target is 32-bit. On Arch the `lib32-*`
packages ship runtime objects only, so the ordinary `sdl2`/`ffmpeg` packages
supply the headers, and `lib32-mesa` is the GL driver SDL loads when it creates
the context. The build finds the 32-bit library directory itself (Debian
`/usr/lib/i386-linux-gnu`, Arch `/usr/lib32`) and links the sonames actually
installed, so the same tree configures on both — no pkg-config path or other
environment needed.

Note that the result is dynamically linked against the host's ffmpeg ABI
(ffmpeg 6 = `libavcodec.so.60` on Ubuntu 24.04, ffmpeg 9 = `.so.62` on Arch):
build on the distribution you intend to run on.

Build:
```
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j
```

This produces `build/linux/residentevil`, which can be launched from anywhere —
assets and saves are resolved from the binary's directory plus `config.ini`, not
from the working directory:

```
./build/linux/residentevil
```

Diagnostics go to stderr; `RE1_DEBUGLOG=1` also appends them to
`re1_debug.log`. A fatal signal writes a symbolized `crash.log`. Test hooks:
`--capture <file> [frames]` dumps the back buffer and exits, `--press
<scancode> <frame> [hold]` injects a synthetic key press, which together can
drive an attract-demo run headlessly.

For the sanitizer build (ASan/UBSan) and the rest of the porting notes see
`docs/LINUX_PORT.md`.

### Packaging a portable bundle

`build/linux/residentevil` is linked against the build host's ffmpeg, so it only
runs where those exact sonames exist. To run it on another distribution, whose rootfs is read-only and whose package repo is frozen — build
a bundle:

```
bash tools/package_linux.sh            # --with-assets also copies assets/
```

`dist/residentevil-<version>-linux-x86/` then holds the binary, a launcher that
puts `lib/` on `LD_LIBRARY_PATH`, the initial `config.ini` (from
`config.ini.template`) and the bundled 32-bit libraries. Only the
glibc family and the GL driver stack (`libGL`/`libEGL`/`libdrm`/`libgbm`/
`libvulkan`) stay the host's, because they have to match the running kernel and
GPU — on Arch/CachyOS that means `lib32-mesa`, which SteamOS and any
32-bit-capable desktop already have. Everything else travels, including the
X11/Wayland and ALSA/PulseAudio clients, so the bundle does not depend on the
target's 32-bit package set.

Game data is not included, same as the Windows release — migrate your own copy
into place with the asset migrator (below) and put the `USA/` (and optionally
`JPN/`, and the Director's Cut `DC/`) trees next to the launcher.

### Running the game

The decompilation took as base the GOG USA version, which is the same binary
released in 1997, so this build expects the assets of any of those versions.

Some features of the Japanese PC release (*Biohazard* Mediakite version) is supported as well

The PS1 Director's Cut's content also runs, as an overlay on top of either base
tree. Use the asset migrator (below) to lay out `USA/`, `JPN/` and the
Director's Cut `DC/` overlay. The PS1 ending credits can replace the PC's
credit movies in either mode; see [PS1 ending credits](#ps1-ending-credits).

Every asset path is built against a data root that is resolved at startup:

| What | Comes from | Default |
|---|---|---|
| Assets folder | `[Assets] Path` in `config.ini` | the executable's own directory |
| Region tree | `[Assets] Version` (`USA` / `JPN`) | `USA` |
| Save folder | `[Save] Path` in `config.ini` | `<assets folder>/SAVE` |

Relative paths are resolved from the **executable's directory**, never from the
working directory, so launching through a shortcut or a launcher with an
arbitrary CWD behaves the same. With nothing configured, put the region folder
next to the executable:

```
RE1/
  residentevil
  config.ini
  USA/
  JPN/
  DC/        (Director's Cut overlay; optional)
  SAVE/
```

The development tree keeps the assets under `assets/` one level above the build
directory, so its `config.ini` sets `Path=../../assets`. Under the VS debugger
the executable is in `bin\Debug\`, so that build needs `Path=..\..\assets` too.

### config.ini

The game reads `config.ini` from the executable's directory first (falling back
to the working directory) and creates one with commented defaults if none is
present. Release packages ship `config.ini.template` from the repository root as
that initial `config.ini`. The `[Display]`, `[Player]` and `[Input]` keys are
saved on exit; `[Assets]`, `[Save]`, `[Game]` and `[Debug]` are yours to edit and
are never rewritten.

### Asset version (USA / JPN)

`[Assets] Version` in `config.ini` selects which release the build runs. It is
read once at startup (`src/system/ConfigFile.cpp` → `SetAssetVersion`,
`src/system/AssetPath.cpp`) and swaps the data root every asset reader uses, so
**one binary runs either version** — no rebuild:

```ini
[Assets]
; Folder holding the USA/ and JPN/ trees; relative to the executable's
; directory. Empty = the executable's directory itself.
Path=
; USA = North American (default)
; JPN = Japanese (Biohazard)
Version=USA

[Save]
; Where savedat*.dat lives. Empty = <Assets Path>/SAVE, which with the default
; [Assets] Path is SAVE/ next to the executable.
Path=
```

Diagnostics: with no debugger attached, trace output is suppressed (see
below); set the environment variable `RE1_DEBUGLOG=1` to append every trace
line to `re1_debug.log` next to the exe instead. Fatal startup errors also
append a symbolized stack trace to `crash.log`.

> **Note:** never call raw `OutputDebugStringA` from code that can run inside
> a scheduler task — without a debugger it fail-fasts the process
> (`0xC0000409`) because the exception cannot be dispatched on the task's
> switched stack. Use `dbg_printf` / `dbg_safe_str` (`src/DebugPrint.h`),
> which are gated on `IsDebuggerPresent`.

### Asset migration

No game data is distributed with this repository. The player-facing importer is a
portable Qt GUI in `tools/asset_migrator/` (released as
`asset-migrator-<version>-windows-x64.zip`). It has two tabs:

- **PC Assets** — migrates a base tree from a folder (an installed GOG/retail
  copy) or from a disc image. Choose **USA** or **Japanese** for the destination
  (`USA/` or `JPN/`), and optionally convert the `Movie/*.avi` files to `.mp4`.
  The port prefers a `.mp4` sibling of the FMV table's `.avi` and falls back to
  the AVI, so keeping both is safe. Requires `ffmpeg` on `PATH` for the
  conversion.
- **Director's Cut** — builds the `DC/` overlay from a Director's Cut disc image.
  An image is required: a raw 2352-byte `.bin`/`.cue` keeps the `.STR` movies'
  CD-XA audio, which a 2048-byte copy truncates. Every DC room and all
  backgrounds are migrated.

The tool's own `README.md` covers building it, its options and the headless
verifier. The same migrations are scriptable on either platform through the
Python tools (`tools/port_dc_assets.py`, `tools/bss_to_pak.py`,
`tools/str_to_video.py`).

### Director's Cut mode

`[Game] Mode` in `config.ini` picks which release's content runs. It is a single
key that selects both the code branches and the asset overlay folder, so the two
can never disagree:

```ini
[Game]
; OG = the PC release (default); DC = the PS1 Director's Cut overlay
Mode=DC
```

`DC` is an **overlay**, not a second full tree: `DC/` beside `USA/` and `JPN/`
holds only the files the Director's Cut changes or adds, laid out like a base
tree. `ResolveAssetRoot` (`src/system/AssetPath.cpp`) probes it first and falls
through to the base tree for the folders the DC disc genuinely cannot supply, so
the base trees stay complete and pristine and several releases can share one
install. Build the overlay with the Director's Cut tab of the asset migrator; the
full design is in `docs/DC_PORT.md`.

### PS1 ending credits

The PC release plays its credits as movies with the text baked into the
picture (`stfc_r`, `stfj_r`, `stfz_r`, `staf_r`). The PS1 versions draw them
live instead, and the port can do the same, following PS1 `ENDING.EXE`:

- **Endings 1-3 (staff roll):** the `STAFF2.STF` credit strips scroll up over a
  black screen, with the movie playing as music only. The last strips are the
  RESIDENT EVIL logo, which fades out with the rest.
- **Endings 4-7 (cast roll):** the character's movie (`STFC` for Chris, `STFJ`
  for Jill) plays in shots that fade in and out. The `STAFF.STF` name panels
  appear beside it, a black wipe covers part of one shot, and the roll ends on
  the zoomed `BIO.TIM` logo.

Director's Cut mode turns it on automatically. In OG mode, turn it on in
`config.ini`:

```ini
[Game]
; 1 = draw the PS1 credits over the ending movies (DC mode is always on)
Ps1EndingCredits=1
```

It needs these files in the active tree (the `DC/` overlay, or `USA/` / `JPN/`
in OG mode):

| File | Holds |
|---|---|
| `Data/STAFF.STF` | the cast-roll name panels (endings 4-7) |
| `Data/STAFF2.STF` | the staff-roll strips (endings 1-3) |
| `Data/BIO.TIM` | the RESIDENT EVIL logo |
| `Movie/STFC.mp4`, `Movie/STFJ.mp4` | the PS1 movies, which have no text in them |

The Director's Cut tab of the asset migrator already writes all of these into
`DC/`. For OG mode, tick **Add PS1 assets from a PS1 disc image** on the PC
Assets tab and point it at a PS1 disc (the 1996 release or the Director's Cut).
Use a raw `.bin`/`.cue` image so the movies keep their CD-XA audio. If the
`.STF` files are missing, the ending plays the PC credit movie as before.

### Skippable FMVs

Each movie carries its own skip mask in the original's FMV table
(`0x004c39dc`, `src/video/VideoPlayback.cpp`): `0x0fff` for the cutscenes and
the Capcom/Virgin logos, `0x0000` — unskippable — for the endings (`ED1`-`ED8`,
`EU4`/`EU5`), the PC credit movies (`stf*_r`) and `DMF` / `DME`. A single
config key lifts that:

```ini
[Game]
; 1 = let every FMV be skipped, including the ones the original locks
SkipUnskippableFmv=1
```

It is off by default, so the shipped behaviour is the original's masks. The
original's 100-frame grace period still applies, so a press during the first few
seconds of a movie is ignored.

## Controls

The port keeps the original 1997 input model: every binding is a *function*
(action, cancel, aim, inventory, options), and both the keyboard and the pad
are remapped onto the same PS1 button word before the game logic sees them
(`JoyToPSX`, `src/game/InputSystem.cpp`). Both devices are always live and can
be used interchangeably.

### Keyboard (default layout)

The original "Key Def" defaults, unchanged (`g_keyBindingData`, `src/Globals.cpp`):

| Key | Function |
|---|---|
| Arrow Up / Down | Walk forward / backward |
| Arrow Left / Right | Turn left / right |
| `C`, `Enter`, `Space` | Action / confirm (open, examine, fire while aiming) |
| `V`, `Ctrl` | Cancel / run (hold while moving to run) |
| `Esc` | Cancel / back |
| `X` | Aim — hold to ready the weapon, then press the action key to fire |
| `Z` | Inventory / status screen |
| `A` | Options screen |

### Game pad

Every backend publishes into the same joystick slot, so the game cannot tell
them apart:

- **XInput** (Xbox pads and anything exposing an XInput device) — preferred on
  Windows, brought up first (`src/marni/MarniXInput.cpp`).
- **WinMM / HID** — the original 1997 joystick path, used on Windows when no
  XInput pad is present (e.g. a DualShock 4 plugged in directly, or an old
  SideWinder).
- **SDL_GameController** — the Linux backend (`src/platform/linux/input.cpp`);
  the standard SDL mapping covers Xbox, DualShock/DualSense and Switch Pro pads.

The original joystick defaults had no OPTIONS binding at all, so the port
installs a usable layout out of the box (`InstallPadDefaultBindings`). It only
ever replaces a table that is still empty or byte-identical to the 1997
default — anything configured in the options screen is left untouched.

| XInput / SDL | WinMM / HID (DualShock naming) | Function |
|---|---|---|
| Left stick / D-pad | Left stick / D-pad / POV hat | Move and turn |
| `A` | Cross | Action / confirm |
| `B` | Circle | Cancel / run |
| `X`, `LB`, `RB`, `RT` | Square, L1, R1, R2 | Aim |
| `Y`, `Start` | Triangle, Options | Inventory / status screen |
| `LT` | L2 | Run |
| `Back` | Share | Options screen |

A pad may be plugged in or unplugged at any time; the XInput backend rescans
on a throttle and takes over slot 0 when a pad appears.

### Rebinding

Both layouts are editable in-game from the options screen (the original
"Key Def" / "Joy Def" screens, `src/game/OptionsMenu.cpp`), including the four
original direction-mapping presets. Bindings are persisted with the rest of
the video/sound settings on exit.

### System keys

| Key | Action |
|---|---|
| `F9` | In game: return to title screen (press again to confirm). At the title: exit the game (press again to confirm). Debounced, and blocked during FMV playback. |
| `Print Screen` | Save the current frame as a timestamped `.BMP` next to the game (original behaviour; skipped when the target drive has under 1 MB free). |

### Debug keys (port addition)

Compiled into both configurations but gated at runtime on
`[Debug] EnableDebug` in `config.ini` (default: on in Debug builds, off in
Release):

| Key | Action |
|---|---|
| `F1`, or pad `L1`+`R1` together | Open / close the debug menu: room change, inventory editor, flag editor, quick access (save / load / item box). Freezes gameplay underneath. Navigate with the arrows / `Enter` / `Esc`, or with the pad's *bound* action, cancel, aim and direction functions. |
| `F6` | Texture viewer overlay. Arrows select a page, hold `A` + arrows to pan, `R` resets the pan, `F6` or `Esc` closes. |
| `F8` | Toggle the collision boundary overlay. |

The debug menu's open/close pad combo is the one input read from raw hardware
rather than through the remap table — `L1`+`R1` are buttons 5 and 6 in the
XInput, WinMM and SDL orderings, so the combo means the same thing on every
backend.

## Project layout

- `src/marni/` — the Marni System compatibility layer; `MarniDX.h` is the
  backend interface, implemented by `MarniDX.cpp` (DX11/XAudio2, Windows) and
  `MarniDX_GL.cpp` (OpenGL, Linux). Both replace DirectDraw/DirectSound/D3D5.
- `src/game/` — decompiled game logic (rooms, entities, menus, door system,
  TMD renderer, effects, screens), one module per subsystem where practical.
  Shared verbatim by both platforms.
- `src/platform/win32/`, `src/platform/linux/` — entry points, input, audio,
  video, settings, crash reporting; `src/platform/platform.h` is the seam.
- `src/video/` — FMV playback: a shared state machine over `plat_video_*`
  (MCI on Windows, ffmpeg on Linux).
- `docs/` — architecture notes: task scheduler, memory layout, classes and
  vtable conventions, implementation plan, and `LINUX_PORT.md` for the port.
- `tests/` — build/verification scripts (compile gate, frame comparison).
- `tools/` — tools used to help decompilation

Every rewritten function carries its original address as a comment, and every
named global documents its original variable address, so any line in `src/`
can be traced back to the Ghidra project.

## License

This project is licensed under the **GNU General Public License v3.0** — see
[`LICENSE`](../LICENSE) for the full text.

In practice: fork it, port it, mod it — but derivative works have to ship
their source under the same terms, so improvements stay available to
everyone.

The license covers **only the source in this repository** (`src/`, `tools/`,
`tests/`, `docs/`). It does not and cannot grant any rights over the original
game.

### Legal notice

- **No game assets are distributed here.** The repository contains source code
  only; `assets\` is git-ignored. Running the build requires your own legally
  obtained copy of *Resident Evil* for PC (the GOG or 1997 retail USA release,
  or the Japanese *Biohazard* PC release) to supply the data files.
- *Resident Evil*, its code, assets, characters and trademarks are the
  property of **CAPCOM CO., LTD.** This project is an independent
  reverse-engineering and preservation effort, **not affiliated with,
  authorized, endorsed or sponsored by Capcom** in any way.
- The decompiled logic in `src\` is derived from the original executable for
  interoperability, documentation and preservation purposes. It is published
  in the belief that this constitutes fair use / lawful reverse engineering in
  the contributors' jurisdictions; no rights over Capcom's copyrighted work
  are claimed or granted.
