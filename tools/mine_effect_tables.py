#!/usr/bin/env python3
"""Mine the billboard-effect data tables out of assets/ResidentEvil.exe.

The 2D effects system (update_2d_effects / EffectActor_UpdateAndRender) reads
several static tables from .data / .rdata:

  0x004c47b0  int   scaleX bias added per camera light record
  0x004c47b4  int   scaleY bias added per camera light record
  0x004c47b8  dword[32]  effect behavior function table (indexed by animId/updateId)
  0x004c48a0  byte[(stage*0x20 + room)*4 + depth]   sprite-depth -> blend-record idx (0xff = none)
  0x004c4f50  dword count / dword ptr  blend records (8 bytes per idx)
  0x004c5288  dword count / dword ptr  color-tint records (8 bytes per idx)
  0x004c5468  byte[(room + stageFold*0x20)*8 + camera]  camera -> light-record idx
  0x004c5968  int[3] per light record: scaleX add, scaleY add, brightness
  0x004c59bc  int   == 1 (never written; gates the pool iteration direction)

Dumps them as C initializer lists ready to paste into Globals.cpp.
"""
import struct
import sys
from pathlib import Path

EXE = Path(__file__).resolve().parents[1] / "assets" / "ResidentEvil.exe"

def read_exe(path):
    data = path.read_bytes()
    assert data[:2] == b"MZ", "not a PE"
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe_off:pe_off+4] == b"PE\0\0", "no PE header"
    nsec = struct.unpack_from("<H", data, pe_off + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe_off + 20)[0]
    sec_tbl = pe_off + 24 + opt_size
    sections = []
    for i in range(nsec):
        off = sec_tbl + i * 40
        name = data[off:off+8].rstrip(b"\0").decode("ascii", "replace")
        vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((name, vaddr, vsize, rptr, rsize))
    return data, sections

def va_to_off(sections, va):
    # Ghidra addresses are absolute (image base 0x400000); section VAs are RVAs.
    if va >= 0x400000:
        va -= 0x400000
    for name, vaddr, vsize, rptr, rsize in sections:
        if vaddr <= va < vaddr + max(vsize, rsize):
            off = rptr + (va - vaddr)
            if off < len(data):
                return off
    raise KeyError("VA 0x%08x not in any section" % va)

data, sections = read_exe(EXE)
print("// %s sections:" % EXE.name, [(s[0], hex(s[1]), hex(s[2])) for s in sections])

def rd(va, n):
    return data[va_to_off(sections, va):va_to_off(sections, va) + n]

def c_bytes(va, n, per=16):
    b = rd(va, n)
    lines = []
    for i in range(0, n, per):
        chunk = b[i:i+per]
        lines.append("    " + ", ".join("0x%02x" % x for x in chunk) + ",")
    return "\n".join(lines)

def c_dwords(va, n, per=8):
    b = rd(va, n)
    vals = struct.unpack("<%dI" % (n // 4), b)
    lines = []
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join("0x%08x" % x for x in vals[i:i+per]) + ",")
    return "\n".join(lines)

def c_ints(va, n, per=8):
    b = rd(va, n)
    vals = struct.unpack("<%di" % (n // 4), b)
    lines = []
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join("%10d" % x for x in vals[i:i+per]) + ",")
    return "\n".join(lines)

print("""
// ============================================================================
// scaleX/scaleY bias (0x004c47b0 / 0x004c47b4)
// ============================================================================""")
print("static const int g_EffectScaleBiasX = %d;" % struct.unpack("<i", rd(0x004c47b0, 4))[0])
print("static const int g_EffectScaleBiasY = %d;" % struct.unpack("<i", rd(0x004c47b4, 4))[0])

print("""
// ============================================================================
// effect behavior table (0x004c47b8) - 32 function pointers
// ============================================================================
static const void* g_effectBehaviorTable[32] = {
%s
};""" % c_dwords(0x004c47b8, 128))

print("""
// ============================================================================
// sprite-depth table (0x004c48a0) - byte per (stage,room,depth)
// [(stage*0x20 + room)*4 + depth]; 0xff = no blend record for this depth.
// Dumped raw; stage block length is 0x20*4 = 128 bytes.
// ============================================================================
static const unsigned char g_EffectDepthIndex[5 * 0x20 * 4] = {
%s
};""" % c_bytes(0x004c48a0, 5 * 0x20 * 4, per=16))

# ---- blend record headers (0x004c4f50) - count + pointer pairs ----
# The valid records are 0..31; pointers all land in 0x4c4d38..0x4c4f50. Anything
# past that is unrelated data, so cap the scan by pointer range.
recs = []
off = 0x004c4f50
while True:
    cnt, ptr = struct.unpack("<II", rd(off, 8))
    if not (0x004c4d38 <= ptr < 0x004c4f50 and 0 < cnt < 0x100):
        break
    recs.append((cnt, ptr))
    off += 8
print("""
// ============================================================================
// blend records (0x004c4f50) - %d records: {count, table ptr}
// ============================================================================
static const struct { int count; const unsigned char* table; } g_EffectBlendRecords[%d] = {
%s
};""" % (len(recs), len(recs), "\n".join(
    "    { %3d, (const unsigned char*)0x%08x }," % (c, p) for c, p in recs)))

# entries: each table is rows of [startV, len, blendMode, colorIdx]
print("""
// per-record tables (raw bytes)""")
for i, (cnt, ptr) in enumerate(recs):
    print("// record %2d @ 0x%08x: %d entries" % (i, ptr, cnt))
    print(c_bytes(ptr, cnt * 4, per=16))

# ---- color-tint records (0x004c5288) ----
# Headers run 0x4c5288..0x4c5468 (60 records); the tables live backwards in
# 0x4c5050..0x4c5288. Terminator is (0,0).
recs2 = []
off = 0x004c5288
while True:
    cnt, ptr = struct.unpack("<II", rd(off, 8))
    if cnt == 0 and ptr == 0:
        break
    recs2.append((cnt, ptr))
    off += 8
print("""
// ============================================================================
// color-tint records (0x004c5288) - %d records: {count, table ptr}
// ============================================================================
static const struct { int count; const unsigned char* table; } g_EffectColorRecords[%d] = {
%s
};""" % (len(recs2), len(recs2), "\n".join(
    "    { %3d, (const unsigned char*)0x%08x }," % (c, p) for c, p in recs2)))
for i, (cnt, ptr) in enumerate(recs2):
    print("// record %2d @ 0x%08x: %d entries (0xAARRGGBB dwords; code reads bytes 0-2)" % (i, ptr, cnt))
    print(c_dwords(ptr, cnt * 4, per=4))

print("""
// ============================================================================
// camera light-record index (0x004c5468) - byte per (room, camera)
// [(room + stageFold*0x20)*8 + camera]; stageFold = stage > 4 ? stage - 5 : stage
// ============================================================================
static const unsigned char g_EffectCameraLightIndex[5 * 0x20 * 8] = {
%s
};""" % c_bytes(0x004c5468, 5 * 0x20 * 8, per=16))

# ---- light records (0x004c5968) - int[3] each; count from max idx in g_EffectCameraLightIndex ----
maxidx = max(rd(0x004c5468, 5 * 0x20 * 8))
nrec = maxidx + 1
print("""
// ============================================================================
// camera light records (0x004c5968) - %d records of {scaleXadd, scaleYadd, brightness}
// ============================================================================
static const int g_EffectLightRecords[%d][3] = {
%s
};""" % (nrec, nrec, "\n".join(
    "    { %6d, %6d, %6d }," % struct.unpack("<iii", rd(0x004c5968 + i * 12, 12))
    for i in range(nrec))))

print("""
// 0x004c59bc flag: %d
// ============================================================================
""" % struct.unpack("<I", rd(0x004c59bc, 4))[0])
