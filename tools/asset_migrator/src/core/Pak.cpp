#include "core/Pak.h"

#include <cstdio>
#include <unordered_map>

namespace re1 {

namespace {
constexpr int kWidth = 320;
constexpr int kHeight = 240;

inline uint16_t ch5(uint8_t v) {
    const int r = ((int)v + 4) >> 3;
    return (uint16_t)(r > 31 ? 31 : r);
}

void putU16(std::vector<uint8_t>* out, uint16_t v) {
    out->push_back((uint8_t)(v & 0xFF));
    out->push_back((uint8_t)(v >> 8));
}
void putU32(std::vector<uint8_t>* out, uint32_t v) {
    putU16(out, (uint16_t)(v & 0xFFFF));
    putU16(out, (uint16_t)(v >> 16));
}
void putI16(std::vector<uint8_t>* out, int16_t v) {
    putU16(out, (uint16_t)v);
}
}  // namespace

std::vector<uint16_t> rgbTo555(const std::vector<uint8_t>& rgb, int width,
                               int height) {
    std::vector<uint16_t> out((size_t)width * height);
    for (size_t i = 0, n = out.size(); i < n; ++i) {
        const uint8_t r = rgb[i * 3 + 0];
        const uint8_t g = rgb[i * 3 + 1];
        const uint8_t b = rgb[i * 3 + 2];
        out[i] = (uint16_t)((ch5(b) << 10) | (ch5(g) << 5) | ch5(r));
    }
    return out;
}

std::vector<uint8_t> makeBackgroundTim(const std::vector<uint8_t>& rgb) {
    const std::vector<uint16_t> pix = rgbTo555(rgb, kWidth, kHeight);
    std::vector<uint8_t> out;
    out.reserve(20 + pix.size() * 2);
    putU32(&out, 0x10);  // TIM magic
    putU32(&out, 2);     // 16bpp, no CLUT
    // Image block: length counts ONLY the pixel bytes (not the 12 that follow).
    putU32(&out, (uint32_t)(pix.size() * 2));
    putI16(&out, 0);           // x
    putI16(&out, 240);         // y
    putU16(&out, kWidth);
    putU16(&out, kHeight);
    for (uint16_t p : pix) putU16(&out, p);
    return out;
}

std::vector<uint8_t> lzwPack(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int nbits = 0;

    auto emit = [&](uint32_t code, int width) {
        acc = (acc << width) | code;
        nbits += width;
        while (nbits >= 8) {
            nbits -= 8;
            out.push_back((uint8_t)((acc >> nbits) & 0xFF));
        }
    };

    std::unordered_map<std::string, int> table;
    int nextCode = 0x103;
    int width = 9;
    const int kMax = 0x1FFF;

    std::string cur;
    for (uint8_t byte : data) {
        std::string nxt = cur;
        nxt.push_back((char)byte);
        if (cur.empty()) {
            cur = nxt;
            continue;
        }
        if (table.count(nxt)) {
            cur = nxt;
            continue;
        }
        emit(cur.size() > 1 ? (uint32_t)table[cur] : (uint32_t)(uint8_t)cur[0],
             width);
        if (nextCode > kMax) {
            emit(0x102, width);
            table.clear();
            nextCode = 0x103;
            width = 9;
        } else {
            table[nxt] = nextCode++;
            // The decoder adds its entry AFTER reading the next code, so the
            // width has to grow one code early or it reads too few bits.
            if (nextCode > (1 << width) - 1 && width < 13) {
                emit(0x101, width);
                ++width;
            }
        }
        cur.assign(1, (char)byte);
    }
    if (!cur.empty()) {
        emit(cur.size() > 1 ? (uint32_t)table[cur] : (uint32_t)(uint8_t)cur[0],
             width);
    }
    emit(0x100, width);
    if (nbits) out.push_back((uint8_t)((acc << (8 - nbits)) & 0xFF));
    return out;
}

std::string pakName(char stageDigit, int roomId, int cam) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "RC%c%02X%X.pak", stageDigit, roomId, cam);
    return buf;
}

}  // namespace re1
