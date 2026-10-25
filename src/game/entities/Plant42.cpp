// Plant42.cpp - Plant 42 boss (enemy type 8).
//
// Original PC addresses:
//   plant42_init            0x004648d0   state 0
//   plant42_state_check     0x00464e50   state 1
//                             (0x00464e60 is its tail-jump body - Ghidra
//                              lists it separately; port keeps one function)
//   plant42_damaged         0x00465130   state 2   (-> 0x00465310)
//   plant42_die             0x00465620   state 3
//   plant42_attack          0x00465740   state 4   (empty)
//   plant42_scd_state       0x004049f0   state 8
//   plant42_update          0x00464d10   per-frame entry
//   action table            0x004c2a50   16 entries, dispatched by FUN_00465750
//   SCD action table        0x004b1808   used ONLY by state 8
//   plant42_joint_move      0x0046a830   Plant 42's OWN joint animator
//   plant42_render_copies   0x00469d20   body + roots companions
//   body state table        0x004c2b90 : 0x00469fb0 / 0x0046a0e0 / 0x0046a370
//   roots state table       0x004c2ba0 : 0x0046a6b0 / 0x0046a6e0 / 0x0046a790
//
// Plant 42 owns two *copied* entities allocated by FUN_0048a630 out of the
// room data buffer: the flower body (Entity+0xB8) and the root ball
// (Entity+0x178).  They are not enemy slots, so update_entities never sees
// them; plant42_update renders and ticks them itself.
//
// Room 40C0 drives three different set-ups through behavior_flags:
//   flags & 0x0F == 4  the fallen-core cutscene (body and roots frozen)
//   flags & 0x0F == 6  Chris held in the vine (state 8, SCD behavior 0x0B)
//   flags & 0x40       boss fight (state 8 until the SCD hands over)
//
// Field offsets are deliberately raw.  Plant 42 reuses most of the generic
// movement bytes at a different width (0xC4 as a 16-bit timer, 0x172 as a
// signed 16-bit sweep step, 0x16C/0x16D as signed bytes, 0x16C/0x170/0x174 of
// the COPIES as three 32-bit scale factors), so naming them through the
// generic Entity fields would be actively misleading.
#include "EntityCommon.h"
#include "../../Globals.h"
#include "../BioCard.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

extern void ResetJointTransforms(void);                                   // 0x0048bad0
extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);         // 0x0048b6b0
extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2); // 0x0048b700 helper
// 0x00473b10. The parameter widths are the ORIGINAL's: p1..p3 are signed
// 16-bit tint deltas and p4/p5 are 16-bit queue words.  A previous revision
// declared p1/p4/p5 as bytes, which silently truncated Plant 42's
// `..., 0, 0x200, 8)` fade parameter to 0 and clipped the negative deltas.
extern void scd_model_tint_apply(short p1, short p2, short p3,
                                 unsigned short p4, unsigned short p5,
                                 unsigned char p6);                       // 0x00473b10
extern void Flg_on(int baseAddr, unsigned int bitIndex);                  // 0x00473ef0
extern int player_distance_z;                                            // 0x00be0de4
extern void update_entity_lighting(VECTOR* entityPos);                   // 0x004830d0

namespace {

// ---------------------------------------------------------------------------
// Static tables mined from .rdata / .data
// ---------------------------------------------------------------------------

// 0x004c29c0 - four 6-short SCA collision records, contiguous.  Sca_info gets
// record 0 (0x004c29c0, PTR_DAT_004c29f0) for the split vines and record 2
// (0x004c29d8, PTR_DAT_004c29f4) for the main plant.
static const short s_plant42ScaInfo[4][6] = {
    { (short)0x8000, 0, 0, 0, 0x00fa, 0x00c8 },
    { (short)0x8000, 0, 0, 0, 0x00fa, 0x00c8 },
    {              0, 0, 0, 0, 0x00fa, 0x00c8 },
    { (short)0x8000, 0, 0, 0, 0x1b58, 0x07d0 },
};

// 0x004c2a40 / 0x004c2a44 - X and Z anchors for the ambient room spit effect.
static const short s_plant42EffectX[2] = { 0x0d54, 0x45ec };
static const short s_plant42EffectZ[2] = { 0x0d54, 0x46fa };

// 0x004c2a98 - idle animation pool.  Eleven bytes are stored; the selector
// only ever indexes rand() % 10, so entry 10 (0x0F) is unreachable.
static const unsigned char s_plant42IdleAnims[11] = {
    2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 15
};

// 0x004c2ad8 - 90-entry pulse ramp, read backwards as table[0x59 - counter].
// Drives the flower's breathing scale and the root ball's own scale.
static const unsigned char s_plant42Pulse[90] = {
      0,   1,   3,   6,  10,  15,  21,  28,  36,  45,
     55,  66,  78,  91, 105, 120, 136, 153, 172, 190,
    205, 217, 226, 232, 237, 241, 249, 252, 254, 255,
    254, 253, 252, 251, 249, 247, 245, 243, 240, 237,
    234, 231, 227, 223, 219, 215, 210, 205, 200, 195,
    189, 183, 177, 171, 164, 157, 150, 143, 135, 127,
    119, 111, 113, 106,  99,  92,  86,  80,  74,  68,
     63,  58,  53,  48,  44,  40,  36,  32,  29,  26,
     23,  20,  18,  16,  14,  12,  10,   8,   6,   4
};

// 0x004c2aa8 - fixed 3x3 orientation used to seat Chris in the vine
// (behaviour 6, room 40C0 only).
static const short s_plant42ChrisHoldMatrix[9] = {
    (short)0x0090, (short)0xf06d, (short)0xfc95,
    (short)0xf044, (short)0xff0d, (short)0x01e7,
    (short)0xfde8, (short)0x034b, (short)0xf0a4
};

} // namespace

// 0x004c29f8 - the live capture matrix. 0x004c2a0c/10/14 are its t[] and the
// original writes them directly, so this must stay one object.
//
// NOT file-static: t[0] (0x004c2a0c) is shared scratch. plant42_sweep_hit
// stores the knock-back facing there and player_anim_knockdown (0x004699d0
// state 6) reads it back to steer the slide.
MATRIX g_plant42CaptureMatrix = {};

namespace {

// ---------------------------------------------------------------------------
// Raw field access.  See the file header for why these are not Entity fields.
// ---------------------------------------------------------------------------
static inline signed char&    eb (void* e, unsigned o) { return *reinterpret_cast<signed char*>((char*)e + o); }
static inline unsigned char&  eub(void* e, unsigned o) { return *reinterpret_cast<unsigned char*>((char*)e + o); }
static inline short&          ew (void* e, unsigned o) { return *reinterpret_cast<short*>((char*)e + o); }
static inline unsigned short& euw(void* e, unsigned o) { return *reinterpret_cast<unsigned short*>((char*)e + o); }
static inline int&            ei (void* e, unsigned o) { return *reinterpret_cast<int*>((char*)e + o); }
static inline unsigned int&   eu (void* e, unsigned o) { return *reinterpret_cast<unsigned int*>((char*)e + o); }

// Entity+0x84 is written as one DWORD in a dozen places
// (state | ignore<<8 | action_behavior<<16 | action_state<<24).
static inline void set_state_word(unsigned int v) { eu(ENTITY, 0x84) = v; }

// The three 16-bit Plant 42 timers/steps.
static inline short& p42_ticks(void) { return ew(ENTITY, 0xc4); }   // frame timer
static inline short& p42_step(void)  { return ew(ENTITY, 0x172); }  // sweep speed
static inline short& p42_life(void)  { return ew(ENTITY, 0x180); }  // ambient FX timer
static inline unsigned int& p42_dist(void) { return eu(ENTITY, 0x17c); }

static inline Entity* plant42_body(void)
{
    return reinterpret_cast<Entity*>(static_cast<uintptr_t>(ENTITY->scd_target_ptr));
}

static inline Entity* plant42_roots(void)
{
    return reinterpret_cast<Entity*>(static_cast<uintptr_t>(eu(ENTITY, 0x178)));
}

// Joint N's world matrix / translation. Joints are 0x7C bytes, world at +0x44,
// translation at +0x58.
static inline MATRIX* jw(int n)  { return reinterpret_cast<MATRIX*>((char*)ENTITY->jointsStructs + n * 0x7c + 0x44); }
static inline int*    jwt(int n) { return reinterpret_cast<int*>((char*)ENTITY->jointsStructs + n * 0x7c + 0x58); }

static inline int player_sound_pos(void) { return (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]; }
static inline int joint_sound_pos(int n) { return (int)jwt(n); }

// ---------------------------------------------------------------------------
// Small GTE helpers the original inlines from libgte (0x0040a990 / 0x0040a530
// / 0x0040a250 / 0x0040a460).  MainMenu.cpp keeps private copies of the first
// three; they are file-static there, so Plant 42 carries its own.
// ---------------------------------------------------------------------------
static int gte_atan2_int(int y, int x)
{
    return (int)(atan2((double)y, (double)x) * 57.29577791868204 * 11.377777777777778);
}

static int gte_fsqrt_int(int value)
{
    if (value < 0) return 0;
    return (int)(sqrt((double)value * 0.000244140625) * 4096.0);
}

// 0x0040a250 - transpose the 3x3 short block of a MATRIX.
static void matrix_to_short_array(const MATRIX* src, MATRIX* dst)
{
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            ((short*)dst)[j * 3 + i] = src->m[i][j];
}

// 0x0040a460 - the 32-bit sibling of fp_lerp: out = (a*wa + b*wb) >> 12.
static void plant42_lerp3(const int* a, const int* b, int wa, int wb, int* out)
{
    for (int i = 0; i < 3; i++) {
        int ta = a[i] * wa;
        int tb = b[i] * wb;
        out[i] = ((ta + (ta >> 31 & 0xFFF)) >> 12) + ((tb + (tb >> 31 & 0xFFF)) >> 12);
    }
}

// ---------------------------------------------------------------------------
// plant42_joint_move (0x0046a830)
//
// Plant 42 does NOT use the shared Joint_move (0x0048b700).  Three differences
// matter and all three are visible on screen:
//
//   * Joint_move returns early while timing_control > 1, holding the pose.
//     This one always poses the skeleton and instead SUB-FRAME INTERPOLATES:
//     when the blend counter has run out but timing_control has not, it
//     synthesises a blend weight of (0x1000/blendStep) / (timing_control + 1).
//     That is what makes the vines sway continuously instead of stepping.
//   * The blend counter is consumed BEFORE it is used, not after.
//   * The frame only advances on the tick where timing_control reaches 0.
//
// It also ignores the per-joint 0x10 "skip rotation" flag that Joint_move
// honours, because the plant's own joints never set it.
// ---------------------------------------------------------------------------
static unsigned int plant42_joint_move(char reverse, unsigned int animHeader,
                                       unsigned int animBase, short blendStep)
{
    unsigned char* timing = &ENTITY->timing_control;
    if (*timing != 0) *timing = (unsigned char)(*timing - 1);

    g_playerDisplacement = (int)(*(short*)(animHeader + 6) / 2);

    unsigned short* animSlot = (unsigned short*)(animBase + (unsigned int)ENTITY->animationId * 4);
    unsigned short* frameEntry = (unsigned short*)((animSlot[1] & 0xFFFFFFFC) +
        (unsigned int)ENTITY->animation_frame_id * 4 + animBase);
    if (reverse != 0) {
        frameEntry = frameEntry +
            ((unsigned int)*animSlot + (unsigned int)ENTITY->animation_frame_id * (unsigned int)-2) * 2 +
            (unsigned int)-2;
    }

    short headerStride = *(short*)(animHeader + 2);
    short aligned = (short)(((int)headerStride + ((int)headerStride >> 31 & 3)) >> 2);
    short* animData = (short*)(animHeader + (int)aligned * 4 +
        (unsigned int)*frameEntry * g_playerDisplacement * 2);

    unsigned char* joint = (unsigned char*)ENTITY->jointsStructs;

    unsigned char blend = ENTITY->blend_counter;
    if (blend != 0) ENTITY->blend_counter = (unsigned char)(blend - 1);

    *(int*)(joint + 0x38) = (int)animData[0];

    if (blend == 0 && ENTITY->timing_control != 0) {
        blend = (unsigned char)((0x1000 / (int)blendStep) /
                                (int)((unsigned int)ENTITY->timing_control + 1));
    }

    int jointCount = (int)(unsigned int)ENTITY->jointCount;

    if (blend == 0) {
        *(int*)(joint + 0x3c) = (int)animData[1];
        *(int*)(joint + 0x40) = (int)animData[2];
        short* rotData = animData + 6;
        while (jointCount-- != 0) {
            *(short*)(joint + 4) = rotData[0];
            *(short*)(joint + 6) = rotData[1];
            *(short*)(joint + 8) = rotData[2];
            rotData += 3;
            RotMatrix((SVECTOR*)(joint + 4), (MATRIX*)(joint + 0x24));
            joint += 0x7c;
        }
    } else {
        int step = (int)blendStep;
        int invBlend = (int)(0x1000 / step) - (int)(unsigned int)blend;
        short* rotData = animData + 6;

        int ty = (int)animData[1] * invBlend * step;
        int cy = *(int*)(joint + 0x3c) * (int)(unsigned int)blend * step;
        *(int*)(joint + 0x3c) = ((ty + (ty >> 31 & 0xFFF)) >> 12) +
                                ((cy + (cy >> 31 & 0xFFF)) >> 12);
        *(int*)(joint + 0x40) = (int)animData[2];

        const int wCur = (int)(unsigned int)blend * step;
        const int wTgt = invBlend * step;

        while (jointCount-- != 0) {
            g_playerPosScratch.x = (int)rotData[0];
            g_playerPosScratch.y = (int)rotData[1];
            g_playerPosScratch.z = (int)rotData[2];
            g_svecScratch.x = *(short*)(joint + 4);
            g_svecScratch.y = *(short*)(joint + 6);
            g_svecScratch.z = *(short*)(joint + 8);
            g_svecScratch.pad = *(short*)(joint + 10);

            // Last blend step: shortest-arc fixup so a joint never spins the
            // long way round into the final pose.
            if (invBlend == 1) {
                for (int i = 2; i >= 0; i--) {
                    short cur = (&g_svecScratch.x)[i];
                    unsigned short diff = (unsigned short)((short)(&g_playerPosScratch.x)[i] - cur + 0x800);
                    if (0x1000 < diff) {
                        (&g_svecScratch.x)[i] =
                            (short)((((diff & 0x8000) == 0) ? 0x2000 : 0) + cur - 0x1000);
                    }
                }
            }

            int target[3] = { g_playerPosScratch.x << 4, g_playerPosScratch.y << 4, g_playerPosScratch.z << 4 };
            int cur[3]    = { (int)g_svecScratch.x << 4, (int)g_svecScratch.y << 4, (int)g_svecScratch.z << 4 };
            plant42_lerp3(cur, target, wCur, wTgt, cur);

            g_svecScratch.x = (short)(cur[0] >> 4);
            g_svecScratch.y = (short)(cur[1] >> 4);
            g_svecScratch.z = (short)(cur[2] >> 4);

            *(short*)(joint + 4)  = g_svecScratch.x;
            *(short*)(joint + 6)  = g_svecScratch.y;
            *(short*)(joint + 8)  = g_svecScratch.z;
            *(short*)(joint + 10) = g_svecScratch.pad;

            rotData += 3;
            RotMatrix(&g_svecScratch, (MATRIX*)(joint + 0x24));
            joint += 0x7c;
        }
    }

    if (ENTITY->timing_control == 0) {
        ENTITY->timing_control = (unsigned char)frameEntry[1];
        ENTITY->animation_frame_id = (unsigned char)(ENTITY->animation_frame_id + 1);
        if ((int)((unsigned int)*animSlot - 1) < (int)(unsigned int)ENTITY->animation_frame_id) {
            ENTITY->animation_frame_id = 0;
            return 1;
        }
    }
    return 0;
}

static inline unsigned int p42_anim(char reverse, short blend)
{
    return plant42_joint_move(reverse, ENTITY->animHeader, ENTITY->animBase, blend);
}

static inline void p42_reset_anim(unsigned char animation)
{
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3f;
    ENTITY->animationId = animation;
}

// Every capture behaviour ends with the same player billboard resize
// (0x00456790 on the PLAYER's shadow quad at 0x00be63c8, not the plant's).
static void plant42_player_billboard(void)
{
    int y = g_playerEntity.scaMatrixData.localMatrix.t[1] / 0x11;
    BillboardSetSize(&g_playerEntity.pushVelocity, (short)(y + 500), (short)(y + 700));
}

// Snap the player's local matrix onto the grabbing joint through the capture
// matrix.  jointIdx comes from Entity+0x175.
static void plant42_hold_player(int jointIdx)
{
    // This overwrites the player's world matrix outright, so a Plant 42 whose
    // model is gone must not run it: it would drive the player to a new garbage
    // position every frame.
    if (ENTITY->jointsStructs == NULL) return;
    g_playerEntity.zoneFlags |= 0x80;
    ApplyLVAndMul0Matrix(jw(jointIdx), &g_plant42CaptureMatrix,
                         &g_playerEntity.scaMatrixData.localMatrix);
}

// 0x0048c530 - build the capture matrix from a joint and the player matrix.
static void plant42_capture_setup(int jointIdx)
{
    if (ENTITY->jointsStructs == NULL) return;
    MATRIX* joint = jw(jointIdx);
    MATRIX transposed;
    matrix_to_short_array(joint, &transposed);
    MulMatrix0(&transposed, &g_playerEntity.scaMatrixData.localMatrix, &g_plant42CaptureMatrix);
    VECTOR rel;
    rel.x = g_playerEntity.scaMatrixData.localMatrix.t[0] - joint->t[0];
    rel.y = g_playerEntity.scaMatrixData.localMatrix.t[1] - joint->t[1];
    rel.z = g_playerEntity.scaMatrixData.localMatrix.t[2] - joint->t[2];
    ApplyMatrixLV(&transposed, &rel, &rel);
    g_plant42CaptureMatrix.t[0] = rel.x;
    g_plant42CaptureMatrix.t[1] = rel.y;
    g_plant42CaptureMatrix.t[2] = rel.z;
}

// 0x0046abd0 - derive a rotation from a fixed 3x3 and write it into `out`.
static void plant42_orient_from_matrix(const short* m, MATRIX* out)
{
    int c0 = (int)m[2];
    int c1 = (int)m[5];
    int c2 = (int)m[8];
    int len = c2 * c2 + c0 * c0 + c1 * c1;
    len = (c0 << 12) / ((int)(len + (len >> 31 & 0xFFF)) >> 12);

    SVECTOR rot;
    rot.x = (short)gte_atan2_int(-c1, c2);
    int sq = len * len;
    rot.y = (short)gte_atan2_int(len, gte_fsqrt_int(0x1000 - ((int)(sq + (sq >> 31 & 0xFFF)) >> 12)));
    rot.z = 0;
    rot.pad = 0;

    MATRIX rm;
    rm.t[0] = rm.t[1] = rm.t[2] = 0;
    RotMatrix(&rot, &rm);

    MATRIX transposed;
    matrix_to_short_array(&rm, &transposed);

    SVECTOR axis;
    axis.x = m[0];
    axis.y = m[3];
    axis.z = m[6];
    axis.pad = 0;
    ApplyMatrixSV(&transposed, &axis, &axis);

    rot.z = (short)gte_atan2_int((int)axis.y, (int)axis.x);
    RotMatrix(&rot, out);
}

// ---------------------------------------------------------------------------
// 0x004660e0 - sweep contact test, called from behaviour 1 states 5 and 6.
// ---------------------------------------------------------------------------
static void plant42_sweep_hit(void)
{
    unsigned short reach = (unsigned short)((short)p42_dist() - 2000);
    if (ENTITY->behavior_flags == 5)
        reach = (unsigned short)((short)p42_dist() - 0xdac);
    if (0x157b < reach) return;
    if ((short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                                  p42_step()) != 0) return;
    if (g_playerEntity.isBeingAttackedFlag != 0) return;
    if (p42_step() < 0x51) return;

    ENTITY->angle_z -= 200;
    ENTITY->action_state = 6;
    ENTITY->angle += 0x400;
    g_animFrameIdSave = is_facing_toward_entity(&g_playerEntity) & 0xff;
    ENTITY->angle -= 0x400;
    g_playerEntity.isBeingAttackedFlag = 1;

    int soundPos;
    unsigned char soundId;
    const bool facing = (g_animFrameIdSave != 0);
    const bool dirSet = (eb(ENTITY, 0x174) != 0);

    if (facing != dirSet) {
        // Knocked back on the front foot: play the stagger pose directly.
        g_playerEntity.animationId = 6;
        g_playerEntity.animFrameId = 8;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.directionAngle = (short)(ENTITY->angle + (facing ? 0x400 : -0x400));
        soundPos = joint_sound_pos(14);
        soundId = 4;
    } else {
        // Swept off the feet: hand over to the knock-down animation.
        g_plant42CaptureMatrix.t[0] = (int)(short)(ENTITY->angle + (facing ? 0x400 : -0x400));
        g_playerEntity.animationId = 6;
        g_playerEntity.animFrameId = 8;
        g_playerEntity.action_behavior = 2;
        g_playerEntity.action_state = 5;
        soundPos = joint_sound_pos(14);
        soundId = 2;
    }

    Play3DSnd(2, soundId, 0, soundPos);
    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)
        g_playerEntity.health -= 0x10;
    else
        g_playerEntity.health -= 0x19;
    if (g_playerEntity.health < 0) g_playerEntity.health = 1;
}

// The three attack behaviours end with the same "re-arm the split vines"
// tail: clear our hit flag, and re-raise it while the main body still has
// more than one hit left in it.
static void plant42_rearm_hit_flag(void)
{
    ENTITY->hit_state = 0;
    Entity* body = plant42_body();
    if ((ENTITY->behavior_flags & 1) == 0 && body != NULL && 1 < ew(body, 0x70))
        ENTITY->hit_state = 1;
}

// ---------------------------------------------------------------------------
// Action table (0x004c2a50)
// ---------------------------------------------------------------------------
static void plant42_action_select(void);   // 0x004657d0 / 0x004658f0

// 0x00465a00 - behaviour 0: idle sway, also the SCD table's entry 0.
static void plant42_behavior_idle(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        p42_ticks() = (short)((rand() & 0xf) + 0xf);
        ENTITY->blend_counter = 0x3f;
        eb(ENTITY, 0x16c) = (signed char)(rand() & 0xf);
        eb(ENTITY, 0x16d) = (signed char)(rand() & 7);
        eb(ENTITY, 0x16c) = (signed char)((1 - (rand() & 2)) * eb(ENTITY, 0x16c));
        eb(ENTITY, 0x16d) = (signed char)((1 - (rand() & 2)) * eb(ENTITY, 0x16d));
        if (ENTITY->animationId == 8) ENTITY->animationId = 9;
        ENTITY->angle = (short)((ENTITY->angle - (eb(ENTITY, 0x16c) / 2)) & 0xfff);
        ENTITY->angle_z -= (short)(eb(ENTITY, 0x16d) / 2);
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        break;
    case 2:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(1, 0x40));
        break;
    case 3:
        ENTITY->action_state = 0;
        break;
    default:
        break;
    }

    if ((ENTITY->behavior_flags & 0x40) == 0) {
        ENTITY->angle = (short)((eb(ENTITY, 0x16c) + (unsigned short)ENTITY->angle) & 0xfff);
        entity_rotate_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 8);
    }

    g_playerDisplacement = (int)ENTITY->angle_z;
    ENTITY->angle_z += eb(ENTITY, 0x16d);
    if (((unsigned int)(ENTITY->angle_z - 0x3e) & 0xfff) < 0xe32)
        ENTITY->angle_z -= eb(ENTITY, 0x16d);

    // The head joint has risen above the arena: snap back down and reroll.
    if (-600 < jwt(14)[1]) {
        ENTITY->angle_z = (short)g_playerDisplacement - 0x10;
        ENTITY->animationId = s_plant42IdleAnims[rand() % 10];
        ENTITY->blend_counter = 0x3f;
    }

    short t = p42_ticks();
    p42_ticks() = (short)(t - 1);
    if (t == 0) {
        ENTITY->animationId = s_plant42IdleAnims[rand() % 10];
        ENTITY->blend_counter = 0x3f;
    }
}

// 0x00465cb0 - behaviour 1: horizontal sweep.
// The three per-direction lookups in the original are overlapping reads of one
// 8-byte stack blob { 0,1, 1,1,0,0xFF }; they reduce to these three values.
// They are read at point of use, not on entry: state 0 falls through into
// state 1 after choosing the direction.
#define P42_SWEEP_DIR   (eb(ENTITY, 0x174))
#define P42_SWEEP_ANIM_A ((char)(P42_SWEEP_DIR ? 0 : 1))   // &local_7 + dir*3
#define P42_SWEEP_ANIM_B ((char)(P42_SWEEP_DIR ? 1 : 0))   // &local_8 + dir*3
#define P42_SWEEP_SIGN  (P42_SWEEP_DIR ? -1 : 1)           // local_6[dir*3]
static void plant42_behavior_sweep(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0c;
        p42_ticks() = 0x32;
        eb(ENTITY, 0x174) = 1;
        if ((short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 1) < 0)
            eb(ENTITY, 0x174) = 0;
        // fall through
    case 1: {
        p42_anim(P42_SWEEP_ANIM_A, 0x40);
        ENTITY->angle += (short)(P42_SWEEP_SIGN * 0x10);
        if (jwt(14)[1] < -0x9c4) ENTITY->angle_z += 8;
        if (-1000 < jwt(14)[1])  ENTITY->angle_z -= 8;
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 2;
        break;
    }
    case 2:
        p42_ticks() = 0xf;
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        // fall through
    case 3: {
        p42_anim(P42_SWEEP_ANIM_B, 0x40);
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 4;
        break;
    }
    case 4:
        ENTITY->action_state = 5;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0c;
        // fall through
    case 5:
        p42_step() += 4;
        if (0x28 < p42_step()) p42_step() += 0x1c;
        p42_anim(P42_SWEEP_ANIM_A, 0x40);
        ENTITY->angle -= (short)(P42_SWEEP_SIGN * p42_step());
        if (0xc0 < p42_step()) {
            ENTITY->action_state = 6;
            Play3DSnd(2, 3, 0, joint_sound_pos(14));
        }
        plant42_sweep_hit();
        break;
    case 6:
        ENTITY->angle_z -= 8;
        p42_anim(P42_SWEEP_ANIM_B, 0x40);
        plant42_sweep_hit();
        if (p42_step() < 1) {
            ENTITY->action_state = 7;
            ENTITY->angle += (short)(P42_SWEEP_SIGN * -8);
            break;
        }
        ENTITY->angle -= (short)(P42_SWEEP_SIGN * p42_step());
        p42_step() -= 0x10;
        plant42_rearm_hit_flag();
        break;
    case 7:
        ENTITY->angle += (short)(P42_SWEEP_SIGN * 8);
        set_state_word(1);
        p42_step() = 0;
        break;
    default:
        break;
    }
}

// 0x004662b0 - behaviour 2: acid spit.
static void plant42_behavior_spit(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 7;
        ENTITY->hit_state = 1;
        p42_step() = 4;
        // fall through
    case 1: {
        if (p42_anim(0, 0x40) != 0) {
            ENTITY->action_state = 2;
            ENTITY->angle_z -= p42_step();
            p42_step() -= 0x30;
            Play3DSnd(2, 1, 0, joint_sound_pos(14));
            return;
        }
        Entity* body = plant42_body();
        g_playerDisplacement = (int)(body ? (unsigned int)body->hit_state : 0u) * 0x20 + 0x10;
        turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                           (short)g_playerDisplacement);
        if (ENTITY->animation_frame_id < 8) {
            ENTITY->angle_z += 0x40;
            return;
        }
        if (0xc < ENTITY->animation_frame_id) {
            ENTITY->angle_z -= p42_step();
            p42_step() += 0x18;
        }
        g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
        if (10 < ENTITY->animation_frame_id && g_playerEntity.isBeingAttackedFlag == 0 &&
            FUN_0048ae00(jw(15), &g_playerPosScratch, 0x4b0,
                         &g_playerEntity.scaMatrixData.localMatrix.t[0]) != 0 &&
            -0x9c4 < jwt(15)[1]) {
            g_playerEntity.isBeingAttackedFlag = 1;
            *(unsigned int*)&g_playerEntity.animationId = 0x00640002;
            p42_step() -= 8;
            if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)
                g_playerEntity.health -= 8;
            else
                g_playerEntity.health -= 15;
            if (g_playerEntity.health < 0) g_playerEntity.health = 1;
            Play3DSnd(2, 2, 0, joint_sound_pos(14));
            return;
        }
        break;
    }
    case 2:
        ENTITY->angle_z -= p42_step();
        p42_step() -= 8;
        if (p42_step() < 0) ENTITY->action_state = 3;
        break;
    case 3:
        ENTITY->animationId = 2;
        ENTITY->action_state = 4;
        p42_step() = (short)((rand() & 1) * -0x20 + 0x10);
        ENTITY->blend_counter = 0x3f;
        // fall through
    case 4:
        ENTITY->angle_z += 0x10;
        if (-400 < ENTITY->angle_z) ENTITY->action_state = 5;
        ENTITY->angle += p42_step();
        p42_anim(0, 0x40);
        break;
    case 5:
        ENTITY->angle += (short)(p42_step() / 2);
        set_state_word(1);
        ENTITY->animationId = 4;
        plant42_rearm_hit_flag();
        break;
    default:
        break;
    }
}

// 0x00466640 - behaviour 3: approach, grab, lift.  This is the branch that
// actually picks the player up in the boss fight.
static void plant42_behavior_move(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 8;
        p42_ticks() = 0x3c;
        p42_step() = 8;
        // fall through
    case 1: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 5;
        ENTITY->angle -= 0x28;
        ENTITY->angle += (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x10);
        ENTITY->angle += 0x28;
        p42_anim(1, 0x40);
        int headY = jwt(14)[1];
        if (headY < -0x9c4) {
            ENTITY->angle_z += 0x10;
        } else if (headY < -999) {
            if (p42_ticks() < 0x1e) {
                ENTITY->action_state = 2;
                p42_ticks() = 0x3c;
            }
        } else {
            ENTITY->angle_z -= 0x10;
        }
        break;
    }
    case 2: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0 || g_playerEntity.isBeingAttackedFlag != 0) ENTITY->action_state = 5;
        ENTITY->angle -= 0x30;
        ENTITY->angle += (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x10);
        ENTITY->angle += 0x30;

        g_playerDisplacement =
            abs(g_playerEntity.scaMatrixData.localMatrix.t[0] - jwt(14)[0]) +
            abs(g_playerEntity.scaMatrixData.localMatrix.t[2] - jwt(14)[2]);
        player_distance_z =
            abs(g_playerEntity.scaMatrixData.localMatrix.t[2] - jwt(12)[2]) +
            abs(g_playerEntity.scaMatrixData.localMatrix.t[0] - jwt(12)[0]);

        if ((799 < (int)g_playerDisplacement && 499 < player_distance_z) ||
            (g_playerEntity.healthStatusFlags & 8) != 0 ||
            g_playerEntity.isBeingAttackedFlag != 0 ||
            g_playerEntity.scaMatrixData.localMatrix.t[1] != 0 ||
            (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x20) != -0x20) {
            p42_anim(1, 0x40);
            ENTITY->angle_z += p42_step();
            if (-0x5dc < jwt(14)[1] && p42_step() >= 0) p42_step() = (short)-p42_step();
            if (jwt(14)[1] < -0x9c4 && p42_step() < 0)  p42_step() = (short)-p42_step();
            break;
        }

        ENTITY->action_state = 3;
        ENTITY->animationId = 0x0e;
        ENTITY->hit_state = 1;
        eb(ENTITY, 0x175) = 0xf;                       // grab joint = 15
        g_playerEntity.animationId = 6;
        g_playerEntity.animFrameId = 8;
        g_playerEntity.action_behavior = 1;
        g_playerEntity.action_state = 0;
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.speed.y   = (short)((jwt(11)[0] - g_playerEntity.scaMatrixData.localMatrix.t[0]) / 6);
        g_playerEntity.speed.z   = (short)(((jwt(11)[1] - g_playerEntity.scaMatrixData.localMatrix.t[1]) + 0x7ef) / 6);
        g_playerEntity.speed.pad = (short)((jwt(11)[2] - g_playerEntity.scaMatrixData.localMatrix.t[2]) / 6);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        // fall through
    }
    case 3: {
        const int grabJoint = eb(ENTITY, 0x175);
        if (3 < ENTITY->animation_frame_id) {
            g_playerPosScratch.x = -0x60f;
            g_playerPosScratch.y = 0;
            g_playerPosScratch.z = 700;
            ApplyMatrixLV(jw(grabJoint), &g_playerPosScratch, &g_playerPosScratch);
            g_playerEntity.speed.y   = (short)(((jwt(grabJoint)[0] - g_playerEntity.scaMatrixData.localMatrix.t[0]) + g_playerPosScratch.x) / 2);
            g_playerEntity.speed.z   = (short)(((jwt(grabJoint)[1] - g_playerEntity.scaMatrixData.localMatrix.t[1]) + g_playerPosScratch.y) / 2);
            g_playerEntity.speed.pad = (short)(((jwt(grabJoint)[2] - g_playerEntity.scaMatrixData.localMatrix.t[2]) + g_playerPosScratch.z) / 2);
        }
        if (-2000 < jwt(14)[1]) ENTITY->angle_z -= 0x10;
        g_playerEntity.scaMatrixData.localMatrix.t[0] += g_playerEntity.speed.y;
        g_playerEntity.scaMatrixData.localMatrix.t[1] += g_playerEntity.speed.z;
        g_playerEntity.scaMatrixData.localMatrix.t[2] += g_playerEntity.speed.pad;
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x400));
        break;
    }
    case 4:
        Play3DSnd(2, 5, 0, joint_sound_pos(14));
        set_state_word(0x00040101);                     // -> behaviour 4
        break;
    case 5:
        p42_ticks() = 0x14;
        ENTITY->action_state = 6;
        p42_step() = 0;
        if (-2000 < jwt(14)[1]) p42_step() = 0x10;
        ENTITY->animationId = 5;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        // fall through
    case 6: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 7;
        ENTITY->angle_z -= p42_step();
        ENTITY->angle += 8;
        p42_anim(0, 0x40);
        break;
    }
    case 7:
        set_state_word(1);
        ENTITY->animationId = 9;
        plant42_rearm_hit_flag();
        break;
    default:
        break;
    }
    plant42_player_billboard();
}

// 0x00466cf0 - behaviour 4: held aloft, squeezed, dropped.
static void plant42_behavior_hold(void)
{
    const int grabJoint = eb(ENTITY, 0x175);

    switch (ENTITY->action_state) {
    case 0:
        p42_ticks() = 10;
        ENTITY->action_state = 1;
        p42_step() = 0x18;
        g_playerEntity.flags |= 2;
        plant42_capture_setup(grabJoint);
        g_plant42CaptureMatrix.t[0] = -0x60f;
        g_plant42CaptureMatrix.t[1] = 0;
        g_plant42CaptureMatrix.t[2] = 700;
        Play3DSnd(2, 6, 0, joint_sound_pos(14));
        Play3DSnd(2, (g_playerEntity.id & 1) + 0x17, 0, player_sound_pos());
        // fall through
    case 1: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 2;
        ENTITY->angle_z -= p42_step();
        plant42_hold_player(grabJoint);
        break;
    }
    case 2:
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0f;
        if (g_playerEntity.health < 0x28) {
            set_state_word(0x00080101);                 // low health -> behaviour 8 (kill)
            break;
        }
        // fall through
    case 3:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        ENTITY->angle_z -= 4;
        plant42_hold_player(grabJoint);
        break;
    case 4:
        p42_ticks() = 0xf;
        ENTITY->action_state = 5;
        // fall through
    case 5: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 6;
        ENTITY->angle_z -= 8;
        plant42_hold_player(grabJoint);
        break;
    }
    case 6:
        ENTITY->action_state = 7;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1f;
        ENTITY->animationId = 0;
        p42_step() = 8;
        // fall through
    case 7:
        ENTITY->angle_z += p42_step();
        p42_step() += 1;
        if (g_playerEntity.scaMatrixData.localMatrix.t[1] < -0x5db) {
            p42_anim(0, 0x80);
            plant42_hold_player(grabJoint);
            break;
        }
        Play3DSnd(2, 0x1a, 0, player_sound_pos());
        // fall through
    case 8:
        ENTITY->action_state = 9;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = 0;
        g_playerEntity.zoneFlags &= 0x7f;
        g_playerEntity.flags &= 0xfd;
        ENTITY->animationId = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        g_playerEntity.animationId = 6;
        g_playerEntity.animFrameId = 8;
        g_playerEntity.action_behavior = 2;
        g_playerEntity.action_state = 0;
        if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)
            g_playerEntity.health -= 0x14;
        else
            g_playerEntity.health -= 0x28;
        // fall through
    case 9:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        ENTITY->angle_z -= 2;
        break;
    case 10:
        plant42_rearm_hit_flag();
        set_state_word(1);
        break;
    default:
        break;
    }
    plant42_player_billboard();
}

// 0x004671d0 - behaviour 5: the alternate grab that uses joint 15 directly.
static void plant42_behavior_grab_alt(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 8;
        p42_ticks() = 0x1e;
        // fall through
    case 1: {
        ENTITY->angle -= 0x30;
        ENTITY->angle += (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x10);
        ENTITY->angle += 0x30;
        p42_anim(1, 0x40);
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) {
            ENTITY->action_state = 2;
            ENTITY->animationId = 0x0e;
            g_playerEntity.speed.y   = (short)((jwt(11)[0] - g_playerEntity.scaMatrixData.localMatrix.t[0]) / 6);
            g_playerEntity.speed.z   = (short)(((jwt(11)[1] - g_playerEntity.scaMatrixData.localMatrix.t[1]) + 0x7ef) / 6);
            g_playerEntity.speed.pad = (short)((jwt(11)[2] - g_playerEntity.scaMatrixData.localMatrix.t[2]) / 6);
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 3;
        }
        if (jwt(14)[1] < -0x9c4) ENTITY->angle_z += 0x10;
        if (-1000 < jwt(14)[1])  ENTITY->angle_z -= 0x10;
        break;
    }
    case 2:
        if (3 < ENTITY->animation_frame_id) {
            g_playerPosScratch.x = -0x60f;
            g_playerPosScratch.y = 0;
            g_playerPosScratch.z = 700;
            ApplyMatrixLV(jw(15), &g_playerPosScratch, &g_playerPosScratch);
            g_playerEntity.speed.y   = (short)(((jwt(15)[0] - g_playerEntity.scaMatrixData.localMatrix.t[0]) + g_playerPosScratch.x) / 2);
            g_playerEntity.speed.z   = (short)(((jwt(15)[1] - g_playerEntity.scaMatrixData.localMatrix.t[1]) + g_playerPosScratch.y) / 2);
            g_playerEntity.speed.pad = (short)(((jwt(15)[2] - g_playerEntity.scaMatrixData.localMatrix.t[2]) + g_playerPosScratch.z) / 2);
        }
        g_playerEntity.scaMatrixData.localMatrix.t[0] += g_playerEntity.speed.y;
        g_playerEntity.scaMatrixData.localMatrix.t[1] += g_playerEntity.speed.z;
        g_playerEntity.scaMatrixData.localMatrix.t[2] += g_playerEntity.speed.pad;
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x400));
        break;
    case 3:
        p42_ticks() = 10;
        ENTITY->action_state = 4;
        p42_step() = 0x18;
        g_playerEntity.flags |= 2;
        plant42_capture_setup(15);
        g_plant42CaptureMatrix.t[0] = -0x60f;
        g_plant42CaptureMatrix.t[1] = 0;
        g_plant42CaptureMatrix.t[2] = 700;
        Play3DSnd(2, 5, 0, joint_sound_pos(14));
        // fall through
    case 4: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) {
            ENTITY->action_state = 5;
            Play3DSnd(2, 6, 0, joint_sound_pos(14));
            Play3DSnd(2, (g_playerEntity.id & 1) + 0x17, 0, player_sound_pos());
        }
        ENTITY->angle_z -= p42_step();
        plant42_hold_player(15);
        break;
    }
    case 5:
        plant42_hold_player(15);
        break;
    default:
        break;
    }
    plant42_player_billboard();
}

// 0x004675e0 - behaviour 6: the player is held motionless in the vine.
// This is SCD table entry 0x0B, which is how room 40C0 keeps Chris suspended
// while the Rebecca sequence plays.
static void plant42_behavior_suspend(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animationId = 0x0e;
        ENTITY->animation_frame_id = 4;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        p42_anim(0, 0x400);
        // Chris (id bit 0 clear) gets a hand-authored hold orientation.
        if ((g_playerEntity.id & 1) == 0 && (ENTITY->behavior_flags & 0xf) == 6)
            plant42_orient_from_matrix(s_plant42ChrisHoldMatrix, &g_plant42CaptureMatrix);
        eb(ENTITY, 0x16c) = 1;
        eb(ENTITY, 0x16d) = 1;
        ew(ENTITY, 0x16e) = 3;
        ew(ENTITY, 0x170) = 6;
        p42_step() = 1;
    } else if (ENTITY->action_state != 1) {
        plant42_player_billboard();
        return;
    }

    short t = p42_step();
    p42_step() = (short)(t - 1);
    if (t == 0) {
        ENTITY->angle   -= ew(ENTITY, 0x16e);
        ENTITY->angle_z -= ew(ENTITY, 0x170);
        ew(ENTITY, 0x16e) -= (short)eb(ENTITY, 0x16c);
        ew(ENTITY, 0x170) -= (short)eb(ENTITY, 0x16d);
        if (8 < (unsigned short)(ew(ENTITY, 0x16e) + 4))
            eb(ENTITY, 0x16c) = (signed char)-eb(ENTITY, 0x16c);
        if (0xe < (unsigned short)(ew(ENTITY, 0x170) + 7))
            eb(ENTITY, 0x16d) = (signed char)-eb(ENTITY, 0x16d);
        p42_step() = 1;
    }
    plant42_hold_player(15);
    plant42_player_billboard();
}

// 0x004677e0 - behaviour 7: release the held player.  SCD table entry 0x0C.
// Case 6 is where room 40C0 puts Chris back on his feet; the destination is
// hard-coded in the original and MUST NOT be replaced by a saved pre-grab
// position - that is what dropped him outside the room's collision.
static void plant42_behavior_release(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0f;
        g_playerEntity.animationId = 6;
        g_playerEntity.animFrameId = 8;
        g_playerEntity.action_behavior = 1;
        g_playerEntity.action_state = 0;
        Play3DSnd(2, 6, 0, joint_sound_pos(14));
        Play3DSnd(2, (g_playerEntity.id & 1) + 0x17, 0, player_sound_pos());
        // fall through
    case 1:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        ENTITY->angle_z -= 4;
        plant42_hold_player(15);
        break;
    case 2:
        p42_ticks() = 0xf;
        ENTITY->action_state = 3;
        // fall through
    case 3: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 4;
        ENTITY->angle_z -= 8;
        plant42_hold_player(15);
        break;
    }
    case 4:
        ENTITY->action_state = 5;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0;
        p42_step() = 8;
        // fall through
    case 5:
        ENTITY->angle_z += p42_step();
        p42_step() += 1;
        if (g_playerEntity.scaMatrixData.localMatrix.t[1] < -0x5db) {
            p42_anim(0, 0x40);
            plant42_hold_player(15);
            break;
        }
        Play3DSnd(2, 0x1a, 0, player_sound_pos());
        // fall through
    case 6:
        ENTITY->action_state = 7;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = 0;
        g_playerEntity.zoneFlags &= 0x7f;
        g_playerEntity.flags &= 0xfd;
        ENTITY->animationId = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.animationId = 6;
        g_playerEntity.animFrameId = 8;
        g_playerEntity.action_behavior = 2;
        g_playerEntity.action_state = 0;
        g_playerEntity.scaMatrixData.localMatrix.t[0] = 0x1757;
        g_playerEntity.scaMatrixData.localMatrix.t[2] = 0x3bbd;
        g_playerEntity.directionAngle = 0x499;
        // fall through
    case 7:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        ENTITY->angle_z -= 2;
        break;
    case 8:
        set_state_word(8);
        break;
    default:
        break;
    }
    plant42_player_billboard();
}

// 0x00467b40 - behaviour 8: the kill animation (player health too low).
static void plant42_behavior_kill(void)
{
    const int grabJoint = eb(ENTITY, 0x175);

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        p42_ticks() = 0x1e;
        Play3DSnd(2, 6, 0, joint_sound_pos(14));
        // fall through
    case 1: {
        plant42_hold_player(grabJoint);
        SVECTOR* rot = (SVECTOR*)((char*)ENTITY->jointsStructs + 0x5d4);   // joint 12 rotation
        eub(ENTITY->jointsStructs, 0x5d0) &= 0xfd;
        rot->x = (short)(rot->x + 2);
        RotMatrix(rot, (MATRIX*)((char*)ENTITY->jointsStructs + 0x5f4));
        ApplyLVAndMul0Matrix((char*)ENTITY->jointsStructs + 0x598,
                             (char*)ENTITY->jointsStructs + 0x5f4,
                             (char*)ENTITY->jointsStructs + 0x614);
        ENTITY->angle_z -= 1;
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) {
            ENTITY->action_state = 2;
            p42_ticks() = 0x5a;
            *(unsigned int*)&g_playerEntity.animationId = 0x04010806;
            p42_step() = 0x18;
        }
        break;
    }
    case 2: {
        g_playerDisplacement = (int)(rand() & 3);
        ENTITY->angle += (short)((int)p42_step() / (g_playerDisplacement + 1));
        ENTITY->angle_z -= 1;
        plant42_hold_player(grabJoint);
        ApplyLVAndMul0Matrix((char*)ENTITY->jointsStructs + 0x598,
                             (char*)ENTITY->jointsStructs + 0x5f4,
                             (char*)ENTITY->jointsStructs + 0x614);
        p42_step() = (short)-p42_step();
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) {
            JointStruct* headJoint = (JointStruct*)((char*)ENTITY->jointsStructs + 0x45c);
            ENTITY->action_state = 3;
            p42_ticks() = 0x50;
            JointApplyColorTint(headJoint, 0x30, 0x80820, (void*)0x00606060);
            JointApplyColorTint(headJoint, 0x30, 0x80820, (void*)0x00606060);
            JointApplyColorTint(headJoint, 0x30, 0x80820, (void*)0x00606060);
            JointStruct* pj = g_playerEntity.jointsStructs;
            g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            Effect_CreateBillboard(0, 3, 0, &pj[0].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 3, 0, &pj[2].world, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0, 0, 0, &pj[1].world, &g_playerPosScratch, 0);
            pj[2].flags |= 8;
            pj[0].flags |= 0x10;
            g_playerEntity.animationId = 7;
            g_playerEntity.animFrameId = 8;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state = 0;
            return;
        }
        break;
    }
    case 3: {
        plant42_hold_player(grabJoint);
        SVECTOR* rot = (SVECTOR*)((char*)ENTITY->jointsStructs + 0x5d4);
        rot->x = (short)(rot->x + 0x10);
        RotMatrix(rot, (MATRIX*)((char*)ENTITY->jointsStructs + 0x5f4));
        ApplyLVAndMul0Matrix((char*)ENTITY->jointsStructs + 0x598,
                             (char*)ENTITY->jointsStructs + 0x5f4,
                             (char*)ENTITY->jointsStructs + 0x614);
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 4;
        break;
    }
    case 4:
        set_state_word(1);
        eub(ENTITY->jointsStructs, 0x5d0) |= 2;
        *(unsigned int*)&g_playerEntity.animationId = 0x02000807;
        break;
    default:
        break;
    }
}

// 0x00467ed0 - behaviour 9: the death/withering sequence.
static void plant42_behavior_wither(void)
{
    Entity* body = plant42_body();
    Entity* roots = plant42_roots();

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0d;
        euw(ENTITY, 0xca) = 0x1000;
        if ((ENTITY->behavior_flags & 1) == 0) {
            if (body)  { ew(body, 0x7e) = 7; eu(body, 0x84) = 1; }
            if (roots) { eu(roots, 0x84) = 1; }
            Snd_em(7);
        }
        // fall through
    case 1:
        ew(ENTITY, 0xca) -= 0x20;
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        return;
    case 2:
        p42_ticks() = 0xd2;
        ENTITY->action_state = 3;
        if ((ENTITY->behavior_flags & 1) == 0) {
            if (body)  { ew(body, 0x7e) = 4; body->action_state = 2; }
            if (roots) { roots->action_state = 2; }
        }
        // fall through
    case 3: {
        short t = p42_ticks();
        p42_ticks() = (short)(t - 1);
        if (t == 0) ENTITY->action_state = 4;
        return;
    }
    case 4:
        ENTITY->action_state = 5;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 2;
        if ((ENTITY->behavior_flags & 1) == 0) {
            if (body)  { ew(body, 0x7e) = 0xe; body->action_state = 3; }
            if (roots) { roots->action_state = 3; }
        }
        // fall through
    case 5:
        ew(ENTITY, 0xca) += 0x20;
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        break;
    case 6:
        set_state_word(1);
        euw(ENTITY, 0xca) = 0;
        ENTITY->hit_state = 0;
        if ((ENTITY->behavior_flags & 1) == 0) {
            if (body)  { ew(body, 0x7e) = 0; eu(body, 0x84) = 0; }
            if (roots) { eu(roots, 0x84) = 0; }
        }
        if ((ENTITY->behavior_flags & 0x40) != 0) ENTITY->state = 8;
        break;
    default:
        break;
    }
}

// 0x004657d0 / 0x004658f0 - the idle action selector, also used as the
// behaviour handler for table slots 10-13 and 15.
static void plant42_action_select(void)
{
    static const unsigned char kActions[8] = { 1, 2, 3, 1, 3, 1, 1, 2 };
    if (g_playerEntity.isBeingAttackedFlag != 0) return;

    if ((int)g_playerDisplacement < 11000) {
        ENTITY->angle += (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 4);
    }
    if ((int)g_playerDisplacement < 10000 &&
        (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x80) == 0 &&
        g_playerEntity.isBeingAttackedFlag == 0) {
        set_state_word(0x00010101);
        ENTITY->action_behavior = (unsigned char)(ENTITY->action_behavior + (rand() & 1));
    }
    if ((int)g_playerDisplacement < 9000 &&
        (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x100) == 0 &&
        g_playerEntity.isBeingAttackedFlag == 0) {
        set_state_word(0x00030101);
        ENTITY->action_behavior = kActions[rand() & 7];
    }
}

// 0x00468170 - the hit-reaction behaviour (SCD table entry 2's payload).
static void plant42_behavior_recoil(void)
{
    static const unsigned char kHitAnims[8] = { 2, 3, 4, 5, 10, 0xb, 0xc, 0xd };

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = kHitAnims[rand() & 7];
        p42_step() = (short)((rand() & 1) * -0xc0 + 0x60);
        eb(ENTITY, 0x175) = 8;
        p42_ticks() = (short)((rand() & 7) + 1);
        if ((ENTITY->behavior_flags & 1) == 0) p42_ticks() = 6;
        if ((rand() & 1) != 0) {
            ENTITY->blend_counter = 7;
            ENTITY->action_state = 3;
            ENTITY->animationId = 0;
            Play3DSnd(2, 1, 0, joint_sound_pos(14));
            return;
        }
        // fall through
    case 1:
        if (0xc0 < (unsigned short)(p42_step() + 0x60))
            eb(ENTITY, 0x175) = (signed char)-eb(ENTITY, 0x175);
        p42_step() -= (short)eb(ENTITY, 0x175);
        if (p42_step() == 0) {
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 0x3f;
            ENTITY->animationId = kHitAnims[rand() & 7];
            p42_ticks() -= 1;
            if (p42_ticks() == 0) ENTITY->action_state = 2;
        }
        // 0x0043d8a0: a heavy weapon cancels the flinch every fifth frame.
        if (0x6e < g_playerEntity.equippedWeaponId && ENTITY->animation_frame_id % 5 == 0)
            ENTITY->hit_state = 0;
        if (ENTITY->angle_z < -0x17c) ENTITY->angle_z += 0x10;
        ENTITY->angle -= p42_step();
        p42_anim(0, 0x40);
        break;
    case 2: {
        set_state_word(1);
        ENTITY->hit_state = 0;
        ENTITY->animationId = 2;
        Entity* body = plant42_body();
        if ((ENTITY->behavior_flags & 1) == 0 && body != NULL) {
            body->hit_state = 0;
            body->behavior_flags = 0;
            if (1 < ew(body, 0x70)) ENTITY->hit_state = 1;
        }
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            set_state_word(8);
            return;
        }
        break;
    }
    case 3:
        p42_anim(0, 0x200);
        entity_rotate_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x40);
        if (-0x254 < ENTITY->angle_z) ENTITY->angle_z -= 0x40;
        if (ENTITY->blend_counter == 0) {
            ENTITY->action_state = 1;
            return;
        }
        break;
    default:
        break;
    }
}

// 0x004684a0 - the SCD-driven flinch (SCD table entry 16).
static void plant42_behavior_scd_flinch(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0d;
        p42_ticks() = 3;
        // fall through
    case 1:
        if ((ENTITY->hit_state & 7) != 0) ENTITY->angle_z -= 8;
        if ((ENTITY->behavior_flags & 0x40) != 0 && (ENTITY->animation_frame_id & 7) == 0) {
            g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            g_animFrameIdSave = (unsigned int)(rand() % 5);
            Effect_CreateBillboard(0x0e, 3, 0, jw((int)g_animFrameIdSave), &g_playerPosScratch, 0);
            Effect_CreateBillboard(9, 0, 0, jw((int)g_animFrameIdSave), &g_playerPosScratch, 0);
            eub(ENTITY, 0x177) = 10;
        }
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        break;
    case 2:
        if ((ENTITY->hit_state & 7) != 0) ENTITY->angle_z += 8;
        if ((ENTITY->behavior_flags & 0x40) != 0 && (ENTITY->animation_frame_id & 7) == 0) {
            g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            g_animFrameIdSave = (unsigned int)(rand() % 5);
            Effect_CreateBillboard(0x0e, 3, 0, jw((int)g_animFrameIdSave), &g_playerPosScratch, 0);
            Effect_CreateBillboard(9, 0, 0, jw((int)g_animFrameIdSave), &g_playerPosScratch, 0);
        }
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(1, 0x40));
        return;
    case 3: {
        set_state_word(1);
        ENTITY->hit_state = 0;
        Entity* body = plant42_body();
        if ((ENTITY->behavior_flags & 1) == 0 && body != NULL) {
            body->hit_state = 0;
            body->behavior_flags = 0;
            if (1 < ew(body, 0x70)) ENTITY->hit_state = 1;
        }
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            ENTITY->state = 8;
            return;
        }
        break;
    }
    default:
        break;
    }
}

// 0x00468760 - the death fall (SCD table entry 14's payload).
static void plant42_behavior_death(void)
{
    switch (ENTITY->action_state) {
    case 0: {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3f;
        ENTITY->animationId = 0x0d;
        euw(ENTITY, 0xca) = 0x1000;
        p42_ticks() = 0;
        eub(ENTITY, 0x183) = 5;
        eb(ENTITY, 0x182) = (signed char)((rand() % 0xe) + 0x0f);
        p42_step() = (short)((rand() & 7) + 8);
        p42_step() = (short)(((rand() & 1) * -2 + 1) * p42_step());
        euw(ENTITY, 0x16e) = (unsigned short)((rand() & 1) << 11);
        euw(ENTITY, 0x170) = (unsigned short)((rand() & 1) * 0x800 + 0x400);
        eub(ENTITY, 0x16c) = (unsigned char)((rand() & 1) * -0x40 + 0x20);
        eb(ENTITY, 0x16d)  = (signed char)((char)rand() * -0x80 + 0x40);
        if ((ENTITY->behavior_flags & 1) == 0) {
            Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
            Snd_em(7);
            g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            Effect_CreateBillboard(0, 0x1b, 0, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        }
        Play3DSnd(2, 9, 0, joint_sound_pos(9));
        // fall through
    }
    case 1: {
        p42_anim(0, 0x40);
        p42_anim(0, 0x40);
        ENTITY->angle_z += 0x10;
        ew(ENTITY, 0xca) -= 0x10;
        signed char c = eb(ENTITY, 0x182);
        eb(ENTITY, 0x182) = (signed char)(c - 1);
        if (c == 0) {
            ENTITY->action_state = 2;
            return;
        }
        break;
    }
    case 2:
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + (char)p42_anim(0, 0x40));
        // fall through
    case 3: {
        ENTITY->animationId = 2;
        if (1000 < euw(ENTITY, 0xca)) {
            euw(ENTITY, 0xca) -= 8;
            if (100 < (unsigned short)((short)jwt(7)[1] - (short)ENTITY->scaMatrixData.localMatrix.t[1] + 0x32) &&
                100 < (unsigned short)((short)jwt(14)[1] - (short)ENTITY->scaMatrixData.localMatrix.t[1] + 0x32)) {
                short pitch = ENTITY->position.pad;
                if (100 < (((unsigned int)(pitch - ew(ENTITY, 0x170)) + 0x32) & 0xfff))
                    ENTITY->position.pad = (short)(eb(ENTITY, 0x16d) + pitch);
                if (100 < (((unsigned int)(ENTITY->angle - ew(ENTITY, 0x16e)) + 0x32) & 0xfff))
                    ENTITY->angle += (short)eb(ENTITY, 0x16c);
            }
            ENTITY->scaMatrixData.localMatrix.t[1] += (int)p42_ticks();
            p42_ticks() += 5;
            if (-200 < ENTITY->scaMatrixData.localMatrix.t[1]) {
                p42_ticks() = (short)(p42_ticks() / -3);
                ENTITY->scaMatrixData.localMatrix.t[1] = -200;
                unsigned char b = eub(ENTITY, 0x183);
                eub(ENTITY, 0x183) = (unsigned char)(b - 1);
                if (b == 0) {
                    ENTITY->action_state = 4;
                    ENTITY->hit_state = 0x80;
                }
                if (eub(ENTITY, 0x183) == 4) {
                    Play3DSnd(2, 8, 0, joint_sound_pos(10));
                    g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
                    Effect_CreateBillboard(0, 0x1b, 0, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    g_playerPosScratch.y = -400;
                    if ((ENTITY->behavior_flags & 0x40) == 0) {
                        Effect_CreateBillboard(9, 0x11, 0, jw(2),  &g_playerPosScratch, 0);
                        Effect_CreateBillboard(9, 0x11, 0, jw(7),  &g_playerPosScratch, 0);
                        Effect_CreateBillboard(9, 0x11, 0, jw(13), &g_playerPosScratch, 0);
                    }
                    g_playerPosScratch.y = -400;
                    Effect_CreateBillboard(9, 0x11, 0, jw(4),  &g_playerPosScratch, 0);
                    Effect_CreateBillboard(9, 0x11, 0, jw(9),  &g_playerPosScratch, 0);
                    Effect_CreateBillboard(9, 0x11, 0, jw(14), &g_playerPosScratch, 0);
                }
            }
            p42_anim(0, 0x40);
            p42_anim(0, 0x40);
        }
        break;
    }
    case 4:
        if (euw(ENTITY, 0xca) < 0x12d) {
            if ((ENTITY->behavior_flags & 1) != 0) ENTITY->has_enter_switch_zone = 0;
            ENTITY->action_state = 5;
            return;
        }
        euw(ENTITY, 0xca) -= 0x20;
        BillboardAdjSize(&ENTITY->pushVelocity, (short)-21, (short)-1);
        if (100 < (unsigned short)((short)jwt(7)[1] - (short)ENTITY->scaMatrixData.localMatrix.t[1] + 0x32) &&
            100 < (unsigned short)((short)jwt(14)[1] - (short)ENTITY->scaMatrixData.localMatrix.t[1] + 0x32)) {
            short pitch = ENTITY->position.pad;
            if (100 < (((unsigned int)(pitch - ew(ENTITY, 0x170)) + 0x32) & 0xfff))
                ENTITY->position.pad = (short)(eb(ENTITY, 0x16d) + pitch);
            if (100 < (((unsigned int)(ENTITY->angle - ew(ENTITY, 0x16e)) + 0x32) & 0xfff))
                ENTITY->angle += (short)eb(ENTITY, 0x16c);
        }
        p42_anim(0, 0x40);
        p42_anim(0, 0x40);
        return;
    default:
        break;
    }
}

// 0x00465750 - the action-table dispatcher.
static void plant42_run_action(void)
{
    Entity* body = plant42_body();
    if (body != NULL && (eub(body, 0x89) & 0x80) != 0) {   // body health went negative
        set_state_word(0x00000103);
        ENTITY->hit_state = 1;
        ENTITY->health = -1;
        return;
    }

    switch (ENTITY->action_behavior) {
    case 0:  plant42_behavior_idle();      break;
    case 1:  plant42_behavior_sweep();     break;
    case 2:  plant42_behavior_spit();      break;
    case 3:  plant42_behavior_move();      break;
    case 4:  plant42_behavior_hold();      break;
    case 5:  plant42_behavior_grab_alt();  break;
    case 6:  plant42_behavior_suspend();   break;
    case 7:  plant42_behavior_release();   break;
    case 8:  plant42_behavior_kill();      break;
    case 9:  plant42_behavior_wither();    break;
    case 14: break;                                       // 0x004658e0 - no-op
    default: plant42_action_select();      break;         // 10-13, 15
    }
}

// 0x004657a0 - pick the next behaviour when the plant is idle.
static void plant42_action_dispatch(void)
{
    static const unsigned char kDefault[8] = { 1, 2, 3, 1, 3, 1, 1, 2 };
    static const unsigned char kFlags5[8]  = { 1, 2, 1, 1, 2, 1, 1, 2 };

    g_playerDisplacement = (int)p42_dist();

    const unsigned char kind = (unsigned char)(ENTITY->behavior_flags & 7);
    if (kind == 4) return;
    const unsigned char* table = (kind == 5) ? kFlags5 : kDefault;

    if (g_playerEntity.isBeingAttackedFlag != 0) return;

    if (g_playerDisplacement < 11000) {
        ENTITY->angle += (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 4);
    }
    if (g_playerDisplacement < 10000 &&
        (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x80) == 0 &&
        g_playerEntity.isBeingAttackedFlag == 0) {
        set_state_word(0x00010101);
        ENTITY->action_behavior = (unsigned char)(ENTITY->action_behavior + (rand() & 1));
    }
    if (g_playerDisplacement < 9000 &&
        (short)turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x100) == 0 &&
        g_playerEntity.isBeingAttackedFlag == 0) {
        set_state_word(0x00030101);
        ENTITY->action_behavior = table[rand() & 7];
    }
}

// ---------------------------------------------------------------------------
// The ambient room effects shared by state 1 and state 2 (identical code at
// 0x00465136 and 0x004651e7 in each).
// ---------------------------------------------------------------------------
static void plant42_ambient_effects(void)
{
    p42_life() = (short)(p42_life() - 1);
    if (p42_life() == 0)
        p42_life() = (short)((rand() & 0x3f) + 0x32);

    if ((ENTITY->behavior_flags & 1) == 0) {
        if (p42_life() == 1 && 0x1964 < p42_dist()) {
            g_playerPosScratch.y = -10000;
            g_playerPosScratch.x = (rand() & 0x1ff) + g_playerEntity.scaMatrixData.localMatrix.t[0] - 0x100;
            g_playerPosScratch.z = (rand() & 0x1ff) + g_playerEntity.scaMatrixData.localMatrix.t[2] - 0x100;
            if ((rand() & 1) == 0)
                Effect_CreateBillboard(0x1e, 1, 0, NULL, &g_playerPosScratch, 0);
        }
        if (p42_life() < 0x1e && 0x1964 < p42_dist() && (short)(p42_life() % 4) == 0) {
            g_playerPosScratch.y = -10000;
            g_playerPosScratch.x = g_playerEntity.scaMatrixData.localMatrix.t[0] - (rand() & 0x1ff) + 0x100;
            g_playerPosScratch.z = g_playerEntity.scaMatrixData.localMatrix.t[2] - (rand() & 0x1ff) + 0x100;
            Effect_CreateBillboard(0, 0x1c, 0, NULL, &g_playerPosScratch, 0);
        }
    }

    if ((short)(p42_life() % 0x32) == 0) {
        g_playerPosScratch.y = -10000;
        g_playerPosScratch.x = (int)s_plant42EffectX[rand() & 1] + (rand() & 0x7ff) - 0x400;
        g_playerPosScratch.z = (int)s_plant42EffectZ[rand() & 1] + (rand() & 0x7ff) - 0x400;
        Effect_CreateBillboard(0, 0x1c, 0, NULL, &g_playerPosScratch, 0);
    }
}

// The "last vine down" payoff, shared by 0x00465310 and plant42_die.
static void plant42_award_kill(void)
{
    Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_PLANT42_DEAD);
    if ((g_playerEntity.id & 1) != 0)
        g_message_flags |= 0x100;
    for (int i = 5; ; --i) {
        Entity& e = g_EnemiesList[i];
        if ((e.health & 0x8000) == 0) {
            e.state = 1;
            e.ignore_player_flag = 1;
            e.action_behavior = 9;
            e.action_state = 0;
            e.hit_state = 1;
        }
        if (i == 0) break;
    }
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------

// 0x00464e50 - state 1.
static void plant42_state_check(void)
{
    ENTITY->status_flags &= 0x1f;
    ENTITY->status_flags |= 0x40;

    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - ENTITY->scaMatrixData.localMatrix.t[0];
    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - ENTITY->scaMatrixData.localMatrix.t[2];
    p42_dist() = (unsigned int)SquareRoot0(dz * dz + dx * dx);
    if (p42_dist() < 10000) ENTITY->status_flags |= 0x80;

    if (ENTITY->ignore_player_flag == 0) {
        Entity* body = plant42_body();
        if (body != NULL && body->hit_state != 0) {
            set_state_word(0x00000102);
            if ((rand() & 3) == 0) ENTITY->action_behavior = 1;
            if ((ENTITY->behavior_flags & 1) == 0) return;
            if ((rand() & 3) == 0) return;
            set_state_word(0x00010101);
            ENTITY->action_behavior = (unsigned char)(ENTITY->action_behavior + ((rand() & 3) == 0 ? 1 : 0));
            return;
        }
        plant42_action_dispatch();
    }

    plant42_run_action();
    plant42_ambient_effects();
}

// 0x00465310 - the damage reaction driver behind state 2.
static void plant42_damage_react(void)
{
    Entity* body = plant42_body();
    if (body != NULL && (eub(body, 0x89) & 0x80) != 0) {
        set_state_word(0x00000103);
        ENTITY->hit_state = 1;
        ENTITY->health = -1;
        return;
    }

    if (ENTITY->ignore_player_flag != 0 || (ENTITY->hit_state & 7) == 0) {
        if (ENTITY->action_behavior == 0) { plant42_behavior_recoil(); return; }
        if (ENTITY->action_behavior == 1) { plant42_behavior_scd_flinch(); return; }
        return;
    }

    ENTITY->hit_state &= 0xf8;
    ENTITY->hit_state |= 1;
    set_state_word(0x00000102);
    if ((rand() & 1) != 0) ENTITY->action_behavior = 1;

    if ((ENTITY->hit_state & 0x78) == 8) {
        VECTOR* dead = reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
        g_playerPosScratch.x = dead->x;
        g_playerPosScratch.z = dead->z;
        g_playerPosScratch.pad = dead->pad;
        g_playerPosScratch.y = 200;
        Effect_CreateBillboard(0, 0x18, 0, &g_playerEntity.jointsStructs[0x0e].world,
                               &g_playerPosScratch, 0);
    } else if (body == NULL || (body->hit_state & 7) == 0) {
        g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
        // `joints + (rand()&7)*0x7c + 0x3a8` - 0x3a8 is joint 7's world matrix,
        // so the spray comes off one of joints 7..14.
        g_playerDisplacement = (int)(rand() & 7);
        Effect_CreateBillboard(0, 0x1b, 0, jw(7 + (int)g_playerDisplacement), &g_playerPosScratch, 0);
        g_playerPosScratch.x = g_playerEntity.scaMatrixData.localMatrix.t[0] - ENTITY->scaMatrixData.localMatrix.t[0];
        g_playerPosScratch.y = ENTITY->scaMatrixData.localMatrix.t[1];
        g_playerPosScratch.z = g_playerEntity.scaMatrixData.localMatrix.t[2] - ENTITY->scaMatrixData.localMatrix.t[2];
        VectorNormal(&g_playerPosScratch, &g_playerPosScratch);
        g_playerPosScratch.x = (g_playerPosScratch.x >> 1) + ENTITY->scaMatrixData.localMatrix.t[0];
        g_playerPosScratch.y = ENTITY->scaMatrixData.localMatrix.t[1] + 0x1194;
        g_playerPosScratch.z = (g_playerPosScratch.z >> 1) + ENTITY->scaMatrixData.localMatrix.t[2];
        Effect_CreateBillboard(0, 0x18, 0,
                               reinterpret_cast<void*>(static_cast<uintptr_t>(g_deadMoveValue)),
                               &g_playerPosScratch, 0);
    }

    if ((rand() & 1) != 0 && (ENTITY->behavior_flags & 1) != 0 && ENTITY->health < 10) {
        set_state_word(0x00000103);
        ENTITY->health = -1;
        if (body != NULL) {
            short remaining = (short)(ew(body, 0x70) - 1);
            ew(body, 0x70) = remaining;
            if (remaining != 1) return;
            plant42_award_kill();
        }
        return;
    }

    if (body != NULL) {
        if (ew(body, 0x70) < 2) ENTITY->hit_state = 0;
        body->hit_state = 1;
        body->behavior_flags = 1;
    }
}

// 0x00465130 - state 2.
static void plant42_damaged(void)
{
    plant42_damage_react();
    plant42_ambient_effects();
}

// 0x00465620 - state 3.
static void plant42_die(void)
{
    if (ENTITY->ignore_player_flag == 0) {
        set_state_word(0x00000103);
        Entity* body = plant42_body();
        if (body != NULL) {
            ew(body, 0x70) = (short)(ew(body, 0x70) - 1);
            player_distance_z = (int)ew(body, 0x70);
            if (player_distance_z == 1) plant42_award_kill();
        }
        if ((ENTITY->behavior_flags & 1) == 0) {
            Entity* roots = plant42_roots();
            if (body)  { body->health = -1; eu(body, 0x84) = 2; }
            if (roots) { eu(roots, 0x84) = 2; }
        }
    }
    if (ENTITY->action_behavior == 0)
        plant42_behavior_death();
}

// 0x004049f0 - state 8.  Note the SEPARATE action table at 0x004b1808; the
// action_behavior values the SCD writes here do NOT index the normal table.
static void plant42_scd_state(void)
{
    ENTITY->status_flags &= 0x1f;

    switch (ENTITY->action_behavior) {
    case 0:                                                // 0x00465a00
        plant42_behavior_idle();
        break;
    case 2: {                                              // 0x00404b80
        int step = (short)turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x20);
        g_playerDisplacement = step;
        ENTITY->angle += (short)step;
        if (g_playerDisplacement == 0)
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        plant42_behavior_idle();
        break;
    }
    case 10: plant42_behavior_grab_alt(); break;           // 0x004671d0
    case 11: plant42_behavior_suspend();  break;           // 0x004675e0
    case 12: plant42_behavior_release();  break;           // 0x004677e0
    case 13:                                               // 0x00404bd0
        if (ENTITY->action_state == 0) eub(ENTITY, 0x177) = 10;
        plant42_behavior_recoil();
        break;
    case 14: {                                             // 0x00404bf0
        if (ENTITY->ignore_player_flag == 0) {
            set_state_word(0x000e0108);
            Entity* body = plant42_body();
            Entity* roots = plant42_roots();
            if (body != NULL) {
                ew(body, 0x70) = (short)(ew(body, 0x70) - 1);
                player_distance_z = (int)ew(body, 0x70);
                if (player_distance_z == 1) ew(body, 0x70) = (short)(ew(body, 0x70) - 1);
            }
            if ((ENTITY->behavior_flags & 1) == 0) {
                if (body)  { body->health = -1; eu(body, 0x84) = 2; }
                if (roots) { eu(roots, 0x84) = 2; }
            }
        }
        plant42_behavior_death();
        break;
    }
    case 15: plant42_behavior_wither();     break;         // 0x00467ed0
    case 16: plant42_behavior_scd_flinch(); break;         // 0x004684a0
    default: break;                                        // 1, 3-9, 17-19 are NULL
    }

    unsigned char fxTimer = eub(ENTITY, 0x177);
    if (fxTimer != 0) {
        eub(ENTITY, 0x177) = (unsigned char)(fxTimer - 1);
        if (fxTimer == 1) {
            Entity* body = plant42_body();
            g_playerPosScratch = *reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            g_animFrameIdSave = (unsigned int)(rand() % 5);
            g_playerPosScratch.y = (rand() & 0xfff) + 0x200;
            if (body != NULL && -2000 < body->scaMatrixData.localMatrix.t[1])
                g_playerPosScratch.y = 0;
            g_playerPosScratch.x = 0x200 - (rand() & 0x3ff);
            g_playerPosScratch.z = 0x200 - (rand() & 0x3ff);
            if (body != NULL) {
                Effect_CreateBillboard(0x0e, 3, 0, &body->scaMatrixData.localMatrix, &g_playerPosScratch, 0x28);
                Effect_CreateBillboard(9, 0, 0, &body->scaMatrixData.localMatrix, &g_playerPosScratch, 0x28);
            }
            eub(ENTITY, 0x177) = 0x0d;
            if (body != NULL) {
                Play3DSnd(2, 0x1e, 0, (int)&body->scaMatrixData.localMatrix.t[0]);
                if (-300 < body->scaMatrixData.localMatrix.t[1]) eub(ENTITY, 0x177) = 0;
            }
        }
    }

    if ((ENTITY->behavior_flags & 0x40) == 0)
        set_state_word(1);
}

// ---------------------------------------------------------------------------
// The two companion entities
// ---------------------------------------------------------------------------
static inline unsigned char plant42_pulse(short counter)
{
    int idx = 0x59 - (int)counter;
    if (idx < 0) idx = 0;
    if (idx > 89) idx = 89;
    return s_plant42Pulse[idx];
}

// 0x00469fb0 - flower body, state 0 (alive).  ENTITY is the body here.
static void plant42_body_idle(void)
{
    int scale = (int)plant42_pulse(ew(ENTITY, 0xc4)) * 3 + 0x1000;
    ei(ENTITY, 0x16c) = scale;
    ei(ENTITY, 0x170) = scale;
    ei(ENTITY, 0x174) = scale;

    if (ew(ENTITY, 0xc4) == 0x41) Snd_em(0);

    short t = ew(ENTITY, 0xc4);
    ew(ENTITY, 0xc4) = (short)(t - 1);
    if (t == 0) ew(ENTITY, 0xc4) = 0x59;

    ew(ENTITY, 0x74) += ew(ENTITY, 0x7a);
    ew(ENTITY, 0x76) += ew(ENTITY, 0x6c);
    ew(ENTITY, 0x7a) -= ew(ENTITY, 0x7c);
    ew(ENTITY, 0x6c) -= ew(ENTITY, 0x6e);
    if (0x10 < (unsigned char)((char)ew(ENTITY, 0x7a) + 8))
        ew(ENTITY, 0x7c) = (short)-ew(ENTITY, 0x7c);
    if (0x0e < (unsigned char)((char)ew(ENTITY, 0x6c) + 7))
        ew(ENTITY, 0x6e) = (short)-ew(ENTITY, 0x6e);
}

// 0x0046a0e0 - flower body, state 1 (shrivelling) and 3 (regrowing).
static void plant42_body_shrivel(void)
{
    unsigned char st = ENTITY->action_state;
    if (st == 0) {
        ew(ENTITY, 0x78) = 0x80;
        scd_model_tint_apply(-1, -1, 0, 0, 0x200, 8);
        ENTITY->blend_counter = 0xf;
        ENTITY->action_state = 1;
        st = 1;
    }

    if (st == 1) {
        int d = ew(ENTITY, 0x78) + 0x18;
        ei(ENTITY, 0x16c) -= d;
        ei(ENTITY, 0x170) -= d;
        ei(ENTITY, 0x174) -= d;
        ew(ENTITY, 0x78) = (short)-ew(ENTITY, 0x78);
        char c = (char)ENTITY->blend_counter;
        ENTITY->blend_counter = (unsigned char)(c - 1);
        if (c == 0) scd_model_tint_apply(0, -1, 0, 0, 0x200, 8);
        BillboardAdjSize(&ENTITY->pushVelocity, (short)-16, (short)-16);
        if ((ENTITY->behavior_flags & 0x40) == 0 && ENTITY->blend_counter % 0x1e == 0) {
            VECTOR* dead = reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            g_playerPosScratch.x = dead->x;
            g_playerPosScratch.z = dead->z;
            g_playerPosScratch.pad = dead->pad;
            g_playerPosScratch.y = 3000;
            Effect_CreateBillboard(0, 0x1b, 0, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        }
    } else if (st == 3 || st == 4) {
        if (st == 3) {
            ew(ENTITY, 0x78) = (short)0xff80;
            ENTITY->action_state = 4;
        }
        int d = ew(ENTITY, 0x78) + 0x18;
        ei(ENTITY, 0x16c) += d;
        ei(ENTITY, 0x170) += d;
        ei(ENTITY, 0x174) += d;
        ew(ENTITY, 0x78) = (short)-ew(ENTITY, 0x78);
        ENTITY->blend_counter = (unsigned char)(ENTITY->blend_counter - 1);
        BillboardAdjSize(&ENTITY->pushVelocity, 0x10, 0x10);
        if ((ENTITY->behavior_flags & 0x40) == 0 && ENTITY->blend_counter % 0x1e == 0) {
            VECTOR* dead = reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
            g_playerPosScratch.x = dead->x;
            g_playerPosScratch.z = dead->z;
            g_playerPosScratch.pad = dead->pad;
            g_playerPosScratch.y = 3000;
            Effect_CreateBillboard(0, 0x1b, 0, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        }
    }

    if ((rand() & 0x1f) == 0) {
        VECTOR* dead = reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14);
        g_playerPosScratch.x = dead->x;
        g_playerPosScratch.z = dead->z;
        g_playerPosScratch.pad = dead->pad;
        g_playerPosScratch.y = 2000;
        g_playerPosScratch.x = (int)(rand() & 0x1ff);
        g_playerPosScratch.z = (int)(rand() & 0x1ff);
        Effect_CreateBillboard(0, 0x1c, 0, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
    }
}

// 0x0046a370 - flower body, state 2 (the core drops and bounces).
//
// action_state 3 is a deliberate DEAD END: plant42_init parks the room-40C0
// cutscene core there so the fall never runs.  Falling through into the
// action_state 1 body is what made the core sink to the floor on entry.
static void plant42_body_fall(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ew(ENTITY, 0x78) = 0;
        ENTITY->action_state = 1;
        ENTITY->blend_counter = 0x78;
    } else if (st != 1) {
        if (st != 2) return;
        if (ENTITY->blend_counter == 0) return;
        ei(ENTITY, 0x170) += ((rand() & 1) * -2 + 1) * (int)ew(ENTITY, 0x78);
        ei(ENTITY, 0x16c) += ((rand() & 1) * -2 + 1) * (int)ew(ENTITY, 0x78);
        ei(ENTITY, 0x174) += ((rand() & 1) * -2 + 1) * (int)ew(ENTITY, 0x78);
        ew(ENTITY, 0x78) = (short)-ew(ENTITY, 0x78);
        if (ew(ENTITY, 0x78) < 0) ew(ENTITY, 0x78) += 1;
        ENTITY->blend_counter = (unsigned char)(ENTITY->blend_counter - 1);
        return;
    }

    ei(ENTITY, 0x38) += (int)ew(ENTITY, 0x78) * 5;
    ew(ENTITY, 0x78) += 1;
    if (ew(ENTITY, 0x78) < 6 && (ew(ENTITY, 0x78) & 1) != 0)
        scd_model_tint_apply(-1, -1, 0, 0, 0x200, 8);

    if (199 < ei(ENTITY, 0x38)) {
        ew(ENTITY, 0x78) -= 1;
        ei(ENTITY, 0x38) += (int)ew(ENTITY, 0x78) * -5;
        ENTITY->action_state = 2;
        ENTITY->blend_counter = (unsigned char)(ENTITY->blend_counter - 1);
        ei(ENTITY, 0x170) -= 0x914;
        ei(ENTITY, 0x16c) += 0x9dc;
        ei(ENTITY, 0x174) += 0x9dc;
        if ((ENTITY->behavior_flags & 0x40) == 0) {
            g_playerPosScratch.pad = reinterpret_cast<VECTOR*>(static_cast<uintptr_t>(g_deadMoveValue) + 0x14)->pad;
            g_playerPosScratch.x = 600;  g_playerPosScratch.z = 600;  g_playerPosScratch.y = -200;
            Effect_CreateBillboard(9, 0x11, 0,      &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0x28);
            g_playerPosScratch.x = -600; g_playerPosScratch.z = 600;
            Effect_CreateBillboard(9, 0x11, 0x400,  &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0x28);
            g_playerPosScratch.x = -600; g_playerPosScratch.z = -600;
            Effect_CreateBillboard(9, 0x11, 0x200,  &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0x28);
            g_playerPosScratch.x = 600;  g_playerPosScratch.z = -600;
            Effect_CreateBillboard(9, 0x11, 0x800,  &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0x28);
            g_playerPosScratch.x = 0; g_playerPosScratch.y = -600; g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 0x1b, 0,      &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0x3c);
        }
        int soundPos = (int)&ENTITY->scaMatrixData.localMatrix.t[0];
        Play3DSnd(2, 8, 0, soundPos);
        Play3DSnd(2, 0, 0, soundPos);
        Play3DSnd(2, 0, 0, soundPos);
        Play3DSnd(2, 0, 0, soundPos);
        ew(ENTITY, 0x78) = 0x3c;
    }
}

// 0x0046a6b0 / 0x0046a6e0 / 0x0046a790 - root ball states 0, 1 and 2.
static void plant42_roots_idle(void)
{
    short t = ew(ENTITY, 0xc4);
    ew(ENTITY, 0xc4) = (short)(t - 1);
    if (t == 0) ew(ENTITY, 0xc4) = 0x59;
}

static void plant42_roots_pulse(void)
{
    unsigned char st = ENTITY->action_state;
    if (st == 0) { ew(ENTITY, 0x78) = 0x10; ENTITY->action_state = 1; st = 1; }
    if (st == 1) {
        ew(ENTITY, 0xca) -= (short)(ew(ENTITY, 0x78) + 0x30);
        ew(ENTITY, 0x78) = (short)-ew(ENTITY, 0x78);
        return;
    }
    if (st == 3) { ENTITY->action_state = 4; ew(ENTITY, 0x78) = (short)0xfff0; st = 4; }
    if (st == 4) {
        ew(ENTITY, 0xca) += (short)(ew(ENTITY, 0x78) + 0x30);
        ew(ENTITY, 0x78) = (short)-ew(ENTITY, 0x78);
    }
}

// action_state 2 is the frozen cutscene pose (room 40C0's flags==4 set-up).
// The port used to run the rise unconditionally, which is what dragged the
// root ball across the floor as soon as the room loaded.
static void plant42_roots_rise(void)
{
    if (ENTITY->action_state == 0) {
        ew(ENTITY, 0x78) = 0;
        ENTITY->action_state = 1;
    } else if (ENTITY->action_state != 1) {
        return;
    }

    ei(ENTITY, 0x38) += (int)ew(ENTITY, 0x78) * 4;
    ew(ENTITY, 0x78) += 1;
    if (-0x513 < ei(ENTITY, 0x38)) {
        ew(ENTITY, 0x78) -= 1;
        ei(ENTITY, 0x38) += (int)ew(ENTITY, 0x78) * -4;
        g_playerPosScratch.y = g_playerPosScratch.y / 2;
    }
    if (999 < euw(ENTITY, 0xca))
        euw(ENTITY, 0xca) -= 0x38;
}

// The two companions are carved out of the room data buffer, which a room
// transition rebuilds while the OLD enemy list is still being updated: the
// transition task keeps running the game loop, so plant42_update fires at least
// once with scd_target_ptr / Entity+0x178 pointing at memory the destination
// room has already overwritten. The original tolerated whatever it read there;
// here the garbage reached FUN_00483080 as an animation-object pointer and
// memcpy'd through it (observed: a write to 0x0000FFA6 when leaving room 40C0
// after the Rebecca hand-off).
//
// So: only follow a companion pointer that still lands inside one of the pools
// the clones can legitimately live in. This is a port-level lifetime guard, not
// something the original does.
static bool plant42_pool_pointer(const void* p, unsigned int bytes)
{
    uintptr_t ptr = (uintptr_t)p;
    uintptr_t end = ptr + bytes;
    if (ptr == 0 || end < ptr) return false;

    const struct { uintptr_t base; size_t size; } pools[] = {
        { (uintptr_t)&g_DataBuffer[0],         sizeof(g_DataBuffer)         },
        { (uintptr_t)&g_entityModelBuffer[0],  sizeof(g_entityModelBuffer)  },
        { (uintptr_t)&g_entityModelBuffer2[0], sizeof(g_entityModelBuffer2) },
    };
    for (const auto& pool : pools) {
        if (ptr >= pool.base && end <= pool.base + pool.size) return true;
    }
    return false;
}

// 0x00469d20 - tick and draw the two companions.
//
// The old port submitted only a fade sprite for each copy and skipped
// ScaleMatrixCols entirely, so the flower body (which lives ONLY in the scale
// factors at +0x16C/+0x170/+0x174) had no size at all and the root ball drew
// unscaled.  It also ran hand-written state code instead of the real tables.
static void plant42_render_copies(void)
{
    Entity* saved = ENTITY;
    Entity* body = plant42_body();
    Entity* roots = plant42_roots();
    if (g_RoomCameraDataCopy == 0) return;
    if (!plant42_pool_pointer(body, sizeof(Entity)) ||
        !plant42_pool_pointer(roots, sizeof(Entity))) return;
    // plant42_clone_entity stores the clone's own address in the field the
    // parent handed it BEFORE the struct copy, so a live body reads
    // body+0xB8 == body and a live root ball reads roots+0x178 == roots.
    // Reloaded buffer contents will not satisfy that.
    if (eu(body, 0xb8) != (unsigned int)(uintptr_t)body ||
        eu(roots, 0x178) != (unsigned int)(uintptr_t)roots) return;
    if (!plant42_pool_pointer((const void*)(uintptr_t)body->unk_18, 0xB4) ||
        !plant42_pool_pointer((const void*)(uintptr_t)roots->unk_18, 0xB4)) return;

    void* spriteSlot = (void*)((char*)&g_spriteAnimSlots[2] + (unsigned int)g_spriteAnimActive * 0x14);

    // ---- flower body ----
    ENTITY = body;
    update_entity_lighting(reinterpret_cast<VECTOR*>(&body->scaMatrixData.localMatrix.t[0]));
    RotMatrix(reinterpret_cast<SVECTOR*>(&body->position.pad), &body->scaMatrixData.localMatrix);
    if ((g_message_flags & 4) != 0) {
        switch (body->state) {
        case 0: plant42_body_idle();    break;
        case 1: plant42_body_shrivel(); break;
        case 2: plant42_body_fall();    break;
        default: break;                            // state 3 = NULL slot
        }
    }
    ScaleMatrixCols(&body->scaMatrixData.localMatrix, reinterpret_cast<VECTOR*>((char*)body + 0x16c));
    ApplyLVAndMul0Matrix(reinterpret_cast<void*>(static_cast<uintptr_t>(g_RoomCameraDataCopy)),
                         &body->scaMatrixData.localMatrix, &g_matrixScratch);

    MATRIX lightMatrix = (g_deadMoveValue != 0)
        ? *reinterpret_cast<MATRIX*>(static_cast<uintptr_t>(g_deadMoveValue))
        : g_identityMatrixData;
    lightMatrix.t[0] = body->scaMatrixData.localMatrix.t[0];
    lightMatrix.t[1] = body->scaMatrixData.localMatrix.t[1];
    lightMatrix.t[2] = body->scaMatrixData.localMatrix.t[2];
    if (g_lightMatrixPtr != 0)
        MulMatrix0(reinterpret_cast<MATRIX*>(static_cast<uintptr_t>(g_lightMatrixPtr)),
                   &lightMatrix, &lightMatrix);
    SetLightMatrix(&lightMatrix);
    SetRotAndTransMatrix(&g_matrixScratch);
    FUN_00483250(0, 0, 0, (int)body->unk_18, 0, 4, spriteSlot);
    entity_add_fade_sprite(reinterpret_cast<VECTOR*>(&body->scaMatrixData.localMatrix.t[0]),
                           (short*)&body->pushVelocity, 0, 0);

    // ---- root ball ----
    ENTITY = roots;
    RotMatrix(reinterpret_cast<SVECTOR*>(&roots->position.pad), &roots->scaMatrixData.localMatrix);
    int rootScale = (int)plant42_pulse(ew(roots, 0xc4)) + (int)euw(roots, 0xca);
    g_playerPosScratch.x = rootScale;
    g_playerPosScratch.y = rootScale;
    g_playerPosScratch.z = rootScale;
    ScaleMatrixCols(&roots->scaMatrixData.localMatrix, &g_playerPosScratch);
    if ((g_message_flags & 4) != 0) {
        switch (roots->state) {
        case 0: plant42_roots_idle();  break;
        case 1: plant42_roots_pulse(); break;
        case 2: plant42_roots_rise();  break;
        default: break;
        }
    }
    ApplyLVAndMul0Matrix(reinterpret_cast<void*>(static_cast<uintptr_t>(g_RoomCameraDataCopy)),
                         &roots->scaMatrixData.localMatrix, &g_matrixScratch);
    SetRotAndTransMatrix(&g_matrixScratch);
    FUN_00483250(0, 0, 0, (int)roots->unk_18, 0, 4, spriteSlot);

    ENTITY = saved;
}

// ---------------------------------------------------------------------------
// 0x0048a630 - clone the current entity `count` times into the room data
// buffer and give each clone its own animation object.
// ---------------------------------------------------------------------------
static void plant42_clone_entity(unsigned char count, int animSlotBytes,
                                 unsigned char jointIndex, unsigned int* out)
{
    (void)animSlotBytes;   // the original computes the slot from jointIndex only
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

} // namespace

// ===========================================================================
// 0x004648d0 - state 0 (one-time setup)
// ===========================================================================
void plant42_init(void)
{
    set_state_word(1);
    g_deathAnimationFlag = 0;
    ENTITY->has_enter_switch_zone = 1;
    ENTITY->scaMatrixData.field_00 = 0;
    ENTITY->hit_state = 0;
    ResetJointTransforms();
    g_svecScratch = {0, 0, 0, 0};
    g_animFrameIdSave = 0x404040;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 2000, 300);
    rand();
    ENTITY->health = 0x28;
    if ((ENTITY->behavior_flags & 1) == 0) ENTITY->health += 100;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->animationId = 2;
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
    ENTITY->Sca_info = (unsigned int)(uintptr_t)s_plant42ScaInfo[0];
    p42_step() = 0;
    eub(ENTITY, 0x176) = 0;
    eub(ENTITY, 0x177) = 0;
    p42_life() = (short)((rand() & 0xff) + 0x32);

    if ((ENTITY->behavior_flags & 1) == 0) {
        ENTITY->hit_state = 1;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)s_plant42ScaInfo[2];
        if (ENTITY->pSca_hit_data != 0) {
            *(short*)(ENTITY->pSca_hit_data + 6) = 0;
            *(short*)(ENTITY->pSca_hit_data + 8) = 0;
            *(short*)(ENTITY->pSca_hit_data + 10) = 0;
        }

        // ---- flower body ----
        plant42_clone_entity(1, 900, (unsigned char)(ENTITY->jointCount + 1), &ENTITY->scd_target_ptr);
        Entity* body = plant42_body();
        body->behavior_flags = 0;
        body->hit_state = 0;
        euw(body, 0xca) = 0x1000;
        ew(body, 0x7a) = 0;
        ew(body, 0x7c) = 1;
        ew(body, 0x6c) = 0;
        ew(body, 0x6e) = 1;
        ew(body, 0x70) = 4;              // vines left before the plant dies
        ew(body, 0x7e) = 0;
        ew(body, 0x72) = 0;
        ew(body, 0x74) = 0;
        ew(body, 0x76) = 0;
        ew(body, 0xc4) = 0x59;
        ew(body, 0x78) = 0;
        body->blend_counter = 7;
        ew(body, 0x8e) = 0;
        ei(body, 0x16c) = 0x1000;
        ei(body, 0x170) = 0x1000;
        ei(body, 0x174) = 0x1000;
        if ((ENTITY->behavior_flags & 0xf) == 8) {
            scd_model_tint_apply(-1, -2, 0, 0, 0x200, 8);
            ew(body, 0x70) = 0;
        }
        eu(body, 0x84) = 0;
        if (ENTITY->behavior_flags == 4) {
            // Room 40C0's fallen-core pose: the body is DEAD and parked in
            // state 2 / action_state 3, which is a no-op branch of
            // plant42_body_fall.  It must never animate.
            body->health = -1;
            body->state = 2;
            body->action_state = 3;
            euw(body, 0x8e) = 0x0a20;
            ei(body, 0x16c) = 0x19dc;
            ei(body, 0x170) = 0x0818;
            ei(body, 0x174) = 0x19dc;
            body->blend_counter = 1;
        }
        g_svecScratch = {0, 0, 0, 0};
        g_animFrameIdSave = 0x606060;
        FUN_004565f0(&g_svecScratch, (SVECTOR*)&body->pushVelocity, 3000, 3000);

        // Every vine shares one body: the hit counter at body+0x70 is the
        // plant's real health pool.
        for (int i = 0; i < g_enemy_count; ++i)
            g_EnemiesList[i].scd_target_ptr = (unsigned int)(uintptr_t)body;
        g_playerDisplacement = 0;

        // ---- root ball ----
        // DAT_004d2bd8 selects ComplexTmdObjectSetup inside CreateAnimObject.
        // Without it the roots are built through the ordinary async path and
        // render as flickering garbage.
        DAT_004d2bd8 = 1;
        plant42_clone_entity(1, 0xdc, ENTITY->jointCount,
                             reinterpret_cast<unsigned int*>((char*)ENTITY + 0x178));
        DAT_004d2bd8 = 0;
        Entity* roots = plant42_roots();
        ew(roots, 0x72) = 0;
        ew(roots, 0x74) = 0;
        ew(roots, 0x76) = 0;
        ew(roots, 0xc4) = 0x59;
        roots->behavior_flags = 0;
        roots->hit_state = 0;
        ew(roots, 0x7a) = 0;
        ew(roots, 0x7c) = 1;
        ew(roots, 0x6c) = 0;
        ew(roots, 0x6e) = 1;
        euw(roots, 0xca) = 0x1000;
        ew(roots, 0x78) = 0;
        if (ENTITY->behavior_flags == 4) {
            euw(roots, 0xca) = 1000;
            roots->scaMatrixData.localMatrix.t[1] = -1300;
            roots->state = 2;
            roots->action_state = 2;     // frozen: plant42_roots_rise returns
        }
    }

    if ((ENTITY->behavior_flags & 0x40) != 0) ENTITY->state = 8;
    if (ENTITY->behavior_flags == 4) {
        ENTITY->state = 4;
        scd_model_tint_apply(-4, -5, 0, 0, 0x200, 8);
    }
    if ((ENTITY->behavior_flags & 0xf) == 6) {
        // Room 40C0, Chris held in the vine. State 8 + SCD behaviour 0x0B.
        set_state_word(0x000b0108);
        plant42_behavior_suspend();
    }
    if (ENTITY->behavior_flags == 5) euw(ENTITY, 0xca) = 0x12c0;
}

// ===========================================================================
// 0x00464d10 - per-frame entry
// ===========================================================================
void plant42_update(void)
{
    if ((g_message_flags & 4) != 0) {
        if ((ENTITY->behavior_flags & 0x40) != 0 &&
            (unsigned char)(ENTITY->state - 1) < 2) {
            // Read BEFORE the store: the DWORD write clobbers 0x86/0x87.
            const bool withering = (euw(ENTITY, 0x86) == 0x0309);
            set_state_word(0x030f0008);
            if (!withering) set_state_word(8);
        }

        switch (ENTITY->state) {
        case 0: plant42_init();        break;
        case 1: plant42_state_check(); break;
        case 2: plant42_damaged();     break;
        case 3: plant42_die();         break;
        case 4:                        break;   // 0x00465740 - empty
        case 8: plant42_scd_state();   break;
        default: break;
        }

        // Keep the SCA hit box glued to the head joint. Missing entirely from
        // the old port, so the plant's hurt box never left the origin.
        if (ENTITY->jointsStructs != NULL && ENTITY->pSca_hit_data != 0) {
            short* hit = (short*)ENTITY->pSca_hit_data;
            hit[0] = (short)((short)jwt(14)[0] - (short)ENTITY->scaMatrixData.localMatrix.t[0]);
            hit[1] = (short)((short)jwt(14)[1] - (short)ENTITY->scaMatrixData.localMatrix.t[1]);
            hit[2] = (short)((short)jwt(14)[2] - (short)ENTITY->scaMatrixData.localMatrix.t[2]);
        }
    }

    if ((ENTITY->behavior_flags & 1) == 0)
        plant42_render_copies();

    if (ENTITY->behavior_flags == 4) {
        ENTITY->has_enter_switch_zone = 0;
        return;
    }

    if (ENTITY->jointsStructs == NULL) return;

    if ((ENTITY->hit_state & 0x80) == 0) {
        int y = jwt(11)[1];
        BillboardSetSize(&ENTITY->pushVelocity, (short)(y / 7 + 2000), (short)(y / 0x46 + 300));
    }
    entity_add_fade_sprite(reinterpret_cast<VECTOR*>(jwt(11)),
                           (short*)&ENTITY->pushVelocity, 0, ENTITY->angle);
}
