// DebugMenu.cpp - F1 debug menu (port-only, debug builds)
//
// Not in the original binary. An interactive pause overlay in the spirit of
// the PS1 retail debug menu: F1 freezes gameplay and opens a menu whose
// entries mirror the original's DEBUG MENU ("- DEBUG MENU -", Room change /
// Utility / Memory viewer / Light editor / Sound test / Inventory / Flag
// editor / Quick access). Room change, Inventory, the Flag editor and Quick
// access are implemented; the rest are drawn dimmed until they get their own
// screens. Quick access holds in-menu save/load slot lists (the save list
// writes the bio card inline, the load list restores through game_start) and
// an item box shortcut.
//
// Rendering goes through the pending-sprite queue (draw_rect + PrintText8x8,
// the 8x8 ASCII font region of fontus.tim), flushed with the rest of the
// frame exactly like texture_viewer_overlay (DebugScreens.cpp).
//
// Room change: builds a synthetic 0x18-byte door record for the selected
// stage/room and hands it off exactly like door_begin_transition
// (0x0041b59c, PlayerAnimations.cpp): g_pendingDoorRecord + g_openMenuFlag =
// 1, so game_loop's menu path runs the vanilla room_transition_load
// (0x004813c0, DoorSystem.cpp) with the real fade / door animation / camera
// pipeline.
//
// Player placement: the arrival point (record+0x0E) is encoded in the SOURCE
// room's script - the destination room's own script does not know where
// players spawn inside it. So after room_transition_load returns,
// DebugRoomChange_ApplyPendingPlacement (called from game_loop right after
// it) scans the freshly-loaded room's action table for its first door
// record and stands the player just past that door's action zone, with a
// collision point query to reject spots that are inside a wall.
//
// Menu layout (8x8 font, one 8px cell per character):
//
//      - DEBUG MENU -                    - ROOM CHANGE -
//    >ROOM CHANGE                    ROOM 100 - MANSION SAVE ROOM
//     UTILITY MENU
//     ...
// ============================================================================
#include "../Globals.h"
#include "../platform/platform.h"
#include "../DebugPrint.h"
#include "../Version.h"
#include "../system/AssetPath.h"
#include "../marni/MarniSystem.h"
#include "SpriteRenderer.h"
#include "FileLoader.h"
#include <cstdio>
#include <cstring>
#include <cmath>

typedef int debugmenu_translation_unit_t;

// Flg_on is only extern'd locally by its users (0x00473ef0)
extern void Flg_on(int baseAddr, unsigned int bitIndex);

// room_collision_check_0047da50 is declared in Globals.h

// ============================================================================
// Stage/room tables (names from the Types.h room constants). Stages 5/6 are
// the return-mansion variants and share the stage 0/1 room sets.
// ============================================================================

// Stage 0/5 - Mansion 1F (ROOM_MANSION_SAVE_ROOM..ROOM_WARDROBE_CLOSET)
static const char* const s_dbgRoomsMansion1F[29] = {
    "SAVE ROOM",           "1F LEFT STAIRS",   "VACANT ROOM",
    "F PASSAGE",           "TEA ROOM",         "DINING ROOM",
    "MAIN HALL",           "GALLERY",          "L PASSAGE",
    "TRAP PASSAGE",        "BACK PASSAGE",     "1F RIGHT STAIRS",
    "GREENHOUSE",          "TIGER STATUE ROOM", "KEEPERS BEDROOM",
    "MANSION BAR",         "1F ELEVATOR FRONT", "DRESSING ROOM",
    "WARDROBE",            "MANSION BATHROOM", "BOILER",
    "TRAP ROOM",           "LIVING ROOM",      "LARGE GALLERY",
    "MANSION STOREROOM",   "1F STUDY",         "ROOFED PASSAGE",
    "STOREROOM",           "WARDROBE CLOSET",
};

// Stage 1/6 - Mansion 2F and B1
static const char* const s_dbgRoomsMansion2F[29] = {
    "1F TO B1 ELEVATOR",   "2F LEFT STAIRS",   "DINING ROOM 2F",
    "MAIN HALL 2F",        "C PASSAGE",        "ARMOR ROOM",
    "SMALL LIBRARY",       "2F RIGHT STAIRS",  "DEER ROOM",
    "2F BEDROOM",          "STUDY 2F",         "FRONT LESSON ROOM",
    "LESSON ROOM",         "PILLAR PASSAGE",   "FRONT OF ATTIC",
    "SMALL DINING",        "ATTIC",            "TERRACE PASSAGE",
    "TERRACE",             "2F FRONT ELEVATOR", "2F ROUGH PASSAGE",
    "TROPHY ROOM",         "LARGE LIBRARY",    "PRIVATE LIBRARY",
    "HELIPORT LOOKOUT",    "MANSION SHED",     "B1 PASSAGE 1",
    "B1 PASSAGE 2",        "MANSION KITCHEN",
};

// Stage 2 - Courtyard and Underground
static const char* const s_dbgRoomsCourtyard[18] = {
    "COURTYARD GARDEN",    "WATER GATE",       "FALLS",
    "HELIPORT",            "GUARDHOUSE GATE",  "FOUNTAIN",
    "ITEM CHAMBER",        "UNDERGROUND ENTRY", "BRANCHED PASSAGE",
    "UNDERGROUND GENERATOR", "ENRICO ROOM",    "BOULDER 1 PASSAGE",
    "BLACK TIGER ROOM",    "STRAIGHT PASSAGE", "UNDERGROUND SAVE ROOM",
    "BOULDER 2 PASSAGE",   "ELEVATOR TO LAB",  "SCRAPPED HELIPORT",
};

// Stage 3 - Guardhouse
static const char* const s_dbgRoomsGuardhouse[18] = {
    "GUARDHOUSE ENTRY",    "ROOM 001",         "001 BATHROOM",
    "GUARDHOUSE SAVE ROOM", "GUARDHOUSE BAR",  "GH CENTER PASSAGE",
    "ROOM 002",            "002 BATHROOM",     "BEEHIVE PASSAGE",
    "DRUG STOREHOUSE",     "ROOM 003",         "003 BATHROOM",
    "PLANT 42 ROOM",       "WATER TANK ENTRY", "WATER TANK",
    "SECURITY ROOM",       "ARMS STOREHOUSE",  "CONTROL ROOM",
};

// Stage 4 - Laboratory
static const char* const s_dbgRoomsLaboratory[22] = {
    "LABORATORY ENTRY",    "EMERGENCY TUNNEL", "LAB LADDER ROOM",
    "LAB B2 STAIRWAY",     "VISUAL DATA ROOM", "LAB B3 O PASSAGE",
    "SMALL LABORATORY",    "MORGUE",           "LAB B3 PRIVATE PASSAGE",
    "LAB B3 ROOM A",       "LAB B3 ROOM B",    "CELL ENTRY",
    "LAB B3 ELEVATOR ENTRY", "ESCAPE ELEVATOR", "LAB SAVE ROOM",
    "POWER MAZE 1",        "POWER MAZE 2",     "POWER ROOM",
    "CELL",                "MAIN LAB",         "MAIN LAB ENTRY",
    "LAB B3 TO B4 ELEVATOR",
};

struct DebugStageRooms {
    const char* const* names;
    int                count;
};

// Only the seven base stages are listed. The Director's Cut's arrange stages
// (ids 7-13, STAGE8-E) are not a destination of their own: they hold the same
// physical rooms as the base stage seven ids below, and ADVANCED mode already
// redirects a base stage/room load to its arrange file (room_file_stage, in
// dc/ArrangeStages.cpp), so picking the base room is how you reach it.
#define DBG_STAGE_COUNT   7

static const DebugStageRooms s_dbgStageRooms[DBG_STAGE_COUNT] = {
    { s_dbgRoomsMansion1F, 29 },    // stage 0 - mansion 1F
    { s_dbgRoomsMansion2F, 29 },    // stage 1 - mansion 2F
    { s_dbgRoomsCourtyard, 18 },    // stage 2 - courtyard / underground
    { s_dbgRoomsGuardhouse, 18 },   // stage 3 - guardhouse
    { s_dbgRoomsLaboratory, 22 },   // stage 4 - laboratory
    { s_dbgRoomsMansion1F, 29 },    // stage 5 - mansion return 1F
    { s_dbgRoomsMansion2F, 29 },    // stage 6 - mansion return 2F
};

// Highest stage the room-change menu will scroll to.
static int DebugStageMax(void)
{
    return DBG_STAGE_COUNT - 1;
}

// Menu entries, mirroring the PS1 debug menu. "ROOM CHANGE" replaces its
// "JUMP" entry. Only the implemented ones react to confirm.
static const char* const s_dbgMenuItems[8] = {
    "ROOM CHANGE",
    "UTILITY MENU",
    "MEMORY VIEWER",
    "LIGHT EDITOR",
    "SOUND TEST",
    "INVENTORY",
    "FLAG EDITOR",
    "QUICK ACCESS",
};
static const unsigned char s_dbgItemImplemented[8] = {
    1, 0, 0, 0, 0, 1, 1, 1,
};

// ============================================================================
// State (persists across open/close so the selection is remembered)
// ============================================================================
int g_debugMenuOpen = 0;            // 1 while the overlay is up (game_loop reads it to pause entity updates)

static int           s_dbgContext = 0;          // 0 = main menu, 1 = room change, 2 = inventory, 3 = flag editor, 4 = quick access, 5/6 = save/load slot list
static int           s_dbgCursor = 0;           // main menu selection
static int           s_dbgQuickCursor = 0;      // quick access selection
static int           s_dbgStage = 0;            // room change selection
static int           s_dbgRoom = 0;
static int           s_dbgInvSlot = 0;          // inventory editor: selected slot
static int           s_dbgInvPick = 0;          // inventory editor: 1 = item-id picker submode
static int           s_dbgPickId = 0;           // inventory editor: id being picked
static int           s_dbgFlagView = 4;         // flag editor: selected view (4 = g_SysFlags)
static int           s_dbgFlagBit = 0;          // flag editor: selected bit, ABSOLUTE within the bank
static int           s_dbgPrevKeys = 0;         // edge-detect sample of the previous frame
static int           s_dbgPrevF1 = 0;           // edge-detect sample of F1 across open/close
static int           s_dbgRoomChangeArmed = 0;  // set on confirm, consumed by the game_loop hook
static int           s_dbgRestoreVariant = 0;   // SCENARIO_FLAG_STAGE_VARIANT was cleared for the transition
static unsigned char s_dbgDoorRecord[0x18];     // synthetic door record (must outlive the transition)

// Input bit masks for s_dbg*Keys sampling
#define DBGKEY_LEFT    0x001
#define DBGKEY_RIGHT   0x002
#define DBGKEY_UP      0x004
#define DBGKEY_DOWN    0x008
#define DBGKEY_CONFIRM 0x030   // ENTER or SPACE
#define DBGKEY_ESC     0x040
#define DBGKEY_F1      0x080

// ---------------------------------------------------------------------------
// Game pad support
//
// Navigation reads g_RawPadHeld, the FUNCTION-level word: it has already been
// through g_JoyRemapTbl, so whatever the player bound to action / cancel / the
// d-pad drives this menu without it having to know which physical button that
// is, on either input backend. It stays live while the menu is open -
// PlayerPad_Update blanks only the published gameplay words
// (g_PlayerPadPressed and friends), never g_RawPadHeld. It also carries the
// keyboard, so the GetAsyncKeyState samples are partly redundant; they set the
// same bits, which is harmless.
//
// The open / close toggle cannot work that way: every bound function already
// means something in gameplay. It reads the raw hardware mask instead
// (read_sidewinder_pad = joysticks[0].currPress, bit 8 = pad button 1) and
// wants BOTH shoulder buttons at once. Buttons 5 and 6 are L1/R1 (LB/RB) in
// the XInput ordering AND in the WinMM/HID ordering, so bits 12|13 is the one
// combo that means the same thing on both backends.
#define DBGPAD_UP        0x1000    // g_RawPadHeld: raw pad-word direction bits
#define DBGPAD_DOWN      0x4000
#define DBGPAD_LEFT      0x8000
#define DBGPAD_RIGHT     0x2000
#define DBGPAD_ACTION    0x0080    // action / confirm
#define DBGPAD_CANCEL    0x0040    // cancel / run
#define DBGPAD_AIM       0x0008    // aim - the flag editor's modifier
#define DBGPAD_TOGGLE  0x00003000u // HARDWARE mask (not the raw pad word):
                                   // bits 12|13 = pad buttons 5+6 = L1+R1

static int DebugMenu_SampleKeys(void)
{
    int keys = 0;
    if (plat_key_state(VK_LEFT)   & 0x8000) keys |= DBGKEY_LEFT;
    if (plat_key_state(VK_RIGHT)  & 0x8000) keys |= DBGKEY_RIGHT;
    if (plat_key_state(VK_UP)     & 0x8000) keys |= DBGKEY_UP;
    if (plat_key_state(VK_DOWN)   & 0x8000) keys |= DBGKEY_DOWN;
    if (plat_key_state(VK_RETURN) & 0x8000) keys |= DBGKEY_CONFIRM;
    if (plat_key_state(VK_SPACE)  & 0x8000) keys |= DBGKEY_CONFIRM;
    if (plat_key_state(VK_ESCAPE) & 0x8000) keys |= DBGKEY_ESC;
    if (plat_key_state(VK_F1)     & 0x8000) keys |= DBGKEY_F1;

    DWORD pad = g_RawPadHeld;
    if (pad & DBGPAD_LEFT)   keys |= DBGKEY_LEFT;
    if (pad & DBGPAD_RIGHT)  keys |= DBGKEY_RIGHT;
    if (pad & DBGPAD_UP)     keys |= DBGKEY_UP;
    if (pad & DBGPAD_DOWN)   keys |= DBGKEY_DOWN;
    if (pad & DBGPAD_ACTION) keys |= DBGKEY_CONFIRM;
    if (pad & DBGPAD_CANCEL) keys |= DBGKEY_ESC;
    return keys;
}

// ============================================================================
// Rendering helpers
//
// PrintText8x8 colour notes (measured against AddTintSprite's text pass,
// Rendering.cpp 0x0046e0a0): the upper 4 bits are brightness scaled by 255/30,
// with brightness 0 (-> 2) special-cased to FULL brightness; there is no
// max-brightness bit like PrintText8x14's 0x80. The `shadow` argument
// retints the WHOLE string (index +8), it does not draw a second offset
// pass - so it must stay 0. The lower nibble selects the tint palette and
// must be 0 for white (1 green, 2 red, 3 gray, any other index yellow).
//
// Brightness is a NIBBLE, so white text tops out at 0xF0 = 50% grey: there is
// no "white but dimmer". Readable off-white therefore comes from the gray
// tint (3) at full brightness, which is 204/255 - noticeably lighter than the
// 50% ceiling and clearly legible on the navy box.
//   0x00 = white (full)   0xF0 = 50% grey (brightest white)
//   0x03 = 80% grey       0xC0 = 40% grey
// ============================================================================

#define DBGCOL_TEXT 0x00    // full-brightness white (tint 0)
#define DBGCOL_GREEN 0x01   // full-brightness green - flag editor cursor bit
#define DBGCOL_HINT 0x03    // light grey (tint 3 at full brightness) - control hints
#define DBGCOL_DIM  0xF0    // 50% grey - unimplemented entries / off-window bits
#define DBGCOL_RED  0x02    // full-brightness red (tint 2)

// The translucent navy menu box, drawn as two same-depth tints (draw_rect's
// colour is a per-channel on/off mask for the tint variants, with the last
// non-zero byte as the alpha, so a single tint can only be pure R/G/B):
//   layer 1: variant 3 forces black at alpha 96
//   layer 2: variant 1 blue at alpha 80
// blend 100 with flags 1 puts both at depth 2100, behind the depth-4xx text
// (see draw_rect, Rendering.cpp 0x00470350).
// Same two-layer translucent navy box at a custom game-space rect (the flag
// editor is taller: 16 rows of 16 bits for the 32-byte banks).
static void DebugMenu_DrawBoxSize(int gx, int gy, int gw, int gh)
{
    static RectDrawDesc box;
    box.x = gx - g_ScreenOffsetX;
    box.y = gy - g_ScreenOffsetY;
    box.w = gw;
    box.h = gh;

    box.textureId = 0x70000000;         // variant 3: semi-transparent black
    box.r = 0;
    box.g = 96;                         // -> alpha 96
    box.b = 0;
    draw_rect(&box, 100, 1);

    box.textureId = 0x40000000;         // variant 1: semi-transparent tint
    box.r = 0;
    box.g = 0;
    box.b = 80;                         // -> alpha 80
    draw_rect(&box, 100, 1);
}

static void DebugMenu_DrawBox(void)
{
    DebugMenu_DrawBoxSize(24, 64, 272, 136);
}

static void DebugMenu_PrintCentered(short y, const char* text, unsigned char color)
{
    int px = 24 + ((272 - (int)strlen(text) * 8) / 2);
    sprintf(PRINT_TEXT_BUFFER, "%s", text);
    PrintText8x8((short)px, y, color, 0);
}

// ============================================================================
// Room change (second menu context)
// ============================================================================

// ============================================================================
// Room availability. LoadRoomRdt (0x00477d90) builds the RDT path directly
// from g_stageId + 1 (stage1\\room1190.rdt for stage 0 room 0x19), so a
// stage/room pair is only selectable if a valid RDT ships for it.
// ============================================================================
static int DebugRoomSelectable(int stage, int room)
{
    // ROOM_MANSION_1F_STUDY (0x19): room1190.rdt (stage 1 directory) is NOT
    // valid - the original crashes if it is forced to load. The return
    // stages ship their own copies (room6190.rdt / room7190.rdt), so the
    // study is only excluded from the early mansion stage (0-index 0).
    if (stage == STAGE_MANSION_1F) {
        switch(room) {
        case ROOM_MANSION_1F_ELEVATOR_FRONT:
        case ROOM_MANSION_1F_STUDY:
            return 0;
        }
    }

    // Mansion 2F (stage 2): same rule - the B1 / return-only rooms ship valid
    // RDTs only in the stage 7 (0-index 6) directory; their stage 2
    // room2XX0.rdt files do not exist, so they are excluded there.
    if (stage == STAGE_MANSION_2F) {
        switch (room) {
        case ROOM_MANSION_1F_TO_B1_ELEVATOR:     // 0x00 - stage 7 only
        case ROOM_MANSION_2F_FRONT_ELEVATOR:     // 0x13 - stage 7 only
        case ROOM_mansion_2F_ROUGH_PASSAGE:      // 0x14 - stage 7 only
        case ROOM_TROPHY_ROOM:                   // 0x15 - stage 7 only
        case ROOM_LARGE_LIBRARY:                 // 0x16 - stage 7 only
        case ROOM_PRIVATE_LIBRARY:               // 0x17 - stage 7 only
        case ROOM_HELIPORT_LOOKOUT:              // 0x18 - stage 7 only
        case ROOM_MANSION_SHED:                  // 0x19 - stage 7 only
        case ROOM_MANSION_B1_PASSAGE_1:          // 0x1a - stage 7 only
        case ROOM_MANSION_B1_PASSAGE_2:          // 0x1b - stage 7 only
        case ROOM_MANSION_KITCHEN:               // 0x1c - stage 7 only
            return 0;
        }
    }

    return 1;
}

// After a selection change, step off any unavailable room in the direction
// the user was moving (the room table is contiguous and only a few entries
// are invalid, so this always lands on a selectable room).
static void DebugRoomSkipInvalid(int dir)
{
    int count = s_dbgStageRooms[s_dbgStage].count;
    int guard = 0;
    while (!DebugRoomSelectable(s_dbgStage, s_dbgRoom) && guard++ < count) {
        s_dbgRoom += dir;
        if (s_dbgRoom < 0) {
            s_dbgRoom = 0;
            dir = 1;
        }
        if (s_dbgRoom >= count) {
            s_dbgRoom = count - 1;
            dir = -1;
        }
    }
}

static void DebugRoomChange_Trigger(void)
{
    int stage = s_dbgStage;

    // Defensive: never trigger a transition to a room without a valid RDT.
    if (!DebugRoomSelectable(stage, s_dbgRoom)) {
        dbg_printf("[debugmenu] room change blocked: stage %d room %02X has no valid RDT\n",
                   stage, s_dbgRoom);
        return;
    }

    memset(s_dbgDoorRecord, 0, sizeof(s_dbgDoorRecord));

    // record+0x0D destination, room_transition_load's encoding: < 0x20 is a
    // room in the current stage, otherwise the stage is (dest >> 5) - 1 and
    // the low 5 bits are the room.
    if (stage == (int)g_stageId) {
        s_dbgDoorRecord[0x0D] = (unsigned char)s_dbgRoom;
    } else {
        s_dbgDoorRecord[0x0D] = (unsigned char)(((stage + 1) << 5) | s_dbgRoom);
    }
    s_dbgDoorRecord[0x0A] = 0;      // door type 0 -> door00.dor, standard door animation
    s_dbgDoorRecord[0x0B] = 0;      // entry camera 0; no camera-only (0x80) / silent (0x40) flags
    s_dbgDoorRecord[0x16] = 0xFF;   // no key item

    // room_transition_load adds 5 to stages 0/1 when
    // SCENARIO_FLAG_STAGE_VARIANT is set (the return-mansion remap). Clear it
    // for the transition and restore it once the destination room is loaded
    // so the menu can reach the exact stage it displays.
    s_dbgRestoreVariant = 0;
    if (stage < 2 && Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_STAGE_VARIANT) != 0) {
        FUN_00473f10((int*)g_ScenarioFlags, SCENARIO_FLAG_STAGE_VARIANT);
        s_dbgRestoreVariant = 1;
    }

    s_dbgRoomChangeArmed = 1;

    // The door handoff, identical to door_begin_transition (0x0041b59c):
    // instant full-screen black rect, one frame of sleep, then StMask.
    // game_loop exits its inner frame loop on g_openMenuFlag == 1 and runs
    // room_transition_load through the menu path.
    dbg_printf("[debugmenu] room change: stage %d room %d (display ROOM %X)\n",
               stage, s_dbgRoom, (stage + 1) * 0x100 + s_dbgRoom);

    g_pendingDoorRecord = (int)s_dbgDoorRecord;
    g_main_state_flags |= MSF_GAMEPLAY_ACTIVE;
    g_message_flags = 0;

    g_rect.textureId = 0;
    g_rect.x = -160;
    g_rect.y = -120;
    g_rect.w = 320;
    g_rect.h = 240;
    g_rect.r = 0;
    g_rect.g = 0;
    g_rect.b = 0;

    g_openMenuFlag = 1;

    draw_rect(&g_rect, 0, 0);
    Task_sleep(1);
    StMask(0, 0);
}

static void DebugRoomChange_Draw(void)
{
    DebugMenu_DrawBox();

    DebugMenu_PrintCentered(76, "- ROOM CHANGE -", DBGCOL_TEXT);

    // Room id in hex: (stage + 1) * 0x100 + room, so stage 0 room 0 is
    // ROOM 100, stage 4 room 0x06 is ROOM 506.
    char line[64];
    sprintf(line, "ROOM %X", (s_dbgStage + 1) * 0x100 + s_dbgRoom);
    DebugMenu_PrintCentered(106, line, DBGCOL_TEXT);

    sprintf(line, "%s", s_dbgStageRooms[s_dbgStage].names[s_dbgRoom]);
    DebugMenu_PrintCentered(122, line, DBGCOL_TEXT);

    DebugMenu_PrintCentered(150, "LEFT/RIGHT: STAGE   UP/DOWN: ROOM", DBGCOL_HINT);
    DebugMenu_PrintCentered(162, "ENTER: CHANGE ROOM   F1/ESC: BACK", DBGCOL_HINT);
}

// ============================================================================
// DebugRoomChange_ApplyPendingPlacement
// game_loop calls this right after room_transition_load when a menu-path room
// load finished. Only acts on debug-menu transitions (s_dbgRoomChangeArmed).
//
// Places the player at the destination room's first door: scans the freshly
// loaded room's action table (g_RoomActionTable, 12-byte entries built
// by cmd_door_set / 0x004611b0) for the first door entry (action id 1 =
// door_try_enter, room_check_actions[1]) and stands the player just outside
// that door's action zone, which is the 4-u16 box at record+0x00 (x, z,
// width, depth - the same box is_point_in_action_zone / 0x0041b3c0 tests).
// Facing +x (directionAngle 0) keeps the 600-unit reach probe pointed away
// from the zone so the door does not instantly trigger back.
// ============================================================================
// Is (px, pz) inside one of the room's walkable zones (RDT+0x58 NPC grid)?
// Containment is half-open per the RDT doc: x in [x1,x2), z in [z1,z2).
static int DebugRoom_InWalkZone(int px, int pz)
{
    if (g_RdtPointer == NULL || g_RdtPointer->walk_zones == NULL) {
        return 1;               // no grid data: fall back to the blocking test alone
    }
    unsigned char* table = (unsigned char*)g_RdtPointer->walk_zones;
    unsigned char  count = table[0];
    unsigned char* rec = table + 2;
    for (int i = 0; i < count; i++, rec += 0xC) {
        short x1 = *(short*)(rec + 0);
        short z1 = *(short*)(rec + 2);
        short x2 = *(short*)(rec + 4);
        short z2 = *(short*)(rec + 6);
        if (px >= x1 && px < x2 && pz >= z1 && pz < z2) {
            return 1;
        }
    }
    return 0;
}

// Is (px, pz) fully clear for the player body? The blocking point query
// (0x0047da50) tests radius 0, so also probe the four body-radius offsets.
static int DebugRoom_SpotBlocked(int px, int pz)
{
    static const int r = 160;   // approx player body radius (Sca_info+10)

    VECTOR origin;              // zero base: offset is the absolute point
    VECTOR point;
    origin.x = 0; origin.y = 0; origin.z = 0; origin.pad = 0;

    static const int off[5][2] = { {0,0}, {r,0}, {-r,0}, {0,r}, {0,-r} };
    for (int i = 0; i < 5; i++) {
        point.x = px + off[i][0];
        point.y = 0;
        point.z = pz + off[i][1];
        point.pad = 0;
        if (room_collision_check_0047da50(&origin, &point) == 1) {
            return 1;
        }
    }
    return 0;
}

// Decode a door record's destination byte exactly the way room_transition_load
// does: < 0x20 is a room in `curStage`; otherwise the stage is (dest >> 5) - 1
// and the low five bits are the room, with the return-mansion remap (+5 for
// stages 0/1) applied once SCENARIO_FLAG_STAGE_VARIANT is set.
static void DebugRoom_DecodeDest(unsigned char dest, unsigned char curStage,
                                 unsigned char* outStage, unsigned char* outRoom)
{
    if (dest < 0x20) {
        *outStage = curStage;
        *outRoom  = dest;
    } else {
        unsigned char stage = (unsigned char)((dest >> 5) - 1);
        if (stage < 2 && Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_STAGE_VARIANT) != 0) {
            stage = (unsigned char)(stage + 5);
        }
        *outStage = stage;
        *outRoom  = (unsigned char)(dest & 0x1F);
    }
}

void DebugRoomChange_ApplyPendingPlacement(void)
{
    if (s_dbgRoomChangeArmed == 0) {
        return;
    }
    s_dbgRoomChangeArmed = 0;

    // Restore the stage-variant bit cleared by DebugRoomChange_Trigger.
    if (s_dbgRestoreVariant != 0) {
        Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_STAGE_VARIANT);
        s_dbgRestoreVariant = 0;
    }

    unsigned char* table = g_RoomActionTable;
    unsigned char* tail  = (unsigned char*)g_RoomActionTail;
    if (tail == NULL || tail < table) {
        dbg_printf("[debugmenu] room change: no room action table, player left at record entry\n");
        return;
    }

    for (unsigned char* entry = table; entry <= tail; entry += 12) {
        if (entry[0] != 1) {            // room_check_actions[1] = door
            continue;
        }
        unsigned char* rec = *(unsigned char**)(entry + 8);
        if (rec == NULL) {
            continue;
        }

        unsigned short zx = *(unsigned short*)(rec + 0x00);
        unsigned short zz = *(unsigned short*)(rec + 0x02);
        unsigned short zw = *(unsigned short*)(rec + 0x04);
        unsigned short zd = *(unsigned short*)(rec + 0x06);

        int cx = (int)zx + ((int)zw / 2);
        int cz = (int)zz + ((int)zd / 2);

        // ------------------------------------------------------------------
        // Authentic entry placement via the PAIRED door record.
        //
        // When the player walks through a door, the arrival point is encoded
        // in the door record of the room they are LEAVING - it lives in the
        // neighbour room's script, never in this room's own data (the dir
        // byte does NOT encode the edge; rooms with identical dir values
        // arrive on opposite zone sides). So: take this room's first door's
        // destination (the neighbour it connects to), load that neighbour's
        // RDT into a scratch buffer, and find its door record whose
        // destination points back here. Its arrival (x, y, z, angle) is the
        // exact spot the game uses when entering this room through the paired
        // door. Verified: ROOM20D0 (via ROOM2040) -> (3000,12600) ang 0x400,
        // ROOM1000 (via ROOM1010) -> (3500,2600) ang 0xC00, ROOM2060 (via
        // ROOM2040 door 2) -> (3000,2600) ang 0xC00, ROOM1040 (via ROOM1030)
        // -> (15800,3700) ang 0x400 - all matching normal gameplay.
        //
        // The neighbour may be in ANOTHER STAGE (dest >= 0x20, e.g. the
        // guardhouse entry hall's front door to the guardhouse gate). The
        // neighbour's RDT then lives in that stage's own directory, so the
        // path and the back-reference test are both derived from the decoded
        // destination rather than from g_stageId. Skipping this case used to
        // fall through to the dir heuristic, which picks a side from the dir
        // byte alone - and for ROOM4000 it picked the OUTSIDE of the front
        // door, stranding the player in the unwalled strip behind it.
        // ------------------------------------------------------------------
        int spotX = -1;
        int spotZ = -1;
        int spotY = 0;
        int spotAngle = -1;
        int pairedArrival = 0;      // spot came from the neighbour's door record

        unsigned char nStage, nRoom;
        DebugRoom_DecodeDest(rec[0x0D], g_stageId, &nStage, &nRoom);

        if (nStage != g_stageId || nRoom != g_roomId) {
            static const char hexDigits[] = "0123456789abcdef";
            char path[64];
            // The room file name is room<S><Rhi><Rlo><F>, so the stage digit
            // feeds both the directory and the first character of the name.
            sprintf(path, GAME_DATA_ROOT "stage%c\\room%c%c%c0.rdt",
                    hexDigits[nStage + 1],
                    hexDigits[nStage + 1],
                    hexDigits[nRoom >> 4],
                    hexDigits[nRoom & 0xF]);

            static unsigned char s_neighbourRdt[768 * 1024];   // largest shipped RDT is ~630KB
            size_t size = LoadFile(path, s_neighbourRdt, 1);
            if (size > 0x94 + 0x100) {
                unsigned int init = *(unsigned int*)(s_neighbourRdt + 0x60);
                if (init > 0x94 && init < size - 4) {
                    // The init script opens with 2 preamble bytes and its door
                    // records follow. Scan a bounded window for a door record
                    // pointing back at this room: opcode 0x0C, sane zone
                    // extents, sane type/cam/angle, dest == this room.
                    unsigned int end = init + 2048;
                    if (end > size - 26) {
                        end = size - 26;
                    }
                    for (unsigned int p = init + 2; p < end; p++) {
                        if (s_neighbourRdt[p] != 0x0C) {
                            continue;
                        }
                        unsigned char* r = s_neighbourRdt + p + 2;
                        unsigned short rzw = *(unsigned short*)(r + 4);
                        unsigned short rzd = *(unsigned short*)(r + 6);
                        // Zone extents: the widest shipped door zone is 6000
                        // and the deepest is an elevator shaft at 29400, so the
                        // bound has to clear those or the elevator/locked-door
                        // records are skipped and the room falls back to the
                        // heuristic.
                        if (rzw == 0 || rzw > 0x8000 || rzd == 0 || rzd > 0x8000) {
                            continue;
                        }
                        unsigned char bStage, bRoom;
                        DebugRoom_DecodeDest(r[13], nStage, &bStage, &bRoom);
                        // Door type indexes the 34-entry .dor name table
                        // (0x00-0x21: doorNN, mon, ele, kai, lad, doorNNk), so
                        // the old > 0x15 bound rejected every locked door and
                        // every stair/ladder record.
                        if (bStage != g_stageId || bRoom != g_roomId || r[10] > 0x21) {
                            continue;
                        }
                        unsigned short ang = *(unsigned short*)(r + 20);
                        if (ang > 0xFFF) {
                            continue;
                        }
                        spotX = *(unsigned short*)(r + 14);
                        spotY = (short)*(unsigned short*)(r + 16);
                        spotZ = *(unsigned short*)(r + 18);
                        spotAngle = (int)ang;
                        pairedArrival = 1;
                        dbg_printf("[debugmenu] room change: paired record in %s door %d -> arrival (%d,%d,%d) ang 0x%X\n",
                                   path, (int)r[-1], spotX, spotY, spotZ, spotAngle);
                        break;
                    }
                }
            }
        }

        // Fallback: the dir heuristic (edge of this room's own zone). Verified
        // only for a subset of rooms, so it is used only when the paired
        // record could not be found.
        if (spotAngle < 0) {
            switch (rec[0x08] & 3) {
            case 0:  spotX = cx;                spotZ = (int)zz + (int)zd + 300; spotAngle = 0xC00; break;
            case 1:  spotX = (int)zx + (int)zw + 300; spotZ = cz;          spotAngle = 0x000; break;
            case 2:  spotX = (int)zx - 300;     spotZ = cz;                spotAngle = 0x800; break;
            default: spotX = cx;                spotZ = (int)zz - 300;     spotAngle = 0x400; break;
            }
            dbg_printf("[debugmenu] room change: no paired record, using dir heuristic\n");
        }

        // Fall back through the zone-edge spots if the GUESSED spot is inside a
        // wall or off the walkable area.
        //
        // This net must NOT run on a paired-record arrival: that is the point
        // the game itself uses when the player walks through the door, so it is
        // valid by definition. It routinely lies OUTSIDE the RDT+0x58 grid -
        // which is the NPC navigation grid, not a player boundary - and it can
        // sit inside the door's own collision volume, where the collision pass
        // pushes the player clear on the first frame. Rejecting it is what sent
        // ROOM4000 to the heuristic's (3650,6300): the strip on the OUTSIDE of
        // the closed front door, with no camera covering it.
        if (spotX >  32000) spotX =  32000;
        if (spotX < -32000) spotX = -32000;
        if (spotZ >  32000) spotZ =  32000;
        if (spotZ < -32000) spotZ = -32000;
        if (pairedArrival == 0 &&
            (DebugRoom_InWalkZone(spotX, spotZ) == 0 ||
             DebugRoom_SpotBlocked(spotX, spotZ) != 0)) {
            struct DebugSpot { int x, z, angle; };
            DebugSpot spots[4] = {
                { cx,                      cz,                      0     },  // zone centre
                { (int)zx + (int)zw + 200, cz,                      0     },
                { (int)zx - 200,           cz,                      0x800 },
                { cx,                      (int)zz - 200,           0x400 },
            };
            for (int i = 0; i < 4; i++) {
                if (DebugRoom_InWalkZone(spots[i].x, spots[i].z) != 0 &&
                    DebugRoom_SpotBlocked(spots[i].x, spots[i].z) == 0) {
                    spotX = spots[i].x;
                    spotZ = spots[i].z;
                    spotAngle = spots[i].angle;
                    break;
                }
            }
            dbg_printf("[debugmenu] room change: spot rejected, using fallback\n");
        }

        // NOTE: the camera is NOT touched here. The synthetic door record
        // carries entry camera 0, Room_SetupCamera has already cut to it, and
        // check_camera_switch re-cuts as the player walks into trigger quads.
        // (The door record's cam byte is NOT a safe index: paired records
        // carry the neighbour-side camera id, which can exceed the loaded
        // room's camera count and crashes Room_LoadCameraSprites.)

        // X and Z are zero-extended into the 32-bit matrix translation,
        // Y is sign-extended (heights go negative) - the same asymmetry as
        // room_transition_load's own placement code.
        g_playerEntity.scaMatrixData.localMatrix.t[0] = spotX;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = spotY;
        g_playerEntity.scaMatrixData.localMatrix.t[2] = spotZ;
        g_playerEntity.position.x = (short)spotX;
        g_playerEntity.position.y = (short)spotY;
        g_playerEntity.position.z = (short)spotZ;
        g_playerEntity.posY = (unsigned short)spotY;
        g_playerEntity.directionAngle = (short)spotAngle;

        dbg_printf("[debugmenu] room change: placed at x=%d z=%d ang=0x%X cam=%d (door dir=%d zone %u,%u %ux%u)\n",
                   spotX, spotZ, (int)g_playerEntity.directionAngle,
                   (int)g_roomCameraId, (int)(rec[0x08] & 3),
                   (unsigned int)zx, (unsigned int)zz,
                   (unsigned int)zw, (unsigned int)zd);
        return;
    }

    dbg_printf("[debugmenu] room change: no door record in loaded room, player left at record entry\n");
}

// ============================================================================
// Inventory editor (third menu context)
//
// Slot count for the active character: Jill (id & 3 == 1) gets 8 slots,
// Chris and Rebecca 6 - the same rule as CountHeldItems (0x00451600) and
// g_totalInventorySlots (0x00ae9f1a, main_menu 0x00463710).
static int DebugInv_Capacity(void)
{
    return ((g_playerEntity.id & 3) == CHAR_JILL) ? 8 : 6;
}

// Decode an item name from g_ItemNamePointers (0x004bf0a0, indexed by
// itemId-1) into ASCII for the 8x8 font. The table's strings are RE1 8x14
// font encoded (PrintText.h encodeChar): A-Z at 0x1D+, a-z at 0x3D+, digits
// at 0x0C+, space 0x00, terminator 0x07. Item id 0 is the empty slot.
static void DebugInv_ItemName(unsigned char itemId, char* out)
{
    if (itemId == ITEM_NONE) {
        strcpy(out, "NOTHING");
        return;
    }
    if (itemId > 128) {
        sprintf(out, "ITEM %02X", itemId);
        return;
    }

    const unsigned char* p = g_ItemNamePointers[itemId - 1];
    int o = 0;
    for (; *p != 0x07 && *p != 0x01 && o < 24; p++) {
        unsigned char c = *p;
        char a;
        if (c >= 0x1D && c <= 0x36)      a = (char)(c - 0x1D + 'A');
        else if (c >= 0x3D && c <= 0x56) a = (char)(c - 0x3D + 'a');
        else if (c >= 0x0C && c <= 0x15) a = (char)(c - 0x0C + '0');
        else if (c == 0x00) a = ' ';
        else if (c == 0x19) a = '"';
        else if (c == 0x18) a = ',';
        else if (c == 0x16) a = ':';
        else if (c == 0x17) a = ';';
        else if (c == 0x1A) a = '!';
        else if (c == 0x1B) a = '?';
        else if (c == 0x79) a = '.';
        else if (c == 0x3A) a = '\'';
        else if (c == 0x37) a = '(';
        else if (c == 0x39) a = ')';
        else if (c == 0x3B) a = '-';
        else if (c == 0x38) a = '/';
        else a = '?';
        out[o++] = a;
    }
    out[o] = 0;
    if (o == 0) {
        strcpy(out, "UNKNOWN");
    }
}

// Rebuild the HUD's inventory bookkeeping after an edit: CountHeldItems +
// g_ItemSlotsBitmask + g_ItemSlotIndices + the HUD icon textures
// (LoadHeldItemsImages, 0x00451640).
static void DebugInv_Apply(void)
{
    LoadHeldItemsImages();
}

static void DebugInv_DrawRow(short y, int slot, unsigned char itemId, unsigned char qty)
{
    char name[28];
    char qtyText[8];
    DebugInv_ItemName(itemId, name);

    if (slot == s_dbgInvSlot) {
        sprintf(PRINT_TEXT_BUFFER, ">");
        PrintText8x8(32, y, DBGCOL_TEXT, 0);
    }
    sprintf(PRINT_TEXT_BUFFER, "%s", name);
    PrintText8x8((short)(32 + 16), y, DBGCOL_TEXT, 0);

    // Right-aligned "x<qty>", mirroring the original's quantity column.
    sprintf(qtyText, "x%d", qty);
    sprintf(PRINT_TEXT_BUFFER, "%s", qtyText);
    PrintText8x8((short)(288 - (int)strlen(qtyText) * 8), y, DBGCOL_TEXT, 0);
}

static void DebugInv_DrawList(void)
{
    DebugMenu_DrawBox();

    DebugMenu_PrintCentered(76, "- INVENTORY EDITOR -", DBGCOL_TEXT);

    char line[64];
    sprintf(line, "INVENTORY CAPACITY [%d]", DebugInv_Capacity());
    DebugMenu_PrintCentered(90, line, DBGCOL_RED);

    if (g_ItemSlotsPointer == NULL) {
        DebugMenu_PrintCentered(130, "NO INVENTORY", DBGCOL_DIM);
        return;
    }

    unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
    short y = 102;
    for (int i = 0; i < DebugInv_Capacity(); i++) {
        DebugInv_DrawRow(y, i, slots[i * 2], slots[i * 2 + 1]);
        y += 11;
    }

    DebugMenu_PrintCentered(190, "UP/DOWN: SLOT   LEFT/RIGHT: QTY", DBGCOL_TEXT);
}

static void DebugInv_DrawPick(void)
{
    DebugMenu_DrawBox();

    DebugMenu_PrintCentered(76, "- INVENTORY EDITOR -", DBGCOL_TEXT);

    char line[64];
    sprintf(line, "SLOT %d", s_dbgInvSlot + 1);
    DebugMenu_PrintCentered(96, line, DBGCOL_HINT);

    char name[28];
    DebugInv_ItemName((unsigned char)s_dbgPickId, name);
    DebugMenu_PrintCentered(116, name, DBGCOL_TEXT);

    if (s_dbgPickId != ITEM_NONE) {
        sprintf(line, "ID %02X", s_dbgPickId);
        DebugMenu_PrintCentered(134, line, DBGCOL_HINT);
    }

    DebugMenu_PrintCentered(152, "LEFT/RIGHT: ID-1/+1", DBGCOL_TEXT);
    DebugMenu_PrintCentered(164, "UP/DOWN: ID-16/+16", DBGCOL_TEXT);
    DebugMenu_PrintCentered(178, "ENTER: SET   ESC: CANCEL", DBGCOL_TEXT);
}

// ============================================================================
// Flag editor (fourth menu context)
//
// Edits the raw flag bitfields read by cmd_bit_test (0x00460570) and written
// by cmd_bit_op (0x00460650): the ten SCD flag banks of their switch.
//
// Addressing is Flg_on's (0x00473ef0) / Flg_ck's (0x00473f40), which is NOT
// byte-linear: a flag id picks a DWORD (id >> 5) and then a bit counted from
// that dword's MSB (mask 0x80000000 >> (id & 0x1F)). Little-endian puts ids
// 0-7 in the dword's LAST byte, so the memory byte is
//     (id >> 5) * 4 + 3 - ((id >> 3) & 3)
// and the bit within it is 0x80 >> (id & 7). See DebugFlag_BitAddr - editing
// with the obvious byte-linear formula writes a different byte of the same
// dword, which is what this editor used to do.
//
// Each row renders 16 consecutive flag ids as '0'/'1' glyphs with the row's
// first id as a hex prefix ("00 00000000 00000000"), the selected bit drawn in
// green, like the PS1 debug menu's FLAG EDITOR screen.
// ============================================================================

// cmd_bit_test's bank switch (case 0..9). Banks 0/1/2/3/7/8 live inside the
// BioCard block (BioCard.h), 4/5/6/9 are standalone globals.
static void DebugFlag_GetBank(int bank, const char** name, unsigned char** data, int* bytes)
{
    switch (bank) {
    case 0:  *name = "SCENARIO";   *data = (unsigned char*)g_ScenarioFlags;     *bytes = 16; break;
    case 1:  *name = "SCENARIO2";  *data = (unsigned char*)g_ScenarioFlags2;    *bytes = 32; break;
    case 2:  *name = "LOCKS";      *data = (unsigned char*)g_LocksFlags;        *bytes = 8;  break;
    case 3:  *name = "ENEMIES";    *data = (unsigned char*)g_EnemiesFlags;      *bytes = 32; break;
    case 4:  *name = "SYSTEM";     *data = (unsigned char*)g_SysFlags;          *bytes = 8;  break;
    case 5:  *name = "STATE";      *data = (unsigned char*)g_MainStateFlagBank;  *bytes = 8;  break;
    case 6:  *name = "MESSAGE";    *data = (unsigned char*)&g_message_flags;    *bytes = 2;  break;
    case 7:  *name = "ROOM ITEMS"; *data = (unsigned char*)g_roomItemsFlags;    *bytes = 32; break;
    case 8:  *name = "ROOM";       *data = (unsigned char*)g_RoomFlags;         *bytes = 20; break;
    default: *name = "ITEM USE";   *data = (unsigned char*)g_itemUseFlags;      *bytes = 8;  break;
    }
}

// Flag id -> memory byte + mask, exactly as Flg_on / Flg_ck compute it.
static void DebugFlag_BitAddr(int id, int* byteIdx, unsigned char* mask)
{
    *byteIdx = (id >> 5) * 4 + 3 - ((id >> 3) & 3);
    *mask    = (unsigned char)(0x80 >> (id & 7));
}

// The 16 documents live at ROOM_FLAG_FILE_BASE + (itemId - 0x5F).
#define DBGFLAG_FILE_COUNT      16
#define DBGFLAG_FILE_FIRST_ITEM 0x5F

// Per-bit labels for the two g_RoomFlags blocks that have them. The visited
// groups are indexed by (stageId % 5), the same index g_StageRoomFlagOffset
// takes; the map names are indexed by MAP_INDEX_*.
static const char* const s_dbgVisitedGroups[5] = {
    "MANSION 1F", "MANSION 2F", "COURTYARD", "GUARDHOUSE", "LABORATORY",
};
// g_main_state_flags / g_main_state_flags2 bit names, indexed by FLAG ID
// (id = 31 - bitNumber, so these read high bit first). Empty = no constant.
static const char* const s_dbgMsfNames[32] = {
    "SCREEN_REBUILD",     "SCREEN_STANDALONE", "FADE_ACTIVE",      "CONTINUE_GAME",
    "UNUSED_27",          "ROOM_TRANSITION",   "GAMEPLAY_ACTIVE",  "PLAYER_DEAD",
    "CHAR_VARIANT",       "OPTIONS_REQUEST",   "UNUSED_21",        "CAMERA_LOCK",
    "PANNING_RESET",      "FMV_REQUEST",       "VOICE_PLAYING",    "INTENSITY_RAMP",
    "MENU_ACTIVE",        "SCRIPT_ONLY_14",    "MODE1_KEY_DEPLET", "MODE2_ITEMBOX",
    "MODE3_ITEM_VIEW",    "MODE4_GOT_ITEM",    "MODE5_MAP_ITEM",   "PICKUP_SCREEN",
    "DOOR_TRANSITION",    "OBJECT_PUSH",       "CAMERA_REDRAW",    "LADDER_DOWN",
    "SCRIPT_ONLY_03",     "CAMERA_DEFER",      "MIRROR_PLANE_X",   "MIRROR_ENABLE",
};
static const char* const s_dbgMsf2Names[32] = {
    "DEATH_VARIANT",      "",                  "PLAYER_INITIALISED", "ATTRACT_DEMO",
    "COUNTDOWN_ACTIVE",   "COSTUME_VARIANT",   "ROOM_SPRITES_OFF", "DOOR_ANGLE_STEP",
    "SND_BUSY",           "DOOR_TURN_PENDING", "SFX_BANK1_HALF",   "",
    "PRESERVED_19",       "",                  "",                 "",
    "",                   "",                  "",                 "",
    "",                   "",                  "",                 "",
    "",                   "",                  "",                 "",
    "FADE_NO_DEPTH_CLMP", "SCREEN_BORDER",     "SCREEN_SHAKE",     "EFFECT_ZONE",
};

static const char* const s_dbgMapNames[MAP_INDEX_COUNT] = {
    "MANSION 1F", "MANSION 2F", "COURTYARD", "UNDERGROUND", "GUARDHOUSE", "LABORATORY",
};

// ----------------------------------------------------------------------------
// Views
//
// A view is a bit WINDOW into a bank, so one bank can be edited as several
// independent blocks. Only g_RoomFlags needs it: bank 8 is three unrelated bit
// blocks packed into one 20-byte array (see BioCard.h), and editing it as a
// single 160-bit run makes it far too easy to flip a room-visited bit while
// aiming for a map bit - the block boundaries are mid-byte (0x7C and 0x82).
//
// Bits outside the active window are still drawn, dimmed, so the neighbouring
// block stays visible for context; the cursor cannot reach them.
//
// s_dbgFlagBit stays ABSOLUTE within the bank, so the "BIT" readout is the
// number an SCD script would pass to cmd_bit_op.
// ----------------------------------------------------------------------------
typedef struct {
    int         bank;       // cmd_bit_test bank id
    const char* label;
    int         firstBit;   // first editable bit
    int         bitCount;   // 0 = the whole bank
} DebugFlagView;

static const DebugFlagView s_dbgFlagViews[] = {
    { 0, "SCENARIO",      0,                    0  },
    { 1, "SCENARIO2",     0,                    0  },
    { 2, "LOCKS",         0,                    0  },
    { 3, "ENEMIES",       0,                    0  },
    { 4, "SYSTEM",        0,                    0  },
    { 5, "STATE (msf)",   0,                    32 },
    { 5, "STATE2 (msf2)", 32,                   32 },
    { 6, "MESSAGE",       0,                    0  },
    { 7, "ROOM ITEMS",    0,                    0  },
    // Bank 8's three blocks, split out. Bases and sizes from BioCard.h.
    { 8, "ROOM VISITED",  0,                    ROOM_FLAG_MAP_BASE          },
    { 8, "ROOM MAPS",     ROOM_FLAG_MAP_BASE,   MAP_INDEX_COUNT             },
    { 8, "ROOM FILES",    ROOM_FLAG_FILE_BASE,  DBGFLAG_FILE_COUNT          },
    { 8, "ROOM UNUSED",   ROOM_FLAG_FILE_BASE + DBGFLAG_FILE_COUNT,
                          160 - (ROOM_FLAG_FILE_BASE + DBGFLAG_FILE_COUNT)  },
    { 9, "ITEM USE",      0,                    0  },
};

#define DBGFLAG_VIEWS ((int)(sizeof(s_dbgFlagViews) / sizeof(s_dbgFlagViews[0])))

// Resolve a view to its bank storage and bit window.
static void DebugFlag_GetView(int view, const DebugFlagView** outView,
                              unsigned char** data, int* firstBit, int* bitCount)
{
    const DebugFlagView* v = &s_dbgFlagViews[view];
    const char* bankName;
    int bytes;
    DebugFlag_GetBank(v->bank, &bankName, data, &bytes);
    *outView   = v;
    *firstBit  = v->firstBit;
    *bitCount  = (v->bitCount != 0) ? v->bitCount : bytes * 8;
}

// Name the bit under the cursor where the block has a known per-bit meaning.
// Everything else (and every bank but 8) gets an empty string.
static void DebugFlag_DescribeBit(const DebugFlagView* v, int bit, char* out)
{
    out[0] = 0;

    if (v->bank == 5) {
        // ids 0x00-0x1F are msf, 0x20-0x3F are msf2 (the bank's second dword)
        const int second = (bit >= 32);
        const char* n = second ? s_dbgMsf2Names[bit & 0x1f] : s_dbgMsfNames[bit & 0x1f];
        if (n[0] != 0) {
            sprintf(out, "%s%s", second ? "MSF2_" : "MSF_", n);
        } else {
            sprintf(out, "%sbit %d - no constant", second ? "msf2 " : "msf ",
                    31 - (bit & 0x1f));
        }
        return;
    }

    if (v->bank != 8) return;

    if (bit >= ROOM_FLAG_FILE_BASE && bit < ROOM_FLAG_FILE_BASE + DBGFLAG_FILE_COUNT) {
        sprintf(out, "FILE %02X  ITEM %02X", bit - ROOM_FLAG_FILE_BASE,
                bit - ROOM_FLAG_FILE_BASE + DBGFLAG_FILE_FIRST_ITEM);
    } else if (bit >= ROOM_FLAG_MAP_BASE && bit < ROOM_FLAG_MAP_BASE + MAP_INDEX_COUNT) {
        sprintf(out, "MAP  %s", s_dbgMapNames[bit - ROOM_FLAG_MAP_BASE]);
    } else if (bit < ROOM_FLAG_MAP_BASE) {
        // Rooms visited: the group is the last g_StageRoomFlagOffset base at or
        // below the bit (the bases are the running sum of g_MapRoomCounts).
        int group = 0;
        for (int i = 0; i < 5; i++) {
            if (bit >= (int)g_StageRoomFlagOffset[i]) group = i;
        }
        sprintf(out, "%s ROOM %02X", s_dbgVisitedGroups[group],
                bit - (int)g_StageRoomFlagOffset[group]);
    }
}

// Read the aim action's bound key from the "Key Def" binding table
// (g_keyBindingData[10], 'X' by default - see Globals.cpp). The options menu
// can rebind it (registry / save block), so it is read live. The pad's aim
// binding is the same modifier - g_RawPadHeld carries it whichever button it
// sits on.
static int DebugFlag_AimKeyHeld(void)
{
    if (g_keyBindingData[10] != 0 && (plat_key_state(g_keyBindingData[10]) & 0x8000)) {
        return 1;
    }
    return (g_RawPadHeld & DBGPAD_AIM) ? 1 : 0;
}

// One row: hex byte offset + 16 bits as '0'/'1'. Each glyph gets a colour -
// green for the cursor, dim for bits outside the active view's window, white
// otherwise - and the row is emitted as maximal same-colour runs, so a row
// still costs one or two PrintText8x8 calls in the common case.
static void DebugFlag_DrawRow(short y, unsigned char* data, int base, int cursor,
                              int firstBit, int lastBit)
{
    char text[24];
    unsigned char col[24];

    sprintf(text, "%02X ", base);          // the row's first FLAG ID, not a byte
    int n = 3;
    for (int i = 0; i < n; i++) col[i] = DBGCOL_TEXT;

    for (int i = 0; i < 16; i++) {
        if (i == 8) {
            text[n] = ' ';
            col[n] = DBGCOL_TEXT;
            n++;
        }
        int bit = base + i;
        int byteIdx; unsigned char bmask;
        DebugFlag_BitAddr(bit, &byteIdx, &bmask);
        text[n] = (data[byteIdx] & bmask) ? '1' : '0';
        // Off-window bits stay at the 50% grey ceiling so they read as dim
        // against the white in-window bits (DBGCOL_HINT is lighter now).
        col[n] = (bit == cursor)                        ? DBGCOL_GREEN
               : (bit < firstBit || bit > lastBit)      ? DBGCOL_DIM
                                                        : DBGCOL_TEXT;
        n++;
    }
    text[n] = 0;

    const int x = 80;                   // game-space column of the row
    int i = 0;
    while (i < n) {
        int j = i;
        while (j < n && col[j] == col[i]) j++;
        char saved = text[j];
        text[j] = 0;
        sprintf(PRINT_TEXT_BUFFER, "%s", text + i);
        PrintText8x8((short)(x + i * 8), y, col[i], 0);
        text[j] = saved;
        i = j;
    }
}

static void DebugFlag_Draw(void)
{
    // Taller box: game-space 32..228 - fits 16 rows of 16 bits.
    DebugMenu_DrawBoxSize(24, 32, 272, 196);

    DebugMenu_PrintCentered(40, "- FLAG EDITOR -", DBGCOL_TEXT);

    const DebugFlagView* v;
    unsigned char* data;
    int firstBit, bitCount;
    DebugFlag_GetView(s_dbgFlagView, &v, &data, &firstBit, &bitCount);
    const int lastBit = firstBit + bitCount - 1;

    sprintf(PRINT_TEXT_BUFFER, "%s", v->label);
    PrintText8x8(80, 54, DBGCOL_TEXT, 0);

    char line[32];
    sprintf(line, "BIT %02X", s_dbgFlagBit);
    sprintf(PRINT_TEXT_BUFFER, "%s", line);
    PrintText8x8((short)(288 - (int)strlen(line) * 8), 54, DBGCOL_HINT, 0);

    // Rows stay aligned to the 16-bit grid so the byte-offset prefix keeps
    // meaning even when a window starts mid-byte (0x7C, 0x82).
    short y = 68;
    for (int base = firstBit & ~15; base <= (lastBit | 15); base += 16) {
        DebugFlag_DrawRow(y, data, base, s_dbgFlagBit, firstBit, lastBit);
        y += 8;
    }

    // What the selected bit means, where the block has per-bit labels.
    DebugFlag_DescribeBit(v, s_dbgFlagBit, line);
    if (line[0] != 0) {
        sprintf(PRINT_TEXT_BUFFER, "%s", line);
        PrintText8x8(80, (short)(y + 4), DBGCOL_GREEN, 0);
    }

    DebugMenu_PrintCentered(202, "LEFT/RIGHT: BIT  UP/DOWN: +16", DBGCOL_TEXT);
    DebugMenu_PrintCentered(210, "AIM+LEFT/RIGHT: CLEAR/SET BIT", DBGCOL_TEXT);
    DebugMenu_PrintCentered(218, "AIM+UP/DOWN: VIEW  ESC: BACK", DBGCOL_TEXT);
}

// ============================================================================
// Quick access (fifth menu context) and its slot lists (sixth/seventh)
// In-menu shortcuts that bypass the real save/load screens:
//   SAVE MENU -> a slot list; confirming writes the current game's bio card
//                to savedat<N>.dat inline (DebugQuick_SaveSlot, the same
//                0x800 block write the DebugSaveMenu / save screen use), so
//                the slot shows up on the real load screen too. No fade, no
//                screen change, the game stays frozen underneath.
//   LOAD MENU -> a slot list; confirming closes the debug menu and arms
//                g_debugOpenLoadScreenFlag + g_debugLoadSlot. GameLoop.cpp's
//                quick access machine fades out, restores the card
//                (DebugQuick_LoadSlot) and restarts through game_start.
//   ITEMBOX   -> g_debugOpenItemboxFlag (the F3 path: menu mode 2 through
//                g_openMenuFlag, GameLoop.cpp)
// ============================================================================

static const char* const s_dbgQuickItems[3] = {
    "SAVE MENU",
    "LOAD MENU",
    "ITEMBOX",
};
static int           s_dbgQuickSlot = 0;        // slot lists: selected slot
static int           s_dbgQuickMode = 0;        // slot lists: 0 = save, 1 = load

// One save file's summary, read by DebugQuick_ScanSlots when a slot list
// opens. Offsets are the save screen's (SaveLoadScreen.cpp OFFSET_* defines,
// fields of the 0x800 bio-card block the file starts with).
struct DebugQuickSlotInfo {
    int hasData;
    int characterId;    // file 0x22B
    int stageId;        // file 0x200
    int roomId;         // file 0x201
    int savesCount;     // file 0x228
    int dcGameMode;     // file 0x233 (g_DcGameMode); port-only, see below
};
static DebugQuickSlotInfo s_dbgQuickSlotInfo[8];

// mirror of SaveLoadScreen.cpp's SaveSlotLoadBlocked. In OG mode a
// DC TRAINING/ADVANCED/ADVANCED* save cannot be loaded (its rules exist only in
// DC mode); the row is dimmed and the quick load is refused. Overwriting stays
// allowed through the save list.
static int DebugQuickSlotLoadBlocked(const DebugQuickSlotInfo* info)
{
    if (g_bDcMode || !info->hasData) {
        return 0;
    }
    return (info->dcGameMode & 3) != DC_DIFFICULTY_STANDARD;
}

static void DebugQuick_ScanSlots(void)
{
    char buffer[0xA90];                 // full save file (0xA82) + margin
    for (int i = 0; i < 8; i++) {
        DebugQuickSlotInfo* info = &s_dbgQuickSlotInfo[i];
        info->hasData = 0;
        sprintf(g_saveFileName, "%ssavedat%d.dat", GetSaveRoot(), i + 1);
        int size = ReadSaveFile(g_saveFileName, buffer);
        if (size >= 0x200) {
            info->hasData     = 1;
            info->characterId = (unsigned char)buffer[0x22B];
            info->stageId     = (unsigned char)buffer[0x200];
            info->roomId      = (unsigned char)buffer[0x201];
            info->savesCount  = (unsigned char)buffer[0x228];
            info->dcGameMode  = (unsigned char)buffer[0x233];
        }
    }
}

static void DebugQuick_DrawSlotList(void)
{
    DebugMenu_DrawBox();

    DebugMenu_PrintCentered(76, (s_dbgQuickMode == 0) ? "- QUICK SAVE -" : "- QUICK LOAD -",
                            DBGCOL_TEXT);

    short y = 92;
    for (int i = 0; i < 8; i++, y += 11) {
        char line[40];
        unsigned char color = DBGCOL_TEXT;
        if (s_dbgQuickSlotInfo[i].hasData) {
            // Character naming rule of the save screen: id & 3 == 1 is Jill
            const char* chName = ((s_dbgQuickSlotInfo[i].characterId & 3) == CHAR_JILL)
                                     ? "JILL" : "CHRIS";
            // Room id in the room change screen's hex form: (stage+1)*0x100+room
            sprintf(line, "%d %-5s ROOM %X", i + 1, chName,
                    (s_dbgQuickSlotInfo[i].stageId + 1) * 0x100 + s_dbgQuickSlotInfo[i].roomId);
            // a DC TRAINING/ADVANCED save is dimmed, and in OG mode
            // its load is refused (overwriting it from the save list is fine).
            if (DebugQuickSlotLoadBlocked(&s_dbgQuickSlotInfo[i])) {
                color = DBGCOL_DIM;
                strcat(line, " [DC]");
            }
        } else {
            sprintf(line, "%d EMPTY", i + 1);
            color = DBGCOL_DIM;
        }

        if (i == s_dbgQuickSlot) {
            sprintf(PRINT_TEXT_BUFFER, ">");
            PrintText8x8(64, y, DBGCOL_TEXT, 0);
        }
        sprintf(PRINT_TEXT_BUFFER, "%s", line);
        PrintText8x8((short)(64 + 16), y, color, 0);
    }

    if (s_dbgQuickMode == 0) {
        DebugMenu_PrintCentered(186, "ENTER: SAVE   F1/ESC: BACK", DBGCOL_HINT);
    } else {
        DebugMenu_PrintCentered(186, "ENTER: LOAD   F1/ESC: BACK", DBGCOL_HINT);
    }
}

static void DebugQuick_Draw(void)
{
    DebugMenu_DrawBox();

    DebugMenu_PrintCentered(76, "- QUICK ACCESS -", DBGCOL_TEXT);

    short y = 96;
    for (int i = 0; i < 3; i++) {
        if (i == s_dbgQuickCursor) {
            sprintf(PRINT_TEXT_BUFFER, ">");
            PrintText8x8(64, y, DBGCOL_TEXT, 0);
        }
        sprintf(PRINT_TEXT_BUFFER, "%s", s_dbgQuickItems[i]);
        PrintText8x8((short)(64 + 16), y, DBGCOL_TEXT, 0);

        y += 12;
    }

    DebugMenu_PrintCentered(186, "ENTER: SELECT   F1/ESC: BACK", DBGCOL_HINT);
}

// ============================================================================
// Main menu (first context)
// ============================================================================

static void DebugMenu_DrawMain(void)
{
    // 12px taller than the standard box: the control hint does not fit one
    // 272px line (see below), so it needs a second row at y=202.
    DebugMenu_DrawBoxSize(24, 64, 272, 148);

    DebugMenu_PrintCentered(76, "- DEBUG MENU -", DBGCOL_TEXT);

    short y = 92;
    for (int i = 0; i < 8; i++) {
        unsigned char color;
        if (i == s_dbgCursor) {
            color = DBGCOL_TEXT;
        } else if (s_dbgItemImplemented[i]) {
            color = DBGCOL_TEXT;
        } else {
            color = DBGCOL_DIM;
        }

        if (i == s_dbgCursor) {
            sprintf(PRINT_TEXT_BUFFER, ">");
            PrintText8x8(64, y, DBGCOL_TEXT, 0);
        }
        sprintf(PRINT_TEXT_BUFFER, "%s", s_dbgMenuItems[i]);
        PrintText8x8((short)(64 + 16), y, color, 0);

        y += 11;
    }

    // Play timer, bottom-right like the PS1 debug menu: "HH:MM:SS FF'".
    // Game_timer (0x00d22730) counts frames at 30 fps - the same divisors
    // EndingScreen.cpp uses for the results screen: 0x1A5E0 = 1h, 0x708 = 1min,
    // 0x1E = 1s; the trailing number is the frame within the second.
    {
        char timer[24];
        unsigned int t = (unsigned int)Game_timer;
        sprintf(timer, "%02u:%02u:%02u %02u'",
                (unsigned int)(t / 0x1A5E0),
                (unsigned int)((t / 0x708) % 60),
                (unsigned int)((t / 0x1E) % 60),
                (unsigned int)(t % 0x1E));
        sprintf(PRINT_TEXT_BUFFER, "%s", timer);
        PrintText8x8((short)(288 - (int)strlen(timer) * 8), 182, DBGCOL_RED, 0);
    }

    // Port version (Version.h, rewritten by release-please when a release is
    // cut), bottom-left on the same row as the play timer. Not in the
    // original - it is here so a screenshot of this menu identifies the
    // build it came from.
    sprintf(PRINT_TEXT_BUFFER, "VER %s", GAME_VERSION_STRING);
    PrintText8x8(32, 182, DBGCOL_HINT, 0);

    // Two lines: the combined close/select hint is 42 characters (336px) and
    // would be clipped at both edges of the 272px box on one line.
    DebugMenu_PrintCentered(192, "F1/L1+R1/ESC: CLOSE", DBGCOL_HINT);
    DebugMenu_PrintCentered(202, "ENTER/ACTION: SELECT", DBGCOL_HINT);
}

// ============================================================================
// debug_menu_overlay
// Called by game_loop once per frame, both open and closed (it owns the F1
// toggle edge detection). Returns 1 while the menu is open - game_loop then
// freezes the pad state and skips update_entities so the game is paused
// underneath.
//
// Controls (a game pad works throughout - the pad equivalents are listed
// once here rather than repeated on every line below):
//   F1, or pad L1+R1 together : open / close
//   pad d-pad / stick         : the UP / DOWN / LEFT / RIGHT entries
//   pad ACTION button         : the ENTER / SPACE entries
//   pad CANCEL button         : the ESC entries
//   pad AIM button            : the "AIM key" modifier
// The pad reads its BOUND functions (g_RawPadHeld), so a rebind in the options
// menu carries over here; only the open/close combo is a fixed hardware one.
//
//   F1              : open / close
//   UP / DOWN       : move the cursor
//   ENTER / SPACE   : confirm
//   ESC             : close (or go back from the room change context)
//   LEFT / RIGHT    : room change - change stage
//   UP / DOWN       : room change - change room
//   UP / DOWN       : inventory - select slot
//   LEFT / RIGHT    : inventory - change quantity
//   ENTER           : inventory - open item-id picker for the slot
//   LEFT/RIGHT/UP/DOWN : inventory picker - change item id
//   UP / DOWN       : flag editor - move bit cursor by 16
//   LEFT / RIGHT    : flag editor - move bit cursor by 1
//   AIM key + LEFT/RIGHT : flag editor - clear / set the bit under the cursor
//   AIM key + UP/DOWN    : flag editor - previous / next flag bank
//   ENTER           : flag editor - toggle the bit under the cursor
//   UP / DOWN       : quick access - select entry
//   ENTER           : quick access - open the save / load slot list or the
//                     item box
//   UP / DOWN       : quick access save list - select slot
//   ENTER           : quick access save list - save the game to that slot
//   UP / DOWN       : quick access load list - select slot
//   ENTER           : quick access load list - load that slot (fade out,
//                     restore, restart through game_start)
// ============================================================================
int debug_menu_overlay(void)
{
    if (g_debugFeaturesEnabled == 0) {
        return 0;
    }

    // F1 / L1+R1 toggle: sampled every frame with edge detection so one press
    // is one toggle, whether the menu is open or closed. The pad combo is read
    // from the raw hardware mask, not g_RawPadHeld - see DBGPAD_TOGGLE.
    int padToggle = ((DWORD)read_sidewinder_pad() & DBGPAD_TOGGLE) == DBGPAD_TOGGLE;
    int f1 = ((plat_key_state(VK_F1) & 0x8000) != 0 || padToggle) ? 1 : 0;
    int f1Edge = f1 && !s_dbgPrevF1;
    s_dbgPrevF1 = f1;

    if (g_debugMenuOpen == 0) {
        if (f1Edge) {
            g_debugMenuOpen = 1;
            s_dbgContext = 0;
            s_dbgCursor = 0;
            // Clamp: only the seven base stages are listed.
            s_dbgStage = (int)g_stageId;
            if (s_dbgStage > DebugStageMax()) {
                s_dbgStage = DebugStageMax();
            }
            s_dbgRoom = g_roomId;
            if (s_dbgRoom >= s_dbgStageRooms[s_dbgStage].count) {
                s_dbgRoom = s_dbgStageRooms[s_dbgStage].count - 1;
            }
            s_dbgPrevKeys = DebugMenu_SampleKeys();
        }
        return g_debugMenuOpen;
    }

    // --- Input (edge detected) ---
    int keys = DebugMenu_SampleKeys();
    int newKeys = keys & ~s_dbgPrevKeys;
    s_dbgPrevKeys = keys;

    if (s_dbgContext == 0) {
        // --- Main menu ---
        if (newKeys & DBGKEY_UP) {
            s_dbgCursor = (s_dbgCursor + 7) % 8;
        }
        if (newKeys & DBGKEY_DOWN) {
            s_dbgCursor = (s_dbgCursor + 1) % 8;
        }

        if (newKeys & DBGKEY_CONFIRM) {
            if (s_dbgItemImplemented[s_dbgCursor]) {
                if (s_dbgCursor == 0) {
                    s_dbgContext = 1;       // room change
                } else if (s_dbgCursor == 5) {
                    s_dbgContext = 2;       // inventory editor
                    s_dbgInvSlot = 0;
                    s_dbgInvPick = 0;
                } else if (s_dbgCursor == 6) {
                    s_dbgContext = 3;       // flag editor
                    s_dbgFlagView = 4;      // SYSTEM, like the PS1 debug menu's opening page
                    s_dbgFlagBit = 0;
                } else if (s_dbgCursor == 7) {
                    s_dbgContext = 4;       // quick access
                    s_dbgQuickCursor = 0;
                }
                // Further implemented entries get their contexts here.
            }
        }

        if ((newKeys & DBGKEY_ESC) || f1Edge) {
            g_debugMenuOpen = 0;
            return 0;
        }

        DebugMenu_DrawMain();
        return 1;
    }

    // --- Inventory editor ---
    if (s_dbgContext == 2) {
        int cap = DebugInv_Capacity();
        if (s_dbgInvSlot >= cap) {
            s_dbgInvSlot = cap - 1;
        }

        if (s_dbgInvPick == 0) {
            // --- Slot list ---
            if (newKeys & DBGKEY_UP) {
                if (s_dbgInvSlot > 0) {
                    s_dbgInvSlot--;
                }
            }
            if (newKeys & DBGKEY_DOWN) {
                if (s_dbgInvSlot < cap - 1) {
                    s_dbgInvSlot++;
                }
            }
            if (newKeys & (DBGKEY_LEFT | DBGKEY_RIGHT)) {
                unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
                if (slots != NULL && slots[s_dbgInvSlot * 2] != ITEM_NONE) {
                    // Wraps: LEFT below 0 goes to the max quantity (0xFF) so a
                    // full stack can be set in one press, and vice versa.
                    int qty = slots[s_dbgInvSlot * 2 + 1];
                    qty += (newKeys & DBGKEY_RIGHT) ? 1 : -1;
                    if (qty < 0)   qty = 255;
                    if (qty > 255) qty = 0;
                    slots[s_dbgInvSlot * 2 + 1] = (unsigned char)qty;
                    DebugInv_Apply();
                }
            }
            if (newKeys & DBGKEY_CONFIRM) {
                unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
                if (slots != NULL) {
                    s_dbgPickId = slots[s_dbgInvSlot * 2];
                    s_dbgInvPick = 1;
                }
            }
            if ((newKeys & DBGKEY_ESC) || f1Edge) {
                s_dbgContext = 0;
                return 1;
            }

            DebugInv_DrawList();
            return 1;
        }

        // --- Item-id picker ---
        if (newKeys & DBGKEY_LEFT) {
            if (s_dbgPickId > 0) {
                s_dbgPickId--;
            }
        }
        if (newKeys & DBGKEY_RIGHT) {
            if (s_dbgPickId < ITEM_MINIMI) {        // highest shipped item id
                s_dbgPickId++;
            }
        }
        if (newKeys & DBGKEY_UP) {
            s_dbgPickId += 0x10;
            if (s_dbgPickId > ITEM_MINIMI) {
                s_dbgPickId = ITEM_MINIMI;
            }
        }
        if (newKeys & DBGKEY_DOWN) {
            s_dbgPickId -= 0x10;
            if (s_dbgPickId < 0) {
                s_dbgPickId = 0;
            }
        }
        if (newKeys & DBGKEY_CONFIRM) {
            unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
            slots[s_dbgInvSlot * 2] = (unsigned char)s_dbgPickId;
            if (s_dbgPickId == ITEM_NONE) {
                slots[s_dbgInvSlot * 2 + 1] = 0;    // empty slots carry no qty
            }
            DebugInv_Apply();
            dbg_printf("[debugmenu] inventory: slot %d set to item %02X qty %d\n",
                       s_dbgInvSlot, (int)s_dbgPickId, (int)slots[s_dbgInvSlot * 2 + 1]);
            s_dbgInvPick = 0;
        }
        if ((newKeys & DBGKEY_ESC) || f1Edge) {
            s_dbgInvPick = 0;
        }

        DebugInv_DrawPick();
        return 1;
    }

    // --- Flag editor ---
    if (s_dbgContext == 3) {
        const DebugFlagView* v;
        unsigned char* data;
        int firstBit, bits;
        DebugFlag_GetView(s_dbgFlagView, &v, &data, &firstBit, &bits);
        if (s_dbgFlagBit >= firstBit + bits) s_dbgFlagBit = firstBit + bits - 1;
        if (s_dbgFlagBit < firstBit)         s_dbgFlagBit = firstBit;
        const char* name = v->label;

        if (DebugFlag_AimKeyHeld()) {
            // AIM + UP/DOWN: previous / next view (cursor to the window start)
            if (newKeys & (DBGKEY_UP | DBGKEY_DOWN)) {
                s_dbgFlagView = (newKeys & DBGKEY_UP)
                              ? (s_dbgFlagView + DBGFLAG_VIEWS - 1) % DBGFLAG_VIEWS
                              : (s_dbgFlagView + 1) % DBGFLAG_VIEWS;
                DebugFlag_GetView(s_dbgFlagView, &v, &data, &firstBit, &bits);
                s_dbgFlagBit = firstBit;
                name = v->label;
            }
            // AIM + LEFT/RIGHT: clear / set the bit under the cursor, then
            // step one bit in that direction so a held sweep edits a run.
            if (newKeys & (DBGKEY_LEFT | DBGKEY_RIGHT)) {
                int set = (newKeys & DBGKEY_RIGHT) ? 1 : 0;
                int byteIdx; unsigned char mask;
                DebugFlag_BitAddr(s_dbgFlagBit, &byteIdx, &mask);
                if (set) {
                    data[byteIdx] |= mask;
                } else {
                    data[byteIdx] &= (unsigned char)~mask;
                }
                dbg_printf("[debugmenu] flag editor: bank %d (%s) bit %02X = %d\n",
                           v->bank, name, s_dbgFlagBit, set);
                s_dbgFlagBit = firstBit + (s_dbgFlagBit - firstBit + (set ? 1 : bits - 1)) % bits;
            }
        } else {
            // Cursor movement wraps inside the view's window, never into the
            // neighbouring block.
            int rel = s_dbgFlagBit - firstBit;
            if (newKeys & DBGKEY_LEFT)  rel = (rel + bits - 1) % bits;
            if (newKeys & DBGKEY_RIGHT) rel = (rel + 1) % bits;
            if (newKeys & DBGKEY_UP)    rel = (rel + bits - 16) % bits;
            if (newKeys & DBGKEY_DOWN)  rel = (rel + 16) % bits;
            s_dbgFlagBit = firstBit + rel;
            if (newKeys & DBGKEY_CONFIRM) {
                int byteIdx; unsigned char mask;
                DebugFlag_BitAddr(s_dbgFlagBit, &byteIdx, &mask);
                data[byteIdx] ^= mask;
                dbg_printf("[debugmenu] flag editor: bank %d (%s) bit %02X toggled to %d\n",
                           v->bank, name, s_dbgFlagBit,
                           (data[byteIdx] & mask) ? 1 : 0);
            }
        }

        if ((newKeys & DBGKEY_ESC) || f1Edge) {
            s_dbgContext = 0;
            return 1;
        }

        DebugFlag_Draw();
        return 1;
    }

    // --- Quick access ---
    if (s_dbgContext == 4) {
        if (newKeys & DBGKEY_UP) {
            s_dbgQuickCursor = (s_dbgQuickCursor + 2) % 3;
        }
        if (newKeys & DBGKEY_DOWN) {
            s_dbgQuickCursor = (s_dbgQuickCursor + 1) % 3;
        }

        if (newKeys & DBGKEY_CONFIRM) {
            switch (s_dbgQuickCursor) {
            case 0:
                s_dbgContext = 5;       // save slot list
                s_dbgQuickSlot = 0;
                s_dbgQuickMode = 0;
                DebugQuick_ScanSlots();
                return 1;
            case 1:
                s_dbgContext = 6;       // load slot list
                s_dbgQuickSlot = 0;
                s_dbgQuickMode = 1;
                DebugQuick_ScanSlots();
                return 1;
            case 2:
                g_debugOpenItemboxFlag = 1;
                dbg_printf("[debugmenu] quick access: itembox requested\n");
                g_debugMenuOpen = 0;
                return 0;       // the game loop's driver picks the flag up next frame
            }
        }

        if ((newKeys & DBGKEY_ESC) || f1Edge) {
            s_dbgContext = 0;
            return 1;
        }

        DebugQuick_Draw();
        return 1;
    }

    // --- Quick access slot list (save / load) ---
    if (s_dbgContext == 5 || s_dbgContext == 6) {
        s_dbgQuickMode = s_dbgContext - 5;

        if (newKeys & DBGKEY_UP) {
            s_dbgQuickSlot = (s_dbgQuickSlot + 7) % 8;
        }
        if (newKeys & DBGKEY_DOWN) {
            s_dbgQuickSlot = (s_dbgQuickSlot + 1) % 8;
        }

        if (newKeys & DBGKEY_CONFIRM) {
            if (s_dbgQuickMode == 0) {
                // Inline save: the game is frozen while the menu is up, so the
                // snapshot the helper takes is exactly what is on screen.
                DebugQuick_SaveSlot(s_dbgQuickSlot);
                DebugQuick_ScanSlots();     // refresh the list contents
            } else if (s_dbgQuickSlotInfo[s_dbgQuickSlot].hasData &&
                       !DebugQuickSlotLoadBlocked(&s_dbgQuickSlotInfo[s_dbgQuickSlot])) {
                // Load: hand the slot to GameLoop's quick access machine,
                // which fades out, restores the card and restarts through
                // game_start. Empty and greyed (DC TRAINING/ADVANCED in OG
                // mode) slots do nothing.
                g_debugLoadSlot = s_dbgQuickSlot;
                g_debugOpenLoadScreenFlag = 1;
                g_debugMenuOpen = 0;
                return 0;
            }
        }

        if ((newKeys & DBGKEY_ESC) || f1Edge) {
            s_dbgContext = 4;
            return 1;
        }

        DebugQuick_DrawSlotList();
        return 1;
    }

    // --- Room change ---
    int count = s_dbgStageRooms[s_dbgStage].count;

    if (newKeys & DBGKEY_LEFT) {
        if (s_dbgStage > 0) {
            s_dbgStage--;
            if (s_dbgRoom >= s_dbgStageRooms[s_dbgStage].count) {
                s_dbgRoom = s_dbgStageRooms[s_dbgStage].count - 1;
            }
            DebugRoomSkipInvalid(1);
        }
    }
    if (newKeys & DBGKEY_RIGHT) {
        if (s_dbgStage < DebugStageMax()) {
            s_dbgStage++;
            if (s_dbgRoom >= s_dbgStageRooms[s_dbgStage].count) {
                s_dbgRoom = s_dbgStageRooms[s_dbgStage].count - 1;
            }
            DebugRoomSkipInvalid(1);
        }
    }
    if (newKeys & DBGKEY_UP) {
        if (s_dbgRoom < count - 1) {
            s_dbgRoom++;
        }
        DebugRoomSkipInvalid(1);
    }
    if (newKeys & DBGKEY_DOWN) {
        if (s_dbgRoom > 0) {
            s_dbgRoom--;
        }
        DebugRoomSkipInvalid(-1);
    }

    if (newKeys & DBGKEY_CONFIRM) {
        DebugRoomChange_Trigger();
        g_debugMenuOpen = 0;
        return 0;               // the transition runs through game_loop's menu path
    }

    if ((newKeys & DBGKEY_ESC) || f1Edge) {
        s_dbgContext = 0;
        return 1;
    }

    DebugRoomChange_Draw();
    return 1;
}
