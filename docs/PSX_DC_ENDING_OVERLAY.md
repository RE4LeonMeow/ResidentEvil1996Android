# Director's Cut ENDING overlay (`PROG2/ENDING.EXE`) vs OG

Comparison of the Director's Cut (`SLUS_005.51`) ENDING overlay against the
original PS1 release (`SLUS_001.70`). Part of
`docs/PSX_DIRECTORS_CUT_ANALYSIS.md`.

> Both overlays load at `0x800e0000`.

## Verdict

**ENDING is 90.9% identical but the DC adds real mode-aware logic.** Unlike
SELECT and PROLOGUE, this overlay *consumes* the STANDARD/TRAINING/ADVANCED
choice: it saves `g_status_flags` (msf2), decodes its mode bits into
`g_abDcGameMode`, and runs ADVANCED-only unlock code. This is the first
downstream consumer of the mode after TITLE.

## Build

| | OG | DC |
|---|---|---|
| File size | 26624 | 26624 |
| Load address | `0x800e0000` | `0x800e0000` |
| Text size | `0x6000` | `0x6000` |
| PS-EX entry | `0x800e5cf8` | `0x800e5bc8` |
| Decoded words | 6144 | 6144 |
| Normalised instruction match | — | **90.9%** |

The DC stack frame is 8 bytes larger (`addiu $sp,$sp,-0x48` vs `-0x40`) because
of one extra local — the saved msf2 word (`0x34($fp)`).

## New mode logic (all DC-only)

### 1. Entry — save and mask the flags  (DC `0x800e00a4`)

```c
DAT_800e67d0   = g_status_flags;      // save msf2 (carries the mode bits)
g_status_flags = g_status_flags & 0x20080000;   // MSF2_RESET_KEEP_MASK
g_gameStateFlags |= 8;
```

The OG does only `g_gameStateFlags |= 8` — no save, no mask. `DAT_800e67d0` is
a **DC-only word** that holds the incoming msf2 (mode) for the rest of the
sequence; the OG's analogous byte, `DAT_800e67e0`, is unrelated and read with
`lbu`.

### 2. Decode the mode  (DC `0x800e1a84`)

```c
if (DAT_800e67d0 & 0x40000) g_abDcGameMode = 1;   // TRAINING
if (DAT_800e67d0 & 0x20000) g_abDcGameMode = 2;   // ADVANCED
if (DAT_800e67d0 & 0x10000) g_abDcGameMode = 3;   // ADVANCED variant
```

`g_abDcGameMode` is `0x800c8693`, written from DC `0x800e1aa4/1ac8/1aec`. The
OG has no reference to it. The bit→value mapping is the same one TITLE sets
(`docs/PSX_DC_TITLE_OVERLAY.md`), so this is a decode, not a new assignment.

### 3. ADVANCED-gated ending content  (DC `0x800e1914`)

```c
if (DAT_800e67d0 & 0x20000) {                 // ADVANCED
    if (DAT_800e66c0 == 6 || DAT_800e66c0 == 7) {   // ending id
        Flg_on(g_gameOptionsFlags, 0x7a);     // resident call, a1 = 0x7a
        // writes 5 into item-slot entries of the table at g_ItemSlotItemId (0x800c8784)
    }
}
```

### 4–6. Temporary msf2 manipulation

- DC `0x800e17d0`: if `DAT_800e67d0 & 0x20000`, set `g_status_flags |= 0x20000`
  around one resident call, then clear it (`&= ~0x20000`).
- DC `0x800e1af0`: save the live msf2, set `g_status_flags = DAT_800e67d0`
  (the saved mode), call a resident helper (`a1 = DAT_800e5ec4`, `a3 = 1`), then
  restore the live msf2.
- DC `0x800e1554`: if `DAT_800e67d0 & 0x20000`, increment the ADVANCED counter
  `DAT_800e66bc` and index a table at `0x800e5e0e` by the ending id
  (`DAT_800e66c0`).

Everything else is overlay-local table/object shifts (e.g. the object pointer
`0x800e65b8` → `0x800e648c`, the `0x5xxx` tables moved by ~0x124) and the
codegen differences seen in every overlay (`ori`→`addiu`, load-delay slots).

## Globals

| Global | OG refs | DC refs | Role |
|---|---:|---:|---|
| `g_status_flags` `0x800c3004` | 0 | 10 | msf2 — carries the mode; DC saves/masks/swaps it |
| `g_abDcGameMode` `0x800c8693` | 0 | 3 | mode id decode target |
| `DAT_800e67d0` | — | many | DC-only saved msf2 word |
| `DAT_800e66c0` | 1 (byte) | — | ending / scenario id used by the ADVANCED branch |
| `DAT_800e66bc` | — | — | ADVANCED completion counter |
| `g_gameOptionsFlags` `0x800c8700` | yes | yes | DC adds `Flg_on(...,0x7a)` |
| `g_ItemSlotItemId` `0x800c8784` | yes | yes | DC writes item-slot value `5` on the ADVANCED branch |

Unchanged and identical in both builds: `g_gameStateFlags` (12 refs),
`g_frameBufferIndex` (12), `DAT_800c301c` (5), `g_CharacterId` `0x800c5125` (7),
`DAT_800c8688` (4), `DAT_800cab0a` (2), `DAT_800cf63b/63c` (3/3),
`g_fade_type_id` (2).

## Tooling note

Compared at the file level (Capstone sweep + sequence alignment). Importing
ENDING into Ghidra failed to produce useful function boundaries — the DC entry
folded the whole overlay into one function (DC: 7 functions, OG: 22) — so
`/ENDING_DC.EXE` and `/ENDING_OG.EXE` in the project are **not reliable** for
decompilation. They were closed, but the bridge refused to delete them
("in use"); treat them as scratch if they appear.
