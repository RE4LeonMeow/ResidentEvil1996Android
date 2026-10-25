// main_loop.cpp - Main game loop
// Original function: main_loop at 0x00428eb0 (Ghidra)
// Adapted from Ghidra decompilation
#include "Globals.h"
#include "../system/AssetPath.h"

// 0x00470750 - declared in SpriteRenderer.h; the signature must match that
// definition exactly (see the stub-overload note on display_room_camera_bg).
extern void Display_SetParams(int param1, int param2);

// Sprite animation/screen tint state (original addresses 0x00be41d0-0x00be41d4)
// These are global in the original binary; game_start resets g_spriteAnimIntensity
// to prevent stale white-flash overlays from the character-select fade transition.
int g_spriteAnimActive;             // 0x00be41d0
int g_spriteAnimR;                  // 0x00be41d1
int g_spriteAnimG;                  // 0x00be41d2
int g_spriteAnimB;                  // 0x00be41d3
short g_spriteAnimIntensity;        // 0x00be41d4

// g_pressF9Flag is defined in Globals.cpp and declared in Globals.h; the old
// file-local `static` here shadowed it, so window_proc's clear had no effect
// on this file (GCC rejects the shadowing outright).
static int g_fading_007d9048;       // 0x007d9048
static int g_fading_007d904c;       // 0x007d904c
int g_MaxHealthDisplayFlag;  // 0x00d227c0 (also written by the effect system)
RectDrawDesc g_FadingRect = { 0x60000000, -160, -120, 320, 240, 0, 0, 0 };  // 0x004ba720
static RectDrawDesc g_LetterboxBarTop    = { 0x60000000, -164, -130, 328,  38, 0, 0, 0 };  // 0x004ba730
static RectDrawDesc g_LetterboxBarBottom = { 0x60000000, -164,   92, 328,  38, 0, 0, 0 };  // 0x004ba740

// ============================================================================
// main_loop - Main game update/rendering (0x00428eb0)
// Called once per frame by the outer Win32 message loop. No internal loop.
// ============================================================================
int main_loop(void)
{
    SetFrameRateMode(g_bGameActive != 0);

    // 0x00428ed1: Initialize game on first run
    if ((char)init_game_flag == 0) {
        init_and_start_game();
        init_game_flag = 1;
    }

    // 0x00428ee0: Frame startup - input update
    InputUpdate();
    PlayerPad_Update();

    // 0x00428eff: Check for special key combination (F9/F10/F11 scan codes)
    // Only fires when no message is currently displayed (bit 0x80 of g_menu_choice_id = message active)
    if ((g_padEdgeDetectedWord != 0) && (((g_lastScanCodeOrMsgID == 0x5b || (g_lastScanCodeOrMsgID == 0x5c)) || (g_lastScanCodeOrMsgID == 0x5d)))) {
        DAT_00d91bc8 = 1;
        g_menu_choice_id = 0;
    }

    if ((g_main_state_flags2 & MSF2_ATTRACT_DEMO) == 0) {
        if (g_isSideWinderConnected) {
            g_PlayerPadHeld |= 0x800;
            g_button_pressed_id |= 0x800;
            g_isSideWinderConnected = FALSE;
        }
        if (g_isPaused) {
            WORD savedButtons = (WORD)g_button_pressed_id;
            g_button_pressed_id |= 0x900;
            g_isPaused = FALSE;
            g_PlayerPadHeld = savedButtons | 0x900;
        }
    } else {
        g_isSideWinderConnected = FALSE;
        g_isPaused = FALSE;
    }

    // Declared before the _post goto so the jump cannot cross their
    // initialisations (GCC rejects that; MSVC allowed it). Assigned at their
    // use sites below.
    bool shakeActive;
    BYTE intensity;

    if ((g_main_state_flags & MSF_FMV_REQUEST) != 0) {
        g_window_rect.h = 240;
        g_main_state_flags &= ~MSF_FMV_REQUEST;
        g_window_rect.w = 320;
        g_CurrentFMVID = (int)g_selectedFmvId;
        g_bMCINotifyEnabled = TRUE;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_FmvCharacterId = (int)g_SelectedCharactedId;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.y = -g_ScreenOffsetY;
        draw_rect(&g_window_rect, 0, 0);
        g_bMCINotifyFlag = TRUE;
        StMask(1, 0);
        goto _post;
    }

    if ((g_AttractModeIdleTimer == 0) && ((g_main_state_flags & MSF_VOICE_PLAYING) == 0) &&
        (g_displayReturnToTitleScreen_Flag != 0)) {

        // F9 return-to-title prompt. The Japanese build (Biohazard.exe
        // 0x00484130, strings at 0x004cb2d8/0x004cb2d4/0x004cb2c0) shows a
        // dialog instead of the USA sentence: the action title at y=100, a
        // large "F9" at (0x8c,0x78) and the cancel hint at (0x1c,0xa0).
        if (GetAssetVersion() != 0) {
            sprintf(PRINT_TEXT_BUFFER, "RETURN TITLE");
            PrintText8x14(0x46, 100, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "F9");
            PrintText8x14(0x8c, 0x78, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "(CANCEL: OTHER KEY)");
            PrintText8x14(0x1c, 0xa0, 128, 1);
        } else {
            sprintf(PRINT_TEXT_BUFFER, "Press F9 to abort game and return to");
            PrintText8x14(16, 100, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "title screen.");
            PRINT_TEXT_BUFFER[0x0C] = 0x9d;
            PrintText8x14(16, 116, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "Or any other key to continue game.");
            PRINT_TEXT_BUFFER[0x21] = 0x9d;
            PrintText8x14(16, 150, 128, 1);
        }

        g_window_rect.w = 320;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.h = 240;
        g_window_rect.y = -g_ScreenOffsetY;
        draw_rect(&g_window_rect, 100, 1);

        if (g_pressF9Flag == 0) {
            PauseSounds();
        }
        g_pressF9Flag = 1;

        if (g_PlayerPadHeld != 0) {
            g_displayReturnToTitleScreen_Flag = FALSE;
            ResumePausedSounds();
        }
        goto _post;
    }

    if (g_displayExitGameScreen_flag != 0) {
        // F9 exit-to-desktop prompt. The Japanese build shows the same dialog
        // with "EXIT GAME" at x=0x5a instead of "RETURN TITLE" at x=0x46.
        if (GetAssetVersion() != 0) {
            sprintf(PRINT_TEXT_BUFFER, "EXIT GAME");
            PrintText8x14(0x5a, 100, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "F9");
            PrintText8x14(0x8c, 0x78, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "(CANCEL: OTHER KEY)");
            PrintText8x14(0x1c, 0xa0, 128, 1);
        } else {
            sprintf(PRINT_TEXT_BUFFER, "Press F9 to exit game and return");
            PrintText8x14(16, 100, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "to desktop.");
            PRINT_TEXT_BUFFER[0x0A] = 0x9d;
            PrintText8x14(16, 116, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "Or any other key to return to");
            PrintText8x14(16, 150, 128, 1);

            sprintf(PRINT_TEXT_BUFFER, "title screen.");
            PRINT_TEXT_BUFFER[0x0C] = 0x9d;
            PrintText8x14(16, 166, 128, 1);
        }

        g_window_rect.w = 320;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.h = 240;
        g_window_rect.y = -g_ScreenOffsetY;
        draw_rect(&g_window_rect, 100, 1);

        if (g_PlayerPadHeld != 0) {
            g_displayExitGameScreen_flag = FALSE;
        }
        goto _post;
    }

    if (g_pressF9Flag != 0) {
        g_pressF9Flag = 0;
        ResumePausedSounds();
    }

    UpdateDemoTimer();

    if ((g_main_state_flags & MSF_PANNING_RESET) != 0) {
        ResetScreenPanning();
    }
    if (g_SndFadeType != 0) {
        UpdateSoundFadeState();
    }
    if (g_SndRampFramesLeft != 0) {
        UpdateSoundDecay();
    }

    // 0x004297e5-0x004297f6: msf2 bit 1 is the screen-shake enable (SCD bank 5,
    // byte offset 4, bit 0x02 - set by the boulder tunnel's event scripts), and
    // the gate is `CMP byte ptr [0x00be41c1], BL` - byte 1 of msf only, i.e. no
    // menu/message/got-item state. Testing `g_main_state_flags >> 8` instead of
    // `(g_main_state_flags >> 8) & 0xFF` folded in bits 16-31, which always hold
    // the "game loop active"/"game initialized" bits during play, so the shake
    // never ran once.
    shakeActive = ((g_main_state_flags2 & MSF2_SCREEN_SHAKE) != 0) &&
                  ((g_main_state_flags & MSF_MENU_BYTE) == 0);

    // The offsets have to be rolled BEFORE TaskScheduler_Update, not at the end
    // of the frame where the original rolls them (0x004297f6). The original
    // transforms everything it draws into the ordering table during the task
    // pass, so one roll per frame feeds the whole next frame uniformly. This
    // port only bakes the offset in at queue time for the 2D sprites and the
    // room masks (AddSprite/draw_texture add g_ScreenOffsetX); the 3D pass and
    // the background quad read g_SubpixelOffsetX/g_displayImageOriginX live at
    // render time. Rolling at the end therefore cut the frame in half - the
    // masks were drawn with the previous roll while the entities and the
    // background used the new one, which is the mask/background desync.
    if (shakeActive) {
        ApplyScreenShake();
    }

    if ((g_main_state_flags & MSF_FADE_ACTIVE) != 0) {
        // Fade transition in progress
        if (g_fading_state < 0) {
            g_fading_007d9048 = 0;
            g_FadingRect.textureId = (unsigned int)(g_fade_type_id | 4) << 28;
            g_fading_state = 0;
            if (g_fading_counter < 1) {
                g_fading_state = 0x7FFF;
            }
            g_fading_007d904c = 0;
            if ((g_TasksTable[0].state & 0x40) == 0) {
                Task_suspend(0);
            } else {
                g_fading_007d904c = g_TasksTable[0].state;
            }
        }

        g_FadingRect.b = (BYTE)(g_fading_state >> 7);
        g_fading_007d9048 += g_fading_counter;

        if ((g_fading_007d9048 & 0xFC00) == 0x400) {
            g_FadingRect.b = 8;
        }
        g_fading_007d9048 &= 0x2FF;

        g_FadingRect.r = g_FadingRect.b;
        g_FadingRect.g = g_FadingRect.b;

        draw_rect(&g_FadingRect, 0, 0);

        TaskScheduler_Update();

        g_fading_state += g_fading_counter;

        if (g_fading_state < 0) {
            g_fading_counter = 0;
            g_main_state_flags &= ~MSF_FADE_ACTIVE;
            g_fading_state = 0;
            if (g_fading_007d904c == 0) {
                Task_Resume(0);
            }
            goto _fade_done;
        }
    } else {
_fade_done:
        g_spriteAnimActive ^= 1;
        g_MaxHealthDisplayFlag = 0x7FFFFFFF;

        TaskScheduler_Update();

        if ((g_menu_choice_id & 0x80) != 0) {
            UpdateMessageDisplay();
        }

        if (g_fading_state >= 0) {
            g_FadingRect.textureId = (unsigned int)(g_fade_type_id | 4) << 28;
            g_FadingRect.r = (BYTE)(g_fading_state >> 7);
            g_fading_state += g_fading_counter;

            int blend;
            if ((g_playerEntity.health < 0) && (g_MaxHealthDisplayFlag != 0x7FFFFFFF)) {
                blend = 100;
            } else {
                blend = 0;
            }

            g_FadingRect.g = g_FadingRect.r;
            g_FadingRect.b = g_FadingRect.r;

            draw_rect(&g_FadingRect, blend, 0);
        }
    }

    // 0x004297e0: Sprite animation intensity
    if ((g_main_state_flags & MSF_INTENSITY_RAMP) != 0) {
        if ((unsigned char)g_spriteAnimIntensity < 0xF0) {
            g_spriteAnimIntensity += 16;
        }
    } else if ((unsigned char)g_spriteAnimIntensity > 0x0F) {
        g_spriteAnimIntensity -= 16;
    }

    intensity = (BYTE)g_spriteAnimIntensity;
    g_LetterboxBarTop.r = intensity;
    g_LetterboxBarTop.g = intensity;
    g_LetterboxBarTop.b = intensity;
    g_LetterboxBarBottom.r = intensity;
    g_LetterboxBarBottom.g = intensity;
    g_LetterboxBarBottom.b = intensity;

    // Original PS1 GPU POLY_F4 screen-tint primitives (dead code in D3D11 port).
    // In the original binary these wrote to the PS1 ordering table for screen
    // color tinting. The modern port uses draw_rect() with the letterbox bars
    // instead. Preserved here for reference; writing to the array crashes on
    // modern Windows due to linker section placement.
    // Poly_F4_ARRAY_004ba750[g_spriteAnimActive].r0 = intensity;
    // Poly_F4_ARRAY_004ba750[g_spriteAnimActive].g0 = intensity;
    // Poly_F4_ARRAY_004ba750[g_spriteAnimActive].b0 = intensity;
    // Poly_F4_ARRAY_004ba750[g_spriteAnimActive + 2].r0 = intensity;
    // Poly_F4_ARRAY_004ba750[g_spriteAnimActive + 2].g0 = intensity;
    // Poly_F4_ARRAY_004ba750[g_spriteAnimActive + 2].b0 = intensity;

    if (g_spriteAnimIntensity == 0xF0) {
        g_LetterboxBarTop.r = 0xFF;
        g_LetterboxBarTop.g = 0xFF;
        g_LetterboxBarTop.b = 0xFF;
        g_LetterboxBarBottom.r = 0xFF;
        g_LetterboxBarBottom.g = 0xFF;
        g_LetterboxBarBottom.b = 0xFF;
    }

    if ((g_main_state_flags & (MSF_ROOM_TRANSITION | MSF_MENU_ACTIVE)) == 0) {
        if (g_spriteAnimIntensity != 0) {
            if ((g_stageId == STAGE_LABORATORY) && (g_roomId == ROOM_MAIN_LAB) && (g_roomCameraId == 5)) {
                draw_rect(&g_LetterboxBarTop, 0, 0);
                draw_rect(&g_LetterboxBarBottom, 0, 0);
            } else {
                draw_rect(&g_LetterboxBarTop, 0x28, 0);
                draw_rect(&g_LetterboxBarBottom, 0x28, 0);
            }
        }

        if (g_SpecialRoomLightState >= 0) {
            g_FadingRect.textureId = (unsigned int)(g_SpecialRoomLightR | 4) << 28;

            BYTE lightMask = (BYTE)(g_SpecialRoomLightState >> 7);
            g_FadingRect.r = g_SpecialR1 & lightMask;
            g_FadingRect.g = g_SpecialG1 & lightMask;
            g_FadingRect.b = lightMask & g_SpecialB1;

            // Stage 3, Room 5, Camera 3: double color components
            if ((g_stageId == STAGE_COURTYARD) && (g_roomId == ROOM_FOUNTAIN) && (g_roomCameraId == 3)) {
                if ((g_FadingRect.r & 0x80) == 0) {
                    g_FadingRect.r = g_FadingRect.r << 1;
                } else {
                    g_FadingRect.r = 0xFF;
                }
                if ((g_FadingRect.g & 0x80) == 0) {
                    g_FadingRect.g = g_FadingRect.g << 1;
                } else {
                    g_FadingRect.g = 0xFF;
                }
                if ((g_FadingRect.b & 0x80) == 0) {
                    g_FadingRect.b = g_FadingRect.b << 1;
                } else {
                    g_FadingRect.b = 0xFF;
                }
            }

            g_SpecialRoomLightState += g_SpecialRoomLightDelta;

            if ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_CONTROL_ROOM)) {
                draw_rect(&g_FadingRect, 0x14, 0);
                int blend2;
                if ((g_roomCameraId == 4) || (g_roomCameraId == 3)) {
                    blend2 = 0x14;
                } else {
                    blend2 = 0x31;
                }
                draw_rect(&g_FadingRect, blend2, 0);
            } else {
                bool useSpecialFade = true;

                if ((g_stageId == STAGE_COURTYARD) && (useSpecialFade = (g_roomId != ROOM_COURTYARD_GARDEN), g_roomId == ROOM_WATER_GATE)) {
                    useSpecialFade = false;
                }
                if (g_stageId == STAGE_COURTYARD) {
                    if (g_roomId == ROOM_FALLS) {
                        useSpecialFade = false;
                    }
                    if ((g_roomId == ROOM_FOUNTAIN) && (g_roomCameraId == 0)) {
                        useSpecialFade = false;
                    }
                }
                if (g_stageId == STAGE_COURTYARD) {
                    if ((g_roomId == ROOM_BOULDER_2_PASSAGE) && (g_roomCameraId == 1)) {
                        useSpecialFade = false;
                    }
                    if ((g_roomId == ROOM_ELEVATOR_TO_LABORATORY) && (g_roomCameraId == 0)) {
                        useSpecialFade = false;
                    }
                }
                if ((g_stageId == STAGE_LABORATORY) && (g_roomId == ROOM_LABORATORY_ENTRY)) {
                    useSpecialFade = false;
                }

                if (useSpecialFade) {
                    draw_rect(&g_FadingRect, 0x31, 0);
                }
            }
        }
    }

    // 0x00429dc0: Screen state flags
    if ((g_main_state_flags & MSF_SCREEN_STANDALONE) != 0) {
        g_window_rect.w = 320;
        g_window_rect.textureId = 0;
        g_window_rect.r = g_spriteAnimR;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.g = g_spriteAnimG;
        g_window_rect.h = 240;
        g_window_rect.y = -g_ScreenOffsetY;
        g_window_rect.b = g_spriteAnimB;
        SetScreenReadyWithDebugColor(g_spriteAnimR, g_spriteAnimG, g_spriteAnimB);
    } else if ((g_main_state_flags & MSF_SCREEN_REBUILD) != 0) {
        if ((g_main_state_flags2 & MSF2_SCREEN_BORDER) == 0) {
            ResetScreenAndRebuildSprites(g_spriteAnimActive == 0 ? 0xF0 : 0);

            if (shakeActive) {
                // ResetScreenAndRebuildSprites just re-centred the screen offset
                // and put the display image back at (0,0), both of which the
                // render-time readers below would otherwise pick up. Re-apply
                // this frame's roll - the same values the masks queued during
                // TaskScheduler_Update already baked in - so the whole frame
                // agrees. No re-roll here: that happened before the task pass.
                SetScreenOffset(g_ScreenShakeOffsetX + 160, g_ScreenShakeOffsetY + 120);

                // The room background is the display image, not a sprite, so
                // SetScreenOffset does not touch it - only its own origin does.
                // The original moves that origin in ApplyShakeAndRebuildSprites
                // (0x0045ab72), but that whole branch sits behind msf2 bit 2,
                // the "background inset to 316x236 so the shake has 2px of
                // margin" mode, and nothing in the retail exe ever sets msf2
                // bit 2 - so its background never moved. No inset is needed
                // here: OT_InsertPrimitive shifts the sampled window instead of
                // the full-screen quad, so nothing can be uncovered.
                Display_SetParams(g_ScreenShakeOffsetX, g_ScreenShakeOffsetY);
            }
        } else {
            ApplyShakeAndRebuildSprites();
        }
    }

    // (0x004297f6's ApplyScreenShake call lives at the top of the frame here -
    // see the note next to it.)

    if (g_ScreenAccessCheck != 0) {
        SetScreenReady(1);
    }

    // empty_483510(): returns 0 in the original - call dropped
    UpdateMusicWaitState();

_post:
    // 0x0042a060: FMV cleanup on state change
    if ((g_main_state_flags & MSF_FMV_REQUEST) != 0) {
        ResetFmvRenderState();
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.y = -g_ScreenOffsetY;
        g_window_rect.w = 320;
        g_window_rect.h = 240;
        draw_rect(&g_window_rect, 0, 0);
        StMask(1, 1);
    }

    // 0x0042a0d0: Frame timing + present governor
    FrameRateGovernor();
    g_gameTimerSnapshot = Game_timer;
    DAT_004d46d4 = Game_timer;

    return 1;
}
