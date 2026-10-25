# dump_tim.py - parse a PSX TIM and write a PPM (viewable) copy
# Usage: python dump_tim.py <file.tim> [out.ppm]
import struct, sys

def rgb555(v):
    r = (v >> 10) & 0x1F; g = (v >> 5) & 0x1F; b = v & 0x1F
    return (r * 255 // 31, g * 255 // 31, b * 255 // 31)

def main():
    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else path + ".ppm"
    data = open(path, 'rb').read()
    magic, ver = struct.unpack_from('<II', data, 0)
    assert magic == 0x10, "not a TIM"
    bpp = ver & 0xFF
    off = 8
    imgLen = struct.unpack_from('<I', data, off)[0]; off += 4
    x, y, w, h = struct.unpack_from('<hhHH', data, off); off += 8
    if bpp == 0x10:
        pw, ph, bytespp = w, h, 2
    elif bpp == 0x08:
        pw, ph, bytespp = w * 2, h, 1
    else:
        pw, ph, bytespp = w * 4, h, 0  # 4bpp nibbles
    img = data[off:off + imgLen - 12]; off += imgLen - 12
    print(f"magic={magic:#x} bpp={bpp:#x} imgLen={imgLen} vram=({x},{y}) w={w} h={h} -> pixels {pw}x{ph}")
    clut = b''
    if off + 4 <= len(data):
        clutLen = struct.unpack_from('<I', data, off)[0]
        if clutLen > 0:
            off += 4
            cx, cy, cw, ch = struct.unpack_from('<hhHH', data, off); off += 8
            clut = data[off:off + clutLen - 12]
            print(f"CLUT len={clutLen} at ({cx},{cy}) {cw}x{ch}")
    colors = []
    for i in range(0, len(clut), 2):
        colors.append(rgb555(struct.unpack_from('<H', clut, i)[0]))
    pix = []
    if bpp == 0x10:
        for i in range(0, len(img), 2):
            pix.append(rgb555(struct.unpack_from('<H', img, i)[0]))
    elif bpp == 0x08:
        for b in img:
            pix.append(colors[b] if b < len(colors) else (0, 0, 0))
    else:
        for b in img:
            pix.append(colors[b & 0xF] if (b & 0xF) < len(colors) else (0, 0, 0))
            pix.append(colors[b >> 4] if (b >> 4) < len(colors) else (0, 0, 0))
    with open(out, 'wb') as f:
        f.write(f"P3\n{pw} {ph}\n255\n".encode())
        for i, c in enumerate(pix):
            f.write(f"{c[0]} {c[1]} {c[2]}\n".encode())
    print(f"wrote {out}")

if __name__ == '__main__':
    main()
