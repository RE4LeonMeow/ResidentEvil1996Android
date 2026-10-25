# Director's Cut item differences (vs OG PS1)

Item tables in the two PS1 builds, the new weapon/item slots, and the mixing
changes. Companion to `docs/PSX_DIRECTORS_CUT_ANALYSIS.md`.

> Addresses are MIPS virtual. Item ids are 1-based (`itemId`); every table is
> indexed by `itemId - 1`.

## Tables

| Table | OG | DC |
|---|---|---|
| item **name** pointers (`g_ItemsNamesTable`) | `0x8008fc5c` | `0x800901d8` |
| item **lookup records** (4 bytes/item) | `0x8008dc34` | `0x8008e18c` |
| name lookup fn (`message_item_name_lookup`) | `0x8003527c` | `0x80034b34` |

Both name tables are 77 entries (the fn bounds the alternate-name path at
`id-1 < 0x4d`), and the name lookup swaps to `g_UncheckedItemsNamesTbl` when the
record's flag byte (`+3`) has bit 7 clear **and** the `g_status_flags`-bank bit
is clear.

**Lookup record** (`[0]`,`[1]`,`[2]`,`[3]`): `[1]` is the item image/icon class,
`[2]` is the **combine-table index** (proven by the mixed herbs `0x49`–`0x4B`,
whose `[2]` is 32/33/34 as the PC comment states), `[3]` is the alternate-name
flag (`0x80` = primary name).

## Repurposed free slots

The DC's item changes are **not renames of live items** — Capcom recycled slot
ids that are unused in the OG build and gave them to the new Arrange items:

| id | OG defined as | OG status | DC |
|---|---|---|---|
| 4 | `COLT PYTHON` | unused (id 5 is the live Python) | new **Beretta** |
| 13 | `DUMDUM ROUNDS` | unused | `LOCKPICK` |
| 49 | `LOCKPICK` | unused (Jill's lockpick is a scenario flag) | **crest part** |
| 50 | *(empty)* | unused | **crest part** |

So the name/lookup diffs below are slot repurposing, and the OG build's live
item set is a subset of the 77 slots. (A direct "which ids are referenced"
scan needs the `item_model_set` record's item-id offset, which is not byte 0 —
not done here.)

## New Beretta — item **4**

Item 4 was the OG's unused duplicate Python (id 5 is the live one); the DC
recycled it:

| | OG | DC |
|---|---|---|
| name | `COLT PYTHON` | **`BERETTA`** |
| lookup record | `06 04 02 80` | **`0f 4b 02 80`** |

Class `[1]` is `0x06` (Python) → `0x0F`, the same class as the normal Beretta
(item 2 = `0f 02 00 80`), so item 4 is a **second handgun** with different data
(`[1] 0x4B` vs `0x02`). It is the **ADVANCED-mode starting weapon**: in
`SetInitialItems` (DC `0x8002c7a8`) the start list is `0x0F020001`
(item 2, 15 rounds) and, when `g_status_flags & 0x20000`,

```c
if ((g_status_flags & 0x20000) != 0) _local_8 = 0x0F040001;   // item 4, 15 rounds
...
DAT_800c8790 = (g_status_flags & 0x20000) ? 4 : 2;             // ammo/slot type
```

so ADVANCED gives item 4 instead of item 2. (`0x0F` is the same byte as the
handgun class; the two `0x0F`s are the packed `{01,00,04,0f}` list — item 1
knife + item 4 ×15.) The high insta-kill chance is not in the item table — it
lives in the per-weapon effect/damage data, still to dump.

## Mixable crest parts

The lookup record `[2]` (combine index) was **added** to three item slots and
the whole herb block shifted by +3 (three new recipes inserted at 26-28):

| id | name OG → DC | `[2]` OG → DC |
|---|---|---|
| 44 | MOON CREST | `0x80` → `0x1C` |
| 49 | `LOCKPICK` slot (unused) → **MOON CREST** | `0x80` → `0x1A` |
| 50 | *(empty slot)* → **MOON CREST** | `0x80` → `0x1B` |
| 67-75 | RED/GREEN/BLUE/MIXED HERBS | `0x1A…0x22` → `0x1D…0x25` |

An index of `0x80` means "not combinable"; the DC makes items 44/49/50
combinable and inserts three combine tables (indices `0x1A`,`0x1B`,`0x1C` =
26/27/28) before the herb tables.

## Crest mixing — resolved

The combine tables are the PC `g_ItemCombinePtrs`/`g_ItemCombineData` pair
(pointer table immediately followed by the data; each data table is a count byte
+ 4-byte `{otherItem, newForCursor, newForTarget, effect}` records). Located in
the PS1 builds:

| | OG | DC |
|---|---|---|
| pointer table | `0x8008db84` (35 entries) | `0x8008e0cc` (**38** entries) |
| data | `0x8008d9cc` | `0x8008df04` |

The DC's three extra tables are the **MOON CREST parts**:

| idx | owned by | contents | meaning |
|---|---|---|---|
| 26 (`0x1A`) | item 49 | `01 32 2C 00 00` | 49 + item `0x32`(50) → **44** |
| 27 (`0x1B`) | item 50 | `01 31 2C 00 00` | 50 + item `0x31`(49) → **44** |
| 28 (`0x1C`) | item 44 | `00` | assembled crest — no further combine |

So **items 49 and 50 are the two broken halves and mixing them yields item 44,
the MOON CREST** (both the parts and the result carry the name `MOON CREST` in
the DC's name table). That is the "broken Moon-crest parts" — there is no
separate MO-disk recipe; the herbs then run 29-37.

So the DC's crest items are assembled by mixing parts.

`id 13` (OG's unused `DUMDUM ROUNDS` slot) became `LOCKPICK`, and `id 4`'s name
as above — both repurposed free slots, not renames.

## Extra item-name strings (DC-only)

The DC item-name string block contains names with no OG counterpart — the
Arrange-mode extra key/file items:

```
MANSION KEY   DORMITORY KEY   SMALL KEY   LAB KEY   SPECIAL KEY   RED BOOK
CHEMICAL      CRANK
RESEARCHER'S WILL   KEEPER'S DIARY   ORDERS   PASS NUMBER   PLANT42 REPORT
FAX   SCRAPBOOK   SECURITY SYSTEM   RESEARCHER'S LETTER   "V-JOLT" REPORT
BARRY'S PICTURE   PASS CODE01/02/03   BOTANY BOOK
```

(The file/diary titles are the DC's collectible file items — in Arrange they are
carried rather than only read.) `CRANK`/`SQUARE CRANK`/`HEX. CRANK` and
`CHEMICAL` are also new/changed.

## New item-view art

`ITEM_M2` gains `I00V_S1.IVM`, `I60V_L.IVM`, `I60V_R.IVM`, `I99V.IVM` (OG has
`I00V`…`I73V`,`I75V`). The DC also merges the OG's 46 `FILEM_*.PIX` pages into
one `FILEM.PIX`.

An `.IVM` is the item's menu view: a TIM (its texture page) followed by a TMD
(its mesh). Rendering the four (the port's `tools/dump_ivm.py`), reading their
headers and checking the disc's file dates identifies three of them:

| file | date | contents | owner |
|---|---|---|---|
| `I00V_S1` | 1997-04-23 | the standard Beretta's mesh, flat-shaded (295 normals vs `I00V`'s 1316), re-textured silver + wooden grip | item 4, the **Beretta M92FS custom** (image type 0x4B) |
| `I60V_L` | 1997-02-25 | the assembled crest's TIM (`I60V`) with a left-half mesh | item 0x31, MOON CREST left half (image type 0x2E) |
| `I60V_R` | 1997-02-25 | as above, right half | item 0x32, MOON CREST right half (image type 0x2F) |
| `I99V` | 1997-02-25 | a second silver Beretta (`I00V`'s mesh and normals, different palette + texture) | **none found** — no DC item's image type resolves to it |

The names are the PS1 disc's, not the game's: the PS1 loads `ITEM_M2` by file
index and carries no name table at all (neither `SLUS_001.70` nor `SLUS_005.51`
contains an `iNNv`/`.IVM` string), so the ownership above comes from the image
types the DC's own lookup table assigns, not from the exe. See
`docs/DC_PORT.md` §3g.

## Weapon damage tables — one per difficulty

`apply_weapon_damage` = **OG `0x80011df8` / DC `0x800120e8`**. It walks
`g_EnemiesList`, calls the per-weapon hit-detection callback, then computes
`recIdx = weaponAdj*12 + enemyType*120` (`weaponAdj = weaponId - 1`,
`enemyType = entity->id`).

- **OG**: one damage table `0x8008bbe0`; damage is read at `+6`
  (`0x8008bbe6 + recIdx`). No mode split.
- **DC**: the knockback/type/hit fields still come from the base table
  `0x8008ac34`, but the **damage short is picked from one of three tables by
  `g_status_flags`**:

| mode | test | damage address | table base |
|---|---|---|---|
| STANDARD | neither bit | `0x8008ac3a` | `0x8008ac34` |
| TRAINING | `g_status_flags & 0x40000` (bit 18) | `0x8008b59a` | `0x8008b594` |
| ADVANCED | `(g_status_flags & 0x30000) == 0x20000` (bit 17) | `0x8008befa` | `0x8008bef4` |

The three tables are `0x960` (2400 B = 200 records × 12) apart. So the DC did
**not** add a table per difficulty wholesale — it only varies the damage column;
knockback and hit-state stay shared.

Verified zombie row (10 weapons: knife, handgun, shotgun, python, magnum,
flame, GL-exp, GL-acid, GL-flame, rocket):

| table | shotgun | magnum | GL-acid |
|---|---:|---:|---:|
| OG | 20 | 60 | 70 |
| DC STANDARD | 53 | 130 | 95 |
| DC ADVANCED | 34 | 60 | 70 |

(So DC STANDARD is *not* the OG table — the OG's shotgun `20` sits outside all
three DC columns; the DC rebalanced the weapon values.)

### ADVANCED Beretta insta-kill

In the same DC function, before the damage lookup:

```c
if ((g_status_flags & 0x20000) != 0 && DAT_1f800008 == 1 &&
    (&DAT_8008c854)[rand() & 0xf] != 0) {
    weaponAdj = 3; DAT_1f800008 = 3; DAT_1f800018 = 4;
}
```

`DAT_1f800008` is `weaponAdj`; `0x8008c854` is a 16-byte table
(`01 00 00 … 00 01 00` — nonzero at indices 0 and 14, so a **~2/16 = 1/8
chance**). In ADVANCED, firing the handgun (weapon index 1) has that chance to
be upgraded to **weapon index 3 with hit-state 4** — the one-shot-kill shot,
i.e. item 4's "insta-kill" behaviour. It only exists in the DC; the OG function
has no such branch.

## Player health and damage taken

**Max health differs by mode** (already in `PSX_DC_TITLE_OVERLAY.md`): the DC
`InitializeGame` writes `g_PlayerHealth`/`g_PlayerHealthMirror` from
`g_status_flags` —

| mode | bits | formula | health (char 0 / 1) |
|---|---|---|---|
| STANDARD | neither | `(charId&1)*-0x2C + 0x8C` | 140 / 96 |
| TRAINING | `& 0x40000` | `(charId&1)*-0x1E + 0xB4` | 180 / 150 |
| ADVANCED | `& 0x20000` | `(charId&1)*-0x1E + 0x64` | 100 / 70 |

**Damage taken is unchanged.** The enemy→player damage is hard-coded per attack
inside each enemy handler (e.g. contact `health -= 0x12`, plant42 `-= 0x19`),
with the usual `SCENARIO_FLAG_SECOND_PLAYTHROUGH` "hard" variant. Both checked
sites are byte-identical between OG and DC (only `ori`→`addiu`), consistent with
the enemy handlers being recompile-only. There is **no per-mode damage-taken
table or multiplier**: the DC's only player-side change is the max-health value.

So ADVANCED is harder via the lower 100/70 health (plus the lower ADVANCED
weapon damage / the extra enemies), not via enemies dealing more.

## Open items

1. Find the ADVANCED Beretta's effect (insta-kill) in the per-weapon
   damage/effect data.
2. Map the extra key/file strings to their item ids (the DC name block has
   names with no OG counterpart, but the 77-slot name table does not reference
   them — likely a separate file/document table).
