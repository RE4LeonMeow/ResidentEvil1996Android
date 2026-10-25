# SCD Command Opcode Reference

Per-opcode specification of the 81-entry command dispatch table
`script_command_funcs_table` (`0x004c1110`, opcodes `0x00`–`0x50`), used by both
the command VM (`run_command_functions`, `0x00473f60`) and the event VM
(`room_events_check`, `0x0041d6a0`). Handlers are implemented in
[CmdFunctions.cpp](../src/game/CmdFunctions.cpp); high-level VM behaviour is
documented in [SCD_SCRIPT_SYSTEM.md](SCD_SCRIPT_SYSTEM.md).

Notation:

- All offsets are relative to the start of the instruction (the opcode byte is
  at `+0`).
- `u8` = unsigned byte, `s8` = signed byte, `u16`/`s16` = little-endian word.
- **Cond** marks condition commands: the handler returns its boolean test
  directly, so a failed test (`0`) aborts the straight-line run and the
  interpreter resumes at the address `cmd_if` (`0x01`) pushed on the branch
  stack. All non-condition commands return `1` (continue).
- `0x51`–`0xFF` are not valid opcodes: the original table ends at `0x50` with
  no padding (the next bytes are the string `"DOOR_AT_SET "`). The decomp pads
  the 256-entry array with `nullptr`, turning out-of-range dispatch into a
  null-pointer crash.

---

## Control flow

### `0x00` — `cmd_block_end`

| Key | Value |
|---|---|
| Index | `0x00` |
| Address | `0x004604d0` |
| Length | 1 byte |
| Returns | `0` (stop) |
| Description | Block terminator. Clears the pending-branch counter `g_ScriptContinueFlag` and returns 0, ending the straight-line run of the current block. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x00`. No payload bytes are consumed. |

### `0x01` — `cmd_if`

| Key | Value |
|---|---|
| Index | `0x01` |
| Address | `0x004604e0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Conditional-skip head. Pushes resume address `(ip + 2) + skipLen` onto the branch stack and increments `g_ScriptContinueFlag`. When a following condition command fails, the interpreter resumes there, skipping the `if` body. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x01` |
| +1 | skipLen | u8 | Bytes to skip after this instruction to reach the resume address. |

### `0x02` — `cmd_else`

| Key | Value |
|---|---|
| Index | `0x02` |
| Address | `0x00460520` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Else-branch. Pops the branch stack, decrements `g_ScriptContinueFlag`, then jumps to `(address of this instruction) + jumpLen`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x02` |
| +1 | jumpLen | u8 | Jump distance measured from the `cmd_else` opcode byte itself. |

### `0x03` — `cmd_end_if`

| Key | Value |
|---|---|
| Index | `0x03` |
| Address | `0x00460550` |
| Length | 2 bytes |
| Returns | `1` |
| Description | End of `if`/`else` block. Pops the branch stack and decrements `g_ScriptContinueFlag`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x03` |
| +1 | pad | u8 | Unused; consumed to advance the stream. |

---

## Flag bits

### `0x04` — `cmd_bit_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x04` |
| Address | `0x00460570` |
| Length | 4 bytes |
| Returns | bool: `bitIsSet XOR expect` |
| Description | Tests one bit in a flag bank. Bit numbering is MSB-first within a dword (bit 0 = `0x80000000`). Byte offset into the bank is `(sel & 0xE0) >> 3`, bit index is `sel & 0x1F`. Unknown bank returns 0 (stop). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x04` |
| +1 | bank | u8 | `0`=g_ScenarioFlags, `1`=g_ScenarioFlags2, `2`=g_LocksFlags, `3`=g_EnemiesFlags, `4`=g_SysFlags, `5`=g_main_state_flags, `6`=g_message_flags, `7`=g_roomItemsFlags, `8`=g_RoomFlags, `9`=g_itemUseFlags (per-frame item-use flags) |
| +2 | sel | u8 | Bits 0–4: bit index within the dword. Bits 5–7: dword byte offset (`(sel & 0xE0) >> 3`). |
| +3 | expect | u8 | Expected bit state: `0` = expect set, `1` = expect clear. |

### `0x05` — `cmd_bit_op`

| Key | Value |
|---|---|
| Index | `0x05` |
| Address | `0x00460650` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Set/clear/toggle one bit in a flag bank. Same bank and `sel` encoding as `0x04`; mask is `0x80000000 >> (sel & 0x1F)`. Unknown bank or mode returns 0 (stop). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x05` |
| +1 | bank | u8 | Same bank table as `0x04`. |
| +2 | sel | u8 | Bits 0–4: bit index. Bits 5–7: dword byte offset. |
| +3 | mode | u8 | `0` = set (OR), `1` = clear (AND-NOT), `2` = toggle (XOR). |

---

## Room / fade state

### `0x06` — `cmd_state_byte_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x06` |
| Address | `0x00460760` |
| Length | 4 bytes |
| Returns | bool |
| Description | Compares one byte of the BioCard state block against `cmpVal`. The index is a byte offset from `g_stageId` (BioCard `+0x200`): 0 stageId, 1 roomId, 2 roomCameraId, 3 attractMode_RoomCameraId, 4 cutId, 5 menu_choice_id, 6 selectedItemId, 7 totalHeldItems, 8 specialRoomLightR, 9 characterModelId, 10 scdLastEnemyFlags, 11 bulletEffectId, 12-14 pickupQtyA/B/C, 16 fwdPosActionId, 17 entPosActionId, 18 usedItemId, 19 pickedItemId. Sampled scripts test indices 2 (268 sites), 3, 5, 9, 16, 17, 18 and 19 — nothing room-specific. Unknown mode returns 0. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x06` |
| +1 | fieldIdx | u8 | Byte index into the `g_stageId` state array. |
| +2 | mode | u8 | `0` `==`, `1` state `>`, `2` state `>=`, `3` state `<`, `4` state `<=`, `5` `!=` (state vs `cmpVal`). |
| +3 | cmpVal | u8 | Comparison constant. |

### `0x07` — `cmd_state_word_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x07` |
| Address | `0x00460800` |
| Length | 6 bytes |
| Returns | bool |
| Description | Compares one short of the BioCard state block against `cmpVal`. The index counts **shorts** from `g_fading_state` (BioCard `+0x214`): 0 fadingState, 1 specialRoomLightState, 2 specialRoomLightDelta, 3 randSeed, 4 countdownTimer, 5 playerHealthCopy, 6 playerDpadHeld, 7 playerDpadPressed. Of the 221 *reachable* uses, 214 read index 3 (`randSeed` — this is how scripts roll dice) and 7 read index 4 (the lab countdown); none reads index 0 at all. Unknown mode returns 0. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x07` |
| +1 | pad | u8 | Unused. |
| +2 | fieldIdx | u8 | Element index into the `g_fading_state` word array. |
| +3 | mode | u8 | `0` `==`, `1` state `>`, `2` state `>=`, `3` cmp `>`, `4` cmp `>=`, `5` `!=`. |
| +4 | cmpVal | u16 | Comparison constant. |

### `0x08` — `cmd_state_byte_set`

| Key | Value |
|---|---|
| Index | `0x08` |
| Address | `0x004608a0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Writes one byte of the BioCard state block; same index space as `0x06`. The index is unbounded and scripts use that — ROOM1130 writes index 82, which lands in `scenarioFlags2[30]`, and ROOM40A0 writes index 57 (`scenarioFlags2[5]`). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x08` |
| +1 | fieldIdx | u8 | Byte index into the `g_stageId` state array. |
| +2 | value | u8 | New byte value. |
| +3 | pad | u8 | Unused. |

### `0x31` — `cmd_state_word_set`

| Key | Value |
|---|---|
| Index | `0x31` |
| Address | `0x004608d0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Writes one short of the BioCard state block; same index space as `0x07`. Known users: index 4 (the lab countdown) and indices 1/2 in ROOM4110, which drives the flashing red emergency light. The index counts **shorts** from `g_fading_state` (BioCard `+0x214`): 0 fadingState, 1 specialRoomLightState, 2 specialRoomLightDelta, 3 randSeed, 4 countdownTimer, 5 playerHealthCopy, 6 playerDpadHeld, 7 playerDpadPressed. ROOM4110's init desyncs in `mine_room_scd.py`, so a script survey alone will not find that second user. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x31` |
| +1 | fieldIdx | u8 | Element index (byte offset `fieldIdx * 2` into `g_fading_state`). |
| +2 | value | u16 | New word value. |

---

## Camera

### `0x09` — `cmd_cut_lock_set`

| Key | Value |
|---|---|
| Index | `0x09` |
| Address | `0x00460920` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Saves the current camera id in `g_cutId`, switches to `camId` (walks `cam_switch_zones`, stride `0x14`, id at `+2`, calls `cut_set`), and sets `g_main_state_flags |= 0x100000` (camera locked). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x09` |
| +1 | camId | u8 | Camera id to lock to. |

### `0x0A` — `cmd_current_cut_set`

| Key | Value |
|---|---|
| Index | `0x0A` |
| Address | `0x00460990` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Restores the camera saved in `g_cutId` and clears the `0x100000` lock bit. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x0A` |
| +1 | pad | u8 | Unused. |

### `0x23` — `cmd_cut_lock_write`

| Key | Value |
|---|---|
| Index | `0x23` |
| Address | `0x00431280` |
| Length | 2 bytes |
| Returns | `1` |
| Description | **Writes** the `g_main_state_flags` `0x100000` camera-lock bit from the operand: `0` clears, anything else sets. Not a toggle — sampled scripts pass a literal `1` (16 sites) or `0` (14 sites). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x23` |
| +1 | lock | u8 | `0` = unlock, any other value = lock. |

### `0x3A` — `cmd_cut_zone_set`

| Key | Value |
|---|---|
| Index | `0x3A` |
| Address | `0x00431e50` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Rewrites two fields of the `cam_switch_zones[zoneIdx]` record (stride `0x14`): word at `+2` and word at `+0`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x3A` |
| +1 | zoneIdx | u8 | Index into `cam_switch_zones`. |
| +2 | toCam | u8 | Written as a word to zone `+2` (current camera of the zone). |
| +3 | fromCam | u8 | Written as a word to zone `+0` (zone trigger camera). |

---

## Messages / FMV

### `0x0B` — `cmd_message_set`

| Key | Value |
|---|---|
| Index | `0x0B` |
| Address | `0x004609f0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `set_message_display(msgId, pause)` — displays a message window; the pause operand is a full word (a common transcription bug was reading it as a byte). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x0B` |
| +1 | msgId | u8 | Message id (MSG table index). |
| +2 | pause | u16 | Game-pause duration while the message shows. |

### `0x29` — `cmd_fmv_set`

| Key | Value |
|---|---|
| Index | `0x29` |
| Address | `0x00461a40` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Selects an FMV to play: sets `g_main_state_flags |= 0x40000`, stores `fmvId` in `g_selectedFmvId` and points `g_fmvDataPointer` at `g_loadDataDestPointer`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x29` |
| +1 | fmvId | u8 | FMV id. |

---

## Room action table (doors, items, desks)

The 12-byte entries live in `g_RoomActionTable` (`0x00d91aa0`, 24 slots,
stride `0xC`); see [SCD_SCRIPT_SYSTEM.md](SCD_SCRIPT_SYSTEM.md) §4.

### `0x0C` — `cmd_door_set`

| Key | Value |
|---|---|
| Index | `0x0C` |
| Address | `0x004611b0` |
| Length | 26 bytes |
| Returns | `1` |
| Description | Registers a door interaction. Fills event entry `slot`: `[0]=1`, `[1]=record byte 0x17`, `[2..3]=slot`, `[8..11]=` pointer to the 24-byte record (`p+2`). Bumps `g_RoomActionTail`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x0C` |
| +1 | slot | u8 | Room action slot (index * 0xC). |
| +2 | record | u8[24] | 24-byte door record (RDT DOOR data), referenced whole via the pointer stored in the entry; see the record layout below. |
| +0x19 | subType | u8 | Last record byte; copied into event entry byte `[1]`. |

Door record layout (starts at opcode stream `+2`, so record offset = stream offset − 2):

| Record offset | Key | Type | Description |
|---|---|---|---|
| +0x00 | posX | u16 | Door X co-ords. |
| +0x02 | posY | u16 | Door Y co-ords. |
| +0x04 | posZ | u16 | Door Z co-ords. |
| +0x06 | rotation | u16 | Door rotation. |
| +0x08 | doorOpen | u8 | Door-open id (door opening cutscene). |
| +0x09 | pad0 | u8 | Unused. |
| +0x0A | latchType | u8 | Latch type. |
| +0x0B | doorType | u8 | Door type. |
| +0x0C | pad1 | u8 | Unused. |
| +0x0D | nextRoom | u8 | Room the door leads to. |
| +0x0E | nextRoomX | u16 | Next room start X co-ords. |
| +0x10 | nextRoomY | u16 | Next room start Y co-ords. |
| +0x12 | nextRoomZ | u16 | Next room start Z co-ords. |
| +0x14 | nextRoomRot | u16 | Next room rotation. |
| +0x16 | keyId | u8 | Key needed (key item id). |
| +0x17 | pad2 | u8 | Unused; stored into event entry byte `[1]` (`subType` above). |

### `0x0D` — `cmd_room_action_set`

| Key | Value |
|---|---|
| Index | `0x0D` |
| Address | `0x00461130` |
| Length | 18 bytes |
| Returns | `1` |
| Description | Builds one room action (trigger zone) from scratch: an 8-byte zone box plus an explicit handler index, probe flags and three parameter words. Fills entry `slot`: `[0]=handler`, `[1]=flags`, `[2..7]=` three `u16`, `[8..11]=` pointer to the record (`p+2`). Bumps `g_RoomActionTail`. The generic sibling of `0x0C`, which hardcodes handler `1`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x0D` |
| +1 | slot | u8 | Room action slot. |
| +2 | record | u8[16] | The record the entry's `+8` pointer aims at; see the layout below. |

Record layout (starts at opcode stream `+2`, so record offset = stream offset − 2):

| Record offset | Key | Type | Description |
|---|---|---|---|
| +0x00 | zoneX | u16 | Zone box origin X. |
| +0x02 | zoneZ | u16 | Zone box origin Z. |
| +0x04 | zoneW | u16 | Zone width along +X. |
| +0x06 | zoneD | u16 | Zone depth along +Z. The box test (`is_point_in_action_zone`, `0x0041b3c0`) compares **unsigned**, so the box only ever extends towards +X/+Z. |
| +0x08 | handler | u8 | Entry byte `[0]`: `room_check_actions` index. |
| +0x09 | flags | u8 | Entry byte `[1]`: probe flags. Low three bits are a **participation mask** matched against the prober's own mask: `0x01` the player pass (`game_loop`), `0x02` the Tyrant pass, `0x04` the room-object push pass. `0x40` tests the prober entity's own position instead of the 600-unit forward point; `0x80` makes the entry action-key only, skipped by the per-frame passes. |
| +0x0A | word0 | u16 | Entry word at `+2` (handler parameter). |
| +0x0C | word1 | u16 | Entry word at `+4` (handler parameter). |
| +0x0E | word2 | u16 | Entry word at `+6` (handler parameter). |

### `0x12` — `cmd_room_action_reset`

| Key | Value |
|---|---|
| Index | `0x12` |
| Address | `0x00460fc0` |
| Length | 10 bytes |
| Returns | `1` |
| Description | Rewrites bytes `[0..7]` of room action entry `slot`, leaving the SCD record pointer at `+8` — and so the zone geometry — untouched. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x12` |
| +1 | slot | u8 | Room action slot. |
| +2 | type | u8 | Entry byte `[0]`: `room_check_actions` handler index. |
| +3 | flags | u8 | Entry byte `[1]`: probe flags. Low three bits are a **participation mask** matched against the prober's own mask: `0x01` the player pass (`game_loop`), `0x02` the Tyrant pass, `0x04` the room-object push pass. `0x40` tests the prober entity's own position instead of the 600-unit forward point; `0x80` makes the entry action-key only, skipped by the per-frame passes. |
| +4 | word0 | u16 | Entry word at `+2`. |
| +6 | word1 | u16 | Entry word at `+4`. |
| +8 | word2 | u16 | Entry word at `+6`. |

### `0x13` — `cmd_room_action_arm`

| Key | Value |
|---|---|
| Index | `0x13` |
| Address | `0x00461010` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Arms, disarms or re-types one room action entry: writes only the handler index and the probe flags, so the zone geometry and parameters built by `0x0C`/`0x0D` survive. Handler `0` (`no_room_action`) or flags without bit `0x01` make the zone dead — scripts use it in `if`/`else` pairs to toggle a trigger. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x13` |
| +1 | slot | u8 | Room action slot. |
| +2 | type | u8 | Entry byte `[0]`: `room_check_actions` handler index. |
| +3 | flags | u8 | Entry byte `[1]`: probe flags. Low three bits are a **participation mask** matched against the prober's own mask: `0x01` the player pass (`game_loop`), `0x02` the Tyrant pass, `0x04` the room-object push pass. `0x40` tests the prober entity's own position instead of the 600-unit forward point; `0x80` makes the entry action-key only, skipped by the per-frame passes. |

### `0x24` — `cmd_room_action`

| Key | Value |
|---|---|
| Index | `0x24` |
| Address | `0x004312b0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `room_check_actions[actionIdx](&g_RoomActionTable[slot * 0xC])` — runs one of the 18 room-action handlers (`0x00`–`0x11`, see [SCD_SCRIPT_SYSTEM.md](SCD_SCRIPT_SYSTEM.md) §4). Out-of-range or unimplemented action index is a guarded no-op in the decomp. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x24` |
| +1 | itemSlot | u8 | Room action slot passed to the handler. |
| +2 | actionIdx | u8 | Index into `room_check_actions` (`0x004b9340`). |
| +3 | pad | u8 | Unused. |

### `0x2D` — `cmd_got_item`

| Key | Value |
|---|---|
| Index | `0x2D` |
| Address | `0x00431a20` |
| Length | 4 bytes (consumed by the nested `cmd_room_action`) |
| Returns | `0` (always stops the stream) |
| Description | Runs the same function as opcode `0x24` (so it consumes the same operand bytes), then sets `g_main_state_flags |= 0x400` and toggles `0x800`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x2D` |
| +1 | itemSlot | u8 | As `0x24`. |
| +2 | actionIdx | u8 | As `0x24`. |
| +3 | pad | u8 | As `0x24`. |

### `0x4C` — `cmd_item_record_transfer`

| Key | Value |
|---|---|
| Index | `0x4C` |
| Address | `0x004322d0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Moves a pick-up **quantity** between a room action record and one BioCard state byte. Record byte 8 is the item id and byte 9 its quantity, so mode 0 remembers the count, mode 1 restores it and mode 2 loads the quantity the player is actually carrying into both. The three uses found are all mode 1 on `pickupQtyA/B/C` (state indices 12/13/14): ROOM1160 restores a shotgun (7 shells), ROOM30B0 and ROOM3080 a flamethrower (240 fuel each). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x4C` |
| +1 | mode | u8 | `0` = record byte 9 → state byte, `1` = state byte → record byte 9, `2` = copy the inventory quantity of record byte 8 into both. |
| +2 | slotIdx | u8 | Room action slot. |
| +3 | fieldIdx | u8 | Byte index into the `g_stageId` state array. |

---

## Inventory / items

### `0x10` — `cmd_used_item_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x10` |
| Address | `0x00460f30` |
| Length | 2 bytes |
| Returns | bool: `itemId == g_usedItemId` |
| Description | Tests the item id the player just used. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x10` |
| +1 | itemId | u8 | Item id to compare against. |

### `0x11` — `cmd_picked_item_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x11` |
| Address | `0x00460f10` |
| Length | 2 bytes |
| Returns | bool: `itemId == g_pickedItemId` |
| Description | Tests the item id recorded by the last pickup (`g_pickedItemId`, `0x00be9833`). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x11` |
| +1 | itemId | u8 | Item id to compare against. |

### `0x1A` — `cmd_item_search` (Cond)

| Key | Value |
|---|---|
| Index | `0x1A` |
| Address | `0x00460f80` |
| Length | 2 bytes |
| Returns | bool: found |
| Description | Scans the inventory (`g_ItemSlotsPointer`, stride 2, `g_TotalHeldItems` slots) for `itemId`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x1A` |
| +1 | itemId | u8 | Item id to search for. |

### `0x1D` — `cmd_equipped_item_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x1D` |
| Address | `0x00460ee0` |
| Length | 2 bytes |
| Returns | bool |
| Description | Compares the item id in the currently equipped inventory slot against `itemId`. Not weapon-specific — every sampled use tests `0`, i.e. "nothing equipped". |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x1D` |
| +1 | itemId | u8 | Item id to compare against the equipped slot. |

### `0x22` — `cmd_item_count_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x22` |
| Address | `0x00431100` |
| Length | 4 bytes |
| Returns | bool (`0` when no matching slot is held) |
| Description | Sums inventory quantities for an ammo/item *family* selected by `searchId` and compares the total against `cmpVal`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x22` |
| +1 | searchId | u8 | Family selector: `0x0A` = any, `0x0B` = item 2, `0x0C` = item 3, `0x0D` = items 4/5, `0x0F` = item 6, `0x10`–`0x12` = items 7/8/9. |
| +2 | mode | u8 | `0` `==`, `1` total `>`, `2` total `>=`, `3` total `<`, `4` total `<=`, `5` `!=`. |
| +3 | cmpVal | u8 | Comparison constant. |

### `0x2C` — `cmd_item_remove` (Cond)

| Key | Value |
|---|---|
| Index | `0x2C` |
| Address | `0x004319e0` |
| Length | 2 bytes |
| Returns | bool: `1` if removed, `0` if the item was absent |
| Description | Zeroes the inventory slot holding `itemId` and calls `rearrange_item_slots`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x2C` |
| +1 | itemId | u8 | Item id to remove. |

---

## Event scripts

### `0x14` — `cmd_scd_event_create`

| Key | Value |
|---|---|
| Index | `0x14` |
| Address | `0x00461040` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `ScdEventEntry_Create(slot, scriptIdx)` — starts event script `scriptIdx` in event slot `slot` (slot > 7 allocates the first free slot). The original reads the slot/script pair as one word; byte-wise reads made scriptIdx always 0. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x14` |
| +1 | pad | u8 | Unused. |
| +2 | slot | u8 | Event slot index. |
| +3 | scriptIdx | u8 | Index into `g_RoomEventScripts`. |

### `0x44` — `cmd_scd_event_kill`

| Key | Value |
|---|---|
| Index | `0x44` |
| Address | `0x00461080` |
| Length | 2 bytes |
| Returns | `1` |
| Description | `g_ScdEventTable[slot].active = 0` — deactivates a running event script. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x44` |
| +1 | slot | u8 | Event slot index. |

### `0x39` — `cmd_enemy_flags_get`

| Key | Value |
|---|---|
| Index | `0x39` |
| Address | `0x00431e10` |
| Length | 2 bytes |
| Returns | `1` |
| Description | `g_scdLastEnemyFlags = g_EnemiesList[enemyIdx].behavior_flags` — stashes an enemy's behavior flags for later tests. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x39` |
| +1 | enemyIdx | u8 | Index into `g_EnemiesList`. |

---

## Sound / BGM

### `0x15` — `cmd_bgm_play`

| Key | Value |
|---|---|
| Index | `0x15` |
| Address | `0x00460a80` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Starts a BGM channel: `SetSndSlot(handle, slot)` on the `g_SndBank[ch]` record (when its handle is non-zero) and `g_BGM_STATE |= 1 << (ch+3)`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x15` |
| +1 | ch | u8 | Sound channel index (0–3) into `g_SndBank` (8-byte records). |

### `0x16` — `cmd_bgm_stop`

| Key | Value |
|---|---|
| Index | `0x16` |
| Address | `0x00460c70` |
| Length | 2 bytes |
| Returns | `1` |
| Description | If `g_BGM_STATE` bit `ch+3` is set: `setSndStop(handle)`, clears the bit and `set_volume(handle, -1)`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x16` |
| +1 | ch | u8 | Sound channel index. |

### `0x17` — `cmd_sfx_3d_play`

| Key | Value |
|---|---|
| Index | `0x17` |
| Address | `0x00460d80` |
| Length | 10 bytes (posType 0–3) / 6 bytes (posType > 3) |
| Returns | `1` |
| Description | Plays a 3D sound. Positions come from the 3-int `scaMatrixData.localMatrix.t` of the entity, not the packed `position` SVECTOR. posType 0 writes the explicit coordinates into `g_playerPosScratch` (`0x00be11b0`, three ints, y forced 0); posType 3 plays non-positionally via `play_sfx`; posType > 3 plays nothing. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x17` |
| +1 | bank | u8 | Sound bank / type id. |
| +2 | sndId | u8 | Sound id within the bank. |
| +3 | vol | s8 | Volume. |
| +4 | posType | u8 | `0` explicit, `1` player, `2` enemy, `3` non-positional. |
| +5 | enemyIdx | u8 | Enemy index used when `posType == 2`. |
| +6 | posX | s16 | X coordinate (posType 0 only). |
| +8 | posZ | s16 | Z coordinate (posType 0 only). |

### `0x1E` — `cmd_voice_play`

| Key | Value |
|---|---|
| Index | `0x1E` |
| Address | `0x00461a80` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Starts or ends a cutscene **voice** line (`0x17` is the sound-effect command). `play_sound_and_voice_effect` type 1 loads and plays the line, type 2 ends it, resets the mixer and clears the wait flag. Raising `MSF_VOICE_PLAYING` (`0x20000`) here is what event-VM opcode `0xF7` blocks on, so this gates a scripted line advancing. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x1E` |
| +1 | type | u8 | Sound/voice type. |
| +2 | param | u16 | Sound id / parameter word. |

### `0x27` — `cmd_snd_fade_set`

| Key | Value |
|---|---|
| Index | `0x27` |
| Address | `0x00460cf0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | `BuildSndFadeTbl(fadeType, 0x7F)` — builds a sound fade table. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x27` |
| +1 | fadeType | u8 | Fade profile selector. |

### `0x2F` — `cmd_snd_pan_vol_set`

| Key | Value |
|---|---|
| Index | `0x2F` |
| Address | `0x00460c00` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `FUN_004805d0`, then stores the pan/volume pair in the 8-byte `g_SndPanVol[ch]` record (`DAT_00ac98e0`/`DAT_00ac98e4` in the original). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x2F` |
| +1 | ch | u8 | Sound channel index. |
| +2 | pan | u8 | Pan parameter. |
| +3 | volume | u8 | Volume parameter. |

### `0x43` — `cmd_bgm_volume_ramp`

| Key | Value |
|---|---|
| Index | `0x43` |
| Address | `0x00460d20` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Only acts when `g_BGM_STATE` bit `ch+3` is set: `FUN_004804a0` ramps the channel volume. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x43` |
| +1 | ch | u8 | Sound channel index. |
| +2 | a | s8 | Ramp parameter 1. |
| +3 | b | u8 | Ramp parameter 2. |

### `0x4A` — `cmd_bgm_restore`

| Key | Value |
|---|---|
| Index | `0x4A` |
| Address | `0x00460ae0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | No-op unless `g_targetBgmState != 0xFF`. Restores the three channels saved by `0x4B`: `g_BGM_STATE >>= 8`, then `SetSndSlot` per set bit (`0x08`/`0x10`/`0x20`). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x4A` |
| +1 | pad | u8 | Unused. |

### `0x4B` — `cmd_bgm_stop_all`

| Key | Value |
|---|---|
| Index | `0x4B` |
| Address | `0x00460b80` |
| Length | 2 bytes |
| Returns | `1` |
| Description | No-op unless `g_targetBgmState != 0xFF`. Stops all sound banks (and `g_BgmSoundBank`), then `g_BGM_STATE <<= 8` to save the live channel mask into the high byte. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x4B` |
| +1 | pad | u8 | Unused. |

### `0x37` — `cmd_room_bgm_state_set`

| Key | Value |
|---|---|
| Index | `0x37` |
| Address | `0x00460a30` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `g_roomBgmState[stage * 32 + roomIdx] = value` — sets the remembered BGM state for a room. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x37` |
| +1 | stage | u8 | Stage id (row, 32 rooms per stage). |
| +2 | roomIdx | u8 | Room id (column). |
| +3 | value | u8 | BGM state byte. |

---

## Models / objects

### `0x0F` — `cmd_mirror_set`

| Key | Value |
|---|---|
| Index | `0x0F` |
| Address | `0x004610b0` |
| Length | 8 bytes |
| Returns | `1` |
| Description | Mirror-room setup: writes `modeBits` into `g_main_state_flags` bits 0–1, sets the mirror plane globals, re-runs `SetupEntityJointAnimation` on the player and rebuilds the weapon-joint clone (`FUN_0048bfe0` / `FUN_0048c020`). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x0F` |
| +1 | modeBits | u8 | Written into `g_main_state_flags` bits 0–1. |
| +2 | mirrorMin | u16 | `g_mirrorExtentMin`. |
| +4 | mirrorMax | u16 | `g_mirrorExtentMax`. |
| +6 | planeCoord | u16 | `g_mirrorPlaneCoord`. |

### `0x18` — `cmd_item_model_set`

| Key | Value |
|---|---|
| Index | `0x18` |
| Address | `0x00461220` |
| Length | 26 bytes |
| Returns | `1` |
| Description | Item model setup: loads the RDT item TMD, initialises the item model record in `g_item_model_table` (SCA hierarchy, transform, `+0x86` billboard handle) and fills event entry `slot`. May spawn a billboard effect (the item sparkle) when the room flag is set and the flags word has bit `0x8000`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x18` |
| +1 | slot | u8 | Bits 0–6: event slot index. Bit 7: alternate rotation (`0x40` rotation word). |
| +2 | record | u8[24] | Obstacle record (RDT obstacle data), also stored whole via entry pointer `+8`; see the record layout below. |

Obstacle record layout (starts at opcode stream `+2`, so record offset = stream offset − 2):

| Record offset | Key | Type | Description |
|---|---|---|---|
| +0x00 | unused0 | u8[8] | Not read by the handler; the record is referenced whole via the entry pointer. |
| +0x08 | itemType | u8 | Item id/code. The `0x54` cut-off is `<= ITEM_MAP_LAST` (`0x53`), which selects the map pickup-handler shape. Four ids get special handling: `ITEM_MAP_COURTYARD` (`0x50`) and `ITEM_MAP_GUARDHOUSE` (`0x52`) have their 256-entry palette darkened, `ITEM_INK_RIBBONS` (`0x2F`) is skipped for Jill on a first playthrough, and `ITEM_CRANK_HEX` (`0x1E`) attaches an extra model. |
| +0x0A | modelIdx | u8 | Index into `g_item_model_table` / RDT `item_models`; also event entry word `+4`. |
| +0x0B | scaParent | u8 | Whose matrix the item hangs off — this is what the posX/Y/Z below are *relative to*. `0xFF` none (absolute room coords), `0xFE` player, else `g_omodel_table[value]`. See [SCA parent](#0x18-sca-parent) below. |
| +0x0C | posX | s16 | Position X (also object `+0x34`/`+0x6C`). |
| +0x0E | posY | s16 | Position Y (also object `+0x38`/`+0x6E`). |
| +0x10 | posZ | s16 | Position Z (also object `+0x3C`/`+0x70`). |
| +0x12 | animWord | u16 | Written to object `+0x74`. |
| +0x14 | flagBit | u8 | `g_roomItemsFlags` bit index gating visibility; also event entry word `+6`. |
| +0x15 | entryFlags | u8 | Event entry byte `[1]`. |
| +0x16 | flags | u16 | Bit 0 (masked back into the stream) → entry word `+2`; bit `0x8000` = spawn billboard; bits `0x0F00` = effect id; bits `0x00F0` = height bias (`-2` per unit). |

<a name="0x18-sca-parent"></a>

#### The `scaParent` byte — what coordinate space posX/Y/Z are in

The three position operands are written to `modelPtr + 0x34/0x38/0x3C`. That is
not an arbitrary scratch area: the item's `ScaMatrixData` starts at `+0x1C`, its
`localMatrix` at `+0x20`, and a `MATRIX`'s `t[]` sits at `+0x14` inside it — so
`+0x34` **is `localMatrix.t[0]`**. The operands are the item's own *local* matrix
translation, and `scaParent` decides what that local matrix gets composed with.
`InitScaMatrix(modelPtr+0x64, modelPtr+0x1C)` then seeds the hierarchy.

| Value | Parent pointer (`modelPtr+0x64`) | Sprite matrix | posX/Y/Z mean |
|---|---|---|---|
| `0xFF` | zeroed — none | the item's own, `modelPtr+0x20` | absolute room coordinates |
| `0xFE` | `&g_playerEntity + 0x1C` | `g_playerEntity.scaMatrixData.localMatrix` | offset from the player; the item moves with them |
| other | `g_omodel_table[value] + 0x1C` | that object's, `+0x20` | offset in that room object's local space; the item rides its transform |

Note there is **no `0x80` split here**, unlike `cmd_omodel_set` (`0x1F`) — every
value that is not `0xFF`/`0xFE` is an omodel index. `g_omodel_table` is the room's
*object model* table (RDT `object_models` at +0x50: furniture, doors, the item box
lid).

Across every shipped room only four values are ever used, and each shows the
mechanic cleanly:

- **`0xFF` — 570 of the 584 uses.** The ordinary pick-up lying on the floor:
  `ROOM1000  SWORD KEY  pos(5160,-930,8690)` is a literal room coordinate.
- **`0xFE` — 4 uses**, all the same flare: `ROOM3030  FLARE  pos(800,0,200)`,
  i.e. 800 units out from the player rather than anywhere in the room.
- **omodel — 10 uses.** `ROOM10D0` is the clearest. Omodel 0 is placed in world
  space at `(3505,0,4145)`; the wind crest and the Colt Python declare
  `PARENT 0x00` with `pos(-450,-1550,530)` and `(-450,-1500,-360)`. They are not
  at `-450` in the room — they are ~1550 units *above* omodel 0's origin (Y is
  negative-up), i.e. resting inside the lion statue and
  both items follow its rotation.
- **`(0,0,0)` with a parent means "exactly at the object".** `ROOM5130` sets
  omodel 6 twice from two different branches — `(10060,-20000,8080)` and
  `(10975,0,8464)` — and declares `MASTER KEY  PARENT 0x06  pos(0,0,0)`. The item
  declaration never changes; the key tracks whichever placement ran.

**Authoring rule:** an item on the floor takes `0xFF` and real room coordinates.
Reach for an omodel parent only when the item belongs *to* an object — on a desk,
in a drawer, on the item box lid — and especially when that object's own position
is script-dependent.

The handler branches on `0xFF` a second time when it spawns the sparkle billboard
(`0x00461220`, the `flags & 0x8000` block). Unparented, the billboard spawns at
`(0, heightBias, 0)` against the item's own matrix, which already puts it on the
item. Parented, the billboard hangs off the *parent's* matrix instead, so the
handler feeds it the posX/Y/Z operands to bring it back onto the item. Both paths
land in the same place; the asymmetry is only because the two cases start from
different matrices.

### `0x19` — `cmd_model_flag_set`

| Key | Value |
|---|---|
| Index | `0x19` |
| Address | `0x00460f50` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `*(u8*)g_item_model_table[modelIdx] = value` — writes the item model's byte 0 (bit 0 = drawn). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x19` |
| +1 | modelIdx | u8 | Index into `g_item_model_table`. |
| +2 | value | u8 | Byte value to write. |
| +3 | pad | u8 | Unused. |

### `0x1F` — `cmd_omodel_set`

| Key | Value |
|---|---|
| Index | `0x1F` |
| Address | `0x00461ac0` |
| Length | 28 bytes |
| Returns | `1` |
| Description | Static object model setup: loads the RDT item TMD (with several stage/room-specific palette and position fixups), initialises the object's SCA hierarchy and transform, and copies the sprite/anim parameter block into the object's entry data area. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x1F` |
| +1 | slot | u8 | Bits 0–5: omodel slot index (`g_omodel_table` / RDT `object_models`). Bit 7: queue texture for processing. |
| +2 | entryFlags | u8 | Stored to object byte 0; bit 4 = alternate rotation (`0x40000040`). |
| +3 | scaParent | u8 | `0xFF` none, `0xFE` player, `< 0x80` omodel index (`g_omodel_table[value]`), else `g_EnemiesList[value & 0x7F]`. Same relative-space meaning as [0x18's scaParent](#0x18-sca-parent). |
| +4 | posX | s16 | Position X (object `+0x6C`/`+0x34`). |
| +6 | posY | s16 | Position Y (object `+0x6E`/`+0x38`). |
| +8 | posZ | s16 | Position Z (object `+0x70`/`+0x3C`). |
| +0x0A | animWord | u16 | Written to object `+0x7E` and `+0x74`. |
| +0x0C | params | u8[16] | Sprite/anim parameter block copied word-by-word into the object entry data area; see the block layout below. |

Parameter block layout (starts at opcode stream `+0x0C`, so block offset = stream offset − 0x0C):

| Block offset | Key | Type | Description |
|---|---|---|---|
| +0x00 | p0 | u16 | → object `+0x94`. |
| +0x02 | p1 | u16 | → object `+0x98`. |
| +0x04 | p2 | u16 | → object `+0x9C`. |
| +0x06 | p3 | u16 | → object `+0xA0`. |
| +0x08 | p4 | u16 | → object `+0x92`. |
| +0x0A | p5 | u16 | → object `+0x8C` and `+0x90`. |
| +0x0C | p6 | u16 | → object `+0x8A`. |
| +0x0E | p7 | u16 | → object `+0x8E`. |

### `0x35` — `cmd_obj_flag_set`

| Key | Value |
|---|---|
| Index | `0x35` |
| Address | `0x00431bf0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Writes `value` to byte 0 of an object pointer. Special case: stage 3 / room 13 / object 5 forces 0. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x35` |
| +1 | table | u8 | `0` = `g_omodel_table`, `1` = `g_item_model_table`. |
| +2 | objIdx | u8 | Object index. |
| +3 | value | u8 | Byte value to write. |

### `0x36` — `cmd_obj_field_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x36` |
| Address | `0x00431c90` |
| Length | 4 bytes |
| Returns | bool |
| Description | Compares the `u16` at `g_omodel_table[objIdx] + 0x86` (the billboard effect handle) against `cmpVal`. Unknown mode returns 0. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x36` |
| +1 | objIdx | u8 | Itembox object index. |
| +2 | mode | u8 | `0` `==`, `1` field `>`, `2` field `>=`, `3` cmp `>`, `4` cmp `>=`, `5` `!=`. |
| +3 | cmpVal | u8 | Comparison constant. |

### `0x3B` — `cmd_obj_rotation_set`

| Key | Value |
|---|---|
| Index | `0x3B` |
| Address | `0x00431ea0` |
| Length | 6 bytes |
| Returns | `1` |
| Description | Writes two rotation words at object `+0x72` and `+0x76` — but only when the object is active (byte 0 non-zero); otherwise the four operand bytes are skipped without effect. Selector asymmetry is original behaviour: item models use the raw byte, omodels mask with `0x7F`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x3B` |
| +1 | sel | u8 | High byte of the first word. Bit 7 clear = interactable model `(g_item_model_table[sel])`; set = room object `g_omodel_table[sel & 0x7F]`. |
| +2 | rotA | u16 | Written to object `+0x72`. |
| +4 | rotB | u16 | Written to object `+0x76`. |

### `0x47` — `cmd_obj_transform_set`

| Key | Value |
|---|---|
| Index | `0x47` |
| Address | `0x00431080` |
| Length | 14 bytes |
| Returns | `1` |
| Description | Sets rotation (`+0x72/+0x74/+0x76`) and position (`+0x6C/+0x6E/+0x70`, mirrored into `+0x34/+0x38/+0x3C`) of `g_omodel_table[objIdx]`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x47` |
| +1 | objIdx | u8 | Index into `g_omodel_table`. |
| +2 | rotX | s16 | Object `+0x72`. |
| +4 | rotY | s16 | Object `+0x74`. |
| +6 | rotZ | s16 | Object `+0x76`. |
| +8 | posX | s16 | Object `+0x6C` (mirrored to `+0x34`). |
| +10 | posY | s16 | Object `+0x6E` (mirrored to `+0x38`). |
| +12 | posZ | s16 | Object `+0x70` (mirrored to `+0x3C`). |

---

## Entities (player / enemies)

### `0x20` — `cmd_player_pos_set`

| Key | Value |
|---|---|
| Index | `0x20` |
| Address | `0x00430f60` |
| Length | 14 bytes |
| Returns | `1` |
| Description | Sets the player's packed position and clears `unk_e0` bits 2–3 (`&= 0xFFF3`). Position words mirror into `localMatrix.t[0..2]`. Note `+6` is `speed.x` (`0x76`) in `PlayerEntity`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x20` |
| +1 | pad | u8 | Unused. |
| +2 | posPad | s16 | `position.pad`. |
| +4 | directionAngle | s16 | `directionAngle`. |
| +6 | speedX | s16 | `speed.x`. |
| +8 | posX | s16 | `position.x` / `localMatrix.t[0]`. |
| +10 | posY | s16 | `position.y` / `localMatrix.t[1]`. |
| +12 | posZ | s16 | `position.z` / `localMatrix.t[2]`. |

### `0x21` — `cmd_enemy_pos_set`

| Key | Value |
|---|---|
| Index | `0x21` |
| Address | `0x00430fe0` |
| Length | 14 bytes |
| Returns | `1` |
| Description | Same operand shape as `0x20`, applied to `g_EnemiesList[enemyIdx]`. `enemyIdx` comes from an arithmetic shift of the signed first word (`(s16)word0 >> 8`). Clears `scd_entity_flags` (`+0xE0`) bits 2–3. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x21` |
| +1 | enemyIdx | s8 | Index into `g_EnemiesList` (arithmetic). |
| +2 | pad | s16 | `position.pad`. |
| +4 | yaw | s16 | Low half of `angle`. |
| +6 | pitch | s16 | High half of `angle`. |
| +8 | posX | s16 | `position.x` / `localMatrix.t[0]`. |
| +10 | posY | s16 | `position.y` / `localMatrix.t[1]`. |
| +12 | posZ | s16 | `position.z` / `localMatrix.t[2]`. |

### `0x1B` — `cmd_enemy_set`

| Key | Value |
|---|---|
| Index | `0x1B` |
| Address | `0x004617d0` |
| Length | 22 bytes |
| Returns | `1` |
| Description | Enemy spawn. If the `g_EnemiesFlags` guard bit is already set (`guardBit != 0xFF`), the spawn is skipped (stream still advances 22). Otherwise allocates the enemy slot, optionally force-initialises it, and registers it (`status_flags & 1` path sets id, death event, SCA pool etc.). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x1B` |
| +1 | enemyTypeId | u8 | Enemy type id (written to `id`). |
| +2 | behaviorFlags | u8 | `behavior_flags`. |
| +3 | guardBit | u8 | `g_EnemiesFlags` bit; `0xFF` = no guard. Spawn skipped when already set. |
| +4 | forceInit | u8 | Non-zero forces re-initialisation (and sets `pad_160[1]` bit 7). |
| +5 | scaHitWords | u8 | SCA hit-data size in 6-byte units (`g_scaPoolPtr += n * 6`). |
| +6 | posPad | s16 | `position.pad`. |
| +8 | yaw | u16 | Low half of `angle`. |
| +10 | pitch | u16 | High half of `angle`. |
| +12 | posX | u16 | `localMatrix.t[0]` / `position.x`. |
| +14 | posY | s16 | `localMatrix.t[1]` / `position.y`. |
| +16 | posZ | u16 | `localMatrix.t[2]` / `position.z`. |
| +18 | slot | u8 | Low nibble: enemy slot index (also stored in `pad_160[1]`). |
| +19 | animationId | u8 | `animationId`. |
| +20 | animFrameId | u8 | `animation_frame_id`. |
| +21 | extraFlags | u8 | High nibble shifted into `pad_160[1]` bits 4–7. |

### `0x28` — `cmd_enemy_prop_set`

| Key | Value |
|---|---|
| Index | `0x28` |
| Address | `0x004312f0` |
| Length | 4/6/8 bytes (see subCmd) |
| Returns | `1` |
| Description | Modifies one property of `g_EnemiesList[enemyIdx]`. Sub-commands select the field and the consumed length; an unknown subCmd consumes nothing (interpreter spins, as in the original). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x28` |
| +1 | pad | u8 | Unused. |
| +2 | enemyIdx | u8 | Index into `g_EnemiesList`. |
| +3 | subCmd | u8 | `0` behaviorFlags (len 6); `1` state=2 + health + hitState (len 8, byte +6 = hitState); `2` actionBehavior (len 6); `3` statusFlags SET/OR/XOR by `param>>8` (len 6); `5` yaw (len 6); `6` blendCounter=0 (len 4); `8` state=9 (len 4); `9` toggle joint flags by bitmask (len 6); `10` actionState (len 6). |
| +4 | param | u16 | Value / operand word (subCmd dependent). |
| +6 | extra | u8 | Only subCmd 1: `hit_state` byte. |

### `0x33` — `cmd_player_prop_set`

| Key | Value |
|---|---|
| Index | `0x33` |
| Address | `0x004314b0` |
| Length | 2/4 bytes (see subCmd) |
| Returns | `1` |
| Description | Multi-subcommand **player** property setter — the twin of `0x28` `cmd_enemy_prop_set`. Sub-commands: 0 clear equipped weapon, 1 enter the being-attacked animation, 3 write/or/xor `player.flags`, 4 force action 1/6, 5 `directionAngle`, 6 clear `unk_8c`, 7 reset to idle, 8 write/or/xor `healthStatusFlags`, 9 xor joint flags, 10 set/clear `unk_e0` bit `0x40`. Sampled scripts use only 10, 8 and 0 — the damage-adjacent sub-command 1 never turned up. Unknown subCmd consumes nothing (interpreter spins, as in the original). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x33` |
| +1 | subCmd | u8 | `0` unequip (len 2); `1` hit react: isBeingAttackedFlag + anim reset (len 4); `3` flags SET/OR/XOR (len 4); `4` actionBehavior=1/actionState=6 (len 2); `5` directionAngle=param (len 4); `6` clear unk_8c (len 2); `7` reset to idle (len 2); `8` healthStatusFlags SET/OR/XOR (len 4); `9` toggle joint flags by bitmask (len 4); `10` set/clear `unk_e0` bit `0x40` by `param>>8` (len 4). |
| +2 | param | u16 | Value / operand word (subCmd dependent). |

### `0x2B` — `cmd_attack_anim_set`

| Key | Value |
|---|---|
| Index | `0x2B` |
| Address | `0x00431990` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Scripted attack animation: sets `attackAnim`, derives `action_behavior`/`action_state` from `(behavior + 0x200) & 0xFF00`, sets `animation_frame_id` and `animationId = 8`, clears `unk_bf`/`unk_8c`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x2B` (low byte of the `behavior` word) |
| +1 | behaviorHi | u8 | High byte of the signed `behavior` word. |
| +2 | attackAnim | u8 | `attackAnim`. |
| +3 | animFrameId | u8 | `animation_frame_id`. |

### `0x41` — `cmd_entity_posy_set`

| Key | Value |
|---|---|
| Index | `0x41` |
| Address | `0x00432090` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Writes a `u16` to `g_playerEntity.posY` (entity `+0x8E`) when `entIdx == 0`, else to enemy `entIdx` at `+0x82`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x41` |
| +1 | entIdx | u8 | `0` = player, else enemy index. |
| +2 | value | u16 | Word to write. |

### `0x45` — `cmd_player_posy_add`

| Key | Value |
|---|---|
| Index | `0x45` |
| Address | `0x004320f0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | `g_playerEntity.posY += (s8)delta`. Player-only — unlike `0x41` it has no entity selector. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x45` |
| +1 | delta | s8 | Signed amount to add. |

### `0x4D` — `cmd_player_joint_tint`

| Key | Value |
|---|---|
| Index | `0x4D` |
| Address | `0x004323a0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Applies `JointApplyColorTint` (second argument `0x30`) to joints 0–13 of the player's `jointsStructs` (entity `+0x98`, stride `0x7C`), visiting them in the original's order `0,1,2,9,12,3,4,5,6,7,8,10,11,13`. The pushed `0x00606060` colour is a dead argument. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x4D` |
| +1 | pad | u8 | Unused. |

### `0x3C` — `cmd_player_dist_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x3C` |
| Address | `0x00431f20` |
| Length | 6 bytes |
| Returns | bool: `SquareRoot0(dx² + dz²) <= maxDist` |
| Description | Horizontal distance test between the player and a target entity. Unknown target type returns 0. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x3C` |
| +1 | pad | u8 | Unused. |
| +2 | targetSpec | u16 | Low byte: `0` enemy, `1` omodel (`g_omodel_table`), `2` item model. High byte: target index. |
| +4 | maxDist | u16 | Maximum distance. |

### `0x3F` — `cmd_player_dir_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x3F` |
| Address | `0x00431fd0` |
| Length | 6 bytes |
| Returns | bool |
| Description | Wrap-aware range test on `directionAngle`: `(u16)(dir - minAngle) <= (maxAngle - minAngle)`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x3F` |
| +1 | pad | u8 | Unused. |
| +2 | minAngle | u16 | Range start (4096-degree units). |
| +4 | maxAngle | u16 | Range end. |

### `0x38` — `cmd_dpad_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x38` |
| Address | `0x00431dc0` |
| Length | 4 bytes |
| Returns | bool |
| Description | Tests `g_PlayerDpadHeld & mask`; `invert != 0` returns the negation. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x38` |
| +1 | invert | u8 | `0` = any set bit passes, non-zero = all clear passes. |
| +2 | mask | u16 | D-pad bitmask. |

---

## Effects

### `0x2A` — `cmd_effect_spawn`

| Key | Value |
|---|---|
| Index | `0x2A` |
| Address | `0x004316c0` |
| Length | 12 bytes |
| Returns | `1` |
| Description | `Effect_CreateBillboard` — spawns a billboard effect. The position words are sign-extended here (unlike `0x3D`) and passed as one `VECTOR` (original used three adjacent stack dwords). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x2A` |
| +1 | type | u8 | Effect type id. |
| +2 | parentIdx | u8 | Low byte of the parent word; passed to `Effect_CreateBillboard` as its second argument. |
| +3 | parentType | u8 | High byte of the parent word. `0` identity matrix, `1` player matrix, `2..0x7F` effect-pool matrix (`g_effectPool[type * 3 + 0x3D]`, past the 64-slot pool — the original overruns here too), `0x80..0xFF` omodel matrix `g_omodel_table[type & 0x7F]`. The `0x8000` test is on this parent word, not on the trailing flags word. |
| +4 | posX | s16 | Spawn X. |
| +6 | posY | s16 | Spawn Y. |
| +8 | posZ | s16 | Spawn Z. |
| +10 | flags | u16 | Effect flags word. |

### `0x3D` — `cmd_bullet_effect_spawn`

| Key | Value |
|---|---|
| Index | `0x3D` |
| Address | `0x00431770` |
| Length | 12 bytes |
| Returns | `1` |
| Description | Same layout as `0x2A`, but position words are zero-extended (unsigned) in the original. Stashes the effect type in `g_bulletEffectId` and the sprite matrix in `DAT_00bf0a34` for the paired `0x3E`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x3D` |
| +1 | type | u8 | Effect type id. |
| +2 | parentIdx | u8 | Sprite parent index. |
| +3 | parentType | u8 | `0` identity, `1` player; else effect-pool (degenerates to pool slot `0x3D`, original behaviour) or omodel matrix (degenerates to omodel 0, original behaviour). |
| +4 | posX | u16 | Spawn X (zero-extended). |
| +6 | posY | u16 | Spawn Y (zero-extended). |
| +8 | posZ | u16 | Spawn Z (zero-extended). |
| +10 | flags | u16 | Effect flags word. |

### `0x3E` — `cmd_bullet_effect_clear`

| Key | Value |
|---|---|
| Index | `0x3E` |
| Address | `0x00431840` |
| Length | 2 bytes |
| Returns | `1` |
| Description | `FUN_0047cf80(9, g_bulletEffectId, 0, 0, DAT_00bf0a34)` — a criteria-masked slot remover: frees every effect-pool slot whose type and sprite matrix match the values stashed by `0x3D`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x3E` |
| +1 | pad | u8 | Unused. |

### `0x42` — `cmd_effect_clear_typed`

| Key | Value |
|---|---|
| Index | `0x42` |
| Address | `0x00431870` |
| Length | 4 bytes |
| Returns | `1` |
| Description | `FUN_0047cf80(3, type, param, 0, 0)` — removes effect-pool slots matching `type`/`param`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x42` |
| +1 | type | u8 | Effect type to clear. |
| +2 | param | u16 | Additional match parameter. |

### `0x48` — `cmd_effect_pool_clear`

| Key | Value |
|---|---|
| Index | `0x48` |
| Address | `0x004318a0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Clears `animId`/`updateId` on all 64 effect-pool slots. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x48` |
| +1 | pad | u8 | Unused. |

### `0x4E` — `cmd_effect_flags_modify`

| Key | Value |
|---|---|
| Index | `0x4E` |
| Address | `0x00431910` |
| Length | 4 bytes |
| Returns | `1` |
| Description | OR / AND-NOT / XOR `mask` into the flags word (effect `+0x0E`) of every live effect-pool slot (`animId` or `updateId` non-zero). |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x4E` |
| +1 | mode | u8 | `0` = OR, `1` = AND-NOT, `2` = XOR. |
| +2 | mask | u16 | Bitmask to apply. |

---

## Lights / boundaries / sprites

### `0x1C` — `cmd_room_light_fade_set`

| Key | Value |
|---|---|
| Index | `0x1C` |
| Address | `0x00462210` |
| Length | 6 bytes |
| Returns | `1` |
| Description | Special room light: a non-zero `delta` seeds `g_SpecialRoomLightState` to `0` (fade up) or `0x7FFF` (fade down) by sign. Mask bits force B/G/R channels to `0xFF`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x1C` |
| +1 | lightR | u8 | `g_SpecialRoomLightR`. |
| +2 | delta | s16 | `g_SpecialRoomLightDelta` (sign selects fade direction). |
| +4 | rgbMask | u16 | Bit 0 = B, bit 1 = G, bit 2 = R → channel set to `0xFF`. |

### `0x40` — `cmd_light_param_set`

| Key | Value |
|---|---|
| Index | `0x40` |
| Address | `0x00432010` |
| Length | 16 bytes |
| Returns | `1` |
| Description | Writes seven `s16` into light `lightIdx` of `g_RdtPointer[1].lights` (stride `0x2C`, base − 4): dwords `[0..5]` and `[8]`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x40` |
| +1 | lightIdx | u8 | Light index in the RDT light table. |
| +2 | w0 | s16 | Light dword `[0]`. |
| +4 | w1 | s16 | Light dword `[1]`. |
| +6 | w2 | s16 | Light dword `[2]`. |
| +8 | w3 | s16 | Light dword `[3]`. |
| +10 | w4 | s16 | Light dword `[4]`. |
| +12 | w5 | s16 | Light dword `[5]`. |
| +14 | w6 | s16 | Light dword `[8]`. |

### `0x46` — `cmd_room_lights_set`

| Key | Value |
|---|---|
| Index | `0x46` |
| Address | `0x00432110` |
| Length | 44 bytes |
| Returns | `1` |
| Description | Overwrites the three RDT room lights (`RDT.lights[0..2]`: position ints at `+0x00/04/08`, RGB bytes at `+0x0C/0D/0E`, word at `+0x10`, radius `s16` at `+0x12`), then the three ambient words into `RDT+6/8/10`, then calls `setBackColor` with them. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x46` |
| +1 | pad | u8 | Unused. |
| +2 | light0 | 12 bytes | Light record → `RDT.lights[0]`; see the record layout below. |
| +14 | light1 | 12 bytes | Same record shape → `RDT.lights[1]`. |
| +26 | light2 | 12 bytes | Same record shape → `RDT.lights[2]`. |
| +38 | ambientR | s16 | → `RDT+6`, passed to `setBackColor`. |
| +40 | ambientG | s16 | → `RDT+8`. |
| +42 | ambientB | s16 | → `RDT+10`. |

Light record layout (12 bytes, repeated for `light0`/`light1`/`light2`; record offset is relative to each record's start):

| Record offset | Key | Type | Description |
|---|---|---|---|
| +0x00 | x | s16 | Light position X (sign-extended into the RDT light position int). |
| +0x02 | y | s16 | Light position Y. |
| +0x04 | z | s16 | Light position Z. |
| +0x06 | r | u8 | Red component. |
| +0x07 | g | u8 | Green component. |
| +0x08 | b | u8 | Blue component. |
| +0x09 | zero | u8 | Stored zero-extended into the RDT light `zero2` word. |
| +0x0A | radius | s16 | Light radius. |

### `0x30` — `cmd_boundary_set`

| Key | Value |
|---|---|
| Index | `0x30` |
| Address | `0x00431a40` |
| Length | 12 bytes |
| Returns | `1` |
| Description | Rewrites a boundary record `boundaries[listIdx][boundIdx]` (stride `0xC`): words `[2]`, `[3]`, `[0]`, `[1]` in that order. A non-zero `flagMode` rewrites bits `0x0F00` of `boundary[5]`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x30` |
| +1 | listIdx | u8 | Boundary list index. |
| +2 | boundIdx | u8 | Record index within the list. |
| +3 | flagMode | u8 | Non-zero: patch `boundary[5]` bits `0x0F00` with this value's `0x0F00`. |
| +4 | z | u16 | → `boundary[2]`. |
| +6 | w | u16 | → `boundary[3]`. |
| +8 | x | u16 | → `boundary[0]`. |
| +10 | y | u16 | → `boundary[1]`. |

### `0x25` — `cmd_room_sprite_set`

| Key | Value |
|---|---|
| Index | `0x25` |
| Address | `0x004621d0` |
| Length | 4 bytes |
| Returns | `1` |
| Description | Enables or disables a room sprite. The original tests the high byte of the second word to pick the branch. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x25` |
| +1 | pad | u8 | Unused. |
| +2 | sprId | u8 | Sprite id. |
| +3 | disable | u8 | `0` → `RoomSpr_SetActive`, non-zero → `RoomSpr_SetInactive`. |

### `0x49` — `cmd_room_sprite_hide`

| Key | Value |
|---|---|
| Index | `0x49` |
| Address | `0x00432290` |
| Length | 2 bytes |
| Returns | `1` |
| Description | Queues a room sprite id for hiding: sets bit `id & 0x1F` of the pending mask (`DAT_00d22770`), or wipes the mask when `id == 0xFF`. `Room_ApplySpriteFlags` (`0x00432220`) walks it every frame, sets `active = 0` on every room sprite whose id is a set bit, then clears it. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x49` |
| +1 | bit | u8 | Bit index (`0xFF` = clear all). |

### `0x34` — `cmd_model_tint_set`

| Key | Value |
|---|---|
| Index | `0x34` |
| Address | `0x00431b10` |
| Length | 8 bytes |
| Returns | `1` |
| Description | Model/texture tint: `variant` selects the apply function (`0` → `scd_model_tint_apply`, `1` → `FUN_00473d10`, `2` → `FUN_00473d60`); `bias` is a signed offset from `0x80`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x34` |
| +1 | variant | u8 | Tint function selector (0/1/2). |
| +2 | bias | s8 | Signed bias (byte − 0x80). |
| +3 | p3 | u8 | Parameter 3. |
| +4 | p4 | u8 | Parameter 4. |
| +5 | p5 | u8 | Parameter 5. |
| +6 | p6 | u8 | Parameter 6. |
| +7 | p7 | u8 | Parameter 7. |

---

## Streaming / skipping / script flag

Note: event-VM state-0 opcode `0x06` runs an inline SCD block through
`run_command_functions`, and event opcode `0x07` dispatches a single command
through this table — both are documented in
[SCD_SCRIPT_SYSTEM.md](SCD_SCRIPT_SYSTEM.md) §3 and are not part of the
81-entry table.

### `0x0E` — `cmd_skip_2bytes_opcode`

| Key | Value |
|---|---|
| Index | `0x0E` |
| Address | `0x00460900` |
| Length | 2 bytes |
| Returns | `1` |
| Description | No-op that consumes one operand byte. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x0E` |
| +1 | pad | u8 | Unused. |

### `0x32` — `cmd_skip_4bytes`

| Key | Value |
|---|---|
| Index | `0x32` |
| Address | `0x00431b00` |
| Length | 4 bytes |
| Returns | `1` |
| Description | No-op that consumes three operand bytes. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x32` |
| +1 | pad0 | u8 | Unused. |
| +2 | pad1 | u8 | Unused. |
| +3 | pad2 | u8 | Unused. |

### `0x4F` — `cmd_costume_variant_set`

| Key | Value |
|---|---|
| Index | `0x4F` |
| Address | `0x004622b0` |
| Length | 2 bytes |
| Returns | `1` |
| Description | `FUN_0040c560(param)` — stores `param & 1` into `g_bCostumeVariant` (`0x004d6444`), the costume-variant selector consumed by `LoadEntityEMD` and persisted in the save block at `0xA01`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x4F` |
| +1 | param | u8 | Bit 0 becomes `g_bCostumeVariant`. |

### `0x50` — `cmd_costume_variant_test` (Cond)

| Key | Value |
|---|---|
| Index | `0x50` |
| Address | `0x004622e0` |
| Length | 2 bytes |
| Returns | bool: `g_bCostumeVariant` |
| Description | Returns `g_bCostumeVariant`, the bit `0x4F` wrote. The original consumes the 2-byte instruction and returns the byte directly — there is no callee, and the operand byte is ignored. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x50` |
| +1 | pad | u8 | Unused. |

---

## Dead slots

### `0x26` — `cmd_dead_slot_hang_26`

| Key | Value |
|---|---|
| Index | `0x26` |
| Address | `0x00460ce0` |
| Length | 0 bytes (consumes nothing) |
| Returns | the opcode byte itself (`0x26`) — non-zero |
| Description | Dead table slot: the original is a bare `RET` that never touches `EAX`, so the dispatcher's stale opcode value is returned and the interpreter spins forever. No shipped script contains it. Reproduce as-is; returning 0 would diverge by silently ending the block. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x26`. No payload; the stream pointer is never advanced. |

### `0x2E` — `cmd_dead_slot_hang_2e`

| Key | Value |
|---|---|
| Index | `0x2E` |
| Address | `0x00460a70` |
| Length | 0 bytes (consumes nothing) |
| Returns | the opcode byte itself (`0x2E`) — non-zero |
| Description | Dead table slot, identical to `0x26`. |

| Offset | Key | Type | Description |
|---|---|---|---|
| +0 | opcode | u8 | `0x2E`. No payload; the stream pointer is never advanced. |
