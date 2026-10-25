#pragma once
// JPN PS1 prologue FMV subtitles: the disc's DATA/JIMAKU*.RGB bitmap sets ->
// <tree>/Data/jimaku*.png, the 320-wide 8-bit greyscale images (one 18-row
// line after another) src/game/Ps1FmvSubtitles.cpp loads. A port of
// tools/decode_jimaku.py; the format is documented in
// docs/PS1_FMV_SUBTITLES.md.
//
// Only the JPN releases carry JIMAKU*.RGB; the USA discs have none, so the
// conversion skips them instead of failing.

#include "core/DiscImage.h"
#include "core/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace re1 {

// One 320x18 line of JIMAKU*.RGB -> 320*18 bytes. The disc keeps the lines as
// plain row-major 24 bpp greyscale; only the first byte of each pixel
// survives, because the original's blit writes that level into all three
// destination bytes and skips fully-zero pixels. False when `src` is shorter
// than one line.
bool decodeJimakuLine(const uint8_t* src, size_t srcSize, uint8_t* dst);

// Decode every JIMAKU*.RGB the image carries into `dataDir` as
// jimakuNN.png, named the way the game looks them up. `written` (optional)
// receives the number of planes written; 0 means the disc has no JIMAKU files
// (the USA releases), which is not an error. False only on a real failure.
bool convertJimakuSubtitles(const DiscImage& img, const std::string& dataDir,
                            const Progress& progress, int* written,
                            std::string* error);

}  // namespace re1
