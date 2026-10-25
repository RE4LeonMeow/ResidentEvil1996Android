"""mdec.py - PlayStation MDEC (BS) bitstream decoder.

Decodes one PS1 video frame's demultiplexed bitstream into RGB. Used by
tools/str_to_video.py (STR movies); the DC asset importer keeps its own copy so
it stays a standalone script.

The bitstream is the concatenated 2016-byte payloads of the frame's video
sectors (see docs/PS1_VIDEO_FORMAT.txt, ch. 2.2/2.3): an <n_codes, 0x3800,
qscale, version> header, then macro blocks. Each macro block is six 8x8 blocks
in the order Cr, Cb, Y1, Y2, Y3, Y4, walked down each 16-pixel column and then
across (ch. 2.2 header, ch. 2.3.5). Version 2 stores a plain 10-bit signed DC
per block; version 3 stores a Huffman-coded DC differential per plane.

Dequantisation, un-zig-zag and the IDCT follow ch. 2.3.2-2.3.4: the DC is
multiplied by the quant matrix only, every AC coefficient by
quant[qscale/8] (the "x2 /16" of the doc).
"""
import struct

import numpy as np


class BitReader:
    """A PSX BS bitstream is a run of 16-bit LITTLE-ENDIAN words whose bits are
    consumed most-significant first. Reading it as a plain big-endian byte
    stream gives every pair of bytes swapped, which decodes into noise - the
    single easiest thing to get wrong."""

    def __init__(self, data, start):
        body = bytearray(data[start:])
        if len(body) & 1:
            body.append(0)
        body[0::2], body[1::2] = body[1::2], bytes(body[0::2])
        self.buf = bytes(body)
        self.pos = 0

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


# Huffman tables (MPEG-1, which is what the PSX MDEC bitstream uses).
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

_c = np.array([np.sqrt(1.0 / 8) if u == 0 else np.sqrt(2.0 / 8)
               for u in range(8)])
_IDCT = np.array([[_c[u] * np.cos((2 * x + 1) * u * np.pi / 16)
                   for u in range(8)] for x in range(8)])


def idct8x8(block):
    return _IDCT @ block @ _IDCT.T


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


def decode_frame(data, off, width, height):
    """Decode one MDEC frame into an (height, width, 3) uint8 RGB array.

    `data` may be a bytes/bytearray holding the frame bitstream at `off`, or a
    memoryview. Extra rows/columns when width/height are not multiples of 16 are
    decoded (the MDEC always works in macro blocks) and left in the result; the
    caller can crop.
    """
    n_codes, magic, qscale, version = struct.unpack_from('<4H', data, off)
    if magic != 0x3800:
        raise ValueError('not an MDEC frame at %#x (magic %#06x)' % (off, magic))

    mw = (width + 15) // 16
    mh = (height + 15) // 16
    br = BitReader(data, off + 8)
    ybuf = np.zeros((mh * 16, mw * 16), dtype=np.float64)
    cbbuf = np.zeros((mh * 16, mw * 16), dtype=np.float64)
    crbuf = np.zeros((mh * 16, mw * 16), dtype=np.float64)

    dc = [0, 0, 0]          # Y, Cb, Cr predictors
    # MDEC walks macroblocks DOWN each column, then across (ch. 2.2).
    for mx in range(mw):
        for my in range(mh):
            b, dc[2] = decode_block(br, qscale, dc[2], 1, version)
            crbuf[my * 16:my * 16 + 16, mx * 16:mx * 16 + 16] = \
                np.repeat(np.repeat(b, 2, 0), 2, 1)
            b, dc[1] = decode_block(br, qscale, dc[1], 2, version)
            cbbuf[my * 16:my * 16 + 16, mx * 16:mx * 16 + 16] = \
                np.repeat(np.repeat(b, 2, 0), 2, 1)
            for k in range(4):
                b, dc[0] = decode_block(br, qscale, dc[0], 0, version)
                oy = my * 16 + (k >> 1) * 8
                ox = mx * 16 + (k & 1) * 8
                ybuf[oy:oy + 8, ox:ox + 8] = b

    y = ybuf + 128.0
    cb = cbbuf
    cr = crbuf

    r = y + 1.402 * cr
    g = y - 0.3437 * cb - 0.7143 * cr
    b = y + 1.772 * cb
    rgb = np.stack([r, g, b], axis=-1)[:height, :width]
    return np.clip(rgb + 0.5, 0, 255).astype(np.uint8)
