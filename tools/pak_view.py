# pak_view.py - view RE1 PC .pak background images (LZW -> TIM -> PNG/PPM)
#
# The .pak files are raw LZW bitstreams decompressed by unpack_pakfile_
# (0x00425ab0, mirrored in src/game/FileLoader.cpp). The decompressed data
# is one or more PSX TIM images concatenated. This tool reproduces the exact
# decoder and renders each TIM to PNG (Pillow) or PPM (fallback).
#
# Usage:
#   python tools/pak_view.py <file.pak> [more.pak ...] [-o outdir] [--show]
#
# Also accepts plain .tim files (skips decompression).
import struct, sys, os

# ---------------------------------------------------------------------------
# LZW decompression - exact port of unpack_pakfile_ (0x00425ab0)
# ---------------------------------------------------------------------------
class PakDecoder:
    def __init__(self, src):
        self.src = src
        self.input_pos = 0
        self.bit_mask = 0x80
        self.cur_byte = 0
        self.next_code = 0x103
        self.code_size = 9
        # dict: code -> (prefix, ch); 34981 entries like the original's 12-byte
        # records at 0x00d2b0b0 (only codes up to 0x1FFF are reachable)
        self.dict = {}
        self.out = bytearray()

    def reset_dict(self):  # 0x00425a70
        self.dict = {}
        self.next_code = 0x103
        self.code_size = 9

    def read_code(self):  # 0x00425a00
        result = 0
        bit = 1 << (self.code_size - 1)
        while bit != 0:
            if self.bit_mask == 0x80:
                self.cur_byte = self.src[self.input_pos]
                self.input_pos += 1
            if (self.cur_byte & self.bit_mask) != 0:
                result |= bit
            self.bit_mask >>= 1
            bit >>= 1
            if self.bit_mask == 0:
                self.bit_mask = 0x80
        return result

    def run(self):
        while True:  # outer loop: 0x102 restarts the dictionary
            self.reset_dict()
            code = self.read_code()
            if code == 0x100:
                break
            self.out.append(code)
            prev_code = code
            prev_first = code
            while True:
                code = self.read_code()
                if code == 0x100:
                    return bytes(self.out)
                if code == 0x102:
                    break  # restart outer loop
                if code == 0x101:
                    self.code_size += 1
                    continue
                # KwKwK case: the code is not in the table yet, so the string
                # is the PREVIOUS one with its own first character appended.
                #
                # Fixed 2026-09-14: this used to prepend that character and take
                # `first` from the result, i.e. it produced prevFirst + S(prev)
                # where unpack_pakfile_ produces S(prev) + prevFirst. The
                # original writes the appended char into stringBuf[0] and then
                # emits the buffer from charCount DOWN to 0, so index 0 comes out
                # LAST (FileLoader.cpp, 0x00425b20-0x00425b64). Any pak whose
                # encoder used the KwKwK case decoded wrong here - which is how
                # it was found, round-tripping tools/bss_to_pak.py output.
                special = self.next_code <= code
                lookup = prev_code if special else code
                # decode_string (0x00425bc0): walk prefix chain (reversed)
                chars = []
                c = lookup
                while c > 0xFF:
                    prefix, ch = self.dict[c]
                    chars.append(ch)
                    c = prefix
                chars.append(c)
                string = list(reversed(chars))
                first = string[0]          # first char of S(lookup), both cases
                if special:
                    string.append(first)
                prev_first = first
                self.out.extend(string)
                self.dict[self.next_code] = (prev_code, first)
                self.next_code += 1
                prev_code = code
        return bytes(self.out)

# ---------------------------------------------------------------------------
# TIM parsing (same layout as dump_tim.py, plus 24bpp and multi-TIM streams)
# ---------------------------------------------------------------------------
def rgb555(v):
    r = (v >> 10) & 0x1F; g = (v >> 5) & 0x1F; b = v & 0x1F
    stp = bool(v & 0x8000)
    return (r * 255 // 31, g * 255 // 31, b * 255 // 31, stp)

def parse_tim(data, off, warn=lambda m: None):
    """Parse one TIM at data[off:]. Returns (image_pixels, pw, ph, info, next_off)."""
    magic, flags = struct.unpack_from('<II', data, off)
    if magic != 0x10:
        raise ValueError(f"not a TIM (magic {magic:#x} at {off:#x})")
    bpp = flags & 7
    has_clut = bpp in (0, 1)   # 4bpp / 8bpp carry a CLUT block first
    p = off + 8
    clut = []
    if has_clut:
        clut_len = struct.unpack_from('<I', data, p)[0]; p += 4
        cx, cy, cw, ch = struct.unpack_from('<hhHH', data, p); p += 8
        raw = data[p:p + clut_len - 12]; p += clut_len - 12
        for i in range(0, len(raw), 2):
            clut.append(rgb555(struct.unpack_from('<H', raw, i)[0]))
    img_len = struct.unpack_from('<I', data, p)[0]; p += 4
    x, y, w, h = struct.unpack_from('<hhHH', data, p); p += 8
    raw = data[p:p + img_len - 12]
    p += img_len - 12

    stp_count = 0
    if bpp == 2:      # 16bpp direct color
        pw, ph = w, h
        pix = []
        for i in range(0, len(raw), 2):
            c = rgb555(struct.unpack_from('<H', raw, i)[0])
            stp_count += c[3]
            pix.append(c[:3])
    elif bpp == 1:    # 8bpp CLUT
        pw, ph = w * 2, h
        pix = []
        for b in raw:
            c = clut[b] if b < len(clut) else (0, 0, 0, False)
            stp_count += c[3]
            pix.append(c[:3])
    elif bpp == 0:    # 4bpp CLUT
        pw, ph = w * 4, h
        pix = []
        for b in raw:
            for idx in (b & 0xF, b >> 4):
                c = clut[idx] if idx < len(clut) else (0, 0, 0, False)
                stp_count += c[3]
                pix.append(c[:3])
    elif bpp == 3:    # 24bpp
        pw, ph = w * 2 // 3, h  # w is in u16 units (orgW = pw*3 rounded to u16)
        pw = (w * 2) // 3
        pix = []
        for i in range(0, len(raw), 3):
            pix.append((raw[i], raw[i + 1], raw[i + 2], False))
    else:
        raise ValueError(f"unsupported TIM bpp mode {bpp}")

    if pw * ph > len(pix):
        warn(f"truncated image block: expected {pw}x{ph}, got {len(pix)} px")
    pix = pix[:pw * ph]
    info = (f"TIM@{off:#x} bpp={bpp} vram=({x},{y}) {pw}x{ph} "
            f"{'CLUT ' + str(len(clut)) + 'c ' if has_clut else ''}"
            f"{('STP:' + str(stp_count)) if stp_count else ''}").rstrip()
    return pix, pw, ph, info, p

def find_tims(data):
    """Return TIM block offsets: direct scan for magic 0x10 followed by a
    plausible flags word (handles a leading offset table if present)."""
    offs, p = [], 0
    while p + 8 <= len(data):
        magic = struct.unpack_from('<I', data, p)[0]
        if magic == 0x10:
            flags = struct.unpack_from('<I', data, p + 4)[0]
            if (flags & 7) <= 4 and (flags & ~7) == 0:
                offs.append(p)
                try:
                    _, _, _, _, p = parse_tim(data, p)
                    continue
                except Exception:
                    p += 8
                    continue
        p += 4
    return offs

# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------
def write_png(path, pix, w, h):
    from PIL import Image
    img = Image.new('RGB', (w, h))
    img.putdata(pix)
    img.save(path)

def write_ppm(path, pix, w, h):
    with open(path, 'wb') as f:
        f.write(f"P6\n{w} {h}\n255\n".encode())
        f.write(bytes(b for c in pix for b in c))

def render(data, base_name, out_dir, use_png):
    tims = find_tims(data)
    if not tims:
        print(f"  no TIM found in decompressed data ({len(data)} bytes)")
        return []
    paths = []
    for i, off in enumerate(tims):
        try:
            pix, w, h, info, _ = parse_tim(data, off)
        except Exception as e:
            print(f"  [{i}] parse failed at {off:#x}: {e}")
            continue
        suffix = '' if len(tims) == 1 else f'_{i}'
        ext = 'png' if use_png else 'ppm'
        out = os.path.join(out_dir, f"{base_name}{suffix}.{ext}")
        if use_png:
            write_png(out, pix, w, h)
        else:
            write_ppm(out, pix, w, h)
        print(f"  [{i}] {info} -> {out}")
        paths.append(out)
    return paths

def main():
    import argparse
    ap = argparse.ArgumentParser(description="RE1 .pak background viewer (LZW->TIM)")
    ap.add_argument('files', nargs='+', help=".pak (LZW) or .tim files")
    ap.add_argument('-o', '--outdir', default=None, help="output dir (default: tools/pak_view_out)")
    ap.add_argument('--show', action='store_true', help="open the generated images")
    ap.add_argument('--ppm', action='store_true', help="write PPM instead of PNG")
    args = ap.parse_args()

    out_dir = args.outdir or os.path.join(os.path.dirname(__file__), 'pak_view_out')
    os.makedirs(out_dir, exist_ok=True)
    use_png = not args.ppm

    for path in args.files:
        name = os.path.splitext(os.path.basename(path))[0]
        print(f"{path}:")
        data = open(path, 'rb').read()
        if data[:4] == b'\x10\x00\x00\x00':
            print("  plain TIM (no LZW layer)")
            raw = data
        else:
            raw = PakDecoder(data).run()
            print(f"  LZW: {len(data)} -> {len(raw)} bytes")
        outs = render(raw, name, out_dir, use_png)
        if args.show:
            for o in outs:
                os.startfile(o)
    return 0

if __name__ == '__main__':
    sys.exit(main())
