// VideoPlayback.cpp - FMV playback state machine (0x00474e00)
//
// Platform-neutral. The decoder/presenter is the plat_video_* backend
// (src/platform/win32/video.cpp = MCI, src/platform/linux/video.cpp
// = ffmpeg); this file owns the 4-state machine, the per-FMV skip masks, the
// skip grace period and the prologue scenario cut.

#include "../Globals.h"
#include "../game/Ps1EndingCredits.h"
#include "../game/Ps1FmvSubtitles.h"
#include "../platform/platform.h"
#include "../system/AssetPath.h"
#include "../marni/MarniSystem.h"
#include <stdio.h>

// ============================================================================
// FMV path table - maps FMV IDs to AVI filenames
// Original USA table at 0x004c39d8 in Ghidra.
// Each region keeps its own table in the original layout (full path string +
// isSkippable mask). The paths use the retail ".\usa\" root; ResolveAssetRoot
// (config.ini [Assets] Version) swaps that root for the configured tree, so both
// tables reuse the same base and only differ in the video filename.
// ============================================================================
struct FMVEntry {
    const char* filename;
    int         isSkippable;
};

// Original skip-mask table at 0x004c39dc in Ghidra (one WORD per FMV entry).
// 0x0fff = bits 0..11 of the PSX button word (Cross, Circle, Square, Triangle,
// L1, L2, R1, R2, Select, Start, L3, R3) - all standard accept/skip buttons.
// 0x0000 = FMV cannot be skipped.
static const FMVEntry g_FMVTableUSA[] = {
    { ".\\usa\\MOVIE\\OU.avi",      0x0fff },   // 0  - Opening / title movie
    { ".\\usa\\MOVIE\\PU.avi",      0x0fff },   // 1  - Intro
    { ".\\usa\\MOVIE\\DMF.avi",     0x0000 },   // 2
    { ".\\usa\\MOVIE\\DM3.avi",     0x0fff },   // 3
    { ".\\usa\\MOVIE\\DM4.avi",     0x0fff },   // 4
    { ".\\usa\\MOVIE\\DM1.avi",     0x0fff },   // 5
    { ".\\usa\\MOVIE\\DM6.avi",     0x0fff },   // 6
    { ".\\usa\\MOVIE\\DM7.avi",     0x0fff },   // 7
    { ".\\usa\\MOVIE\\DM8.avi",     0x0fff },   // 8
    { ".\\usa\\MOVIE\\DM2.avi",     0x0fff },   // 9
    { NULL,                         0x0fff },   // 10 - null entry
    { ".\\usa\\MOVIE\\DMB.avi",     0x0fff },   // 11
    { ".\\usa\\MOVIE\\DMC.avi",     0x0fff },   // 12
    { ".\\usa\\MOVIE\\DMD.avi",     0x0fff },   // 13
    { ".\\usa\\MOVIE\\DME.avi",     0x0000 },   // 14
    { ".\\usa\\MOVIE\\ED1.avi",     0x0000 },   // 15
    { ".\\usa\\MOVIE\\ED2.avi",     0x0000 },   // 16
    { ".\\usa\\MOVIE\\ED3.avi",     0x0000 },   // 17
    { ".\\usa\\MOVIE\\EU4.avi",     0x0000 },   // 18
    { ".\\usa\\MOVIE\\EU5.avi",     0x0000 },   // 19
    { ".\\usa\\MOVIE\\ED6.avi",     0x0000 },   // 20
    { ".\\usa\\MOVIE\\ED7.avi",     0x0000 },   // 21
    { ".\\usa\\MOVIE\\ED8.avi",     0x0000 },   // 22
    { ".\\usa\\MOVIE\\capcom.avi",  0x0fff },   // 23 - Capcom logo (skippable)
    { ".\\usa\\MOVIE\\stfc_r.avi",  0x0000 },   // 24
    { ".\\usa\\MOVIE\\stfj_r.avi",  0x0000 },   // 25
    { ".\\usa\\MOVIE\\stfz_r.avi",  0x0000 },   // 26
    { ".\\usa\\MOVIE\\staf_r.avi",  0x0000 },   // 27
    { ".\\usa\\MOVIE\\vlogo.avi",   0x0fff },   // 28 - Virgin logo (skippable)
};

// Japanese PC (Biohazard) FMV table. Same IDs/skip masks with the JPN filenames
// (see Biohazard.exe @0x004b532c+: OJ.avi/ED4.avi/_b variants; the endings also
// differ, so EU4/EU5 become ED4/ED5). Paths use the JPN data root macro so the
// table reads as the JPN tree; ResolveAssetRoot leaves them unchanged.
static const FMVEntry g_FMVTableJPN[] = {
    { GAME_DATA_ROOT_JPN "MOVIE\\OJ.avi",    0x0fff },   // 0  - Opening / title movie
    { GAME_DATA_ROOT_JPN "MOVIE\\PJ.avi",    0x0fff },   // 1
    { GAME_DATA_ROOT_JPN "MOVIE\\DMF.avi",   0x0000 },   // 2
    { GAME_DATA_ROOT_JPN "MOVIE\\DM3.avi",   0x0fff },   // 3
    { GAME_DATA_ROOT_JPN "MOVIE\\DM4.avi",   0x0fff },   // 4
    { GAME_DATA_ROOT_JPN "MOVIE\\DM1.avi",   0x0fff },   // 5
    { GAME_DATA_ROOT_JPN "MOVIE\\DM6.avi",   0x0fff },   // 6
    { GAME_DATA_ROOT_JPN "MOVIE\\DM7.avi",   0x0fff },   // 7
    { GAME_DATA_ROOT_JPN "MOVIE\\DM8.avi",   0x0fff },   // 8
    { GAME_DATA_ROOT_JPN "MOVIE\\DM2.avi",   0x0fff },   // 9
    { NULL,                                  0x0fff },   // 10 - null entry
    { GAME_DATA_ROOT_JPN "MOVIE\\DMB.avi",   0x0fff },   // 11
    { GAME_DATA_ROOT_JPN "MOVIE\\DMC.avi",   0x0fff },   // 12
    { GAME_DATA_ROOT_JPN "MOVIE\\DMD.avi",   0x0fff },   // 13
    { GAME_DATA_ROOT_JPN "MOVIE\\DME.avi",   0x0000 },   // 14
    { GAME_DATA_ROOT_JPN "MOVIE\\ED1.avi",   0x0000 },   // 15
    { GAME_DATA_ROOT_JPN "MOVIE\\ED2.avi",   0x0000 },   // 16
    { GAME_DATA_ROOT_JPN "MOVIE\\ED3.avi",   0x0000 },   // 17
    { GAME_DATA_ROOT_JPN "MOVIE\\ED4.avi",   0x0000 },   // 18
    { GAME_DATA_ROOT_JPN "MOVIE\\ED5.avi",   0x0000 },   // 19
    { GAME_DATA_ROOT_JPN "MOVIE\\ED6.avi",   0x0000 },   // 20
    { GAME_DATA_ROOT_JPN "MOVIE\\ED7.avi",   0x0000 },   // 21
    { GAME_DATA_ROOT_JPN "MOVIE\\ED8.avi",   0x0000 },   // 22
    { GAME_DATA_ROOT_JPN "MOVIE\\capcom.avi",0x0fff },   // 23 - Capcom logo (skippable)
    { GAME_DATA_ROOT_JPN "MOVIE\\stfc_b.avi",0x0000 },   // 24
    { GAME_DATA_ROOT_JPN "MOVIE\\stfj_b.avi",0x0000 },   // 25
    { GAME_DATA_ROOT_JPN "MOVIE\\stfz_b.avi",0x0000 },   // 26
    { GAME_DATA_ROOT_JPN "MOVIE\\staf_b.avi",0x0000 },   // 27
    { GAME_DATA_ROOT_JPN "MOVIE\\vlogo.avi", 0x0fff },   // 28 - Virgin logo (skippable)
};

static const int g_FMVTableCount = sizeof(g_FMVTableUSA) / sizeof(g_FMVTableUSA[0]);
static_assert(sizeof(g_FMVTableJPN) == sizeof(g_FMVTableUSA),
              "USA and JPN FMV tables must have the same layout/ID count");

// Bits 0..11 of the PSX button word - the full set of accept/skip buttons every
// skippable entry in the table above carries.
#define FMV_SKIP_ALL_BUTTONS 0x0fff

// Active table for the configured version (config.ini [Assets] Version).
static const FMVEntry* GetFmvTable(void)
{
    return (GetAssetVersion() == 1) ? g_FMVTableJPN : g_FMVTableUSA;
}

// ============================================================================
// GetFmvSkipMask - the button mask that skips an FMV. Port-added override:
// [Game] SkipUnskippableFmv=1 hands every movie the full button mask, so the
// ones the original left unskippable (mask 0x0000) can be skipped too. The
// per-entry masks are what ship.
// ============================================================================
static WORD GetFmvSkipMask(int fmvId)
{
    if (g_bSkipUnskippableFmv) {
        return (WORD)FMV_SKIP_ALL_BUTTONS;
    }
    if (fmvId < 0 || fmvId >= g_FMVTableCount) {
        return 0;
    }
    return (WORD)GetFmvTable()[fmvId].isSkippable;
}

// ============================================================================
// FMV 1 (PU.avi / PJ.avi) scenario cut; UpdateVideoPlayback @0x00474e00 uses
// millisecond cut points so the PC and DC streams select their native frames.
// ============================================================================
#define FMV_PROLOGUE_ID                 1
#define FMV_PROLOGUE_CUT_START_USAGE_MS 177800
#define FMV_PROLOGUE_CUT_END_USAGE_MS   188500
#define FMV_PROLOGUE_CUT_START_DC_FRAME 2430 // SLUS_001.70 PlayFMV @0x80037020: 0x97e
#define FMV_PROLOGUE_CUT_END_DC_FRAME   2591 // SLUS_001.70 PlayFMV @0x80037020: 0xa1f
#define FMV_PROLOGUE_DC_FPS             15

static int DcFrameToMs(int frame)
{
    return (frame * 1000 + FMV_PROLOGUE_DC_FPS / 2) / FMV_PROLOGUE_DC_FPS;
}

static int GetPrologueCutStartMs(void)
{
    return g_bDcMode ? DcFrameToMs(FMV_PROLOGUE_CUT_START_DC_FRAME)
                     : FMV_PROLOGUE_CUT_START_USAGE_MS;
}

static int GetPrologueCutEndMs(void)
{
    return g_bDcMode ? DcFrameToMs(FMV_PROLOGUE_CUT_END_DC_FRAME)
                     : FMV_PROLOGUE_CUT_END_USAGE_MS;
}

// ============================================================================
// Global video playback state (file-scope, persistent across UpdateVideoPlayback calls)
// ============================================================================
static int   g_FMVPlaybackState = 0;
static DWORD g_videoFlagA4 = 0;
static WORD  g_videoSkipInput = 0;
static int   g_videoSkipCounter = 0;
static char  g_videoFilePath[MAX_PATH] = {};
static BOOL  s_overlayCallbackRegistered = FALSE;

static BOOL Ps1VideoOverlayCallback(DWORD32 frameTexture, int frameMs)
{
    // Whoever claims the frame draws the movie too, so the chain is ordered:
    // the credits overlay owns the whole picture when it is running.
    if (Ps1EndingCredits_RenderFrame((MarniHandle)frameTexture, frameMs)) {
        return TRUE;
    }
    return Ps1FmvSubtitles_RenderFrame((MarniHandle)frameTexture, frameMs);
}

// ============================================================================
// ResolveVideoPath - Normalize the FMV path's data root to the selected version.
// The USA table is compiled with the retail ".\usa\" root; ResolveAssetRoot swaps
// that for the config-selected tree (config.ini [Assets] Version), mapping it to
// ".\assets\USA\" in debug builds. The JPN table is already compiled against the
// JPN root macro, so ResolveAssetRoot leaves those paths unchanged.
// ============================================================================
static const char* ResolveVideoPath(const char* originalPath, char* outPath, size_t outSize)
{
    if (originalPath == NULL || outPath == NULL || outSize == 0) return NULL;

    const char* resolved = ResolveAssetRoot(originalPath, outPath, outSize);
    return (resolved != NULL) ? resolved : originalPath;
}

// ============================================================================
// CheckVideoFileExists - Check if video file exists (0x00474d90)
// The path is normalised through the platform layer (separators + case) so the
// backend can open exactly what was probed here.
// ============================================================================
static BOOL FileReadable(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL) return FALSE;
    fclose(f);
    return TRUE;
}

// Resolve a path through the asset root and the platform's case resolution.
// `out` receives the usable path; returns FALSE when normalization fails.
static BOOL NormalizeVideoPath(const char* filename, char* out, size_t size)
{
    char resolvedPath[MAX_PATH];
    const char* filePath = ResolveVideoPath(filename, resolvedPath, sizeof(resolvedPath));
    if (filePath == NULL) filePath = filename;
    plat_normalize_path(filePath, out, size);
    return out[0] != '\0';
}

// Prefer a modern container over the legacy Cinepak AVI when one sits beside
// it. The FMV table still names the .avi, so the modern file is the same
// rooted path with the extension swapped; resolve it through the active
// asset overlay before falling back to the base tree.
static BOOL PreferModernSibling(const char* aviPath, char* out, size_t size)
{
    const char* dot = strrchr(aviPath, '.');
    if (dot == NULL || _stricmp(dot, ".avi") != 0) return FALSE;

    char candidate[MAX_PATH];
    const size_t prefix = (size_t)(dot - aviPath);
    if (prefix + 5 > MAX_PATH) return FALSE;
    memcpy(candidate, aviPath, prefix);
    strcpy_s(candidate + prefix, sizeof(candidate) - prefix, ".mp4");

    char resolved[MAX_PATH];
    const char* candidatePath = ResolveVideoPath(candidate, resolved, sizeof(resolved));
    if (candidatePath == NULL) candidatePath = candidate;

    char normalized[MAX_PATH];
    plat_normalize_path(candidatePath, normalized, sizeof(normalized));
    if (!FileReadable(normalized)) return FALSE;

    strcpy_s(out, size, normalized);
    return TRUE;
}

BOOL CheckVideoFileExists(const char* filename)
{
    if (filename == NULL) return FALSE;

    char creditsPath[MAX_PATH];
    if (Ps1EndingCredits_ResolveVideoPath(g_CurrentFMVID, creditsPath,
                                          sizeof(creditsPath)) &&
        FileReadable(creditsPath)) {
        strcpy_s(g_videoFilePath, sizeof(g_videoFilePath), creditsPath);
        Ps1EndingCredits_SetVideoPath(g_videoFilePath);
        return TRUE;
    }

    char modern[MAX_PATH];
    if (PreferModernSibling(filename, modern, sizeof(modern))) {
        strcpy_s(g_videoFilePath, sizeof(g_videoFilePath), modern);
        Ps1EndingCredits_SetVideoPath(g_videoFilePath);
        return TRUE;
    }

    char normalized[MAX_PATH];
    if (!NormalizeVideoPath(filename, normalized, sizeof(normalized))) {
        return FALSE;
    }

    if (!FileReadable(normalized)) {
        dbg_printf("[VIDEO] Could not open file: %s\n", normalized);
        return FALSE;
    }

    strcpy_s(g_videoFilePath, sizeof(g_videoFilePath), normalized);
    Ps1EndingCredits_SetVideoPath(g_videoFilePath);
    return TRUE;
}

// ============================================================================
// UsesScenarioCut - TRUE while the prologue FMV must drop the Chris-only beat.
// g_FmvCharacterId is latched from g_SelectedCharactedId by main_loop when the
// FMV request is consumed; 0 = Chris (play the movie whole), non-zero = Jill.
// ============================================================================
static BOOL UsesScenarioCut(void)
{
    return (g_CurrentFMVID == FMV_PROLOGUE_ID) && (g_FmvCharacterId != 0);
}

// ============================================================================
// UpdateVideoPlayback - FMV playback state machine (0x00474e00)
// ============================================================================
void UpdateVideoPlayback(void)
{
    if (!s_overlayCallbackRegistered) {
        plat_video_set_overlay_callback(Ps1VideoOverlayCallback);
        s_overlayCallbackRegistered = TRUE;
    }
    if (g_bIsSoftwareRendering) {
        g_bMCINotifyEnabled = FALSE;
        return;
    }

    switch (g_FMVPlaybackState) {
    case 0: // Initialize
        {
            ClearScreen();
            ClearScreen();

            // Present current frame before video overlay
            if (g_pMarniDirect3D != NULL) {
                MarniPresent();
            }

            g_videoFlagA4 = 1;
            g_demoIdleTimer1 = 1;

            // Get the filename for this FMV ID
            const char* videoFile = NULL;
            if (g_CurrentFMVID >= 0 && g_CurrentFMVID < g_FMVTableCount) {
                videoFile = GetFmvTable()[g_CurrentFMVID].filename;
            }

            if (videoFile != NULL && CheckVideoFileExists(videoFile)) {
                PauseGameSoundsAsync();

                plat_video_set_window_mode(TRUE);

                if (!plat_video_init()) {
                    dbg_safe_str("[VIDEO] Failed to init video system, skipping FMV\n");
                    Ps1EndingCredits_End();
                    Ps1FmvSubtitles_End();
                    g_bMCINotifyEnabled = FALSE;
                    plat_video_close();
                    setMenuScreenOffset(320, 240, 0, 0, 0);
                    CenterScreenOrigin();
                    return;
                }

                // JPN prologue subtitle track, if the config asked for it and
                // the JPN assets are present.
                Ps1FmvSubtitles_Begin(g_CurrentFMVID);

                g_FMVPlaybackState = 1;
            } else {
                // No video file - skip FMV
                Ps1EndingCredits_End();
                Ps1FmvSubtitles_End();
                g_bMCINotifyEnabled = FALSE;
                plat_video_close();
                setMenuScreenOffset(320, 240, 0, 0, 0);
                CenterScreenOrigin();
            }
        }
        break;

    case 1: // Open and start playing
        {
            // Jill: stop the first chunk right before the Chris-only dialogue.
            int playTo = UsesScenarioCut() ? GetPrologueCutStartMs() : 0;

            Ps1EndingCredits_StartVideo();
            if (!plat_video_open_and_play(g_videoFilePath, playTo)) {
                g_FMVPlaybackState = 3;
                break;
            }

            g_FMVPlaybackState = 2;
            plat_video_tick();

            InputUpdate();
            g_videoSkipInput = (WORD)PlayerPad_Update();
            g_videoSkipCounter = 100;
        }
        break;

    case 2: // Playing - monitor for skip/completion
        {
            plat_video_tick();

            if (g_videoSkipCounter > 0) {
                g_videoSkipCounter--;
            }

            InputUpdate();
            WORD currentInput = (WORD)PlayerPad_Update();

            // Check for skip input (per-FMV skip mask from g_FMVTable[i].isSkippable)
            WORD skipMask = GetFmvSkipMask(g_CurrentFMVID);
            if (((skipMask & ~g_videoSkipInput & currentInput) != 0) && (g_videoSkipCounter == 0)) {
                // Skip requested - stop playback
                plat_video_stop();
            }

            g_videoSkipInput = currentInput;

            // The staff roll leaves its loop on its own counter (0x1c20),
            // a moment before the STR runs out.
            if (Ps1EndingCredits_IsFinished()) {
                plat_video_stop();
            }

            // Segment/film-end notification from the backend
            if (plat_video_take_end_event()) {
                if (UsesScenarioCut() && g_videoFlagA4 != 0) {
                    // The first chunk ended at the cut point: jump past the
                    // Chris-only beat and play the remainder. g_videoFlagA4 is
                    // cleared below, so the next notification ends the FMV.
                    plat_video_play_from(GetPrologueCutEndMs());
                } else {
                    plat_video_stop();
                }
                g_videoFlagA4 = 0;
            }

            // Check if video has ended
            if (!plat_video_is_active()) {
                g_FMVPlaybackState = 3;

                plat_video_set_window_mode(FALSE);

                setMenuScreenOffset(320, 240, 0, 0, 0);
                CenterScreenOrigin();
                plat_video_close();
            }
        }
        break;

    case 3: // Cleanup
        {
            plat_video_close();
            Ps1EndingCredits_End();
            Ps1FmvSubtitles_End();
            ResumeGameSoundsAsync();

            g_FMVPlaybackState = 0;
            g_bMCINotifyEnabled = FALSE;
            g_demoIdleTimer1 = 0;
            StMask(3, 0);
        }
        break;
    }
}

// ============================================================================
// QueueVideoPlayback (0x004422d0)
// Software-renderer FMV queue. g_pSharedMemory[4] is the queue fill count;
// the FMV id goes to +0x0C+count and its sub-flag to +0x14+count (both slots
// pre-initialised to 8 entries by InitSoftwareRenderer). No-op when the
// software-renderer shared memory was never mapped (D3D path plays FMVs via
// the main_loop 0x40000 state flag instead).
// ============================================================================
void QueueVideoPlayback(int fmvId, int flag)
{
    if (g_pSharedMemory != NULL) {
        int count = g_pSharedMemory[4];
        g_pSharedMemory[0x0C + count] = (BYTE)fmvId;
        g_pSharedMemory[0x14 + count] = (BYTE)flag;
        g_pSharedMemory[4] = (BYTE)(count + 1);
    }
}

// ============================================================================
// StartVideoPlayback - Signal software video player to start (0x004423b0)
// ============================================================================
void StartVideoPlayback(void)
{
    if (g_pSharedMemory != NULL) {
        g_pSharedMemory[6] = 1;
    }
}

// ============================================================================
// IsVideoPlaybackComplete - Check if software video player finished (0x00442310)
// ============================================================================
bool IsVideoPlaybackComplete(void)
{
    if (g_pSharedMemory != NULL) {
        return (g_pSharedMemory[3] == 1);
    }
    return false;
}

// ============================================================================
// SignalVideoSkip - Signal software video player to skip (0x00442330)
// ============================================================================
void SignalVideoSkip(void)
{
    if (g_pSharedMemory != NULL) {
        g_pSharedMemory[2] = 1;
    }
}

// ============================================================================
// SetVideoResolution - Set video display resolution (0x00497f30)
// Original: writes ONLY the logical render resolution (CMarniDirect3D
// field_0x8/0xc) and rescales the 0x34/0x38 float factors (x2.0 for 320x240,
// x0.5 for 640x480). It never touches the physical surface dims
// (field_0x10/0x14). The Marni draw layer scales game-space primitives by
// physical/logical at render time, which is how a 640x480 surface gets
// filled while the game runs at logical 320x240.
// (The original callee also read only its first argument.)
// ============================================================================
void SetVideoResolution(int width, int height)
{
    (void)height; // unused, same as the original
    if (g_pMarniDirect3D != NULL) {
        CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
        if (width == 320) {
            pD3D->m_logicalWidth  = 320;
            pD3D->m_logicalHeight = 240;
        } else {
            pD3D->m_logicalWidth  = 640;
            pD3D->m_logicalHeight = 480;
        }
    }
}
