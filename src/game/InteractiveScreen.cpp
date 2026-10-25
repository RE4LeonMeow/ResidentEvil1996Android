// InteractiveScreen.cpp - Numeric passcode panel interaction
// Decompiled from Ghidra. The panel is used by the interactive-screen path in
// room4080 and is shared by the Jill/Rebecca room variants.
#include "../Globals.h"
#include "BioCard.h"
#include "PrintText.h"
#include <cstdio>

extern void Flg_on(int baseAddr, unsigned int bitIndex);              // 0x00473ef0
extern void FUN_00473f10(int* baseAddr, unsigned int bitIndex);       // 0x00473f10 - Flg_off

// The panel lives on its own room camera. Bit SCENARIO_FLAG_INTERACTIVE_SCREEN of g_ScenarioFlags (0x00be98c0)
// is the "an interactive screen is up" gate that check_and_display_interactive_screen
// polls; the panel is responsible for clearing it when it is dismissed.

// 0x004ba960..0x004ba998 - Initial lit-state tables selected by the room's
// interactive-screen mode and player character. Each table has nine entries,
// one for each room sprite in the 3x3 panel.
static const unsigned char s_passcodePanelInitialStates[4][9] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // 0x004ba960
    { 1, 0, 0, 1, 1, 0, 1, 0, 1 }, // 0x004ba970
    { 0, 1, 0, 1, 1, 1, 0, 0, 1 }, // 0x004ba980
    { 1, 0, 1, 0, 1, 1, 0, 0, 1 }, // 0x004ba990
};

// 0x004ba9a0 - Success animation delays. The zero terminator advances the
// animation to its final state after the last panel sprite is removed.
static const unsigned char s_passcodePanelAnimationDelays[18] = {
    0, 30, 1, 25, 2, 16, 5, 8, 8, 6, 7, 4, 6, 2, 3, 1, 4, 0
};

static unsigned char passcode_panel_pad_held(void)
{
    // 0x0042a230: the original reads byte 1 of g_PlayerPadHeld.
    return (unsigned char)(g_PlayerPadHeld >> 8);
}

static unsigned char passcode_panel_pad_pressed(void)
{
    // 0x0042a2a2: the original reads byte 1 of the remapped edge state.
    return (unsigned char)(g_PlayerDpadPressed >> 8);
}

// 0x0042a0f0 - Initialize the numeric panel and its room-sprite state.
static void passcode_panel_init(void)
{
    g_labSlidesFuncIndex++;
    g_labSlidesAnimState = 0;
    g_interactiveScreenSavedCameraId = g_roomCameraId;
    g_roomCameraId = 5;

    // 0x0042a11d: RDT + 0x198 is camera 5's fov field. The camera records start
    // at RDT + 0x94 and are 0x2c bytes each (see Room_SetupCamera, 0x00462982),
    // so 0x94 + 5*0x2c + 0x28 == 0x198. The panel close-up is drawn with a fixed
    // fov of 0xc0 regardless of what the RDT shipped.
    if (g_RdtPointer != NULL) {
        *(unsigned int*)((unsigned char*)g_RdtPointer + 0x198) = 0xc0;
    }

    // 0x0042a12c: this is what actually puts the panel on screen. It walks the
    // switch-zone table to camera 5's group and calls cut_set(), which reloads
    // the camera sprite list, the camera transform and the background image.
    // Without it the room stays on the camera the player walked in on and no
    // panel artwork is ever displayed.
    display_room_camera_bg();

    g_message_flags &= 0xfeb0;
    g_labSlidesScrollX = 0;
    g_labSlidesScrollY = 0;

    // 0x0042a137/0x0042a17c: these two checks read g_ScenarioFlags (0x00be98c0),
    // NOT the g_SysFlags bank (0x00be41c8) that check_and_display_interactive_screen
    // uses to pick the screen type. The bit numbers collide, the banks do not.
    const unsigned char* initialStates = s_passcodePanelInitialStates[0];
    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_PANEL_VARIANT_A) != 0) {
        // 0x0042a17c..0x0042a1a2: the alternate interactive screen mode.
        initialStates = (g_playerEntity.id == 1)
            ? s_passcodePanelInitialStates[2]
            : s_passcodePanelInitialStates[0];
    } else if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_PANEL_VARIANT_B) != 0) {
        initialStates = (g_playerEntity.id == 1)
            ? s_passcodePanelInitialStates[3]
            : s_passcodePanelInitialStates[1];
    }

    for (int i = 0; i < 9; ++i) {
        g_passcodePanelSprites[i] = initialStates[i];
    }
    g_passcodePanelActive = 0;
    set_message_display(0x0e, 0xff);
}

// 0x0042a490 - Finish/cancel the numeric panel interaction.
static void passcode_panel_finish(void)
{
    g_message_flags |= 0x014f;
    g_roomCameraId = g_interactiveScreenSavedCameraId;
    display_room_camera_bg();
    g_passcodePanelActive = 0;
    // 0x0042a4b6 calls 0x00473f10 (Flg_off), not 0x00473ef0 (Flg_on): the panel
    // CLEARS the interactive-screen gate to hand control back to the room. Setting
    // it instead left check_and_display_interactive_screen re-entering this
    // function every frame, so the panel never closed.
    FUN_00473f10((int*)g_ScenarioFlags, SCENARIO_FLAG_INTERACTIVE_SCREEN);
}

// 0x0042a230 - Navigate the 3x3 panel and dispatch confirm/cancel input.
static void passcode_panel_input(void)
{
    unsigned char held = passcode_panel_pad_held();

    if ((held & 0x10) != 0 && g_labSlidesScrollY > 0) {
        --g_labSlidesScrollY;
    }
    if ((held & 0x40) != 0 && g_labSlidesScrollY < 2) {
        ++g_labSlidesScrollY;
    }
    if ((held & 0x80) != 0 && g_labSlidesScrollX > 0) {
        --g_labSlidesScrollX;
    }
    if ((held & 0x20) != 0 && g_labSlidesScrollX < 2) {
        ++g_labSlidesScrollX;
    }

    unsigned char pressed = passcode_panel_pad_pressed();
    if ((pressed & 0x40) != 0) {
        ++g_labSlidesAnimState;
    }
    if ((pressed & 0x80) != 0) {
        ++g_labSlidesFuncIndex;
        g_labSlidesAnimState = 0;
    }
}

// 0x0042a2d0 - Toggle the selected tile and its orthogonal neighbours.
static void passcode_panel_toggle(void)
{
    play_sfx(2, 0x17, 0);

    const int row = g_labSlidesScrollY;
    const int column = g_labSlidesScrollX;
    const int index = row * 3 + column;
    g_passcodePanelSprites[index] ^= 1;
    if (row > 0) g_passcodePanelSprites[index - 3] ^= 1;
    if (row < 2) g_passcodePanelSprites[index + 3] ^= 1;
    if (column > 0) g_passcodePanelSprites[index - 1] ^= 1;
    if (column < 2) g_passcodePanelSprites[index + 1] ^= 1;

    int litCount = 0;
    for (int i = 0; i < 9; ++i) {
        litCount += g_passcodePanelSprites[i];
    }

    if (litCount == 9) {
        g_labSlidesAnimState = 3;
        g_passcodePanelAnimationState = 0;
        g_passcodePanelTimer = 30;
        g_passcodePanelPatternIndex = 0;
        g_passcodePanelActive = 0;
    } else {
        g_labSlidesAnimState = 1;
    }
}

// 0x0042a3b0 - Start the panel success removal animation.
static void passcode_panel_animation_start(void)
{
    // 0x0042a3b5: the original decrements unconditionally and only then tests
    // for zero, so an already-expired timer wraps instead of re-firing.
    --g_passcodePanelTimer;
    if (g_passcodePanelTimer == 0) {
        g_passcodePanelAnimationState = 1;
        g_passcodePanelTimer = 1;
        g_passcodePanelPatternIndex = 0;
    }
}

// 0x0042a3e0 - Remove panel sprites in the original success order.
static void passcode_panel_animation_step(void)
{
    // 0x0042a3e5: unconditional decrement, matching the original.
    --g_passcodePanelTimer;
    if (g_passcodePanelTimer != 0) {
        return;
    }

    const unsigned int patternIndex = g_passcodePanelPatternIndex;
    if (patternIndex < 18) {
        g_passcodePanelSprites[s_passcodePanelAnimationDelays[patternIndex]] = 0;
    }
    // The original table interleaves sprite IDs and their delays. It advances
    // once after selecting the sprite and once more after loading the delay.
    ++g_passcodePanelPatternIndex;
    const unsigned int nextIndex = g_passcodePanelPatternIndex;
    g_passcodePanelTimer = nextIndex < 18
        ? s_passcodePanelAnimationDelays[nextIndex]
        : 0;
    ++g_passcodePanelPatternIndex;
    if (g_passcodePanelTimer == 0) {
        ++g_passcodePanelAnimationState;
        g_passcodePanelTimer = 30;
    }
}

// 0x0042a450 - Complete the success animation and signal the room script.
static void passcode_panel_animation_finish(void)
{
    // 0x0042a455: unconditional decrement, matching the original.
    --g_passcodePanelTimer;
    if (g_passcodePanelTimer == 0) {
        ++g_labSlidesFuncIndex;
        play_sfx(2, 0x18, 0);
        // 0x0042a477 pushes 0x00be98c0 - This is
        // the bit the room script waits on to unlock the door.
        Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_PANEL_SOLVED);
    }
}

// 0x0042a1f0 - Wait for the panel's introductory message to close.
static void passcode_panel_wait_message(void)
{
    if ((g_menu_choice_id & 0x80) != 0) {
        return;
    }

    if (g_menu_choice_id != 0) {
        g_labSlidesFuncIndex = 2;
        g_labSlidesAnimState = 0;
    } else {
        g_labSlidesAnimState = 1;
        g_passcodePanelActive = 1;
    }
}

// 0x0042a1d0 - Panel input/animation state dispatcher.
static void passcode_panel_update(void)
{
    switch (g_labSlidesAnimState) {
    case 0:
        passcode_panel_wait_message();
        break;
    case 1:
        passcode_panel_input();
        break;
    case 2:
        passcode_panel_toggle();
        break;
    case 3:
        switch (g_passcodePanelAnimationState) {
        case 0: passcode_panel_animation_start(); break;
        case 1: passcode_panel_animation_step(); break;
        default: passcode_panel_animation_finish(); break;
        }
        break;
    default:
        break;
    }
}

// 0x0042a4c0 - Draw the numeric panel cursor and its 3x3 tile labels.
static void passcode_panel_draw(void)
{
    if (g_passcodePanelActive == 0) {
        return;
    }

    // The frame is nine '$' glyphs wide: the left edge starts at x=0x7b and the
    // right edge glyph sits at 0xbb, so the bars span 0x7b..0xc3 exactly.
    //
    // 0x0042a4e3 reads an uninitialised stack slot for this x (the same slot the
    // row loop later writes row*3 into) and feeds it through `x*8 + 0x7b`. The
    // only value that lines the bars up with the columns is 0, which is what the
    // original's frame holds in practice, so the port uses 0x7b directly instead
    // of reproducing a garbage read.
    sprintf(PRINT_TEXT_BUFFER, "$$$$$$$$$");
    PrintText8x14(0x7b, 0x8c, 0, 1);
    PrintText8x14(0x7b, 0xc4, 0, 1);

    for (int row = 0; row < 3; ++row) {
        // 0x004ba9ec is "$$" (two glyphs), not one: the left edge is 16px wide
        // so it reaches from 0x7b up to the first tile at 0x8b.
        sprintf(PRINT_TEXT_BUFFER, "$$");
        const short y = (short)((row + 0x0b) * 0x0e);
        PrintText8x14(0x7b, y, 0, 1);

        sprintf(PRINT_TEXT_BUFFER, "$");
        PrintText8x14(0xbb, y, 0, 1);

        for (int column = 0; column < 3; ++column) {
            const unsigned char color =
                (g_labSlidesScrollX == column && g_labSlidesScrollY == row) ? 1 : 3;
            sprintf(PRINT_TEXT_BUFFER, "%c$", (char)(row * 3 + column + 0x31));
            PrintText8x14((short)(column * 0x10 + 0x8b), y, color, 1);
        }
    }
}

// 0x0042a610 - Apply the nine panel states to room sprites 1..9.
static void passcode_panel_update_room_sprites(void)
{
    for (int i = 0; i < 9; ++i) {
        if (g_passcodePanelSprites[i] == 0) {
            RoomSpr_SetInactive((char)(i + 1));
        } else {
            RoomSpr_SetActive((char)(i + 1));
        }
    }
}

// 0x0042a0d0 - Display/update the numeric passcode panel.
static void display_passcode_panel(void)
{
    switch (g_labSlidesFuncIndex) {
    case 0:
        passcode_panel_init();
        break;
    case 1:
        passcode_panel_update();
        break;
    case 2:
        passcode_panel_finish();
        break;
    default:
        break;
    }
    passcode_panel_draw();
    passcode_panel_update_room_sprites();
}

// 0x0042a030 - Check and display the active interactive room screen.
void check_and_display_interactive_screen(void)
{
    const unsigned int interactive =
        Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_INTERACTIVE_SCREEN);
    if (interactive != 0) {
        if (g_labSlidesState == 0) {
            // The original clears the first four bytes of the shared state
            // block when an interactive screen is entered. All four: the
            // fourth is the computer terminal's innermost sub-state, and
            // leaving it stale resumes the terminal mid-sequence.
            g_labSlidesFuncIndex = 0;
            g_labSlidesAnimState = 0;
            g_passcodePanelAnimationState = 0;
            g_labSlidesSubState2 = 0;
        }

        // 0x0042a05e: the SysFlags bit picks which screen. 0x1d is the numeric
        // panel (room 4080), 0x1e the lab computer terminal (room 5060), 0x1f
        // the slide projector (room 4070, LabSlides.cpp).
        if (Flg_ck((int)g_SysFlags, 0x1d) != 0) {
            display_passcode_panel();
        } else if (Flg_ck((int)g_SysFlags, 0x1e) != 0) {
            display_computer_lab();
        } else if (Flg_ck((int)g_SysFlags, 0x1f) != 0) {
            display_slides();
        }
    }
    // 0x0042a0ad: the flag is re-read AFTER the dispatch, not reused from the
    // entry test. The cancel path clears it inside passcode_panel_finish, and
    // the original records that cleared value here.
    g_labSlidesState = (int)Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_INTERACTIVE_SCREEN);
}
