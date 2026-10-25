// Chimera.cpp - Chimera (entity type 9, enemy/em1009.emd)
//
// The humanoid-ape mutant that hangs from ceilings and drops on the player.
//
// Original PC addresses:
//   chimera_update               0x00438a70  per-frame entry, dispatch table [9]
//   chimera_state_init           0x00438880  state 0 spawn
//   chimera_state_run            0x00438be0  state 1 AI driver
//   chimera_hit_dispatch         0x00438cb0  state 2 hit-reaction layer
//   chimera_death_dispatch       0x00438db0  state 3 death layer
//   chimera_behavior_dispatch    0x00438e60  state 1 behaviour prologue + jump
//   chimera_variant_ai           0x00438eb0  pre-AI manhattan roll, per variant
//   chimera_behavior_table       0x004bb450  SEVENTEEN live entries (0-0x10)
//   chimera_state_table          0x004bb438  FOUR entries
//   chimera_seed_tbl             0x004bb447  8 bytes, indexed by hit_state & 7
//   chimera_brain_table          0x004bb488  THREE entries (also behaviour 14-16)
//   chimera_idle_variant_table   0x004bb498  THREE entries (idle by variant)
//   chimera_sca_info_a           0x004bb410  SCA record (same as hunter_sca_info_b)
//   chimera_arc_heights          0x004bb4a8  33 shorts, the ceiling dive arc
//   chimera_death_anim_tbl       0x004bb52c  death anim per variant
//
// ---------------------------------------------------------------------------
// Shape of the AI - three stacked dispatch layers
// ---------------------------------------------------------------------------
//   Entity+0x84 state     0 init / 1 AI driver / 2 hit reaction / 3 death.
//                         There is no state 4+ entry: the death dissolve raises
//                         the room event flag and the room script despawns the
//                         corpse before the next frame would index the table
//                         out of bounds - same contract as the shipped exe.
//   Entity+0x85 flag      1 = a behaviour/hit layer owns the chimera; the
//                         variant brain (0x00438f00/0x00439120/0x00439400)
//                         only runs while it is 0.
//   Entity+0x86 behaviour shared by the AI driver and the hit/death layers.
//                         0 idle / 1 walk / 2 turn / 3 swipe / 4 grab / 5
//                         variant toggle / 6 ceiling drop / 7 ceiling swoop /
//                         8 acid spit / 9 flee / 10 turn-back / 11 quick
//                         toggle / 12 claw / 13 hit-fall / 14-16 the three
//                         brains themselves (the behaviour table and the
//                         variant table SHARE those entries).
//   Entity+0x87 action    per-behaviour sub-state.
//
// behavior_flags (+0x02) is the VARIANT selector: 0/1 floor chimeras, 2 the
// ceiling-hanging one, 3 = scripted hard-mode (init rerolls it to 0/1 and
// latches +0x17E). Bit 0x80 freezes the update entirely.
//
// ---------------------------------------------------------------------------
// Ghidra traps in this block
// ---------------------------------------------------------------------------
// - The state 1 jump `JMP [ECX*4 + 0x4bb450]` has SEVENTEEN live entries.
//   Ghidra merged every target body into one pseudo-function; the "switch"
//   cases are the individual behaviour functions, inlined.
// - chimera_behavior_idle (0x004395e0) is a 17-byte dispatcher that jumps
//   through a SECOND table at 0x004bb498 - the two floor variants share one
//   idle body.
// - The seed table at 0x004bb447 overlaps the last byte of the state table's
//   fourth entry in the image; separate arrays here preserve the values.
// - The dive-arc table at 0x004bb4a8 is read FORWARD by the swoop (behaviour
//   7) and BACKWARD (base 0x004bb4e8) by the drop (behaviour 6) and the two
//   knockdown/death drivers. The arrays below are in memory order; backward
//   reads are spelled [32 - frame].
// - Several Joint_move calls pass the PLAYER's isBeingAttackedFlag (or the
//   variant byte) as the first argument - the CONCAT31 noise in the
//   decompiler is just the char parameter's register.
//
// All original addresses from Ghidra.
// ============================================================================
#include "EntityCommon.h"
#include "../../Globals.h"
#include <cstdlib>

extern void ResetJointTransforms(void);                            // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);           // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90 - Room.cpp
// 0x0043d8a0 - shared heavy-weapon hit-latch reset, named for the shark.
// chimera_hit_stagger reaches it as a CALL at the end of the recovery.
extern void neptune_clear_hit_state(void);                         // 0x0043d8a0 - Neptune.cpp

// 0x00be0de4 / 0x00be0de8 - shared entity scratch, defined in EntityCommon.cpp.
extern int player_distance_z;                                      // 0x00be0de4
extern int g_scaled_down_dist;                                     // 0x00be0de8

// ============================================================================
// Offset accessors. The port's Entity field names come from the zombie's point
// of view; the chimera reuses several at other widths, so they are reached by
// offset per the "offset writes, not nearest field" rule.
// ============================================================================
#define C_STATE_WORD    (*(unsigned short*)((char*)ENTITY + 0x84))  // state+ignore
#define C_BEH_WORD      (*(unsigned short*)((char*)ENTITY + 0x86))  // behavior+action
#define C_HEALTH        (*(short*)         ((char*)ENTITY + 0x88))
#define C_POS_T         ((int*)            ((char*)ENTITY + 0x34))
#define C_Y             (*(int*)           ((char*)ENTITY + 0x38))  // localMatrix.t[1]
#define C_LOCAL_MATRIX  ((MATRIX*)         ((char*)ENTITY + 0x20))
#define C_QUAD          ((SVECTOR*)        ((char*)ENTITY + 0xE4))  // shadow/fade quad
#define C_SPEED_W       (*(unsigned short*)((char*)ENTITY + 0xC2))
#define C_TICKS         (*(short*)         ((char*)ENTITY + 0xC4))
#define C_TURN_STEP     (*(short*)         ((char*)ENTITY + 0x16C))
#define C_PATH_LATCH    (*(unsigned short*)((char*)ENTITY + 0x16E))
#define C_TOUCH_LATCH   (*(short*)         ((char*)ENTITY + 0x170))
#define C_REPAUSE       (*(short*)         ((char*)ENTITY + 0x172))
#define C_FADE_FREEZE   (*(short*)         ((char*)ENTITY + 0x174))
#define C_WALL_HIT      (*(short*)         ((char*)ENTITY + 0x176))
#define C_WALL_FRAMES   (*(short*)         ((char*)ENTITY + 0x178))
#define C_FAR_LATCH     (*(short*)         ((char*)ENTITY + 0x17A))
#define C_GRAB_MASH     (*(short*)         ((char*)ENTITY + 0x17C))
#define C_HARD_MODE     (*(short*)         ((char*)ENTITY + 0x17E))

// The player's position, as the original addresses it (0x00be6318).
#define CH_PLAYER_T     ((VECTOR*) g_playerEntity.scaMatrixData.localMatrix.t)
#define CH_PLAYER_T_INT ((int*)    g_playerEntity.scaMatrixData.localMatrix.t)

// g_deadMoveValue is a DWORD holding an address; +0x14 is the position block
// every effect spawn seeds from.
#define CH_DMV          ((const char*)(uintptr_t)g_deadMoveValue)

// ============================================================================
// chimera_sca_info_a @ 0x004bb410
// One six-short SCA record: [0] 0x8000 id/terminator, [1..3] local box,
// [4] half-width, [5] radius. Byte-identical to hunter_sca_info_b. A second
// record lives at 0x004bb420 ({0x8001, 0, 5000, 0, 0, 0}) but nothing in the
// exe references it.
// ============================================================================
const short chimera_sca_info_a[8] = { (short)0x8000, 0, (short)-180, 0, 180, 500, 0, 0 };

// ============================================================================
// chimera_seed_tbl @ 0x004bb447 - the initial hit/death behaviour roll,
// indexed by hit_state & 7. Overlaps the state table's last pointer byte in
// the image; values preserved here.
// ============================================================================
const unsigned char chimera_seed_tbl[8] = { 0, 2, 0, 2, 0, 4, 4, 0 };

// ============================================================================
// chimera_arc_heights @ 0x004bb4a8 (33 shorts, through 0x004bb4e8)
// The ceiling dive arc as NEGATED Y heights: the swoop (behaviour 7) reads
// forward (rises from 1540 to the 4700..5068 ceiling plateau) and the drop
// (behaviour 6) plus the two hit/death drivers read BACKWARD from
// 0x004bb4e8, i.e. [32 - frame], descending from the ceiling.
// ============================================================================
const short chimera_arc_heights[33] = {
    1540, 1225, 1040, 1290, 1625, 2100, 2500, 2900,
    3200, 3500, 3800, 4100, 4300, 4500, 4600, 4700,
    4700, 4700, 4700, 4700, 4700, 4700, 4700, 4700,
    4700,
    4800, 4900, 5000, 5068, 5068, 5068, 5068, 5068
};

// ============================================================================
// Horizontal speeds read alongside the arc. The swoop reads forward from
// 0x004bb4ea - whose first word is ZERO, so the climb starts with no
// horizontal motion and only drifts 150 on frames 18-27; the drop reads
// backward from 0x004bb52a (drift 150 on frames 6-15). Both spell out the
// zero runs the original walks through. The same 150 run serves both reads.
// ============================================================================
const short chimera_swoop_speed[33] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
    0, 0, 0, 0, 0
};
const short chimera_drop_speed[32] = {
    0, 0, 0, 0, 0, 0,
    150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// ============================================================================
// chimera_death_anim_tbl @ 0x004bb52c - the hit-fall (behaviour 13) anim per
// variant. Variants 0/1 share anim 6; the third byte (variant 2) is 0.
// ============================================================================
const unsigned char chimera_death_anim_tbl[3] = { 6, 6, 0 };

// ---------------------------------------------------------------------------
// Forward declarations, in address order.
// ---------------------------------------------------------------------------
static void chimera_state_init(void);            // 0x00438880
static void chimera_state_run(void);             // 0x00438be0
static void chimera_hit_dispatch(void);          // 0x00438cb0
static void chimera_death_dispatch(void);        // 0x00438db0
static void chimera_behavior_dispatch(void);     // 0x00438e60
static void chimera_variant_ai(void);            // 0x00438eb0

static void chimera_brain_a(void);               // 0x00438f00 - variant 0 brain
static void chimera_brain_b(void);               // 0x00439120 - variant 1 brain
static void chimera_brain_c(void);               // 0x00439400 - variant 2 (ceiling) brain

static void chimera_behavior_idle(void);         // 0x004395e0
static void chimera_idle_floor(void);            // 0x00439600
static void chimera_idle_ceiling(void);          // 0x00439680
static void chimera_behavior_walk(void);         // 0x00439760
static void chimera_walk_timeout(void);          // 0x004398a0
static void chimera_behavior_turn(void);         // 0x00439980
static void chimera_behavior_swipe(void);        // 0x00439ad0
static void chimera_behavior_grabhold(void);     // 0x00439df0
static void chimera_behavior_toggle(void);       // 0x0043a0f0
static void chimera_behavior_drop(void);         // 0x0043a2c0
static void chimera_behavior_swoop(void);        // 0x0043a5b0
static void chimera_behavior_spit(void);         // 0x0043a7e0
static void chimera_behavior_flee(void);         // 0x0043aa10
static void chimera_behavior_turn_back(void);    // 0x0043aae0
static void chimera_behavior_toggle_short(void); // 0x0043ac20
static void chimera_behavior_claw(void);         // 0x0043ad00
static void chimera_hit_fall(void);              // 0x0043af60

static void chimera_hit_stagger(void);           // 0x0043aff0
static void chimera_hit_knockdown(void);         // 0x0043b1f0
static void chimera_death_dissolve(void);        // 0x0043b520
static void chimera_death_drop_dissolve(void);   // 0x0043b710
static void chimera_acid_burst(void);            // 0x0043bce0

// The dispatch tables are defined further down (their entries need the
// forward declarations above); declared here so the state functions can
// index them.
extern void (*const chimera_state_table[4])(void);
extern void (*const chimera_behavior_table[17])(void);
extern void (*const chimera_brain_table[3])(void);
extern void (*const chimera_idle_variant_table[3])(void);

// Shared effect helper: stage the g_deadMoveValue + 0x14 position block into
// g_playerPosScratch, the same seeding idiom Neptune/Cerberus use.
static void chimera_seed_from_dead_move(void)
{
    g_playerPosScratch = *(VECTOR*)((uintptr_t)g_deadMoveValue + 0x14);
}

// ============================================================================
// chimera_update @ 0x00438a70
// Per-frame entry (dispatch table [9]). The state table runs under the game
// message gate; everything gets the SCA + room pass (the chimera has no
// action-layer skip like the hunter). The fade-sprite pass also resizes the
// shadow quad every frame unless the death dissolve froze it.
// ============================================================================
void chimera_update(void) // 0x00438a70
{
    JointStruct* joints = ENTITY->jointsStructs;

    if ((g_message_flags & 4) != 0) {
        chimera_state_table[ENTITY->state]();

        SetEntityScaHitData(ENTITY);
        C_TOUCH_LATCH = (short)ResolveEntityScaCollision((Entity*)&g_playerEntityPointer, ENTITY);
        HandleEnemyPlayerCollisions();
        ENTITY->collisionFlags = (unsigned char)(ENTITY->collisionFlags & 0xf7);
        C_WALL_HIT = (short)check_room_collision(
            (VECTOR*)C_POS_T, *(short*)(uintptr_t)(ENTITY->Sca_info + 10));
    }

    ENTITY->has_enter_switch_zone =
        (unsigned char)is_entity_in_switch_zone((VECTOR*)C_POS_T, g_CurrentRdtDataTypePtr);
    if (ENTITY->has_enter_switch_zone != 0) {
        if (C_FADE_FREEZE == 0) {
            int size = (*(int*)((char*)joints + 0x1d0) >> 4) + 800;
            // Unsigned test: -(joint Y) - 0x898 < 2000, i.e. the model anchor
            // sits in the band the shadow clamps at 600 for.
            if ((unsigned int)(-*(int*)((char*)joints + 0x1d0) - 0x898u) < 2000u) {
                size = 600;
            }
            if (C_Y < -0x17d4) {
                size = size - 200;
            }
            BillboardSetSize(C_QUAD, (short)(size + 0x32), (short)(size + 200));
        }
        entity_add_fade_sprite((VECTOR*)C_POS_T, (short*)C_QUAD, 0, ENTITY->angle);
    }
}

// ============================================================================
// chimera_state_table @ 0x004bb438 - indexed by Entity+0x84. Only four live
// entries; the death dissolve hands the corpse to the room script before a
// state >= 4 could index past the table.
// ============================================================================
void (*const chimera_state_table[4])(void) = {
    chimera_state_init,        // [0] spawn
    chimera_state_run,         // [1] AI driver
    chimera_hit_dispatch,      // [2] hit-reaction layer
    chimera_death_dispatch     // [3] death layer
};

// ============================================================================
// chimera_state_init @ 0x00438880
// One-shot spawn. behavior_flags 3 = scripted hard mode (reroll to 0/1 and
// latch +0x17E). Variants 0/1 start on the floor, variant >= 2 is parked at
// Y = -6008 hanging from the ceiling with rotation.z = 8. Health rolls three
// d3s (the first rand() is discarded) on top of 0x50.
// ============================================================================
static void chimera_state_init(void) // 0x00438880
{
    C_STATE_WORD = 1;                     // state 1, ignore 0
    C_BEH_WORD = 0;                       // behavior 0, action_state 0
    *(int*)&ENTITY->scaMatrixData.field_00 = 0;
    ENTITY->pad_c0[1] = 0;                // +0xC1
    C_TICKS = 0;
    ENTITY->death_timer = 0;
    ENTITY->hit_state = 0;
    C_HARD_MODE = 0;

    if (ENTITY->behavior_flags == 3) {
        C_HARD_MODE = 1;
        ENTITY->behavior_flags = (unsigned char)(rand() & 1);
    }
    if (ENTITY->behavior_flags < 2) {
        ((SVECTOR*)((char*)ENTITY + 0x72))->x = 0;   // rotation.x
        ENTITY->angle_z = 0;                          // rotation.z
        C_Y = 0;
    }
    if (ENTITY->behavior_flags > 1) {
        ((SVECTOR*)((char*)ENTITY + 0x72))->x = 0;   // rotation.x
        ENTITY->angle_z = 0x800;                      // rotation.z: 180 deg, upside down
        C_Y = (int)0xffffe818;                        // -6008, ceiling height
        ENTITY->behavior_flags = 2;
    }

    ResetJointTransforms();

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_animFrameIdSave = 0x808080;
    FUN_004565f0(&g_svecScratch, C_QUAD, 0, 0);

    rand();  // first roll discarded
    int r1 = rand();
    int r2 = rand();
    int r3 = rand();
    C_HEALTH = (short)(((unsigned)r2 & 7) * 2 + ((unsigned)r1 & 7) * 2 + ((unsigned)r3 & 7) * 2 + 0x50);

    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->animationId = ENTITY->behavior_flags;   // idle anim = variant
    ENTITY->pad_ca[0] = 0;
    ENTITY->pad_ca[1] = 0;
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1f);
    ENTITY->Sca_info = (unsigned int)(uintptr_t)chimera_sca_info_a;

    C_REPAUSE = 0;
    C_FADE_FREEZE = 0;
    C_WALL_FRAMES = 0;
    C_FAR_LATCH = 0;

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
}

// ============================================================================
// chimera_state_run @ 0x00438be0
// The per-frame AI driver. Bit 0x80 of behavior_flags freezes everything.
// While no behaviour layer owns the chimera the variant brain runs; the
// status/range prologue and the wall-stuck counters close the frame.
// ============================================================================
static void chimera_state_run(void) // 0x00438be0
{
    if ((ENTITY->behavior_flags & 0x80) != 0) {
        return;
    }

    if (ENTITY->ignore_player_flag == 0) {
        chimera_variant_ai();
        chimera_behavior_dispatch();
    } else if (ENTITY->ignore_player_flag == 1) {
        chimera_behavior_dispatch();
    }

    ENTITY->status_flags = (unsigned char)((ENTITY->status_flags & 0x1f) | 0x40);
    entity_check_visual_range(4000);

    if (ENTITY->behavior_flags != 0 && ENTITY->action_behavior == 0) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1f);
        entity_check_visual_range(4000);
    }
    if ((ENTITY->behavior_flags & 2) != 0) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1f);
        entity_check_alert_range(5000);
    }

    if (C_REPAUSE != 0) {
        C_REPAUSE = (short)(C_REPAUSE - 1);
    }

    if (C_WALL_HIT == 0) {
        C_WALL_FRAMES = 0;
        return;
    }
    C_WALL_FRAMES = (short)(C_WALL_FRAMES + 1);
}

// ============================================================================
// chimera_hit_dispatch @ 0x00438cb0 - state 2.
// Same seeding shape as the hunter's action layer: the behaviour comes from
// the seed table (hit_state & 7) plus the turn-toward-player bit, then the
// hit animation is picked and the shared stagger/knockdown drivers run.
// ============================================================================
static void chimera_hit_dispatch(void) // 0x00438cb0
{
    if (ENTITY->ignore_player_flag == 0) {
        unsigned char behavior = chimera_seed_tbl[ENTITY->hit_state & 7];
        unsigned int turn = turn_toward_target(CH_PLAYER_T, 0x400);
        behavior = (unsigned char)(behavior + ((turn >> 10) & 1));
        ENTITY->action_behavior = behavior;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_state = 0;
    }

    switch (ENTITY->action_behavior) {
    case 0:
        ENTITY->animationId = 0xd;
        chimera_hit_stagger();
        return;
    case 1:
        ENTITY->animationId = 0xc;
        chimera_hit_stagger();
        return;
    case 2:
    case 3:
        ENTITY->animationId = 0xf;
        if (ENTITY->behavior_flags != 0) {
            ENTITY->animationId = 6;
        }
        chimera_hit_stagger();
        return;
    case 4:
    case 5:
        ENTITY->animationId = 0xb;
        chimera_hit_knockdown();
        return;
    }
}

// ============================================================================
// chimera_death_dispatch @ 0x00438db0 - state 3.
// Behaviour is the turn bit alone (no seed table); ceiling variants
// (flags bit 1) always take behaviour 4, the drop-and-dissolve.
// ============================================================================
static void chimera_death_dispatch(void) // 0x00438db0
{
    if (ENTITY->ignore_player_flag == 0) {
        unsigned int turn = turn_toward_target(CH_PLAYER_T, 0x400);
        ENTITY->action_behavior = (unsigned char)((turn >> 10) & 1);
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_state = 0;
        if ((ENTITY->behavior_flags & 2) != 0) {
            ENTITY->action_behavior = 4;
        }
    }

    switch (ENTITY->action_behavior) {
    case 0:
        ENTITY->animationId = 0xb;
        chimera_death_dissolve();
        return;
    case 1:
        ENTITY->animationId = 0xc;
        chimera_death_dissolve();
        return;
    case 4:
        ENTITY->animationId = 0xb;
        chimera_death_drop_dissolve();
        return;
    }
}

// ============================================================================
// chimera_behavior_dispatch @ 0x00438e60
// The state 1 behaviour prologue: latch the pathfinder result (bit 0 of the
// +0x16E word), then dispatch through chimera_behavior_table on behaviour.
// ============================================================================
static void chimera_behavior_dispatch(void) // 0x00438e60
{
    unsigned char path = (unsigned char)entity_pathfind_update();
    g_animFrameIdSave = path;

    if ((path & 0xfe) == 0) {
        C_PATH_LATCH = (unsigned short)(C_PATH_LATCH & 0xfffe);
        C_PATH_LATCH = (unsigned short)(C_PATH_LATCH | (path & 1));
    }

    chimera_behavior_table[ENTITY->action_behavior]();
}

// ============================================================================
// chimera_variant_ai @ 0x00438eb0
// The pre-AI roll: manhattan distance to the player, then the per-variant
// brain. Entries 14-16 of chimera_behavior_table double as this table.
// ============================================================================
static void chimera_variant_ai(void) // 0x00438eb0
{
    int dz = CH_PLAYER_T_INT[2] - C_POS_T[2];
    int dx = CH_PLAYER_T_INT[0] - C_POS_T[0];
    g_playerDisplacement = (unsigned int)((dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx));

    chimera_brain_table[ENTITY->behavior_flags]();
}

// ============================================================================
// chimera_brain_a @ 0x00438f00 - the variant 0 decision layer (also behaviour
// 14). A confirmed path hands over to the walk; losing the player or getting
// stuck drops to the quick toggle (11); a player aiming a weapon from range
// rolls the swoop (7) or the claw (12); a knockdown blow takes the hit-fall
// (13); and hard mode with a pending repause toggles variants (5).
// ============================================================================
static void chimera_brain_a(void) // 0x00438f00
{
    unsigned char savedState = ENTITY->action_state;
    char prevBehavior = (char)ENTITY->action_behavior;

    if ((C_PATH_LATCH & 1) != 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 1;
        if (prevBehavior != 1) {
            ENTITY->animationId = 9;
            C_SPEED_W = 0xb4;
            ENTITY->action_state = 0;
        }
    }

    unsigned char inRange = checkAngularViewAndDistance(0x200, 0x5dc, CH_PLAYER_T);
    if (inRange == 0 || check_line_of_sight(CH_PLAYER_T) != 0 ||
        g_playerEntityPointer.isBeingAttackedFlag != 0) {
        if (((C_PATH_LATCH & 1) == 0 && g_playerDisplacement < 6000) ||
            0x14 < C_WALL_FRAMES) {
            ENTITY->ignore_player_flag = 1;
            C_BEH_WORD = 0xb;
            C_WALL_FRAMES = 0;
        }
        if (g_playerEntityPointer.action_behavior == 0x14 && 2000 < g_playerDisplacement &&
            is_facing_toward_entity(&g_playerEntityPointer) == 0 &&
            g_playerEntityPointer.action_state == 0) {
            ENTITY->ignore_player_flag = 1;
            C_BEH_WORD = (unsigned short)((rand() & 1) * 4 + 7);
            if (C_HARD_MODE != 0) {
                C_BEH_WORD = 0xb;
            }
        }
        if ((g_playerEntityPointer.isBeingAttackedFlag & 0x80) != 0) {
            ENTITY->ignore_player_flag = 1;
            C_BEH_WORD = 0xd;
        }
        return;
    }

    ENTITY->ignore_player_flag = 1;
    C_BEH_WORD = 7;
    if (C_HARD_MODE == 0) {
        if (is_facing_toward_entity(&g_playerEntityPointer) == 0) {
            return;
        }
        C_BEH_WORD = 0xc;
        return;
    }
    C_BEH_WORD = 0xc;
    if (C_REPAUSE == 0) {
        return;
    }
    ENTITY->action_behavior = 5;
    ENTITY->action_state = savedState;
    if (prevBehavior == 5) {
        return;
    }
    ENTITY->action_state = 0;
}

// ============================================================================
// chimera_brain_b @ 0x00439120 - the variant 1 decision layer (also behaviour
// 15). Same skeleton as brain_a but the walk hand-over keeps the run/step
// animation pair, the attack roll is a coin flip gated on range + line of
// sight, and losing the player hands over to the turn (2) with a distance-
// dependent step/animation.
// ============================================================================
static void chimera_brain_b(void) // 0x00439120
{
    char prevBehavior = (char)ENTITY->action_behavior;
    unsigned char savedState = ENTITY->action_state;

    if ((C_PATH_LATCH & 1) != 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 1;
        if (prevBehavior != 1) {
            ENTITY->action_state = 0;
            if (5000 < g_playerDisplacement) {
                C_FAR_LATCH = 1;
            }
            ENTITY->animationId = 3;
            C_SPEED_W = 100;
            if (C_FAR_LATCH != 0) {
                ENTITY->animationId = 2;
                C_SPEED_W = 200;
            }
        }
    }

    unsigned char inRange = checkAngularViewAndDistance(0x200, 0x5dc, CH_PLAYER_T);
    if (inRange != 0 && check_line_of_sight(CH_PLAYER_T) == 0 &&
        g_playerEntityPointer.isBeingAttackedFlag == 0 && (rand() & 1) != 0) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 8;
        if (is_facing_toward_entity(&g_playerEntityPointer) != 0) {
            C_BEH_WORD = 0xc;
        }
        if (C_HARD_MODE != 0) {
            if (C_REPAUSE == 0) {
                if (C_TOUCH_LATCH != 0) {
                    C_BEH_WORD = 8;
                }
            } else {
                ENTITY->action_behavior = 5;
                ENTITY->action_state = savedState;
                if (prevBehavior != 5) {
                    ENTITY->action_state = 0;
                }
            }
        }
        C_FAR_LATCH = 0;
        return;
    }

    if (((C_PATH_LATCH & 1) == 0 && g_playerDisplacement < 8000) || 0x14 < C_WALL_FRAMES) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 2;
        ENTITY->animationId = 3;
        C_TURN_STEP = 0x20;
        if (g_playerDisplacement < 4000 || C_FAR_LATCH != 0) {
            ENTITY->animationId = 2;
            C_TURN_STEP = 0x40;
        }
        C_WALL_FRAMES = 0;
        C_FAR_LATCH = 0;
    }

    if ((C_PATH_LATCH & 1) != 0) {
        if (is_facing_toward_entity(&g_playerEntityPointer) == 0 &&
            6000 < g_playerDisplacement) {
            int turn = turn_toward_target(CH_PLAYER_T, 0x40);
            if ((short)turn == 0 && g_playerEntityPointer.action_behavior == 0x13) {
                ENTITY->ignore_player_flag = 1;
                C_BEH_WORD = 0xb;
                C_FAR_LATCH = 0;
            }
        }
    }

    if ((g_playerEntityPointer.isBeingAttackedFlag & 0x80) != 0) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 0xd;
    }
}

// ============================================================================
// chimera_brain_c @ 0x00439400 - the variant 2 (ceiling) decision layer (also
// behaviour 16). The whole body runs with the yaw flipped by 180 degrees (the
// hanging chimera's model faces backwards), so every decision below sees the
// mirrored angle. Confirmed paths hand over to the flee (9); losing them or
// getting stuck starts the turn-back (10); close and unmolested it swipes
// (3) or drops (6); a knockdown blow takes the hit-fall (13).
// ============================================================================
static void chimera_brain_c(void) // 0x00439400
{
    char prevBehavior = (char)ENTITY->action_behavior;

    ENTITY->angle = (short)(ENTITY->angle + 0x800);

    if ((C_PATH_LATCH & 1) == 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 9;
        if (prevBehavior != 9) {
            ENTITY->action_state = 0;
            if (3000 < g_playerDisplacement) {
                C_FAR_LATCH = 1;
            }
        }
    }

    if ((C_PATH_LATCH & 1) != 0 || 0x1e < C_WALL_FRAMES) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 10;
        ENTITY->animationId = 3;
        C_TURN_STEP = 0x20;
        if (g_playerDisplacement < 4000 || C_FAR_LATCH != 0) {
            ENTITY->animationId = 2;
            C_TURN_STEP = 0x40;
        }
        C_WALL_FRAMES = 0;
        C_FAR_LATCH = 0;
    }

    if (C_REPAUSE == 0 && g_playerDisplacement < 2000 &&
        g_playerEntityPointer.isBeingAttackedFlag == 0) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 3;
        if (is_facing_toward_entity(&g_playerEntityPointer) != 0) {
            C_BEH_WORD = 6;
        }
        C_FAR_LATCH = 0;
    }

    if (15000 < g_playerDisplacement &&
        (short)turn_toward_target(CH_PLAYER_T, 0x200) == 0) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 6;
        C_FAR_LATCH = 0;
    }

    if ((g_playerEntityPointer.isBeingAttackedFlag & 0x80) != 0) {
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 0xd;
    }

    ENTITY->angle = (short)(ENTITY->angle - 0x800);
}

// ============================================================================
// chimera_behavior_table @ 0x004bb450 - indexed by Entity+0x86 while state 1.
// SEVENTEEN live entries; slots 14-16 are the three brains, which the
// variant roll reaches through chimera_brain_table below.
// ============================================================================
void (*const chimera_behavior_table[17])(void) = {
    chimera_behavior_idle,         // [0]  idle by variant
    chimera_behavior_walk,         // [1]  walk toward the player
    chimera_behavior_turn,         // [2]  timed turn toward the player
    chimera_behavior_swipe,        // [3]  double-claw swipe
    chimera_behavior_grabhold,     // [4]  grab and maul
    chimera_behavior_toggle,       // [5]  variant toggle with landing
    chimera_behavior_drop,         // [6]  ceiling drop attack
    chimera_behavior_swoop,        // [7]  ceiling swoop
    chimera_behavior_spit,         // [8]  acid spit
    chimera_behavior_flee,         // [9]  run away
    chimera_behavior_turn_back,    // [10] turn away from the player
    chimera_behavior_toggle_short, // [11] quick variant toggle
    chimera_behavior_claw,         // [12] claw with grab cancel
    chimera_hit_fall,              // [13] hit-reaction fall
    chimera_brain_a,               // [14] variant 0 brain
    chimera_brain_b,               // [15] variant 1 brain
    chimera_brain_c                // [16] variant 2 (ceiling) brain
};

// ============================================================================
// chimera_brain_table @ 0x004bb488 - indexed by behavior_flags from
// chimera_variant_ai. The SAME three pointers as behaviour slots 14-16.
// ============================================================================
void (*const chimera_brain_table[3])(void) = {
    chimera_brain_a,   // [0] 0x004bb488
    chimera_brain_b,   // [1] 0x004bb48c
    chimera_brain_c    // [2] 0x004bb490
};

// ============================================================================
// chimera_behavior_idle @ 0x004395e0 - behaviour 0.
// A 17-byte dispatcher: the idle body is picked per variant through the
// second table (the two floor variants share one body).
// ============================================================================
static void chimera_behavior_idle(void) // 0x004395e0
{
    chimera_idle_variant_table[ENTITY->behavior_flags]();
}

// ============================================================================
// chimera_idle_variant_table @ 0x004bb498 - indexed by behavior_flags from
// chimera_behavior_idle. Slot 3+ is NULL in the image (init rerolls variant 3
// before any behaviour can run).
// ============================================================================
void (*const chimera_idle_variant_table[3])(void) = {
    chimera_idle_floor,     // [0] 0x004bb498
    chimera_idle_floor,     // [1] 0x004bb49c
    chimera_idle_ceiling    // [2] 0x004bb4a0
};

// ============================================================================
// chimera_idle_floor @ 0x00439600 - variants 0/1 idle: play the variant's own
// animation until the AI layer takes over.
// ============================================================================
static void chimera_idle_floor(void) // 0x00439600
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = ENTITY->behavior_flags;
        ENTITY->blend_counter = 3;
    } else if (ENTITY->action_state != 1) {
        return;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// chimera_idle_ceiling @ 0x00439680 - variant 2 idle: animation 1 with a
// random timeout (shorter once a path to the player exists), then release
// back to the brain.
// ============================================================================
static void chimera_idle_ceiling(void) // 0x00439680
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 1;
        C_TICKS = (short)((rand() & 0x3f) + (1 - (C_PATH_LATCH & 1)) * 0x50);
        ENTITY->blend_counter = 3;
    } else if (ENTITY->action_state != 1) {
        return;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    short prev = C_TICKS;
    C_TICKS = (short)(C_TICKS - 1);
    if (prev == 0) {
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
    }
}

// ============================================================================
// chimera_behavior_walk @ 0x00439760 - behaviour 1.
// With a path: refresh the zone waypoint and steer at it; without one, turn
// at the player scaled by the (zero) path latch. Footstep sounds ride the
// animation id: 2 every 6 frames, 3 every 10, 9 every 15.
// ============================================================================
static void chimera_behavior_walk(void) // 0x00439760
{
    if ((C_PATH_LATCH & 1) == 0) {
        int turn = turn_toward_target(CH_PLAYER_T, 0x10);
        ENTITY->angle = (short)(ENTITY->angle + (short)turn * (int)(C_PATH_LATCH & 1));
    } else {
        zone_path_find(CH_PLAYER_T_INT[0], CH_PLAYER_T_INT[2],
                     (int*)&ENTITY->player_pos_x, (int*)&ENTITY->player_pos_z);
        g_playerPosScratch.x = ENTITY->player_pos_x;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = ENTITY->player_pos_z;
        entity_rotate_toward_target(&g_playerPosScratch, 0x40);
    }

    chimera_walk_timeout();
    Add_speedXZ(0);
    g_collPushDepthZHi = 0;

    if (ENTITY->animation_frame_id % 6 == 0 && ENTITY->animationId == 2) {
        Snd_em(0);   // original reads the constant back from 0x00be0dec
    }
    if (ENTITY->animation_frame_id % 10 == 0 && ENTITY->animationId == 3) {
        Snd_em(0);
    }
    if (ENTITY->animation_frame_id % 15 == 0 && ENTITY->animationId == 9) {
        Snd_em(0);
    }
}

// ============================================================================
// chimera_walk_timeout @ 0x004398a0
// Shared idle-timeout driver for the walk (1) and flee (9) behaviours: arms
// (rand & 0x1f) * behaviour frames (plus 0x50 for even behaviours) and
// releases back to the brain when it burns out.
// ============================================================================
static void chimera_walk_timeout(void) // 0x004398a0
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        C_TICKS = (short)((rand() & 0x1f) * (unsigned short)ENTITY->action_behavior);
        if (((unsigned char)ENTITY->action_behavior & 0xfe) == 0) {
            C_TICKS = (short)(C_TICKS + 0x50);
        }
    } else if (ENTITY->action_state != 1) {
        return;
    }
    // The original passes the PLAYER's isBeingAttackedFlag as Joint_move's
    // first parameter (the CONCAT31 in the decompiler is register reuse).
    Joint_move((char)g_playerEntityPointer.isBeingAttackedFlag,
               ENTITY->animHeader, ENTITY->animBase, 0x400);
    short prev = C_TICKS;
    C_TICKS = (short)(C_TICKS - 1);
    if (prev == 0) {
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
    }
}

// ============================================================================
// chimera_behavior_turn @ 0x00439980 - behaviour 2.
// Turn toward the player at +0x16C's step until the timer burns out or the
// chimera is aligned, then release (latching the far flag when a path
// exists) and refresh the waypoint pair.
// ============================================================================
static void chimera_behavior_turn(void) // 0x00439980
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->action_state = 1;
        C_TICKS = (short)((rand() & 0x1f) + 0x50);
    } else if (state != 1) {
        if (state != 2) {
            return;
        }
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
        if ((C_PATH_LATCH & 1) != 0) {
            C_FAR_LATCH = 1;
        }
        ENTITY->player_pos_x = (short)CH_PLAYER_T_INT[0];
        ENTITY->player_pos_z = (short)CH_PLAYER_T_INT[2];
        return;
    }

    int turn = turn_toward_target(CH_PLAYER_T, C_TURN_STEP);
    g_animFrameIdSave = (unsigned int)(short)turn;
    short prev = C_TICKS;
    C_TICKS = (short)(C_TICKS - 1);
    if (prev == 0 || (short)turn == 0) {
        ENTITY->action_state = 2;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->angle = (short)(ENTITY->angle + (short)(int)g_animFrameIdSave);
}

// ============================================================================
// chimera_behavior_swipe @ 0x00439ad0 - behaviour 3.
// The double-claw swipe. During frames 7-12 both claws (joint matrices at
// joints+0x32c / +0x51c) reach-test the player; a hit deals a flat 20
// damage, spawns the impact billboards on the claws that connected and tints
// them.
// ============================================================================
static void chimera_behavior_swipe(void) // 0x00439ad0
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 8;
        ENTITY->blend_counter = 3;
        Snd_em(3);
    } else if (state != 1) {
        if (state != 2) {
            return;
        }
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
        C_REPAUSE = 0x3c;
        return;
    }

    // States 0 AND 1 run the reach test (the init branch falls through).

    JointStruct* joints = ENTITY->jointsStructs;
    MATRIX* clawA = (MATRIX*)((char*)joints + 0x32c);
    MATRIX* clawB = (MATRIX*)((char*)joints + 0x51c);

    g_playerPosScratch.y = *(int*)(CH_DMV + 0x18);
    g_playerPosScratch.z = *(int*)(CH_DMV + 0x1c);
    g_playerPosScratch.pad = *(int*)(CH_DMV + 0x20);
    g_playerPosScratch.x = 100;

    player_distance_z = FUN_0048ae00(clawA, &g_playerPosScratch, 800, CH_PLAYER_T_INT);
    g_scaled_down_dist = FUN_0048ae00(clawB, &g_playerPosScratch, 800, CH_PLAYER_T_INT);

    if (check_line_of_sight(CH_PLAYER_T) == 0 &&
        g_playerEntityPointer.isBeingAttackedFlag == 0 &&
        (player_distance_z != 0 || g_scaled_down_dist != 0) &&
        (unsigned char)(ENTITY->animation_frame_id - 7) < 6) {
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -0x9d8;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0x200,
                               &g_playerEntityPointer.scaMatrixData.localMatrix,
                               &g_playerPosScratch, 0);
        Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH);   // result discarded on this path
        g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 0x14);

        unsigned int facing = is_facing_toward_entity(&g_playerEntityPointer);
        g_scaled_down_dist = facing & 0xff;
        g_playerEntityPointer.isBeingAttackedFlag = (unsigned char)(facing + 1);
        g_playerEntityPointer.action_behavior = (unsigned char)(facing + 0x66);
        Play3DSnd(3, g_playerEntityPointer.isBeingAttackedFlag, 0, (int)CH_PLAYER_T);
        Snd_em(5);

        chimera_seed_from_dead_move();
        if (player_distance_z != 0) {
            g_playerPosScratch.x = 100;
            Effect_CreateBillboard(0, 0, 0, clawA, &g_playerPosScratch, 0);
            g_playerPosScratch.x = 300;
            Effect_CreateBillboard(0, 0, 0, clawA, &g_playerPosScratch, 0);
            JointApplyColorTint((JointStruct*)((char*)joints + 0x2e8), 0, 0x40, (void*)0x90);
        }
        if (g_scaled_down_dist != 0) {
            g_playerPosScratch.x = 100;
            Effect_CreateBillboard(0, 0, 0, clawB, &g_playerPosScratch, 0);
            g_playerPosScratch.x = 300;
            Effect_CreateBillboard(0, 0, 0, clawB, &g_playerPosScratch, 0);
            JointApplyColorTint((JointStruct*)((char*)joints + 0x4d8), 0x90, 0x50, (void*)0x10);
        }
    }

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->action_state = 2;
    }
}

// ============================================================================
// chimera_behavior_grabhold @ 0x00439df0 - behaviour 4.
// The grab. State 0 snaps onto the player (both sides store the grab point,
// the player is forced into the grabbed pose); state 1 mauls - the bite at
// frame 0x22 deals 10/30 damage, and mashing buttons bleeds the +0x17C
// countdown into the player's attack direction; state 2 releases, toggling
// to variant 1 and chaining into the swoop (7), or in hard mode the quick
// toggle (5).
// ============================================================================
static void chimera_behavior_grabhold(void) // 0x00439df0
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->animation_frame_id = 10;
        ENTITY->timing_control = 0;
        ENTITY->action_state = 1;
        ENTITY->blend_counter = 0;
        ENTITY->angle = g_playerEntityPointer.directionAngle;
        C_Y = 0;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
        C_GRAB_MASH = 0x5a;
        ENTITY->animationId = 5;
        ENTITY->unk_c6 = (unsigned short)CH_PLAYER_T_INT[0];
        ENTITY->unk_c8 = (unsigned short)CH_PLAYER_T_INT[2];
        g_playerEntityPointer.unk_c8 = (unsigned short)CH_PLAYER_T_INT[2];
        g_playerEntityPointer.unk_c6 = (unsigned short)CH_PLAYER_T_INT[0];
        ENTITY->hit_state = 1;
        g_playerEntityPointer.action_behavior = 0;
        g_playerEntityPointer.action_state = 0;
        g_playerEntityPointer.isBeingAttackedFlag = 1;
        g_playerEntityPointer.animationId = 6;
        g_playerEntityPointer.animFrameId = 9;
    } else if (state != 1) {
        if (state != 2) {
            return;
        }
        ENTITY->hit_state = 0;
        ENTITY->behavior_flags = 1;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 7;
        ENTITY->action_state = 0;
        if (C_HARD_MODE == 0) {
            return;
        }
        ENTITY->action_behavior = 5;
        ENTITY->action_state = 0;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0xfd);
        C_REPAUSE = 0x5a;
        return;
    }

    // States 0 AND 1 run the maul body (the init branch falls through).
    entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->action_state = 2;
    }
    ENTITY->death_timer = (unsigned char)(ENTITY->death_timer + 1);

    if (ENTITY->animation_frame_id == 0x22) {
        g_playerPosScratch.x = 800;
        g_playerPosScratch.y = -0x71c;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0x200,
                               &g_playerEntityPointer.scaMatrixData.localMatrix,
                               &g_playerPosScratch, 0);
        Play3DSnd(3, 1, 0, (int)CH_PLAYER_T);
        Snd_em(6);
        JointStruct* joints = ENTITY->jointsStructs;
        chimera_seed_from_dead_move();
        Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x32c, &g_playerPosScratch, 0);
        JointApplyColorTint((JointStruct*)((char*)joints + 0x2e8), 0xa0, 0x50, (void*)0x10);
        if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 10);
        } else {
            g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 0x1e);
        }
    }

    short input = GetPlayerInputMasked();
    C_GRAB_MASH = (short)(C_GRAB_MASH + (input != 0 ? -3 : 0));
    g_playerEntityPointer.attackDirection = (unsigned short)C_GRAB_MASH;
    if (C_GRAB_MASH < 0) {
        C_GRAB_MASH = 0;
        g_playerEntityPointer.attackDirection = 0;
    }
}

// ============================================================================
// chimera_behavior_toggle @ 0x0043a0f0 - behaviour 5.
// The floor<->ceiling variant toggle with the full landing sequence. Floor
// chimeras crouch (anim 4), swap the variant bit, then play the landing
// (anim 7) with a dust billboard before waiting out the repause.
// ============================================================================
static void chimera_behavior_toggle(void) // 0x0043a0f0
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        if (ENTITY->behavior_flags != 0) {
            ENTITY->action_state = 3;
            return;
        }
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 4;
        Snd_em(0);
        // fallthrough
    case 1:
        if (Joint_move((char)ENTITY->behavior_flags,
                       ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->action_state = 2;
            return;
        }
        break;
    case 2:
        Snd_em(0);
        ENTITY->behavior_flags = (unsigned char)((ENTITY->behavior_flags + 1) & 1);
        ENTITY->action_state = 3;
        // fallthrough
    case 3:
        ENTITY->action_state = 4;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 7;
        g_playerPosScratch.x = 200;
        g_playerPosScratch.y = -0x654;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0x1f, 0, C_LOCAL_MATRIX, &g_playerPosScratch, 0);
        C_FAR_LATCH = 0;
        // fallthrough
    case 4: {
        int done = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + done);
        break;
    }
    case 5:
        ENTITY->action_state = 0;
        if (C_REPAUSE == 0) {
            C_STATE_WORD = 1;
            C_BEH_WORD = 0;
            return;
        }
        break;
    }
}

// ============================================================================
// chimera_behavior_drop @ 0x0043a2c0 - behaviour 6.
// The ceiling drop attack. Plays anim 0x10 REVERSED while descending the arc
// table backwards, then levels out (variant becomes 1) and slides toward the
// player; at frame 0x19 a facing, in-view, clear-line player is grabbed
// (behaviour 4). Frame 0x1f thuds.
// ============================================================================
static void chimera_behavior_drop(void) // 0x0043a2c0
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        ENTITY->animationId = 0x10;
        C_TICKS = 0;
        ENTITY->hit_state = 1;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
        // fallthrough
    case 1:
        C_Y = -chimera_arc_heights[32 - ENTITY->animation_frame_id];
        Joint_move(1, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = 2;
        return;
    case 2:
        break;
    case 3:
        goto descent;
    case 4:
        ENTITY->hit_state = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 3;
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0xfd);
        C_Y = 0;
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
        return;
    default:
        return;
    }

    // state 2: level out
    ENTITY->angle_z = 0;
    ENTITY->action_state = 3;
    ENTITY->blend_counter = 0;
    ENTITY->behavior_flags = 1;
    Snd_em(3);

descent:
    ENTITY->angle = (short)(ENTITY->angle +
                            (short)turn_toward_target(CH_PLAYER_T, 0x20));
    if (ENTITY->animation_frame_id == 0x19 &&
        checkAngularViewAndDistance(0x400, 2000, CH_PLAYER_T) != 0 &&
        check_line_of_sight(CH_PLAYER_T) == 0 &&
        g_playerEntityPointer.isBeingAttackedFlag == 0 &&
        is_facing_toward_entity(&g_playerEntityPointer) != 0) {
        ENTITY->action_behavior = 4;
        ENTITY->action_state = 0;
        ENTITY->angle = (short)getAngleTowardsTarget(CH_PLAYER_T_INT[0], CH_PLAYER_T_INT[2]);
        g_playerEntityPointer.isBeingAttackedFlag = 1;
        ENTITY->hit_state = 0;
        return;
    }
    C_Y = -chimera_arc_heights[32 - ENTITY->animation_frame_id];
    C_SPEED_W = (unsigned short)chimera_drop_speed[ENTITY->animation_frame_id];
    Add_speedXZ(0x800);
    int done = Joint_move(1, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->action_state = (unsigned char)(ENTITY->action_state + done);
    if (ENTITY->animation_frame_id == 0x1f) {
        Snd_em(4);
    }
}

// ============================================================================
// chimera_behavior_swoop @ 0x0043a5b0 - behaviour 7.
// The ceiling swoop: anim 0x10 forward along the arc (rise to the 4700+
// plateau with the 5068 launch burst), then at frame 0x20 tilt (rotation.z
// 8), flip back to variant 2 and climb home (anim back to 1, Y -6008).
// ============================================================================
static void chimera_behavior_swoop(void) // 0x0043a5b0
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        ENTITY->animationId = 0x10;
        C_TICKS = 0;
        ENTITY->hit_state = 1;
        Snd_em(1);
        // fallthrough
    case 1:
        C_Y = -chimera_arc_heights[ENTITY->animation_frame_id];
        C_SPEED_W = (unsigned short)chimera_swoop_speed[ENTITY->animation_frame_id];
        Add_speedXZ(0);
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (ENTITY->animation_frame_id == 0x20) {
            ENTITY->action_state = 2;
            return;
        }
        break;
    case 2:
        C_Y = -chimera_arc_heights[ENTITY->animation_frame_id];
        ENTITY->angle_z = 0x800;   // bytes {0x76=0, 0x77=8}: flip upside down again
        ENTITY->action_state = 3;
        ENTITY->blend_counter = 0;
        ENTITY->behavior_flags = 2;
        Snd_em(2);
        // fallthrough
    case 3: {
        int done = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + done);
        break;
    }
    case 4:
        ENTITY->hit_state = 0;
        C_REPAUSE = 0x5a;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0xfd);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 1;
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
        C_Y = (int)0xffffe818;   // -6008, back on the ceiling
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
        return;
    }
}

// ============================================================================
// chimera_behavior_spit @ 0x0043a7e0 - behaviour 8.
// The acid spit (anim 0xe). During frames 10-12 an in-view, clear-line,
// unmolested player takes 15 (or 20 on hard) damage plus the impact
// billboard and a blue-ish tint on the head joint.
// ============================================================================
static void chimera_behavior_spit(void) // 0x0043a7e0
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0xe;
        ENTITY->blend_counter = 3;
        Snd_em(3);
    } else if (state != 1) {
        if (state != 2) {
            return;
        }
        if (C_HARD_MODE != 0) {
            C_REPAUSE = 0x28;
        }
        ENTITY->behavior_flags = 0;
        C_STATE_WORD = 1;
        C_BEH_WORD = 0;
        return;
    }

    // States 0 AND 1 run the spit reach test (the init branch falls through).
    JointStruct* joints = ENTITY->jointsStructs;
    if (checkAngularViewAndDistance(0x200, 2000, CH_PLAYER_T) != 0) {
        if (check_line_of_sight(CH_PLAYER_T) == 0 &&
            g_playerEntityPointer.isBeingAttackedFlag == 0 &&
            (unsigned char)(ENTITY->animation_frame_id - 10) < 3) {
            g_playerPosScratch.x = 0;
            g_playerPosScratch.y = -0x9d8;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 0, 0x200,
                                   &g_playerEntityPointer.scaMatrixData.localMatrix,
                                   &g_playerPosScratch, 0);
            if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 15);
            } else {
                g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 20);
            }
            unsigned int facing = is_facing_toward_entity(&g_playerEntityPointer);
            g_scaled_down_dist = facing & 0xff;
            g_playerEntityPointer.isBeingAttackedFlag = (unsigned char)(facing + 1);
            g_playerEntityPointer.action_behavior = (unsigned char)(facing + 0x66);
            Play3DSnd(3, 1, 0, (int)CH_PLAYER_T);
            Snd_em(6);
            chimera_seed_from_dead_move();
            Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x32c, &g_playerPosScratch, 0);
            JointApplyColorTint((JointStruct*)((char*)joints + 0x2e8), 0x10, 0x20, (void*)0x60);
        }
    }

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->action_state = 2;
    }
}

// ============================================================================
// chimera_behavior_flee @ 0x0043aa10 - behaviour 9.
// Run away: the yaw is flipped 180 degrees so the walk steering (and the
// turn_toward_target call) drives it AWAY from the player, then flipped
// back. Speed/animation depend on the far latch.
// ============================================================================
static void chimera_behavior_flee(void) // 0x0043aa10
{
    ENTITY->animationId = 3;
    C_SPEED_W = 100;
    if (C_FAR_LATCH != 0) {
        ENTITY->animationId = 2;
        C_SPEED_W = 200;
    }
    ENTITY->angle = (short)(ENTITY->angle + 0x800);
    int turn = turn_toward_target(CH_PLAYER_T, 0x10);
    ENTITY->angle = (short)(ENTITY->angle +
                            (short)turn * (int)(C_PATH_LATCH & 1));
    ENTITY->angle = (short)(ENTITY->angle - 0x800);

    chimera_walk_timeout();
    Add_speedXZ(0x800);
    g_collPushDepthZHi = 2;
    if (ENTITY->animation_frame_id == 0 && ENTITY->animationId == 2) {
        Snd_em(2);
    }
}

// ============================================================================
// chimera_behavior_turn_back @ 0x0043aae0 - behaviour 10.
// Timed turn executed with the yaw flipped (like the flee), so it turns its
// BACK to the player; releases and refreshes the waypoints when aligned.
// ============================================================================
static void chimera_behavior_turn_back(void) // 0x0043aae0
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->action_state = 1;
        C_TICKS = 0x50;
    } else if (state != 1) {
        if (state != 2) {
            return;
        }
        ENTITY->ignore_player_flag = 0;
        C_BEH_WORD = 0;
        ENTITY->player_pos_x = (short)CH_PLAYER_T_INT[0];
        ENTITY->player_pos_z = (short)CH_PLAYER_T_INT[2];
        return;
    }

    ENTITY->angle = (short)(ENTITY->angle + 0x800);
    int turn = turn_toward_target(CH_PLAYER_T, C_TURN_STEP);
    g_animFrameIdSave = (unsigned int)(short)turn;
    short prev = C_TICKS;
    C_TICKS = (short)(C_TICKS - 1);
    if (prev == 0 || (short)turn == 0) {
        ENTITY->action_state = 2;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->angle = (short)(ENTITY->angle + (short)(int)g_animFrameIdSave);
    ENTITY->angle = (short)(ENTITY->angle - 0x800);
}

// ============================================================================
// chimera_behavior_toggle_short @ 0x0043ac20 - behaviour 11.
// The quick variant toggle: crouch (anim 4), swap the variant bit, done -
// no landing animation.
// ============================================================================
static void chimera_behavior_toggle_short(void) // 0x0043ac20
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 4;
        ENTITY->blend_counter = 3;
        Snd_em(0);
        return;
    }
    if (state != 1) {
        if (state != 2) {
            return;
        }
        Snd_em(0);
        ENTITY->behavior_flags = (unsigned char)((ENTITY->behavior_flags + 1) & 1);
        C_STATE_WORD = 1;
        C_BEH_WORD = 0;
        return;
    }
    if (Joint_move((char)ENTITY->behavior_flags,
                   ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->action_state = 2;
    }
}

// ============================================================================
// chimera_behavior_claw @ 0x0043ad00 - behaviour 12.
// The standing claw swipe (anim 0xa). At frame 10 a facing, in-view,
// clear-line, unmolested player is grabbed (behaviour 4) unless the wall-
// push bit is set; in hard mode merely touching (+0x170) at frame 10 deals
// 5/10 damage. Frame 0x17 thuds.
// ============================================================================
static void chimera_behavior_claw(void) // 0x0043ad00
{
    unsigned char state = ENTITY->action_state;
    if (state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 10;
        ENTITY->blend_counter = 3;
        Snd_em(1);
    } else if (state != 1) {
        if (state != 2) {
            return;
        }
        ENTITY->behavior_flags = 1;
        C_STATE_WORD = 1;
        C_BEH_WORD = 0;
        return;
    }

    // States 0 AND 1 run the swipe body (the init branch falls through).
    ENTITY->angle = (short)(ENTITY->angle +
                            (short)turn_toward_target(CH_PLAYER_T, 0x20) *
                                (int)(C_PATH_LATCH & 1));

    if (ENTITY->animation_frame_id == 10) {
        if (checkAngularViewAndDistance(0x200, 2000, CH_PLAYER_T) != 0 &&
            check_line_of_sight(CH_PLAYER_T) == 0 &&
            g_playerEntityPointer.isBeingAttackedFlag == 0) {
            if (is_facing_toward_entity(&g_playerEntityPointer) != 0) {
                ENTITY->collisionFlags = (unsigned char)(ENTITY->collisionFlags & 8);
                if (ENTITY->collisionFlags == 0) {
                    C_BEH_WORD = 4;
                    ENTITY->angle =
                        (short)getAngleTowardsTarget(CH_PLAYER_T_INT[0], CH_PLAYER_T_INT[2]);
                    g_playerEntityPointer.isBeingAttackedFlag = 1;
                    ENTITY->hit_state = 1;
                    return;
                }
            }
        }
    }

    if (C_HARD_MODE != 0 && C_TOUCH_LATCH != 0 &&
        ENTITY->animation_frame_id == 10 &&
        g_playerEntityPointer.isBeingAttackedFlag == 0) {
        if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 5);
        } else {
            g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 10);
        }
        unsigned int facing = is_facing_toward_entity(&g_playerEntityPointer);
        g_scaled_down_dist = facing & 0xff;
        g_playerEntityPointer.isBeingAttackedFlag = (unsigned char)(facing + 1);
        g_playerEntityPointer.action_behavior = (unsigned char)(facing + 0x66);
        Play3DSnd(3, 0, 0, (int)CH_PLAYER_T);
    }

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->action_state = 2;
    }
    Add_speedXZ(0);
    if (ENTITY->animation_frame_id == 0x17) {
        Snd_em(4);
    }
}

// ============================================================================
// chimera_hit_fall @ 0x0043af60 - behaviour 13.
// The knockdown hit reaction: plays the variant's death animation table
// entry (both live variants use anim 6) once through.
// ============================================================================
static void chimera_hit_fall(void) // 0x0043af60
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = chimera_death_anim_tbl[ENTITY->behavior_flags];
        Snd_em(8);
    } else if (ENTITY->action_state != 1) {
        return;
    }
    // First parameter is the PLAYER's isBeingAttackedFlag (register reuse in
    // the original - see chimera_walk_timeout). State 0 falls through here.
    Joint_move((char)g_playerEntityPointer.isBeingAttackedFlag,
               ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// chimera_hit_stagger @ 0x0043aff0
// The state 2 stagger driver (hit behaviours 0-3). Plays the hit animation
// with a shove (Add_speedXZ on the behaviour parity); wounded chimeras that
// have never taken a heavy hit burst acid. On release it may chain straight
// into the swoop (7), or the claw (12) in hard mode.
// ============================================================================
static void chimera_hit_stagger(void) // 0x0043aff0
{
    JointStruct* joints = ENTITY->jointsStructs;
    unsigned char state = ENTITY->action_state;

    if (state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        C_SPEED_W = 0xa0;
        Snd_em(7);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x1b8, &g_playerPosScratch, 0);

        int hard = Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH);
        int wounded;
        if (hard == 0) {
            if (0x1d < C_HEALTH) {
                goto run;
            }
        } else {
            if (0x13 < C_HEALTH) {
                goto run;
            }
        }
        wounded = ENTITY->hit_state;
        if ((wounded & 1) == 0) {
            chimera_acid_burst();
        }
    } else if (state != 1) {
        // The run body above bumps action_state 1 -> 2 when the animation
        // finishes; only that state releases. State 1 must fall through to
        // `run` below (the original jumps state 1 straight to the run body).
        if (state != 2) {
            return;
        }
        ENTITY->hit_state = 0;
        C_STATE_WORD = 1;
        C_BEH_WORD = 0;
        if ((rand() & 1) == 0) {
            return;
        }
        ENTITY->ignore_player_flag = 1;
        C_BEH_WORD = 7;
        if (C_HARD_MODE == 0) {
            return;
        }
        C_BEH_WORD = 0xc;
        return;
    }

run:
    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + 1);
    }
    if ((unsigned char)ENTITY->action_behavior < 2) {
        if (ENTITY->animation_frame_id > 9) {
            C_SPEED_W = 0x3c;
        }
        Add_speedXZ((1 - (unsigned int)ENTITY->action_behavior) * 0x800);
    }
    if (ENTITY->animation_frame_id == 10) {
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x1b8, &g_playerPosScratch, 0);
    }
    neptune_clear_hit_state();
}

// ============================================================================
// chimera_hit_knockdown @ 0x0043b1f0
// The state 2 knockdown driver (hit behaviours 4/5). Drops the chimera out
// of the air along the arc table backwards, lands it (variant becomes 1),
// holds it grounded for a health-scaled timer, then recovers through anim
// 0x11 (flipping around at frame 0x16) and may chain into the swoop (7).
// ============================================================================
static void chimera_hit_knockdown(void) // 0x0043b1f0
{
    JointStruct* joints = ENTITY->jointsStructs;

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        ENTITY->animationId = 0x10;
        C_TICKS = 0;
        // fallthrough
    case 1:
        C_Y = -chimera_arc_heights[32 - ENTITY->animation_frame_id];
        Joint_move(1, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = 2;
        return;
    case 2:
        C_Y = C_Y + 1000;
        ENTITY->angle_z = 0;
        ENTITY->action_state = 3;
        ENTITY->blend_counter = 0;
        ENTITY->behavior_flags = 1;
        C_TICKS = (short)(((0x82 - C_HEALTH) >> 2) + 0x14);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x1b8, &g_playerPosScratch, 0);
        {
            int hard = Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH);
            if (hard == 0) {
                if (C_HEALTH < 0x1e) {
                    if ((ENTITY->hit_state & 1) == 0) {
                        chimera_acid_burst();
                    }
                }
            } else if (C_HEALTH < 0x14) {
                if ((ENTITY->hit_state & 1) == 0) {
                    chimera_acid_burst();
                }
            }
        }
        // fallthrough
    case 3:
        C_Y = C_Y + ((ENTITY->animation_frame_id * 5 + 10) * 10);
        if (C_Y > 0) {
            C_Y = 0;
            if (ENTITY->hit_state > 1) {
                Snd_em(4);
                ENTITY->hit_state = 1;
            }
        }
        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->action_state = 4;
        }
        break;
    case 4: {
        short prev = C_TICKS;
        C_TICKS = (short)(C_TICKS - 1);
        if (prev == 0) {
            ENTITY->action_state = 5;
        }
        break;
    }
    case 5:
        ENTITY->action_state = 6;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        C_TICKS = 0;
        // fallthrough
    case 6:
        ENTITY->animationId = 0x11;
        if (ENTITY->animation_frame_id == 0x16) {
            ENTITY->angle = (short)(ENTITY->angle + 0x800);
        }
        {
            int done = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
            ENTITY->action_state = (unsigned char)(ENTITY->action_state + done);
        }
        return;
    case 7:
        ENTITY->hit_state = 0;
        C_STATE_WORD = 1;
        C_BEH_WORD = 0;
        if ((rand() & 1) != 0) {
            ENTITY->ignore_player_flag = 1;
            C_BEH_WORD = 7;
        }
        break;
    }
}

// ============================================================================
// chimera_death_dissolve @ 0x0043b520
// The ground death (death behaviours 0/1). Plays the death animation while
// sliding, then the body dissolves: the shadow quad is tinted yellow-green
// and grown from zero over 60 frames before the room death event fires.
// ============================================================================
static void chimera_death_dissolve(void) // 0x0043b520
{
    JointStruct* joints = ENTITY->jointsStructs;

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        C_SPEED_W = 0xa0;
        C_TICKS = 0x3c;
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x1b8, &g_playerPosScratch, 0);
        Snd_em(8);
        chimera_acid_burst();
        // fallthrough
    case 1:
        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->action_state = (unsigned char)(ENTITY->action_state + 1);
        }
        if (ENTITY->animation_frame_id > 9) {
            C_SPEED_W = 0x3c;
        }
        Add_speedXZ((1 - (unsigned int)ENTITY->action_behavior) * 0x800);
        if (ENTITY->animation_frame_id == 0xf && ENTITY->action_behavior != 0) {
            ENTITY->action_state = 2;
            return;
        }
        break;
    case 2:
        ENTITY->action_state = 3;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 8);
        C_FADE_FREEZE = 1;
        BillboardSetColor(C_QUAD, 1, 2, 0x00ffff50);
        BillboardSetSize(C_QUAD, 0, 0);
        // fallthrough
    case 3: {
        BillboardAdjSize(C_QUAD, 0x16, 0x16);
        short prev = C_TICKS;
        C_TICKS = (short)(C_TICKS - 1);
        if (prev == 0) {
            ENTITY->action_state = 4;
            Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        }
        break;
    }
    }
}

// ============================================================================
// chimera_death_drop_dissolve @ 0x0043b710
// The air death (death behaviour 4, ceiling/knocked-down chimeras). Falls
// along the arc table backwards, lands (variant becomes 1), then dissolves
// exactly like chimera_death_dissolve.
// ============================================================================
static void chimera_death_drop_dissolve(void) // 0x0043b710
{
    JointStruct* joints = ENTITY->jointsStructs;

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        ENTITY->animationId = 0x10;
        C_TICKS = 0x3c;
        Snd_em(8);
        // fallthrough
    case 1:
        C_Y = -chimera_arc_heights[32 - ENTITY->animation_frame_id];
        Joint_move(1, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = 2;
        return;
    case 2:
        C_Y = C_Y + 1000;
        ENTITY->angle_z = 0;
        ENTITY->action_state = 3;
        ENTITY->blend_counter = 0;
        ENTITY->behavior_flags = 1;
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 0, 0, (char*)joints + 0x1b8, &g_playerPosScratch, 0);
        chimera_acid_burst();
        // fallthrough
    case 3:
        C_Y = C_Y + ((ENTITY->animation_frame_id * 5 + 10) * 10);
        if (C_Y > 0) {
            C_Y = 0;
            if (ENTITY->hit_state > 1) {
                Snd_em(4);
                ENTITY->hit_state = 1;
            }
        }
        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->action_state = 4;
            ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
            ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 8);
            C_FADE_FREEZE = 1;
            BillboardSetColor(C_QUAD, 1, 2, 0x00ffff50);
            BillboardSetSize(C_QUAD, 0, 0);
        }
        break;
    case 4: {
        BillboardAdjSize(C_QUAD, 0x18, 0x18);
        short prev = C_TICKS;
        C_TICKS = (short)(C_TICKS - 1);
        if (prev == 0) {
            ENTITY->action_state = 5;
            Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
            return;
        }
        break;
    }
    }
}

// ============================================================================
// chimera_acid_burst @ 0x0043bce0
// The acid/blood burst the wounded stagger and every death plays: two
// billboards on the torso joint (joints+0xc0), then a fan of five type 0x1d
// billboards around the entity matrix, yawed away from the player's facing.
// ============================================================================
static void chimera_acid_burst(void) // 0x0043bce0
{
    void* spriteInfo = (char*)ENTITY->jointsStructs + 0xc0;

    g_playerPosScratch.x = 0;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = 0;
    Effect_CreateBillboard(0, 0, 0, spriteInfo, &g_playerPosScratch, 0);
    Effect_CreateBillboard(3, 0, 0, spriteInfo, &g_playerPosScratch, 0);

    g_playerPosScratch.x = 100;
    g_playerPosScratch.y = -0x5dc;
    g_playerPosScratch.z = 0;

    unsigned char depthGroup = 4;
    do {
        short yaw = (short)(((depthGroup + 5) * 0x100 - ENTITY->angle) +
                            g_playerEntityPointer.directionAngle);
        Effect_CreateBillboard(0x1d, depthGroup, yaw, C_LOCAL_MATRIX,
                               &g_playerPosScratch, 0);
        depthGroup = (unsigned char)(depthGroup - 1);
    } while (depthGroup != 0);
}
