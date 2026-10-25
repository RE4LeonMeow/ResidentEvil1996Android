// Yawn.cpp - Yawn, the giant snake boss (enemy types 13 and 18).
//
// Original PC addresses:
//   yawn_update              0x004051e0   per-frame entry (table entry 13 & 18)
//   state table              0x004b19e0   6 entries, indexed by ENTITY->state
//     [0] yawn_init          0x00404cf0
//     [1] yawn_state_check   0x00405690
//     [2] yawn_damaged       0x00405750  -> yawn_damaged_run 0x00407af0
//     [3] yawn_die           0x004057b0  -> yawn_die_run     0x00407e00
//     [4] yawn_state_wait    0x004057d0
//     [5] NULL
//   action table             0x004b19f8   10 entries, indexed by action_behavior
//     [0] idle 0x00405bf0   [1] move    0x00405ca0   [2] bite    0x00405e00
//     [3] rear 0x00406360   [4] swallow 0x00406570   [5] entry   0x00406df0
//     [6] emerge 0x00407030 [7] flee    0x004075c0   [8] reposition 0x00407920
//     [9] NULL
//   selector table           0x004b1a20   7 entries, indexed by behavior_flags & 7
//     [0]/[2] yawn_pick_action_normal   0x00405860
//     [4]/[6] yawn_pick_action_scripted 0x00405a80
//     odd entries are NULL - behavior_flags & 0x0F is only ever 0, 2, 4 or 6.
//   yawn_check_actions       0x004057f0   tail-jump into the action table
//   yawn_select_behavior     0x00405810   distance calc + selector dispatch
//   yawn_turn_accel          0x00407d50   shared "swim" acceleration tail
//   yawn_anim_advance        0x00408e00   Yawn's OWN skeleton animator
//   yawn_pose_init           0x004094f0   first-frame pose (init only)
//   yawn_post_move           0x004096f0   collision + follow-the-leader chain
//   yawn_chain_follow        0x004089b0   drag one segment behind the previous
//   yawn_chain_align         0x00408c50   yaw-only align of a segment pair
//   yawn_spawn_dust          0x004098d0   ground dust puff off the head matrix
//   Sca_info record          0x004b19b0   (pointer at 0x004b19bc)
//   capture matrix           0x004b19c0   swallow-attack player transform
//
// ---------------------------------------------------------------------------
// How Yawn is built
// ---------------------------------------------------------------------------
// Yawn is ONE model with fifteen joints and THIRTEEN enemy slots.  yawn_init
// runs on the head (g_EnemiesList[0]) and memcpy's it into g_EnemiesList[1..12],
// giving each copy behavior_flags = 1 and scd_target_ptr = &joints[2 + k].
// update_entities therefore ticks all thirteen; yawn_update branches on
// behavior_flags:
//
//   behavior_flags & 1 == 0   the HEAD.  Runs the state machine, the action
//                             table and the animator.
//   behavior_flags == 1       a SEGMENT.  Skips the state machine entirely and
//                             instead mirrors its joint's world position into
//                             its own position, takes room collision at that
//                             point, and on a hit drags every joint from its
//                             own index to 14 along behind it.
//
// That is what makes the body collide with walls per-segment while only one
// skeleton is animated.
//
// The head's joint chain is built in <<9 fixed point inside yawn_anim_advance
// and converted back at the end of it, anchored so joint 14 lands on joint 13's
// previous world position.  Nothing else in the game does this, which is why
// Yawn carries its own animator instead of using Joint_move.
//
// ---------------------------------------------------------------------------
// Field map (raw offsets - Yawn reuses the generic movement bytes at other
// widths, exactly like Plant 42, so naming them through Entity would lie)
// ---------------------------------------------------------------------------
//   0x16C  signed char   turn acceleration, |value| doubles until it hits 0x40
//   0x16E  byte          FORM: 0 = juvenile (first fight), 1 = grown (rematch)
//   0x16F  byte          consecutive-bite counter (>4 forces a rear-up)
//   0x170  short         model scale ramp (swallow grab / death shrink)
//   0x172  short         current XZ speed, driven by yawn_turn_accel
//   0x174  short         ground Y captured from joints[7] at init
//   0x176  short         death shrink ramp
//   0x178  dword         saved state word, restored when a hit reaction ends
//   0x17C  signed char   turn direction during a bite; 0xFF right after a hit
//   0x17D  byte          post-bite recovery timer (bit 7 = "was interrupted")
//   0x17E  byte          waypoint index into the patrol / flee / retreat paths
//   0x17F  byte          "player is nearly dead" flag - forces the rear-up
//   0x180  ushort        death effect timer
//   0x182  byte          wall-stuck counter (>0x96 forces a reposition)
//   0x183  byte          wall-stuck cooldown
//   0x184  byte          hiss/dust timer
//   0x185  byte          bite cooldown, decremented by yawn_update
#include "EntityCommon.h"
#include "../../Globals.h"
#include "../BioCard.h"
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Yawn hardcodes a 15-joint skeleton (joints[0..14]) in three places, exactly
// as the original does - the real Yawn EMD has 15 joints, confirmed at runtime.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Engine dependencies
// ---------------------------------------------------------------------------
extern void ResetJointTransforms(void);                                   // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);                  // 0x00473ef0
extern unsigned int Flg_ck(int baseAddr, unsigned int bitIndex);          // 0x00473f40
// 0x00473b10 - see the note in Plant42.cpp: p1..p3 are SIGNED 16-bit tint
// deltas and p4/p5 are 16-bit queue words.  Declaring any of them as bytes
// silently truncates the 0x200 fade parameter.
extern void scd_model_tint_apply(short p1, short p2, short p3,
                                 unsigned short p4, unsigned short p5,
                                 unsigned char p6);                       // 0x00473b10
extern int  player_distance_z;                                            // 0x00be0de4
extern int  g_scaled_down_dist;                                           // 0x00be0de8
extern unsigned int g_entity_bkp;                                         // 0x00be0df4
// Joint_move (0x0048b700) and EntityComputeJointWorldMatrices (0x0048c190) are
// declared in Globals.h - do NOT redeclare them here with wider parameter
// types. `Joint_move(int, ..., int)` mangles to a different symbol than the
// real `Joint_move(char, ..., short)` and would link as a missing overload.
extern int  is_entity_in_switch_zone(VECTOR* position, void* zoneData);   // 0x00462d90 - Room.cpp

// g_tempVar (0x00be0df8) is declared `void*` in Globals.h because the joint
// code stores a pointer there.  Yawn uses the same slot as a loop counter -
// the original does too - so alias it rather than adding a second global.
#define YAWN_TMP (*(int*)&g_tempVar)

// ============================================================================
// Static tables mined from .rdata
// ============================================================================
namespace {

// 0x004b19b0 - Yawn's SCA record.  ENTITY->Sca_info points at 0x004b19bc,
// which holds the address of this block; field [5] (0x0320 = 800) is the
// collision radius every check_room_collision call reads back as
// *(short *)(Sca_info + 10).
//
// NOT const: this becomes ENTITY->Sca_info, which is handed to the engine's
// SCA collision code. The original's copy lives in writable .data, so a
// read-only copy here would fault the moment anything wrote back through it.
short s_yawnScaInfo[6] = {
    (short)0x8000, 0, (short)0xf830, 0, 0x07d0, 0x0320
};

// 0x004b1a3c - bite damage window, two bytes per form: {first frame, length}.
// The test is `(unsigned char)(animation_frame_id - start) < length`, so the
// grown form bites later and for one frame less.
const unsigned char s_yawnBiteWindow[2][2] = {
    { 13, 6 },   // form 0 - juvenile
    { 18, 5 },   // form 1 - grown
};

// 0x004b1a40 - patrol path walked by action 5 (the scripted entry crawl).
const short s_yawnEntryPath[6][2] = {
    {  4500, 24500 }, { 11000, 24500 }, { 10500, 14000 },
    {  8500, 11500 }, {  6800, 14200 }, {  3800,  8400 },
};

// 0x004b1a58 - flee path walked by action 7 (low health, snake leaves).
// Step 9 raises behavior_flags 0x80, which parks the state machine in state 4.
const short s_yawnFleePath[10][2] = {
    {  7300, 10000 }, { 12000, 12000 }, { 11000, 15500 }, { 11000, 25000 },
    { 11000, 25000 }, {  6500, 25000 }, {  2500, 26000 }, {  2500, 31000 },
    { 15000, 31000 }, {     0,     0 },
};

// 0x004b1a80 - reposition path walked by action 8 (wall-stuck recovery).
const short s_yawnRepositionPath[4][2] = {
    { 6500,  7500 }, { 11500, 19500 }, { 7500, 24500 }, { 11500, 24500 },
};

// 0x004b1a98 - dust-puff offsets, in the head joint's local frame.  The Z
// component is mirrored at random so the two sides of the body kick up dust.
const short s_yawnDustOffset[3][2] = {
    { 300, 400 }, { 600, 100 }, { 800, 50 },
};

} // namespace

// 0x004b19c0 - the swallow-attack capture matrix.  Its translation
// (0x004b19d4/d8/dc) is stepped every frame while the player is in Yawn's
// mouth, so this has to stay one object.  Also read by the swallowed player's
// own animation handler (0x00408900, PlayerAnimations.cpp), which recomposes
// the player matrix from it during Yawn's chew - so it is NOT file-static.
MATRIX g_yawnCaptureMatrix = {};

namespace {

// ---------------------------------------------------------------------------
// Raw field access
// ---------------------------------------------------------------------------
inline signed char&    eb (void* e, unsigned o) { return *reinterpret_cast<signed char*>((char*)e + o); }
inline unsigned char&  eub(void* e, unsigned o) { return *reinterpret_cast<unsigned char*>((char*)e + o); }
inline short&          ew (void* e, unsigned o) { return *reinterpret_cast<short*>((char*)e + o); }
inline unsigned short& euw(void* e, unsigned o) { return *reinterpret_cast<unsigned short*>((char*)e + o); }
inline int&            ei (void* e, unsigned o) { return *reinterpret_cast<int*>((char*)e + o); }
inline unsigned int&   eu (void* e, unsigned o) { return *reinterpret_cast<unsigned int*>((char*)e + o); }

// Entity+0x84 is written as one DWORD everywhere Yawn changes state:
// state | ignore_player<<8 | action_behavior<<16 | action_state<<24.
inline void set_state_word(unsigned int v) { eu(ENTITY, 0x84) = v; }

// action_behavior + action_state as ONE 16-bit store.  Several sites do
// `*(short *)(ENTITY + 0x86) = 1`, which also clears action_state - dropping
// that would leave the next behaviour running from whatever sub-state the
// previous one ended in.
inline void set_action(unsigned char behavior, unsigned char state)
{
    eub(ENTITY, 0x86) = behavior;
    eub(ENTITY, 0x87) = state;
}

// Joint helpers.  Joints are 0x7C bytes: transform at +0x24, world at +0x44,
// world translation at +0x58.
inline JointStruct* jnt(int n)   { return ENTITY->jointsStructs + n; }
inline MATRIX*      jw(int n)    { return &ENTITY->jointsStructs[n].world; }
inline int*         jwt(int n)   { return ENTITY->jointsStructs[n].world.t; }

inline VECTOR* entity_pos(void)      { return (VECTOR*)ENTITY->scaMatrixData.localMatrix.t; }
inline VECTOR* player_pos_vec(void)  { return (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t; }

// The dead-move matrix (0x00d1fdd0) HOLDS a pointer - see the note in
// CharacterNpc.cpp.  Its translation block at +0x14 is the room-space anchor
// every Yawn billboard is spawned from.
inline const int* dead_move_pos(void) { return (const int*)((char*)g_deadMoveValue + 0x14); }

// The original inlines this 32-byte copy at four sites (`REP MOVSD ECX=8`).
inline void copy_dead_move_matrix(void)
{
    memcpy(&g_matrixScratch, (const void*)g_deadMoveValue, sizeof(MATRIX));
}

// 0x0040a250 - transpose the 3x3 short block of a MATRIX.  MainMenu.cpp and
// Plant42.cpp each keep a file-static copy; Yawn carries its own for the same
// reason (they are all static in the original translation units).
void matrix_to_short_array(const MATRIX* src, MATRIX* dst)
{
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            ((short*)dst)[j * 3 + i] = src->m[i][j];
}

// 0x0048c530 - build a capture matrix that expresses the player's transform in
// a joint's local frame.  Plant 42 has the same call with the output hard-wired
// to its own matrix; this is the general three-argument form Yawn uses.
void yawn_capture_setup(const MATRIX* joint, MATRIX* playerMtx, MATRIX* out)
{
    MATRIX transposed;
    matrix_to_short_array(joint, &transposed);
    MulMatrix0(&transposed, playerMtx, out);

    VECTOR rel;
    rel.x = playerMtx->t[0] - joint->t[0];
    rel.y = playerMtx->t[1] - joint->t[1];
    rel.z = playerMtx->t[2] - joint->t[2];
    ApplyMatrixLV(&transposed, &rel, &rel);
    out->t[0] = rel.x;
    out->t[1] = rel.y;
    out->t[2] = rel.z;
}

// ============================================================================
// yawn_chain_follow @ 0x004089b0
// Drag `seg` along behind `lead`.  Both are JointStructs.
//
// The rest position is 34/35 of the lead's local offset (`/ 0x23 * 0x22`),
// which is what gives the body its slight compression when the head stops.  If
// the segment is further than 50 units from that point it re-aims its yaw at
// the lead, clamped to +/- `angleStep`, and - for joints 0..4 only - is kept
// within 90 degrees of the lead (joint 3 measures against the HEAD entity's
// yaw at 0x00be64d8 instead, because joint 3 is what the head model rides on).
// ============================================================================
void yawn_chain_follow(JointStruct* lead, JointStruct* seg, short angleStep)
{
    g_playerDisplacement = (int)seg->rotation.y;

    g_playerPosScratch.x = seg->transform.t[0] << 12;
    g_playerPosScratch.y = seg->transform.t[1] << 12;
    g_playerPosScratch.z = seg->transform.t[2] << 12;
    ApplyMatrixLV(&lead->world, &g_playerPosScratch, &g_playerPosScratch);

    // The rotated offset is kept at full precision for the final write; only
    // the copy used for the distance test gets the 34/35 shrink.
    const int rotX = g_playerPosScratch.x;
    const int rotY = g_playerPosScratch.y;
    const int rotZ = g_playerPosScratch.z;

    g_playerPosScratch.x = (g_playerPosScratch.x / 0x23) * 0x22 + lead->world.t[0] * 0x1000;
    g_playerPosScratch.y = (g_playerPosScratch.y / 0x23) * 0x22 + lead->world.t[1] * 0x1000;
    g_playerPosScratch.z = (g_playerPosScratch.z / 0x23) * 0x22 + lead->world.t[2] * 0x1000;

    const int leadX = lead->world.t[0];
    const int leadY = lead->world.t[1];
    const int leadZ = lead->world.t[2];

    int dx = g_playerPosScratch.x - seg->world.t[0] * 0x1000;
    int dz = g_playerPosScratch.z - seg->world.t[2] * 0x1000;

    int hx = dx >> 12;
    int hz = dz >> 12;
    unsigned short dist = (unsigned short)SquareRoot0(hz * hz + hx * hx);

    if (dist >= 0x32) {
        dx >>= 4;
        dz >>= 4;

        if (dx == 0) {
            seg->rotation.y = (short)((dz > 0 ? 1 : 0) * 0x800 + 0x400);
        } else {
            short q = (short)GetAngleQuadrantValue((dz << 12) / dx);
            seg->rotation.y = (short)((2 - (dx < 0 ? 1 : 0)) * 0x800 - q);
        }

        const short prevAngle = (short)g_playerDisplacement;
        player_distance_z = ((seg->rotation.y - g_playerDisplacement) + angleStep) & 0xFFF;

        if (angleStep * 2 < player_distance_z) {
            seg->rotation.y = (short)(prevAngle - angleStep);
            if (angleStep + 0x800 >= player_distance_z) {
                seg->rotation.y = (short)(prevAngle + angleStep);
            }
        } else if (ew(g_EnemiesList, 0xc2) < 100) {
            // The HEAD's move_speed_current, absolute - this is Yawn-specific
            // code, so slot 0 is always the head.  While it is nearly stopped
            // the segment snaps to the lead instead of easing.
            seg->rotation.y = prevAngle;
        }

        if (seg->index < 5) {
            short ref = lead->rotation.y;
            if (seg->index == 3) {
                ref = (short)g_EnemiesList[0].angle;
            }
            unsigned int rel = ((int)seg->rotation.y - (int)ref + 0x400) & 0xFFF;
            if (rel > 0x800) {
                seg->rotation.y = (short)(ref - 0x400);
                if (rel < 0xC00) {
                    seg->rotation.y = (short)(ref + 0x400);
                }
            }
        }
    }

    copy_dead_move_matrix();
    RotMatrix(&seg->rotation, &seg->world);

    // The final three writes read `[ESP+0x1c]`, `[ESP+0x18]` and `[ESP+0x1c]`
    // again - but the first of them runs with two arguments still pushed for
    // the RotMatrix call above, so it resolves to the X slot, not the Z one.
    // This is x/y/z, not x/y/x; see the stack-offset push-shift note in the
    // project memory before "fixing" it.
    seg->world.t[0] = (rotX + leadX * 0x1000) >> 12;
    seg->world.t[1] = (rotY + leadY * 0x1000) >> 12;
    seg->world.t[2] = (rotZ + leadZ * 0x1000) >> 12;
}

// ============================================================================
// yawn_chain_align @ 0x00408c50
// Yaw-only version of the above, used once per frame on the joint immediately
// ahead of a moving segment.  It turns the LEAD toward the segment (up to 0x30
// per frame) instead of turning the segment, which is what lets a body segment
// that has been pushed off a wall drag the neck around with it.
// ============================================================================
void yawn_chain_align(JointStruct* lead, JointStruct* seg)
{
    g_playerPosScratch.x = seg->transform.t[0] << 12;
    g_playerPosScratch.y = seg->transform.t[1] << 12;
    g_playerPosScratch.z = seg->transform.t[2] << 12;
    ApplyMatrixLV(&lead->world, &g_playerPosScratch, &g_playerPosScratch);

    g_playerPosScratch.x = (g_playerPosScratch.x / 0x23) * 0x22 + lead->world.t[0] * 0x1000;
    g_playerPosScratch.y = (g_playerPosScratch.y / 0x23) * 0x22 + lead->world.t[1] * 0x1000;
    g_playerPosScratch.z = (g_playerPosScratch.z / 0x23) * 0x22 + lead->world.t[2] * 0x1000;

    int dx = (g_playerPosScratch.x - seg->world.t[0] * 0x1000) >> 4;
    int dz = (g_playerPosScratch.z - seg->world.t[2] * 0x1000) >> 4;

    if (dx == 0) {
        player_distance_z = (dz > 0 ? 1 : 0) * 0x800 + 0x400;
    } else {
        int q = GetAngleQuadrantValue((dz << 12) / dx);
        player_distance_z = (2 - (dx < 0 ? 1 : 0)) * 0x800 - q;
    }

    unsigned int delta = (unsigned int)(player_distance_z - (int)seg->rotation.y);
    if (((delta + 0x800) & 0xFFF) < 0x800) {
        player_distance_z = ((int)seg->rotation.y - player_distance_z) & 0xFFF;
        if (player_distance_z > 0x30) player_distance_z = 0x30;
        lead->rotation.y = (short)(lead->rotation.y - (short)player_distance_z);
    } else {
        player_distance_z = (int)(delta & 0xFFF);
        if (player_distance_z > 0x30) player_distance_z = 0x30;
        lead->rotation.y = (short)(lead->rotation.y + (short)player_distance_z);
    }

    copy_dead_move_matrix();
    RotMatrix(&lead->rotation, &lead->world);
}

// ============================================================================
// yawn_anim_advance @ 0x00408e00
// Yawn's own skeleton animator.  Returns 1 on the frame the animation wraps,
// which every caller adds straight into action_state.
//
// `reverse` plays the frame list backwards (used by the swallow animation);
// `blendStep` is 0x100 or 0x200 and divides 0x1000 into the blend weight.
//
// Three things are Yawn-specific:
//   * Joints 0-2 are posed from the animation directly; joints 3-14 stack their
//     yaw and roll onto the previous joint, so the body is a chain, not a tree.
//   * The whole chain is accumulated in <<9 fixed point and converted back at
//     the end, which keeps a 15-link body from drifting apart.
//   * The final offset anchors joint 14 onto joint 13's PREVIOUS world position
//     and the body's Y onto the ground height captured at init (+0x174).  The
//     leftover XZ delta is what actually moves the entity.
// ============================================================================
unsigned char yawn_anim_advance(unsigned char reverse, short blendStep)
{
    unsigned char* timing = (unsigned char*)ENTITY + 0xbf;
    if (*timing > 1) {
        *timing = (unsigned char)(*timing - 1);
        return 0;
    }

    int animHeader = (int)ENTITY->animHeader;
    g_playerDisplacement = (int)(*(short*)(animHeader + 6) / 2);

    unsigned short* slot = (unsigned short*)(ENTITY->animBase + (unsigned int)eub(ENTITY, 0xbd) * 4);
    int frames = (int)((slot[1] & ~3u) + ENTITY->animBase);

    unsigned short* frame;
    if (reverse == 0) {
        frame = (unsigned short*)(frames + (unsigned int)eub(ENTITY, 0xbe) * 4);
    } else {
        frame = (unsigned short*)(frames - 4 + ((unsigned int)slot[0] - eub(ENTITY, 0xbe)) * 4);
    }

    char* j = (char*)ENTITY->jointsStructs;

    int poseBase = animHeader
                 + (short)((int)((int)*(short*)(animHeader + 2)
                                 + ((int)*(short*)(animHeader + 2) >> 31 & 3u)) >> 2) * 4
                 + (unsigned int)frame[0] * g_playerDisplacement * 2;
    short* pose = (short*)(poseBase + 0xc);

    *(int*)(j + 0x3c) = (int)*(short*)(poseBase + 2);   // joints[0].transform.t[1]

    g_entity_bkp = (unsigned int)eub(ENTITY, 0x8c);     // blend counter
    YAWN_TMP = 2;

    // ---- joints 0..2: posed straight from the animation ----
    char* cur = j;
    if (g_entity_bkp == 0) {
        do {
            SVECTOR* rot = (SVECTOR*)(cur + 4);
            rot->x = pose[0];
            rot->y = pose[1];
            rot->z = pose[2];
            pose += 3;
            MATRIX* m = (MATRIX*)(cur + 0x24);
            cur += 0x7c;
            RotMatrix(rot, m);
        } while (YAWN_TMP-- != 0);
    } else {
        int step = (int)blendStep;
        do {
            g_svecScratch.x = pose[0];
            g_svecScratch.y = pose[1];
            g_svecScratch.z = pose[2];
            pose += 3;
            SVECTOR* rot = (SVECTOR*)(cur + 4);
            fp_lerp(rot, &g_svecScratch, g_entity_bkp * step,
                    ((int)(0x1000 / step) - (int)g_entity_bkp) * step, rot);
            RotMatrix(rot, (MATRIX*)(cur + 0x24));
            cur += 0x7c;
        } while (YAWN_TMP-- != 0);
    }
    // `cur` now points at joints[3].

    RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &ENTITY->scaMatrixData.localMatrix);
    ApplyLVAndMul0Matrix(&ENTITY->scaMatrixData.localMatrix, cur - 0x150, cur - 0x130);
    ApplyLVAndMul0Matrix(cur - 0x130, cur - 0x58, cur - 0x38);

    // Joint 2's world translation moves into <<9 space; the rest of the chain
    // is accumulated there and converted back at the very end.
    *(int*)(cur - 0x24) = *(int*)(cur - 0x24) << 9;
    *(int*)(cur - 0x20) = *(int*)(cur - 0x20) << 9;
    *(int*)(cur - 0x1c) = *(int*)(cur - 0x1c) << 9;

    // Joint 13's world position and the init ground height, read BEFORE the
    // chain is rebuilt - they are the anchor the body is snapped onto below.
    const int anchorX = *(int*)(cur + 0x530);
    const int anchorZ = *(int*)(cur + 0x538);
    const short anchorY = ew(ENTITY, 0x174);

    // ---- joint 3: stacks onto joints 0 and 2 ----
    if (g_entity_bkp == 0) {
        g_animFrameIdSave = (unsigned int)(int)pose[1];
        if (eub(ENTITY, 0xbe) == 0) *(short*)(cur + 0x72) = pose[1];
        *(short*)(cur + 6) = (short)(*(short*)(cur + 6)
                                     + ((short)g_animFrameIdSave - *(short*)(cur + 0x72)));
        *(short*)(cur + 0x72) = (short)g_animFrameIdSave;
        *(short*)(cur + 8) = (short)(*(short*)(cur - 0x16c) + *(short*)(cur - 0x74) + pose[2]);
    } else {
        g_svecScratch.x = 0;
        g_animFrameIdSave = (unsigned int)(int)pose[1];
        if (eub(ENTITY, 0xbe) == 0) *(short*)(cur + 0x72) = pose[1];
        g_svecScratch.y = (short)((*(short*)(cur + 6) - *(short*)(cur + 0x72))
                                  + (short)g_animFrameIdSave);
        *(short*)(cur + 0x72) = (short)g_animFrameIdSave;
        g_svecScratch.z = (short)(*(short*)(cur - 0x16c) + *(short*)(cur - 0x74) + pose[2]);
        int step = (int)blendStep;
        fp_lerp((SVECTOR*)(cur + 4), &g_svecScratch, g_entity_bkp * step,
                ((int)(0x1000 / step) - (int)g_entity_bkp) * step, (SVECTOR*)(cur + 4));
    }
    pose += 3;

    g_playerPosScratch.x = *(int*)(cur + 0x38) << 9;
    g_playerPosScratch.y = *(int*)(cur + 0x3c) << 9;
    g_playerPosScratch.z = *(int*)(cur + 0x40) << 9;
    RotMatrix((SVECTOR*)(cur + 4), (MATRIX*)(cur + 0x44));
    ApplyMatrixLV((MATRIX*)(cur - 0x38), &g_playerPosScratch, &g_playerPosScratch);
    *(int*)(cur + 0x58) = *(int*)(cur - 0x24) + g_playerPosScratch.x;
    *(int*)(cur + 0x5c) = *(int*)(cur - 0x20) + g_playerPosScratch.y;
    *(int*)(cur + 0x60) = *(int*)(cur - 0x1c) + g_playerPosScratch.z;

    // ---- joints 4..14 ----
    char* seg = cur + 0x7c;
    char* end = seg;
    YAWN_TMP = 10;
    if (g_entity_bkp == 0) {
        do {
            g_animFrameIdSave = (unsigned int)(int)pose[1];
            if (eub(ENTITY, 0xbe) == 0) *(short*)(seg + 0x72) = pose[1];
            *(short*)(seg + 6) = (short)(*(short*)(seg + 6)
                                         + ((short)g_animFrameIdSave - *(short*)(seg + 0x72)));
            *(short*)(seg + 0x72) = (short)g_animFrameIdSave;
            *(short*)(seg + 8) = (short)(*(short*)(seg - 0x74) + pose[2]);
            g_playerPosScratch.x = *(int*)(seg + 0x38) << 9;
            g_playerPosScratch.y = *(int*)(seg + 0x3c) << 9;
            g_playerPosScratch.z = *(int*)(seg + 0x40) << 9;
            if (YAWN_TMP == 7) *(short*)(seg + 8) = 0;   // joint 7 stays level
            end = seg + 0x7c;
            RotMatrix((SVECTOR*)(seg + 4), (MATRIX*)(seg + 0x44));
            ApplyMatrixLV((MATRIX*)(seg - 0x38), &g_playerPosScratch, &g_playerPosScratch);
            *(int*)(seg + 0x58) = *(int*)(seg - 0x24) + g_playerPosScratch.x;
            *(int*)(seg + 0x5c) = *(int*)(seg - 0x20) + g_playerPosScratch.y;
            *(int*)(seg + 0x60) = *(int*)(seg - 0x1c) + g_playerPosScratch.z;
            seg = end;
            pose += 3;
        } while (YAWN_TMP-- != 0);
    } else {
        int step = (int)blendStep;
        do {
            g_svecScratch.x = 0;
            g_animFrameIdSave = (unsigned int)(int)pose[1];
            if (eub(ENTITY, 0xbe) == 0) *(short*)(seg + 0x72) = pose[1];
            SVECTOR* rot = (SVECTOR*)(seg + 4);
            g_svecScratch.y = (short)((*(short*)(seg + 6) - *(short*)(seg + 0x72))
                                      + (short)g_animFrameIdSave);
            *(short*)(seg + 0x72) = (short)g_animFrameIdSave;
            g_svecScratch.z = (short)(*(short*)(seg - 0x74) + pose[2]);
            fp_lerp(rot, &g_svecScratch, g_entity_bkp * step,
                    ((int)(0x1000 / step) - (int)g_entity_bkp) * step, rot);
            g_playerPosScratch.x = *(int*)(seg + 0x38) << 9;
            g_playerPosScratch.y = *(int*)(seg + 0x3c) << 9;
            g_playerPosScratch.z = *(int*)(seg + 0x40) << 9;
            if (YAWN_TMP == 7) *(short*)(seg + 8) = 0;
            end = seg + 0x7c;
            RotMatrix(rot, (MATRIX*)(seg + 0x44));
            ApplyMatrixLV((MATRIX*)(seg - 0x38), &g_playerPosScratch, &g_playerPosScratch);
            *(int*)(seg + 0x58) = *(int*)(seg - 0x24) + g_playerPosScratch.x;
            *(int*)(seg + 0x5c) = *(int*)(seg - 0x20) + g_playerPosScratch.y;
            *(int*)(seg + 0x60) = *(int*)(seg - 0x1c) + g_playerPosScratch.z;
            seg = end;
            pose += 3;
        } while (YAWN_TMP-- != 0);
        eb(ENTITY, 0x8c) = (signed char)(eb(ENTITY, 0x8c) - 1);
    }
    // `end` now points one past joints[14].

    // ---- snap the body onto its anchor and convert back out of <<9 ----
    int offX = anchorX * 0x200 - *(int*)(end - 0xa0);
    int offY = anchorY * 0x200 - *(int*)(end - 0x9c);
    int offZ = anchorZ * 0x200 - *(int*)(end - 0x98);

    YAWN_TMP = 0xb;
    char* back = end;
    do {
        *(int*)(back - 0x24) = (*(int*)(back - 0x24) + offX) >> 9;
        *(int*)(back - 0x20) = (*(int*)(back - 0x20) + offY) >> 9;
        *(int*)(back - 0x1c) = (*(int*)(back - 0x1c) + offZ) >> 9;
        back -= 0x7c;
    } while (YAWN_TMP-- != 0);


    ENTITY->scaMatrixData.localMatrix.t[0] += (offX >> 9);
    ENTITY->scaMatrixData.localMatrix.t[2] += (offZ >> 9);
    ENTITY->jointsStructs[0].transform.t[1] += (offY >> 9);

    eub(ENTITY, 0xbf) = (unsigned char)frame[1];
    eub(ENTITY, 0xbe) = (unsigned char)(eub(ENTITY, 0xbe) + 1);
    if ((int)eub(ENTITY, 0xbe) <= (int)(slot[0] - 1)) {
        return 0;
    }
    eub(ENTITY, 0xbe) = 0;
    return 1;
}

// ============================================================================
// yawn_pose_init @ 0x004094f0
// One-shot pose used by yawn_init before the copies are made.  Same layout as
// the animator, minus the blending and the <<9 anchoring - the world matrices
// only need to be valid enough for the copies to pick up their joint positions.
// ============================================================================
void yawn_pose_init(void)
{
    int animHeader = (int)ENTITY->animHeader;
    g_playerDisplacement = (int)(*(short*)(animHeader + 6) / 2);

    char* j = (char*)ENTITY->jointsStructs;

    unsigned short frameOff = *(unsigned short*)(ENTITY->animBase + 2
                                                 + (unsigned int)eub(ENTITY, 0xbd) * 4);
    unsigned short frameIdx = *(unsigned short*)((frameOff & ~3u)
                                                 + (unsigned int)eub(ENTITY, 0xbe) * 4
                                                 + ENTITY->animBase);
    int poseBase = animHeader
                 + (short)((int)((int)*(short*)(animHeader + 2)
                                 + ((int)*(short*)(animHeader + 2) >> 31 & 3u)) >> 2) * 4
                 + (unsigned int)frameIdx * g_playerDisplacement * 2;

    *(int*)(j + 0x3c) = (int)*(short*)(poseBase + 2);
    short* pose = (short*)(poseBase + 0xc);

    unsigned char count = ENTITY->jointCount;
    if (count == 0) return;

    char* cur = j;
    for (unsigned char i = 0; i < count; i++) {
        if (i < 3) {
            ((SVECTOR*)(cur + 4))->x = pose[0];
            *(short*)(cur + 6) = pose[1];
            MATRIX* m = (MATRIX*)(cur + 0x24);
            *(short*)(cur + 8) = pose[2];
            RotMatrix((SVECTOR*)(cur + 4), m);
            if (i == 0) {
                RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &ENTITY->scaMatrixData.localMatrix);
                ApplyLVAndMul0Matrix(&ENTITY->scaMatrixData.localMatrix, m, cur + 0x44);
            }
            if ((i & 2) != 0) {
                ApplyLVAndMul0Matrix(cur - 0xb4, m, cur + 0x44);
            }
        } else {
            g_animFrameIdSave = (unsigned int)(int)pose[1];
            *(short*)(cur + 0x72) = pose[1];
            short roll;
            if (i == 3) {
                *(short*)(cur + 6) = (short)(ENTITY->angle + (short)g_animFrameIdSave);
                roll = (short)(*(short*)(cur - 0x16c) + *(short*)(cur - 0x74));
            } else {
                *(short*)(cur + 6) = (short)(*(short*)(cur - 0x76) + (short)g_animFrameIdSave);
                roll = *(short*)(cur - 0x74);
            }
            *(short*)(cur + 8) = (short)(roll + pose[2]);
            if (i == 7) *(short*)(cur + 8) = 0;
            RotMatrix((SVECTOR*)(cur + 4), (MATRIX*)(cur + 0x44));
            ApplyMatrixLV((MATRIX*)(cur - 0x38), (VECTOR*)(cur + 0x38), &g_playerPosScratch);
            *(int*)(cur + 0x58) = *(int*)(cur - 0x24) + g_playerPosScratch.x;
            *(int*)(cur + 0x5c) = *(int*)(cur - 0x20) + g_playerPosScratch.y;
            *(int*)(cur + 0x60) = *(int*)(cur - 0x1c) + g_playerPosScratch.z;
        }
        pose += 3;
        cur += 0x7c;
    }
}

// ============================================================================
// yawn_post_move @ 0x004096f0
// End-of-frame work for the HEAD: player collision (skipped for the three
// scripted behaviours), room collision with the wall-stuck counter, and the
// follow-the-leader pass over joints 2..14.
//
// The last four links only follow while the snake is actually moving
// (move_speed_current != 0) or shrinking on death (0x176 != 0), and they use
// half the angular step - that is what makes the tail lag behind.
// ============================================================================
void yawn_post_move(short angleStep)
{
    JointStruct* j = ENTITY->jointsStructs;

    unsigned char behavior = eub(ENTITY, 0x86);
    if (behavior != 5 && behavior != 6 && behavior != 7) {
        ResolveEntityScaCollision((Entity*)&g_playerEntity, ENTITY);
    }

    g_playerDisplacement = check_room_collision(entity_pos(),
                                                *(short*)(ENTITY->Sca_info + 10));

    if ((ENTITY->behavior_flags & 1) == 0) {
        if (g_playerDisplacement == 0) {
            if (eb(ENTITY, 0x183) != 0) eb(ENTITY, 0x183) = (signed char)(eb(ENTITY, 0x183) - 1);
        } else {
            eb(ENTITY, 0x182) = (signed char)(eb(ENTITY, 0x182) + 1);
            eub(ENTITY, 0x183) = 0x14;
        }
    }

    RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &ENTITY->scaMatrixData.localMatrix);
    ApplyLVAndMul0Matrix(&ENTITY->scaMatrixData.localMatrix, &j[0].transform, &j[0].world);
    ApplyLVAndMul0Matrix(&j[0].world, &j[2].transform, &j[2].world);

    for (int k = 2; k < 10; k++) {
        yawn_chain_follow(&j[k], &j[k + 1], angleStep);
    }

    if (euw(ENTITY, 0xc2) != 0 || ew(ENTITY, 0x176) != 0) {
        yawn_chain_follow(&j[10], &j[11], angleStep);
        yawn_chain_follow(&j[11], &j[12], angleStep);
        yawn_chain_follow(&j[12], &j[13], (short)(angleStep / 2));
        yawn_chain_follow(&j[13], &j[14], (short)(angleStep / 2));
    }
}

// ============================================================================
// yawn_spawn_dust @ 0x004098d0
// Ground dust puff, positioned in the head joint's frame from
// s_yawnDustOffset[side].  The Z offset is randomly mirrored so both sides of
// the body kick up dust as the snake slides.
// ============================================================================
void yawn_spawn_dust(int side)
{
    copy_dead_move_matrix();

    unsigned int r = (unsigned int)rand();
    g_matrixScratch.t[1] = 0x32;
    g_matrixScratch.t[2] = (int)(1 - (r & 2)) * (int)s_yawnDustOffset[side][1];
    g_matrixScratch.t[0] = (int)s_yawnDustOffset[side][0];

    Matrix_MulMatrix((MATRIX*)((char*)ENTITY->jointsStructs + 0xc0), &g_matrixScratch);

    g_playerPosScratch.x = g_matrixScratch.t[0];
    g_playerPosScratch.y = g_matrixScratch.t[1];
    g_playerPosScratch.z = g_matrixScratch.t[2];
    Effect_CreateBillboard(0, 4, 0, (void*)g_deadMoveValue, &g_playerPosScratch, 0x14);
}

// ============================================================================
// yawn_turn_accel @ 0x00407d50
// The "swim" acceleration shared by every moving behaviour.  Speed (+0x172) is
// integrated by an accelerator byte (+0x16C) whose magnitude DOUBLES every
// frame until it reaches 0x40; when the speed leaves the +/-900 band the
// accelerator is divided by 0x40, negated (so it flips to +/-1) and the slither
// sound plays.  That produces the snake's oscillating glide.
// ============================================================================
void yawn_turn_accel(void)
{
    if ((unsigned short)(ew(ENTITY, 0x172) + 900) > 0x708) {
        ew(ENTITY, 0x172) = (short)(ew(ENTITY, 0x172) - (short)eb(ENTITY, 0x16c));
        eb(ENTITY, 0x16c) = (signed char)(eb(ENTITY, 0x16c) / 0x40);
        eb(ENTITY, 0x16c) = (signed char)(-eb(ENTITY, 0x16c));
        Snd_em(0);
    }
    if ((signed char)(eb(ENTITY, 0x16c) % 0x40) != 0) {
        eb(ENTITY, 0x16c) = (signed char)(eb(ENTITY, 0x16c) * 2);
    }
    ew(ENTITY, 0x172) = (short)(ew(ENTITY, 0x172) + (short)eb(ENTITY, 0x16c));
}

// ============================================================================
// Action behaviour 0 - idle @ 0x00405bf0
// ============================================================================
void yawn_action_idle(void)
{
    if (eb(ENTITY, 0x87) == 0) {
        eb(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 1;
        eub(ENTITY, 0x8c) = 7;
        ew(ENTITY, 0x172) = 0;
        euw(ENTITY, 0xc4) = (unsigned short)(((unsigned short)rand() & 0xf) + 0xf);
    }

    yawn_anim_advance(0, 0x200);

    short ticks = ew(ENTITY, 0xc4);
    ew(ENTITY, 0xc4) = (short)(ticks - 1);
    if (ticks == 0) {
        set_action(1, 0);
    }

    yawn_post_move(0x30);
}

// ============================================================================
// Action behaviour 1 - move @ 0x00405ca0
// ============================================================================
void yawn_action_move(void)
{
    if (eb(ENTITY, 0x87) == 0) {
        eb(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eb(ENTITY, 0xbd) = (signed char)(eb(ENTITY, 0x16e) + 1);
        eub(ENTITY, 0x8c) = 7;
        ew(ENTITY, 0x172) = 0;
        euw(ENTITY, 0xc2) = 0xa0;

        // The grown form switches to the fast slither once the player is nearly
        // dead, has been chased off (0x17F) or is recovering from a bite.
        if (((g_playerEntity.health - 10 < 0) || eb(ENTITY, 0x17f) != 0
             || (eub(ENTITY, 0x17d) & 0x80) != 0)
            && eb(ENTITY, 0x16e) != 0) {
            eub(ENTITY, 0xbd) = 5;
        }
    }

    if ((eub(ENTITY, 0x17d) & 0x7f) != 0) {
        eub(ENTITY, 0x17d) = (unsigned char)(eub(ENTITY, 0x17d) - 1);
        if (eb(ENTITY, 0x17d) == 0) {
            eb(ENTITY, 0x17d) = 0;
            eub(ENTITY, 0x87) = 0;
        }
    }

    if (eb(ENTITY, 0xbd) == 5 && (eb(ENTITY, 0xbe) == 0x0f || eb(ENTITY, 0xbe) == 0x28)) {
        Snd_em(1);
    }

    yawn_anim_advance(0, 0x200);

    if (eb(ENTITY, 0x184) != 0 && (eub(ENTITY, 0xbe) & 7) == 0) {
        int r = rand();
        yawn_spawn_dust(r < 0 ? -(-r & 1) : (r & 1));
    }

    Add_speedXZ((int)ew(ENTITY, 0x172));
    yawn_post_move(0x30);
}

// ============================================================================
// Action behaviour 2 - bite @ 0x00405e00
//
// action_state 0/1 is the lunge, 2/3 the recoil-and-turn, 4/5 the miss recovery.
// The damage window is s_yawnBiteWindow[form]; a connect swaps the player into
// a damage animation, tints joint 1, spins Yawn 0x100 away and - for the first
// Yawn only, if the serum has not been used - poisons the player.
// ============================================================================
void yawn_action_bite(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0:
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0x8c) = 7;
        eb(ENTITY, 0xbd) = (signed char)(eb(ENTITY, 0x16e) * 10 + 3);
        euw(ENTITY, 0xc2) = 100;
        eb(ENTITY, 0x16f) = (signed char)(eb(ENTITY, 0x16f) + 1);
        eub(ENTITY, 0x17c) = 1;
        Snd_em(2);
        // fall through
    case 1: {
        const unsigned char* win = s_yawnBiteWindow[eub(ENTITY, 0x16e)];
        if ((unsigned char)(eub(ENTITY, 0xbe) - win[0]) < win[1]) {
            const int* dead = dead_move_pos();
            g_playerPosScratch.y   = dead[1];
            g_playerPosScratch.z   = dead[2];
            g_playerPosScratch.pad = dead[3];
            g_playerPosScratch.x   = 1000;   // 1000 units ahead of the head

            JointStruct* j = ENTITY->jointsStructs;
            player_distance_z = FUN_0048ae00(&j[0].world, &g_playerPosScratch, 800,
                                             g_playerEntity.scaMatrixData.localMatrix.t);

            // Already-wounded players get sprayed with blood on alternate frames.
            if (g_playerEntity.action_behavior > 0x65 && (eub(ENTITY, 0xbe) & 1) != 0) {
                Effect_CreateBillboard(0, 0, 0, (void*)((char*)j + 0xc0),
                                       &g_playerPosScratch, 0);
                g_playerPosScratch.x = 0;
                g_playerPosScratch.y = (int)eub(ENTITY, 0x16e) * -0x5dc - 300;
                Effect_CreateBillboard(0, 0, 0, &g_playerEntity.scaMatrixData.localMatrix,
                                       &g_playerPosScratch, 0);
            }

            if (player_distance_z != 0 && g_playerEntity.isBeingAttackedFlag == 0) {
                g_animFrameIdSave = is_facing_toward_entity(&g_playerEntity) & 0xff;

                if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                    g_playerEntity.health = (short)(g_playerEntity.health - 10);
                } else {
                    g_playerEntity.health = (short)(g_playerEntity.health - 0x1c);
                }
                if (g_playerEntity.health < 0) g_playerEntity.health = 1;

                g_playerEntity.isBeingAttackedFlag = (unsigned char)(g_animFrameIdSave + 1);
                g_playerEntity.action_behavior     = (unsigned char)(g_animFrameIdSave + 0x66);

                const int* d = dead_move_pos();
                g_playerPosScratch.y   = d[1];
                g_playerPosScratch.z   = d[2];
                g_playerPosScratch.pad = d[3];
                g_playerPosScratch.x   = 1000;

                j = ENTITY->jointsStructs;
                Effect_CreateBillboard(0, 0, 0, (void*)((char*)j + 0xc0),
                                       &g_playerPosScratch, 0);
                // &DAT_00606060 is the packed colour, not a pointer.
                JointApplyColorTint(&j[1], 0x30, 0x80820, (void*)0x00606060);
                ENTITY->angle = (short)(ENTITY->angle + 0x100);

                // Only the FIRST Yawn (entity id 13) poisons, and only if the
                // serum has not already been taken.
                if (ENTITY->id == 0x0d && Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_YAWN_SERUM) == 0) {
                    Flg_on((int)&g_ScenarioFlags2, SCENARIO2_FLAG_YAWN_POISONED);
                    g_playerEntity.healthStatusFlags |= 0x20;
                }

                eub(ENTITY, 0x17c) = 0xff;
                eub(ENTITY, 0x17d) = 0;
                eub(ENTITY, 0x182) = 0;
                eub(ENTITY, 0x184) = 0xf0;
                Snd_em(4);
            }
        }

        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87) + (signed char)yawn_anim_advance(0, 0x200));
        entity_rotate_toward_target(player_pos_vec(), 0x38);

        if ((int)eub(ENTITY, 0x16e) * 5 - (int)eub(ENTITY, 0xbe) == -8) {
            if (eub(ENTITY, 0x16e) == 0 || eub(ENTITY, 0x183) < 0xb) {
                euw(ENTITY, 0xc2) = 200;
                Snd_em(3);
            } else {
                // Grown form biting into a wall: abandon the lunge, back off.
                eub(ENTITY, 0x85) = 0;
                set_action(1, 0);
                eub(ENTITY, 0x17c) = 0;
                eub(ENTITY, 0x16f) = 5;
                eub(ENTITY, 0x8a) = 0;
                entity_rotate_toward_target(player_pos_vec(), 0xfff0);
            }
        }
        break;
    }
    case 2:
        euw(ENTITY, 0xc4) = 0x1e;
        euw(ENTITY, 0xc2) = 0x78;
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x16e) + 3);
        // fall through
    case 3: {
        entity_rotate_toward_target(player_pos_vec(),
                                    (unsigned short)((short)eb(ENTITY, 0x17c) << 4));
        short ticks = ew(ENTITY, 0xc4);
        ew(ENTITY, 0xc4) = (short)(ticks - 1);
        if (ticks == 0) {
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
            eub(ENTITY, 0x17c) = 0;
            eub(ENTITY, 0x8a) = 0;
        }
        break;
    }
    case 4:
        eub(ENTITY, 0x87) = 5;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 4;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0x78;
        // fall through
    case 5:
        entity_rotate_toward_target(player_pos_vec(),
                                    (unsigned short)((short)eb(ENTITY, 0x17c) << 4));
        if (yawn_anim_advance(0, 0x200) != 0) {
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
            eub(ENTITY, 0x17c) = 0;
            eub(ENTITY, 0x8a) = 0;
        }
        break;
    default:
        break;
    }

    Add_speedXZ(0);
    yawn_post_move(0x30);
}

// ============================================================================
// Action behaviour 3 - rear up / form change @ 0x00406360
//
// This is where the snake toggles form (+0x16E).  Rearing while form 0 raises
// it to 1 and goes straight to the hiss; rearing while form 1 drops back to 0
// and returns to the move behaviour.
// ============================================================================
void yawn_action_rear(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0:
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 4;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0x78;
        eub(ENTITY, 0x16f) = 0;
        if (eb(ENTITY, 0x17f) != 0 && eb(ENTITY, 0x16e) != 0) {
            eub(ENTITY, 0x87) = 3;
            break;
        }
        // fall through
    case 1:
        entity_rotate_toward_target(player_pos_vec(), 0x30);
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(eub(ENTITY, 0x16e), 0x200));
        break;
    case 2:
        if (eb(ENTITY, 0x16e) == 0) {
            eb(ENTITY, 0x16e) = 1;
            eub(ENTITY, 0x87) = 3;
        } else {
            eb(ENTITY, 0x16e) = 0;
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
        }
        break;
    case 3:
        eub(ENTITY, 0x87) = 4;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 5;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0x46;
        euw(ENTITY, 0xc4) = 0x3c;
        // fall through
    case 4: {
        entity_rotate_toward_target(player_pos_vec(), 0x30);
        yawn_anim_advance(eub(ENTITY, 0x16e), 0x200);
        if (eb(ENTITY, 0xbe) == 0x0f || eb(ENTITY, 0xbe) == 0x28) {
            Snd_em(1);
        }
        short ticks = ew(ENTITY, 0xc4);
        ew(ENTITY, 0xc4) = (short)(ticks - 1);
        if (ticks == 0) {
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
        }
        break;
    }
    default:
        break;
    }

    yawn_turn_accel();
    Add_speedXZ((int)ew(ENTITY, 0x172));
    yawn_post_move(0x30);
}

// ============================================================================
// Action behaviour 4 - swallow @ 0x00406570
//
// The instant-death grab.  action_state runs 0 -> 1 (the mouth-open lunge),
// then either releases (player got away) or goes to 2/3 (the player is held in
// the mouth by g_yawnCaptureMatrix and shaken), 4/5 (the swallow), 6/7 (the
// coil-and-settle).  Joint flags are cleared as body parts disappear.
// ============================================================================
void yawn_action_swallow(void)
{
    JointStruct* pj;

    switch (eub(ENTITY, 0x87)) {
    case 0:
        goto grab_start;

    case 1:
        goto grab_hold;

    case 2:
        eub(ENTITY, 0x87) = 3;
        yawn_capture_setup(&ENTITY->jointsStructs[0].world,
                           &g_playerEntity.scaMatrixData.localMatrix,
                           &g_yawnCaptureMatrix);
        g_svecScratch.x = 0;
        g_svecScratch.y = 0;
        g_svecScratch.z = -200;
        RotMatrix(&g_svecScratch, &g_matrixScratch);
        MulMatrixInPlace(&g_matrixScratch, &g_yawnCaptureMatrix);
        g_yawnCaptureMatrix.t[0] = 0x937;
        g_yawnCaptureMatrix.t[1] = 700;
        g_yawnCaptureMatrix.t[2] = 0;
        ew(ENTITY, 0x170) = 0x1388;
        // fall through
    case 3:
        g_playerEntity.zoneFlags |= 0x80;
        ApplyLVAndMul0Matrix(&ENTITY->jointsStructs[0].world, &g_yawnCaptureMatrix,
                             &g_playerEntity.scaMatrixData.localMatrix);
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + yawn_anim_advance(0, 0x200));

        if (eub(ENTITY, 0xbe) > 0x42 && eub(ENTITY, 0xbe) < 0x4b) {
            if (eub(ENTITY, 0xbe) == 0x43) Snd_em(6);
            if (eub(ENTITY, 0xbe) == 0x4a) { Snd_em(7); Snd_em(4); }

            pj = g_playerEntity.jointsStructs;
            g_yawnCaptureMatrix.t[0] -= 0x78;
            g_yawnCaptureMatrix.t[1] -= 0x28;
            pj[2].flags  &= 0xfe;
            pj[10].flags &= 0xfe;
            pj[13].flags &= 0xfe;
            pj[11].flags &= 0xfe;
            pj[14].flags &= 0xfe;

            {
                const int* d = dead_move_pos();
                g_playerPosScratch.x   = d[0];
                g_playerPosScratch.z   = d[2];
                g_playerPosScratch.pad = d[3];
                g_playerPosScratch.y   = 500;
            }
            Effect_CreateBillboard(0, 3, 0, &pj[0].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0, 0, &pj[0].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 3, 0, &pj[2].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0, 0, &pj[2].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0, 0, &pj[3].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0, 0, &pj[6].world, &g_playerPosScratch, 0);

            g_svecScratch.x = 0;
            g_svecScratch.z = 0;
            // Masked to 0x1ff afterwards, so the shift's signedness cannot show.
            g_svecScratch.y = (short)((((unsigned int)((int)ENTITY->angle
                                                       - (int)g_playerEntity.directionAngle)
                                        - 0x800u) >> 3) & 0x1ff);
            RotMatrix(&g_svecScratch, &g_matrixScratch);
            MulMatrix(&g_yawnCaptureMatrix, &g_matrixScratch);
            ew(ENTITY, 0x170) = (short)(ew(ENTITY, 0x170) + 100);
        }
        goto tail;

    case 4:
        eub(ENTITY, 0x87) = 5;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0x8c) = 7;
        eub(ENTITY, 0xbd) = 4;
        euw(ENTITY, 0xc2) = 0;
        g_playerEntity.health = -1;
        // fall through
    case 5:
        eub(ENTITY, 0x87) = (unsigned char)(eub(ENTITY, 0x87) + yawn_anim_advance(1, 0x200));
        Add_speedXZ(0);
        if (eub(ENTITY, 0xbe) == 0xf) {
            for (g_playerDisplacement = 0xc; ; g_playerDisplacement--) {
                int i = g_playerDisplacement;
                g_EnemiesList[i].status_flags |= 4;
                if (i == 0) break;
            }
        }
        goto tail;

    case 6:
        eub(ENTITY, 0x87) = 7;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0x8c) = 7;
        eub(ENTITY, 0xbd) = 1;
        euw(ENTITY, 0xc2) = 0;
        euw(ENTITY, 0xc4) = 0x96;
        ENTITY->status_flags |= 4;
        // fall through
    case 7: {
        g_yawnCaptureMatrix.t[0] -= 10;
        g_yawnCaptureMatrix.t[1] -= 6;
        yawn_anim_advance(0, 0x200);
        short ticks = ew(ENTITY, 0xc4);
        ew(ENTITY, 0xc4) = (short)(ticks - 1);
        if (ticks == 0) eub(ENTITY, 0x87) = 8;

        pj = g_playerEntity.jointsStructs;
        if (ew(ENTITY, 0xc4) == 0x6e) {
            pj[3].flags &= 0xfe;
            pj[4].flags &= 0xfe;
            pj[5].flags &= 0xfe;
            pj[6].flags &= 0xfe;
            pj[7].flags &= 0xfe;
            pj[8].flags &= 0xfe;
        }
        goto tail;
    }
    default:
        goto tail;
    }

grab_start:
    eub(ENTITY, 0x87) = 1;
    eub(ENTITY, 0xbe) = 0;
    eub(ENTITY, 0xbf) = 0;
    eub(ENTITY, 0x8c) = 7;
    eub(ENTITY, 0xbd) = 0x0e;
    eub(ENTITY, 0x16f) = (unsigned char)(eub(ENTITY, 0x16f) + 1);
    eub(ENTITY, 0x8a) = 1;
    ENTITY->status_flags |= 2;
    g_playerEntity.flags |= 6;
    g_playerEntity.animationId     = 7;
    g_playerEntity.animFrameId     = 0x0d;
    g_playerEntity.action_behavior = 0;
    g_playerEntity.action_state    = 0;
    eub(ENTITY, 0x185) = 0x1e;
    // fall through

grab_hold:
    yawn_anim_advance(0, 0x200);
    {
        int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
               - ENTITY->scaMatrixData.localMatrix.t[2];
        int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
               - ENTITY->scaMatrixData.localMatrix.t[0];
        int sz = dz >> 31;
        int sx = dx >> 31;
        g_playerDisplacement = (((dz ^ sz) - sz) - sx) + (dx ^ sx);
    }
    euw(ENTITY, 0xc2) = 0xa0;
    if (g_playerDisplacement < 0x9c4) {
        euw(ENTITY, 0xc2) = 0;
    }
    entity_rotate_toward_target(player_pos_vec(), 0x30);

    if (eub(ENTITY, 0xbe) == 10) Snd_em(3);

    if (eub(ENTITY, 0xbe) == 0x14) {
        eub(ENTITY, 0x8a) = 0;
        if (g_playerDisplacement > 2000) {
            // Player broke free before the mouth closed.
            g_playerEntity.animationId     = 1;
            g_playerEntity.animFrameId     = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
            g_playerEntity.flags &= 0xf9;
            g_playerEntity.isBeingAttackedFlag = 0;
            goto tail;
        }

        eub(ENTITY, 0x87) = 2;
        for (g_entity_bkp = 0xc; ; g_entity_bkp--) {
            unsigned int i = g_entity_bkp;
            g_EnemiesList[i].status_flags |= 4;
            if (i == 0) break;
        }
        pj = g_playerEntity.jointsStructs;
        pj[0].flags  &= 0xfe;
        pj[1].flags  &= 0xfe;
        pj[9].flags  &= 0xfe;
        pj[12].flags &= 0xfe;
        euw(ENTITY, 0xc2) = 0;

        {
            const int* d = dead_move_pos();
            g_playerPosScratch.x   = d[0];
            g_playerPosScratch.z   = d[2];
            g_playerPosScratch.pad = d[3];
            g_playerPosScratch.y   = 1000;
        }
        Effect_CreateBillboard(0, 3, 0, &pj[0].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, &pj[0].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 3, 0, &pj[2].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, &pj[2].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, &pj[3].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, &pj[6].world, &g_playerPosScratch, 0);
        for (int k = 3; k <= 8; k++) {
            JointApplyColorTint(&pj[k], 0x70, 0x484860, (void*)0x00a0a0a0);
        }
        Snd_em(5);
        Play3DSnd(2, (g_playerEntity.id & 1) + 0x17, 0,
                  (int)ENTITY->scaMatrixData.localMatrix.t);
    }
    Add_speedXZ(0);

tail:
    yawn_post_move(0x30);

    if (eub(ENTITY, 0x87) > 1) {
        if ((rand() & 1) == 0) yawn_spawn_dust(0);
        if ((rand() & 1) == 0) yawn_spawn_dust(1);
        if ((rand() & 1) == 0) yawn_spawn_dust(2);
    }

    if (ew(ENTITY, 0x170) != 0) {
        char* j = (char*)ENTITY->jointsStructs;
        g_playerPosScratch.x = ew(ENTITY, 0x170) - 300;
        g_playerPosScratch.y = g_playerPosScratch.x;
        g_playerPosScratch.z = g_playerPosScratch.x;
        ScaleMatrixCols((MATRIX*)(j + 0x1b8), &g_playerPosScratch);   // joints[3].world
        g_playerPosScratch.x = (int)ew(ENTITY, 0x170);
        g_playerPosScratch.y = g_playerPosScratch.x;
        g_playerPosScratch.z = g_playerPosScratch.x;
        ScaleMatrixCols((MATRIX*)(j + 0x11c), &g_playerPosScratch);   // joints[2].transform
    }
}

// ============================================================================
// Action behaviour 5 - scripted entry crawl @ 0x00406df0
//
// Walks s_yawnEntryPath, hands over to behaviour 6 (the emerge/hiss) at the
// end, and rains ambient dust in one of two room-sized boxes depending on
// behavior_flags & 0x3F.
// ============================================================================
void yawn_action_entry(void)
{
    if (eb(ENTITY, 0x87) == 0) {
        eb(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 1;
        eub(ENTITY, 0x8c) = 7;
        ew(ENTITY, 0x172) = 0;
        euw(ENTITY, 0xc2) = 0xbe;
        if (eb(ENTITY, 2) == 4) {
            euw(ENTITY, 0xc2) = 0x82;
            eub(ENTITY, 0x17e) = 3;
        }
        euw(ENTITY, 0xc4) = 2;
        eub(ENTITY, 0x8a) = 1;
        if (eb(ENTITY, 1) == 0x0d) {
            Flg_on((int)&g_ScenarioFlags, SCENARIO_FLAG_YAWN_BITE);
        }
    }

    {
        const short* wp = s_yawnEntryPath[eub(ENTITY, 0x17e)];
        g_playerPosScratch.y = 0;
        g_playerPosScratch.x = (int)(unsigned short)wp[0];
        g_playerPosScratch.z = (int)(unsigned short)wp[1];
    }
    entity_rotate_toward_target(&g_playerPosScratch, 0x30);

    int dz = g_playerPosScratch.z - ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = g_playerPosScratch.x - ENTITY->scaMatrixData.localMatrix.t[0];
    if (SquareRoot0(dz * dz + dx * dx) < 500) {
        short ticks = ew(ENTITY, 0xc4);
        ew(ENTITY, 0xc4) = (short)(ticks - 1);
        if (ticks == 0) {
            set_action(6, 0);
        }
        eb(ENTITY, 0x17e) = (signed char)(eb(ENTITY, 0x17e) + 1);
        Snd_em(0);
    }

    if ((eub(ENTITY, 2) & 0x3f) == 0) {
        g_playerPosScratch.x = ((unsigned int)rand() & 0x1ff) + 0x10cc;
        g_playerPosScratch.y = -(int)((unsigned int)rand() & 0x3ff);
        g_playerPosScratch.z = ((unsigned int)rand() & 0x3ff) + 0x5c94;
    } else {
        g_playerPosScratch.x = ((unsigned int)rand() & 0x1ff) + 0x2904;
        g_playerPosScratch.y = -(int)((unsigned int)rand() & 0x3ff);
        g_playerPosScratch.z = ((unsigned int)rand() & 0x3ff) + 0x2904;
    }
    Effect_CreateBillboard(9, 0x11, 0, (void*)g_deadMoveValue, &g_playerPosScratch, 0);

    yawn_anim_advance(0, 0x200);
    Add_speedXZ((int)ew(ENTITY, 0x172));
    yawn_post_move(0x30);
}

// ============================================================================
// Action behaviour 6 - emerge / hiss @ 0x00407030
//
// Raises the snake to form 1 and blows the ceiling dust.  A behavior_flags == 0
// Yawn skips straight to the hiss (state 6); the others play the rear-up first.
// ============================================================================
void yawn_action_emerge(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0:
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 4;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0;
        eub(ENTITY, 0x16e) = 1;
        eub(ENTITY, 0x17e) = 0;
        // fall through
    case 1:
        if (yawn_anim_advance(0, 0x200) != 0) {
            eub(ENTITY, 0x87) = 2;
            if (eb(ENTITY, 2) == 0) {
                eub(ENTITY, 0x87) = 6;
                return;
            }
        }
        break;

    case 2:
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0x8c) = 7;
        eb(ENTITY, 0xbd) = (signed char)(eb(ENTITY, 0x16e) * 10 + 3);
        // fall through
    case 3:
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(0, 0x200));

        if ((int)eub(ENTITY, 0x16e) * 5 - (int)eub(ENTITY, 0xbe) == -8) {
            Snd_em(3);
            euw(ENTITY, 0xc2) = 0xbe;
        }

        if (eub(ENTITY, 0xbe) > 0x17) {
            euw(ENTITY, 0xc2) = 0x32;
            // Ten debris puffs, all in the same box, cycling four sprite pairs.
            static const unsigned char kType[10]  = { 9, 9, 0x0e, 0x0e, 9, 0x0e, 0x0e, 0x0e, 0x0e, 0x11 };
            static const unsigned char kDepth[10] = { 0x13, 0x15, 0x13, 0x11, 0x15, 0x13, 0x11, 0x13, 0x11, 0x11 };
            g_playerPosScratch.y = 0;
            for (int i = 0; i < 10; i++) {
                g_playerPosScratch.x = ((unsigned int)rand() & 0x7ff) + 2000;
                g_playerPosScratch.z = ((unsigned int)rand() & 0x7ff) + 0xe74;
                Effect_CreateBillboard(kType[i], kDepth[i], 0, (void*)g_deadMoveValue,
                                       &g_playerPosScratch, 0);
            }
        }
        Add_speedXZ(0);
        yawn_post_move(0x30);
        return;

    case 4:
        eub(ENTITY, 0x87) = 5;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 4;
        eub(ENTITY, 0x8c) = 7;
        // fall through
    case 5:
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(0, 0x200));
        entity_rotate_toward_target(player_pos_vec(), 0x30);
        return;

    case 6:
        eub(ENTITY, 0x87) = 7;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 5;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0x46;
        euw(ENTITY, 0xc4) = 0x5a;
        // fall through
    case 7: {
        yawn_anim_advance(eub(ENTITY, 0x16e), 0x200);
        if (eb(ENTITY, 0xbe) == 0x0f || eb(ENTITY, 0xbe) == 0x28) {
            Snd_em(1);
        }
        short ticks = ew(ENTITY, 0xc4);
        ew(ENTITY, 0xc4) = (short)(ticks - 1);
        if (ticks == 0) {
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
            eub(ENTITY, 0x8a) = 0;
            for (g_playerDisplacement = 0xc; ; g_playerDisplacement--) {
                int i = g_playerDisplacement;
                g_EnemiesList[i].status_flags &= 0xfb;
                if (i == 0) break;
            }
        }
        break;
    }
    default:
        break;
    }
}

// ============================================================================
// Action behaviour 7 - flee @ 0x004075c0
//
// Low-health escape.  Picks the nearest entry point on s_yawnFleePath from the
// snake's current position, then walks it; segments are switched off in three
// batches as the body disappears into the hole, and step 9 raises
// behavior_flags 0x80 which parks the state machine in state 4 for good.
// ============================================================================
void yawn_action_flee(void)
{
    if (eb(ENTITY, 0x87) == 0) {
        eb(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 1;
        eub(ENTITY, 0x8c) = 0x0f;
        euw(ENTITY, 0xc2) = 0xb4;
        eub(ENTITY, 0x182) = 0;

        eub(ENTITY, 0x17e) = 4;
        if (ENTITY->scaMatrixData.localMatrix.t[0] < 9000
            && ENTITY->scaMatrixData.localMatrix.t[2] < 23000) {
            eub(ENTITY, 0x17e) = 3;
        }
        if (ENTITY->scaMatrixData.localMatrix.t[2] < 0x3c8c) eub(ENTITY, 0x17e) = 2;
        if (ENTITY->scaMatrixData.localMatrix.t[2] < 9000)   eub(ENTITY, 0x17e) = 0;
        if (ENTITY->scaMatrixData.localMatrix.t[0] > 0x2580) eub(ENTITY, 0x17e) = 1;

        eub(ENTITY, 0x16e) = 0;
        eub(ENTITY, 0x17c) = 1;
        eub(ENTITY, 0x16c) = 1;
        ew(ENTITY, 0x172) = 0;
        Flg_on((int)g_EnemiesFlags, eub(ENTITY, 0x163));
    }

    if (eub(ENTITY, 0x17e) > 6) {
        g_playerPosScratch.x = ((unsigned int)rand() & 0x1ff) + 0x10cc;
        g_playerPosScratch.y = -(int)((unsigned int)rand() & 0x3ff);
        g_playerPosScratch.z = ((unsigned int)rand() & 0x7ff) + 0x58ac;
        Effect_CreateBillboard(9, 0x11, 0, (void*)g_deadMoveValue, &g_playerPosScratch, 0);
    }

    {
        const short* wp = s_yawnFleePath[eub(ENTITY, 0x17e)];
        g_playerPosScratch.y = 0;
        g_playerPosScratch.x = (int)(unsigned short)wp[0];
        g_playerPosScratch.z = (int)(unsigned short)wp[1];
    }
    entity_rotate_toward_target(&g_playerPosScratch, 0x30);

    int dz = g_playerPosScratch.z - ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = g_playerPosScratch.x - ENTITY->scaMatrixData.localMatrix.t[0];
    if (SquareRoot0(dz * dz + dx * dx) < 800) {
        eb(ENTITY, 0x17e) = (signed char)(eb(ENTITY, 0x17e) + 1);
        if (eb(ENTITY, 0x17e) == 1) eb(ENTITY, 0x17e) = 2;
        ew(ENTITY, 0x172) = 0;

        if (eb(ENTITY, 0x17e) == 6) {
            for (g_playerDisplacement = 3; ; g_playerDisplacement--) {
                int i = g_playerDisplacement;
                g_EnemiesList[i].status_flags |= 4;
                if (i == 0) break;
            }
        }
        if (eb(ENTITY, 0x17e) == 7) {
            for (g_playerDisplacement = 3; ; g_playerDisplacement--) {
                int i = g_playerDisplacement;
                g_EnemiesList[i + 4].status_flags |= 4;
                if (i == 0) break;
            }
        }
        if (eb(ENTITY, 0x17e) == 8) {
            for (g_playerDisplacement = 4; ; g_playerDisplacement--) {
                int i = g_playerDisplacement;
                g_EnemiesList[i + 8].status_flags |= 4;
                if (i == 0) break;
            }
        }
        if (eb(ENTITY, 0x17e) == 9) {
            eub(ENTITY, 2) |= 0x80;
        }
    }

    if (eub(ENTITY, 0x182) > 0x96) {
        set_state_word(0x80101);
    }

    yawn_anim_advance(0, 0x100);
    if (eub(ENTITY, 0x17e) < 4) {
        yawn_turn_accel();
    }
    Add_speedXZ((int)ew(ENTITY, 0x172));
    yawn_post_move(0x38);
}

// ============================================================================
// Action behaviour 8 - reposition @ 0x00407920
//
// The wall-stuck recovery: pick the nearest of four open floor points, crawl
// to it, then drop back to state 1 / behaviour 1.  If the snake gets stuck
// AGAIN on the way (0x182 > 0x96) action_state resets and a new point is
// picked from the new position.
// ============================================================================
void yawn_action_reposition(void)
{
    if (eb(ENTITY, 0x87) == 0) {
        eb(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 1;
        eub(ENTITY, 0x8c) = 0x0f;
        euw(ENTITY, 0xc2) = 0xb4;
        eub(ENTITY, 0x182) = 0;

        eub(ENTITY, 0x17e) = 3;
        if (ENTITY->scaMatrixData.localMatrix.t[0] > 9000)  eub(ENTITY, 0x17e) = 2;
        if (ENTITY->scaMatrixData.localMatrix.t[2] < 19000) eub(ENTITY, 0x17e) = 1;
        if (ENTITY->scaMatrixData.localMatrix.t[2] < 15000) eub(ENTITY, 0x17e) = 0;

        eub(ENTITY, 0x16e) = 0;
        eub(ENTITY, 0x17c) = 1;
        eub(ENTITY, 0x16c) = 1;
        ew(ENTITY, 0x172) = 0;
    }

    if (eub(ENTITY, 0x182) > 0x96) {
        eub(ENTITY, 0x87) = 0;
    }

    {
        const short* wp = s_yawnRepositionPath[eub(ENTITY, 0x17e)];
        g_playerPosScratch.y = 0;
        g_playerPosScratch.x = (int)(unsigned short)wp[0];
        g_playerPosScratch.z = (int)(unsigned short)wp[1];
    }
    entity_rotate_toward_target(&g_playerPosScratch, 0x30);

    int dz = g_playerPosScratch.z - ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = g_playerPosScratch.x - ENTITY->scaMatrixData.localMatrix.t[0];
    if (SquareRoot0(dz * dz + dx * dx) < 800) {
        ew(ENTITY, 0x172) = 0;
        set_state_word(0x10001);
    }

    yawn_anim_advance(0, 0x100);
    yawn_turn_accel();
    Add_speedXZ((int)ew(ENTITY, 0x172));
    yawn_post_move(0x38);
}

// ============================================================================
// yawn_damaged_run @ 0x00407af0  (state 2)
// The flinch.  action_state 0/1 is the recoil, 2/3 the recovery turn (which
// swings the other way once the timer drops past 30).  On exit the snake is
// forced into form 1 with a 0x5A-frame recovery lockout (0xDA = 0x80 | 0x5A;
// bit 7 is the flag yawn_action_move reads to pick the fast slither).
// ============================================================================
void yawn_damaged_run(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0: {
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eb(ENTITY, 0xbd) = (signed char)(eb(ENTITY, 0x16e) + 9);
        eub(ENTITY, 0x8c) = 7;
        ew(ENTITY, 0x172) = 0;
        euw(ENTITY, 0xc2) = 0x46;

        const int* d = dead_move_pos();
        g_playerPosScratch.x   = d[0];
        g_playerPosScratch.z   = d[2];
        g_playerPosScratch.pad = d[3];
        g_playerPosScratch.y   = -300;
        if (ENTITY->jointsStructs[0].world.t[1] < -0x5dc) {
            g_playerPosScratch.y = 300;
        }
        Effect_CreateBillboard(0, 8, 0, &ENTITY->jointsStructs[0].world,
                               &g_playerPosScratch, 0);
        Snd_em(2);
        // fall through
    }
    case 1:
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(0, 0x200));
        break;

    case 2:
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 5;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0x78;
        euw(ENTITY, 0xc4) = 0x3c;
        // fall through
    case 3: {
        yawn_anim_advance(0, 0x200);
        if (eb(ENTITY, 0xbe) == 0x0f || eb(ENTITY, 0xbe) == 0x28) {
            Snd_em(1);
        }
        g_playerDisplacement = 1;
        if (ew(ENTITY, 0xc4) > 0x1e) g_playerDisplacement = -1;
        entity_rotate_toward_target(player_pos_vec(),
                                    (unsigned short)((short)g_playerDisplacement * 0x30));
        short ticks = ew(ENTITY, 0xc4);
        ew(ENTITY, 0xc4) = (short)(ticks - 1);
        if (ticks == 0) {
            // 16-bit store: state = 1, ignore_player_flag = 0.
            eub(ENTITY, 0x84) = 1;
            eub(ENTITY, 0x85) = 0;
            set_action(1, 0);
            eub(ENTITY, 0x16e) = 1;
            eub(ENTITY, 0x8a) = 0;
            eub(ENTITY, 0x17d) = 0xda;
            Snd_em(1);
        }
        yawn_turn_accel();
        break;
    }
    default:
        break;
    }

    Add_speedXZ((int)ew(ENTITY, 0x172));
    yawn_post_move(0x30);
}

// ============================================================================
// yawn_die_run @ 0x00407e00  (state 3)
//
// The full death: thrash (0/1), coil (2/3), collapse (4/5), then the two-phase
// dissolve.  Phase 7 shrinks the model with ScaleMatrixCols on every joint
// NOTE: Ghidra splits the death switch into separate case functions - caseD_6
// at 0x004080de and caseD_8 at 0x00408705 are phases of THIS state machine
// (the dissolve with tint flashes and bubble billboards); the port keeps them
// inlined here.
// while the RGB tint is bled out one channel at a time; phase 8 recolours the
// segment sprites and finishes the collapse.  The ambient sparkle runs off
// +0x180 for 800 frames regardless of phase.
// ============================================================================
void yawn_die_run(void)
{
    switch (eub(ENTITY, 0x87)) {
    case 0: {
        eub(ENTITY, 0x87) = 1;
        eub(ENTITY, 0xbe) = 0;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 6;
        eub(ENTITY, 0x8c) = 7;
        ew(ENTITY, 0x172) = 0;
        euw(ENTITY, 0xc2) = 0;
        eub(ENTITY, 0x8a) = 1;

        const int* d = dead_move_pos();
        g_playerPosScratch.x   = d[0];
        g_playerPosScratch.z   = d[2];
        g_playerPosScratch.pad = d[3];
        g_playerPosScratch.y   = -300;
        if (ENTITY->jointsStructs[0].world.t[1] < -0x5dc) {
            g_playerPosScratch.y = 500;
        }
        MATRIX* head = &ENTITY->jointsStructs[0].world;
        Effect_CreateBillboard(0, 0x0b, 0, head, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0x08, 0, head, &g_playerPosScratch, 0);
        Snd_em(2);
        Flg_on((int)g_EnemiesFlags, eub(ENTITY, 0x163));
        // fall through
    }
    case 1:
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(0, 0x200));
        Add_speedXZ((int)ew(ENTITY, 0x172));
        yawn_post_move(0x30);
        break;

    case 2:
        eub(ENTITY, 0x87) = 3;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 7;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 0x46;
        Snd_em(1);
        // fall through
    case 3:
        entity_rotate_toward_target(player_pos_vec(), 0x30);
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(0, 0x200));
        Add_speedXZ((int)ew(ENTITY, 0x172));
        yawn_post_move(0x30);
        break;

    case 4:
        eub(ENTITY, 0x87) = 5;
        eub(ENTITY, 0xbf) = 0;
        eub(ENTITY, 0xbd) = 0x0f;
        eub(ENTITY, 0x8c) = 7;
        euw(ENTITY, 0xc2) = 100;
        Snd_em(2);
        // fall through
    case 5:
        eb(ENTITY, 0x87) = (signed char)(eb(ENTITY, 0x87)
                                         + (signed char)yawn_anim_advance(0, 0x200));
        if (eub(ENTITY, 0xbe) == 0x32) {
            MATRIX* head = &ENTITY->jointsStructs[0].world;
            const int* d = dead_move_pos();
            g_playerPosScratch.x   = d[0];
            g_playerPosScratch.z   = d[2];
            g_playerPosScratch.pad = d[3];
            g_playerPosScratch.y   = 500;
            Effect_CreateBillboard(0, 0x0b, 0, head, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0x08, 0, head, &g_playerPosScratch, 0);
        }
        Add_speedXZ((int)ew(ENTITY, 0x172));
        yawn_post_move(0x30);
        break;

    case 6:
        euw(ENTITY, 0xc2) = 0;
        ENTITY->status_flags |= 6;
        for (g_playerDisplacement = 0xc; ; g_playerDisplacement--) {
            int i = g_playerDisplacement;
            g_EnemiesList[i].status_flags |= 4;
            if (i == 0) break;
        }
        eub(ENTITY, 0x87) = 7;
        ew(ENTITY, 0x176) = 0x1000;
        euw(ENTITY, 0xc4) = 0;
        // fall through
    case 7: {
        Add_speedXZ((int)ew(ENTITY, 0x172));
        yawn_post_move(0x30);

        char* j = (char*)ENTITY->jointsStructs;
        short shrink = ew(ENTITY, 0x176);
        g_playerPosScratch.x = (0x1000 - shrink) / 2 + 0x1000;
        g_playerPosScratch.y = (int)shrink;
        g_playerPosScratch.z = (0x1000 - shrink) / 2 + 0x1000;
        ENTITY->has_enter_switch_zone |= 0x80;
        RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &ENTITY->scaMatrixData.localMatrix);
        ScaleMatrixCols(&ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch);
        ENTITY->scaMatrixData.localMatrix.t[1] += 2;
        for (int k = 3; k <= 14; k++) {
            ScaleMatrixCols((MATRIX*)(j + k * 0x7c + 0x44), &g_playerPosScratch);
        }

        if (ew(ENTITY, 0xc4) < 0x50) {
            ew(ENTITY, 0xc4) = (short)(ew(ENTITY, 0xc4) + 1);
            ew(ENTITY, 0xc4) = (short)(ew(ENTITY, 0xc4) + 1);
            if ((eub(ENTITY, 0xc4) & 7) == 0) {
                scd_model_tint_apply(-1, 0, 0, 0, 0x200, eub(ENTITY, 1));
            }
            ew(ENTITY, 0xc4) = (short)(ew(ENTITY, 0xc4) + 1);
            if ((eub(ENTITY, 0xc4) & 7) == 0) {
                scd_model_tint_apply(0, -1, 0, 0, 0x200, eub(ENTITY, 1));
            }
            if ((eub(ENTITY, 0xc4) & 0x1f) == 0) {
                scd_model_tint_apply(0, 0, -1, 0, 0x200, eub(ENTITY, 1));
            }
        }

        ew(ENTITY, 0x176) = (short)(ew(ENTITY, 0x176) - 0x14);
        if (ew(ENTITY, 0x176) < 500) {
            eub(ENTITY, 0x87) = 8;
            euw(ENTITY, 0xc4) = 0;
            for (g_playerDisplacement = 0xc; ; g_playerDisplacement--) {
                BillboardSetColor(&g_EnemiesList[g_playerDisplacement].pushVelocity,
                                  1, 2, 0xafdf9f);
                int i = g_playerDisplacement;
                if (i == 0) break;
            }
        }
        break;
    }
    case 8: {
        Add_speedXZ((int)ew(ENTITY, 0x172));
        yawn_post_move(0x30);

        char* j = (char*)ENTITY->jointsStructs;
        short t = ew(ENTITY, 0xc4);
        g_playerPosScratch.x = t * -0x10 + 0x1706;
        g_playerPosScratch.y = 500 - t;
        g_playerPosScratch.z = t * -0x10 + 0x1706;
        ENTITY->has_enter_switch_zone |= 0x80;
        RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &ENTITY->scaMatrixData.localMatrix);
        ScaleMatrixCols(&ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch);
        for (int k = 3; k <= 14; k++) {
            ScaleMatrixCols((MATRIX*)(j + k * 0x7c + 0x44), &g_playerPosScratch);
        }
        ew(ENTITY, 0xc4) = (short)(ew(ENTITY, 0xc4) + 1);
        if (ew(ENTITY, 0xc4) > 0x15e) {
            eub(ENTITY, 0x87) = 9;
        }
        break;
    }
    default:
        break;
    }

    // Ambient dissolve sparkle - runs for 800 frames from the moment the death
    // state starts, independent of the phase above.
    if (euw(ENTITY, 0x180) < 800) {
        euw(ENTITY, 0x180) = (unsigned short)(euw(ENTITY, 0x180) + 1);

        if ((eub(ENTITY, 0x180) & 0xf) == 0) {
            char* j = (char*)ENTITY->jointsStructs;
            g_playerPosScratch.y = 500;
            for (int n = 0; n < 3; n++) {
                g_animFrameIdSave = (unsigned int)(rand() % 0xf);
                g_playerPosScratch.x = (int)((unsigned int)rand() & 0x3ff) - 0x200;
                g_playerPosScratch.z = (int)((unsigned int)rand() & 0x3ff) - 0x200;
                Effect_CreateBillboard(9, 0x11, 0,
                                       (void*)(j + g_animFrameIdSave * 0x7c + 0x44),
                                       &g_playerPosScratch, 0x14);
            }
        }

        if (((euw(ENTITY, 0x180) + 3) & 7) == 0 && euw(ENTITY, 0x180) < 600) {
            char* j = (char*)ENTITY->jointsStructs;
            g_playerPosScratch.x = 0;
            g_playerPosScratch.z = 0;
            g_playerPosScratch.y = 500;
            for (int n = 0; n < 3; n++) {
                g_animFrameIdSave = (unsigned int)(rand() % 0xf);
                g_playerPosScratch.x = (int)((unsigned int)rand() & 0x3ff) - 0x200;
                g_playerPosScratch.z = (int)((unsigned int)rand() & 0x3ff) - 0x200;
                Effect_CreateBillboard(9, 0x11, 0,
                                       (void*)(j + g_animFrameIdSave * 0x7c + 0x44),
                                       &g_playerPosScratch, 0x14);
            }
        }
    }
}

// ============================================================================
// yawn_pick_action_normal @ 0x00405860   (behavior_flags & 7 == 0 or 2)
//
// The free-roaming decision tree.  Every clause writes the WHOLE state word, so
// the last matching one wins - the order below is the priority order.
// ============================================================================
void yawn_pick_action_normal(void)
{
    entity_rotate_toward_target(player_pos_vec(), 0x30);

    if (g_playerEntity.isBeingAttackedFlag == 0) {
        player_distance_z = check_line_of_sight(player_pos_vec());

        // Close enough to bite outright.
        if (g_playerDisplacement < 2000 && player_distance_z == 0
            && g_playerEntity.health > 9) {
            set_state_word(0x20101);
        }
        // Or inside the form's lunge range and already lined up.
        if (g_playerDisplacement < (int)(((unsigned int)eub(ENTITY, 0x16e) * 5 + 0x19) * 200)
            && player_distance_z == 0 && g_playerEntity.health > 9) {
            if ((short)turn_toward_target(player_pos_vec(), 0x200) == 0) {
                set_state_word(0x20101);
            }
        }
        // Too many bites in a row -> rear up.
        if (g_playerDisplacement < 7000 && eb(ENTITY, 0x16f) > 4) {
            set_state_word(0x30101);
        }
        // Grown form, player nearly dead, lined up and off cooldown -> swallow.
        if (g_playerDisplacement < 4000 && eb(ENTITY, 0x16e) != 0
            && player_distance_z == 0) {
            if ((short)turn_toward_target(player_pos_vec(), 0x40) == 0
                && g_playerEntity.health - 10 < 0
                && eb(ENTITY, 0x185) == 0) {
                set_state_word(0x40101);
            }
        }
        // Juvenile form with a nearly-dead player -> grow first.
        if (g_playerEntity.health - 10 < 0 && eb(ENTITY, 0x16e) == 0) {
            eub(ENTITY, 0x17f) = 1;
            set_state_word(0x30101);
        }
        // Grown form pushed too far back in the room while biting -> shrink.
        if (ENTITY->scaMatrixData.localMatrix.t[2] > 17000 && eb(ENTITY, 0x16e) != 0
            && eb(ENTITY, 0x86) == 2) {
            set_state_word(0x30101);
            eub(ENTITY, 0x17f) = 0;
        }
        // Wedged against geometry -> reposition.
        if (eub(ENTITY, 0x182) > 0x96) {
            set_state_word(0x80101);
        }
        // Player is poisoned but healthy -> shrink back to the juvenile form.
        if ((g_playerEntity.healthStatusFlags & 8) != 0 && eb(ENTITY, 0x16e) != 0
            && g_playerEntity.health - 10 >= 0) {
            set_state_word(0x30101);
            eub(ENTITY, 0x17f) = 0;
        }
        // Low health -> flee.
        if (ew(ENTITY, 0x88) < 0xaf0) {
            set_state_word(0x70101);
        }
    }

    yawn_turn_accel();
}

// ============================================================================
// yawn_pick_action_scripted @ 0x00405a80   (behavior_flags & 7 == 4 or 6)
//
// The cutscene-spawned Yawn.  Same tree minus the four clauses that would end
// the fight on their own: no room-position shrink, no reposition, no
// poison-shrink and no flee.
// ============================================================================
void yawn_pick_action_scripted(void)
{
    entity_rotate_toward_target(player_pos_vec(), 0x30);

    if (g_playerEntity.isBeingAttackedFlag == 0) {
        player_distance_z = check_line_of_sight(player_pos_vec());

        if (g_playerDisplacement < 2000 && player_distance_z == 0
            && g_playerEntity.health > 9) {
            set_state_word(0x20101);
        }
        if (g_playerDisplacement < (int)(((unsigned int)eub(ENTITY, 0x16e) * 5 + 0x19) * 200)
            && player_distance_z == 0 && g_playerEntity.health > 9) {
            if ((short)turn_toward_target(player_pos_vec(), 0x200) == 0) {
                set_state_word(0x20101);
            }
        }
        if (g_playerDisplacement < 7000 && eb(ENTITY, 0x16f) > 4) {
            set_state_word(0x30101);
        }
        if (g_playerDisplacement < 4000 && eb(ENTITY, 0x16e) != 0
            && player_distance_z == 0) {
            if ((short)turn_toward_target(player_pos_vec(), 0x40) == 0
                && g_playerEntity.health - 10 < 0
                && eb(ENTITY, 0x185) == 0) {
                set_state_word(0x40101);
            }
        }
        if (g_playerEntity.health - 10 < 0 && eb(ENTITY, 0x16e) == 0) {
            eub(ENTITY, 0x17f) = 1;
            set_state_word(0x30101);
        }
    }

    yawn_turn_accel();
}

// ============================================================================
// yawn_select_behavior @ 0x00405810
// Manhattan distance to the player into g_playerDisplacement, then dispatch on
// behavior_flags & 7 (table 0x004b1a20).
// ============================================================================
void yawn_select_behavior(void)
{
    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
           - ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
           - ENTITY->scaMatrixData.localMatrix.t[0];
    int sz = dz >> 31;
    int sx = dx >> 31;
    g_playerDisplacement = (((dz ^ sz) - sz) - sx) + (dx ^ sx);

    switch (ENTITY->behavior_flags & 7) {
    case 0:
    case 2: yawn_pick_action_normal();   break;
    case 4:
    case 6: yawn_pick_action_scripted(); break;
    default:
        // The odd entries of 0x004b1a20 are NULL. behavior_flags & 0x0F is only
        // ever 0/2/4/6, so the original would jump through a null pointer here.
        break;
    }
}

// ============================================================================
// yawn_check_actions @ 0x004057f0
// Tail-jump into the action table (0x004b19f8) on action_behavior.
// ============================================================================
void yawn_check_actions(void)
{
    switch (eub(ENTITY, 0x86)) {
    case 0: yawn_action_idle();       break;
    case 1: yawn_action_move();       break;
    case 2: yawn_action_bite();       break;
    case 3: yawn_action_rear();       break;
    case 4: yawn_action_swallow();    break;
    case 5: yawn_action_entry();      break;
    case 6: yawn_action_emerge();     break;
    case 7: yawn_action_flee();       break;
    case 8: yawn_action_reposition(); break;
    default:
        // Entry [9] of the table is NULL; nothing ever writes a value above 8.
        break;
    }
}

// ============================================================================
// State 0 - yawn_init @ 0x00404cf0
//
// Poses the skeleton once, then clones the head into g_EnemiesList[1..12] so
// each body segment gets its own collision slot.  Health is 0x0bea normally;
// entity id 0x12 (the second Yawn) uses 0x012c, or 0x0190 once the serum has
// been taken - the rematch is shorter if you cured yourself.
// ============================================================================
void yawn_init(void)
{
    set_state_word(0x00000101);   // state 1, ignore 0, behavior 1, action_state 0

    ENTITY->scaMatrixData.field_00 = 0;
    eub(ENTITY, 0x8a) = 0;

    ResetJointTransforms();

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_animFrameIdSave = 0x00808080;               // shadow tint (an immediate)
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 1000, 1000);

    ew(ENTITY, 0x88) = 0x0bea;
    if (eub(ENTITY, 1) == 0x12) {
        if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            ew(ENTITY, 0x88) = 0x012c;
        } else {
            ew(ENTITY, 0x88) = 0x0190;
        }
    }

    eub(ENTITY, 0xbe) = 0;
    eub(ENTITY, 0xbf) = 0;
    eub(ENTITY, 0xbd) = (unsigned char)((ENTITY->behavior_flags & 2) * 5 + 1);
    if ((ENTITY->behavior_flags & 0xf) == 2) {
        eub(ENTITY, 0xbd) = 2;
    }
    eub(ENTITY, 0x8c) = 0;

    // Back the entity off by the head joint's local X so the model lands where
    // the spawn point says it should.
    g_svecScratch.z = 0;
    g_svecScratch.y = 0;
    g_svecScratch.x = (short)ENTITY->jointsStructs[0].transform.t[0];
    RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), &ENTITY->scaMatrixData.localMatrix);
    ApplyMatrixSV(&ENTITY->scaMatrixData.localMatrix, &g_svecScratch, &g_svecScratch);
    ENTITY->scaMatrixData.localMatrix.t[0] -= (int)g_svecScratch.x;
    ENTITY->scaMatrixData.localMatrix.t[2] -= (int)g_svecScratch.z;

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
    EntityComputeJointWorldMatrices(0);

    JointStruct* j = ENTITY->jointsStructs;
    ENTITY->scaMatrixData.localMatrix.t[0] = j[0].world.t[0];
    ENTITY->scaMatrixData.localMatrix.t[1] = 0;
    ENTITY->scaMatrixData.localMatrix.t[2] = j[0].world.t[2];
    ENTITY->position.x = (short)ENTITY->scaMatrixData.localMatrix.t[0];
    ENTITY->position.y = 0;
    ENTITY->position.z = (short)ENTITY->scaMatrixData.localMatrix.t[2];

    // The ground height the animator anchors the whole body to every frame.
    ew(ENTITY, 0x174) = (short)j[7].world.t[1];

    // 0x004b19bc holds the address of the SCA record; the record's [1] field is
    // copied into the hit-data block so the segments share one hitbox height.
    ENTITY->Sca_info = (unsigned int)(uintptr_t)s_yawnScaInfo;
    *(short*)(uintptr_t)ENTITY->pSca_hit_data       = 0;
    *(short*)((uintptr_t)ENTITY->pSca_hit_data + 4) = 0;
    *(short*)((uintptr_t)ENTITY->pSca_hit_data + 2) = *(short*)(ENTITY->Sca_info + 4);

    j[0].transform.t[0] = 0;
    j[0].transform.t[2] = 0;
    for (int k = 3; k <= 14; k++) {
        j[k].flags |= 8;
    }

    EntityComputeJointWorldMatrices(0);

    ew(ENTITY, 0x172) = 0;
    eub(ENTITY, 0x16c) = 0x40;
    eub(ENTITY, 0x16e) = 0;
    eub(ENTITY, 0x16f) = 0;
    ew(ENTITY, 0x170) = 0;
    eub(ENTITY, 0x17c) = 0;
    eub(ENTITY, 0x17d) = 0;
    eub(ENTITY, 0x17e) = 0;
    ew(ENTITY, 0x176) = 0;
    ew(ENTITY, 0x180) = 0;
    eub(ENTITY, 0x17f) = 0;
    eub(ENTITY, 0x182) = 0;
    eub(ENTITY, 0x184) = 0;
    eub(ENTITY, 0x185) = 0;

    // Twelve extra enemy slots for the body.
    g_enemy_count = (unsigned char)(g_enemy_count + 12);

    Entity* copy = &g_EnemiesList[12];
    char* joints = (char*)ENTITY->jointsStructs;
    for (g_playerDisplacement = 12; ; g_playerDisplacement--, copy--) {
        memcpy(copy, ENTITY, 0x18c);
        // Segment k tracks joint 2 + k.
        copy->scd_target_ptr = (unsigned int)(uintptr_t)(joints + g_playerDisplacement * 0x7c + 0xf8);
        copy->scaMatrixData.localMatrix.t[0] = *(int*)(joints + 0x150 + g_playerDisplacement * 0x7c);
        copy->scaMatrixData.localMatrix.t[1] = *(int*)(joints + 0x154 + g_playerDisplacement * 0x7c);
        copy->scaMatrixData.localMatrix.t[2] = *(int*)(joints + 0x158 + g_playerDisplacement * 0x7c);
        copy->position.x = (short)ENTITY->scaMatrixData.localMatrix.t[0];
        copy->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
        copy->position.z = (short)ENTITY->scaMatrixData.localMatrix.t[2];
        copy->behavior_flags = 1;
        copy->health = -1;
        unsigned char st = (unsigned char)(copy->status_flags | 100);
        copy->status_flags = st;
        if ((ENTITY->behavior_flags & 0xf) == 2 || (ENTITY->behavior_flags & 0xf) == 6) {
            copy->status_flags = (unsigned char)(st & 0xfb);
        }
        if (g_playerDisplacement == 1) break;
    }

    yawn_pose_init();

    if ((ENTITY->behavior_flags & 0xf) == 0 || (ENTITY->behavior_flags & 0xf) == 4) {
        // The scripted entrance: hidden until behaviour 5 crawls it in.
        ENTITY->status_flags |= 4;
        eub(ENTITY, 0x85) = 1;
        set_action(5, 0);
        if ((ENTITY->behavior_flags & 0x80) != 0) {
            eub(ENTITY, 0x84) = 4;
        }
    } else {
        // Already in the room, grown, and the fight flag is up.
        eub(ENTITY, 0x16e) = 1;
        if (eub(ENTITY, 1) == 0x0d) {
            Flg_on((int)&g_ScenarioFlags, SCENARIO_FLAG_YAWN_BITE);
        }
        if ((ENTITY->behavior_flags & 0x80) != 0) {
            eub(ENTITY, 0x84) = 4;
        }
    }
}

// ============================================================================
// State 1 - yawn_state_check @ 0x00405690
// ============================================================================
void yawn_state_check(void)
{
    if ((ENTITY->behavior_flags & 0x80) != 0) return;

    ENTITY->status_flags &= 0x1f;
    ENTITY->status_flags |= 0x40;
    if (entity_check_alert_range(4000) < 4000) {
        ENTITY->status_flags &= 0xbf;
    }
    if (eub(ENTITY, 0x16e) == 0) {
        ENTITY->status_flags &= 0x7f;
        entity_check_visual_range(4000);
    }

    if (eub(ENTITY, 0x85) == 0) {
        yawn_select_behavior();
    }
    yawn_check_actions();

    // Snapshot the state word so a hit reaction can restore it (yawn_damaged).
    eu(ENTITY, 0x178) = eu(ENTITY, 0x84);

    unsigned char hiss = eub(ENTITY, 0x184);
    if (hiss != 0) {
        eub(ENTITY, 0x184) = (unsigned char)(hiss - 1);
        if ((hiss & 7) == 0) {
            int r = rand();
            yawn_spawn_dust(r < 0 ? -(-r & 1) : (r & 1));
        }
    }
}

// ============================================================================
// State 2 - yawn_damaged @ 0x00405750
// A hit only interrupts when the snake is not mid-bite (0x17C) and not in the
// post-bite lockout (0x17D); otherwise the saved state word is restored and the
// flinch never plays.
// ============================================================================
void yawn_damaged(void)
{
    if (eb(ENTITY, 0x17c) != 0) {
        eu(ENTITY, 0x84) = eu(ENTITY, 0x178);
        return;
    }
    if (eb(ENTITY, 0x17d) != 0) {
        eu(ENTITY, 0x84) = eu(ENTITY, 0x178);
        return;
    }
    if (eb(ENTITY, 0x85) == 0) {
        set_state_word(0x102);   // state 2, ignore_player 1, behavior 0, sub 0
    }
    yawn_damaged_run();
}

// ============================================================================
// State 3 - yawn_die @ 0x004057b0
// ============================================================================
void yawn_die(void)
{
    if (eb(ENTITY, 0x85) == 0) {
        set_state_word(0x103);   // state 3, ignore_player 1, behavior 0, sub 0
    }
    yawn_die_run();
}

// ============================================================================
// State 4 - yawn_state_wait @ 0x004057d0
// Parking state for a Yawn whose script has taken it over (behavior_flags
// 0x80).  It sits here until the flag is cleared, then drops back to state 1.
// ============================================================================
void yawn_state_wait(void)
{
    if ((ENTITY->behavior_flags & 0x80) == 0) {
        eub(ENTITY, 0x84) = 1;
    }
}

} // namespace

// ============================================================================
// yawn_update @ 0x004051e0 - table entries 13 (Yawn 1) and 18 (Yawn 2)
// ============================================================================
void yawn_update(void)
{
    JointStruct* seg = NULL;

    // Head only: run the state machine and tick the bite cooldown.
    if ((g_message_flags & 4) != 0 && (ENTITY->behavior_flags & 1) == 0) {
        switch (eub(ENTITY, 0x84)) {
        case 0: yawn_init();       break;
        case 1: yawn_state_check(); break;
        case 2: yawn_damaged();    break;
        case 3: yawn_die();        break;
        case 4: yawn_state_wait(); break;
        default:
            // Entry [5] of the state table is NULL.
            break;
        }
        if (eub(ENTITY, 0x185) != 0) {
            eub(ENTITY, 0x185) = (unsigned char)(eub(ENTITY, 0x185) - 1);
        }

    }

    // Segment: mirror the joint it owns, and take damage from the boss's death.
    if (ENTITY->behavior_flags == 1) {
        seg = (JointStruct*)(uintptr_t)ENTITY->scd_target_ptr;

        ENTITY->scaMatrixData.localMatrix.t[0] = seg->world.t[0];
        ENTITY->scaMatrixData.localMatrix.t[1] = seg->world.t[1];
        ENTITY->scaMatrixData.localMatrix.t[2] = seg->world.t[2];

        ENTITY->status_flags &= 0x1f;
        entity_check_visual_range(4000);
        if (ENTITY->scaMatrixData.localMatrix.t[1] < -0x5dc) {
            ENTITY->status_flags |= 0xc0;   // segment is up in the ceiling hole
        }

        // A segment that was shot (low 3 bits of hit_state) bleeds and drains
        // the HEAD's health - this is how damage anywhere on the body counts.
        if ((eub(ENTITY, 0x8a) & 0x07) != 0) {
            eub(ENTITY, 0x8a) = (unsigned char)(eub(ENTITY, 0x8a) - 1);

            g_playerPosScratch.x = 0;
            g_playerPosScratch.y = -300;
            if (seg->world.t[1] < -0x5dc) g_playerPosScratch.y = 300;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 8, 0, &seg->world, &g_playerPosScratch, 0);

            // Reaction phase 0x40 / 0x50 = a heavy hit: 0x41 damage.
            if ((eub(ENTITY, 0x8a) & 0x78) == 0x40 || (eub(ENTITY, 0x8a) & 0x78) == 0x50) {
                g_EnemiesList[0].health = (short)(g_EnemiesList[0].health - 0x41);
                g_EnemiesList[0].angle = (short)(g_EnemiesList[0].angle
                        + (short)turn_toward_target(player_pos_vec(), 0x10));
                if (g_EnemiesList[0].health < 0) {
                    g_EnemiesList[0].state             = 3;
                    g_EnemiesList[0].ignore_player_flag = 0;
                    g_EnemiesList[0].action_behavior   = 0;
                    g_EnemiesList[0].action_state      = 0;
                }
            }
            if ((eub(ENTITY, 0x8a) & 7) == 0) {
                // Serum taken -> the snake only takes 5 per tick instead of 15.
                if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                    g_EnemiesList[0].health = (short)(g_EnemiesList[0].health - 0xf);
                } else {
                    g_EnemiesList[0].health = (short)(g_EnemiesList[0].health - 5);
                }
                // A hit on joints 0-9 (the front half) with the serum taken
                // does another 15.
                JointStruct* hit = (JointStruct*)(uintptr_t)ENTITY->scd_target_ptr;
                if (hit->index < 10 && Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) != 0) {
                    g_EnemiesList[0].health = (short)(g_EnemiesList[0].health - 0xf);
                }
                g_EnemiesList[0].angle = (short)(g_EnemiesList[0].angle
                        + (short)turn_toward_target(player_pos_vec(), 0x10));
                if (g_EnemiesList[0].health < 0) {
                    g_EnemiesList[0].state             = 3;
                    g_EnemiesList[0].ignore_player_flag = 0;
                    g_EnemiesList[0].action_behavior   = 0;
                    g_EnemiesList[0].action_state      = 0;
                }
            }
            eub(ENTITY, 0x8a) &= 0x87;
        }
    }

    g_playerDisplacement = check_room_collision(entity_pos(),
                                                *(short*)(ENTITY->Sca_info + 10));

    if (ENTITY->behavior_flags == 1 && g_playerDisplacement != 0) {
        // The segment was pushed out of a wall: drag the rest of the body.
        YAWN_TMP = (int)seg->index;

        if (seg->index == 3) {
            // Joint 3 carries the head, so shifting it has to carry joints
            // 0-2 (and joint 4's world position) with it.
            player_distance_z  = seg->world.t[0] - ENTITY->scaMatrixData.localMatrix.t[0];
            g_scaled_down_dist = seg->world.t[2] - ENTITY->scaMatrixData.localMatrix.t[2];
            for (g_animFrameIdSave = 3; ; g_animFrameIdSave--) {
                JointStruct* back = seg - g_animFrameIdSave;
                back->world.t[0] -= player_distance_z;
                back->world.t[2] -= g_scaled_down_dist;
                if (g_animFrameIdSave == 1) break;
            }
            seg[1].world.t[0] -= player_distance_z;
            seg[1].world.t[2] -= g_scaled_down_dist;
            seg->world.t[0] = ENTITY->scaMatrixData.localMatrix.t[0];
            seg->world.t[2] = ENTITY->scaMatrixData.localMatrix.t[2];
            YAWN_TMP = 4;
        } else {
            seg->world.t[0] = ENTITY->scaMatrixData.localMatrix.t[0];
            seg->world.t[2] = ENTITY->scaMatrixData.localMatrix.t[2];
            // Joints 6 and up steer the joint in front of them around too, but
            // only for the juvenile form (the grown body is too stiff).
            if (seg->index > 5 && eub(ENTITY, 0x16e) == 0) {
                yawn_chain_align(seg - 1, seg);
            }
        }

        {
            JointStruct* chain = seg;
            while (YAWN_TMP < 0xf) {
                yawn_chain_follow(chain - 1, chain, 0x30);
                YAWN_TMP++;
                chain++;
            }
        }

        // The head's model scale ramp (swallow grab) is applied here, with
        // ENTITY temporarily pointed at slot 0.
        JointStruct* own = (JointStruct*)(uintptr_t)ENTITY->scd_target_ptr;
        Entity* saved = ENTITY;
        ENTITY = g_EnemiesList;
        DAT_00be0e00 = (int)(uintptr_t)saved;

        if (own->index == 4 && ew(g_EnemiesList, 0x170) != 0
            && eub(g_EnemiesList, 0x16e) == 0) {
            g_playerPosScratch.x = ew(g_EnemiesList, 0x170) - 300;
            g_playerPosScratch.y = g_playerPosScratch.x;
            g_playerPosScratch.z = g_playerPosScratch.x;
            ScaleMatrixCols(&own[-1].world, &g_playerPosScratch);
        }
        if (own->index == 3 && ew(ENTITY, 0x170) != 0) {
            g_playerPosScratch.x = ew(ENTITY, 0x170) - 300;
            g_playerPosScratch.y = g_playerPosScratch.x;
            g_playerPosScratch.z = g_playerPosScratch.x;
            ScaleMatrixCols(&own->world, &g_playerPosScratch);
        }

        ENTITY = (Entity*)(uintptr_t)DAT_00be0e00;
    }

    ENTITY->has_enter_switch_zone &= 0x80;
    ENTITY->has_enter_switch_zone |= (unsigned char)is_entity_in_switch_zone(
            entity_pos(), g_CurrentRdtDataTypePtr);

    if (ENTITY->behavior_flags == 1) {
        JointStruct* own = (JointStruct*)(uintptr_t)ENTITY->scd_target_ptr;

        if (g_EnemiesList[0].health < 0) {
            ENTITY->status_flags |= 2;
            eub(ENTITY, 0x8a) = 0x80;
        }

        // Joint 14 is the tail, i.e. the LAST segment slot - it drives the
        // ground-shadow pass for the whole body exactly once per frame.
        if (own->index == 0x0e) {
            g_playerDisplacement = 0x0b;
            // The head's shadow rides joints[0]; the eleven body shadows ride
            // each segment slot's own joint.  The original preloads slot 0's
            // jointsStructs and then reads scd_target_ptr for the rest, which
            // is why the first iteration is spelled out differently.
            JointStruct* shadowJoint = g_EnemiesList[0].jointsStructs;
            Entity* e = g_EnemiesList;
            for (;;) {
                if ((e->has_enter_switch_zone & 0x7f) != 0) {
                    entity_add_fade_sprite((VECTOR*)shadowJoint->world.t,
                                           (short*)&e->pushVelocity, 0, e->angle);
                }
                shadowJoint = (JointStruct*)(uintptr_t)e[1].scd_target_ptr;
                e++;
                int n = g_playerDisplacement--;
                if (n == 0) break;
            }
        }
    }
}
