#!/usr/bin/env python3
"""
extract_global_messages.py - Extract and decode the global_messages table from RE1 PC binary.
"""

import struct
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from decode_re1 import decode_formatted_text, format_output, IMAGE_BASE_OFFSET

PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
BINARY_PATH = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')

TABLE_GHIDRA_VA = 0x004BFC58
TABLE_COUNT = 64


def ascii_to_str_literal(decoded_text):
    out = []
    for ch in decoded_text:
        if ch == '\u25ba':
            out.append(STR_START)
        elif ch == '[L2]':
            out.append(STR_L2)
        elif ch == '[R2]':
            out.append(STR_R2)
        elif ch == '[L1]':
            out.append(STR_L1)
        elif ch == '[R1]':
            out.append(STR_R1)
        elif ch == '\u25b3':
            out.append(STR_TRI)
        elif ch == '\u25cb':
            out.append(STR_CIR)
        elif ch == '\u00d7':
            out.append(STR_CROSS)
        elif ch == '\u25a1':
            out.append(STR_SQR)
        elif ch == '\u25bc':
            out.append(STR_SEL)
        elif ord(ch) >= 32 and ord(ch) < 127:
            if ch == '"':
                out.append('\\"')
            elif ch == '\\':
                out.append('\\\\')
            else:
                out.append(ch)
        elif ch == '\u201c':
            out.append('\\"')
        elif ch == '\u201d':
            out.append('\\"')
        elif ch == '\u2013':
            out.append('-')
        elif ch == '\u00b7':
            out.append('-')
        elif ch == '\u2026':
            out.append('...')
        elif ch == '\u203d':
            out.append('!?')
        elif ch == '\u00c4' or ch == '\u00e4':
            out.append('Ae' if ch == '\u00c4' else 'ae')
        elif ch == '\u00d6' or ch == '\u00f6':
            out.append('Oe' if ch == '\u00d6' else 'oe')
        elif ch == '\u00dc' or ch == '\u00fc':
            out.append('Ue' if ch == '\u00dc' else 'ue')
        elif ch == '\u00df':
            out.append('ss')
        else:
            out.append('?')
    return ''.join(out)

STR_START  = '\\x02'
STR_L2     = '\\x03'
STR_R2     = '\\x04'
STR_L1     = '\\x05'
STR_R1     = '\\x06'
STR_TRI    = '\\x07'
STR_CIR    = '\\x08'
STR_CROSS  = '\\x09'
STR_SQR    = '\\x0A'
STR_SEL    = '\\x0B'


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    if not os.path.isfile(BINARY_PATH):
        print('Error: binary not found: %s' % BINARY_PATH, file=sys.stderr)
        sys.exit(1)

    file_size = os.path.getsize(BINARY_PATH)

    with open(BINARY_PATH, 'rb') as f:
        table_file_offset = TABLE_GHIDRA_VA - IMAGE_BASE_OFFSET
        f.seek(table_file_offset)
        raw_table = f.read(TABLE_COUNT * 4)
        pointers = struct.unpack('<%dI' % TABLE_COUNT, raw_table)

        print('=' * 80)
        print('global_messages table at Ghidra VA 0x%08X' % TABLE_GHIDRA_VA)
        print('File offset: 0x%08X' % table_file_offset)
        print('Entries: %d' % TABLE_COUNT)
        print('=' * 80)
        print()

        print('%-5s %-12s %s' % ('Index', 'Ghidra VA', 'Decoded Text'))
        print('-' * 80)

        decoded_messages = []

        for i, ptr in enumerate(pointers):
            if ptr == 0:
                decoded_messages.append((i, 0, None, None))
                print('%-5d %-12s (null)' % (i, '0x00000000'))
                continue

            file_off = ptr - IMAGE_BASE_OFFSET
            if file_off < 0 or file_off >= file_size:
                decoded_messages.append((i, ptr, None, None))
                print('%-5d 0x%08X   (out of bounds, file offset 0x%X)' % (i, ptr, file_off))
                continue

            f.seek(file_off)
            data = bytearray()
            for _ in range(512):
                byte_val = f.read(1)
                if not byte_val:
                    break
                b = byte_val[0]
                data.append(b)
                if b in (0x01, 0x07):
                    break

            result = decode_formatted_text(bytes(data))
            decoded = format_output(result)
            decoded_messages.append((i, ptr, decoded, bytes(data)))
            print('%-5d 0x%08X   %s' % (i, ptr, decoded))

        print()
        print('=' * 80)
        print('C++ source code for global_messages[] array')
        print('=' * 80)
        print()
        print('// global_messages - decoded from ResidentEvil.exe')
        print('// Table at Ghidra VA 0x%08X (%d entries)' % (TABLE_GHIDRA_VA, TABLE_COUNT))
        print('unsigned char* global_messages[%d] = {' % TABLE_COUNT)

        for i, ptr, decoded, raw_data in decoded_messages:
            if ptr == 0:
                print('    nullptr,  // [%d]' % i)
            elif decoded is None:
                print('    nullptr,  // [%d] 0x%08X (out of bounds)' % (i, ptr))
            else:
                str_lit = ascii_to_str_literal(decoded)
                if len(str_lit) > 120:
                    print('    // [%d] 0x%08X' % (i, ptr))
                    print('    STR("%s"),' % str_lit)
                else:
                    print('    STR("%s"),  // [%d] 0x%08X' % (str_lit, i, ptr))

        print('};')
        print()

        print('=' * 80)
        print('Raw hex dumps for each non-null entry')
        print('=' * 80)
        print()
        for i, ptr, decoded, raw_data in decoded_messages:
            if raw_data is not None:
                hex_str = ' '.join('%02X' % b for b in raw_data)
                print('[%d] 0x%08X: %s' % (i, ptr, hex_str))


if __name__ == '__main__':
    main()
