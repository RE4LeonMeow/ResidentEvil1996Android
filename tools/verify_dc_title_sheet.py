#!/usr/bin/env python3
"""verify_dc_title_sheet.py - the in-engine DC title sheet assembly.

`dc_title_build_page` in src/game/TitleScreen.cpp builds the two title option
texture pages at runtime, straight out of the disc's DATA/BT367OAB.TIM, so that
the DC asset tree carries the file the player's own disc has rather than two
pre-split ones.

This re-implements that function byte-for-byte in Python and checks it against
a known-good reference: the digests below are of the sheets
tools/port_dc_assets.py used to emit (t_dc.tim / t_dc2.tim), which were
rendered and confirmed against the PS1 frames before the split moved into the
engine. A match means the C produces exactly that already-verified art.

The reference is a digest rather than a file on purpose - the split sheets no
longer ship, so there is nothing for this check to drift against.

Usage: python tools/verify_dc_title_sheet.py
"""
import argparse
import hashlib
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, 'assets', 'PSX_DC', 'DATA', 'BT367OAB.TIM')

CHUNK = 0x2840
CELL_W = 256
CELL_H = 80
ROW_BYTES = CELL_W // 2
CLUT_OFF = 0x14
PIX_OFF = 0x40
GREEN_SLOT = 5


def build_page(file_bytes, cells, rows, pal_from, green_from):
    """Mirror of dc_title_build_page (TitleScreen.cpp)."""
    count = len(cells)
    h = rows * count
    pix_bytes = CELL_W * h // 2

    out = bytearray()
    out += struct.pack('<II', 0x10, 8)                    # magic, 4bpp + CLUT
    out += struct.pack('<IhhHH', 12 + 32, 0, 0x1E0, 16, 1)
    clut = bytearray(file_bytes[cells[pal_from] * CHUNK + CLUT_OFF:
                                cells[pal_from] * CHUNK + CLUT_OFF + 32])
    if green_from >= 0:
        base = cells[green_from] * CHUNK + CLUT_OFF + 2
        clut[GREEN_SLOT * 2:GREEN_SLOT * 2 + 2] = file_bytes[base:base + 2]
    out += clut
    out += struct.pack('<IhhHH', 12 + pix_bytes, 0, 0, CELL_W // 4, h)

    for i, c in enumerate(cells):
        n = rows * ROW_BYTES
        cell = bytearray(file_bytes[c * CHUNK + PIX_OFF:c * CHUNK + PIX_OFF + n])
        if i == green_from:
            for k in range(n):
                lo = cell[k] & 0xF
                hi = cell[k] >> 4
                if lo == 1:
                    lo = GREEN_SLOT
                if hi == 1:
                    hi = GREEN_SLOT
                cell[k] = lo | (hi << 4)
        out += cell
    return bytes(out)


# label, source cells, rows per cell, palette cell, green-remap cell, size, sha256
PAGES = [
    ('page A (slot 12, main menu)',  [0, 1, 2],    CELL_H, 1, -1, 30784,
     '93307334f1462fb8f6175fc0395dcb68ea87eb5df496f41d8486ed1dd9b599d2'),
    ('page B (slot 13, difficulty)', [3, 4, 5, 6], 64,     0,  3, 32832,
     'fc962105fd2a0961ae5ceac35cce1978f38e586d6b1e453a0f0a7e5e4ff74c9a'),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.parse_args()

    if not os.path.exists(SRC):
        print('%s not present - nothing to check' % os.path.relpath(SRC, REPO))
        return 0
    data = open(SRC, 'rb').read()
    cells = len(data) // CHUNK
    print('BT367OAB.TIM: %d B = %d cells of %d B' % (len(data), cells, CHUNK))
    if cells != 7 or len(data) % CHUNK:
        print('FAIL: expected exactly 7 whole cells')
        return 1

    bad = 0
    for label, sel, rows, pal, green, want_len, want_sha in PAGES:
        got = build_page(data, sel, rows, pal, green)
        sha = hashlib.sha256(got).hexdigest()
        if len(got) == want_len and sha == want_sha:
            print('  %-32s %6d B  matches the verified sheet' % (label, len(got)))
        else:
            bad += 1
            print('  %-32s %d B / %s' % (label, len(got), sha[:16]))
            print('  %-32s expected %d B / %s' % ('', want_len, want_sha[:16]))
    print()
    if bad:
        print('FAIL: the in-engine assembly does not match the reference')
        return 1
    print('ok: TitleScreen.cpp builds the same bytes the split sheets held')
    return 0


if __name__ == '__main__':
    sys.exit(main())
