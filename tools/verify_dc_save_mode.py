#!/usr/bin/env python3
"""verify_dc_save_mode.py - check the Director's Cut save-mode port.

The DC records the mode a save was made in and colour-codes the save screen by
it. Three pieces of that have to agree with data outside the source tree, and
each one fails quietly if it drifts:

1. **The card byte.** The mode lives at bio-card +0x233 (state block +0x33, PS1
   g_abDcGameMode), a byte the USA build never touches. This recomputes every
   BioCardLayout field offset from the struct itself and checks it against the
   offset each field's own comment claims - so `dcGameMode` really is 0x233,
   and nothing above it has shifted - then checks SaveLoadScreen.cpp's
   OFFSET_DC_MODE against it and confirms bio_card.dat has a zero there (a new
   game, and any USA save, must read as STANDARD).

2. **The mode -> flag-bit mapping**, against the PS1's own load path
   (SLUS_005.51 0x80019638): 1 = TRAINING 0x40000, 2 = ADVANCED 0x20000,
   3 = both. Read through the Ghidra bridge, skipped if it is not up.

3. **The font CLUT.** The colours are four palettes the port selects by row.
   This checks the generated overlay font against both sources: its glyph block
   must be the base tree's byte for byte, row 0 must be the base font's own
   palette, and rows 1-3 must be the PS1 FONT.TIM's colour columns 1-3.

4. **The ADVANCED best-ending infinite Colt Python.** The ending raises
   `DC_SCENARIO_FLAG_INF_COLT_PYTHON` (0x7A); `dc_is_infinite_colt_python`
   (`dc/Items.cpp`) is the port's single reader, consumed by the ammo check,
   the quantity glyph and the two empty-click paths. This checks the helper and
   its call sites, then the two PS1 sites through the bridge: the ammo check
   (SLUS_005.51 0x8004228c) must test 0x7a and refill to 6, the viewer
   (0x8005436c) must test 0x7a for item 5.

    python tools/verify_dc_save_mode.py
"""
import json
import os
import re
import struct
import sys
import urllib.error
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRIDGE = 'http://127.0.0.1:8089/decompile_function'
PROGRAM = 'SLUS_005.51'
DC_LOAD_MODE_FN = '0x80019638'
DC_PYTHON_AMMO_FN = '0x8004228c'
DC_PYTHON_QTY_FN = '0x8005436c'

BIOCARD_H = os.path.join(REPO, 'src', 'game', 'BioCard.h')
SAVE_SRC = os.path.join(REPO, 'src', 'game', 'SaveLoadScreen.cpp')
PS_FONT = os.path.join(REPO, 'assets', 'PSX_DC', 'DATA', 'FONT.TIM')

DC_MODE_OFFSET = 0x233
CLUT_COLOURS = 16
CLUT_ROWS = 4

# base tree -> the font file that tree's build reads (GameInit.cpp).
FONTS = {'USA': 'fontus.tim', 'JPN': 'FONT.TIM'}

# The mode byte the DC's load path turns into each g_status_flags mask.
PS1_MODE_BITS = {1: 0x40000, 2: 0x20000, 3: 0x30000}

SIZES = {'unsigned char': 1, 'char': 1, 'BYTE': 1, 'short': 2,
         'unsigned short': 2, 'DWORD': 4, 'int': 4, 'unsigned int': 4,
         'ItemSlot': 2}   # id + qty, the PS1's `slot * 2` indexing

failures = []


def fail(msg):
    failures.append(msg)
    print('  FAIL %s' % msg)


def ok(msg):
    print('  ok   %s' % msg)


# ---------------------------------------------------------------------------
# 1. The card layout
# ---------------------------------------------------------------------------
FIELD_RE = re.compile(
    r'^\s{4}([A-Za-z_][\w ]*?)\s+([A-Za-z_]\w*)(?:\[(0[xX][0-9A-Fa-f]+|\d+)\])?;'
    r'\s*//\s*0x([0-9A-Fa-f]+)')


def check_layout():
    print('1. the bio-card layout and the mode byte')
    text = open(BIOCARD_H, encoding='utf-8', errors='replace').read()
    start = text.index('struct BioCardLayout {')
    end = text.index('};', start)
    off = 0
    fields = {}
    checked = 0
    for line in text[start:end].splitlines():
        m = FIELD_RE.match(line)
        if not m:
            continue
        ctype, name, count, want = m.group(1).strip(), m.group(2), m.group(3), int(m.group(4), 16)
        if ctype not in SIZES:
            fail('unknown field type %r on %s - the offset walk stops here'
                 % (ctype, name))
            return None
        if off != want:
            fail('%s is at 0x%X, its comment says 0x%X' % (name, off, want))
            return None
        fields[name] = off
        off += SIZES[ctype] * (int(count, 0) if count else 1)
        checked += 1
    ok('%d field offsets match their comments, total 0x%X bytes' % (checked, off))

    if fields.get('dcGameMode') != DC_MODE_OFFSET:
        fail('dcGameMode is at 0x%X, the DC puts its mode byte at 0x%X'
             % (fields.get('dcGameMode', -1), DC_MODE_OFFSET))
    else:
        ok('dcGameMode is card +0x%03X (PS1 state block +0x33)' % DC_MODE_OFFSET)

    save = open(SAVE_SRC, encoding='utf-8', errors='replace').read()
    m = re.search(r'#define OFFSET_DC_MODE\s+0x([0-9A-Fa-f]+)', save)
    if not m or int(m.group(1), 16) != DC_MODE_OFFSET:
        fail('SaveLoadScreen.cpp OFFSET_DC_MODE is not 0x%X' % DC_MODE_OFFSET)
    else:
        ok('SaveLoadScreen.cpp reads the slot mode from the same offset')

    # ADVANCED* shares ADVANCED's palette; the caller adds its extra pass.
    if not re.search(r'mode == DC_DIFFICULTY_ADVANCED_HOLD\s*\)\s*\{\s*'
                     r'mode = DC_DIFFICULTY_ADVANCED;', save):
        fail('DcSaveRowColour no longer folds ADVANCED* onto the ADVANCED row')
    else:
        ok('ADVANCED* draws in the ADVANCED palette, plus its 0x11 pass')

    for tree in ('USA', 'JPN', os.path.join('DC')):
        card = _find_ci(os.path.join(REPO, 'assets', tree, 'Data'), 'bio_card.dat')
        if card is None:
            continue
        with open(card, 'rb') as f:
            data = f.read()
        if len(data) <= DC_MODE_OFFSET:
            fail('%s is only %d bytes' % (card, len(data)))
        elif data[DC_MODE_OFFSET] != 0:
            fail('%s has 0x%02X at +0x%X; a new game would not start in STANDARD'
                 % (os.path.relpath(card, REPO), data[DC_MODE_OFFSET], DC_MODE_OFFSET))
        else:
            ok('%s reads STANDARD at +0x%X'
               % (os.path.relpath(card, REPO), DC_MODE_OFFSET))
    ending = open(os.path.join(REPO, 'src', 'game', 'EndingScreen.cpp'),
                  encoding='utf-8', errors='replace').read()
    reset = ending.index('LoadFile(GAME_DATA_ROOT "data\\\\bio_card.dat", g_BioCardData, 0x20);')
    initial_items = ending.index('SetInitialItems();', reset)
    if not re.search(r'if\s*\(g_bDcMode\)\s*\{\s*'
                     r'g_DcGameMode\s*=\s*\(unsigned char\)g_DcDifficulty\s*;',
                     ending[reset:initial_items]):
        fail('ending reset does not restore the completed run\'s DC mode before rebuilding the save')
    else:
        ok('cleared saves restore the DC difficulty after the bio-card template reset')
    return fields


# ---------------------------------------------------------------------------
# 2. The PS1's own mode -> bits mapping
# ---------------------------------------------------------------------------
def check_ps1_mapping():
    print('2. the mode byte -> mode bits, against SLUS_005.51 %s' % DC_LOAD_MODE_FN)
    url = '%s?program=%s&address=%s' % (BRIDGE, PROGRAM, DC_LOAD_MODE_FN)
    try:
        with urllib.request.urlopen(url, timeout=60) as r:
            raw = r.read().decode('utf-8', 'replace')
    except (urllib.error.URLError, OSError) as exc:
        print('  SKIP Ghidra bridge not reachable (%s)' % exc)
        return
    # The bridge answers this endpoint with the pseudocode itself, not JSON.
    try:
        body = json.loads(raw)
        src = body.get('result', raw) if isinstance(body, dict) else raw
    except ValueError:
        src = raw
    for mode, mask in sorted(PS1_MODE_BITS.items()):
        pat = re.compile(r'==\s*%d\)\s*\{[^}]*?\|\s*0x%x' % (mode, mask), re.S)
        if pat.search(src):
            ok('mode %d -> 0x%05X' % (mode, mask))
        else:
            fail('mode %d does not OR 0x%05X in the PS1 load path' % (mode, mask))

    # And the port derives the same three from g_DcDifficulty.
    gs = open(os.path.join(REPO, 'src', 'game', 'GameStart.cpp'),
              encoding='utf-8', errors='replace').read()
    for want in ('case DC_DIFFICULTY_TRAINING:', 'case DC_DIFFICULTY_ADVANCED:',
                 'case DC_DIFFICULTY_ADVANCED_HOLD:'):
        if want not in gs:
            fail('dc_apply_mode_flags has no %s' % want)
    if 'g_DcDifficulty = (g_DcGameMode' not in gs:
        fail('dc_apply_mode_flags does not adopt the save\'s mode byte')
    else:
        ok('a continued game takes its mode from the card, not config.ini')


# ---------------------------------------------------------------------------
# 3. The font CLUT rows
# ---------------------------------------------------------------------------
def _find_ci(dirname, name):
    if not os.path.isdir(dirname):
        return None
    for f in os.listdir(dirname):
        if f.lower() == name.lower():
            return os.path.join(dirname, f)
    return None


def tim_clut(data):
    blen, x, y, w, h = struct.unpack_from('<IHHHH', data, 8)
    pal = data[20:20 + w * h * 2]
    rows = [struct.unpack_from('<%dH' % w, pal, r * w * 2) for r in range(h)]
    return (x, y, w, h), rows, data[8 + blen:]


def check_fonts():
    print('3. the DC font CLUT rows')
    if not os.path.exists(PS_FONT):
        print('  SKIP no assets/PSX_DC/DATA/FONT.TIM')
        return
    with open(PS_FONT, 'rb') as f:
        ps_clut, ps_rows, _ = tim_clut(f.read())

    for tree, name in sorted(FONTS.items()):
        dc_path = _find_ci(os.path.join(REPO, 'assets', 'DC', 'Data'), name)
        base_path = _find_ci(os.path.join(REPO, 'assets', tree, 'Data'), name)
        if base_path is None:
            continue
        if dc_path is None:
            fail('the overlay has no %s - DC mode would fall back to the base'
                 ' font and every slot row would draw in one colour' % name)
            continue
        with open(dc_path, 'rb') as f:
            dc_clut, dc_rows, dc_pixels = tim_clut(f.read())
        with open(base_path, 'rb') as f:
            base_clut, base_rows, base_pixels = tim_clut(f.read())

        if dc_pixels != base_pixels:
            fail('%s: the overlay font\'s glyph block is not the base tree\'s'
                 % name)
        if dc_clut[:2] != base_clut[:2]:
            fail('%s: the overlay CLUT sits at %s, the base at %s'
                 % (name, dc_clut[:2], base_clut[:2]))
        if dc_clut[2:] != (CLUT_COLOURS, CLUT_ROWS):
            fail('%s: the overlay CLUT is %dx%d, expected %dx%d'
                 % (name, dc_clut[2], dc_clut[3], CLUT_COLOURS, CLUT_ROWS))
            continue

        # Row 0 must be what the base font already drew with.
        if list(dc_rows[0]) != list(base_rows[0][:CLUT_COLOURS]):
            fail('%s: row 0 is not the base font\'s own palette - DC mode would'
                 ' recolour every string in the game' % name)
        else:
            ok('%s row 0 = the base font\'s palette' % name)

        # Rows 1-3 are the PS1's colour columns 1-3. A JPN base ships that same
        # wide CLUT, so it is its own source; the USA's has one palette.
        src_rows = base_rows[0] if base_clut[2] >= CLUT_ROWS * CLUT_COLOURS else ps_rows[0]
        src_name = name if base_clut[2] >= CLUT_ROWS * CLUT_COLOURS else 'the PS1 FONT.TIM'
        for row in range(1, CLUT_ROWS):
            want = list(src_rows[row * CLUT_COLOURS:(row + 1) * CLUT_COLOURS])
            if list(dc_rows[row]) != want:
                fail('%s: row %d is not %s column %d' % (name, row, src_name, row))
            else:
                ok('%s row %d = %s column %d (%s)'
                   % (name, row, src_name, row,
                      ('TRAINING', 'ADVANCED', 'ADVANCED*')[row - 1]))


# ---------------------------------------------------------------------------
# 4. The ADVANCED best-ending infinite Colt Python (flag 0x7A)
# ---------------------------------------------------------------------------
def _bridge_decompile(address):
    """The bridge's pseudocode for one function, or None if it is not up."""
    url = '%s?program=%s&address=%s' % (BRIDGE, PROGRAM, address)
    try:
        with urllib.request.urlopen(url, timeout=60) as r:
            raw = r.read().decode('utf-8', 'replace')
    except (urllib.error.URLError, OSError) as exc:
        print('  SKIP Ghidra bridge not reachable (%s)' % exc)
        return None
    try:
        body = json.loads(raw)
        return body.get('result', raw) if isinstance(body, dict) else raw
    except ValueError:
        return raw


def check_infinite_python():
    print('4. the ADVANCED best-ending infinite Colt Python (flag 0x7A)')
    # The helper is the one flag reader; the four DC sites all consume it.
    items = open(os.path.join(REPO, 'src', 'game', 'dc', 'Items.cpp'),
                 encoding='utf-8', errors='replace').read()
    helper = re.search(r'int dc_is_infinite_colt_python\(.*?\n\}', items, re.S)
    if (helper is None
            or 'DC_SCENARIO_FLAG_INF_COLT_PYTHON' not in helper.group(0)
            or 'ITEM_COLT_PYTHON_MAG' not in helper.group(0)):
        fail('dc_is_infinite_colt_python does not test flag 0x7A and item 5')
    else:
        ok('dc_is_infinite_colt_python reads flag 0x7A for item 5')

    calls = {
        os.path.join('src', 'game', 'PlayerAnimations.cpp'): 3,  # ammo + 2 clicks
        os.path.join('src', 'game', 'MainMenu.cpp'): 1,          # quantity glyph
    }
    for path, want in sorted(calls.items()):
        src = open(os.path.join(REPO, path), encoding='utf-8', errors='replace').read()
        n = src.count('dc_is_infinite_colt_python')
        if n < want:
            fail('%s calls dc_is_infinite_colt_python %d time(s), expected %d'
                 % (path, n, want))
        else:
            ok('%s calls it %d time(s)' % (path, n))

    src = _bridge_decompile(DC_PYTHON_AMMO_FN)
    if src is not None:
        if not re.search(r'Flg_ck\([^)]*,\s*0x7a\)', src):
            fail('%s does not test flag 0x7a' % DC_PYTHON_AMMO_FN)
        elif not (re.search(r'=\s*6;', src) and re.search(r'return\s+6;', src)):
            fail('%s does not refill the magnum to 6' % DC_PYTHON_AMMO_FN)
        else:
            ok('%s refills the magnum to 6 on flag 0x7a' % DC_PYTHON_AMMO_FN)

    src = _bridge_decompile(DC_PYTHON_QTY_FN)
    if src is not None:
        if not (re.search(r'Flg_ck\([^)]*,\s*0x7a\)', src)
                and re.search(r'!=\s*5\)', src)):
            fail('%s does not special-case item 5 on flag 0x7a' % DC_PYTHON_QTY_FN)
        else:
            ok('%s draws the infinity glyph for item 5 on flag 0x7a'
               % DC_PYTHON_QTY_FN)


def main():
    check_layout()
    check_ps1_mapping()
    check_fonts()
    check_infinite_python()
    print()
    if failures:
        print('%d check(s) FAILED' % len(failures))
        return 1
    print('ok: the DC save-mode port matches the disc and the PS1 build')
    return 0


if __name__ == '__main__':
    sys.exit(main())
