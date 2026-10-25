// LabSlides.cpp - Slide projector interactive screen (SysFlags bit 0x1f)
// Decompiled from Ghidra. The projector is used by the lab projection room
// (stage 4 / room 4, camera 4); the room script raises SysFlags 0x1f and the
// shared player-flag 0x20 gate in check_and_display_interactive_screen
// (0x0042a030) dispatches here.
//
// Block layout note: 0x00463300 is a tiny three-entry dispatcher on
// g_labSlidesFuncIndex whose targets (0x00463320 init, 0x004633c0 update,
// 0x004636b0 finish) are ALSO the tail-jump targets of the message-action
// jump table at 0x004c2130 (actions 4/5/6 read by handle_message_post_action,
// 0x00455fb0). Both entry paths execute the very same code - each block ends
// with its own RET - so the port exposes them as lab_slides_start/update/
// finish and shares them instead of duplicating the bodies.
// The slide-change step FUN_0046336f (stop both sound slots, reset scroll to
// (-96,-66), refresh texture page 0x2e) is inlined in the port's update path.
#include "../Globals.h"
#include "BioCard.h"
#include "SpriteRenderer.h"
#include "FileLoader.h"
#include "../system/AssetPath.h"
#include "../marni/MarniSound.h"

extern void Flg_on(int baseAddr, unsigned int bitIndex);              // 0x00473ef0
extern void FUN_00473f10(int* baseAddr, unsigned int bitIndex);       // 0x00473f10 - Flg_off
extern void FUN_004805d0(short param1, unsigned int param2,
                         unsigned int param3, unsigned int param4);   // 0x004805d0 SoundSystem.cpp

// Projector sound + sprite helpers moved below (they were in GameState.cpp)
void lab_slides_stop_snd(short slot);                                 // 0x0047f960
void lab_slides_set_snd_slot(short slot);                             // 0x0047f930
void lab_slides_set_snd_params(int channel, int pan, int volume);     // 0x0047f990

// The projector clears the same interactive-screen gate the numeric panel
// does (passcode_panel_finish) to hand control back to the room.

// 0x004c2150..0x004c2170 - Projector frame masks, walked BACKWARDS by the
// original draw loop. Game space is x=-160..160 / y=-120..120; the four rects
// mask everything outside the projection window (-103..102 x -72..66).
// Table order here is draw order: right bar, left bar, bottom band, top band.
static const short s_slideFrameMasks[4][4] = {
    { 0x66, -72,   58, 138 },   // 0x004c2168
    { -160, -72,   57, 138 },   // 0x004c2160
    { -160,  66, 0x140, 54 },   // 0x004c2158
    { -160, -120, 0x140, 48 },  // 0x004c2150
};

// 0x00463320 - Shared init block (funcIndex 0 / message action 4). Switches
// the room to the projector close-up camera, primes the BGM channels and
// rewinds the slide strip to the first frame.
void lab_slides_start(void)
{
    g_cutId = g_roomCameraId;
    g_labSlidesAnimState = 0;
    g_bGameActive = 0;
    g_labSlidesFuncIndex = 1;
    g_roomCameraId = 4;
    g_message_flags = (unsigned short)(g_message_flags & 0xfeff);
    display_room_camera_bg();
    g_labSlidesSlideIndex = 0;
    g_labSlidesLoopDone = 0;
    g_labSlidesMsgId = 0;
    g_labSlidesCountdown = 0x10;
    lab_slides_stop_snd(0);
    lab_slides_set_snd_slot(1);
    lab_slides_set_snd_params(2, 0x14, 0x14);
    lab_slides_set_snd_slot(2);
    // 0x00463396/0x004633a3: raw dword stores; scrollX starts off-screen left
    // and scrollY keeps the packed 0xffffffbe marker the panel path also uses.
    g_labSlidesScrollX = -96;          // 0xffffffa0
    g_labSlidesScrollY = 0xffffffbe;
    TexturePage_Refresh(0x2e, 0);
}

// 0x004633c0 - Shared update block (funcIndex 1 / message action 5): advances
// the slide-scroll animation, then redraws the projector frame masks and the
// current slide strip sprite.
void lab_slides_update(void)
{
    switch (g_labSlidesAnimState) {
    case 0:
        // 0x004633d0: unconditional decrement, zero-tested afterwards.
        --g_labSlidesCountdown;
        if (g_labSlidesCountdown != 0) {
            break;
        }
        g_labSlidesAnimState = 1;
        g_labSlidesScrollX = 0x66;
        // fall through - the original has no break between states 0 and 1
    case 1:
        g_labSlidesScrollX -= 0x12;
        if (g_labSlidesScrollX == 0x1e) {
            play_sfx(2, 0x1b, 0);
        }
        if (g_labSlidesScrollX == -0x60) {
            g_labSlidesAnimState = 2;
            set_message_display(g_labSlidesMsgId, 0);
        }
        break;
    case 2:
        // Wait for the slide caption to be dismissed.
        if ((g_menu_choice_id & 0x80) == 0) {
            g_labSlidesAnimState = 3;
            play_sfx(2, 0x1a, 0);
        }
        break;
    case 3:
        g_labSlidesScrollX -= 0x12;
        if (g_labSlidesScrollX < -0x127) {
            g_labSlidesAnimState = 0;
            g_labSlidesCountdown = 0x10;
            if (g_labSlidesSlideIndex < 5) {
                ++g_labSlidesSlideIndex;
                ++g_labSlidesMsgId;
            } else if (g_labSlidesLoopDone == 0) {
                // One full pass done: show slide 5 once more darkened, then quit.
                g_labSlidesLoopDone = 1;
                ++g_labSlidesMsgId;
            } else {
                g_labSlidesFuncIndex = 2;
                ++g_labSlidesMsgId;
            }
        }
        break;
    default:
        break;
    }

    // Projector frame masks (0x004634e0..0x00463537), drawn back to front.
    g_rect.r = 0x80;
    g_rect.g = 0x80;
    g_rect.b = 0x80;
    g_rect.textureId = 0x60000000;
    for (int i = 0; i < 4; ++i) {
        g_rect.x = s_slideFrameMasks[i][0];
        g_rect.y = s_slideFrameMasks[i][1];
        g_rect.w = s_slideFrameMasks[i][2];
        g_rect.h = s_slideFrameMasks[i][3];
        draw_rect(&g_rect, 4, 1);
    }

    // Slide strip sprite (0x00463539..0x004636a9). Only drawn while the strip
    // is actually scrolling or being captioned.
    if (g_labSlidesAnimState != 0) {
        g_TextureDesc.texU = 0;
        g_rect.y = -0x42;
        g_rect.h = 0x7f;
        // 0x00463564: unsigned test - true while the visible window covers the
        // whole strip, i.e. the strip is parked fully inside the projector.
        if ((unsigned int)(g_labSlidesScrollX + 0x67) < 0xe) {
            g_rect.w = 0xbf;
            g_rect.x = (short)g_labSlidesScrollX;
        } else if (g_labSlidesAnimState == 1) {
            // Strip entering from the right: clip against the leading edge.
            g_rect.w = (short)(0x66 - g_labSlidesScrollX);
            g_rect.x = (short)g_labSlidesScrollX;
        } else {
            // Strip leaving to the left: clip against the trailing edge.
            g_rect.x = -0x67;
            g_rect.w = (short)(g_labSlidesScrollX + 0x126);
            g_TextureDesc.texU = (unsigned char)(0x99 - (char)g_labSlidesScrollX);
        }

        if (g_labSlidesLoopDone == 0 && g_labSlidesSlideIndex == 5) {
            // Second showing of the last slide is dimmed and has no sprite.
            g_rect.r = 0x38;
            g_rect.g = 0x38;
            g_rect.b = 0x38;
        } else {
            g_TextureDesc.screenX = g_rect.x;
            g_rect.r = 0x70;
            g_rect.g = 0x70;
            g_rect.b = 0x70;
            g_TextureDesc.flags = 0x41000040;
            g_TextureDesc.screenY = -0x42;
            g_TextureDesc.height = 0x7f;
            g_TextureDesc.texV = (unsigned char)(g_labSlidesSlideIndex << 7);
            g_TextureDesc.width = (unsigned short)g_rect.w;
            g_TextureDesc.clutX = 0;
            // clutY = slide CLUT index (+0x1ed base recorded by
            // TexturePage_LoadImage when load_slides_images ran).
            g_TextureDesc.clutY =
                (short)(g_labSlidesSlideIndex + 0x1ed);
            // 0x00463651..0x00463689: depth = ((slide & ~1) * 0x60) >> 7 + 9.
            g_TextureDesc.texturePage = (short)(
                (((unsigned int)g_labSlidesSlideIndex & 0xfffffffeu) * 0x60) >> 7) + 9;
            AddTintSprite_Ex(&g_TextureDesc, 4);
        }
        draw_rect(&g_rect, 4, 1);
    }
}

// 0x004636b0 - Shared finish block (funcIndex 2 / message action 6): restore
// the room camera, release the slide textures and hand control back.
void lab_slides_finish(void)
{
    TexturePage_DeleteSet(0x2e);
    g_roomCameraId = g_cutId;
    display_room_camera_bg();
    // Clears the player-flag gate, exactly like passcode_panel_finish.
    FUN_00473f10((int*)g_ScenarioFlags, SCENARIO_FLAG_INTERACTIVE_SCREEN);
    lab_slides_stop_snd(1);
    lab_slides_stop_snd(2);
    lab_slides_set_snd_slot(0);
    g_message_flags = (unsigned short)(g_message_flags | 0x100);
    g_main_state_flags &= ~MSF_INTENSITY_RAMP;
    g_bGameActive = 2;
}

// 0x00463300 - Display/update the slide projector screen. Gated on message
// flags bit 0x80; dispatches on g_labSlidesFuncIndex into the shared blocks.
void display_slides(void)
{
    if (((unsigned short)g_message_flags & 0x80) == 0) {
        return;
    }
    switch (g_labSlidesFuncIndex) {
    case 0:
        lab_slides_start();
        break;
    case 1:
        lab_slides_update();
        break;
    case 2:
        lab_slides_finish();
        break;
    default:
        break;
    }
}

// ============================================================================
// load_slides_images (0x00478110)
// Loads the projector slide TIM image and creates a texture page from it.
// Called during room_set for stage 4, room 4 (the lab projector room).
// ============================================================================
void load_slides_images(void)
{
    // 0x00478110: Load slide TIM file into display image buffer
    LoadFile(GAME_DATA_ROOT "data\\slide.tim", g_TimImageBuffer__bitmap, 0x20);
    // 0x00478124: Create texture page from loaded TIM data
    TexturePage_LoadImage(g_TimImageBuffer__bitmap, 9, 0xd);
}

// ============================================================================
// Projector audio + sprite producers (moved here from GameState.cpp)
// ============================================================================

// ============================================================================
// lab_slides_stop_snd (0x0047f960) - stop one BGM channel for the slide
// projector. Skipped entirely while a BGM fade to 0xff is pending
// (g_targetBgmState); slot is bounds-checked signed against 3.
// ============================================================================
void lab_slides_stop_snd(short slot)
{
    if (g_targetBgmState == 0xff || slot >= 3) {
        return;
    }
    if (g_SndBank[slot].handle != 0) {
        setSndStop(g_SndBank[slot].handle);
    }
}

// ============================================================================
// lab_slides_set_snd_slot (0x0047f930) - re-assign the hardware slot of one
// BGM channel (used to swap in the projector's audio loop).
// ============================================================================
void lab_slides_set_snd_slot(short slot)
{
    if (slot >= 3) {
        return;
    }
    if (g_SndBank[slot].handle != 0) {
        SetSndSlot(g_SndBank[slot].handle, (int)g_SndBank[slot].slot);
    }
}

// ============================================================================
// lab_slides_set_snd_params (0x0047f990) - apply pan/volume to one BGM
// channel via snd_set_channel_pan_volume; param1 is the cached SCD pan base.
// ============================================================================
void lab_slides_set_snd_params(int channel, int pan, int volume)
{
    FUN_004805d0((short)(unsigned char)DAT_00bf07ef,
                 (unsigned int)channel, (unsigned int)pan, (unsigned int)volume);
}

// ============================================================================
// AddTintSprite_Ex (0x0046f8a0)
// Sprite producer used by the slide projector: like AddTintSprite but the
// texture is the slide.tim sheet and clutY selects which of its CLUTs
// (one per slide frame) to render with.
//
// The original refreshed PSX texture page 0x2e with CLUT variant
// (clutY - 0x1ed) and drew through the per-variant page handle. The
// D3D11 port instead pre-builds one RGBA SRV per CLUT when load_slides_images
// parses slide.tim (see TexturePage_LoadImage), so this producer resolves the
// variant's SRV and submits a normal type-10 sprite command with the same
// geometry/depth math as the original.
// ============================================================================
int AddTintSprite_Ex(TextureDesc* texture, unsigned short brightness)
{
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) {
        return 0;
    }

    // 0x0046f8bb: variant = clutY - CLUT base (0x008f788a = 0xd +
    // 0x1e0 recorded by TexturePage_LoadImage). The font shadow row (diff 8)
    // maps to variant 1 exactly like the original.
    int variant = (int)texture->clutY - 0x1ed;
    if (variant == 8) {
        variant = 1;
    }
    // 0x0046f8d8: unsigned bound check against the parsed CLUT count.
    if (variant < 0 || Slides_GetVariantCount() < variant) {
        return 0;
    }

    // 0x0046f8e2: recreate the legacy page for this CLUT (parity with the
    // original pipeline; rendering samples the pre-built SRV below).
    TexturePage_RefreshCLUT(0x2e, 0, variant);

    MarniHandle srv = Slides_GetVariantSRV(variant);
    if (srv == MARNI_NULL_HANDLE) {
        return 0;
    }

    // Find the slot index the flusher resolves extraFlags through.
    const int texSlot = Slides_GetVariantSlot(variant);
    if (texSlot < 0 || texSlot >= 256 || g_TexturePageSRV[texSlot] != srv) {
        return 0;
    }

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = SPRITE_CLASS_NORMAL;
    unsigned int flags;
    BuildSpriteRenderFlags(texture->flags, &flags);
    if (GetTextureVariant(texture->flags) != 0) {
        flags |= SPRITE_FLAG_VARIANT;
    }
    cmd->spriteFlags = flags;

    short sx = (short)(texture->screenX + g_ScreenOffsetX);
    short sy = (short)(texture->screenY + g_ScreenOffsetY);
    cmd->x0 = sx - texture->pivotX;
    cmd->y0 = sy - texture->pivotY;
    cmd->x1 = (short)(sx + ((unsigned int)texture->width - (unsigned int)texture->pivotX) - 1);
    cmd->y1 = (short)(sy + ((unsigned int)texture->height - (unsigned int)texture->pivotY) - 1);

    cmd->r = (float)texture->colorMulR * 0.0078125f;
    cmd->g = (float)texture->colorMulG * 0.0078125f;
    cmd->b = (float)texture->colorMulB * 0.0078125f;

    // OT depth = brightness * 16 + 500, with the shared fade inversion/clamp.
    unsigned short fade = brightness;
    if (g_nFadeInverted != 0) {
        if (fade > g_MaxFadeValue) fade = (unsigned short)g_MaxFadeValue;
        fade = (unsigned short)(g_MaxFadeValue - fade);
    }
    if (fade > 0xfff) {
        fade = 0xfff;
    }
    cmd->depthSort = (unsigned int)fade * 0x10u + 500u;

    // v0 is always 0 in the original: each variant page already encodes the
    // slide row, so the port's per-variant SRV is sampled from its top-left.
    cmd->u0 = (unsigned short)texture->texU;
    cmd->v0 = 0;
    cmd->u1 = (unsigned short)(cmd->u0 + texture->width - 1);
    cmd->v1 = (unsigned short)(cmd->v0 + texture->height - 1);

    cmd->extraFlags = (unsigned int)texSlot;
    cmd->variantAlpha = SpriteVariantAlpha(texture->flags);
    cmd->alpha = SpriteDrawAlpha(cmd->variantAlpha);

    // 0x0046fa3c: submission is suppressed while rendering is disabled, but
    // the call still reports success without advancing the queue.
    if ((g_RenderDisableFlags & 0x21) == 0) {
        g_SpriteQueueCount++;
    }
    return 1;
}
