#pragma once
// Minimal PNG writer (no zlib): 8-bit greyscale, filter 0, stored deflate
// blocks. The output is byte-identical to tools/decode_jimaku.py's write_png,
// which is how the two decoders are checked against each other. The game reads
// these with src/system/PngImage.cpp.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace re1 {

// `pixels` is width * height bytes, row-major.
std::vector<uint8_t> encodeGreyPng(int width, int height, const uint8_t* pixels);

}  // namespace re1
