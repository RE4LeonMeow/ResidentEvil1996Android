#!/usr/bin/env python3
"""
decode_re1.py - Resident Evil 1 (PC) Custom Text Encoding Decoder

Reads and decodes PrintFormattedText / PrintText8x14 encoded strings from
the original game binary. The 8x14 game text font (fontus.tim region 2)
uses a custom 18-column encoding where byte values index into an 18x8 grid.

Memory mapping:
    Ghidra VA = file_offset + 0x00400C00
    So file_offset = Ghidra_VA - 0x00400C00

Usage:
    decode_re1.py <address> [--length N] [--raw]
    decode_re1.py --hex <hex_bytes> [--raw]

Examples:
    decode_re1.py 0x004d40f8                  # s_ChrisName -> CHRIS
    decode_re1.py 0x004d40f8 --length 64      # read 64 bytes
    decode_re1.py 0x004d40f8 --raw            # show detailed decode
    decode_re1.py --hex "1F FB 24 FB 2E FB 25 FB 2F FB 01"
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
BINARY_PATH = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')
IMAGE_BASE_OFFSET = 0x00400C00

# ============================================================================
# RE1 8x14 Font Encoding Table (18 columns x 8 rows = 144 glyphs)
# ============================================================================
# Byte values 0-143 index directly: row = byte/18, col = byte%18
# Rendered as: texU = (byte % 18) * 8,  texV = (byte / 18 + 2) * 14

FONT_8x14 = [
    # Row 0 (0-17)
    ' ', ' ', '\u25ba', '[L2]', '[R2]', '[L1]', '[R1]',
    '[\u25b3]', '[\u25cb]', '[\u00d7]', '[\u25a1]', '\u25bc',
    '0', '1', '2', '3', '4', '5',
    # Row 1 (18-35)
    '6', '7', '8', '9', ':', ';', ',', '\u201c', '!', '?', '\u203d',
    'A', 'B', 'C', 'D', 'E', 'F', 'G',
    # Row 2 (36-53)
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
    'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
    # Row 3 (54-71)
    'Z', '(', '\\', ')', "'", '\u2013', '\u00b7',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    # Row 4 (72-89)
    'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u',
    'v', 'w', 'x', 'y', 'z', '\u00c4', '\u00e4', '\u00d6',
    # Row 5 (90-107)
    '\u00f6', '\u00dc', '\u00fc', '\u00df',
    '\u00c0', '\u00e0', '\u00c2', '\u00e2',
    '\u00c8', '\u00e8', '\u00c9', '\u00e9',
    '\u00ca', '\u00ea', '\u00cf', '\u00ef',
    '\u00ce', '\u00ee',
    # Row 6 (108-125)
    '\u00d4', '\u00f4', '\u00d9', '\u00f9',
    '\u00db', '\u00fb', '\u00c7', '\u00e7',
    'S.', 'T.', 'A.', 'R.',
    '\u201c', '.', '\u2026', '\u2013', '\u2013', '+',
    # Row 7 (126-143)
    '=', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
]

FONT_14x14 = [
    '[L2]', '[R2]', '[L1]', '[R1]',
    '[\u25b3]', '[\u25cb]', '[\u00d7]', '[\u25a1]',
    '[Up]', '[Right]', '[Down]', '[Left]',
]

# PrintFormattedText control codes
CTRL_END_VISIBLE = 0x00
CTRL_END_BLOCK_1 = 0x01
CTRL_END_BLOCK_2 = 0x07
CTRL_SYMBOL_14   = 0xF8
CTRL_CHAR_D31    = 0xF9
CTRL_CHAR_D31R   = 0xFA
CTRL_NOP         = 0xFB
CTRL_HALF_SPACE  = 0xFF


def decode_byte(b):
    if 0 <= b < len(FONT_8x14):
        return FONT_8x14[b]
    return '[?%02X]' % b


def decode_formatted_text(data, raw=False):
    result = []
    i = 0
    while i < len(data):
        b = data[i]

        if b == CTRL_END_BLOCK_1:
            result.append(('[END]', 'End text block (0x01)', False))
            break
        elif b == CTRL_END_BLOCK_2:
            result.append(('[END]', 'End text block (0x07)', False))
            break
        elif b == CTRL_END_VISIBLE:
            result.append((' ', 'space (0x00)', True))
            i += 1
        elif b == CTRL_SYMBOL_14:
            if i + 1 < len(data):
                idx = data[i + 1]
                col = idx % 18
                ch = FONT_14x14[col] if col < len(FONT_14x14) else '[SYM%d]' % col
                result.append((ch, '14x14 sym idx=%d (F8 %02X)' % (idx, idx), True))
                i += 2
            else:
                result.append(('[?F8]', 'F8 no operand', False))
                i += 1
        elif b == CTRL_CHAR_D31:
            if i + 1 < len(data):
                idx = data[i + 1]
                result.append((decode_byte(idx), 'd31 idx=%d (F9 %02X)' % (idx, idx), True))
                i += 2
            else:
                result.append(('[?F9]', 'F9 no operand', False))
                i += 1
        elif b == CTRL_CHAR_D31R:
            if i + 1 < len(data):
                idx = data[i + 1]
                result.append((decode_byte(idx), 'd31+r idx=%d (FA %02X)' % (idx, idx), True))
                i += 2
            else:
                result.append(('[?FA]', 'FA no operand', False))
                i += 1
        elif b == CTRL_NOP:
            result.append(('', 'FB spacer', False))
            i += 1
        elif b == CTRL_HALF_SPACE:
            result.append(('', 'FF half-space', False))
            i += 1
        else:
            result.append((decode_byte(b), 'idx=%d (%02X)' % (b, b), True))
            i += 1

    return result


def format_output(result, raw=False):
    if raw:
        lines = []
        for ch, desc, visible in result:
            marker = 'v' if visible else '-'
            lines.append('  [%s] %-10s %s' % (marker, repr(ch), desc))
        text = ''.join(ch for ch, _, vis in result if vis)
        return text + '\n' + '\n'.join(lines)
    return ''.join(ch for ch, _, visible in result if visible)


def read_binary_bytes(gidhra_va, length=256):
    if not os.path.isfile(BINARY_PATH):
        print('Error: binary not found: %s' % BINARY_PATH, file=sys.stderr)
        print('Place the original ResidentEvil.exe in the assets/ folder.', file=sys.stderr)
        sys.exit(1)

    file_offset = gidhra_va - IMAGE_BASE_OFFSET
    if file_offset < 0:
        print('Error: address 0x%08X is below image base (file offset would be 0x%X)' % (
            gidhra_va, file_offset), file=sys.stderr)
        sys.exit(1)

    with open(BINARY_PATH, 'rb') as f:
        f.seek(file_offset)
        data = f.read(length)
    return data


def main():
    # Force UTF-8 output on Windows console
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    parser = argparse.ArgumentParser(
        description='Decode RE1 custom text encoding',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('address', nargs='?', help='Ghidra virtual address (hex)')
    parser.add_argument('--hex', '-x', help='Decode raw hex bytes (space-separated)')
    parser.add_argument('--length', '-n', type=int, default=256, help='Max bytes (default: 256)')
    parser.add_argument('--raw', '-r', action='store_true', help='Show detailed decode info')

    args = parser.parse_args()

    if args.hex:
        try:
            data = bytes(int(x, 16) for x in args.hex.split())
        except ValueError:
            print('Error: invalid hex. Use: --hex "1F FB 24"', file=sys.stderr)
            sys.exit(1)
    else:
        if not args.address:
            parser.print_help()
            sys.exit(1)
        try:
            va = int(args.address, 16)
        except ValueError:
            print('Error: invalid address "%s"' % args.address, file=sys.stderr)
            sys.exit(1)
        data = read_binary_bytes(va, args.length)

    if args.raw:
        va = int(args.address, 16) if args.address else 0
        print('Bytes at 0x%08X:' % va)
        for off in range(0, min(len(data), 80), 16):
            hex_part = ' '.join('%02X' % b for b in data[off:off+16])
            print('  %08X  %s' % (va + off, hex_part))
        print()

    result = decode_formatted_text(data, raw=args.raw)
    print(format_output(result, raw=args.raw))


if __name__ == '__main__':
    main()
