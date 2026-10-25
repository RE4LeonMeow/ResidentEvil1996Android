# PS1 Engine & File-Format Analysis (SLUS_001.70 vs PC)

Analysis of the 1997 PlayStation 1 Resident Evil 1 build against this PC
decompilation, produced from the Ghidra programs `SLUS_001.70` (PS1, USA) and
`ResidentEvil.exe` (PC, USA), the assets under `assets/PSX/` and `assets/USA/`,
and `tools/psx_overlay.py` (Capstone-based PS-EX/overlay inspector).

> Addresses on the PS1 side are MIPS virtual addresses (`0x80......`);
> addresses on the PC side are the ones already commented in `src/`.

---

## 1. Sources

| Program / path | What it is |
|---|---|
| `SLUS_001.70` (Ghidra) | PS1 main executable — the resident kernel + engine |
| `assets/PSX/PROG2/*.EXE` | 12 overlay PS-EX executables swapped in on demand |
| `assets/PSX/` | PS1 asset tree extracted from the disc |
| `ResidentEvil.exe` (Ghidra), `src/` | PC build and this decompilation |
| `assets/USA/` | PC asset tree |

The PS1 disc is the USA release (`BASLUS-00170`); the executable self-identifies
as `BASLUS-001700`.

---

## 2. Architecture at a glance

```
   PS1 (2 MB RAM)                              PC (single process)
   ──────────────────────────────────────      ────────────────────────────────
   SLUS_001.70                                 ResidentEvil.exe (one image)
     ├─ resident engine       0x80011xxx..       ├─ game logic (same code)
     ├─ PSYQ libs (libgs,     0x8005f8a0..       │  reached via main()/WinMain
     │  libgpu, libcd, libspu, 0x800aefff)       ├─ Marni System (PSYQ emulation
     │  libpad, libetc, BIOS)                     │  over DirectX 5) + MSVC CRT
     └─ PROG2 overlays (on demand)               └─ no overlays, no swapping
         0x800e0000  LOGO PROLOGUE TITLE
                     SELECT ENDING
         0x80105400  STAGE1..STAGE7
```

- **PS1:** only the core engine is resident. Everything stage- or screen-specific
  lives in an overlay loaded into one of two fixed regions and executed as a
  task. This is forced by the 2 MB RAM budget.
- **PC:** the whole program is a single `ResidentEvil.exe`; every overlay body is
  linked into it. The PSYQ runtime the PS1 game ran on is replaced by Capcom's
  **Marni System** (a DirectX 5 wrapper) plus the MSVC CRT, so the game code
  itself could be reused with minimal change.

The game logic (task scheduler, entities, SCD opcodes, inventory, camera zones,
RDT handling, item/door logic) is the **same** code on both platforms — the PC
port is the PS1 game running on a compatibility layer. The differences are in
*packaging*, *platform services* and a small number of *file formats*.

---

## 3. PS1 memory map (from the Ghidra memory blocks)

| Range | Block | Meaning |
|---|---|---|
| `0x1f800000`–`0x1f8003ff` | `CACHE` | GTE/scratchpad (1 KB) |
| `0x1f801000`–`0x1f801fff` | `MCTRL1`..`CDROM_REGS` | Hardware registers (DMA, timers, GPU, MDEC, CD, SPU) |
| `0x1f801c00`–`0x1f801dbf` | `SPU_*` | SPU voices / control |
| `0x20000000`–`0x20000263` | `GTEMAC` | GTE register map (asm wrappers `gte_*`) |
| `0x80000000`–`0x8000ffff` | `RAM` | PS1 kernel/BIOS sys area (`SUM Error!!%x:%x`, `BASLUS-00170*`) |
| `0x80010000`–`0x800af000` | `CODE` | Main PS-EX text (651264 bytes) |
| `0x800af000`–`0x801fffff` | `RAM` | Engine data/BSS + the two overlay regions |
| `0x800e0000`–`0x800e6000` | overlay region A | LOGO/PROLOGUE/TITLE/SELECT/ENDING |
| `0x80105400`–`0x80133400` | overlay region B | STAGE1..STAGE7 |

Main PS-EX header: load `0x80010000`, text size `0x9f000`, entry `0x8005f8a0`
(PSYQ `start`), game `main` at `0x80031954`, stack `0x801ffff0`.

The two overlay regions are visible in Ghidra as the overlay address space
`OVR_LOGO` (the LOGO overlay is imported there) plus ordinary `ram` blocks.

---

## 4. The overlay system

### 4.1 Overlay inventory (`python tools/psx_overlay.py info`)

| Overlay | File size | Entry | Load | Text size | Load range |
|---|---:|---|---:|---:|---|
| `LOGO` | 6144 | `800e08a8` | `800e0000` | 4096 | `800e0000`–`800e1000` |
| `PROLOGUE` | 4096 | `800e02f4` | `800e0000` | 2048 | `800e0000`–`800e0800` |
| `TITLE` | 8192 | `800e11a8` | `800e0000` | 6144 | `800e0000`–`800e1800` |
| `SELECT` | 24576 | `800e51ac` | `800e0000` | 22528 | `800e0000`–`800e5800` |
| `ENDING` | 26624 | `800e5cf8` | `800e0000` | 24576 | `800e0000`–`800e6000` |
| `STAGE1` | 122880 | `80121edc` | `80105400` | 120832 | `80105400`–`80122c00` |
| `STAGE2` | 120832 | `8012173c` | `80105400` | 118784 | `80105400`–`80122400` |
| `STAGE3` | 190464 | `80132718` | `80105400` | 188416 | `80105400`–`80133400` |
| `STAGE4` | 192512 | `80133124` | `80105400` | 190464 | `80105400`–`80133c00` |
| `STAGE5` | 169984 | `8012dad4` | `80105400` | 167936 | `80105400`–`8012e400` |
| `STAGE6` | 151552 | `80128ec0` | `80105400` | 149504 | `80105400`–`80129c00` |
| `STAGE7` | 163840 | `8012bdd4` | `80105400` | 161792 | `80105400`–`8012cc00` |

Region A (`0x800e0000`, 24 KB) is the boot/menu swap slot; region B
(`0x80105400`, the "stage slot") hosts the per-stage logic. Only one overlay per
region is resident at a time.

### 4.2 Overlay file format

Every `PROG2/*.EXE` is a plain **PS-EX** file (magic `PS-X EXE`, 0x800-byte
header followed by the text). The game does **not** parse the header as a
whole; it is read into a buffer and the loader treats `buffer + 0x10` as its
overlay header, i.e. exactly the PS-EX fields after the 16-byte magic prologue:

| Off in file | Off in game header | Field |
|---:|---:|---|
| `0x00` | – | `PS-X EXE` magic |
| `0x10` | `+0x00` | initial PC = **overlay entry point** |
| `0x14` | `+0x04` | initial GP |
| `0x18` | `+0x08` | **text load address (destAddress)** |
| `0x1c` | `+0x0c` | text size |
| `0x20` | `+0x10` | memory-fill address (unused: 0) |
| `0x24` | `+0x14` | memory-fill size |
| `0x30` | `+0x20` | stack base (`0x801ffff0`) |

So `g_OverlayHeader = g_OverlayBuffer + 0x10` and is 60 (`0x3c`) bytes. This was
verified against `LoadStageOverlay` (0x80036ba8), where the header is copied to
`g_TaskTable[thread].overlayHeader`, the destination is read at `+8` and the
entry at `+0`.

### 4.3 Loader

`LoadStageOverlay(threadId, fileIndex)` — 0x80036ba8:

1. Reads the first `0x800` bytes of the file into `g_OverlayBuffer`
   (`0x801d08ec`).
2. Copies the 60-byte header (`g_OverlayHeader`, `0x801d08fc`) into the target
   task's `overlayHeader`.
3. Re-reads the file body (`size - 0x800`) at the header's `destAddress`.
4. Runs it with `Task_execute(threadId, entryPoint)`, restoring the task's GP.

`LoadAndScheduleOverlay(id)` — 0x80036d10 does the same but `Task_chain`s the
overlay entry into the running task (used to replace a task in place).

### 4.4 The file-system table

The engine has its own directory, **`g_FileSystemTable`** at `0x8009166c`,
built at boot from the CD's ISO-9660 directory. Each entry is 12 bytes:

| Offset | Type | Field |
|---:|---|---|
| `+0x00` | u16 | CD sector (LBA); the loader bumps it by 1 to skip the 0x800 header |
| `+0x04` | u32 | file size in bytes |
| `+0x08` | u32 | 16-bit checksum (`CheckFileChecksum`, retried by `LoadFileFromCD`) |

Entries are **fixed, index-ordered** and match an alphabetical directory list —
index 0 = `BIO.TIM` (8768 B), 1 = `BIO_CARD.DAT` (1052 B), 2 = `BT367.TIM`
(30913 B), 3 = `CAPCOM.PTC` (548352 B)… Overlays are addressed by absolute
index (e.g. `ScheduleLogoOverlay` loads index `0x14a` = 330 = `LOGO.EXE`).
`LoadStageOverlay`/`LoadAndScheduleOverlay` read and restore the size/LBA
fields around the two-part read.

`LoadFileFromCD` (0x8002b37c) drives `libcd` streaming (`CD_LoadFileByIndex`,
`CdReadSync`, retry loop, checksum) — this is the function the PC port replaces
with normal file I/O in `FileLoader.cpp`.

### 4.5 Overlay/engine contract — callback registration

Stage overlays are **plugins**. `STAGE1`'s entry (`0x80121edc`) is a stub that
jumps to `0x80105534`, which does nothing but write the overlay's room-specific
functions into resident engine globals and then call one:

```
0x8010555c  store 0x8010eebc  -> [0x800caa18]
0x8010556c  store 0x80117560  -> [0x800caab8]
0x8010557c  store 0x8011d460  -> [0x800caa78]
0x8010558c  store 0x80121a44  -> [0x800caaa0]
0x8010554c  store 0x80122100  -> [0x801fec1c]   (engine callbacks)
0x8010559c  store 0x801221c0  -> [0x801fec04]
0x801055ac  store 0x80121eec  -> [0x801fec08]
0x801055bc  store 0x80122018  -> [0x801fec00]
0x801055d8  store [0x801fe908] -> [0x801fec18]
0x801055e0  jalr  [0x801fe908]                   (invoke resident entry)
```

So the resident engine owns the frame/task machinery and dispatch tables; each
overlay supplies the stage's room-action/SCD command handlers and its own
per-frame logic. On the PC these are simply part of the monolithic EXE, called
directly instead of through overlay-registered pointers.

### 4.6 What each stage overlay contains

Every `STAGE1..7.EXE` is a self-contained stage plugin. Its entry stub jumps to
a registration routine that (a) installs the stage's callback pointers into
resident tables and (b) patches resident state pointers, after which the resident
engine drives everything through those pointers. The overlay itself then holds
the stage's game code plus its per-room data tables.

**Hardware/engine globals the registration sets:**

| Global | Meaning |
|---|---|
| `0x800caa18 + slot*4` | resident **behavior/handler table** (~58 slots). Each stage fills only the slots it uses (the others stay 0); the entries are dispatchers keyed on the resident state byte `0x800c51ab`, which jump through a per-overlay jump table (e.g. STAGE1 uses `0x801054f0`) |
| `0x801fec00` | per-room **background checksum table** (read by `FUN_80014e4c`) |
| `0x801fec04` | per-room **sound/BGM config table** (read by `AudioLoadBank4` / `FUN_80051e24` = `update_room_bgm`) |
| `0x801fec08` | per-room **CD-streaming table** (read by `FUN_80014ee8`, which computes a CD LBA and calls `InitCDStreamingMode`) |
| `0x801fec1c` | per-room stage table (read via a computed base, so it has no direct xref) |
| `0x801fec18` | set to the constant `0x80200000` (empty-list sentinel; read by `update_entities`/`InitGameTask`) |
| `0x800cab14`, `0x800cab1c`, `0x800c5158`/`515c`/`5160`/`5164`, `0x800c51a8`, `0x800c3000` | stage-init scratch/pointer globals |

**Per overlay** (text size and function count from a prologue scan; callbacks =
`0x800caa18` array slots it fills):

| Overlay | Stage | Area (rooms) | Text size | ~functions | Callback slots filled | BSS |
|---|---|---|---:|---:|---|---|
| `STAGE1` | 0 | Mansion 1F (`1xx`) | 120832 | ~207 | `aa18`, `aa78`, `aab8`, `aaa0` | 29 |
| `STAGE2` | 1 | Mansion 2F (`2xx`) | 118784 | ~113 | `aa18`, `aa78`, `aae4` | 29 |
| `STAGE3` | 2 | Courtyard / underground (`3xx`) | 188416 | ~245 | `aab8`, `aac8`, `aa94`, `aae0` | 18 |
| `STAGE4` | 3 | Guardhouse (`4xx`) | 190464 | ~248 | `aa18`, `aa34`, `aa84`, `aad0`, `aaa0`, `aa90`, `aadc` | 18 |
| `STAGE5` | 4 | Laboratory (`5xx`) | 167936 | ~202 | `aa18`, `aa88`, `aa94`, `aae0` | 22 |
| `STAGE6` | 5 | Mansion return 1F (`6xx`) | 149504 | ~227 | `aa18`, `aa78`, `aac8` | 0 |
| `STAGE7` | 6 | Mansion return 2F (`7xx`) | 161792 | ~186 | `aa18`, `aa78`, `aac8`, `aae4` | 0 |

Stage ids/names are the engine's `STAGE_*` constants (`src/game/Types.h`);
`STAGE6/7` are the "mansion return" variants. They ship **no `.BSS`** because
`load_room_bg_image` indexes the background table with `g_StageId % 5`, so
STAGE6 (`5%5=0`) reuses STAGE1's mansion backgrounds and STAGE7 (`6%5=1`) reuses
STAGE2's — confirmed by the asset tree (BSS only for stages 1–5).

So in short: each stage overlay supplies **(1)** the stage-specific per-frame
state machine and room/enemy/object handlers (its own ~113–248 functions),
**(2)** the stage's room tables (BG checksums, sound config, CD streaming), and
**(3)** the jump tables the registered dispatchers use — the resident engine
provides all shared systems and calls back into the overlay through the
installed pointers.

---

## 5. Resident engine module map

The main binary is the same subsystem set documented for the PC in
`docs/ARCHITECTURE.md`, minus Marni. Named ranges (PS1 addresses):

| PS1 range | Subsystem | PC counterpart |
|---|---|---|
| `80011xxx`–`80014d94` | room/model/texture load + checksums | `TextureLoader.cpp`, `EntityModelLoader.cpp` |
| `8001482c`–`800156d0` | CD streaming (`InitCD`, `CD_LoadFileByIndex`, `CdDriveStateHandler`, `UpdateCDStreamingState`) | `FileLoader.cpp` (+ Marni file I/O) |
| `80015984`–`800176d4` | palette animation, camera background upload | `Rendering.cpp` |
| `80017760`–`80017da0` | `cut_set`, `SetActiveCamera`, `LoadRoomBackground`, `CheckCameraSwitch`, `DisplayRoomCameraBg`, `CheckPointInQuad` | `Room.cpp`, `RoomInit.cpp` |
| `8001b884`–`8001b9b8` | `ScreenShake`, `UpdateScreenWithBorders` | `Rendering.cpp` |
| `800226f4`–`80023d68` | effect billboards (`Effect_CreateBillboard`, `update_2d_effects`, `EffectBillboard_UpdateAndRender`, `memclr`) | `EffectSystem.cpp` |
| `80024480`–`80026534` | `LoadTMD`, `LoadWeaponModel`, `UpdateEntities`, `RoomEventsCheck` | `TmdRenderer.cpp`, `ObjectManager.cpp`, `RoomEvents.cpp` |
| `80027adc`–`8002a0c8` | fading, `StartGame`, `game_init`, `GameLoop` | `FadeSprite.cpp`, `GameStart.cpp`, `GameLoop.cpp` |
| `8002ae68`–`8002b4a8` | geometry task, room lights, player data, `LoadFileFromCD`, `LoadFile` | `GameStart.cpp`, `FileLoader.cpp` |
| `8002ce40`–`8002d3cc` | inventory (`setInitialItems`, `rearrangeItemSlots`, `LoadInventorySprites`, `GetItemSlot`) | `MenuData.cpp` |
| `8002fe80`–`800310a4` | entity skeleton/anim (`UpdateEntityAnimation`, `UpdateEntitySkeleton`, `UpdateJointHierarchy`, `RenderEntityPrimitives`) | `TmdAnimation.cpp` |
| `80031954`–`800327b8` | `main`, `Boot_system`, `InitSystem`, `InitHW`, `ScheduleLogoOverlay`, `ScheduleTitleOverlay` | `MainLoop.cpp`, `LogosScreen.cpp`, `TitleScreen.cpp` |
| `80034ad0`–`800365a8` | items, text (`useCurrentItem`, `printText`, `displayMessage`, `DisplayFoundItemMenu`) | `PrintText.cpp`, `MenuData.cpp` |
| `80036950`–`80037020` | task scheduler + overlay loaders + `PlayFMV` | `TaskScheduler.cpp`, `LogosScreen.cpp` |
| `80038be4`–`80039fd4` | room object/entity update + render | `Room.cpp`, `Rendering.cpp` |
| `8003a6a8`–`8003b038` | pad thresholds, input state, player anim | `InputSystem.cpp`, `PlayerAnimations.cpp` |
| `80045064`–`80048f18` | SCD command handlers (`cmd_*`), flags, `RunCmdFunction` | `CmdFunctions.cpp` |
| `8004905c`–`8004be30` | interactive screen, save/load, menus | `InteractiveScreen.cpp`, `SaveLoadScreen.cpp`, `MainMenu.cpp` |
| `8004f8f0`–`8004f910` | image buffers / VRAM image load | `Rendering.cpp` |
| `800511b0`–`80058db0` | sound system (`AudioSystemInit`, `AudioLoadBank*`, `Sound_SetFade`, voice channels) | `SoundSystem.cpp` |
| `8005a90c`–`8005bbf0` | model relocate, textures, FMV/MDEC (`PlayMovieByPath`, `DecodeFMVFrame`, `UpdateVideoPlayback`, `PlayXATrack`, `DecodeStillImage`) | `VideoPlayback` / Marni FMV |
| `8005c2f0`–`8005d7d4` | globals reset, effect joints | `GameStart.cpp`, `EffectSystem.cpp` |
| `8005f898`–`8005f8a0` | `__main`, `start` | MSVC CRT entry |
| `8005f94c`–`800aefff` | PSYQ libraries (`GsSort*`, `DrawOTag`, `CdControl`, `Spu*`, `Ss*`, `Pad*`, kernel `sys.c`/`bios.c`/`intr.c`) | Marni System + Win32 |

PSYQ/BIOS strings (`$Id: sys.c`, `$Id: bios.c`, `$Id: intr.c`, `GS`/`CD`/`SPU`
error strings) confirm the tail of the image is the stock PSYQ runtime, not game
code — this is the region Capcom replaced with Marni on PC.

---

## 6. Boot / frame flow compared

| PS1 | PC |
|---|---|
| `start` (`8005f8a0`) → `main` (`80031954`) | `WinMain` → `main_loop` |
| `main` → `Boot_system`/`InitSystem`/`InitHW` (PSYQ init: `ResetGraph`, `SpuInit`, `CdInit`, `PadInit`) | `InitializeGame`/Marni init (D3D11, XAudio2, XInput) |
| `ScheduleLogoOverlay` loads font + menu textures, then overlay index `0x14a` via `LoadAndScheduleOverlay` | `logos_state` → `title_state` tasks in the same EXE |
| `StartGame` → `game_init` → `GameLoop` (`8002a0c8`) | `game_start` → `game_loop` (`0x00480b30`) |
| Stage overlay `STAGE1..7` swapped into `0x80105400` when a stage starts | stage code always resident |

### GameLoop comparison

`GameLoop` (PS1 `0x8002a0c8`) and `game_loop` (PC `0x00480b30`) are the same
function. The first frame of the PS1 body maps 1:1 onto the PC source:

| PS1 | PC |
|---|---|
| `g_fade_type_id = 2` (`DAT_800cf86f`) | `g_fade_type_id = 2` |
| `Flg_ck(&g_gameOptionsFlags,0x7d)` | `Flg_ck(..., SCENARIO_FLAG_MENU_FADE_LATCH)` |
| `FadingUpdate()` | `fade_update()` |
| `RunCmdFunction(g_CmdOpcode); RoomEventsCheck(); room_state_reset(); DisplayInteractiveScreen();` | `run_command_functions(); room_events_check(); room_state_reset(); check_and_display_interactive_screen();` |
| `FUN_80022b64(); FUN_80037964();` | `update_2d_effects(); DrawRoomSpr();` |
| `FUN_8002b288()` | `StartAttractDemo()` |
| `FUN_80027b1c(2,0xC00)` / `(2,0x600)` | `set_fading(2, 0xC00)` / `(2, 0x600)` |
| `FUN_8002ab20()` / `FUN_8002ade0()` | `room_transition_load()` / `check_menus_state()` |
| `FUN_8002a940()` / `FUN_8002aa28()` | `TimeoutDeathFadeOut()` / `die_state()` |

Differences found:

- **Debug/decomp features are PC-only** and absent from the PS1 loop: F1 debug
  menu, texture viewer, quick-access load, room-change placement (all guarded by
  port-only flags in `GameLoop.cpp`).
- **Death sequence shape differs.** The PS1 loop drives death with a 2-state
  counter at `0x800c8454` (`0 → TimeoutDeathFadeOut, 1 → wait for fade → die`).
  The PC `game_loop` expands this to a 4-state machine (`DAT_00be9614`) that adds
  a 90-frame (`0x5A`) delay for ordinary rooms and immediate deaths in the three
  boss rooms. Room-specific behaviour can also live in the PS1 stage overlay, so
  this is an observed structural difference rather than a proven behaviour gap.
- **`room_state_reset` is smaller on PS1** (clears 2 fields at
  `0x800c300c`/`0x800c8670`); the PC port clears four (it also resets the two
  item-use flags — see the comment at `RoomInit.cpp:229`).

---

## 7. File formats: PS1 vs PC

Extension histogram of the shipped asset trees:

| PS1 (`assets/PSX`) | count | PC (`assets/USA`) | count | Notes |
|---|---:|---|---:|---|
| `.RDT` | 348 | `.RDT` | 348 | room definition; **format unchanged** (most files byte-identical) |
| `.BSS` | 116 | `.pak` | 1112 | per-room raw background vs per-camera LZW-compressed background |
| `.EMD` | 65 | `.emd` | 67 | enemy model — **same format** (identical files/sizes) |
| `.EMW` | 30 | `.EMW` | 32 | player/weapon model — **same format** |
| `.DOR` | 34 | `.DOR` | 34 | door animation scripts — **same format** (a few 4-byte deltas) |
| `.TIM`/`.PIX` | 28/56 | `.tim`/`.pix` | 84/28 | PS1 4/8/16-bit PSX textures; PC adds platform boot/font textures |
| `.IVM` | 75 | `.IVM` | 77 | item/inventory view images — same |
| `.HSB`/`.HED`/`.VB` | 57/35/35 | `.wav` | 1110 | PSYQ SEQ/VAB banks vs pre-rendered WAV |
| `.STR` | 25 | `.avi` | 27 | PS1 MDEC streaming video vs PC AVI |
| `.XAS` | 5 | `.wav` | — | PS1 CD-XA audio streams vs PC WAV |
| `.EXE` | 12 | — | — | PROG2 overlays — folded into the PC EXE |
| `.DAT` | 4 | `.dat` | 5 | bio card save + attract-demo data |
| `.STF` | 2 | `.avi` | — | staff-roll data vs pre-rendered AVI (`staf_r.avi`, `stfc_r.avi`) |
| `.ESP`/`.ETM` | 1/1 | `.ESP`/`.ETM` | 1/1 | shared effect sprite/object tables (unchanged) |
| — | — | `.SCD`,`.tmext`,`.tga`,`.ppm`,`.bak` | — | PC-only dev/build artifacts |

### 7.1 Room definitions (`.RDT`) — unchanged

PS1 and PC keep the **same `.RDT` format and file names**
(`ROOM1000.RDT` … ). Sampled rooms (`ROOM1000`, `ROOM2010`) hash **identically**
between the two trees; only individual rooms the port patched differ (e.g.
`ROOM1050.RDT`). The format is documented in `docs/RDT_FILE_FORMAT.md` and was
clearly derived from the PS1 original: 0x94-byte header, 19 relative pointers,
camera records, boundary/zone/SCD/message sections.

Consequence: any future PS1 feature porting can read the PSX `.RDT` with the
existing PC parser.

### 7.2 Backgrounds: `.BSS` → `.pak`

- **PS1:** one `ROOMxxx.BSS` per room (29 in stage 1 vs 58 RDTs — each room's
  two RDT variants share one BSS). The BSS is a flat concatenation of fixed
  **0x8000-byte (32 KB) blocks, one per camera**; the file size is always
  `cameras_count × 0x8000` (and equals the number of PC `.pak`s for that room).
  - Loader: `load_room_bg_image` (0x80017868). It looks the room up in a
    room→file-index table (`DAT_8009064c`, indexed by `stage%5` and `g_RoomId`),
    rewrites `g_FileSystemTable[index]` to `{size = 0x8000, lba +=
    g_ActiveCameraId*0x10}` (0x10 sectors × 2048 B = 0x8000), reads the block into
    `DAT_801f6900`, restores the table entry, then decodes it.
  - **Each block is an MDEC bitstream** for one 320×240 still. `DecodeStillImage`
    (0x8005bbf0) runs `DecDCTReset` → `DecDCTvlc` → `DecDCTin` → `DecDCTout` →
    `LoadImage`/`StoreImage`, and the call passes depth 0, so the output is
    **16-bit (RGB555)**, copied into `g_VramImageBuffer1` at VRAM `y = 0/240`
    (`GsGetActiveBuff` picks the page) — i.e. a double-buffered 320×240 16bpp
    image. All blocks share the same 8-byte prefix pattern
    `?? ?? 00 38 01 00 03 00` followed by the VLC data.
  - Integrity: the block has **no internal checksum**. `FUN_80014e4c` XORs one
    byte every 0x800 across the block and compares it to a table in the
    executable (`DAT_801fec00`, 8 bytes per room indexed by camera); a mismatch
    prints `SUM Error!!` and retries the CD read.
  - The per-camera **mask** overlay is *not* in the BSS — it lives in the RDT
    camera record (`cameras[i].mask_pointer`, see §7.1 / `RDT_FILE_FORMAT.md`).
- **PC:** one `RCxxxx.pak` per camera (157 in stage 1), produced by
  LZW-compressing each camera background and wrapping it as a TIM-like image.
  This is a **PC port conversion**, not a native PS1 format; the `.pak` viewer
  (`tools/pak_view.py`) decodes it. The port thus replaces the PS1
  MDEC-still path with an LZW+PSX-TIM path (the `DecodeStillImage` MDEC calls
  have no PC counterpart).

### 7.3 Textures

Same PS1 TIM/PIX concept on both sides (see `ARCHITECTURE.md` §Texture Loading).
The PC tree adds platform-specific images the PS1 never had — `3dfx.tim`,
`matrox.tim`, `rend.tim`, `virgin.tim`, `t_press.tim`/`t_start.tim`,
`fontus.tim` — because the PC port renders its own boot logos and a converted
font atlas.

### 7.4 Models

`.EMW` (player/weapon), `.EMD` (enemy), `.TMD` (room/prop models) are shared and
**byte-identical in the sampled files** (e.g. `W00.EMW` = 13692 B both sides,
`CHAR10.EMD` = 106224 B both sides). PS1 `.TMD` is the PSYQ `GsTMDdiv*` format;
the PC port keeps it and feeds it through Marni instead of `libgs`.

### 7.5 Sound

The PS1 side is Capcom's `libsd`/`libspu` bank system; the PC side is the same
bank/id layout pre-rendered to PCM WAV.

**PS1 formats (`assets/PSX/SOUND`, `assets/PSX/VOICE`):**

| File | What it is |
|---|---|
| `X.HED` (35) | PSYQ **VAB header** (`pBAV` magic) behind a small Capcom wrapper (the header starts at offset 64 or 128, e.g. `BIO.HED`@128, `WP001.HED`@64) |
| `X.VB` (35) | matching **VAB body** (raw 4-bit VAG ADPCM samples) |
| `SEPxx.HSB` (57) | a full **VAB** (`pBAV` at +0) with an appended **SSEQ sequence** (a few KB trailer) — the per-room BGM bank |
| `VOICE1..5.XAS` | **CD-XA ADPCM** voice streams (11–27 MB each; length is a multiple of the 2304-byte XA audio payload) |

**How the engine loads them** (`SsVab*`/`SsSeq*`/`SsSep*` are libsd):

- **Bank 0** `AudioLoadBank0` (0x80051534) — global SFX: `LoadFileFromCD(HED)`
  → `SsVabOpenHeadSticky` → `LoadFileFromCD(VB)` → `SsVabTransBody` →
  `SsVabTransCompleted`.
- **Bank 1** `LoadAudioSample` (0x80051680) — item/equipped sounds, same
  HED+VB pattern (called from `room_set` with `g_EquippedItemId`).
- **Bank 2** `AudioLoadBank2` (0x800517cc) — a VAB **embedded in the RDT**
  (`RDT->vb_file` header at +0x8C, `RDT->sound_banks_offset` body at +0x90).
- **Bank 3** `AudioLoadBank3` (0x800518bc) — weapon/door sounds (`WP001-00A`,
  `DOORxxx`), HED+VB.
- **Bank 4** `AudioLoadBank4` (0x80051a10) — the room **BGM** bank (`SEPxx.HSB`):
  opens the VAB and hands the appended SSEQ pointer to `AudioSepOpen`
  (`SsSepOpen(seq, vabId, 3)`), then `AudioSepReplay` starts the three SEP
  tracks (`SsSepReplay(sep, 0/1/2)`). The room→config byte table is
  `DAT_800cf870`; `FUN_80051e24` (the `update_room_bgm` equivalent) decides
  keep/reload/replay from the config's high bits.
- `.XAS` voices are streamed as CD-XA by the CD controller (`PlayXATrack`,
  `SsSetSerialVol` for the XA volume).

**PC:** no ADPCM/SEQ at runtime. Every bank was pre-rendered to
**RIFF WAVE, 22050 Hz mono** (8- or 16-bit, e.g. `bgm_00.wav` 8-bit,
`40s&w.wav` 16-bit) — `Sound/*.wav` (547: `bgm_XX`, `se_XX`, named SFX) +
`Voice/*.wav` (563), 1110 total — and is played through the Marni
`DirectSound` class (XAudio2 backend here). The bank/SFX *ids* and grouping are
preserved (`src/game/SFXIds.h`, `SoundSystem.cpp`). The RDT's sound blocks
(`.snd` attribute table, `.vh`/`.vb`) are dead on PC; the RDT `.vb` region is
even reused as scratch memory for room-object records (`RDT_FILE_FORMAT.md`).

### 7.5.1 Matching the PS1 and PC sound tables

The match is **inherent** — the PC port's `SoundTables.cpp` / `SFXIds.h` *are*
the PS1 table data, re-emitted at new addresses. The port preserved the ids, so
a PS1 sound id resolves to the same PC id and therefore to the same `.wav`
basename:

| PS1 data | PC table |
|---|---|
| per-stage voice clip offsets (bit 15 = 2-word entry, bits 0-14 = offset/16) | `g_StageVoiceOffsetTable[8]` |
| per-room voice name records (`"V001_00"`, 9 bytes) | `g_StageVoiceNamesTable[8]` |
| per-room enemy/ambient sound names (up to 48) | `g_RoomSoundNameTable[203]` |
| room → up to 4 BGM group ids | `g_BgmRoomData[7][32][4]` |
| BGM group → basename + loop flag | `g_BgmNameTable[57][4]`, `g_BgmLoopTable[57][4]` |
| SFX bank → wav names | `g_SoundBanksTable` (see `SFXIds.h`) |

**The PC releases already ship the converted audio.** The USA and JPN PC
releases contain the *same* WAV set — 547 `Sound` + 563 `Voice`, byte-identical
(sampled 200 of each; the JPN files just use uppercase names). So porting JPN
audio needs no PS1 decoding at all: reuse the WAVs.

**If PS1-original audio is still wanted** (e.g. to restore the PS1 mix, or for a
sound neither PC build has), the sources and conversions are:

| PS1 source | Codec | Conversion to PC format |
|---|---|---|
| `SOUND/*.VB` (+ `.HED` header), `*.HSB` | PSX SPU 4-bit ADPCM (VAG samples inside a VAB bank) | decode ADPCM → PCM → 22050 Hz mono WAV |
| `VOICE/*.XAS` | CD-XA ADPCM (2304-byte payload/sector) | decode XA → PCM → WAV |
| BGM (SSEQ sequence + VAB) | libsd SSEQ + SPU | needs an SPU/sequencer emulator; the PC `bgm_XX.wav` are pre-rendered |

Verified detail for a future extractor: the `.HED` trailer's **last 8 bytes hold
the VAB header offset** — the loader opens
`buffer + *(u32*)(buffer + size - 8)` (64 for `WP001`, 128 for `BIO`); the `.HSB`
files are full VABs (`pBAV` at +0). The remaining unknown is the VAB header's
internal VAG offset table (the Capcom `.HED` carries a much larger descriptor
block than a stock VAB), which must be decoded to get per-sample boundaries.
The `.VB` bodies themselves are plain SPU-ADPCM frame streams.

**Converter:** `tools/psx_audio_extract.py` implements the above. The SFX path
is validated and working:

```
python tools/psx_audio_extract.py vb assets/PSX/SOUND/WP002.VB -o out/ \
       --match assets/USA/Sound        # names outputs by correlation
```

- The `.VB` body is split into samples by the per-frame end flag; dropping the
  1-frame pad/separator segments reproduces the VAB header's `vs` count exactly
  (WP001 → 3, BIO → 6).
- `.HSB` (full VAB) bodies start at the offset stored in the file trailer
  (`*(u32*)(size - 0x0c)`); splitting from there and capping to the header's
  `vs` recovers exactly the declared VAGs (SEP00 → 15). These are the **VAB
  instrument samples the SSEQ BGM mixes** — not rendered music tracks like the
  PC `bgm_XX.wav`.
- `python tools/psx_audio_extract.py all` converts every `.VB` and `.HSB` in
  `assets/PSX/SOUND` and writes the WAVs into that same folder.
- **The playback rate is per-VAG.** `play_sfx` (0x80052570) reads the tone
  record at `vabPtr + prog*0x200 + tone*0x20 + 0x820` and passes note (+6) and
  fine (+5) to `SsUtKeyOn`; the record's +4 is the sample centre, so the SPU
  plays the VAG at `44100 · 2^((note − centre + fine/128)/12)`. That rate varies
  per bank (doors ≈ 21–22 kHz, `SELECT` ≈ 25–28 kHz, `CHAR00` ≈ 12.7 kHz,
  `BIO`/`EVIL` ≈ 33 kHz), which is why a single output rate was wrong. The `.VB`
  converter applies the per-VAG rate from the matching `.HED` (VAG v uses tone
  v); `.HSB` banks stay at 22050 Hz. `--flat-rate` forces a single `--rate`.
- `--match DIR` correlates each decoded sample against the PC `Sound/*.wav` and
  names the output after the match, producing drop-in replacements.

### 7.5.2 BGM sequences (`.HSB` → SEQ)

Each `SEPxx.HSB` packs a **PSYQ SEQ sequence** alongside the VAB: the trailer
words give the sequence offset (`*(u32*)(size-8)`) and the VAG body offset
(`*(u32*)(size-0xc)`), so the sequence is `data[seq:body]`. The SEQ is a
standard `pQES` stream — variable-length delta times then MIDI-like events
(`0x9n` note on, `0x8n` note off, `0xBn` control change, `0xCn` program change,
`0xEn` pitch bend, `0xFF` end) with running status. `tools/psx_bgm_extract.py`
extracts all 57 sequences to `.seq` and prints channels/programs/notes; the 57
banks match `g_BgmNameTable[57][4]` (the PC port holds one rendered `bgm_XX.wav`
per group).

**Rendering a SEQ to audio** is implemented in `tools/psx_bgm_render.py`. The
VAB layout was recovered from libsd's `SsVabOpenHeadWithMode` and `play_sfx`:

- `VabHdr` at +0 (ver@4, ps@0x12, ts@0x14, vs@0x16).
- `ProgAtr[128]` at +0x20 (16 B each): `[0]`=tone count, `[1]`=master volume.
- `ToneAtr` at `+0x820 + prog*0x200 + tone*0x20` (32 B): `[2]`=vol, `[3]`=pan,
  `[4]`=centre, `[5]`=fine, `[6]`=min note, `[7]`=max note, `[16]`=adsr1,
  `[18]`=adsr2, `[22]`=VAG index (1-based).
- VAG size table (u16 × vs, ×8 bytes) after the tone table; the VAG data lives
  at `*(u32*)(size-0x0c)`.

The renderer decodes each VAG to PCM at 44100, plays a note at
`2^((note − centre + fine/128)/12)` with a PSX-style ADSR, and mixes the
24-voice-polyphony timeline. All 57 BGMs are rendered to
`assets/PSX/SOUND/SEPxx.wav`.

Open calibration item: the SEQ tick length is not in the header fields we
decoded, so `--tick-rate` (ticks per second, default 60) is a parameter to be
tuned against a known in-game BGM.

**SEQ structure (from libsd `InitSoundSep`):** the file is multi-track —
`pQES` + 4 bytes, then tracks; track 0 has an 8-byte prefix, later tracks 2
bytes. Each track header is 11 bytes: time-base (u16 BE), tempo (u24 BE,
microseconds per beat), 2 bytes, event-data size (u32 BE), then the events.
`BPM = 60000000 / tempo` and **ticks/s = BPM · time-base / 60**. Each track is
a separate song (a BGM slot); `SEPxx` holds up to three, matching the up-to-four
names in `g_BgmNameTable[group]`. Rendered tracks are written as
`SEPxx_tN.wav` (97 non-empty tracks across the 57 banks); e.g. `SEP00_t0` is
62.1 s at 48 BPM, which matches the shipped `bgm_00.wav` (~60.6 s).

The **voice (`.XAS`) path is experimental**. The voice offset table
(`0x801fec08`, registered by the stage overlay) is walked by `FUN_80014ee8`:
it counts bit-15 entries and computes a CD sector range
`(value & 0x7fff) * 0x10 + extra + CD_GetFileLBA(stage_voice_file)`, then
`CdIntToPos` + `InitCDStreamingMode` (`SsSetSerialAttr(0,0,1)` + `CdControlB
0x0E` with the XA mode). So the voices are CD-XA streams, one track per stage
(`g_VoiceOffsetData_StageN` → `VOICE(N+1).XAS`).

Structural analysis of `VOICE1.XAS` confirms the group layout: the 16-byte
group header holds four 4-byte channel parameters at offsets 0/4/8/12, and the
**filter is the high nibble** (≤ 4 for 100% of them across the whole file — the
valid XA filter range) with the shift in the low nibble. The decoder uses that.

**Resolved (2026-09-26): the `.XAS` ARE the source of the PC voices.** The
earlier attempt read a 2048-byte extract as one stream, which cannot work: the
raw sectors' subheaders show `VOICE<n>.XAS` is **16 CD-XA channels interleaved
sector by sector** (file 1, channels 0..15 in turn, coding 0x00 = mono,
37.8 kHz, 4-bit). `VoicePlayClip` (0x800152a4) (SLUS_005.51; the voice start from
`cmd_voice_play`) walks the stage's offset table: clip *i* spans entries
`p`..`p+1`, in units of one 16-sector interleave cycle, and every bit-15 entry
closes a group - the walk skips its end word and the count of such entries is
**added to the sector**, i.e. it is the XA channel. So clip *i* of stage *s* is
channel `ch`, sectors `ch + 16*c` for `c` in `[start, end)` of `VOICE<s+1>.XAS`,
named `g_StageVoiceNamesTable[s][i]`. Decoded that way, 502 of 517 named clips
match the PC WAV at envelope correlation >= 0.9 (short clips at waveform NCC
0.999; long ones drift, because the PC resampled 37.8 kHz to 22.05 kHz). Two
table quirks: a name can sit at several ids with different audio (`V007_0d` is
ids 99 and 116; 116 is the PC's), and 15 PC files match no clip of their stage
at all. `tools/gen_ps1_audio_manifest.py` bakes the mapping into the asset
migrator, which converts them.

### 7.6 Video

- **PS1:** `.STR` MDEC streams, decoded with `DecodeFMVFrame`/`DecodeStillImage`
  and read straight off CD (`StopCDStreaming`/`ResumeCDStreaming`,
  `PlayXATrack`).
- **PC:** `.avi` (MCI/native player). The `_fmvPlay`/`UpdateVideoPlayback` state
  machine is shared; only the decode/streaming backend differs. `PJ.STR` is the
  large live-action intro; `STFC/STFJ` are the staff rolls, rendered to AVI on PC.

### 7.7 Save data

PS1 uses the memory card / engine "bio card" (`bio_card.dat`, 1052 bytes; the
PC reads the same file at `assets/USA/Data/bio_card.dat`). The 1052-byte layout
is shared (`docs/MEMORY_LAYOUT.md`, `BioCard.h`); only the transport differs
(memory-card sectors vs a file).

### 7.8 Text encoding and the font (PS1 vs USA/JPN)

The PS1 `DATA/FONT.TIM` is the JPN font *file* carrying the USA glyph *set*.

**Font file.** PS1 `FONT.TIM` and JPN `DATA/FONT.TIM` are byte-size identical
(99968 B) with identical TIM headers — 4bpp, 768×256 pixels (the width field is
in 16-bit words), CLUT at `(256,480)` 272×3. The USA build instead uses
`Data/fontus.tim` (256×256 4bpp, 32832 B) plus `font03t.tim`.

**Glyph content = USA.** The PS1 font holds the USA inventory: digits, `A–Z`,
`a–z`, the western punctuation set, the accented-Latin block (`Ä ä Ö ö Ü ü ß À à
Â â È è É é Ê ê Ï ï Î î Ô ô Ù ù Û û Ç ç`) and the 12 control symbols
`L2 R2 L1 R1 △ ○ × □ Up Right Down Left`. There are **no kana/kanji**: the RIGHT
page (the JPN kana region) is empty — 21 non-zero pixels in PS1 vs 46186 in JPN.

**Byte order = USA.** At every index where the USA and JPN tables disagree the
PS1 font matches USA:

| bytes | USA | JPN |
|---|---|---|
| `0x02–0x0B` | control symbols (►, L2/R2/L1/R1, △○×□▼) | blank |
| `0x17` / `0x18` | `;` / `,` | `、` / `。` |
| `0x37` / `0x39` | `(` / `)` | `[` / `]` |
| `0x3B` / `0x3C` | `–` / `·` | `ー` / `・` |
| `0x57`+ | accented Latin | kana |

**The decoded data proves it.** The engine's global message table is at
`0x8008f0f8` (64 pointers, reached from `set_message_display` 0x800359cc, the
`msgId & 0x40` branch). Decoding it with the **USA** table (`tools/decode_re1.py`)
reproduces the PC USA `STR()` strings verbatim, including the `0x02` newline
("►"), `0x08` = `○`, `0x0A` = `□`, and `0x3B` = `–` (`s_gm07`, `s_gm43`, …).
Roughly 30 of the 62 live messages contain at least one USA/JPN-distinguishing
byte, so a JPN decode would visibly break (drop the control glyphs, turn the
dashes into long-vowel marks, and read the accented block as kana).

**Shared structure.** All three builds use the same family of scheme: a plain
byte `0x0C–0xF7` selects grid cell `byte/18, byte%18`, and `0xF8 nn` selects a
14×14 page. JPN adds `0xF9 nn` / `0xFA nn` to reach the kana pages; PS1 never
uses them (its right page is empty). Room text confirms this independently: the
PS1 `.RDT` files are byte-identical to the USA ones.

**Practical consequence.** Decode PS1 text with `tools/decode_re1.py` (the USA
scheme). The JPN `JpnTextTables.cpp` / `jpn_msg_decode.py` do **not** apply. The
PS1 renders its text with 14×14 cells (JPN geometry) even though the codes are
USA, so glyphs are the USA shapes at the larger cell size.

---

## 8. Naming the PS1 program after the PC

### 8.1 The two naming vocabularies

`SLUS_001.70` has **1500 functions** (about 1043 named). The PS1 names were
assigned by an earlier PS1-side pass with its **own vocabulary**, which is
almost disjoint from the PC decomp:

| | PS1 | PC | in common |
|---|---:|---:|---:|
| Function names | 1488 | 2370 | **69** |
| Global names | 693 | 1103 | **13** |

The 69 shared function names are mostly the SCD `cmd_*` handlers, `Task_*`, and
the handful already renamed here; the 13 shared globals are
`g_CharacterId`, `g_DataOffset`, `g_CurrentTask`, `g_DataBuffer`,
`g_EnemiesList`, `g_EquippedItemId`, `g_PlayerHealth`, `g_PlayerLastSafeX/Z`,
`g_PlayerPosition`, plus the three renamed globals. Everything else differs in
both directions (e.g. PS1 `g_gameStateFlags` = PC `g_main_state_flags`; PS1
`CheckCameraSwitch` = PC `check_camera_switch`; PS1 `UpdateEntitySkeleton` =
PC `EntityComputeJointWorldMatrices`).

Because of this, no automated alignment works: the two builds are different
architectures (MIPS R3000 vs x86) so opcode/structural fuzzy matching returns
nothing (`find_similar_functions_fuzzy` on `game_loop` → 0 matches); call graphs
are sparse and dominated by indirect/task dispatch; and name/global token
matching only recovers case/underscore variants. Mapping is therefore done by
**behaviour and call-site**, using the PC source as the key.

(Note: the PS1 and PC Ghidra projects can be queried over the MCP server's REST
interface — `/list_functions?program=…`, `/list_globals`, `/get_full_call_graph`
— which is how the 69/13 overlap counts were measured and how the name lists in
this section were diffed.)

### 8.2 Method

1. Read the PC function's call-site context and body (`src/`).
2. Find the PS1 function at the same call site in the resident engine (the
   `game_loop` call sequence is a particularly strong anchor).
3. Confirm behaviour (constants, global addresses, callees) matches.
4. Rename in Ghidra and add a plate comment naming the PC equivalent and its
   address.

### 8.3 Pass 1 — naming previously-unnamed engine functions

| PS1 address | New name | PC equivalent |
|---|---|---|
| `0x80022b64` | `update_2d_effects` | `update_2d_effects` (0x0047c0c0) |
| `0x80037964` | `DrawRoomSpr` | `DrawRoomSpr` (0x00475b80) |
| `0x8005c380` | `room_state_reset` | `room_state_reset` (0x00475700) |
| `0x80044704` | `room_set` | `room_set` (0x00477720) |
| `0x800533b4` | `init_room` | `init_room` (RoomInit.cpp) |
| `0x8002616c` | `BuildEnemySnap` | `BuildEnemySnap` (0x0048f1a0) |
| `0x80034424` | `room_set_visited_flag` | `room_set_visited_flag` (0x00488570) |
| `0x80049044` | `lab_slides_reset` | `lab_slides_reset` (0x0042a020) |
| `0x8002b508` | `display_die_screen` | `display_die_screen` (0x00443090) |
| `0x8001b84c` | `door_system_start_animation` | `DoorAnimTask` spawn (0x00444770) |
| `0x8001b288` | `DoorAnimInit` | `DoorAnimInit` (0x004443c0) |
| `0x8001b5a4` | `DoorAnimLoop` | `DoorAnimLoop` (0x00444540) |
| `0x8001b534` | `DoorAnimTeardown` | `DoorAnimTeardown` (0x00444500) |
| `0x80048a58` | `Room_ApplySpriteFlags` | `Room_ApplySpriteFlags` (0x00432220) |
| `0x80052570` | `play_sfx` | `play_sfx` (SoundSystem.cpp) |
| `0x8002b288` | `StartAttractDemo` | `StartAttractDemo` (0x004818b0) |
| `0x8002a940` | `TimeoutDeathFadeOut` | `TimeoutDeathFadeOut` (0x00481250) |
| `0x8002aa28` | `die_state` | `die_state` (0x00481310) |
| `0x8002ab20` | `room_transition_load` | `room_transition_load` (0x004813c0) |
| `0x8002ade0` | `check_menus_state` | `check_menus_state` (0x004815f0) |
| `0x80027b1c` | `set_fading` | `set_fading` (0x0047b980) |
| `0x800cf86f` (global) | `g_fade_type_id` | `g_fade_type_id` (0x00bf0a2f) |
| `0x800cf638` (global) | `g_short_message_flags` | `g_short_message_flags` (0x00bebcc2) |
| `0x800c845b` (global) | `g_openMenuFlag` | `g_openMenuFlag` (0x00d22760) |

All renamed entries carry a plate comment with the PC address.

### 8.4 Pass 2 — correcting existing misnomers

The following functions already had names, but they were PS1-side guesses that
contradicted the behaviour or the PC name. Each was re-derived from the PC
source (call site + body) and renamed to the PC name.

| PS1 address | Old (misnomer) | New (PC name) |
|---|---|---|
| `0x8002a0c8` | `GameLoop` | `game_loop` |
| `0x80029bf0` | `StartGame` | `game_start` |
| `0x8002604c` | `UpdateEntities` | `update_entities` |
| `0x80026534` | `RoomEventsCheck` | `room_events_check` |
| `0x800398cc` | `UpdateSounds` | `update_room_objects` |
| `0x800310a4` | `RenderEntityPrimitives` | `render_entity` |
| `0x80030e2c` | `UpdateEntitySkeleton` | `EntityComputeJointWorldMatrices` |
| `0x80042754` | `ApplyPostAnimationJointRotation` | `EntityApplyLookAtRotation` |
| `0x8003b038` | `UpdatePlayerAnimation` | `update_player_anim` |
| `0x8003adf8` | `UpdateInputState` | `PlayerPad_Update` |
| `0x80048f18` | `RunCmdFunction` | `run_command_functions` |
| `0x8004905c` | `DisplayInteractiveScreen` | `check_and_display_interactive_screen` |
| `0x800177d8` | `SetActiveCamera` | `Room_SetupCamera` |
| `0x80017868` | `LoadRoomBackground` | `load_room_bg_image` |
| `0x80017c4c` | `CheckCameraSwitch` | `check_camera_switch` |
| `0x80017d34` | `DisplayRoomCameraBg` | `display_room_camera_bg` |
| `0x80022d58` | `EffectBillboard_UpdateAndRender` | `EffectActor_UpdateAndRender` |
| `0x80032738` | `UpdateDemoPlayback` | `UpdateDemoTimer` |
| `0x80036a8c` | `TaskScheduler` | `TaskScheduler_Update` |
| `0x80036ffc` | `Task_resume` | `Task_Resume` |
| `0x80038be4` | `CheckDeskState` | `check_desk_state` |
| `0x800390a8` | `UpdatePlayerPosition` | `update_player_position` |
| `0x8003942c` | `CheckItemboxState` | `check_itembox_state` |
| `0x80039594` | `CheckTypewriterState` | `check_typewriter_state` |
| `0x80039738` | `CheckEventItemUsage` | `check_event_item_usage` |
| `0x800397a0` | `RenderRoomObjects` | `render_room_objects` |
| `0x800357d8` | `printText` | `PrintText8x14` |
| `0x80045578` | `cmd_skip_2byte_opcode` | `cmd_skip_2bytes_opcode` |
| `0x8001b884` | `ScreenShake` | `ApplyScreenShake` |
| `0x8002ce40` | `setInitialItems` | `SetInitialItems` |
| `0x8002d084` | `rearrangeItemSlots` | `rearrange_item_slots` |
| `0x8002d354` | `GetItemSlot` | `get_item_slot` |
| `0x80052e20` | `Sound_SetFade` | `BuildSndFadeTbl` |
| `0x80052e6c` | `UpdateAudioFade` | `UpdateSoundFadeState` |
| `0x8005aad8` | `LoadTexture` | `ProcessTextureImage` |
| `0x8002b0b0` | `LoadDemoPlayerDate` | `LoadAttractModePlayerData` |
| `0x8002d28c` | `LoadInventorySprites` | `LoadHeldItemsImages` |

`ProcessTextureImage` is a good example of why call-site/behaviour beats the
name: the PS1 `LoadTexture` and PC `ProcessTextureImage` (0x0046c5f0) share the
same bank-to-VRAM placement rule (`bank*0x40`, minus `0x400` when bank ≥ 0x10),
which the general `LoadTexturePage` does not.

### 8.5 Pass 3 — entity model / room-init / message subsystems

Continuing the same method through `room_set`'s call tree, the entity model
loader and the message pipeline.

| PS1 address | Old | New (PC name) |
|---|---|---|
| `0x8002423c` | `FUN_8002423c` | `LoadEntityEMD` |
| `0x8002fd4c` | `FUN_8002fd4c` | `LoadEntityModel` |
| `0x8002fe14` | `FUN_8002fe14` | `InitAnimStructure` |
| `0x80030338` | `FUN_80030338` | `SetupJointStructures` |
| `0x80030410` | `FUN_80030410` | `ResetJointTransforms` |
| `0x80030698` | `FUN_80030698` | `Entity_SetJoints` |
| `0x80030a50` | `FUN_80030a50` | `SetupEntityJointAnimation` |
| `0x8003149c` | `FUN_8003149c` | `ClearAnimTiming` |
| `0x80042134` | `FUN_80042134` | `SetWeaponBodyParts` |
| `0x8003b354` | `FUN_8003b354` | `SetupCharacterData` |
| `0x8003b48c` | `FUN_8003b48c` | `InitPlayerEntity` |
| `0x80024684` | `LoadWeaponModel` | `LoadEquippedWeaponAnimation` |
| `0x8005a90c` | `RelocateModelData` | `ProcessTmdTextures` |
| `0x80044d54` | `FUN_80044d54` | `LoadRoomRdt` |
| `0x80045024` | `FUN_80045024` | `room_action_table_reset` |
| `0x800157a8` | `FUN_800157a8` | `texture_queue_reset` |
| `0x8001580c` | `FUN_8001580c` | `SetupTextureBankData` |
| `0x8003527c` | `getItemName` | `message_item_name_lookup` |
| `0x800359cc` | `displayMessage` | `set_message_display` |
| `0x800365a8` | `displayFoundItemName` | `message_render_chars` |
| `0x80035b9c` | `DisplayFoundItemMenu` | `UpdateMessageDisplay` |
| `0x800364dc` | `displayItemName` | `handle_message_post_action` |
| `0x80024480` | `LoadTMD` | `LoadBehaviorModel` (no PC equivalent) |

`0x8002423c` vs `0x80024480`: both load an EMD model, but only `0x8002423c`
matches PC `LoadEntityEMD` (same `g_emdPathTable[(charId&1)*… + id]` index and
the `modelLoadBuffer`/`animHeader`/`animBase` stores). `0x80024480` selects by
`behavior_flags` and is used by the five `0x80059xxx` weapon/effect setups; the
PC has no single counterpart, so it keeps a descriptive name.
`LoadEntityModel` (`0x8002fd4c`) is the wrapper that calls `LoadEntityEMD`,
`Entity_SetJoints`, `InitAnimStructure`, `SetupJointStructures`,
`ResetJointTransforms` — exactly PC `LoadEntityModel` (0x0048b630).

### 8.6 Pass 4 — SCD event VM

The room-event script VM (`RoomEvents.cpp`) is a clean one-to-one cluster. Every
function was confirmed body-for-body against the PC (e.g. PS1 `FUN_80027310`
matches `scd_event_state3_set_behavior` exactly; `FUN_80027390` reproduces every
opcode case of `scd_event_state2_movement`).

| PS1 address | Old | New (PC name) |
|---|---|---|
| `0x80026474` | `FUN_80026474` | `ScdEventEntry_Init` |
| `0x800264b0` | `FUN_800264b0` | `ScdEventEntry_Create` |
| `0x80026b38` | `FUN_80026b38` | `scd_event_state1_anim` |
| `0x80027310` | `FUN_80027310` | `scd_event_state3_set_behavior` |
| `0x80027390` | `FUN_80027390` | `scd_event_state2_movement` |
| `0x8002787c` | `FUN_8002787c` | `scd_event_cmd_set_entity` |
| `0x80027974` | `FUN_80027974` | `scd_event_cmd_create` |
| `0x800279dc` | `FUN_800279dc` | `scd_event_cmd_init` |
| `0x80027a10` | `FUN_80027a10` | `scd_event_cmd_run_scd` |
| `0x80027a6c` | `FUN_80027a6c` | `scd_event_cmd_exec` |

**Automated matching caveat.** With ~130 shared-name anchors the call-graph
matcher can now reach ~180 more PS1 functions, but it *permutes* members of a
cluster (it paired `0x80027310` with `scd_event_cmd_set_entity` even though the
body is `scd_event_state3_set_behavior`). Its output is therefore only a
candidate generator; each mapping still has to be confirmed against the PC body.
Do not bulk-apply it.

### 8.7 Pass 5 — room sprites, effects and collision

Body-for-body matches against `Room.cpp`, `EffectSprites.cpp` and
`RoomCollision.cpp`.

| PS1 address | Old | New (PC name) |
|---|---|---|
| `0x800220a4` | `FUN_800220a4` | `InitRoomEffSprite` |
| `0x8002233c` | `FUN_8002233c` | `load_effect_sprite_data` |
| `0x800223e8` | `FUN_800223e8` | `setup_effect_sprite_textures` |
| `0x8002aeb0` | `SetupRoomLights` | `update_entity_lighting` |
| `0x8003761c` | `FUN_8003761c` | `Room_LoadCameraSprites` |
| `0x80039ed4` | `FUN_80039ed4` | `ChkObjSlide` |
| `0x80039fd4` | `RenderObject` | `room_object_render` |
| `0x8004fa14` | `FUN_8004fa14` | `boundary_classify` |
| `0x8004fa70` | `FUN_8004fa70` | `boundary_classify_flags` |
| `0x8004fadc` | `FUN_8004fadc` | `ChkOutsideCell` |
| `0x8004fb18` | `FUN_8004fb18` | `boundary_point_outside` |
| `0x8004fb7c` | `FUN_8004fb7c` | `check_room_collision` |
| `0x80050134` | `FUN_80050134` | `check_room_collision_two_point` |

`room_object_render` is a good illustration of a port rewrite: the PS1 body is a
short `update_entity_lighting` + `GsGetLs` + `MulMatrix0` + `CheckPointInQuad` +
`GsSortObject5`, while the PC `room_object_render` (0x004745f0) adds pages of
stage/room special cases around the same core. `update_entity_lighting` was the
PC name behind the PS1-side `SetupRoomLights`.

Note the collision callback table `DAT_800b8a04` is only *read* in the main
binary (by `check_room_collision` and `check_room_collision_two_point`); it is
populated by the stage overlay, so the PS1 `Room_SetupCollisionCallbacks`
equivalent does not live in `SLUS_001.70`.

### 8.8 Pass 6 — camera zone, boot chain

| PS1 address | Old | New (PC name) |
|---|---|---|
| `0x80017da0` | `CheckPointInQuad` | `is_entity_in_switch_zone` |
| `0x8003239c` | `Boot_system` | `init_and_start_game` |
| `0x80032670` | `ScheduleLogoOverlay` | `load_global_assets` |
| `0x8005c2f0` | `ClearSystemFlags` | `ClearGameStateFlags` |
| `0x80023b8c` | `UpdateEffectBillboardAnim` | `Effect_AnimateSprite` |

The PS1 boot chain maps onto `GameInit.cpp`: `Boot_system` does the same
`ClearGameStateFlags` + task-table reset + POLY_F4 setup + `Task_execute(0, …)`
as `init_and_start_game`, and `ScheduleLogoOverlay` is `load_global_assets`
(load font + menu textures, then schedule the LOGO overlay / `logos_state`).

Functions that are genuinely PSYQ-only were deliberately **not** renamed
(no PC equivalent): `InitSystem`/`InitHW` (graphics+pad+CD init), `SetDrawOrigin`
(libgs draw origin), and the CD/FMV/MDEC wrappers (`PlayMovieByPath`, `_fmvPlay`,
`DecodeFMVFrame`, `StopCDStreaming`, `ResumeCDStreaming`, `WaitForMovieSync`,
`GetMovieFrameCounter`, `PlayXATrack`, `DecodeStillImage`) — the PC replaced all
of those with Marni/MCI.

### 8.9 Pass 7 — collision callbacks and game init

| PS1 address | Old | New (PC name) |
|---|---|---|
| `0x80029cc8` | `game_init` | `InitializeGame` |
| `0x8002b4a8` | `DisplayGameStartMessage` | `display_game_loading_message` |
| `0x8004f968` | `FUN_8004f968` | `Room_SetupCollisionCallbacks` |
| `0x80050d3c` | `FUN_80050d3c` | `collision_push_rect` |
| `0x80051004` | `FUN_80051004` | `collision_push_circle` |
| `0x80051190` | `FUN_80051190` | `collision_flag_set` |

This corrects the §8.7 note: `Room_SetupCollisionCallbacks` **is** in the main
binary (`FUN_8004f968`) — it rewrites the RDT boundary group pointers and
installs the handlers at `DAT_800b8a08`+ (the table base is `DAT_800b8a04`,
which is why an xref scan on `+0` missed it). The slot/signature match to the PC
is exact: `[1]=[5]=` `collision_push_rect`, `[3]=` `collision_push_circle`,
`[4]=` `collision_flag_set` (`ENTITY->collisionFlags |= 8`).

`game_init` → `InitializeGame` and `DisplayGameStartMessage` →
`display_game_loading_message` complete the `GameStart.cpp`/`RoomInit.cpp`
startup chain.

### 8.10 Pass 8 — SCD command handlers (`cmd_*`)

The PS1 binary's own `cmd_*` names encode the SCD opcode either as an `_0xNN`
suffix or as `objNN`. Since `CmdFunctions.cpp` documents the opcode for every
PC handler (`// 0xNN - cmd_name`), the whole namespace could be realigned to the
PC opcode table and confirmed against each PC body.

| PS1 address | Old | New (PC name / opcode) |
|---|---|---|
| `0x80045064` | `cmd_nop` | `cmd_block_end` (0x00) |
| `0x800453b4` | `cmd_obj06_test` | `cmd_state_byte_test` (0x06) |
| `0x8004545c` | `cmd_obj07_test` | `cmd_state_word_test` (0x07) |
| `0x80045504` | `cmd_room_cam_set` | `cmd_state_byte_set` (0x08) |
| `0x80045598` | `cmd_cut_set_0x09` | `cmd_cut_lock_set` (0x09) |
| `0x800457b8` | `cmd_nop_0x2e` | `cmd_dead_slot_hang_2e` (0x2E) |
| `0x800457c0` | `cmd_bgm_0x15` | `cmd_bgm_play` (0x15) |
| `0x80045c48` | `cmd_nop_0x26` | `cmd_dead_slot_hang_26` (0x26) |
| `0x80045d1c` | `cmd_player_pos_0x17` | `cmd_sfx_3d_play` (0x17) |
| `0x80045e8c` | `cmd_weapon_set` | `cmd_equipped_item_test` (0x1D) |
| `0x80045ed4` | `cmd_obj11_test` | `cmd_picked_item_test` (0x11) |
| `0x80045f08` | `cmd_obj10_test` | `cmd_used_item_test` (0x10) |
| `0x80045f3c` | `cmd_obj19_set` | `cmd_model_flag_set` (0x19) |
| `0x80045ff4` | `cmd_item_flag_0x12` | `cmd_room_action_reset` (0x12) |
| `0x80046068` | `cmd_0x13` | `cmd_room_action_arm` (0x13) |
| `0x800460b8` | `cmd_0x14` | `cmd_scd_event_create` (0x14) |
| `0x80046168` | `cmd_entities_0x0f` | `cmd_mirror_set` (0x0F) |
| `0x80046b7c` | `cmd_sfx_set` | `cmd_voice_play` (0x1E) |
| `0x80046f4c` | `cmd_rdt_0x25` | `cmd_room_sprite_set` (0x25) |
| `0x80046fc0` | `cmd_0x1c` | `cmd_room_light_fade_set` (0x1C) |
| `0x80047308` | `cmd_item_cmd_0x22` | `cmd_item_count_test` (0x22) |
| `0x800474c4` | `cmd_cut_toggle` | `cmd_cut_lock_write` (0x23) |
| `0x800475ac` | `cmd_enemy_0x28` | `cmd_enemy_prop_set` (0x28) |
| `0x80047edc` | `cmd_player_anim_0x2b` | `cmd_attack_anim_set` (0x2B) |
| `0x80047fac` | `cmd_item_get` | `cmd_got_item` (0x2D) |

**Completing the dispatch table.** The remaining slots were still raw
`LAB_*`/`FUN_*`/misnamed targets because Ghidra had not turned the table-only
code pointers into functions. Reading the table at `0x8009080c` (index = opcode)
gave the full mapping, and each undefined target was created as a function and
named:

| opcode | address | PC name |
|---|---|---|
| 0x2F | `0x80045ad0` | `cmd_snd_pan_vol_set` |
| 0x30 | `0x80047fe8` | `cmd_boundary_set` |
| 0x31 | `0x8004553c` | `cmd_state_word_set` |
| 0x32 | `0x800480b0` | `cmd_skip_4bytes` |
| 0x33 | `0x800477bc` | `cmd_player_prop_set` |
| 0x34 | `0x80048134` | `cmd_model_tint_set` |
| 0x35 | `0x80048240` | `cmd_obj_flag_set` |
| 0x36 | `0x800482cc` | `cmd_obj_field_test` |
| 0x37 | `0x80045768` | `cmd_room_bgm_state_set` |
| 0x38 | `0x8004843c` | `cmd_dpad_test` |
| 0x39 | `0x80048498` | `cmd_enemy_flags_get` |
| 0x3A | `0x800484ec` | `cmd_cut_zone_set` |
| 0x3B | `0x80048550` | `cmd_obj_rotation_set` |
| 0x3C | `0x80048624` | `cmd_player_dist_test` |
| 0x3D | `0x80047bbc` | `cmd_bullet_effect_spawn` |
| 0x3E | `0x80047cf4` | `cmd_bullet_effect_clear` |
| 0x3F | `0x80048770` | `cmd_player_dir_test` |
| 0x40 | `0x800487b8` | `cmd_light_param_set` |
| 0x41 | `0x80048860` | `cmd_entity_posy_set` |
| 0x42 | `0x80047d48` | `cmd_effect_clear_typed` |
| 0x43 | `0x80045ca0` | `cmd_bgm_volume_ramp` |
| 0x44 | `0x80046114` | `cmd_scd_event_kill` |
| 0x45 | `0x800488e0` | `cmd_player_posy_add` |
| 0x46 | `0x8004891c` | `cmd_room_lights_set` |
| 0x47 | `0x80047258` | `cmd_obj_transform_set` |
| 0x48 | `0x80047d9c` | `cmd_effect_pool_clear` |
| 0x49 | `0x80048b10` | `cmd_room_sprite_hide` |
| 0x4A | `0x80045838` | `cmd_bgm_restore` |
| 0x4B | `0x80045978` | `cmd_bgm_stop_all` |
| 0x4C | `0x80048b70` | `cmd_item_record_transfer` |
| 0x4D | `0x80048cd8` | `cmd_player_joint_tint` |
| 0x4E | `0x80047e1c` | `cmd_effect_flags_modify` |

Four earlier names were wrong and are corrected:

| address | old | correct (opcode) |
|---|---|---|
| `0x80045118` | `cmd_else_if` | `cmd_end_if` (0x03) |
| `0x80046230` | `cmd_item_set` | `cmd_room_action_set` (0x0D) |
| `0x80047538` | `cmd_room_action_set` | `cmd_room_action` (0x24) |
| `0x80045b70` | `cmd_volume_set` | `cmd_bgm_stop` (0x16) |

The PS1 table has entries for opcodes `0x00`–`0x4E` only; `0x4F`/`0x50`
(`cmd_costume_variant_set`/`_test`) are PC-table entries with no PS1 handler.
The whole SCD command namespace is now consistent with the PC opcode table.

### 8.11 Unresolved / conflicting names

- `SetCDVolume` at `0x80015108` (CD-DA streaming mix via `g_CdVolumeATV`) and at
  `0x8005ba20` (CD-DA mode/volume via `g_cddaVolume`). The PC decomp has no
  matching pair, so one must be named with a disambiguating name.
- `UpdateJointEffect` at `0x8005c4e0` (the gravity/floor joint state updater —
  correct) and at `0x8005d7d4` (a joint-ribbon `RotAverage3`+`addPrim` renderer
  corresponding to PC `FUN_0048aef0`/`FUN_00486280`, which has no PC name yet).
- PSYQ-runtime duplicates (`memclr`, `memcpy`, `memset`, `SsUtKeyOff`,
  `_dws`, `_spu_setInTransfer`) are legitimate separate library copies and are
  not misnomers.

### 8.12 Remaining work

The same call-site method applies to the rest. Two backlogs remain:

1. **Named but still PS1-side names** (the larger backlog): the ~600 game
   functions outside the passes above still use the PS1 vocabulary (for example
   `AudioSystemInit`/`AudioLoadBank*` and the CD wrappers, the menu/save-screen
   block at `0x8004905c`–`0x8004be30`, the model/texture loaders at
   `0x800119b4`–`0x80014d94`, the effect/joint helpers). Each needs the same
   PC-source lookup and then a rename.
2. **Unnamed `FUN_*` engine helpers** — highest value:
   - `0x800119b4`–`0x80014d94` (room/model/texture loaders) — match against
     `TextureLoader.cpp` / `EntityModelLoader.cpp` by callee set.
   - `main_menu`/`options_menu` are referenced as data labels
     `DAT_800535a4`/`DAT_8003a35c` from `check_menus_state`; they are not yet
     defined as functions in Ghidra.
   - `0x8002b988`/`0x8002b9e4` die-screen helpers (`image_update` /
     `update_image_fading_`).
   - Stage-overlay bodies (`STAGE1..7`) and the menu overlays, once their entry
     callback tables (see §4.5) are labelled; these have no single PC address
     because the PC links them directly.

PSYQ library functions (`0x8005f94c`+) should keep their PSYQ names; they are
deliberately not renamed to PC names because there is no counterpart.

---

## 9. Tooling added

`tools/psx_overlay.py` — Capstone-based inspector for the PS1 build:

```
python tools/psx_overlay.py info                 # PS-EX header table (main + overlays)
python tools/psx_overlay.py strings [--min N]    # embedded ASCII strings per image
python tools/psx_overlay.py dis <image> [--at ADDR] [--count N]
python tools/psx_overlay.py calls <image> --target ADDR
python tools/psx_overlay.py extern <image>       # call targets outside the image
```

`<image>` is `main` or an overlay name (`LOGO`, `STAGE1`, …). The main
executable path defaults to `assets/PSX/SLUS_001.70`, then to the extracted ISO
copy `G:\redecomp\iso_content\RE1_psx\SLUS_001.70`; override with
`RE1_PSX_MAIN` or place the file in `assets/PSX/`.

---

## 10. Summary of engine differences

1. **Packaging:** PS1 = resident kernel + 12 overlays in two swap regions; PC =
   one monolithic EXE with all overlays linked in.
2. **Runtime:** PS1 = PSYQ libraries + PS1 BIOS/hardware; PC = Marni System over
   DirectX 5 (here D3D11/XAudio2/XInput) + MSVC CRT.
3. **Game logic is shared.** Task scheduler, SCD VM, entities, inventory,
   camera zones, RDT handling and the `GameLoop` frame body map one-to-one.
4. **RDT is unchanged** and mostly byte-identical; the PC port moved room
   backgrounds out of `.BSS` into per-camera LZW `.pak`.
5. **Models (`.EMW`/`.EMD`/`.TMD`) and door scripts (`.DOR`) are unchanged.**
6. **Audio and video are reformatted** on PC (SEQ/VAB/XA → WAV, MDEC `.STR` →
   `.avi`); the calling engine code is shared, only the backends differ.
7. **Small behavioural deltas** from the port: a longer, room-aware death state
   machine and a slightly larger `room_state_reset`, plus PC-only debug
   features. None change the game's data formats.
