#!/usr/bin/env python3
"""Dump the init-script door_set records (and other room action entries) of an RDT.

Usage: dump_init_doors.py <room.rdt>
"""
import struct
import sys

CMDS = {
    0x00: ('block_end', 0), 0x01: ('if', 1), 0x02: ('else', 1), 0x03: ('end_if', 1),
    0x04: ('bit_test', 3), 0x05: ('bit_op', 3), 0x06: ('state_byte_test', 3),
    0x07: ('state_word_test', 5), 0x08: ('state_byte_set', 3), 0x09: ('cut_lock_set', 1),
    0x0a: ('current_cut_set', 1), 0x0b: ('message_set', 3), 0x0c: ('door_set', 25),
    0x0d: ('room_action_set', 17), 0x0e: ('skip_2bytes_opcode', 1), 0x0f: ('mirror_set', 7),
    0x10: ('used_item_test', 1), 0x11: ('picked_item_test', 1), 0x12: ('room_action_reset', 9),
    0x13: ('room_action_arm', 3), 0x14: ('scd_event_create', 3), 0x15: ('bgm_play', 1),
    0x16: ('bgm_stop', 1), 0x17: ('sfx_3d_play', 9), 0x18: ('item_model_set', 25),
    0x19: ('model_flag_set', 3), 0x1a: ('item_search', 1), 0x1b: ('enemy_set', 21),
    0x1c: ('room_light_fade_set', 5), 0x1d: ('equipped_item_test', 1), 0x1e: ('voice_play', 3),
    0x1f: ('omodel_set', 3), 0x20: ('player_pos_set', 13), 0x21: ('enemy_pos_set', 13),
    0x22: ('item_count_test', 3), 0x23: ('cut_lock_write', 1), 0x24: ('room_action', 3),
    0x25: ('room_sprite_set', 3), 0x26: ('dead_slot_hang_26', 0), 0x27: ('snd_fade_set', 1),
    0x28: ('enemy_prop_set', 5), 0x29: ('fmv_set', 1), 0x2a: ('effect_spawn', 11),
    0x2b: ('attack_anim_set', 3), 0x2c: ('item_remove', 1), 0x2d: ('got_item', 1),
    0x2e: ('dead_slot_hang_2e', 0), 0x2f: ('snd_pan_vol_set', 3), 0x30: ('boundary_set', 11),
    0x31: ('state_word_set', 3), 0x32: ('skip_4bytes', 3), 0x33: ('player_prop_set', 1),
    0x34: ('model_tint_set', 1), 0x35: ('obj_flag_set', 3), 0x36: ('obj_field_test', 3),
    0x37: ('room_bgm_state_set', 3), 0x38: ('dpad_test', 3), 0x39: ('enemy_flags_get', 1),
    0x3a: ('cut_zone_set', 3), 0x3b: ('obj_rotation_set', 5), 0x3c: ('player_dist_test', 5),
    0x3d: ('bullet_effect_spawn', 11), 0x3e: ('bullet_effect_clear', 1),
    0x3f: ('player_dir_test', 5), 0x40: ('light_param_set', 15), 0x41: ('entity_posy_set', 3),
    0x42: ('effect_clear_typed', 3), 0x43: ('bgm_volume_ramp', 3), 0x44: ('scd_event_kill', 1),
    0x45: ('player_posy_add', 1), 0x46: ('room_lights_set', 1), 0x47: ('obj_transform_set', 13),
    0x48: ('effect_pool_clear', 1), 0x49: ('room_sprite_hide', 1), 0x4a: ('bgm_restore', 1),
    0x4b: ('bgm_stop_all', 1), 0x4c: ('item_record_transfer', 3), 0x4d: ('player_joint_tint', 1),
    0x4e: ('effect_flags_modify', 3), 0x4f: ('costume_variant_set', 1),
}


def u16(d, o):
    return struct.unpack_from('<H', d, o)[0]


def s16(d, o):
    return struct.unpack_from('<h', d, o)[0]


def decode(d, start, end, label):
    print('=== %s @ 0x%X (len 0x%X) ===' % (label, start, end - start))
    p = start
    while p < end - 1:
        blk = u16(d, p)
        if blk == 0:
            print('  %06X: block size 0 -> end' % p)
            break
        print('  block @ 0x%X size 0x%X' % (p, blk))
        q = p + 2
        stop = p + blk
        while q < stop:
            op = d[q]
            if op not in CMDS:
                print('    %06X: UNKNOWN opcode %02X' % (q, op))
                return
            name, w = CMDS[op]
            args = d[q + 1:q + 1 + w]
            if op == 0x0C:
                r = q + 2
                print('    %06X: door_set slot=%d zone=(%d,%d %dx%d) dir=%d sfx=%d type=%d cam=%02X lock=%d dest=%02X arrival=(%d,%d,%d) ang=0x%X key=%02X tail=%02X'
                      % (q, d[q + 1], u16(d, r), u16(d, r + 2), u16(d, r + 4), u16(d, r + 6),
                         d[r + 8], d[r + 9], d[r + 10], d[r + 11], d[r + 12], d[r + 13],
                         u16(d, r + 14), s16(d, r + 16), u16(d, r + 18), u16(d, r + 20),
                         d[r + 22], d[r + 23]))
            elif op in (0x0D, 0x12, 0x13):
                print('    %06X: %-18s slot=%d args=%s' % (q, name, d[q + 1], ' '.join('%02X' % b for b in args)))
            else:
                print('    %06X: %-18s %s' % (q, name, ' '.join('%02X' % b for b in args)))
            q += 1 + w
        if q != stop:
            print('    !! stream ended at 0x%X, expected 0x%X' % (q, stop))
        p = stop


def main():
    path = sys.argv[1]
    d = open(path, 'rb').read()
    init = struct.unpack_from('<I', d, 0x60)[0]
    scd = struct.unpack_from('<I', d, 0x64)[0]
    scd2 = struct.unpack_from('<I', d, 0x68)[0]
    print('%s size 0x%X  init=0x%X scd=0x%X scd2=0x%X' % (path, len(d), init, scd, scd2))
    decode(d, init, scd, 'init script')


if __name__ == '__main__':
    main()
