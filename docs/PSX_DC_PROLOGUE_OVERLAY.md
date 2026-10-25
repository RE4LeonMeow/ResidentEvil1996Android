# Director's Cut PROLOGUE overlay (`PROG2/PROLOGUE.EXE`) vs OG

Comparison of the Director's Cut (`SLUS_005.51`) PROLOGUE overlay against the
original PS1 release (`SLUS_001.70`). Part of
`docs/PSX_DIRECTORS_CUT_ANALYSIS.md`.

## Verdict

**PROLOGUE is semantically identical.** The two overlays are the same code; the
DC build only differs by *compiler code generation*, with **no value, control
flow, global or option changes at all**.

## Build

| | OG | DC |
|---|---|---|
| File size | 4096 | 4096 |
| Load address | `0x800e0000` | `0x800e0000` |
| Text size | `0x800` | `0x800` |
| PS-EX entry | `0x800e02f4` | `0x800e02b4` |
| Body (`jal` target at start) | `0x800e02ec` | `0x800e02ac` |
| Normalised instruction match | — | **94.7%** |

The text is padded with `nop`s to the full `0x800`, so both files stay 4096 B
even though the DC code is `0x40` bytes shorter.

## What differs (all codegen)

1. **16 load-delay `nop`s removed.** The OG code has 39 `nop`s in the code
   region, the DC 23. Every removed one was a load-delay slot after a `lw`
   /`lbu`; the DC compiler fills the slot with the following independent
   `lui` instead. Example, the resident-callback swap:

   ```
   OG 800e0018  lui $v0,0x8020       DC 800e0018  lui $v0,0x8020
       800e001c  lw  $v0,-0x156c($v0)     800e001c  lw  $v0,-0x156c($v0)
       800e0020  nop                      800e0020  lui $at,0x8020   <- slot filled
       800e0024  lui $at,0x8020           800e0024  sw  $v0,-0x13e8($at)
       800e0028  sw  $v0,-0x13e8($at)     800e0028  jalr $v0
   ```

   This is a pure scheduling change; `lui` does not depend on the load.

2. **`ori $r,$zero,N` → `addiu $r,$zero,N`** for the small constants
   (`0xf0`, `0x1e0`, `0x140`, `1`), same as SELECT.

3. **Entry/GP shifted by the shorter code**: `jal` target `0x800e02ec` →
   `0x800e02ac`, `lui $gp,0x800e; addiu $gp,$gp,0x304` → `0x2c4`.

A semantic diff over the aligned instructions finds **one** "value difference",
and it is only the shifted GP base in the entry stub — nothing else.

## Behaviour (unchanged)

The body does exactly the same thing in both builds:

- swaps `DAT_801fec18` through the overlay callback table (`0x8020xxxx`) and
  `jalr`s the resident helpers — same sequence and same slots;
- stores `0x80105400` to `DAT_800cab1c`;
- reads `g_frameBufferIndex` (`0x800c3010`) and builds the `0x140`/`0xf0`
  sprite rect (`sh 0x18/0x1a/0x1c/0x1e($fp)`);
- writes `DAT_800cf63b = 1` and `DAT_800cf63c = DAT_800cab1c`;
- sets `g_gameStateFlags` (`0x800c3000`) bit `4` then bit `8`.

None of these differ between builds, and there is **no reference to
`g_status_flags` (`0x800c3004`) or `g_abDcGameMode` (`0x800c8693`)** — the mode
is irrelevant to this screen.

## Tooling note

Compared at the file level (Capstone sweep + sequence alignment). Not imported
into Ghidra: there is nothing new to name.

## Pattern across overlays

The DC's toolchain consistently (a) fills MIPS load-delay slots instead of
emitting `nop`s and (b) prefers `addiu`/negative immediates over
`ori`/`+0xff` forms. Both show up in TITLE, SELECT and PROLOGUE, so expect them
in the remaining overlays and treat them as non-semantic.
