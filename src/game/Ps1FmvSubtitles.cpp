// JPN PS1 prologue FMV subtitles, ported from Biohazard Director's Cut
// (SLPS_009.98). See Ps1FmvSubtitles.h and docs/PS1_FMV_SUBTITLES.md.
#include "Ps1FmvSubtitles.h"
#include "Ps1FmvSubtitleData.h"
#include "../Globals.h"
#include "../marni/MarniDX.h"
#include "../marni/MarniSystem.h"
#include "../platform/platform.h"
#include "../system/AssetPath.h"
#include "../system/PngImage.h"

#include <stdlib.h>
#include <string.h>
#include <vector>

namespace {

// VideoPlayback.cpp's g_CurrentFMVID for the prologue movie.
const int kPrologueFmvId = 1;

const int kLineW = 320;
const int kLineH = 18;

// STR movies decode at 15 fps, which is the unit the cue tables count in.
const int kFps = 15;

struct Track {
    const char* file;
    const FmvSubtitleCue* cues;
    short cueCount;
    MarniHandle texture;
    int lineCount;
};

std::vector<Track> s_tracks;
int s_track = -1;
BOOL s_active = FALSE;

// Loads one jimakuNN.png set (320 wide, 18 rows per line, the level in the
// first channel) and turns it into a 32-bit grey texture. The original's blit
// writes the line's greyscale level over the decoded movie and skips
// fully-zero pixels, so level 0 is transparent and every other level is drawn
// opaque.
BOOL BuildTrackTexture(Track& t)
{
    char path[MAX_PATH];
    if (snprintf(path, sizeof(path), GAME_DATA_ROOT_JPN "data\\%s", t.file) < 0) {
        return FALSE;
    }
    // Same path handling as LoadFile, but sized by the file: a PNG saved by an
    // image editor has no fixed size to allocate for.
    char resolved[MAX_PATH];
    char normalized[MAX_PATH];
    const char* filePath = ResolveAssetRoot(path, resolved, sizeof(resolved));
    filePath = plat_normalize_path(filePath, normalized, sizeof(normalized));
    size_t size = 0;
    unsigned char* file = static_cast<unsigned char*>(plat_file_read_all(filePath, &size));
    if (file == NULL) return FALSE;

    std::vector<unsigned char> plane;
    int w = 0;
    int h = 0;
    const bool decoded = PngDecodeFirstChannel(file, size, &plane, &w, &h);
    free(file);
    if (!decoded || w != kLineW || h < kLineH || h % kLineH != 0) {
        dbg_printf("[subs] %s: not a %d-wide PNG of %d-row lines\n", t.file,
                   kLineW, kLineH);
        return FALSE;
    }
    t.lineCount = h / kLineH;

    std::vector<DWORD32> tex(plane.size());
    for (size_t i = 0; i < tex.size(); ++i) {
        // PS1 writes the level into all three colour bytes (an opaque grey
        // pixel) and leaves the movie untouched only where the level is zero,
        // so zero becomes the one fully transparent texel.
        const DWORD32 a = plane[i];
        tex[i] = a ? (0xFF000000u | (a << 16) | (a << 8) | a) : 0x00000000u;
    }
    t.texture = MARNI_NULL_HANDLE;
    return MarniCreateTexture(kLineW, h, 32, tex.data(), &t.texture);
}

const FmvSubtitleCue* ActiveCue(const Track& t, int frame)
{
    const FmvSubtitleCue* cue = NULL;
    for (short i = 0; i < t.cueCount; ++i) {
        if (frame < t.cues[i].startFrame) break;
        if (frame < t.cues[i].startFrame + t.cues[i].duration) cue = &t.cues[i];
    }
    return cue;
}

void ReleaseTracks(void)
{
    MarniDX* dx = Marni_DX();
    if (dx != NULL) {
        for (size_t i = 0; i < s_tracks.size(); ++i) {
            if (s_tracks[i].texture != MARNI_NULL_HANDLE) {
                dx->DestroyTexture(s_tracks[i].texture);
            }
        }
    }
    s_tracks.clear();
}

}  // namespace

BOOL Ps1FmvSubtitles_IsEnabled(void)
{
    return g_bPs1FmvSubtitles ? TRUE : FALSE;
}

BOOL Ps1FmvSubtitles_Begin(int fmvId)
{
    Ps1FmvSubtitles_End();
    if (!Ps1FmvSubtitles_IsEnabled()) return FALSE;

    // Only the prologue FMV carries a JPN subtitle track on this disc. The
    // original picks the track by screen, not by file; the prologue uses track
    // 0, which is JIMAKU00.RGB.
    if (fmvId != kPrologueFmvId) return FALSE;

    if (s_tracks.empty()) {
        Track tracks[3] = {
            { "jimaku00.png", kFmvSubtitleCues0, kFmvSubtitleCueCount0,
              MARNI_NULL_HANDLE, 0 },
            { "jimaku02.png", kFmvSubtitleCues1, kFmvSubtitleCueCount1,
              MARNI_NULL_HANDLE, 0 },
            { "jimaku01.png", kFmvSubtitleCues2, kFmvSubtitleCueCount2,
              MARNI_NULL_HANDLE, 0 },
        };
        for (int i = 0; i < 3; ++i) {
            if (!BuildTrackTexture(tracks[i])) {
                for (int j = 0; j < i; ++j) {
                    if (tracks[j].texture != MARNI_NULL_HANDLE) {
                        Marni_DX()->DestroyTexture(tracks[j].texture);
                    }
                }
                dbg_printf("[subs] cannot load data\\%s\n", tracks[i].file);
                return FALSE;
            }
            s_tracks.push_back(tracks[i]);
        }
    }

    s_track = 0;
    s_active = TRUE;
    return TRUE;
}

void Ps1FmvSubtitles_End(void)
{
    s_active = FALSE;
    s_track = -1;
}

BOOL Ps1FmvSubtitles_IsActive(void)
{
    return s_active;
}

BOOL Ps1FmvSubtitles_RenderFrame(MarniHandle videoTexture, int frameMs)
{
    if (!s_active || s_track < 0 || s_track >= static_cast<int>(s_tracks.size())) {
        return FALSE;
    }
    const Track& t = s_tracks[s_track];

    MarniDX* dx = Marni_DX();
    if (dx == NULL) return FALSE;

    DWORD backWidth = 0;
    DWORD backHeight = 0;
    dx->GetBackBufferSize(&backWidth, &backHeight);
    if (backWidth == 0 || backHeight == 0) return FALSE;

    // The backend clears to black and only draws the movie quad when the
    // overlay declines, so owning the frame means drawing the movie here too.
    if (videoTexture != MARNI_NULL_HANDLE) {
        dx->DrawSprite(0.0f, 0.0f, static_cast<float>(backWidth),
                       static_cast<float>(backHeight), 0.0f, 0.0f, 1.0f, 1.0f,
                       0xFFFFFFFFu, videoTexture, MARNI_SAMPLER_POINT,
                       MARNI_BLEND_DISABLE);
    }

    if (frameMs < 0) frameMs = 0;
    const int frame = (frameMs * kFps + 500) / 1000;
    const FmvSubtitleCue* cue = ActiveCue(t, frame);
    if (cue == NULL || t.texture == MARNI_NULL_HANDLE) return TRUE;

    const int rows = cue->lines > 0 ? cue->lines : 1;
    if (cue->line < 0 || cue->line + rows > t.lineCount) return TRUE;

    const float atlasH = static_cast<float>(kLineH * t.lineCount);
    const float v0 = static_cast<float>(cue->line * kLineH) / atlasH;
    const float v1 = static_cast<float>((cue->line + rows) * kLineH) / atlasH;
    const float scaleX = static_cast<float>(backWidth) / 320.0f;
    const float scaleY = static_cast<float>(backHeight) / 240.0f;
    dx->DrawSprite(cue->x * scaleX, cue->y * scaleY,
                   kLineW * scaleX, rows * kLineH * scaleY,
                   0.0f, v0, 1.0f, v1, 0xFFFFFFFFu, t.texture,
                   MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA);
    return TRUE;
}
