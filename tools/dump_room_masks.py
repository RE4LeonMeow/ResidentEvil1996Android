"""Dump the per-camera room-mask (overlay) sprite tables from RDT files.

Used to check the posData / flags of every overlay in a room so the
DrawRoomSpr ordering can be compared against the ground-shadow key.
"""
import struct
import sys
import glob
import os

RDT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "USA")


def parse(path):
    d = open(path, "rb").read()
    cam_count = d[1]
    out = []
    for c in range(cam_count):
        base = 0x94 + c * 0x2C
        if base + 0x2C > len(d):
            break
        mask_ptr = struct.unpack_from("<i", d, base)[0]
        tim_ptr = struct.unpack_from("<i", d, base + 4)[0]
        pos = struct.unpack_from("<3i", d, base + 8)
        look = struct.unpack_from("<3i", d, base + 0x14)
        fov = struct.unpack_from("<i", d, base + 0x28)[0]
        sprites = []
        if mask_ptr:
            off = mask_ptr
            if 0 <= off < len(d) - 4:
                grp_count = struct.unpack_from("<i", d, off)[0]
                if 0 < grp_count < 64:
                    groups = []
                    g = off + 4
                    for _ in range(grp_count):
                        if g + 8 > len(d):
                            break
                        sc, bits, ox, oy = struct.unpack_from("<HHhh", d, g)
                        groups.append((sc, bits, ox, oy))
                        g += 8
                    rec = g
                    for gi, (sc, bits, ox, oy) in enumerate(groups):
                        for si in range(sc):
                            if rec + 8 > len(d):
                                break
                            w0, w1, posdata, flags = struct.unpack_from("<HHHH", d, rec)
                            rec += 8
                            wh = None
                            if (flags & 0xF000) == 0:
                                if rec + 4 <= len(d):
                                    wh = struct.unpack_from("<HH", d, rec)
                                    rec += 4
                            sprites.append((gi + 1, posdata, flags, w0, w1, wh))
        out.append((c, pos, look, fov, sprites))
    return out


for path in sys.argv[1:]:
    for f in sorted(glob.glob(path)):
        name = os.path.basename(f)
        print("=== %s" % name)
        for (c, pos, look, fov, sprites) in parse(f):
            print("  cam %d pos=%s look=%s fov=%d  masks=%d" % (c, pos, look, fov, len(sprites)))
            for (gid, posdata, flags, w0, w1, wh) in sprites:
                print("     id=%2d posData=0x%04x (%5d) flags=0x%04x uv=(%d,%d) scr=(%d,%d) wh=%s"
                      % (gid, posdata, posdata, flags, w0 & 0xFF, w0 >> 8, w1 & 0xFF, w1 >> 8, wh))
