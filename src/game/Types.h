#pragma once

// Win32 types come from the platform layer, never <windows.h> directly:
// src/game/ must stay OS-agnostic (docs/LINUX_PORT.md Phase 0).
#include "../platform/types.h"
#include <cstdio>
#include "../marni/MarniDX.h"
#include "../marni/MarniBits.h"
#include "../marni/Marni3DObject.h"
#include "../marni/MarniInput.h"

struct TextureDesc;
struct Entity;

// PS1 GTE matrix type (0x00ac93b0 layout, 32 bytes)
struct MATRIX {
    short m[3][3];  // 0x00: Rotation matrix (9 x short = 18 bytes)
    short _pad;     // 0x12: Padding for int alignment
    int t[3];       // 0x14: Translation vector (3 x int = 12 bytes)
};

// PSQY short vector type
struct SVECTOR {
    short x, y, z;
    short pad;
};

// PSYQ - Character vector
struct CVECTOR {
    short r, g, b;  // Color palette
    short cd;       // GPU code
};

// 32-bit integer vector
struct VECTOR {
    int x, y, z;
    int pad;
};

struct POLY_F4 {
    unsigned int tag; // Next primitive pointer + size (OT tag)
    unsigned char r0, g0, b0; // RGB color values
    unsigned char code; // Primitive ID (reserved)
    short x0, y0; // Vertex coordinates
    short x1, y1; // Vertex coordinates
    short x2, y2; // Vertex coordinates
    short x3, y3; // Vertex coordinates
}; 

// PSX TIM Image format structs

struct TIMHeader {
    unsigned int magic; // always 0x10
    unsigned int flags;
};

struct TIMBlockHeader {
    unsigned int size;
    short x, y;
    short width, height;
};

// Forward declarations for circular references
struct JointStruct;
struct ScaMatrixData;

// ============================================================================
// JointStruct (0x7C bytes) - Per-joint animation/transform data
// ============================================================================
#pragma pack(push, 1)
struct JointStruct {
    unsigned char  flags;              // 0x00 - bit 0x10 = skip rotation; 3=active
    unsigned char  index;              // 0x01 - joint index number
    unsigned char  field_02;           // 0x02
    unsigned char  pad_03;             // 0x03
    SVECTOR        rotation;           // 0x04 - rotation angles
    int            anim_field;         // 0x0C - anim sub-struct base
    void*          data_ptr;           // 0x10 - points to &scale_flag
    int            anim_slot_ptr;      // 0x14
    void*          anim_object;        // 0x18
    int            field_1c;           // 0x1C
    int            scale_flag;         // 0x20 - set to 1
    MATRIX         transform;          // 0x24 - current joint transform
    MATRIX         world;              // 0x44 - composite world-space matrix
    int            unk_64;             // 0x64
    int            unk_68;             // 0x68
    int            unk_6c;             // 0x6C
    short          velX;               // 0x70 - X velocity / flags (upper byte = bounce flag 0x80)
    short          velY;               // 0x72 - Y velocity
    short          velZ;               // 0x74 - Z velocity / rotation damping
    short          rotDeltaX;           // 0x76 - rotation delta X
    short          rotDeltaY;           // 0x78 - rotation delta Y
    short          rotDeltaZ;           // 0x7A - rotation delta Z
};
#pragma pack(pop)
static_assert(sizeof(JointStruct) == 0x7C, "JointStruct size mismatch");

// ============================================================================
// ScaMatrixData (0x50 bytes)
// ============================================================================
struct ScaMatrixData {
    unsigned int   field_00;          // 0x00 - zeroed
    MATRIX         localMatrix;       // 0x04 - identity x 0x1000, zero translation
    MATRIX         worldMatrix;       // 0x24 - identity x 0x1000, zero translation
    unsigned int   field_44;          // 0x44 - untouched by InitScaMatrix
    unsigned int   owner;             // 0x48 - back-pointer to owning entity (NULL for player)
    unsigned int   field_4c;          // 0x4C - zeroed
};
static_assert(sizeof(ScaMatrixData) == 0x50, "ScaMatrixData size mismatch");

// ============================================================================
// Character IDs (0x00be9823 / 0x00be984b area)
// ============================================================================
#define CHAR_CHRIS      0
#define CHAR_JILL       1
#define CHAR_REBECCA    3

// ============================================================================
// Item IDs - Inventory items (0x00be9944 area, ItemSlot.Id field)
// ============================================================================
#define ITEM_NONE               0x00

// Weapons (0x01-0x0A)
#define ITEM_KNIFE              0x01
#define ITEM_BERETTA            0x02
#define ITEM_SHOTGUN            0x03
#define ITEM_COLT_PYTHON_DUM    0x04    // item 4: Colt Python, DumDum rounds - unobtainable in the
                                        // OG builds. The Director's Cut gives item 4 to its
                                        // Beretta M92FS custom (verified in-game), the ADVANCED
                                        // starting handgun; its weapon slot is slot 3.
#define ITEM_COLT_PYTHON_MAG    0x05    // item 5: the live Colt Python, magnum rounds
#define ITEM_FLAMETHROWER       0x06
#define ITEM_BAZOOKA_EXPLOSIVE  0x07
#define ITEM_BAZOOKA_ACID       0x08
#define ITEM_BAZOOKA_FLAME      0x09
#define ITEM_ROCKET_LAUNCHER    0x0A

// Ammunition (0x0B-0x12)
#define ITEM_CLIP               0x0B
#define ITEM_SHELLS             0x0C
#define ITEM_DUM_DUM_ROUNDS     0x0D
#define ITEM_MAGNUM_ROUNDS      0x0E
#define ITEM_FUEL               0x0F
#define ITEM_EXPLOSIVE_ROUNDS   0x10
#define ITEM_ACID_ROUNDS        0x11
#define ITEM_FLAME_ROUNDS       0x12

// Chemicals & Fluids (0x13-0x1B)
#define ITEM_EMPTY_BOTTLE       0x13
#define ITEM_BOTTLE_WATER       0x14
#define ITEM_UMB_NO2            0x15
#define ITEM_UMB_NO4            0x16
#define ITEM_UMB_NO7            0x17
#define ITEM_UMB_NO13           0x18
#define ITEM_YELLOW6            0x19
#define ITEM_NP003              0x1A
#define ITEM_VJOLT              0x1B

// Quest Items (0x1C-0x2E)
#define ITEM_BROKEN_SHOTGUN     0x1C
#define ITEM_CRANK_SQUARE       0x1D
#define ITEM_CRANK_HEX          0x1E
#define ITEM_EMBLEM             0x1F
#define ITEM_GOLD_EMBLEM        0x20
#define ITEM_BLUE_JEWEL         0x21
#define ITEM_RED_JEWEL          0x22
#define ITEM_MUSIC_NOTES        0x23
#define ITEM_WOLF_MEDAL         0x24
#define ITEM_EAGLE_MEDAL        0x25
#define ITEM_CHEMICAL           0x26
#define ITEM_BATTERY            0x27
#define ITEM_MO_DISK            0x28
#define ITEM_WIND_CREST         0x29
#define ITEM_FLARE              0x2A
#define ITEM_SLIDES             0x2B
#define ITEM_MOON_CREST         0x2C
#define ITEM_STAR_CREST         0x2D
#define ITEM_SUN_CREST          0x2E

// Utility Items (0x2F-0x32)
#define ITEM_INK_RIBBONS        0x2F
#define ITEM_LIGHTER            0x30
#define ITEM_LOCK_PICK          0x31    // DC: MOON CREST left part (mix 0x31 + 0x32 -> 0x2C)
#define ITEM_OIL                0x32    // DC: MOON CREST right part

// Keys (0x33-0x3D)
#define ITEM_SWORD_KEY          0x33
#define ITEM_ARMOR_KEY          0x34
#define ITEM_SHIELD_KEY         0x35
#define ITEM_HELMET_KEY         0x36
#define ITEM_LAB_KEY_A          0x37
#define ITEM_SPECIAL_KEY        0x38
#define ITEM_DORMITORY_KEY_A    0x39
#define ITEM_DORMITORY_KEY_B    0x3A
#define ITEM_CROOM_KEY          0x3B
#define ITEM_LAB_KEY_B          0x3C
#define ITEM_DESK_KEY           0x3D

// Books (0x3E-0x40)
#define ITEM_RED_BOOK           0x3E
#define ITEM_DOOM_BOOK2         0x3F
#define ITEM_DOOM_BOOK1         0x40

// Healing (0x41-0x4B)
#define ITEM_FIRST_AID_SPRAY    0x41
#define ITEM_SERUM              0x42
#define ITEM_RED_HERB           0x43
#define ITEM_GREEN_HERB         0x44
#define ITEM_BLUE_HERB          0x45
#define ITEM_MIX_GREEN_RED      0x46
#define ITEM_MIX_2GREEN         0x47
#define ITEM_MIX_GREEN_BLUE     0x48
#define ITEM_MIX_GREEN_RED_BLUE 0x49
#define ITEM_MIX_3GREEN         0x4A
#define ITEM_MIX_2GREEN_BLUE    0x4B

// Misc (0x4C-0x4D)
#define ITEM_PICK_AXE           0x4C    // unused in both builds (name and "nothing important"
                                        // description are the same); the DC swaps its item *model*
                                        // to the Com. Radio, the OG shows a Pick Axe
#define ITEM_COMM_RADIO         0x4D

// ============================================================================
// Maps (0x4E-0x53)
//
// The six map pick-ups. cmd_item_model_set (0x00461220) special-cases this id
// range: an item model whose id is in [ITEM_MAP_FIRST, ITEM_MAP_LAST] gets room
// check action 0x0F (pickup_key_event) instead of the ordinary 4, and the pickup
// then calls set_room_item_seen_flag (0x004885a0) with the item's MAP INDEX -
// itemId - ITEM_MAP_FIRST - which raises g_RoomFlags bit
// ROOM_FLAG_MAP_BASE + index. map_area_known (0x004885c0) reads those bits back
// to decide which map areas the map screen may show.
//
// Only four of the six are actually placed as item models; the other two raise
// their bit directly (see the per-id comments). Full trace in
// docs/SCENARIO_FLAGS.md.
// ============================================================================
#define ITEM_MAP_MANSION_1F     0x4E    // room 107, item model slot 2
#define ITEM_MAP_MANSION_2F     0x4F    // room 20B - no item model; the event script sets the bit
#define ITEM_MAP_COURTYARD      0x50    // room 300, item model slot 4
#define ITEM_MAP_UNDERGROUND    0x51    // room 30F, item model slot 4
#define ITEM_MAP_GUARDHOUSE     0x52    // room 406, item model slot 8
#define ITEM_MAP_LABORATORY     0x53    // room 506 - no item model; cl_cmd_unlock sets the bit

// Highest non infinite item id
#define ITEM_NON_INFINITE_MAX   0x6E

// PC exclusive weapons (rewards for finishing the game under 4 hours)
#define ITEM_INGRAM             0x6F    // Jill's exclusive sub machinegun
#define ITEM_MINIMI             0x70    // Chris' exclusive machinegun


// ============================================================================
// Maps
// ============================================================================

// Map indexes: the argument set_room_item_seen_flag takes, i.e. itemId minus
// ITEM_MAP_FIRST. Also the index map_area_known derives from a map area.
#define MAP_INDEX_MANSION_1F    0       // map area 0
#define MAP_INDEX_MANSION_2F    1       // map area 1
#define MAP_INDEX_COURTYARD     2       // map area 3
#define MAP_INDEX_UNDERGROUND   3       // map area 4
#define MAP_INDEX_GUARDHOUSE    4       // map areas 5 and 6
#define MAP_INDEX_LABORATORY    5       // map areas 8 and 9
#define MAP_INDEX_COUNT         6

#define ITEM_MAP_FIRST          ITEM_MAP_MANSION_1F
#define ITEM_MAP_LAST           ITEM_MAP_LABORATORY

// Item id <-> map index
#define ITEM_IS_MAP(itemId)       ((itemId) >= ITEM_MAP_FIRST && (itemId) <= ITEM_MAP_LAST)
#define ITEM_TO_MAP_INDEX(itemId) ((itemId) - ITEM_MAP_FIRST)
#define MAP_INDEX_TO_ITEM(index)  ((index) + ITEM_MAP_FIRST)


// ============================================================================
// Stages - zero-index stages constants. 
// 
// Stages directories and some stage id tests use 1-index format.
// ============================================================================

#define STAGE_MANSION_1F            0x00
#define STAGE_MANSION_2F            0x01 // limited west wing access
#define STAGE_COURTYARD             0x02 // courtyard and underground
#define STAGE_GUARDHOUSE            0x03
#define STAGE_LABORATORY            0x04
#define STAGE_MANSION_RETURN_1F     0x05 // same as stage 1, but with access to elevator and study
#define STAGE_MANSION_RETURN_2F     0x06 // west wing access, plus mansion B1

// mansion stages share the same rooms between stages variants (start game mansion and 
// return from courtyard mansion). the return mansion RDTs that were accesible in earlier stages
// has the same structure, but its items, events and enemies changes (zombies and cerberus for early game 
// stages, hunters and spiders for return stages), but the return stages directories do not have 
// background images files to avoid having duplicates (to save disk storage probably). For this, the engine
// does a modulus calculus to get the absolute mansion stage index ((g_stageId + 1) % 5). This is because
// Stages directories and RTD files uses 1-indexed ids.
#define MANSION_1F      1
#define MANSION_2F      2
#define COURTYARD       3
#define GUARDHOUSE      4
#define LABORATORY      5

// ============================================================================
// Rooms
// ============================================================================

// Stage 1/6 (Mansion 1F)
#define ROOM_MANSION_SAVE_ROOM          0x00
#define ROOM_1F_LEFT_STAIRS             0x01 // west wing stairs
#define ROOM_VACANT_ROOM                0x02 // broken shotgun room
#define ROOM_F_PASSAGE                  0x03 // west wing central corridor
#define ROOM_TEA_ROOM                   0x04 // first zombie room
#define ROOM_DINING_ROOM                0x05
#define ROOM_MAIN_HALL                  0x06
#define ROOM_GALLERY                    0x07
#define ROOM_L_PASSAGE                  0x08 // first cerberus room
#define ROOM_TRAP_PASSAGE               0x09 // east wing winding corridor 
#define ROOM_BACK_PASSAGE               0x0a // corridor near back of the mansion
#define ROOM_1F_RIGHT_STAIRS            0x0b // east wing stairs
#define ROOM_GREENHOUSE                 0x0c
#define ROOM_TIGER_STATUE_ROOM          0x0d
#define ROOM_KEEPERS_BEDROOM            0x0e              
#define ROOM_MANSION_BAR                0x0f
#define ROOM_MANSION_1F_ELEVATOR_FRONT  0x10 // elevator stairway (stage 6 only)
#define ROOM_DRESSING_ROOM              0x11
#define ROOM_WARDROBE                   0x12 // big mirror room
#define ROOM_MANSION_BATHROOM           0x13 
#define ROOM_BOILER                     0x14
#define ROOM_TRAP_ROOM                  0x15
#define ROOM_LIVING_ROOM                0x16 // where you get the shotgun
#define ROOM_LARGE_GALLERY              0x17
#define ROOM_MANSION_STOREROOM          0x18 // storeroom under east wing stairs
#define ROOM_MANSION_1F_STUDY           0x19 // stage 6 only
#define ROOM_ROOFED_PASSAGE             0x1a // passage to courtyard
#define ROOM_STOREROOM                  0x1b // storeroom next to courtyard
#define ROOM_WARDROBE_CLOSET            0x1c // alternate outfit change room

// Stage 2/7 (Mansion 2F and B1)
#define ROOM_MANSION_1F_TO_B1_ELEVATOR  0x00 // stage 7 only
#define ROOM_2F_LEFT_STAIRS             0x01 // west wing stairs
#define ROOM_DINING_ROOM_2F             0x02
#define ROOM_MAIN_HALL_2F               0x03
#define ROOM_C_PASSAGE                  0x04 // 2F east wing main corridor
#define ROOM_ARMOR_ROOM                 0x05
#define ROOM_SMALL_LIBRARY              0x06
#define ROOM_2F_RIGHT_STAIRS            0X07 // 2F east wing stairs
#define ROOM_DEER_ROOM                  0x08 
#define ROOM_MANSION_2F_BEDROOM         0x09
#define ROOM_STUDY_2F                   0x0a                 
#define ROOM_FRONT_LESSON_ROOM          0x0b
#define ROOM_LESSON_ROOM                0x0c // Yawn2 room (stage 7 only)
#define ROOM_PILLAR_PASSAGE             0x0d // Richard's room
#define ROOM_FRONT_OF_ATTIC             0x0e
#define ROOM_SMALL_DINING               0x0f // small dining room near attic
#define ROOM_ATTIC                      0x10 // Yawn1 room
#define ROOM_TERRACE_PASSAGE            0x11
#define ROOM_TERRACE                    0x12
#define ROOM_MANSION_2F_FRONT_ELEVATOR  0x13 // elevator passage (stage 7 only)
#define ROOM_mansion_2F_ROUGH_PASSAGE   0x14 // west wing central corridor (stage 7 only)
#define ROOM_TROPHY_ROOM                0x15 // stage 7 only
#define ROOM_LARGE_LIBRARY              0x16 // stage 7 only
#define ROOM_PRIVATE_LIBRARY            0x17 // stage 7 only
#define ROOM_HELIPORT_LOOKOUT           0x18 // stage 7 only
#define ROOM_MANSION_SHED               0x19 // where you get courtyard elevator battery (stage 7 only)
#define ROOM_MANSION_B1_PASSAGE_1       0x1a // stage 7 only
#define ROOM_MANSION_B1_PASSAGE_2       0x1b // stage 7 only
#define ROOM_MANSION_KITCHEN            0x1c // mansion b1, stage 7 only

// Stage 3 (Courtyard and Underground)
#define ROOM_COURTYARD_GARDEN           0x00
#define ROOM_WATER_GATE                 0x01
#define ROOM_FALLS                      0x02
#define ROOM_HELIPORT                   0x03
#define ROOM_GUARDHOUSE_GATE            0x04
#define ROOM_FOUNTAIN                   0x05
#define ROOM_ITEM_CHAMBER               0x06 // where you get the doom book 2 (underground)
#define ROOM_UNDERGROUND_ENTRY          0x07
#define ROOM_BRANCHED_PASSAGE           0x08 // underground branched passage
#define ROOM_UNDERGROUND_GENERATOR      0x09
#define ROOM_ENRICO_ROOM                0x0a // underground
#define ROOM_BOULDER_1_PASSAGE          0x0b
#define ROOM_BLACK_TIGER_ROOM           0x0c // Black Tiger (giant spider) boss room
#define ROOM_STRAIGHT_PASSAGE           0x0d // near underground save room
#define ROOM_UNDERGROUND_SAVE_ROOM      0x0e
#define ROOM_BOULDER_2_PASSAGE          0x0f
#define ROOM_ELEVATOR_TO_LABORATORY     0x10 // fountain's elevator to laboratory
#define ROOM_SCRAPPED_HELIPORT          0x11 // dev leftover, never used: stub heliport RDT (ROOM3110.RDT, also present in the PS1
                                             // version). The shipped heliport is ROOM_HELIPORT. Special-cased in Room.cpp because its
                                             // stub RDT has no masks/models and its bg must bypass the cache (see room3110 memory note)

// Stage 4 (Guardhouse)
#define ROOM_GUARDHOUSE_ENTRY           0x00
#define ROOM_001                        0x01
#define ROOM_001_BATHROOM               0x02
#define ROOM_GUARDHOUSE_SAVE_ROOM       0x03
#define ROOM_GUARDHOUSE_BAR             0x04
#define ROOM_GUARDHOUSE_CENTER_PASSAGE  0x05
#define ROOM_002                        0x06
#define ROOM_002_BATHROOM               0x07
#define ROOM_BEEHIVE_PASSAGE            0x08
#define ROOM_DRUG_STOREHOUSE            0x09
#define ROOM_003                        0x0a
#define ROOM_003_BATHROOM               0x0b
#define ROOM_PLANT_42_ROOM              0x0c // Plant42 boss room
#define ROOM_WATER_TANK_ENTRY           0x0d
#define ROOM_WATER_TANK                 0x0e
#define ROOM_SECURITY_ROOM              0x0f
#define ROOM_ARMS_STOREHOUSE            0x10
#define ROOM_CONTROL_ROOM               0x11

// Stage 5 (Laboratory)
#define ROOM_LABORATORY_ENTRY           0x00
#define ROOM_EMERGENCY_TUNNEL           0x01 // heliport elevator passage
#define ROOM_LAB_LADDER_ROOM            0x02
#define ROOM_LAB_B2_STAIRWAY            0x03
#define ROOM_VISUAL_DATA_ROOM           0x04
#define ROOM_LAB_B3_O_PASSAGE           0x05 // Lab b3 central passage
#define ROOM_SMALL_LABORATORY           0x06
#define ROOM_MORGUE                     0x07
#define ROOM_LAB_B3_PRIVATE_PASSAGE     0x08 // where the cell entry locks are
#define ROOM_LAB_B3_PRIVATE_ROOM_A      0x09 // where a MO disk reader terminal is
#define ROOM_LAB_B3_PRIVATE_ROOM_B      0x0a // the blue light switch room
#define ROOM_CELL_ENTRY                 0x0b
#define ROOM_LAB_B3_ELEVATOR_ENTRY      0x0c
#define ROOM_ESCAPE_ELEVETOR            0x0d
#define ROOM_LAB_SAVE_ROOM              0x0e
#define ROOM_POWER_MAZE_1               0x0f
#define ROOM_POWER_MAZE_2               0x10
#define ROOM_POWER_ROOM                 0x11
#define ROOM_CELL                       0x12
#define ROOM_MAIN_LAB                   0x13 // Tyrant 1 room (lab b4)
#define ROOM_MAIM_LAB_ENTRY             0x14 // lab b4
#define ROOM_LAB_B3_TO_B4_ELEVATOR      0x15

// ============================================================================
// RDT Light structure (0x14 / 20 bytes each)
// ============================================================================
struct RDT_Light {
    int            pos_x;       // 0x00
    int            pos_y;       // 0x04
    int            pos_z;       // 0x08
    unsigned char  red;         // 0x0C
    unsigned char  green;       // 0x0D
    unsigned char  blue;        // 0x0E
    unsigned char  zero1;       // 0x0F
    // 0x10 is ONE 16-bit field, not two bytes. update_entity_lighting selects
    // point vs directional with a word test - 0x00481673:
    // CMP word ptr [ECX+0x1c],0x0 where ECX = g_RdtPointer + i*0x14, and
    // RDT+0x1c is lights[i]+0x10. cmd_light_set writes it as a word too.
    // Modelling it as a single byte made a light whose high byte is non-zero
    // read as a point light and get radial attenuation applied, when the
    // original treats it as directional and passes the colour through unchanged.
    unsigned short lightType;   // 0x10 - 0 = point light with radial falloff
    short          radius;      // 0x12
};
static_assert(sizeof(RDT_Light) == 0x14, "RDT_Light size mismatch");

// ============================================================================
// RDT Camera structure (0x2C / 44 bytes each)
// ============================================================================
struct RDT_Camera {
    int   mask_pointer;         // 0x00
    int   tim_mask_pointer;     // 0x04
    int   cam_from_x;           // 0x08
    int   cam_from_y;           // 0x0C
    int   cam_from_z;           // 0x10
    int   cam_to_x;             // 0x14
    int   cam_to_y;             // 0x18
    int   cam_to_z;             // 0x1C
    int   roll;                 // 0x20
    int   zero;                 // 0x24
    int   fov;                  // 0x28
};
static_assert(sizeof(RDT_Camera) == 0x2C, "RDT_Camera size mismatch");

// ============================================================================
// CAM_SWITCH_ZONE (0x14 / 20 bytes each)
// Camera switch zones, pointed to by RDT::cam_switch_zones (0x48).
//
// The table is grouped by source camera: every entry that shares a camFrom
// belongs to one camera, and the first entry of each group is that camera's own
// header record (g_CurrentRdtDataTypePtr points at it). The entries that follow
// it are the switch zones tested against the player position; entering one sets
// g_roomCameraId to its camTo.
//
// (x0,y0)..(x3,y3) form a quadrilateral in the room's XZ plane. The original
// reads every coordinate ZERO-extended from 16 bits (see
// is_entity_in_switch_zone), so they behave as unsigned, not signed.
// ============================================================================
struct CAM_SWITCH_ZONE {
    short camTo;                // 0x00 - camera to switch to on entry
    short camFrom;              // 0x02 - camera this group belongs to
    short x0, y0;               // 0x04 - quad corner 0 (XZ plane)
    short x1, y1;               // 0x08 - quad corner 1
    short x2, y2;               // 0x0C - quad corner 2 (second pivot)
    short x3, y3;               // 0x10 - quad corner 3
};
static_assert(sizeof(CAM_SWITCH_ZONE) == 0x14, "CAM_SWITCH_ZONE size mismatch");

// ============================================================================
// RDT_Boundary (0x0C / 12 bytes each)
// One room collision record, in the block pointed to by RDT::boundaries (0x4C).
//
// The MAX corner comes first: the record covers [xMin..xMax] x [zMin..zMax] in
// the room's XZ plane. See the header comment in RoomCollision.cpp for how that
// ordering was established - the RE2-era notes on this format describe the
// second pair as a width/depth, which does not hold here.
//
// type:  low byte  = shape index into g_CollisionShapeHandlers
//                    (1 = rect push, 3 = circle push, 4 = soft zone,
//                     5 = rect push skipped while collisionFlags bit 4 is set)
//        bits 8-14 = floor/step magnitude, bit 15 = the step is downward
// flags: low byte  = floor/step fine value
//        bit 8     = record blocks movement (cleared = floor zone only)
//        bit 9     = record participates in the "still stuck" re-test
// ============================================================================
// The four coordinates are UNSIGNED. boundary_classify zero-extends each one
// (XOR EAX,EAX / MOV AX,word at 0x0047d1c1) before the signed 32-bit grow, and
// 222 of the 5380 shipped records carry a coordinate above 32767 - e.g. stage 1
// room 05 has one spanning x 675..35677, which read as a signed short becomes
// x 675..-29859 and inverts the box.
struct RDT_Boundary {
    unsigned short xMax;        // 0x00
    unsigned short zMax;        // 0x02
    unsigned short xMin;        // 0x04
    unsigned short zMin;        // 0x06
    unsigned short type;        // 0x08
    unsigned short flags;       // 0x0A
};
static_assert(sizeof(RDT_Boundary) == 0x0C, "RDT_Boundary size mismatch");

// ============================================================================
// RDT_BoundaryHeader (0x18 / 24 bytes)
// Header of the room collision block. In the file, group[] holds the five
// per-quadrant record counts; Room_SetupCollisionCallbacks rewrites them in
// place into absolute pointers so quadrant q spans [group[q], group[q+1]).
// ============================================================================
struct RDT_BoundaryHeader {
    short          cellX;       // 0x00 - X of the quadrant split point
    short          cellZ;       // 0x02 - Z of the quadrant split point
    RDT_Boundary*  group[5];    // 0x04 - counts in the file, pointers after setup
    // 0x18: RDT_Boundary entries[]
};
static_assert(sizeof(RDT_BoundaryHeader) == 0x18, "RDT_BoundaryHeader size mismatch");

// ============================================================================
// RDT (Room Definition Table) - 0x94 byte header + variable data
// ============================================================================
#pragma pack(push, 1)
struct RDT {
    unsigned char  sprites_count;       // 0x00 - number of room sprite entries in g_RoomSprEntries
    unsigned char  cameras_count;       // 0x01
    unsigned char  omodel_slot_count;   // 0x02 - omodel {TMD,TIM} pair count at
                                        //         object_models AND the omodel record
                                        //         slot count (never used for sounds
                                        //         on PC)
    unsigned char  item_count;          // 0x03 - item {TMD,TIM} pair count at
                                        //         item_models, the 0xA4-byte record
                                        //         slot count carved into
                                        //         g_item_model_table, and the number
                                        //         of icon tiles at item_icons
    unsigned char  pad_04[2];           // 0x04-0x05 - no reader in the binary; zero in
                                        //             all 320 shipped RDTs
    short          ambient_light_r;     // 0x06
    short          ambient_light_g;     // 0x08
    short          ambient_light_b;     // 0x0A
    RDT_Light      lights[3];           // 0x0C-0x47
    unsigned char* cam_switch_zones;    // 0x48
    unsigned char* boundaries;          // 0x4C
    unsigned char* object_models;       // 0x50 - omodel_slot_count x {TMD*, TIM*}
    unsigned char* item_models;         // 0x54 - item_count x {TMD*, TIM*}
    unsigned char* walk_zones;          // 0x58 - NPC navigation grid: count byte,
                                        //         then 0xC-byte zones from +2
    unsigned char* footstep_sound_zones;// 0x5C
    unsigned char* initialization_scd;  // 0x60
    unsigned char* scd_opcodes;         // 0x64
    unsigned char* scd_opcodes2;        // 0x68
    unsigned char* player_anim_header;  // 0x6C - room's player animation pair; both
    unsigned char* player_anim_base;    // 0x70   go to g_playerEntity.jointMoveData2/3
                                        //        and feed Joint_move (door / push /
                                        //        crank poses)
    unsigned char* messages;            // 0x74
    unsigned char* item_icons;          // 0x78 - item_count x 1200-byte 40x30 8bpp
                                        //         inventory icons, the same tiles as
                                        //         Data/ITEM_ALL.tim. PS1 leftover:
                                        //         no reader anywhere in the binary
    unsigned char* effect_anim_index;   // 0x7C
    unsigned char* effect_anim_data;    // 0x80
    unsigned char* effect_anim_sprite;  // 0x84
    unsigned char* sound_attribute_table;// 0x88
    unsigned char* vab_header_file;     // 0x8C
    unsigned char* vab_sound_file;      // 0x90
};
#pragma pack(pop)
static_assert(sizeof(RDT) == 0x94, "RDT header size mismatch");

// ============================================================================
// ScdEventEntry (0x34 bytes) - SCD event execution context
// One entry per active room event script. 8-slot table at 0x00bf084c.
// Initialized by FUN_0041d620, executed by room_events_check.
// ============================================================================
#pragma pack(push, 1)
struct ScdEventEntry {
    unsigned char   state;              // 0x00: execution state (0=idle, 1=wait anim, 2=run script, 3=wait condition)
    unsigned char   pad_01;             // 0x01: unused (zeroed by init)
    unsigned char   active;             // 0x02: active flag (0=inactive, non-zero=active)
    unsigned char   stackDepth;         // 0x03: operand stack index (0xFF = empty)
    Entity*         entity;             // 0x04: associated entity pointer
    unsigned char*  scriptPtr;          // 0x08: current SCD instruction pointer
    unsigned int    returnStack[4];     // 0x0C: return address stack (for SCD loops)
    unsigned int    callStack[4];       // 0x1C: saved script pointer stack (for SCD subroutines)
    short           counterStack[4];    // 0x2C: loop counter stack
};
#pragma pack(pop)
static_assert(sizeof(ScdEventEntry) == 0x34, "ScdEventEntry size mismatch");

// ============================================================================
// Effect (0x84 bytes) - Billboard/sprite effect slot in the 64-slot pool
// Pool base address: 0x00be41e4, allocated by Effect_CreateBillboard
// ============================================================================
#pragma pack(push, 1)
struct Effect {
    // Animation header (0x00-0x17, 24 bytes, bulk-copied from frame data)
    unsigned char  animId;               // 0x00 - animation ID (0 = free slot)
    unsigned char  updateId;             // 0x01 - behavior update index
    unsigned char  type;                 // 0x02 - 1 = active billboard
    unsigned char  lightFactor;          // 0x03 - lighting intensity
    unsigned char  animHeader[0x12];     // 0x04-0x15 - rest of animation header
    short          yaw;                  // 0x16 - yaw rotation angle (base + param)

    // Movement / physics (0x18-0x1F)
    short          rotSpeedX;            // 0x18 - rotation speed X
    short          rotSpeedY;            // 0x1A - rotation speed Y
    short          rotSpeedZ;            // 0x1C - rotation speed Z
    unsigned char  frameDelay;           // 0x1E - frame delay counter
    unsigned char  frameIndex;           // 0x1F - current animation frame

    // World position (0x20-0x25)
    short          posX;                 // 0x20 - world position X
    short          posY;                 // 0x22 - world position Y
    short          posZ;                 // 0x24 - world position Z

    // Spawn parameters (0x26-0x2F)
    unsigned char  effectType;           // 0x26 - effect type ID (param_1)
    unsigned char  depthGroup;           // 0x27 - depth group + flags (param_2)
    short          localOffsetX;         // 0x28 - local offset X (truncated spawn pos)
    short          localOffsetY;         // 0x2A - local offset Y
    short          localOffsetZ;         // 0x2C - local offset Z
    short          depthScaled;          // 0x2E - depth-scaled value

    // 3x3 rotation matrix (0x30-0x41, 9 shorts = 18 bytes)
    short          transform[9];         // 0x30 - rotation matrix rows

    // Padding for alignment (0x42-0x43)
    short          pad_42;               // 0x42

    // Sprite world offset (0x44-0x50)
    int            spriteOffsetX;        // 0x44 - sprite world offset X
    int            spriteOffsetY;        // 0x48 - sprite world offset Y
    int            spriteOffsetZ;        // 0x4C - sprite world offset Z
    int            projDepth;            // 0x50 - projected depth value

    // Spawn position copy (0x54-0x63, full precision from VECTOR)
    int            spawnPosX;            // 0x54 - spawn position X
    int            spawnPosY;            // 0x58 - spawn position Y
    int            spawnPosZ;            // 0x5C - spawn position Z
    int            spawnPosW;            // 0x60 - spawn position W (pad)

    // Texture / rendering pointers (0x64-0x83)
    int            spriteInfo;           // 0x64 - MATRIX pointer or identity
    int            clutInfo;             // 0x68 - CLUT texture header pointer
    int            vramInfo;             // 0x6C - VRAM sprite info pointer
    int            vramInfoBackup;       // 0x70 - VRAM info backup
    int            uvData;               // 0x74 - UV data pointer
    int            uvDataBackup;         // 0x78 - UV data backup
    int            animDataBase;         // 0x7C - animation frame data base
    int            animDataFrame;        // 0x80 - current animation frame pointer
};
#pragma pack(pop)
static_assert(sizeof(Effect) == 0x84, "Effect size mismatch");

#define MAX_EFFECTS 64

// ============================================================================
// Item Slot structure (each inventory entry: Id + quantity)
// ============================================================================
#pragma pack(push, 1)
struct ItemSlot {
    unsigned char Id;       // +0x00: Item ID (0 = empty)
    unsigned char qty;      // +0x01: Quantity / ammo count
};
#pragma pack(pop)
static_assert(sizeof(ItemSlot) == 2, "ItemSlot size mismatch");

// ============================================================================
// Display and rendering structures
// ============================================================================
struct DisplayModeInfo {
    DWORD dwWidth;
    DWORD dwHeight;
    DWORD dwBPP;
    DWORD dwRefreshRate;
    DWORD dwFlags;
};

struct RectDrawDesc {
    unsigned int textureId;
    short x;
    short y;
    short w;
    short h;
    unsigned char r;
    unsigned char g;
    unsigned char b;
};
static_assert(sizeof(RectDrawDesc) == 16, "RectDrawDesc size mismatch");

#pragma pack(push, 1)
struct TextureDesc {
    unsigned int flags;         // 0x00
    short screenX;              // 0x04
    short screenY;              // 0x06
    unsigned short width;       // 0x08
    unsigned short height;      // 0x0a
    // PS1 tpage code. AddTintSprite (USA 0x0046e0a0, JPN 0x00441120) reads
    // this as a word and linear-searches the texture page descriptors for
    // the slot whose page id matches; draw_texture / AddSprite decode it as
    // a VRAM page ((v & 15) * 0x40 words across, (v / 16) * 0x100 lines
    // down). It is the only field wide enough to reach a second page -
    // texU/texV below are bytes.
    short texturePage;          // 0x0c
    unsigned char texU;         // 0x0e
    unsigned char texV;         // 0x0f
    // CLUT position in PS1 VRAM. The PC build unpacks every CLUT to RGBA at
    // load time and keeps them all at one VRAM X, so clutX is write-only
    // here - no submitter in the original reads +0x10. clutY still carries
    // information: it lands on the palette strip at lines 0x1E0+, and
    // AddTintSprite turns (clutY - page CLUT base) into the 0-7 row index
    // that selects a glyph colour.
    short clutX;                // 0x10
    short clutY;                // 0x12
    unsigned char colorMulR;    // 0x14
    unsigned char colorMulG;    // 0x15
    unsigned char colorMulB;    // 0x16
    unsigned char pad17;        // 0x17
    short pivotX;               // 0x18
    short pivotY;               // 0x1a
    short scaleX;               // 0x1c (fix16.12, 0x1000 = 1.0)
    short scaleY;               // 0x1e (fix16.12, 0x1000 = 1.0)
};
#pragma pack(pop)
static_assert(sizeof(TextureDesc) == 0x20, "TextureDesc size mismatch");

// ============================================================================
// RoomSprEntry (0x24 bytes) - Room sprite/object entry
// Each room has an array of these, populated from RDT data by FUN_004757c0.
// The 'active' flag controls visibility; 'id' is the sprite type identifier
// used by SCD commands (cmd_room_sprite_set) to show/hide objects.
// Base address: 0x00d213d0 (g_RoomSprEntries)
// ============================================================================
#pragma pack(push, 1)
struct RoomSprEntry {
    TextureDesc      texDesc;   // 0x00-0x1F: texture/rendering descriptor
    unsigned char    active;    // 0x20: visibility flag (1=visible, 0=hidden)
    unsigned char    id;        // 0x21: sprite type identifier
    unsigned short   posData;   // 0x22: position/z-depth data
};
#pragma pack(pop)
static_assert(sizeof(RoomSprEntry) == 0x24, "RoomSprEntry size mismatch");

struct TaskControlBlock {
    short state;           // 0x00 - State/flags
    short sleepCounter;    // 0x02 - Sleep counter
    BYTE  reserved[0x78];
};

struct D3DRendererInfo {
    char name[256];
    DWORD flags;
};

// ============================================================================
// PS1 digital controller button bit constants
// ============================================================================
#define PAD_SELECT      0x0001
#define PAD_L3          0x0002
#define PAD_R3          0x0004
#define PAD_START       0x0008
#define PAD_UP          0x0010
#define PAD_RIGHT       0x0020
#define PAD_DOWN        0x0040
#define PAD_LEFT        0x0080
#define PAD_L2          0x0100
#define PAD_R2          0x0200
#define PAD_L1          0x0400
#define PAD_R1          0x0800
#define PAD_TRIANGLE    0x1000
#define PAD_CIRCLE      0x2000
#define PAD_CROSS       0x4000
#define PAD_SQUARE      0x8000

#define PAD_ANY         0xFFFF
#define PAD_DPAD        (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT)
#define PAD_SHOULDER    (PAD_L1|PAD_L2|PAD_R1|PAD_R2)
#define PAD_FACE        (PAD_TRIANGLE|PAD_CIRCLE|PAD_CROSS|PAD_SQUARE)
#define PAD_MENU_CONFIRM PAD_CROSS
#define PAD_MENU_BACK   PAD_SQUARE
#define PAD_MENU_UP     PAD_UP
#define PAD_MENU_DOWN   PAD_DOWN
#define PAD_CONFIRM     (PAD_CROSS|PAD_START)
#define PAD_TITLE_ANY   (PAD_SELECT|PAD_L3|PAD_R3|PAD_START|PAD_RIGHT|PAD_LEFT|PAD_R2|PAD_L1|PAD_R1|PAD_CROSS)

// ============================================================================
// Registry
// ============================================================================
#define REGKEY_PATH "Software\\CAPCOM\\RESIDENT EVIL"
#define MAX_DISPLAY_MODES 100
#define MAX_DRIVES 26

// ============================================================================
// SpriteAnimSlot (0x14 / 20 bytes each)
// Sprite animation data table entry. 6 entries at 0x00be9a60.
// Indexed by g_spriteAnimActive in render_room_objects and
// render_entity (stride 0x14).
// ============================================================================
#pragma pack(push, 1)
struct SpriteAnimSlot {
    int   count;        // 0x00 - count / OT shift value (4 or 10)
    void* dataPtr;      // 0x04 - pointer to sprite/lighting data buffer
    BYTE  pad_08[0x0C]; // 0x08 - unused/unknown
};
#pragma pack(pop)
static_assert(sizeof(SpriteAnimSlot) == 0x14, "SpriteAnimSlot size mismatch");

// ============================================================================
// SndBankSlot (0x08 / 8 bytes)
// One entry of a sound-bank table. All five sound-bank arrays in the original
// share this record layout; sounds_reset (0x0047ea90) walks each of them with a
// stride of 8 and clears handle, field_04 and slot (plus `paused` for g_SndBank).
//
// Array bases and element counts, taken from the original loop bounds:
//   g_RoomSfxBanks      0x00ac9910   2   (bound 0x00ac9920)
//   g_CharacterSfxBanks 0x00ac9950   9   (bound 0x00ac9998)
//   g_SndBank           0x00ac99d0   3   (bound 0x00ac99e8)  <- BGM channels
//   g_emSndBanks        0x00ac99f0  48   (bound 0x00ac9b70)
//   g_SfxBanks          0x00ac9b80  16   (bound 0x00ac9c00)
// ============================================================================
#pragma pack(push, 1)
struct SndBankSlot {
    int           handle;    // 0x00 - createSndBank handle; 0 = slot empty
    unsigned char field_04;  // 0x04 - cleared by sounds_reset; purpose unknown
    signed char   slot;      // 0x05 - slot index passed to SetSndSlot / playSnd
    unsigned char paused;    // 0x06 - PauseSounds sets, ResumePausedSounds clears
    unsigned char pad_07;    // 0x07
};
#pragma pack(pop)
static_assert(sizeof(SndBankSlot) == 0x08, "SndBankSlot size mismatch");

// ============================================================================
// SndPanVol (0x08 / 8 bytes) - 3 entries at 0x00ac98e0 (bound 0x00ac98f8)
// Per-BGM-channel cached pan/volume, written by SCD opcode 0x2F alongside the
// call to snd_set_channel_pan_volume (0x004805d0). Indexed by the same channel
// index as g_SndBank. Previously modeled as two overlapping arrays
// (DAT_00ac98e0[4] and DAT_00ac98e4[4], whose ranges collided).
// ============================================================================
#pragma pack(push, 1)
struct SndPanVol {
    unsigned int pan;     // 0x00
    unsigned int volume;  // 0x04
};
#pragma pack(pop)
static_assert(sizeof(SndPanVol) == 0x08, "SndPanVol size mismatch");

// ============================================================================
// SavedEnemyState (0x1C / 28 bytes) - 16 entries at 0x00be92cc
// Persists an enemy's position/orientation so that re-entering a room restores it
// instead of respawning it at the script's coordinates. Searched by
// restore_saved_enemy_state (0x0048f330) on (roomId, enemyType) among slots whose
// `valid` byte is non-zero; a match is consumed (valid cleared) and copied into
// the current ENTITY. Called from SCD opcode 0x1B (cmd_enemy_set): a hit makes the
// command skip its own initialisation.
// ============================================================================
#pragma pack(push, 1)
struct SavedEnemyState {
    unsigned char  statusFlags;    // 0x00 -> ENTITY->status_flags
    unsigned char  behaviorFlags;  // 0x01 -> ENTITY->behavior_flags
    unsigned char  roomId;         // 0x02    match key
    unsigned char  enemyType;      // 0x03    match key
    unsigned char  pad_04[4];      // 0x04
    short          posX;           // 0x08 -> localMatrix.t[0]
    short          posY;           // 0x0A -> localMatrix.t[1], only if behaviorFlags & 0x70
    short          posZ;           // 0x0C -> localMatrix.t[2]
    unsigned char  pad_0e[2];      // 0x0E
    unsigned short angle;          // 0x10 -> low half of ENTITY->angle (+0x74)
    unsigned char  pad_12[2];      // 0x12
    unsigned char  valid;          // 0x14    non-zero = slot occupied
    unsigned char  pad_15[7];      // 0x15
};
#pragma pack(pop)
static_assert(sizeof(SavedEnemyState) == 0x1C, "SavedEnemyState size mismatch");
