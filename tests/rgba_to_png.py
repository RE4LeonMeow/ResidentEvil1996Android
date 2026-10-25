"""Convert a --capture dump into a PNG (no third-party dependency).

The Linux build's --capture flag writes "RE1CAP <w> <h>\\n" followed by
w*h*4 raw RGBA bytes. This turns that into a PNG using only zlib + struct, so
the static-frame comparison in docs/LINUX_PORT.md 6.7 works without
ImageMagick or PIL installed.

Usage:  python3 tests/rgba_to_png.py capture.rgba out.png
"""
import struct
import sys
import zlib


def read_capture(path):
    with open(path, "rb") as f:
        header = f.readline().decode("ascii").strip()
        parts = header.split()
        if len(parts) != 3 or parts[0] != "RE1CAP":
            raise ValueError("not a RE1CAP dump: %r" % header)
        w, h = int(parts[1]), int(parts[2])
        data = f.read(w * h * 4)
        if len(data) != w * h * 4:
            raise ValueError("truncated: expected %d bytes, got %d"
                             % (w * h * 4, len(data)))
    return w, h, data


def png_chunk(tag, payload):
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def write_png(path, w, h, rgba):
    # Filter type 0 on every scanline.
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]

    out = b"\x89PNG\r\n\x1a\n"
    out += png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    out += png_chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    out += png_chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    w, h, data = read_capture(sys.argv[1])
    write_png(sys.argv[2], w, h, data)
    print("wrote %s (%dx%d)" % (sys.argv[2], w, h))
    return 0


if __name__ == "__main__":
    sys.exit(main())
