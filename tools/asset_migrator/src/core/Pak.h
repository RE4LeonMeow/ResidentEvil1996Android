#pragma once
// PC .pak encoding: RGB -> 5-5-5 -> TIM -> LZW. A port of the encoder in
// tools/bss_to_pak.py / scripts/build_dc_assets.py.

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

// 8-bit RGB (width*height*3) -> packed PSX 5-5-5 (B<<10 | G<<5 | R), rounded.
std::vector<uint16_t> rgbTo555(const std::vector<uint8_t>& rgb, int width,
                               int height);

// The exact TIM Capcom's own background paks carry: 16bpp, VRAM (0,240),
// 320x240, with the image block length counting only the pixel bytes.
std::vector<uint8_t> makeBackgroundTim(const std::vector<uint8_t>& rgb);

// LZW stream for unpack_pakfile_ (0x00425ab0): 0x101 grows the code width,
// 0x102 resets the dictionary, 0x100 ends the stream.
std::vector<uint8_t> lzwPack(const std::vector<uint8_t>& data);

// "RC<stage><room:02X><cam>.pak".
std::string pakName(char stageDigit, int roomId, int cam);

}  // namespace re1
