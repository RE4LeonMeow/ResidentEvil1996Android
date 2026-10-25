# RDT File Format (Room Definition Table)

RDT files (.rdt) define all data for a single room in Resident Evil 1.
Each room has its own .rdt file located at:

    .\usa\stage{S}\room{S}{RR}{V}.rdt

Where `S` is the stage ID + 1 (hex digit), `RR` is the room ID (two hex
digits), and the trailing digit `V` is the **scenario/character selector**:
`'0'` for the Chris scenario, `'1'` for Jill — chosen by `g_main_state_flags`
bit 0x800000, which character selection sets whenever the picked character ID
is non-zero (`CharacterSelectionScreen.cpp:700`, `SaveLoadScreen.cpp:662`;
built by `LoadRoomRdt`, 0x00477d90, from the path template at 0x004c44f8).
E.g. stage 1 room 06 is `.\usa\stage1\room1060.rdt`.

## File Structure

The RDT file is a contiguous binary blob. After loading via `LoadFile()`, all
embedded relative pointers are relocated to absolute memory addresses by
`LoadRoomRdt()` (0x00477d90). The C++ mirror of the whole structure lives in
`src/game/Types.h` (`struct RDT`, `RDT_Light`, `RDT_Camera`, `CAM_SWITCH_ZONE`,
`RDT_Boundary`, `RDT_BoundaryHeader`).

The layout below describes the file **as shipped**; several fields change
meaning once relocated or rewritten at load time. Those cases are called out.

### Header (0x00 - 0x47)

| Offset | Size  | Type   | Description |
|--------|-------|--------|-------------|
| 0x00   | 1     | uchar  | `sprites_count` — number of active room sprite entries (`g_RoomSprEntries`). Overwritten at runtime by `Room_LoadCameraSprites` (0x004757c0), so the file value is never used as-is |
| 0x01   | 1     | uchar  | `cameras_count` — number of camera entries following the header (also drives background PAK loading) |
| 0x02   | 1     | uchar  | `omodel_slot_count` — number of `{TMD,TIM}` model pairs at `object_models` **and** number of 0xA4-byte omodel records `room_set` carves out of the VB block. Never read by any sound path on the PC engine |
| 0x03   | 1     | uchar  | `item_count` — number of `{TMD,TIM}` pairs at `item_models`, of 0xA4-byte item/interactable records carved after the omodel records (`g_item_model_table`), and of icon tiles at `item_icons`. The original format's `nItem` |
| 0x04   | 2     | uchar  | `pad_04` — reserved. No reader anywhere in the decompiled binary, and zero in all 320 shipped RDT files |
| 0x06   | 2     | short  | Ambient light — Red component (12-bit PS1 channel) |
| 0x08   | 2     | short  | Ambient light — Green component (12-bit PS1 channel) |
| 0x0A   | 2     | short  | Ambient light — Blue component (12-bit PS1 channel) |
| 0x0C   | 0x3C  | Light[3]| Room-wide light set, see below |

Ambient light is fed straight into `setBackColor()`, which scales the 12-bit
channels by 255/4096. Reading these as bytes instead of shorts under-exposes
every room 8x (see the comment in `LoadRoomRdt`, `src/game/RoomInit.cpp`).

### Lights (0x0C - 0x47)

3 light structures, each 0x14 (20) bytes. One room-wide set shared by every
camera — `update_entity_lighting` walks them at `g_RdtPointer + i*0x14`
(SCD command 0x47 can swap them at runtime; the computer-lab cutscene backs up
and restores all three):

| Offset | Size  | Type   | Description |
|--------|-------|--------|-------------|
| 0x00   | 4     | int    | Position X |
| 0x04   | 4     | int    | Position Y |
| 0x08   | 4     | int    | Position Z |
| 0x0C   | 1     | uchar  | Red component |
| 0x0D   | 1     | uchar  | Green component |
| 0x0E   | 1     | uchar  | Blue component |
| 0x0F   | 1     | uchar  | Zero (padding) |
| 0x10   | 2     | ushort | Light type — ONE 16-bit field. 0 = point light with radial falloff, non-zero = directional (colour passed through unchanged). Selected by a **word** compare at 0x00481673; modelling it as two bytes made high-byte-coloured lights render wrong |
| 0x12   | 2     | short  | Light radius (point lights only) |

### Data Type Pointers (0x48 - 0x93)

All 19 pointer fields are relative offsets within the .rdt file, stored as
`target - (file_base + 3)` — a PS1-era bias. `LoadRoomRdt` relocates every
dword from 0x48 to 0x94 by adding the load base minus 3.

| Offset | Size | Type   | Field (Types.h)          | Description | Asset |
|--------|------|--------|--------------------------|-------------|-------|
| 0x48   | 4    | ptr    | `cam_switch_zones`       | Camera switch zone table (see below) | — |
| 0x4C   | 4    | ptr    | `boundaries`             | Room collision boundaries (see `.blk` section) | .blk |
| 0x50   | 4    | ptr    | `object_models`          | Room-object (omodel) models — pushable/climbable objects: `omodel_slot_count` × `{TMD*, TIM*}` pairs, consumed by SCD 0x1F | .tmd/.tim |
| 0x54   | 4    | ptr    | `item_models`            | Item models — the 3D model of each pick-up in the room: `item_count` × `{TMD*, TIM*}` pairs, consumed by `cmd_item_model_set` (SCD 0x18) into `g_item_model_table` | .tmd/.tim |
| 0x58   | 4    | ptr    | `walk_zones`             | Walkable-zone grid used for NPC navigation (see below) | — |
| 0x5C   | 4    | ptr    | `footstep_sound_zones`   | Footstep sound zone map (see below) | .flr |
| 0x60   | 4    | ptr    | `initialization_scd`     | Init script (run once at room load) | .scd |
| 0x64   | 4    | ptr    | `scd_opcodes`            | Main script (run every frame) | .scd |
| 0x68   | 4    | ptr    | `scd_opcodes2`           | Event script table (see SCD section) | .scd |
| 0x6C   | 4    | ptr    | `player_anim_header`     | Room-specific player animation **header**, copied into `g_playerEntity.jointMoveData2` by `room_set` and driven with `Joint_move` (e.g. the scripted door-push / stair motions in `PlayerAnimations.cpp`) | — |
| 0x70   | 4    | ptr    | `player_anim_base`       | Matching animation **frame-data base** → `jointMoveData3` | — |
| 0x74   | 4    | ptr    | `messages`               | Message text table (see `.msg` section) | .msg |
| 0x78   | 4    | ptr    | `item_icons`             | `item_count` × 1200-byte inventory icons (40×30, 8bpp), the same tiles as `Data/ITEM_ALL.tim`. PS1 leftover — no reader anywhere in the decompiled binary (see below) | — |
| 0x7C   | 4    | ptr    | `effect_anim_index`      | Effect animation index block | .esp |
| 0x80   | 4    | ptr    | `effect_anim_data`       | Effect animation frame data | .eff |
| 0x84   | 4    | ptr    | `effect_anim_sprite`     | 8 dwords of effect sprite-sheet relative pointers (see below) | .tim |
| 0x88   | 4    | ptr    | `sound_attribute_table`  | Sound attribute table (.snd). Relocated like the rest but **never dereferenced** by the PC engine — PS1 leftover; the port plays WAVs instead | .snd |
| 0x8C   | 4    | ptr    | `vab_header_file`        | VAB header (.vh). Loaded, also unused on PC (WAV replacement) | .vh |
| 0x90   | 4    | ptr    | `vab_sound_file`         | VAB body (.vb). On PC this region is **reused as scratch memory**: `room_set` carves `omodel_slot_count` + `item_count` 0xA4-byte room-object records out of it (`g_omodel_table`, `g_item_model_table`) before the init script runs | .vb |

### Pointer relocation passes (`LoadRoomRdt`, 0x00477d90)

1. **Header pointers 0x48..0x93**: `value += file_base - 3`.
2. **Camera entries**: for each of `cameras_count` cameras, both `mask_pointer`
   and `tim_mask_pointer` get the same treatment.
3. **Omodel pairs**: `omodel_slot_count` pairs at `object_models`; each
   non-null half gets `+= file_base - 3`. The matching slot in
   `g_omodel_table` is zeroed in reverse order.
4. **Item model pairs**: same, count = `item_count`, into
   `g_item_model_table`.
5. **Event script table** (RDT+0x68) is special: its leading dword offsets are
   relative to the *table itself* and are relocated with plain
   `offset += table_base` (no -3), terminated by a zero dword.

The -3 bias means a stored value of 3 points at the start of the file; a null
pointer stays null and is skipped by every pass.

### Cameras (0x94 +)

Variable-length array of camera entries immediately after the 0x94-byte header
(`(RDT_Camera*)((char*)g_RdtPointer + sizeof(RDT))`). The number of entries is
given by `cameras_count` in the header. Each camera is 0x2C (44) bytes:

| Offset | Size  | Type  | Description |
|--------|-------|-------|-------------|
| 0x00   | 4     | int   | Mask data pointer (relocated) — camera sprite/mask block, see below |
| 0x04   | 4     | int   | TIM mask texture pointer (relocated) — the mask ART, embedded (see below) |
| 0x08   | 4     | int   | Camera position X |
| 0x0C   | 4     | int   | Camera position Y |
| 0x10   | 4     | int   | Camera position Z |
| 0x14   | 4     | int   | Camera look-at target X |
| 0x18   | 4     | int   | Camera look-at target Y |
| 0x1C   | 4     | int   | Camera look-at target Z |
| 0x20   | 4     | int   | Camera roll angle |
| 0x24   | 4     | int   | Reserved (always read as part of the view block, never used alone) |
| 0x28   | 4     | int   | Field of view (FOV) |

`Room_SetupCamera` (0x00462970) feeds `fov` to `set_scene_render_param` and the
8-int block at +0x08 to `MatrixToCamera` as a `MATRIX`, so the position/look-at
pairs are really the camera view transform in the engine's matrix layout.

**The mask art is in the file, and `objspr/` is a copy of it.** `+0x04` points
at a whole TIM: 8bpp, a 256-entry CLUT at VRAM (0, 0x1E0), the image block a
tile atlas up to 256x256. The PC build does not read it - `load_room_masks`
(0x00475a90) unpacks `objspr\osp0<stage><room><cam>.pak` instead - but that pak
IS this TIM, LZW-compressed: `OSP01030.pak` and stage 2 room 3 camera 0's
embedded TIM agree to **one byte in 66080**. The PS1 and PC RDTs being the same
size for the same room (302808 bytes for ROOM2030 in both) says the same thing:
nothing was moved out of the file, the PC just added a second copy in the
format its own loader wanted.

That matters for any room the PC release never shipped: the Director's Cut's
arrange rooms have no `objspr` files and need none, because their masks travel
in the RDT like everything else (`room_file_stage() >= STAGE_ARRANGE_FIRST` in
Room.cpp takes that path). Their TIMs are all full 256x256 pages; the base
stages' vary (128x64 up to 256x256) and the PC's paks pad them out.

### Camera switch zones (RDT+0x48)

A flat table of 0x14 (20)-byte records (`CAM_SWITCH_ZONE` in Types.h). The
table is grouped by source camera: every record sharing `camFrom` belongs to one
camera and the first record of each group is that group's header; the following
records are the trigger quads tested against the player. Entering a quad sets
`g_roomCameraId = camTo`. SCD commands rewrite zone entries at runtime:
0x09 / `cmd_cut_lock_set` force a cut, and 0x3A / `cmd_cut_zone_set`
(0x00431e50) writes a zone's `camFrom` (+0x02) and `camTo` (+0x00).

| Offset | Size | Type   | Description |
|--------|------|--------|-------------|
| 0x00   | 2    | short  | `camTo` — camera to cut to on entry |
| 0x02   | 2    | short  | `camFrom` — camera this group belongs to |
| 0x04   | 2    | short  | Quad corner 0, X (XZ plane) |
| 0x06   | 2    | short  | Quad corner 0, Z |
| 0x08.. | ...  |        | Corners 1-3 as `(x, z)` short pairs |

All eight coordinates are **zero-extended from 16 bits** by the original test
(`is_entity_in_switch_zone`), i.e. they behave unsigned.

### Walkable zone table (RDT+0x58)

The NPC navigation grid. Layout:

```
uchar  count                 // zone count
uchar  pad
{ short x1, z1, x2, z2; ushort field_08; ushort flags; }  // × count, 0xC stride
```

* Containment test is half-open: `x ∈ [x1, x2)`, `z ∈ [z1, z2)` via unsigned
  wrap-around compare — `walk_zone_find` (0x00460230) returns the containing
  zone index or 0xFF.
* On a hit, `field_08`/`flags` are parked in `g_playerDisplacement` /
  `player_distance_z` as a side effect (overwritten again downstream).
* Entities remember their current/target zone in Entity+0x174/+0x175;
  `zone_walk_ccw` (0x0045fdb0) / `zone_walk_cw` (0x0045fae0) BFS across shared
  edges through this table, and `walk_zone_shared_edge` computes edge midpoints
  between adjacent zones.

### Item icon tiles (RDT+0x78)

`item_count` × 1200 bytes, one per item slot: a **40×30 8-bit indexed bitmap**,
row-major, no header — the item's inventory icon. `Data/ITEM_ALL.tim` is the
same tiles back to back (0x14 header + 72 × 1200 = 86420 bytes, its file size
exactly), so the room copies are redundant on PC.

Evidence: the block length is `item_count * 1200` in all 320 shipped RDTs;
row-to-row autocorrelation picks width 40 over every other divisor of 1200; and
matching all 582 room blocks against the 72 `ITEM_ALL` tiles gives 325
byte-identical hits and 253 more within 120 differing pixels (99.3%). The
handful of near-misses are icon revisions between the PS1 art and the PC sheet.

**Nothing in the PC binary reads it.** `LoadRoomRdt` relocates the pointer with
the rest of the 0x48..0x94 block and no code ever dereferences it — no
`mov`/`lea` against `[rdt + 0x78]` exists anywhere in the image. The PC engine
draws inventory icons from `ITEM_ALL.tim`; the per-room copies are how the PS1
build shipped them.

### Omodel & item model tables (RDT+0x50 / RDT+0x54)

Both are flat arrays of 8-byte `{ TMD*, TIM* }` pairs (either half may be null).
The omodel pairs are consumed by SCD command 0x1F `cmd_omodel_set`
(`object_models + slot * 8`, `CmdFunctions.cpp:914`) to build the pushable/
climbable room objects; the item pairs by `cmd_item_model_set` (0x18)
(`item_models + index * 8`, `CmdFunctions.cpp:611`) that builds pick-ups and
searchable room models (containers, lids, desks, ...) and fills
`g_RoomActionTable`. Counts come from the header: omodel pair count is
**`omodel_slot_count`**, item pair count is **`item_count`** — each table
feeds its own 0xA4-byte record pool (`g_omodel_table` / `g_item_model_table`).

That RDT+0x54 is the *item* table (the original `nItem` / item-model block) is
confirmed twice over: its consumer is `cmd_item_model_set`, and the icon
section at RDT+0x78 holds exactly `item_count` inventory icons.

### Footstep sound zones (RDT+0x5C, .flr)

```
ushort header[?]            // skipped, never read
{ ushort baseX; ushort baseZ; ushort width; ushort height; ushort soundData; }
```

`LookupFootstepZone` (0x00460480) scans linearly until `(posX,posZ)` falls in
`[baseX, baseX+width) × [baseZ, baseZ+height)` and returns
`(height >> 8) << 8 | (soundData & 0xFF)` — high byte = surface type, low byte
= sound offset for that surface. There is **no terminator**: every shipped room
ends its table with a catch-all zone that always matches, so an entity outside
the room runs the scan off the end of the RDT (the port adds a 256-entry guard).

### Camera mask/sprite block (`cameras[i].mask_pointer`)

Parsed per camera cut by `Room_LoadCameraSprites` (0x004757c0), which fills
`g_RoomSprEntries[128]` (max 128 sprites; the final count is written back into
header byte 0x00):

```
int32  groupCount
// group headers, 8 bytes each:
{ ushort spriteCount; ushort texBits; short originX; short originY; } × groupCount
// sprite records follow the last group header:
ushort w0                    // low byte = tex U, high byte = tex V
ushort w1                    // low byte = screen X delta, high byte = screen Y delta
ushort posData
ushort flags
[ushort width; ushort height]           // present only when (flags & 0xF000) == 0
```

Screen position = `(originX - 0xA0 + xDelta, originY - 0x78 + yDelta)`
(centred on the 320×240 frame). `texBits & 0x3F` selects the texture page;
`flags` bits 0-4 add to the texture bank depth, bits 5-6 pick the blend mode,
bits 7-8 the transparency mode, bit 11 toggles a render flag, and when bit 15
region is non-zero the sprite size comes from `(flags & 0xF1FF) >> 9` (square)
instead of explicit width/height. Sprites are grouped: each group's id
(`grpIdx + 1`) is what SCD visibility commands toggle via
`Room_ApplySpriteFlags` (0x00432220). `tim_mask_pointer` holds the TIM image
used to mask the pre-rendered background (`load_room_masks`,
`load_room_bg_masks`).

### Effect sprite data (RDT+0x7C / 0x80 / 0x84)

`InitRoomEffSprite` loads the animation index + frame-data pair with
`load_effect_sprite_data(effect_anim_index, effect_anim_data, ...)`, then reads
**8 dword relative pointers** out of `effect_anim_sprite` (negative offsets —
the table is walked backwards) into the sheet-pointer table `DAT_00ac9cd0`,
again with the `-3` bias. The sheets themselves are TIM images loaded by
`load_effect_sprites`.

### Sound blocks (RDT+0x88 / 0x8C / 0x90)

PS1 legacy. The `.snd` attribute table and `.vh` header are relocated but never
dereferenced by the PC engine; all audio loads from `.wav` banks instead
(`bgm_load_and_start`, `load_character_sfx`, `Room_LoadEnemySoundBanks` use
their own stage/room name tables). The `.vb` body has a second life as heap
space: `room_set` slices it into 0xA4-byte room-object records — first
`omodel_slot_count` for `g_omodel_table`, then `item_count` more
for `g_item_model_table` — before running the init script, which is why the
room-object system indexes those two tables rather than allocating.

**They are still the room's own answer to "what is in slot N", and the DC's
arrange rooms are the only place that answer exists.** The three blocks together
are a complete per-room sound bank:

| offset | what |
|---|---|
| 0x88 | 48 records × 4 bytes, one per bank slot: `byte[1]` = VAB **program**, `byte[2]` = **tone** within it. An all-zero record is an *unused* slot — indistinguishable from program 0 / tone 0, which is slot 0's real sound |
| 0x8C | the VAB header: magic `pBAV`, then `ps`/`vs`/`vg` counts at +0x12/+0x14/+0x16, 128 × 16-byte `ProgAtr`, `ps` × 16 × 32-byte `VagAtr`, and 256 `u16` VAG sizes (÷8). A `VagAtr` names its VAG index at +0x16 and the note it is keyed at (`min`) at +0x06. A tone index past its program's tone count has an all-zero `VagAtr`: an intentionally **empty** slot |
| 0x90 | the VAB body: the VAGs packed in index order, so VAG *n* starts at `sum(sizes[1..n-1]) * 8` |

A slot therefore resolves to a byte range of the body, and **(VAG bytes, note)**
— not the bytes alone — is the identity of a sound: the PS1 plays one VAG at
several notes and the PC pre-rendered each pitch as its own `.wav`, which is why
`cer_taoA`/`cer_taoB` and `ft_wdA`/`ft_wdB` are one sample each. Hashing that
pair and looking it up in a base room, whose `g_RoomSndData` row names the slot,
names any arrange room's bank without guessing.
`tools/gen_dc_arrange_sound_rows.py` does exactly this; see `DC_PORT.md` §5a.

## Collision Boundary Data (.blk)

The `boundaries` pointer (RDT+0x4C) points at the room's collision geometry:
a 0x18-byte header followed by a flat array of 0x0C-byte records.

### Header (0x00 - 0x17)

| Offset | Size | Type   | Description |
|--------|------|--------|-------------|
| 0x00   | 2    | short  | cellX — X of the quadrant split point |
| 0x02   | 2    | short  | cellZ — Z of the quadrant split point |
| 0x04   | 4    | int    | Record count for quadrant 0 |
| 0x08   | 4    | int    | Record count for quadrant 1 |
| 0x0C   | 4    | int    | Record count for quadrant 2 |
| 0x10   | 4    | int    | Record count for quadrant 3 |
| 0x14   | 4    | int    | Unused (read and discarded) |

`Room_SetupCollisionCallbacks` (0x0047d140) rewrites 0x04-0x14 **in place** into
five absolute pointers, so after room load quadrant `q` spans
`[group[q], group[q+1])`. The rewrite is not idempotent — it runs exactly once
per `LoadRoomRdt`.

### Record (0x0C bytes)

| Offset | Size | Type   | Description |
|--------|------|--------|-------------|
| 0x00   | 2    | short  | xMax |
| 0x02   | 2    | short  | zMax |
| 0x04   | 2    | short  | xMin |
| 0x06   | 2    | short  | zMin |
| 0x08   | 2    | ushort | Shape / step: low byte = shape index, bits 8-14 = floor step magnitude, bit 15 = step is downward |
| 0x0A   | 2    | ushort | Flags: low byte = step fine value, bit 8 = blocks movement, bit 9 = participates in the "still stuck" re-test |

The **MAX corner comes first**. This is not the `(x, z, w, d)` layout the
RE2-era notes on this format describe: `boundary_point_outside` (0x0047d2a0)
accepts a point only when `field[2] - r <= px <= field[0] + r`, which is an
empty interval for any box wider than `2r` under the width reading. Rasterising
the shipped rooms with the max-first reading reproduces their real floorplans.

Shape index selects a handler from `g_CollisionShapeHandlers` (0x00ac9c00).
Only four values occur across all 320 shipped RDTs:

| Shape | Handler | Address | Behaviour |
|-------|---------|---------|-----------|
| 1 | `collision_push_rect` | 0x0047df10 | Push out of a rectangle |
| 3 | `collision_push_circle` | 0x0047e0e0 | Push out of a circle (radius from the X extent) |
| 4 | `collision_flag_set` | 0x0047e1b0 | Soft zone — raises `collisionFlags` bit 3, no push |
| 5 | `collision_push_rect` | 0x0047df10 | As 1, but skipped while `collisionFlags` bit 4 is set |

### Quadrants

`ChkOutsideCell` (0x0047d270) returns a 2-bit quadrant index for a position:
bit 0 = the point is on the low side of `cellX`, bit 1 = low side of `cellZ`.
Records whose box straddles a split are duplicated into every quadrant they
touch, so testing one quadrant's list is sufficient for any single point.

### Resolution

`check_room_collision` (0x0047d310) runs two passes over the quadrant list.
Pass 1 tests the incoming position and lets each hit record's handler push the
entity out along the axis whose push opposes this frame's movement. Pass 2
re-tests the pushed position; if it is still inside a blocking record the whole
frame's movement is rolled back to `Entity+0x6C` (the last safe position),
otherwise `Entity+0x6C` advances to the resolved position.

Return values: 0 = clear, 1 = pushed out and now clear, 2 = still stuck
(position reverted), 3 = a floor/step zone was crossed, with its step value left
in the scratch dword at 0x00be0dfc. The distance actually travelled is left at
0x00be0df8.

**`boundary_classify` (0x0047d1b0) tests a global, not its argument.** The point
it classifies is always `g_playerPosScratch` (0x00be11b0); the `SVECTOR*` first
parameter is only an offset added to it. Every caller has to publish the point
it means into that global first. `check_room_collision` does so at the top of
each pass, and `check_room_collision_two_point` does so once per endpoint (0x0047d80a) and again
before its re-walk (0x0047d8ec) — as a plain 16-byte VECTOR copy that Ghidra
renders as five stores through short halves (`player_pos.x._0_2_ = centre->x`
and friends), which reads like scratch bookkeeping and is easy to mistake for
dead code. Drop it and the routine silently classifies whatever position the
previous caller left behind.

### Two-point body test (0x0047d6f0)

`check_room_collision_two_point(endA, endB)` resolves a body that is too long for the single-point
`check_room_collision`: a prone zombie, or a pushed room object's footprint. The
two `SVECTOR` endpoints are in body-local space and get rotated twice — by the
entity yaw (+0x74) to produce the test centres, and by the mirror angle (+0x7E)
to produce the values the shape handlers write back into.

End B is walked first (the original's loop counter starts at 1). If end A came
out clear the push stands and the position/angle backups advance; otherwise end
B's records are re-walked and, if anything still blocks, position **and** angle
roll back together from +0x6C/+0x70 and +0x7E.

Returns 0 = clear, 1 = pushed clear, 0x80 = still stuck (rolled back). Note that
when end A is clear it returns end B's bits, so a hit on either end is enough to
report blocked — the test is not direction-aware.

Quirk of the original, reproduced: the re-walk uses the **previous** quadrant
(`group[cellB - 1] .. group[cellB]`), not end B's own quadrant, because the
saved slot holds `&group[cellB]` and the loop reads through `[-1]`.

### Room 3D-object collision, pushing and climbing

Room 3D objects do NOT go through `check_room_collision` — that function only
ever walks the boundary records. Objects have their own resolver,
`update_room_objects` (0x00474090), called once per frame from `game_loop`
between `DrawFadeSpr` and the camera/lighting update.

#### The `omodel_set` instruction (SCD 0x1F, 0x1C bytes)

`cmd_omodel_set` (0x00461ac0) unpacks it into a 0xA4-byte block:

| Operand | Size | Goes to | Meaning |
| ------- | ---- | ------- | ------- |
| +0x00 | 1 | — | opcode 0x1F |
| +0x01 | 1 | record +0x01 (masked 0x7F) | model index; bit 0x80 queues the texture, bits 0-5 also index the RDT model table |
| +0x02 | 1 | record +0x00 | flag byte |
| +0x03 | 1 | record +0x64 | SCA parent: 0xFE = player, 0xFF = none, <0x80 = another object, else enemy `& 0x7F` |
| +0x04 | 2 | record +0x34 / +0x6C | position X |
| +0x06 | 2 | record +0x38 / +0x6E | position Y |
| +0x08 | 2 | record +0x3C / +0x70 | position Z |
| +0x0A | 2 | record +0x74 and +0x7E | yaw |
| +0x0C | 2 | record +0x94 | floor probe end A, X |
| +0x0E | 2 | record +0x98 | floor probe end A, Z |
| +0x10 | 2 | record +0x9C | floor probe end B, X |
| +0x12 | 2 | record +0xA0 | floor probe end B, Z |
| +0x14 | 2 | record +0x92 | body radius (`Sca_info` +10) |
| +0x16 | 2 | record +0x8C **and** +0x90 | half-extent Y |
| +0x18 | 2 | record +0x8A | half-extent X |
| +0x1A | 2 | record +0x8E | half-extent Z |

The endpoints' Y components (+0x96, +0x9E) are never written — not by the port
and not by the original — so they hold whatever was in the buffer. Harmless: a
Y rotation cannot mix Y into X or Z, and the boundary test is 2D.

`tools/parse_init_scd.py` decodes 0x1F one byte late (it reads position from
+5/+7/+9); use the table above, not that script's output.

#### The runtime record

The block lives in `g_omodel_table[0 .. RDT.omodel_slot_count)` and is
laid out like the head of an `Entity`, so the shared helpers can take one on
either side of a collision test:

| Offset | Meaning |
| ------ | ------- |
| +0x00  | flag byte — see below |
| +0x01  | model byte; bit 0x40 selects the heavy grunt SFX (0x17 vs 0x16) |
| +0x04  | `Sca_info` — points at the record's own +0x88 |
| +0x34/+0x38/+0x3C | live position X/Y/Z (`localMatrix.t`, 32-bit) |
| +0x64  | SCA parent matrix pointer |
| +0x6C/+0x6E/+0x70 | last committed position X/Y/Z (16-bit) |
| +0x74  | yaw; +0x7E is its rollback copy |
| +0x86  | push hold counter (16-bit; +0x86/+0x87, an Entity's behavior/state) |
| +0x88  | 0x8000 — the negative terminator of the size list |
| +0x8A / +0x8C / +0x8E | half-extents X / Y / Z (`Sca_info` +2/+4/+6) |
| +0x90 / +0x92 | the entity-side extents (`Sca_info` +8/+10) |
| +0x94, +0x9C | the two floor-probe endpoints fed to `check_room_collision_two_point` |

Flag byte bits: 0x01 = active, 0x02 = intangible, 0x04 = skip the floor probe,
0x08 = no collision at all, 0x20 = not pushable, 0x40 = climbable.

Only bit 0 is cleared at room load; the rest of the block is uninitialised until
an `omodel_set` fills it, which is why every pass gates on it.

#### Per frame

For every active record:

1. every enemy is resolved out of it with `ChkEntitySlide(enemy, obj, 0)`;
2. `ChkEntitySlide(player, obj, 1)` runs in "move the object" mode purely as a
   test — if it reports an overlap **and** the player is holding forward
   (D-pad bit 0) **and** `ChkPlReachEntity` puts the object inside the 470-unit
   reach probe, the record's +0x86 counter increments, otherwise it is reset;
3. at counter == 9 (nine straight frames of pushing into it) the push starts:
   the player's facing snaps to the nearest cardinal, `DAT_00ae9ee8` remembers
   the record, and the push is cancelled again if the object's own floor probe
   (`check_room_collision_two_point` on +0x94/+0x9C, skipped when flag bit 0x04 is set) hits a
   wall, if an enemy is in the way, or if another object overlaps
   (`ChkObjSlide`), each of which parks the counter at 10;
4. `ChkEntitySlide(player, obj, 0)` resolves the player out of the object —
   this, and only this, is what makes room objects solid;
5. if the object moved, it shoves the other objects with `ChkObjSlide` and
   commits the new position to +0x6C/+0x70;
6. `update_player_position(obj, 4)` fires the object's own event-zone probes.

Step 2 moves the object as a side effect, and step 3 normally puts it straight
back. The **one** path that keeps the displacement is the frame where the push
animation is already running (`action_behavior` already 0x10) — that is what
actually slides the object across the floor. A successful start parks the
counter at 8, not 0, so it climbs back to 9 on every subsequent frame and the
whole block re-runs for the duration of the push.

After the loop, a started push raises `g_main_state_flags` bit 0x40, and that
bit is the sole trigger for `action_behavior` 0x10 (`behavior_10_push`,
0x00457230). Releasing forward clears the bit, which is how the push animation
knows to end. The grunt SFX it plays on animation frame 1 comes from bit 0x40 of
byte +0x01 of the pushed record (0x17 heavy / 0x16 light).

#### The box tests

Three routines share one idiom: containment is checked as an unsigned wrap,
`(unsigned)(delta + ext) <= (unsigned)(2 * ext)`, which rejects both sides of
the interval in a single compare because a delta below `-ext` wraps to a huge
unsigned value.

| Routine | Address | Test |
| ------- | ------- | ---- |
| `ChkPlReachEntity` | 0x00474a20 | Is the player's 470-unit forward probe inside the record's +0x8A/+0x8E box? Side effect: zeroes the shallower axis of `g_svecScratch`. The delta is `probe - centre`, i.e. the object position is **subtracted out of** the extent (0x00474a8c / 0x00474a8f) — reversing it makes the sum ≈ `probe + centre` and the test fails for every object not at the world origin. |
| `ChkEntitySlide` | 0x00474330 | Entity vs object on all three axes, then resolve along whichever of X/Z is cheaper to escape (compared via the cross products `extX*dz` and `extZ*dx`, so no divide). Mode 0 moves the entity, mode 1 moves the object and returns the number of axes moved. |
| `ChkObjSlide` | 0x00474500 | Object vs object, X/Z only, both extents from `Sca_info`. Places the pushed object one unit clear of touching so the next frame does not re-trigger. |

`ChkEntitySlide` walks the entity's part list: offsets advance 6 bytes per part
(from +0x08) while the size list advances 0xC (from +0x04), terminating on a
negative first short — tested *after* the part is processed, so a list is always
walked at least once. The extents themselves are re-read from the **first** size
record every iteration (0x004743b6 reloads `[ECX+4]`, not the advanced pointer),
so only the part offsets really vary. Object records set +0x88 to 0x8000, giving
them exactly one part.

#### Climbing

Climbing is a separate path and does not involve `update_room_objects`.
`check_climb_object` (0x00474930) scans the same table on the action-button
press, from `g_omodelCount - 1` down, for the first record with flag bit 0x40,
inside `ChkPlReachEntity`, and facing within 299/4096 (~26°) of the player on
either wrap-around side. It raises `g_main_state_flags` bit 0x80, stores the
record at 0x00ae9ef0, and picks `attackDirection` from the facing.

Note the two scratch globals sit next to each other and are easy to confuse:
0x00ae9ef0 is the **climb** candidate, 0x00ae9ee8 is the object
`update_room_objects` is **pushing**. `behavior_10_push` reads the latter.

`player_input_to_behavior` turns that into `action_behavior` 0x0A — the same
handler as a door (`FUN_00457390`) — where bit 0x80 selects the vault
displacement and SFX (0x23) instead of the door ones (0x2D). While the bit is
raised, calling `check_climb_object` again verifies the player has not turned
away: drifting cancels, staying clears the bit and raises `unk_03` bit 0x10,
which mirrors every Z displacement (climbing down rather than up).

#### Reference room

Room 1070 has both cases:

| Slot | Flags | Object | Half-extents X/Z | Radius |
| ---- | ----- | ------ | ---------------- | ------ |
| 0 | 0x41 | step ladder — climbable **and** pushable | 990 / 800 | 725 |
| 1 | 0x01 | shelf — pushable only | 1050 / 1350 | 540 |

Neither sets bit 0x20, so both push; only slot 0 sets bit 0x40, so only it
climbs.

## Message Data Format (.msg)

The `messages` pointer (RDT+0x74) points to a message table with the format:

```
uint16  offset[msg_count]   // Offset table: each entry is a byte offset
                             // from the start of the messages block
char    message_data[]       // Null-terminated message strings
```

To resolve message `i`: `msgBase + offset[i]`

Global messages (msg_id bit 6 set) use the `global_messages[]` table at
0x004bfc58 instead of the RDT-local messages.

## SCD Script Format (.scd)

The SCD (Script) data contains bytecode opcodes that control room behavior.
All three blocks are relocated by `LoadRoomRdt` and their base pointers stashed
in `g_RoomInitScd` / `g_RoomScdOpcodes` / `g_RoomEventScripts`:

- **initialization_scd** (RDT+0x60): Runs once when the room is loaded
  (`run_command_functions(g_RoomInitScd)` from `room_set`)
- **scd_opcodes** (RDT+0x64): Runs every frame (main loop)
- **scd_opcodes2** (RDT+0x68): Event script table. Its leading dwords are
  offsets relative to the block start, terminated by a zero dword —
  `LoadRoomRdt` rewrites each one in place into an absolute pointer
  (`offset += table_base`). Each entry is an independently scheduled event
  script executed through the 8-slot `ScdEventEntry` context table at
  0x00bf084c (`room_events_check`).

Script opcodes are processed by `run_command_functions()` during `game_loop()`.

## Related Functions

| Function | Address | Description |
|----------|---------|-------------|
| `LoadRoomRdt` | 0x00477d90 | Loads .rdt file and relocates pointers |
| `init_room` | 0x00409990 | Initializes room: stage tables, animations, room_set |
| `room_set` | 0x00477720 | Sets up room collision, cameras, items, obstacles |
| `Room_SetupCollisionCallbacks` | 0x0047d140 | Relocates boundary counts to pointers, installs shape handlers |
| `ChkOutsideCell` | 0x0047d270 | Boundary quadrant index for a position |
| `boundary_classify` | 0x0047d1b0 | Shape index if `g_playerPosScratch` is inside a record, else 0xFFFF |
| `check_room_collision` | 0x0047d310 | Resolves an entity against the room boundaries |
| `check_room_collision_two_point` | 0x0047d6f0 | Two-point body test (prone entities, pushed objects) |
| `cmd_omodel_set` | 0x00461ac0 | SCD 0x1F — builds a room 3D-object record |
| `update_room_objects` | 0x00474090 | Per-frame object collision + push driver |
| `ChkEntitySlide` | 0x00474330 | Entity vs object box resolve |
| `ChkObjSlide` | 0x00474500 | Object vs object box resolve |
| `ChkPlReachEntity` | 0x00474a20 | Player's 470-unit forward probe vs an object box |
| `check_climb_object` | 0x00474930 | Action-key climb candidate scan |
| `behavior_10_push` | 0x00457230 | The push animation (`action_behavior` 0x10) |
| `set_message_display` | 0x00455670 | Resolves message from RDT or global table |
| `Room_LoadCameraSprites` | 0x004757c0 | Parses the camera mask/sprite block into `g_RoomSprEntries` |
| `Room_SetupCamera` | 0x00462970 | Applies camera FOV + view matrix |
| `LookupFootstepZone` | 0x00460480 | Footstep-zone lookup for a position |
| `walk_zone_find` | 0x00460230 | Walkable-zone lookup (RDT+0x58 grid) |
| `cmd_cut_zone_set` | 0x00431e50 | SCD 0x3A — writes a camera switch zone's camFrom/camTo |
| `run_command_functions` | — | Processes SCD script opcodes |

## Memory Layout

The RDT is loaded into `g_loadDataDestPointer` (typically `g_DataBuffer`).
After pointer relocation, `g_RdtPointer` points to the loaded RDT header.

Key globals:
- `g_RdtPointer` (0x00bebcd0): Pointer to current room's RDT data
- `g_stageId` (0x00be9820): Current stage ID
- `g_roomId` (0x00be9821): Current room ID within the stage
- `g_roomCameraId` (0x00be9822): Current active camera index