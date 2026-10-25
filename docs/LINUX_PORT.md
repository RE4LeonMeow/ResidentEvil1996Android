# Native Linux Port — Analysis and Work Plan

Status: **Phases 0-5 implemented.** The Linux binary boots the real game,
renders the title screen, responds to the keyboard, and renders an in-game room
with correct 3D depth. Phases 6-10 planned.
Scope: a native Linux binary that shares the game logic with the existing
Win32 build, which must keep building exactly as it does today.

**Decided:** OpenGL backend (not Vulkan), Linux first, structured as the
template for a later Android/Switch port (§6.1, §7).

**Deviation policy (porting):** types, naming and structure may change freely.
Two things may not: **game logic**, and **any layout that is an on-disk or
in-memory format** (the `BioCard` 0x41C block, save blocks, packed structs read
from asset files, the `static_assert`s that pin them). Layouts that exist only
to mirror the original binary's addresses are fair game to change.

---

## 1. Verdict

The port is **feasible and structurally cheap**, because the project already
contains the exact seam it needs.

`docs/MARNI_SYSTEM.md` states the rule the codebase was built to:

> **Backend Isolation**: no ported game code calls raw DirectX APIs — everything
> routes through the Marni classes, which forward to `MarniDX`. This is what made
> the D3D5 → DX11 swap possible without touching game code.

A Linux backend is that same manoeuvre a third time (D3D5 → D3D11 → GL/Vulkan).
`MarniDX.h` is already a backend-agnostic interface with an explicit contract in
its own header comment:

> Public header — MUST NOT include d3d11.h, dxgi.h, d3dcompiler.h, xaudio2.h,
> or xinput.h. Only `<windows.h>` + `MarniBits.h` are permitted.

Four measured facts make the job smaller than it looks:

| Finding | Evidence | Consequence |
|---|---|---|
| **No GDI text/font rendering anywhere** | `GetDC` only at `main.cpp:129,304` (color-depth query); `GetStockObject` at `main.cpp:546` (window brush). All text is a PSX TIM atlas drawn as sprites (`PrintText.cpp` → `AddTintSprite`) | The entire text/UI path is portable as-is |
| **No threading** | Whole tree: 2× `Sleep`, 1× `WaitForSingleObject`, zero `CreateThread`/`CriticalSection`/`Interlocked*` | No thread abstraction needed; the cooperative task scheduler is the only concurrency |
| **File I/O is mostly stdio** | 10× `fopen`, 9× `fclose`, 6× `fseek`, 4× `fread` vs 3× `CreateFileA`/`ReadFile`, 1 memory-mapped file | File layer is nearly portable already |
| **The Win32 surface is types, not APIs** | exactly **1 of 84** `.cpp` files is Windows-free today; 75 of 76 reach `<windows.h>` transitively | a typedef shim unlocks the whole tree |

The platform-specific code is roughly **12k of 113k lines (~10%)**; the ~90k
lines of game logic in `src/game/` are shared verbatim.

---

## 2. Measured platform surface

Line counts from the current tree:

| Area | Lines | Windows dependency | Linux replacement |
|---|---|---|---|
| `src/game/` (+ `entities/`) | 63,725 + 33,636 | Win32 *types* only, via `Globals.h` → `Types.h` | shared, unchanged |
| `src/marni/` | 9,147 | D3D11, DXGI, XAudio2, XInput, WinMM | new backend(s) |
| `src/Globals.cpp` | 2,042 | `HWND`/`HANDLE` fields, section placement | shim + linker script |
| `src/platform/win32/main.cpp` + `window_proc.cpp` | 778 + 205 | window, message pump, registry, mutex | platform layer |
| `src/system/` | 951 | registry, display modes, crash log | platform layer |
| `src/video/` | 554 | MCI AVI playback | ffmpeg-based FMV |
| `src/DebugPrint.h`, `Types.h` | small | `windows.h`, `mmsystem.h` | shim |

### Win32 API call census (whole tree)

| API | Count | Used for | Linux approach |
|---|---|---|---|
| `GetAsyncKeyState` | 59 | original "DirectInput" keyboard wrapper | SDL2 keyboard / evdev |
| `RegQueryValueExA` / `RegSetValueExA` / `RegOpenKeyExA` | 14 / 8 / 2 | settings + install path | own config store |
| `mciSendStringA` | 11 | FMV playback | ffmpeg decode → texture |
| `joyGetPosEx` / `joyGetNumDevs` / `joyGetDevCapsA` | 10 / 3 / 2 | WinMM pad backend | SDL2 gamepad |
| `timeGetTime` / `GetTickCount` | 8 / 3 | frame limiter, timers | `clock_gettime(CLOCK_MONOTONIC)` |
| `GetPrivateProfileIntA` / `GetPrivateProfileStringA` | 6 / 3 | `config.ini` | own ini parser |
| `XInputGetState` | 5 | pad backend | SDL2 gamepad |
| `PlaySound` | 12 | legacy sound path | audio backend |
| `SystemParametersInfoA` / `ShowCursor` | 4 / 4 | accessibility anim, cursor | drop / SDL cursor |
| `GetFileAttributesA` / `CreateFileA` / `GetModuleFileNameA` | 4 / 3 / 1 | existence, load, exe dir | `stat` / stdio / `/proc/self/exe` |
| `waveOut*` | 8 total | sound device enumeration | audio backend |
| `GetDeviceCaps` | 2 | color-depth check | drop on Linux |
| `ImmAssociateContext` | 1 | IME detach | drop |

Note how *narrow* this is. There is no DirectInput, no COM, no shell API, no
GDI drawing, no thread pool, and no SEH-dependent control flow in game logic.

---

## 3. The backend contract a Linux renderer must satisfy

`src/marni/MarniDX.h` is the complete, already-D3D11-free interface — **30 public
methods** plus 3 globals. A GL (or Vulkan) backend must implement, as a drop-in
replacement:

**Lifecycle**
- `Create(HWND, w, h, fullScreen, *outW, *outH)`, `Destroy()`, `IsReady()`
- `GetBackBufferSize()`, `ChangeDisplayMode()`, `HandleWindowMessage()`
- `QueryVideoMemory()`, `EnumerateDisplayModes()`, `EnumerateAdapters()`
- `Marni_DX()`, `MarniDX_Create()`, `MarniDX_DestroyGlobal()`

**Textures** (opaque `MarniHandle`, 1-based; no COM pointer ever escapes)
- `CreateTexture(w, h, bpp, pixels, *outW, *outH)`
- `CreateTextureFromBits(CMarniBits*)`, `UpdateTexturePixels(...)`
- `GetTextureSize()`, `DestroyTexture()`, `WhiteTexture()`
- `SetFontTexture()`, `FontTexture()`, `FontTextureWidth/Height()`

**Drawing**
- `DrawSprite(x,y,w,h,u0,v0,u1,v1,color,tex,sampler,blend)`
- `DrawRect()`, `DrawLine()`
- `DrawTriangles()` — 8 floats/vertex, screen-space affine
- `DrawTrianglesPersp()` — 10 floats/vertex, `w` for perspective-correct UV
- `DrawTriangles3D()` — 9 floats/vertex, depth test/write `LESS_EQUAL`

**Readback**
- `CaptureBackbufferToRGBA()` (screenshots / `SaveBitmapToFile`)
- `ReadTextureRGBA()` (card-mask compositing)

The render state they need is small and well documented in
`docs/MARNI_SYSTEM.md`: three blend modes (`ALPHA`/`ADD`/`DISABLE`), two
samplers (`POINT` for pixel art, `LINEAR` for backgrounds), a depth buffer
cleared once per frame, scissor, and a 320×240 logical → physical scale.

The HLSL is three trivial shaders (quad VS, quad PS, model3D VS) embedded as
source in `MarniDX.cpp:41/69/85`; they translate directly to GLSL/SPIR-V.
The D3D11 state all lives in `struct MarniDX::Impl` (`MarniDX.cpp:128-190`) —
one device, one context, a `TexSlot slots[2048]` handle table, three blend
states, two samplers, three depth-stencil states, a dynamic quad VB capped at
1024 triangles per call.

### 3b. ABI constraints that are *not* negotiable

Game code does not only call the Marni facade — it reaches through raw
pointers, so the following must be preserved exactly by any Linux build:

- **The 12-entry `CMarniDirect3D` vtable** (`g_CMarniDirect3D_VTable`,
  `MarniSystem.cpp:72`) with its original entry order and `__cdecl` adapter
  convention (`self` as the first stack argument). Game code indexes it
  directly: `WindowProc.cpp:87-90` (vtable[5]), `TextureLoader.cpp:256-289`
  (vtable[6..9]), `PathTrail.cpp:218-221,361-362,619-621`,
  `Rendering.cpp:651-652` (vtable[11]), `ObjectManager.cpp:23-27,102,135`.
- **`CMarniDirect3D` field offsets** (size forced to `0x21DC`,
  `MarniSystem.h:89`) — game code casts `g_pMarniDirect3D` and reads fields by
  name at fixed offsets.
- **`CMarniViewport2` / `CDirect3DObject` vtables are `__stdcall`** (callee
  cleans, `RET N`); calling them through a `__cdecl` pointer type corrupts ESP.
  `CMarniBits` vtables are `__cdecl`. See `docs/CLASSES_AND_VTABLES.md:360-398`.
- **`Marni_DX()` is called directly by game code** in 20+ places
  (`TmdRenderer.cpp:373,715,727,738`; `TextureLoader.cpp:87..1374`;
  `Rendering.cpp:1013`; `SpriteRenderer.cpp:1004`; `EffectSprites.cpp:469,476,700`;
  `CollisionDebug.cpp:273`), so the whole `MarniDX` method set must survive,
  not just the free-function facade.

**One genuine boundary leak exists today**: `src/system/DisplayConfig.cpp:7-8`
includes `<d3d11.h>` and `<dxgi.h>` and drives `CreateDXGIFactory` /
`IDXGIFactory` / `IDXGIAdapter` / `IDXGIOutput` / `DXGI_MODE_DESC` directly
(lines 18-88), called from `main.cpp:407,422`. It must be replaced along with
the backend, not just recompiled.

---

## 4. Work items

### A. Build system
- Add `CMakeLists.txt` as the single source of truth for the file list; the
  existing `Game.vcxproj` (109 `ClCompile` entries) must keep working. Either
  generate the vcxproj from CMake or keep both in sync with a check.
- Keep the MSVC path byte-for-byte intact: `WholeProgramOptimization` stays off
  (see `Game.vcxproj:33-37` — LTCG miscompiles the task scheduler).
- Linux target: C++17, `-m32`, and the same `/FS`-equivalent constraints.

### B. Win32 compatibility layer
`src/game/Types.h:4-5` includes `<windows.h>` + `<mmsystem.h>`, and
`Globals.h` pulls `Types.h` into **75 of the 84** `.cpp` files. So the shared
logic needs a **type shim**, not a rewrite:

- `typedef uint32_t DWORD; uint8_t BYTE; int BOOL; ...`, `TRUE/FALSE`,
  `HWND`/`HANDLE` as opaque pointers, `RECT`, `LARGE_INTEGER`, `MCIERROR`,
  `VK_*` scancodes, `MAKEWORD`, `LOWORD/HIWORD`, `RGB()`.
- Guard the two `#include`s in `Types.h:4-5` with `#ifdef _WIN32`.
- **Exactly 1 of the 84 `.cpp` files under `src/` is currently Windows-free**
  (`src/system/AssetPath.cpp`) — and even it calls `sprintf_s`. 75 of the 76
  non-Marni files reach `<windows.h>` transitively (74 of them through
  `Globals.h`). This is precisely why the shim is mandatory: without it,
  *nothing* compiles on Linux.
- Only **17 of 109** files include `<windows.h>` directly (16 headers + 2 .cpp),
  so the shim surface itself is small and the include graph is shallow.
- MSVC `_s` CRT calls must be shimmed too: `sprintf_s`, `strcpy_s`, `strcat_s`,
  `_vsnprintf_s`, `_snprintf` appear in `AssetPath.cpp:88`,
  `VideoPlayback.cpp:164-306`, `CrashLog.cpp:69`, `DebugPrint.h:55`,
  `SoundSystem.cpp:1469`, `GteMatrix.cpp:998`, `MainMenu.cpp:6393`.
- Two things that *do* transfer unchanged, so don't touch them: `#pragma pack(1)`
  (identical semantics in GCC/Clang, and 65 `static_assert`s already pin the
  sizes) and the **vtable-as-plain-field-at-offset-0** pattern — the classes
  deliberately declare no C++ virtuals, so no compiler-inserted vptr shifts the
  fields (`MarniSystem.h:34`, `MarniBits.h:10`, `Marni3DObject.h:21`).
- `#pragma comment(lib, ...)` (4 files) becomes link flags.
- This is the difference between "port 90k lines" and "port 12k lines".

### C. Graphics backend (`MarniDX_GL.cpp`)
- Implement §3 (30 methods) against OpenGL, written to a GLES-3-compatible
  subset (see §7.1 — decided; this is what keeps Android/Switch reachable).
- Replace `src/system/DisplayConfig.cpp`'s DXGI enumeration with SDL2 display
  mode enumeration (see §3b — it is the one leak outside `src/marni/`).
- Preserve the vtable/ABI constraints in §3b; do not "clean up" the raw
  vtable calls in game code.
- Reuse `MarniBits`/`PSXTexture` CPU-side CLUT→RGBA conversion unchanged —
  only the upload differs.
- Preserve the documented format traps: `DXGI_FORMAT_R8G8B8A8_UNORM` packs red
  in the **lowest** byte (`docs/MARNI_SYSTEM.md` "Pipeline invariants").
- Preserve the depth/OT split (`docs/ARCHITECTURE.md` "Rendering Pipeline"):
  phase 1 depth ≥ 500, phase 2 sprite commands, phase 3 depth < 500.

### D. Audio backend
`MarniSound` wraps DirectSound→XAudio2 with a stable, already-abstracted API.
The model is **one reusable source voice per bank** (`g_BankVoices[81]`, banks
1-80, created eagerly in `CreateSound` from a hand-parsed WAV/RIFF `fmt `+
`data` chunk pair, `MarniSound.cpp:586-656`), a fixed 2ch/22050 Hz mastering
voice (`:923`), **no software mixer, no resampler, no 3D panning** — XAudio2
mixes. `PlaySound(bank, slot)` maps `slot != 0` to `XAUDIO2_LOOP_INFINITE`;
`SetPan` only records the value. The one semantic contract to preserve is the
volume curve `DsVolumeToAmplitude(mB) = 10^(mB/2000)`, clamped, `0` at ≤ −10000
and `1.0` at ≥ 0 (`MarniSound.cpp:161-166`), because every caller passes
DirectSound millibels.

An OpenAL/miniaudio/SDL backend must satisfy the `DirectSound` method set plus
the free functions (`loadSndBankFromWav`, `destroySndBank`, `setSndStop`,
`playSnd`, `SetSndSlot`, `set_volume`, `pan_set`, `getSndVol`,
`UpdateSoundFade`, `InitializeSoundSystem`, `CleanupSoundManagerResources`).
`PauseGameSoundsCallback`/`ResumeGameSoundsCallback` are game-layer
(`SoundSystem.cpp:188,211`) and need no work.

### E. Input backend
The seam is already clean and single-slot. A replacement only has to produce
the raw masks; everything downstream is game logic:

- **Keyboard**: 32 VK codes in `MasterInputState.keyMap` → a 32-bit bitmask via
  `GetAsyncKeyState` (`MarniInput.cpp:35-42`). Replace with an SDL2/evdev state
  array; keep the `prev`/`curr`/`newPress`/`repeat` transition semantics.
- **Pad**: device → `JoystickEntry.currPress`, a 32-bit mask with **bits 0-3 =
  axis directions, bits 4-7 = POV hat (deliberately unused), bits 8-19 =
  buttons**. XInput and WinMM both publish into `joysticks[0]` with identical
  `prev/curr/newPress` semantics (`MarniInput.cpp:107-146`), so the game cannot
  tell them apart.
- Downstream, `JoyToPSX` (`InputSystem.cpp:102`) walks the 32 bits against
  `g_JoyRemapTbl` to build the PS1 button word — shared and untouched.
  See `docs/GAMEPAD_INPUT.md` for the bit contract.

### F. FMV playback
`src/video/VideoPlayback.cpp` drives MCI, which **renders directly into
`g_hWnd`**. Assets are 27 Cinepak (`cvid`) AVIs, 242 MB, 10 fps. Replacement:
decode with ffmpeg/libavcodec into a texture, keep the existing 4-state machine
(`g_FMVPlaybackState` 0..3), the skip mask table and the `g_videoSkipCounter`
grace period. Audio must be synced to the same clock.

### G. Window and lifecycle
`main.cpp:552` `CreateWindowExA`, `main.cpp:603` `PeekMessageA` pump,
`main.cpp:697` `timeGetTime()` 33 ms limiter, focus-loss pause gate at
`main.cpp:686-691`. Replace with SDL2 window + event pump, keeping the
**33 ms tick** semantics (the play clock and every scripted frame count assume
30 ticks/s). Fullscreen is `WS_POPUP | WS_EX_TOPMOST` today, and the D3D11
swap chain is deliberately **windowed blt-model even in fullscreen**
(`MarniDX.cpp:455-487`) so DWM scales the back buffer — use a borderless
fullscreen window on Linux (see the existing "borderless swapchain for GDI MCI"
note).

### H. Task scheduler (highest-risk item)
`src/game/TaskScheduler.cpp` is MSVC-only: five `__declspec(naked)` functions
with `__asm {}` blocks doing raw `ESP` switching, plus
`#pragma runtime_checks("s", off)`, `#pragma optimize("y", on)` and
`#pragma warning(disable: 4731)`.
- GCC cannot compile MSVC inline asm at all; Clang can on i386 with
  `-fms-extensions`. Portable options: a GAS-syntax `__asm__` translation in the
  same file under `#ifdef`, or a `.S` file. The machine contract is in the file
  header: `PUSHAD`/`PUSHFD`, save scheduler ESP, load task ESP, `JMP` task EIP;
  resume via `POPAD`/`RET`. `Task_chain`/`Task_exit` use inline
  `mov esp,[g_SchedulerESP]; ret`.
- **Frame-pointer omission is mandatory** and must be forced on Linux with
  `-fomit-frame-pointer` (the `#pragma optimize("y", on)` has no effect there).
  Without it a task's `mov esp,ebp` epilogue jumps into the scheduler's frame.
- The `/RTCs` hazard that `#pragma runtime_checks` guards against **does not
  exist** in GCC/Clang, so that constraint simply disappears.
- **Keep LTO off** — it is the GCC/Clang analogue of the `/GL`+`/LTCG`
  miscompilation documented in `Game.vcxproj:33-38`.
- Guard pages: `VirtualAlloc`/`VirtualProtect` → `mmap`/`mprotect`
  (`TaskScheduler.cpp:338,345`); `VirtualQuery` also appears in
  `MarniSystem.cpp:313,345`.
- **The entry-ESP trick must be re-derived.** `TaskStackTop()` returns
  `slot_top - 16` specifically because MSVC's Release alignment prologue
  (`push ebx / mov ebx,esp / and esp,-16 / mov ebp,[ebx+4]`) reads
  `[entry_esp]`, and `slot_top` is the first `PAGE_NOACCESS` byte
  (`TaskScheduler.cpp:63-77`). GCC/Clang emit a different prologue; the offset
  and the readability assumption need re-testing against their codegen. The
  32-bit i386 SysV ABI keeps the stack 16-byte aligned at call boundaries, so
  the alignment half of the trick should still hold.
- The guard pages themselves are a **port addition** (the original had
  contiguous stacks) kept for crash safety; they can be preserved as-is on
  Linux via `mmap`.

### I. Memory layout
`src/Globals.cpp` places globals into custom sections:
- `.sched` — task scheduler state, `g_pMarniDirect3D`, `g_pMasterInputState`
  (kept out of the wiped range). Single section, declaration-ordered.
- `.gwipe$<4 hex digits>` — 20 subsections reproducing the original
  `0x00be41e0..0x00be9620` game-init wipe block, with `$41e0`
  (`g_defaultItemSlot`) and `$9620` (`g_BioCard`) as the sentinels.
- `.items` — `g_ItemsImageBuffer` (86400 B, `data/item_all.pix` load target).

**The ordering is purely MSVC `$`-subsection name sorting.** Verified: there is
no `/ORDER`, `/MERGE`, `/SECTION`, `/FIXED`, `/BASE`, `.def`, or linker script
anywhere in the project, and all `.gwipe` members live in one TU. GNU `ld` has
no equivalent merge-and-sort, so as written the guarantee does not carry over.

**Recommendation: remove the requirement rather than reproduce it.** The wipe
is already symbol-to-symbol (`memclr(&g_defaultItemSlot, g_BioCardData)`,
`GameStart.cpp:272`); the ordering only exists so that the contiguous range
happens to enclose the intended set. The repo has already converted one such
range into explicit per-symbol clears (`ClearGameStateFlags`,
`GameInit.cpp:31-47`). Doing the same for the game-init wipe deletes the
ordering constraint for both toolchains — no linker script needed.

Two smaller notes: `g_hWnd` sits in `.sched` incidentally (a bare
`__declspec(allocate)` at `Globals.cpp:76` attaches to the next declaration)
and is not in the documented resident list; and `MEMORY_LAYOUT.md:91-94` still
lists `$63a0..$63b0` members that no longer exist.

**No base-address dependency exists** — no `/BASE`/`/FIXED`, and the literal
original addresses that appear in code are inert (verified: the `0x606060` tint
argument is unused, the `FUN_00484420` dst pointer is ignored, and the
`g_EffectSpriteTexConfig` address table is dead data).

### J. Config, paths, persistence
- `GetPrivateProfile*` (9 calls) → own ini parser; `config.ini` semantics
  (`[Display]`, `[Assets] Version`, `[Debug] EnableDebug`) must stay identical.
- Registry (24 calls, `HKCU\Software\CAPCOM\RESIDENT EVIL`) → a config file
  under `$XDG_CONFIG_HOME`; keep the key/value names for save compatibility.
- **Backslash paths**: `GAME_DATA_ROOT "..."` templates appear **110 times
  across 27 files** (heaviest: `DoorSystem.cpp` 35, `VideoPlayback.cpp` 28,
  `MainMenu.cpp` 28), plus plain literals like `stage%c\room%c%c%c0.rdt`,
  `sound\%s.wav`, `data\fontus.tim`, `SAVE\`, `.\usa\`. All must become `/`.
  The root constants are centralized in `AssetPath.h:25-45`, which helps.
- **Case sensitivity**: asset dirs are case-mixed (`Data`, `Effspr`, `ITEM_M1`,
  `STAGE1`, `voice` vs `DATA`, `effspr`, `ENEMY`, `STAGE1` in JPN). NTFS hid
  this. Note there is **no `FindFirstFile` anywhere** — existence checks use
  `fopen`/`GetFileAttributesA` — so a directory-scan fallback has to be written
  into the path layer; exact-name matching or a case-folding resolver are the
  options.

### K. Crash log and diagnostics
`src/system/CrashLog.cpp` uses `dbghelp` (`StackWalk`) → `backtrace()` +
`addr2line`/`libbacktrace`. `OutputDebugStringA` / `IsDebuggerPresent` gating in
`DebugPrint.h` → stderr / `RE1_DEBUGLOG` behavior preserved.

### L. Misc / startup checks
- `MessageBoxA` → `SDL_ShowSimpleMessageBox`.
- Named mutex single-instance check (`main.cpp:325-343`) → `flock`/lockfile.
- Shared-memory software-render path (`CreateFileMappingA(..., "bio1997")`,
  `main.cpp:191-206`) → drop on Linux.
- `GetDeviceCaps` color-depth check (`main.cpp:129-131,304-306`) → drop.
- `ImmAssociateContext` (IME, `main.cpp:566`) → drop.
- `ShowCursor` (`main.cpp:613`, `SystemChecks.cpp:102-108`) → SDL cursor API.
- `LoadLibraryA("dinput.dll")` (`main.cpp:102`) → dead load, drop.
- `CreateProcessA("\\Setup.exe", ...)` (`main.cpp:387`) → install/setup path;
  drop or replace with a no-op on Linux.
- System checks in `SystemChecks.cpp` (`GetDiskFreeSpaceExA`,
  `GetLogicalDriveStringsA`, `GetDriveTypeA`, `"C:\\"`) and `main.cpp`
  (`GetVersionExA`, `GlobalMemoryStatus`) → reimplement or drop.
- Crash handler (`CrashLog.cpp`) uses SEH + dbghelp:
  `SetUnhandledExceptionFilter`, `AddVectoredExceptionHandler`,
  `_set_invalid_parameter_handler`, `_set_purecall_handler`,
  `SymInitialize`/`SymFromAddr` → `signal`/`sigaction` + `backtrace()`.
- `WM_MCINOTIFY`/`MCI_NOTIFY_SUCCESSFUL` (`WindowProc.cpp:195-199`) → the FMV
  backend signals completion directly.
- The F9/F10/F11 menu combo is driven by raw scan codes
  (`g_lastScanCodeOrMsgID`, compared to `0x5b/0x5c/0x5d` in `MainLoop.cpp:47`)
  — needs the Linux key layer to expose equivalent scan codes.

---

## 5. Risks

| Risk | Why it matters |
|---|---|
| **Task scheduler fidelity** | Release-grade requirement. Naked-asm ESP switching + compiler-specific stack alignment. A wrong entry ESP corrupts task stacks intermittently. |
| **Depth/ordering parity** | The three-phase depth split and per-triangle depth sort are behavioural, not cosmetic. |
| **Color/format traps** | Documented invariants where two errors cancelled; changing one breaks rendering (see `docs/MARNI_SYSTEM.md` invariants table). |
| **Case-sensitive assets** | Silent "file not found" → wrong/missing textures and rooms. |
| **FMV timing** | 10 fps Cinepak + skip masks + audio sync; MCI did the clock today. |
| **Save compatibility** | `BioCard` layout, save block spans, path roots. |
| **No test harness** | There is no automated behavioural test; parity relies on playtesting. |

---

## 6. Implementation plan

**Decisions taken:** OpenGL backend (not Vulkan), Linux first, structured so the
Linux port becomes the template for Android/Switch rather than a throwaway.
That means three portability hooks go in *now*, at no extra cost:
a `ucontext`-based scheduler instead of new x86 asm, `uintptr_t` for stored
pointers instead of `DWORD`, and a GLES-3-compatible GL subset.

### 6.1 Target shape

One tree, backend selected at build time. Two orthogonal axes:

- **`src/platform/<os>/`** — OS services (window, raw input devices, time,
  filesystem, config store, crash reporting, scheduler stack switching).
- **`src/marni/`** — graphics/audio *API* backends, selected independently of
  the OS (D3D11 vs GL; XAudio2 vs SDL audio).

```
CMakeLists.txt                     # new: file list + backend selection
tests/                             # port tests + the boundary gate
  check_platform_boundary.py       # src/game must stay OS-agnostic
src/
  platform/
    types.h                        # portable Win32 typedefs (DWORD/BYTE/BOOL/…)
    platform.h                     # OS services the game calls — no OS types
    win32/
      main.cpp               # WinMain + startup (moved in P0)
      window_proc.cpp              # = today's WindowProc.cpp (moved in P0)
      platform.cpp               # P0: keys, time, files, debug, audio vol, stacks
      plat_window.cpp              # (P2) CreateWindowEx / GL context
      config.cpp              # (P8) registry + GetPrivateProfile*
      crash.cpp               # (P8) = today's system/CrashLog.cpp (dbghelp/SEH)
      plat_display.cpp             # (P8) = today's system/DisplayConfig.cpp (DXGI)
      plat_install.cpp             # (P8) = today's system/Installation.cpp (registry)
      plat_sched_asm.cpp           # (P1) MSVC naked-asm task switch
    linux/
      main.cpp
      plat_window.cpp              # SDL2 window/context/fullscreen/focus
      input.cpp               # SDL2 keyboard + gamepad → masks
      plat_time.cpp
      plat_file.cpp                # path normalize + case-fold resolver
      config.cpp              # config.ini + settings store
      crash.cpp               # signal handlers + backtrace
      plat_display.cpp             # SDL2 display modes
      plat_sched_ucontext.cpp      # portable task switch (also the ARM path)
  marni/
    MarniDX.h                      # UNCHANGED interface (30 methods)
    MarniDX_D3D11.cpp              # = today's MarniDX.cpp
    MarniDX_GL.cpp                 # NEW: same class, same globals, GL impl
    MarniSound_XAudio2.cpp         # = today's MarniSound.cpp
    MarniSound_SDL.cpp             # NEW: same DirectSound API
    MarniInput.h/.cpp              # neutral: polls via platform/plat_input
  video/
    VideoPlayback_MCI.cpp          # = today's file (Windows)
    VideoPlayback_ffmpeg.cpp       # NEW: same 4-state machine, ffmpeg decoder
  game/                            # OS-agnostic after Phase 0
```

Tests live in `tests/`, not `tools/`. `tools/` stays what it is — decompilation
tooling. Its `verify_msg_*.py` / `test_rdt_editor.js` scripts are referenced by
`docs/TEXT_ENCODING.md` and test artifacts that live in `tools/`, so they stay
put; anything written for the port goes in `tests/`.

The renderer/sound backends are not new abstractions — they are **alternative
definitions of the existing classes**, so `Marni_DX()`, `g_pMarniDirect3D`, the
vtables and every call site keep working verbatim. Same trick as D3D5 → D3D11,
one level down.

**`src/platform/`, not a root-level `platforms/`.** It matches the existing
`src/<subsystem>/` layout, keeps the include root at `src/` (no extra include
dir in both CMake and `Game.vcxproj`), and keeps per-OS code next to the game
that calls it. A root `platforms/` works too if you prefer physical separation;
the only cost is one more include path in two build files.

**What "OS-agnostic" means here.** `src/game/` must end up with **no OS API
calls and no OS handles**. The `DWORD`/`BYTE`/`BOOL`/`WORD` *typedefs* stay,
because `platform/types.h` supplies them on both platforms (`<windows.h>` on
Windows, explicit typedefs elsewhere) — one header, no churn, and `src/game/`
stops including `<windows.h>` at all.

A mass `DWORD` → `uint32_t` rename is now *permitted* by the deviation policy
but still not recommended as a big-bang: ~90k lines of mechanical diff for no
functional gain, and it would conflict with ongoing decompilation work. Migrate
opportunistically — in the new `platform/` code, and in any file a phase already
touches. The place where dropping Win32 types genuinely pays off is the
scheduler (§6.5), which is where the 32-bit assumption actually lives.

### 6.2 Build system

- `CMakeLists.txt` becomes the source-of-truth file list; `Game.vcxproj` keeps
  its own copy. Add a small script (run in CI) that diffs the two `.cpp` lists
  so drift is caught instead of discovered.
- Options: `RE1_PLATFORM=win32|linux`, `RE1_RENDERER=d3d11|gl`,
  `RE1_AUDIO=xaudio2|sdl`, `RE1_VIDEO=mci|ffmpeg`.
- Windows keeps building through `Game.vcxproj`/`build.bat` exactly as today;
  CMake's Windows path is for parity checks only.
- Linux flags: `-m32` (first cut), `-fomit-frame-pointer` **mandatory** for the
  scheduler TU, no LTO.

### 6.3 Phases

Each phase ends in a gate that must pass before the next starts. The Windows
build must stay green at every gate — for phases 0-3 that *is* the regression
test.

| # | Phase | Deliverable | Gate |
|---|---|---|---|
| 0 | **Platform extraction** *(done)* | `src/platform/{types.h,platform.h,win32/}`; move every Win32 call and handle out of `src/game/` | `src/game/` has no `<windows.h>` and no Win32 API call (CI grep); Windows build behaves identically |
| 1 | Build skeleton + shim *(done)* | Linux `types.h` path, ucontext scheduler, per-symbol state reset, platform file/memory calls | **72/72 shared TUs compile on Linux x86-32**; Windows Release + Debug green; boundary check green |
| 2 | Window + GL lifecycle *(done)* | SDL2 window, GL context, `Clear`/`Present`, `main.cpp` pump + 33 ms limiter | a cleared window at 30 Hz; scheduler runs for minutes without stack corruption |
| 3 | 2D + textures *(done)* | texture create/update/destroy/white/font, `DrawSprite`/`DrawRect`/`DrawLine`, samplers, blends, path layer | **title screen renders**; glyphs pixel-crisp |
| 4 | 3D *(done)* | `DrawTrianglesPersp`, `DrawTriangles3D`, depth test/write, perspective-correct UV | **in-game room renders**; models correct; depth ordering correct |
| 5 | Input *(keyboard done)* | SDL keyboard + gamepad → `keyMap` / `joysticks[0]` masks | playable title → gameplay on keyboard (pad path unverified — no hardware) |
| 6 | Audio *(implemented)* | SDL2 software mixer behind the `DirectSound` API; shared wrappers split into `src/game/SoundApi.cpp` | banks load, device opens at 22050 Hz; **audible confirmation pending** |
| 7 | FMV *(done)* | ffmpeg (or vendored Cinepak) behind the decoder interface | intro FMV plays, skips, and audio stays in sync |
| 8 | Platform misc *(done)* | config store, path/case resolver, crash handler, dialogs, single-instance, screenshots | runs from an asset tree; save/load round-trips; `crash.log` on fault |
| 9 | Parity + hardening | side-by-side playtest, ASan/valgrind pass | your sign-off |
| 10 | Portability hooks *(later)* | `uintptr_t` audit, pointer-in-DWORD removal, GLES context path | Android/Switch evaluation can begin |

### 6.4 Phase 0 detail — extracting the platform from `src/game/`

The boundary does not have to be guessed: the Win32 surface inside `src/game/`
is already enumerated (§2, §4). Only **~11 files** carry real API calls; three
more include `<windows.h>` for types alone.

| Source in `src/game/` | Windows thing | Moves to | Risk |
|---|---|---|---|
| `InputSystem.cpp`, `DebugMenu.cpp`, `DebugScreens.cpp`, `OptionsMenu.cpp`, `MainMenu.cpp` | `GetAsyncKeyState` (~59 sites) | `platform/*/input.cpp` behind `plat_key_down(vk)` | low — mechanical |
| `Rendering.cpp` | `timeGetTime` | `plat_time.cpp` behind `plat_time_ms()` | low |
| `SaveLoadScreen.cpp` | `CreateDirectoryA`, `SECURITY_ATTRIBUTES` | `plat_file.cpp` behind `plat_mkdir()` | low |
| `SoundSystem.cpp` | `waveOut*` device probe | audio backend | low |
| all path literals | backslashes, case | path layer + resolver | medium (110 sites) |
| `Types.h`, `FileLoader.h`, `SpriteRenderer.h`, `SoundTables.cpp`, `GteMatrix.cpp` | `<windows.h>` include | `platform/types.h` | low |
| `Globals.h` | `HWND`/`HINSTANCE`/`HANDLE` globals | opaque `PlatWindow`/`PlatHandle` | medium |
| `TaskScheduler.cpp` | naked asm + `VirtualAlloc`/`VirtualProtect` | `platform/*/plat_sched_*.cpp` | **high** |

Sequencing, lowest risk first, each step independently playtested on Windows:
keyboard polling → timing → filesystem → the type headers → the path layer →
the handle globals → the scheduler. The scheduler goes last because it is the
only item whose failure mode is intermittent stack corruption.

**Enforcement matters more than the move.** `tests/check_platform_boundary.py`
greps `src/game/**` for Windows headers and live Win32 calls (comments exempt,
so the original-binary documentation survives) and runs as a CI step in
`.github/workflows/release.yml`. Without it the boundary erodes within a few
commits.

**Status: implemented and playtested.** New files
`src/platform/{types.h,platform.h}` and `src/platform/win32/platform.cpp`;
the entry point and window procedure moved to
`src/platform/win32/{main,window_proc}.cpp` (pure file move — `src/` root
now holds only `Globals.cpp`); `src/game/` has no Windows headers and no live
Win32 calls; Release and Debug both build. What moved:

| Interface | Replaced | Call sites |
|---|---|---|
| `plat_key_state` / `plat_key_flush` | `GetAsyncKeyState` | 53 in 5 files |
| `plat_time_ms` | `timeGetTime` | 1 (`Rendering.cpp`) |
| `plat_mkdir` | `CreateDirectoryA` | 1 (`SaveLoadScreen.cpp`) |
| `plat_debug_output` / `plat_env_get` / `plat_is_debugger_present` | `OutputDebugStringA` / `GetEnvironmentVariableA` / `IsDebuggerPresent` | `DebugPrint.h` (39 macro uses unchanged) |
| `plat_audio_probe_and_cache_volume` / `plat_audio_restore_volume` | `waveOut*` device walk | 2 (`SoundSystem.cpp`) |
| `plat_alloc_guarded_stacks` / `plat_fatal` | `VirtualAlloc`/`VirtualProtect`/`MessageBoxA`/`ExitProcess` | `TaskScheduler.cpp` |

`types.h` is included by `src/game/Types.h`, `FileLoader.h`, `SpriteRenderer.h`,
`SoundTables.cpp` and `GteMatrix.cpp`; on Windows it is a thin wrapper over
`<windows.h>`, so the MSVC build sees the same types it always did.

**Tradeoff to be aware of:** this is a refactor of release-grade code with no
behavioural test harness, done *before* the Linux side can validate it. That is
why the table is ordered by risk and why the gate is "Windows behaves
identically", not "it compiles". If you would rather not touch working code
blind, the alternative is to extract *on demand* — each phase of §6.3 pulls out
only what it needs — at the cost of touching the same files twice.

### 6.5 Phase 1 detail

**Status: implemented.** Gate met — **72/72 shared TUs compile on Linux
x86-32**, and Windows Release + Debug both build. What was done:

1. `types.h` grew the full shim: `DWORD`/`BYTE`/`BOOL`/`HWND`/`HANDLE`/`RECT`/
   `POINT`/`WPARAM`/`LPARAM`/`LRESULT`/`LPCSTR`/`JOYINFOEX`/`__int8..64`/
   `VK_*`/`WM_*`/`MB_*`/`LOWORD`/`MAKEWORD`/`_stricmp`/`_s` CRT shims, and the
   calling-convention keywords (no-ops off Windows).
2. The eight `src/marni/*.h` headers plus `Types.h`/`FileLoader.h`/
   `SpriteRenderer.h`/`SoundTables.cpp`/`GteMatrix.cpp` include `platform/types.h`
   instead of `<windows.h>`. Two Windows-only escapes remain, both guarded:
   `MarniInput.h` (JOYINFOEX) and `MarniSound.h` (the `PlaySound`→`PlaySoundA`
   macro, which would otherwise rename the class method).
3. `TaskScheduler.cpp` split into shared public API + three per-platform switch
   primitives (`TaskSwitch_Start`/`TaskSwitch_Resume`/`TaskYield`). MSVC keeps
   the original naked asm; everything else uses `makecontext`/`swapcontext` on
   the same guard-paged stacks. `g_TasksEIP`/`g_TasksESP`/`g_SchedulerESP` are
   now `void*`/`uintptr_t`, so the 32-bit pointer assumption is gone.
4. The `.gwipe`/`.sched`/`.items` sections and their `__declspec(allocate)`
   are deleted; `ResetGameStateBlock()` (`GameStart.cpp`) clears the 19 wiped
   globals by name in original address order. No linker ordering is required.
5. Compile gate: `tests/compile_linux.sh` (g++ -m32, compile-only).

**Deferred to Phase 2:** `CMakeLists.txt`. A CMake target that cannot link is
not a useful gate, so the file list was the compile script's `SRC` for now;
CMake arrived with the GL backend and is verified in §6.6.

**Two behaviour-affecting fixes found on the way** (flagged for playtest):

- `MainLoop.cpp` carried a file-local `static int g_pressF9Flag` that shadowed
  the real global — `window_proc.cpp`'s clear of it did nothing. The shadow is
  removed; both now use `Globals.cpp`'s single variable, matching the original
  binary's one variable at 0x004ba718.
- `Rendering.cpp` carried a `static GetTextureVariant` duplicating the shared
  one (0x0046d950 vs 0x0046d940); the duplicate is gone. Bodies were identical.

Everything else was portability-neutral: `<cstdlib>` includes MSVC had been
getting through `<windows.h>`, `goto` hoists GCC requires (CmdFunctions,
MainMenu, MainLoop), `%Iu`→`%zu`, and platform calls for `VirtualQuery`,
`CreateFileA`/`ReadFile`, `DestroyWindow`, `ShowCursor` and `timeGetTime`.

### 6.6 Phase 2-3 detail

**Status: implemented and verified.** The first Linux executable exists:
`build/linux/residentevil`, ELF 32-bit, linked against SDL2 + GL.

| File | Role |
|---|---|
| `src/platform/linux/platform.cpp` | all 16 `plat_*` functions (SDL2 keys/time/window/cursor/dialog, mmap guard stacks, `/proc/self/maps` readback, stdio files) |
| `src/platform/linux/main.cpp` | SDL2 window + GL context, config.ini read, 33 ms limiter, scheduler smoke tasks |
| `src/platform/linux/stubs.cpp` | inert placeholders for the not-yet-ported subsystems (P5/P6/P7/P8) |
| `src/marni/MarniDX_GL.cpp` | the GL backend: lifecycle, Clear/Present, textures, 2D quad/triangle path |
| `src/marni/MarniGLFuncs.{h,cpp}` | hand-rolled GL loader (no glad/GLEW dependency) |
| `CMakeLists.txt` | Linux target + explicit source list |

Build, verified from a clean tree in WSL2 Ubuntu-24.04:

```
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j8
```

User-verified on screen: a window with three colour bars and a moving white
square, ESC to quit.

Measured on WSL2 Ubuntu-24.04 / llvmpipe: GL 4.5 core context, 1024×768
backbuffer, **30-31 fps** against the 33 ms limiter, and both smoke tasks
increment at exactly their configured rates (task A sleeps 15 frames → 2/s;
task B sleeps 7 → 4.3/s), which is the scheduler-resume proof the gate asks for.

A 95-second soak held 31 fps throughout with **linear, monotonic counters**
(taskA 0→197, taskB 0→421 — exactly the configured rates) and exited cleanly:
no corruption, drift or freeze on the guard-paged task stacks.

Deliberately **not** in this phase: `DrawTrianglesPersp`/`DrawTriangles3D`
route to the affine 2D path (real depth and perspective-correct interpolation
belong with the TMD work in Phase 4), and `ReadTextureRGBA` returns FALSE until
the framebuffer-object round trip lands.

**Phase 3 — the real game boots.** `main.cpp` now mirrors
`RunMessageLoop`: `TaskScheduler_Init()` → `InitializeMarniSystem()` →
`main_loop()` at the 33 ms tick with the same skip-frame arithmetic. Two
supporting pieces landed with it:

- **Path layer** (pulled forward from Phase 8): `plat_normalize_path()` converts
  `\` to `/` and resolves each *existing* path component case-insensitively
  (cached), because the game's literals are NTFS-shaped (`data\fontus.tim`)
  while the asset tree is `assets/USA/Data/fontus.tim`. `AssetPath.h` gains a
  non-Windows branch rooting at `./assets/USA|JPN/`.
- **Frame capture**: `--capture <file> [frames]` dumps the back buffer, and
  `tests/rgba_to_png.py` converts it to PNG (zlib only — no ImageMagick/PIL).

Verified: the title screen renders — background image, logo, "PRESS ANY BUTTON"
and the Capcom copyright line, all through the shared 2D path. One observation:
`CMarniBits::CopyFrom` prints `invalid source` three times at boot. That is a
pre-existing `printf` (invisible under Windows' `/SUBSYSTEM:WINDOWS`) and the
render is correct, so it is not a port regression — but the Linux build makes
these previously-invisible diagnostics visible, which is a testing benefit worth
keeping in mind.

**Not yet interactive:** input is stubbed (Phase 5), so "PRESS ANY BUTTON" does
not advance. Testing the Phase 4 3D path needs input to reach a room, so Phase 5
should come before or with it.

**Phase 5 — input.** `src/platform/linux/input.cpp` implements
`CMarniDirectInput::UpdateKeyboardInputState/UpdateAllInputStates/InitJoysticks`,
`MarniPadIsConnected()` and `MarniXInput::IsConnected()`, reproducing the mask
contract bit for bit (bits 0-3 axis, 4-7 POV unused, 8-19 buttons) and the same
`prev`/`curr`/`newPress` transitions, so the game cannot tell the backend apart.
There is no WinMM on Linux, so the device sweep is SDL's game-controller API;
a pad plugged in mid-session is picked up by a one-second throttled rescan.

One design change was forced by testing: `plat_key_state()` no longer reads
`SDL_GetKeyboardState()`. SDL only updates that array from events its *backend*
delivers, so a synthetic `SDL_PushEvent` (the `--press` hook) never appeared
there. Key state is now owned by the platform layer and fed by
`plat_key_event(scancode, down)` from the window loop — real and synthetic
events take the same path, which is also what makes the hook a faithful proxy
for a human pressing the key.

Verified end-to-end with `--press`: title screen → NEW GAME/LOAD GAME menu →
the Load Game list, driven entirely by synthetic key events through the real
`JoyToPSX` remap. The pad path is implemented but **unverified — no controller
available in WSL**; the user should confirm it with real hardware.

**Phase 4 — 3D.** `DrawTrianglesPersp` and `DrawTriangles3D` are now real:
both take the 10-float `{x, y, z, w, u, v, r, g, b, a}` layout, run a
perspective vertex shader (premultiply by `w`, clamped at `1e-4` like the
Windows VS, so the rasteriser's divide restores the screen position while
UV/colour interpolate with 1/w), and differ only in depth state — 3D writes
(`LESS_EQUAL`, `depthWrite` honoured for translucent water/glass), Persp tests
only. GL's NDC z is [-1,1] and the callers pass [0,1], so the shader maps
`z*2-1`. The internal vertex stride went 8 → 10 floats with a scratch expansion
buffer, so `DrawSprite`/`DrawRect`/`DrawLine`/`DrawTriangles` are unchanged.

Verified: the mansion hall renders with correct depth ordering (arches,
pillars, staircase, chandelier, stained glass, floor tiles) and Jill's model,
stable through frame 1500.

**Phase 6 — audio.** `src/platform/linux/audio.cpp` implements the
`DirectSound` class over one SDL2 stream with a software mixer in the callback
(XAudio2 mixes per-bank voices in hardware; SDL gives one stream). The contract
is preserved: banks 1-80, `PlaySound(bank, slot)` loops when `slot != 0`,
volume is DirectSound millibels mapped through `10^(mB/2000)`, `GetStatus`
reports 1 while playing.

The platform-neutral half of `MarniSound.cpp` — the `g_pDirectSound` wrappers,
`UpdateSoundFade`, the async callbacks, `SndCompactCallback`, the SFX filename
tables and `findAndOpenFile` — moved to a new shared `src/game/SoundApi.cpp`,
because both backends need it and it only ever calls the class API. The Windows
file keeps just the XAudio2 engine.

**Fixed 2026-09-09: banks never reached the configured asset root.** The
XAudio2 backend remaps a WAV path through `ResolveAssetRoot` before opening it;
the SDL2 backend passed the compile-time `GAME_DATA_ROOT` form straight to
`plat_file_read_all`, so with `[Assets] Path` set (the repository's
`../../assets`) every bank was looked for under the *build* directory's own
`./assets/USA/` and silently failed. The symptom is distinctive: FMV audio plays
(it resolves in `VideoPlayback.cpp`) while gameplay is silent. `CreateSound` and
the shared `findAndOpenFile` — which gates `load_voice` — now both resolve, and a
missing bank names itself on stderr (first four, then suppressed).

`InitializeSoundSystem` is normally reached from `WM_CREATE`; with no window
procedure on Linux, `main.cpp` calls `StartSoundSystemAsync(g_hWnd)`
explicitly (without it `g_pDirectSound` stays NULL and every bank load reports
"NOT READY").

**Open (Phase 6): a rare audio-path crash.** Roughly 2 runs in 13 died with a
SIGSEGV on the SDL audio thread under WSLg's Pulse backend; `SDL_AUDIODRIVER=dummy`
and `=alsa` were clean over 150 s each, and Pulse itself was clean in 6 of 8
runs. The only captured backtrace belonged to a mixer overflow that is now
fixed, so the current fault is uncharacterised. Two later repro attempts were
killed by host memory pressure before producing output. Not yet explained —
needs a real-Linux check before it can be called environmental. Audible output
also still needs the user's ear.

**Two defects surfaced by this phase:**

1. **A latent buffer overflow the Linux layout exposed.** `LoadEntityModel`
   points `g_loadDataDestPointer` at `g_entityModelBuffer` (52224 B) but
   `char10.emd` is 106224 B. The original's two buffers are exactly adjacent
   (`0x00bf11c0 + 0xCC00 == 0x00bfddc0`, totalling 108544 B) precisely so the
   player model spans both; MSVC kept that adjacency and absorbed the spill,
   GCC reordered them and the spill landed on `ENTITY` — a segfault at room
   start. They are now members of one struct (`EntityModelStorage`) exposed as
   array references, which guarantees contiguity on every compiler without
   touching a single call site.
2. **`DrawTriangles3D`'s stride was documented as 9 floats.** It is 10 —
   `TmdRenderer`'s `TMD_VERT_FLOATS` is 10 and the Windows backend memcpy's
   `sizeof(Model3DVertex)`. The header comment is corrected.

**Playtest report #1 (2026-09-08): three symptoms, two root causes.**

The report was "win ver: controls moving automatically, cannot control in-game;
linux ver: message glyphs corrupted, item name missing from the inventory, and a
3D model that is invisible from any side but the front". Two defects, found with
the scripted-input hooks (`RE1_TEST_KEYS` on Windows, `--press` on Linux) plus
the `--capture` readback:

3. **The player model spills out of its buffer — and the spill landed on the
   input state (Windows) or on `s_assetIsJpn` (Linux).** `LoadEntityModel`
   reads the whole player EMD into `g_entityModelBuffer`, i.e. the
   108544-byte pair. `char11.emd` (Jill) is **112968 bytes**, `char12.emd`
   112620 and the costume variants up to 112664, so up to 4424 bytes run past
   the pair. In the original binary that tail lands in the animation buffer and
   is harmless; in this decompilation the linker placed `g_pMasterInputState`
   immediately after the pair on Windows, so loading Jill overwrote `keyMap`
   and the joystick entries with model bytes — `joysticks[0].enabled` became
   nonzero garbage, `ReadPadBoth` merged a phantom pad word and the character
   walked by itself while the keyboard was dead. On Linux the same spill hit
   `s_assetIsJpn`, which `GetAssetVersion()` reads: the game switched to the
   JPN font/name tables while running USA assets, which is exactly the
   corrupted message glyphs and the missing item name. `EntityModelStorage`
   gained a 16 KB `spillGuard` tail that absorbs every such spill; the model's
   own byte offsets inside the load region are unchanged, so nothing else
   moves. Verified: after loading the Jill save, `keyMap` stays `26 28 25 27`,
   no phantom pad, movement and turning respond, and the inventory renders
   "COMBAT KNIFE" and the message glyphs correctly.
4. **`glClear(GL_DEPTH_BUFFER_BIT)` was masked by the depth write mask.** The
   2D path leaves `glDepthMask(GL_FALSE)`, and OpenGL honours the mask for
   `glClear` — so the depth buffer was never cleared and every frame's 3D pass
   tested against the *previous* frame's depths. Static geometry survived
   (identical depths frame to frame), which is why it went unnoticed; the item
   examine screen, whose model rotates, lost everything but a few fragments
   that happened to be no farther than last frame's surface. `MarniDX::Clear`
   now forces `glDepthMask(GL_TRUE)` first. D3D11 has no equivalent trap —
   `ClearDepthStencilView` ignores the depth-write state — so this was purely
   a backend divergence.

Both fixes are behaviour-preserving for the original: (3) keeps the spill
inside the port's own storage instead of a neighbour, (4) makes the depth
buffer clear actually happen, which is what the D3D11 backend already did.

**Playtest report #2 (2026-09-08): "the linux ver only displays a black
screen".** The binary ran and the window opened, but every asset load failed
(`[PSXTexture::Store] Invalid magic: 0x00000000`) because the process had been
started from `build/linux/` — where the binary lives — and every path in the
port is relative (`GAME_DATA_ROOT` is `./assets/USA/`, `config.ini` is opened as
`./config.ini`). `main.cpp` briefly resolved the game root by walking up
from `/proc/self/exe` until it found `config.ini` + `assets/` and `chdir`-ing
there. **Superseded** by the explicit path scheme below, which removed the
guesswork entirely.

**Explicit asset/save paths (2026-09-09).** The game no longer searches for its
data. `config.ini` names it:

```ini
[Assets]
Path=            ; folder holding USA/ and JPN/; empty = the binary's folder
Version=USA
[Save]
Path=            ; empty = <Assets Path>/SAVE (case-resolved against the fs)
```

Relative paths resolve against the **executable's directory** (`plat_exe_dir`,
`/proc/self/exe` on Linux, `GetModuleFileNameA` on Windows), never the working
directory, so a shortcut or launcher with any CWD behaves identically.
`config.ini` itself is read from the executable's directory first, falling back
to the old working-directory candidates. Both keys are user-owned: the exit-time
rewrite never touches them.

Mechanically, `SetAssetBase()` stores the folder and `SetAssetVersion()`
composes `<base>/<USA|JPN>/` into the runtime root `ResolveAssetRoot` rewrites
paths to; with no base configured the compile-time roots stay in force, which is
what keeps the character-index-patched templates (`g_bgPathTemplate`,
`g_maskPathTemplate`) valid — they are compiled against the compile-time root
and remapped at open time, so a root of any length works. The save folder
became `GetSaveRoot()` for the same reason (`GAME_SAVE_ROOT` is only the
fallback now). The repository's own `config.ini` therefore carries
`Path=../../assets`, because the binary lives in `build/linux`.

**Phase 7 — FMV (implemented).** Windows handed the AVI to MCI, which painted
the movie into the game window itself; there is no equivalent service on Linux,
so the decoder moved behind a small backend interface and the 4-state machine
became shared code.

| File | Role |
|---|---|
| `src/video/VideoPlayback.cpp` | shared state machine: the FMV table + per-movie skip masks, the `g_videoSkipCounter` grace period, the prologue scenario cut, `SetVideoResolution`, the software-renderer queue |
| `src/platform/win32/video.cpp` | MCI (`avivideo`) backend: open/play/play-from/stop/close, the `MM_MCINOTIFY` event, the adapter 5/7 window-style dance |
| `src/platform/linux/video.cpp` | ffmpeg backend: libavformat/libavcodec decode the Cinepak stream, swscale converts to RGBA, the frame is uploaded to a MarniDX texture and drawn as a full-screen quad |
| `src/platform/linux/audio.cpp` | `plat_audio_stream_*`: the movie's audio, pre-decoded to S16 stereo at the device rate, mixed alongside the banks and used as the video clock |

The audio clock is the interesting part. A movie's audio is decoded up front
into one buffer and handed to the mixer, whose play cursor advances with the
sound card; `plat_video_tick()` turns that cursor into a frame index
(`seconds * fps`) and decodes forward to it. The picture therefore cannot drift
from the sound, and the cut points stay frame-exact. `PauseGameSoundsAsync`
stops the game's own banks (it calls `setSndStop`, not `DirectSound::Release`),
so the stream mixes alone.

Two traps found while building it, both of which cost a "the movie ends
instantly" or "the movie hangs" afternoon:

- **The audio clock is only trustworthy if the device actually consumes in real
  time.** A headless ALSA/Pulse fallback discards its output and calls the
  mixer callback as fast as the CPU allows - measured here at ~500k callbacks/s,
  which swallowed a 7.5 s track in under a millisecond and fast-forwarded the
  movie to its end on the first tick. `plat_video_tick()` now accepts the audio
  cursor only while it stays within half a second of the wall clock, and falls
  back to the wall clock otherwise.
- **ffmpeg's send/receive handshake must be drained exactly once.** A decoder
  whose input queue is full returns EAGAIN from `send_packet` and wants a
  `receive` first; after the file ends the NULL-packet flush must go in once.
  The first version spun forever about one run in four.

Gate (verified with `--capture` + `--press`): the Capcom and opening movies
play full-screen at the right speed, `--press` skips them on the skip mask, and
the title screen comes up afterwards.

**Playtest report #3 (2026-09-09): the Jill prologue cut went black.** "It
skips the Chris section at 02:58, but the screen goes black while the sound
carries on from 03:08." The audio was right and the picture was wrong, which
pointed straight at the seek: `SeekTo` labelled the first frame after
`av_seek_frame` as index 0 and then counted decodes forward to the target. A
keyframe seek lands *before* the target, so the count ran past it - at frame
1885 it decoded 1885 more frames from 1877, i.e. past the 2259-frame end of the
file, leaving nothing to draw. `video.cpp` now takes the frame
number from each frame's PTS (`pts * fps`), so a seek lands exactly on the
requested frame; verified with a standalone probe against `PU.avi`: seeking to
1885 lands on index 1885 after 9 decodes. The audio path was always correct
because it seeks by sample count.

That left the picture frozen on the resumed segment's first frame: frame
numbers are absolute but the tick computed its target from the *segment*
start, so it never asked for a frame past 1885. The target is now
`s_baseFrame + seconds * fps`; the probe walks 1885 -> 1905 over two seconds
and stops cleanly at the 2259-frame end. User-confirmed 2026-09-09: the Jill
prologue cut plays through with picture and sound.

**Playtest report #4 (2026-09-09): no 2D effect sprite rendered in gameplay.**
Blood, water splashes, muzzle flashes and the room's esp effects were missing
entirely; everything else - rooms, models, shadows, masks - was correct.

The effect sprites were being *drawn* the whole time, just 2-6 pixels across
and in the wrong place. The cause was a layout assumption the Windows build
happens to satisfy and the Linux one does not:

`ProjectEffectSprite` (0x0040aa50) hands `MulMatrixVec3` the address of
`g_fixedPointPipe_matrix_m00` and indexes it as `m[0..8]` - the original's nine
matrix globals are contiguous at 0x004c3790..0x004c37b0 and that array
indexing is how it addresses them. The port declared them (and
`matrix_t0/1/2`) as twelve separate globals. MSVC emits them in declaration
order, so Windows was accidentally right. GCC's top-level reordering put them
in `.bss` in **reverse** - `m22` at the lowest address - so only `m[0]` was
real: every other row term read the neighbouring variable. Effects projected
with a garbage view Z (a sprite at view Z 16 876 came out at 144 508, i.e. 1/8
scale, or off-screen), while the 3D path - which uses its own float matrix at
`objData+0x08` - stayed perfect.

The fix declares the storage as one array with `#define` aliases for the
original names (`Globals.h`, `GteMatrix.cpp`), so the layout is guaranteed on
both toolchains. Verified by logging `ProjectEffectSprite`'s raw `v`/`out`/`t`
on both builds with identical input: before the fix Linux returned
`out=(146,-2100,-600)` where Windows returned `out=(815,116,246)`; after it
they are byte-identical, and the attract reel that carries the water-splash
sprites (pdemo1) now draws them at full size.

This is the same failure class as the item viewer's rotation block
([[byte-offset-indexed-globals-need-one-array]]): whenever the original
addresses a global block by index through `&DAT_x`, the port must declare the
whole span as one array.

**Playtest report #5 (2026-09-09): the first zombie met you in the corridor.**
On a new Jill game the tea room's first zombie stood in the middle of the
corridor view instead of lying by Kenneth's body, while the Windows build was
correct. Another shared-code bug that only the Linux address layout exposes:

`cmd_enemy_set` (0x004617d0) at **0x004619e4** is `MOV EAX, ds:0x4d4540` - the
**value** of `g_scaDataTable[0]`, the Chris SCA record. The port stored the
array's **address**. Entities whose own init overrides `Sca_info` (zombies via
`g_pZombieScaInfo[0]`, the player via `SetupCharacterData`) never noticed;
static models never override it, so Kenneth's corpse (entity id 37) kept the
table address - and `ResolveEntityScaCollision` reads the collision radius at
`Sca_info + 10`, i.e. the high word of a pointer: 0x004d = 77 in the original
and 0x0082 = 130 in the Windows port (both harmless), **0x5675 = 22133** in the
Linux binary. The resulting penetration (21946) made the corpse-vs-zombie push
(21621, -3603): the zombie teleported ~22 000 units east on its first update
frame.

Found by logging every `ResolveEntityScaCollision` push above 2000 units with
both entities' `Sca_info`/`pSca_hit_data` and radii (the garbage radius names
the entity at once), then bisecting inside `zombie_update` - after the state
handler, after `SetEntityScaHitData`, after `ResolveEntityScaCollision`, after
`HandleEnemyPlayerCollisions`, after `check_room_collision`. Fixed by storing
`g_scaDataTable[0]`; the zombie now stays at its scripted (4150, 6300) and the
same push is ~230 units. `GameStart.cpp`'s placeholder store was corrected the
same way (it is overwritten by `SetupCharacterData`, but it was the same slip).

`tools/dump_init_scd.py` was added while chasing this: it decodes the RDT's
initialization SCD at header +0x60 (the script that spawns a room's enemies),
which `mine_room_scd.py` does not cover.

**Phase 8 — platform misc (implemented).** Settings, single-instance and crash
reporting are real now, and the settings store is shared:

| File | Provides |
|---|---|
| `src/system/ConfigFile.cpp` | `config.ini` for **both** builds: creates it with commented defaults when missing, loads `[Display]`/`[Player]`/`[Input]`/`[Assets]`/`[Debug]`, and rewrites only the keys it owns on exit - comments, unknown keys and section order survive |
| `src/platform/linux/config.cpp` | `plat_single_instance_check` (flock on `re1.lock`) + `CleanupVideoConfigAndSaveAllSettings` |
| `src/platform/linux/crash.cpp` | `crashlog_install` / `crashlog_mark` - fatal-signal handler writing a symbolized trace to `crash.log` |

The registry write is gone on Windows; it is still *read* once at startup so an
install that predates the switch keeps its bindings, and `config.ini` then
overrides it. Settings therefore travel between the two builds. A live
`config.ini` is no longer tracked in git (it is runtime state now), so the
repository carries `config.ini.template` and both release packages copy it in
as the initial `config.ini`; the game rewrites it on exit, and still creates
one from its compiled defaults if the file is missing. `re1.lock` is gitignored
too.

The crash handler is the Linux counterpart of `system/CrashLog.cpp`: SIGSEGV,
SIGABRT, SIGBUS, SIGILL and SIGFPE append a `backtrace_symbols_fd` trace and
re-raise with the default disposition, so the exit status and any core dump
still behave normally. `-rdynamic` is on the link line so the trace names
functions instead of printing bare addresses.

Verified: a hand-written `config.ini` is rewritten with only the owned keys
touched (comments and `[Assets]`/`[Debug]` intact), the rewrite is idempotent
across runs, and an edited `PlayCount` round-trips; a second instance exits 3
with a message; a SIGSEGV injected into a running game produced a symbolized
`crash.log`.

**Phase 9 — parity + hardening (in progress).** The sanitizer pass is the
automated half; the side-by-side playtest is the other.

Build with `-fsanitize=address,undefined` and run the attract demo through it
(`--press 40 400 3 --capture ... 3000`, ~100 s of title + demo + room loads).
`-fno-sanitize=alignment` is deliberate: the PS1 data layouts are full of
intentionally unaligned DWORD accesses (e.g. the 223-byte descriptor stride),
which x86 tolerates and the original does too - they are a known item for the
ARM/Android target, not a bug to chase here. ASan warns once that it does not
fully support `makecontext`/`swapcontext`; the task stacks are guard-paged, so
a real overflow still faults.

What the first pass found and what was done about it:

| Finding | Verdict |
|---|---|
| `SetupTexturePageHandles` (0x0046d0e0) wrote the legacy descriptor tables **unguarded** - the startup slot-15 write landed 2321 bytes past `g_TexturePageTable_DAT`, in `g_imageBufferDataB` | real OOB write; guarded like its three siblings (`LoadTexturePage`, `ProcessTextureImage`, `LoadImage`) |
| `CreateTexturedQuad` (0x0046fb50) read the page table and the per-slot arrays out of range | real OOB; guarded |
| `title_setup_texture_pages` (0x00470970) read/wrote the same tables at `slot * 0x37C` with `slot` = camera index (0..7) | real OOB; guarded |
| `g_textureBankRedirect[23]`, but `TmdProcessingCallback` writes `pageCount` entries from `g_TextureBankID` and the callers leave it unmasked (banks up to 31) | **real OOB write** into `g_asyncTmdDataPtr`; the original's array is 32 entries (0x00aae2b0..0x00aae330, exactly 32 dwords to the next global). Resized to 32 |
| `SetGlobalScaledRotationMatrix`, `CalculateAngleBetweenPointsXZ`, the SCD bit test: `<<` on negative values | UB only; rewritten as `* 4` / `* 4096` / an unsigned mask. Needed for a non-x86 target |
| **heap-use-after-free**: `ProcessTextureImage` passes a *local* `PSXTexture` to `create_texture_page`, which aliases it into `g_MarniBitsWorkBuffer` (`CMarniBits::CopyFrom` copies pointers, not data) and defers the read to the async worker - so the worker reads 8KB of freed heap on every font-page creation | real UAF; `create_texture_page` now snapshots the pixel and palette data into a persistent buffer it owns. The original aliases safely because its callers pass the slot's stored descriptor |
| `CreateTmdObjectInternal` (0x00483910) indexes `g_tmdObjectSlotAnimPtrs` with the animation object's own `+8` field, which a fresh object has not written yet - the read landed 4 bytes before the table | faithful to the original, but an OOB read; range-checked |

After the fixes the same run - title, FMV skip, attract demo, room loads, entity
and effect updates, 3000 frames - reports **zero** UBSan diagnostics and no ASan
errors.

The alignment class is left alone by design. The rare SIGSEGV seen in about 2
of 13 Release runs has not reproduced under the sanitized build, but the
use-after-free above is a plausible cause - it read freed heap on every font
page creation - and the `g_textureBankRedirect` overrun wrote into
`g_asyncTmdDataPtr`, so the hardening pass may already have removed it. That
needs a longer Release soak to confirm.

### 6.7 How testing works

There is no automated behavioural test today, and the port replaces the
renderer, so "it compiles" proves little. Four tiers, cheapest first:

**Tier 1 — unit tests for deterministic subsystems** (no window, no GPU).
These are already pure functions with known-good behaviour, and most of them are
where the project's documented traps live:

| Target | What it pins |
|---|---|
| `GteMatrix.cpp` | PS1 GTE matrix/trig emulation — 14-bit tables, fully deterministic |
| `PSXTexture::Store` | TIM/PIX parsing, CLUT field order, `m_NumCLUTs` |
| `MarniBits` pixel ops | SetColor/GetColor/CLUT nearest-match, BMP round-trip |
| `BioCard` | byte-exact 0x41C save-block round-trip |
| `ResolveAssetRoot` | idempotence (double-apply was a real bug) |
| `DsVolumeToAmplitude` | the documented volume curve |
| Cinepak/AVI parser | decode a fixture to a known frame hash |

Harness: `tests/` with a tiny assert macro and a `ctest` target — no dependency,
consistent with the project's style (Catch2/doctest are fine if you prefer).
`tests/check_platform_boundary.py` is the first inhabitant. `tools/` keeps its
decompilation tooling and the verifiers that test tools living there.

**Tier 2 — golden-frame image tests** (GPU, no input).
Add a debug-only hook (`--capture-frame N --room X`) that renders a deterministic
scene and writes a PNG plus a hash. Compare Linux against Windows
*differentially*, or against checked-in goldens. Caveat: rasterization rounding
differs across GPUs/drivers, so exact byte equality is not realistic
cross-vendor — use a tolerance compare (≥99.9% of pixels within ±2/255) and keep
goldens per-GPU-vendor. This is the tier that catches the documented invariants
(red-in-lowest-byte, UV, depth sort, sampler choice) long before a playtest.

**Tier 3 — deterministic input replay** (the game, no wall clock).
Record `(frame, pad word)` sequences and replay with the frame limiter disabled,
so the loop is driven by tick count rather than `timeGetTime`. Prerequisites: a
fixed-tick mode, audio/FMV disabled or stubbed, and a nondeterminism audit
(`rand()` seeding, uninitialized reads, float variance). Gate: the same input
file produces the same frame-hash sequence on Windows and Linux. This tests
*gameplay logic*, not just rendering — the strongest check short of playing.

**Tier 4 — playtesting** (you). Enemy AI feel, door timing, audio mixing and
dynamic parity still rest on you running both builds. That is how the rest of
this project has been validated, and it remains the final gate.

**Two safety nets that are not tests but catch whole bug classes:**
- **Sanitizers** — ASan/UBSan on the Linux build. The code was written against
  MSVC semantics; this is where latent UB surfaces. MSan for uninitialized reads.
- **Parser fuzzing** — mutate RDT/SCD/TIM/EMD inputs through the existing
  parsers (libFuzzer/AFL). The Python tools already parse these formats, so
  deriving fixtures is cheap.
- **Compile-time layout checks** — the 65 existing `static_assert`s already test
  struct layout; keep them, and add a startup assert that the wipe range covers
  exactly the intended globals.

### 6.8 Rough effort

Estimates only, relative to each other (this is a ~12k-line backend, not a
rewrite):

| Phase | Relative size | Main unknown |
|---|---|---|
| 0 platform extraction | medium | refactoring release-grade code with no test net |
| 1 skeleton + shim | small | whether the shim is really enough |
| 2 window + GL | small | scheduler behaviour under a different compiler |
| 3 2D + textures | medium | sampler/blend parity, font crispness |
| 4 3D | **large** | depth sort + projection parity (the documented traps) |
| 5 input | small | mask/scancode mapping |
| 6 audio | medium | voice/loop semantics without XAudio2 |
| 7 FMV | medium | decoder choice + A/V clock |
| 8 platform misc | medium | case-folding paths |
| 9 parity | open-ended | parity bugs only a playtest finds |

---

## 7. Decisions

Resolved for maximum future portability. Ordered by how much each helps:

1. **Renderer: OpenGL (ES-3-compatible subset), not Vulkan** — *decided*.
   Switch homebrew
   has **no Vulkan** — it offers `switch-mesa` (OpenGL 4.3 core, plus ES 2.x/3.x)
   or `deko3d`, a non-Vulkan low-level API. Vulkan would buy Android and lose
   Switch. Everything this game needs (quads, alpha/add blend, a depth buffer,
   point/linear sampling, scissor, 320×240 → screen scale) is GLES 2.0/3.0-era
   functionality, so writing the backend against GLES-3 semantics and mapping
   to desktop GL 3.3 core costs nothing and covers desktop + Android + Switch.
2. **Window/input/audio: SDL2.** `switch-sdl2` is a devkitPro portlib; SDL2 is
   the native Android path; it is the same API on desktop. Raw X11/Wayland is
   desktop-only and would have to be thrown away.
3. **FMV: a vendored Cinepak decoder (+ a small AVI chunk parser).** The assets
   are exclusively Cinepak, which is a compact, well-specified codec (~1k lines
   of dependency-free C). `switch-ffmpeg` does exist as a portlib, so ffmpeg is
   not a dead end — but it is a heavy dependency and a build-system liability on
   every target. ffmpeg is the faster route to correctness; vendoring is the
   portability-optimal one. Put either behind a small decoder interface.

   *Blocked on a decision (Phase 7).* The build host has no ffmpeg at all, so
   the choice is concrete: install
   `libavcodec-dev:i386 libavformat-dev:i386 libswscale-dev:i386` (correct and
   testable immediately; `switch-ffmpeg` covers the Switch target later), or
   vendor a Cinepak decoder (dependency-free, but written without a reference
   decoder to test against — only plausibility checks, then visual judgement).
   Either way the 4-state machine, skip masks and `g_videoSkipCounter` grace
   period are preserved and the decoder sits behind an interface.
4. **Memory layout: remove the `.gwipe` range dependency** (per-symbol clears)
   — *decided*. Keeping the ordering would mean a linker script for *each*
   toolchain (MSVC, GNU ld, Android NDK lld, devkitA64). Removing it makes the
   layout toolchain-agnostic and deletes the ordering risk outright. See §6.5.

### The bigger constraint: this codebase is 32-bit-native by design

The graphics choice is *not* the main obstacle to Android/Switch. Two deeper
assumptions are:

- **Pointers are assumed to be 32 bits.** `static_assert`s pin the sizes of
  structs containing pointers (they mirror the original 32-bit binary's layout),
  and code stores pointers in DWORDs (`g_TasksEIP[id] = (DWORD)func`,
  `TaskScheduler.cpp:197`; `(DWORD)g_scaChrisData`, `Globals.cpp:1229`).
  **Switch homebrew is aarch64-only**, so a Switch port is a 64-bit port: every
  pointer-bearing struct changes size and those asserts must be revisited.
  Android is more forgiving — armeabi-v7a (32-bit ARM) still runs on 64-bit
  devices and Play's 64-bit rule applies to store distribution, which a
  decompilation project is unlikely to use — so Android can plausibly stay
  32-bit.
- **The task scheduler is x86 inline assembly.** ESP/EIP switching has no ARM
  equivalent. A portable rewrite (`ucontext`/`swapcontext`, or a coroutine
  refactor) is required for *any* ARM target, 32- or 64-bit.

Also note `#pragma pack(1)` structs holding pointers/DWORDs are a strict-
alignment hazard on ARMv7 (ARM64 tolerates most unaligned access). Both targets
are little-endian, so byte order is not a concern.

**Implication:** if Android/Switch are real goals, do the scheduler and layout
work once, portably, inside the Linux port — a `ucontext`-based scheduler, no
pointer-in-DWORD, per-symbol clears — and the Linux port becomes the template
rather than a throwaway.
