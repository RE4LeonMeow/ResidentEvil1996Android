// BlackTiger.cpp - Black Tiger (entity type 4, enemy/em1004.emd) - the giant
// spider boss.
//
// Original PC addresses:
//   black_tiger_update            0x0044f300   per-frame entry, dispatch table [4]
//   bt_state0 (init)              0x0044f1b0
//   bt_state1 (run)               0x0044f3e0
//   bt_state2 (web-build)         0x0044f470
//   bt_state3 (web-shoot)         0x0044f490
//   bt_picker                     0x0044f690   behaviour picker (0x85 == 0)
//   bt_action_runner              0x0044f560   action dispatcher (0x85 == 1)
//   bt_behaviour_idle             0x0044f800   behaviour 0
//   bt_behaviour_walk             0x0044f880   behaviour 1
//   bt_behaviour_strafe           0x0044fb60   behaviour 2
//   bt_behaviour_approach         0x0044fa00   behaviour 3
//   bt_behaviour_circle           0x0044fcd0   behaviour 10
//   bt_behaviour_bite             0x0044fe60   behaviour 12 (0xc)
//   bt_behaviour_spit             0x00450180   behaviour 13 (0xd)
//   bt_web_build                  0x004502e0   state 2 body (web threads)
//   bt_web_shoot_a                0x00450550   state 3 default shooter (0x33 count)
//   bt_web_shoot_b                0x00450810   state 3 behaviours 6/8/9
//   bt_web_shoot_c                0x00450c90   state 3 behaviours 1/2
//   bt_leg_reach                  0x004512e0   leg-reach probe (CompMatrix variant)
//
// The id is 4 and the model is em1004.emd: g_emdPathTable index (id + 4) = 8.
//
// ---------------------------------------------------------------------------
// Shape of the AI
// ---------------------------------------------------------------------------
//   Entity+0x84 state           init / run / web-build / web-shoot
//   Entity+0x85 ignore_player_flag 0 = the behaviour picker runs this frame,
//                                  1 = the chosen behaviour owns the spider,
//                                  anything else = frozen.
//   Entity+0x86 action_behavior the thing the spider is doing.
//                                 0 idle, 1 walk, 2 strafe, 3 approach,
//                                 10 circle, 12 bite, 13 spit;
//                                 6/8/9 and 1/2 are also threaded back into
//                                 the web-shooters by bt_state3.
//   Entity+0x87 action_state    the per-behaviour animation sub-state.
//
// The dispatch model is the same as the Web Spinner's (WebSpinner.cpp): a
// behaviour picker (bt_picker) decides WHICH behaviour to enter while
// ignore_player_flag == 0, and the action runner (bt_action_runner) executes
// it while it is 1. The difference is the black tiger has a single picker
// rather than the spawn-kind pickers A/B/C, and it uses m~[0,12] behaviours
// where the web spinner uses 0-11.
//
// ---------------------------------------------------------------------------
// The web shooters, shared clone helpers
// ---------------------------------------------------------------------------
// states 2 and 3 are entered by the damage system (like the Web Spinner).
// State 2 spins idle web threads (bt_web_build); state 3 maps hit_state into
// one of three web shooters and clones the spider `count` times (each clone is
// one thread of web the spider then flies at the player).
//
// The clone helper (0x0048a630) and the web update (0x0048a730) are the SAME
// functions the Web Spinner uses - the original shares one copy across both
// entity types. They are exported from WebSpinner.cpp (ws_clone_entity /
// ws_update_webs) and declared extern here, exactly like the shared
// neptune_clear_hit_state below.
//
// ---------------------------------------------------------------------------
// SCA collision profiles (one pointer table @ 0x004bf398)
// ---------------------------------------------------------------------------
// Entity+4 is set to one of four 6-short records in one block:
//
//   @0x004bf350  idle    {0x8000, -500, -180, 0, 180, 2500}   single box, rear
//   @0x004bf360  unused  {0x8000,  500, -180, 0, 180, 2000}   single box, front
//   @0x004bf370  walking {0x0000,    0, -180, 1000, 180, 2000},
//                        {0x8000,    0, -180, -1000, 180, 2000}  TWO boxes
//   @0x004bf388  shoot   {0x8000,    0,    0, 0,     0,  100}   no horizontal extent
//
// The walking profile is a two-record list (front and rear box 2000 apart,
// because the spider's body is long) walked until the 0x8000 flags word of the
// second entry. The idle setup (0x0044f289) loads PTR_DAT_004bf398 = idle;
// walking and approaching load PTR_DAT_004bf3a0 = walking; the three web
// shooters load PTR_DAT_004bf3a4 = shoot.
//
// ---------------------------------------------------------------------------
// Field aliases
// ---------------------------------------------------------------------------
// The port's Entity struct names these bytes from the zombie's point of view.
// The spider reuses them at other widths with other meanings, so they are
// reached by offset per the "offset writes, not nearest field" rule:
//
//   +0x16C short  the turn step (attacking_direction / dir_control_flags).
//   +0x16E ushort the pathfind LOS bit (texBank); bit 0 = line of sight clear.
//   +0x172 short  the post-behaviour delay (is_moving / move_max_steps).
//   +0x174 short  the web-splat flag (splatter_flag).
//   +0x176 short  the room-collision result (reaction_timer).
//   +0x178 short  the wall-count / chase timer (subpixel_pos_x low word).
//   +0x17A short  a per-run counter (hit_threshold region).
//   +0x17C short  the web-joint registry index (action_speed / hit_threshold).
//   +0x17E short  the post-attack cooldown (behavior_step).
//
// All original addresses from Ghidra.
// ============================================================================
#include "EntityCommon.h"
#include "../../Globals.h"
#include "../BioCard.h"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Raw field access. Every width here is one the original actually uses.
// ============================================================================
static inline signed char&    eb (void* e, unsigned o) { return *reinterpret_cast<signed char*>((char*)e + o); }
static inline unsigned char&  eub(void* e, unsigned o) { return *reinterpret_cast<unsigned char*>((char*)e + o); }
static inline short&          ew (void* e, unsigned o) { return *reinterpret_cast<short*>((char*)e + o); }
static inline unsigned short& euw(void* e, unsigned o) { return *reinterpret_cast<unsigned short*>((char*)e + o); }
static inline int&            ei (void* e, unsigned o) { return *reinterpret_cast<int*>((char*)e + o); }
static inline unsigned int&   eu (void* e, unsigned o) { return *reinterpret_cast<unsigned int*>((char*)e + o); }

// ============================================================================
// Offset accessors for the black tiger's reinterpreted bytes.
// ============================================================================
#define BT_X          (*(int*)             ((char*)ENTITY + 0x34))
#define BT_Y          (*(int*)             ((char*)ENTITY + 0x38))
#define BT_Z          (*(int*)             ((char*)ENTITY + 0x3C))
#define BT_ANGLE      (*(short*)           ((char*)ENTITY + 0x74))
#define BT_SPEED      (*(short*)           ((char*)ENTITY + 0xC2))
#define BT_DWELL      (*(short*)           ((char*)ENTITY + 0xC4))
#define BT_TILT       (*(short*)           ((char*)ENTITY + 0xCA))
#define BT_TURN       (*(short*)           ((char*)ENTITY + 0x16C))
#define BT_PATHW      (*(unsigned short*)  ((char*)ENTITY + 0x16E))
#define BT_TOUCH      (*(short*)           ((char*)ENTITY + 0x170))
#define BT_DELAY      (*(short*)           ((char*)ENTITY + 0x172))
#define BT_SPLAT      (*(short*)           ((char*)ENTITY + 0x174))
#define BT_TRAIL      (*(short*)           ((char*)ENTITY + 0x176))
#define BT_COUNT      (*(short*)           ((char*)ENTITY + 0x178))
#define BT_CFLAG      (*(short*)           ((char*)ENTITY + 0x17A))
#define BT_WEBIDX     (*(short*)           ((char*)ENTITY + 0x17C))
#define BT_COOLDOWN   (*(short*)           ((char*)ENTITY + 0x17E))
#define BT_ANIM_ID    (*(unsigned char*)   ((char*)ENTITY + 0xBD))
#define BT_ANIM_FRAME (*(unsigned char*)   ((char*)ENTITY + 0xBE))
#define BT_TIMING     (*(unsigned char*)   ((char*)ENTITY + 0xBF))
#define BT_BLEND      (*(unsigned char*)   ((char*)ENTITY + 0x8C))
#define BT_HITSTATE   (*(unsigned char*)   ((char*)ENTITY + 0x8A))
#define BT_DEATH_EV   (*(unsigned char*)   ((char*)ENTITY + 0x163))
#define BT_WAY_X      (*(short*)           ((char*)ENTITY + 0x166))
#define BT_WAY_Z      (*(short*)           ((char*)ENTITY + 0x168))
#define BT_STATE      (*(unsigned char*)   ((char*)ENTITY + 0x84))
#define BT_IGNORE     (*(unsigned char*)   ((char*)ENTITY + 0x85))
#define BT_BEHAVIOR   (*(unsigned char*)   ((char*)ENTITY + 0x86))
#define BT_SUBSTATE   (*(unsigned char*)   ((char*)ENTITY + 0x87))
// The original switches behaviours with WORD stores to 0x86 (behavior +
// action_state), so every pick/completion ALSO resets action_state to 0.
// Byte stores here leak a stale sub into the next behaviour - and sub==2/6 are
// terminal states in idle/walk/strafe/spit, which stranded the boss mid-fight.
#define BT_SET_BEHAVIOR(v) (euw(ENTITY, 0x86) = (unsigned short)(v))

// The entity's own transform and position, as the original addresses them.
#define BT_MATRIX     ((MATRIX*)((char*)ENTITY + 0x20))
#define BT_POS        ((VECTOR*)((char*)ENTITY + 0x34))
#define BT_ROT        ((SVECTOR*)((char*)ENTITY + 0x72))

// The player's position, the argument every reach test takes (0x00be6318).
#define PLAYER_T      ((VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t)

// ============================================================================
// Data tables. All original addresses on the right.
// ============================================================================
const short bt_sca_info_idle[6]   = { (short)0x8000, -500, -180, 0, 180, 2500 };   // 0x004bf350
const short bt_sca_info_front[6]  = { (short)0x8000,  500, -180, 0, 180, 2000 };   // 0x004bf360
const short bt_sca_info_walk[2][6] = {                                              // 0x004bf370
    { 0,                    0, -180,  1000, 180, 2000 },
    { (short)0x8000,        0, -180, -1000, 180, 2000 }
};
const short bt_sca_info_shoot[6]  = { (short)0x8000,    0,    0, 0,   0,  100 };   // 0x004bf388
const short* const bt_sca_table[4] = { bt_sca_info_idle, bt_sca_info_front,
                                       bt_sca_info_walk[0], bt_sca_info_shoot };   // 0x004bf398

// 0x004bf3b8 - per-frame scuttle SFX, indexed by animation_frame_id.
const unsigned char bt_frame_sfx[16] = {
    0x00,0x02,0x00,0x00, 0x00,0x00,0x00,0x01,
    0x00,0x00,0x01,0x00, 0x00,0x00,0x01,0x00
};

// 0x004bf3c8 - the odd leg/joint indices the web can be shot from; 0x004bf3d0
// is the runtime registry of which are already in use.
const unsigned char bt_web_joints[8] = { 0x05,0x07,0x07,0x09,0x0B,0x0B,0x0F,0x11 };
unsigned char bt_web_joints_used[8] = {};

// helper: rebuild the Manhattan distance to the player into g_playerDisplacement.
static inline void bt_dist(void)
{
    unsigned int dz = (unsigned int)(g_playerEntity.scaMatrixData.localMatrix.t[2] - BT_Z);
    unsigned int dx = (unsigned int)(g_playerEntity.scaMatrixData.localMatrix.t[0] - BT_X);
    int sdz = (int)dz >> 31;
    int sdx = (int)dx >> 31;
    g_playerDisplacement = ((int)((dz ^ sdz) - sdz) - sdx) + (int)(dx ^ sdx);
}

// The player's pose the boss reacts to (being knocked down / grabbed).
#define PLAYER_DOWN   (g_playerEntity.action_behavior == 0x14 && g_playerEntity.action_state == 0)

// ============================================================================
// Cross-file dependencies set up from 0x0044f000 block. ResetJointTransforms
// lives in TmdAnimation (EntityModelLoader.cpp); the shared web helpers are
// exported from WebSpinner.cpp; the hit-latch reset is exported from
// Neptune.cpp; Flg_on in CmdFunctions.cpp; is_entity_in_switch_zone in Room.cpp.
// ============================================================================
extern void ResetJointTransforms(void);
extern void Flg_on(int baseAddr, unsigned int bitIndex);           // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90
extern void ws_clone_entity(unsigned char count, int animSlotBytes,
                            unsigned char jointIndex, unsigned int* out);  // 0x0048a630
extern void ws_update_webs(char count);                                     // 0x0048a730
void neptune_clear_hit_state(void);                                         // 0x0043d8a0
extern unsigned int g_entity_bkp;                                           // 0x00be0df4

// 0x00be0de4/0x00be0de8 - shared entity scratch, defined in EntityCommon.cpp.
// The bite writes the two fang-joint reach results through them, exactly as the
// original's `MOV dword ptr [0x00be0de4], ECX` / `[0x00be0de8], ECX` does.
extern int player_distance_z;       // 0x00be0de4
extern int g_scaled_down_dist;      // 0x00be0de8

// ============================================================================
// Forward declarations of the file-scope state/behaviour functions.
// ============================================================================
void bt_state0(void);
void bt_state1(void);
void bt_state2(void);
void bt_state3(void);
void bt_picker(void);                     // 0x0044f690
void bt_action_runner(void);              // 0x0044f560
void bt_behaviour_idle(void);             // 0x0044f800
void bt_behaviour_walk(void);             // 0x0044f880
void bt_behaviour_strafe(void);           // 0x0044fb60
void bt_behaviour_approach(void);         // 0x0044fa00
void bt_behaviour_circle(void);           // 0x0044fcd0
void bt_behaviour_bite(void);             // 0x0044fe60
void bt_behaviour_spit(void);             // 0x00450180
void bt_web_shoot_a(void);                // 0x00450550
void bt_web_shoot_b(void);                // 0x00450810
void bt_web_shoot_c(void);                // 0x00450c90
void bt_web_build(void);                  // 0x004502e0
void bt_leg_reach(unsigned char part, int scale);  // 0x004512e0

// ============================================================================
// bt_state0 @ 0x0044f1b0 - one-shot init. Clears the AI block, builds the
// ground shadow (half-extent 0x9C4), rolls the health and installs the idle SCA
// profile. The state block is a single DWORD write (state 1, ignore 0,
// behaviour 0, action 0).
// ============================================================================
void bt_state0(void)
{
    eu(ENTITY, 0x84) = 1;                 // state->run, ignore=0, behavior=0, action=0
    eu(ENTITY, 0x1c) = 0;                 // scaMatrixData.field_00
    euw(ENTITY, 0xc4) = 0;                // action_ticks_counter
    BT_HITSTATE  = 0;
    BT_ANIM_FRAME = 0;
    BT_TIMING    = 0;
    BT_BLEND     = 0;
    BT_ANIM_ID   = 0;

    ResetJointTransforms();
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    g_svecScratch.z = 0;
    g_svecScratch.y = 0;
    g_svecScratch.x = 0;
    g_animFrameIdSave = 0x404040;         // ground-shadow tint
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 0x9C4, 0x9C4);

    rand();                               // discarded
    ew(ENTITY, 0x88) = 0xCC;              // health = 204

    ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_idle;
    BT_TILT = 0x1B33;

    BT_DELAY  = 0;
    BT_SPLAT  = 0;
    BT_COUNT  = 0;
    BT_CFLAG  = 0;
    BT_WEBIDX = 0;
    BT_COOLDOWN = 0x2D;
}

// ============================================================================
// bt_state1 @ 0x0044f3e0 - the main AI driver. Three layers: the behaviour
// picker while ignore_player_flag==0, the action runner while it is 1, then
// the shared per-frame bookkeeping (visual range, delay countdown, wall count).
// ============================================================================
void bt_state1(void)
{
    if (BT_IGNORE == 0) {
        bt_picker();
    } else if (BT_IGNORE != 1) {
        goto bt_state1_tail;
    }
    bt_action_runner();

bt_state1_tail:
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 0x40);

    entity_check_visual_range(4000);

    if (BT_DELAY != 0) {
        BT_DELAY = (short)(BT_DELAY - 1);
    }
    if (BT_TRAIL == 0) {
        eub(ENTITY, 0x178) = 0;
        eub(ENTITY, 0x179) = 0;
        return;
    }
    BT_COUNT = (short)(BT_COUNT + 1);
}

// ============================================================================
// bt_state2 @ 0x0044f470 - the "build a web" wait, run every frame while the
// boss is idle-building; action_behavior 0 sets the animation and waits.
// ============================================================================
void bt_state2(void)
{
    if (BT_BEHAVIOR == 0) {
        BT_ANIM_ID = 1;
        bt_web_build();
    }
}

// ============================================================================
// bt_state3 @ 0x0044f490 - the web-shoot state. Entered by the damage system;
// maps hit_state to a behaviour and shoots. Behaviours 1/2 take shoot_c,
// 6/8/9 take shoot_b, everything else the default shoot_a.
// ============================================================================
void bt_state3(void)
{
    if (BT_IGNORE == 0) {
        BT_BEHAVIOR = (unsigned char)(BT_HITSTATE >> 3);
        BT_IGNORE   = 1;
        if (BT_SPLAT != 0) {
            BT_SUBSTATE = 3;
            BT_BEHAVIOR = (unsigned char)BT_SPLAT;
        }
    }

    switch (BT_BEHAVIOR) {
    case 1:
    case 2:
        BT_SPLAT = 1;
        bt_web_shoot_c();
        return;
    case 6:
    case 8:
    case 9:
        BT_SPLAT = 6;
        bt_web_shoot_b();
        return;
    default:
        bt_web_shoot_a();
        return;
    }
}

// ============================================================================
// bt_picker @ 0x0044f690 - the behaviour picker (ignore_player_flag == 0).
// While the post-bite cooldown (0x17E) is running it just sits; otherwise it
// chases the player with behaviour 1, closes for the bite (12) / spit (13)
// and randomly retreats as behaviour 2/3 when it loses line of sight.
// ============================================================================
void bt_picker(void)
{
    bt_dist();

    unsigned char savedSub = BT_SUBSTATE;

    if (BT_COOLDOWN != 0) {
        if (2000 < g_playerDisplacement) {
            BT_IGNORE   = 0;
            BT_BEHAVIOR = 1;
            BT_SUBSTATE = savedSub;
        }
        BT_COOLDOWN = (short)(BT_COOLDOWN - 1);
        return;
    }

    if ((BT_COUNT < 0x15) && ((eub(ENTITY, 0x16E) & 1) != 0)) {
        if (g_playerDisplacement < 5000) {
            BT_IGNORE = 1;
            BT_SET_BEHAVIOR(0xC);         // word: behavior=0xC, action_state=0
        }
        if (BT_DELAY == 0) {
            int turn = (int)(short)turn_toward_target(
                (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 0x200);
            if (turn == 0) {
                BT_IGNORE = 1;
                BT_SET_BEHAVIOR(0xD);     // word
            }
        }
        if (9000 < g_playerDisplacement) {
            BT_IGNORE   = 0;
            BT_BEHAVIOR = 1;              // byte + explicit sub restore (original)
            BT_SUBSTATE = savedSub;
            return;
        }
    } else {
        BT_IGNORE = 1;
        BT_SET_BEHAVIOR((rand() & 1) + 2); // word
        BT_COUNT = 0;
    }
}

// ============================================================================
// bt_action_runner @ 0x0044f560 - executes the behaviour chosen by the picker.
// First updates the pathfind/LOS flag, then dispatches on action_behavior.
// ============================================================================
void bt_action_runner(void)
{
    unsigned int path = entity_pathfind_update();
    g_animFrameIdSave = path;
    if ((path & 0xFE) == 0) {
        BT_PATHW = (unsigned short)((BT_PATHW & 0xFFFE) | (path & 1));
    }

    switch (BT_BEHAVIOR) {
    case 0:  bt_behaviour_idle();  return;
    case 1:  // walk toward, short turn
    case 4:  // post-circle resume walk (0x44f674 index-> no-op RET in the
             // original, but the picker strands it with ignore=0 at mid-range
             // and clear LOS; routing it here keeps the chase going)
        BT_ANIM_ID = 3;
        BT_ANGLE = (short)(BT_ANGLE + (short)turn_toward_target(
            (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 0x10) *
            (euw(ENTITY, 0x16E) & 1));
        BT_CFLAG = 6;
        bt_behaviour_walk();
        return;
    case 2:  bt_behaviour_strafe();  return;
    case 3:  // close in, fast
        BT_ANIM_ID = 2;
        BT_TURN = 0x80;
        bt_behaviour_approach();
        return;
    case 10: bt_behaviour_circle();  return;
    case 0xC: bt_behaviour_bite();   return;
    case 0xD: bt_behaviour_spit();   return;
    }
}

// ============================================================================
// bt_behaviour_idle @ 0x0044f800 - behaviour 0. Stands still on the standard
// animation; the picker owns the transitions, so there is no dwell/re-roll.
// ============================================================================
void bt_behaviour_idle(void)
{
    if (BT_SUBSTATE == 0) {
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_ANIM_ID = 0;
        BT_BLEND = 3;
    } else if (BT_SUBSTATE != 1) {
        return;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// bt_behaviour_walk @ 0x0044f880 - behaviour 1 (turn toward and walk). Plays a
// scuttle SFX on certain animation frames, measures the leg reach
// (bt_leg_reach), clamps the walk speed and steps off the action when the
// dwell runs out.
// ============================================================================
void bt_behaviour_walk(void)
{
    if (BT_SUBSTATE == 0) {
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_DWELL = (short)((rand() & 0x1F) * (unsigned char)BT_BEHAVIOR);
        if ((eub(ENTITY, 0x86) & 0xFE) == 0) {
            BT_DWELL = (short)(BT_DWELL + 0x50);
        }
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_walk[0];
    } else if (BT_SUBSTATE != 1) {
        return;
    }

    unsigned char sfx = bt_frame_sfx[eub(ENTITY, 0xBE)];
    if (sfx != 0) Snd_em(sfx);

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    unsigned char frame = eub(ENTITY, 0xBE);
    unsigned int  part  = 1;    // ground-reach leg pair
    if (frame == 0 || (8 < frame)) part = 0;
    bt_leg_reach((unsigned char)part, BT_TILT);

    if (0x50 < BT_SPEED) {
        BT_SPEED = 0x50;
    }
    Add_speedXZ(0);

    short d = BT_DWELL;
    BT_DWELL = (short)(d - 1);
    if (d == 0) {
        BT_IGNORE = 0;
        BT_SET_BEHAVIOR(0);           // word: behavior=0, action_state=0
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_idle;
    }
}

// ============================================================================
// bt_behaviour_strafe @ 0x0044fb60 - behaviour 2. Side-steps at a fixed angle
// (0x18, or -0x18 when the dwell bit is odd), flips to the spit when it faces
// the player.
// ============================================================================
void bt_behaviour_strafe(void)
{
    if (BT_SUBSTATE == 0) {
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_ANIM_ID = 3;
        BT_DWELL = (short)(rand() & 0x3F);
        BT_BLEND = 3;
        BT_TURN = 0x18;
        if ((eub(ENTITY, 0xC4) & 1) != 0) {
            BT_TURN = (short)0xFFE8;
        }
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_walk[0];
    } else if (BT_SUBSTATE != 1) {
        return;
    }

    BT_ANGLE = (short)(BT_ANGLE + BT_TURN);
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    if ((eub(ENTITY, 0x16E) & 1) != 0) {
        int turn = (int)(short)turn_toward_target(
            (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, BT_TURN);
        if (turn == 0 && BT_DELAY == 0) {
            BT_IGNORE = 1;
            BT_SET_BEHAVIOR(0xD);         // word
            return;
        }
    }

    short d = BT_DWELL;
    BT_DWELL = (short)(d - 1);
    if (d == 0) {
        BT_IGNORE = 0;
        BT_SET_BEHAVIOR(0);           // word
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_idle;
    }
}

// ============================================================================
// bt_behaviour_approach @ 0x0044fa00 - behaviour 3. Close the gap, holding the
// approach turn step (BT_TURN); when the dwell or the facing runs out the
// behaviour collapses back to the idle/chase.
// ============================================================================
void bt_behaviour_approach(void)
{
    char sub = (char)BT_SUBSTATE;
    if (sub == 0) {
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_HITSTATE = 0;
        BT_SUBSTATE = 1;
        BT_DWELL = (short)((rand() & 0x1F) + 0x50);
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_walk[0];
    } else if (sub != 1) {
        if (sub != 2) return;
        BT_HITSTATE = 0;
        BT_IGNORE = 0;
        BT_SET_BEHAVIOR(0);           // word: behavior=0, action_state=0
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_idle;
        BT_WAY_X = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[0];
        BT_WAY_Z = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[2];
        return;
    }

    int turn = turn_toward_target((VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t,
                                  BT_TURN);
    g_animFrameIdSave = (unsigned int)turn;   // stored to the scratch global, as the original does
    short d = BT_DWELL;
    BT_DWELL = (short)(d - 1);
    if (d == 0 || turn == 0) {
        BT_SUBSTATE = 2;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    BT_ANGLE = (short)(BT_ANGLE + (short)turn);
}

// ============================================================================
// bt_behaviour_circle @ 0x0044fcd0 - behaviour 10. Circles the player at speed,
// correcting the heading each frame and chaining the world position.
// ============================================================================
void bt_behaviour_circle(void)
{
    char sub = (char)BT_SUBSTATE;
    if (sub == 0) {
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_SUBSTATE = 1;
        BT_ANIM_ID = 6;
        BT_TURN = (short)((rand() & 1) * -0x658 + 0x32C);
        BT_TURN = (short)(BT_TURN + ((euw(ENTITY, 0x16E) & 1) == 0) * (rand() & 0xFFF));
        BT_SPEED = (short)((rand() & 0x3F) + 0xFA);
        Add_speedXZ((int)BT_TURN);
    } else if (sub != 1) {
        if (sub != 2) return;
        BT_HITSTATE = 0;
        BT_IGNORE = 0;
        BT_SET_BEHAVIOR(((euw(ENTITY, 0x16E) & 1) << 2)); // word: behavior=0/4, sub=0
        BT_COUNT = 0;
        return;
    }

    BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    BT_ANGLE = (short)(BT_ANGLE + (short)turn_toward_target(
        (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 0x40));
    BT_X = BT_X + (int)(short)ew(ENTITY, 0x78);
    BT_Y = BT_Y + (int)(short)ew(ENTITY, 0x7A);
    BT_Z = BT_Z + (int)(short)ew(ENTITY, 0x7C);
}

// ============================================================================
// bt_behaviour_bite @ 0x0044fe60 - behaviour 12, the bite-and-chew. A long
// sub-state walk (wait -> rush -> turn -> bite -> shake -> recover). The bite
// tests the two fang joints against the player's position (the g_deadMoveValue
// + 0x14 block staged into g_playerPosScratch) and costs 20 health.
// ============================================================================
void bt_behaviour_bite(void)
{
    switch (BT_SUBSTATE) {
    case 0:
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_ANIM_ID = 8;
        // fall through
    case 1:
        BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    case 2:
        BT_SUBSTATE = 3;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_ANIM_ID = 0xB;
        BT_DWELL = (short)((rand() & 0x1F) + 0x14);
        // fall through
    case 3:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        {
            int turn3 = (int)(short)turn_toward_target(
                (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 0x40);
            g_animFrameIdSave = (unsigned int)turn3;
            BT_ANGLE = (short)(BT_ANGLE + (short)turn3);
            if (turn3 == 0) BT_SUBSTATE = 4;
        }
        {
            short d = BT_DWELL;
            BT_DWELL = (short)(d - 1);
            if (d == 0) { BT_SUBSTATE = 4; return; }
        }
        break;
    case 4:
        BT_SUBSTATE = 5;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_ANIM_ID = 9;
        // fall through
    case 5:
        if (eub(ENTITY, 0xBE) == 0xC && g_playerEntity.isBeingAttackedFlag == 0) {
            // Stage the g_deadMoveValue + 0x14 block into g_playerPosScratch, then
            // test the two fang joints against the player's position (0xbe6318).
            const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
            g_playerPosScratch.x = seed[0];
            g_playerPosScratch.y = seed[1];
            g_playerPosScratch.z = seed[2];
            g_playerPosScratch.pad = seed[3];
            int joints = (int)(uintptr_t)ENTITY->jointsStructs;
            unsigned char b1 = FUN_0048ae00((MATRIX*)(joints + 0xC0), &g_playerPosScratch,
                                            0x514, (int*)PLAYER_T);
            player_distance_z = (unsigned int)b1;
            unsigned char b2 = FUN_0048ae00((MATRIX*)(joints + 0x13C), &g_playerPosScratch,
                                            0x514, (int*)PLAYER_T);
            g_scaled_down_dist = (unsigned int)b2;
            if (player_distance_z != 0 || g_scaled_down_dist != 0) {
                g_animFrameIdSave = is_facing_toward_entity(&g_playerEntity) & 0xFF;
                Snd_em(4);
                g_playerEntity.health = (short)(g_playerEntity.health - 0x14);
                if (g_playerEntity.health < 0) g_playerEntity.health = 1;
                if (g_playerEntity.health >= 0) {
                    g_playerEntity.isBeingAttackedFlag = (unsigned char)(g_animFrameIdSave + 1);
                    g_playerEntity.action_behavior = (unsigned char)(g_animFrameIdSave + 0x66);
                }
            }
        }
        BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        break;
    case 6:
        Snd_em(3);
        BT_IGNORE = 0;
        BT_SET_BEHAVIOR(0);           // word: behavior=0, action_state=0
        if (g_playerEntity.health < 0) {
            euw(ENTITY, 0x84) = 0x101;
            BT_SET_BEHAVIOR(0xC);     // word
            return;
        }
    }
}

// ============================================================================
// bt_behaviour_spit @ 0x00450180 - behaviour 13. Rears back and spits three
// globs of acid at the player; when the sub-state trips, drops back to the bite.
// ============================================================================
void bt_behaviour_spit(void)
{
    char sub = (char)BT_SUBSTATE;
    if (sub == 0) {
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_ANIM_ID = 0xC;
        BT_DWELL = 0;
        g_playerPosScratch.x = 1000;
        g_playerPosScratch.y = -600;
        g_playerPosScratch.z = 0;
        unsigned int r1 = (unsigned int)rand();
        unsigned int r2 = (unsigned int)rand();
        int n = 2 - (r1 & 1) * (r2 & 1);
        g_collPushDepthZHi = n * 0x80;
        Effect_CreateBillboard(0x1E, 0, (short)(n * -0x80), (void*)((char*)ENTITY + 0x20), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x1E, 0, 0, (void*)((char*)ENTITY + 0x20), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x1E, 0, (short)g_collPushDepthZHi, (void*)((char*)ENTITY + 0x20), &g_playerPosScratch, 0);
        Snd_em(7);
    } else if (sub != 1) {
        if (sub != 2) return;
        euw(ENTITY, 0x84) = 0x101;
        BT_SET_BEHAVIOR(0xC);         // word: behavior=0xC, action_state=0
        BT_DELAY = 0x14;
        return;
    }

    BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// bt_leg_reach @ 0x004512e0
// Measures how far the boss's leg reach is and stores it in move_speed_current
// (Entity+0xC2). Composes the leg-chain world matrices (an eight-joint stride,
// the two joints at index part*8+3 and part*8+4) and subtracts the leg tip's
// world position to get a reach vector, then returns its magnitude. If scale !=
// 0 the whole chain matrix is first scaled isotropically. Clears the speed (and
// returns) when the "should this leg exist" flag is not set.
// ============================================================================
void bt_leg_reach(unsigned char part, int scale)
{
    int joints = (int)(uintptr_t)ENTITY->jointsStructs;

    RotMatrix(BT_ROT, BT_MATRIX);
    CompMatrix(BT_MATRIX, (MATRIX*)(joints + 0x24), &g_matrixScratch);

    if (scale != 0) {
        g_playerPosScratch.x = scale;
        g_playerPosScratch.y = scale;
        g_playerPosScratch.z = scale;
        ScaleMatrixCols(&g_matrixScratch, &g_playerPosScratch);
    }

    // 0x1f0 = joint 4 * 0x7c; 0x3e0 = an eight-joint stride. The legs are packed
    // eight joints apart, so `part` selects a leg pair.
    unsigned char* tip = (unsigned char*)(joints + (unsigned int)part * 0x3E0 + 0x1F0);

    if ((*tip & 1) == 0) {
        BT_SPEED = 0;
        return;
    }

    unsigned char b = 2;
    do {
        CompMatrix(&g_matrixScratch, (MATRIX*)(tip + (unsigned int)b * 0x7C - 0x58), &g_matrixScratch);
        b--;
    } while (b != 0);

    g_matrixScratch.t[0] = g_matrixScratch.t[0] - *(int*)(tip + 0x58);
    g_matrixScratch.t[1] = 0;
    g_matrixScratch.t[2] = g_matrixScratch.t[2] - *(int*)(tip + 0x60);

    FUN_0040a380((VECTOR*)g_matrixScratch.t, &g_playerPosScratch);
    BT_SPEED = (short)SquareRoot0(g_playerPosScratch.z + g_playerPosScratch.x);
}

// ============================================================================
// bt_web_build @ 0x004502e0 - the idle "spin a web" behaviour (state 2). Walks
// down the web-joint registry (8 odd leg/joint indices) and attaches a fresh
// web thread to the next unused one, spawning two collision joints and a pair
// of billboards per thread. When the sub-state trips, it hands off to a chase
// behaviour (3, or 10 if the LOS is blocked).
//
// NOTE: the original writes the registry at index WEBIDX (Entity+0x17C) with no
// bound, so past the eighth web it ran off the end of the 8-byte registry into
// the neighbouring table. The port masks the index to 8 slots (a ring) so the
// registry stays in-bounds while keeping the "find the next free leg" intent.
// ============================================================================
void bt_web_build(void)
{
    char sub = (char)BT_SUBSTATE;
    if (sub == 0) {
        BT_SUBSTATE = 1;
        BT_SPEED = 0;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        Snd_em(4);

        if ((BT_HITSTATE & 1) == 0) {
            unsigned int pick = (unsigned int)rand() & 7;
            unsigned char chosen = bt_web_joints[pick];
            g_entity_bkp = 0;
            int tvar = 7;
            int i;
            do {
                i = tvar;
                if (bt_web_joints_used[tvar] == chosen) g_entity_bkp = 1;
                tvar--;
            } while (i != 0);

            if (g_entity_bkp == 0) {
                int joints = (int)(uintptr_t)ENTITY->jointsStructs;
                bt_web_joints_used[BT_WEBIDX & 7] = chosen;
                joint_setup_attack_effect(joints + (unsigned int)chosen * 0x7C, 0x1E, 0x14, 3);
                joint_setup_attack_effect(joints +
                    (unsigned int)(unsigned char)bt_web_joints[pick] * 0x7C + 0x7C, 0x1E, 0x14, 3);
                g_playerPosScratch.x = 0;
                g_playerPosScratch.y = 0;
                g_playerPosScratch.z = 0;
                Effect_CreateBillboard(0, 8, 0,
                    (void*)(joints + (unsigned int)(unsigned char)bt_web_joints[pick] * 0x7C + 0x44),
                    &g_playerPosScratch, 0);
                Effect_CreateBillboard(0, 8, 0,
                    (void*)(joints + (unsigned int)(unsigned char)bt_web_joints[pick] * 0x7C + 0xC0),
                    &g_playerPosScratch, 0);
                Snd_em(5);
                BT_WEBIDX = (short)(BT_WEBIDX + 1);
            }
        }
    } else if (sub != 1) {
        if (sub != 2) return;
        euw(ENTITY, 0x84) = 0x101;        // state->run, ignore=1
        BT_SET_BEHAVIOR(3);               // word: behavior=3, action_state=0
        BT_WAY_X = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[0];
        BT_WAY_Z = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[2];
        neptune_clear_hit_state();
        if ((eub(ENTITY, 0x16E) & 1) != 0) {
            euw(ENTITY, 0x84) = 0x101;    // state->run, ignore=1
            BT_SET_BEHAVIOR(10);          // word: behavior=10, action_state=0
        }
        BT_DELAY = 0;
        return;
    }

    BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// bt_web_shoot_a @ 0x00450550 - the default web-shooter (state 3, behaviours
// not 1/2/6/8/9). Case 0 primes the fangs; case 2/3 animate the spit, swap the
// SCA to the shoot profile and clone the web threads (count 0x33 = 51).
// Case 4 keeps the cloned threads flying via ws_update_webs.
// ============================================================================
void bt_web_shoot_a(void)
{
    switch (BT_SUBSTATE) {
    case 0: {
        euw(ENTITY, 0xC2) = 0;
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 0;
        int joints = (int)(uintptr_t)ENTITY->jointsStructs;
        BillboardSetSize(&ENTITY->pushVelocity, 0, 0);
        joint_setup_attack_effect(joints, 0x1E, 0x14, 3);
        joint_setup_attack_effect(joints + 0x934, 0x1E, 0x14, 3);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x978), &g_playerPosScratch, 0);
        Snd_em(5);
        unsigned int j = 8;
        do {
            unsigned char* p = (unsigned char*)(joints + 0x7C + j * 0xF8);
            unsigned char v = *p;
            if (v != 0 && (v & 0x20) == 0) {
                *(unsigned char*)(joints + 0xF8 + j * 0xF8) |= 0xC;
                *(unsigned char*)(joints + 0x174 + j * 0xF8) |= 0x10;
            }
            j--;
        } while (j != 0);
        // fall through
    }
    case 1:
        BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    case 2:
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00DF809F);
        BillboardSetSize(&ENTITY->pushVelocity, 500, 500);
        BT_SUBSTATE = 3;
        eub(ENTITY, 0x00) |= 0xA;         // status_flags
        euw(ENTITY, 0xC2) = 0;
        euw(ENTITY, 0xC4) = 0x5A;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_shoot;
        Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
        // fall through
    case 3:
        BillboardAdjSize(&ENTITY->pushVelocity, 9, 9);
        BT_DWELL = (short)(BT_DWELL - 1);
        if (BT_DWELL == 0) {
            Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
            BT_SUBSTATE = 4;
            ws_clone_entity(0x33, 0xF2, ENTITY->jointCount, (unsigned int*)&ENTITY->scd_target_ptr);
        }
        return;
    case 4:
        Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
        ws_update_webs(0x33);
        return;
    }
}

// ============================================================================
// bt_web_shoot_b @ 0x00450810 - the web-shooter for state 3 behaviours 6/8/9.
// Nearly identical to bt_web_shoot_a but uses the shoot profile earlier and
// clones a variable web count ((hit_state >> 3) * 5 + 1).
// ============================================================================
void bt_web_shoot_b(void)
{
    unsigned char sub = BT_SUBSTATE;
    if (sub == 1 || sub == 3) {
        if ((BT_HITSTATE & 2) != 0) {
            int joints = (int)(uintptr_t)ENTITY->jointsStructs;
            BillboardSetSize(&ENTITY->pushVelocity, 0, 0);
            joint_setup_attack_effect(joints, 0x1E, 0x1E, 3);
            joint_setup_attack_effect(joints + 0x934, 0x1E, 0x1E, 3);
            g_playerPosScratch.x = 0;
            g_playerPosScratch.y = 0;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x978), &g_playerPosScratch, 0);
            Snd_em(5);
            unsigned int j = 8;
            do {
                unsigned char* p = (unsigned char*)(joints + 0x7C + j * 0xF8);
                unsigned char v = *p;
                if (v != 0 && (v & 0x20) == 0) {
                    *p = (unsigned char)(v | 0xC);
                    *(unsigned char*)(joints + 0xF8 + j * 0xF8) |= 0x10;
                }
                j--;
            } while (j != 0);
            BT_BEHAVIOR = 3;
            BT_SUBSTATE = 2;
            return;
        }
        if ((BT_HITSTATE & 1) != 0) {
            BT_SUBSTATE = 4;
        }
    }

    switch (BT_SUBSTATE) {
    case 0: {
        euw(ENTITY, 0xC2) = 0;
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_HITSTATE = 0;
        BT_ANIM_ID = 4;
        BillboardSetSize(&ENTITY->pushVelocity, 0x708, 0x708);
        // fall through
    }
    case 1:
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        if (0xD < eub(ENTITY, 0xBE)) ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 0x20);
        BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    case 2:
        eub(ENTITY, 0x00) |= 0xA;         // status_flags
        BT_ANIM_ID = 5;
        BT_SUBSTATE = 3;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_HITSTATE = 0;
        BT_DWELL = 0xB4;
        Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
        // fall through
    case 3: {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 0x20);
        short s = BT_DWELL;
        BT_DWELL = (short)(s - 1);
        if (s == 0) {
            BT_SUBSTATE = 4;
            BT_HITSTATE = 0x11;
        }
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    }
    case 4:
        BT_ANIM_ID = 5;
        BT_SUBSTATE = 5;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 1;
        Snd_em(5);
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00DF809F);
        BillboardSetSize(&ENTITY->pushVelocity, 2000, 2000);
        euw(ENTITY, 0xC4) = 100;
        euw(ENTITY, 0xC2) = 0;
        eub(ENTITY, 0x00) |= 0xA;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_shoot;
        // fall through
    case 5:
        BillboardAdjSize(&ENTITY->pushVelocity, 4, 4);
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        BT_DWELL = (short)(BT_DWELL - 1);
        if (BT_DWELL != 0) return;
        BT_SUBSTATE = 6;
        ws_clone_entity((unsigned char)((BT_HITSTATE >> 3) * 5 + 1), 0xF2,
                        ENTITY->jointCount, (unsigned int*)&ENTITY->scd_target_ptr);
        return;
    case 6:
        Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
        ws_update_webs((char)((BT_HITSTATE >> 3) * 5 + 1));
        return;
    }
}

// ============================================================================
// bt_web_shoot_c @ 0x00450c90 - the web-shooter for state 3 behaviours 1/2.
// The longest of the three shooters: primes the fangs, spits a web that grows
// the shadow, clones the threads and flies them at the player while steering
// with Add_speedXZ.
// ============================================================================
void bt_web_shoot_c(void)
{
    unsigned char sub = BT_SUBSTATE;
    if (sub == 1 || sub == 3) {
        if ((BT_HITSTATE & 2) != 0) {
            int joints = (int)(uintptr_t)ENTITY->jointsStructs;
            BillboardSetSize(&ENTITY->pushVelocity, 0, 0);
            joint_setup_attack_effect(joints, 0x1E, 0x1E, 3);
            joint_setup_attack_effect(joints + 0x934, 0x1E, 0x1E, 3);
            g_playerPosScratch.x = 0;
            g_playerPosScratch.y = 0;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x978), &g_playerPosScratch, 0);
            Snd_em(5);
            unsigned int j = 8;
            do {
                unsigned char* p = (unsigned char*)(joints + 0x7C + j * 0xF8);
                unsigned char v = *p;
                if (v != 0 && (v & 0x20) == 0) {
                    *p = (unsigned char)(v | 0xC);
                    *(unsigned char*)(joints + 0xF8 + j * 0xF8) |= 0x10;
                }
                j--;
            } while (j != 0);
            BT_BEHAVIOR = 3;
            BT_SUBSTATE = 2;
            return;
        }
        if ((BT_HITSTATE & 1) != 0) {
            BT_SUBSTATE = 4;
        }
    }

    switch (BT_SUBSTATE) {
    case 0: {
        euw(ENTITY, 0xC2) = 0;
        BT_SUBSTATE = 1;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_HITSTATE = 0;
        BT_ANIM_ID = 1;
        int joints = (int)(uintptr_t)ENTITY->jointsStructs;
        BillboardSetSize(&ENTITY->pushVelocity, 0x5DC, 0x5DC);
        if (BT_WEBIDX < 6) {
            joint_setup_attack_effect(joints + 0x934, 0x1E, 0x1E, 3);
        }
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0xB, 0, (void*)(joints + 0xC0), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0xB, 0, (void*)(joints + 0x13C), &g_playerPosScratch, 0);
        Snd_em(5);
        // fall through
    }
    case 1:
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        BT_SUBSTATE = BT_SUBSTATE + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    case 2:
        BT_ANIM_ID = 7;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_DWELL = 0xD2;
        eub(ENTITY, 0x00) |= 0xA;         // status_flags
        BT_HITSTATE = 0;
        Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
        BT_SUBSTATE = 3;
        // fall through
    case 3: {
        unsigned short u = euw(ENTITY, 0xC4);
        euw(ENTITY, 0xC4) = (unsigned short)(u - 1);
        if ((u & 1) != 0) {
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        }
        if (eub(ENTITY, 0xBE) == 0) {
            ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
            ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 0x20);
            g_playerPosScratch.x = 0;
            g_playerPosScratch.y = 0;
            g_playerPosScratch.z = 0;
            int joints = (int)(uintptr_t)ENTITY->jointsStructs;
            Effect_CreateBillboard(0, 0xB, 0, (void*)(joints + 0xC0), &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0xB, 0, (void*)(joints + 0x13C), &g_playerPosScratch, 0);
        }
        if (euw(ENTITY, 0xC4) == 0) {
            BT_SUBSTATE = 4;
            BT_HITSTATE = 0x11;
            return;
        }
        break;
    }
    case 4:
        BT_ANIM_ID = 1;
        BT_SUBSTATE = 5;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 1;
        Snd_em(5);
        BT_DWELL = 100;
        euw(ENTITY, 0xC2) = 0x78;
        eub(ENTITY, 0x00) |= 0xA;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)bt_sca_info_shoot;
        ws_clone_entity((unsigned char)((BT_HITSTATE >> 3) * 5 + 1), 0xF2,
                        ENTITY->jointCount, (unsigned int*)&ENTITY->scd_target_ptr);
        // fall through
    case 5:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ws_update_webs((char)((BT_HITSTATE >> 3) * 5 + 1));
        BT_SPEED = (short)(BT_SPEED - 1);
        {
            unsigned int i = (unsigned int)rand() & 7;
            unsigned int j = (unsigned int)rand();
            Add_speedXZ(((j & 1) + 1) * (int)(unsigned char)bt_web_joints[i] * 100);
        }
        BT_DWELL = (short)(BT_DWELL - 1);
        if (BT_DWELL == 0) {
            BT_SUBSTATE = 6;
            return;
        }
        break;
    case 6:
        euw(ENTITY, 0xC2) = 0;
        BT_SUBSTATE = 7;
        BT_ANIM_FRAME = 0;
        BT_TIMING = 0;
        BT_BLEND = 3;
        BT_ANIM_ID = 4;
        // fall through
    case 7:
        ws_update_webs((char)((BT_HITSTATE >> 3) * 5 + 1));
        if ((unsigned char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            BT_SUBSTATE = 8;
            BT_DWELL = 0x3C;
        }
        break;
    case 8:
        Flg_on((int)g_EnemiesFlags, BT_DEATH_EV);
        ws_update_webs((char)((BT_HITSTATE >> 3) * 5 + 1));
        return;
    }
}

// ============================================================================
// bt_state_table @ 0x004bf3a8 - the four state function pointers.
// ============================================================================
void (*bt_state_table[4])(void) = {
    bt_state0, bt_state1, bt_state2, bt_state3
};

// ============================================================================
// black_tiger_update @ 0x0044f300 - the per-frame entry, dispatch table [4].
// The state function runs first, then the shared SCA-collision tail (except it
// is unconditional here, unlike the Web Spinner's state-5 skip), then the
// switch-zone fade sprite.
// ============================================================================
void black_tiger_update(void)
{
    if ((g_message_flags & 4) != 0) {
        bt_state_table[BT_STATE]();
        SetEntityScaHitData(ENTITY);
        euw(ENTITY, 0x170) = (unsigned short)ResolveEntityScaCollision((Entity*)&g_playerEntity, ENTITY);
        HandleEnemyPlayerCollisions();
        euw(ENTITY, 0x176) = (unsigned short)check_room_collision(
            (VECTOR*)((char*)ENTITY + 0x34), *(short*)(*(int*)((char*)ENTITY + 4) + 10));
    }

    eub(ENTITY, 0x03) = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)((char*)ENTITY + 0x34), g_CurrentRdtDataTypePtr);
    if (eub(ENTITY, 0x03) != 0) {
        entity_add_fade_sprite((VECTOR*)((char*)ENTITY + 0x34),
            (short*)((char*)ENTITY + 0xE4), 0, euw(ENTITY, 0x74));
    }
}
