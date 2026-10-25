#include "core/Png.h"

#include <cstring>

namespace re1 {
namespace {

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        built = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void putBE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

void putChunk(std::vector<uint8_t>& out, const char type[4],
              const std::vector<uint8_t>& body) {
    putBE32(out, (uint32_t)body.size());
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());
    putBE32(out, crc32(&out[start], out.size() - start));
}

}  // namespace

std::vector<uint8_t> encodeGreyPng(int width, int height, const uint8_t* pixels) {
    // Scanlines, each prefixed with filter type 0 (none).
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(width + 1) * height);
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), pixels + (size_t)y * width,
                   pixels + (size_t)(y + 1) * width);
    }

    // zlib stream: header 0x78 0x01, stored blocks of at most 65535 bytes,
    // Adler-32 of the uncompressed data.
    std::vector<uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x01);
    size_t pos = 0;
    do {
        const size_t len = raw.size() - pos < 65535 ? raw.size() - pos : 65535;
        z.push_back(pos + len == raw.size() ? 1 : 0);  // BFINAL, BTYPE 00
        z.push_back((uint8_t)len);
        z.push_back((uint8_t)(len >> 8));
        z.push_back((uint8_t)~len);
        z.push_back((uint8_t)(~len >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    } while (pos < raw.size());
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    putBE32(z, (b << 16) | a);

    static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> out(kSig, kSig + 8);
    std::vector<uint8_t> ihdr;
    putBE32(ihdr, (uint32_t)width);
    putBE32(ihdr, (uint32_t)height);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(0);  // colour type: greyscale
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // no interlace
    putChunk(out, "IHDR", ihdr);
    putChunk(out, "IDAT", z);
    putChunk(out, "IEND", std::vector<uint8_t>());
    return out;
}

}  // namespace re1
