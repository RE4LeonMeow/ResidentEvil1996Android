#!/usr/bin/env python3
"""verify_dc_entity_models.py - check the DC entity-model port against the disc.

Two Director's Cut edits live in `src/game/dc/EntityModels.cpp` and
`dc_apply_zombie_tables()`, and both are transcriptions of data that is on the
player's disc rather than in this repo:

1. **The model table.** The DC's LoadEntityEMD (SLUS_005.51 0x800238c8) indexes
   a 69-entry-per-block table at 0x8008d03c (stride 0x8a) of PS1 file-system
   indices, where the port's g_emdPathTable is the OG's 53-per-block table of
   names. Eight of those entries name a file the port's table does not have at
   that index; five of them sit past the port's block width, where the port's
   table holds the *other* character block. This checks the DC values, resolves
   them to file names through the disc's own ENEMY directory (the file-system
   index counts the ISO directory in alphabetical order), and compares the
   result against the strings in EntityModels.cpp.

2. **Entity id 0x16 is a zombie.** update_entities dispatches through a table
   the stage overlay registers, so the Forest zombie's handler is per-stage
   data. This reads slot 0x16 out of every OG and DC stage overlay and checks
   that only the two DC overlays the arrange balcony rooms run under register
   it, both with the same handler their zombie slots carry.

The model table needs the DC executable, which is not in this repo, so check 1
reads it through the same Ghidra bridge tools/gen_dc_damage_tables.py uses and
reports SKIP if the bridge is not up. Everything else is local.

    python tools/verify_dc_entity_models.py
"""
import json
import os
import re
import struct
import sys
import urllib.error
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRIDGE = 'http://127.0.0.1:8089/read_memory'
PROGRAM = 'SLUS_005.51'

SRC = os.path.join(REPO, 'src', 'game', 'dc', 'EntityModels.cpp')
HDR = os.path.join(REPO, 'src', 'game', 'dc', 'EntityModels.h')
ZOMBIE_SRC = os.path.join(REPO, 'src', 'game', 'entities', 'Zombie.cpp')
DC_ENEMY = os.path.join(REPO, 'assets', 'PSX_DC', 'ENEMY')
DC_OVERLAY_ENEMY = os.path.join(REPO, 'assets', 'DC', 'Enemy')
DC_OVERLAY_PLAYERS = os.path.join(REPO, 'assets', 'DC', 'Players')

# The DC's model table: base, entries per character block, block stride.
MODEL_TABLE = 0x8008D03C
BLOCK_ENTRIES = 69
BLOCK_STRIDE = 0x8A

# First file-system index of the ENEMY directory: the table's entry 0 is
# CHAR10.EMD, the first name in it.
ENEMY_FIRST_INDEX = 28

# What the port claims, as (model index, Chris file, Jill file). Indices 51/52
# are deliberately absent: they land inside the port's own block, so the port's
# table already names the DC's files and EntityModels.cpp does not override
# them. They are checked separately against the port's table below.
EXPECTED = [
    (26,   'EM1016.EMD', 'EM1116.EMD'),   # Forest zombie
    (0x35, 'EM1035.EMD', 'EM1035.EMD'),   # player 3, ADVANCED
    (0x38, 'EM1040.EMD', 'EM1040.EMD'),   # NPC Chris, ADVANCED
    (0x39, 'EM1041.EMD', 'EM1041.EMD'),   # NPC Jill, ADVANCED
    (0x3B, 'EM1043.EMD', 'EM1043.EMD'),   # NPC Rebecca, ADVANCED
    (0x44, 'EM104C.EMD', 'EM104C.EMD'),   # NPC id 0x2C, ADVANCED
]

# The two the port's g_emdPathTable is expected to cover on its own.
IN_PORT_TABLE = [
    (0x33, 'EM1030.EMD', 'EM1031.EMD'),   # costume variant
    (0x34, 'EM1032.EMD', 'EM1033.EMD'),   # player, ADVANCED
]

# Per-stage entity dispatch tables (docs/PSX_DC_ENEMY_AI.md). Slot 0x16 is the
# Forest zombie; the arrange balcony rooms are STAGE9 (folds onto stage 1, the
# STAGE2 overlay) and STAGEE (folds onto stage 6, the STAGE7 overlay).
ENTITY_TABLES = {
    'DC': {'STAGE1': 0x801210E0, 'STAGE2': 0x80120A00, 'STAGE3': 0x8013052C,
           'STAGE4': 0x80131704, 'STAGE5': 0x8012C3E4, 'STAGE6': 0x80127E9C,
           'STAGE7': 0x8012AA0C},
    'OG': {'STAGE1': 0x80122100, 'STAGE2': 0x80121910, 'STAGE3': 0x8013283C,
           'STAGE4': 0x80133270, 'STAGE5': 0x8012DD2C, 'STAGE6': 0x801290E4,
           'STAGE7': 0x8012BFA8},
}
OVERLAY_DIR = {'DC': os.path.join(REPO, 'assets', 'PSX_DC', 'PROG2'),
               'OG': os.path.join(REPO, 'assets', 'PSX', 'PROG2')}
FOREST_STAGES = {'STAGE2', 'STAGE7'}

failures = []
skips = []


def fail(msg):
    failures.append(msg)
    print('  FAIL %s' % msg)


def ok(msg):
    print('  ok   %s' % msg)


# ---------------------------------------------------------------------------
# 1. The DC's model table
# ---------------------------------------------------------------------------
def fetch(address, length):
    url = '%s?program=%s&address=%s&length=%d' % (BRIDGE, PROGRAM, hex(address), length)
    with urllib.request.urlopen(url, timeout=30) as r:
        return bytes(json.load(r)['data'])


def enemy_index_map():
    """file-system index -> file name, from the disc's own ENEMY directory."""
    names = sorted(os.listdir(DC_ENEMY))
    return {ENEMY_FIRST_INDEX + i: n for i, n in enumerate(names)}


def check_model_table(src, port_table):
    print('1. the DC model table (SLUS_005.51 0x%08X)' % MODEL_TABLE)
    if not os.path.isdir(DC_ENEMY):
        skips.append('no assets/PSX_DC/ENEMY - cannot resolve file indices')
        print('  SKIP no %s' % DC_ENEMY)
        return
    try:
        raw = fetch(MODEL_TABLE, BLOCK_STRIDE * 2)
    except (urllib.error.URLError, OSError) as exc:
        skips.append('Ghidra bridge unavailable (%s)' % exc)
        print('  SKIP Ghidra bridge not reachable at %s' % BRIDGE)
        return

    blocks = [struct.unpack_from('<%dH' % BLOCK_ENTRIES, raw, b * BLOCK_STRIDE)
              for b in (0, 1)]
    names = enemy_index_map()

    # The anchors that make the index->name mapping trustworthy: if these hold,
    # the alphabetical-order premise holds for the whole directory.
    for idx, want in ((0, 'CHAR10.EMD'), (4, 'EM1000.EMD')):
        got = names.get(blocks[0][idx])
        if got != want:
            fail('anchor: Chris entry %d is file index %d = %s, expected %s'
                 % (idx, blocks[0][idx], got, want))
    if not failures:
        ok('file-index anchors (entry 0 = CHAR10, entry 4 = EM1000)')

    for index, chris, jill in EXPECTED:
        got = (names.get(blocks[0][index]), names.get(blocks[1][index]))
        if got != (chris, jill):
            fail('model index %d (0x%02X): DC has %s / %s, the port ports %s / %s'
                 % (index, index, got[0], got[1], chris, jill))
            continue
        # ... and the port really names those files.
        want = {'enemy/%s' % chris.lower().replace('.emd', '.emd'),
                'enemy/%s' % jill.lower()}
        for path in want:
            if '"%s"' % path not in src:
                fail('model index %d (0x%02X): "%s" is not in EntityModels.cpp'
                     % (index, index, path))
                break
        else:
            ok('model index %2d (0x%02X) -> %s / %s' % (index, index, chris, jill))

    # The two the port's own table must cover, or the DC loads the wrong outfit.
    for index, chris, jill in IN_PORT_TABLE:
        got = (names.get(blocks[0][index]), names.get(blocks[1][index]))
        if got != (chris, jill):
            fail('model index 0x%02X: DC has %s / %s, expected %s / %s'
                 % (index, got[0], got[1], chris, jill))
            continue
        have = (port_table[index], port_table[index + 53])
        want = ('enemy/%s' % chris.lower(), 'enemy/%s' % jill.lower())
        if have != want:
            fail("model index 0x%02X: the port's g_emdPathTable has %s / %s,"
                 ' the DC needs %s / %s' % (index, have[0], have[1], want[0], want[1]))
        else:
            ok('model index 0x%02X -> %s / %s, already in g_emdPathTable'
               % (index, chris, jill))


def port_emd_table():
    """The port's g_emdPathTable, as a list of 106 paths."""
    path = os.path.join(REPO, 'src', 'game', 'EntityModelLoader.cpp')
    text = open(path, encoding='utf-8', errors='replace').read()
    start = text.index('g_emdPathTable[106][17] = {')
    end = text.index('};', start)
    entries = re.findall(r'"([^"]+)"', text[start:end])
    if len(entries) != 106:
        fail('g_emdPathTable parsed as %d entries, expected 106' % len(entries))
    return entries


# ---------------------------------------------------------------------------
# 2. Entity id 0x16's handler, from the stage overlays
# ---------------------------------------------------------------------------
def overlay_table(path, table_va, count=40):
    data = open(path, 'rb').read()
    load = struct.unpack_from('<I', data, 0x18)[0]
    off = table_va - load + 0x800
    return struct.unpack_from('<%dI' % count, data, off)


def check_entity_tables(zombie_src):
    print('2. entity id 0x16 in the per-stage dispatch tables')
    for build in ('OG', 'DC'):
        if not os.path.isdir(OVERLAY_DIR[build]):
            skips.append('no %s overlays' % build)
            print('  SKIP no %s' % OVERLAY_DIR[build])
            continue
        for stage, va in sorted(ENTITY_TABLES[build].items()):
            exe = os.path.join(OVERLAY_DIR[build], stage + '.EXE')
            if not os.path.exists(exe):
                skips.append('missing %s' % exe)
                continue
            ent = overlay_table(exe, va)
            slot = ent[0x16]
            zombie = ent[0]                     # ids 0/1/0x11 are the zombie
            expect_forest = build == 'DC' and stage in FOREST_STAGES
            if expect_forest:
                if slot == 0:
                    fail('%s %s registers no handler for id 0x16' % (build, stage))
                elif slot != zombie:
                    fail('%s %s id 0x16 -> 0x%08X, but its zombie slot is 0x%08X'
                         % (build, stage, slot, zombie))
                else:
                    ok('%s %s id 0x16 -> 0x%08X = its zombie handler'
                       % (build, stage, slot))
            elif slot != 0:
                fail('%s %s registers id 0x16 (0x%08X); only DC %s should'
                     % (build, stage, slot, '/'.join(sorted(FOREST_STAGES))))
    if 'enemies_update_functions_tbl[ENEMY_ZOMBIE_FOREST] = (void*)zombie_update;' \
            not in zombie_src:
        fail('dc_apply_zombie_tables() does not point id 0x16 at zombie_update')
    else:
        ok('dc_apply_zombie_tables() points id 0x16 at zombie_update')


# ---------------------------------------------------------------------------
# 3. The overlay actually carries the models the port now asks for
# ---------------------------------------------------------------------------
def check_overlay_files(src):
    print('3. every model EntityModels.cpp names is in the DC overlay')
    if not os.path.isdir(DC_OVERLAY_ENEMY):
        skips.append('no assets/DC/Enemy')
        print('  SKIP no %s' % DC_OVERLAY_ENEMY)
        return
    have = {n.lower() for n in os.listdir(DC_OVERLAY_ENEMY)}
    wanted = sorted(set(re.findall(r'"enemy/([a-z0-9_]+\.emd)"', src)))
    if not wanted:
        fail('no model paths found in EntityModels.cpp')
    for name in wanted:
        if name in have:
            ok('assets/DC/Enemy/%s' % name)
        else:
            fail('assets/DC/Enemy/%s is missing - DC mode would fall back to the'
                 ' base tree, which does not have it either' % name)

    # The same file also names the DC's in-hand weapon models (the custom
    # Beretta's Players/W0F and W1F). Those live in the overlay's Players folder,
    # and a miss there is silent too: LoadFile returns -1 and the equipped weapon
    # renders with no model at all.
    print('   and the weapon models it names are in assets/DC/Players')
    if not os.path.isdir(DC_OVERLAY_PLAYERS):
        skips.append('no assets/DC/Players')
        print('  SKIP no %s' % DC_OVERLAY_PLAYERS)
        return
    have_p = {n.lower() for n in os.listdir(DC_OVERLAY_PLAYERS)}
    wanted_p = sorted(set(re.findall(r'"(players/[a-z0-9_]+\.emw)"', src)))
    if not wanted_p:
        fail('no weapon model paths found in EntityModels.cpp')
    for name in wanted_p:
        if name.split('/')[-1] in have_p:
            ok('assets/DC/Players/%s' % name.split('/')[-1])
        else:
            fail('assets/DC/Players/%s is missing - the custom Beretta would '
                 'load no model in DC mode, and the base tree does not have it '
                 'either' % name.split('/')[-1])


def main():
    src = open(SRC, encoding='utf-8', errors='replace').read()
    hdr = open(HDR, encoding='utf-8', errors='replace').read()
    zombie_src = open(ZOMBIE_SRC, encoding='utf-8', errors='replace').read()

    # The one constant both halves share: id 0x16 + 4.
    m = re.search(r'#define DC_EMD_INDEX_FOREST_ZOMBIE\s+(\d+)', hdr)
    if not m or int(m.group(1)) != 0x16 + 4:
        fail('DC_EMD_INDEX_FOREST_ZOMBIE is not 0x16 + 4')

    port_table = port_emd_table()
    check_model_table(src, port_table)
    check_entity_tables(zombie_src)
    check_overlay_files(src)

    print()
    for s in skips:
        print('skipped: %s' % s)
    if failures:
        print('%d check(s) FAILED' % len(failures))
        return 1
    print('ok: the DC entity-model port matches the disc')
    return 0


if __name__ == '__main__':
    sys.exit(main())
