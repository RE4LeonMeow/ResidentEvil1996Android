#!/usr/bin/env python3
"""
extract_item_descriptions.py - Extract and decode the item description table
(0x004C6160) from the original RE1 PC binary.

The table is a flat array of message pointers indexed by (itemId - 1); it is
read only by set_item_description_message (0x00455730), the message setter the
inventory item viewer uses when the player presses the examine button. Unlike
set_message_display, these texts are NOT in the RDT/global message tables.

Prints C++ source for g_ItemDescriptions[] (see src/Globals.cpp).
"""

import os
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from decode_re1 import decode_formatted_text, format_output, IMAGE_BASE_OFFSET
from extract_global_messages import ascii_to_str_literal

PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
BINARY_PATH = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')

TABLE_GHIDRA_VA = 0x004C6160
# ascii_to_str_literal renders font index 0x02 (the line break) as the escape
# \x02. A C hex escape swallows following hex digits ("be\x02enough" would come
# out as 0x2E), so rewrite it as the STR() encoder's own \n escape.
NEWLINE_ESCAPE = r'\x02'


def read_pointers(data):
    off = TABLE_GHIDRA_VA - IMAGE_BASE_OFFSET
    ptrs = []
    while True:
        value = struct.unpack_from('<I', data, off + len(ptrs) * 4)[0]
        if value == 0:
            break
        ptrs.append(value)
    return ptrs


def read_message(data, ptr):
    off = ptr - IMAGE_BASE_OFFSET
    raw = bytearray()
    while off < len(data) and len(raw) < 512:
        b = data[off]
        raw.append(b)
        off += 1
        if b in (0x01, 0x07):
            break
    return bytes(raw), (data[off] if off < len(data) else 0)


def fix_quotes(literal, raw):
    """The font has two double-quote glyphs: 0x78 opens, 0x19 closes. The
    decoder renders both as the same character, so walk the raw bytes and turn
    the opening ones back into the STR() encoder's \\o escape."""
    quotes = [b for b in raw if b in (0x19, 0x78)]
    if 0x78 not in quotes:
        return literal
    out = []
    i = 0
    k = 0
    while i < len(literal):
        if literal.startswith(r'\"', i):
            out.append(r'\o' if quotes[k] == 0x78 else r'\"')
            k += 1
            i += 2
            continue
        out.append(literal[i])
        i += 1
    return ''.join(out)


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    if not os.path.isfile(BINARY_PATH):
        print('Error: binary not found: %s' % BINARY_PATH, file=sys.stderr)
        sys.exit(1)

    data = open(BINARY_PATH, 'rb').read()
    ptrs = read_pointers(data)

    literals = {}
    for ptr in ptrs:
        raw, after = read_message(data, ptr)
        decoded = format_output(decode_formatted_text(raw))
        # `after` is the byte past the 0x01 terminator: 0 = hold until the
        # player presses a button, which every entry in this table uses.
        literal = ascii_to_str_literal(decoded).replace(NEWLINE_ESCAPE, r'\\n')
        literals[ptr] = (fix_quotes(literal, raw), after)

    order = []
    for ptr in ptrs:
        if ptr not in order:
            order.append(ptr)

    print('// Item description table (0x%08X) - %d entries, index = itemId - 1' % (
        TABLE_GHIDRA_VA, len(ptrs)))
    for i, ptr in enumerate(order):
        text, after = literals[ptr]
        note = '' if after == 0 else '  // NOTE: auto-dismiss operand %02X' % after
        print('static constexpr auto s_idesc%02d = STR("%s");%s' % (i, text, note))
    print()
    print('unsigned char* g_ItemDescriptions[%d] = {' % len(ptrs))
    for idx, ptr in enumerate(ptrs):
        text = literals[ptr][0].replace(r'\\n', ' ')
        print('    (unsigned char*)s_idesc%02d.bytes,  // [0x%02X] item 0x%02X - %s' % (
            order.index(ptr), idx, idx + 1, text))
    print('};')


if __name__ == '__main__':
    main()
