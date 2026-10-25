#!/usr/bin/env python3
"""Emit the effect-system data tables as C initializer lists.

Reads assets/ResidentEvil.exe and prints the arrays for EffectSystem.cpp:

  0x004c4f50  32 blend records: {startV, len, blendMode, colorIdx}
  0x004c5050  60 color tables (0xAARRGGBB dwords; only bytes 0-2 are read)
  0x004c5288  60 color-record headers {count, ptr}
  0x004c5468  camera light-record index, byte[(room + fold*0x20)*8 + camera]
  0x004c5968  light records {scaleXadd, scaleYadd, brightness}
"""
import struct
from pathlib import Path

EXE = Path(__file__).resolve().parents[1] / "assets" / "ResidentEvil.exe"
data = EXE.read_bytes()
pe_off = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe_off + 6)[0]
opt_size = struct.unpack_from("<H", data, pe_off + 20)[0]
sec_tbl = pe_off + 24 + opt_size
secs = []
for i in range(nsec):
    off = sec_tbl + i * 40
    vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, off + 8)
    secs.append((vaddr, rsize, rptr))

def rd(va, n):
    va -= 0x400000
    for vaddr, rsize, rptr in secs:
        if vaddr <= va < vaddr + rsize:
            return data[rptr + va - vaddr: rptr + va - vaddr + n]
    raise KeyError(hex(va))

# ---- blend records: 32 {count, ptr} headers at 0x004c4f50 ----
blends = []
for i in range(32):
    cnt, ptr = struct.unpack("<II", rd(0x004c4f50 + i * 8, 8))
    blends.append((cnt, ptr))
assert all(0 < c < 0x100 for c, _ in blends), "blend header scan off"

print("// ============================================================================")
print("// Blend tables (0x004c4f50) - one 4-byte record per sprite-depth slot.")
print("// Entry: {startV, len, blendMode, colorIdx}; the scan finds the first entry")
print("// whose texV < startV + len.")
print("// ============================================================================")
for i, (cnt, ptr) in enumerate(blends):
    rows = []
    for j in range(cnt):
        b = rd(ptr + j * 4, 4)
        rows.append("    { 0x%02x, 0x%02x, 0x%02x, 0x%02x }," % tuple(b))
    print("// slot %2d: %d entries" % (i, cnt))
    print("\n".join(rows))

# ---- color-record headers at 0x004c5288 ----
colors = []
off = 0x004c5288
while True:
    cnt, ptr = struct.unpack("<II", rd(off, 8))
    if cnt == 0 and ptr == 0:
        break
    colors.append((cnt, ptr))
    off += 8
print("// ============================================================================")
print("// Color tables (0x004c5288) - %d records; each is 0xAARRGGBB dwords, the" % len(colors))
print("// code reads bytes 0-2 as R,G,B. First row of each = count.")
print("// ============================================================================")
for i, (cnt, ptr) in enumerate(colors):
    vals = struct.unpack("<%dI" % cnt, rd(ptr, cnt * 4))
    rows = ["    { %3d, { 0x%02x, 0x%02x, 0x%02x }," % (cnt, v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF) for v in vals]
    print("// record %2d @ 0x%08x" % (i, ptr))
    for r in rows:
        print(r)

print("// ============================================================================")
print("// Camera light-record index (0x004c5468): byte[(room + fold*0x20)*8 + camera]")
print("// ============================================================================")
b = rd(0x004c5468, 5 * 0x20 * 8)
for stage in range(5):
    print("// stage %d" % stage)
    for room in range(0x20):
        row = b[(stage * 0x20 + room) * 8: (stage * 0x20 + room) * 8 + 8]
        print("    " + ", ".join("%2d" % x for x in row) + ",  // room %2d" % room)

print("// ============================================================================")
print("// Camera light records (0x004c5968): {scaleXadd, scaleYadd, brightness}")
print("// ============================================================================")
maxidx = max(b)
for i in range(maxidx + 1):
    x, y, br = struct.unpack("<iii", rd(0x004c5968 + i * 12, 12))
    print("    { %6d, %6d, %6d }," % (x, y, br))
