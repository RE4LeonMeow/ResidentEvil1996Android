// WebSpinner.cpp - Web Spinner (entity type 3, enemy/em1003.emd) - the big spider
//
// Original PC addresses:
//   web_spinner_update          0x00478310   per-frame entry, dispatch table [3]
//   wsp_state0  (init)          0x00478140
//   wsp_state1  (run)           0x00478420
//   wsp_state2  (web-build)     0x00478550
//   wsp_state3  (web-shoot)     0x00478570
//   wsp_state4                  0x00478620   bare RET
//   wsp_state5                  0x00478630   bare RET
//   FUN_00478640 (action run)   0x00478640
//   FUN_00478820 (dispatch)     0x00478820
//   wsp_handlerA                0x00478870   spawn-variant 0 behaviour picker
//   wsp_handlerB                0x004789b0   spawn-variant 1 behaviour picker
//   wsp_handlerC                0x00478b20   spawn-variant 2 behaviour picker
//   FUN_00478be0 (behaviour 0)  0x00478be0   idle (two paths by behavior_flags)
//   FUN_00478d90 (behaviour 1)  0x00478d90   walk/turn
//   FUN_00478ed0 (behaviour 3/5)0x00478ed0   approach
//   FUN_00479030 (behaviour 2)  0x00479030   strafe
//   FUN_00479190 (behaviour 6)  0x00479190   lunge bite
//   FUN_00479590 (behaviour 7)  0x00479590   spit
//   FUN_00479680 (behaviour 8)  0x00479680   web drop
//   FUN_00479820 (behaviour 9)  0x00479820   bare RET
//   FUN_00479830 (behaviour 10) 0x00479830   circle
//   FUN_004799d0 (web build)    0x004799d0
//   FUN_00479c40 (web shoot A)  0x00479c40
//   FUN_00479f10 (web shoot B)  0x00479f10
//   FUN_0047a380 (leg reach)    0x0047a380
//   FUN_0048a630 (clone entity) 0x0048a630
//   FUN_0048a730 (web update)   0x0048a730
//
// The id is 3 and the model is em1003.emd: g_emdPathTable index (id + 4) = 7.
//
// ---------------------------------------------------------------------------
// TWO tables share one block at 0x004c4708 (the "overlapping dispatch tables"
// trap). The state table is REALLY six entries (states 0-5) followed by the
// three behaviour pickers; the picker dispatch in FUN_00478820 indexes the
// SAME memory from base 0x004c4720 (state table + 6) by behavior_flags. So:
//
//   state 0..5            wsp_state0 .. wsp_state5
//   state 6/7/8/9         handlerA / handlerB / handlerC / handlerC
//
// and `wsp_handler_table[k]` aliases `wsp_state_table[6 + k]`. Reproduced
// verbatim by taking the address past the sixth pointer.
//
// ---------------------------------------------------------------------------
// Shape of the AI
// ---------------------------------------------------------------------------
//   Entity+0x02 behavior_flags  the SPAWN KIND, clamped to 2 in state0:
//                                 0/1 -> the spider sits at world Y = 0
//                                 2   -> hung at Y = -6136 (or -7200 in stage 3)
//                                       i.e. dropping from the ceiling; the SCA
//                                       is swapped to the small half-size profile
//   Entity+0x84 state           init / run / web-build / web-shoot / RET / RET
//   Entity+0x85 ignore_player_flag 0 = the behaviour picker runs this frame,
//                                  1 = the chosen behaviour owns the spider,
//                                  anything else = frozen.
//   Entity+0x86 action_behavior 0-10, the thing the spider is doing.
//   Entity+0x87 action_state     the per-behaviour animation sub-state.
//
// The behaviour picker (handlerA/B/C, chosen by behavior_flags) only decides
// WHICH behaviour to enter; the action runner (FUN_00478640) executes it. The
// picker sets ignore_player_flag=1 and action_behavior=N; the runner then steps
// the behaviour's own sub-state machine each frame. Behaviours that complete
// reset ignore_player_flag=0 so the picker runs again next frame.
//
// ---------------------------------------------------------------------------
// The two web-shooters, and the web projectiles they spit
// ---------------------------------------------------------------------------
// State 3 (wsp_state3) is the web-shoot state, entered by the damage system.
// It maps the damage hit_state (bits 3-6 of 0x8a) into a behaviour and shoots.
// The two shoot behaviours (0x00479c40 / 0x00479f10) clone the spider `count`
// times into the room data buffer (FUN_0048a630) - each clone is one thread of
// web the original then flies at the player each frame (FUN_0048a730). The
// clone count derives from hit_state: `(hit_state >> 2 & 0xfe) + 4` or `+ 8`.
//
// FUN_0048a630 / FUN_0048a730 are shared with the weapon/menu projectile code
// (FUN_00450550 etc.) and with Plant 42 / the Tyrant, each of which carries its
// own file-static copy of the clone helper. The Web Spinner carries its own here.
//
// ---------------------------------------------------------------------------
// SCA collision profile
// ---------------------------------------------------------------------------
// Entity+4 gets 0x004c46d0 (big) or 0x004c46e0 (small). Both are ONE 6-short
// record: [0] id/terminator (0x8000), [1..3] local x/y/z, [4] half-height,
// [5] radius - same layout as the wasp's and the zombie's.
//
//   big   {0x8000, 0, -180, 0,   180, 1000}
//   small {0x8000, 0,    0, 0,     0,  500}
//
// The small profile swaps in when the spider shoots a web (half-height 0, so the
// box has no vertical extent). The pointer table at 0x004c46f4 holds the small
// record's address; state0 loads the big one, the shooters load the small.
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
// Offset accessors. The port's Entity struct names these bytes from the
// zombie's point of view; the spider reuses them at other widths with other
// meanings, so they are reached by offset per the "offset writes, not nearest
// field" rule.
// ============================================================================
#define WS_X          (*(int*)            ((char*)ENTITY + 0x34))
#define WS_Y          (*(int*)            ((char*)ENTITY + 0x38))
#define WS_Z          (*(int*)            ((char*)ENTITY + 0x3C))
#define WS_ANGLE      (*(short*)          ((char*)ENTITY + 0x74))
#define WS_SPEED      (*(short*)          ((char*)ENTITY + 0xC2))
#define WS_DWELL      (*(short*)          ((char*)ENTITY + 0xC4))
#define WS_TILT       (*(short*)          ((char*)ENTITY + 0xCA))
#define WS_TURN       (*(short*)          ((char*)ENTITY + 0x16C))
#define WS_PATHW      (*(unsigned short*) ((char*)ENTITY + 0x16E))
#define WS_TOUCH      (*(short*)          ((char*)ENTITY + 0x170))
#define WS_DELAY      (*(short*)          ((char*)ENTITY + 0x172))
#define WS_SPLAT      (*(short*)          ((char*)ENTITY + 0x174))
#define WS_TRAIL      (*(short*)          ((char*)ENTITY + 0x176))
#define WS_COUNT      (*(short*)          ((char*)ENTITY + 0x178))
#define WS_WEBIDX     (*(short*)          ((char*)ENTITY + 0x17A))
#define WS_ANIM_ID    (*(unsigned char*)  ((char*)ENTITY + 0xBD))
#define WS_ANIM_FRAME (*(unsigned char*)  ((char*)ENTITY + 0xBE))
#define WS_TIMING     (*(unsigned char*)  ((char*)ENTITY + 0xBF))
#define WS_BLEND      (*(unsigned char*)  ((char*)ENTITY + 0x8C))
#define WS_HITSTATE   (*(unsigned char*)  ((char*)ENTITY + 0x8A))
#define WS_DEATH_EV   (*(unsigned char*)  ((char*)ENTITY + 0x163))
#define WS_WAY_X      (*(short*)          ((char*)ENTITY + 0x166))
#define WS_WAY_Z      (*(short*)          ((char*)ENTITY + 0x168))
#define WS_WEB_OUT    (*(void**)          ((char*)ENTITY + 0xB8))

// The player's pose the spider reacts to (being knocked down / grabbed).
#define PLAYER_DOWN   (g_playerEntity.action_behavior == 0x14 && g_playerEntity.action_state == 0)

// ============================================================================
// Cross-file dependencies (declared up here so both the anonymous-namespace
// helpers and the file-scope state functions below can use them). SetAnimSlot /
// CreateAnimObject live in TmdAnimation (EntityModelLoader.cpp);
// ResetJointTransforms in EntityModelLoader.cpp; Flg_on in CmdFunctions.cpp;
// is_entity_in_switch_zone in Room.cpp.
// ============================================================================
extern void ResetJointTransforms(void);
extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);
extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2);
extern void Flg_on(int baseAddr, unsigned int bitIndex);
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData);
extern unsigned int g_entity_bkp;   // 0x00be0df4 - shared scratch

// Forward declarations of the file-scope functions (defined below), so the
// state functions and the anonymous-namespace helpers can reference them.
void wsp_state0(void);
void wsp_state1(void);
void wsp_state2(void);
void wsp_state3(void);
void wsp_state4(void);
void wsp_state5(void);
void wsp_handlerA(void);
void wsp_handlerB(void);
void wsp_handlerC(void);
void ws_action_runner(void);          // 0x00478640
void ws_behaviour_idle(void);         // 0x00478be0
void ws_behaviour_walk_turn(void);    // 0x00478d90
void ws_behaviour_strafe(void);       // 0x00479030
void ws_behaviour_approach(void);     // 0x00478ed0
void ws_behaviour_lunge(void);        // 0x00479190
void ws_behaviour_spit(void);         // 0x00479590
void ws_behaviour_webdrop(void);      // 0x00479680
void ws_behaviour_ret(void);          // 0x00479820
void ws_behaviour_circle(void);       // 0x00479830
void ws_web_build(void);              // 0x004799d0
void ws_web_shoot_a(void);            // 0x00479c40
void ws_web_shoot_b(void);            // 0x00479f10
void ws_update_webs(char count);      // 0x0048a730
void ws_leg_reach(unsigned char part, int scale);   // 0x0047a380

// The shared dispatch table (and its behaviour-picker alias, which is the same
// memory starting six pointers in). Defined at the bottom of the file.
extern void (*wsp_state_table[10])(void);
extern void (**wsp_handler_table)(void);

namespace {

// ============================================================================
// Data tables. All original addresses on the right.
// ============================================================================
const short ws_sca_info_big[6]   = { (short)0x8000, 0, -180, 0,   180, 1000 };   // 0x004c46d0
const short ws_sca_info_small[6] = { (short)0x8000, 0,    0, 0,     0,  500 };   // 0x004c46e0
const short* const ws_sca_table[2] = { ws_sca_info_big, ws_sca_info_small };     // 0x004c46f0

// 0x004c46f8 - random health roll, indexed by rand() & 0xf.
const unsigned char ws_health_table[16] = {
   0x63,0x63,0x63,0x63,0x77,0x63,0x63,0x77,
   0x63,0x63,0x63,0x77,0x63,0x59,0x63,0x63
};

// 0x004c4730 - random "next behaviour" table, indexed by (rand & 7) + (LOS*8):
// entries 0-7 when the line of sight is clear, 8-15 when it is blocked.
const unsigned char ws_behavior_pick[16] = {
   0x02,0x01,0x02,0x02, 0x02,0x01,0x02,0x05,
   0x0B,0x0B,0x05,0x0B, 0x03,0x05,0x04,0x04
};

// 0x004c4760 - per-frame scuttle SFX, indexed by animation_frame_id.
const unsigned char ws_frame_sfx[16] = {
   0x00,0x02,0x00,0x00, 0x00,0x00,0x00,0x01,
   0x00,0x00,0x01,0x00, 0x00,0x00,0x01,0x00
};

// 0x004c4770 - the odd leg/joint indices the web can be shot from; 0x004c4778
// is the runtime registry of which are already in use.
const unsigned char ws_web_joints[8] = { 0x03,0x05,0x07,0x09,0x0B,0x0D,0x0F,0x11 };
unsigned char ws_web_joints_used[8] = {};

// helper: rebuild the Manhattan distance to the player into g_playerDisplacement.
static inline void ws_dist(void)
{
    unsigned int dz = (unsigned int)(g_playerEntity.scaMatrixData.localMatrix.t[2] - WS_Z);
    unsigned int dx = (unsigned int)(g_playerEntity.scaMatrixData.localMatrix.t[0] - WS_X);
    int sdz = (int)dz >> 31;
    int sdx = (int)dx >> 31;
    g_playerDisplacement = ((int)((dz ^ sdz) - sdz) - sdx) + (int)(dx ^ sdx);
}

} // namespace

// ============================================================================
// Cross-file dependencies. SetAnimSlot / CreateAnimObject live in TmdAnimation
// (EntityModelLoader.cpp); ResetJointTransforms in EntityModelLoader.cpp;
// Flg_on in CmdFunctions.cpp; is_entity_in_switch_zone in Room.cpp.
// ============================================================================
extern void ResetJointTransforms(void);
extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);
extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2);
extern void Flg_on(int baseAddr, unsigned int bitIndex);
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData);
extern unsigned int g_entity_bkp;   // 0x00be0df4 - shared scratch

// The shared dispatch table (and its behaviour-picker alias, which is the same
// memory starting six pointers in). Defined at the bottom of the file; declared
// here so the state functions above can index it.
extern void (*wsp_state_table[10])(void);
extern void (**wsp_handler_table)(void);

// ============================================================================
// FUN_0048a630 - clone the current entity `count` times into the room data
// buffer and give each clone its own animation object. Plant 42 and the
// Tyrant carry their own file-static copy, per their convention; this one has
// external linkage (the same original function is the Black Tiger's clone
// helper - BlackTiger.cpp declares it extern), and it sits at file scope now
// for that reason. The spider-specific code above still calls it from inside
// the anonymous namespace; the helper itself only touches entity-generic data.
// ============================================================================
void ws_clone_entity(unsigned char count, int /*animSlotBytes*/,
                     unsigned char jointIndex, unsigned int* out)
{
    g_playerDisplacement = (int)(*(unsigned int*)((char*)ENTITY->jointsStructs + 0x14) +
                                 (unsigned int)jointIndex * 0x1c);

    *out = (unsigned int)(uintptr_t)g_loadDataDestPointer;
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + (unsigned int)count * 0x18c;

    Entity* copy = reinterpret_cast<Entity*>(static_cast<uintptr_t>(*out));
    MATRIX savedLocal = ENTITY->scaMatrixData.localMatrix;

    unsigned char remaining = count;
    do {
        std::memcpy(copy, ENTITY, 0x18c);
        copy->scaMatrixData.localMatrix = savedLocal;
        ew(copy, 0x74) = (short)(ew(copy, 0x74) + (short)((unsigned short)remaining * 0x100));
        copy->modelLoadBuffer = (unsigned int)g_playerDisplacement;
        copy->unk_18 = (unsigned int)(uintptr_t)g_loadDataDestPointer;
        SetAnimSlot(reinterpret_cast<AnimSlot*>(static_cast<uintptr_t>(copy->modelLoadBuffer)),
                    (int)&copy->unk_0c, 0);
        g_loadDataDestPointer = CreateAnimObject((int)&copy->unk_0c,
            reinterpret_cast<unsigned int*>(static_cast<uintptr_t>(copy->unk_18)));
        copy->state = 0;
        copy->action_state = (unsigned char)((rand() & 3) == 0);
        copy = reinterpret_cast<Entity*>((char*)copy + 0x18c);
        remaining--;
    } while (remaining != 0);
}

// ============================================================================
// wsp_state0 @ 0x00478140 - one-time init, and the landing pad after death.
// ============================================================================
void wsp_state0(void)
{
    eub(ENTITY, 0x85) = 0;            // ignore_player_flag
    eub(ENTITY, 0x86) = 0;            // action_behavior
    eub(ENTITY, 0x87) = 0;            // action_state
    eu(ENTITY, 0x1c) = 0;             // scaMatrixData.field_00
    eub(ENTITY, 0xc1) = 0;
    euw(ENTITY, 0xc4) = 0;            // action_ticks_counter
    eub(ENTITY, 0xbc) = 0;            // death_timer
    WS_HITSTATE = 0;
    WS_ANIM_ID  = 0;

    // DC behavior, this is the death spawn animation of the webspinner
    // Wesker killed in the Guardhouse cutscene after you kill Plant42
    const bool dcDeadSpawn = g_bDcMode && ENTITY->behavior_flags == 4;
    if (dcDeadSpawn) {
        ENTITY->behavior_flags = 0;
        euw(ENTITY, 0x72) = 0;
        euw(ENTITY, 0x76) = 0;
        WS_HITSTATE = 0x11;
    }

    // Spawn kind decides the resting altitude. 0/1 sit on the floor (Y = 0);
    // variant 2 hangs from the ceiling.
    if (ENTITY->behavior_flags < 2) {
        eu(ENTITY, 0x38) = 0;          // world Y = 0
    }

    if (1 < ENTITY->behavior_flags) {
        eub(ENTITY, 0x72) = 0;         // rotation.x low byte
        eub(ENTITY, 0x73) = 8;         // rotation.x high byte (0x0800)
        eub(ENTITY, 0x76) = 0;         // angle_z low
        eub(ENTITY, 0x77) = 0;         // angle_z high
        ei(ENTITY, 0x38) = 0xFFFFE818; // world Y = -6136 (PS1 negative up)
        if (g_stageId == STAGE_GUARDHOUSE) {
            ei(ENTITY, 0x38) = 0xFFFFE3E0; // world Y = -7200 in stage 3
        }
        if (3 < ENTITY->behavior_flags) {
            ENTITY->behavior_flags = 2;
        }
    }

    ResetJointTransforms();
    eub(ENTITY, 0x84) = dcDeadSpawn ? 3 : 1;
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    g_svecScratch.z = 0;
    g_svecScratch.y = 0;
    g_svecScratch.x = 0;
    g_animFrameIdSave = 0x808080;      // ground-shadow tint
    {
        short half = 0x4B0;
        FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, half, half);
    }

    rand();                            // discarded
    ENTITY->health = (short)(unsigned short)ws_health_table[rand() & 0xf];
    if (dcDeadSpawn) {
        ENTITY->health = -1;
    }

    ENTITY->Sca_info = (unsigned int)(uintptr_t)ws_sca_info_big;
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
    WS_TILT = (short)((ENTITY->behavior_flags & 4) * 0xaaa);   // always 0 after the clamp to 2

    // 0x0047830c..: four WORD clears. A dword write here would also wipe 0x176
    // (the room-collision result) and 0x17c, which the original leaves alone.
    WS_DELAY  = 0;                     // 0x172
    WS_SPLAT  = 0;                     // 0x174
    WS_COUNT  = 0;                     // 0x178
    WS_WEBIDX = 0;                     // 0x17a
}

// ============================================================================
// wsp_state1 @ 0x00478420 - the main AI driver.
// Three layers: the behaviour picker (alias A/B/C) while ignore_player_flag==0,
// the action runner while it is 1, then the shared per-frame bookkeeping.
// ============================================================================
void wsp_state1(void)
{
    if ((ENTITY->behavior_flags & 0x80) != 0) return;   // bit 7 = skip the whole update

    // 0x00478430: the picker and the runner are NOT mutually exclusive. The
    // original tests ignore_player_flag, calls the picker when it is 0, and then
    // FALLS THROUGH into the runner (0x00478443 CALL picker -> 0x00478448 CALL
    // runner). Only a flag that is neither 0 nor 1 jumps past the runner.
    //
    // This matters: the whole autonomous idle -> pick -> walk -> idle cycle runs
    // with ignore_player_flag at 0 (every behaviour clears it back to 0 when it
    // completes; the flag is only the picker-override latch). Gating the runner
    // on == 1 froze the spider in place - no animation, no movement - on every
    // frame in which no picker condition happened to fire.
    if (eub(ENTITY, 0x85) == 0) {
        // Pick a behaviour: compute the Manhattan distance, then dispatch on the
        // spawn kind to handlerA/B/C (aliased at wsp_state_table[6 + kind]).
        ws_dist();
        wsp_state_table[6 + (ENTITY->behavior_flags & 3)]();
        ws_action_runner();
    } else if (eub(ENTITY, 0x85) == 1) {
        ws_action_runner();
    }

    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 0x40);

    entity_check_visual_range(4000);

    if ((ENTITY->behavior_flags & 2) != 0) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        ws_dist();

        if (g_playerDisplacement < 3000 && eub(ENTITY, 0x86) != 8) {
            eub(ENTITY, 0x85) = 1;     // behaviour owns the spider now
            eub(ENTITY, 0x86) = 8;
            eub(ENTITY, 0x87) = 0;
        }
        if (PLAYER_DOWN) {
            eub(ENTITY, 0x85) = 1;
            eub(ENTITY, 0x86) = 8;
            eub(ENTITY, 0x87) = 0;
        }
    }

    if (WS_DELAY != 0) {
        WS_DELAY = (short)(WS_DELAY - 1);
    }
    if (WS_TRAIL == 0) {
        eub(ENTITY, 0x178) = 0;
        eub(ENTITY, 0x179) = 0;
        return;
    }
    WS_COUNT = (short)(WS_COUNT + 1);
}

// ============================================================================
// wsp_state2 @ 0x00478550 - the "build a web" wait, run every frame while the
// spider is idle-building; action_behavior 0 sets the animation and waits.
// ============================================================================
void wsp_state2(void)
{
    if (eub(ENTITY, 0x86) == 0) {
        WS_ANIM_ID = 1;
        ws_web_build();
    }
}

// ============================================================================
// wsp_state3 @ 0x00478570 - the web-shoot state. Entered by the damage system;
// maps hit_state to a behaviour and shoots. Behaviour 1/2/6/8/9 take the
// alternate web-shooter, everything else the normal one.
// ============================================================================
void wsp_state3(void)
{
    if (eub(ENTITY, 0x85) == 0) {
        eub(ENTITY, 0x86) = (unsigned char)(WS_HITSTATE >> 3);
        eub(ENTITY, 0x85) = 1;
        if (WS_SPLAT != 0) {
            eub(ENTITY, 0x87) = 3;
            eub(ENTITY, 0x86) = 2;
        }
        WS_SPLAT = 1;
    }

    switch (eub(ENTITY, 0x86)) {
    case 1:
    case 2:
    case 6:
    case 8:
    case 9:
        ws_web_shoot_b();
        return;
    default:
        WS_ANIM_ID = 1;
        ws_web_shoot_a();
        return;
    }
}

// ============================================================================
// wsp_state4 / wsp_state5 @ 0x00478620 / 0x00478630 - deliberate no-ops.
// ============================================================================
void wsp_state4(void) { }
void wsp_state5(void) { }

// ============================================================================
// ws_leg_reach @ 0x0047a380
// Measures how far the spider's leg reach is and stores it in move_speed_current
// (Entity+0xC2). Composes the leg-chain world matrices (an eight-joint stride,
// the two joints at index part*8+3 and part*8+4) and subtracts the leg tip's
// world position to get a reach vector, then returns its magnitude. If scale !=
// 0 the whole chain matrix is first scaled isotropically. Clears the speed (and
// returns) when the "should this leg exist" flag is not set.
// ============================================================================
void ws_leg_reach(unsigned char part, int scale)
{
    int joints = (int)(uintptr_t)ENTITY->jointsStructs;

    RotMatrix(reinterpret_cast<SVECTOR*>((char*)ENTITY + 0x72),
              reinterpret_cast<MATRIX*>((char*)ENTITY + 0x20));
    ApplyLVAndMul0Matrix((char*)ENTITY + 0x20, (void*)(joints + 0x24), &g_matrixScratch);

    if (scale != 0) {
        g_playerPosScratch.x = scale;
        g_playerPosScratch.y = scale;
        g_playerPosScratch.z = scale;
        ScaleMatrixCols(&g_matrixScratch, &g_playerPosScratch);
    }

    // 0x1f0 = joint 4 * 0x7c; 0x3e0 = an eight-joint stride. The legs are packed
    // eight joints apart, so `part` selects a leg pair.
    int tip = joints + (int)part * 0x3e0 + 0x1f0;

    if ((*(unsigned char*)(tip - 0x7c) & 1) == 0) {
        WS_SPEED = 0;
        return;
    }

    unsigned char b = 2;
    do {
        ApplyLVAndMulMatrix(&g_matrixScratch, (MATRIX*)(tip - (int)b * 0x7c + 0xa0));
        b--;
    } while (b != 0);

    g_matrixScratch.t[0] = g_matrixScratch.t[0] - *(int*)(tip + 0x58);
    g_matrixScratch.t[1] = 0;
    g_matrixScratch.t[2] = g_matrixScratch.t[2] - *(int*)(tip + 0x60);

    FUN_0040a380((VECTOR*)g_matrixScratch.t, &g_playerPosScratch);
    WS_SPEED = (short)SquareRoot0(g_playerPosScratch.z + g_playerPosScratch.x);
}

// ============================================================================
// Forward declarations (state functions above call these; all are defined
// below before the entry point).
// ============================================================================
void wsp_handlerA(void);
void wsp_handlerB(void);
void wsp_handlerC(void);
void ws_action_runner(void);          // 0x00478640
void ws_behaviour_idle(void);         // 0x00478be0
void ws_behaviour_walk_turn(void);    // 0x00478d90
void ws_behaviour_strafe(void);       // 0x00479030
void ws_behaviour_approach(void);     // 0x00478ed0
void ws_behaviour_lunge(void);        // 0x00479190
void ws_behaviour_spit(void);         // 0x00479590
void ws_behaviour_webdrop(void);      // 0x00479680
void ws_behaviour_ret(void);          // 0x00479820
void ws_behaviour_circle(void);       // 0x00479830
void ws_web_build(void);              // 0x004799d0
void ws_update_webs(char count);      // 0x0048a730
void ws_web_shoot_a(void);            // 0x00479c40
void ws_web_shoot_b(void);            // 0x00479f10

// ============================================================================
// ws_action_runner @ 0x00478640 - executes the behaviour chosen by the picker.
// First updates the pathfind/LOS flag, then dispatches on action_behavior.
// ============================================================================
void ws_action_runner(void)
{
    unsigned int path = entity_pathfind_update();
    if ((path & 0xFE) == 0) {
        WS_PATHW = (unsigned short)((WS_PATHW & 0xFFFE) | (path & 1));
    }

    switch (eub(ENTITY, 0x86)) {
    case 0:  ws_behaviour_idle();      return;
    case 1:  // walk toward, short turn
        WS_ANIM_ID = 3;
        WS_ANGLE = (short)(WS_ANGLE + (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x10) *
            (WS_PATHW & 1));
        ws_behaviour_walk_turn();
        WS_SPEED = (short)(WS_SPEED + 0x3C);
        Add_speedXZ(0);
        return;
    case 2:  ws_behaviour_strafe();    return;
    case 3:  // close in, fast
        WS_ANIM_ID = 2;
        WS_TURN = 0x80;
        ws_behaviour_approach();
        return;
    case 4:  // walk toward, wide turn
        WS_ANIM_ID = 2;
        WS_ANGLE = (short)(WS_ANGLE + (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x30));
        ws_behaviour_walk_turn();
        WS_SPEED = (short)(WS_SPEED + 100);
        Add_speedXZ(0);
        return;
    case 5:  // close in, slower
        WS_ANIM_ID = 3;
        WS_TURN = 0x20;
        ws_behaviour_approach();
        return;
    case 6:  ws_behaviour_lunge();     return;
    case 7:  ws_behaviour_spit();      return;
    case 8:  ws_behaviour_webdrop();   return;
    case 9:  ws_behaviour_ret();       return;
    case 10: ws_behaviour_circle();    return;
    case 11:  // walk toward, tiny turn
        WS_ANIM_ID = 3;
        WS_ANGLE = (short)(WS_ANGLE + (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x10));
        ws_behaviour_walk_turn();
        WS_SPEED = (short)(WS_SPEED + 0x14);
        Add_speedXZ(0);
        return;
    }
}

// ============================================================================
// wsp_handlerA @ 0x00478870 - behaviour picker for spawn kind 0. Chooses among
// the chase behaviours based on distance; also reacts to a knocked-down player.
// ============================================================================
void wsp_handlerA(void)
{
    if (g_playerDisplacement < 6000 && (WS_PATHW & 1) == 0) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 3;
    }
    if (WS_DELAY == 0) {
        if ((rand() & 0x1ff) == 0 && (WS_PATHW & 1) != 0) {
            eub(ENTITY, 0x85) = 1;
            euw(ENTITY, 0x86) = 6;
        }
        if (g_playerDisplacement < 7000) {
            if ((short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                                          0x80) == 0) {
                eub(ENTITY, 0x85) = 1;
                euw(ENTITY, 0x86) = 6 + (rand() & 1);
            }
        }
    }
    if (PLAYER_DOWN) {
        if ((rand() & 1) == 0) {
            eub(ENTITY, 0x85) = 1;
            euw(ENTITY, 0x86) = 10;
        }
    }
    if (0x1e < WS_COUNT) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 10;
    }
    if ((g_playerEntity.isBeingAttackedFlag & 0x80) != 0) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 0xB;
    }
}

// ============================================================================
// wsp_handlerB @ 0x004789b0 - behaviour picker for spawn kind 1. The "hunter"
// variant: probes with two turn steps, closes when facing the player, and uses
// the web-shot and the chase more aggressively than variant 0.
// ============================================================================
void wsp_handlerB(void)
{
    short turnSlow = (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x80);
    short turnFast = (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x100);

    if (PLAYER_DOWN && turnSlow != 0) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 3;
    }
    if (g_playerDisplacement < 8000 && turnSlow != 0) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 3;
    }
    if (WS_DELAY == 0) {
        if (g_playerDisplacement < 0x1d4c && (WS_PATHW & 1) != 0) {
            eub(ENTITY, 0x85) = 1;
            euw(ENTITY, 0x86) = 6 + (rand() & 1);
        }
        if (g_playerDisplacement < 6000 && turnFast == 0) {
            eub(ENTITY, 0x85) = 1;
            euw(ENTITY, 0x86) = 7;
        }
    }
    if (PLAYER_DOWN) {
        if ((rand() & 0xf) == 0) {
            eub(ENTITY, 0x85) = 1;
            euw(ENTITY, 0x86) = 10;
        }
    }
    if (0x1e < WS_COUNT) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 10;
    }
}

// ============================================================================
// wsp_handlerC @ 0x00478b20 - behaviour picker for the ceiling-hung spawn kind
// 2. Mostly chases or retreats; drops the web (behaviour 8) on a close player.
// ============================================================================
void wsp_handlerC(void)
{
    if (g_playerDisplacement < 3000) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 8;
        return;
    }
    if (PLAYER_DOWN) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 8;
        return;
    }
    if (5000 < g_playerDisplacement) {
        eub(ENTITY, 0x85) = 1;
        euw(ENTITY, 0x86) = 2;
        if (5000 < g_playerDisplacement) {
            short turn = (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                                                   0x80);
            if (turn == 0) {
                eub(ENTITY, 0x85) = 1;
                euw(ENTITY, 0x86) = 0xB;
            }
        }
    }
}

// ============================================================================
// ws_behaviour_idle @ 0x00478be0 - behaviour 0. Two bodies depending on the
// spawn kind: variant 0 uses the fast "look around" idle with a randomised
// dwell; variants 1/2 idle slower and never re-roll the behaviour here.
// ============================================================================
void ws_behaviour_idle(void)
{
    if (ENTITY->behavior_flags != 0) {
        // slow idle (0x00478d10)
        if (eub(ENTITY, 0x87) == 0) {
            eub(ENTITY, 0x87) = 1;
            eub(ENTITY, 0xbe) = 0;
            eub(ENTITY, 0xbf) = 0;
            eub(ENTITY, 0xbd) = 0;
            WS_BLEND = 3;
        } else if (eub(ENTITY, 0x87) != 1) {
            return;
        }
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    }

    // fast idle (0x00478c00)
    if (eub(ENTITY, 0x87) == 0) {
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 0;
        unsigned short los = WS_PATHW;
        WS_DWELL = (short)((1 - (los & 1)) * 0x50 + (rand() & 0x3f));
        WS_BLEND = 3;
    } else if (eub(ENTITY, 0x87) != 1) {
        return;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    short d = WS_DWELL;
    WS_DWELL = (short)(d - 1);
    if (d == 0) {
        eub(ENTITY, 0x85) = 0;
        unsigned short los = WS_PATHW;
        eub(ENTITY, 0x86) = ws_behavior_pick[(rand() & 7) + (los & 1) * 8];
        eub(ENTITY, 0x87) = 0;
    }
}

// ============================================================================
// ws_behaviour_walk_turn @ 0x00478d90 - behaviours 1/4/11 (turn toward and
// walk). Plays a scuttle SFX on certain animation frames, measures the leg
// reach (ws_leg_reach) and steps off the action when the dwell runs out.
// ============================================================================
void ws_behaviour_walk_turn(void)
{
    if (eub(ENTITY, 0x87) == 0) {
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_DWELL = (short)((rand() & 0x1f) * (unsigned char)eub(ENTITY, 0x86));
        if ((eub(ENTITY, 0x86) & 0xFE) == 0) {
            WS_DWELL = (short)(WS_DWELL + 0x50);
        }
    } else if (eub(ENTITY, 0x87) != 1) {
        return;
    }

    unsigned char sfx = ws_frame_sfx[eub(ENTITY, 0xbe)];
    if (sfx != 0) Snd_em(sfx);

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    unsigned char frame = eub(ENTITY, 0xbe);
    unsigned int  part  = 1;   // ground-reach leg pair
    if (frame == 0 || (8 < frame)) part = 0;   // the off-beat frames brace on the other pair
    ws_leg_reach((unsigned char)part, WS_TILT);

    short d = WS_DWELL;
    WS_DWELL = (short)(d - 1);
    if (d == 0) {
        eub(ENTITY, 0x85) = 0;
        euw(ENTITY, 0x86) = 0;
    }
}

// ============================================================================
// ws_behaviour_strafe @ 0x00479030 - behaviour 2. Side-steps at a fixed angle
// (0x18, or -0x18 when the dwell bit is odd), glancing at the player.
// ============================================================================
void ws_behaviour_strafe(void)
{
    if (eub(ENTITY, 0x87) == 0) {
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_ANIM_ID = 3;
        WS_DWELL = (short)(rand() & 0x3f);
        WS_BLEND = 3;
        WS_TURN = 0x18;
        if ((eub(ENTITY, 0xc4) & 1) != 0) {
            WS_TURN = (short)0xFFE8;
        }
    } else if (eub(ENTITY, 0x87) != 1) {
        return;
    }

    WS_ANGLE = (short)(WS_ANGLE + WS_TURN);
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    if ((WS_PATHW & 1) != 0) {
        if ((short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                                      WS_TURN) == 0) {
            eub(ENTITY, 0x86) = 4;
            eub(ENTITY, 0x87) = 0;
            return;
        }
    }

    short d = WS_DWELL;
    WS_DWELL = (short)(d - 1);
    if (d == 0) {
        eub(ENTITY, 0x85) = 0;
        eub(ENTITY, 0x86) = (unsigned char)((WS_PATHW & 1) << 2);
        eub(ENTITY, 0x87) = 0;
    }
}

// ============================================================================
// ws_behaviour_approach @ 0x00478ed0 - behaviours 3/5. Close the gap, holding
// the approach turn step (WS_TURN); when the dwell or the facing runs out the
// behaviour collapses to a shortcut chase (behaviour 11 when the LOS is clear).
// ============================================================================
void ws_behaviour_approach(void)
{
    char sub = eub(ENTITY, 0x87);
    if (sub == 0) {
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_HITSTATE = 0;
        eub(ENTITY, 0x87) = 1;
        WS_DWELL = (short)((rand() & 0x1f) + 0x50);
    } else if (sub != 1) {
        if (sub != 2) return;
        eub(ENTITY, 0x85) = 0;
        eub(ENTITY, 0x86) = (unsigned char)(((unsigned char)WS_PATHW & 1) * 0x0B);
        eub(ENTITY, 0x87) = 0;
        WS_WAY_X = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        WS_WAY_Z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        return;
    }

    // The original sign-extends the return through a short before the == 0 test,
    // so the "am I facing the player" check sees the TRUNCATED value.
    short turn = (short)turn_toward_target(
        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, WS_TURN);
    short d = WS_DWELL;
    WS_DWELL = (short)(d - 1);
    if (d == 0 || turn == 0) {
        eub(ENTITY, 0x87) = 2;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    WS_ANGLE = (short)(WS_ANGLE + turn);
}

// ============================================================================
// ws_behaviour_lunge @ 0x00479190 - behaviour 6, the pounce-and-chew. A long
// sub-state walk (wait -> rush -> bite -> shake -> recover) and the only
// behaviour that deals damage directly. Reads is_facing_toward_entity and wears
// the player's health down by 10 (or 18 on easy mode).
// ============================================================================
void ws_behaviour_lunge(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0:
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_ANIM_ID = 8;
        // fall through
    case 1:
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400));
        euw(ENTITY, 0xc2) = 100;
        Add_speedXZ(0);
        return;
    case 2:
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_ANIM_ID = 0xB;
        WS_DWELL = (short)((rand() & 0x1f) + 0x14);
        // fall through
    case 3:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        {
            int turn3 = (int)(short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x40);
            g_animFrameIdSave = (unsigned int)turn3;   // stored to the scratch global, as the original does
            WS_ANGLE = (short)(WS_ANGLE + (short)turn3);
            if (turn3 == 0) eub(ENTITY, 0x87) = 4;
        }
        {
            short d = WS_DWELL;
            WS_DWELL = (short)(d - 1);
            if (d == 0) { eub(ENTITY, 0x87) = 6; return; }
        }
        break;
    case 4:
        eub(ENTITY, 0x87) = 5;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_ANIM_ID = 0xB;
        WS_DWELL = (short)((rand() & 0x1f) + 10);
        // fall through
    case 5:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        WS_ANGLE = (short)(WS_ANGLE + (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x10));
        euw(ENTITY, 0xc2) = 300;
        Add_speedXZ(0);
        {
            short d5 = WS_DWELL;
            WS_DWELL = (short)(d5 - 1);
            if (d5 == 0 || (short)turn_toward_target(
                    (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200) != 0) {
                eub(ENTITY, 0x87) = 6;
            }
        }
        ws_dist();

        if (WS_TOUCH != 0 && g_playerEntity.isBeingAttackedFlag == 0) {
            unsigned int facing = is_facing_toward_entity(&g_playerEntity) & 0xFF;
            eub(ENTITY, 0x87) = 6;
            Snd_em(4);
            if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                g_playerEntity.health = (short)(g_playerEntity.health - 10);
            } else {
                g_playerEntity.health = (short)(g_playerEntity.health - 0x12);
            }
            if (g_playerEntity.health < 0) g_playerEntity.health = 1;
            g_playerEntity.isBeingAttackedFlag = (unsigned char)((int)facing + 1);
            g_playerEntity.action_behavior = (unsigned char)((int)facing + 0x66);
            return;
        }
        break;
    case 6:
        eub(ENTITY, 0x87) = 7;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_ANIM_ID = 9;
        euw(ENTITY, 0xc4) = 7;
        // fall through
    case 7:
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400));
        euw(ENTITY, 0xc2) = 100;
        Add_speedXZ(0);
        break;
    case 8:
        eub(ENTITY, 0x85) = 0;
        euw(ENTITY, 0x86) = 0;
        euw(ENTITY, 0x172) = 0xF;
        return;
    }
}

// ============================================================================
// ws_behaviour_spit @ 0x00479590 - behaviour 7. Rears back and spits a glob of
// web at the player; when the sub-state trips, drops back to a chase.
// ============================================================================
void ws_behaviour_spit(void)
{
    char sub = eub(ENTITY, 0x87);
    if (sub == 0) {
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_ANIM_ID = 0xC;
        g_playerPosScratch.x = 1000;
        g_playerPosScratch.y = -600;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0x1E, 0, 0, (void*)((char*)ENTITY + 0x20), &g_playerPosScratch, 0);
        Snd_em(7);
    } else if (sub != 1) {
        if (sub != 2) return;
        euw(ENTITY, 0x84) = 1;         // WORD: state -> run AND 0x85 (the picker
                                       // latch) -> 0, as ws_web_build also does.
        euw(ENTITY, 0x86) = 0xB;
        euw(ENTITY, 0x172) = 0xF;
        return;
    }
    eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x400));
}

// ============================================================================
// ws_behaviour_webdrop @ 0x00479680 - behaviour 8. The ceiling drop: the spider
// swings down on its thread (world Y spins up by a squared ramp against a
// thread counter), then dangles and drops the web profile.
// ============================================================================
void ws_behaviour_webdrop(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0:
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_ANIM_ID = 0;
        euw(ENTITY, 0xc4) = 0;
        // fall through
    case 1:
        WS_Y = WS_Y + (int)(short)WS_DWELL * (int)(short)WS_DWELL * 8;
        WS_DWELL = (short)(WS_DWELL + 1);
        ew(ENTITY, 0x72) = (short)(ew(ENTITY, 0x72) - 0x100);   // pitch, not WS_TILT
        if ((ew(ENTITY, 0x72) & 0x8000) != 0) ew(ENTITY, 0x72) = 0;
        if ((*(unsigned char*)((char*)ENTITY + 0x3B) & 0x80) == 0) {
            eu(ENTITY, 0x38) = 0;
            euw(ENTITY, 0x72) = 0;
            eub(ENTITY, 0x87) = 2;
        }
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    case 2:
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        WS_ANIM_ID = 7;
        Snd_em(3);
        ENTITY->behavior_flags = (unsigned char)(ENTITY->behavior_flags & 0xFD);
        // fall through
    case 3:
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400));
        return;
    case 4:
        eub(ENTITY, 0x85) = 0;
        euw(ENTITY, 0x86) = 0;
        return;
    }
}

// ============================================================================
// ws_behaviour_ret @ 0x00479820 - behaviour 9. A deliberate no-op slot.
// ============================================================================
void ws_behaviour_ret(void) { }

// ============================================================================
// ws_behaviour_circle @ 0x00479830 - behaviour 10. Circles the player at speed,
// correcting the heading each frame and chaining the world position of a leg.
// ============================================================================
void ws_behaviour_circle(void)
{
    char sub = eub(ENTITY, 0x87);
    if (sub == 0) {
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        eub(ENTITY, 0x87) = 1;
        WS_ANIM_ID = 6;
        short base = (short)((rand() & 1) * -0x658 + 0x32c);
        WS_TURN = (short)(base + (unsigned short)((WS_PATHW & 1) == 0) * (rand() & 0xFFF));
        WS_SPEED = (short)((rand() & 0x3f) + 200);
        Add_speedXZ(WS_TURN);
    } else if (sub != 1) {
        if (sub != 2) return;
        eub(ENTITY, 0x8a) = 0;
        eub(ENTITY, 0x85) = 0;
        eub(ENTITY, 0x86) = (unsigned char)(((unsigned char)WS_PATHW & 1) << 2);
        eub(ENTITY, 0x87) = 0;
        euw(ENTITY, 0x178) = 0;
        return;
    }

    eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x400));
    WS_ANGLE = (short)(WS_ANGLE + (short)turn_toward_target(
        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x40));
    WS_X = WS_X + (int)(short)ew(ENTITY, 0x78);
    WS_Y = WS_Y + (int)(short)ew(ENTITY, 0x7a);
    WS_Z = WS_Z + (int)(short)ew(ENTITY, 0x7c);
}

// ============================================================================
// ws_web_build @ 0x004799d0 - the idle "spin a web" behaviour (state 2). Walks
// down the web-joint registry (8 odd leg/body-part indices) and attaches a
// fresh web thread to the next unused one, spawning two collision joints and a
// pair of billboards per thread. When the sub-state trips, it hands off to a
// chase behaviour (3, or 10 if the LOS is clear).
//
// NOTE: the original writes the registry at index WEBIDX (Entity+0x17A) with no
// bound, so past the eighth web it ran off the end of the 8-byte registry into
// the neighbouring table. The port masks the index to 8 slots (a ring) so the
// registry stays in-bounds while keeping the "find the next free leg" intent.
// ============================================================================
void ws_web_build(void)
{
    char sub = eub(ENTITY, 0x87);
    if (sub == 0) {
        eub(ENTITY, 0x87) = 1;
        euw(ENTITY, 0xc2) = 0;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        Snd_em(4);

        if ((WS_HITSTATE & 1) == 0) {
            unsigned int pick = (unsigned int)rand() & 7;
            unsigned char chosen = ws_web_joints[pick];
            g_entity_bkp = 0;
            int tvar = 7;
            int i;
            do {
                i = tvar;
                if (ws_web_joints_used[tvar] == chosen) g_entity_bkp = 1;
                tvar--;
            } while (i != 0);

            if (g_entity_bkp == 0) {
                int joints = (int)(uintptr_t)ENTITY->jointsStructs;
                ws_web_joints_used[WS_WEBIDX & 7] = chosen;
                joint_setup_attack_effect(joints + (unsigned int)chosen * 0x7c, 0x1e, 0x14, 3);
                joint_setup_attack_effect(joints +
                    (unsigned int)(unsigned char)ws_web_joints[pick] * 0x7c + 0x7c, 0x1e, 0x14, 3);
                g_playerPosScratch.x = 0;
                g_playerPosScratch.y = 0;
                g_playerPosScratch.z = 0;
                Effect_CreateBillboard(0, 8, 0,
                    (void*)(joints + (unsigned int)(unsigned char)ws_web_joints[pick] * 0x7c + 0xc0),
                    &g_playerPosScratch, 0);
                Effect_CreateBillboard(0, 8, 0,
                    (void*)(joints + (unsigned int)(unsigned char)ws_web_joints[pick] * 0x7c + 0x44),
                    &g_playerPosScratch, 0);
                Snd_em(5);
                WS_WEBIDX = (short)(WS_WEBIDX + 1);
            }
        }
    } else if (sub != 1) {
        if (sub != 2) return;
        euw(ENTITY, 0x84) = 1;                              // state -> run
        eub(ENTITY, 0x86) = (unsigned char)((ENTITY->behavior_flags & 1) + 3);
        eub(ENTITY, 0x87) = 0;
        WS_WAY_X = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        WS_WAY_Z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        eub(ENTITY, 0x8a) = 0;
        if ((WS_PATHW & 1) == 0) return;                    // LOS clear -> keep steering
        eub(ENTITY, 0x85) = 1;
        eub(ENTITY, 0x86) = 10;
        return;
    }
    eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
        0, ENTITY->animHeader, ENTITY->animBase, 0x400));
}

// ============================================================================
// ws_web_shoot_a @ 0x00479c40 - the normal web-shooter (state 3, behaviours not
// 1/2/6/8/9). Case 0 primes the fangs; case 2/3 animate the spit, swap the SCA
// to the small profile and clone the web threads (count = hit_state>>2&0xFE+8).
// Case 6 keeps the cloned threads flying via ws_update_webs.
// ============================================================================
void ws_web_shoot_a(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0: {
        euw(ENTITY, 0xc2) = 0;
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        euw(ENTITY, 0xc4) = 0x1e;
        WS_BLEND = 3;
        int joints = (int)(uintptr_t)ENTITY->jointsStructs;
        joint_setup_attack_effect(joints, 0x1e, 0x1e, 3);
        joint_setup_attack_effect(joints + 0x934, 0x1e, 0x1e, 3);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x978), &g_playerPosScratch, 0);
        Flg_on((int)g_EnemiesFlags, WS_DEATH_EV);
        Snd_em(5);
        unsigned int j = 8;
        do {
            unsigned char* p = (unsigned char*)(joints + 0x7c + j * 0xf8);
            unsigned char v = *p;
            if (v != 0 && (v & 0x20) == 0) {
                *p = (unsigned char)(v | 0xc);
                *(unsigned char*)(joints + 0xf8 + j * 0xf8) |= 0x10;
            }
            j--;
        } while (j != 0);
        // fall through
    }
    case 1:
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400));
        return;
    case 2:
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00df809f);
        BillboardAdjSize(&ENTITY->pushVelocity, -100, -100);
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0x00) |= 2;                            // status_flags
        eub(ENTITY, 0x00) |= 8;
        euw(ENTITY, 0xc2) = 0;
        euw(ENTITY, 0xc4) = 0x5a;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)ws_sca_info_small;
        // fall through
    case 3:
        BillboardAdjSize(&ENTITY->pushVelocity, 3, 3);
        WS_DWELL = (short)(WS_DWELL - 1);
        if (WS_DWELL == 0) {
            eub(ENTITY, 0x87) = 6;
            ws_clone_entity((unsigned char)((WS_HITSTATE >> 2 & 0xFE) + 8), 0xf2,
                            ENTITY->jointCount, (unsigned int*)&ENTITY->scd_target_ptr);
        }
        return;
    default:
        return;
    case 6:
        Flg_on((int)g_EnemiesFlags, WS_DEATH_EV);
        ws_update_webs((char)((WS_HITSTATE >> 2 & 0xFE) + 8));
        return;
    }
}

// ============================================================================
// ws_web_shoot_b @ 0x00479f10 - the alternate web-shooter (state 3, behaviours
// 1/2/6/8/9). Nearly identical to shoot_a, but maps hit_state onto a different
// action and a lower clone count (`+ 4`). The bit-1 hit_state injects an extra
// "shoot again" that forces behaviour 3 / sub-state 2; bit 0 skips to the 4-5
// chain that dangles the body on the thread.
// ============================================================================
void ws_web_shoot_b(void)
{
    unsigned char sub = eub(ENTITY, 0x87);
    if (sub == 1 || sub == 3) {
        if ((WS_HITSTATE & 2) != 0) {
            int joints = (int)(uintptr_t)ENTITY->jointsStructs;
            joint_setup_attack_effect(joints, 0x1e, 0x1e, 3);
            joint_setup_attack_effect(joints + 0x934, 0x1e, 0x1e, 3);
            g_playerPosScratch.x = 0;
            g_playerPosScratch.y = 0;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 8, 0, (void*)(joints + 0x978), &g_playerPosScratch, 0);
            g_animFrameIdSave = 8;
            do {
                unsigned char* p = (unsigned char*)(joints + 0x7c + g_animFrameIdSave * 0xf8);
                unsigned char v = *p;
                if (v != 0 && (v & 0x20) == 0) {
                    *p = (unsigned char)(v | 0xc);
                    *(unsigned char*)(joints + 0xf8 + g_animFrameIdSave * 0xf8) |= 0x10;
                }
                g_animFrameIdSave--;
            } while (g_animFrameIdSave != 0);
            eub(ENTITY, 0x86) = 3;
            eub(ENTITY, 0x87) = 2;
            return;
        }
        if ((WS_HITSTATE & 1) != 0) {
            eub(ENTITY, 0x87) = 4;
        }
    }

    switch (eub(ENTITY, 0x87)) {
    case 0: {
        euw(ENTITY, 0xc2) = 0;
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 3;
        eub(ENTITY, 0x8c) = 3;
        eub(ENTITY, 0x8a) = 0;
        eub(ENTITY, 0xbd) = 4;
        Flg_on((int)g_EnemiesFlags, WS_DEATH_EV);
        // fall through
    }
    case 1:
        eub(ENTITY, 0x00) &= 0x1f;                         // status_flags
        if (0xd < eub(ENTITY, 0xbe)) eub(ENTITY, 0x00) |= 0x20;
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + (unsigned char)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400));
        return;
    case 2:
        eub(ENTITY, 0x00) |= 2;
        eub(ENTITY, 0x00) |= 8;
        eub(ENTITY, 0xbd) = 5;
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0x8a) = 0;
        eub(ENTITY, 0xf0) = 0x9f; eub(ENTITY, 0xf1) = 0x80;
        eub(ENTITY, 0x118) = 0x9f; eub(ENTITY, 0x119) = 0x80;
        eub(ENTITY, 0xf2) = 0xdf;
        eub(ENTITY, 0x11a) = 0xdf;
        // fall through
    case 3:
        eub(ENTITY, 0x00) &= 0x1f;
        eub(ENTITY, 0x00) |= 0x20;
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;
    case 4:
        eub(ENTITY, 0xbd) = 5;
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        WS_BLEND = 1;
        Snd_em(5);
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00df809f);
        BillboardAdjSize(&ENTITY->pushVelocity, -100, -100);
        eub(ENTITY, 0x87) = 5;
        euw(ENTITY, 0xc4) = 0x78;
        euw(ENTITY, 0xc2) = 0;
        eub(ENTITY, 0x00) |= 2;
        eub(ENTITY, 0x00) |= 8;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)ws_sca_info_small;
        goto ws_shoot_b_dangle;
    case 5:
        goto ws_shoot_b_dangle;
    case 6:
        Flg_on((int)g_EnemiesFlags, WS_DEATH_EV);
        ws_update_webs((char)((WS_HITSTATE >> 2 & 0xFE) + 4));
        return;
    default:
        return;
    }

ws_shoot_b_dangle:
    BillboardAdjSize(&ENTITY->pushVelocity, 3, 3);
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    WS_DWELL = (short)(WS_DWELL - 1);
    if (WS_DWELL != 0) return;
    eub(ENTITY, 0x87) = 6;
    ws_clone_entity((unsigned char)((WS_HITSTATE >> 2 & 0xFE) + 4), 0xf2,
                    ENTITY->jointCount, (unsigned int*)&ENTITY->scd_target_ptr);
}

// ============================================================================
// ws_update_webs @ 0x0048a730 - per-frame update of the cloned web threads. The
// clone chain hangs off the parent's scd_target_ptr (0xB8). Each thread is a
// full 0x18C Entity clone that either hangs on a thread, flies at the player, or
// (on contact) sticks and drains the player's health. The global ENTITY is
// swapped to walk the chain, exactly like the original.
// ============================================================================
void ws_update_webs(char count)
{
    Entity* saved = ENTITY;
    Entity* web = reinterpret_cast<Entity*>(ENTITY->scd_target_ptr);
    ENTITY = web;

    do {
        eub(ENTITY, 0x03) = (unsigned char)is_entity_in_switch_zone(
            (VECTOR*)((char*)ENTITY + 0x34), g_CurrentRdtDataTypePtr);

        if (eub(ENTITY, 0x00) == 0) {                 // status_flags == 0 -> thread spent
            if (eub(ENTITY, 0x03) != 0) {
                entity_add_fade_sprite((VECTOR*)((char*)ENTITY + 0x34),
                    (short*)((char*)ENTITY + 0xe4), 0, ew(ENTITY, 0x74));
            }
        } else if ((g_message_flags & 4) != 0) {
            ws_dist();

            if (ew(ENTITY, 0x174) != 0) ew(ENTITY, 0x174) -= 1;

            // Reached the player: the thread sticks, drops a shadow and a sticky
            // billboard, and leaves the web to dangle (state 6).
            if (g_playerDisplacement < 400 && g_playerEntity.move_speed_current != 0 &&
                eub(ENTITY, 0x84) != 5) {
                eub(ENTITY, 0x00) = 0;
                g_svecScratch.z = 0;
                g_svecScratch.y = 0;
                g_svecScratch.x = 0;
                g_animFrameIdSave = 0xff55df;
                FUN_004565f0(&g_svecScratch, (SVECTOR*)((char*)ENTITY + 0xe4), 300, 300);
                g_playerPosScratch.x = 0;
                g_playerPosScratch.y = 0;
                g_playerPosScratch.z = 0;
                Effect_CreateBillboard(0, 10, 0, (void*)((char*)ENTITY + 0x20),
                                       &g_playerPosScratch, 0);
                if (ew(ENTITY, 0x174) == 0) {
                    Snd_em(6);
                    ew(ENTITY, 0x174) = (short)-0x2d;
                }
                eub(ENTITY, 0x84) = 6;
            }

            switch (eub(ENTITY, 0x84)) {
            case 0: {
                euw(ENTITY, 0xc4) = (unsigned short)(rand() & 0x3f);
                int d = (int)(rand() & 0x3f);
                euw(ENTITY, 0x16c) = (unsigned short)(-((int)(rand() & 1)) * d);
                eub(ENTITY, 0x84) = 1;
                euw(ENTITY, 0xc2) = (unsigned short)((rand() & 0x1f) + 100);
                eub(ENTITY, 0x172) = (unsigned char)-0x80;   // swing seed, byte store
                eub(ENTITY, 0x173) = 0;
                // fall through to the swing
            }
            case 1: {
                int turn = (int)(short)turn_toward_target(
                    (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x3e);
                if (g_playerEntity.move_speed_current != 0 && eub(ENTITY, 0x87) != 0) {
                    turn = -turn;
                }
                WS_ANGLE = (short)(WS_ANGLE + ew(ENTITY, 0x16c) + (short)turn);
                WS_ANGLE = (short)(WS_ANGLE + ew(ENTITY, 0x172));
                ew(ENTITY, 0x172) = (short)-ew(ENTITY, 0x172);
                short s = ew(ENTITY, 0xc4);
                ew(ENTITY, 0xc4) = (short)(s - 1);
                if (s == 0) {
                    eub(ENTITY, 0x84) = (unsigned char)((turn == 0) ? 4 : 2);
                }
                break;
            }
            case 2:
                euw(ENTITY, 0xc4) = (unsigned short)(rand() & 0x1f);
                eub(ENTITY, 0x84) = 3;
                euw(ENTITY, 0xc2) = 0;
                euw(ENTITY, 0x172) = 0;
                // fall through
            case 3: {
                short s = ew(ENTITY, 0xc4);
                ew(ENTITY, 0xc4) = (short)(s - 1);
                if (s == 0) eub(ENTITY, 0x84) = 0;
                break;
            }
            case 4:
                euw(ENTITY, 0xc2) = (unsigned short)((rand() & 0x1f) + 0x8c);
                eub(ENTITY, 0x84) = 5;
                eub(ENTITY, 0xbc) = 0;
                euw(ENTITY, 0x16c) = (unsigned short)((rand() & 0x7f) + 100);
                euw(ENTITY, 0x170) = 0;
                euw(ENTITY, 0x172) = 0;
                // fall through
            case 5: {
                unsigned int arc = entity_ballistic_step((short)ew(ENTITY, 0xc2),
                                                         (short)ew(ENTITY, 0x16c),
                                                         -30, 0);
                if (arc != 0) eub(ENTITY, 0x84) = 2;

                if (g_playerDisplacement < 800 && ew(ENTITY, 0x170) == 0) {
                    WS_ANGLE = (short)(WS_ANGLE + 0x400);
                    euw(ENTITY, 0x170) = 1;
                    g_playerPosScratch.x = 0;
                    g_playerPosScratch.y = 0;
                    g_playerPosScratch.z = 0;
                    Effect_CreateBillboard(0, 0, 0, (void*)((char*)ENTITY + 0x20),
                                           &g_playerPosScratch, 0);
                    ew(ENTITY, 0xc2) = (short)(ew(ENTITY, 0xc2) - 0x3c);
                    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                        g_playerEntity.health = (short)(g_playerEntity.health - 2);
                    } else {
                        g_playerEntity.health = (short)(g_playerEntity.health - 3);
                    }
                    if (g_playerEntity.health < 0) g_playerEntity.health = 1;
                    g_playerEntity.action_behavior = 100;
                    g_playerEntity.isBeingAttackedFlag = 1;
                }
                break;
            }
            }

            RotMatrix(reinterpret_cast<SVECTOR*>((char*)ENTITY + 0x72),
                      reinterpret_cast<MATRIX*>((char*)ENTITY + 0x20));
            if (eub(ENTITY, 0x84) < 6) {
                Add_speedXZ(ew(ENTITY, 0x172));
            }
        }

        check_room_collision((VECTOR*)((char*)ENTITY + 0x34),
                             *(short*)(*(int*)((char*)ENTITY + 4) + 10));
        ew(ENTITY, 0x6c) = (short)(*(int*)((char*)ENTITY + 0x34));
        ew(ENTITY, 0x6e) = (short)(*(int*)((char*)ENTITY + 0x38));
        ew(ENTITY, 0x70) = (short)(*(int*)((char*)ENTITY + 0x3c));
        ApplyLVAndMul0Matrix(&g_RoomCameraData, (char*)ENTITY + 0x20, &g_matrixScratch);
        g_entityJointPosX = *(int*)((char*)ENTITY + 0x14);
        SetRotAndTransMatrix(&g_matrixScratch);
        if (eub(ENTITY, 0x03) != 0) {
            FUN_00483250(0, 0, 0, *(int*)((char*)ENTITY + 0x18), 0, 4,
                (void*)((char*)&g_spriteAnimSlots[2] + (unsigned int)g_spriteAnimActive * 0x14));
        }

        ENTITY = (Entity*)((char*)ENTITY + 0x18c);
        count--;
    } while (count != 0);

    ENTITY = saved;
}

// ============================================================================
// wsp_state_table @ 0x004c4708 - six states, then the three behaviour pickers
// (states 6-9). The picker dispatch aliases this memory from +6, exactly as the
// original's FUN_00478820 indexes base 0x004c4720 by behavior_flags.
// ============================================================================
void (*wsp_state_table[10])(void) = {
    wsp_state0, wsp_state1, wsp_state2, wsp_state3, wsp_state4, wsp_state5,
    wsp_handlerA, wsp_handlerB, wsp_handlerC, wsp_handlerC
};
void (**wsp_handler_table)(void) = &wsp_state_table[6];

// ============================================================================
// web_spinner_update @ 0x00478310 - the per-frame entry, dispatch table [3].
// The SCA-collision tail runs after the state function (except state 5 skips
// it), then state 3 with action_state 6 runs the web-shooter a second time when
// the message flag is clear - the "shoot twice in one frame" quirk.
// ============================================================================
void web_spinner_update(void)
{
    if ((g_message_flags & 4) != 0) {
        wsp_state_table[eub(ENTITY, 0x84)]();
        if (eub(ENTITY, 0x84) != 5) {
            SetEntityScaHitData(ENTITY);
            euw(ENTITY, 0x170) = (unsigned short)ResolveEntityScaCollision((Entity*)&g_playerEntity, ENTITY);
            HandleEnemyPlayerCollisions();
            euw(ENTITY, 0x176) = (unsigned short)check_room_collision(
                (VECTOR*)((char*)ENTITY + 0x34), *(short*)(*(int*)((char*)ENTITY + 4) + 10));
        }
        if ((g_message_flags & 4) != 0) goto wsp_switch_zone;
        if (eub(ENTITY, 0x84) == 3 && eub(ENTITY, 0x87) == 6) {
            wsp_state_table[3]();
        }
    } else if (eub(ENTITY, 0x84) == 3 && eub(ENTITY, 0x87) == 6) {
        wsp_state_table[3]();
    }

wsp_switch_zone:
    eub(ENTITY, 0x03) = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)((char*)ENTITY + 0x34), g_CurrentRdtDataTypePtr);
    if (eub(ENTITY, 0x03) != 0 && (ENTITY->behavior_flags & 2) == 0) {
        entity_add_fade_sprite((VECTOR*)((char*)ENTITY + 0x34),
            (short*)((char*)ENTITY + 0xe4), 0, ew(ENTITY, 0x74));
    }
}
