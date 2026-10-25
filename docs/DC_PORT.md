# Director's Cut port

Status: **implemented and playable.** The PS1 Director's Cut (`SLUS_005.51`)
content and behaviour are ported into the PC decomp, selected by one
`config.ini` key (`[Game] Mode`). With `Mode=OG` the executable is the PC
release and behaves exactly as it did before any of this existed.

Reference for every DC behaviour below: `docs/PSX_DC_*.md` (title/modes, items,
enemy AI, stage overlays, audio) and `docs/PSX_DIRECTORS_CUT_ANALYSIS.md`.

---

## 0. Design: one key, an overlay, and a mode-exclusive folder

- **Base trees are complete and never written to.** `assets/USA/` and
  `assets/JPN/` stay exactly as the retail discs ship them.
- **A mode owns its content.** `assets/DC/` is laid out like a base tree and
  holds the DC's own assets under their *ordinary* names —
  `assets/DC/Data/title.pix` is the DC's title art. It is **not** a sparse diff:
  in DC mode the game reads the DC's files, including the rooms the DC did not
  change, so that OG stays vanilla and a session never silently mixes the two.
  The source is the player's own disc, extracted to `assets/PSX_DC/`, which the
  tools convert into the tree.
- **Resolution is mode tree first, base second.** `ResolveAssetRoot()`
  (`src/system/AssetPath.cpp`) probes the mode tree and falls through to the
  base only for the folders a mode genuinely cannot supply. All six readers —
  `LoadFile`, `SoundApi`, `MarniSound`, `PSXTexture`, the Linux audio backend
  and `VideoPlayback` — funnel through that one function, so nothing else
  changed. With no mode active the probe does not run at all and OG is
  byte-for-byte what it was.
- **What legitimately falls back**, and why it is only these (measured against
  the disc, not chosen):

  | folder | why |
  |---|---|
  | `sound`, `voice` | the PS1 keeps samples in VAB banks and streams speech as XA. **547** of the PC tree's Sound names and **563** of its Voice names have no DC counterpart at all; the DC's audio is identical to the USA release's anyway |
  | `movie` | PS1 `.STR`, which the port cannot play, and 8 of the PC's 27 movies have no DC counterpart |
  | `effspr`, `objspr` | PC-only asset classes — the DC disc has no such folder |
  | `item_m2/filem_`, `item_m2/arror` | the readable documents: the DC's single `FILEM.PIX` is 46 pages, every one byte-identical to a PS1 OG page, so the DC adds no document art. The PC's per-file containers hold the same pictures and already load |

  Everything else the DC does own, so a miss there means the mode tree is
  incomplete and the player is about to see OG content in a DC session.
  `ResolveAssetRoot` logs each one (`[assets] DC has no <path> - falling back`)
  rather than passing it over silently. Three folders are only partly covered
  and so log a known shortfall: **Data** (37 PC names — fonts, menu sheets,
  vendor logos), **Item_m2** (20), **Players** (2: `w08`, `w18`). `Enemy` and
  `Item_m1` cover every PC name outright.
- **One key selects both the code and the assets.** `[Game] Mode = OG | DC |
  SATURN | DS` sets `g_GameMode` *and* the overlay folder. Two keys would let
  the executable boot DC code against OG assets, which looks like a content bug
  and is not one. `g_bDcMode` is a macro over `g_GameMode`, so every branch
  reads the same and an accidental assignment fails to compile. `OG` is the PC
  release, i.e. the original PS1 game's content, and uses no overlay.
- Everything DC-specific is gated by `g_bDcMode` or by the DC's own in-file mode
  flags (§2e). With `Mode=OG` the executable behaves exactly as before.
- No stubs: where the DC changed data, the data is ported; where it changed
  code, the branch is ported.
- **Mode-exclusive code lives in its own folder**: `src/game/dc/`, with the
  `Dc` prefix dropped from the file names (`dc/ItemTables.cpp`,
  `dc/WeaponDamageTables.cpp`, ...). A future `src/game/saturn/` follows the
  same shape.

---

## 1. Foundation — the mode key

| PC file | what it holds |
|---|---|
| `src/Globals.h` / `Globals.cpp` | `g_GameMode` (`GAME_MODE_OG/DC/SATURN/NDS`), `g_bDcMode` as a macro over it, and `g_DcDifficulty` (0=STANDARD, 1=TRAINING, 2=ADVANCED, 3=ADVANCED*) |
| `src/system/ConfigFile.cpp` | reads `[Game] Mode` (default `OG`) in `ConfigFile_Load` and calls `SetAssetMode`; preserves it on save. A pre-`Mode` file's `DcMode=1` is still honoured, and writing the file back replaces it. The name doubles as the overlay folder (`kGameModeNames`), so a mode name is spelled in exactly one place |
| `src/system/AssetPath.h/.cpp` | `SetAssetMode()` / `GetAssetModeName()` + the overlay probe in `ResolveAssetRoot` (§0) |
| `config.ini` | documents `[Game] Mode` in the generated default file.

No `GetAssetVersion()` change was needed: the overlay sits *on top of* whichever
base tree `[Assets] Version` picks, so the existing `!= 0` JPN branches are
untouched and a DC overlay works over either base.

An unknown or missing `Mode` name falls back to OG rather than refusing to
start. The engine logs the active mode and overlay at startup
(`[CONFIG] mode=... overlay=...`) and logs which tree each top-level asset
folder resolved from (`[assets] DC/<folder> -> overlay|BASE TREE`).

---

## 2. Difficulty modes and player stats

### 2a. Mode selection (title screen)

`src/game/TitleScreen.cpp` — the title screen, **not** the in-game main menu.
PS1 ref `docs/PSX_DC_TITLE_OVERLAY.md`.

- DC flow — same shape as the USA plus one level: attract draws the DC title
  background and frame 0 (`PRESS ANY BUTTON`), any button enters the menu; the
  menu draws the NEW GAME / LOAD GAME frames; confirming NEW GAME opens the
  **STANDARD / TRAINING / ADVANCED** submenu (PS1 `g_titleMenuChoice` 3/4/5,
  frames 3/4/5), and holding on ADVANCED selects the green frame 6
  (`g_DcDifficulty` 3, ADVANCED* — the double-ammo mode).
- Wired into the PC state machine (`init_title_screen` / `update_title_options`
  / `g_titleOptionsFading`): `TitleTextPosData` gained a `slot` field and the
  table is mode-selected (`g_titleTextPosTableUsa[3]` /
  `g_titleTextPosTableDc[7]`; the DC rows use `screenY 0` so the existing +38
  offset matches the PS1's fixed screenY 38). `update_title_options` gained
  state 10 for the submenu; confirm writes `g_DcDifficulty` before running the
  original new-game exit.
- The hold is **Right** on the raw d-pad word (`0x2000`). Note
  `g_PlayerPadHeld` is the *edge-detected* word in this port, so the continuous
  source is `g_button_pressed_id` (== `g_RawPadHeld`); using the wrong one makes
  the hold advance a single frame.
- The title sfx plays where the PS1 plays it: the OG plays "Resident Evil" on
  the NEW GAME *or* LOAD GAME confirm, while the DC is silent on NEW GAME (it
  only opens the submenu) and plays it on the *submenu* confirm, or on LOAD
  GAME.
- `title_state`'s exit switch had `case 1` (NEW GAME) fall through into
  `case 2/3` (the interactive load screen); the DC path returns after chaining
  the character select, and the submenu keeps its selection 3..5 through the
  exit fade so the difficulty cell stays on screen. The `case 1` change is gated
  on `g_bDcMode` so the USA path is untouched.
- STANDARD reproduces the USA new-game exactly.
- Assets: the DC title background is `assets/PSX_DC/DATA/TITLE.PIX`, which is
  byte-different from the USA/PS1-OG image (6461 bytes), shipped as
  `assets/DC/Data/title.pix` (the USA `title.pix` is left untouched because it
  has no in-file gate). The option cells are read from the disc's own
  `DATA/BT367OAB.TIM` and assembled in memory by `dc_title_build_page`
  (`TitleScreen.cpp`) — the engine splits the seven 80-row cells across two
  texture pages (`Data/t_dc.tim` shape: chunks 0/1/2 at vramY 0/80/160; the
  second page carries chunks 3/4/5/6 cropped to 64 rows plus the normal and
  green ADVANCED* palettes). The split is necessary because
  `TextureDesc.texV` is a byte and can only address 256 rows, against the 560
  the seven cells need. `tools/verify_dc_title_sheet.py` re-implements
  `dc_title_build_page` in Python and checks the two pages against the digests
  of the sheets that were rendered and confirmed against the PS1 frames.

### 2b. Player health

`src/game/GameStart.cpp`, `InitializeGame`. When `g_bDcMode`, the starting
health is set from the mode (PS1 `InitializeGame`):

| mode | Chris | Jill |
|---|---|---|
| STANDARD | 140 | 96 |
| TRAINING | 180 | 150 |
| ADVANCED / ADVANCED* | 100 | 70 |

Max health follows it. The DC has a *second* mode block later in
`InitializeGame` (after the new-game/continue split) writing
`g_playerEntity.maxHealth` (PS1 `DAT_800c5299` = entity `+0x175`) with the same
three formulas, and `room_set`'s Rebecca swap gets per-mode values too
(Rebecca 88 / 140 / 64; restore 140 / 180 / 100, flat, no character adjust).

The EKG then needs the DC's clamp (`SLUS_005.51 0x80054708`): its state is
`(health - 1) / (maxHealth >> 2)`, and the shift drops the remainder, so a full
bar at a max that is not a multiple of 4 divides out to 4 — the POISONED colour
row and face. Jill ADVANCED (69/17) and Jill TRAINING (149/37) both hit it. The
USA build has no clamp because 140/96/88 all land on 3.

### 2c. Weapon damage

`src/game/WeaponDamage.cpp`. The DC's three damage columns (dumped from PS1
`SLUS_005.51` `0x8008ac34` / `0x8008b594` / `0x8008bef4`) are emitted into
`src/game/dc/WeaponDamageTables.{h,cpp}` by `tools/gen_dc_damage_tables.py`,
which verifies the STANDARD column equals the port's USA table for all 200
records and reports 42 / 71 differing records for TRAINING / ADVANCED.
`apply_weapon_damage` selects the column from `g_DcDifficulty` (STANDARD and
ADVANCED* share the original column) and skips the second-playthrough table in
DC mode (the PS1 has none).

The DC also adds an ADVANCED Beretta insta-kill (PS1 `0x800120e8`): when
ADVANCED and `weaponAdj == 1`, `table_8008c854[rand() & 0xF]` (~1/8) upgrades
the shot to **slot 3** (item 4, the Beretta M92FS custom) with a hit-state base
of 4. The port's table labels are `python (DumDum)` for slot 3 and
`python (magnum)` for slot 4.

### 2d. Damage taken — unchanged

The DC does not alter enemy→player damage (verified). The tables stay as the USA
port has them.

### 2e. The ADVANCED mode gate in the RDTs

`src/game/GameStart.cpp` — `InitializeGame`. The DC's RDTs choose their room
content with a flag inside the scripts:
`if (g_main_state_flags2 & 0x00020000)`, where **clear runs the original USA
content and set runs the arrange/Advanced content**. Verified on
STAGE1/ROOM1030: `04 bit_test` bank 5, sel `0x2E`, cond 1 — bank 5 + `off 4` is
the second dword (`g_main_state_flags2`), mask `0x00020000`; `cmd_bit_test`
returns `bit ^ cond`, so `cond 1` makes the then-branch (the USA's three
`enemy_set` records) run while the bit is **clear**. STAGE1/ROOM1050's event
shows the same shape (`if[0a]; bit_test[05 2E 01]; message_set[90]; else;
message_set[9C]`).

Bit 17 is the DC's **ADVANCED** flag (the same bit the weapon-damage and health
code selects on). The DC put all its arrange room content only in ADVANCED;
STANDARD and TRAINING take the original branch. `dc_apply_mode_flags()`
derives the three free bits from `g_DcDifficulty`:

- `MSF2_DC_ADVANCED` (bit 17) — ADVANCED / ADVANCED*
- `MSF2_DC_TRAINING` (bit 18) — TRAINING
- `MSF2_DC_ADVANCED_HOLD` (bit 16) — ADVANCED*

The DC's repurposed RDTs test ADVANCED at 190 sites and TRAINING at 3. The bits
are **not** in `MSF2_RESET_KEEP_MASK`, so a fresh game start clears them and
`dc_apply_mode_flags()` re-applies them every time.

### 2f. The DC has no first-playthrough gameplay difficulty

The USA build scales enemy damage and the ink-ribbon rules off
`SCENARIO_FLAG_SECOND_PLAYTHROUGH` (0x7B): clear = first playthrough (Jill saves
without a ribbon, the main-hall ribbon is dropped, enemies hit for less), set =
cleared-once hard mode. The DC replaced that whole axis with the title's
STANDARD/TRAINING/ADVANCED choice, so its gameplay code has **no
first-playthrough branch left**:

- `cmd_item_model_set` (`SLUS_005.51 0x80045f28`) has no Jill ink-ribbon skip
  and no `modelPtr[0] = 0` hide;
- `check_typewriter` (`0x800385fc`) always shows "no ink ribbon" (0xde) when
  none is held;
- `check_typewriter_state` (`0x80038d04`) only ever shows message 0xdf;
- `InitializeGame` (`0x8002958c`) has no Jill first-run block;
- `check_desk_state` (`0x8003816c`) has no guardhouse-003/Jill reset;
- the STAGE1 overlay contains no read of `g_gameOptionsFlags` bit 0x7B at all,
  and the DC zombie bite loads the **hard** damage table unconditionally.

The flag survives only as the ending's "already cleared once" marker (backdrop
and congratulations FMV). The port therefore **sets
`SCENARIO_FLAG_SECOND_PLAYTHROUGH` on the game-init path in DC mode**
(`InitializeGame`, after both the new-game and continue branches, skipped for
attract demos), so every existing USA branch takes the hard path. No gameplay
call site changed. `Mode=OG` is untouched.

The DC ending must not see the forced flag: the DC's ENDING overlay never
*reads* 0x7B (its only 0x7B ref is the `Flg_on` that marks the next cycle
cleared, `0x800e1a74`), so the USA/PC cleared-once backdrop/congrats-FMV swap is
a DC-dropped behaviour. `EndingScreen.cpp` guards its two 0x7B reads with
`!g_bDcMode`, keeping a first DC playthrough on the first-run art.

This is also what makes a DC Jill session playable: with the ribbon skipped, the
main hall's room-action slot kept stale data, so inspecting the typewriter next
to it took "an Explosive Round" and the typewriter itself could not be used
until the room reloaded.

---

## 3. Items

PS1 ref `docs/PSX_DC_ITEMS.md`. The DC **repurposes free USA item slots**.

### 3a. Repurposed item ids

Verified in-game against the emulator; every other id is identical between the
two builds:

| id | USA | Director's Cut |
|---|---|---|
| `0x04` | Colt Python, DumDum rounds (unobtainable) | **Beretta M92FS custom** — the ADVANCED starting handgun |
| `0x0D` | DumDum rounds | **LOCKPICK** (name-only: model, description and slot sprite stay the DumDum rounds') |
| `0x31` | Lock Pick (unused) | **MOON CREST left part** |
| `0x32` | Oil (unused) | **MOON CREST right part** |
| `0x4C` | Pick Axe model (unused; "nothing important" text in both) | same text, **Com. Radio model** |

Mixing `0x31` + `0x32` yields `0x2C` (`ITEM_MOON_CREST`, item 44) — which is why
the DC's combine table gains the crest recipes and the herbs' combine index
shifts +3. `Types.h` records these DC meanings in comments, and two
long-standing port naming errors were fixed along the way: `0x46`
`ITEM_MIX_BLUE_RED` -> **`ITEM_MIX_GREEN_RED`** and `0x4B`
`ITEM_MIX_2GREEN_RED` -> **`ITEM_MIX_2GREEN_BLUE`**. The id *numbers* in the
code (the `ITEM_OIL < id` category comparisons) are unchanged: the DC's own
code uses the same boundaries.

`src/game/dc/Items.h/.cpp` owns the two derivations that change with the ids:
`lockpick_item_id()` / `is_lockpick_item()` (§3i) and
`weapon_ammo_item_id()` (§3i).

### 3b. Item tables

`tools/gen_dc_item_tables.py` emits `src/game/dc/ItemTables.{h,cpp}`.

The PS1's item tables are **1-based** (`FUN_80042410` reads
`(&DAT_8008e18c)[(itemId - 1) * 4]`) and, exactly like the PC build, the
**image-lookup table overlaps the max-quantity table by one byte** — PC
`g_ItemMaxQty`/`g_ItemImageLookupTable` are 0x004bd81c/0x004bd81d, PS1
`0x8008E18C`/`0x8008E18D`. So a lookup record's 4th byte is the next
max-quantity record's 1st byte.

The DC's table is one record **shorter** than the port's (its records run
1..0x4D; item 0x4E is past its end) and has no dummy item 0, so its records sit
4 bytes earlier and its *tail* tables another 4 earlier still:

| what the engine reads | port (`g_ItemImageLookupTable`) | DC (`0x8008E18D`) |
|---|---|---|
| record for item id | `id * 4` | `(id - 1) * 4` |
| use-category thresholds (10) | `0x13B` | `0x133` |
| `heal[itemId]` | `0x10A + itemId` | `0xFE + itemId` |
| examine messages (16) | `0x15B + flagIndex` | `0x14F + flagIndex` |

A single shift cannot place both ends (8 vs 4 bytes), so the generator re-bases
the DC's records onto the port's own array **item by item, one 4-byte record at
a time**, keeping the port's tail (the DC moved the tables, it did not change
them) and taking only the DC's records. `dc_apply_item_tables()`
(`GameStart.cpp`) copies the lookup + combine data in and re-points the combine
table. The port's `g_ItemImageLookupTable` is non-const, and
`g_ItemCombinePtrs`/`Data` are sized for the DC (38 / 454; USA data in the first
35 / 440).

The DC's records match the port's except for the real edits: item 4
`04 02 80 06` -> `4B 02 80 06` (the Beretta's new view art), the crest parts
0x2C/0x2E/0x30/0x31/0x32 gaining combine indices, and the herb block
0x43..0x4B shifting +3. The generator asserts the thresholds, the heal values
and every item id's use category; `verify_dc_item_models.py` re-checks them
against the shipped file. The engine reads the thresholds and heal tables **out
of this array** (`menu_item_use_item` `0x00401070`, `menu_item_use_heal`
`0x00401260`), so getting the re-basing wrong filed the first aid spray under
"unusable" and the Beretta under "heal".

`g_ItemMaxQty` overlaps the image lookup by one byte in both builds, so the DC's
is simply the DC lookup shifted by one — `dc_apply_item_tables()` derives it, no
separate extraction is needed. Both tables are non-const.

### 3c. Starting items

Verified against PS1 `0x8002c7a8`. For `g_status_flags & 0x20000`
(ADVANCED / ADVANCED*) the PS1 overrides the start list's second entry and the
Rebecca slot to **item 4** (the Beretta M92FS custom) while keeping the 15
rounds; TRAINING and STANDARD keep item 2. The port mirrors that after copying
the slots, so the plain Beretta is swapped for item 4 across the starting
inventory (both characters, plus Rebecca). `dc_apply_mode_flags()` must run
*before* `InitPlayerData`/`SetInitialItems` for the bit to be visible.

### 3d. Item names

`tools/gen_dc_item_names.py` -> `src/game/dc/ItemNames.{h,cpp}`, emitted in the
port's own style — decoded `STR("NAME\x07")` constants plus the pointer table,
not raw byte strings. The generator takes each name's text from the DC's table
where it decodes cleanly, falling back to the port's own constant (read from
`MenuData.cpp`) only where the DC's bytes hit a gap in the project's glyph
table.

The DC's name pointer table (SLUS_005.51 `0x800901D8`, 1-based like the port's)
differs in the repurposed ids — `0x04` COLT PYTHON -> **BERETTA**, `0x0D`
DUMDUM ROUNDS -> **LOCKPICK**, `0x31`/`0x32` -> **MOON CREST** — plus the
key/file names in the table's tail. `message_item_name_lookup` picks the DC
table on `g_bDcMode` over a USA base.

The table also has **no sub machine gun entries**: entries 110/111 (ids 0x6F
INGRAM and 0x70 MINIMI, both PC-exclusive) point at the same stale string,
`0x8008FEF6` = **CRANK**, and the generic-name group that follows starts two
entries early (0x71 CHEMICAL where the USA table has 0x71 CRANK). Nothing in
the DC ever reads those two ids, but this port keeps the PC weapons in DC mode
— the ending hands them out (`EndingScreen.cpp`) and the item viewer loads the
USA `item_m2` models — so `dc_usa_item_names` (`Rendering.cpp`) copies the DC
table and puts the USA names back on 0x6F/0x70. Without it both weapons were
labelled CRANK. The Japanese table has the two names already, so DC+JPN needs
no such fix.

Over a JPN base it does **not** switch to the English table: `dc_jpn_item_names`
(`Rendering.cpp`) applies the same four relocations to the JPN table using the
JPN strings the DC moved to — 0x02 `ベレッタ`, 0x31 `キーピック`, 0x2C
`ムーンクレスト` — so DC+JPN stays Japanese. Language is chosen first, the DC
edit second.

### 3e. Item descriptions

`tools/gen_dc_item_descriptions.py` -> `src/game/dc/ItemDescriptions.{h,cpp}`.

The DC's table is **not** named in either PS1 program, so it was found by string
search — the port's item 0x2C text "A carving of the moon" appears at
`0x8008E769`, and searching for that pointer located the table. The 79 pointers
start at **`0x8008E9C4`** (entry k = item k+1, item-ordered) with the strings in
`0x8008E2xx-0x8008E9C4`. Extracted and validated three ways against the port:
item 0x02 "Beretta M92FS. / loaded with 9mm bullets.", item 0x03 "Remington
M870. / A pump-action shotgun." and item 0x2C "A carving of the moon." all
match byte-for-byte. The DC's real differences: item 0x04 =
**"A beretta M92FS Automatic. / Custom edition."** and the two crest halves
(0x31/0x32) = **"Nothing important."** (the DC's own text; its table points
items 0x31/0x32 at the same string as items 0x33..0x36).

The DC's table has no sub machine gun text either: its last real entry is
index 0x4C "The battery is still charged." and 0x4D/0x4E — the two indices the
item viewer passes for the INGRAM / MINIMI examine — point outside the string
pool (`0x80080F3C` / `0x80031604`), so the generator's tail fallback repeated
0x4C and both weapons read "The battery is still charged." in DC mode.
`dc_usa_item_descriptions` (`RoomInit.cpp`) copies the DC table and restores
the USA entries 0x4D "A sub machine gun loaded with 9mm bullets." and 0x4E "
A full-automatic light-weight machine gun."; the JPN table already has its own
pair there, so DC+JPN needs no fix.

Emitted in the port's `STR()` style (`.`=0x79, `-`=0x3B, line break=0x02,
quotes = the `\o` / `\"` escapes) and selected in
`set_item_description_message` on `g_bDcMode` over a USA base. Over a JPN base
`dc_jpn_item_descriptions` (`RoomInit.cpp`) applies the DC's edits to the JPN
table instead — item 0x04 takes the JPN Beretta text (there is no JPN "custom
edition" wording in the port's data) and 0x31/0x32 the JPN "nothing important"
string. Same language-first rule as the names.

### 3f. Item sprites

`tools/port_dc_assets.py --only item_sprites`: the DC's `ITEM_ALL.PIX` (91200
vs the USA's 86400 = 4 appended rows, plus the MOON CREST halves replacing
existing sprites) and `ITEM_MIX.PIX` (25200 vs 21600 = 3 appended mixed-herb
rows) ship as `Data/item_all.pix` / `Data/item_mix.pix` in the overlay and are
picked up by the ordinary load sites (`GameInit.cpp`, `MainMenu.cpp`) through
the overlay probe.

`g_ItemsImageBuffer` (`Globals.cpp`, `0x00bcb430`) was 86400 B = the USA sheet's
72 rows. The DC sheet is 76 rows (91200 B) and `LoadAllItemsTexture` loads it in
one `LoadFile`, so in DC mode it **overran the buffer by 4800 bytes** and
`LoadHeldItemsImages` read item 0x04's sprite (image type 0x4B -> row 74, offset
88800) *past the end* — the glitched inventory icon, whose bytes are whatever
the game last wrote into the globals that follow. The buffer is now `[91200]`;
nothing else reads or writes it, and the USA sheet simply leaves the tail
unused.

### 3g. Item view art and model names

`src/game/dc/ItemModels.{h,cpp}` (hand-written; not generated).

`menu_load_item_model` picks the view file with byte 0 of the item's lookup
record through `g_ItemModelFileNames` (`0x004bd348`, 75 × 8-byte names). The
DC's lookup reaches **0x4B** = one past that table's last index (0x4A), and the
bytes that follow it in `.rdata` are the `"ING"` string of
`g_ItemModelFileNameING` — which is why item 0x04 rendered as the **Ingram**.
`item_model_file_name()` now consults the DC table on `g_bDcMode` and, for a DC
image type past the USA table with no override, returns the empty name (the load
fails and the viewer stays inert rather than opening an unrelated model).

- `0x4B` -> **`i00v_s1`** — `I00V_S1.IVM` is the DC's "custom edition" of the
  standard Beretta's view (`I00V`, image type 0x02): same mesh, flat-shaded
  (295 normals vs 1316), re-textured silver with the wooden grip, matching item
  0x04's "A beretta M92FS Automatic. / Custom edition." Its file date (Apr
  1997) is the newest in `ITEM_M2`.
- `0x2E`/`0x2F` -> **`i60v_l`/`i60v_r`** — the MOON CREST halves (items
  0x31/0x32). Both share the assembled crest's TIM (image type 0x29 = `i60v`,
  item 0x2C) and carry one TMD each — the crest's left and right half. The USA
  table named these two image types `i18v`/`i40v` (a grey prop and the OIL can),
  which the unreachable slots never showed.
- `I99V.IVM` (a second silver Beretta, dated with the February crest batch) has
  **no owner found**: no DC image type resolves to it. It ships with the set and
  is noted rather than guessed at.
- `0x4C`'s Com. Radio model is **not** ported: the DC's own data disagrees with
  itself there — image type 0x49 (item 0x4C) still names `i10v`, and the DC's
  appended sheet rows for image types 0x49/0x4A/0x4C are all the custom
  Beretta. Left as the USA mapping.

`tools/port_dc_assets.py --only item_models` copies the four `ITEM_M2`
additions into `assets/DC/ITEM_M2/` (DC-exclusive names, so plain additions).
The rest of the DC's `ITEM_M2` is byte-identical to the OG PS1 disc's.

`tools/verify_dc_item_models.py` cross-checks the mapping against the generated
DC lookup and the shipped art: every image type the DC's lookup produces
resolves to a name, each override's IVM exists both in the tree and on the
extracted disc, the overrides are unique and fit the 8-byte records, and
`g_ItemsImageBuffer` covers the DC sheet with the lookup's highest sprite row in
bounds.

### 3h. Mixing / combine

`src/game/MenuData.cpp`: `g_ItemCombinePtrs` grew 35 -> 38 and the data to 454
bytes for the three crest recipes. The herbs' combine index shift of +3 is
carried in the DC's lookup data itself, so no arithmetic is needed in the
consumers.

The combine-sprite table (`tools/gen_dc_item_tables.py` ->
`g_dcItemImageTypeTable`): `menu_item_combine_refresh` reloads the *result*
slot's icon through `g_ItemImageTypeTable[lookup[result * 4 + 1]]` — a combine
index -> the 1-based `ITEM_MIX.PIX` row of the sprite. The port's table was 35
bytes (0x004bd7f8, immediately before `g_ItemMaxQty`) and was **not** being
replaced in DC mode, so mixing the two MOON CREST halves (items 0x31 + 0x32 ->
0x2C) read the USA's zero, skipped the reload and left the half's sprite in the
slot. The DC's table is 38 bytes at **0x8008E164** (immediately before its
max-quantity table at 0x8008E18C): indices 0..25 identical to the port's, the
DC's three crest recipes **inserted at 26..28** = `0D 0E 0E`, and the six
mixed-herb entries at 32..37 = `10..15` (the port's `0D..12` value-shifted by
+3). The generator verifies that structure against both builds' own lookups
before writing, and refuses to emit a table whose crest entries are zero. The
port's `g_ItemImageTypeTable` is 38 bytes, non-const, copied in by
`dc_apply_item_tables()`.

### 3i. Engine fixes the repurposed ids required

The DC did not renumber the item table, so USA code that names one of the
repurposed slots names some other item in a DC session. These are the places
that had to be made mode-aware. Two of them (the lockpick id and the
weapon→ammo derivation) are shared by several call sites and live in
`src/game/dc/Items.{h,cpp}`.

- **Item 0x04's examine text**: with byte 2 of the lookup record at 0x80
  (unexaminable), `FUN_0044ed40` (`MainMenu.cpp`) falls into a switch on the
  item id whose `ITEM_COLT_PYTHON_DUM` case scans the slot quantity and, when it
  is non-zero, sets `g_selectedItemId = itemId + 9` and shows global message
  0xF0 = `"\i loaded."`. That `+9` maps a USA weapon onto its own ammo item
  (4 + 9 = 0x0D = DUMDUM ROUNDS); the DC gives id 4 to the Beretta M92FS custom
  and renamed 0x0D to LOCKPICK, so examining the Beretta printed "LOCKPICK
  loaded." instead of its description. In DC mode the case now breaks, so item 4
  takes the description path like any other unexaminable item — which is what
  the PS1 DC shows. *(Inference, not a located DC branch; the DC's copy of this
  function was not found in SLUS_005.51.)*
- **Item 0x04's ammo digits**: `display_item_qty` (`MainMenu.cpp`) picks the
  digit column in `STATUS.tim` from a switch on the item id; the USA build lists
  item 4 with the DumDum/acid ammo in the **orange** column. The DC's copy
  (SLUS_005.51 `0x8005436c`) keeps the red set and drops `case 4` from the
  orange set, so the Beretta M92FS custom counts its rounds in **green**, like
  the plain Beretta. The port takes the default (green) column for item 4 when
  `g_bDcMode`.
- **The lockpick is item `0x0D` in the DC**, not `0x31` (the DC's own RDTs
  confirm it — STAGE1/ROOM1061 scripts `0x0D` where the USA scripts `0x31`).
  Four places hardcoded `ITEM_LOCK_PICK` and then printed its name:
  `door_try_enter`'s Jill substitution (`0x0041b474`), `check_desk_state`'s desk
  flow (`0x0041c330`, both messages), the "using the lockpick does not consume
  it" skip in `use_room_action_item` and in Rendering's message action 1.
  Naming `0x31` in a DC session printed *"MOON CREST used."* on a lockpick door
  and would have consumed Jill's lockpick on its first use. `lockpick_item_id()`
  / `is_lockpick_item()` now answer per build.
- **Weapon -> ammo is `weaponId + 9` everywhere**, and the DC's item 4 is the
  Beretta M92FS custom, whose 9mm rounds are the CLIP; `4 + 9` names `0x0D`,
  which the DC gave to the lockpick. Firing the ADVANCED starting gun therefore
  consumed *lockpicks*, and with none held the stack search found no slot and
  cleared slot 0. `weapon_ammo_item_id()` now maps item 4 to the clip, and
  `fire_weapon_fx`'s id-4 case runs the Beretta's routine instead of the
  Python's.
- **The custom Beretta's in-hand model**: `g_weaponPathTable` is indexed by the
  weapon's item id and names the Python's model at 4, so an ADVANCED session
  carried a Python in hand. `dc_weapon_model_path()` (`dc/EntityModels.*`)
  answers `Players/W0F.EMW` (Chris) / `W1F.EMW` (Jill) for item 4. The owner is
  identified by size: each file is *exactly* its block's plain-Beretta model
  (24028 / 25580 bytes, against the Python's 29020 / 30792).
- **The unlimited Colt Python**: the DC adds a second infinite-ammo branch next
  to the rocket launcher's — `Flg_ck(g_gameOptionsFlags, 0x7a)` with
  `itemId == ITEM_COLT_PYTHON_MAG` draws the infinity glyph. That is the
  ADVANCED best-ending unlock the ending raises (§7), so
  `dc_is_infinite_colt_python` (`dc/Items.{h,cpp}`) reads the flag and four DC
  sites consume it — `weapon_autoaim_check` (SLUS_005.51 `0x8004228c`) refills
  an empty cylinder to **6**, `display_item_qty` (`0x8005436c`) draws the glyph,
  and the two empty-click paths (`FUN_8003f008` `0x8003f234`, `0x8003fc6c`)
  stay silent. The DC's `weapon_autoaim_check` also drops the PC-only
  special-weapon refill (both PS1 builds lack it); the port keeps that branch
  and adds only the Python one.
- **ADVANCED* / TRAINING double items**: the green ADVANCED* cell (hold Right
  on ADVANCED) is the DC's "double ammo" mode, and the PS1 implements it in
  `IncludeCurrentItem` (SLUS_005.51 `0x8002ce28`, the port's
  `room_event_item_pickup`): the record's quantity byte is shifted left once for
  an ammo stack (`0x0B..0x12`) or an ink ribbon (`0x2F`) when
  `g_status_flags & 0x50000`. That mask is bits 18 and 16 — **TRAINING and
  ADVANCED*** (16 = the held confirm, 18 = TRAINING; bit 17 = ADVANCED is *not*
  in it), so the original doubles in TRAINING too. The item viewer's fit
  pre-check (`FUN_8002e1a4`, the port's `FUN_0044e1b0`) repeats the same shift
  so its "you got it" / "no room" choice agrees with what the inventory will do.
  Ported once as `dc_item_pickup_quantity()` and called from both.

### 3j. Extra key/file items

The DC-only item names (MANSION/DORMITORY/SMALL/LAB/SPECIAL KEY, RED BOOK,
CHEMICAL, CRANK, the file/diary items) are in the DC name table; whichever the
DC actually gives out through an RDT `item_model_set` resolves to them.

---

## 4. Zombie variants

PS1 ref `docs/PSX_DC_ENEMY_AI.md`.

**Table validation.** The three builds' tables were dumped byte-for-byte and the
dispatch bases read off the actual instructions:

| build | `zombie_states_table` | behaviour view | behaviour 11 |
|---|---|---|---|
| PS1 OG `STAGE1` | 20 entries | `states + 9` (`0x801222c0`) | past the end |
| PS1 DC `STAGE1` | 21 entries | `states + 9` (`0x801212b0`) | `states[20]` = `0x8010cdf8` |
| PC `ResidentEvil.exe` | 22 entries | `states + 10` (`0x004bb2f0`) | `states[21]` = NULL |

The PC table is the OG's with a NULL inserted at index 9 and another appended at
index 21; the first NULL is why its behaviour base is one entry further along,
and `PC behavior[k] == OG behavior[k]` for k = 0..10. So the port's existing
table and `&zombie_states_table[10]` view are already right, the DC adds
**behaviour 11** (not "state 20 / behaviour 10"), and the port's spare
`states[21]` is exactly the slot it goes in.

- **Behaviour 11 — the scripted entrance** (`src/game/entities/Zombie.cpp`):
  `dc_standup_lunge`, the PS1 `0x8010cdf8` handler. Spawns with collision off
  (`status_flags` bit 2) and a third, larger `ScaInfo` record (radius 1000 vs
  422); plays animation 29 with `Snd_em(4)` at frame 20; when `Joint_move`
  reports the animation finished it lunges (`move_speed_current` 487 then 991 at
  a 0x400 offset), turns +0x400, restores the normal hit box and collision, and
  drops into the ordinary chase. Three companions in `zombie_init`: the `0xB`
  block (bit 2 + third ScaInfo, **before** the naked-zombie check, which can
  overwrite `Sca_info`), `zombie_anim_id_tbl[11] = 29`, and the new record
  itself. Plus one edit in `zombie_update` itself (DC `STAGE1` `0x8010615c`): a
  behaviour-11 zombie **skips the whole entity-collision trio**
  (`SetEntityScaHitData` / `ResolveEntityScaCollision` /
  `HandleEnemyPlayerCollisions`) — bit 2 only covers the room collision, and the
  entity push tests bit 1. Without that branch the prone Forest zombie can be
  shoved around by the player.
- **Fast zombie — the spawn markers** (`zombie_init` + `zombie_chase_walk`, not
  `cmd_enemy_set`): the DC rewrites `behavior_flags` nibbles `0xC`/`0xD`/`0xE`
  **inside `zombie_init`**, between the `move_speed`/`turn_speed` stores — set
  `+0x16B` to 1, mask the nibble down (`0xD` -> 1, else 0), and for `0xE` add
  100 to `health`. Doing it in `zombie_init` rather than `cmd_enemy_set` is also
  what makes it zombie-only. The marker is read in exactly three places, all in
  `zombie_chase_walk`, and each one **repeats the call above it**: two
  `Joint_move`s and two `Add_speedXZ`s per frame = double speed. There is no new
  speed constant. (The DC also adds `move_speed_current = max(45, move_speed)`
  there; inert, since nothing raises `move_speed` above 45 — ported anyway.)
- **Which nibbles the shipped data uses** (both builds' RDTs script-walked):
  zombie `0xC` in STAGE1/ROOM1030+1031, zombie `0xD` in STAGE1/ROOM1110 and
  STAGE6/ROOM6110, green-coat zombie (id `0x11`) `0xC` in STAGE1/ROOM10A0 and
  `0xE` in STAGE7/ROOM71C0+71C1 — and zombie `0xE` **never**. Behaviour `0xB`
  appears only on id `0x16` in STAGE9/ROOM9120+9121 and STAGEE/ROOME120+E121.
  Cerberus (id `0x02`) `0xC` exists in **both** builds, which is the proof that
  the rewrite must not be applied to every entity.
- **Forest zombie** (`src/game/dc/EntityModels.*` + `EntityModelLoader`):
  entity id `0x16`, model `EM1016`/`EM1116` (US = 1026 corpse stays); spawned by
  the arrange balcony rooms above. Entity id `0x16` is a zombie, not an NPC:
  `update_entities` dispatches through a per-stage overlay table, and slot 0x16
  is filled in **only** by DC STAGE2 (`0x80120a00`) and DC STAGE7
  (`0x8012aa0c`), in both cases with the same handler their slots 0/1/0x11 carry
  — the zombie. No OG overlay registers it. Those are exactly the overlays the
  arrange balcony rooms run under (STAGE9 folds onto stage 1, STAGEE onto stage
  6). `dc_apply_zombie_tables()` points `enemies_update_functions_tbl[22]` at
  `zombie_update`, so behaviour 11 has its shipped user. This **corrects**
  `docs/PSX_DC_ENEMY_AI.md`, whose "Forest zombie" section closed with "driven
  by the shared human-NPC update (id 22)".

Enemy handlers otherwise need **no change** (recompile-only in the DC).

---

## 5. Arrange data, placements, models

- **Enemy placements**: the DC-only `enemy_set` records (cerberus in the
  corridor/courtyard, hunter everywhere, chimera in the lab, etc.) live in the
  RDTs, so they come with §6's replaced RDTs; the in-script `bit_test` gate
  (§2e) picks original vs arrange. Code changes only where a placement needs one
  of the new spawn markers (§4).
- **ADVANCED model remap** (`src/game/EntityModelLoader.cpp`, `LoadEntityEMD`):
  the PS1 `0x800238c8` remap (player -> EM1032/EM1033/EM1035; NPCs +0x14 ->
  EM1040/EM1041/EM1043/EM104C) gated on ADVANCED, implemented as
  `dc_emd_advanced_index()` in `src/game/dc/EntityModels.cpp`, in the PS1's
  order — the `param_2 - 0x24` probe is computed between the two player arms and
  forced to 0x11 on the id-3 arm, so the NPC test cannot fire on an index the
  player arms just produced. Plus the **post-load** half: `if (param_2 == 0x1a)
  entity->id = 0`. That is what makes the Forest zombie damageable in ADVANCED —
  id 0x16 is past the 20-row hit table, and the DC keeps the `< 0x14` bound
  (`0x800120e8`), so without it he would be invulnerable. The arrange rooms'
  second, inert id-0x16 record reuses the loaded model, never re-enters the
  function, and so keeps its id — which is how the DC has it.
- **Model table**: the DC's is 69 entries per block at stride 0x8a
  (`0x8008d03c`) against the port's 53 (the OG's). Eight entries name a file the
  port's table does not have at that index, and five of them sit **past the
  port's block width**, where the port's table holds the other character block —
  so they cannot be reached by remapping an index and are answered by
  `dc_emd_path()`: 26 -> EM1016/EM1116, 0x35 -> EM1035, 0x38 -> EM1040, 0x39 ->
  EM1041, 0x3B -> EM1043, 0x44 -> EM104C. 0x33 and 0x34 are *not* overridden:
  they land inside the port's block, which already names em1030/em1031 and
  em1032/em1033 there.
- **New models**: `EM1016, EM1032, EM1033, EM1035, EM1040, EM1041, EM1043,
  EM104C, EM1116, W0F, W1F` are added to the overlay (`Enemy/`, `Players/`;
  §6).
- `tools/verify_dc_entity_models.py` re-reads all of it: the model table through
  the Ghidra bridge, resolved to names through the disc's own ENEMY directory;
  slot 0x16 out of all 14 stage overlays; and that every model the port now
  names is in the overlay.

### 5a. Enemy sound banks — the arrange rooms' own rows

`Room_LoadEnemySoundBanks` fills 48 WAV slots from
`g_RoomSoundNameTable[stage_data_row() * 29 + roomId]`, i.e. from the **base**
room's row. That is right for a room ADVANCED only re-dresses — same floor, same
doors, same props — and wrong for every room where ADVANCED changed the enemy:
those slots still name the base room's animal, or name nothing at all when the
base room had no enemy.

**The answer is in the arrange RDT, not in the enemy id.** A PS1/DC RDT carries
its room's sound bank inside itself (`RDT+0x88` slot->tone table, `+0x8C` VAB
header, `+0x90` VAB body), so every arrange room states its own 48 slots.
`tools/gen_dc_arrange_sound_rows.py` reads them, names each slot by finding the
same VAG bytes in a base room whose PC row *does* name that slot, and emits
`src/game/dc/ArrangeSoundRows.cpp`; the loader applies the table after the base
row. 183 slot overrides in all. **12 of the 38 arrange rooms differ**, in 127
slots:

| room | base | ADVANCED |
|---|---|---|
| 101 | zombies | cerberus, + `glass` at slots 23/24 |
| 113 / 613 | nothing (`bathMIX` at 23) | zombie, and the bathtub cue at 23 becomes the zombie's own `z_taore` |
| 212 / 712 | crows | zombie; `RVpatA`/`RVpatB` at 24/25 are **cleared** |
| 405 | wasps | web spinner (`kuasi_*`, `sp_*`) |
| 507 | nothing | zombie (the `ze_tomo` variant) |
| 604 | hunter | web spinner |
| 607 | nothing | zombie (the `z_aoya` variant) |
| 703 | nothing | zombie (the `z_haki`/`z_sanj` variant) |
| 718 | nothing | crows — **eight** slots, `RVpatA`/`RVpatB` at 6/7 — + `glass` at 24 |
| 71A | zombie at group 1 | hunter (`He_*`); slots 18/19 **cleared** |

**A room with no arrange RDT can be silent too**, and an arrange-vs-base diff
cannot see it — there is no arrange file to diff against. Room **116** (the
shotgun room) has no entry in the arrange table, so ADVANCED loads the ordinary
`ROOM1160`; its init spawns three id-17 zombies behind `04 05 2E 00`, a
`bit_test` on `main_state_flags2` bit 17 with **cond 0**, so the body runs when
ADVANCED is *set*. The RDT's own bank fills slots 0-9 and `g_RoomSndData` names
none of them, so those zombies played nothing at all. Six such (room, group)
cases exist — **116, 40A, 50A, 605, 616, 713** — and they carry
`DC_SND_ANY_DC`, because the room keeps the same file in every mode.

> Reading these gates: `cmd_bit_test` returns `(bit != 0) ^ cond` and
> `run_command_functions` pops the branch stack when a command returns 0, so
> **cond 0 = "run the body when ADVANCED", cond 1 = "run it when not"**. Room
> 101 uses cond 1 — zombies in ORIGINAL, cerberus in ADVANCED — which the PC's
> own shipped row for that room (the zombie group) independently confirms.

Three consequences worth keeping in mind, because they are what a "one sound
group per enemy type" table gets wrong:

- **There is no single zombie group.** The base rooms carry at least four
  variants and each arrange room picks one; room 212's is the
  `z_osou`/`z_unaruA`/`z_Hkick`/`z_Ugoron` set, not the `z_k01..z_k03` one.
- **Slots must sometimes be cleared.** Where the new enemy has fewer sounds the
  DC points those slots past their VAB program's tone count. Leaving the base
  room's sample there is the same bug in the other direction.
- **Not every byte difference is a different sound.** Room 507's whole bank is
  the same samples re-encoded at half the byte length, and room 505's chimera
  group was re-authored while the SCD still spawns enemy 9. Those rooms keep the
  base row; the generator lists them with the evidence.

---

## 6. Asset pipeline — the `assets/DC/` overlay

The DC gets an **overlay**, not a tree and not in-place replacement (§0). One
script fills `assets/DC/` with the diff; the base tree is never written to.

### 6a. The overlay steps

**`tools/port_dc_assets.py`** copies + converts into `assets/DC/`
(`--base USA|JPN` only names the tree the overlay is compared against, for
reporting). Steps are also the `--only` names:

| step | DC source (PS1) | target (PC tree) | notes |
|---|---|---|---|
| `title_bg` | `DATA/TITLE.PIX` | `Data/title.pix` | DC title background ("DIRECTOR'S CUT") |
| `title_menu` | `DATA/BT367OAB.TIM` | `Data/BT367OAB.TIM` | copied as-is; the **engine** splits it into two texture pages at load time (§2a) |
| `item_sprites` | `DATA/ITEM_ALL.PIX`, `DATA/ITEM_MIX.PIX` | `Data/item_all.pix`, `Data/item_mix.pix` | §3f |
| `item_models` | `ITEM_M2/I00V_S1,I60V_L,I60V_R,I99V.IVM` | `Item_m2/` | §3g |
| `arrange_rdt` | `STAGE8-E/**.RDT` | `Stage8-E/` | the arrange rooms (§6b) |
| `rooms` | `STAGE1-7/**.RDT` the DC changed | `Stage1-7/` | under their own names; the base file is untouched |
| `models` | `ENEMY`, `PLAYERS`, `ITEM_M1`, `ITEM_M2` | same | DC-exclusive art (§4/§5) |
| `data` | `DATA/*` the PC tree also has | `Data/*` | the four files that differ (§6b) |
| `font` | the PC font + the PS1 CLUT's colour columns | `Data/fontus.tim` | the save screen's per-mode text colours (§7) |
| `transparency` | — | the overlay's PS1 TIMs | relabels PS1 colour-keyed TIMs onto the PC's index-0 key; runs last, over everything above |

Arrange-stage **backgrounds** are a separate job (a format conversion, not a
copy) and live in `tools/bss_to_pak.py --arrange` (§6b). Sources are read from
`assets/PSX_DC/`. The script is idempotent (it rewrites only files it owns and
reports unchanged ones as up to date) and prints a summary so the overlay's
state is verifiable. `tools/verify_dc_tree.py` checks the manifest.

The DC also rewrote files that already exist in the base tree (the STAGE1-7
RDTs and some `DATA` files). Each one simply goes into `assets/DC/` at the same
relative path, and `ResolveAssetRoot` prefers it while `Mode=DC`. Anything the
DC does not have — the `...1` room variants it drops, and whole rooms it omits
(e.g. STAGE1 110/119) — is *absent* from the overlay and falls through to the
base file, which is exactly the behaviour those rooms need.

**Overlay hazard.** A half-populated overlay fails *quietly*: the missing file
falls back to the base and you get a Frankenstein room (DC script, base
background) rather than an error. The tool's summary and the manifest verifier
are therefore not optional polish — they are how that failure is caught. The
engine also logs the active mode and overlay at startup.

### 6b. Arrange stages, backgrounds and stage-id folding

STAGE8-E are stage ids **7-13**, seven ids past everything the port's
stage-indexed tables were built for. Rather than widen those tables, the port
folds: `get_stage_id()` (`src/Globals.h`) maps arrange stage S onto base stage
S-7, and is the identity for stages 0-6 so `Mode=OG` is untouched.

The premise is that an arrange room is a re-dressed copy of the base stage's
room with the same room id, which the shipped data confirms: every one of the 38
distinct arrange room ids also exists in its base stage, and the highest is
`0x1C` = 28, inside both the 29-room stride of `g_RoomSoundNameTable` and the
32-room stride of the rest. `tools/verify_stage_tables.py` checks all of it.

Folding also avoids a trap. `g_roomBgmState` is 224 bytes **inside
`g_BioCard`**, so widening it to 14 stages would move every field after it and
change the save format.

Two kinds of site, and they must not be confused:

| site | uses | why |
|---|---|---|
| table rows: BGM state, room effect sprites, room sprites, camera light index, fade-sprite params, voice offsets/names, enemy sound banks, the `(stage+1) % 5` script special cases | `get_stage_id()` | an arrange room wants its base room's data |
| file paths: the RDT name, `g_bgPathTemplate`, `g_maskPathTemplate` | **raw** `g_stageId`, and `STAGE_IS_REVISIT()` in place of `g_stageId > 4` | the arrange stages ship their own files under STAGE8-E and must not fold onto stage 1/2's |

**STAGED and STAGEE are the arrange revisit stages.** They ship **no
backgrounds at all** and reuse STAGE8/STAGE9's, exactly as base stages 6 and 7
reuse stage 1 and 2's — and their room ids match their source stage's exactly.
So the engine's existing `g_stageId > 4` revisit fold is the right rule on a
background path too; it just has to see the **folded row**, so that it fires for
base 5/6 *and* arrange 12/13 while leaving arrange 7-11 — which do ship their own
art — alone. One wrinkle when folding a path: the original subtracts 5 from the
stage **character**, which only works inside the run of decimal digits; arrange
stages recompute from the id instead. The base-stage expression is left
byte-for-byte as the original wrote it, including a genuine Capcom bug —
`load_room_bg`'s non-cached branch folds from the ROOM's low hex digit
(`[0x11]`) where its cached branch uses the stage digit (`[0x0f]`). Verified
against `0x00462b00`; reproduced, not fixed, because normal room loads take the
cached branch.

Known gap: the handful of packed
`*(unsigned short*)&g_stageId != (STAGE | ROOM << 8)` room-identity tests
(`Zombie.cpp` ×2, `PlayerAnimations.cpp` ×3, `Hunter.cpp`) still compare the raw
id, so those per-room scripted details do not fire in an arrange copy of the
room. Costs a missing detail, not a crash.

**The background converter.** `tools/bss_to_pak.py` is a PSX MDEC bitstream
decoder feeding an encoder for the PC's LZW; `docs/PSX_BSS_AND_PC_PAK.md`
documents both formats. Of the **153** arrange camera frames, only 39 are
byte-identical to a frame in some base stage, so **114** genuinely had to be
decoded.

*How it is trusted.* Stages 1-7 exist in *both* forms, so the decoder is run
against Capcom's own conversion of the same bitstream, frame by frame: **641 of
645 frames match** (99.4%), at a residual well under one step in 31 per channel.
The four that do not decode to clean images are art the PC release re-authored
(STAGE1/ROOM107 cam 7, STAGE2/ROOM200 cam 1, STAGE3/ROOM30E cams 2-3);
`--verify` tells those apart from real failures with a luma-gradient test,
because a broken decode gives noise rather than a different photograph. Three
findings worth keeping:

- The 8->5 bit conversion must **round** (`(v + 4) >> 3`), not truncate. With
  truncation the decode is correct but uniformly half a step dark, and agreement
  with the shipped paks sits at ~31% of pixels instead of ~86%.
- The background TIM's image-block length field counts **only the pixel bytes**
  (153600), not the 12 bytes of length + `x/y/w/h` that the TIM spec says it
  should. A spec-correct writer produces a file the port mis-reads.
- `tools/pak_view.py` had a real bug: it built the LZW **KwKwK** string
  backwards. The game is right — `unpack_pakfile_` puts the appended character
  at `stringBuf[0]` and emits the buffer downwards so it lands last. Fixed.

**The 76 rooms both builds edited: resolved, no merge needed.** 76 of the 138
rooms the DC changed were *also* changed by the PC release; every one was
compared section by section, using each file's own header pointers. In most of
those rooms the PC and the DC changed the **same** sections, and there the DC's
version is simply the content DC mode wants. The PC edits that would actually be
lost are only:

| section | rooms | what it is | impact |
|---|---|---|---|
| `vab_sound_file` (0x90) | 14 | the embedded PS1 sound bank; the PC rewrote ~8% of its ADPCM bytes | **none at runtime** — nothing in the port reads RDT+0x88/0x8C/0x90. §5a mines those blocks **offline** to get the arrange rooms' rows |
| `scd_opcodes` (0x64) | 4 | one opcode, `0x2F cmd_snd_pan_vol_set`: the PC raised three cuts' volumes | a PC audio rebalance of mansion-1F room `0x0C`; DC mode uses the PS1's quieter mix |
| `scd_opcodes2` (0x68) | 12 | small edits inside existing event bodies — event *counts* unchanged | cutscene timing and flag tweaks |

No header count field differs in any of the 76. None of it needs engine code:
the port's `script_command_funcs_table` is 81 entries covering `0x00`-`0x50`
with no NULL slots, and the DC uses **no SCD opcode the PS1 OG does not already
use**; the event VM is a separate instruction set and every DC event script uses
only the ten control bytes `RoomEvents.cpp` implements. The one thing worth
remembering is the audio rebalance above — if room `0x0C` sounds quieter in DC
mode than in OG, that is the PS1 data, not a bug.

**The four DC `Data` files that differ.** The DC's `DATA` shares 20 names with
the PC tree; 10 are byte-identical and these four are not:

- **`bio_card.dat`** — the new-game state template. 248 bytes differ, but 247
  are in the 0x200-byte save-card prefix, which nothing reads by field (the PS1
  memory-card block header; the PC build zeroed it). The single meaningful byte
  is at **0x3C2**, inside `roomBgmState[224]` at index 134 = stage 4 room 6, the
  Laboratory's SMALL LABORATORY: **PC `0x40`, PS1 `0x08`**. The PS1 OG and the
  PC differ in *that one byte and nothing else*, so it is a PC-only music
  change, and DC mode takes the PS1 value.
- **`core00.esp`** — the global weapon-FX sheet. Exactly **one byte** differs,
  at **0x1E0**: PC `0x36`, DC `0x00`. One field of one sprite record; the file
  is parsed generically, so nothing structural.
- **`en05.tim` / `en07.tim`** — the ending screens. 94% of the bytes differ:
  these are genuinely the DC's own ending art (the DC ships a whole
  `ENDING_DC.EXE` alongside the OG one).
- **`pdemo0-2.dat`** — the attract-mode demo recordings. The DC recorded its
  own, and this one needed a code fix: `LoadAttractModePlayerData` reads a fixed
  `0x994`-byte record, taking the controller config from `+0x990` and the demo
  player's health from `+0x992`. **The DC's `pdemo0.dat` is `0x990` bytes** —
  four short, with no tail. The staging buffer is `static` and reused across
  reels, so the port would have handed reel 0 the *previous* reel's recorded
  health. It is now cleared before each load, which is both deterministic and
  what the data means: every shipped reel except the PC's `pdemo2` records
  health 0 there. The original never meets a short file, so OG is unchanged.
  Open, minor: the DC ships **three** reels and the port cycles **four**, so
  reel 3 falls back to the PC's `pdemo3.dat` — one OG demo in a DC attract loop.

**The file/document images: nothing to do.** The three builds package the
readable in-game documents differently, which looks like it needs a converter
and does not: the PS1 OG ships 45 separate `FILEM_*.PIX` (raw 49216-byte pages),
the PS1 DC ships one **`FILEM.PIX`** of 2,355,200 bytes = 46 pages of the same
49216 bytes padded to a 51200 stride, and the PC ships 16 `filem_*.pix`
containers. Every one of the DC's 46 pages is byte-identical to a PS1 OG page
(one page is a duplicate), so the DC adds no document art. DC mode reads the
PC's `filem_*.pix` through the overlay's expected-fallback list (§0), and
nothing is lost. `FILEMARR.PIX` is identical between the builds; the PC's
`ARROR.TIM` is its name for it.

**The PS1 transparency key.** The PS1 GPU keys a texel transparent on its
COLOUR (a palette entry of exactly `0x0000` is a hole, whatever index it sits
at); the PC engine keys on the INDEX (`CreateTextureHandle`, MarniSystem.cpp:
alpha 0 for index 0, 0xFF for everything else), and the PC release re-authored
its own art to match — not one TIM in `assets/USA` puts a `0x0000` entry where a
texel uses it. The PS1 art does, so every PS1-authored texture in the overlay
rendered its holes as opaque black. Found via `ele01.dor`, the room 50D lift
gate: the mesh squares are index 224 (`0x0000`) on the DC disc and index 0 in
the PC copy, so the DC gate read as one solid door. 120 files were affected
across every stage — room models, enemy and item models, doors. The
`--only transparency` step RELABELS rather than recolours: indices whose entry
is `0x0000` in every palette row are folded onto index 0 (swapping index 0 out
first when it holds a visible colour), so the colours and the file length are
untouched. It skips overlay files byte-identical to the base tree and leaves
indices that are a hole in one palette row and a colour in another
(`em1009.emd` 251-254), which relabelling cannot resolve.

**How the arrange rooms are entered.** Not by a door: a door's destination byte
packs the stage as `(dest >> 5) - 1` (`DoorSystem.cpp`), which tops out at stage
6, and the DC's `room_transition_load` (`0x8002a70c`) is byte-for-byte the OG's
— no `+7`, no mode branch. Instead the DC re-points the FILE lookup per room:
`FUN_80043fb4` (SLUS_005.51 `0x80043fb4`, ported as `room_file_stage()` in
`src/game/dc/ArrangeStages.cpp`): when ADVANCED, it scans a 7x9 table of room
ids at `0x80010670` for the current stage's row and returns `g_StageId + 7` if
the current room is in it. It is called from exactly two places — `LoadRoomRdt`
(`0x8004484c`) and the background loader (`0x80017c2c`). So the game keeps
walking the mansion with `g_stageId` 0-6 and a room that has an arrange version
simply loads it: stage 1 room 3 reads `Stage9/ROOM9030.RDT`, cameras, script,
models and all. That also means the stage-indexed tables need no help at all in
a real DC session; `get_stage_id()` still earns its place for the F1 debug menu,
which sets `g_stageId` to 7-13 directly. The table is the DC's own data and
matches the shipped arrange RDTs exactly, both ways — 8/5/2/3/4/9/7 rooms
against Stage8-E's 38 — checked by `tools/verify_stage_tables.py` check 5.

**Masks: the art was already in the RDT.** Each camera has TWO pointers in the
RDT, `mask_pointer` (the sprite records) and `tim_mask_pointer` — and the second
is the mask TIM itself: 8bpp, 256-colour CLUT at VRAM (0,0x1E0). The PC's
objspr pak IS that TIM, LZW'd; `OSP01030.pak` and stage 2 room 3 camera 0's
embedded TIM agree to **a single byte in 66080**. So `load_room_masks` reads an
arrange camera's TIM straight out of the loaded RDT — no new assets, no
converter, and it is what the PS1 does. Base rooms keep the shipped pak path.
One real bug fell out of the same look: a missing mask pak made `LoadFile`
return `-1`, and that was added to the write cursor, walking it BACKWARDS
through `g_bgMaskDataBuffer` and leaving later cameras with negative offsets.
Now a miss is a miss.

**The `...1` room variants.** The PC release ships BOTH character variants of
every room, so its loader can always append the variant digit; the DC disc does
not (`STAGE9` has `ROOM9010` and no `ROOM9011`). The DC carries a table for
exactly this (`0x80090f48`, 14 rows of 32, read by its own `LoadRoomRdt`): a zero
means the room has a variant and the character bit is added to its file index,
anything else means one file serves both. Ported as
`dc_room_has_char_variant()` and checked against the disc by
`verify_stage_tables.py` check 6 — **0 rooms** where the table asks for a file
that is not shipped, and 5 the other way round (a file the game never asks for).

A failed RDT load is now handled too: `LoadRoomRdt` used to ignore
`LoadFile`'s failure and relocate pointers through a buffer still holding the
PREVIOUS room, so counts came out of stale bytes and the omodel walk ran into
unmapped memory. A failed load now keeps the old RDT and says so.

---

## 7. Save/load and endings

- **The mode lives in the save.** The DC stores it at bio-card `+0x233`
  (state-block `+0x33` — `g_abDcGameMode` is `g_StageId + 0x33`), copied with
  the rest of the `0x240`-byte slot record. `BioCardLayout` now has
  `dcGameMode` / `g_DcGameMode`, and it rides the existing whole-card copy.
  `dc_apply_mode_flags()` reads it on the continue path and writes it on the
  new-game path (PS1 `FUN_80019638`), which is what makes a continued game run
  in the mode it was started in rather than in the default a fresh title-screen
  choice would leave behind. A USA save has 0
  there and loads as STANDARD.
- **Slot colour-coding** (`src/game/SaveLoadScreen.cpp`): the DC draws each slot
  row's text with colour = that row's mode byte. The two builds pack the colour
  differently: the PS1 font CLUT is 17 palettes wide by 3 rows at VRAM
  (0x100, 0x1E0) and the print picks a **column**
  (`clutX = 0x100 + (colour & 3) * 0x10`), while the port keeps clutX at 0x100
  and selects on the low nibble as a **row**. So the four palettes ship as four
  CLUT rows in the DC's own font (`port_dc_assets.py --only font`), and the mode
  byte is the row index. The PC font's 16 colours are byte-identical to the PS1
  CLUT's column 0, which is what makes row 0 — and every uncoloured string in
  the game — unchanged. STANDARD cream, TRAINING green, ADVANCED red, ADVANCED*
  grey, plus the extra pass at colour 0x11 the PS1 makes over the name cells.
- **A JPN base** reads `Data/FONT.TIM`, which already ships the PS1's wide CLUT,
  so it is its own colour source; the DC's `FONT.TIM` is excluded from the
  `data` step outright (same format, same size, but the *US* glyph sheet under
  the port's Japanese text tables).
- **The ending** (`src/game/EndingScreen.cpp`): an ADVANCED run reaching ending
  **6 or 7** — the best ending for Chris and for Jill — calls
  `Flg_on(g_gameOptionsFlags, 0x7a)` and grants item 5 / qty 1, the magnum, into
  the next cycle's inventory (read off `PROG2/ENDING.EXE` with Capstone). The
  `0x7b` in the same block is the port's existing
  `SCENARIO_FLAG_SECOND_PLAYTHROUGH`, so only `0x7A` is new. The overlay's other
  arm (item 0x38 when the ADVANCED counter is set but the run was not ADVANCED)
  cannot fire — that counter is overlay BSS, zeroed on load, and only the
  ADVANCED path increments it — so it is documented, not ported. The two 0x7B
  reads are guarded with `!g_bDcMode` (§2f).
- **The next-cycle save's start handgun.** The ending rebuilds the save the player
  starts the following game from, and `SetInitialItems` picks the Beretta M92FS
  custom (item 4) over the plain Beretta only while `MSF2_DC_ADVANCED` is set. The
  DC gets there by raising the bit for that one call and clearing it again
  (`ENDING_DC.EXE 0x800e17d0`, which tests the `g_status_flags` snapshot its
  overlay entry took at `0x800e00a4`). The PS1 can test the bit because the
  snapshot predates the mask; in the port `game_start` has already run
  `g_main_state_flags2` through `MSF2_RESET_KEEP_MASK`, which does not keep
  `0x70000`, so `ending_state` puts the bit back from `g_DcDifficulty` — the same
  stand-in the other DC branches in that function already use — around its
  `SetInitialItems` call. Without it, every game started from a post-ending save
  began with the plain Beretta.
- **Port-only: OG mode refuses DC TRAINING/ADVANCED saves.** In `Mode=OG`, a
  save whose `file[0x233]` is TRAINING/ADVANCED/ADVANCED* (1/2/3) is drawn grey
  in the save/load screen and cannot be loaded; it can still be chosen in the
  SAVE menu and overwritten. Mode 0 is allowed — an OG save and a DC STANDARD
  save both carry 0 and cannot be told apart (all shipped `bio_card.dat`
  templates have 0), so the rule is `(mode & 3) != DC_DIFFICULTY_STANDARD`.
  `SaveLoadScreen.cpp` (grey tint 3, and the load guards in `STATE_IDLE` /
  `STATE_LOAD_SLOT_SELECTED`), the F1 debug quick lists (`DebugMenu.cpp`), and
  `DebugQuick_LoadSlot` (`DebugSaveLoad.cpp`) all enforce it. `Mode=DC` is
  unaffected.
- `tools/verify_dc_save_mode.py` recomputes every `BioCardLayout` offset from
  the struct and checks each against its own comment, checks the mode->bit
  mapping against the PS1's own load path (`0x80019638`) through the Ghidra
  bridge, reads the ending overlay's raise/clear-`ADVANCED` window
  (`0x800e17d0`) to confirm the port wraps `SetInitialItems` the same way, and
  diffs both overlay fonts against the base glyph block and the PS1
  colour columns.

---

## 8. Verification and testing

- `python tests/check_platform_boundary.py` — `src/game/` must stay
  OS-agnostic.
- Build both: `build.bat` (MSVC) and
  `cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release` + build.
- Every generated table has a verifier:
  - `tools/gen_dc_damage_tables.py` verifies the STANDARD column equals the
    USA table before emitting `dc/WeaponDamageTables`.
  - `tools/gen_dc_item_tables.py` asserts the thresholds, heal values and every
    id's use category; `tools/verify_dc_item_models.py` re-checks the shipped
    file plus the view-name overrides, the IVMs and `g_ItemsImageBuffer`.
  - `tools/verify_dc_entity_models.py` re-reads the model table and slot 0x16
    from the Ghidra bridge / stage overlays.
  - `tools/verify_dc_save_mode.py` checks the save layout and font colours.
  - `tools/verify_dc_title_sheet.py` mirrors `dc_title_build_page`.
  - `tools/verify_stage_tables.py` checks the arrange room-id premise, the
    per-stage stride, that no `.cpp` indexes a stage table with a raw
    `g_stageId`, the arrange entry table, and the `...1` variant table.
  - `tools/verify_dc_tree.py` checks the overlay manifest.
- `tools/bss_to_pak.py --verify` checks the background decoder against Capcom's
  own conversion (641/645 frames).
- Playtest checklist (user-run): `Mode=OG` unchanged; `Mode=DC` -> mode menu,
  per-mode health/damage, Beretta insta-kill, crest mixing, fast/Forest zombie,
  arrange rooms, save colours and mode round-trip.

---

## 9. Known gaps and deferred work

- The handful of packed room-identity tests that still compare the raw
  `g_stageId` (§6b) — per-room scripted details do not fire in an arrange copy.
- The DC ships three attract reels and the port cycles four; reel 3 falls back
  to the PC's `pdemo3.dat` (§6b).
- The submenu's *cancel* (the PS1 tests a held-button combo and returns to the
  main menu) is not ported; and the ADVANCED* green cell is drawn from the
  shared palette's spare entry, so it does not need the PS1's second CLUT row.
- Item 0x4C's Com. Radio model is left as the USA mapping because the DC's own
  data disagrees with itself there (§3g).
- The two MOON CREST halves' description reads "Nothing important." in DC mode,
  which is the DC's *own* text (§3e); if the PS1 shows something else there, the
  halves must reach a different branch.
- Item 0x04's examine-text break is an inference rather than a located DC branch
  (§3i).
- SATURN and DS are reserved mode names with no code behind them (§1).
