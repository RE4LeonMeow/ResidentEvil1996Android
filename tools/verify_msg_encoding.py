#!/usr/bin/env python3
"""
verify_msg_encoding.py - replicate the STR() Encoded constructor from
PrintText.h and compare the encoded global messages against the original
message bytes in ResidentEvil.exe. Run after editing PrintText.h escapes or
Globals.cpp message strings.
"""
import os
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
EXE = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')


def pft_hex(c):
    if '0' <= c <= '9':
        return ord(c) - 48
    if 'a' <= c <= 'f':
        return ord(c) - 87
    if 'A' <= c <= 'F':
        return ord(c) - 55
    return -1


def encodeChar(c):
    o = ord(c)
    if 0x02 <= o <= 0x0B or o >= 0xF8:
        return o
    if 'A' <= c <= 'Z':
        return o - 65 + 0x1D
    if 'a' <= c <= 'z':
        return o - 97 + 0x3D
    if '0' <= c <= '9':
        return o - 48 + 0x0C
    tbl = {' ': 0, '!': 0x1A, '"': 0x19, ',': 0x18, '.': 0x79, ':': 0x16,
           ';': 0x17, '?': 0x1B, "'": 0x3A, '(': 0x37, ')': 0x39, '-': 0x3B,
           '/': 0x38, '\\': 0x38}
    return tbl[c]


def enc(s):
    """Mirror of pft_detail::Encoded's constructor in PrintText.h."""
    N = len(s) + 1
    out = []
    dismiss = 0
    i = 0
    while i < N - 1:
        c = s[i]
        if c == '\\' and i + 1 < N - 1:
            nxt = s[i + 1]
            if nxt == 'n':
                out.append(0x02); i += 2; continue
            if nxt == 'p':
                out.append(0x03); i += 2; continue
            if nxt == 's':
                out.append(0x04); i += 2; continue
            if nxt == 'i':
                out += [0x05, 0x01, 0x06, 0x00, 0x05, 0x00]; i += 2; continue
            if nxt == 'c':
                out.append(0x08); i += 2; continue
            if nxt == 'q':
                out.append(0x0A); i += 2; continue
            if nxt == 'o':
                out.append(0x78); i += 2; continue
            if nxt == 'x':
                hi, lo = pft_hex(s[i + 2]), pft_hex(s[i + 3])
                if hi >= 0 and lo >= 0:
                    out.append((hi << 4) | lo); i += 4; continue
            if nxt == 'd':
                if i + 5 < N and s[i + 2] == '\\' and s[i + 3] == 'x':
                    hi, lo = pft_hex(s[i + 4]), pft_hex(s[i + 5])
                    if hi >= 0 and lo >= 0:
                        dismiss = (hi << 4) | lo; i += 6; continue
                    i += 2; continue
                if i + 2 < N - 1:
                    dismiss = encodeChar(s[i + 2]); i += 3; continue
                i += 2; continue
        out.append(encodeChar(c))
        i += 1
    out.append(0x01)
    if dismiss:
        out.append(dismiss)
    return bytes(out)


data = open(EXE, 'rb').read()


def va_to_off(va):
    return va - 0x400000 - 0xB1000 + 0xB0400   # .data


def orig(va):
    off = va_to_off(va)
    raw = bytearray()
    i = 0
    while True:
        b = data[off + i]
        raw.append(b)
        if b == 0x01:
            nxt = data[off + i + 1]
            if nxt:
                raw.append(nxt)
            break
        if b in (0x05, 0x06):
            raw.append(data[off + i + 1])
            i += 1
        i += 1
    return bytes(raw)


TESTS = [
    ("[0]",  0x4BF3D8, r"Will you take\nthe \i?\c\n\q "),
    ("[1]",  0x4BF3F7, r"You got the \i."),
    ("[3]",  0x4BF42D, r"You have used\nthe \i."),
    ("[4]",  0x4BF448, r"Will you put down the\n\i?\c\n\q\d\x01"),
    ("[5]",  0x4BF46B, r"Will you use\nthe \i?\c\n\q\d\x01"),
    ("[6]",  0x4BF489, r"\i\nhas been filed.\p \d\x01"),
    ("[12]", 0x4BF566, r"The door is tightly\nlocked.\p There's a plate on right\nhand side."),
    ("[16]", 0x4BF61B, "\\nIt's locked.\\p The door says\\n\\oControl Room\"."),
    ("[17]", 0x4BF649, r'\n\oPower Room"\p The door is tightly\nlocked.'),
    ("[21]", 0x4BF6B2, r"\nIt's locked.\p Use the lockpick to open\nthe door."),
    ("[25]", 0x4BF72F, r"\nThe desk is locked.\p Will you use\nthe \i?\c\n\q\d\x01"),
    ("[30]", 0x4BF886, r"\nIt's an old typewriter.\p If I had an \x05\x01INK RIBBON\x05\x00, I\ncould save my progress..."),
    ("[31]", 0x4BF8D9, r"You can save your progress\nwith this.\p Will you use\nthe \x05\x01INK RIBBON\x05\x00?\c "),
    ("[32]", 0x4BF924, r"You can save your progress\nwith this.\p Will you save your progress?\c "),
    ("[48]", 0x4BFABE, r"\i loaded."),
]

ok = True
for name, va, src in TESTS:
    e = enc(src)
    o = orig(va)
    if e == o:
        print(f"OK   {name}: {e.hex(' ')}")
    else:
        ok = False
        print(f"DIFF {name}: encoded={e.hex(' ')}")
        print(f"        original={o.hex(' ')}")
print("ALL MATCH" if ok else "MISMATCHES FOUND")
