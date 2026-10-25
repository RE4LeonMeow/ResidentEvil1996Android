#pragma once
#include "../platform/types.h"
#include "../marni/MarniDX.h"

// JPN PS1 prologue FMV subtitles (Biohazard Director's Cut, SLPS_009.98).
//
// The original draws them in the video display module: StartSubtitleTrack
// (0x80037b74) picks a cue track and loads its 4bpp bitmap set, and
// ProcessSubtitleCues (0x80037c44) walks the 0x10-byte cue records once a
// frame, handing the active cue to BlitSubtitleLine4bppTo24bpp (0x80037d98),
// which greyscales the line straight into the 24bpp framebuffer over the
// decoded movie. The cue timing is therefore in the movie's own frames.
//
// tools/decode_jimaku.py (or the asset migrator) turns the disc's JIMAKU*.RGB
// into one greyscale jimakuNN.png per set (see docs/PS1_FMV_SUBTITLES.md) and emits the cue tables baked into
// Ps1FmvSubtitleData.h.

// TRUE when the config enables them and the assets are present.
BOOL Ps1FmvSubtitles_IsEnabled(void);

// Binds the subtitle track to an FMV. Only the prologue FMV carries one on
// the JPN disc; FALSE leaves the module inert.
BOOL Ps1FmvSubtitles_Begin(int fmvId);
void Ps1FmvSubtitles_End(void);
BOOL Ps1FmvSubtitles_IsActive(void);

// Draws the movie quad and, while a cue is live, its subtitle line, then
// returns TRUE so the backend does not draw the movie a second time.
// Returns FALSE when inactive, which leaves the backend's quad in place.
BOOL Ps1FmvSubtitles_RenderFrame(MarniHandle videoTexture, int frameMs);
