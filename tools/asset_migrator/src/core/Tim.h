#pragma once
// TIM helpers: CLUT read/replace for the DC font, and the PS1 colour-key ->
// PC index-0 transparency relabel. Ports of the same code in
// tools/port_dc_assets.py.

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

struct TimClut {
    int x = 0, y = 0, w = 0, h = 0;
    std::vector<std::vector<uint16_t>> rows;
};

// Parse a TIM's CLUT block (requires a CLUT). False on a malformed TIM.
bool timReadClut(const std::vector<uint8_t>& data, TimClut* out,
                 std::string* error);

// `data` with its CLUT block replaced by `rows` (all the same width).
std::vector<uint8_t> timWithClut(const std::vector<uint8_t>& data,
                                 const TimClut& clut,
                                 const std::vector<std::vector<uint16_t>>& rows);

// One well-formed 4/8bpp + CLUT TIM block found inside a container.
struct TimBlock {
    size_t clutOff = 0;
    int clutW = 0, clutH = 0;
    size_t pixOff = 0, pixLen = 0;
    int bpp = 0;
};

// Every valid 4/8bpp TIM block in `data`, found by signature.
std::vector<TimBlock> iterTimBlocks(const std::vector<uint8_t>& data);

// Move transparent texels onto palette index 0, in place. Returns texels moved.
int foldTimTransparency(std::vector<uint8_t>* buf, const TimBlock& blk);

}  // namespace re1
