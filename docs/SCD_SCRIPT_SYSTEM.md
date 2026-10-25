# SCD Script System

The SCD (SCript Data) system is the room scripting VM. Every room's logic —
door placement, item placement, enemy spawns, camera cuts, cutscene
choreography, lighting, BGM — is data in the RDT file interpreted by two
cooperating interpreters:

| Interpreter | Address | Source | Role |
|---|---|---|---|
| `run_command_functions` | `0x00473f60` | [RoomEvents.cpp:99](../src/game/RoomEvents.cpp#L99) | **Command VM.** Runs a linear block of SCD commands to completion, synchronously. Used for room init. |
| `room_events_check` | `0x0041d6a0` | [RoomEvents.cpp:555](../src/game/RoomEvents.cpp#L555) | **Event VM.** Runs 8 concurrent coroutine-like event scripts, one step per frame. Used for cutscenes and per-frame room logic. |

Both dispatch individual commands through the same 81-entry table
`script_command_funcs_table` (`0x004c1110`), implemented in
[CmdFunctions.cpp](../src/game/CmdFunctions.cpp).

---

## 1. Data sources

`room_set` (`0x00477xxx`, [RoomInit.cpp:555](../src/game/RoomInit.cpp#L555))
wires three RDT sections into globals:

```c
g_RoomInitScd    = g_RdtPointer->initialization_scd;  // 0x00d213bc — block list for the command VM
g_RoomScdOpcodes = g_RdtPointer->scd_opcodes;         // 0x00d213b8
g_RoomEventScripts     = g_RdtPointer->scd_opcodes2;        // 0x00d213b4 — event script pointer table
```

`g_RoomEventScripts` is a NULL-terminated array of `int` **relative** offsets that
`room_set` relocates in place to absolute pointers:

```c
int* evtPtr = (int*)g_RoomEventScripts;
while (*evtPtr != 0) { *evtPtr += (int)g_RoomEventScripts; evtPtr++; }
```

So `((unsigned char**)g_RoomEventScripts)[scriptIndex]` is the entry point of event
script `scriptIndex`.

---

## 2. Command VM — `run_command_functions`

### Container format

The initialization SCD is a chain of **blocks**. Each block is:

```
+0x00  u16  blockSize    // total bytes of this block INCLUDING these 2 header bytes
+0x02  ...  opcodes      // command stream, terminated by opcode 0x00 (cmd_block_end)
```

A `blockSize` of `0` terminates the chain.

```c
g_CmdOpcodesPointer = g_ScdBranchStack;   // reset the branch stack (0x00bf0808)
g_ScriptContinueFlag = 0;             // reset the nesting depth
for (u16 sz = *p; sz != 0; sz = *p) {
    g_ScdOpcodes = (u8*)(p + 1);      // skip the size word
    ... run until cmd_block_end, then unwind pending branches ...
    p = (u16*)((u8*)p + sz);          // next block
}
g_ScdOpcodes = (u8*)p;                // leave the pointer on the terminator
```

### Execution + branch unwinding

```c
while (true) {
    do {
        cmdResult = table[*g_ScdOpcodes]();
    } while (cmdResult != 0);         // run until a command returns 0

    if (g_ScriptContinueFlag == 0) break;   // no pending branch → block done

    g_CmdOpcodesPointer--;                  // pop a saved resume address
    g_ScdOpcodes = (u8*)*g_CmdOpcodesPointer;
    g_ScriptContinueFlag--;
}
```

Two return conventions drive this:

- **Return 1** — "continue": the interpreter immediately dispatches the next
  opcode. Every side-effecting command returns 1.
- **Return 0** — "stop": ends the straight-line run. Used by `cmd_block_end` (end of
  stream) and by *condition* commands whose test failed. A failed condition
  therefore aborts the rest of the stream, and the outer loop resumes at the
  address `cmd_if` pushed — that is how `if` skips its body.

Conditional commands are `0x04 0x06 0x07 0x10 0x11 0x1A 0x1D 0x22 0x36 0x38
0x3C 0x3F 0x50`. They return the boolean directly, so `false` = 0 = stop = skip.

### Interpreter state

| Global | Address | Type | Meaning |
|---|---|---|---|
| `g_ScdOpcodes` | `0x00bf0800` | `u8*` | Instruction pointer. **In the original this is a `ushort*`** — see §5. |
| `g_CmdOpcodesPointer` | `0x00bf0804` | `u32*` | Branch stack pointer (grows up). |
| `g_ScdBranchStack` | `0x00bf0808` | `u32[16]` | Branch stack storage. 64 bytes, ends at `g_pScdEventCurrent` (`0x00bf0848`). Declared `static` in [RoomEvents.cpp:124](../src/game/RoomEvents.cpp#L124). |
| `g_ScriptContinueFlag` | `0x00bf07fa` | `u8` | Pending-branch / nesting depth counter. |

### Callers

- `room_init` — runs `g_RoomInitScd` once when a room loads.
- Event opcode `0x06` (`scd_event_cmd_run_scd`) — runs an inline block embedded
  in an event script.

---

## 3. Event VM — `room_events_check`

Runs every frame from `game_loop`, plus once from `room_set`. Gated on
`g_message_flags & 0x80`. Iterates all 8 slots of `g_ScdEventTable`.

### `ScdEventEntry` (0x34 bytes, table at `0x00bf084c`)

```c
struct ScdEventEntry {
    u8   state;            // 0x00  0=cmd, 1=wait-anim, 2=movement, 3=set-behavior
    u8   pad_01;           // 0x01  cleared when leaving state 1
    u8   active;           // 0x02  0 = slot free
    u8   stackDepth;       // 0x03  index into the three stacks below; 0xFF = empty
    Entity* entity;        // 0x04  entity this script drives
    u8*  scriptPtr;        // 0x08  instruction pointer
    u32  returnStack[4];   // 0x0C  loop / call resume addresses
    u32  callStack[4];     // 0x1C  saved command addresses (0xFC / 0xFD)
    s16  counterStack[4];  // 0x2C  loop counters
};
```

`stackDepth` is indexed as `(signed char)stackDepth`, so the `0xFF` empty
sentinel means the first push lands at index 0 (`0xFF + 1`).

### Entry lifecycle

`ScdEventEntry_Init` (`0x0041d620`) / `ScdEventEntry_Create` (`0x0041d650`):

```c
entry->active     = 1;
entry->state      = 0;
entry->scriptPtr  = ((u8**)g_RoomEventScripts)[scriptIndex];
entry->stackDepth = 0xFF;
entry->entity     = ENTITY;
```

`ScdEventEntry_Create(slot, scriptIndex)` with `slot > 7` allocates the first
free slot.

### Execution states — why the same byte means different things

`ScdEventEntry.state` is not bookkeeping: **it selects which opcode table the
next byte is read from.** Each frame `room_events_check` takes the byte at
`scriptPtr` and

1. tests it against the `0xF6`–`0xFF` control range *first* — those work
   identically in every state (see the table below), then
2. `switch (entry->state)` to pick the opcode table for everything else.

So one byte has up to three meanings:

| Byte | in state 0 | in state 1 | in state 2 |
|---|---|---|---|
| `0x00` | NOP | advance | NOP |
| `0x01` | → state 1 | **hangs** | → state 0 |
| `0x02` | → state 2 + reset the entity | **hangs** | `position += speed` |
| `0x04` | set the current entity (3 bytes) | **hangs** | `position += speed` and apply rotation |

State 1 only understands `0x00` and `0x80`–`0x8B`; its `default` breaks out
**without advancing `scriptPtr`** and returns 1, so the dispatcher's
`do { ... } while (result != 0)` re-reads the same byte forever. State 2's
`default` just returns, which also leaves the pointer put — the outer loop
`goto`s straight back. Only state 0 treats an unknown byte as an error, and it
deactivates the entry. A byte read in the wrong state does not misbehave
quietly; it locks the game.

There is no way to know a byte's meaning from the bytes alone — you have to
know which state execution reached it in. That is why a disassembler has to
either simulate from the entry point or let you choose the state, and why
`tools/rdt_event_editor.html` offers "read from state 0/1/2" per script.

**Reading a script in practice.** Every script the RDT+0x68 table points at
begins in state 0 (`ScdEventEntry_Init` sets `state = 0`), so start there. Only
switch views once you have traced an actual `0x01` / `0x02` / `0x03` and want to
read the bytes that follow it.

#### What each state is for

- **State 0 — command state.** The structural work: set the current entity,
  create or kill another event, run an inline SCD block (`0x06`) or a single SCD
  command (`0x07`), restart with a different script. It is also the only state
  that can enter the others. A script that only touches flags and room actions —
  like `ROOM20E1`'s event 0 — never leaves it.
- **State 1 — animation wait.** The entity is playing an animation and the
  script rides along with it. `scd_event_state1_anim` returning 0 means *stop
  for this frame*, which is what makes a scripted gesture or line take time.
  Its opcodes are a disjoint set in the `0x80`+ range (look-at targets, anim
  selection, timers).
- **State 2 — movement.** Per-frame integration with no waiting: add `speed` to
  the position, apply the rotation steps, set an absolute position. An NPC
  walking a path is state 2 inside a counted `0xFA`/`0xFB` loop.

#### Transitions

```
                 0x01
   ┌──────────────────────────────►  state 1  (wait animation)
   │                                    │
   │                          0x80 / 0x8B│
   │            ◄───────────────────────┘
state 0
   │            0x02 (reset entity) / 0x03 (keep it)
   ├──────────────────────────────►  state 2  (movement)
   │                                    │
   │                                0x01│
   │            ◄───────────────────────┘
   └── 0x08 → ScdEventEntry_Init: restart, back to state 0
```

`0xFF` deactivates the slot from any state; `0x08` re-inits it (which resets the
state to 0 along with the stack depth).

State 3 exists in the dispatcher but is **unreachable** — see below.

### Control-flow opcodes (checked before the state switch, so they work in every state)

| Op | Len | Effect |
|---|---|---|
| `0xF6` | 1 | Push stack level, advance, then fall into `0xF7`. |
| `0xF7` | 1 | **Wait**: if `((u8*)&g_main_state_flags)[1] & 2` is clear *and* `g_menu_choice_id & 0x80` is clear, advance and pop. Otherwise stay. Yields the frame either way. |
| `0xF8` | 3 | Push level, `counterStack[top] = *(s16*)(p+2)`, advance 1, fall into `0xF9`. |
| `0xF9` | 3 | `--counterStack[top]`; when it hits 0, skip 3 bytes and pop. Yields the frame. |
| `0xFA` | 4 | Loop head: push level, `counterStack[top] = *(s16*)(p+2)`, advance 4, `returnStack[top] = scriptPtr`. |
| `0xFB` | 1 | Loop tail: `--counterStack[top]`; 0 → advance 1 and pop, else jump to `returnStack[top]`. |
| `0xFC` | 2 | Call: push level, `callStack[top] = p+2`, `scriptPtr += p[1]`, `returnStack[top] = scriptPtr`. |
| `0xFD` | 1 | Run the command at `callStack[top]` through `script_command_funcs_table`. Returned 0 → advance 1 and pop; non-zero → jump to `returnStack[top]`. This is the event VM's `if`. |
| `0xFE` | 1 | Advance 1, yield the frame. |
| `0xFF` | 1 | `active = 0` then advance 1, yield. |

### State 0 — command opcodes ([RoomEvents.cpp:655](../src/game/RoomEvents.cpp#L655))

| Op | Len | Effect |
|---|---|---|
| `0x00` | 1 | NOP. |
| `0x01` | 1 | → state 1 (wait animation). |
| `0x02` | 1 | → state 2, and `entity->ignore_player_flag = 2`, `action_behavior = 0`, `action_state = 0`. |
| `0x03` | 1 | → state 2 without touching the entity. |
| `0x04` | 3 | Set `entity` from `[type][index]`: 0=player, 1=`g_EnemiesList[i]`, 2=`g_omodel_table[i]`, 3=`g_item_model_table[i]` (item models). |
| `0x05` | 3 | `ScdEventEntry_Create(p[1], p[2])`. |
| `0x06` | var | `run_command_functions(p+2)`, then `scriptPtr += p[1]`. |
| `0x07` | var | Set `g_ScdOpcodes = p+2`, `scriptPtr += (*(u16*)p >> 8)`, then dispatch one command through the table. |
| `0x08` | 2 | `ScdEventEntry_Init(self, p[1])` — restart with a different script. Yields. |
| `0x09` | 2 | `g_ScdEventTable[p[1]].active = 0`. |
| other | — | Unknown opcode: deactivate the entry and bail out of the whole function. |

### State 1 — animation opcodes (`scd_event_state1_anim`, `0x0041da30`)

Operates on `g_pScdEventCurrent->entity`. All return 1.

| Op | Len | Effect |
|---|---|---|
| `0x00` | 1 | Advance. |
| `0x80` | 1 | Clear `ignore_player_flag`, then as `0x8B`. |
| `0x8B` | 1 | `pad_01 = 0`, advance, → state 0. |
| `0x81` | 2 or 12 | Set `lookAtFlags = p[1]`. If `flags & 0x0F`, consume 10 more bytes: `0x93` → resolve `scd_target_ptr` from a `[type][index]` pair; otherwise set `scd_pos_x/y/z` (with `flags & 0x20` wrapping negatives by `+0x1000`). Then `lookAtYawStep` (default `0xC0`) and `lookAtPitchStep` (default `0x40`). |
| `0x82` | 1 | `lookAtFlags &= ~0x10`. |
| `0x83` | 9 | `state=8`, behavior/action from `p[1..2]` (bit `0x1000` = additive mode gated on `collisionFlags & 0x80`), then `unk_c6`, `unk_c8`, `scd_anim_param`; `scd_timer_lo=0x28`. |
| `0x84` | 5 | `state=8`, `action_behavior=1`; `animationId`, `scd_anim_param`, `scd_entity_flags = (animData >> 6) & 0x3FC`. |
| `0x85` | 5 | `state=8`; behavior/action from `p[1..2]`, `animationId` + `scd_anim_param` from the next word. |
| `0x86` | 1 | Idle: `state=1`, behavior/action/hit_state cleared. |
| `0x87` | 4 | `scd_entity_flags` OR / SET / XOR by mode `p[1]`. |
| `0x88` | 3 | `scd_timer_lo`, `scd_timer_hi`. |
| `0x89` | 2 | Set `animation_frame_id`, reset blend. For `id < 0x20` and `animationId <= 0x0F`, remap through `g_ScdAnimRemap`; `animationId > 0x0F` → `action_state = 3`. For `id >= 0x20` → `action_state = 1`. |
| `0x8A` | 2 | As `0x89` but no remap. |

`g_ScdAnimRemap` (`0x004bec80`, 32 bytes) — `(action_state - 1, animationId)` pairs
indexed by the incoming `animationId`. Declared in [Globals.h:650](../src/Globals.h#L650),
defined in [CmdFunctions.cpp](../src/game/CmdFunctions.cpp):

```
anim: 0      1      2      3      4      5      6      7      8      9
pair: (0,0) (0,1) (0,2) (0,3) (0,4) (1,0) (1,1) (1,2) (1,3) (1,4)
```

### State 2 — movement opcodes (`scd_event_state2_movement`, `0x0041e1a0`)

| Op | Len | Effect |
|---|---|---|
| `0x00` | 1 | NOP. |
| `0x01` | 1 | → state 0. |
| `0x02` | 1 | `localMatrix.t[0..2] += speed.x/y/z`. |
| `0x03` | 1 | Apply `move_step_x` to `position.pad` and `move_step_z` / `state` to the two angle halves. |
| `0x04` | 1 | `0x02` + `0x03`. |
| `0x05` | 4 | Set `speed.x/y/z` from 3 signed bytes. |
| `0x06` | 4 | Set `move_step_x`, `move_step_z`, and `state`/`ignore_player_flag` from 3 signed bytes. |
| `0x07` | 8 | Absolute position from 3 `s16`. |
| `0x08` | 3 | Write one transform component at byte offset `p[1]`. |
| `0x09` | 2 | `hit_state = p[1]`. |
| `0x0A` | 4 | Parametric set by `p[1]`: 0/1/2 = `localMatrix.t[i]`, 3 = `health`, 4 = `unk_c6`, 5 = `unk_c8`. |
| `0x0B` | 12 | As `0x0A`, then also `position.pad` and both angle halves. |

### State 3 — `scd_event_state3_set_behavior` (`0x0041e150`) — UNREACHABLE

2 bytes. Sets `g_PlayerDpadHeld = 0xFF`; if `p[0] != entity->action_behavior`
resets `action_state`; assigns `action_behavior = p[0]`. Returns 1.

**Nothing ever enters this state.** The dispatcher has a `case 3:` for it, but
`ScdEventEntry.state` is written in exactly six places across the whole
codebase and every one stores 0, 1 or 2:

| Site | Value |
|---|---|
| `ScdEventEntry_Init` ([RoomEvents.cpp:18](../src/game/RoomEvents.cpp#L18)) | 0 |
| state 2 opcode `0x01` ([:215](../src/game/RoomEvents.cpp#L215)) | 0 |
| state 1 opcode `0x80`/`0x8B` ([:354](../src/game/RoomEvents.cpp#L354)) | 0 |
| state 0 opcode `0x01` ([:728](../src/game/RoomEvents.cpp#L728)) | 1 |
| state 0 opcode `0x02` ([:732](../src/game/RoomEvents.cpp#L732)) | 2 |
| state 0 opcode `0x03` ([:739](../src/game/RoomEvents.cpp#L739)) | 2 |

Treat the handler as dead code: no shipped script byte is ever read through it,
and a disassembler should not offer state 3 as a reading mode.

---

## 4. Command opcode reference (`script_command_funcs_table`, `0x004c1110`)

81 entries, `0x00`–`0x50`. There is **no padding** in the original — the byte
right after entry `0x50` is the string `"DOOR_AT_SET "`, so an opcode above
`0x50` executes string data. The decomp declares the array as 256 entries with
`0x51`–`0xFF` left `nullptr`, which turns that into a null-pointer crash instead
of arbitrary execution.

Notation: `[a,b]` is one 16-bit word, `a` = low byte, `b` = high byte. `Len` is
the total instruction length in bytes. **Cond** marks commands whose return
value is a condition (0 aborts the straight-line run).

Naming convention: all 81 handlers follow `cmd_<subject>_<verb>` — `_set` for
side-effecting commands, `_test` for **Cond** commands, plus a few plain verbs
(`cmd_item_search`, `cmd_got_item`).

| Op | Name | Addr | Len | Layout / effect |
|---|---|---|---|---|
| `0x00` | `cmd_block_end` | `004604d0` | 1 | Clears `g_ScriptContinueFlag`, returns 0. End of block. |
| `0x01` | `cmd_if` | `004604e0` | 2 | `[op, skipLen]`. Pushes `(p+2) + skipLen` on the branch stack, `++g_ScriptContinueFlag`. |
| `0x02` | `cmd_else` | `00460520` | 2 | `[op, jumpLen]`. Pops the branch stack and jumps `p += jumpLen`. |
| `0x03` | `cmd_end_if` | `00460550` | 2 | Pops the branch stack. |
| `0x04` | `cmd_bit_test` | `00460570` | 4 | **Cond.** `[op, bank][sel, expect]`. `sel & 0x1F` = bit index, `(sel & 0xE0) >> 3` = byte offset into the bank. Returns `bitIsSet ^ expect`. Banks: 0 `g_ScenarioFlags`, 1 `g_ScenarioFlags2`, 2 `g_LocksFlags`, 3 `g_EnemiesFlags`, 4 `g_SysFlags`, 5 `g_main_state_flags`, 6 `g_message_flags`, 7 `g_roomItemsFlags`, 8 `g_RoomFlags`, 9 `g_itemUseFlags` (0x00d213a0 — per-frame item-use flags, cleared by `game_loop` each frame and re-armed by room logic; bits `itemId-0x1B` = "item usable here", bit `0x3F` = radio transmission active). |
| `0x05` | `cmd_bit_op` | `00460650` | 4 | `[op, bank][sel, mode]`. `mode` 0=set, 1=clear, 2=toggle. Same bank/sel encoding as `0x04`. |
| `0x06` | `cmd_state_byte_test` | `00460760` | 4 | **Cond.** `[op, fieldIdx][mode, cmpVal]`. Compares the byte at `(&g_stageId)[fieldIdx]`. `mode` 0 `==`, 1 `>`, 2 `>=`, 3 `<`, 4 `<=`, 5 `!=` (relative to `cmpVal`). |
| `0x07` | `cmd_state_word_test` | `00460800` | 6 | **Cond.** `[op, pad][fieldIdx, mode][cmpVal:u16]`. Compares `((u16*)&g_fading_state)[fieldIdx]`. |
| `0x08` | `cmd_state_byte_set` | `004608a0` | 4 | `[op, fieldIdx][value, pad]`. `(&g_stageId)[fieldIdx] = value`. |
| `0x09` | `cmd_cut_lock_set` | `00460920` | 2 | `[op, camId]`. Saves the current camera in `g_cutId`, switches to `camId`, walks `cam_switch_zones` (stride `0x14`, id at `+2`) to find it, calls `cut_set`, sets `g_main_state_flags |= 0x100000` (lock camera). |
| `0x0A` | `cmd_current_cut_set` | `00460990` | 2 | Restores the camera saved in `g_cutId` and clears `0x100000`. |
| `0x0B` | `cmd_message_set` | `004609f0` | 4 | `[op, msgId][pause:u16]`. `set_message_display(msgId, pause)`. The pause operand is a word, not a byte. |
| `0x0C` | `cmd_door_set` | `004611b0` | 26 | `[op, slot]` + a 24-byte door record. Writes `g_RoomActionTable[slot*0xC]`: `[0]=1`, `[1]=p[0x19]`, `[2..3]=slot`, `[8..11]= p+2` (record pointer). Bumps `g_RoomActionTail`. |
| `0x0D` | `cmd_room_action_set` | `00461130` | 18 | `[op, slot]` + 16 bytes: an 8-byte zone box then handler/flags/three `u16`. Writes the 12-byte room action entry: `[0]=p[0xA]` (handler), `[1]=p[0xB]` (probe flags), `[2..7]` = three `u16` from `p[0xC..0x11]`, `[8..11]= p+2`. Not item-specific — pick-ups come from `0x18`. |
| `0x0E` | `cmd_skip_2bytes_opcode` | `00460900` | 2 | No-op that advances. |
| `0x0F` | `cmd_mirror_set` | `004610b0` | 8 | `[op, modeBits][mirrorMin:u16][mirrorMax:u16][planeCoord:u16]`. Sets `g_main_state_flags` bits 0-1, writes the mirror plane globals (`g_mirrorExtentMin`, `g_mirrorExtentMax`, `g_mirrorPlaneCoord`), then re-runs `SetupEntityJointAnimation` on the player and rebuilds the weapon-joint clone (`FUN_0048bfe0` / `FUN_0048c020`). |
| `0x10` | `cmd_used_item_test` | `00460f30` | 2 | **Cond.** `[op, itemId]`. `itemId == g_usedItemId`. |
| `0x11` | `cmd_picked_item_test` | `00460f10` | 2 | **Cond.** `[op, itemId]`. `itemId == g_pickedItemId` (`DAT_00be9833`) — the item id recorded by the last pickup. |
| `0x12` | `cmd_room_action_reset` | `00460fc0` | 10 | `[op, slot]` + 8 bytes → overwrites bytes `[0..7]` of the event entry. |
| `0x13` | `cmd_room_action_arm` | `00461010` | 4 | `[op, slot][b0, b1]` → event entry bytes `[0]`, `[1]`. |
| `0x14` | `cmd_scd_event_create` | `00461040` | 4 | `[op, pad][slot, scriptIdx]`. `ScdEventEntry_Create(slot, scriptIdx)`. |
| `0x15` | `cmd_bgm_play` | `00460a80` | 2 | `[op, ch]`. Sound channel record = `(u8*)&g_SndBank + ch*8` (bank `int` at `+0`, slot byte at `+5`). Calls `SetSndSlot`, sets `g_BGM_STATE |= 1 << (ch+3)`. |
| `0x16` | `cmd_bgm_stop` | `00460c70` | 2 | `[op, ch]`. If `g_BGM_STATE` bit `ch+3` set: `setSndStop`, clear the bit, `set_volume(bank, -1)`. |
| `0x17` | `cmd_sfx_3d_play` | `00460d80` | 6 or 10 | `[op, bank][sndId, vol][posType, enemyIdx]` then 4 more bytes for `posType` 0-3. `posType` 0 = explicit `(x,0,z)` written into `g_playerPosScratch` (`0x00be11b0`, three **ints**); 1 = `g_playerEntity.scaMatrixData.localMatrix.t` (`+0x34`); 2 = `g_EnemiesList[enemyIdx].scaMatrixData.localMatrix.t`; 3 = `play_sfx(bank, bank)`. `posType > 3` consumes only 6 bytes. Positions come from the 3-int `localMatrix.t`, **not** the packed `position` SVECTOR at `+0x6C`. |
| `0x18` | `cmd_item_model_set` | `00461220` | 26 | Room items model. `p[1] & 0x7F` = event slot (bit 7 = alt rotation), `p[0xA]` = item type, `p[0xC]` = model index, `p[0xD]` = SCA parent, i.e. the space `p[0xE..0x13]` are relative to (`0xFF` none = absolute room coords, `0xFE` player, else `g_omodel_table[value]`; see docs/SCD_COMMAND_OPCODES.md), `p[0xE..0x13]` = position `s16 x/y/z`, `p[0x14..0x15]` = anim word, `p[0x16]` = `g_roomItemsFlags` bit, `p[0x17..0x19]` = entry flags. Loads the TMD, may spawn a billboard when the flag is set and bit `0x8000` is present. |
| `0x19` | `cmd_model_flag_set` | `00460f50` | 4 | `[op, modelIdx][value, pad]`. `*(u8*)g_item_model_table[modelIdx] = value`. |
| `0x1A` | `cmd_item_search` | `00460f80` | 2 | **Cond.** `[op, itemId]`. Scans `g_ItemSlotsPointer` (stride 2) over `g_TotalHeldItems`. |
| `0x1B` | `cmd_enemy_set` | `004617d0` | 22 | Enemy spawn. `p[1]` = enemy type id, `p[2]` = `behavior_flags`, `p[3]` = `g_EnemiesFlags` guard bit (`0xFF` = none; if already set, skip the spawn), `p[4]` = force-init, `p[5]` = SCA hit-data size / 6, `p[6..7]` = `position.pad`, `p[8..9]` = yaw, `p[0xA..0xB]` = pitch, `p[0xC..0xD]` = x, `p[0xE..0xF]` = y, `p[0x10..0x11]` = z, `p[0x12] & 0xF` = slot, `p[0x13]` = `animationId`, `p[0x14]` = `animation_frame_id`, `p[0x15]` = extra flags. |
| `0x1C` | `cmd_room_light_fade_set` | `00462210` | 6 | `[op, lightR][delta:s16][rgbMask:u16]`. Special room light: `delta != 0` seeds `g_SpecialRoomLightState` to `0` or `0x7FFF` by sign. Mask bits 0/1/2 → B/G/R = `0xFF`. |
| `0x1D` | `cmd_equipped_item_test` | `00460ee0` | 2 | **Cond.** `[op, weaponId]`. Compares the equipped slot's item id. |
| `0x1E` | `cmd_voice_play` | `00461a80` | 4 | `[op, type][param:u16]`. `play_sound_and_voice_effect`, then `g_main_state_flags |= 0x20000`. |
| `0x1F` | `cmd_omodel_set` | `00461ac0` | 28 | Static object model. `p[1] & 0x3F` = omodel slot (bit 7 = queue texture), `p[2]` = entry flags (bit 4 = alt rotation), `p[3]` = SCA parent (`0xFF` none, `0xFE` player, `<0x80` `g_omodel_table[value]`, else `g_EnemiesList[value & 0x7F]`), `p[4..9]` = position `s16 x/y/z`, `p[0xA..0xB]` = anim word, `p[0xC..0x1B]` = sprite/anim parameters. Contains many stage/room-specific palette and position fixups. |
| `0x20` | `cmd_player_pos_set` | `00430f60` | 14 | `[op,pad][posPad:s16][directionAngle:s16][speedX:s16][x:s16][y:s16][z:s16]` — offsets `+2,+4,+6,+8,+10,+12`. Clears `unk_e0` bits 2-3 via `&= 0xFFF3`. Positions mirror into `localMatrix.t[0..2]`. Note `+6` is `speed.x` (`0x76`) in `PlayerEntity`, whereas the same offset in `Entity` (opcode `0x21`) is the upper half of `angle`. |
| `0x21` | `cmd_enemy_pos_set` | `00430fe0` | 14 | `[op, enemyIdx][pad:s16][yaw:s16][pitch:s16][x:s16][y:s16][z:s16]` — offsets `+2,+4,+6,+8,+10,+12`, same shape as `0x20`. `enemyIdx` is `(s16)word0 >> 8` (arithmetic). Clears `scd_entity_flags` (`+0xE0`) bits 2-3 via `&= 0xFFF3`. Positions mirror into `localMatrix.t[0..2]`. |
| `0x22` | `cmd_item_count_test` | `00431100` | 4 | **Cond.** `[op, searchId][mode, cmpVal]`. Sums inventory quantities for an ammo *family* (`searchId` 10-0x12 group several item ids), compares the total. Returns 0 if nothing matched. |
| `0x23` | `cmd_cut_lock_write` | `00431280` | 2 | `[op, lock]`. `lock != 0` sets `g_main_state_flags |= 0x100000`, else clears it. |
| `0x24` | `cmd_room_action` | `004312b0` | 4 | `[op, itemSlot][actionIdx, pad]`. Calls `room_check_actions[actionIdx](&g_RoomActionTable[itemSlot*0xC])`. |
| `0x25` | `cmd_room_sprite_set` | `004621d0` | 4 | `[op, pad][sprId, disable]`. `disable == 0` → `RoomSpr_SetActive(sprId)`, else `RoomSpr_SetInactive(sprId)`. |
| `0x26` | `cmd_dead_slot_hang_26` | `00460ce0` | 0 | **Dead slot.** Bare `ret`; returns the opcode value left in `EAX` and consumes nothing, so the interpreter spins forever. See §6. |
| `0x27` | `cmd_snd_fade_set` | `00460cf0` | 2 | `[op, fadeType]`. `BuildSndFadeTbl(fadeType, 0x7F)`. |
| `0x28` | `cmd_enemy_prop_set` | `004312f0` | 4/6/8 | `[op,pad][enemyIdx, subCmd][param:u16]...`. `subCmd`: 0 `behavior_flags`, 1 `state=2` + `health` + `hit_state` (8 bytes), 2 `action_behavior`, 3 `status_flags` SET/OR/XOR, 5 yaw, 6 `blend_counter=0` (4 bytes), 8 `state=9` (4 bytes), 9 toggle joint flags by bitmask, 10 `action_state`. |
| `0x29` | `cmd_fmv_set` | `00461a40` | 2 | `[op, fmvId]`. `g_main_state_flags |= 0x40000`. |
| `0x2A` | `cmd_effect_spawn` | `004316c0` | 12 | `[op, type][parentIdx, parentType][x:s16][y:s16][z:s16][flags:u16]`. `parentType` 0 = identity matrix, 1 = player matrix, `2..0x7F` effect-pool, `0x80..0xFF` omodel `g_omodel_table[type & 0x7F]` (the `0x8000` test is on the parent word). Calls `Effect_CreateBillboard`. |
| `0x2B` | `cmd_attack_anim_set` | `00431990` | 4 | `[behavior:s16][animParam:u16]`. Sets `attackAnim`, `action_behavior/state` from `(val + 0x200) & 0xFF00`, `animation_frame_id`, `animationId = 8`. |
| `0x2C` | `cmd_item_remove` | `004319e0` | 2 | **Cond.** `[op, itemId]`. Zeroes the slot and calls `rearrange_item_slots`. Returns 0 if the item was absent. |
| `0x2D` | `cmd_got_item` | `00431a20` | 0 | Calls `cmd_room_action` (**the same function as opcode `0x24`**), sets `g_main_state_flags |= 0x400` and toggles `0x800`, returns 0. Consumes the operand bytes via the nested `cmd_room_action`. |
| `0x2E` | `cmd_dead_slot_hang_2e` | `00460a70` | 0 | **Dead slot.** Bare `ret`, identical to `0x26`. |
| `0x2F` | `cmd_snd_pan_vol_set` | `00460c00` | 4 | `[op, ch][paramA, paramB]`. `FUN_004805d0`, then stores `paramA`/`paramB` at byte offset `ch*8` in `DAT_00ac98e0` / `DAT_00ac98e4`. |
| `0x30` | `cmd_boundary_set` | `00431a40` | 12 | `[op, listIdx][boundIdx, flagMode][z:u16][w:u16][x:u16][y:u16]`. Boundary record = `boundaries[listIdx][boundIdx]` (stride `0xC`). `flagMode != 0` rewrites bits `0x0F00` of `boundary[5]`. |
| `0x31` | `cmd_state_word_set` | `004608d0` | 4 | `[op, fieldIdx][value:u16]`. `*(u16*)((u8*)&g_fading_state + fieldIdx*2) = value`. |
| `0x32` | `cmd_skip_4bytes` | `00431b00` | 4 | No-op that advances. |
| `0x33` | `cmd_player_prop_set` | `004314b0` | 2/4 | `[op, subCmd][param:u16]`. `subCmd`: 0 unequip (2 bytes), 1 set `isBeingAttackedFlag` + reset anim, 3 `flags` SET/OR/XOR, 4 `action_behavior=1, action_state=6` (2 bytes), 5 `directionAngle`, 6 clear `unk_8c` (2 bytes), 7 reset to idle (2 bytes), 8 `healthStatusFlags` SET/OR/XOR, 9 toggle joint flags, 10 set/clear `unk_e0 & 0x40`. |
| `0x34` | `cmd_model_tint_set` | `00431b10` | 8 | `[op][variant][bias][p3][p4][p5][p6][p7]`, all bytes; `bias = byte - 0x80`. `variant` 0 → `FUN_00473b10`, 1 → `FUN_00473d10`, 2 → `FUN_00473d60`. |
| `0x35` | `cmd_obj_flag_set` | `00431bf0` | 4 | `[op, table][objIdx, value]`. `table` 0 = `g_omodel_table`, 1 = `g_item_model_table`; writes byte `[0]`. Special-cases stage 3 / room 13 / object 5 → force 0. |
| `0x36` | `cmd_obj_field_test` | `00431c90` | 4 | **Cond.** `[op, objIdx][mode, cmpVal]`. Compares the `u16` at `g_omodel_table[objIdx] + 0x86` (the billboard effect handle). |
| `0x37` | `cmd_room_bgm_state_set` | `00460a30` | 4 | `[op, stage][roomIdx, value]`. `g_roomBgmState[stage*32 + roomIdx] = value`. |
| `0x38` | `cmd_dpad_test` | `00431dc0` | 4 | **Cond.** `[op, invert][mask:u16]`. Tests `g_PlayerDpadHeld & mask`; `invert != 0` negates. |
| `0x39` | `cmd_enemy_flags_get` | `00431e10` | 2 | `[op, enemyIdx]`. `g_scdLastEnemyFlags = g_EnemiesList[enemyIdx].behavior_flags`. |
| `0x3A` | `cmd_cut_zone_set` | `00431e50` | 4 | `[op, zoneIdx][toCam, fromCam]`. Rewrites `cam_switch_zones[zoneIdx]` fields at `+2` and `+0`. |
| `0x3B` | `cmd_obj_rotation_set` | `00431ea0` | 6 | `[op, sel][a:u16][b:u16]`. `sel < 0x8000` → item model `(sel >> 6) & 0x3F`, else omodel `(sel & 0x7F00) >> 8`. Writes `+0x72` and `+0x76` only when the object is active. |
| `0x3C` | `cmd_player_dist_test` | `00431f20` | 6 | **Cond.** `[op, pad][targetSpec:u16][maxDist:u16]`. `targetSpec & 0xFF`: 0 = enemy `spec >> 8`, 1 = omodel (`g_omodel_table`), 2 = item model. Returns `SquareRoot0(dx²+dz²) <= maxDist` against the player. |
| `0x3D` | `cmd_bullet_effect_spawn` | `00431770` | 12 | Same layout as `0x2A`. Additionally stashes the type in `g_bulletEffectId` and the matrix in `DAT_00bf0a34` for `0x3E`. |
| `0x3E` | `cmd_bullet_effect_clear` | `00431840` | 2 | `FUN_0047cf80(9, g_bulletEffectId, 0, 0, DAT_00bf0a34)` — frees every effect-pool slot whose type **and** sprite matrix match the values stashed by `cmd_bullet_effect_spawn` (`FUN_0047cf80` is a criteria-masked slot remover, not a spawner). Pairs with `0x3D`: spawn impact billboards now, sweep them later. |
| `0x3F` | `cmd_player_dir_test` | `00431fd0` | 6 | **Cond.** `[op,pad][minAngle:u16][maxAngle:u16]`. Wrap-aware range test on `directionAngle`. |
| `0x40` | `cmd_light_param_set` | `00432010` | 16 | `[op, lightIdx]` + seven `s16`. Writes light fields `[0..5]` and `[8]` at `&g_RdtPointer[1].lights + lightIdx*0x2C - 4`. |
| `0x41` | `cmd_entity_posy_set` | `00432090` | 4 | `[op, entIdx][value:u16]`. `entIdx == 0` → `g_playerEntity.posY`, else entity `entIdx` at `+0x82`. |
| `0x42` | `cmd_effect_clear_typed` | `00431870` | 4 | `[op, type][param:u16]`. `FUN_0047cf80(3, type, param, 0, 0)`. |
| `0x43` | `cmd_bgm_volume_ramp` | `00460d20` | 4 | `[op, ch][a, b]`. Only acts when `g_BGM_STATE` bit `ch+3` is set. |
| `0x44` | `cmd_scd_event_kill` | `00461080` | 2 | `[op, slot]`. `g_ScdEventTable[slot].active = 0`. |
| `0x45` | `cmd_player_posy_add` | `004320f0` | 2 | `[op, delta]`. `g_playerEntity.posY += (s8)delta`. |
| `0x46` | `cmd_room_lights_set` | `00432110` | 44 | `[op,pad]` then 3 × 12-byte light records `[x:s16][y:s16][z:s16][r][g][b][zero2:u8][radius:s16]` written into `RDT.lights[0..2]` (`+0x00/04/08` as ints, `+0x0C/0D/0E` bytes, word at `+0x10`, `radius` at `+0x12`), then 3 × `s16` into `RDT+6/8/10`. Ends with `setBackColor(RDT+6, RDT+8, RDT+10)`. |
| `0x47` | `cmd_obj_transform_set` | `00431080` | 14 | `[op, objIdx]` + six `s16`: rotation `+0x72/+0x74/+0x76` and position `+0x6C/+0x6E/+0x70` (mirrored into `+0x34/+0x38/+0x3C`) of `g_omodel_table[objIdx]`. |
| `0x48` | `cmd_effect_pool_clear` | `004318a0` | 2 | Clears `animId`/`updateId` on all 64 effect-pool slots. |
| `0x49` | `cmd_room_sprite_hide` | `00432290` | 2 | `[op, bit]`. `bit == 0xFF` clears `DAT_00d22770`, else sets bit `bit & 0x1F`. |
| `0x4A` | `cmd_bgm_restore` | `00460ae0` | 2 | No-op unless `g_targetBgmState != 0xFF`. Restores the three sound channels saved by `0x4B`: `g_BGM_STATE >>= 8`, then `SetSndSlot` per set bit (`0x08` / `0x10` / `0x20`). |
| `0x4B` | `cmd_bgm_stop_all` | `00460b80` | 2 | No-op unless `g_targetBgmState != 0xFF`. Stops all four sound banks, then `g_BGM_STATE <<= 8` to save the live channel mask into the high byte. |
| `0x4C` | `cmd_item_record_transfer` | `004322d0` | 4 | `[op, mode][slotIdx, fieldIdx]`. 0 = event-entry byte → `(&g_stageId)[fieldIdx]`; 1 = the reverse; 2 = look the entry's item up in the inventory and copy the quantity into both. |
| `0x4D` | `cmd_player_joint_tint` | `004323a0` | 2 | Applies a joint colour tint (second `JointApplyColorTint` argument, `0x30`) to joints 0-13 of the pointer at entity `+0x98` (`jointsStructs`, stride `0x7c`). The `0x00606060` push is a dead argument. |
| `0x4E` | `cmd_effect_flags_modify` | `00431910` | 4 | `[op, mode][mask:u16]`. OR / AND-NOT / XOR `mask` into `+2` of every live effect-pool slot. |
| `0x4F` | `cmd_costume_variant_set` | `004622b0` | 2 | `[op, param]`. `FUN_0040c560(param)` — stores `param & 1` into `g_bCostumeVariant`. |
| `0x50` | `cmd_costume_variant_test` | `004622e0` | 2 | **Cond.** Returns `g_bCostumeVariant`. |

`g_bCostumeVariant` (`0x004d6444`) is the costume-variant selector: opcode `0x4F`
writes `param & 1`, and `LoadEntityEMD` adds it to `0x33` when the costume-swap
flag (`g_main_state_flags2` bit `0x4000000`) is active, so player EMD ids 0/1
resolve to the alternate-outfit models (`em1030.emd` / `em1032.emd`). The byte
is persisted in the save block at offset `0xA01`.

### `room_check_actions` (`0x004b9340`)

18 entries, indices `0x00`–`0x11`, followed by two NULL slots. Each takes a
pointer to a 12-byte `g_RoomActionTable` entry. Port names (defined in
[RoomEvents.cpp](../src/game/RoomEvents.cpp)):

```
0x00 0041c050 no_room_action          0x01 0041b400 door_try_enter
0x02 0041b630 display_msg_room_action 0x03 0041b650 include_key
0x04 0041b6a0 set_key_flag            0x05 0041b6d0 check_door
0x06 0041b790 check_door_side         0x07 0041b850 flag_bank_set
0x08 0041b990 open_itembox            0x09 0041b9e0 create_room_event
0x0A 0041ba00 room_action_noop10      0x0B 0041ba10 room_action_effect
0x0C 0041baa0 set_stairs_zone         0x0D 0041bae0 set_room_event_flag
0x0E 0041bb10 check_desk              0x0F 0041be70 pickup_key_event
0x10 0041bed0 check_typewriter        0x11 0041bf90 stairs_height_update
```

### Room action entry (12 bytes, `g_RoomActionTable` at `0x00d91aa0`, 24 slots)

```
+0x00  u8   room_check_actions handler index (0 = dead)
+0x01  u8   probe flags (see below)
+0x02  u16  id or flag word
+0x04  u16  parameter
+0x06  u16  flag bit index
+0x08  u32  pointer to the originating SCD record (opcodes + 2)
```

**Probe flags (byte `+0x01`).** The low three bits are a participation mask, not
an enable bit. `update_player_position` (`0x0041c060`) takes a `mask` argument and
skips any entry where `(mask & flags) == 0`, so each bit belongs to one prober:

| Bit | Prober | Passes |
|---|---|---|
| `0x01` | `game_loop` (`0x00480ebd`) | the player walking |
| `0x02` | `tyrant_update` | the Tyrant walking |
| `0x04` | `update_room_objects` (`RoomCollision.cpp`) | a pushed object entering a zone |

`0x40` switches the hit test from the 600-unit forward reach point to the
prober entity's own position. `0x80` removes the entry from every per-frame pass
and hands it to `check_action_object` instead, which fires on the action key and
requires `0x01` and `0x80` both set.

That is what the odd shipped values mean: `0x44` is an own-position zone only a
pushed object can trip, `0x42` one only the Tyrant can, and `0x45` one both the
player and a pushed object can.

`g_RoomActionTail` (`0x00d91bc0`) tracks the highest entry written so far,
inclusive: the setup commands only raise it and the probe loops run while
`entry <= g_RoomActionTail`.

---

## 5. `g_ScdOpcodes` pointer-width hazard

In the original binary Ghidra types `g_ScdOpcodes` as `ushort*` in most command
functions and as `byte*`/`short*`/`uint*` in others, per function. That matters
for two things:

- `g_ScdOpcodes + 1` means **+2 bytes** when the local type is `ushort*`
  (and +4 for `uint*`, +2 for `short*`).
- `*g_ScdOpcodes` reads a **16-bit word**, so `*g_ScdOpcodes >> 8` is the second
  byte of the instruction and `*g_ScdOpcodes & 0xFF00` is a real test.

The decomp declares a single `unsigned char* g_ScdOpcodes`. Each ported function
must therefore convert `p++` → `p += 2` (or `+= 4`) and `*p` →
`*(unsigned short*)p` to match. Where that conversion is missed the command
reads the wrong operand or advances by half the instruction length,
desynchronising the whole rest of the block.

### Ghidra index idiom

Ghidra renders "extract the high byte and scale it" as a masked shift. Read it as:

```
(word & ~((1 << N) - 1 ... )) >> N   ==   (word >> 8) << (8 - N)
```

| Ghidra form | Meaning | Used by |
|---|---|---|
| `(x & 0xffffff07) >> 3` | `(x >> 8) * 32` — byte offset | `cmd_room_bgm_state_set` (`g_roomBgmState`, 32 rooms/stage) |
| `(x & 0xffffff1f) >> 5` | `(x >> 8) * 8` — byte offset | `cmd_bgm_play`, `cmd_bgm_stop`, `cmd_snd_pan_vol_set` (8-byte sound channel records) |
| `(x >> 6) & 0xfffffffc` | `(x >> 8) * 4` — byte offset | `cmd_obj_field_test`, `cmd_player_dist_test`, `cmd_boundary_set` (pointer tables) |
| `(x >> 7) & 0xfffffffe` | `(x >> 8) * 2` — byte offset | `cmd_state_word_set` (`u16` array) |

These are **byte offsets added to a base address**, not element indices. Writing
them as `array[idx]` in C double-scales the offset.

---

## 6. Interpreter return-value semantics

The dispatcher loop is `do { result = table[*g_ScdOpcodes](); } while (result != 0)`,
and the call site leaves the opcode byte in `EAX` (`XOR EAX,EAX / MOV AL,[ECX] /
CALL [EAX*4 + 0x4c1110`). Two consequences every handler inherits:

- A handler that falls through without writing `EAX` returns its **own opcode
  number** - non-zero, i.e. "continue".
- Opcodes `0x26` and `0x2E` (`cmd_dead_slot_hang_26/2e`) are bare `RET`s in the
  original: they write no `EAX`, consume no operand bytes, and therefore spin
  the interpreter forever. No shipped script contains them. Reproduce as-is;
  returning 0 would silently end the block instead (divergence). By contrast
  `cmd_block_end` (0x00) explicitly clears `EAX`, which is what makes it a real block
  terminator.