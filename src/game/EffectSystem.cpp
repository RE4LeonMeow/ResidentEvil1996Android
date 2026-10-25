// EffectSystem.cpp - 2D billboard effect system (muzzle flashes, bullets,
// blood, fire, smoke, dust) - decompiled from the original.
//
// The pool is 64 slots of 0x84 bytes (g_effectPool, 0x00be41e4), allocated by
// Effect_CreateBillboard (PlayerAnimations.cpp). Every frame game_loop calls
// update_2d_effects (0x0047c0c0), which walks the pool and runs
// EffectActor_UpdateAndRender (0x0047c2f0) per active slot: it dispatches the
// slot's behavior function (g_effectBehaviorTable[animId], and a second one by
// updateId), integrates velocity/rotation through the animation header, steps
// the sprite frame, projects the world position, and submits the sprite via
// SubmitEffectSprite (SpriteRenderer.cpp). In a mirror room
// (g_main_state_flags & 1) each effect is additionally redrawn through
// effect_draw_mirror_reflection in the reflected camera.
//
// Each effect's animation data is a chain of 24-byte header blocks. The header
// bytes (animId, updateId, type, lightFactor + 16 more) select the behavior
// and the per-frame velocities; behaviors advance to the next block through
// effect_behavior_next_phase (0x0040cd40), which copies the following 24 bytes
// over the slot's header. The RDT/esp data tables and the 32 behaviors were
// all recovered from assets/ResidentEvil.exe (see tools/mine_effect_tables.py
// for the table dumps).
#include "../Globals.h"
#include "SpriteRenderer.h"
#include "Entities.h"
#include "BioCard.h"
#include "dc/ArrangeStages.h"   // room_effect_page_entry()
#include "../DebugPrint.h"
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Cross-file symbols
// ---------------------------------------------------------------------------
// CompMatrix is declared in Globals.h (GteMatrix.cpp 0x0040a190).
extern int  g_MaxHealthDisplayFlag;       // MainLoop.cpp 0x00d227c0
extern int  g_spriteAnimActive;           // MainLoop.cpp 0x00be41d0
extern void FlipSprite(int* src, MATRIX* dst, unsigned char mirror, unsigned int width); // 0x0048bca0
extern void Matrix_MulMatrix(MATRIX* a, MATRIX* b);             // 0x0040a210
extern unsigned int ChkOutsideCell(VECTOR* position, SVECTOR* offset,
                                   int cellX, int cellZ);    // RoomCollision.cpp
extern unsigned short boundary_classify_flags(SVECTOR* offset, RDT_Boundary* rec,
                                              unsigned int radius); // RoomCollision.cpp
extern unsigned char check_weapon_line_of_sight(VECTOR* targetPos); // WeaponDamage.cpp 0x0048a530
extern unsigned char Effect_CreateBillboard(unsigned char type, unsigned char depthGroup,
                                            short yaw, void* spriteInfo, void* pos,
                                            char lightFactor);       // PlayerAnimations.cpp
extern const unsigned char g_RoomEffectSpriteTable[7 * 32 * 4]; // EffectSprites.cpp 0x004c48b8
extern unsigned int g_entity_bkp;          // EntityCommon.cpp 0x00be0df4
extern int  ProjectEffectSprite(SVECTOR* world, int* outxy);   // GteMatrix.cpp 0x0040aa50
extern int  is_entity_in_switch_zone(VECTOR* position, void* zoneData); // Room.cpp 0x00462d90
extern unsigned char apply_weapon_damage(unsigned int weapon_id);       // WeaponDamage.cpp 0x0043c020
extern unsigned char weapon_autoaim_check(void);                        // PlayerAnimations.cpp 0x0045a4b0
extern void FUN_0047cf80(int param1, unsigned int param2, unsigned int param3,
                          unsigned int param4, MATRIX* param5);          // CmdFunctions.cpp 0x0047cf80

// ============================================================================
// g_activeEffectIndex (0x00bf0a2e) - the slot being processed. update_2d_effects
// walks the pool through this byte; every behavior and the render helpers read
// it to find their effect. Lives in .bss adjacent to the effect texture state.
// ============================================================================
unsigned char g_activeEffectIndex = 0;

// ============================================================================
// Behavior support data
// ============================================================================

// 0x004c47b0 / 0x004c47b4 - constants added to the per-camera scale values by
// EffectActor_UpdateAndRender only. Both are 0 in the shipped exe.
static const int g_EffectScaleBiasX = 0;
static const int g_EffectScaleBiasY = 0;

// ============================================================================
// Blend tables (0x004c4f50) - one record per sprite-depth slot of
// g_RoomEffectSpriteTable. Entry: {startV, len, blendMode, colorIdx}; the
// render scans for the first entry whose texV < startV + len. The colorIdx
// then selects a color-tint record below.
//
// The table packs every slot's entries consecutively; g_EffectBlendStart gives
// the first row of each slot (see below). Declared with the flat row count
// (124): MSVC counts each brace pair as one ROW of a [32][4] array, so the
// variable-length slot layout cannot use the 32-row form.
// ============================================================================
static const unsigned char g_EffectBlendTable[124][4] = {
    // slot  0
    { 0x00, 0x40, 0x00, 0x00 },
    { 0x40, 0x70, 0x80, 0x01 },
    { 0xb0, 0x40, 0x80, 0x02 },
    { 0xf0, 0x10, 0x80, 0x03 },
    // slot  1
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    // slot  2
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x18, 0x00, 0x08 },
    // slot  3
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x28, 0x00, 0x09 },
    { 0xbb, 0x18, 0x00, 0x0a },
    { 0xd3, 0x20, 0x00, 0x0b },
    // slot  4
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x28, 0x00, 0x0c },
    { 0xbb, 0x18, 0x00, 0x0d },
    // slot  5
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x18, 0x00, 0x0e },
    { 0xab, 0x18, 0x00, 0x0f },
    { 0xc3, 0x20, 0x00, 0x10 },
    // slot  6
    { 0x03, 0x20, 0x00, 0x11 },
    { 0x23, 0x20, 0x00, 0x12 },
    // slot  7
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x48, 0x80, 0x13 },
    // slot  8
    { 0x03, 0x50, 0x80, 0x14 },
    // slot  9
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x18, 0x00, 0x15 },
    // slot 10
    { 0x03, 0x78, 0x00, 0x16 },
    // slot 11
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x10, 0x80, 0x17 },
    // slot 12
    { 0x03, 0x40, 0x80, 0x18 },
    // slot 13
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x28, 0x00, 0x19 },
    { 0xbb, 0x18, 0x00, 0x1a },
    { 0xd3, 0x10, 0x80, 0x1b },
    // slot 14
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x18, 0x00, 0x1c },
    { 0xab, 0x40, 0x00, 0x1d },
    // slot 15
    { 0x03, 0xe8, 0x80, 0x1e },
    // slot 16
    { 0x03, 0x40, 0x00, 0x1f },
    // slot 17
    { 0x03, 0x40, 0x00, 0x20 },
    { 0x43, 0x20, 0x00, 0x21 },
    { 0x63, 0x20, 0x00, 0x22 },
    // slot 18
    { 0x03, 0x40, 0x00, 0x23 },
    // slot 19
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x50, 0x00, 0x24 },
    // slot 20
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x40, 0x00, 0x25 },
    // slot 21
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x40, 0x00, 0x26 },
    // slot 22
    { 0x03, 0x50, 0x00, 0x27 },
    // slot 23
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x40, 0x00, 0x28 },
    // slot 24
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x40, 0x00, 0x29 },
    { 0xd3, 0x10, 0x80, 0x2a },
    // slot 25
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x18, 0x00, 0x2b },
    { 0xab, 0x40, 0x00, 0x2c },
    // slot 26
    { 0x03, 0x18, 0x00, 0x2d },
    { 0x1b, 0x48, 0x00, 0x2e },
    // slot 27
    { 0x03, 0x18, 0x00, 0x04 },
    { 0x1b, 0x48, 0x80, 0x05 },
    { 0x63, 0x18, 0x80, 0x06 },
    { 0x7b, 0x18, 0x80, 0x07 },
    { 0x93, 0x28, 0x00, 0x2f },
    { 0xbb, 0x18, 0x00, 0x30 },
    { 0xd3, 0x18, 0x00, 0x31 },
    // slot 28
    { 0x03, 0x48, 0x00, 0x32 },
    { 0x4b, 0x18, 0x80, 0x33 },
    // slot 29
    { 0x03, 0xfd, 0x00, 0x34 },
    // slot 30
    { 0x03, 0x18, 0x00, 0x35 },
    { 0x1b, 0x20, 0x00, 0x36 },
    { 0x3b, 0x20, 0x00, 0x37 },
    { 0x5b, 0x20, 0x00, 0x38 },
    { 0x7b, 0x10, 0x80, 0x39 },
    { 0x8b, 0x18, 0x00, 0x3a },
    // slot 31
    { 0x03, 0xf0, 0x00, 0x3b },
};
// ============================================================================
// Color-tint tables (0x004c5288) - one RGB-triplet row per tint level; the
// render picks row `tint` (clamped to the record's count). Stored in the exe
// as 0xAARRGGBB dwords; only bytes 0-2 (R,G,B) are read.
// ============================================================================
struct EffectColorRecord {
    int   count;
    const unsigned char* table;   // 3 bytes per tint level
};

static const unsigned char g_EffectColorTable0[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable1[] = { 0xff, 0xff, 0xff, 0xb2, 0xb2, 0xb2, 0xff, 0xcc, 0x66, 0xcc, 0xcc, 0x33 };
static const unsigned char g_EffectColorTable2[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable3[] = { 0xff, 0x99, 0x99, 0x99, 0x99, 0xff };
static const unsigned char g_EffectColorTable4[] = { 0x69, 0x1e, 0x0a, 0x37, 0x5a, 0x14, 0x91, 0x5a, 0x14, 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable5[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable6[] = { 0xff, 0xff, 0xff, 0x7f, 0x7f, 0x7f, 0xcc, 0xff, 0xcc, 0xff, 0xff, 0x99 };
static const unsigned char g_EffectColorTable7[] = { 0xb2, 0xb2, 0xff, 0xff, 0xaf, 0x69, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f };
static const unsigned char g_EffectColorTable8[] = { 0xb2, 0xb2, 0xff, 0xff, 0xb2, 0xb2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb2 };
static const unsigned char g_EffectColorTable9[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable10[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable11[] = { 0xd2, 0xa0, 0x28, 0x66, 0xb2, 0x66 };
static const unsigned char g_EffectColorTable12[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable13[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable14[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable15[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable16[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable17[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable18[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable19[] = { 0xb4, 0xbe, 0xff, 0xff, 0x73, 0x23 };
static const unsigned char g_EffectColorTable20[] = { 0xb4, 0xbe, 0xff, 0xff, 0x73, 0x23 };
static const unsigned char g_EffectColorTable21[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable22[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable23[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable24[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable25[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable26[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable27[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable28[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable29[] = { 0xff, 0xff, 0xff, 0xb4, 0xc8, 0xb4 };
static const unsigned char g_EffectColorTable30[] = { 0xff, 0xff, 0xff, 0x6e, 0x78, 0xae };
static const unsigned char g_EffectColorTable31[] = { 0xff, 0xff, 0xff, 0xb4, 0xc8, 0xb4 };
static const unsigned char g_EffectColorTable32[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable33[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable34[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable35[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable36[] = { 0xff, 0xff, 0xff, 0xcc, 0xff, 0xcc, 0xff, 0xb2, 0xb2 };
static const unsigned char g_EffectColorTable37[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable38[] = { 0xb2, 0xb2, 0xff, 0xb2, 0xb2, 0xff, 0xcc, 0xcc, 0xff, 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable39[] = { 0xff, 0xff, 0xff, 0xcc, 0xff, 0xcc, 0xff, 0xb2, 0xb2 };
static const unsigned char g_EffectColorTable40[] = { 0xff, 0xff, 0xff, 0xb4, 0xc8, 0xb4 };
static const unsigned char g_EffectColorTable41[] = { 0xff, 0xff, 0xff, 0xb4, 0xc8, 0xb4 };
static const unsigned char g_EffectColorTable42[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable43[] = { 0xff, 0xb2, 0xb2, 0xb2, 0xb2, 0xff, 0xff, 0x66, 0x66, 0x93, 0x66, 0xff };
static const unsigned char g_EffectColorTable44[] = { 0xff, 0xff, 0xff, 0xb4, 0xc8, 0xb4 };
static const unsigned char g_EffectColorTable45[] = { 0xff, 0xb2, 0xb2, 0xb2, 0xb2, 0xff, 0xff, 0x66, 0x66, 0xa0, 0xb4, 0xff };
static const unsigned char g_EffectColorTable46[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable47[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable48[] = { 0x99, 0x33, 0x33, 0x31, 0x8c, 0x2d, 0xc1, 0xa0, 0x33 };
static const unsigned char g_EffectColorTable49[] = { 0xff, 0xb2, 0xb2, 0xb2, 0xb2, 0xff, 0xff, 0x66, 0x66, 0x93, 0x66, 0xff };
static const unsigned char g_EffectColorTable50[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable51[] = { 0xff, 0xff, 0xff, 0x64, 0x64, 0x64, 0xd2, 0xa0, 0x8c, 0xd2, 0xaa, 0x46 };
static const unsigned char g_EffectColorTable52[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable53[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable54[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable55[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable56[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable57[] = { 0xff, 0xff, 0xff };
static const unsigned char g_EffectColorTable58[] = { 0xff, 0xb2, 0xb2, 0xb2, 0xb2, 0xff, 0xff, 0x66, 0x66, 0x93, 0x66, 0xff };
static const unsigned char g_EffectColorTable59[] = { 0xff, 0xff, 0xff };

static const EffectColorRecord g_EffectColorRecords[60] = {
    { 1, g_EffectColorTable0 }, { 4, g_EffectColorTable1 }, { 1, g_EffectColorTable2 }, { 2, g_EffectColorTable3 }, { 4, g_EffectColorTable4 }, { 3, g_EffectColorTable5 },
    { 4, g_EffectColorTable6 }, { 4, g_EffectColorTable7 }, { 4, g_EffectColorTable8 }, { 3, g_EffectColorTable9 }, { 3, g_EffectColorTable10 }, { 2, g_EffectColorTable11 },
    { 3, g_EffectColorTable12 }, { 3, g_EffectColorTable13 }, { 3, g_EffectColorTable14 }, { 1, g_EffectColorTable15 }, { 1, g_EffectColorTable16 }, { 1, g_EffectColorTable17 },
    { 1, g_EffectColorTable18 }, { 2, g_EffectColorTable19 }, { 2, g_EffectColorTable20 }, { 3, g_EffectColorTable21 }, { 1, g_EffectColorTable22 }, { 1, g_EffectColorTable23 },
    { 1, g_EffectColorTable24 }, { 3, g_EffectColorTable25 }, { 3, g_EffectColorTable26 }, { 1, g_EffectColorTable27 }, { 3, g_EffectColorTable28 }, { 2, g_EffectColorTable29 },
    { 2, g_EffectColorTable30 }, { 2, g_EffectColorTable31 }, { 1, g_EffectColorTable32 }, { 1, g_EffectColorTable33 }, { 2, g_EffectColorTable34 }, { 1, g_EffectColorTable35 },
    { 3, g_EffectColorTable36 }, { 1, g_EffectColorTable37 }, { 4, g_EffectColorTable38 }, { 3, g_EffectColorTable39 }, { 2, g_EffectColorTable40 }, { 2, g_EffectColorTable41 },
    { 1, g_EffectColorTable42 }, { 4, g_EffectColorTable43 }, { 2, g_EffectColorTable44 }, { 4, g_EffectColorTable45 }, { 1, g_EffectColorTable46 }, { 3, g_EffectColorTable47 },
    { 3, g_EffectColorTable48 }, { 4, g_EffectColorTable49 }, { 1, g_EffectColorTable50 }, { 4, g_EffectColorTable51 }, { 1, g_EffectColorTable52 }, { 1, g_EffectColorTable53 },
    { 1, g_EffectColorTable54 }, { 1, g_EffectColorTable55 }, { 1, g_EffectColorTable56 }, { 1, g_EffectColorTable57 }, { 4, g_EffectColorTable58 }, { 1, g_EffectColorTable59 },
};

// ============================================================================
// Camera light-record index (0x004c5468): byte[(room + fold*0x20)*8 + camera],
// where fold = stage > 4 ? stage - 5 : stage. Selects the light record used to
// shade the effect sprite.
// ============================================================================
static const unsigned char g_EffectCameraLightIndex[5 * 0x20 * 8] = {
    // stage 0
    0, 0, 0, 0, 0, 0, 0, 0,   // room  0
    0, 0, 0, 0, 0, 0, 0, 0,   // room  1
    0, 0, 0, 0, 0, 0, 0, 0,   // room  2
    0, 0, 0, 0, 0, 0, 0, 0,   // room  3
    0, 0, 0, 0, 0, 0, 0, 0,   // room  4
    0, 0, 0, 0, 0, 0, 0, 0,   // room  5
    0, 0, 0, 0, 0, 0, 0, 0,   // room  6
    0, 0, 0, 0, 0, 0, 0, 0,   // room  7
    0, 0, 0, 0, 0, 0, 0, 0,   // room  8
    0, 0, 0, 0, 0, 0, 0, 0,   // room  9
    0, 0, 0, 0, 0, 0, 0, 0,   // room 10
    0, 0, 0, 0, 0, 0, 0, 0,   // room 11
    6, 6, 6, 6, 6, 0, 6, 6,   // room 12
    0, 0, 0, 0, 0, 0, 0, 0,   // room 13
    0, 0, 0, 0, 0, 0, 0, 0,   // room 14
    0, 0, 0, 0, 0, 0, 0, 0,   // room 15
    0, 0, 0, 0, 0, 0, 0, 0,   // room 16
    0, 0, 0, 0, 0, 0, 0, 0,   // room 17
    0, 0, 0, 0, 0, 0, 0, 0,   // room 18
    0, 0, 0, 0, 0, 0, 0, 0,   // room 19
    0, 0, 0, 0, 0, 0, 0, 0,   // room 20
    0, 0, 0, 0, 0, 0, 0, 0,   // room 21
    0, 0, 0, 0, 0, 0, 0, 0,   // room 22
    0, 0, 0, 0, 0, 0, 0, 0,   // room 23
    0, 0, 0, 0, 0, 0, 0, 0,   // room 24
    0, 0, 0, 0, 0, 0, 0, 0,   // room 25
    0, 0, 0, 0, 0, 0, 0, 0,   // room 26
    0, 0, 0, 0, 0, 0, 0, 0,   // room 27
    0, 0, 0, 0, 0, 0, 0, 0,   // room 28
    0, 0, 0, 0, 0, 0, 0, 0,   // room 29
    0, 0, 0, 0, 0, 0, 0, 0,   // room 30
    0, 0, 0, 0, 0, 0, 0, 0,   // room 31
    // stage 1
    0, 0, 0, 0, 0, 0, 0, 0,   // room  0
    0, 0, 0, 0, 0, 0, 0, 0,   // room  1
    0, 0, 0, 0, 0, 0, 0, 0,   // room  2
    0, 0, 0, 0, 0, 0, 0, 0,   // room  3
    0, 0, 0, 0, 0, 0, 0, 0,   // room  4
    0, 0, 3, 0, 0, 0, 0, 0,   // room  5
    0, 0, 0, 0, 0, 0, 0, 0,   // room  6
    0, 0, 0, 0, 0, 0, 0, 0,   // room  7
    0, 0, 0, 0, 0, 0, 0, 0,   // room  8
    0, 0, 0, 0, 0, 0, 0, 0,   // room  9
    0, 0, 0, 0, 0, 0, 0, 0,   // room 10
    0, 0, 2, 0, 0, 0, 0, 0,   // room 11
    3, 3, 3, 3, 3, 3, 3, 3,   // room 12
    0, 0, 0, 0, 0, 0, 0, 0,   // room 13
    0, 0, 0, 0, 0, 0, 0, 0,   // room 14
    0, 0, 0, 0, 0, 0, 0, 0,   // room 15
    0, 0, 0, 0, 3, 0, 0, 0,   // room 16
    0, 0, 0, 0, 0, 0, 0, 0,   // room 17
    0, 0, 0, 0, 0, 0, 0, 0,   // room 18
    0, 0, 0, 0, 0, 0, 0, 0,   // room 19
    0, 0, 0, 0, 0, 0, 0, 0,   // room 20
    0, 0, 0, 0, 0, 0, 0, 0,   // room 21
    0, 0, 0, 0, 0, 0, 0, 0,   // room 22
    0, 0, 0, 0, 0, 0, 0, 0,   // room 23
    0, 0, 0, 0, 0, 0, 0, 0,   // room 24
    0, 0, 0, 0, 0, 0, 0, 0,   // room 25
    0, 0, 0, 0, 0, 0, 0, 0,   // room 26
    0, 0, 0, 0, 0, 0, 0, 0,   // room 27
    0, 0, 0, 0, 0, 0, 0, 0,   // room 28
    0, 0, 0, 0, 0, 0, 0, 0,   // room 29
    0, 0, 0, 0, 0, 0, 0, 0,   // room 30
    0, 0, 0, 0, 0, 0, 0, 0,   // room 31
    // stage 2
    0, 0, 0, 0, 0, 0, 0, 0,   // room  0
    0, 0, 0, 0, 0, 0, 0, 0,   // room  1
    0, 0, 0, 0, 0, 0, 0, 0,   // room  2
    0, 0, 0, 0, 0, 0, 0, 0,   // room  3
    0, 0, 0, 0, 0, 0, 0, 0,   // room  4
    0, 0, 0, 0, 0, 0, 0, 0,   // room  5
    0, 0, 0, 0, 0, 0, 0, 0,   // room  6
    0, 0, 0, 0, 0, 0, 0, 0,   // room  7
    0, 0, 0, 0, 0, 0, 0, 0,   // room  8
    0, 0, 0, 0, 0, 0, 0, 0,   // room  9
    0, 0, 0, 0, 0, 0, 0, 0,   // room 10
    0, 0, 0, 0, 0, 0, 0, 0,   // room 11
    0, 0, 0, 0, 0, 0, 0, 0,   // room 12
    0, 0, 0, 0, 0, 0, 0, 0,   // room 13
    0, 0, 0, 0, 0, 0, 0, 0,   // room 14
    0, 0, 0, 0, 0, 0, 0, 0,   // room 15
    0, 0, 0, 0, 0, 0, 0, 0,   // room 16
    0, 0, 0, 0, 0, 0, 0, 0,   // room 17
    0, 0, 0, 0, 0, 0, 0, 0,   // room 18
    0, 0, 0, 0, 0, 0, 0, 0,   // room 19
    0, 0, 0, 0, 0, 0, 0, 0,   // room 20
    0, 0, 0, 0, 0, 0, 0, 0,   // room 21
    0, 0, 0, 0, 0, 0, 0, 0,   // room 22
    0, 0, 0, 0, 0, 0, 0, 0,   // room 23
    0, 0, 0, 0, 0, 0, 0, 0,   // room 24
    0, 0, 0, 0, 0, 0, 0, 0,   // room 25
    0, 0, 0, 0, 0, 0, 0, 0,   // room 26
    0, 0, 0, 0, 0, 0, 0, 0,   // room 27
    0, 0, 0, 0, 0, 0, 0, 0,   // room 28
    0, 0, 0, 0, 0, 0, 0, 0,   // room 29
    0, 0, 0, 0, 0, 0, 0, 0,   // room 30
    0, 0, 0, 0, 0, 0, 0, 0,   // room 31
    // stage 3
    0, 0, 0, 0, 0, 0, 0, 0,   // room  0
    0, 0, 0, 0, 0, 0, 0, 0,   // room  1
    0, 0, 0, 0, 0, 0, 0, 0,   // room  2
    0, 0, 0, 0, 0, 0, 0, 0,   // room  3
    0, 0, 0, 0, 0, 0, 0, 0,   // room  4
    0, 0, 0, 0, 0, 0, 0, 0,   // room  5
    0, 0, 0, 0, 0, 0, 0, 0,   // room  6
    0, 0, 0, 0, 0, 0, 0, 0,   // room  7
    0, 0, 0, 0, 0, 0, 0, 0,   // room  8
    0, 0, 0, 0, 0, 0, 0, 0,   // room  9
    0, 0, 0, 0, 0, 0, 0, 0,   // room 10
    0, 0, 0, 0, 0, 0, 0, 0,   // room 11
    5, 5, 5, 5, 5, 5, 5, 5,   // room 12
    0, 0, 0, 0, 0, 0, 0, 0,   // room 13
    0, 0, 0, 0, 0, 0, 0, 0,   // room 14
    0, 0, 0, 0, 0, 0, 0, 0,   // room 15
    0, 0, 0, 0, 0, 0, 0, 0,   // room 16
    0, 0, 0, 0, 0, 0, 0, 0,   // room 17
    0, 0, 0, 0, 0, 0, 0, 0,   // room 18
    0, 0, 0, 0, 0, 0, 0, 0,   // room 19
    0, 0, 0, 0, 0, 0, 0, 0,   // room 20
    0, 0, 0, 0, 0, 0, 0, 0,   // room 21
    0, 0, 0, 0, 0, 0, 0, 0,   // room 22
    0, 0, 0, 0, 0, 0, 0, 0,   // room 23
    0, 0, 0, 0, 0, 0, 0, 0,   // room 24
    0, 0, 0, 0, 0, 0, 0, 0,   // room 25
    0, 0, 0, 0, 0, 0, 0, 0,   // room 26
    0, 0, 0, 0, 0, 0, 0, 0,   // room 27
    0, 0, 0, 0, 0, 0, 0, 0,   // room 28
    0, 0, 0, 0, 0, 0, 0, 0,   // room 29
    0, 0, 0, 0, 0, 0, 0, 0,   // room 30
    0, 0, 0, 0, 0, 0, 0, 0,   // room 31
    // stage 4
    0, 0, 0, 0, 0, 0, 0, 0,   // room  0
    0, 0, 0, 0, 0, 0, 0, 0,   // room  1
    0, 0, 0, 0, 0, 0, 0, 0,   // room  2
    0, 0, 0, 0, 0, 0, 0, 0,   // room  3
    0, 0, 0, 0, 0, 0, 0, 0,   // room  4
    0, 0, 0, 0, 0, 0, 0, 0,   // room  5
    0, 0, 0, 0, 0, 0, 0, 0,   // room  6
    0, 0, 0, 0, 0, 0, 0, 0,   // room  7
    0, 0, 0, 0, 0, 0, 0, 0,   // room  8
    0, 0, 0, 0, 0, 0, 0, 0,   // room  9
    0, 0, 0, 0, 0, 0, 0, 0,   // room 10
    0, 0, 0, 0, 0, 0, 0, 0,   // room 11
    0, 0, 0, 0, 0, 0, 0, 0,   // room 12
    0, 0, 0, 0, 0, 0, 0, 0,   // room 13
    0, 0, 0, 0, 0, 0, 0, 0,   // room 14
    0, 0, 0, 0, 0, 0, 0, 0,   // room 15
    0, 0, 0, 0, 0, 0, 0, 0,   // room 16
    0, 0, 0, 0, 0, 0, 0, 0,   // room 17
    0, 0, 0, 0, 0, 0, 0, 0,   // room 18
    0, 0, 0, 0, 0, 1, 0, 0,   // room 19
    0, 0, 0, 0, 0, 0, 0, 0,   // room 20
    0, 0, 0, 0, 0, 0, 0, 0,   // room 21
    0, 0, 0, 0, 0, 0, 0, 0,   // room 22
    0, 0, 0, 0, 0, 0, 0, 0,   // room 23
    0, 0, 0, 0, 0, 0, 0, 0,   // room 24
    0, 0, 0, 0, 0, 0, 0, 0,   // room 25
    0, 0, 0, 0, 0, 0, 0, 0,   // room 26
    0, 0, 0, 0, 0, 0, 0, 0,   // room 27
    0, 0, 0, 0, 0, 0, 0, 0,   // room 28
    0, 0, 0, 0, 0, 0, 0, 0,   // room 29
    0, 0, 0, 0, 0, 0, 0, 0,   // room 30
    0, 0, 0, 0, 0, 0, 0, 0,   // room 31
};

static const int g_EffectLightRecords[7][3] = {
    {      0,      0,      0 },
    {      0,      0,      0 },
    {      0,      0,      0 },
    {  -1000,      0,   1000 },
    {  -4000,      0,      0 },
    {  -2500,      0,   2500 },
    {      0,      0,     60 },
};

// ============================================================================
// effect_depth_record - the sprite-depth slot for the current effect.
//
// The original reads a byte at 0x004c48a0 + depth + (stage*0x20 + room)*4,
// where depth is the sprite's texture-Y (g_TextureDesc.texturePage). That region is
// the ROOM EFFECT SPRITE TABLE at 0x004c48b8 (g_RoomEffectSpriteTable) viewed
// 0x18 bytes early: weapon-FX texY starts at 0x18 and room effects continue
// from there, so slot = depth - 0x18. Depth < 0x18 would land on six dwords of
// unrelated pointer data (0x00410680/0x0040cc60) in the original; no sprite
// ever has texY that low, so the guard below only fires on impossible input.
// ============================================================================
static unsigned int effect_depth_record(void)
{
    int base = ((int)get_stage_id() * 0x20 + (int)g_roomId) * 4;
    int slot = (int)g_TextureDesc.texturePage - 0x18;
    int idx  = base + slot;
    if (idx < 0 || idx >= (int)sizeof(g_RoomEffectSpriteTable)) return 0xFF;
    // The room's own four pages come through the arrange-aware row
    // (dc/ArrangeStages.cpp): an arrange room has no row of its own in the PC
    // table, and where ADVANCED adds an effect its base row still says 0xFF -
    // which drops the effect entirely, below. texY past 0x1B keeps the
    // original's read into the next room's bytes.
    if (slot >= 0 && slot < 4) return room_effect_page_entry()[slot];
    return g_RoomEffectSpriteTable[idx];
}

// ============================================================================
// effect_light_value - the per-camera lighting value used to scale sprites:
// *(int*)(g_RdtPointer + 0xbc + cameraId * 0x2c). The RDT lights block.
// ============================================================================
static int effect_light_value(void)
{
    return *(int*)((char*)g_RdtPointer + 0xbc + (int)g_roomCameraId * 0x2c);
}

// ============================================================================
// Behavior functions (0x0040cc70..0x0040ce50)
//
// Each behavior is a tiny state machine operating on the active effect's
// animation header (eff->animHeader = the 24-byte block copied from the RDT/esp
// animation data at spawn). The header doubles as the phase timer: byte 3
// (lightFactor) counts down to trigger the next phase, bytes 0-2 are
// phase-specific counts/parameters, and the short pairs at +4/+6/+8 and
// +0xc/+0xe/+0x10 are the per-frame velocity deltas and positions that
// EffectActor_UpdateAndRender integrates when (g_message_flags & 8).
//
// The shared "advance to the next phase" helper (0x0040cd40) moves animDataFrame
// forward 24 bytes and copies that block over the header, so a chain of blocks
// in the data describes the whole life of the effect (e.g. rise, burst, fall,
// splat).
// ============================================================================

// Header short accessors (header spans effect+0x04..0x15).
#define AH_SHORT(eff, off)  (*(short*)&(eff)->animHeader[(off)])
#define AH_USHORT(eff, off) (*(unsigned short*)&(eff)->animHeader[(off)])

// Forward declarations (behaviors reference each other out of order).
static void effect_behavior_spawn_from_header(void);
static void effect_behavior_drop_splat(void);
static void effect_behavior_kill(void);
static void effect_shot_impact(int hit);
static void effect_rocket_explode(void);

// ============================================================================
// effect_behavior_next_phase (0x0040cd40)
// Advance to the next 24-byte animation block: animDataFrame += 0x18, then copy
// the block over the slot's header. yaw and lightFactor survive the swap (the
// decompiler's temporaries). Also used as behavior 28.
// ============================================================================
static void effect_behavior_next_phase(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->animDataFrame += 0x18;

    unsigned char lightFactor = eff->lightFactor;
    short yaw = eff->yaw;

    unsigned int* src = (unsigned int*)eff->animDataFrame;
    unsigned char* dst = (unsigned char*)&eff->animId;
    for (int i = 6; i != 0; i--) {
        unsigned int v = *src++;
        *dst++ = (unsigned char)v;
        *dst++ = (unsigned char)(v >> 8);
        *dst++ = (unsigned char)(v >> 0x10);
        *dst++ = (unsigned char)(v >> 0x18);
    }

    eff->yaw = yaw;
    eff->lightFactor = lightFactor;
}

// ============================================================================
// effect_behavior_set_frame (0x0040ce50)
// Point the sprite at the animation frame named by animHeader[0]: the vram
// frame entry at vramInfo + animHeader[0]*4 becomes the current one. Also used
// as behavior 31.
// ============================================================================
static void effect_behavior_set_frame(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->vramInfoBackup = eff->vramInfo + (unsigned int)eff->animHeader[0] * 4;
    eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
    eff->uvDataBackup = (unsigned int)*(unsigned char*)eff->vramInfoBackup * 4 + eff->uvData;
}

// ============================================================================
// effect_probe_ground (0x0047daf0)
// Walk the boundary records of the quadrant `param_1` falls in and test the
// point against each with `param_2` as the probe offset. Returns non-zero when
// a record's flag bits 8-9 are 0b11 (ground contact); otherwise the OR of the
// seen flags.
//
// The player_pos packing the decompiler shows is self-assignment - param_1 IS
// g_playerPosScratch (the behaviors stage the effect position there before
// calling), so the 16-bit stores read and write the same dwords. The original's
// quadrant span is [group[cell-1], group[cell]), one record list earlier than
// room_check_sight_blocked's [group[cell], group[cell+1]); both are reproduced
// from their own assembly.
// ============================================================================
static unsigned short effect_probe_ground(SVECTOR* pos, SVECTOR* offset, unsigned int radius)
{
    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return 0;

    unsigned short flags = 0;
    unsigned int cell = ChkOutsideCell((VECTOR*)pos, offset,
                                       *(int*)g_RdtPointer->boundaries,
                                       (int)*(short*)((char*)g_RdtPointer->boundaries + 2));

    unsigned char* boundaries = g_RdtPointer->boundaries;
    unsigned int* group = (unsigned int*)(boundaries + 8 + cell * 4);
    unsigned int rec = group[-1];
    unsigned int last = *group;

    while (rec < last) {
        unsigned short r = boundary_classify_flags(offset, (RDT_Boundary*)rec, radius);
        if ((short)r != -1) {
            if ((unsigned char)((r & 0x0300) >> 8) == 0x03) return 1;
            flags = (unsigned short)(flags | (r & 0x0300));
        }
        rec += 0xc;
    }
    return flags;
}

// ============================================================================
// effect_ground_splat (0x0040e770)
// Land on the ground: jump to the splash frame (vramInfo + 0x14), zero the
// velocity header, switch to the inert behavior (animId = updateId = 1), and
// if the phase asked for it (animHeader[1] != 0) spawn a type-9 dust ring and
// play the splat sound.
// ============================================================================
static void effect_ground_splat(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->vramInfoBackup = eff->vramInfo + 0x14;
    eff->frameIndex = *(unsigned char*)eff->vramInfoBackup;
    eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
    eff->uvDataBackup = (unsigned int)eff->frameIndex * 4 + eff->uvData;
    eff->animId = 1;
    eff->updateId = 1;
    eff->animHeader[6] = 0;
    eff->animHeader[7] = 0;
    AH_SHORT(eff, 4) = AH_SHORT(eff, 6);
    AH_SHORT(eff, 0xe) = AH_SHORT(eff, 4);
    AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);

    if (eff->animHeader[1] != 0) {
        g_playerPosScratch.x = (int)eff->posX;
        g_playerPosScratch.y = (int)eff->posY;
        g_playerPosScratch.z = (int)eff->posZ;
        Effect_CreateBillboard(9, 1, 0, NULL, &g_playerPosScratch, eff->animHeader[3]);
        Play3DSnd(2, 8, 0, (int)&g_playerPosScratch);
    }
}

// ============================================================================
// effect_distance_to_player (0x0043bf20)
// Is the horizontal distance between (x, z) and the player's feet <
// g_playerDisplacement? (g_playerDisplacement is the range staged by the
// caller - 600 for the projectile splash.)
// ============================================================================
static bool effect_distance_to_player(short x, short z, PlayerEntity* player)
{
    int dz = (int)player->scaMatrixData.localMatrix.t[2] - (int)z;
    int dx = (int)player->scaMatrixData.localMatrix.t[0] - (int)x;
    int dist = SquareRoot0(dz * dz + dx * dx);
    return dist < g_playerDisplacement;
}

// ============================================================================
// effect_distance_to_entity (0x0043bf60)
// Same test with a 500-unit grace: dist < g_playerDisplacement + 500.
// ============================================================================
static bool effect_distance_to_entity(short x, short z, Entity* ent)
{
    int dz = (int)ent->scaMatrixData.localMatrix.t[2] - (int)z;
    int dx = (int)ent->scaMatrixData.localMatrix.t[0] - (int)x;
    int dist = SquareRoot0(dz * dz + dx * dx);
    return dist < g_playerDisplacement + 500;
}

// ============================================================================
// effect_projectile_hit_check (0x0043bd90)
// The projectile splash test used by behaviors 0x16 (drop_splat, range 600) and
// 0x37 (splash_timer, range 900): when the splash lands within `range` of the
// player with a clear line of sight, damage the player; the stage-2 room-0xc
// shark gets the same test with a grace radius. Returns 1 when either was hit.
// ============================================================================
static int effect_projectile_hit_check(int range, short x, short z)
{
    PlayerEntity* pPVar = NULL;
    g_playerDisplacement = range;

    if (effect_distance_to_player(x, z, &g_playerEntity)) {
        pPVar = &g_playerEntity;
    }

    if (pPVar != NULL) {
        if (check_weapon_line_of_sight((VECTOR*)pPVar->scaMatrixData.localMatrix.t) == 0) {
            if (pPVar->isBeingAttackedFlag == 0) {
                if ((g_EnemiesList[0].id == 3) || (g_EnemiesList[0].id == 4)) {
                    if ((g_RandSeed & 1) * ((unsigned int)g_RandSeed & 1) != 0) {
                        pPVar->healthStatusFlags |= 2;
                        *(unsigned char*)((char*)pPVar + 0x174) = 0x96;
                    }
                }
                short* health = &pPVar->health;
                short base = *health;
                if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                    *health = (short)(base - 5);
                    if (g_EnemiesList[0].id == 3) *health = (short)(base - 10);
                    if (g_EnemiesList[0].id == 4) *health = *health - 7;
                } else {
                    *health = (short)(base - 0xf);
                    if (g_EnemiesList[0].id == 4) *health = (short)(base - 0x1e);
                }
                pPVar->isBeingAttackedFlag = 1;
                pPVar->animationId = 2;
                pPVar->animFrameId = 0;
                pPVar->action_behavior = 100;
                pPVar->action_state = 0;
                if (*health < 0) {
                    pPVar->animationId = 3;
                    pPVar->animFrameId = 0;
                    pPVar->action_behavior = 0;
                    pPVar->action_state = 0;
                }
            }
            return 1;
        }
    }

    if ((g_EnemiesList[1].id == 0x13) && (g_stageId == STAGE_COURTYARD) && (g_roomId == ROOM_BLACK_TIGER_ROOM)) {
        Entity* shark = &g_EnemiesList[1];
        if (effect_distance_to_entity(x, z, shark)) {
            if (*(unsigned char*)((char*)shark + 0x138) == 0) {
                shark->health = (short)(shark->health - 5);
                *(unsigned char*)((char*)shark + 0x138) = 9;
                shark->state = 3;
                shark->ignore_player_flag = 0;
                shark->action_behavior = 0;
                shark->action_state = 0;
                if (shark->health >= 0) {
                    shark->state = 2;
                    shark->ignore_player_flag = 0;
                    shark->action_behavior = 0;
                    shark->action_state = 0;
                }
            }
            return 1;
        }
    }
    return 0;
}

// ============================================================================
// mirror_point_visible - the mirror visibility probe.
//
// `light` is the RDT camera record for the current camera, aimed at its posX
// (RDT + 0x9c + cameraId*0x2c), and `param_3` a world position. `param_2` is
// g_main_state_flags bit 1, the mirror's plane axis: 0 = the plane is Z = k,
// 1 = the plane is X = k, with k = g_mirrorPlaneCoord.
//
// First the camera and the position must lie on the SAME side of the plane -
// you cannot see the reflection of something behind the mirror - which is the
// sign test on the XOR. Then it intersects the camera-to-position segment with
// the plane and leaves that crossing coordinate in g_entity_bkp, returning 1
// only when it falls inside g_mirrorExtentMin .. g_mirrorExtentMax, the mirror's extent
// along the other axis. In short: is this point visible in the mirror?
//
// Called from effect_draw_mirror_reflection (effects),
// entity_build_mirror_joints (per entity joint) and the two mirror-pass call
// sites in update_entities / update_player_anim.
// ============================================================================
unsigned char mirror_point_visible(void* light, unsigned char param_2, int param_3)
{
    // *8, NOT *4. The original doubles the flag before scaling it:
    //   0048bd06  ADD AL,AL          ; AL = param_2 * 2
    //   0048bd16  LEA EBX,[ECX*0x4]  ; EBX = param_2 * 8
    // Ghidra folded the two into a single *4 and the port inherited it. With
    // *4 the axis-X case reads the position's Y for BOTH the plane coordinate
    // (pp - off + 8) and the cross coordinate (pp + off), which is degenerate;
    // with *8 it reads x for the plane and z for the extent, as it must. The
    // axis-Z case has offset 0 either way, which is why rooms 1120/1130
    // (plane Z) always worked and 1110/40B0/6110 (plane X) never did.
    unsigned int uVar4 = (unsigned int)param_2 * 8;
    unsigned int uVar5 = (unsigned int)g_mirrorPlaneCoord;
    char* lp = (char*)light;
    char* pp = (char*)param_3;

    int iVar3 = *(int*)(pp - uVar4 + 8);
    int iVar1 = *(int*)(lp - uVar4 + 8);

    unsigned int uVar2 = (unsigned int)(iVar1 - (int)uVar5) ^ (unsigned int)(iVar3 - (int)uVar5);
    if ((uVar2 & 0x80000000) != 0) return 0;

    int product = (*(int*)(pp + uVar4) - *(int*)(lp + uVar4)) * (iVar1 - (int)uVar5);
    int quotient = product / (int)(iVar3 - (int)(uVar5 * 2) + iVar1);

    g_entity_bkp = (unsigned int)(*(int*)(lp + uVar4) + quotient);
    return (unsigned char)(g_entity_bkp - (unsigned int)g_mirrorExtentMin
                           < (unsigned int)g_mirrorExtentMax - (unsigned int)g_mirrorExtentMin);
}

// ============================================================================
// Behavior 0/1 (0x0040cc70) - the inert behavior. Does nothing; the effect just
// holds its current frame and position until the sprite animation frees it.
// ============================================================================
static void effect_behavior_idle(void) { }

// ============================================================================
// Behavior 2 (0x0040d350) - phase timer that refreshes the transform: count
// animHeader[3] down; when it hits zero, advance to the next animation block
// and re-copy the sprite matrix into the slot's transform. The muzzle flash /
// ember tail pattern.
// ============================================================================
static void effect_behavior_timer_refresh(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[3] == 0) {
        effect_behavior_next_phase();
        memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
        return;
    }
    eff->animHeader[3]--;
}

// ============================================================================
// Behavior 3 (0x0040cc80) - freeze the current sprite frame (frameDelay = 0
// makes Effect_AnimateSprite advance every frame instead).
// ============================================================================
static void effect_behavior_hold_frame(void)
{
    g_effectPool[g_activeEffectIndex].frameDelay = 0;
}

// ============================================================================
// Behavior 4 (0x0040d5c0) - burning ember: count animHeader[3] down, then enter
// the next phase with a hot colour (lightFactor 0x1e) and a jump sideways
// (localOffsetX -= 0xfa).
// ============================================================================
static void effect_behavior_ember(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[3] == 0) {
        effect_behavior_next_phase();
        eff->lightFactor = 0x1e;
        eff->localOffsetX -= 0xfa;
        return;
    }
    eff->animHeader[3]--;
}

// ============================================================================
// Behavior 5 (0x0040d650) - gravity settle: probe straight down; on contact
// zero the velocity header and switch to the inert behavior.
// ============================================================================
static void effect_behavior_gravity_settle(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    if (effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4) != 0) {
        eff->animHeader[0x10] = 0;
        eff->animHeader[0x11] = 0;
        AH_SHORT(eff, 0xe) = AH_SHORT(eff, 0x10);
        AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);
        eff->animId = 1;
    }
}

// ============================================================================
// Behavior 6 (0x0040d620) - plain phase timer: count animHeader[3] down, then
// advance to the next animation block.
// ============================================================================
static void effect_behavior_timer_phase(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[3] == 0) {
        effect_behavior_next_phase();
        return;
    }
    eff->animHeader[3]--;
}

// ============================================================================
// Behavior 7 (0x0040d880) - floor bounce: when posY + vy crosses 0, bounce -
// switch to the inert behavior, zero the velocity header, mark the sprite
// ground-facing (flags |= 0x4007) and cancel the upward motion.
// ============================================================================
static void effect_behavior_bounce(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        eff->animId = 1;
        eff->updateId = 1;
        AH_USHORT(eff, 10) |= 0x4007;
        eff->animHeader[8] = 0;
        eff->animHeader[9] = 0;
        AH_SHORT(eff, 6) = AH_SHORT(eff, 8);
        AH_SHORT(eff, 4) = AH_SHORT(eff, 6);
        eff->animHeader[0x10] = 0;
        eff->animHeader[0x11] = 0;
        AH_SHORT(eff, 0xe) = AH_SHORT(eff, 0x10);
        AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);
        eff->rotSpeedY = (short)(-(eff->spriteOffsetY + eff->localOffsetY));
    }
}

// ============================================================================
// Behavior 8 (0x0040d740) - gravity impact: probe straight down; on contact
// switch to the inert behavior and play the landing sound. The impact splat
// pattern (blood / debris).
// ============================================================================
static void effect_behavior_gravity_impact(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    if (effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4) != 0) {
        eff->animId = 1;
        eff->animHeader[0] = 2;
        eff->animHeader[2] = 1;
        AH_SHORT(eff, 0xc) = (short)-AH_SHORT(eff, 0xc);
        if ((unsigned int)eff->animHeader[3] - (unsigned int)g_playerEntity.equippedWeaponId == -2) {
            g_playerPosScratch.x = (int)eff->posX;
            g_playerPosScratch.y = (int)eff->posY;
            g_playerPosScratch.z = (int)eff->posZ;
            Play3DSnd(1, 10, 0, (int)&g_playerPosScratch);
        }
    }
}

// ============================================================================
// Behavior 9 (0x0040d9e0) - gravity rise: pull the effect toward the player's
// height (posY); when it arrives, mark it ground-facing, switch to the inert
// behavior and freeze the vertical motion.
// ============================================================================
static void effect_behavior_gravity_rise(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->animHeader[10] = 3;
    eff->animHeader[11] = 0x40;

    if ((int)g_playerEntity.posY < (int)eff->posY + (int)AH_SHORT(eff, 0xe)) {
        AH_USHORT(eff, 10) |= 0x400b;
        eff->animId = 0x2f;
        eff->updateId = 0;
        eff->rotSpeedY = (short)((g_playerEntity.posY - eff->spriteOffsetY) - eff->localOffsetY);
    }
}

// ============================================================================
// Behavior 10 (0x0040da90) - the ARROW / BULLET: gravity + player hit. Probes
// straight down; when the ground (or the menu-scene ceiling, when the item-box
// menu is open) is reached, switch to a random follow-up phase from the next
// animation block, recompute the yaw drift, and when the phase's weapon byte
// matches the equipped weapon spawn the hit-check. This is the behavior that
// makes projectiles deal damage to the player.
// ============================================================================
static void effect_behavior_projectile(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) == 0) {
        if ((int)g_playerEntity.posY < (int)eff->posY + (int)AH_SHORT(eff, 0xe)) {
            AH_USHORT(eff, 10) |= 0x400b;
            eff->animDataFrame += 0x18;
            eff->animDataFrame += ((unsigned int)g_RandSeed % (unsigned int)eff->animHeader[1]) * 0x18;
            eff->updateId = *(unsigned char*)(eff->animDataFrame + 1);
            AH_SHORT(eff, 0xc) = *(short*)(eff->animDataFrame + 0x10);
            AH_SHORT(eff, 0xe) = *(short*)(eff->animDataFrame + 0x12);
            AH_SHORT(eff, 0x10) = (short)(((unsigned int)g_RandSeed % (unsigned int)eff->animHeader[1]) * 10
                                          + *(short*)(eff->animDataFrame + 0x14));
            eff->yaw += *(short*)(eff->animDataFrame + 0x16);
            eff->rotSpeedY = (short)((g_playerEntity.posY - eff->spriteOffsetY) - eff->localOffsetY);
            if (eff->animHeader[2] != 0) {
                AH_SHORT(eff, 0xc) = (short)-AH_SHORT(eff, 0xc);
            }
            if ((unsigned int)eff->animHeader[3] - (unsigned int)g_playerEntity.equippedWeaponId == -2) {
                g_playerPosScratch.x = (int)eff->posX;
                g_playerPosScratch.y = (int)eff->posY;
                g_playerPosScratch.z = (int)eff->posZ;
                Play3DSnd(1, 10, 0, (int)&g_playerPosScratch);
            }
        }
    } else {
        if (*(int*)&g_omodel_table[7] < (int)eff->posY + (int)AH_SHORT(eff, 0xe)) {
            AH_USHORT(eff, 10) |= 0x400b;
            eff->animId = 0x2f;
            eff->updateId = 0;
            eff->rotSpeedY = (short)((g_playerEntity.posY - eff->spriteOffsetY) - eff->localOffsetY);
            g_playerPosScratch.x = (int)eff->posX;
            g_playerPosScratch.y = (int)eff->posY;
            g_playerPosScratch.z = (int)eff->posZ;
            Effect_CreateBillboard(0x17, 8, eff->yaw, NULL, &g_playerPosScratch, 5);
        }
    }
}

// ============================================================================
// effect_copy_header_block - overwrite the slot's 24-byte animation header with
// the block at `src` (the inline copy of 0x0040d3b0 / 0x0040cf90, which does not
// preserve yaw/lightFactor - the callers restore them explicitly).
// ============================================================================
static void effect_copy_header_block(Effect* eff, const unsigned char* src)
{
    unsigned int* s = (unsigned int*)src;
    unsigned char* dst = (unsigned char*)&eff->animId;
    for (int i = 6; i != 0; i--) {
        unsigned int v = *s++;
        *dst++ = (unsigned char)v;
        *dst++ = (unsigned char)(v >> 8);
        *dst++ = (unsigned char)(v >> 0x10);
        *dst++ = (unsigned char)(v >> 0x18);
    }
}

// ============================================================================
// Behavior 11 (0x0040d3b0) - random next phase: half the time (bit 0 of
// g_RandSeed) the header is overwritten by the block right after the current
// one; the other half the block after THAT is used. Either way the slot ends
// two blocks ahead, with the old yaw folded in and the old phase byte restored.
// ============================================================================
static void effect_behavior_rand_phase(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    effect_behavior_set_frame();
    eff->animDataFrame += 0x18;

    unsigned char phase = eff->animHeader[3];
    short yaw = eff->yaw;
    unsigned char lightFactor = eff->lightFactor;

    if ((g_RandSeed & 1) == 0) {
        effect_copy_header_block(eff, (const unsigned char*)eff->animDataFrame);
        eff->animDataFrame += 0x18;
    } else {
        eff->animDataFrame += 0x18;
        effect_copy_header_block(eff, (const unsigned char*)eff->animDataFrame);
    }

    eff->yaw = (short)(eff->yaw + yaw);
    eff->animHeader[3] = phase;
    eff->lightFactor = lightFactor;
    memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
}

// ============================================================================
// Behavior 12 (0x0040cf00) - spawner: count animHeader[2] down, then spawn a
// new billboard of the same type at the slot's spawn position and enter the
// next phase.
// ============================================================================
static void effect_behavior_spawner(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[2] == 0) {
        g_playerPosScratch.x = (int)eff->localOffsetX;
        g_playerPosScratch.y = (int)eff->localOffsetY;
        g_playerPosScratch.z = (int)eff->localOffsetZ;
        Effect_CreateBillboard(eff->effectType, eff->depthGroup, eff->yaw,
                               (void*)eff->spriteInfo, &g_playerPosScratch, 0);
        effect_behavior_next_phase();
        return;
    }
    eff->animHeader[2]--;
}

// ============================================================================
// Behavior 13 (0x0040cca0) - count animHeader[2] down, then next phase.
// ============================================================================
static void effect_behavior_count_phase(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[2] == 0) {
        effect_behavior_next_phase();
        return;
    }
    eff->animHeader[2]--;
}

// ============================================================================
// Behavior 14 (0x0040ccd0) - blink: toggle the skip-render bit (0x80) of the
// header flags byte, hiding the sprite on alternating frames.
// ============================================================================
static void effect_behavior_blink(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    eff->animHeader[11] = (unsigned char)(eff->animHeader[11] ^ 0x80);
}

// ============================================================================
// Behavior 15 (0x0040ddc0) - floor splash: when the effect crosses y = 0,
// switch to the inert behavior, freeze it at the floor, spawn a new billboard
// (type/phase from the header via behavior 26) and merge the splash's light
// factor into it. The g_playerDisplacement-scratch effect slot is read via the
// global the spawn helper left behind.
// ============================================================================
static void effect_behavior_floor_splash(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        eff->animId = 0x2f;
        eff->updateId = 0;
        eff->rotSpeedY = (short)(-(eff->spriteOffsetY + eff->localOffsetY));
        effect_behavior_spawn_from_header();
        g_effectPool[g_playerDisplacement].lightFactor =
            (unsigned char)(g_effectPool[g_playerDisplacement].lightFactor
                            + (eff->lightFactor - 3));
    }
}

// ============================================================================
// Behavior 16 (0x0040e230) - projectile wobble: count animHeader[3] down while
// jittering the velocity header, spin and colour each frame; when the count
// expires switch to the fire behavior (0x11) in the inert-ish mode (type 2).
// ============================================================================
static void effect_behavior_wobble(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[3] != 0) {
        eff->animHeader[3]--;
        // random sign flips of the horizontal spin
        AH_USHORT(eff, 10) |= (unsigned short)(((g_RandSeed & 1) != 0) ? 0x80 : 0);
        AH_SHORT(eff, 0xc) = (short)((short)(g_RandSeed % 6) * -0x32 + 0x78)
                           * (short)(char)eff->animHeader[2];
        AH_SHORT(eff, 6) = (short)(-8 - (short)(g_RandSeed % 6));
        eff->lightFactor = (unsigned char)(eff->lightFactor - ((g_RandSeed & 1) ? 1 : 0));
        eff->yaw = (short)(((g_RandSeed % 6) * 0x3c000) / 0x168);
        eff->animHeader[0] = (unsigned char)(g_RandSeed % 6);
        effect_behavior_set_frame();
        return;
    }
    eff->animId = 0x11;
    eff->animHeader[3] = 0;
    eff->type = 2;
    memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
}

// ============================================================================
// Behavior 17 (0x0040e3f0) - bouncing debris: reflect the vertical velocity on
// ground contact, and while the vertical delta is above the phase count,
// re-randomize the horizontal spread, spin and bounce strength. The reflection
// sign-flip mirrors the C bounce in the original (the - of -(animHeader[4])).
// ============================================================================
static void effect_behavior_bounce_debris(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    if (0 < AH_SHORT(eff, 0xe)) {
        if (effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 0x3c) != 0) {
            AH_SHORT(eff, 4) = (short)-AH_SHORT(eff, 4);
        }
    }

    if (AH_SHORT(eff, 0xe) < (short)(char)eff->animHeader[3]) {
        AH_USHORT(eff, 0xc) = (unsigned short)((0x1e - (g_RandSeed & 3)) * (short)(char)eff->animHeader[2]);
        AH_USHORT(eff, 0xe) = (unsigned short)((0x23 - (g_RandSeed & 3)) * 2);
        eff->animHeader[3] = (unsigned char)(AH_SHORT(eff, 0xe) / -3);
        AH_SHORT(eff, 0x10) = (short)(AH_SHORT(eff, 0x10) + (short)(g_RandSeed % 5) * (short)(char)eff->animHeader[2]);
        AH_SHORT(eff, 4) = (short)((short)(g_RandSeed % 3) * (short)(char)eff->animHeader[2]);
        AH_USHORT(eff, 6) = (unsigned short)(-2 - (g_RandSeed & 1));
        eff->animHeader[2] = (unsigned char)(-eff->animHeader[2]);
    }
}

// ============================================================================
// Behavior 18 (0x0040cf90) - clone: count animHeader[0] down; on zero, copy the
// whole slot into a free one (or fall back to the inert behavior when the pool
// is full), advance the clone one animation block, and retire the original.
// ============================================================================
static void effect_behavior_clone(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[0] == 0) {
        if (g_freeEffectSlots == 0) {
            eff->animId = 1;
            return;
        }
        // Scan 63..0 for a free slot; the loop ends the moment the free count
        // changes (the first free slot found, which then gets decremented).
        unsigned char prevFree = g_freeEffectSlots;
        unsigned char slot = 64;
        do {
            slot--;
            if (g_effectPool[slot].animId == 0) {
                g_freeEffectSlots--;
            }
        } while (g_freeEffectSlots == prevFree);

        Effect* dst = &g_effectPool[slot];
        dst->rotSpeedX = 0; dst->rotSpeedY = 0; dst->rotSpeedZ = 0;
        dst->posX = 0; dst->posY = 0; dst->posZ = 0;
        dst->transform[0] = 0; dst->transform[1] = 0; dst->transform[2] = 0;
        dst->transform[3] = 0; dst->transform[4] = 0; dst->transform[5] = 0;
        dst->transform[6] = 0; dst->transform[7] = 0; dst->transform[8] = 0;
        dst->spriteOffsetX = 0; dst->spriteOffsetY = 0; dst->spriteOffsetZ = 0;
        dst->depthScaled = 0; dst->projDepth = 0;
        dst->effectType = eff->effectType;
        dst->depthGroup = eff->depthGroup;
        dst->spriteInfo = eff->spriteInfo;
        dst->spawnPosX = eff->spawnPosX;
        dst->spawnPosY = eff->spawnPosY;
        dst->spawnPosZ = eff->spawnPosZ;
        dst->spawnPosW = eff->spawnPosW;
        dst->clutInfo = eff->clutInfo;
        dst->vramInfo = eff->vramInfo;
        dst->vramInfoBackup = eff->vramInfo;
        dst->uvData = eff->uvData;
        dst->uvDataBackup = eff->uvData;
        dst->animDataBase = eff->animDataBase;
        dst->animDataFrame = eff->animDataBase;
        dst->localOffsetX = (short)eff->spawnPosX;
        dst->localOffsetY = (short)eff->spawnPosY;
        dst->localOffsetZ = (short)eff->spawnPosZ;
        dst->animDataBase = eff->animDataBase;
        dst->animDataFrame = eff->animDataFrame;
        dst->animDataFrame += 0x18;
        effect_copy_header_block(dst, (const unsigned char*)dst->animDataFrame);
        eff->animId = 1;
        return;
    }
    eff->animHeader[0]--;
}

// ============================================================================
// Behavior 19 (0x0040de70) - fall and stop: when the effect crosses y = 0,
// switch to the inert behavior and zero the velocity header.
// ============================================================================
static void effect_behavior_fall_stop(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        eff->animId = 0x2f;
        eff->animHeader[10] = 7;
        eff->animHeader[11] = 0x40;
        eff->animHeader[8] = 0;
        eff->animHeader[9] = 0;
        AH_SHORT(eff, 6) = AH_SHORT(eff, 8);
        AH_SHORT(eff, 4) = AH_SHORT(eff, 6);
        eff->animHeader[0x10] = 0;
        eff->animHeader[0x11] = 0;
        AH_SHORT(eff, 0xe) = AH_SHORT(eff, 0x10);
        AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);
        eff->rotSpeedY = (short)(-(eff->spriteOffsetY + eff->localOffsetY));
    }
}

// ============================================================================
// Behavior 20 (0x0040ccf0) - next phase with a downward lurch
// (animHeader[0xe] -= 0x18).
// ============================================================================
static void effect_behavior_phase_drop(void)
{
    effect_behavior_next_phase();
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    AH_SHORT(eff, 0xe) = (short)(AH_SHORT(eff, 0xe) - 0x18);
}

// ============================================================================
// Behavior 21 (0x0040e5f0) - fading fall: count animHeader[0] down, then switch
// to the drop-splat behavior (0x16) and run it this frame.
// ============================================================================
static void effect_behavior_fade_fall(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[0] == 0) {
        eff->animId = 0x16;
        eff->animHeader[0xe] = 0;
        eff->animHeader[0xf] = 0;
        AH_SHORT(eff, 6) = (short)eff->animHeader[2];
    }
    effect_behavior_drop_splat();
}

// ============================================================================
// Behavior 22 (0x0040e670) - drop and splat: probe straight down; on ground
// contact splat (behavior 23's helper) and then test the splash against the
// player, damaging them when it lands nearby with a clear line of sight.
// ============================================================================
static void effect_behavior_drop_splat(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    if (effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4) != 0) {
        effect_ground_splat();
    }

    if (effect_projectile_hit_check(600, eff->posX, eff->posZ) != 0) {
        effect_ground_splat();
    }
}

// ============================================================================
// Behavior 23 (0x0040e720) - ground contact splat: when the effect crosses
// y = 0, splat and mark the sprite ground-facing.
// ============================================================================
static void effect_behavior_ground_splat(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        effect_ground_splat();
        AH_USHORT(eff, 10) |= 0x4007;
    }
}

// ============================================================================
// Behavior 24 (0x0040e940) - jitter: add a random 0-4 delta to each velocity
// component whose bit is set in animHeader[3].
// ============================================================================
static void effect_behavior_jitter(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if ((eff->animHeader[3] & 1) != 0) {
        AH_SHORT(eff, 0xc) = (short)(AH_SHORT(eff, 0xc) + (short)(g_RandSeed % 5));
    }
    if ((eff->animHeader[3] & 2) != 0) {
        AH_SHORT(eff, 0xe) = (short)(AH_SHORT(eff, 0xe) + (short)(g_RandSeed % 5));
    }
    if ((eff->animHeader[3] & 4) != 0) {
        AH_SHORT(eff, 0x10) = (short)(AH_SHORT(eff, 0x10) + (short)(g_RandSeed % 5));
    }
    if ((eff->animHeader[3] & 8) != 0) {
        AH_SHORT(eff, 4) = (short)(AH_SHORT(eff, 4) + (short)(g_RandSeed % 5));
    }
    if ((eff->animHeader[3] & 0x10) != 0) {
        AH_SHORT(eff, 6) = (short)(AH_SHORT(eff, 6) + (short)(g_RandSeed % 5));
    }
    if ((eff->animHeader[3] & 0x20) != 0) {
        AH_SHORT(eff, 8) = (short)(AH_SHORT(eff, 8) + (short)(g_RandSeed % 5));
    }
}

// ============================================================================
// Behavior 25 (0x0040ebd0) - drift: move the local offset along the header's
// (signed) direction bytes, doubled, then next phase.
// ============================================================================
static void effect_behavior_drift(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->localOffsetX = (short)(eff->localOffsetX + (char)eff->animHeader[0] * 2);
    eff->localOffsetY = (short)(eff->localOffsetY + (char)eff->animHeader[1] * 2);
    eff->localOffsetZ = (short)(eff->localOffsetZ + (char)eff->animHeader[2] * 2);
    effect_behavior_next_phase();
}

// ============================================================================
// Behavior 26 (0x0040d270) - spawn from header: create a billboard whose type
// and depth group come from the header, at the slot's world position, and leave
// its slot index in g_playerDisplacement (read by behavior 15).
// ============================================================================
static void effect_behavior_spawn_from_header(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    unsigned char slot = Effect_CreateBillboard(eff->animHeader[0], eff->animHeader[1],
                                                eff->yaw, NULL, &g_playerPosScratch, 0);
    g_playerDisplacement = (int)(char)slot;
}

// ============================================================================
// Behavior 27 (0x0040cd10) - next vram frame entry: vramInfoBackup += 4 with no
// delay, so Effect_AnimateSprite steps to it next frame.
// ============================================================================
static void effect_behavior_next_frame(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    eff->vramInfoBackup += 4;
    eff->frameDelay = 0;
}

// ============================================================================
// Behavior 29 (0x0040cdd0) - random vram frame: jump 0-2 frame entries forward
// from the current one.
// ============================================================================
static void effect_behavior_rand_frame(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->vramInfoBackup += (g_RandSeed % 3) * 4;
    eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
    eff->uvDataBackup = (unsigned int)*(unsigned char*)eff->vramInfoBackup * 4 + eff->uvData;
}

// ============================================================================
// Behavior 30 (0x0040d2e0) - spawn and advance: spawn a billboard from the
// header (behavior 26), then next phase.
// ============================================================================
static void effect_behavior_spawn_advance(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    Effect_CreateBillboard(eff->animHeader[0], eff->animHeader[1],
                           eff->yaw, NULL, &g_playerPosScratch, 0);
    effect_behavior_next_phase();
}

// ============================================================================
// Behavior 47 / effect_behavior_kill (0x0040f7b0) - free the slot immediately:
// bump the free counter and clear updateId then animId (animId 0 = free, the
// same two bytes FUN_0047cf80 and Effect_AnimateSprite clear).
// ============================================================================
static void effect_behavior_kill(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    g_freeEffectSlots++;
    eff->updateId = 0;
    eff->animId = 0;
}

// ============================================================================
// effect_shot_impact (0x0040f850) - the impact visuals when a bullet lands
// (`hit` != 0) or hits the player (`hit` == 0). The weapon byte (animHeader[1],
// set by the spawners from the equipped weapon id) selects the pattern:
//   7 = pistol-style (0xc sound, three embers + smoke)
//   8 = shotgun-style (0xd sound, three pellets + smoke)
//   9 = magnum-style (0xe sound, four embers, no smoke)
// anything else = just the kill. The smoke billboard (type 9) is patched into
// its resting pose when it spawns.
// ============================================================================
static void effect_shot_impact(int hit)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;

    switch (eff->animHeader[1]) {
    case 7:
        Play3DSnd(1, 0xc, 4, (int)&g_playerPosScratch);
        Play3DSnd(1, 0xc, 0, (int)&g_playerPosScratch);
        if (hit != 0) {
            g_playerPosScratch.y -= 0x96;
            Effect_CreateBillboard(0xe, 3, eff->yaw, NULL, &g_playerPosScratch, 0x19);
            g_playerPosScratch.y += 0x96;
            g_playerPosScratch.x -= 0xb4;
            g_playerPosScratch.z += 0xb4;
            Effect_CreateBillboard(0xe, 3, eff->yaw, NULL, &g_playerPosScratch, 0x1e);
            g_playerPosScratch.y += 0x96;
            g_playerPosScratch.x += 0x154;
            g_playerPosScratch.z -= 0x168;
            Effect_CreateBillboard(0xe, 3, eff->yaw, NULL, &g_playerPosScratch, 0x1c);

            g_playerPosScratch.x = (int)eff->posX;
            g_playerPosScratch.z = (int)eff->posZ;
            g_playerPosScratch.y = (int)eff->posY - 200;
            unsigned char smoke = Effect_CreateBillboard(9, 0xd, eff->yaw, NULL,
                                                         &g_playerPosScratch, 0x14);
            if (smoke != 0xFF) {
                Effect* s = &g_effectPool[smoke];
                s->animId = 1;
                s->updateId = 0;
                s->animHeader[10] = 3;
                s->animHeader[11] = 0x40;
                s->animHeader[6] = 0xff;
                s->animHeader[7] = 0xff;
                s->animHeader[0xe] = 0;
                s->animHeader[0xf] = 0;
                s->animHeader[0xc] = 0;
                s->animHeader[0xd] = 0;
                s->animHeader[4] = 0;
                s->animHeader[5] = 0;
                effect_behavior_kill();
                return;
            }
        }
        break;
    case 8:
        Play3DSnd(1, 0xd, 4, (int)&g_playerPosScratch);
        Play3DSnd(1, 0xd, 0, (int)&g_playerPosScratch);
        if (hit != 0) {
            Effect_CreateBillboard(9, 0, eff->yaw, NULL, &g_playerPosScratch, 0x10);
            Effect_CreateBillboard(9, 0, (short)(eff->yaw + 0x555), NULL,
                                   &g_playerPosScratch, 0x10);
            Effect_CreateBillboard(9, 0, (short)(eff->yaw + 0xaaa), NULL,
                                   &g_playerPosScratch, 0x10);
            unsigned char smoke = Effect_CreateBillboard(9, 0, eff->yaw, NULL,
                                                         &g_playerPosScratch, 0x14);
            if (smoke != 0xFF) {
                Effect* s = &g_effectPool[smoke];
                s->animHeader[4] = 0;
                s->animHeader[5] = 0;
                s->animHeader[0xc] = 0;
                s->animHeader[0xd] = 0;
                s->animHeader[0xe] = 0xce;
                s->animHeader[0xf] = 0xff;
                effect_behavior_kill();
                return;
            }
        }
        break;
    case 9:
        Play3DSnd(1, 0xe, 4, (int)&g_playerPosScratch);
        Play3DSnd(1, 0xe, 0, (int)&g_playerPosScratch);
        if (hit != 0) {
            Effect_CreateBillboard(0xe, 5, eff->yaw, NULL, &g_playerPosScratch, 0x1b);
            g_playerPosScratch.x -= 0xb4;
            Effect_CreateBillboard(0xe, 4, eff->yaw, NULL, &g_playerPosScratch, 0x17);
            g_playerPosScratch.x += 0x140;
            g_playerPosScratch.z += 0x8c;
            Effect_CreateBillboard(0xe, 4, eff->yaw, NULL, &g_playerPosScratch, 0x16);
            g_playerPosScratch.z -= 0x118;
            Effect_CreateBillboard(0xe, 4, eff->yaw, NULL, &g_playerPosScratch, 0x15);
        }
        break;
    default:
        effect_behavior_kill();
        return;
    }

    effect_behavior_kill();
}

// ============================================================================
// effect_rocket_explode (0x0040feb0) - the rocket's impact: explosion sound,
// two blast billboards, a smoke column (patched into its resting pose), and a
// FUN_0047cf80 sweep freeing the effect family (mask 7: same type, depth
// group and header[2]).
// ============================================================================
static void effect_rocket_explode(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;

    Play3DSnd(1, 0xf, 4, (int)&g_playerPosScratch);
    Play3DSnd(1, 0xf, 0, (int)&g_playerPosScratch);
    Effect_CreateBillboard(0xe, 3, eff->yaw, NULL, &g_playerPosScratch, 0x1c);
    Effect_CreateBillboard(0xe, 3, eff->yaw, NULL, &g_playerPosScratch, 0x19);

    g_playerPosScratch.y -= 200;
    unsigned char smoke = Effect_CreateBillboard(9, 0xd, eff->yaw, NULL,
                                                 &g_playerPosScratch, 0x13);
    if (smoke == 0xFF) {
        FUN_0047cf80(7, (unsigned int)eff->effectType, (unsigned int)eff->depthGroup,
                     (unsigned int)eff->animHeader[2], NULL);
        return;
    }
    Effect* s = &g_effectPool[smoke];
    s->animId = 1;
    s->updateId = 0;
    s->animHeader[10] = 3;
    s->animHeader[11] = 0x40;
    s->animHeader[0xe] = 0;
    s->animHeader[0xf] = 0;
    s->animHeader[0xc] = 0;
    s->animHeader[0xd] = 0;
    s->animHeader[4] = 0;
    s->animHeader[5] = 0;

    FUN_0047cf80(7, (unsigned int)eff->effectType, (unsigned int)eff->depthGroup,
                 (unsigned int)eff->animHeader[2], NULL);
}

// ============================================================================
// Behavior 32 (0x0040dfb0) - floor contact: when posY + vy crosses 0, freeze
// the velocities, step the sprite frame and enter the next phase.
// ============================================================================
static void effect_behavior_floor_phase(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        eff->animHeader[8] = 0;
        eff->animHeader[9] = 0;
        AH_SHORT(eff, 6) = AH_SHORT(eff, 8);
        AH_SHORT(eff, 4) = AH_SHORT(eff, 6);
        eff->animHeader[0x10] = 0;
        eff->animHeader[0x11] = 0;
        AH_SHORT(eff, 0xe) = AH_SHORT(eff, 0x10);
        AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);
        eff->rotSpeedY = (short)(-(eff->spriteOffsetY + eff->localOffsetY));
        effect_behavior_set_frame();
        effect_behavior_next_phase();
    }
}

// ============================================================================
// Behavior 33 (0x0040ec50) - step the sprite frame, then next phase.
// ============================================================================
static void effect_behavior_frame_phase(void)
{
    effect_behavior_set_frame();
    effect_behavior_next_phase();
}

// ============================================================================
// Behavior 34 (0x0040ec60) - burning ground fire: probe straight down with the
// player's flags staged from the header; on contact (twice-probed, the second
// accepting the 1 flag only) spawn the header billboard and kill the fire.
// ============================================================================
static void effect_behavior_burn(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    unsigned char savedFlags = g_playerEntity.flags;

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    g_playerEntity.flags = eff->animHeader[2];

    unsigned short r = effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4);
    if ((r != 0) && (eff->animHeader[2] == 0x21)) {
        effect_behavior_spawn_from_header();
        effect_behavior_kill();
        g_playerEntity.flags = savedFlags;
        return;
    }

    r = effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4);
    if (r == 1) {
        effect_behavior_spawn_from_header();
        effect_behavior_kill();
    }
    g_playerEntity.flags = savedFlags;
}

// ============================================================================
// Behavior 35 (0x0040eda0) - the fire wobble: like behavior 16 with smaller
// random spreads; when the count expires switch to the fire behavior (0x11).
// ============================================================================
static void effect_behavior_fire_wobble(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[3] != 0) {
        eff->animHeader[3]--;
        AH_USHORT(eff, 10) |= (unsigned short)(((g_RandSeed & 1) != 0) ? 0x80 : 0);
        AH_SHORT(eff, 0xc) = (short)((short)(g_RandSeed % 6) * -0x19 + 0x3c)
                           * (short)(char)eff->animHeader[2];
        AH_SHORT(eff, 6) = (short)(-10 - (short)(g_RandSeed % 6));
        eff->lightFactor = (unsigned char)(eff->lightFactor - ((g_RandSeed & 1) ? 1 : 0));
        eff->yaw = (short)(((g_RandSeed % 6) * 0x3c000) / 0x168);
        eff->animHeader[0] = (unsigned char)(g_RandSeed % 6);
        effect_behavior_set_frame();
        return;
    }
    eff->animId = 0x11;
    eff->animHeader[3] = 0;
    eff->type = 2;
    memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
}

// ============================================================================
// Behavior 36 (0x0040ef60) - fire bounce debris: like behavior 17 with tighter
// spreads (10-based instead of 0x1e/0x23-based).
// ============================================================================
static void effect_behavior_fire_bounce(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    if (0 < AH_SHORT(eff, 0xe)) {
        if (effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 0x3c) != 0) {
            AH_SHORT(eff, 4) = (short)-AH_SHORT(eff, 4);
        }
    }

    if (AH_SHORT(eff, 0xe) < (short)(char)eff->animHeader[3]) {
        AH_USHORT(eff, 0xc) = (unsigned short)((10 - (g_RandSeed & 3)) * (short)(char)eff->animHeader[2]);
        AH_USHORT(eff, 0xe) = (unsigned short)((0x23 - (g_RandSeed & 3)) * 2);
        eff->animHeader[3] = (unsigned char)(AH_SHORT(eff, 0xe) / -3);
        AH_SHORT(eff, 0x10) = (short)(AH_SHORT(eff, 0x10) + (short)(g_RandSeed % 5) * (short)(char)eff->animHeader[2]);
        AH_SHORT(eff, 4) = (short)((short)(g_RandSeed % 3) * (short)(char)eff->animHeader[2]);
        AH_USHORT(eff, 6) = (unsigned short)(-10 - (g_RandSeed & 1));
        eff->animHeader[2] = (unsigned char)(-eff->animHeader[2]);
    }
}

// ============================================================================
// Behavior 37 (0x0040ed30) - the FLAMETHROWER: stage the player's flags from
// the header and apply weapon 6's damage at the flame's tip; on a hit spawn
// the header billboard and kill the flame.
// ============================================================================
static void effect_behavior_flamethrower(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    unsigned char savedFlags = g_playerEntity.flags;

    g_playerEntity.flags = eff->animHeader[2];
    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.z = (int)eff->posZ;

    if (apply_weapon_damage(6) != 0) {
        effect_behavior_spawn_from_header();
        effect_behavior_kill();
    }
    g_playerEntity.flags = savedFlags;
}

// ============================================================================
// Behavior 38 (0x0040d4d0) - tint selector: switch to the inert mode (type 2)
// and pick the sprite light factor from the header byte via a 10-entry table.
// ============================================================================
static void effect_behavior_set_tint(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    static const unsigned char tintTable[10] = { 7, 0xd, 8, 8, 0xf, 0xf, 0xf, 0x11, 0x11, 0x11 };

    eff->updateId = 1;
    eff->type = 2;
    memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
    eff->lightFactor = tintTable[eff->animHeader[0]];
}

// ============================================================================
// Behavior 39 (0x0040f160) - paired timers: count animHeader[3] down and kill
// at zero; count animHeader[2] down and re-spawn the header billboard every 4
// ticks while it lasts.
// ============================================================================
static void effect_behavior_pair_timer(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->animHeader[2]--;
    eff->animHeader[3]--;

    if (eff->animHeader[3] == 0) {
        effect_behavior_kill();
        return;
    }
    if (eff->animHeader[2] == 0) {
        eff->animHeader[2] = 4;
        Effect_CreateBillboard(eff->animHeader[0], eff->animHeader[1], eff->yaw,
                               (void*)eff->spriteInfo, &eff->spawnPosX, eff->lightFactor);
    }
}

// ============================================================================
// Behavior 40 (0x0040d590) - phase timer that latches the player's flags into
// the header (the flag-staging variant of behavior 2).
// ============================================================================
static void effect_behavior_timer_flags(void)
{
    effect_behavior_timer_refresh();
    g_effectPool[g_activeEffectIndex].animHeader[2] = g_playerEntity.flags;
}

// ============================================================================
// Behavior 41 (0x0040ea80) - random spin direction: randomize the header's
// flip/spin bits, then jump to the behavior named by the header byte.
// ============================================================================
static void effect_behavior_rand_anim(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    AH_USHORT(eff, 10) |= (unsigned short)(g_RandSeed % 3) << 6;
    eff->animId = eff->animHeader[2];
}

// ============================================================================
// Behavior 42 (0x0040f210) - weapon charge: capture the player's flags and
// weapon id, then the inert pose; phase 2 spawns the charge-up ring (type 8).
// ============================================================================
static void effect_behavior_charge(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    switch (eff->animHeader[3]) {
    case 0:
        eff->animHeader[0] = g_playerEntity.flags;
        eff->animHeader[1] = g_playerEntity.equippedWeaponId;
        eff->animHeader[3]++;
        return;
    case 1:
        eff->type = 2;
        memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
        eff->animHeader[3]++;
        return;
    case 2:
        g_playerPosScratch.x = (int)eff->posX;
        g_playerPosScratch.y = (int)eff->posY;
        g_playerPosScratch.z = (int)eff->posZ;
        Effect_CreateBillboard(8, 1, eff->yaw, NULL, &g_playerPosScratch, 0);
        return;
    default:
        return;
    }
}

// ============================================================================
// Behavior 43 (0x0040f320) - the BULLET: elevates the player's aim flags by
// comparing the bullet's height to the player's, probes for the ground (and
// splats when hit), and applies the weapon damage named by the header byte
// when the bullet reaches the player's level; the impact visuals come from
// effect_shot_impact. `hit` = 1 for a ground landing, 0 for a player hit.
// ============================================================================
static void effect_behavior_bullet(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    unsigned char savedFlags = g_playerEntity.flags;

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;

    int playerY = (int)g_playerEntity.posY;
    if (playerY < (int)AH_SHORT(eff, 0xe) + (int)g_playerPosScratch.y) {
        effect_shot_impact(1);
        return;
    }

    // aim elevation: below the player's waist -> up (0x20), between waist and
    // head -> level (0x40), above the head -> down (0x80)
    g_playerEntity.flags &= 0x1f;
    int groundY = (int)AH_SHORT(eff, 0xe) + (int)eff->posY;
    if (playerY - 0xaf0 < groundY) {
        if (playerY - 500 < groundY) {
            g_playerEntity.flags |= 0x20;
        } else {
            g_playerEntity.flags |= 0x40;
        }
    } else {
        g_playerEntity.flags |= 0x80;
    }

    unsigned short r = effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4);
    if (r == 1) {
        g_playerEntity.flags = savedFlags;
        effect_shot_impact(1);
        return;
    }

    unsigned char hit = apply_weapon_damage((unsigned int)eff->animHeader[1]);
    if (hit != 0) {
        g_playerEntity.flags = savedFlags;
        effect_shot_impact(0);
        return;
    }

    unsigned char* counter = &eff->animHeader[2];
    unsigned char c = *counter;
    g_playerEntity.flags = savedFlags;
    *counter = (unsigned char)(c + 1);
    if (8 < c) {
        eff->vramInfoBackup = eff->vramInfo;
    }
}

// ============================================================================
// Behavior 44 (0x0040f490) - fire burst: spawns a ladder of type-1 billboards
// at rising depth groups (1..4), then kills itself.
// ============================================================================
static void effect_behavior_fire_burst(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    g_playerPosScratch.x = (int)eff->localOffsetX;
    g_playerPosScratch.y = (int)eff->localOffsetY;
    g_playerPosScratch.z = (int)eff->localOffsetZ;

    switch (eff->animHeader[0]) {
    case 0:
        Effect_CreateBillboard(1, (unsigned char)(eff->depthGroup + 1), eff->yaw,
                               (void*)eff->spriteInfo, &g_playerPosScratch, 0);
        eff->animHeader[0] = 1;
        return;
    case 1:
        Effect_CreateBillboard(1, (unsigned char)(eff->depthGroup + 2), eff->yaw,
                               (void*)eff->spriteInfo, &g_playerPosScratch, 0);
        Effect_CreateBillboard(1, (unsigned char)(eff->depthGroup + 3), eff->yaw,
                               (void*)eff->spriteInfo, &g_playerPosScratch, 0);
        eff->animHeader[0] = 2;
        return;
    case 2:
        Effect_CreateBillboard(1, (unsigned char)(eff->depthGroup + 4), eff->yaw,
                               (void*)eff->spriteInfo, &g_playerPosScratch, 0);
        effect_behavior_kill();
        return;
    default:
        return;
    }
}

// ============================================================================
// Behavior 45 (0x0040f5d0) - the ROCKET LAUNCHER: spawns a tracer from the
// header, culls the rocket when it leaves the room (negative coordinates),
// probes the ground, and applies weapon 10's damage at the rocket tip; the
// explosion visuals come from effect_rocket_explode.
// ============================================================================
static void effect_behavior_rocket(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    effect_behavior_spawn_from_header();

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_playerPosScratch.x = (int)eff->posX;
    g_playerPosScratch.y = (int)eff->posY;
    g_playerPosScratch.z = (int)eff->posZ;

    if (((unsigned int)g_playerPosScratch.x & 0x80000000u) != 0
        || ((unsigned int)g_playerPosScratch.z & 0x80000000u) != 0) {
        // left the room - sweep the family away (mask 7: type/depth/header[2])
        FUN_0047cf80(7, (unsigned int)eff->effectType, (unsigned int)eff->depthGroup,
                     (unsigned int)eff->animHeader[2], NULL);
    }

    unsigned short r = effect_probe_ground((SVECTOR*)&g_playerPosScratch, &g_svecScratch, 4);
    unsigned char savedFlags = g_playerEntity.flags;
    if (r == 1) {
        g_playerPosScratch.x = (int)eff->posX;
        g_playerPosScratch.y = (int)eff->posY;
        g_playerPosScratch.z = (int)eff->posZ;
        effect_rocket_explode();
        return;
    }

    g_playerEntity.flags = (g_playerEntity.flags & 0x1f) | 0x40;
    unsigned char hit = apply_weapon_damage(10);
    g_playerEntity.flags = savedFlags;
    if (hit != 0) {
        g_playerPosScratch.x = (int)eff->posX;
        g_playerPosScratch.y = (int)eff->posY;
        g_playerPosScratch.z = (int)eff->posZ;
        Play3DSnd(1, 0xf, 4, (int)&g_playerPosScratch);
        Play3DSnd(1, 0xf, 0, (int)&g_playerPosScratch);
        FUN_0047cf80(7, (unsigned int)eff->effectType, (unsigned int)eff->depthGroup,
                     (unsigned int)eff->animHeader[2], NULL);
        return;
    }
}

// ============================================================================
// Behavior 46 (0x0040f780) - count animHeader[2] down, then kill.
// ============================================================================
static void effect_behavior_countdown_kill(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[2] == 0) {
        effect_behavior_kill();
        return;
    }
    eff->animHeader[2]--;
}

// ============================================================================
// Behavior 48 (0x0040fcc0) - fade: advance the sprite every frame and decay
// the light factor by 2 per tick.
// ============================================================================
static void effect_behavior_fade(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->frameDelay = 0;
    if (eff->lightFactor != 0) {
        eff->lightFactor = (unsigned char)(eff->lightFactor - 2);
    }
}

// ============================================================================
// Behavior 49 (0x0040f800) - flash marker: pin the header flags to the
// (6, 0x80) pose - the odd short re-check reverts to (6, 0) every frame, which
// is what the original does - and count animHeader[1] down to the kill.
// ============================================================================
static void effect_behavior_flash_marker(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->animHeader[10] = 6;
    eff->animHeader[11] = 0x80;
    if (AH_SHORT(eff, 10) != 6) {
        eff->animHeader[10] = 6;
        eff->animHeader[11] = 0;
    }

    if (eff->animHeader[1] == 0) {
        effect_behavior_kill();
        return;
    }
    eff->animHeader[1]--;
}

// ============================================================================
// Behavior 50 (0x0040e0d0) - floor stop (inert variant): when posY + vy
// crosses 0, switch to behavior 0x2f (the inert pose with flags 6) and freeze
// the velocities.
// ============================================================================
static void effect_behavior_floor_stop(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        eff->animId = 0x2f;
        eff->updateId = 0;
        eff->animHeader[10] = 6;
        eff->animHeader[11] = 0;
        eff->animHeader[8] = 0;
        eff->animHeader[9] = 0;
        AH_SHORT(eff, 6) = AH_SHORT(eff, 8);
        AH_SHORT(eff, 4) = AH_SHORT(eff, 6);
        eff->animHeader[0x10] = 0;
        eff->animHeader[0x11] = 0;
        AH_SHORT(eff, 0xe) = AH_SHORT(eff, 0x10);
        AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);
        eff->rotSpeedY = (short)(-(eff->spriteOffsetY + eff->localOffsetY));
    }
}

// ============================================================================
// Behavior 51 (0x0040fc90) - system-flag gate: when SCD system flag 0 is set,
// switch to the inert behavior and snap to the header's sprite frame.
// ============================================================================
static void effect_behavior_sysflag(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (Flg_ck((int)g_SysFlags, 0) != 0) {
        eff->animId = 1;
        effect_behavior_set_frame();
    }
}

// ============================================================================
// Behavior 52 (0x0040fd00) - settle: adopt the inert pose and hand over to
// behavior 5 (gravity settle) immediately.
// ============================================================================
static void effect_behavior_to_settle(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
    eff->type = 2;
    eff->animId = 5;
    effect_behavior_gravity_settle();
}

// ============================================================================
// Behavior 53 (0x0040fd70) - floor stop (inert variant, flags untouched): when
// posY + vy crosses 0, switch to behavior 1 and freeze the velocities.
// ============================================================================
static void effect_behavior_floor_stop1(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->posY + AH_SHORT(eff, 0xe) >= 0) {
        eff->animId = 1;
        eff->updateId = 0;
        eff->animHeader[8] = 0;
        eff->animHeader[9] = 0;
        AH_SHORT(eff, 6) = AH_SHORT(eff, 8);
        AH_SHORT(eff, 4) = AH_SHORT(eff, 6);
        eff->animHeader[0x10] = 0;
        eff->animHeader[0x11] = 0;
        AH_SHORT(eff, 0xe) = AH_SHORT(eff, 0x10);
        AH_SHORT(eff, 0xc) = AH_SHORT(eff, 0xe);
        eff->rotSpeedY = (short)(-(eff->spriteOffsetY + eff->localOffsetY));
    }
}

// ============================================================================
// Behavior 54 (0x0040ead0) - random spin: give each rotation axis a random
// speed of +-5 or +-15 (the even/odd bits of two draws).
// ============================================================================
static void effect_behavior_random_spin(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    // magnitude: even draw -> 15, odd -> 5; sign: even draw -> -1, odd -> +1
    int mag = ((g_RandSeed & 1) == 0) ? 15 : 5;
    int dir = ((g_RandSeed & 1) == 0) ? -1 : 1;
    eff->rotSpeedX = (short)(mag * dir);

    mag = ((g_RandSeed & 1) == 0) ? 15 : 5;
    dir = ((g_RandSeed & 1) == 0) ? -1 : 1;
    eff->rotSpeedY = (short)(mag * dir);

    mag = ((g_RandSeed & 1) == 0) ? 15 : 5;
    dir = ((g_RandSeed & 1) == 0) ? -1 : 1;
    eff->rotSpeedZ = (short)(mag * dir);
}

// ============================================================================
// Behavior 55 (0x00410060) - splash timer: every 4 ticks step to the splash
// frame; when the splash crosses the floor or reaches the player (range 900,
// only while the player's animation is the idle run), splat.
// ============================================================================
static void effect_behavior_splash_timer(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    eff->animHeader[0]--;
    if (eff->animHeader[0] == 0) {
        eff->animHeader[0] = 4;
        eff->vramInfoBackup = eff->vramInfo + 0x1c;
        eff->frameIndex = *(unsigned char*)eff->vramInfoBackup;
        eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
        eff->uvDataBackup = (unsigned int)eff->frameIndex * 4 + eff->uvData;
    }

    int y = (int)AH_SHORT(eff, 0xe) + (int)eff->posY;
    if (-0x961 < y) {
        if (-1 < y) {
            effect_ground_splat();
            AH_USHORT(eff, 10) |= 0x4007;
        }
        if (g_playerEntity.animationId == 1) {
            if (effect_projectile_hit_check(900, eff->posX, eff->posZ) != 0) {
                effect_ground_splat();
                return;
            }
        }
    }
}

// ============================================================================
// Behavior 56 (0x004101d0) - the fire/ember lifecycle: a 4-phase state machine
// (grow -> steady -> random re-ignition -> decay) driven by animHeader[0], with
// the sprite frame stepped to the vram+0x14 entry at each transition.
// ============================================================================
static void effect_behavior_fire_phases(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    switch (eff->animHeader[0]) {
    case 0:
        eff->vramInfoBackup = eff->vramInfo + 0x14;
        eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
        eff->uvDataBackup = (unsigned int)*(unsigned char*)eff->vramInfoBackup * 4 + eff->uvData;
        eff->animHeader[1] = 0xb;
        eff->animHeader[0] = 1;
        // fall through
    case 1:
        eff->animHeader[10] = 3;
        eff->animHeader[11] = 0x40;
        eff->lightFactor = eff->animHeader[2];
        eff->animHeader[1]--;
        if (eff->animHeader[1] == 0) {
            eff->animHeader[0] = 2;
            eff->animHeader[10] = 0;
            eff->animHeader[11] = 0;
            eff->lightFactor = 0;
        }
        break;
    case 2:
        eff->vramInfoBackup = eff->vramInfo + 0x14;
        eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
        eff->uvDataBackup = (unsigned int)*(unsigned char*)eff->vramInfoBackup * 4 + eff->uvData;
        eff->animHeader[1] = (unsigned char)(((char)(g_RandSeed % 5) + 10) * 10);
        eff->animHeader[0] = 3;
        eff->animHeader[10] = 0;
        eff->animHeader[11] = 0;
        eff->lightFactor = 0;
        return;
    case 3:
        eff->animHeader[10] = 0;
        eff->animHeader[11] = 0;
        eff->lightFactor = 0;
        eff->animHeader[1]--;
        if (eff->animHeader[1] == 0) {
            eff->animHeader[0] = 0;
            eff->animHeader[1] = 0xb;
        }
        break;
    default:
        break;
    }
}

// ============================================================================
// Behavior 57 (0x004104c0) - the muzzle-flash lifecycle: a 4-phase state
// machine (arm -> flash pose -> flicker countdown -> done).
// ============================================================================
static void effect_behavior_muzzle_phases(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    switch (eff->animHeader[0]) {
    case 0:
        eff->animHeader[1] = 1;
        eff->animHeader[0] = 1;
        return;
    case 1:
        memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
        eff->type = 2;
        eff->animHeader[1] = 2;
        eff->animHeader[0] = 2;
        return;
    case 2:
        eff->animHeader[1]++;
        if (eff->animHeader[1] == 9) {
            eff->animHeader[2]--;
            eff->animHeader[1] = 4;
            eff->vramInfoBackup = eff->vramInfo + 0x10;
            eff->frameDelay = *(unsigned char*)(eff->vramInfoBackup + 1);
            eff->uvDataBackup = (unsigned int)*(unsigned char*)eff->vramInfoBackup * 4 + eff->uvData;
        }
        if (eff->animHeader[2] == 0) {
            eff->animHeader[0] = 3;
        }
        return;
    default:
        return;
    }
}

// ============================================================================
// Behavior 58 (0x00410680) - auto-aim flash: the phase timer of behavior 2,
// with the auto-aim state (2 bits) latched into the header.
// ============================================================================
static void effect_behavior_autoaim_flash(void)
{
    effect_behavior_timer_refresh();
    g_effectPool[g_activeEffectIndex].animHeader[2] = weapon_autoaim_check() & 3;
}

// ============================================================================
// Behavior 59 (0x004106b0) - dual shot: count animHeader[0] down, then fire
// one or two type-5 billboards (the second offset by the header's aim bytes
// and 10 units up) and enter the next phase. The spawned bullets get the
// weapon id in their header for effect_shot_impact's pattern switch.
// ============================================================================
static void effect_behavior_dual_shot(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->animHeader[0] == 0) {
        if (eff->animHeader[1] != 0) {
            g_playerPosScratch.x = (int)eff->localOffsetX - (int)(char)eff->animHeader[2];
            g_playerPosScratch.y = (int)eff->localOffsetY + 10;
            g_playerPosScratch.z = (int)(char)eff->animHeader[3] + (int)eff->localOffsetZ;
            unsigned char slot = Effect_CreateBillboard(5, 3, eff->yaw, (void*)eff->spriteInfo,
                                                        &g_playerPosScratch, eff->lightFactor);
            g_playerDisplacement = (int)(char)slot;
            g_effectPool[g_playerDisplacement].animHeader[3] =
                (unsigned char)(g_playerEntity.equippedWeaponId - 2);
        }
        g_playerPosScratch.x = (int)(char)eff->animHeader[2] + (int)eff->localOffsetX;
        g_playerPosScratch.y = (int)eff->localOffsetY;
        g_playerPosScratch.z = (int)(char)eff->animHeader[3] + (int)eff->localOffsetZ;
        unsigned char slot = Effect_CreateBillboard(5, 3, eff->yaw, (void*)eff->spriteInfo,
                                                    &g_playerPosScratch, eff->lightFactor);
        g_playerDisplacement = (int)(char)slot;
        g_effectPool[g_playerDisplacement].animHeader[3] =
            (unsigned char)(g_playerEntity.equippedWeaponId - 2);
        effect_behavior_next_phase();
        return;
    }
    eff->animHeader[0]--;
}

// ============================================================================
// g_effectBehaviorTable (0x004c47b8) - 64 behavior function pointers, indexed
// by the animation header's animId (byte 0) and updateId (byte 1). The table
// runs to 0x004c48b8, right up against the room effect sprite table; entries
// 0/1 and 60-63 are the inert behavior.
// ============================================================================
static void (*const g_effectBehaviorTable[64])(void) = {
    effect_behavior_idle,            //  0
    effect_behavior_idle,            //  1
    effect_behavior_timer_refresh,   //  2
    effect_behavior_hold_frame,      //  3
    effect_behavior_ember,           //  4
    effect_behavior_gravity_settle,  //  5
    effect_behavior_timer_phase,     //  6
    effect_behavior_bounce,          //  7
    effect_behavior_gravity_impact,  //  8
    effect_behavior_gravity_rise,    //  9
    effect_behavior_projectile,      // 10
    effect_behavior_rand_phase,      // 11
    effect_behavior_spawner,         // 12
    effect_behavior_count_phase,     // 13
    effect_behavior_blink,           // 14
    effect_behavior_floor_splash,    // 15
    effect_behavior_wobble,          // 16
    effect_behavior_bounce_debris,   // 17
    effect_behavior_clone,           // 18
    effect_behavior_fall_stop,       // 19
    effect_behavior_phase_drop,      // 20
    effect_behavior_fade_fall,       // 21
    effect_behavior_drop_splat,      // 22
    effect_behavior_ground_splat,    // 23
    effect_behavior_jitter,          // 24
    effect_behavior_drift,           // 25
    effect_behavior_spawn_from_header, // 26
    effect_behavior_next_frame,      // 27
    effect_behavior_next_phase,      // 28
    effect_behavior_rand_frame,      // 29
    effect_behavior_spawn_advance,   // 30
    effect_behavior_set_frame,       // 31
    effect_behavior_floor_phase,     // 32
    effect_behavior_frame_phase,     // 33
    effect_behavior_burn,            // 34
    effect_behavior_fire_wobble,     // 35
    effect_behavior_fire_bounce,     // 36
    effect_behavior_flamethrower,    // 37
    effect_behavior_set_tint,        // 38
    effect_behavior_pair_timer,      // 39
    effect_behavior_timer_flags,     // 40
    effect_behavior_rand_anim,       // 41
    effect_behavior_charge,          // 42
    effect_behavior_bullet,          // 43
    effect_behavior_fire_burst,      // 44
    effect_behavior_rocket,          // 45
    effect_behavior_countdown_kill,  // 46
    effect_behavior_kill,            // 47
    effect_behavior_fade,            // 48
    effect_behavior_flash_marker,    // 49
    effect_behavior_floor_stop,      // 50
    effect_behavior_sysflag,         // 51
    effect_behavior_to_settle,       // 52
    effect_behavior_floor_stop1,     // 53
    effect_behavior_random_spin,     // 54
    effect_behavior_splash_timer,    // 55
    effect_behavior_fire_phases,     // 56
    effect_behavior_muzzle_phases,   // 57
    effect_behavior_autoaim_flash,   // 58
    effect_behavior_dual_shot,       // 59
    effect_behavior_idle,            // 60
    effect_behavior_idle,            // 61
    effect_behavior_idle,            // 62
    effect_behavior_idle,            // 63
};

// ============================================================================
// Effect_AnimateSprite (0x0047ce10)
// Steps the sprite animation: when frameDelay expires, advance to the next
// vram frame entry and update the UV/position pointers. A frame entry of
// (0, 0) ends the effect: the slot is freed (g_freeEffectSlots++,
// animId = updateId = 0). A delay byte of 0xFF is a jump: the frame byte
// selects the new entry.
// ============================================================================
static void Effect_AnimateSprite(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    if (eff->frameDelay == 0) {
        unsigned char* entry = (unsigned char*)(eff->vramInfoBackup + 4);
        eff->frameIndex++;

        if (entry[0] == 0 && entry[1] == 0) {
            // end of animation - free the slot (no delay decrement here)
            if (eff->animId == 0 && eff->updateId == 0) {
                return;
            }
            g_freeEffectSlots++;
            eff->updateId = 0;
            eff->animId = 0;
            return;
        }

        if (entry[1] == 0xFF) {
            // jump: the frame byte selects the new entry from the vram base
            entry = (unsigned char*)((unsigned int)entry[0] * 4 + eff->vramInfo);
            eff->frameIndex = entry[0];
        }

        eff->frameDelay = entry[1];
        eff->uvDataBackup = (unsigned int)entry[0] * 4 + eff->uvData;
        eff->vramInfoBackup = (int)entry;
    }

    // the original decrements the (possibly freshly loaded) delay every frame
    eff->frameDelay--;
}

// ============================================================================
// Blend-entry count and start row per depth slot (0x004c4f50 count fields).
// g_EffectBlendTable packs every slot's entries consecutively.
// ============================================================================
static const unsigned char g_EffectBlendCount[32] = {
    4, 4, 5, 7, 6, 7, 2, 5, 1, 5, 1, 5, 1, 7, 6, 1,
    1, 3, 1, 5, 5, 5, 1, 5, 6, 6, 2, 7, 2, 1, 6, 1,
};
static const unsigned char g_EffectBlendStart[32] = {
    0, 4, 8, 13, 20, 26, 33, 35, 40, 41, 46, 47, 52, 53, 60, 66,
    67, 68, 71, 72, 77, 82, 87, 88, 93, 99, 105, 107, 114, 116, 117, 123,
};

// ============================================================================
// effect_submit_sprite - the shared render tail of EffectActor_UpdateAndRender
// (0x0047c2f0) and effect_draw_mirror_reflection: fill g_TextureDesc from the slot's sprite
// pointers, compute the size from the camera light and distance, walk the
// blend/colour tables, and submit through SubmitEffectSprite.
//
// `scaleDivisor` is the depth factor of the caller (depthScaled + 1 for the
// in-game path, (depth<<2) + 1 for the menu path - the same value, recomputed
// in the menu's mirrored camera), `depth` the full projection used for the
// distance cull (bits 4+ >= 0x4000 skips) and the sort key (>> 4).
// `texVHack` enables the stage-6 room-0xc tint remap and `stage4Special` the
// stage-4 room-0x13 camera-5 fixed-depth draw; both are in-game-path only.
// ============================================================================
static void effect_submit_sprite(Effect* eff, short screenX, short screenY,
                                 int scaleDivisor, unsigned int depth,
                                 int texVHack, int stage4Special)
{
    unsigned char* uv = (unsigned char*)eff->uvDataBackup;

    // ---- texture descriptor (0x0047c6d8-0x0047c79b) ----
    g_TextureDesc.flags = ((unsigned int)(eff->animHeader[10] >> 6) << 0x16) | 0x40;
    g_TextureDesc.width = (unsigned short)*(unsigned char*)(eff->vramInfoBackup + 2);
    g_TextureDesc.height = (unsigned short)*(unsigned char*)(eff->vramInfoBackup + 3);
    g_TextureDesc.pivotX = 0x80 - (unsigned short)uv[2];
    g_TextureDesc.pivotY = 0x80 - (unsigned short)uv[3];
    g_TextureDesc.screenX = screenX;
    g_TextureDesc.screenY = screenY;
    g_TextureDesc.texturePage = *(unsigned short*)(eff->clutInfo + 6);
    g_TextureDesc.clutX = (*(unsigned short*)(eff->clutInfo + 4) & 0x3f) << 4;
    g_TextureDesc.texU = uv[0];
    g_TextureDesc.texV = uv[1];
    // depthGroup packs two things: the low 3 bits select the animation, and
    // depthGroup >> 3 lands here as the tint index.
    //
    // AUDITED 2026-08-16 - this path is complete, and an earlier note claiming a
    // "missing CLUT half" was wrong. The audit, so nobody repeats it:
    //
    //   * g_TextureDesc.clutY is 0x00be1172. It has exactly TWO readers
    //     in the whole exe - 0x0047c8cf (this function) and 0x0047cd6b (the
    //     menu redraw) - and both use it only as the index into
    //     g_EffectColorRecords[colorIdx], which is what the port does below.
    //   * Effect_CreateBillboard (0x0047be30) stores depthGroup verbatim and
    //     otherwise touches only `depthGroup & 7`, the animation index. nClutInfo
    //     comes from g_effectSpriteInfo[type], per sprite TYPE, never per tint.
    //   * SubmitEffectSprite takes texture page variant 0 unconditionally
    //     (`g_TexturePageTable[textureId * 223]`). AddSprite_Ex is the one that
    //     resolves clutY into a CLUT variant, and effects do not go
    //     through it. That asymmetry in the ORIGINAL is what the old note
    //     mistook for something the port had dropped.
    //
    // So through THIS path depthGroup >> 3 has exactly one effect: picking the
    // RGB triple that multiplies the sprite. The multi-row-sheet palette
    // variants (blood rows, room light rows) are a SECOND axis that lives in
    // the sheet TIMs' CLUT rows and is resolved at submit time by the
    // sheetSlot redirects further down - see the Plant 42 white-sap fix
    // (2026-08-25) and the room redirect below (ROOM5080 light).
    //
    // The old note also reasoned that Plant 42's tint resolving to FFFFFF meant
    // the sap "should be white". FFFFFF is the identity multiplier - it means no
    // modulation, i.e. render the sprite in its own colours. It says nothing
    // about what colour those are. If the sap ever does look wrong, the place to
    // look is the sprite art itself (which CLUT blit_effect_tim_at pulls out of
    // the RDT/esp TIM), not this index.
    g_TextureDesc.clutY = (short)(eff->depthGroup >> 3);

    // ---- scale: camera light * sprite light factor * width / distance ----
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    int scale = (int)((unsigned int)effect_light_value()
                      * (unsigned int)eff->lightFactor
                      * (unsigned int)g_TextureDesc.width * 0x100u) / scaleDivisor;
    g_TextureDesc.colorMulB = 0x80;
    if ((unsigned int)scale >= 0x8000) scale = (int)0xffff8000;
    g_TextureDesc.scaleX = (short)scale;
    g_TextureDesc.scaleY = g_TextureDesc.scaleX;

    // ---- distance cull ----
    if ((depth & 0xfffffff0u) > 0x3fff) {
        return;
    }

    // ---- sprite-depth slot (the room effect sprite table, see above) ----
    unsigned int rec = effect_depth_record();
    if (rec == 0xFF) {
        return;
    }
    if (rec >= 32) return;   // port-only: the original would read past the table

    // ---- blend entry scan: first entry with texV < startV + len ----
    //
    // The scan needs the sprite's PAGE-ABSOLUTE V. In the original every sprite
    // is blitted into a shared page so its stored V already is absolute; the
    // port samples the weapon-FX block from per-sprite SRVs, whose UVs stay
    // sprite-local (v = 0 for the first row), so the page offset is carried in
    // g_effectSpriteBandV and re-applied here. Without it every weapon sprite
    // matched band 0 - colorIdx 0 is {0xff,0xff,0xff}, so blood rendered white.
    // Room sprites share a page and already have the offset in their UV records,
    // so their bias is 0. The sampling coordinate in g_TextureDesc is untouched.
    unsigned int bandV = (unsigned int)(unsigned char)
        (g_TextureDesc.texV + g_effectSpriteBandV[eff->effectType]);

    int i = 0;
    int count = g_EffectBlendCount[rec];
    const unsigned char* row = g_EffectBlendTable[g_EffectBlendStart[rec]];
    while (i < count) {
        if (bandV < (unsigned int)row[0] + (unsigned int)row[1]) break;
        row += 4;
        i++;
    }
    if (i >= count) i = 0;
    row = g_EffectBlendTable[g_EffectBlendStart[rec]] + i * 4;

    unsigned char blendMode = row[2];
    if ((g_stageId == STAGE_GUARDHOUSE) && ((g_roomId == ROOM_WATER_TANK) || (g_roomId == ROOM_SECURITY_ROOM) || (g_roomId == ROOM_CONTROL_ROOM))) {
        blendMode = 0;
    }
    unsigned int colorIdx = row[3];

    int tint = (int)g_TextureDesc.clutY;

    if (texVHack && (g_stageId == STAGE_MANSION_RETURN_2F) && (g_roomId == ROOM_LESSON_ROOM)
        && ((tint == 2) || (tint == 1))
        && (0x1a < g_TextureDesc.texV)
        && ((unsigned short)(g_TextureDesc.texV + g_TextureDesc.height) < 99)) {
        g_TextureDesc.texV = (unsigned char)(g_TextureDesc.texV + 0x7b);
        tint = 0;
    }

    if (colorIdx >= 60) return;   // port-only guard
    if (g_EffectColorRecords[colorIdx].count <= tint) tint = 0;

    const unsigned char* color = g_EffectColorRecords[colorIdx].table + tint * 3;

    // ---- room effect sprites draw in their OWN authored colours ----
    //
    // The original resolves the RGB through g_EffectColorRecords (0x004c5288)
    // + tint*3 exactly as above (0x0047c99e), but its texture layer also
    // carries a per-primitive CLUT coordinate for these sprites
    // (g_TextureDesc.clutX = (spriteInfo[2] & 0x3f) << 4, stored by
    // setup_effect_sprite_textures at 0x0047bdab) that selects which 16-entry
    // CLUT row of the resident page the art indexes - a second palette axis
    // that has no equivalent once the page is baked to RGBA. Baking CLUT row 0
    // AND multiplying by the record tint applies the variant selection twice.
    //
    // The flooded rooms' type-0x17 water ripple (ROOM40D0/40E0) shows it: rec
    // 24, band [147,211) -> colour record 41 (0x29) = {ffffff, b4c8b4}, tint =
    // depthGroup>>3 = 1 (spawners: room_action_effect 0x0041ba10 with
    // depthGroup 8, and SCD cmd_effect_spawn 0x004316c0 - ROOM40D0's script at
    // RDT+0x13CE4/0x13CF0 - with parentParam 0x000B), so the ripple's light-
    // blue RDT art came out green. The per-room TIM is authored with its final
    // palette (the ripple's CLUT row 0 IS the light-blue ramp), so sprites
    // served from the room pages (SRV 11-14) render untinted; weapon-FX sheets
    // (core00, SRV 3-10) keep their records - those differentiate blood red
    // from spray yellow on one shared sheet.
    bool roomArt = false;
    for (int rSlot = 0; rSlot < 8; rSlot++) {
        if (g_abEffSpriteIndexTable[8 + rSlot] == eff->effectType) {
            roomArt = true;
            break;
        }
    }
    static const unsigned char kRoomArtNoTint[3] = { 0xff, 0xff, 0xff };
    if (roomArt) color = kRoomArtNoTint;

    unsigned int stage = get_stage_id();
    if (stage > 4) stage -= 5;
    unsigned int iCam = ((unsigned int)g_roomId + stage * 0x20) * 8 + (unsigned int)g_roomCameraId;
    if (iCam >= sizeof(g_EffectCameraLightIndex)) return;   // port-only guard
    unsigned int lightRec = g_EffectCameraLightIndex[iCam];
    if (lightRec >= 7) return;   // port-only guard

    int scaleXadd = g_EffectLightRecords[lightRec][0] + g_EffectScaleBiasX;
    int scaleYadd = g_EffectLightRecords[lightRec][1] + g_EffectScaleBiasY;
    int brightness = g_EffectLightRecords[lightRec][2];

    // ---- stage-5 room-0x13 camera-5 special case (in-game path only): the
    // Tyrant's perspective inside the stasis pod view renders the effect unscaled
    // and at a fixed depth ----
    //
    // The colour is NOT reset here. 0x0047c99e is `LEA ECX,[EDI+EDI*2]` then
    // `ADD ECX,[EBP+0x4c528c]` - tint*3 added to the record table, exactly like
    // the general path. This branch only zeroes the two scale addends and pins
    // the depth to 0x3c; brightness and blendMode carry through untouched.
    unsigned int depthArg;
    if (stage4Special && (g_stageId == STAGE_LABORATORY) && (g_roomId == ROOM_MAIN_LAB) && (g_roomCameraId == 5)) {
        scaleXadd = 0;
        scaleYadd = 0;
        depthArg = 0x3c;
    } else {
        depthArg = depth >> 4;
    }

    // The texture id handed to SubmitEffectSprite becomes the D3D11 SRV slot.
    // The original derives it from the sprite depth (texY - 0x18), but several
    // sheets share each depth row, so the port resolves it through the sheet
    // slot recorded by setup_effect_sprite_textures instead, offset by 3 to
    // match where LoadEffectTextureSheet stores the SRVs (3-10 weapon,
    // 11-14 room). Every effect that reaches the submit has a valid sheet
    // (Effect_CreateBillboard rejects unloaded sprite types).
    unsigned char sheetSlot = g_effectSpriteSheetSlot[eff->effectType];
    if (sheetSlot == 0xFF) {
        return;
    }
    int texSlot = 3 + (int)sheetSlot;

    // ---- weapon-FX sheets serve their Effspr page palette; the tint record
    // does the enemy colouring ----
    //
    // the OG's effect path serves page variant 0 - SubmitEffectSprite 
    // (0x0046d9b0) indexes g_TexturePageTable[textureId*223] with no variant, 
    // and GetTextureVariant is always 0 for effect descriptors.
    // The per-tint CLUT-row redirect lives ONLY in
    // AddSprite_Ex/AddTintSprite_Ex (room static sprites). The old
    // port-side redirect (SRV 120+sheetSlot*4+clutRow) fabricated that
    // mechanism for weapon sheets and is what made the explosion smoke render
    // the smoke sheet's DARK row-1 palette at tint 1 (the OG keeps the WHITE
    // row-0 palette there and multiplies by the all-white/0xb2 record).
    // Weapon sheet SRVs are now baked straight from the Effspr page regions by
    // load_shoot_direction_data (see kWeaponSheetPageV), so no redirect is
    // needed: texSlot = 3 + sheetSlot for every sheet, and the RGB record
    // above supplies the per-tint colour (zombie red blood, hunter green,
    // Plant 42 white - the gore art is light grey, the record is the hue).

    // ---- room RDT sheets: the same CLUT-row variant selection ----
    //
    // The room esp TIMs carry the same per-row palette variants the weapon
    // sheets do, selected per spawn by the tint index. ROOM5080's passcode
    // panel light is the proof: the init SCD spawns type 0x0A with depthGroup
    // 0x00-0x02 (red instances) and the main SCD re-spawns it with depthGroup
    // 0x08-0x0A once a pass code is entered, while the sheet ships row 0 red /
    // row 1 cyan-blue / row 2 orange / row 3 bright blue. The band multiplier
    // cannot be the colour mechanism here: the light's band (page 1, V=3)
    // maps to colour record 4 - the BLOOD record {69,1e,0a / 37,5a,14 /
    // 91,5a,14 / ff,ff,ff} shared with the zombie splatter that packs to the
    // same page position - which multiplies, and a red ramp times anything
    // stays red. So the row IS the tint for these room sheets (the weapon
    // sheets above are the opposite case: page variant 0 in the OG, the RGB
    // record does the colouring - see the audit note).
    // load_effect_sprites bakes rows 1-3 to SRV 152 + (sheetSlot-8)*4 +
    // (row-1); row 0 stays on the plain page SRVs 11-14. Single-row room
    // sheets (g_effectSpriteClutRows[type] <= 1) are untouched and keep
    // rendering their base palette from the page SRV.
    if (sheetSlot >= 8) {
        int rows = (int)g_effectSpriteClutRows[eff->effectType];
        if (rows > 1) {
            int clutRow = (int)g_TextureDesc.clutY;
            if (clutRow > rows - 1) clutRow = rows - 1;
            if (clutRow > 3) clutRow = 3;
            if (clutRow > 0) {
                int varSlot = 152 + ((int)sheetSlot - 8) * 4 + (clutRow - 1);
                while (clutRow > 1 && g_TexturePageSRV[varSlot] == MARNI_NULL_HANDLE) {
                    clutRow--;
                    varSlot--;
                }
                if (g_TexturePageSRV[varSlot] != MARNI_NULL_HANDLE) {
                    texSlot = varSlot;
                }
            }
        }
    }

    int submitted = SubmitEffectSprite(&g_TextureDesc, (int)depthArg, texSlot,
                                       color[0], color[1], color[2],
                                       scaleXadd, scaleYadd, (int)blendMode, (short)brightness);
    (void)submitted;

    unsigned int depthSort = depth >> 4;
    if (depthSort < (unsigned int)g_MaxHealthDisplayFlag) {
        g_MaxHealthDisplayFlag = (int)depthSort;
    }
}

// ============================================================================
// EffectActor_UpdateAndRender (0x0047c2f0)
// The per-slot update+render step. When (g_message_flags & 8) the slot's
// behavior runs, then - for transform-type effects (animHeader[10] & 2) - the
// position is integrated from the velocity header through the sprite matrix,
// projected, and the velocity header advanced. The sprite animation is stepped
// and the sprite submitted.
//
// Effects whose header lacks bit 1 keep whatever position the stack had in the
// original; the port zeroes that scratch so the zone test deterministically
// rejects them instead of randomly drawing at a stale spot.
// ============================================================================
void EffectActor_UpdateAndRender(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];

    // Projected screen coordinates: ProjectEffectSprite writes a packed dword
    // (x in the low 16 bits, y in the high 16) through the pointer. Two
    // separate `short` locals are NOT guaranteed adjacent, and with /RTCs the
    // 2-byte overhang past the first trips the stack guard - the original's
    // assembly uses one dword slot, so model it as an explicit short pair.
    struct { short x; short y; } local_1c = { 0, 0 };

    // Seeded from the slot's LAST KNOWN world position, not zero.
    //
    // Only the `animHeader[10] & 2` branch below writes local_10, and the
    // original leaves it uninitialised otherwise - it reads whatever the stack
    // slot happens to hold. In practice that is almost always this same
    // function's previous value, i.e. the last position written for this or a
    // neighbouring effect, so the original keeps drawing the sprite through
    // frames whose animation phase has the transform bit clear.
    //
    // Zeroing it made the zone test reject those frames outright, and an effect
    // whose phases alternate between hdr10=0x03 and hdr10=0x00 then FLICKERED -
    // visible in ROOM1000 as the type-0x0B sprite drawing every other frame:
    //   hdr10=03 world=(5160,-930,8690) -> DREW
    //   hdr10=00 world=(0,0,0)          -> outside switch zone
    // eff->posX/Y/Z is exactly what the transform branch last stored, so it is
    // the deterministic stand-in for the original's stale stack value.
    VECTOR local_10;
    local_10.x = (int)eff->posX;
    local_10.y = (int)eff->posY;
    local_10.z = (int)eff->posZ;
    local_10.pad = 0;

    if ((g_message_flags & 8) != 0) {
        g_effectBehaviorTable[eff->animId]();
    }

    if ((eff->animHeader[10] & 2) != 0) {
        // ---- integrate the velocity header through the yaw and sprite matrix ----
        RotMatrixY((int)eff->yaw, &g_matrixScratch);
        ApplyMatrixSV(&g_matrixScratch, (SVECTOR*)&eff->rotSpeedX, &g_svecScratch);
        g_svecScratch.x += eff->localOffsetX;
        g_svecScratch.y += eff->localOffsetY;
        g_svecScratch.z += eff->localOffsetZ;

        if (eff->type == 1) {
            memcpy(eff->transform, (void*)eff->spriteInfo, 0x20);
        }

        ApplyMatrix((MATRIX*)eff->transform, &g_svecScratch, (VECTOR*)&g_playerPosScratch);

        g_svecScratch.x = (short)((short)eff->spriteOffsetX + (short)g_playerPosScratch.x);
        g_svecScratch.y = (short)((short)eff->spriteOffsetY + (short)g_playerPosScratch.y);
        g_svecScratch.z = (short)((short)eff->spriteOffsetZ + (short)g_playerPosScratch.z);

        if ((eff->animHeader[10] & 4) != 0) g_svecScratch.y = 0;
        if ((eff->animHeader[10] & 8) != 0) g_svecScratch.y = (short)g_playerEntity.posY;

        eff->posX = g_svecScratch.x;
        eff->posY = g_svecScratch.y;
        eff->posZ = g_svecScratch.z;

        local_10.x = (int)g_svecScratch.x;
        local_10.y = (int)g_svecScratch.y;
        local_10.z = (int)g_svecScratch.z;

        GetMatrixTranslation(&g_RoomCameraData);
        SetGlobalScaledRotationMatrix(&g_RoomCameraData);

        eff->projDepth = ProjectEffectSprite(&g_svecScratch, (int*)&local_1c);
        local_1c.x -= 0xa0;
        local_1c.y -= 0x78;
        eff->depthScaled = (short)((unsigned short)eff->projDepth << 2);

        if ((g_message_flags & 8) != 0) {
            AH_SHORT(eff, 0xc) = (short)(AH_SHORT(eff, 0xc) + AH_SHORT(eff, 4));
            AH_SHORT(eff, 0xe) = (short)(AH_SHORT(eff, 0xe) + AH_SHORT(eff, 6));
            AH_SHORT(eff, 0x10) = (short)(AH_SHORT(eff, 0x10) + AH_SHORT(eff, 8));
            eff->rotSpeedX += AH_SHORT(eff, 0xc);
            eff->rotSpeedY += AH_SHORT(eff, 0xe);
            eff->rotSpeedZ += AH_SHORT(eff, 0x10);
        }
    }

    if ((g_message_flags & 8) != 0) {
        g_effectBehaviorTable[eff->updateId]();
    }

    if (((eff->animHeader[10] & 1) != 0) && ((g_message_flags & 8) != 0)) {
        Effect_AnimateSprite();
    }

    if ((eff->animHeader[11] & 0x80) != 0) return;

    if (is_entity_in_switch_zone(&local_10, g_CurrentRdtDataTypePtr) == 0) return;

    effect_submit_sprite(eff, local_1c.x, local_1c.y,
                         (int)(unsigned short)eff->depthScaled + 1,
                         (unsigned int)eff->projDepth, 1, 1);
}

// ============================================================================
// effect_draw_mirror_reflection - the mirror-pass redraw of one effect.
//
// update_2d_effects calls this after EffectActor_UpdateAndRender when the room
// script has turned the mirror on (g_main_state_flags & 1): the camera has been
// reflected about the mirror plane, so the effect is re-projected through the
// reflected matrix and submitted a second time. Transform-type effects project
// their stored position; the screenY offset moves by a full frame height when
// g_spriteAnimActive == 0 (the pass renders into the other half of the 480-line
// buffer). mirror_point_visible then culls anything not actually visible in the mirror.
// ============================================================================
static void effect_draw_mirror_reflection(void)
{
    Effect* eff = &g_effectPool[g_activeEffectIndex];
    // packed screen coordinates, see EffectActor_UpdateAndRender
    struct { short x; short y; } local_1c = { 0, 0 };
    VECTOR local_10 = { 0, 0, 0, 0 };
    unsigned int local_20 = 0;   // high half is stack residue in the original
    unsigned short depth = 0;

    if ((eff->animHeader[10] & 2) != 0) {
        local_10.x = (int)eff->posX;
        local_10.y = (int)eff->posY;
        local_10.z = (int)eff->posZ;

        GetMatrixTranslation(&g_RoomCameraData);
        SetGlobalScaledRotationMatrix(&g_RoomCameraData);

        depth = (unsigned short)ProjectEffectSprite((SVECTOR*)&eff->posX, (int*)&local_1c);
        local_20 = (local_20 & 0xFFFF0000u) | (unsigned int)depth;
        local_1c.x -= 0xa0;
        local_1c.y += (g_spriteAnimActive == 0) ? -0xf0 : 0;
        local_1c.y -= 0x78;
    }

    if ((eff->animHeader[11] & 0x80) != 0) return;

    int* camera = (int*)((char*)g_RdtPointer + 0x9c + (int)g_roomCameraId * 0x2c);
    if (mirror_point_visible((void*)camera, (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
                     (int)&local_10) == 0) return;

    if (is_entity_in_switch_zone(&local_10, g_CurrentRdtDataTypePtr) == 0) return;

    // the depth factor is recomputed from this function's own projection (the
    // camera was mirrored since the in-game pass); the sort key keeps the low
    // 16 bits of local_20 >> 4, as SubmitEffectSprite masks it anyway.
    effect_submit_sprite(eff, local_1c.x, local_1c.y,
                         (int)(unsigned short)(depth << 2) + 1,
                         local_20, 0, 0);
}

// ============================================================================
// update_2d_effects (0x0047c0c0)
// Walks the 64-slot pool from the top and updates/renders every active slot.
// In a mirror room (g_main_state_flags & 1, set only by SCD opcode 0x0F), each
// effect is redrawn into the reflected camera: the camera record is read out of
// the RDT, reflected about the mirror plane (FlipSprite), installed
// (MatrixToCamera), the handedness flip composed into g_RoomCameraData
// (Matrix_MulMatrix), the effect redrawn (effect_draw_mirror_reflection), and the camera
// restored. This is the effect-side twin of entity_draw_mirror_reflection in EntityCommon.cpp.
//
// The 0x004c59bc flag is a static 1 in the shipped exe (never written - the
// only xref is this read), so the reverse iteration branch (63..0) is the one
// that runs; the forward branch is kept for completeness.
// ============================================================================
void update_2d_effects(void)
{
    MATRIX tempMatrix;
    static const int g_EffectPoolFlag = 1;   // 0x004c59bc

    if (g_EffectPoolFlag != 0) {
        g_activeEffectIndex = 64;
        do {
            g_activeEffectIndex--;
            Effect* eff = &g_effectPool[g_activeEffectIndex];
            if (eff->animId != 0) {
                g_matrixScratch = g_identityMatrixData;
                EffectActor_UpdateAndRender();

                if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
                    int* camera = (int*)((char*)g_RdtPointer + 0x9c
                                         + (int)g_roomCameraId * 0x2c);
                    FlipSprite(camera, &tempMatrix,
                               (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
                               g_mirrorPlaneCoord);
                    MatrixToCamera(&tempMatrix);

                    g_matrixScratch = g_identityMatrixData;
                    g_matrixScratch.m[0][0] = -g_matrixScratch.m[0][0];
                    Matrix_MulMatrix(&g_matrixScratch, &g_RoomCameraData);

                    effect_draw_mirror_reflection();

                    MatrixToCamera((MATRIX*)camera);
                }
            }
        } while (g_activeEffectIndex != 0);
        return;
    }

    // Forward branch (0x0047c1dd) - the original only takes it when the flag
    // is zero, which the shipped exe never does.
    g_activeEffectIndex = 0;
    do {
        Effect* eff = &g_effectPool[g_activeEffectIndex];
        if (eff->animId != 0) {
            g_matrixScratch = g_identityMatrixData;
            EffectActor_UpdateAndRender();

            if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
                int* camera = (int*)((char*)g_RdtPointer + 0x9c
                                     + (int)g_roomCameraId * 0x2c);
                FlipSprite(camera, &tempMatrix,
                           (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
                           g_mirrorPlaneCoord);
                MatrixToCamera(&tempMatrix);

                g_matrixScratch = g_identityMatrixData;
                g_matrixScratch.m[0][0] = -g_matrixScratch.m[0][0];
                Matrix_MulMatrix(&g_matrixScratch, &g_RoomCameraData);

                effect_draw_mirror_reflection();

                MatrixToCamera((MATRIX*)camera);
            }
        }
        g_activeEffectIndex++;
    } while (g_activeEffectIndex < 0x40);
}

// ============================================================================
// FlipSprite (0x0048bca0)
// Copies the camera record (8 dwords: posX, posY, posZ, toX, toY, toZ, roll,
// lightIndex) from `src` into `dst`, then reflects it about the mirror plane.
// `mirror` is the plane axis: set folds the X pair (dwords 0 and 3, the eye and
// the look-at target) around width*2, i.e. the plane X = width; clear folds the
// Z pair (dwords 2 and 5), the plane Z = width. `width` is g_mirrorPlaneCoord. The
// fold reads the ORIGINAL source dwords - the copy does not advance the pointer
// used by the fold.
// ============================================================================
void FlipSprite(int* src, MATRIX* dst, unsigned char mirror, unsigned int width)
{
    int* out = (int*)dst;
    for (int i = 0; i < 8; i++) {
        out[i] = src[i];
    }

    int fold = (int)(width & 0xffff) * 2;
    if (mirror != 0) {
        ((int*)dst)[0] = fold - src[0];
        ((int*)dst)[3] = fold - src[3];
        return;
    }
    ((int*)dst)[2] = fold - src[2];
    ((int*)dst)[5] = fold - src[5];
}

// ============================================================================
// Matrix_MulMatrix (0x0040a210)
// m1 = m0 * m1 (in-place compose). Only used by the mirror pass - here and in
// entity_draw_mirror_reflection.
// ============================================================================
void Matrix_MulMatrix(MATRIX* m0, MATRIX* m1)
{
    CompMatrix(m0, m1, m1);
}

// ============================================================================
// FUN_0047d0e0 - effect cleanup: re-create the current room's four effect
// texture pages (called when leaving the menu so the room textures are
// refreshed).
// ============================================================================
void FUN_0047d0e0(void)
{
    const unsigned char* row = room_effect_page_entry();
    for (int i = 0; i < 4; i++) {
        if (row[i] != 0xFF) {
            TexturePage_Create(i);
        }
    }
}

