// Cerberus.cpp - Cerberus (zombie dog, entity type 2, em1002.emd)
//
// Original PC addresses:
//   cerberus_update            0x00497fb0   per-frame entry (dispatch table [2])
//   state / behaviour block    0x004d4810   see the note on the table below
//   SCA record                 0x004d47e8
//
// The dog is the only monster besides the zombie with a full two-level AI: the
// STATE (Entity+0x84) picks init / behaviour-driver / hit-reaction / death, and
// while the driver runs, the BEHAVIOUR (Entity+0x85) picks patrol, chase, leap,
// strafe, alert, maul, bite, SCD-idle or stalk. Each behaviour then runs its own
// little sub-state machine off Entity+0x86 (action_behavior).
//
// What makes the dog different from the zombie is that almost all of its
// steering is done by PROBING: cerberus_probe_ahead (0x0049b1a0) advances the
// entity 500 units along its facing, asks check_room_collision, and rolls the
// move back. cerberus_probe_turn wraps that with a temporary yaw and speed
// scale, so the AI can ask "would I hit a wall if I turned left at half speed?"
// before committing. The answers accumulate in the flag word at Entity+0x178.
//
// Field aliases used throughout. The port's Entity struct names these bytes
// from the zombie's point of view; the dog reuses them for other things, and
// several are 16-bit accesses that span two of the port's byte fields - so they
// are reached by offset, per the "offset writes, not nearest field" rule:
//
//   +0x16C dword  distance to the player (manhattan |dx|+|dz|)
//   +0x170 short  turn step per frame
//   +0x172 short  signed turn currently being applied
//   +0x178 short  collision-probe history: bit 0 this frame, bits 1-3 the three
//                 frames before it, bit 4 "position did not change"
//   +0x17A byte   entity_pathfind_update result
//   +0x17C short  aggro / phase counter
//   +0x17E short  phase timer
//   +0x180 short  swerve angle (also the head-track yaw)
//   +0x182 short  blood billboards still owed
//   +0x184 byte   alert latch, bit 7 = "already reacted to this hit"
//   +0x186 short  behaviour flags (bit 0 run, bit 1 hunting, bit 2 gave up)
//   +0x188 short  AI flags (see CB_AI_* below)
//   +0x18A short  re-target pause
//
// All original addresses from Ghidra.
#include "EntityCommon.h"
#include "../../Globals.h"
#include <cstdlib>

extern void ResetJointTransforms(void);                       // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);      // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90 - Room.cpp
// 0x0043d8a0. Named for the zombie because that is where it was first found,
// but it is entity-generic: a heavy weapon cancels the flinch every fifth frame.
// cerberus_damaged reaches it as a TAIL JMP at 0x0049a92e.
extern void zombie_check_special_weapon(void);                // 0x0043d8a0

// ============================================================================
// Offset accessors. Every one of these is a width the original actually uses -
// see the header comment for why they are not plain struct fields.
// ============================================================================
#define CB_DIST        (*(int*)   ((char*)ENTITY + 0x16C))
// +0x174 is a WORD here (the port's splatter_flag + bob_speed): it is the
// initial vertical velocity handed to entity_ballistic_step. Read as a byte it
// would be 0x2C instead of 300, 0xE0 instead of 480 and 0x90 instead of 400,
// and every leap would barely leave the floor.
#define CB_LAUNCH_VY   (*(short*) ((char*)ENTITY + 0x174))
#define CB_TURN_DELTA  (*(short*) ((char*)ENTITY + 0x170))
#define CB_TURN_STEP   (*(short*) ((char*)ENTITY + 0x172))
#define CB_PROBE       (*(short*) ((char*)ENTITY + 0x178))
#define CB_PATH        (*(unsigned char*)((char*)ENTITY + 0x17A))
#define CB_AGGRO       (*(short*) ((char*)ENTITY + 0x17C))
#define CB_PHASE       (*(short*) ((char*)ENTITY + 0x17E))
#define CB_SWERVE      (*(short*) ((char*)ENTITY + 0x180))
#define CB_BLOOD       (*(short*) ((char*)ENTITY + 0x182))
#define CB_ALERT       (*(unsigned char*)((char*)ENTITY + 0x184))
#define CB_BEHFLAGS    (*(short*) ((char*)ENTITY + 0x186))
#define CB_BEHFLAGS_B  (*(unsigned char*)((char*)ENTITY + 0x186))
#define CB_AIFLAGS     (*(short*) ((char*)ENTITY + 0x188))
#define CB_REPAUSE     (*(short*) ((char*)ENTITY + 0x18A))
// action_behavior + action_state as one word: the original stores 0/1/2 into
// [entity+0x86] as a WORD, which clears action_state at the same time.
#define CB_SUBSTATE_W  (*(short*) ((char*)ENTITY + 0x86))
// Entity+0xBC is `death_timer` in the port's struct. The dog uses it as the
// ballistic tick counter for entity_ballistic_step - the number of frames it
// has been airborne, which scales gravity.
#define CB_AIRTICKS    (ENTITY->death_timer)

// The player's animationId/animFrameId/action_behavior/action_state read as one
// dword, exactly as the original does (`CMP dword ptr [0x00be6368],0x140301`).
#define PLAYER_ANIM_WORD (*(unsigned int*)((char*)&g_playerEntity + 0x84))
// 0x00140301 / 0x03140301 - the two player poses the dog cannot reach (the
// scripted climb). In either of them it drops whatever it was doing and falls
// back to the plain chase.
#define PLAYER_UNREACHABLE_A 0x00140301u
#define PLAYER_UNREACHABLE_B 0x03140301u

// CB_AIFLAGS bits (Entity+0x188)
#define CB_AI_REPROBE     0x0001  // recompute the swerve angle next frame
#define CB_AI_LOCKED_ON   0x0002  // facing the player within one turn step
#define CB_AI_SKID        0x0004  // overshot: run the skid-turn sub-state
#define CB_AI_SKID2       0x0008
#define CB_AI_WALL        0x0010  // wall-follow gave up / hit a dead end
#define CB_AI_HURT        0x0020  // hit reaction owns the entity this frame
#define CB_AI_ACTIVE      0x0080  // the dog has been woken up

namespace {

// ---------------------------------------------------------------------------
// 0x004d47e8 - the SCA collision record. Same six-short layout as the zombie's
// (see Zombie.cpp): [0] id/terminator, [1..3] local x/y/z, [4] half-height,
// [5] radius. Two of these shorts are read from OUTSIDE SetEntityScaHitData:
//
//   0x004d47ea (= [1], 500) is the forward probe distance in
//     cerberus_probe_ahead - the original reads it as `word ptr [0x004d47ea]`,
//     i.e. straight out of the middle of this record, not as a separate
//     constant.
//   0x004d47f2 (= [5], 400) is the collision radius check_room_collision reads
//     back out of Entity->Sca_info + 10.
// ---------------------------------------------------------------------------
const short cerberus_sca_info[8] = {
    (short)0x8000, 500, (short)-800, 0, 800, 400, 0, 0
};

// 0x004d47f8 - health base, index = rand() & 0xF. cerberus_init then ADDS
// rand() & 3, so a dog spawns with 59..122 HP.
const unsigned char cerberus_health_tbl[16] = {
    119, 99, 119, 99, 119, 99, 119, 99,
     99, 99,  59, 99,  59, 99,  59, 99
};

// 0x004d4918 - extra yaw sprinkled onto the hit-recoil direction,
// index = rand() & 7.
const short cerberus_recoil_angle_tbl[8] = {
    128, -192, -128, 192, -256, 256, 256, -256
};

// 0x004d4938..0x004d493a, addressed from 0x004d4939 with an index of -1/0/+1.
// cerberus_set_turn_anim picks the animation by the SIGN of the turn:
// 15 = lean left, 2 = straight, 16 = lean right.
const unsigned char cerberus_turn_anim_tbl[3] = { 15, 2, 16 };
// The original's base pointer. Keeping the -1..+1 indexing honest rather than
// rewriting it as [sign + 1] keeps this readable against the disassembly.
const unsigned char* const cerberus_turn_anim_base = &cerberus_turn_anim_tbl[1];

// 0x004d4888 - the walk/run parameter rows. The original indexes them with
// `(behavior_flags & ~1) * 2`, which is a BYTE offset, so it is really
// row (behavior_flags >> 1) of a 4-byte struct.
struct CerberusWalkParams {
    unsigned char  animId;
    unsigned char  pad;
    short          speed;
};
const CerberusWalkParams cerberus_walk_params[2] = {
    {  1, 0,  40 },   // 0x004d4888 - walk
    { 14, 0, 130 },   // 0x004d488c - run
};

// 0x004d4808 - "a dog has already given its approach bark" latch. It is a
// FILE-SCOPE global in the original, not a per-entity field: every cerberus in
// the room shares it, and cerberus_init resets it to 0.
int g_cerberusBarked = 0;

// ---------------------------------------------------------------------------
// Forward declarations - the dispatch tables below need them.
// ---------------------------------------------------------------------------
void cerberus_init(void);            // 0x004980f0
void cerberus_behavior_run(void);    // 0x00498310
void cerberus_damaged(void);         // 0x0049a400
void cerberus_die(void);             // 0x0049a940
void cerberus_no_action(void);       // 0x0049ab90
void cerberus_beh_none(void);        // 0x0049aba0
void cerberus_beh_patrol(void);      // 0x00499ae0
void cerberus_beh_chase(void);       // 0x00498920
void cerberus_beh_leap(void);        // 0x00498e20
void cerberus_beh_strafe(void);      // 0x004993a0
void cerberus_beh_alert(void);       // 0x00499640
void cerberus_beh_maul(void);        // 0x00499780
void cerberus_beh_bite(void);        // 0x004999d0
void cerberus_beh_scd(void);         // 0x00499ab0
void cerberus_beh_stalk(void);       // 0x00499d90

// ---------------------------------------------------------------------------
// 0x004d4810 - ONE pointer block read through TWO bases, the same trap the
// zombie's table documents:
//
//   0x004d4810  cerberus_states_table   indexed by Entity+0x84 (state)
//   0x004d4824  = table + 5             indexed by Entity+0x85 (behaviour)
//
// cerberus_update calls `[ECX*4 + 0x4d4810]` (0x00497fd3) and
// cerberus_behavior_run calls `[ECX*4 + 0x4d4824]` (0x004983c2), so
// behaviour[n] IS state[n + 5]. Entry [15] is the NULL terminator; behaviour
// index 10 would land on it, and nothing ever produces a 10.
// ---------------------------------------------------------------------------
void* const cerberus_states_table[16] = {
    (void*)cerberus_init,          // [0]  one-time init
    (void*)cerberus_behavior_run,  // [1]  behaviour driver
    (void*)cerberus_damaged,       // [2]  hit reaction
    (void*)cerberus_die,           // [3]  death sequence
    (void*)cerberus_no_action,     // [4]  0x0049ab90 - a bare RET
    // ---- from here the behaviour view aliases the same entries ----
    (void*)cerberus_beh_none,      // [5]  = behaviour[0] - 0x0049aba0, a bare RET
    (void*)cerberus_beh_patrol,    // [6]  = behaviour[1]
    (void*)cerberus_beh_chase,     // [7]  = behaviour[2]
    (void*)cerberus_beh_leap,      // [8]  = behaviour[3]
    (void*)cerberus_beh_strafe,    // [9]  = behaviour[4]
    (void*)cerberus_beh_alert,     // [10] = behaviour[5]
    (void*)cerberus_beh_maul,      // [11] = behaviour[6]
    (void*)cerberus_beh_bite,      // [12] = behaviour[7]
    (void*)cerberus_beh_scd,       // [13] = behaviour[8]
    (void*)cerberus_beh_stalk,     // [14] = behaviour[9]
    NULL                           // [15] terminator
};
void* const* const cerberus_behavior_view = &cerberus_states_table[5];

} // namespace

// ============================================================================
// The probe / steering primitives (0x0049b120 - 0x0049b5c0)
// ============================================================================

// ---------------------------------------------------------------------------
// cerberus_probe_ahead @ 0x0049b1a0
// Steps the entity 500 units (cerberus_sca_info[1], read as
// `word ptr [0x004d47ea]`) along its current facing, asks check_room_collision
// with radius 400, then steps it straight back. Returns 1 when the step would
// have hit something.
//
// The seed vector is the 4-dword block at g_deadMoveValue + 0x14, with only .x
// overwritten - the same block the zombie's billboard code copies. y/z/pad come
// out of it verbatim.
// ---------------------------------------------------------------------------
static unsigned char cerberus_probe_ahead(void)
{
    const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
    VECTOR step;
    step.y   = seed[1];
    step.z   = seed[2];
    step.pad = seed[3];
    step.x   = (int)cerberus_sca_info[1];   // 500

    RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &g_matrixScratch);
    ApplyMatrixLV(&g_matrixScratch, &step, &step);

    ENTITY->scaMatrixData.localMatrix.t[0] += step.x;
    ENTITY->scaMatrixData.localMatrix.t[2] += step.z;
    // `TEST AL,AL / SETG AL` - a SIGNED > 0, not != 0.
    char hit = (char)check_room_collision(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t, 400);
    ENTITY->scaMatrixData.localMatrix.t[0] -= step.x;
    ENTITY->scaMatrixData.localMatrix.t[2] -= step.z;

    return (unsigned char)(hit > 0);
}

// ---------------------------------------------------------------------------
// cerberus_probe_turn @ 0x0049b260
// "Would I hit a wall if I turned by `angleDelta` and ran at `speedMul` times
// my current speed?" Add_speedXZ moves the entity for real, so every field it
// touches - localMatrix.t[0]/t[2], position.x/z and move_speed_current - is
// saved and put back afterwards.
//
// The return value is cerberus_probe_ahead's, still in AL at the RET; Ghidra
// prints this function as void.
// ---------------------------------------------------------------------------
static unsigned char cerberus_probe_turn(short angleDelta, short speedMul)
{
    short savedPosZ  = ENTITY->position.z;
    int   savedX     = ENTITY->scaMatrixData.localMatrix.t[0];
    short savedSpeed = (short)ENTITY->move_speed_current;
    int   savedZ     = ENTITY->scaMatrixData.localMatrix.t[2];
    short savedPosX  = ENTITY->position.x;

    ENTITY->move_speed_current = (unsigned short)(short)(savedSpeed * speedMul);
    Add_speedXZ((int)angleDelta);
    unsigned char blocked = cerberus_probe_ahead();

    ENTITY->scaMatrixData.localMatrix.t[0] = savedX;
    ENTITY->scaMatrixData.localMatrix.t[2] = savedZ;
    ENTITY->position.x = savedPosX;
    ENTITY->position.z = savedPosZ;
    ENTITY->move_speed_current = (unsigned short)savedSpeed;
    return blocked;
}

// ---------------------------------------------------------------------------
// cerberus_probe_both_turns @ 0x0049b2f0
// Probes a hard left (+0x400) and a hard right (-0x400). Returns
// left - right: +1 "only left is blocked, go right", -1 "only right is blocked,
// go left", 0 "both clear". When BOTH are blocked it returns `bothBlocked`
// instead, which the callers use to pick a fallback.
// ---------------------------------------------------------------------------
static char cerberus_probe_both_turns(short speedMul, char bothBlocked)
{
    char left  = (char)cerberus_probe_turn(0x400, speedMul);
    char right = (char)-(char)cerberus_probe_turn((short)0xFC00, speedMul);
    if (left != 0 && right != 0) {
        return bothBlocked;
    }
    return (char)(right + left);
}

// ---------------------------------------------------------------------------
// cerberus_probe_toward_player @ 0x0049b5c0
// The same probe, aimed at the player instead of at a fixed turn.
// ---------------------------------------------------------------------------
static unsigned char cerberus_probe_toward_player(short speedMul)
{
    short toPlayer = (short)getAngleTowardsTarget(
        g_playerEntity.scaMatrixData.localMatrix.t[0],
        g_playerEntity.scaMatrixData.localMatrix.t[2]);
    return cerberus_probe_turn((short)(toPlayer - ENTITY->angle), speedMul);
}

// ---------------------------------------------------------------------------
// cerberus_speed_up @ 0x0049b330 / cerberus_slow_down @ 0x0049b370
// Ramp move_speed_current toward a cap, one `step` per frame. Both no-op once
// the cap is reached, so a state can call them unconditionally.
// ---------------------------------------------------------------------------
static void cerberus_speed_up(short step, short maxSpeed)
{
    if ((short)ENTITY->move_speed_current < maxSpeed) {
        ENTITY->move_speed_current =
            (unsigned short)(short)(step + (short)ENTITY->move_speed_current);
        if ((short)ENTITY->move_speed_current > maxSpeed) {
            ENTITY->move_speed_current = (unsigned short)maxSpeed;
        }
    }
}

static void cerberus_slow_down(short step, short minSpeed)
{
    if (minSpeed < (short)ENTITY->move_speed_current) {
        ENTITY->move_speed_current =
            (unsigned short)(short)((short)ENTITY->move_speed_current - step);
        if ((short)ENTITY->move_speed_current < minSpeed) {
            ENTITY->move_speed_current = (unsigned short)minSpeed;
        }
    }
}

// ---------------------------------------------------------------------------
// cerberus_set_turn_anim @ 0x0049b3b0
// Swaps the body animation to match the sign of the turn (lean left / straight
// / lean right) and restarts the blend, but only when it actually changes.
// ---------------------------------------------------------------------------
static void cerberus_set_turn_anim(short turn)
{
    int sign;
    if (turn == 0)      sign = 0;
    else if (turn < 1)  sign = -1;
    else                sign = 1;

    if (ENTITY->animationId != cerberus_turn_anim_base[sign]) {
        ENTITY->animationId = cerberus_turn_anim_base[sign];
        ENTITY->timing_control = 0;
        if (ENTITY->blend_counter < 4) {
            ENTITY->blend_counter = 7;
        }
    }
}

// ---------------------------------------------------------------------------
// cerberus_rotate_spine @ 0x0049b4f0
// Adds a rotation to joints 1, 3 and 5 of the dog's neck chain, each one taking
// a bit more of it than the last (x1, x1.25, x1.5), and rebuilds their local
// matrices. Returns the address of joint 5's block, which cerberus_head_track
// reads the head's world position out of.
//
// The offsets are raw joint-array bytes, not JointStruct indices: 0x80/0xA0 is
// joint 1, 0xFC/0x11C is joint 3, 0x178/0x198 is joint 5 (stride 0x7C).
// ---------------------------------------------------------------------------
static int cerberus_rotate_spine(short rx, short ry, short rz)
{
    int joints = (int)ENTITY->jointsStructs;

    SVECTOR* r1 = (SVECTOR*)(joints + 0x80);
    r1->x = (short)(r1->x + rx);
    r1->y = (short)(r1->y + ry);
    r1->z = (short)(r1->z + rz);
    RotMatrix(r1, (MATRIX*)(joints + 0xA0));

    SVECTOR* r3 = (SVECTOR*)(joints + 0xFC);
    r3->x = (short)(r3->x + (rx >> 2) + rx);
    r3->y = (short)(r3->y + (ry >> 2) + ry);
    r3->z = (short)(r3->z + (rz >> 2) + rz);
    RotMatrix(r3, (MATRIX*)(joints + 0x11C));

    SVECTOR* r5 = (SVECTOR*)(joints + 0x178);
    r5->x = (short)(r5->x + (rx >> 1) + rx);
    r5->y = (short)(r5->y + (ry >> 1) + ry);
    r5->z = (short)(r5->z + (rz >> 1) + rz);
    RotMatrix(r5, (MATRIX*)(joints + 0x198));

    return joints + 0x174;
}

// ---------------------------------------------------------------------------
// cerberus_head_track @ 0x0049b400
// Turns the head toward the player and clamps the accumulated head yaw to
// +/-0x100. To ask "where would the head be looking", it temporarily moves the
// ENTITY to the head joint's world position and adds the head yaw, runs
// turn_toward_target from there, then puts position and angle back.
//
// Returns 1 when the head hit the clamp - i.e. the dog can no longer follow the
// player without turning its body.
// ---------------------------------------------------------------------------
static unsigned char cerberus_head_track(void)
{
    int head = cerberus_rotate_spine(0, CB_SWERVE, 0);

    short savedAngle = ENTITY->angle;
    int   savedX     = ENTITY->scaMatrixData.localMatrix.t[0];
    int   savedZ     = ENTITY->scaMatrixData.localMatrix.t[2];

    ENTITY->scaMatrixData.localMatrix.t[0] = *(int*)(head + 0x58) + *(int*)(head + 0x38);
    ENTITY->scaMatrixData.localMatrix.t[2] = *(int*)(head + 0x60) + *(int*)(head + 0x40);
    ENTITY->angle = (short)(ENTITY->angle + (CB_SWERVE >> 1) + CB_SWERVE);

    CB_SWERVE = (short)(CB_SWERVE + (short)turn_toward_target(
        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 8));

    bool clampedLow = CB_SWERVE < -0x100;
    if (clampedLow)  CB_SWERVE = -0x100;
    bool clampedHigh = CB_SWERVE > 0x100;
    if (clampedHigh) CB_SWERVE = 0x100;

    ENTITY->scaMatrixData.localMatrix.t[0] = savedX;
    ENTITY->scaMatrixData.localMatrix.t[2] = savedZ;
    ENTITY->angle = savedAngle;

    return (unsigned char)(clampedHigh || clampedLow);
}

// ---------------------------------------------------------------------------
// cerberus_spawn_blood @ 0x0049b120
// Emits one blood billboard per frame at joint `jointIdx` of `owner` (which is
// the PLAYER during a maul, and the dog otherwise), while CB_BLOOD is positive.
//
// Argument order is the original's and is NOT the zombie's: here the spawn
// position is the joint's world translation (joints + 0x58 + idx*0x7C, i.e.
// JointStruct.world.t) and the sprite/transform argument is the g_deadMoveValue
// pointer itself. The g_playerPosScratch fill above it is dead in this path -
// the original still does it, and RoomCollision.cpp shares that word, so it
// stays.
// ---------------------------------------------------------------------------
static void cerberus_spawn_blood(void* owner, unsigned char jointIdx,
                                 unsigned char depthGroup)
{
    if (CB_BLOOD <= 0) {
        return;
    }
    const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
    g_playerPosScratch.x   = seed[0];
    g_playerPosScratch.y   = seed[1];
    g_playerPosScratch.z   = seed[2];
    g_playerPosScratch.pad = seed[3];

    int joints = *(int*)((char*)owner + 0x98);
    Effect_CreateBillboard(0, depthGroup, 0,
                           (void*)g_deadMoveValue,
                           (void*)(joints + 0x58 + (unsigned int)jointIdx * 0x7C),
                           0);
    CB_BLOOD--;
}

namespace {

// ============================================================================
// State 0 - cerberus_init @ 0x004980f0
// ============================================================================
void cerberus_init(void)
{
    // `MOV dword ptr [EAX+0x84],1` - one DWORD store covering state,
    // ignore_player_flag, action_behavior and action_state.
    ENTITY->state = 1;
    ENTITY->ignore_player_flag = 0;
    ENTITY->action_behavior = 0;
    ENTITY->action_state = 0;

    // Three rand() calls; the FIRST result is thrown away. Health is
    // table[rand & 0xF] + (rand & 3) - the dog is the only enemy that ADDS its
    // variance instead of subtracting it.
    rand();
    unsigned short healthBase = (unsigned short)cerberus_health_tbl[rand() & 0xF];
    ENTITY->health = (short)(healthBase + (unsigned short)(rand() & 3));
    ENTITY->hit_state = 0;

    CB_PROBE    = 0;
    CB_BEHFLAGS = 1;                 // word store: run flag set, [0x187] cleared
    CB_AIFLAGS  = CB_AI_ACTIVE;      // word store: 0x0080

    ENTITY->Sca_info = (unsigned int)cerberus_sca_info;
    ENTITY->status_flags &= 0x1F;

    ENTITY->animationId = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ResetJointTransforms();

    ENTITY->action_ticks_counter = 0;
    CB_AGGRO   = 0;
    CB_PHASE   = 0;
    CB_SWERVE  = 0;
    CB_REPAUSE = 0;
    ENTITY->move_speed_current = 0;
    CB_PATH  = 0;
    CB_ALERT = 0;

    // Nudge the spawn point 500 units along the dog's own facing. Same
    // g_deadMoveValue + 0x14 seed as cerberus_probe_ahead, .x replaced. The
    // original steers this through g_playerPosScratch (0x00be11b0), not a
    // local, and the clobber is observable.
    {
        const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
        g_playerPosScratch.y   = seed[1];
        g_playerPosScratch.z   = seed[2];
        g_playerPosScratch.pad = seed[3];
        g_playerPosScratch.x   = (int)cerberus_sca_info[1];   // 500

        RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &g_matrixScratch);
        ApplyMatrixLV(&g_matrixScratch, &g_playerPosScratch, &g_playerPosScratch);
        ENTITY->position.x = (short)(ENTITY->position.x + (short)g_playerPosScratch.x);
        ENTITY->position.z = (short)(ENTITY->position.z + (short)g_playerPosScratch.z);
    }

    // Ground shadow. 0x00808080 is an IMMEDIATE colour, not a pointer - see the
    // note in zombie_init.
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = -0x50;
    g_cerberusBarked = 0;
    g_animFrameIdSave = 0x00808080;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 0x550, 0x200);

    // Behaviours 6, 7 and 0x80 start the dog IN THE AIR (it is about to crash
    // through a window, or the script drops it), so status bit 2 stays set and
    // the Y position is left where the script put it. Everything else is
    // planted on the floor. The compare is `CMP ECX,6 / JL` then
    // `CMP ECX,7 / JLE`, i.e. 6 <= b <= 7, plus the explicit b == 0x80.
    unsigned char beh = ENTITY->behavior_flags;
    if (beh >= 6 && (beh <= 7 || beh == 0x80)) {
        ENTITY->status_flags |= 0x04;
        return;
    }
    ENTITY->scaMatrixData.localMatrix.t[1] = 0;
}

// ============================================================================
// The behaviour selector (0x004983d0) and its per-behaviour_flags entry points
// ============================================================================
void cerberus_beh_select_stalk(void);

// ---------------------------------------------------------------------------
// 0x00498450 - behavior_flags 0..3: patrol.
//
// `(behavior_flags & ~1) * 2` is a BYTE offset into 0x004d4888, so it is
// row (b >> 1) of a 4-byte struct: behaviours 0/1 walk at 40, behaviour 2 runs
// at 130. Behaviour 3 never reaches the row lookup - it tail-jumps to the stalk
// setup on the second instruction.
// ---------------------------------------------------------------------------
void cerberus_beh_select_patrol(void)
{
    CB_BEHFLAGS = 4;
    if ((char)ENTITY->behavior_flags == 3) {
        cerberus_beh_select_stalk();   // `JMP 0x004988a0` - a tail call
        return;
    }
    ENTITY->ignore_player_flag = 1;    // -> cerberus_beh_patrol

    const CerberusWalkParams& row = cerberus_walk_params[ENTITY->behavior_flags >> 1];
    ENTITY->animationId = row.animId;
    ENTITY->animation_frame_id = (unsigned char)(rand() & 0x1F);
    ENTITY->move_speed_current = (unsigned short)row.speed;

    CB_TURN_DELTA = (short)(unsigned short)(((ENTITY->behavior_flags >> 1) + 1) * 0x10);
    CB_BEHFLAGS   = (short)(ENTITY->behavior_flags & 1);
    CB_SWERVE     = (short)(((short)rand() - 2) & 0xF);
    ENTITY->blend_counter = 0xF;
}

// ---------------------------------------------------------------------------
// 0x00498520 - behavior_flags 4: chase. Latches the player's current position
// as the waypoint and clears the skid bits.
// ---------------------------------------------------------------------------
void cerberus_beh_select_chase(void)
{
    if (ENTITY->blend_counter > 7) {
        ENTITY->blend_counter = 7;
    }
    CB_TURN_DELTA = (short)((rand() & 7) + 0x3D);
    CB_AGGRO  = 0;
    CB_PHASE  = 0;
    CB_SWERVE = 0;
    ENTITY->reaction_timer = 0;

    if ((CB_AIFLAGS & CB_AI_REPROBE) != 0) {
        CB_SWERVE = (short)((rand() & 0xF) + 0x14);
    }
    ENTITY->ignore_player_flag = 2;    // -> cerberus_beh_chase
    ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
    CB_AIFLAGS = (short)((CB_AIFLAGS & ~(CB_AI_SKID | CB_AI_SKID2)) | CB_AI_ACTIVE);
}

// ---------------------------------------------------------------------------
// 0x00498600 - behavior_flags 5, 6 and 7: the scripted entrances.
//
//   5  the dog is already inside and takes a running leap (its yaw is nudged
//      +/-0x20 first so it does not land exactly on the player)
//   6  it crashes IN through a window - status bits 1|2, a big upward kick
//   7  it drops from above - status bit 2, air ticks pre-loaded to 10 so it is
//      already falling fast on the first frame
//
// 6 and 7 also arm hit_state, because the entrance itself can hurt the player;
// 5 skips that pair (`goto LAB_00498736`).
// ---------------------------------------------------------------------------
void cerberus_beh_select_entrance(void)
{
    CB_AIRTICKS = 0;
    unsigned char beh = ENTITY->behavior_flags;

    if (beh == 5) {
        if (ENTITY->animationId == 0x10) {
            ENTITY->angle = (short)(ENTITY->angle - 0x20);
        } else if (ENTITY->animationId == 0x0F) {
            ENTITY->angle = (short)(ENTITY->angle + 0x20);
        }
        ENTITY->animationId = 6;
        ENTITY->move_speed_current = 0x41;
        CB_LAUNCH_VY = 0x012C;             // 300
        ENTITY->reaction_timer = 0xFFD8;   // -40 gravity at +0x176
    } else if (beh == 6) {
        ENTITY->status_flags |= 6;
        ENTITY->move_speed_current = 0x41;
        CB_LAUNCH_VY = 0x01E0;             // 480 - crashing in through glass
        ENTITY->reaction_timer = 0xFFD8;   // -40
        ENTITY->animationId = 6;
        CB_BEHFLAGS = 5;
        ENTITY->hit_state = 1;
    } else if (beh == 7) {
        ENTITY->status_flags |= 4;
        ENTITY->animationId = 7;
        CB_AIRTICKS = 10;
        ENTITY->move_speed_current = 0x118;
        CB_LAUNCH_VY = 0x0190;             // 400
        ENTITY->reaction_timer = 0xFFD8;   // -40
        CB_BEHFLAGS = 5;
        ENTITY->hit_state = 1;
    }

    ENTITY->ignore_player_flag = 3;    // -> cerberus_beh_leap
    ENTITY->blend_counter = 3;
    ENTITY->action_ticks_counter = 8;
    CB_AGGRO = 0;
    // word store at +0x182: internal_timer AND pad_183.
    ENTITY->internal_timer = 0;
    *((unsigned char*)ENTITY + 0x183) = 0;
}

// ---------------------------------------------------------------------------
// 0x00498780 - behavior_flags 8: bite. Straight into the close-range snap with
// animation 4 and no blend.
// ---------------------------------------------------------------------------
void cerberus_beh_select_bite(void)
{
    ENTITY->ignore_player_flag = 7;    // -> cerberus_beh_bite
    ENTITY->animationId = 4;
    ENTITY->blend_counter = 0;
    ENTITY->hit_state = 1;
}

// ---------------------------------------------------------------------------
// 0x004987c0 - behavior_flags 9: strafe. If no turn is in progress, picks one
// at random: +0x80 or -0x80 (`(-(rand & 1 == 0) & 0xFF00) + 0x80`).
// ---------------------------------------------------------------------------
void cerberus_beh_select_strafe(void)
{
    ENTITY->ignore_player_flag = 4;    // -> cerberus_beh_strafe
    if (CB_TURN_STEP == 0) {
        unsigned short mask = (rand() & 1) == 0 ? (unsigned short)0xFF00
                                                : (unsigned short)0x0000;
        CB_TURN_STEP = (short)(unsigned short)(mask + 0x80);
    }
    ENTITY->action_ticks_counter = 8;
    CB_AGGRO = 0;
    CB_AIFLAGS |= CB_AI_ACTIVE;
}

// ---------------------------------------------------------------------------
// 0x00498830 - behavior_flags 10 and 11: the alert / bark pause.
// ---------------------------------------------------------------------------
void cerberus_beh_select_alert(void)
{
    ENTITY->ignore_player_flag = 5;    // -> cerberus_beh_alert
    ENTITY->animationId = 0;
    ENTITY->blend_counter = 0xF;
    ENTITY->animation_frame_id = 0;
    ENTITY->action_ticks_counter = 0x14;
    CB_TURN_STEP = 0;
    CB_AGGRO = 0;
    ENTITY->hit_state = 1;
}

// ---------------------------------------------------------------------------
// 0x004988a0 - behavior_flags 12 (and the tail of behaviour 3): stalk.
// Raises the "hunting" bit unless the dog has already given up (bit 2).
// ---------------------------------------------------------------------------
void cerberus_beh_select_stalk(void)
{
    ENTITY->ignore_player_flag = 9;    // -> cerberus_beh_stalk
    ENTITY->animationId = 1;
    ENTITY->animation_frame_id = (unsigned char)(rand() & 0x1F);
    ENTITY->blend_counter = 0;
    CB_AGGRO = 0;
    ENTITY->move_speed_current = 0;
    if ((CB_BEHFLAGS & 4) == 0) {
        CB_BEHFLAGS |= 2;
    }
    ENTITY->hit_state = 0;
}

// ---------------------------------------------------------------------------
// 0x004d4850 - behaviour selector table, indexed by behavior_flags (0..12).
// The 14th slot in the original is the NULL terminator; nothing produces 13.
// ---------------------------------------------------------------------------
void (*const cerberus_behavior_select_tbl[13])(void) = {
    cerberus_beh_select_patrol,    // [0]  0x00498450
    cerberus_beh_select_patrol,    // [1]
    cerberus_beh_select_patrol,    // [2]
    cerberus_beh_select_patrol,    // [3]  tail-jumps to the stalk setup
    cerberus_beh_select_chase,     // [4]  0x00498520
    cerberus_beh_select_entrance,  // [5]  0x00498600
    cerberus_beh_select_entrance,  // [6]
    cerberus_beh_select_entrance,  // [7]
    cerberus_beh_select_bite,      // [8]  0x00498780
    cerberus_beh_select_strafe,    // [9]  0x004987c0
    cerberus_beh_select_alert,     // [10] 0x00498830
    cerberus_beh_select_alert,     // [11]
    cerberus_beh_select_stalk      // [12] 0x004988a0
};

// ---------------------------------------------------------------------------
// cerberus_behavior_select @ 0x004983d0
// Runs whenever ignore_player_flag drops to 0: turns the SCD-facing
// behavior_flags byte into a behaviour index. behavior_flags 0x80 is the
// script-driven idle and bypasses the table entirely.
// ---------------------------------------------------------------------------
void cerberus_behavior_select(void)
{
    CB_ALERT = 0;
    ENTITY->timing_control = 0;
    if ((CB_AIFLAGS & 0x80) != 0) {
        ENTITY->animation_frame_id = 0;
    }
    if (ENTITY->behavior_flags == 0x80) {
        ENTITY->ignore_player_flag = 8;   // -> cerberus_beh_scd
        ENTITY->animationId = 0;
        ENTITY->blend_counter = 0xF;
        return;
    }
    cerberus_behavior_select_tbl[ENTITY->behavior_flags]();
}

// ============================================================================
// State 1 - cerberus_behavior_run @ 0x00498310
// The per-frame driver: refresh the distance to the player, run the pathfinder,
// re-arm the "solid" status bits, and dispatch the behaviour.
// ============================================================================
void cerberus_behavior_run(void)
{
    if (ENTITY->behavior_flags == 0x80) {
        ENTITY->ignore_player_flag = 0;
    }

    // Manhattan distance, built as |dx| + |dz| with the CDQ/XOR/SUB absolute
    // value the original emits twice.
    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
           - ENTITY->scaMatrixData.localMatrix.t[0];
    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
           - ENTITY->scaMatrixData.localMatrix.t[2];
    CB_DIST = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);

    CB_PATH = (unsigned char)entity_pathfind_update();
    ENTITY->status_flags &= 0x1F;
    ENTITY->status_flags |= 0x40;

    if (CB_ALERT == 0) {
        entity_check_visual_range(5000);
    }
    if (ENTITY->ignore_player_flag == 0) {
        cerberus_behavior_select();
        CB_SUBSTATE_W = 0;   // action_behavior AND action_state
    }
    ((void(*)())cerberus_behavior_view[ENTITY->ignore_player_flag])();
}

// ============================================================================
// Behaviour 1 - patrol (0x00499ae0) and its three sub-states (0x004d48c8)
// ============================================================================

// 0x00499b90 - sub 0: entered on the first frame; plants the dog on the floor
// and clears the turn bookkeeping.
void cerberus_patrol_begin(void)
{
    ENTITY->action_behavior++;
    ENTITY->scaMatrixData.localMatrix.t[1] = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0xF;
    CB_AGGRO  = 0;
    CB_PHASE  = 0;
    CB_SWERVE = 0;
}

// 0x00499bf0 - sub 1: hold the current heading until either the forward probe
// trips or the dwell timer runs out, then pick the next turn.
void cerberus_patrol_pick_turn(void)
{
    if ((CB_PROBE & 1) == 0
        && (CB_BEHFLAGS != 0 || ENTITY->action_ticks_counter != 0)) {
        ENTITY->action_ticks_counter--;
        return;
    }
    ENTITY->action_behavior++;
    ENTITY->action_ticks_counter = 0x400;

    char dir = cerberus_probe_both_turns(3, 0);
    if (dir == 0) {
        // Nothing blocked: a running dog always turns the same way, a walking
        // one flips a coin.
        if (CB_BEHFLAGS == 0) {
            CB_TURN_STEP = (rand() & 1) == 0 ? (short)-CB_TURN_DELTA : CB_TURN_DELTA;
        } else {
            CB_TURN_STEP = CB_TURN_DELTA;
        }
    } else {
        CB_TURN_STEP = (short)(CB_TURN_DELTA * (short)dir);
        CB_PHASE = 0;
    }

    // A walking dog that would be turning AWAY from the player cuts the turn
    // short, so it drifts back toward them.
    if (CB_BEHFLAGS == 0) {
        int toPlayer = turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200);
        if ((int)(short)toPlayer * (int)(short)dir < 0) {
            ENTITY->action_ticks_counter =
                (unsigned short)((short)ENTITY->action_ticks_counter >> 2);
        }
    }
}

// 0x00499d20 - sub 2: burn the turn budget down at |turn| per frame.
void cerberus_patrol_turn(void)
{
    ENTITY->angle = (short)(ENTITY->angle + CB_TURN_STEP);
    short mag = CB_TURN_STEP < 0 ? (short)-CB_TURN_STEP : CB_TURN_STEP;
    ENTITY->action_ticks_counter =
        (unsigned short)((short)ENTITY->action_ticks_counter - mag);

    if ((short)ENTITY->action_ticks_counter < 0) {
        ENTITY->action_behavior = 0;
        if (CB_BEHFLAGS == 0) {
            ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1F) + 0x4B);
        }
    }
}

void (*const cerberus_patrol_tbl[3])(void) = {   // 0x004d48c8
    cerberus_patrol_begin,      // 0x00499b90
    cerberus_patrol_pick_turn,  // 0x00499bf0
    cerberus_patrol_turn        // 0x00499d20
};

// ---------------------------------------------------------------------------
// cerberus_beh_patrol @ 0x00499ae0 (states_table[6] = behaviour[1])
// The idle wander. Once the player is inside 0x1194 (4500) - or is stuck in one
// of the two unreachable poses - it drops back to behaviour_flags 4, the plain
// chase.
// ---------------------------------------------------------------------------
void cerberus_beh_patrol(void)
{
    if (CB_DIST < 0x1194
        || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_A
        || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_B) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 4;
        ENTITY->blend_counter = 7;
    }
    Add_speedXZ(8);
    cerberus_patrol_tbl[ENTITY->action_behavior]();
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);

    if (ENTITY->animation_frame_id == 0 || ENTITY->animation_frame_id == 0x1B) {
        Snd_em(9);   // footfall
    }
}

// ============================================================================
// Behaviour 2 - chase (0x00498920) and its three sub-states (0x004d4890)
// ============================================================================

// ---------------------------------------------------------------------------
// cerberus_consider_attack @ 0x0049af00
// The decision that turns a chase into an attack. Only runs inside 5000 units
// and only while the player is not already being grabbed.
//
// Facing the player (turn step 0x100 returns 0):
//   * if the player is hurt enough AND facing the dog, roll (rand & 0x7F)
//     against 126 (100 while poisoned) - on a hit, behaviour_flags 9, the
//     strafe, so the dog lines up a killing lunge;
//   * otherwise behaviour_flags 5, the running leap.
// The health thresholds are difficulty-dependent: 13 normally, 17 on the easy
// flag (g_ScenarioFlags bit SCENARIO_FLAG_SECOND_PLAYTHROUGH).
//
// Not facing yet: wind the swerve counter up, and once the dog is roughly
// aligned (turn step 0x400 returns 0) latch CB_AI_LOCKED_ON.
// ---------------------------------------------------------------------------
void cerberus_consider_attack(void)
{
    short playerHealth = g_playerEntity.health;
    if (CB_DIST >= 5000 || g_playerEntity.isBeingAttackedFlag != 0) {
        return;
    }

    if ((short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x100) == 0) {
        unsigned char poisoned = g_playerEntity.healthStatusFlags & 8;
        int easy = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH);
        // `(-(poisoned == 0) & 0x1a) + 100` - 126 when healthy, 100 when poisoned.
        short chance = (short)((poisoned == 0 ? 0x1A : 0) + 100);
        short threshold = easy == 0 ? (short)0x0D : (short)0x11;

        if (playerHealth < threshold
            && (char)is_facing_toward_entity(&g_playerEntity) != 0
            && (unsigned int)(rand() & 0x7F) < (unsigned int)(int)chance) {
            ENTITY->ignore_player_flag = 0;
            ENTITY->behavior_flags = 9;
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
            return;
        }
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 5;
        CB_AIFLAGS |= CB_AI_ACTIVE;
        CB_SWERVE = 0;
        return;
    }

    if (CB_SWERVE < 10) {
        CB_SWERVE++;
        if (CB_SWERVE > 9) {
            CB_SWERVE = (short)((rand() & 7) + CB_SWERVE + 0x1E);
            ENTITY->reaction_timer = 0;
        }
    }

    if ((short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x400) == 0) {
        CB_AIFLAGS |= CB_AI_LOCKED_ON;
        return;
    }
    if ((CB_AIFLAGS & 3) == CB_AI_LOCKED_ON) {
        // `(rand & 0x40) > 0x38` - true on the draws that have bit 6 set.
        if ((rand() & 0x40) > 0x38) {
            CB_AIFLAGS = (short)((CB_AIFLAGS & ~CB_AI_LOCKED_ON)
                                 | (CB_AI_REPROBE | CB_AI_SKID));
        }
    }
}

// ---------------------------------------------------------------------------
// 0x00498990 - chase sub 0: run at the waypoint.
//
// Two steering modes. While the dog is not paused, is not already swerving and
// is at full speed it just turns toward the waypoint. Otherwise it steers off
// the collision-probe history in CB_PROBE: a fresh block (bit 0) or a stall
// (bit 4) re-derives a turn - wider by 0x20 when CB_AI_REPROBE is set - and
// that turn is then applied every frame while the swerve counter winds down.
// ---------------------------------------------------------------------------
void cerberus_chase_run(void)
{
    cerberus_speed_up(0x14, 0xF0);

    g_playerPosScratch.x = (int)ENTITY->player_pos_x;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = (int)ENTITY->player_pos_z;

    unsigned int turn;
    if (CB_REPAUSE == 0 && CB_SWERVE < 10 && (short)ENTITY->move_speed_current > 0xEF) {
        turn = (unsigned int)turn_toward_target(&g_playerPosScratch, CB_TURN_DELTA) & 0xFFFF;
        entity_rotate_toward_target(&g_playerPosScratch, (unsigned short)CB_TURN_DELTA);
    } else {
        if ((CB_PROBE & 0x11) != 0) {
            short step = (short)(((CB_AIFLAGS & CB_AI_REPROBE) != 0 ? 0x20 : 0)
                                 + CB_TURN_DELTA);
            ENTITY->reaction_timer =
                (unsigned short)(short)turn_toward_target(&g_playerPosScratch, step);
            CB_AIFLAGS &= ~CB_AI_REPROBE;
        }
        turn = (unsigned int)ENTITY->reaction_timer;
        ENTITY->angle = (short)(ENTITY->angle + (short)ENTITY->reaction_timer);
        CB_SWERVE--;
        if (CB_SWERVE < 0xB) {
            CB_SWERVE = 5;
            CB_AIFLAGS &= ~CB_AI_REPROBE;
        }
    }

    // The pathfinder's verdict: 1 = the waypoint is fresh, refresh it and go
    // back to full speed; 0 = six consecutive dead frames and the dog gives up
    // (behaviour_flags 12 = stalk, with the "gave up" bit set); 2 = still
    // thinking, leave the counter alone.
    if ((CB_PATH & 2) == 0) {
        if ((CB_PATH & 1) == 0) {
            CB_PHASE++;
            if (CB_PHASE == 6) {
                ENTITY->ignore_player_flag = 0;
                ENTITY->behavior_flags = 12;
                CB_BEHFLAGS_B |= 4;
                return;
            }
        } else {
            CB_PHASE = 0;
            ENTITY->move_speed_current = 0xF0;
            ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
            ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        }
    }

    cerberus_set_turn_anim((short)turn);

    if ((CB_BEHFLAGS_B & 2) == 0) {
        if ((CB_PROBE & 0x11) != 0) {
            // Wedged against geometry: after eight blocked frames, bail into
            // the strafe.
            CB_AGGRO += 2;
            if (CB_AGGRO > 0xF) {
                ENTITY->ignore_player_flag = 0;
                ENTITY->behavior_flags = 9;
                CB_TURN_STEP = (short)((short)turn * 2);
                CB_AIFLAGS &= ~CB_AI_ACTIVE;
            }
        } else if (CB_AGGRO > 0) {
            CB_AGGRO--;
        }
    } else {
        // Hunting: probe straight at the player, and once that path is clear,
        // switch to the wall-follow turn sub-state.
        if (cerberus_probe_toward_player(4) != 0 && CB_REPAUSE == 0) {
            CB_SUBSTATE_W = 1;
        }
        if (CB_REPAUSE > 0) {
            CB_REPAUSE--;
        }
    }

    if ((CB_AIFLAGS & (CB_AI_SKID | CB_AI_SKID2)) != 0) {
        CB_SUBSTATE_W = 2;
    }
}

// 0x0049a130 - one tick of the wall-follow sequence; defined with the stalk.
short cerberus_wall_follow_step(short phaseLimit, short probeSpeed);

// 0x00498c30 - chase sub 1: follow the wall around until the sequence gives up
// (CB_AI_WALL), then drop back to a fresh chase with a randomised re-target
// pause.
void cerberus_chase_turn(void)
{
    cerberus_speed_up(0x14, 0xF0);
    short dir = cerberus_wall_follow_step(0x5A, 0x40);
    if (dir != 3) {
        cerberus_set_turn_anim((short)(dir - 1));
    }
    if ((CB_AIFLAGS & CB_AI_WALL) != 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 4;
        CB_TURN_DELTA = (short)((rand() & 7) + 0x3D);
        CB_AIFLAGS &= ~CB_AI_ACTIVE;
        CB_REPAUSE = (short)(((rand() & 3) * 2) + 8);
    }
}

// 0x00498cd0 - chase sub 2: the skid. Brakes hard, swings around toward the
// waypoint and counts the total yaw swept; once it has turned more than half a
// circle, or the turn collapses to zero, it hands back to sub 0.
void cerberus_chase_skid(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->action_ticks_counter = 0;
    }
    g_playerPosScratch.x = (int)ENTITY->player_pos_x;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = (int)ENTITY->player_pos_z;

    CB_TURN_STEP = (short)turn_toward_target(&g_playerPosScratch, 0x60);
    // The accumulated yaw doubles as the sideways drift while braking.
    Add_speedXZ(-(int)(short)ENTITY->action_ticks_counter);

    if (ENTITY->action_state == 1) {
        cerberus_slow_down(0x0E, 10);
        if ((short)ENTITY->move_speed_current < 0xB) {
            ENTITY->action_state++;
        }
    } else {
        cerberus_speed_up(0x0E, 0xF0);
    }

    ENTITY->angle = (short)(ENTITY->angle + CB_TURN_STEP);
    ENTITY->action_ticks_counter =
        (unsigned short)((short)ENTITY->action_ticks_counter + CB_TURN_STEP);
    cerberus_set_turn_anim(CB_TURN_STEP);

    short swept = (short)ENTITY->action_ticks_counter;
    int sweptAbs = swept < 0 ? -(int)swept : (int)swept;
    if (sweptAbs > 0x7FF || CB_TURN_STEP == 0) {
        ENTITY->action_behavior = 0;
        CB_AIFLAGS &= ~(CB_AI_SKID | CB_AI_SKID2);
    }
}

void (*const cerberus_chase_tbl[3])(void) = {   // 0x004d4890
    cerberus_chase_run,   // 0x00498990
    cerberus_chase_turn,  // 0x00498c30
    cerberus_chase_skid   // 0x00498cd0
};

// ---------------------------------------------------------------------------
// cerberus_beh_chase @ 0x00498920 (states_table[7] = behaviour[2])
// ---------------------------------------------------------------------------
void cerberus_beh_chase(void)
{
    if (ENTITY->action_behavior < 2) {
        Add_speedXZ(0);
    }
    cerberus_chase_tbl[ENTITY->action_behavior]();
    cerberus_consider_attack();
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        Snd_em(0);   // running loop cue, on animation wrap
    }
}

// ============================================================================
// Behaviour 3 - leap (0x00498e20) and its five sub-states (0x004d48a0)
// ============================================================================

// ---------------------------------------------------------------------------
// cerberus_bite_player @ 0x0049ac80
// The hit itself. If the player is already low enough on health AND is facing
// away, the dog goes for the kill instead: it turns the player away from it,
// forces them into animation 7 (the maul), and returns 2 - which the leap code
// reads as "go to the maul behaviour".
//
// Otherwise it is an ordinary bite: -12 health, one of two flinch animations
// (0x66 / 0x67) chosen by whether the player is facing the dog, and three blood
// billboards owed.
// ---------------------------------------------------------------------------
unsigned char cerberus_bite_player(void)
{
    ENTITY->hit_state = 0;

    int  easy = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH);
    short threshold = easy == 0 ? (short)0x0D : (short)0x11;

    if (g_playerEntity.health < threshold
        && (char)is_facing_toward_entity(&g_playerEntity) == 0) {
        g_playerEntity.flags |= 2;
        ENTITY->status_flags |= 2;
        ENTITY->move_speed_current = 0;
        CB_AGGRO = 2;
        g_playerEntity.directionAngle = (short)((ENTITY->angle + 0x800) & 0xFFF);
        ENTITY->hit_state = 1;
        PLAYER_ANIM_WORD = 0x02000207u;   // anim 7, frame 2, behaviour 0, state 2
        g_playerEntity.isBeingAttackedFlag = 1;
        ENTITY->status_flags &= 0xFB;
        return (unsigned char)CB_AGGRO;
    }

    g_playerEntity.action_state = (unsigned char)is_facing_toward_entity(&g_playerEntity);
    // Entity+0xC6/0xC8: where the dog last saw the player.
    ENTITY->unk_c6 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    ENTITY->unk_c8 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[2];

    if (ENTITY->behavior_flags != 6) {
        ENTITY->action_behavior = 4;      // -> the tumble sub-state
        ENTITY->animationId = 0x11;
        ENTITY->animation_frame_id = 1;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0xF;
        ENTITY->status_flags &= 0xFB;
        ENTITY->move_speed_current =
            (unsigned short)((short)ENTITY->move_speed_current >> 2);
    }

    g_playerEntity.unk_c6 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_playerEntity.unk_c8 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[2];

    unsigned char facing = (unsigned char)is_facing_toward_entity(&g_playerEntity);
    g_playerEntity.isBeingAttackedFlag = (unsigned char)(facing + 1);
    g_playerEntity.action_behavior     = (unsigned char)(facing + 0x66);
    g_playerEntity.health = (short)(g_playerEntity.health - 0xC);

    CB_TURN_DELTA = 0x200;
    CB_SWERVE = 0;
    CB_BLOOD  = 3;
    CB_PHASE  = 0;
    Snd_em(3);   // bite
    return (unsigned char)CB_AGGRO;
}

// 0x00498e50 - leap sub 0: the crouch. Waits out the wind-up timer (or an
// already-loaded air tick), then kicks off. Behaviour 7 - the drop from above -
// is silent, everything else yelps.
void cerberus_leap_crouch(void)
{
    Add_speedXZ(0);
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0 || CB_AIRTICKS != 0) {
        ENTITY->action_behavior++;
        ENTITY->move_speed_current = 0x118;
        CB_ALERT = 1;
        if ((char)ENTITY->behavior_flags != 7) {
            Snd_em(6);
        }
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// 0x00498ee0 - leap sub 1: airborne. Ballistic step, then a reach test off
// joint 4's world matrix. The reach shrinks to 800 when the player is already
// badly hurt and grows to 1500 when they are poisoned.
void cerberus_leap_airborne(void)
{
    ENTITY->status_flags |= 0x40;
    entity_ballistic_step((short)ENTITY->move_speed_current,
                          CB_LAUNCH_VY,
                          (short)ENTITY->reaction_timer, 0);

    int joints = (int)ENTITY->jointsStructs;
    unsigned char airTicks = CB_AIRTICKS;
    short gravity  = (short)ENTITY->reaction_timer;
    short launchVy = CB_LAUNCH_VY;

    const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
    g_playerPosScratch.x   = seed[0];
    g_playerPosScratch.y   = seed[1];
    g_playerPosScratch.z   = seed[2];
    g_playerPosScratch.pad = seed[3];

    short reach;
    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
        reach = g_playerEntity.health > 0x0C ? (short)1000 : (short)800;
    } else {
        reach = g_playerEntity.health > 0x10 ? (short)1000 : (short)800;
    }
    if ((g_playerEntity.healthStatusFlags & 8) != 0) {
        reach = 0x5DC;   // 1500 - a poisoned player is easier to catch
    }

    if (CB_AGGRO == 0) {
        CB_AGGRO = (short)(unsigned short)FUN_0048ae00(
            (MATRIX*)(joints + 0x234), &g_playerPosScratch, reach,
            g_playerEntity.scaMatrixData.localMatrix.t);

        if (CB_AGGRO != 0
            && g_playerEntity.isBeingAttackedFlag == 0
            && ENTITY->scaMatrixData.localMatrix.t[1] > -0x708
            && cerberus_bite_player() == 1) {
            // An ordinary bite connected: cerberus_bite_player already rewrote
            // the sub-state, so nothing else runs this frame.
            if (ENTITY->animation_frame_id == 0) {
                return;
            }
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
            return;
        }
    }

    // Past the top of the arc? `(airTicks + 1) * gravity + launchVy` is next
    // frame's vertical velocity.
    if ((int)((airTicks + 1) * (int)gravity + (int)launchVy) < 1) {
        if (CB_AGGRO == 2) {
            ENTITY->ignore_player_flag = 6;    // -> cerberus_beh_maul
            ENTITY->action_behavior = 0;
            ENTITY->status_flags |= 2;
            g_playerEntity.flags |= 2;
        } else {
            ENTITY->action_behavior++;
            if (ENTITY->animationId == 6) {
                ENTITY->animationId = 7;
                ENTITY->timing_control = 0;
                ENTITY->blend_counter = 0xF;
            }
        }
    }

    if (ENTITY->animation_frame_id != 0) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
    }
}

// 0x004990f0 - leap sub 2: falling. Ends the moment the ballistic step brings Y
// back to ground level.
void cerberus_leap_land(void)
{
    ENTITY->status_flags |= 0x40;
    entity_ballistic_step((short)ENTITY->move_speed_current,
                          CB_LAUNCH_VY,
                          (short)ENTITY->reaction_timer, 0);

    if (ENTITY->scaMatrixData.localMatrix.t[1] >= 0) {
        ENTITY->action_behavior++;
        ENTITY->scaMatrixData.localMatrix.t[1] = 0;
        ENTITY->status_flags = (unsigned char)((ENTITY->status_flags & 0xF9) | 0x20);
        Snd_em(5);   // landing thud
    }
    if (ENTITY->animation_frame_id != 9) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
    }
}

// 0x00499190 - leap sub 3: get back up. On the last frame it picks what to do
// next: if the dog is still jammed against geometry it turns away (behaviour 9,
// strafe); otherwise it goes back to the plain chase.
void cerberus_leap_recover(void)
{
    Add_speedXZ(0);
    cerberus_slow_down(0x40, 0xF0);
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100) == 0) {
        return;
    }

    ENTITY->status_flags &= 0xFB;
    ENTITY->ignore_player_flag = 0;
    ENTITY->hit_state = 0;
    CB_AIFLAGS &= ~CB_AI_REPROBE;

    if ((CB_PROBE & 0x11) != 0
        && ENTITY->behavior_flags != 6 && ENTITY->behavior_flags != 7
        && (CB_BEHFLAGS_B & 2) == 0) {
        ENTITY->behavior_flags = 9;
        CB_TURN_STEP =
            (short)((short)turn_toward_target(&g_playerPosScratch, CB_TURN_DELTA) * 2);
        if (CB_TURN_STEP == 0) {
            // Dead ahead but blocked: probe both ways and turn the free one.
            char dir = cerberus_probe_both_turns(1, -1);
            CB_TURN_STEP = (short)((short)dir * CB_TURN_DELTA * 2);
        }
        return;
    }
    ENTITY->behavior_flags = 4;
}

// 0x004992b0 - leap sub 4: the tumble after a connected bite. Spins the dog a
// full 0x800 while it slides, then drops back into the chase.
void cerberus_leap_tumble(void)
{
    if (ENTITY->animation_frame_id < 0x10) {
        Add_speedXZ(0x800);
    }
    if (CB_SWERVE < 0x800) {
        ENTITY->angle = (short)(ENTITY->angle + 0x80);
        CB_SWERVE = (short)(CB_SWERVE + 0x80);
    }
    entity_ballistic_step(0, CB_LAUNCH_VY,
                          (short)ENTITY->reaction_timer, 0);

    if (ENTITY->scaMatrixData.localMatrix.t[1] >= 0 && CB_PHASE == 0) {
        Snd_em(5);
        CB_PHASE = 1;
    }
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100) != 0) {
        ENTITY->behavior_flags = 4;
        ENTITY->ignore_player_flag = 0;
        ENTITY->move_speed_current = 0;
        CB_AIFLAGS |= CB_AI_REPROBE;
    }
}

void (*const cerberus_leap_tbl[5])(void) = {   // 0x004d48a0
    cerberus_leap_crouch,    // 0x00498e50
    cerberus_leap_airborne,  // 0x00498ee0
    cerberus_leap_land,      // 0x004990f0
    cerberus_leap_recover,   // 0x00499190
    cerberus_leap_tumble     // 0x004992b0
};

// ---------------------------------------------------------------------------
// cerberus_beh_leap @ 0x00498e20 (states_table[8] = behaviour[3])
// ---------------------------------------------------------------------------
void cerberus_beh_leap(void)
{
    cerberus_leap_tbl[ENTITY->action_behavior]();
    cerberus_spawn_blood(ENTITY, 4, 0);
}

// ============================================================================
// Behaviour 4 - strafe (0x004993a0) and its three sub-states (0x004d48b8)
// ============================================================================

// 0x00499410 - strafe sub 0: hold the sidestep for the dwell timer. When it
// expires: if nothing is blocking, go back to the chase; if something is, count
// blocked frames and flip the strafe direction after eight of them.
void cerberus_strafe_hold(void)
{
    if ((short)ENTITY->action_ticks_counter < 1) {
        if ((CB_PROBE & 0x11) == 0) {
            ENTITY->ignore_player_flag = 0;
            ENTITY->behavior_flags = 4;
            CB_AIFLAGS &= ~CB_AI_ACTIVE;
            if ((CB_BEHFLAGS_B & 1) == 0) {
                CB_SWERVE = 0x28;
            }
        } else {
            short n = CB_AGGRO;
            CB_AGGRO = (short)(n + 1);
            if (n > 7 && CB_PHASE == 0) {
                ENTITY->action_behavior++;
                CB_TURN_STEP = (short)-CB_TURN_STEP;
                CB_AGGRO = 0;
                return;
            }
        }
        cerberus_consider_attack();
    } else {
        ENTITY->action_ticks_counter--;
    }
    cerberus_set_turn_anim(CB_TURN_STEP);
}

// 0x004994f0 - strafe sub 1: accelerate out of the strafe. Once eight blocked
// frames have passed it switches to the lunge animation (10) and a NEGATIVE
// speed - the dog backs up before springing.
void cerberus_strafe_accel(void)
{
    if (CB_PHASE != 0) {
        cerberus_speed_up(0x32, 0x82);
    }
    if ((CB_PROBE & 0x11) == 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 4;
        CB_AIFLAGS &= ~CB_AI_ACTIVE;
        if ((CB_BEHFLAGS_B & 1) == 0) {
            CB_SWERVE = 0x28;
        }
    } else {
        short n = CB_AGGRO;
        CB_AGGRO = (short)(n + 1);
        if (n > 7) {
            ENTITY->action_behavior++;
            ENTITY->move_speed_current = 0xFF7E;   // -130
            ENTITY->animationId = 10;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            CB_AGGRO = 0;
            return;
        }
    }
    cerberus_set_turn_anim(CB_TURN_STEP);
}

// 0x004995d0 - strafe sub 2: the spring. Tracks the player through frames 8-14,
// coasts to a stop at 15, and hands back to the chase at 19.
void cerberus_strafe_spring(void)
{
    if (ENTITY->animation_frame_id < 0xF) {
        if (ENTITY->animation_frame_id > 7) {
            cerberus_slow_down(0x20, 0x28);
        }
        entity_rotate_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x40);
    } else {
        ENTITY->move_speed_current = 0;
        CB_TURN_STEP = 0;
    }
    if (ENTITY->animation_frame_id > 0x12) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 4;
    }
}

void (*const cerberus_strafe_tbl[3])(void) = {   // 0x004d48b8
    cerberus_strafe_hold,   // 0x00499410
    cerberus_strafe_accel,  // 0x004994f0
    cerberus_strafe_spring  // 0x004995d0
};

// ---------------------------------------------------------------------------
// cerberus_beh_strafe @ 0x004993a0 (states_table[9] = behaviour[4])
// ---------------------------------------------------------------------------
void cerberus_beh_strafe(void)
{
    ENTITY->angle = (short)(ENTITY->angle + CB_TURN_STEP);
    Add_speedXZ(0);
    cerberus_strafe_tbl[ENTITY->action_behavior]();
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        Snd_em(0);
    }
}

// ============================================================================
// Behaviour 5 - alert / bark (0x00499640)
// ============================================================================

// 0x004996a0 - sub 0: hold still until the player is inside 0x1964 (6500), is
// stuck in an unreachable pose, or the script forces it (behaviour_flags 11).
void cerberus_alert_wait(void)
{
    if (CB_DIST < 0x1964
        || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_A
        || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_B
        || (char)ENTITY->behavior_flags == 11) {
        ENTITY->action_behavior++;
        CB_SWERVE = 0;
        ENTITY->animationId = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0xF) + 0x20);
    }
}

// 0x00499740 - sub 1: head-track the player for the countdown, then move on to
// the stalk (behaviour_flags 12).
void cerberus_alert_track(void)
{
    cerberus_head_track();
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 12;
    }
}

// ---------------------------------------------------------------------------
// cerberus_beh_alert @ 0x00499640 (states_table[10] = behaviour[5])
// The animation frame id is saved across Joint_move and put back, so the pose
// advances its blend without ever advancing its frame - that is what holds the
// dog's "ears up, listening" pose still.
// ---------------------------------------------------------------------------
void cerberus_beh_alert(void)
{
    unsigned char savedFrame = ENTITY->animation_frame_id;
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    ENTITY->animation_frame_id = savedFrame;

    if (ENTITY->action_behavior == 0) {
        cerberus_alert_wait();
    } else {
        cerberus_alert_track();
    }
}

// ============================================================================
// Behaviour 6 - maul (0x00499780)
// The killing animation. The dog holds the player in place, drops their health
// to -1 on frame 42, and greys their model out on frame 102.
// ============================================================================
void cerberus_beh_maul(void)
{
    if (ENTITY->action_behavior > 1) {
        return;
    }
    if (ENTITY->action_behavior == 0) {
        ENTITY->action_behavior++;
        ENTITY->animationId = 8;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        ENTITY->internal_timer = 0;                 // word at +0x182
        *((unsigned char*)ENTITY + 0x183) = 0;
        g_playerEntity.flags |= 6;
        g_playerEntity.directionAngle = ENTITY->angle;
        snap_player_to_grab_position(&g_playerEntity);
        CB_SWERVE = 0;
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.animationId = 7;
        g_playerEntity.animFrameId = 2;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        ENTITY->move_speed_current = 0;
        return;
    }

    ENTITY->scaMatrixData.localMatrix.t[1] = 0;
    if (ENTITY->animation_frame_id < 0x14) {
        snap_player_to_grab_position(&g_playerEntity);
    }
    entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100) != 0) {
        ENTITY->action_behavior++;
    }

    char frame = (char)ENTITY->animation_frame_id;
    if (frame == 0x2A) {        // 42 - the kill
        g_playerEntity.health = -1;
    }
    if (frame == 0x28) {        // 40
        Snd_em(3);
    }
    if (frame == 0x5E) {        // 94
        Snd_em(7);
    }
    if (frame == 0x66) {        // 102 - the body goes grey
        CB_BLOOD = 6;
        // 0x00606060 is an IMMEDIATE tint, not a pointer.
        JointStruct* pj = g_playerEntity.jointsStructs;
        JointApplyColorTint(pj,       0x30, 0x80820, (void*)0x00606060);
        JointApplyColorTint(pj + 1,   0x30, 0x80820, (void*)0x00606060);
        JointApplyColorTint(pj + 9,   0x30, 0x80820, (void*)0x00606060);
        JointApplyColorTint(pj + 0xC, 0x30, 0x80820, (void*)0x00606060);
        JointApplyColorTint((JointStruct*)((int)ENTITY->jointsStructs + 0x1F0),
                            0x30, 0x80820, (void*)0x00606060);
        Play3DSnd(3, 3, 0, (int)g_playerEntity.scaMatrixData.localMatrix.t);
    }

    cerberus_spawn_blood(&g_playerEntity, 1, 3);
    cerberus_spawn_blood(ENTITY, 3, 0);
}

// ============================================================================
// Behaviour 7 - bite (0x004999d0)
// The short close-range snap. Sub 0 waits for the player to come inside 0x1195
// (4501) - or be unreachable - then advances the animation by one and runs the
// head track until frame 33 hands back to the chase.
// ============================================================================
void cerberus_beh_bite(void)
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);

    if (ENTITY->action_behavior == 0) {
        if (CB_DIST < 0x1195
            || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_A
            || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_B) {
            ENTITY->action_behavior++;
            ENTITY->animationId++;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 0xF;
            CB_SWERVE = 0;
        }
        return;
    }

    cerberus_head_track();
    if ((char)ENTITY->animation_frame_id == 0x21) {   // 33
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 4;
        ENTITY->hit_state = 0;
    }
}

// ============================================================================
// Behaviour 8 - SCD idle (0x00499ab0)
// behavior_flags 0x80 means the room script owns the dog. It just animates and
// re-arms the selector every frame, so the moment the script clears 0x80 the AI
// picks up from behaviour 0.
// ============================================================================
void cerberus_beh_scd(void)
{
    ENTITY->ignore_player_flag = 0;
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
}

// ============================================================================
// Behaviour 9 - stalk (0x00499d90), its four sub-states (0x004d48d8) and the
// five-step wall-follow sequence (0x004d48e8)
// ============================================================================

// 0x0049a240 - wall-follow step 1: wait for the forward probe to trip, then
// advance. Returns 0 while clear, the turn delta once blocked - which is what
// cerberus_wall_follow_step reduces to a sign.
short cerberus_wall_step_wait(void)
{
    short result = 0;
    if ((CB_PROBE & 1) != 0) {
        ENTITY->action_state++;
        result = CB_TURN_DELTA;
    }
    if (CB_PHASE > 8) {
        CB_PHASE = 8;
    }
    return result;
}

// 0x0049a1d0 - wall-follow step 0: pick the direction to turn - toward the
// player, or the caller's default step if already aligned - then fall straight
// through into step 1 (`CALL 0x0049a240` with the result still in EAX).
short cerberus_wall_step_pick(short step)
{
    ENTITY->action_state++;
    CB_PHASE = 0;
    CB_TURN_DELTA = (short)turn_toward_target(
        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, step);
    if (CB_TURN_DELTA == 0) {
        CB_TURN_DELTA = step;
    }
    CB_AIFLAGS &= ~CB_AI_WALL;
    return cerberus_wall_step_wait();
}

// 0x0049a280 - step 2: turn along the wall until the probe history clears
// completely (all four low bits), then latch the escape direction.
short cerberus_wall_step_slide(void)
{
    if ((CB_PROBE & 0xF) == 0) {
        ENTITY->action_state++;
        ENTITY->action_ticks_counter = 0;
        char dir = cerberus_probe_both_turns(2, 1);
        CB_TURN_STEP = (short)dir;
        if (CB_TURN_STEP == 0) {
            CB_TURN_STEP = -1;
        }
    }
    ENTITY->angle = (short)(ENTITY->angle + CB_TURN_DELTA);
    if (CB_PHASE > 8) {
        CB_PHASE = 8;
    }
    return CB_TURN_DELTA;
}

// 0x0049a310 - step 3: hold the escape heading until the probe says it is
// really clear, then commit to it for 0x400 of yaw.
short cerberus_wall_step_commit(void)
{
    char blocked = (char)cerberus_probe_turn((short)(CB_TURN_STEP << 10), 1);
    if ((blocked != 0 || ENTITY->action_ticks_counter != 0) && (CB_PROBE & 1) == 0) {
        if ((short)ENTITY->action_ticks_counter > 0) {
            ENTITY->action_ticks_counter--;
        }
        return 0;
    }
    ENTITY->action_state = 4;
    CB_TURN_DELTA = CB_TURN_STEP;
    ENTITY->action_ticks_counter = 0x400;
    return 0;
}

// 0x0049a3a0 - step 4: burn the 0x400 budget down at `step` per frame, then go
// back to step 3 for a short re-check.
short cerberus_wall_step_run(short step)
{
    ENTITY->angle = (short)(ENTITY->angle + CB_TURN_DELTA * step);
    ENTITY->action_ticks_counter =
        (unsigned short)((short)ENTITY->action_ticks_counter - step);
    if ((short)ENTITY->action_ticks_counter < 1) {
        ENTITY->action_state = 3;
        ENTITY->action_ticks_counter = 4;
    }
    return CB_TURN_DELTA;
}

// ---------------------------------------------------------------------------
// cerberus_wall_follow_step @ 0x0049a130
// One tick of the 0x004d48e8 sequence, dispatched on action_state. The
// sub-step's return value is reduced to a SIGN and shifted into 0/1/2, which
// cerberus_chase_turn maps back to -1/0/+1 for cerberus_set_turn_anim.
//
// When the direct line to the player is clear and the sequence has been running
// more than 12 frames, it stops turning and raises CB_AI_WALL - "this wall does
// not go anywhere, give up".
// ---------------------------------------------------------------------------
short cerberus_wall_follow_step(short phaseLimit, short probeSpeed)
{
    short sub;
    switch (ENTITY->action_state) {
    case 0:  sub = cerberus_wall_step_pick(probeSpeed);   break;
    case 1:  sub = cerberus_wall_step_wait();             break;
    case 2:  sub = cerberus_wall_step_slide();            break;
    case 3:  sub = cerberus_wall_step_commit();           break;
    default: sub = cerberus_wall_step_run(probeSpeed);    break;
    }

    if (cerberus_probe_toward_player(4) == 0 && CB_PHASE > 0xC) {
        CB_TURN_STEP = 0;
        CB_AIFLAGS |= CB_AI_WALL;
    }
    CB_PHASE++;
    if (phaseLimit < CB_PHASE) {
        ENTITY->action_state = 0;
    }

    if (sub == 0)     return 1;
    if (sub < 1)      return 0;   // `MOV EAX,-1 / INC AX` - AX comes out 0
    return 2;
}

// 0x00499f90 - stalk sub 0: pick an arc direction (matching the sign of the
// current swerve) and a dwell.
void cerberus_stalk_begin(void)
{
    ENTITY->action_behavior++;
    CB_TURN_DELTA = (short)((rand() & 7) + 0x10);
    CB_PHASE = 0;
    if (CB_SWERVE < 0) {
        CB_TURN_DELTA = (short)-CB_TURN_DELTA;
    }
    ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1F) + 0x2D);
}

// 0x0049a000 - stalk sub 1: swing along the arc for the dwell.
void cerberus_stalk_arc(void)
{
    ENTITY->angle = (short)(ENTITY->angle + CB_TURN_DELTA);
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        ENTITY->action_behavior++;
        // Two rand() draws: `(rand & 7) - (rand & 0xF) + 30`.
        unsigned short a = (unsigned short)(rand() & 7);
        unsigned short b = (unsigned short)(rand() & 0xF);
        ENTITY->action_ticks_counter = (unsigned short)(a - b + 0x1E);
    }
}

// 0x0049a060 - stalk sub 2: pause, then re-face the player and growl (about one
// draw in four).
void cerberus_stalk_reface(void)
{
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter != 0 && (CB_PROBE & 1) == 0) {
        return;
    }
    CB_TURN_DELTA = (short)turn_toward_target(
        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
        (short)((rand() & 7) + 0x10));
    ENTITY->action_ticks_counter = 2;
    ENTITY->action_behavior = 1;
    ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1F) + 0x2D);
    if ((unsigned char)(rand() & 0x3F) > 0x30) {
        Snd_em(2);   // growl
    }
}

// 0x0049a100 - stalk sub 3: wall-follow around an obstacle; drop back to sub 0
// as soon as the sequence gives up.
void cerberus_stalk_wallfollow(void)
{
    cerberus_wall_follow_step(600, 0x10);
    if ((CB_AIFLAGS & CB_AI_WALL) != 0) {
        ENTITY->action_behavior = 0;
    }
}

void (*const cerberus_stalk_tbl[4])(void) = {   // 0x004d48d8
    cerberus_stalk_begin,      // 0x00499f90
    cerberus_stalk_arc,        // 0x0049a000
    cerberus_stalk_reface,     // 0x0049a060
    cerberus_stalk_wallfollow  // 0x0049a100
};

// ---------------------------------------------------------------------------
// cerberus_beh_stalk @ 0x00499d90 (states_table[14] = behaviour[9])
// The circling / working-up-to-it behaviour. CB_AGGRO is the patience meter:
// inside 2500 units it climbs (much faster while the player is running, and
// straight to 0x41 once the dog has given up on pathing); outside it decays. At
// 0x40 the dog commits to the chase, barking once per spawn via
// g_cerberusBarked.
// ---------------------------------------------------------------------------
void cerberus_beh_stalk(void)
{
    cerberus_speed_up(2, 0x28);
    Add_speedXZ(0);
    cerberus_stalk_tbl[ENTITY->action_behavior]();
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
    if (ENTITY->animation_frame_id == 0 || ENTITY->animation_frame_id == 0x1B) {
        Snd_em(9);   // footfall
    }
    cerberus_head_track();

    if (cerberus_probe_toward_player(8) != 0 && ENTITY->action_behavior != 3) {
        ENTITY->action_behavior = 3;
        ENTITY->action_state = 0;
    }

    if (CB_DIST < 0x9C4) {          // 2500
        CB_AGGRO++;
        if ((CB_BEHFLAGS_B & 4) != 0) {
            CB_AGGRO = 0x41;        // already gave up on pathing - commit now
        }
        if (g_playerEntity.move_speed_current > 200) {
            CB_AGGRO += 0x14;       // a running player draws it in much faster
        }
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 128) == 0
            && CB_DIST > 800) {
            ENTITY->ignore_player_flag = 0;
            ENTITY->behavior_flags = 5;   // lined up: take the running leap
            g_cerberusBarked = 1;
        }
    } else {
        if (CB_AGGRO > 0) {
            CB_AGGRO--;
        }
        if ((short)turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x80) == 0
            && g_playerEntity.move_speed_current > 200) {
            CB_AGGRO += 6;
        }
    }

    if (CB_AGGRO > 0x40
        || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_A
        || PLAYER_ANIM_WORD == PLAYER_UNREACHABLE_B
        || (g_cerberusBarked != 0 && (CB_BEHFLAGS_B & 4) == 0)) {
        ENTITY->ignore_player_flag = 0;
        ENTITY->behavior_flags = 4;
        CB_SWERVE = 0;
        if (g_cerberusBarked == 0) {
            Snd_em(6);
            g_cerberusBarked = 1;
        }
    }
}

// ============================================================================
// State 2 - cerberus_damaged @ 0x0049a400
// ============================================================================
void cerberus_damaged(void)
{
    ENTITY->status_flags &= 0x1F;
    if ((CB_AIFLAGS & CB_AI_HURT) == 0) {
        ENTITY->status_flags |= 0x40;
    }
    entity_check_visual_range(5000);

    switch (ENTITY->ignore_player_flag) {
    case 0: {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        CB_AIFLAGS |= CB_AI_HURT;

        unsigned char beh = ENTITY->behavior_flags;
        // `CMP BL,4 / JC` then `CMP BL,7 / JBE` - unsigned 4..7, or exactly 9.
        // Those are the mid-air and strafe behaviours: a hit there does not
        // knock the dog down, it only plays the stumble.
        if (!((beh >= 4 && beh <= 7) || beh == 9) && (ENTITY->hit_state & 1) != 0) {
            ENTITY->ignore_player_flag = 1;
            ENTITY->animationId = 9;
        } else {
            // Recoil away from the direction the player is facing, plus jitter.
            CB_TURN_DELTA = (short)((g_playerEntity.directionAngle & 0xFFF)
                                    - (ENTITY->angle & 0xFFF));
            CB_TURN_DELTA = (short)(CB_TURN_DELTA + cerberus_recoil_angle_tbl[rand() & 7]);

            if (beh == 4 || (CB_ALERT & 0x7F) == 0) {
                ENTITY->ignore_player_flag = 3;
                ENTITY->animationId = 0x0C;
                ENTITY->blend_counter = 0;
                ENTITY->move_speed_current = 0xF0;
                Snd_em(1);
            } else {
                ENTITY->ignore_player_flag = 2;
                ENTITY->animationId = 0x0B;
                ENTITY->move_speed_current =
                    (unsigned short)((short)ENTITY->move_speed_current >> 1);
                CB_ALERT |= 0x80;
            }
        }
        ENTITY->action_behavior = 0;
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        Snd_em(8);   // yelp
        return;      // skips the special-weapon tail
    }

    case 1: {
        // The plain stumble. CB_SWERVE takes Joint_move's completion flag.
        CB_SWERVE = (short)(unsigned short)Joint_move(
            0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (ENTITY->action_behavior == 0) {
            // 0x0049a620: once the animation ends, switch to the get-up pose.
            if (CB_SWERVE != 0) {
                ENTITY->action_behavior++;
                ENTITY->animationId = 10;
                ENTITY->timing_control = 0;
                ENTITY->move_speed_current = 0x82;
            }
        } else {
            // 0x0049a660: get up, tracking the player through frames 8-14.
            if (ENTITY->animation_frame_id < 0xF) {
                if (ENTITY->animation_frame_id > 7) {
                    cerberus_slow_down(0x20, 0x28);
                }
                Add_speedXZ(0x800);
                entity_rotate_toward_target(
                    (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x40);
            }
            if (CB_SWERVE != 0) {
                ENTITY->state = 1;
                ENTITY->ignore_player_flag = 0;
                ENTITY->behavior_flags = 4;
                ENTITY->hit_state = 0;
            }
        }
        break;
    }

    case 2:
        // Knocked into the air: ballistic until Y comes back to the floor.
        Add_speedXZ((int)CB_TURN_DELTA);
        entity_ballistic_step(0, CB_LAUNCH_VY,
                              (short)ENTITY->reaction_timer, 0);
        if (ENTITY->scaMatrixData.localMatrix.t[1] >= 0) {
            ENTITY->ignore_player_flag++;
            ENTITY->animationId++;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 0;
            ENTITY->scaMatrixData.localMatrix.t[1] = 0;
            Snd_em(1);
        }
        if (ENTITY->animation_frame_id != 0) {
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
        }
        return;      // skips the special-weapon tail

    case 3:
        // The skid along the floor, then either straight back up (already
        // facing the player) or through the roll-over animation first.
        if (ENTITY->animationId == 0x0C) {
            Add_speedXZ((int)CB_TURN_DELTA);
            cerberus_slow_down(10, 0);
        }
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->animationId++;
            ENTITY->hit_state = 0;
            if (ENTITY->animationId == 0x0E) {
                CB_AIFLAGS &= ~CB_AI_HURT;
                if ((short)turn_toward_target(
                        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200) == 0) {
                    ENTITY->ignore_player_flag++;
                    ENTITY->animationId = 3;
                } else {
                    ENTITY->ignore_player_flag = 1;
                    ENTITY->action_behavior = 1;
                    ENTITY->animationId = 10;
                    ENTITY->move_speed_current = 0x82;
                }
            }
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 3;
        }
        break;

    case 4:
        if (ENTITY->animation_frame_id == 10 && ENTITY->timing_control == 1) {
            Snd_em(2);
        }
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->state = 1;
            ENTITY->ignore_player_flag = 0;
            ENTITY->behavior_flags = 4;
            ENTITY->hit_state = 0;
            CB_ALERT &= 0x7F;
            Snd_em(6);
        }
        break;

    default:
        break;
    }

    // `JMP 0x0043d8a0` at 0x0049a92e - a tail call, so only the cases that fall
    // out of the switch reach it.
    zombie_check_special_weapon();
}

// ============================================================================
// State 3 - cerberus_die @ 0x0049a940
// ============================================================================
void cerberus_die(void)
{
    switch (ENTITY->ignore_player_flag) {
    case 0: {
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 7;
        ENTITY->action_behavior = 0;
        CB_TURN_DELTA = (short)((g_playerEntity.directionAngle & 0xFFF)
                                - (ENTITY->angle & 0xFFF));

        if (ENTITY->behavior_flags == 4 || (CB_ALERT & 0x7F) == 0) {
            ENTITY->ignore_player_flag = 2;
            ENTITY->animationId = 0x0C;
            ENTITY->blend_counter = 0;
            ENTITY->move_speed_current = 0xF0;
        } else {
            ENTITY->ignore_player_flag = 1;
            ENTITY->animationId = 0x0B;
            ENTITY->move_speed_current =
                (unsigned short)((short)ENTITY->move_speed_current >> 1);
        }
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
        Snd_em(4);   // death yelp
        return;
    }

    case 1:
        Add_speedXZ((int)CB_TURN_DELTA);
        entity_ballistic_step(0, CB_LAUNCH_VY,
                              (short)ENTITY->reaction_timer, 0);
        if (ENTITY->scaMatrixData.localMatrix.t[1] >= 0) {
            ENTITY->ignore_player_flag++;
            ENTITY->animationId++;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 0;
            ENTITY->scaMatrixData.localMatrix.t[1] = 0;
            Snd_em(1);
        }
        if (ENTITY->animation_frame_id != 0) {
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);
        }
        return;

    case 2:
        if (ENTITY->animationId == 0x0C) {
            Add_speedXZ((int)CB_TURN_DELTA);
            cerberus_slow_down(10, 0);
        }
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
            ENTITY->animationId++;
            ENTITY->timing_control = 0;
        }
        if (ENTITY->animationId == 0x0D) {
            ENTITY->ignore_player_flag++;
            // DECIMAL 10 = bits 1 and 3: bit 1 is the "intangible" bit
            // ResolveEntityScaCollision tests, bit 3 marks the entity dead.
            ENTITY->status_flags |= 10;
            ENTITY->move_speed_current = 0;
            ENTITY->action_ticks_counter = 0x46;   // 70 frames of shadow fade
            // 0x00ffff50 and -90 are IMMEDIATES, not pointers.
            BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00FFFF50);
            BillboardAdjSize(&ENTITY->pushVelocity, -90, -90);
        }
        return;

    case 3:
        // Grow the corpse shadow back out over 70 frames.
        if ((short)ENTITY->action_ticks_counter == 0) {
            ENTITY->ignore_player_flag++;   // -> state 4's bare RET
            return;
        }
        BillboardAdjSize(&ENTITY->pushVelocity, 6, 6);
        ENTITY->action_ticks_counter--;
        return;

    default:
        return;
    }
}

// ============================================================================
// State 4 and behaviour 0 - bare RETs (0x0049ab90 / 0x0049aba0)
// State 4 is where cerberus_die parks the corpse; behaviour 0 is unreachable,
// because cerberus_behavior_run re-runs the selector whenever the behaviour
// index is 0.
// ============================================================================
void cerberus_no_action(void) { }
void cerberus_beh_none(void)  { }

} // namespace

// ============================================================================
// cerberus_update @ 0x00497fb0
// enemies_update_functions_tbl[2]. Runs the state machine, resolves collision,
// ages the probe-history word and submits the ground shadow.
// ============================================================================
void cerberus_update(void)
{
    // Latched before the state runs, so the "did not move" test below compares
    // against the position at the top of the frame.
    int startX = ENTITY->scaMatrixData.localMatrix.t[0];
    int startZ = ENTITY->scaMatrixData.localMatrix.t[2];

    if ((g_message_flags & 0x0004) != 0) {
        // `CALL [ECX*4 + 0x4d4810]` - unconditional, no NULL test, exactly like
        // the zombie's.
        ((void(*)())cerberus_states_table[ENTITY->state])();

        SetEntityScaHitData(ENTITY);
        ResolveEntityScaCollision((Entity*)&g_playerEntity, ENTITY);
        HandleEnemyPlayerCollisions();

        // Age the probe history: this frame's low three bits shift up into
        // bits 1-3, and everything above bit 3 is dropped.
        CB_PROBE = (short)((CB_PROBE & 7) * 2);

        if ((ENTITY->status_flags & 4) == 0) {
            // On the ground: re-run the forward probe into bit 0.
            char blocked = (char)cerberus_probe_ahead();
            CB_PROBE |= (short)blocked;
        } else {
            // Airborne: no probe, just mirror the world position into the
            // 16-bit position triple the animator reads.
            ENTITY->position.x = (short)ENTITY->scaMatrixData.localMatrix.t[0];
            ENTITY->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
            ENTITY->position.z = (short)ENTITY->scaMatrixData.localMatrix.t[2];
        }
    }

    // Bit 4 = "I tried to move and went nowhere". Only meaningful when the
    // forward probe did NOT already say blocked.
    if ((CB_PROBE & 1) == 0
        && ENTITY->scaMatrixData.localMatrix.t[0] == startX
        && ENTITY->scaMatrixData.localMatrix.t[2] == startZ) {
        CB_PROBE |= 0x10;
    }

    // `MOV dword ptr [EAX+0x1c],0x0` - the whole scaMatrixData.field_00 dword.
    *(int*)(&ENTITY->scaMatrixData) = 0;

    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t, g_CurrentRdtDataTypePtr);

    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite(
            (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t,
            (short*)&ENTITY->pushVelocity,
            0,
            ENTITY->angle);
    }
}
