# Director's Cut STAGE overlays (`PROG2/STAGE1`–`STAGE7.EXE`) vs OG

Comparison of the Director's Cut (`SLUS_005.51`) stage overlays against the
original PS1 release (`SLUS_001.70`). Part of
`docs/PSX_DIRECTORS_CUT_ANALYSIS.md`.

> All stage overlays load at `0x80105400`. Addresses are MIPS virtual.

## Verdict

All seven are the same code recompiled (~90% normalised-instruction match, DC
`0x1000`–`0x2000` smaller each) **plus two shared additions** and some
stage-specific edits. Critically, **this is where the ADVANCED mode is
consumed**: every stage's init selects a mode-specific stage descriptor from
`g_status_flags` bit `0x20000` before handing control to the engine.

## Per stage

| Stage | entry OG → DC | text OG → DC | match | new `g_status_flags` refs | largest extra |
|---|---|---|---|---|---|
| STAGE1 | `80121edc`→`80120dd4` | `0x1d800`→`0x1c800` | 0.901 | 1 → 2 | +139w, +89w |
| STAGE2 | `8012173c`→`80120744` | `0x1d000`→`0x1c000` | 0.900 | 1 → 2 | +139w, +89w, −65w |
| STAGE3 | `80132718`→`80130378` | `0x2e000`→`0x2c000` | 0.898 | 0 → 2 | +196w, +89w |
| STAGE4 | `80133124`→`80131528` | `0x2e800`→`0x2d000` | 0.891 | 1 → 2 | +285w, −309w |
| STAGE5 | `8012dad4`→`8012c0dc` | `0x29000`→`0x27800` | 0.897 | 1 → 2 | +139w, +89w |
| STAGE6 | `80128ec0`→`80127b90` | `0x24800`→`0x23800` | 0.904 | 1 → 3 | +141w, +89w |
| STAGE7 | `8012bdd4`→`8012a750` | `0x27800`→`0x26000` | 0.900 | 1 → 3 | −158w, +89w |

(`+Nw`/`−Nw` = DC-only / OG-only code runs. The `+89w` block is in every stage;
`+139w` is shared by 1/2/5 and analogous in 3/6.)

Note **no stage references `g_abDcGameMode` (`0x800c8693`)** — the stage code
tests the raw msf2 bit, not the decoded id.

## 1. ADVANCED-mode stage descriptor (the mode consumer)

At the very start of every stage overlay (right after registering the stage's
task-table pointers), the DC inserts:

```c
// DC only, e.g. STAGE1 @ 0x801055bc
if (g_status_flags & 0x20000)            // ADVANCED
    *DAT_801fec00 = <ADV descriptor>;
else
    *DAT_801fec00 = <STD descriptor>;
```

The OG stores a single descriptor unconditionally. `DAT_801fec00` is the
resident work slot the engine reads (`0x80200000 - 0x1400`). The two
descriptors are adjacent overlay structures, one per mode:

| Stage | STANDARD descriptor | ADVANCED descriptor | Δ |
|---|---|---|---|
| STAGE1 | `0x8012f10` | `0x8012ff8` | `0xE8` |
| STAGE2 | `0x80120830` | `0x80120918` | `0xE8` |
| STAGE3 | `0x8013040c` | `0x8013049c` | `0x90` |
| STAGE4 | `0x801315e4` | `0x80131674` | `0x90` |
| STAGE5 | `0x8012c284` | `0x8012c334` | `0xB0` |
| STAGE6 | `0x80127ccc` | `0x80127db4` | `0xE8` |
| STAGE7 | `0x8012a83c` | `0x8012a924` | `0xE8` |

The descriptors sit in the overlay's BSS (past the loaded text), so they are
populated at runtime — this is the hook that gives ADVANCED its alternate
stage data. The OG equivalent table (e.g. STAGE1 `0x80122018`) is in the loaded
image and has no alternate.

This closes the open question from the earlier passes: ADVANCED is selected
**both** in `TITLE` (setting the bit) and here in every stage overlay (reading
it). The `STAGE8`–`STAGEE` room data is the engine-side counterpart (the stage
overlay itself is still `STAGE1`–`STAGE7`; there are no `STAGE8`–`STAGEE`
overlays).

## 2. Shared new entity code (every stage)

Two DC-only runs are common to all seven overlays:

- **`+139w` new function** (STAGE1 `0x8010cdf8`): a per-frame entity updater
  on `g_CurrentEntity` (`0x800cab14`). It drives a state machine on
  `entity[0x87]` (0 → 1), sets `entity[0xbe]=entity[0xbf]=0`, `entity[0x8c]=3`,
  `entity[0xbd]=0x1d`, computes `entity[0xc4] = (DAT_800c867a & 0xF) + 0x2D`,
  and in state 1 calls resident helpers and sets `entity[0xc2]=0x1E7` on
  success. These fields read like an **enemy AI/behaviour**.
- **`+89w` inserted block** (STAGE1 `0x80105b4c`): reads `g_CurrentEntity`,
  tests `entity[2] & 0xF` against `0xC`/`0xD`/`0xE`, and for each sets
  `entity[0x16b]=1`, masks `entity[2]`, and adds `0x64` to `entity[0x88]`.
  The three entity types (`0xC`, `0xD`, `0xE`) have no branch here in the OG —
  this is the insertion point for the new enemy variants.

## 3. Stage-specific edits

Beyond the shared blocks, each stage has a few unique added/removed runs — the
arrange-mode content edits:

- STAGE2: `−65w` at OG `0x801222fc` (OG code the DC dropped).
- STAGE4: `−309w` at OG `0x8011a5b8`, `+285w` at DC `0x80119a80` (largest
  rewrite), `+214w` near DC `0x801320a8`.
- STAGE6: `+141w` at DC `0x801289cc`, extra `g_status_flags` read at DC
  `0x80119cc4`.
- STAGE7: `−158w` at OG `0x8012c988`, extra `g_status_flags` read at DC
  `0x801150d0`.
- STAGE3: extra `g_status_flags` read at DC `0x80112bf4`.

Everything else — the `0x1000`-shifted `0x8012`/`0x8013` pointer tables (the
bulk of the "semantic" diffs above) and the `ori`→`addiu` / load-delay-slot
codegen — is non-semantic.

## Tooling note

Compared at the file level (Capstone sweep + sequence alignment); no stage
overlay was imported into Ghidra.
