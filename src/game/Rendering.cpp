// Rendering.cpp - Frame rendering, present, sprite drawing
// All functions decompiled from Ghidra with original addresses
#include "../Globals.h"
#include "../platform/platform.h"
#include "../DebugPrint.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "../system/AssetPath.h"
#include "dc/Items.h"    // lockpick item id (is_lockpick_item)
#include "SpriteRenderer.h"
#include "TmdRenderer.h"
#include <cstdlib>
#include <cstdio>
#include <time.h>

extern unsigned int set_message_display(unsigned short msg_id, unsigned short pause_game);
extern void Flg_on(int baseAddr, unsigned int bitIndex);
extern void room_event_item_pickup(void);        // 0x00451700
extern void display_room_camera_bg(void);
extern void rearrange_item_slots(void);

// ============================================================================
// Pending sprite queue (filled by AddTintSprite / draw_rect / OT_InsertPrimitive,
// rendered by FrameRateGovernor)
// ============================================================================
// 512: the F1 debug menu's flag editor pages a 32-byte bank as 16 rows of
// per-glyph text sprites (~390 sprites with hints) - at the original 300 the queue
// overflowed, silently dropping the bottom rows AND the background quad that
// OT_InsertPrimitive adds at present time, which blacked the whole screen.
#define MAX_PENDING_SPRITES 512

// Pending sprites at or above this depth are scene elements drawn BEFORE the
// 3D TMD pass (room backgrounds, window fills); below it they are overlays that
// FrameRateGovernor orders against the command-buffer sprites. The boundary
// must sit above the pause menu's black masking rects (blend 0x1e -> 980) and
// below the menu window fills and room background (2100 / 0xFFF).
#define PENDING_SCENE_DEPTH 0x400u

struct PendingSprite {
    float x, y, w, h;
    float u0, v0, u1, v1;
    DWORD color;
    MarniHandle tex;
    BOOL valid;
    unsigned int depth;   // OT depth sort value (lower = closer = on top)
};
static PendingSprite g_pendingSprites[MAX_PENDING_SPRITES];
static int g_pendingSpriteCount = 0;

MarniHandle g_displayImageSRV = MARNI_NULL_HANDLE;

// ============================================================================
// AddTintSprite (0x0046e0a0)
// Adds a tinted font character sprite to the pending sprite queue.
// Brightness controls color intensity. Pending sprites are rendered in
// FrameRateGovernor before FlushSpriteCommands.
// ============================================================================
int AddTintSprite(TextureDesc* texture, unsigned short brightness)
{
    if (g_pendingSpriteCount >= MAX_PENDING_SPRITES) return 0;

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL) return 0;

    // Determine which font texture to use based on the tpage code.
    // texturePage == 0x15 (default) → fontus.tim (m_FontTexHandle, bank 0x1E)
    // texturePage == 1 (font03t)    → font03t.tim (texture page slot 2)
    // texturePage == 0              → fontus.tim (standard text rendering)
    MarniHandle texHandle = pD3D->m_FontTexHandle;
    int texW = pD3D->m_FontTexWidth;
    int texH = pD3D->m_FontTexHeight;

    if (texture->texturePage == 1) {
        // Font03t.tim is loaded at texture page slot 2 (slot+0xF = 17)
        int srvIdx = 2 + 0xF;
        if (srvIdx >= 0 && srvIdx < 256 && g_TexturePageSRV[srvIdx] != MARNI_NULL_HANDLE) {
            texHandle = g_TexturePageSRV[srvIdx];
            texW = g_TexturePageWidth[srvIdx];
            texH = g_TexturePageHeight[srvIdx];
        }
    }

    if (texHandle == MARNI_NULL_HANDLE) return 0;
    if (texW <= 0 || texH <= 0) return 0;

    float gameX = (float)(texture->screenX + g_ScreenOffsetX);
    float gameY = (float)(texture->screenY + g_ScreenOffsetY);

    // Scale game-space to the backbuffer: physical/logical, exactly how the
    // original Marni layer scaled primitives at draw time (FUN_0042ba60).
    float scaleX, scaleY;
    MarniGetRenderScale(&scaleX, &scaleY);

    float screenX = gameX * scaleX;
    float screenY = gameY * scaleY;
    float charW = (float)texture->width * scaleX;
    float charH = (float)texture->height * scaleY;

    // Font page select. The original's AddTintSprite looks `texturePage` up in
    // the texture-page table (JPN 0x00441120 / USA 0x0046e0a0 search slots
    // 12-14 for the id at TextureDesc+0x0C, returning 0 if none matches), so
    // 0x1E and 0x1F name two DIFFERENT pages of the same font sheet.
    // fontus.tim is 256x256 and only ever fills page 0x1E; the
    // Japanese FONT.TIM is 768x256, so its kanji half is page 0x1F, one
    // 256-texel page to the right. texU is a byte in the descriptor and cannot
    // carry that, exactly as in the original — the page adds it here.
    int texUBase = texture->texU;
    if (texture->texturePage == 0x1F && texW >= 512) {
        texUBase += 256;
    }

    float texW_f = (float)texW;
    float texH_f = (float)texH;
    float u0 = (float)texUBase / texW_f;
    float v0 = (float)texture->texV / texH_f;
    float u1 = (float)(texUBase + texture->width) / texW_f;
    float v1 = (float)(texture->texV + texture->height) / texH_f;

    // Calculate RGB from tint values — cast to unsigned int first to avoid overflow
    unsigned int r = ((unsigned int)(texture->colorMulR & 0xFF)) * 2; if (r > 255) r = 255;
    unsigned int g = ((unsigned int)(texture->colorMulG & 0xFF)) * 2; if (g > 255) g = 255;
    unsigned int b = ((unsigned int)(texture->colorMulB & 0xFF)) * 2; if (b > 255) b = 255;

    // CLUT-tint table (original AddTintSprite 0x0046e0a0): clutY minus
    // the font CLUT base (0x1E0, set by ProcessTextureImage bank 0x1E) selects
    // an RGB tint multiplier — 0 white, 1 green (message item names), 2 red,
    // 3 gray, anything else yellow. CLUT 8 (the PrintText shadow row 0x1E8)
    // maps to 1 like the original. The message renderer sets 0x1E1 around an
    // item name, which is what turns "INK RIBBON" green.
    int clutTint = (int)texture->clutY - 0x1E0;
    if (clutTint == 8) clutTint = 1;
    switch (clutTint) {
    case 0:  break;                       // white (default)
    case 1:  r = 0; g = 255; b = 0; break;       // green
    case 2:  r = 255; g = 0; b = 0; break;       // red
    case 3:  r = 204; g = 204; b = 204; break;   // gray (0.8)
    default: r = 255; g = 255; b = 0; break;     // yellow
    }

    DWORD color;
    if (texture->colorMulR == 0 && texture->colorMulG == 0 && texture->colorMulB == 0) {
        // Shadow pass: semi-transparent black (brightness controls alpha)
        unsigned int a = ((unsigned int)brightness * 255) / 30;
        if (a > 255) a = 255;
        color = (a << 24) | (0 << 16) | (0 << 8) | 0;
    } else {
        // Text pass: opaque color, brightness dims RGB (original PS1 CLUT-based dimming)
        unsigned int brightnessScale = ((unsigned int)brightness * 255) / 30;
        if (brightnessScale > 255) brightnessScale = 255;
        // The original AddTintSprite (0x0046e0a0) collapses fade 2 -> 0 before
        // the draw, so 0 and 2 are the SAME full-brightness render. The
        // room-4110 special case (stage 3 room 0x11 cams 4/0: message glyphs,
        // yes/no cursor, PrintText8x14) passes 0; without this it scaled the
        // RGB to black and every message glyph in that room rendered black.
        if (brightness == 0 || brightness == 2) brightnessScale = 255;
        r = (r * brightnessScale) / 255;
        g = (g * brightnessScale) / 255;
        b = (b * brightnessScale) / 255;
        color = (255u << 24) | (r << 16) | (g << 8) | b;
    }

    PendingSprite* spr = &g_pendingSprites[g_pendingSpriteCount];
    spr->x = screenX;
    spr->y = screenY;
    spr->w = charW;
    spr->h = charH;
    spr->u0 = u0;
    spr->v0 = v0;
    spr->u1 = u1;
    spr->v1 = v1;
    spr->color = color;
    spr->tex = texHandle;
    spr->valid = TRUE;
    spr->depth = (unsigned int)brightness * 16 + 0x1C2;  // OT depth: higher=further behind

    g_pendingSpriteCount++;
    return 1;
}

// ============================================================================
// GetTextureVariant (0x0046d950)
//
// Identical to the shared GetTextureVariant (0x0046d940, SpriteRenderer.cpp);
// this file used to carry a `static` duplicate, which GCC rejects as a static
// redeclaration of an extern function. Use the shared one.
// ============================================================================

// ============================================================================
// draw_rect (0x00470350)
// Fills a rectangle with the given color. Uses pending sprite queue.
// The blend parameter controls depth/ordering.
// The textureId field selects blend variant via GetTextureVariant:
//   0: Opaque fill (g_window_rect, etc.)
//   1: Semi-transparent tinted overlay (special room lighting)
//   2: Semi-transparent white flash (fade_type_id=1)
//   3: Semi-transparent black fade (fade_type_id=2)
// ============================================================================
void draw_rect(RectDrawDesc* rect, int blend, int flags)
{
    if (rect == NULL) return;
    if (g_pendingSpriteCount >= MAX_PENDING_SPRITES) return;

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL) return;

    float gameX = (float)(rect->x + g_ScreenOffsetX);
    float gameY = (float)(rect->y + g_ScreenOffsetY);
    float gameW = (float)rect->w;
    float gameH = (float)rect->h;

    // Scale game-space (320x240) to the real backbuffer (see AddTintSprite).
    float scaleX, scaleY;
    MarniGetRenderScale(&scaleX, &scaleY);

    float screenX = gameX * scaleX;
    float screenY = gameY * scaleY;
    float screenW = gameW * scaleX;
    float screenH = gameH * scaleY;

    unsigned char r = (unsigned char)(rect->r & 0xFF);
    unsigned char g = (unsigned char)(rect->g & 0xFF);
    unsigned char b = (unsigned char)(rect->b & 0xFF);

    // 0x0047039b-0x004703fa: the primitive's colour is a PER-CHANNEL ON/OFF
    // MASK, not the rect's literal colour. Each component is stored as 1.0f
    // when its byte is non-zero and 0x3b449ba6 (~0.003, i.e. black) when it is
    // zero, and `brightness` - the level that becomes the blend weight - is the
    // LAST non-zero component. A tint is therefore always drawn fully
    // saturated, with only its alpha varying.
    unsigned char brightness = 0;
    unsigned char mr = 0, mg = 0, mb = 0;
    if (r != 0) { mr = 0xFF; brightness = r; }
    if (g != 0) { mg = 0xFF; brightness = g; }
    if (b != 0) { mb = 0xFF; brightness = b; }

    int variant = GetTextureVariant(rect->textureId);

    // 0x004703fd: an all-zero colour draws nothing - unless the descriptor
    // carries no variant at all, since variant 0 is the opaque fill and may
    // legitimately be black.
    if (brightness == 0 && variant != 0) return;

    // The original stores (0x100 - brightness) / 256 into the primitive's +0x2c,
    // which the sprite draw uses as the DESTINATION weight; `brightness` is
    // therefore the source alpha, which is what this pending-sprite path wants.
    unsigned char a;

    switch (variant) {
    case 1:
    case 2:
        // 0x00470526 and 0x00470553 are BYTE-IDENTICAL blocks (only the jmp
        // displacement differs): both set the variant flag and keep the mask
        // colour built above. Case 2 used to force r=g=b=255 here. That is
        // invisible for a greyscale fade - every component is already equal -
        // but it destroys a COLOURED tint. Room 2050's poison gas arms
        // `1C 01 C8 00 06 00`: g_SpecialRoomLightR = 1 -> variant 2, flags 6 =
        // R|G, i.e. a YELLOW veil, and it rendered white.
        r = mr; g = mg; b = mb;
        a = brightness;
        break;
    case 3:
        // 0x00470580: forces the colour to black, keeps the level.
        r = 0; g = 0; b = 0;
        a = brightness;
        break;
    case 4:
        // 0x004705c8: the same tinted overlay at HALF the level - `shr bl,1`
        // before the (0x100 - bl) / 256 store. This is the stage-4 room-0x11
        // emergency light (g_SpecialRoomLightR = 3 -> textureId 0x70000000).
        r = mr; g = mg; b = mb;
        a = (unsigned char)(brightness >> 1);
        break;
    default:
        // 0x00470472: variant 0 is the opaque fill (g_window_rect etc.) and is
        // the one case that keeps the rect's literal colour.
        a = 255;
        break;
    }

    if (a == 0) return;

    DWORD color = ((unsigned int)a << 24) | ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;

    MarniHandle srv = MARNI_NULL_HANDLE; // null = white fallback

    PendingSprite* spr = &g_pendingSprites[g_pendingSpriteCount];
    spr->x = screenX;
    spr->y = screenY;
    spr->w = screenW;
    spr->h = screenH;
    spr->u0 = 0.0f;
    spr->v0 = 0.0f;
    spr->u1 = 1.0f;
    spr->v1 = 1.0f;
    spr->color = color;
    spr->tex = srv;
    spr->valid = TRUE;
    // Depth: higher value = further back (drawn first).
    // In original OT: flags==0 → blend+450, else → blend*16+500
    spr->depth = (flags == 0) ? ((unsigned int)blend + 450) : ((unsigned int)blend * 16 + 500);

    g_pendingSpriteCount++;
}

// ============================================================================
// QueueTexturedSprite — Queue a textured sprite for rendering
// Uses a specific SRV instead of the white fallback. Coordinates are in
// game-space (320x240) and get scaled to actual screen resolution.
// ============================================================================
void QueueTexturedSprite(float gameX, float gameY, float gameW, float gameH,
                         MarniHandle tex, unsigned int depth)
{
    if (tex == MARNI_NULL_HANDLE) return;
    if (g_pendingSpriteCount >= MAX_PENDING_SPRITES) return;

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL) return;

    // Scale game-space (320x240) to the real backbuffer (see AddTintSprite).
    float scaleX, scaleY;
    MarniGetRenderScale(&scaleX, &scaleY);

    PendingSprite* spr = &g_pendingSprites[g_pendingSpriteCount];
    spr->x = (gameX + (float)g_ScreenOffsetX) * scaleX;
    spr->y = (gameY + (float)g_ScreenOffsetY) * scaleY;
    spr->w = gameW * scaleX;
    spr->h = gameH * scaleY;
    spr->u0 = 0.0f;
    spr->v0 = 0.0f;
    spr->u1 = 1.0f;
    spr->v1 = 1.0f;
    spr->color = 0xFFFFFFFF;
    spr->tex = tex;
    spr->valid = TRUE;
    spr->depth = depth;

    g_pendingSpriteCount++;
}

// ============================================================================
// FrameRateGovernor (0x004973d0)
// ============================================================================
void FrameRateGovernor(void)
{
    g_numFramesRendered++;

    DWORD currentTime = plat_time_ms();

    // 0x004973e2: RETAIL PATCHES THE FRAME-TIME MEASUREMENT OUT. Do not restore it.
    //
    //   004973E2  cmp dword ptr [0x4d45fc], 0    <- flags computed...
    //   004973E9  mov esi, eax
    //   004973EB  e9 4d 01 00 00  jmp 0x49753d   <- ...and never read: unconditional
    //   004973F0  90              nop            <- leftover byte of the 6-byte Jcc
    //
    // A Jcc rel32 is six bytes and jmp rel32 is five, and the spare byte is still
    // sitting there as a nop. Nothing in the whole .text branches into
    // 0x004973f0-0x0049753c, so the block below - record the delta into the
    // four-entry ring at 0x00ac4000, sum it, scale it, clamp it - is unreachable in
    // the shipped game. Its inputs and outputs corroborate that: g_LastFrameTime_ms
    // (0x004d45fc) and the ring are written but only read from inside the orphan,
    // and DAT_004d45e8 / DAT_004d45f0 are write-only across the entire image.
    //
    // So in the retail build g_frameTargetTime is permanently its initial 100 and
    // g_bFrameSkipDetected permanently 0. That is the whole point: the accumulator
    // adds exactly 100 per tick, so every tick presents, and the pump's 33 ms
    // limiter is armed forever - a dead-steady 30 ticks/s with no measurement
    // feedback to destabilise it.
    //
    // Transcribing the orphan as live code is what produced the room 10F symptom
    // after the limiter was added: main_loop's own duration jitter pushed a measured
    // delta from 33 to 34, which is sum 134 -> target 101 - inside the 101..103 band
    // the 104/132 scaling does not cover (it is guarded by > 103) - which set
    // g_bFrameSkipDetected, which switched the limiter off for the two frames until
    // the next recompute. Measured 30.4 ms per tick instead of 33.
    //
    // The dead transcription lived here in full; it is not kept, because leaving it
    // reachable is precisely the bug. The frame-skip machinery it fed
    // (g_frameTimeBuffer, g_frameTimeIndex, g_LastFrameTime_ms) stays declared, and
    // the writes the reachable half of this function makes to g_LastFrameTime_ms
    // below are transcribed as-is - write-only, exactly like the original.

    g_frameTimeAccumulator += 100;

    if (g_frameTimeAccumulator < g_frameTargetTime) {
        // Dropped frame: discard everything queued for it, the 3D queue
        // included, otherwise objects pile up until the next presented frame.
        g_LastFrameTime_ms = 0;
        ResetSpriteQueue();
    } else {
        if (g_ScreenAccessReady && g_RenderAccessReady) {
            MarniClear();

            FUN_0040a8f0(NULL);

            // Sort pending sprites by depth (descending: high depth first = behind, low depth last = on top)
            for (int i = 0; i < g_pendingSpriteCount - 1; i++) {
                for (int j = i + 1; j < g_pendingSpriteCount; j++) {
                    if (g_pendingSprites[i].depth < g_pendingSprites[j].depth) {
                        PendingSprite tmp = g_pendingSprites[i];
                        g_pendingSprites[i] = g_pendingSprites[j];
                        g_pendingSprites[j] = tmp;
                    }
                }
            }

            // Command-buffer sprites with depthSort >= 0x10000 are background
            // elements (the death screen's base died.tim image at
            // 0xFFA*16+500 = 0x10194) that must draw BEHIND both the pending
            // overlays and the 3D TMD pass - the original sorted everything in
            // a single OT, where 0x10194 was the farthest primitive.
            // Effect sprites no longer pass through here at all - they are
            // SPRITE_CLASS_EFFECT and FlushTmdObjects interleaves them with the
            // entity triangles, so this range only ever sees NORMAL commands.
            FlushSpriteCommandsRange(0x10000, 0xFFFFFFFFu);

            // Render high-depth pending sprites (background, room lighting).
            // At/above this threshold are scene elements that must sit behind
            // the 3D pass: the room BG (0xFFF), the death screen's black rect
            // (0xFC00), the menu's window fills (2100). Below it are overlays,
            // which are ordered against the command sprites further down.
            //
            // 0x800 was used here once: it moved the menu's black masking
            // rects (980) into the on-top bucket, where they covered the
            // frame, item icons and character portrait - only the text (also
            // an overlay, drawn after them) stayed visible. That is what the
            // interleaved pass below fixes properly: at 980 the masks cover
            // the sliding file book and map pages (1060-1140) while the frame
            // parts (820-964) and everything nearer still draw on top.
            for (int i = 0; i < g_pendingSpriteCount; i++) {
                if (g_pendingSprites[i].valid && g_pendingSprites[i].depth >= PENDING_SCENE_DEPTH) {
                    MarniDrawSprite(
                        g_pendingSprites[i].x, g_pendingSprites[i].y,
                        g_pendingSprites[i].w, g_pendingSprites[i].h,
                        g_pendingSprites[i].u0, g_pendingSprites[i].v0,
                        g_pendingSprites[i].u1, g_pendingSprites[i].v1,
                        g_pendingSprites[i].color,
                        g_pendingSprites[i].tex);
                }
            }

            // Collision boundary overlay ([Debug] ShowCollision, F8) - drawn
            // only while debug features are enabled (the flag is set from
            // config.ini / the F8 toggle under the same gate, and the draw
            // itself early-returns on it). Between the background and the 3D
            // so characters occlude the outlines and it reads as geometry
            // lying on the floor.
            CollisionDebug_Draw();

            // Render queued 3D TMD objects (entities, options-menu character)
            FlushTmdObjects();

            // Render command buffer sprites (game objects, title text, etc.).
            // The >= 0x10000 half already drew before the TMD pass.
            //
            // An overlay whose depth falls INSIDE the command-sprite range has
            // to be interleaved, not deferred to the pass below. Two cases:
            //   * the file reader's fullscreen black (548) must cover the
            //     pause-menu frame (command sprites at 1060-1140) while the
            //     document page, its page arrows and the EXIT label (500-516)
            //     draw on top of it;
            //   * the pause menu's four black masking rects (980) must cover
            //     the sliding file book / map pages (1060-1140) - otherwise
            //     those show through the gaps between the frame panels - while
            //     the frame parts (820-964), item icons, portrait and text all
            //     stay above them.
            // The pending list is already sorted far-to-near, so walking it and
            // flushing the sprites behind each overlay reproduces the
            // original's single ordering table.
            //
            // 500 is the floor because a command sprite's depthSort is
            // depth*16 + 500: nothing from display_texture/draw_texture can
            // land below it, so overlays under 500 (every screen fade, at
            // 450-499) keep drawing after ALL of them exactly as before.
            // Effect sprites used to be the one class that could sort lower;
            // they are SPRITE_CLASS_EFFECT now and drew during the TMD pass, so
            // this range sees nothing below 500 at all.
            unsigned int spriteCursor = 0x10000;
            for (int i = 0; i < g_pendingSpriteCount; i++) {
                if (!g_pendingSprites[i].valid) continue;
                unsigned int d = g_pendingSprites[i].depth;
                if (d >= PENDING_SCENE_DEPTH || d < 500) continue;
                FlushSpriteCommandsRange(d, spriteCursor);
                spriteCursor = d;
                MarniDrawSprite(
                    g_pendingSprites[i].x, g_pendingSprites[i].y,
                    g_pendingSprites[i].w, g_pendingSprites[i].h,
                    g_pendingSprites[i].u0, g_pendingSprites[i].v0,
                    g_pendingSprites[i].u1, g_pendingSprites[i].v1,
                    g_pendingSprites[i].color,
                    g_pendingSprites[i].tex);
            }
            FlushSpriteCommandsRange(0, spriteCursor);
            // Both range flushes above drew; clear the queue now (the original
            // reset it inside the single flush). Without this the command
            // buffer accumulates across frames and display_texture returns 0
            // on overflow - every 2D effect stops rendering.
            g_SpriteQueueCount = 0;

            // Render the remaining low-depth pending sprites last (the screen
            // fade overlays and colour tinting under depth 500; the
            // 500-PENDING_SCENE_DEPTH band already drew, interleaved, above).
            for (int i = 0; i < g_pendingSpriteCount; i++) {
                if (g_pendingSprites[i].valid && g_pendingSprites[i].depth < 500) {
                    MarniDrawSprite(
                        g_pendingSprites[i].x, g_pendingSprites[i].y,
                        g_pendingSprites[i].w, g_pendingSprites[i].h,
                        g_pendingSprites[i].u0, g_pendingSprites[i].v0,
                        g_pendingSprites[i].u1, g_pendingSprites[i].v1,
                        g_pendingSprites[i].color,
                        g_pendingSprites[i].tex);
                }
            }

            if (!g_DisablePad) {
                MarniPresent();
            }
            g_numFramesPresented++;
        }

        // Outside the ready check on purpose. Everything queued above belongs
        // to THIS frame, presented or not - StMask(0,N) holds
        // g_ScreenAccessReady low for 2-3 frames after every cut_set, and
        // leaving the queues alone on those frames meant the commands built
        // while the screen was masked survived into the next presented frame.
        // That is why the previous camera's room masks kept drawing over the
        // new background after a camera change, and why several frames' worth
        // of masks - each baked with a different screen-shake offset - could
        // end up on screen at once during the shake.
        ResetSpriteQueue();

        g_frameTimeAccumulator -= g_frameTargetTime;
        if (g_frameTimeAccumulator < 0) g_frameTimeAccumulator = 0;

        g_LastFrameTime_ms = currentTime;

        if (g_frameTimeAccumulator > g_frameTargetTime) {
            g_frameTimeAccumulator = 0;
        }
    }

    // StMask countdown (0x004d4684). StMask(0,N) stores N here and clears
    // g_ScreenAccessReady; once it reaches zero, presentation is re-enabled.
    // The original FrameRateGovernor (0x004973d0) decrements THIS variable,
    // not g_ScreenAccessCheck (0x004d2290, which is a separate flag read by
    // main_loop and written by the menu-transition code). Using the wrong
    // variable left g_ScreenAccessReady stuck at 0 after any StMask(0,N),
    // freezing presentation on a stale backbuffer.
    if (g_ScreenAccessCountdown != 0) {
        g_ScreenAccessCountdown--;
        if (g_ScreenAccessCountdown == 0) g_ScreenAccessReady = 1;
    }
    if (g_RenderAccessCheck != 0) {
        g_RenderAccessCheck--;
        if (g_RenderAccessCheck == 0) g_RenderAccessReady = 1;
    }
}

// ============================================================================
// FUN_0040a8f0 (0x0040a8f0)
// ============================================================================
void FUN_0040a8f0(void* param)
{
    g_titlePrimType = 1;
    g_primFlag2 = 2;
    g_primParam = g_sceneRenderParam;
    OT_InsertPrimitive(&g_titlePrimType, 0xFFF);
}

// ============================================================================
// OT_InsertPrimitive (0x004402f0)
// ============================================================================
void OT_InsertPrimitive(void* prim, unsigned int depth)
{
    if (depth != 0xFFF) return;

    DWORD* p = (DWORD*)prim;
    if (p[0] != 1) return;

    if (g_displayImageSRV == MARNI_NULL_HANDLE) return;
    // Menu-mode bit hides the room bg quad (the real menus draw their own
    // graphics). The F1 debug menu keeps the frozen game visible underneath,
    // so don't drop the bg while it is open - its flag editor can set this
    // very bit (g_main_state_flags is flag bank 5), which blacked the screen.
    // g_debugMenuOpen stays 0 while debug features are disabled.
    if ((g_main_state_flags & MSF_SCREEN_STANDALONE) != 0 && g_debugMenuOpen == 0) {
        return;
    }
    if (g_pendingSpriteCount >= MAX_PENDING_SPRITES) return;

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;

    // The display image is a logical-resolution framebuffer (320x240 in game);
    // the quad spans logical * scale = the full backbuffer (see AddTintSprite).
    float scaleX, scaleY;
    MarniGetRenderScale(&scaleX, &scaleY);
    DWORD lw = (pD3D && pD3D->m_logicalWidth  >= 320) ? pD3D->m_logicalWidth  : 320;
    DWORD lh = (pD3D && pD3D->m_logicalHeight >= 240) ? pD3D->m_logicalHeight : 240;

    // Display-image origin (0x004c335c/0x004c3360, set by Display_SetParams).
    // The original moves the background with the screen shake by moving this
    // origin - FUN_00470a90 rebuilt the background sprites around it every
    // frame, and that rebuild is a no-op here because the background is one
    // full-screen quad instead. Shifting the quad would uncover a gap at the
    // trailing edge, so shift the sampled window by the same amount instead:
    // the sampler is CLAMP (MarniDX.cpp), so the edge row/column smears by a
    // pixel rather than showing through. Origin +1 moves the image right, i.e.
    // samples one pixel further left.
    float du = -(float)g_displayImageOriginX / (float)lw;
    float dv = -(float)g_displayImageOriginY / (float)lh;

    for (int i = g_pendingSpriteCount; i > 0; i--) {
        g_pendingSprites[i] = g_pendingSprites[i - 1];
    }
    g_pendingSprites[0].x = 0;
    g_pendingSprites[0].y = 0;
    g_pendingSprites[0].w = (float)lw * scaleX;
    g_pendingSprites[0].h = (float)lh * scaleY;
    g_pendingSprites[0].u0 = du;
    g_pendingSprites[0].v0 = dv;
    g_pendingSprites[0].u1 = 1.0f + du;
    g_pendingSprites[0].v1 = 1.0f + dv;
    unsigned int r = (unsigned int)((g_color_r < 0.0f ? 0.0f : g_color_r > 1.0f ? 1.0f : g_color_r) * 255.0f);
    unsigned int g = (unsigned int)((g_color_g < 0.0f ? 0.0f : g_color_g > 1.0f ? 1.0f : g_color_g) * 255.0f);
    unsigned int b = (unsigned int)((g_color_b < 0.0f ? 0.0f : g_color_b > 1.0f ? 1.0f : g_color_b) * 255.0f);
    g_pendingSprites[0].color = 0xFF000000u | (r << 16) | (g << 8) | b;
    g_pendingSprites[0].tex = g_displayImageSRV;
    g_pendingSprites[0].valid = TRUE;
    g_pendingSprites[0].depth = 0xFFF;  // background (far, drawn first)
    g_pendingSpriteCount++;
}

// ============================================================================
// ResetSpriteQueue
// ============================================================================
void ResetSpriteQueue(void)
{
    g_pendingSpriteCount = 0;
    SpriteQueue_Reset();
    TmdQueue_Reset();
}

// ============================================================================
// ResetFmvRenderState (0x004973a0) - FMV cleanup on state change, called from
// main_loop when the 0x40000 state flag drops. Calls CMarniDirect3D vtable[11]
// ResetTextures (0x00448380), marks m_scratch (0x324) = 3, resets all
// sprite/TMD queues and clears the video-mode debug overlay flag.
// ============================================================================
void ResetFmvRenderState(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[11] != NULL) {
        ((void(*)(void*))pD3D->vtable[11])(pD3D);
        pD3D->m_scratch = 3;
    }
    ResetSpriteQueue();
    g_VideoModeOverlayActive = 0;
}

// ============================================================================
// ShowVideoModeDebugText (0x00497af0) - debug overlay showing the current
// video mode ("%dx%dx%d", top-left) for 60 frames after the toggle key.
// g_ShowVideoModeOverlay is the edge-trigger latch (cleared here, timer armed
// to 60); ResetFmvRenderState clears the active flag.
//
// D3D adaptation: the original queues per-glyph descriptors through the
// MarniSystem DirectFont class (DrawFormattedBitmapText 0x0040c8d0). The port
// renders all text through the fontus.tim pipeline instead, so the draw goes
// through PRINT_TEXT_BUFFER / PrintText8x8 with the same position and timing.
// ============================================================================
void ShowVideoModeDebugText(void)
{
    if (g_ShowVideoModeOverlay != 0) {
        g_ShowVideoModeOverlay = 0;
        g_VideoModeOverlayTimer = 60;
    }

    if (g_VideoModeOverlayTimer > 0) {
        g_VideoModeOverlayTimer--;
        CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
        sprintf(PRINT_TEXT_BUFFER, "%dx%dx%d",
                pD3D ? pD3D->m_width : 0,
                pD3D ? pD3D->m_height : 0,
                pD3D ? pD3D->m_bitDepth : 0);
        PrintText8x8(0, 0, 0, 1);
        g_VideoModeOverlayActive = 0;
    }
}

// ============================================================================
// display_texture (0x0046ea05)
// Original 4-param version: takes a texture descriptor, depth/fade value,
// starting slot, and page count. Searches texture page descriptors to find
// the VRAM page containing the texture, computes page-relative UVs, and
// enqueues a sprite command.
//
// DX11 adaptation: The original's page descriptor table spans ~393KB in
// Marni memory (0x008ed838 to 0x008f7890), but our g_VideoDriverArray_838
// is only 8KB. The search loop cannot work with undersized arrays, so we
// use the caller-provided slot+0xF directly as the SRV index. UVs are
// scaled by the BPP factor because PS1 texU/texV are in VRAM-pixel
// coordinates and the SRV is expanded to full RGBA texels.
// ============================================================================
int display_texture(TextureDesc* texture, unsigned short depth, int slot, int pageCount,
                    unsigned int sortClass)
{
    // 0x0046e8d0: queue overflow
    if ((MAX_SPRITE_COMMANDS - 1) < g_SpriteQueueCount) return 0;

    // BPP scale = g_dwTexScaleFactors[(flags>>24)&3] = {4,2,1,1}
    static const int s_TexScale[4] = { 4, 2, 1, 1 };
    int scale = s_TexScale[(texture->flags >> 24) & 3];

    // VRAM-space texture position (from the tpage code, texU, texV)
    unsigned int vAdd = 0;
    unsigned int p    = texture->texturePage;
    if (p > 16) { vAdd = 256; p -= 16; }
    int texUWords = (int)(p * 0x40u + texture->texU / scale);
    int texVAbs   = (int)(vAdd + texture->texV);

    // Search page descriptors — original scans up to pageCount (max 0x2E)
    int shiftedSlot = slot + 0xF;
    if (shiftedSlot < 0 || shiftedSlot >= 256) return 0;

    int foundSlot    = -1;
    int foundOriginX = 0, foundOriginY = 0;
    int foundPage   = 0;

    // DEBUG: remember the first loaded slot in range so a failed search can
    // report what was checked against what. The menu textures (frame 15,
    // portrait 24, items 16-23) live in 15-30; a failure there with loaded
    // SRVs means the page-bounds/clut metadata does not contain the desc.
    int firstLoaded = -1;
    int firstMeta[6] = { 0, 0, 0, 0, 0, 0 };   // oX, oY, depth, bpp, W, H
    int firstBw = 1;

    for (int i = 0; i < pageCount; i++) {
        int cur = shiftedSlot + i;
        if (cur >= 256) break;
        if (g_TexturePageSRV[cur] == NULL) continue;

        short oX  = g_TexturePageOriginX[cur];
        short oY  = g_TexturePageOriginY[cur];
        short d   = g_TexturePageId[cur];
        int   bpp = g_TexturePageBpp[cur];
        if (bpp <= 0) bpp = 16;
        int bw = bpp == 4 ? 4 : bpp == 8 ? 2 : 1;
        if (firstLoaded < 0) {
            firstLoaded = cur;
            firstMeta[0] = oX; firstMeta[1] = oY; firstMeta[2] = d;
            firstMeta[3] = bpp; firstMeta[4] = g_TexturePageWidth[cur];
            firstMeta[5] = g_TexturePageHeight[cur];
            firstBw = bw;
        }

        // VRAM page corner from pageDepth (tpage code):
        int pX = (int)(d & 15) * 0x40;
        int pY = (int)(d / 16) * 0x100;

        int pageW = g_TexturePageWidth[cur]  / bw;
        int pageH = g_TexturePageHeight[cur];
        int pageL = oX + pX, pageR = pageL + pageW;
        int pageT = oY + pY, pageB = pageT + pageH;

        int texR  = texUWords + texture->width / scale;
        int texB  = texVAbs   + texture->height;

        if (pageL <= texUWords && texR <= pageR &&
            pageT <= texVAbs   && texB <= pageB) {
            foundSlot   = cur;
            foundOriginX = oX;
            foundOriginY = oY;
            foundPage   = d;
            break;
        }
    }
    if (foundSlot < 0) return 0;

    // CLUT lookup: uVar4 = clutY - clutBase;  ==8 → 1
    int clutIdx = (int)texture->clutY - g_TexturePageClutBase[foundSlot];
    if (clutIdx < 0 || clutIdx > 7) return 0;
    if (clutIdx == 8) clutIdx = 1;
    // (Multi-CLUT: page handle = g_TexturePageTable[foundSlot*0xDF + clutIdx]
    //  DX11: use foundSlot + clutIdx for per-palette SRVs when implemented)

    // UV computation (page-relative PIXEL units):
    int pageOfs = ((int)texture->texturePage - foundPage) * scale * 0x40;
    int su0 = (int)texture->texU - foundOriginX * scale + pageOfs;
    int sv0 = (int)texture->texV - foundOriginY;
    int su1 = su0 + texture->width  - 1;
    int sv1 = sv0 + texture->height - 1;

    // ---------- Build sprite command ----------
    short sx = texture->screenX + g_ScreenOffsetX;
    short sy = texture->screenY + g_ScreenOffsetY;
    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = sortClass;

    cmd->spriteFlags = SpriteBuildFlags(texture->flags);

    cmd->r = (float)texture->colorMulR * g_ColorScaleFactor;
    cmd->g = (float)texture->colorMulG * g_ColorScaleFactor;
    cmd->b = (float)texture->colorMulB * g_ColorScaleFactor;

    // 0x0046e70e: an x87 FLOAT store of g_dwTexVariantBlend[variant]/256 into
    // +0x2c - the per-primitive semi-transparency level, not a page index.
    cmd->variantAlpha = SpriteVariantAlpha(texture->flags);
    cmd->alpha        = SpriteDrawAlpha(cmd->variantAlpha);

    cmd->x0  = sx - texture->pivotX;
    cmd->y0  = sy - texture->pivotY;
    cmd->x1  = (texture->width  - texture->pivotX) + sx - 1;
    cmd->y1  = (texture->height - texture->pivotY) + sy - 1;
    cmd->depthSort = (unsigned int)depth * 0x10 + 500;

    cmd->u0 = (unsigned short)(su0 >= 0 ? su0 : 0);
    cmd->v0 = (unsigned short)(sv0 >= 0 ? sv0 : 0);
    cmd->u1 = (unsigned short)(su1 >= 0 ? su1 : 0);
    cmd->v1 = (unsigned short)(sv1 >= 0 ? sv1 : 0);
    cmd->extraFlags = foundSlot;

    // Fade inversion + enqueue
    unsigned short fadeVal = depth;
    if (g_nFadeInverted) {
        if ((int)fadeVal > g_MaxFadeValue) fadeVal = (unsigned short)g_MaxFadeValue;
        fadeVal = (unsigned short)(g_MaxFadeValue - fadeVal);
    }
    if (fadeVal > 0xFFF) fadeVal = 0xFFF;

    if ((g_RenderDisableFlags & 0x21) == 0) g_SpriteQueueCount++;
    return 1;
}

// ============================================================================
// AddSprite_Ex (0x0046f280)
// Scaled-sprite variant of display_texture, used by the map screen for the
// zoom in/out animation. Same page-search core as display_texture, plus:
//   - when scaleX/scaleY (fix16.12) are both 0x1000 (1.0), positions are
//     relative to g_ScreenOffsetX/Y (screen space, pivot not scaled);
//   - otherwise the quad is scaled about pivotX/pivotY with fix16.12 math
//     and placed relative to g_SubpixelOffsetX/Y (PS1 subpixel space).
// ============================================================================
// SubmitEffectSprite_Ex (0x0046edb0) shares this whole body: same page search,
// same CLUT check, same UV maths. It differs only in that it never scales
// about the pivot and that its ordering key is a separate argument instead of
// the fade value. Both are expressed through the two extra parameters here.
static int AddSpriteEx_Core(TextureDesc* texture, unsigned short depth, int slot,
                            int pageCount, int depthKey, bool allowScale,
                            unsigned int sortClass)
{
    if ((MAX_SPRITE_COMMANDS - 1) < g_SpriteQueueCount) return 0;

    static const int s_TexScale[4] = { 4, 2, 1, 1 };
    int scale = s_TexScale[(texture->flags >> 24) & 3];

    // VRAM-space texture position (from the tpage code, texU, texV)
    unsigned int vAdd = 0;
    unsigned int p    = texture->texturePage;
    if (p > 16) { vAdd = 256; p -= 16; }
    int texUWords = (int)(p * 0x40u + texture->texU / scale);
    int texVAbs   = (int)(vAdd + texture->texV);

    // Search page descriptors — original scans up to pageCount (max 0x2E)
    int shiftedSlot = slot + 0xF;
    if (shiftedSlot < 0 || shiftedSlot >= 256) return 0;

    int foundSlot    = -1;
    int foundOriginX = 0, foundOriginY = 0;
    int foundPage   = 0;

    for (int i = 0; i < pageCount; i++) {
        int cur = shiftedSlot + i;
        if (cur >= 256) break;
        if (g_TexturePageSRV[cur] == NULL) continue;

        short oX  = g_TexturePageOriginX[cur];
        short oY  = g_TexturePageOriginY[cur];
        short d   = g_TexturePageId[cur];
        int   bpp = g_TexturePageBpp[cur];
        if (bpp <= 0) bpp = 16;
        int bw = bpp == 4 ? 4 : bpp == 8 ? 2 : 1;

        int pX = (int)(d & 15) * 0x40;
        int pY = (int)(d / 16) * 0x100;

        int pageW = g_TexturePageWidth[cur]  / bw;
        int pageH = g_TexturePageHeight[cur];
        int pageL = oX + pX, pageR = pageL + pageW;
        int pageT = oY + pY, pageB = pageT + pageH;

        int texR  = texUWords + texture->width / scale;
        int texB  = texVAbs   + texture->height;

        if (pageL <= texUWords && texR <= pageR &&
            pageT <= texVAbs   && texB <= pageB) {
            foundSlot   = cur;
            foundOriginX = oX;
            foundOriginY = oY;
            foundPage   = d;
            break;
        }
    }
    if (foundSlot < 0) return 0;

    int clutIdx = (int)texture->clutY - g_TexturePageClutBase[foundSlot];
    if (clutIdx < 0 || clutIdx > 7) return 0;
    if (clutIdx == 8) clutIdx = 1;

    // UV computation (page-relative PIXEL units):
    int pageOfs = ((int)texture->texturePage - foundPage) * scale * 0x40;
    int su0 = (int)texture->texU - foundOriginX * scale + pageOfs;
    int sv0 = (int)texture->texV - foundOriginY;
    int su1 = su0 + texture->width  - 1;
    int sv1 = sv0 + texture->height - 1;

    // 0x0046f2dd: unscaled sprites use the screen offset; scaled sprites
    // (map zoom) use the subpixel offset.
    bool scaled = allowScale && ((texture->scaleX != 0x1000) || (texture->scaleY != 0x1000));
    short sx = (short)(texture->screenX + (scaled ? (short)g_SubpixelOffsetX : (short)g_ScreenOffsetX));
    short sy = (short)(texture->screenY + (scaled ? (short)g_SubpixelOffsetY : (short)g_ScreenOffsetY));

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = sortClass;

    cmd->spriteFlags = SpriteBuildFlags(texture->flags);

    cmd->r = (float)texture->colorMulR * g_ColorScaleFactor;
    cmd->g = (float)texture->colorMulG * g_ColorScaleFactor;
    cmd->b = (float)texture->colorMulB * g_ColorScaleFactor;

    // 0x0046f0c5 / 0x0046f61b: see TextureDraw::variantAlpha.
    cmd->variantAlpha = SpriteVariantAlpha(texture->flags);
    cmd->alpha        = SpriteDrawAlpha(cmd->variantAlpha);

    // 0x0046f36a: fix16.12 scale about the pivot point, rounding like the
    // original ((x + ((x >> 0x1f) & 0xfff)) >> 0xc).
    if (scaled) {
        int ix = (int)texture->pivotX * (int)texture->scaleX;
        cmd->x0 = sx - (short)((ix + (ix >> 0x1f & 0xfff)) >> 0xc);
        int iy = (int)texture->pivotY * (int)texture->scaleY;
        cmd->y0 = sy - (short)((iy + (iy >> 0x1f & 0xfff)) >> 0xc);
        ix = ((int)texture->width - (int)texture->pivotX) * (int)texture->scaleX;
        cmd->x1 = (short)((ix + (ix >> 0x1f & 0xfff)) >> 0xc) + sx - 1;
        iy = ((int)texture->height - (int)texture->pivotY) * (int)texture->scaleY;
        cmd->y1 = (short)((iy + (iy >> 0x1f & 0xfff)) >> 0xc) + sy - 1;
    } else {
        cmd->x0 = sx - texture->pivotX;
        cmd->y0 = sy - texture->pivotY;
        cmd->x1 = (texture->width  - texture->pivotX) + sx - 1;
        cmd->y1 = (texture->height - texture->pivotY) + sy - 1;
    }
    cmd->depthSort = (unsigned int)depthKey * 0x10 + 500;

    cmd->u0 = (unsigned short)(su0 >= 0 ? su0 : 0);
    cmd->v0 = (unsigned short)(sv0 >= 0 ? sv0 : 0);
    cmd->u1 = (unsigned short)(su1 >= 0 ? su1 : 0);
    cmd->v1 = (unsigned short)(sv1 >= 0 ? sv1 : 0);
    cmd->extraFlags = foundSlot;

    // Fade inversion + enqueue
    unsigned short fadeVal = depth;
    if (g_nFadeInverted) {
        if ((int)fadeVal > g_MaxFadeValue) fadeVal = (unsigned short)g_MaxFadeValue;
        fadeVal = (unsigned short)(g_MaxFadeValue - fadeVal);
    }
    if (fadeVal > 0xFFF) fadeVal = 0xFFF;

    if ((g_RenderDisableFlags & 0x21) == 0) g_SpriteQueueCount++;
    return 1;
}

int AddSprite_Ex(TextureDesc* texture, unsigned short depth, int slot, int pageCount)
{
    return AddSpriteEx_Core(texture, depth, slot, pageCount, (int)depth, true,
                            SPRITE_CLASS_NORMAL);
}

// ============================================================================
// SubmitEffectSprite_Ex (0x0046edb0)
// The paged-sprite submit the lab computer terminal draws its whole UI
// through. Unscaled, and the sort key is explicit so a caller can lay several
// sprites of the same fade value into a deliberate front-to-back order.
//
// These are SPRITE_CLASS_SCENE, not NORMAL. The explicit sort key IS an
// ordering-table index - CMarniDirect3D::SetTexture (0x00448300) files a type-10
// primitive at `depthSort >> 4` in the one OT that also holds the entity
// triangles (an entity enters it at t[2] >> 4), so the terminal's overlays
// genuinely interleave with the player's forearm models by depth. Classing them
// NORMAL put every one of them in the flat 2D pass that runs AFTER
// FlushTmdObjects, so the keyboard and the typed text painted over the hands
// instead of the hands reaching in front of them. The window frames and the
// monitor pictures sit at depthSort 4596-26100 and stay behind the arms either
// way; it is the keyboard/text/caret group at 660-740 that has to lose.
// ============================================================================
int SubmitEffectSprite_Ex(TextureDesc* texture, unsigned short fade, int slot,
                          int pageCount, int depthKey)
{
    return AddSpriteEx_Core(texture, fade, slot, pageCount, depthKey, false,
                            SPRITE_CLASS_EFFECT);
}

// ============================================================================
// display_image (0x00470770)
// Loads raw 16-bit PS1 pixel data and creates a D3D11 texture + SRV.
// ============================================================================
void display_image(int slot, void* buffer, int width, int height)
{
    g_DisplayImageWidth = width;
    g_DisplayImageHeight = height;

    if (g_displayImageSRV != MARNI_NULL_HANDLE) {
        Marni_DX()->DestroyTexture(g_displayImageSRV);
        g_displayImageSRV = MARNI_NULL_HANDLE;
    }

    unsigned short* src = (unsigned short*)buffer;
    int pixelCount = width * height;
    unsigned int* rgba = (unsigned int*)malloc(pixelCount * 4);
    if (rgba == NULL) return;

    // The display image is the base layer: AddBackgroundQuad only accepts it at
    // depth 0xFFF and inserts it at the head of the pending list, so nothing is
    // ever drawn behind it. The original blitted it into the framebuffer WITHOUT
    // CMarniBits::BltFast's colorkey flag (flags bit 0), unlike the texture-page
    // blits — a background has nothing to key against.
    //
    // Every pixel is therefore opaque. Black must NOT be keyed out here: unlike
    // the sprite pages, pure black in a background is real artwork. TYPE00.TIM
    // (the save-screen typewriter) is 43% pure black, OPT11/JOPT06 store the
    // option panel's dark fill as black, and room backgrounds are full of black
    // shadow. Keying those transparent let VTable_Clear's clear colour — which
    // is (0, 0, 0.05), not pure black, and is overridable via g_debugClearR/G/B —
    // bleed through every dark area of every background as a blue lift.
    for (int i = 0; i < pixelCount; i++) {
        unsigned short px = src[i];
        unsigned char r = ((px >> 0)  & 0x1F) * 255 / 31;
        unsigned char g = ((px >> 5)  & 0x1F) * 255 / 31;
        unsigned char b = ((px >> 10) & 0x1F) * 255 / 31;
        rgba[i] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
    }

    MarniCreateTexture(width, height, 32, rgba, &g_displayImageSRV);
    free(rgba);
}

// ============================================================================
// SetFrameRateMode - Set frame rate unlocked mode based on game active state
// Original: FUN_00442150 at 0x00442150
// ============================================================================
void SetFrameRateMode(int bActive) {
    if (bActive != 0) { g_bUseFrameSkip = TRUE; }
    else { g_bUseFrameSkip = FALSE; }
}

void ResetScreenPanning(void) {
    CenterScreenOrigin();
    // Dummy_00429a30()
    g_main_state_flags = g_main_state_flags & ~MSF_PANNING_RESET;
}

// ============================================================================
// SetScreenOffset (0x00483600)
// Sets both screen offset and subpixel rendering offset.
// The original PS1 code stored absolute screen coordinates (including g_ScreenOffsetX/Y)
// in the sprite command buffer. FlushSpriteCommands converts these PS1-space coordinates
// to screen-space coordinates using scaleX/scaleY.
// ============================================================================
void SetScreenOffset(int x, int y)
{
    g_ScreenOffsetX = x;
    g_ScreenOffsetY = y;
    g_SubpixelOffsetX = x;
    g_SubpixelOffsetY = y;
}

// ============================================================================
// ApplyScreenShake (0x0045aac0)
// Generates random ±1 screen shake offsets and applies them centered at (160,120).
// ============================================================================
void ApplyScreenShake(void)
{
    int val;

    // Random X offset: (rand() & 1) with random sign
    val = rand();
    signed char signX = (signed char)(val >> 31);
    g_ScreenShakeOffsetX = (signed char)((((unsigned char)val ^ signX) - signX) & 1 ^ signX) - signX;

    // Random Y offset: (rand() & 1) with random sign
    val = rand();
    signed char signY = (signed char)(val >> 31);
    g_ScreenShakeOffsetY = (signed char)((((unsigned char)val ^ signY) - signY) & 1 ^ signY) - signY;

    // Random direction (0-3) to optionally negate X and/or Y
    val = rand();
    unsigned int uSign = (unsigned int)((int)val >> 31);
    int dir = (int)(((val ^ uSign) - uSign) & 3 ^ uSign) - uSign;
    if (dir != 1) {
        if (dir == 2) {
            g_ScreenShakeOffsetX = -g_ScreenShakeOffsetX;
        } else if (dir == 3) {
            g_ScreenShakeOffsetX = -g_ScreenShakeOffsetX;
            g_ScreenShakeOffsetY = -g_ScreenShakeOffsetY;
        }
    } else {
        g_ScreenShakeOffsetY = -g_ScreenShakeOffsetY;
    }

    // Apply shake to screen offset and subpixel offset.
    // In the original: SetScreenOffset(shakeX + 0xa0, shakeY + 0x78) set both
    // g_ScreenOffsetX/Y and g_SubpixelOffsetX/Y to the full centering+shake value.
    // (0x0045ab34-0x0045ab4c is only the SetScreenOffset call - the two stores of
    // the bare shake offset that used to sit here were dead, SetScreenOffset
    // overwrites both with the centred value anyway.)
    SetScreenOffset(g_ScreenShakeOffsetX + 160, g_ScreenShakeOffsetY + 120);
}

// 0x0045ab60
void ApplyShakeAndRebuildSprites() {
    if ( (g_main_state_flags2 & MSF2_SCREEN_SHAKE) == 0 || (g_main_state_flags & MSF_MENU_BYTE) != 0 )
    Display_SetParams(2, 2);
    else
    Display_SetParams(g_ScreenShakeOffsetX + 2, g_ScreenShakeOffsetY + 2);
    SetScreenReady(1);
    FUN_00470a90();
}

// ============================================================================
// FUN_00455140 (0x00455140) - Item name lookup
// Returns a pointer to the item name string (RE1 font encoding). If the item
// has not been examined yet (its game flag is clear), the generic name for
// its category is returned instead (e.g. "MANSION KEY").
// ItemNames.h (the DC item-name table) is included here rather than at the
// top: this file's includes sit at the bottom of the file (line ~1870).
#include "dc/ItemNames.h"

// DC + JPN name table. The Japanese release has no Director's Cut table of its
// own, and the DC's table is English, so in DC + JPN the DC's four relocations
// are applied to the Japanese table instead: the DC moves the LOCKPICK name
// onto item 0x0D (where the DUMDUM rounds were), gives item 0x04 the BERETTA
// name (the "custom edition" handgun) and puts MOON CREST on the two crest
// halves 0x31/0x32. Each override points at the Japanese string for that same
// thing - 0x02 BERETTA, 0x31 LOCKPICK and 0x2C MOON CREST in the JPN table - so
// no English text leaks into a Japanese session. Built once, on first use.
static const unsigned char** dc_jpn_item_names(void)
{
    static const unsigned char* table[128];
    static int built = 0;
    if (!built) {
        for (int i = 0; i < 128; ++i) {
            table[i] = g_ItemNamePointersJpn[i];
        }
        table[0x04 - 1] = g_ItemNamePointersJpn[0x02 - 1];   // BERETTA
        table[0x0d - 1] = g_ItemNamePointersJpn[0x31 - 1];   // LOCKPICK
        table[0x31 - 1] = g_ItemNamePointersJpn[0x2c - 1];   // MOON CREST
        table[0x32 - 1] = g_ItemNamePointersJpn[0x2c - 1];   // MOON CREST
        built = 1;
    }
    return table;
}

// DC + USA name table. The DC has no sub machine guns, so its table carries
// neither name: ids 0x6F and 0x70 both point at the stale "CRANK" string
// (SLUS_005.51 0x800901D8 entries 110/111 -> 0x8008FEF6) and the generic-name
// group that follows starts two entries early. The port keeps the PC weapons in
// DC mode - the ending hands them out and the item viewer loads the USA
// item_m2 models - so those two ids take the USA names.
static const unsigned char** dc_usa_item_names(void)
{
    static const unsigned char* table[DC_ITEM_NAME_COUNT];
    static int built = 0;
    if (!built) {
        for (int i = 0; i < DC_ITEM_NAME_COUNT; ++i) {
            table[i] = g_dcItemNamePointers[i];
        }
        table[ITEM_INGRAM - 1] = g_ItemNamePointers[ITEM_INGRAM - 1];   // INGRAM
        table[ITEM_MINIMI - 1] = g_ItemNamePointers[ITEM_MINIMI - 1];   // MINIMI
        built = 1;
    }
    return table;
}

unsigned char* message_item_name_lookup(unsigned char itemId)
{
    // The Japanese release has its own pair of tables (0x004cd388/0x004cd548,
    // read by its message_item_name_lookup at 0x00491440) holding the names in
    // FONT.TIM's encoding. The USA strings would still draw - the two fonts
    // share their latin rows - but they would draw in English. So the version
    // picks the language first and only then applies the DC's edits.
    const int jpn = (GetAssetVersion() != 0);
    // The Director's Cut renames four ids (0x04, 0x0D, 0x31/0x32) and drops the
    // two PC-only sub machine guns; its table is otherwise identical to the USA
    // one, so only the pointer table switches.
    const unsigned char** names = jpn ? (g_bDcMode ? dc_jpn_item_names()
                                                   : g_ItemNamePointersJpn)
                               : g_bDcMode ? dc_usa_item_names()
                                           : g_ItemNamePointers;
    const unsigned char** unknown = jpn ? g_UnknownItemNamePointersJpn
                                        : g_UnknownItemNamePointers;

    unsigned char bVar2 = itemId - 1;
    unsigned char* puVar3 = (unsigned char*)names[bVar2];
    if ((bVar2 < 0x4d) && ((bVar2 = g_ItemImageLookupTable[(unsigned int)bVar2 * 4 + 6], (bVar2 & 0x80) == 0))) {
        if (Flg_ck((int)g_itemExaminedFlags, (unsigned int)bVar2) == 0) {
            puVar3 = (unsigned char*)unknown[bVar2];
        }
    }
    return puVar3;
}

// ============================================================================
// FUN_00456020 (0x00456020) - Message character rendering
// Renders the message text characters from g_MessagePtr up to g_MessageCurrentPtr.
// Handles newlines, color changes, item name substitution, and character glyphs.
// ============================================================================
static void message_render_chars(void)
{
    unsigned char bVar1;
    unsigned char* pbVar2;
    unsigned char* pbVar3;
    unsigned short fade;
    // 0x00456020: case 7 (return-from-item-name) resumes at savedPtr+2 from the
    // last case 6 tag. A tag 7 can arrive without a preceding tag 6 when
    // set_message_display restarts the pass mid-substitution (rapid menu
    // open/close); reading the uninitialized local sent the render loop through
    // stale stack garbage (release crash: EIP=0x000EA98D, garbage call target;
    // debug crash log: EAX/EDX=0xCCCCCCCE in message_render_chars).
    unsigned char* savedPtr = NULL;

    // Game-text glyph width: 8px (USA/GOG fontus.tim) or 14px (Japanese
    // FONT.TIM). The JPN renderers (draw_item_name 0x004912c0,
    // PrintFormattedText 0x00491490, PrintText8x14 0x00491830) all use
    // texU=(b%18)*14 and a +14 cursor advance.
    const int glyphW = (GetAssetVersion() != 0) ? 14 : 8;
    // Left margin of the message box. The wider Japanese glyphs need the text
    // to start further left or the line runs off the right edge, so the JPN
    // message_render_chars (0x00492360) opens at 0x22 where the USA one
    // (0x00456020) opens at 0x30 — both on entry and after every line break.
    const short msgLeft = (GetAssetVersion() != 0) ? 0x22 : 0x30;

    g_TextureDesc.screenX = msgLeft - g_ScreenOffsetX;
    g_TextureDesc.screenY = g_MessageScreenY;
    g_TextureDesc.flags = 0x40;
    g_TextureDesc.width = glyphW;
    g_TextureDesc.clutY = g_MessageClutBase + 0x1e0;
    g_TextureDesc.height = 0xe;
    g_TextureDesc.clutX = 0x100;

    pbVar2 = g_MessagePtr;
    if (g_MessagePtr == g_MessageCurrentPtr) {
        g_DepthSortOverride = 0;
        return;
    }

    do {
        bVar1 = *pbVar2;
        if (bVar1 == 0) goto msg_next_char;

        switch (bVar1) {
        case 2: // newline
            pbVar3 = pbVar2 + 1;
            g_TextureDesc.screenY += 0x10;
            g_TextureDesc.screenX = msgLeft - g_ScreenOffsetX;
            break;

        case 3: // unknown tag (skip 1 byte)
        case 4: // unknown tag (skip 1 byte)
            pbVar2 = pbVar2 + 1;
            // fall through
        case 1: // end-of-page delay marker
            pbVar3 = pbVar2 + 1;
            break;

        case 5: // set CLUT color
            g_TextureDesc.clutX = 0x100;
            g_TextureDesc.clutY = pbVar2[1] + 0x1e0;
            pbVar3 = pbVar2 + 2;
            break;

        case 6: // item name lookup
            bVar1 = pbVar2[1];
            if (pbVar2[1] == 0) {
                bVar1 = g_selectedItemId;
            }
            pbVar3 = message_item_name_lookup(bVar1);
            savedPtr = pbVar2;
            break;

        case 7: // return from item name
            if (savedPtr != NULL) {
                pbVar3 = savedPtr + 2;
            } else {
                // No matching tag 6 this pass: skip the stray tag instead of
                // dereferencing stale stack data.
                pbVar3 = pbVar2 + 1;
            }
            break;

        case 0xf8: // single-width character
            pbVar3 = pbVar2 + 1;
            pbVar2 = pbVar2 + 1;
            bVar1 = *pbVar3 / 0x12 + 0xf;
            goto msg_render_char;

        case 0xf9: // medium-width character
            bVar1 = pbVar2[1] / 0x12;
            goto msg_render_char_wide;

        case 0xfa: // full-width character
            bVar1 = pbVar2[1] / 0x12 + 0xe;
msg_render_char_wide:
            g_TextureDesc.texturePage = 0x1f;
            pbVar2 = pbVar2 + 1;
            goto msg_draw_char;

        default: // normal character
            bVar1 = bVar1 / 0x12 + 2;
msg_render_char:
            g_TextureDesc.texturePage = 0x1e;
msg_draw_char:
            // Calculate texture coordinates from character index
            g_TextureDesc.texV = bVar1 * 0xe;
            g_TextureDesc.texU = *pbVar2 % 0x12 * glyphW;

            g_DepthSortOverride = 0;

            // Fade type selection for specific room/camera
            if ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_CONTROL_ROOM) &&
                (g_roomCameraId == 0x04 || g_roomCameraId == 0x00)) {
                fade = 0;
            } else {
                fade = 2;
            }

            AddTintSprite(&g_TextureDesc, fade);

msg_next_char:
            g_TextureDesc.screenX += glyphW;
            pbVar3 = pbVar2 + 1;
            break;
        }
        pbVar2 = pbVar3;
    } while (pbVar3 != g_MessageCurrentPtr);

    g_DepthSortOverride = 0;
}

// ============================================================================
// handle_message_post_action (0x00455fb0)
// Handles the action that follows a dismissed message: item usage,
// room events, lab slides, follow-up messages, and other conditional actions.
//
// Lab-slides note: the original's action switch is a tail-jump table at
// 0x004c2130 whose actions 4/5/6 jump straight INTO the shared lab-slides
// blocks (0x00463320/0x004633c0/0x004636b0) that display_slides also
// dispatches to. Those blocks are ported once in LabSlides.cpp and called
// from here.
// ============================================================================
static void handle_message_post_action(void)
{
    g_MessageCurrentPtr = (unsigned char*)((int)g_MessageCurrentPtr + 1);
    unsigned char* pbVar2 = g_MessageCurrentPtr;
    int iVar5 = (unsigned int)(g_menu_choice_id & 1) * (unsigned int)*g_MessageCurrentPtr;
    g_MessageCurrentPtr = g_MessageCurrentPtr + iVar5 + 1;
    unsigned char bVar1 = *g_MessageCurrentPtr;
    g_MessageCurrentPtr = pbVar2 + iVar5 + 2;

    if (bVar1 == 9) {
        set_message_display(*g_MessageCurrentPtr, g_PauseGameInMsgFlag);
        return;
    }
    if (bVar1 != 10) {
        return;
    }

    switch (*g_MessageCurrentPtr) {
    case 0: // Room event / flag action
        // The original has room_event_take_item (0x004631c0) inlined here;
        // same body, one copy.
        room_event_take_item();
        return;

    case 1: // Use selected item
        {
            g_usedItemId = g_selectedItemId;
            if (!is_lockpick_item(g_selectedItemId)) {
                unsigned char bVar4 = 0;
                unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
                bVar1 = slots[0];
                while (bVar1 != g_selectedItemId) {
                    bVar4 = bVar4 + 1;
                    bVar1 = slots[(unsigned int)bVar4 * 2];
                }
                if (g_selectedItemId < ITEM_CLIP) { // is weapon
                    slots[(unsigned int)bVar4 * 2] = 0;
                    if ((unsigned int)g_EquippedItemId - (unsigned int)bVar4 == 1) {
                        g_EquippedItemId = 0;
                    }
                    rearrange_item_slots();
                    return;
                }
                bVar1 = slots[(unsigned int)bVar4 * 2 + 1];
                if (bVar1 != 0) {
                    slots[(unsigned int)bVar4 * 2 + 1] = bVar1 - 1;
                    if (slots[(unsigned int)bVar4 * 2 + 1] == 0) {
                        if ((ITEM_OIL < g_selectedItemId) && (g_selectedItemId < ITEM_DESK_KEY)) { // is door key
                            g_main_state_flags = g_main_state_flags | MSF_MENU_MODE_KEY_DEPLETED;
                            return;
                        }
                        slots[(unsigned int)bVar4 * 2] = 0;
                        rearrange_item_slots();
                    }
                }
            }
        }
        return;

    case 2: // Discard selected item from inventory
        {
            unsigned char bVar4 = 0;
            unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
            bVar1 = slots[0];
            while (bVar1 != g_selectedItemId) {
                bVar4 = bVar4 + 1;
                bVar1 = slots[(unsigned int)bVar4 * 2];
            }
            slots[(unsigned int)bVar4 * 2] = 0;
            rearrange_item_slots();
        }
        return;

    case 3: // No action
        return;

    case 4: // Lab slides start (tail-jumps into 0x00463320)
        lab_slides_start();
        return;

    case 6: // Lab slides end (tail-jumps into 0x004636b0)
        lab_slides_finish();
        return;
    }

    // Any other action value tail-jumps into the shared lab-slides update
    // block at 0x004633c0 (action 5 advances the strip one step).
    lab_slides_update();
}

// ============================================================================
// UpdateMessageDisplay (0x004557b0) - Message display state machine
// Called each frame from main_loop to advance the message display.
// Manages character-by-character text reveal, timing, yes/no prompts,
// and message dismissal.
// ============================================================================
void UpdateMessageDisplay(void)
{
    unsigned char bVar1;
    int lineCount;
    unsigned char* pbVar3;
    short screenX;
    unsigned short fade;

    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;
    g_SpriteAsyncFlag = 0;

    switch (g_MessageStateCounter) {

    // === State 0: Initialize message display ===
    case 0:
        g_MessageStateCounter = 1;
        // Set char delay: 1 frame normally, shifted by g_bGameActive
        g_MessageCharDelay = (unsigned char)(1 << (g_bGameActive == 0));
        g_MessageCurrentPtr = g_MessagePtr;
        g_MessageClutBase = 0;
        g_MessageClutCopy = 0;
        g_MessageLineCounter = 0;
        g_MessageCharTimer = g_MessageCharDelay;
        // Fall through to state 1

    // === State 1: Character-by-character text reveal ===
    case 1:
        bVar1 = g_MessageCharTimer;
        g_MessageCharTimer = g_MessageCharTimer - 1;

        if (g_MessageSpeedUpFlag == 0) {
    state1_check_timer:
            // Normal speed: wait for timer
            if (g_MessageCharTimer != 0) break;
        
            // Speed-up mode: reduce timer faster
        } else if (g_MessageCharTimer != 0) {
            if ((g_PlayerDpadHeld & 0x4000) != 0) {
                g_MessageCharTimer = bVar1 - 2; // double speed
            }
            goto state1_check_timer;
        }

        // Timer expired: process next character
        bVar1 = *g_MessageCurrentPtr;
        lineCount = g_MessageLineCounter;
        pbVar3 = g_MessageCurrentPtr;

    state1_process_char:
        g_MessageCurrentPtr = pbVar3;
        g_MessageLineCounter = lineCount;

        // Check for end of text or special character
        if (bVar1 == 0) goto msg_skip_char;
        bVar1 = *pbVar3;
        if (bVar1 > 0x0b && bVar1 < 0xf8) goto msg_skip_char;

        switch (bVar1) {
        case 1: // Page end marker with delay
            g_MessageCurrentPtr = pbVar3 + 1;
            if (*g_MessageCurrentPtr == 0) {
                // End of message: wait for input
                g_MessageStateCounter = 5;
                message_render_chars();
                return;
            }
            // Auto-advance after delay
            g_MessageStateCounter = 6;
            g_MessageCharTimer = *g_MessageCurrentPtr << (g_bGameActive == 0);
            message_render_chars();
            return;

        case 2: // Next page / skip
            goto state1_case2;

        case 3: // Newline (page break)
            g_SpriteAsyncFlag = 1;
            g_MessageLineCounter = lineCount + 1;
            if (lineCount < 5) {
                g_MessageCharTimer = g_MessageCharTimer + 1;
                message_render_chars();
                return;
            }
            // More than 5 lines: clear and continue
            g_MessageCurrentPtr = pbVar3 + 1;
            g_MessageLineCounter = 0;
            if (*g_MessageCurrentPtr == 0) {
                g_MessageStateCounter = 2;
                g_MessageCurrentPtr = pbVar3 + 2;
                message_render_chars();
                return;
            }
            g_MessageStateCounter = 3;
            g_MessageCharTimer = *g_MessageCurrentPtr << (g_bGameActive == 0);
            g_MessageCurrentPtr = pbVar3 + 2;
            message_render_chars();
            return;

        case 4: // Skip embedded tags
            g_MessageCurrentPtr = pbVar3 + 1;
            if (*g_MessageCurrentPtr == 0) {
                // Skip to end marker (tag 4)
                g_MessageCurrentPtr = pbVar3 + 2;
                bVar1 = *g_MessageCurrentPtr;
                while (bVar1 != 4) {
                    switch (*g_MessageCurrentPtr) {
                        case 5:
                        case 6:
                        case 0xf8:
                        case 0xf9:
                        case 0xfa:
                            g_MessageCurrentPtr++;
                    }
                    g_MessageCurrentPtr++;
                    bVar1 = *g_MessageCurrentPtr;
                }
                g_MessageCurrentPtr++;
            }
            g_MessageCharDelay = *g_MessageCurrentPtr << (g_bGameActive == 0);
            goto state1_case2;

        case 5: // Set CLUT color
            g_MessageCurrentPtr = pbVar3 + 1;
            g_MessageClutCopy = *g_MessageCurrentPtr;
state1_case2:
            g_MessageCurrentPtr++;
            break;

        case 6: // Item name lookup
            g_MessageCurrentPtr = pbVar3 + 1;
            bVar1 = *g_MessageCurrentPtr;
            if (*g_MessageCurrentPtr == 0) {
                bVar1 = g_selectedItemId;
            }
            g_MessageSavedPtr = pbVar3;
            g_MessageCurrentPtr = message_item_name_lookup(bVar1);
            break;

        case 7: // Return from item name
            g_MessageCurrentPtr = g_MessageSavedPtr + 2;
            break;

        case 8: // Yes/No prompt
            g_MessageStateCounter = 4;
            message_render_chars();
            return;

        case 0xf8:
        case 0xf9:
        case 0xfa: // Character codes — skip to render
            g_MessageCurrentPtr = pbVar3 + 1;
msg_skip_char:
            g_MessageCurrentPtr++;
            g_MessageCharTimer = g_MessageCharDelay;
            goto state1_default;
        }

        // Process next character in sequence
        bVar1 = *g_MessageCurrentPtr;
        lineCount = g_MessageLineCounter;
        pbVar3 = g_MessageCurrentPtr;
        goto state1_process_char;

    // === State 2: Waiting with blinking cursor ===
    case 2:
        if ((g_PlayerDpadPressed & 0xC000) != 0) {
            // Button pressed: restart text reveal
            g_MessageStateCounter = 1;
            g_MessagePtr = g_MessageCurrentPtr;
            g_MessageClutBase = g_MessageClutCopy;
            g_MessageCharTimer = (unsigned char)(1 << (g_bGameActive == 0));
            message_render_chars();
            return;
        }
        g_MessageCharTimer = g_MessageCharTimer - 1;
        // Blink cursor every ~24 frames (0x18 = 24)
        if ((((unsigned int)g_MessageCharTimer & (0x18 << (g_bGameActive == 0)))) != 0) {
            // Draw cursor indicator: character-table index 11, the ▼ on row 0.
            // Both builds draw the same glyph, but the row is 18 columns of
            // whatever the font's glyph width is, so the Japanese one lands at
            // 11*14 = 0x9A and is 14 wide (JPN UpdateMessageDisplay 0x00491ac0).
            const int curGlyphW = (GetAssetVersion() != 0) ? 14 : 8;
            g_TextureDesc.flags = 0x40;
            g_TextureDesc.width = curGlyphW;
            g_TextureDesc.height = 14;
            g_TextureDesc.texturePage = 0x1e;
            g_TextureDesc.texU = (unsigned char)(11 * curGlyphW);
            g_TextureDesc.texV = 28;
            g_TextureDesc.clutX = 0x100;
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.screenX = 0x99 - g_ScreenOffsetX;
            g_TextureDesc.screenY = g_MessageScreenY + 0x1e;
            AddTintSprite(&g_TextureDesc, 2);
            message_render_chars();
            return;
        }
        break;

    // === State 3: Post-newline delay ===
    case 3:
        g_MessageCharTimer = g_MessageCharTimer - 1;
        if (g_MessageCharTimer == 0) {
            g_MessageStateCounter = 1;
            g_MessagePtr = g_MessageCurrentPtr;
            g_MessageClutBase = g_MessageClutCopy;
            g_MessageCharTimer = g_MessageCharDelay << (g_bGameActive == 0);
            message_render_chars();
            return;
        }
        break;

    // === State 4: Yes/No prompt ===
    case 4: {
        // Yes/No layout. "Yes  No" is drawn one glyph to the right of the
        // "Yes" cursor, and the "No" cursor sits five glyphs further along, so
        // the whole row scales with the font: 0xD0/0xF8 + text at 0xD8 in the
        // USA build, 0xA0/0xE6 + text at 0xAE in the Japanese one
        // (UpdateMessageDisplay 0x00491ac0 / PrintText8x14 0x00491830).
        const int   ynGlyphW = (GetAssetVersion() != 0) ? 14 : 8;
        const short ynCursorX = (GetAssetVersion() != 0) ? 0xa0 : 208;
        const short ynTextX = ynCursorX + ynGlyphW;

        if ((g_PlayerDpadPressed & 0x4000) == 0) {
            if ((g_PlayerPadHeld & (0x2000 | 0x8000)) != 0) {
                g_menu_choice_id = g_menu_choice_id ^ 1;
                g_MessageCharTimer = 0;
            }
            g_MessageCharTimer = g_MessageCharTimer - 1;
            // Blink cursor
            if ((((unsigned int)g_MessageCharTimer & (0x18 << (g_bGameActive == 0)))) != 0) {
                g_TextureDesc.flags = 0x40;
                if ((g_menu_choice_id & 1) == 0) {
                    screenX = ynCursorX;
                } else {
                    screenX = ynCursorX + 5 * ynGlyphW;
                }
                g_TextureDesc.width = ynGlyphW;
                g_TextureDesc.height = 14;
                // Character-table index 2, the ► on row 0: 2*8 in the USA font,
                // 2*14 = 0x1C in the Japanese one.
                g_TextureDesc.texU = (unsigned char)(2 * ynGlyphW);
                g_TextureDesc.texV = 0x1c;
                g_TextureDesc.texturePage = 0x1e;
                g_TextureDesc.clutX = 0x100;
                g_TextureDesc.clutY = 0x1e0;
                g_TextureDesc.screenX = screenX - g_ScreenOffsetX;
                g_TextureDesc.screenY = g_MessageScreenY + 0x10;

                if ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_CONTROL_ROOM) && (g_roomCameraId == 0x04)) {
                    fade = 0;
                } else {
                    fade = 2;
                }
                AddTintSprite(&g_TextureDesc, fade);
            }
            // Original string at 0x004c0618 is "Yes  No" (TWO spaces): the
            // selection arrow is drawn at 0xd0/0xf8, so "No" must start at
            // 0x100 — with one space the "N" lands at 0xf8 and covers the arrow.
            sprintf(PRINT_TEXT_BUFFER, "Yes  No");
            PrintText8x14(ynTextX, g_ScreenOffsetY + g_MessageScreenY + 16, 0, 0);
            message_render_chars();
            return;
        }

        // Confirm pressed: dismiss message
        g_menu_choice_id = g_menu_choice_id & 0x7f;
        g_message_flags = g_messageFlagsBackup;
        handle_message_post_action();
        return;
    }

    // === State 5: Waiting for player input to dismiss ===
    case 5:
        if ((g_PlayerDpadPressed & 0xC000) != 0) {
            g_menu_choice_id = g_menu_choice_id & 0x7f;
            if ((g_message_flags & 1) == 0) {
                g_PlayerDpadHeld = g_PlayerDpadHeld & 0xf000;
                g_PlayerDpadHeldPrev = g_PlayerDpadHeldPrev & 0xf000;
            }
            g_message_flags = g_messageFlagsBackup;
            return;
        }
        break;

    // === State 6: Auto-dismiss after timeout ===
    case 6:
        g_MessageCharTimer = g_MessageCharTimer - 1;
        if (g_MessageCharTimer == 0) {
            g_menu_choice_id = g_menu_choice_id & 0x7f;
            if ((g_message_flags & 1) == 0) {
                g_PlayerDpadHeld = g_PlayerDpadHeld & 0xf000;
                g_PlayerDpadHeldPrev = g_PlayerDpadHeldPrev & 0xf000;
            }
            g_message_flags = g_messageFlagsBackup;
            return;
        }
        break;
    }

state1_default:
    message_render_chars();
}

void SetSubpixelOffset(int x, int y)
{
    g_SubpixelOffsetX = x;
    g_SubpixelOffsetY = y;
}

void CenterScreenOrigin(void)
{
    SetSubpixelOffset(160, 120);
    g_ScreenOffsetX = 160;
    g_ScreenOffsetY = 120;
}

void setMenuScreenOffset(int w, int h, int x, int y, int mode)
{
    g_ScreenOffsetX = 0;
    g_ScreenOffsetY = 0;
}

// ============================================================================
// clear_textures (0x00470a00)
// Clears texture page entries across all active slots.
// Original: calls TexturePage_DeleteSet for slots 0-11, then
// delete_texture_set_secondary for slots 12-26.
// ============================================================================
void clear_textures(void)
{
    for (int i = 0; i < 4; i++) {
        TexturePage_DeleteSet(i);
    }
    for (int i = 4; i < 12; i++) {
        TexturePage_DeleteSet(i);
    }
    for (int i = 12; i < 27; i++) {
        delete_texture_set_secondary(i);
    }
}

// ============================================================================
// FUN_0046c230 / FUN_0046c280 — Marni execute buffer stubs (return 0)
// ============================================================================
int FUN_0046c230(void* data)
{
    return 0;
}

int FUN_0046c280(int id)
{
    return 0;
}

// ============================================================================
// SetScreenReady (0x00497340)
// Sets the MarniDirect3D screen-ready flag and clears the debug color override.
// Original: writes to CMarniDirect3D field_0x2ec and field_0x2f0
// ============================================================================
void SetScreenReady(int param)
{
    g_MarniScreenReady = param;
    g_MarniScreenColor = 0;
}

// ============================================================================
// SetScreenReadyWithDebugColor (0x00497360)
// Enables screen-ready and sets a packed RGB debug color override.
// Original: writes to CMarniDirect3D field_0x2ec=1 and field_0x2f0=(r<<16|g<<8|b)
// ============================================================================
void SetScreenReadyWithDebugColor(int r, int g, int b)
{
    g_MarniScreenReady = 1;
    g_MarniScreenColor = ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;
}

// ============================================================================
// FUN_00401020 (0x00401020)
// Resets screen offset to center, disables screen-ready, resets subpixel
// params, and rebuilds title background sprites.
// ============================================================================
void ResetScreenAndRebuildSprites(int param)
{
    SetScreenOffset(160, 120);
    SetScreenReady(0);
    Display_SetParams(0, 0);
    FUN_00470a90();
}

// ============================================================================
// StMask (0x00497670)
// Controls frame presentation gating.
//   param_1 != 0 → immediately enable screen present (g_ScreenAccessReady = 1)
//   param_1 == 0 → disable screen present and set countdown timer
// ============================================================================
void StMask(int param_1, int param_2)
{
    if (param_1 != 0) {
        g_ScreenAccessReady = 1;
        return;
    }
    g_ScreenAccessReady = 0;
    g_ScreenAccessCountdown = (char)param_2;
}

// ============================================================================
// CreateTimestampedLogFile - 0x004427a0
// Triggered by PrintScreen (VK_SNAPSHOT). Builds a timestamped .BMP filename
// from the current local time, prepends the install path, and saves the
// current screen to that file. The save is skipped when free disk space on
// the target drive is <= 1 MB.
// Original: calls SaveBitmapToFile on the framebuffer CMarniBits at
// g_pMarniDirect3D + 0x2064 (always populated by software rendering).
// Modern: g_MarniFrameBuffer has m_isValid=1 / m_pPixelData=NULL;
// SaveBitmapToFile captures the D3D11 backbuffer before writing BMP.
// ============================================================================
void CreateTimestampedLogFile(void)
{
    time_t rawTime;
    struct tm* timeInfo;
    char fileName[260];
    char fullPath[MAX_PATH];

    time(&rawTime);
    timeInfo = localtime(&rawTime);
    if (timeInfo == NULL) {
        return;
    }

    // asctime() yields e.g. "Wed Jul 14 20:14:08 2026\n"
    sprintf(fileName, "%s", asctime(timeInfo));

    // Replace ':' with '-' so the timestamp is filesystem-safe
    int len = (int)strlen(fileName);
    for (int i = 0; i < len; i++) {
        if (fileName[i] == ':') {
            fileName[i] = '-';
        }
    }

    // Append the .BMP extension, overwriting the trailing newline
    sprintf(fileName + len - 1, ".BMP");

    // Prepend the install path (g_szInstallPath)
    sprintf(fullPath, "%s%s", g_szInstallPath, fileName);

    // if (GetFreeDiskSpaceMB(fullPath) > 1) {
        g_MarniFrameBuffer.SaveBitmapToFile(fullPath);
    // }
}

// ============================================================================
// FUN_00470a90 (0x00470a90)
// Builds title background sprite commands for the display image.
// In the original, this splits the background into two halves (left/right)
// and inserts them into the ordering table at depths 0xFFE and 0xFFF
// via vtable calls on the CMarniDirect3D object.
// Modern impl: background rendering is already handled by OT_InsertPrimitive
// in FrameRateGovernor via FUN_0040a8f0, so this is a no-op in the modern pipeline.
// ============================================================================
void FUN_00470a90(void)
{
}

// ============================================================================
// Async render-state texture/TMD loaders (moved here from GameState.cpp).
// SCD opcodes 0x18/0x34 hand these a TMD or TIM pointer plus a texture bank /
// depth byte, and they stage it into the render-state page/tmd through
// ExecAsync while the game keeps running.
//
// Helpers defined elsewhere
// ============================================================================
#include "../marni/Marni3DObject.h"
extern int PSXObject_Store(CMarniDirect3DTMD* self, int* tmdHdr, int objIndex,
                           int bankOrTpage, int texRef);   // Marni3DObject.cpp

// (0x00484c40)
static void FUN_00484c40(void)
{
    int tmdData = DAT_008f8c74;
    int bank = DAT_009104c0;
    int depth = DAT_008ffc34;

    BYTE* page = (BYTE*)g_renderStateTex;

    // 0x00484c6c: release the page's previous texture handles
    VideoDriver_ClearState348(page, g_pMarniDirect3D);

    // 0x00484c71: already set up — the flag at +0x348 is what
    // Direct3DTIM_Create also uses for the same purpose.
    if (*(DWORD*)(page + 0x348) == 1) return;

    // 0x00484c86: load the TIM image + CLUT into the page
    ((PSXTexture*)page)->Store((int*)tmdData, 1);

    // 0x00484c8d-0x00484cfb: per-material transparent-colour cleanup. Every
    // palette entry with 5551 bit 15 set has its index zeroed out of the pixel
    // data so those texels sample CLUT 0 (the transparent colour).
    int matCount = *(DWORD*)(page + 0x340);     // m_NumCLUTs
    for (int i = 0; i < matCount; i++) {
        BYTE* mat = page + i * 0x68;
        int* vtable = *(int**)mat;
        void* pixelData = NULL;
        DWORD clutPtr = 0;
        typedef int (*LockFn)(void* self, void** outData, DWORD* outClut);
        typedef int (*UnlockFn)(void* self);
        // vtable[4] = CMarniBits::Lock (0x00403450): outData = m_pPixelData
        // (+0x04), outClut = m_pPalette (+0x08, the CLUT heap copy).
        // The original ignores the result; the guard is port-only defence.
        if (((LockFn)vtable[4])(mat, &pixelData, &clutPtr) != 0) {
            BYTE* clut = (BYTE*)(ULONG_PTR)clutPtr;
            BYTE* px = (BYTE*)pixelData;
            int size = *(DWORD*)(mat + 0x2c) * *(DWORD*)(mat + 0x30);
            for (int clutIdx = 0; clutIdx < 0x100; clutIdx++) {
                if ((clut[clutIdx * 2 + 1] & 0x80) != 0) {   // 5551 bit 15
                    for (int p = 0; p < size; p++) {
                        if (px[p] == clutIdx) px[p] = 0;
                    }
                }
            }
        }
        ((UnlockFn)vtable[5])(mat);
    }

    // 0x00484cfd-0x00484d77: patch each material's CLUT descriptor (the same
    // +0x54/+0x58/+0x5C/+0x60 layout PSXObject_Store matches against) and
    // create the D3D texture handle for it.
    for (int i = 0; i < matCount; i++) {
        BYTE* mat = page + i * 0x68;
        *(DWORD*)(mat + 0x54) = 0;
        *(DWORD*)(mat + 0x58) = depth + 0x1e0;
        *(DWORD*)(mat + 0x5c) = (bank & 0xf) << 6;
        *(DWORD*)(mat + 0x60) = (bank & 0x10) << 4;
        void** d3dVtable = *(void***)g_pMarniDirect3D;
        typedef DWORD (*CreateTextureFn)(void*, BYTE*, int, int);
        CreateTextureFn createTex = (CreateTextureFn)d3dVtable[6];
        DWORD handle = createTex(g_pMarniDirect3D, mat, 0x21, 0);
        *(DWORD*)(page + 0x34C + i * 4) = handle;
    }

    *(DWORD*)(page + 0x348) = 1;
}

// (0x00484dc0)
static void FUN_00484dc0(void)
{
    int* tmdHdr = (int*)DAT_00aae740;
    int bank = DAT_00aad6ec;

    CMarniDirect3DTMD* tmd = (CMarniDirect3DTMD*)g_renderStateTMD;

    // 0x00484dd9: clean the slot before re-storing
    tmd->CleanupObjects(g_pMarniDirect3D);

    // 0x00484dee: parse the TMD geometry (texRef 0x80 = the page's UV divisor)
    PSXObject_Store(tmd, tmdHdr, 0, bank, 0x80);

    // 0x00484e05: bind the render-state texture page
    tmd->Create(g_pMarniDirect3D, g_renderStateTex, (void*)1);

    // 0x00484e0c-0x00484e2f: mark every embedded object's transparency flag
    // (stride 0x108: each m_objectData entry and its copy, +0 and +0x84)
    int count = *(int*)((BYTE*)tmd + 0x4C0);    // m_objectCount
    for (int i = 0; i < count; i++) {
        *(DWORD*)((BYTE*)tmd + 0x550 + i * 0x108) |= 2;
        *(DWORD*)((BYTE*)tmd + 0x550 + i * 0x108 + 0x84) |= 2;
    }
}

// (0x00484d90)
void FUN_00484d90(int param1, unsigned char param2, unsigned char param3)
{
    DAT_008f8c74 = param1;
    DAT_009104c0 = param2;
    DAT_008ffc34 = param3;
    ExecAsync((void*)FUN_00484c40);
}

// (0x00484e40)
void FUN_00484e40(int param1, unsigned char param2, unsigned char param3)
{
    DAT_00aae740 = param1;
    DAT_00aad6ec = param2;
    DAT_00ac34f8 = param3;
    ExecAsync((void*)FUN_00484dc0);
}
