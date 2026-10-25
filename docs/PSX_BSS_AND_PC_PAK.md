# Room backgrounds: the PS1 `.BSS` and the PC `.pak`

The two containers the same camera background lives in, and what it takes to
convert one into the other. Written while building `tools/bss_to_pak.py` for the
Director's Cut's arrange stages (`docs/DC_PORT.md` §6b), whose 114 new
camera frames exist in no PC release.

---

## PS1: `STAGEn/ROOMxyy.BSS`

A flat array of **`0x8000`-byte slots, one per camera**, in camera order. The
file size is always a multiple of `0x8000`, so the camera count is
`size / 0x8000` — there is no header, index or count anywhere in the file.

Each slot holds a standard PSX **BS** (MDEC) bitstream for one 320×240 frame,
with the usual 8-byte header:

| offset | type | meaning |
|---|---|---|
| `0x00` | `u16` | number of MDEC codes in the frame, rounded up to a multiple of 32 |
| `0x02` | `u16` | `0x3800`, the magic |
| `0x04` | `u16` | quantisation scale |
| `0x06` | `u16` | version — RE1 uses **3** |

Both builds' room files use version 3 and a quantisation scale of 1 or 2, and
the scale varies **per camera within one room** (STAGE1/ROOM107 runs 2, 2, 2, 1,
1, 1, 1, 1), so it must be read per slot and never assumed.

The bitstream itself is ordinary MPEG-1-derived MDEC:

- **Bit order.** The data is a run of 16-bit **little-endian** words whose bits
  are consumed most-significant-first. Reading it as a flat big-endian byte
  stream swaps every pair of bytes and decodes to noise. Byte-swapping each
  `u16` up front and then treating the result as an MSB-first stream is the
  simplest way to get this right.
- **Huffman.** DC coefficient sizes use MPEG-1 tables B-12 (luma) and B-13
  (chroma); AC coefficients use table B-14, with `10` as end-of-block and
  `000001` as the escape (6-bit run + 10-bit signed level).
- **Version 3 DC coding** is differential per plane, with three separate
  predictors (Y, Cb, Cr), and the differential is scaled by **4**.
- **Block order** inside a macroblock is Cr, Cb, Y0, Y1, Y2, Y3.
- **Macroblock order** across the frame is **column-major**: all 15 macroblocks
  down a column, then the next column, 20 columns for 320×240. Row-major gives a
  picture that is recognisable but scrambled into vertical strips, which is a
  useful thing to recognise when debugging.
- Dequantisation is the MPEG-1 default intra matrix, `coeff * quant[i] * scale /
  8` for AC and `dc * quant[0]` for DC, then an 8×8 IDCT, then YCbCr→RGB with
  the usual `1.402 / -0.3437 / -0.7143 / 1.772` coefficients and a `+128` luma
  offset.

---

## PC: `Stagen/RC<stage><room><cam>.pak`

An **LZW stream** — the exact bit format `unpack_pakfile_` (`0x00425ab0`,
`src/game/FileLoader.cpp`) reads — wrapping a single 16bpp TIM.

The TIM is 320×240 at VRAM `(0, 240)`, and its header is worth copying
byte-for-byte rather than generating from a spec, because of one deviation:

```
10 00 00 00   magic 0x10
02 00 00 00   flags: 16bpp direct colour
00 58 02 00   image block length = 0x25800 = 153600
00 00 f0 00   vram x = 0, y = 240
40 01 f0 00   w = 320, h = 240
<153600 bytes of RGB555, little-endian>
```

The length field is **153600 = the pixel bytes only**. The TIM spec says that
field counts itself and the four `x/y/w/h` shorts as well, which would make it
153612. Every shipped background pak has 153600, and the port's reader is built
around that, so a spec-correct writer produces a file the game mis-reads.

Total decompressed size is therefore always 153620 bytes.

### The LZW variant

Standard LZW with 9-bit initial codes and three control codes:

| code | meaning |
|---|---|
| `0x100` | end of stream |
| `0x101` | increase the code width by one |
| `0x102` | reset the dictionary (back to 9 bits, next free code `0x103`) |

Two properties matter when writing an encoder. The decoder **never grows the
code width on its own** — it waits to be told with `0x101` — and it emits the
very first code as a raw literal byte. So the encoder owns the width schedule
entirely, which makes producing a valid stream easy: emit `0x101` one step
before the width would overflow, and `0x102` when the dictionary fills.

A from-scratch encoder compresses these backgrounds to about 29% of the TIM,
against Capcom's 27% — close enough that no further tuning is worthwhile.

### A bug this turned up in `tools/pak_view.py`

`pak_view.py`'s decoder mishandled the **KwKwK case** (a code that is not yet in
the dictionary, which LZW resolves as "the previous string plus its own first
character"). It *prepended* that character instead of appending it, producing
`prevFirst + S(prev)` where the game produces `S(prev) + prevFirst`, and took
the new `prev_first` from the wrong end as well.

The game is right. `unpack_pakfile_` writes the appended character into
`g_pakStringBuf[0]`, decodes the previous string **reversed** from index 1, and
then emits the buffer from `charCount` *down* to 0 — so index 0 comes out last
(`FileLoader.cpp`, `0x00425b20`–`0x00425b64`).

Shipped paks happen to decode correctly either way, which is why it went
unnoticed; it only shows up on a stream whose encoder actually uses the case.
Round-tripping `bss_to_pak.py`'s output through `pak_view` is what exposed it.
Fixed 2026-09-14.

---

## Converting, and how it is checked

`tools/bss_to_pak.py`:

```
python tools/bss_to_pak.py --verify          # check against the shipped paks
python tools/bss_to_pak.py --arrange         # STAGE8-E -> assets/DC/Stage8-E/
python tools/bss_to_pak.py <file.BSS> --out <dir>
```

`--verify` is the reason this is trustworthy: **stages 1-7 exist in both
forms**, so the decoder can be run against Capcom's own conversion of the same
bitstream, frame by frame, and scored per pixel.

Two results are expected and are *not* decoder faults:

- **A residual of well under one step in 31 per channel.** Capcom used a
  fixed-point IDCT and this uses floating point, so the two disagree in the last
  bit on some pixels. Rounding the 8→5 bit conversion with `(v + 4) >> 3` rather
  than truncating with `>> 3` is what takes the agreement from roughly a third
  of pixels to the high eighties; truncation leaves a systematic half-step
  darkening that is easy to mistake for a decode problem.
- **A few frames that do not match at all.** The PC release re-authored some
  room art, so its pak at that camera index holds a different picture.
  STAGE1/ROOM107 camera 7 is the clearest: it decodes to a clean image that
  matches none of the eight shipped `RC107x` paks. `--verify` separates these
  from real failures with a luma-gradient test — a broken decode produces noise,
  not a different photograph.
