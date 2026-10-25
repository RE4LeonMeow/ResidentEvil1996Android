// ArrangeStages.cpp - the Director's Cut's arrange-room table (PS1 0x80043fb4).
#include "ArrangeStages.h"

#include "../../Globals.h"

// EffectSprites.cpp, PC 0x004c48b8. Four page-name indices per room.
extern const unsigned char g_RoomEffectSpriteTable[7 * 32 * 4];

// The rooms that have an arrange version, by base stage: 7 rows of 9 entries,
// 0xFF padding. Read from SLUS_005.51 0x80010670, which the original copies
// onto its stack before the scan.
//
// The row lengths are the table's own proof: they match the arrange RDTs on the
// disc one for one - 8 rooms in Stage8, 5 in Stage9, 2 in StageA, 3 in StageB,
// 4 in StageC, 9 in StageD, 7 in StageE.
#define DC_ARRANGE_ROWS  7
#define DC_ARRANGE_COLS  9
#define DC_VARIANT_ROWS  14
#define DC_VARIANT_COLS  32

static const unsigned char g_dcArrangeRooms[DC_ARRANGE_ROWS][DC_ARRANGE_COLS] = {
    // stage 0 (Stage1, mansion 1F)  -> Stage8
    { 0x01, 0x07, 0x0B, 0x0D, 0x12, 0x13, 0x1A, 0x1C, 0xFF },
    // stage 1 (Stage2, mansion 2F)  -> Stage9
    { 0x01, 0x02, 0x03, 0x04, 0x12, 0xFF, 0xFF, 0xFF, 0xFF },
    // stage 2 (Stage3, courtyard/underground)   -> StageA
    { 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    // stage 3 (Stage4, guardhouse)  -> StageB
    { 0x00, 0x04, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    // stage 4 (Stage5, laboratory) -> StageC
    { 0x04, 0x05, 0x07, 0x11, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    // stage 5 (Stage6, 1F revisit)  -> StageD
    { 0x01, 0x04, 0x07, 0x0B, 0x0D, 0x12, 0x13, 0x1A, 0x1C },
    // stage 6 (Stage7, 2F revisit)  -> StageE
    { 0x01, 0x02, 0x03, 0x04, 0x12, 0x18, 0x1A, 0xFF, 0xFF },
};

// Which rooms have a second, character-specific RDT, by stage: the DC's own
// table at SLUS_005.51 0x80090f48, 14 rows of 32. A ZERO means the room has
// a variant and LoadRoomRdt adds the character bit to its file index; any
// other value means the one file serves both characters.
//
// The PC release needs no such table because it ships BOTH variants for every
// room, duplicating the file where the PS1 shares it - which is why the port
// could always append the variant digit and find something. The DC disc does
// not: STAGE9 has ROOM9010 and no ROOM9011, so Jill walking into arrange 2F
// asked for a file that is not there, and LoadRoomRdt then relocated pointers
// through an unloaded buffer.
static const unsigned char g_dcRoomCharVariant[DC_VARIANT_ROWS][DC_VARIANT_COLS] = {
    // Stage1
    { 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0 },
    // Stage2
    { 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 },
    // Stage3
    { 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    // Stage4
    { 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    // Stage5
    { 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    // Stage6
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
    // Stage7
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0 },
    // Stage8
    { 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0 },
    // Stage9
    { 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 },
    // StageA
    { 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    // StageB
    { 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    // StageC
    { 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    // StageD
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
    // StageE
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0 },
};

int dc_room_has_arrange(unsigned char stageId, unsigned char roomId)
{
    if (stageId >= DC_ARRANGE_ROWS) {
        return 0;
    }
    for (int i = 0; i < DC_ARRANGE_COLS; i++) {
        unsigned char entry = g_dcArrangeRooms[stageId][i];
        if (entry != 0xFF && entry == roomId) {
            return 1;
        }
    }
    return 0;
}

unsigned char room_file_stage(void)
{
    // Already an arrange stage: return it unchanged - the scan below only
    // covers the seven base rows anyway.
    if ((unsigned int)g_stageId >= STAGE_ARRANGE_FIRST) {
        return g_stageId;
    }

    // The original gates the whole scan on the ADVANCED bit, so STANDARD and
    // TRAINING walk the base rooms even though the arrange files are right
    // there on the disc.
    if (g_bDcMode && (g_main_state_flags2 & MSF2_DC_ADVANCED) != 0 &&
        dc_room_has_arrange(g_stageId, g_roomId)) {
        return (unsigned char)(g_stageId + STAGE_ARRANGE_FIRST);
    }
    return g_stageId;
}

int dc_room_has_char_variant(unsigned char stageId, unsigned char roomId)
{
    if (stageId >= DC_VARIANT_ROWS || roomId >= DC_VARIANT_COLS) {
        return 1;               // off the table: behave as the port always has
    }
    return g_dcRoomCharVariant[stageId][roomId] == 0;
}

// ============================================================================
// room_effect_page_entry - the effect-sprite page row for the current room.
//
// g_RoomEffectSpriteTable (EffectSprites.cpp, PC 0x004c48b8) is 7 stages x 32
// rooms x 4 bytes, and its four bytes are the sprite-page indices at texY
// 0x18..0x1B; 0xFF means the room has no such page. An arrange room indexes it
// at the BASE row, which is right for the pages the room shares - and wrong
// exactly where ADVANCED adds an effect.
//
// Room 101 is the case - the 1F stairs corridor. Its base row is {00, 03, FF,
// FF}: no third page. Its windows are smashed by the dogs one at a time (the
// room's own init script builds a room-action zone per window, each calling
// create_room_event on flags 0x1C8/0x1C9/0x1CA, and the main script then hides
// room sprites 3/7/A/B/D/E/F), and the glass burst those windows spawn lives on
// that missing third page. With the byte at 0xFF, effect_depth_record() returns
// 0xFF and effect_submit_sprite drops the effect on the floor before drawing
// anything, and FUN_0047d0e0 never creates the page either.
//
// WHERE THE VALUE COMES FROM: room 108 (stage 0 room 8) is the ONLY row in the
// whole 224-room table with that third page set - {00, 05, 06, FF} - and the DC
// runs the same dog-through-the-window event there. So the arrange stairs
// corridor takes that page index and keeps everything else from its base row.
//
// Room 718 (stage 6 room 0x18, the 2F revisit corridor) is the second case, and
// it does not need the twin-room argument at all - the packing settles it. The
// page a declared sprite lands on is NOT stored anywhere; setup_effect_sprite_
// textures assigns it with a running cursor that starts where the core00 weapon
// pass left it (texY 0x19, curU 147) and advances whenever imgH + curU > 0x100.
// Replaying that over each RDT's own effect_anim_index gives:
//
//   ROOM1010 (base 101)  {03,04,20}                  -> all page 1
//   ROOM8010 (arr. 101)  {03,04,20, 15,21,22,27}     -> page 1, then page 2
//   ROOM1080 (room 108)  {04, 15,21, 22,27}          -> page 1, then page 2
//   ROOM1170/2120/7120   {1C}  (the crow rooms)      -> page 2
//   ROOM7180 (base 718)  {} - nothing declared at all
//   ROOME180 (arr. 718)  {1C, 15,21,22,27}           -> ALL FIVE on page 2
//
// which matches every shipped row that exists (room 108 is {00,05,06,FF}, the
// crow rooms are {00,01,0A,FF}, room 101 is {00,03,FF,FF}) and independently
// reproduces the page the room-101 fix above already targets. ROOME180 leads
// with sprite 0x1C - a 120-row sheet - so it overflows page 1 immediately and
// every later sprite follows it onto page 2. Base row {00,01,FF,FF} => slot 2
// is 0xFF => the crow burst AND the glass burst are both dropped, which is
// exactly the reported symptom.
//
// The value 0x0A (esp208) is the crow rooms' own page: 0x1C is the one sprite
// whose V range a band actually covers, and all three base crow rooms put it
// there. Room 108's 0x06 would render identically here - the only reader of
// this byte is effect_depth_record, whose band lookup yields blendMode 0 for
// every one of these sprites either way, and the art itself comes from the
// RDT's embedded TIMs (load_effect_sprites), not from the named esp file.
//
// Patch one room at a time, from evidence: an entry means "ADVANCED adds an
// effect page to this room", NOT "copy this room wholesale".
// ============================================================================
struct DcArrangeEffectPage { unsigned char stage; unsigned char room; unsigned char page2; };

// stage/room are the BASE ones (g_stageId / g_roomId), which is what the script
// and the room files agree on; the entry only applies to an arrange load.
static const DcArrangeEffectPage g_dcArrangeEffectPages[] = {
    // room 101 (1F stairs corridor): the dogs' window glass, as room 108 has it.
    { 0x00, 0x01, 0x06 },
    // room 718 (2F revisit): the crows' window burst. See below - this row is
    // the stronger case of the two, because EVERY sprite the room declares
    // lands on the missing page.
    { 0x06, 0x18, 0x0A },
};

const unsigned char* room_effect_page_entry(void)
{
    static unsigned char patched[4];
    const unsigned char* base = &g_RoomEffectSpriteTable
        [(get_stage_id() * 0x20 + (unsigned int)g_roomId) * 4];

    if (!g_bDcMode || room_file_stage() < STAGE_ARRANGE_FIRST) {
        return base;        // base stages: the row itself, untouched
    }

    for (unsigned int i = 0;
         i < sizeof(g_dcArrangeEffectPages) / sizeof(g_dcArrangeEffectPages[0]); i++) {
        const DcArrangeEffectPage& e = g_dcArrangeEffectPages[i];
        if (e.stage == g_stageId && e.room == g_roomId) {
            patched[0] = base[0];
            patched[1] = base[1];
            patched[2] = e.page2;       // the page ADVANCED added
            patched[3] = base[3];
            return patched;
        }
    }
    return base;
}
