// LogosScreen.cpp - Logos/opening state
// Decompiled from Ghidra with original addresses
#include "../Globals.h"
#include "../marni/MarniSystem.h"

// Forward declarations for helpers defined in other files
extern void Object_DeleteAll(int a);            // ObjectManager.cpp
extern void SetVideoResolution(int w, int h);   // 0x00497f30 VideoPlayback.cpp
extern void setSomeColor(int r, int g, int b);  // 0x00470a50
extern void title_state(void);                  // TitleScreen.cpp

// ============================================================================
// logos_state (0x00442bb0)
// Logos/opening state: plays intro videos then chains to title_state.
// NOTE: the original also calls logos_draw_tile_grid (0x00442ee0) from inside
// this state - it lays out six 256x256 texture quads at fixed screen offsets
// via display_texture as the video backdrop. The port's FMV path renders
// full-screen through MarniDX, so the tile grid has no equivalent here.
// ============================================================================
void logos_state(void)
{
    g_playingGameFlag = 0;
    g_fmvPlayCount = 0;
    g_demoIdleTimer1 = 1;

    clear_textures();
    // 0x00470a30: empty in the original (single RET) - call dropped
    Object_DeleteAll(1);

    SetVideoResolution(640, 480);
    g_demoIdleTimer1 = 0;
    SetVideoResolution(320, 240);

    setSomeColor(128, 128, 128);

    // if (g_bIsSoftwareRendering == FALSE) {
        // g_CurrentFMVID = 28;
        // g_FmvCharacterId = 0;
        // g_main_state_flags |= 0x40000;
    // } else {
    //     QueueVideoPlayback(29, 0);
    // }

    Task_sleep(3);

    // if (g_bIsSoftwareRendering == FALSE) {
        g_selectedFmvId = 23;
        // g_CurrentFMVID = 23;
        g_FmvCharacterId = 0;
        g_main_state_flags |= MSF_FMV_REQUEST;
    // } else {
    //     QueueVideoPlayback(29, 0);
    // }

    Task_sleep(1);

    // Task_chain((void*)game_start); // debug only, to go directly to game
    Task_chain((void*)title_state);
}
