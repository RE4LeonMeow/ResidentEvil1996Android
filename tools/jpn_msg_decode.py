# -*- coding: utf-8 -*-
"""jpn_msg_decode.py - decode / re-encode the Japanese Biohazard.exe text tables.

Reads the encoded byte streams straight out of the JPN executable and prints
them as readable text using tools/jpn_font_table.py, in exactly the escape
syntax the STR_JP() macro accepts.  `verify` re-encodes what it decoded and
diffs it against the original bytes, which is what proves both the glyph table
and the encoder: a wrong table entry or a wrong escape shows up as a byte
mismatch, not as text that merely looks plausible.

  python tools/jpn_msg_decode.py messages     # global_messages[64]
  python tools/jpn_msg_decode.py items        # item descriptions
  python tools/jpn_msg_decode.py names        # item names
  python tools/jpn_msg_decode.py unknown      # unexamined-item generic names
  python tools/jpn_msg_decode.py verify       # round-trip every table
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from jpn_font_table import build_map  # noqa: E402
from jpn_font_table import LEFT, RIGHT  # noqa: E402

EXE = r'C:\Users\eduar\Downloads\re1cr-2019-09-06\Biohazard.exe'

# global_messages[64]  (PTR_DAT_004cde58, read by set_message_display 0x00491980)
GLOBAL_MESSAGES = 0x004CDE58
# item descriptions    (PTR_DAT_004c9370, set_item_description_message 0x00491a40)
ITEM_DESCRIPTIONS = 0x004C9370
# item names          (PTR_DAT_004cd388, message_item_name_lookup 0x00491440)
# Its last 16 entries ARE the unexamined-item table, exactly as in the USA
# build where g_ItemNamePointers[112..127] overlaps g_UnknownItemNamePointers.
ITEM_NAMES = 0x004CD388
UNKNOWN_ITEM_NAMES = 0x004CD548

# Each table's terminator byte. Names end with 0x07 ("return from item name",
# which the message state machine reads as the end of a substitution); every
# other table ends with 0x01.
TABLES = {
    'messages': (GLOBAL_MESSAGES, 63, 0x01),
    'items':    (ITEM_DESCRIPTIONS, 79, 0x01),
    'names':    (ITEM_NAMES, 128, 0x07),
    'unknown':  (UNKNOWN_ITEM_NAMES, 16, 0x07),
}

# Tags that have a one-character escape and no operand.
TAG_ESCAPE = {0x02: 'n', 0x08: 'c', 0x0A: 'q'}
# Tags whose operand is the next source character (same rule as the USA STR).
TAG_ESCAPE_OPERAND = {0x03: 'p', 0x04: 's'}
# The full item-name sequence STR/STR_JP spell as \i.
ITEM_NAME_SEQ = bytes([0x05, 0x01, 0x06, 0x00, 0x05, 0x00])

MAP = build_map()
REV = {}
for _ch, _b in MAP.items():
    REV.setdefault(_b, _ch)


def va2off(va):
    if 0x401000 <= va < 0x4A8000:
        return va - 0x401000 + 0x400
    if 0x4A8000 <= va < 0x4AA000:
        return va - 0x4A8000 + 0xA6600
    return va - 0x4AA000 + 0xA7A00


def glyph(seq, page, row, col):
    tbl = LEFT if page == 'L' else RIGHT
    if row < len(tbl) and col < len(tbl[row]):
        ch = tbl[row][col]
        if ch and MAP.get(ch) == seq:
            return ch
    # No round-trippable name for this cell: keep the raw bytes.
    return ''.join('\\x%02X' % b for b in seq)


def decode(data, pos, term=0x01):
    """Decode one message; returns (text, offset of its terminator byte)."""
    out = []
    while pos < len(data):
        b = data[pos]
        if b == term:
            return ''.join(out), pos
        if data[pos:pos + 6] == ITEM_NAME_SEQ:
            out.append('\\i')
            pos += 6
            continue
        if b in TAG_ESCAPE:
            out.append('\\' + TAG_ESCAPE[b])
            pos += 1
            continue
        if b in TAG_ESCAPE_OPERAND:
            out.append('\\' + TAG_ESCAPE_OPERAND[b] + '\\x%02X' % data[pos + 1])
            pos += 2
            continue
        if b in (0x05, 0x06):                # CLUT colour / item id, 1 operand
            out.append('\\x%02X\\x%02X' % (b, data[pos + 1]))
            pos += 2
            continue
        if b == 0x00:
            out.append(' ')
            pos += 1
            continue
        if b in (0xF8, 0xF9, 0xFA):
            nn = data[pos + 1]
            seq = bytes([b, nn])
            page = 'L' if b == 0xF8 else 'R'
            row = nn // 18 + {0xF8: 13, 0xF9: 0, 0xFA: 14}[b]
            out.append(glyph(seq, page, row, nn % 18))
            pos += 2
            continue
        if b < 0x0C or b == 0xFB or b == 0xFF:
            out.append('\\x%02X' % b)
            pos += 1
            continue
        out.append(glyph(bytes([b]), 'L', b // 18, b % 18))
        pos += 1
    return ''.join(out), pos - 1


def hexval(c):
    return int(c, 16) if c in '0123456789abcdefABCDEF' else -1


def encode(text):
    """Mirror of STR_JP(): source text -> message bytes (no terminator)."""
    out = bytearray()
    i = 0
    while i < len(text):
        c = text[i]
        if c == '\\' and i + 1 < len(text):
            e = text[i + 1]
            if e == 'n':
                out.append(0x02); i += 2; continue
            if e == 'p':
                out.append(0x03); i += 2; continue
            if e == 's':
                out.append(0x04); i += 2; continue
            if e == 'c':
                out.append(0x08); i += 2; continue
            if e == 'q':
                out.append(0x0A); i += 2; continue
            if e == 'i':
                out += ITEM_NAME_SEQ; i += 2; continue
            if e == 'x' and i + 3 < len(text):
                hi, lo = hexval(text[i + 2]), hexval(text[i + 3])
                if hi >= 0 and lo >= 0:
                    out.append((hi << 4) | lo); i += 4; continue
        seq = MAP.get(c)
        if seq is None:
            raise KeyError('no glyph for %r (U+%04X)' % (c, ord(c)))
        out += seq
        i += 1
    return bytes(out)


def table(data, va, count, term=0x01):
    ptrs = [struct.unpack_from('<I', data, va2off(va + i * 4))[0] for i in range(count)]
    rows = []
    for i, p in enumerate(ptrs):
        o = va2off(p)
        if not 0 <= o < len(data) - 4:
            rows.append((i, p, None, 0, b''))
            continue
        text, end = decode(data, o, term)
        # Only the 0x01-terminated tables carry a trailing auto-dismiss byte;
        # a name's 0x07 is the last byte of the string.
        dismiss = data[end + 1] if term == 0x01 else 0
        rows.append((i, p, text, dismiss, bytes(data[o:end])))
    return rows


def main():
    data = open(EXE, 'rb').read()
    what = sys.argv[1] if len(sys.argv) > 1 else 'messages'

    if what == 'verify':
        bad = 0
        for name, (va, n, term) in TABLES.items():
            for i, p, text, dismiss, raw in table(data, va, n, term):
                if text is None:
                    continue
                try:
                    got = encode(text)
                except KeyError as e:
                    print('%s[%d] %s' % (name, i, e))
                    bad += 1
                    continue
                if got != raw:
                    bad += 1
                    print('%s[%2d] MISMATCH\n  orig %s\n  got  %s\n  text %s'
                          % (name, i, raw.hex(' '), got.hex(' '), text))
        print('round-trip: %s'
              % ('%d mismatches' % bad if bad else 'all entries byte-identical'))
        return

    if what not in TABLES:
        print('unknown table %r; pick one of %s or verify'
              % (what, ', '.join(TABLES)))
        return
    va, n, term = TABLES[what]
    for i, p, text, dismiss, raw in table(data, va, n, term):
        note = '   <auto-dismiss %d>' % dismiss if dismiss else ''
        print('[%3d] %08X %s%s'
              % (i, p, '<unused slot>' if text is None else text, note))


if __name__ == '__main__':
    main()
