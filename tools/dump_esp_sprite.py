# Dump a sprite region from an effspr TIM (8bpp 256x256 sheet): decode pixels,
# print CLUT entries with their STP bit, so we can see whether a sprite draws
# semi-transparent. Layout per PSXTexture::Store:
#   +0x00 magic, +0x04 flags, +0x08 clut size, +0x0C clut X, +0x10 clut Y,
#   +0x14 clutW, +0x16 clutH, +0x14+... CLUT colours (clutW*clutH words),
#   then image section: dword size, dword origin, dword (w in 16-bit words)<<16|h,
#   then pixel data.
# Usage: python dump_esp_sprite.py <file.tim> <x> <y> <w> <h>
import struct, sys

def main():
    path = sys.argv[1]
    x0, y0, w, h = int(sys.argv[2], 0), int(sys.argv[3], 0), int(sys.argv[4], 0), int(sys.argv[5], 0)
    data = open(path, "rb").read()
    flags = struct.unpack_from("<I", data, 4)[0]
    if flags & 8:
        clutX = struct.unpack_from("<H", data, 0x0C)[0]
        clutY = struct.unpack_from("<H", data, 0x0E)[0]
        clutW = struct.unpack_from("<H", data, 0x10)[0]
        clutH = struct.unpack_from("<H", data, 0x12)[0]
        clut = data[0x14 : 0x14 + clutW*clutH*2]
        pix = 0x14 + clutW*clutH*2
    else:
        clutW = clutH = 0
        clut = b""
        pix = 8
    # image section
    isize = struct.unpack_from("<I", data, pix)[0]
    iw = struct.unpack_from("<H", data, pix + 8)[0]
    ih = struct.unpack_from("<H", data, pix + 10)[0]
    bpp = flags & 7
    pwidth = iw * (2 if bpp == 1 else 4) if bpp != 2 else iw
    img = data[pix + 12 : pix + 12 + pwidth * ih]
    print("flags=%02x bpp=%d CLUT at (%d,%d) %dx%d | image %dx%d at file 0x%x" %
          (flags, bpp, clutX, clutY, clutW, clutH, pwidth, ih, pix+12))

    if clutW == 256:
        print("CLUT (256 entries):")
        for e in range(0, 256, 8):
            row = []
            for c in range(e, e+8):
                v = struct.unpack_from("<H", clut, c*2)[0]
                stp = (v >> 15) & 1
                r = (v & 0x1F) * 255 / 31; g = ((v>>5) & 0x1F) * 255 / 31; b = ((v>>10) & 0x1F) * 255 / 31
                row.append("%02X:%s(%d,%d,%d)" % (v & 0x7FFF, "S" if stp else "-", r, g, b))
            print("  %3d: %s" % (e, "  ".join(row)))

    stp_count = 0
    nz_count = 0
    print("pixels at (%d,%d)+%dx%d:" % (x0, y0, w, h))
    for y in range(y0, y0 + h):
        line = []
        for x in range(x0, x0 + w):
            idx = img[y*pwidth + x]
            v = struct.unpack_from("<H", clut, idx*2)[0] if idx < clutW else 0
            stp = (v >> 15) & 1
            if idx != 0:
                nz_count += 1
                if stp: stp_count += 1
            line.append("%02X%s" % (idx, "s" if stp else " "))
        print("  y=%3d: %s" % (y, " ".join(line)))
    print("non-zero pixels: %d, of which STP (semi-transparent): %d" % (nz_count, stp_count))

main()
