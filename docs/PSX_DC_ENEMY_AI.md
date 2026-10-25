# Director's Cut enemy AI — zombie state machines

Where the enemy state machines live on PS1, what the Director's Cut
(`SLUS_005.51`) adds to the zombie, and the new enemy models. Companion to
`docs/ZOMBIE_STATE_MACHINE.md` (the PC zombie) and
`docs/PSX_DC_STAGE_OVERLAYS.md`.

> Addresses are MIPS virtual. Unless noted, examples are the `STAGE1` overlay
> (load `0x80105400`).

## Where the enemy AI lives

On PS1 the monster code is **not** in the resident executable — it is linked
into each stage overlay. Proof: the zombie health table
(`{59,59,79,59,59,39,59,79,79,99,59,79,59,59,79,59}`) is present in
`STAGE1`, `STAGE2`, `STAGE4`, `STAGE5`, `STAGE6`, `STAGE7` in **both** builds
(`STAGE3` — courtyard/underground — has no zombie table at all). So every
zombie change below appears once per stage overlay, at stage-shifted addresses.

## The zombie dispatch (identical structure to the PC)

`zombie_update` tail-jumps through `zombie_states_table`, indexed by
`entity->state` at **`+0x84`**:

```asm
; DC STAGE1 0x80106054
lui  v0,0x800d
lw   v0,-0x54ec(v0)    ; v0 = g_CurrentEntity  (0x800cab14)
lbu  v1,0x84(v0)       ; entity->state
sll  v1,v0,2
lui  a0,0x8012
addiu a0,a0,0x128c     ; &zombie_states_table
addu v0,v1,a0
lw   s0,(v0)
jalr s0
```

| table | STAGE1 DC | STAGE1 OG |
|---|---|---|
| `zombie_states_table` (by `state`, `+0x84`) | `0x8012128c` | `0x8012229c` |
| behaviour view (**`states + 9`**, by `behavior_flags & 0xF`) | `0x801212b0` | `0x801222c0` |
| `zombie_action_tbl` (by `action_behavior`, `+0x86`) | `0x801212ec` | `0x801222f8` |
| dispatch code (states) | `0x80106074` | `0x80105f3c` |
| dispatch code (action_behavior) | `0x80107280` | `0x80107240` |

> **Corrected 2026-09-14.** Two entries above were wrong in the first pass and
> both mattered. (1) The behaviour view is `states + 9`, not `states + 10`:
> every `lui/addiu` in either overlay that lands inside the table block resolves
> to `states+0`, `states+9`, `states+20/21` (the damage bytes) and `states+23/24`
> — there is no reference to `states+10`. (2) `0x801212ec` is **not** the
> entity-id dispatch: the instruction that loads it is preceded by
> `lbu v1,0x86(v0)`, i.e. `action_behavior`, so it is the zombie's own action
> table. The entity-id dispatch is elsewhere and is not needed for this port.

The health table sits just before `zombie_states_table`
(DC `0x80121250`, OG `0x80122260`), so the tables can be located in any stage
overlay by searching for those 16 bytes; the zombie code follows at the same
addressing pattern. The entity-id table has the PC's 0-21 monster layout
(ids 0/1 = zombie, 2 = cerberus, …), so the DC does **not** add a new enemy id
for its zombies.

## DC addition 1 — a new zombie behaviour (11)

The DC `zombie_states_table` has **one more entry than the OG**: a pointer is
inserted at DC index 20, and everything after it shifts one word (the
`02 00 05 00` damage bytes that sit at OG index 20 are at DC index 21). Since
the behaviour view is `states + 9`, that new entry is **behaviour 11**, not
behaviour 10 — the behaviours the two builds share keep their indices, and the
handler is reached through `zombie_behavior_tbl[behavior_flags & 0xF] == 11`.

Table lengths, byte-verified against the three builds:

| build | states entries | behaviour base | behaviour 11 |
|---|---|---|---|
| PS1 OG `STAGE1` | 20 (`states[0..19]`) | `states + 9` | past the end (damage bytes) |
| PS1 DC `STAGE1` | 21 | `states + 9` | `states[20]` = `0x8010cdf8` |
| PC `ResidentEvil.exe` | 22 | `states + 10` (`0x004bb2f0`) | `states[21]` = **NULL** |

The PC build's table is the OG's with a NULL inserted at index 9 and a second
NULL appended at index 21; that first NULL is why its behaviour base is one
entry further along. Entry for entry, `PC behavior[k] == OG behavior[k]` for
k = 0..10, so the port's table and its `&zombie_states_table[10]` view are
correct as they stand — and the port's spare `states[21]` is exactly the slot
the DC fills.

The handler is a small 2-state machine on `entity->action_state` (`+0x87`):

```c
// DC STAGE1 0x8010cdf8   (new; 139 words)
if (entity->action_state == 0) {
    entity->action_state  = 1;
    entity[0xbe] = entity[0xbf] = 0;          // animation_frame_id, timing_control
    entity[0x8c] = 3;                         // blend_counter
    entity[0xbd] = 0x1D;                      // animationId 29
    entity->action_ticks (0xc4) = (DAT_800c867a & 0xF) + 0x2D;   // set, never read
} else if (entity->action_state != 1) {
    return;
}
if (entity[0xbe] == 0x14) Snd_em(4);          // sound on animation frame 20
if (Joint_move(0, entity[0x90], entity[0x94], 0x400)) {   // animation finished
    entity->move_speed_current (0xc2) = 0x1E7;   // 487
    Add_speedXZ(0);
    entity->move_speed_current (0xc2) = 0x3DF;   // 991
    Add_speedXZ(0x400);
    entity->angle (0x74) += 0x400;               // +90 degrees
    entity->behavior_flags (0x02) = 0;           // -> plain chasing zombie
    entity[0x84] = 1;  // state = state_check
    entity[0x85] = 1;  // ignore_player_flag
    entity[0x86] = 3;  // action_behavior
    entity[0x87] = 0;  // action_state
    entity[0x04] = &sca_info[0];                 // back to the normal hit box
    entity[0x00] &= 0xFB;                        // collision back ON
}
```

Read with the two `zombie_init` additions below, behaviour 11 is a **scripted
entrance**: the zombie spawns with collision disabled (`status_flags` bit 2 —
`check_room_collision` returns 0 on it) and a third, much larger `ScaInfo`
record; it plays animation 29 with a sound at frame 20; when the animation
finishes it lunges forward hard (487, then 991 at a 90° offset), turns 90°,
restores the normal hit box and collision, and hands itself over to the ordinary
chase behaviour.

`zombie_init` gains two blocks for it, both absent from the OG:

```c
if ((entity->behavior_flags & 0xF) == 0xB) {   // before the naked-zombie check,
    entity[0x00] |= 4;                         // which can overwrite Sca_info
    entity[0x04] = &sca_info[2];               // the DC's THIRD ScaInfo record
}
```

and `zombie_anim_id_tbl[11]`, which is `0` in the OG and the PC build, is
**29** in the DC — the same animation the handler sets.

### DC addition 1b — `zombie_update` skips entity collision for behaviour 11

`zombie_init`'s bit 2 only turns off the **room** collision
(`check_room_collision` / `check_room_collision_two_point` return 0 on it). The
**entity-vs-entity** push in `ResolveEntityScaCollision` tests **bit 1**
instead, which nothing about behaviour 11 sets — so the DC edits the call site
too. In `zombie_update`, right after the `state != 5` test:

```c
// DC only: the OG/PC go straight from the state!=5 test into the trio.
if ((entity->behavior_flags & 0xF) == 0xB)
    goto skip_entity_collision;     // -> the collisionFlags clear

SetEntityScaHitData(entity);
ResolveEntityScaCollision(&player_entity, entity);
HandleEnemyPlayerCollisions();
skip_entity_collision:
entity->collisionFlags &= ~0x08;
```

DC `STAGE1` `0x8010615c` is `lbu 2($v0)` / `andi 0xf` / `addiu 0xb` / `beq`
branching to `0x801061c0` (the `andi $a0, 0xf7` on `+0xDC`); the OG's
`state != 5` branch (`0x80106014`) falls straight into `0x8010601c`, the first
`sll`/`jalr` of the trio. So behaviour 11's zombie neither pushes nor is pushed
while it lies there — the second half matters for the Forest zombie, whose body
is the `id 0x16` prone entity on the 2F balcony, walked past by the player
before the script releases it (see the Forest-zombie section below). Once
`dc_standup_lunge` clears `behavior_flags` the zombie collides normally again.

This is a call-site edit, not a `status_flags` bit: a port that only sets bit 2
leaves the prone body shovable, which is exactly the defect reported against
the port on 2026-09-15.

The third `ScaInfo` record is DC-only. All three, 12 bytes each on PS1
(`0x80121220` / `0x8012122c` / `0x80121238`, pointed at by `0x80121244`):

| record | +0x00 | +0x02 | +0x04 | +0x06 | +0x08 | +0x0A (radius) |
|---|---|---|---|---|---|---|
| 0 standard | `0x8001` | `0` | `0xfa06` | `0` | `0x05fa` | `0x01a6` = 422 |
| 1 naked | `0x8001` | `0` | `0xfa06` | `0` | `0x05fa` | `0x0142` = 322 |
| 2 **DC only** | `0x8001` | `0x03e8` | `0xfa06` | `0` | `0x05fa` | `0x03e8` = **1000** |

The OG's pointer table has only the first two (it sits at `0x80122258`, two
entries; `0x80122254` is record data, not a third pointer).

`entity + 0xC2` is `move_speed_current` (the PC port documents it as a signed
word; the normal zombie walks at 45). 487 and 991 are roughly 11× and 22× that,
i.e. a **burst of fast movement** — this is the "fast walking zombie"
behaviour. It only runs once the handler is entered (new state 20), which the
new spawn markers below gate.

## DC addition 2 — new spawn behaviour markers

Inserted near the top of every stage overlay (DC STAGE1 `0x80105b4c`, ~89
words) is a block that recognises **new `behavior_flags` low-nibble values
`0xC`, `0xD`, `0xE`** on `g_CurrentEntity` and rewrites them into a normal
behaviour plus a marker:

```c
entity[0x16b] = 0;
switch (entity->behavior_flags (0x02) & 0xF) {
case 0xC: entity[0x16b] = 1; entity->behavior_flags &= 0xF0;            break; // -> behaviour 0
case 0xD: entity[0x16b] = 1; entity->behavior_flags &= 0xF1;            break; // -> behaviour 1
case 0xE: entity[0x16b] = 1; entity->behavior_flags &= 0xF0;
          entity[0x88] += 0x64;                                         break; // -> behaviour 0 + boost
}
```

(`entity + 0x88` is `health`, an s16 — so `0xE` is the **+100 HP** tough variant.)

The OG has no `0xC/0xD/0xE` cases (`behavior_flags & 0xF` never exceeded `0xA`).
The block sits between the `move_speed = 45` and `turn_speed = 24` stores in
`zombie_init`; the three `if`s are sequential, not a `switch`, but each masks its
own nibble away so they stay disjoint.

**What the marker does** (this is the whole "fast zombie"): `entity[0x16b]` is
read in exactly three places, all inside `zombie_chase_walk` (DC STAGE1
`0x801090b8`, the PC's `0x00434eb0`), and each one simply **repeats the call
above it**:

```c
case 3:  Joint_move(0, animHeader, animBase, 0x400);
         if (entity[0x16b]) Joint_move(0, animHeader, animBase, 0x400);   // DC
         entity->move_speed_current = 45;
         if (entity->move_speed > 45)                                     // DC
             entity->move_speed_current = entity->move_speed;
...
case 4:  Joint_move(0, animHeader, animBase, 0x400);
         if (entity[0x16b]) Joint_move(0, animHeader, animBase, 0x400);   // DC
...
         Add_speedXZ(0);
         if (entity[0x16b]) Add_speedXZ(0);                               // DC
```

So a marked zombie advances its walk animation twice and applies its XZ motion
twice per frame — **double speed**, with no new speed constant anywhere. (The
`max(45, move_speed)` line is a DC addition too, but inert: `zombie_init` sets
`move_speed = 45` and nothing raises it.)

## Which behaviour nibbles the shipped RDTs actually use

Walking every RDT's `init_scd` and `scd_opcodes` streams in both builds and
collecting `enemy_set` (opcode `0x1B`) records — id in body[0], `behavior_flags`
in body[1] — stopping each walk at its first desync:

| id | nibble | OG | DC |
|---|---|---|---|
| `0x00` zombie | `0xC` | — | STAGE1/ROOM1030, ROOM1031 |
| `0x00` zombie | `0xD` | — | STAGE1/ROOM1110, STAGE6/ROOM6110 |
| `0x00` zombie | `0xE` | — | **never used** |
| `0x02` cerberus | `0xC` | STAGE3/ROOM3040, 3041 | STAGE3/ROOM3040, 3041 |
| `0x11` zombie (green coat) | `0xC` | — | STAGE1/ROOM10A0 |
| `0x11` zombie (green coat) | `0xE` | — | STAGE7/ROOM71C0, 71C1 |
| `0x16` | `0xB` | — | STAGE9/ROOM9120, 9121, STAGEE/ROOME120, E121 |

Two things follow. The cerberus row is in **both** builds, so the nibble rewrite
must be gated on the zombie ids and not applied to every entity — a cerberus
spawned with nibble `0xC` means something else entirely. And the only user of
behaviour `0xB` is id `0x16` in the four arrange balcony rooms, which is the
Forest zombie: the scripted entrance above is his.

## Entity id → model → file (resolved)

`LoadEntityEMD` picks the `.EMD` for an entity. Room entities are passed
`entity_id + 4` — the `+4` sits in the caller's branch-delay slot
(`room_set` `0x800446C4`/`0x800446C8`, `addiu $a1,$a1,4`), which executes before
the call; the player is passed its raw id from `LoadEntityModel`. The argument
indexes a per-scenario `u16` table of **file-system indices**:

| | OG | DC |
|---|---|---|
| `LoadEntityEMD` | `0x8002423c` | `0x800238c8` |
| table | `0x8008ccf4` (52/block, stride `0x68`) | `0x8008d03c` (**69**/block, stride `0x8a`) |

Entry 0-3 = `CHAR10`-`CHAR13` (4 player models), entry 4 = `EM1000` (zombie),
entry 26 = `EM1016`, …. The value is resolved through `g_FileSystemTable`, which
is built from the ISO directory in **alphabetical path order** — verified exactly
against the documented OG indices: `0 = DATA\BIO.TIM`, `1 = BIO_CARD.DAT`,
`2 = DATA\BT367.TIM`, `330 (0x14A) = PROG2\LOGO.EXE`, then `32 = EM1000`,
`55 = EM1020`, `69 = EM102E`, `70 = EM1030`. The DC tree has one fewer file
before the ENEMY set (no `STAFF2.STF`), so indices run one low: `299 (0x12B) =
PROG2\LOGO.EXE`, `301 = SELECT.EXE`, `309 = TITLE.EXE` — matching the TITLE
overlay's scheduled indices.

Resolved DC entries (`EMD file index`, Chris block / Jill block):

| model idx | entity id | DC value (≈ file) | filename |
|---|---|---|---|
| 4 | 0 | 32 / 79 | `EM1000.EMD` / `EM1100.EMD` (zombies) |
| 5 | 1 | 33 / 80 | `EM1001.EMD` / `EM1101.EMD` |
| 26 | `0x16` (22) | 54 / 101 | **`EM1016.EMD` / `EM1116.EMD`** |
| 51 (`0x33`) | costume variant | 70 / 71 | `EM1030.EMD` / `EM1031.EMD` |
| 52 (`0x34`) | player (ADVANCED) | 72 / 73 | `EM1032.EMD` / `EM1033.EMD` |
| 53 (`0x35`) | player id 3 | 74 / 74 | `EM1035.EMD` / `EM1035.EMD` |
| 56 (`0x38`) | NPC 32 | 75 | `EM1040.EMD` |
| 57 (`0x39`) | NPC 33 | 76 | `EM1041.EMD` |
| 59 (`0x3B`) | NPC 35 | 77 | `EM1043.EMD` |
| 68 (`0x44`) | NPC 44 | 78 | `EM104C.EMD` |

### The ADVANCED remap (corrected)

The earlier note that "ADVANCED zombies load model 0x34" was wrong: because room
entities already carry `+4`, the `< 2` / `== 3` arms are the **player** path
(raw id from `LoadEntityModel`), and the `+0x14` arms are **NPCs**. Decompiled
(and now resolved):

```c
if (g_status_flags & 0x20000) {                 // ADVANCED
    if (param_2 < 2)        param_2 = 0x34;     // player -> EM1032 / EM1033
    if (param_2 == 3)       param_2 = 0x35;     // player model 3 -> EM1035
    if ((param_2 - 0x24) < 2 || param_2 == 0x27 || param_2 == 0x30)
        param_2 += 0x14;                        // Chris NPC -> EM1040, Jill -> EM1041,
                                                // Rebecca -> EM1043, id 44 -> EM104C
    if (param_2 == 0x33 && Flg_ck(g_gameOptionsFlags, 0x6f))
        param_2 = g_CharacterId & 1;            // NPC 47 -> player model
}
```

So ADVANCED changes the **player and cutscene-NPC** models (the new outfits),
not the monsters. Zombies keep `EM1000`/`EM1001`.

## Forest zombie — found, model resolved

`STAGE9/ROOM9121` (arrange 2F, Forest's body room) and its Jill variant
`STAGEE/ROOME121` spawn, next to Forest's corpse (`id 0x26`):

```
enemy_set  id=0x26 beh=0x00   ; Forest's corpse (EM1026 / EM1126)
enemy_set  id=0x22 beh=0x04
enemy_set  id=0x16 beh=0x8B   ; 0x80 = SCD-controlled -> FOREST ZOMBIE
enemy_set  id=0x16 beh=0x00   ; the same model, inert
```

The original `STAGE2/ROOM2121` has only the corpse (`0x26`) and `id 0x22`, so the
two `id 0x16` records are **arrange-only**. Model index `0x16 + 4 = 26` →
`EM1016.EMD` (Chris scenario) / `EM1116.EMD` (Jill), the two new zombie models —
**the Forest zombie**. Id `0x16` is not in the ADVANCED remap, so it always uses
those files.

**Corrected 2026-09-15.** This section used to end "driven by the shared
human-NPC update (id 22) ... rather than the monster zombie AI". That is wrong,
and it contradicted behaviour `0xB` being a *zombie* behaviour two sections up.
The per-stage entity tables settle it: slot `0x16` is registered **only** by DC
STAGE2 (`0x80120a00` → `0x80106040`) and DC STAGE7 (`0x8012aa0c` →
`0x801060d4`), and in both the value is the same handler their slots 0/1/0x11
carry — the zombie update. No OG overlay registers the slot at all. Those two
overlays are the ones the arrange balcony rooms run under (STAGE9 folds onto
stage 1, STAGEE onto stage 6), i.e. exactly where the `id 0x16` records live.

The DC also **rewrites the entity's id to 0** once the model is loaded, in
ADVANCED only: `LoadEntityEMD`'s post-load block ends
`if (param_2 == 0x1a) *(byte *)(g_CurrentEntity + 1) = 0`. Entity `+1` is the
id, so the Forest zombie becomes an ordinary zombie the moment his model is in
memory — which is also what makes him damageable, since id `0x16` is past the
20-row hit table and the DC keeps that `< 0x14` bound (`0x800120e8`). The
second, inert `id 0x16` record reuses the loaded model, never re-enters the
function, and keeps its id.

## entity[0x16b] — named

`entity + 0x16B` is inside the PC struct's `pad_16a[1]`. The DC uses it as the
**ADVANCED zombie variant flag**:

- written by the spawn block (`0x80105b60/…/0x80105c50`): `= 1` for spawn
  nibbles `0xC/0xD/0xE`, else `0`;
- read at DC STAGE1 `0x801093c0`, `0x801096b8`, `0x80109788`, where it gates an
  extra resident movement call (the same `0x8020-0x161C` helper the fast-zombie
  handler calls).

So: **`entity->dc_fast_zombie` (bit 0 of `pad_16a[1]`)** — set ⇒ the zombie runs
the fast/burst movement path. Rename it in the DC Ghidra struct as
`dc_fast_zombie` if the overlay program is imported.

## Mapping `0xC` / `0xD` / `0xE`

The spawn block is not stage-specific — it is in the zombie path in every stage
overlay. The values are used only by the arrange branch of mode-gated
`enemy_set`s (nibble `0xC` also appears in **both** builds for cerberus, id 2,
so the block is zombie-path-only). Confirmed spawn sites:

| Nibble | Spawns | Rooms (DC) | Result after remap |
|---|---|---|---|
| `0xC` | white zombie id 0, green id `0x11` | `STAGE1/ROOM1030`,`1031`,`10A0` | behaviour 0 + `dc_fast_zombie=1` |
| `0xD` | white zombie id 0 | `STAGE1/ROOM1110`, `STAGE6/ROOM6110` | behaviour 1 + flag |
| `0xE` | green zombie id `0x11` | `STAGE7/ROOM71C0`,`71C1` (as `0x8E`) | behaviour 0 + flag + `entity[0x88] += 0x64` |

`0xE`'s `+0x64` is the tougher variant (the `enemy_set` records carry
`slot_snd = 1` there). `0xC`/`0xD`/`0xE` are the "fast walking zombie" family;
the exact `fast`/`tough` split comes from the behaviour they normalise to and
the `+0x64`.

## Decoding `cmd_enemy_set` in an RDT

`tools/mine_room_scd.py --init <room.rdt>` decodes the PS1 init SCD directly.
`enemy_set` is opcode `0x1B`, 22 bytes; the record bytes after the opcode are
`[0]=id [1]=behavior_flags [2]=death_event_id [3]=condition [4]=sca_size
[5..6]=y [7..8]=angle [0xb..0xc]=x [0xd..0xe]=y [0xf..0x10]=z [0x11]=slot/snd
[0x12]=anim [0x13]=frame [0x14]=snd_group` (from `cmd_enemy_set`,
`CmdFunctions.cpp:845`).

The arrange spawns are gated by an RDT `if` on the mode flag, e.g.
`STAGE1/ROOM1030`:

```
if (bit_test bank5 off4 msbidx14 mask=0x00020000 cond=1) {
    ... 3 original zombies (id 0, behaviour 0/0/1)     // STANDARD
} else {
    enemy_set id=0 beh=0x0C ...                        // ADVANCED
}
```

`bank 5` is the PS1 `g_status_flags` (= PC `g_main_state_flags2`), so the test is
the ADVANCED bit — the same `0x20000` the stage overlay descriptor selection
uses. The arrange directories `STAGE8`–`STAGEE` hold the wholly-new rooms;
the `STAGE1`–`STAGE7` RDTs keep both branches inline.

## Per-enemy diffs

**Overall finding.** Across zombie, cerberus, hunter, chimera and adder, the
**only enemy whose AI/behaviour differs is the zombie** (new state 20 handler,
`entity[0x16b]` variant flag, `0xC/0xD/0xE` spawn markers). Every other enemy's
model is byte-identical and its handler is a pure recompile (register
allocation / `ori`→`addiu` only); what changes for them is the **RDT spawn /
placement data** (arrange-mode additions). Checked so far:

| Enemy | Model | AI code | Spawns |
|---|---|---|---|
| zombie (0,1,17) | identical | **changed** (new state + markers) | changed |
| cerberus (2) | identical | recompile-only | changed |
| hunter (6) | identical | recompile-only | changed |
| black tiger (4) | identical | recompile-only | unchanged¹ |
| plant42 (8) | identical | recompile-only | unchanged |
| chimera (9) | identical | recompile-only | changed |
| adder (10) | identical | recompile-only | unchanged |
| yawn (13/18) | identical | recompile-only | unchanged¹ |
| tyrant (12/16) | identical | recompile-only | unchanged |

¹ only the usual dropped `…1` room variant.

Checked (handler) details:

| Enemy | Handler (stage) DC / OG | match |
|---|---|---|
| cerberus | STAGE1 `801104d0` / `801107d0` | 91.6% |
| black tiger | STAGE3 `8010d824` / `8010df3c` | 88.1% |
| hunter | STAGE3 `80113048` / `80113b20` | 88.7% |
| plant42 | STAGE4 `801190d0` / `80119a28` | 90.6% |
| chimera | STAGE5 `801109d8` / `80110d18` | 89.8% |
| adder | STAGE3 `8011cef4` / `8011e0e0` | 91.6% |
| yawn | STAGE2 `80115be4` / `80116340` | 92.2% |
| tyrant | STAGE5 `80118080` / `80118988` | 87.5% |

Every "recompile-only" diff is the same register-allocation rewrite
(`move $v0,$v1; lui/lw; addiu $v1,$a0,off; move $a0,$v1; l(b/h)u $v1,off($a0)` →
`lui/lw; addiu $v0,$v1,off; l(b/h)u $a0,off($v1)`), i.e. the DC compiler kept
`entity` in one register; no semantic difference.

Not yet swept (same method): wasp 7, crow 5, monster plant 15, neptune 11,
and the two arms 20/21.

### Web spinner (id 3 — `EM1003`)

**AI changed** — one new spawn kind. The DC `wsp_state0` (STAGE4 DC
`0x801107a0`, same shape in every stage) special-cases `behavior_flags == 4`
before the OG's ceiling-hang block: it rewrites kind to 0, clears angle_x /
angle_z and world Y (so the spider lies on the floor at the spawn point), sets
state (`+0x84`) to **3** (web-shoot), health (`+0x88`) to **-1** and
`hit_state` (`+0x8A`) to **0x11**, then joins the normal init path (the
`+0x8A` value is what state 3 maps into a web-shoot behaviour, and `+0x88`
short -1 makes it dead on arrival). OG and the PC have no kind 4: they clamp it
into ceiling spider kind 2 (live, `Y = -6136`).

**Why it matters — room 405 (arrange `STAGEB/ROOMB050`/`B051`):** the Wesker
cutscene room's only spider `enemy_set` record is `1b 03 04 …` (kind **4**) —
the dead spider Wesker's script tramples. The base `ROOM4050`/`4051` files have
no spider spawn at all, so kind 4 never reaches the init outside DC ADVANCED.
With the clamp, the port's spider stayed alive, hung from the ceiling and
tracked the player through the cutscene instead of lying dead on the floor.
Ported and verified in-game 2026-09-17 (`WebSpinner.cpp` `wsp_state0`).

### Locating a monster handler (do it this way)

`update_entities` (OG `0x8002604c`) dispatches
`(*(entity->id * 4 + DAT_801fec1c))()`. `DAT_801fec1c` is a **per-stage** table
the overlay registers at entry (DC STAGE1 `0x80105558`), so every stage overlay
has its own table and handler addresses. **Per-stage entity tables:**

| Stage | OG | DC |
|---|---|---|
| STAGE1 | `0x80122100` | `0x801210e0` |
| STAGE2 | `0x80121910` | `0x80120a00` |
| STAGE3 | `0x8013283c` | `0x8013052c` |
| STAGE4 | `0x80133270` | `0x80131704` |
| STAGE5 | `0x8012dd2c` | `0x8012c3e4` |
| STAGE6 | `0x801290e4` | `0x80127e9c` |
| STAGE7 | `0x8012bfa8` | `0x8012aa0c` |

(Do **not** use the tables at `0x8012128c`/`0x801212ec`: those are the zombie's
*state* / *action_behavior* dispatchers, §"the zombie dispatch".)

### Cerberus (id 2 — `EM1002`/`EM1102`)

**Model unchanged:** `EM1002.EMD` and `EM1102.EMD` are byte-identical between OG
and DC (172804 B, same SHA-1).

**AI unchanged:** the id-2 handler (STAGE1 DC `0x801104d0`, OG `0x801107d0`) is
91.6% identical; the differences are register allocation
(`lui/lw/addiu $v0` vs `move $v0,$v1` chains) and the window boundary. No new
states or behaviours.

**Spawns changed** — the DC adds dogs:

| Room | OG | DC |
|---|---|---|
| `STAGE1/ROOM1140`, `STAGE6/ROOM6140` (mansion 1F dog corridor) | 4 dogs (beh 1,0,8,8) | +2 dogs (beh 1) at (9963,13964) and (9063,15464) → 6 |
| `STAGE3/ROOM3040`,`3041` (courtyard) | 3 dogs (beh `0xC`) | +3 dogs (beh `0xC`) at (25657,11984)dup, (3983,21562), (4200,16000) → 6 |
| `STAGE8/ROOM8010`,`8011` (arrange mansion 1F, new rooms) | — | 6 dogs: 3× beh 0 at (30000,30000) + 3× beh 1 |
| `STAGE1/ROOM1141`, `STAGE6/ROOM6141` | 4 dogs | dropped (the `…1` variant) |

Cerberus legitimately uses `behavior_flags` nibble `0xC` in **both** builds
(courtyard). The DC's new `0xC/0xD/0xE` marker block does **not** touch it: that
block lives inside the **zombie** spawn init (just above it, DC `0x80105880`
computes `health` from the zombie health table at `0x80121250`), so it only runs
for zombie entities.

### Hunter (id 6 — `EM1006`/`EM1106`)

**Model unchanged:** `EM1006.EMD` / `EM1106.EMD` byte-identical (191476 B).

**AI unchanged:** the id-6 handler (STAGE3 DC `0x80113048`, OG `0x80113b20`) is
88.7% identical with **register-allocation-only** differences. No new states.

**Spawns changed** — the DC adds hunters broadly (behaviour nibble in parens):

| Stage / room | OG → DC |
|---|---|
| `STAGE2/ROOM2070`,`2071` | 2 → 3 (beh 2) |
| `STAGE3/ROOM3080`,`3081` | 3 → 6 (beh 0) |
| `STAGE3/ROOM3090`,`3091` | 2/4 → 5/7 (+beh 0,0,2) |
| `STAGE3/ROOM30A0` | 4 → 6 (+beh 2) |
| `STAGE6/ROOM6030`,`6031` | 4 → 6 (+beh 0,0,2) |
| `STAGE6/ROOM6121` | 1 (beh 7) → dropped (`…1` variant) |
| `STAGE7/ROOM7070`,`7071` | 2 → 3 (beh 2) |
| `STAGE7/ROOM7130` | — → 2 (beh 0) |
| `STAGE7/ROOM71A0` | — → 3 (beh 0) |
| `STAGE8/ROOM8120` (arrange 1F) | — → 1 (beh 7) |
| `STAGED/ROOMD010` (arrange return 1F) | — → 6 (beh 7), `D011` → 2, `D040/D041` → 2× beh 0, `D120` → 1 |
| `STAGEE/ROOME010/E011` | — → 2 (beh 0), `E040/E041` → 3× beh 0, `E1A0` → 3 (beh 0) |

So the arrange stages (`STAGE8`/`STAGED`/`STAGEE`) are hunter-heavy in the DC,
on top of extra hunters sprinkled through the original mansion/courtyard rooms.

### Chimera (id 9 — `EM1009`/`EM1109`)

**Model unchanged:** `EM1009.EMD` / `EM1109.EMD` byte-identical (172384 B).

**AI unchanged:** the id-9 handler (STAGE5 DC `0x801109d8`, OG `0x80110d18`) is
89.8% identical with **register-allocation-only** differences. No new states.

**Spawns changed:**

| Stage / room | OG → DC |
|---|---|
| `STAGE5/ROOM50F0`,`50F1` (lab) | 5 → 10 (beh 2), +5 at new positions |
| `STAGE5/ROOM5101` | 2 (beh 2) → dropped (`…1` variant) |
| `STAGEC/ROOMC050`,`C051` (arrange lab) | — → 5 (beh 3,3,2,2,2) |
| `STAGEC/ROOMC110`,`C111` | — → 3/4 (beh 2, and 3 on C111) |

### Adder (id 10 — `EM100A`/`EM110A`)

**Model unchanged:** `EM100A.EMD` / `EM110A.EMD` byte-identical (60936 B).

**AI unchanged:** the id-10 handler (STAGE3 DC `0x8011cef4`, OG `0x8011e0e0`) is
91.6% identical with **register-allocation-only** differences. No new states.

**Spawns unchanged:** no room's id-10 records differ between OG and DC.

## Summary of the DC zombie differences

| What | OG | DC |
|---|---|---|
| zombie states_table entries | 20 (0-19) | 21 (0-20) |
| state/behaviour 20/10 handler | none (past table end) | `0x8010cdf8` fast-zombie burst |
| spawn behaviour nibbles | 0-0xA | + `0xC`, `0xD`, `0xE` markers |
| `entity[0x16b]` | pad | `dc_fast_zombie` variant flag |
| ADVANCED model remap | — | player/NPC outfits only (zombies keep `EM1000`) |
| Forest zombie | corpse `0x26` only | + entity `0x16` = `EM1016`/`EM1116`, arrange rooms 9121/E121 |
| enemy update ids | 0,1,17 = zombie | unchanged |

## Open items

1. Confirm the new handler + marker block at the corresponding address in every
   stage overlay (STAGE1 was disassembled; STAGE3 has no zombie table).
2. Which arrange rooms use the new ADVANCED outfit/NPC models (`EM1032`/
   `EM1033`/`EM1035`, `EM1040`/`EM1041`/`EM1043`/`EM104C`) — the model ids are
   resolved; the spawn/SCD side can be read the same way as the Forest zombie.
