#include "core/Tim.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace re1 {
namespace {

uint16_t getU16(const std::vector<uint8_t>& d, size_t off) {
    return (uint16_t)(d[off] | (d[off + 1] << 8));
}
uint32_t getU32(const std::vector<uint8_t>& d, size_t off) {
    return (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8) |
           ((uint32_t)d[off + 2] << 16) | ((uint32_t)d[off + 3] << 24);
}
void setU16(std::vector<uint8_t>* d, size_t off, uint16_t v) {
    (*d)[off] = (uint8_t)(v & 0xFF);
    (*d)[off + 1] = (uint8_t)(v >> 8);
}
void appendU16(std::vector<uint8_t>* d, uint16_t v) {
    d->push_back((uint8_t)(v & 0xFF));
    d->push_back((uint8_t)(v >> 8));
}
void appendU32(std::vector<uint8_t>* d, uint32_t v) {
    appendU16(d, (uint16_t)(v & 0xFFFF));
    appendU16(d, (uint16_t)(v >> 16));
}

}  // namespace

bool timReadClut(const std::vector<uint8_t>& data, TimClut* out,
                 std::string* error) {
    if (data.size() < 20) {
        if (error) *error = "TIM too small";
        return false;
    }
    const uint32_t flags = getU32(data, 4);
    if (!(flags & 8)) {
        if (error) *error = "TIM has no CLUT block";
        return false;
    }
    const uint32_t blen = getU32(data, 8);
    out->x = getU16(data, 12);
    out->y = getU16(data, 14);
    out->w = getU16(data, 16);
    out->h = getU16(data, 18);
    (void)blen;
    const size_t need = 20 + (size_t)out->w * out->h * 2;
    if (data.size() < need) {
        if (error) *error = "TIM CLUT past the end";
        return false;
    }
    out->rows.assign(out->h, {});
    for (int r = 0; r < out->h; ++r) {
        out->rows[r].resize(out->w);
        for (int c = 0; c < out->w; ++c)
            out->rows[r][c] = getU16(data, 20 + ((size_t)r * out->w + c) * 2);
    }
    return true;
}

std::vector<uint8_t> timWithClut(
    const std::vector<uint8_t>& data, const TimClut& clut,
    const std::vector<std::vector<uint16_t>>& rows) {
    const int w = rows.empty() ? 0 : (int)rows[0].size();
    std::vector<uint8_t> body;
    for (const auto& row : rows)
        for (uint16_t v : row) appendU16(&body, v);
    std::vector<uint8_t> block;
    appendU32(&block, (uint32_t)(12 + body.size()));
    appendU16(&block, (uint16_t)clut.x);
    appendU16(&block, (uint16_t)clut.y);
    appendU16(&block, (uint16_t)w);
    appendU16(&block, (uint16_t)rows.size());
    block.insert(block.end(), body.begin(), body.end());

    const uint32_t oldLen = getU32(data, 8);
    std::vector<uint8_t> out;
    out.reserve(data.size() - oldLen + block.size());
    out.insert(out.end(), data.begin(), data.begin() + 8);
    out.insert(out.end(), block.begin(), block.end());
    if (8 + oldLen <= data.size())
        out.insert(out.end(), data.begin() + 8 + oldLen, data.end());
    return out;
}

std::vector<TimBlock> iterTimBlocks(const std::vector<uint8_t>& data) {
    std::vector<TimBlock> out;
    const size_t n = data.size();
    size_t off = 0;
    while (true) {
        // find the next 0x10 00 00 00 signature
        size_t i = std::string::npos;
        for (size_t k = off; k + 4 <= n; ++k) {
            if (data[k] == 0x10 && data[k + 1] == 0 && data[k + 2] == 0 &&
                data[k + 3] == 0) {
                i = k;
                break;
            }
        }
        if (i == std::string::npos || i + 8 > n) return out;
        off = i + 4;
        const uint32_t flags = getU32(data, i + 4);
        const int bpp = flags & 3;
        if ((flags & ~0x0Fu) || bpp > 1 || !(flags & 8)) continue;
        if (i + 20 > n) continue;
        const uint32_t cl = getU32(data, i + 8);
        const int cx = getU16(data, i + 12);
        const int cy = getU16(data, i + 14);
        const int cw = getU16(data, i + 16);
        const int ch = getU16(data, i + 18);
        if (cw != (bpp == 0 ? 16 : 256) || ch < 1 || ch > 256) continue;
        if (cl != 12u + (uint32_t)cw * ch * 2 || cx >= 1024 || cy >= 512)
            continue;
        const size_t p = i + 8 + cl;
        if (p + 12 > n) continue;
        const uint32_t pl = getU32(data, p);
        const int px = getU16(data, p + 4);
        const int py = getU16(data, p + 6);
        const int pw = getU16(data, p + 8);
        const int ph = getU16(data, p + 10);
        if (pw < 1 || pw > 1024 || ph < 1 || ph > 512 || px >= 1024 ||
            py >= 512)
            continue;
        if (pl != 12u + (uint32_t)pw * ph * 2 || p + pl > n) continue;
        TimBlock b;
        b.clutOff = i + 20;
        b.clutW = cw;
        b.clutH = ch;
        b.pixOff = p + 12;
        b.pixLen = (size_t)pw * ph * 2;
        b.bpp = bpp;
        out.push_back(b);
        off = p + pl;
    }
}

int foldTimTransparency(std::vector<uint8_t>* buf, const TimBlock& blk) {
    const int cw = blk.clutW;
    const int ch = blk.clutH;
    std::vector<std::vector<uint16_t>> rows(ch, std::vector<uint16_t>(cw));
    for (int r = 0; r < ch; ++r)
        for (int c = 0; c < cw; ++c)
            rows[r][c] = getU16(*buf, blk.clutOff + ((size_t)r * cw + c) * 2);

    std::set<int> zeros;
    for (int k = 0; k < cw; ++k) {
        bool all = true;
        for (int r = 0; r < ch; ++r) {
            if (rows[r][k] != 0) {
                all = false;
                break;
            }
        }
        if (all) zeros.insert(k);
    }
    if (zeros.empty()) return 0;

    int swap = -1;
    if (!zeros.count(0)) {
        swap = *zeros.begin();
        for (int r = 0; r < ch; ++r)
            std::swap(rows[r][0], rows[r][swap]);
        zeros.erase(swap);
        zeros.insert(0);
    }

    std::set<int> fold;
    for (int k : zeros)
        if (k != 0) fold.insert(k);
    if (fold.empty() && swap < 0) return 0;

    std::vector<uint8_t> pix(buf->begin() + blk.pixOff,
                             buf->begin() + blk.pixOff + blk.pixLen);
    int moved = 0;
    if (blk.bpp == 1) {
        uint8_t table[256];
        for (int k = 0; k < 256; ++k) table[k] = (uint8_t)k;
        for (int k : fold) table[k] = 0;
        if (swap >= 0) std::swap(table[0], table[swap]);
        for (int k = 0; k < 256; ++k)
            if (table[k] != k)
                for (uint8_t b : pix)
                    if (b == k) ++moved;
        for (auto& b : pix) b = table[b];
    } else {
        int nib[16];
        for (int k = 0; k < 16; ++k) nib[k] = k;
        for (int k : fold) nib[k] = 0;
        if (swap >= 0) std::swap(nib[0], nib[swap]);
        uint8_t table[256];
        for (int b = 0; b < 256; ++b)
            table[b] = (uint8_t)(nib[b & 0xF] | (nib[b >> 4] << 4));
        for (uint8_t b : pix) {
            if (nib[b & 0xF] != (b & 0xF)) ++moved;
            if (nib[b >> 4] != (b >> 4)) ++moved;
        }
        for (auto& b : pix) b = table[b];
    }

    std::copy(pix.begin(), pix.end(), buf->begin() + blk.pixOff);
    if (swap >= 0) {
        for (int r = 0; r < ch; ++r)
            for (int c = 0; c < cw; ++c)
                setU16(buf, blk.clutOff + ((size_t)r * cw + c) * 2, rows[r][c]);
    }
    return moved;
}

}  // namespace re1
