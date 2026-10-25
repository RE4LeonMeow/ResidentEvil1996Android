# PS1 prologue FMV subtitles (JPN)

The JPN PlayStation release carries subtitle lines for the FMVs. They are
absent from the USA disc entirely, so this is a JPN-only feature, gated by
`[Game] Ps1FmvSubtitles=1` in `config.ini` and off by default.

Everything below was read out of the original with the Ghidra MCP against
`SLPS_009.98` (Biohazard Director's Cut) and `SLUS_001.70` (USA), not guessed.
Addresses are the JPN resident exe unless noted.

## Where the code lives

The `PROLOGUE.EXE` overlay does **not** contain the subtitle code. It only
builds the 480x240 framebuffer rect, sets `DAT_800cf63b = 1` and
`DAT_800cf63c` to the scratch buffer, and kicks off the movie. That overlay is
plain PSX MIPS stored **word-swapped** (little-endian); it decodes cleanly with
Capstone `CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN` at file offset 0x800.

The subtitles are drawn by the video display module in the resident exe:

| Symbol | Address | Role |
|---|---|---|
| `StartSubtitleTrack` | `0x80037b74` | binds a cue track and loads its bitmap set |
| `ProcessSubtitleCues` | `0x80037c44` | per-frame walk of the cue records |
| `BlitSubtitleLine4bppTo24bpp` | `0x80037d98` | greyscales a line into the framebuffer |
| `SetDisplayModeAndPresent` | `0x80037574` | display init; drives the two above |
| `DAT_80090490` | `0x80090490` | subtitle track directory |
| `DAT_80090290` | `0x80090290` | track 0 cue table |

Both exes are little-endian word-swapped PSX MIPS. Neither contains a literal
asset filename — files are addressed purely through the CD file table.

## The CD file table

`CD_GetFileLBA` (`0x80014ebc`) indexes a 12-byte-stride array based at
`0x80092620`:

```
+0x00  u16  lba low
+0x04  u32  size in bytes
+0x08  u8   lba high
```

`CD_LoadFileByIndex` (`0x80014ae4`) is a raw `CdRead` of `(size + 0x7ff) >> 11`
sectors into the caller's address — there is no decompression at load time, so
any transform belongs to the format's consumer.

The three JIMAKU (Subtitle/caption in japanese) files are **CD file indices 18, 19 and 20**:

| index | file | lba | size | lines |
|---|---|---|---|---|
| 18 | `JIMAKU00.RGB` | 0x209 | 397440 | 23 |
| 19 | `JIMAKU01.RGB` | 0x2cc | 120960 | 7 |
| 20 | `JIMAKU02.RGB` | 0x308 | 86400 | 5 |

The USA disc has no JIMAKU or FONT01/FONT02 files.

## The track directory

`DAT_80090490` is two parallel `u32[]` arrays, 8 bytes per track:

```
+0x00  u32  pointer to the cue table
+0x04  u32  CD file index of the bitmap set
```

Track 0 -> `0x80090290` / 18, track 1 -> `0x800903b0` / 20,
track 2 -> `0x80090410` / 19.

## Cue records

0x10 bytes, walked by `ProcessSubtitleCues`. Offsets are byte offsets read
straight out of the disassembly:

| offset | type | meaning |
|---|---|---|
| `+0x00` | u16 | startFrame (movie frame the cue switches on) |
| `+0x04` | u16 | duration in frames |
| `+0x08` | u16 | lineIndex — first line inside the JIMAKU file |
| `+0x0A` | u16 | lineCount — raster height is `lineCount * 18` |
| `+0x0C` | u16 | destination x |
| `+0x0E` | u16 | destination y |
| `+0x10` | u32 | `0xFFFFFFFF` ends the track |

Cue timing is in the movie's own frames; STR movies decode at **15 fps**.

## Bitmap format

`JIMAKU*.RGB` is a flat run of 320x18 pixel lines, 24 bpp greyscale, stored
**plain row-major** — the whole file is one 320-wide image, one line after the
other:

```
320 * 18 * 3 = 17280 bytes per line
```

`ProcessSubtitleCues` points the source at `line * 320 * 18 * 3` bytes into the
loaded file, and `BlitSubtitleLine4bppTo24bpp` walks it 3 bytes at a time in
exactly the order of the 320-wide 24 bpp framebuffer rect it `StoreImage`s and
`LoadImage`s back, so there is no reordering anywhere. It writes the **first**
byte into all three destination bytes (every pixel on the disc is R=G=B
anyway) and skips the pixel when all three source bytes are zero — that is
what leaves the movie visible around the text. There is no compression, no
CLUT and no swizzle; the loader (`LoadFileToAddress`, `0x8002b924`) is a raw
`CdRead`.

An earlier version of this document described a 16x16 column-major VRAM tile
swizzle. That was fitted to a **corrupted extracted copy** of `JIMAKU00.RGB`
(correct first sector, garbage after it) and scrambled the real disc data; the
bytes read straight from the disc image are 100% R=G=B and render as clean text
row-major. Always read these files from the disc image, not from a loose copy.

## The port

`src/game/Ps1FmvSubtitles.{h,cpp}` mirrors the above. `tools/decode_jimaku.py`
decodes each `JIMAKUnn.RGB` into `assets/JPN/data/jimakunn.png`: one 8-bit
greyscale image, 320 wide, the 18-row lines stacked top to bottom (so line `n`
starts at row `18 * n`). The cue tables are baked into
`src/game/Ps1FmvSubtitleData.h`.

The PNGs can be viewed and edited with any image tool. The game reads them with
the port's own decoder (`src/system/PngImage.{h,cpp}`, no zlib), which accepts
any non-interlaced PNG: it keeps the first channel (grey, or red for RGB /
palette images) and treats a fully transparent pixel as level 0. The image must
stay 320 wide and a whole number of 18-row lines tall, or the set is rejected.

Each non-zero level becomes an opaque grey texel (level in R, G and B) and level
0 a fully transparent one, which reproduces the original's "write the level,
skip the zeros" behaviour under alpha blending.
`Ps1VideoOverlayCallback` chains after `Ps1EndingCredits_RenderFrame`; because
returning TRUE means the overlay owns the whole frame, `Ps1FmvSubtitles_RenderFrame`
draws the movie quad itself before the subtitle.

Regenerating the data:

```
python tools/decode_jimaku.py --iso <BiohazardDirectrorsCut.img> \
    --exe assets/PSX_JPNDC/SLPS_009.98 --track 0 --out assets/JPN/data
```

`--track` takes 0, 1 or 2; the prologue uses track 0. `--out` wants the `data`
folder of the **JPN** tree, because `Ps1FmvSubtitles` builds its path from the
compile-time `GAME_DATA_ROOT_JPN` (`.\assets\JPN\data\` in a dev build,
`.\jpn\data\` retail) rather than from `config.ini [Assets] Path`.

The asset migrator does the same job from the GUI, from either tab:

- **PC Assets** -> *Add PS1 assets from a PS1 disc image* -> *Prologue FMV
  subtitles*, decoding into `<asset base>/JPN/Data/jimaku*.png`
  (`PcMigrationOptions::ps1FmvSubtitles`, decoder
  `tools/asset_migrator/src/core/Jimaku.{h,cpp}`).
- **Director's Cut** -> *Prologue FMV subtitles* (on by default), decoding the DC
  disc's own `JIMAKU*.RGB` into `<target>/JPN/Data/jimaku*.png`
  (`DcMigrationOptions::fmvSubtitles`).

Both land in the JPN tree on purpose: that is the only path the loader reads
from, so a copy in the `DC/` overlay would never be loaded. The decoder is
byte-identical to `decode_jimaku.py` for all three planes (see
`tools/asset_migrator/README.md`); `re1am_selftest jimaku <image> <outdir>`
runs it headlessly.
