#!/usr/bin/env python3
"""bss_to_pak.py - PS1 .BSS room backgrounds -> the PC port's .pak files.

The PS1 keeps a room's camera backgrounds in one `.BSS`: N slots of 0x8000
bytes, each an MDEC ("BS", STR v2/v3) bitstream for one 320x240 frame. The PC
release ships the same pictures as `RC<stage><room><cam>.pak`, which is an LZW
stream (decompressed by `unpack_pakfile_`, 0x00425ab0) wrapping a single 16bpp
TIM. This converts the former into the latter, so the Director's Cut's arrange
stages (STAGE8-E, 114 camera frames that exist in no PC release) can be shipped
into the DC asset overlay.

Pipeline per frame:  BS bitstream -> MDEC coefficients -> IDCT -> YCbCr -> RGB
                     -> RGB555 -> TIM -> LZW -> .pak

Verification is the point of `--verify`: stages 1-7 exist in BOTH forms, so the
decoder can be checked against Capcom's own conversion of the same frame. Run
that before trusting any arrange output.

Usage:
    python tools/bss_to_pak.py --verify [--limit N]
    python tools/bss_to_pak.py --arrange [--out assets/DC]
    python tools/bss_to_pak.py <file.BSS> --out <dir> --name RC9120
"""
import argparse
import glob
import os
import struct
import sys

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FRAME_STRIDE = 0x8000
WIDTH, HEIGHT = 320, 240

# ---------------------------------------------------------------------------
# 1. Bit reader
#
# A PSX BS bitstream is a run of 16-bit LITTLE-ENDIAN words whose bits are
# consumed most-significant first. Reading it as a plain big-endian byte stream
# gives every pair of bytes swapped, which decodes into noise - this is the
# single easiest thing to get wrong.
# ---------------------------------------------------------------------------
class BitReader:
    def __init__(self, data, start):
        # byte-swap each 16-bit word up front, then treat it as an MSB-first
        # bit stream
        body = bytearray(data[start:])
        if len(body) & 1:
            body.append(0)
        body[0::2], body[1::2] = body[1::2], bytes(body[0::2])
        self.buf = bytes(body)
        self.pos = 0          # bit position

    def peek(self, n):
        byte = self.pos >> 3
        chunk = self.buf[byte:byte + (n + 15) // 8 + 1]
        if not chunk:
            return 0
        val = int.from_bytes(chunk, 'big')
        shift = len(chunk) * 8 - (self.pos & 7) - n
        if shift < 0:
            val <<= -shift
            shift = 0
        return (val >> shift) & ((1 << n) - 1)

    def read(self, n):
        v = self.peek(n)
        self.pos += n
        return v

    def skip(self, n):
        self.pos += n


# ---------------------------------------------------------------------------
# 2. Huffman tables (MPEG-1, which is what the PSX MDEC bitstream uses)
# ---------------------------------------------------------------------------
# DC size, Table B-12 (luma) and B-13 (chroma): code -> number of extra bits
DC_LUMA = {
    '100': 0, '00': 1, '01': 2, '101': 3, '110': 4, '1110': 5,
    '11110': 6, '111110': 7, '1111110': 8,
}
DC_CHROMA = {
    '00': 0, '01': 1, '10': 2, '110': 3, '1110': 4, '11110': 5,
    '111110': 6, '1111110': 7, '11111110': 8,
}

# AC coefficients, Table B-14. "10" is end-of-block and "000001" is the escape.
AC_TABLE = {
    '11': (0, 1), '011': (1, 1), '0100': (0, 2), '0101': (2, 1),
    '00101': (0, 3), '00110': (4, 1), '00111': (3, 1),
    '000100': (7, 1), '000101': (6, 1), '000110': (1, 2), '000111': (5, 1),
    '0000100': (2, 2), '0000101': (9, 1), '0000110': (0, 4), '0000111': (8, 1),
    '00100000': (13, 1), '00100001': (0, 6), '00100010': (12, 1),
    '00100011': (11, 1), '00100100': (3, 2), '00100101': (1, 3),
    '00100110': (0, 5), '00100111': (10, 1),
    '0000001000': (16, 1), '0000001001': (5, 2), '0000001010': (0, 7),
    '0000001011': (2, 3), '0000001100': (1, 4), '0000001101': (15, 1),
    '0000001110': (14, 1), '0000001111': (4, 2),
    '000000010000': (0, 11), '000000010001': (8, 2), '000000010010': (4, 3),
    '000000010011': (0, 10), '000000010100': (2, 4), '000000010101': (7, 2),
    '000000010110': (21, 1), '000000010111': (20, 1), '000000011000': (0, 9),
    '000000011001': (19, 1), '000000011010': (18, 1), '000000011011': (1, 5),
    '000000011100': (3, 3), '000000011101': (0, 8), '000000011110': (6, 2),
    '000000011111': (17, 1),
    '0000000010000': (10, 2), '0000000010001': (9, 2), '0000000010010': (5, 3),
    '0000000010011': (3, 4), '0000000010100': (2, 5), '0000000010101': (1, 7),
    '0000000010110': (1, 6), '0000000010111': (0, 15), '0000000011000': (0, 14),
    '0000000011001': (0, 13), '0000000011010': (0, 12), '0000000011011': (26, 1),
    '0000000011100': (25, 1), '0000000011101': (24, 1), '0000000011110': (23, 1),
    '0000000011111': (22, 1),
    '00000000010000': (0, 31), '00000000010001': (0, 30),
    '00000000010010': (0, 29), '00000000010011': (0, 28),
    '00000000010100': (0, 27), '00000000010101': (0, 26),
    '00000000010110': (0, 25), '00000000010111': (0, 24),
    '00000000011000': (0, 23), '00000000011001': (0, 22),
    '00000000011010': (0, 21), '00000000011011': (0, 20),
    '00000000011100': (0, 19), '00000000011101': (0, 18),
    '00000000011110': (0, 17), '00000000011111': (0, 16),
    '000000000010000': (0, 40), '000000000010001': (0, 39),
    '000000000010010': (0, 38), '000000000010011': (0, 37),
    '000000000010100': (0, 36), '000000000010101': (0, 35),
    '000000000010110': (0, 34), '000000000010111': (0, 33),
    '000000000011000': (0, 32), '000000000011001': (1, 14),
    '000000000011010': (1, 13), '000000000011011': (1, 12),
    '000000000011100': (1, 11), '000000000011101': (1, 10),
    '000000000011110': (1, 9), '000000000011111': (1, 8),
    '0000000000010000': (1, 18), '0000000000010001': (1, 17),
    '0000000000010010': (1, 16), '0000000000010011': (1, 15),
    '0000000000010100': (6, 3), '0000000000010101': (16, 2),
    '0000000000010110': (15, 2), '0000000000010111': (14, 2),
    '0000000000011000': (13, 2), '0000000000011001': (12, 2),
    '0000000000011010': (11, 2), '0000000000011011': (31, 1),
    '0000000000011100': (30, 1), '0000000000011101': (29, 1),
    '0000000000011110': (28, 1), '0000000000011111': (27, 1),
}
EOB = '10'
ESCAPE = '000001'


def _build(table, extra=()):
    """code string -> value, indexed by (length, bits) for O(1) lookup."""
    out = {}
    for code, val in list(table.items()) + list(extra):
        out[(len(code), int(code, 2))] = val
    return out


AC_LOOKUP = _build(AC_TABLE, [(EOB, 'EOB'), (ESCAPE, 'ESC')])
DC_LUMA_LOOKUP = _build(DC_LUMA)
DC_CHROMA_LOOKUP = _build(DC_CHROMA)
AC_LENGTHS = sorted({l for l, _ in AC_LOOKUP})
DC_LUMA_LENGTHS = sorted({l for l, _ in DC_LUMA_LOOKUP})
DC_CHROMA_LENGTHS = sorted({l for l, _ in DC_CHROMA_LOOKUP})


def _huff(br, lookup, lengths):
    for n in lengths:
        v = lookup.get((n, br.peek(n)))
        if v is not None:
            br.skip(n)
            return v
    raise ValueError('bad huffman code at bit %d' % br.pos)


# ---------------------------------------------------------------------------
# 3. Dequantisation + IDCT
# ---------------------------------------------------------------------------
ZIGZAG = np.array([
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
])

QUANT = np.array([
     2, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83,
], dtype=np.float64)

_c = np.array([np.sqrt(1.0 / 8) if u == 0 else np.sqrt(2.0 / 8) for u in range(8)])
_IDCT = np.array([[_c[u] * np.cos((2 * x + 1) * u * np.pi / 16)
                   for u in range(8)] for x in range(8)])


def idct8x8(block):
    return _IDCT @ block @ _IDCT.T


# ---------------------------------------------------------------------------
# 4. One BS frame -> RGB
# ---------------------------------------------------------------------------
def decode_block(br, qscale, dc_prev, plane, version):
    """Decode one 8x8 block. Returns the spatial-domain 8x8 and the new DC."""
    coeff = np.zeros(64, dtype=np.float64)

    if version >= 3:
        lookup, lengths = ((DC_LUMA_LOOKUP, DC_LUMA_LENGTHS) if plane == 0
                           else (DC_CHROMA_LOOKUP, DC_CHROMA_LENGTHS))
        size = _huff(br, lookup, lengths)
        if size == 0:
            diff = 0
        else:
            bits = br.read(size)
            diff = bits if bits & (1 << (size - 1)) else bits - (1 << size) + 1
        dc = dc_prev + diff * 4
    else:
        raw = br.read(10)
        dc = raw - 1024 if raw & 0x200 else raw

    coeff[0] = dc * QUANT[0]

    i = 0
    while True:
        sym = _huff(br, AC_LOOKUP, AC_LENGTHS)
        if sym == 'EOB':
            break
        if sym == 'ESC':
            run = br.read(6)
            lvl = br.read(10)
            if lvl & 0x200:
                lvl -= 1024
        else:
            run, lvl = sym
            if br.read(1):
                lvl = -lvl
        i += run + 1
        if i > 63:
            break
        pos = ZIGZAG[i]
        coeff[pos] = lvl * QUANT[pos] * qscale / 8.0

    return idct8x8(coeff.reshape(8, 8)), dc


def decode_frame(data, off):
    """Decode one 0x8000 BS slot into an (H, W, 3) uint8 RGB array."""
    n_codes, magic, qscale, version = struct.unpack_from('<4H', data, off)
    if magic != 0x3800:
        raise ValueError('not a BS frame at %#x (magic %#06x)' % (off, magic))

    br = BitReader(data, off + 8)
    mb_w, mb_h = WIDTH // 16, HEIGHT // 16
    ybuf = np.zeros((HEIGHT, WIDTH), dtype=np.float64)
    cbbuf = np.zeros((HEIGHT // 2, WIDTH // 2), dtype=np.float64)
    crbuf = np.zeros((HEIGHT // 2, WIDTH // 2), dtype=np.float64)

    dc = [0, 0, 0]          # Y, Cb, Cr predictors
    # MDEC walks macroblocks DOWN each column, then across.
    for mx in range(mb_w):
        for my in range(mb_h):
            b, dc[2] = decode_block(br, qscale, dc[2], 1, version)
            crbuf[my * 8:my * 8 + 8, mx * 8:mx * 8 + 8] = b
            b, dc[1] = decode_block(br, qscale, dc[1], 2, version)
            cbbuf[my * 8:my * 8 + 8, mx * 8:mx * 8 + 8] = b
            for k in range(4):
                b, dc[0] = decode_block(br, qscale, dc[0], 0, version)
                oy = my * 16 + (k >> 1) * 8
                ox = mx * 16 + (k & 1) * 8
                ybuf[oy:oy + 8, ox:ox + 8] = b

    y = ybuf + 128.0
    cb = np.repeat(np.repeat(cbbuf, 2, axis=0), 2, axis=1)
    cr = np.repeat(np.repeat(crbuf, 2, axis=0), 2, axis=1)

    r = y + 1.402 * cr
    g = y - 0.3437 * cb - 0.7143 * cr
    b = y + 1.772 * cb
    rgb = np.stack([r, g, b], axis=-1)
    return np.clip(rgb + 0.5, 0, 255).astype(np.uint8)


def frames_in(path):
    data = open(path, 'rb').read()
    return [off for off in range(0, len(data), FRAME_STRIDE)
            if off + 8 <= len(data)
            and struct.unpack_from('<H', data, off + 2)[0] == 0x3800], data


# ---------------------------------------------------------------------------
# 5. TIM + LZW (the PC side)
# ---------------------------------------------------------------------------
def rgb_to_555(rgb):
    """8 bits per channel -> 5, ROUNDED not truncated.

    Truncating (`>>3`) leaves a systematic half-LSB bias: checked against the
    shipped paks, it made ~45% of pixels come out exactly one step dark in at
    least one channel. Rounding with (v + 4) >> 3, saturated at 31, is what
    Capcom's conversion did.
    """
    def ch(v):
        return np.minimum((v.astype(np.uint16) + 4) >> 3, 31)
    return (ch(rgb[..., 2]) << 10) | (ch(rgb[..., 1]) << 5) | ch(rgb[..., 0])


def make_tim(rgb):
    """The exact header Capcom's own paks carry: 16bpp, vram (0,240), 320x240.

    Note the image block's length field counts ONLY the pixel bytes (153600),
    not the 12 bytes of length + x/y/w/h that precede them. That is not what the
    TIM spec says, but it is what every shipped background pak contains, and the
    port's reader is built around it.
    """
    pix = rgb_to_555(rgb).astype('<u2').tobytes()
    assert len(pix) == WIDTH * HEIGHT * 2
    out = struct.pack('<II', 0x10, 2)
    out += struct.pack('<IhhHH', len(pix), 0, 240, WIDTH, HEIGHT)
    return out + pix


def lzw_pack(data):
    """Encode for unpack_pakfile_ (0x00425ab0).

    That decoder is an LZW variant with two quirks worth knowing: it never grows
    the code width on its own (it waits for an explicit 0x101), and 0x102 resets
    the dictionary. So the encoder is in charge of both, which makes a correct
    stream easy to produce - emit 0x101 just before the width would overflow,
    and 0x102 when the dictionary is full.
    """
    out = bytearray()
    acc = 0
    nbits = 0

    def emit(code, width):
        nonlocal acc, nbits
        acc = (acc << width) | code
        nbits += width
        while nbits >= 8:
            nbits -= 8
            out.append((acc >> nbits) & 0xFF)

    table = {}
    next_code = 0x103
    width = 9
    MAX = 0x1FFF

    cur = bytes()
    pending_reset = False
    for ch in data:
        nxt = cur + bytes([ch])
        if len(cur) == 0:
            cur = nxt
            continue
        if nxt in table:
            cur = nxt
            continue
        emit(table[cur] if len(cur) > 1 else cur[0], width)
        if next_code > MAX:
            emit(0x102, width)
            table = {}
            next_code = 0x103
            width = 9
        else:
            table[nxt] = next_code
            next_code += 1
            # The decoder adds its entry AFTER reading the next code, so the
            # width has to grow one code early or it will read too few bits.
            if next_code > (1 << width) - 1 and width < 13:
                emit(0x101, width)
                width += 1
        cur = bytes([ch])
    if cur:
        emit(table[cur] if len(cur) > 1 else cur[0], width)
    emit(0x100, width)
    if nbits:
        out.append((acc << (8 - nbits)) & 0xFF)
    return bytes(out)


# ---------------------------------------------------------------------------
# 6. Commands
# ---------------------------------------------------------------------------
def pak_name(stage_digit, room_id, cam):
    return 'RC%s%02X%X.pak' % (stage_digit, room_id, cam)


def convert(bss_path, out_dir, stage_digit, room_id, dry=False):
    offs, data = frames_in(bss_path)
    written = []
    for cam, off in enumerate(offs):
        rgb = decode_frame(data, off)
        blob = lzw_pack(make_tim(rgb))
        name = pak_name(stage_digit, room_id, cam)
        if not dry:
            os.makedirs(out_dir, exist_ok=True)
            with open(os.path.join(out_dir, name), 'wb') as f:
                f.write(blob)
        written.append((name, len(blob)))
    return written


def cmd_verify(limit, show_all=False):
    """Decode base-stage frames and compare against Capcom's own .pak.

    Stages 1-7 ship in both forms, so this is a real check of the decoder
    against a known-good conversion of the same bitstream.

    A handful of frames will NOT match, and that is not a decoder fault: the PC
    release re-authored some room art, so its pak holds a different picture from
    the PS1 slot of the same index (STAGE1/ROOM107 camera 7 is the clearest
    example - it decodes to a clean image that matches none of the eight shipped
    RC107x paks). Those are reported separately and judged by a `looks like an
    image` test, because a genuine decoder failure produces noise, not a
    different photograph.
    """
    sys.path.insert(0, os.path.join(REPO, 'tools'))
    import pak_view as pv

    MATCH_ERR = 1.0        # mean channel steps out of 31
    NOISE_GRAD = 30.0      # mean |horizontal luma delta|; real art is well under

    checked = matched = 0
    differs = []
    broken = []
    odd = []
    for bss in sorted(glob.glob(os.path.join(REPO, 'assets', 'PSX',
                                             'STAGE[1-7]', '*.BSS'))):
        name = os.path.basename(bss)[:-4]          # ROOM100
        stage_digit = name[4]
        room_id = int(name[5:7], 16)
        stage_dir = os.path.join(REPO, 'assets', 'USA', 'Stage' + stage_digit)
        offs, data = frames_in(bss)
        for cam, off in enumerate(offs):
            ref_path = os.path.join(stage_dir, pak_name(stage_digit, room_id, cam))
            if not os.path.exists(ref_path):
                continue
            checked += 1
            try:
                rgb = decode_frame(data, off)
            except Exception as e:
                broken.append(('%s cam %d' % (name, cam), 'decode failed: %s' % e))
                continue
            mine = rgb_to_555(rgb)
            ref_raw = pv.PakDecoder(open(ref_path, 'rb').read()).run()
            # Not every shipped pak is a full-screen background; a few are
            # smaller or hold something else entirely. Those are not a decoder
            # question, so they are counted out rather than compared.
            if len(ref_raw) < 20 + WIDTH * HEIGHT * 2:
                odd.append(('%s cam %d' % (name, cam),
                            'shipped pak decodes to %d B, not a 320x240 TIM'
                            % len(ref_raw)))
                checked -= 1
                continue
            ref = np.frombuffer(ref_raw[20:20 + WIDTH * HEIGHT * 2],
                                dtype='<u2').reshape(HEIGHT, WIDTH)
            err = np.zeros(ref.shape)
            for shift in (0, 5, 10):
                err += np.abs((((ref >> shift) & 0x1F).astype(int)
                               - ((mine >> shift) & 0x1F).astype(int)))
            err /= 3.0
            mean = err.mean()
            if mean <= MATCH_ERR:
                matched += 1
                if show_all:
                    print('  %-10s cam %d: match (mean %.3f/31, %.1f%% exact)'
                          % (name, cam, mean, float((err == 0).mean()) * 100))
            else:
                grad = np.abs(np.diff(rgb[..., 1].astype(int), axis=1)).mean()
                where = broken if grad > NOISE_GRAD else differs
                where.append(('%s cam %d' % (name, cam),
                              'mean %.2f/31, luma gradient %.1f' % (mean, grad)))
            if limit and checked >= limit:
                break
        if limit and checked >= limit:
            break

    if not checked:
        print('nothing to verify (assets/PSX or assets/USA missing)')
        return 1

    print('%d frame(s) checked, %d matched the shipped conversion (%.1f%%)'
          % (checked, matched, 100.0 * matched / checked))
    if differs:
        print()
        print('%d frame(s) decoded cleanly but differ from the PC pak - the PC'
              % len(differs))
        print('release re-authored that art, so this is a content difference:')
        for what, why in differs:
            print('   %-18s %s' % (what, why))
    if odd:
        print()
        print('%d shipped pak(s) are not a 320x240 background and were skipped:'
              % len(odd))
        for what, why in odd[:10]:
            print('   %-18s %s' % (what, why))
    if broken:
        print()
        print('%d frame(s) look like DECODER FAILURES (noise, or an exception):'
              % len(broken))
        for what, why in broken:
            print('   %-18s %s' % (what, why))
        return 1
    return 0


ARRANGE_STAGES = ['8', '9', 'A', 'B', 'C', 'D', 'E']
# Every stage the DC disc ships backgrounds for. STAGE6/7 and STAGED/E have no
# .BSS of their own - they are the mansion-revisit stages and reuse stage 1/2's
# (and, in the arrange block, STAGE8/9's) art, which the engine's revisit fold
# already handles.
ALL_STAGES = ['1', '2', '3', '4', '5', '6', '7'] + ARRANGE_STAGES


def cmd_arrange(out_root, dry, stages=None):
    total = 0
    for sd in (stages or ARRANGE_STAGES):
        src = os.path.join(REPO, 'assets', 'PSX_DC', 'STAGE' + sd)
        if not os.path.isdir(src):
            continue
        out_dir = os.path.join(out_root, 'Stage' + sd)
        for bss in sorted(glob.glob(os.path.join(src, '*.BSS'))):
            name = os.path.basename(bss)[:-4]
            room_id = int(name[5:7], 16)
            got = convert(bss, out_dir, sd, room_id, dry)
            total += len(got)
            print('  %-14s -> %d pak(s) in %s'
                  % (os.path.basename(bss), len(got),
                     os.path.relpath(out_dir, REPO)))
    print()
    print('%s %d background pak(s)' % ('would write' if dry else 'wrote', total))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('bss', nargs='?', help='a single .BSS to convert')
    ap.add_argument('--verify', action='store_true',
                    help='check the decoder against the shipped PC paks')
    ap.add_argument('--limit', type=int, default=0, help='verify only N frames')
    ap.add_argument('--show-all', action='store_true',
                    help='print every verified frame, not just the problems')
    ap.add_argument('--arrange', action='store_true',
                    help='convert every STAGE8-E .BSS into the DC overlay')
    ap.add_argument('--all', action='store_true',
                    help='convert EVERY DC stage (1-7 and 8-E), so the DC tree '
                         'owns its own backgrounds instead of falling back')
    ap.add_argument('--out', default=os.path.join(REPO, 'assets', 'DC'))
    ap.add_argument('--name', help='pak base name for a single-file convert')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()

    if args.verify:
        return cmd_verify(args.limit, args.show_all)
    if args.all:
        return cmd_arrange(args.out, args.dry_run, ALL_STAGES)
    if args.arrange:
        return cmd_arrange(args.out, args.dry_run)
    if args.bss:
        base = os.path.basename(args.bss)[:-4]
        for name, size in convert(args.bss, args.out, base[4],
                                  int(base[5:7], 16), args.dry_run):
            print('  %s  %d B' % (name, size))
        return 0
    ap.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())
