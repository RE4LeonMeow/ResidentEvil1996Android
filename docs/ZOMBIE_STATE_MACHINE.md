# Zombie State Machine

> Resident Evil 1 PC — Enemy type 0 / 1 / 17
> All addresses from Ghidra analysis of `assets/ResidentEvil.exe`.
> Implementation: `src/game/entities/Zombie.cpp`

Every table listed here was read byte-for-byte out of the exe, and every function
address was confirmed against the jumptable that reaches it. Where the port
deviates from the original on purpose, it says so inline.

**Runtime status: playable and behaviour-verified as of 2026-08-06.** Chase,
damage reactions, falldown/get-up, death, severed limbs, head explosion, enemy
SFX and all weapon classes were tested in-game against the original. The defects
that only a running build could expose are collected under
[Found at runtime](#found-at-runtime-2026-08-06) — they are worth reading before
porting the next enemy type, because most of them are patterns rather than
one-offs.

---

## Read this first: the zombie has FOUR dispatch tables, and three of them overlap

This is the single most important structural fact about the zombie, and it is the
thing an earlier pass got wrong in every one of its four opportunities.

**All four dispatchers are tiny tail-jump functions.** None of them contains a
switch. For example, the whole of `update_zombie_action` is:

```asm
004342f0  MOV EAX,[_ENTITY]
004342f5  XOR ECX,ECX
004342f7  MOV CL, byte ptr [EAX + 0x86]     ; action_behavior
004342fd  JMP dword ptr [ECX*4 + 0x4bb350]  ; ...and that is the entire function
```

Fourteen bytes. `zombie_action_update` is twenty, and
`zombie_update_player_distance` is a Manhattan-distance calculation followed by
the same shape.

**Ghidra inlines every jump target.** Decompiling `update_zombie_action` produces
a ~500-line function with a thirteen-arm switch, because the decompiler follows
each `JMP` and inlines the callee. That listing is *not* the function. Taking it
at face value is how the port ended up with hand-rolled switches whose arms
pointed at unrelated functions. **Always read the jumptable address out of the
`JMP` and dump the table.**

**Three of the tables overlap each other.** They are one contiguous pointer block
addressed through several bases:

| Base | Name | Indexed by | Entries |
|---|---|---|---|
| `0x004bb2c8` | `zombie_states_table` | `state` (0x84) | 22 (see below) |
| `0x004bb2f0` | `zombie_behavior_tbl` | `behavior_flags & 0x0F` | 12 |
| `0x004bb350` | `zombie_action_tbl` | `action_behavior` (0x86) | 8 |
| `0x004bb370` | `zombie_move_behavior_tbl` | `behavior_flags & 0x0F` | 12 |
| `0x004c05c8` | `zombie_action_tbl_scd` | `action_behavior` (0x86) | 14, mostly NULL |

- `0x004bb2f0` is `zombie_states_table + 10*4`, so **`behavior_tbl[n]` IS
  `states_table[n + 10]`**. The states table therefore needs 22 entries, not 16:
  indices 16–21 exist only as the behaviour view's tail.
- `0x004bb370` is `0x004bb350 + 8*4`, so `zombie_action_tbl` indices **8–12 run
  off the end** into `zombie_move_behavior_tbl[0–4]`. The original has no bounds
  check; `Zombie.cpp` spells both views out rather than hiding the aliasing.

Same pattern as the three-views table documented in `CharacterNpc.cpp`.

---

## Architecture

```
update_entities (0x0048f0f0)
  └─ enemies_update_functions_tbl[entity.id]()      48 entries, EntityCommon.cpp
      └─ zombie_update (0x004338c0)                 ids 0, 1, 17
          ├─ zombie_states_table[state]()           ← unconditional, no NULL test
          ├─ mirror state fields into 0x184-0x187
          ├─ internal_timer--
          ├─ if state != ATTACK:
          │   ├─ SetEntityScaHitData / ResolveEntityScaCollision(player, ENTITY)
          │   ├─ HandleEnemyPlayerCollisions
          │   ├─ check_room_collision(pos, Sca_info+0x0A)
          │   └─ if laying down: check_room_collision_two_point(±600 body ends)   two-point push
          ├─ if *(u16*)0x174 != 0: blood_splatter_physics(joint+0xF8, 6)
          ├─ is_entity_in_switch_zone → has_enter_switch_zone
          └─ entity_add_fade_sprite ×2 (body, and hand joint if flagged)
```

The zombie is a **two-level** machine: `state` (0x84) picks a handler, and most
handlers then dispatch again on `action_behavior` (0x86) × `action_state` (0x87).

---

## Top-level state table

`zombie_states_table` @ **0x004bb2c8** — 22 entries. Dispatched
**unconditionally**: states 6, 7 and 9 are NULL and the original faults on them
too, so `Zombie.cpp` does not guard.

| State | Handler | Address | Notes |
|---|---|---|---|
| 0 | `zombie_init` | `0x00433440` | One-time spawn setup |
| 1 | `zombie_state_check` | `0x00433ae0` | → `zombie_behavior_tbl` + range/vertical checks |
| 2 | `zombie_damaged` | `0x00433db0` | → `zombie_damage_behavior_tbl` |
| 3 | `zombie_die` | `0x004340e0` | 4 death variants |
| 4 | `zombie_no_action` | `0x004342d0` | **A bare `RET`** — one instruction |
| 5 | `zombie_attack` | `0x004342e0` → `0x00435200` | 11-sub-state attack FSM (thunk) |
| 6, 7 | — | NULL | |
| 8 | `zombie_action_update` | `0x00454ab0` | SCD-driven path → `0x004c05c8` |
| 9 | — | NULL | |
| 10, 11 | `zombie_chase_player` | `0x00433bb0` | = `behavior_tbl[0]`, `[1]` |
| 12, 13 | `zombie_pushed_back` | `0x00433c60` | = `behavior_tbl[2]`, `[3]` |
| 14 | `zombie_random_chase` | `0x00433cf0` | = `behavior_tbl[4]` |
| 15 | `zombie_eating` | `0x00436690` | = `behavior_tbl[5]` |
| 16–21 | *(behaviour view only)* | | see next table |

> **State 4 is not the corpse animation.** `zombie_dead_animation` is a real
> ~100-line function at **`0x00437740`**, called from `zombie_die` and from the
> SCD handlers — not a state-table entry. Conflating the two left the port with an
> empty `{ }` stub, so no corpse ever counted down its `death_timer`, shrank its
> shadow, raised its death event flag, or took the get-back-up path.

---

## Behaviour table (`behavior_flags & 0x0F`)

`zombie_behavior_tbl` @ **0x004bb2f0** — the second view of the states table.
Dispatched unconditionally from `zombie_state_check`.

| Idx | = state | Handler | Address |
|---|---|---|---|
| 0 | 10 | `zombie_chase_player` | `0x00433bb0` |
| 1 | 11 | `zombie_chase_player` | `0x00433bb0` |
| 2 | 12 | `zombie_pushed_back` | `0x00433c60` |
| 3 | 13 | `zombie_pushed_back` | `0x00433c60` |
| 4 | 14 | `zombie_random_chase` | `0x00433cf0` |
| 5 | 15 | `zombie_eating` | `0x00436690` |
| 6 | 16 | `zombie_chase_player` | `0x00433bb0` |
| 7 | 17 | `zombie_eating` | `0x00436690` |
| 8 | 18 | `zombie_chase_player` | `0x00433bb0` |
| 9 | 19 | NULL | |
| 10 | 20 | `zombie_pushed_back` | `0x00433c60` |
| 11 | 21 | NULL | |
| 12–15 | — | **not pointers** — reads into `zombie_damage_action_tbl`'s bytes | |

---

## Action table (`action_behavior`) — `update_zombie_action` @ 0x004342f0

`zombie_action_tbl` @ **0x004bb350**, 8 entries.

| Idx | Handler | Address | Description |
|---|---|---|---|
| 0 | `zombie_idle` | `0x004349d0` | 8-sub-state head-turn loop |
| 1 | `zombie_slow_walk` | `0x00434cd0` | Shamble + footstep/leg-drag SFX |
| 2 | `zombie_chase_walk` | `0x00434eb0` | Walking chase with periodic weave |
| 3 | `fast_player_facing` | `0x00435c60` | Spin to face at 0x54/frame |
| 4 | `benddown_and_eat` | `0x00435e00` | Bend over a corpse and feed |
| 5 | `turn_towards_player` | `0x00436120` | The lunge — travels along `speed` |
| 6 | `zombie_vomiting` | `0x00436520` | Vomit, then recover |
| 7 | `zombie_falldown` | `0x004368a0` | Fall + recover |
| 8–12 | *aliases* | | `zombie_move_behavior_tbl[0–4]` |

## Move-behaviour table — `zombie_update_player_distance` @ 0x00434330

`zombie_move_behavior_tbl` @ **0x004bb370**, indexed by `behavior_flags & 0x0F`.
The function computes the Manhattan distance to the player into
`g_playerDisplacement` first, then tail-jumps.

| Idx | Handler | Address |
|---|---|---|
| 0 | `zombie_walk1` | `0x004343b0` |
| 1, 5, 6, 7, 8 | `zombie_check_player_distance` | `0x004345b0` |
| 2 | `zombie_slow_walk_alt` | `0x00434660` |
| 3, 10 | `zombie_idling` | `0x00434730` |
| 4 | `zombie_walk2` | `0x00434750` |
| 9, 11 | NULL | |

## SCD action table — `zombie_action_update` @ 0x00454ab0

`zombie_action_tbl_scd` @ **0x004c05c8**. Only six slots are live; the SCD only
ever writes those `action_behavior` values.

| Idx | Handler | Address | Description |
|---|---|---|---|
| 0 | `zombie_scd_no_action` | `0x00454ad0` | A second bare `RET`; `Zombie.cpp` fills this slot with `zombie_no_action` since both are one `RET` |
| 2 | `zombie_aggresive_roar` | `0x00454ae0` | Roar, walk to the SCD target at 0xC6/0xC8 |
| 10 | `zombie_headshot` | `0x00454d20` | Head bursts; 5 billboards aimed away from the shooter |
| 11 | `zombie_scd_dying` | `0x00454ed0` | Crawl-and-die |
| 12 | `zombie_scd_vomiting` | `0x00454f30` | animation 4 |
| 13 | `zombie_scd_vomiting2` | `0x00454f80` | animation 5 |
| 1, 3–9 | NULL | | |

`Zombie.cpp` bounds-checks this one where the original does not: the index comes
from script data, so unlike the other tables there is no invariant in code
proving a stray value is impossible.

---

## Duplicate names — five pairs to watch

Ghidra gives the same name to distinct functions. The port disambiguates and the
Ghidra database has been renamed to match.

| Name | Addresses | Port names |
|---|---|---|
| `zombie_slow_walk` | `0x00434cd0`, `0x00434660` | `zombie_slow_walk`, `zombie_slow_walk_alt` |
| `zombie_no_action` | `0x004342d0`, `0x00454ad0` | `zombie_no_action`, `zombie_scd_no_action` |
| `zombie_vomiting` | `0x00436520`, `0x00454f30` | `zombie_vomiting`, `zombie_scd_vomiting` |
| `zombie_attack` | `0x004342e0` (thunk), `0x00435200` | one function; the thunk is state 5's entry |
| `zombie_dying` | `0x00454ed0` | `zombie_scd_dying` (vs `zombie_die` @ `0x004340e0`) |

---

## The `0x004bb260`–`0x004bb31f` data block

One contiguous run addressed through several bases. The three byte tables are
read as **raw offsets off the pointer table**, e.g.
`*(byte *)((int)g_pZombieScaInfo + (rand & 0xf) + 8)` — which is why filing them
at their apparent addresses puts each one 8 bytes too far.

| Address | Contents |
|---|---|
| `0x004bb260` | ScaInfo record, standard zombie (16 bytes) |
| `0x004bb270` | ScaInfo record, naked zombie |
| `0x004bb280` | `g_pZombieScaInfo[2] = { 0x004bb260, 0x004bb270 }` |
| `0x004bb288` | = base + `0x08` — health base `[16]` |
| `0x004bb298` | = base + `0x18` — initial `animationId` `[16]` |
| `0x004bb2a8` | = base + `0x28` — stagger/poise budget `[32]` |
| `0x004bb2c8` | = base + `0x48` — `zombie_states_table` |
| `0x004bb31f` | `zombie_damage_action_tbl` (unaligned; overlaps `states_table[21]`, which is NULL) |

### ScaInfo — the collision radius

`zombie_init` stores `g_pZombieScaInfo[id]` (the **dereferenced** entry) into
`ENTITY->Sca_info`. Layout matches `CharScaInfo` in `CharacterNpc.cpp`;
`check_room_collision` reads the radius at `+0x0A`.

| Id | Record | Radius |
|---|---|---|
| 0 standard | `0x004bb260` | **422** |
| 1 naked | `0x004bb270` | **322** |

> Storing `0x004bb280` (the table itself) instead makes `Sca_info + 10` land in
> the health table and yields a collision radius of **15183** — a cylinder ~36×
> too wide.

### Health

```
health = zombie_health_tbl[rand & 0xF] - (rand % 22)
```

`zombie_health_tbl` = `{59, 59, 79, 59, 59, 39, 59, 79, 79, 99, 59, 79, 59, 59, 79, 59}`

So a zombie spawns with **17–99 HP**, not the 180–240 an earlier pass invented.

### Other tables

| Table | Address | Contents |
|---|---|---|
| `zombie_anim_id_tbl` | `0x004bb298` | `{0,0,9,9,0,12,9,29, 0,0,9,0,0,0,0,0}` — initial `animationId` by `behavior_flags & 0xF` |
| `zombie_stagger_tbl` | `0x004bb2a8` | 32 × 3–5 — `stagger_timer` by `g_RandSeed & 0x1F` |
| `zombie_damage_action_tbl` | `0x004bb31f` | 14 × `{0,0,5,0,2,0,0,4, 4,0,0,0,0,0}` |
| `zombie_damage_behavior_tbl` | `0x004bb330` | 12 handler pointers (below) |
| `zombie_recovery_timer_tbl` | `0x004bb400` | 16 × 2–9, × 30 frames |
| `zombie_attack_data_tbl` | `0x004bb3e8` | 4 × 3 shorts — lunge speed / timer / angle |

---

## State details

### 0 · `zombie_init` @ 0x00433440

1. `state = 1`, `ignore_player_flag = action_behavior = action_state = 0`
2. `Sca_info = g_pZombieScaInfo[0]`, swapped to `[1]` if `id == 1`
3. Two shadow quads via `FUN_004565f0`, tinted through **`g_animFrameIdSave`**
   (`0x00be0dfc`) — *not* `g_tempVar` (`0x00be0df8`). The tints are the
   **immediates** `0x00FFFF50` (the 400×400 SCA quad) and `0x00808080` (the
   700×900 body shadow); Ghidra prints both as `&DAT_00ffff50` / `&DAT_00808080`
   because the values look like addresses, and taking the address of a
   placeholder global feeds the quad a garbage RGB
4. `health` from the table above
5. `hit_threshold` from a 64-byte **stack** table, half selected by
   `Flg_ck(g_ScenarioFlags, 0x7b)` (difficulty)
6. `stagger_timer = zombie_stagger_tbl[g_RandSeed & 0x1F]`
7. `move_speed = 45`, `turn_speed = 24`
8. `animationId = zombie_anim_id_tbl[behavior_flags & 0xF]`
9. If `behavior_flags & 0x02` → `localMatrix.t[1] = 1` (the dword at `+0x38`)
10. If `behavior_flags == 6` (**whole byte**) → `status_flags |= 0x0A`,
    `health = -1`, state 3 / behaviour 4
11. If `behavior_flags == 10` → `status_flags |= 0x04`, disable six leg joints
12. If `behavior_flags & 0x40` → `state = 8` (SCD path)

> `status_flags |= 0x0A` is bits **1 and 3**, not 0 and 3 — Ghidra prints the
> immediate in decimal (`| 10`). Bit 1 is the one `ResolveEntityScaCollision`
> tests to skip a deactivated entity, so getting it wrong leaves corpses shoving
> the player around.

### 1 · `zombie_state_check` @ 0x00433ae0

Returns immediately if `behavior_flags & 0x80` (SCD owns the entity). Otherwise:
dispatch `zombie_behavior_tbl[behavior_flags & 0xF]`, `status_flags &= 0x1F`,
`entity_check_alert_range(3000)` (unless laying down or falling),
`entity_check_visual_range(4500)`, vertical check at ±100 → sets the
above/below bits, `entity_check_visual_range(4000)` for the eating behaviours
(`behavior_flags == 5` or `== 7`, whole byte), and finally `behavior_step & 0x04`
forces the aligned bit (follow without attacking).

### 2 · `zombie_damaged` @ 0x00433db0

```
if ignore_player_flag == 0:
    behavior_step & 0x04  → restore the mirror dword from 0x184, hit_state = 0, return
    action_speed & 0x80   → store dword 0x03070101 at 0x84:
                            state=1, ignore=1, action_behavior=7, action_state=3
                            hit_state = 1; moan unless behavior_step & 1
    (hit_state & 0x78) == 0x08 → action_speed++;  if hit_threshold <= action_speed
                                 and not laying → action_speed = 0x80, reroll threshold
    (hit_state & 0x78) == 0x10 and action_state == 0 and --stagger_timer == 0
                                 and not laying → action_speed = 0x80
    action_behavior = zombie_damage_action_tbl[(hit_state & 7)
                                              + (behavior_flags & 2) * 3]
    ((hit_state - 1) & 2)  → action_behavior += bit 10 of turn_toward_target(player, 1024)
    behaviour 7 or 5       → action_behavior = 7 (always);
                             action_state = 1 only if animationId == 13
    ignore_player_flag = 1
zombie_damage_behavior_tbl[action_behavior]()      ← unconditional, no bound
```

> The laying-down stride is **6**, not 3: `0x00434037` is
> `LEA ESI,[ECX + ECX*2]` applied to the already-masked `0x02`. Max index 13,
> hence 14 entries. With a stride of 3 a prone zombie picks the standing reaction.

`zombie_damage_behavior_tbl` @ **0x004bb330**:

| Idx | Handler | Address |
|---|---|---|
| 0, 1 | `short_push_back` | `0x00436c80` |
| 2, 3 | `push_and_stagger` | `0x00436f00` |
| 4 | `push_and_drop` | `0x00437450` |
| 5 | `explode_leg_and_drop` | `0x00437050` |
| 6 | `long_push_back` | `0x00437540` |
| 7 | `benddown_and_standup` | `0x00437690` |
| 8 | `zombie_idle` | `0x004349d0` |
| 9 | `zombie_slow_walk` | `0x00434cd0` |
| 10 | `zombie_chase_walk` | `0x00434eb0` |
| 11 | `fast_player_facing` | `0x00435c60` |

`zombie_falldown` (`0x004368a0`) is **not** in this table. Its call site is still
unidentified.

**Entries 0–3 share a tail, and it is the single most damaging thing to get
wrong.** `short_push_back` (`0x00436e76`) and `push_and_stagger` (`0x00436fdf`)
both finish by storing the dword **`0x00030101`** at `+0x84` — state 1 (IDLE),
ignore 1, behaviour 3, sub 0 — *not* state 3. Writing state 3 here sends every
ordinary hit reaction into the death sequence: one handgun shot kills, and
because `action_state` is left at 2 the corpse enters `zombie_dead_animation` at
its hold state and never plays the fall, so it appears to die and freeze on the
spot. `zombie_falldown` and `benddown_and_standup` end with the same dword, which
is what makes the mistake easy to make in only some of the four places.

Both also store the waypoint as **words** (`MOV word ptr [ECX+0x166]`), set
`behavior_flags = 0` as a plain byte write when the low nibble is 5, and
`push_and_stagger` selects its `Add_speedXZ` argument (`0x800` vs `0`) from the
same `action_behavior == 2` test that picks animation 7 vs 6 — its frame gate is
`animation_frame_id == 0x14`, an equality, and neither arm returns early.

### 3 · `zombie_die` @ 0x004340e0

Picks `action_behavior` once, then dispatches. Raises the death room event via
`Flg_on(`**`g_EnemiesFlags`**`, death_event_id)` — bank `0x00be987c`, *not*
`g_roomItemsFlags` (`0x00be989c`). Using the item bank means the kill never fires
its scripted event and scribbles on item state instead.

| `action_behavior` | Anim | Tail |
|---|---|---|
| 0 | 8 | two SFX cues (frames 8 and `0x1a`), `Add_speedXZ(0)` |
| 1 | 10 | one cue (frame 18), `frame < 5 → speed += 90`, `Add_speedXZ(0x800)` |
| 2 | 16 | `zombie_dead_animation` only |
| 3 | — | `magnum_shot_pushback` (`0x00436b70`) |

Cases 0 and 1 are easy to conflate — they differ in the cue set, the speed bump
and the `Add_speedXZ` argument.

### `zombie_dead_animation` @ 0x00437740

The tail of every death, and the only place a "dead" zombie can stand back up.

```
0 → death_timer = 70, blend = 3, hit_state = 1
    moan unless behavior_step & 1 or head joint & 0xCC   (already blown apart)
1 → action_state += Joint_move(...)
2 → probe the prone body with check_room_collision_two_point(±800), saving and restoring
    localMatrix.t[0..2] + 0x40 and the angle so the probe cannot move anything
    ┌ gets back up if ALL hold:
    │   not SCD-controlled (0x40), behaviour byte != 4, head intact,
    │   *(u16*)&g_stageId != 0x201, animationId == 8, floor clear,
    │   (g_RandSeed & 3) == 0, g_main_state_flags bit 16 clear
    │ → behavior_flags = 3, health = 1, state dword = 1, t[1] = 1
    └ otherwise: health = -1, shadow tint + shrink, raise the room event,
                 action_state = 3, status_flags |= 0x0E
3 → move_speed_current = 0, shadow shrinks, death_timer--; at 0 → action_state 4
4 → if behavior_flags & 0x40: Flg_on(g_SysFlags, scd_anim_param), clear 0x86/0x87
```

The stage/room test is a **word** compare at `0x00be9820`, spanning `g_stageId`
(low byte) and `g_roomId` (high).

### 5 · `zombie_attack` @ 0x004342e0 → 0x00435200

| Sub | Phase |
|---|---|
| 0 | Roar, pick attack type into `attacking_direction`, snap player to the grab point, set player anim 5 |
| 1 | Wind-up |
| 2 | Arm the damage loop; `ATTACK_TIMER` = 105, or 30 if the player is already mid-attack |
| 3 | Damage every 19 frames, blood billboard, button-mash reduction, player-death branch |
| 4 | Release → **falls straight into** 5, 6 or 8 in the same frame |
| 5 | Withdrawal → state 2, behaviour 6, `status_flags &= 0xF5` |
| 6 | Head bite; keyframe fires the head explosion (5 billboards + 2 joint tints + SFX) |
| 7 / 9 | Headless corpse — identical code twice, only two stores reordered |
| 8 | Vomiting |
| 10 | Finish the bite → sub-state 7 |

Attack types (`attacking_direction`, 0x16C): 0 facing, 1 laying front, 2 laying
back (vomit).

> Sub-state 4 exits by `goto` into the 5/6/8 bodies — the follow-up runs in the
> **same frame**. Breaking out instead costs a frame on every release.
> `Zombie.cpp` factors those three bodies into helpers to express it, because a
> `goto` into a case that declares locals will not compile.

> Sub-states 7 and 9 store the dword `0x03020103` at `+0x84`
> (state 3, ignore 1, behaviour 2, sub 3), call `BillboardSetColor` on
> **`+0xE4`** (`pushVelocity`, the shadow quad — not `+0x1C`), and use
> `g_EnemiesFlags`.

### 8 · `zombie_action_update` @ 0x00454ab0

See the SCD table above. `zombie_aggresive_roar` is the interesting one: it walks
to the **SCD target** at `0xC6`/`0xC8` (not the player), takes its `Joint_move`
reverse flag from `scd_entity_flags & 1` — the only place in the zombie that
varies it — and loops sub-state 3 back to 2 while `collisionFlags & 0x80` is set
so the script can hold it walking.

### 10–15 · Movement states

All four follow the same shape: an FOV + line-of-sight test that can jump
straight into `zombie_attack`, otherwise `zombie_update_player_distance` (gated on
`ignore_player_flag == 0`) followed by the state's own action dispatcher.

| State | Function | FOV, range | Extra gate | Tail |
|---|---|---|---|---|
| 10, 11 | `zombie_chase_player` | 700, 1500 | `behavior_step & 4`, `action_speed & 0x80` clear | `update_zombie_action` |
| 12, 13 | `zombie_pushed_back` | 512, 2200 | none | `zombie_pushback_action` |
| 14 | `zombie_random_chase` | 700, 1500 | also `collisionFlags & 8` clear | `update_zombie_action` |

The attack entry is a **word** store of `5` at `0x84`, so it clears
`ignore_player_flag` at `0x85` as well.

### 15 · `zombie_eating` @ 0x00436690

Sub-states 0→1 and 2→3 fall through. Loops animation `0x1E`/`0x1D`, blood
billboard at frame `0x19`, and stands up when the player comes within 3000
(Manhattan, into `g_playerDisplacement`). On completion: `behavior_flags = 0`
(plain store), or `= 4` if `*(u16*)&g_stageId == 0x504`; then the dword
`0x00030101` at `0x84` (state 1, ignore 1, **behaviour 3**), and
`localMatrix.t[1] = 0`.

---

## Entity field widths — five fields are wider than they look

An earlier pass declared these as bytes, sometimes with a `pad_*` after them. All
five are accessed as words by the original. Verified by checking every access at
each offset against Ghidra's parsed instruction width.

| Offset | Field | Was | Is | Consequence of the byte version |
|---|---|---|---|---|
| `0xC4` | `action_ticks_counter` | `u8` + `pad_c5` | **`u16`** | Every timer wrapped at 256 instead of 65536 |
| `0xE2` | `next_turn_timer` | `u8` + `pad_e3` | **`u16`** | (no users yet) |
| `0xDE` | `scd_timer` | `scd_timer_lo` + `_hi` | **`u16`** | Latent: writes were LE-equivalent, but a byte *read* truncates |
| `0x166` | `player_pos_x` | `u8` + `pad_167` | **`s16`** | Waypoints truncated to the low 8 bits |
| `0x168` | `player_pos_z` | `u8` | **`s16`** | ″ |

`0x166`/`0x168` are the movement **waypoint**, not "the player's last known
position" — `zombie_slow_walk` sets it 5000 units ahead of the zombie's own
facing, and `fast_player_facing` sets it to the player's position on exit.

**Two negative results.** `hit_state` (`0x8a`) is genuinely a byte — 40 accesses,
all `byte ptr`, including bit tests. And `internal_timer` (`0x182`) is **mixed by
entity type**: every zombie site is `byte ptr`, but `monster_plant_update` and
two others use `word ptr` at the same offset, including `DEC word ptr`. Do not
widen it — an offset's width is only well-defined per entity type, so this sweep
is valid for the zombie and will need redoing for each type ported.

---

## Bulk stores at 0x84 and 0x86

The original repeatedly writes several adjacent one-byte fields as a single
dword or word. Transcribing only the fields you recognise silently drops the
rest — this was the most common defect found in the whole audit.

| Store | Decodes to |
|---|---|
| `dword 0x00000001` | state 1, ignore 0, behaviour 0, sub 0 |
| `dword 0x00010001` | state 1, ignore 0, behaviour 1, sub 0 |
| `dword 0x00030101` | state 1, ignore 1, behaviour 3, sub 0 |
| `dword 0x00070101` | state 1, ignore 1, behaviour 7, sub 0 |
| `dword 0x03070101` | state 1, ignore 1, behaviour 7, sub 3 |
| `dword 0x03020103` | state 3, ignore 1, behaviour 2, sub 3 |
| `dword 0x02020001` | state 1, ignore 0, behaviour 2, sub 2 |
| `word  0x0005` at 0x84 | state 5, ignore 0 |
| `word  0x0200` at 0x86 | behaviour 0, sub 2 |
| `word  0xNN00` at 0x86 | behaviour `NN`, sub 0 |

> `0x02020001` is the value Ghidra prints as `&g_enemy_state` — the constant
> happens to look like an address. It appears in `fast_player_facing`,
> `long_push_back` and the `update_zombie_action` tail.

Word tests over adjacent *independent* fields are also common and legitimate:
`CMP word [0x172]` spans `is_moving` + `move_max_steps`, and
`CMP word [0x174]` spans `splatter_flag` + `bob_speed`.

---

## Two timer idioms — do not mix them up

```asm
; test AFTER the decrement  → fires when the counter reaches 0
DEC  word ptr [ECX + 0xc4]
CMP  word ptr [ECX + 0xc4], 0x0

; test BEFORE the decrement → fires one frame later, wraps through 0xFFFF
MOV  CX, word ptr [EAX + 0xc4]
TEST CX, CX
LEA  EDX, [ECX + -0x1]
MOV  word ptr [EAX], DX
```

`zombie_idle` uses **both**: sub-state 1 is after-decrement, sub-states 3–6 are
before-decrement. `Zombie.cpp` factors the second form into
`zombie_tick_expired()`.

---

## Behaviour flags (`behavior_flags` @ 0x02)

| Bit | Mask | Name |
|---|---|---|
| 1 | `0x02` | `ZOMBIE_FLAG_LAYING_DOWN` |
| 2 | `0x04` | `ZOMBIE_FLAG_INACTIVE` (dead on floor) |
| 6 | `0x40` | `ZOMBIE_FLAG_VOMITING` / SCD-driven |
| 7 | `0x80` | `ZOMBIE_FLAG_SCD_CONTROLLED` |

Lower nibble: 0 normal · 2 laying · 3 crawling · 5 laying eating · 6 spawned as a
corpse · 7 laying eating (variant) · 10 lying on floor.

**Many comparisons against `behavior_flags` use the whole byte, not the nibble** —
`zombie_init`'s corpse and floor checks, `zombie_die`'s behaviour-5 test, and
`zombie_state_check`'s eating test are all `CMP AL, imm8`. Several stores are
plain byte writes (`= 0`, `= 4`, `= 2`) rather than masked merges.

## Status flags (`status_flags` @ 0x00)

| Bit | Mask | Meaning |
|---|---|---|
| 0 | `0x01` | Active / visible |
| 1 | `0x02` | Deactivated — `ResolveEntityScaCollision` skips the entity |
| 3 | `0x08` | Dead |
| 5 | `0x20` | Player above (vertical) / in visual range (distance) |
| 6 | `0x40` | Aligned with player |
| 7 | `0x80` | Player below (vertical) / in alert range (distance) |

---

## Shared scratch globals

Four scratch words the zombie reuses. Two of them are 4 bytes apart and Ghidra
labels them inconsistently across functions — confusing them is silent.

| Address | Name | Used for |
|---|---|---|
| `0x00be0de0` | `g_playerDisplacement` | Manhattan distance to the player / to the waypoint |
| `0x00be0df0` | `g_collPushDepthZLo` | "`action_behavior` on entry" in `zombie_walk1` |
| `0x00be0df4` | `g_entity_bkp` | Last `entity_pathfind_update` result |
| `0x00be0df8` | `g_tempVar` | "`action_behavior` on entry" in `zombie_walk2`, `zombie_chase_walk` |
| `0x00be0dfc` | `g_animFrameIdSave` | Wander-turn mode (6 or 7); shadow tint in `zombie_init` |
| `0x00be11b0` | `g_playerPosScratch` | Steering target passed to `turn_toward_target` |

Ghidra renders `[0x00be0df0]` as `g_tempVar` in one `zombie_walk1` comparison and
as `action_behavior_00be0df0` in another; `CMP dword [0x00be0df0],0x2` at
`0x004344f3` settles it — both touch the same word.

---

## Found at runtime (2026-08-06)

Nine defects that reading the disassembly did not catch, in the order they were
found. Each was confirmed against the original before fixing. The middle column
is what the player actually sees — that mapping is the useful part, because the
next enemy type will fail the same ways.

| Symptom in game | Cause | Where |
|---|---|---|
| Zombies wander instead of closing on the player | `entity_update_wander_turn` read its steering waypoint from **bytes at `+0xB3`/`+0xB4`** (inside `pad_b0`, always 0, so every entity turned toward the room origin) instead of the sign-extended shorts at `+0x166`/`+0x168`; and its stuck threshold from a byte at `+0x61` instead of the signed word `move_speed_current` at `+0xC2` | `EntityCommon.cpp` |
| One handgun shot kills, corpse freezes mid-air | `short_push_back` / `push_and_stagger` wrote `state = 3` instead of the dword `0x00030101` (see above) | `Zombie.cpp` |
| Waypoint refreshes on the wrong frames | `entity_pathfind_update` rebuilt its state byte as `counter + 1`; the original is `INC byte`, which preserves the accumulated bit 5 (LOS-blocked) across frames 0–2 so it can be tested once on frame 3 | `EntityCommon.cpp` |
| Head-explosion FX wrong; sound from the player's position | `enemy_hit_reaction_zombie` must swap `ENTITY` to the hit enemy for `Flg_on` / `joint_setup_attack_effect` / `Snd_em` and restore it before the billboards (`0x0043d0c5`–`0x0043d10c`). `joint_setup_attack_effect` reads `ENTITY->id` for the effect size and `weaponJointsPtr - jointsStructs` to reach the weapon joint, so on the player it wrote outside the zombie entirely | `WeaponDamage.cpp` |
| No enemy SFX at all, though banks load | `Snd_em` read the bank-group nibble from **`entity+0x10`** — which `EntityModelLoader` fills with the low byte of `&ENTITY->scaMatrixData` — instead of `entity+0x161`, whose high nibble `cmd_enemy_set` fills from the SCD enemy record's byte `0x15`. `id + group*10` then ran past the 48-record table and `Snd_em` returned before touching it | `SoundSystem.cpp` |
| Blown-off arm hangs in mid-air | `FUN_004896c0` (`0x004896c0`) was an empty stub. It is the severed-limb ballistic step and the **only** thing that integrates a detached joint's world translation — `rotate_entity` stops recomputing the matrix once `0x8`/`0x2` clear. See below | `GteMatrix.cpp` |
| Fallen limb never gets its ground shadow | `blood_splatter_physics` wrote its resting height as −99; the original writes **−100**, and `zombie_update`'s limb-shadow test is `world.t[1] == -100` exactly | `EntityCommon.cpp` |
| Knife crash (`/GS`: stack around `knifePos` corrupted) | `weapon_hit_detect_knife` handed an 8-byte `SVECTOR` to `ApplyLVAndMul0Matrix`, which writes a whole 32-byte `MATRIX`. The original reserves exactly `0x20` and reuses its offsets scratch as the output; distances come from that output's `t[0]`/`t[2]` | `WeaponDamage.cpp` |
| Flamethrower, grenade launcher and rocket do nothing | `weapon_hit_detect_projectile` measured from the player entity. The original measures from **`g_playerPosScratch`** (`0x0043d819`/`0x0043d824`) — the caller stages the *projectile's* position there. That is the entire difference between this detector and the gun one | `WeaponDamage.cpp` |

### Severed-limb physics — `FUN_004896c0` @ 0x004896c0

`render_entity` calls this once per frame for every joint whose flags carry
`0x4`, having first reloaded the launch velocity into the joint's rotation
SVECTOR: `rotation.x = -20`, `rotation.y = 200`, `rotation.z = 0`. Arguments are
`(joint, gravity = -35, floorY = -100, siblingIdx = 1)`.

| Joint offset | Field | Use |
|---|---|---|
| `+0x02` | `field_02` | Frame counter; reset to 3 on landing |
| `+0x03` | `pad_03` | `0x80` = has touched the floor, `0x01` = at rest |
| `+0x04` | `rotation` | The velocity SVECTOR (x, y, z) |
| `+0x58/5C/60` | `world.t[0..2]` | X / Y / Z |

`vy = (u16)field_02 * gravity + rotation.y`, all in 16 bits, then `Y -= vy`. Y is
negative-up, so vy falling through zero is the arc. The tumble rotation's sign
flips once `pad_03 & 0x80` is set, which is what settles the limb flat. First
touch bounces to −350; the second snaps to exactly `floorY`.

### Two patterns worth generalising

**A shared scratch global can be the reference point, not just a temporary.**
`weapon_hit_detect_projectile` is the clearest case: `g_playerPosScratch` carries
the projectile position *into* the detector. Reading the "obvious" source instead
compiles, runs, and silently never hits.

**`ENTITY` is an implicit argument to most helpers.** Any helper reading
`ENTITY->id`, `->angle`, `->jointsStructs` or `->weaponJointsPtr` needs `ENTITY`
pointing at the right entity, and the original swaps it around narrow call
groups. Check the save/restore pairs — they are load-bearing, and dropping one
corrupts memory rather than merely misbehaving.

---

## Still outstanding

- `entity+0x7e` is an **angle backup**, modelled as `speed.pad`.
  `check_room_collision` uses the same `0x6c`/`0x70` position backups but never
  touches `0x7e`, because it does not revert rotation.
- `zombie_falldown`'s call site is unidentified — it is not in the damage table.
- Ghidra has `explode_leg_and_drop` mis-bounded as `0x00437050`–`0x0043743d`.
- The zombie's ground shadow tint flips between two interleaved ramps every
  frame (visible in the `[shadow]` trace in `FadeSprite.cpp`). A ground shadow
  should hold a steady colour; something is writing the quad's colour dword each
  frame. Not diagnosed.
- `blendMode` from the effect band table is dropped in the sprite flush —
  `MarniDrawSprite` takes only colour + SRV, so the PS1 semi-transparency modes
  are not wired through. Blood splatter may want additive.

---

## Porting pitfalls, in the order they cost the most time

1. **Read the jumptable, never the inlined decompile.** Ghidra follows tail-jumps
   and inlines callees; the result looks like a giant switch and is not one.
2. **Dump every data table out of the exe.** Four tables in this file had
   plausible, invented contents. An all-plausible table silences a subsystem
   while the code around it reads correctly.
3. **Check whether a table has a second base.** Three of the four here overlap.
4. **Decode dword/word stores at `0x84`/`0x86` field by field.**
5. **Check field widths against instruction widths, not against the struct.**
   Five fields here were declared narrower than the code accesses them.
6. **Watch for missing `case` fallthroughs** — the original leans on them heavily,
   and an added `break` costs exactly one frame, which is invisible in a build.
7. **Distinguish the two timer idioms** above.
8. **A stub with a different parameter list is an overload, not a duplicate.** It
   links fine and silently wins at every call site that sees the header.
9. **An out-parameter's SIZE is part of the contract.** `ApplyLVAndMul0Matrix`
   writes a 32-byte `MATRIX`; handing it an 8-byte `SVECTOR` smashed the stack the
   first time the knife swung. Check what every other call site passes.
10. **Confirm a value survives to its consumer before debating its source.** Three
    rounds went into *which* colour the head-explosion FX picked while the tint was
    being clamped to white downstream regardless. Trace the output end first.
11. **`&DAT_00xxxxxx` is usually an immediate, not a pointer.** Colours, packed
    RGB and masks whose value lands in the image range get rendered that way, and
    `&placeholder` compiles cleanly while substituting garbage.
12. **Resizing a static array reshuffles `.bss`** and moves where any unrelated
    latent overrun lands. Prefer the heap for port-only buffers, and pin the size
    of structs backing large statics with a `static_assert`.

---

## SCD integration

Zombies are spawned by `cmd_omodel_set`; cutscene actors by `cmd_enemy_set`. The
entity's `id` selects `enemies_update_functions_tbl[id]` (**48** entries — ids
0–21 monsters, 22–47 the shared human driver in `CharacterNpc.cpp`).
`death_event_id` at `+0x163` is the `g_EnemiesFlags` bit raised on death.

Script-driven zombies set `behavior_flags & 0x40` and run through state 8.

### `entity+0x161` — the sound-bank group

`cmd_enemy_set` (`0x004617d0`) packs three things into `entity+0x161`:

```
entity[0x161]  = scd[0x12] & 0x0F        ; the enemy slot index
entity[0x161] |= scd[0x15] << 4          ; the SOUND BANK GROUP
entity[0x161] |= 0x80  if scd[0x04] != 0
```

`Snd_em` (`0x0047fca0`) reads that high nibble: `id + ((x & 0x70) >> 4) * 10`
indexes `g_emSndBanks` (`0x00ac99f0`, 48 records of 8 bytes), bailing out above
47. The zombie's ten cues sit at records **0–9** — group 0 — in the per-room name
table: `z_taore`, `z_ftL`, `z_ftR`, `z_kamu`, `z_osou`, `z_unaruA`, `z_head`,
`z_Hkick`/`z_haki`, `z_Ugoron`/`z_sanj`, `z_unaruB`.

`Snd_em` takes **one** argument even though call sites push two (`PUSH 0; PUSH 9`);
the second is ignored and the caller cleans up 8 bytes.

---

## Weapon damage system

> Source: `src/game/WeaponDamage.cpp`

### Entry point

`apply_weapon_damage(weapon_id)` @ **0x0043c020**, each frame the player fires:

1. Scan `g_EnemiesList` (30 slots) for `status_flags != 0`
2. Per candidate, call `PTR_weapons_hit_detection_functions[weapon_id - 1]`
3. Keep the closest via `g_playerDisplacement`
4. `check_weapon_line_of_sight` — `room_check_sight_blocked` over boundary
   quadrants 3→0; returns 0 only if every layer is clear. Weapons 0–4 are blocked
   by walls, 5+ bypass
5. Damage lookup at `tableIdx = weaponAdj + enemyType * 10`
6. `health -= damage`, `hit_state = hitState | (weapon_id << 3)`, post-hit callback
7. `state = 3` if `health < 0`, else `2`

### Hit detection callbacks

`PTR_weapons_hit_detection_functions` @ **0x004bb530**:

| Idx | Weapon | Function | Address | Method |
|---|---|---|---|---|
| 0 | Knife | `weapon_hit_detect_knife` | `0x0043d690` | Distance from the weapon joint, per-enemy range offsets |
| 1–4 | HG / Shotgun / Python / GL | `weapon_hit_detect_gun` | `0x0043d410` | 3D aim cone, near/far tiers, aim-up headshot cone |
| 5–9 | Heavy | `weapon_hit_detect_projectile` | `0x0043d810` | Euclidean distance **from `g_playerPosScratch`** |

**The three detectors do not share a reference point.** The knife measures from
the player's weapon joint (`joints[14].world` composed with a per-character
offset, output into a full `MATRIX` whose `t[0]`/`t[2]` are the reach point). The
gun cone measures from the player entity. The projectile detector measures from
**`g_playerPosScratch`** (`0x00be11b0`/`0x00be11b8`), which the *caller* fills
with the projectile's own position — `effect_behavior_flamethrower`
(`0x0040ed30`, behaviour 37) before `apply_weapon_damage(6)`, and likewise the
grenade shot and the rocket. It also zero-extends the enemy radius
(`XOR EDI,EDI; MOV DI, word [EAX+0xa]`).

> The flamethrower's damage does **not** come from the fire animation.
> `g_weaponFireData[4]` really is `{6, 0, 0, 0}` in the exe — an all-zero row is
> correct there. Do not "fix" it.

### Aim cone

Two triangular wedges (near depth 50, far depth 200 + range), each bounded by a
left/right vector, tested by `checkEntityInRangeCone` (`0x0043d590`) with three
2D cross products. The vertical offset in `g_svecScratch.x` picks the SCA volume:

| Player state | offset | Hits |
|---|---|---|
| Aim normal | 50 | Torso / leg volumes |
| Aim up (`flags & 0x20`) | 0 | Head volume |
| Aim down (`flags & 0x80`) | 50 | Skips the zombie unless shotgun |

### Hit state → reaction

| `hit_state & 7` | Body part | Reaction |
|---|---|---|
| 0 | Torso | Standard stagger / fall backward |
| 1 | Back | Reverse stagger |
| 3 | Legs | Leg cut / crawl |
| 4 | Head | Head explosion / headless death |

Two paths produce a head hit: the damage table (the Python has `hit_state = 4`
hardcoded for zombies, so it **always** headshots), and the aim cone shifting to
head height when aiming up.

Post-processing before the store:

```c
if ((player.flags & 0xE0) != 0x20)
    hitState += (player.flags >> 5);   // 0, 2, 4 or 6 by aim
hitState |= (weapon_id << 3);
enemy->hit_state = hitState;
```

`zombie_damaged` then reads low 3 bits as direction/type, bits 3–6 as the
reaction phase, and bit 7 (from the weapon shift) in `short_push_back`'s joint
severing.

### Other weapon tables

All verified byte-for-byte against the exe when the aim/fire system landed
(2026-08-03). The hit records are **12-byte** records, not shorts:

| Table | Address | Layout |
|---|---|---|
| `PTR_post_hit_callbacks` | `0x004bb558` | 10 fn ptrs (knife sfx+reaction, reaction-only, shotgun range chip, blood FX family) |
| `g_enemy_hit_reactions` | `0x004bb580` | 20 fn ptrs, indexed by enemy id (`g_weaponHitEnemyType`) |
| `g_enemyHitJointLists` | `0x004bb5d0` | 20 enemy types × 6 joint indices for the blood spurts |
| `weapons_ranges` | **`0x004bb648`** (DWORDs) | 10 weapons × 2 characters, `weaponAdj + (id&1)*10` |
| `weapon_hit_records_easy` | `0x004bb698` | 200 × 12B: `{short kx,ky,kz; short dmg@+6; byte type@+8, data@+9, hit@+10, pad}` — kx/ky/kz (knockback → `g_playerPosScratch`), type/data (→ `0x00be0dec`/`0x00be0df0`) and the health snapshot (`0x00be0df4`) are used for **both** difficulties |
| `weapon_hit_records_normal` | `0x004bbffe` | 200 × 12B: `{short dmg@+0; short unk@+2; byte hit@+4; short kx,ky,kz}` |

`apply_weapon_damage` (0x0043c020) writes `g_weaponHitEnemyType` (0x00be0de4)
= enemy id before the post-hit callback, which the callbacks and reactions
read. The previous port stubs (`weapons_damage_table[30]` etc.) had the wrong
layout and were removed.

---

## Effect sprite FX — how the head explosion reaches the screen

> Source: `src/game/EffectSystem.cpp`, `src/game/RoomStubs.cpp`,
> `src/game/SpriteRenderer.cpp`

The head explosion spawns effect types **0** (blood puff, a core00 sprite) and
**3** / **4** (gore splatter, room sprites) via `Effect_CreateBillboard`
(`0x0047be30`). Getting them to look right needed four separate facts, none of
which is visible from the zombie code.

### Sprites pack DOWN a page, and `curU` is a V cursor

In `setup_effect_sprite_textures` (`0x0047bc80`) the variable Ghidra suggests is
"curU" is the **V cursor** down a 256-tall texture page, advanced by
`header.field_0A` (the sprite's V extent) and wrapped at `0x100`. **`texY` is the
page index** and increments on each wrap; `texY - 0x18` is the original's texture
id. At `0x0047bdc6` it adds the cursor to byte **+1** of every one of the
sprite's 4-byte UV records — and `effect_submit_sprite` reads
`uv[0] = U, uv[1] = V, uv[2]/uv[3] = pivot`, so that is the V. Sprites ship with
UVs local to their own image and this makes them page-absolute.

`texY` runs across **both** blocks: the weapon pass starts it at `0x18`, and the
room pass resumes from where that left off (`DAT_00bf0a3e` is written only by the
startSlot-0 pass, so the room block's starting V is a game-wide constant the
effspr TIMs are authored around). Measure the page from the absolute `0x18` bias,
never from the start of the room block.

The decisive evidence is in the sheets — convert them and look. **`esp000` is
page 0 and holds exactly the eight core00 weapon FX sprites** (glass cracks,
smoke, muzzle flash, sparks) stacked down its 256 rows, so the cursor reaches
~256 by the end of the weapon pass and the first room sprite wraps to page 1 at
V=3. That is why `esp001` / `esp201` / `esp202` begin their first sprite row at
y=3. A per-block page index puts the gore on `esp000` and it draws muzzle flashes.

Rooms declare up to 7 sprite types against as few as 2 pages (ROOM1010 declares
3, 4, 32 against `esp000` + `esp201`), so sprite→sheet is the wrap count, never
the declaration order. 8 of 320 RDTs need a 5th page and there is no SRV for it;
those now log and skip.

### The sprites are greyscale and tinted at draw time

`effect_submit_sprite` scans `g_EffectBlendTable` for the first row whose
`startV + len` exceeds the sprite's **page-absolute** V, and that row's
`colorIdx` selects a record in `g_EffectColorRecords`. Which band table applies
comes from `effect_depth_record()`: `texY - 0x18` indexes the room's four effspr
entries, and the resulting effspr **file** index picks the rows. Those rows live
behind the pointer in `g_EffectSpriteTexConfig` (`0x004c4f50`, 8 bytes/entry) —
and that struct's "mode" field is really the **row count**:
`TexturePage_Load(page, tim, rowCount, rows)`.

`colorIdx 0` is `{ff,ff,ff}` (white); the blood tint is `colorIdx 4`,
`{69,1e,0a}`; the gore bands resolve to 12/13, `{99,33,33}`. So a sprite that
reports V=0 renders **white**. The port samples the weapon-FX block from
per-sprite SRVs whose UVs stay sprite-local, so their page V is carried
separately in `g_effectSpriteBandV` for the band scan only — the sampling
coordinate is left alone.

### The tint has to survive the sprite queue

`TextureDraw::r/g/b` are a **0..1 multiplier** — `FlushSpriteCommands` does
`(int)(cmd->r * 255.0f)` and clamps. `draw_texture` sets the convention:
`colorMulR` alone (`0x80`, PS1-neutral) gives `128 * 2/255 = 1.004`. A 0..255
per-effect tint therefore needs the same `/255`. Without it `SubmitEffectSprite`
produced 153.6, clamped to white, and **every effect sprite rendered with its raw
texture colour and no tint at all**. Because the gore frames are stored
near-black, blood drew dark grey while sprites already warm in the sheet looked
correct — which is why only *some* were wrong, and why fixing the band selection
alone changed nothing on screen.

> Order of investigation matters here: confirm the colour survives to the draw
> call *before* reasoning about which colour was chosen.
