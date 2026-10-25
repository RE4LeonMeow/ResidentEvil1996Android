#!/usr/bin/env python3
"""
extract_string_table.py - Extract ASCII string tables from the RE1 binary.

Reads null-terminated (or custom-terminated) fixed-length string arrays
from the original ResidentEvil.exe at a given Ghidra virtual address.

Memory mapping:
    Ghidra VA = file_offset + 0x00400C00
    So file_offset = Ghidra_VA - 0x00400C00

Usage:
    extract_string_table.py <address> <element_length> [--count N] [--terminator 0x00]
    extract_string_table.py <address> <element_length> --c-array NAME

Examples:
    # List voice filename table (auto-detect count)
    extract_string_table.py 0x004b1aa8 9

    # Generate C array for a voice name subtable
    extract_string_table.py 0x004b1aa8 9 --c-array g_VoiceName_Stage0

    # Generic string table, custom terminator
    extract_string_table.py 0x004d0000 12 --terminator 0xFF
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
BINARY_PATH = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')
IMAGE_BASE_OFFSET = 0x00400C00


def read_binary(file_offset, size):
    with open(BINARY_PATH, 'rb') as f:
        f.seek(file_offset)
        return f.read(size)


def extract_strings(data, element_length, terminator, count):
    strings = []
    for i in range(count):
        offset = i * element_length
        raw = data[offset:offset + element_length]
        # Truncate at terminator
        term_pos = raw.find(terminator)
        if term_pos != -1:
            raw = raw[:term_pos]
        # Decode as ASCII, replacing non-printable chars
        try:
            s = raw.decode('ascii')
        except UnicodeDecodeError:
            s = raw.decode('ascii', errors='replace')
        strings.append(s)
    return strings


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    parser = argparse.ArgumentParser(
        description='Extract ASCII string tables from the RE1 binary',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('address', help='Ghidra virtual address (hex, e.g. 0x004b1aa8)')
    parser.add_argument('element_length', type=int, help='Fixed byte length of each string entry')
    parser.add_argument('--count', '-n', type=int, default=0,
                        help='Number of entries (default: read until end of binary)')
    parser.add_argument('--terminator', '-t', type=str, default='0x00',
                        help='Terminator byte value (hex, default: 0x00)')
    parser.add_argument('--offset', '-o', type=int, default=0,
                        help='Skip first N entries (default: 0)')
    parser.add_argument('--raw', '-r', action='store_true',
                        help='Show raw hex dump alongside extracted strings')
    parser.add_argument('--c-array', '-c', type=str, default=None, metavar='NAME',
                        help='Generate C source code for a 2D char array with the given name')

    args = parser.parse_args()

    # Parse terminator
    try:
        terminator = int(args.terminator, 16)
    except ValueError:
        print('Error: invalid terminator value "%s"' % args.terminator, file=sys.stderr)
        sys.exit(1)

    if terminator < 0 or terminator > 255:
        print('Error: terminator must be a single byte (0x00-0xFF)', file=sys.stderr)
        sys.exit(1)
    terminator = bytes([terminator])

    # Parse address
    try:
        va = int(args.address, 16)
    except ValueError:
        print('Error: invalid address "%s"' % args.address, file=sys.stderr)
        sys.exit(1)

    file_offset = va - IMAGE_BASE_OFFSET
    if file_offset < 0:
        print('Error: address 0x%08X is below image base (file offset would be 0x%X)' % (
            va, file_offset), file=sys.stderr)
        sys.exit(1)

    if not os.path.isfile(BINARY_PATH):
        print('Error: binary not found: %s' % BINARY_PATH, file=sys.stderr)
        print('Place the original ResidentEvil.exe in the assets/ folder.', file=sys.stderr)
        sys.exit(1)

    bin_size = os.path.getsize(BINARY_PATH)

    if args.count > 0:
        count = args.count
        total_size = count * args.element_length
        if file_offset + total_size > bin_size:
            count = (bin_size - file_offset) // args.element_length
            print('Warning: requested data extends past end of file, clamping', file=sys.stderr)
        if count <= 0:
            print('No data to read at offset 0x%X' % file_offset, file=sys.stderr)
            sys.exit(1)
        data = read_binary(file_offset, count * args.element_length)
        strings = extract_strings(data, args.element_length, terminator, count)
    else:
        # Auto-detect: read entries until we hit an empty entry (first byte == terminator)
        strings = []
        max_possible = (bin_size - file_offset) // args.element_length
        for i in range(max_possible):
            raw = read_binary(file_offset + i * args.element_length, args.element_length)
            if len(raw) < args.element_length:
                break
            if raw[0:1] == terminator:
                break
            term_pos = raw.find(terminator)
            if term_pos != -1:
                raw = raw[:term_pos]
            try:
                s = raw.decode('ascii')
            except UnicodeDecodeError:
                s = raw.decode('ascii', errors='replace')
            strings.append(s)
        count = len(strings)
        data = read_binary(file_offset, count * args.element_length)

    skip = args.offset
    if skip > 0:
        strings = strings[skip:]

    if args.c_array:
        # Validate the name is a valid C identifier
        name = args.c_array
        if not name.isidentifier():
            print('Error: "%s" is not a valid C identifier' % name, file=sys.stderr)
            sys.exit(1)
        print('// Auto-generated from ResidentEvil.exe at Ghidra 0x%08X (file offset 0x%X)' % (va, file_offset))
        print('// %d entries, %d bytes each, null-terminated' % (len(strings), args.element_length))
        print('static const char %s[%d][%d] = {' % (name, len(strings), args.element_length))
        for i, s in enumerate(strings):
            escaped = s.encode('ascii', errors='replace').decode('ascii')
            escaped = escaped.replace('\\', '\\\\').replace('"', '\\"')
            trailing = ',' if i < len(strings) - 1 else ' '
            print('    "%s"%s' % (escaped, trailing))
        print('};')
    else:
        # Output
        print('String table at Ghidra 0x%08X (file 0x%X), %d entries, %d bytes each, terminator=%s\n' % (
            va, file_offset, len(strings), args.element_length, args.terminator))

        for i, s in enumerate(strings):
            label = 's_%s_%08x' % (s.replace(' ', '_') if s else 'empty', va + (i + skip) * args.element_length)
            if args.raw:
                raw = data[(i + skip) * args.element_length:(i + skip + 1) * args.element_length]
                hex_str = ' '.join('%02X' % b for b in raw)
                print('  [%3d] %-30s %s' % (i + skip, repr(s), hex_str))
            else:
                print('  [%3d] %s' % (i + skip, repr(s)))


if __name__ == '__main__':
    main()
