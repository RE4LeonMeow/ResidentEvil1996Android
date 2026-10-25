// CharacterSelectionScreen.cpp - Character select screen state and rendering
// Decompiled from Ghidra at 0x00492340
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include "SpriteRenderer.h"
#include "SFXIds.h"
#include <cstdio>
#include "../system/AssetPath.h"

// ============================================================================
// Forward declarations for external functions
// ============================================================================
extern void logos_state(void);
extern void title_state(void);
extern void game_start(void);
extern void texture_viewer_state(void);

// ============================================================================
// DisplayIntroAndStartGame (0x00420060)
// Plays intro FMV then chains to game_start
// ============================================================================
static void DisplayIntroAndStartGame(void)
{
    sounds_reset();
    g_selectedFmvId = 1;
    g_main_state_flags |= (MSF_FMV_REQUEST | MSF_PANNING_RESET);
    nullsub_0047eb80();
    Task_sleep(1);
    Task_chain((void*)game_start);
}

// ============================================================================
// set_fading (0x0047b980)
// ============================================================================
static void SetFading(unsigned char fade_type, short fade_counter)
{
    if (g_fading_state < 1) {
        g_main_state_flags |= MSF_FADE_ACTIVE;
        g_fading_counter = fade_counter;
        g_fade_type_id = fade_type;
    }
}

// ============================================================================
// Character selection screen state variables (0xac9xxx range)
// ============================================================================

// Selection state
static unsigned char g_selState = 0;       // DAT_00ac9880 - main state
static unsigned char g_selSubState = 0;    // DAT_00ac9881
static unsigned char g_bSelResetGameFlag = 0;  // 0x00ac9882

static unsigned char g_selSelected = 0;    // DAT_00ac93f0 - 0=Chris, 1=Jill
static char g_selTimer = 0;                // DAT_00ac93f1
static unsigned char g_selSwapDir = 0;     // DAT_00ac93f2

// Character 0 (left panel) data
static short g_char0PosX = 0;             // DAT_00ac9400
static short g_char0PosY = 0;             // DAT_00ac9402
static short g_char0Scale = 0;            // DAT_00ac9404
static short g_char0VelX = 0;             // DAT_00ac9406
static short g_char0VelY = 0;             // DAT_00ac9408
static signed char g_char0AccX = 0;       // DAT_00ac940a
static signed char g_char0AccY = 0;       // DAT_00ac940b
static unsigned char g_char0Tpage = 0;    // DAT_00ac940c
static unsigned char g_char0Bright = 0;   // DAT_00ac940d

// Character 1 (right panel) data
static short g_char1PosX = 0;             // DAT_00ac9640
static short g_char1PosY = 0;             // DAT_00ac9642
static short g_char1Scale = 0;            // DAT_00ac9644
static short g_char1VelX = 0;             // DAT_00ac9646
static short g_char1VelY = 0;             // DAT_00ac9648
static signed char g_char1AccX = 0;       // DAT_00ac964a
static signed char g_char1AccY = 0;       // DAT_00ac964b
static unsigned char g_char1Tpage = 0;    // DAT_00ac964c
static unsigned char g_char1Bright = 0;   // DAT_00ac964d

// Rotation and matrix data (for PS1 GTE compatibility)
static short g_selRotVec[3] = {};          // DAT_00ac93d0
static int g_selTransVec[3] = {};          // DAT_00ac93e0
static int g_selMatrix[12] = {};           // DAT_00ac93b0

// PS1-style sprite primitive data blocks (0xac9410, 0xac94b0, 0xac9650, 0xac96f0)
static unsigned char g_char0PortraitSprites[0x28 * 4];
static unsigned char g_char0NameSprites[0x28 * 10];
static unsigned char g_char1PortraitSprites[0x28 * 4];
static unsigned char g_char1NameSprites[0x28 * 10];

// Sprite UV data tables (from Ghidra at 0x4d3ed0, 0x4d3ed8, 0x4d3ee0)
static const unsigned char g_charPortraitTable[2][4] = {
    { 0x00, 0x00, 0x50, 0x80 },
    { 0x50, 0x00, 0x70, 0x80 },
};
static const unsigned char g_char1PortraitTable[2][4] = {
    { 0x00, 0x80, 0x50, 0x80 },
    { 0x00, 0x80, 0x70, 0x80 },
};
static const unsigned char g_nameSpriteTable[5][4] = {};

// ============================================================================
// GetScaledValue - Apply fix16.12 scale to a dimension
// ============================================================================
static int GetScaledValue(int value, short scaleFix12)
{
    return (value * scaleFix12) >> 12;
}

// ============================================================================
// ComputeScale - Convert panel scale value to fix16.12
// Original: scale * -0x18 + 0x2680 (from AddSprite_Ex in FUN_00492d80)
// At scale=0xF0(240): -5760+9856=4096=0x1000=1.0
// ============================================================================
static short ComputeScale(short panelScale)
{
    return panelScale * -24 + 0x2680;
}

// ============================================================================
// CharSelectDrawShadowRect - Renders a semi-transparent shadow overlay on a card
// Submits to g_SpriteCommandBuffer so it renders between back and front cards
// ============================================================================
static void CharSelectDrawShadowRect(short posX, short posY, short scaleFix12)
{
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return;

    // Scaled dimensions: card total is 0xC0 x 0x80 = 192 x 128 at 1.0x
    int shadowW = (int)(192.0f * (float)scaleFix12 / 4096.0f);
    int shadowH = (int)(128.0f * (float)scaleFix12 / 4096.0f);

    int sx = posX - 0x100 + g_ScreenOffsetX;
    int sy = posY - 0x98 + g_ScreenOffsetY;

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = SPRITE_CLASS_NORMAL;
    cmd->spriteFlags = 0;
    cmd->alpha = 100.0f / 255.0f;  // ~0.392
    cmd->r = 0.0f;
    cmd->g = 0.0f;
    cmd->b = 0.0f;
    cmd->variantAlpha = 0.0f;   // translucency is set explicitly in cmd->alpha

    cmd->x0 = (short)sx;
    cmd->y0 = (short)sy;
    cmd->x1 = (short)(sx + shadowW - 1);
    cmd->y1 = (short)(sy + shadowH - 1);

    cmd->depthSort = 42 * 16 + 500;

    // Use an opaque pixel from the portrait texture page (slot 0x0C)
    cmd->u0 = 0x40;
    cmd->v0 = 0x40;
    cmd->u1 = 0x40;
    cmd->v1 = 0x40;

    cmd->extraFlags = 0x0C + 0xF;

    g_SpriteQueueCount++;
}

// ============================================================================
// CharSelectRenderSprite - Renders a portrait sprite with scaling
// Replaces AddSprite_Ex calls from FUN_00492d80 with D3D11-compatible rendering
// ============================================================================
static void CharSelectRenderSprite(unsigned char texU, unsigned char texV,
                                   unsigned short width, unsigned short height,
                                   short posX, short posY,
                                   short pivotX, short pivotY,
                                   short scaleFix12, unsigned short depth,
                                   int slot)
{
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return;
    if ((g_main_state_flags & MSF_SCREEN_STANDALONE) != 0) return;

    int shiftedSlot = slot + 0xF;
    if (shiftedSlot < 0 || shiftedSlot >= 256) return;
    if (g_TexturePageSRV[shiftedSlot] == NULL) return;

    // Apply scaling
    int scaledW = GetScaledValue(width, scaleFix12);
    int scaledH = GetScaledValue(height, scaleFix12);
    int scaledPX = GetScaledValue(pivotX, scaleFix12);
    int scaledPY = GetScaledValue(pivotY, scaleFix12);

    short sx = posX + g_ScreenOffsetX;
    short sy = posY + g_ScreenOffsetY;

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type = 10;
    cmd->sortClass = SPRITE_CLASS_NORMAL;
    cmd->spriteFlags = 0;
    cmd->alpha = 1.0f;
    cmd->r = 1.0f;
    cmd->g = 1.0f;
    cmd->b = 1.0f;
    cmd->variantAlpha = 0.0f;

    cmd->x0 = sx - scaledPX;
    cmd->y0 = sy - scaledPY;
    cmd->x1 = sx + scaledW - scaledPX - 1;
    cmd->y1 = sy + scaledH - scaledPY - 1;

    cmd->depthSort = (unsigned int)depth * 16 + 500;

    cmd->u0 = (unsigned short)texU;
    cmd->v0 = (unsigned short)texV;
    cmd->u1 = cmd->u0 + width - 1;
    cmd->v1 = cmd->v0 + height - 1;

    cmd->extraFlags = shiftedSlot;

    g_SpriteQueueCount++;
}

// ============================================================================
// CharSelectDrawCursor (FUN_00493200)
// Draws the blinking selection cursor arrows
// ============================================================================
static void CharSelectDrawCursor(void)
{
    g_TextureDesc.texU = 0xC0;
    g_TextureDesc.texV = 0x30;
    g_TextureDesc.screenX = -0x88;
    g_TextureDesc.screenY = -0x16;
    g_TextureDesc.width = 0x0C;
    g_TextureDesc.height = 0x0B;
    g_TextureDesc.clutY = 0x1EA;

    if ((g_selTimer & 0x30) == 0) {
        g_TextureDesc.texV = 0x50;
    }

    display_texture(&g_TextureDesc, 1, 0x0C, 1);
    g_TextureDesc.texV += 0x10;
    g_TextureDesc.screenX = 0x4C;
    display_texture(&g_TextureDesc, 1, 0x0C, 1);
}

// ============================================================================
// CharSelectUpdateVelocity (FUN_004932d0)
// Updates position deltas from acceleration values
// ============================================================================
static void CharSelectUpdateVelocity(void)
{
    g_char0VelX += g_char0AccX;
    g_char0VelY += g_char0AccY;
    g_char1VelX += g_char1AccX;
    g_char1VelY += g_char1AccY;
}

// ============================================================================
// CharSelectApplyPosition (FUN_00493290)
// Applies velocity to position
// ============================================================================
static void CharSelectApplyPosition(void)
{
    g_char0PosX += g_char0VelX;
    g_char0PosY += g_char0VelY;
    g_char1PosX += g_char1VelX;
    g_char1PosY += g_char1VelY;
}


// ============================================================================
// CharSelectDrawPortraits (FUN_00492d80)
// Renders the character portrait sprites with scaling and shadow
// Back card (higher tpage) is drawn first, shadow on back card, then front card
// ============================================================================
static void CharSelectDrawPortraits(void)
{
    short scale0 = ComputeScale(g_char0Scale);
    short scale1 = ComputeScale(g_char1Scale);

    if (g_char0Tpage > g_char1Tpage) {
        // Char 0 has higher tpage = back panel → draw first
        CharSelectRenderSprite(0, 0, 0x50, 0x80, g_char0PosX - 0x100, g_char0PosY - 0x98, 0, 0, scale0, g_char0Tpage << 4, 0x0C);
        CharSelectRenderSprite(0x50, 0, 0x70, 0x80, g_char0PosX - 0x100, g_char0PosY - 0x98, -0x50, 0, scale0, g_char0Tpage << 4, 0x0C);
        // Shadow on back card
        CharSelectDrawShadowRect(g_char0PosX, g_char0PosY, scale0);
        // Char 1 is front panel → draw last
        // Original FUN_00492d80: char1 first sprite uses texU=0, texV=0 (same as char0 left half)
        CharSelectRenderSprite(0, 0, 0x50, 0x80, g_char1PosX - 0x100, g_char1PosY - 0x98, 0, 0, scale1, g_char1Tpage << 4, 0x0C);
        CharSelectRenderSprite(0, 0x80, 0x70, 0x80, g_char1PosX - 0x100, g_char1PosY - 0x98, -0x50, 0, scale1, g_char1Tpage << 4, 0x0C);
    } else {
        // Char 1 has higher tpage = back panel → draw first
        CharSelectRenderSprite(0, 0, 0x50, 0x80, g_char1PosX - 0x100, g_char1PosY - 0x98, 0, 0, scale1, g_char1Tpage << 4, 0x0C);
        CharSelectRenderSprite(0, 0x80, 0x70, 0x80, g_char1PosX - 0x100, g_char1PosY - 0x98, -0x50, 0, scale1, g_char1Tpage << 4, 0x0C);
        // Shadow on back card
        CharSelectDrawShadowRect(g_char1PosX, g_char1PosY, scale1);
        // Char 0 is front panel → draw last
        CharSelectRenderSprite(0, 0, 0x50, 0x80, g_char0PosX - 0x100, g_char0PosY - 0x98, 0, 0, scale0, g_char0Tpage << 4, 0x0C);
        CharSelectRenderSprite(0x50, 0, 0x70, 0x80, g_char0PosX - 0x100, g_char0PosY - 0x98, -0x50, 0, scale0, g_char0Tpage << 4, 0x0C);
    }
}

// ============================================================================
// characterSelectionScreen (0x00492340)
// Main character selection state - state machine driven by g_selState
// ============================================================================
void characterSelectionScreen(void)
{
    OutputDebugStringA("[CharSelection] Entered character selection screen\n");

    g_playingGameFlag = 1;
    g_bGameActive = 0;

    g_selState = 0;
    g_selSubState = 0;
    g_bSelResetGameFlag = 0;

    sounds_reset();
    g_loadDataDestPointer = g_DataBuffer;
    g_roomId = 0x1B;   // 0x0049239? - placeholder room id for the selection screen, not a real location

    LoadSoundBank(BANK_SELECT, g_DataBuffer);

    // Load characters police cards and selection arrow texture
    LoadFile(GAME_DATA_ROOT "data\\select_b.tim", g_TimImageBuffer__bitmap, 0x20);
    g_TextureBankID = 0x0A;
    g_TextureCurrentPage = 5;
    LoadTexturePage(g_TimImageBuffer__bitmap, 5, 10, 0x0C, 0, 0, 0, 0);

    // unused texture
    LoadFile(GAME_DATA_ROOT "data\\select_k.tim", g_TimImageBuffer__bitmap, 0x20);
    LoadTexturePage(g_TimImageBuffer__bitmap, 5, 10, 0x0D, 7, 0, 0, 0);

    // Load background image
    LoadFile(GAME_DATA_ROOT "data\\sel_back.pix", g_TimImageBuffer__bitmap, 0x20);
    display_image(0, g_TimImageBuffer__bitmap, 320, 240);

    title_setup_texture_pages(0, 1);
    // empty_00470960(0): empty in the original - call dropped

    // Initialize rotation matrix
    g_selRotVec[0] = 0;
    g_selRotVec[1] = 0;
    g_selRotVec[2] = 0;
    g_selTransVec[0] = 0;
    g_selTransVec[1] = 0;
    g_selTransVec[2] = 0;
    RotMatrix((SVECTOR*)g_selRotVec, (MATRIX*)g_selMatrix);
    MatrixSetTranslation((MATRIX*)g_selMatrix, g_selTransVec);
    SetGlobalScaledRotationMatrix((MATRIX*)g_selMatrix);
    GetMatrixTranslation((MATRIX*)g_selMatrix);
    // 0x004924b5 calls set_scene_render_param_tail (0x0040aba0), a one-line tail
    // wrapper around set_scene_render_param - served by the same function here.
    set_scene_render_param(0xF0);

    //Init Character Sprite Data
    for (int local_6 = 1; local_6 >= 0; local_6--) {
        for (int local_5 = 1; local_5 >= 0; local_5--) {
            int uVar9 = local_5;
            int iVar1 = local_6 * 2 + uVar9;
            int iVar2 = iVar1 * 0x28;

            GteSpriteHeaderInit((SVECTOR*)(g_char0PortraitSprites + iVar2));
            GteSpriteHeaderInit((SVECTOR*)(g_char1PortraitSprites + iVar2));

            unsigned char b0 = g_charPortraitTable[uVar9][0];
            unsigned char b1 = g_charPortraitTable[uVar9][1];
            unsigned char b2 = g_charPortraitTable[uVar9][2];
            unsigned char b3 = g_charPortraitTable[uVar9][3];

            *(g_char0PortraitSprites + iVar2 + 0x0c) = b0;
            *(g_char0PortraitSprites + iVar2 + 0x0d) = b1;
            *(g_char0PortraitSprites + iVar2 + 0x14) = b0 + b2;
            *(g_char0PortraitSprites + iVar2 + 0x15) = b1;
            *(g_char0PortraitSprites + iVar2 + 0x1c) = b0;
            *(g_char0PortraitSprites + iVar2 + 0x1d) = b1 + b3;
            *(g_char0PortraitSprites + iVar2 + 0x24) = b0 + b2;
            *(g_char0PortraitSprites + iVar2 + 0x25) = b1 + b3;

            b0 = g_char1PortraitTable[uVar9][0];
            b1 = g_char1PortraitTable[uVar9][1];
            b2 = g_char1PortraitTable[uVar9][2];
            b3 = g_char1PortraitTable[uVar9][3];

            *(g_char1PortraitSprites + iVar2 + 0x0c) = b0;
            *(g_char1PortraitSprites + iVar2 + 0x0d) = b1;
            *(g_char1PortraitSprites + iVar2 + 0x14) = b0 + b2;
            *(g_char1PortraitSprites + iVar2 + 0x15) = b1;
            *(g_char1PortraitSprites + iVar2 + 0x1c) = b0;
            *(g_char1PortraitSprites + iVar2 + 0x1d) = b1 + b3;
            *(g_char1PortraitSprites + iVar2 + 0x24) = b0 + b2;
            *(g_char1PortraitSprites + iVar2 + 0x25) = b1 + b3;

            *(short*)(g_char0PortraitSprites + iVar2 + 0x0e) =
                (short)GteClutBuild(0, (short)(uVar9 + 0x1ea));
            *(short*)(g_char1PortraitSprites + iVar2 + 0x0e) =
                (short)GteClutBuild(0, (short)(uVar9 * 2 + 0x1ea));

            *(short*)(g_char0PortraitSprites + iVar2 + 0x16) =
                (short)GteTpageBuild(1, 0, 0x140, 0);
            *(short*)(g_char1PortraitSprites + iVar2 + 0x16) =
                (short)GteTpageBuild(1, 0, 0x140, 0);
        }

        for (int local_5 = 4; local_5 >= 0; local_5--) {
            int uVar9 = local_5;
            int iVar1 = local_6 * 5 + uVar9;
            int iVar2 = iVar1 * 0x28;

            GteSpriteHeaderInit((SVECTOR*)(g_char0NameSprites + iVar2));
            GteSpriteHeaderInit((SVECTOR*)(g_char1NameSprites + iVar2));

            unsigned char b0 = g_nameSpriteTable[uVar9][0];
            unsigned char b1 = g_nameSpriteTable[uVar9][1];
            unsigned char b2 = g_nameSpriteTable[uVar9][2];
            unsigned char b3 = g_nameSpriteTable[uVar9][3];

            *(g_char0NameSprites + iVar2 + 0x0c) = b0;
            *(g_char0NameSprites + iVar2 + 0x0d) = b1;
            *(g_char0NameSprites + iVar2 + 0x14) = b0 + b2;
            *(g_char0NameSprites + iVar2 + 0x15) = b1;
            *(g_char0NameSprites + iVar2 + 0x1c) = b0;
            *(g_char0NameSprites + iVar2 + 0x1d) = b1 + b3;
            *(g_char0NameSprites + iVar2 + 0x24) = b0 + b2;
            *(g_char0NameSprites + iVar2 + 0x25) = b1 + b3;

            *(g_char1NameSprites + iVar2 + 0x0c) = b0;
            *(g_char1NameSprites + iVar2 + 0x0d) = b1;
            *(g_char1NameSprites + iVar2 + 0x14) = b0 + b2;
            *(g_char1NameSprites + iVar2 + 0x15) = b1;
            *(g_char1NameSprites + iVar2 + 0x1c) = b0;
            *(g_char1NameSprites + iVar2 + 0x1d) = b1 + b3;
            *(g_char1NameSprites + iVar2 + 0x24) = b0 + b2;
            *(g_char1NameSprites + iVar2 + 0x25) = b1 + b3;

            *(short*)(g_char0NameSprites + iVar2 + 0x0e) =
                (short)GteClutBuild(0, 0x1ea);
            *(short*)(g_char1NameSprites + iVar2 + 0x0e) =
                (short)GteClutBuild(0, 0x1ea);

            *(short*)(g_char0NameSprites + iVar2 + 0x16) =
                (short)GteTpageBuild(1, 2, 0x140, 0);
            *(short*)(g_char1NameSprites + iVar2 + 0x16) =
                (short)GteTpageBuild(1, 2, 0x140, 0);

            *(g_char0NameSprites + iVar2 + 0x04) = 0x40;
            *(g_char0NameSprites + iVar2 + 0x05) = 0x40;
            *(g_char0NameSprites + iVar2 + 0x06) = 0x40;
            *(g_char1NameSprites + iVar2 + 0x04) = 0x40;
            *(g_char1NameSprites + iVar2 + 0x05) = 0x40;
            *(g_char1NameSprites + iVar2 + 0x06) = 0x40;
        }
    }

    // Initialize character panel state
    g_char0Tpage = 2;
    g_char1Tpage = 3;
    g_char0Bright = 0x80;
    g_char1Bright = 0x50;
    g_char0PosX = 0x88;
    g_char0PosY = 0x48;
    g_char0Scale = 0xF0;
    g_selSelected = 0;
    g_fade_type_id = 2;
    g_char1PosX = 200;
    g_char1PosY = 0x68;
    g_char1Scale = 0x110;
    g_fading_counter = 0xF800;
    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
    fade_update();

    // Main loop
    while (1) {
        // Check game reset flag
        if (g_bSelResetGameFlag != 0) {
            g_bSelResetGameFlag = 0;
            StMask(0, 3);
            g_main_state_flags2 = g_main_state_flags2 & MSF2_RESET_KEEP_MASK;
            g_main_state_flags = (g_main_state_flags & ~(MSF_SCREEN_MODE_MASK | MSF_CONTINUE_GAME)) | MSF_SCREEN_STANDALONE;
            Task_chain((void*)title_state);
        }

        // Common texture descriptor setup
        g_TextureDesc.clutX = 0;
        g_TextureDesc.pivotX = 0;
        g_TextureDesc.pivotY = 0;
        g_TextureDesc.clutY = 0;
        g_TextureDesc.flags = 0x01000040;
        g_TextureDesc.colorMulR = 0x80;
        g_TextureDesc.colorMulG = 0x80;
        g_TextureDesc.colorMulB = 0x80;
        g_TextureDesc.texturePage = 5;

        switch (g_selState) {
        case 0:
            // Wait for initial fade-in to complete
            if (g_fading_state < 0) {
                g_selState = 1;
                g_selTimer = 0;
            }
            break;

        case 1:
        {
            // Character selection input handling

            if (((g_RawPadHeld & 4) == 0) || ((g_RawPadHeld & 8) == 0)) {
                // SideWinder check
                DWORD sidewinderPress = 0;
                if (g_bPadConnected) {
                    sidewinderPress = read_sidewinder_pad();
                    sidewinderPress &= 0x10000;
                }

                // Swap characters on LEFT or RIGHT press
                if (((g_PlayerPadPressed & 0x8f0) == 0) && (sidewinderPress == 0)) {
                    if ((g_RawPadHeld & 0xa100) != 0) {
                        g_selSubState = 0;
                        g_selState = 2;
                        g_selSwapDir = (g_RawPadHeld & 0x8000) ? 1 : 0;
        
                        play_sfx(1, 0);
        
                        if (g_selSwapDir == g_selSelected) {
                            g_char0AccX = 2;
                            g_char0AccY = 1;
                        } else {
                            g_char0AccX = -2;
                            g_char0AccY = -1;
                        }
        
                        g_char0VelX = 0;
                        g_char0VelY = 0;
                        g_char1AccX = -g_char0AccX;
                        g_char1AccY = -g_char0AccY;
                        g_char1VelX = 0;
                        g_char1VelY = 0;
                        g_selTimer = 8;
                        goto case_2;
                    }

                    g_selTimer--;
                    CharSelectDrawCursor();

                } else {
                    play_sfx(1, 1);
                    g_selSubState = 0;
                    g_selState = 3;
                    g_fade_type_id = 1;
                    g_fading_counter = 0x100;
                    fade_update();
                }
            } else {
                g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
                g_selSubState = 0;
                g_selState = 7;
                SetFading(2, 0xC00);
            }
            break;
        }

        case 2:
            // Character swap animation
case_2:
            g_selTimer--;

            switch (g_selSubState) {
            case 0:
                // Phase 0: wait for initial timer
                if (g_selTimer == 0) {
                    g_selSubState = 1;
                    g_char0VelX += g_char0AccX;
                    g_char0VelY += g_char0AccY;
                    g_selTimer = (g_selSwapDir ^ 1) << 2;
                    g_char1VelX += g_char1AccX;
                    g_char1VelY += g_char1AccY;
                    goto sub_1;
                }
                goto do_update_velocity;

            default:
            do_update_velocity:
                CharSelectUpdateVelocity();
                break;

            case 1:
sub_1:
                // Phase 1: movement phase
                if (g_selTimer == 0) {
                    g_selSubState = 2;
                    g_selTimer = 0x0F;
                    g_char0AccX = -g_char0AccX;
                    g_char0AccY = -g_char0AccY;
                    g_char1AccX = -g_char1AccX;
                    g_char1AccY = -g_char1AccY;
                    goto sub_2;
                }
                break;

            case 2:
sub_2:
                // Phase 2: velocity reaches zero → swap tpage (front/back flip)
                if (g_char0VelX == 0) {
                    g_selSubState = 3;
                    g_char0Tpage ^= 1;
                    g_char1Tpage ^= 1;
                }

            case 3:
                {
                    // Phase 3: scale and brightness adjustment
                    unsigned char sel = g_selSelected;
                    unsigned char other = g_selSelected ^ 1;

                    // Adjust scale: selected panel grows, other shrinks
                    if (sel == 0) {
                        g_char0Scale += 2;
                        g_char1Scale -= 2;
                    } else {
                        g_char1Scale += 2;
                        g_char0Scale -= 2;
                    }

                    // Adjust brightness
                    if (sel == 0) {
                        g_char0Bright -= 3;
                        g_char1Bright += 3;
                    } else {
                        g_char1Bright -= 3;
                        g_char0Bright += 3;
                    }

                    if (g_selTimer == 0) {
                        g_selSubState = 4;
                        g_char0VelX += g_char0AccX;
                        g_char0VelY += g_char0AccY;
                        g_selTimer = g_selSwapDir << 2;
                        g_char1VelX += g_char1AccX;
                        g_char1VelY += g_char1AccY;
                    } else {
                        goto do_update_velocity;
                    }
                }

            case 4:
                // Phase 4: second movement phase
                // Original (0x00492b3d case 4): just breaks when timer!=0 (NO velocity update)
                // This is intentional - cards coast at constant velocity during this phase
                if (g_selTimer == 0) {
                    g_selSubState = 5;
                    g_selTimer = 7;
                    g_char0AccX = -g_char0AccX;
                    g_char0AccY = -g_char0AccY;
                    g_char1AccX = -g_char1AccX;
                    g_char1AccY = -g_char1AccY;
                    goto sub_5;
                }
                break;

            case 5:
sub_5:
                // Phase 5: final adjustment
                if (g_selTimer == 0) {
                    g_selState = 1;
                    g_selSelected ^= 1;
                }
                goto do_update_velocity;
            }

            CharSelectApplyPosition();
            break;

        case 3:
            // Fade out after selection confirmed
            if (g_fading_state < 0) {
                g_selState = 4;
                g_fade_type_id = 1;
                g_fading_counter = 0xFF00;
                fade_update();
                g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
            }
            break;

        case 4:
            // Wait for fade to complete
            if (g_fading_state < 0) {
                g_selState = 5;
                g_selTimer = -16;
            }
            break;

        case 5:
            // Post-fade timer delay
            g_selTimer--;
            if (g_selTimer == 0) {
                goto case_6;
            }
            break;

        case 6:
case_6:
            // Set selected player and transition to game.
            g_SelectedCharactedId = g_selSelected;
            if (g_selSelected != 0) {
                g_main_state_flags |= MSF_CHAR_VARIANT;
            }
            g_bGameActive = 2;
            nullsub_0047eb80();
            cleanup_texture_slot(12);
            cleanup_texture_slot(13);
            cleanup_texture_slot(14);
            DisplayIntroAndStartGame();
            return;

        case 7:
            // Return to title screen
            if ((g_main_state_flags & MSF_FADE_ACTIVE) == 0) {
                g_bGameActive = 2;
                nullsub_0047eb80();
                cleanup_texture_slot(0x0C);
                cleanup_texture_slot(0x0D);
                cleanup_texture_slot(0x0E);
                Task_chain((void*)title_state);
                return;
            }
            break;
        }

        // Render portraits when in selection or transition states
        if (g_selState < 4) {
            CharSelectDrawPortraits();
        }

        // Draw cursor arrows on top of portraits when idle in selection state
        if (g_selState == 1) {
            CharSelectDrawCursor();
        }

        Task_sleep(1);
    }
}
