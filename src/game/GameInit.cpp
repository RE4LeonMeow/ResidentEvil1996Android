// GameInit.cpp - Game initialization and startup
// All functions decompiled from Ghidra with original addresses
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include "../system/AssetPath.h"
#include <cstdlib>

// Forward declarations for task function registered from init_and_start_game
extern void load_global_assets(void);
extern void logos_state(void);                     // LogosScreen.cpp

extern void SetupTexturePageHandles(int, int);
extern void LoadTexturePage(void*, short, short, int, int, short, short, unsigned);
extern void ProcessTextureImage(void*, short, short, int);
extern void LoadShadowMaskTexture(void*, int);
extern void CreateTexturedQuad(int, int, int*);


// ---------------------------------------------------------------------------
// setPolyF4 (0x0040abe0)
// ---------------------------------------------------------------------------
static void setPolyF4(POLY_F4* entry)
{
    entry->code = 0x28;
    entry->tag = entry->tag & 0xffffff | 0x5000000;
}

// ---------------------------------------------------------------------------
// ClearGameStateFlags (0x004756c0)
// The original zeroes seven DWORDs at 0x00be41c0..0x00be41dc:
//   0x00be41c0 g_main_state_flags        0x00be41c4 g_main_state_flags2
//   0x00be41c8 / 41cc unnamed scratch    0x00be41d0 g_spriteAnimActive/R/G/B
//   0x00be41d4 g_spriteAnimIntensity     0x00be41d8 unnamed scratch
// (g_bGameActive at 0x00be41dc is NOT part of the wipe.)
// Port note: this used to be a literal pointer walk over &g_main_state_flags,
// which zeroed whatever globals the linker placed next (see
// docs/MEMORY_LAYOUT.md). It is now an explicit clear of exactly the original
// members; the unnamed scratch dwords have no port equivalent.
// ============================================================================
static void ClearGameStateFlags(void)
{
    g_main_state_flags = 0;
    g_main_state_flags2 = 0;
    g_spriteAnimActive = 0;
    g_spriteAnimR = 0;
    g_spriteAnimG = 0;
    g_spriteAnimB = 0;
    g_spriteAnimIntensity = 0;
    g_menu_choice_id = 0;
}

// ---------------------------------------------------------------------------
// InitSoundAndFadeState (0x00429a...)
// Initializes sound status and fade state variables
// ---------------------------------------------------------------------------
static void InitSoundAndFadeState(void)
{
    g_SndFadeType = 0;
    g_fading_state = -1;
    g_SpecialRoomLightState = (short)0xFFFF;
}

// ---------------------------------------------------------------------------
// InitInputKeyBindings (0x00497c20)
// Copies key binding configuration data to the master input state's keyMap
// ---------------------------------------------------------------------------
void InitInputKeyBindings(void)
{
    for (int i = 0; i < 32; i++) {
        g_pMasterInputState.keyMap[i] = g_keyBindingData[i];
    }
}

// ---------------------------------------------------------------------------
// InitPlayerInputData (0x004...)
// Initializes player input configuration values
// ---------------------------------------------------------------------------
static void InitPlayerInputData(void)
{
    g_PlayerDpadHeld = 0;
    g_PlayerInputConfig_3e = 0x2000;
    g_PlayerInputConfig_42 = 0x8000;
    g_PlayerInputConfig_3c = 0x1000;
    g_PlayerInputConfig_40 = 0x4000;
    g_PlayerInputConfig_44 = 0x1000;
    g_PlayerInputConfig_46 = 0x4000;
    g_PlayerInputConfig_48 = 0x80;
    g_PlayerInputConfig_52 = 4;
    g_PlayerInputConfig_4a = 0x80;
    g_PlayerInputConfig_56 = 0x10;
    g_PlayerInputConfig_58 = 0x40;
    g_PlayerInputConfig_4c = 8;
    g_PlayerInputConfig_4e = 0x20;
    g_PlayerInputConfig_50 = 8;
    g_PlayerInputConfig_54 = 0x20;
    g_PlayerInputConfig_5a = 0x80;
}

// ============================================================================
// init_and_start_game (0x00429920)
// Main game initialization. Sets up input, clears state, initializes task
// data entries, registers the asset loading task.
// ============================================================================
void init_and_start_game(void)
{
    InitInputKeyBindings();
    CenterScreenOrigin();

    g_bGameActive = 2; // 0x00be41dc

    // Initialize sprite animation slot table (6 entries x 0x14 bytes)
    // Entries accessed by g_spriteAnimActive in rendering functions
    g_spriteAnimSlots[2].count = 10;                     // 0x00be9a88
    g_spriteAnimSlots[3].count = 10;                     // 0x00be9a9c
    g_spriteAnimSlots[4].count = 4;                      // 0x00be9ab0
    g_spriteAnimSlots[3].dataPtr = g_entityLightData_bad8; // 0x00be9aa0
    g_spriteAnimSlots[4].dataPtr = g_entityLightData_bad8; // 0x00be9ab4
    g_spriteAnimSlots[5].count = 4;                      // 0x00be9ac4
    g_spriteAnimSlots[5].dataPtr = g_entityLightData_bb58; // 0x00be9ac8
    g_spriteAnimSlots[0].count = 4;                      // 0x00be9a60
    g_spriteAnimSlots[0].dataPtr = g_entityLightData_bb58; // 0x00be9a64
    g_spriteAnimSlots[1].count = 4;                      // 0x00be9a74
    g_spriteAnimSlots[2].dataPtr = g_entityLightData_9ad8; // 0x00be9a8c
    g_spriteAnimSlots[1].dataPtr = g_playerAnimFunctions;  // 0x00be9a78

    ClearGameStateFlags();
    InitSoundAndFadeState();

    g_imageBufferPtr = g_imageBufferDataA;               // 0x00bebce8
    g_imageBufferPtr2 = g_imageBufferDataB;              // 0x00bee268

    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;

    TaskScheduler_Reset();

    InitPlayerInputData();

    POLY_F4* poly = Poly_F4_ARRAY_004ba750;
    do {
        setPolyF4(poly);
        setPolyF4(poly + 2);
        poly++;
    } while (poly < Poly_F4_ARRAY_004ba750 + 2);

    Task_execute(0, (void*)load_global_assets);
}

// ---------------------------------------------------------------------------
// LoadAllItemsTexture
// Loads item_all.pix into the items image buffer.
// ---------------------------------------------------------------------------
static void LoadAllItemsTexture(void)
{
    // One path for every mode: the DC's sheet is a superset (4 extra rows plus
    // the MOON CREST halves replacing USA sprites) and lives in the DC overlay
    // as its own item_all.pix, so ResolveAssetRoot picks it without a branch
    // here. g_ItemsImageBuffer is sized for the larger sheet.
    LoadFile(GAME_DATA_ROOT "data\\item_all.pix", g_ItemsImageBuffer, 0x20);
}

// ============================================================================
// load_global_assets (0x00429a40)
// Task function: loads all global textures/assets needed by the game.
// After loading, chains to logos_state.
// ============================================================================
void load_global_assets(void)
{
    LoadAllItemsTexture();

    // Load main Fonts textures
    // The primary text font differs by region: the North American/GOG build uses
    // data\fontus.tim, while the Japanese PC (Biohazard) build ships it as
    // data\FONT.TIM. Both resolve through GAME_DATA_ROOT (swapped by
    // ResolveAssetRoot for the configured tree).
    if (GetAssetVersion() != 0) {
        LoadFile(GAME_DATA_ROOT "data\\FONT.TIM", g_DataBuffer, 0x20);
    } else {
        LoadFile(GAME_DATA_ROOT "data\\fontus.tim", g_DataBuffer, 0x20);
    }
    g_TextureBankID = 30;
    ProcessTextureImage(g_DataBuffer, 30, 0, 0);

    // Load numeric panel and puzzles font textures
    LoadFile(GAME_DATA_ROOT "data\\Font03t.tim", g_DataBuffer, 0x20);
    g_TextureBankID = 1;
    ProcessTextureImage(g_DataBuffer, 1, 0, 2);

    // Load Options menu textures (24bits)
    LoadFile(GAME_DATA_ROOT "data\\Optkey03.tim", g_DataBuffer, 0x20);
    g_TextureBankID = 2;
    LoadTexturePage(g_DataBuffer, 2, 0, 0xB, 0, 0, 0, 0);

    // Load Main menu textures (8bits)
    LoadFile(GAME_DATA_ROOT "data\\status.tim", g_DataBuffer, 0x20);
    g_TextureBankID = 0x41C;
    LoadTexturePage(g_DataBuffer, 0x1C, 4, 0, 0, 0, 0, 1);

    SetupTexturePageHandles(0, 1);

    // Loan Main menu characters faces texture (8bits)
    LoadFile(GAME_DATA_ROOT "data\\statface.tim", g_DataBuffer, 0x20);
    LoadTexturePage(g_DataBuffer, g_TextureBankID & 0xFF, 0, 9, 0, 0, 0, 0);

    // Load Inventory slot background texture (8bits)
    LoadFile(GAME_DATA_ROOT "data\\blue.tim", g_DataBuffer, 0x20);
    LoadTexturePage(g_DataBuffer, g_TextureBankID & 0xFF, 0, 10, 0, 0, 0, 0);

    // Load unused weapons texture (Uzi and machinegun) (8bits)
    LoadFile(GAME_DATA_ROOT "data\\staitem.tim", g_DataBuffer, 0x20);
    LoadTexturePage(g_DataBuffer, 0, 0, 0x1E, 0, 0, 0, 0);

    // Load character shadow texture (8bits)
    LoadFile(GAME_DATA_ROOT "data\\kage.tim", g_DataBuffer, 0x20);
    LoadShadowMaskTexture(g_DataBuffer, 0);

    int rectConfig[24] = {
        -400,     400,     -400,     400,
           0,       0,        0,       0,
         400,     400,     -400,    -400,
           0,    0x1A,        0,    0x1A,
           0,       0,     0x1D,    0x1D,
           0,       1,        0,       0,
    };
    CreateTexturedQuad(0, 0x2F, rectConfig);

    Task_chain((void*)logos_state);
}
