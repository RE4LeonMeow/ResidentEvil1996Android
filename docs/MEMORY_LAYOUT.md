# Memory Layout & Global Placement Rules

**Read this before defining any new global.** The original binary accesses several
global clusters as *address ranges* (block wipes, block copies, pointer walks).
Our decompilation lets the MSVC linker place globals wherever it wants, so any
global that lands inside such a range by accident gets silently corrupted at
runtime — and any global that *should* be inside the range but isn't escapes
the operation. Both failure modes have already produced real, hard-to-diagnose
bugs (see [Incident history](#incident-history)).

## The placement decision

When you decompile a new global, check its **original address** against this
table before writing the definition:

| Original address range | Placement | Mechanism |
|---|---|---|
| `[0x00be41e0, 0x00be9620)` | game-init wipe block | `.gwipe$<tag>` ordered section (see below) |
| `[0x00be9620, 0x00be9a3c)` | bio card / save block | field of `BioCardLayout` + macro alias in `src/game/BioCard.h` |
| Task scheduler cluster (`0x00d91a68..0x00d91a90`, `g_TasksTable` 0x00d1fde4, `g_StackPointer` 0x007e0cc8) | `.sched` section | `__declspec(allocate(".sched"))` in `src/Globals.cpp` |
| Anything a range operation must **never** touch but the linker keeps placing at risk (historically `g_pMarniDirect3D`, `g_pMasterInputState`) | `.sched` section | same |
| Everything else | normal global | plain definition + original-address comment |

If you find a **new range-based operation** in decompiled code (a `memclr`,
`memset`, `memcpy`, or pointer loop whose start/end are two *different*
globals), stop and model the whole range explicitly with one of the mechanisms
below. Never leave it depending on linker luck.

The same applies to **past-the-end addressing**: original code that reaches a
neighboring data block via `&someArray[N]` (one past the end) works only
because of the original address layout. Never port it as-is — define a real
global for the neighboring block (sized from the access pattern / Ghidra) and
point the code at it. Example: `ComplexTmdObjectSetup` addressed the object
data area DAT_008ffcc0 as `&g_objectCountArray[32]`, which in our layout wrote
0x84-byte object entries over unrelated globals (see incident 4).

## Mechanism 1: `.gwipe` — the game-init wipe block

`InitializeGame` (0x004807a0) executes:

```c
memclr(&g_defaultItemSlot, g_BioCardData);   // original: wipes 0x00be41e0..0x00be9620
```

Every global whose original address is in `[0x00be41e0, 0x00be9620)` **must**
be allocated into the `.gwipe` section, tagged with the low 4 hex digits of its
original address:

```c
#pragma section(".gwipe$41e4", read, write)          // once, in the list at the
                                                     // top of src/Globals.cpp
__declspec(allocate(".gwipe$41e4")) Effect g_effectPool[MAX_EFFECTS] = {};
```

The MSVC linker sorts `$`-suffixed subsections **alphabetically** and
concatenates them into one final `.gwipe` section. Because the tags are
fixed-width lowercase hex, alphabetical order == original address order, so the
block reassembles itself correctly no matter which translation unit defines
which global.

Sentinels:

- `.gwipe$41e0` = `g_defaultItemSlot` — **first** byte, the memclr's begin.
- `.gwipe$9620` = `g_BioCard` — **exclusive end**; the memclr stops here and
  the bio card itself is *not* wiped.

Guarantees:

- No global outside `.gwipe` can ever land between the sentinels → nothing
  foreign is wiped.
- Every member listed is inside the sentinels → everything the original wiped
  is wiped.

### Current members

| `$tag` | Global | Original address | Size | Defined in |
|---|---|---|---|---|
| `41e0` | `g_defaultItemSlot` | 0x00be41e0 | 1 | Globals.cpp |
| `41e1` | `DAT_00be41e1` | 0x00be41e1 | 1 | Globals.cpp |
| `41e2` | `g_enemy_count` | 0x00be41e2 | 4 (orig 1) | Globals.cpp |
| `41e4` | `g_effectPool[64]` | 0x00be41e4 | 0x2100 | Globals.cpp |
| `62e4` | `g_playerEntity` | 0x00be62e4 | 0x180 | Globals.cpp |
| `6350` | `g_playerPosX` | 0x00be6350 * | 4 | Globals.cpp |
| `6358` | `g_playerPosZ` | 0x00be6358 * | 4 | Globals.cpp |
| `6368` | `g_playerAngle` | 0x00be6368 * | 4 | Globals.cpp |
| `6370` | `g_healthStatus` | 0x00be6370 * | 4 | Globals.cpp |
| `6380` | `g_playerBkpPosX` | 0x00be6380 * | 2 | Globals.cpp |
| `6382` | `g_playerBkpPosZ` | 0x00be6382 * | 2 | Globals.cpp |
| `6384` | `g_playerBkpHealthStat` | 0x00be6384 * | 4 | Globals.cpp |
| `6388` | `g_playerBkpAngle` | 0x00be6388 * | 2 | Globals.cpp |
| `63a0` | `g_firstItemSlotPointer` | 0x00be63a0 * | 4 | Globals.cpp |
| `63a4` | `g_totalHeldItems` | 0x00be63a4 * | 4 | Globals.cpp |
| `63a8` | `g_heItemsX2Less1` | 0x00be63a8 * | 4 | Globals.cpp |
| `63b0` | `g_itemSlotIndices[8]` | 0x00be63b0 * | 8 | Globals.cpp |
| `6464` | `g_EnemiesList[30]` | 0x00be6464 | 0x2e68 | Globals.cpp |
| `92cc` | `g_savedEnemyStates[16]` | 0x00be92cc | see Globals.cpp | Globals.cpp |
| `9614` | `DAT_00be9614` | 0x00be9614 | 1 | Globals.cpp |
| `961d` | `g_SpecialR1` | 0x00be961d | 4 (orig 1) | Globals.cpp |
| `961e` | `g_SpecialG1` | 0x00be961e | 4 (orig 1) | Globals.cpp |
| `961f` | `g_SpecialB1` | 0x00be961f | 4 (orig 1) | Globals.cpp |
| `9620` | `g_BioCard` | 0x00be9620 | 0x41c | Globals.cpp (END marker, not wiped) |

`*` = in the original binary these addresses overlay `g_playerEntity`'s range
(the decompilation gave them separate storage); they carry tags so they are
still wiped like the original bytes were.

Unmapped gaps in the original range hold bytes the original wiped but no
ported global uses. With the decomp function-complete, only one gap remains:
`0x00be9320..0x00be9613` (between `g_savedEnemyStates[16]` and `DAT_00be9614`)
— truly-unused scratch space in the original. If you ever name a global there
in Ghidra and add it to the code, it **must** get a `.gwipe` tag.

### Adding a member (checklist)

1. Confirm the original address is in `[0x00be41e0, 0x00be9620)`.
2. Add `#pragma section(".gwipe$<tag>", read, write)` to the list at the top of
   `src/Globals.cpp` (tag = low 4 hex digits, lowercase).
3. Prefix the definition with `__declspec(allocate(".gwipe$<tag>"))`.
4. Update the table above.

MSVC quirks: the section name must be a **single string literal** — you cannot
build it with macro string concatenation (`".gwipe$" tag` fails with C2341/C2059),
which is why every subsection is spelled out. A global defined in another .cpp
file can join the section the same way (the `#pragma section` must appear in
that file too); alphabetical `$` sorting keeps the order correct across TUs.

## Mechanism 2: `.sched` — protected survivors

Globals that a range operation must never touch, but whose original addresses
are *outside* any modeled block, live in `.sched` (declared at the top of
`src/Globals.cpp`). Current residents: task scheduler state (`g_TasksTable`,
`g_TasksESP/EIP`, `g_SchedulerESP`, `g_CurrentTask*`, `g_StackPointer`),
`g_pMarniDirect3D`, `g_pMasterInputState`.

Use `.sched` when a pointer or system-level struct keeps getting corrupted by a
block operation and its original address proves it should be far away from the
operated range.

## Mechanism 3: struct overlay — the bio card block

The save-game block `0x00be9620..0x00be9a3c` (0x41C bytes) is accessed both as
a unit (`memcpy(&g_BioCardData[0], ..., 1052)` when loading `bio_card.dat` /
save files) and as ~60 individual variables. It is modeled as one packed struct
`BioCardLayout g_BioCard` (`src/game/BioCard.h`, `static_assert`ed to 0x41C) with
`#define` aliases for every original symbol name (`g_stageId`, `g_RandSeed`,
`g_fading_state`, ...). Field order/offsets inside the struct are load-bearing —
never reorder; add new aliases at their correct offset.

Use this pattern when a block is **copied/serialized as a whole** and byte
offsets inside it matter (saves, file formats). Use `.gwipe`-style sections when
only *membership and ordering* matter (wipes).

## Known range-based operations

| Operation | Range (original) | Status |
|---|---|---|
| `InitializeGame` → `memclr(&g_defaultItemSlot, g_BioCardData)` | 0x00be41e0..0x00be9620 | modeled by `.gwipe` |
| bio_card.dat / save load `memcpy` (0x41C bytes) | 0x00be9620..0x00be9a3c | modeled by `BioCardLayout` |
| `ClearGameStateFlags` (GameInit.cpp): zeroes **7 DWORDs from `&g_main_state_flags`** | 0x00be41c0..0x00be41dc | **FIXED** — was a pointer walk over linker-placed globals; now an explicit clear of exactly the original members (`g_main_state_flags`, `g_main_state_flags2`, `g_spriteAnimActive/R/G/B`, `g_spriteAnimIntensity`; the unnamed scratch dwords at 0x00be41c8/41cc/41d8 have no port equivalent). See the comment in GameInit.cpp. |
| `InitJoysticks`: zeroes `pState+0x28..+0x3B28` | inside `g_pMasterInputState` | safe (single struct, internal offsets) |

## Verifying the layout

```powershell
# .gwipe should exist and span nearly the full wiped range (~0x5400; the
# original block is 0x5440 minus the unused gap)
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\<ver>\bin\Hostx64\x86\dumpbin.exe" `
    /HEADERS bin\Debug\residentevil.exe | Select-String -Context 0,3 "\.gwipe|\.sched"
```

For symbol-level checks, `dumpbin /SYMBOLS` on `obj\Debug\Globals.obj` shows the
`.gwipe$xxxx` subsection assignments.

## Incident history

Incidents 1–3 and 5 share one root cause — a range operation touching
linker-placed bystanders:

1. **Task scheduler state** — `g_SchedulerESP` zeroed mid-task, crashed
   `TaskYield` (led to the `.sched` section).
2. **`g_pMarniDirect3D`** — nulled mid-game, crashed the graphics readiness
   check (moved to `.sched`).
3. **`g_pMasterInputState`** (2026-07-20) — `keyMap` zeroed the moment gameplay
   started, killing ALL keyboard input in-game while title/character-select
   still worked (input is initialized before the wipe). Symptom was "the main
   menu doesn't open" because the START/menu key could never register. Fixed by
   moving to `.sched`, then superseded by modeling the whole wipe block as
   `.gwipe`.

Related, same class: `g_enemy_count` (0x00be41e2) sat *outside* the wipe block
while the original has it *inside* — enemy count was never reset on new game.
Membership errors cut both ways.

4. **Complex TMD object data area** (2026-07-20) — `ComplexTmdObjectSetup` and
   `ObjectList_Cleanup` addressed the original data block at 0x008ffcc0 as
   `(DWORD*)&g_objectCountArray[32]` (past the end of the count array, adjacent
   only in the original layout). Every processed TMD object wrote a 0x84-byte
   entry over whatever globals the linker placed next; after the `.gwipe`
   re-pack that became `g_objectDeletePtr`, which got stamped with vertex data
   (0x3030302C) → AV in `CreateTmdObjectInternal`. Fixed by defining
   `g_complexTmdObjectData[0x10800]` (0x008ffcc0) and
   `g_tmdObjectSlotAnimPtrs[250]` (0x00aabd6c, the table `g_objectDeletePtr`
   statically points to in the original — it was `NULL` in the decomp, which
   silently disabled TMD slot reuse).

5. **`ClearGameStateFlags` over-wipe** — the 7-DWORD pointer walk from
   `&g_main_state_flags` zeroed five linker-placed globals past
   `g_main_state_flags2` (in practice the fade/message scratch cluster).
   Behaviorally masked because `InitSoundAndFadeState` reinitializes most of
   them right after, but it was pure linker luck. Fixed by replacing the walk
   with explicit clears of exactly the original members (see
   [Known range-based operations](#known-range-based-operations)).

6. **Player-model spill past `EntityModelStorage`** (2026-09-08) — the same
   class again, this time from a *load* rather than a range operation.
   `LoadEntityModel` reads the whole player EMD into `g_entityModelBuffer`,
   which with `g_entityModelBuffer2` forms one 108544-byte region. The larger
   player models do not fit: `char11.emd` (Jill) is 112968 bytes, `char12.emd`
   112620, the costume variants up to 112664 — up to 4424 bytes of file data
   run past the region. In the original binary that tail lands in the
   animation buffer; in this decompilation it landed on `g_pMasterInputState`
   (Windows), overwriting `keyMap` and the joystick entries so the game saw a
   phantom gamepad and the character walked by itself, and on `s_assetIsJpn`
   (Linux), flipping `GetAssetVersion()` to the JPN tables and corrupting the
   message glyphs and item names. `EntityModelStorage` now carries a 16 KB
   `spillGuard` tail. **When a global is a load target, size it for the largest
   file the game actually ships, not for the original symbol's extent** — and
   re-check that size when a new asset version is added.

## Status note

The decompilation is function-complete: every global the original binary
references has been identified, named, and placed per these rules. The
mechanisms above remain load-bearing — if you ever add or move a global that
overlays an original range-operation address, follow the checklist.
