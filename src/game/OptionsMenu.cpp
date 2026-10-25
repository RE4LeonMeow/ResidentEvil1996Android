// OptionsMenu.cpp - Options/configuration menu (0x004761b0)
// Decompiled from Ghidra with original addresses.
//
// Handles:
//   - Key binding configuration (keyboard keys -> PS1 buttons)
//   - Display configuration (keyboard D-pad layout)
//   - Joystick configuration (joystick buttons -> PS1 buttons)
//   - Player model rendering with animation in options background
//
// State machine: s_optMainState controls the current submenu (0-5)
// ============================================================================

#include "../Globals.h"
#include "../platform/platform.h"
#include "game/Types.h"
#include "game/Entities.h"
#include "game/OptionsMenu.h"
#include "game/FileLoader.h"
#include "PrintText.h"
#include <cstring>
#include <cstdio>
#include "../system/AssetPath.h"

// ============================================================================
// Forward declarations for unimplemented functions
// ============================================================================
extern void SetLightMatrix(MATRIX* m);
extern void SetRotAndTransMatrix(MATRIX* m);
extern void FUN_004896c0(void* joint, short p1, short p2, int p3);
extern void FUN_0048a210(void* joint);
extern void FUN_00483250(int p0, int p1, int p2, int p3, int p4, int p5, void* p6);
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData);
extern void menu_update_equipped_weapon(void); // 0x00463ec0 in MainMenu.cpp

// ============================================================================
// FUN_00497de0 (0x00497de0) - Options-menu keyboard scan.
// Original: ExecAsync(options_key_scan_task / 0x00497d90) then return the
// result byte at 0x00ac4020. The async task walks the 67-entry VK watch table
// at 0x004d46e8 with GetAsyncKeyState and stores the first key that is
// currently held (high bit) without the "pressed since last call" edge bit
// (low bit). The port scans synchronously, which is equivalent at menu frame
// rates.
// ============================================================================
static unsigned char g_nOptKeyScanResult = 0;   // 0x00ac4020
static const unsigned char g_abOptKeyScanTable[67] = {   // 0x004d46e8
    0x0d, 0x20, 0x11, 0x1b,                         // Enter, Space, Ctrl, Esc
    0x25, 0x26, 0x27, 0x28,                         // arrows
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,  // 0-9
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,  // A-J
    0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54,  // K-T
    0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,             // U-Z
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,  // numpad 0-9
    0x6e,                                           // numpad decimal
    0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,       // ; = , - . / `
    0xdb, 0xdc, 0xdd, 0xde,                         // [ \ ] '
    0xe2,                                           // OEM 102
};

unsigned char FUN_00497de0(void)
{
    g_nOptKeyScanResult = 0;
    for (int i = 0; i < 67; i++) {
        SHORT state = plat_key_state((int)g_abOptKeyScanTable[i]);
        if ((state & 0x8000) != 0 && (state & 1) == 0) {
            g_nOptKeyScanResult = g_abOptKeyScanTable[i];
            break;
        }
    }
    return g_nOptKeyScanResult;
}

// Helper for byte-offset access to ENTITY (matching Ghidra's raw pointer arithmetic)
// In Ghidra, _ENTITY is an undefined1* (byte pointer), but in our code ENTITY is Entity*,
// so ENTITY + N does pointer arithmetic (N * sizeof(Entity)). This macro gives us byte access.
#define EB(offset) (((unsigned char*)ENTITY)[(offset)])

// ============================================================================
// Options-menu demo animation scripts (0x004bfd78 - 0x004c0540)
//
// The player model in the options background acts out the action that the
// highlighted option row controls. Each script is a list of {animId, reverse}
// int pairs: animId goes to ENTITY->attackAnim (+0xBD) and reverse becomes the
// first argument of Joint_move. The scripts advance two ints per step and wrap
// once the step counter reaches the matching length entry.
//
// Data lifted verbatim from the retail executable; the per-character tables are
// selected with g_playerEntity.id (0 = Chris, 1 = Jill - options_menu masks the
// id with 1 during setup).
// ============================================================================

// 0x004bfd78 (11 frames) - ACTION/SELECT
static const int s_optAnim_ChrisAction[22] = {
    4, 0, 4, 1, 0, 0, 0, 1, 5, 0, 6, 0,
    6, 0, 6, 0, 5, 1, 0, 0, 0, 1,
};
// 0x004bfdd0 (6 frames) - DASH/CANCEL
static const int s_optAnim_ChrisCancel[12] = {
    3, 0, 3, 0, 3, 0, 2, 0, 0, 0, 0, 1,
};
// 0x004bfe00 (24 frames) - GET READY
static const int s_optAnim_ChrisStart[48] = {
    5, 0, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1,
    7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1,
    7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1,
    7, 1, 7, 1, 7, 1, 5, 1, 0, 0, 0, 1,
};
// 0x004bfec0 (29 frames) - FWD./U.ATTACK
static const int s_optAnim_ChrisFwdAttack[58] = {
    2, 0, 2, 0, 2, 0, 0, 0, 0, 1, 5, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 5, 1, 0, 0, 0, 1,
};
// 0x004bffa8 (30 frames) - BACK/L.ATTACK
static const int s_optAnim_ChrisBackAttack[60] = {
    3, 0, 3, 0, 3, 0, 3, 0, 0, 0, 0, 1,
    5, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 5, 1, 0, 0, 0, 1,
};
// 0x004c0098 (2 frames) - R.TURN / L.TURN
static const int s_optAnim_ChrisTurn[4] = {
    2, 0, 2, 0,
};
// 0x004c00a8 (13 frames)
static const int s_optAnim_JillAction[26] = {
    4, 0, 4, 1, 0, 0, 1, 0, 0, 1, 5, 0,
    6, 0, 6, 0, 6, 0, 5, 1, 0, 0, 1, 0,
    0, 1,
};
// 0x004c0110 (7 frames)
static const int s_optAnim_JillCancel[14] = {
    3, 0, 3, 0, 3, 0, 2, 0, 0, 0, 1, 0,
    0, 1,
};
// 0x004c0148 (25 frames)
static const int s_optAnim_JillStart[50] = {
    5, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 5, 1, 0, 0, 1, 0,
    0, 1,
};
// 0x004c0210 (31 frames)
static const int s_optAnim_JillFwdAttack[62] = {
    2, 0, 2, 0, 2, 0, 0, 0, 1, 0, 0, 1,
    5, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 5, 1, 0, 0, 1, 0,
    0, 1,
};
// 0x004c0308 (32 frames)
static const int s_optAnim_JillBackAttack[64] = {
    3, 0, 3, 0, 3, 0, 3, 0, 0, 0, 1, 0,
    0, 1, 5, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
    7, 0, 7, 0, 7, 0, 7, 0, 5, 1, 0, 0,
    1, 0, 0, 1,
};
// 0x004c0408 (2 frames)
static const int s_optAnim_JillTurn[4] = {
    2, 0, 2, 0,
};
// 0x004c0418 (1 frame) - stand still (SUB SCREEN / OPTION rows)
static const int s_optAnim_Idle[2] = {
    1, 0,
};

// 0x004c0420 / 0x004c0448 - key & display config scripts, one per option row
static const int* const s_optAnimTbl_Chris[10] = {
    s_optAnim_ChrisAction, s_optAnim_ChrisCancel, s_optAnim_ChrisStart,
    s_optAnim_ChrisFwdAttack, s_optAnim_ChrisBackAttack, s_optAnim_ChrisTurn,
    s_optAnim_ChrisTurn, s_optAnim_Idle, s_optAnim_Idle, NULL,
};
static const int* const s_optAnimTbl_Jill[10] = {
    s_optAnim_JillAction, s_optAnim_JillCancel, s_optAnim_JillStart,
    s_optAnim_JillFwdAttack, s_optAnim_JillBackAttack, s_optAnim_JillTurn,
    s_optAnim_JillTurn, s_optAnim_Idle, s_optAnim_Idle, NULL,
};
// 0x004c04b0 / 0x004c04d8 - matching script lengths, in ints
static const int s_optAnimLen_Chris[10] = { 22, 12, 48, 58, 60, 4, 4, 2, 2, 0 };
static const int s_optAnimLen_Jill[10]  = { 26, 14, 50, 62, 64, 4, 4, 2, 2, 0 };

// 0x004c0478 / 0x004c0490 - joystick config has only 5 rows
static const int* const s_optJoyAnimTbl_Chris[6] = {
    s_optAnim_ChrisAction, s_optAnim_ChrisCancel, s_optAnim_ChrisStart,
    s_optAnim_Idle, s_optAnim_Idle, NULL,
};
static const int* const s_optJoyAnimTbl_Jill[6] = {
    s_optAnim_JillAction, s_optAnim_JillCancel, s_optAnim_JillStart,
    s_optAnim_Idle, s_optAnim_Idle, NULL,
};
// 0x004c0508 / 0x004c0520
static const int s_optJoyAnimLen_Chris[6] = { 22, 12, 48, 2, 2, 0 };
static const int s_optJoyAnimLen_Jill[6]  = { 26, 14, 50, 2, 2, 0 };

// 0x004c0470 / 0x004c0500 / 0x004c04a8 / 0x004c0538 - per-character dispatch
static const int* const* const PTR_PTR_004c0470[2] = { s_optAnimTbl_Chris, s_optAnimTbl_Jill };
static const int* const     PTR_DAT_004c0500[2] = { s_optAnimLen_Chris, s_optAnimLen_Jill };
static const int* const* const PTR_PTR_004c04a8[2] = { s_optJoyAnimTbl_Chris, s_optJoyAnimTbl_Jill };
static const int* const     PTR_DAT_004c0538[2] = { s_optJoyAnimLen_Chris, s_optJoyAnimLen_Jill };

// File path strings for options menu backgrounds
static const char s_optBgKeyConfig[]  = GAME_DATA_ROOT "data\\Jopt06.tim";  // 0x004c4400
static const char s_optBgDisplayCfg[] = GAME_DATA_ROOT "data\\Opt11.tim";   // 0x004c43c8
static const char s_optBgJoystick[]   = GAME_DATA_ROOT "data\\Side06.tim";  // 0x004c43e4

// Options menu light data (3 lights × 16 bytes each)
// 0x00d22700: Light 0 {pos_x, pos_y, pos_z, r, g, b, pad, pad}
static int s_optLightData0[4];  // 0x00d22700
static int s_optLightData1[4];  // 0x00d22710
static int s_optLightData2[4];  // 0x00d22720

#define DAT_00be0e18 g_entityJointPosX  // 0x00be0e18 - entity joint position X (Globals.cpp)

// 0x00be9a88 is g_spriteAnimSlots[2] (base 0x00be9a60 + 2 * 0x14); the original
// options renderer indexes from there with g_spriteAnimActive, same as
// render_entity does in gameplay.
#define DAT_00be9a88 (*(unsigned char*)&g_spriteAnimSlots[2])

// Load/save state flag (aliased from g_loadSaveStateFlag)
// 0x004d4678
static int s_loadSaveStateFlag;

// ============================================================================
// Static globals - Options menu state
// ============================================================================

// Options menu state (0x00ac9cf0-0x00ac9e80 range)
static char   s_optUseJoystickMode;    // 0x00ac9cf8 - 0=key config, 1=joystick config
static char   s_optCurrentTab;         // 0x00ac9cf9 - 0=key bindings, 1=display, 2=joystick
static char   s_optMainState;          // 0x00ac9e70 - 0-5 state machine
static char   s_optSubInitState;       // 0x00ac9e71 - 0=init, 1=active, other=return
static char   s_optSubSubState;        // 0x00ac9e72 - sub-sub-menu state
static char   s_optCursorPos;          // 0x00ac9e73 - cursor position 0-2
static unsigned char  s_optRepeatTimer;   // 0x00ac9e79
static unsigned short s_optPrevButtons;   // 0x00ac9e7a

// Key binding entry structure (20 bytes each)
struct KeyBindEntry {
    unsigned char vkCode;       // +0x00: virtual key code or PS1 button value
    unsigned char displayChar;  // +0x01: display character index
    unsigned char symbolChar;   // +0x02: symbol character index
    unsigned char pad;          // +0x03
    int keyIndex;               // +0x04: key/button index
    int animState;              // +0x08: animation highlight state
    int pad2;                   // +0x0C
    int pad3;                   // +0x10
};
static_assert(sizeof(KeyBindEntry) == 0x14, "KeyBindEntry size mismatch");

// 0x00ac9d00 - 0x00ac9e68: Key binding display table (18 entries)
// Layout: [0..8] = keyboard keys (ACTION..QUICKTURN), [9..17] = joystick buttons
static KeyBindEntry s_keyBindDisplay[18]; // 0x00ac9d00

// Additional key bind entries at end of display table
static KeyBindEntry s_keyBindAccept;     // 0x00ac9e40 (ACTION accept key)
static KeyBindEntry s_keyBindCancel;     // 0x00ac9e54 (CANCEL key)

// Options temp state block (0x00bcb2e0-0x00bcb430)
static int    s_optTempFlags;         // 0x00bcb2e4
static int    s_optCursorIndex;       // 0x00bcb2e8
static int    s_optAcceptButtonMask;  // 0x00bcb2ec
static int    s_optDebounceTimer;     // 0x00bcb2f0
static int    s_optCancelKeyVK;       // 0x00bcb2f4
static KeyBindEntry s_optTempEntries[9]; // 0x00bcb300
static int    s_optWalkAnimTrigger;   // 0x00bcb3b4
static int    s_optAnimFrameCounter;  // 0x00bcb3b8
static int    s_optCursorHighlight[11]; // 0x00bcb3c0 (44 bytes, 11 ints)
static unsigned char s_optKeyScanResult;   // 0x00bcb3ec
static int    s_optJoyLabelYOffset;   // 0x00bcb3f0 - joypad tab label Y offset
                                     // (read at 0x00453303, never written: always 0)
static int    s_optAnimFrameData;     // 0x00bcb3f4
static int    s_optJoyButtonScan;     // 0x00bcb3f8
static unsigned char s_optPrevKeyScan;     // 0x00bcb3fc
static int    s_optPrevJoyButtonScan; // 0x00bcb400
static int    s_optSwappedButtonVal;  // 0x00bcb404
static int    s_optDisplayXOffset;    // 0x00bcb410
static int    s_optDisplayYOffset;    // 0x00bcb414
static int    s_optAnimSkipFlag;      // 0x00bcb418
static int    s_optAcceptKeyVK;       // 0x00bcb41c
static int    s_optIdleAnimTrigger;   // 0x00bcb420
static int    s_optUnknown424;        // 0x00bcb424

// Joystick remap backup tables — shared with the save screen (see Globals.h),
// the original has a single pair of buffers at 0x004d3f58 / 0x004d3fd8.
#define s_joyRemapBackupKey g_joyRemapBackupKey
#define s_joyRemapBackupJoy g_joyRemapBackupJoy

// Sound config sensitivity indices
static int    s_optSensitivityIdx[9]; // 0x008e1c38
static int    s_optDefaultSensIdx;    // 0x008e1c34

// ============================================================================
// Static const data tables
// ============================================================================

// Y positions for key config labels (5 entries, 0x004c43a8)
static const int s_optKeyLabelY[5] = {34, 75, 119, 161, 204}; // 0x004c43a8

// Y positions for display config labels (10 entries, 0x004c4380)
static const int s_optDisplayLabelY[10] = {25, 45, 65, 90, 110, 130, 150, 178, 204, 0}; // 0x004c4380

// The button-scan state machines below treat any change in the pad word as a
// button event, and search bits 8-15 for the button that was pressed. Bits 0-7
// are stick and POV-hat DIRECTIONS, which on the original SideWinder layout
// were unmapped and so never appeared here; now that the port binds the hat,
// they would churn the scan every time the player moved the cursor - the row
// would appear to react to the D-pad and the edit state would not settle.
// Mask them out: every reader of s_optJoyButtonScan cares only about buttons.
#define JOY_SCAN_BUTTON_MASK 0xFFFFFF00u

// X/Y positions for the 8 sidewinder config rows (0x004c0580 / 0x004c05a0).
// Rows 0-2 are the left label column, rows 3-7 the right one.
static const short s_optJoyLabelX[8] = {238, 238, 238, 13, 13, 13, 13, 238}; // 0x004c0580
static const short s_optJoyLabelY[8] = {175, 143, 111, 175, 143, 111, 79, 79}; // 0x004c05a0

// Text resource data — compile-time encoded via STR() macro (0x004c4308)
static constexpr auto s_optText_ActionSelect = STR("ACTION/SELECT");   // 0x004c4308
static constexpr auto s_optText_DashCancel   = STR("DASH/CANCEL");     // 0x004c4318
static constexpr auto s_optText_GetReady     = STR("GET READY");       // 0x004c4328
static constexpr auto s_optText_FwdUAttack   = STR("FWD./U.ATTACK");   // 0x004c4338
static constexpr auto s_optText_BackLAttack  = STR("BACK/L.ATTACK");   // 0x004c4348
static constexpr auto s_optText_RTurn        = STR("R.TURN");          // 0x004c4358
static constexpr auto s_optText_LTurn        = STR("L.TURN");          // 0x004c4360
static constexpr auto s_optText_SubScreen    = STR("SUB SCREEN");      // 0x004c4368
static constexpr auto s_optText_Option       = STR("OPTION");          // 0x004c4378

// Label text pointers
static const unsigned char* const s_optLabelText_Action     = s_optText_ActionSelect;
static const unsigned char* const s_optLabelText_Cancel     = s_optText_DashCancel;
static const unsigned char* const s_optLabelText_Start      = s_optText_GetReady;
static const unsigned char* const s_optLabelText_LRotate    = s_optText_FwdUAttack;
static const unsigned char* const s_optLabelText_RRotate    = s_optText_BackLAttack;
static const unsigned char* const s_optLabelText_Run        = s_optText_RTurn;
static const unsigned char* const s_optLabelText_Aim        = s_optText_LTurn;
static const unsigned char* const s_optLabelText_QuickTurn  = s_optText_SubScreen;
static const unsigned char* const s_optLabelText_Map        = s_optText_Option;

// Joystick button name text resources (0x004c0540)
static constexpr auto s_joyBtnText_Action   = STR("ACTION");     // 0x004c0540
static constexpr auto s_joyBtnText_Dash     = STR("DASH");       // 0x004c0548
static constexpr auto s_joyBtnText_GetReady = STR("GET READY");  // 0x004c0550
static constexpr auto s_joyBtnText_Menu     = STR("MENU");       // 0x004c0560
static constexpr auto s_joyBtnText_Option   = STR("OPTION");     // 0x004c0568
static constexpr auto s_joyBtnText_NotUsed  = STR("NOT USED");   // 0x004c0570

// sprintf format strings
static const char s_fmt_c[] = "%c";   // 0x004c05c4
static const char s_fmt_plus[] = "+"; // 0x004c05c0


// ============================================================================
// options_input_repeat (0x00451900)
// Input repeat handler for D-pad cursor navigation.
// Handles auto-repeat with configurable timing.
// ============================================================================
unsigned short options_input_repeat(unsigned char* repeatTimer, unsigned short* prevButtons, unsigned char timing, unsigned short mask)
{
    unsigned short held = (unsigned short)g_PlayerPadHeld;
    unsigned short result = held & mask;

    if ((mask & *prevButtons & (unsigned short)g_button_pressed_id) == 0) {
        timing = timing >> 4;
    }
    else {
        unsigned char t = *repeatTimer;
        *repeatTimer = t - 1;
        if ((unsigned char)(t - 1) != 0) goto done;
        result = mask & (held | (unsigned short)g_button_pressed_id);
        timing = timing & 0xf;
    }
    *repeatTimer = timing;
done:
    *prevButtons = (unsigned short)g_button_pressed_id;
    return result;
}


// ============================================================================
// options_map_key_to_print_index (0x00453950)
// Maps joystick button index to font character for display.
//
// Index is a bit position in the pad word: 0-3 stick directions, 4-7 POV hat
// (both render as arrow glyphs), 8+ pad buttons. The original stops at index
// 16 = button 9, because it spells the button number with a single digit and
// runs out at '9'; every button past that showed as '_'. A modern pad has
// more - a DualShock 4 on WinMM reports 14, and the pad word has room for 24 -
// so the run continues with 'A'..'O' for buttons 10-24. One glyph per button
// is all the fixed-width column can show, and the value is printed with "%c".
// ============================================================================
void options_map_key_to_print_index(int param_1)
{
    int idx = *(int*)((unsigned char*)param_1 + 4);
    unsigned char ch;
    switch (idx) {
    case 0:  ch = 0x10; break;
    case 1:  ch = 0x12; break;
    case 2:  ch = 0x0f; break;
    case 3:  ch = 0x11; break;
    case 4:  ch = 0x10; break;
    case 5:  ch = 0x12; break;
    case 6:  ch = 0x0f; break;
    case 7:  ch = 0x11; break;
    case 8:  ch = 0x31; break;
    case 9:  ch = 0x32; break;
    case 10: ch = 0x33; break;
    case 11: ch = 0x34; break;
    case 12: ch = 0x35; break;
    case 13: ch = 0x36; break;
    case 14: ch = 0x37; break;
    case 15: ch = 0x38; break;
    case 16: ch = 0x39; break;   // button 9
    default:
        // Port addition: buttons 10-24 (bit indices 17-31) as 'A'..'O'.
        if (idx >= 17 && idx <= 31) {
            ch = (unsigned char)('A' + (idx - 17));
        } else {
            ch = 0x5f; // '_' - unbound
        }
        break;
    }
    *(unsigned char*)(param_1 + 2) = ch;
    *(unsigned char*)(param_1 + 1) = ch;
}


// ============================================================================
// options_map_vk_to_controller_symbol (0x00453790)
// Maps VK code to controller symbol index (stored at offset 2).
// ============================================================================
void options_map_vk_to_controller_symbol(unsigned char* param_1)
{
    switch (*param_1) {
    case 0x25: param_1[2] = 0x00; return;
    case 0x26: param_1[2] = 0x01; return;
    case 0x27: param_1[2] = 0x02; return;
    case 0x28: param_1[2] = 0x03; return;
    case 0x60: param_1[2] = 0x04; return;
    case 0x61: param_1[2] = 0x05; return;
    case 0x62: param_1[2] = 0x06; return;
    case 0x63: param_1[2] = 0x07; return;
    case 0x64: param_1[2] = 0x08; return;
    case 0x65: param_1[2] = 0x09; return;
    case 0x66: param_1[2] = 0x0a; return;
    case 0x67: param_1[2] = 0x0b; return;
    case 0x68: param_1[2] = 0x0c; return;
    case 0x69: param_1[2] = 0x0d; return;
    case 0x6e: param_1[2] = 0x0e; return;
    case 0xba: param_1[2] = 0x23; return; // ';'
    case 0xbb: param_1[2] = 0x24; return; // '='
    case 0xbc: param_1[2] = 0x3b; return; // ','
    case 0xbd: param_1[2] = 0x25; return; // '-'
    case 0xbe: param_1[2] = 0x5d; return; // '.'
    case 0xbf: param_1[2] = 0x26; return; // '/'
    case 0xc0: param_1[2] = 0x27; return; // '`'
    case 0xdb: param_1[2] = 0x28; return; // '['
    case 0xdc: param_1[2] = 0x1d; return; // '\'
    case 0xdd: param_1[2] = 0x1e; return; // ']'
    case 0xde: param_1[2] = 0x1f; return; // '\''
    case 0xe2: param_1[2] = 0x20; return; // extra
    }
}


// ============================================================================
// options_map_vk_to_font_index (0x00453610)
// Maps VK code to font character index (stored at offset 1).
// Same switch as options_map_vk_to_controller_symbol but sets byte at +1.
// ============================================================================
void options_map_vk_to_font_index(unsigned char* param_1)
{
    switch (*param_1) {
    case 0x25: param_1[1] = 0x00; return;
    case 0x26: param_1[1] = 0x01; return;
    case 0x27: param_1[1] = 0x02; return;
    case 0x28: param_1[1] = 0x03; return;
    case 0x60: param_1[1] = 0x04; return;
    case 0x61: param_1[1] = 0x05; return;
    case 0x62: param_1[1] = 0x06; return;
    case 0x63: param_1[1] = 0x07; return;
    case 0x64: param_1[1] = 0x08; return;
    case 0x65: param_1[1] = 0x09; return;
    case 0x66: param_1[1] = 0x0a; return;
    case 0x67: param_1[1] = 0x0b; return;
    case 0x68: param_1[1] = 0x0c; return;
    case 0x69: param_1[1] = 0x0d; return;
    case 0x6e: param_1[1] = 0x0e; return;
    case 0xba: param_1[1] = 0x23; return;
    case 0xbb: param_1[1] = 0x24; return;
    case 0xbc: param_1[1] = 0x3b; return;
    case 0xbd: param_1[1] = 0x25; return;
    case 0xbe: param_1[1] = 0x5d; return;
    case 0xbf: param_1[1] = 0x26; return;
    case 0xc0: param_1[1] = 0x27; return;
    case 0xdb: param_1[1] = 0x28; return;
    case 0xdc: param_1[1] = 0x1d; return;
    case 0xdd: param_1[1] = 0x1e; return;
    case 0xde: param_1[1] = 0x1f; return;
    case 0xe2: param_1[1] = 0x20; return;
    }
}


// ============================================================================
// options_print_14x14 (0x00456460)
// Render 14x14 controller symbol glyphs from PRINT_TEXT_BUFFER.
// ============================================================================
void options_print_14x14(short x, short y, unsigned char tint, char mirror)
{
    unsigned char brightness = tint >> 4;
    if (brightness == 0) brightness = 2;
    tint = tint & 0xf;

    g_TextureDesc.flags = (unsigned int)(mirror != 0) * 0x40000000 + 0x40;
    g_TextureDesc.screenX = x - g_ScreenOffsetX;
    g_TextureDesc.texturePage = 1;
    g_TextureDesc.screenY = y - g_ScreenOffsetY;
    g_TextureDesc.width = 0xe;
    g_TextureDesc.height = 0xe;
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;

    if (mirror != 0) tint = tint + 8;
    g_TextureDesc.clutX = 0x100;
    g_TextureDesc.clutY = tint + 0x1e0;

    unsigned char* pb = (unsigned char*)PRINT_TEXT_BUFFER;
    do {
        g_TextureDesc.texU = (*pb % 9) * 14;
        g_TextureDesc.texV = (*pb / 9) * 14 + 0x1c;
        AddTintSprite(&g_TextureDesc, (unsigned short)brightness);
        g_TextureDesc.screenX = g_TextureDesc.screenX + 14;
        pb++;
    } while (*pb != 0);
}


// ============================================================================
// options_print_8x8_glyph (0x00456360)
// Render 8x8 variant text glyphs from PRINT_TEXT_BUFFER.
// ============================================================================
void options_print_8x8_glyph(short x, short y, unsigned char tint, char mirror)
{
    unsigned char brightness = tint >> 4;
    if (brightness == 0) brightness = 2;
    tint = tint & 0xf;

    g_TextureDesc.flags = (unsigned int)(mirror != 0) * 0x40000000 + 0x40;
    g_TextureDesc.width = 8;
    g_TextureDesc.height = 8;
    g_TextureDesc.texturePage = 1;
    g_TextureDesc.screenX = x - g_ScreenOffsetX;
    g_TextureDesc.screenY = y - g_ScreenOffsetY;
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;

    if (mirror != 0) tint = tint + 8;
    g_TextureDesc.clutX = 0x100;
    g_TextureDesc.clutY = tint + 0x1e0;

    unsigned char* pb = (unsigned char*)PRINT_TEXT_BUFFER;
    do {
        g_TextureDesc.texU = (*pb & 0xf) << 3;
        unsigned char* next = pb + 1;
        g_TextureDesc.texV = (*pb & 0xf1) >> 1;
        AddTintSprite(&g_TextureDesc, (unsigned short)brightness);
        g_TextureDesc.screenX = g_TextureDesc.screenX + 8;
        pb = next;
    } while (*pb != 0);
}


// ============================================================================
// options_menu_exit (0x00476b00)
// Initiate exit from options menu with fade.
// ============================================================================
void options_menu_exit(void)
{
    s_optMainState = 4; // 0x00ac9e70
    StMask(0, 3);
    set_fading(2, 0x800);
    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
}


// ============================================================================
// options_init_keybind_display (0x00477230)
// Populate key binding display table from g_keyBindingData and g_JoyRemapTbl.
// Scans all 28 keyboard keys and 32 joystick buttons to find 9 PS1 actions.
// ============================================================================
void options_init_keybind_display(void)
{
    KeyBindEntry* entry;
    int i;

    // Clear all 18 entries (2 groups of 9)
    entry = &s_keyBindDisplay[0];
    i = 2;
    do {
        int j = 9;
        KeyBindEntry* cur = entry;
        do {
            cur->vkCode = 0x5f;        // '_'
            cur->displayChar = 0xff;
            cur->symbolChar = 0xff;
            cur->keyIndex = -1;
            cur->animState = 0;
            cur->pad2 = 0;
            cur->pad3 = 0;
            entry = cur + 1;
            j--;
            cur = entry;
        } while (j != 0);
        i--;
    } while (i != 0);

    // Scan keyboard binding table (g_keyBindingData[0..27] with g_JoyRemapTbl[0])
    const unsigned int* remapTbl = (const unsigned int*)(g_JoyRemapTbl[0] + 0x1b);
    i = 0x1b;
    do {
        unsigned int btnMask = *remapTbl;
        if (btnMask == 0x80) {         // ACTION (PAD_SQUARE)
            s_keyBindDisplay[0].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[0].keyIndex = i;
            s_optAcceptKeyVK = s_keyBindDisplay[0].vkCode; // 0x00bcb41c
        }
        else if (btnMask == 0x40) {    // CANCEL (PAD_CROSS)
            s_keyBindDisplay[1].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[1].keyIndex = i;
            s_optCancelKeyVK = s_keyBindDisplay[1].vkCode; // 0x00bcb2f4
        }
        else if (btnMask == 8) {       // START
            s_keyBindDisplay[2].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[2].keyIndex = i;
        }
        else if (btnMask == 0x1000) {  // L ROTATE (PAD_TRIANGLE)
            s_keyBindDisplay[3].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[3].keyIndex = i;
        }
        else if (btnMask == 0x4000) {  // R ROTATE (PAD_CROSS)
            s_keyBindDisplay[4].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[4].keyIndex = i;
        }
        else if (btnMask == 0x2000) {  // RUN (PAD_CIRCLE)
            s_keyBindDisplay[5].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[5].keyIndex = i;
        }
        else if (btnMask == 0x8000) {  // AIM (PAD_SQUARE)
            s_keyBindDisplay[6].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[6].keyIndex = i;
        }
        else if (btnMask == 0x800) {   // R1
            s_keyBindDisplay[7].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[7].keyIndex = i;
        }
        else if (btnMask == 0x900) {   // L1+R1 combo (QUICK TURN)
            s_keyBindDisplay[8].vkCode = g_keyBindingData[i];
            s_keyBindDisplay[8].keyIndex = i;
        }
        remapTbl--;
        i--;
    } while (i >= 0);

    // Scan the joystick remap table, entries 31..1 descending (lower indices
    // win, so a function bound to several buttons displays the lowest one).
    // The original walks DOWN from &g_JoyWarnPrinted, which in the ORIGINAL
    // binary IS g_JoyRemapTbl[1][32]: the table sits at 0x004b1858 and spans
    // 0x100 bytes, so 0x004b1958 - g_JoyWarnPrinted - is exactly one past its
    // end, and the walk covers indices 31..1 of the live table.
    //
    // The port cannot address it that way. g_JoyWarnPrinted is
    // zero-initialised so the linker puts it in .bss, while g_JoyRemapTbl has
    // non-zero initialisers and lands in .data (dumpbin: SECT4 vs SECT5), so
    // the walk-down read 32 unrelated globals instead of the table and
    // whatever happened to equal a function value decided what this screen
    // showed. Index the table directly - see docs/MEMORY_LAYOUT.md on
    // past-the-end addressing, and docs/GAMEPAD_INPUT.md.
    //
    // The dropped index-32 iteration only ever read the warning flag (0 or 1),
    // which matches none of the values tested below.
    for (i = 31; i > 0; i--) {
        int joyVal = (int)g_JoyRemapTbl[1][i];
        if (joyVal == 0x80) {
            s_keyBindDisplay[9].vkCode = 0x80;    // 0x00ac9dc0
            s_keyBindDisplay[9].keyIndex = i;      // 0x00ac9db8
        }
        else if (joyVal == 0x40) {
            s_keyBindDisplay[10].vkCode = 0x40;   // 0x00ac9dd4
            s_optAcceptButtonMask = 1 << ((unsigned char)i & 0x1f); // 0x00bcb2ec
            s_keyBindDisplay[10].keyIndex = i;     // 0x00ac9dcc
        }
        else if (joyVal == 8) {
            s_keyBindDisplay[11].vkCode = 8;       // 0x00ac9de8
            s_keyBindDisplay[11].keyIndex = i;     // 0x00ac9de0
        }
        else if (joyVal == 0x1000) {
            s_keyBindDisplay[12].vkCode = 0x1000;  // 0x00ac9dfc
            s_keyBindDisplay[12].keyIndex = i;     // 0x00ac9df4
        }
        else if (joyVal == 0x4000) {
            s_keyBindDisplay[13].vkCode = 0x4000;  // 0x00ac9e10
            s_keyBindDisplay[13].keyIndex = i;     // 0x00ac9e08
        }
        else if (joyVal == 0x2000) {
            s_keyBindDisplay[14].vkCode = 0x2000;  // 0x00ac9e24
            s_keyBindDisplay[14].keyIndex = i;     // 0x00ac9e1c
        }
        else if (joyVal == 0x8000) {
            s_keyBindDisplay[15].vkCode = 0x8000;  // 0x00ac9e38
            s_keyBindDisplay[15].keyIndex = i;     // 0x00ac9e30
        }
        else if (joyVal == 0x800) {
            s_keyBindDisplay[16].vkCode = 0x800;   // 0x00ac9e4c
            s_keyBindDisplay[16].keyIndex = i;     // 0x00ac9e44
        }
        else if (joyVal == 0x900) {
            s_keyBindDisplay[17].vkCode = 0x900;   // 0x00ac9e60
            s_keyBindDisplay[17].keyIndex = i;     // 0x00ac9e58
        }
    }
}


// ============================================================================
// options_render_cursor (0x00477490)
// Draw cursor highlight rectangle on the options menu.
// ============================================================================
void options_render_cursor(void)
{
    // X positions for 3 cursor positions in key config mode
    int cursorXPositions[3];
    cursorXPositions[0] = -0x95;   // -149
    cursorXPositions[1] = -0x6e;   // -110
    cursorXPositions[2] = -0x46;   // -70

    // Texture U offsets for each cursor position
    unsigned char texUOffsets[12];
    texUOffsets[0] = 3;   texUOffsets[1] = 0; texUOffsets[2] = 0; texUOffsets[3] = 0;
    texUOffsets[4] = 0x2a; texUOffsets[5] = 0; texUOffsets[6] = 0; texUOffsets[7] = 0;
    texUOffsets[8] = 0x52; texUOffsets[9] = 0; texUOffsets[10] = 0; texUOffsets[11] = 0;

    g_TextureDesc.flags = 0x01000040;

    if (s_optUseJoystickMode == 0) {
        g_TextureDesc.screenX = (short)cursorXPositions[(unsigned char)s_optCursorPos];
    }
    else {
        g_TextureDesc.screenX = -0x95;
    }

    g_TextureDesc.height = 0xc;
    g_TextureDesc.texturePage = 2;
    g_TextureDesc.screenY = (short)(s_optUseJoystickMode * -0xe + -0x4b);
    g_TextureDesc.width = (unsigned short)(s_optUseJoystickMode * 0x4f + 0x25);
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.clutY = 0x1e0;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.clutX = 0;

    if (s_optUseJoystickMode == 0) {
        g_TextureDesc.texU = texUOffsets[(unsigned char)s_optCursorPos * 4];
    }
    else {
        g_TextureDesc.texU = 3;
    }
    g_TextureDesc.texV = (unsigned char)(s_optUseJoystickMode * 0x10 + 4);

    display_texture(&g_TextureDesc, 0, 0xb, 1);
}


// ============================================================================
// options_render_entity (0x004775b0)
// Render player model joints for options menu background.
// ============================================================================
void options_render_entity(int param_1)
{
    short jointFlags;
    int i;
    MATRIX* pJoint;
    MATRIX localMatrix;
    unsigned char jointIdx;

    g_animFrameIdSave = (unsigned int)(((*(unsigned char*)(param_1 + 3) & 0x7f) == 0));

    jointIdx = *(char*)(param_1 + 0x8d) - 1;
    pJoint = (MATRIX*)((unsigned int)jointIdx * 0x7c + *(int*)(param_1 + 0x98));

    // Not in the original: guard against being called before the player model
    // has been loaded, which would walk a null joint array.
    if (*(int*)(param_1 + 0x98) == 0 || *(unsigned char*)(param_1 + 0x8d) == 0) {
        return;
    }

    do {
        jointFlags = pJoint->m[0][0];

        if ((jointFlags & 4) != 0) {
            g_svecScratch.y = 0x1e;
            g_svecScratch.x = 0;
            g_svecScratch.z = 0;
            pJoint->m[0][2] = -0x14;
            pJoint->m[1][1] = 0;
            pJoint->m[1][0] = 200;
            FUN_004896c0(pJoint, 0xffdd, 0xff9c, 1);
        }

        if ((jointFlags & 1) == 0) {
            if ((jointFlags & 0x20) != 0) {
                FUN_0048a210(pJoint);
            }
        }
        else {
            ApplyLVAndMul0Matrix(&g_RoomCameraData, pJoint[2].m[0] + 2, &localMatrix);

            // Copy g_lightMatrix to g_matrixScratch
            MATRIX* src = &g_lightMatrix;
            MATRIX* dst = &g_matrixScratch;
            for (i = 8; i != 0; i--) {
                *(unsigned int*)dst->m[0] = *(unsigned int*)src->m[0];
                src = (MATRIX*)(src->m[0] + 2);
                dst = (MATRIX*)(dst->m[0] + 2);
            }

            if (g_animFrameIdSave == 0) {
                if ((jointFlags & 0x74) != 0) goto checkSwitchZone;
doRender:
                DAT_00be0e18 = pJoint->t[0];
                SetLightMatrix(&g_matrixScratch);
                SetRotAndTransMatrix(&localMatrix);
                FUN_00483250(0, 0, 0, pJoint->t[1], 0, 4,
                    &DAT_00be9a88 + (unsigned int)g_spriteAnimActive * 0x14);
            }
            else if ((jointFlags & 0x74) != 0) {
checkSwitchZone:
                i = is_entity_in_switch_zone((VECTOR*)(pJoint[2].t + 1), g_CurrentRdtDataTypePtr);
                if (i != 0) goto doRender;
            }
        }

        pJoint = (MATRIX*)(pJoint[-4].m[0] + 2);
        bool done = (jointIdx == 0);
        jointIdx--;
        if (done) return;
    } while (true);
}


// ============================================================================
// options_menu_render (0x00476b40)
// Main render function for options menu.
// Renders all text labels and key bindings for the current tab,
// then renders the player model.
// ============================================================================
void options_menu_render(void)
{
    if (s_optCurrentTab == 0) {
        // Tab 0: Key config mode - show joystick bindings (entries 9-11)
        // 0x00ac9db4 = s_keyBindDisplay[9] (joystick ACTION)
        unsigned char* pEntry = (unsigned char*)&s_keyBindDisplay[9];
        const int* pY = s_optKeyLabelY;

        do {
            KeyBindEntry* next = (KeyBindEntry*)(pEntry + 0x14);
            options_map_key_to_print_index((int)pEntry);
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)pEntry[1]);
            PrintText8x14(0x91, (short)(*pY + 1), 0, 0);
            pEntry = (unsigned char*)next;
            pY++;
        } while (pEntry < (unsigned char*)&s_keyBindDisplay[12]); // entries 9,10,11

        // Accept key (entry 16) at Y[3]
        options_map_key_to_print_index((int)&s_keyBindDisplay[16]);
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[16].displayChar);
        PrintText8x14(0x91, (short)(s_optKeyLabelY[3] + 1), 0, 0);

        // Cancel key (entry 17) at Y[4]
        options_map_key_to_print_index((int)&s_keyBindDisplay[17]);
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[17].displayChar);
        PrintText8x14(0x91, (short)(s_optKeyLabelY[4] + 1), 0, 0);

        // Action labels
        PrintFormattedText(0xa2, s_optKeyLabelY[0], 0, s_optLabelText_Action);
        PrintFormattedText(0xa2, s_optKeyLabelY[1], 0, s_optLabelText_Cancel);
        PrintFormattedText(0xa2, s_optKeyLabelY[2], 0, s_optLabelText_Start);
        PrintFormattedText(0xa2, s_optKeyLabelY[3], 0, s_optLabelText_QuickTurn);
        PrintFormattedText(0xa2, s_optKeyLabelY[4], 0, s_optLabelText_Map);
    }
    else if (s_optCurrentTab == 1) {
        // Tab 1: Display config (keyboard D-pad)
        const int* pY = s_optDisplayLabelY;
        unsigned char* pEntry = (unsigned char*)&s_keyBindDisplay[0].symbolChar;

        do {
            options_map_vk_to_controller_symbol(pEntry - 2);
            if ((*pEntry == 0xff) || ((short)*pEntry == 0xbc) || ((short)*pEntry == 0xbf)) {
                sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)pEntry[-2]);
                PrintText8x14((short)(s_optDisplayXOffset + 0x91),
                    (short)(*pY + s_optDisplayYOffset + 1), 0, 0);
            }
            else {
                sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(short)*pEntry);
                options_print_14x14((short)(s_optDisplayXOffset + 0x8e),
                    (short)(*pY + s_optDisplayYOffset + 1), 0, 0);
            }
            pY++;
            pEntry += 0x14;
        } while (pEntry < (unsigned char*)&s_keyBindDisplay[9].symbolChar); // 0x00ac9db6 - 9 entries only

        // Render current/up/down/left/right column labels
        options_map_vk_to_font_index((unsigned char*)&s_keyBindDisplay[0]);
        if (s_keyBindDisplay[0].displayChar == 0xff) {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[0].vkCode);
            PrintText8x8(0x130, (short)(s_optDisplayLabelY[0] + 6), 0, '\0');
        }
        else {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[0].displayChar);
            options_print_8x8_glyph(0x130, s_optDisplayLabelY[0] + 6, 0, 0);
        }

        options_map_vk_to_font_index((unsigned char*)&s_keyBindDisplay[1]);
        if (s_keyBindDisplay[1].displayChar == 0xff) {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[1].vkCode);
            PrintText8x8(0x120, (short)(s_optDisplayLabelY[1] + 6), 0, '\0');
        }
        else {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[1].displayChar);
            options_print_8x8_glyph(0x120, s_optDisplayLabelY[1] + 6, 0, 0);
        }

        // START key
        options_map_vk_to_font_index((unsigned char*)&s_keyBindDisplay[2]);
        if (s_keyBindDisplay[2].displayChar == 0xff) {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[2].vkCode);
            PrintText8x8(0x120, (short)(s_optDisplayLabelY[0] + 6), 0, '\0');
            PrintText8x8(0x120, (short)(s_optDisplayLabelY[3] + 6), 0, '\0');
            PrintText8x8(0x120, (short)(s_optDisplayLabelY[4] + 6), 0, '\0');
        }
        else {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[2].displayChar);
            options_print_8x8_glyph(0x120, s_optDisplayLabelY[0] + 6, 0, 0);
            options_print_8x8_glyph(0x120, s_optDisplayLabelY[3] + 6, 0, 0);
            options_print_8x8_glyph(0x120, s_optDisplayLabelY[4] + 6, 0, 0);
        }

        // L1
        options_map_vk_to_font_index((unsigned char*)&s_keyBindDisplay[3]);
        if (s_keyBindDisplay[3].displayChar == 0xff) {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[3].vkCode);
            PrintText8x8(0x130, (short)(s_optDisplayLabelY[1] + 6), 0, '\0');
            PrintText8x8(0x130, (short)(s_optDisplayLabelY[3] + 6), 0, '\0');
        }
        else {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[3].displayChar);
            options_print_8x8_glyph(0x130, s_optDisplayLabelY[1] + 6, 0, 0);
            options_print_8x8_glyph(0x130, s_optDisplayLabelY[3] + 6, 0, 0);
        }

        // R1
        options_map_vk_to_font_index((unsigned char*)&s_keyBindDisplay[4]);
        if (s_keyBindDisplay[4].displayChar == 0xff) {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[4].vkCode);
            PrintText8x8(0x130, (short)(s_optDisplayLabelY[4] + 6), 0, '\0');
        }
        else {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_keyBindDisplay[4].displayChar);
            options_print_8x8_glyph(0x130, s_optDisplayLabelY[4] + 6, 0, 0);
        }

        // Empty strings for layout
        sprintf(PRINT_TEXT_BUFFER, s_fmt_plus);
        PrintText8x8(0x128, (short)(s_optDisplayLabelY[0] + 6), 0, '\0');
        PrintText8x8(0x128, (short)(s_optDisplayLabelY[1] + 6), 0, '\0');
        PrintText8x8(0x128, (short)(s_optDisplayLabelY[3] + 6), 0, '\0');
        PrintText8x8(0x128, (short)(s_optDisplayLabelY[4] + 6), 0, '\0');

        // Action labels for display config
        PrintFormattedText(0xa2, s_optDisplayLabelY[0], 0, s_optLabelText_Action);
        PrintFormattedText(0xa2, s_optDisplayLabelY[1], 0, s_optLabelText_Cancel);
        PrintFormattedText(0xa2, s_optDisplayLabelY[2], 0, s_optLabelText_Start);
        PrintFormattedText(0xa2, s_optDisplayLabelY[3], 0, s_optLabelText_LRotate);
        PrintFormattedText(0xa2, s_optDisplayLabelY[4], 0, s_optLabelText_RRotate);
        PrintFormattedText(0xa2, s_optDisplayLabelY[5], 0, s_optLabelText_Run);
        PrintFormattedText(0xa2, s_optDisplayLabelY[6], 0, s_optLabelText_Aim);
        PrintFormattedText(0xa2, s_optDisplayLabelY[7], 0, s_optLabelText_QuickTurn);
        PrintFormattedText(0xa2, s_optDisplayLabelY[8], 0, s_optLabelText_Map);
    }

    // Render player model animation.
    //
    // Not in the original: zoneFlags (entity+3) is the "inside camera switch zone"
    // flag maintained by the room code, and options_render_entity only takes the
    // unconditional render path while its low 7 bits are set. The original menu
    // inherits a non-zero value from gameplay; our room code does not always
    // maintain it, and with it clear every joint gets filtered out (the options
    // menu has no RDT zone data to fall back on), so force it here.
    g_playerEntity.zoneFlags = 1;
    ENTITY = (Entity*)&g_playerEntity;
    g_playerEntity.attackAnim = 1; // 0xbd
    Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
    EntityComputeJointWorldMatrices(g_playerEntity.unk_ca);
    EntityApplyLookAtRotation();
    options_render_entity((int)&g_playerEntity);
    options_render_cursor();
}


// ============================================================================
// options_display_config_handler (0x00451960)
// Display config sub-menu handler.
// Returns 0 to keep running, non-zero to exit.
// ============================================================================
unsigned int options_display_config_handler(void)
{
    // Find L1 button mask from joystick table.
    // NOTE: the original leaves this scan result uninitialized when 0x800 is
    // not in the table (stack garbage, never 0 in practice). Initializing to 0
    // made the exit gate below fire instantly whenever the table has no 0x800
    // entry (read_sidewinder_pad() is 0 with no pad connected), so use a
    // never-matching sentinel instead.
    int l1ButtonMask = -1;
    int i;
    // The original walks DOWN from &g_JoyWarnPrinted, which in the ORIGINAL
    // binary IS g_JoyRemapTbl[1][32]: the table sits at 0x004b1858 and spans
    // 0x100 bytes, so 0x004b1958 - g_JoyWarnPrinted - is exactly one past its
    // end, and the walk covers indices 31..1 of the live table.
    //
    // The port cannot address it that way. g_JoyWarnPrinted is
    // zero-initialised so the linker puts it in .bss, while g_JoyRemapTbl has
    // non-zero initialisers and lands in .data (dumpbin: SECT4 vs SECT5), so
    // the walk-down read 32 unrelated globals instead of the table and
    // whatever happened to equal a function value decided what this screen
    // showed. Index the table directly - see docs/MEMORY_LAYOUT.md on
    // past-the-end addressing, and docs/GAMEPAD_INPUT.md.
    // This site never read index 32 (it decrements before dereferencing), so
    // it is a pure wrong-base bug: the original scans 31..0 for 0x800.
    for (int idx = 31; idx >= 0; idx--) {
        if ((int)g_JoyRemapTbl[1][idx] == 0x800) {
            l1ButtonMask = 1 << (idx & 0x1f);
            break;
        }
    }

    // Set up render state
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.clutY = 0x1ff;
    g_TextureDesc.texturePage = 0x15;
    g_TextureDesc.clutX = 0;
    g_TextureDesc.colorMulR = 0;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.colorMulG = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.colorMulB = 0;
    unk_00be1180 = 0;

    if (s_optSubInitState == 0) {
        // Initialize sub-menu state
        s_optPrevButtons = 0;
        s_optSubSubState = 0;
        s_optSubInitState = 1;

        // Clear cursor highlights
        unsigned int* pHL = (unsigned int*)&s_optCursorHighlight[0];
        for (i = 0xb; i != 0; i--) {
            *pHL = 0;
            pHL++;
        }

        // Clear temp key entries
        unsigned char* pTmp = (unsigned char*)&s_optTempEntries[0];
        do {
            pTmp[0] = 0x5f;   // '_'
            pTmp[1] = 0xff;
            pTmp[2] = 0xff;
            *(int*)(pTmp + 4) = 0;
            *(int*)(pTmp + 8) = 0;
            *(int*)(pTmp + 0x10) = 0;
            pTmp += 0x14;
        } while (pTmp < (unsigned char*)&s_optTempEntries[9]);

        // Set up player entity for display
        ENTITY = (Entity*)&g_playerEntity;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.attackAnim = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 1;
        if (g_playerEntity.id == 3) {
            g_playerEntity.id = 1;
        }
        g_playerEntity.scaMatrixData.localMatrix.t[0] = 0x960;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = 0x834;
        g_playerEntity.scaMatrixData.localMatrix.t[2] = -700;

        s_optAnimFrameData = 0;
        s_optIdleAnimTrigger = 0;
        s_optWalkAnimTrigger = 0;
        s_optTempFlags = 0;
        s_optUnknown424 = 0;
        s_optAnimFrameCounter = 0;
    }
    else if (s_optSubInitState != 1) {
        return 0;
    }

    // Read sidewinder pad and process input
    int padResult = read_sidewinder_pad();
    if ((l1ButtonMask != padResult) && (options_display_config_input() != 0)) {
        return 0;
    }

    // Copy temp entries back to key binding data
    int* pBind = &s_optTempEntries[0].keyIndex;
    do {
        unsigned char* pVk = (unsigned char*)(pBind - 1);
        int idx = *pBind;
        pBind += 5; // 0x14 / 4 = 5 ints per entry
        g_keyBindingData[idx] = *pVk;
    } while (pBind < &s_optTempEntries[9].keyIndex);

    s_optAcceptKeyVK = s_optTempEntries[0].vkCode;
    s_optCancelKeyVK = s_optTempEntries[1].vkCode;
    InitInputKeyBindings();

    // Clear temp entries again
    unsigned char* pTmp = (unsigned char*)&s_optTempEntries[0];
    do {
        pTmp[0] = 0x5f;
        pTmp[1] = 0xff;
        pTmp[2] = 0xff;
        *(int*)(pTmp + 4) = 0;
        *(int*)(pTmp + 8) = 0;
        *(int*)(pTmp + 0xc) = 0;
        pTmp += 0x14;
    } while (pTmp < (unsigned char*)&s_optTempEntries[9]);

    // Reset state
    *(int*)(((unsigned char*)&s_optTempEntries[0]) + 0x108) = 0; // 0x00bcb408
    *(int*)(((unsigned char*)&s_optTempEntries[0]) + 0x10c) = 0; // 0x00bcb40c
    s_optCursorIndex = 0;
    s_optAnimFrameData = 0;
    s_optIdleAnimTrigger = 0;
    s_optWalkAnimTrigger = 0;
    s_optTempFlags = 0;
    s_optUnknown424 = 0;
    s_optAnimFrameCounter = 0;
    options_init_keybind_display();
    return 1;
}


// ============================================================================
// options_display_config_input (0x00451b80)
// Display config input handler - 4-state machine.
// State 0: Populate temp entries from key bindings
// State 1: Navigation (up/down/select)
// State 2: Key capture (waiting for new key press)
// State 3: Debounce delay
// Returns 0 to keep running, non-zero to exit.
// ============================================================================
unsigned int options_display_config_input(void)
{
    int i, j;
    bool found;
    unsigned char scanResult;

    switch (s_optSubSubState) {
    case 0: {
        // Populate temp entries from g_keyBindingData + g_JoyRemapTbl[0]
        s_optCursorIndex = 0;
        s_optCursorHighlight[0] = 1;
        const unsigned int* remapTbl = (const unsigned int*)(g_JoyRemapTbl[0] + 27);
        i = 0x1b;
        do {
            unsigned int btnMask = *remapTbl;
            if (btnMask == 0x80) {
                s_optTempEntries[0].vkCode = g_keyBindingData[i];
                s_optTempEntries[0].keyIndex = i;
            }
            else if (btnMask == 0x40) {
                s_optTempEntries[1].vkCode = g_keyBindingData[i];
                s_optTempEntries[1].keyIndex = i;
            }
            else if (btnMask == 8) {
                s_optTempEntries[2].vkCode = g_keyBindingData[i];
                s_optTempEntries[2].keyIndex = i;
            }
            else if (btnMask == 0x1000) {
                s_optTempEntries[3].vkCode = g_keyBindingData[i];
                s_optTempEntries[3].keyIndex = i;
            }
            else if (btnMask == 0x4000) {
                s_optTempEntries[4].vkCode = g_keyBindingData[i];
                s_optTempEntries[4].keyIndex = i;
            }
            else if (btnMask == 0x2000) {
                s_optTempEntries[5].vkCode = g_keyBindingData[i];
                s_optTempEntries[5].keyIndex = i;
            }
            else if (btnMask == 0x8000) {
                s_optTempEntries[6].vkCode = g_keyBindingData[i];
                s_optTempEntries[6].keyIndex = i;
            }
            else if (btnMask == 0x800) {
                s_optTempEntries[7].vkCode = g_keyBindingData[i];
                s_optTempEntries[7].keyIndex = i;
            }
            else if (btnMask == 0x900) {
                s_optTempEntries[8].vkCode = g_keyBindingData[i];
                s_optTempEntries[8].keyIndex = i;
            }
            remapTbl--;
            i--;
        } while (i >= 0);
        s_optPrevKeyScan = FUN_00497de0();
        s_optSubSubState = 1;
        break;
    }

    case 1: {
        // Navigation - checks button byte (bits 8-15) of pad state
        // Ghidra: g_PlayerDpadPressed._1_1_ (remapped) for cross/square
        //         g_PlayerPadPressed._1_1_ (raw) for triangle/circle
        unsigned short dpadByte = (unsigned short)(g_PlayerDpadPressed >> 8);
        unsigned short padByte = (unsigned short)(g_PlayerPadPressed >> 8);

        if ((dpadByte & 0x40) != 0) {
            // Cross (remapped) → enter key capture mode
            play_sfx(3, 6, 0);
            s_optCursorHighlight[s_optCursorIndex] = 2;
            s_optSubSubState = 2;
            s_optPrevKeyScan = FUN_00497de0();
        }
        else if ((dpadByte & 0x80) != 0) {
            // Square (remapped) → exit handler
            play_sfx(3, 5, 0);
            return 0;
        }
        else if ((padByte & 0x80) != 0) {
            // Square (raw) → back to EXIT slot (0x00451c62)
            play_sfx(3, 4, 0);
            s_optCursorPos = 2;
            return 0;
        }
        else if ((padByte & 0x20) != 0) {
            // Circle (raw) → back to JOY PAD slot (0x00451c76)
            play_sfx(3, 4, 0);
            s_optCursorPos = 0;
            return 0;
        }
        else if ((padByte & 0x10) != 0) {
            // Triangle (raw) → cursor UP
            play_sfx(3, 4, 0);
            s_optCursorHighlight[s_optCursorIndex] = 0;
            s_optCursorIndex--;
            if (s_optCursorIndex < 0) s_optCursorIndex = 8;
            s_optAnimFrameCounter = 0;
            s_optCursorHighlight[s_optCursorIndex] = 1;
            ((unsigned char*)&g_playerEntity)[0xbe] = 0;
        }
        else if ((padByte & 0x40) != 0) {
            // Cross (raw) → cursor DOWN
            play_sfx(3, 4, 0);
            s_optCursorHighlight[s_optCursorIndex] = 0;
            s_optCursorIndex++;
            if (s_optCursorIndex > 8) s_optCursorIndex = 0;
            s_optAnimFrameCounter = 0;
            s_optCursorHighlight[s_optCursorIndex] = 1;
            ((unsigned char*)&g_playerEntity)[0xbe] = 0;
        }
        break;
    }

    case 2: {
        // Key capture
        s_optJoyButtonScan = read_sidewinder_pad() & JOY_SCAN_BUTTON_MASK;
        if (s_optAcceptButtonMask == s_optJoyButtonScan) {
            // Cancel - same button pressed
            play_sfx(3, 5, 0);
            s_optSubSubState = 1;
            s_optCursorHighlight[s_optCursorIndex] = 1;
        }
        else {
            scanResult = FUN_00497de0();
            s_optKeyScanResult = scanResult;
            if (scanResult != s_optPrevKeyScan) {
                if (scanResult == 0) {
                    s_optPrevKeyScan = 0;
                }
                else {
                    s_optPrevKeyScan = scanResult;
                    // Check if this is a valid key (not ESC/ENTER/SPACE/DELETE)
                    switch ((unsigned int)scanResult) {
                    case 0xd:  // ENTER
                    case 0x11: // ESC
                    case 0x1b: // ESCAPE
                    case 0x20: // SPACE
                        s_optSubSubState = 1;
                        break;

                    case 0x25: case 0x26: case 0x27: case 0x28: // Arrow keys
                    case 0x30: case 0x31: case 0x32: case 0x33: // 0-3
                    case 0x34: case 0x35: case 0x36: case 0x37: // 4-7
                    case 0x38: case 0x39: // 8-9
                    case 0x41: case 0x42: case 0x43: case 0x44: // A-D
                    case 0x45: case 0x46: case 0x47: case 0x48: // E-H
                    case 0x49: case 0x4a: case 0x4b: case 0x4c: // I-L
                    case 0x4d: case 0x4e: case 0x4f: case 0x50: // M-P
                    case 0x51: case 0x52: case 0x53: case 0x54: // Q-T
                    case 0x55: case 0x56: case 0x57: case 0x58: // U-X
                    case 0x59: case 0x5a: // Y-Z
                    case 0x60: case 0x61: case 0x62: // Numpad 0-2
                    case 0x63: case 0x64: case 0x65: // Numpad 3-5
                    case 0x66: case 0x67: case 0x68: // Numpad 6-8
                    case 0x69: case 0x6e: // Nump9, NumpDot
                    case 0xba: case 0xbb: case 0xbc: // ; = ,
                    case 0xbd: case 0xbe: case 0xbf: // - . /
                    case 0xc0: case 0xdb: case 0xdc: // ` [ \
                    case 0xdd: case 0xde: case 0xe2: // ] ' extra
                    {
                        // Check for duplicate key - swap if found
                        found = false;
                        j = 0;
                        unsigned char* pCheck = (unsigned char*)&s_optTempEntries[0];
                        do {
                            if ((int)*pCheck == (unsigned int)scanResult) {
                                // Swap the keys
                                int srcOff = j * 0x14;
                                int dstOff = s_optCursorIndex * 0x14;
                                unsigned char* pSrc = (unsigned char*)&s_optTempEntries[0] + srcOff;
                                unsigned char* pDst = (unsigned char*)&s_optTempEntries[0] + dstOff;

                                // Move old binding to the duplicate entry and
                                // assign the new key to the current entry (0x00451f7a)
                                pSrc[0] = pDst[0];
                                pDst[0] = scanResult;

                                // Set anim state for both
                                ((int*)&s_optTempEntries[0])[s_optCursorIndex * 5 + 2] = 1;
                                ((int*)&s_optTempEntries[0])[j * 5 + 2] = 1;

                                // Reset display chars
                                pSrc[1] = 0xff;
                                pSrc[2] = 0xff;
                                pDst[1] = 0xff;
                                pDst[2] = 0xff;

                                // Update global VK references
                                s_optAcceptKeyVK = s_optTempEntries[0].vkCode;
                                s_optCancelKeyVK = s_optTempEntries[1].vkCode;

                                // Update the key binding for the swapped entry
                                g_keyBindingData[s_optTempEntries[j].keyIndex] = pSrc[0];
                                InitInputKeyBindings();
                                found = true;
                                break;
                            }
                            pCheck += 0x14;
                            j++;
                        } while (pCheck < (unsigned char*)&s_optTempEntries[9]);

                        i = s_optCursorIndex;
                        if (!found) {
                            // No duplicate - assign new key
                            ((unsigned char*)&s_optTempEntries[0])[s_optCursorIndex * 0x14] = scanResult;
                            int off = i * 0x14;
                            ((int*)&s_optTempEntries[0])[i * 5 + 2] = 1;
                            ((unsigned char*)&s_optTempEntries[0])[off + 1] = 0xff;
                            ((unsigned char*)&s_optTempEntries[0])[off + 2] = 0xff;
                            s_optAcceptKeyVK = s_optTempEntries[0].vkCode;
                            s_optCancelKeyVK = s_optTempEntries[1].vkCode;
                            g_keyBindingData[s_optTempEntries[i].keyIndex] = ((unsigned char*)&s_optTempEntries[0])[off];
                            InitInputKeyBindings();
                        }
                        s_optAnimFrameCounter = 0;
                        ((unsigned char*)&g_playerEntity)[0xbe] = 0;
                        s_optSubSubState = 3;
                        s_optDebounceTimer = 2;
                        s_optCursorHighlight[s_optCursorIndex] = 1;
                        break;
                    }
                    }
                }
            }
        }
        break;
    }

    case 3:
        // Debounce delay
        if (s_optDebounceTimer == 0) {
            s_optSubSubState = 1;
        }
        s_optDebounceTimer--;
        break;
    }

    // Render the display config labels with cursor highlights
    i = 0;
    unsigned char* pEntry = (unsigned char*)&s_optTempEntries[0].symbolChar;
    do {
        int hlState = s_optCursorHighlight[i / 4];
        int animState = *(int*)(pEntry + 6); // animState field
        if (hlState <= animState) hlState = animState;

        options_map_vk_to_controller_symbol(pEntry - 2);
        if ((*pEntry == 0xff) || ((short)*pEntry == 0xbc) || ((short)*pEntry == 0xbf)) {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)pEntry[-2]);
            PrintText8x14((short)(s_optDisplayXOffset + 0x91),
                s_optDisplayLabelY[i / 4] + s_optDisplayYOffset + 1, hlState, 0);
        }
        else {
            sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(short)*pEntry);
            options_print_14x14((short)(s_optDisplayXOffset + 0x8e),
                (short)(s_optDisplayLabelY[i / 4] + s_optDisplayYOffset + 1), (unsigned char)hlState, 0);
        }
        i += 4;
        pEntry += 0x14;
    } while (pEntry < ((unsigned char*)&s_optTempEntries[9]) + 2);

    // Print action labels
    PrintFormattedText(0xa2, s_optDisplayLabelY[0], s_optCursorHighlight[0], s_optLabelText_Action);
    PrintFormattedText(0xa2, s_optDisplayLabelY[1], s_optCursorHighlight[1], s_optLabelText_Cancel);
    PrintFormattedText(0xa2, s_optDisplayLabelY[2], s_optCursorHighlight[2], s_optLabelText_Start);
    PrintFormattedText(0xa2, s_optDisplayLabelY[3], s_optCursorHighlight[3], s_optLabelText_LRotate);
    PrintFormattedText(0xa2, s_optDisplayLabelY[4], s_optCursorHighlight[4], s_optLabelText_RRotate);
    PrintFormattedText(0xa2, s_optDisplayLabelY[5], s_optCursorHighlight[5], s_optLabelText_Run);
    PrintFormattedText(0xa2, s_optDisplayLabelY[6], s_optCursorHighlight[6], s_optLabelText_Aim);
    PrintFormattedText(0xa2, s_optDisplayLabelY[7], s_optCursorHighlight[7], s_optLabelText_QuickTurn);
    PrintFormattedText(0xa2, s_optDisplayLabelY[8], s_optCursorHighlight[8], s_optLabelText_Map);

    // Render special key display (ACTION accept key)
    options_map_vk_to_font_index((unsigned char*)&s_optTempEntries[0]);
    if (s_optTempEntries[0].displayChar == 0xff) {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)s_optTempEntries[0].vkCode);
        i = s_optTempEntries[0].animState;
        if (s_optTempEntries[0].animState <= s_optCursorHighlight[0]) i = s_optCursorHighlight[0];
        PrintText8x8(0x130, (short)(s_optDisplayLabelY[0] + 6), (unsigned char)i, '\0');
    }
    else {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_optTempEntries[0].displayChar);
        i = s_optTempEntries[0].animState;
        if (s_optTempEntries[0].animState <= s_optCursorHighlight[0]) i = s_optCursorHighlight[0];
        options_print_8x8_glyph(0x130, s_optDisplayLabelY[0] + 6, (unsigned char)i, 0);
    }

    // CANCEL key
    options_map_vk_to_font_index((unsigned char*)&s_optTempEntries[1]);
    if (s_optTempEntries[1].displayChar == 0xff) {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)s_optTempEntries[1].vkCode);
        i = s_optTempEntries[1].animState;
        if (s_optTempEntries[1].animState <= s_optCursorHighlight[1]) i = s_optCursorHighlight[1];
        PrintText8x8(0x120, (short)(s_optDisplayLabelY[1] + 6), (unsigned char)i, '\0');
    }
    else {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_optTempEntries[1].displayChar);
        i = s_optTempEntries[1].animState;
        if (s_optTempEntries[1].animState <= s_optCursorHighlight[1]) i = s_optCursorHighlight[1];
        options_print_8x8_glyph(0x120, s_optDisplayLabelY[1] + 6, (unsigned char)i, 0);
    }

    // START key (multi-column)
    options_map_vk_to_font_index((unsigned char*)&s_optTempEntries[2]);
    if (s_optTempEntries[2].displayChar == 0xff) {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)s_optTempEntries[2].vkCode);
        i = s_optTempEntries[2].animState;
        if (s_optTempEntries[2].animState <= s_optCursorHighlight[0]) i = s_optCursorHighlight[0];
        PrintText8x8(0x120, (short)(s_optDisplayLabelY[0] + 6), (unsigned char)i, '\0');
        i = s_optTempEntries[2].animState;
        if (s_optTempEntries[2].animState <= s_optCursorHighlight[3]) i = s_optCursorHighlight[3];
        PrintText8x8(0x120, (short)(s_optDisplayLabelY[3] + 6), (unsigned char)i, '\0');
        i = s_optTempEntries[2].animState;
        if (s_optTempEntries[2].animState <= s_optCursorHighlight[4]) i = s_optCursorHighlight[4];
        PrintText8x8(0x120, (short)(s_optDisplayLabelY[4] + 6), (unsigned char)i, '\0');
    }
    else {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_optTempEntries[2].displayChar);
        i = s_optTempEntries[2].animState;
        if (s_optTempEntries[2].animState <= s_optCursorHighlight[0]) i = s_optCursorHighlight[0];
        options_print_8x8_glyph(0x120, s_optDisplayLabelY[0] + 6, (unsigned char)i, 0);
        i = s_optTempEntries[2].animState;
        if (s_optTempEntries[2].animState <= s_optCursorHighlight[3]) i = s_optCursorHighlight[3];
        options_print_8x8_glyph(0x120, s_optDisplayLabelY[3] + 6, (unsigned char)i, 0);
        i = s_optTempEntries[2].animState;
        if (s_optTempEntries[2].animState <= s_optCursorHighlight[4]) i = s_optCursorHighlight[4];
        options_print_8x8_glyph(0x120, s_optDisplayLabelY[4] + 6, (unsigned char)i, 0);
    }

    // L1 key (multi-column)
    options_map_vk_to_font_index((unsigned char*)&s_optTempEntries[3]);
    if (s_optTempEntries[3].displayChar == 0xff) {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)s_optTempEntries[3].vkCode);
        i = s_optTempEntries[3].animState;
        if (s_optTempEntries[3].animState <= s_optCursorHighlight[1]) i = s_optCursorHighlight[1];
        PrintText8x8(0x130, (short)(s_optDisplayLabelY[1] + 6), (unsigned char)i, '\0');
        i = s_optTempEntries[3].animState;
        if (s_optTempEntries[3].animState <= s_optCursorHighlight[3]) i = s_optCursorHighlight[3];
        PrintText8x8(0x130, (short)(s_optDisplayLabelY[3] + 6), (unsigned char)i, '\0');
    }
    else {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_optTempEntries[3].displayChar);
        i = s_optTempEntries[3].animState;
        if (s_optTempEntries[3].animState <= s_optCursorHighlight[1]) i = s_optCursorHighlight[1];
        options_print_8x8_glyph(0x130, s_optDisplayLabelY[1] + 6, (unsigned char)i, 0);
        i = s_optTempEntries[3].animState;
        if (s_optTempEntries[3].animState <= s_optCursorHighlight[3]) i = s_optCursorHighlight[3];
        options_print_8x8_glyph(0x130, s_optDisplayLabelY[3] + 6, (unsigned char)i, 0);
    }

    // R1 key
    options_map_vk_to_font_index((unsigned char*)&s_optTempEntries[4]);
    if (s_optTempEntries[4].displayChar == 0xff) {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)s_optTempEntries[4].vkCode);
        i = s_optTempEntries[4].animState;
        if (s_optTempEntries[4].animState <= s_optCursorHighlight[4]) i = s_optCursorHighlight[4];
        PrintText8x8(0x130, (short)(s_optDisplayLabelY[4] + 6), (unsigned char)i, '\0');
    }
    else {
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)s_optTempEntries[4].displayChar);
        i = s_optTempEntries[4].animState;
        if (s_optTempEntries[4].animState <= s_optCursorHighlight[4]) i = s_optCursorHighlight[4];
        options_print_8x8_glyph(0x130, s_optDisplayLabelY[4] + 6, (unsigned char)i, 0);
    }

    // Empty label separators
    sprintf(PRINT_TEXT_BUFFER, s_fmt_plus);
    PrintText8x8(0x128, (short)(s_optDisplayLabelY[0] + 6), 0, '\0');
    PrintText8x8(0x128, (short)(s_optDisplayLabelY[1] + 6), 0, '\0');
    PrintText8x8(0x128, (short)(s_optDisplayLabelY[3] + 6), 0, '\0');
    PrintText8x8(0x128, (short)(s_optDisplayLabelY[4] + 6), 0, '\0');

    // Player model animation based on cursor position
    if (s_optAnimSkipFlag == 0) {
        if ((*(char*)(((unsigned char*)ENTITY) + 0xbe) == 0) && (*(char*)(((unsigned char*)ENTITY) + 0xbf) == 1)) {
            // Advance the demo script for the highlighted option row
            unsigned char charId = *(unsigned char*)(((unsigned char*)ENTITY) + 1);
            const int* script = PTR_PTR_004c0470[charId][s_optCursorIndex];
            if (script != NULL) {  // row 9 is the table terminator
                s_optAnimFrameData = script[s_optAnimFrameCounter + 1];
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) =
                    (unsigned char)script[s_optAnimFrameCounter];
                s_optAnimFrameCounter += 2;
                if (PTR_DAT_004c0500[charId][s_optCursorIndex] <= s_optAnimFrameCounter) {
                    s_optAnimFrameCounter = 0;
                }
            }
        }

        // Handle special animation triggers based on cursor position
        switch (s_optCursorIndex) {
        case 0:
        case 2:
            break;
        case 1:
            if ((*(char*)(((unsigned char*)ENTITY) + 0xbd) == 3) && (s_optAnimFrameCounter > 5) &&
                (*(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) > 7)) {
                *(char*)(((unsigned char*)ENTITY) + 0xbd) = 2;
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) = 0x18;
                *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
            }
            if ((s_optAnimFrameData == 1) &&
                ((((*(char*)(((unsigned char*)ENTITY) + 1) == 0) && (*(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) > 0x1f)) ||
                  ((*(char*)(((unsigned char*)ENTITY) + 1) == 1) && (*(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) > 0x12))) &&
                 (*(char*)(((unsigned char*)ENTITY) + 0xbd) == 0))) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
            }
            break;
        case 3:
            if (*(char*)(((unsigned char*)ENTITY) + 0xbd) == 7) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 10;
                if (*(char*)(((unsigned char*)ENTITY) + 1) == 0) {
                    if (s_optAnimFrameCounter == 0xe) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                    if (s_optAnimFrameCounter == 0x34) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 7;
                }
                if (*(char*)(((unsigned char*)ENTITY) + 1) == 1) {
                    if (s_optAnimFrameCounter == 0x10) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                    if (s_optAnimFrameCounter == 0x36) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 7;
                }
                if (*(char*)(((unsigned char*)ENTITY) + 1) == 3) {
                    if (s_optAnimFrameCounter == 0x10) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                    if (s_optAnimFrameCounter == 0x36) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 7;
                }
            }
            break;
        case 4:
            if (*(char*)(((unsigned char*)ENTITY) + 0xbd) == 7) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0xd;
                if (*(char*)(((unsigned char*)ENTITY) + 1) == 0) {
                    if (s_optAnimFrameCounter == 0x10) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                    if (s_optAnimFrameCounter == 0x36) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 7;
                }
                if (*(char*)(((unsigned char*)ENTITY) + 1) == 1) {
                    if (s_optAnimFrameCounter == 0x10) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                    if (s_optAnimFrameCounter == 0x36) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 7;
                }
                if (*(char*)(((unsigned char*)ENTITY) + 1) == 3) {
                    if (s_optAnimFrameCounter == 0x10) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                    if (s_optAnimFrameCounter == 0x36) *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 7;
                }
            }
            // Fall through to joint move
            goto doJointMove;
        case 5:
            if (*(char*)(((unsigned char*)ENTITY) + 0xbd) == 2) {
                unsigned short* pAngle = (unsigned short*)(((unsigned char*)ENTITY) + 0x74);
                if (*pAngle == 0xca4) {
                    *pAngle = 0xce4;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) = 0;
                    s_optAnimFrameData = 0;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                }
                else {
                    *pAngle = (*pAngle + 0x60) & 0xfff;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 2;
                }
            }
            break;
        case 6:
            if (*(char*)(((unsigned char*)ENTITY) + 0xbd) == 2) {
                if (*(short*)(((unsigned char*)ENTITY) + 0x74) == 0xd24) {
                    *(unsigned short*)(((unsigned char*)ENTITY) + 0x74) = 0xce4;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) = 0;
                    s_optAnimFrameData = 0;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
                }
                else {
                    *(unsigned short*)(((unsigned char*)ENTITY) + 0x74) = (*(short*)(((unsigned char*)ENTITY) + 0x74) - 0x60) & 0xfff;
                    *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 2;
                }
            }
            break;
        case 7:
            if (s_optWalkAnimTrigger == 0) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0;
                s_optWalkAnimTrigger = 1;
            }
            break;
        case 8:
            if (s_optIdleAnimTrigger == 0) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0;
                s_optIdleAnimTrigger = 1;
            }
            break;
        default:
            goto updateRender;
        }

doJointMove:
        Joint_move(s_optAnimFrameData, g_playerEntity.jointMoveData0,
            g_playerEntity.jointMoveData1, 0x400);
    }

updateRender:
    EntityComputeJointWorldMatrices(g_playerEntity.unk_ca);
    EntityApplyLookAtRotation();
    options_render_entity((int)&g_playerEntity);
    return 1;
}


// ============================================================================
// options_key_config_handler (0x00452c50)
// Key config sub-menu handler.
// Returns 0 to keep running, non-zero to exit.
// ============================================================================
unsigned int options_key_config_handler(void)
{
    unsigned char* pTmp;

    // Set up render state
    g_TextureDesc.clutX = 0;
    g_TextureDesc.colorMulR = 0;
    g_TextureDesc.colorMulG = 0;
    g_TextureDesc.colorMulB = 0;
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.clutY = 0x1ff;
    g_TextureDesc.texturePage = 0x15;

    if (s_optSubInitState == 0) {
        s_optPrevButtons = 0;
        s_optSubSubState = 0;
        s_optSubInitState = 1;

        // Clear cursor highlights
        unsigned int* pHL = (unsigned int*)&s_optCursorHighlight[0];
        for (int i = 0xb; i != 0; i--) {
            *pHL = 0;
            pHL++;
        }

        // Clear temp entries (5 entries for joystick config)
        pTmp = (unsigned char*)&s_optTempEntries[0];
        do {
            pTmp[0] = 0x5f;
            pTmp[1] = 0xff;
            pTmp[2] = 0xff;
            *(int*)(pTmp + 4) = -1;
            *(int*)(pTmp + 8) = 0;
            *(int*)(pTmp + 0xc) = 0;
            pTmp += 0x14;
        } while (pTmp < (unsigned char*)&s_optTempEntries[5]);

        // Set up player entity
        ENTITY = (Entity*)&g_playerEntity;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.attackAnim = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.scaMatrixData.localMatrix.t[0] = 0x960;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = 0x834;
        g_playerEntity.scaMatrixData.localMatrix.t[2] = -700;
    }
    else if (s_optSubInitState != 1) {
        return 0;
    }

    // Process input (unless ESC/ENTER was pressed)
    if ((s_optKeyScanResult != 0x11) && (s_optKeyScanResult != 0x1b)) {
        char result = options_key_config_input();
        if (result != 0) return 0;
    }

    // Clear temp entries
    pTmp = (unsigned char*)&s_optTempEntries[0];
    do {
        pTmp[0] = 0x5f;
        pTmp[1] = 0xff;
        pTmp[2] = 0xff;
        *(int*)(pTmp + 4) = 0;
        *(int*)(pTmp + 8) = 0;
        *(int*)(pTmp + 0xc) = 0;
        pTmp += 0x14;
    } while (pTmp < (unsigned char*)&s_optTempEntries[5]);

    // Reset state
    *(int*)(((unsigned char*)&s_optTempEntries[0]) + 0x108) = 0;
    *(int*)(((unsigned char*)&s_optTempEntries[0]) + 0x10c) = 0;
    s_optCursorIndex = 0;
    s_optAnimFrameData = 0;
    s_optIdleAnimTrigger = 0;
    s_optWalkAnimTrigger = 0;
    s_optTempFlags = 0;
    s_optUnknown424 = 0;
    s_optAnimFrameCounter = 0;
    options_init_keybind_display();
    return 1;
}


// ============================================================================
// options_key_config_input (0x00452de0)
// Key config input handler - joystick button remapping with swap logic.
// Returns 0 to keep running, non-zero to exit.
// ============================================================================
unsigned int options_key_config_input(void)
{
    int i, j;
    bool found;

    if (s_optSubSubState == 0) {
        // Populate temp entries from g_JoyRemapTbl[1]
        s_optCursorIndex = 0;
        s_optCursorHighlight[0] = 2;
    // The original walks DOWN from &g_JoyWarnPrinted, which in the ORIGINAL
    // binary IS g_JoyRemapTbl[1][32]: the table sits at 0x004b1858 and spans
    // 0x100 bytes, so 0x004b1958 - g_JoyWarnPrinted - is exactly one past its
    // end, and the walk covers indices 31..1 of the live table.
    //
    // The port cannot address it that way. g_JoyWarnPrinted is
    // zero-initialised so the linker puts it in .bss, while g_JoyRemapTbl has
    // non-zero initialisers and lands in .data (dumpbin: SECT4 vs SECT5), so
    // the walk-down read 32 unrelated globals instead of the table and
    // whatever happened to equal a function value decided what this screen
    // showed. Index the table directly - see docs/MEMORY_LAYOUT.md on
    // past-the-end addressing, and docs/GAMEPAD_INPUT.md.
        //
        // The dropped index-32 iteration only ever read the warning flag.
        for (i = 31; i > 0; i--) {
            int joyVal = (int)g_JoyRemapTbl[1][i];
            if (joyVal == 0x80) {
                s_optTempEntries[0].pad2 = 0x80;
                s_optTempEntries[0].keyIndex = i;
            }
            else if (joyVal == 0x40) {
                s_optTempEntries[1].pad2 = 0x40;
                s_optTempEntries[1].keyIndex = i;
            }
            else if (joyVal == 8) {
                s_optTempEntries[2].pad2 = 8;
                s_optTempEntries[2].keyIndex = i;
            }
            else if (joyVal == 0x800) {
                s_optTempEntries[3].pad2 = 0x800;
                s_optTempEntries[3].keyIndex = i;
            }
            else if (joyVal == 0x900) {
                s_optTempEntries[4].pad2 = 0x900;
                s_optTempEntries[4].keyIndex = i;
            }
        }
        s_optSubSubState = 1;
    }
    else if (s_optSubSubState == 1) {
        // Navigation - button byte (bits 8-15) of g_PlayerPadPressed
        // Ghidra: g_PlayerPadPressed._1_1_ byte checks
        unsigned short padByte = (unsigned short)(g_PlayerPadPressed >> 8);
        if ((padByte & 0x80) != 0) {
            // Square → exit to joystick tab
            play_sfx(3, 4, 0);
            s_optCursorPos = 2;
            return 0;
        }
        else if ((padByte & 0x20) != 0) {
            // Circle → exit to key config tab
            play_sfx(3, 4, 0);
            s_optCursorPos = 0;
            return 0;
        }
        else if ((padByte & 0x10) != 0) {
            // Triangle → cursor UP
            play_sfx(3, 4, 0);
            s_optCursorHighlight[s_optCursorIndex] = 0;
            s_optCursorIndex--;
            if (s_optCursorIndex < 0) s_optCursorIndex = 4;
            s_optAnimFrameCounter = 0;
            s_optCursorHighlight[s_optCursorIndex] = 2;
            ((unsigned char*)&g_playerEntity)[0xbe] = 0;
        }
        else if ((padByte & 0x40) != 0) {
            // Cross → cursor DOWN
            play_sfx(3, 4, 0);
            s_optCursorHighlight[s_optCursorIndex] = 0;
            s_optCursorIndex++;
            if (s_optCursorIndex > 4) s_optCursorIndex = 0;
            s_optAnimFrameCounter = 0;
            s_optCursorHighlight[s_optCursorIndex] = 2;
            ((unsigned char*)&g_playerEntity)[0xbe] = 0;
        }
        else {
            // Check for keyboard cancel
            s_optKeyScanResult = FUN_00497de0();
            if ((s_optKeyScanResult == 0x11) || (s_optKeyScanResult == 0x1b) ||
                (s_optCancelKeyVK == s_optKeyScanResult)) {
                play_sfx(3, 5, 0);
                s_optCursorHighlight[s_optCursorIndex] = 1;
                s_optSubSubState = 1;
            }
            else {
                // Check for joystick button press
                s_optJoyButtonScan = read_sidewinder_pad() & JOY_SCAN_BUTTON_MASK;
                i = s_optCursorIndex;
                if (s_optPrevJoyButtonScan != s_optJoyButtonScan) {
                    if (s_optJoyButtonScan == 0) {
                        s_optPrevJoyButtonScan = 0;
                    }
                    else {
                        // Find which bit is set. The original stops before
                        // bit 31 (button 24); the scan word has room for it and
                        // g_JoyRemapTbl[1] has an entry for it, so include it.
                        bool validBit = false;
                        int bitIdx = 4;
                        do {
                            if (1 << ((unsigned char)bitIdx & 0x1f) == s_optJoyButtonScan) {
                                validBit = true;
                                break;
                            }
                            bitIdx++;
                        } while (bitIdx < 0x20);

                        bool swapped = false;
                        s_optPrevJoyButtonScan = s_optJoyButtonScan;

                        if (validBit) {
                            // Check for existing assignment to swap
                            int checkIdx = 0;
                            unsigned char* pCheck = (unsigned char*)&s_optTempEntries[0].keyIndex;
                            do {
                                if (1 << (*pCheck & 0x1f) == s_optJoyButtonScan) {
                                    // Swap buttons
                                    int tmp = s_optTempEntries[checkIdx].keyIndex;
                                    swapped = true;
                                    s_optSwappedButtonVal = tmp;
                                    s_optTempEntries[checkIdx].keyIndex = s_optTempEntries[i].keyIndex;
                                    s_optTempEntries[i].keyIndex = tmp;
                                    s_optTempEntries[i].animState = 1;
                                    s_optTempEntries[checkIdx].animState = 1;

                                    // Update remap table for swapped entry
                                    switch (checkIdx) {
                                    case 0: ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[checkIdx].keyIndex] = 0x80; break;
                                    case 1:
                                        s_optAcceptButtonMask = 1 << ((unsigned char)s_optTempEntries[checkIdx].keyIndex & 0x1f);
                                        ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[checkIdx].keyIndex] = 0x40;
                                        break;
                                    case 2: ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[checkIdx].keyIndex] = 8; break;
                                    case 3: ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[checkIdx].keyIndex] = 0x800; break;
                                    case 4: ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[checkIdx].keyIndex] = 0x900; break;
                                    }
                                    break;
                                }
                                pCheck += 0x14;
                                checkIdx++;
                            } while (pCheck < (unsigned char*)&s_optTempEntries[5].keyIndex + 4);

                            // Find bit position of new button
                            for (j = 0; 1 << ((unsigned char)j & 0x1f) != s_optJoyButtonScan; j++) {}

                            // Update remap table for current entry
                            switch (s_optCursorIndex) {
                            case 0:
                                if (!swapped) ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[0].keyIndex] = 0;
                                ((unsigned int*)g_JoyRemapTbl[1])[j] = 0x80;
                                s_optTempEntries[0].keyIndex = j;
                                break;
                            case 1:
                                if (!swapped) ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[1].keyIndex] = 0;
                                s_optAcceptButtonMask = s_optJoyButtonScan;
                                s_optTempEntries[1].keyIndex = j;
                                ((unsigned int*)g_JoyRemapTbl[1])[j] = 0x40;
                                break;
                            case 2:
                                if (!swapped) ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[2].keyIndex] = 0;
                                ((unsigned int*)g_JoyRemapTbl[1])[j] = 8;
                                s_optTempEntries[2].keyIndex = j;
                                break;
                            case 3:
                                if (!swapped) ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[3].keyIndex] = 0;
                                ((unsigned int*)g_JoyRemapTbl[1])[j] = 0x800;
                                s_optTempEntries[3].keyIndex = j;
                                break;
                            case 4:
                                if (!swapped) ((unsigned int*)g_JoyRemapTbl[1])[s_optTempEntries[4].keyIndex] = 0;
                                ((unsigned int*)g_JoyRemapTbl[1])[j] = 0x900;
                                s_optTempEntries[4].keyIndex = j;
                                break;
                            }

                            options_init_keybind_display();
                            i = s_optCursorIndex;

                            if (!swapped) {
                                // Calculate bit position
                                unsigned int tmpScan = s_optJoyButtonScan;
                                int bitPos = 0;
                                while ((tmpScan = tmpScan >> 1) != 0) bitPos++;
                                s_optTempEntries[s_optCursorIndex].keyIndex = bitPos;
                                s_optTempEntries[i].animState = 1;
                            }

                            s_optAnimFrameCounter = 0;
                            ((unsigned char*)&g_playerEntity)[0xbe] = 0;
                            s_optDebounceTimer = 2;
                            s_optSubSubState = 3;
                            s_optCursorHighlight[s_optCursorIndex] = 2;
                        }
                    }
                }
            }
        }
    }
    else if (s_optSubSubState == 3) {
        // Debounce
        if (s_optDebounceTimer == 0) {
            s_optSubSubState = 1;
        }
        s_optDebounceTimer--;
    }

    // Render joypad binding values - 5 rows only (0x004532e8)
    unsigned char* pEntry = (unsigned char*)&s_optTempEntries[0];
    KeyBindEntry* next;
    i = 0;
    do {
        next = (KeyBindEntry*)(pEntry + 0x14);
        options_map_key_to_print_index((int)pEntry);
        sprintf(PRINT_TEXT_BUFFER, s_fmt_c, (int)(char)pEntry[1]);
        PrintText8x14(0x91, s_optKeyLabelY[i / 4] + s_optJoyLabelYOffset + 1,
            (unsigned char)s_optCursorHighlight[i / 4], 0);
        pEntry = (unsigned char*)next;
        i += 4;
    } while (next < &s_optTempEntries[5]);

    // Action labels (0x00453318)
    PrintFormattedText(0xa2, s_optKeyLabelY[0], s_optCursorHighlight[0], s_optLabelText_Action);
    PrintFormattedText(0xa2, s_optKeyLabelY[1], s_optCursorHighlight[1], s_optLabelText_Cancel);
    PrintFormattedText(0xa2, s_optKeyLabelY[2], s_optCursorHighlight[2], s_optLabelText_Start);
    PrintFormattedText(0xa2, s_optKeyLabelY[3], s_optCursorHighlight[3], s_optLabelText_QuickTurn);
    PrintFormattedText(0xa2, s_optKeyLabelY[4], s_optCursorHighlight[4], s_optLabelText_Map);

    // Player model animation
    if (s_optAnimSkipFlag == 0) {
        if ((*(char*)(((unsigned char*)ENTITY) + 0xbe) == 0) && (*(char*)(((unsigned char*)ENTITY) + 0xbf) == 1)) {
            // Advance the demo script for the highlighted joystick option row
            unsigned char charId = *(unsigned char*)(((unsigned char*)ENTITY) + 1) & 1;
            const int* script = PTR_PTR_004c04a8[charId][s_optCursorIndex];
            if (script != NULL) {  // row 5 is the table terminator
                s_optAnimFrameData = script[s_optAnimFrameCounter + 1];
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) =
                    (unsigned char)script[s_optAnimFrameCounter];
                s_optAnimFrameCounter += 2;
                if (PTR_DAT_004c0538[charId][s_optCursorIndex] <= s_optAnimFrameCounter) {
                    s_optAnimFrameCounter = 0;
                }
            }
        }

        switch (s_optCursorIndex) {
        case 0:
            s_optIdleAnimTrigger = 0;
            break;
        case 1:
            if ((*(char*)(((unsigned char*)ENTITY) + 0xbd) == 3) && (s_optAnimFrameCounter > 5) &&
                (*(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) > 7)) {
                *(char*)(((unsigned char*)ENTITY) + 0xbd) = 2;
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) = 0x18;
                *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
            }
            if ((s_optAnimFrameData == 1) &&
                ((((*(char*)(((unsigned char*)ENTITY) + 1) == 0) && (*(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) > 0x1f)) ||
                  ((*(char*)(((unsigned char*)ENTITY) + 1) == 1) && (*(unsigned char*)(((unsigned char*)ENTITY) + 0xbe) > 0x12))) &&
                 (*(char*)(((unsigned char*)ENTITY) + 0xbd) == 0))) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0x8c) = 3;
            }
            break;
        case 3:
            if (s_optWalkAnimTrigger == 0) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0;
                s_optWalkAnimTrigger = 1;
            }
            s_optIdleAnimTrigger = 0;
            break;
        case 4:
            if (s_optIdleAnimTrigger == 0) {
                *(unsigned char*)(((unsigned char*)ENTITY) + 0xbd) = 0;
                s_optIdleAnimTrigger = 1;
            }
        case 2:
            s_optWalkAnimTrigger = 0;
            break;
        default:
            goto keyConfigRender;
        }

        Joint_move(s_optAnimFrameData, g_playerEntity.jointMoveData0,
            g_playerEntity.jointMoveData1, 0x400);
    }

keyConfigRender:
    EntityComputeJointWorldMatrices(g_playerEntity.unk_ca);
    EntityApplyLookAtRotation();
    options_render_entity((int)&g_playerEntity);
    return 1;
}


// ============================================================================
// options_joystick_config_handler (0x00453a80)
// Joystick config sub-menu handler.
// Returns 0 to keep running, non-zero to exit.
// ============================================================================
unsigned int options_joystick_config_handler(void)
{
    unsigned char* pTmp;

    // Set up render state
    g_TextureDesc.clutX = 0;
    g_TextureDesc.colorMulR = 0;
    g_TextureDesc.colorMulG = 0;
    g_TextureDesc.colorMulB = 0;
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.clutY = 0x1ff;
    g_TextureDesc.texturePage = 0x15;

    if (s_optSubInitState == 0) {
        s_optPrevButtons = 0;
        s_optSubSubState = 0;
        s_optSubInitState = 1;

        unsigned int* pHL = (unsigned int*)&s_optCursorHighlight[0];
        for (int i = 0xb; i != 0; i--) {
            *pHL = 0;
            pHL++;
        }

        pTmp = (unsigned char*)&s_optTempEntries[0];
        do {
            pTmp[0] = 0x5f;
            pTmp[1] = 0xff;
            pTmp[2] = 0xff;
            *(int*)(pTmp + 4) = -1;
            *(int*)(pTmp + 8) = 0;
            *(int*)(pTmp + 0xc) = 0;
            pTmp += 0x14;
        } while (pTmp < (unsigned char*)&s_optTempEntries[5]);

        ENTITY = (Entity*)&g_playerEntity;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.attackAnim = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.scaMatrixData.localMatrix.t[0] = 0x960;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = 0x834;
        g_playerEntity.scaMatrixData.localMatrix.t[2] = -700;
    }
    else if (s_optSubInitState != 1) {
        return 0;
    }

    if ((s_optKeyScanResult != 0x11) && (s_optKeyScanResult != 0x1b)) {
        char result = options_joystick_config_input();
        if (result != 0) return 0;
    }

    // Clear temp entries
    pTmp = (unsigned char*)&s_optTempEntries[0];
    do {
        pTmp[0] = 0x5f;
        pTmp[1] = 0xff;
        pTmp[2] = 0xff;
        *(int*)(pTmp + 4) = 0;
        *(int*)(pTmp + 8) = 0;
        *(int*)(pTmp + 0xc) = 0;
        pTmp += 0x14;
    } while (pTmp < (unsigned char*)&s_optTempEntries[5]);

    *(int*)(((unsigned char*)&s_optTempEntries[0]) + 0x108) = 0;
    *(int*)(((unsigned char*)&s_optTempEntries[0]) + 0x10c) = 0;
    s_optCursorIndex = 0;
    s_optAnimFrameData = 0;
    s_optIdleAnimTrigger = 0;
    s_optWalkAnimTrigger = 0;
    s_optTempFlags = 0;
    s_optUnknown424 = 0;
    s_optAnimFrameCounter = 0;
    options_init_keybind_display();
    return 1;
}


// ============================================================================
// options_joystick_config_input (0x00453c10)
// SideWinder game pad config input handler - 4-state machine.
// 10 cursor positions: 8 remappable pad buttons (g_JoyRemapTbl[1][8..15])
// laid out around the pad picture, plus DEFAULT (8) and EXIT (9).
// Each row cycles through the button choice list; sidewinder buttons 8-15
// pick a row directly. Green connector lines are drawn with the EKG line
// primitive (FUN_00470c60).
// Returns 0 to keep running... inverted: returns 1 while running, and the
// caller (0x00453a80) returns 0 while this keeps running. A 0 return here
// exits the sidewinder screen (and with it the whole options menu).
// ============================================================================
unsigned int options_joystick_config_input(void)
{
    // EKG line struct accessors (g_EkgPrimaryLine at 0x00be1198)
    #define OPT_EKG_FLAGS (*(unsigned int*)&g_EkgPrimaryLine[0])
    #define OPT_EKG_X0    (*(short*)&g_EkgPrimaryLine[4])
    #define OPT_EKG_Y0    (*(short*)&g_EkgPrimaryLine[6])
    #define OPT_EKG_X1    (*(short*)&g_EkgPrimaryLine[8])
    #define OPT_EKG_Y1    (*(short*)&g_EkgPrimaryLine[10])
    #define OPT_EKG_R     (g_EkgPrimaryLine[12])
    #define OPT_EKG_G     (g_EkgPrimaryLine[13])
    #define OPT_EKG_B     (g_EkgPrimaryLine[14])

    // Button choice list per row: ACTION, DASH, GET READY, MENU, OPTION, NOT USED
    static const unsigned int btnChoices[6] = { 0x80, 0x40, 8, 0x800, 0x900, 0 };

    // Cursor box X positions (local_208[0x46..0x4f])
    static const short cursorBoxX[10] = {
        0xc, 0x17, 0x24, 6, 0x11, 0x1c, -66, 0x1d, -150, 0x4c
    };
    // Cursor box Y positions (local_c8, even entries: 0x13,7,0,4,-5,-0xc,-0x20,-0x1e,0x5d,0x5d)
    static const short cursorBoxY[10] = {
        0x13, 7, 0, 4, -5, -0xc, -0x20, -0x1e, 0x5d, 0x5d
    };
    // Cursor box widths [0..9] / heights [10..19] (local_a0)
    static const short cursorBoxW[20] = {
        0xe, 0xe, 0xe, 0xb, 0xb, 0xb, 0x23, 0x22, 0x4a, 0x4a,
        0xd, 0xf, 0xe, 0xb, 0xc, 0xc, 0x20, 0x1f, 0x12, 0x12
    };
    // Cursor box texture U/V coordinates (local_50)
    static const unsigned char cursorBoxTexU[10] = {
        0x4d, 0x5c, 0x6b, 0x4e, 0x5a, 0x66, 0x01, 0x26, 0x00, 0x00
    };
    static const unsigned char cursorBoxTexV[10] = {
        0x31, 0x31, 0x31, 0x24, 0x23, 0x23, 0x23, 0x23, 0x58, 0x45
    };

    // Green connector line coordinates (local_208[6..0x45])
    static const short lineUnderX0[8] = { 0xea, 0xe6, 0xd7, 9, 9, 9, 9, 0xe9 };
    static const short lineUnderY0[8] = { 0xc0, 0xa0, 0x7d, 0xbe, 0x9e, 0x7e, 0x5e, 0x60 };
    static const short lineUnderX1[8] = { 0x137, 0x138, 0x138, 0x56, 0x59, 0x5a, 0x59, 0x138 };
    static const short lineUnderY1[8] = { 0xc0, 0xa0, 0x7d, 0xbe, 0x9e, 0x7e, 0x5e, 0x60 };
    static const short lineDiagX0[8]  = { 0xea, 0xe6, 0xd7, 0x56, 0x59, 0x5a, 0x59, 0xe9 };
    static const short lineDiagY0[8]  = { 0xbf, 0x9f, 0x7f, 0xbe, 0x9e, 0x7e, 0x5e, 0x5f };
    static const short lineDiagX1[8]  = { 0xbb, 0xc6, 0xd7, 0xa6, 0xad, 0xb8, 0x6d, 0xd4 };
    static const short lineDiagY1[8]  = { 0x9b, 0x8f, 0x7f, 0x86, 0x7a, 0x71, 0x68, 0x68 };

    int i, j;

    switch (s_optSubSubState) {
    case 0: {
        // Populate the 8 temp entries from g_JoyRemapTbl[1][8..15] (0x00453e6b)
        int btnIdx = 8;
        s_optCursorIndex = 8;
        s_optCursorHighlight[8] = 1;
        s_optDefaultSensIdx = 5;
        for (i = 0; i < 8; i++) {
            s_optTempEntries[i].pad2 = g_JoyRemapTbl[1][8 + i];
            s_optTempEntries[i].keyIndex = btnIdx++;
        }
        s_optPrevKeyScan = FUN_00497de0();

        // Derive each row's button choice index (0x00453ec2); the search
        // resumes from the previous row's match like the original.
        j = 0;
        for (i = 0; i < 8; i++) {
            while (j <= 5 && btnChoices[j] != s_optTempEntries[i].pad2) j++;
            if (j > 5) j = 5;
            s_optSensitivityIdx[i] = j;
        }
        s_optSubSubState = 1;
        break;
    }

    case 1: {
        // Navigation (0x00453ef5)
        bool done = false;
        unsigned char scan;
        s_optKeyScanResult = FUN_00497de0();
        if ((s_optKeyScanResult == 0x11) || (s_optKeyScanResult == 0x1b) ||
            (s_optCancelKeyVK == s_optKeyScanResult)) {
            s_optKeyScanResult = 0;
            play_sfx(3, 5, 0);
            return 0;
        }
        scan = s_optKeyScanResult;
        if ((scan != 0x20) && (scan != 0x0d) && (s_optAcceptKeyVK != scan)) {
            unsigned short padByte = (unsigned short)(g_PlayerPadPressed >> 8);
            if ((padByte & 0xa0) != 0) {
                // Circle/Square → move between columns
                s_optCursorHighlight[s_optCursorIndex] = 0;
                play_sfx(3, 4, 0);
                if (s_optCursorIndex < 6) {
                    j = s_optCursorIndex + 3;
                    if (j > 5) j = s_optCursorIndex - 3;
                    s_optCursorHighlight[j] = 1;
                    s_optCursorIndex = j;
                }
                else if (s_optCursorIndex == 6) {
                    s_optCursorIndex = 7;
                    s_optCursorHighlight[7] = 1;
                }
                else if (s_optCursorIndex == 7) {
                    s_optCursorIndex = 6;
                    s_optCursorHighlight[6] = 1;
                }
                else if (s_optCursorIndex == 8) {
                    s_optCursorIndex = 9;
                    s_optCursorHighlight[9] = 1;
                }
                else if (s_optCursorIndex == 9) {
                    s_optCursorIndex = 8;
                    s_optCursorHighlight[8] = 1;
                }
                done = true;
            }
            if (!done && ((padByte & 0x10) != 0)) {
                // Triangle → next row (0x00453f8c)
                int cur;
                play_sfx(3, 4, 0);
                cur = s_optCursorIndex;
                s_optCursorHighlight[s_optCursorIndex] = 0;
                if ((cur != 2) && (cur < 6)) {
                    s_optCursorIndex = (cur + 1) & 7;
                    s_optCursorHighlight[s_optCursorIndex] = 1;
                }
                else if (cur == 6) {
                    s_optCursorIndex = 8;
                    s_optCursorHighlight[8] = 1;
                }
                else if (cur == 7) {
                    s_optCursorIndex = 9;
                    s_optCursorHighlight[9] = 1;
                }
                else if (cur == 8) {
                    s_optCursorIndex = 3;
                    s_optCursorHighlight[3] = 1;
                }
                else if (cur == 9) {
                    s_optCursorIndex = 0;
                    s_optCursorHighlight[0] = 1;
                }
                else if (cur == 2) {
                    s_optCursorIndex = 7;
                    s_optCursorHighlight[7] = 1;
                }
                done = true;
            }
            if (!done && ((padByte & 0x40) != 0)) {
                // Cross → previous row (0x00454042)
                int cur;
                play_sfx(3, 4, 0);
                cur = s_optCursorIndex;
                s_optCursorHighlight[s_optCursorIndex] = 0;
                if (cur != 3) {
                    if ((cur != 0) && (cur < 7)) {
                        s_optCursorHighlight[cur - 1] = 1;
                        s_optCursorIndex = cur - 1;
                        done = true;
                    }
                    else if (cur == 7) {
                        s_optCursorIndex = 2;
                        s_optCursorHighlight[2] = 1;
                        done = true;
                    }
                    else if (cur == 8) {
                        s_optCursorIndex = 6;
                        s_optCursorHighlight[6] = 1;
                        done = true;
                    }
                    else if (cur == 9) {
                        s_optCursorIndex = 7;
                        s_optCursorHighlight[7] = 1;
                        done = true;
                    }
                    else if (cur == 0) {
                        s_optCursorIndex = 9;
                        s_optCursorHighlight[9] = 1;
                        done = true;
                    }
                    // else: fall through to sidewinder scan (0x00454443)
                }
                else {
                    s_optCursorIndex = 8;
                    s_optCursorHighlight[8] = 1;
                    done = true;
                }
            }
            if (!done) {
                // Sidewinder buttons 8-15 jump straight to select (0x00454443)
                s_optJoyButtonScan = read_sidewinder_pad() & JOY_SCAN_BUTTON_MASK;
                if (s_optPrevJoyButtonScan != s_optJoyButtonScan) {
                    if (s_optJoyButtonScan == 0) {
                        s_optPrevJoyButtonScan = 0;
                        done = true;
                    }
                    else {
                        bool validBit = false;
                        j = 8;
                        do {
                            if (s_optJoyButtonScan & (1 << (j & 0x1f))) {
                                validBit = true;
                                break;
                            }
                            j++;
                        } while (j < 0x10);
                        scan = s_optPrevKeyScan;
                        s_optPrevJoyButtonScan = s_optJoyButtonScan;
                        if (!validBit &&
                            (((g_PlayerDpadPressed >> 8) & 0x40) == 0)) {
                            done = true;
                        }
                    }
                }
                else {
                    done = true;
                }
            }
        }
        s_optPrevKeyScan = scan;
        if (done) break;

        // Select (0x0045447c)
        if (s_optCursorIndex < 8) {
            play_sfx(3, 6, 0);
            s_optDefaultSensIdx = s_optSensitivityIdx[s_optCursorIndex];
            s_optSubSubState = 2;
            s_optCursorHighlight[s_optCursorIndex] = 2;
        }
        else if (s_optCursorIndex == 8) {
            // DEFAULT - restore the default sidewinder mapping (0x004544b4)
            play_sfx(3, 6, 0);
            for (i = 0; i < 8; i++) {
                g_JoyRemapTbl[1][8 + i] = 0;
                s_optTempEntries[i].pad2 = 0;
            }
            g_JoyRemapTbl[1][8] = 0x80;
            g_JoyRemapTbl[1][9] = 0x40;
            g_JoyRemapTbl[1][0xa] = 0x800;
            g_JoyRemapTbl[1][0xb] = 0x900;
            g_JoyRemapTbl[1][0xf] = 8;
            s_optTempEntries[0].pad2 = 0x80;
            s_optTempEntries[1].pad2 = 0x40;
            s_optTempEntries[2].pad2 = 0x800;
            s_optTempEntries[3].pad2 = 0x900;
            s_optTempEntries[7].pad2 = 8;
            options_init_keybind_display();
            s_optSubSubState = 3;
            s_optDebounceTimer = 2;
        }
        else {
            // EXIT (0x00454563)
            return 0;
        }
        break;
    }

    case 2: {
        // Edit the highlighted row (0x00454578)
        s_optKeyScanResult = FUN_00497de0();
        s_optJoyButtonScan = read_sidewinder_pad() & JOY_SCAN_BUTTON_MASK;
        if ((s_optPrevJoyButtonScan != s_optJoyButtonScan) ||
            (s_optKeyScanResult != s_optPrevKeyScan)) {
            bool commit = true;
            s_optPrevKeyScan = s_optKeyScanResult;
            if ((s_optKeyScanResult == 0x11) || (s_optKeyScanResult == 0x1b) ||
                (s_optCancelKeyVK == s_optKeyScanResult)) {
                s_optKeyScanResult = 0;
                play_sfx(3, 5, 0);
                return 0;
            }
            if ((s_optKeyScanResult != 0x20) && (s_optKeyScanResult != 0x0d) &&
                (s_optAcceptKeyVK != s_optKeyScanResult)) {
                unsigned short padByte = (unsigned short)(g_PlayerPadPressed >> 8);
                if ((padByte & 0x80) != 0) {
                    // Square → previous button choice (0x00454608)
                    play_sfx(3, 4, 0);
                    s_optSensitivityIdx[s_optCursorIndex]--;
                    if (s_optSensitivityIdx[s_optCursorIndex] < 0) {
                        s_optSensitivityIdx[s_optCursorIndex] = 5;
                    }
                    s_optTempEntries[s_optCursorIndex].pad2 =
                        btnChoices[s_optSensitivityIdx[s_optCursorIndex]];
                    commit = false;
                }
                else if ((padByte & 0x20) != 0) {
                    // Circle → next button choice (0x00454646)
                    play_sfx(3, 4, 0);
                    s_optSensitivityIdx[s_optCursorIndex]++;
                    if (s_optSensitivityIdx[s_optCursorIndex] > 5) {
                        s_optSensitivityIdx[s_optCursorIndex] = 0;
                    }
                    s_optTempEntries[s_optCursorIndex].pad2 =
                        btnChoices[s_optSensitivityIdx[s_optCursorIndex]];
                    commit = false;
                }
                else if (s_optPrevJoyButtonScan == s_optJoyButtonScan) {
                    commit = false;
                }
                else if (s_optJoyButtonScan == 0) {
                    s_optPrevJoyButtonScan = 0;
                    commit = false;
                }
                else {
                    bool validBit = false;
                    j = 8;
                    do {
                        if (s_optJoyButtonScan & (1 << (j & 0x1f))) {
                            validBit = true;
                            break;
                        }
                        j++;
                    } while (j < 0x10);
                    s_optPrevJoyButtonScan = s_optJoyButtonScan;
                    if (!validBit) {
                        commit = false;
                    }
                    else if ((padByte & 0x80) != 0) {
                        // Sidewinder press + square cancels back to the
                        // default choice (0x004546d8)
                        play_sfx(3, 5, 0);
                        s_optTempEntries[s_optCursorIndex].pad2 =
                            btnChoices[s_optDefaultSensIdx];
                        s_optSensitivityIdx[s_optCursorIndex] = s_optDefaultSensIdx;
                        s_optCursorHighlight[s_optCursorIndex] = 1;
                        s_optSubSubState = 1;
                        commit = false;
                    }
                }
            }
            if (commit) {
                // Commit the selected choice to the remap table (0x00454730)
                unsigned int val = btnChoices[s_optSensitivityIdx[s_optCursorIndex]];
                play_sfx(3, 6, 0);
                g_JoyRemapTbl[1][s_optCursorIndex + 8] = val;
                s_optTempEntries[s_optCursorIndex].pad2 = val;
                options_init_keybind_display();
                s_optSubSubState = 3;
                s_optDebounceTimer = 2;
                s_optCursorHighlight[s_optCursorIndex] = 1;
            }
        }
        break;
    }

    case 3:
        // Debounce delay (0x0045477c)
        if (s_optDebounceTimer == 0) {
            s_optSubSubState = 1;
        }
        s_optDebounceTimer--;
        break;
    }

    // Render the 8 row labels (0x00454790)
    for (i = 0; i < 8; i++) {
        const unsigned char* text;
        switch (s_optTempEntries[i].pad2) {
        case 0x80:  text = s_joyBtnText_Action;   break;
        case 0x40:  text = s_joyBtnText_Dash;     break;
        case 8:     text = s_joyBtnText_GetReady; break;
        case 0x800: text = s_joyBtnText_Menu;     break;
        case 0x900: text = s_joyBtnText_Option;   break;
        default:    text = s_joyBtnText_NotUsed;  break;
        }
        PrintFormattedText(s_optJoyLabelX[i], (short)(s_optJoyLabelY[i] - 0xf),
            (unsigned char)s_optCursorHighlight[i], text);
    }

    // Render the green connector lines (0x00454818): an underline below each
    // label plus a diagonal line to the pad button, each drawn twice 1px apart.
    for (i = 0; i < 8; i++) {
        OPT_EKG_FLAGS = 0;
        OPT_EKG_R = 0;
        OPT_EKG_G = (i == s_optCursorIndex) ? 200 : 0x68;
        OPT_EKG_B = 0;
        OPT_EKG_X0 = lineUnderX0[i];
        OPT_EKG_X1 = lineUnderX1[i];
        OPT_EKG_Y0 = (short)(lineUnderY0[i] - 0xf);
        OPT_EKG_Y1 = (short)(lineUnderY1[i] - 0xf);
        FUN_00470c60(g_EkgPrimaryLine, 0);
        OPT_EKG_Y0++;
        OPT_EKG_Y1++;
        FUN_00470c60(g_EkgPrimaryLine, 0);
        OPT_EKG_X0 = lineDiagX0[i];
        OPT_EKG_X1 = lineDiagX1[i];
        OPT_EKG_Y0 = (short)(lineDiagY0[i] - 0xf);
        OPT_EKG_Y1 = (short)(lineDiagY1[i] - 0xf);
        FUN_00470c60(g_EkgPrimaryLine, 0);
        OPT_EKG_Y0++;
        OPT_EKG_Y1++;
        FUN_00470c60(g_EkgPrimaryLine, 0);
    }

    // Render the cursor box around the selected row (0x00454908)
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.screenX = cursorBoxX[s_optCursorIndex];
    g_TextureDesc.texturePage = 2;
    g_TextureDesc.screenY = (short)((s_optCursorIndex < 8 ? -0xf : -5) +
        cursorBoxY[s_optCursorIndex]);
    g_TextureDesc.width = (unsigned short)cursorBoxW[s_optCursorIndex];
    g_TextureDesc.height = (unsigned short)cursorBoxW[s_optCursorIndex + 10];
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.texU = cursorBoxTexU[s_optCursorIndex];
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.texV = cursorBoxTexV[s_optCursorIndex];
    g_TextureDesc.clutX = 0;
    g_TextureDesc.clutY = 0x1e0;
    display_texture(&g_TextureDesc, 2, 0xb, 1);
    return 1;
}


// ============================================================================
// options_menu (0x004761b0)
// Main options menu state machine.
// Called as Task_execute from check_menus_state.
// 6 states: 0=wait fade, 1=main nav, 2=display config, 3=key config, 4=exit, 5=joystick config
// ============================================================================
void options_menu(void)
{
    MATRIX localCamMatrix;
    unsigned char savedEntityData[384]; // 0x180 bytes
    unsigned char savedEquippedItemId;

    // Initialize camera matrix
    memset(&localCamMatrix, 0, sizeof(MATRIX));
    localCamMatrix.m[1][1] = 5000;

    s_optCurrentTab = 0; // 0x00ac9cf9

    // Sub-menu handler function table
    typedef unsigned int (*SubHandler)(void);
    SubHandler handlers[3];
    handlers[0] = options_key_config_handler;
    handlers[1] = options_display_config_handler;
    handlers[2] = options_joystick_config_handler;

    Task_suspend(0);
    s_optMainState = 0; // 0x00ac9e70
    g_controllerConfig = g_controllerConfig & 0xef;
    s_optCursorPos = 0; // 0x00ac9e73

    // Check if controller flag needs to be set
    if ((g_main_state_flags2 & MSF2_SCREEN_BORDER) != 0) {
        g_controllerConfig = g_controllerConfig | 0x10;
        g_main_state_flags2 = g_main_state_flags2 & ~MSF2_SCREEN_BORDER;
    }

    // Set up 3 light sources for options background
    // Light 0: position (100, 80, -780), color (128, 128, 128)
    s_optLightData0[0] = 100;        // 0x00d22700 pos_x
    s_optLightData0[1] = 0x50;       // 0x00d22704 pos_y
    s_optLightData0[2] = -780;       // 0x00d22708 pos_z
    ((unsigned char*)&s_optLightData0[3])[0] = 0x80; // 0x00d2270c R
    ((unsigned char*)&s_optLightData0[3])[1] = 0x80; // 0x00d2270d G
    ((unsigned char*)&s_optLightData0[3])[2] = 0x80; // 0x00d2270e B

    // Light 1: position (-100, 80, -780), color (128, 128, 128)
    s_optLightData1[0] = -100;       // 0x00d22710 pos_x
    s_optLightData1[1] = 0x50;       // 0x00d22714 pos_y
    s_optLightData1[2] = -780;       // 0x00d22718 pos_z
    ((unsigned char*)&s_optLightData1[3])[0] = 0x80; // 0x00d2271c R
    ((unsigned char*)&s_optLightData1[3])[1] = 0x80; // 0x00d2271d G
    ((unsigned char*)&s_optLightData1[3])[2] = 0x80; // 0x00d2271e B

    // Light 2: position (-100, 0, -780), color (128, 128, 128)
    s_optLightData2[0] = -100;       // 0x00d22724 pos_x
    s_optLightData2[1] = 0;          // 0x00d22720 pos_y (set to 0, then pos_x overwritten)
    s_optLightData2[2] = -780;       // 0x00d22728 pos_z
    ((unsigned char*)&s_optLightData2[3])[0] = 0x80; // 0x00d2272c R
    ((unsigned char*)&s_optLightData2[3])[1] = 0x80; // 0x00d2272d G
    ((unsigned char*)&s_optLightData2[3])[2] = 0x80; // 0x00d2272e B

    FUN_0040ac80(0, s_optLightData0);
    FUN_0040ac80(1, s_optLightData1);
    FUN_0040ac80(2, s_optLightData2);
    setBackColor(409, 409, 409);

    // Fade in
    g_fade_type_id = 2;
    g_fading_counter = 0xf800;
    fade_update();

    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;

    // Load options background texture
    LoadFile(s_optBgKeyConfig, &g_TimImageBuffer, 0x20);
    display_image(8, g_TimImageBuffer__bitmap, 320, 240);
    title_setup_texture_pages(8, 1);
    // empty_00470960(8): empty in the original - call dropped
    StMask(0, 3);
    set_scene_render_param(0xcf);
    MatrixToCamera(&localCamMatrix);

    // Save player entity state
    memcpy(savedEntityData, &g_playerEntity, 0x180);
    savedEquippedItemId = g_EquippedItemId;

    // Set up player entity for options display
    g_EquippedItemId = 1;
    g_playerEntity.equippedWeaponId = 2;
    LoadEquippedWeaponAnimation(2, 0xe, (unsigned int)g_animationBuffer,
                                (unsigned int)g_animObjectBuffer);
    SetSubpixelOffset(0xa0, 0x78);

    ENTITY = (Entity*)&g_playerEntity;
    g_playerEntity.unk_8c = 0;
    g_playerEntity.attackAnim = 0;
    g_playerEntity.animation_frame_id = 0;
    g_playerEntity.unk_bf = 0;
    g_playerEntity.id = g_playerEntity.id & 1;
    g_playerEntity.scaMatrixData.localMatrix.t[0] = 0x960;
    g_playerEntity.scaMatrixData.localMatrix.t[1] = 0x834;
    g_playerEntity.scaMatrixData.localMatrix.t[2] = -700;
    g_playerEntity.position.pad = 0;
    g_playerEntity.directionAngle = 0xce4;
    g_playerEntity.speed.x = 0;

    options_init_keybind_display();

    // Main state machine loop
    do {
        if (g_resetGameFlag != 0) {
            s_optMainState = 4;
        }

        switch (s_optMainState) {
        case 0:
            // Wait for fade to complete
            if ((short)g_fading_state < 0) {
                s_optMainState = 1;
                s_optPrevButtons = 0;
                goto stateMainNav;
            }
            break;

        case 1:
stateMainNav:
        {
            unsigned short held = (unsigned short)g_PlayerPadHeld;
            if ((held & 0x840) != 0) {
                // L1+R1 or SELECT pressed - exit
                play_sfx(3, 5, 0);
                options_menu_exit();
            }
            else if ((held & 0xa0) != 0) {
                // L2 or R2 - switch joystick mode
                if (s_optUseJoystickMode == 0) {
                    if (s_optCursorPos == 0) {
                        play_sfx(3, 6, 0);
                        s_optSubInitState = 0;
                        s_optMainState = 3;
                        if (s_optCurrentTab != 0) {
                            LoadFile(s_optBgKeyConfig, &g_TimImageBuffer, 0x20);
                            display_image(8, g_TimImageBuffer__bitmap, 0x140, 0xf0);
                            title_setup_texture_pages(8, 1);
                            // empty_00470960(8): empty in the original - call dropped
                            StMask(0, 3);
                            goto loadKeyConfigBg;
                        }
                    }
                    else if (s_optCursorPos == 1) {
                        play_sfx(3, 6, 0);
                        s_optSubInitState = 0;
                        s_optMainState = 2;
                        if (s_optCurrentTab != 1) {
                            LoadFile(s_optBgDisplayCfg, &g_TimImageBuffer, 0x20);
                            display_image(8, g_TimImageBuffer__bitmap, 0x140, 0xf0);
                            title_setup_texture_pages(8, 1);
                            // empty_00470960(8): empty in the original - call dropped
                            StMask(0, 3);
                            s_optCurrentTab = 1;
                        }
                    }
                    else if (s_optCursorPos == 2) {
                        options_menu_exit();
                    }
                }
                else {
                    play_sfx(3, 6, 0);
                    s_optSubInitState = 0;
                    s_optMainState = 5;
                    if (s_optCurrentTab != 2) {
                        LoadFile(s_optBgJoystick, &g_TimImageBuffer, 0x20);
                        display_image(8, g_TimImageBuffer__bitmap, 0x140, 0xf0);
                        title_setup_texture_pages(8, 0);
                        // empty_00470960(8): empty in the original - call dropped
                        StMask(0, 3);
                        s_optCurrentTab = 2;
                    }
                }
            }
            else {
                // D-pad navigation
                unsigned short result = options_input_repeat(&s_optRepeatTimer, &s_optPrevButtons, 0xe6, 0xf000);
                if ((result & 0xa000) != 0) {
                    // UP/DOWN selected
                    play_sfx(3, 4, 0);
                    if (s_optUseJoystickMode != 0) {
                        // In joystick mode - run joystick sub-handler
                        s_optSubInitState = 0;
                        do {
                            char ret = handlers[s_optCurrentTab]();
                            if (ret != 0) goto afterSubHandler;
                            Task_sleep(1);
                        } while (g_resetGameFlag == 0);
                        // Restore joystick remap on cancel
                        if (s_optCurrentTab == 2) {
                            memcpy(s_joyRemapBackupJoy, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
                        }
                        else {
                            memcpy(s_joyRemapBackupKey, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
                        }
afterSubHandler:
                        if ((held & 0x800) == 0) {
                            s_optMainState = 1;
                            s_optCursorPos = 0;
                        }
                        else {
                            play_sfx(3, 5, 0);
                            options_menu_exit();
                        }
                        break;
                    }
                    if ((result & 0x8000) == 0) {
                        // Not CROSS pressed
                        if (s_optCursorPos == 2) {
                            s_optSubInitState = 0;
                            do {
                                char ret = handlers[s_optCurrentTab]();
                                if (ret != 0) goto afterSubHandlerNav;
                                Task_sleep(1);
                            } while (g_resetGameFlag == 0);
                            // Restore remap tables
                            if (s_optCurrentTab == 2) {
                                memcpy(s_joyRemapBackupJoy, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
                            }
                            else {
                                memcpy(s_joyRemapBackupKey, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
                            }
afterSubHandlerNav:;
                        }
                        else {
                            s_optCursorPos = s_optCursorPos + 1;
                        }
                    }
                    else if (s_optCursorPos == 0) {
                        s_optSubInitState = 0;
                        do {
                            char ret = handlers[s_optCurrentTab]();
                            if (ret != 0) goto afterSubHandlerCross;
                            Task_sleep(1);
                        } while (g_resetGameFlag == 0);
                        if (s_optCurrentTab == 2) {
                            memcpy(s_joyRemapBackupJoy, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
                        }
                        else {
                            memcpy(s_joyRemapBackupKey, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
                        }
afterSubHandlerCross:;
                    }
                    else {
                        s_optCursorPos = s_optCursorPos - 1;
                    }
                }
                if ((result & 0x5000) != 0) {
                    // LEFT/RIGHT - toggle joystick mode
                    play_sfx(3, 4, 0);
                    s_optUseJoystickMode = (s_optUseJoystickMode == 0) ? 1 : 0;
                }
            }
            break;
        }

        case 2:
            // Display config sub-menu
            do {
                char ret = options_display_config_handler();
                if (ret != 0) break;
                Task_sleep(1);
            } while (g_resetGameFlag == 0);
            if (((unsigned short)g_PlayerPadHeld & 0x800) == 0) {
                s_optMainState = 1;
            }
            else {
                play_sfx(3, 5, 0);
                options_menu_exit();
            }
            break;

        case 3:
            // Key config sub-menu
            do {
                char ret = options_key_config_handler();
                if (ret != 0) goto afterKeyConfig;
                Task_sleep(1);
            } while (g_resetGameFlag == 0);
            // Restore key remap backup
            memcpy(s_joyRemapBackupKey, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
afterKeyConfig:
            memcpy(s_joyRemapBackupKey, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
            if (((unsigned short)g_PlayerPadHeld & 0x800) == 0) {
                s_optMainState = 1;
            }
            else {
                play_sfx(3, 5, 0);
                options_menu_exit();
            }
            break;

        case 4:
            // Exit state
            if ((g_main_state_flags & MSF_FADE_ACTIVE) == 0) {
                // Restore player entity
                memcpy(&g_playerEntity, savedEntityData, 0x180);
                g_EquippedItemId = savedEquippedItemId;
                menu_update_equipped_weapon();
                LoadEquippedWeaponAnimation(g_playerEntity.equippedWeaponId, 0xe,
                    (unsigned int)g_animationBuffer, (unsigned int)g_animObjectBuffer);
                g_playerEntity.unk_8c = 0;
                g_playerEntity.animation_frame_id = 0;
                g_playerEntity.unk_bf = 0;
                g_playerEntity.isBeingAttackedFlag = 0;
                g_playerEntity.attackAnim = 0;
                g_playerEntity.animationId = 1;
                g_playerEntity.animFrameId = 0;
                g_playerEntity.action_behavior = 0;
                g_playerEntity.action_state = 0;
                Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
                g_main_state_flags = g_main_state_flags & ~MSF_MENU_BYTE;

                // Restore controller config flag
                if ((g_controllerConfig & 0x10) != 0) {
                    g_main_state_flags2 = g_main_state_flags2 | MSF2_SCREEN_BORDER;
                    g_controllerConfig = g_controllerConfig & 0xef;
                }

                // Restore room lighting
                unsigned char lightIdx = 3;
                do {
                    lightIdx--;
                    FUN_0040ac80((int)lightIdx, &g_RdtPointer->lights[lightIdx]);
                } while (lightIdx != 0);

                setBackColor((unsigned short)g_RdtPointer->ambient_light_r,
                             (unsigned short)g_RdtPointer->ambient_light_g,
                             (unsigned short)g_RdtPointer->ambient_light_b);
                cut_set();
                s_loadSaveStateFlag = 0;
                Task_Resume(0);
                Task_exit(); // Does not return
            }
            break;

        case 5:
            // Joystick config sub-menu
            do {
                char ret = options_joystick_config_handler();
                if (ret != 0) goto afterJoyConfig;
                Task_sleep(1);
            } while (g_resetGameFlag == 0);
            memcpy(s_joyRemapBackupJoy, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
afterJoyConfig:
            memcpy(s_joyRemapBackupJoy, g_JoyRemapTbl[1], 32 * sizeof(unsigned int));
            play_sfx(3, 5, 0);
            options_menu_exit();
loadKeyConfigBg:
            s_optCurrentTab = 0;
            break;
        }

        // Render current state (except during exit)
        if (s_optMainState != 4) {
            g_TextureDesc.clutX = 0;
            g_TextureDesc.colorMulR = 0;
            g_TextureDesc.pivotX = 0;
            g_TextureDesc.colorMulG = 0;
            g_TextureDesc.pivotY = 0;
            g_TextureDesc.colorMulB = 0;
            g_TextureDesc.clutY = 0x1ff;
            g_TextureDesc.texturePage = 0x15;
            unk_00be1180 = 0;
            options_menu_render();
        }

        Task_sleep(1);
    } while (true);
}
