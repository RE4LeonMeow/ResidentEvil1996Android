#!/usr/bin/env python3
"""
evt_disasm.py - disassemble an RDT's SCD *event* scripts (the cutscene VM).

This is NOT the SCD command stream that mine_room_scd.py decodes. The event VM is
the outer state machine driven by room_events_check (0x0041d6a0): a table of
scripts at RDT+0x68, each a mix of control-flow opcodes (0xF6-0xFF) and
state-specific opcodes dispatched through the entry's `state` field.

  state 0 - sequencing: pick an entity, spawn events, run SCD commands
  state 1 - animation:  opcodes 0x80-0x8B, set poses / look-at / wait
  state 2 - movement:   opcodes 0x00-0x0B, per-frame position and rotation
  state 3 - behavior:   one byte, sets action_behavior

Widths come from the port's RoomEvents.cpp, which was verified instruction-for-
instruction against the original. The walk tracks `state` as it goes, because the
same byte means different things in different states.

Usage:
  evt_disasm.py <room.rdt>              # all scripts
  evt_disasm.py <room.rdt> -s 3         # just script 3
"""
import argparse
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    from mine_room_scd import CMDS as SCD_CMDS
except Exception:
    SCD_CMDS = {}

EVT_TABLE_OFF = 0x68   # RDT header slot holding the event-script table


# ---------------------------------------------------------------- control flow
# (name, width, yields_frame, comment)
CTRL = {
    0xF6: ('push_wait',  1, True,  'depth++ then fall into wait'),
    0xF7: ('wait',       1, True,  'block while msf bit9 or menu active'),
    0xF8: ('push_count', 4, True,  'depth++, counter = i16 @+2, fall into dec'),
    0xF9: ('dec_count',  3, True,  'counter--; when 0 advance 3 and pop'),
    0xFA: ('loop_begin', 4, False, 'depth++, counter = i16 @+2, mark resume'),
    0xFB: ('loop_end',   1, False, 'counter--; branch back unless 0'),
    0xFC: ('call',       0, False, 'depth++, retaddr = +2, jump += byte@+1'),
    0xFD: ('call_cmd',   1, False, 'run SCD cmd at callStack top; loop while !=0'),
    0xFE: ('nop_yield',  1, True,  'advance 1, yield the frame'),
    0xFF: ('end',        1, True,  'deactivate this event slot'),
}

# ------------------------------------------------------------------- state 0
S0 = {
    0x00: ('nop',            1, ''),
    0x01: ('enter_state1',   1, 'animation state'),
    0x02: ('enter_state2_i', 1, 'movement state + reset entity action'),
    0x03: ('enter_state2',   1, 'movement state'),
    0x04: ('set_entity',     3, '0=player 1=enemy 2=itembox 3=interactable model, idx @+2'),
    0x05: ('event_create',   4, 'slot @+1, script @+2'),
    0x06: ('run_scd',        0, 'inline SCD block, length = byte @+1'),
    0x07: ('exec_scd',       0, 'one SCD command, length = byte @+1'),
    0x08: ('event_reinit',   0, 're-init this slot from script byte @+1'),
    0x09: ('event_kill',     2, 'deactivate slot @+1'),
}

# ------------------------------------------------------------------- state 1
S1 = {
    0x00: ('s1_nop',         1, ''),
    0x80: ('s1_end_clear',   1, 'clear ignore_player, back to state 0'),
    0x81: ('s1_lookat',     10, 'lookAtFlags @+1; 2 if low nibble == 0'),
    0x82: ('s1_lookat_off',  1, 'clear lookAtFlags bit 0x10'),
    0x83: ('s1_pose_full',   7, 'state=8, behavior @+1, then c6/c8/anim param'),
    0x84: ('s1_pose_anim',   4, 'state=8, behavior=1, animationId @+1'),
    0x85: ('s1_pose_beh',    4, 'state=8, behavior @+1, animationId @+2'),
    0x86: ('s1_idle',        1, 'state=1, behavior=0, clear hit_state'),
    0x87: ('s1_flags',       4, '0=OR 1=SET 2=XOR on scd_entity_flags'),
    0x88: ('s1_timer',       4, 'scd_timer = u16 @+2'),
    0x89: ('s1_frame_remap', 2, 'animation_frame_id @+1, remap via g_ScdAnimRemap'),
    0x8A: ('s1_frame',       2, 'animation_frame_id @+1, no remap'),
    0x8B: ('s1_end',         1, 'back to state 0'),
}

# ------------------------------------------------------------------- state 2
S2 = {
    0x00: ('s2_nop',        1, ''),
    0x01: ('s2_end',        1, 'back to state 0'),
    0x02: ('s2_step_pos',   1, 'position += speed'),
    0x03: ('s2_step_rot',   1, 'rotation += move_step'),
    0x04: ('s2_step_both',  1, 'position += speed, rotation += move_step'),
    0x05: ('s2_set_speed',  4, 'speed.x/y/z = i8 @+1..+3'),
    0x06: ('s2_set_step',   4, 'move_step_x/z and state word @+1..+3'),
    0x07: ('s2_set_pos',    8, 'localMatrix.t = i16 @+2,+4,+6'),
    0x08: ('s2_set_byte',   3, 'entity[0x84 + byte@+1] = byte@+2'),
    0x09: ('s2_hit_state',  2, 'hit_state = byte @+1'),
    0x0A: ('s2_set_field',  4, 'sel @+1: 0..2 pos, 3 health, 4 unk_c6, 5 unk_c8'),
    0x0B: ('s2_set_rot',    8, 'rotation = u16 @+2,+4,+6'),
}


def u16(b, o):
    return struct.unpack_from('<H', b, o)[0]


def i16(b, o):
    return struct.unpack_from('<h', b, o)[0]


def decode_scd(body):
    """Decode ONE embedded SCD command (the 0x07 exec_scd payload)."""
    if not body:
        return ''
    op = body[0]
    name = SCD_CMDS[op][0] if op in SCD_CMDS else f'cmd_{op:02x}?'
    args = ' '.join(f'{x:02X}' for x in body[1:])
    return f'{name} [{args}]' if args else name


def decode_scd_block(body):
    """Decode a run_scd (event opcode 0x06) payload.

    run_command_functions is handed `scriptPtr + 2`, and it reads a u16 BLOCK SIZE
    from there before stepping to the first command at +2. So the first two bytes
    of the payload are a length, not an opcode - decoding body[0] as a command is
    how `0C 00 ...` reads as a bogus `door_set`.
    """
    if len(body) < 2:
        return ''
    block = struct.unpack_from('<H', body, 0)[0]
    cmds = []
    off = 2
    end = min(block, len(body))
    while off < end:
        op = body[off]
        if op in SCD_CMDS:
            name, width, _ = SCD_CMDS[op]
        else:
            name, width = f'cmd_{op:02x}?', 0
        if width <= 0 and op != 0x00:
            cmds.append(f'{name}(?)')
            break
        args = body[off + 1: off + 1 + width]
        if args:
            cmds.append(f'{name}[{" ".join(f"{x:02X}" for x in args)}]')
        elif name != 'nop':
            cmds.append(name)
        off += 1 + width
    return f'blk={block:#x}: ' + '; '.join(cmds)


def walk(data, base, start, limit, want_state=0):
    """Linear walk from `start`, tracking the VM state as transitions happen."""
    out = []
    off = start
    state = want_state
    seen = set()
    steps = 0

    while 0 <= off < limit and steps < 4000:
        steps += 1
        if off in seen:
            out.append((off, state, '...', 'already walked (loop target)', ''))
            break
        seen.add(off)

        op = data[off]
        rel = off - base

        # control flow is tested BEFORE the state dispatch, exactly as the VM does
        if op in CTRL:
            name, width, _yield, note = CTRL[op]
            if op == 0xFC:
                width = data[off + 1] if off + 1 < limit else 0
                note = f'jump +{width:#x} (retaddr +2)'
            elif op == 0xF8:
                note = f'counter = {i16(data, off + 2)}'
            elif op == 0xFA:
                note = f'counter = {i16(data, off + 2)}'
            out.append((rel, state, name,
                        ' '.join(f'{x:02X}' for x in data[off:off + max(width, 1)]), note))
            if op == 0xFF:
                break
            if width <= 0:
                break
            off += width
            continue

        table = {0: S0, 1: S1, 2: S2}.get(state)
        if table is None:      # state 3 - one byte operand, always 2 wide
            out.append((rel, state, 's3_behavior',
                        ' '.join(f'{x:02X}' for x in data[off:off + 2]),
                        f'action_behavior = {data[off + 1]:#x}'))
            off += 2
            state = 3
            continue

        if op not in table:
            out.append((rel, state, f'??? op {op:#04x}',
                        ' '.join(f'{x:02X}' for x in data[off:off + 4]),
                        'UNKNOWN for this state - VM would deactivate the slot'))
            break

        name, width, note = table[op]

        # variable-width and state-changing cases
        if state == 0 and op == 0x06:
            width = data[off + 1]
            payload = data[off + 2: off + width] if width > 2 else b''
            note = f'len={width:#x} -> {decode_scd_block(payload)}'
        elif state == 0 and op == 0x07:
            width = data[off + 1]
            payload = data[off + 2: off + width] if width > 2 else b''
            note = f'len={width:#x} -> {decode_scd(payload)}'
        elif state == 0 and op == 0x08:
            width = 2
            note = f're-init from script {data[off + 1]:#x}'
        elif state == 1 and op == 0x81:
            flags = data[off + 1]
            width = 10 if (flags & 0x0F) else 2
            note = f'lookAtFlags={flags:#04x} ' + (
                'target mode' if flags == 0x93 else 'position mode' if flags & 0xF else 'off')
        elif state == 0 and op == 0x05:
            note = f'slot {data[off + 1]:#x} <- script {data[off + 2]:#x}'
        elif state == 0 and op == 0x04:
            kind = {0: 'player', 1: 'enemy', 2: 'itembox', 3: 'model'}.get(data[off + 1], '?')
            note = f'{kind} idx {data[off + 2]:#x}'
        elif state == 1 and op == 0x84:
            # 0x84 forces action_behavior = 1; the byte at +1 IS the animation id.
            # scd_anim_param is the LOW byte of the word at +2 - `(char)uVar5` at
            # 0x0041dd2c - and the byte at +3 is NOT a second parameter: the whole
            # word feeds scd_entity_flags as (word >> 6) & 0x3FC (bit 1 = run the
            # state-8 handler twice, bit 2 = refresh the weapon joint, bit 3 =
            # which hand). Labelling +3 as scd_param was wrong.
            w = u16(data, off + 2)
            note = (f'behavior=1 anim={data[off + 1]:#x} scd_param={w & 0xFF:#x}'
                    f' entity_flags={(w >> 6) & 0x3FC:#06x}')
        elif state == 1 and op == 0x85:
            # 0x85 takes the behaviour from +1 and the animation from +2 - the
            # byte at +1 is NOT the animation. This is the pair that selects an
            # npc_scd_NN handler, so getting it backwards sends you to the wrong
            # behaviour entirely.
            note = (f'behavior={data[off + 1]:#x} anim={data[off + 2]:#x}'
                    f' scd_param={data[off + 3]:#x}')
        elif state == 1 and op in (0x89, 0x8A):
            note = f'frame {data[off + 1]:#x}'

        out.append((rel, state, name,
                    ' '.join(f'{x:02X}' for x in data[off:off + max(width, 1)]), note))

        # apply the state transition
        if state == 0:
            if op == 0x01:
                state = 1
            elif op in (0x02, 0x03):
                state = 2
        elif state == 1 and op in (0x80, 0x8B):
            state = 0
        elif state == 2 and op == 0x01:
            state = 0

        if width <= 0:
            break
        off += width

    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rdt')
    ap.add_argument('-s', '--script', type=int, default=None)
    args = ap.parse_args()

    data = open(args.rdt, 'rb').read()
    table_off = struct.unpack_from('<I', data, EVT_TABLE_OFF)[0]
    print(f'{os.path.basename(args.rdt)}  event table @ {table_off:#x}')

    # the table is a run of relative dwords terminated by 0
    offs = []
    p = table_off
    while True:
        v = struct.unpack_from('<I', data, p)[0]
        if v == 0:
            break
        offs.append(v)
        p += 4
    print(f'{len(offs)} event scripts\n')

    for idx, rel in enumerate(offs):
        if args.script is not None and idx != args.script:
            continue
        start = table_off + rel
        end = table_off + (offs[idx + 1] if idx + 1 < len(offs) else rel + 0x400)
        print(f'===== script {idx}  (table+{rel:#x} = file {start:#x}) =====')
        for r, st, name, raw, note in walk(data, table_off, start, min(end + 0x200, len(data))):
            print(f'  +{r:05X}  s{st}  {raw:<26} {name:<16} {note}')
        print()


if __name__ == '__main__':
    main()
