// GteMatrix.cpp - PS1 GTE matrix operations
// Decompiled from Ghidra
//  RotMatrix (0x004406a0)
//  RotMatrixY (0x00409aa0)
//  ApplyMatrix (0x00409cd0)
//  ApplyMatrixSV (0x00409db0)
//  FUN_0040ab80 (0x0040ab80)
//  SetGlobalScaledRotationMatrix (0x0040a9e0)
//  get_matrix_t (0x0040a9c0)
#include "../Globals.h"
#include "../platform/types.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// ============================================================================
// GTE trig lookup tables (standard PS1 12-bit angle precision)
// ============================================================================
static short g_sinTable[4096];
static short g_cosTable[4096];
static bool g_trigTablesInitialized = false;

static void InitTrigTables(void)
{
    if (g_trigTablesInitialized) return;

    // FUN_00440a30: both tables hold 4096 entries at 14-bit amplitude
    // (fsin/fcos * 16384.0, angle step 2*pi/4096 = 0.0015339807880859375),
    // saturated to +/-0x3FFF so a value never reads back as -1.0 exactly.
    //
    // The amplitude matters: GteRotationMatrixCalc combines two table lookups
    // with >> 14 and RotMatrix shifts the result down by 2 more to reach the
    // 4.12 matrix scale. Filling the tables at 4096 instead made every
    // rotation matrix 4-16x too small, and since each joint composes with its
    // parent the error compounded - deep joints ended up with an all-zero
    // rotation, collapsing every vertex of the model onto a single point.
    for (int i = 0; i < 4096; i++) {
        double angle = (double)i * 0.0015339807880859375;
        int s = (int)(sin(angle) * 16384.0);
        int c = (int)(cos(angle) * 16384.0);
        if (s ==  0x4000) s =  0x3FFF;
        if (s == -0x4000) s = -0x3FFF;
        if (c ==  0x4000) c =  0x3FFF;
        if (c == -0x4000) c = -0x3FFF;
        g_sinTable[i] = (short)s;
        g_cosTable[i] = (short)c;
    }
    g_trigTablesInitialized = true;
}

// ============================================================================
// GTE sin/cos lookups (0x004409f0 / 0x00440a10)
// ============================================================================
// Verified against the original's disassembly:
//   FUN_00440a30 allocates 0x004bcacc and fills it with fsin, then allocates
//   0x004bcad0 and fills it with fcos.
//   0x004409f0 reads [0x004bcacc] -> sine.
//   0x00440a10 reads [0x004bcad0] -> cosine.
// (The Ghidra symbols for these two were originally attached the other way
// round; both the names and every call site below have been corrected, so
// GteSin() is sine and GteCos() is cosine throughout.)
int GteSin(int angle)
{
    return g_sinTable[angle & 0xFFF];   // 0x004409f0 -> sin table (0x004bcacc)
}

int GteCos(int angle)
{
    return g_cosTable[angle & 0xFFF];   // 0x00440a10 -> cos table (0x004bcad0)
}

// ============================================================================
// GteRotationMatrixCalc (0x004406a0)
// Compute rotation matrix components from Euler angles (12-bit fixed)
// ============================================================================
static void GteRotationMatrixCalc(int sx, int sy, int sz, int* result)
{
    InitTrigTables();

    int iVar1, iVar2, iVar3, iVar4, iVar5;

    // result[0] = cos(sz) * cos(sy)
    iVar1 = GteCos(sz);
    iVar2 = GteCos(sy);
    result[0] = (int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe;

    // result[1] = -cos(sy) * sin(sz)
    iVar1 = GteCos(sy);
    iVar2 = GteSin(sz);
    result[1] = -((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe);

    // result[2] = sin(sy)
    iVar1 = GteSin(sy);
    result[2] = iVar1;

    // result[3] = sin(sx)*sin(sy)*cos(sz) + cos(sx)*sin(sz)
    iVar1 = GteSin(sx);
    iVar2 = GteSin(sy);
    iVar3 = GteCos(sz);
    iVar3 = ((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe) * iVar3;
    iVar1 = GteCos(sx);
    iVar2 = GteSin(sz);
    result[3] = ((int)(iVar3 + (iVar3 >> 0x1f & 0x3fffU)) >> 0xe) +
                ((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe);

    // result[4] = cos(sx)*cos(sz) - sin(sx)*sin(sz)*sin(sy)
    iVar1 = GteCos(sx);
    iVar2 = GteCos(sz);
    iVar3 = GteSin(sx);
    iVar4 = GteSin(sz);
    iVar5 = GteSin(sy);
    iVar5 = ((int)(iVar3 * iVar4 + (iVar3 * iVar4 >> 0x1f & 0x3fffU)) >> 0xe) * iVar5;
    result[4] = ((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe) -
                ((int)(iVar5 + (iVar5 >> 0x1f & 0x3fffU)) >> 0xe);

    // result[5] = -(sin(sx)*cos(sy))
    iVar1 = GteSin(sx);
    iVar2 = GteCos(sy);
    result[5] = -((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe);

    // result[6] = sin(sx)*sin(sz) - cos(sx)*sin(sy)*cos(sz)
    iVar1 = GteSin(sx);
    iVar2 = GteSin(sz);
    iVar3 = GteCos(sx);
    iVar4 = GteSin(sy);
    iVar5 = GteCos(sz);
    iVar5 = ((int)(iVar3 * iVar4 + (iVar3 * iVar4 >> 0x1f & 0x3fffU)) >> 0xe) * iVar5;
    result[6] = ((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe) -
                ((int)(iVar5 + (iVar5 >> 0x1f & 0x3fffU)) >> 0xe);

    // result[7] = sin(sz)*sin(sy)*cos(sx) + sin(sx)*cos(sz)
    iVar1 = GteSin(sz);
    iVar2 = GteSin(sy);
    iVar3 = GteCos(sx);
    iVar3 = ((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe) * iVar3;
    iVar1 = GteSin(sx);
    iVar2 = GteCos(sz);
    result[7] = ((int)(iVar3 + (iVar3 >> 0x1f & 0x3fffU)) >> 0xe) +
                ((int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe);

    // result[8] = cos(sx)*cos(sy)
    iVar1 = GteCos(sx);
    iVar2 = GteCos(sy);
    result[8] = (int)(iVar1 * iVar2 + (iVar1 * iVar2 >> 0x1f & 0x3fffU)) >> 0xe;
}

// ============================================================================
// RotMatrix (0x00409df0) - calls GteRotationMatrixCalc at 0x004406a0
// Builds a 3x3 rotation matrix from a PS1 SVECTOR rotation (12-bit angles)
// ============================================================================
MATRIX* RotMatrix(SVECTOR* r, MATRIX* m)
{
    int result[9];

    GteRotationMatrixCalc(0x1000 - r->x, (int)r->y, 0x1000 - r->z, result);

    m->m[0][0] = (short)((int)(result[0] + (result[0] >> 0x1f & 3U)) >> 2);
    m->m[0][1] = (short)((int)(result[1] + (result[1] >> 0x1f & 3U)) >> 2);
    m->m[0][2] = (short)((int)(result[2] + (result[2] >> 0x1f & 3U)) >> 2);
    m->m[1][0] = (short)((int)(result[3] + (result[3] >> 0x1f & 3U)) >> 2);
    m->m[1][1] = (short)((int)(result[4] + (result[4] >> 0x1f & 3U)) >> 2);
    m->m[1][2] = (short)((int)(result[5] + (result[5] >> 0x1f & 3U)) >> 2);
    m->m[2][0] = (short)((int)(result[6] + (result[6] >> 0x1f & 3U)) >> 2);
    m->m[2][1] = (short)((int)(result[7] + (result[7] >> 0x1f & 3U)) >> 2);
    m->m[2][2] = (short)((int)(result[8] + (result[8] >> 0x1f & 3U)) >> 2);

    return m;
}

// ============================================================================
// MatrixSetTranslation (0x0040ab80)
// Copies translation vector into the matrix struct
// ============================================================================
void MatrixSetTranslation(MATRIX* m, int* translation)
{
    m->t[0] = translation[0];
    m->t[1] = translation[1];
    m->t[2] = translation[2];
}

// ============================================================================
// GTE fixed-point pipe globals
// ============================================================================
int g_fixedPointPipeMatrix[9] = {};       // 0x004c3790: m00..m22 (see Globals.h)
int g_fixedPointPipeTranslation[3] = {};  // 0x004c37b8: t0..t2

// ============================================================================
// SetGlobalScaledRotationMatrix (0x0040a9e0)
// Copies matrix to the global fixed-point pipe (with 2-bit shift for scaling)
// ============================================================================
void SetGlobalScaledRotationMatrix(MATRIX* m)
{
    // * 4, not << 2: the original's SHL is an arithmetic shift on a signed
    // value, which is UB in C++ for negatives (UBSan flags every call) and
    // would not survive a move to a non-x86 target. The matrix elements are
    // shorts, so * 4 cannot overflow.
    g_fixedPointPipe_matrix_m00 = (int)m->m[0][0] * 4;
    g_fixedPointPipe_matrix_m01 = (int)m->m[0][1] * 4;
    g_fixedPointPipe_matrix_m02 = (int)m->m[0][2] * 4;
    g_fixedPointPipe_matrix_m10 = (int)m->m[1][0] * 4;
    g_fixedPointPipe_matrix_m11 = (int)m->m[1][1] * 4;
    g_fixedPointPipe_matrix_m12 = (int)m->m[1][2] * 4;
    g_fixedPointPipe_matrix_m20 = (int)m->m[2][0] * 4;
    g_fixedPointPipe_matrix_m21 = (int)m->m[2][1] * 4;
    g_fixedPointPipe_matrix_m22 = (int)m->m[2][2] * 4;
}

// ============================================================================
// GetMatrixTranslation (0x0040a9c0)
// Copies translation from matrix to globals
// ============================================================================
void GetMatrixTranslation(MATRIX* m)
{
    matrix_t0 = m->t[0];
    matrix_t1 = m->t[1];
    matrix_t2 = m->t[2];
}

// ============================================================================
// MulMatrixVec3 (0x004410e0)
// 3x3 matrix times 3-vector in the fixed-point pipe's scale. The pipe holds
// matrices shifted left by 2 (SetGlobalScaledRotationMatrix), so 1.0 is 16384 and
// the products come back down with >> 14 - the same 14-bit convention as the trig
// tables, not the 4.12 used by MATRIX itself.
// ============================================================================
void MulMatrixVec3(const int* m, const int* v, int* out)
{
    int acc;
    acc    = v[0] * m[0] + m[2] * v[2] + m[1] * v[1];
    out[0] = (int)(acc + (acc >> 31 & 0x3FFFU)) >> 14;
    acc    = m[4] * v[1] + m[3] * v[0] + m[5] * v[2];
    out[1] = (int)(acc + (acc >> 31 & 0x3FFFU)) >> 14;
    acc    = m[8] * v[2] + m[7] * v[1] + m[6] * v[0];
    out[2] = (int)(acc + (acc >> 31 & 0x3FFFU)) >> 14;
}

// ============================================================================
// ProjectEffectSprite (0x0040aa50)
// Projects one shadow/effect-sprite corner to screen space through the global
// fixed-point pipe, and returns its depth.
//
// `outxy` receives both screen coordinates packed into one dword: X in the low
// 16 bits, Y in the high 16. The +160/+120 are half of the 320x240 framebuffer,
// i.e. the screen origin. Y is negated on the way in (PSX screen space is Y-down
// while the world is Y-up) and negated again via matrix_t1 - out[1].
//
// The returned depth is out[2] >> 2, which is what RotAverage4 averages into a
// sort key.
// ============================================================================
int ProjectEffectSprite(SVECTOR* world, int* outxy)
{
    int v[3];
    int out[3];

    v[0] =  (int)world->x;
    v[1] = -(int)world->y;
    v[2] =  (int)world->z;

    MulMatrixVec3(&g_fixedPointPipe_matrix_m00, v, out);

    out[2] += matrix_t2;
    if (out[2] == 0) {
        out[2] = 1;   // the original guards the divide this way, not by skipping
    }

    unsigned int sx = ((unsigned int)(((out[0] + matrix_t0) * g_sceneRenderParam) / out[2] + 160))
                      & 0xFFFF;
    int sy = ((matrix_t1 - out[1]) * g_sceneRenderParam) / out[2] + 120;
    *outxy = (int)(sx + (unsigned int)sy * 0x10000);

    return out[2] >> 2;
}

// ============================================================================
// RotAverage4 (0x0040ab00)
// Projects four corners and returns their mean depth, rounded toward zero. The
// trailing `p` and `flag` parameters exist in the original's signature and are
// never read - the PSX GTE equivalent returned them, this reimplementation does
// not.
// ============================================================================
int RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                int* sxy0, int* sxy1, int* sxy2, int* sxy3,
                int* p, int* flag)
{
    (void)p; (void)flag;

    int z0 = ProjectEffectSprite(v0, sxy0);
    int z1 = ProjectEffectSprite(v1, sxy1);
    int z2 = ProjectEffectSprite(v2, sxy2);
    int z3 = ProjectEffectSprite(v3, sxy3);

    int sum = z3 + z2 + z1 + z0;
    return (int)(sum + (sum >> 31 & 3U)) >> 2;
}

// ============================================================================
// GteSpriteHeaderInit (0x0040abc0)
// Initializes a PS1-style sprite primitive header
// ============================================================================
void GteSpriteHeaderInit(SVECTOR* header)
{
    *(unsigned char*)((int)&header->pad + 1) = 44;
    unsigned int packed = (unsigned short)header->x | ((unsigned short)header->y << 16);
    packed = (packed & 0xFFFFFF) | 0x09000000;
    header->x = (short)(packed & 0xFFFF);
    header->y = (short)((packed >> 16) & 0xFFFF);
}

// ============================================================================
// GteClutBuild (0x0040ac00)
// Builds a PS1 CLUT reference value
// ============================================================================
int GteClutBuild(int param_1, short param_2)
{
    param_1 = param_1 + (param_1 >> 0x1f & 0xfU);
    return (int)((short)((unsigned short)(param_1 >> 4) & 0x3f ^ (unsigned short)param_2 << 6) |
                 ((param_1 >> 0x14) << 16));
}

// ============================================================================
// GteTpageBuild (0x0040ac30)
// Builds a PS1 tpage/depth reference value
// ============================================================================
int GteTpageBuild(unsigned short param_1, unsigned short param_2, int param_3, int param_4)
{
    param_3 = param_3 + (param_3 >> 0x1f & 0x3fU);
    return (int)((short)((param_1 & 3) * 0x80 +
                         (short)((param_4 + (param_4 >> 0x1f & 0xffU)) >> 8) * 0x10 +
                         (param_2 & 3) * 0x20 +
                         (short)(param_3 >> 6)) |
                 ((param_3 >> 0x16) << 16));
}

// ============================================================================
// GteFixedMul12 (helper)
// 4.12 fixed-point multiply: (a * b) >> 12 with rounding toward zero
// ============================================================================
// The intermediate is 64-bit, and that is load-bearing. The original does this
// multiply in FLOATING POINT - ApplyMatrixLV (0x00409bc0) is three __ftol()
// calls over x87 expressions - so the product is formed at full width and only
// the result is converted to long. A 32-bit `a * b` silently wraps once the
// product passes 2^31.
//
// Nothing hit that until Yawn. Its skeleton animator (0x00408e00) builds the
// body chain in <<9 fixed point, so it hands ApplyMatrixLV offsets 512x larger
// than any other caller: a -1099 joint offset becomes -562688, and against the
// identity's 4096 that is -2,304,770,048 - just past INT_MIN. It wrapped to
// +1,990,197,248, and >>12 then >>9 turned a -1099 step per body segment into
// +949, so the snake built itself backwards and the animator's anchor step
// converted the disagreement into ~20000 units of entity motion per frame.
//
// For every product that already fit in 32 bits this is bit-identical to the
// old code, so no existing caller changes behaviour.
static inline int GteFixedMul12(int a, int b)
{
    long long val = (long long)a * (long long)b;
    return (int)((val + (val >> 63 & 0xFFF)) >> 12);
}

// ============================================================================
// GteFixedMul14 (helper)
// Multiply a 14-bit-amplitude trig value (GteSin/GteCos return +/-0x3FFF for
// +/-1.0) by a 4.12 matrix element and get a 4.12 result: (a * b) >> 14.
//
// Mixing the two scales through GteFixedMul12 instead makes the product 4x too
// large - (0x4000 * 0x1000) >> 12 is 0x4000 where 0x1000 is correct. Every other
// sin/cos consumer in this file does its own >> 0xe for exactly this reason.
// ============================================================================
static inline int GteFixedMul14(int a, int b)
{
    int val = a * b;
    return (val + (val >> 31 & 0x3FFF)) >> 14;
}

// ============================================================================
// RotMatrixY (0x00409aa0)
// Rotates matrix m around the Y axis by PS1 12-bit angle r.
// m = Ry(r) * m
// The original scales m up by 4, calls GteRotationMatrixYXZ(0, r, 0) and
// composes, then scales back down by 4; this is the equivalent closed form.
// The compose step is matrix_compose_yxz (0x00440e30) in the original:
// GteRotationMatrixYXZ(0,r,0) into a scratch matrix followed by FUN_00440e80
// (the covered compose helper); the closed form below replaces both.
// ============================================================================
MATRIX* RotMatrixY(int r, MATRIX* m)
{
    InitTrigTables();

    int sinR = GteSin(r);
    int cosR = GteCos(r);

    short r00 = m->m[0][0], r01 = m->m[0][1], r02 = m->m[0][2];
    short r20 = m->m[2][0], r21 = m->m[2][1], r22 = m->m[2][2];

    // [cos  0  sin]   [r00 r01 r02]   [cos*r00+sin*r20  cos*r01+sin*r21  cos*r02+sin*r22]
    // [  0  1    0] * [r10 r11 r12] = [      r10              r11              r12       ]
    // [-sin 0  cos]   [r20 r21 r22]   [-sin*r00+cos*r20 -sin*r01+cos*r21 -sin*r02+cos*r22]
    // GteFixedMul14, not 12: sinR/cosR are 14-bit amplitude while the matrix is
    // 4.12. Using the 12-bit multiply here scaled every rotation built by this
    // function up by 4, and since Add_speedXZ feeds the result to ApplyMatrixSV
    // (which divides by 4096) every walking or running entity moved 4x its
    // move_speed_current per frame. That is what made the player overshoot his
    // scripted run target - he stepped 840 units a frame past a 250-unit stop
    // threshold - and made NPCs circle a target they could never close on.
    m->m[0][0] = (short)(GteFixedMul14(cosR, r00) + GteFixedMul14(sinR, r20));
    m->m[0][1] = (short)(GteFixedMul14(cosR, r01) + GteFixedMul14(sinR, r21));
    m->m[0][2] = (short)(GteFixedMul14(cosR, r02) + GteFixedMul14(sinR, r22));

    m->m[2][0] = (short)(GteFixedMul14(-sinR, r00) + GteFixedMul14(cosR, r20));
    m->m[2][1] = (short)(GteFixedMul14(-sinR, r01) + GteFixedMul14(cosR, r21));
    m->m[2][2] = (short)(GteFixedMul14(-sinR, r02) + GteFixedMul14(cosR, r22));

    return m;
}

// ============================================================================
// ApplyMatrix (0x00409cd0)
// Applies a rotation matrix to an SVECTOR (PS1 GTE convention: Y-negated I/O).
// result = M * v, rotation only.
//
// The DESTINATION IS A `VECTOR` - three 32-bit ints, not an SVECTOR. The original
// ends with three dword stores:
//
//   00409d94: MOV EAX,dword ptr [ESP+0x54]      ; arg3
//   00409d9c: MOV dword ptr [EAX],ECX           ; ->x
//   00409d9e: MOV dword ptr [EAX+0x4],EDX       ; ->y
//   00409da5: MOV dword ptr [EAX+0x8],ECX       ; ->z
//
// This was declared `SVECTOR* v1` and wrote shorts at stride 2, so callers that
// read the result back as ints - MovePlayerXZ does exactly that, from
// g_playerPosScratch - got the low halves of x/y interleaved as garbage. Rotation
// results routinely exceed 0x7FFF, so the width is load-bearing.
//
// Two details of the original that do NOT change the arithmetic:
//  - it copies the matrix into a scratch struct with every element << 2 and calls
//    MulMatrixVec3, which shifts down by 14. Net (m<<2 * v) >> 14 == (m*v) >> 12,
//    which is what this computes directly.
//  - it also copies t[0] / -t[1] / t[2] into that struct, but MulMatrixVec3 only
//    reads m[0..8], so the translation is dead. This is rotation only.
// ============================================================================
void ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1)
{
    int vx = (int)v0->x;
    int vy = -(int)v0->y;  // PS1 Y negation
    int vz = (int)v0->z;

    int rx = (int)m->m[0][0] * vx + (int)m->m[0][1] * vy + (int)m->m[0][2] * vz;
    int ry = (int)m->m[1][0] * vx + (int)m->m[1][1] * vy + (int)m->m[1][2] * vz;
    int rz = (int)m->m[2][0] * vx + (int)m->m[2][1] * vy + (int)m->m[2][2] * vz;

    // Shift right by 12 (4.12 fixed-point) with rounding toward zero
    rx = (rx + (rx >> 31 & 0xFFF)) >> 12;
    ry = (ry + (ry >> 31 & 0xFFF)) >> 12;
    rz = (rz + (rz >> 31 & 0xFFF)) >> 12;

    v1->x = rx;
    v1->y = -ry;           // PS1 Y negation
    v1->z = rz;
}

// ============================================================================
// ApplyMatrixSV (0x00409db0)
// Applies rotation matrix to SVECTOR, storing result as SVECTOR.
// PS1 GTE convention: Y components are negated on input and output.
// ============================================================================
void ApplyMatrixSV(MATRIX* m, SVECTOR* src, SVECTOR* dst)
{
    int vx = (int)src->x;
    int vy = -(int)src->y;  // PS1 Y negation
    int vz = (int)src->z;

    int rx = (int)m->m[0][0] * vx + (int)m->m[0][1] * vy + (int)m->m[0][2] * vz;
    int ry = (int)m->m[1][0] * vx + (int)m->m[1][1] * vy + (int)m->m[1][2] * vz;
    int rz = (int)m->m[2][0] * vx + (int)m->m[2][1] * vy + (int)m->m[2][2] * vz;

    rx = (rx + (rx >> 31 & 0xFFF)) >> 12;
    ry = (ry + (ry >> 31 & 0xFFF)) >> 12;
    rz = (rz + (rz >> 31 & 0xFFF)) >> 12;

    dst->x = (short)rx;
    dst->y = (short)(-ry);  // PS1 Y negation
    dst->z = (short)rz;
}

// ============================================================================
// fp_lerp (0x0040a3b0)
// Fixed-point SVECTOR interpolation: out = (current * wCur + target * wTgt) >> 12
// ============================================================================
void fp_lerp(SVECTOR* current, SVECTOR* target, int weightCurrent, int weightTarget, SVECTOR* out)
{
    int t, c;

    t = target->x * weightTarget;
    c = current->x * weightCurrent;
    out->x = (short)(((t + (t >> 31 & 0xFFF)) >> 12) + ((c + (c >> 31 & 0xFFF)) >> 12));

    t = target->y * weightTarget;
    c = current->y * weightCurrent;
    out->y = (short)(((t + (t >> 31 & 0xFFF)) >> 12) + ((c + (c >> 31 & 0xFFF)) >> 12));

    t = target->z * weightTarget;
    c = current->z * weightCurrent;
    out->z = (short)(((t + (t >> 31 & 0xFFF)) >> 12) + ((c + (c >> 31 & 0xFFF)) >> 12));
}

// ============================================================================
// MulMatrix0 (0x00409fb0)
// 3x3 matrix multiply: m2 = m0 * m1 (rotation only, 4.12 fixed-point)
// ============================================================================
MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int val = GteFixedMul12((int)m0->m[i][0], (int)m1->m[0][j]) +
                      GteFixedMul12((int)m0->m[i][1], (int)m1->m[1][j]) +
                      GteFixedMul12((int)m0->m[i][2], (int)m1->m[2][j]);
            m2->m[i][j] = (short)val;
        }
    }
    return m2;
}

// ============================================================================
// ApplyMatrixLV (0x00409bc0)
// Matrix-vector multiply for translation: v1 = Rotate(m0, v0)
// Rotation-only transform with PS1 Y-axis negation.
// ============================================================================
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1)
{
    int vx = v0->x;
    int vy = -v0->y;  // PS1 Y negation
    int vz = v0->z;

    int rx = GteFixedMul12((int)m->m[0][0], vx) + GteFixedMul12((int)m->m[0][1], vy) + GteFixedMul12((int)m->m[0][2], vz);
    int ry = GteFixedMul12((int)m->m[1][0], vx) + GteFixedMul12((int)m->m[1][1], vy) + GteFixedMul12((int)m->m[1][2], vz);
    int rz = GteFixedMul12((int)m->m[2][0], vx) + GteFixedMul12((int)m->m[2][1], vy) + GteFixedMul12((int)m->m[2][2], vz);

    v1->x = rx;
    v1->y = -ry;  // PS1 Y negation
    v1->z = rz;
    return v1;
}

// ============================================================================
// CompMatrix (0x0040a190)
// Full matrix composition: m2 = m0 * m1 (rotation + translation)
// ============================================================================
MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2)
{
    MATRIX tmp;
    MulMatrix0(m0, m1, &tmp);
    ApplyMatrixLV(m0, (VECTOR*)m1->t, (VECTOR*)tmp.t);
    tmp.t[0] += m0->t[0];
    tmp.t[1] += m0->t[1];
    tmp.t[2] += m0->t[2];
    *m2 = tmp;
    return m2;
}

// ============================================================================
// ApplyLVAndMulMatrix (0x0040a230)
// In-place matrix composition: m0 = m0 * m1
// ============================================================================
void ApplyLVAndMulMatrix(MATRIX* m0, MATRIX* m1)
{
    CompMatrix(m0, m1, m0);
}

// ============================================================================
// FUN_004403c0 (0x004403c0)
// Euler angle to rotation matrix helper.
// Computes a 9-element rotation matrix from 3 angles (param_1, param_2, param_3)
// and stores the result in param_4[0..8] (3x3 row-major, 4.12 fixed-point).
//
// The (int) casts on the rounded shifts are load-bearing: Ghidra renders the
// sign-correction mask with a U suffix, which makes the addition unsigned and
// turns the following >> into a LOGICAL shift, so any negative product comes
// out as a huge positive. Currently only reachable via camera roll, which every
// shipped RDT leaves at 0 - see the note on MatrixToCamera.
// ============================================================================
void FUN_004403c0(int param_1, int param_2, int param_3, int* param_4) // 0x004403c0
{
    int sum23 = param_3 + param_2;
    int dif23 = param_2 - param_3;
    int c_sum = GteCos(sum23);
    int c_dif = GteCos(dif23);
    int s1 = GteSin(param_1);
    int tmp = ((c_sum - c_dif) / 2) * s1;
    c_sum = GteCos(sum23);
    c_dif = GteCos(dif23);
    param_4[0] = ((int)(tmp + ((tmp >> 0x1f) & 0x3fff)) >> 0xe) + (c_dif + c_sum) / 2;

    int ss1p3 = GteSin(param_3 + param_1);
    int sm1p3 = GteSin(param_1 - param_3);
    int sum12 = param_1 + param_2;
    int dif12 = param_2 - param_1;
    param_4[1] = (ss1p3 - sm1p3) / 2;

    int s_sum12 = GteSin(sum12);
    int s_dif12 = GteSin(dif12);
    int s3 = GteSin(param_3);
    tmp = ((s_sum12 - s_dif12) / 2) * s3;
    int s_dif23 = GteSin(dif23);
    int s_sum23b = GteSin(sum23);
    param_4[2] = ((int)(tmp + ((tmp >> 0x1f) & 0x3fff)) >> 0xe) + (s_sum23b + s_dif23) / 2;

    int c_sum12 = GteCos(sum12);
    int c_dif12 = GteCos(dif12);
    int c3 = GteCos(param_3);
    tmp = ((c_sum12 - c_dif12) / 2) * c3;
    s_dif23 = GteSin(dif23);
    s_sum23b = GteSin(sum23);
    param_4[3] = ((int)(tmp + ((tmp >> 0x1f) & 0x3fff)) >> 0xe) + (s_dif23 - s_sum23b) / 2;

    int cp1p3 = GteCos(param_3 + param_1);
    int cm1p3 = GteCos(param_1 - param_3);
    param_4[4] = (cm1p3 + cp1p3) / 2;

    s_sum12 = GteSin(sum12);
    s_dif12 = GteSin(dif12);
    c3 = GteCos(param_3);
    tmp = ((s_sum12 - s_dif12) / 2) * c3;
    int c_sum23b = GteCos(sum23);
    int c_dif23 = GteCos(dif23);
    param_4[5] = ((int)(tmp + ((tmp >> 0x1f) & 0x3fff)) >> 0xe) + (c_sum23b - c_dif23) / 2;

    int sm1m2 = GteSin(param_1 - param_2);
    int sp1p2 = GteSin(sum12);
    param_4[6] = (sm1m2 - sp1p2) / 2;

    int s1b = GteSin(param_1);
    param_4[7] = -s1b;

    int cm1m2 = GteCos(param_1 - param_2);
    int cp1p2 = GteCos(sum12);
    param_4[8] = (cp1p2 + cm1m2) / 2;
}

// ============================================================================
// MatrixToCamera (0x0040a680)
// Computes a camera view matrix from position/orientation data and stores
// it in g_RoomCameraData (0x004bca88). Used during room/camera transitions.
// Input: pointer to camera from/to position data (ints: from_xyz, to_xyz,
// roll, ...) — also called with a MATRIX* by options_menu.
//
// Rotation formulas (recovered from the original's FPU code):
//   dx = to_x-from_x, dy = to_y-from_y, dz = to_z-from_z
//   len = sqrt(dx²+dy²+dz²), h = sqrt(dx²+dz²)
//   m[0] = ( dz/h,           0,        -dx/h )
//   m[1] = ( dx*dy/(h*len),  h/len,     dy*dz/(h*len) )
//   m[2] = ( dx/len,        -dy/len,    dz/len )
//
// This IS an orthonormal basis, despite appearances. The original negates to_y
// and from_y up front (0x0040a694 / 0x0040a69e) and works with dy' = -dy, so
// m[2] is the forward axis in that flipped frame, m[0] = (dz,0,-dx)/h is the
// horizontal right axis, and m[1] = m[2] x m[0] reduces to the row above
// because dx²+dz² = h². The negated m[2].y is the frame flip, not a quirk.
//
// Translation: t = R * (-from). The original also composes a roll rotation via
// EulerToRotationMatrix(0,0,roll) + MulMatrix0; every shipped RDT camera has
// roll = 0, where that is the identity, so it is omitted here.
// ============================================================================
int MatrixToCamera(MATRIX* m) // 0x0040a680
{
    int camData[8];
    int* src = (int*)m;
    for (int i = 0; i < 8; i++) {
        camData[i] = src[i];
    }

    int fromX = camData[0];
    int fromY = camData[1];
    int fromZ = camData[2];

    double dx = (double)(camData[3] - fromX);
    double dy = (double)(camData[4] - fromY);
    double dz = (double)(camData[5] - fromZ);

    double len = sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1.0) len = 1.0;
    double h = sqrt(dx * dx + dz * dz);
    if (h < 1.0) h = 1.0;   // original divides by h; clamp to avoid NaN

    // Two of these divided by `len` where the original divides by `h`
    // (m[0][2] and m[1][0]). h/len is cos(pitch), so the error vanished on level
    // cameras and grew with the tilt: the right-row came out 17% short and the
    // up-row's X term came out with the wrong sign and magnitude on a 34-degree
    // camera, which pulled the character toward the screen centre and made him
    // look small and stopped short of the wall. Verified instruction by
    // instruction against the FPU code at 0x0040a6f7-0x0040a7bа: the divisors
    // are ST3 = h for dz and dx, and the (-dy'/L) term is multiplied by dx/h and
    // dz/h - i.e. h*len, never len*len. The 4096.0 factors are the literals at
    // 0x004af030 (+4096.0) and 0x004af048 (-4096.0).
    MATRIX* camMatrix = (MATRIX*)&g_RoomCameraData;
    camMatrix->m[0][0] = (short)(int)( dz / h * 4096.0);
    camMatrix->m[0][1] = 0;
    camMatrix->m[0][2] = (short)(int)(-dx / h * 4096.0);
    camMatrix->m[1][0] = (short)(int)( dx * dy / (h * len) * 4096.0);
    camMatrix->m[1][1] = (short)(int)( h / len * 4096.0);
    camMatrix->m[1][2] = (short)(int)( dy * dz / (h * len) * 4096.0);
    camMatrix->m[2][0] = (short)(int)( dx / len * 4096.0);
    camMatrix->m[2][1] = (short)(int)(-dy / len * 4096.0);
    camMatrix->m[2][2] = (short)(int)( dz / len * 4096.0);

    // Translation: rotated camera position (-from)
    VECTOR camPos;
    camPos.x = -fromX;
    camPos.y = -fromY;
    camPos.z = -fromZ;
    ApplyMatrixLV(camMatrix, &camPos, (VECTOR*)camMatrix->t);

    return 0;
}

// ============================================================================
// MulMatrix (0x0040a150)
// In-place matrix multiplication: m0 = m0 * m1
// ============================================================================
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1)
{
    MulMatrix0(m0, m1, m0);
    return m0;
}

// ============================================================================
// ScaleMatrixCols (0x0040a2a0)
// Scale each column of a 3x3 rotation matrix by the corresponding component
// of a VECTOR. Fixed-point: multiply then shift right by 12 (divide by 4096).
// ============================================================================
void ScaleMatrixCols(MATRIX* m, VECTOR* scale)
{
    short* p = &m->m[0][0];
    int sx = scale->x;
    int sy = scale->y;
    int sz = scale->z;

    // Column 0: m[0][0], m[1][0], m[2][0] scaled by sx
    p[0] = (short)((p[0] * sx + ((p[0] * sx >> 31) & 0xFFF)) >> 12);
    p[3] = (short)((p[3] * sx + ((p[3] * sx >> 31) & 0xFFF)) >> 12);
    p[6] = (short)((p[6] * sx + ((p[6] * sx >> 31) & 0xFFF)) >> 12);

    // Column 1: m[0][1], m[1][1], m[2][1] scaled by sy
    p[1] = (short)((p[1] * sy + ((p[1] * sy >> 31) & 0xFFF)) >> 12);
    p[4] = (short)((p[4] * sy + ((p[4] * sy >> 31) & 0xFFF)) >> 12);
    p[7] = (short)((p[7] * sy + ((p[7] * sy >> 31) & 0xFFF)) >> 12);

    // Column 2: m[0][2], m[1][2], m[2][2] scaled by sz
    p[2] = (short)((p[2] * sz + ((p[2] * sz >> 31) & 0xFFF)) >> 12);
    p[5] = (short)((p[5] * sz + ((p[5] * sz >> 31) & 0xFFF)) >> 12);
    p[8] = (short)((p[8] * sz + ((p[8] * sz >> 31) & 0xFFF)) >> 12);
}

// ============================================================================
// GteRotationMatrixYXZ (0x00440b70)
// Compute full rotation matrix from YXZ Euler angles (12-bit fixed).
// Helper for RotMatrixYXZ.
//
// The output is an int[9] (row-major 3x3) at 14-bit amplitude, NOT a MATRIX -
// the original writes nine consecutive dwords at result+0x00..+0x20. Confirmed
// two ways: RotMatrixYXZ (0x00409ed0) reads dwords at [esp+4..esp+0x24] and
// packs them into the nine shorts at m+0..m+0x10, and the matrix multiply at
// 0x00440e80 indexes the same +0x00/+0x04/+0x08 .. +0x20 rows.
//
// With RotMatrixYXZ's argument negation this yields D * Ry(y)Rx(x)Rz(z) * D
// where D = diag(1,-1,1) - the GTE Y-negation baked into the matrix, matching
// the Y handling in ApplyMatrix/ApplyMatrixLV.
// ============================================================================
void GteRotationMatrixYXZ(int x, int y, int z, int* result)
{
    int xMinusY = x - y;
    int xPlusY = x + y;
    int zPlusY = z + y;
    int yMinusZ = y - z;

    int cosXmY = GteCos(xMinusY);
    int cosXpY = GteCos(xPlusY);
    int sinZ = GteSin(z);
    int temp = ((cosXmY - cosXpY) / 2) * sinZ;
    int cosZpY = GteCos(zPlusY);
    int cosYmZ = GteCos(yMinusZ);
    result[0] = ((temp + ((temp >> 31) & 0x3FFF)) >> 14) + (cosYmZ + cosZpY) / 2;

    cosXmY = GteCos(xMinusY);
    cosXpY = GteCos(xPlusY);
    int cosZ = GteCos(z);
    temp = ((cosXmY - cosXpY) / 2) * cosZ;
    int sinYmZ = GteSin(yMinusZ);
    int sinZpY = GteSin(zPlusY);
    result[1] = ((temp + ((temp >> 31) & 0x3FFF)) >> 14) + (sinYmZ - sinZpY) / 2;

    int sinXpY = GteSin(xPlusY);
    int sinXmY = GteSin(xMinusY);
    result[2] = (sinXpY - sinXmY) / 2;

    int sinZpX = GteSin(z + x);
    int sinXmZ = GteSin(x - z);
    result[3] = (sinZpX - sinXmZ) / 2;

    int cosZpX = GteCos(z + x);
    int cosXmZ = GteCos(x - z);
    result[4] = (cosXmZ + cosZpX) / 2;

    int sinX = GteSin(x);
    result[5] = -sinX;

    sinXmY = GteSin(xMinusY);
    sinXpY = GteSin(xPlusY);
    sinZ = GteSin(z);
    temp = ((sinXpY + sinXmY) / 2) * sinZ;
    sinZpY = GteSin(zPlusY);
    sinYmZ = GteSin(yMinusZ);
    result[6] = ((temp + ((temp >> 31) & 0x3FFF)) >> 14) - (sinZpY + sinYmZ) / 2;

    sinXmY = GteSin(xMinusY);
    sinXpY = GteSin(xPlusY);
    cosZ = GteCos(z);
    temp = ((sinXpY + sinXmY) / 2) * cosZ;
    cosYmZ = GteCos(yMinusZ);
    int cosZpY2 = GteCos(zPlusY);
    result[7] = ((temp + ((temp >> 31) & 0x3FFF)) >> 14) + (cosYmZ - cosZpY2) / 2;

    cosXpY = GteCos(xPlusY);
    cosXmY = GteCos(xMinusY);
    result[8] = (cosXmY + cosXpY) / 2;
}

// ============================================================================
// RotMatrixYXZ (0x00409ed0)
// Compute rotation matrix from SVECTOR using YXZ Euler angle order.
// Inverts X and Z axes (PS1 convention), then divides the nine 14-bit
// intermediates by 4 to reach the standard 4.12 matrix scale.
// Only the 3x3 rotation is written; the translation vector is left alone.
// ============================================================================
MATRIX* RotMatrixYXZ(SVECTOR* r, MATRIX* m)
{
    int result[9];
    GteRotationMatrixYXZ(0x1000 - r->x, (int)r->y, 0x1000 - r->z, result);

    m->m[0][0] = (short)((result[0] + ((result[0] >> 31) & 3)) >> 2);
    m->m[0][1] = (short)((result[1] + ((result[1] >> 31) & 3)) >> 2);
    m->m[0][2] = (short)((result[2] + ((result[2] >> 31) & 3)) >> 2);
    m->m[1][0] = (short)((result[3] + ((result[3] >> 31) & 3)) >> 2);
    m->m[1][1] = (short)((result[4] + ((result[4] >> 31) & 3)) >> 2);
    m->m[1][2] = (short)((result[5] + ((result[5] >> 31) & 3)) >> 2);
    m->m[2][0] = (short)((result[6] + ((result[6] >> 31) & 3)) >> 2);
    m->m[2][1] = (short)((result[7] + ((result[7] >> 31) & 3)) >> 2);
    m->m[2][2] = (short)((result[8] + ((result[8] >> 31) & 3)) >> 2);

    return m;
}

// ============================================================================
// rotate_entity (0x0048c2a0)
// Recursively compute world-space matrices for entity joint hierarchy.
// For each child joint of the given index, applies the parent's world matrix
// to the child's local transform, then recurses into the child's children.
// ============================================================================
void rotate_entity(MATRIX* parentMtx, void* animData, unsigned char jointIdx)
{
    Entity* ent = ENTITY;
    if (ent == NULL || ent->jointsStructs == NULL) return;
    JointStruct* joints = ent->jointsStructs;
    JointStruct* joint = &joints[jointIdx];

    unsigned char* animBase = (unsigned char*)animData;
    unsigned short childListOffset = *(unsigned short*)(animBase + 2 + jointIdx * 4);
    unsigned char* childList = animBase + childListOffset;

    // Apply parent transform to joint's local transform → joint's world matrix
    if ((joint->flags & 8) != 0 && (joint->flags & 0x40) == 0) {
        ApplyLVAndMul0Matrix(parentMtx, &joint->transform, &joint->world);
        unsigned char f = joint->flags;
        joint->flags = (f & 0xF5) | 0x50;
        joint->field_02 = 0;
    }
    if ((joint->flags & 2) != 0) {
        ApplyLVAndMul0Matrix(parentMtx, &joint->transform, &joint->world);
    }

    // Recurse into children
    char childCount = *(char*)(animBase + jointIdx * 4);
    for (char i = childCount; i != 0; i--) {
        unsigned char childIdx = *childList;
        childList++;
        rotate_entity(&joint->world, animData, childIdx);
    }
}

// ============================================================================
// EntityComputeJointWorldMatrices (0x0048c190)
// Sets up the entity's camera-relative transform and recursively computes
// all joint world matrices. Called before entity rendering.
// ca: optional scale factor applied to the entity's local matrix (0 = no scale)
// ============================================================================
void EntityComputeJointWorldMatrices(int ca)
{
    Entity* ent = ENTITY;
    if (ent == NULL) return;
    unsigned char* entBytes = (unsigned char*)ent;

    // Skip if entity type is 0x13/0x18 with sub-type 1
    // Original: *(char*)(_ENTITY + 1) and *(char*)(_ENTITY + 2)
    if (((entBytes[1] != 0x0D) && (entBytes[1] != 0x12)) || (entBytes[2] != 1)) {
        if (ent->jointsStructs == NULL || ent->animHeader == 0) return;
        JointStruct* joints = ent->jointsStructs;

        // Parse animation header to get joint hierarchy
        unsigned short* animPtr = (unsigned short*)ent->animHeader;
        unsigned short baseOffset = *animPtr;
        unsigned char* animBase = (unsigned char*)animPtr + (baseOffset & 0xFFFFFFFC);
        unsigned short rootChildOffset = *(unsigned short*)(animBase + 2);
        unsigned char* rootChildList = animBase + rootChildOffset;

        // Build entity rotation matrix from direction angles
        // The rotation SVECTOR is at entity+0x72: {position.pad, directionAngle, speed.x}
        if ((entBytes[3] & 0x80) == 0) {
            SVECTOR* rotVec = (SVECTOR*)((unsigned char*)ent + 0x72);
            RotMatrix(rotVec, &ent->scaMatrixData.localMatrix);
        }

        // Apply optional scale
        if (ca != 0) {
            g_playerPosScratch.x = ca;
            g_playerPosScratch.y = ca;
            g_playerPosScratch.z = ca;
            ScaleMatrixCols(&ent->scaMatrixData.localMatrix, &g_playerPosScratch);
        }

        // Process root joint (index 0): apply entity transform to first joint
        JointStruct* rootJoint = &joints[0];
        if ((rootJoint->flags & 8) != 0 && (rootJoint->flags & 0x40) == 0) {
            ApplyLVAndMul0Matrix(&ent->scaMatrixData.localMatrix,
                                 &rootJoint->transform, &rootJoint->world);
            unsigned char f = rootJoint->flags;
            rootJoint->flags = (f & 0xF5) | 0x50;
            rootJoint->field_02 = 0;
        }
        if ((rootJoint->flags & 2) != 0) {
            ApplyLVAndMul0Matrix(&ent->scaMatrixData.localMatrix,
                                 &rootJoint->transform, &rootJoint->world);
        }

        // Recursively process children of root joint
        char rootChildCount = *(char*)animBase;
        for (char i = rootChildCount; i != 0; i--) {
            unsigned char childIdx = *rootChildList;
            rootChildList++;
            rotate_entity(&rootJoint->world, animBase, childIdx);
        }
    }
}

// ============================================================================
// EntityApplyLookAtRotation (0x0045a2e0)
// Composes the entity's look-at (head/aim tracking) rotation into the world
// matrix of one designated joint, after EntityComputeJointWorldMatrices has
// built the hierarchy:
//
//     joint->world = joint->world * RotMatrixYXZ(0, yaw, pitch)
//
// The two angles are produced by EntityUpdateLookAtAngles (0x00459eb0), which
// aims that joint at the target stored in entity+0xCC/0xD0/0xD4: it derives a
// yaw from getAngleTowardsTarget() minus the entity's own facing, and a pitch
// from atan(dy / sqrt(dx^2 + dz^2)), slews both toward the target by the
// per-frame step limits at entity+0xD9/0xDA, and clamps them to +/-0x2C8 yaw
// (~62 deg) and +/-0x138 pitch (~27 deg) - i.e. a look-at cone.
//
// Gating: the whole lookAtFlags byte is tested, not a single bit - the slew
// updater stops when 0x10 is cleared but this keeps re-applying the last
// angles, which is what freezes a look-at in place instead of snapping back.
// ============================================================================
void EntityApplyLookAtRotation(void)
{
    Entity* ent = ENTITY;
    if (ent == NULL) return;

    if (ent->lookAtFlags != 0) {
        if (ent->jointsStructs == NULL) return;

        // The original indexes jointsStructs by lookAtJointIdx with no bounds
        // check, relying on every writer of entity+0xDD to stay inside the
        // model's joint count. CompMatrix below writes 32 bytes at joint+0x44,
        // so a stale or uninitialised index silently corrupts the heap and
        // surfaces much later as a wild pointer somewhere unrelated. If this
        // ever fires, the real bug is a missing 0xDD write on some entity
        // setup path, not here.
        if (ent->lookAtJointIdx >= ent->jointCount) {
            static int reported = 0;
            if (reported < 8) {
                reported++;
                char buf[160];
                sprintf_s(buf, sizeof(buf),
                          "[LOOKAT] out-of-range joint: idx=%u jointCount=%u "
                          "entity id=%u flags=0x%02X\n",
                          (unsigned)ent->lookAtJointIdx, (unsigned)ent->jointCount,
                          (unsigned)ent->id, (unsigned)ent->lookAtFlags);
                OutputDebugStringA(buf);
            }
            return;
        }

        JointStruct* joint = &ent->jointsStructs[ent->lookAtJointIdx];

        g_svecScratch.x = 0;
        g_svecScratch.y = joint->rotDeltaX;   // yaw   (joint+0x76)
        g_svecScratch.z = joint->rotDeltaY;   // pitch (joint+0x78)

        // Copy identity matrix to scratch
        MATRIX* src = &g_identityMatrixData;
        MATRIX* dst = &g_matrixScratch;
        for (int i = 8; i != 0; i--) {
            *(unsigned int*)dst->m[0] = *(unsigned int*)src->m[0];
            src = (MATRIX*)(src->m[0] + 2);
            dst = (MATRIX*)(dst->m[0] + 2);
        }

        // Apply YXZ rotation and compose with joint's world matrix
        RotMatrixYXZ(&g_svecScratch, &g_matrixScratch);
        CompMatrix(&joint->world, &g_matrixScratch, &joint->world);
    }
}

// ===========================================================================
// vectorMul3 (0x0040a550)
// libgte's OuterProduct with no fixed-point shift: v2 = v0 x v1 on plain ints.
// The .y component is the XZ-plane cross product, which is why every caller
// only looks at its sign - see room_check_sight_blocked in RoomCollision.cpp
// and checkAngularViewAndDistance in entities/EntityCommon.cpp.
//
// All six components are read before the first store: callers pass the same
// VECTOR as v1 and v2 (`vectorMul3(&edge, &p, &p)`), so writing as we go would
// feed a partially updated operand back into the remaining terms.
// ===========================================================================
void vectorMul3(VECTOR* v0, VECTOR* v1, VECTOR* v2)
{
    int x0 = v0->x, y0 = v0->y, z0 = v0->z;
    int x1 = v1->x, y1 = v1->y, z1 = v1->z;

    v2->x = y0 * z1 - y1 * z0;
    v2->y = z0 * x1 - z1 * x0;
    v2->z = y1 * x0 - y0 * x1;

    // The original stores an uninitialised stack word into .pad; nothing reads
    // it, so zero goes here rather than an indeterminate value.
    v2->pad = 0;
}

// ===========================================================================
// VectorNormal (0x0040a5c0)
// libgte's VectorNormal: scales v0 to length 4096 (ONE) into v1 and returns the
// input's squared length. Done in x87 doubles with a truncating __ftol per
// component, not in fixed point.
//
// The zero-length guard is a literal float compare against the 0.0 at
// 0x004af038 (FCOM / TEST AH,0x40 tests C3, i.e. equality only); when it hits,
// the divisor becomes the 1e-9 at 0x004af030+8 instead of 0. Scale factor 4096.0
// is the double at 0x004af030.
// ===========================================================================
int VectorNormal(VECTOR* v0, VECTOR* v1)
{
    double x = (double)v0->x;
    double y = (double)v0->y;
    double z = (double)v0->z;

    double lenSq = x * x + y * y + z * z;
    double len = sqrt(lenSq);
    if (len == 0.0) len = 1e-9;

    v1->x = (int)(x * 4096.0 / len);
    v1->y = (int)(y * 4096.0 / len);
    v1->z = (int)(z * 4096.0 / len);

    return (int)lenSq;
}

// (0x0040ac80) - Set light data by index
// Normalizes the light direction (light position) into g_lightMatrix row idx
// (12-bit fixed, Y negated) and stores the light color as 0..1 floats in
// g_d3dLightData[idx*12 + 6..8] (+ 1.0f at [9]). Input light data layout:
//   int x, y, z;  byte r, g, b  (offsets 0xC, 0xD, 0xE)
void FUN_0040ac80(int idx, void* lightData)
{
    int* p = (int*)lightData;
    double x = (double)p[0];
    double y = (double)p[1];
    double z = (double)p[2];

    // Same normalize-to-4096 as VectorNormal (0x0040a5c0, GteMatrix.cpp), inlined
    double len = sqrt(x * x + y * y + z * z);
    if (len < 1.0) len = 1.0;
    int nx = (int)(x / len * 4096.0);
    int ny = (int)(y / len * 4096.0);
    int nz = (int)(z / len * 4096.0);

    g_lightMatrix.m[idx][0] = (short)nx;
    g_lightMatrix.m[idx][1] = (short)-ny;
    g_lightMatrix.m[idx][2] = (short)nz;
    g_lightMatrix.t[idx] = 0;

    unsigned char* c = (unsigned char*)lightData;
    float* lightColor = (float*)&g_d3dLightData[idx * 12 + 6];
    for (int i = 0; i < 3; i++) {
        unsigned char v = c[0xC + i];
        if (v > 0x7F) v = 0x80;
        lightColor[i] = (float)v * 0.0078125f; // 1/128
    }
    g_d3dLightData[idx * 12 + 9] = 0x3F800000; // 1.0f
}

// (0x00482e20) - Set light matrix for entity rendering
// Transforms light direction vectors through camera matrix and stores
// in D3D light data buffer for the rendering pipeline.
void SetLightMatrix(MATRIX* m) {
    // The original copies 32 bytes (8 iterations × 4 bytes) from the input matrix
    // to a local buffer, then iterates 3 light directions from it.
    // The buffer must be at least 32 bytes (the loop writes 8 DWORDs).
    MATRIX srcCopy;
    memcpy(&srcCopy, m, sizeof(MATRIX));

    int lightIndex = 0;
    DWORD* pLight = g_d3dLightData;
    // The original iterates 3 times over SVECTORs at 6-byte offsets starting from srcCopy
    short* pVec = &srcCopy.m[0][0];

    for (int light = 0; light < 3; light++) {
        SVECTOR vec;
        vec.x = pVec[0];
        vec.y = pVec[1];
        vec.z = pVec[2];
        vec.pad = 0;

        ApplyMatrixSV((MATRIX*)&g_RoomCameraData, &vec, &vec);

        pLight[0] = 2;  // directional light type
        pLight[2] = lightIndex;
        lightIndex++;
        float* pF = (float*)&pLight[3];
        pF[0] = (float)(int)vec.x * 0.00024414063f;
        pF[1] = (float)(int)vec.y * 0.00024414063f;
        pF[2] = (float)(int)vec.z * 0.00024414063f;
        pLight[9] = 0x3f800000; // 1.0f
        pLight[10] = 0;
        pLight += 0xC;
        pVec += 3; // advance by 6 bytes (3 shorts)
    }

    g_d3dLightFlags |= 1;
    g_d3dAmbientColor = ((unsigned int)g_green_color << 8) |
                        ((unsigned int)g_red_color << 16) |
                        (unsigned int)g_blue_color;
}

// (0x00482df0) - Copy rotation+translation matrix to GTE state buffer
void SetRotAndTransMatrix(MATRIX* m) {
    memcpy(&g_gteRotTransMatrix, m, sizeof(MATRIX));
    g_gteRotTransMatrix.t[1] = -g_gteRotTransMatrix.t[1];
}


// ============================================================================
// FUN_004896c0 (0x004896c0)
// Severed-limb ballistic step. render_entity calls this once per frame
// for every joint whose flags carry 0x4 (blown off - see short_push_back and
// explode_leg_and_drop), having first reloaded the launch velocity into the
// joint's rotation SVECTOR: rotation.x = -20, rotation.y = 200, rotation.z = 0.
// This is the ONLY thing that moves a detached joint: rotate_entity skips its
// world matrix once 0x8/0x2 are cleared, so with this stubbed out the limb
// simply hung in the air wherever it came off.
//
//   gravity     = -35   (added to rotation.y scaled by the frame counter)
//   floorY      = -100  (the resting height, and what zombie_update's severed-
//                        limb ground-shadow test compares world.t[1] against)
//   siblingIdx  = 1     (joints away, for the "still near the stump?" gap test)
//
// Per-joint fields, all confirmed against the disassembly:
//   +0x02 field_02   frame counter, reset to 3 on landing
//   +0x03 pad_03     state: 0x80 = has touched the floor, 0x01 = at rest
//   +0x04 rotation   the velocity SVECTOR (x, y, z)
//   +0x58/5C/60      world.t[0..2] = X / Y / Z
// Y is negative-up, so `Y -= vy` with vy falling through zero is the arc.
// ============================================================================
void FUN_004896c0(void* jointPtr, short gravity, short floorY, int siblingIdx)
{
    JointStruct* j = (JointStruct*)jointPtr;

    int thisY  = j->world.t[1];
    int otherY = *(int*)((char*)j + (unsigned int)(unsigned char)siblingIdx * 0x7C + 0x5C);
    int floor  = (int)floorY;

    // 0x004896e4: the gap is built in 16 bits and compared UNSIGNED against 400,
    // so anything more than 200 units either side of the sibling joint tumbles.
    unsigned short gap = (unsigned short)((short)otherY - (short)thisY + 200);

    // Landed and settled: latch bit 0 and freeze.
    if (thisY >= floor && (j->pad_03 & 0x80) != 0) {
        j->pad_03 |= 1;
        return;
    }

    if (gap > 400 || (j->pad_03 & 0x80) == 0) {
        // Tumble: spin the limb's world matrix. The step flips sign once the
        // limb has bounced (pad_03 & 0x80), which is what settles it flat.
        unsigned short landed = (unsigned short)(j->pad_03 & 0x80);
        SVECTOR spin;
        spin.x = (short)((int)(0x20 - (int)landed) >> 1);   // SAR, so signed
        spin.y = (short)((unsigned short)(0x40 - landed) * 2);
        spin.z = 0;
        spin.pad = 0;
        RotMatrix(&spin, &g_matrixScratch);
        MulMatrix(&j->world, &g_matrixScratch);
    }

    // Rotate the launch velocity into world space by the entity's yaw.
    g_matrixScratch = g_identityMatrixData;
    RotMatrixY((int)ENTITY->angle, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &j->rotation, &j->rotation);

    short vz = j->rotation.z;
    j->world.t[0] += (int)j->rotation.x;

    // 16-bit throughout: MOVZX counter, IMUL by gravity, ADD rotation.y.
    short vy = (short)((unsigned short)j->field_02 * gravity + j->rotation.y);
    int y = j->world.t[1];
    j->rotation.y = vy;
    j->world.t[2] += (int)vz;

    y -= (int)vy;
    j->world.t[1] = y;

    if (y >= floor) {
        // First touch bounces to -350; the second snaps exactly to floorY, which
        // is the value zombie_update looks for before drawing the limb's shadow.
        j->world.t[1] = -350;
        if ((j->pad_03 & 0x80) != 0) {
            j->world.t[1] = floor;
        }
        j->field_02 = 3;
        j->pad_03 = 0x80;      // plain store, not an OR
    }

    j->field_02++;
}
