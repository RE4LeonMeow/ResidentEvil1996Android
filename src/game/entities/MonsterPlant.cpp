// MonsterPlant.cpp - Monster plant (enemy type 15, enemy/em100f.emd).
//
// Original PC addresses:
//   monster_plant_update      0x0045abb0   per-frame entry (table[15] @ 0x004d3ccc)
//   monster_plant_init        0x0045ac80   state 0
//   monster_plant_state_check 0x0045ade0   state 1
//   monster_plant_damaged     0x0045c110   state 2
//   monster_plant_die         0x0045c160   state 3
//   monster_plant_noop        0x0045c440   state 4  (bare RET)
//   monster_plant_select      0x0045aec0   variant selector
//   monster_plant_idle_fidget 0x0045c580   shared idle-sway helper
//   (0x0045c1f0 - the damaged-state countdown case of the switch inside
//    monster_plant_damaged/die; Ghidra lists it separately, port inlines it)
//
// ---------------------------------------------------------------------------
// THREE OVERLAPPING TABLE BASES IN ONE POINTER BLOCK
// ---------------------------------------------------------------------------
// 0x004c0fe8 is NOT one table. The block from 0x004c0fe8 to 0x004c107f is read
// through four different bases, and every one of them was verified by
// re-reading `base + idx*4` one entry at a time (see the count-with-aligned-
// reads note in the project memory - eyeballing a NULL run here would have
// split it in the wrong place):
//
//   0x004c0fe8  s_mpState[5]     indexed by Entity+0x84  (state)
//   0x004c0ffc  s_mpBehavior[7]  indexed by Entity+0x85  (behaviour)   = base+5
//   0x004c1018  s_mpVariant[10]  indexed by behavior_flags & 0xF       = base+12
//   0x004c1040  s_mpSub[4]       indexed by behavior_flags & 0xF       = base+22
//   0x004c1050  s_mpStep[12]     COMPILER JUMPTABLE, three bases:
//                                  behaviour 2 -> +0   (0x0045b610)
//                                  behaviour 3 -> +6   (0x0045bb00)
//                                  behaviour 4 -> +8   (0x0045bcb0)
//
// The three step bases are why Ghidra shows 0x0045b610 / 0x0045bb00 /
// 0x0045bcb0 as three overlapping functions with byte-identical tails: they are
// one 12-case switch entered at three different offsets. Behaviour 3's "case 0"
// IS behaviour 2's "case 6", and behaviour 4's "case 0" IS behaviour 2's
// "case 8". Transcribing Ghidra's three views as three independent functions
// would triplicate the shared vine code and desynchronise the step counter.
//
// s_mpVariant is 10 entries, not 16, even though the index is masked with 0xF:
// 0x004c1040 is provably a different base (behaviour 1 CALLs through it), so
// the array cannot run past it. The 10 entries cover every behavior_flags value
// the rooms actually use - 0,1 -> variant 0; 2; 3; 4; 5; 6,7; 8,9 (0x29 & 0xF) -
// and the duplicated adjacent slots are what make that reading self-consistent.
// s_mpSub likewise stops at 4 because 0x004c1050 is a jumptable; its entry 3 is
// a genuine NULL in the original, so behavior_flags & 0xF == 3 would fault
// there too. Both are bounds-guarded here rather than reproducing the fault.
//
// ---------------------------------------------------------------------------
// WHAT THE ENTITY IS
// ---------------------------------------------------------------------------
// The plant is a 15-segment vine. Segment N is JointStruct N (0x7C stride) off
// Entity+0x98, and bit 0 of each joint's `flags` is its visibility: the vine
// "strikes" by revealing segments from the tip down to a stop index, and
// "retracts" by hiding them from 0 upward. The two stop tables are
// s_mpExtendStops (0x004c1080) and s_mpRetractStops (0x004c1088).
//
// Joint 11's WORLD translation (Entity+0x98 -> +0x5AC, i.e. joints[11].world.t)
// is the vine head. monster_plant_update writes head-minus-body into the SCA
// hit vector at Entity+0x08 every frame, which is what makes the hitbox track
// the extended vine instead of the base, and feeds the same point to
// entity_add_fade_sprite so the ground shadow follows the head.
//
// ---------------------------------------------------------------------------
// FIELD WIDTHS
// ---------------------------------------------------------------------------
// Offsets 0x170-0x188 are deliberately raw. docs/ZOMBIE_STATE_MACHINE.md
// already records that Entity+0x182 is "mixed by entity type: every zombie site
// is byte ptr, but monster_plant_update and two others use word ptr at the same
// offset, including DEC word ptr". That is true of nearly the whole tail here:
// the plant uses 0x170/0x172/0x174/0x176/0x178/0x17A/0x17C/0x17E/0x180/0x182/
// 0x188 as 16-bit values and 0x16C as a full 32-bit distance. Reading any of
// them at the port's zombie-shaped byte width truncates a timer or a distance.
// 0x184 is a 32-bit snapshot of the 0x84 state dword (state/behaviour/step/
// substep in one store), which is how monster_plant_damaged restores the
// behaviour it interrupted.
#include "EntityCommon.h"
#include "../../Globals.h"
#include "../BioCard.h"
#include <cstdlib>

extern void ResetJointTransforms(void);                                   // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);                  // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData);        // 0x00462d90 - Room.cpp
// 0x00473b10 / 0x00473d10. Both take SIGNED 16-bit tint deltas and UNSIGNED
// 16-bit queue words - see the Plant42.cpp note for scd_model_tint_apply. The
// same defect was still live in FUN_00473d10's declaration when this file was
// written: p4/p5 were `unsigned char` there, which silently truncated the
// plant's `0x100` fade parameter to 0 and killed the poison variant's tint.
extern void scd_model_tint_apply(short p1, short p2, short p3,
                                 unsigned short p4, unsigned short p5,
                                 unsigned char p6);                       // 0x00473b10
extern void FUN_00473d10(short p1, short p2, short p3,
                         unsigned short p4, unsigned short p5,
                         char p6);                                        // 0x00473d10

namespace {

// ---------------------------------------------------------------------------
// Raw-offset accessors. Every width here is one the original actually uses.
// ---------------------------------------------------------------------------
#define MP_DIST     (*(int*)  ((char*)ENTITY + 0x16C))  // Manhattan distance to player
#define MP_HOLDOFF  (*(short*)((char*)ENTITY + 0x170))  // re-engage lockout
#define MP_ANIM_END (*(short*)((char*)ENTITY + 0x172))  // Joint_move "animation finished"
#define MP_SEG      (*(short*)((char*)ENTITY + 0x174))  // visible-segment cursor
#define MP_TIMER_A  (*(short*)((char*)ENTITY + 0x176))  // strike/drain/tint timer
#define MP_TIMER_B  (*(short*)((char*)ENTITY + 0x178))  // second tint timer
#define MP_FIDGET   (*(short*)((char*)ENTITY + 0x17A))  // idle-sway countdown
#define MP_ALERTED  (*(short*)((char*)ENTITY + 0x17C))  // 1 = never engaged yet
#define MP_ANGLE_BK (*(short*)((char*)ENTITY + 0x17E))  // saved yaw / red tint steps left
#define MP_SWERVE   (*(short*)((char*)ENTITY + 0x180))  // lunge side / green tint steps left
#define MP_SHADOW   (*(short*)((char*)ENTITY + 0x182))  // bit 0 = draw ground shadow
#define MP_STATE32  (*(unsigned int*)((char*)ENTITY + 0x84))
#define MP_STATE_BK (*(unsigned int*)((char*)ENTITY + 0x184))
#define MP_HITS     (*(short*)((char*)ENTITY + 0x188))  // hits taken

// Entity+0x86/0x87 are written together as one word in three places.
#define MP_STEP16   (*(unsigned short*)((char*)ENTITY + 0x86))

// The port's Entity names for the fields the plant reuses.
#define MP_BEHAVIOR (ENTITY->ignore_player_flag)  // 0x85 - s_mpBehavior index
#define MP_STEP     (ENTITY->action_behavior)     // 0x86 - s_mpStep index
#define MP_TICKS    (ENTITY->action_ticks_counter)// 0xC4 - already u16 in Entity

// Segment count. init and two variants clear exactly 15 joints, and em100f.emd
// declares 15 in its header.
const int MP_SEGMENTS = 15;

inline JointStruct* mp_joints(void) { return ENTITY->jointsStructs; }

// The vine head: joints[11].world.t, i.e. Entity+0x98 -> +0x5AC/0x5B0/0x5B4.
inline int* mp_head(void) { return mp_joints()[11].world.t; }

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------

// 0x004c0fd8 - the plant's Sca_info collision record: six shorts, the same
// shape as Plant 42's. Field 5 (0x190 = 400) is the body radius.
const short s_mpScaInfo[6] = { (short)0x8000, 0, 0, 0, 0, 0x0190 };

// 0x004c0fe4 - shared across every plant in the room: a shift register of the
// sides previously chosen, so two consecutive lunges do not pick the same side
// three times running. Reset by init, consumed by variant 8.
unsigned int s_mpLastSides = 0;

// 0x004c1080 - vine EXTEND stops, indexed by MP_TICKS (guarded < 4 by the
// caller, so exactly 4 entries). The reveal loop walks MP_SEG DOWN to the stop.
const short s_mpExtendStops[4] = { 8, 4, 2, 0 };

// 0x004c1088 - vine RETRACT stops, indexed by MP_TICKS. The hide loop walks
// MP_SEG UP to the stop; the caller's `MP_SEG < 15` guard is what bounds the
// index, and it trips at entry 10 (value 14), so entry 11's 0 is never read.
// The two leading zeroes are real: they buy a one-frame pause before segment 1
// starts to disappear.
const short s_mpRetractStops[12] = { 0, 0, 1, 2, 3, 4, 5, 7, 9, 12, 14, 0 };

// ---------------------------------------------------------------------------
// Forward declarations (the tables below mirror the original's data layout)
// ---------------------------------------------------------------------------
void mp_init(void);            void mp_state_check(void);
void mp_damaged(void);         void mp_die(void);
void mp_noop(void);

void mp_behavior_0(void);      void mp_behavior_1(void);
void mp_behavior_2(void);      void mp_behavior_3(void);
void mp_behavior_4(void);      void mp_behavior_5(void);
void mp_behavior_6(void);

void mp_variant_0(void);       void mp_variant_2(void);
void mp_variant_3(void);       void mp_variant_4(void);
void mp_variant_5(void);       void mp_variant_6(void);
void mp_variant_8(void);

void mp_sub_0(void);           void mp_sub_1(void);
void mp_sub_2(void);

void mp_step_00(void); void mp_step_01(void); void mp_step_02(void);
void mp_step_03(void); void mp_step_04(void); void mp_step_05(void);
void mp_step_06(void); void mp_step_07(void); void mp_step_08(void);
void mp_step_10(void); void mp_step_11(void);

void mp_idle_fidget(void);

// 0x004c0fe8 - state table, indexed by Entity+0x84.
void (*const s_mpState[5])(void) = {
    /* 0  0x0045ac80 */ mp_init,
    /* 1  0x0045ade0 */ mp_state_check,
    /* 2  0x0045c110 */ mp_damaged,
    /* 3  0x0045c160 */ mp_die,
    /* 4  0x0045c440 */ mp_noop,
};

// 0x004c0ffc - behaviour table, indexed by Entity+0x85. Entry 0 is a bare RET
// in the original and is unreachable: every variant handler leaves 0x85 in
// 1..6 before mp_state_check dispatches.
void (*const s_mpBehavior[7])(void) = {
    /* 0  0x0045c450 */ mp_behavior_0,
    /* 1  0x0045b320 */ mp_behavior_1,
    /* 2  0x0045b610 */ mp_behavior_2,
    /* 3  0x0045bb00 */ mp_behavior_3,
    /* 4  0x0045bcb0 */ mp_behavior_4,
    /* 5  0x0045bfa0 */ mp_behavior_5,
    /* 6  0x0045bff0 */ mp_behavior_6,
};

// 0x004c1018 - variant table, indexed by behavior_flags & 0xF. The duplicated
// pairs are in the original data, not a transcription slip.
void (*const s_mpVariant[10])(void) = {
    /* 0  0x0045af00 */ mp_variant_0,
    /* 1  0x0045af00 */ mp_variant_0,
    /* 2  0x0045af30 */ mp_variant_2,
    /* 3  0x0045afc0 */ mp_variant_3,
    /* 4  0x0045b000 */ mp_variant_4,
    /* 5  0x0045b080 */ mp_variant_5,
    /* 6  0x0045b160 */ mp_variant_6,
    /* 7  0x0045b160 */ mp_variant_6,
    /* 8  0x0045b200 */ mp_variant_8,
    /* 9  0x0045b200 */ mp_variant_8,
};

// 0x004c1040 - behaviour 1's per-variant tick, indexed by behavior_flags & 0xF.
// Entry 3 is NULL in the original.
void (*const s_mpSub[4])(void) = {
    /* 0  0x0045b430 */ mp_sub_0,
    /* 1  0x0045b460 */ mp_sub_1,
    /* 2  0x0045b4e0 */ mp_sub_2,
    /* 3  -          */ nullptr,
};

// 0x004c1050 - the shared 12-case step jumptable. Behaviour 2 enters at 0,
// behaviour 3 at 6, behaviour 4 at 8. Entry 9 aliases entry 3 in the original.
void (*const s_mpStep[12])(void) = {
    /*  0  0x0045b670 */ mp_step_00,
    /*  1  0x0045b7c0 */ mp_step_01,
    /*  2  0x0045b840 */ mp_step_02,
    /*  3  0x0045b910 */ mp_step_03,
    /*  4  0x0045b9e0 */ mp_step_04,
    /*  5  0x0045ba30 */ mp_step_05,
    /*  6  0x0045bb30 */ mp_step_06,
    /*  7  0x0045bc30 */ mp_step_07,
    /*  8  0x0045bd00 */ mp_step_08,
    /*  9  0x0045b910 */ mp_step_03,
    /* 10  0x0045be60 */ mp_step_10,
    /* 11  0x0045bec0 */ mp_step_11,
};

// The original JMPs through s_mpStep with a zero-extended byte and no bounds
// check; the flow guarantees the index. Guarding here turns a corrupt step into
// a dropped frame instead of a wild jump.
void mp_dispatch_step(int index)
{
    if (index < 0 || index >= 12) return;
    s_mpStep[index]();
}

// ---------------------------------------------------------------------------
// Shared idiom: `(-((x & 1) == 0) & MASK) + BASE`.
//
// This is the compiler's branchless "pick one of two constants" - SBB CL,CL
// after CMP EAX,1 leaves 0 or -1, the AND selects the mask, and the ADD
// overflows the byte for the -1 case. Spelling it out as a ternary keeps the
// byte truncation visible: 0xFA + 10 is 0x104, which stores as 4, NOT 0x104.
// ---------------------------------------------------------------------------
inline unsigned char mp_pick(unsigned int odd, unsigned int mask, unsigned int base)
{
    return (unsigned char)((odd ? 0u : mask) + base);
}

// ---------------------------------------------------------------------------
// monster_plant_idle_fidget (0x0045c580)
// Re-rolls the idle sway animation whenever its countdown expires.
// ---------------------------------------------------------------------------
void mp_idle_fidget(void)
{
    if (MP_FIDGET != 0) {
        MP_FIDGET = (short)(MP_FIDGET - 1);
        return;
    }

    // (-((rand() & 1) == 0) & 0xFB) + 8  ->  8 when odd, 3 when even.
    ENTITY->animationId    = mp_pick((unsigned int)rand() & 1, 0xFB, 8);
    ENTITY->timing_control = 0;
    ENTITY->blend_counter  = 0x1F;
    MP_FIDGET = (short)((unsigned short)rand() & 0x1F);

    // Only the "3" pose rewinds the frame cursor, and only past frame 0x13.
    if ((char)ENTITY->animationId == 3) {
        unsigned char frame = ENTITY->animation_frame_id;
        if (frame > 0x13) {
            ENTITY->animation_frame_id = (unsigned char)(frame - 10);
        }
    }
}

// ---------------------------------------------------------------------------
// monster_plant_init (0x0045ac80) - state 0
// ---------------------------------------------------------------------------
void mp_init(void)
{
    // One dword store: state = 1, behaviour = 0, step = 0, substep = 0.
    MP_STATE32 = 1;
    ENTITY->health = 1;

    ENTITY->Sca_info = (unsigned int)(void*)s_mpScaInfo;

    // Two separate stores in the original, not one masked assignment.
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 10);

    ENTITY->animationId        = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0;

    ResetJointTransforms();

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    // 0x00be0dfc is the shadow tint the quad builder copies into its header -
    // an IMMEDIATE 0x404040, not the address of anything. See the Zombie.cpp
    // note on this exact trap.
    g_animFrameIdSave = 0x00404040;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 500, 100);

    MP_SHADOW  = 1;
    MP_HOLDOFF = 0;
    MP_FIDGET  = 0;
    MP_ALERTED = 1;
    MP_HITS    = 0;

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);

    s_mpLastSides = 0;

    // behavior_flags bit 1 = "spawn already sprung": the whole vine starts
    // hidden, the plant is immortal (health -1) and casts no shadow.
    if ((ENTITY->behavior_flags & 2) != 0) {
        JointStruct* j = mp_joints();
        for (int i = 0; i < MP_SEGMENTS; i++) {
            j[i].flags = (unsigned char)(j[i].flags & 0xFE);
        }
        ENTITY->health = -1;
        MP_SHADOW      = 0;
    }
}

// ---------------------------------------------------------------------------
// monster_plant_select (0x0045aec0)
// Picks the behaviour for the current behavior_flags variant.
// ---------------------------------------------------------------------------
void mp_select_variant(void)
{
    if ((ENTITY->behavior_flags & 0x80) == 0) {
        unsigned int idx = (unsigned int)(ENTITY->behavior_flags & 0xF);
        if (idx < 10) {
            s_mpVariant[idx]();
        }
    } else {
        mp_variant_0();
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 8);
    }
    MP_STEP16 = 0;   // clears 0x86 and 0x87 together
}

// ---------------------------------------------------------------------------
// monster_plant_state_check (0x0045ade0) - state 1
// ---------------------------------------------------------------------------
void mp_state_check(void)
{
    unsigned char flags = ENTITY->behavior_flags;

    // bit 6 = the room asked for the death sequence.
    if ((flags & 0x40) != 0) {
        ENTITY->state       = 3;
        ENTITY->ignore_player_flag = 0;
        return;
    }

    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);

    if (flags == 0 || flags == 1) {
        entity_check_visual_range(5000);
    }
    if (flags == 3 || flags == 4) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 0xC0);
        entity_check_visual_range(5000);
    }

    // Manhattan distance, both axes folded through the branchless abs()
    // idiom `(v ^ (v >> 31)) - (v >> 31)`.
    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
           - ENTITY->scaMatrixData.localMatrix.t[0];
    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
           - ENTITY->scaMatrixData.localMatrix.t[2];
    int sx = dx >> 31;
    int sz = dz >> 31;
    MP_DIST = ((dz ^ sz) - sz) + ((dx ^ sx) - sx);

    if (MP_BEHAVIOR == 0) {
        MP_STEP16 = 0;
        mp_select_variant();
    }

    unsigned int behavior = MP_BEHAVIOR;
    if (behavior < 7) {
        s_mpBehavior[behavior]();
    }

    // One dword snapshot of state/behaviour/step/substep, restored by state 2.
    MP_STATE_BK = MP_STATE32;
}

// ---------------------------------------------------------------------------
// monster_plant_damaged (0x0045c110) - state 2
// The plant has no flinch of its own: it counts the hit, then re-enters the
// behaviour it was running via the 0x184 snapshot.
// ---------------------------------------------------------------------------
void mp_damaged(void)
{
    MP_HITS = (short)(MP_HITS + 1);
    if ((unsigned short)MP_HITS > 3) {
        Flg_on((int)(void*)g_ScenarioFlags, SCENARIO_FLAG_MONSTER_PLANT_PROG);
    }

    MP_STATE32 = MP_STATE_BK;
    ENTITY->hit_state = 0;
    mp_state_check();
}

// ---------------------------------------------------------------------------
// monster_plant_die (0x0045c160) - state 3
// Bounds-checked 3-case jumptable at 0x004c10a0; case 0 ends with an explicit
// CALL into case 1 (0x0045c1da), it does not fall through.
// ---------------------------------------------------------------------------
void mp_die_case_1(void);

void mp_die_case_0(void)
{
    ENTITY->blend_counter      = 0x1F;
    ENTITY->ignore_player_flag = 1;
    MP_FIDGET = 0;

    // Two rolls summed: 1..63 frames of thrashing.
    int r1 = rand();
    int r2 = rand();
    MP_TICKS = (unsigned short)(((unsigned short)r1 & 0x1F)
                              + ((unsigned short)r2 & 0x1F) + 1);

    ENTITY->health = -1;
    mp_die_case_1();
}

void mp_die_case_1(void)
{
    if (MP_TICKS != 0) {
        MP_TICKS = (unsigned short)(MP_TICKS - 1);
        if (MP_TICKS != 0) {
            mp_behavior_6();       // keep thrashing (and keep the poison tint)
            return;
        }
        ENTITY->animationId        = 0x0E;   // collapse pose
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->blend_counter      = 0x1F;
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
        ENTITY->ignore_player_flag = (unsigned char)(ENTITY->ignore_player_flag + 1);

        // FOUR rand() calls, and the FIRST result is discarded - EAX is
        // overwritten by the second CALL at 0x0045c285 before it is used.
        // Dropping the dead call would desynchronise every later roll.
        rand();
        ENTITY->animationId        = mp_pick((unsigned int)rand() & 1, 7, 0);
        ENTITY->animation_frame_id = (unsigned char)(rand() & 0xF);
        ENTITY->timing_control     = 0;
        ENTITY->blend_counter      = 0x1F;
        MP_TICKS  = (unsigned short)(((unsigned short)rand() & 0xF) + 0x32);
        MP_TIMER_A = 4;
        MP_TIMER_B = 8;
    }
}

void mp_die_case_2(void)
{
    // SIGNED compare - `MOVSX EDX, word ptr [ECX+0xC4]` at 0x0045c31e. The
    // twitch rate tapers off as the counter runs down.
    if ((int)(rand() & 0x3F) < (int)MP_TICKS) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    }

    MP_TICKS = (unsigned short)(MP_TICKS - 1);
    if (MP_TICKS == 0) {
        ENTITY->ignore_player_flag = (unsigned char)(ENTITY->ignore_player_flag + 1);
        Flg_on((int)(void*)g_EnemiesFlags, ENTITY->death_event_id);
    }

    // behavior_flags bit 0 = the withering colour ramp.
    if ((ENTITY->behavior_flags & 1) != 0) {
        MP_TIMER_A = (short)(MP_TIMER_A - 1);
        if (MP_TIMER_A == 0) {
            scd_model_tint_apply(1, 0, 0, 0, 0x100, ENTITY->id);
            MP_TIMER_A = 4;
        }
        MP_TIMER_B = (short)(MP_TIMER_B - 1);
        if (MP_TIMER_B == 0) {
            scd_model_tint_apply(0, 1, 0, 0, 0x100, ENTITY->id);
            MP_TIMER_B = 8;
        }
    }
}

void mp_die(void)
{
    unsigned char sub = ENTITY->ignore_player_flag;
    if (sub > 2) return;              // 0x0045c16b - explicit bounds check
    switch (sub) {
    case 0: mp_die_case_0(); return;
    case 1: mp_die_case_1(); return;
    case 2: mp_die_case_2(); return;
    }
}

// 0x0045c440 - state 4 is a bare RET in the original.
void mp_noop(void) { }

// ---------------------------------------------------------------------------
// Variant handlers - one per behavior_flags & 0xF, run once when the behaviour
// index is 0. Each one installs the behaviour it wants next.
// ---------------------------------------------------------------------------

// 0x0045af00 - variants 0, 1, and the 0x80 "scripted" variant: plain idle.
void mp_variant_0(void)
{
    ENTITY->ignore_player_flag = 1;
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0xF7);
    MP_ALERTED = 1;
}

// 0x0045af30 - variant 2: retract out of sight and go dormant.
void mp_variant_2(void)
{
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 8);
    ENTITY->ignore_player_flag = 1;
    // 0x10 - ((id & 1) == 0)  ->  0x10 when odd, 0x0F when even.
    ENTITY->animationId        = (unsigned char)(0x10 - (((g_playerEntity.id & 1) == 0) ? 1 : 0));
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0;
    MP_SHADOW = 0;
    MP_SWERVE = 0;

    JointStruct* j = mp_joints();
    for (int i = 0; i < MP_SEGMENTS; i++) {
        j[i].flags = (unsigned char)(j[i].flags & 0xFE);
    }
}

// 0x0045afc0 - variant 3: wind up for the grab.
void mp_variant_3(void)
{
    ENTITY->ignore_player_flag = 2;
    ENTITY->animationId        = 1;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0x1F;
}

// 0x0045b000 - variant 4: the lunge. Two entry poses depending on which way
// the plant is already facing.
void mp_variant_4(void)
{
    ENTITY->ignore_player_flag = 3;
    // SIGNED 16-bit compares - `CMP AX,0x7FF / JLE` then `CMP AX,0x1000 / JGE`
    // at 0x0045b015. Pose 2 is only for a yaw already inside the second half
    // turn; everything else (including a negative yaw) uses pose 9.
    if (ENTITY->angle <= 0x7FF || ENTITY->angle >= 0x1000) {
        ENTITY->animationId = 9;
    } else {
        ENTITY->animationId = 2;
    }
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 7;
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
    Snd_em(0);
}

// 0x0045b080 - variant 5: strike from hiding. Hides the whole vine, parks the
// cursor at the tip and snaps to face the player before the reveal starts.
void mp_variant_5(void)
{
    ENTITY->ignore_player_flag = 4;
    ENTITY->animationId        = (unsigned char)(0x10 - (((g_playerEntity.id & 1) == 0) ? 1 : 0));
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0;

    JointStruct* j = mp_joints();
    for (int i = 0; i < MP_SEGMENTS; i++) {
        j[i].flags = (unsigned char)(j[i].flags & 0xFE);
    }

    MP_SEG      = 14;
    MP_TICKS    = 0;
    MP_TIMER_A  = 2;
    MP_ALERTED  = 1;
    MP_ANGLE_BK = ENTITY->angle;
    ENTITY->angle = (short)getAngleTowardsTarget(
        g_playerEntity.scaMatrixData.localMatrix.t[0],
        g_playerEntity.scaMatrixData.localMatrix.t[2]);

    mp_step_01();     // grab the player immediately
    Snd_em(2);
}

// 0x0045b160 - variants 6 and 7: idle thrash. Variant 7 also arms the poison
// colour pulse through the texture queue.
void mp_variant_6(void)
{
    ENTITY->ignore_player_flag = 5;
    ENTITY->health             = -1;
    ENTITY->animationId        = mp_pick((unsigned int)rand() & 1, 7, 0);
    ENTITY->animation_frame_id = (unsigned char)(rand() & 0xF);
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0;
    MP_TICKS = 1;

    if ((char)ENTITY->behavior_flags == 7) {
        // p5 = 0x100 is a 16-BIT queue word. FUN_00473d10's old byte-wide
        // declaration truncated it to 0 and the pulse never armed.
        FUN_00473d10(2, -3, 0, 0, 0x100, (char)ENTITY->id);
    }
}

// 0x0045b200 - variants 8 and 9 (behavior_flags 0x29 lands here via & 0xF):
// the poison plant. Alternates lunge sides through a shared shift register.
void mp_variant_8(void)
{
    ENTITY->ignore_player_flag = 6;

    unsigned char side = (unsigned char)(rand() & 1);
    // If the last two recorded sides already match this one, flip it - stops a
    // third identical lunge in a row.
    if ((unsigned int)side * 3 == (s_mpLastSides & 3)) {
        side = (unsigned char)(side ^ 1);
    }
    s_mpLastSides = (unsigned int)side | (s_mpLastSides * 2);

    // (-(side == 0) & 2) + 0x11  ->  0x11 when side is 1, 0x13 when 0.
    ENTITY->animationId = (unsigned char)((side ? 0u : 2u) + 0x11);
    ENTITY->animation_frame_id = (unsigned char)(((unsigned char)rand() & 7) * 3);
    if ((char)ENTITY->animationId == 0x11) {
        ENTITY->animation_frame_id =
            (unsigned char)(ENTITY->animation_frame_id + ((unsigned char)rand() & 7) * 3);
    }
    ENTITY->timing_control = 0;
    ENTITY->blend_counter  = 0x1F;
    MP_FIDGET = 0;
    MP_TICKS  = 1;

    if ((char)ENTITY->behavior_flags == 0x29) {
        MP_TIMER_A  = 0x28;
        MP_TIMER_B  = 0x28;
        MP_ANGLE_BK = 2;      // red tint steps owed
        MP_SWERVE   = 3;      // green tint steps owed
        Snd_em(4);
    }
}

// ---------------------------------------------------------------------------
// Behaviour 1's per-variant tick (0x004c1040)
// ---------------------------------------------------------------------------

// 0x0045b430 - variant 0: wake into the wind-up once the player is close.
void mp_sub_0(void)
{
    if (MP_DIST < 0xA8D) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags     = 3;
    }
}

// 0x0045b460 - variant 1: needs a facing check too, and uses a wider trigger
// radius the first time (MP_ALERTED still 1).
void mp_sub_1(void)
{
    if (MP_HOLDOFF != 0) {
        MP_HOLDOFF = (short)(MP_HOLDOFF - 1);
        return;
    }

    // 3000 - (MP_ALERTED == 0 ? 0x1F4 : 0)  ->  3000 while never engaged,
    // 2500 afterwards.
    int trigger = 3000 + ((MP_ALERTED == 0) ? (int)0xFFFFFE0C : 0);
    if (MP_DIST <= trigger && g_playerEntity.isBeingAttackedFlag == 0) {
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x400) == 0) {
            ENTITY->ignore_player_flag = 0;
            ENTITY->behavior_flags     = 4;
        }
    }
}

// 0x0045b4e0 - variant 2: dormant. Picks a lunge side, then rotates a virtual
// yaw to test whether the player is reachable from that side before committing.
void mp_sub_2(void)
{
    if ((char)ENTITY->behavior_flags != 2) {
        ENTITY->ignore_player_flag = 0;
        return;
    }

    if (MP_DIST > 4000) {
        MP_SWERVE  = 0;
        MP_HOLDOFF = 0;
        return;
    }

    if (MP_DIST >= 0xC45) return;

    if (MP_SWERVE == 0) {
        // 2 = dead ahead, 1 = needs a swerve.
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200) == 0) {
            MP_SWERVE = 2;
        }
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x600) != 0) {
            MP_SWERVE = 1;
        }
        return;
    }

    if (g_playerEntity.isBeingAttackedFlag != 0) return;

    // Temporarily swing the yaw by (side - 1) * 0x800 so turn_toward_target
    // answers for the swerved approach, then put it straight back.
    MP_ANGLE_BK = ENTITY->angle;
    ENTITY->angle = (short)(ENTITY->angle + (short)((MP_SWERVE - 1) * 0x800));
    if ((short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200) == 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags     = 5;
    }
    ENTITY->angle = MP_ANGLE_BK;
}

// ---------------------------------------------------------------------------
// Behaviours
// ---------------------------------------------------------------------------

// 0x0045c450 - unreachable bare RET (see the s_mpBehavior comment).
void mp_behavior_0(void) { }

// 0x0045b320 - behaviour 1: idle. Sways on a timer, and on the frames it is
// not re-rolling the sway it runs the per-variant approach test.
void mp_behavior_1(void)
{
    if ((ENTITY->behavior_flags & 0x20) != 0) {
        ENTITY->ignore_player_flag = 0;
        return;
    }

    short fidget = MP_FIDGET;
    if (fidget == 0) {
        ENTITY->animationId    = mp_pick((unsigned int)rand() & 1, 7, 0);
        ENTITY->timing_control = 0;
        ENTITY->blend_counter  = 0x1F;
        MP_FIDGET = (short)((unsigned short)rand() & 0x1F);
        return;
    }
    MP_FIDGET = (short)(fidget - 1);

    unsigned char flags = ENTITY->behavior_flags;
    if (flags != 0x80) {
        unsigned int idx = (unsigned int)(flags & 0xF);
        if (idx < 4 && s_mpSub[idx] != nullptr) {
            s_mpSub[idx]();
        }
    }

    if (ENTITY->behavior_flags != 2) {
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
            ENTITY->ignore_player_flag = 0;
        }
    }

    // The player's whole 0x84 state dword: animationId 1, animFrameId 3,
    // action_behavior 0x14, action_state 0 - the "being held" pose. While the
    // player is in it the plant runs its animation a second time so the two
    // stay in step.
    if (*(unsigned int*)&g_playerEntity.animationId == 0x140301) {
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
            ENTITY->ignore_player_flag = 0;
        }
    }
}

// 0x0045b610 - behaviour 2: the grab sequence. Step base 0.
void mp_behavior_2(void)
{
    if ((ENTITY->behavior_flags & 0x20) != 0) {
        ENTITY->ignore_player_flag = 0;
        return;
    }
    MP_ANIM_END = (short)(unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    mp_dispatch_step(MP_STEP);
}

// 0x0045bb00 - behaviour 3: the bite/hold sequence. Step base 6, and NO
// Joint_move prologue - step 6 runs its own at blend 0x200 and the later steps
// inherit whatever MP_ANIM_END it left.
void mp_behavior_3(void)
{
    if ((ENTITY->behavior_flags & 0x20) != 0) {
        ENTITY->ignore_player_flag = 0;
        return;
    }
    mp_dispatch_step(MP_STEP + 6);
}

// 0x0045bcb0 - behaviour 4: the vine strike. Step base 8, no 0x20 check.
void mp_behavior_4(void)
{
    MP_ANIM_END = (short)(unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    mp_dispatch_step(MP_STEP + 8);
}

// 0x0045bfa0 - behaviour 5: winding down after a release. Immortal while the
// counter drains.
void mp_behavior_5(void)
{
    ENTITY->health = -1;
    if (MP_TICKS != 0) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
        MP_TICKS = (unsigned short)(MP_TICKS - 1);
    }
}

// 0x0045bff0 - behaviour 6: the poison plant's idle. Re-rolls a thrash pose on
// roughly half the animation wraps, and bleeds the red/green tint out on two
// independent 40-frame timers.
void mp_behavior_6(void)
{
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
        // `AND AL,0x80; CMP AL,0x7D; JBE` - an unsigned byte test that is true
        // only when bit 7 survives, i.e. a straight 50%.
        if (((unsigned char)rand() & 0x80) != 0) {
            ENTITY->animationId   = (unsigned char)((((unsigned int)rand() & 1) ? 0u : 2u) + 0x11);
            ENTITY->blend_counter = 0x1F;
        }
    }

    if ((char)ENTITY->behavior_flags == 0x29) {
        MP_TIMER_A = (short)(MP_TIMER_A - 1);
        if (MP_TIMER_A == 0) {
            if (MP_ANGLE_BK != 0) {
                scd_model_tint_apply(1, 0, 0, 0, 0x100, ENTITY->id);
                MP_ANGLE_BK = (short)(MP_ANGLE_BK - 1);
            }
            MP_TIMER_A = 0x28;
        }
        MP_TIMER_B = (short)(MP_TIMER_B - 1);
        if (MP_TIMER_B == 0) {
            if (MP_SWERVE != 0) {
                scd_model_tint_apply(0, -1, 0, 0, 0x100, ENTITY->id);
                MP_SWERVE = (short)(MP_SWERVE - 1);
            }
            MP_TIMER_B = 0x28;
        }
    }
}

// ---------------------------------------------------------------------------
// The 12 shared step handlers (0x004c1050)
// ---------------------------------------------------------------------------

// Steps 2 and 8 both close a grab with the same three lines.
void mp_grab_sound(void)
{
    Snd_em(3);
    if ((g_playerEntity.id & 3) != 3) {
        Play3DSnd(2, (int)(g_playerEntity.id & 1) + 0x17, 0,
                  (int)(void*)g_playerEntity.scaMatrixData.localMatrix.t);
    } else {
        Play3DSnd(3, 0, 0, (int)(void*)g_playerEntity.scaMatrixData.localMatrix.t);
    }
}

// 0x0045b670 - step 0: idle in reach, waiting for the player to come close
// enough to grab. Also the step the plant returns to after a failed grab.
void mp_step_00(void)
{
    if (MP_ANIM_END != 0 && ENTITY->animationId == 1) {
        ENTITY->animationId    = 8;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter  = 0x0F;
    }

    if (ENTITY->animationId != 1) {
        mp_idle_fidget();
        if (g_playerEntity.action_behavior == 0x14) {
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
        }
    }

    if (MP_DIST > 0x2134) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags     = 0;
    }

    if (MP_HOLDOFF != 0) {
        MP_HOLDOFF = (short)(MP_HOLDOFF - 1);
        return;
    }

    if (MP_DIST < 0x708 && g_playerEntity.isBeingAttackedFlag == 0) {
        MP_STEP = (unsigned char)(MP_STEP + 1);
        // (-((id & 1) == 0) & 0xFA) + 10  ->  10 when odd, 4 when even
        // (0xFA + 10 = 0x104, truncated to a byte).
        ENTITY->animationId        = mp_pick((unsigned int)g_playerEntity.id & 1, 0xFA, 10);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->blend_counter      = 0;

        // Not facing the player yet - divert to the recovery step instead.
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200) != 0) {
            MP_STEP    = 4;
            MP_TIMER_A = 2;
        }
    }
}

// 0x0045b7c0 - step 1: the grab connects. Hands the player over to animation
// 6 / frame 15, which player state 6 dispatches as
// g_playerAnimFunctions[15 + 0x13] = slot 34 = the DAT_004c10b0 handlers below.
void mp_step_01(void)
{
    MP_STEP = (unsigned char)(MP_STEP + 1);
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);

    g_playerEntity.attackAnim     = (unsigned char)((char)is_facing_toward_entity(&g_playerEntity) + 2);
    g_playerEntity.directionAngle = ENTITY->angle;
    snap_player_to_grab_position(&g_playerEntity);

    g_playerEntity.action_behavior      = 0;
    g_playerEntity.action_state         = 0;
    g_playerEntity.isBeingAttackedFlag  = 1;
    g_playerEntity.animationId          = 6;
    g_playerEntity.animFrameId          = 0x0F;
    g_playerEntity.flags = (unsigned char)(g_playerEntity.flags | 2);

    Snd_em(2);
}

// 0x0045b840 - step 2: reel the player in, then start the drain.
void mp_step_02(void)
{
    MP_ANGLE_BK = ENTITY->angle;
    entity_rotate_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x80);

    if (MP_ANIM_END != 0) {
        MP_STEP = (unsigned char)(MP_STEP + 1);
        ENTITY->animationId    = (unsigned char)(ENTITY->animationId + 1);
        ENTITY->timing_control = 0;
        ENTITY->blend_counter  = 0;
        MP_TICKS   = 0x3C;
        MP_TIMER_A = 0x0C;
        mp_grab_sound();
    }
}

// 0x0045b910 - steps 3 and 9: the drain. 2 HP every 12 frames, and mashing the
// pad burns the 60-frame hold down 4x faster.
void mp_step_03(void)
{
    MP_TIMER_A = (short)(MP_TIMER_A - 1);
    if (MP_TIMER_A == 0) {
        g_playerEntity.health = (short)(g_playerEntity.health - 2);
        // Only variant 5 refuses to kill outright.
        if (ENTITY->behavior_flags == 5 && g_playerEntity.health < 0) {
            g_playerEntity.health = 0;
        }
        MP_TIMER_A = 0x0C;
    }

    short hold = (short)MP_TICKS;
    if (hold > 0 && g_playerEntity.health >= 0) {
        MP_TICKS = (unsigned short)(hold - 1 - ((GetPlayerInputMasked() != 0) ? 3 : 0));
        return;
    }

    if (MP_ANIM_END != 0) {
        MP_STEP = (unsigned char)(MP_STEP + 1);
        ENTITY->animationId    = (unsigned char)(ENTITY->animationId + 1);
        ENTITY->timing_control = 0;
        // action_state 2 -> DAT_004c10b0[2], the player's release handler.
        g_playerEntity.action_state = 2;
    }
}

// 0x0045b9e0 - step 4: recover from a grab that never landed.
void mp_step_04(void)
{
    if (MP_ANIM_END != 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags     = 0;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0xFD);
        MP_HOLDOFF    = 0x1E;
        ENTITY->angle = MP_ANGLE_BK;
    }
}

// 0x0045ba30 - step 5: the post-release flail. Either re-arms for another grab
// if the player is still in reach, or gives up and goes back to idle.
void mp_step_05(void)
{
    if (MP_ANIM_END == 0) return;

    MP_TIMER_A = (short)(MP_TIMER_A - 1);
    if (MP_TIMER_A == 0) {
        if (MP_DIST < 0xA8D) {
            MP_STEP                    = 0;
            ENTITY->animationId        = 8;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control     = 0;
            ENTITY->blend_counter      = 0x0F;
        } else {
            ENTITY->ignore_player_flag = 0;
            ENTITY->behavior_flags     = 3;
        }
        MP_HOLDOFF = 0x1E;
        return;
    }

    ENTITY->animationId        = (unsigned char)(ENTITY->animationId + 2);
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0x0F;
    Snd_em(0);
}

// 0x0045bb30 - step 6 (behaviour 3's step 0): the bite. Runs its own animation
// step at blend 0x200, and lands the hit on animation frame 6.
void mp_step_06(void)
{
    MP_ANIM_END = (short)(unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x200);
    if (MP_ANIM_END != 0) {
        MP_STEP = (unsigned char)(MP_STEP + 1);
        MP_FIDGET  = 0;
        MP_ALERTED = 0;
    }

    if (ENTITY->animation_frame_id == 6 && g_playerEntity.isBeingAttackedFlag == 0) {
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x400) == 0) {
            char front = 0;
            g_playerEntity.action_state = 0;
            // Hit from the front unless the player is facing across the plant.
            if (g_playerEntity.directionAngle <= 0x400
                || g_playerEntity.directionAngle >= 0xC00) {
                front = 1;
            }
            g_playerEntity.isBeingAttackedFlag = (unsigned char)(front + 1);
            g_playerEntity.action_behavior     = (unsigned char)(front + 0x66);
            g_playerEntity.health = (short)(g_playerEntity.health - 2);
            if (g_playerEntity.health < 0) {
                g_playerEntity.health = 0;
            }
            Snd_em(1);
            MP_HOLDOFF = 1;
        }
    }
}

// 0x0045bc30 - step 7 (behaviour 3's step 1): back to sway, still watching.
void mp_step_07(void)
{
    mp_idle_fidget();
    mp_sub_1();

    if (MP_DIST > 0x2133) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags     = 1;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    if (g_playerEntity.action_behavior == 0x14) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    }
}

// 0x0045bd00 - step 8, shared by behaviour 2 (step 8), behaviour 3 (step 2)
// and behaviour 4 (step 0): the vine EXTENDS. Each pass reveals segments from
// the current cursor down to s_mpExtendStops[MP_TICKS]; when the animation
// wraps, the grab closes.
void mp_step_08(void)
{
    bool revealed = false;

    if (MP_TIMER_A == 0 && MP_TICKS < 4) {
        short seg = MP_SEG;
        unsigned char* p = (unsigned char*)mp_joints() + (int)seg * 0x7C;
        short stop = s_mpExtendStops[MP_TICKS];
        while (stop <= seg) {
            seg--;
            *p = (unsigned char)(*p | 1);
            p -= 0x7C;
        }
        MP_SEG   = seg;
        MP_TICKS = (unsigned short)(MP_TICKS + 1);
        revealed = true;
    }

    // The decrement is skipped ONLY on a frame that revealed segments. When
    // MP_TIMER_A is already 0 and the cursor is spent, this wraps to 0xFFFF -
    // that is the original's behaviour, and the step is left before it matters.
    if (!revealed) {
        MP_TIMER_A = (short)(MP_TIMER_A - 1);
    }

    if (MP_ANIM_END != 0) {
        MP_STEP = (unsigned char)(MP_STEP + 1);
        // (-((id & 1) == 0) & 0xFA) + 0x0B  ->  0x0B when odd, 0x05 when even.
        ENTITY->animationId        = mp_pick((unsigned int)g_playerEntity.id & 1, 0xFA, 0x0B);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->blend_counter      = 0;
        g_playerEntity.flags = (unsigned char)(g_playerEntity.flags | 8);
        MP_TICKS   = 0x3C;
        MP_TIMER_A = 0x0C;
        mp_grab_sound();
    }
}

// 0x0045be60 - step 10: arm the retraction.
void mp_step_10(void)
{
    MP_STEP = (unsigned char)(MP_STEP + 1);
    ENTITY->animationId        = 0x12;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0;
    MP_SEG   = 0;
    MP_TICKS = 0;
}

// 0x0045bec0 - step 11: the vine RETRACTS. Hides segments from the base
// upward, gated on the animation having passed frame 2.
void mp_step_11(void)
{
    if (ENTITY->animation_frame_id > 2 && MP_SEG < 0x0F) {
        short seg = MP_SEG;
        unsigned char* p = (unsigned char*)mp_joints() + (int)seg * 0x7C;
        short stop = s_mpRetractStops[MP_TICKS];
        while (seg <= stop) {
            seg++;
            *p = (unsigned char)(*p & 0xFE);
            p += 0x7C;
        }
        MP_SEG   = seg;
        MP_TICKS = (unsigned short)(MP_TICKS + 1);
    }

    if (MP_ANIM_END != 0) {
        ENTITY->ignore_player_flag = 0;
        // 0x80 is the scripted variant; it keeps its flags.
        if (ENTITY->behavior_flags != 0x80) {
            ENTITY->behavior_flags = 2;
        }
        ENTITY->angle = MP_ANGLE_BK;
        MP_SWERVE  = 0;
        MP_HOLDOFF = 0;
    }
}

// ---------------------------------------------------------------------------
// Player-side "held by the vine" animation (0x004c10b0)
// Reached as g_playerAnimFunctions[34] -> 0x0045c460, which dispatches on the
// player's action_state. This table was an all-NULL placeholder in Globals.cpp,
// so a grabbed player froze with no animation and never got released - the same
// defect the Tyrant's DAT_004ba360 had. It lives here, with its handlers, for
// the same reason that one lives in Tyrant.cpp.
// ---------------------------------------------------------------------------

// 0x0045c470 - state 0: entry frame. Advances to state 1 and resets the frame
// cursor.
void player_plant_hold_00(void)
{
    g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + 1);
    g_playerEntity.animation_frame_id = 0;
    g_playerEntity.unk_bf             = 0;
    g_playerEntity.unk_8c             = 0;
    entity_apply_anim_vertex((Entity*)&g_playerEntity,
                             g_playerEntity.emdScratchPtr1,
                             g_playerEntity.emdScratchPtr2);
}

// 0x0045c4b0 - state 1: the struggle loop. Frames past 0x18 wrap back to 10 so
// the hold cycles instead of running off the end of the animation.
void player_plant_hold_01(void)
{
    entity_apply_anim_vertex((Entity*)&g_playerEntity,
                             g_playerEntity.emdScratchPtr1,
                             g_playerEntity.emdScratchPtr2);
    Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x80);
    if (g_playerEntity.animation_frame_id > 0x18) {
        g_playerEntity.animation_frame_id = 0x0A;
    }
}

// 0x0045c500 - state 2: the release. Hands control back on frame 0x27, or
// immediately if the player died in the grip.
void player_plant_hold_02(void)
{
    entity_apply_anim_vertex((Entity*)&g_playerEntity,
                             g_playerEntity.emdScratchPtr1,
                             g_playerEntity.emdScratchPtr2);

    if (g_playerEntity.animation_frame_id == 0x27 || g_playerEntity.health < 0) {
        // One word store each: animationId/animFrameId, then
        // action_behavior/action_state.
        g_playerEntity.animationId     = 1;
        g_playerEntity.animFrameId     = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state    = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        // attackAnim 2 = grabbed from behind, so the player is spun round.
        if (g_playerEntity.attackAnim == 2) {
            g_playerEntity.directionAngle = (short)(g_playerEntity.directionAngle + 0x800);
        }
        g_playerEntity.flags = (unsigned char)(g_playerEntity.flags & 0xF5);
    }

    Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x80);
}

} // namespace

// 0x004c10b0 - three slots; the original's fourth entry is NULL.
void* DAT_004c10b0[4] = {
    /* 0  0x0045c470 */ (void*)player_plant_hold_00,
    /* 1  0x0045c4b0 */ (void*)player_plant_hold_01,
    /* 2  0x0045c500 */ (void*)player_plant_hold_02,
    /* 3  -          */ nullptr
};

// ============================================================================
// monster_plant_update (0x0045abb0) - per-frame entry, enemies table slot 15.
// ============================================================================
void monster_plant_update(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    if ((g_message_flags & 4) != 0) {
        unsigned int state = ENTITY->state;
        if (state < 5) {
            s_mpState[state]();
        }

        // Retarget the SCA hit vector at Entity+0x08 onto the vine head, as an
        // offset from the body. Without this the hitbox stays at the base and
        // an extended vine cannot be shot.
        short* hit = (short*)ENTITY->pSca_hit_data;
        int*   head = joints[11].world.t;
        hit[0] = (short)((short)head[0] - *(short*)((char*)ENTITY + 0x34));
        hit[1] = (short)((short)head[1] - *(short*)((char*)ENTITY + 0x38));
        hit[2] = (short)((short)head[2] - *(short*)((char*)ENTITY + 0x3C));
    }

    ENTITY->scaMatrixData.field_00 = 0;
    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0], g_CurrentRdtDataTypePtr);

    if (ENTITY->has_enter_switch_zone != 0 && (MP_SHADOW & 1) != 0) {
        entity_add_fade_sprite((VECTOR*)joints[11].world.t,
                               (short*)&ENTITY->pushVelocity,
                               0, ENTITY->angle);
    }
}
