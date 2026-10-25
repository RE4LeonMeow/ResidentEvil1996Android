#!/usr/bin/env python3
"""
mine_room_scd.py - dump and decode the per-frame SCD room script from an RDT.

RDT header is 0x94 bytes; scd_opcodes pointer is at offset 0x64 (relative).
Command widths taken from the port's CmdFunctions.cpp. Widths for commands
not yet transcribed are flagged so the stream can be re-synced on 0x0C/0x0D
item/door setup blocks.

Usage: mine_room_scd.py <room.rdt> [--scd2]
"""
import argparse
import struct
import sys

# room_check_actions (0x004b9340) - entry byte 0 selects one of these.
ROOM_ACTIONS = {
    0x00: 'no_room_action',        0x01: 'door_try_enter',
    0x02: 'display_msg_room_action', 0x03: 'include_key',
    0x04: 'set_key_flag',          0x05: 'check_door',
    0x06: 'check_door_side',       0x07: 'flag_bank_set',
    0x08: 'open_itembox',          0x09: 'create_room_event',
    0x0a: 'room_action_noop10',    0x0b: 'room_action_effect',
    0x0c: 'set_stairs_zone',       0x0d: 'set_room_event_flag',
    0x0e: 'check_desk',            0x0f: 'pickup_key_event',
    0x10: 'check_typewriter',      0x11: 'stairs_height_update',
}

# opcode -> (name, width_in_bytes_after_opcode, comment)
CMDS = {
    0x00: ('block_end', 0, ""),
    0x01: ('if', 1, ""),
    0x02: ('else', 1, ""),
    0x03: ('end_if', 1, ""),
    0x04: ('bit_test', 3, ""),
    0x05: ('bit_op', 3, ""),
    0x06: ('state_byte_test', 3, ""),
    0x07: ('state_word_test', 5, ""),
    0x08: ('state_byte_set', 3, ""),
    0x09: ('cut_lock_set', 1, ""),
    0x0a: ('current_cut_set', 1, ""),
    0x0b: ('message_set', 3, ""),
    0x0c: ('door_set', 25, ""),
    0x0d: ('room_action_set', 17, "slot, zone box, handler index, probe flags"),
    0x0e: ('skip_2bytes_opcode', 1, ""),
    0x0f: ('mirror_set', 7, ""),
    0x10: ('used_item_test', 1, ""),
    0x11: ('picked_item_test', 1, ""),
    0x12: ('room_action_reset', 9, "rewrite entry bytes [0..7], keep record ptr"),
    0x13: ('room_action_arm', 3, "slot, handler index, probe flags"),
    0x14: ('scd_event_create', 3, ""),
    0x15: ('bgm_play', 1, ""),
    0x16: ('bgm_stop', 1, ""),
    0x17: ('sfx_3d_play', 9, ""),
    0x18: ('item_model_set', 25, ""),
    0x19: ('model_flag_set', 3, ""),
    0x1a: ('item_search', 1, ""),
    0x1b: ('enemy_set', 21, "22 bytes total - cmd_enemy_set adds 0x16"),
    0x1c: ('room_light_fade_set', 5, ""),
    0x1d: ('equipped_item_test', 1, ""),
    0x1e: ('voice_play', 3, ""),
    0x1f: ('omodel_set', 3, ""),
    0x20: ('player_pos_set', 13, ""),
    0x21: ('enemy_pos_set', 13, ""),
    0x22: ('item_count_test', 3, ""),
    0x23: ('cut_lock_write', 1, ""),
    0x24: ('room_action', 3, ""),
    0x25: ('room_sprite_set', 3, ""),
    0x26: ('dead_slot_hang_26', 0, ""),
    0x27: ('snd_fade_set', 1, ""),
    0x28: ('enemy_prop_set', 5, ""),
    0x29: ('fmv_set', 1, ""),
    0x2a: ('effect_spawn', 11, ""),
    0x2b: ('attack_anim_set', 3, ""),
    0x2c: ('item_remove', 1, ""),
    0x2d: ('got_item', 1, ""),
    0x2e: ('dead_slot_hang_2e', 0, ""),
    0x2f: ('snd_pan_vol_set', 3, ""),
    0x30: ('boundary_set', 11, ""),
    0x31: ('state_word_set', 3, ""),
    0x32: ('skip_4bytes', 3, ""),
    0x33: ('player_prop_set', 1, ""),
    0x34: ('model_tint_set', 1, ""),
    0x35: ('obj_flag_set', 3, ""),
    0x36: ('obj_field_test', 3, ""),
    0x37: ('room_bgm_state_set', 3, ""),
    0x38: ('dpad_test', 3, ""),
    0x39: ('enemy_flags_get', 1, ""),
    0x3a: ('cut_zone_set', 3, ""),
    0x3b: ('obj_rotation_set', 5, ""),
    0x3c: ('player_dist_test', 5, ""),
    0x3d: ('bullet_effect_spawn', 11, ""),
    0x3e: ('bullet_effect_clear', 1, ""),
    0x3f: ('player_dir_test', 5, ""),
    0x40: ('light_param_set', 15, ""),
    0x41: ('entity_posy_set', 3, ""),
    0x42: ('effect_clear_typed', 3, ""),
    0x43: ('bgm_volume_ramp', 3, ""),
    0x44: ('scd_event_kill', 1, ""),
    0x45: ('player_posy_add', 1, ""),
    0x46: ('room_lights_set', 1, ""),
    0x47: ('obj_transform_set', 13, ""),
    0x48: ('effect_pool_clear', 1, ""),
    0x49: ('room_sprite_hide', 1, ""),
    0x4a: ('bgm_restore', 1, ""),
    0x4b: ('bgm_stop_all', 1, ""),
    0x4c: ('item_record_transfer', 3, ""),
    0x4d: ('player_joint_tint', 1, ""),
    0x4e: ('effect_flags_modify', 3, ""),
    0x4f: ('costume_variant_set', 1, ""),
    0x50: ('costume_variant_test', 1, ""),
}

# cmd_bit_test / cmd_bit_op bank switch - see docs/SCENARIO_FLAGS.md
BANKS = {0: "ScenarioFlags", 1: "ScenarioFlags2", 2: "LocksFlags", 3: "EnemiesFlags",
         4: "SysFlags", 5: "main_state_flags", 6: "message_flags",
         7: "roomItemsFlags", 8: "RoomFlags", 9: "itemUseFlags"}


def u16(data, off):
    # zero-pad short bodies so detail lines never overrun on odd widths
    if off + 2 > len(data):
        return 0
    return struct.unpack_from("<H", data, off)[0]


def decode(data, off, length, label):
    end = off + length
    print(f"=== {label} @ {off:#x} len={length:#x} ===")
    while off < end:
        op = data[off]
        name, width, comment = CMDS.get(op, (f"UNKNOWN_{op:02X}", None, ""))
        if width is None:
            print(f"  {off:#06x}: ?? {op:02X}  <-- unknown width, stream desync from here")
            return off + 1
        body = data[off + 1:off + 1 + width]
        detail = ""
        if op == 0x04 or op == 0x05:
            # cmd_bit_test (0x00460570) / cmd_bit_op (0x00460650) read through
            # g_ScdOpcodes, which points at the OPCODE - so scd_read_u16(0) >> 8
            # is the byte immediately AFTER it. Per byte that is
            #   body[0] = bank, body[1] = offset+bit, body[2] = cond/operation.
            # Reading the pair one byte later (u16(body, 0) >> 8) reported the
            # wrong bank for every flag test in every room.
            #
            # The bit index is counted from the MSB: cmd_bit_op builds its mask
            # as `0x80000000 >> bitIndex` and cmd_bit_test shifts LEFT by it and
            # tests the sign. So raw index 31 is bit 0, index 0 is bit 31.
            # Printing the raw index as "bit=31" is how ROOM1110's mirror-enable
            # (`05 05 1f 00`, which sets bit 0) got read as a write to bit 31.
            # Report the LSB bit number, with the raw index alongside.
            bank, sel, cond = body[0], body[1], body[2]
            raw = sel & 0x1F
            # Where the bit lands in memory: the dword offset + MSB-first index
            # map to byte[off + 3 - raw/8], bit 7 - raw%8 (little-endian dword),
            # e.g. bank 1 flag 0x40 is byte[11] bit 7 (scenarioFlags2[11]=0x80).
            byte_idx = ((sel & 0xE0) >> 3) + (3 - (raw >> 3))
            bit_in_byte = 7 - (raw & 7)
            opname = {0: 'set', 1: 'clear', 2: 'toggle'}.get(cond, f'op{cond:#x}')
            detail = (f"bank={bank:#x}({BANKS.get(bank, '?')}) "
                      f"off={(sel & 0xE0) >> 3} bit={31 - raw}(msbidx {raw}) "
                      f"mask={0x80000000 >> raw:#010x} "
                      f"mem=byte[{byte_idx}] bit{bit_in_byte}({0x80 >> (raw & 7):#04x}) "
                      + (opname if op == 0x05 else f"cond={cond:#x}"))
        elif op == 0x0B:
            detail = f"msg={u16(body, 0) >> 8:#x} pause={u16(body, 2):#x}"
        elif op == 0x0D:
            slot = body[0]
            # body[1..8] is the zone box the entry+8 record pointer aims at:
            # u16 x, z, width, depth (is_point_in_action_zone, 0x0041b3c0).
            zx, zz, zw, zd = (u16(body, 1), u16(body, 3),
                              u16(body, 5), u16(body, 7))
            action = body[9]          # entry[0] = opcode[10] -> body[9]
            flags = body[10]          # entry[1] = opcode[0xb] -> body[10]
            # cmd_room_action_set (0x00460970) advances 0x12, so the body is 17
            # bytes and holds only THREE u16 after the two flag bytes - entry+2/
            # +4/+6 come from opcode +0xc/+0xe/+0x10. Reading a fourth ran off the
            # end. These are handler PARAMETERS, not the zone.
            p0, p1, p2 = u16(body, 11), u16(body, 13), u16(body, 15)
            detail = (f"slot={slot} {ROOM_ACTIONS.get(action, hex(action))} "
                      f"flags={flags:#x} zone=({zx},{zz}) {zw}x{zd} "
                      f"params=({p0},{p1},{p2})")
            # entry+8 = g_ScdOpcodes + 2, and body[0] sits at off+1, so the
            # record starts at off+2 (this printed off+3 before).
            detail += f" record@{off + 2:#x}"
        elif op == 0x0C:
            door = body[0]
            flags = body[0x18]
            detail = f"door={door} flags={flags:#x}"
        elif op == 0x01:
            # 1-byte body: the jump offset is the byte itself (the u16 read
            # below used to overrun on short bodies)
            detail = f"jump+{body[0]:#x}" if body else "jump?"
        elif op == 0x0F:
            # cmd_mirror_set (0x004610b0) - arms the room mirror.
            # body[0] -> g_main_state_flags bits 0-1, then three u16 params.
            # body[0] goes into g_main_state_flags bits 0-1: bit 0 enables the
            # pass, bit 1 picks the plane axis. A plane-X room writes 0x02 here
            # and then enables with a separate bit_op (bank 5, msb index 31), so
            # bit0=0 on this line does NOT mean the mirror is off - check the
            # following commands.
            axis = "X" if (body[0] >> 1) & 1 else "Z"
            detail = (f"MIRROR bit0={body[0] & 1} axis={axis} "
                      f"extent={u16(body, 1)}..{u16(body, 3)} "
                      f"plane={axis}={u16(body, 5)}")
        elif op == 0x02:
            detail = f"jump+{body[0]:#x}"
        hx = " ".join(f"{b:02X}" for b in body)
        print(f"  {off:#06x}: {op:02X} {name:<16} {hx:<56} {detail} {comment}")
        off += 1 + width
    return off


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rdt")
    ap.add_argument("--scd2", action="store_true", help="dump scd_opcodes2 (events) instead")
    ap.add_argument("--init", action="store_true",
                    help="dump initialization_scd (RDT+0x60, runs once at room load)")
    args = ap.parse_args()

    data = open(args.rdt, "rb").read()
    if len(data) < 0x94:
        print("too small")
        return
    sprites = data[0]
    cams = data[1]
    print(f"sprites={sprites} cameras={cams} size={len(data):#x}")

    if args.init:
        ptr_off = 0x60
    elif args.scd2:
        ptr_off = 0x68
    else:
        ptr_off = 0x64
    rel = u16(data, ptr_off) | (data[ptr_off + 2] << 16) | (data[ptr_off + 3] << 24)
    if rel == 0:
        print("no script pointer")
        return
    print(f"scd ptr offset {ptr_off:#x}: {rel:#x}")

    # A script section ends at the next section, not only at a 0 size word.
    # Every dword in the RDT header (0x08..0x90) is a section pointer, so the
    # smallest one greater than `rel` is this section's hard end. Without that
    # bound the block chain runs off into whatever follows and decodes it as
    # opcodes: ROOM1130's init walked 0x3a bytes past the start of its own main
    # script into a dword offset table and reported a bogus desync.
    limit = len(data)
    for h in range(0x08, 0x94, 4):
        q = struct.unpack_from("<I", data, h)[0]
        if rel < q < limit:
            limit = q
    if limit < len(data):
        print(f"section ends at {limit:#x} (next section pointer)")

    # scd section: blocks with 16-bit size prefix, terminated by 0
    off = rel
    block_no = 0
    while off + 2 <= limit:
        size = u16(data, off)
        if size == 0:
            print(f"=== end of script (0 terminator at {off:#x}) ===")
            break
        if off + 2 + size > limit:
            print(f"=== block {block_no + 1} @ {off:#x} claims {size:#x} bytes, "
                  f"past the section end - stopping ===")
            break
        block_no += 1
        off = decode(data, off + 2, size, f"block {block_no}")
        if off >= len(data):
            break


if __name__ == "__main__":
    main()