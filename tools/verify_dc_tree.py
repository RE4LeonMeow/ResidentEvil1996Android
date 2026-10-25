#!/usr/bin/env python3
"""verify_dc_tree.py - is the Director's Cut asset tree complete enough to run?

In DC mode the game reads `assets/DC/`, and `ResolveAssetRoot`
(src/system/AssetPath.cpp) only falls back to the base tree for the folders the
DC disc genuinely cannot supply. Everywhere else a fallback means the DC tree is
short a file and the player will see OG content inside a DC session - which
looks like a content bug and is not one.

This reports that shortfall up front, instead of leaving it to be discovered one
`[assets]` log line at a time while playing:

  1. Every base-tree file, checked against the DC tree. Misses inside the
     fallback folders are expected and counted; misses outside them are listed.
  2. Every DC room, walked through the engine's own background path rule -
     including the revisit fold that sends stages 6/7 to stage 1/2's art and
     STAGED/STAGEE to STAGE8/STAGE9's - to confirm it lands on a real file.

Usage: python tools/verify_dc_tree.py [--base USA|JPN]
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Must match kBaseFallbackFolders in src/system/AssetPath.cpp.
FALLBACK_FOLDERS = {'sound', 'voice', 'movie', 'effspr', 'objspr'}

# Folders the DC owns only in part; a shortfall here is known, not news.
PARTIAL = {'data': 'fonts, menu sheets and vendor logos the PS1 has no form of',
           'item_m2': 'PC-only item view art, plus the document pages - the DC '
                      'packs those into one FILEM.PIX whose every page is '
                      'byte-identical to the OG art, so falling back loses '
                      'nothing',
           'players': 'w08 / w18'}

STAGE_DIGITS = '123456789ABCDE'


def bg_stage_digit(stage_id):
    """Mirrors load_room_bg: the revisit fold fires on the FOLDED row."""
    row = stage_id - 7 if stage_id >= 7 else stage_id
    if row > 4:
        stage_id -= 5
    return STAGE_DIGITS[stage_id]


def listdir_lower(path):
    return {f.lower() for f in os.listdir(path)} if os.path.isdir(path) else set()


def check_coverage(base):
    print('1. base-tree files the DC tree does not carry')
    base_root = os.path.join(REPO, 'assets', base)
    dc_root = os.path.join(REPO, 'assets', 'DC')
    if not os.path.isdir(dc_root):
        print('   assets/DC does not exist - run tools/port_dc_assets.py')
        return ['assets/DC missing']
    problems = []
    for folder in sorted(os.listdir(base_root)):
        bpath = os.path.join(base_root, folder)
        if not os.path.isdir(bpath):
            continue
        low = folder.lower()
        have = listdir_lower(os.path.join(dc_root, folder))
        # Dev leftovers in the base tree (extracted .scd dumps, .bak, .ppm,
        # .tmext) are not game assets and are not the DC tree's business.
        IGNORE_EXT = ('.scd', '.bak', '.ppm', '.tmext', '.tga', '.bin')
        want = {f for f in listdir_lower(bpath)
                if not f.endswith(IGNORE_EXT)}
        missing = sorted(want - have)
        if low in FALLBACK_FOLDERS:
            print('   %-9s %4d missing  (expected - falls back by design)'
                  % (folder, len(missing)))
        elif low in PARTIAL:
            print('   %-9s %4d missing  (known partial: %s)'
                  % (folder, len(missing), PARTIAL[low]))
        elif low.startswith('stage'):
            # A stage folder's shortfall is two different things.
            #
            # Missing rc*.pak: either art a revisit stage reads from its source
            # stage, or a background for a room the DC does not ship. Check 2
            # walks every room the DC DOES ship through the engine's own path
            # rule, so it is the authority here and this is not double-counted.
            #
            # Missing room*.rdt: rooms the Director's Cut simply does not have -
            # the Jill (...1) variants it drops, and a few rooms it omits
            # outright (STAGE1 110, STAGE2 0C0/130, STAGE3 110). Those fall back
            # to the base room, which is the behaviour the port wants: the
            # alternative is a room that will not load at all.
            rooms = [m for m in missing if m.startswith('room') and m.endswith('.rdt')]
            others = [m for m in missing
                      if not m.startswith('rc') and m not in rooms]
            if rooms:
                print('   %-9s %4d room(s) the DC does not ship - fall back: %s%s'
                      % (folder, len(rooms), ' '.join(r[4:-4] for r in rooms[:8]),
                         ' ...' if len(rooms) > 8 else ''))
            if others:
                print('   %-9s %4d unexpected missing: %s' % (folder, len(others),
                      ' '.join(others[:8])))
                problems.append('%s is short %d unexpected file(s)'
                                % (folder, len(others)))
            if not rooms and not others:
                print('   %-9s complete' % folder)
        elif missing:
            print('   %-9s %4d missing' % (folder, len(missing)))
            print('        %s%s' % (' '.join(missing[:10]),
                                    ' ...' if len(missing) > 10 else ''))
            problems.append('%s is short %d file(s)' % (folder, len(missing)))
        else:
            print('   %-9s complete' % folder)
    return problems


def check_backgrounds():
    print('2. every DC room resolves to a background')
    dc_root = os.path.join(REPO, 'assets', 'DC')
    missing = []
    checked = 0
    for stage_id, digit in enumerate(STAGE_DIGITS):
        stage_dir = os.path.join(dc_root, 'Stage' + digit)
        if not os.path.isdir(stage_dir):
            continue
        src = bg_stage_digit(stage_id)
        rooms = sorted({int(f[5:7], 16) for f in os.listdir(stage_dir)
                        if re.fullmatch(r'ROOM[0-9A-Fa-f]{4}\.RDT', f, re.I)})
        for rid in rooms:
            checked += 1
            pak = os.path.join(dc_root, 'Stage' + src,
                               'RC%s%02X0.pak' % (src, rid))
            if not os.path.exists(pak):
                missing.append('Stage%s room 0x%02X -> %s'
                               % (digit, rid, os.path.relpath(pak, REPO)))
        if src != digit and rooms:
            print('   Stage%s (id %2d) reads Stage%s\'s art (revisit fold), %d room(s)'
                  % (digit, stage_id, src, len(rooms)))
    print('   %d room(s) checked, %d without a camera-0 background'
          % (checked, len(missing)))
    for m in missing[:12]:
        print('      %s' % m)
    return ['no background for ' + m for m in missing]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', default='USA', choices=['USA', 'JPN'])
    args = ap.parse_args()

    problems = check_coverage(args.base)
    print()
    problems += check_backgrounds()
    print()
    if problems:
        print('FAIL: %d problem(s)' % len(problems))
        for p in problems[:20]:
            print('   %s' % p)
        return 1
    print('ok: the DC tree covers everything it owns')
    return 0


if __name__ == '__main__':
    sys.exit(main())
