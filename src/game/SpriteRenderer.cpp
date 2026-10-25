#include "SpriteRenderer.h"
#include "../Globals.h"
#include "../DebugPrint.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "../marni/MarniBits.h"
#include "TmdRenderer.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cmath>

// ============================================================================
// Global variables
// ============================================================================
TextureDraw g_SpriteCommandBuffer[MAX_SPRITE_COMMANDS];
OTEntry g_OT[MAX_OT_ENTRIES];
int g_RenderBufferIndex      = 0;
int g_RenderDisableFlags     = 0;
int g_SubpixelOffsetX        = 0;   // 0x004d2bd0
int g_SubpixelOffsetY        = 0;   // 0x004d2bd4
int g_displayImageOriginX    = 0;   // 0x004c335c - display image origin (FUN_00470a90)
int g_displayImageOriginY    = 0;   // 0x004c3360
int g_MaxFadeValue           = 4095;
int g_DepthSortOverride      = 0;
float g_ColorScaleFactor     = 2.0f / 255.0f;
int g_nFadeInverted          = 0;   // 0x004c333c
int g_renderPrimCount        = 0;   // 0x004c2d10

// Page width factors indexed by flags bits 22-23 (0x004c2d78)
static const int g_PageWidthFactor[4] = { 1, 2, 4, 8 };
// g_dwTexVariantBlend (0x004c2d64) used to be mirrored here as
// `{ 0, 1, 2, 4, 8 }`, which is not what is in the exe - the real bytes are
// { 0, 0x80, 0x80, 0, 0x80 }. It now lives in SpriteVariantAlpha()
// (SpriteRenderer.h) together with the 1/256 scale the producers apply to it.

// ============================================================================
// BuildSpriteRenderFlags (0x0046d960)
// ============================================================================
void BuildSpriteRenderFlags(unsigned int textureFlags, unsigned int* outFlags) {
    unsigned int flags = 0;
    if (textureFlags & TEXDESC_MIRROR_V) flags |= SPRITE_FLAG_MIRROR_V;
    if (textureFlags & TEXDESC_MIRROR_U) flags |= SPRITE_FLAG_MIRROR_U;
    *outFlags = flags;
}

// ============================================================================
// GetTextureVariant (0x0046d940)
// Returns 0 when the descriptor carries no variant, else the 2-bit variant
// field biased by 1 so that "variant 0" is distinguishable from "none".
// ============================================================================
int GetTextureVariant(unsigned int textureFlags) {
    if (textureFlags & TEXDESC_VARIANT_ENABLE) {
        return (int)((textureFlags & TEXDESC_VARIANT_MASK) >> 28) + 1;
    }
    return 0;
}

// ============================================================================
// SpriteQueue_Reset (0x0046d990)
// ============================================================================
void SpriteQueue_Reset(void) {
    g_SpriteQueueCount = 0;
    g_OTIndex = 0;
    g_renderPrimCount = 0;
}

// ============================================================================
// FlushSpriteCommandsRange
// Converts TextureDraw entries whose depthSort falls in [minDepth, maxDepth)
// to D3D11 draw calls at the current display resolution. The PS1 ordering
// table (OT) is emulated with a single stable sort inside
// FlushSpriteCommands, so the range split preserves the global order:
// the death screen's base died.tim image (depthSort 0x10194) is flushed
// before the 3D TMD pass and the wavy strip (660) after it.
//
// classMask selects which sprite classes take part. The 2D passes ask for
// SPRITE_CLASS_NORMAL only: room masks carry a view-space Z in depthSort and
// are drawn by FlushTmdObjects, interleaved with the entity triangles, so
// letting them out here as well would draw them twice - and on top of the
// player, which is the bug this split exists to fix.
// ============================================================================
void FlushSpriteCommandsRange(unsigned int minDepth, unsigned int maxDepth,
                              unsigned int classMask)
{
    if (g_SpriteQueueCount == 0) {
        return;
    }

    // 0x0046d...: Emulate the PS1 ordering table (OT). The original GPU linked
    // primitives by Z-depth so that a LOWER depthSort value rendered ON TOP
    // (nearer the camera). Without this, sprites are drawn in submission order,
    // which makes later-drawn frames occlude the icons/textures they should sit
    // behind (e.g. the equipped-weapon frame covering the weapon texture).
    // Stable sort so equal-depth draws keep their submission order. The two
    // range flushes (FrameRateGovernor) each re-sort the untouched queue, so
    // both halves come out in the same global order.
    std::stable_sort(
        g_SpriteCommandBuffer,
        g_SpriteCommandBuffer + g_SpriteQueueCount,
        [](const TextureDraw& a, const TextureDraw& b) {
            return a.depthSort > b.depthSort;
        });

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    (void)pD3D;
    // Scale PS1-space coords to the backbuffer: physical/logical, exactly how
    // the original Marni layer scaled primitives at draw time (FUN_0042ba60).
    float scaleX, scaleY;
    MarniGetRenderScale(&scaleX, &scaleY);

    for (int i = 0; i < g_SpriteQueueCount; i++) {
        TextureDraw* cmd = &g_SpriteCommandBuffer[i];
        if (cmd->depthSort < minDepth || cmd->depthSort >= maxDepth) continue;
        if ((cmd->sortClass & classMask) == 0) continue;

        // Line primitives (type 11): used by the menu EKG health bar.
        // The original FUN_00470c60 built a line primitive and inserted it
        // into the ordering table with depthSort = depth*16 + 500, exactly
        // like draw_texture, so it participates in the same OT sort.
        if (cmd->type == 11) {
            float sx0 = (float)cmd->x0 * scaleX;
            float sy0 = (float)cmd->y0 * scaleY;
            float sx1 = (float)cmd->x1 * scaleX;
            float sy1 = (float)cmd->y1 * scaleY;

            int cr = (int)(cmd->r * 255.0f);
            int cg = (int)(cmd->g * 255.0f);
            int cb = (int)(cmd->b * 255.0f);
            if (cr > 255) cr = 255; if (cr < 0) cr = 0;
            if (cg > 255) cg = 255; if (cg < 0) cg = 0;
            if (cb > 255) cb = 255; if (cb < 0) cb = 0;
            float alpha = cmd->alpha;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            int ca = (int)(alpha * 255.0f);
            if (ca > 255) ca = 255; if (ca < 0) ca = 0;
            DWORD color = ((DWORD)ca << 24) | ((DWORD)cr << 16) | ((DWORD)cg << 8) | (DWORD)cb;
            MarniDrawLine(sx0, sy0, sx1, sy1, 1.0f, color);
            continue;
        }
        if (cmd->type == 12) {
            // 4-corner textured quad (ground shadows / death blood pool).
            // The original rendered the quad through a Marni viewport, so its
            // footprint follows the rotated quad exactly; an axis-aligned
            // sprite can't (its bbox oscillates with the entity angle and
            // over-stretches the texture), so the clipped projected polygon
            // is drawn as two triangles with per-corner UVs.
            int texSlot = cmd->extraFlags;
            MarniHandle srv = MARNI_NULL_HANDLE;
            float pageW = 0.0f;
            float pageH = 0.0f;
            if (texSlot >= 0 && texSlot < 256) {
                srv = g_TexturePageSRV[texSlot];
                if (g_TexturePageWidth[texSlot] > 0)  pageW = (float)g_TexturePageWidth[texSlot];
                if (g_TexturePageHeight[texSlot] > 0) pageH = (float)g_TexturePageHeight[texSlot];
            }
            // No second chance at the page: the field that used to be read here
            // as a fallback slot is the semi-transparency level, not a slot
            // index (see TextureDraw::variantAlpha), so this only ever resolved
            // slot 0 - never a real page.
            if (srv == MARNI_NULL_HANDLE) {
                continue;
            }
            if (pageW <= 0.0f || pageH <= 0.0f) {
                continue;
            }

            int cr = (int)(cmd->r * 255.0f);
            int cg = (int)(cmd->g * 255.0f);
            int cb = (int)(cmd->b * 255.0f);
            if (cr > 255) cr = 255; if (cr < 0) cr = 0;
            if (cg > 255) cg = 255; if (cg < 0) cg = 0;
            if (cb > 255) cb = 255; if (cb < 0) cb = 0;
            float alpha = cmd->alpha;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            int ca = (int)(alpha * 255.0f);
            if (ca > 255) ca = 255; if (ca < 0) ca = 0;

            // Two triangles, drawn PERSPECTIVE-CORRECT: the per-corner
            // view-space z goes in as w, so the UVs interpolate with the
            // perspective divide the way the original's D3D7 hardware did.
            //
            // The per-corner view z ALSO becomes real NDC depth and the quad
            // is drawn with a test-only depth state. The original rendered
            // this poly as a world-space viewport quad against the same
            // Z-buffer as the TMD objects, so a model in front clipped it no
            // matter where its ordering-table key landed - which matters,
            // because the placement records with flag != 0 (0x00456f6d:
            // alpha = forceAlpha + 1) collapse that key to a near-constant
            // ~400 and would otherwise sort the shadow NEARER THAN EVERYTHING
            // in the painter walk (rendered above every model and room mask;
            // most visible in the stage-3 lab cameras, which all use those
            // records). The ramp must be TmdViewZToNdc, i.e. exactly the one
            // DrawTriangles3D writes, or the comparison is meaningless.
            //
            // The fan MUST be (0,1,2) + (0,2,3). DrawFadeSpr's near-plane
            // clipper walks the quad's edges in ring order (0->1->3->2) and
            // emits its vertices in that sequence, so cmd->x0..x3 are a CYCLIC
            // polygon, not the TL/TR/BL/BR strip that a (0,1,2)+(2,1,3) fan
            // assumes. Fanning a ring that way splits the first triangle along
            // the 0-2 diagonal and the second along the 1-3 diagonal: the two
            // halves then OVERLAP down the middle (drawn twice, so twice as
            // dark) and leave a sliver at each end uncovered. That is the
            // long-running "the oval is cut in half and both halves are
            // rotated and overlap" artifact - it was never the projection or
            // the texture.
            const short cxy[4][2] = {
                { cmd->x0, cmd->y0 }, { cmd->x1, cmd->y1 },
                { cmd->x2, cmd->y2 }, { cmd->x3, cmd->y3 },
            };
            const short cuv[4][2] = {
                { cmd->u0, cmd->v0 }, { cmd->u1, cmd->v1 },
                { cmd->u2, cmd->v2 }, { cmd->u3, cmd->v3 },
            };
            const short cwz[4] = { cmd->wz0, cmd->wz1, cmd->wz2, cmd->wz3 };
            float verts[6][10];
            const int idx[6] = { 0, 1, 2, 0, 2, 3 };
            for (int t = 0; t < 6; t++) {
                int k = idx[t];
                verts[t][0] = (float)cxy[k][0] * scaleX;
                verts[t][1] = (float)cxy[k][1] * scaleY;
                verts[t][2] = TmdViewZToNdc((float)cwz[k]);     // NDC z (depth test)
                verts[t][3] = (float)cwz[k];                   // view z = w
                verts[t][4] = (float)cuv[k][0] * (1.0f / 4096.0f);
                verts[t][5] = (float)cuv[k][1] * (1.0f / 4096.0f);
                verts[t][6] = (float)cr * (1.0f / 255.0f);
                verts[t][7] = (float)cg * (1.0f / 255.0f);
                verts[t][8] = (float)cb * (1.0f / 255.0f);
                verts[t][9] = (float)ca * (1.0f / 255.0f);
            }
            MarniDrawTrianglesPersp((const float*)verts, 2, srv, TRUE);
            continue;
        }
        if (cmd->type != 10) continue;

        // Snap the quad to whole PIXEL bounds before drawing. Game-space
        // coordinates are integers, but the render scale (physical/logical)
        // is fractional on a native-resolution fullscreen backbuffer (e.g.
        // 4.5x vertical for 240->1080). Scaling adjacent mask strips
        // independently leaves their shared edges on half-pixels, and point
        // sampling then shows a hairline seam of the neighbouring texel row
        // (the full-width horizontal lines across the room background).
        // Rounding the two edges of every quad to the same integer pixel
        // lines makes abutting strips agree exactly - their shared boundary
        // rounds to the same value because it is the same game coordinate.
        float x0s = (float)cmd->x0 * scaleX;
        float y0s = (float)cmd->y0 * scaleY;
        float x1s = (float)(cmd->x1 + 1) * scaleX;
        float y1s = (float)(cmd->y1 + 1) * scaleY;
        float x = (float)floor(x0s + 0.5f);
        float y = (float)floor(y0s + 0.5f);
        float w = (float)floor(x1s + 0.5f) - x;
        float h = (float)floor(y1s + 0.5f) - y;
        if (w < 1.0f) w = 1.0f;
        if (h < 1.0f) h = 1.0f;

        int cr = (int)(cmd->r * 255.0f);
        int cg = (int)(cmd->g * 255.0f);
        int cb = (int)(cmd->b * 255.0f);
        if (cr > 255) cr = 255; if (cr < 0) cr = 0;
        if (cg > 255) cg = 255; if (cg < 0) cg = 0;
        if (cb > 255) cb = 255; if (cb < 0) cb = 0;

        float alpha = cmd->alpha;
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        int ca = (int)(alpha * 255.0f);
        if (ca > 255) ca = 255; if (ca < 0) ca = 0;
        DWORD color = (ca << 24) | (cr << 16) | (cg << 8) | cb;

        int texSlot = cmd->extraFlags;
        MarniHandle srv = MARNI_NULL_HANDLE;
        // pageW/pageH must come from g_TexturePageWidth/Height, which every
        // SRV-creating path sets. A 0 here means the slot's SRV metadata was
        // lost (or never set); we must NOT fall back to 256, because that
        // would under-sample a smaller SRV and render a tiny sub-region
        // stretched across the sprite (the statface/blue/staitem 8x8 bug).
        float pageW = 0.0f;
        float pageH = 0.0f;
        if (texSlot >= 0 && texSlot < 256) {
            srv = g_TexturePageSRV[texSlot];
            if (g_TexturePageWidth[texSlot] > 0)  pageW = (float)g_TexturePageWidth[texSlot];
            if (g_TexturePageHeight[texSlot] > 0) pageH = (float)g_TexturePageHeight[texSlot];
        }
        // See the note in the type-12 branch: +0x2c is not a page slot.
        if (srv == MARNI_NULL_HANDLE) {
            continue;
        }
        if (pageW <= 0.0f || pageH <= 0.0f) {
            // SRV exists but its dimensions are unknown — can't normalize UVs.
            continue;
        }

        // UV normalization: cmd->u0/v0 are pixel offsets within the SRV,
        // cmd->u1/v1 are inclusive pixel endpoints. The original DX5
        // draw code adds +1 to the endpoint before normalizing, converting
        // from inclusive to exclusive range. D3D11 point sampling with
        // pixel-center interpolation correctly samples the full range when
        // endpoint is converted by +1 (no half-texel offset needed).
        // Mirror flags 0x10(X)/0x20(Y) are emulated by swapping UVs.
        float u0 = (float)cmd->u0 / pageW;
        float v0 = (float)cmd->v0 / pageH;
        float u1 = (float)(cmd->u1 + 1) / pageW;
        float v1 = (float)(cmd->v1 + 1) / pageH;

        // Emulate PS1 texture flip (0x10=X, 0x20=Y)
        if (cmd->spriteFlags & SPRITE_FLAG_MIRROR_U) { float t = u0; u0 = u1; u1 = t; }
        if (cmd->spriteFlags & SPRITE_FLAG_MIRROR_V) { float t = v0; v0 = v1; v1 = t; }

        MarniDrawSprite(x, y, w, h, u0, v0, u1, v1, color, srv);
    }
}

// ============================================================================
// SpriteQueue_CollectSceneDepths
// The distinct depthSort values of the queued scene primitives, far to near.
// Masks commonly share a depth (one overlay group is split into several tiles)
// and a clipped shadow is submitted as two commands at one depth, so collapsing
// duplicates keeps the number of interleave points - and therefore the number
// of range flushes FlushTmdObjects has to issue - small.
// ============================================================================
int SpriteQueue_CollectSceneDepths(unsigned int* out, int maxOut)
{
    if (out == NULL || maxOut <= 0) return 0;

    int n = 0;
    for (int i = 0; i < g_SpriteQueueCount && n < maxOut; i++) {
        if ((g_SpriteCommandBuffer[i].sortClass & SPRITE_CLASS_SCENE) == 0) continue;
        out[n++] = g_SpriteCommandBuffer[i].depthSort;
    }
    if (n == 0) return 0;

    std::sort(out, out + n, [](unsigned int a, unsigned int b) { return a > b; });
    return (int)(std::unique(out, out + n) - out);
}

// ============================================================================
// FlushSpriteCommands
// Sorts the full queue like the original OT (lower depthSort = nearer, drawn
// last), then flushes everything. FrameRateGovernor splits the flush around
// the 3D TMD pass at depth 0x1000 instead (see Rendering.cpp) so the death
// screen's background image sorts behind the entities.
// ============================================================================
void FlushSpriteCommands(void) {

    FlushSpriteCommandsRange(0, 0xFFFFFFFFu, SPRITE_CLASS_ALL);
    g_SpriteQueueCount = 0;
}
int draw_texture(TextureDesc* texture, unsigned short depth) {
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return 0;

    // BPP scale = g_dwTexScaleFactors[(flags>>24)&3] = {4,2,1,1}
    static const int s_TexScale[4] = { 4, 2, 1, 1 };
    int scale = s_TexScale[(texture->flags >> 24) & 3];

    // VRAM-space texture position (from the tpage code, texU, texV)
    unsigned int vAdd = 0;
    unsigned int p    = texture->texturePage;
    if (p > 16) { vAdd = 256; p -= 16; }
    int texUWords = (int)(p * 0x40u + texture->texU / scale);
    int texVAbs   = (int)(vAdd + texture->texV);

    // Search page descriptors from slot 0xF upward. The original (0x0046e410)
    // scans the whole descriptor table and requires the found slot <= 0x2D;
    // the previous port version skipped the search and always sampled texture
    // slot 0 (the font), so every draw_texture call (menu cursor, itembox
    // frame borders, health-bar pieces) rendered the wrong texture or nothing.
    int foundSlot    = -1;
    int foundOriginX = 0, foundOriginY = 0;
    int foundPage   = 0;

    for (int cur = 0xF; cur <= 0x2D; cur++) {
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

        int texR = texUWords + texture->width / scale;
        int texB = texVAbs   + texture->height;

        if (pageL <= texUWords && texR <= pageR &&
            pageT <= texVAbs   && texB <= pageB) {
            foundSlot    = cur;
            foundOriginX = oX;
            foundOriginY = oY;
            foundPage   = d;
            break;
        }
    }
    if (foundSlot < 0) return 0;

    // CLUT lookup: uVar4 = clutY - clutBase; ==8 → 1
    int clutIdx = (int)texture->clutY - g_TexturePageClutBase[foundSlot];
    if (clutIdx < 0 || clutIdx > 7) return 0;
    if (clutIdx == 8) clutIdx = 1;

    // UV computation (page-relative PIXEL units):
    int pageOfs = ((int)texture->texturePage - foundPage) * scale * 0x40;
    int su0 = (int)texture->texU - foundOriginX * scale + pageOfs;
    int sv0 = (int)texture->texV - foundOriginY;
    int su1 = su0 + texture->width  - 1;
    int sv1 = sv0 + texture->height - 1;

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = SPRITE_CLASS_NORMAL;

    cmd->spriteFlags = SpriteBuildFlags(texture->flags);

    cmd->r = (float)texture->colorMulR * g_ColorScaleFactor;
    cmd->g = (float)texture->colorMulG * g_ColorScaleFactor;
    cmd->b = (float)texture->colorMulB * g_ColorScaleFactor;

    cmd->variantAlpha = SpriteVariantAlpha(texture->flags);
    cmd->alpha        = SpriteDrawAlpha(cmd->variantAlpha);

    short sx = texture->screenX + g_ScreenOffsetX;
    short sy = texture->screenY + g_ScreenOffsetY;
    cmd->x0 = sx - texture->pivotX;
    cmd->y0 = sy - texture->pivotY;
    cmd->x1 = (texture->width - texture->pivotX) + sx - 1;
    cmd->y1 = (texture->height - texture->pivotY) + sy - 1;

    cmd->depthSort = (unsigned int)depth * 16 + 500;

    cmd->u0 = (unsigned short)(su0 >= 0 ? su0 : 0);
    cmd->v0 = (unsigned short)(sv0 >= 0 ? sv0 : 0);
    cmd->u1 = (unsigned short)(su1 >= 0 ? su1 : 0);
    cmd->v1 = (unsigned short)(sv1 >= 0 ? sv1 : 0);
    cmd->extraFlags = foundSlot;

    g_SpriteQueueCount++;
    return 1;
}

// ============================================================================
// SubmitLine (0x00470c60 helper)
// Queues a 1px line primitive (type 11) into the sprite command buffer.
// Coordinates are in PS1 game space (with screen offset applied by the
// caller, matching how draw_texture handles screenX/screenY).
// ============================================================================
int SubmitLine(short x0, short y0, short x1, short y1, unsigned short depth,
               float r, float g, float b, float alpha)
{
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return 0;

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 11;
    cmd->sortClass = SPRITE_CLASS_NORMAL;
    cmd->x0 = x0;
    cmd->y0 = y0;
    cmd->x1 = x1;
    cmd->y1 = y1;
    cmd->depthSort = (unsigned int)depth * 16 + 500;
    cmd->spriteFlags = 0;
    cmd->alpha = alpha;
    cmd->r = r;
    cmd->g = g;
    cmd->b = b;
    cmd->variantAlpha = 0.0f;
    cmd->extraFlags = 0;
    cmd->u0 = 0;
    cmd->v0 = 0;
    cmd->u1 = 0;
    cmd->v1 = 0;

    g_SpriteQueueCount++;
    return 1;
}

// ============================================================================
// AddSprite (0x0046ddc0)
// ============================================================================
int AddSprite(TextureDesc* texture, short depth, int tpage, int fade) {
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return 0;

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    // DrawRoomSpr is AddSprite's only caller (the original has exactly four
    // call sites, all inside it), so every command built here is a room mask
    // and its depthSort is a scene Z rather than a 2D layer.
    cmd->sortClass = SPRITE_CLASS_ROOMMASK;

    cmd->spriteFlags = SpriteBuildFlags(texture->flags);
    cmd->alpha = 1.0f;

    cmd->r = 1.0f;
    cmd->g = 1.0f;
    cmd->b = 1.0f;
    // Room masks are always opaque - AddSprite (0x0046ddc0) writes a literal 0
    // to +0x2c and never consults the descriptor's variant.
    cmd->variantAlpha = 0.0f;

    short sx = texture->screenX + g_ScreenOffsetX;
    short sy = texture->screenY + g_ScreenOffsetY;
    cmd->x0 = sx - texture->pivotX;
    cmd->y0 = sy - texture->pivotY;
    cmd->x1 = (texture->width - texture->pivotX) + sx - 1;
    cmd->y1 = (texture->height - texture->pivotY) + sy - 1;

    cmd->depthSort = (depth == 0) ? 550 : (fade * 16);

    // Room sprites use PS1 VRAM-page coordinates. The original AddSprite
    // combines the descriptor's texture depth with the local U coordinate and
    // moves pages above depth 0x10 into the next 0x100-pixel V band. Using the
    // raw descriptor U/V values samples unrelated art from the room page.
    static const int pageWidthFactor[4] = { 1, 2, 4, 8 };
    const int factor = pageWidthFactor[(texture->flags & 0x03000000) >> 24];
    unsigned short pageCode = (unsigned short)texture->texturePage;
    short vPageOffset = 0;
    if (pageCode > 0x10) {
        pageCode = (unsigned short)(pageCode - 0x10);
        vPageOffset = 0x100;
    }

    // The original tpage+4 selects the legacy texture-set metadata. The port
    // stores the resulting D3D room-mask SRV at the direct tpage slot.
    const int textureSlot = tpage;
    const short pageOriginX = (textureSlot >= 0 && textureSlot < 256)
        ? g_TexturePageOriginX[textureSlot] : 0;
    const short pageOriginY = (textureSlot >= 0 && textureSlot < 256)
        ? g_TexturePageOriginY[textureSlot] : 0;
    cmd->u0 = (unsigned short)(factor *
        ((int)pageCode * 0x40 + ((unsigned int)texture->texU / factor) - pageOriginX));
    cmd->v0 = (unsigned short)(vPageOffset + texture->texV - pageOriginY);
    cmd->u1 = cmd->u0 + texture->width - 1;
    cmd->v1 = cmd->v0 + texture->height - 1;

    cmd->extraFlags = textureSlot;

    g_SpriteQueueCount++;
    return 1;
}

// ============================================================================
// SubmitEffectSprite (0x0046d9b0)
//
// The 2D billboard effects - water, steam, fire, muzzle flashes, blood. These
// are SCENE primitives, not overlays: the original hands them to the ordering
// table through the same MarniDirect3D vtable+0x28 insert that AddSprite and
// AddSprite_Ex use, so an effect interleaves with the entity geometry by depth.
//
// The port had them as SPRITE_CLASS_NORMAL, which meant they were flushed in
// the 2D pass that runs entirely AFTER FlushTmdObjects - so every effect
// painted over every model no matter where it was in the room. They are now
// SPRITE_CLASS_EFFECT and take part in FlushTmdObjects' far-to-near walk,
// exactly like room masks and ground shadows.
//
// depthSort keeps the original's `depth * 0x40 - scaleY` unchanged, and the
// 0x40 is NOT a mistake even though every other producer scales its key by 16.
// Work the units through:
//
//   ProjectEffectSprite (0x0040aa50)  returns  viewZ >> 2   <- the easy one to
//   eff->projDepth                    =        viewZ / 4       miss
//   effect_submit_sprite's depthArg   =        viewZ / 64   (a further >> 4)
//   depth * 0x40                      =        viewZ        <- view-space Z
//
// So this lands in exactly the same units as a TMD triangle's view Z and as a
// room mask's `fade << 4`, which is what makes the interleave in FlushTmdObjects
// meaningful. `scaleY` is g_EffectLightRecords[rec][1], 0 in all seven records.
//
// The ordering-table key the original passes to the vtable+0x28 insert is a
// different, coarser quantity - clamp(depth - scaleX, 0, 0xfff) then minus
// brightness and g_DepthFadeBias (0x004c3350: .rdata, one xref, 0). It is one
// OT bucket per 64 view units here versus one per 16 for masks. The port sorts
// on the field, not the bucket, so it is deliberately not reproduced; rewriting
// depthSort as otKey*16 makes every effect sort four times too near and puts
// them back on top of everything, which is the bug this comment exists to stop
// someone re-introducing.
// ============================================================================
int SubmitEffectSprite(TextureDesc* texture, int depth, int textureId,
                       unsigned char r, unsigned char g, unsigned char b,
                       int scaleX, int scaleY, int blendMode, short brightness) {
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return 0;

    // Both only feed the OT bucket, which the port does not use.
    (void)scaleX; (void)brightness;

    bool useSubpixel = !(texture->scaleX == 0x1000 && texture->scaleY == 0x1000);

    short sx, sy;
    if (useSubpixel) {
        sx = texture->screenX + (short)g_SubpixelOffsetX;
        sy = texture->screenY + (short)g_SubpixelOffsetY;
    } else {
        sx = texture->screenX + g_ScreenOffsetX;
        sy = texture->screenY + g_ScreenOffsetY;
    }

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = SPRITE_CLASS_EFFECT;

    cmd->spriteFlags = SpriteBuildFlags(texture->flags);

    // cmd->r/g/b are a 0..1 MULTIPLIER - FlushSpriteCommands does
    // `(int)(cmd->r * 255.0f)` and clamps. draw_texture sets the convention:
    // colorMulR alone (0x80, PS1-neutral) gives 128 * 2/255 = 1.004, i.e. 1.0.
    // The per-effect tint arrives here as 0..255, so it has to be normalised
    // too. Without the /255 every effect sprite came out at 153.6 -> clamped to
    // white, so the tint was discarded entirely and each sprite rendered with
    // its raw texture colour. The gore frames are stored near-black, so blood
    // splatter drew dark grey; sprites that were already warm in the sheet
    // happened to look right, which is why only "some" were grey.
    const float tintNorm = g_ColorScaleFactor * (1.0f / 255.0f);
    cmd->r = (float)r * (float)texture->colorMulR * tintNorm;
    cmd->g = (float)g * (float)texture->colorMulG * tintNorm;
    cmd->b = (float)b * (float)texture->colorMulB * tintNorm;

    // blendMode is this producer's semi-transparency level: 0x0046dbae is
    // `fild [esp+0x40]` * the 1/256 at 0x004af29c, stored as a FLOAT into the
    // +0x2c field (see TextureDraw::variantAlpha). It is NOT scaled by
    // g_ColorScaleFactor (1/128) - that constant belongs to r/g/b, and using it
    // here turned the usual 0x80 into a 1.0 instead of a 0.5.
    cmd->variantAlpha = (float)blendMode * 0.00390625f;
    cmd->alpha        = SpriteDrawAlpha(cmd->variantAlpha);
    cmd->extraFlags = textureId;

    if (useSubpixel) {
        int iVar4 = (int)texture->pivotX * (int)texture->scaleX;
        cmd->x0 = sx - (short)((iVar4 + (iVar4 >> 0x1F & 0xFFF)) >> 12);
        iVar4 = (int)texture->pivotY * (int)texture->scaleY;
        cmd->y0 = sy - (short)((iVar4 + (iVar4 >> 0x1F & 0xFFF)) >> 12);
        iVar4 = ((unsigned int)texture->width - (int)texture->pivotX) * (int)texture->scaleX;
        cmd->x1 = (short)((iVar4 + (iVar4 >> 0x1F & 0xFFF)) >> 12) + sx;
        iVar4 = ((unsigned int)texture->height - (int)texture->pivotY) * (int)texture->scaleY;
        cmd->y1 = (short)((iVar4 + (iVar4 >> 0x1F & 0xFFF)) >> 12) + sy;
        if (cmd->x0 < cmd->x1) cmd->x1 = cmd->x1 - 1;
        if (cmd->y1 > cmd->y0) cmd->y1 = cmd->y1 - 1;
    } else {
        cmd->x0 = sx - texture->pivotX;
        cmd->y0 = sy - texture->pivotY;
        cmd->x1 = (texture->width - texture->pivotX) + sx - 1;
        cmd->y1 = (texture->height - texture->pivotY) + sy - 1;
    }

    // 0x0046dcdd, verbatim. This IS view-space Z - see the unit chain in the
    // header comment. Do not "normalise" the 0x40 to the 16 the other producers
    // use; the factor of four is what cancels ProjectEffectSprite's `>> 2`.
    cmd->depthSort = ((unsigned int)(depth & 0xFFFF)) * 0x40 - scaleY;

    cmd->u0 = (unsigned short)texture->texU;
    cmd->v0 = (unsigned short)texture->texV;
    cmd->u1 = cmd->u0 + texture->width - 1;
    cmd->v1 = cmd->v0 + texture->height - 1;

    g_SpriteQueueCount++;
    return 1;
}

// ============================================================================
// AddFadePoly (0x0046fea0) - simplified for D3D11 port
//
// The original built a 4-vertex Marni OT primitive carrying the shadow's
// composed matrix (columns + translation in 4.12 fixed point), the viewport
// and texture-page handles, and inserted it with the fade value as the OT
// depth; the hardware rendered the quad through the viewport, so its
// footprint followed the rotated quad exactly. The port draws the same quad
// as TWO textured triangles over the clipped projected polygon: DrawFadeSpr
// clips the world-space corners against the near plane, projects them, and
// passes the 3..5-corner polygon (px/py, PS1 screen coords) with per-corner
// UVs (cu/cv, 0..4096 over the whole kage page) here. `alpha` is the OT depth
// and nothing else: it becomes the sprite's depthSort so farther shadows draw
// first (see the note on the vertex alpha below).
//
// `rgb` is the quad's primitive-header tint (0x00808080 shadow grey,
// 0x00FFFF50 blood-pool red). The original's byte conversion is b/256 below
// 0x80 and (256-b)/256 above it, with an all-equal special case that
// collapses to near-black (0x3b83126f == 0.004f): the grey shadow hits the
// special case and multiplies the texture to black, the blood pool's
// (0x50, 0xFF, 0xFF) comes out (0.31, 0.004, 0.004) dark red.
//
// That tint is the WHOLE colour signal: AddFadePoly stores 6 in the OT
// record's flag word (+0x80, bit 1 set), which sends FUN_00446e40 down its
// direct path - vertex diffuse = tint * 127, no light accumulation - and the
// kage page only supplies coverage through its alpha. So this tint must reach
// the blend undiluted; LoadShadowMaskTexture bakes the SRV texels white for
// exactly that reason. Multiplying the old grey texels into it here turned
// the blood pool into a 4%-red wash that read as grey.
// ============================================================================
// Per-frame record of the fade polys already inserted, mirroring the original's
// g_OTFadeTbl (0x008ed430) plus the translation Z it reads back out of each OT
// record. Both are only ever read at indices below g_OTIndex, which
// SpriteQueue_Reset zeroes each frame, so they need no clearing of their own.
// The bound is the original's OT capacity (0x20 entries per render buffer);
// DrawFadeSpr can queue at most FADE_SPR_MAX = 32 shadows anyway.
#define FADE_OT_MAX  32
static int            g_FadeOtZ[FADE_OT_MAX];
static unsigned short g_FadeOtKey[FADE_OT_MAX];

int AddFadePoly(unsigned short alpha, int transZ, int sortZ, int tpage,
                unsigned char* rgb, const int* px, const int* py,
                const int* wz, const int* cu, const int* cv, int count) {
    // A clipped convex quad has 3..5 corners; a TextureDraw carries 4, so the
    // pentagon (one corner behind the camera) is split into (0,1,2,3) plus
    // the triangle (0,3,4). Fewer than 4 corners repeat the last one.
    int cmds = (count > 4) ? 2 : 1;
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - cmds) return 0;

    // 0x00470100: the FIRST ordering-table key is (alpha >> 2) - g_OTIndex,
    // floored at 0 and capped at 0xfff. g_OTIndex counts the fade polys already
    // submitted this frame, so each successive shadow lands one step nearer
    // than the last and two of them never collide on one slot.
    //
    // This is NOT the key the quad is drawn at. The original has TWO ordering
    // tables: AddFadePoly inserts the quad into the first one at the key below,
    // and when the first table's walk reaches it, FUN_00446e40 transforms the
    // quad and re-inserts its triangles into the SECOND table - the one
    // d3d_do_render_ot_walk's second pass actually draws - keyed at
    // `mean(view z of the triangle's vertices) >> 4` (0x0044764a: sum the three
    // vertex z's, IDIV 3, SAR 4). Every type-10 sprite is identity-mapped
    // between the two tables (d3d_do_render_ot_walk re-inserts it at
    // `capacity - 1 - walkIndex`, i.e. its own bucket), so a room mask keeps its
    // authored key and the shadow lands among the masks at its TRUE view-space
    // Z. The first-table key only decides WHEN the quad's triangles are
    // inserted, which matters only for ties inside one second-table bucket.
    //
    // The port used otKey * 16 as the drawn key. That is the first-table key,
    // and it is wrong: for the placement records with flag == 0 it is only
    // ~400 view units too far back, but the flag != 0 records (0x00456f6d,
    // alpha = forceAlpha + 1) collapse it to a near-constant 400/592/800 no
    // matter where the character stands - so the shadow sorted as if it were
    // 800 units from the camera and painted over every room mask in front of
    // her (the "shadow drawn above the background mask" report). Sort on the
    // projected corners, which is what the original's second table holds.
    //
    unsigned int otKey = (unsigned int)alpha >> 2;
    otKey = (otKey > (unsigned int)g_OTIndex) ? (otKey - (unsigned int)g_OTIndex) : 0u;
    if (otKey > 0xfff) otKey = 0xfff;

    // 0x004702a5-0x004702fd: walk the fade polys already inserted this frame and
    // push this one one slot behind any that is NEARER in view space. Two
    // shadows whose keys land on the same slot - which the >> 2 makes easy, as
    // it quantises 16 view units into one step - would otherwise be ordered by
    // submission, so the far one could paint over the near one wherever they
    // overlap. The bump is sequential and order-dependent exactly as written:
    // each hit raises the running key, so a later comparison sees the raised
    // value. The original compares the OT records' translation Z with a strict
    // FCOMP/C0 test, i.e. `prev < this`.
    //
    // This pass orders the FIRST table, so it no longer changes what is drawn
    // where - the drawn key is sortZ below, which is already ordered by depth.
    // It is kept because it is the original's bookkeeping for the insertion
    // order of ties, and because g_FadeOtKey is the table that tie-break reads.
    for (int j = 0; j < g_OTIndex && j < FADE_OT_MAX; j++) {
        if (g_FadeOtZ[j] < transZ && otKey <= (unsigned int)g_FadeOtKey[j]) {
            otKey = (unsigned int)g_FadeOtKey[j] + 1u;
            if (otKey > 0xfff) otKey = 0xfff;
        }
    }
    if (otKey > 0xfff) otKey = 0xfff;

    // The key the quad is DRAWN at - see the two-ordering-table note above.
    // sortZ is the mean view-space Z of the quad's corners, the same quantity
    // and the same units as a TMD triangle's `depth` and a mask's `fade << 4`,
    // so the shadow now interleaves with both instead of floating over them.
    // A corner behind the camera makes the mean meaningless; floor it at 0 so
    // it cannot wrap the unsigned comparison.
    const unsigned int shadowDepth = (sortZ < 0) ? 0u : (unsigned int)sortZ;

    // 0x0047032d/0x00470337: publish this poly's key, then advance. One
    // increment per CALL, not per command - a clipped pentagon becomes two
    // TextureDraws here but was still a single primitive in the original.
    if (g_OTIndex >= 0 && g_OTIndex < FADE_OT_MAX) {
        g_FadeOtZ[g_OTIndex]   = transZ;
        g_FadeOtKey[g_OTIndex] = (unsigned short)otKey;
    }
    g_OTIndex++;

    // `alpha` is the ORDERING-TABLE key only - it never reaches the blend.
    // The original inserts the primitive with (alpha >> 2) - g_OTIndex as its
    // OT depth (0x00470100) and hands the hardware a texture page created in
    // mode 1, whose blend is dst * brightness with brightness coming purely
    // from the kage CLUT. Scaling the sprite's opacity by alpha/g_MaxFadeValue
    // was a port invention: it faded the shadow to ~25% wherever the camera
    // sat a few thousand units away, which is every in-game camera. The
    // texture's own alpha (255 - brightness, baked by LoadShadowMaskTexture)
    // must be the whole story, so the vertex alpha stays at full strength.
    const float a = 1.0f;

    float r, g, b;
    if (rgb[1] == rgb[0] && rgb[2] == rgb[0]) {
        r = 0.0f;
        g = 0.0f;
        b = 0.004f;
    } else {
        r = (float)(rgb[0] < 0x80 ? rgb[0] : 256 - rgb[0]) * 0.00390625f;
        g = (float)(rgb[1] < 0x80 ? rgb[1] : 256 - rgb[1]) * 0.00390625f;
        b = (float)(rgb[2] < 0x80 ? rgb[2] : 256 - rgb[2]) * 0.00390625f;
    }

    static const int quadIdx[2][4] = { { 0, 1, 2, 3 }, { 0, 3, 4, 4 } };
    for (int q = 0; q < cmds; q++) {
        int k0 = quadIdx[q][0];
        int k1 = quadIdx[q][1];
        int k2 = quadIdx[q][2] > count - 1 ? count - 1 : quadIdx[q][2];
        int k3 = quadIdx[q][3] > count - 1 ? count - 1 : quadIdx[q][3];

        TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
        cmd->type = 12;
        cmd->sortClass = SPRITE_CLASS_SHADOW;
        cmd->renderFlags = 0;
        cmd->x0 = (short)px[k0];
        cmd->y0 = (short)py[k0];
        cmd->x1 = (short)px[k1];
        cmd->y1 = (short)py[k1];
        cmd->x2 = (short)px[k2];
        cmd->y2 = (short)py[k2];
        cmd->x3 = (short)px[k3];
        cmd->y3 = (short)py[k3];
        cmd->u0 = (short)cu[k0];
        cmd->v0 = (short)cv[k0];
        cmd->u1 = (short)cu[k1];
        cmd->v1 = (short)cv[k1];
        cmd->u2 = (short)cu[k2];
        cmd->v2 = (short)cv[k2];
        cmd->u3 = (short)cu[k3];
        cmd->v3 = (short)cv[k3];
        int w0 = wz[k0] > 30000 ? 30000 : wz[k0];
        int w1 = wz[k1] > 30000 ? 30000 : wz[k1];
        int w2 = wz[k2] > 30000 ? 30000 : wz[k2];
        int w3 = wz[k3] > 30000 ? 30000 : wz[k3];
        cmd->wz0 = (short)w0;
        cmd->wz1 = (short)w1;
        cmd->wz2 = (short)w2;
        cmd->wz3 = (short)w3;
        cmd->depthSort = shadowDepth;
        cmd->spriteFlags = 0;
        cmd->alpha = a;
        cmd->r = r;
        cmd->g = g;
        cmd->b = b;
        cmd->variantAlpha = 0.0f;   // shadow alpha travels in cmd->alpha
        cmd->extraFlags = (unsigned int)tpage;

        g_SpriteQueueCount++;
    }
    return 1;
}

// ============================================================================
// DrawPrim_SpriteLarge - simplified for D3D11 port
// ============================================================================
int DrawPrim_SpriteLarge(int* params, unsigned short alpha, int tpage,
                         unsigned int u, unsigned int v, unsigned int clut) {
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return 0;

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = SPRITE_CLASS_NORMAL;
    cmd->r = (float)params[0] * g_ColorScaleFactor;
    cmd->g = (float)params[1] * g_ColorScaleFactor;
    cmd->b = (float)params[2] * g_ColorScaleFactor;
    cmd->spriteFlags = 0;
    cmd->alpha = 1.0f;
    cmd->variantAlpha = 0.0f;
    cmd->x0 = 0;
    cmd->y0 = 0;
    cmd->x1 = 320;
    cmd->y1 = 240;
    cmd->u0 = (short)u;
    cmd->v0 = (short)v;
    cmd->u1 = (short)(u + 320);
    cmd->v1 = (short)(v + 240);
    cmd->depthSort = (int)alpha;
    cmd->extraFlags = tpage;

    g_SpriteQueueCount++;
    return 1;
}

// ============================================================================
// TexturePage functions (unchanged)
// ============================================================================
void TexturePage_Load(int slotIndex, void* imageData) {
    ProcessTextureImage(imageData, (short)slotIndex, 0, slotIndex);
}

void TexturePage_ClearAll(void) {
    // Only clear the legacy PSX texture-page handle table. The D3D11 SRV
    // metadata (g_TexturePageWidth/Height/Bpp) must stay in sync with
    // g_TexturePageSRV, which is NOT released here — see the preservation
    // note in TexturePage_DeleteSet. Zeroing width/height while the SRV
    // persists made FlushSpriteCommands fall back to pageW=256, which
    // under-sampled 64x64 global textures (statface/blue/staitem) and
    // rendered their sprites as a tiny 8x8 region stretched to size.
    for (int i = 0; i < 256; i++) {
        g_TexturePageTable_DAT[i] = 0;
    }
}

// TexturePage_Create (0x0046c3c0) - re-create a texture page from the slot's
// stored PSXTexture work buffer. The original calls this from FUN_0047d0e0 at
// menu exit, when the PSX renderer's pages were clobbered by the menu. The
// port keeps no per-slot data copy - the D3D11 SRVs persist across the menu
// (TexturePage_ClearAll only clears the legacy handle table) - so this is a
// preservation no-op; create_texture_page(NULL) returns 0 without touching
// anything.
void TexturePage_Create(int slotIndex) {
    int mode = (slotIndex < 2) ? 2 : 0x22;
    int handle = create_texture_page(NULL, mode);
    if (handle != 0 && slotIndex >= 0 && slotIndex < 256) {
        g_TexturePageTable_DAT[slotIndex] = (DWORD)handle;
    }
}

void TexturePage_SetupFull(void* imageData, short bankID, short pageOffset, int slotIndex) {
    ProcessTextureImage(imageData, bankID, pageOffset, slotIndex);
    // ProcessTextureImage rebuilds the legacy Marni page, but the DX11 path
    // needs an explicit SRV as well. Room masks are loaded through this entry
    // point and AddSprite samples the resulting page directly.
    LoadEffectTextureSheet(slotIndex, imageData);
    if (slotIndex >= 0 && slotIndex < 256) {
        // This is the same origin written by ProcessTextureImage's original
        // descriptor at 0x008ed838/0x008ed83a. AddSprite subtracts it after
        // adding the descriptor's bank/depth page offset.
        g_TexturePageOriginX[slotIndex] =
            (short)(bankID * 0x40 - ((bankID < 0x10) ? 0 : 0x400));
        g_TexturePageOriginY[slotIndex] = (bankID < 0x10) ? 0 : 0x100;
    }
}

void TexturePage_Refresh(int slotIndex, int mode) {
    int handle = create_texture_page(NULL, (mode == 0) ? 2 : 1);
    if (handle != 0 && slotIndex >= 0 && slotIndex < 256) {
        g_TexturePageTable_DAT[slotIndex] = (DWORD)handle;
    }
}

void TexturePage_RefreshCLUT(int slotIndex, int mode, int clutIndex) {
    int cmode = (mode != 0) ? 1 : 2;
    int handle = create_texture_page(NULL, cmode);
    if (handle != 0 && slotIndex >= 0 && slotIndex < 256) {
        g_TexturePageTable_DAT[slotIndex] = (DWORD)handle;
    }
}

void TexturePage_DeleteSet(int slotIndex) {
    for (int i = 0; i < 8; i++) {
        int pageIdx = slotIndex * 8 + i;
        if (pageIdx >= 0 && pageIdx < 256) {
            DWORD handle = g_TexturePageTable_DAT[pageIdx];
            if (handle != 0) {
                destroy_texture_page(handle);
                g_TexturePageTable_DAT[pageIdx] = 0;
            }
            // SRVs, width, height, and Bpp are preserved across clear_textures()
            // so global textures (fonts, status.tim) remain valid for rendering.
        }
    }
}

void delete_texture_set_secondary(int slotIndex) {
    TexturePage_DeleteSet(slotIndex + 0xF);
}

// ============================================================================
// Slide projector CLUT variants (port-only helpers)
//
// load_slides_images (0x00478110) hands slide.tim to TexturePage_LoadImage,
// and the original's TexturePage_LoadImage tail (0x0046d77e) walks the parsed
// CLUT list and creates ONE texture page per CLUT - each slide frame is the
// same image with its own palette. AddTintSprite_Ex then picks the page whose
// index matches clutY - 0x1ed.
//
// The D3D11 path mirrors that with one pre-built RGBA SRV per CLUT at fixed
// slots SLIDES_TEX_BASE..+7, so the projector never has to rebuild textures
// mid-scroll (the original recreated its page on every sprite submission).
// ============================================================================
#define SLIDES_TEX_BASE 240
static MarniHandle s_slidesVariantSRV[8];
static int         s_slidesVariantCount = 0;

MarniHandle Slides_GetVariantSRV(int variant) {
    if (variant < 0 || variant >= 8) return MARNI_NULL_HANDLE;
    return s_slidesVariantSRV[variant];
}

int Slides_GetVariantCount(void) {
    return s_slidesVariantCount;
}

int Slides_GetVariantSlot(int variant) {
    return SLIDES_TEX_BASE + variant;
}

static void Slides_ReleaseVariants(void) {
    for (int i = 0; i < 8; i++) {
        if (s_slidesVariantSRV[i] != MARNI_NULL_HANDLE) {
            Marni_DX()->DestroyTexture(s_slidesVariantSRV[i]);
            s_slidesVariantSRV[i] = MARNI_NULL_HANDLE;
        }
        const int slot = SLIDES_TEX_BASE + i;
        if (slot >= 0 && slot < 256) g_TexturePageSRV[slot] = MARNI_NULL_HANDLE;
    }
    s_slidesVariantCount = 0;
}

void TexturePage_LoadImage(void* imageData, short param2, short param3) {
    ProcessTextureImage(imageData, param2, param3, 0);

    // 0x0046d77e tail: parse the TIM again and build one RGBA texture per
    // CLUT palette. slide.tim packs all six projector slides as 192x128
    // cells tiled 3-across / 2-down inside the shared 8bpp pixel plane, and
    // cell N is drawn with palette N - the per-variant copies the original
    // uploads are these crops re-paletted, not the whole sheet.
    Slides_ReleaseVariants();

    PSXTexture psxTex;
    if (psxTex.Store((int*)imageData, 1) == 0) return;
    const int w = psxTex.m_WidthPixels;
    const int h = psxTex.m_Height;
    const int bpp = psxTex.m_BitDepth;
    if (w <= 0 || h <= 0 || psxTex.m_pPixelData == NULL) return;

    int numCluts = (int)psxTex.m_NumCLUTs;
    if (numCluts < 1) numCluts = 1;
    if (numCluts > 8) numCluts = 8;

    // Cell geometry: the projector draws u in 0..153 and h = 0x7f out of a
    // MarniBits__CreateWork(0xc0, 0x80) page, i.e. every cell is 192x128.
    const int cellW = 192;
    const int cellH = 128;

    const int entriesPerCLUT = (bpp == 4) ? 16 : 256;
    DWORD* clutRGBA = new DWORD[entriesPerCLUT];

    const int drawW = (cellW < w) ? cellW : w;
    const int drawH = (cellH < h) ? cellH : h;

    for (int c = 0; c < numCluts; c++) {
        WORD* clut = psxTex.m_pCLUTData + (size_t)c * entriesPerCLUT;
        for (int i = 0; i < entriesPerCLUT; i++) {
            WORD clr = clut[i];
            // Same conversion as LoadEffectTextureSheet: STP is not alpha,
            // index 0 is the black colour key.
            DWORD r = ((clr >> 0)  & 0x1F) * 255 / 31;
            DWORD g = ((clr >> 5)  & 0x1F) * 255 / 31;
            DWORD b = ((clr >> 10) & 0x1F) * 255 / 31;
            DWORD a = (i == 0) ? 0x00 : 0xFF;
            clutRGBA[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }

        // Crop cell c. The original uploads each variant to VRAM
        // CalcAddress((c & ~1) * 0x60, (c & 1) << 7): cells are tiled in
        // column PAIRS - slide 0 above slide 1 in column 0, 2 above 3 in
        // column 1, 4 above 5 in column 2 - which is also why the sprite's
        // texV (slideIndex << 7) only ever lands on rows 0 and 128.
        const int cellX = (c >> 1) * cellW;
        const int cellY = (c & 1) * cellH;

        DWORD* rgba = new DWORD[drawW * drawH];
        for (int y = 0; y < drawH; y++) {
            const int srcY = cellY + y;
            for (int x = 0; x < drawW; x++) {
                const int srcX = cellX + x;
                DWORD colour = 0xFF000000;
                if (srcX < w && srcY < h) {
                    const BYTE* pix = (const BYTE*)psxTex.m_pPixelData;
                    if (bpp == 8) {
                        colour = clutRGBA[pix[srcY * w + srcX]];
                    } else if (bpp == 4) {
                        BYTE byteVal = pix[srcY * (w / 2) + srcX / 2];
                        BYTE nibble = (srcX & 1) ? (byteVal >> 4) : (byteVal & 0xF);
                        colour = clutRGBA[nibble];
                    }
                }
                rgba[y * drawW + x] = colour;
            }
        }

        MarniCreateTexture(drawW, drawH, 32, rgba, &s_slidesVariantSRV[c]);
        delete[] rgba;

        const int slot = SLIDES_TEX_BASE + c;
        if (slot >= 0 && slot < 256) {
            g_TexturePageSRV[slot] = s_slidesVariantSRV[c];
            g_TexturePageWidth[slot] = drawW;
            g_TexturePageHeight[slot] = drawH;
            g_TexturePageBpp[slot] = bpp;
        }
    }

    delete[] clutRGBA;
    s_slidesVariantCount = numCluts;
}

// Display_SetParams (0x00470750)
// Sets the display-image origin read by FUN_00470a90 when it rebuilds the
// background sprites: 0x004c335c / 0x004c3360.
//
// These are NOT the subpixel offset (0x004d2bd0 / 0x004d2bd4). Writing the
// subpixel offset here zeroed the projection centre every frame, because
// ResetScreenAndRebuildSprites (0x00401020) calls this with (0,0) and runs once
// per frame from the main loop - so every 3D object was projected around the
// top-left corner of the screen instead of the screen centre.
void Display_SetParams(int param1, int param2) {
    g_displayImageOriginX = param1;
    g_displayImageOriginY = param2;
}

// ============================================================================
// texture_queue_reset (0x004739e0)
// Resets the texture queue state and all 4 queue entries.
// Each entry is 10 bytes. Clears counters DAT_00ae9f04, DAT_00ae9f06,
// DAT_00ae9f00, DAT_00ae9efc.
//
// FOUR entries, not five: the original is `cVar2 = 4; do { ... } while (--cVar2
// != 0);`, and FUN_00473d10 / FUN_00473d60 likewise scan exactly 4. This used to
// loop 5 times and ran 10 bytes off the end of the 40-byte g_textureQueueData.
// Harmless in the original address map, but our .bss puts g_MessageSpeedUpFlag,
// g_MessageCharDelay, g_MessageCharTimer and g_MessageClutBase immediately after
// the array, so the phantom fifth entry's `p[1] = 0` landed exactly on
// g_MessageCharDelay. room_set -> texture_queue_reset runs a few frames into the
// new-game loading message, so the reveal started at the correct speed and then
// dropped to delay 0; msg_skip_char then set the timer to 0 and the next frame's
// `timer - 1` wrapped an unsigned char to 255, stalling ~256 frames per glyph.
// It also clobbered 6 bytes of g_psxTextureArray. Do not "restore" the 5.
// ============================================================================
void texture_queue_reset(void) {
    DAT_00ae9f04 = 0;
    DAT_00ae9f06 = 0;
    DAT_00ae9f00 = 0;
    DAT_00ae9efc = 0;
    unsigned char* p = g_textureQueueData;
    for (int i = 0; i < 4; i++) {
        p[3] = 0;
        p[4] = 0;
        p[5] = 0;
        *(unsigned short*)(p + 6) = 0;
        *(unsigned short*)(p + 8) = 0x100;
        p[1] = 0;
        p += 10;
    }
}

// (0x00470c60) - Queue EKG line primitive (primary line)
// Builds a line primitive from the 16-byte EKG line struct and inserts it
// into the ordering table with OT depth `param_2`. The original wrote the
// primitive into the DAT_008e3e60 render buffer and called the
// CMarniDirect3D vtable[10] entry (SetTexture == OT_InsertPrimitive); the
// port submits the same line into the sprite command queue instead.
// Returns 1 when submitted, 0 when the per-frame primitive cap is reached.
int FUN_00470c60(void* prim, int depth)
{
    if (g_renderPrimCount >= 0x28) return 0;

    unsigned char* p = (unsigned char*)prim;
    unsigned short depthOut = (unsigned short)depth;

    // Software-renderer modes offset the OT depth by 0x28.
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D && (pD3D->m_deviceType == 5 || pD3D->m_deviceType == 7)) {
        depthOut = depthOut + 0x28;
    }

    short x0 = *(short*)(p + 4);
    short y0 = *(short*)(p + 6);
    short x1 = *(short*)(p + 8);
    short y1 = *(short*)(p + 10);
    float r = (float)p[0xC] * 0.00390625f;
    float g = (float)p[0xD] * 0.00390625f;
    float b = (float)p[0xE] * 0.00390625f;

    if (g_nFadeInverted != 0) {
        if (g_MaxFadeValue < (int)depthOut) depthOut = (unsigned short)g_MaxFadeValue;
        depthOut = (unsigned short)(g_MaxFadeValue - (int)depthOut);
    }

    if ((g_RenderDisableFlags & 0x10) == 0) {
        SubmitLine(x0, y0, x1, y1, depthOut, r, g, b, 1.0f);
        g_renderPrimCount++;
    }
    return 1;
}

// (0x00470e60) - Queue EKG line primitive (secondary line with gradient)
// Identical to FUN_00470c60 but the line struct carries a second color
// endpoint at bytes 0xF-0x11 (the gradient target computed by FUN_00438800).
int FUN_00470e60(void* prim, int depth)
{
    if (g_renderPrimCount >= 0x28) return 0;

    unsigned char* p = (unsigned char*)prim;
    unsigned short depthOut = (unsigned short)depth;

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D && (pD3D->m_deviceType == 5 || pD3D->m_deviceType == 7)) {
        depthOut = depthOut + 0x28;
    }

    short x0 = *(short*)(p + 4);
    short y0 = *(short*)(p + 6);
    short x1 = *(short*)(p + 8);
    short y1 = *(short*)(p + 10);
    float r = (float)p[0xC] * 0.00390625f;
    float g = (float)p[0xD] * 0.00390625f;
    float b = (float)p[0xE] * 0.00390625f;

    if (g_nFadeInverted != 0) {
        if (g_MaxFadeValue < (int)depthOut) depthOut = (unsigned short)g_MaxFadeValue;
        depthOut = (unsigned short)(g_MaxFadeValue - (int)depthOut);
    }

    if ((g_RenderDisableFlags & 0x10) == 0) {
        SubmitLine(x0, y0, x1, y1, depthOut, r, g, b, 1.0f);
        g_renderPrimCount++;
    }
    return 1;
}

