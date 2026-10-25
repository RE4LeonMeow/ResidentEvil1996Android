#pragma once
// PlayStation MDEC ("BS") bitstream decoder - a C++ port of tools/mdec.py.
//
// Decodes one demultiplexed frame bitstream into RGB. Used for both the .BSS
// room backgrounds and the .STR movies.

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

// Decode the frame whose header starts at `off` in `data`. Writes
// width*height*3 bytes of RGB into `rgb`. Returns false and sets `error` on a
// malformed bitstream.
bool mdecDecodeFrame(const uint8_t* data, size_t size, size_t off, int width,
                     int height, std::vector<uint8_t>* rgb, std::string* error);

// True when a frame header (magic 0x3800) sits at `off`.
bool mdecHasFrame(const uint8_t* data, size_t size, size_t off);

}  // namespace re1
