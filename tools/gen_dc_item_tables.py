#!/usr/bin/env python3
"""gen_dc_item_tables.py - Director's Cut item tables for the port.

Two tables matter here:

  * the 459-byte item image-lookup table (4 bytes per item, then the
    use-category threshold + heal tables) - the port's
    `g_ItemImageLookupTable` (0x004bd81d);
  * the combine table: an array of pointers to `count + count*4-byte record`
    blobs - the port's `g_ItemCombinePtrs` / `g_ItemCombineData`
    (0x004bd768 / 0x004bd5b0, 35 entries, 440 bytes; the DC has 38).

Layout mapping, verified against the PS1 consumers:

  * In both builds the max-quantity table and the image-lookup table overlap by
    one byte: the PC has g_ItemMaxQty at 0x004bd81c and the lookup at
    0x004bd81d (its own comment says the lookup "starts inside record..."), and
    the PS1 has the same pair at 0x8008E18C / 0x8008E18D. A lookup record's
    fourth byte is therefore the next max-quantity record's first byte.
  * Both PS1 tables are **1-based** - FUN_80042410 reads
    `(&DAT_8008e18c)[(itemId - 1) * 4]` - so item 1's record is at the base,
    while the port indexes `itemId * 4` with a dummy item 0 record first.
  * The record layout is otherwise identical: [image type, combine index,
    unknown-name category, flags] for the lookup.

So the emitted lookup is the port's item 0 record, then the DC's item 1..N
records, then the DC's category/heal tail - i.e. the 1-based PS1 table re-based
to 0. That keeps the port's item 0 and applies the DC's edits everywhere else.

Neither PS1 binary is in this repo, so both are read through the Ghidra bridge.

    python tools/gen_dc_item_tables.py            # compare only
    python tools/gen_dc_item_tables.py --write    # emit the C files
"""
import argparse
import json
import os
import re
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRIDGE = 'http://127.0.0.1:8089/read_memory'
PROGRAM = 'SLUS_005.51'
OG_PROGRAM = 'SLUS_001.70'      # the OG PS1 build, for the comparison column
OUT_C = os.path.join(REPO, 'src', 'game', 'dc', 'ItemTables.cpp')
OUT_H = os.path.join(REPO, 'src', 'game', 'dc', 'ItemTables.h')
GLOBALS_SRC = os.path.join(REPO, 'src', 'Globals.cpp')
MENUDATA_SRC = os.path.join(REPO, 'src', 'game', 'MenuData.cpp')

LOOKUP_BYTES = 459
ITEMS = 0x4F                     # the port's table holds 79 item records (0..0x4E)
# The DC's table is one record shorter: its records run 1..0x4D, and its
# threshold/heal tables sit that much further along (see DC_THRESHOLDS).
DC_LAST_ITEM = 0x4D

# Where the tables the engine reads actually live, in each build's own
# coordinates, relative to the lookup base (item 1's record).
#
# The DC's arrays are NOT the port's shifted by a constant. Because its record
# region is one record shorter and its 1-based layout has no dummy item 0, its
# tail tables each sit at their own offset, and the two differ by 4:
#
#   what the engine reads            | port (g_ItemImageLookupTable) | DC
#   ---------------------------------+-------------------------------+--------
#   lookup record for item id        | id*4                          | (id-1)*4
#   use-category thresholds, 10      | 0x13B                         | 0x133
#   heal[itemId]                     | 0x10A + itemId                | 0xFE + itemId
#   examine messages, 16             | 0x15B (+ flagIndex)           | 0x14F
#
# Verified against the DC's own consumers rather than inferred: its
# menu_item_use_heal (0x80056660) reads `(&DAT_8008e28b)[itemId]` (0xFE 8, the
# heal base) and its item-use dispatch (0x80056380-8c) walks a 10-byte threshold
# table at 0x8008E2C0 + k (0x133), reading the top entry at 0x8008E2C9 (0x13C).
# Both the threshold values and the heal values the engine reads are IDENTICAL
# to the port's - the DC moved the tables, it did not change them - so the
# emitted table keeps the port's tail and takes only the DC's records.
PORT_THRESHOLDS = 0x13B
DC_THRESHOLDS = 0x133
THRESHOLD_COUNT = 10
PORT_HEAL = 0x10A
DC_HEAL = 0xFE
HEAL_COUNT = 0x61

# Both lookups are the max-quantity table + 1 (the two overlap by one byte) and
# item 1's record sits at that base: verified by re-reading the OG's item-1
# record (`01 80 80 0f`, the knife) at 0x8008DC35 and the DC's at 0x8008E18D.
DC_LOOKUP = 0x8008E18D           # the image-lookup table (1-based item 1)
DC_MAXQTY = 0x8008E18C           # the max-quantity table (1 byte earlier)
OG_LOOKUP = 0x8008DC35           # item 1's record in the OG build
OG_MAXQTY = 0x8008DC34           # the max-quantity table (1 byte earlier)
DC_COMBINE = 0x8008E0CC          # pointer table, 38 entries
DC_COMBINE_COUNT = 38
OG_COMBINE_COUNT = 35
TAIL_BYTES = LOOKUP_BYTES - ITEMS * 4       # category threshold + heal tables

# g_ItemImageTypeTable: combine index -> the 1-based row of the *mix* sheet
# (ITEM_MIX.PIX) holding the sprite of the item that owns that index. The port's
# is 35 bytes at 0x004bd7f8, immediately before g_ItemMaxQty (0x004bd81c); the
# DC's is 38 bytes immediately before its max-quantity table (0x8008E18C), i.e.
# at 0x8008E164 - one byte per combine table, and the DC has 38 of them.
# menu_item_combine_refresh reloads a slot's icon through this table, so a
# combine whose result has no entry keeps the pre-combine sprite.
DC_IMAGE_TYPE = 0x8008E164
DC_IMAGE_TYPE_COUNT = 38


def fetch(address, length, program=PROGRAM):
    url = f'{BRIDGE}?program={program}&address={hex(address)}&length={length}'
    with urllib.request.urlopen(url, timeout=30) as r:
        payload = json.load(r)
    if 'data' not in payload:
        raise RuntimeError(f'read {program} {address:#x} len {length}: {payload}')
    return bytes(payload['data'])


def u32(b, off):
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


def fetch_combine(base, n):
    """Return (pointers, blobs); each blob is `count + count*4` bytes."""
    table = fetch(base, n * 4)
    ptrs = [u32(table, i * 4) for i in range(n)]
    lo, hi = min(ptrs), max(ptrs)
    region = fetch(lo, hi - lo + 512)
    return ptrs, [region[p - lo:p - lo + 1 + region[p - lo] * 4] for p in ptrs]


def ps1_record_to_port(rec):
    """The record layout matches the port's (only the base is 1-based)."""
    return bytes(rec)


def port_lookup():
    text = open(GLOBALS_SRC, encoding='utf-8').read()
    body = text.split('g_ItemImageLookupTable[459] = {', 1)[1].split('\n};', 1)[0]
    return bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body))


def port_combine():
    src = open(MENUDATA_SRC, encoding='utf-8').read()
    body = re.split(r'g_ItemCombineData\[\d+\] = \{', src, 1)[1].split('\n};', 1)[0]
    data = bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body))
    ptr_body = re.split(r'g_ItemCombinePtrs\[\d+\] = \{', src, 1)[1].split('};', 1)[0]
    ptrs = [int(v) for v in re.findall(r'g_ItemCombineData \+ (\d+)', ptr_body)]
    return data, [data[p:p + 1 + data[p] * 4] for p in ptrs[:OG_COMBINE_COUNT]]


def port_image_types():
    text = open(MENUDATA_SRC, encoding='utf-8').read()
    body = re.split(r'g_ItemImageTypeTable\[\d+\] = \{', text, 1)[1].split('\n};', 1)[0]
    return bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body))


def check_image_types(dc, port, dc_items, port_lk):
    """The DC table is the USA one with its three crest entries inserted.

    Verified against the two builds' own lookups rather than assumed: the
    entries the DC did not touch (below the first crest index) must match the
    port's byte for byte, every item whose combine index the DC uses must have a
    non-zero entry (or its combine result keeps the pre-combine sprite - the
    defect this table exists to fix), and each mixed-herb item's entry must be
    the port's own value for that herb, shifted by the same +3 as the herb
    combine indices.
    """
    problems = []
    insert_at = 26                       # first crest recipe index
    insert_n = DC_COMBINE_COUNT - OG_COMBINE_COUNT
    if dc[:insert_at] != port[:insert_at]:
        problems.append(f'entries 0..{insert_at - 1} differ from the port')

    # A zero entry is normal (the port has them throughout): it means the item's
    # sprite does not change when it is combined into (ammo transfers into a
    # weapon, say). The DC's three new crest indices must not be zero, though -
    # those are exactly the combines this table was extended for.
    for idx in range(insert_at, insert_at + insert_n):
        if dc[idx] == 0:
            owners = ', '.join(f'{i:#04x}' for i in dc_items if dc_items[i][1] == idx)
            problems.append(f'new crest entry [{idx}] (item(s) {owners}) is zero - '
                            f'the combine result would keep the pre-combine sprite')

    # The herbs: the DC moved their rows +3 along with their combine indices.
    for i, rec in dc_items.items():
        idx = rec[1]
        if 0x80 in (idx,) or idx < insert_at + insert_n or idx >= len(dc) or dc[idx] == 0:
            continue
        pidx = idx - insert_n
        if pidx < len(port) and port[pidx] != 0 and dc[idx] != port[pidx] + insert_n:
            problems.append(f'item {i:#04x}: mix row {dc[idx]} is not the port\'s '
                            f'{port[pidx]} + {insert_n}')

    print(f'dc image types: {len(dc)} entries, crest {insert_at}..'
          f'{insert_at + insert_n - 1} = '
          + '/'.join(str(v) for v in dc[insert_at:insert_at + insert_n])
          + f', herbs..{len(dc) - 1} = '
          + '/'.join(str(v) for v in dc[insert_at + insert_n:]))
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    try:
        dc_raw = fetch(DC_LOOKUP, LOOKUP_BYTES)
        og_raw = fetch(OG_LOOKUP, (ITEMS - 1) * 4, OG_PROGRAM)
        dc_ptrs, dc_blobs = fetch_combine(DC_COMBINE, DC_COMBINE_COUNT)
        dc_imgtype = fetch(DC_IMAGE_TYPE, DC_IMAGE_TYPE_COUNT)
    except Exception as e:                      # noqa: BLE001
        print(f'bridge read failed: {type(e).__name__}: {e}', file=sys.stderr)
        print('the Ghidra bridge must be running with SLUS_005.51 open', file=sys.stderr)
        return 1

    port_lk = port_lookup()
    port_data, port_blobs = port_combine()
    print(f'port lookup bytes parsed: {len(port_lk)} (expected {LOOKUP_BYTES})')
    print(f'port combine blobs parsed: {len(port_blobs)}')
    print(f'dc combine pointers: {len(dc_ptrs)}, first {dc_ptrs[0]:#x}, last {dc_ptrs[-1]:#x}')

    # Port-order DC records, indexed by item id (item 0 has no PS1 record, and
    # the DC's table ends at item 0x4D: past that the bytes belong to its
    # threshold/heal tables, so item 0x4E keeps the port's record).
    dc_items = {i: ps1_record_to_port(dc_raw[(i - 1) * 4:(i - 1) * 4 + 4])
                for i in range(1, DC_LAST_ITEM + 1)}
    for i in range(DC_LAST_ITEM + 1, ITEMS):
        dc_items[i] = port_lk[i * 4:i * 4 + 4]
    og_items = {i: ps1_record_to_port(og_raw[(i - 1) * 4:(i - 1) * 4 + 4])
                for i in range(1, ITEMS)}

    # The two tail tables the engine reads by offset. Both must carry the values
    # the port's own array has there (the DC moved them, it did not change them);
    # if that ever stops holding, the emitted table has to place the DC's values
    # at the port's offsets the way the records are placed.
    tail_problems = []
    if dc_raw[DC_THRESHOLDS:DC_THRESHOLDS + THRESHOLD_COUNT] != \
            port_lk[PORT_THRESHOLDS:PORT_THRESHOLDS + THRESHOLD_COUNT]:
        tail_problems.append('use-category thresholds differ from the port')
    for i in range(HEAL_COUNT):
        if dc_raw[DC_HEAL + i] != port_lk[PORT_HEAL + i] and 0x41 <= i <= 0x4B:
            tail_problems.append(f'heal for item {i:#04x} differs from the port')
    for p in tail_problems:
        print(f'  TAIL PROBLEM: {p}')

    agree = sum(1 for i in range(1, ITEMS)
                if port_lk[i * 4:i * 4 + 4] == og_items[i])
    print(f'layout check: port == rotated OG for {agree} of {ITEMS - 1} items')

    changed = []
    for i in range(1, ITEMS):
        p = port_lk[i * 4:i * 4 + 4]
        d = dc_items[i]
        o = og_items[i]
        if d == p:
            continue
        changed.append(i)
        tag = '' if p == o else '  (port also != og -> conflict, DC wins)'
        print(f'  item 0x{i:02X}: port {p.hex(" ")}  og {o.hex(" ")}  dc {d.hex(" ")}{tag}')
    print(f'lookup: {len(changed)} item(s) differ from the port: '
          + ', '.join(f'{i:#04x}' for i in changed))

    same = sum(1 for i in range(max(len(port_blobs), len(dc_blobs)))
               if (port_blobs[i] if i < len(port_blobs) else None)
               == (dc_blobs[i] if i < len(dc_blobs) else None))
    print(f'combine: {same} of {max(len(port_blobs), len(dc_blobs))} entries identical '
          f'(DC has {len(dc_blobs) - len(port_blobs)} extra)')
    for i in range(max(len(port_blobs), len(dc_blobs))):
        p = port_blobs[i] if i < len(port_blobs) else None
        d = dc_blobs[i] if i < len(dc_blobs) else None
        if p != d:
            print(f'  combine[{i:2d}]: port {"-" if p is None else p.hex(" ")} '
                  f'-> dc {"-" if d is None else d.hex(" ")}')

    port_imgtype = port_image_types()
    imgtype_problems = check_image_types(dc_imgtype, port_imgtype, dc_items, port_lk)
    for p in imgtype_problems:
        print(f'  IMAGE-TYPE PROBLEM: {p}')

    # Lookup: the port's own array with the DC's records re-based onto it, so
    # every consumer's offset keeps meaning what it always did. Blitting the
    # DC's region whole would NOT do: its addresses are all one record and one
    # base apart from the port's, and the fix-up for the record region is a
    # different length from the fix-up for the tail (8 vs 4 bytes), so a single
    # shift cannot place both. The tail is left as the port's because the DC's
    # values there are the same - checked above - and the record for item 0x4E
    # stays too, since the DC's table has nothing at that index.
    merged = bytearray(port_lk)
    for i in range(1, DC_LAST_ITEM + 1):
        merged[i * 4:i * 4 + 4] = dc_raw[(i - 1) * 4:(i - 1) * 4 + 4]
    merged[PORT_THRESHOLDS:PORT_THRESHOLDS + THRESHOLD_COUNT] = \
        dc_raw[DC_THRESHOLDS:DC_THRESHOLDS + THRESHOLD_COUNT]
    merged = bytes(merged)

    # The check the shipped table would have failed. menu_item_use_item
    # (0x00401070) picks an item's use category by walking the ten thresholds
    # from the top - it reads [0x144] first, then [bVar5 + 0x13B] for bVar5 = 8
    # down. Those ten bytes are NOT part of any record: in the original the
    # table's first entry lands one byte past the last record, and the shipped
    # DC table wrote the DC's shifted tail over them, so every consumer of this
    # table read a category belonging to some other item. First aid spray ended
    # up in category 9 ("unusable", what the doc items are) and the Beretta in
    # category 7 (the healables), which is exactly the reported "the first aid
    # spray does nothing" / "using an item does the wrong thing".
    #
    # The invariant to hold: every item id's category in the emitted table must
    # be the one the port's own table gives, since the DC's thresholds are the
    # port's (checked above, so this is a restatement - but it is the property
    # the engine actually depends on, and it fails loudly if a future DC dump
    # moves these bytes again).
    def use_category(table, item_id):
        idx = 9
        threshold = table[PORT_THRESHOLDS + 9]
        while item_id < threshold:
            idx -= 1
            if idx < 0:
                return None            # the engine over-runs below 0x0B
            threshold = table[PORT_THRESHOLDS + idx]
        return idx

    bad = []
    for i in range(0x0B, ITEMS):
        want = use_category(port_lk, i)
        got = use_category(merged, i)
        if got != want:
            bad.append((i, got, want))
    # and the two categories the fix was reported against, as a sanity net on
    # the table itself: the healables (spray 0x41, the herbs 0x43..0x4B) must
    # be 7 and the first doc item 0x4C must be 9. The shipped table put the
    # spray in 9 and the Beretta in 7 - the two swapped, which is why using a
    # spray did nothing and using a gun tried to heal.
    for i, want in ((0x41, 7), (0x42, 7), (0x43, 7), (0x4B, 7), (0x4C, 9)):
        if use_category(merged, i) != want:
            bad.append((i, use_category(merged, i), want))
    print('use categories: %d of %d item ids match the port\'s%s'
          % (ITEMS - 0x0B - len([b for b in bad if b[0] >= 0x0B]), ITEMS - 0x0B,
             '' if not bad else '  MISMATCHES: ' + ', '.join(
                 f'{i:#04x} got {g} want {w}' for i, g, w in bad)))
    if bad:
        tail_problems.append('use categories are wrong for ' + ', '.join(
            f'{i:#04x}' for i, _, _ in bad))

    if not args.write:
        print('(--write not given: C files not modified)')
        return 0

    if imgtype_problems or tail_problems:
        print('refusing to write: a DC table failed its checks', file=sys.stderr)
        for p in imgtype_problems + tail_problems:
            print(f'  {p}', file=sys.stderr)
        return 1

    data = bytearray()
    offsets = []
    for blob in dc_blobs:
        offsets.append(len(data))
        data += blob

    def hex_rows(b, per=12):
        return '\n'.join('    ' + ','.join(f'0x{v:02X}' for v in b[i:i + per]) + ','
                         for i in range(0, len(b), per))

    header = f'''// ItemTables.h - Director's Cut item data.
//
// GENERATED by tools/gen_dc_item_tables.py - do not edit by hand.
#pragma once

#define DC_ITEM_LOOKUP_BYTES {LOOKUP_BYTES}
#define DC_ITEM_COMBINE_COUNT {len(dc_blobs)}
#define DC_ITEM_COMBINE_BYTES {len(data)}
#define DC_ITEM_IMAGE_TYPE_COUNT {DC_IMAGE_TYPE_COUNT}

// The port's item image-lookup table (g_ItemImageLookupTable) with the DC's
// per-item records applied. The PS1 stores the records 1-based and rotated
// (flags first); this table is in the port's order, so consumers switch tables
// on g_bDcMode with no other change. DC differs from the port at
// {len(changed)} item(s).
extern const unsigned char g_dcItemImageLookupTable[DC_ITEM_LOOKUP_BYTES];

// The DC's combine table (SLUS_005.51 0x8008E0CC, 38 entries vs the port's 35):
// each entry is a `count` byte then count 4-byte records, with
// g_dcItemCombinePtrs holding offsets into g_dcItemCombineData. The three extra
// entries are the MOON CREST recipes, so the lookup's combine indices already
// point at the shifted entries - no +3 arithmetic in the consumers.
extern const unsigned char g_dcItemCombineData[DC_ITEM_COMBINE_BYTES];
extern const unsigned char* g_dcItemCombinePtrs[DC_ITEM_COMBINE_COUNT];

// g_ItemImageTypeTable for the DC (SLUS_005.51 0x8008E164, 38 bytes vs the
// port's 35): combine index -> the 1-based ITEM_MIX.PIX row of the sprite for
// the item owning that index, read by menu_item_combine_refresh when a combine
// changes a slot. The DC inserted its three crest recipes at 26..28 and the
// mixed herbs' rows moved +3 with their combine indices, so a combine into the
// assembled MOON CREST (index 0x1C) reloads the crest icon instead of leaving
// the half's sprite in place.
extern const unsigned char g_dcItemImageTypeTable[DC_ITEM_IMAGE_TYPE_COUNT];
'''

    ptrs = '\n'.join(f'    g_dcItemCombineData + {o},' for o in offsets)
    source = f'''// ItemTables.cpp - Director's Cut item data.
//
// GENERATED by tools/gen_dc_item_tables.py - do not edit by hand.
#include "ItemTables.h"

const unsigned char g_dcItemImageLookupTable[DC_ITEM_LOOKUP_BYTES] = {{
{hex_rows(bytes(merged))}
}};

const unsigned char g_dcItemCombineData[DC_ITEM_COMBINE_BYTES] = {{
{hex_rows(bytes(data))}
}};

const unsigned char* g_dcItemCombinePtrs[DC_ITEM_COMBINE_COUNT] = {{
{ptrs}
}};

const unsigned char g_dcItemImageTypeTable[DC_ITEM_IMAGE_TYPE_COUNT] = {{
{hex_rows(bytes(dc_imgtype))}
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
