# verify_msg_fixes.py - Encodes the fixed STR() sources with a Python port of
# the PrintText.h encoder and compares byte-for-byte against the original
# ResidentEvil.exe message data. Run: python tools/verify_msg_fixes.py
import os
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
EXE = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')
PTR_TABLE_VA = 0x004BFC58

with open(EXE, "rb") as f:
    data = f.read()

pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
opt = struct.unpack_from("<H", data, pe + 20)[0]
image_base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
sections = []
for i in range(nsec):
    off = pe + 24 + opt + i * 40
    name = data[off:off + 8].rstrip(b"\0").decode()
    vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
    sections.append((name, vaddr, vsize, rawptr, rawsize))


def va2off(va):
    for name, vaddr, vsize, rawptr, rawsize in sections:
        if vaddr <= va - image_base < vaddr + max(vsize, rawsize):
            return rawptr + (va - image_base - vaddr)
    raise ValueError("VA %08x" % va)


def encode_char(c):
    if 0x02 <= c <= 0x0B or c >= 0xF8:
        return c
    if ord("A") <= c <= ord("Z"): return c - ord("A") + 0x1D
    if ord("a") <= c <= ord("z"): return c - ord("a") + 0x3D
    if ord("0") <= c <= ord("9"): return c - ord("0") + 0x0C
    return {ord(" "): 0x00, ord("!"): 0x1A, ord('"'): 0x19, ord(","): 0x18,
            ord("."): 0x79, ord(":"): 0x16, ord(";"): 0x17, ord("?"): 0x1B,
            ord("'"): 0x3A, ord("("): 0x37, ord(")"): 0x39, ord("-"): 0x3B,
            ord("/"): 0x38, ord("\\"): 0x38}.get(c, 0x1B)


def pft_hex(c):
    if ord("0") <= c <= ord("9"): return c - ord("0")
    if ord("a") <= c <= ord("f"): return c - ord("a") + 10
    if ord("A") <= c <= ord("F"): return c - ord("A") + 10
    return -1


def str_encode(src):
    """Port of pft_detail::Encoded constructor from PrintText.h."""
    src = src.encode("latin-1")
    out = bytearray()
    dismiss = 0
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if c == ord("\\") and i + 1 < n:
            nxt = src[i + 1]
            if nxt == ord("n"): out += b"\x02"; i += 2; continue
            if nxt == ord("p"): out += b"\x03"; i += 2; continue
            if nxt == ord("s"): out += b"\x04"; i += 2; continue
            if nxt == ord("i"):
                out += bytes([0x05, 0x01, 0x06, 0x00, 0x05, 0x00]); i += 2; continue
            if nxt == ord("c"): out += b"\x08"; i += 2; continue
            if nxt == ord("q"): out += b"\x0A"; i += 2; continue
            if nxt == ord("o"): out += b"\x78"; i += 2; continue
            if nxt == ord("x") and i + 3 < n:
                hi, lo = pft_hex(src[i + 2]), pft_hex(src[i + 3])
                if hi >= 0 and lo >= 0:
                    out.append((hi << 4) | lo); i += 4; continue
            if nxt == ord("d"):
                if i + 5 < n and src[i + 2] == ord("\\") and src[i + 3] == ord("x"):
                    hi, lo = pft_hex(src[i + 4]), pft_hex(src[i + 5])
                    if hi >= 0 and lo >= 0:
                        dismiss = (hi << 4) | lo; i += 6; continue
                if i + 2 < n:
                    dismiss = encode_char(src[i + 2]); i += 3; continue
                i += 2; continue
            if nxt == ord("\\"): out += bytes([encode_char(ord("\\"))[0]]); i += 2; continue
        out += bytes([encode_char(c)])
        i += 1
    out += b"\x01"          # terminator
    out += bytes([dismiss])  # byte after it: 0x00 = wait (zero-init tail in C++)
    return bytes(out)


# (index, fixed STR source) — index into global_messages, source as it appears
# in Globals.cpp after the C-string escapes are resolved
FIXES = {
    8:  r"\nIt's locked.\p \nA carving of a sword.",
    9:  r"\nIt's locked.\p \nA carving of armor.",
    10: r"\nIt's locked.\p \nA carving of a shield.",
    11: r"\nIt's locked.\p \nA carving of a helmet.",
    13: r"\nIt's locked.\p \nThe door says \oCloset" + '"' + ".",
    14: r"\nIt's locked.\p \nThe plate says 002.",
    15: r"\nIt's locked.\p \nThe plate says 003.",
    25: r"\nThe desk is locked.\p Will you use\nthe \i?\c\n\q\x01",
    30: r"\nIt's an old typewriter.\p If I had an \x05\x01INK RIBBON\x05\x00, I\ncould save my progress...",
    31: r"You can save your progress\nwith this.\p Will you use\nthe \x05\x01INK RIBBON\x05\x00?\c\x00",
    32: r"You can save your progress\nwith this.\p Will you save your progress?\c ",
    48: r"\i loaded.",
}

# Pointer 33 sits BEFORE 32 in address order (the carving texts are shared
# suffix strings), so next-pointer bounds only work when ptr[idx+1] > ptr[idx].
def msg_bounds(idx):
    start = ptrs[idx]
    end = None
    for k in range(idx + 1, 64):
        if ptrs[k] > start:
            end = ptrs[k]
            break
    if end is None:
        raise ValueError("no end for %d" % idx)
    return va2off(start), va2off(end)

tbl = va2off(PTR_TABLE_VA)
ptrs = struct.unpack_from("<64I", data, tbl)

ok = True
for idx, src in sorted(FIXES.items()):
    off0, off1 = msg_bounds(idx)
    orig = data[off0:off1]
    enc = str_encode(src)
    match = "OK " if enc == orig else "DIFF"
    if enc != orig:
        ok = False
        # find first difference
        for k in range(min(len(enc), len(orig))):
            if enc[k] != orig[k]:
                print("  first diff at byte %d: enc=%02X orig=%02X" % (k, enc[k], orig[k]))
                break
        else:
            print("  length: enc=%d orig=%d" % (len(enc), len(orig)))
    print("[%2d] %s (enc %d bytes, orig %d bytes)" % (idx, match, len(enc), len(orig)))

print("ALL MATCH" if ok else "MISMATCHES FOUND")
