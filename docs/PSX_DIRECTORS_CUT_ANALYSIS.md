# Director's Cut (SLUS_005.51) vs OG PS1 (SLUS_001.70)

Second PS1-side analysis, comparing the 1997 **Director's Cut** PS1 USA build
against the original PS1 USA build already documented in
`docs/PSX_ENGINE_ANALYSIS.md`. Produced from the Ghidra programs
`SLUS_005.51` and `SLUS_001.70`, the extracted asset trees `assets/PSX_DC/`
(DC) and `assets/PSX/` (OG), and the byte-level alignment tool
`tools/psx_align.py`.

> Addresses are MIPS virtual addresses (`0x80......`). Neither build's
> addresses map to the other by a constant offset; every mapping in this file
> was derived structurally (see §3).
>
> PROG2 overlays are diffed separately: `docs/PSX_DC_TITLE_OVERLAY.md` covers
> `TITLE.EXE` and the STANDARD/TRAINING/ADVANCED mode flags;
> `docs/PSX_DC_SELECT_OVERLAY.md` covers `SELECT.EXE` (unchanged recompile);
> `docs/PSX_DC_PROLOGUE_OVERLAY.md` covers `PROLOGUE.EXE` (identical, codegen
> only); `docs/PSX_DC_ENDING_OVERLAY.md` covers `ENDING.EXE` (mode-aware,
> ADVANCED-only unlocks); `docs/PSX_DC_STAGE_OVERLAYS.md` covers
> `STAGE1`–`STAGE7.EXE` (ADVANCED stage-descriptor selection — the mode
> consumer); `docs/PSX_DC_ENEMY_AI.md` covers the per-stage enemy AI and the
> DC zombie additions; `docs/PSX_DC_ITEMS.md` covers the item tables, the new
> ADVANCED Beretta and the mixable crest parts; `docs/PSX_DC_AUDIO.md` covers
> the audio (unchanged).

---

## 1. Sources

| Program / path | What it is |
|---|---|
| `SLUS_005.51` (Ghidra) | Director's Cut PS1 main executable, 651264 B |
| `assets/PSX_DC/` | Director's Cut asset tree (918 files) |
| `SLUS_001.70` (Ghidra) | original PS1 main executable, 653312 B |
| `assets/PSX/` | original PS1 asset tree (937 files + derived WAVs) |

The DC executable lives at `G:\Emuladores\pc iso\Directors Cut\SLUS_005.51`;
the OG one at `G:\redecomp\iso_content\RE1_psx\SLUS_001.70`. Both are plain
PS-EX images (`PS-X EXE`), text `< 0x800af000`.

---

## 2. Build / layout differences

| | SLUS_001.70 (OG) | SLUS_005.51 (DC) |
|---|---|---|
| File size | 653312 | 651264 |
| Entry point | `0x8005f8a0` | `0x8005ef34` |
| Text load address | `0x80010000` | `0x80010000` |
| Text size | `0x9f000` | `0x9e800` |
| Stack | `0x801ffff0` | `0x801ffff0` |
| Functions in Ghidra (fresh) | 1527 | 1652 |

The DC image is **2048 bytes smaller** yet has ~125 more functions: the build
deleted/merged some resident code and added new routines. Function addresses
shift non-uniformly — the local `dc - og` delta ranges from about `-0x3C8` to
`-0x980` across the image — so **no constant address translation exists**.

Landmarks re-derived for the DC:

| Thing | OG | DC |
|---|---|---|
| SCD command dispatch table (`run_command_functions`) | `0x8009080c` | `0x80091108` |
| `run_command_functions` | `0x80048f18` | `0x800489cc` |
| `game_loop` | `0x8002a0c8` | `0x80029ae4` |
| `room_set` | `0x80044704` | `0x80044138` |
| `load_global_assets` (`ScheduleLogoOverlay`) | `0x80032670` | `0x80032074` |

The SCD table is the strongest anchor: it has the **same 79 entries**
(opcodes `0x00`–`0x4E`) in the same order, so opcode *n* maps to the same
handler on both builds. No new opcodes were added for Arrange mode.

---

## 3. Naming the DC after the OG (and therefore after the PC)

The two PS1 builds are MIPS but different compilations, and Ghidra's
cross-binary fuzzy matcher ties on identical small functions. `tools/psx_align.py`
instead disassembles both binaries with Capstone at Ghidra's function
boundaries, normalises every instruction to `mnemonic + operand-shape` (all
immediates and register names masked), fingerprints each function by its set of
5-gram instruction tuples, and runs a Needleman–Wunsch alignment that preserves
function order. The order constraint resolves the duplicate-function ties, and
the address delta was verified to be locally monotonic with **zero anomalies**
across the aligned pairs.

Result of the alignment: **713 function pairs** (the remainder are functions
that changed too much or were inserted/deleted).

Applied to the DC Ghidra program (saved):

| Batch | Count | Source |
|---|---:|---|
| Game functions renamed | 163 | alignment pairs with an OG name, DC still `FUN_*` |
| SCD handlers created + named | 79 | OG opcode table at `0x8009080c` → DC `0x80091108` |
| LibPSYQ/libgs functions renamed | 46 | alignment pairs, DC still `FUN_*` |
| Globals named | 77 | constant-reference voting (§3.1) |

Every renamed function/global carries a plate comment naming its
`SLUS_001.70` equivalent and address. DC named functions went from ~1040/1652
to **1313/1728**; named *game* functions from ~66 to **309**.

### 3.1 Globals

OG globals were mapped by matching absolute address constants that appear at
aligned instruction positions inside aligned function pairs (a `lui`+`addiu`/`ori`
sequence is reconstructed to its 32-bit address, then voted across functions).
77 OG globals got a mutual-best mapping with ≥2 votes.

Two useful facts fell out:

- The **`0x800b…`/`0x800c…` BSS/state block is byte-for-byte address-identical**
  between the builds (`g_gameStateFlags` `0x800c3000`, `g_PlayerEntityPtr`
  `0x800c5124`, `g_PlayerPosition` `0x800c5158`, `g_StageId` `0x800c8660`,
  …). 231 of the DC's 237 existing globals already match the OG by address
  *and* name.
- Only the **initialized data inside the text segment** (`0x800a…`) and the
  PSYQ/CD work area (`0x800b5…`, `0x800ba…`, …) shifted, tracking the code.
  Those are the ones the vote mapped (e.g. `debug_level` `0x800a649e` →
  `0x800a6866`, `g_CdCommandBuffer` `0x800aecf8` → `0x800ae5d0`).

397 OG-only globals remain unnamed in the DC program; the vote only covers
constants referenced from aligned functions.

---

## 4. Asset differences

`assets/PSX_DC/` (918 files) vs `assets/PSX/` (937 files, excluding the 296
WAVs previously generated in place by `tools/psx_audio_extract.py`).

### 4.1 New stages: `STAGE8`–`STAGEE`

The headline addition: **seven new stage directories** with `8xx`–`Exx` room
ids, one per original area. Rooms per stage (RDT / BSS):

| Stage | DC RDT | DC BSS | OG counterpart | OG RDT / BSS |
|---|---:|---:|---|---:|
| `STAGE1` | 43 | 29 | Mansion 1F | 58 / 29 |
| `STAGE2` | 30 | 29 | Mansion 2F | 58 / 29 |
| `STAGE3` | 29 | 18 | Courtyard/underground | 36 / 18 |
| `STAGE4` | 30 | 18 | Guardhouse | 36 / 18 |
| `STAGE5` | 37 | 22 | Laboratory | 44 / 22 |
| `STAGE6` | 51 | 0 | Mansion return 1F | 58 / 0 |
| `STAGE7` | 49 | 0 | Mansion return 2F | 58 / 0 |
| `STAGE8` | 13 | 9 | *(new)* | – |
| `STAGE9` | 9 | 7 | *(new)* | – |
| `STAGEA` | 4 | 2 | *(new)* | – |
| `STAGEB` | 5 | 3 | *(new)* | – |
| `STAGEC` | 7 | 4 | *(new)* | – |
| `STAGED` | 16 | 0 | *(new)* | – |
| `STAGEE` | 12 | 0 | *(new)* | – |

The new directories use the same `.RDT`/`.BSS` formats, so the existing parser
reads them. `STAGE8`–`STAGEE` are the Arrange-mode room data; they reuse
`STAGE1`–`STAGE7`'s background banks where a room is unchanged (the
`STAGE6`/`STAGE7` no-BSS rule from the OG analysis still holds), which is why
the layout is best read as *per-area Arrange overrides* rather than a full
parallel map. **The exact directory-selection path (mode flag → stage index)
has not been located yet — open item.**

The DC also **drops a number of `…1` alternate-state RDTs** from the original
stage directories (e.g. `ROOM1021`, `ROOM1071`, `ROOM10A1`, `ROOM10B1`,
`ROOM10D1`, `ROOM1100/1101`, `ROOM1111`, `ROOM1131`, `ROOM1141`, `ROOM1161`,
`ROOM1181`, `ROOM1190/1191`, `ROOM11A1` in `STAGE1` alone). Every DC directory
is a strict subset of the OG one for the original stages.

### 4.2 New enemy models

Nine `.EMD` files that do not exist in the OG tree:

```
EM1016 EM1032 EM1033 EM1035 EM1040 EM1041 EM1043 EM104C EM1116
```

(OG 65 models → DC 74). These are additional enemy model variants; `EM1016`
and `EM1116` are 189980 B, `EM1043` 194604 B.

### 4.3 New player / weapon models

`PLAYERS/W0F.EMW` (24028 B) and `PLAYERS/W1F.EMW` (25580 B) — new weapon or
costume models (OG 37 → DC 39).

### 4.4 New item art

`ITEM_M2/` adds `I00V_S1.IVM`, `I60V_L.IVM`, `I60V_R.IVM`, `I99V.IVM`, and
consolidates the OG's 46 split `FILEM_*.PIX` pages into one `FILEM.PIX`
(2355200 B). The shared item atlases grew: `DATA/ITEM_ALL.PIX` 86400 → 91200 B
and `DATA/ITEM_MIX.PIX` 21600 → 25200 B, i.e. **more item sprite slots**.

### 4.5 Other data

- `DATA/BT367OAB.TIM` added (72128 B); the OG's `BT367.TIM` (30913 B) and
  `CAPCOM.PTC` (548352 B) are absent. `BT367OAB.TIM` is ~2.3× larger.
- Many original-stage `.RDT` files are **larger in the DC** (e.g.
  `ROOM1050.RDT` 560596 → 595124 B, `ROOM1051` 604720 → 639468,
  `ROOM20A0` 467288 → 502776) — the changed camera/enemy/item data.
- `MOVIE/CAPCOM.STR` shrank 2969600 → 2459648 B.
- All 12 `PROG2/*.EXE` overlays are **smaller** in the DC (`STAGE1.EXE`
  122880 → 118784 B, …) with no new overlay added. This confirms Arrange mode
  is driven by resident code + the new stage data, not a new overlay.

---

## 5. Engine notes

- `run_command_functions` (`0x800489cc`, DC) reads the handler table at
  `0x80091108`; the handler body is otherwise identical to the OG's (same
  `DAT_800cf640`/`DAT_800cf63a` cursor state). The table was used to
  reconstruct all 79 DC handlers, which Ghidra had not split into functions
  (the fresh DC program lacked the table-only targets the earlier OG pass had
  created).
- The OG analysis' `0x800b8a04` collision-callback table and the overlay
  callback registration (`0x800caa18`) are in the resident image on both
  builds; the DC restructuring did not move the game-state block (§3.1).
- Remaining DC game functions still named `FUN_*`: **322** (many newly split
  or genuinely changed Arrange-mode code); 110 of them had no alignment pair
  at all.

---

## 6. Remaining work

1. Locate the **Original/Arrange mode selection**: where the mode flag is read
   and how it offsets the stage directory (`STAGE1`→`STAGE8`, …) at room load.
2. Map the 397 unmapped OG globals (extend the constant-vote pass to
   unaligned functions, or use xref sites).
3. Name the 110 unmatched DC game functions against the PC decomp
   (`src/`) — they are the best candidates for Arrange-mode-only logic.
4. Diff the DC `.RDT` camera/enemy/item sections against the OG rooms to
   characterise the Arrange changes per room (the existing PC RDT parser and
   `tools/mine_room_scd.py` apply unchanged).

---

## 7. Tooling

`tools/psx_align.py` — Capstone + Needleman–Wunsch alignment of two PS1 builds
(see its header for the three subcommands: `dump`, `align`, `rename-tsv`).
It needs the Ghidra function lists from the REST bridge:

```
python tools/psx_align.py dump SLUS_001.70 og_fn.txt
python tools/psx_align.py dump SLUS_005.51 dc_fn.txt
python tools/psx_align.py align <og_exe> <dc_exe> og_fn.txt dc_fn.txt -o pairs.json
python tools/psx_align.py rename-tsv pairs.json dc_fn.txt -o renames.tsv
```
