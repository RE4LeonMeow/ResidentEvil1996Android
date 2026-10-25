#!/usr/bin/env python3
"""dump_init_scd.py - decode an RDT's initialization SCD (header +0x60).

The init script runs once at room load (RoomInit.cpp: run_command_functions(
g_RoomInitScd)) and is what spawns a room's enemies via enemy_set (0x1B) and
moves them with enemy_pos_set (0x21). mine_room_scd.py decodes the per-frame
script at +0x64 instead, so this fills the gap.

Format (RoomEvents.cpp run_command_functions): a list of blocks, each
`[u16 size][commands of size-2 bytes]`, terminated by a zero u16.

Usage: dump_init_scd.py <room.rdt>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mine_room_scd import CMDS

data = open(sys.argv[1], 'rb').read()
off = struct.unpack_from('<I', data, 0x60)[0]
print('%s init_scd @ 0x%x' % (os.path.basename(sys.argv[1]), off))

p = off
blocks = 0
while p + 2 <= len(data):
    size = struct.unpack_from('<H', data, p)[0]
    if size == 0:
        print('  -- end of script (0x%05x)' % p)
        break
    blocks += 1
    print('block %d @ 0x%05x size=0x%x' % (blocks, p, size))
    q = p + 2
    end = p + size
    while q < end:
        op = data[q]
        name, width, _ = CMDS.get(op, ('??', None, ''))
        if width is None:
            print('  0x%05x: %02x <unknown width - stop>' % (q, op))
            q = end
            break
        body = data[q + 1:q + 1 + width]
        print('  0x%05x: %02x %-18s %s' % (q, op, name, body.hex(' ')))
        q += 1 + width
    p = end
