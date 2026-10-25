#!/usr/bin/env python3
"""Decode the JPN PS1 prologue FMV subtitles (JIMAKU*.RGB) and the cue tables.

Reverse-engineered from SLPS_009.98 (Biohazard / Biohazard Director's Cut,
SLPS_009.98) with the Ghidra MCP; see docs/PS1_FMV_SUBTITLES.md.

Data layout (all confirmed against the original, not guessed):

  JIMAKU*.RGB  a flat array of 320x18 pixel lines, 24 bpp greyscale (R=G=B,
               3 bytes per pixel, the value is byte 0), plain row-major --
               the whole file is one 320-wide image.
                   320 * 18 * 3 = 17280 bytes per line
                   JIMAKU00.RGB 397440 B = 23 lines
                   JIMAKU01.RGB 120960 B =  7 lines
                   JIMAKU02.RGB  86400 B =  5 lines

  cue table    0x10-byte records in the resident exe, walked by
               ProcessSubtitleCues (0x80037c44) in SLPS_009.98:
                   +0x00 u16 startFrame
                   +0x04 u16 duration
                   +0x08 u16 lineIndex      (line inside the JIMAKU file)
                   +0x0A u16 lineCount      (raster height = lineCount * 18)
                   +0x0C u16 destX
                   +0x0E u16 destY
                   +0x10 u32 0xFFFFFFFF     end-of-track marker

The subtitle blit (BlitSubtitleLine4bppTo24bpp, 0x80037d98) copies byte 0 of
each 3-byte group into all three destination bytes, i.e. the greyscale value
survives verbatim and a pixel whose three source bytes are all zero is
transparent.  The tool keeps that level and writes each file as one 8-bit
greyscale PNG, 320 wide, the 18-row lines stacked top to bottom
(jimakuNN.png).  The PNG is filter 0 with stored deflate blocks, byte-identical
to what tools/asset_migrator writes.

Usage (from the repo root):
    python tools/decode_jimaku.py --iso <BiohazardDirectrorsCut.img> \\
        --exe assets/PSX_JPNDC/SLPS_009.98 --track 0 --out <dir>
"""
import argparse
import os
import struct
import sys
import zlib

LINE_W = 320
LINE_H = 18
LINE_BYTES = LINE_W * LINE_H * 3          # 17280

# Track directory in SLPS_009.98: DAT_80090490, 8 bytes per track.
#   +0 u32 cue-table pointer, +4 u32 CD file index (18/19/20 = JIMAKU00/01/02)
TRACK_DIR_VA = 0x80090490
JIMAKU_FILE_FOR_INDEX = {18: "JIMAKU00.RGB", 19: "JIMAKU01.RGB", 20: "JIMAKU02.RGB"}


def decode_line(raw):
    """One 17280-byte line -> 320x18 bytes (8-bit level).

    Plain row-major: the blit walks the source 3 bytes at a time in the same
    order as the 320-wide 24 bpp framebuffer rect, and only byte 0 is read."""
    return bytes(raw[0:LINE_BYTES:3])


def write_png(path, width, height, pixels):
    """8-bit greyscale PNG, filter 0, stored deflate blocks (no compression),
    laid out exactly like encodeGreyPng in tools/asset_migrator/src/core/Png.cpp."""
    raw = b"".join(b"\x00" + pixels[y * width:(y + 1) * width] for y in range(height))
    z = bytearray(b"\x78\x01")
    pos = 0
    while True:
        n = min(65535, len(raw) - pos)
        final = 1 if pos + n == len(raw) else 0
        z += struct.pack("<BHH", final, n, n ^ 0xFFFF)
        z += raw[pos:pos + n]
        pos += n
        if pos >= len(raw):
            break
    z += struct.pack(">I", zlib.adler32(raw) & 0xFFFFFFFF)

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
                chunk(b"IDAT", bytes(z)) + chunk(b"IEND", b""))


def read_cue_table(exe, table_va):
    """Walk a 0x10-byte cue table until the 0xFFFFFFFF end marker."""
    off = 0x800 + (table_va - 0x80010000)
    cues = []
    while len(cues) < 4096:
        if off + 20 > len(exe):
            break
        start, _, dur, _, line, lines, x, y = struct.unpack_from("<8H", exe, off)
        end_marker = struct.unpack_from("<i", exe, off + 16)[0]
        cues.append((start, dur, line, lines, x, y))
        if end_marker == -1:
            break
        off += 16
    return cues


def read_track(exe, track):
    off = 0x800 + (TRACK_DIR_VA - 0x80010000) + track * 8
    table_va, file_index = struct.unpack_from("<II", exe, off)
    return table_va, file_index


def iso_sector(img, lba):
    return img[lba * 2352 + 24: lba * 2352 + 24 + 2048]


def read_iso_file(img, lba, size):
    data = bytearray()
    sectors = (size + 2047) // 2048
    for s in range(sectors):
        data += iso_sector(img, lba + s)
    return bytes(data[:size])


def iso_files(img):
    pvd = iso_sector(img, 16)
    root = pvd[156:190]
    lba = struct.unpack_from("<I", root, 2)[0]
    out = {}

    def walk(dir_lba, path):
        for s in range(0, 8):
            block = iso_sector(img, dir_lba + s)
            if not block or block[0] == 0:
                break
            pos = 0
            while pos < len(block):
                rec_len = block[pos]
                if rec_len == 0:
                    break
                name_len = block[pos + 32]
                name = block[pos + 33: pos + 33 + name_len]
                child = path + "/" + name.decode("latin1")
                child_lba = struct.unpack_from("<I", block, pos + 2)[0]
                child_size = struct.unpack_from("<I", block, pos + 10)[0]
                if name_len == 1 and name in (b"\x00", b"\x01"):
                    child = path
                out[child] = (child_lba, child_size)
                pos += rec_len
        return out

    walk(lba, "")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iso", required=True, help="Biohazard Director's Cut disc image")
    ap.add_argument("--exe", required=True, help="extracted SLPS_009.98")
    ap.add_argument("--track", type=int, default=0, help="subtitle track id (default 0)")
    ap.add_argument("--out", required=True, help="output directory")
    args = ap.parse_args()

    exe = open(args.exe, "rb").read()
    os.makedirs(args.out, exist_ok=True)

    table_va, file_index = read_track(exe, args.track)
    name = JIMAKU_FILE_FOR_INDEX.get(file_index)
    if name is None:
        print("track %d -> file index %d, which is not a JIMAKU file" %
              (args.track, file_index))
        return 1
    print("track %d: cue table 0x%08x, file index %d = %s" %
          (args.track, table_va, file_index, name))

    cues = read_cue_table(exe, table_va)
    print("  %d cues" % len(cues))

    files = iso_files(open(args.iso, "rb").read())
    want = "/PSX/DATA/" + name + ";1"
    if want not in files:
        cand = [k for k in files if k.endswith("/" + name + ";1")]
        if not cand:
            print("  %s not found on the disc" % name)
            return 1
        want = cand[0]
    lba, size = files[want]
    data = read_iso_file(open(args.iso, "rb").read(), lba, size)
    line_count = len(data) // LINE_BYTES
    print("  %s: lba 0x%05x, %d bytes, %d lines" % (want, lba, len(data), line_count))

    # decoded lines stacked into one 320-wide greyscale image
    atlas = bytearray()
    for i in range(line_count):
        atlas += decode_line(data[i * LINE_BYTES:(i + 1) * LINE_BYTES])
    atlas_path = os.path.join(args.out, "jimaku%s.png" % name[6:8])
    write_png(atlas_path, LINE_W, line_count * LINE_H, bytes(atlas))
    print("  wrote %s (%dx%d = %d lines of %dx%d)" %
          (atlas_path, LINE_W, line_count * LINE_H, line_count, LINE_W, LINE_H))

    # C++ side: cue table, as consumed by Ps1FmvSubtitles
    rows = []
    for (start, dur, line, lines, x, y) in cues:
        rows.append("    { %4d, %4d, %2d, %d, %3d, %3d }," %
                    (start, dur, line, lines, x, y))
    hdr = os.path.join(args.out, "jimaku_track%d.h" % args.track)
    with open(hdr, "w") as f:
        f.write("// Generated by tools/decode_jimaku.py - do not edit.\n")
        f.write("// Biohazard DC (SLPS_009.98) subtitle track %d -> %s.\n"
                % (args.track, name))
        f.write("#pragma once\n\n")
        f.write("struct FmvSubtitleCue { short startFrame; short duration;"
                " short line; short lines; short x; short y; };\n\n")
        f.write("static const FmvSubtitleCue kFmvSubtitleCues[] = {\n")
        f.write("\n".join(rows))
        f.write("\n};\n")
    print("  wrote %s" % hdr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
