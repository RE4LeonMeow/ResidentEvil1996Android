#!/usr/bin/env python3
"""verify_stage_tables.py - the Director's Cut arrange stages (STAGE8-E) and the
port's stage-indexed tables.

The port does NOT grow its stage-indexed tables for the DC's seven arrange
stages. It folds instead: arrange stage id S (7..13) reads the table row of base
stage S-7, because an arrange room is a re-dressed copy of the base stage's room
with the same room id. `get_stage_id()` in src/Globals.h does the fold and is
the identity for stages 0-6, so nothing changes with Mode=OG.

Folding rather than growing is not only cheaper - g_roomBgmState lives inside
g_BioCard, so widening it would move every field after it and change the save
format.

This checks the three things that claim rests on:

  1. Every arrange room's room id exists in its base stage (the fold's premise).
  2. Every arrange room id fits the stage-indexed tables' per-stage width, so a
     folded index cannot run off the end of a row.
  3. No .cpp still indexes a stage-indexed table with a raw g_stageId - a site
     that missed the fold would read another stage's row, or past the table.
  4. Every arrange room resolves to a background, following the same path rule
     the engine uses - including the revisit fold that sends STAGED/STAGEE to
     STAGE8/STAGE9's art, as base stages 6/7 go to stage 1/2's.
  5. The arrange-room table in src/game/dc/ArrangeStages.cpp - the DC's own
     data at SLUS_005.51 0x80010670, which decides which rooms have an arrange
     version at all - names exactly the rooms the arrange stages ship, in both
     directions.
  6. The character-variant table (SLUS_005.51 0x80090f48) never sends the
     second character to a `...1` RDT the disc does not carry.

Exit status is non-zero on any failure, so it can gate a build.

Usage: python tools/verify_stage_tables.py
"""
import os
import re
import sys
import glob

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DC = os.path.join(REPO, 'assets', 'PSX_DC')

# arrange stage folder -> the base stage folder it is a re-dressed copy of
ARRANGE = [('STAGE8', 'STAGE1'), ('STAGE9', 'STAGE2'), ('STAGEA', 'STAGE3'),
           ('STAGEB', 'STAGE4'), ('STAGEC', 'STAGE5'), ('STAGED', 'STAGE6'),
           ('STAGEE', 'STAGE7')]

# The narrowest per-stage width among the stage-indexed tables. g_RoomSoundNameTable
# is 7 x 29; the rest are 7 x 32. A room id at or past this is only safe because
# Room_LoadEnemySoundBanks bounds-checks, so report it rather than pass silently.
SOUND_ROOMS_PER_STAGE = 29
TABLE_ROOMS_PER_STAGE = 32

failures = []
notes = []


def room_ids(folder):
    """{roomId: [file names]} for one stage folder, keyed by the ROOM id byte.

    A name is ROOM<stage><roomHi><roomLo><variant>.RDT, so the room id is the
    two hex digits in the middle - the variant digit is NOT part of g_roomId.
    """
    out = {}
    for f in glob.glob(os.path.join(DC, folder, '*.RDT')):
        name = os.path.basename(f)[:-4]          # ROOMxYYv
        body = name[4:]
        if len(body) != 4:
            continue
        try:
            rid = int(body[1:3], 16)
        except ValueError:
            continue
        out.setdefault(rid, []).append(os.path.basename(f))
    return out


def check_room_mapping():
    print('1. arrange room ids vs their base stage')
    if not os.path.isdir(DC):
        notes.append('assets/PSX_DC not present - skipped the data checks')
        print('   assets/PSX_DC not present, skipping')
        return
    total = 0
    for arr, base in ARRANGE:
        a = room_ids(arr)
        b = room_ids(base)
        if not a:
            continue
        missing = sorted(set(a) - set(b))
        total += len(a)
        status = 'ok' if not missing else 'MISSING in %s: %s' % (
            base, ', '.join('0x%02X' % r for r in missing))
        print('   %-7s -> %-7s %2d room ids  %s' % (arr, base, len(a), status))
        if missing:
            failures.append('%s has room ids absent from %s: %s'
                            % (arr, base, [hex(r) for r in missing]))
    print('   %d arrange rooms checked' % total)


def check_room_width():
    print('2. arrange room ids against the per-stage table widths')
    if not os.path.isdir(DC):
        return
    worst = -1
    worst_file = ''
    for arr, _ in ARRANGE:
        for rid, files in room_ids(arr).items():
            if rid > worst:
                worst, worst_file = rid, files[0]
    if worst < 0:
        return
    print('   highest arrange room id: 0x%02X (%d) in %s' % (worst, worst, worst_file))
    if worst >= TABLE_ROOMS_PER_STAGE:
        failures.append('room id %d is past the %d-room stride of the 32-wide '
                        'stage tables' % (worst, TABLE_ROOMS_PER_STAGE))
    elif worst >= SOUND_ROOMS_PER_STAGE:
        notes.append('room id %d is past g_RoomSoundNameTable\'s %d-room stride; '
                     'Room_LoadEnemySoundBanks bounds-checks, so that room loads '
                     'no entity SFX rather than crashing'
                     % (worst, SOUND_ROOMS_PER_STAGE))
        print('   note: past the %d-room sound-table stride (guarded)'
              % SOUND_ROOMS_PER_STAGE)
    else:
        print('   fits both the %d- and %d-room strides'
              % (SOUND_ROOMS_PER_STAGE, TABLE_ROOMS_PER_STAGE))


# Tables indexed by the stage. Each entry is a regex that matches an indexing
# expression built from a RAW g_stageId; every one of these should read
# get_stage_id() instead.
RAW_INDEX_PATTERNS = [
    # g_stageId * <n>  - any table stride
    (r'g_stageId\s*\*\s*0?x?[0-9a-fA-F]', 'g_stageId used as a table row'),
    # [g_stageId] - direct row lookup
    (r'\[\s*g_stageId\s*\]', 'g_stageId used as a direct table index'),
    # g_stageId > 4 - the mansion-revisit fold. It must see the FOLDED row in
    # every case, path or table: the arrange revisit stages (STAGED/STAGEE, ids
    # 12/13) carry no backgrounds and reuse STAGE8/STAGE9's, the same way base
    # stages 6/7 reuse stage 1/2's, so a bare test both misses them and wrongly
    # fires for arrange stages 7-11.
    (r'g_stageId\s*>\s*4', 'bare `g_stageId > 4` revisit fold'),
]

# Sites that legitimately use the raw id: file paths (the arrange stages ship
# their own files under STAGE8-E) and the debug/save screens, which show or set
# the real stage.
ALLOW_FILES = {
    os.path.join('src', 'game', 'RoomInit.cpp'): ['hexDigits[g_stageId + 1]'],
}


def check_sources():
    print('3. source scan for un-folded stage indexing')
    hits = 0
    for path in glob.glob(os.path.join(REPO, 'src', '**', '*.cpp'), recursive=True):
        rel = os.path.relpath(path, REPO)
        with open(path, encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
        for n, line in enumerate(lines, 1):
            code = line.split('//')[0]
            if 'g_stageId' not in code:
                continue
            for pat, why in RAW_INDEX_PATTERNS:
                if re.search(pat, code):
                    print('   %s:%d  %s' % (rel, n, why))
                    print('      %s' % code.strip())
                    failures.append('%s:%d %s' % (rel, n, why))
                    hits += 1
    if hits == 0:
        print('   clean: every stage-indexed read goes through get_stage_id()')


# Stage ids, in the order the RDT/background path builds them: id 0 is STAGE1.
STAGE_DIGITS = '123456789ABCDE'


def bg_stage_digit(stage_id):
    """The stage digit the engine puts in a background path for this stage.

    Mirrors load_room_bg (Room.cpp): the revisit fold fires on the FOLDED row,
    so base 5/6 -> 0/1 and arrange 12/13 -> 7/8, while arrange 7-11 keep their
    own digit.
    """
    row = stage_id - 7 if stage_id >= 7 else stage_id
    if row > 4:
        stage_id -= 5
    return STAGE_DIGITS[stage_id]


def check_backgrounds():
    print('4. arrange rooms resolve to a background')
    overlay = os.path.join(REPO, 'assets', 'DC')
    if not os.path.isdir(overlay):
        print('   assets/DC not built yet, skipping')
        return
    missing = []
    checked = 0
    for i, (arr, _) in enumerate(ARRANGE):
        stage_id = 7 + i
        digit = bg_stage_digit(stage_id)
        for rid in sorted(room_ids(arr)):
            checked += 1
            pak = os.path.join(overlay, 'Stage' + digit,
                               'RC%s%02X0.pak' % (digit, rid))
            if not os.path.exists(pak):
                missing.append('%s room 0x%02X -> %s'
                               % (arr, rid, os.path.relpath(pak, REPO)))
        if digit != STAGE_DIGITS[stage_id]:
            print('   %-7s (id %2d) reuses Stage%s\'s backgrounds (revisit fold)'
                  % (arr, stage_id, digit))
    print('   %d arrange room(s), %d without a camera-0 background'
          % (checked, len(missing)))
    for m in missing[:12]:
        print('      %s' % m)
        failures.append('no background for ' + m)


ARRANGE_SRC = os.path.join(REPO, 'src', 'game', 'dc', 'ArrangeStages.cpp')


def parse_arrange_table():
    """The room-id rows out of dc/ArrangeStages.cpp, as [set(roomIds)] x 7."""
    text = open(ARRANGE_SRC, encoding='utf-8', errors='replace').read()
    start = text.index('g_dcArrangeRooms[DC_ARRANGE_ROWS][DC_ARRANGE_COLS] = {')
    end = text.index('};', start)
    rows = []
    for body in re.findall(r'\{([^{}]*)\}', text[start:end]):
        vals = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
        if len(vals) != 9:
            continue                      # not a table row
        rows.append([v for v in vals if v != 0xFF])
    return rows


def check_arrange_table():
    """The table that decides WHICH rooms have an arrange version.

    This is the DC's own data (SLUS_005.51 0x80010670, read by 0x80043fb4) and
    it is what the port's room_file_stage() walks. It must agree exactly with
    the arrange RDTs on the disc in both directions: a room in the table with no
    arrange file would send the game looking for a file that is not there, and
    an arrange file whose room is not in the table is a room the player can
    never reach.
    """
    print('5. the arrange-room table vs the shipped arrange RDTs')
    if not os.path.isdir(DC):
        print('   assets/PSX_DC not present - skipped')
        return
    rows = parse_arrange_table()
    if len(rows) != len(ARRANGE):
        failures.append('ArrangeStages.cpp has %d rows, expected %d'
                        % (len(rows), len(ARRANGE)))
        print('   FAIL %d rows parsed, expected %d' % (len(rows), len(ARRANGE)))
        return

    for i, (arr, base) in enumerate(ARRANGE):
        want = set(rows[i])
        have = set(room_ids(arr).keys())
        if want == have:
            print('   %-7s %d room(s): %s'
                  % (arr, len(want), ' '.join('%02X' % r for r in sorted(want))))
            continue
        for r in sorted(want - have):
            failures.append('%s: table says room %02X has an arrange version, '
                            'but %s has no such RDT' % (base, r, arr))
            print('   FAIL room %02X in the table, not in %s' % (r, arr))
        for r in sorted(have - want):
            failures.append('%s ships room %02X, but the table never sends the '
                            'game there' % (arr, r))
            print('   FAIL %s ships room %02X, unreachable' % (arr, r))


# Every stage folder on the disc, in the order the variant table rows run.
ALL_STAGE_FOLDERS = ['STAGE1', 'STAGE2', 'STAGE3', 'STAGE4', 'STAGE5', 'STAGE6',
                     'STAGE7', 'STAGE8', 'STAGE9', 'STAGEA', 'STAGEB', 'STAGEC',
                     'STAGED', 'STAGEE']
STAGE_DIGITS = '123456789ABCDE'


def parse_variant_table():
    """The 14x32 character-variant rows out of dc/ArrangeStages.cpp."""
    text = open(ARRANGE_SRC, encoding='utf-8', errors='replace').read()
    start = text.index('g_dcRoomCharVariant[DC_VARIANT_ROWS][DC_VARIANT_COLS] = {')
    end = text.index('};', start)
    rows = []
    for body in re.findall(r'\{([^{}]*)\}', text[start:end]):
        vals = [int(v) for v in re.findall(r'(\d+)', body)]
        if len(vals) == 32:
            rows.append(vals)
    return rows


def check_variant_table():
    """Which rooms have a character-specific RDT.

    The dangerous direction is one-way: if the table says a room HAS a variant
    and the disc does not carry it, the game asks for a file that is not there -
    which is how Jill's entry into arrange 2F ended in an access violation. The
    other way round costs nothing: a file on the disc the game never asks for.
    """
    print('6. the character-variant table vs the shipped RDTs')
    if not os.path.isdir(DC):
        print('   assets/PSX_DC not present - skipped')
        return
    rows = parse_variant_table()
    if len(rows) != len(ALL_STAGE_FOLDERS):
        failures.append('ArrangeStages.cpp has %d variant rows, expected %d'
                        % (len(rows), len(ALL_STAGE_FOLDERS)))
        print('   FAIL %d rows parsed' % len(rows))
        return

    missing = 0
    unused = 0
    for si, folder in enumerate(ALL_STAGE_FOLDERS):
        d = os.path.join(DC, folder)
        if not os.path.isdir(d):
            continue
        have = {n.upper() for n in os.listdir(d) if n.upper().endswith('.RDT')}
        for room in range(32):
            base = 'ROOM%s%02X0.RDT' % (STAGE_DIGITS[si], room)
            alt = 'ROOM%s%02X1.RDT' % (STAGE_DIGITS[si], room)
            if base not in have:
                continue
            says_variant = rows[si][room] == 0
            has_variant = alt in have
            if says_variant and not has_variant:
                failures.append('%s: the table sends the second character to %s,'
                                ' which the disc does not have' % (folder, alt))
                print('   FAIL %s/%s is asked for but not shipped' % (folder, alt))
                missing += 1
            elif has_variant and not says_variant:
                unused += 1
    print('   %d room(s) whose second file is never asked for (harmless), '
          '%d asked for and absent' % (unused, missing))


def main():
    check_room_mapping()
    print()
    check_room_width()
    print()
    check_sources()
    print()
    check_backgrounds()
    print()
    check_arrange_table()
    print()
    check_variant_table()
    print()
    for note in notes:
        print('note: %s' % note)
    if failures:
        print('FAIL: %d problem(s)' % len(failures))
        return 1
    print('ok: arrange stages fold onto their base stage rows safely')
    return 0


if __name__ == '__main__':
    sys.exit(main())
