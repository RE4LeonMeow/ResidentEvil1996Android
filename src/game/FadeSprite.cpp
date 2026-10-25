// FadeSprite.cpp - Ground-shadow / fade-sprite queue and renderer
//
// Entities queue a translucent ground shadow each frame via
// entity_add_fade_sprite (0x00456810); game_loop then drains the queue through
// DrawFadeSpr (0x00456d30). Both were stubbed, so no shadow reached the renderer
// even once the quad builder (FUN_004565f0) was in place.
//
// The queue record is 144 bytes:
//   +0x00  world position (x, y, z, pad) as ints
//   +0x10  120-byte copy of the entity's shadow quad from entity+0xE4
//          (quad[0] world offset, quad[1..5] header/CLUT/tpage/UVs,
//           quad[0xB..0xE] the four corners - see FUN_004565f0)
//   +0x88  short  y offset
//   +0x8A  short  angle
//   +0x8C  short  sort key
//
// Within that copy the corners sit at short offsets 0x2C/0x30/0x34/0x38, and the
// projected screen coordinates are written back into the quad's vertex slots
// offset by g_spriteAnimActive * 0x14 shorts - the double-buffered half the
// original alternates between.
// ============================================================================
#include "../Globals.h"
#include "SpriteRenderer.h"
#include <cstring>

extern void SetRotAndTransMatrix(MATRIX* m);
extern int  RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                        int* sxy0, int* sxy1, int* sxy2, int* sxy3,
                        int* p, int* flag);

// ============================================================================
// The fade-sprite queue (0x00bca0e0). 32 records of 144 bytes; g_FadeSprCount is
// the fill level, which DrawFadeSpr resets to 0 after draining.
// ============================================================================
#define FADE_SPR_MAX      32
#define FADE_SPR_RECSIZE  0x90

unsigned char g_FadeSpr[FADE_SPR_MAX * FADE_SPR_RECSIZE] = {};   // 0x00bca0e0
int           g_FadeSprCount = 0;

#define FS_REC(i)    (g_FadeSpr + (i) * FADE_SPR_RECSIZE)
#define FS_POS(i)    ((int*)(FS_REC(i)))
#define FS_QUAD(i)   ((short*)(FS_REC(i) + 0x10))
#define FS_YOFS(i)   (*(short*)(FS_REC(i) + 0x88))
#define FS_ANGLE(i)  (*(short*)(FS_REC(i) + 0x8A))
#define FS_SORT(i)   (*(unsigned short*)(FS_REC(i) + 0x8C))

// Placement constants read alongside the tables below.
//   0x004c0d20 = 2  per-sprite vertical step, so coplanar shadows do not fight
//   0x004c0624 = 0  added into the alpha/sort base
//   0x004c0620 = 0  added into the screen X offset
static const int g_FadeSprLayerStep = 2;    // 0x004c0d20
static const int g_FadeSprAlphaBias = 0;    // 0x004c0624
static const int g_FadeSprOffsetX   = 0;    // 0x004c0620

// Near-plane distance (view z) the shadow quad is clipped against before
// projection. The original's hardware clip did this for the Marni quad;
// the port's sprite approximation needs it so behind-camera corners don't
// divide by a negative z (which wraps the screen coords and the depth).
static const int FADE_NEAR = 128;

struct FadeSprParam {
    unsigned char flag;      // +0x00  selects which alpha branch DrawFadeSpr takes
    int           sortBias;  // +0x04
    int           offsetY;   // +0x08  view-space Y offset, NOT a screen X shift
    int           unk_0c;    // +0x0C  16 in every record; unread on this path
    int           forceFlag; // +0x10  0 in every record; unread on this path
};

// ============================================================================
// Fade-sprite (ground shadow) placement tables
//
// g_FadeSprParamIndex (0x004c0630): one byte per (stage, room, camera), laid
// out as [(room + stage * 0x20) * 8 + camera]. Selects a record in
// g_FadeSprParams. Stages above 4 are folded down by 5 before indexing, which
// is why only 5 stage blocks exist.
// ============================================================================
static const unsigned char g_FadeSprParamIndex[5 * 0x20 * 8] = {
    // stage 0
    0, 0, 0, 0, 0, 0, 0, 0,    // room  0
    0, 0, 0, 0, 0, 0, 0, 0,    // room  1
    0, 0, 0, 0, 0, 0, 0, 0,    // room  2
    0, 0, 0, 0, 0, 0, 0, 0,    // room  3
    4, 4, 4, 4, 4, 4, 4, 4,    // room  4
    3, 3, 3, 3, 3, 3, 3, 3,    // room  5
    0, 0, 0, 0, 0, 0, 0, 0,    // room  6
    0, 0, 0, 0, 0, 0, 0, 0,    // room  7
    0, 0, 0, 0, 0, 0, 0, 0,    // room  8
    0, 0, 0, 0, 0, 0, 0, 0,    // room  9
    5, 5, 5, 5, 5, 5, 5, 5,    // room 10
    0, 0, 0, 0, 0, 0, 0, 0,    // room 11
    0, 0, 0, 0, 0, 0, 0, 0,    // room 12
    0, 0, 0, 0, 0, 0, 0, 0,    // room 13
    0, 0, 0, 0, 0, 0, 0, 0,    // room 14
    0, 0, 0, 0, 0, 0, 0, 0,    // room 15
    0, 0, 0, 0, 0, 0, 0, 0,    // room 16
    0, 0, 0, 0, 0, 0, 0, 0,    // room 17
    0, 0, 0, 0, 0, 0, 0, 0,    // room 18
    3, 3, 3, 3, 3, 3, 3, 3,    // room 19
    0, 0, 0, 0, 0, 0, 0, 0,    // room 20
    0, 0, 0, 0, 0, 0, 0, 0,    // room 21
    0, 0, 0, 0, 0, 0, 0, 0,    // room 22
    0, 0, 0, 0, 0, 9, 0, 0,    // room 23
    0, 0, 0, 0, 0, 0, 0, 0,    // room 24
    0, 0, 0, 0, 0, 0, 0, 0,    // room 25
    0, 0, 0, 0, 0, 0, 0, 0,    // room 26
    0, 0, 0, 0, 0, 0, 0, 0,    // room 27
    0, 0, 0, 0, 0, 0, 0, 0,    // room 28
    0, 0, 0, 0, 0, 0, 0, 0,    // room 29
    0, 0, 0, 0, 0, 0, 0, 0,    // room 30
    0, 0, 0, 0, 0, 0, 0, 0,    // room 31
    // stage 1
    0, 0, 0, 0, 0, 0, 0, 0,    // room  0
    0, 0, 0, 0, 0, 0, 0, 0,    // room  1
    0, 0, 0, 0, 0, 0, 0, 0,    // room  2
    0, 0, 0, 0, 0, 0, 0, 0,    // room  3
    0, 0, 0, 0, 0, 0, 0, 0,    // room  4
    0, 0, 0, 0, 0, 0, 0, 0,    // room  5
    0, 0, 0, 0, 0, 0, 0, 0,    // room  6
    0, 0, 0, 0, 0, 0, 0, 0,    // room  7
    0, 0, 0, 0, 0, 0, 0, 0,    // room  8
    0, 0, 0, 0, 0, 0, 0, 0,    // room  9
    0, 0, 0, 0, 0, 0, 0, 0,    // room 10
    0, 0, 0, 0, 0, 0, 0, 0,    // room 11
    0, 0, 0, 0, 0, 0, 0, 0,    // room 12
    0, 0, 0, 0, 0, 0, 0, 0,    // room 13
    0, 0, 0, 0, 0, 0, 0, 0,    // room 14
    0, 0, 0, 0, 0, 0, 0, 0,    // room 15
    0, 0, 0, 0, 0, 0, 0, 0,    // room 16
    0, 0, 0, 0, 0, 0, 0, 0,    // room 17
    0, 0, 0, 0, 0, 0, 0, 0,    // room 18
    0, 0, 0, 0, 0, 0, 0, 0,    // room 19
    0, 0, 0, 0, 0, 0, 0, 0,    // room 20
    0, 0, 0, 0, 0, 0, 0, 0,    // room 21
    0, 0, 0, 0, 0, 0, 0, 0,    // room 22
    1, 1, 1, 1, 12, 1, 1, 1,    // room 23
    0, 0, 0, 0, 0, 0, 0, 0,    // room 24
    0, 0, 0, 0, 0, 0, 0, 0,    // room 25
    0, 0, 0, 0, 0, 0, 0, 0,    // room 26
    0, 0, 0, 0, 0, 0, 0, 0,    // room 27
    0, 0, 0, 0, 0, 0, 0, 0,    // room 28
    0, 0, 0, 0, 0, 0, 0, 0,    // room 29
    0, 0, 0, 0, 0, 0, 0, 0,    // room 30
    0, 0, 0, 0, 0, 0, 0, 0,    // room 31
    // stage 2
    1, 0, 12, 0, 0, 0, 0, 0,    // room  0
    6, 6, 6, 6, 6, 6, 6, 14,    // room  1
    19, 6, 6, 6, 6, 6, 6, 6,    // room  2
    0, 0, 0, 0, 0, 0, 0, 0,    // room  3
    0, 0, 0, 0, 0, 0, 0, 0,    // room  4
    10, 0, 0, 0, 0, 0, 0, 0,    // room  5
    0, 0, 0, 0, 0, 0, 0, 0,    // room  6
    0, 0, 0, 0, 16, 0, 0, 0,    // room  7
    0, 0, 0, 0, 0, 0, 0, 0,    // room  8
    0, 0, 0, 0, 0, 0, 0, 0,    // room  9
    0, 0, 0, 0, 0, 0, 0, 0,    // room 10
    0, 0, 0, 0, 0, 0, 0, 0,    // room 11
    0, 0, 0, 0, 0, 0, 0, 0,    // room 12
    0, 0, 0, 0, 0, 0, 0, 0,    // room 13
    0, 0, 0, 0, 0, 0, 0, 0,    // room 14
    0, 18, 0, 0, 0, 14, 14, 0,    // room 15
    0, 0, 0, 0, 0, 0, 0, 0,    // room 16
    0, 0, 0, 0, 0, 0, 0, 0,    // room 17
    0, 0, 0, 0, 0, 0, 0, 0,    // room 18
    0, 0, 0, 0, 0, 0, 0, 0,    // room 19
    0, 0, 0, 0, 0, 0, 0, 0,    // room 20
    0, 0, 0, 0, 0, 0, 0, 0,    // room 21
    0, 0, 0, 0, 0, 0, 0, 0,    // room 22
    0, 0, 0, 0, 0, 0, 0, 0,    // room 23
    0, 0, 0, 0, 0, 0, 0, 0,    // room 24
    0, 0, 0, 0, 0, 0, 0, 0,    // room 25
    0, 0, 0, 0, 0, 0, 0, 0,    // room 26
    0, 0, 0, 0, 0, 0, 0, 0,    // room 27
    0, 0, 0, 0, 0, 0, 0, 0,    // room 28
    0, 0, 0, 0, 0, 0, 0, 0,    // room 29
    0, 0, 0, 0, 0, 0, 0, 0,    // room 30
    0, 0, 0, 0, 0, 0, 0, 0,    // room 31
    // stage 3
    0, 0, 0, 0, 0, 0, 0, 0,    // room  0
    17, 17, 17, 17, 17, 17, 17, 17,    // room  1
    0, 0, 0, 0, 0, 0, 0, 0,    // room  2
    0, 0, 0, 0, 0, 0, 0, 0,    // room  3
    0, 0, 0, 0, 0, 0, 0, 0,    // room  4
    0, 0, 0, 0, 0, 0, 0, 0,    // room  5
    7, 7, 7, 7, 7, 7, 7, 7,    // room  6
    0, 0, 0, 0, 0, 0, 0, 0,    // room  7
    0, 0, 0, 0, 0, 0, 0, 0,    // room  8
    0, 0, 0, 0, 0, 0, 0, 0,    // room  9
    0, 0, 0, 0, 0, 0, 0, 0,    // room 10
    0, 0, 0, 0, 0, 0, 0, 0,    // room 11
    0, 0, 0, 0, 0, 0, 0, 0,    // room 12
    0, 0, 0, 0, 0, 11, 0, 0,    // room 13
    8, 8, 8, 8, 8, 8, 8, 8,    // room 14
    0, 0, 0, 0, 0, 0, 0, 0,    // room 15
    0, 0, 0, 0, 0, 0, 0, 0,    // room 16
    0, 0, 0, 0, 0, 0, 0, 0,    // room 17
    0, 0, 0, 0, 0, 0, 0, 0,    // room 18
    0, 0, 0, 0, 0, 0, 0, 0,    // room 19
    0, 0, 0, 0, 0, 0, 0, 0,    // room 20
    0, 0, 0, 0, 0, 0, 0, 0,    // room 21
    0, 0, 0, 0, 0, 0, 0, 0,    // room 22
    0, 0, 0, 0, 0, 0, 0, 0,    // room 23
    0, 0, 0, 0, 0, 0, 0, 0,    // room 24
    0, 0, 0, 0, 0, 0, 0, 0,    // room 25
    0, 0, 0, 0, 0, 0, 0, 0,    // room 26
    0, 0, 0, 0, 0, 0, 0, 0,    // room 27
    0, 0, 0, 0, 0, 0, 0, 0,    // room 28
    0, 0, 0, 0, 0, 0, 0, 0,    // room 29
    0, 0, 0, 0, 0, 0, 0, 0,    // room 30
    0, 0, 0, 0, 0, 0, 0, 0,    // room 31
    // stage 4
    0, 0, 0, 0, 0, 0, 0, 0,    // room  0
    0, 0, 0, 0, 0, 0, 0, 0,    // room  1
    0, 0, 0, 0, 0, 0, 0, 0,    // room  2
    0, 0, 0, 0, 0, 0, 0, 0,    // room  3
    0, 0, 0, 0, 0, 0, 0, 0,    // room  4
    0, 0, 0, 0, 0, 0, 0, 0,    // room  5
    0, 0, 0, 0, 0, 0, 0, 0,    // room  6
    2, 2, 2, 2, 2, 13, 2, 2,    // room  7
    0, 0, 0, 0, 0, 0, 0, 0,    // room  8
    0, 0, 0, 0, 0, 0, 0, 0,    // room  9
    0, 0, 0, 0, 0, 0, 0, 0,    // room 10
    0, 0, 0, 0, 0, 0, 0, 0,    // room 11
    0, 0, 0, 0, 0, 0, 0, 0,    // room 12
    0, 0, 0, 0, 0, 0, 0, 0,    // room 13
    0, 0, 0, 0, 0, 0, 0, 0,    // room 14
    0, 0, 0, 0, 0, 0, 0, 0,    // room 15
    0, 0, 0, 0, 0, 0, 0, 0,    // room 16
    0, 0, 0, 0, 0, 0, 0, 0,    // room 17
    0, 0, 0, 0, 0, 0, 0, 0,    // room 18
    0, 0, 0, 0, 0, 0, 0, 0,    // room 19
    0, 0, 0, 0, 0, 0, 0, 0,    // room 20
    0, 0, 0, 0, 0, 0, 0, 0,    // room 21
    0, 0, 0, 0, 0, 0, 0, 0,    // room 22
    0, 0, 0, 0, 0, 0, 0, 0,    // room 23
    0, 0, 0, 0, 0, 0, 0, 0,    // room 24
    0, 0, 0, 0, 0, 0, 0, 0,    // room 25
    0, 0, 0, 0, 0, 0, 0, 0,    // room 26
    0, 0, 0, 0, 0, 0, 0, 0,    // room 27
    0, 0, 0, 0, 0, 0, 0, 0,    // room 28
    0, 0, 0, 0, 0, 0, 0, 0,    // room 29
    0, 0, 0, 0, 0, 0, 0, 0,    // room 30
    0, 0, 0, 0, 0, 0, 0, 0,    // room 31
};

// ============================================================================
// g_FadeSprParams (0x004c0b30): 20 records of 0x14 bytes. DrawFadeSpr reads
// the flag at +0, the sort bias at +4 and the view-space Y offset at +8;
// entity_add_fade_sprite reads the +4 bias too. +0xC is 16 in every record and
// +0x10 is 0 in every record - neither is read by any call site reached from
// the shadow path, so a non-zero +0x10 (which would force the alpha) never
// occurs in practice.
// ============================================================================
static const FadeSprParam g_FadeSprParams[20] = {
    { 0,      0,      0,  16, 0 },   //  0
    { 1,      0,      0,  16, 0 },   //  1
    { 0,   -100,      0,  16, 0 },   //  2
    { 0,   1500,      0,  16, 0 },   //  3
    { 0,   -300,      0,  16, 0 },   //  4
    { 0,      0,   -200,  16, 0 },   //  5
    { 0,     10,    -40,  16, 0 },   //  6
    { 1,     50,      0,  16, 0 },   //  7
    { 1,      0,      0,  16, 0 },   //  8
    { 0,  16000,      0,  16, 0 },   //  9
    { 0,     -2,    100,  16, 0 },   // 10
    { 0,  15000,    120,  16, 0 },   // 11
    { 0,  15000,      0,  16, 0 },   // 12
    { 0,  11500,      0,  16, 0 },   // 13
    { 0,  16600,      0,  16, 0 },   // 14
    { 0,   5000,      0,  16, 0 },   // 15
    { 0,  13800,      0,  16, 0 },   // 16
    { 1,    100,      0,  16, 0 },   // 17
    { 0,   -102,    100,  16, 0 },   // 18
    { 0,   -110,    -40,  16, 0 },   // 19
};

// ============================================================================
// fade_spr_param_index - the (stage, room, camera) lookup, with the original's
// stage folding: anything above stage 4 has 5 subtracted first.
//
// The bounds checks are port-only. The original indexes straight in; a stage or
// room past the table would read adjacent data rather than fault, which is not
// worth reproducing.
// ============================================================================
static unsigned int fade_spr_param_index(void)
{
    unsigned int stage = get_stage_id();
    if (stage > 4) {
        stage -= 5;
    }
    unsigned int i = ((unsigned int)g_roomId + stage * 0x20) * 8 + (unsigned int)g_roomCameraId;
    if (i >= sizeof(g_FadeSprParamIndex)) {
        return 0;
    }
    unsigned int rec = g_FadeSprParamIndex[i];
    return (rec < 20) ? rec : 0;
}

// ============================================================================
// entity_add_fade_sprite (0x00456810)
// Builds the shadow's world matrix, projects its four corners for a depth, then
// copies the whole quad into the queue with a sort key.
// ============================================================================
void entity_add_fade_sprite(VECTOR* pos, short* quad, short yOffset, short angle)
{
    MATRIX local, composed;
    int discard0 = 0, discard1 = 0;

    local = g_identityMatrixData;
    local.t[1] = (int)yOffset;
    local.t[0] = (int)quad[0] + pos->x;
    local.t[2] = (int)quad[2] + pos->z;

    RotMatrixY((int)angle, &local);
    ApplyLVAndMul0Matrix(&g_RoomCameraData, &local, &composed);
    SetRotAndTransMatrix(&composed);
    SetGlobalScaledRotationMatrix(&composed);
    GetMatrixTranslation(&composed);

    unsigned int slot = (unsigned int)g_spriteAnimActive;
    unsigned int depth = (unsigned int)RotAverage4(
        (SVECTOR*)(quad + 0x2C), (SVECTOR*)(quad + 0x30),
        (SVECTOR*)(quad + 0x34), (SVECTOR*)(quad + 0x38),
        (int*)(quad + slot * 0x14 + 8),    (int*)(quad + slot * 0x14 + 0xC),
        (int*)(quad + slot * 0x14 + 0x10), (int*)(quad + slot * 0x14 + 0x14),
        &discard0, &discard1);

    const FadeSprParam* prm = &g_FadeSprParams[fade_spr_param_index()];
    short sortBase = (short)(prm->sortBias + 100);

    // The original duplicates the store block across the two branches of
    // g_main_state_flags2 bit 3; the only difference is that the depth clamp is
    // skipped when the bit is set. Folded together here.
    if ((g_main_state_flags2 & MSF2_FADE_NO_DEPTH_CLAMP) == 0) {
        if (depth > 0xFEF) {
            depth = 0xFF0;
        }
    }

    if (g_FadeSprCount >= FADE_SPR_MAX) {
        return;
    }

    int i = g_FadeSprCount;
    FS_POS(i)[0] = pos->x;
    FS_POS(i)[1] = pos->y;
    FS_POS(i)[2] = pos->z;
    FS_POS(i)[3] = pos->pad;
    memcpy(FS_QUAD(i), quad, 0x1E * 4);   // 30 dwords = 120 bytes
    g_FadeSprCount++;
    FS_YOFS(i)  = yOffset;
    FS_ANGLE(i) = angle;
    FS_SORT(i)  = (unsigned short)((short)depth + sortBase);
}

// ============================================================================
// DrawFadeSpr (0x00456d30)
// Sorts the queued shadows by sort key, then re-projects and submits each one.
// Clears the queue on the way out.
//
// The sort keeps the original's shape - a selection pass swapping whole 144-byte
// records, not an index sort.
// ============================================================================
void DrawFadeSpr(void)
{
    unsigned char tmp[FADE_SPR_RECSIZE];

    for (int a = 0; a < g_FadeSprCount - 1; a++) {
        for (int b = a + 1; b < g_FadeSprCount; b++) {
            if (FS_SORT(b) < FS_SORT(a)) {
                memcpy(tmp,       FS_REC(a), FADE_SPR_RECSIZE);
                memcpy(FS_REC(a), FS_REC(b), FADE_SPR_RECSIZE);
                memcpy(FS_REC(b), tmp,       FADE_SPR_RECSIZE);
            }
        }
    }

    for (int i = 0; i < g_FadeSprCount; i++) {
        MATRIX local, composed;

        short* quad = FS_QUAD(i);

        local = g_identityMatrixData;
        local.t[0] = (int)quad[0] + FS_POS(i)[0];
        local.t[2] = (int)quad[2] + FS_POS(i)[2];
        local.t[1] = (int)FS_YOFS(i) + g_FadeSprLayerStep * i;

        RotMatrixY((int)FS_ANGLE(i), &local);
        ApplyLVAndMul0Matrix(&g_RoomCameraData, &local, &composed);
        SetRotAndTransMatrix(&composed);
        SetGlobalScaledRotationMatrix(&composed);
        GetMatrixTranslation(&composed);

        unsigned int slot = (unsigned int)g_spriteAnimActive;

        // The per-room placement record. Its +8 field is a VIEW-SPACE Y
        // offset, not a screen X shift: the original writes the primitive's
        // translation row as (t[0], t[1] + prm[8] + DAT_004c0620, t[2])
        // (0x0046ffc0), so it lifts or drops the shadow relative to the
        // entity - which is what the -200 / +100 / +120 values in the table
        // are for.
        const FadeSprParam* prm = &g_FadeSprParams[fade_spr_param_index()];
        int offY = prm->offsetY + g_FadeSprOffsetX;

        // Transform the four world-space corners to view space, clip the quad
        // against the near plane, and project the clipped polygon. The
        // original handed the composed matrix to a Marni viewport quad and
        // let the hardware clip it; RotAverage4's raw outputs are poisoned by
        // behind-camera corners (dividing by a negative z wraps the screen
        // coordinates and the mean depth), which stretched the sprite into a
        // screen-sized square anchored at one correct corner and forced the
        // alpha to fully opaque. With a close camera the clipped corners land
        // far off-screen and the sprite spills over the screen edge - the
        // same spill the original's clipped quad produced.
        //
        // Footprint: the original passes AddFadePoly a SCALE triple built
        // from this record's own corner 1 -
        //   u = quad[0x30] * 0.0025, v = 1.0f, clut = quad[0x32] * 0.0025
        // (0x00456ec8; 0.0025 == 1/400 at 0x004af290) - and AddFadePoly
        // stores it as the primitive's (sx, sy, sz), the same slot the fire
        // effect fills with (2.0, 2.0, 1.0). The quad it scales is the fixed
        // rectConfig registered by CreateTexturedQuad(0, 0x2F, ...):
        // (-400,0,+400)(+400,0,+400)(-400,0,-400)(+400,0,-400). So the drawn
        // footprint is exactly ±halfW × ±halfH in the entity's own frame -
        // 500 × 700 for the player (FUN_004565f0(&scratch, quad, 500, 700)
        // at 0x00494ef7), 400 × 400 for a zombie. The hardcoded 400/200 pair
        // this used to carry made the shadow a third of its depth.
        int halfW = (int)quad[0x30];
        int halfH = (int)quad[0x32];
        if (halfW <= 0 || halfH <= 0) {
            continue;   // corner 1 is (+halfW, y, +halfH); a degenerate quad
        }               // would divide by zero in the UV below
        int cvx[4], cvy[4], cvz[4];
        int cu[4], cv2[4];
        for (int c = 0; c < 4; c++) {
            // The viewport quad's corners, in the same (-w,+h)..(+w,-h)
            // arrangement as the entity corners below.
            int cx = (c == 0 || c == 2) ? -halfW : halfW;
            int cz = (c == 0 || c == 1) ?  halfH : -halfH;
            int rx = (int)composed.m[0][0] * cx + (int)composed.m[0][2] * cz;
            int ry = (int)composed.m[1][0] * cx + (int)composed.m[1][2] * cz;
            int rz = (int)composed.m[2][0] * cx + (int)composed.m[2][2] * cz;
            cvx[c] = ((rx + (rx >> 31 & 0xFFF)) >> 12) + (int)composed.t[0];
            cvy[c] = ((ry + (ry >> 31 & 0xFFF)) >> 12) + (int)composed.t[1];
            cvz[c] = ((rz + (rz >> 31 & 0xFFF)) >> 12) + (int)composed.t[2];
            // Texture UV over the whole kage page in 0..4096 fixed point:
            // u = (x + w) / 2w, v = (h - z) / 2h - v=0 at +z, the same
            // corner orientation as the original viewport quad's UVs.
            cu[c]  = ((cx + halfW) << 12) / (2 * halfW);
            cv2[c] = ((halfH - cz) << 12) / (2 * halfH);
        }
        // Quad edge order 0->1->3->2: the corners are laid out
        // (-w,+h) (+w,+h) (-w,-h) (+w,-h), the same arrangement as the
        // viewport quad whose index list is 0,1,3,2 - NOT the bowtie
        // 0->1->2->3, whose diagonals would clip at the wrong places.
        static const int edge[4] = { 0, 1, 3, 2 };
        int n = 0;
        int vx[8], vy[8], vz[8];
        int vu[8], vv[8];
        for (int e = 0; e < 4; e++) {
            int a = edge[e], b = edge[(e + 1) & 3];
            int za = cvz[a], zb = cvz[b];
            int ain = (za >= FADE_NEAR) ? 1 : 0;
            int bin = (zb >= FADE_NEAR) ? 1 : 0;
            if (ain) { vx[n] = cvx[a]; vy[n] = cvy[a]; vz[n] = za;
                       vu[n] = cu[a]; vv[n] = cv2[a]; n++; }
            if (ain != bin) {
                int t = ((FADE_NEAR - za) << 12) / (zb - za);   // 4.12 lerp
                vx[n] = cvx[a] + (((cvx[b] - cvx[a]) * t) >> 12);
                vy[n] = cvy[a] + (((cvy[b] - cvy[a]) * t) >> 12);
                vz[n] = FADE_NEAR;
                // UVs lerp along the edge with the same t, so the texture
                // stays glued to the world quad across the clip.
                vu[n] = cu[a] + (((cu[b] - cu[a]) * t) >> 12);
                vv[n] = cv2[a] + (((cv2[b] - cv2[a]) * t) >> 12);
                n++;
            }
        }
        if (n == 0) {
            continue;   // whole quad behind the camera
        }

        int px[8], py[8];
        for (int c = 0; c < n; c++) {
            // Same projection as ProjectEffectSprite (0x0040aa50):
            //   sx = (viewX * f) / z + 160
            //   sy = ((matrix_t1 - rotatedY) * f) / z + 120
            // where rotatedY is the corner's y WITHOUT the translation.
            //
            // The subtraction is NOT a mirror, however much it reads like one:
            // this engine's rotation rows produce a Y-UP value while the
            // translation is Y-DOWN, so the two must be combined with opposite
            // signs. The TMD path - the one that demonstrably renders models
            // correctly - does exactly the same thing by another route:
            // sy = cy - vy*f/vz over a matrix whose t[1] SetRotAndTransMatrix
            // has already negated, i.e. cy + (t1 - rotY)*f/vz. Rewriting this
            // as the "true" view y (t1 + rotY) mirrored the ground quad and the
            // shadow oval visibly counter-rotated against the character.
            //
            // prm->offsetY joins t1, matching the original's primitive
            // translation row (t[0], t[1] + prm[8] + DAT_004c0620, t[2]).
            px[c] = (vx[c] * g_sceneRenderParam) / vz[c] + 160;
            py[c] = ((matrix_t1 + offY - (vy[c] - matrix_t1)) * g_sceneRenderParam)
                    / vz[c] + 120;
        }
        // Depth (the sort key / alpha) comes from the ENTITY corners at
        // quad+0x2C - the original's RotAverage4 input - not the drawn
        // viewport quad. The entity rectangle is the actual ground footprint
        // of the character (player 500x700, zombie 400x400), so its mean
        // view-z is the meaningful depth.
        int depthSum = 0;
        for (int c = 0; c < 4; c++) {
            const SVECTOR* sv = (const SVECTOR*)(quad + 0x2C + c * 4);
            int rz = (int)composed.m[2][0] * (int)sv->x
                   + (int)composed.m[2][2] * (int)sv->z;
            int ez = ((rz + (rz >> 31 & 0xFFF)) >> 12) + (int)composed.t[2];
            depthSum += ez >> 2;
        }
        unsigned int depth =
            (unsigned int)((depthSum + (depthSum >> 31 & 3U)) >> 2);

        unsigned short forceAlpha =
            (unsigned short)(prm->sortBias + 100 + g_FadeSprAlphaBias);

        // 0x00456f6d-0x00456f7d: the record's flag and g_main_state_flags2 bit 3
        // select between three call sites, two of which have identical bodies -
        // so there are only two distinct outcomes, the clamped projected depth
        // or forceAlpha + 1.
        //
        // The selector is an XOR of the two, not an AND: flag == 0 with bit 3
        // SET takes the forceAlpha path too (0x00456fc7 jne). Bit 3 is clear
        // throughout normal play, which is why reading it as an AND behaved.
        unsigned short alpha;
        if ((prm->flag != 0) != ((g_main_state_flags2 & MSF2_FADE_NO_DEPTH_CLAMP) != 0)) {
            alpha = (unsigned short)(forceAlpha + 1);
        } else {
            if (depth > 0xFEF) {
                depth = 0xFF0;
            }
            alpha = (unsigned short)((short)depth + forceAlpha);
        }

        // The tint is three bytes of the quad's primitive header, i.e. the
        // packed 0xRRGGBB the caller left in g_animFrameIdSave and
        // FUN_004565f0 copied into quad[1].z/.pad: 0x00808080 for a normal
        // shadow, 0x00FFFF50 for the death blood pool. The original reads
        // record + 0x1C + slot*0x28 (the double-buffered half's copy of the
        // same header, memcpy'd by FUN_004565f0), not any position bytes.
        unsigned char* rgb = (unsigned char*)FS_REC(i) + 0x1C + slot * 0x28;

        // The tpage parameter becomes the SRV slot in AddFadePoly. The
        // original looked the texture up by tpage in its page descriptor
        // table; in the port the shadow texture (kage.tim) is created at
        // slot 0x2F by LoadShadowMaskTexture, so pass that slot here or the
        // sprite looks up a NULL SRV and is skipped (no shadows / no blood
        // puddle on the corpse).
        // composed.t[2] is the quad origin's view-space Z - the value the
        // original stores in the OT record's translation row (0x004701b4, from
        // the composed matrix) and that AddFadePoly's de-collision pass reads
        // back when ordering this shadow against the ones already queued.
        //
        // The DRAWN key is different: the original re-inserts the transformed
        // quad into the second ordering table - the one that is drawn - at the
        // mean view Z of its vertices (0x0044764a), so pass the mean of the
        // four UNCLIPPED corners here. cvz, not vz: vz holds the clipped
        // polygon, and a corner pushed back to the near plane would drag the
        // mean onto the camera and sort the shadow in front of the very masks
        // it should be behind.
        const int sortZ = (cvz[0] + cvz[1] + cvz[2] + cvz[3]) / 4;
        AddFadePoly(alpha, (int)composed.t[2], sortZ, 0x2F, rgb, px, py, vz, vu, vv, n);
    }

    g_FadeSprCount = 0;
}
