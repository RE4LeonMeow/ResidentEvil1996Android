#pragma once
#include "../platform/types.h"
#include "../marni/MarniDX.h"
#include <stddef.h>

BOOL Ps1EndingCredits_IsEnabled(void);
BOOL Ps1EndingCredits_Begin(int endingId, int characterId);
void Ps1EndingCredits_End(void);
BOOL Ps1EndingCredits_IsActive(void);
BOOL Ps1EndingCredits_ResolveVideoPath(int fmvId, char* out, size_t outSize);
void Ps1EndingCredits_SetVideoPath(const char* path);
void Ps1EndingCredits_StartVideo(void);
// Draws the whole movie frame while the credits own it (TRUE); FALSE leaves
// the backend's full-screen movie quad in place. frameMs is the presented
// frame's stream time: the overlay's clock, like the PS1's decoded frame number.
BOOL Ps1EndingCredits_RenderFrame(MarniHandle videoTexture, int frameMs);
BOOL Ps1EndingCredits_IsFinished(void);
