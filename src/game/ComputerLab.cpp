// ComputerLab.cpp - The lab computer terminal (room 5060, stage 5).
//
// This is the third of the three "interactive screens" dispatched by
// check_and_display_interactive_screen (0x0042a030). g_ScenarioFlags bit 0x20
// (SCENARIO_FLAG_INTERACTIVE_SCREEN) is the "a screen is up" gate; the SysFlags
// bit picks which one:
//
//   SysFlags 0x1d -> display_passcode_panel  (InteractiveScreen.cpp)
//   SysFlags 0x1e -> display_computer_lab    (0x00412390)   <- this file
//   SysFlags 0x1f -> display_slides          (not ported)
//
// The terminal is a first-person state: the player model is parked 9000 units
// below the floor, the room switches to camera 4 (the monitor close-up), the
// RDT's three lights and ambient colour are swapped for a fixed set, and two
// separate entities - em1014 and em1015, Chris's right and left forearm -
// are driven as the on-screen hands. They are ordinary entities in slots 0 and
// 1; this file only writes commands into their behavior_flags and polls bit
// 0x20 for "command finished". See entities/ComputerArms.cpp.
//
// The player logs in with a virtual keyboard (user JOHN, password ADA), then
// picks a door from a menu and types the release command (MOLE). Success
// raises a bit in g_LocksFlags, which is the bank door_try_enter reads.
//
// Structure. The original is one big three-level state machine indexed by the
// first four bytes of the shared interactive-screen state block at 0x00d22790
// (the same block the passcode panel uses - see the note on the accessors
// below), dispatched through nine function tables at 0x004b44a8..0x004b459c.
// Several of those tables OVERLAP: a "later" dispatcher indexes the same array
// from a shifted base so that two stages share a common tail of steps. The
// port keeps one array per logical step list and reproduces the shift as an
// explicit base offset, which is why e.g. s_loginSteps is indexed [subSub2]
// from one caller and [6 + subSub2] from another.
//
// Original step-handler tables (Ghidra names them lab_tN_stepM; every entry's
// behaviour is implemented by this file's state machine):
//   T1 @0x004b44a8: 0x004123d0 (computer_lab_init) 0x004125b0 0x00413920
//                   (computer_lab_finish)
//   T2 @0x004b44b8: 0x004125d0 0x00412920 0x00412df0 0x00413380 0x00413710
//   T3 @0x004b44d0: 0x00412680 0x004126c0 0x00412730 0x00412790 0x00412940
//                   0x00412a20 0x00412bf0
//   T4 @0x004b44f0: 0x00412a40 0x00412a80 0x00412af0 0x00412a80 0x00412b50
//                   0x00412bb0 0x00412c10 0x00412c30 0x00412c60 0x00412d10
//                   0x00412d80   (step 3 reuses step 1's handler)
//   T5 @0x004b4520: 0x00412f60 0x00412fd0 0x00413040 0x004130d0 0x00413170
//                   0x004131b0 0x00413240 0x00413280 0x004132f0
//   T6 @0x004b4548: 0x004133a0 0x004134a0 0x004134e0
//   T7 @0x004b4558: 0x00413500 0x00413540 0x004135a0 0x004135d0 0x00413650
//                   0x004136c0 0x00413780 0x00413820 0x00413850
//   T8 @0x004b4580: 0x00413a00 0x00413b50 0x00413ba0
//   T9 @0x004b4590: 0x00413ea0 0x00413ec0 0x00413f90
//   plus computer_lab_finish_helper (0x00413c10), called once from
//   computer_lab_finish (0x00413930).
// ============================================================================
#include "../Globals.h"
#include "Types.h"
#include "Entities.h"
#include "PrintText.h"
#include "SpriteRenderer.h"
#include "FileLoader.h"
#include "../system/AssetPath.h"
#include "../DebugPrint.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

extern void Flg_on(int baseAddr, unsigned int bitIndex);          // 0x00473ef0
extern void FUN_00473f10(int* baseAddr, unsigned int bitIndex);   // 0x00473f10 - Flg_off
extern void play_sound_and_voice_effect(int type, int id);        // SoundSystem.cpp

// 0x0046edb0 - the scaled/paged sprite submit the terminal UI draws through.
// Added alongside AddSprite_Ex in Rendering.cpp.
extern int SubmitEffectSprite_Ex(TextureDesc* texture, unsigned short fade,
                                 int slot, int pageCount, int depthKey);


// ============================================================================
// The shared interactive-screen state block (0x00d22790)
//
// `sprite_anim_flags_` in the decompilation is a POINTER at 0x00d2278c to this
// block, and Ghidra types it differently per function - byte* in some, int* in
// others. That is why the same source text `sprite_anim_flags_ + 4` means byte
// offset 4 in cl_keyboard_input but byte offset 0x10 in cl_stage_login. Both
// readings are in the decompiled output and they are NOT the same field; the
// int* functions are the ones that also write `*(char *)((int)ptr + 2)` with
// the extra (int) cast. Getting this backwards silently turns the login step
// timer into the keyboard cursor column.
//
// Byte layout, and the port global that already covers each slot:
//   +0x00 funcIndex   g_labSlidesFuncIndex
//   +0x01 subState    g_labSlidesAnimState
//   +0x02 subSub      g_passcodePanelAnimationState
//   +0x03 subSub2     g_labSlidesSubState2
//   +0x04 int  cursorCol   g_labSlidesScrollX   (passcode panel: cursor X)
//   +0x08 int  cursorRow   g_labSlidesScrollY   (passcode panel: cursor Y)
//   +0x0c int  savedPlayerY g_labSlidesSavedPlayerY
//   +0x10 short timerA     g_labSlidesTimerA
//   +0x12 short timerB     g_labSlidesTimerB
// ============================================================================
#define ST_FUNC     g_labSlidesFuncIndex
#define ST_SUB      g_labSlidesAnimState
#define ST_SUB2     g_passcodePanelAnimationState
#define ST_SUB3     g_labSlidesSubState2
#define ST_COL      g_labSlidesScrollX
#define ST_ROW      g_labSlidesScrollY
#define ST_TIMER_A  g_labSlidesTimerA
#define ST_TIMER_B  g_labSlidesTimerB

// 0x004123da / 0x00413660: `MOV dword ptr [EAX],<imm>` - a DWORD store over
// bytes 0..3, so it sets funcIndex AND clears the three sub-states.
static void cl_set_state_word(unsigned int packed)
{
    ST_FUNC = (unsigned char)(packed);
    ST_SUB  = (unsigned char)(packed >> 8);
    ST_SUB2 = (unsigned char)(packed >> 16);
    ST_SUB3 = (unsigned char)(packed >> 24);
}

// ============================================================================
// Data structures (all in .data at 0x004d6d70..0x004d6eb0 in the original)
// ============================================================================

// One row of art inside a terminal window. 0x10 bytes; the drawn size is
// width<<3 by height<<4 because the source art is a character grid.
struct ClRow {
    unsigned char  texU;
    unsigned char  pad1[3];
    unsigned char  texV;
    unsigned char  pad2[3];
    short          width;
    short          height;
    short          texturePage;
    short          pad3;
};

// A terminal window (frame + rows of art). 4 of them at 0x004d6dc8, 0x1c apart.
// Window 3 is special: it has no geometry of its own and its `enabled` byte
// doubles as a bitfield whose bit 7 means "the keyboard cursor moved this
// frame" (cl_keyboard_input sets it, cl_draw_keyboard consumes it).
struct ClWindow {
    unsigned char  enabled;     // +0x00
    unsigned char  rowCount;    // +0x01  (window 3: reused as "cancelled")
    short          x;           // +0x02
    short          y;           // +0x04
    short          cols;        // +0x06
    short          rows;        // +0x08
    unsigned char  pad0a[2];    // +0x0a
    const ClRow*   rowData;     // +0x0c
    unsigned char  animCmd;     // +0x10  0 idle, 1 open, 0x81 close
    unsigned char  animState;   // +0x11
    short          alpha;       // +0x12
    short          visible;     // +0x14
    short          big;         // +0x16  picks the 0xff/16-step ramp over 0x80/8
    unsigned char  pad18[4];    // +0x18
};

// A text-entry field. 2 of them at 0x004d6e88, 0x12 apart.
struct ClText {
    unsigned char  enabled;     // +0x00
    unsigned char  winIdx;      // +0x01
    short          x;           // +0x02
    short          y;           // +0x04
    unsigned char  text[9];     // +0x06  8 chars + NUL
    unsigned char  masked;      // +0x0f  draw '*' instead of the glyph
    unsigned char  pad10[2];    // +0x10
};

// The "hands type what is on screen" animator at 0x004d6db0.
struct ClTyper {
    unsigned char  active;      // +0x00
    unsigned char  state;       // +0x01
    unsigned char  pad2[2];
    int            charIdx;     // +0x04
    unsigned char  pad8[8];
    short          timer;       // +0x10
    short          line;        // +0x12
};

static ClWindow s_win[4];       // 0x004d6dc8
static ClText   s_text[2];      // 0x004d6e88
static ClTyper  s_typer;        // 0x004d6db0

static unsigned char  s_inputEnabled;    // 0x004d6d70
static unsigned char  s_caretWinIdx;     // 0x004d6d71
static short          s_caretBaseX;      // 0x004d6d72
static short          s_caretBaseY;      // 0x004d6d74
static unsigned char  s_caretMode;       // 0x004d6d76 0 = char caret, 1 = menu bar
static unsigned short s_caretPos;        // 0x004d6d77
static unsigned char  s_textRecIdx;      // 0x004d6d79
static unsigned char  s_blinkCounter;    // 0x004d6d7a
static unsigned char  s_blinkHold;       // 0x004d6d7b
static unsigned char  s_ledFlags;        // 0x004d6d84
static unsigned char  s_msgFlags;        // 0x004d6d88
static unsigned char  s_statusIdx;       // 0x004d6d9c
static unsigned char  s_doorSel;         // 0x004d6da0 bit7 = show the big picture
static unsigned char  s_colB;            // 0x004d6da4
static unsigned char  s_colG;            // 0x004d6da8
static unsigned char  s_colR;            // 0x004d6dc4
static unsigned short s_padRepeatLatch;  // 0x004d6e22
static unsigned char  s_padRepeatTimer;  // 0x004d6e36
static unsigned char  s_pendingCamera;   // 0x004d6e38 bit7 = pending
static unsigned char  s_pendingCamDelay; // 0x004d6e40
static unsigned char  s_simpleMode;      // 0x004b44a4
static unsigned char  s_introLogoState;  // 0x00d91bc5

// Saved RDT lighting, restored on exit.
static RDT_Light s_savedLight[3];        // 0x004d6e48 / 0x5c / 0x70
static int       s_savedAmbient[3];      // 0x004d6d90 / 0x94 / 0x98

// The window that a "cancelled" flag lives in - window 3's rowCount byte.
#define S_CANCELLED  s_win[3].rowCount

// ============================================================================
// Const tables (0x004b3f20 .. 0x004b4308)
// ============================================================================

// 0x004b3f20 - the three lights the terminal installs over the room's own.
static const RDT_Light s_labLights[3] = {
    { 14300, -1950,  9650, 0x78, 0x78, 0x78, 0, 0, 6735 },
    {  4450, -2650, 12000, 0x78, 0x78, 0x78, 0, 0, 7055 },
    { 14700, -1600,  5600, 0x78, 0x78, 0x78, 0, 0, 7495 },
};

#define ROW(u, v, w, h, d) { (u), {0,0,0}, (v), {0,0,0}, (w), (h), (d), 0 }

// 0x004b3f60 - login window. Row 2's width is patched between 0 and 0xc to
// hide/reveal the password line (the original writes _DAT_004b3f88 directly,
// which is this row's `width` field).
static ClRow s_rowsLogin[3] = {
    ROW(0x00, 0x90, 32, 2, 9),
    ROW(0x00, 0xe0,  7, 1, 7),
    ROW(0x00, 0xf0,  0, 1, 7),
};
static const ClRow s_rowsPanelA[3] = {      // 0x004b3fa0
    ROW(0x00, 0xb0, 32, 1, 9),
    ROW(0x00, 0x00,  0, 0, 7),
    ROW(0x60, 0xf0, 13, 1, 7),
};
static const ClRow s_rowsSide[3] = {        // 0x004b3fd0
    ROW(0x38, 0xe0, 3, 1, 7),
    ROW(0x50, 0xe0, 3, 1, 7),
    ROW(0xc8, 0xe0, 7, 1, 7),
};
static const ClRow s_rowsMenu[4] = {        // 0x004b4000
    ROW(0xc8, 0xf0, 6, 1, 7),
    ROW(0x00, 0x00, 0, 0, 7),
    ROW(0xe0, 0x68, 4, 1, 7),
    ROW(0xe0, 0x78, 4, 1, 7),
};
static const ClRow s_rowsPanelB[4] = {      // 0x004b4040
    ROW(0x00, 0xc0, 21, 1, 9),
    ROW(0x00, 0xd0, 21, 1, 9),
    ROW(0x00, 0x00,  0, 0, 7),
    ROW(0x00, 0xf0, 12, 1, 7),
};
#undef ROW

// Expected inputs. These are virtual-keyboard char codes, not ASCII: code 1 is
// 'A', so {10,15,8,14} spells JOHN. 0x004b3f90 / 0x004b3f98 / 0x004b4080.
static const unsigned char s_expectUser[8] = { 10, 15,  8, 14, 0, 0, 0, 0 };  // JOHN
static const unsigned char s_expectPass[8] = {  1,  4,  1,  0, 0, 0, 0, 0 };  // ADA
static const unsigned char s_expectCmd [8] = { 13, 15, 12,  5, 0, 0, 0, 0 };  // MOLE

// 0x004b40a8 - char code -> which forearm reaches for the key and with which
// animation. Bit 7 picks the left arm (entity slot 1), the low bits are the
// reach animation; the command written is (v & 0x3f) | 0x40.
static const unsigned char s_armForKey[32] = {
    0x83, 0x82, 0x81, 0x01, 0x02, 0x02, 0x02, 0x03,
    0x83, 0x82, 0x81, 0x01, 0x01, 0x02, 0x02, 0x03,
    0x83, 0x82, 0x81, 0x01, 0x01, 0x01, 0x02, 0x03,
    0x83, 0x82, 0x81, 0x01, 0x01, 0x01, 0x03, 0x03,
};

// A drawable quad for cl_draw_quads. 12 bytes in the original; `flags` bit 0
// makes `repeat` tile vertically instead of horizontally, and bits 6-7 are
// folded into the sprite blend flags.
struct ClQuad {
    short         x, y;
    unsigned char texU, texV;
    short         width, height;
    unsigned char flags, repeat;
};

// 0x004b40c8 - the 9-slice window frame. Six of these fields are rewritten
// every frame from the window's own size before the draw (see cl_draw_windows).
static ClQuad s_frameQuads[8] = {
    {  0,  0, 0x50, 0x20, 16, 16, 0x00, 0 },
    {  0,  0, 0x70, 0x20, 16, 16, 0x00, 0 },
    {  0,  0, 0x50, 0x40, 16, 16, 0x00, 0 },
    {  0,  0, 0x70, 0x40, 16, 16, 0x00, 0 },
    { 16,  0, 0x60, 0x20, 16, 16, 0x00, 0 },
    { 16,  0, 0x60, 0x40, 16, 16, 0x00, 0 },
    {  0, 16, 0x50, 0x30, 16, 16, 0x01, 0 },
    {  0, 16, 0x70, 0x30, 16, 16, 0x01, 0 },
};

// 0x004b4128 - the virtual keyboard's own frame (10 quads).
static const ClQuad s_keyboardFrame[10] = {
    {  16,  16, 0x10, 0x20, 192, 96, 0x00,  0 },
    {   8,   8, 0x00, 0x48,  16,  8, 0x00, 12 },
    { 200,   8, 0x00, 0x48,   8,  8, 0x00,  0 },
    {   8,  16, 0x08, 0x50,   8, 16, 0x00,  0 },
    {   8,  32, 0x08, 0x60,   8, 16, 0x01,  5 },
    {   8, 112, 0x08, 0x70,   8,  8, 0x00,  0 },
    {  16, 112, 0x20, 0x80,  16,  8, 0x00, 11 },
    { 192, 112, 0x30, 0x80,  24,  8, 0x00,  0 },
    { 208,   8, 0x50, 0x80,   8, 16, 0x01,  6 },
    { 208, 104, 0x50, 0x80,   8,  8, 0x00,  0 },
};

// 0x004b4194 - full-screen monitor pictures, picked by (msgFlags & 3). Index 0
// overlaps the last quad of s_keyboardFrame in the original and is never drawn
// because the caller gates on (msgFlags & 3) != 0; the port keeps the slot so
// the indices line up.
static const ClQuad s_msgPictures[4] = {
    { 0, 0, 0x00, 0x00,   0,   0, 0x00, 0 },   // unused (overlap slot)
    { 0, 0, 0x00, 0x00, 120, 104, 0x00, 0 },
    { 0, 0, 0x78, 0x00, 120, 104, 0x00, 0 },
    { 0, 0, 0x00, 0x68, 120, 104, 0x00, 0 },
};

// 0x004b41bc - status banners, picked by statusIdx. Index 4 is two quads; the
// rest are one. Index 0 is the never-drawn overlap slot again.
static const ClQuad s_statusBanners[6] = {
    { 0,  0, 0x00, 0x00,   0,  0, 0x00, 0 },   // unused (overlap slot)
    { 0,  0, 0x78, 0x68, 100, 32, 0x00, 0 },
    { 0,  0, 0x78, 0x88, 100, 32, 0x00, 0 },
    { 0,  0, 0x78, 0xa8, 100, 32, 0x00, 0 },
    { 0,  0, 0x78, 0xc8, 100, 24, 0x00, 0 },
    { 0, 24, 0x78, 0xa0, 100,  8, 0x00, 0 },   // second quad of index 4
};

static const ClQuad s_stripA[2] = {         // 0x004b4220 (msgFlags & 0x10)
    { 12, 29, 0x84, 0x17, 42, 3, 0x00, 0 },
    { 12, 32, 0x84, 0x14, 20, 3, 0x00, 0 },
};
static const ClQuad s_stripB[3] = {         // 0x004b4238 (msgFlags & 0x20)
    { 12, 39, 0x84, 0x17, 30, 3, 0x00, 0 },
    { 12, 42, 0x8a, 0x17, 30, 3, 0x00, 0 },
    { 12, 47, 0x84, 0x14, 16, 3, 0x00, 0 },
};

// 0x004b425c/0x004b4260 and 0x004b42d8/0x004b42e0 - the "this door is now
// released" marker, one per door. The two bytes before each quad list are an
// unsigned origin that the caller biases by (-0x3c, -0x5f).
static const unsigned char s_doorOrigin[2][2] = { { 50, 20 }, { 28, 8 } };
static const ClQuad s_doorMarker[2] = {
    {  0, 8, 0x68, 0xe0, 20,  8, 0x01, 3 },
    { 12, 4, 0x68, 0xe0, 12, 12, 0x00, 0 },
};

// 0x004b45a0 - the password mask glyph.
static const char s_maskString[] = "*";

// ============================================================================
// Forward declarations
// ============================================================================
static void cl_windows_update(void);
static void cl_typing_update(void);
static void cl_draw_quads(short baseX, short baseY, unsigned char flags,
                          unsigned char count, const ClQuad* quads,
                          unsigned short texturePage, short depthKeyA, int depthKeyB);
static int  cl_fade_step(char step, unsigned char msgOnFadeOut);
static void cl_text_begin(unsigned char recIdx);

// ============================================================================
// Pad helpers
//
// The terminal reads byte 1 of the remapped pad words (see the note in
// InteractiveScreen.cpp): 0x40 = confirm, 0x80 = cancel. Cursor movement comes
// off g_button_pressed_id's raw D-pad nibble instead - 0x1000 up, 0x2000
// right, 0x4000 down, 0x8000 left.
// ============================================================================
static inline unsigned char cl_dpad_pressed(void) { return (unsigned char)(g_PlayerDpadPressed >> 8); }
static inline unsigned char cl_dpad_held(void)    { return (unsigned char)(g_PlayerDpadHeld >> 8); }
static inline unsigned char cl_pad_held(void)     { return (unsigned char)(g_PlayerPadHeld >> 8); }

// ============================================================================
// cl_window_setup (0x00414290)
// Point a window at a row list and open it closed (alpha 0x80 seed, hidden).
// ============================================================================
static void cl_window_setup(unsigned char idx, short x, short y, short cols,
                            short rows, unsigned char rowCount, const ClRow* rowData)
{
    ClWindow* w = &s_win[idx];
    w->x        = x;
    w->y        = y;
    w->cols     = cols;
    w->rows     = rows;
    w->rowCount = rowCount;
    w->rowData  = rowData;
    w->alpha    = 0x80;
    w->visible  = 0;
    w->animState = 0;
}

// ============================================================================
// cl_text_begin (0x00414230)
// Clear a text field and make it the one the caret and the keyboard write to.
// ============================================================================
static void cl_text_begin(unsigned char recIdx)
{
    ClText* t = &s_text[recIdx];
    memset(t->text, 0, sizeof(t->text));
    s_caretWinIdx  = t->winIdx;
    s_caretPos     = 0;
    s_blinkCounter = 0;
    s_textRecIdx   = recIdx;
    s_caretBaseX   = t->x;
    s_caretBaseY   = t->y;
}

// ============================================================================
// cl_text_append (0x004141b0)
// Commit one virtual-keyboard result into a field. -1 (enter) and -2 (exit)
// are handled by the caller and ignored here; -3 is backspace.
//
// The caret stops at 7, so the eighth character is overwritten in place rather
// than advancing - that is the original's behaviour, not a clamp bug.
// ============================================================================
static void cl_text_append(short key, unsigned char* buffer)
{
    if (key == -1 || key == -2) return;

    if (key == -3) {
        if (s_caretPos != 0) {
            if (s_caretPos < 7) {
                s_caretPos--;
            } else if (buffer[s_caretPos] == 0) {
                s_caretPos--;
            }
        }
        buffer[s_caretPos] = 0;
        return;
    }

    bool advance = (s_caretPos < 7);
    buffer[s_caretPos] = (unsigned char)key;
    if (advance) s_caretPos++;
}

// ============================================================================
// cl_keyboard_input (0x00414010)
//
// The virtual keyboard is an 8x4 grid; cell = row*8 + col. Three cells are not
// letters:
//   cell 0     -> -2  EXIT      (top-left)
//   cell 0x17  -> -1  ENTER     (drawn double height, covers cell 0x1f)
//   cell 0x1d  -> -3  BACKSPACE (drawn double width, covers cell 0x1e)
// Every other cell yields a char code equal to the cell index, less one for
// cells past ENTER so the codes stay contiguous (A=1 .. Z=26).
//
// The two covered cells are unreachable: landing on 0x1f steps the row back
// and landing on 0x1e steps the column back.
//
// Returns 0 when nothing was committed this frame.
// ============================================================================
static short cl_keyboard_input(void)
{
    // Bit 7 of window 3's `enabled` is the "cursor moved" flag consumed by
    // cl_draw_keyboard; clear it first and re-raise it below.
    s_win[3].enabled &= 0x7f;

    short cell = (short)((short)ST_ROW * 8 + (short)ST_COL);
    short result = cell;

    if ((cl_dpad_pressed() & 0x40) != 0) {
        if (cell == 0)            result = -2;
        else if (cell == 0x17)    result = -1;
        else if (cell == 0x1d)    result = -3;
        else if (cell > 0x16)     result = (short)(cell - 1);
        s_win[3].enabled |= 0x80;
        return result;
    }

    // Auto-repeat: the same D-pad direction held re-fires every 0xf frames
    // after an initial 8-frame delay.
    const unsigned short dpad = (unsigned short)(g_button_pressed_id & 0xf000);
    if (dpad == s_padRepeatLatch && s_padRepeatLatch != 0) {
        if (s_padRepeatTimer != 0) {
            s_padRepeatTimer--;
            return 0;
        }
        s_padRepeatTimer = 0xf;
    } else {
        s_padRepeatTimer = 8;
    }
    s_padRepeatLatch = (unsigned short)(g_PlayerPadHeld & 0xf000);

    const int oldRow = (int)ST_ROW;
    const int oldCol = (int)ST_COL;

    if ((dpad & 0x1000) != 0 && (int)ST_ROW > 0) ST_ROW = (unsigned int)((int)ST_ROW - 1);
    if ((dpad & 0x4000) != 0 && (int)ST_ROW < 3) ST_ROW = (unsigned int)((int)ST_ROW + 1);
    if ((dpad & 0x8000) != 0) {
        ST_COL--;
        if (ST_COL < 0) ST_COL = 7;
    }
    if ((dpad & 0x2000) != 0) {
        ST_COL++;
        // Stepping right off BACKSPACE skips its second cell in one go.
        if (cell == 0x1d) ST_COL++;
        if (ST_COL > 7) ST_COL = 0;
    }

    cell = (short)((short)ST_COL + (short)ST_ROW * 8);
    if (cell == 0x1f) ST_ROW = (unsigned int)((int)ST_ROW - 1);   // inside ENTER
    if (cell == 0x1e) ST_COL--;                                   // inside BACKSPACE

    if ((int)ST_COL != oldCol || (int)ST_ROW != oldRow) s_win[3].enabled |= 0x80;
    return 0;
}

// ============================================================================
// cl_fade_step (0x00415410)
//
// Ramps the global colour multiplier down then up, one `step` per frame per
// channel. Phase lives in timerB: 0 = fading out, non-zero = fading in. When
// the fade-out completes it also installs `msgOnFadeOut` into msgFlags, which
// is how the screen contents get swapped at the darkest point.
//
// The `timerA == 2` early-out at 0x00415415 really is on timerA and not on the
// phase counter (verified against the instruction stream: `cmp word ptr
// [eax+0x10], 2`). Callers that are counting timerA down past 2 get a constant
// "done" from then on.
// ============================================================================
static int cl_fade_step(char step, unsigned char msgOnFadeOut)
{
    if (ST_TIMER_A == 2) return 1;

    if (ST_TIMER_B == 0) {
        s_colR = (unsigned char)(s_colR - step);
        s_colG = (unsigned char)(s_colG - step);
        s_colB = (unsigned char)(s_colB - step);
        if (s_colR == 0 || s_colG == 0 || s_colB == 0) {
            s_colB = 0;
            s_colG = 0;
            s_colR = 0;
            s_msgFlags = msgOnFadeOut;
            ST_TIMER_B++;
            return 0;
        }
    } else {
        s_colR = (unsigned char)(s_colR + step);
        s_colG = (unsigned char)(s_colG + step);
        s_colB = (unsigned char)(s_colB + step);
        if (s_colR >= 0x80 || s_colG >= 0x80 || s_colB >= 0x80) {
            s_colB = 0x80;
            s_colG = 0x80;
            s_colR = 0x80;
            ST_TIMER_B++;
            return 1;
        }
    }
    return 0;
}

// ============================================================================
// cl_camera_step (0x004154d0)
// Deferred camera switch: the state machine writes the target camera with bit
// 7 set and an optional frame delay, and the switch happens here.
// ============================================================================
static void cl_camera_step(void)
{
    if ((s_pendingCamera & 0x80) == 0) return;
    if (s_pendingCamDelay != 0) { s_pendingCamDelay--; return; }

    g_roomCameraId = (unsigned char)(s_pendingCamera & 0x7f);
    check_camera_switch(1);
    s_pendingCamDelay = 0;
    s_pendingCamera &= 0x7f;
}

// ============================================================================
// Texture management
// ============================================================================

// 0x00477fa0
static void cl_load_textures(void)
{
    LoadFile(GAME_DATA_ROOT "data\\umb00.tim", g_TimImageBuffer, 0x20);
    LoadTexturePage(g_TimImageBuffer, 7, 0xb, 0x17, 0, 0, 0, 0);
    LoadFile(GAME_DATA_ROOT "data\\umb01.tim", g_TimImageBuffer, 0x20);
    LoadTexturePage(g_TimImageBuffer, 9, 0xc, 0x18, 0, 0, 0, 0);
    LoadFile(GAME_DATA_ROOT "data\\umb02.tim", g_TimImageBuffer, 0x20);
    LoadTexturePage(g_TimImageBuffer, 0xb, 0xd, 0x19, 0, 0, 0, 0);

    // 0x00477fc6 also rebuilds texture SET 0x30 from the same umb02 buffer
    // (FUN_0046ca30) and then walks 0x004c4420 building one textured quad per
    // viewport slot against it (CreateTexturedQuad). Both feed only the
    // Umbrella boot-logo animation's DrawPrim_SpriteLarge path, which is not
    // ported yet - see cl_intro_logo_update. cl_free_textures still tears set
    // 0x30 down so the behaviour matches once that is filled in.
}

// 0x00478070
static void cl_load_intro_texture(void)
{
    delete_texture_set_secondary(0x19);
    LoadFile(GAME_DATA_ROOT "data\\umbrella.tim", g_TimImageBuffer, 0x20);
    LoadTexturePage(g_TimImageBuffer, 0, 0, 0x1c, 0, 0, 0, 0);
}

// 0x004780b0
static void cl_free_textures(void)
{
    for (int slot = 0x1d; slot >= 0x17; slot--) delete_texture_set_secondary(slot);
    TexturePage_DeleteSet(0x30);
    TexturePage_DeleteSet(0x2e);
}

// ============================================================================
// The Umbrella boot logo (0x00488a60)
//
// A nine-slot animation system at 0x00ac3698, stride 0x30, walked through a
// global current-slot pointer at 0x00ac3848 (which is why every original
// function here indexes off one pointer instead of taking an argument).
//
// What it builds is the Umbrella Corp. mark: eight wedges, alternating between
// the two halves of the sheet, swept out around a common centre. Each wedge is
// one slot, and the eight fall out of the spawn arithmetic exactly:
//
//   the hub itself                                          1  (quad 0)
//   + three behaviour-1 wedges, one per 90 degrees           3  (quad 0)
//   + one behaviour-2 wedge to close the hub's ring          1  (quad 1)
//   + one behaviour-2 wedge seeded by each behaviour-1       3  (quad 1)
//                                                          ---
//                                                            8
//
// four of each quad, which is what makes the mark alternate. Plus the wordmark,
// and nine slots is exactly the array size.
//
// Slots co-operate through two fields: `phase` (+0x16), which a slot raises for
// its children to watch, and `parent` (+0x18). The choreography is:
//
//   behaviour 0  the hub. Spins, and every 90 degrees spawns one wedge, three
//                as behaviour 1 and the last as behaviour 2. Also spawns the
//                behaviour-3 wordmark. Raises phase 1 to start the fly-out and
//                phase 2 to tell everyone to retire.
//   behaviour 1  a wedge that spawns one more behaviour-2 wedge of its own once
//                the hub has swept round to 270 degrees.
//   behaviour 2  a wedge that only follows.
//   behaviour 3  the UMBRELLA wordmark: wipes in, holds for the jingle, then
//                slides down and shrinks. Its case 4 is what writes 2 into the
//                gate at 0x00d91bc5 and ends the whole sequence.
//
// `angle` (+0x2c) is in DEGREES, not PS1 units: every mover steps it by 15 and
// wraps at 0x168 == 360, so one revolution is 24 frames. The draw scales it back
// into PS1 units per path - `angle << 12` for the wordmark's descriptor and
// `angle * 0xb` for the wedges (0xb because 360 * 11 = 3960 ~ 4096).
// ============================================================================
struct ClLogoSlot {
    unsigned char  active;      // +0x00
    unsigned char  behaviour;   // +0x01
    unsigned char  sub;         // +0x02
    unsigned char  pad03[5];    // +0x03
    int            x;           // +0x08  16.16
    int            y;           // +0x0c  16.16
    unsigned char  pad10[4];    // +0x10
    short          counter;     // +0x14
    short          phase;       // +0x16
    ClLogoSlot*    parent;      // +0x18
    unsigned char  texU;        // +0x1c
    unsigned char  texV;        // +0x1d
    short          width;       // +0x1e
    short          height;      // +0x20
    short          depthBase;   // +0x22
    short          pivotX;      // +0x24
    short          pivotY;      // +0x26
    short          scaleX;      // +0x28
    short          scaleY;      // +0x2a
    int            angle;       // +0x2c  DEGREES, 0..359
};
static_assert(sizeof(ClLogoSlot) == 0x30, "ClLogoSlot stride mismatch");

#define CL_LOGO_SLOTS 9
static ClLogoSlot s_logoSlots[CL_LOGO_SLOTS];   // 0x00ac3698

// 0x004895c0 - first free slot, marked active with its sub-state cleared.
// Returns NULL when all nine are taken; the original does NOT check, it writes
// through the null. Every call site here is guarded (port-only).
static ClLogoSlot* cl_logo_alloc(void)
{
    for (int i = 0; i < CL_LOGO_SLOTS; i++) {
        if (s_logoSlots[i].active == 0) {
            s_logoSlots[i].active = 1;
            s_logoSlots[i].sub    = 0;
            return &s_logoSlots[i];
        }
    }
    return NULL;
}

// Spawn a wedge that tracks `s`, copying its position and phase angle.
static void cl_logo_spawn_wedge(ClLogoSlot* s, unsigned char behaviour)
{
    ClLogoSlot* c = cl_logo_alloc();
    if (c == NULL) return;
    c->behaviour = behaviour;
    c->x      = s->x;
    c->y      = s->y;
    c->angle  = s->angle;
    c->parent = s;
}

// The +15-degrees-per-frame step every mover shares.
static void cl_logo_spin(ClLogoSlot* s)
{
    s->angle += 15;
    if (s->angle == 0x168) s->angle = 0;
}

// 0x00489030 - the shared fly-out: slide until x reaches -42 and shrink as it
// goes. Reaching the end advances the sub-state, which is how each wedge knows
// to move on to waiting for the hub's phase 2.
static void cl_logo_move_out(ClLogoSlot* s)
{
    if (s->x <= (int)0xffd60000) {           // -42.0 in 16.16
        s->sub++;
        s->x = (int)0xffd60000;
        return;
    }
    s->x    -= 0x30000;
    s->scaleX = (short)(s->scaleX - 0xd0);
    s->scaleY = (short)(s->scaleY - 0xd0);
}

// --------------------------------------------------------------------------
// behaviour 0 - the hub (0x00488e20 + its six sub-states)
// --------------------------------------------------------------------------

// sub 2 (0x00488f40). Called both from the table and directly by sub 1.
static void cl_logo_hub_spawn(ClLogoSlot* s)
{
    if (s->angle % 0x5a != 0) return;       // every 90 degrees = 6 frames

    s->counter--;
    if (s->counter == 0) {
        s->sub++;
        s->counter = 3;
        s->phase   = 0;
        cl_logo_spawn_wedge(s, 2);          // the last wedge is a follower
        return;
    }
    cl_logo_spawn_wedge(s, 1);
}

// sub 0 (0x00488e60)
static void cl_logo_hub_init(ClLogoSlot* s)
{
    s->sub++;
    s->texU   = 0x20;
    s->texV   = 0x18;
    s->width  = 0x20;
    s->height = 0x28;
    s->pivotX = 0x10;
    s->pivotY = 0x28;      // pivot at the wedge's base, so it sweeps
    s->scaleX = 0x1000;
    s->scaleY = 0x1000;
    s->angle  = 0;
    s->depthBase = 5;
    s->counter   = 3;
}

// sub 1 (0x00488ef0) - three full revolutions alone, then start the ring and
// the wordmark. Note it runs sub 2's body in the SAME frame (0x00488f19 is a
// direct call), so the first wedge appears on the transition frame.
static void cl_logo_hub_spin_up(ClLogoSlot* s)
{
    if (s->angle != 0) return;
    s->counter--;
    if (s->counter != 0) return;

    s->sub++;
    s->counter = 4;
    cl_logo_hub_spawn(s);

    ClLogoSlot* w = cl_logo_alloc();
    if (w != NULL) {
        w->behaviour = 3;
        w->parent    = s;
    }
}

// sub 3 (0x00489000) - hold the assembled ring, then release the fly-out.
static void cl_logo_hub_hold(ClLogoSlot* s)
{
    if (s->angle != 0) return;
    s->counter--;
    if (s->counter != 0) return;
    s->sub++;
    s->phase = 1;
}

// sub 5 (0x00489080) - retire on the first frame the angle is 15 short of a
// quarter turn, and tell the children to go with it.
static void cl_logo_hub_end(ClLogoSlot* s)
{
    if ((s->angle + 15) % 0x5a != 0) return;
    s->phase  = 2;
    s->active = 0;
    s->sub    = 0;
}

typedef void (*ClLogoStep)(ClLogoSlot*);

// 0x004d34d0 - behaviour 0's sub-table. Slot 4 is the shared mover.
static const ClLogoStep s_logoHubSteps[6] = {
    cl_logo_hub_init,     // 0
    cl_logo_hub_spin_up,  // 1
    cl_logo_hub_spawn,    // 2
    cl_logo_hub_hold,     // 3
    cl_logo_move_out,     // 4
    cl_logo_hub_end,      // 5
};

// 0x00488e20 - the hub always spins after its sub-state runs.
static void cl_logo_behaviour_hub(ClLogoSlot* s)
{
    if (s->sub < 6) s_logoHubSteps[s->sub](s);
    cl_logo_spin(s);
}

// --------------------------------------------------------------------------
// behaviour 1 (0x004890c0) - a wedge that seeds one more wedge at 270 degrees
// --------------------------------------------------------------------------
static void cl_logo_behaviour_wedge_a(ClLogoSlot* s)
{
    switch (s->sub) {
    case 0:
        s->sub++;
        s->texU   = 0x20;
        s->texV   = 0x18;
        s->width  = 0x20;
        s->height = 0x28;
        s->pivotX = 0x10;
        s->pivotY = 0x28;
        s->scaleX = 0x1000;
        s->scaleY = 0x1000;
        s->depthBase = 5;
        return;
    case 1:
        // Wait for the hub to reach 270 degrees, then seed a follower. This one
        // does not spin yet, so it sits where it was spawned.
        if (s->parent != NULL && s->parent->angle == 0x10e) {
            s->sub++;
            s->phase = 0;
            cl_logo_spawn_wedge(s, 2);
        }
        return;
    case 2:
        cl_logo_spin(s);
        if (s->parent != NULL && s->parent->phase == 1) {
            s->sub++;
            s->phase = 1;
        }
        return;
    case 3:
        cl_logo_spin(s);
        cl_logo_move_out(s);
        return;
    default:
        cl_logo_spin(s);
        if (s->parent != NULL && s->parent->phase == 2) {
            s->phase  = 2;
            s->active = 0;
            s->sub    = 0;
        }
        return;
    }
}

// --------------------------------------------------------------------------
// behaviour 2 (0x00489270) - a follower wedge. Same shape as behaviour 1 minus
// the spawn, and it uses the other half of the sheet (texU 0).
// --------------------------------------------------------------------------
static void cl_logo_behaviour_wedge_b(ClLogoSlot* s)
{
    switch (s->sub) {
    case 0:
        s->sub++;
        s->texU   = 0x00;
        s->texV   = 0x18;
        s->width  = 0x20;
        s->height = 0x28;
        s->pivotX = 0x10;
        s->pivotY = 0x28;
        s->scaleX = 0x1000;
        s->scaleY = 0x1000;
        s->depthBase = 5;
        return;
    case 1:
        // Hold still until it trails the parent by exactly 45 degrees. It is
        // spawned at the parent's angle and does not spin here, so the parent's
        // +15 a frame opens the gap in three frames.
        if (s->parent != NULL && s->angle - s->parent->angle == -0x2d) s->sub++;
        return;
    case 2:
        cl_logo_spin(s);
        if (s->parent != NULL && s->parent->phase == 1) s->sub++;
        return;
    case 3:
        cl_logo_spin(s);
        cl_logo_move_out(s);
        return;
    default:
        cl_logo_spin(s);
        if (s->parent != NULL && s->parent->phase == 2) {
            s->phase  = 2;
            s->active = 0;
            s->sub    = 0;
        }
        return;
    }
}

// --------------------------------------------------------------------------
// behaviour 3 (0x00489360) - the UMBRELLA wordmark, and the slot that ends the
// sequence. This is the only slot whose draw the port can already express.
// --------------------------------------------------------------------------
static void cl_logo_behaviour_wordmark(ClLogoSlot* s)
{
    switch (s->sub) {
    case 0:
        s->sub++;
        s->texU   = 0x18;
        s->texV   = 0x00;
        s->width  = 0;          // nothing to draw until case 1 opens it out
        s->height = 0x18;
        s->pivotX = 0;
        s->pivotY = 0;
        s->scaleX = 0x1000;
        s->scaleY = 0x1000;
        s->x      = 0x3b0000;   //  59.0
        s->y      = (int)0xffc90000;  // -55.0
        s->angle  = 0;
        s->depthBase = 4;
        return;
    case 1: {
        // Wipe in: the left edge walks left at 1.125 px a frame while the width
        // grows to match, so the word unrolls out of its right-hand end.
        s->x -= 0x12000;
        const short xi = (short)(s->x >> 16);
        s->width = (short)(0x3b - xi);
        if (xi < -0x1e) {
            s->sub++;
            s->x = (int)0xffe20000;   // -30.0
        }
        return;
    }
    case 2:
        // Wait for the ring to finish retiring, then snap to the full logo.
        if (s->parent != NULL && s->parent->phase == 2) {
            s->sub++;
            s->x       = (int)0xffca0000;   // -54.0
            s->width   = 0x70;
            s->texU    = 0;
            s->counter = 0x5a;              // 90 frames
        }
        return;
    case 3:
        // Hold for the jingle. Bit 0x20000 is the voice-busy flag: it is raised
        // when the logo asks for voice 0xb7 and cleared by
        // play_sound_and_voice_effect type 2 / UpdateMusicWaitState, so the hold
        // is however long the sting actually runs, not a fixed count.
        if (s->counter > 0) s->counter--;
        if (s->counter < 1 && (g_main_state_flags & MSF_VOICE_PLAYING) == 0) {
            s->sub++;
            s->x       = 0x20000;    // 2.0
            s->y      += 0xc0000;    // +12.0
            s->width   = 0x70;
            s->texU    = 0;
            s->pivotX  = 0x38;
            s->pivotY  = 0x0c;
            s->scaleX  = 0x1000;
            s->scaleY  = 0x1000;
        }
        return;
    default:
        // Shrink about its own centre down to 2/3 while sliding down, then end
        // the whole sequence.
        if (s->scaleX < 0xaab) {
            s->scaleX = 0xaaa;
            s->scaleY = 0xaaa;
        } else {
            s->scaleX = (short)(s->scaleX - 0x32);
            s->scaleY = (short)(s->scaleY - 0x32);
        }
        s->y += 0x10000;
        if ((int)(s->y & 0xffff0000u) > -0x120001) {
            s->y      = (int)0xffee0000;    // -18.0
            s->active = 0;
            s_introLogoState = 2;           // 0x00d91bc5 - releases cl_boot_logo
        }
        return;
    }
}

static const ClLogoStep s_logoBehaviours[4] = {   // 0x004d34c0
    cl_logo_behaviour_hub,       // 0
    cl_logo_behaviour_wedge_a,   // 1
    cl_logo_behaviour_wedge_b,   // 2
    cl_logo_behaviour_wordmark,  // 3
};

// ============================================================================
// The eight Umbrella-mark wedges - the port's stand-in for OT type 4
//
// The wedges do not go through the sprite path at all in the original. Their
// case calls DrawPrim_SpriteLarge (0x0046fcf0), which fills an ORDERING-TABLE
// ENTRY OF TYPE 4 that the OT dispatcher (FUN_0042bf20 case 4) hands to
// FUN_0042a9e0, the matrix-transformed textured-object draw. Reproducing
// FUN_0042a9e0 literally is out of the question - it is a D3D5 execute-buffer
// builder - and unnecessary: it is the same machinery the project already
// replaced for models with a CPU transform plus DrawTriangles3D.
//
// The OT entry (base 0x008e1d60, stride 0x84) was solved by matching
// DrawPrim_SpriteLarge's stores against FUN_0042a9e0's loads:
//   +0x08..+0x47  a row-major 4x4, rotation from FUN_0048c8a0 and its
//                 TRANSLATION IN ROW 3 (+0x38/+0x3c/+0x40 - the xyz is not a
//                 separate field, which is what makes this one matrix multiply)
//   +0x48/+0x4c/+0x50  three floats that SCALE MATRIX ROWS 0/1/2 (2.0, 2.0, 1.0)
//   +0x54/+0x58   the Marni 3D-object handle and the page
//   +0x68         alpha, where 0 means OPAQUE - same convention as the sprite
//                 path's +0x2c (see TextureDraw::variantAlpha)
// so a vertex is  v' = 2*(q rotated about Z) + (px, py, pz).
//
// Rotation is Z-ONLY: params[3] and params[4] are 0 at all three call sites,
// which collapses the general case to a 2D rotation about the wedge's base.
// FUN_0048c8a0 reads each angle as params[n] * (1/4096), i.e. TURNS, so the
// wedge turns (angle * 0xb) / 4096 of a revolution - very slightly under a full
// turn per 360 state-machine degrees, because 0xb is the original's integer
// stand-in for 4096/360 = 11.378. That imprecision is reproduced.
//
// Two substitutions, both port-only:
//  * Texture. The original samples texture SET 0x30, which FUN_0046ca30 builds
//    from umb02. umb02 is ALREADY loaded as a page by cl_load_textures, so the
//    same image is reachable at page descriptor 0x28 (0x19 + the 0xF that
//    LoadTexturePage adds internally) and set 0x30 need not be ported.
//  * Geometry. CreateTexturedQuad's records at 0x004c4420 are transcribed below
//    rather than run through the port's CreateTexturedQuad, whose viewport calls
//    are stubbed and which therefore retains nothing usable.
//
// The result is submitted as a type-12 four-corner command, which is exactly a
// rotated textured quad with per-corner UVs and a per-corner w - already
// implemented in FlushSpriteCommandsRange for ground shadows.
// ============================================================================

// 0x004c4420 - the two wedge quads, 23 dwords each read COLUMN-MAJOR at stride
// 4. Both are the same 32x40 rect with +y UP and the origin at the wedge's base
// (v 24 sits at y 40 and v 64 at y 0); only the U range differs, and it matches
// the slot's own texU exactly - 0x20 for behaviours 0/1, 0 for behaviour 2 -
// which is what confirms the quad's u/v share the descriptor's texU/texV space.
// Stored in RING order (record order 0,1,3,2) because that is what the type-12
// corner fan expects; see the note in FlushSpriteCommandsRange.
struct ClLogoQuad {
    short x[4];
    short y[4];
    short u[4];
    short v[4];
};
static const ClLogoQuad s_logoWedgeQuad[2] = {
    // viewport slot 1 - behaviours 0 and 1
    { { -16, 16, 16, -16 }, { 40, 40, 0, 0 }, { 32, 64, 64, 32 }, { 24, 24, 64, 64 } },
    // viewport slot 2 - behaviour 2
    { { -16, 16, 16, -16 }, { 40, 40, 0, 0 }, {  0, 32, 32,  0 }, { 24, 24, 64, 64 } },
};

// umb02's page descriptor, and the tpage code the wordmark draws it with.
#define CL_LOGO_WEDGE_SLOT  0x28
#define CL_LOGO_WEDGE_TPAGE 0x0b

// Verified in-game: with f = g_sceneRenderParam = 589 and centre (160, 120) the
// mark's rotation origin projects to (160, 81) - the middle of the monitor,
// whose 120x104 image spans y 25..129 - and the UVs land exactly on the
// 32..64 / 24..64 pixel box the quad records describe. The substituted
// projection is therefore the one the Marni viewport was using.
static void cl_logo_draw_wedge(const ClLogoSlot* s, int quadIdx)
{
    if (g_SpriteQueueCount >= MAX_SPRITE_COMMANDS - 1) return;
    if (g_TexturePageSRV[CL_LOGO_WEDGE_SLOT] == MARNI_NULL_HANDLE) return;

    const int pageW = g_TexturePageWidth[CL_LOGO_WEDGE_SLOT];
    const int pageH = g_TexturePageHeight[CL_LOGO_WEDGE_SLOT];
    if (pageW <= 0 || pageH <= 0) return;

    // The three params the original computes from the slot's scale. The wedge
    // does not scale on screen - the shrink is expressed as a 3D position that
    // recedes, which is why params[2] (z) grows as scaleX falls.
    const int sc = (int)s->scaleX;
    const float px = (float)((sc - 0x1000) / 0x15);
    const float py = (float)((0x1000 - sc) / 0x1a + 0x50);
    const float pz = (float)((0x1000 - sc) / 2 + 0x4b0);
    if (pz <= 1.0f) return;

    // params[5] * (1/4096) turns.
    const float turns = (float)(s->angle * 0xb) * (1.0f / 4096.0f);
    const float th = turns * 6.2831853f;
    const float cs = cosf(th);
    const float sn = sinf(th);

    // Same projection the TMD pass uses - models go through the very viewport
    // FUN_0042a9e0 would have used, so this is the consistent substitution.
    // Kept in GAME space (320x240): FlushSpriteCommandsRange applies the
    // render scale itself.
    const float f  = (float)g_sceneRenderParam;
    const float cx = (float)g_SubpixelOffsetX;
    const float cy = (float)g_SubpixelOffsetY;

    // Page-relative pixel UVs, derived exactly as the sprite path does. Scale 2
    // is s_TexScale[(flags >> 24) & 3] for the 0x01000040 the draw pass sets.
    const int   uvScale  = 2;
    const int   pageOfs = (CL_LOGO_WEDGE_TPAGE - (int)g_TexturePageId[CL_LOGO_WEDGE_SLOT])
                           * uvScale * 0x40;
    const int   uBase    = pageOfs - (int)g_TexturePageOriginX[CL_LOGO_WEDGE_SLOT] * uvScale;
    const int   vBase    = -(int)g_TexturePageOriginY[CL_LOGO_WEDGE_SLOT];

    const ClLogoQuad* q = &s_logoWedgeQuad[quadIdx];

    short sxOut[4], syOut[4], suOut[4], svOut[4], wzOut[4];
    for (int k = 0; k < 4; k++) {
        // Object space -> row-scaled rotation -> translation. The row scales are
        // 2.0 on rows 0 and 1, so they multiply the object's x and y.
        //
        // MIND THE SIGNS. FUN_0048cb90 does not build a rotation matrix and
        // multiply by it - it rotates the matrix's three basis ROWS in place
        // through FUN_0048c9b0, which is
        //     x' = c*x + s*y      y' = c*y - s*x
        // Starting from the identity FUN_0048c8a0 lays down, that leaves
        //     row0 = ( c, -s, 0)      row1 = ( s, c, 0)
        // and v' = qx*row0 + qy*row1 + row3 is therefore the form below. Writing
        // it the other way round (qx*c - qy*s, qx*s + qy*c) is the same rotation
        // BACKWARDS, which spins the mark the wrong way - subtle while it is
        // assembling, because the eight wedges are near enough symmetric, and
        // obvious once it flies out off-centre.
        const float qx = (float)q->x[k] * 2.0f;
        const float qy = (float)q->y[k] * 2.0f;
        const float vx =  qx * cs + qy * sn + px;
        const float vy = -qx * sn + qy * cs + py;
        const float vz = pz;

        const float iz = f / vz;
        sxOut[k] = (short)(cx + vx * iz);
        syOut[k] = (short)(cy - vy * iz);   // view Y is up, screen Y is down
        wzOut[k] = (short)(vz > 30000.0f ? 30000.0f : vz);

        const int up = uBase + (int)q->u[k];
        const int vp = vBase + (int)q->v[k];
        suOut[k] = (short)(up * 4096 / pageW);
        svOut[k] = (short)(vp * 4096 / pageH);
    }

    TextureDraw* cmd = &g_SpriteCommandBuffer[g_SpriteQueueCount];
    cmd->type        = 12;
    // Same interleaved class as the rest of the terminal, so the forearms can
    // still come in front of it.
    cmd->sortClass   = SPRITE_CLASS_EFFECT;
    cmd->renderFlags = 0;
    cmd->x0 = sxOut[0]; cmd->y0 = syOut[0];
    cmd->x1 = sxOut[1]; cmd->y1 = syOut[1];
    cmd->x2 = sxOut[2]; cmd->y2 = syOut[2];
    cmd->x3 = sxOut[3]; cmd->y3 = syOut[3];
    cmd->u0 = suOut[0]; cmd->v0 = svOut[0];
    cmd->u1 = suOut[1]; cmd->v1 = svOut[1];
    cmd->u2 = suOut[2]; cmd->v2 = svOut[2];
    cmd->u3 = suOut[3]; cmd->v3 = svOut[3];
    cmd->wz0 = wzOut[0]; cmd->wz1 = wzOut[1];
    cmd->wz2 = wzOut[2]; cmd->wz3 = wzOut[3];
    // otKey = depthBase + 0x80, on the same depthSort scale as everything else.
    cmd->depthSort   = (unsigned int)(s->depthBase + 0x80) * 0x10 + 500;
    cmd->spriteFlags = 0;
    cmd->alpha       = 1.0f;             // OT +0x68 was 0 = opaque
    cmd->r = 1.0f;
    cmd->g = 1.0f;
    cmd->b = 1.0f;
    cmd->variantAlpha = 0.0f;
    cmd->extraFlags   = CL_LOGO_WEDGE_SLOT;

    g_SpriteQueueCount++;
}

// ============================================================================
// The draw pass (the second loop of 0x00488a60)
//
// The wordmark is an ordinary scaled sprite off page 0x19 (umb02, the page
// cl_load_textures puts there) and goes through AddSprite_Ex; behaviours 0 and 1
// draw wedge quad 0 and behaviour 2 draws quad 1.
// ============================================================================
static void cl_logo_draw(void)
{
    g_TextureDesc.clutX = 0;
    g_TextureDesc.flags = 0x01000040;

    for (int i = 0; i < CL_LOGO_SLOTS; i++) {
        ClLogoSlot* s = &s_logoSlots[i];
        if (s->active == 0) continue;
        if (s->behaviour != 3) {
            cl_logo_draw_wedge(s, (s->behaviour == 2) ? 1 : 0);
            continue;
        }

        g_TextureDesc.texturePage  = 0x0b;
        g_TextureDesc.clutY = 0x1ed;
        g_TextureDesc.pivotX = s->pivotX;
        g_TextureDesc.pivotY = s->pivotY;
        g_TextureDesc.texU   = s->texU;
        g_TextureDesc.texV   = s->texV;
        g_TextureDesc.width  = (unsigned short)s->width;
        g_TextureDesc.height = (unsigned short)s->height;
        g_TextureDesc.scaleX = s->scaleX;
        g_TextureDesc.scaleY = s->scaleY;
        // The descriptor's rotation field. Always 0 here - the wordmark never
        // spins - but it is the same global the wedges would drive, and no draw
        // path in this port reads it yet.
        unk_00be1180 = s->angle << 0x0c;
        g_TextureDesc.screenX = (short)(s->x >> 0x10);
        g_TextureDesc.screenY = (short)(s->y >> 0x10);
        g_TextureDesc.colorMulR = 0x80;
        g_TextureDesc.colorMulG = 0x80;
        g_TextureDesc.colorMulB = 0x80;
        AddSprite_Ex(&g_TextureDesc, 10, 0x19, 1);
    }
}

// ============================================================================
// cl_intro_logo_update (0x00488a60)
// ============================================================================
static void cl_intro_logo_update(void)
{
    if (s_introLogoState == 0) {
        // 0x00488a72: every slot is cleared by hand - only `active` and `sub`,
        // the rest is left over from the last run and re-initialised by each
        // behaviour's sub-state 0.
        for (int i = 0; i < CL_LOGO_SLOTS; i++) {
            s_logoSlots[i].active = 0;
            s_logoSlots[i].sub    = 0;
        }

        ClLogoSlot* hub = cl_logo_alloc();
        if (hub != NULL) {
            hub->behaviour = 0;
            hub->x = 0;
            hub->y = (int)0xffd50000;   // -43.0
        }

        play_sound_and_voice_effect(1, 0xb7);
        s_introLogoState++;
        g_main_state_flags |= MSF_VOICE_PLAYING;
    }

    for (int i = 0; i < CL_LOGO_SLOTS; i++) {
        ClLogoSlot* s = &s_logoSlots[i];
        if (s->active == 0) continue;
        if (s->behaviour < 4) s_logoBehaviours[s->behaviour](s);
    }

    cl_logo_draw();
}

// ============================================================================
// Window open/close animation (0x00413d20 / 0x00413d70 / 0x00413dc0)
//
// animCmd bit 0 starts the animation and bit 7 makes it a close. animState 0
// snaps to the start value, animState 1 ramps 8 (or 16 for a "big" window) per
// frame until the target is reached, and then clears both bytes.
// ============================================================================
static void cl_window_anim_begin(ClWindow* w)
{
    w->animState++;
    if ((w->animCmd & 0x80) != 0) {
        w->alpha   = (short)((w->big == 0) ? 0xff : 0x80);
        w->visible = 1;
    } else {
        w->alpha   = 0;
        w->visible = 1;
    }
}

static void cl_window_anim_step(ClWindow* w)
{
    const short step   = (short)((w->big == 0) ? 16 : 8);
    const short target = (short)((w->big == 0) ? 0xff : 0x80);

    if ((w->animCmd & 0x80) == 0) {
        w->alpha = (short)(w->alpha + step);
        if (w->alpha >= target) {
            w->animState++;
            w->alpha     = 0x80;
            w->visible   = 0;
            w->animCmd   = 0;
            w->animState = 0;
        }
    } else {
        w->alpha = (short)(w->alpha - step);
        if (w->alpha < 1) {
            w->animState++;
            w->alpha     = 0;
            w->animCmd   = 0;
            w->animState = 0;
        }
    }
}

static void cl_windows_update(void)
{
    for (int i = 0; i < 4; i++) {
        ClWindow* w = &s_win[i];
        if (w->enabled == 0) continue;
        if ((w->animCmd & 1) == 0) continue;
        if (w->animState == 0) cl_window_anim_begin(w);
        else                   cl_window_anim_step(w);
    }
}

// ============================================================================
// cl_typing_update (0x00413e70 + 0x00413ea0..0x00413ff0)
//
// Replays a finished text field one character at a time by driving the two
// forearms. s_armForKey picks which arm reaches and how; Chris (player id 0)
// gets a random 10-25 frame gap between keys while Jill (id 1) gets a fixed
// gap that depends on whether consecutive keys use the same hand.
// ============================================================================
static void cl_typer_begin(void)
{
    s_typer.state++;
    s_typer.charIdx = 0;
}

static void cl_typer_press(void)
{
    s_typer.state++;

    const unsigned char* line = s_text[s_typer.line].text;
    if (line[s_typer.charIdx] == 0) {
        s_typer.state = 3;
        g_EnemiesList[0].behavior_flags = 0x46;
        return;
    }

    const unsigned char arm = s_armForKey[line[s_typer.charIdx]];
    if ((arm & 0x80) == 0) g_EnemiesList[0].behavior_flags = (unsigned char)((arm & 0x3f) | 0x40);
    else                   g_EnemiesList[1].behavior_flags = (unsigned char)((arm & 0x3f) | 0x40);

    if ((g_playerEntity.id & 1) != 0) {
        // Same hand twice in a row is SLOWER than alternating hands. The
        // original is `(-(same) & 7) + 6`, i.e. 13 when the bit-7 arm bits
        // match and 6 when they differ (0x00413e70 / 0x00413ec0).
        const unsigned char next = s_armForKey[s_text[s_typer.line].text[s_typer.charIdx + 2]];
        s_typer.timer = (short)((((next ^ arm) & 0x80) == 0) ? 13 : 6);
    } else {
        s_typer.timer = (short)((rand() & 0xf) + 10);
    }
}

static void cl_typer_wait(void)
{
    s_typer.timer--;
    if (s_typer.timer != 0) return;

    s_typer.charIdx += (g_playerEntity.id & 1) + 1;
    if (s_text[s_typer.line].text[s_typer.charIdx] == 0) {
        s_typer.state = 3;
        g_EnemiesList[0].behavior_flags = 0x46;
        return;
    }
    s_typer.state = 1;
}

static void cl_typer_finish(void)
{
    if ((g_EnemiesList[0].behavior_flags & 0x20) != 0) {
        s_typer.active = 0;
        s_typer.state  = 0;
    }
}

static void cl_typing_update(void)
{
    if (s_typer.active == 0) return;
    switch (s_typer.state) {
    case 0:  cl_typer_begin();  break;
    case 1:  cl_typer_press();  break;
    case 2:  cl_typer_wait();   break;
    default: cl_typer_finish(); break;
    }
}

// ============================================================================
// cl_draw_quads (0x00414ad0)
//
// The terminal's whole UI is built from lists of these quads. Quads are walked
// BACK TO FRONT (the original advances to entry count-1 and decrements), a
// zero `repeat` counts as one, and `flags` bit 0 tiles down instead of across.
// `flags` bit 6 pushes the sprite 0x3ff further back in the sort. `texturePage`
// is the tpage the quads sample from (it also picks the CLUT row); the real
// sort keys are depthKeyA/depthKeyB.
// ============================================================================
static void cl_draw_quads(short baseX, short baseY, unsigned char flags,
                          unsigned char count, const ClQuad* quads,
                          unsigned short texturePage, short depthKeyA, int depthKeyB)
{
    g_TextureDesc.texturePage  = (short)texturePage;
    g_TextureDesc.clutX  = 0;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.clutY = (short)((int)(texturePage - 7) / 2 + 0x1eb);

    if (count == 0) return;

    const ClQuad* q = quads + (count - 1);
    for (unsigned char left = count; left != 0; left--, q--) {
        g_TextureDesc.flags =
            ((unsigned int)(q->flags & 0xc0) << 16) |
            ((unsigned int)(flags & 0x82 | 2) << 23);
        g_TextureDesc.screenX = (short)(q->x + baseX);
        g_TextureDesc.screenY = (short)(q->y + baseY);
        g_TextureDesc.texU    = q->texU;
        g_TextureDesc.texV    = q->texV;
        g_TextureDesc.width   = (unsigned short)q->width;
        g_TextureDesc.height  = (unsigned short)q->height;
        g_TextureDesc.colorMulR = s_colR;
        g_TextureDesc.colorMulG = s_colG;
        g_TextureDesc.colorMulB = s_colB;

        unsigned char reps = (unsigned char)(q->repeat + (q->repeat == 0));

        short  keyA = depthKeyA;
        int    keyB = depthKeyB;
        if ((flags & 0x40) != 0) { keyA = (short)(depthKeyA + 0x3ff); keyB = depthKeyB + 0x3ff; }

        if ((q->flags & 1) == 0) {
            g_TextureDesc.screenX = (short)(g_TextureDesc.screenX + (short)(reps - 1) * q->width);
            for (unsigned char i = reps; i != 0; i--) {
                SubmitEffectSprite_Ex(&g_TextureDesc, (unsigned short)keyA, 0x17, 2, keyB);
                g_TextureDesc.screenX = (short)(g_TextureDesc.screenX - q->width);
            }
        } else {
            g_TextureDesc.screenY = (short)(g_TextureDesc.screenY + (short)(reps - 1) * q->height);
            for (unsigned char i = reps; i != 0; i--) {
                SubmitEffectSprite_Ex(&g_TextureDesc, (unsigned short)keyA, 0x17, 2, keyB);
                g_TextureDesc.screenY = (short)(g_TextureDesc.screenY - q->height);
            }
        }
    }
}

// ============================================================================
// cl_draw_keyboard (0x00414300)
// The 8x4 key grid plus the highlight box over the selected key. ENTER is
// double height and BACKSPACE double width, which is why the two size fixups
// key off the cell index rather than a per-cell table.
// ============================================================================
static void cl_draw_keyboard(void)
{
    const unsigned char savedR = s_colR, savedG = s_colG, savedB = s_colB;

    if ((s_win[3].enabled & 0x7f) != 0 && s_win[3].alpha != 0) {
        const unsigned char bright = (unsigned char)s_win[3].alpha;

        g_TextureDesc.flags  = 0x1000000;
        g_TextureDesc.texturePage  = 9;
        g_TextureDesc.clutX  = 0;
        g_TextureDesc.width  = 0x18;
        g_TextureDesc.height = 0x18;
        g_TextureDesc.clutY = 0x1ec;
        g_TextureDesc.pivotX = 0;
        g_TextureDesc.pivotY = 0;
        s_colR = bright; s_colG = bright; s_colB = bright;

        const int col = (int)ST_COL;
        const int row = (int)ST_ROW;
        g_TextureDesc.screenX = (short)(col * 0x18 - 0x58);
        g_TextureDesc.screenY = (short)(row * 0x18 + 2);
        g_TextureDesc.texU    = (unsigned char)(col * 0x18 + 0x10);
        g_TextureDesc.texV    = (unsigned char)(row * 0x18 + 0x20);
        g_TextureDesc.colorMulR = bright;
        g_TextureDesc.colorMulG = bright;
        g_TextureDesc.colorMulB = bright;

        const short cell = (short)(row * 8 + col);
        if (cell == 0x17) g_TextureDesc.height = 0x30;   // ENTER
        if (cell == 0x1d) g_TextureDesc.width  = 0x30;   // BACKSPACE

        // Blink the highlight, but hold it solid for 20 frames after a move so
        // the cursor is never invisible while the player is navigating.
        if ((s_blinkCounter & 0x10) == 0 || s_blinkHold != 0) {
            SubmitEffectSprite_Ex(&g_TextureDesc, 10, 0x17, 2, 10);
        }
        if ((s_win[3].enabled & 0x80) != 0) s_blinkHold = 0x14;
        if (s_blinkHold != 0) s_blinkHold--;

        cl_draw_quads(-0x68, -0x0e, 0x8a, 10, s_keyboardFrame, 9, 10, 10);
    }

    s_colB = savedB;
    s_colG = savedG;
    s_colR = savedR;
}

// ============================================================================
// cl_draw_windows (0x004144c0)
//
// Windows 2, 1, 0 in that order (the original walks the array backwards from
// 0x004d6e00). In "simple" mode - the boot screens, before the terminal UI
// exists - each index instead draws one fixed photo.
// ============================================================================
static void cl_draw_windows(void)
{
    const unsigned char savedR = s_colR, savedG = s_colG, savedB = s_colB;

    for (int idx = 0; idx < 3; idx++) {
        ClWindow* w = &s_win[2 - idx];
        if (w->enabled == 0 || w->alpha == 0) continue;

        if (s_simpleMode == 0) {
            // Resize the 9-slice frame to this window before drawing it. The
            // ten patch addresses at 0x00414780..0x004147fd resolve, at stride
            // 12 from the 0x004b40c8 base, to:
            //   0x004b40d4 -> [1].x       0x004b411c -> [7].x
            //   0x004b40e2 -> [2].y       0x004b4106 -> [5].y
            //   0x004b40ec -> [3].x       0x004b40ee -> [3].y
            //   0x004b4103 -> [4].repeat  0x004b410f -> [5].repeat
            //   0x004b411b -> [6].repeat  0x004b4127 -> [7].repeat
            // 0x004b40ec/0x004b40ee are the BOTTOM-RIGHT CORNER's position, not
            // the top edge's size - writing them into [4].width/[4].height blew
            // the 16x16 top edge up to the full frame rectangle and then tiled
            // THAT `cols` times across the top. And [5].y / [7].x were missing
            // entirely, so the bottom edge drew on top of the top edge and the
            // right edge on top of the left one: no bottom or right border.
            const short fw = (short)((w->cols + 1) * 0x10);
            const short fh = (short)((w->rows + 1) * 0x10);
            s_frameQuads[1].x      = fw;
            s_frameQuads[7].x      = fw;
            s_frameQuads[2].y      = fh;
            s_frameQuads[5].y      = fh;
            s_frameQuads[3].x      = fw;
            s_frameQuads[3].y      = fh;
            s_frameQuads[4].repeat = (unsigned char)w->cols;
            s_frameQuads[5].repeat = (unsigned char)w->cols;
            s_frameQuads[6].repeat = (unsigned char)w->rows;
            s_frameQuads[7].repeat = (unsigned char)w->rows;

            s_colR = (unsigned char)w->alpha;
            s_colG = (unsigned char)w->alpha;
            s_colB = (unsigned char)w->alpha;

            const int key = (4 - idx) * 0x80;
            const unsigned char blend =
                (unsigned char)(((w->visible == 0) ? 0x00 : 0x80) | 0x0b) +
                (unsigned char)((5 - idx) * 10);
            cl_draw_quads(w->x, w->y, blend, 8, s_frameQuads, 10, (short)key, key);

            g_TextureDesc.texturePage  = 9;
            g_TextureDesc.clutX  = 0;
            g_TextureDesc.clutY = 0x1ec;
            g_TextureDesc.width  = 0x10;
            g_TextureDesc.height = 0x10;
            g_TextureDesc.pivotX = 0;
            g_TextureDesc.pivotY = 0;
            g_TextureDesc.flags  = ((w->visible == 0) ? 0xc0000000u : 0u) + 0x41000000u;
            g_TextureDesc.colorMulR = (unsigned char)w->alpha;
            g_TextureDesc.colorMulG = (unsigned char)w->alpha;
            g_TextureDesc.colorMulB = (unsigned char)w->alpha;

            const short rowLeft = w->x;
            g_TextureDesc.screenY = (short)(w->y + 0x10);

            const ClRow* row = w->rowData;
            for (int r = 0; r < (int)w->rowCount; r++, row++) {
                g_TextureDesc.screenX = (short)(rowLeft + 0x10);
                if (row->width != 0) {
                    g_TextureDesc.texU   = row->texU;
                    g_TextureDesc.texV   = row->texV;
                    g_TextureDesc.width  = (unsigned short)(row->width << 3);
                    g_TextureDesc.height = (unsigned short)(row->height << 4);
                    g_TextureDesc.texturePage  = row->texturePage;
                    g_TextureDesc.clutY = (short)(((int)(row->texturePage - 7) >> 1) + 0x1eb);
                    SubmitEffectSprite_Ex(&g_TextureDesc, (unsigned short)(idx * -0x80 + 0x210),
                                          0x17, 2, idx * -0x80 + 0x210);
                }

                // Pad the rest of the row out to the window width with the
                // blank cell so a short row still fills its line.
                const int pad = (int)w->cols * 2 - (int)row->width;
                if (row->width < (short)((unsigned short)w->cols * 2)) {
                    g_TextureDesc.texU   = 0xe0;
                    g_TextureDesc.texV   = 0x30;
                    g_TextureDesc.width  = 8;
                    g_TextureDesc.height = 0x10;
                    g_TextureDesc.texturePage  = 9;
                    g_TextureDesc.clutY = 0x1ec;
                    g_TextureDesc.screenX = (short)(rowLeft + 0x10 + row->width * 8);
                    for (int i = 0; i < pad; i++) {
                        SubmitEffectSprite_Ex(&g_TextureDesc,
                                              (unsigned short)((i + idx * -0x20) * 4 + 0x220),
                                              0x17, 2, (i + idx * -8 + 0x22) * 0x10);
                        g_TextureDesc.screenX = (short)(g_TextureDesc.screenX + 8);
                    }
                }
                g_TextureDesc.screenY = (short)(g_TextureDesc.screenY + g_TextureDesc.height);
            }
        } else if (idx == 0) {
            g_TextureDesc.screenX = -0x96;
            g_TextureDesc.screenY = -0x69;
            g_TextureDesc.texturePage   = 0;
            g_TextureDesc.clutX   = 0;
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.texU    = 0x59;
            g_TextureDesc.texV    = 0x5e;
            g_TextureDesc.width   = 0xa3;
            g_TextureDesc.height  = 0x50;
            g_TextureDesc.pivotX  = 0;
            g_TextureDesc.pivotY  = 0;
            g_TextureDesc.flags   = (w->visible == 0) ? 0u : 0x40000000u;
            g_TextureDesc.colorMulR = (unsigned char)w->alpha;
            g_TextureDesc.colorMulG = (unsigned char)w->alpha;
            g_TextureDesc.colorMulB = (unsigned char)w->alpha;
            SubmitEffectSprite_Ex(&g_TextureDesc, 10, 0x1c, 1000, 1000);

            g_TextureDesc.texU    = 0x80;
            g_TextureDesc.texV    = 0xaf;
            g_TextureDesc.screenX = 0x0d;
            g_TextureDesc.width   = 0x7e;
            SubmitEffectSprite_Ex(&g_TextureDesc, 100, 0x1c, 1, 1000);
        } else if (idx == 1) {
            g_TextureDesc.screenX = -0x10;
            g_TextureDesc.screenY = -0x39;
            g_TextureDesc.texturePage   = 0;
            g_TextureDesc.clutX   = 0;
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.texU    = 0x56;
            g_TextureDesc.texV    = 0x0a;
            g_TextureDesc.width   = 0x59;
            g_TextureDesc.height  = 0x50;
            g_TextureDesc.pivotX  = 0;
            g_TextureDesc.pivotY  = 0;
            g_TextureDesc.flags   = (w->visible == 0) ? 0u : 0x40000000u;
            g_TextureDesc.colorMulR = (unsigned char)w->alpha;
            g_TextureDesc.colorMulG = (unsigned char)w->alpha;
            g_TextureDesc.colorMulB = (unsigned char)w->alpha;
            SubmitEffectSprite_Ex(&g_TextureDesc, 0x50, 0x1c, 1, 800);
        } else {
            g_TextureDesc.screenX = 0x10;
            g_TextureDesc.screenY = -0x28;
            g_TextureDesc.texturePage   = 0;
            g_TextureDesc.clutX   = 0;
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.texU    = 0xb3;
            g_TextureDesc.texV    = 0x01;
            g_TextureDesc.width   = 0x4b;
            g_TextureDesc.height  = 0x5c;
            g_TextureDesc.pivotX  = 0;
            g_TextureDesc.pivotY  = 0;
            g_TextureDesc.flags   = (w->visible == 0) ? 0u : 0x40000000u;
            g_TextureDesc.colorMulR = (unsigned char)w->alpha;
            g_TextureDesc.colorMulG = (unsigned char)w->alpha;
            g_TextureDesc.colorMulB = (unsigned char)w->alpha;
            SubmitEffectSprite_Ex(&g_TextureDesc, 0x3c, 0x1c, 1, 600);
        }
    }

    s_colR = savedR;
    s_colG = savedG;
    s_colB = savedB;
}

// ============================================================================
// cl_draw_text (0x00414cd0)
// The typed characters. Glyph index is (code - 1) into a 16-wide sheet, so the
// column is (idx * 0x10) truncated to a byte and the row is idx & 0x10.
// Masked fields print a '*' through the 8x8 text layer instead.
// ============================================================================
static void cl_draw_text(void)
{
    g_TextureDesc.texturePage  = 9;
    g_TextureDesc.clutX  = 0;
    g_TextureDesc.clutY = 0x1ec;
    g_TextureDesc.width  = 0x10;
    g_TextureDesc.height = 0x10;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.pivotY = 0;
    sprintf(PRINT_TEXT_BUFFER, "%s", s_maskString);

    for (int r = 0; r < 2; r++) {
        ClText*   t = &s_text[r];
        ClWindow* w = &s_win[t->winIdx];

        g_TextureDesc.flags = ((w->visible == 0) ? 0xc0000000u : 0u) + 0x41000000u;

        if (t->enabled == 0 || w->enabled == 0 || w->animCmd != 0 || w->alpha == 0) continue;

        g_TextureDesc.screenX = (short)(t->x * 8 + 0x10 + w->x);
        g_TextureDesc.screenY = (short)((t->y + 1) * 0x10 + w->y);

        for (unsigned char i = 0; i < 8; i++) {
            const short sx = g_TextureDesc.screenX;
            const short sy = g_TextureDesc.screenY;
            if (t->text[i] != 0) {
                const unsigned char glyph = (unsigned char)(t->text[i] - 1);
                g_TextureDesc.texV = (unsigned char)(glyph & 0x10);
                g_TextureDesc.texU = (unsigned char)(glyph * 0x10);
                if (t->masked == 0) {
                    SubmitEffectSprite_Ex(&g_TextureDesc, 10, 0x17, 2, 10);
                } else {
                    sprintf(PRINT_TEXT_BUFFER, "%s", s_maskString);
                    PrintText8x8((short)(sx + 0xa4), (short)(sy + 0x7c), 0xf0, 0);
                    g_TextureDesc.texU   = 0xb0;
                    g_TextureDesc.texV   = 0x0f;
                    g_TextureDesc.texturePage  = 9;
                    g_TextureDesc.clutX  = 0;
                    g_TextureDesc.clutY = 0x1ec;
                    g_TextureDesc.width  = 0x10;
                    g_TextureDesc.height = 0x10;
                    g_TextureDesc.flags  = ((w->visible == 0) ? 0xc0000000u : 0u) + 0x41000000u;
                    g_TextureDesc.screenX = sx;
                    g_TextureDesc.screenY = sy;
                }
            }
            g_TextureDesc.screenX = (short)(g_TextureDesc.screenX + 0x10);
        }
    }
}

// ============================================================================
// cl_draw_caret (0x00414ee0)
// Two shapes share this function: the blinking block caret in a text field,
// and the highlight bar over the selected line of a menu.
// ============================================================================
static void cl_draw_caret(void)
{
    if (s_inputEnabled == 0) return;

    ClWindow* w = &s_win[s_caretWinIdx];
    if (w->enabled == 0) return;

    g_TextureDesc.clutX  = 0;
    g_TextureDesc.flags  = 0x01000040;
    g_TextureDesc.texturePage  = 9;
    g_TextureDesc.clutY = 0x1ec;
    g_TextureDesc.width  = 0x10;
    s_blinkCounter++;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.height = 0x10;
    g_TextureDesc.pivotY = 0;

    if (s_caretMode == 0) {
        g_TextureDesc.texU = 0xb0;
        g_TextureDesc.texV = 0x10;
        g_TextureDesc.screenX = (short)((s_caretPos * 2 + 2 + s_caretBaseX) * 8 + w->x);
        g_TextureDesc.screenY = (short)(w->y + (s_caretBaseY + 1) * 0x10);

        if ((s_blinkCounter & 0x10) != 0) {
            // Blink "off" half: a filled cell is left as-is so the character
            // underneath stays readable, an empty cell shows the underscore.
            if (s_text[s_textRecIdx].text[s_caretPos] != 0) return;
            g_TextureDesc.texV = 0x0f;
        }
        if (s_caretPos == 7 && s_text[s_textRecIdx].text[s_caretPos] != 0) {
            g_TextureDesc.texV   = 0x1f;
            g_TextureDesc.height = 1;
            g_TextureDesc.screenY = (short)(g_TextureDesc.screenY + 0x0f);
        }
        SubmitEffectSprite_Ex(&g_TextureDesc, 0x0f, 0x17, 2, 0x0f);
        return;
    }

    if ((s_blinkCounter & 0x10) != 0) return;

    if (s_caretWinIdx == 0) {
        g_TextureDesc.flags   = 0x40000000;
        g_TextureDesc.screenX = (short)(s_caretBaseX * 8 + 8 + s_win[0].x);
        g_TextureDesc.clutY = 0x1e0;
        g_TextureDesc.width   = 0x3b;
        g_TextureDesc.height  = 0x11;
        g_TextureDesc.screenY = (short)((s_caretPos + s_caretBaseY) * 0x10 + s_win[0].y + 0x0e);
        g_TextureDesc.texV    = 0xc6;
        g_TextureDesc.texturePage   = 0;
        g_TextureDesc.texU    = 0;
        g_TextureDesc.clutX   = 0;
        g_TextureDesc.pivotX  = 0;
        g_TextureDesc.pivotY  = 0;
        g_TextureDesc.colorMulR = 0x80;
        g_TextureDesc.colorMulG = 0x80;
        g_TextureDesc.colorMulB = 0x80;
        SubmitEffectSprite_Ex(&g_TextureDesc, 0x14, 0x1c, 1, 200);
    } else if (s_caretWinIdx == 1) {
        g_TextureDesc.flags   = 0x40000000;
        g_TextureDesc.screenX = (short)(s_caretBaseX * 8 + 8 + s_win[1].x);
        g_TextureDesc.clutY = 0x1e0;
        g_TextureDesc.width   = 0x49;
        g_TextureDesc.height  = 0x11;
        g_TextureDesc.screenY = (short)((s_caretPos + s_caretBaseY) * 0x12 + s_win[1].y + 0x0c);
        g_TextureDesc.texV    = 0xc6;
        g_TextureDesc.texturePage   = 0;
        g_TextureDesc.texU    = 0;
        g_TextureDesc.clutX   = 0;
        g_TextureDesc.pivotX  = 0;
        g_TextureDesc.pivotY  = 0;
        g_TextureDesc.colorMulR = 0x80;
        g_TextureDesc.colorMulG = 0x80;
        g_TextureDesc.colorMulB = 0x80;
        SubmitEffectSprite_Ex(&g_TextureDesc, 0x14, 0x1c, 1, 200);
    }
}

// ============================================================================
// cl_draw_status (0x004151c0)
// The banner strip under the window stack plus the full-screen photo behind it.
// ============================================================================
static void cl_draw_status(void)
{
    if (s_statusIdx != 0) {
        cl_draw_quads(-0x30, -0x3d, 0x7c,
                      (unsigned char)((s_statusIdx == 4) ? 2 : 1),
                      &s_statusBanners[s_statusIdx], 7, 1000, 1000);
    }

    if ((s_msgFlags & 3) == 0) return;

    if ((s_msgFlags & 3) == 2) {
        if ((s_msgFlags & 0x10) != 0)
            cl_draw_quads(-0x3c, -0x5f, 0x54, 2, s_stripA, 7, 0x5dc, 0x5dc);
        if ((s_msgFlags & 0x20) != 0)
            cl_draw_quads(-0x3c, -0x5f, 0x54, 3, s_stripB, 7, 0x5dc, 0x5dc);
    }

    cl_draw_quads(-0x3c, -0x5f, (unsigned char)((s_msgFlags & 0xf2) | 0x72), 1,
                  &s_msgPictures[s_msgFlags & 3], 7, 0x640, 0x640);
}

// ============================================================================
// cl_draw_door_picture (0x004152a0)
// The large "door released" diagram, once a door has been unlocked.
// ============================================================================
static void cl_draw_door_picture(void)
{
    if ((s_doorSel & 0x80) == 0) return;

    g_TextureDesc.screenX = -0x28;
    g_TextureDesc.screenY = -0x5c;
    g_TextureDesc.clutY = 0x1e0;
    g_TextureDesc.width  = 0x54;
    g_TextureDesc.height = 0x61;
    g_TextureDesc.texturePage  = 0;
    g_TextureDesc.flags  = 0;
    g_TextureDesc.clutX  = 0;
    g_TextureDesc.texU   = 0;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.texV   = 0;
    g_TextureDesc.pivotY = 0;

    if ((s_doorSel & 0x7f) == 0) {
        g_TextureDesc.texV    = 100;
        g_TextureDesc.screenX = -0x26;
        g_TextureDesc.screenY = -0x5d;
    }
    SubmitEffectSprite_Ex(&g_TextureDesc, 0x14, 0x1c, 1, 0x578);
}

// ============================================================================
// cl_draw_leds (0x00415340)
// The two indicator lamps on the console bezel. The second one flickers.
//
// These are the only part of the terminal that goes through display_texture
// (0x0046e8d0) rather than SubmitEffectSprite_Ex, and they are lamps on the
// machine standing in the room - so they belong in the SAME interleaved pass as
// the rest of the terminal, not in the flat 2D pass that runs after it. Lamp 1's
// depthSort is 0x4b4*16+500 = 19764, well behind the window frames at 4596-8948,
// and that ordering is only honoured if both are in one pass; left as
// SPRITE_CLASS_NORMAL the lamps painted over the windows and the overlays.
//
// (The original pushes a fifth dword at both call sites - 0x400 and 4 - and
// cleans up 0x14. display_texture only ever reads four parameters and derives
// depthSort from the second, so that fifth push is dead in the retail build.)
// ============================================================================
static void cl_draw_leds(void)
{
    g_TextureDesc.flags  = 0x41000040;
    g_TextureDesc.texturePage  = 7;
    g_TextureDesc.clutY = 0x1eb;
    g_TextureDesc.width  = 6;
    g_TextureDesc.height = 4;
    g_TextureDesc.clutX  = 0;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.pivotY = 0;

    if ((s_ledFlags & 1) != 0) {
        g_TextureDesc.texU = 0xe8;
        g_TextureDesc.texV = 0x88;
        g_TextureDesc.screenX = 0x6f;
        g_TextureDesc.screenY = -0x28;
        display_texture(&g_TextureDesc, 0x4b4, 0x17, 2, SPRITE_CLASS_EFFECT);
    }
    if ((s_ledFlags & 2) != 0 && (rand() & 1) != 0) {
        g_TextureDesc.texU = 0xe8;
        g_TextureDesc.texV = 0x98;
        g_TextureDesc.screenX = 0x6f;
        g_TextureDesc.screenY = -0x23;
        display_texture(&g_TextureDesc, 4, 0x17, 2, SPRITE_CLASS_EFFECT);
    }
}

// The full-screen monitor image, drawn scaled by g_TextureDesc.scaleX/scaleY so
// the boot sequence can do the CRT power-on stretch. 0x004125f5 / 0x0041399a.
static void cl_draw_monitor(void)
{
    g_TextureDesc.texturePage  = 7;
    g_TextureDesc.clutY = 0x1eb;
    g_TextureDesc.pivotX = 0x3c;
    g_TextureDesc.pivotY = 0x34;
    g_TextureDesc.screenY = -0x2b;
    g_TextureDesc.clutX  = 0;
    g_TextureDesc.screenX = 0;
    g_TextureDesc.texU   = 0;
    g_TextureDesc.height = 0x68;
    g_TextureDesc.width  = 0x78;
    g_TextureDesc.texV   = 0x68;
    AddSprite_Ex(&g_TextureDesc, 3, 0x17, 1);
}

// ============================================================================
// Stage 0 - boot: CRT power-on, then the Umbrella logo
// Step table T_c[0..3] @0x004b44d0.
// ============================================================================
static void cl_boot_zoom_start(void)
{
    ST_SUB3++;
    g_TextureDesc.scaleY = 100;
    g_TextureDesc.scaleX = 0;
    s_ledFlags   = 1;
    s_msgFlags   = 0x83;
    ST_TIMER_A   = 0;
}

static void cl_boot_zoom_wide(void)
{
    ST_TIMER_A = (short)(ST_TIMER_A + 0x800);
    g_TextureDesc.flags = 0x1000000;
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.scaleX = ST_TIMER_A;
    g_TextureDesc.scaleY = 100;
    if (ST_TIMER_A > 0xfff) {
        ST_SUB3++;
        ST_TIMER_A = 100;
    }
}

static void cl_boot_zoom_tall(void)
{
    ST_TIMER_A = (short)(ST_TIMER_A + 0x800);
    if (ST_TIMER_A > 0x1000) {
        ST_TIMER_A = 0x1000;
        ST_TIMER_B = 0x5a;
        ST_SUB3++;
        s_msgFlags = 0;
    }
    g_TextureDesc.flags  = 0x1000000;
    g_TextureDesc.scaleX = 0x1000;
    g_TextureDesc.scaleY = ST_TIMER_A;
}

static void cl_boot_flash(void)
{
    ST_TIMER_B--;
    g_TextureDesc.flags = (unsigned int)(((ST_TIMER_B & 1) == 0) ? 0x40000000u : 0u) + 0x1000000u;
    const unsigned char lvl = (unsigned char)((int)ST_TIMER_B / 2);
    g_TextureDesc.colorMulR = lvl;
    g_TextureDesc.colorMulG = lvl;
    g_TextureDesc.colorMulB = lvl;
    g_TextureDesc.scaleX = 0x1000;
    g_TextureDesc.scaleY = 0x1000;

    if (ST_TIMER_B == 0x3c) play_sfx(2, 0x17, 0);
    if (ST_TIMER_B < 0x1e)  s_ledFlags = 3;
    if (ST_TIMER_B == 0) {
        s_introLogoState = 0;
        s_ledFlags = 1;
        ST_SUB2++;
        ST_SUB3 = 0;
        // Holding confirm/cancel through the power-on skips the logo.
        if ((cl_dpad_held() & 0xc0) != 0) {
            s_msgFlags = 1;
            ST_SUB3    = 1;
            ST_TIMER_A = 0x1e;
        }
    }
}

static void (*const s_bootSteps[4])(void) = {
    cl_boot_zoom_start, cl_boot_zoom_wide, cl_boot_zoom_tall, cl_boot_flash,
};

// 0x00412870
static void cl_boot_logo(void)
{
    switch (ST_SUB3) {
    case 0:
        cl_intro_logo_update();
        if (s_introLogoState == 2) {
            s_msgFlags = 1;
            ST_SUB3++;
            ST_TIMER_A = 0x1e;
        }
        break;
    case 1:
        ST_TIMER_A--;
        if (ST_TIMER_A == 0) {
            ST_SUB3++;
            ST_TIMER_A = 0x1e;
            ST_TIMER_B = 0;
        }
        break;
    case 2:
        if (cl_fade_step(8, 2) != 0) {
            ST_TIMER_A--;
            if (ST_TIMER_A == 0) {
                ST_SUB++;
                ST_SUB2 = 0;
            }
        }
        break;
    default:
        break;
    }
}

// 0x004125d0
static void cl_stage_boot(void)
{
    if (ST_SUB2 == 0) {
        s_bootSteps[ST_SUB3 & 3]();
        cl_draw_monitor();
        return;
    }
    cl_boot_logo();
}

// ============================================================================
// Stage 1 - login. Steps 0..5 run under subSub 1, steps 6..10 under subSub 2;
// the original shares one table (T_d @0x004b44f0) between two dispatchers whose
// bases are 6 entries apart.
// ============================================================================

// 0x00412940 - also reached as T_c[4]
static void cl_login_init(void)
{
    cl_load_intro_texture();
    s_simpleMode = 0;
    ST_SUB2++;
    ST_SUB3 = 0;

    cl_window_setup(0, -0x90, -0x68, 0x10, 4, 3, s_rowsLogin);
    s_win[0].enabled    = 1;
    s_padRepeatTimer    = 0;
    s_rowsLogin[2].width = 0;     // password row hidden until the user is in
    s_text[0].enabled   = 0;
    s_win[0].animCmd    = 1;
    s_win[3].animCmd    = 1;
    s_text[0].winIdx    = 0;
    s_text[0].masked    = 0;
    s_text[1].masked    = 0;
    s_win[3].enabled    = 1;
    s_text[0].x         = 7;
    s_text[0].y         = 2;
    cl_text_begin(0);
    s_statusIdx = 0;
    ST_COL = 1;      // start on 'A', not on the EXIT key
    ST_ROW = 0;
    s_pendingCamDelay = 0;
    g_EnemiesList[0].behavior_flags = 0x47;
    g_EnemiesList[1].behavior_flags = 0x47;
    s_pendingCamera = 0x85;
}

static void cl_login_wait_open(void)
{
    if (s_win[0].animCmd == 0 && s_win[3].animCmd == 0) {
        ST_SUB3++;
        ST_TIMER_A     = 0;      // also: the text field the keyboard writes to
        s_text[0].enabled = 1;
        s_inputEnabled = 1;
        s_caretMode    = 0;
    }
}

// Steps 1 and 3: read the keyboard into the current field.
static void cl_login_typing(void)
{
    S_CANCELLED = 0;
    const short key = cl_keyboard_input();

    if (key == -1) {                    // ENTER
        ST_SUB3++;
        return;
    }
    if (key == -2 || (cl_dpad_pressed() & 0x80) != 0) {   // EXIT / cancel
        ST_SUB3++;
        S_CANCELLED = 1;
        return;
    }
    if (key != 0) {
        cl_text_append(key, s_text[ST_TIMER_A].text);
        if (s_caretPos != 7) s_blinkCounter = 0;
    }
}

static void cl_login_to_password(void)
{
    if (S_CANCELLED != 0) { cl_set_state_word(2); return; }

    ST_SUB3++;
    s_text[1].y        = 3;
    s_text[1].enabled  = 1;
    s_rowsLogin[2].width = 0xc;     // reveal the password row
    s_text[1].x        = 0xc;
    s_text[1].masked   = 1;
    s_text[1].winIdx   = 0;
    ST_TIMER_A         = 1;
    cl_text_begin(1);
}

static void cl_login_submit(void)
{
    if (S_CANCELLED != 0) { cl_set_state_word(2); return; }

    ST_SUB3++;
    s_inputEnabled    = 0;
    s_pendingCamDelay = 0;
    s_win[0].animCmd  = 0x81;
    s_msgFlags        = 2;
    s_pendingCamera   = 0x84;
    s_win[3].animCmd  = 0x81;
    g_EnemiesList[0].behavior_flags = 0x40;
    g_EnemiesList[1].behavior_flags = 0x40;
}

static void cl_login_wait_close(void)
{
    if (s_win[0].animCmd == 0 && s_win[3].animCmd == 0) {
        ST_SUB2++;
        ST_SUB3 = 0;
        s_text[0].enabled = 0;
        s_text[1].enabled = 0;
        s_win[0].enabled  = 0;
        s_win[3].enabled  = 0;
    }
}

// Step 6: replay line 0 with the hands.
static void cl_login_type_line(void)
{
    ST_SUB3++;
    s_typer.line   = 0;
    s_typer.active = 1;
}

// Step 7: after each line finishes, start the next; two lines then validate.
static void cl_login_next_line(void)
{
    if (s_typer.active != 0) return;
    s_typer.line++;
    if (s_typer.line == 2) { ST_SUB3++; return; }
    s_typer.active = 1;
}

// Step 8: validate.
static void cl_login_validate(void)
{
    const bool userOk = strcmp((const char*)s_text[0].text, (const char*)s_expectUser) == 0;
    const bool passOk = userOk &&
                        strcmp((const char*)s_text[1].text, (const char*)s_expectPass) == 0;
    if (passOk) {
        ST_SUB3    = 4;
        ST_TIMER_A = 0x5a;
        return;
    }
    ST_SUB3     = 3;
    s_statusIdx = 4;
    ST_TIMER_A  = 0x5a;
    play_sfx(2, 0x19, 0);
}

// Step 9: rejected - hold the error, then go round again.
static void cl_login_rejected(void)
{
    if (ST_TIMER_A > 0) ST_TIMER_A--;
    if (ST_TIMER_A == 0 &&
        (g_EnemiesList[0].behavior_flags & 0x20) != 0 &&
        (g_EnemiesList[1].behavior_flags & 0x20) != 0) {
        ST_SUB2 = 0;
        ST_SUB3 = 0;
    }
    if (ST_TIMER_A == 0x4b) {
        g_EnemiesList[0].behavior_flags = 0x44;
        g_EnemiesList[1].behavior_flags = 0x44;
    }
}

// Step 10: accepted - hold "access granted", then open the menu.
static void cl_login_accepted(void)
{
    if (ST_TIMER_A > 0) ST_TIMER_A--;
    if (ST_TIMER_A == 0) {
        ST_SUB++;
        ST_SUB2 = 0;
    }
    if (ST_TIMER_A > 0x32) {
        cl_draw_quads(-0x3c, -0x5f, 3, 1, s_stripA, 7, 10, 10);
        return;
    }
    s_msgFlags |= 0x10;
}

static void (*const s_loginSteps[11])(void) = {
    cl_login_wait_open,     // 0
    cl_login_typing,        // 1
    cl_login_to_password,   // 2
    cl_login_typing,        // 3
    cl_login_submit,        // 4
    cl_login_wait_close,    // 5
    cl_login_type_line,     // 6
    cl_login_next_line,     // 7
    cl_login_validate,      // 8
    cl_login_rejected,      // 9
    cl_login_accepted,      // 10
};

// 0x00412920
static void cl_stage_login(void)
{
    switch (ST_SUB2) {
    case 0:  cl_login_init(); break;
    case 1:  s_loginSteps[(ST_SUB3 <= 10) ? ST_SUB3 : 10](); break;
    default: s_loginSteps[6 + ((ST_SUB3 <= 4) ? ST_SUB3 : 4)](); break;
    }
}

// ============================================================================
// Stage 2 - the door menu. Step table T_e[0..8] @0x004b4520.
// ============================================================================
static void cl_menu_open(void)
{
    if (ST_TIMER_A != 0) {
        ST_TIMER_A--;
        if (ST_TIMER_A == 0) {
            s_win[1].enabled = 1;
            s_win[1].animCmd = 1;
        }
        return;
    }
    if (s_win[0].animCmd == 0 && s_win[1].animCmd == 0) {
        ST_SUB3++;
        g_EnemiesList[0].behavior_flags = 0x47;
        g_EnemiesList[1].behavior_flags = 0x47;
        s_inputEnabled = 1;
    }
}

static void cl_menu_select(void)
{
    if ((cl_dpad_pressed() & 0x40) != 0) {
        if (s_caretPos == 0)      { s_doorSel = 0; ST_SUB3 = 4; }
        else if (s_caretPos == 1) { s_doorSel = 1; ST_SUB3 = 4; }
        else if (s_caretPos == 2) { ST_SUB3 = 2; s_inputEnabled = 0; }
    }
    if ((cl_dpad_pressed() & 0x80) != 0) {
        ST_SUB3 = 2;
        s_inputEnabled = 0;
    }
}

static void cl_menu_to_command(void)
{
    ST_SUB3++;
    cl_window_setup(0, 0x10, -0x28, 3, 4, 4, s_rowsMenu);
    s_win[0].rowData = s_rowsMenu;
    s_caretBaseX     = 0;
    s_win[0].enabled = 1;
    s_inputEnabled   = 1;
    s_caretWinIdx    = 0;
    s_caretPos       = 0;
    s_caretMode      = 1;
    s_blinkCounter   = 0;
    s_textRecIdx     = 4;
    s_win[0].alpha   = 0x80;
    s_caretBaseY     = 2;
    ST_TIMER_B       = 1;
    g_EnemiesList[0].behavior_flags = 0x40;
}

static void cl_menu_confirm_exit(void)
{
    bool leave = (cl_dpad_pressed() & 0x80) != 0;
    if ((cl_dpad_pressed() & 0x40) != 0) {
        if (s_caretPos == 0) { cl_set_state_word(2); return; }   // quit the terminal
        leave = true;
    }
    if (!leave) return;

    ST_SUB3        = 1;
    s_caretBaseX   = 0;
    s_caretWinIdx  = 1;
    s_caretBaseY   = 0;
    s_caretPos     = 0;
    s_caretMode    = 1;
    s_blinkCounter = 0;
    s_win[0].enabled = 0;
    s_inputEnabled = 1;
    g_EnemiesList[0].behavior_flags = 0x47;
    s_textRecIdx   = 5;
    g_EnemiesList[1].behavior_flags = 0x47;
    ST_TIMER_B     = 2;
}

static void cl_menu_close(void)
{
    ST_SUB3++;
    s_inputEnabled   = 0;
    s_msgFlags       = 0x12;
    s_win[1].animCmd = 0x81;
    ST_TIMER_A       = 0x0c;
    s_pendingCamera  = 0x84;
}

static void cl_menu_wait_close(void)
{
    if (ST_TIMER_A != 0) {
        ST_TIMER_A--;
        if (ST_TIMER_A == 0) s_win[2].animCmd = 0x81;
        return;
    }
    if (s_win[1].animCmd == 0 && s_win[2].animCmd == 0) {
        ST_SUB3++;
        s_win[1].enabled = 0;
        s_win[2].enabled = 0;
        ST_TIMER_A = 0x5a;
        if (s_doorSel == 0) {
            g_EnemiesList[0].behavior_flags = 0x46;
            ST_TIMER_B = 1;
            return;
        }
        g_EnemiesList[0].behavior_flags = 0x48;
        ST_TIMER_B = 2;
    }
}

static void cl_menu_arm_settle(void)
{
    if ((g_EnemiesList[0].behavior_flags & 0x20) == 0) return;
    ST_TIMER_B--;
    if (ST_TIMER_B != 0) {
        g_EnemiesList[0].behavior_flags = 0x46;
        return;
    }
    ST_SUB3++;
    s_ledFlags |= 2;
    g_EnemiesList[1].behavior_flags = 0x47;
}

static void cl_menu_blink(void)
{
    ST_TIMER_A--;
    if (ST_TIMER_A != 0) {
        s_statusIdx = (unsigned char)(((unsigned short)ST_TIMER_A & 0x10) == 0);
        return;
    }
    if (s_doorSel == 1) {
        ST_SUB  = 4;
        ST_SUB2 = 0;
        return;
    }
    ST_SUB3++;
    ST_TIMER_A  = 0x1e;
    ST_TIMER_B  = 0;
    s_statusIdx = 0;
}

static void cl_menu_countdown(void)
{
    ST_TIMER_A--;
    if (ST_TIMER_A == 0) {
        ST_TIMER_A = 0x1e;
        ST_TIMER_B++;
        if (ST_TIMER_B == 3) {
            ST_SUB     = 3;
            ST_SUB2    = 0;
            ST_TIMER_A = 0x1e;
            ST_TIMER_B = 0;
            s_msgFlags = 0x32;
        }
    }
    if (ST_TIMER_B != 0) {
        cl_draw_quads(-0x3c, -0x5f, 0x40, (unsigned char)ST_TIMER_B, s_stripB, 7, 0, 0);
    }
}

static void (*const s_menuSteps[9])(void) = {
    cl_menu_open,         // 0
    cl_menu_select,       // 1
    cl_menu_to_command,   // 2
    cl_menu_confirm_exit, // 3
    cl_menu_close,        // 4
    cl_menu_wait_close,   // 5
    cl_menu_arm_settle,   // 6
    cl_menu_blink,        // 7
    cl_menu_countdown,    // 8
};

// 0x00412df0
static void cl_stage_menu(void)
{
    if (ST_SUB2 == 0) {
        s_simpleMode = 1;
        ST_SUB2++;
        ST_SUB3 = 0;

        cl_window_setup(1, -0x10, -0x38, 4, 3, 3, s_rowsSide);
        s_win[1].enabled = 0;
        s_win[1].animCmd = 0;
        cl_window_setup(2, -0x90, -0x68, 0x10, 3, 3, s_rowsPanelA);
        s_caretBaseX   = 0;
        s_caretBaseY   = 0;
        s_caretPos     = 0;
        s_win[2].enabled = 1;
        s_win[2].animCmd = 1;
        s_blinkCounter = 0;
        s_inputEnabled = 0;
        s_text[0].enabled = 0;
        s_text[1].enabled = 0;
        s_pendingCamDelay = 0;
        s_statusIdx    = 0;
        s_caretWinIdx  = 1;
        s_caretMode    = 1;
        s_textRecIdx   = 5;
        s_pendingCamera = 0x85;
        ST_TIMER_A     = 0x0c;
        ST_TIMER_B     = 2;
        return;
    }

    // Menu navigation is live in every step, not just the select step.
    if (s_inputEnabled != 0) {
        if ((cl_pad_held() & 0x10) != 0 && s_caretPos != 0) {
            s_blinkCounter = 0;
            s_caretPos--;
        }
        if ((cl_pad_held() & 0x40) != 0 && (short)s_caretPos < ST_TIMER_B) {
            s_blinkCounter = 0;
            s_caretPos++;
        }
    }

    s_menuSteps[(ST_SUB3 <= 8) ? ST_SUB3 : 8]();
}

// ============================================================================
// Stage 3 - the release-command prompt, and stage 4 - the unlocked screen.
// Step table T_g[0..8] @0x004b4558; stage 4 enters it at index 6.
// ============================================================================
static void cl_cmd_close_windows(void)
{
    ST_SUB3++;
    s_win[2].animCmd = 0x81;
    s_win[3].animCmd = 0x81;
    s_inputEnabled   = 0;
    s_msgFlags       = 0x32;
    g_EnemiesList[0].behavior_flags = 0x40;
    g_EnemiesList[1].behavior_flags = 0x40;
}

static void cl_cmd_after_close(void)
{
    if (s_win[2].animCmd != 0 || s_win[3].animCmd != 0) return;
    ST_SUB3++;
    s_win[2].enabled = 0;
    s_win[3].animCmd = 0;
    s_win[3].enabled = 0;
    if (S_CANCELLED == 0) {
        s_typer.active = 1;
        s_typer.line   = 0;
    } else {
        g_EnemiesList[1].behavior_flags = 0x46;
    }
    s_pendingCamDelay = 2;
    s_pendingCamera   = 0x84;
}

static void cl_cmd_wait_typing(void)
{
    if (S_CANCELLED == 0) {
        if (s_typer.active == 0) ST_SUB3++;
        return;
    }
    if ((g_EnemiesList[1].behavior_flags & 0x20) != 0) cl_set_state_word(0x201);
}

static void cl_cmd_validate(void)
{
    if (strcmp((const char*)s_text[0].text, (const char*)s_expectCmd) != 0) {
        ST_SUB3     = 4;
        ST_TIMER_A  = 0x5a;
        s_statusIdx = 2;
        play_sfx(2, 0x19, 0);
        return;
    }
    ST_SUB3     = 5;
    ST_TIMER_A  = 0x5a;
    s_statusIdx = 3;
    s_ledFlags |= 2;
}

static void cl_cmd_rejected(void)
{
    if (ST_TIMER_A > 0) ST_TIMER_A--;
    if (ST_TIMER_A == 0 &&
        (g_EnemiesList[0].behavior_flags & 0x20) != 0 &&
        (g_EnemiesList[1].behavior_flags & 0x20) != 0) {
        ST_SUB  = 2;
        ST_SUB2 = 0;
    }
    if (ST_TIMER_A == 0x4b) {
        g_EnemiesList[0].behavior_flags = 0x44;
        g_EnemiesList[1].behavior_flags = 0x44;
    }
}

static void cl_cmd_accepted(void)
{
    if (ST_TIMER_A > 0) ST_TIMER_A--;
    if (ST_TIMER_A == 0) {
        ST_SUB++;
        ST_SUB2    = 0;
        s_ledFlags = 1;
        s_text[0].enabled = 0;
    }
}

// Step 6 - the actual unlock. This is the point of the whole sequence.
static void cl_cmd_unlock(void)
{
    ST_SUB2++;
    ST_SUB3         = 0;
    s_pendingCamera = 0x84;
    s_statusIdx     = 0;
    s_ledFlags      = 1;
    s_doorSel      |= 0x80;
    ST_TIMER_A      = 0x5a;

    // set_room_item_seen_flag(0x004885a0) inlined - the original pushes the bare
    // constant 5, which is the MAP INDEX of the lab map (item ITEM_MAP_LABORATORY,
    // 0x53 - ITEM_MAP_FIRST). The lab map has no item model anywhere in the RDTs,
    // so this terminal unlock is the only thing that ever raises its bit.
    Flg_on((int)g_RoomFlags, ROOM_FLAG_MAP_BASE + MAP_INDEX_LABORATORY);

    const int lockBit = 0x26 - (((s_doorSel & 0x7f) == 0) ? 1 : 0);
    if (Flg_ck((int)g_LocksFlags, lockBit) != 0) {
        // Already released on an earlier visit - skip the celebration.
        ST_TIMER_A = 0x20;
        return;
    }
    Flg_on((int)g_LocksFlags, lockBit);
    g_EnemiesList[0].behavior_flags = 0x45;
    g_EnemiesList[1].behavior_flags = 0x45;
}

static void cl_cmd_unlock_hold(void)
{
    ST_TIMER_A--;
    if (ST_TIMER_A == 0) {
        ST_SUB2++;
        ST_TIMER_A = 0x10;
    }
}

static void cl_cmd_unlock_prompt(void)
{
    if (ST_SUB3 == 0) {
        ST_SUB3    = 1;
        ST_TIMER_B = 0x96;
    }
    ST_TIMER_B--;
    if (ST_TIMER_B != 0 && (cl_dpad_pressed() & 0xc0) == 0) {
        g_TextureDesc.texturePage  = 0;
        g_TextureDesc.flags  = 0;
        g_TextureDesc.screenX = -0x38;
        g_TextureDesc.screenY = -0x40;
        g_TextureDesc.texU   = 3;
        g_TextureDesc.texV   = 0xd7;
        g_TextureDesc.clutY = 0x1e0;
        g_TextureDesc.width  = 0x72;
        g_TextureDesc.clutX  = 0;
        g_TextureDesc.pivotX = 0;
        g_TextureDesc.pivotY = 0;
        g_TextureDesc.height = 0x29;
        SubmitEffectSprite_Ex(&g_TextureDesc, 10, 0x1c, 1, 1000);
        return;
    }
    cl_set_state_word(0x201);
    s_win[0].enabled = 0;
    s_doorSel = 0;
}

static void (*const s_cmdSteps[9])(void) = {
    cl_cmd_close_windows,  // 0
    cl_cmd_after_close,    // 1
    cl_cmd_wait_typing,    // 2
    cl_cmd_validate,       // 3
    cl_cmd_rejected,       // 4
    cl_cmd_accepted,       // 5
    cl_cmd_unlock,         // 6
    cl_cmd_unlock_hold,    // 7
    cl_cmd_unlock_prompt,  // 8
};

// 0x00413380
static void cl_stage_command(void)
{
    if (ST_SUB2 == 0) {
        s_simpleMode = 0;
        ST_TIMER_A--;
        if (ST_TIMER_A != 0) return;

        ST_SUB2++;
        ST_SUB3    = 0;
        s_ledFlags = 1;
        cl_window_setup(2, -0x90, -0x68, 0x10, 4, 4, s_rowsPanelB);
        s_win[2].enabled = 1;
        s_win[2].animCmd = 1;
        s_win[3].animCmd = 1;
        s_text[0].enabled = 1;
        s_text[0].masked  = 1;
        s_text[0].winIdx  = 2;
        s_text[0].x       = 0x0c;
        s_text[0].y       = 3;
        cl_text_begin(0);
        s_win[3].enabled = 1;
        s_caretMode      = 0;
        s_inputEnabled   = 0;
        s_padRepeatTimer = 0;
        s_pendingCamera  = 0x85;
        ST_COL     = 1;
        ST_ROW     = 0;
        ST_TIMER_A = 0;
        play_sfx(2, 0x19, 0);
        g_EnemiesList[0].behavior_flags = 0x47;
        g_EnemiesList[1].behavior_flags = 0x47;
        return;
    }

    if (ST_SUB2 == 1) {
        if (s_win[2].animCmd != 0 || s_win[3].animCmd != 0) return;
        s_inputEnabled = 1;
        if (ST_SUB3 != 0) {
            ST_SUB2++;
            ST_SUB3 = 0;
            return;
        }
        cl_login_typing();      // shared with the login stage (T_d[1])
        return;
    }

    s_cmdSteps[(ST_SUB3 <= 8) ? ST_SUB3 : 8]();
}

// 0x00413710
static void cl_stage_unlocked(void)
{
    s_cmdSteps[6 + ((ST_SUB2 <= 2) ? ST_SUB2 : 2)]();

    const int door = ((s_doorSel & 0x7f) == 0) ? 0 : 1;
    if ((ST_TIMER_A & 0x10) != 0) {
        cl_draw_quads((short)(s_doorOrigin[door][0] - 0x3c),
                      (short)(s_doorOrigin[door][1] - 0x5f),
                      5, 1, &s_doorMarker[door], 10, 0x4b0, 0x4b0);
    }
}

// ============================================================================
// Top-level state functions
// ============================================================================
static void (*const s_stages[5])(void) = {
    cl_stage_boot, cl_stage_login, cl_stage_menu, cl_stage_command, cl_stage_unlocked,
};

// 0x004123d0
static void computer_lab_init(void)
{
    cl_set_state_word(1);                 // funcIndex = 1, sub-states cleared

    g_interactiveScreenSavedCameraId = g_roomCameraId;
    g_roomCameraId = 4;
    check_camera_switch(1);

    g_message_flags = (unsigned short)(g_message_flags & 0xfff8);
    g_message_flags = (unsigned short)(g_message_flags & 0xfeff);

    g_labSlidesSavedPlayerY = g_playerEntity.scaMatrixData.localMatrix.t[1];
    g_playerEntity.scaMatrixData.localMatrix.t[1] = -9000;

    for (int i = 0; i < 3; i++) s_savedLight[i] = g_RdtPointer->lights[i];
    s_savedAmbient[0] = (int)(unsigned short)g_RdtPointer->ambient_light_r;
    s_savedAmbient[1] = (int)(unsigned short)g_RdtPointer->ambient_light_g;
    s_savedAmbient[2] = (int)(unsigned short)g_RdtPointer->ambient_light_b;

    for (int i = 0; i < 3; i++) g_RdtPointer->lights[i] = s_labLights[i];
    g_RdtPointer->ambient_light_r = 0x708;
    g_RdtPointer->ambient_light_g = 0x708;
    g_RdtPointer->ambient_light_b = 0x708;
    setBackColor((unsigned short)g_RdtPointer->ambient_light_r,
                 (unsigned short)g_RdtPointer->ambient_light_g,
                 (unsigned short)g_RdtPointer->ambient_light_b);

    s_doorSel      = 0;
    s_inputEnabled = 0;
    for (int i = 0; i < 4; i++) {
        s_win[i].enabled = 0;
        s_win[i].animCmd = 0;
        s_win[i].alpha   = 0;
        s_win[i].big     = 0;
    }
    s_win[3].big = 1;
    for (int i = 0; i < 2; i++) s_text[i].enabled = 0;

    s_colB = 0x80;
    s_colG = 0x80;
    s_colR = 0x80;

    g_EnemiesList[0].behavior_flags = 0x40;
    g_EnemiesList[1].behavior_flags = 0x40;

    cl_load_textures();
    s_simpleMode = 0;
}

// 0x004125b0
static void computer_lab_update(void)
{
    s_stages[(ST_SUB <= 4) ? ST_SUB : 4]();
    cl_windows_update();
    cl_typing_update();
}

// 0x00413a00
static void cl_finish_start(void)
{
    if (ST_SUB3 == 0) {
        ST_SUB3 = 1;
        for (int i = 0; i < 4; i++) {
            s_win[i].animCmd = 0x81;
            if (s_win[i].enabled == 0) s_win[i].animCmd = 0;
        }
        // "LOGOUT" typed by the hands as the terminal closes.
        s_text[0].text[0] = 0x13;
        s_text[0].text[1] = 0x08;
        s_text[0].text[2] = 0x15;
        s_text[0].text[3] = 0x14;
        s_typer.active    = 1;
        s_typer.line      = 0;
        s_inputEnabled    = 0;
        s_pendingCamDelay = 0;
        s_pendingCamera   = 0x84;
        s_text[0].text[4] = 0;
        return;
    }

    if (ST_SUB3 == 1) {
        if (s_typer.active == 0) {
            ST_TIMER_B = 0;
            ST_SUB3++;
            s_ledFlags = 3;
        }
        return;
    }
    if (ST_SUB3 != 2) return;

    cl_fade_step(8, 0);
    if (ST_TIMER_B != 0 &&
        s_win[0].animCmd == 0 && s_win[1].animCmd == 0 &&
        s_win[2].animCmd == 0 && s_win[3].animCmd == 0) {
        for (int i = 0; i < 4; i++) s_win[i].enabled = 0;
        ST_SUB2++;
        g_TextureDesc.scaleX = 0x1000;
        g_TextureDesc.scaleY = 0x1000;
        s_msgFlags = 0;
        ST_TIMER_A = 0x1000;
        s_ledFlags = 0;
    }
}

// 0x00413b50
static void cl_finish_shrink_v(void)
{
    ST_TIMER_A = (short)(ST_TIMER_A - 2000);
    g_TextureDesc.scaleX = 0x1000;
    g_TextureDesc.scaleY = ST_TIMER_A;
    if (ST_TIMER_A < 0x65) {
        ST_SUB2++;
        ST_TIMER_A = 0x1000;
    }
}

// 0x00413ba0
static void cl_finish_shrink_h(void)
{
    ST_TIMER_A = (short)(ST_TIMER_A - 0x800);
    g_TextureDesc.flags = 0x1000000;
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.scaleX = ST_TIMER_A;
    g_TextureDesc.scaleY = 0x60;
    if (ST_TIMER_A < 1) {
        g_TextureDesc.scaleX = 0;
        ST_SUB++;
        ST_TIMER_A = 0x1e;
    }
}

static void (*const s_finishSteps[3])(void) = {
    cl_finish_start, cl_finish_shrink_v, cl_finish_shrink_h,
};

// 0x00413920
static void computer_lab_finish(void)
{
    if (ST_SUB == 0) {
        s_finishSteps[(ST_SUB2 <= 2) ? ST_SUB2 : 2]();
        cl_typing_update();
        cl_windows_update();
        if (ST_SUB2 != 0) {
            g_TextureDesc.clutX = 0;
            g_TextureDesc.colorMulR = 0x80;
            g_TextureDesc.colorMulG = 0x80;
            g_TextureDesc.flags = 0x5000000;
            g_TextureDesc.texturePage = 7;
            g_TextureDesc.colorMulB = 0x80;
            g_TextureDesc.clutY = 0x1eb;
            g_TextureDesc.screenX = 0;
            g_TextureDesc.texU    = 0;
            g_TextureDesc.pivotX  = 0x3c;
            g_TextureDesc.height  = 0x68;
            g_TextureDesc.texV    = 0x68;
            g_TextureDesc.pivotY  = 0x34;
            g_TextureDesc.screenY = -0x2b;
            g_TextureDesc.width   = 0x78;
            AddSprite_Ex(&g_TextureDesc, 3, 0x17, 1);
        }
        return;
    }

    ST_TIMER_A--;
    if (ST_TIMER_A != 0) return;

    for (int i = 0; i < 3; i++) g_RdtPointer->lights[i] = s_savedLight[i];
    g_RdtPointer->ambient_light_r = (short)s_savedAmbient[0];
    g_RdtPointer->ambient_light_g = (short)s_savedAmbient[1];
    g_RdtPointer->ambient_light_b = (short)s_savedAmbient[2];
    setBackColor((unsigned short)g_RdtPointer->ambient_light_r,
                 (unsigned short)g_RdtPointer->ambient_light_g,
                 (unsigned short)g_RdtPointer->ambient_light_b);

    g_EnemiesList[0].behavior_flags = 0x80;
    g_EnemiesList[1].behavior_flags = 0x80;

    ST_FUNC = 0;
    g_message_flags = (unsigned short)(g_message_flags | 0x0107);

    cl_free_textures();
    g_roomCameraId = g_interactiveScreenSavedCameraId;
    check_camera_switch(1);
    FUN_00473f10((int*)g_ScenarioFlags, SCENARIO_FLAG_INTERACTIVE_SCREEN);
    g_playerEntity.scaMatrixData.localMatrix.t[1] = g_labSlidesSavedPlayerY;
}

// ============================================================================
// display_computer_lab (0x00412390)
// ============================================================================
void display_computer_lab(void)
{
    switch (ST_FUNC) {
    case 0:  computer_lab_init();   break;
    case 1:  computer_lab_update(); break;
    case 2:  computer_lab_finish(); break;
    default: break;
    }

    cl_draw_caret();
    cl_draw_text();
    cl_draw_windows();
    cl_draw_keyboard();
    cl_draw_door_picture();
    cl_draw_status();
    cl_draw_leds();
    cl_camera_step();
}
