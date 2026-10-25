# decode_msg_table.py - Decode the global message table from the original
# ResidentEvil.exe for comparison against the STR() sources in Globals.cpp.
# Usage: python tools/decode_msg_table.py
import os
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
EXE = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')
PTR_TABLE_VA = 0x004BFC58
MESSAGE_COUNT = 64

with open(EXE, "rb") as f:
    data = f.read()

# Map VA -> file offset via PE section headers.
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
    raise ValueError("VA %08x not in any section" % va)


# Font byte -> ASCII (inverse of PrintText.h encodeChar)
def font2char(b):
    if b == 0x00: return " "
    if 0x1D <= b <= 0x36: return chr(b - 0x1D + ord("A"))
    if 0x3D <= b <= 0x56: return chr(b - 0x3D + ord("a"))
    if 0x0C <= b <= 0x15: return chr(b - 0x0C + ord("0"))
    return {0x1A: "!", 0x19: '"', 0x18: ",", 0x79: ".", 0x16: ":",
            0x17: ";", 0x1B: "?", 0x3A: "'", 0x37: "(", 0x39: ")",
            0x3B: "-", 0x38: "/"}.get(b, "[%02X]" % b)


def decode_msg(ptr_va):
    p = va2off(ptr_va)
    out = []
    while p < len(data):
        b = data[p]
        if b == 0x01:
            # terminator; next byte = wait(0) or dismiss(N)
            nxt = data[p + 1] if p + 1 < len(data) else None
            out.append("<END:%s>" % ("wait" if nxt == 0 else "dismiss %d" % nxt))
            break
        if b == 0x02: out.append("\n")
        elif b == 0x03:
            op = data[p + 1] if p + 1 < len(data) else None
            out.append("<PAGE op=%02X>" % op)
            p += 1
        elif b == 0x04: out.append("<DELAY>")
        elif b == 0x05:
            out.append("<CLUT%d>" % data[p + 1])
            p += 1
        elif b == 0x06:
            out.append("<ITEM%d>" % data[p + 1])
            p += 1
        elif b == 0x07: out.append("<RET>")
        elif b == 0x08: out.append("<YESNO>")
        elif b == 0x0A: out.append("[SQR]")
        elif 0x0C <= b <= 0xF7:
            out.append(font2char(b))
        else:
            out.append("[%02X]" % b)
        p += 1
    return "".join(out)


tbl = va2off(PTR_TABLE_VA)
ptrs = struct.unpack_from("<%dI" % MESSAGE_COUNT, data, tbl)
for i, ptr in enumerate(ptrs):
    if ptr == 0 or ptr < 0x00400000 or ptr > 0x00800000:
        print("[%2d] NULL" % i)
        continue
    try:
        print("[%2d] 0x%08X: %s" % (i, ptr, decode_msg(ptr)))
    except Exception as e:
        print("[%2d] 0x%08X: ERROR %s" % (i, ptr, e))
