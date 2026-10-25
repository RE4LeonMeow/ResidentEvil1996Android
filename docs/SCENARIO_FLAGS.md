# Global Flag Banks

Almost every persistent and per-frame boolean in the game lives in one of ten
global bitfields. They are one system, not ten: the same three accessors address
all of them, and every one is reachable from RDT script data through the same
three SCD entry points. This document is the index — storage, addressing, and
the per-bit meaning of each bank that has been audited.

## The banks

`cmd_bit_test` (`0x00460570`) and `cmd_bit_op` (`0x00460650`) select a bank with
a `switch` on their first operand; the numbers below *are* that switch's cases,
so a bank number in an RDT script means exactly this table.

| Bank | Port name | Address | Size | Scope | Saved | Per-bit table |
|---|---|---|---|---|---|---|
| 0 | `g_ScenarioFlags` | `0x00be98c0` | 16 B (128 bits) | scenario-wide story / character state | yes (BioCard `0x2A0`) | [below](#g_scenarioflags--bank-0-0x00be98c0) |
| 1 | `g_ScenarioFlags2` | `0x00be9854` | 32 B (256 bits) | scenario-wide, second bank | yes (BioCard `0x234`) | [below](#g_scenarioflags2--bank-1-0x00be9854) |
| 2 | `g_LocksFlags` | `0x00be9874` | 8 B (64 bits) | door / desk lock state | yes | not audited |
| 3 | `g_EnemiesFlags` | `0x00be987c` | 32 B (256 bits) | per-room enemy spawn / kill bits | yes | not audited |
| 4 | `g_SysFlags` | `0x00be41c8` | 8 B (64 bits) | system / menu state | no | not audited |
| 5 | `g_main_state_flags` | `0x00be41c0` | 4 B (+4, see below) | runtime frame and screen state | no | [below](#g_main_state_flags--bank-5-0x00be41c0) |
| 6 | `g_message_flags` | `0x00bebcc0` | 2 B (16 bits) | message-system gating | no | not audited |
| 7 | `g_roomItemsFlags` | `0x00be989c` | 32 B (256 bits) | per-room item / model visibility | yes | not audited |
| 8 | `g_RoomFlags` | `0x00be98d0` | 20 B (160 bits) | rooms visited, maps owned, files collected | yes | [below](#g_roomflags--bank-8-0x00be98d0) |
| 9 | `g_itemUseFlags` | `0x00d213a0` | 8 B (64 bits) | per-frame item-use flags, cleared by `game_loop` every frame | no | see [SCD_COMMAND_OPCODES.md](SCD_COMMAND_OPCODES.md) |

Banks 0/1/2/3/7/8 sit inside `BioCardLayout`, the 0x800-byte memory-card save
block ([BioCard.h](../src/game/BioCard.h)); 4/5/6/9 are standalone globals that
reset with the process.

Banks 0 and 1 are the scenario banks: character equipment traits, story and boss
progression, map and BGM progression, playthrough state and ending state.
Nothing about a bit's meaning can be inferred from which of the two it sits in —
the split is storage, not category.

## Addressing

Every bank is bit-addressed identically, and the idiom is worth memorising
because it is not the obvious one:

- **Bit 0 is the MSB of the first dword.** `Flg_on` builds its mask as
  `0x80000000 >> (bit & 0x1F)`, so bit 0 is `0x80000000` of dword 0, bit 31 is
  `0x00000001` of dword 0, and bit 32 is `0x80000000` of dword 1.
- **The byte offset is a dword offset.** `(bit & 0xFFFFFFE7) >> 3` — the mask
  clears bits 3 and 4, so this is `(bit >> 5) * 4`. A bank of N bytes therefore
  holds `N * 8` bits and is read four bytes at a time.
- **Accessors**: `Flg_on` (`0x00473ef0`) sets, `FUN_00473f10` (`0x00473f10`)
  clears, `Flg_ck` (`0x00473f40`) tests. All three take the bank base address
  and a bit index.
- **From SCD**: `cmd_bit_test` (`0x04`), `cmd_bit_op` (`0x05`) and the
  `flag_bank_set` room action (`0x0041b918`). Their `sel` byte is the same bit
  index: byte offset `(sel & 0xE0) >> 3`, bit `sel & 0x1F`.

Two traps this idiom has produced in the port:

- Indexing a `DWORD[]` bank with the raw byte offset scales it by four again.
  `&g_SysFlags[(bit >> 3) & ~3]` is a byte offset applied to a dword array.
- Printing the raw `sel & 0x1F` as "bit N" reports the MSB-first index, which
  reads as `31 - N` in the usual LSB numbering.

Because scripts can touch every bit of every bank, the tables below only list
bits whose meaning is verified from ported code. Unlisted bits are script-only
(RDT data) and not yet traced — an absent row is "unknown", never "unused",
except where a row says so explicitly.

Code-verified bits have named constants in
[BioCard.h](../src/game/BioCard.h) (`SCENARIO_FLAG_*`, `SCENARIO2_FLAG_*`,
`ROOM_FLAG_*`) — use them at call sites instead of magic numbers.

---

## `g_ScenarioFlags` — bank 0 (`0x00be98c0`)

### Character / inventory traits

| Bit | Meaning | Set by | Read by |
|---|---|---|---|
| `0x7C` | Has the lockpick (Jill) — selects the lockpick (`0x31`) over the small key (`0x3D`) for desk interactions | save data / item pickup script | `check_desk_state`, `check_desk`, desk unlock flow |
| `0x7F` | Has the radio (item `0x4D`) | `room_event_take_item` on the radio pickup | item menu radio-tab availability, `menu_handle_input` |
| `0x7E` | Rocket launcher unlock | `EndingScreen` (no-save clear or carried over) | `EndingScreen` grant, `PlayerAnimations` item `0x0A` use path |
| `0x2A` | Alternate outfit | save data | `GameStart` (adds 8 to the character model id) |

### Story / boss progression

| Bit | Meaning | Set by | Read by |
|---|---|---|---|
| `0x29` | Plant 42 defeated | `plant42_award_kill` (last vine down) | room SCD scripts (bank 0 tests) |
| `0x5B` | Monster plant combat progression | `mp_damaged` after 3+ hits | room SCD scripts |
| `0x21` | Lab passcode panel solved (`PLAYER_FLAG_PANEL_SOLVED`) | `check_and_display_interactive_screen` | `ComputerLab`, `LabSlides` |
| `0x20` | Interactive screen active gate (`PLAYER_FLAG_INTERACTIVE_SCREEN`) | `check_and_display_interactive_screen` | `InteractiveScreen` panel flow, `ComputerLab`, `LabSlides` |
| `0x1E` / `0x1F` | Passcode-panel variant selectors (pick the initial 3x3 panel state set, per character) | room SCD scripts | `check_and_display_interactive_screen` |
| `0x10` | Yawn bite event | Yawn attack entry (enemy type id `0x0D`) | room SCD scripts |
| `0x47` | Yawn serum marker — the first Yawn poisons only while this is clear | serum event script | `Yawn` bite |
| `0x16` | Chemical combine performed (combine effect 4) | `menu_use_set_flag` | combine / V-JOLT progression logic |
| `0x37` | Wesker later-animation variant (larger shadow, anim `0x30`) | room SCD scripts | `char_init_wesker` |
| `0x13` | Progression gate — item `0x13` use is rejected while set | room SCD scripts | item-menu use check |

### Map / BGM progression

| Bit | Meaning | Set by | Read by |
|---|---|---|---|
| `0x48` | Objective **acknowledged** for `g_ScenarioFlags2` `0x38` — stops the guardhouse map highlight | room SCD scripts (`ROOM40F0` = security room, and others) | [map objective highlight](#the-map-objective-highlight) |
| `0x49` / `0x4A` | Objective **acknowledged** for the serum objectives — raised by `ROOM1000`/`ROOM1001`'s init script on entering the serum room while the matching objective bit is set, which stops the Mansion 1F map highlight. Other rooms set them too | room SCD scripts | [map objective highlight](#the-map-objective-highlight) |

### Playthrough / scenario state

| Bit | Meaning | Set by | Read by |
|---|---|---|---|
| `0x7B` | **Second playthrough** ("hard mode"). Set once a run is cleared; every enemy AI (Cerberus, Chimera, Crow, Hunter, Neptune, Plant 42, Tyrant, Wasp, WebSpinner, Yawn, Zombie), the `WeaponDamage` hit tables and the endings select their second-playthrough variant from it | `EndingScreen` after the clear | all of the above |
| `0x7A` | **Director's Cut: ADVANCED cleared with the best ending** (ending 6/7). The next cycle's Colt Python becomes unlimited — it refills to 6 when empty, draws the infinity glyph and drops the empty click. DC mode only; the USA build never sets or reads it | `EndingScreen` (DC mode only) | `dc_is_infinite_colt_python` (`PlayerAnimations`, `MainMenu`) |
| `0x7D` | Fade/menu latch — picks the fade-in counter at gameplay entry; cleared when the menu closes | menu flow | `game_loop` |
| `0x00` | Scenario stage-variant bit — stage changes remap stages 0/1 to their `+5` variants while set (uses the heavier `init_room` path) | room SCD scripts / save data (never ported code) | `DoorSystem` stage transition, item-menu use check |

---

## `g_ScenarioFlags2` — bank 1 (`0x00be9854`)

| Bit | Meaning | Set by | Read by |
|---|---|---|---|
| `0x0B` | Jill first-playthrough marker, armed at new game (paired with `g_roomItemsFlags` bit `0x34`) | `GameStart` | room SCD scripts |
| `0x22` | Story progression gate for radio-tab availability (character id `& 3 == 3` path) | room SCD scripts | `main_menu`, `menu_handle_input` |
| `0x23` / `0x24` | **Jill: serum objective outstanding**, one bit per serum. Set by the pillar-passage (`ROOM20D1`) poisoning cutscene; `ROOM1001` arms a serum pickup per bit and clears the bit once that serum is taken. `0x23` drives the Mansion 1F map highlight | `ROOM20D1` cutscene | [map objective highlight](#the-map-objective-highlight), `ROOM1001` |
| `0x2D` / `0x2E` (`SCENARIO2_FLAG_SERUM_OBJ1_CHRIS` / `..._OBJ2_CHRIS`) | **Chris: serum objective outstanding** — the same pair for Chris. `0x2D` is set by the pillar-passage cutscene (`ROOM20D0`, byte-identical to Jill's but for the flag), `0x2E` by the front-of-attic cutscene (`ROOM20E0`); `ROOM1000` clears each when its serum is taken | `ROOM20D0` / `ROOM20E0` cutscenes | [map objective highlight](#the-map-objective-highlight), `ROOM1000` |
| `0x38` (`SCENARIO2_FLAG_PLANT42_OBJ`) | **Plant 42 objective outstanding** (the V-JOLT hunt). Set by the Plant 42 room's event script, cleared by the same room when Plant 42 dies alongside `g_ScenarioFlags` `0x29`. Drives the guardhouse map highlight, and gates Rebecca's radio tab | `ROOM40C0` event script | [map objective highlight](#the-map-objective-highlight), `menu_handle_input` |
| `0x48` / `0x55` / `0x5C` | Stage 2 room 7 BGM channel conditions (Jill path) — `bgm_load_and_start` reads all three from **this** bank, not bank 0 | room SCD scripts | `bgm_load_and_start` |
| `0x43` | **Poisoned by Yawn** — set together with `healthStatusFlags \|= 0x20` on the first Yawn's bite; cleared when the serum is used | `Yawn` bite | serum use in the item menu |
| `0x4B` | Ending "second survivor" bit | ending scenario scripts | `ending_select_id` |
| `0xC0` | Ending "partner survived" bit — also gates the NPC reunion animation | ending scenario scripts | `ending_select_id`, `CharacterNpc` |

---

## `g_main_state_flags` — bank 5 (`0x00be41c0`)

The runtime state word: what the frame is currently doing. Unlike banks 0/1/8
this one is **not saved** — `GameInit` and `game_start` zero it, and several
screens reset large parts of it with an AND mask on entry.

**Bank 5 is one 8-byte region, not two globals.** `cmd_bit_test` / `cmd_bit_op`
resolve the bank to a single base (`0x00be41c0`) and then add
`(sel & 0xE0) >> 3` as a byte offset (`mov eax, [eax + ecx]` at `0x004605f9`),
so a selector of `0x20` or more walks into the second dword —
`g_main_state_flags2` at `0x00be41c4`. Selector id = `0x20 + (31 - bit)`, so the
boulder-tunnel screen shake (msf2 bit 1) is script selector `0x3E`, with 64
script writers.

The port therefore stores both dwords as one array, `g_MainStateFlagBank[2]`,
with `g_main_state_flags` / `g_main_state_flags2` as macros onto its elements —
separate globals would have made that byte offset depend on link order.

One asymmetry to know: the `flag_bank_set` room action does **not** add the byte
offset for banks 5 and 6 (its arms are a bare `mov edx, 0xbe41c0` /
`mov edx, 0xbebcc0` at `0x0041b8e4` / `0x0041b8eb`), so that action can only
reach each bank's first dword. Only `cmd_bit_op` / `cmd_bit_test` reach msf2.

### `g_main_state_flags2`

| Bit | Mask | Constant | Meaning |
|---|---|---|---|
| 0 | `0x00000001` | `MSF2_EFFECT_ZONE` | `room_action_effect` (room action `0x0B`) ran this frame: dust billboard under a moving player, health forced to 1 (cannot die), footstep sound type shifted, projectile effects change. `game_loop` clears it after `update_player_anim` |
| 1 | `0x00000002` | `MSF2_SCREEN_SHAKE` | screen shake enable; `main_loop` also requires `MSF_MENU_BYTE` clear |
| 2 | `0x00000004` | `MSF2_SCREEN_BORDER` | backgrounds load 316x236 instead of 320x240 and `ResetScreenAndRebuildSprites` is skipped; parked in `g_controllerConfig` bit `0x10` while the options menu is open |
| 3 | `0x00000008` | `MSF2_FADE_NO_DEPTH_CLAMP` | `FadeSprite` skips its depth clamp |
| 19 | `0x00080000` | `MSF2_PRESERVED_19` | no reader or writer found — only ever *preserved*, by `MSF2_RESET_KEEP_MASK` |
| 21 | `0x00200000` | `MSF2_SFX_BANK1_HALF` | sound bank 1 holds 16 entries instead of 32 |
| 22 | `0x00400000` | `MSF2_DOOR_TURN_PENDING` | `check_door` / `check_door_side` asked for a turn; the door animation consumes it |
| 23 | `0x00800000` | `MSF2_SND_BUSY` | sound/BGM busy — the `.dor` script's `play_sfx` op waits for it |
| 24 | `0x01000000` | `MSF2_DOOR_ANGLE_STEP` | door animation is stepping the player's facing angle |
| 25 | `0x02000000` | `MSF2_ROOM_SPRITES_OFF` | `DrawRoomSpr` returns early |
| 26 | `0x04000000` | `MSF2_COSTUME_VARIANT` | alternate costume model selection |
| 27 | `0x08000000` | `MSF2_COUNTDOWN_ACTIVE` | self-destruct countdown running |
| 28 | `0x10000000` | `MSF2_ATTRACT_DEMO` | attract-mode demo playback |
| 29 | `0x20000000` | `MSF2_PLAYER_INITIALISED` | raised by `InitPlayerData`; survives the reset mask |
| 31 | `0x80000000` | `MSF2_DEATH_VARIANT` | picks the death fade length and whether `game_loop`'s die path reports "continue" |

Bits 4-18, 20 and 30 have no reader or writer in code, and no RDT script writes
them. `MSF2_RESET_KEEP_MASK` (`0x20080000`) is what `game_start`, character
select and the F9 reset preserve; `MSF2_ROOM_RESET_MASK` (`0x0000000F`) is the
low nibble `room_set` clears, same as msf.

Every bit below has an `MSF_*` constant in [Globals.h](../src/Globals.h), next to
the declaration, and no call site uses a raw mask any more — including the ones
the decompiler had written as byte-indexed or shifted accesses
(`((unsigned char*)&msf)[1] & 0x7F`, `(msf >> 8) & 0xFF`, `(msf >> 1) & 1`),
which are now plain masks on the dword.

### Bit table

| Bit | Mask | Meaning | Notes |
|---|---|---|---|
| 0 | `0x00000001` | Mirror pass enabled | Bits 0-1 are written together by `cmd_mirror_set` (`0x004610b0`) from `g_ScdOpcodes[1]`. Read by `EffectSystem`, `EntityCommon`, `PlayerAnimations`, `RoomInit`, `MainMenu` |
| 1 | `0x00000002` | Mirror plane axis: 0 = Z, 1 = X | Passed to `mirror_point_visible` as `(msf >> 1) & 1` |
| 2 | `0x00000004` | Deferred camera redisplay mode | With this set, a camera-zone switch raises bit 5 and returns instead of redrawing immediately (`check_camera_switch` `0x00462cc0`) |
| 3 | `0x00000008` | Script-only (`MSF_SCRIPT_ONLY_03`) | No reader in ported code, but **18 room scripts set it** (bank 5 selector `0x1C`). The low nibble is cleared wholesale by `room_set` |
| 4 | `0x00000010` | Ladder/stairs direction latch | Set by the ladder room action; picks player behaviour `0x0B` over `0x11`. Cleared when the climb ends |
| 5 | `0x00000020` | Pending camera-background redisplay | Raised by bit 2's deferred path, consumed once by `game_loop` (`display_room_camera_bg`, then cleared) |
| 6 | `0x00000040` | Object push in progress | Set by `RoomCollision` while a push is running; forces player behaviour `0x10` (climb/vault). `behavior_10_push` watches it drop to run its release state |
| 7 | `0x00000080` | Door transition in progress | `door_transition_update` runs while set |
| 8 | `0x00000100` | Pickup/message screen requested | Sends `main_menu` straight to state 8 before the mode scan. Set by the `set_key_flag` / `set_room_event_flag` room actions |
| 9 | `0x00000200` | menu mode 5 — the **map-item display** | No C writer, but **7 room scripts set it** (bank 5 selector `0x16`: `ROOM2040/2041`, `ROOM20D0`, `ROOM4080/4081`, `ROOM7040/7041`), so the mode-5 branch is live. Also read by the event VM's `0xF7` wait opcode, which stalls the script while it (or `g_menu_choice_id & 0x80`) is up |
| 10 | `0x00000400` | menu mode 4 — "got item" | Set by `cmd_got_item` (`0x2D`) |
| 11 | `0x00000800` | menu mode 3 — item viewer / examine | |
| 12 | `0x00001000` | menu mode 2 — item box | |
| 13 | `0x00002000` | menu mode 1 — key item depleted | The "you can drop it now" prompt after a door key's last use |
| 14 | `0x00004000` | Script-only (`MSF_SCRIPT_ONLY_14`) | Outside the mode ladder (it is only the scan's shift base), but **14 stage-3 event scripts set it** (bank 5 selector `0x11`) |
| 15 | `0x00008000` | Menu task active | Re-entry lock: `game_loop` refuses to open another menu while set |
| 16 | `0x00010000` | Screen intensity ramping up | `main_loop` walks `g_spriteAnimIntensity` toward `0xF0` while set, back toward 0 while clear. Cleared by `LabSlides` and `title_state` |
| 17 | `0x00020000` | Voice / SFX line playing | Polled as "wait for the line to finish" by `ComputerLab`, `ComputerArms`, `CharacterNpc`, `Tyrant`; cleared by `SoundSystem` |
| 18 | `0x00040000` | FMV requested | `main_loop` consumes and clears it, then plays `g_selectedFmvId` |
| 19 | `0x00080000` | Reset screen panning every frame | `main_loop` calls `ResetScreenPanning` while set (ending sequence) |
| 20 | `0x00100000` | Camera zone switching disabled | `check_camera_switch` returns immediately — the cutscene camera lock |
| 21 | `0x00200000` | unused | No reader or writer, in code or in any RDT script |
| 22 | `0x00400000` | Options menu requested | Set by the START-combo check in `game_loop`; the menu opener consumes it and launches `options_menu` instead of `main_menu` |
| 23 | `0x00800000` | Character RDT variant: 0 = Chris, 1 = Jill | Picks the room file suffix (`ROOMxxx0` / `ROOMxxx1`) and the Chris/Jill half of the file list. Also read as `((unsigned char*)&msf)[2] & 0x80` |
| 24 | `0x01000000` | Player dead / ending fade running | Set when `health < 0`; freezes the frame counter and the countdown timer, and drives the death state machine |
| 25 | `0x02000000` | Gameplay task active | Set on entry to `game_loop` |
| 26 | `0x04000000` | Room / door transition animation running | `cut_set` and `RestoreRoomCamera` skip the camera setup while set; `room_transition_load` polls it |
| 27 | `0x08000000` | unused | **No writer in code or scripts.** It appears only inside clear masks (`game_loop`'s `msf &= ~(MSF_GAMEPLAY_ACTIVE \| MSF_UNUSED_27)`) |
| 28 | `0x10000000` | Continue / load (1) vs new game (0) | `InitializeGame` branches on it; a loaded save carries it |
| 29 | `0x20000000` | Fade transition in progress | `main_loop` drives `g_fading_state` while set |
| 30 | `0x40000000` | **Screen mode**: standalone screen | Also suppresses the room background quad and the world sprite path — the menus and title screens draw their own graphics. See below |
| 31 | `0x80000000` | **Screen mode**: full sprite rebuild present | See below |

### Byte 1 is the menu mode

Bits 8-15 are one field, not eight independent flags, and `main_menu` reads it
as a priority-encoded mode:

```c
DAT_00ae9f10 = 5;
do {
    if ((g_main_state_flags & (0x4000U >> (DAT_00ae9f10 & 0x1f))) != 0) break;
    DAT_00ae9f10--;
} while (DAT_00ae9f10 != 0);
```

The shift walks `0x200, 0x400, 0x800, 0x1000, 0x2000` for modes 5 down to 1,
falling out at mode 0 (the ordinary inventory menu) when none is set. Bit 8 is
tested *before* this loop and short-circuits to state 8, so it is not part of
the ladder. Bit 14 is only the shift's base value and bit 15 is the re-entry
lock, so neither is a mode.

Two idioms fall out of that packing:

- `MSF_MENU_PENDING` (`0x7F00`) means "**a menu or message mode is pending**".
  It is the standard gate on player interactions and on opening the menu. The
  original expresses it three ways — `msf & 0x7F00`,
  `((unsigned char*)&msf)[1] & 0x7F`, and `(msf >> 8) & 0xFF` for the whole
  byte — all now written as one mask.
- `menu_restore_game_state` clears the whole field with `msf &= ~MSF_MENU_BYTE`
  when the menu closes.

### Bits 30/31 are a two-bit screen mode

They are never set independently. Every writer uses
`msf = (msf & 0x3FFFFFFF) | 0x40000000` or `| 0x80000000`, i.e. clear both and
select one, and `main_loop` reads them as an if/else chain at the end of the
frame:

| Value | Present path |
|---|---|
| bit 30 | `SetScreenReadyWithDebugColor` — flat `g_spriteAnim{R,G,B}` fill, background quad and world sprites suppressed. Title, character select, options, ending, death, `game_start` |
| bit 31 | `ResetScreenAndRebuildSprites` — the full rebuild, plus the screen-shake re-offset. Gameplay, save/load, menus over the world |
| neither | Nothing is presented. `title_state` clears both with `msf &= ~0xC0000000` once the title selection is resolved |

Treat them as one field: setting bit 30 without clearing bit 31 leaves the
older mode winning, because bit 30 is tested first.

---

---

## `g_RoomFlags` — bank 8 (`0x00be98d0`)

20 bytes = 160 bits, carved into three unrelated blocks plus padding. Each
accessor is a two-line wrapper around `Flg_on`/`Flg_ck`, so the base offset is
the only thing that tells them apart.

| Bits | Count | Block | Index | Written by | Read by |
|---|---|---|---|---|---|
| `0x00`–`0x7B` | 124 | Room **visited** | `g_StageRoomFlagOffset[stageId % 5] + roomId` | `room_set_visited_flag` (`0x00488570`) | map screen (`0x0048775f`, `0x004878db`, `0x00488459`) |
| `0x7C`–`0x81` | 6 | **Map** owned | `ROOM_FLAG_MAP_BASE + mapIndex` | `set_room_item_seen_flag` (`0x004885a0`) | `map_area_known` (`0x004885c0`) |
| `0x82`–`0x91` | 16 | **File** collected | `ROOM_FLAG_FILE_BASE + (itemId - 0x5F)` | `0x00488660` | `pickup_item_seen` (`0x00488680`) |
| `0x92`–`0x9F` | 14 | unused | — | — | — |

The visited block's per-group bases are `g_StageRoomFlagOffset` = `{ 0, 32, 63,
82, 100 }`, the running sum of `g_MapRoomCounts` = `{ 32, 31, 19, 18, 24 }`:
Mansion 1F `0`–`31`, Mansion 2F `32`–`62`, courtyard + underground `63`–`81`,
guardhouse `82`–`99`, laboratory `100`–`123`. It therefore ends exactly where the
map block starts.

> Watch the base collision: decimal **82** is the guardhouse *visited* base,
> hex **0x82** is the *file* base. They address different blocks.
> `0x00488660` takes a file index, not a room id. It was named
> `map_set_room_flag`, which is why the file block was long mistaken for a
> second rooms block; it is now `file_set_collected_flag` in both the port and
> Ghidra.

Room SCD scripts reach the whole bank as flag bank 8 (`cmd_bit_test`,
`cmd_bit_op`, and the `flag_bank_set` room action at `0x0041b918`), so a script
can set or test bits in any of these blocks.

### The map bits

`ROOM_FLAG_MAP_BASE` (`0x7C`) `+ map index`. Both constants live in
[BioCard.h](../src/game/BioCard.h); the item ids and map indexes are
`ITEM_MAP_*` / `MAP_INDEX_*` in [Types.h](../src/game/Types.h).

| Bit | Map index | Item id | Map areas granted | Room | Set by |
|---|---|---|---|---|---|
| `0x7C` | 0 | `0x4E` | 0 (Mansion 1F) | 107 | `pickup_key_event`, item model slot 2 (`g_roomItemsFlags` bit `0x91`) |
| `0x7D` | 1 | `0x4F` | 1 (Mansion 2F) | 20B | event SCD `05 08 7D 00` (`ROOM20B0/1` `@0xCEAC`) — no item model exists |
| `0x7E` | 2 | `0x50` | 3 (Courtyard) | 300 | `pickup_key_event`, item model slot 4 (`g_roomItemsFlags` bit `0x4F`) |
| `0x7F` | 3 | `0x51` | 4 (Underground) | 30F | `pickup_key_event`, item model slot 4 (`g_roomItemsFlags` bit `0x76`) |
| `0x80` | 4 | `0x52` | 5 **and** 6 (Guardhouse) | 406 | `pickup_key_event`, item model slot 8 (`g_roomItemsFlags` bit `0x87`) |
| `0x81` | 5 | `0x53` | 8 **and** 9 (Laboratory) | 506 | `cl_cmd_unlock` (`0x004137a1`: `push 5`) — no item model exists |

Notes:

- The dispatch is data-driven, not per-room code: `cmd_item_model_set`
  (`0x00461220`) gives any item model whose id falls in
  `[ITEM_MAP_FIRST, ITEM_MAP_LAST]` the room-check action `0x0F`
  (`pickup_key_event`) instead of the `4` an ordinary item gets, and that
  handler does `Flg_on(g_RoomFlags, itemId - ITEM_MAP_FIRST + 0x7C)`.
- Map areas **2** (Mansion B1), **7** (Lab B1) and **10** (Lab lowest) have no
  map bit at all — `map_area_known` returns 0 for them, so they only ever appear
  on the map screen through the `0x82 + roomId` explored bits.
- These bits decide *which areas may be shown*. They are unrelated to the
  objective-highlight gates (`g_ScenarioFlags` `0x48`/`0x49`/`0x4A` and
  `g_ScenarioFlags2` `0x23`/`0x2D`/`0x2E`/`0x38`) described
  [below](#the-map-objective-highlight).
- The stage `+5` variants carry the same data: `ROOM6070` mirrors `ROOM1070` and
  `ROOM70B0` mirrors `ROOM20B0`.

---

## The map objective highlight

`map_update_objective_highlight` (`0x00488950`, called `map_update_variant`
until this was traced) is not a map-*data* selector. It computes `MAP_MODE`, and `MAP_MODE`'s only real job is
to make the map screen **blink an area with a label sprite** when the story has
just given the player somewhere to go:

| `MAP_MODE` | after `menu_init_map_screen`'s `+1` | Highlighted area |
|---|---|---|
| 0 | 0 | none |
| 1 | 2 | area 0 — Mansion 1F |
| 3 | 4 | area 6 — the guardhouse water-tank half |

`map_display_animate` starts the highlight on `state[0xf] == 2 && state[6] == 0`
or `state[0xf] == 4 && state[6] == 6`, i.e. exactly those two area matches, and
`state[0xf] / 3` picks where the label sprite is drawn. The odd values 1 and 3
are what the map-*item* path (`menu_init_map_display`, menu mode 5) sees; that
path is reached from **room scripts**, which are the only thing that sets msf
bit 9 — no C code does.

Every input to it is one of two kinds of bit, and they always come in pairs —
**objective outstanding** (`g_ScenarioFlags2`) and **objective acknowledged**
(`g_ScenarioFlags`). The highlight shows while the first is set and the second
is still clear:

| Objective | Set by | Acknowledged by | Highlight |
|---|---|---|---|
| `0x23` (Jill) | pillar-passage poisoning cutscene | `0x49` | Mansion 1F |
| `0x2D` (Chris) | pillar-passage poisoning cutscene | `0x49` | Mansion 1F |
| `0x2E` (Chris) | front-of-attic cutscene | `0x4A` | Mansion 1F (outside the mansion it also needs `0x38`) |
| `0x38` (both) | Plant 42 room event | `0x48` | guardhouse water-tank half |

The Jill/Chris split is a straight duplication: `ROOM20D0` and `ROOM20D1` are
byte-identical over the whole cutscene except for the flag they raise, `0x2D`
versus `0x23`.

Only three of these have named constants —
`SCENARIO2_FLAG_SERUM_OBJ1_CHRIS` (`0x2D`), `SCENARIO2_FLAG_SERUM_OBJ2_CHRIS`
(`0x2E`) and `SCENARIO2_FLAG_PLANT42_OBJ` (`0x38`), whose only script uses in
the whole game are the ones above. `0x23` / `0x24` keep generic
`SCENARIO2_FLAG_PROGRESS_*` names because they are **reused**: `ROOM3070` (a
Chris file) sets `0x23` in a block with `0x30`/`0x33`/`0xC0`, and `ROOM3030`
clears `0x22`/`0x23`/`0x24` together with audio side effects. So do
`0x48`/`0x49`/`0x4A`, which several rooms set for reasons unrelated to the map.

### The serum objectives

`ROOM1000` / `ROOM1001` (the mansion save room) hold **two** serum item models,
item `0x42`, on `g_roomItemsFlags` bits `0x17` and `0x18`. The room's scripts
wire one objective bit to each:

```
init:  if (SF2 0x2D set) set SF 0x49        ; arriving acknowledges objective 1
       if (SF2 0x2E set) set SF 0x4A        ; arriving acknowledges objective 2
main:  if (SF2 0x2D set) room_action 6      ; offer serum 1
       if (SF2 0x2E set) room_action 7      ; offer serum 2
       if (both clear)   message 0x4F
       if (SF2 0x2D set && roomItems 0x17 clear) clear SF2 0x2D   ; serum 1 taken
       if (SF2 0x2E set && roomItems 0x18 clear) clear SF2 0x2E   ; serum 2 taken
```

So the full loop is: a poisoning cutscene raises the objective bit → the map
highlights Mansion 1F → walking into the serum room raises the acknowledgement
bit and stops the highlight → taking that serum clears the objective bit
itself. Jill's file uses `0x23` / `0x24` and room actions 3 / 4 for the same
two slots.

### The Plant 42 objective

`ROOM40C0`'s event script raises `0x38` (with `0x3D`) when the Plant 42 scene
fires, and the same room clears it when the boss dies, in the same breath as
`g_ScenarioFlags` `0x29` (`SCENARIO_FLAG_PLANT42_DEAD`):

```
04 01 38 00    ; if SF2 0x38 set
05 01 38 01    ;   clear SF2 0x38
05 00 29 00    ;   set SF 0x29 (Plant 42 defeated)
```

While it is up, the map points at map area 6 — the water tank, security room,
arms storehouse and control room, which is where the V-JOLT chemicals are. The
acknowledgement bit `0x48` is raised by several rooms, `ROOM40F0` (the security
room, itself in area 6) among them.

`0x38` has a second, unrelated reader: with Rebecca (`id & 3 == 3`),
`main_menu` shows the radio tab when she does *not* have the radio item, `0x38`
is set and `0x22` is clear.

---

## Other banks

Not yet audited bit by bit. Storage, size and addressing are in
[the bank table](#the-banks); the notes below are what the port has confirmed
in passing.

- **Bank 2, `g_LocksFlags`** — door and desk lock state. `door_try_enter` reads
  it; the lab terminal raises bits `0x25`/`0x26` to release its two doors.
- **Bank 3, `g_EnemiesFlags`** — per-room enemy bits, indexed by the enemy
  slot's flag byte in the RDT `enemy_set` record.
- **Bank 4, `g_SysFlags`** — system/menu state. Known bits: `0x1D`/`0x1E`/`0x1F`
  select which interactive screen is up (passcode panel / lab computer /
  slides); `0x20` is the crank-use completion flag.
- **Bank 6, `g_message_flags`** — 16 bits gating the message system. `game_loop`
  requires `0x40` and `0x100` both set before it will open the menu, seeds the
  word with `0xFD3F` on entry to gameplay, and backs it up in
  `g_short_message_flags` while a menu is open.
- **Bank 7, `g_roomItemsFlags`** — one bit per room item/model, the bit index
  carried in the `room_action_set` / `item_model_set` record itself (`opcode[0x16]`).
  Set means the item is still there; picking it up clears the bit.
- **Bank 9, `g_itemUseFlags`** — per-frame, cleared by `game_loop` every frame.
