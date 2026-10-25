// EndingScreen.cpp - The ending sequence (ending_state, 0x00410820)
//
// Runs after game_loop returns 0 (the player escaped the mansion). The whole
// sequence is one task, driven by Task_sleep(1) between frames:
//
//   1. Pick the ending id (1..7) from the two survivor flags + the character.
//   2. Play the ending FMVs (id-selected), then the STAFF ROLL movie.
//   3. Run the RESULT screen - a 21-slot sprite/text list with per-slot fades,
//      stepped by a 13-state machine (ending_result_update, 0x00411220).
//   4. If the ending qualifies and the player is not already carrying the
//      special key, run the rocket-launcher epilogue: a 3D item model spinning
//      over rc1121.pix (ending_epilogue_update, 0x00411940).
//   5. Rebuild the next-cycle save: reload bio_card.dat, reset stage/health,
//      SetInitialItems, then grant the unlock items (rocket launcher, special
//      key, and the sub-4h infinite weapon) before LoadSaveGameState writes
//      the "cleared" slot and the task chains back to title_state.
//
// Buffer note: the original hands out fixed absolute addresses inside the big
// data block at 0x00c26dc0 (= g_DataBuffer). Every one of them is reproduced
// here as g_DataBuffer + the same offset, except g_loadDataDestPointer's first
// value (0x00d0d5c0) which lies outside the block and is overwritten with
// g_DataBuffer before anything reads it.
// ============================================================================
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include "Ps1EndingCredits.h"
#include "SpriteRenderer.h"
#include "BioCard.h"
#include "PrintText.h"
#include "../system/AssetPath.h"
#include <cstdio>
#include <cstring>

extern void  setSomeColor(int r, int g, int b);                 // 0x00470a50
extern void  Flg_on(int baseAddr, unsigned int bitIndex);       // 0x00473ef0
extern void  FUN_00484420(void* src, void* dst);                // menu_load_item_model_texture
extern void  FUN_004846d0(int slot);                            // item_viewer_queue_draw
extern void  FUN_00483580(int* joint, MATRIX* out);             // item_viewer_compose_matrix
extern void  SetInitialItems(void);                             // 0x004513f0
extern void  QueueVideoPlayback(int id, int b);                 // 0x004422d0
extern void  InitScaMatrix(int owner, ScaMatrixData* scaData);   // 0x00483520
// 0x0047cf80 - retire every effect matching the given field mask
extern void  FUN_0047cf80(int mask, unsigned int type, unsigned int depthGroup,
                          unsigned int d, MATRIX* pos);

// ============================================================================
// The RESULT-screen slot (0x44 bytes, 21 of them at 0x004d6458)
//
// Every slot is a fade-driven draw command. `fadeLevel` is a signed 16-bit
// ramp: ending_result_draw steps it by `fadeVel`, saturates it at 0x8000 /
// 0 and then feeds its HIGH BYTE into the descriptor's colour multipliers,
// so 0x8000 reads back as the neutral 0x80.
// ============================================================================
#pragma pack(push, 1)
struct EndSlot {
    short       active;      // 0x00 - 0 = free (ending_slot_alloc's search key)
    short       type;        // 0x02 - 1 screen colour, 2 sprite, 3 timer, 5 saves
    short       unk04;       // 0x04 - always 3
    short       pad06;       // 0x06
    int         pad08;       // 0x08
    int         x;           // 0x0c - 16.16, screen X before the -0xa0 centring
    int         y;           // 0x10 - 16.16, screen Y before the -0x78 centring
    int         pad14;       // 0x14
    short       fadeVel;     // 0x18
    short       fadeLevel;   // 0x1a
    int         pad1c;       // 0x1c
    TextureDesc tex;         // 0x20 - the descriptor draw_texture/AddTintSprite get
    int         pad40;       // 0x40
};
#pragma pack(pop)
static_assert(sizeof(EndSlot) == 0x44, "EndSlot size mismatch");

#define END_SLOT_COUNT 0x15

// ============================================================================
// Ending descriptor table (0x004b37bc), indexed by the ending id 1..7.
// Row 0 is unused - ending_select_id never returns 0.
// ============================================================================
struct EndingRow {
    unsigned char staffRoll;   // +0 - 1 = tag the STAFF ROLL movie (id 22) on
    unsigned char plate;       // +1 - selects the "congratulations" FMV group
    unsigned char epilogue;    // +2 - 1 = the rocket-launcher epilogue applies
    unsigned char background;  // +3 - 1 = character .pix backdrop + en07.tim
};

static const EndingRow s_endingTable[8] = {
    { 0, 0, 0, 0 },   // 0 - unused
    { 1, 1, 0, 0 },   // 1 - Chris, both survivors
    { 1, 1, 0, 0 },   // 2 - Jill,  both survivors
    { 1, 1, 0, 0 },   // 3 - one survivor + partner
    { 0, 0, 0, 1 },   // 4 - Chris alone
    { 0, 0, 0, 1 },   // 5 - Jill alone
    { 0, 0, 1, 1 },   // 6 - Chris, no survivors
    { 0, 0, 1, 1 },   // 7 - Jill,  no survivors
};

// ============================================================================
// RESULT-screen entry table, 8 bytes each.
//
// Entries 0-4 are the "background" layout (endings 4-7), 7-11 the plain one
// (endings 1-3); ending_result_build picks the base from the row's background
// flag. Entries 5-6 belong to the rocket-launcher epilogue instead.
//
// The Japanese release ships its OWN copy of this table (Biohazard.exe
// 0x004b20d0) and the only differences are the x of the four type 3 / type 5
// value slots - exactly (14 - 8) * character_count, so the strings keep the
// same centre once the 14px Japanese cell replaces the 8px USA one:
//
//   entry  1  clear time  USA 0x30  JPN 0x18   (8 chars -> -24)
//   entry  3  save count  USA 0x48  JPN 0x42   (2 chars ->  -6)
//   entry  8  clear time  USA 0x80  JPN 0x68
//   entry 10  save count  USA 0x98  JPN 0x92
//
// Without those the values sit 24px / 6px right of the label. Verified
// byte-for-byte in both executables.
// ============================================================================
struct CreditEntry {
    short         type;
    short         x;
    short         y;
    unsigned char texV;      // V cursor into the en0x.tim page
    unsigned char height;
};

static const CreditEntry s_creditEntries[12] = {          // 0x004b37e0
    { 2, 0x00, 0x30, 0x00, 0x18 },   //  0
    { 3, 0x30, 0x50, 0x00, 0x00 },   //  1 - clear time
    { 2, 0x00, 0x74, 0x18, 0x18 },   //  2
    { 5, 0x48, 0x94, 0x00, 0x00 },   //  3 - save count
    { 2, 0x25, 0x66, 0x30, 0x28 },   //  4
    { 2, 0x20, 0x20, 0x58, 0x30 },   //  5 - epilogue
    { 2, 0x20, 0xC0, 0x88, 0x20 },   //  6 - epilogue
    { 2, 0x20, 0x16, 0x00, 0x36 },   //  7
    { 3, 0x80, 0x58, 0x00, 0x00 },   //  8 - clear time
    { 2, 0x20, 0x7A, 0x36, 0x36 },   //  9
    { 5, 0x98, 0xB8, 0x00, 0x00 },   // 10 - save count
    { 2, 0x25, 0x66, 0x30, 0x28 },   // 11
};

static const CreditEntry s_creditEntriesJpn[12] = {       // 0x004b20d0 (Biohazard.exe)
    { 2, 0x00, 0x30, 0x00, 0x18 },   //  0
    { 3, 0x18, 0x50, 0x00, 0x00 },   //  1 - clear time
    { 2, 0x00, 0x74, 0x18, 0x18 },   //  2
    { 5, 0x42, 0x94, 0x00, 0x00 },   //  3 - save count
    { 2, 0x25, 0x66, 0x30, 0x28 },   //  4
    { 2, 0x20, 0x20, 0x58, 0x30 },   //  5 - epilogue
    { 2, 0x20, 0xC0, 0x88, 0x20 },   //  6 - epilogue
    { 2, 0x20, 0x16, 0x00, 0x36 },   //  7
    { 3, 0x68, 0x58, 0x00, 0x00 },   //  8 - clear time
    { 2, 0x20, 0x7A, 0x36, 0x36 },   //  9
    { 5, 0x92, 0xB8, 0x00, 0x00 },   // 10 - save count
    { 2, 0x25, 0x66, 0x30, 0x28 },   // 11
};

// The two originals each read one absolute table, so the version picks which.
static const CreditEntry* credit_entries(void)
{
    return (GetAssetVersion() != 0) ? s_creditEntriesJpn : s_creditEntries;
}

// ============================================================================
// The fabricated room the RESULT screen runs inside.
//
// The ending never loads an RDT, so ending_result_build installs this one:
// zeroed everywhere except camera 0's fov, which it stamps with the same
// value it hands set_scene_render_param. The boundary block is a zeroed
// RDT_BoundaryHeader, so every quadrant span is empty and the effect probes
// walk nothing.
// ============================================================================
static struct {                                  // 0x004d6c60
    RDT        hdr;
    RDT_Camera cams[4];
} s_endingRdt;
static_assert(offsetof(decltype(s_endingRdt), cams) == 0x94,
              "ending RDT cameras must follow the header with no padding");

static const unsigned char s_endingBoundaries[32] = { 0 };   // 0x004b3858

// 0x004b3840 - the cam-switch group header g_CurrentRdtDataTypePtr is parked
// on for the duration. Copied verbatim from the exe.
static const unsigned char s_endingCamSwitchZone[20] = {
    0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7D, 0x00, 0x7D, 0x00, 0x7D,
    0x00, 0x7D, 0x00, 0x00,
};

// ============================================================================
// Ending state (the 0x004d6xxx block)
// ============================================================================
static EndSlot  s_slots[END_SLOT_COUNT];    // 0x004d6458
static EndSlot* s_drawCursor;               // 0x004d69f0 - ending_result_draw's cursor
static unsigned char s_underTimeLimit;      // 0x004d69f4 - cleared in under 3h20m
static void*    s_dat4d69f8;                // 0x004d69f8
static unsigned char s_endingId;            // 0x004d69fc - 1..7
static unsigned char s_fmvCharId;           // 0x004d6a04 - PLAYABLE_CHAR_ID
static int      s_lightData[4];             // 0x004d6a08 - dir xyz + packed rgb
static int      s_dat4d6a18;                // 0x004d6a18
static void*    s_viewerJoint;              // 0x004d6a1c - the epilogue Sca chain leaf
static int      s_camera[8];                // 0x004d6a30 - from xyz, to xyz, roll, 0
static EndSlot* s_alloc;                    // 0x004d6a50 - ending_slot_alloc's result
static EndSlot* s_bgSlot;                   // 0x004d6a54 - the screen-colour slot
static EndSlot* s_creditSlots[7];           // 0x004d6a58..0x004d6a70
static unsigned char s_effectsEnabled;      // 0x004d6a78 - the no-save-run bonus
static unsigned char s_grantRocket;         // 0x004d6ae0
static unsigned char s_stepDone;            // 0x004d6af8 - loop exit latch
static unsigned char s_step;                // 0x004d6af0 - state machine index
static unsigned char s_dat4d6af4;           // 0x004d6af4
static unsigned short s_stepTimer;          // 0x004d6b00
static ScaMatrixData s_scaLeaf;             // 0x004d6b08
static void*    s_dat4d6b58;                // 0x004d6b58
static ScaMatrixData s_scaRoot;             // 0x004d6b60
static ScaMatrixData s_scaSpin;             // 0x004d6bb0
static short    s_plateGroup;               // 0x004d6c00
static void*    s_dat4d6c10;                // 0x004d6c10
// 0x00cdd5c0 - the i73v.ivm image. That address is 0xb6800 into the original's
// data block, but I73V.IVM is 0x167b8 bytes, so the original's copy runs 0x1ae0
// bytes past the end of what this port models as g_DataBuffer (0xcb4d8). Give it
// its own buffer rather than scribbling on the next .bss object.
static BYTE     s_itemModelBuffer[0x18000];
static void*    s_itemModelBuf;             // 0x004d6c20
static unsigned char s_hasSpecialKey;       // 0x004d6c54 - g_HasRocketLauncherFlag
static void*    s_dat4d644c;                // 0x004d644c
static void*    s_dat4d6450;                // 0x004d6450
static int      s_dat4d6c4c;                // 0x004d6c4c
static int      s_dat4d6c50;                // 0x004d6c50
static void*    s_itemModelDst;             // 0x004d6d34
static void*    s_saveScratch;              // 0x004d6d38
static void*    s_bgImageBuffer;            // 0x004d6d3c
static SVECTOR  s_spinAngles;               // 0x004d6d58
static int      s_effectPos[3];             // 0x004d6d40 - the bonus billboard origin

// ============================================================================
// ending_select_id (0x00411190)
// The ending id is (Barry/Rebecca alive) x (the partner rescued) x character.
// Flag SCENARIO2_FLAG_PARTNER_ALIVE of g_ScenarioFlags2 is the "partner survived" bit, SCENARIO2_FLAG_SECOND_SURVIVOR the
// "second survivor" bit.
// ============================================================================
static void ending_select_id(void)
{
    if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_PARTNER_ALIVE) == 0) {
        if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_SECOND_SURVIVOR) != 0) {
            s_endingId = ((g_playerEntity.id & 3) == CHAR_CHRIS) ? 6 : 7;
            return;
        }
        s_endingId = ((g_playerEntity.id & 3) == CHAR_CHRIS) ? 4 : 5;
        return;
    }
    if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_SECOND_SURVIVOR) != 0) {
        s_endingId = 3;
        return;
    }
    s_endingId = ((g_playerEntity.id & 3) == CHAR_CHRIS) ? 1 : 2;
}

// ============================================================================
// ending_scene_reset (0x00410e60)
// ============================================================================
static void ending_scene_reset(void)
{
    s_dat4d6c50 = 0;
    s_dat4d6c4c = 0;
    g_bGameActive = 0;
    s_dat4d6450 = s_saveScratch;
}

// ============================================================================
// ending_slots_clear (0x00410f10)
// Only the `active` word is cleared - every other field is re-filled by
// whoever claims the slot next.
// ============================================================================
static void ending_slots_clear(void)
{
    for (int i = 0; i < END_SLOT_COUNT; i++) {
        s_slots[i].active = 0;
    }
}

// ============================================================================
// ending_slot_alloc (0x00410ec0)
// First free slot into s_alloc, NULL when the table is full.
// ============================================================================
static void ending_slot_alloc(void)
{
    for (int i = 0; i < END_SLOT_COUNT; i++) {
        if (s_slots[i].active == 0) {
            s_alloc = &s_slots[i];
            return;
        }
        s_alloc = NULL;
    }
}

// ============================================================================
// ending_draw_text (USA 0x00411da0, Biohazard.exe 0x0043e900)
// Draws PRINT_TEXT_BUFFER through the current slot's descriptor using the
// current font's 18-column glyph sheet: u = (c % 18) * glyphW,
// v = (c / 18) * 14. glyphW is 8 for the USA font (fontus.tim) and 14 for the
// Japanese one (data\FONT.TIM), exactly as the two originals differ:
//   USA     texU = (c % 0x12) * 8,   pen += 8
//   Biohazard.exe texU = (c % 0x12) * 0x0E, pen += 0x0E
// Spaces are skipped but still advance the pen, and the pen advance is NOT
// undone - ending_result_draw rewrites screenX from the slot position at the
// top of every frame, which is what puts it back.
// ============================================================================
static void ending_draw_text(void)
{
    // 8px cells in fontus.tim, 14px cells in the Japanese FONT.TIM. See the
    // sibling renderers PrintText8x14 / PrintFormattedText / draw_item_name,
    // which switch on the same flag.
    const int glyphW = (GetAssetVersion() != 0) ? 14 : 8;

    const unsigned char* p = (const unsigned char*)PRINT_TEXT_BUFFER;
    do {
        if (*p != 0x20) {
            s_drawCursor->tex.texU = (unsigned char)((*p % 0x12) * glyphW);
            s_drawCursor->tex.texV = (unsigned char)((*p / 0x12) * 0x0E);
            AddTintSprite(&s_drawCursor->tex, 2);
        }
        p++;
        s_drawCursor->tex.screenX = (short)(s_drawCursor->tex.screenX + glyphW);
    } while (*p != 0);
}

// ============================================================================
// ending_result_draw (USA 0x00410f40, Biohazard.exe 0x0043daa0)
// One pass over all 21 slots: step the fade, refresh the descriptor from the
// slot position, then dispatch on the slot type.
// ============================================================================
static void ending_result_draw(void)
{
    s_drawCursor = &s_slots[0];

    for (int n = END_SLOT_COUNT; n != 0; n--, s_drawCursor++) {
        EndSlot* s = s_drawCursor;
        if (s->active == 0) continue;

        s->tex.screenX = (short)((s->x >> 16) - 0xA0);
        s->tex.screenY = (short)((s->y >> 16) - 0x78);

        if (s->fadeVel != 0) {
            s->fadeLevel = (short)(s->fadeLevel + s->fadeVel);
            if (s->fadeLevel < 0) {
                // Wrapped: a rising ramp pins at the top, a falling one at 0.
                s->fadeLevel = (short)0x8000;
                if (s->fadeVel <= 0) s->fadeLevel = 0;
                s->fadeVel = 0;
            }
        }

        unsigned char shade = (unsigned char)((unsigned short)s->fadeLevel >> 8);
        s->tex.colorMulR = shade;
        s->tex.colorMulG = shade;
        s->tex.colorMulB = shade;

        switch (s->type) {
        case 1:
            setSomeColor(s->tex.colorMulR, s->tex.colorMulG, s->tex.colorMulB);
            break;

        case 2:
            if (s->fadeLevel != 0) {
                draw_texture(&s->tex, 2);
            }
            break;

        case 3:
            // Clear time. The game timer ticks at 30Hz: 0x1a5e0 = 1h,
            // 0x708 = 1min, 0x1e = 1s. 0x5e is the sheet's separator glyph -
            // both originals pass the same byte here, and it reads as a kana
            // in the Japanese sheet ('く', L[5][4]) exactly as Biohazard.exe
            // does. Cell width follows the font: USA 0x00410f40 writes 8,
            // Biohazard.exe 0x0043daa0 writes 0xe.
            s->tex.texturePage         = 0x1E;
            s->tex.width         = (unsigned short)((GetAssetVersion() != 0) ? 0x0E : 8);
            s->tex.height        = 0x0E;
            s->tex.clutX         = 0x100;
            s->tex.clutY = 0x1E0;
            sprintf(PRINT_TEXT_BUFFER, "%02d:%02d%1c%02d",
                    (int)(g_gameTimerSnapshot / 0x1A5E0),
                    (int)((g_gameTimerSnapshot % 0x1A5E0) / 0x708),
                    0x5E,
                    (int)((g_gameTimerSnapshot % 0x708) / 0x1E));
            if (s->fadeLevel != 0) ending_draw_text();
            break;

        case 5:
            // Save count. The counter is one ahead of the number of saves the
            // player actually made, so it is decremented unless it is zero.
            s->tex.texturePage         = 0x1E;
            s->tex.width         = (unsigned short)((GetAssetVersion() != 0) ? 0x0E : 8);
            s->tex.height        = 0x0E;
            s->tex.clutX         = 0x100;
            s->tex.clutY = 0x1E0;
            sprintf(PRINT_TEXT_BUFFER, "%02d",
                    (g_SavesCounter == 0) ? 0 : (g_SavesCounter - 1));
            if (s->fadeLevel != 0) ending_draw_text();
            break;

        default:
            break;
        }
    }
}

// ============================================================================
// ending_bg_slot_setup (0x00410e80)
// Claims the type-1 slot that owns the screen clear colour.
// ============================================================================
static void ending_bg_slot_setup(void)
{
    ending_slot_alloc();
    s_bgSlot = s_alloc;
    s_alloc->active = 1;
    s_bgSlot->type = 1;
    s_bgSlot->fadeLevel = 0;
    s_bgSlot->fadeVel = 0;
}

// ============================================================================
// ending_effects_enable (0x00411910)
// The confetti bonus is a no-save run: savesCounter still reads 1 (its initial
// value) only if the player never used a typewriter.
// ============================================================================
static void ending_effects_enable(void)
{
    if (g_SavesCounter == 1) {
        s_effectsEnabled = 1;
        g_RdtPointer->boundaries = (unsigned char*)s_endingBoundaries;
        return;
    }
    s_effectsEnabled = 0;
}

// ============================================================================
// ending_result_build (USA 0x00411640, Biohazard.exe 0x0043e1a0)
// Claims the five RESULT slots and installs the fabricated room.
// ============================================================================
static void ending_result_build(void)
{
    const CreditEntry* entries = credit_entries();
    int base = (s_endingTable[s_endingId].background == 0) ? 7 : 0;

    for (int i = 0; i < 5; i++) {
        ending_slot_alloc();
        const CreditEntry* e = &entries[base + i];

        s_alloc->active = 1;
        s_alloc->type   = e->type;
        s_alloc->unk04  = 3;
        s_alloc->x      = (int)e->x << 16;

        // Jill's layout on the backdrop endings is shifted a plate width right.
        if (((g_playerEntity.id & 3) != CHAR_CHRIS) &&
            (s_endingTable[s_endingId].background != 0)) {
            s_alloc->x += 0x9C0000;
        }

        s_alloc->y                 = (int)e->y << 16;
        s_alloc->fadeVel           = 0;
        s_alloc->fadeLevel         = 0;
        s_alloc->tex.flags         = 0x10000000;
        s_alloc->tex.texU          = 0;
        s_alloc->tex.texV          = e->texV;
        s_alloc->tex.width         = 0x100;
        s_alloc->tex.height        = e->height;
        s_alloc->tex.clutX         = 0;
        s_alloc->tex.clutY = 0x1FB;
        s_alloc->tex.texturePage         = 0x1B;

        if (s_alloc->type == 3) {
            // Re-centre the clear time once it needs more than two digits of
            // hours. 0x34bc0 is two hours of ticks; in practice unreachable.
            unsigned int hours = (unsigned int)(g_gameTimerSnapshot / 0x34BC0);
            while ((hours / 100) != 0) {
                s_alloc->x -= 0x70000;
                hours /= 10;
            }
        }

        s_creditSlots[i] = s_alloc;
    }

    // Every slot starts hidden; the state machine raises them one pair at a time.
    for (int i = 0; i < 5; i++) {
        s_creditSlots[i]->active = 0;
    }

    s_camera[0] = 0x5DC;      // from
    s_camera[1] = -1500;
    s_camera[2] = -1500;
    s_camera[3] = 6000;       // to
    s_camera[4] = 0;
    s_camera[5] = 0;
    s_camera[6] = 0;          // roll
    s_camera[7] = 0;
    set_scene_render_param(0x82);
    MatrixToCamera((MATRIX*)s_camera);

    g_CurrentRdtDataTypePtr = (void*)s_endingCamSwitchZone;
    g_RdtPointer = (RDT*)&s_endingRdt;
    g_roomCameraId = 0;

    // 0x00411884: the camera block is copied straight over camera 0's
    // from/to/roll fields (8 dwords, RDT+0x9c..0xbb), then the fov is stamped.
    memcpy((char*)g_RdtPointer + 0x9C, s_camera, sizeof(s_camera));
    s_endingRdt.cams[g_roomCameraId].fov = 0x82;

    // empty_483510(): returns 0 in the original - call dropped

    if (s_endingTable[s_endingId].background != 0) {
        setSomeColor(0, 0, 0);
        display_image(0, s_bgImageBuffer, 0x140, 0xF0);
        title_setup_texture_pages(0, 1);
        // empty_00470960(0): empty in the original - call dropped
    }
}

// ============================================================================
// ending_result_update (0x00411220)
// The RESULT screen's 13-step machine. 0x800 per frame is a ~40 frame fade in,
// 0xf800 the same ramp downwards.
// ============================================================================
static void ending_result_update(void)
{
    switch (s_step) {
    case 0:
        s_stepTimer = 0;
        g_bGameActive = 2;
        g_message_flags = (WORD)(g_message_flags | 8);
        s_plateGroup = 1;
        ending_slots_clear();
        ending_bg_slot_setup();
        s_bgSlot->fadeLevel = 0;
        s_bgSlot->fadeVel = 0x800;
        ending_result_build();
        s_step = 1;
        break;

    case 1:
        s_stepTimer++;
        if (s_stepTimer > 0x3C) {
            s_step = 2;
            s_stepTimer = 0;
            s_creditSlots[0]->active = 1;
            s_creditSlots[0]->fadeVel = 0x800;
            s_creditSlots[1]->active = 1;
            s_creditSlots[1]->fadeVel = 0x800;
        }
        break;

    case 2:
        s_stepTimer++;
        if (s_stepTimer > 0x3C) {
            s_step = 3;
            s_stepTimer = 0;
            s_creditSlots[2]->active = 1;
            s_creditSlots[2]->fadeVel = 0x800;
            s_creditSlots[3]->active = 1;
            s_creditSlots[3]->fadeVel = 0x800;
        }
        break;

    case 3:
        s_stepTimer++;
        if (s_stepTimer > 0x5A) {
            s_step = 4;
            s_stepTimer = 0;
        }
        break;

    case 4:
        // Hold until the player presses something or ~76 seconds elapse.
        s_stepTimer++;
        if ((s_stepTimer > 0x708) || ((g_PlayerPadHeld & 0x9F0) != 0)) {
            s_stepTimer = 0;
            s_step = 5;
            if (s_endingId == 6) s_step = 6;
        }
        break;

    case 5:
        s_step = 10;
        s_stepTimer = 0;
        s_bgSlot->fadeVel = (short)0xF800;
        s_creditSlots[0]->fadeVel = (short)0xF800;
        s_creditSlots[1]->fadeVel = (short)0xF800;
        s_creditSlots[2]->fadeVel = (short)0xF800;
        s_creditSlots[3]->fadeVel = (short)0xF800;
        break;

    case 6:
        // Ending 6 keeps the backdrop up for the extra plate below.
        s_step = 7;
        s_stepTimer = 0;
        s_creditSlots[0]->fadeVel = (short)0xF800;
        s_creditSlots[1]->fadeVel = (short)0xF800;
        s_creditSlots[2]->fadeVel = (short)0xF800;
        s_creditSlots[3]->fadeVel = (short)0xF800;
        break;

    case 7:
        s_stepTimer++;
        if (s_stepTimer > 0x5A) {
            s_creditSlots[4]->active = 1;
            s_creditSlots[4]->fadeVel = 0x800;
            s_creditSlots[4]->tex.flags |= 0x40000000;
            s_stepTimer = 0;
            s_step = 8;
        }
        break;

    case 8:
        s_stepTimer++;
        // Drop the additive bit once the plate has reached full brightness.
        if (s_creditSlots[4]->fadeLevel == (short)0x8000) {
            s_creditSlots[4]->tex.flags &= 0xBFFFFFFF;
        }
        if (s_stepTimer > 0x96) {
            s_step = 9;
            s_stepTimer = 0;
            s_bgSlot->fadeVel = (short)0xF800;
        }
        break;

    case 9:
        s_stepTimer++;
        if (s_stepTimer > 0x96) {
            s_step = 10;
            s_stepTimer = 0;
            s_creditSlots[4]->fadeVel = (short)0xF800;
            s_creditSlots[4]->tex.flags |= 0x40000000;
        }
        break;

    case 10:
        s_stepTimer++;
        if (s_stepTimer > 0x1E) {
            ending_effects_enable();
            s_effectPos[0] = 0;
            s_effectPos[1] = 0;
            s_effectPos[2] = 0;
            s_bgSlot->active = 0;
            s_creditSlots[0]->active = 0;
            s_creditSlots[1]->active = 0;
            s_creditSlots[2]->active = 0;
            s_creditSlots[3]->active = 0;
            s_creditSlots[4]->active = 0;
            if (s_effectsEnabled != 0) {
                Effect_CreateBillboard(0x0B, 0x0A, 0, NULL, s_effectPos, 0);
                play_sfx(1, 0, 0);
                play_sfx(1, 1, 0);
            }
            s_step = 11;
            s_stepTimer = 0;
        }
        break;

    case 11:
        s_stepTimer++;
        if (s_stepTimer > 0x32) {
            // Retire the bonus billboard (match on type 0x0b and group 0x0a).
            FUN_0047cf80(3, 0x0B, 10, 0, NULL);
            s_step = 12;
        }
        break;

    case 12:
        s_stepDone = 99;
        s_stepTimer = 0;
        break;

    default:
        break;
    }

    if (s_effectsEnabled != 0) {
        update_2d_effects();
    }
}

// ============================================================================
// ending_epilogue_build (USA 0x00411a90)
// The rocket-launcher reward scene: rc1121.pix behind a spinning i73v model.
// ============================================================================
static void ending_epilogue_build(void)
{
    LoadFile(GAME_DATA_ROOT "data\\rc1121.pix", s_bgImageBuffer, 0x20);
    // empty_483510(): returns 0 in the original - call dropped
    display_image(0, s_bgImageBuffer, 0x140, 0xF0);
    title_setup_texture_pages(0, 1);
    // empty_00470960(0): empty in the original - call dropped

    // Entries 5 and 6 are the epilogue's two caption plates, both fully faded
    // in from the start (fadeLevel 0x8000). Both originals read them out of the
    // same version-specific table the RESULT screen does; entries 5-6 happen
    // to be byte-identical, so this is not a behavioural difference.
    const CreditEntry* entries = credit_entries();
    for (int i = 5; i < 7; i++) {
        ending_slot_alloc();
        const CreditEntry* e = &entries[i];

        s_alloc->active            = 1;
        s_alloc->type              = e->type;
        s_alloc->unk04             = 3;
        s_alloc->x                 = (int)e->x << 16;
        s_alloc->y                 = (int)e->y << 16;
        s_alloc->fadeVel           = 0;
        s_alloc->fadeLevel         = (short)0x8000;
        s_alloc->tex.flags         = 0;
        s_alloc->tex.texU          = 0;
        s_alloc->tex.texV          = e->texV;
        s_alloc->tex.width         = 0x100;
        s_alloc->tex.height        = e->height;
        s_alloc->tex.clutX         = 0;
        s_alloc->tex.clutY = 0x1FB;
        s_alloc->tex.texturePage         = 0x1B;
        s_creditSlots[i] = s_alloc;
    }

    // The model itself goes through the item viewer's own loader/renderer.
    g_TextureCurrentPage = 0x1C;
    g_TextureBankID = 0x15;
    FUN_00484420(s_itemModelBuf, s_itemModelDst);

    // Sca chain: root -> spin -> leaf, the leaf being what the compose walks.
    InitScaMatrix(0, &s_scaRoot);
    InitScaMatrix((int)&s_scaRoot, &s_scaSpin);
    InitScaMatrix((int)&s_scaSpin, &s_scaLeaf);

    s_camera[0] = 12000;   // from
    s_camera[1] = 0;
    s_camera[2] = 0;
    s_camera[3] = 0;       // to
    s_camera[4] = 0;
    s_camera[5] = 0;
    s_camera[6] = 0;
    s_camera[7] = 0;
    MatrixToCamera((MATRIX*)s_camera);
    set_scene_render_param(0x1A4);

    s_lightData[0] = 100;
    s_lightData[1] = 100;
    s_lightData[2] = 100;
    ((unsigned char*)&s_lightData[3])[0] = 0xFF;
    ((unsigned char*)&s_lightData[3])[1] = 0xFF;
    ((unsigned char*)&s_lightData[3])[2] = 0xFF;
    FUN_0040ac80(0, s_lightData);
    setBackColor(0xFFF, 0xFFF, 0xFFF);
    empty_40ae40(0);

    s_spinAngles.y = 0;
    s_spinAngles.z = 0;
    s_dat4d6a18 = 0;
    s_viewerJoint = &s_scaLeaf;

    s_spinAngles.x = 0xAA;
    RotMatrix(&s_spinAngles, &s_scaLeaf.localMatrix);
    s_spinAngles.x = 0;
    s_spinAngles.y = 0;
    s_scaLeaf.field_00 = 0;
    s_spinAngles.z = 0;
    RotMatrix(&s_spinAngles, &s_scaSpin.localMatrix);
    s_spinAngles.y = 0;
    s_spinAngles.z = 0;
    s_scaSpin.field_00 = 0;

    s_spinAngles.x = (short)0xF95C;
    RotMatrix(&s_spinAngles, &s_scaRoot.localMatrix);
    s_scaRoot.localMatrix.t[0] = 0;
    s_spinAngles.x = 0;
    s_scaRoot.localMatrix.t[2] = 0;
    s_spinAngles.y = 0;
    s_spinAngles.z = 0;
    s_scaRoot.localMatrix.t[1] = 500;
    s_scaRoot.field_00 = 0;
}

// ============================================================================
// ending_epilogue_update (0x00411940)
// Three steps: build + fade in, hold ~270 frames, fade out and latch.
// ============================================================================
static void ending_epilogue_update(void)
{
    MATRIX composed;

    if (s_step == 0) {
        g_bGameActive = 2;
        ending_slots_clear();
        ending_bg_slot_setup();
        s_bgSlot->fadeLevel = 0x6000;
        ending_epilogue_build();

        g_fade_type_id = 2;
        g_fading_state = (short)0xFFFF;
        g_fading_counter = (short)0xF800;
        fade_update();

        s_step++;
        s_stepTimer = 0;
        play_sfx(1, 2, 0);
        play_sfx(1, 3, 0);
        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
    } else if (s_step != 1) {
        if ((s_step == 2) && ((short)g_fading_state > 0x7680)) {
            s_stepDone++;
        }
        goto draw;
    }

    s_stepTimer++;
    if (s_stepTimer > 0x10D) {
        g_fade_type_id = 2;
        g_fading_counter = 0x800;
        fade_update();
        s_step++;
    }

draw:
    ending_result_draw();

    // Spin the model about Z, compose the chain and queue the item draw.
    s_spinAngles.z = (short)(s_spinAngles.z + 0x20);
    RotMatrix(&s_spinAngles, &s_scaSpin.localMatrix);
    s_scaSpin.field_00 = 0;
    FUN_00483580((int*)s_viewerJoint, &composed);

    // multAndSetLightMatrix(&g_RoomCameraData) (0x0040ad70)
    MATRIX lightM;
    MulMatrix0(&g_lightMatrix, &g_RoomCameraData, &lightM);
    SetLightMatrix(&lightM);

    SetRotAndTransMatrix(&composed);
    FUN_004846d0(0);
}

// ============================================================================
// ending_state (0x00410820)
// ============================================================================
void ending_state(void)
{
    g_loadSaveStateFlag = 1;
    g_blockF9Flag = 1;    // 0x004b3870 - F9 return-to-title is inert for the whole ending
    g_main_state_flags |= MSF_PANNING_RESET;
    Task_sleep(1);

    sounds_reset();
    LoadSoundBank(0xE, g_DataBuffer);

    // The original's absolute buffer addresses, as offsets into g_DataBuffer
    // (0x00c26dc0). 0x00d0d5c0 is the one address outside the block; it only
    // ever reaches g_loadDataDestPointer, which is rewritten below before
    // anything reads it, so it is left out here.
    s_saveScratch   = g_DataBuffer + 0x10000;   // 0x00c36dc0
    s_dat4d6b58     = g_DataBuffer + 0x50000;   // 0x00c76dc0
    s_bgImageBuffer = g_DataBuffer + 0x51000;   // 0x00c77dc0
    s_dat4d69f8     = g_DataBuffer + 0x76800;   // 0x00c9d5c0
    s_dat4d6c10     = NULL;                     // 0x00d0d5c0 (never read)
    s_dat4d644c     = g_DataBuffer + 0x86800;   // 0x00cad5c0
    s_itemModelBuf  = s_itemModelBuffer;        // 0x00cdd5c0 (see the decl)
    // 0x00ced7e0 - the original's dst is the .ivm's TMD part, 0x10220 into the
    // file. FUN_00484420 recomputes it from the TIM header and ignores this.
    s_itemModelDst  = s_itemModelBuffer + 0x10220;

    ending_select_id();

    g_loadDataDestPointer = g_DataBuffer;
    LoadFile(s_endingTable[s_endingId].background != 0
                 ? GAME_DATA_ROOT "data\\en07.tim"
                 : GAME_DATA_ROOT "data\\en05.tim",
             g_DataBuffer, 0x20);
    LoadFile(GAME_DATA_ROOT "item_m2\\i73v.ivm", s_itemModelBuf, 0x20);

    // The backdrop is the character's "escaped" still. Flag 0x7B is the
    // already-cleared-once bit, which swaps in the alternate stills.
    //
    // The Director's Cut's ENDING overlay (ENDING_DC.EXE) never READS 0x7B: its
    // only 0x7B reference is the Flg_on that marks the next cycle cleared
    // (0x800e1a74), and its only g_gameOptionsFlags read is bit 0x7E (infinite
    // rocket launcher, 0x800e1688). The cleared-once art swap is
    // a USA/PC behaviour the DC dropped, so in DC mode take the first-run branch
    // even though InitializeGame forces the flag for gameplay (GameStart.cpp).
    const char* bgPath;
    if (g_bDcMode) {
        bgPath = ((g_playerEntity.id & 3) == CHAR_CHRIS)
                     ? GAME_DATA_ROOT "data\\clis01.pix"
                     : GAME_DATA_ROOT "data\\jill01.pix";
    } else if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) != 0) {
        bgPath = ((g_playerEntity.id & 3) == CHAR_CHRIS)
                     ? GAME_DATA_ROOT "data\\clis01.pix"
                     : GAME_DATA_ROOT "data\\jill01.pix";
    } else {
        bgPath = ((g_playerEntity.id & 3) == CHAR_CHRIS)
                     ? GAME_DATA_ROOT "data\\chris02.pix"
                     : GAME_DATA_ROOT "data\\jill02.pix";
    }
    LoadFile(bgPath, s_bgImageBuffer, 0x20);

    g_TextureBankID = 0x1B;
    g_TextureCurrentPage = 0x1B;
    LoadTexturePage(g_loadDataDestPointer, 0x1B, 0x1B, 0xC, 0, 0, 0, 0);
    g_loadDataDestPointer = s_dat4d69f8;

    memset_((unsigned int*)g_effectPool, 0x840);
    // empty_00497c10(0): returns 0 in the original - call dropped

    // 0x004109a1: a clip rect built on the caller's own frame; the callee
    // (empty_0040abb0) just returns 0 in this build, so only the call itself
    // was reproduced and is now dropped.
    // empty_483510(): returns 0 in the original - call dropped

    // ------------------------------------------------------------------
    // The ending movies. The original branches on 0x004bcda8 (the MCI/video
    // backend select, default 1); this port routes every FMV through the
    // main_loop 0x40000 path, exactly as logos_state does, so the
    // g_bIsSoftwareRendering == 0 side is the live one here.
    // ------------------------------------------------------------------
    if (g_bIsSoftwareRendering == 0) {
        g_selectedFmvId = 14;
        g_main_state_flags |= MSF_FMV_REQUEST;
        Task_sleep(1);
    } else {
        QueueVideoPlayback(0xE, 0);
    }

    g_selectedFmvId = (unsigned char)(s_endingId + 14);
    if (g_bIsSoftwareRendering == 0) {
        g_main_state_flags |= MSF_FMV_REQUEST;
        Task_sleep(1);
    } else {
        QueueVideoPlayback(g_selectedFmvId, 0);
    }

    ending_scene_reset();
    ending_slots_clear();

    s_plateGroup = (short)s_endingTable[s_endingId].plate;
    s_stepDone = (unsigned char)(s_endingTable[s_endingId].plate ^ 1);

    // The congratulations movie: 24/25/26 per character when the plate group
    // is 1, else 27. Flag 0x7E is "has infinite rocket launcher".
    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_INF_R_LAUNCHER) != 0) {
        s_fmvCharId = 2;
    } else {
        s_fmvCharId = (unsigned char)(g_playerEntity.id & 3);
    }

    if (s_stepDone == 0) {
        g_selectedFmvId = 0x1B;
    } else if (s_stepDone == 1) {
        if (s_fmvCharId == 0)      g_selectedFmvId = 24;
        else if (s_fmvCharId == 1) g_selectedFmvId = 25;
        else                       g_selectedFmvId = 26;
    } else {
        g_selectedFmvId = 27;
    }
    // DC ending has no cleared-once congratulations-FMV swap (see the backdrop
    // note above).
    if (!g_bDcMode &&
        Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) != 0) {
        g_selectedFmvId = 26;
    }

    if (g_bIsSoftwareRendering == 0 && Ps1EndingCredits_IsEnabled()) {
        // The overlay keys its movie and tables on the player character
        // (0x800c5125 & 3), not on the rocket-launcher congratulations id.
        Ps1EndingCredits_Begin(s_endingId, g_playerEntity.id & 3);
    }

    if (g_bIsSoftwareRendering == 0) {
        g_main_state_flags |= MSF_FMV_REQUEST;
        Task_sleep(1);
    }

    if (s_endingTable[s_endingId].staffRoll == 1) {
        if (g_bIsSoftwareRendering == 0) {
            g_main_state_flags =
                (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
            g_spriteAnimR = 0;
            g_spriteAnimG = 0;
            g_spriteAnimB = 0;
            Task_sleep(0x96);
        } else {
            QueueVideoPlayback(g_selectedFmvId, 0);
        }
        g_selectedFmvId = 22;                 // STAFF ROLL
        g_main_state_flags |= MSF_FMV_REQUEST;
        Task_sleep(1);
    } else if (g_bIsSoftwareRendering != 0) {
        g_main_state_flags |= MSF_FMV_REQUEST;
        Task_sleep(1);
    }
    Ps1EndingCredits_End();

    // ------------------------------------------------------------------
    // RESULT screen
    // ------------------------------------------------------------------
    s_stepDone = 0;
    s_step = 0;
    if (s_endingId < 4) {
        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
    } else {
        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
    }

    do {
        ending_result_update();
        ending_result_draw();
        Task_sleep(1);
    } while (s_stepDone == 0);

    // ------------------------------------------------------------------
    // Does the player already own the special key (item 0x38)? If so the
    // rocket-launcher epilogue is skipped and the key simply carries over.
    // ------------------------------------------------------------------
    s_hasSpecialKey = 0;
    if (g_TotalHeldItems != 0) {
        for (unsigned short i = 0; i < g_TotalHeldItems; i++) {
            if (g_ItemsSlots[i].Id == ITEM_SPECIAL_KEY) {
                s_hasSpecialKey = 1;
                break;
            }
        }
    }
    for (unsigned short i = 0; i < 48; i++) {
        if (g_itemboxSlots[i].Id == ITEM_SPECIAL_KEY) {
            s_hasSpecialKey++;
            break;
        }
    }

    if ((s_endingTable[s_endingId].epilogue == 1) &&
        (!g_bDcMode || g_DcDifficulty < DC_DIFFICULTY_ADVANCED) &&
        (s_hasSpecialKey == 0)) {
        s_stepTimer = 0;
        s_stepDone = 0;
        s_step = 0;
        s_dat4d6af4 = 0;
        do {
            ending_epilogue_update();
            Task_sleep(1);
        } while (s_stepDone == 0);
        s_hasSpecialKey++;
    }

    Task_sleep(1);

    // ------------------------------------------------------------------
    // Next-cycle save data
    // ------------------------------------------------------------------
    // 0x69780 = 432000 ticks = 4 hours at 30Hz - the infinite-weapon cutoff.
    s_underTimeLimit = (g_gameTimerSnapshot < 0x69780) ? 1 : 0;

    s_grantRocket = 0;
    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_INF_R_LAUNCHER) != 0) s_grantRocket = 1;
    if (g_SavesCounter == 1) s_grantRocket = 1;

    LoadFile(GAME_DATA_ROOT "data\\bio_card.dat", g_BioCardData, 0x20);
    if (g_bDcMode) {
        g_DcGameMode = (unsigned char)g_DcDifficulty;
    }
    g_fading_state = (short)0xFFFF;
    ending_slots_clear();

    g_SavesCounter = 0;
    g_stageId = STAGE_MANSION_1F;
    g_playerEntity.healthStatusFlags = 0;
    g_playerEntity.health = (short)((g_playerEntity.id & 1) * -44 + 140);
    g_PlayerHealthCopy = g_playerEntity.health;

    // The DC's ending raises MSF2_DC_ADVANCED for the SetInitialItems call and
    // clears it again straight after (ENDING_DC.EXE 0x800e17d0-0x800e1830), so
    // the next-cycle save an ADVANCED run leaves behind starts with the Beretta
    // M92FS custom (item 4) instead of the plain Beretta. The PS1 can test the
    // bit itself because its ending overlay snapshots g_status_flags on entry,
    // while the bits are still set from the run; here game_start has already run
    // them through MSF2_RESET_KEEP_MASK, which does not keep 0x70000. The
    // completed run's mode is g_DcDifficulty - what the rest of this function's
    // DC branches already test - so use that to stand in for the PS1's snapshot.
    if (g_bDcMode && g_DcDifficulty >= DC_DIFFICULTY_ADVANCED) {
        g_main_state_flags2 |= MSF2_DC_ADVANCED;
    }
    SetInitialItems();
    g_main_state_flags2 &= ~MSF2_DC_ADVANCED;

    unsigned char slot = g_TotalHeldItems;
    if ((s_effectsEnabled != 0) || (s_grantRocket != 0)) {
        // The no-save run and the carried-over unlock both hand out the
        // inf. rocket launcher (item 0x0a) and raise its permanent flag.
        Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_INF_R_LAUNCHER);
        g_ItemsSlots[slot].Id = ITEM_ROCKET_LAUNCHER;
        g_ItemsSlots[slot].qty = 1;
        g_TotalHeldItems++;
        slot++;
    }
    if (s_hasSpecialKey != 0 &&
        (!g_bDcMode || g_DcDifficulty < DC_DIFFICULTY_ADVANCED)) {
        g_ItemsSlots[slot].Id = ITEM_SPECIAL_KEY;
        g_ItemsSlots[slot].qty = 1;
        g_TotalHeldItems++;
        slot++;
    }
    if (s_underTimeLimit != 0) {
        g_ItemsSlots[slot].Id =
            ((g_playerEntity.id & 3) != CHAR_CHRIS) ? ITEM_INGRAM : ITEM_MINIMI;
        g_ItemsSlots[slot].qty = 1;
        g_TotalHeldItems++;
        slot++;
    }

    // Director's Cut's ending bonus (0x800e1914): 
    // an ADVANCED run that reaches ending 6 or 7 - the
    // best ending for Chris and for Jill - grants infinite
    // Colt Python Magnum
    if (g_bDcMode && g_DcDifficulty >= DC_DIFFICULTY_ADVANCED
        && (s_endingId == 6 || s_endingId == 7)) {
        Flg_on((int)g_ScenarioFlags, DC_SCENARIO_FLAG_INF_COLT_PYTHON);
        g_ItemsSlots[slot].Id = ITEM_COLT_PYTHON_MAG;
        g_ItemsSlots[slot].qty = 1;
        g_TotalHeldItems++;
        slot++;
    }
    (void)slot;

    Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH);

    setSomeColor(128, 128, 128);
    g_SpecialRoomLightState = (short)0xFFFF;
    LoadSaveGameState(0, (int)s_saveScratch, 0, 1, 1);

    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
    cleanup_texture_slot(0xC);
    FUN_004844b0();
    g_bGameActive = 2;

    g_loadSaveStateFlag = 0;
    g_blockF9Flag = 0;
    g_dwClearCount++;
    Task_chain((void*)title_state);
}
