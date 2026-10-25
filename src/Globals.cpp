// Original addresses from Ghidra marked for each global variable
#include "Globals.h"
#include "game/PrintText.h"
#include "game/SoundTables.h"
#include "system/AssetPath.h"

// ============================================================================
// ============================================================================
// Global placement — no custom sections
// ----------------------------------------------------------------------------
// The original binary wipes the fixed game-state range 0x00be41e0..0x00be9620
// at game init (InitializeGame's memclr). This decomp used to reproduce that
// as a contiguous block by allocating every in-range global into a
// linker-ordered .gwipe section (plus .sched for state that had to stay out of
// the wipe, and .items for the item image buffer). That required MSVC's
// $-subsection sorting and could not be expressed on GNU ld/lld.
//
// The wipe is now explicit instead: ResetGameStateBlock() in GameStart.cpp
// clears each wiped global by name. That is exactly the set the range covered,
// with no ordering requirement, so the sections are gone and every global here
// is an ordinary definition.
//
// When decompiling a NEW global whose original address is in
// [0x00be41e0, 0x00be9620): add it to ResetGameStateBlock(). Full member table
// and incident history: docs/MEMORY_LAYOUT.md
// ============================================================================


// --- Window system ---
// 0x00bcb2c0
HWND g_hWnd = NULL;
// 0x00bcb2c4
HINSTANCE g_hInstance = NULL;
// 0x004bcb2c
BOOL g_bIsSoftwareRendering = FALSE;
// 0x00be0e29
BOOL g_isGameCursorHiddenFlag = FALSE;
// 0x004bcb78
BOOL g_bHasFinalizedSettings = FALSE;
// 0x00bcb2c8
HANDLE g_hMutex = NULL;

// --- Display / Adapter ---
// 0x007d9148
DWORD g_dwSelectedDisplayAdapterID = 0;
// 0x007d914c
DWORD g_dwSelectedDisplayModeID = 0;
// 0x007d9150
DWORD g_dwScreenWidth = 640;
// 0x007d9154
DWORD g_dwScreenHeight = 480;
// 0x007d9158
BOOL g_bFullScreen = FALSE;
// config.ini [Display] VSync. Off by default: the engine paces itself in
// software (the 33 ms limiter at 0x00441f25 plus FrameRateGovernor), exactly
// as the original did blitting to a window without waiting for a vblank.
// Blocking on a vblank instead makes the display, not the limiter, set the
// tick rate - see the note on MarniDX::Present.
BOOL g_bVSync = FALSE;
// 0x004d642c
int g_dwBitDepth = 16;
// 0x004d6430
DWORD g_dwPlayCount = 0;
// 0x004d6434
DWORD g_dwClearCount = 0;
// 0x004bcb64
DWORD g_GPU_VENDOR_ID = 0;

// --- Display mode storage ---
// 0x007d8f28
DisplayModeInfo g_DisplayModeBuffer[MAX_DISPLAY_MODES] = {};
// 0x007d8f24
int g_NumDisplayModes = 0;

// --- D3D Renderer info ---
// 0x007e0e10
D3DRendererInfo g_D3DRenderers[8] = {};
// 0x007e0e08
int g_NumD3DRenderersAvailable = 0;


// 0x008f879c (ID to choose FMVs exclusives for the selected character)
int g_FmvCharacterId = 0;

// --- Drive types ---
// 0x008f87c4
UINT g_DriveTypes[MAX_DRIVES] = {};
// Drive letter buffer
char g_DriveLetterBuffer[256] = {};

// --- Installation path ---
// 0x00d91bd0
char g_szInstallPath[MAX_PATH] = {};
char g_szCreateDir[260] = {};

// 0x004d4730
BYTE g_keyBindingData[32] = {
    VK_UP,       // [0]  → 0x26
    VK_DOWN,     // [1]  → 0x28
    VK_LEFT,     // [2]  → 0x25
    VK_RIGHT,    // [3]  → 0x27
    0,           // [4]
    'V',         // [5]  → 0x56 run/cancel
    0,           // [6]
    0,           // [7]
    0,           // [8]
    0,           // [9]
    'X',         // [10] → 0x58 aim
    'C',         // [11] → 0x43 action/confirm
    0,           // [12]
    'Z',         // [13] → 0x5a open menu
    0,           // [14]
    0,           // [15]
    0,           // [16]
    0,           // [17]
    0,           // [18]
    0,           // [19]
    0,           // [20]
    0,           // [21]
    0,           // [22]
    0,           // [23]
    0,           // [24]
    0,           // [25]
    0,           // [26]
    'A',         // [27] → 0x41 open options
    VK_CONTROL,  // [28] → 0x11 run/cancel
    VK_RETURN,   // [29] → 0x0d action/confirm
    VK_SPACE,    // [30] → 0x20 action/confirm
    VK_ESCAPE    // [31] → 0x1b run/cancel
};

BYTE g_joystickBindingData[128] = {};

BOOL g_isPaused = FALSE;                   // 0_004d46ac
BOOL g_isSideWinderConnected = FALSE;      // 0x004d46b0 - one-shot START event
BOOL g_bPadConnected = FALSE;              // capability: a game pad is present

BYTE g_InstallFlagData = 0;

// --- Shared memory ---
// DAT_007dfd20
HANDLE g_hFileMapping = NULL;
BYTE* g_pSharedMemory = NULL;

// --- Window rect for drawing ---
// 0xd227b0
RectDrawDesc g_window_rect = {320, 0, 0, 0, 0, 0, 240, 0};

// --- Marni System objects ---
// 0x00d227b0
// NOTE: This MUST live in the .sched section. InitializeGame()'s
// memclr(&g_defaultItemSlot, g_BioCardData) wipes the [.bss] range that the
// linker otherwise places this global inside, nullifying the CMarniDirect3D
// pointer mid-game and crashing the graphics readiness check. Isolating it
// into .sched (alongside the task scheduler state) guarantees the memclr
// never touches it. See docs/MEMORY_LAYOUT.md.
void* g_pMarniDirect3D = NULL;

// Input state master 0x00ac4030
// NOTE: MUST live in .sched. In the original binary this struct (0x00ac4030) is
// far below the game-state block (0x00be41e0..0x00be9620) that InitializeGame's
// memclr(&g_defaultItemSlot, g_BioCardData) wipes. Our linker placed it inside
// that range, so starting a game zeroed keyMap and killed ALL keyboard input in
// gameplay (menu button included). See docs/MEMORY_LAYOUT.md.
MasterInputState g_pMasterInputState = {};

// --- Main state flags ---
// 0x00be41c0
// SCD flag bank 5 in one piece, the way the original stores it at 0x00be41c0:
// cmd_bit_test / cmd_bit_op take ONE base for the bank and index it by a byte
// offset, so a script selector of 0x20+ walks from the first dword into the
// second. Two separate globals would make that walk depend on link order.
DWORD g_MainStateFlagBank[2] = { 0, 0 };

// 0x00be41c4


// 0x00bebcca
short g_fading_counter = 0;
// 0x00bf0a2f
unsigned char g_fade_type_id = 0;


// --- Game state ---
// 0x00d91bc8
int DAT_00d91bc8 = 0;

// 0x008f8790
int g_CurrentFMVID = 0;


// --- Screen pos ---
// 0x004bcac8
short g_ScreenOffsetX = 0;
// 0x04bcaca
short g_ScreenOffsetY = 0;


// 0x00aea0d0 - Per-camera background PAK load buffer (128KB)
BYTE           g_bgPakLoadBuffer[0x20000] = {};

// 0x00b0a0d0 - All-camera cached background buffer (768KB)
// Ghidra detects 786440 bytes (0xC0008) until the next global
BYTE           g_bgCacheBuffer[0xC0008] = {};

// 0x00bca0d8
signed char g_ScreenShakeOffsetX = 0;
// 0x00bca0d9
signed char g_ScreenShakeOffsetY = 0;

// 0x00be41dc
BYTE g_bGameActive = 0;

// --- Task system globals ---
// NOTE: These live in the dedicated .sched section (see top of file) so that
// InitializeGame's memclr(&g_defaultItemSlot, g_BioCardData) cannot wipe them.
// Relative order mirrors the original binary's 0x00d91a68 cluster.
// _g_StackPointer 0x007e0cc8
void* g_StackPointer = NULL;
// 0x00d1fde4
TaskControlBlock g_TasksTable[3] = {};
// 0x00bf09ec
TaskControlBlock* g_CurrentTask = NULL;
// 0x00d91a68
TaskControlBlock* g_CurrentTaskPtr = NULL;
// 0x00d91a70
uintptr_t g_TasksESP[3] = {};
// 0x00d91a7c
DWORD g_CurrentTaskID = 0;
// 0x00d91a80
void* g_TasksEIP[3] = {};
// 0x00d91a8c
uintptr_t g_SchedulerESP = 0;
// 0x00d91a90
void* g_AsyncRpcCallback = NULL;
// 0x004ba0b8
DWORD g_SchedulerRunningFlag = 0;

// --- Input state ---
// 0x00bcb2e0 - last keyboard scan code or dialog message ID
DWORD g_lastScanCodeOrMsgID = 0;

// 0x00be05b4 - edge-detected raw pad word (~prev & current)
WORD g_padEdgeDetectedWord = 0;
// 0x00bf0a04 - raw held state (input to edge detection, used by menu code for held checks)
DWORD g_RawPadHeld = 0;
// 0x00bf0a08 (edge-detected: pressed this frame only)
DWORD g_PlayerPadPressed = 0;
// 0x00bf0a0c
DWORD g_button_pressed_id = 0;
// 0x00bf0a10 - edge-detected held state (output of PlayerPad_Update)
DWORD g_PlayerPadHeld = 0;
// 0x004bae30
DWORD g_PlayerPadHeldPrev = 0;

// 0x00bcb430 - the item sprite sheet (data\item_all.pix): 1200-byte rows of
// 40x30 8bpp indexed art, one row per item image type. The USA sheet is 72 rows
// (86400 B); the Director's Cut's is 76 (91200 B) - it replaces rows 45/46 with
// the MOON CREST halves and appends rows 72..75, whose art the custom Beretta
// M92FS (item 4, image type 0x4B -> row 74) and the crest-halves' neighbours
// use. Sized for the larger sheet: LoadAllItemsTexture loads the whole file, so
// an 86400-byte buffer overflowed by 4800 bytes in DC mode and
// LoadHeldItemsImages read item 4's row out of bounds - the glitched sprite.
// See docs/DC_PORT.md 3f.
BYTE g_ItemsImageBuffer[91200] = {};

// 0x00be05b2 (raw pad state snapshot)
WORD g_RawPadState = 0;
// 0x004bcb3c
BOOL g_DisablePad = FALSE;

// Joystick/controller globals (used by ReadPadBoth / JoyToPSX)
// NOTE: g_PadActiveP1/P2, g_PadRawP1/P2 are aliases for g_pMasterInputState
// fields. See MarniInput.h for the struct layout:
//   g_PadActiveP1 (0x00ac422c) = g_pMasterInputState.frameFlag
//   g_PadRawP1   (0x00ac4058) = g_pMasterInputState.keyboardPrev
//   g_PadRawP2   (0x00ac4230) = g_pMasterInputState.joysticks[0].currPress
//   g_PadActiveP2 (0x00ac4404) = g_pMasterInputState.joysticks[0].enabled
// 0x00ac4018 - Combined PSX button word from both controllers
DWORD g_PadBtnWord = 0;
// 0x00ac7b58 - Number of connected controllers
int   g_NumControllers = 1;
// 0x004b1958 - JoyToPSX overflow warning flag
int   g_JoyWarnPrinted = 0;
// 0x004b1858 - PC joystick bit → PSX button remap table (2 players x 32 entries)
// Maps 32 keyboard/joystick button bits to PSX controller button codes.
// Verified against original binary at 0x004b1858.
DWORD g_JoyRemapTbl[2][32] = {
    { // Player 1
        0x00001000, 0x00004000, 0x00008000, 0x00002000,
        0x00000020, 0x00000040, 0x00000002, 0x00000010,
        0x00000004, 0x00000001, 0x00000008, 0x00000080,
        0x00000100, 0x00000800, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000900,
        0x00000040, 0x00000080, 0x00000080, 0x00000040
    },
    { // Player 2
        0x00001000, 0x00004000, 0x00008000, 0x00002000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000080, 0x00000040, 0x00000002, 0x00000010,
        0x00000020, 0x00000001, 0x00000004, 0x00000008,
        0x00000100, 0x00000800, 0x00000000, 0x00000000, 
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000
    }
};

// The original 1997 joystick ("Joy Def") layout, byte-identical to the
// Player 2 initialiser above - keep the two in sync. InstallPadDefaultBindings
// compares against this to decide whether a table is still the untouched
// original (safe to replace with the port's pad defaults) or something the
// player configured (never touched).
const DWORD g_JoyRemapTblLegacyJoyDefault[32] = {
    0x00001000, 0x00004000, 0x00008000, 0x00002000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000080, 0x00000040, 0x00000002, 0x00000010,
    0x00000020, 0x00000001, 0x00000004, 0x00000008,
    0x00000100, 0x00000800, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

// 0x004d3f58 / 0x004d3fd8 - joystick remap backup tables.
// Shared by the options menu (binding editor scratch) and the save screen
// (the save file stores both tables; the load restores the active one).
unsigned int g_joyRemapBackupKey[32] = {};   // 0x004d3f58
unsigned int g_joyRemapBackupJoy[32] = {};   // 0x004d3fd8

// Pad remap sub-tables (ROM data from 0x004bf2a0, 0x004bf2c0, 0x004bf2e0)
// Each entry maps a PS1 button bitmask to a dpad output position
// Sub-table index 0 (controller config 0)
static const WORD g_padRemapSubTable0[16] = {
    0x1000, 0x2000, 0x4000, 0x8000,
    0x1000, 0x4000, 0x0080, 0x0080,
    0x0008, 0x0040, 0x0008, 0x0004,
    0x0002, 0x0001, 0x0080, 0x0040
};
// Sub-table index 1 (controller config 1)
// Entry [14] verified against the original binary at 0x004bf2c0: it is 0x0020,
// not 0x0080 (the port previously had the sub-table-0 value here).
static const WORD g_padRemapSubTable1[16] = {
    0x1000, 0x2000, 0x4000, 0x8000,
    0x1000, 0x4000, 0x0020, 0x0020,
    0x0008, 0x0040, 0x0008, 0x0004,
    0x0002, 0x0001, 0x0020, 0x0040
};
// Sub-table index 2 (controller config 2)
static const WORD g_padRemapSubTable2[16] = {
    0x1000, 0x0008, 0x4000, 0x0004,
    0x1000, 0x4000, 0x0020, 0x0020,
    0x0002, 0x0040, 0x0008, 0x0004,
    0x0002, 0x0001, 0x0020, 0x0040
};
// Sub-table index 3 (0x00be9a3c) - runtime configurable
WORD g_padRemapSubTable3[16] = {};

// 0x004bf300 - Array of pointers to remap sub-tables
const WORD* g_padRemapTable[4] = {
    g_padRemapSubTable0,
    g_padRemapSubTable1,
    g_padRemapSubTable2,
    g_padRemapSubTable3
};

// 0x00bf0a14 - Previous dpad held state
WORD g_PlayerDpadHeldPrev = 0;
// 0x00bf0a0e - placeholder for high WORD of g_button_pressed_id (use (WORD)(g_button_pressed_id >> 16))
// 0x00bf0a12 - placeholder for high WORD of g_PlayerPadHeld (== g_RawPadState)
// 0x00be9842 - g_PlayerDpadPressed is now a macro to g_BioCard.playerDpadPressed (see BioCard.h)
// 0x00d21d10 - Attract demo input data. The whole-file pdemoN.dat load covers
// this block up to the file tail at 0x00d22674: (0x994 - 0x30) / 2 = 1202
// words, enough for the longest reel (1046 words). Do not shrink to 512 - the
// playback index runs past 512 in every full-length demo.
WORD g_demoPadData[1202] = {};

// 0x00be41d6 - index of the pdemoN.dat attract demo currently playing
WORD g_CurrentAttractModeId = 0;
// 0x00d21ce0 - pdemoN.dat header image; g_DemoTimerCur/g_DemoTimerMax are
// macro aliases into this struct (see Globals.h)
AttractDemoData g_AttractDemoData = {};
// 0x00d22670 - controller config carried in the pdemoN.dat tail (+0x990)
WORD g_AttractMode_ControllerConfig = 0;
// 0x00d22672 - player health carried in the pdemoN.dat tail (+0x992)
short g_AttractMode_PlayerHealth = 0;

// --- Menu / dialog flags ---
// 0x00be9825 - g_menu_choice_id is now a macro to g_BioCard.menu_choice_id (see BioCard.h)
// 0x004D4668
BOOL g_displayExitGameScreen_flag = FALSE;
// 0x004d466c
BOOL g_displayReturnToTitleScreen_Flag = FALSE;

// F9 key handling state (used by OnKeyDown)
// 0x004d46e0 - timeGetTime() of last F9 press for debounce
DWORD g_lastF9PressTime = 0;
// 0x004b3870 - blocks F9 processing when set
int   g_blockF9Flag = 0;
// 0x004d4654 - debug mode counter, cycles 0-3 on F1 press
int   g_F1DebugMode = 0;
// 0x004ba718 - set when F9 triggers game reset
int   g_pressF9Flag = 0;
// 0x004d4670 - triggers game state reset
int   g_resetGameFlag = 0;

// --- Sound system ---
// 0x00BF0A2D
int g_SndFadeType = 0;
// 0x00ac9908
int g_SndRampFramesLeft = 0;
int g_BgmSoundBank = 0;
int g_SfxBanks[64] = {};
int g_RoomSfxBanks[64] = {};
int g_CharacterSfxBanks[64] = {};
int g_emSndBanks[96] = {};   // 48 records x 2 ints - see Globals.h
SndBankSlot g_SndBank[3] = {};          // 0x00ac99d0 - 3 BGM channels
int g_SfxVolume = -1;
char g_BgmPaused = 0;
int g_SndRampDirection = 0;
int g_SndRampCurrentVolume = 0;
int g_SndRampBankIndex = 0;
int g_SndDistSteps = 0;
int g_EnemySndVolume = -1;              // 0x00ac98d0 - enemy sound volume
unsigned int g_SoundSystemFlags = 0;    // 0x004b3998 - sound system flags
char g_SoundAltPathPrefix[256] = {};    // 0x00d91bd0

// g_CollisionShapeHandlers (0x00ac9c00) is defined in game/RoomCollision.cpp,
// next to the handlers it holds.

void* g_SoundManager = NULL;
DirectSound* g_pDirectSound = NULL;
DWORD g_CachedWaveOutVolume = 0;
int g_WaitForMusicTimer = 0;
unsigned int g_BGM_STATE = 0xFF;        // 0x00d226a0 - see Globals.h
HWND g_MainWindowHandle = NULL;
int g_setVolResult = 0;
int g_CurBank = 0;
// 0x00ac99d5 / d8 / dd / e0 / e5 are fields inside the g_SndBank record array
// (g_SndBank[0].slot, [1].handle, [1].slot, [2].handle, [2].slot). They used to be
// declared here as five independent globals for addresses that fall inside that
// array, so writes through one name were invisible to the others.
int           g_bgmDefaultVolume = -1;           // 0x00ac98c4
unsigned short g_snd_pan_left  = 0x7F;        // 0x00ac98c8 - 3D sound left channel pan
unsigned short g_snd_pan_right = 0x7F;        // 0x00ac98cc - 3D sound right channel pan
MATRIX        MATRIX_00d22680 = {};               // 0x00d22680 - camera view matrix for 3D sound
unsigned char g_prevBgmState = 0;               // 0x00bf07f1
unsigned char g_targetBgmState = 0;             // 0x00bf07f0
unsigned char* g_RoomBgmStatePtr = NULL;              // 0x00bf0a30 - pointer to current stage's room BGM state data
// 0x00d1fdc4 → 0x004d0c30. sounds_reset assigns this on every reset, exactly as
// the original does; the static initialiser here only removes the ordering hazard
// of a room loading before the first sounds_reset.
const unsigned char* g_bgmDataTable = &g_BgmRoomData[0][0][0];
void*         g_StageDataPtr = NULL;             // 0x00d1fdc8 - stage-specific data pointer

int g_SoundPanVol = 0;
int g_SndPanSet_result = 0;
char* g_wavName = NULL;
int g_sndload_bank_index = 0;
short g_CurSlot = 0;
int g_SndFadeStepTbl[64] = {};
int g_roomSfxVolume = -1;              // 0x00ac9b70
int g_charSfxVolume = -1;              // 0x00ac98fc

// --- Game init ---

// 0x00be9a60 - Sprite animation slot table (6 entries x 0x14 bytes)
// Indexed by g_spriteAnimActive in render_room_objects / render_entity
SpriteAnimSlot g_spriteAnimSlots[6] = {};

// Per-frame room object render state (render_room_objects, 0x00473ff0)
int    DAT_008f8688 = 0;    // 0x008f8688 - pass index (0 = omodels, 1 = item models)
int    DAT_00ae9ee4 = 0;    // 0x00ae9ee4 - force object depth 0x33 (stage 1 rooms A/B lid)
int    DAT_00ae9ef8 = 0;    // 0x00ae9ef8 - keep the fixed 0x32/0x33 object depth

// Sprite animation data buffers (pointed to by g_spriteAnimSlots entries)
BYTE g_entityLightData_9ad8[0x1000] = {};  // 0x00be9ad8 - entry[2] target
BYTE g_entityLightData_bad8[0x80] = {};    // 0x00bebad8 - entry[3]/[4] target
BYTE g_entityLightData_bb58[0x80] = {};    // 0x00bebb58 - entry[0]/[5] target

// Image buffer data (pointed to by g_imageBufferPtr / g_imageBufferPtr2)
BYTE g_imageBufferDataA[0x2580] = {};      // 0x00bebce8 - primary image buffer data
BYTE g_imageBufferDataB[0x1000] = {};      // 0x00bee268 - secondary image buffer data

// Image buffer pointers (set by init_and_start_game, swapped by display_die_screen)
void* g_imageBufferPtr = NULL;             // 0x00d213a8 - primary image buffer pointer
void* g_imageBufferPtr2 = NULL;            // 0x00d213ac - secondary image buffer pointer

// 0x004ba750
POLY_F4 Poly_F4_ARRAY_004ba750[4] = {};

// 0x004ba7b0
int init_game_flag = 0;

// --- Timing ---
// 0x007e0df4
DWORD g_dwSystemTimer1 = 0;
// 0x007e0df8
DWORD g_dwGameTimer1 = 0;
// 0x004d46c4
DWORD g_GameInitTime = 0;
// 0x00d22730
DWORD Game_timer = 0;
// 0x004d46d4
DWORD DAT_004d46d4 = 0;
// 0x004d45fc
DWORD g_LastFrameTime_ms = 0;

// Frame rate governor globals (0x004973d0)
int g_frameTimeIndex = 0;          // 0x004d45f8
int g_frameTimeBuffer[4] = {};     // 0x00ac4000
int g_frameTimeAccumulator = 0;   // 0x004d45f4
int g_frameTargetTime = 100;      // 0x004d45ec (start at 100 for first-frame present)
int g_ScreenAccessReady = 1;      // 0x004d4658 (start ready so first frame presents)
int g_ScreenAccessCountdown = 0;  // 0x004d4684 - StMask countdown (frames until re-enable)
int g_RenderAccessReady = 1;      // 0x004d4688
int g_MarniScreenReady = 0;       // FUN_00497340 - screen present enable (marni field_0x2ec)
DWORD g_MarniScreenColor = 0;     // FUN_00497360 - packed RGB debug color override

// --- MCIVideo ---
// 0x004bcb44
int g_mciVideoDeviceID = 0;
// 0x004bcb48
BOOL g_bMCIVideoEvent = FALSE;

// --- Misc flags ---

// DAT_004bcb30. In the original this lives in .bss and has exactly two xrefs:
// one read at 0x00441eb9 and one `mov [0x4bcb30], ebx` (ebx == 0) at 0x00441f88.
// Nothing ever stores a non-zero value, so it is always FALSE - it only gates
// the g_hWnd == NULL arm of the game-loop test. Initialising it TRUE (as the
// port used to) inverts the whole gate and defeats the focus-loss pause.
BOOL g_bWindowActive = FALSE;
// 0x004bcb2c - window focused flag, written by WM_ACTIVATE, read by the message
// pump in main(). NOT the same variable as g_isPaused (0x004d46ac), which is the
// SideWinder pause-button event flag consumed by main_loop (injects START+bit8).
// Starts FALSE like the original's .bss; ShowWindow() sends WM_ACTIVATE
// synchronously before the pump starts, so it is already TRUE by then.
BOOL g_bWindowFocused = FALSE;
BOOL g_bQuitFlag = FALSE;       // DAT_004bcb40
BOOL g_bUseFrameSkip = TRUE;  // DAT_004bcb48 - 1 in the image (SetFrameRateMode
                              // rewrites it from g_bGameActive every frame anyway)
BOOL g_bFrameSkipDetected = FALSE; // DAT_004d46dc - 0 in the image. This means
                              // "the machine cannot hold the target", is set by
                              // FrameRateGovernor, and it DISABLES the pump's
                              // 33 ms pacer. Starting it TRUE ran unpaced until
                              // the governor first computed a target.

// --- Print text buffer ---
// 0x00be0e20
char PRINT_TEXT_BUFFER[256] = {};

// --- Texture descriptor for text rendering ---
// 0x00be1150
RectDrawDesc g_rect = {};
// 0x00be1160
TextureDesc g_TextureDesc = {};
// 0x00be1180
int unk_00be1180 = 0;

// EKG line drawing data (primary line at 0x00be1198, secondary at 0x00be1184)
unsigned char g_EkgPrimaryLine[16] = {};               // 0x00be1198
unsigned char g_EkgSecondaryLine[24] = {};             // 0x00be1184

// --- Counters ---
int g_numFramesRendered = 0;    // DAT_004d4694
int g_numFramesPresented = 0;   // DAT_004d469c

// --- Screen info ---
BOOL g_bAccessibilityAnimations = FALSE; // DAT_007d9144

// --- MCI notification ---
BOOL g_bMCINotifyEnabled = FALSE;  // 0x004bcb58
BOOL g_bMCINotifyFlag = FALSE;     // 0x004bcb5c

// --- Frame counter ---
int g_loopCounter = 0;

// Debug clear color
float g_debugClearR = 0.05f;
float g_debugClearG = 0.05f;
float g_debugClearB = 0.08f;
int   g_debugTaskFrame = 0;

// Sprite/clear color (set by setSomeColor)
float g_color_r = 0.0f;              // 0x004c336c
float g_color_g = 0.0f;              // 0x004c3370
float g_color_b = 0.0f;              // 0x004c3374
float g_spriteColorScale = 1.0f;     // 0x004af2ac (read-only multiplier constant)

// Title state globals
unsigned char g_titleLoopFlag = 0;        // 0x00d22777
unsigned char g_titleMode = 0;            // 0x00d22775
unsigned char g_titleOptionsFading = 0;   // 0x00d22776
unsigned char g_titleSelectionId = 0;     // 0x00d22774
short         g_titleDemoTime = 0;        // 0x00d22788 - demo countdown
unsigned char g_titleHoldTimer = 0;       // DC title submenu hold counter (port-added)
short         g_titleTexturePageData[8] = {}; // 0x00d22778 - per-selection tpage
int           g_sceneRenderParam = 0;     // 0x004d6300
DWORD          g_titlePrimType = 0;        // 0x004d6398
DWORD          g_primParam = 0;            // 0x004d63e0
DWORD          g_primFlag2 = 0;            // 0x004d63e4
int           g_displayWidth = 0;         // 0x00bf09f8
int           g_displayHeight = 0;        // 0x00bf09fc
int           g_displayMode = 0;          // 0x00bf09f4
int           g_DisplayImageWidth = 0;    // 0x004c3364
int           g_DisplayImageHeight = 0;   // 0x004c3368
int           g_titleTextureSlotId = 0;   // 0x004c331c
int           g_texturePageMode = 0;      // DAT_008ec9c0
int           g_texturePageHandle = 0;    // global handle
void*         g_texturePageSrcDesc = NULL; // port-only diagnostic (see create_texture_page)
int           g_AsyncResult = 0;          // async operation result
int           g_ExecuteBufferHandle = 0;  // DAT_008e1d58
int           g_SpriteQueueCount = 0;     // sprite queue count
int           g_OTIndex = 0;              // OT index
DWORD         g_SpriteQueueIndex = 0;     // DAT_004c2d10
CMarniBits    g_MarniBitsWorkBuffer;      // DAT_008ed478
DWORD         g_MarniBitsOutput = 0;      // DAT_008ed470
CMarniBits    g_MarniFrameBuffer;         // framebuffer proxy for screenshots
DWORD         g_ObjectWorkBuffer[64] = {};// DAT_008ec9c8


int   g_InstallFlagDataLoaded = 0;      // DAT_004b3998
int   g_SpriteBufferFlag = 0;           // DAT_004b399c
int   g_SpriteAsyncFlag = 0;            // DAT_00d91bc8
int   g_FileOpenCount = 0;              // DAT_00d91bcc
int   g_FileRetryFlag = 0;              // DAT_004d4690
int   g_playingGameFlag = 0;            // 0x004d4674
int   g_loadSaveStateFlag = 0;          // 0x004d4678
// g_demoIdleTimer1 is now a #define alias for g_AttractModeIdleTimer
void* g_loadDataDestPointer = NULL;     // 0x00bebcdc
void* g_RdtLoadDataBackup = NULL;       // 0x00bebce0 - backup of g_loadDataDestPointer before room_set
unsigned char g_selectedFmvId = 0;          // 0x00bf07fb
void* g_fmvDataPointer = NULL;          // 0x00bf07fc
int   g_fmvPlayCount = 0;               // 0x004bae34

// Texture page table
void* g_TexturePageTable = NULL;
DWORD g_TexturePageTable_DAT[256] = {};
MarniHandle g_TexturePageSRV[256] = {};
int   g_TexturePageWidth[256] = {};
int   g_TexturePageHeight[256] = {};
int   g_TexturePageBpp[256] = {};
short g_TexturePageOriginX[256] = {};
short g_TexturePageOriginY[256] = {};
short g_TexturePageId[256] = {};
short g_TexturePageClutBase[256] = {};

// Player input data
int   g_PlayerInputConfig_3c = 0;
int   g_PlayerInputConfig_3e = 0;
int   g_PlayerInputConfig_40 = 0;
int   g_PlayerInputConfig_42 = 0;
int   g_PlayerInputConfig_44 = 0;
int   g_PlayerInputConfig_46 = 0;
int   g_PlayerInputConfig_48 = 0;
int   g_PlayerInputConfig_4a = 0;
int   g_PlayerInputConfig_4c = 0;
int   g_PlayerInputConfig_4e = 0;
int   g_PlayerInputConfig_50 = 0;
int   g_PlayerInputConfig_52 = 0;
int   g_PlayerInputConfig_54 = 0;
int   g_PlayerInputConfig_56 = 0;
int   g_PlayerInputConfig_58 = 0;
int   g_PlayerInputConfig_5a = 0;

// Player animation function pointer table (0x00bebbd8, 52 entries)
// Populated by set_player_animations_functions (0x00409a00)
// Indexed by g_playerEntityPointer.animFrameId * 4
void* g_playerAnimFunctions[52] = {};     // 0x00bebbd8

// (0x004c2ac8) - player_anim_set_attacked_flag dispatch (action_behavior). It was
// the compiler's 3-entry jump table (0x00469410/0x00469840/0x004699d0), not
// installable handlers; its cases are folded into the switch inside
// player_anim_set_attacked_flag (PlayerAnimations.cpp), so no array is needed.
// DAT_004ba360 - player_anim_limb_physics dispatch (action_behavior). Three real
// entries, and they are all Tyrant player-hit reactions living inside the Tyrant's
// own data block (0x004ba240..0x004ba377), so the table is DEFINED IN Tyrant.cpp
// alongside them. It was an all-NULL placeholder here, which softlocked Chris
// mid-pose the first time the Tyrant connected.
// DAT_004b1a90 - player_anim_death_alt dispatch (action_behavior). ONE entry
// (-> 0x00408900) sitting between s_yawnRepositionPath and s_yawnDustOffset in
// Yawn's data block; its case is folded into player_anim_death_alt
// (PlayerAnimations.cpp), so no array is needed. It was an all-NULL placeholder
// here, which froze the swallowed player mid-pose during Yawn's low-health
// swallow: part of the body kept floating in the air because the handler that
// pins the player to the snake's mouth never ran.
// DAT_004c10b0 - player_anim_dispatch_4b1a90 dispatch (action_state). Three real
// entries, all of them the monster plant's "held by the vine" player animation
// living inside the plant's own data block (0x004c0fd8..0x004c10bb), so the table
// is DEFINED IN MonsterPlant.cpp alongside them. It was an all-NULL placeholder
// here, which froze a grabbed player with no animation and never released them.

// Texture/room state
// Bank id + depth as the single 16-bit cell the original word accesses expect
// (see the TextureBankCell note in Globals.h).
TextureBankCell g_TextureBankCell = {0, 0};  // 0x00bebcc4 / 0x00bebcc5
unsigned short g_SavedTextureBankID = 0; // 0x00bebcc6


// File path construction buffer
char   FILE_PATH[260] = {};             // global file path buffer

// Texture/model loading flags
DWORD  DAT_004d2bd8 = 0;                // 0x004d2bd8 - special model flag
DWORD  DAT_004d2bdc = 0;                // 0x004d2bdc - TMD rendering disabled flag
int    DAT_004d2be0 = -1;               // 0x004d2be0 - blend mode override value
unsigned char g_red_color   = 0x34;   // 0x004d2be4 - back color red component
unsigned char g_green_color = 0x34;   // 0x004d2be5 - back color green component
unsigned char g_blue_color  = 0x00;   // 0x004d2be6 - back color blue component
DWORD  DAT_004d2bf4 = 0;                // 0x004d2bf4 - TMD processing flag
DWORD  g_tmdAsyncData = 0;               // 0x008fc424 - TMD async processing data pointer
// 0x004c1a2c - bitmask of entity types that get a g_textureQueueData entry,
// tested as `DAT_004c1a2c >> (entityType & 0x1F) & 1` in load_enemy_model.
// Bits 8, 11, 12, 13, 14, 15, 18 = plant42, neptune, tyrant1, yawn1, plant42
// roots, monster plant, yawn2 - exactly the types scd_model_tint_apply knows
// how to tint. This was 0, which is a NEVER-WRITTEN BUILD-TIME GATE: with no
// queue entry registered, scd_model_tint_apply / FUN_00473d10 / FUN_00473d60
// all fall out of their `find the entry whose id byte matches` scan and return
// silently, so every model tint in the game - the monster plant's death ramp,
// SCD opcode 0x34, Plant 42's and Yawn's fades - was a no-op.
DWORD  DAT_004c1a2c = 0x0004F900;       // 0x004c1a2c

// Weapon animation angle adjustment constants (0x004c2028–0x004c204c)
int    g_weaponAngle_Special = 0x12;     // 0x004c2028 - special weapon angle
int    g_weaponAngle_PrimX   = 0x14;     // 0x004c202c - primary X rotation
int    g_weaponAngle_PrimY   = 0x05;     // 0x004c2030 - primary Y rotation
int    g_weaponAngle_PrimZ   = 0x1E;     // 0x004c2034 - primary Z rotation
int    g_weaponAngle_Sec1X   = -20;      // 0x004c2038 - secondary set 1 X rotation
int    g_weaponAngle_Sec1Y   = 0x10;     // 0x004c203c - secondary set 1 Y rotation
int    g_weaponAngle_Sec1Z   = -50;      // 0x004c2040 - secondary set 1 Z rotation
int    g_weaponAngle_Sec2X   = 0x05;     // 0x004c2044 - secondary set 2 X rotation
int    g_weaponAngle_Sec2Y   = 0x05;     // 0x004c2048 - secondary set 2 Y rotation
int    g_weaponAngle_Sec2Z   = -48;      // 0x004c204c - secondary set 2 Z rotation
DWORD  DAT_00ae9f04 = 0;                // 0x00ae9f04
DWORD  DAT_00ae9f06 = 0;                // 0x00ae9f06
DWORD  DAT_00ae9f00 = 0;                // 0x00ae9f00
DWORD  DAT_00ae9efc = 0;                // 0x00ae9efc
BYTE   g_textureQueueData[40] = {};     // 0x00d22740

// Additional globals
DWORD  g_bCostumeVariant = 0;           // 0x004d6444 (DAT_004d6444) - costume variant selector

// Key binding vectors
BYTE  g_KeyBindingVectors[32] = {};

// Marni video driver arrays (large BSS allocations)
DWORD g_VideoDriverArray_D0[64] = {};
DWORD g_VideoDriverArray_03c[64] = {};
DWORD g_VideoDriverArray_04c[64] = {};
unsigned char g_eventItemUsedFlag = 0;   // 0x00be9615
int g_pendingDoorRecord = 0;             // 0x00bebcbc
unsigned char g_typewriter_state = 0;    // 0x00be9616
unsigned char g_itembox_state = 0;       // 0x00be9617
unsigned char g_desk_check_state = 0;    // 0x00be9618
unsigned short g_short_itembox_open_timer = 0; // 0x004d6eac
void* g_itembox_cover_pointer = NULL;    // 0x004d6eb0
unsigned int g_typewriter_id = 0;        // 0x004d6eb8
short g_counter_increase = 0;            // 0x004d6ebc
unsigned char g_ItemSlotIndices[8] = {}; // 0x00d21cd0
unsigned int DAT_00ae9ef0 = 0;           // 0x00ae9ef0
unsigned int DAT_00ae9ee8 = 0;           // 0x00ae9ee8
unsigned int DAT_00d226e8 = 0;           // 0x00d226e8
unsigned char DAT_00be63c8[0x80] = {};   // 0x00be63c8
unsigned char g_nextRoomDoorType = 0;    // 0x00be0bc8
unsigned char g_nextRoomSfxId = 0;       // 0x00be0bc1
unsigned char g_nextRoom_be05b7 = 0;     // 0x00be05b7
unsigned char g_nextRoomCameraId = 0;    // 0x00be0dd4
unsigned char g_nextRoomDest = 0;        // 0x00be0bc0
int g_roomTransitionBusy = 0;            // 0x004d2290
DWORD g_VideoDriverArray_068[64] = {};
DWORD g_VideoDriverArray_06c[64] = {};
DWORD g_VideoDriverArray_4d0[1024] = {};
DWORD g_VideoDriverArray_4fa[256] = {};
DWORD g_VideoDriverArray_4fc[256] = {};
DWORD g_VideoDriverArray_500[256] = {};
DWORD g_VideoDriverArray_810[256] = {};
DWORD g_VideoDriverArray_814[256] = {};
DWORD g_VideoDriverArray_838[2048] = {};
DWORD g_VideoDriverArray_520[2048] = {};
short g_VideoDriverArray_FA[256] = {};
DWORD g_animSlotIndex = 0;               // 0x008f8c78

// --- Other state vars ---
int g_ScreenAccessCheck = 1;    // 0x004d2290 (start enabled so rendering happens)
int g_RenderAccessCheck = 1;    // DAT_004d468c (start enabled so rendering happens)
int g_demoTimer = 0;            // DEMO timer pattern field
BOOL g_bFullScreenFlag_68 = FALSE;  // used for cursor hiding logic

// Object cleanup globals (Object_DeleteAll / ObjectCleanupCallback / ObjectList_Cleanup)
int    g_objectDeleteFlag = 0;                // 0x004d2bfc
int    g_objectCountArray[32] = {};           // 0x008ffc40
int    g_objectDeleteCounter = 0;             // 0x00aabd68

// 0x008ffcc0 - Complex TMD object data area (DAT_008ffcc0).
// Layout: 0x54-byte header, then 256 entries x 0x84 bytes (D3D handle at
// entry+0x54), plus a secondary copy of each entry at entry+0x83AC. The last
// secondary write (entry 255) ends exactly at +0x10800.
// ComplexTmdObjectSetup / ObjectList_Cleanup used to reach this area via
// (DWORD*)&g_objectCountArray[32] (one past the end of the count array, which
// is adjacent in the ORIGINAL layout) — in our build that wrote into whatever
// globals the linker placed after g_objectCountArray (corrupted
// g_objectDeletePtr with vertex data → crash in CreateTmdObjectInternal).
BYTE   g_complexTmdObjectData[0x10800] = {};

// 0x00aabd6c - TMD object slot → animObjPtr table (250 ints, ends at
// g_renderStateTMD 0x00aac158). In the original binary g_objectDeletePtr is
// STATICALLY initialized to point here (0x004d2bf8 holds 0x00aabd6c and is
// only ever read); it was NULL in this decomp, which silently disabled the
// TMD slot-reuse logic in CreateTmdObjectInternal.

int*   g_objectDeletePtr = g_tmdObjectSlotAnimPtrs;  // 0x004d2bf8 (static init → 0x00aabd6c)
int    g_objectListCleanupFlag = 0;           // 0x004d2fb4
int    g_objectListCleanupCount = 0;          // 0x004d2fb0
// 0x008fc430 - 256 CMarniViewport2 entries x 0x38 = 0x3800 bytes. The original
// placement-news CMarniViewport2[256] here at boot (ctor 0x004272e0); the
// entries are seeded on first use by MarniViewport2_InitEntry (see
// ComplexTmdObjectSetup). ComplexTmdObjectSetup walks this array in 0x38-byte
// steps with a hard bound of +0x3800.
DWORD  g_objectListPtrArray[0xE00] = {};      // 0x008fc430 - 256 x 0x38 entries

// 0x00a75168 - PSXTexture array. Ghidra confirms this spans exactly 32 banks
// (0xa75168..0xaabd68 = 0x36c00 bytes = 32 * 0x1b60), ending precisely at
// g_objectDeleteCounter (0x00aabd68) — NOT 23 banks. ObjectCleanupCallback's
// first-pass loop walks all 32 slots of g_objectCountArray/DAT_008ffc40, and
// TmdProcessingCallback indexes this array directly by g_TextureBankID (a
// byte, unmasked) up to bank 31. Declaring only 23 banks here made every
// access to banks 23-31 write/read past the end of this array, corrupting
// whatever the linker placed next (observed: g_objectCountArray itself ended
// up holding garbage counts, crashing ObjectCleanupCallback).
BYTE   g_psxTextureArray[32 * 0x1b60] = {};   // 0x00a75168 - PSXTexture array
DWORD  g_textureBankRedirect[32] = {};        // 0x00aae2b0

// Async TMD object creation globals (FUN_00483cc0)
DWORD  g_asyncTmdDepth = 0;                   // 0x008fc42c
DWORD  g_asyncTmdDataPtr = 0;                 // 0x00aae330
DWORD  g_asyncTmdObjectPtr = 0;               // 0x008f88a0
DWORD  g_asyncTmdResult = 0;                  // 0x008ffc30

// Face normal buffer
BYTE         g_faceNormalBuffer[250 * 8] = {};      // 0x008fb8b0

// TMD buffers

int          ARRAY_00922260[2016] = {};           // 0x00922260

DWORD        g_tmdTextureAllocated[48] = {};     // 0x00922a40

int          g_complexTmdObjectArray[256] = {};     // 0x00922b00
int          INT_ARRAY_00922f00[530] = {};          // 0x00922f00
int          g_complexTmdObjectIds[256] = {};       // 0x00923748

int          INT_ARRAY_00923b48[2] = {};       // 0x00923b48

BYTE         g_tmdObjectBuffer[1606172] = {};  // 0x00923b50 area

int          g_tmdObjectSlotAnimPtrs[251] = {};     // 0x00aabd6c - TMD slot → animObjPtr table

int          DAT_00aad6ec = 0; // 0x00aad6ec - 
int          DAT_00aae740 = 0; // 0x00aae740 - 
int          DAT_00ac34f8 = 0; // 0x00ac34f8 - 

int          DAT_008f8c74 = 0; // 0x008f8c74 - TMD texture/CLUT data for PSXTexture::Store
int          DAT_009104c0 = 0; // 0x009104c0 - texture bank id
int          DAT_008ffc34 = 0; // 0x008ffc34 - texture depth byte

BYTE         g_renderStateTMD[5524] = {};         // 0x00aac158


// Global render-state objects
char          g_renderStateTex[0x36c];          // 0x00aad6f0

// --- Save/Load game state globals ---
// NOTE: the 0x00be63xx entries overlay g_playerEntity's range in the original
// binary; they get separate storage here but must still be wiped on game init,
// so they carry .gwipe$ tags placing them inside the .gwipe block.
int           g_healthStatus = 0;              // 0x00be6370
int           g_playerAngle = 0;               // 0x00be6368
short         g_playerBkpPosX = 0;             // 0x00be6380
short         g_playerBkpPosZ = 0;             // 0x00be6382
int           g_playerBkpHealthStat = 0;       // 0x00be6384
short         g_playerBkpAngle = 0;            // 0x00be6388
int           g_playerPosX = 0;                // 0x00be6350
int           g_playerPosZ = 0;                // 0x00be6358
int           g_savesCounter = 0;              // 0x004d467c (outside wipe range)
char          g_saveFileName[260] = {};         // 0x004d42d8


// --- Game start / game loop globals ---
// 0x00bebcc0
unsigned short g_message_flags = 0;

// Message display system
unsigned short g_messageFlagsBackup = 0;            // 0x00bebcc2
unsigned short g_PauseGameInMsgFlag = 0;            // 0x00bf0a18
unsigned char* g_MessagePtr = NULL;                 // 0x00bf0a1c
unsigned char  g_MessageStateCounter = 0;            // 0x00bf0a16
short          g_MessageScreenY = 0;                 // 0x00bf0a1a
unsigned char  g_MessageSpeedUpFlag = 0;             // 0x00bf0a17
RDT*           g_RdtPointer = NULL;                  // 0x00bebcd0
unsigned char* g_MessageCurrentPtr = NULL;           // 0x00bf0a20
unsigned char* g_MessageSavedPtr = NULL;             // 0x00bf0a24
unsigned char  g_MessageCharDelay = 0;               // 0x00bf0a29
unsigned char  g_MessageCharTimer = 0;               // 0x00bf0a2a
unsigned char  g_MessageClutBase = 0;                // 0x00bf0a2b
unsigned char  g_MessageClutCopy = 0;                // 0x00bf0a2c
int            g_MessageLineCounter = 0;             // 0x008e1c64
// 0x008f8894
int end_game_status = 0;

// ============================================================================
// Entity / Player globals (non-bio_card addresses)
// ============================================================================

// 0x00be62e4 - Main player entity structure (0x180 bytes)
PlayerEntity g_playerEntity = {};

// 0x00be6464 - Enemy entity array (30 x 0x18C bytes)
Entity g_EnemiesList[30] = {};

// 0x00be92cc - saved enemy state table, 16 x 0x1C bytes (0x00be92cc-0x00be948b).
// Inside the game-init wipe block, so it needs a .gwipe tag (see docs/MEMORY_LAYOUT.md).
// The slot below is g_EnemiesList, which ends at 0x00be928b; the slot above is
// 0x00be9614 - the range is otherwise unoccupied.
SavedEnemyState g_savedEnemyStates[16] = {};

// 0x00be41e2 - Enemy count (inside the wiped range: original zeroes it on game init)
int g_enemy_count = 0;

// ============================================================================
// Effect system globals (billboard/sprite effect pool)
// ============================================================================

// 0x00be41e4 - Effect pool: 64 slots x 0x84 bytes each (ends at 0x00be62e4)
Effect g_effectPool[MAX_EFFECTS] = {};

// 0x00bf07ee - Free effect slot counter (initialized to 64 by InitRoomEffSprite)
unsigned char g_freeEffectSlots = 0;

// 0x00bebcd4 - Pointer to current active entity (points to g_playerEntity during gameplay)
Entity* ENTITY = NULL;
SavedEnemyState* g_pSavedEnemyState = NULL;              // 0x00bebcd8

// 0x00be9a5c - Controller configuration byte
unsigned char g_controllerConfig = 0;

// 0x004c44d8 - Attract mode idle timer
DWORD         g_AttractModeIdleTimer = 0;

// 0x00d213b0 - Zeroed on new game init (InitializeGame non-continue path, +0x1E0 in entity data struct)
DWORD         g_gameSessionInitFlag = 0;

// 0x004bca88 - Room camera matrix (32 bytes)
MATRIX        g_RoomCameraData = {};

// 0x00d1fdd4 - Camera data copy pointer (holds → 0x4bca88)
DWORD         g_RoomCameraDataCopy = 0;

// 0x00d1fdd0 - Dead player move matrix pointer (holds → 0x4bca68, identity matrix)
DWORD         g_deadMoveValue = 0;

// 0x004bd2b0 - shared one-shot grab flag. The hunter's pounce grab sets it to
// 1 (Hunter.cpp, 0x00417a20); the death screen consumes and clears it while
// hiding the player's head joint (0x00443181). Image value 0x00006C6C - the
// tail of the "dinput.dll" string it overlaps in .rdata.
int           DAT_004bd2b0 = 0x00006C6C;

// 0x00d1fdcc - Light direction matrix pointer (holds → g_lightMatrix)
DWORD         g_lightMatrixPtr = 0;

// 0x004bcaa8 - Light direction matrix (3 light source direction vectors for GTE lighting)
MATRIX        g_lightMatrix = {};

// 0x004bca68 - Identity matrix data constant (PS1 GTE: 0x1000 = 1.0 on diagonal)
MATRIX        g_identityMatrixData = {{{4096,0,0}, {0,4096,0}, {0,0,4096}}, 0, {0,0,0}};

// 0x00be11b0 - Player position scratch VECTOR for billboard effects
VECTOR        g_playerPosScratch = {};

// 0x00be11a8 - Scratch SVECTOR for velocity calculations
SVECTOR       g_svecScratch = {};

// 0x00be11c0 - Scratch MATRIX for matrix operations
MATRIX        g_matrixScratch = {};

// 0x008f88e8 - GTE rotation+translation matrix buffer (set by SetRotAndTransMatrix)
// t[2] at offset 0x1C is the depth value (DAT_008f8904 in Ghidra)
MATRIX        g_gteRotTransMatrix = {};

// 0x00aae748 - D3D light data buffer (3 lights × 12 DWORDs each)
DWORD         g_d3dLightData[36] = {};

// 0x00aae770 - D3D light dirty flags
DWORD         g_d3dLightFlags = 0;

// 0x00aae774 - D3D packed ambient color (R<<16 | G<<8 | B)
DWORD         g_d3dAmbientColor = 0;

// 0x00be0dd8 - Death animation state flag
unsigned int  g_deathAnimationFlag = 0;

// 0x00be0dfc - Temp save for ENTITY->animation_frame_id during animation extraction
unsigned int  g_animFrameIdSave = 0;

// 0x00be0e18 - Entity joint position X (set per-joint before TMD render)
int           g_entityJointPosX = 0;

// 0x00be0de0 - Joint displacement value for animation color tinting
int           g_playerDisplacement = 0;

// 0x00be0df8 - Temp pointer for joint processing (second joint in weapon pair)
void*         g_tempVar = NULL;

// 0x00be0de4 - Enemy type snapshot written by apply_weapon_damage for the
// post-hit callbacks and hit reactions.
int           g_weaponHitEnemyType = 0;

// ============================================================================
// Player aim/fire data tables (ROM .data, dumped byte-for-byte from the exe)
// ============================================================================

// 0x004c0cc0 - Special-weapon animation frame windows, 24 dwords. Indexed by
// (character*3 + (attackAnim-1 & 3))*4 + step in weapon_special_frame_update.
unsigned int  g_weaponSpecialFrameWindows[24] = {
    0, 17, 25, 43,  0,  5, 13, 29,
    0,  5, 13, 29,  0, 15, 25, 43,
    0,  5, 13, 29,  0,  5, 13, 29,
};

// 0x004c0d58 - Auto-aim fire end frames, 16 bytes (weapons 2..11 then the
// special-weapon entries 12/13; the "release after end frame" check reads
// [weaponId - 99] for specials, so the table must be 16 wide).
unsigned char g_weaponFireEndFrame[16] = {
    0x08, 0x18, 0x0c, 0x0c, 0x00, 0x0d, 0x0d, 0x0d, 0x12, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// 0x004c0d68 - Per-weapon auto-aim fire data, 8 bytes each, 14 entries.
// Normal weapons are indexed equippedWeaponId - 2 (entries 0..9 = weapons
// 2..11); the special weapons index equippedWeaponId - 99 (ITEM_INGRAM 0x6F
// -> entry 12, ITEM_MINIMI 0x70 -> entry 13). Fields:
//   weaponId  +0x00 - id passed to apply_weapon_damage (0 = no auto-aim
//                     fire: the flamethrower damages via effect behavior 37)
//   fireFrame +0x04 - animation frame that triggers damage + fire sounds
//   sfx1      +0x05 - first fire sound id (Play3DSnd bank 1)
//   sfx2      +0x06 - second fire sound id (Play3DSnd bank 1)
//   pad       +0x07 - unused
WeaponFireData g_weaponFireData[14] = {
    {  2,   5,  7,  8, 0 },  // [ 0] weapon 2  handgun (Beretta)
    {  3,   5,  7,  8, 0 },  // [ 1] weapon 3  shotgun
    {  4,   5,  7,  8, 0 },  // [ 2] weapon 4  python
    {  5,   5,  7,  8, 0 },  // [ 3] weapon 5  magnum
    {  6,   0,  0,  0, 0 },  // [ 4] weapon 6  flamethrower (no auto-aim fire)
    {  7,   2,  7,  8, 0 },  // [ 5] weapon 7  GL explosive rounds
    {  8,   2,  7,  8, 0 },  // [ 6] weapon 8  GL acid rounds
    {  9,   2,  7,  8, 0 },  // [ 7] weapon 9  GL flame rounds
    { 10,   5,  7,  8, 0 },  // [ 8] weapon 10 rocket launcher
    {  0,   0,  0,  0, 0 },  // [ 9] weapon 11 unused
    {  0,   0,  0,  0, 0 },  // [10] unused
    {  0,   0,  0,  0, 0 },  // [11] unused
    {  2,   5,  7,  8, 0 },  // [12] special weapon 0x6f Ingram (weaponId 2 = handgun damage)
    {  2,   5,  7,  8, 0 },  // [13] special weapon 0x70 Minimi (weaponId 2 = handgun damage)
};

// 0x004c0dd8 - Muzzle billboard params, 10 bytes each, 14 entries. Same
// indexing as g_weaponFireData. Fields:
//   b0   +0x00 - animation frame that decrements the ammo and spawns the
//                  billboard on the normal fire path (0x004580f0)
//   type +0x01 - Effect_CreateBillboard type
//   data +0x02 - Effect_CreateBillboard data (first billboard frame)
//   b3   +0x03 - always 0
// x/y/z +0x04..+0x08 - billboard offset from the weapon joint (joint 0xe)
// Entries 8..11 are read as 8 + (ammo & 3) by the rocket launcher path
// (0x004580f0), but the shipped rocket launcher has no round variants -
// likely scrapped multi-round data (the GL is the 3-round weapon).
WeaponFxEntry g_weaponFireBillboard[14] = {
    {  1, 17,  0, 0,   110,   540,     0 },  // [ 0] weapon 2  handgun (Beretta)
    {  1, 17,  1, 0,   640,  1110,     0 },  // [ 1] weapon 3  shotgun
    {  1, 17,  2, 0,   160,   610,     0 },  // [ 2] weapon 4  python
    {  1, 17, 10, 0,   160,   610,     0 },  // [ 3] weapon 5  magnum
    {  0,  0,  0, 0,     0,     0,     0 },  // [ 4] weapon 6  flamethrower (no billboard)
    {  2,  8,  7, 0,   400,   660,     0 },  // [ 5] weapon 7  GL explosive rounds
    {  2,  8,  7, 0,   400,   660,     0 },  // [ 6] weapon 8  GL acid rounds
    {  2,  8,  7, 0,   400,   660,     0 },  // [ 7] weapon 9  GL flame rounds
    {  1, 11,  9, 0,  -190,  1020,    90 },  // [ 8] weapon 10 rocket launcher
    {  1, 11,  9, 0,  -190,  1020,   -60 },  // [ 9] rocket launcher "variant" (ammo & 3) - likely scrapped
    {  1, 11,  9, 0,   -60,  1040,    90 },  // [10] rocket launcher "variant" (ammo & 3) - likely scrapped
    {  1, 11,  9, 0,   -60,  1040,   -60 },  // [11] rocket launcher "variant" (ammo & 3) - likely scrapped
    {  1, 17,  0, 0,   110,   540,     0 },  // [12] special weapon 0x6f Ingram
    {  1, 17,  0, 0,   600,  1370,     0 },  // [13] special weapon 0x70 Minimi
};

// 0x004c0e68 - Big muzzle-flash billboard params, 14 entries. Same layout
// and indexing as g_weaponFireBillboard; b0 here is only the spawn frame
// (99 = never fires). The flash yaw is derived from the table index at
// 0x0045837b, and the rocket launcher (index 8) sets the effect's
// animHeader[0] to 8.
WeaponFxEntry g_weaponMuzzleFlash[14] = {
    {  3,  5,  0, 0,   370, -2870,  -220 },  // [ 0] weapon 2  handgun (Beretta)
    { 25,  5,  9, 0,   360, -2050,  -440 },  // [ 1] weapon 3  shotgun
    { 99,  0,  0, 0,     0,     0,     0 },  // [ 2] weapon 4  python (frame 99 = never)
    { 99,  0,  0, 0,     0,     0,     0 },  // [ 3] weapon 5  magnum (frame 99 = never)
    {  0,  0,  0, 0,     0,     0,     0 },  // [ 4] weapon 6  flamethrower (no flash)
    { 99,  0,  0, 0,     0,     0,     0 },  // [ 5] weapon 7  GL explosive rounds (frame 99 = never)
    { 99,  0,  0, 0,     0,     0,     0 },  // [ 6] weapon 8  GL acid rounds (frame 99 = never)
    { 99,  0,  0, 0,     0,     0,     0 },  // [ 7] weapon 9  GL flame rounds (frame 99 = never)
    {  2,  9, 11, 0,  1400, -2800,  -300 },  // [ 8] weapon 10 rocket launcher
    {  0,  0,  0, 0,     0,     0,     0 },  // [ 9] unused (weapon 11)
    {  0,  0,  0, 0,     0,     0,     0 },  // [10] unused
    {  0,  0,  0, 0,     0,     0,     0 },  // [11] unused
    {  3,  5,  0, 0,   250, -1900,  -250 },  // [12] special weapon 0x6f Ingram
    {  3,  5,  0, 0,   250, -1900,  -250 },  // [13] special weapon 0x70 Minimi
};

// 0x004c0ef8 - Second muzzle-flash billboard params, 14 entries. Same layout
// and indexing as g_weaponFireBillboard; entries 8..11 are read as
// 8 + (ammo & 3) by the rocket launcher path, but the shipped rocket launcher
// has no round variants - likely scrapped multi-round data.
WeaponFxEntry g_weaponFlash2[14] = {
    {  2,  9, 11, 0,   110,   500,     0 },  // [ 0] weapon 2  handgun (Beretta)
    {  2,  9, 11, 0,   640,  1060,     0 },  // [ 1] weapon 3  shotgun
    {  2,  9, 11, 0,   160,   610,     0 },  // [ 2] weapon 4  python
    {  2,  9, 11, 0,   160,   610,     0 },  // [ 3] weapon 5  magnum
    {  0,  0,  0, 0,     0,     0,     0 },  // [ 4] weapon 6  flamethrower (no flash)
    {  2,  9, 11, 0,   640,  1060,     0 },  // [ 5] weapon 7  GL explosive rounds
    {  2,  9, 11, 0,   640,  1060,     0 },  // [ 6] weapon 8  GL acid rounds
    {  2,  9, 11, 0,   640,  1060,     0 },  // [ 7] weapon 9  GL flame rounds
    {  2,  8,  2, 0,   430,  -830,    90 },  // [ 8] weapon 10 rocket launcher
    {  2,  8,  2, 0,   430,  -830,   -60 },  // [ 9] rocket launcher "variant" (ammo & 3) - likely scrapped
    {  2,  8,  2, 0,   570,  -810,    90 },  // [10] rocket launcher "variant" (ammo & 3) - likely scrapped
    {  2,  8,  2, 0,   570,  -810,   -60 },  // [11] rocket launcher "variant" (ammo & 3) - likely scrapped
    {  2,  9, 11, 0,   110,   500,     0 },  // [12] special weapon 0x6f Ingram
    {  2,  9, 11, 0,   640,  1500,     0 },  // [13] special weapon 0x70 Minimi
};

// 0x004c0f84 - Special-weapon fire intervals (frame % interval == 0 fires)
unsigned int  g_weaponFireIntervals[3] = { 2, 6, 10 };

// 0x004c0f90 / 0x004c0f98 - per-weapon FX bytes for behavior 0x18 (magnum
// family). The callbacks at 0x004c0fa0 are not yet transcribed.
unsigned char g_weaponFxBytesA[8] = { 0x11, 0x0d, 0x21, 0x21, 0x11, 0x10, 0x10, 0x10 };
unsigned char g_weaponFxBytesB[8] = { 0x0a, 0x0a, 0x0c, 0x0c, 0x0a, 0x07, 0x07, 0x07 };

// 0x004c0fc0 - Per-character aim height pairs (normal / gun / special), shorts.
// Indexed (character & 1) * 6 + pair by auto_aim_pitch_update.
short         g_aimHeightTable[12] = {
    -2026, -1656, -2530, -2280, -2040, -1800,   // Chris
    -1917, -1617, -2190, -1940, -2003, -1720,   // Jill
};

// 0x004c062c - 1 in the exe: the aim reticle scan (player_reticle_enemy) is
// live. A 0 here would skip the reticle and the auto-turn lock-on.
unsigned char g_aimReticleEnabled = 1;

// 0x008e1c68 - special-weapon fire frame countdown (weapon_special_frame_update
// path in player_behavior_15_gun_fire)
char          g_weaponSpecialFireCountdown = 0;

// 0x004d4510 - Chris SCA collision data (16 bytes)
DWORD         g_scaChrisData[4] = {
    0x00008000,  // first short negative = terminator, rest zero
    0x0000FA06,
    0x01A605FA,
    0x00000000
};

// 0x004d4520 - SCA collision data table 2 (16 bytes, unknown character)
DWORD         g_scaData2[4] = {
    0x00008000,
    0x00000190,
    0x00000000,
    0x00000000
};

// 0x004d4530 - Jill SCA collision data (16 bytes)
DWORD         g_scaJillData[4] = {
    0x00008000,
    0x0000FABA,
    0x01740546,
    0x00000000
};

// 0x004d4540 - SCA data pointer table (index = (characterId & 1) * 2)
// [0]: Chris, [1]: table2, [2]: Jill, [3]: NULL sentinel
DWORD         g_scaDataTable[4] = {
    (DWORD)g_scaChrisData,
    (DWORD)g_scaData2,
    (DWORD)g_scaJillData,
    0x00000000
};

// 0x00d211d0 - Entity/game state data block (enemy SCA pool, image buffer ptrs, SCD ptrs)
BYTE          g_entityDataBlock[0x200] = {};

// 0x00d213d0 - Room sprite entries table (populated from RDT data by FUN_004757c0)
// 128 entries max, each 0x24 bytes. Count stored in g_RdtPointer->sprites_count.
RoomSprEntry  g_RoomSprEntries[128] = {};

// 0x00d21354 - Enemy SCA hit data pool allocator (current position, starts at g_entityDataBlock+6)
DWORD         g_scaPoolPtr = 0;

// 0x00d21358 - Enemy SCA hit data pool base (reset value = g_entityDataBlock+6)
DWORD         g_scaPoolBase = 0;

// 0x00be995c - Per-room BGM state table (7 stages x 32 rooms, 224 bytes)
// Each byte is a BGM state descriptor: bits 0-2 = track index, bits 3-5 = secondary slot enables,
// bits 6-7 = transition mode (0=normal, 1=always reload, 2=force restart), 0xFF = no BGM.
// Modified at runtime by SCD opcode cmd_room_bgm_state_set and preserved in save files.
// 0x00be995c to 0x00be9a3c - g_roomBgmState is a macro to g_BioCard.roomBgmState (see BioCard.h)

// 0x00be98d0 - Room flags bitfield (set by Flg_on via room_set_visited_flag)
// g_RoomFlags is now a macro to g_BioCard.roomFlags (see BioCard.h)

// 0x004d31e0 - Stage room-flag base offset table (indexed by stageId % 5)
// 6th entry (0) covers the map display's lab group (group 5, used when the
// map is opened from the map item with DAT_00ac989f != 1).
const unsigned char g_StageRoomFlagOffset[6] = { 0, 32, 63, 82, 100, 0 };

// 0x00be41c8 - SCD flag bank 4: system flags used by room scripts (case 4 in cmd_bit_test/cmd_bit_op)
// DWORD array, index 0 = flags 0-31, index 1 = flags 32-63. Cleared on room change.
DWORD         g_SysFlags[2] = {};
// DAT_00be9830 (g_fwdPosActionId) is now a macro to g_BioCard.fwdPosActionId (see BioCard.h)

// 0x00d91aa0 - Room action table (AOT list), 24 entries x 12 bytes = 288 bytes.
// Built by door_set/room_action_set/item_model_set and probed every frame by the
// player position update; entry byte 0 selects a room_check_actions handler.
unsigned char g_RoomActionTable[288] = {};
// 0x00d91bc0 - Highest table entry any command has touched this room, INCLUSIVE:
// the setup commands only ever raise it and the probe loops run while
// entry <= it. Reset to the table base on room load.
void*         g_RoomActionTail = NULL;

// 0x00d22790 - Lab slides state block (reset by lab_slides_reset)
unsigned char g_labSlidesFuncIndex = 0;
unsigned char g_labSlidesAnimState = 0;     // 0x00d22791
unsigned char g_passcodePanelAnimationState = 0; // 0x00d22792
int           g_labSlidesScrollX = 0;       // 0x00d22794
unsigned int  g_labSlidesScrollY = 0;       // 0x00d22798
unsigned char g_labSlidesSlideIndex = 0;    // 0x00d227a0
unsigned char g_labSlidesLoopDone = 0;      // 0x00d227a1
unsigned char g_labSlidesMsgId = 0;         // 0x00d227a2
unsigned char g_labSlidesCountdown = 0;     // 0x00d227a3
// 0x007d9120 - Lab slides state (reset by lab_slides_reset)
int           g_labSlidesState = 0;

// Passcode panel state. The original overlays these fields with the lab-slide
// block depending on which interactive screen is active.
unsigned char  g_interactiveScreenSavedCameraId = 0; // 0x00d2278a
unsigned char  g_passcodePanelActive = 0;            // 0x007d9124
unsigned char  g_passcodePanelSprites[9] = {};       // 0x007d9128
unsigned short g_passcodePanelTimer = 0;              // 0x00d227a0 (overlaid)
unsigned short g_passcodePanelPatternIndex = 0;       // 0x00d227a2 (overlaid)

// Lab computer terminal slots in the same block - see Globals.h.
unsigned char  g_labSlidesSubState2 = 0;              // 0x00d22793
int            g_labSlidesSavedPlayerY = 0;           // 0x00d2279c
short          g_labSlidesTimerA = 0;                 // 0x00d227a0 (overlaid)
short          g_labSlidesTimerB = 0;                 // 0x00d227a2 (overlaid)

// 0x00d226a4 - the room action entry the last fired handler latched. A pointer
// into g_RoomActionTable, not an index; unrelated to the SCD event scripts.
void*         g_pRoomActionEntry = NULL;

// 0x00d22734 - Bitmask of held items (1 << count) - 1
DWORD         g_ItemSlotsBitmask = 0;

// 0x00be41e0 - Default/reset item slot (byte). FIRST member of the .gwipe
// block: memclr(&g_defaultItemSlot, g_BioCardData) starts here.
unsigned char g_defaultItemSlot = 0;

// 0x00be41e1 - Zeroed on game init (adjacent byte to g_defaultItemSlot)
unsigned char DAT_00be41e1 = 0;

// 0x00be9614 - Death/timeout state machine byte (game_loop switch)
unsigned char DAT_00be9614 = 0;

// 0x004bd81d - Item image lookup table (459 bytes from Ghidra, verified against
// the original binary). Indexed by itemId * 4. Each entry is 4 bytes:
//   byte 0 = image type index for LoadItemImage
//   byte 1 = combine-table / model-path image type (indexes g_ItemCombinePtrs)
//   byte 2 = "unknown name" category (message_item_name_lookup)
//   byte 3 = item usage flags
// After the 0x4d item entries (0x13c = 316) sits the item-use category
// threshold table (read at offsets 0x13b..0x144 by FUN_00401070) and at
// 0x14b+ the heal-effect table (also addressable as DAT_004bd927 + itemId).
unsigned char g_ItemImageLookupTable[459] = {
    0x00,0x00,0x00,0x00,0x01,0x80,0x80,0x0F,0x02,0x00,0x80,0x07,
    0x03,0x01,0x80,0x06,0x04,0x02,0x80,0x06,0x04,0x03,0x80,0xF0,
    0x05,0x04,0x80,0x06,0x06,0x05,0x80,0x06,0x06,0x06,0x80,0x06,
    0x06,0x07,0x80,0x04,0x07,0x80,0x80,0x0F,0x08,0x08,0x80,0x07,
    0x09,0x09,0x80,0x06,0x0A,0x0A,0x80,0x06,0x0B,0x0B,0x80,0xF0,
    0x0C,0x0C,0x80,0x06,0x0D,0x0D,0x80,0x06,0x0E,0x0E,0x80,0x06,
    0x0F,0x0F,0x80,0x01,0x10,0x10,0x80,0x01,0x11,0x11,0x80,0x01,
    0x12,0x12,0x80,0x01,0x13,0x13,0x80,0x01,0x14,0x14,0x80,0x01,
    0x15,0x15,0x80,0x01,0x16,0x16,0x80,0x01,0x17,0x17,0x80,0x01,
    0x18,0x18,0x80,0x01,0x19,0x80,0x80,0x00,0x1A,0x80,0x00,0x00,
    0x1B,0x80,0x01,0x01,0x1C,0x80,0x80,0x01,0x1D,0x80,0x80,0x01,
    0x1E,0x80,0x80,0x01,0x1F,0x80,0x80,0x01,0x20,0x80,0x80,0x01,
    0x21,0x80,0x80,0x01,0x22,0x80,0x80,0x01,0x23,0x80,0x02,0x01,
    0x24,0x80,0x80,0x01,0x25,0x80,0x80,0x01,0x26,0x80,0x80,0x01,
    0x27,0x80,0x80,0x01,0x28,0x80,0x80,0x01,0x29,0x80,0x80,0x01,
    0x2A,0x80,0x80,0x01,0x2B,0x80,0x80,0x03,0x2C,0x19,0x80,0x00,
    0x2D,0x80,0x80,0x00,0x2E,0x80,0x80,0x02,0x2F,0x80,0x80,0x00,
    0x30,0x80,0x03,0x00,0x31,0x80,0x04,0x00,0x32,0x80,0x05,0x00,
    0x33,0x80,0x06,0x00,0x34,0x80,0x07,0x00,0x35,0x80,0x08,0x00,
    0x36,0x80,0x09,0x00,0x37,0x80,0x0A,0x00,0x38,0x80,0x80,0x00,
    0x39,0x80,0x0B,0x01,0x3A,0x80,0x0C,0x01,0x3B,0x80,0x0D,0x01,
    0x3C,0x80,0x0E,0x01,0x3D,0x80,0x0F,0x01,0x3E,0x80,0x80,0x01,
    0x3F,0x80,0x80,0x01,0x40,0x1A,0x80,0x01,0x41,0x1B,0x80,0x01,
    0x42,0x1C,0x80,0x01,0x43,0x1D,0x80,0x01,0x44,0x1E,0x80,0x01,
    0x45,0x1F,0x80,0x01,0x46,0x20,0x80,0x01,0x47,0x21,0x80,0x01,
    0x48,0x22,0x80,0x00,0x49,0x80,0x80,0x00,0x4A,0x80,0x80,0x00,
    0x00,0x00,0x00,0x0B,0x13,0x14,0x1B,0x33,0x3D,0x3E,0x41,0x4C,
    0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x10,0x00,0x01,0x20,
    0x03,0x02,0x21,0x23,0x03,0x22,0x00,0x00,0x00,0x00,0x00,0xEB,
    0xEC,0xEF,0xE1,0xE2,0xE3,0xE4,0xE9,0xEA,0xE5,0xE6,0xE8,0xE7,
    0xED,0xEE,0xEE,0x00,0x00,0x00,0x00,0x2C,0x01,0x00,0x0C,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x00,0x30,
    0x02,0x00,0x08,0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x00,0x30,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x90,0x01,0x00,0x04,0x00,
    0x00,0x00,0x00,0x10,0x10,0x00,0x11,0x11,0x11,0x11,0x00,0x00,
    0x11,0x11,0x21,0x00,0x13,0x13,0x13,0x00,0x80,0x44,0xFD,0x00,
    0x00,0x00,0x00,0x00,0x00,0xF4,0x01,0x00,0x00,0x00,0x00,0x00,
    0x80,0x0C,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0xF4,0x01,0x00,
    0x00,0x00,0x00,
};

// ============================================================================
// Bio Card data zone globals (0x00be9620 - 0x00be9A3B, 1052 bytes)
// These globals overlay the bio_card.dat buffer. The memcpy in InitializeGame
// copies 1052 bytes to g_BioCard, populating all fields at once.
// ORDER AND POSITION ARE CRITICAL - they must match the bio_card.dat layout.
// All individual bio_card globals are now #define macros to g_BioCard fields.
// ============================================================================
// 0x00be9620 - LAST member of .gwipe: exclusive END marker of the game-init
// memclr (memclr(&g_defaultItemSlot, g_BioCardData) stops here; the bio card
// itself is NOT wiped).
BioCardLayout g_BioCard = {};

// ============================================================================
// Global Messages Table (0x004bfc58)
// Decoded from original ResidentEvil.exe using tools/decode_re1.py
// These messages are displayed by set_message_display() when msg_id bit 6 is set.
// Index = msg_id & 0x3F. The STR() macro encodes ASCII to RE1 font bytes.
//
// The full encoding reference (STR escapes, terminators, message-stream tags)
// is in docs/TEXT_ENCODING.md.
//
// Control characters embedded via STR_* macros:
//   STR_START (0x02) = line break/newline
//   STR_L2    (0x03) = page break / section separator
//   STR_R2    (0x04) = line spacing / formatting marker
//   \i        (05 01 06 00 05 00) = dynamic item name placeholder (CLUT 1,
//              tag 6 arg 0 = selected item, CLUT 0 - see PrintText.h)
//   STR_CIR   (0x08) = Circle button glyph
//   STR_SQR   (0x0A) = Square button glyph
// ============================================================================

// [0] 0x004BF3D8 - Item pickup prompt (yes/no + Square glyph hint)
static constexpr auto s_gm00 = STR("Will you take\\nthe \\i?\\c\\n\\q ");
// [1] 0x004BF3F7 - Item obtained
static constexpr auto s_gm01 = STR("You got the \\i.");
// [2] 0x004BF40C - Inventory full
static constexpr auto s_gm02 = STR("You can't carry any more\\nitems.");
// [3] 0x004BF42D - Item used
static constexpr auto s_gm03 = STR("You have used\\nthe \\i.");
// [4] 0x004BF448 - Item discard prompt (auto-dismiss 1 after the choice)
static constexpr auto s_gm04 = STR("Will you put down the\\n\\i?\\c\\n\\q\\d\\x01");
// [5] 0x004BF46B - Item use prompt (auto-dismiss 1 after the choice)
static constexpr auto s_gm05 = STR("Will you use\\nthe \\i?\\c\\n\\q\\d\\x01");
// [6] 0x004BF489 - Picked up the item (msg 0xc6; auto-dismiss 1)
static constexpr auto s_gm06 = STR("\\i\\nhas been filed.\\p \\d\\x01");
// [7] 0x004BF4A3 - Key discard confirmation
static constexpr auto s_gm07 = STR("This key is useless now.\\nDiscard?\\c\\n\\q\\n");
// [8] 0x004BF4CA - Door locked, sword carving
static constexpr auto s_gm08 = STR("\\nIt's locked.\\p \\nA carving of a sword.");
// [9] 0x004BF4F1 - Door locked, armor carving
static constexpr auto s_gm09 = STR("\\nIt's locked.\\p \\nA carving of armor.");
// [10] 0x004BF516 - Door locked, shield carving
static constexpr auto s_gm10 = STR("\\nIt's locked.\\p \\nA carving of a shield.");
// [11] 0x004BF53E - Door locked, helmet carving
static constexpr auto s_gm11 = STR("\\nIt's locked.\\p \\nA carving of a helmet.");
// [12] 0x004BF566 - Door tightly locked with plate
static constexpr auto s_gm12 = STR("The door is tightly\\nlocked.\\p There's a plate on right\\nhand side.");
// [13] 0x004BF5A8 - Door locked, says Closet
static constexpr auto s_gm13 = STR("\\nIt's locked.\\p \\nThe door says \\oCloset\".");
// [14] 0x004BF5D1 - Door locked, plate says 002
static constexpr auto s_gm14 = STR("\\nIt's locked.\\p \\nThe plate says 002.");
// [15] 0x004BF5F6 - Door locked, plate says 003
static constexpr auto s_gm15 = STR("\\nIt's locked.\\p \\nThe plate says 003.");
// [16] 0x004BF61B - Door locked, says Control Room
static constexpr auto s_gm16 = STR("\\nIt's locked.\\p The door says\\n\\oControl Room\".");
// [17] 0x004BF649 - Power Room door locked
static constexpr auto s_gm17 = STR("\\n\\oPower Room\"\\p The door is tightly\\nlocked.");
// [18] 0x004BF675 - Generic locked
static constexpr auto s_gm18 = STR("\\nIt's locked.");
// [19] 0x004BF684 - Locked from inside
static constexpr auto s_gm19 = STR("\\nIt's locked from inside.");
// [20] 0x004BF69F - Unlocked
static constexpr auto s_gm20 = STR("\\nYou unlocked it.");
// [21] 0x004BF6B2 - Locked, use lockpick
static constexpr auto s_gm21 = STR("\\nIt's locked.\\p Use the lockpick to open\\nthe door.");
// [22] 0x004BF6E5 - Hurry message
static constexpr auto s_gm22 = STR("\\nI've got to hurry!");
// [23] 0x004BF6FA - No time to check
static constexpr auto s_gm23 = STR("There is no time to check\\nit.");
// [24] 0x004BF719 - Desk locked
static constexpr auto s_gm24 = STR("\\nThe desk is locked.");
// [25] 0x004BF72F - Desk locked, use item prompt (yes = use the selected key;
// the post-action selector is \q and its data byte is the message terminator)
static constexpr auto s_gm25 = STR("\\nThe desk is locked.\\p Will you use\\nthe \\i?\\c\\n\\q\\x01");
// The four opening narrations are the only global messages that auto-dismiss.
// Every other entry in this table ends `01 00` in the original and holds until
// the player presses a button; these end `01 30` (26/27/28) and `01 40` (29),
// i.e. clear themselves after 48 and 64 frames. The trailing \dT / \dd encodes
// that operand ('T' -> 0x30, 'd' -> 0x40); without it the intro text stalls on
// screen waiting for input instead of playing through. Verified against
// 0x004bf763 / 0x004bf795 / 0x004bf7e4 / 0x004bf836 in ResidentEvil.exe.
// [26] 0x004BF763 - Opening narration: Chris
static constexpr auto s_gm26 = STR("\\n\\s           Chris...\\s4\\pT\\n \\s          Chris...\\s\\n\\dT");
// [27] 0x004BF795 - Opening narration: escaped into mansion
static constexpr auto s_gm27 = STR("\\s\\nThey have escaped\\ninto the mansion\\pTwhere they thought\\nit was safe.\\pTYet...\\dT");
// [28] 0x004BF7E4 - Opening narration: survival horror
static constexpr auto s_gm28 = STR("\\s\\nYou have once\\n\\s\\nagain entered\\pT\\s\\nthe world of\\n\\s\\nsurvival horror.\\pT\\s\\nGood luck!\\dT");
// [29] 0x004BF836 - Opening narration: be smart
static constexpr auto s_gm29 = STR("\\s\\nBe smart!\\n\\s\\nFighting foes\\pT\\s\\nisn't the only\\n\\s\\nway to survive\\pT\\s\\nthis horror.\\dd");
// [30] 0x004BF886 - Typewriter, no ink ribbon. The name is literal (05 01
// CLUT-green brackets, like the original) — g_selectedItemId is NOT the
// ribbon here, so a \i lookup would show the wrong item.
static constexpr auto s_gm30 = STR("\\nIt's an old typewriter.\\p If I had an \\x05\\x01INK RIBBON\\x05\\x00, I\\ncould save my progress...");
// [31] 0x004BF8D9 - Typewriter, use ink ribbon prompt (yes/no; literal
// INK RIBBON for the same reason as [30]; the \x00 after \c is the
// choice-1 offset — this message has no post-action, the typewriter state
// machine reads g_menu_choice_id itself).
static constexpr auto s_gm31 = STR("You can save your progress\\nwith this.\\p Will you use\\nthe \\x05\\x01INK RIBBON\\x05\\x00?\\c\\x00");
// [32] 0x004BF924 - Typewriter, save confirmation
static constexpr auto s_gm32 = STR("You can save your progress\\nwith this.\\p Will you save your progress?\\c ");
// [33] 0x004BF4DA - Carving of a sword (examined)
static constexpr auto s_gm33 = STR("A carving of a sword.");
// [34] 0x004BF501 - Carving of armor (examined)
static constexpr auto s_gm34 = STR("A carving of armor.");
// [35] 0x004BF526 - Carving of a shield (examined)
static constexpr auto s_gm35 = STR("A carving of a shield.");
// [36] 0x004BF54E - Carving of a helmet (examined)
static constexpr auto s_gm36 = STR("A carving of a helmet.");
// [37] 0x004BF96B - Number 002 engraved
static constexpr auto s_gm37 = STR("The number 002 is engraved.");
// [38] 0x004BF988 - Number 003 engraved
static constexpr auto s_gm38 = STR("The number 003 is engraved.");
// [39] 0x004BF9A5 - A desk key
static constexpr auto s_gm39 = STR("A desk key.");
// [40] 0x004BF9B2 - Strange mark carved
static constexpr auto s_gm40 = STR("A strange mark is carved\\nhere.");
// [41] 0x004BF9D2 - Specially coated, looks important
static constexpr auto s_gm41 = STR("It's specially coated and\\nlooks important...");
// [42] 0x004BFA00 - Must be a closet somewhere
static constexpr auto s_gm42 = STR("There must be a closet\\nsomewhere.");
// [43] 0x004BFA23 - Square-shaped end (Crank Square)
static constexpr auto s_gm43 = STR("Its end is square-shaped.");
// [44] 0x004BFA3E - Hex-shaped end (Crank Hex)
static constexpr auto s_gm44 = STR("Its end is hex.-shaped.");
// [45] 0x004BFA57 - All pages blank
static constexpr auto s_gm45 = STR("All pages are blank.\\nWhat's it for...?");
// [46] 0x004BFA7F - Medal in the book
static constexpr auto s_gm46 = STR("There was a medal in the\\nbook.");
// [47] 0x004BFA9F - Chemical to kill weeds
static constexpr auto s_gm47 = STR("A chemical to kill the\\nweeds.");
// [48] 0x004BFABE - Item loaded (e.g. "MAGNUM loaded.")
static constexpr auto s_gm48 = STR("\\i loaded.");
// [49] 0x004BFACE - Don't need this any more
static constexpr auto s_gm49 = STR("I don't need this any\\nmore.");
// [50] 0x004BFAEB - Too dangerous to mix here
static constexpr auto s_gm50 = STR("Too dangerous to mix here.");
// [51] 0x004BFB07 - Poison gas enveloped
static constexpr auto s_gm51 = STR("The poison gas enveloped.");
// [52] 0x004BFB22 - Mixing failed
static constexpr auto s_gm52 = STR("The mixing seems to have\\nfailed.  It disappeared.");
// [53] 0x004BFB55 - Mix herbs confirmation
static constexpr auto s_gm53 = STR("Will you mix the herbs?\\c ");
// [54] 0x004BFB70 - Mixing doesn't work
static constexpr auto s_gm54 = STR("Mixing these does not seem\\nto work.");
// [55] 0x004BFB95 - Can't use alone
static constexpr auto s_gm55 = STR("You can't use this alone.");
// [56] 0x004BFBE2 - Can't use it here
static constexpr auto s_gm56 = STR("You can't use it here.");
// [57] 0x004BFBB0 - Chemical can be mixed
static constexpr auto s_gm57 = STR("This chemical can be mixed\\nwith other chemicals.");
// [58] 0x004BFBE2 - Can't use it here (dup of 56)
static constexpr auto s_gm58 = STR("You can't use it here.");
// [59] 0x004BFBE2 - Can't use it here (dup of 56)
static constexpr auto s_gm59 = STR("You can't use it here.");
// [60] 0x004BFBE2 - Can't use it here (dup of 56)
static constexpr auto s_gm60 = STR("You can't use it here.");
// [61] 0x004BFBE2 - Can't use it here (dup of 56)
static constexpr auto s_gm61 = STR("You can't use it here.");
// [62] 0x004BFC27 - Don't need to use at the moment
static constexpr auto s_gm62 = STR("You don't need to use this\\nitem at the moment.");
// [63] - NULL (pointer 0x73755C2E is out of bounds)

unsigned char* global_messages[64] = {
    (unsigned char*)s_gm00.bytes,  // [0]  Will you take the [L1]
    (unsigned char*)s_gm01.bytes,  // [1]  You got the [L1]
    (unsigned char*)s_gm02.bytes,  // [2]  You can't carry any more items.
    (unsigned char*)s_gm03.bytes,  // [3]  You have used the [L1]
    (unsigned char*)s_gm04.bytes,  // [4]  Will you put down the [L1]
    (unsigned char*)s_gm05.bytes,  // [5]  Will you use the [L1]
    (unsigned char*)s_gm06.bytes,  // [6]  [L1] (item name)
    (unsigned char*)s_gm07.bytes,  // [7]  Key discard confirmation
    (unsigned char*)s_gm08.bytes,  // [8]  Locked - sword carving
    (unsigned char*)s_gm09.bytes,  // [9]  Locked - armor carving
    (unsigned char*)s_gm10.bytes,  // [10] Locked - shield carving
    (unsigned char*)s_gm11.bytes,  // [11] Locked - helmet carving
    (unsigned char*)s_gm12.bytes,  // [12] Door tightly locked with plate
    (unsigned char*)s_gm13.bytes,  // [13] Locked - Closet
    (unsigned char*)s_gm14.bytes,  // [14] Locked - plate 002
    (unsigned char*)s_gm15.bytes,  // [15] Locked - plate 003
    (unsigned char*)s_gm16.bytes,  // [16] Locked - Control Room
    (unsigned char*)s_gm17.bytes,  // [17] Power Room locked
    (unsigned char*)s_gm18.bytes,  // [18] It's locked.
    (unsigned char*)s_gm19.bytes,  // [19] Locked from inside
    (unsigned char*)s_gm20.bytes,  // [20] You unlocked it.
    (unsigned char*)s_gm21.bytes,  // [21] Locked - use lockpick
    (unsigned char*)s_gm22.bytes,  // [22] I've got to hurry!
    (unsigned char*)s_gm23.bytes,  // [23] No time to check it.
    (unsigned char*)s_gm24.bytes,  // [24] Desk is locked.
    (unsigned char*)s_gm25.bytes,  // [25] Desk locked - use item?
    (unsigned char*)s_gm26.bytes,  // [26] Opening narration: Chris
    (unsigned char*)s_gm27.bytes,  // [27] Opening: escaped into mansion
    (unsigned char*)s_gm28.bytes,  // [28] Opening: survival horror
    (unsigned char*)s_gm29.bytes,  // [29] Opening: be smart
    (unsigned char*)s_gm30.bytes,  // [30] Typewriter - no ink ribbon
    (unsigned char*)s_gm31.bytes,  // [31] Typewriter - use ink ribbon?
    (unsigned char*)s_gm32.bytes,  // [32] Typewriter - save confirmation
    (unsigned char*)s_gm33.bytes,  // [33] A carving of a sword.
    (unsigned char*)s_gm34.bytes,  // [34] A carving of armor.
    (unsigned char*)s_gm35.bytes,  // [35] A carving of a shield.
    (unsigned char*)s_gm36.bytes,  // [36] A carving of a helmet.
    (unsigned char*)s_gm37.bytes,  // [37] Number 002 engraved
    (unsigned char*)s_gm38.bytes,  // [38] Number 003 engraved
    (unsigned char*)s_gm39.bytes,  // [39] A desk key.
    (unsigned char*)s_gm40.bytes,  // [40] Strange mark carved here.
    (unsigned char*)s_gm41.bytes,  // [41] Specially coated, looks important.
    (unsigned char*)s_gm42.bytes,  // [42] Must be a closet somewhere.
    (unsigned char*)s_gm43.bytes,  // [43] Square-shaped end.
    (unsigned char*)s_gm44.bytes,  // [44] Hex-shaped end.
    (unsigned char*)s_gm45.bytes,  // [45] All pages blank.
    (unsigned char*)s_gm46.bytes,  // [46] Medal in the book.
    (unsigned char*)s_gm47.bytes,  // [47] Chemical to kill weeds.
    (unsigned char*)s_gm48.bytes,  // [48] [L1] loaded.
    (unsigned char*)s_gm49.bytes,  // [49] Don't need this any more.
    (unsigned char*)s_gm50.bytes,  // [50] Too dangerous to mix here.
    (unsigned char*)s_gm51.bytes,  // [51] Poison gas enveloped.
    (unsigned char*)s_gm52.bytes,  // [52] Mixing failed.
    (unsigned char*)s_gm53.bytes,  // [53] Mix herbs?
    (unsigned char*)s_gm54.bytes,  // [54] Mixing doesn't work.
    (unsigned char*)s_gm55.bytes,  // [55] Can't use this alone.
    (unsigned char*)s_gm56.bytes,  // [56] Can't use it here.
    (unsigned char*)s_gm57.bytes,  // [57] Chemical can be mixed.
    (unsigned char*)s_gm58.bytes,  // [58] Can't use it here.
    (unsigned char*)s_gm59.bytes,  // [59] Can't use it here.
    (unsigned char*)s_gm60.bytes,  // [60] Can't use it here.
    (unsigned char*)s_gm61.bytes,  // [61] Can't use it here.
    (unsigned char*)s_gm62.bytes,  // [62] Don't need to use at the moment.
    NULL,                            // [63] (out of bounds pointer)
};

// ============================================================================
// Item Description Table (0x004c6160)
// Decoded from the original ResidentEvil.exe by
// tools/extract_item_descriptions.py. Read only by
// set_item_description_message() (0x00455730) - the examine text the item
// viewer shows when the player presses the action button on a 3D item model.
// Index = itemId - 1; the caller remaps items 0x6f/0x70 onto 0x4d/0x4e.
// These strings live in their own table: they are neither RDT nor global
// messages, so set_message_display() cannot reach them.
// ============================================================================
static constexpr auto s_idesc00 = STR("This doesn't seem to be\\nenough for this mission.");
static constexpr auto s_idesc01 = STR("Beretta M92FS. Automatic\\nloaded with 9mm bullets.");
static constexpr auto s_idesc02 = STR("Remington M870.\\nA pump-action shotgun.");
static constexpr auto s_idesc03 = STR("Powerful gun can be loaded\\nwith .357 magnum rounds.");
static constexpr auto s_idesc04 = STR("Can throw flame for 9 sec.\\nwith max. fuel.");
static constexpr auto s_idesc05 = STR("A launcher can be loaded\\nwith various rounds.");
static constexpr auto s_idesc06 = STR("One shot can destroy any\\ntarget.");
static constexpr auto s_idesc07 = STR("Clip for Beretta.");
static constexpr auto s_idesc08 = STR("Shells for the shotgun.");
static constexpr auto s_idesc09 = STR("More powerful than magnum\\nrounds.  For the C.Python.");
static constexpr auto s_idesc10 = STR(".357 magnum rounds.\\nFor the Colt Python.");
static constexpr auto s_idesc11 = STR("Fuel for the flamethrower.");
static constexpr auto s_idesc12 = STR("Powerful rounds for the\\nBazooka.");
static constexpr auto s_idesc13 = STR("A glass bottle to put\\nchemicals in.");
static constexpr auto s_idesc14 = STR("There's water in the\\nbottle.");
static constexpr auto s_idesc15 = STR("This is needed to generate\\nV-JOLT.");
static constexpr auto s_idesc16 = STR("This is not the chemical I\\nneed.");
static constexpr auto s_idesc17 = STR("Now I can destroy that ivy\\nmonster.");
static constexpr auto s_idesc18 = STR("It's too dangerous to fire!\\nDose it have another use?");
static constexpr auto s_idesc19 = STR("Nothing important.");
static constexpr auto s_idesc20 = STR("There's a scratch.\\nSomeone may have used it.");
static constexpr auto s_idesc21 = STR("It's shining beautifully.");
static constexpr auto s_idesc22 = STR("The title is \\oMoonlight\\nSonata\".");
static constexpr auto s_idesc23 = STR("A medal from the\\nsecond Doom Book.");
static constexpr auto s_idesc24 = STR("A medal from the\\nfirst Doom Book.");
static constexpr auto s_idesc25 = STR("Now I can move the\\nelevator.");
static constexpr auto s_idesc26 = STR("It seems to be a start-up \\ndisk.");
static constexpr auto s_idesc27 = STR("It represents wind.");
static constexpr auto s_idesc28 = STR("I can signal Brad with this.");
static constexpr auto s_idesc29 = STR("It seems to be some kind\\nof research report.");
static constexpr auto s_idesc30 = STR("A carving of the moon.");
static constexpr auto s_idesc31 = STR("A carving of the star.");
static constexpr auto s_idesc32 = STR("A carving of the sun.");
static constexpr auto s_idesc33 = STR("It's used with a\\ntypewriter.");
static constexpr auto s_idesc34 = STR("Some fluid is left.");
static constexpr auto s_idesc35 = STR("A simple lock can be\\nopened with this.");
static constexpr auto s_idesc36 = STR("A key to enter the Control\\nRoom.");
static constexpr auto s_idesc37 = STR("I can heal any wound with\\nthis.");
static constexpr auto s_idesc38 = STR("Only one dose is left.");
static constexpr auto s_idesc39 = STR("It's a local herb.");
static constexpr auto s_idesc40 = STR("It's a mixture of green\\nand red herbs.");
static constexpr auto s_idesc41 = STR("It's a mixture of 2 green\\nherbs.");
static constexpr auto s_idesc42 = STR("It's a mixture of green\\nand blue herbs.");
static constexpr auto s_idesc43 = STR("It's a mixture of green,\\nblue and red herbs.");
static constexpr auto s_idesc44 = STR("It's a mixture of 3 green\\nherbs.");
static constexpr auto s_idesc45 = STR("It's a mixture of 2 green\\nherbs and blue herb.");
static constexpr auto s_idesc46 = STR("The battery is still\\ncharged.");
static constexpr auto s_idesc47 = STR("A sub machine gun loaded\\nwith 9mm bullets.");
static constexpr auto s_idesc48 = STR(" A full-automatic\\nlight-weight machine gun.");

unsigned char* g_ItemDescriptions[79] = {
    (unsigned char*)s_idesc00.bytes,  // [0x00] item 0x01 - This doesn't seem to be enough for this mission.
    (unsigned char*)s_idesc01.bytes,  // [0x01] item 0x02 - Beretta M92FS. Automatic loaded with 9mm bullets.
    (unsigned char*)s_idesc02.bytes,  // [0x02] item 0x03 - Remington M870. A pump-action shotgun.
    (unsigned char*)s_idesc03.bytes,  // [0x03] item 0x04 - Powerful gun can be loaded with .357 magnum rounds.
    (unsigned char*)s_idesc03.bytes,  // [0x04] item 0x05 - Powerful gun can be loaded with .357 magnum rounds.
    (unsigned char*)s_idesc04.bytes,  // [0x05] item 0x06 - Can throw flame for 9 sec. with max. fuel.
    (unsigned char*)s_idesc05.bytes,  // [0x06] item 0x07 - A launcher can be loaded with various rounds.
    (unsigned char*)s_idesc05.bytes,  // [0x07] item 0x08 - A launcher can be loaded with various rounds.
    (unsigned char*)s_idesc05.bytes,  // [0x08] item 0x09 - A launcher can be loaded with various rounds.
    (unsigned char*)s_idesc06.bytes,  // [0x09] item 0x0A - One shot can destroy any target.
    (unsigned char*)s_idesc07.bytes,  // [0x0A] item 0x0B - Clip for Beretta.
    (unsigned char*)s_idesc08.bytes,  // [0x0B] item 0x0C - Shells for the shotgun.
    (unsigned char*)s_idesc09.bytes,  // [0x0C] item 0x0D - More powerful than magnum rounds.  For the C.Python.
    (unsigned char*)s_idesc10.bytes,  // [0x0D] item 0x0E - .357 magnum rounds. For the Colt Python.
    (unsigned char*)s_idesc11.bytes,  // [0x0E] item 0x0F - Fuel for the flamethrower.
    (unsigned char*)s_idesc12.bytes,  // [0x0F] item 0x10 - Powerful rounds for the Bazooka.
    (unsigned char*)s_idesc12.bytes,  // [0x10] item 0x11 - Powerful rounds for the Bazooka.
    (unsigned char*)s_idesc12.bytes,  // [0x11] item 0x12 - Powerful rounds for the Bazooka.
    (unsigned char*)s_idesc13.bytes,  // [0x12] item 0x13 - A glass bottle to put chemicals in.
    (unsigned char*)s_idesc14.bytes,  // [0x13] item 0x14 - There's water in the bottle.
    (unsigned char*)s_idesc15.bytes,  // [0x14] item 0x15 - This is needed to generate V-JOLT.
    (unsigned char*)s_idesc15.bytes,  // [0x15] item 0x16 - This is needed to generate V-JOLT.
    (unsigned char*)s_idesc16.bytes,  // [0x16] item 0x17 - This is not the chemical I need.
    (unsigned char*)s_idesc16.bytes,  // [0x17] item 0x18 - This is not the chemical I need.
    (unsigned char*)s_idesc16.bytes,  // [0x18] item 0x19 - This is not the chemical I need.
    (unsigned char*)s_idesc16.bytes,  // [0x19] item 0x1A - This is not the chemical I need.
    (unsigned char*)s_idesc17.bytes,  // [0x1A] item 0x1B - Now I can destroy that ivy monster.
    (unsigned char*)s_idesc18.bytes,  // [0x1B] item 0x1C - It's too dangerous to fire! Dose it have another use?
    (unsigned char*)s_idesc19.bytes,  // [0x1C] item 0x1D - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x1D] item 0x1E - Nothing important.
    (unsigned char*)s_idesc20.bytes,  // [0x1E] item 0x1F - There's a scratch. Someone may have used it.
    (unsigned char*)s_idesc20.bytes,  // [0x1F] item 0x20 - There's a scratch. Someone may have used it.
    (unsigned char*)s_idesc21.bytes,  // [0x20] item 0x21 - It's shining beautifully.
    (unsigned char*)s_idesc21.bytes,  // [0x21] item 0x22 - It's shining beautifully.
    (unsigned char*)s_idesc22.bytes,  // [0x22] item 0x23 - The title is "Moonlight Sonata".
    (unsigned char*)s_idesc23.bytes,  // [0x23] item 0x24 - A medal from the second Doom Book.
    (unsigned char*)s_idesc24.bytes,  // [0x24] item 0x25 - A medal from the first Doom Book.
    (unsigned char*)s_idesc19.bytes,  // [0x25] item 0x26 - Nothing important.
    (unsigned char*)s_idesc25.bytes,  // [0x26] item 0x27 - Now I can move the elevator.
    (unsigned char*)s_idesc26.bytes,  // [0x27] item 0x28 - It seems to be a start-up  disk.
    (unsigned char*)s_idesc27.bytes,  // [0x28] item 0x29 - It represents wind.
    (unsigned char*)s_idesc28.bytes,  // [0x29] item 0x2A - I can signal Brad with this.
    (unsigned char*)s_idesc29.bytes,  // [0x2A] item 0x2B - It seems to be some kind of research report.
    (unsigned char*)s_idesc30.bytes,  // [0x2B] item 0x2C - A carving of the moon.
    (unsigned char*)s_idesc31.bytes,  // [0x2C] item 0x2D - A carving of the star.
    (unsigned char*)s_idesc32.bytes,  // [0x2D] item 0x2E - A carving of the sun.
    (unsigned char*)s_idesc33.bytes,  // [0x2E] item 0x2F - It's used with a typewriter.
    (unsigned char*)s_idesc34.bytes,  // [0x2F] item 0x30 - Some fluid is left.
    (unsigned char*)s_idesc35.bytes,  // [0x30] item 0x31 - A simple lock can be opened with this.
    (unsigned char*)s_idesc19.bytes,  // [0x31] item 0x32 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x32] item 0x33 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x33] item 0x34 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x34] item 0x35 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x35] item 0x36 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x36] item 0x37 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x37] item 0x38 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x38] item 0x39 - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x39] item 0x3A - Nothing important.
    (unsigned char*)s_idesc36.bytes,  // [0x3A] item 0x3B - A key to enter the Control Room.
    (unsigned char*)s_idesc19.bytes,  // [0x3B] item 0x3C - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x3C] item 0x3D - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x3D] item 0x3E - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x3E] item 0x3F - Nothing important.
    (unsigned char*)s_idesc19.bytes,  // [0x3F] item 0x40 - Nothing important.
    (unsigned char*)s_idesc37.bytes,  // [0x40] item 0x41 - I can heal any wound with this.
    (unsigned char*)s_idesc38.bytes,  // [0x41] item 0x42 - Only one dose is left.
    (unsigned char*)s_idesc39.bytes,  // [0x42] item 0x43 - It's a local herb.
    (unsigned char*)s_idesc39.bytes,  // [0x43] item 0x44 - It's a local herb.
    (unsigned char*)s_idesc39.bytes,  // [0x44] item 0x45 - It's a local herb.
    (unsigned char*)s_idesc40.bytes,  // [0x45] item 0x46 - It's a mixture of green and red herbs.
    (unsigned char*)s_idesc41.bytes,  // [0x46] item 0x47 - It's a mixture of 2 green herbs.
    (unsigned char*)s_idesc42.bytes,  // [0x47] item 0x48 - It's a mixture of green and blue herbs.
    (unsigned char*)s_idesc43.bytes,  // [0x48] item 0x49 - It's a mixture of green, blue and red herbs.
    (unsigned char*)s_idesc44.bytes,  // [0x49] item 0x4A - It's a mixture of 3 green herbs.
    (unsigned char*)s_idesc45.bytes,  // [0x4A] item 0x4B - It's a mixture of 2 green herbs and blue herb.
    (unsigned char*)s_idesc19.bytes,  // [0x4B] item 0x4C - Nothing important.
    (unsigned char*)s_idesc46.bytes,  // [0x4C] item 0x4D - The battery is still charged.
    (unsigned char*)s_idesc47.bytes,  // [0x4D] item 0x4E - A sub machine gun loaded with 9mm bullets.
    (unsigned char*)s_idesc48.bytes,  // [0x4E] item 0x4F -  A full-automatic light-weight machine gun.
};

// ============================================================================
// External data constants at fixed addresses in original binary
// These are placeholder definitions for ported code.
// ============================================================================
// 0x00606060 — joint animation constant data (referenced by JointApplyColorTint)
DWORD DAT_00606060 = 0;


// Effect sprite texture management state (FUN_0047bc80 / InitRoomEffSprite)
unsigned char  DAT_00bf0a38 = 0;               // 0x00bf0a38
short          DAT_00bf0a3c = 0;               // 0x00bf0a3c
unsigned short DAT_00bf0a3e = 0;               // 0x00bf0a3e
short          DAT_00bf0a40 = 0;               // 0x00bf0a40
unsigned short DAT_00bf0a42 = 0;               // 0x00bf0a42
unsigned char  g_abEffSpriteIndexTable[16] = {};          // 0x00bf0a44

// Effect sprite per-slot image data pointers
int            DAT_00ac9cd0[8] = {};           // 0x00ac9cd0
unsigned char  g_effectSpriteSheetSlot[50] = {};   // per-sprite SRV sheet slot (see Globals.h)
unsigned char  g_effectSpriteBandV[50] = {};       // per-sprite page V offset (see Globals.h)
unsigned char  g_effectSpritePageV[50] = {};       // room sprites: page V blit offset (see Globals.h)
unsigned char  g_effectSpriteClutRows[50] = {};    // per-sprite sheet CLUT row count (see Globals.h)

// Entity joint animation copy base pointer
int            DAT_00be0e00 = 0;               // 0x00be0e00

// Effect sprite stage/room cache
unsigned int   STAGE_ID_00ac9cf0 = 0;          // 0x00ac9cf0
unsigned int   ROOM_ID_00ac9cf4 = 0;           // 0x00ac9cf4


// 0x00bf0a54 - Per-type sprite header pointers (50 DWORDs = 200 bytes)
DWORD   g_effectSpriteInfo[50] = {};

// 0x00bf0b1c - Effect animation data (immediately follows g_effectSpriteInfo[50])
DWORD   g_effectAnimData[425] = {};   // 0x00bf0b1c - per-type effect animation pointers (DWORD array, 1700 bytes)

// One contiguous region, exactly as the original: see the note in Globals.h.
static EntityModelStorage s_entityModelStorage = {};
BYTE (&g_entityModelBuffer)[52224]  = s_entityModelStorage.first;    // 0x00bf11c0
BYTE (&g_entityModelBuffer2)[56320] = s_entityModelStorage.second;   // 0x00bfddc0

// 0x00c0b9c0
BYTE   g_animationBuffer[37888] = {};

// 0x00c133c0 - weapon animation object buffer (LoadEquippedWeaponAnimation param_4)
DWORD  g_animObjectBuffer[0x680] = {};

// Shoot direction ESP data buffer (loaded from core00.esp)
// 0x00c14dc0 - ESP effect data
BYTE   g_shootDirEspBuffer[73728] = {};

// 0x00c26dc0 - General purpose data buffer (832728 bytes)
BYTE    g_DataBuffer[832728];

// Display image buffer for TIM slide loading (at 0x00cf22ac in original)
BYTE    g_TimImageBuffer[187160+20] = {};

// ============================================================================
// Background loading globals
// ============================================================================

// 0x004d46b4 - Background loading mode flag
// 0 = per-camera: load PAK, unpack, display each camera individually
// non-zero = cache: load all camera PAKs into cache buffer with offset table
int            g_bgCacheMode = 1;             // 0x004d46b4 — original value is 1 (cache mode)

// 0x004c2060 - Hex character lookup table for path construction
char           g_hexCharTable[17] = "0123456789abcdef";

// 0x004c2078 - Path template for room background PAK files (mutated at runtime)
// Format: <GAME_DATA_ROOT>stageS\rcSRRC.pak
// Positions 0x0B, 0x0F = stage digit, 0x10 = room high nibble, 0x11 = room low nibble
// Position 0x12 = camera digit (cache mode only)
// Built from GAME_DATA_ROOT so the debug asset root is substituted at compile
// time. The characters below are patched by index at runtime, and the original's
// indices are relative to its own 6-character root - call sites go through
// GAME_DATA_PATH_IDX() rather than hardcoding. Sized with headroom for the longer
// debug root.
char           g_bgPathTemplate[40] = GAME_DATA_ROOT "stageS\\rcSRRC.pak";

// 0x004c2090 - Camera hex char (stored after path template)
char           DAT_004c2090 = 0;

// ============================================================================
// Critical small globals - declared BEFORE large buffers to protect them from
// buffer overflows. In the original binary g_ItemSlotsPointer (0x00d22768) is
// ~2MB after g_bgCacheBuffer (0x00b0a0d0), so forward overflow never reaches it.
// In the recompiled binary the linker may place them adjacent, so we force them
// to lower addresses by declaring them first.
// ============================================================================

// 0x008f87b0 - Backup of player health during character switch
short          HEALTH_BKP = 0;

// 0x008f87b4 - Backup of health status flags during character switch
unsigned short HEALTH_STATUS_BKP = 0;

// 0x00d22768 - Pointer to active character's item slots (g_ItemsSlots or g_RebeccaItemSlots)
void*          g_ItemSlotsPointer = NULL;
unsigned char* g_pCurrentItemSlot = NULL;               // 0x00d226f0

// 0x00aea08c/0x00aea090 - Per-camera offset into g_bgCacheBuffer
int            g_bgCameraOffsets[17] = {};

// 0x00ae9e80 - Per-camera mask offset into g_bgMaskDataBuffer
int            g_bgMaskOffsets[16] = {};

// 0x00ac9e80 - Mask data buffer (128KB)
BYTE           g_bgMaskDataBuffer[0x20000] = {};

// 0x004c3bc8 - Path template for mask PAK files (mutated at runtime)
// Format: <GAME_DATA_ROOT>objspr\osp0SRRC.pak
// Position 0x11 = stageId+'0', 0x12/0x13 = roomId decimal digits, 0x14 = camera index+'0'
// Same treatment as g_bgPathTemplate - see the note there.
char           g_maskPathTemplate[40] = GAME_DATA_ROOT "objspr\\osp0SRRC.pak";

// ============================================================================
// LZW decompression state (unpack_pakfile_ at 0x00425ab0)
// ============================================================================

// 0x00d2b0a4 - Current byte offset into input data
unsigned int   g_pakDecompInputPos = 0;

// 0x00d91a64 - Current bit mask (starts at 0x80, shifts right)
unsigned int   g_pakDecompBitMask = 0x80;

// 0x00d227c4 - Current byte buffer from input stream
unsigned int   g_pakDecompCurByte = 0;

// 0x00d2b0a8 - Current code size in bits (starts at 9)
unsigned int   g_pakDecompCodeSize = 9;

// 0x00d227c8 - Next free dictionary code (starts at 0x103)
unsigned int   g_pakDecompNextCode = 0x103;

// 0x00d2b0a0 - Maximum code value for current code size
unsigned int   g_pakDecompMaxCode = 0x1ff;

// 0x00d2b0b0 - LZW dictionary: one array of 12-byte records. The prefix field
// (+4) and char field (+8) were previously declared as two separate arrays,
// which put every dictionary access at the wrong address. See Globals.h.
PakDictEntry   g_pakDict[PAK_DICT_ENTRIES] = {};

// 0x00d227d0 - String output buffer for building decoded strings
char           g_pakStringBuf[512] = {};

// ============================================================================
// Room data globals (populated by room_set from RDT file data)
// ============================================================================

// 0x00d226b0 - Room-object (omodel) record table. Each slot points at one
// 0xA4-byte record carved from the RDT VB region by room_set (count = RDT
// omodel_slot_count) and filled by cmd_omodel_set; walked by
// update_room_objects / check_climb_object and targetable from SCD scripts.
void*          g_omodel_table[8] = {};

// 0x00d21360 - Item model record table. One 0xA4-byte record per RDT item_models
// pair (count = RDT item_count), carved from the VB region by room_set and filled
// by cmd_item_model_set (SCD 0x18): the 3D model of each pick-up in the room -
// the one lying on the floor, or the one a desk close-up reveals. Byte 0 bit 0 is
// "currently drawn" (renderer pass 1); pickup clears it and frees the sparkle
// billboard at +0x86. Also an entity target for event scripts.
void*          g_item_model_table[8] = {};

// 0x00ae9ef4 - Count of object models loaded by cmd_omodel_set in current room
int            g_omodelCount = 0;

// 0x00bebcc9 - Last enemy model ID loaded during room_set enemy loop (for model reuse caching)
unsigned char  g_LastEnemyModelId = 0xff;

// 0x00ae9eec - Count of item/obstacle models loaded by cmd_item_model_set in current room
int            g_ItemModelCount = 0;

// 0x00d213bc - SCD opcodes pointer for room initialization script (set by LoadRoomRdt)
void*          g_RoomInitScd = NULL;

// 0x00bebccc - Current RDT data type pointer (set to cam_switch_zones by room_set)
void*          g_CurrentRdtDataTypePtr = NULL;

// 0x00d213c0 - Data pointer saved for stage 2 room 3 (into g_loadDataDestPointer)
void*          DAT_00d213c0 = NULL;

// 0x00bf084c - SCD event execution context table (8 entries x 0x34 bytes)
// Each entry tracks a running room event script executed by room_events_check.
ScdEventEntry  g_ScdEventTable[8] = {};

// SCD event system globals
ScdEventEntry* g_pScdEventCurrent = NULL;               // 0x00bf0848
unsigned char* g_ScdOpcodes = NULL;                     // 0x00bf0800
unsigned int*  g_CmdOpcodesPointer = NULL;              // 0x00bf0804
unsigned char  g_ScriptContinueFlag = 0;                // 0x00bf07fa
unsigned char* g_RoomEventScripts = NULL;                     // 0x00d213b4
unsigned char* g_RoomScdOpcodes = NULL;                  // 0x00d213b8

// Per-frame item-use flag bank (SCD flag bank 9) - see Globals.h
unsigned int   g_itemUseFlags[2] = {};                // 0x00d213a0

// Mirror (planar reflection) parameters - set by SCD opcode 0x0F
unsigned short g_mirrorExtentMin = 0;                   // 0x00d211c4
unsigned short g_mirrorExtentMax = 0;                   // 0x00d21350
unsigned short g_mirrorPlaneCoord = 0;                  // 0x00d2276c
unsigned int   DAT_00d22770 = 0;                        // 0x00d22770

// Sound system BGM state
unsigned char  DAT_00bf07ef = 0;                        // 0x00bf07ef
// 0x00bf07f0 == g_targetBgmState; the duplicate DAT_00bf07f0 was removed.

// Room BGM state table
unsigned char  g_abRoomBgmState[224] = {};              // 0x00ac98e8

// Screen effect parameter storage
SndPanVol      g_SndPanVol[3] = {};                     // 0x00ac98e0 (bound 0x00ac98f8)
int            DAT_00ac98f8 = 0;                        // 0x00ac98f8

// Special room lighting globals (also used by MainLoop.cpp)
int            g_SpecialR1 = 0;           // 0x00be961d
int            g_SpecialG1 = 0;           // 0x00be961e
int            g_SpecialB1 = 0;           // 0x00be961f

// TMD model caching state
int*           DAT_00bca0d0 = NULL;                     // 0x00bca0d0
int*           DAT_00bca0d4 = NULL;                     // 0x00bca0d4
unsigned char  DAT_008e1c78 = 0;                        // 0x008e1c78
unsigned char  DAT_008e1c70 = 0;                        // 0x008e1c70
unsigned char  DAT_008e1c7c = 0;                        // 0x008e1c7c
unsigned char  DAT_008e1c74 = 0;                        // 0x008e1c74

// Bullet effect parent sprite info pointer
int            DAT_00bf0a34 = 0;                        // 0x00bf0a34

// ============================================================================
// game_loop globals (0x00480b30)
// ============================================================================

// 0x00d22760 - Menu open state machine: 0=none, 1=open requested, 2=initializing, 3=closing
int            g_openMenuFlag = 0;

// 0x00bebcc2 - Backup of g_message_flags when menu is requested
unsigned short g_short_message_flags = 0xFFFF;

// 0x008f8898 - Saved special room light state across room transitions
int            g_int_008f8898 = -1;

// 0x004d2294 - Countdown frame counter (counts 0-29, then increments countdown timer)
int            DAT_004d2294 = 0;

// 0x004d2288 - Death delay countdown (set to 90 frames before triggering fade)
int            DAT_004d2288 = 0;

// 0x004d46a4 - room-object draw enable flag (Ghidra: g_dwCameraLightingEnabled)
// 0x004d46a4 / 0x004d46a8 - render feature gates, both statically 1 in the
// original .data and never written. See the note in Globals.h before changing.
int            g_dwRoomObjectRenderEnabled = 1;
int            g_dwEntityRenderEnabled = 1;

// 0x004d4680 - Debug save menu display trigger (toggled by debug key)
int            g_displayDebugSaveMenu = 0;

// 0x004d4684 - Debug save menu state flag (prevents re-trigger until re-armed)
int            g_debugSaveMenuFlag = 0;

// 0x004d461c - Video-mode debug overlay active flag (cleared by
// ResetFmvRenderState 0x004973a0; read/written by ShowVideoModeDebugText)
BYTE           g_VideoModeOverlayActive = 0;

// 0x004d4600 - Video-mode debug overlay toggle latch (set by the debug key
// handler, consumed + turned into a 60-frame timer by ShowVideoModeDebugText)
int            g_ShowVideoModeOverlay = 0;

// 0x004d475c - Video-mode debug overlay frame countdown
int            g_VideoModeOverlayTimer = 0;

// 0x004d228c - Menu processing active flag (set during menu open/close)
int            DAT_004d228c = 0;

// Master switch for the port-added debug features (Globals.h). Default 1 in
// debug builds, 0 in release; [Debug] EnableDebug in config.ini overrides.
#ifdef _DEBUG
int            g_debugFeaturesEnabled = 1;
#else
int            g_debugFeaturesEnabled = 0;
#endif

// Director's Cut mode (port-added steering globals, Globals.h). g_bDcMode is
// [Game] DcMode; g_DcDifficulty is the DC title screen's STANDARD/TRAINING/
// ADVANCED choice (PS1 g_abDcGameMode), also written into the save block.
int            g_GameMode = GAME_MODE_OG;
int            g_DcDifficulty = DC_DIFFICULTY_STANDARD;
bool           g_bPs1EndingCredits = false;
bool           g_bPs1FmvSubtitles = false;
bool           g_bSkipUnskippableFmv = false;

// ---------------------------------------------------------------------------
// re1_rand / re1_srand (port-only)
// The PS1's rand (SLUS_005.51 0x8005f6d0, SLUS_001.70 equivalent 0x8005f7a4)
// is seed*0x41c64e6d + 0x3039, returning (state >> 16) & 0x7fff. The PC
// release called the CRT rand (MSVC: state*214013 + 2531011), and the PC
// demo reels were recorded against THAT sequence. So the stream is mode
// data, not a detail: DC sessions must produce the PS1 sequence or every
// attract demo desyncs from its first random draw (zombie health rolls,
// knockdown thresholds, Beretta insta-kill), while OG must keep the CRT
// sequence the PC recordings encode.
// ---------------------------------------------------------------------------
static unsigned int g_randState = 1;

// extern "C": the rand/srand macros in Globals.h rewrite the CRT's own
// declarations inside <stdlib.h>'s extern "C" block, so the port functions
// must carry C linkage or every TU that includes <cstdlib> after Globals.h
// fails to link.
extern "C" void re1_srand(unsigned int seed)
{
    g_randState = seed;
}

extern "C" int re1_rand(void)
{
    if (g_GameMode == GAME_MODE_DC) {
        g_randState = g_randState * 0x41c64e6d + 0x3039;
        return (int)((g_randState >> 16) & 0x7fff);
    }
    g_randState = g_randState * 214013 + 2531011;
    return (int)((g_randState >> 16) & 0x7fff);
}

#if defined(__linux__)
extern "C" int rand() noexcept
{
    return re1_rand();
}

extern "C" void srand(unsigned int seed) noexcept
{
    re1_srand(seed);
}
#endif

// Port-added debug helpers (no original address; set by F6 in WindowProc
// and by the F1 debug menu's QUICK ACCESS screen). F6 requests the texture viewer
// overlay, g_debugOpenLoadScreenFlag requests the load screen (title flow).
int            g_debugOpenLoadScreenFlag = 0;
int            g_debugLoadSlot = 0;    // quick access load: selected slot index
int            g_debugOpenItemboxFlag = 0;
int            g_debugTextureViewerFlag = 0;
int            g_debugTextureViewerOpen = 0;
