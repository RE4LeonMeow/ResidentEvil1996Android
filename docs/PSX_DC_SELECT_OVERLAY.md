# Director's Cut SELECT overlay (`PROG2/SELECT.EXE`) vs OG

Comparison of the Director's Cut (`SLUS_005.51`) SELECT overlay against the
original PS1 release (`SLUS_001.70`). Part of
`docs/PSX_DIRECTORS_CUT_ANALYSIS.md`; TITLE is in
`docs/PSX_DC_TITLE_OVERLAY.md`.

> Both overlays load at `0x800e0000`. Addresses are MIPS virtual addresses.

## Verdict

**SELECT is functionally unchanged.** The DC build is the same code recompiled:
87% of the normalised instruction stream matches, and a diff over the whole
overlay finds **no function-sized insertion or deletion** and **no new logic,
options or flag handling**. The STANDARD/TRAINING/ADVANCED choice lives entirely
in `TITLE.EXE` (see `PSX_DC_TITLE_OVERLAY.md`) and is consumed by the resident
`InitializeGame`; SELECT never reads the mode.

## Build

| | OG | DC |
|---|---|---|
| File size | 24576 | 22528 |
| Load address | `0x800e0000` | `0x800e0000` |
| Text size | `0x5800` | `0x5000` |
| PS-EX entry | `0x800e51ac` | `0x800e4d80` |
| Decoded words | 5632 | 5120 |
| Normalised instruction match | — | **86.9%** |

## What actually differs

1. **Recompile encodings** — the same operations emitted differently:
   `ori $r,$zero,N` → `addiu $r,$zero,N`, and `addiu $r,$v,0xff` (stored back as
   a byte, i.e. `-1` mod 256) → `addiu $r,$v,-1` (`0xfd` → `-3` likewise). These
   are byte-for-byte equivalent in context (`lbu` … `addiu +0xff` … `sb`).

2. **Overlay-local tables and BSS shifted down by `0x420`** — every
   `lui $r,0x800e; addiu $r,$r,TABLE` base moved together:
   `0x5260`→`0x4e40`, `0x54a0`→`0x5080`, `0x5300`→`0x4ee0`, `0x5540`→`0x5120`.
   The tail data region is byte-identical once aligned by that shift.

3. **Two file loads decremented by one.** The only two semantic value changes
   in the whole overlay are `LoadFile` indices:

   | OG `0x800e00e4` / `0x800e013c` | DC `0x800e00d8` / `0x800e0128` |
   |---|---|
   | `ori $a0,$zero,0x16` | `addiu $a0,$zero,0x15` |
   | `ori $a0,$zero,0x17` | `addiu $a0,$zero,0x16` |

   Both feed the same call (`a0 = index`, `a1 = 0x801d10ec`, `jalr` — the
   resident file loader). This is the **same one-less shift seen in TITLE**
   (`0x1b`→`0x1a`): the DC's file-system table lost/added entries ahead of these
   files, so equal-content files sit at index − 1. It is not a new option.

## No mode handling

`SELECT.EXE` has no reference to `g_status_flags` (`0x800c3004`, the msf2 bank
the DC modes use) or to the DC-only `g_abDcGameMode` (`0x800c8693`). Its only
resident-global references are identical between builds and unrelated to the
mode: `g_gameStateFlags` (`0x800c3000`, 9 sites each), `g_frameBufferIndex`
(`0x800c3010`, 93), `DAT_800c301c` (3), `DAT_800c868b` (1) and `g_fade_type_id`
(`0x800cf86f`, 3).

So the DC NEW GAME flow is: `TITLE` sets the STANDARD/TRAINING/ADVANCED bits →
schedules SELECT (`0x12d`; OG `0x14c`) → SELECT runs the OG character/start
sequence unchanged → `InitializeGame` reads the bits.

## Tooling note

The comparison above was done at the file level (Capstone sweep + sequence
alignment); SELECT was **not** imported into Ghidra because there is nothing to
name — renaming it would just duplicate the OG names.
