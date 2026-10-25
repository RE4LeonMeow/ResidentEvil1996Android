#include "core/Bss.h"

#include "core/Mdec.h"
#include "core/Pak.h"
#include "core/Util.h"

namespace re1 {

namespace {
constexpr size_t kFrameStride = 0x8000;
constexpr int kWidth = 320;
constexpr int kHeight = 240;
}  // namespace

std::vector<size_t> bssFrameOffsets(const std::vector<uint8_t>& data) {
    std::vector<size_t> offs;
    for (size_t off = 0; off < data.size(); off += kFrameStride) {
        if (off + 8 <= data.size() && mdecHasFrame(data.data(), data.size(), off))
            offs.push_back(off);
    }
    return offs;
}

bool convertBss(const std::vector<uint8_t>& data, char stageDigit, int roomId,
                const std::string& outDir, const Progress& progress,
                std::vector<std::string>* written, std::string* error) {
    const std::vector<size_t> offs = bssFrameOffsets(data);
    int cam = 0;
    for (size_t off : offs) {
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        std::vector<uint8_t> rgb;
        if (!mdecDecodeFrame(data.data(), data.size(), off, kWidth, kHeight, &rgb,
                             error)) {
            return false;
        }
        const std::vector<uint8_t> tim = makeBackgroundTim(rgb);
        const std::vector<uint8_t> blob = lzwPack(tim);
        const std::string name = pakName(stageDigit, roomId, cam);
        if (!writeFile(joinPath(outDir, name), blob)) {
            if (error) *error = "cannot write " + joinPath(outDir, name);
            return false;
        }
        if (written) written->push_back(name);
        ++cam;
    }
    return true;
}

}  // namespace re1
