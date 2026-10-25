#include "Ps1EndingCredits.h"

#include "../DebugPrint.h"
#include "../Globals.h"
#include "../marni/MarniDX.h"
#include "../marni/MarniSystem.h"
#include "../platform/platform.h"
#include "../system/AssetPath.h"
#include "FileLoader.h"

#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <vector>

namespace {

constexpr size_t kStfHeaderSize = 0x200;
constexpr size_t kStripPixels = 0x1000;
constexpr size_t kDecodedStripBytes = kStripPixels + 1;
constexpr int kStripWidth = 256;
constexpr int kStripHeight = 16;
constexpr int kAtlasPadding = 2;
constexpr int kAtlasStride = kStripHeight + kAtlasPadding * 2;
constexpr int kMaxStrips = 512;
// Staff roll (ENDING.EXE mode 0, the plate == 1 endings 1-3). Frame numbers
// are the overlay's 60 Hz loop counter (DC 0x800e66e0 / OG 0x800e6804).
// The scroll accumulator starts once the counter passes 0x3b and gains 0x8000
// (half a pixel) a frame; every 0x100000 (16 px, 32 frames) the next strip is
// spawned at y = 256 (0x800e1fb0), until the accumulator reaches 0xce40000.
constexpr int kScrollStart = 0x3c;
constexpr int kSpawnInterval = 32;
constexpr int kMaxScrollStrips = 0xce4 / kStripHeight;   // 206 spawns
constexpr int kScrollSpawnY = 256;
constexpr int kScrollFreeze = 0x1af4;   // counter > 0x1af3: type 0 -> 1, strips stop
constexpr int kScrollFade = 0x1ba8;     // every strip gets fadeVel 0xfc00
constexpr int kScrollFadeStep = 4;      // 0xfc00 >> 8: brightness 0x80 -> 0 in 32 frames
constexpr int kLogoEnd = 0x1c20;        // counter > 0x1c20 leaves the loop
// Cast roll (mode 1, the plate == 0 endings 4-7). Its loop runs once per
// decoded STR frame - the main counter gains 4 a pass so it still reads in
// 60 Hz units - and every table below is keyed on the STR frame number
// (0x800e64f8). Nothing here draws a still: the only pictures are the movie,
// decoded to VRAM x=0x240 and drawn as a 256+64 sprite pair, and the strips.
//
// The roll is clocked in the movie's own milliseconds: the frame number every
// table keys on is the presented frame's stream time read off that timeline
// (CastFrameAt), not the game's wall clock. The backend paces playback on the
// audio cursor, which measures ~205 ms behind the wall clock for the whole
// roll (measured: lead steady at -205 ms, drifting 13 ms over 120 s), so a
// wall clock ran the boxes a full three 15 fps frames ahead of the picture and
// they cleared before the moment they cover had arrived. The PS1's tables stay
// in frames; only the clock feeding them is in milliseconds, and it is the
// picture's.
constexpr int kCastFps = 15;
constexpr int kCastEnd = 0x690;          // frame > 0x690: mode 2, the BIO logo
constexpr int kCastFadeStep = 0x1100;    // fadeVel of the movie and the panel
constexpr int kCastRows = 15;            // strips per panel (256x240 sprite)
constexpr int kLogoFps = 30;             // mode 2 adds 2 to the counter a pass

struct PanelEvent {   // 0x800e5bd8 / 0x800e5c58: {start, end, screen x}
    short start;
    short end;
    short x;
};

static const PanelEvent kPanelEvents[2][19] = {
    {
        {61, 166, 160}, {181, 226, 0}, {241, 286, 0}, {301, 346, 0},
        {361, 406, 0}, {421, 466, 160}, {481, 519, 160}, {534, 639, 160},
        {654, 699, 0}, {714, 759, 0}, {774, 819, 0}, {834, 879, 0},
        {894, 999, 160}, {1014, 1059, 160}, {1074, 1111, 160},
        {1126, 1171, 0}, {1186, 1231, 0}, {1246, 1351, 32},
        {1366, 1486, 160},
    },
    {
        {61, 161, 160}, {176, 221, 0}, {236, 281, 0}, {296, 341, 0},
        {356, 396, 0}, {411, 456, 160}, {471, 516, 160}, {531, 631, 160},
        {646, 691, 0}, {706, 751, 0}, {766, 811, 0}, {826, 871, 0},
        {886, 986, 160}, {1001, 1046, 160}, {1061, 1106, 160},
        {1121, 1166, 0}, {1181, 1226, 0}, {1241, 1371, 32},
        {1386, 1486, 160},
    },
};

struct MovieEvent {   // 0x800e5ce0 / 0x800e5d50: {start, end, x, y}
    short start;
    short end;
    short x;
    short y;
};

static const MovieEvent kMovieEvents[2][13] = {
    {
        {61, 166, -10, 0}, {181, 286, 10, 0}, {301, 406, 10, 0},
        {421, 519, -10, 0}, {534, 639, -10, 0}, {654, 759, 10, 0},
        {774, 879, 10, 0}, {894, 999, -10, 0}, {1014, 1111, -10, 0},
        {1126, 1231, 10, 0}, {1246, 1351, 10, 0}, {1366, 1486, -10, 0},
        {1501, 1666, 0, 0},
    },
    {
        {61, 161, -10, 0}, {176, 281, 10, 0}, {296, 396, 10, 0},
        {411, 516, -10, 0}, {531, 631, -10, 0}, {646, 751, 10, 0},
        {766, 871, 10, 0}, {886, 986, -10, 0}, {1001, 1106, -10, 0},
        {1121, 1226, 10, 0}, {1241, 1371, 10, 0}, {1386, 1486, -10, 0},
        {1501, 1666, 0, 0},
    },
};

// 0x800e5dc8 / 0x800e5de0, run by 0x800e2654: two black GsBOXFs, A 320x120
// and B 160x240, parked off screen until `start`, then A at y and B over the
// left half; from `end` both move by (vx, vy) a frame until B's overlay x
// leaves [-320, 160] (screen [-160, 320]).
//
// Rows are {start, end, x, y, vx, vy} (0x800e5dc8: x is always 0), one per
// character; 0x800e3050 adds the velocity BEFORE sorting the boxes, so the
// first move is already on screen at `end`. Every shot cut in STFC/STFJ lands
// exactly on an event's `start` when STR frame = movie frame index + 1, so
// the tables only line up if the backend numbers the pictures from 0: the
// Media Foundation path once took the first picture's B-frame delay (2
// frames) as its position and slid these boxes away 2 frames early.
struct WipeEvent {
    short start;
    short end;
    short y;
    short vx;
    short vy;
};

static const WipeEvent kWipeEvents[2] = {
    {774, 834, 130, -20, 20},
    {301, 375, 130, -20, 20},
};

// GsSPRITE x = (0x20 << 16 >> 16) - 0xa0 about the 160,120 draw offset.
constexpr int kCreditsX = 32;

static BYTE s_staffBuffer[0x30000];
static BYTE s_bioBuffer[0x4000];
static std::vector<DWORD> s_atlas;
static DWORD s_bioPixels[256 * 68];
static MarniHandle s_stripTexture = MARNI_NULL_HANDLE;
static MarniHandle s_bioTexture = MARNI_NULL_HANDLE;
static int s_stripCount = 0;
static int s_frame = 0;
static DWORD s_startTimeMs = 0;
static int s_endingId = 0;
static int s_characterId = 0;
static BOOL s_active = FALSE;
static BOOL s_videoCanRender = FALSE;
static BOOL s_clockPending = FALSE;   // re-anchor s_startTimeMs on the first frame

bool ReadU16(const BYTE* data, size_t size, size_t offset, uint16_t& value)
{
    if (offset + 2 > size) return false;
    value = static_cast<uint16_t>(data[offset]) |
            (static_cast<uint16_t>(data[offset + 1]) << 8);
    return true;
}

bool ReadU32(const BYTE* data, size_t size, size_t offset, uint32_t& value)
{
    if (offset + 4 > size) return false;
    value = static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8) |
            (static_cast<uint32_t>(data[offset + 2]) << 16) |
            (static_cast<uint32_t>(data[offset + 3]) << 24);
    return true;
}

DWORD PsxColor(uint16_t color, bool transparentBit)
{
    if (transparentBit && (color & 0x8000u) != 0) return 0;
    const int r = ((color >> 10) & 0x1fu) * 255 / 31;
    const int g = ((color >> 5) & 0x1fu) * 255 / 31;
    const int b = (color & 0x1fu) * 255 / 31;
    return 0xff000000u | (static_cast<DWORD>(b) << 16) |
           (static_cast<DWORD>(g) << 8) | static_cast<DWORD>(r);
}

bool BuildStfAtlas(const BYTE* data, size_t size, std::vector<DWORD>& atlas,
                   int& stripCount)
{
    if (data == NULL || size < kStfHeaderSize + 6) return false;

    uint16_t palette[256];
    for (size_t i = 0; i < 256; ++i) {
        if (!ReadU16(data, size, i * 2, palette[i])) return false;
    }

    atlas.clear();
    std::vector<std::vector<BYTE>> decoded;
    size_t offset = kStfHeaderSize;
    stripCount = 0;
    while (offset + 6 <= size && stripCount < kMaxStrips) {
        uint16_t next = 0;
        uint16_t countOffset = 0;
        uint16_t valueOffset = 0;
        if (!ReadU16(data, size, offset, next) ||
            !ReadU16(data, size, offset + 2, countOffset) ||
            !ReadU16(data, size, offset + 4, valueOffset)) {
            return false;
        }
        if (next == 0xffffu || next == 0) break;
        if (countOffset == 0xffffu || valueOffset == 0xffffu) {
            if (valueOffset != 0xffffu || countOffset > size - offset ||
                size - offset - countOffset < kDecodedStripBytes) {
                return false;
            }
        } else if (countOffset > size - offset || valueOffset > size - offset) {
            return false;
        }

        BYTE pixels[kDecodedStripBytes];
        if (valueOffset == 0xffffu) {
            std::memcpy(pixels, data + offset + countOffset, kDecodedStripBytes);
        } else {
            size_t countPos = offset + countOffset;
            size_t valuePos = offset + valueOffset;
            size_t output = 0;
            bool terminated = false;
            while (countPos + 2 <= size) {
                uint16_t run = 0;
                if (!ReadU16(data, size, countPos, run)) return false;
                countPos += 2;
                if (run == 0) {
                    terminated = true;
                    break;
                }
                if (valuePos >= size || run > kDecodedStripBytes - output) return false;
                std::memset(pixels + output, data[valuePos], run);
                output += run;
                ++valuePos;
            }
            if (!terminated || output != kDecodedStripBytes) return false;
        }

        decoded.emplace_back(pixels, pixels + kStripPixels);
        ++stripCount;

        if (next > size - offset) return false;
        const size_t nextOffset = offset + next;
        if (nextOffset <= offset) return false;
        offset = nextOffset;
    }
    // The strips are 8-bit CLUT sprites with no ABE (attribute 0x01000000),
    // so the GPU's only transparency is a CLUT colour of exactly 0x0000.
    atlas.reserve(decoded.size() * kStripWidth * kAtlasStride);
    for (const std::vector<BYTE>& strip : decoded) {
        for (int y = 0; y < kAtlasStride; ++y) {
            const int sourceY = y - kAtlasPadding;
            for (int x = 0; x < kStripWidth; ++x) {
                DWORD color = 0;
                if (sourceY >= 0 && sourceY < kStripHeight) {
                    const BYTE index = strip[sourceY * kStripWidth + x];
                    if (palette[index] != 0) color = PsxColor(palette[index], false);
                }
                atlas.push_back(color);
            }
        }
    }
    return stripCount != 0;
}

bool BuildBioTexture(const BYTE* data, size_t size)
{
    if (data == NULL || size < 12 || data[0] != 0x10) return false;

    const uint16_t flags = static_cast<uint16_t>(data[4]) |
                           (static_cast<uint16_t>(data[5]) << 8);
    size_t offset = 8;
    uint16_t palette[16] = {};
    bool haveClut = false;
    int imageWidth = 0;
    int imageHeight = 0;
    size_t imageData = 0;
    size_t imageDataSize = 0;

    while (offset + 4 <= size) {
        uint32_t blockSize = 0;
        if (!ReadU32(data, size, offset, blockSize)) return false;
        if (blockSize < 4 || blockSize > size - offset) return false;
        const BYTE* block = data + offset + 4;
        const size_t blockBytes = blockSize - 4;
        if (blockBytes >= 8) {
            uint16_t x = 0;
            uint16_t y = 0;
            uint16_t w = 0;
            uint16_t h = 0;
            if (ReadU16(block, blockBytes, 0, x) &&
                ReadU16(block, blockBytes, 2, y) &&
                ReadU16(block, blockBytes, 4, w) &&
                ReadU16(block, blockBytes, 6, h)) {
                if (w == 16 && h >= 1 && blockBytes >= 12) {
                    for (int i = 0; i < 16; ++i) {
                        if (!ReadU16(block, blockBytes, 8 + i * 2, palette[i])) return false;
                    }
                    haveClut = true;
                }
                if (w >= 1 && h >= 1 && blockBytes >= 12 && x < 1024 && y < 512) {
                    const int bpp = flags & 3;
                    imageWidth = bpp == 0 ? static_cast<int>(w) * 4 :
                                 bpp == 1 ? static_cast<int>(w) * 2 :
                                             static_cast<int>(w);
                    imageHeight = static_cast<int>(h);
                    imageData = offset + 4 + 8;
                    imageDataSize = blockBytes - 8;
                }
            }
        }
        offset += blockSize;
        if (offset >= size) break;
    }

    if (!haveClut || imageWidth != 256 || imageHeight != 68) return false;
    const size_t pixelCount = static_cast<size_t>(imageWidth) * imageHeight;
    const size_t packedSize = (pixelCount + 1) / 2;
    if (imageData + packedSize > size || imageDataSize < packedSize) return false;

    for (size_t i = 0; i < pixelCount; ++i) {
        const size_t word = i / 4;
        const size_t nibble = i & 3;
        uint16_t packed = 0;
        if (!ReadU16(data, size, imageData + word * 2, packed)) return false;
        const int index = (packed >> (nibble * 4)) & 0xf;
        s_bioPixels[i] = PsxColor(palette[index], true);
    }
    s_bioTexture = MARNI_NULL_HANDLE;
    return MarniCreateTexture(256, 68, 32, s_bioPixels, &s_bioTexture) == TRUE;
}

bool EndsWithIgnoreCase(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) return false;
    const size_t textLength = std::strlen(text);
    const size_t suffixLength = std::strlen(suffix);
    if (suffixLength > textLength) return false;
    const char* tail = text + textLength - suffixLength;
    for (size_t i = 0; i < suffixLength; ++i) {
        char a = tail[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

bool ResolveMovie(const char* stem, char* out, size_t outSize)
{
    const char* extensions[] = { ".mp4", ".avi" };
    for (const char* extension : extensions) {
        char candidate[512];
        const int n = std::snprintf(candidate, sizeof(candidate),
                                    GAME_DATA_ROOT "movie\\%s%s", stem, extension);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(candidate)) continue;
        char resolved[512];
        const char* resolvedPath = ResolveAssetRoot(candidate, resolved, sizeof(resolved));
        if (resolvedPath == NULL) continue;
        char normalized[512];
        plat_normalize_path(resolvedPath, normalized, sizeof(normalized));
        FILE* file = std::fopen(normalized, "rb");
        if (file == NULL) continue;
        std::fclose(file);
        if (outSize == 0) return false;
        std::strncpy(out, normalized, outSize - 1);
        out[outSize - 1] = '\0';
        return true;
    }
    return false;
}

// A fadeLevel driven by an event's +/-0x1100 fadeVel, one step per STR frame:
// up from 0 at `start`, saturating at 0x8000 by sign wrap, down from `end`.
int CastFadeLevel(int frame, int start, int end)
{
    if (frame < start) return 0;
    if (frame < end) {
        const int level = (frame - start + 1) * kCastFadeStep;
        return level < 0x8000 ? level : 0x8000;
    }
    int level = (end - start) * kCastFadeStep;
    if (level > 0x8000) level = 0x8000;
    level -= (frame - end + 1) * kCastFadeStep;
    return level > 0 ? level : 0;
}

// Sprite colour for a fadeLevel: colour 0x80 (level 0x8000) is the neutral 1.0.
DWORD Grey(int level)
{
    int c = (level >> 8) * 255 / 0x80;
    if (c > 255) c = 255;
    if (c < 0) c = 0;
    return 0xff000000u | (static_cast<DWORD>(c) << 16) |
           (static_cast<DWORD>(c) << 8) | static_cast<DWORD>(c);
}

void DrawStrip(MarniDX* dx, int strip, int x, int y, float scaleX, float scaleY,
               DWORD color, MarniBlend blend)
{
    const float atlasHeight = static_cast<float>(s_stripCount * kAtlasStride);
    const float v0 = static_cast<float>(strip * kAtlasStride + kAtlasPadding) / atlasHeight;
    const float v1 = static_cast<float>(strip * kAtlasStride + kAtlasPadding + kStripHeight) /
                     atlasHeight;
    dx->DrawSprite(x * scaleX, y * scaleY, kStripWidth * scaleX, kStripHeight * scaleY,
                   0.0f, v0, 1.0f, v1, color, s_stripTexture, MARNI_SAMPLER_POINT, blend);
}

}

BOOL Ps1EndingCredits_IsEnabled(void)
{
    return (g_bDcMode || g_bPs1EndingCredits) ? TRUE : FALSE;
}

BOOL Ps1EndingCredits_Begin(int endingId, int characterId)
{
    Ps1EndingCredits_End();
    if (!Ps1EndingCredits_IsEnabled()) return FALSE;

    const char* staffPath = endingId <= 3
                                ? GAME_DATA_ROOT "data\\staff2.stf"
                                : GAME_DATA_ROOT "data\\staff.stf";
    size_t staffSize = LoadFile(staffPath, s_staffBuffer, 0x20);
    if ((staffSize == static_cast<size_t>(-1) ||
         staffSize > sizeof(s_staffBuffer)) && endingId <= 3) {
        staffPath = GAME_DATA_ROOT "data\\staff.stf";
        staffSize = LoadFile(staffPath, s_staffBuffer, 0x20);
    }
    if (staffSize == static_cast<size_t>(-1) ||
        staffSize > sizeof(s_staffBuffer)) {
        dbg_printf("[credits] missing %s\n", staffPath);
        return FALSE;
    }

    if (!BuildStfAtlas(s_staffBuffer, staffSize, s_atlas, s_stripCount)) {
        dbg_printf("[credits] invalid %s\n", staffPath);
        Ps1EndingCredits_End();
        return FALSE;
    }

    s_endingId = endingId;
    s_characterId = characterId;
    s_frame = 0;
    s_startTimeMs = plat_time_ms();
    s_videoCanRender = FALSE;
    s_active = TRUE;

    if (!MarniCreateTexture(256, s_stripCount * kAtlasStride, 32,
                            s_atlas.data(), &s_stripTexture)) {
        dbg_printf("[credits] texture creation failed\n");
        Ps1EndingCredits_End();
        return FALSE;
    }

    const size_t bioSize = LoadFile(GAME_DATA_ROOT "data\\bio.tim",
                                   s_bioBuffer, 0x20);
    if (bioSize != static_cast<size_t>(-1) && bioSize <= sizeof(s_bioBuffer)) {
        if (!BuildBioTexture(s_bioBuffer, bioSize)) {
            s_bioTexture = MARNI_NULL_HANDLE;
        }
    }
    return TRUE;
}

void Ps1EndingCredits_End(void)
{
    MarniDX* dx = Marni_DX();
    if (dx != NULL) {
        if (s_stripTexture != MARNI_NULL_HANDLE) dx->DestroyTexture(s_stripTexture);
        if (s_bioTexture != MARNI_NULL_HANDLE) dx->DestroyTexture(s_bioTexture);
    }
    s_stripTexture = MARNI_NULL_HANDLE;
    s_bioTexture = MARNI_NULL_HANDLE;
    s_atlas.clear();
    s_stripCount = 0;
    s_frame = 0;
    s_startTimeMs = 0;
    s_endingId = 0;
    s_characterId = 0;
    s_active = FALSE;
    s_videoCanRender = FALSE;
    s_clockPending = FALSE;
}

BOOL Ps1EndingCredits_IsActive(void)
{
    return s_active;
}

BOOL Ps1EndingCredits_ResolveVideoPath(int fmvId, char* out, size_t outSize)
{
    if (!s_active || out == NULL || outSize == 0) return FALSE;
    // Both overlay modes stream the character's STF movie (ENDING.EXE
    // 0x800e5e88[player & 3]); the PC splits the same slot into staf_r (27,
    // the plate == 1 endings) and stfc_r/stfj_r/stfz_r (24-26).
    if (fmvId < 24 || fmvId > 27) return FALSE;
    const char* stem = s_characterId == 1 ? "STFJ" : "STFC";
    if (!ResolveMovie(stem, out, outSize)) return FALSE;
    return TRUE;
}

void Ps1EndingCredits_SetVideoPath(const char* path)
{
    s_videoCanRender = FALSE;
    if (!s_active || path == NULL || !EndsWithIgnoreCase(path, ".mp4")) return;
    s_videoCanRender = TRUE;
}

void Ps1EndingCredits_StartVideo(void)
{
    if (s_active) {
        s_frame = 0;
        s_startTimeMs = plat_time_ms();
        // Opening the movie (the backends decode its whole audio track up
        // front) takes a while after this; the first presented frame moves
        // the start to the picture's real time 0.
        s_clockPending = TRUE;
    }
}

static DWORD ElapsedMs(void)
{
    const DWORD now = plat_time_ms();
    return now >= s_startTimeMs ? now - s_startTimeMs : 0;
}

// An STR frame number as a position in the movie, in milliseconds (the same
// rounding as VideoPlayback.cpp's DcFrameToMs).
int CastFrameToMs(int frame)
{
    return (frame * 1000 + kCastFps / 2) / kCastFps;
}

// The movie's position in milliseconds as the 1-based 15 fps frame number the
// tables above key on, so a table read stays in the PS1's units while the
// clock feeding it is in milliseconds.
int CastFrameAt(int posMs)
{
    if (posMs < 0) posMs = 0;
    return (posMs * kCastFps + 500) / 1000 + 1;
}

static int CurrentFrame(void)
{
    const DWORD elapsed = ElapsedMs();
    const DWORD durationMs = (static_cast<DWORD>(kLogoEnd + 1) * 1000u) / 60u;
    return elapsed >= durationMs ? kLogoEnd + 1 :
           static_cast<int>((elapsed * 60u) / 1000u);
}

BOOL Ps1EndingCredits_IsFinished(void)
{
    // Only the staff roll ends on its own counter; the cast roll's logo tail
    // outlasts the stream.
    if (!s_active || !s_videoCanRender || s_endingId >= 4) return FALSE;
    return CurrentFrame() > kLogoEnd;
}

// Mode 2 (0x800e4a70): BIO.TIM as a GsSPRITE centred on the screen, scale
// 0x10000000 / z with z += v, v -= 4 each pass; fadeVel 0x300 in, 0xfe00 out
// once z passes 0xffff; removed once z passes 0x177ff or v runs out.
static void DrawCastLogo(MarniDX* dx, int pass, float scaleX, float scaleY)
{
    if (s_bioTexture == MARNI_NULL_HANDLE) return;
    int state = 0;
    int z = 0x2000;
    int v = 0x300;
    int level = 0;
    int fadeVel = 0x300;
    for (int i = 0; i <= pass; ++i) {
        if (state == 0) {
            state = 1;
        } else {
            z += v;
            v -= 4;
            if (state == 1) {
                if (z > 0x97ff) state = 2;
            } else if (state == 2) {
                if (z > 0xffff) {
                    fadeVel = -0x200;
                    state = 3;
                }
            } else if (z > 0x177ff || v < 1) {
                return;   // slot freed, then the 300-pass wait on black
            }
        }
        level += fadeVel;
        if (level >= 0x8000) level = 0x8000;
        if (level <= 0) {
            level = 0;
            fadeVel = 0;
        }
    }
    if (level == 0 || z <= 0) return;
    const float scale = 65536.0f / static_cast<float>(z);
    const float w = 256.0f * scale;
    const float h = 68.0f * scale;
    dx->DrawSprite((160.0f - w * 0.5f) * scaleX, (120.0f - h * 0.5f) * scaleY,
                   w * scaleX, h * scaleY, 0.0f, 0.0f, 1.0f, 1.0f,
                   Grey(level), s_bioTexture);
}

static void DrawCastRoll(MarniDX* dx, MarniHandle videoTexture, int frameMs,
                         float scaleX, float scaleY)
{
    const int chr = s_characterId == 1 ? 1 : 0;
    // The movie's own clock, in the tables' 1-based frame units, so the boxes,
    // the movie and the panel all step off the one timeline the picture runs
    // on. A wall clock started before the movie opened ran ahead of it: the
    // wipe slid away while the frames it hides were still on screen.
    const int frame = CastFrameAt(frameMs);
    s_frame = frame;

    if (frame > kCastEnd) {
        // Mode 2 runs on the VSync, not the stream: the anchored wall clock
        // keeps its zoom smooth between movie frames.
        const DWORD elapsed = ElapsedMs();
        const DWORD logoStartMs = static_cast<DWORD>(kCastEnd * 1000 / kCastFps);
        const DWORD logoMs = elapsed > logoStartMs ? elapsed - logoStartMs : 0;
        DrawCastLogo(dx, static_cast<int>((logoMs * kLogoFps) / 1000u), scaleX, scaleY);
        return;
    }

    // The movie sprite pair (pri 0xf, behind everything). Between events it
    // is faded out and the screen is black.
    if (videoTexture != MARNI_NULL_HANDLE) {
        for (const MovieEvent& event : kMovieEvents[chr]) {
            const int level = CastFadeLevel(frame, event.start, event.end);
            if (level == 0) continue;
            dx->DrawSprite(event.x * scaleX, event.y * scaleY,
                           320.0f * scaleX, 240.0f * scaleY, 0.0f, 0.0f, 1.0f, 1.0f,
                           Grey(level), videoTexture, MARNI_SAMPLER_POINT,
                           MARNI_BLEND_DISABLE);
            break;
        }
    }

    // The wipe boxes (pri 0xe, over the movie): A 320x120 at y and B 160x240
    // over the left half, from `start`, then both move by (vx, vy) a frame
    // from `end` until B's overlay x leaves [-320, 160] (screen [-160, 320]).
    const WipeEvent& wipe = kWipeEvents[chr];
    if (frame >= wipe.start) {
        const int moves = frame >= wipe.end ? frame - wipe.end + 1 : 0;
        int steps = 0;
        while (steps < moves) {
            ++steps;
            const int boxBX = steps * wipe.vx;
            if (boxBX < -160 || boxBX > 320) break;   // vx/vy zeroed after this move
        }
        const float boxAY = static_cast<float>(wipe.y + steps * wipe.vy);
        const float boxBX = static_cast<float>(steps * wipe.vx);
        dx->DrawSprite(0.0f, boxAY * scaleY, 320.0f * scaleX, 120.0f * scaleY,
                       0.0f, 0.0f, 1.0f, 1.0f, 0xff000000u, MARNI_NULL_HANDLE,
                       MARNI_SAMPLER_POINT, MARNI_BLEND_DISABLE);
        dx->DrawSprite(boxBX * scaleX, 0.0f, 160.0f * scaleX, 240.0f * scaleY,
                       0.0f, 0.0f, 1.0f, 1.0f, 0xff000000u, MARNI_NULL_HANDLE,
                       MARNI_SAMPLER_POINT, MARNI_BLEND_DISABLE);
    }

    // The credit panel (pri 2): 15 strips, the next 15 per event, at the
    // event's screen x. Its attribute 0x51000000 is ABE + ABR 1 (additive);
    // 0x800e29a0 clears ABE once the fade-in completes and sets it again for
    // the fade-out.
    const PanelEvent* events = kPanelEvents[chr];
    for (int i = 0; i < 19; ++i) {
        const int level = CastFadeLevel(frame, events[i].start, events[i].end);
        if (level == 0) continue;
        const MarniBlend blend = level < 0x8000 ? MARNI_BLEND_ADD : MARNI_BLEND_ALPHA;
        for (int row = 0; row < kCastRows; ++row) {
            const int strip = i * kCastRows + row;
            if (strip >= s_stripCount) break;
            DrawStrip(dx, strip, events[i].x, row * kStripHeight, scaleX, scaleY,
                      Grey(level), blend);
        }
        break;
    }
}

BOOL Ps1EndingCredits_RenderFrame(MarniHandle videoTexture, int frameMs)
{
    MarniDX* dx = Marni_DX();
    if (!s_active || !s_videoCanRender || s_stripTexture == MARNI_NULL_HANDLE ||
        dx == NULL) {
        return FALSE;
    }

    // The overlay's 320x240 space is the whole backbuffer, like the movie quad.
    DWORD backWidth = 0;
    DWORD backHeight = 0;
    dx->GetBackBufferSize(&backWidth, &backHeight);
    if (backWidth == 0 || backHeight == 0) return FALSE;
    const float scaleX = static_cast<float>(backWidth) / 320.0f;
    const float scaleY = static_cast<float>(backHeight) / 240.0f;

    if (frameMs < 0) frameMs = 0;
    if (s_clockPending) {
        s_startTimeMs = plat_time_ms() - static_cast<DWORD>(frameMs);
        s_clockPending = FALSE;
    }

    // The backend has only cleared to black; everything, the movie included,
    // is drawn here.
    if (s_endingId >= 4) {
        DrawCastRoll(dx, videoTexture, frameMs, scaleX, scaleY);
        return TRUE;
    }

    // Staff roll. Mode 0 never decodes the stream's video (only mode 1 calls
    // the 0x240 decode), so the PS1 shows the strips on black and the STR only
    // supplies the music.
    s_frame = CurrentFrame();
    if (s_frame > kLogoEnd) return TRUE;

    // 0x800e3050 type 0: brightness is fadeLevel >> 8, 0x80 until the fade
    // starts, then 4 lower per frame (the add happens before the draw).
    int brightness = 0x80;
    if (s_frame >= kScrollFade) {
        brightness -= (s_frame - kScrollFade + 1) * kScrollFadeStep;
        if (brightness <= 0) return TRUE;
    }
    const DWORD color = Grey(brightness << 8);

    // A strip moves half a pixel per frame from its spawn frame and stops
    // when the freeze flips it to type 1.
    const int moveFrame = s_frame < kScrollFreeze ? s_frame : kScrollFreeze;
    const int spawnCount = s_stripCount < kMaxScrollStrips ? s_stripCount
                                                           : kMaxScrollStrips;
    for (int i = 0; i < spawnCount; ++i) {
        const int spawnFrame = kScrollStart + (i + 1) * kSpawnInterval;
        if (s_frame < spawnFrame) break;
        const int moved = moveFrame - spawnFrame;
        const int y = kScrollSpawnY - (moved + 1) / 2;   // (y16 - moved * 0x8000) >> 16
        if (y >= 240 || y <= -kStripHeight) continue;
        DrawStrip(dx, i, kCreditsX, y, scaleX, scaleY, color, MARNI_BLEND_ALPHA);
    }
    return TRUE;
}
