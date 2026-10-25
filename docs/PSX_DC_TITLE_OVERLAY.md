# Director's Cut TITLE overlay (`PROG2/TITLE.EXE`) vs OG

Comparison of the Director's Cut (`SLUS_005.51`) title overlay against the
original PS1 release (`SLUS_001.70`), and the game-mode flags each DC option
sets. Part of `docs/PSX_DIRECTORS_CUT_ANALYSIS.md`.

> Both overlays load at `0x800e0000` and are the same size (8192 B), so the
> comparison is direct. Addresses are MIPS virtual addresses.

## 1. Build / layout

| | OG | DC |
|---|---|---|
| File size | 8192 | 8192 |
| Load address | `0x800e0000` | `0x800e0000` |
| PS-EX entry | `0x800e11a8` | `0x800e16b8` |
| Body | inline at the entry | entry is a thin stub → `title_state` `0x800e0064` |

The DC moved the title code to the front of the overlay and left the entry as a
one-call stub. Re-derived DC functions (named in Ghidra):

| Address | Name | Role |
|---|---|---|
| `0x800e0064` | `title_state` | whole-screen state machine, resolves the selection |
| `0x800e0740` | `title_menu_update` | per-frame input / page / submenu state machine |
| `0x800e0498` | `title_init` | loads the menu sprites, sets the initial selection |
| `0x800e14d8` | `title_draw_option` | draws item *n* (`0x80` fade byte, option index) |
| `0x800e1664` | `title_request_exit` | clears the loop flag / sets the screen mode |

OG counterparts: the state machine is inline in the entry and its menu update is
`FUN_800e06ac` (OG `0x800e06ac`), draw `FUN_800e0fac`.

## 2. Menu graphics — the actual "different options"

Each frame is one **10304-byte TIM** (`0x2840`): 8-byte TIM header + a 44-byte
16-colour CLUT block at VRAM `(0,480)` + a 10252-byte **256×80 4bpp** image. The
frames are concatenated in the file, so `title_init` loads the whole file and
copies `source + n*0x2840` into its sprite buffer.

Rendered contents (each chunk decoded through its own CLUT):

| Chunk | OG `DATA/BT367.TIM` (30913 B, 3 chunks) | DC `DATA/BT367OAB.TIM` (72128 B, 7 chunks) |
|---:|---|---|
| 0 | `PRESS ANY BUTTON` + copyright | `PRESS ANY BUTTON` + copyright |
| 1 | `NEW GAME`/`LOAD GAME` — NEW GAME selected, + copyright | `NEW GAME`/`LOAD GAME` — NEW GAME selected |
| 2 | `NEW GAME`/`LOAD GAME` — LOAD GAME selected, + copyright | `NEW GAME`/`LOAD GAME` — LOAD GAME selected |
| 3 | — | `STANDARD`/`TRAINING`/`ADVANCED` — STANDARD selected |
| 4 | — | `STANDARD`/`TRAINING`/`ADVANCED` — TRAINING selected |
| 5 | — | `STANDARD`/`TRAINING`/`ADVANCED` — ADVANCED selected |
| 6 | — | `STANDARD`/`TRAINING`/`ADVANCED` — ADVANCED selected in **green** |

Two differences beyond the extra frames: the DC copyright reads
`©CAPCOM CO.,LTD.1996, 1997` / `©CAPCOM U.S.A.,INC.1996,1997 ALL RIGHTS RESERVED.`,
and the DC's **menu frames (chunks 1-6) carry no copyright line** — it appears
only on the PRESS ANY BUTTON frame.

`title_init` loads file index **2** (OG `DATA\BT367.TIM`, DC `DATA\BT367OAB.TIM`
— the DC has no plain `BT367.TIM`), so the DC's new options are literally four
more `0x2840` frames in that one file.

### PS1 draw geometry (verified in `TITLE.EXE`)

`title_draw_option` (`0x800e14d8`) draws one option as a **whole 256×80 cell** at
a fixed `screenX = -130` (`0xFF7E`), `screenY = 38` (`0x26`), width `0x100` and
height `0x50`. Per option it takes:

- `g_titleOptionId[option]` (`0x800e16e8`) → the cell's `texV` (its row in the
  loaded sheet). `title_init` fills it at run time, so the array is BSS-zero at
  rest.
- `g_titleOptionX[option]` (`0x800e16d0`) → `clutY = 0x1E0 + g_titleOptionX`,
  i.e. which palette row to use (also filled at run time).

Palettes: all seven cells carry the same 16-entry CLUT — `0` transparent,
`1` bright, `2` light, `3` dim, `4..15` black — except cell 6, whose entry `1` is
pure green (`0x81C0`) for the ADVANCED* highlight. `g_titleOptionX` only has to
distinguish those two rows.

### How the PC port draws it

The PC port pre-bakes the same art as a 256×256 sheet, one file per prompt:

| PC asset | Content |
|---|---|
| `Data/TITLE.PIX` | the 320×240 16-bit title background |
| `Data/t_press.tim` | sheet rows: `PRESS ANY BUTTON` (vramY 0), NEW GAME frame (83), LOAD GAME frame (175), each with copyright — used with no pad |
| `Data/t_start.tim` | same sheet but `PRESS START BUTTON` — used when a pad is connected |

`src/game/TitleScreen.cpp` (`init_title_screen`) loads `data\title.pix` through
`display_image` and one of the two sheets; `UpdateTitleTextSprite` (`0x00430d40`)
then blits row `g_titleSelectionId` of the sheet using `g_titleTextPosTable[3]`
(`vramY` / `sprHeight` / `screenY` per row).

So the DC title needs **two** asset changes: a different `TITLE.PIX` (6461 bytes
differ from the USA/PS1-OG image — the "DIRECTOR'S CUT" logo), and the option
frames stacked into a PC sheet built from `BT367OAB.TIM`'s 7 chunks, with
`g_titleTextPosTable` and the selection index extended for the submenu.

**DC options:**

```
NEW GAME  →  STANDARD   (same room/enemy/item data as the OG — no mode flag)
             TRAINING   (more starting health)
             ADVANCED   (fewer starting health; the reworked mode)
             ADVANCED*  (green frame; reached by holding on ADVANCED)
LOAD GAME
```

`STANDARD` / `TRAINING` / `ADVANCED` are the DC's answer to Original/Arrange;
`ADVANCED` is the mode that ships the changed cameras/enemies/items (the
`STAGE8`–`STAGEE` data, §4 of the main analysis).

## 3. State machine difference

| | OG | DC |
|---|---|---|
| Choice byte | `DAT_800e11d0` (1..2) | `g_titleMenuChoice` `0x800e16f6` (1..2, submenu 3..6, 0 = attract) |
| Loop/exit | `DAT_800e11d4` | `g_titleExitFlag` `0x800e16f8` |
| Page / sub-page | `DAT_800e11d8` | `g_titlePage` `0x800e16f9` |
| State | `DAT_800e11dc` | `g_titleState` `0x800e16fa` |
| Timer / hold | `DAT_800e11b8` | `g_titleStateTimer` `0x800e16c8`, `g_titleHoldTimer` `0x800e16ca` |
| Menu items | 2 (`NEW GAME`, `LOAD GAME`) | 2 + submenu of 3 (`STANDARD`, `TRAINING`, `ADVANCED`) |
| Extra BSS zeroed on entry | 3 bytes | 4 bytes (`g_titleOptionPad` `0x800e16f7` is new) |

The DC added a **submenu state** (`g_titleState == 10`): pressing NEW GAME opens
it (`g_titleMenuChoice = 3`), up/down cycle `3..5`, and **holding** on
`ADVANCED` (`g_titleHoldTimer`, 0x5A frames) promotes the choice to `6` (the
green frame). Confirm then runs the same memory-card/new-game path as the OG's
`NEW GAME`. The attract-demo timeout (`g_titleMenuChoice = 0`) and the
`LOAD GAME` path are unchanged in structure.

Overlay indices scheduled at exit (DC indices shifted because the DC file table
gained the `STAGE8`–`STAGEE` files): attract → `0x12b`, new game → `0x12d`,
card-fail → `0x135` (OG: `0x14a` / `0x14c` / `0x154`).

## 4. Flags and globals set per mode

**Terminology.** PS1 `g_gameStateFlags` (`0x800c3000`) is the PC
`g_main_state_flags` (`0x00be41c0`); PS1 `g_status_flags` (`0x800c3004`) is the
PC `g_main_state_flags2` (`0x00be41c4`) — confirmed by both `InitializeGame`s
branching on `g_status_flags & 0x10000000` for `MSF2_ATTRACT_DEMO`. The USA PC
build leaves msf2 **bits 16-18 (`0x10000`/`0x20000`/`0x40000`) unused**; the DC
repurposes them for the game mode.

The DC title clears `0x70000` (`g_status_flags &= 0xfff8ffff`) and then, in
`title_menu_update` case 10, sets:

| DC option | `g_titleMenuChoice` | `g_DcGameMode` `0x800c8693` | `g_status_flags` (msf2) | starting health (char 0 / char 1) | `DAT_800c8724` |
|---|---|---:|---|---:|---|
| `STANDARD` | 3 | 0 | — | 140 / 96 (OG value) | — |
| `TRAINING` | 4 | 1 | `0x40000` (bit 18) | 180 / 150 | `0x1e0b1e0b` |
| `ADVANCED` | 5 | 2 | `0x20000` (bit 17) | 100 / 70 | — |
| `ADVANCED*` (hold, double ammo) | 6 | 3 | `0x30000` (bits 16+17) | 100 / 70 | `0x1e0b1e0b` |

`g_DcGameMode` (`0x800c8693`) is a **DC-only byte** — the OG main binary has no
reference to it. It is written by the title and by `InitializeGame`, and read by
`FUN_80019638`, which maps it straight back onto the same `g_status_flags` bits
and sets `g_gameStateFlags |= 0x10000000` (`|0x10800000` when the character id
`& 3` is non-zero). So the mode is carried as a 2-bit field:

```
0 = STANDARD   1 = TRAINING   2 = ADVANCED   3 = ADVANCED variant
```

### Where the mode changes gameplay (`InitializeGame`)

In the new-game branch the DC adds a mode block that the OG does not have
(the OG only sets `g_PlayerHealth = (charId & 1) * -0x2c + 0x8c`):

```c
DAT_800c867e = (charId & 1) * -0x2c + 0x8c;      // STANDARD (OG value)
DAT_800c51ac = DAT_800c867e;                     // health mirror
if (g_status_flags & 0x40000) {                  // TRAINING
    DAT_800c867e = (charId & 1) * -0x1e + 0xb4;  // 180 / 150
    g_DcGameMode = 1;
    DAT_800c8724 = 0x1e0b1e0b;
}
if (g_status_flags & 0x20000) {                  // ADVANCED
    g_DcGameMode = 2;
    DAT_800c51ac = (charId & 1) * -0x1e + 100;   // 100 / 70
    DAT_800c867e = DAT_800c51ac;
    if (g_status_flags & 0x10000) {              // ADVANCED* (hold)
        g_DcGameMode = 3;
        DAT_800c8724 = 0x1e0b1e0b;
    }
}
```

`DAT_800c867e` / `DAT_800c51ac` are the player health and its mirror, and
`DAT_800c8724 = 0x1e0b1e0b` (two `0x1e0b` words) is the mode-dependent starting
loadout touched only by the DC. A later block also adjusts `DAT_800c5299`
(another per-character offset) on the same two bits.

Other title-screen globals, unchanged in role from the OG: `DAT_800c301c`
(screen sub-state, set to 2 on new-game entry), `g_fade_type_id`
(`DAT_800cf86f`) and the fade level `DAT_800cab0a`, `DAT_800c8674/6` (menu
timer), `g_gameStateFlags` screen-mode bits 30/31, `g_status_flags` bit
`0x10000000` for the attract demo.

## 5. Open items

1. **Which consumer selects `STAGE8`–`STAGEE`.** `g_status_flags` is read at
   ~60 sites in the DC main binary, so the stage/directory selection that acts
   on `ADVANCED` (`0x20000`) still has to be traced. `STANDARD` (no bits) must
   fall back to `STAGE1`–`STAGE7`.
2. **`DAT_800c8724 = 0x1e0b1e0b`** — the two `0x1e0b` words look like an item /
   model id granted per mode; needs the item-id table to name.
3. `TRAINING` and `ADVANCED*` both set `DAT_800c8724`; whether `ADVANCED*` is a
   distinct mode or just ADVANCED's "held" confirmation is not yet proven from
   gameplay — the flag is distinct (`g_DcGameMode == 3`).
4. Remaining PROG2 overlays (`LOGO`, `PROLOGUE`, `SELECT`, `ENDING`,
   `STAGE1`–`STAGE7`) still to be diffed; `SELECT` and the stage overlays are
   the next likely mode consumers.

## 6. The mode byte in the save block — slot colour-coding

The DC also stores the selected mode **inside the save block**, and the
save/load screen colours each slot's text from it (verified in the DC binary):

- `g_abDcGameMode` (`0x800c8693`) sits at `g_StageId` (`0x800c8660`) + `0x33`,
  i.e. **bio-card +0x233** (`+0x33` inside the 0x240-byte per-slot state block
  that starts at bio-card +0x200). The save path memcpy's the whole state block
  (`&g_StageId`, `0x13c` bytes) into the slot record, so every slot records its
  mode; the load path memcpy's it back and calls `FUN_80019638`, which maps the
  byte onto the `g_status_flags` bits (§4).
- The save/load screen is `FUN_800181b0` (resident); its slot list is drawn by
  `FUN_800191b4`, which draws row *n* as
  `FUN_80034bd0(0x30, y, *(byte *)(record + 0xa33), rowText)` — the text
  **colour argument is that row's mode byte**.
- `FUN_80034bd0` (`0x80034bd0`) converts the colour byte into a CLUT select:
  `CLUT_x = 0x100 + (colour & 3) * 0x10`, `CLUT_y = 0x1E0 + (colour >> 2)`. So
  modes `0`–`3` pick four 16-entry colour columns — one per difficulty. For mode
  `3` the row is drawn a second time with colour `0x11`
  (`CLUT_x 0x110`, `CLUT_y 0x1E4`), i.e. ADVANCED* has a special highlight.
- Empty slots draw the dash template with colour `0`.

So a save made in TRAINING / ADVANCED / ADVANCED* is colour-coded on the
save/load screen — that is how a slot list shows each save's difficulty.
