#!/usr/bin/env python3
"""gen_dc_item_names.py - Director's Cut item names for the port.

The port keeps its item names as `STR("NAME\\x07")` constants (0x004bf0a0's
pointer table, indexed by itemId - 1). The DC's equivalent table is the one the
analysis calls "name ptr table DC 0x800901D8"; like the port's it is 1-based and
its strings use the same font encoding, so the DC differs from the USA table in
only four ids:

    0x04  COLT PYTHON    -> BERETTA
    0x0D  DUMDUM ROUNDS  -> LOCKPICK   (name only - model/description/sprite stay)
    0x31  LOCKPICK       -> MOON CREST
    0x32  (empty)        -> MOON CREST

This tool emits a ItemNames.cpp in the port's own style: decoded `STR()`
constants plus the pointer table. Unchanged ids reuse the port's text verbatim
(read from MenuData.cpp), so the only decoding needed is for the four renames -
whose text is plain letters and spaces.

The DC has no sub machine guns, so its table carries neither name: entries
110/111 (the port's INGRAM / MINIMI ids 0x6F / 0x70) both point at the stale
CRANK string and the generic-name group that follows starts two entries early.
That is the DC's table as built; the port keeps the PC weapons in DC mode, so
Rendering.cpp's dc_usa_item_names() overrides those two ids with the USA names.

    python tools/gen_dc_item_names.py            # compare only
    python tools/gen_dc_item_names.py --write    # emit the C files
"""
import argparse
import json
import os
import re
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'tools'))
from decode_re1 import FONT_8x14        # noqa: E402  the project's glyph table

BRIDGE = 'http://127.0.0.1:8089/read_memory'
PROGRAM = 'SLUS_005.51'
OUT_C = os.path.join(REPO, 'src', 'game', 'dc', 'ItemNames.cpp')
OUT_H = os.path.join(REPO, 'src', 'game', 'dc', 'ItemNames.h')
MENUDATA_SRC = os.path.join(REPO, 'src', 'game', 'MenuData.cpp')

DC_NAMES = 0x800901D8            # name pointer table (entry 0 = item 1)
COUNT = 128                      # the port's table size; the DC has 126 valid
NAME_TERMINATOR = 0x07
MAX_NAME = 48
# The ids the DC genuinely renames, verified in-game (see docs/DC_PORT.md
# and memory/psx-dc-items.md). The comparison below still reports every text
# difference, but the emitted table only renames these.
RENAMED = {0x04, 0x0D, 0x31, 0x32}


def fetch(address, length):
    url = f'{BRIDGE}?program={PROGRAM}&address={hex(address)}&length={length}'
    with urllib.request.urlopen(url, timeout=30) as r:
        payload = json.load(r)
    if 'data' not in payload:
        raise RuntimeError(f'read {address:#x} len {length}: {payload}')
    return bytes(payload['data'])


def u32(b, off):
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


def decode(raw):
    """RE1 8x14 font encoding -> text, using the project's own glyph table."""
    return ''.join(FONT_8x14[v] if v < len(FONT_8x14) else '?' for v in raw)


def port_tables():
    """(constant name -> STR text, pointer-table order) from MenuData.cpp."""
    src = open(MENUDATA_SRC, encoding='utf-8').read()
    consts = dict(re.findall(r'static constexpr auto (\w+)\s*=\s*STR\("(.*?)"\);', src))
    body = re.split(r'g_ItemNamePointers\[\d+\] = \{', src, 1)[1].split('};', 1)[0]
    order = re.findall(r'\(unsigned char\*\)(\w+)\.bytes', body)
    return consts, order


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    try:
        table = fetch(DC_NAMES, COUNT * 4)
    except Exception as e:                      # noqa: BLE001
        print(f'bridge read failed: {type(e).__name__}: {e}', file=sys.stderr)
        print('the Ghidra bridge must be running with SLUS_005.51 open', file=sys.stderr)
        return 1

    ptrs = [u32(table, i * 4) for i in range(COUNT)]
    valid = [p for p in ptrs if 0x80080000 <= p < 0x80200000]
    print(f'dc name pointers: {len(ptrs)} ({len(valid)} valid), '
          f'first {valid[0]:#x}, last {valid[-1]:#x}')

    strings = []
    for p in ptrs:
        if 0x80080000 <= p < 0x80200000:
            raw = fetch(p, MAX_NAME)
            end = raw.find(bytes([NAME_TERMINATOR]))
            strings.append(raw[:end + 1] if end >= 0 else raw)
        else:
            strings.append(strings[-1] if strings else bytes([NAME_TERMINATOR]))

    consts, order = port_tables()
    print(f'port names: {len(consts)} constants, {len(order)} table entries')
    if len(order) < COUNT:
        print('could not read the port pointer table', file=sys.stderr)
        return 1

    sample = decode(strings[1][:-1])
    print(f'layout check: dc item 2 decodes to {sample!r} (expected "BERETTA")')

    # Compare the DC's text with the port's for every valid entry. Entries whose
    # DC bytes decode to non-ascii are glyph-table gaps (the project's table has
    # no punctuation rows), not differences.
    changed = []
    for i in range(COUNT):
        if not (0x80080000 <= ptrs[i] < 0x80200000):
            continue
        dc_name = decode(strings[i][:-1])
        port_name = consts[order[i]].replace('\\x07', '')
        if dc_name != port_name and dc_name.isascii():
            changed.append(i + 1)
            print(f'  item 0x{i + 1:02X}: port {port_name!r} -> dc {dc_name!r}')
    print(f'names differing from the port: {len(changed)} -> '
          + ', '.join(f'{i:#04x}' for i in changed))

    # The DC text per entry: use the DC's own text wherever its bytes decode
    # cleanly, and fall back to the port's text for the entries whose bytes hit
    # a gap in the project's glyph table (punctuation rows).
    texts = []
    for i in range(COUNT):
        if 0x80080000 <= ptrs[i] < 0x80200000:
            t = decode(strings[i][:-1])
            if t.isascii():
                texts.append(t)
                continue
        texts.append(consts[order[i]].replace('\\x07', ''))

    if not args.write:
        print('(--write not given: C files not modified)')
        return 0

    # One STR() constant per distinct name, then the pointer table.
    uniq = []
    for t in texts:
        if t not in uniq:
            uniq.append(t)
    const_of = {t: f's_dcItemName{n:03d}' for n, t in enumerate(uniq)}
    consts_out = '\n'.join(
        f'static constexpr auto {const_of[t]} = STR("{t}\\x07");' for t in uniq)
    table_out = '\n'.join(
        f'    (unsigned char*){const_of[t]}.bytes,'
        f'    // [{i:3d}] {t}' for i, t in enumerate(texts))

    header = f'''// ItemNames.h - Director's Cut item names.
//
// GENERATED by tools/gen_dc_item_names.py - do not edit by hand.
#pragma once

#define DC_ITEM_NAME_COUNT {COUNT}

// The DC's item-name pointer table (SLUS_005.51 0x800901D8, indexed by
// itemId - 1 like the port's g_ItemNamePointers). The DC renames four ids -
// 0x04 -> BERETTA, 0x0D -> LOCKPICK (name only) and 0x31/0x32 -> MOON CREST -
// and every other name is identical to the USA build's.
//
// The two PC-only sub machine guns are the exception: the DC has none, so ids
// 0x6F/0x70 (INGRAM / MINIMI) both point at the stale CRANK string and the
// generic-name group starts two entries early. Rendering.cpp's
// dc_usa_item_names() puts the USA names back on those two ids.
extern const unsigned char* g_dcItemNamePointers[DC_ITEM_NAME_COUNT];
'''

    source = f'''// ItemNames.cpp - Director's Cut item names.
//
// GENERATED by tools/gen_dc_item_names.py - do not edit by hand.
#include "../Globals.h"
#include "PrintText.h"              // STR()
#include "ItemNames.h"

{consts_out}

const unsigned char* g_dcItemNamePointers[DC_ITEM_NAME_COUNT] = {{
{table_out}
}};
'''

    with open(OUT_H, 'w', encoding='utf-8', newline='\n') as f:
        f.write(header)
    with open(OUT_C, 'w', encoding='utf-8', newline='\n') as f:
        f.write(source)
    print(f'wrote {os.path.relpath(OUT_H, REPO)} and {os.path.relpath(OUT_C, REPO)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
