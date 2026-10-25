#!/usr/bin/env python3
"""dump_ivm.py - inspect a PS1 item view file (.IVM from ITEM_M2).

An .IVM is a TIM (the item's texture page) followed immediately by a TMD (the
item's model) - exactly what the port's FUN_00484420 splits:

    g_itemModelTmdBase = p + 8 + clutLen + imgLen

This tool renders the TIM part to a PNG and prints the TIM/TMD header facts, so
the Director's Cut's new item views can be identified and converted
(docs/DC_PORT.md 3g).

Usage:
    python tools/dump_ivm.py FILE.IVM [OUT.png] [--scale N]
"""
import argparse
import struct
import sys

from PIL import Image


def rgb555(v):
    return (((v >> 0) & 0x1F) * 255 // 31,
            ((v >> 5) & 0x1F) * 255 // 31,
            ((v >> 10) & 0x1F) * 255 // 31)


def parse(data):
    magic, flags = struct.unpack_from('<II', data, 0)
    if magic != 0x10:
        raise SystemExit(f'not a TIM (magic {magic:#x})')
    bpp_code = flags & 3
    off = 8
    clut = []
    if flags & 8:
        clut_len = struct.unpack_from('<I', data, off)[0]
        cx, cy, cw, ch = struct.unpack_from('<hhHH', data, off + 4)
        raw = data[off + 12:off + clut_len]
        clut = [rgb555(struct.unpack_from('<H', raw, 2 * i)[0])
                for i in range(len(raw) // 2)]
        print(f'TIM: flags={flags:#x} bpp={bpp_code} '
              f'CLUT at ({cx},{cy}) {cw}x{ch} = {len(clut)} entries')
        off += clut_len
    img_len = struct.unpack_from('<I', data, off)[0]
    ix, iy, iw, ih = struct.unpack_from('<hhHH', data, off + 4)
    pixels = data[off + 12:off + img_len]
    off += img_len
    if bpp_code == 0:
        pw, ph, bpp = iw * 4, ih, 4
    elif bpp_code == 1:
        pw, ph, bpp = iw * 2, ih, 8
    else:
        pw, ph, bpp = iw, ih, 16
    print(f'TIM image: vram=({ix},{iy}) {iw}x{ih} -> {pw}x{ph} at {bpp}bpp')
    print(f'TMD base offset: {off:#x} (file is {len(data)} B, '
          f'{len(data) - off} B of TMD)')
    return pw, ph, bpp, clut, pixels, off


def render(pw, ph, bpp, clut, pixels, scale):
    img = Image.new('RGB', (pw * scale, ph * scale), (32, 32, 32))
    px = img.load()
    for y in range(ph):
        for x in range(pw):
            if bpp == 8:
                idx = pixels[y * pw + x] if y * pw + x < len(pixels) else 0
                c = clut[idx] if idx < len(clut) else (255, 0, 255)
                if idx == 0:
                    c = (0, 0, 0)
            elif bpp == 4:
                b = pixels[y * (pw // 2) + x // 2]
                idx = (b & 0xF) if (x & 1) == 0 else (b >> 4)
                c = clut[idx] if idx < len(clut) else (255, 0, 255)
            else:
                c = rgb555(struct.unpack_from('<H', pixels, 2 * (y * pw + x))[0])
            for dy in range(scale):
                for dx in range(scale):
                    px[x * scale + dx, y * scale + dy] = c
    return img


def tmd_summary(data, base):
    """Print the TMD header/object table sizes (magic 0x41)."""
    if base + 12 > len(data):
        print('TMD: truncated')
        return
    magic, flags, nobj = struct.unpack_from('<III', data, base)
    print(f'TMD: magic={magic:#x} flags={flags} objects={nobj}')
    if magic != 0x41:
        return
    off = base + 12
    for i in range(min(nobj, 8)):
        vtop, vnum, nnum, pnum = struct.unpack_from('<IIII', data, off)
        print(f'  obj {i}: verts={vtop:#x}({vnum}) normals={nnum} prims={pnum}')
        off += 28 + vnum * 8 + nnum * 8 + pnum * 8


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ivm')
    ap.add_argument('out', nargs='?')
    ap.add_argument('--scale', type=int, default=2)
    args = ap.parse_args()

    data = open(args.ivm, 'rb').read()
    print(f'{args.ivm}: {len(data)} B')
    pw, ph, bpp, clut, pixels, tmd = parse(data)
    tmd_summary(data, tmd)
    if args.out:
        render(pw, ph, bpp, clut, pixels, args.scale).save(args.out)
        print(f'wrote {args.out}')


if __name__ == '__main__':
    sys.exit(main())
