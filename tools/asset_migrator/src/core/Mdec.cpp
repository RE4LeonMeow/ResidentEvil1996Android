#include "core/Mdec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace re1 {
namespace {

// ---------------------------------------------------------------------------
// Bit reader: 16-bit little-endian words consumed most-significant bit first.
// ---------------------------------------------------------------------------
class BitReader {
public:
    BitReader(const uint8_t* data, size_t size, size_t start) {
        const size_t n = start < size ? size - start : 0;
        const uint8_t* p = data + (start < size ? start : 0);
        m_buf.assign(p, p + n);
        if (m_buf.size() & 1) m_buf.push_back(0);
        for (size_t i = 0; i + 1 < m_buf.size(); i += 2)
            std::swap(m_buf[i], m_buf[i + 1]);
    }

    int peek(int n) const {
        const size_t byte = m_pos >> 3;
        if (byte >= m_buf.size()) return 0;
        const size_t want = (size_t)((n + 15) / 8 + 1);
        const size_t len = std::min(want, m_buf.size() - byte);
        uint64_t val = 0;
        for (size_t i = 0; i < len; ++i) val = (val << 8) | m_buf[byte + i];
        int shift = (int)(len * 8) - (int)(m_pos & 7) - n;
        if (shift < 0) {
            val <<= -shift;
            shift = 0;
        }
        return (int)((val >> shift) & ((1ull << n) - 1));
    }

    int read(int n) {
        const int v = peek(n);
        m_pos += n;
        return v;
    }
    void skip(int n) { m_pos += n; }

private:
    std::vector<uint8_t> m_buf;
    size_t m_pos = 0;
};

// ---------------------------------------------------------------------------
// Huffman tables (MPEG-1, Table B-12/13/14)
// ---------------------------------------------------------------------------
struct AcVal {
    int run = 0;
    int level = 0;
    int kind = 0;  // 0 = run/level, 1 = EOB, 2 = escape
};

using DcMap = std::unordered_map<int, int>;      // bits -> size
using AcMap = std::unordered_map<int, AcVal>;    // bits -> symbol

struct Tables {
    DcMap dcLuma[18];
    DcMap dcChroma[18];
    AcMap ac[18];
    std::vector<int> acLengths, dcLumaLengths, dcChromaLengths;

    // `arr` is the per-length table array; index by code length, then bits.
    void addDc(DcMap* arr, const char* code, int size) {
        const int len = (int)std::strlen(code);
        int bits = 0;
        for (const char* c = code; *c; ++c) bits = (bits << 1) | (*c - '0');
        arr[len][bits] = size;
    }
    void addAc(const char* code, int run, int level) {
        const int len = (int)std::strlen(code);
        int bits = 0;
        for (const char* c = code; *c; ++c) bits = (bits << 1) | (*c - '0');
        AcVal v;
        v.run = run;
        v.level = level;
        ac[len][bits] = v;
    }
    void addAcSpecial(const char* code, int kind) {
        const int len = (int)std::strlen(code);
        int bits = 0;
        for (const char* c = code; *c; ++c) bits = (bits << 1) | (*c - '0');
        AcVal v;
        v.kind = kind;
        ac[len][bits] = v;
    }
};

const Tables& tables() {
    static const Tables t = [] {
        Tables x;
        struct Dc {
            const char* code;
            int size;
        };
        static const Dc kLuma[] = {
            {"100", 0}, {"00", 1},   {"01", 2},   {"101", 3},  {"110", 4},
            {"1110", 5}, {"11110", 6}, {"111110", 7}, {"1111110", 8},
        };
        static const Dc kChroma[] = {
            {"00", 0},   {"01", 1},    {"10", 2},   {"110", 3},   {"1110", 4},
            {"11110", 5}, {"111110", 6}, {"1111110", 7}, {"11111110", 8},
        };
        for (const auto& d : kLuma) x.addDc(x.dcLuma, d.code, d.size);
        for (const auto& d : kChroma) x.addDc(x.dcChroma, d.code, d.size);

        struct Ac {
            const char* code;
            int run;
            int level;
        };
        static const Ac kAc[] = {
            {"11", 0, 1},          {"011", 1, 1},         {"0100", 0, 2},
            {"0101", 2, 1},        {"00101", 0, 3},       {"00110", 4, 1},
            {"00111", 3, 1},       {"000100", 7, 1},      {"000101", 6, 1},
            {"000110", 1, 2},      {"000111", 5, 1},      {"0000100", 2, 2},
            {"0000101", 9, 1},     {"0000110", 0, 4},     {"0000111", 8, 1},
            {"00100000", 13, 1},   {"00100001", 0, 6},    {"00100010", 12, 1},
            {"00100011", 11, 1},   {"00100100", 3, 2},    {"00100101", 1, 3},
            {"00100110", 0, 5},    {"00100111", 10, 1},   {"0000001000", 16, 1},
            {"0000001001", 5, 2},  {"0000001010", 0, 7},  {"0000001011", 2, 3},
            {"0000001100", 1, 4},  {"0000001101", 15, 1}, {"0000001110", 14, 1},
            {"0000001111", 4, 2},  {"000000010000", 0, 11}, {"000000010001", 8, 2},
            {"000000010010", 4, 3}, {"000000010011", 0, 10}, {"000000010100", 2, 4},
            {"000000010101", 7, 2}, {"000000010110", 21, 1}, {"000000010111", 20, 1},
            {"000000011000", 0, 9}, {"000000011001", 19, 1}, {"000000011010", 18, 1},
            {"000000011011", 1, 5}, {"000000011100", 3, 3}, {"000000011101", 0, 8},
            {"000000011110", 6, 2}, {"000000011111", 17, 1},
            {"0000000010000", 10, 2}, {"0000000010001", 9, 2},
            {"0000000010010", 5, 3},  {"0000000010011", 3, 4},
            {"0000000010100", 2, 5},  {"0000000010101", 1, 7},
            {"0000000010110", 1, 6},  {"0000000010111", 0, 15},
            {"0000000011000", 0, 14}, {"0000000011001", 0, 13},
            {"0000000011010", 0, 12}, {"0000000011011", 26, 1},
            {"0000000011100", 25, 1}, {"0000000011101", 24, 1},
            {"0000000011110", 23, 1}, {"0000000011111", 22, 1},
            {"00000000010000", 0, 31}, {"00000000010001", 0, 30},
            {"00000000010010", 0, 29}, {"00000000010011", 0, 28},
            {"00000000010100", 0, 27}, {"00000000010101", 0, 26},
            {"00000000010110", 0, 25}, {"00000000010111", 0, 24},
            {"00000000011000", 0, 23}, {"00000000011001", 0, 22},
            {"00000000011010", 0, 21}, {"00000000011011", 0, 20},
            {"00000000011100", 0, 19}, {"00000000011101", 0, 18},
            {"00000000011110", 0, 17}, {"00000000011111", 0, 16},
            {"000000000010000", 0, 40}, {"000000000010001", 0, 39},
            {"000000000010010", 0, 38}, {"000000000010011", 0, 37},
            {"000000000010100", 0, 36}, {"000000000010101", 0, 35},
            {"000000000010110", 0, 34}, {"000000000010111", 0, 33},
            {"000000000011000", 0, 32}, {"000000000011001", 1, 14},
            {"000000000011010", 1, 13}, {"000000000011011", 1, 12},
            {"000000000011100", 1, 11}, {"000000000011101", 1, 10},
            {"000000000011110", 1, 9},  {"000000000011111", 1, 8},
            {"0000000000010000", 1, 18}, {"0000000000010001", 1, 17},
            {"0000000000010010", 1, 16}, {"0000000000010011", 1, 15},
            {"0000000000010100", 6, 3},  {"0000000000010101", 16, 2},
            {"0000000000010110", 15, 2}, {"0000000000010111", 14, 2},
            {"0000000000011000", 13, 2}, {"0000000000011001", 12, 2},
            {"0000000000011010", 11, 2}, {"0000000000011011", 31, 1},
            {"0000000000011100", 30, 1}, {"0000000000011101", 29, 1},
            {"0000000000011110", 28, 1}, {"0000000000011111", 27, 1},
        };
        for (const auto& a : kAc) x.addAc(a.code, a.run, a.level);
        x.addAcSpecial("10", 1);       // EOB
        x.addAcSpecial("000001", 2);   // escape

        for (int n = 0; n < 18; ++n) {
            if (!x.ac[n].empty()) x.acLengths.push_back(n);
            if (!x.dcLuma[n].empty()) x.dcLumaLengths.push_back(n);
            if (!x.dcChroma[n].empty()) x.dcChromaLengths.push_back(n);
        }
        return x;
    }();
    return t;
}

// ---------------------------------------------------------------------------
// Dequantisation + IDCT
// ---------------------------------------------------------------------------
const int kZigzag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

const double kQuant[64] = {
    2,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83,
};

struct Idct {
    double m[8][8];
    Idct() {
        for (int x = 0; x < 8; ++x) {
            for (int u = 0; u < 8; ++u) {
                const double c = u == 0 ? std::sqrt(1.0 / 8) : std::sqrt(2.0 / 8);
                m[x][u] = c * std::cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0);
            }
        }
    }
};

const Idct& idct() {
    static const Idct v;
    return v;
}

void idct8x8(const double* block, double* out) {
    const Idct& I = idct();
    double tmp[8][8];
    for (int x = 0; x < 8; ++x)
        for (int v = 0; v < 8; ++v) {
            double s = 0;
            for (int u = 0; u < 8; ++u) s += I.m[x][u] * block[u * 8 + v];
            tmp[x][v] = s;
        }
    for (int x = 0; x < 8; ++x)
        for (int y = 0; y < 8; ++y) {
            double s = 0;
            for (int v = 0; v < 8; ++v) s += tmp[x][v] * I.m[y][v];
            out[x * 8 + y] = s;
        }
}

bool huffAc(BitReader& br, AcVal* out) {
    const Tables& t = tables();
    for (int n : t.acLengths) {
        auto it = t.ac[n].find(br.peek(n));
        if (it != t.ac[n].end()) {
            br.skip(n);
            *out = it->second;
            return true;
        }
    }
    return false;
}

bool huffDc(BitReader& br, const DcMap* map, const std::vector<int>& lengths,
            int* out) {
    for (int n : lengths) {
        auto it = map[n].find(br.peek(n));
        if (it != map[n].end()) {
            br.skip(n);
            *out = it->second;
            return true;
        }
    }
    return false;
}

bool decodeBlock(BitReader& br, int qscale, int* dcPrev, int plane, int version,
                 double* out) {
    const Tables& t = tables();
    double coeff[64];
    std::memset(coeff, 0, sizeof(coeff));

    int dc;
    if (version >= 3) {
        int size = 0;
        const DcMap* map = plane == 0 ? t.dcLuma : t.dcChroma;
        const std::vector<int>& lens =
            plane == 0 ? t.dcLumaLengths : t.dcChromaLengths;
        if (!huffDc(br, map, lens, &size)) return false;
        int diff = 0;
        if (size != 0) {
            const int bits = br.read(size);
            diff = (bits & (1 << (size - 1))) ? bits : bits - (1 << size) + 1;
        }
        dc = *dcPrev + diff * 4;
    } else {
        const int raw = br.read(10);
        dc = (raw & 0x200) ? raw - 1024 : raw;
    }
    *dcPrev = dc;
    coeff[0] = dc * kQuant[0];

    int i = 0;
    for (;;) {
        AcVal sym;
        if (!huffAc(br, &sym)) return false;
        if (sym.kind == 1) break;  // EOB
        int run = sym.run;
        int level = sym.level;
        if (sym.kind == 2) {  // escape
            run = br.read(6);
            level = br.read(10);
            if (level & 0x200) level -= 1024;
        } else {
            if (br.read(1)) level = -level;
        }
        i += run + 1;
        if (i > 63) break;
        const int pos = kZigzag[i];
        coeff[pos] = level * kQuant[pos] * qscale / 8.0;
    }

    idct8x8(coeff, out);
    return true;
}

}  // namespace

bool mdecHasFrame(const uint8_t* data, size_t size, size_t off) {
    if (off + 4 > size) return false;
    return (uint16_t)(data[off + 2] | (data[off + 3] << 8)) == 0x3800;
}

bool mdecDecodeFrame(const uint8_t* data, size_t size, size_t off, int width,
                     int height, std::vector<uint8_t>* rgb, std::string* error) {
    if (off + 8 > size) {
        if (error) *error = "MDEC frame header past the end";
        return false;
    }
    const int qscale = data[off + 4] | (data[off + 5] << 8);
    const int version = data[off + 6] | (data[off + 7] << 8);
    if (!mdecHasFrame(data, size, off)) {
        if (error) *error = "not an MDEC frame";
        return false;
    }

    const int mw = (width + 15) / 16;
    const int mh = (height + 15) / 16;
    const int cw = mw * 8, ch = mh * 8;
    const int yw = mw * 16, yh = mh * 16;

    std::vector<double> ybuf((size_t)yw * yh, 0.0);
    std::vector<double> cbbuf((size_t)cw * ch, 0.0);
    std::vector<double> crbuf((size_t)cw * ch, 0.0);

    BitReader br(data, size, off + 8);
    int dc[3] = {0, 0, 0};  // Y, Cb, Cr

    double block[64];
    for (int mx = 0; mx < mw; ++mx) {
        for (int my = 0; my < mh; ++my) {
            if (!decodeBlock(br, qscale, &dc[2], 1, version, block)) {
                if (error) *error = "bad MDEC bitstream (Cr)";
                return false;
            }
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    crbuf[(size_t)(my * 8 + r) * cw + mx * 8 + c] =
                        block[r * 8 + c];

            if (!decodeBlock(br, qscale, &dc[1], 2, version, block)) {
                if (error) *error = "bad MDEC bitstream (Cb)";
                return false;
            }
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    cbbuf[(size_t)(my * 8 + r) * cw + mx * 8 + c] =
                        block[r * 8 + c];

            for (int k = 0; k < 4; ++k) {
                if (!decodeBlock(br, qscale, &dc[0], 0, version, block)) {
                    if (error) *error = "bad MDEC bitstream (Y)";
                    return false;
                }
                const int oy = my * 16 + (k >> 1) * 8;
                const int ox = mx * 16 + (k & 1) * 8;
                for (int r = 0; r < 8; ++r)
                    for (int c = 0; c < 8; ++c)
                        ybuf[(size_t)(oy + r) * yw + ox + c] = block[r * 8 + c];
            }
        }
    }

    rgb->assign((size_t)width * height * 3, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double yy = ybuf[(size_t)y * yw + x] + 128.0;
            const double cb = cbbuf[(size_t)(y / 2) * cw + x / 2];
            const double cr = crbuf[(size_t)(y / 2) * cw + x / 2];
            const double r = yy + 1.402 * cr;
            const double g = yy - 0.3437 * cb - 0.7143 * cr;
            const double b = yy + 1.772 * cb;
            auto clamp8 = [](double v) -> uint8_t {
                const int i = (int)(v + 0.5);
                return (uint8_t)(i < 0 ? 0 : (i > 255 ? 255 : i));
            };
            uint8_t* px = rgb->data() + ((size_t)y * width + x) * 3;
            px[0] = clamp8(r);
            px[1] = clamp8(g);
            px[2] = clamp8(b);
        }
    }
    return true;
}

}  // namespace re1
