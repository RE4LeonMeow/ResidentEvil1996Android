#include "core/Jimaku.h"

#include "core/Png.h"
#include "core/Util.h"

#include <cstdio>
#include <vector>

namespace re1 {
namespace {

// One subtitle line: 320x18 pixels, 24 bpp greyscale on the disc.
constexpr int kLineW = 320;
constexpr int kLineH = 18;
constexpr size_t kLineBytes = (size_t)kLineW * kLineH * 3;  // 17280
constexpr size_t kPlaneBytes = (size_t)kLineW * kLineH;      // 5760

// The three bitmap sets the resident exe binds, in CD file index order:
// 18 = JIMAKU00.RGB (prologue, 23 lines), 19 = JIMAKU01.RGB (7),
// 20 = JIMAKU02.RGB (5). Ps1FmvSubtitles_Begin() loads all three, so a disc
// with fewer cannot draw subtitles.
constexpr int kPlaneCount = 3;

// The two digits in JIMAKU<nn>.RGB, or -1 for any other name.
int jimakuIndex(const std::string& path) {
    const std::string name = toUpper(baseName(path));
    if (name.size() != 8 + 4) return -1;  // JIMAKU<nn>.RGB
    if (name.compare(0, 6, "JIMAKU") != 0) return -1;
    if (name.compare(8, 4, ".RGB") != 0) return -1;
    if (name[6] < '0' || name[6] > '9' || name[7] < '0' || name[7] > '9')
        return -1;
    return (name[6] - '0') * 10 + (name[7] - '0');
}

}  // namespace

bool decodeJimakuLine(const uint8_t* src, size_t srcSize, uint8_t* dst) {
    if (src == nullptr || dst == nullptr || srcSize < kLineBytes) return false;
    // Plain row-major 24 bpp: BlitSubtitleLine4bppTo24bpp (0x80037d98,
    // SLPS_009.98) walks the source 3 bytes at a time in the same order as the
    // 320-wide framebuffer rect it copies into, and only byte 0 is ever read.
    for (size_t i = 0; i < kPlaneBytes; ++i) dst[i] = src[i * 3];
    return true;
}

bool convertJimakuSubtitles(const DiscImage& img, const std::string& dataDir,
                            const Progress& progress, int* written,
                            std::string* error) {
    if (written) *written = 0;
    if (!img.isOpen()) {
        if (error) *error = "the disc image is not open";
        return false;
    }

    // The retail disc nests its files differently per release (the DC one puts
    // everything under /PSX), so the sets are picked out of the disc's own file
    // list by name instead of by a hard-coded path. One entry per plane, in
    // case a disc lists the same file twice.
    const DiscEntry* plane[kPlaneCount] = {nullptr, nullptr, nullptr};
    for (const auto& e : img.entries()) {
        if (e.directory) continue;
        const int idx = jimakuIndex(e.path);
        if (idx < 0 || idx >= kPlaneCount) continue;
        if (e.size == 0 || e.size % kLineBytes != 0) {
            if (error)
                *error = baseName(e.path) + " is " + std::to_string(e.size) +
                         " bytes, not a whole number of " +
                         std::to_string(kLineBytes) + "-byte lines";
            return false;
        }
        if (plane[idx] == nullptr) plane[idx] = &e;
    }
    int found = 0;
    for (int i = 0; i < kPlaneCount; ++i) {
        if (plane[i] != nullptr) ++found;
    }
    if (found == 0) {
        progress.info("PS1 subtitles: no JIMAKU*.RGB on this disc, skipped");
        return true;
    }

    std::vector<uint8_t> lines;
    for (int i = 0; i < kPlaneCount; ++i) {
        if (plane[i] == nullptr) continue;
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        std::vector<uint8_t> data;
        if (!img.readFile(*plane[i], &data)) {
            if (error) *error = "cannot read " + plane[i]->path;
            return false;
        }
        const size_t lineCount = data.size() / kLineBytes;
        lines.assign(lineCount * kPlaneBytes, 0);
        for (size_t l = 0; l < lineCount; ++l) {
            if (progress.isCancelled()) {
                if (error) *error = "cancelled";
                return false;
            }
            if (!decodeJimakuLine(&data[l * kLineBytes], kLineBytes,
                                  &lines[l * kPlaneBytes])) {
                if (error) *error = "cannot decode " + plane[i]->path;
                return false;
            }
        }
        char name[32];
        std::snprintf(name, sizeof(name), "jimaku%02d.png", i);
        const std::string out = joinPath(dataDir, name);
        // One 320-wide greyscale image per set, the lines stacked top to
        // bottom, 18 rows each.
        if (!writeFile(out, encodeGreyPng(kLineW, (int)lineCount * kLineH,
                                          lines.data()))) {
            if (error) *error = "cannot write " + out;
            return false;
        }
        progress.info("  " + baseName(plane[i]->path) + " -> " + out + " (" +
                      std::to_string(lineCount) + " lines)");
        if (written) ++*written;
    }
    if (found < kPlaneCount)
        progress.info("  warning: only " + std::to_string(found) +
                      " of the 3 subtitle planes are on the disc; the game "
                      "needs all three");
    return true;
}

}  // namespace re1
