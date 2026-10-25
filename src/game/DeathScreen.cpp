// DeathScreen.cpp - Player death sequence / game-over screen
//
// Functions ported from ResidentEvil.exe:
//   set_fading            (0x0047b980) - start a screen fade transition
//   TimeoutDeathFadeOut   (0x00481250) - the fade-out that precedes the
//                                        game-over screen
//   die_state             (0x00481310) - post-death cleanup state machine
//   display_die_screen    (0x00443090) - the "YOU HAVE DIED" screen:
//                                        died.tim drawn as 4 quadrants, the
//                                        camera orbiting the corpse, the
//                                        wavy red text strip (image_update)
//   update_image_fading_  (0x00443500) - copy a 0x10-byte texture template
//                                        into g_TextureDesc
//   image_update          (0x00443550) - draw the wavy text strip: 256
//                                        sine-modulated 1x64 columns
//   _fsin                 (0x0040a960) - fix12 sine: sin(angle * 2pi/32768)
//                                        scaled by 4096
//
// Flow: player HP < 0 sets g_main_state_flags bit 0x1000000 (game_loop).
// The death machine waits 90 frames (the fall animation), then calls
// TimeoutDeathFadeOut, which fades the screen (type 1 = white flash, or
// type 2 in attract/countdown modes) via main_loop's non-suspend fade
// branch. When g_fading_state wraps negative the machine calls die_state,
// which runs display_die_screen: the screen shows the corpse from a
// rotating camera with the died.tim graphic, then fades back out and
// returns to game_loop (which reports end_game_status == 1 -> title_state).
#include "../Globals.h"
#include "../system/AssetPath.h"
#include "FileLoader.h"
#include <math.h>

// Locally-declared helpers (declared here because Globals.h does not cover
// every original symbol; the definitions live in SoundSystem.cpp /
// CmdFunctions.cpp).
extern void play_sound_and_voice_effect(int type, int id);   // SoundSystem.cpp
extern void BuildSndFadeTbl(char distSteps, int fadeType);   // SoundSystem.cpp (0x0047ff90)

// ============================================================================
// 0x10-byte texture template entries (0x004bd2b8 / 0x004bd308)
// Layout matches the byte offsets update_image_fading_ copies:
//   flags(4) screenX(2) screenY(2) width(2) height(2) texU(1) texV(1) tint(2)
// ============================================================================
struct DieScreenTexEntry {
    unsigned int   flags;
    short          screenX;
    short          screenY;
    unsigned short width;
    unsigned short height;
    unsigned char  texU;
    unsigned char  texV;
    unsigned short clutY;
};

// The four died.tim quadrants (0x004bd2b8). 2x2 grid of 160x120 pieces
// covering 320x240, texture sampled from V = 0x40 with CLUT tint 0x1FF
// (matches the pageOffset + 0x1E0 CLUT base stored by LoadTexturePage).
static const DieScreenTexEntry s_dieScreenPieces[4] = {
    { 0x01000000, -160, -120, 160, 120, 0x00, 0x40, 0x01FF },
    { 0x01400000, -160,    0, 160, 120, 0x00, 0x40, 0x01FF },
    { 0x01800000,    0, -120, 160, 120, 0x00, 0x40, 0x01FF },
    { 0x01C00000,    0,    0, 160, 120, 0x00, 0x40, 0x01FF },
};

// image_update's column-strip template (0x004bd308). width/height/texV are
// overwritten at the top of every image_update call.
static DieScreenTexEntry s_dieStripTemplate = {
    0x41000000, -128, -32, 255, 1, 0x00, 0x00, 0x01FF
};

// 0x004bd2b0 - one-shot flag: the first death screen hides joint 1 (the
// head) by clearing its visibility bit, then clears the flag. Shared with the
// hunter's pounce grab, which sets it (Hunter.cpp 0x00417a20); defined in
// Globals.cpp with its image value 0x00006C6C.
extern int DAT_004bd2b0;

// 0x004bd320 / 0x004bd328 - tyrant impale transform: rotation applied to
// joint 2 and the offset applied through joint 0's transform.
// (Non-const: RotMatrix/ApplyMatrixSV take mutable pointers in this port.)
static SVECTOR s_dieTyrantRot = { 1500, 0, 500, 0 };
static SVECTOR s_dieTyrantOff = { 1600, 0, 0, 0 };

// ============================================================================
// _fsin (0x0040a960)
// (int)(sin(angle * (1/4096) * 2pi) * 4096) - fix12 sine; angle 0..4096 spans
// one full cycle. image_update feeds 0..0x10000 = SIXTEEN ripples across the
// 256 columns, so the wave sign flips every 8 columns: the alternating
// up/down bands of strips that slam together ("even strips from the top, odd
// from the bottom"), settling into the 16-ripple wave.
// The 1/4096 literal is 0x004af028 = 0x3F30000000000000 (2^-12); the
// decompile's 0.000244140625 is authoritative over a naive hex read.
// ============================================================================
int _fsin(int angle)
{
    return (int)(sin((double)angle * (1.0 / 4096.0) * 6.2831854820251465) * 4096.0);
}

// ============================================================================
// update_image_fading_ (0x00443500)
// Copies a 0x10-byte template entry into g_TextureDesc. Ground truth from
// the original disassembly (offsets into 0x00be1160):
//   +0x00 flags (u32), +0x04 screenX/screenY (u32), +0x08 width/height (u32),
//   +0x0E texU/texV from entry+0xC, +0x10 clutX/clutY from entry+0xE
//   (clutX zeroed by the SHL), +0x1E scaleY = param.
// ============================================================================
void update_image_fading_(const void* param1, short param2)
{
    const DieScreenTexEntry* entry = (const DieScreenTexEntry*)param1;
    g_TextureDesc.flags         = entry->flags;
    *(unsigned int*)&g_TextureDesc.screenX = *(const unsigned int*)&entry->screenX;
    *(unsigned int*)&g_TextureDesc.width   = *(const unsigned int*)&entry->width;
    g_TextureDesc.texU          = entry->texU;
    g_TextureDesc.texV          = entry->texV;
    g_TextureDesc.clutX         = 0;
    g_TextureDesc.clutY = entry->clutY;
    g_TextureDesc.scaleY        = param2;
}

// ============================================================================
// image_update (0x00443550)
// Draws the wavy red text: 256 columns of a 1x64 strip, each column a sine-
// displaced slice of the died.tim texture. param scales the wave amplitude
// (param^2 >> 13 px) and the brightness offset (4*param^2 + 0x1600).
// The strip is drawn per-column at alternating depth 0/16 - on the death
// screen the base image draws at depth 0xFFA so the columns shimmer above it.
//
// The wave math follows the original disassembly exactly:
//   baseY  = -(uVar1 * 0x40 >> 13)          - one-time baseline (negative = up)
//   wave16 = (short)((uint)(sin*param^2) >> 13) - the low 16 bits of the
//            UNSIGNED shift. For negative sin values the unsigned result wraps
//            past 2^16 and the low 16 bits come out NEGATIVE, so those columns
//            land BELOW the baseline while positive-sin columns sit above it.
//            At the start (param ~59) the wave is ~1700px: half the columns
//            fly off the top, half off the bottom - the two strips that slam
//            together and join - settling to a ~4px ripple at param 3.
// ============================================================================
void image_update(int param_1)
{
    // Mutate the strip template (0x004bd310 = width, 0x004bd312 = height,
    // 0x004bd315 = texV) before every use.
    *(unsigned short*)&s_dieStripTemplate.width  = 1;
    *(unsigned short*)((char*)&s_dieStripTemplate.width + 2) = 0x40;
    *(unsigned char*)((char*)&s_dieStripTemplate.texU + 1) = 0;

    int iVar3 = param_1 * 8 * param_1 * 8;
    // uVar1 = (short)((iVar3 >> 4) + 0x1600) - the +0x1600 is a 16-bit add.
    short uVar1 = (short)((short)((int)(iVar3 + ((iVar3 >> 31) & 0xF)) >> 4) + 0x1600);
    // The baseline computation zero-extends uVar1 (AND EAX,0xffff before SHL 6).
    unsigned int uVar1u = (unsigned short)uVar1;
    int base = (int)((uVar1u * 0x40) >> 0xd);
    short baseY = (short)(-base);   // NEG AX - 16-bit negate

    unsigned int uVar4 = 0;
    int iVar5;
    do {
        update_image_fading_(&s_dieStripTemplate, uVar1);
        g_TextureDesc.screenX = (short)(uVar4 - 0x80);
        g_TextureDesc.texU    = (unsigned char)uVar4;
        g_TextureDesc.flags   = 0x10000000;
        iVar5 = iVar3 + 0x100;
        iVar3  = _fsin(iVar3);
        short wave16 = (short)((unsigned int)(iVar3 * param_1 * param_1) >> 0xd);
        g_TextureDesc.screenY = (short)(baseY - wave16);   // SUB BP, AX
        unsigned int uVar2 = uVar4 & 1;
        uVar4 = uVar4 + 1;
        display_texture(&g_TextureDesc, (unsigned short)(uVar2 << 4), 0xc, 1);
        iVar3 = iVar5;
    } while (iVar5 < 0x10000);
}

// ============================================================================
// set_fading (0x0047b980)
// ============================================================================
void set_fading(int fade_type_id, int fading_counter)
{
    if ((short)g_fading_state < 1) {
        g_main_state_flags |= MSF_FADE_ACTIVE;
        g_fading_counter = (short)fading_counter;
        g_fade_type_id   = (unsigned char)fade_type_id;
    }
}

// ============================================================================
// TimeoutDeathFadeOut (0x00481250)
// Kicks off the death fade. The 0x20000000 flag is cleared so main_loop's
// non-suspend fade branch draws the fading rect and advances g_fading_state
// by the counter each frame:
//   - countdown expired (0xB4): immediate black, fading_state forced to -1
//     so die_state runs on the next game_loop tick
//   - attract mode (0x10000000): fade type 2 (black), 0x800
//   - demo/other (0x80000000): fade type 2 (black), 0x100
//   - normal death: fade type 1 (white flash), 0x100; the positive counter
//     ramps the white overlay up until the state wraps negative as a short,
//     which triggers die_state
// ============================================================================
void TimeoutDeathFadeOut(void)
{
    g_main_state_flags = g_main_state_flags & MSF_DEATH_KEEP_MASK;
    g_message_flags    = (WORD)(g_message_flags & 0xFEFF);
    g_menu_choice_id   = 0;
    g_openMenuFlag     = 0;

    if (g_CountdownTimer == 0xB4) {
        cmd_bgm_stop_all();
        play_sound_and_voice_effect(2, 0);
        Task_sleep(1);
        g_fading_state = 0xFFFF;
        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
        return;
    }
    if ((g_main_state_flags2 & MSF2_ATTRACT_DEMO) != 0) {
        g_fade_type_id   = 2;
        g_fading_counter = 0x800;
        fade_update();
        return;
    }
    if ((g_main_state_flags2 & MSF2_DEATH_VARIANT) != 0) {
        g_fade_type_id   = 2;
        g_fading_counter = 0x100;
        fade_update();
        return;
    }
    g_fade_type_id   = 1;
    g_fading_counter = 0x100;
    fade_update();
}

// ============================================================================
// display_die_screen (0x00443090)
// The game-over screen. Loads usa/data/died.tim into slot 0xC and orients a
// camera on the fallen player, then runs a state machine:
//   0 -> 1: start the sound fade + white-flash fade-in (counter 0xFE80 is
//           negative as a short, so main_loop drives the state to 0x7FFF and
//           decays it over ~85 frames)
//   1 -> 2: when the flash clears, ramp the black rect overlay up
//           (g_rect r/g/b += 3 per frame) and decay the wavy-strip amplitude
//           (image_update(59) .. image_update(4))
//   2 -> 3: hold the calm text for 0x31 frames
//   3 -> 4: fade back out (type 2, 0x180) while the strip re-waves
//   4 -> 5 -> 6: when the fade completes (or the confirm button is pressed),
//           restore image buffers and clean up slot 0xC
// ============================================================================
void display_die_screen(void)
{
    clear_textures();
    LoadFile(GAME_DATA_ROOT "data\\died.tim", g_DataBuffer, 0x20);
    g_TextureBankID = 6;
    LoadTexturePage(g_DataBuffer, 6, 0x1F, 0xC, 0, 0, 0, 2);
    g_imageBufferPtr  = (void*)g_DataBuffer;
    g_imageBufferPtr2 = (void*)((char*)g_DataBuffer + 0x10000);

    g_rect.textureId = 0x60000000;
    g_rect.x = -0xA0;
    g_rect.y = -0x78;
    g_rect.w = 0x140;
    g_rect.h = 0xF0;
    g_rect.r = 0;
    g_rect.g = 0;
    g_rect.b = 0;

    // Build the camera matrix from the player's world translation (0x00443118).
    // The original scatters the position into the rotation rows and t[0] as
    // full dword writes, so replicate it at that granularity.
    int t0 = g_playerEntity.scaMatrixData.localMatrix.t[0];
    int t1 = g_playerEntity.scaMatrixData.localMatrix.t[1];
    int t2 = g_playerEntity.scaMatrixData.localMatrix.t[2];
    *(unsigned int*)&MATRIX_00d22680.m[0][0] = (unsigned int)(t0 + 5000);
    *(unsigned int*)&MATRIX_00d22680.m[0][2] = (unsigned int)(t1 - 7000);
    *(unsigned int*)&MATRIX_00d22680.m[1][1] = (unsigned int)t2;
    *(unsigned int*)&MATRIX_00d22680.m[2][0] = (unsigned int)t0;
    *(unsigned int*)&MATRIX_00d22680.m[2][2] = (unsigned int)(t1 - 1000);
    MATRIX_00d22680.t[0] = t2;
    MatrixToCamera(&MATRIX_00d22680);

    StMask(0, 5);

    // One-shot: hide joint 1 (the head) on the first death screen.
    if (DAT_004bd2b0 != 0) {
        g_playerEntity.jointsStructs[1].flags &= 0xFE;
    }
    DAT_004bd2b0 = 0;

    unsigned char state      = 0;   // local_a   - state machine index
    short         imageIdx   = 0x3B; // sVar4    - wave amplitude index
    unsigned char holdFrames = 0;   // local_9   - hold-state counter
    short         rectColor  = 0;   // local_c   - black overlay alpha

    // The original's state machine stepped at 25fps; this port ticks at the
    // monitor rate (~60fps), so stepping every frame made the strip slam and
    // hold play ~2.4x fast. Step the machine on alternating frames instead
    // (~30fps) while the draws and the main_loop-driven fades keep their rate.
    unsigned char stepTick = 0;

    for (;;) {
        // Per-frame descriptor reset (0x004431bc)
        g_TextureDesc.pivotX = 0;
        g_TextureDesc.pivotY = 0;
        unk_00be1180 = 0;
        g_TextureDesc.colorMulR = 0x80;
        g_TextureDesc.texturePage     = 6;
        g_TextureDesc.colorMulG = 0x80;
        g_TextureDesc.colorMulB = 0x80;
        g_TextureDesc.scaleX    = 0x1000;

        stepTick ^= 1;
        if (stepTick != 0) {
            // State machine STEPS only (the original stepped these at 25fps;
            // this port ticks at the monitor rate, so step on alternating
            // frames ~30fps). The strip DRAW below runs every frame.
            switch (state) {
            case 0:
                // Start the sound fade and the white-flash fade-in.
                BuildSndFadeTbl(0xFD, 0x2B);
                g_fade_type_id   = 1;
                g_fading_counter = 0xFE80;
                fade_update();
                state = 1;
                // fall through
            case 1:
                // Wait for the flash to clear, then start the reveal.
                if ((short)g_fading_state < 0) {
                    state = 2;
                }
                break;
            case 2:
                // Ramp the black overlay up and decay the wave amplitude.
                if (rectColor < 0xFF) {
                    g_rect.r = (unsigned char)rectColor;
                    g_rect.g = (unsigned char)rectColor;
                    g_rect.b = (unsigned char)rectColor;
                }
                rectColor = (short)(rectColor + 3);
                if (0x3F < rectColor) {
                    imageIdx = (short)(imageIdx - 1);
                }
                if (imageIdx < 4) {
                    state = 3;
                }
                break;
            case 3:
                // Hold the settled text.
                holdFrames = (unsigned char)(holdFrames + 1);
                if (0x30 < holdFrames) {
                    state      = 4;
                    holdFrames = 0;
                }
                break;
            case 4:
                // Start the fade-out while the strip re-waves.
                g_fade_type_id   = 2;
                g_fading_counter = 0x180;
                imageIdx         = 4;
                fade_update();
                state = 5;
                break;
            case 5:
                imageIdx = (short)(imageIdx + 1);
                // fall through
            case 6:
                // Fade-out complete -> exit.
                if ((short)g_fading_state < 0) {
                    goto die_screen_done;
                }
                break;
            }
        }

        // Draw the wavy strip every frame at the current amplitude - the
        // image_update calls are DRAWS in the original, only the amplitude
        // index changes on the stepped frames. Without this the strip
        // alternated with blank frames and flickered.
        switch (state) {
        case 2:
        case 3:
        case 4:
        case 5:
            image_update((int)imageIdx);
            break;
        }

        // Skip with the confirm button (0x004432ed): byte 1 bit 0x40 of the
        // remapped dpad word = bit 0x4000.
        if ((((g_PlayerDpadHeld >> 8) & 0x40) != 0) && (state != 0)) {
            g_fading_state = 0xFFFF;
            set_fading(2, 0x800);
            goto die_screen_done;
        }

        // Draw the four died.tim quadrants (0x00443301).
        for (int i = 0; i < 4; i++) {
            update_image_fading_(&s_dieScreenPieces[i], 0x1000);
            display_texture(&g_TextureDesc, 0xFFA, 0xC, 1);
        }
        draw_rect(&g_rect, 4000, 1);

        // Tyrant (enemy id 8) impale pose: hide joints 0/2, keep the middle.
        if ((g_EnemiesList[0].id == 8) && (g_deathAnimationFlag != 0)) {
            g_playerEntity.zoneFlags &= 0x7F;
            g_playerEntity.jointsStructs[0].flags = 0x83;
            g_playerEntity.jointsStructs[2].flags = 0x83;
            g_playerEntity.position.pad = 0;
            g_playerEntity.speed.x      = 0;
            g_playerEntity.jointsStructs[0].rotation.x = 0;
            g_playerEntity.jointsStructs[0].rotation.y = 0;
            g_playerEntity.jointsStructs[0].rotation.z = 0;
        }

        // Orbit the camera around the corpse.
        g_playerEntity.directionAngle = (short)(g_playerEntity.directionAngle + 8);
        Task_sleep(1);

        if (g_playerEntity.flags != 0) {
            ENTITY = (Entity*)&g_playerEntity;
            EntityComputeJointWorldMatrices(g_playerEntity.unk_ca);
            if ((g_EnemiesList[0].id == 8) && (g_deathAnimationFlag != 0)) {
                RotMatrix(&s_dieTyrantRot, &g_playerEntity.jointsStructs[2].transform);
                SVECTOR sVec;
                ApplyMatrixSV(&g_playerEntity.jointsStructs[0].transform, &s_dieTyrantOff, &sVec);
                g_playerEntity.jointsStructs[2].transform.t[0] = sVec.x;
                g_playerEntity.jointsStructs[2].transform.t[1] = sVec.y;
            }
            EntityApplyLookAtRotation();
            render_entity((Entity*)&g_playerEntity);
        }
    }

die_screen_done:
    // 0x004434af: restore image buffers and free the texture slot.
    StMask(0, 3);
    g_imageBufferPtr  = g_imageBufferDataA;
    g_imageBufferPtr2 = g_imageBufferDataB;
    cleanup_texture_slot(0xC);
}

// ============================================================================
// die_state (0x00481310)
// Runs after the death fade completes: shows the death screen (unless in
// attract/demo mode), stops the sound fade, clears the framebuffer, then
// hands back to game_loop which returns end_game_status == 1 -> title_state.
// ============================================================================
void die_state(void)
{
    g_SpecialRoomLightState = 0xFFFF;
    g_menu_choice_id        = 0;
    g_spriteAnimIntensity   = 0;
    g_main_state_flags      = (g_main_state_flags & ~(MSF_SCREEN_MODE_MASK | MSF_INTENSITY_RAMP)) | MSF_SCREEN_STANDALONE;

    // Skip the death screen entirely in attract/demo mode (0x90000000).
    if ((g_main_state_flags2 & (MSF2_DEATH_VARIANT | MSF2_ATTRACT_DEMO)) == 0) {
        display_die_screen();
    }

    StMask(0, 1);
    Task_sleep(1);
    // vram_clr(0, 0, 0x140, 0x1E0): PS1 leftover, returns immediately in this
    // build (0x00412370) - call dropped

    // Wait for the death sound fade to finish.
    while (g_SndFadeType != 0) {
        Task_sleep(1);
    }

    // FUN_0047eb60: empty in the original (single RET at 0x0047eb60)
    g_BGM_STATE  = 0xFF;
    g_DemoTimerCur = 0;
}
