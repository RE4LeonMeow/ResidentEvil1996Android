#!/usr/bin/env python3
"""gen_dc_item_descriptions.py - Director's Cut item descriptions for the port.

The port keeps item descriptions as `STR()` constants behind
`g_ItemDescriptions[79]` (0x004c6160, indexed by itemId - 1) and the JPN build
has its own at 0x004c9370. The DC's table is *not* named in either PS1 program,
so it was located by searching the image for a known-good string:

  * the port's item 0x2C text "A carving of the moon" encodes to
    `1D 00 3F 3D 4E 52 45 4A 43 00 4B 42 00 50 44 41 00 49 4B 4B 4A`
    and appears at 0x8008E769 (plus 0x8008E809/0x8008E821);
  * searching for the pointer 0x8008E769 found the table entry at 0x8008EA70;
  * the entry for item 3 is 0x8008E38D, which decodes to the port's
    "Remington M870.\\nA pump-action shotgun." - so the entries are item
    ordered with entry 0 = item 1;
  * the table's 79 pointers start at **0x8008E9C6**; item 4's entry is
    0x8008E998 and decodes to "A Beretta M92FS. Automatic\\nCustom edition.".

This emits `ItemDescriptions.{h,cpp}` in the port's own `STR()` style.

The DC has no sub machine guns, so its table stops one entry short of the
USA's: 0x4D/0x4E - the two indices the item viewer passes for the INGRAM /
MINIMI examine - point outside the string pool (0x80080F3C / 0x80031604) and
the tail fallback above repeats 0x4C's text there. That is the DC's table as
built; the port keeps the PC weapons in DC mode, so RoomInit.cpp's
dc_usa_item_descriptions() overrides those two indices with the USA text.

    python tools/gen_dc_item_descriptions.py            # show the table
    python tools/gen_dc_item_descriptions.py --write    # emit the C files
"""
import argparse
import json
import os
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'tools'))
from decode_re1 import FONT_8x14        # noqa: E402

BRIDGE = 'http://127.0.0.1:8089/read_memory'
PROGRAM = 'SLUS_005.51'
OUT_C = os.path.join(REPO, 'src', 'game', 'dc', 'ItemDescriptions.cpp')
OUT_H = os.path.join(REPO, 'src', 'game', 'dc', 'ItemDescriptions.h')

DC_DESCRIPTIONS = 0x8008E9C4     # 79 pointers; entry k = item k + 1
COUNT = 79
TERMINATOR = 0x01
MAX_DESC = 160

# byte -> character. The project's glyph table covers the letters and digits;
# the punctuation the descriptions actually use is filled in from the DC data
# ('.' = 0x79, '-' = 0x3B) and the double quotes / line break use STR()'s own
# parser escapes.
#
# NOTE the two escaping levels, matching the port's own strings:
#   * STR()'s PARSER escapes are doubled in the C source - the port writes
#     "\\n" (line break, 0x02) and "\\o" (the opening quote, 0x78) so the
#     compiler hands the parser the two characters `\`+`n` / `\`+`o`;
#   * a closing quote must use the C escape "\"" (the port's
#     `...Sonata\"."`), because a bare or doubly-escaped quote would terminate
#     the literal instead.
# Emitting a single "\n" let the compiler produce raw 0x0A, the parser built a
# string with no 0x01 terminator and the message renderer hung (item 0x02 froze
# the item viewer on 2026-09-14); "\\\"" broke the literal outright.
SPECIAL = {0x00: ' ', 0x01: None, 0x02: '\\\\n', 0x79: '.', 0x3B: '-',
           0x78: '\\\\o', 0x19: '\\"'}
DECODE = {}
for _i, _g in enumerate(FONT_8x14):
    if len(_g) == 1 and _g.isascii() and _g.isprintable():
        DECODE.setdefault(_i, _g)
DECODE.update({k: v for k, v in SPECIAL.items() if v is not None})


def fetch(address, length):
    url = f'{BRIDGE}?program={PROGRAM}&address={hex(address)}&length={length}'
    with urllib.request.urlopen(url, timeout=30) as r:
        payload = json.load(r)
    if 'data' not in payload:
        raise RuntimeError(f'read {address:#x} len {length}: {payload}')
    return bytes(payload['data'])


def u32(b, off):
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


def decode_text(raw):
    out = []
    for v in raw:
        if v in DECODE:
            out.append(DECODE[v])
        else:
            raise ValueError(f'undecodable byte {v:#04x}')
    return ''.join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    try:
        table = fetch(DC_DESCRIPTIONS, COUNT * 4)
    except Exception as e:                      # noqa: BLE001
        print(f'bridge read failed: {type(e).__name__}: {e}', file=sys.stderr)
        return 1

    ptrs = [u32(table, i * 4) for i in range(COUNT)]
    # The DC's table covers fewer entries than the port's 79 (its items stop
    # earlier); anything pointing outside the string region is tail padding, so
    # reuse the last good description there.
    good = 0
    fixed = []
    for p in ptrs:
        if 0x8008E200 <= p < DC_DESCRIPTIONS:
            good += 1
            fixed.append(p)
        else:
            fixed.append(fixed[-1] if fixed else 0x8008E328)
    ptrs = fixed
    print(f'dc description pointers: {len(ptrs)} ({good} in the string region), '
          f'first {ptrs[0]:#x}')
    if good < 40:
        print('too few valid pointers - wrong table base?', file=sys.stderr)
        return 1

    texts = []
    for i, p in enumerate(ptrs):
        raw = fetch(p, MAX_DESC)
        end = raw.find(bytes([TERMINATOR]))
        raw = raw[:end] if end >= 0 else raw
        try:
            t = decode_text(raw)
        except ValueError as e:
            print(f'item {i + 1:#04x}: {e} in {raw.hex(" ")}', file=sys.stderr)
            return 1
        texts.append(t)

    for i, t in enumerate(texts):
        if i + 1 in (2, 3, 4, 0x2C, 0x31, 0x32):
            print(f'  item {i + 1:#04x}: {t!r}')

    if not args.write:
        print('(--write not given: C files not modified)')
        return 0

    shown = [(i + 1, t) for i, t in enumerate(texts)]
    uniq = []
    for _, t in shown:
        if t not in uniq:
            uniq.append(t)
    const_of = {t: f's_dcidesc{n:02d}' for n, t in enumerate(uniq)}
    consts_out = '\n'.join(
        f'static constexpr auto {const_of[t]} = STR("{t}");' for t in uniq)
    table_out = '\n'.join(
        f'    (unsigned char*){const_of[t]}.bytes,  // [{i - 1:#04x}] item {i:#04x}'
        for i, t in shown)

    header = f'''// ItemDescriptions.h - Director's Cut item descriptions.
//
// GENERATED by tools/gen_dc_item_descriptions.py - do not edit by hand.
#pragma once

#define DC_ITEM_DESCRIPTION_COUNT {COUNT}

// The DC's item description table (SLUS_005.51 0x8008E9C4, indexed by
// itemId - 1 like the port's g_ItemDescriptions). Only a few entries differ
// from the USA build - item 0x04 is the Beretta M92FS "Custom edition" text
// and 0x31/0x32 are the MOON CREST halves.
//
// The DC has no sub machine guns: its last real entry is 0x4C and 0x4D/0x4E -
// the two indices the item viewer passes for the INGRAM / MINIMI examine -
// point outside the string pool, so the generator repeated 0x4C's text there.
// RoomInit.cpp's dc_usa_item_descriptions() puts the USA entries back.
extern unsigned char* g_dcItemDescriptions[DC_ITEM_DESCRIPTION_COUNT];
'''

    source = f'''// ItemDescriptions.cpp - Director's Cut item descriptions.
//
// GENERATED by tools/gen_dc_item_descriptions.py - do not edit by hand.
#include "../Globals.h"
#include "PrintText.h"              // STR()
#include "ItemDescriptions.h"

{consts_out}
unsigned char* g_dcItemDescriptions[DC_ITEM_DESCRIPTION_COUNT] = {{
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
