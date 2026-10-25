#pragma once

#include <cstdlib>

#include "DebugPrint.h"   // dbg_printf / dbg_safe_str

// Ban raw OutputDebugStringA: it fail-fasts when called from a task on the
// scheduler's switched stack with no debugger attached (killed standalone
// Release runs at boot). All output must go through dbg_safe_str/dbg_printf.
#undef OutputDebugStringA
#define OutputDebugStringA(s) dbg_safe_str((const char*)(s))

// Core types and structures
#include "game/Types.h"
#include "game/Entities.h"
#include "game/BioCard.h"

// ============================================================================
// Globals.h — all shared globals and cross-module function declarations
// ----------------------------------------------------------------------------
// Organization (search for "SECTION:"):
//   1. Window & display system
//   2. Timing & frame pacing
//   3. Installation / registry / shared memory
//   4. Marni system (D3D, texture pages, video driver arrays)
//   5. Input (keyboard/pad pipeline)
//   6. Task scheduler
//   7. Core game state (state flags, messages, fade, screen tint)
//   8. Player, entities & effects (game-init wipe block)
//   9. Bio card & items
//  10. Rooms (RDT, SCD scripts, backgrounds, PAK/LZW, lab slides)
//  11. In-game menu
//  12. Title screen & attract demo
//  13. Save / load
//  14. Sound system
//  15. FMV / MCI video
//  16. Rendering, sprites, textures, TMD, animation
//  17. Large data buffers
//  18. Debug & misc
//  19. Function declarations
//
// RULES for adding globals:
//  - Always comment the original address from Ghidra.
//  - BEFORE defining a new global in a .cpp file, check its original address
//    against the placement table in docs/MEMORY_LAYOUT.md. Globals in
//    [0x00be41e0, 0x00be9620) must go into the .gwipe ordered section;
//    [0x00be9620, 0x00be9a3c) belongs to BioCardLayout (BioCard.h).
// ============================================================================

// ============================================================================
// SECTION 1: Window & display system
// ============================================================================

extern HWND          g_hWnd;                           // 0x00bcb2c0
extern HINSTANCE     g_hInstance;                      // 0x00bcb2c4
extern HANDLE        g_hMutex;                         // 0x00bcb2c8
extern BOOL          g_bIsSoftwareRendering;           // 0x004bcb2c
extern BOOL          g_isGameCursorHiddenFlag;         // 0x00be0e29
extern BOOL          g_bHasFinalizedSettings;          // 0x004bcb78

// Window/message-pump state flags
extern BOOL          g_isPaused;                       // 0x004d46ac - SideWinder pause-button event (read+cleared by main_loop, injects START+bit8)
extern BOOL          g_bWindowFocused;                 // 0x004bcb2c - window focused (WM_ACTIVATE), read by message pump
extern BOOL          g_bWindowActive;                  // 0x004bcb30
extern BOOL          g_bQuitFlag;                      // 0x004bcb40
extern BOOL          g_bAccessibilityAnimations;

// Display / adapter selection
extern DWORD         g_dwSelectedDisplayAdapterID;     // 0x007d9148
extern DWORD         g_dwSelectedDisplayModeID;        // 0x007d914c
extern DWORD         g_dwScreenWidth;                  // 0x007d9150
extern DWORD         g_dwScreenHeight;                 // 0x007d9154
extern BOOL          g_bFullScreen;
extern BOOL          g_bVSync;                         // config.ini [Display] VSync                    // 0x007d9158
extern int           g_dwBitDepth;                     // 0x004d642c
extern DWORD         g_GPU_VENDOR_ID;                  // 0x004bcb64

// Display mode storage
extern DisplayModeInfo g_DisplayModeBuffer[MAX_DISPLAY_MODES];
extern int           g_NumDisplayModes;                // 0x007d8f24

// D3D Renderer info
extern D3DRendererInfo g_D3DRenderers[8];
extern int           g_NumD3DRenderersAvailable;       // 0x007e0e08

// Display resolution / image (title screen display_image)
extern int           g_displayWidth;                   // 0x00bf09f8
extern int           g_displayHeight;                  // 0x00bf09fc
extern int           g_displayMode;                    // 0x00bf09f4
extern int           g_DisplayImageWidth;              // 0x004c3364
extern int           g_DisplayImageHeight;             // 0x004c3368

// Screen origin / shake offsets
extern short         g_ScreenOffsetX;                  // 0x004bcac8
extern short         g_ScreenOffsetY;                  // 0x004bcaca
extern signed char   g_ScreenShakeOffsetX;             // 0x00bca0d8
extern signed char   g_ScreenShakeOffsetY;             // 0x00bca0d9

// ============================================================================
// SECTION 2: Timing & frame pacing
// ============================================================================

extern DWORD         g_dwSystemTimer1;                 // 0x007e0df4
extern DWORD         g_dwGameTimer1;                   // 0x007e0df8
extern DWORD         g_GameInitTime;                   // 0x004d46c4
extern DWORD         Game_timer;                       // 0x00d22730
extern DWORD         DAT_004d46d4;                     // 0x004d46d4
extern DWORD         g_LastFrameTime_ms;               // 0x004d45fc

// Frame rate governor
extern int           g_frameTimeIndex;                 // 0x004d45f8
extern int           g_frameTimeBuffer[4];             // 0x00ac4000
extern int           g_frameTimeAccumulator;           // 0x004d45f4
extern int           g_frameTargetTime;                // 0x004d45ec
extern int           g_ScreenAccessReady;              // 0x004d4658
extern int           g_ScreenAccessCountdown;          // 0x004d4684
extern int           g_RenderAccessReady;              // 0x004d4688
extern int           g_MarniScreenReady;               // 0x00497340
extern DWORD         g_MarniScreenColor;               // 0x00497360
extern int           g_ScreenAccessCheck;              // 0x004d2290
extern int           g_RenderAccessCheck;
extern BOOL          g_bUseFrameSkip;
extern BOOL          g_bFrameSkipDetected;             // 0x004d46dc

// Frame counters
extern int           g_numFramesRendered;
extern int           g_numFramesPresented;
extern int           g_loopCounter;

// ============================================================================
// SECTION 3: Installation / registry / shared memory
// ============================================================================

// Installation path
extern char          g_szInstallPath[MAX_PATH];        // 0x00d91bd0
extern char          g_szCreateDir[260];

// Registry loaded data
extern BYTE          g_keyBindingData[32];             // 0x004d4730 - "Key Def" (VK code per PS1 button bit)
extern BYTE          g_joystickBindingData[128];
// One-shot event: raised when the pad wants a START injection, consumed and
// cleared by main_loop (0x00428f4a). This is the original 0x004d46b0 flag set
// by the dispatch at 0x00497877 - NOT a capability flag. Do not test it to ask
// "is a pad plugged in"; use g_bPadConnected for that.
extern BOOL          g_isSideWinderConnected;          // 0x004d46b0
// Persistent capability: a game pad is present. The original conflated this
// with the one-shot above (see docs); the title screen, character select,
// save/load screen and the joystick remap-table selection all mean this one.
extern BOOL          g_bPadConnected;
extern const DWORD   g_JoyRemapTblLegacyJoyDefault[32];
extern BYTE          g_InstallFlagData;
extern int           g_InstallFlagDataLoaded;
// The registry pair. LoadInstallationConfiguration reads "Play Number" into
// 0x004d63f4 (0x0040b159) and "Clear Number" into 0x004d6430 (0x0040b19f);
// ending_state increments the latter.
extern DWORD         g_dwPlayCount;                    // 0x004d63f4
extern DWORD         g_dwClearCount;                   // 0x004d6430

// Drive types
extern UINT          g_DriveTypes[MAX_DRIVES];
extern char          g_DriveLetterBuffer[256];

// Shared memory
extern HANDLE        g_hFileMapping;                   // 0x007dfd20
extern BYTE*         g_pSharedMemory;

// ============================================================================
// SECTION 4: Marni system (D3D, texture pages, video driver arrays)
// ============================================================================

// Marni System objects
extern void*         g_pMarniDirect3D;                 // 0x00ac4028 - lives in .sched (see docs/MEMORY_LAYOUT.md)

int __stdcall VideoDriver_ClearState348(void* obj, void* context);
int            VideoDriver_ReleaseResources(void* obj, void* context);   // 0x00421150
int            Direct3DTIM_Create(void* pagePtr, void* context);         // FUN_00421070 (0x00421070)

// Texture page table and per-page attribute arrays
extern void*         g_TexturePageTable;
extern DWORD         g_TexturePageTable_DAT[256];
extern MarniHandle   g_TexturePageSRV[256];
extern int           g_TexturePageWidth[256];
extern int           g_TexturePageHeight[256];
extern int           g_TexturePageBpp[256];
extern short         g_TexturePageOriginX[256];   // VRAM X origin (halfwords)
extern short         g_TexturePageOriginY[256];   // VRAM Y origin (scanlines)
extern short         g_TexturePageId[256];        // tpage code assigned to each slot;
                                                  // what TextureDesc::texturePage is matched against
extern short         g_TexturePageClutBase[256];  // base clutY (= pageOffset + 0x1E0)
extern int           g_texturePageMode;
extern int           g_texturePageHandle;
extern void*         g_texturePageSrcDesc;   // port-only: last create_texture_page source, for diagnostics

// Marni video driver arrays
extern DWORD         g_VideoDriverArray_D0[64];
extern DWORD         g_VideoDriverArray_03c[64];
extern DWORD         g_VideoDriverArray_04c[64];
extern DWORD         g_VideoDriverArray_068[64];
extern DWORD         g_VideoDriverArray_06c[64];
extern DWORD         g_VideoDriverArray_4d0[1024];
extern DWORD         g_VideoDriverArray_4fa[256];
extern DWORD         g_VideoDriverArray_4fc[256];
extern DWORD         g_VideoDriverArray_500[256];
extern DWORD         g_VideoDriverArray_810[256];
extern DWORD         g_VideoDriverArray_814[256];
extern DWORD         g_VideoDriverArray_838[2048];
extern DWORD         g_VideoDriverArray_520[2048];
extern short         g_VideoDriverArray_FA[256];

// Marni async / work buffers
extern int           g_AsyncResult;
extern int           g_ExecuteBufferHandle;
extern CMarniBits    g_MarniBitsWorkBuffer;
extern DWORD         g_MarniBitsOutput;
extern CMarniBits    g_MarniFrameBuffer;
extern DWORD         g_ObjectWorkBuffer[64];

// ============================================================================
// SECTION 5: Input (keyboard/pad pipeline)
// ----------------------------------------------------------------------------
// Pipeline: GetAsyncKeyState(keyMap) → g_pMasterInputState.keyboardPrev →
// ReadPadBoth/JoyToPSX → g_RawPadHeld → PlayerPad_Update edge detect →
// g_PlayerPadHeld (raw PSX word) / g_PlayerDpadHeld (remapped, see BioCard.h).
// ============================================================================

// 0x00ac4030 - MasterInputState (keyboard + joystick states)
// Lives in .sched: the game-init memclr must never wipe the keyMap
// (see docs/MEMORY_LAYOUT.md incident history).
extern MasterInputState g_pMasterInputState;

extern DWORD g_lastScanCodeOrMsgID;                    // 0x00bcb2e0

// Raw PSX-word pad state (edge detection in PlayerPad_Update)
extern DWORD g_RawPadHeld;                             // 0x00bf0a04 - raw held state (input to edge detect)
extern DWORD g_PlayerPadPressed;                       // 0x00bf0a08
extern DWORD g_button_pressed_id;                      // 0x00bf0a0c
extern DWORD g_PlayerPadHeld;                          // 0x00bf0a10 - edge-detected held state (output)
extern DWORD g_PlayerPadHeldPrev;                      // 0x004bae30

// Joystick/controller globals (used by ReadPadBoth / JoyToPSX)
// NOTE: g_PadActiveP1 (0x00ac422c), g_PadActiveP2 (0x00ac4404),
// g_PadRawP1 (0x00ac4058), g_PadRawP2 (0x00ac4230) are aliases for
// g_pMasterInputState fields and have been merged into the struct.
extern DWORD g_PadBtnWord;                             // 0x00ac4018
extern int   g_NumControllers;                         // 0x00ac7b58
extern int   g_JoyWarnPrinted;                         // 0x004b1958
// NOT const: the original's table at 0x004b1858 is mutable .data — the save
// screen's load path restores it from the save file (memcpy into it).
extern DWORD g_JoyRemapTbl[2][32];                     // 0x004b1858 - PC joystick → PSX button remap
extern unsigned int g_joyRemapBackupKey[32];           // 0x004d3f58 - joy remap backup (keyboard)
extern unsigned int g_joyRemapBackupJoy[32];           // 0x004d3fd8 - joy remap backup (joystick)
extern BOOL  g_DisablePad;                             // 0x004bcb3c

// Pad remap tables and dpad globals (used by PlayerPad_Update)
extern const WORD* g_padRemapTable[4];                 // 0x004bf300 - pointers to remap sub-tables
extern WORD g_padRemapSubTable3[16];                   // 0x00be9a3c - runtime configurable remap table
extern WORD g_PlayerDpadHeldPrev;                      // 0x00bf0a14 - previous dpad held state
// g_PlayerDpadHeld / g_PlayerDpadPressed are macros to g_BioCard fields (see BioCard.h)
extern WORD g_demoPadData[1202];                        // 0x00d21d10 - attract demo input data
                                                        // (0x00d21d10..0x00d22674, filled by the
                                                        // whole-file pdemoN.dat load; a reel is up
                                                        // to 1046 words, NOT 512)
extern unsigned char g_controllerConfig;               // 0x00be9a5c

// Player input configuration fields (set by InitPlayerInputData)
extern int           g_PlayerInputConfig_3c;
extern int           g_PlayerInputConfig_3e;
extern int           g_PlayerInputConfig_40;
extern int           g_PlayerInputConfig_42;
extern int           g_PlayerInputConfig_44;
extern int           g_PlayerInputConfig_46;
extern int           g_PlayerInputConfig_48;
extern int           g_PlayerInputConfig_4a;
extern int           g_PlayerInputConfig_4c;
extern int           g_PlayerInputConfig_4e;
extern int           g_PlayerInputConfig_50;
extern int           g_PlayerInputConfig_52;
extern int           g_PlayerInputConfig_54;
extern int           g_PlayerInputConfig_56;
extern int           g_PlayerInputConfig_58;
extern int           g_PlayerInputConfig_5a;

// Key binding data area
extern BYTE          g_KeyBindingVectors[32];
extern DWORD         g_KeyBindingConfig[32];
extern BYTE          g_MasterInputState[256];

// ============================================================================
// SECTION 6: Task scheduler
// ----------------------------------------------------------------------------
// All of these live in the .sched section (see docs/MEMORY_LAYOUT.md and
// docs/TASK_SCHEDULER.md).
// ============================================================================

extern void*         g_StackPointer;                   // 0x007e0cc8
extern TaskControlBlock  g_TasksTable[3];              // 0x00d1fde4
extern TaskControlBlock* g_CurrentTask;                // 0x00bf09ec
extern uintptr_t     g_TasksESP[3];                    // 0x00d91a70
extern void*         g_TasksEIP[3];                    // 0x00d91a80
extern DWORD         g_CurrentTaskID;                  // 0x00d91a7c
extern TaskControlBlock* g_CurrentTaskPtr;             // 0x00d91a68
extern uintptr_t     g_SchedulerESP;                   // 0x00d91a8c
extern DWORD         g_SchedulerRunningFlag;           // 0x004ba0b8
extern void*         g_AsyncRpcCallback;               // 0x00d91a90

// Task data arrays
extern DWORD         g_TaskDataArray_ba750[16];
extern DWORD         g_TaskDataArray_ba780[16];

// Game init
extern int           init_game_flag;                   // 0x004ba7b0

// ============================================================================
// SECTION 7: Core game state (state flags, messages, fade, screen tint)
// ============================================================================

// ============================================================================
// g_main_state_flags - bank 5 of the SCD flag banks (0x00be41c0)
//
// The runtime state word: what the frame is currently doing. Zeroed by
// GameInit and game_start; never saved. Every bit is documented, with its
// writers and readers, in docs/SCENARIO_FLAGS.md - keep that table as the
// source of truth rather than duplicating it here. The shape, in brief:
//
//   bits 0-7    per-frame gameplay state (mirror pass + axis, deferred camera
//               redisplay, ladder latch, object push, door transition)
//   bits 8-15   the MENU MODE byte. Not eight flags: main_menu priority-encodes
//               bits 9-13 into modes 5..1 (0x200 mode 5 ... 0x2000 mode 1, none
//               = mode 0), bit 8 short-circuits to the pickup screen, bit 15 is
//               the re-entry lock. `msf & 0x7F00` means "a menu or message mode
//               is pending" and is the standard interaction gate;
//               menu_restore_game_state clears the byte with &= 0xFFFF00FF.
//   bits 16-29  subsystem state (screen intensity ramp, voice playing, FMV
//               requested, panning reset, camera lock, options requested,
//               Chris/Jill RDT variant, player dead, gameplay active, room
//               transition running, continue-vs-new-game, fade running)
//   bits 30-31  a two-bit SCREEN MODE, never set independently: every writer
//               does (msf & 0x3FFFFFFF) | 0x40000000 or | 0x80000000. Bit 30 =
//               standalone screen (flat colour present, background quad and
//               world sprites suppressed), bit 31 = full sprite rebuild.
//
// Bits 3, 14, 21 have no reader or writer; bit 27 appears only inside clear
// masks. Note that main_loop's 0x4008000 test is bits 26 AND 15, not 27 and 26.
//
// SCD bank 5 SPILLS INTO g_main_state_flags2: a script `sel` of 0x20 or more
// resolves to byte offset 4, i.e. the adjacent dword below. The boulder-tunnel
// screen shake is bank 5 bit 0x21 = msf2 bit 1.
// ============================================================================
// Bit constants. Names follow docs/SCENARIO_FLAGS.md; use these instead of
// raw masks so a mask can be traced back to a meaning.
#define MSF_MIRROR_ENABLE            0x00000001u  // mirror pass on (cmd_mirror_set writes bits 0-1)
#define MSF_MIRROR_PLANE_X           0x00000002u  // mirror plane axis: 0 = Z, 1 = X
#define MSF_CAMERA_DEFER             0x00000004u  // camera switch raises MSF_CAMERA_REDRAW instead of redrawing now
#define MSF_SCRIPT_ONLY_03           0x00000008u  // no reader in ported code; 18 room scripts SET it (bank 5 sel 0x1C)
#define MSF_LADDER_DOWN              0x00000010u  // ladder/stairs latch: behaviour 0x0B instead of 0x11
#define MSF_CAMERA_REDRAW            0x00000020u  // pending background redisplay, consumed once by game_loop
#define MSF_OBJECT_PUSH              0x00000040u  // object push running; forces behaviour 0x10
#define MSF_DOOR_TRANSITION          0x00000080u  // door transition in progress

// Byte 1 is the MENU MODE field, not eight flags - main_menu priority-encodes
// bits 9-13 into modes 5..1 and falls out at mode 0. See docs/SCENARIO_FLAGS.md.
#define MSF_PICKUP_SCREEN            0x00000100u  // -> main_menu state 8, tested before the mode scan
#define MSF_MENU_MODE_5              0x00000200u  // mode 5 - no C writer, but 7 room scripts set it (bank 5 sel 0x16)
#define MSF_MENU_MODE_GOT_ITEM       0x00000400u  // mode 4 - cmd_got_item
#define MSF_MENU_MODE_ITEM_VIEW      0x00000800u  // mode 3 - item viewer / examine
#define MSF_MENU_MODE_ITEMBOX        0x00001000u  // mode 2 - item box
#define MSF_MENU_MODE_KEY_DEPLETED   0x00002000u  // mode 1 - key item used up, "drop it" prompt
#define MSF_SCRIPT_ONLY_14           0x00004000u  // outside the mode ladder; 14 stage-3 event scripts SET it (bank 5 sel 0x11)
#define MSF_MENU_ACTIVE              0x00008000u  // menu task re-entry lock
#define MSF_MENU_BYTE                0x0000FF00u  // the whole field; cleared on menu close
#define MSF_MENU_PENDING             0x00007F00u  // bits 8-14: "a menu or message mode is pending"
#define MSF_MENU_MODE_SHIFT_BASE     0x00004000u  // main_menu scans (BASE >> n) for n = 5..1

#define MSF_INTENSITY_RAMP           0x00010000u  // ramp g_spriteAnimIntensity up while set
#define MSF_VOICE_PLAYING            0x00020000u  // voice/SFX line playing; polled as "wait for it"
#define MSF_FMV_REQUEST              0x00040000u  // main_loop consumes it and plays g_selectedFmvId
#define MSF_PANNING_RESET            0x00080000u  // ResetScreenPanning every frame (ending)
#define MSF_CAMERA_LOCK              0x00100000u  // camera zone switching disabled (cutscene)
#define MSF_UNUSED_21                0x00200000u  // no reader or writer, in code or in any RDT script
#define MSF_OPTIONS_REQUEST          0x00400000u  // open options_menu instead of main_menu
#define MSF_CHAR_VARIANT             0x00800000u  // room RDT variant: 0 = Chris, 1 = Jill
#define MSF_PLAYER_DEAD              0x01000000u  // player dead / ending fade running
#define MSF_GAMEPLAY_ACTIVE          0x02000000u  // game_loop is running
#define MSF_ROOM_TRANSITION          0x04000000u  // room/door transition animation running
#define MSF_UNUSED_27                0x08000000u  // no writer in code or scripts; only ever cleared
#define MSF_CONTINUE_GAME            0x10000000u  // continue/load (set) vs new game (clear)
#define MSF_FADE_ACTIVE              0x20000000u  // fade transition in progress

// Bits 30-31 are a two-bit SCREEN MODE, never set independently: every writer
// clears both and selects one. Bit 30 is tested first.
#define MSF_SCREEN_STANDALONE        0x40000000u  // flat colour present; bg quad and world sprites suppressed
#define MSF_SCREEN_REBUILD           0x80000000u  // full sprite rebuild present
#define MSF_SCREEN_MODE_MASK         0xC0000000u

// Wholesale reset masks, kept as values because they are not a union of
// meanings - each is "what this screen chooses to preserve".
#define MSF_ROOM_RESET_MASK          0x0000000Fu  // room_set clears the low nibble
#define MSF_DEATH_KEEP_MASK          0xD1FD003Fu  // death_state keeps these
#define MSF_GAMESTART_KEEP_MASK      0xD4E900F0u  // game_start keeps these

// Bank 5 storage. The two names below are the two dwords of ONE array, because
// that is what the original has: cmd_bit_test / cmd_bit_op resolve bank 5 to a
// single base (0x00be41c0) and then add (sel & 0xE0) >> 3 as a byte offset, so
// selector 0x20+ addresses the second dword. Keeping them as separate globals
// made that offset run off the end of a 4-byte object and land wherever the
// linker happened to put the other one.
extern DWORD         g_MainStateFlagBank[2];           // 0x00be41c0
#define g_main_state_flags   (g_MainStateFlagBank[0])  // 0x00be41c0
#define g_main_state_flags2  (g_MainStateFlagBank[1])  // 0x00be41c4
// ============================================================================
// g_main_state_flags2 - the second half of SCD flag bank 5 (0x00be41c4)
//
// Adjacent to g_main_state_flags, and scripts reach it through the SAME bank:
// a bank-5 selector of 0x20 or more resolves to byte offset 4. Selector
// id = 0x20 + (31 - bit), so e.g. the screen-shake bit 1 is script selector
// 0x3E. Zeroed by GameInit and game_start; never saved.
//
// Bits 4-18, 20 and 30 have no reader or writer in code, and no RDT script
// writes them either.
// ============================================================================
#define MSF2_EFFECT_ZONE             0x00000001u  // room_action_effect (room action 0x0B) is running this frame:
                                                  // dust billboard under a moving player, health forced to 1
                                                  // (cannot die), footstep sound type shifted, projectile
                                                  // effects change. game_loop clears it after update_player_anim
#define MSF2_SCREEN_SHAKE            0x00000002u  // screen shake enable (boulder tunnels); main_loop also
                                                  // requires MSF_MENU_BYTE to be clear. 64 script writers
#define MSF2_SCREEN_BORDER           0x00000004u  // backgrounds load as 316x236 instead of 320x240 and
                                                  // ResetScreenAndRebuildSprites is skipped. Parked in
                                                  // g_controllerConfig bit 0x10 while the options menu is open
#define MSF2_FADE_NO_DEPTH_CLAMP     0x00000008u  // FadeSprite skips its depth clamp (> 0xFEF -> 0xFF0)
#define MSF2_PRESERVED_19            0x00080000u  // no reader or writer found; only ever PRESERVED, by
                                                  // MSF2_RESET_KEEP_MASK. Kept named so the mask stays readable
#define MSF2_SFX_BANK1_HALF          0x00200000u  // sound bank 1 holds 16 entries instead of 32
#define MSF2_DOOR_TURN_PENDING       0x00400000u  // check_door / check_door_side asked for a turn; the door
                                                  // animation consumes it to pick the turn direction
#define MSF2_SND_BUSY                0x00800000u  // sound/BGM busy - the .dor script's play_sfx op waits for it
#define MSF2_DOOR_ANGLE_STEP         0x01000000u  // door animation is stepping the player's facing angle
#define MSF2_ROOM_SPRITES_OFF        0x02000000u  // DrawRoomSpr returns early
#define MSF2_COSTUME_VARIANT         0x04000000u  // alternate costume model selection (EntityModelLoader)
#define MSF2_COUNTDOWN_ACTIVE        0x08000000u  // self-destruct countdown running (game_loop's timer path)
#define MSF2_ATTRACT_DEMO            0x10000000u  // attract-mode demo playback
#define MSF2_PLAYER_INITIALISED      0x20000000u  // raised by InitPlayerData; survives the reset mask
#define MSF2_DEATH_VARIANT           0x80000000u  // picks the death fade length and whether game_loop's die
                                                  // path reports "continue"

#define MSF2_ROOM_RESET_MASK         0x0000000Fu  // room_set clears the low nibble, same as msf
#define MSF2_RESET_KEEP_MASK         0x20080000u  // what game_start / char select / F9 preserve

// --- Content mode (port-added steering globals; no original address) ---
// g_GameMode mirrors [Game] Mode in config.ini and is the SINGLE switch that
// says which release this session is running. It drives two things that must
// never disagree: the code branches below, and the asset overlay folder that
// ResolveAssetRoot searches ahead of the base tree (SetAssetMode in
// system/AssetPath.h). Having one knob for both is deliberate - two settings
// would let the executable boot DC code against OG assets, which looks like a
// content bug and is not one.
//
// GAME_MODE_OG is the PC release, which is the original PS1 game's content; it
// uses no overlay at all, so every path resolves exactly as it did before modes
// existed. SATURN and NDS are placeholders: their names reserve the folder and
// the enum value, and neither has any code behind it yet.
#define GAME_MODE_OG                 0   // PC / original PS1 content (default)
#define GAME_MODE_DC                 1   // Director's Cut (SLUS_005.51)
#define GAME_MODE_SATURN             2   // reserved, not implemented
#define GAME_MODE_NDS                3   // reserved, not implemented

extern int           g_GameMode;               // [Game] Mode -> GAME_MODE_*
extern bool          g_bPs1EndingCredits;
extern bool          g_bPs1FmvSubtitles;      // [Game] Ps1FmvSubtitles
// Port-added: treat every FMV as skippable, including the ones the original's
// per-FMV mask table (0x004c39dc) marks 0x0000 - the endings, the staff rolls
// and DMF/DME. Off by default, so the original's masks are what ship.
extern bool          g_bSkipUnskippableFmv;    // [Game] SkipUnskippableFmv

// Every "are we in Director's Cut mode" test in the port. Deliberately a macro
// over g_GameMode rather than a second global: there is one stored value, so
// the two cannot drift, and an accidental assignment fails to compile.
#define g_bDcMode                    (g_GameMode == GAME_MODE_DC)

// g_DcDifficulty is the DC title screen's choice (PS1 g_abDcGameMode
// 0x800c8693); it is also stored in the save block, so a DC save reloads as its
// own mode.
//
// The PS1 carries the mode in three otherwise-free g_status_flags bits, and the
// DC's repurposed RDT scripts test them (190 sites test ADVANCED, 3 TRAINING)
// while the health and weapon-damage code selects on them. The port derives the
// same bits from g_DcDifficulty with dc_apply_mode_flags() so the replaced RDTs
// take the right branch. See docs/PSX_DC_TITLE_OVERLAY.md.
#define DC_DIFFICULTY_STANDARD       0           // the original game
#define DC_DIFFICULTY_TRAINING       1           // more starting health
#define DC_DIFFICULTY_ADVANCED       2           // Arrange mode (STAGE8-E)
#define DC_DIFFICULTY_ADVANCED_HOLD  3           // Same arrange mode, but with double ammo (selected by holding right)
#define MSF2_DC_ADVANCED_HOLD        0x00010000u // bit 16 - ADVANCED* (double ammo)
#define MSF2_DC_ADVANCED             0x00020000u // bit 17 - ADVANCED
#define MSF2_DC_TRAINING             0x00040000u // bit 18 - TRAINING
#define MSF2_DC_MODE_MASK            0x00070000u // all three

extern int           g_DcDifficulty;           // DC_DIFFICULTY_*

// The game's random stream, mode-aware (Globals.cpp). The PC release recorded
// its demo reels against the CRT rand; the PS1 DC uses its own LCG, so DC
// sessions must roll the PS1 sequence for demo playback to line up. Every
// game-code call site goes through the rand()/srand() entry points below. On
// Windows the names are macros; on Linux the C library entry points forward to
// the game functions so standard headers remain untouched.
extern "C" {
void re1_srand(unsigned int seed);
int  re1_rand(void);
}
#if !defined(__linux__)
#define rand(...)  re1_rand(__VA_ARGS__)
#define srand(...) re1_srand(__VA_ARGS__)
#endif

// --- Arrange stages (Director's Cut STAGE8-E) -----------------------------
// Stage ids 0-6 are the base game's seven stages (STAGE1-7). The DC adds seven
// ARRANGE stages, STAGE8-E = ids 7-13, and each is a re-dressed copy of the
// base stage seven ids below it: arrange stage S holds the same physical rooms
// as base stage S-7. Verified against the shipped RDTs - 62 of the 66 arrange
// rooms carry a room id that also exists in their base stage, and the other
// four differ only in the character-variant digit, which is not part of
// g_roomId.
//
// So every table indexed by the stage - BGM state, room effect sprites, room
// sprites, camera light index, fade-sprite params, voice offsets, enemy sound
// banks - is read with the BASE row and none of them has to grow. That matters
// for more than tidiness: g_roomBgmState lives inside g_BioCard, so widening it
// would move every field after it and change the save format.
//
// FILE PATHS keep the raw g_stageId, because the arrange stages ship their own
// RDTs and backgrounds under STAGE8-E - with one exception, which caught the
// first pass out. STAGED and STAGEE carry NO backgrounds at all: they are the
// arrange versions of the mansion-revisit stages and reuse STAGE8/STAGE9's art,
// exactly as base stages 6 and 7 reuse stage 1 and 2's. Their room ids match
// their source stage's exactly, so the existing `g_stageId > 4` revisit fold is
// still the right rule on a path - it just has to see the FOLDED row, so that
// it fires for base 5/6 and arrange 12/13 and not for arrange 7-11.
//
// In short, every one of these sites - row or path - wants get_stage_id().
// What differs is the arithmetic afterwards: the original folds a path by
// subtracting 5 from the stage CHARACTER, which only works inside the run of
// decimal digits, so an arrange stage has to recompute from the id instead.
// See load_room_bg in Room.cpp.
//
// get_stage_id() is the identity for stages 0-6, so nothing changes with
// Mode=OG.
#define STAGE_ARRANGE_FIRST  7   // first DC arrange stage id (STAGE8)
#define STAGE_COUNT_BASE     7   // STAGE1-7

// A macro rather than an inline function because g_stageId is itself a macro
// over g_BioCard, which is declared further down this header. It reads the
// field twice, which is safe - it is a plain byte with no side effects.
#define get_stage_id() STAGE_DATA_ROW_OF(g_stageId)

// The same fold applied to a stage id that is not g_stageId - the one the FILE
// paths use, which in ADVANCED can be the arrange twin of the stage the game
// thinks it is in (room_file_stage(), src/game/dc/ArrangeStages.h).
#define STAGE_DATA_ROW_OF(s) \
    ((unsigned int)(s) >= STAGE_ARRANGE_FIRST \
        ? (unsigned int)(s) - STAGE_ARRANGE_FIRST \
        : (unsigned int)(s))

// Set the DC mode bits in g_main_state_flags2 from g_DcDifficulty. Clears them
// first, so it is safe on every game start (they are not in
// MSF2_RESET_KEEP_MASK, so a stale value must not leak into a non-DC game).
// GameStart.cpp.
void dc_apply_mode_flags(void);

// Swap in the Director's Cut item tables (GameStart.cpp). No-op when
// g_bDcMode is off.
void dc_apply_item_tables(void);

// Install the Director's Cut's one zombie dispatch-table edit - the extra entry
// that lands on behaviour 11 (Zombie.cpp). No-op when g_bDcMode is off.
void dc_apply_zombie_tables(void);

extern WORD          g_message_flags;                  // 0x00bebcc0

// Message display system
extern unsigned short g_messageFlagsBackup;            // 0x00bebcc2
extern unsigned short g_PauseGameInMsgFlag;            // 0x00bf0a18
extern unsigned char* g_MessagePtr;                    // 0x00bf0a1c
extern unsigned char  g_MessageStateCounter;           // 0x00bf0a16
extern short          g_MessageScreenY;                // 0x00bf0a1a
extern unsigned char  g_MessageSpeedUpFlag;            // 0x00bf0a17
extern unsigned char* global_messages[64];             // 0x004bfc58
// The Japanese release's own copies of the text tables (src/game/JpnTextTables.cpp,
// generated by tools/gen_jpn_text.py). The readers below pick these when the
// JPN asset tree is selected - the encoding only means anything against FONT.TIM.
extern unsigned char* global_messages_jpn[64];         // 0x004cde58 (JPN)
extern unsigned char* g_ItemDescriptionsJpn[79];       // 0x004c9370 (JPN)
extern unsigned char* g_ItemDescriptions[79];          // 0x004c6160 - examine text, index = itemId - 1
extern unsigned char* g_MessageCurrentPtr;             // 0x00bf0a20 - current position in message text
extern unsigned char* g_MessageSavedPtr;               // 0x00bf0a24 - saved ptr for item name returns
extern unsigned char  g_MessageCharDelay;              // 0x00bf0a29 - base delay between characters
extern unsigned char  g_MessageCharTimer;              // 0x00bf0a2a - current timer countdown
extern unsigned char  g_MessageClutBase;               // 0x00bf0a2b - CLUT color base
extern unsigned char  g_MessageClutCopy;               // 0x00bf0a2c - copy of CLUT base
extern int            g_MessageLineCounter;            // 0x008e1c64 - line counter for newlines

// Fading
extern short         g_fading_counter;                 // 0x00bebcca
extern unsigned char g_fade_type_id;                   // 0x00bf0a2f
// g_fading_state is a macro to g_BioCard.fadingState (see BioCard.h)
extern BYTE          g_bGameActive;                    // 0x00be41dc

// Sprite animation/screen tint state (0x00be41d0-0x00be41d4)
extern int           g_spriteAnimActive;               // 0x00be41d0
extern int           g_spriteAnimR;                    // 0x00be41d1
extern int           g_spriteAnimG;                    // 0x00be41d2
extern int           g_spriteAnimB;                    // 0x00be41d3
extern short         g_spriteAnimIntensity;            // 0x00be41d4

// game_loop state machine globals (0x00480b30)
// 0x00be9615 - raised by door_try_enter when a key item is consumed; read and
// cleared by check_event_item_usage (0x0041c490), which is not ported yet.
extern unsigned char g_eventItemUsedFlag;              // 0x00be9615
// 0x00bebcbc - the door record door_try_enter hands to the room-change code. No
// port code reads it yet, so the room load after the blackout is still missing.
extern int           g_pendingDoorRecord;              // 0x00bebcbc

// Door-record fields latched by room_transition_load (0x004813c0). Named for the
// record offset they come from where the use is clear, address-suffixed where it is
// not. These are new globals in the port - the addresses are the original's but the
// symbols are NOT .gwipe-placed there yet.
extern unsigned char g_nextRoomDoorType;                // 0x00be0bc8  record+0x08
extern unsigned char g_nextRoomSfxId;                   // 0x00be0bc1  record+0x09
extern unsigned char g_nextRoom_be05b7;                 // 0x00be05b7  record+0x0A
extern unsigned char g_nextRoomCameraId;                // 0x00be0dd4  record[0x0B]&0x3F
extern unsigned char g_nextRoomDest;                    // 0x00be0bc0  record+0x0D
extern int           g_roomTransitionBusy;              // 0x004d2290
extern int           g_openMenuFlag;                   // 0x00d22760 - menu state machine (0=none,1=open,2=init,3=close)
extern unsigned short g_short_message_flags;           // 0x00bebcc2 - backup of g_message_flags when menu opens
extern int           g_int_008f8898;                   // 0x008f8898 - saved light state for room transitions
extern int           DAT_004d2294;                     // 0x004d2294 - countdown frame counter (0-29)
extern int           DAT_004d2288;                     // 0x004d2288 - death delay countdown
// Two render feature gates in .data at 0x004d46a4 / 0x004d46a8. Both are
// statically initialized to 1 in the original and are NEVER WRITTEN anywhere in
// the binary — 0x004d46a4 has exactly one xref and 0x004d46a8 exactly two, all
// reads, all inside game_loop. They are build-time "this subsystem is compiled
// in" constants, not runtime state.
//
// Do not initialize these to 0 and do not alias them onto port-side variables.
// g_dwEntityRenderEnabled gates BOTH render_entity calls in game_loop —
// the enemy loop and the player — so a zero here silently removes every
// character model from the screen while the room still renders normally. It was
// previously read through the port's own g_SpriteQueueCount (which happens to
// live at the same address in the Ghidra labels but is a per-frame sprite
// counter reset to 0 by SpriteRenderer), which is exactly that failure.
extern int           g_dwRoomObjectRenderEnabled;       // 0x004d46a4 - build-time gate on render_room_objects (never written)
extern int           g_dwEntityRenderEnabled;           // 0x004d46a8 - build-time gate on render_entity (never written)
extern int           g_displayDebugSaveMenu;           // 0x004d4680 - debug save menu trigger
extern int           g_debugSaveMenuFlag;              // 0x004d4684 - debug save menu state flag
extern BYTE          g_VideoModeOverlayActive;         // 0x004d461c - video-mode overlay flag
extern int           g_ShowVideoModeOverlay;           // 0x004d4600 - video-mode overlay toggle latch
extern int           g_VideoModeOverlayTimer;          // 0x004d475c - overlay frame countdown
extern int           DAT_004d228c;                     // 0x004d228c - menu processing active flag

// Master switch for every port-added debug feature (F1 debug menu, F6/F7/F8
// tools, quick access, collision overlay). Default 0 in release builds, 1 in
// debug builds; [Debug] EnableDebug=1 in config.ini overrides it either way
// (read by LoadIniConfiguration, main.cpp). The code is compiled into both
// configurations - only this runtime flag gates it.
extern int           g_debugFeaturesEnabled;

// Port-added debug helpers (no original address; set by F6 in WindowProc
// and by the F1 debug menu's QUICK ACCESS screen, consumed by the game loop).
// F6 = texture viewer overlay,
// g_debugOpenLoadScreenFlag = load screen (title flow).
extern int           g_debugOpenLoadScreenFlag;
extern int           g_debugOpenItemboxFlag;
extern int           g_debugTextureViewerFlag;   // F6: request the overlay (edge)
extern int           g_debugTextureViewerOpen;   // 1 while the overlay is active
int texture_viewer_overlay(void);                 // DebugScreens.cpp - per-frame overlay; 0 when closed
extern int           g_debugMenuOpen;            // 1 while the F1 debug menu overlay is open
int debug_menu_overlay(void);                     // DebugMenu.cpp - F1 overlay; 1 while open
void DebugRoomChange_ApplyPendingPlacement(void); // DebugMenu.cpp - post room_transition_load placement
extern int           g_debugLoadSlot;             // quick access load: selected slot index (0-7)
void DebugQuick_SaveSlot(int slot);               // DebugSaveLoad.cpp - write a full save to savedat<slot+1>.dat
int  DebugQuick_LoadSlot(int slot);               // DebugSaveLoad.cpp - restore savedat<slot+1>.dat and arm the continue path; 0 = refused

// Room interaction state (0x00be9616-0x00be9618, adjacent to g_eventItemUsedFlag)
extern unsigned char g_typewriter_state;               // 0x00be9616 - typewriter save-flow state machine
extern unsigned char g_itembox_state;                  // 0x00be9617 - itembox lid animation state (0-4)
extern unsigned char g_desk_check_state;               // 0x00be9618 - desk open/lock state machine (0-5, 35=camera)

// Itembox/desk animation scratch in .data (0x004d6eac-0x004d6ebc)
extern unsigned short g_short_itembox_open_timer;      // 0x004d6eac - lid travel accumulator
extern void*         g_itembox_cover_pointer;          // 0x004d6eb0 - omodel of the itembox lid being animated
extern unsigned int  g_typewriter_id;                  // 0x004d6eb8 - save-slot id the typewriter targets
extern short         g_counter_increase;               // 0x004d6ebc - lid travel step (reversed on overflow)

// Inventory bookkeeping
// 0x00d21cd0 - per-slot sheet row: where each inventory slot's sprite sits in
// the item composite (row = slot at rebuild; maintained through
// rearrange/pickup/swap). The equipped-box draw reads this at
// [g_EquippedItemId - 1] via the aliased address 0x00d21ccf (the byte just
// before the array - NOT a real global).
extern unsigned char g_ItemSlotIndices[8];             // 0x00d21cd0

// Climb-object scratch (0x00ae9ef0, next to g_omodelCount)
extern unsigned int  DAT_00ae9ef0;                     // 0x00ae9ef0 - omodel check_climb_object found in reach
// Push-object scratch. A DIFFERENT global from the climb one above: this is the
// omodel update_room_objects is currently pushing, and it is what
// behavior_10_push reads to pick the grunt SFX.
extern unsigned int  DAT_00ae9ee8;                     // 0x00ae9ee8 - omodel being pushed
// Dead constant read by room_check_actions[0x0B] (0x00d226e8, past g_omodel_table)
extern unsigned int  DAT_00d226e8;                     // 0x00d226e8 - always 0, never written
// Screen-distortion effect struct (0x00be63c8) - FUN_004567d0 writes it; the
// consumer (0x00456a10 camera scroll) is not ported yet, so it is inert.
extern unsigned char DAT_00be63c8[0x80];               // 0x00be63c8

// Game session state
extern int           DAT_00d91bc8;                     // 0x00d91bc8
extern DWORD         g_gameSessionInitFlag;            // 0x00d213b0
extern int           g_playingGameFlag;                // 0x004d4674
extern int           g_loadSaveStateFlag;              // 0x004d4678
extern int           end_game_status;                  // 0x008f8894
extern int           g_SelectedPlayerID;               // 0x008f879c

// Dialog / reset key handling (F9 / F1, used by OnKeyDown and main_loop)
extern BOOL          g_displayReturnToTitleScreen_Flag;// 0x004d466c
extern BOOL          g_displayExitGameScreen_flag;     // 0x004d4668
extern DWORD         g_lastF9PressTime;                // 0x004d46e0 - timeGetTime() of last F9 press
extern int           g_blockF9Flag;                    // 0x004b3870 - blocks F9 processing when set
extern int           g_F1DebugMode;                    // 0x004d4654 - cycles 0-3 on F1 press
extern int           g_pressF9Flag;                    // 0x004ba718 - set when F9 triggers game reset
extern int           g_resetGameFlag;                  // 0x004d4670 - triggers game state reset

// ============================================================================
// SECTION 8: Player, entities & effects
// ----------------------------------------------------------------------------
// Most of these live in the .gwipe ordered section: their original addresses
// are inside the game-init wipe range 0x00be41e0..0x00be9620 that
// InitializeGame's memclr clears. See docs/MEMORY_LAYOUT.md before touching.
// ============================================================================

extern PlayerEntity  g_playerEntity;                   // 0x00be62e4 - main player entity (0x180 bytes) [.gwipe]
extern Entity        g_EnemiesList[30];                // 0x00be6464 - enemy entity array (30 x 0x18C bytes) [.gwipe]
extern int           g_enemy_count;                    // 0x00be41e2 - number of active enemies [.gwipe]
extern Entity*       ENTITY;                           // 0x00bebcd4 - current entity pointer
// 0x00bebcd8 - cursor into g_savedEnemyStates, left pointing at the matched slot
// by restore_saved_enemy_state.
extern SavedEnemyState* g_pSavedEnemyState;
extern SavedEnemyState  g_savedEnemyStates[16];         // 0x00be92cc [.gwipe$92cc]
extern unsigned char g_PlayerMaxHealth;                // 0x00be6459

// Player entity pointer alias (used by decompiler-generated names)
#define g_playerEntityPointer  g_playerEntity

// Entity matrices / SCA collision data
extern MATRIX        g_RoomCameraData;                 // 0x004bca88 - Room camera matrix
extern DWORD         g_RoomCameraDataCopy;             // 0x00d1fdd4
extern DWORD         g_deadMoveValue;                  // 0x00d1fdd0
extern int           DAT_004bd2b0;                     // 0x004bd2b0 - hunter grab / death-screen one-shot
extern DWORD         g_lightMatrixPtr;                 // 0x00d1fdcc
extern MATRIX        g_identityMatrixData;             // 0x004bca68
extern MATRIX        g_lightMatrix;                    // 0x004bcaa8
extern DWORD         g_scaDataTable[4];                // 0x004d4540
extern DWORD         g_scaChrisData[4];                // 0x004d4510
extern DWORD         g_scaData2[4];                    // 0x004d4520
extern DWORD         g_scaJillData[4];                 // 0x004d4530
extern BYTE          g_entityDataBlock[0x200];         // 0x00d211d0
extern DWORD         g_scaPoolPtr;                     // 0x00d21354
extern DWORD         g_scaPoolBase;                    // 0x00d21358

// Effect system (billboard/sprite effect pool)
extern Effect        g_effectPool[MAX_EFFECTS];        // 0x00be41e4 - 64 slots x 0x84 bytes [.gwipe]
extern unsigned char g_freeEffectSlots;                // 0x00bf07ee - free slot counter (starts at 64)
extern DWORD         g_effectSpriteInfo[50];           // 0x00bf0a54 - per-type sprite header pointers
extern unsigned char g_activeEffectIndex;              // 0x00bf0a2e - slot being processed by update_2d_effects
extern int           g_MaxHealthDisplayFlag;           // 0x00d227c0 - nearest-effect depth (shared with health bar)

// Effect sprite texture management state
extern unsigned char  DAT_00bf0a38;                    // 0x00bf0a38 - effect tex Y position
extern short          DAT_00bf0a3c;                    // 0x00bf0a3c - effect tex X offset
extern unsigned short DAT_00bf0a3e;                    // 0x00bf0a3e - effect tex U offset
extern short          DAT_00bf0a40;                    // 0x00bf0a40 - effect tex page row
extern unsigned short DAT_00bf0a42;                    // 0x00bf0a42 - effect tex page col
extern unsigned char  g_abEffSpriteIndexTable[16];     // 0x00bf0a44 - effect sprite index table (first 8 = shoot dir, second 8 = room eff)

// Effect sprite per-slot image data pointers (computed from RDT by InitRoomEffSprite)
extern int           DAT_00ac9cd0[8];                  // 0x00ac9cd0 - effect sprite image pointers
// Per-sprite texture sheet slot (0-7 weapon FX, 8-15 room), recorded by
// setup_effect_sprite_textures; the effect renderer resolves its D3D11 SRV
// through this instead of the depth-derived texture id, which several sheets
// share. Port-only (the original keeps all sheets in one VRAM page).
extern unsigned char g_effectSpriteSheetSlot[50];
// Per-sprite V offset within its page, also recorded by
// setup_effect_sprite_textures. The blend/colour band is chosen by the sprite's
// PAGE-ABSOLUTE V (effect_submit_sprite scans g_EffectBlendTable for the first
// row whose startV+len exceeds it), but the weapon-FX block samples from
// per-sprite SRVs whose UVs are sprite-local, so their absolute V has to be
// carried separately or every one of them picks band 0. Zero for room sprites -
// they share a page, so the offset is already baked into their UV records.
extern unsigned char g_effectSpriteBandV[50];
// Room sprites only: the page V offset at which setup_effect_sprite_textures
// placed the sprite's RDT-embedded TIM. load_effect_sprites uses it to blit the
// per-room art back into the page at the same place the page-absolute UVs point.
// Distinct from g_effectSpriteBandV (which stays 0 for room sprites - their UV
// records are edited in place) so the blend band scan is unaffected.
extern unsigned char g_effectSpritePageV[50];
// Per-sprite CLUT row count of the sheet TIM that carries its art (1 for a
// single-row sheet). The rows are palette VARIANTS selected per spawn by the
// tint index - the same mechanism the weapon-FX sheets use. Filled by
// load_effect_sprites from the RDT-embedded room TIMs (and by
// load_shoot_direction_data for the core00 sheets). Port-only companion table.
extern unsigned char g_effectSpriteClutRows[50];

// Entity joint animation copy base (set by SetupEntityJointAnimation)
extern int           DAT_00be0e00;                     // 0x00be0e00

// Effect sprite stage/room cache (set by load_effect_sprites)
extern unsigned int  STAGE_ID_00ac9cf0;                // 0x00ac9cf0
extern unsigned int  ROOM_ID_00ac9cf4;                 // 0x00ac9cf4

// Bullet effect parent sprite info pointer (set by cmd_bullet_effect_spawn)
extern int           DAT_00bf0a34;                     // 0x00bf0a34

// Special room lighting globals (accessed by cmd_room_light_fade_set and main_loop)
extern int           g_SpecialR1;                      // 0x00be961d [.gwipe]
extern int           g_SpecialG1;                      // 0x00be961e [.gwipe]
extern int           g_SpecialB1;                      // 0x00be961f [.gwipe]

// ============================================================================
// SECTION 9: Bio card & items
// ----------------------------------------------------------------------------
// The bio card block (0x00be9620..0x00be9a3c, 0x41C bytes) is one packed
// struct; individual original globals are macros in game/BioCard.h.
// ============================================================================

extern BioCardLayout g_BioCard;                        // 0x00be9620 [.gwipe end marker]

extern const unsigned char g_StageRoomFlagOffset[6];   // 0x004d31e0 - per-stage room flag base offsets
extern DWORD         g_ItemSlotsBitmask;               // 0x00d22734
extern unsigned char g_defaultItemSlot;                // 0x00be41e0 [.gwipe start marker]
extern unsigned char DAT_00be41e1;                     // 0x00be41e1 [.gwipe]
extern unsigned char DAT_00be9614;                     // 0x00be9614 - death/timeout state machine byte [.gwipe]
// Not const: dc_apply_item_tables() replaces these with the Director's Cut
// tables when g_bDcMode is on (see ItemTables.cpp).
extern unsigned char g_ItemImageLookupTable[459];      // 0x004bd81d

// Active character item slots pointer (points to g_ItemsSlots or g_RebeccaItemSlots)
extern void*         g_ItemSlotsPointer;               // 0x00d22768
// 0x00d226f0 - set by get_item_slot to the matched 2-byte inventory slot, or to
// &g_defaultItemSlot when the item is not held.
extern unsigned char* g_pCurrentItemSlot;

// ============================================================================
// SECTION 10: Rooms (RDT, SCD scripts, backgrounds, PAK/LZW, lab slides)
// ============================================================================

extern RDT*          g_RdtPointer;                     // 0x00bebcd0
extern void*         g_RdtLoadDataBackup;
extern void*         g_loadDataDestPointer;            // 0x00bebcdc
extern char          FILE_PATH[260];

// Room state (reset by room_state_reset / room_set)
extern DWORD         g_SysFlags[2];                    // 0x00be41c8 - SCD flag bank 4 (system flags, case 4 in cmd_bit_test)
// DAT_00be9830 (g_fwdPosActionId) is now a macro to g_BioCard.fwdPosActionId (see BioCard.h)

// Room action table (AOT list): 24 entries x 12 bytes. One entry per trigger
// zone in the room - doors, pick-ups, message zones, stairs, the itembox,
// effect zones and cutscene triggers all live here. Entry byte 0 is the
// room_check_actions handler index, byte 1 the probe flags, +8 the pointer to
// the originating SCD record (the zone geometry).
extern unsigned char g_RoomActionTable[288];        // 0x00d91aa0
extern void*         g_RoomActionTail;              // 0x00d91bc0

// Pointer to the room action entry the last fired handler latched (a
// g_RoomActionTable slot, NOT an index and nothing to do with the SCD event
// scripts in g_ScdEventTable). The interaction that follows the handler -
// the item-menu pickup, the desk unlock, the typewriter - reads the entry's
// +2/+4 fields and follows +8 to the SCD record for the item id and quantity.
extern void*         g_pRoomActionEntry;               // 0x00d226a4

// Room model record tables (populated by room_set from the RDT VB region)
extern void*         g_omodel_table[8];      // 0x00d226b0 - room-object (omodel) records
extern void*         g_item_model_table[8]; // 0x00d21360 - item model records (room pick-up 3D models)

// Enemy model loading state (used by room_set and cmd_omodel_set)
extern int           g_omodelCount;                    // 0x00ae9ef4 - object model count (cmd_omodel_set)
extern unsigned char g_LastEnemyModelId;               // 0x00bebcc9 - last enemy model ID (model reuse cache)
extern int           g_ItemModelCount;                 // 0x00ae9eec - item/obstacle model count (cmd_item_model_set)

// SCD script pointers
extern void*         g_RoomInitScd;                    // 0x00d213bc - room initialization SCD (from RDT)
extern void*         g_CurrentRdtDataTypePtr;          // 0x00bebccc - current RDT data type ptr (cam_switch_zones)
extern void*         DAT_00d213c0;                     // 0x00d213c0 - data ptr saved for stage 2 room 3

// Room sprite entries table (populated from RDT by FUN_004757c0)
// Each entry is 0x24 bytes: TextureDesc (0x20) + active flag + id + posData
// Entry count is g_RdtPointer->sprites_count
extern RoomSprEntry  g_RoomSprEntries[128];            // 0x00d213d0

// SCD event execution context table (8 entries x 0x34 bytes each)
// Each entry tracks a running room event script executed by room_events_check.
extern ScdEventEntry g_ScdEventTable[8];               // 0x00bf084c

// SCD event system globals
extern ScdEventEntry* g_pScdEventCurrent;              // 0x00bf0848 - current event being processed
extern unsigned char* g_ScdOpcodes;                    // 0x00bf0800 - current SCD opcode pointer
extern unsigned int*  g_CmdOpcodesPointer;             // 0x00bf0804 - SCD call stack pointer
extern unsigned char  g_ScriptContinueFlag;            // 0x00bf07fa - SCD call depth counter
extern unsigned char* g_RoomEventScripts;                    // 0x00d213b4 - event script table pointer
extern unsigned char* g_RoomScdOpcodes;                // 0x00d213b8 - room SCD opcodes pointer
extern void*          script_command_funcs_table[256]; // 0x004c1110 - SCD command dispatch table

// Room action dispatch table for SCD opcodes 0x24 / 0x2D.
// 18 handlers (0x00-0x11) + 2 trailing NULL slots in the original.
#define ROOM_CHECK_ACTION_COUNT 20
extern void*          room_check_actions[ROOM_CHECK_ACTION_COUNT]; // 0x004b9340

// Animation remap pairs for SCD event state-1 opcode 0x89. See CmdFunctions.cpp.
extern const unsigned char g_ScdAnimRemap[32];          // 0x004bec80

// Per-frame item-use flag bank (SCD flag bank 9), 64 bits. Cleared every
// frame by game_loop and re-armed by room logic in the same frame:
//  - bits 0x00..0x3E indexed as (itemId - 0x1B): "item X is usable right now"
//    (chemicals / special items / keys / desk key use categories)
//  - bit 0x3F: the radio (item 0x4D) has an active transmission
// Consumed by the inventory "use" command (menu_item_use_if_flag,
// menu_item_use_red_book), the item-menu radio tab (menu_tab_radio), and
// cmd_bit_test with flag bank 9. See room action flag_bank_set (bank 9) and
// cmd_bit_op (bank 9) for how room scripts arm the bits.
extern unsigned int  g_itemUseFlags[2];               // 0x00d213a0

// Mirror (planar reflection) parameters, all written only by SCD opcode 0x0F
// (cmd_mirror_set). The plane axis is g_main_state_flags bit 1; bit 0 arms
// the whole subsystem. See entity_draw_mirror_reflection in EntityCommon.cpp.
// (Ghidra names these g_wMirrorExtentMin / g_wMirrorExtentMax / g_wMirrorPlaneCoord,
// its global naming policy demands a Hungarian prefix this codebase does not use.)
extern unsigned short g_mirrorExtentMin;               // 0x00d211c4 - near edge, cross axis
extern unsigned short g_mirrorExtentMax;               // 0x00d21350 - far edge, cross axis
extern unsigned short g_mirrorPlaneCoord;              // 0x00d2276c - the mirror plane itself
extern unsigned int   DAT_00d22770;                    // 0x00d22770

// Screen effect parameter storage
extern SndPanVol     g_SndPanVol[3];                   // 0x00ac98e0 (bound 0x00ac98f8)
extern int           DAT_00ac98f8;                     // 0x00ac98f8 - zeroed by BuildSndFadeTbl

// Misc globals used by SCD command functions
extern BOOL          g_bFullScreenFlag_68;
extern int           g_FmvCharacterId;

// --- Room backgrounds ---

// 0x00aea0d0 - Per-camera background load buffer (one PAK file worth)
extern BYTE          g_bgPakLoadBuffer[131072];

// 0x00b0a0d0 - Cached all-camera background buffer
extern BYTE          g_bgCacheBuffer[786440];

// Background loading mode flag (0 = per-camera load/display, non-zero = cache all cameras)
extern int           g_bgCacheMode;                    // 0x004d46b4

// Hex character lookup table for path construction
extern char          g_hexCharTable[17];               // 0x004c2060 "0123456789abcdef"

// Path template for room background PAK files (mutated at runtime)
extern char          g_bgPathTemplate[40];             // 0x004c2078 <GAME_DATA_ROOT>stageS\rcSRRC.pak

// Camera hex char (stored after path template, set by load_room_bg)
extern char          DAT_004c2090;                     // 0x004c2090

// Per-camera offset into g_bgCacheBuffer (index 0 unused, 1..N = cameras)
// 0x00aea08c. load_room_bg writes slot i+1 for image i; load_room_bg_image reads
// it from 0x00aea090 (== &g_bgCameraOffsets[1]), so index with camera id + 1.
// 17 entries, not 16: the writer reaches slot cameras_count.
extern int           g_bgCameraOffsets[17];            // 0x00aea08c - slot 0 unused

// Per-camera mask offset into g_bgMaskDataBuffer
extern int           g_bgMaskOffsets[16];              // 0x00ae9e80

// Mask data buffer (PAK mask data loaded here)
extern BYTE          g_bgMaskDataBuffer[0x20000];      // 0x00ac9e80

// Path template for mask PAK files (mutated at runtime)
extern char          g_maskPathTemplate[40];           // 0x004c3bc8 <GAME_DATA_ROOT>objspr\osp0SRRC.pak

// --- PAK LZW decompression state (unpack_pakfile_) ---
extern unsigned int  g_pakDecompInputPos;              // 0x00d2b0a4
extern unsigned int  g_pakDecompBitMask;               // 0x00d91a64
extern unsigned int  g_pakDecompCurByte;               // 0x00d227c4
extern unsigned int  g_pakDecompCodeSize;              // 0x00d2b0a8
extern unsigned int  g_pakDecompNextCode;              // 0x00d227c8
extern unsigned int  g_pakDecompMaxCode;               // 0x00d2b0a0

// LZW dictionary — ONE array of 12-byte records based at 0x00d2b0b0.
// This used to be modeled as two separate arrays (g_pakDictPrefix at 0x00d2b0b4
// and g_pakDictChar at 0x00d2b0b8, strides 4 and 1). Those are the +4 and +8
// FIELDS of a single 12-byte record, not independent arrays, so every walk of
// the dictionary read the wrong addresses and ran off the end.
struct PakDictEntry {
    int  unused;    // +0  0x00d2b0b0 - set to -1 by pak_decomp_reset, never read
    int  prefix;    // +4  0x00d2b0b4 - previous code in the LZW chain
    char ch;        // +8  0x00d2b0b8 - character emitted by this code
    char pad[3];    // +9
};
static_assert(sizeof(PakDictEntry) == 12, "PakDictEntry size mismatch");

// pak_decomp_reset walks entries from 0x00d2b0b0 while the pointer is below
// g_pakDecompBitMask (0x00d91a64), which is 34981 records. Only codes up to
// 0x1FFF are reachable, so the tail is never used — but the count is what the
// original clears, so it is reproduced here.
#define PAK_DICT_ENTRIES 34981
extern PakDictEntry  g_pakDict[PAK_DICT_ENTRIES];      // 0x00d2b0b0

// LZW string output buffer (for building decoded strings)
extern char          g_pakStringBuf[512];              // 0x00d227d0

// --- Lab slides state (reset by lab_slides_reset) ---
extern unsigned char g_labSlidesFuncIndex;             // 0x00d22790
extern unsigned char g_labSlidesAnimState;             // 0x00d22791 - animation phase (0-3)
extern unsigned char g_passcodePanelAnimationState;    // 0x00d22792 - passcode panel animation phase
extern int           g_labSlidesScrollX;               // 0x00d22794 - slide scroll X position
extern unsigned int  g_labSlidesScrollY;               // 0x00d22798
extern unsigned char g_labSlidesSlideIndex;            // 0x00d227a0 - current slide frame
extern unsigned char g_labSlidesLoopDone;              // 0x00d227a1 - loop completion flag
extern unsigned char g_labSlidesMsgId;                 // 0x00d227a2 - current slide message id
extern unsigned char g_labSlidesCountdown;             // 0x00d227a3 - delay countdown timer
extern int           g_labSlidesState;                 // 0x007d9120

// The passcode panel reuses the lab-slide state block at runtime. These fields
// occupy the same original storage as the slide fields above, but are kept as
// separate port globals so the two state machines remain readable.
extern unsigned char  g_interactiveScreenSavedCameraId; // 0x00d2278a
extern unsigned char  g_passcodePanelActive;            // 0x007d9124
extern unsigned char  g_passcodePanelSprites[9];        // 0x007d9128
extern unsigned short g_passcodePanelTimer;             // 0x00d227a0 (overlaid)
extern unsigned short g_passcodePanelPatternIndex;      // 0x00d227a2 (overlaid)

// The lab computer terminal (ComputerLab.cpp) is the third tenant of the same
// block. It needs the four slots the slide/passcode views never named:
//   +0x03 a fourth sub-state byte. check_and_display_interactive_screen clears
//         bytes 0..3 on entry, so this MUST be cleared alongside the other
//         three or the terminal resumes mid-sequence on a second visit.
//   +0x0c the player's saved Y (the model is parked below the floor while the
//         first-person terminal is up).
//   +0x10/+0x12 the two shorts every terminal step uses as timers.
extern unsigned char  g_labSlidesSubState2;             // 0x00d22793
extern int            g_labSlidesSavedPlayerY;          // 0x00d2279c
extern short          g_labSlidesTimerA;                // 0x00d227a0 (overlaid)
extern short          g_labSlidesTimerB;                // 0x00d227a2 (overlaid)

// Character switch backup globals (used by room_set when switching between Jill/Chris and Rebecca)
extern short          HEALTH_BKP;                      // 0x008f87b0 - backup of player health
extern unsigned short HEALTH_STATUS_BKP;               // 0x008f87b4 - backup of health status flags

// ============================================================================
// SECTION 11: In-game menu
// ============================================================================

// Menu frame/rect data block (original binary 0x004c26d0..0x004c2998, 712 bytes)
// Defined in MenuData.cpp as one contiguous block. The code reads entries
// BACKWARDS from various label pointers that sit WITHIN this block
extern const unsigned char g_MenuFrameDataBlock[712];  // 0x004c26d0

// Label pointers into the menu frame data block (byte offsets from 0x004c26d0)
#define g_MainMenuFramesPos     (g_MenuFrameDataBlock + 0)     // 0x004c26d0 bottom-frame end marker
#define g_MainMenuFrames2Pos    (g_MenuFrameDataBlock + 264)   // 0x004c27d8 bottom frame decoration
#define g_MainMenuTopOptionsPos (g_MenuFrameDataBlock + 312)   // 0x004c2808 top options buttons frame parts
#define g_MainMenuFrames3Pos    (g_MenuFrameDataBlock + 368)   // 0x004c2840 frame border segments
// 0x004c28c0 = base 0x004c26d0 + 0x1f0 = +496, NOT +506 (verified: the original
// at 0x0046440e loads the literal 0x004c28c0). This is the 8-slot inventory
// border - the one Jill uses, selected by DAT_00ae9f19 bit 1 - and it is exactly
// 126 bytes (9 entries x 14) ending where g_MainMenuFrames4Pos begins, which the
// +0x7e / 9-iteration backward walk in menu_draw_inventory depends on. Ten bytes
// too high fed that walk garbage geometry, so Jill's inventory panel drew with no
// border at all while Chris's (g_MainMenuFrames3Pos, correct) looked right.
#define g_MainMenuFrames4Pos    (g_MenuFrameDataBlock + 496)   // 0x004c28c0 alternate border
// 0x004c2940 = base 0x004c26d0 + 0x270 = +624, NOT +634. The mask loop at
// 0x00464510 (ESI = 0x004c2940, verified in the original) walks the four black
// masking rects backwards from g_inventorySlotsPos and stops when the pointer
// reaches this marker; ten bytes too high ended it one rect early, dropping the
// top strip (0,0,320,12).
#define DAT_004c2940            (g_MenuFrameDataBlock + 624)   // 0x004c2940 rect outline end marker
#define g_inventorySlotsPos     (g_MenuFrameDataBlock + 656)   // 0x004c2960 inventory slot positions

// EKG line drawing data (primary line at 0x00be1198, secondary at 0x00be1184)
// Primary EKG line primitive (16 bytes): used by menu_draw_health_bar
extern unsigned char g_EkgPrimaryLine[16];             // 0x00be1198
// Secondary EKG line primitive (24 bytes): used by menu_draw_health_bar
// Extended with gradient color endpoints at offsets 15-17
extern unsigned char g_EkgSecondaryLine[24];           // 0x00be1184

// Menu item data tables (defined in MenuData.cpp, extracted from the original
// binary .rdata section). The item name strings (originally one flat 984-byte
// block at 0x004becc8) are STR()-encoded per name in MenuData.cpp; the pointer
// tables below reference them.
extern const unsigned char* g_ItemNamePointers[128];   // 0x004bf0a0 (indexed by itemId-1)
extern const unsigned char* g_UnknownItemNamePointers[16]; // 0x004bf260
// Japanese counterparts (src/game/JpnTextTables.cpp), same shape and the same
// [112..127] overlap between the two tables that the originals have.
extern const unsigned char* g_ItemNamePointersJpn[128];    // 0x004cd388 (JPN)
extern const unsigned char* g_UnknownItemNamePointersJpn[16]; // 0x004cd548 (JPN)
// Records in g_ItemModelFileNames, i.e. the highest usable item image type + 1.
// Byte 0 of an item's g_ItemImageLookupTable record indexes this table, and the
// DC's lookup reaches 0x4B (see ItemModels.h), so a DC image type at or past
// this count must be overridden and never read straight from the table.
#define ITEM_MODEL_NAME_COUNT 75
extern const unsigned char g_ItemModelFileNames[ITEM_MODEL_NAME_COUNT][8]; // 0x004bd348 (75 x 8-byte names)
extern const unsigned char g_ItemModelFileNameING[8];  // 0x004bd5a0
extern const unsigned char g_ItemModelFileNameMINI[8]; // 0x004bd5a8
// Sized for the Director's Cut (38 entries / 454 bytes); the USA data fills the
// first 35 / 440 and the spare tail stays zero. See dc_apply_item_tables().
extern unsigned char* g_ItemCombinePtrs[38];           // 0x004bd768
extern unsigned char g_ItemCombineData[454];           // 0x004bd5b0
// Not const: dc_apply_item_tables() shifts the DC lookup into this when g_bDcMode
// is on (the two tables overlap by one byte, as on the PS1).
extern unsigned char g_ItemMaxQty[448];                // 0x004bd81c
// Combine index -> the 1-based ITEM_MIX.PIX row holding that item's sprite, read
// by menu_item_combine_refresh. Not const and sized for the DC (38 combine
// entries, 0x8008E164): dc_apply_item_tables() copies the DC's table in, so a
// combine into the assembled MOON CREST reloads the crest icon rather than
// leaving the pre-combine half in place. The USA data fills the first 35.
extern unsigned char g_ItemImageTypeTable[38];         // 0x004bd7f8
extern const unsigned char g_ItemModelExtIVM[8];       // 0x004c29a0 ".ivm"
extern const unsigned char g_ItemModelDir[24];         // 0x004c29a8 "./usa/item_m2/"
extern const unsigned char g_ItemMixPixPath[32];       // 0x004b10d4
extern const unsigned char g_MedalPixPath[32];         // 0x004bf330
extern const unsigned char g_ItemHealTable[0x61];      // 0x004bd927 (indexed by itemId; +0x51 = examine messages)
extern const unsigned char g_ItemExamineCombos[48];    // 0x004bd988
extern const unsigned char g_ItemExamineTypes[48];     // 0x004bd9b8

// ============================================================================
// SECTION 12: Title screen & attract demo
// ============================================================================

// Title screen display image SRV
extern MarniHandle   g_displayImageSRV;

// Title state globals
extern unsigned char g_titleLoopFlag;                  // 0x00d22777
extern unsigned char g_titleMode;                      // 0x00d22775
extern unsigned char g_titleOptionsFading;             // 0x00d22776
extern unsigned char g_titleSelectionId;               // 0x00d22774
extern short         g_titleDemoTime;                  // 0x00d22788
// Port-added (Director's Cut): frames the confirm button has been held on the
// ADVANCED option - the PS1 overlay's g_titleHoldTimer (0x800e16ca), which the
// PC build has no equivalent of because it has no submenu.
extern unsigned char g_titleHoldTimer;
extern short         g_titleTexturePageData[8];       // 0x00d22778 - per-selection tpage
extern int           g_sceneRenderParam;               // 0x004d6300
extern DWORD         g_titlePrimType;                  // 0x004d6398
extern DWORD         g_primParam;                      // 0x004d63e0
extern DWORD         g_primFlag2;                      // 0x004d63e4
extern int           g_titleTextureSlotId;             // 0x004c331c

// Attract demo timers
extern DWORD         g_AttractModeIdleTimer;           // 0x004c44d8
#define g_demoIdleTimer1 g_AttractModeIdleTimer
extern int           g_demoTimer;                      // 0x004bcb5c

// 0x00be41d6 - index of the pdemoN.dat attract demo currently being played
// (cycles 0..3; wiped by ClearGameStateFlags' dword run in the original)
extern WORD          g_CurrentAttractModeId;
// 0x00d21ce0 - image of the pdemoN.dat header (first 0x30 bytes of the demo
// file, staged by LoadAttractModePlayerData). g_DemoTimerCur/g_DemoTimerMax
// live INSIDE this block in the original (0x00d21cee/0x00d21cf0): the file
// load overwrites them, which is where the demo frame length comes from.
#pragma pack(push, 1)
struct AttractDemoData {
    BYTE roomId;            // +0x00 -> g_roomId
    BYTE stageId;           // +0x01 -> g_stageId
    BYTE totalHeldItems;    // +0x02 -> g_TotalHeldItems
    BYTE equippedItemId;    // +0x03 -> g_EquippedItemId
    BYTE characterId;       // +0x04 -> player character id
    BYTE pad05;
    short playerPosX;       // +0x06
    short playerPosZ;       // +0x08
    short playerDirAngle;   // +0x0A
    short pad0C;
    short demoTimerCur;     // +0x0E - playback frame counter (reset to 1 after load)
    short demoTimerMax;     // +0x10 - demo length in frames
    short pad12;
    BYTE cameraId;          // +0x14 -> g_AttractMode_RoomCameraId
    BYTE pad15[3];
    BYTE itemsSlots[24];    // +0x18 - 12 item slots (id/qty pairs), file
                            //        bytes 0x18..0x2F
};
static_assert(sizeof(AttractDemoData) == 0x30, "AttractDemoData size mismatch");
#pragma pack(pop)
extern AttractDemoData g_AttractDemoData;
#define g_DemoTimerCur   (g_AttractDemoData.demoTimerCur)   // 0x00d21cee
#define g_DemoTimerMax   (g_AttractDemoData.demoTimerMax)   // 0x00d21cf0

// 0x00d22670 - controller config carried in the pdemoN.dat file tail (+0x990),
// applied while an attract demo plays, restored by StartAttractDemo when it ends
extern WORD          g_AttractMode_ControllerConfig;
// 0x00d22672 - player health carried in the pdemoN.dat file tail (+0x992)
extern short         g_AttractMode_PlayerHealth;

// ============================================================================
// SECTION 13: Save / load
// ============================================================================

// Save/Load game state globals (0x00be63xx entries overlay g_playerEntity → [.gwipe])
extern int           g_healthStatus;                   // 0x00be6370 [.gwipe]
extern int           g_playerAngle;                    // 0x00be6368 [.gwipe]
extern short         g_playerBkpPosX;                  // 0x00be6380 [.gwipe]
extern short         g_playerBkpPosZ;                  // 0x00be6382 [.gwipe]
extern int           g_playerBkpHealthStat;            // 0x00be6384 [.gwipe]
extern short         g_playerBkpAngle;                 // 0x00be6388 [.gwipe]
extern int           g_playerPosX;                     // 0x00be6350 [.gwipe]
extern int           g_playerPosZ;                     // 0x00be6358 [.gwipe]
extern int           g_savesCounter;                   // 0x004d467c
extern char          g_saveFileName[260];              // 0x004d42d8
extern char          g_saveDirPrefix[260];
extern char          g_saveSlotTextBuf[80];
extern BYTE          g_saveFileBuffer[0x1000];
extern char          g_characterNameTable[4][16];
extern char          g_locationNameTable[7][40];

// ============================================================================
// SECTION 14: Sound system
// ============================================================================

extern int           g_SndFadeType;                    // 0x00bf0a2d
extern int           g_SndRampFramesLeft;              // 0x00ac9908
extern int           g_BgmSoundBank;
extern int           g_SfxBanks[64];
extern int           g_RoomSfxBanks[64];
extern int           g_CharacterSfxBanks[64];
// g_emSndBanks (0x00ac99f0) - 48 records of 2 ints (handle, slot/flags), so 96
// ints, NOT 64. PlayEntitySnd indexes g_emSndBanks[soundType * 2] for soundType
// up to 0x2f, i.e. element 94; an int[64] could not even hold the records the
// loader is supposed to fill. Record count confirmed from the original's loop
// bound in Room_LoadEnemySoundBanks: piVar7 walks from 0x00ac99f0 in 8-byte steps
// while piVar7 <= 0xac9b6f, which is records 0 through 47.
extern int           g_emSndBanks[96];
// 0x00ac99d0 - the three BGM channels. 8-byte records (see SndBankSlot); the
// original's loops all stop at 0x00ac99e8, i.e. exactly 3 entries. Previously
// declared as int[64], which both overstated the count and could not express the
// slot/paused byte fields - five separate globals were declared for addresses
// that fall inside this array (g_snd_slot_00ac99d5 etc.) and so never aliased it.
extern SndBankSlot   g_SndBank[3];
extern int           g_SfxVolume;
extern char          g_BgmPaused;
extern int           g_SndRampDirection;
extern int           g_SndRampCurrentVolume;
extern int           g_SndRampBankIndex;
extern int           g_SndDistSteps;
extern int           g_EnemySndVolume;                 // 0x00ac98d0 - enemy sound volume
extern unsigned int  g_SoundSystemFlags;               // 0x004b3998 - sound system flags (bit 3 = alt path)
extern char          g_SoundAltPathPrefix[256];        // 0x00d91bd0 - alternate sound path prefix

// 0x00ac9c00 - room boundary shape handlers, indexed by (RDT_Boundary::type &
// 0xff) and installed by Room_SetupCollisionCallbacks. The original declares
// these as four separate function pointers at 0x00ac9c04/0c/10/14 because slots
// 0 and 2 are never written; check_room_collision indexes the whole block as
// one table, so the table is what it is modelled as here.
//
// All handlers are called with the same three arguments (record, entity
// position, entity rollback position); the circle and soft-zone handlers ignore
// the ones they do not need.
typedef void (*CollisionShapeHandler)(short* bounds, int* pos, short* prevPos);
extern CollisionShapeHandler g_CollisionShapeHandlers[6];

extern int g_collPushDepthZHi;                         // 0x00be0dec - push scratch
extern int g_collPushDepthZLo;                         // 0x00be0df0 - push scratch

// --- Collision overlay (CollisionDebug.cpp, not in the original) ---
// Starts off; F8 toggles it while debug features are enabled (g_debugFeaturesEnabled).
// Drawn over the background, projected through the same path the character
// model uses.
extern BOOL g_bShowCollisionDebug;
extern int  g_iCollisionDebugY;   // world Y of the overlay plane (0 = room floor)
void CollisionDebug_Draw(void);

// --- Room boundary collision (RoomCollision.cpp) ---
void          Room_SetupCollisionCallbacks(void);                      // 0x0047d140
unsigned int  ChkOutsideCell(VECTOR* position, SVECTOR* offset,
                             int cellX, int cellZ);                    // 0x0047d270
unsigned char check_room_collision(VECTOR* position, short radius);    // 0x0047d310
short         room_collision_check_0047da50(VECTOR* position,
                                            VECTOR* offset);           // 0x0047da50
// Line-of-sight query: does ENTITY -> ENTITY+delta cross a sight-blocking
// boundary record in quadrant `cell`? 1 = blocked.
unsigned int  room_check_sight_blocked(VECTOR* delta, unsigned char cell); // 0x0047db90

// Per-room enemy sound name table (indexed by stageId * 29 + roomId)
// Each entry points to an array of 4 sound name strings (or NULL)
extern const char**  g_RoomSoundNameTable[203];
extern void*         g_SoundManager;
extern class DirectSound* g_pDirectSound;
extern DWORD         g_CachedWaveOutVolume;
extern int           g_WaitForMusicTimer;
// 0x00d226a0 - BGM channel state. 32 bits in the original: opcodes 0x4B / 0x4A
// save and restore the live channel mask through the high byte (SHL/SHR dword),
// and the channel bit is 1 << (channel + 3), which exceeds 8 bits for channel 5+.
// Reset value is 0xFF (MOV dword ptr [0x00d226a0],0xff), not 0xFFFFFFFF, so the
// existing `!= 0xFF` comparisons stay correct. Several reads in SoundSystem.cpp
// truncate with (unsigned char) - that matches the original's mixed byte/dword
// accesses and must be preserved.
extern unsigned int  g_BGM_STATE;
extern HWND          g_MainWindowHandle;
extern int           g_setVolResult;
extern int           g_CurBank;
extern int           g_SoundPanVol;
extern int           g_SndPanSet_result;
extern char*         g_wavName;
extern int           g_sndload_bank_index;
extern short         g_CurSlot;
extern int           g_SndFadeStepTbl[64];  // 0x00ac9930 - parallel to g_SndBank (3 used)

// Additional sound globals
// The former g_snd_bank_00ac99d8 / g_snd_slot_00ac99dd / g_snd_bank_00ac99e0 /
// g_snd_slot_00ac99e5 / g_snd_slot_00ac99d5 were separate C globals for addresses
// that live INSIDE the g_SndBank record array. They are now
// g_SndBank[1].handle, g_SndBank[1].slot, g_SndBank[2].handle, g_SndBank[2].slot
// and g_SndBank[0].slot respectively.
extern int           g_bgmDefaultVolume;
extern unsigned char g_prevBgmState;
extern unsigned char g_targetBgmState;
extern unsigned char* g_RoomBgmStatePtr;
// 0x00d1fdc4 - set to g_BgmRoomData (0x004d0c30) by sounds_reset. Indexed flat:
// g_bgmDataTable[(stageId * 0x20 + roomId) * 4 + (bgmState & 7)].
extern const unsigned char* g_bgmDataTable;
extern void*         g_StageDataPtr;
extern const unsigned short* g_StageVoiceOffsetTable[];
extern const char*   g_StageVoiceNamesTable[];
extern int           g_roomSfxVolume;
extern int           g_charSfxVolume;

// 3D sound panning globals (0x00ac98c8 / 0x00ac98cc)
extern unsigned short g_snd_pan_left;                  // 0x00ac98c8 - 3D sound left channel pan
extern unsigned short g_snd_pan_right;                 // 0x00ac98cc - 3D sound right channel pan

// Camera view matrix used for 3D sound Y position (0x00d22680)
extern MATRIX        MATRIX_00d22680;                  // 0x00d22680

// Sound system BGM state
extern unsigned char DAT_00bf07ef;                     // 0x00bf07ef
// 0x00bf07f0 is g_targetBgmState (declared below). A duplicate `int
// DAT_00bf07f0 = -1` used to live here for the same address: separate storage,
// never written, so the `!= -1` guards in opcodes 0x4A / 0x4B were always false
// and both commands were silent no-ops.

// Room BGM state table (separate from g_roomBgmState which is per-stage)
extern unsigned char g_abRoomBgmState[224];            // 0x00ac98e8

// ============================================================================
// SECTION 15: FMV / MCI video
// ============================================================================

extern int           g_CurrentFMVID;                   // 0x008f8790
extern unsigned char g_selectedFmvId;                  // 0x00bf07fb
extern void*         g_fmvDataPointer;                 // 0x00bf07fc
extern int           g_fmvPlayCount;                   // 0x004bae34

// MCI video
extern int           g_mciVideoDeviceID;               // 0x004bcb44
extern BOOL          g_bMCIVideoEvent;                 // 0x004bcb48
extern BOOL          g_bMCINotifyEnabled;              // 0x004bcb58
extern BOOL          g_bMCINotifyFlag;                 // 0x004bcb5c

// ============================================================================
// SECTION 16: Rendering, sprites, textures, TMD, animation
// ============================================================================

// Draw primitives
extern RectDrawDesc  g_window_rect;                    // 0x00d227b0
extern RectDrawDesc  g_rect;                           // 0x00be1150
extern RectDrawDesc  g_FadingRect;                     // 0x004ba720 - fade overlay rect
extern TextureDesc   g_TextureDesc;                    // 0x00be1160
extern int           unk_00be1180;                     // 0x00be1180
extern POLY_F4       Poly_F4_ARRAY_004ba750[4];        // 0x004ba750

// Sprite queue / ordering table
extern int           g_SpriteQueueCount;
extern int           g_OTIndex;
extern DWORD         g_SpriteQueueIndex;
extern int           g_SpriteBufferFlag;
extern int           g_SpriteAsyncFlag;

// Sprite animation slot table (6 entries x 0x14 bytes at 0x00be9a60)
// Indexed by g_spriteAnimActive in render_room_objects / render_entity
extern SpriteAnimSlot g_spriteAnimSlots[6];            // 0x00be9a60

// Per-frame room object render state (render_room_objects, 0x00473ff0)
extern int           DAT_008f8688;                     // 0x008f8688 - pass index (0 = omodels, 1 = item models)
extern int           DAT_00ae9ee4;                     // 0x00ae9ee4 - force object depth 0x33 (stage 1 rooms A/B lid)
extern int           DAT_00ae9ef8;                     // 0x00ae9ef8 - keep the fixed 0x32/0x33 object depth

// Sprite animation data buffers (pointed to by g_spriteAnimSlots entries)
extern BYTE          g_entityLightData_9ad8[];         // 0x00be9ad8 - entry[2] target
extern BYTE          g_entityLightData_bad8[];         // 0x00bebad8 - entry[3]/[4] target
extern BYTE          g_entityLightData_bb58[];         // 0x00bebb58 - entry[0]/[5] target

// Image buffer data (pointed to by g_imageBufferPtr / g_imageBufferPtr2)
extern BYTE          g_imageBufferDataA[];             // 0x00bebce8 - primary image buffer data
extern BYTE          g_imageBufferDataB[];             // 0x00bee268 - secondary image buffer data

// Image buffer pointers (set by init_and_start_game, swapped by display_die_screen)
extern void*         g_imageBufferPtr;                 // 0x00d213a8 - primary image buffer pointer
extern void*         g_imageBufferPtr2;                // 0x00d213ac - secondary image buffer pointer

// Print text
extern char          PRINT_TEXT_BUFFER[256];           // 0x00be0e20
extern int           g_PrintClutTint;

// File loading state
extern int           g_FileOpenCount;
extern int           g_FileRetryFlag;

// Image processing/status variables
//
// 0x00bebcc4 is ONE 16-bit cell and the original accesses it both ways: as two
// bytes (bank id at +0, the current tpage at +1 - see the byte writes all over
// CmdFunctions/EntityModelLoader) and as a single word, which is how cut_set
// and RestoreRoomCamera restore both halves at once:
//     004628fa  MOV AX,[0x00bebcc6]      ; g_SavedTextureBankID
//     00462903  MOV [0x00bebcc4],AX      ; bank AND page
// That is also why DoorSystem's "original writes word 0x1f15" and
// TextureLoader's "_g_TextureBankID >> 8" comments exist - the high half is the
// page id (the PS1 tpage code).
// Declaring the halves as two separate globals let the linker put 15
// bytes between them (0x1b33b and 0x1b34a in the Debug map), so
// `*(unsigned short*)&g_TextureBankID` spilled its high byte onto whatever
// followed the bank - g_MessageCurrentPtr - corrupting the live message pointer
// on every camera cut, and the page byte was never actually restored. Keep the
// pair in one packed object so the word access addresses the bytes it means.
#pragma pack(push, 1)
struct TextureBankCell {
    unsigned char bank;      // 0x00bebcc4
    unsigned char page;      // 0x00bebcc5 - tpage id
};
#pragma pack(pop)
static_assert(sizeof(TextureBankCell) == 2, "TextureBankCell must be the 16-bit cell at 0x00bebcc4");

extern TextureBankCell g_TextureBankCell;              // 0x00bebcc4
#define g_TextureBankID      (g_TextureBankCell.bank)  // 0x00bebcc4
#define g_TextureCurrentPage (g_TextureBankCell.page)  // 0x00bebcc5
extern unsigned short g_SavedTextureBankID;            // 0x00bebcc6 - saved room texture bank ID (restored after cutscenes)

// Texture/bank arrays
extern BYTE          g_textureQueueData[40];           // 0x00d22740

extern BYTE          g_psxTextureArray[32 * 0x1b60];   // 0x00a75168 - 32 banks (verified against Ghidra: spans exactly to 0x00aabd68)
extern DWORD         g_textureBankRedirect[32];        // 0x00aae2b0

// Sprite/clear color
extern float         g_color_r;                        // 0x004c336c
extern float         g_color_g;                        // 0x004c3370
extern float         g_color_b;                        // 0x004c3374
extern float         g_spriteColorScale;               // 0x004af2ac

// Back clear color (set by setBackColor)
extern unsigned char g_red_color;                      // 0x004d2be4 - back color red (0-255)
extern unsigned char g_green_color;                    // 0x004d2be5 - back color green (0-255)
extern unsigned char g_blue_color;                     // 0x004d2be6 - back color blue (0-255)

extern DWORD         g_animSlotIndex;                  // 0x008f8c78

// TMD processing flags
extern DWORD         DAT_004d2bd8;
extern DWORD         DAT_004d2bf4;
extern DWORD         DAT_004d2bdc;                     // 0x004d2bdc
extern int           DAT_004d2be0;                     // 0x004d2be0
extern DWORD         g_tmdAsyncData;                   // 0x008fc424
extern DWORD         DAT_004c1a2c;
extern DWORD         DAT_00ae9f04;
extern DWORD         DAT_00ae9f06;
extern DWORD         DAT_00ae9f00;
extern DWORD         DAT_00ae9efc;
// 0x004d6444 (DAT_004d6444) - costume variant selector. SCD opcode 0x4F
// (cmd_costume_variant_set -> FUN_0040c560) writes param & 1 here. LoadEntityEMD
// adds it to 0x33 to pick the alternate-outfit EMD (em1030/em1032) when the
// costume-swap flag (g_main_state_flags2 bit 0x4000000) is active, and the
// value is persisted in the save block at offset 0xA01.
extern DWORD         g_bCostumeVariant;

// TMD async creation params
extern DWORD         g_asyncTmdDepth;                   // 0x008fc42c
extern DWORD         g_asyncTmdDataPtr;                 // 0x00aae330
extern DWORD         g_asyncTmdObjectPtr;               // 0x008f88a0
extern DWORD         g_asyncTmdResult;                  // 0x008ffc30

// TMD model caching state (used by cmd_item_model_set / cmd_omodel_set)
extern int*          DAT_00bca0d0;                     // 0x00bca0d0
extern int*          DAT_00bca0d4;                     // 0x00bca0d4
extern unsigned char DAT_008e1c78;                     // 0x008e1c78
extern unsigned char DAT_008e1c70;                     // 0x008e1c70
extern unsigned char DAT_008e1c7c;                     // 0x008e1c7c
extern unsigned char DAT_008e1c74;                     // 0x008e1c74

// Face normal buffer
extern BYTE          g_faceNormalBuffer[250 * 8];      // 0x008fb8b0

// Object cleanup globals
extern int           g_objectDeleteFlag;               // 0x004d2bfc
extern int           g_objectCountArray[32];           // 0x008ffc40
extern int           g_objectDeleteCounter;            // 0x00aabd68
extern BYTE          g_complexTmdObjectData[0x10800];  // 0x008ffcc0 - complex TMD object data area (see Globals.cpp)

extern int*          g_objectDeletePtr;                // 0x004d2bf8 - statically points to g_tmdObjectSlotAnimPtrs
extern int           g_objectListCleanupFlag;          // 0x004d2fb4
extern int           g_objectListCleanupCount;         // 0x004d2fb0
extern DWORD         g_objectListPtrArray[0xE00];      // 0x008fc430 - 256 CMarniViewport2 entries x 0x38 bytes (0x3800)


// TMD buffers

extern int          ARRAY_00922260[2016];           // 0x00922260

extern DWORD        g_tmdTextureAllocated[48];     // 0x00922a40

extern int          g_complexTmdObjectArray[256];     // 0x00922b00
extern int          INT_ARRAY_00922f00[530];          // 0x00922f00
extern int          g_complexTmdObjectIds[256];       // 0x00923748

extern int          INT_ARRAY_00923b48[2];       // 0x00923b48

extern BYTE         g_tmdObjectBuffer[1606172];  // 0x00923b50 area

extern int          g_tmdObjectSlotAnimPtrs[251];     // 0x00aabd6c - TMD slot → animObjPtr table

extern int          DAT_00aad6ec; // 0x00aad6ec 
extern int          DAT_00aae740; // 0x00aae740 
extern int          DAT_00ac34f8; // 0x00ac34f8

extern int          DAT_008f8c74; // 0x008f8c74 - TMD texture/CLUT data for PSXTexture::Store
extern int          DAT_009104c0; // 0x009104c0 - texture bank id
extern int          DAT_008ffc34; // 0x008ffc34 - texture depth byte

extern BYTE         g_renderStateTMD[5524];         // 0x00aac158


// Global render-state objects
extern char          g_renderStateTex[0x36c];          // 0x00aad6f0

// Weapon animation angles
extern int           g_weaponAngle_Special;
extern int           g_weaponAngle_PrimX;
extern int           g_weaponAngle_PrimY;
extern int           g_weaponAngle_PrimZ;
extern int           g_weaponAngle_Sec1X;
extern int           g_weaponAngle_Sec1Y;
extern int           g_weaponAngle_Sec1Z;
extern int           g_weaponAngle_Sec2X;
extern int           g_weaponAngle_Sec2Y;
extern int           g_weaponAngle_Sec2Z;

// Animation dispatch jump tables
// (0x004c2ac8) has no array: its 3 entries are folded into the switch in
// player_anim_set_attacked_flag (PlayerAnimations.cpp).
extern void*         DAT_004ba360[];
extern void*         DAT_004c10b0[];

// Animation data constants
extern DWORD         DAT_00606060;

// Scratch globals used by animation functions
extern VECTOR        g_playerPosScratch;               // 0x00be11b0
extern SVECTOR       g_svecScratch;                    // 0x00be11a8 (gSVector in Ghidra)
extern MATRIX        g_matrixScratch;                  // 0x00be11c0 (MATRIX_00be11c0 in Ghidra)
extern unsigned int  g_deathAnimationFlag;             // 0x00be0dd8
extern unsigned int  g_animFrameIdSave;                // 0x00be0dfc - temp save for animation_frame_id
extern int           g_entityJointPosX;                // 0x00be0e18 - joint position X during render (DAT_00be0e18)
extern int           g_playerDisplacement;             // 0x00be0de0 - joint displacement for animation
extern int           g_weaponHitEnemyType;             // 0x00be0de4 - enemy type snapshot for the post-hit callbacks
extern void*         g_tempVar;                        // 0x00be0df8 - temp pointer for joint processing
extern void*         g_playerAnimFunctions[52];        // 0x00bebbd8

// ----------------------------------------------------------------------------
// Player aim/fire data tables (ROM .data, dumped from the original exe)
// ----------------------------------------------------------------------------
// 0x004c0d68 - per-weapon fire data, 8 bytes each, 14 entries.
// Indexing: normal weapons = equippedWeaponId - 2 (entries 0..9 = weapons
// 2..11); special weapons (ITEM_INGRAM 0x6F / ITEM_MINIMI 0x70) =
// equippedWeaponId - 99 (entries 12/13). Both specials carry weaponId 2, so
// they deal the handgun's damage (see the table comment in Globals.cpp).
typedef struct {
    unsigned int  weaponId;    // +0x00: weapon id passed to apply_weapon_damage
    unsigned char fireFrame;   // +0x04: animation frame the damage + sound fire on
    unsigned char sfx1;        // +0x05: first fire sound (bank 1)
    unsigned char sfx2;        // +0x06: second fire sound
    unsigned char pad;         // +0x07
} WeaponFireData;

// 0x004c0dd8 / 0x004c0e68 / 0x004c0ef8 - billboard entries, 10 bytes each,
// 14 entries each, same indexing as WeaponFireData. The three tables share
// the same layout: b0 is the spawn frame (in g_weaponFireBillboard it also
// decrements the ammo; 99 = never), type/data feed Effect_CreateBillboard,
// and x/y/z are the billboard offset from the weapon joint (joint 0xe).
// The rocket launcher path reads entries 8..11 as 8 + (ammo & 3), but the
// shipped rocket launcher has no round variants - likely scrapped data.
typedef struct {
    unsigned char b0;          // +0x00: spawn frame (ammo-decrement frame in 0x004c0dd8)
    unsigned char type;        // +0x01: billboard type
    unsigned char data;        // +0x02: billboard data
    unsigned char b3;          // +0x03: always 0
    short         x;           // +0x04: billboard position offset
    short         y;           // +0x06
    short         z;           // +0x08
} WeaponFxEntry;

extern unsigned int   g_weaponSpecialFrameWindows[24]; // 0x004c0cc0 - per-(character,motion) frame windows for special weapons
extern unsigned char  g_weaponFireEndFrame[16];        // 0x004c0d58 - auto-aim fire end frame, 16 bytes (special path reads [id-99])
extern WeaponFireData g_weaponFireData[14];            // 0x004c0d68 - 14 entries (specials at 12/13)
extern WeaponFxEntry  g_weaponFireBillboard[14];       // 0x004c0dd8 - muzzle billboard + ammo frame, 14 entries
extern WeaponFxEntry  g_weaponMuzzleFlash[14];         // 0x004c0e68 - big muzzle flash, 14 entries
extern WeaponFxEntry  g_weaponFlash2[14];              // 0x004c0ef8 - second flash, 14 entries
extern unsigned int   g_weaponFireIntervals[3];        // 0x004c0f84 - special fire intervals
extern short          g_aimHeightTable[12];            // 0x004c0fc0 - aim heights, 2 chars x 3 pairs (normal/gun/special)
extern unsigned char  g_aimReticleEnabled;             // 0x004c062c - 1 = aim reticle scan is live
extern char           g_weaponSpecialFireCountdown;    // 0x008e1c68 - special-weapon fire frame countdown

// PS1 GTE fixed-point pipe matrix globals.
//
// These MUST be contiguous and in this order: MulMatrixVec3 (0x004410e0) is
// handed &m00 and indexes m[0..8] as an array, exactly as the original does
// with its block at 0x004c3790. Declaring them as twelve separate globals let
// the toolchain lay them out in whatever order it liked - GCC put them in
// .bss in REVERSE (m22 at the lowest address), so on Linux every row but the
// first read the neighbouring variable and effect sprites projected with a
// garbage view Z (they came out as 2-pixel specks, or off-screen entirely,
// while the 3D path - which uses its own float matrix - looked fine).
// Keep the storage in one array; the names below stay usable everywhere.
extern int g_fixedPointPipeMatrix[9];        // 0x004c3790: m00..m22
extern int g_fixedPointPipeTranslation[3];   // 0x004c37b8: t0..t2
#define g_fixedPointPipe_matrix_m00 g_fixedPointPipeMatrix[0]
#define g_fixedPointPipe_matrix_m01 g_fixedPointPipeMatrix[1]
#define g_fixedPointPipe_matrix_m02 g_fixedPointPipeMatrix[2]
#define g_fixedPointPipe_matrix_m10 g_fixedPointPipeMatrix[3]
#define g_fixedPointPipe_matrix_m11 g_fixedPointPipeMatrix[4]
#define g_fixedPointPipe_matrix_m12 g_fixedPointPipeMatrix[5]
#define g_fixedPointPipe_matrix_m20 g_fixedPointPipeMatrix[6]
#define g_fixedPointPipe_matrix_m21 g_fixedPointPipeMatrix[7]
#define g_fixedPointPipe_matrix_m22 g_fixedPointPipeMatrix[8]
#define matrix_t0 g_fixedPointPipeTranslation[0]
#define matrix_t1 g_fixedPointPipeTranslation[1]
#define matrix_t2 g_fixedPointPipeTranslation[2]

// ============================================================================
// SECTION 17: Large data buffers
// ============================================================================

// 0x00bcb430 - item sprite sheet (data\item_all.pix). Sized for the Director's
// Cut's 76-row sheet (91200 B); the USA one is 72 rows (86400 B). See Globals.cpp.
extern BYTE        g_ItemsImageBuffer[91200];        // 0x00bcb430


// DAT_00be05b0 // 0x00be05b0

extern WORD  g_RawPadState;                            // 0x00be05b2
extern WORD  g_padEdgeDetectedWord;                    // 0x00be05b4 - edge-detected raw pad word


// 0x00bf0b1c - per-type animation data pointers
extern DWORD        g_effectAnimData[425];              // 0x00bf0b1c - per-type effect animation pointers (DWORD array; a BYTE declaration truncated every pointer write to its low byte)

// Model/animation buffers
// The original lays these two buffers out adjacently (0x00bf11c0 + 0xCC00 ==
// 0x00bfddc0), forming one contiguous 108544-byte region - which is exactly
// what the player EMD load needs: char10.emd is 106224 bytes and
// LoadEntityModel points g_loadDataDestPointer at g_entityModelBuffer. MSVC
// kept the two adjacent; GCC reordered them, so the spill landed on ENTITY and
// crashed at room start. They are now members of one struct, which guarantees
// contiguity on every compiler, and exposed as array references so every
// existing use (indexing, &buf, sizeof) is unchanged.
//
// `spillGuard` is load-bearing: the player model is read into `first` as one
// whole file, and the larger character models do NOT fit in the 108544-byte
// pair - char11.emd (Jill) is 112968 bytes, so 4424 bytes run past `second`.
// In the original binary that spill landed in the animation buffer and was
// harmless; in this decompilation the linker placed g_pMasterInputState
// immediately after the pair, so loading Jill overwrote keyMap and the
// joystick entries with model bytes - the game then saw a phantom gamepad
// (raw pad word stuck non-zero, keyboard dead) and the character appeared to
// move on its own. The guard absorbs every such spill; the model's own byte
// offsets inside the load region are unchanged. Largest model loaded here:
// char11.emd 112968 B (costume variants: em1030 112664 B).
struct EntityModelStorage {
    BYTE first[52224];    // 0x00bf11c0
    BYTE second[56320];   // 0x00bfddc0
    BYTE spillGuard[16384];  // not part of the original layout - see above
};
extern BYTE (&g_entityModelBuffer)[52224];
extern BYTE (&g_entityModelBuffer2)[56320];

// 0x00c0b9c0
extern BYTE         g_animationBuffer[37888];        // 0x00c0b9c0
extern DWORD        g_animObjectBuffer[0x680];       // 0x00c133c0 - weapon anim object buffer

// 0x00c14dc0 - Shoot direction data buffer
extern BYTE          g_shootDirEspBuffer[73728];

// 0x00c26dc0 - General purpose data buffer (832728 bytes)
extern BYTE          g_DataBuffer[832728];

// 0x00cf2298 - TIM Image buffer, first 20 bytes are the header
// Buffer Size: 187180 bytes (187160 headless)
// For PIX Images (Headless TIM Images), the buffer points directly to the
// bitmap address at 0x00cf22ac (g_TimImageBuffer__bitmap)
extern BYTE          g_TimImageBuffer[187160+20];
// The parentheses are load-bearing. Unparenthesised, `&g_TimImageBuffer__bitmap`
// expanded to `&g_TimImageBuffer + 20`, and since `&g_TimImageBuffer` has type
// BYTE(*)[187180] the +20 advanced 20 * 187180 = 3,743,600 bytes - so
// load_room_masks decompressed each camera's background mask 3.6MB past the end
// of this buffer, straight over unrelated .bss. Parenthesised, the expression is
// an rvalue and any `&` on it is a compile error instead of silent corruption.
#define g_TimImageBuffer__bitmap    (g_TimImageBuffer + 20)

// ============================================================================
// SECTION 18: Debug & misc
// ============================================================================

// Debug clear color
extern float         g_debugClearR;
extern float         g_debugClearG;
extern float         g_debugClearB;
extern int           g_debugTaskFrame;

// ============================================================================
// SECTION 19: Function declarations
// ============================================================================

// --- Entity update tables (per-enemy-type dispatch) ---
extern void* enemies_update_functions_tbl[48];   // 0x004d3c90 - entity type update function table (ids 22-47 = character_npc_update)
// 22 entries, not 16 - zombie_behavior_tbl is a second view of the same block
// based 10 in. See entities/Zombie.h, which owns this declaration.
extern void* zombie_states_table[22];            // 0x004bb2c8

// --- Enemy (zombie) functions ---
void zombie_update(void);                        // 0x004338c0
void zombie_init(void);                          // 0x00433440
void zombie_state_check(void);                   // 0x00433ae0

// --- Game loop / states ---
int  main_loop(void);
int  game_loop(void);
void init_and_start_game(void);
void load_global_assets(void);
void logos_state(void);
void input_test_state(void);
void title_state(void);
void debug_state(void);
void game_start(void);
void characterSelectionScreen(void);

// --- Print text / primitives ---
void PrintText8x8(short x, short y, unsigned char color, char shadow);
void PrintText8x14(short x, short y, unsigned char color, char flags);
void PrintTextFormatted(short x, short y, unsigned char color, const unsigned char* data);
void PrintFormattedText(short x, short y, unsigned char color, const unsigned char* data);
void draw_rect(RectDrawDesc* rect, int blend, int flags);
void QueueTexturedSprite(float gameX, float gameY, float gameW, float gameH,
                         MarniHandle tex, unsigned int depth);
int  RebuildTextureSRV(int slotIndex, int clutIndex);
int  GetTextureNumCLUTs(int slotIndex);

// --- Task scheduler ---
void TaskScheduler_Init(void);
void TaskScheduler_Update(void);
void TaskScheduler_Reset(void);
void Task_execute(int id, void* func);
void Task_suspend(int id);
void Task_Resume(int id);
void Task_sleep(int frames);
void Task_exit(void);
void Task_chain(void* func);
void ExecAsync(void* callback);
void SetFrameRateMode(int status_flags);

// --- Input ---
void  InputUpdate(void);
DWORD PlayerPad_Update(void);
DWORD ReadPadBoth(void);
DWORD JoyToPSX(DWORD pcMask, int player);
void  InitInputKeyBindings(void);
// Replaces g_JoyRemapTbl[1] with the port's pad defaults, but only when the
// table is empty or still the untouched original layout. Safe to call often.
void  InstallPadDefaultBindings(void);
int   read_sidewinder_pad(void);
void  OnKeyDown(HWND hwnd, WPARAM wparam);

// --- Sound ---
void PauseSounds(void);
void ResumePausedSounds(void);
void UpdateSoundFadeState(void);
void UpdateSoundDecay(void);
void UpdateMusicWaitState(void);
void sounds_reset(void);
void LoadSoundBank(int sound_bank_id, void* buffer);
void PauseGameSoundsAsync(void);
void ResumeGameSoundsAsync(void);
void DestroySoundManagerAsync(void);
int  getSndStat(int bank);
void play_sfx(int bank, int soundId, int mode);       // 0x0047f870
void play_sfx(int bank, int soundId);
void Play3DSnd(int bank, int soundId, int vol, int pos);
void PlayEntitySnd(unsigned char soundType);
unsigned short LookupFootstepZone(short posX, short posZ);
void Snd_em(unsigned char em_snd_id);
void Calc3DSndPan(VECTOR* pos);
int  CalcPanVolume(int panL, int panR);
void load_room_sfx(unsigned char soundTableIndex);
void load_character_sfx(unsigned char charId);

// --- Video / FMV ---
void UpdateVideoPlayback(void);
void ClearScreen(void);
void QueueVideoPlayback(int fmvId, int flag);         // 0x004422d0

// --- Marni system ---
BOOL IsGraphicsSystemReadyForOperation(void);
void InitializeMarniSystem(void);
void EnumerateDisplayModes(void);
void EnumerateD3DRenderers(void);
void InitJoysticks(void);
int  IsSideWinderPadConnected(void);
void CreateLights(int count);
void* CMarniDirect3D_Constructor(void* self, HWND hWnd, int width, int height, int modeID, int adapterID);

// --- Screen effects / presentation ---
void ResetScreenPanning(void);
void SetScreenOffset(int x, int y);
void ApplyScreenShake(void);
void UpdateMessageDisplay(void);
void ResetScreenAndRebuildSprites(int param);
void ApplyShakeAndRebuildSprites(void);
void SetScreenReadyWithDebugColor(int r, int g, int b);
void SetScreenReady(int param);
void ResetFmvRenderState(void);                       // 0x004973a0 - FMV cleanup on state change
void FUN_00470a90(void);
void FrameRateGovernor(void);
void FUN_0040a8f0(void* param);
void ResetSpriteQueue(void);
void OT_InsertPrimitive(void* prim, unsigned int depth);

// --- Texture pages / async objects ---
void AsyncCreateTexturePage(void);
void AsyncDestroyTexturePage(void);
void AsyncCreateObject(void);
void AsyncDeleteObject(void);
int  FUN_0046c230(void* data);
int  FUN_0046c280(int id);
void FUN_0046ccd0(void* buf, int a, int b);
void LoadShadowMaskTexture(void* imageBuffer, int slotBase);
void SetupTexturePageHandles(int slotIndex, int pageIndex);
void FUN_0046fb50(int a, int b, int* data);
void CreateTexturedQuad(int viewportSlot, int texturePageId, int* vertexData);
void ProcessTextureImage(void* imageBuffer, short textureBankID, short pageOffset, int slotIndex);
void LoadTexturePage(void* imageBuffer, short texId, short pageOffset, int slotIndex,
                     int unused, short posX, short posY, unsigned int flags);
void LoadEffectTextureSheet(int slot, void* timData);   // effect sheets at explicit slots (TextureLoader.cpp)
int  LoadEffectTextureSheetVariants(int baseSlot, void* timData, int maxRows);  // one SRV per CLUT row (TextureLoader.cpp)
int  create_texture_page(void* data, int mode);
void destroy_texture_page(int handle);
void cleanup_texture_slot(int slot);
void clear_textures(void);
#undef LoadImage  // Win32 macro conflicts with Marni LoadImage
void LoadImage(int srcData, int srcSlot, int dstSlot, short format, short x, short y, short width, short height, int mode);
void LoadItemImage(int item_id, int image_index, int img_buffer);
// sortClass is port-only and defaults to 1 == SPRITE_CLASS_NORMAL (the literal
// avoids pulling game/SpriteRenderer.h into this header). Pass
// SPRITE_CLASS_EFFECT for a sprite that has to interleave with the 3D pass by
// depth rather than draw flat on top of it - see the lab terminal's LEDs.
int  display_texture(TextureDesc* texture, unsigned short depth, int slot, int pageCount,
                     unsigned int sortClass = 1u);
int  AddSprite_Ex(TextureDesc* texture, unsigned short depth, int slot, int pageCount); // 0x0046f280
int  SubmitEffectSprite_Ex(TextureDesc* texture, unsigned short fade, int slot,
                           int pageCount, int depthKey);                               // 0x0046edb0
void display_computer_lab(void);                       // 0x00412390 - ComputerLab.cpp
void display_slides(void);                             // 0x00463300 - LabSlides.cpp
void lab_slides_start(void);                           // 0x00463320 - shared init block
void lab_slides_update(void);                          // 0x004633c0 - shared update block
void lab_slides_finish(void);                          // 0x004636b0 - shared finish block
int  AddTintSprite(TextureDesc* texture, unsigned short brightness);
int  AddTintSprite_Ex(TextureDesc* texture, unsigned short brightness);   // 0x0046f8a0
// slide.tim CLUT-variant SRVs built by TexturePage_LoadImage (SpriteRenderer.cpp)
MarniHandle Slides_GetVariantSRV(int variant);
int         Slides_GetVariantCount(void);
int         Slides_GetVariantSlot(int variant);

// --- Misc game helpers ---
void UpdateDemoTimer(void);
void StMask(int param, int param2);
void setMenuScreenOffset(int w, int h, int x, int y, int mode);
void CenterScreenOrigin(void);
void SetSubpixelOffset(int x, int y);
void CreateTimestampedLogFile(void);
void ShowVideoModeDebugText(void);
// --- Entity path/trail animation (PathTrail.cpp) ---
void FUN_0048a210(void* joint);                       // 0x0048a210 - path animation step
void FUN_00485820(void* trailObj, int count);         // 0x00485820 - stage geometry build
void FUN_00485a00(void* trailObj, int brightness);    // 0x00485a00 - stage trail draw
void FUN_00485aa0(void* trailObj);                    // 0x00485aa0 - stage slot release

// --- game_loop dependency functions (called per-frame from 0x00480b30) ---
void check_camera_switch(int param);                  // 0x00462cc0
void display_room_camera_bg(void);                    // 0x00462d50
void check_desk_state(void);                          // 0x0041bc90
void check_itembox_state(void);                       // 0x0041c240
void check_typewriter_state(void);                    // 0x0041c330
void check_event_item_usage(void);                    // 0x0041c490
void check_and_display_interactive_screen(void);      // 0x0042a030
void TexturePage_ClearAll(void);                      // 0x0046c360
void SetupJointStructures(void* buf);                 // 0x0048b9e0
// 0x00462620 - implemented in EntityModelLoader.cpp. The parameter types must
// match that definition exactly: an extra (int,int,void*,void*) declaration used
// to exist here, which silently overloaded the real function and routed the menu
// call sites to an empty stub (the options menu never loaded its weapon/animation
// set as a result).
void LoadEquippedWeaponAnimation(unsigned char weapon_id, unsigned char param_2,
                                unsigned int anim_buffer, unsigned int param_4);
void FUN_0048c020(int param);                         // 0x0048c020 - entity weapon setup
void FUN_004844b0(void);                              // 0x004844b0 - menu cleanup sub
void FUN_0047d0e0(void);                              // 0x0047d0e0 - effect cleanup
void RestoreRoomCamera(void);                         // 0x00462940 - re-apply RDT room camera after menu
void FUN_0040ac80(int idx, void* lightData);          // 0x0040ac80 - set light data
void SetLightMatrix(MATRIX* m);                       // 0x00482e20 - entity light matrix to D3D
void SetRotAndTransMatrix(MATRIX* m);                 // 0x00482df0 - set GTE rot+trans matrix
void FUN_00483250(int p0, int p1, int p2, int p3, int p4, int p5, void* p6); // 0x00483250 - entity sprite render helper
int  FUN_00470c60(void* prim, int depth);             // 0x00470c60 - queue EKG primary line
int  FUN_00470e60(void* prim, int depth);             // 0x00470e60 - queue EKG gradient line
void setBackColor(unsigned short r, unsigned short g, unsigned short b); // 0x0040ada0
void empty_40ae40(int param);                         // 0x0040ae40
void update_entities(void);                           // 0x0048f0f0
void update_player_anim(void);                        // 0x00494d90
void update_player_position(PlayerEntity* ent, int a);// 0x0041c060
int  check_door(unsigned char* entry);              // 0x0041b6d0 room_check_actions[5]
int  no_room_action(unsigned char* entry);          // 0x0041c050 room_check_actions[0]
int  door_try_enter(unsigned char* entry);           // 0x0041b400 room_check_actions[1]
int  display_msg_room_action(unsigned char* entry);  // 0x0041b630 room_check_actions[2]
int  include_key(unsigned char* entry);              // 0x0041b650 room_check_actions[3]
int  set_key_flag(unsigned char* entry);             // 0x0041b6a0 room_check_actions[4]
int  check_door_side(unsigned char* entry);          // 0x0041b790 room_check_actions[6]
int  flag_bank_set(unsigned char* entry);            // 0x0041b850 room_check_actions[7]
int  open_itembox(unsigned char* entry);             // 0x0041b990 room_check_actions[8]
int  create_room_event(unsigned char* entry);        // 0x0041b9e0 room_check_actions[9]
int  room_action_noop10(unsigned char* entry);       // 0x0041ba00 room_check_actions[0x0A]
int  room_action_effect(unsigned char* entry);       // 0x0041ba10 room_check_actions[0x0B]
int  set_stairs_zone(unsigned char* entry);          // 0x0041baa0 room_check_actions[0x0C]
int  set_room_event_flag(unsigned char* entry);      // 0x0041bae0 room_check_actions[0x0D]
int  check_desk(unsigned char* entry);               // 0x0041bb10 room_check_actions[0x0E]
int  pickup_key_event(unsigned char* entry);         // 0x0041be70 room_check_actions[0x0F]
int  check_typewriter(unsigned char* entry);         // 0x0041bed0 room_check_actions[0x10]
int  stairs_height_update(unsigned char* entry);     // 0x0041bf90 room_check_actions[0x11]
int  check_action_object(void);                      // 0x0041c150 - action-key probe of the event table
void memset_(unsigned int* dst, int dwordCount);     // 0x0047cf60 - zero N dwords
int  check_climb_object(void);                       // 0x00474930 - action-key climb/push probe
int  ChkPlReachEntity(int obj);                      // 0x00474a20
void door_transition_update(void);                   // 0x00495d70
void room_event_item_pickup(void);                   // 0x00451700
void room_event_take_item(void);                     // 0x004631c0 (was FUN_004631c0)
void use_room_action_item(void);                     // 0x004631f0
void DrawFadeSpr(void);                              // 0x00456d30
// 0x00474090 - room 3D-object collision + the walk-into-it push driver.
// Ghidra calls this `update_sounds`; it has nothing to do with sound.
void update_room_objects(void);                       // 0x00474090
int  ChkEntitySlide(unsigned char* ent, unsigned char* obj, int moveObject); // 0x00474330
void render_room_objects(void);                       // 0x00473ff0
void EntityComputeJointWorldMatrices(int ca);         // 0x0048c190 - Compute entity joint world matrices
void EntityApplyLookAtRotation(void);                 // 0x0045a2e0
void render_entity(Entity* ent);                      // 0x0048c350
void update_2d_effects(void);                         // 0x0047c0c0
void DrawRoomSpr(void);                               // 0x00475b80
void DebugSaveMenu(void);                             // 0x00494050
void die_state(void);                                 // 0x00481310
void TimeoutDeathFadeOut(void);                       // 0x00481250
void StartAttractDemo(void);                          // 0x004818b0
void check_menus_state(void);                         // 0x004815f0
void main_menu(void);                                 // 0x00463710 - in-game menu (status/inventory/map)
void options_menu(void);                              // 0x004761b0 - options/configuration menu
void room_transition_load(void);                      // 0x004813c0 - room/stage transition loader
// Door 3D animation (DoorSystem.cpp, 0x00412300 / 0x00444770): loads the door's
// .dor data file and spawns the door-animation task on task 1. Called from
// room_transition_load while the destination room loads underneath.
void door_system_load_data(void);                     // FUN_00412300 - .dor + texture page
void door_system_start_animation(void);               // Task_execute(1, FUN_00444770)
void set_fading(int type, int counter);               // 0x0047b980
int  cmd_bgm_stop_all(void);                                  // 0x00460b80 - stop sound banks
void display_die_screen(void);                        // 0x00443090 - death screen display
void update_image_fading_(const void* param1, short param2);  // 0x00443500 - fill g_TextureDesc from a 0x10-byte template
void image_update(int param);                         // 0x00443550 - wavy died.tim strip effect
int  _fsin(int angle);                                // 0x0040a960 - fix12 sine (angle 0..32768 = 2pi)
unsigned int set_message_display(unsigned short msgId, unsigned short flags); // 0x00455670
unsigned int set_item_description_message(unsigned short descIndex, unsigned short pauseGame); // 0x00455730

// --- System checks / installation ---
DWORD GetFreeDiskSpaceMB(LPCSTR lpPath);
BOOL  EnumerateDriveTypes(void);
int   ShowMessageBox(HWND hWnd, LPCSTR lpMsg, LPCSTR lpCaption, UINT uType);
void  crashlog_install(void);
void  crashlog_mark(const char* step);
BOOL  IsGameInstalled(void);
BOOL  LoadInstallationConfiguration(BYTE* pInstallPath);

// --- Display config ---
int     CheckVideoCapabilities(void);
INT_PTR EnumerateAndSelectDisplayMode(void);
int     GetDisplayModeCount(void);
void    GetDisplayModeRect(int modeIndex, DWORD* pRect);

// --- Cleanup ---
void CleanupSharedMemory(void);
void UpdateGameStatus(void);
void CleanupVideoConfigAndSaveAllSettings(void);
void DestroyAllSoundBanks(void);
void CleanupAsyncTasks(void);

// --- Sound system helpers ---
void SaveGameSettings(void);
void CVideoSystem_Cleanup(void* ptr);
void ProbeWaveOutDevicesAndCacheVolume(void);
void StartSoundSystemAsync(HWND hwnd);
void RestoreWaveOutVolume(void);

// --- Window proc ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// --- Memory ---
void* operator_new(size_t size);
void  operator_delete(void* ptr);

// --- Title screen ---
void set_display_resolution(int w, int h, int mode);
void display_image(int slot, void* buffer, int width, int height);
void title_setup_texture_pages(int slot, int mode);
int  check_save_files_exist(void);
void UpdateTitleTextSprite(unsigned char brightness, unsigned char selectionId);
void title_exit_loop(void);
void fade_update(void);
void init_title_screen(void);
void nullsub_0047eb80(void); // empty no-op in original, PS1 leftover
void set_scene_render_param(int value);
void update_title_options(void);

// --- Save / load ---
void LoadSaveGameState(int mode, int flags, int useInkRibbon, int sfxBank, int cutsceneReset);
int  FileWrite(const char* name, void* buf, int len);
int  ReadSaveFile(const char* path, void* buffer);
void EnsureDirectoryExists(const char* path);
int  GetSaveLocationIndex(int stageId, int roomId);
void DrawSaveCursor(short x, short y, int mode);

// --- Rooms ---
void cut_set(void);
void Room_LoadCameraSprites(void);
void Room_SetupCamera(void);
void load_room_masks(int param_1);
int  Room_ApplySpriteFlags(void);
void FUN_004403c0(int param_1, int param_2, int param_3, int* param_4);
void init_room(void);
void room_set(void);
void LoadRoomRdt(void);
void update_room_bgm(void);
void LoadHeldItemsImages(void);
void load_room_bg(void);
void load_room_bg_image(void);
void load_room_bg_masks(void);
int  unpack_pakfile_(void* src, void* dst);
void RoomSpr_SetActive(char id);    // 0x00476170
void RoomSpr_SetInactive(char id);  // 0x00476130
void use_room_action_item(void);
void rearrange_item_slots(void);
unsigned int Flg_ck(int baseAddr, unsigned int bitIndex);
void         FUN_00473f10(int* baseAddr, unsigned int bitIndex);  // 0x00473f10 - clear a bit flag

// --- Effect sprites / room init ---
// EffectSprites.cpp
unsigned char load_effect_sprite_data(unsigned char* effectAnimIndex,
                                      unsigned char* effectAnimData,
                                      void* rdtBase, unsigned char startSlot);   // 0x0047bbe0
void setup_effect_sprite_textures(unsigned char startSlot);                      // FUN_0047bc80
void InitRoomEffSprite(void);                                                    // 0x0047b9b0
// TmdAnimation.cpp
void reverse_anim_frame_data(int animFieldAddr);                                 // 0x0048bea0
void SetupEntityJointAnimation(void);                                            // 0x0048bef0
// TextureLoader.cpp
void SetupTextureBankData(short param_1);                                        // 0x00473a30
// LabSlides.cpp
void load_slides_images(void);                                                   // 0x00478110

// --- Game state / character setup ---
void SetupCharacterData(void);
void display_game_loading_message(void);
void LoadAttractModePlayerData(void);
void empty_0047eb90(int param);
void load_shoot_direction_data(void);

// --- PS1 GTE trig / matrix ---
int GteSin(int angle);   // 0x004409f0 - 12-bit angle -> 14-bit sine
int GteCos(int angle);   // 0x00440a10 - 12-bit angle -> 14-bit cosine
MATRIX* RotMatrix(SVECTOR* r, MATRIX* m);
void MatrixSetTranslation(MATRIX* m, int* translation);
void SetGlobalScaledRotationMatrix(MATRIX* m);
void GetMatrixTranslation(MATRIX* m);
int  MatrixToCamera(MATRIX* m);
int  SquareRoot0(int val);
int  GetAngleQuadrantValue(int slope);
unsigned short CalculateAngleBetweenPointsXZ(int pos1_x, int pos1_z, int pos2_x, int pos2_z);
void vectorMul3(VECTOR* v0, VECTOR* v1, VECTOR* v2);   // 0x0040a550 - v2 = v0 x v1
int  VectorNormal(VECTOR* v0, VECTOR* v1);             // 0x0040a5c0 - scale to 4096, returns len^2

// --- PS1 sprite primitive helpers ---
void GteSpriteHeaderInit(SVECTOR* header);
int  GteClutBuild(int param_1, short param_2);
int  GteTpageBuild(unsigned short param_1, unsigned short param_2, int param_3, int param_4);

// --- Player animation functions ---
void player_anim_attack_recoil(void);
void player_anim_simple_recovery(void);
void player_anim_multi_attack(void);
void player_anim_dispatch_4c2ac8(void);
void player_anim_crawling(void);
void player_anim_set_attacked_flag(void);
void player_anim_dispatch_4ba360(void);
void player_anim_dispatch_4c10b0(void);
void player_anim_poison_death(void);
void player_anim_death_billboard(void);
void player_anim_limb_physics(void);
void player_anim_enemy_interact(void);
void player_anim_death_alt(void);
void player_anim_dispatch_4b1a90(void);

// --- Animation helpers ---
unsigned int Joint_move(char reverse, unsigned int animHeader, unsigned int animBase, short blendStep);
void  entity_apply_anim_vertex(Entity* entity, unsigned int emdScratch1, unsigned int emdScratch2);
void  entity_extract_anim_vertex(Entity* entity, unsigned int emdScratch1, unsigned int emdScratch2, char reverse);
unsigned char Effect_CreateBillboard(unsigned char type, unsigned char depthGroup, short yaw, void* spriteInfo, void* pos, char lightFactor);
void  EffectActor_UpdateAndRender(void);        // 0x0047c2f0 - per-slot effect update+render (EffectSystem.cpp)
void  JointApplyColorTint(JointStruct* joint, int param2, int param3, void* data);
void  JointSetColorTint(int modelObjPtr, unsigned int packedColor);
MATRIX* RotMatrixY(int angle, MATRIX* m);
void  ApplyMatrix(MATRIX* m, SVECTOR* src, VECTOR* dst);   // 0x00409cd0 - dst is 3 INTs
void  ApplyMatrixSV(MATRIX* m, SVECTOR* src, SVECTOR* dst);
void  fp_lerp(SVECTOR* current, SVECTOR* target, int weightCurrent, int weightTarget, SVECTOR* out);
void  BillboardSetColor(void* quad, int unused1, int unused2, unsigned int color);
void  BillboardAdjSize(void* quad, short halfW, short halfH);
void  BillboardSetSize(void* quad, short halfW, short halfH);
void  BillboardSetRect(void* quad, short right, short left, short front, short back); // 0x004567d0
short GetPlayerInputMasked(void);
void  EntityUpdateWeaponJoint(int weaponIdx);
MATRIX* MulMatrixInPlace(MATRIX* m0, MATRIX* m1);
void  Add_speedXZ(int angleOffset);
void  set_player_animations_functions(void);
MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2);
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1);
MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);
void    ApplyLVAndMulMatrix(MATRIX* m0, MATRIX* m1);
void    ApplyLVAndMul0Matrix(void* m0, void* m1, void* mOut);

// --- Entity rendering functions ---
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);
void    ScaleMatrixCols(MATRIX* m, VECTOR* scale);  // 0x0040a2a0
MATRIX* RotMatrixYXZ(SVECTOR* r, MATRIX* m);        // 0x00409ed0
void    rotate_entity(MATRIX* parentMtx, void* animData, unsigned char jointIdx); // 0x0048c2a0
void    GteRotationMatrixYXZ(int x, int y, int z, int* result); // 0x00440b70 - writes int[9], row-major 3x3 at 14-bit scale
void    FUN_00483080(void* spriteData, int depthShift);  // 0x00483080 - TMD entity render

// --- GTE state globals ---
extern MATRIX        g_gteRotTransMatrix;              // 0x008f88e8 - GTE rotation+translation matrix buffer
                                                     // (t[2] at offset 0x1C = depth value for rendering)
extern DWORD         g_d3dLightData[36];               // 0x00aae748 - D3D light data (3 lights x 12 DWORDs)
extern DWORD         g_d3dLightFlags;                  // 0x00aae770 - D3D light dirty flags
extern DWORD         g_d3dAmbientColor;                // 0x00aae774 - D3D packed ambient color
