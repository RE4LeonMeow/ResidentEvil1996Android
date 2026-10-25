// Hunter.cpp - Hunter (entity type 6, enemy/em1006.emd)
//
// Original PC addresses:
//   hunter_update               0x004161f0  per-frame entry, dispatch table [6]
//   hunter_sca_info_a           0x004b47e0  SCA record (normal)
//   hunter_sca_info_b           0x004b47f0  SCA record (hard variant / stage > 4)
//   hunter_health_tbl           0x004b4820  16-entry health roll
//   hunter_pounce_chance_mid    0x004b4830  16-entry pounce chance (health < 0x4f)
//   hunter_pounce_chance_low    0x004b4840  16-entry pounce chance (health < 0x28)
//   hunter_player_health_gate   0x004b4860  {105, 72} indexed by player id & 1
//   hunter_state_table          0x004b4868  FIVE entries + 3 NULL pad
//   hunter_behavior_seed_tbl    0x004b488f  8 bytes, indexed by hit_state & 7
//   hunter_act_table            0x004b4898  NINE entries (state 2 action layer)
//   hunter_death_table          0x004b48c0  SEVEN entries + NULL pad
//   hunter_behavior_table       0x004b48e0  TWELVE entries (state 1 AI layer)
//   hunter_variant_table        0x004b4910  EIGHT entries (per behavior_flags)
//   hunter_idle_variant_table   0x004b4950  FOUR entries
//   hunter_attack_sub_table     0x004b4970  FIVE entries + NULL pad
//   hunter_pounce_sub_table     0x004b4988  FIVE entries + NULL pad
//   hunter_dodge_sub_table      0x004b49c0  FIVE entries + NULL pad
//   hunter_intro_anim_tbl       0x004b49c8  stride-2 BYTE reads (see below)
//   hunter_intro_angle_tbl      0x004b49cc  stride-2 WORD reads (see below)
//   hunter_intro_move_tbl       0x004b49d8  short pairs, indexed by jump kind
//   hunter_intro_jump_kind      0x004b49f0  int, stage-selected (behavior 11)
//   hunter_scream_latch         0x00d91bc4  byte, scream one-shot latch
//   hunter_scd_target           0x004d3d50  Entity*, set by the SCD dispatcher
//   hunter_scd_table            0x004d3d58  THIRTY-NINE entries (state 8);
//                                           slots 10-13 are wrappers that
//                                           re-enter the SAME table at bases
//                                           [24]/[28]/[36]/[32] indexed by
//                                           action_state (+0x87)
//
// ---------------------------------------------------------------------------
// Shape of the AI - three stacked dispatch layers
// ---------------------------------------------------------------------------
//   Entity+0x84 state     0 init / 1 AI driver / 2 action layer /
//                         3 death / 4 RET / 8 SCD-script-controlled.
//                         hunter_update forces state 8 whenever behavior_flags
//                         bit 0x40 is set (event-driven appearances) and the
//                         state is not already 0.
//   Entity+0x85 flag      1 = the action layer owns the hunter; the AI layer
//                         only runs while it is 0.
//   Entity+0x86 behavior  shared by BOTH upper layers: while state 1 it indexes
//                         hunter_behavior_table, while state 2 it indexes
//                         hunter_act_table. Values are set by the AI decision
//                         code and cleared by the action code, so the same
//                         number means different things per layer.
//   Entity+0x87 action    per-behaviour sub-state.
//
// The behavior_flags byte (+0x02) doubles as the VARIANT selector (low nibble,
// 0-7) and carries control bits: 0x02 "hard", 0x08 "scripted intro", 0x10
// "coward", 0x20 "late-game SCA", 0x40 "SCD-controlled", 0x80 "skip update".
//
// ---------------------------------------------------------------------------
// Ghidra over-read traps in this block
// ---------------------------------------------------------------------------
// - The state 1 jump `JMP [ECX*4 + 0x4b48e0]` has TWELVE live entries. Ghidra's
//   switch recovery rendered FIFTEEN cases: cases 12-14 are the first three
//   entries of the separate hunter_variant_table at 0x004b4910, read through
//   the wrong base. Count readers, not NULL runs.
// - hunter_behavior_idle_dispatch (0x00417110) jumps with
//   `JMP [ECX*4 + 0x4b4950]` on variant: the table spans BOTH 4-entry halves
//   at 0x004b4950 and 0x004b4960. The second half has no literal xrefs of its
//   own, so it looks like dead data in Ghidra - it is not (crash on stage 5
//   room 10 proved it). Ghidra also merged the dispatcher with the bodies.
// - The intro tables at 0x004b49c8/0x004b49cc are read with a stride of TWO
//   (`byte [ECX*2 + 0x4b49c8]`, `word [EDX*2 + 0x4b49cc]`), which walks them
//   straight through the middle of hunter_dodge_sub_table's pointer storage.
//   The arrays below are pre-strided: index [n] holds the value the original
//   reads for n, so the overlap is preserved without pointer casts.
//
// All original addresses from Ghidra.
// ============================================================================
#include "EntityCommon.h"
#include "../../Globals.h"
#include "../BioCard.h"
#include <cstdlib>
#include <cstring>

extern void ResetJointTransforms(void);                            // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);           // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90 - Room.cpp
// 0x0043d8a0 - entity-generic heavy-weapon flinch-cancel, named for the zombie.
// hunter_act_swipe reaches it as a CALL at the end of the swipe recovery.
extern void zombie_check_special_weapon(void);                     // 0x0043d8a0

// 0x004bd2b0 - shared one-shot flag, defined in Globals.cpp. The pounce grab
// sets it to 1 (hunter_pounce_track); the death screen consumes and clears it
// while hiding the player's head joint. Image value 0x00006C6C.
extern int DAT_004bd2b0;                                           // 0x004bd2b0

// 0x00be0de4 / 0x00be0de8 - shared entity scratch, defined in EntityCommon.cpp.
extern int player_distance_z;                                      // 0x00be0de4
extern int g_scaled_down_dist;                                     // 0x00be0de8

// ============================================================================
// Offset accessors. The port's Entity field names come from the zombie's point
// of view; the hunter reuses several at other widths, so they are reached by
// offset per the "offset writes, not nearest field" rule.
// ============================================================================
#define H_STATE_BLOCK   (*(unsigned int*)  ((char*)ENTITY + 0x84))
#define H_STATE_WORD    (*(unsigned short*)((char*)ENTITY + 0x84)) // state+ignore
#define H_BEH_WORD      (*(unsigned short*)((char*)ENTITY + 0x86)) // behavior+action
#define H_HEALTH        (*(short*)         ((char*)ENTITY + 0x88))
#define H_JOINTS        ((JointStruct*)    ENTITY->jointsStructs)
#define H_SPEED_W       (*(unsigned short*)((char*)ENTITY + 0xC2))
#define H_TICKS         (*(short*)         ((char*)ENTITY + 0xC4))
#define H_POS           ((VECTOR*)         ((char*)ENTITY + 0x34))
#define H_POS_T         ((int*)            ((char*)ENTITY + 0x34))
#define H_ROT           ((SVECTOR*)        ((char*)ENTITY + 0x72))
#define H_LOCAL_MATRIX  ((MATRIX*)         ((char*)ENTITY + 0x20))
#define H_PARTNER       (*(Entity**)((char*)ENTITY + 0x174))       // +0x174 dword
#define H_PATH_LATCH    (*(short*) ((char*)ENTITY + 0x170))        // port: angle_turn_delta/move_timer
#define H_GRAB_WORD     (*(short*) ((char*)ENTITY + 0x172))        // port: is_moving/move_max_steps
#define H_TARGET_X      (*(short*) ((char*)ENTITY + 0x178))
#define H_TARGET_Z      (*(short*) ((char*)ENTITY + 0x17A))
#define H_JOINT_SEL     (*(unsigned short*)((char*)ENTITY + 0x17C)) // port: action_speed/hit_threshold
#define H_STEP_WORD     (*(unsigned short*)((char*)ENTITY + 0x17E)) // port: behavior_step/action_counter
#define H_ROOM_HIT      (*(unsigned char*)((char*)ENTITY + 0x180))
#define H_POUNCE_LATCH  (*(unsigned char*)((char*)ENTITY + 0x182))
#define H_DEATH_CNT_A   (*(unsigned char*)((char*)ENTITY + 0x183))
#define H_DEATH_CNT_B   (*(unsigned char*)((char*)ENTITY + 0x184))
#define H_STRAFE_DIR    (*(unsigned char*)((char*)ENTITY + 0x185))
#define H_APPROACH_CNT  (*(unsigned char*)((char*)ENTITY + 0x187))
#define H_POISE         (*(unsigned char*)((char*)ENTITY + 0x188))
#define H_REPAUSE       (*(unsigned char*)((char*)ENTITY + 0x189))
#define H_LEAP_FLAG     (*(unsigned char*)((char*)ENTITY + 0x18A))

// The player's position, as the original addresses it (0x00be6318).
#define PLAYER_T        ((VECTOR*) g_playerEntity.scaMatrixData.localMatrix.t)
#define PLAYER_T_INT    ((int*)    g_playerEntity.scaMatrixData.localMatrix.t)

// g_deadMoveValue is a DWORD holding an address; +0x14 is the position block
// every effect spawn seeds from.
#define H_DMV           ((const char*)g_deadMoveValue)

// ============================================================================
// hunter_sca_info_a @ 0x004b47e0 / hunter_sca_info_b @ 0x004b47f0
// One six-short SCA record each: [0] 0x8000 id/terminator, [1..3] local box,
// [4] half-width, [5] radius. The b record (radius 500 instead of 900) is the
// late-game/hard variant selected in hunter_state_init. check_room_collision
// reads the radius straight out of Sca_info + 10, and hunter_update passes
// the same word back in.
// ============================================================================
const short hunter_sca_info_a[8] = { (short)0x8000, 0, (short)-180, 0, 180, 900, 0, 0 };
const short hunter_sca_info_b[8] = { (short)0x8000, 0, (short)-180, 0, 180, 500, 0, 0 };

// ============================================================================
// hunter_health_tbl @ 0x004b4820 - index = rand() & 0xf. 0x5F = 95, 0x4F = 79,
// 0x6F = 111.
// ============================================================================
const unsigned char hunter_health_tbl[16] = {
    0x5F, 0x5F, 0x5F, 0x5F, 0x4F, 0x5F, 0x4F, 0x6F,
    0x5F, 0x5F, 0x4F, 0x5F, 0x5F, 0x4F, 0x5F, 0x5F
};

// ============================================================================
// hunter_pounce_chance_low @ 0x004b4840 (health word < 0x28) and
// hunter_pounce_chance_mid @ 0x004b4830 (health word < 0x4f). A 1 rolls the
// pounce latch; index = rand() & 0xf. The LOW table is checked FIRST.
// ============================================================================
const unsigned char hunter_pounce_chance_low[16] = {
    0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1
};
const unsigned char hunter_pounce_chance_mid[16] = {
    1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1
};

// ============================================================================
// hunter_player_health_gate @ 0x004b4860 - the pounce decision requires the
// PLAYER's health to be under this; indexed by the player's id & 1
// (0 = Jill at 105, 1 = Chris at 72 - Chris must be hurt worse first).
// ============================================================================
const unsigned char hunter_player_health_gate[2] = { 105, 72 };

// ============================================================================
// hunter_behavior_seed_tbl @ 0x004b488f - the initial behaviour roll when the
// action/death layers engage, indexed by hit_state & 7.
// ============================================================================
const unsigned char hunter_behavior_seed_tbl[8] = { 0, 0, 2, 0, 2, 0, 0, 0 };

// ============================================================================
// hunter_intro_anim_tbl @ 0x004b49c8 and hunter_intro_angle_tbl @ 0x004b49cc.
// The original reads these with a stride of TWO, straight through the dodge
// sub-table's pointer bytes - see the header note. PRE-STRIDED: [n] is what
// the original reads for index n.
// ============================================================================
const unsigned char hunter_intro_anim_tbl[16] = {
    0xA0, 0x41, 0xF0, 0x41, 0x40, 0x41, 0x00, 0x00,
    0x18, 0x19, 0x00, 0x04, 0x00, 0x1A, 0x74, 0xFE
};
const short hunter_intro_angle_tbl[16] = {
    (short)0x7DF0, 0x0041, (short)0x7E40, 0x0041,
    0, 0, 0x0018, 0x0019,
    (short)0xFC00, 0x0400, 0x0000, (short)0xFE1A,
    (short)0xFE74, 0x0000, 0x0113, 0x0000
};

// ============================================================================
// hunter_intro_move_tbl @ 0x004b49d8 - {dx, dz} per frame (doubled by the
// caller) for the scripted walk-in, indexed by hunter_intro_jump_kind.
// ============================================================================
const short hunter_intro_move_tbl[6][2] = {
    {    24,    25 },   // [0] default
    { -1024,  1024 },   // [1]
    {     0,  -486 },   // [2] stage 2 room 10
    {  -396,     0 },   // [3] stage 3 room 5
    {   275,     0 },   // [4] stage 5 room 9, cams 10/21
    {  -384,     0 }    // [5] stage 5 room 9, other cams
};

// 0x004b49f0 - which intro movement row applies. Written once on entry to
// behavior 11 from the stage/room word, read as a BYTE index afterwards.
static int hunter_intro_jump_kind = 0;

// 0x00d91bc4 - scream one-shot latch shared by the death fall and the scream
// behaviour so the death howl plays exactly once.
static unsigned char hunter_scream_latch = 0;

// 0x004d3d50 - the SCD-script target entity, written by the state 8 dispatcher
// on every entry (always the enemy list base, 0x00be6464).
static Entity* hunter_scd_target = NULL;

// ---------------------------------------------------------------------------
// Forward declarations, in address order.
// ---------------------------------------------------------------------------
static void hunter_state_init(void);            // 0x00416020
static void hunter_state_run(void);             // 0x00416310
static void hunter_behavior_dispatch(void);     // 0x004163a0
static void hunter_death_dispatch(void);        // 0x004164c0
static void hunter_state_ret(void);             // 0x00416730 - bare RET

static void hunter_variant_dispatch(void);      // 0x004167f0
static void hunter_variant_0_ai(void);          // 0x00416840
static void hunter_variant_1_ai(void);          // 0x00416c30
static void hunter_variant_2_ai(void);          // 0x00416ca0
static void hunter_variant_4_ai(void);          // 0x00417090
static void hunter_coward_ai(void);             // 0x004170d0

static void hunter_behavior_update(void);       // 0x00416740
static void hunter_behavior_idle_dispatch(void);// 0x00417110
static void hunter_idle_stand(void);            // 0x00417130
static void hunter_idle_walk(void);             // 0x00417210
static void hunter_behavior_chase(void);        // 0x00417290
static void hunter_behavior_nop(void);          // 0x00417400 - RET
static void hunter_behavior_attack(void);       // 0x00417410
static void hunter_behavior_pounce(void);       // 0x00417760
static void hunter_behavior_dodge(void);        // 0x00417a80
static void hunter_behavior_grabhold(void);     // 0x00417ee0
static void hunter_behavior_approach(void);     // 0x00418040
static void hunter_behavior_ledgejump(void);    // 0x00418220
static void hunter_behavior_scream(void);       // 0x00418400
static void hunter_behavior_backstep(void);     // 0x004184f0
static void hunter_behavior_intro(void);        // 0x00418640

static void hunter_atk_start(void);             // 0x00417430
static void hunter_atk_swing(void);             // 0x00417480
static void hunter_atk_lunge(void);             // 0x004175a0
static void hunter_atk_recover(void);           // 0x00417650
static void hunter_atk_finish(void);            // 0x004176c0

static void hunter_pounce_start(void);          // 0x00417800
static void hunter_pounce_bite(void);           // 0x00417870
static void hunter_pounce_end(void);            // 0x00417a00
static void hunter_pounce_track(void);          // 0x00417a20
static void hunter_pounce_to_hold(void);        // 0x00417a70

static void hunter_dodge_start(void);           // 0x00417aa0
static void hunter_dodge_hop(void);             // 0x00417b40
static void hunter_dodge_swipe(void);           // 0x00417ba0
static void hunter_dodge_end(void);             // 0x00417df0
static void hunter_dodge_land(void);            // 0x00417e40

static void hunter_act_swipe(void);             // 0x004187b0
static void hunter_act_leapattack(void);        // 0x00418a50
static void hunter_act_pounce(void);            // 0x00418ef0
static void hunter_act_flurry(void);            // 0x00419100
static void hunter_act_holdplayer(void);        // 0x00419260

static void hunter_death_fall(void);            // 0x004165e0
static void hunter_death_thrash(void);          // 0x00416680
static void hunter_death_settle(void);          // 0x004166e0
static void hunter_death_collapse(void);        // 0x00416710
static void hunter_death_pounce(void);          // 0x00419540

static void hunter_death_fall_driver(void);     // 0x00419310
static void hunter_track_player_joint(void);    // 0x004199e0
static void hunter_recenter_on_joint(unsigned char which); // 0x00419b50

static void hunter_scd_state_dispatch(void);    // 0x0048f410
static void hunter_scd_idle(void);              // 0x0048f450
static void hunter_scd_walk(void);              // 0x0048f4d0
static void hunter_scd_run(void);               // 0x0048f680
static void hunter_scd_dodge_run(void);         // 0x0048f820 - wrapper -> [24+]
static void hunter_scd_anim_release(void);      // 0x0048f840
static void hunter_scd_pounce_run(void);        // 0x0048f8a0 - wrapper -> [28+]
static void hunter_scd_flag_release(void);      // 0x0048f930
static void hunter_scd_swipe_run(void);         // 0x0048f960 - wrapper -> [32+]
static void hunter_scd_tint_release(void);      // 0x0048f980
static void hunter_scd_attack_run(void);        // 0x0048f9e0 - wrapper -> [36+]
static void hunter_scd_lunge_start(void);       // 0x0048fa70
static void hunter_scd_bite_driver(void);       // 0x0048fae0
static void hunter_scd_death(void);             // 0x0048fc80
static void hunter_scd_bite_end(void);          // 0x0048fc10
static void hunter_scd_stalk_player(void);      // 0x0048fef0
static void hunter_scd_track_joint(int unused); // 0x0048fd80
static void hunter_roll_leap(void);             // tail of 0x00416840
static void hunter_roll_leap_fixed(void);       // tail of 0x00416ca0

// The dispatch tables are defined further down (their entries need the
// forward declarations above); declared here so the state functions can
// index them.
extern void (*const hunter_state_table[9])(void);
extern void (*const hunter_behavior_table[12])(void);
extern void (*const hunter_variant_table[8])(void);
extern void (*const hunter_act_table[9])(void);
extern void (*const hunter_death_table[8])(void);
extern void (*const hunter_attack_sub_table[6])(void);
extern void (*const hunter_pounce_sub_table[6])(void);
extern void (*const hunter_dodge_sub_table[6])(void);
extern void (*const hunter_idle_variant_table[8])(void);
extern void (*const hunter_scd_table[39])(void);

// hunter_dodge_swipe's reach-test records @ 0x004b49a0, stride 10 bytes:
// {start_frame, frame_window, joint_idx, billboard_x, reach_radius}. The
// swipe tests joint idx*0x7C + 0x44 against the player during the window.
struct HunterSwipeRecord {
    unsigned char start_frame;      // +0x00
    unsigned char pad_01;
    short         frame_window;     // +0x02
    short         joint_idx;        // +0x04
    short         billboard_x;      // +0x06
    short         reach_radius;     // +0x08
};
const HunterSwipeRecord hunter_swipe_reach_tbl[3] = {
    {  7, 0,  4, 12, 1500,  600 },   // 0x004b49a0
    { 18, 0,  2, 15, 1000,  600 },   // 0x004b49aa
    { 20, 0,  5,  9,    0, 1500 }    // 0x004b49b4
};

// Shared effect helper: stage the g_deadMoveValue + 0x14 position block into
// g_playerPosScratch, the same seeding idiom Neptune/Cerberus use.
static void hunter_seed_from_dead_move(void)
{
    const int* seed = (const int*)(H_DMV + 0x14);
    g_playerPosScratch.x   = seed[0];
    g_playerPosScratch.y   = seed[1];
    g_playerPosScratch.z   = seed[2];
    g_playerPosScratch.pad = seed[3];
}

// The 32-byte copy of g_deadMoveValue's matrix body into g_matrixScratch that
// precedes every RotMatrixY in this file. The original open-codes it as an
// interleaved short-copy loop; the destination m is fully overwritten by the
// RotMatrixY that follows, but the copy is kept for fidelity.
static void hunter_copy_dead_move_matrix(void)
{
    memcpy(&g_matrixScratch, H_DMV, 32);
}

// ============================================================================
// hunter_update @ 0x004161f0
// Per-frame entry (dispatch table [6]). Event-controlled hunters (flags bit
// 0x40) are forced into state 8; the shared damage-latch countdown ticks; then
// the state table runs and the fade-sprite/zone bookkeeping closes the frame.
// ============================================================================
void hunter_update(void) // 0x004161f0
{
    if (ENTITY->state != 0 && (ENTITY->behavior_flags & 0x40) != 0) {
        ENTITY->state = 8;
    }

    // Damage latch countdown at +0x183/+0x184.
    H_DEATH_CNT_A = (unsigned char)(H_DEATH_CNT_A - 1);
    if ((signed char)H_DEATH_CNT_A < 0) {
        H_DEATH_CNT_A = 0;
        H_DEATH_CNT_B = 0;
    }

    if ((g_message_flags & 4) != 0) {
        hunter_state_table[ENTITY->state]();

        // The action layer (state 2) skips collision: its behaviours move the
        // hunter by hand. Everything else gets the full SCA + room pass.
        if (ENTITY->state != 5) {
            SetEntityScaHitData(ENTITY);
            ResolveEntityScaCollision((Entity*)&g_playerEntity, ENTITY);
            HandleEnemyPlayerCollisions();
            unsigned char hit = check_room_collision(
                H_POS, *(short*)(uintptr_t)(ENTITY->Sca_info + 10));
            H_ROOM_HIT = (unsigned char)(H_ROOM_HIT | hit);
            H_STEP_WORD = (unsigned short)(uintptr_t)g_tempVar;
        }
    }

    ENTITY->has_enter_switch_zone =
        (unsigned char)is_entity_in_switch_zone(H_POS, g_CurrentRdtDataTypePtr);
    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite(H_POS, (short*)((char*)ENTITY + 0xE4), 0, ENTITY->angle);
    }
}

// ============================================================================
// hunter_state_table @ 0x004b4868 - indexed by Entity+0x84. Entry [4] is a
// bare RET in the original (0x00416730); slots 5-7 are NULL and state 8 is
// handled by the same table (hunter_scd_state_dispatch at 0x004b4888).
// ============================================================================
void (*const hunter_state_table[9])(void) = {
    hunter_state_init,                 // [0] spawn
    hunter_state_run,                  // [1] AI driver
    hunter_behavior_dispatch,          // [2] action layer
    hunter_death_dispatch,             // [3] death
    hunter_state_ret,                  // [4] RET
    NULL,                              // [5]
    NULL,                              // [6]
    NULL,                              // [7]
    hunter_scd_state_dispatch          // [8] SCD-script-controlled
};

// ============================================================================
// hunter_state_init @ 0x00416020
// One-shot spawn. Rolls health, builds the shadow quad, picks the SCA record
// (hard/late-game variant), wires the partner pointer for pack tactics, and
// scripted spawns (flags bit 3) start parked in action behaviour 11.
// ============================================================================
static void hunter_state_init(void) // 0x00416020
{
    H_STATE_BLOCK = 1;                    // state 1, ignore 0, behavior 0, action 0
    ENTITY->scaMatrixData.field_00 = 0;   // +0x1C, cleared as one dword
    ENTITY->pad_c0[1] = 0;                // +0xC1
    H_TICKS = 0;
    ENTITY->animationId = 0;
    ENTITY->hit_state = 0;
    ResetJointTransforms();

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_animFrameIdSave = 0x808080;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)((char*)ENTITY + 0xE4), 1000, 1000);

    H_HEALTH = (short)hunter_health_tbl[rand() & 0xF];

    ENTITY->Sca_info = (unsigned int)(uintptr_t)hunter_sca_info_a;
    if ((ENTITY->behavior_flags & 0x20) != 0) {
        ENTITY->Sca_info = (unsigned int)(uintptr_t)hunter_sca_info_b;
    }
    if (get_stage_id() > 4) {
        ENTITY->Sca_info = (unsigned int)(uintptr_t)hunter_sca_info_b;
    }

    if ((ENTITY->behavior_flags & 0xF) < 2) {
        // Pair hunters: point at the enemy list head, or the next slot when
        // this hunter IS the head (0x00be65f0 = &g_EnemiesList[1]).
        H_PARTNER = g_EnemiesList;
        if (H_PARTNER == ENTITY) {
            H_PARTNER = &g_EnemiesList[1];
        }
        if ((H_PARTNER->status_flags & 1) == 0) {
            ENTITY->behavior_flags = 2;   // whole-byte write
        }
    }
    if ((ENTITY->behavior_flags & 0xF) == 0) {
        ENTITY->behavior_flags |= 2;
    }

    H_PATH_LATCH = 0;
    H_GRAB_WORD = 0;
    H_JOINT_SEL = 0;
    H_POUNCE_LATCH = 0;
    H_POISE = 0;
    H_REPAUSE = 0;
    ENTITY->lookAtJointIdx = 2;
    H_DEATH_CNT_A = 0;
    H_DEATH_CNT_B = 0;

    if ((ENTITY->behavior_flags & 8) != 0) {
        // Scripted intro: parked in action behaviour 11 until the script
        // releases the entity.
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 0xB;
    }
}

// ============================================================================
// hunter_state_run @ 0x00416310
// The per-frame AI driver. Dead hunters (flags bit 7) do nothing; everyone
// else gets the vertical relation bits, the repause tick, the variant AI
// decision (while the action layer does not own the hunter) and the behaviour.
// ============================================================================
static void hunter_state_run(void) // 0x00416310
{
    if ((ENTITY->behavior_flags & 0x80) != 0) return;

    ENTITY->status_flags = (unsigned char)((ENTITY->status_flags & 0x1F) | 0x40);
    entity_check_visual_range(4000);

    int dy = PLAYER_T_INT[1] - H_POS_T[1];
    if (dy != 0) {
        ENTITY->status_flags &= 0x1F;
        if (dy < 0) ENTITY->status_flags |= 0x20;
        else        ENTITY->status_flags |= 0x80;
    }

    if (H_REPAUSE != 0) {
        H_REPAUSE--;
    }

    if (ENTITY->ignore_player_flag == 0) {
        hunter_variant_dispatch();
    }
    hunter_behavior_update();
}

// ============================================================================
// hunter_behavior_dispatch @ 0x004163a0 - state 2, the action layer.
// On entry (ignore_player_flag == 0) it seeds the action behaviour from the
// hit_state roll, cancels into the pounce (4) when a grab is pending, and
// forces behaviour 5 (leap) / 8 (pounce chain) from the status/flag bits.
// ============================================================================
static void hunter_behavior_dispatch(void) // 0x004163a0
{
    if (ENTITY->ignore_player_flag == 0) {
        H_DEATH_CNT_B++;

        if (H_DEATH_CNT_A == 0) {
            H_DEATH_CNT_A = 0x32;
        }
        ENTITY->state = 2;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 0;

        ENTITY->action_behavior = hunter_behavior_seed_tbl[ENTITY->hit_state & 7];
        ENTITY->action_behavior =
            (unsigned char)(ENTITY->action_behavior + ((turn_toward_target(PLAYER_T, 0x400) >> 10) & 1));

        // A pending grab-word cancels into the pounce, unless the hunter is
        // below the floor line (-500), in which case the word is cleared.
        if (H_GRAB_WORD != 0) {
            unsigned char saved = ENTITY->action_behavior;
            ENTITY->action_behavior = 4;
            if (H_POS_T[1] > -500) {
                H_POS_T[1] = 0;
                H_GRAB_WORD = 0;
                ENTITY->action_behavior = saved;
            }
        }
        if ((ENTITY->status_flags & 0xE0) == 0x20) {
            ENTITY->action_behavior = 5;
        }
        if ((ENTITY->behavior_flags & 8) != 0) {
            ENTITY->action_behavior = 8;
        }
    }

    hunter_act_table[ENTITY->action_behavior]();
}

// ============================================================================
// hunter_death_dispatch @ 0x004164c0 - state 3.
// Same seeding shape as the action layer, minus the +1 turn bit for hit_state
// rolls of 2, with the pounce-cancel threshold, the death event flag raise and
// the scream latch reset. Death behaviours then index their own table.
// ============================================================================
static void hunter_death_dispatch(void) // 0x004164c0
{
    if (ENTITY->ignore_player_flag == 0) {
        ENTITY->state = 3;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 0;

        ENTITY->action_behavior = hunter_behavior_seed_tbl[ENTITY->hit_state & 7];
        if ((ENTITY->hit_state & 3) != 2) {
            ENTITY->action_behavior =
                (unsigned char)(ENTITY->action_behavior + ((turn_toward_target(PLAYER_T, 0x400) >> 10) & 1));
        }

        if (H_GRAB_WORD != 0) {
            unsigned char saved = ENTITY->action_behavior;
            ENTITY->action_behavior = 4;
            if (H_POS_T[1] > -500) {
                H_POS_T[1] = 0;
                H_GRAB_WORD = 0;
                ENTITY->action_behavior = saved;
            }
        }
        if ((ENTITY->status_flags & 0xE0) == 0x20) {
            ENTITY->action_behavior = 6;
        }

        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        hunter_scream_latch = 0;
    }

    hunter_death_table[ENTITY->action_behavior]();
}

// 0x00416730 - state 4 is a single RET in the original: a real table slot
// that deliberately does nothing.
static void hunter_state_ret(void) // 0x00416730
{
}

// ============================================================================
// hunter_variant_dispatch @ 0x004167f0
// The pre-AI roll: manhattan distance, then the per-variant decision handler.
// Variant 3 has no handler (NULL slot); coward variants (flags bit 0x10) get
// the flee logic instead.
// ============================================================================
static void hunter_variant_dispatch(void) // 0x004167f0
{
    int dz = PLAYER_T_INT[2] - H_POS_T[2];
    int dx = PLAYER_T_INT[0] - H_POS_T[0];
    g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);

    if ((ENTITY->behavior_flags & 0x10) != 0) {
        hunter_coward_ai();
        return;
    }
    hunter_variant_table[ENTITY->behavior_flags & 0xF]();
}

// ============================================================================
// hunter_variant_0_ai @ 0x00416840 - the plain hunter's decision layer.
// Dead partner or close-and-blocked drops to behaviour 2 (approach); a grabbed
// player at close range forces the pounce (9); line of sight + aligned starts
// the swipe (4); pack tactics start the coordinated pounce (6); and a lined-up
// close hunter with the pounce latch rolls the leap (5).
// ============================================================================
static void hunter_variant_0_ai(void) // 0x00416840
{
    if (ENTITY->behavior_flags == 0) {
        ENTITY->behavior_flags = 2;
        return;
    }

    Entity* partner = H_PARTNER;
    if ((short)partner->health < 0) {
        ENTITY->behavior_flags = 2;
    }
    VECTOR* partnerPos = (VECTOR*)((char*)partner + 0x34);
    int pdz = *(int*)((char*)partner + 0x3C) - H_POS_T[2];
    int pdx = partnerPos->x - H_POS_T[0];
    player_distance_z = (pdz < 0 ? -pdz : pdz) + (pdx < 0 ? -pdx : pdx);
    g_scaled_down_dist = ENTITY->action_behavior;

    if (partner->hit_state != 0) {
        if (g_scaled_down_dist != 2) {
            ENTITY->action_state = 0;
            ENTITY->blend_counter = 7;
        }
        ENTITY->action_behavior = 2;
        ENTITY->ignore_player_flag = 0;
    }
    if (H_PATH_LATCH != 0 && g_playerDisplacement < 4000) {
        if (g_scaled_down_dist != 2) {
            ENTITY->action_state = 0;
            ENTITY->blend_counter = 7;
        }
        ENTITY->action_behavior = 2;
        ENTITY->ignore_player_flag = 0;
    }
    if ((g_playerEntity.isBeingAttackedFlag & 0x80) != 0 && g_playerDisplacement < 3000) {
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 9;
        ENTITY->action_state = 0;
    }

    if (g_playerEntity.isBeingAttackedFlag == 0) {
        if (check_line_of_sight((VECTOR*)PLAYER_T_INT) == 0 &&
            g_playerDisplacement < 3000 &&
            (short)turn_toward_target(PLAYER_T, 0x80) == 0) {
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior = 4;
            ENTITY->action_state = 0;
        }

        if (H_PATH_LATCH != 0 && (ENTITY->behavior_flags & 0xF) != 7) {
            if (player_distance_z < g_playerDisplacement &&
                (short)turn_toward_target(PLAYER_T, 0x20) == 0 &&
                player_distance_z < 4000 &&
                (short)turn_toward_target(partnerPos, 0x200) == 0 &&
                partner->move_speed_current < 0x32) {
                ENTITY->ignore_player_flag = 1;
                ENTITY->action_behavior = 6;
                ENTITY->action_state = 0;
                ENTITY->status_flags &= 0x1F;
            }
            if ((short)turn_toward_target(partnerPos, 0x200) == 0 &&
                H_ROOM_HIT != 0 &&
                player_distance_z < 3000) {
                ENTITY->ignore_player_flag = 1;
                ENTITY->action_behavior = 6;
                ENTITY->action_state = 0;
                ENTITY->status_flags &= 0x1F;
            }
        }

        int dz = PLAYER_T_INT[2] - H_POS_T[2];
        int dx = PLAYER_T_INT[0] - H_POS_T[0];
        g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);
        if (g_playerDisplacement < 2000 &&
            (short)turn_toward_target(PLAYER_T, 0x100) == 0 &&
            H_POUNCE_LATCH != 0) {
            hunter_roll_leap();
        }
    }
}

// The shared leap-roll tail of hunter_variant_0_ai / hunter_variant_2_ai:
// aim a pounce target point past the player and hand over to behaviour 5.
static void hunter_roll_leap(void)
{
    g_svecScratch.x = (short)(PLAYER_T_INT[0] - (short)H_POS_T[0]);
    g_svecScratch.y = 0;
    g_svecScratch.z = (short)(PLAYER_T_INT[2] - (short)H_POS_T[2]);
    hunter_copy_dead_move_matrix();
    int yaw = (g_RandSeed & 1) * 300 - 0x96;
    RotMatrixY(yaw, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);
    H_JOINT_SEL = (unsigned short)(((yaw + 0x96) / 100) + 6);
    H_TARGET_X = (short)(H_POS_T[0] + g_svecScratch.x);
    H_TARGET_Z = (short)(H_POS_T[2] + g_svecScratch.z);
    Add_speedXZ(0);
    ENTITY->angle = (short)(ENTITY->angle + H_JOINT_SEL * -0x14 + 0x98);
    ENTITY->ignore_player_flag = 1;
    ENTITY->action_behavior = 5;
    ENTITY->action_state = 0;
}

// ============================================================================
// hunter_variant_1_ai @ 0x00416c30 - the pack-follower variant: it never
// decides anything on its own, it just mirrors behaviour 8 (the scripted
// formation move) while the lead hunter is alive.
// ============================================================================
static void hunter_variant_1_ai(void) // 0x00416c30
{
    int dz = *(int*)((char*)H_PARTNER + 0x3C) - H_POS_T[2];
    int dx = *(int*)((char*)H_PARTNER + 0x34) - H_POS_T[0];
    player_distance_z = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);

    if (ENTITY->action_behavior != 8) {
        ENTITY->action_state = 0;
        ENTITY->blend_counter = 7;
    }
    ENTITY->action_behavior = 8;
    ENTITY->ignore_player_flag = 0;
}

// ============================================================================
// hunter_variant_2_ai @ 0x00416ca0 - variants 2 and 7. Same skeleton as
// variant 0 plus the health-gated pounce roll: only a wounded PLAYER
// (under hunter_player_health_gate for their character) at medium range with
// the pounce latch set lets the hunter leap.
// ============================================================================
static void hunter_variant_2_ai(void) // 0x00416ca0
{
    short playerHealth = g_playerEntity.health;

    int dz = PLAYER_T_INT[2] - H_POS_T[2];
    int dx = PLAYER_T_INT[0] - H_POS_T[0];
    g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);
    g_scaled_down_dist = ENTITY->action_behavior;
    H_POUNCE_LATCH = 0;

    if ((unsigned short)playerHealth <
        (unsigned short)hunter_player_health_gate[g_playerEntity.id & 1] &&
        0 < playerHealth) {
        H_POISE = 0;
        if (H_HEALTH < 0x28) {
            H_POUNCE_LATCH = hunter_pounce_chance_low[rand() & 0xF];
        }
        if (H_HEALTH < 0x4F) {
            H_POUNCE_LATCH = hunter_pounce_chance_mid[rand() & 0xF];
        }
        if (playerHealth > 0xF &&
            (g_playerDisplacement < 0x157C || g_playerEntity.isBeingAttackedFlag != 0)) {
            H_POUNCE_LATCH = 0;
        }
    }
    if (H_REPAUSE != 0) {
        H_POUNCE_LATCH = 0;
    }

    if ((short)turn_toward_target(PLAYER_T, 0x2C8) == 0 &&
        g_playerDisplacement < 0x1900 &&
        H_POUNCE_LATCH != 0) {
        hunter_roll_leap_fixed();
        return;
    }

    if (H_PATH_LATCH != 0 && g_playerDisplacement < 6000) {
        if (g_scaled_down_dist != 2) {
            ENTITY->action_state = 0;
            ENTITY->blend_counter = 7;
        }
        ENTITY->action_behavior = 2;
        ENTITY->ignore_player_flag = 0;
    }
    if ((g_playerEntity.isBeingAttackedFlag & 0x80) != 0 && g_playerDisplacement < 3000) {
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 9;
        ENTITY->action_state = 0;
    }

    if (g_playerEntity.isBeingAttackedFlag == 0) {
        if (check_line_of_sight((VECTOR*)PLAYER_T_INT) == 0 &&
            g_playerDisplacement < 3000 &&
            (short)turn_toward_target(PLAYER_T, 0x80) == 0) {
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior = 4;
            ENTITY->action_state = 0;
            return;
        }
        if (H_PATH_LATCH != 0 && (ENTITY->behavior_flags & 0xF) != 7 &&
            g_playerDisplacement > 0x1900 &&
            (short)turn_toward_target(PLAYER_T, 0x20) == 0 &&
            (g_playerEntity.action_behavior == 0x12 ||
             (g_playerEntity.action_behavior == 0x13 && (g_RandSeed & 1) != 0)) &&
            (char)is_facing_toward_entity(&g_playerEntity) == 0) {
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior = 6;
            ENTITY->action_state = 0;
            ENTITY->status_flags &= 0x1F;
        }
    }
}

// The variant-2 flavour of the leap roll: same shape as hunter_roll_leap but
// the yaw roll happens after the matrix copy and always re-rolls rand().
static void hunter_roll_leap_fixed(void)
{
    g_svecScratch.x = (short)(PLAYER_T_INT[0] - (short)H_POS_T[0]);
    g_svecScratch.y = 0;
    g_svecScratch.z = (short)(PLAYER_T_INT[2] - (short)H_POS_T[2]);
    hunter_copy_dead_move_matrix();
    int yaw = 0xFFFFFF6A;             // -150 seed, immediately replaced
    yaw = (rand() & 1) * 300 - 0x96;
    RotMatrixY(yaw, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);
    H_JOINT_SEL = (unsigned short)(((yaw + 0x96) / 100) + 6);
    H_TARGET_X = (short)(H_POS_T[0] + g_svecScratch.x);
    H_TARGET_Z = (short)(H_POS_T[2] + g_svecScratch.z);
    Add_speedXZ(0);
    ENTITY->angle = (short)(ENTITY->angle + H_JOINT_SEL * -0x14 + 0x98);
    ENTITY->ignore_player_flag = 1;
    ENTITY->action_behavior = 5;
    ENTITY->action_state = 0;
}

// ============================================================================
// hunter_variant_4_ai @ 0x00417090 - variants 4-6: walk to the player until
// close, then hand over to the scripted formation behaviour (8) and drop the
// "walk in" flag bit.
// ============================================================================
static void hunter_variant_4_ai(void) // 0x00417090
{
    ENTITY->status_flags &= 0x1F;
    if (g_playerDisplacement < 4000) {
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 8;
        ENTITY->action_state = 0;
        ENTITY->behavior_flags = (unsigned char)(ENTITY->behavior_flags - 4);
    }
}

// ============================================================================
// hunter_coward_ai @ 0x004170d0 - the flags-bit-0x10 variants never fight:
// they clear their own coward bit once the player gets close enough.
// ============================================================================
static void hunter_coward_ai(void) // 0x004170d0
{
    if (H_PATH_LATCH != 0 && g_playerDisplacement < 9000) {
        ENTITY->behavior_flags &= 0xEF;
    }
    if (g_playerDisplacement < 4000) {
        ENTITY->behavior_flags &= 0xEF;
    }
}

// ============================================================================
// hunter_behavior_update @ 0x00416740
// The state 1 behaviour driver. Prologue: latch the pathfinder result (a
// non-zero result also arms the 60-frame repause), refresh the waypoint pair,
// then dispatch through hunter_behavior_table on action_behavior. TWELVE live
// entries - see the header note for the Ghidra over-read.
// ============================================================================
static void hunter_behavior_update(void) // 0x00416740
{
    unsigned char path = (unsigned char)entity_pathfind_update();
    g_animFrameIdSave = path;

    if (H_PATH_LATCH == 0) {
        if ((path & 0xFE) == 0) {
            H_PATH_LATCH = (short)(H_PATH_LATCH | (path & 1));
        }
        if (H_PATH_LATCH != 0) {
            H_REPAUSE = 0x3C;
        }
    }

    zone_path_find(PLAYER_T_INT[0], PLAYER_T_INT[2],
                 (int*)&ENTITY->player_pos_x, (int*)&ENTITY->player_pos_z);

    hunter_behavior_table[ENTITY->action_behavior]();
}

// ============================================================================
// hunter_behavior_table @ 0x004b48e0 - indexed by action_behavior while state 1.
// ============================================================================
void (*const hunter_behavior_table[12])(void) = {
    hunter_behavior_idle_dispatch, // [0]  idle/walk by variant
    hunter_behavior_chase,         // [1]  chase with wander steering
    hunter_behavior_approach,      // [2]  slow approach, 90-frame limit
    hunter_behavior_nop,           // [3]  RET
    hunter_behavior_attack,        // [4]  swipe driver (sub-table)
    hunter_behavior_pounce,        // [5]  aimed leap (sub-table)
    hunter_behavior_dodge,         // [6]  dodge/swipe chain (sub-table)
    hunter_behavior_grabhold,      // [7]  hold-the-player sub-states
    hunter_behavior_ledgejump,     // [8]  scripted formation jump-down
    hunter_behavior_scream,        // [9]  the grabbed-player howl
    hunter_behavior_backstep,      // [10] post-swipe sidestep
    hunter_behavior_intro          // [11] scripted walk-in
};

// ============================================================================
// hunter_variant_table @ 0x004b4910 - indexed by behavior_flags & 0xf from
// hunter_variant_dispatch. Slot 3 is NULL (no such variant).
// ============================================================================
void (*const hunter_variant_table[8])(void) = {
    hunter_variant_0_ai,   // [0] plain hunter
    hunter_variant_1_ai,   // [1] pack follower
    hunter_variant_2_ai,   // [2] health-gated pouncer
    NULL,                  // [3]
    hunter_variant_4_ai,   // [4]
    hunter_variant_4_ai,   // [5]
    hunter_variant_4_ai,   // [6]
    hunter_variant_2_ai    // [7]
};

// ============================================================================
// hunter_behavior_idle_dispatch @ 0x00417110 - behaviour 0.
// A second dispatch layer: the idle stand vs the idle walk, by variant.
// ============================================================================
static void hunter_behavior_idle_dispatch(void) // 0x00417110
{
    hunter_idle_variant_table[ENTITY->behavior_flags & 0xF]();
}

// ============================================================================
// hunter_idle_variant_table @ 0x004b4950 - EIGHT entries by variant. The
// dispatcher at 0x0041711e jumps with `JMP [ECX*4 + 0x4b4950]`, so entries
// 4-7 read the second half at 0x004b4960 - the half that has no literal
// xrefs of its own and looks like dead data in Ghidra. Slot 3 is NULL in the
// image (no such variant reaches behaviour 0).
// ============================================================================
void (*const hunter_idle_variant_table[8])(void) = {
    hunter_idle_stand,          // [0] 0x004b4950
    hunter_idle_walk,           // [1] 0x004b4954
    hunter_idle_stand,          // [2] 0x004b4958
    NULL,                       // [3] 0x004b495c
    hunter_idle_walk,           // [4] 0x004b4960
    hunter_idle_walk,           // [5] 0x004b4964
    hunter_idle_walk,           // [6] 0x004b4968
    hunter_idle_stand           // [7] 0x004b496c
};
// ============================================================================
// hunter_idle_stand @ 0x00417130 - variant 0/2 idle: play animation 0x15 for
// 30 frames, then either re-arm for 200 more (coward bit) or release the
// action layer back to the chase.
// ============================================================================
static void hunter_idle_stand(void) // 0x00417130
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0x15;
        H_TICKS = 0x1E;
        ENTITY->blend_counter = 7;
        H_JOINT_SEL = 0;
        H_SPEED_W = 0;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
    short prev = H_TICKS;
    H_TICKS = (short)(H_TICKS - 1);
    if (prev == 0) {
        if ((ENTITY->behavior_flags & 0x10) != 0) {
            H_TICKS = 200;
            return;
        }
        ENTITY->ignore_player_flag = 0;
        H_BEH_WORD = 1;   // behavior 1, action_state 0
    }
}

// ============================================================================
// hunter_idle_walk @ 0x00417210 - variant 1 idle: animation 1, no timer, the
// AI layer keeps steering.
// ============================================================================
static void hunter_idle_walk(void) // 0x00417210
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 1;
        ENTITY->blend_counter = 7;
        H_SPEED_W = 0;
        H_JOINT_SEL = 0;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
}

// ============================================================================
// hunter_behavior_chase @ 0x00417290 - behaviour 1.
// The run: wander steering toward the player, footstep sounds at animation
// frames 1 and 32, hard homing inside 2000, animation 0x10.
// ============================================================================
static void hunter_behavior_chase(void) // 0x00417290
{
    ENTITY->animationId = 0x10;
    H_SPEED_W = 0x28;
    g_animFrameIdSave = 6;

    g_playerPosScratch.x = PLAYER_T_INT[0];
    g_playerPosScratch.y = PLAYER_T_INT[1];
    g_playerPosScratch.z = PLAYER_T_INT[2];
    g_playerPosScratch.pad = (int)((short)g_playerEntity.scaMatrixData.worldMatrix.m[0][0] |
                                   ((short)g_playerEntity.scaMatrixData.worldMatrix.m[0][1] << 16));

    entity_update_wander_turn(H_STEP_WORD, (unsigned char*)((char*)ENTITY + 0x180),
                              &ENTITY->angle_turn_delta, 0x18, 0x3C);

    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        H_JOINT_SEL = 0;
    }
    if (ENTITY->animation_frame_id == 1)  Snd_em(0);
    if (ENTITY->animation_frame_id == 32) Snd_em(0);

    int dz = PLAYER_T_INT[2] - H_POS_T[2];
    int dx = PLAYER_T_INT[0] - H_POS_T[0];
    g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);
    if (g_playerDisplacement < 2000) {
        entity_rotate_toward_target(PLAYER_T, 0x18);
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    Add_speedXZ(0);
}

// 0x00417400 - behaviour 3 is a single RET.
static void hunter_behavior_nop(void) // 0x00417400
{
}

// ============================================================================
// hunter_behavior_attack @ 0x00417410 - behaviour 4.
// Drives the swipe sub-state machine, then re-centers the hunter on the
// swing joint every frame.
// ============================================================================
static void hunter_behavior_attack(void) // 0x00417410
{
    hunter_attack_sub_table[ENTITY->action_state]();
    hunter_recenter_on_joint(1);
}

// ============================================================================
// hunter_attack_sub_table @ 0x004b4970 - swipe sub-states.
// ============================================================================
void (*const hunter_attack_sub_table[6])(void) = {
    hunter_atk_start,    // [0] wind-up pose
    hunter_atk_swing,    // [1] the claw swing + reach test
    hunter_atk_lunge,    // [2] the follow-up lunge
    hunter_atk_recover,  // [3] drag the player during recovery
    hunter_atk_finish,   // [4] re-roll the next action
    NULL                 // [5]
};

// ============================================================================
// hunter_behavior_pounce @ 0x00417760 - behaviour 5.
// Drives the aimed-leap sub-states; while the wind-up plays it turns toward
// the stored target point and past frame 0x17 charges forward.
// ============================================================================
static void hunter_behavior_pounce(void) // 0x00417760
{
    H_POUNCE_LATCH = 0;
    hunter_pounce_sub_table[ENTITY->action_state]();

    if (ENTITY->animation_frame_id < 0x17) {
        g_playerPosScratch.x = H_TARGET_X;
        g_playerPosScratch.z = H_TARGET_Z;
        g_playerPosScratch.y = 0;
        int turn = turn_toward_target(&g_playerPosScratch, 0x18);
        ENTITY->angle = (short)(ENTITY->angle + (short)turn);
        Add_speedXZ(0);
        return;
    }
    if (ENTITY->animation_frame_id < 0x1F) {
        H_SPEED_W = 0x32;
        Add_speedXZ(0x800);
    }
}

// ============================================================================
// hunter_pounce_sub_table @ 0x004b4988 - aimed-leap sub-states.
// ============================================================================
void (*const hunter_pounce_sub_table[6])(void) = {
    hunter_pounce_start,   // [0] crouch, pick the pounce animation
    hunter_pounce_bite,    // [1] the airborne bite reach test
    hunter_pounce_end,     // [2] land, release the action layer
    hunter_pounce_track,   // [3] drag the grabbed player
    hunter_pounce_to_hold, // [4] hand over to the hold behaviour
    NULL                   // [5]
};

// ============================================================================
// hunter_behavior_dodge @ 0x00417a80 - behaviour 6.
// Drives the dodge sub-states, then brakes.
// ============================================================================
static void hunter_behavior_dodge(void) // 0x00417a80
{
    hunter_dodge_sub_table[ENTITY->action_state]();
    Add_speedXZ(0);
}

// ============================================================================
// hunter_dodge_sub_table @ 0x004b49c0 - dodge sub-states. The pointer bytes
// of this table are ALSO the storage the intro tables read with a stride of
// two - see the header note and the pre-strided arrays.
// ============================================================================
void (*const hunter_dodge_sub_table[6])(void) = {
    hunter_dodge_start,  // [0] hop back, animation 0x13
    hunter_dodge_hop,    // [1] the retreat hop
    hunter_dodge_swipe,  // [2] the counter-swipe, ballistic arc
    hunter_dodge_end,    // [3] release back to the chase
    hunter_dodge_land,   // [4] the swiping land
    NULL                 // [5]
};

// ============================================================================
// hunter_atk_start @ 0x00417430 - swipe [0]. The wind-up pose: animation 5.
// ============================================================================
static void hunter_atk_start(void) // 0x00417430
{
    ENTITY->action_state = 1;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 7;
    H_SPEED_W = 0;
    ENTITY->animationId = 5;
}

// ============================================================================
// hunter_atk_swing @ 0x00417480 - swipe [1].
// The claw swing. Frames 4-11 reach-test joint 12 (offset 0x4A0) with an
// 800-unit box; a hit snaps the player into the clawed pose (anim word
// 0x00640002 = anim 2, frame 0, behaviour 100) and takes 10 health (13 under
// the poison flag). On animation end the hunter drops back to the chase -
// unless it is SCD-controlled, which keeps the action layer alive.
// ============================================================================
static void hunter_atk_swing(void) // 0x00417480
{
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) == 0) {
        if ((ENTITY->behavior_flags & 0x40) == 0 &&
            (unsigned char)(ENTITY->animation_frame_id - 4) < 8) {
            hunter_seed_from_dead_move();
            unsigned char hit = FUN_0048ae00((MATRIX*)((char*)H_JOINTS + 0x4A0),
                                             &g_playerPosScratch, 800, PLAYER_T_INT);
            player_distance_z = hit;
            if (hit != 0 && g_playerEntity.isBeingAttackedFlag == 0) {
                Snd_em(5);
                g_playerEntity.isBeingAttackedFlag = 1;
                *(unsigned int*)&g_playerEntity.animationId = 0x00640002;
                ENTITY->action_state = 2;
                if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) != 0) {
                    g_playerEntity.health = (short)(g_playerEntity.health - 13);
                    return;
                }
                g_playerEntity.health = (short)(g_playerEntity.health - 10);
            }
        }
    } else {
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + 1);
        if ((ENTITY->behavior_flags & 0x40) == 0) {
            H_STATE_BLOCK = 0x10001;
        }
    }
}

// ============================================================================
// hunter_atk_lunge @ 0x004175a0 - swipe [2].
// The follow-up lunge: a burst at speed 200 with a blood billboard on the arm
// and a red tint flash on the joints.
// ============================================================================
static void hunter_atk_lunge(void) // 0x004175a0
{
    H_SPEED_W = 200;
    Add_speedXZ(0x52C);
    ENTITY->action_state = 3;
    H_TICKS = 4;

    hunter_seed_from_dead_move();
    g_playerPosScratch.x = 200;
    Effect_CreateBillboard(0, 0, 0, (void*)((char*)H_JOINTS + 0x4A0), &g_playerPosScratch, 0);
    JointApplyColorTint((JointStruct*)((char*)H_JOINTS + 0x45C), 0x30, 0x80820, (void*)0x606060);
}

// ============================================================================
// hunter_atk_recover @ 0x00417650 - swipe [3].
// Recovery. While the tick counter runs, the player is dragged along by the
// animation's X/Z offsets (+0x78/+0x7C).
// ============================================================================
static void hunter_atk_recover(void) // 0x00417650
{
    ENTITY->action_state = (unsigned char)(ENTITY->action_state +
        (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
    if (H_TICKS != 0) {
        H_TICKS = (short)(H_TICKS - 1);
        PLAYER_T_INT[0] += (short)ENTITY->speed.x;
        PLAYER_T_INT[2] += (short)ENTITY->speed.z;
    }
}

// ============================================================================
// hunter_atk_finish @ 0x004176c0 - swipe [4].
// Re-rolls the next action: a 1-in-4 chance of chaining straight into the
// pounce chain (behaviour 5), otherwise back to the chase (behaviour 1).
// Variant-7 hunters never chain; variant 0 flips a fair coin instead when its
// partner is still alive.
// ============================================================================
static void hunter_atk_finish(void) // 0x004176c0
{
    H_STATE_WORD = 1;   // state 1, ignore 0

    bool chain = ((unsigned int)rand() & 3) == 0;
    if ((ENTITY->behavior_flags & 0xF) == 0 && H_PARTNER->health >= 0) {
        chain = (rand() & 1) != 0;
    }
    if ((ENTITY->behavior_flags & 0xF) == 7) {
        chain = false;
    }

    ENTITY->ignore_player_flag = chain;
    ENTITY->action_behavior = (unsigned char)(chain * 5 + 1);
    ENTITY->action_state = 0;
}

// ============================================================================
// hunter_pounce_start @ 0x00417800 - pounce [0].
// The crouch: picks one of two crouch animations from the joint selector,
// or the scripted-spawn crouch for SCD-controlled hunters.
// ============================================================================
static void hunter_pounce_start(void) // 0x00417800
{
    ENTITY->action_state = 1;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 7;
    H_SPEED_W = 300;
    ENTITY->animationId = (unsigned char)(0x12 - (signed char)H_JOINT_SEL);
    if ((ENTITY->behavior_flags & 0x40) != 0) {
        ENTITY->animationId = 0xC;
    }
    Snd_em(2);
}

// ============================================================================
// hunter_pounce_bite @ 0x00417870 - pounce [1].
// The airborne bite. Frames 7-14 reach-test the selected mouth joint
// (idx*0x7C + 0x44) with a 700-unit box while the player is not already
// grabbed and their head joint is visible; a hit snaps them into the bitten
// pose (anim 7 frame 6) and hands over to the drag sub-state.
// ============================================================================
static void hunter_pounce_bite(void) // 0x00417870
{
    if ((ENTITY->behavior_flags & 0x40) == 0 &&
        g_playerEntity.health > 0 &&
        (unsigned char)(ENTITY->animation_frame_id - 7) < 8 &&
        (g_playerEntity.jointsStructs[1].flags & 0x40) == 0) {
        hunter_seed_from_dead_move();
        MATRIX* mouth = (MATRIX*)((char*)H_JOINTS + H_JOINT_SEL * 0x7C + 0x44);
        unsigned char hit = FUN_0048ae00(mouth, &g_playerPosScratch, 700, PLAYER_T_INT);
        player_distance_z = hit;
        if (hit != 0) {
            g_playerEntity.isBeingAttackedFlag = 1;
            g_playerEntity.animationId = 7;
            g_playerEntity.animFrameId = 6;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state = 0;
            ENTITY->action_state = 3;
            H_TICKS = 4;

            hunter_seed_from_dead_move();
            g_playerPosScratch.x = 200;
            Effect_CreateBillboard(0, 3, 0, mouth, &g_playerPosScratch, 0);
            JointApplyColorTint((JointStruct*)((char*)H_JOINTS + H_JOINT_SEL * 0x7C),
                                0x30, 0x80820, (void*)0x606060);
            Snd_em(5);
            return;
        }
    }
    ENTITY->action_state = (unsigned char)(ENTITY->action_state +
        (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
}

// ============================================================================
// hunter_pounce_end @ 0x00417a00 - pounce [2].
// Land: release the action layer back to the AI layer's chase.
// ============================================================================
static void hunter_pounce_end(void) // 0x00417a00
{
    H_STATE_BLOCK = 0x10001;
    H_JOINT_SEL = 0;
}

// ============================================================================
// hunter_pounce_track @ 0x00417a20 - pounce [3].
// Drags the grabbed player with the mouth-joint tracker and raises the
// shared grab flag for the death screen.
// ============================================================================
static void hunter_pounce_track(void) // 0x00417a20
{
    ENTITY->action_state = (unsigned char)(ENTITY->action_state +
        (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400));
    DAT_004bd2b0 = 1;
    hunter_track_player_joint();
}

// ============================================================================
// hunter_pounce_to_hold @ 0x00417a70 - pounce [4]. Hand over to the hold
// behaviour.
// ============================================================================
static void hunter_pounce_to_hold(void) // 0x00417a70
{
    H_BEH_WORD = 7;
}

// ============================================================================
// hunter_dodge_start @ 0x00417aa0 - dodge [0].
// The retreat hop: animation 0x13, a backward drift, arms the grab word so
// the player's counter-swipes latch, and zeroes the mouth selector.
// ============================================================================
static void hunter_dodge_start(void) // 0x00417aa0
{
    ENTITY->action_state = 1;
    ENTITY->ignore_player_flag = 1;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->death_timer = 0;
    ENTITY->blend_counter = 7;
    H_SPEED_W = 0x5A;
    ENTITY->animationId = 0x13;
    H_GRAB_WORD = 1;
    H_JOINT_SEL = 0;
    if ((ENTITY->behavior_flags & 0x40) != 0) {
        H_SPEED_W = 0x3C;
    }
}

// ============================================================================
// hunter_dodge_hop @ 0x00417b40 - dodge [1].
// The hop itself; every frame whose number has bit 2 set re-arms the
// counter-swipe (sub-state 2) with speed 0x82.
// ============================================================================
static void hunter_dodge_hop(void) // 0x00417b40
{
    ENTITY->status_flags &= 0x1F;
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
    if ((ENTITY->animation_frame_id & 4) != 0) {
        ENTITY->action_state = 2;
        H_SPEED_W = 0x82;
        Snd_em(2);
    }
}

// ============================================================================
// hunter_dodge_swipe @ 0x00417ba0 - dodge [2].
// The counter-swipe, launched ballistically (vy0 580, gravity -50). While
// airborne, the two reach records in hunter_swipe_reach_tbl (rows 2 then 1)
// test their joint against the player during their frame window: a hit snaps
// the player into one of two clawed poses depending on whether they were
// facing the hunter, tints the mouth joint red, and takes 15 health (20
// under the poison flag). On landing: sub-state 3, speed 0x5A.
// ============================================================================
static void hunter_dodge_swipe(void) // 0x00417ba0
{
    ENTITY->status_flags &= 0x1F;
    if (ENTITY->animation_frame_id > 8) {
        ENTITY->status_flags |= 0x80;
    }

    if ((ENTITY->behavior_flags & 0x40) == 0) {
        // Two probe rows, walked backwards like the original's countdown.
        for (int row = 2; row != 0; row--) {
            const HunterSwipeRecord* rec = &hunter_swipe_reach_tbl[row];
            unsigned int inWindow =
                (unsigned char)(ENTITY->animation_frame_id - rec->start_frame);
            if ((int)inWindow < rec->frame_window &&
                g_playerEntity.isBeingAttackedFlag == 0) {
                hunter_seed_from_dead_move();
                g_playerPosScratch.x = rec->billboard_x;
                MATRIX* mouth = (MATRIX*)((char*)H_JOINTS +
                                          rec->joint_idx * 0x7C + 0x44);
                unsigned char hit = FUN_0048ae00(mouth, &g_playerPosScratch,
                                                 rec->reach_radius, PLAYER_T_INT);
                player_distance_z = hit;
                if (hit != 0 && g_playerEntity.isBeingAttackedFlag == 0) {
                    unsigned char facing =
                        (unsigned char)is_facing_toward_entity(&g_playerEntity);
                    g_scaled_down_dist = facing;
                    g_playerEntity.isBeingAttackedFlag = (unsigned char)(facing + 2);
                    g_playerEntity.action_behavior = (unsigned char)(facing + 0x66);

                    hunter_seed_from_dead_move();
                    g_playerPosScratch.x = 100;
                    Effect_CreateBillboard(0, 0, 0, mouth, &g_playerPosScratch, 0);

                    ENTITY->action_state = 4;
                    JointApplyColorTint((JointStruct*)((char*)H_JOINTS +
                                                       rec->joint_idx * 0x7C),
                                        0x30, 0x80820, (void*)0x606060);
                    Snd_em(3);
                    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) != 0) {
                        g_playerEntity.health = (short)(g_playerEntity.health - 0x14);
                        return;
                    }
                    g_playerEntity.health = (short)(g_playerEntity.health - 0xF);
                    return;
                }
            }
        }
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    if (entity_ballistic_step((short)H_SPEED_W, 0x244, (short)0xFFCE, 0) != 0) {
        ENTITY->action_state = 3;
        H_SPEED_W = 0x5A;
        H_GRAB_WORD = 0;
        ENTITY->status_flags &= 0x1F;
    }
}

// ============================================================================
// hunter_dodge_end @ 0x00417df0 - dodge [3]. Release back to the chase.
// ============================================================================
static void hunter_dodge_end(void) // 0x00417df0
{
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->ignore_player_flag = 0;
        H_BEH_WORD = 2;
    }
}

// ============================================================================
// hunter_dodge_land @ 0x00417e40 - dodge [4].
// The swiping landing: same ballistic arc as the counter-swipe without the
// reach tests; sets both "player above" style status bits past frame 8, and
// releases back to the chase when it touches down.
// ============================================================================
static void hunter_dodge_land(void) // 0x00417e40
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->status_flags &= 0x1F;
    if (ENTITY->animation_frame_id > 8) {
        ENTITY->status_flags |= 0xC0;
    }
    if (entity_ballistic_step((short)H_SPEED_W, 0x244, (short)0xFFCE, 0) != 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 2;
        ENTITY->action_state = 0;
        H_GRAB_WORD = 0;
        ENTITY->status_flags &= 0x1F;
    }
}

// ============================================================================
// hunter_behavior_grabhold @ 0x00417ee0 - behaviour 7.
// Four sub-states: bite down, hold (tracking the player's head joint via
// hunter_track_player_joint), shake, and release back to the AI layer.
// ============================================================================
static void hunter_behavior_grabhold(void) // 0x00417ee0
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
        H_SPEED_W = 0;
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state +
            (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
        hunter_track_player_joint();
        return;
    case 2:
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
        H_TICKS = 5;
        // fall through
    case 3:
        H_TICKS = (short)(H_TICKS - (unsigned short)
            (unsigned char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400));
        if (H_TICKS == 0) {
            ENTITY->action_state = 4;
        }
        hunter_track_player_joint();
        return;
    case 4:
        H_STATE_BLOCK = 0x10001;   // state 1, ignore 0, behavior 1, action 0
        return;
    default:
        return;
    }
}

// ============================================================================
// hunter_behavior_approach @ 0x00418040 - behaviour 2.
// The slow walk-in: animation 2 for up to 90 frames, wander steering with a
// hard homing turn inside 4000, footstep sounds at frames 1 and 15.
// ============================================================================
static void hunter_behavior_approach(void) // 0x00418040
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 2;
        H_SPEED_W = 0xF0;
        H_TICKS = 10;
        H_JOINT_SEL = 0;
        H_APPROACH_CNT = 0;
    }

    H_APPROACH_CNT = (unsigned char)(H_APPROACH_CNT + 1);
    if (H_APPROACH_CNT == 90) {
        H_BEH_WORD = 1;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
    if (ENTITY->animation_frame_id == 1)  Snd_em(1);
    if (ENTITY->animation_frame_id == 15) Snd_em(1);

    g_playerPosScratch.x = PLAYER_T_INT[0];
    g_playerPosScratch.y = PLAYER_T_INT[1];
    g_animFrameIdSave = 7;
    g_playerPosScratch.z = PLAYER_T_INT[2];
    g_playerPosScratch.pad = (int)((short)g_playerEntity.scaMatrixData.worldMatrix.m[0][0] |
                                   ((short)g_playerEntity.scaMatrixData.worldMatrix.m[0][1] << 16));

    entity_update_wander_turn(H_STEP_WORD, (unsigned char*)((char*)ENTITY + 0x180),
                              &ENTITY->angle_turn_delta, 0x60, 0x3C);

    H_TICKS = (short)(H_TICKS - 1);

    int dz = PLAYER_T_INT[2] - H_POS_T[2];
    int dx = PLAYER_T_INT[0] - H_POS_T[0];
    g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);
    if (g_playerDisplacement < 4000) {
        entity_rotate_toward_target(PLAYER_T, 0x60);
    }
    Add_speedXZ(0);
}

// ============================================================================
// hunter_behavior_ledgejump @ 0x00418220 - behaviour 8, the scripted
// formation jump-down. The pack follower walks off the ledge with the lead
// hunter: hop, ballistic fall, land, then either resume the chase or (for
// SCD-controlled hunters) park in behaviour 0 and raise the script flag.
// ============================================================================
static void hunter_behavior_ledgejump(void) // 0x00418220
{
    ENTITY->status_flags &= 0x1F;

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0;
        H_SPEED_W = 0x5A;
        H_JOINT_SEL = 0;
        H_GRAB_WORD = 1;
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state +
            (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
        if (ENTITY->animation_frame_id > 9) {
            if (entity_ballistic_step(0, 0, (short)0xFFC4, 0) != 0) {
                ENTITY->action_state = 3;
                ENTITY->animation_frame_id = 0;
                ENTITY->timing_control = 0;
                ENTITY->blend_counter = 7;
                ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
                H_SPEED_W = 0;
                Snd_em(4);
            }
        }
        break;
    case 2:
        if (entity_ballistic_step(0, 0, (short)0xFFC4, 0) != 0) {
            ENTITY->action_state = 3;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 7;
            ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
            H_SPEED_W = 0;
            Snd_em(4);
        }
        break;
    case 3:
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
            ENTITY->ignore_player_flag = 0;
            H_BEH_WORD = 1;
            H_GRAB_WORD = 0;
            ENTITY->status_flags &= 0xFB;
            if ((ENTITY->behavior_flags & 0x40) != 0) {
                Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
                H_BEH_WORD = 0;
            }
        }
        break;
    }
    Add_speedXZ(0);
}

// ============================================================================
// hunter_behavior_scream @ 0x00418400 - behaviour 9.
// The howl it does while the player is in its claws (or dying): animation
// 0x17, one-shot sound at frame 8 through the shared scream latch, then back
// to behaviour 0 (or the script flag for SCD-controlled hunters).
// ============================================================================
static void hunter_behavior_scream(void) // 0x00418400
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 1;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0x17;
        hunter_scream_latch = 0;
    } else if (ENTITY->action_state != 1) {
        return;
    }

    if (ENTITY->animation_frame_id == 8 && hunter_scream_latch == 0) {
        Snd_em(7);
        hunter_scream_latch = 1;
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        }
        H_BEH_WORD = 0;
    }
}

// ============================================================================
// hunter_behavior_backstep @ 0x004184f0 - behaviour 10.
// The post-swipe sidestep: one animation of strafing left or right (the turn
// roll picks the side, stored in +0x185), then back to the chase.
// ============================================================================
static void hunter_behavior_backstep(void) // 0x004184f0
{
    int dz = PLAYER_T_INT[2] - H_POS_T[2];
    int dx = PLAYER_T_INT[0] - H_POS_T[0];
    g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);

    ENTITY->status_flags &= 0x1F;

    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 1;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0x14;
        H_SPEED_W = (g_playerDisplacement < 3000) ? 0x96 : 0xFA;
        H_STRAFE_DIR = (unsigned char)(turn_toward_target(PLAYER_T, 1) + 2);
    } else if (ENTITY->action_state != 1) {
        Add_speedXZ((int)(char)H_STRAFE_DIR << 10);
        return;
    }

    if ((char)Joint_move(1, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        ENTITY->state = 1;
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 2;
        ENTITY->action_state = 0;
    }
    ENTITY->angle = (short)(ENTITY->angle + ((char)H_STRAFE_DIR - 2) * 0x40);
    Add_speedXZ((int)(char)H_STRAFE_DIR << 10);
}

// ============================================================================
// hunter_behavior_intro @ 0x00418640 - behaviour 11, the scripted walk-in.
// Picks the intro animation and movement row from the stage/room word (the
// jump kind), shuffles the hunter along it for the first 6 frames, howls at
// frame 20, and on animation end snaps the entry yaw, parks in behaviour 2
// and drops the scripted-intro flag.
// ============================================================================
static void hunter_behavior_intro(void) // 0x00418640
{
    if (ENTITY->action_state == 0) {
        unsigned short stageRoom = (unsigned short)(g_stageId | (g_roomId << 8));
        if (stageRoom == (STAGE_MANSION_RETURN_1F | (ROOM_F_PASSAGE << 8))) {
            hunter_intro_jump_kind = 3;
        } else if (stageRoom == (STAGE_MANSION_RETURN_1F | (ROOM_TRAP_PASSAGE << 8))) {
            hunter_intro_jump_kind = 4;
            if (g_AttractMode_RoomCameraId == 10 || g_AttractMode_RoomCameraId == 21) {
                hunter_intro_jump_kind = 5;
            }
        } else if (stageRoom == (STAGE_COURTYARD | (ROOM_ENRICO_ROOM << 8))) {
            hunter_intro_jump_kind = 2;
        }
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = hunter_intro_anim_tbl[ENTITY->behavior_flags];
    } else if (ENTITY->action_state != 1) {
        return;
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) == 0) {
        if (ENTITY->animation_frame_id < 6) {
            H_POS_T[0] += hunter_intro_move_tbl[hunter_intro_jump_kind][0] * 2;
            H_POS_T[2] += hunter_intro_move_tbl[hunter_intro_jump_kind][1] * 2;
        }
        if (ENTITY->animation_frame_id == 20) {
            Snd_em(7);
        }
        return;
    }

    H_STATE_BLOCK = 0x20001;   // state 1, ignore 0, behavior 2, action 0
    ENTITY->angle = (unsigned short)(
        hunter_intro_angle_tbl[ENTITY->behavior_flags] + ENTITY->angle) & 0xFFF;
    ENTITY->behavior_flags = 2;
}

// ============================================================================
// hunter_act_table @ 0x004b4898 - the state 2 ACTION layer, indexed by
// action_behavior. Slot 7 is NULL (unreachable: no code path assigns 7 here),
// slot 8 is the pounce chain re-entry.
// ============================================================================
void (*const hunter_act_table[9])(void) = {
    hunter_act_swipe,       // [0] the standing swipe
    hunter_act_swipe,       // [1]
    hunter_act_pounce,      // [2]
    hunter_act_swipe,       // [3]
    hunter_act_leapattack,  // [4] the flying leap (grab-cancel target)
    hunter_act_flurry,      // [5] the claw flurry
    hunter_act_holdplayer,  // [6] holding the player
    NULL,                   // [7]
    hunter_act_pounce       // [8]
};

// ============================================================================
// hunter_act_swipe @ 0x004187b0 - action slots 0/1/3.
// The standing swipe. Sub-state 0/1 play it out (with the heavy-weapon
// flinch cancel); sub-state 2 re-rolls the next action: another swipe, the
// pounce chain, or - if the player was hit twice in a row - the sidestep.
// ============================================================================
static void hunter_act_swipe(void) // 0x004187b0
{
    ENTITY->animationId = (unsigned char)(4 - (ENTITY->action_behavior == 0));

    unsigned char sub = ENTITY->action_state;
    if (sub == 0) {
        ENTITY->action_state = 1;
        H_SPEED_W = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        Snd_em(6);
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            hunter_seed_from_dead_move();
            Effect_CreateBillboard(3, 8, 0, (void*)((char*)H_JOINTS + 0xC0), &g_playerPosScratch, 0);
            Effect_CreateBillboard(9, 6, 0, (void*)((char*)H_JOINTS + 0xC0), &g_playerPosScratch, 0);
        }
    } else if (sub != 1) {
        if (sub != 2) {
            return;
        }

        int turn = turn_toward_target(PLAYER_T, 0x20);
        g_playerDisplacement = (unsigned int)(short)((short)turn >> 5);

        if ((rand() & 1) == 0) {
            turn = turn_toward_target(PLAYER_T, 0x80);
            g_playerDisplacement = (unsigned int)(short)((short)turn >> 7);
            if (g_playerDisplacement == 0) {
                ENTITY->action_behavior = (unsigned char)(ENTITY->action_behavior + 5);
                ENTITY->ignore_player_flag = 1;
            } else {
                ENTITY->ignore_player_flag = 0;
            }
        }
        if ((ENTITY->behavior_flags & 2) != 0) {
            unsigned char roll = (unsigned char)(rand() & 1);
            ENTITY->ignore_player_flag = roll;
            ENTITY->action_behavior = (unsigned char)(roll * 4 + 2);
            ENTITY->status_flags &= 0x1F;
        }

        ENTITY->player_pos_x = (short)PLAYER_T_INT[0];
        ENTITY->player_pos_z = (short)PLAYER_T_INT[2];
        ENTITY->hit_state = 0;

        if ((ENTITY->behavior_flags & 0x40) == 0) {
            ENTITY->state = 1;
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_behavior = 1;
            ENTITY->action_state = 0;
        } else {
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
        }

        if (H_DEATH_CNT_A == 0) return;
        if (H_DEATH_CNT_B != 2) return;

        // Two hits inside the latch window: break off with the sidestep.
        H_DEATH_CNT_A = 0;
        H_DEATH_CNT_B = 0;
        ENTITY->status_flags &= 0x1F;
        ENTITY->state = 1;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 10;
        ENTITY->action_state = 0;
        return;
    }

    // Sub-states 0 and 1 both land here: advance the swipe.
    ENTITY->action_state = (unsigned char)(ENTITY->action_state +
        (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
    zombie_check_special_weapon();
}

// ============================================================================
// hunter_act_leapattack @ 0x00418a50 - action slot 4.
// The flying leap: crouch, leap with a ballistic arc, mid-air pose, then a
// hover with a randomised timer that can re-roll into the flurry, the
// landing dive (which drags the hunter forward by the animation offsets),
// and the release back to the AI layer - or into the pounce chain when the
// leap flag was rolled.
// ============================================================================
static void hunter_act_leapattack(void) // 0x00418a50
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 7;
        Snd_em(6);
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            hunter_seed_from_dead_move();
            Effect_CreateBillboard(9, 6, 0, (void*)((char*)H_JOINTS + 0xC0), &g_playerPosScratch, 0);
            Effect_CreateBillboard(3, 8, 0, (void*)((char*)H_JOINTS + 0xC0), &g_playerPosScratch, 0);
        }
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state +
            (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
        // fall through
    case 2:
        if (entity_ballistic_step((short)(H_SPEED_W / 2), 0, (short)0xFFCE, 0) != 0) {
            ENTITY->action_state = 3;
            return;
        }
        break;

    case 3:
        ENTITY->action_state = 4;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0x12;
        // fall through
    case 4:
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
            ENTITY->action_state = 5;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 7;
            ENTITY->animationId = 8;
            H_TICKS = (short)(((unsigned short)rand() & 0x20) + 0x2D);
            H_SPEED_W = 0;
            ENTITY->hit_state = 0;
            return;
        }
        break;

    case 5: {
        ENTITY->status_flags = (unsigned char)((ENTITY->status_flags & 0x1F) | 0x20);
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);

        int dz = PLAYER_T_INT[2] - H_POS_T[2];
        int dx = PLAYER_T_INT[0] - H_POS_T[0];
        g_playerDisplacement = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);

        if (H_TICKS == 3 && (rand() & 1) != 0) {
            ENTITY->action_state = 0;
            ENTITY->action_behavior = 6;
        }
        {
            short prev = H_TICKS;
            H_TICKS = (short)(H_TICKS - 1);
            if (prev == 0) {
                ENTITY->action_state = 6;
                return;
            }
        }
        break;
    }

    case 6:
        ENTITY->action_state = 7;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0xF;
        H_SPEED_W = 0x50;
        Add_speedXZ(0xC00);
        ENTITY->hit_state = 1;
        if ((g_RandSeed & 1) != 0 && (ENTITY->behavior_flags & 0xF) != 7) {
            H_LEAP_FLAG |= 1;
        }
        // fall through
    case 7: {
        unsigned char adv = (unsigned char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
        g_playerDisplacement = adv;
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);
        if (ENTITY->animation_frame_id < 0x1F) {
            ENTITY->angle = (short)(ENTITY->angle + 0x40);
        }
        if (ENTITY->animation_frame_id < 0x14) {
            H_POS_T[0] += (short)ENTITY->speed.x;
            H_POS_T[2] += (short)ENTITY->speed.z;
        }
        if (g_playerDisplacement != 0) {
            ENTITY->angle = (short)(ENTITY->angle + 0x800);
        }
        break;
    }

    case 8:
        if ((ENTITY->behavior_flags & 0x40) == 0) {
            ENTITY->state = 1;
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_behavior = 2;
            ENTITY->action_state = 0;
            if ((H_LEAP_FLAG & 1) != 0) {
                ENTITY->state = 1;
                ENTITY->ignore_player_flag = 1;
                ENTITY->action_behavior = 6;
                ENTITY->action_state = 0;
                H_LEAP_FLAG &= 0xFE;
            }
        } else {
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
        }
        H_SPEED_W = 0;
        H_GRAB_WORD = 0;
        ENTITY->player_pos_x = (short)PLAYER_T_INT[0];
        ENTITY->player_pos_z = (short)PLAYER_T_INT[2];
        ENTITY->hit_state = 0;
        ENTITY->status_flags |= 0x40;
        return;
    }
}

// ============================================================================
// hunter_act_pounce @ 0x00418ef0 - action slots 2 and 8.
// The pounce chain. For scripted spawns (flags bit 3) the first block snaps
// the entry yaw and drops the flag. The run phase charges at 0x400/0xC00/
// 0x800 depending on the variant byte; on animation end it re-arms the swipe
// (behaviour word 0x604 = swipe with sub-state 4) or raises the script flag.
// ============================================================================
static void hunter_act_pounce(void) // 0x00418ef0
{
    if (ENTITY->action_state == 0) {
        if ((ENTITY->behavior_flags & 8) != 0) {
            if (ENTITY->animation_frame_id > 5) {
                ENTITY->angle = (short)(ENTITY->angle +
                    hunter_intro_angle_tbl[ENTITY->behavior_flags]);
            }
            ENTITY->behavior_flags = 2;
        }
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        if ((ENTITY->behavior_flags & 8) != 0) {
            ENTITY->blend_counter = 0;
        }
        ENTITY->animationId = 7;
        Snd_em(6);
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) == 0) {
        H_SPEED_W = (ENTITY->animation_frame_id < 3) ? 0xFA : 0x46;
        if (ENTITY->animation_frame_id == 0x10) {
            Snd_em(4);
        }
        if (ENTITY->behavior_flags == 8) {
            if (ENTITY->animation_frame_id > 5) {
                ENTITY->angle = (short)(ENTITY->angle - 0x400);
            }
            Add_speedXZ(0x400);
            return;
        }
        if (ENTITY->behavior_flags == 9) {
            if (ENTITY->animation_frame_id > 5) {
                ENTITY->angle = (short)(ENTITY->angle + 0x400);
            }
            Add_speedXZ(0xC00);
            return;
        }
        Add_speedXZ(0x800);
        return;
    }

    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 7;
    ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
    H_TICKS = (short)(((unsigned short)rand() & 0x20) + 0x1E);
    H_SPEED_W = 0;
    H_BEH_WORD = 0x604;   // behavior 4, action_state 6
    if ((ENTITY->behavior_flags & 8) != 0) {
        ENTITY->behavior_flags = 2;
    }
    if ((ENTITY->behavior_flags & 0x40) != 0) {
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        H_BEH_WORD = 0;
    }
}

// ============================================================================
// hunter_act_flurry @ 0x00419100 - action slot 5.
// The claw flurry: animation 6 with a blood billboard at the arm joint every
// frame, then re-arm the swipe (behaviour word 0x604) or raise the script
// flag for SCD-controlled hunters.
// ============================================================================
static void hunter_act_flurry(void) // 0x00419100
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        H_SPEED_W = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 6;
        Snd_em(6);
    }

    hunter_seed_from_dead_move();
    Effect_CreateBillboard(0, 0, 0, (void*)((char*)H_JOINTS + 0x1B8), &g_playerPosScratch, 0);

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        H_BEH_WORD = 0x604;
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
            H_BEH_WORD = 0;
        }
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 6;
        H_TICKS = 9;
        H_SPEED_W = 0;
        ENTITY->hit_state = 0;
    }
}

// ============================================================================
// hunter_act_holdplayer @ 0x00419260 - action slot 6.
// Holds the grabbed player with animation 0x16 until the script (or the
// death of the grab) releases: then back to the action layer's behaviour 2.
// ============================================================================
static void hunter_act_holdplayer(void) // 0x00419260
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        H_SPEED_W = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0x16;
        ENTITY->hit_state = 1;
    }
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        H_STATE_BLOCK = 0x20001;   // state 1, ignore 0, behavior 2, action 0
        H_SPEED_W = 0;
        ENTITY->hit_state = 0;
    }
}

// ============================================================================
// hunter_death_table @ 0x004b48c0 - the state 3 death layer, indexed by
// action_behavior. Slots 0/1/3 are the common fall; 4 is the mid-air pounce
// death; 5/6 are the settle/collapse variants.
// ============================================================================
void (*const hunter_death_table[8])(void) = {
    hunter_death_fall,     // [0]
    hunter_death_fall,     // [1]
    hunter_death_thrash,   // [2]
    hunter_death_fall,     // [3]
    hunter_death_pounce,   // [4]
    hunter_death_settle,   // [5]
    hunter_death_collapse, // [6]
    NULL                   // [7]
};

// ============================================================================
// hunter_death_fall @ 0x004165e0 - death slots 0/1/3.
// The standard death: animation 0x11 driven by hunter_death_fall_driver, the
// death howl once (shared latch), a second cry at frame 0x4B, and the
// re-center pass that keeps the body on its death joint.
// ============================================================================
static void hunter_death_fall(void) // 0x004165e0
{
    ENTITY->animationId = 0x11;
    H_SPEED_W = 0;
    hunter_death_fall_driver();

    if ((ENTITY->animation_frame_id == 0x32 || ENTITY->animation_frame_id == 0x24) &&
        hunter_scream_latch == 0) {
        Snd_em(7);
        hunter_scream_latch = 1;
    }
    if (ENTITY->animation_frame_id == 0x4B) {
        Snd_em(4);
    }
    if (ENTITY->blend_counter == 0) {
        if (ENTITY->animation_frame_id > 0x17) {
            hunter_recenter_on_joint(1);
            return;
        }
        hunter_recenter_on_joint(0);
    }
}

// ============================================================================
// hunter_death_thrash @ 0x00416680 - death slot 2.
// The thrash: animation 7 with an initial burst speed that settles after
// frame 3, a cry at frames 13 and 23.
// ============================================================================
static void hunter_death_thrash(void) // 0x00416680
{
    ENTITY->animationId = 7;
    H_SPEED_W = (ENTITY->animation_frame_id < 3) ? 0xFA : 0x46;
    hunter_death_fall_driver();
    if (ENTITY->animation_frame_id == 0x0D || ENTITY->animation_frame_id == 0x17) {
        Snd_em(4);
    }
}

// ============================================================================
// hunter_death_settle @ 0x004166e0 - death slot 5. Animation 0x0F, no drive.
// ============================================================================
static void hunter_death_settle(void) // 0x004166e0
{
    ENTITY->animationId = 0x0F;
    H_SPEED_W = 0;
    hunter_death_fall_driver();
    hunter_recenter_on_joint(0);
}

// ============================================================================
// hunter_death_collapse @ 0x00416710 - death slot 6. Animation 6, no drive.
// ============================================================================
static void hunter_death_collapse(void) // 0x00416710
{
    ENTITY->animationId = 6;
    H_SPEED_W = 0;
    hunter_death_fall_driver();
}

// ============================================================================
// hunter_death_pounce @ 0x00419540 - death slot 4.
// Killed mid-pounce: the leap continues ballistically, the body crashes down
// (animation 0x12 + cry), and the loop hands back to the settle animation.
// ============================================================================
static void hunter_death_pounce(void) // 0x00419540
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        H_TICKS = 0x46;
        ENTITY->animationId = 0x11;
        Snd_em(7);
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state +
            (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
        // fall through
    case 2:
        if (entity_ballistic_step((short)(H_SPEED_W / 2), 0, (short)0xFFCE, 0) != 0) {
            ENTITY->action_state = 3;
            H_SPEED_W = 0;
            H_GRAB_WORD = 0;
            return;
        }
        break;

    case 3:
        ENTITY->action_state = 4;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->animationId = 0x12;
        Snd_em(4);
        // fall through
    case 4:
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 2;
        }
        break;
    }
}

// ============================================================================
// hunter_death_fall_driver @ 0x00419310
// The shared death animation driver. Sub-state 0/1 play the fall with a
// forward drift; 2 tints the corpse quad and raises the death event flag;
// 3 shrinks the fade quad over the tick counter; 4 raises the script flag
// for SCD-controlled hunters.
// ============================================================================
static void hunter_death_fall_driver(void) // 0x00419310
{
    switch (ENTITY->action_state) {
    case 0:
        H_HEALTH = -1;
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        H_TICKS = 0x46;
        Snd_em(7);
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            hunter_seed_from_dead_move();
            void* spriteInfo = (void*)((char*)H_JOINTS + 0xC0);
            Effect_CreateBillboard(9, 6, 0, spriteInfo, &g_playerPosScratch, 0);
            Effect_CreateBillboard(3, 8, 0, spriteInfo, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0, 0, spriteInfo, &g_playerPosScratch, 0);
        }
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state +
            (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200));
        Add_speedXZ(0x800);
        break;

    case 2:
        BillboardSetColor((short*)((char*)ENTITY + 0xE4), 1, 2, 0x00FFFF50);
        BillboardAdjSize((short*)((char*)ENTITY + 0xE4), -100, -100);
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        ENTITY->action_state = 3;
        ENTITY->status_flags |= 0x0A;
        H_SPEED_W = 0;
        return;

    case 3:
        BillboardAdjSize((short*)((char*)ENTITY + 0xE4), 12, 12);
        H_TICKS = (short)(H_TICKS - 1);
        if (H_TICKS == 0) {
            ENTITY->action_state = 4;
        }
        break;

    case 4:
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        }
        break;
    }
}

// ============================================================================
// hunter_track_player_joint @ 0x004199e0
// The hold-the-player helper: recomposes the held joint chain's world
// matrices from the hunter's yaw, copies the result into the chain's world
// slots, and drags the PLAYER's head joint (joints[1]) to the mouth joint's
// position with a small yaw-dependent offset - the player's head stays in
// the hunter's jaws while it holds/shakes.
// `sel` is Entity+0x17C, the mouth joint selector the behaviours maintain.
// ============================================================================
static void hunter_track_player_joint(void) // 0x004199e0
{
    unsigned short sel = H_JOINT_SEL;
    JointStruct* joints = H_JOINTS;
    RotMatrix(H_ROT, H_LOCAL_MATRIX);
    ApplyLVAndMul0Matrix(H_LOCAL_MATRIX, &joints->transform, &g_matrixScratch);
    ApplyLVAndMulMatrix(&g_matrixScratch, (MATRIX*)((char*)joints + 0xA0));
    for (int i = 3; i != 0; i--) {
        ApplyLVAndMulMatrix(&g_matrixScratch,
                            (MATRIX*)((char*)joints + sel * 0x7C + i * -0x7C + 0xA0));
    }
    // Copy the composed rotation block into the mouth joint's world matrix.
    char* base = (char*)joints + sel * 0x7C;
    memcpy((char*)joints + sel * 0x7C + 0x44, &g_matrixScratch.m, 32);

    JointStruct* pj = g_playerEntity.jointsStructs;
    pj[1].world.t[0] = *(int*)(base + 0x58);
    pj[1].world.t[1] = *(int*)(base + 0x5C);
    pj[1].world.t[2] = *(int*)(base + 0x60);
    pj[1].unk_64 = *(int*)(base + 0x64);

    g_svecScratch.x = 200;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    hunter_copy_dead_move_matrix();
    RotMatrixY((11 - sel) * 600 + ENTITY->angle, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);
    pj[1].world.t[0] += g_svecScratch.x;
    pj[1].world.t[2] += g_svecScratch.z;
}

// ============================================================================
// hunter_recenter_on_joint @ 0x00419b50
// Death-layer body placement: recomposes the death joint chain (joint 12 for
// `which` 0, joint 15 for `which` 1) and shifts the ENTITY's position by the
// difference, so the falling body pivots around the joint that hit the
// ground instead of sliding through the floor.
// ============================================================================
static void hunter_recenter_on_joint(unsigned char which) // 0x00419b50
{
    JointStruct* joints = H_JOINTS;
    RotMatrix(H_ROT, H_LOCAL_MATRIX);
    ApplyLVAndMul0Matrix(H_LOCAL_MATRIX, &joints->transform, &g_matrixScratch);

    char* base = (char*)joints + (which + 4) * 0x174;
    for (int b = 3; b != 0; b--) {
        ApplyLVAndMulMatrix(&g_matrixScratch, (MATRIX*)(base + b * -0x7C + 0xA0));
    }
    g_matrixScratch.t[1] = 0;
    g_matrixScratch.t[0] -= *(int*)(base + 0x58);
    g_matrixScratch.t[2] -= *(int*)(base + 0x60);
    H_POS_T[0] -= g_matrixScratch.t[0];
    H_POS_T[2] -= g_matrixScratch.t[2];
}

// ============================================================================
// hunter_scd_state_dispatch @ 0x0048f410 - state 8.
// SCD-script-controlled hunters (flags bit 0x40): without the bit the state
// falls back to 1; with it, the enemy list base is parked at 0x004d3d50 for
// the tracking helper and the script's behaviour byte indexes
// hunter_scd_table.
// ============================================================================
static void hunter_scd_state_dispatch(void) // 0x0048f410
{
    if ((ENTITY->behavior_flags & 0x40) == 0) {
        ENTITY->state = 1;
        return;
    }
    hunter_scd_target = g_EnemiesList;
    hunter_scd_table[ENTITY->action_behavior]();
}

// ============================================================================
// hunter_scd_table @ 0x004d3d58 - THIRTY-NINE entries, ending where the
// "it failed to the Lock" string data begins at 0x004d3df4. The scripts only
// ever request a subset directly (0-3, 7, 14-23); slots 10-13 are composite
// wrappers that re-enter THIS table at bases [24] (dodge), [28] (pounce),
// [32] (swipe) and [36] (bite) indexed by action_state (+0x87), which is why
// the sub-range handlers share the array with everything else.
// ============================================================================
void (*const hunter_scd_table[39])(void) = {
    hunter_scd_idle,           // [0]  0x0048f450
    NULL,                      // [1]
    hunter_scd_walk,           // [2]  0x0048f4d0
    hunter_scd_run,            // [3]  0x0048f680
    NULL,                      // [4]
    NULL,                      // [5]
    NULL,                      // [6]
    hunter_scd_death,          // [7]  0x0048fc80
    NULL,                      // [8]
    NULL,                      // [9]
    hunter_scd_dodge_run,      // [10] 0x0048f820 -> [24 + action_state]
    hunter_scd_pounce_run,     // [11] 0x0048f8a0 -> [28 + action_state]
    hunter_scd_attack_run,     // [12] 0x0048f9e0 -> [36 + action_state]
    hunter_scd_swipe_run,      // [13] 0x0048f960 -> [32 + action_state]
    hunter_act_swipe,          // [14] 0x004187b0
    hunter_act_leapattack,     // [15] 0x00418a50
    hunter_act_pounce,         // [16] 0x00418ef0
    hunter_act_flurry,         // [17] 0x00419100
    hunter_behavior_scream,    // [18] 0x00418400
    hunter_death_fall,         // [19] 0x004165e0
    hunter_death_collapse,     // [20] 0x00416710
    hunter_death_thrash,       // [21] 0x00416680
    hunter_scd_stalk_player,   // [22] 0x0048fef0
    hunter_behavior_ledgejump, // [23] 0x00418220
    hunter_dodge_start,        // [24] 0x00417aa0  dodge sub-range base
    hunter_dodge_hop,          // [25] 0x00417b40
    hunter_dodge_swipe,        // [26] 0x00417ba0
    hunter_scd_anim_release,   // [27] 0x0048f840
    hunter_pounce_start,       // [28] 0x00417800  pounce sub-range base
    hunter_pounce_bite,        // [29] 0x00417870
    hunter_scd_flag_release,   // [30] 0x0048f930
    NULL,                      // [31]
    hunter_atk_start,          // [32] 0x00417430  swipe sub-range base
    hunter_atk_swing,          // [33] 0x00417480
    hunter_scd_tint_release,   // [34] 0x0048f980
    NULL,                      // [35]
    hunter_scd_lunge_start,    // [36] 0x0048fa70  bite sub-range base
    hunter_scd_bite_driver,    // [37] 0x0048fae0
    hunter_scd_bite_end        // [38] 0x0048fc10
};

// ============================================================================
// hunter_scd_idle @ 0x0048f450 - script slot 0. Parks the hunter looping
// animation 0x15 until the script moves it.
// ============================================================================
static void hunter_scd_idle(void) // 0x0048f450
{
    if (ENTITY->action_state == 0) {
        ENTITY->animationId = 0x15;
        H_SPEED_W = 0;
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
    }
    Joint_move((int)(ENTITY->scd_entity_flags & 1),
               ENTITY->animHeader, ENTITY->animBase, 0x80);
}

// ============================================================================
// hunter_scd_walk @ 0x0048f4d0 - script slot 2. Walks toward the SCD waypoint
// pair (+0xC6/+0xC8), footstep sounds, and hands back to the script (or loops
// while the script's wait latch at +0xDC bit 7 is set) on arrival.
// ============================================================================
static void hunter_scd_walk(void) // 0x0048f4d0
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->animationId = 0x10;
        H_SPEED_W = 0x28;
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        H_JOINT_SEL = 0;
        // fall through
    case 1: {
        g_playerPosScratch.x = (unsigned short)ENTITY->unk_c6;
        g_playerPosScratch.z = (unsigned short)ENTITY->unk_c8;
        g_playerPosScratch.y = 0;
        entity_rotate_toward_target(&g_playerPosScratch, 0x40);
        Joint_move((int)(ENTITY->scd_entity_flags & 1),
                   ENTITY->animHeader, ENTITY->animBase, 0x80);
        Add_speedXZ(0);
        if (ENTITY->animation_frame_id == 1)  Snd_em(0);
        if (ENTITY->animation_frame_id == 32) Snd_em(0);
        int dz = H_POS_T[2] - (unsigned short)ENTITY->unk_c8;
        int dx = H_POS_T[0] - (unsigned short)ENTITY->unk_c6;
        if (SquareRoot0(dz * dz + dx * dx) < 0x96) {
            ENTITY->action_state = (unsigned char)(ENTITY->action_state + 1);
        }
        return;
    }
    case 2:
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        if ((ENTITY->collisionFlags & 0x80) != 0) {
            ENTITY->action_state = 1;
            return;
        }
        H_BEH_WORD = 0;
        return;
    default:
        return;
    }
}

// ============================================================================
// hunter_scd_run @ 0x0048f680 - script slot 3. Same skeleton as the walk with
// animation 2, speed 200 and a faster turn step.
// ============================================================================
static void hunter_scd_run(void) // 0x0048f680
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        ENTITY->animationId = 2;
        H_SPEED_W = 200;
        // fall through
    case 1: {
        g_playerPosScratch.x = (unsigned short)ENTITY->unk_c6;
        g_playerPosScratch.z = (unsigned short)ENTITY->unk_c8;
        g_playerPosScratch.y = 0;
        Joint_move((int)(ENTITY->scd_entity_flags & 1),
                   ENTITY->animHeader, ENTITY->animBase, 0x80);
        if (ENTITY->animation_frame_id == 1)  Snd_em(1);
        if (ENTITY->animation_frame_id == 15) Snd_em(1);
        entity_rotate_toward_target(&g_playerPosScratch, 0x60);
        Add_speedXZ(0);
        int dz = H_POS_T[2] - (unsigned short)ENTITY->unk_c8;
        int dx = H_POS_T[0] - (unsigned short)ENTITY->unk_c6;
        if (SquareRoot0(dz * dz + dx * dx) < 0x96) {
            ENTITY->action_state = (unsigned char)(ENTITY->action_state + 1);
        }
        return;
    }
    case 2:
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        if ((ENTITY->collisionFlags & 0x80) != 0) {
            ENTITY->action_state = 1;
            return;
        }
        H_BEH_WORD = 0;
        return;
    default:
        return;
    }
}

// ============================================================================
// hunter_scd_death @ 0x0048fc80 - script slot 7. The scripted death: two
// animation steps, then the head-joint tracking pass so the corpse follows
// the script's camera focus.
// ============================================================================
static void hunter_scd_death(void) // 0x0048fc80
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
        H_SPEED_W = 0;
        // fall through
    case 1:
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
            ENTITY->action_state = 2;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->animationId = (unsigned char)(ENTITY->animationId + 1);
        }
        hunter_scd_track_joint(0);
        return;
    case 2:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
        hunter_scd_track_joint(0);
        return;
    default:
        return;
    }
}

// ============================================================================
// hunter_scd_track_joint @ 0x0048fd80
// The SCD flavour of hunter_track_player_joint: recomposes the mouth joint
// chain and writes the result into the SCD TARGET ENTITY's look-at slot
// (+0xD4/+0xD8/+0xDC/+0xE0 of the entity parked at 0x004d3d50) instead of
// the player's head joint, with the same yaw-dependent offset.
// The original's single stack parameter is unused in the body.
// ============================================================================
static void hunter_scd_track_joint(int unused) // 0x0048fd80
{
    (void)unused;
    JointStruct* joints = H_JOINTS;
    RotMatrix(H_ROT, H_LOCAL_MATRIX);
    CompMatrix(H_LOCAL_MATRIX, &joints->transform, &g_matrixScratch);
    CompMatrix(&g_matrixScratch, (MATRIX*)((char*)joints + 0xA0), &g_matrixScratch);
    for (int i = 3; i != 0; i--) {
        CompMatrix(&g_matrixScratch, (MATRIX*)((char*)joints + i * -0x7C + 0x388), &g_matrixScratch);
    }
    memcpy((char*)joints + 0x32C, &g_matrixScratch.m, 32);

    JointStruct* tj = hunter_scd_target->jointsStructs;
    *(int*)((char*)tj + 0xD4) = *(int*)((char*)joints + 0x340);
    *(int*)((char*)tj + 0xD8) = *(int*)((char*)joints + 0x344);
    *(int*)((char*)tj + 0xDC) = *(int*)((char*)joints + 0x348);
    *(int*)((char*)tj + 0xE0) = *(int*)((char*)joints + 0x34C);

    g_svecScratch.x = 200;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    hunter_copy_dead_move_matrix();
    RotMatrixY((11 - H_JOINT_SEL) * 600 + ENTITY->angle, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);
    *(int*)((char*)tj + 0xD4) += g_svecScratch.x;
    *(int*)((char*)tj + 0xDC) += g_svecScratch.z;
}

// ============================================================================
// hunter_scd_dodge_run @ 0x0048f820 - script slot 10. Re-enters
// hunter_scd_table at the dodge sub-range base ([24]) with action_state,
// then applies the current movement speed.
// ============================================================================
static void hunter_scd_dodge_run(void) // 0x0048f820
{
    hunter_scd_table[24 + ENTITY->action_state]();
    Add_speedXZ(0);
}

// ============================================================================
// hunter_scd_anim_release @ 0x0048f840 - script slot 27. Holds the animation
// blend to completion (blend step 0x400), then raises the script flag and
// hands control back (behaviour+action word cleared).
// ============================================================================
static void hunter_scd_anim_release(void) // 0x0048f840
{
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        H_BEH_WORD = 0;
    }
}

// ============================================================================
// hunter_scd_pounce_run @ 0x0048f8a0 - script slot 11. Re-enters the table at
// the pounce sub-range base ([28]); while animation_frame_id < 0x17 it homes
// on the waypoint pair (+0x178/+0x17A) with a slow turn (step 0x18), then
// charges (speed 0x32, angle offset 0x800) until frame 0x1F.
// ============================================================================
static void hunter_scd_pounce_run(void) // 0x0048f8a0
{
    hunter_scd_table[28 + ENTITY->action_state]();
    if (ENTITY->animation_frame_id < 0x17) {
        g_playerPosScratch.x = H_TARGET_X;
        g_playerPosScratch.z = H_TARGET_Z;
        g_playerPosScratch.y = 0;
        ENTITY->angle = (short)(ENTITY->angle +
                                (short)turn_toward_target(&g_playerPosScratch, 0x18));
        Add_speedXZ(0);
        return;
    }
    if (ENTITY->animation_frame_id < 0x1F) {
        H_SPEED_W = 0x32;
        Add_speedXZ(0x800);
    }
}

// ============================================================================
// hunter_scd_flag_release @ 0x0048f930 - script slot 30 (pounce tail). Pure
// completion: raise the script flag and hand back.
// ============================================================================
static void hunter_scd_flag_release(void) // 0x0048f930
{
    Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
    H_BEH_WORD = 0;
}

// ============================================================================
// hunter_scd_swipe_run @ 0x0048f960 - script slot 13. Re-enters the table at
// the swipe sub-range base ([32]), then recomposes the death joint chain
// around joint 15 so the swipe pivots around the hitting claw.
// ============================================================================
static void hunter_scd_swipe_run(void) // 0x0048f960
{
    hunter_scd_table[32 + ENTITY->action_state]();
    hunter_recenter_on_joint(1);
}

// ============================================================================
// hunter_scd_tint_release @ 0x0048f980 - script slot 34 (swipe tail). Tints
// the claw joint (+0x45C into the joint block) before the standard completion.
// ============================================================================
static void hunter_scd_tint_release(void) // 0x0048f980
{
    JointApplyColorTint((JointStruct*)((char*)H_JOINTS + 0x45C), 0x30, 0x80820,
                        (void*)0x606060);
    Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
    H_BEH_WORD = 0;
}

// ============================================================================
// hunter_scd_attack_run @ 0x0048f9e0 - script slot 12. Same skeleton as the
// pounce driver but over the bite sub-range base ([36]).
// ============================================================================
static void hunter_scd_attack_run(void) // 0x0048f9e0
{
    hunter_scd_table[36 + ENTITY->action_state]();
    if (ENTITY->animation_frame_id < 0x17) {
        g_playerPosScratch.x = H_TARGET_X;
        g_playerPosScratch.z = H_TARGET_Z;
        g_playerPosScratch.y = 0;
        ENTITY->angle = (short)(ENTITY->angle +
                                (short)turn_toward_target(&g_playerPosScratch, 0x18));
        Add_speedXZ(0);
        return;
    }
    if (ENTITY->animation_frame_id < 0x1F) {
        H_SPEED_W = 0x32;
        Add_speedXZ(0x800);
    }
}

// ============================================================================
// hunter_scd_lunge_start @ 0x0048fa70 - script slot 36. One-shot lunge setup:
// action_state 1, blend counter 7, run speed 300, animation 0xC, growl SFX.
// The original computes 0x12 - joint_sel byte into animationId and then
// immediately overwrites it with 0xC; kept for fidelity.
// ============================================================================
static void hunter_scd_lunge_start(void) // 0x0048fa70
{
    ENTITY->action_state = 1;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 7;
    H_SPEED_W = 300;
    ENTITY->animationId = (unsigned char)(0x12 - (unsigned char)H_JOINT_SEL);
    ENTITY->animationId = 0xC;
    Snd_em(2);
}

// ============================================================================
// hunter_scd_bite_driver @ 0x0048fae0 - script slot 37. Plays the lunge-in
// (blend 0x200); during frames 7-14, once the SCD TARGET's bite-joint flag
// bit 6 clears, the grab lands: the target is knocked into its own state 1 /
// behaviour 2, this hunter parks in action_state 2 for four ticks, a blood
// billboard spawns off the mouth matrix (+0x32C) seeded with x=200 from the
// dead-move block (+0x18/+0x1C/+0x20), the neck joint (+0x2E8) tints and the
// bite SFX plays.
// ============================================================================
static void hunter_scd_bite_driver(void) // 0x0048fae0
{
    if ((unsigned char)(ENTITY->animation_frame_id - 7) < 8 &&
        (hunter_scd_target->jointsStructs[1].flags & 0x40) == 0) {
        hunter_scd_target->hit_state = 1;
        *(unsigned int*)((char*)hunter_scd_target + 0x84) = 0x20001;
        ENTITY->action_state = 2;
        H_TICKS = 4;
        JointStruct* joints = H_JOINTS;
        g_playerPosScratch.y = *(const int*)(H_DMV + 0x18);
        g_playerPosScratch.z = *(const int*)(H_DMV + 0x1C);
        g_playerPosScratch.pad = *(const int*)(H_DMV + 0x20);
        g_playerPosScratch.x = 200;
        Effect_CreateBillboard(0, 0, 0, (void*)((char*)joints + 0x32C),
                               &g_playerPosScratch, 0);
        JointApplyColorTint((JointStruct*)((char*)joints + 0x2E8), 0x30, 0x80820,
                            (void*)0x606060);
        Snd_em(5);
        return;
    }
    char moved = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
    ENTITY->action_state = (unsigned char)(ENTITY->action_state + moved);
}

// ============================================================================
// hunter_scd_bite_end @ 0x0048fc10 - script slot 38. Finishes the bite
// animation, raises the script flag and hands back through behaviour 7 (the
// scripted death); clears the SCD TARGET's bite-joint latch (joint 1 bit 0)
// and runs the head-joint tracking pass against joint 6.
// ============================================================================
static void hunter_scd_bite_end(void) // 0x0048fc10
{
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        H_BEH_WORD = 7;
    }
    hunter_scd_target->jointsStructs[1].flags &= (unsigned char)~0x01;
    hunter_scd_track_joint(6);
}

// ============================================================================
// hunter_scd_stalk_player @ 0x0048fef0 - script slot 22. Face-and-follow:
// one-shot init picks animation 0x10 when a turn is needed and rolls a stalk
// timer of (rand & 0x3F) + 0x1E ticks; while it lasts the hunter shuffles
// toward the player (blend 0x80, turn step 100); on expiry it raises the
// flag, hands back (behaviour word 0) and stores the player position into
// the waypoint pair (+0x166/+0x168).
// ============================================================================
static void hunter_scd_stalk_player(void) // 0x0048fef0
{
    if (ENTITY->action_state == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        ENTITY->hit_state = 0;
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t,
                0x400) != 0) {
            ENTITY->animationId = 0x10;
        }
        ENTITY->action_state = 1;
        H_TICKS = (unsigned short)((rand() & 0x3F) + 0x1E);
    }
    short delta = (short)turn_toward_target(
        (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 100);
    g_animFrameIdSave = (unsigned int)(unsigned short)delta;
    short ticks = (short)H_TICKS;
    H_TICKS = (unsigned short)(ticks - 1);
    if (ticks != 0 && (short)g_animFrameIdSave != 0) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
        ENTITY->angle = (short)(ENTITY->angle + (short)g_animFrameIdSave);
        return;
    }
    Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
    H_BEH_WORD = 0;
    ENTITY->player_pos_x = (short)PLAYER_T_INT[0];
    ENTITY->player_pos_z = (short)PLAYER_T_INT[2];
}
