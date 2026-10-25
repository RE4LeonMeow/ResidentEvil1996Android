#!/usr/bin/env python3
"""dump_item_pix.py - render rows of an item sprite sheet (ITEM_ALL.PIX /
Item_all_dc.pix) using the STATUS.TIM palette, as a PNG grid.

The item sheets are 8bpp index data, 1200 bytes (40x30) per row, with the
palette coming from STATUS.TIM (its CLUT block holds three 256-entry rows; the
port's LoadImage shim composites with row 2 - see TextureLoader.cpp). This is a
verification tool for the DC item-sprite port (docs/DC_PORT.md 3f).

Usage:
    python tools/dump_item_pix.py <sheet.pix> <status.tim> <out.png> ROW [ROW...]
    python tools/dump_item_pix.py <sheet.pix> <status.tim> <out.png> --all
"""
import argparse
import struct
import sys

from PIL import Image

ROW_BYTES = 1200
SPR_W, SPR_H = 40, 30


def read_clut_row(path, row=2):
    """Return the TIM's CLUT row `row` as a list of (r, g, b)."""
    data = open(path, 'rb').read()
    magic, ver = struct.unpack_from('<II', data, 0)
    if magic != 0x10:
        raise SystemExit(f'{path}: not a TIM (magic {magic:#x})')
    if not (ver & 8):
        raise SystemExit(f'{path}: no CLUT block')
    off = 8
    clut_len = struct.unpack_from('<I', data, off)[0]
    cx, cy, cw, ch = struct.unpack_from('<hhHH', data, off + 4)
    if row >= ch:
        raise SystemExit(f'{path}: CLUT has {ch} row(s), asked for {row}')
    base = off + 12 + row * cw * 2
    out = []
    for i in range(cw):
        v = struct.unpack_from('<H', data, base + 2 * i)[0]
        out.append((((v >> 0) & 0x1F) * 255 // 31,
                    ((v >> 5) & 0x1F) * 255 // 31,
                    ((v >> 10) & 0x1F) * 255 // 31))
    print(f'{path}: CLUT at ({cx},{cy}) {cw}x{ch}, using row {row}')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('sheet')
    ap.add_argument('status')
    ap.add_argument('out')
    ap.add_argument('rows', nargs='+')
    ap.add_argument('--clut-row', type=int, default=2)
    ap.add_argument('--scale', type=int, default=4)
    args = ap.parse_args()

    pal = read_clut_row(args.status, args.clut_row)
    data = open(args.sheet, 'rb').read()
    total = len(data) // ROW_BYTES
    print(f'{args.sheet}: {len(data)} B = {total} rows of {ROW_BYTES}')

    rows = list(range(total)) if args.rows == ['--all'] else [int(r, 0) for r in args.rows]
    bad = [r for r in rows if r >= total]
    if bad:
        raise SystemExit(f'row(s) past the sheet: {bad} (sheet has {total})')

    cols = min(len(rows), 8)
    grid_rows = (len(rows) + cols - 1) // cols
    s = args.scale
    img = Image.new('RGB', (cols * SPR_W * s, grid_rows * SPR_H * s), (24, 24, 24))
    px = img.load()
    for n, r in enumerate(rows):
        base = r * ROW_BYTES
        gx, gy = (n % cols) * SPR_W * s, (n // cols) * SPR_H * s
        for y in range(SPR_H):
            for x in range(SPR_W):
                idx = data[base + y * SPR_W + x]
                c = pal[idx] if idx < len(pal) else (255, 0, 255)
                if idx == 0:
                    c = (0, 0, 0)
                for dy in range(s):
                    for dx in range(s):
                        px[gx + x * s + dx, gy + y * s + dy] = c
    img.save(args.out)
    order = ', '.join(str(r) for r in rows)
    print(f'wrote {args.out} ({cols} per row, order: {order})')


if __name__ == '__main__':
    sys.exit(main())
