#!/usr/bin/env python3
"""verify_dc_item_models.py - cross-check the Director's Cut item-view mapping.

The item menu's 3D view loads `item_m2/<name>.ivm`, where <name> comes from the
item's item image type (byte 0 of its g_ItemImageLookupTable record) through
g_ItemModelFileNames - 75 eight-byte records in the USA build, plus the DC
overrides in src/game/dc/ItemModels.cpp (docs/DC_PORT.md 3g).

Nothing about that mapping is generated, so this checks it against the data that
*is* generated or shipped, from the sources of truth on disk:

  1. every image type the DC's lookup table can produce resolves to a name:
     either inside the USA table (< 75) or through a DC override;
  2. every DC override names a real file in the DC asset overlay
     (assets/DC/Item_m2) and in the extracted PS1 disc (assets/PSX_DC/ITEM_M2);
  3. each override's image type is actually reachable - some DC item's byte 0;
  4. the overrides are unique and their names fit the 8-byte record;
  5. g_ItemsImageBuffer in src/Globals.cpp covers the DC sprite sheet, so
     LoadAllItemsTexture cannot overrun it and every item's sprite row is in
     bounds (the DC's appended rows are rows 72..75 of a 76-row sheet).

The overlay is what the engine actually reads with [Game] Mode=DC (see
src/system/AssetPath.cpp); --base only names the tree it falls back to, which
is checked for the ORIGINAL sheet so the row-count comparison below is honest.

Usage: python tools/verify_dc_item_models.py [--base JPN]
"""
import argparse
import os
import re
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DC_MODELS = os.path.join(REPO, 'src', 'game', 'dc', 'ItemModels.cpp')
DC_TABLES = os.path.join(REPO, 'src', 'game', 'dc', 'ItemTables.cpp')
GLOBALS = os.path.join(REPO, 'src', 'Globals.cpp')
DC_DISC = os.path.join(REPO, 'assets', 'PSX_DC')

USA_MODEL_NAME_COUNT = 75        # ITEM_MODEL_NAME_COUNT in src/Globals.h
ITEM_SPRITE_BYTES = 1200         # one 40x30 8bpp row of ITEM_ALL.PIX
FAKE_TYPE = 0xFF                 # matches no item: the "ING" bug's neighbourhood


def parse_overrides():
    """The { imageType, "name" } entries of src/game/dc/ItemModels.cpp."""
    text = open(DC_MODELS, encoding='utf-8').read()
    body = text.split('s_dcItemModels[] = {', 1)[1].split('\n};', 1)[0]
    entries = []
    for m in re.finditer(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*"([^"]*)"\s*\}', body):
        entries.append((int(m.group(1), 16), m.group(2)))
    if not entries:
        raise SystemExit(f'{DC_MODELS}: no override entries parsed')
    return entries


def parse_dc_lookup():
    """The DC item image types: byte 0 of each record in the generated table."""
    text = open(DC_TABLES, encoding='utf-8').read()
    body = text.split('g_dcItemImageLookupTable[', 1)[1].split('= {', 1)[1]
    body = body.split('\n};', 1)[0]
    data = bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body))
    # Item records live at itemId * 4; the 459-byte table's tail is the item-use
    # category and heal tables, whose bytes are not image types. The last real
    # item record is 0x4E (the port's table holds 79 records, 0..0x4E).
    return {i: data[i * 4] for i in range(1, 0x4F)}


def parse_items_buffer_size():
    text = open(GLOBALS, encoding='utf-8').read()
    m = re.search(r'BYTE g_ItemsImageBuffer\[(\d+)\]', text)
    if not m:
        raise SystemExit(f'{GLOBALS}: g_ItemsImageBuffer definition not found')
    return int(m.group(1))


def parse_const_table(path, symbol):
    text = open(path, encoding='utf-8').read()
    m = re.search(re.escape(symbol) + r'\[[^\]]*\] = \{(.*?)\n\};', text, re.S)
    if not m:
        raise SystemExit(f'{path}: {symbol} definition not found')
    return bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(1)))


def sheet_rows(path):
    return os.path.getsize(path) // ITEM_SPRITE_BYTES


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', default='JPN', choices=['USA', 'JPN'],
                    help='base tree the DC overlay sits on (default JPN)')
    args = ap.parse_args()

    overrides = parse_overrides()
    lookup = parse_dc_lookup()
    failures = []

    print(f'DC overrides: {len(overrides)}')
    by_type = {}
    for image_type, name in overrides:
        print(f'  image type {image_type:#04x} -> {name!r}')
        if image_type in by_type:
            failures.append(f'image type {image_type:#04x} overridden twice')
        by_type[image_type] = name
        if len(name) > 7:
            failures.append(f'{name!r} does not fit an 8-byte name record')
        if image_type not in lookup.values():
            failures.append(f'image type {image_type:#04x} ({name!r}) is not '
                            f'used by any DC item')

    # 1. every image type the DC's lookup produces must resolve
    unresolved = sorted({t for t in lookup.values()
                         if t >= USA_MODEL_NAME_COUNT and t not in by_type})
    owners = {}
    for item_id, image_type in sorted(lookup.items()):
        if image_type >= USA_MODEL_NAME_COUNT:
            owners[image_type] = owners.get(image_type, []) + [f'{item_id:#04x}']
    print(f'DC image types >= {USA_MODEL_NAME_COUNT} (past the USA table): '
          + (', '.join(f'{t:#04x} used by item ' + '/'.join(v)
                       for t, v in sorted(owners.items())) or 'none'))
    if unresolved:
        failures.append('DC image types with no view name: '
                        + ', '.join(f'{t:#04x}' for t in unresolved))
    if FAKE_TYPE in lookup.values():
        failures.append('an item claims image type 0xFF')

    # 2. the art must exist, both in the overlay and on the extracted disc
    tree_dir = os.path.join(REPO, 'assets', 'DC', 'Item_m2')
    for image_type, name in overrides:
        for label, d in (('overlay', tree_dir),
                         ('PSX_DC', os.path.join(DC_DISC, 'ITEM_M2'))):
            path = os.path.join(d, name.upper() + '.IVM')
            if not os.path.exists(path):
                failures.append(f'{name!r} missing from {label} ({path})')
            else:
                print(f'  {name}.ivm: {label} ok '
                      f'({os.path.getsize(path)} B)')

    # 5. the sprite sheet must fit its buffer, and every row must be readable
    buf = parse_items_buffer_size()
    worst = max(lookup.values())          # the highest sprite row + 1
    for label, p in (('DC', os.path.join(REPO, 'assets', 'DC', 'Data',
                                         'item_all.pix')),
                     ('USA', os.path.join(REPO, 'assets', args.base, 'DATA',
                                          'ITEM_ALL.PIX'))):
        if not os.path.exists(p):
            continue
        rows = sheet_rows(p)
        size = os.path.getsize(p)
        print(f'{label} item sheet: {rows} rows / {size} B')
        if size > buf:
            failures.append(f'g_ItemsImageBuffer is {buf} B but the {label} '
                            f'sheet is {size} B (load would overrun)')
        elif rows < worst:
            # The DC's lookup asks for rows the USA sheet does not have; that is
            # expected (the DC ships its own sheet) - only the buffer matters.
            print(f'  ({rows} rows is short of the DC lookup\'s row '
                  f'{worst - 1} - Mode=DC reads the overlay sheet instead)')
    # The row an item's sprite comes from is byte0 - 1 (LoadItemImage and
    # itembox_draw_slot_icon both subtract one), so the highest image type in
    # the DC's lookup is the last row the game can ask for.
    if worst * ITEM_SPRITE_BYTES > buf:
        failures.append(f'item sheet row {worst - 1} (image type {worst:#04x}) '
                        f'runs past the {buf}-byte buffer')

    # 6. the combine-sprite table: the DC's g_ItemImageTypeTable must actually be
    # applied, its three new crest entries must point somewhere, and every entry
    # must name a row of the DC's mix sheet (a zero entry means "the sprite does
    # not change when combined into", which is normal elsewhere in the table).
    dc_imgtype = parse_const_table(DC_TABLES, 'g_dcItemImageTypeTable')
    game_start = open(os.path.join(REPO, 'src', 'game', 'GameStart.cpp'),
                      encoding='utf-8').read()
    if 'g_dcItemImageTypeTable' not in game_start:
        failures.append('g_dcItemImageTypeTable is generated but never applied '
                        '(dc_apply_item_tables in GameStart.cpp)')
    mix = os.path.join(REPO, 'assets', 'DC', 'Data', 'item_mix.pix')
    mix_rows = sheet_rows(mix) if os.path.exists(mix) else 21
    crest = [i for i, v in enumerate(dc_imgtype) if v and i >= 26 and i <= 28]
    print(f'DC mix-row table: {len(dc_imgtype)} entries, {mix_rows} sheet rows')
    if len(crest) != 3:
        failures.append('the DC table\'s three crest entries (26..28) are not all '
                        f'set ({crest}) - a crest combine would keep the old sprite')
    for i, v in enumerate(dc_imgtype):
        if v > mix_rows:
            failures.append(f'mix-row table[{i}] = {v} is past the '
                            f'{mix_rows}-row DC mix sheet')

    # 7. the table's TAIL. The engine reads two more things straight out of this
    # array: the item-use category thresholds at [0x13B..0x144] (menu_item_use_item
    # 0x00401070 walks them from the top) and the heal values at [0x10A + itemId]
    # (menu_item_use_heal 0x00401260). The DC's own array holds the same values at
    # 0x133 and 0xFE - one record and one base apart - so its records have to be
    # re-based onto the port's bytes and the tail must stay the port's. Emitting
    # the DC's region whole is what shipped the thresholds four bytes late, which
    # gave the first aid spray category 9 ("unusable") and the Beretta category 7
    # ("heal").
    port_lk = parse_const_table(GLOBALS, 'g_ItemImageLookupTable')
    dc_lk = parse_const_table(DC_TABLES, 'g_dcItemImageLookupTable')
    PORT_THRESHOLDS = 0x13B
    PORT_HEAL = 0x10A
    if len(port_lk) != len(dc_lk):
        failures.append(f'the port\'s lookup is {len(port_lk)} bytes and the DC\'s '
                        f'is {len(dc_lk)}: their offsets cannot line up')
    else:
        n = 10
        if dc_lk[PORT_THRESHOLDS:PORT_THRESHOLDS + n] != \
                port_lk[PORT_THRESHOLDS:PORT_THRESHOLDS + n]:
            failures.append('the DC lookup\'s use-category thresholds are not the '
                            'port\'s values at the port\'s offset (0x13B) - every '
                            'item would be filed under the wrong use category')

        def use_category(table, item_id):
            """The engine's walk: index 9 first, then down, stopping at the first
            threshold the item id reaches."""
            idx = 9
            threshold = table[PORT_THRESHOLDS + 9]
            while item_id < threshold:
                idx -= 1
                if idx < 0:
                    return None       # below the lowest threshold: the engine
                                      # over-runs, and does so in both builds
                threshold = table[PORT_THRESHOLDS + idx]
            return idx

        wrong = [(hex(i), use_category(dc_lk, i), use_category(port_lk, i))
                 for i in range(0x0B, 0x4F)
                 if use_category(dc_lk, i) != use_category(port_lk, i)]
        if wrong:
            failures.append('use categories differ from the port\'s for '
                            + ', '.join(f'{i} (dc {g}, port {w})' for i, g, w in wrong))
        print(f'use categories: {0x4F - 0x0B - len(wrong)} of {0x4F - 0x0B} item '
              f'ids agree with the port (spray 0x41 = {use_category(dc_lk, 0x41)})')
        for i in range(0x41, 0x4C):
            if dc_lk[PORT_HEAL + i] != port_lk[PORT_HEAL + i]:
                failures.append(f'heal for item {i:#04x} is {dc_lk[PORT_HEAL + i]:#04x}, '
                                f'not the port\'s {port_lk[PORT_HEAL + i]:#04x}')

    print()
    if failures:
        for f in failures:
            print(f'FAIL: {f}', file=sys.stderr)
        return 1
    print('ok: DC item views resolve, art present, sprite buffer in bounds')
    return 0


if __name__ == '__main__':
    sys.exit(main())
