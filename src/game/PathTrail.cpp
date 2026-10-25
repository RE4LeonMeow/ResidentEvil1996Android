// PathTrail.cpp - entity path/trail animation system
 //
 // FUN_0048a210 (entity path animation step) and its three async workers:
 //   FUN_00485820 (0x00485820) -> AsyncTrailBuildGeometry (0x00485610)
 //   FUN_00485a00 (0x00485a00) -> AsyncTrailDraw          (0x00485850)
 //   FUN_00485aa0 (0x00485aa0) -> AsyncTrailReleaseSlot   (0x00485a30)
 //
 // A joint whose flags carry 0x20 is driven along an RDT waypoint path
 // (render_entity calls FUN_0048a210 for it). Each frame the joint's
 // current segment matrix is built from the path waypoints, handed to the GTE
 // state buffer via SetRotAndTransMatrix, then per-segment geometry is staged
 // into two 16-entry CMarniViewport2 pools through the async workers.
 //
 // Pool addresses in the original binary:
 //   pool A = 0x00a74de0 = g_tmdObjectBuffer + 0x151290 (constructed by
 //            FUN_00485020's eh_vector_constructor_iterator, 16 x 0x38)
 //   pool B = 0x00ac3178 (same layout)
 // Per-slot draw entries live in INT_ARRAY_00922f00 (0x21 dwords = 0x84 each).
 //
 // D3D adaptation: the original validates slot handles against the driver's
 // handle table (FUN_004483b0 reads pD3D+0x8e0/0x8e4) before rebuilding. The
 // DX11 port keeps no exposed handle table, so that check reduces to the
 // m_isInitialized test; everything else follows the original flow.

#include "../Globals.h"
#include "../marni/Marni3DObject.h"
#include "../marni/MarniSystem.h"

// Defined in EntityCommon.cpp / Room.cpp (see Globals.h for the shared-scratch
// notes on 0x00be0de4/0x00be0de8).
extern int player_distance_z;        // 0x00be0de4
extern int g_scaled_down_dist;       // 0x00be0de8
extern int is_entity_in_switch_zone(VECTOR* position, void* zoneData); // Room.cpp 0x00462d90

void AsyncTrailBuildGeometry(void);
void AsyncTrailDraw(void);
void AsyncTrailReleaseSlot(void);
void FUN_004850d0(void);
void FUN_004855d0(void* animObj, int r, int g, int b);

// ---------------------------------------------------------------------------
// Staging globals (original addresses in comments; each written by its wrapper
// and consumed by the async body on the scheduler task).
// ---------------------------------------------------------------------------
static void* s_trailGeomObj;           // 0x00aae73c - staged by FUN_00485820
static int   s_trailGeomCount;         // 0x00a75160 - staged by FUN_00485820
static void* s_trailDrawObj;           // 0x00aadaa8 - staged by FUN_00485a00
static int   s_trailDrawBright;        // 0x00aae800 - staged by FUN_00485a00
static void* s_trailReleaseObj;        // 0x008fc420 - staged by FUN_00485aa0

// ---------------------------------------------------------------------------
// Staging globals for the trail slot creator (FUN_004855d0 -> FUN_004850d0).
// joint_enable_special_effect (0x0048a140) passes the joint's anim_object
// block and the three effect size params (0x60/0x28/0x28, or 0x18/0x30/0x18
// for the alt costume). FUN_004850d0 consumes them on the scheduler task.
// ---------------------------------------------------------------------------
static void* s_trailCreateObj;         // 0x00923b48 - anim_object block
static int   s_trailCreateR;           // 0x00a74dd8 - effect size param 2
static int   s_trailCreateG;           // 0x00aae7d8 - effect size param 3
static int   s_trailCreateB;           // 0x00aae738 - effect size param 4


// ---------------------------------------------------------------------------
// 0x008f88a8 - per-slot vertex capacity, written by the trail creator
// (FUN_004850d0, not yet ported) and read by AsyncTrailBuildGeometry.
// Until the creator runs this stays all zero, exactly as in the original.
// ---------------------------------------------------------------------------
int g_trailSlotCapacity[16] = {};      // 0x008f88a8

// ---------------------------------------------------------------------------
// 0x00aada68 - per-slot "in use" flag array (scanned by FUN_004850d0,
// cleared by AsyncTrailReleaseSlot).
// ---------------------------------------------------------------------------
int g_trailSlotUsed[64] = {};          // 0x00aada68

// ---------------------------------------------------------------------------
// 0x008f8c80 - per-slot UV scale table, 16 entries x 4 dwords. Written by the
// unported trail creator, read back by AsyncTrailDraw.
// ---------------------------------------------------------------------------
static DWORD g_trailSlotUV[16][4] = {};  // 0x008f8c80

// ---------------------------------------------------------------------------
// The two CMarniViewport2 pools (16 x 0x38-byte entries each). Pool A lives
// inside g_tmdObjectBuffer at +0x151290 exactly as in the original; pool B is
// a standalone region. Elements are seeded with MarniViewport2_InitEntry
// (the port equivalent of FUN_00485020's placement ctor iterator).
// ---------------------------------------------------------------------------
#define TRAIL_POOL_A(i) ((CMarniViewport2*)((BYTE*)g_tmdObjectBuffer + 0x151290 + (i) * 0x38))  // 0x00a74de0
#define TRAIL_POOL_B(i) ((CMarniViewport2*)(s_trailPoolB + (i) * 0x38))                          // 0x00ac3178

static bool s_trailPoolsReady = false;
static BYTE s_trailPoolB[16 * 0x38] = {};

static void ensure_trail_pools(void)
{
    if (s_trailPoolsReady) return;
    for (int i = 0; i < 16; i++) {
        MarniViewport2_InitEntry((void*)TRAIL_POOL_A(i));
        MarniViewport2_InitEntry((void*)TRAIL_POOL_B(i));
    }
    s_trailPoolsReady = true;
}

// ===========================================================================
// FUN_00485820 (0x00485820) - stage one geometry build and queue it.
// Original stores DAT_00aae73c / DAT_00a75160 then ExecAsync(0x00485610).
// ===========================================================================
void FUN_00485820(void* trailObj, int count)
{
    ensure_trail_pools();
    s_trailGeomObj = trailObj;
    s_trailGeomCount = count;
    ExecAsync((void*)AsyncTrailBuildGeometry);
}

// ===========================================================================
// FUN_00485a00 (0x00485a00) - stage one draw and queue it.
// Original stores DAT_00aadaa8 / _DAT_00aae800 then ExecAsync(0x00485850).
// ===========================================================================
void FUN_00485a00(void* trailObj, int brightness)
{
    ensure_trail_pools();
    s_trailDrawObj = trailObj;
    s_trailDrawBright = brightness;
    ExecAsync((void*)AsyncTrailDraw);
}

// ===========================================================================
// FUN_00485aa0 (0x00485aa0) - stage one slot release and queue it.
// Original stores DAT_008fc420 then ExecAsync(0x00485a30).
// ===========================================================================
void FUN_00485aa0(void* trailObj)
{
    ensure_trail_pools();
    s_trailReleaseObj = trailObj;
    ExecAsync((void*)AsyncTrailReleaseSlot);
}

// ===========================================================================
// FUN_004855d0 (0x004855d0) - stage the joint attack-effect slot creation and
// queue it. Called from joint_enable_special_effect (0x0048a140) with the
// joint's anim_object block and the three effect size params.
// ===========================================================================
void FUN_004855d0(void* animObj, int r, int g, int b)
{
    ensure_trail_pools();
    s_trailCreateObj = animObj;
    s_trailCreateR = r;
    s_trailCreateG = g;
    s_trailCreateB = b;
    ExecAsync((void*)FUN_004850d0);
}

// ===========================================================================
// FUN_004850d0 (0x004850d0) - the trail / joint-attack-effect slot creator.
//
// This is the missing half of the flag-0x20 joint pipeline: joint_enable_
// special_effect sets flags 0x28 (bit 0 cleared so the joint's TMD stops
// rendering, bit 5 set so render_entity routes the joint to FUN_0048a210),
// and THIS function allocates the trail slot, builds the strip's initial
// geometry from the model's current animation frame, and fills the slot's
// draw entry (texture handle, unlit colour scales, capacity, UV scales).
//
// Without it the port's g_trailSlotCapacity stayed all-zero, so every
// flag-0x20 joint - the zombie's rocket-kill gore spurts (type 0x1E on six
// joints), the magnum head shot (0x1E), the leg explosion (0x14), the Tyrant
// limb launches and the player's weapon trails - drew nothing while the joint
// was hidden, which read as "missing body explosion particles".
//
// Reconstructed from the 0x004850d0 disassembly. Sources:
//   animObj = the staged anim_object block; its [0] is the AnimSlot pointer.
//   AnimSlot[0] = data0   -> the animation frame's vertex table
//   AnimSlot[4] = data2   -> the TMD primitive packet list (0x1C stride)
//   AnimSlot[5] = entryCount -> the strip's segment count
// Per segment one 0x34000609 (textured gouraud triangle) packet is decoded:
// three vertices from the frame vertex table (x, -y, z), UVs from the packet
// colour bytes divided by the material's texture width/height.
// ===========================================================================
void FUN_004850d0(void)
{
    // 1. Find a free slot (original scans 0x00aada68..0x00aadaa8 = 16).
    int slot = -1;
    for (int i = 0; i < 16; i++) {
        if (g_trailSlotUsed[i] == 0) { slot = i; break; }
    }
    if (slot < 0) {
        return;
    }
    g_trailSlotUsed[slot] = 1;

    unsigned int* animObj = (unsigned int*)s_trailCreateObj;
    if (animObj == NULL) {
        return;
    }
    unsigned int* slotData = (unsigned int*)animObj[0];    // the AnimSlot
    if (slotData == NULL) {
        return;
    }

    int vertBase = slotData[0];                 // AnimSlot[0] = data0 (frame verts)
    unsigned int* prims = (unsigned int*)slotData[4];  // AnimSlot[4] = data2 (TMD prims)
    int count = slotData[5];                    // AnimSlot[5] = entryCount

    // Texture page from the primitive data's page bits, remapped by the bank
    // redirect table. The page lives inside g_psxTextureArray (0x1b60 per bank).
    int texPage = 0;
    if (prims != NULL) {
        texPage = (*(int*)((char*)prims + 8) & 0x1f0000) >> 0x10;
    }
    if (texPage >= 0 && texPage < 23 && g_textureBankRedirect[texPage] != 0) {
        texPage = g_textureBankRedirect[texPage];
    }
    if (texPage < 0 || texPage >= 32) texPage = 0;
    BYTE* page = (BYTE*)&g_psxTextureArray[texPage * 0x1b60];

    // 2. Release any previous handle and (re)size the two pool viewports.
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[9] != NULL) {
        int oldHandle = INT_ARRAY_00922f00[slot * 0x21 + 0x17];
        if (oldHandle != 0) {
            ((void(*)(void*, int))pD3D->vtable[9])(pD3D, oldHandle);
        }
    }
    INT_ARRAY_00922f00[slot * 0x21 + 0x17] = 0;

    CMarniViewport2* objA = TRAIL_POOL_A(slot);
    CMarniViewport2* objB = TRAIL_POOL_B(slot);

    int vtxCount  = count * 3;
    int listCount = count * 2;
    objA->Release();
    objB->Release();
    if (vtxCount > 0) {
        objA->CreateWork(vtxCount, listCount, 3);
        objB->CreateWork(vtxCount, listCount, 3);
    }

    // 3. Build the strip geometry: one textured triangle per segment, from the
    //    model's frame vertices (AnimSlot[0]) and its TMD prim packets.
    int* e = &INT_ARRAY_00922f00[slot * 0x21];
    int vertBaseIdx = 0;
    int listBaseIdx = 0;
    if (count > 0 && prims != NULL) {
        objA->Lock(NULL, NULL);

        unsigned int* pkt = prims;
        int seg = 0;
        while (seg < count) {
            if ((*pkt & 0xFDFFFFFF) == 0x34000609) {
                // Find the material record for this packet's CLUT word.
                unsigned int clut = pkt[1] >> 0x10;
                int xKey = (int)((clut & 0x3f) << 4);
                int yKey = (int)(clut >> 6);
                int matCount = *(int*)(page + 0x340);
                int matIdx = 0;
                while (matIdx < matCount) {
                    DWORD* rec = (DWORD*)(page + 0x54 + matIdx * 0x68);
                    if ((int)rec[0] == xKey && (int)rec[1] == yKey) break;
                    matIdx++;
                }

                int texW = 256, texH = 256;
                if (matIdx < matCount) {
                    DWORD* rec = (DWORD*)(page + 0x54 + matIdx * 0x68);
                    texW = (int)rec[0x2C / 4];
                    texH = (int)rec[0x30 / 4];
                    if (texW <= 0) texW = 256;
                    if (texH <= 0) texH = 256;
                }

                for (int v = 0; v < 3; v++) {
                    unsigned int vtxOff = pkt[4 + v] >> 0x10;
                    const short* src = (const short*)(vertBase + vtxOff * 8);
                    float vert[11];
                    vert[0] = (float)(short)src[0];
                    vert[1] = -(float)(short)src[1];
                    vert[2] = (float)(short)src[2];
                    vert[3] = 1.0f; vert[4] = 1.0f; vert[5] = 1.0f;   // normal
                    vert[6] = 1.0f; vert[7] = 1.0f; vert[8] = 1.0f;   // colour
                    vert[9]  = (float)(pkt[1 + v] & 0xff) / (float)texW;
                    vert[10] = (float)((pkt[1 + v] >> 8) & 0xff) / (float)texH;
                    objA->SetVertex(vertBaseIdx + v, (DWORD*)vert);
                }

                WORD triA[3] = { (WORD)vertBaseIdx, (WORD)(vertBaseIdx + 1),
                                 (WORD)(vertBaseIdx + 2) };
                WORD triB[3] = { (WORD)vertBaseIdx, (WORD)(vertBaseIdx + 2),
                                 (WORD)(vertBaseIdx + 1) };
                objA->SetList(listBaseIdx, triA);
                objA->SetList(listBaseIdx + 1, triB);

                vertBaseIdx += 3;
                listBaseIdx += 2;
            }
            pkt = (unsigned int*)((char*)pkt + ((*pkt & 0xff00) >> 6) + 4);
            seg++;
        }
        objA->Unlock();
    }

    // 4. Copy the base geometry A -> B (the per-frame build reads B and
    //    writes A; see AsyncTrailBuildGeometry).
    if (vtxCount > 0) {
        objB->CopyFrom(objA);
    }

    // 5. Fill the slot's draw entry. objData (what FlushTmdObjects receives)
    //    is &e[2] (+8 bytes): e[4..0x13] = matrix, e[0x18] = texture handle,
    //    e[0x19/0x1A/0x1B] = the unlit colour scales, e[0x1C] = blend weight.
    e[2] = 4;                                        // type (objData+0x00)
    *(DWORD*)((BYTE*)e + 8 + 0x80) = 2;              // unlit flag (objData+0x80)
    e[0x14] = (int)0x3f800000;                       // objData+0x40
    e[0x15] = (int)0x3f800000;                       // objData+0x44
    e[0x16] = (int)0x3f800000;                       // objData+0x48

    // The texture handle: the model bank's per-material D3D handle, created by
    // TmdAnimation's texture setup (page + 0x34C + matIdx*4).
    unsigned int texHandle = 0;
    if (prims != NULL && (*prims & 0xFDFFFFFF) == 0x34000609) {
        unsigned int clut = prims[1] >> 0x10;
        int xKey = (int)((clut & 0x3f) << 4);
        int yKey = (int)(clut >> 6);
        int matCount = *(int*)(page + 0x340);
        int matIdx = 0;
        while (matIdx < matCount) {
            DWORD* rec = (DWORD*)(page + 0x54 + matIdx * 0x68);
            if ((int)rec[0] == xKey && (int)rec[1] == yKey) break;
            matIdx++;
        }
    if (matIdx < matCount) {
            texHandle = *(unsigned int*)(page + 0x34C + matIdx * 4);
        }
    }
    e[0x18] = (int)texHandle;                        // objData+0x58 = texture

    // Colour scales (read by the unlit branch of FlushTmdObjects at
    // objData+0x5C/0x60/0x64, as FLOATS). The original's (0x60,0x28,0x28)
    // gives a dark-red gore tint; the alt-costume variant (0x18,0x30,0x18) is
    // the green-blood tint. The disasm scales by 1/128, but the strip samples
    // the pale body texture, so a /128 scale reads as bright pink - the /255
    // below keeps the same hue relationship while landing the gore on the
    // darker, more saturated red-brown the original shows. Store the float
    // BITS (FlushTmdObjects dereferences them as float, not as an int value).
    float rScale = (float)s_trailCreateR * (1.0f / 255.0f);
    float gScale = (float)s_trailCreateG * (1.0f / 255.0f);
    float bScale = (float)s_trailCreateB * (1.0f / 255.0f);
    e[0x19] = *(int*)&rScale; e[0x1A] = *(int*)&gScale; e[0x1B] = *(int*)&bScale;
    e[0x1C] = 0;                                     // objData+0x68 = blend weight
    e[0x1D] = *(int*)&rScale; e[0x1E] = *(int*)&gScale; e[0x1F] = *(int*)&bScale;
    e[0x20] = 0;

    // UV-scale table (kept for fidelity; the DX11 port samples UVs from the
    // vertex buffer, so these are only written back into the entry).
    g_trailSlotUV[slot][0] = *(DWORD*)&rScale;
    g_trailSlotUV[slot][1] = *(DWORD*)&gScale;
    g_trailSlotUV[slot][2] = *(DWORD*)&bScale;
    g_trailSlotUV[slot][3] = 0;

    // The object handle (opaque token in the port; the release path reads
    // e[0x17] and calls vtable[9]).
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[7] != NULL) {
        e[0x17] = (int)((unsigned int(*)(void*, void*, unsigned char))pD3D->vtable[7])
                  (pD3D, (BYTE*)e + 8, 0);
    }

    g_trailSlotCapacity[slot] = count - 1;

    // Record the slot index on the anim object (+0x0C), where FUN_0048a210 and
    // the async workers read it.
    animObj[3] = (unsigned int)slot;
}

// ===========================================================================
// FUN_0048a210 (0x0048a210) - entity path animation step, called from
// render_entity for every joint flagged 0x20 (path-driven).
//
// Joint fields (raw offsets - see TmdRenderer/OptionsMenu call sites):
//   +0x02  step counter (byte)         +0x03  total steps (byte)
//   +0x14  -> trail object             +0x18  -> trail object (slot at +0x0C)
//   +0x44  joint world MATRIX          +0x58  VECTOR position (switch zone)
//   +0x5C  int position accumulator    +0x70/+0x72 shorts fed into it
//
// Trail object: +0x0C = pool slot index; points at a path header whose
//   dword[0] = waypoint byte data, dword[4] = 0x1C-byte segment record base,
//   dword[5] = segment count walked downwards.
// ===========================================================================
void FUN_0048a210(void* jointPtr)
{
    unsigned char* j = (unsigned char*)jointPtr;
    int* path = *(int**)(j + 0x14);

    MATRIX m;
    MATRIX* src = &g_identityMatrixData;
    MATRIX* dst = &m;
    for (int i = 8; i != 0; i--) {
        *(unsigned int*)dst->m[0] = *(unsigned int*)src->m[0];
        src = (MATRIX*)(src->m[0] + 2);
        dst = (MATRIX*)(dst->m[0] + 2);
    }

    player_distance_z = path[5];
    g_playerDisplacement = (int)j[2];
    g_scaled_down_dist = (int)j[3] - g_playerDisplacement;
    g_collPushDepthZHi = (int)*(short*)(j + 0x72);

    int acc = (int)*(short*)(j + 0x70) * g_playerDisplacement + *(int*)(j + 0x5C);
    *(int*)(j + 0x5C) = acc;
    if (10000 < acc) {
        j[0x5C] = 0x10;                 // clamp to 10000 (little-endian bytes)
        j[0x5D] = 0x27;
        j[0x5E] = 0;
        j[0x5F] = 0;
    }

    if (is_entity_in_switch_zone((VECTOR*)(j + 0x58), g_CurrentRdtDataTypePtr) != 0 &&
        (int)j[2] + 1 <= (int)j[3]) {

        char* seg = (char*)path[4] + player_distance_z * 0x1C - 0x1C;
        short* wpt = (short*)((unsigned int)*(unsigned short*)(seg + 0x12) * 8 + path[0]);
        m.t[0] = (*wpt >> 4) * g_scaled_down_dist + (*wpt >> 1) * g_playerDisplacement;
        m.t[1] = (wpt[1] >> 4) * g_scaled_down_dist + (wpt[1] >> 1) * g_playerDisplacement;
        m.t[2] = (wpt[2] >> 4) * g_scaled_down_dist + (wpt[2] >> 1) * g_playerDisplacement;
        SetRotAndTransMatrix(&m);
        unsigned int bright = *(unsigned int*)&m.m[0][0];

        while (player_distance_z != 0) {
            player_distance_z--;
            wpt = (short*)((unsigned int)*(unsigned short*)(seg + 0x12) * 8 + path[0]);
            if (player_distance_z % g_collPushDepthZHi == 0) {
                m.t[0] = (*wpt >> 4) * g_scaled_down_dist + (*wpt >> 1) * g_playerDisplacement;
                m.t[1] = (wpt[1] >> 4) * g_scaled_down_dist + (wpt[1] >> 1) * g_playerDisplacement;
                m.t[2] = (wpt[2] >> 4) * g_scaled_down_dist + (wpt[2] >> 1) * g_playerDisplacement;
            }
            SetRotAndTransMatrix(&m);
            if (0xfef < bright) {
                bright = 0xff0;
            }
            seg -= 0x1C;
            FUN_00485820(*(void**)(j + 0x18), player_distance_z);
        }

        ApplyLVAndMul0Matrix(&g_RoomCameraData, j + 0x44, &g_matrixScratch);
        SetRotAndTransMatrix(&g_matrixScratch);
        FUN_00485a00(*(void**)(j + 0x18), (int)bright);
    }

    unsigned char prevStep = j[2];
    j[2] = prevStep + 1;
    if (j[3] < prevStep) {
        FUN_00485aa0(*(void**)(j + 0x18));
        *j &= 0xDE;                     // clear bits 0x01 | 0x20
        j[2] = 0x80;
    }
}

// ===========================================================================
// AsyncTrailBuildGeometry (0x00485610) - consume the staged GTE matrix and
// rewrite the three vertices of this frame's segment.
//
// Reconstructed from the 0x00485610 disassembly:
//  - slot index from trail object +0x0C; skipped when negative or above the
//    slot's capacity (g_trailSlotCapacity[idx]).
//  - both pool slots are Lock()ed around the update (vtable[6], NULL args).
//  - a column-major 4x4 is assembled from g_gteRotTransMatrix: rotation
//    words scaled by 1/4096 (const at 0x004af2d0), translation ints used raw,
//    bottom row (0,0,0,1).
//  - for each of the 3 vertices: GetVertex(index) from pool B, rotate the
//    xyz triple by that matrix (FUN_0048c820), add the translation, write
//    back through SetVertex(index) on pool A (vtable[3]).
//  - both slots Unlock()ed (vtable[7]).
// ===========================================================================
void AsyncTrailBuildGeometry(void)
{
    int idx = *(int*)((char*)s_trailGeomObj + 0x0C);
    int count = s_trailGeomCount;
    if (idx < 0 || count > g_trailSlotCapacity[idx]) {
        return;
    }
    CMarniViewport2* objA = TRAIL_POOL_A(idx);   // 0x00a74de0 + idx*0x38
    CMarniViewport2* objB = TRAIL_POOL_B(idx);   // 0x00ac3178 + idx*0x38

    objA->Lock(NULL, NULL);
    objB->Lock(NULL, NULL);

    const float k = 0.00024414063f;              // 1/4096, const @0x004af2d0
    const MATRIX& g = g_gteRotTransMatrix;

    // Column-major 4x4 from the GTE state buffer (see layout table above).
    float t[16];
    t[0] = (float)(short)g.m[0][0] * k;  t[4] = (float)(short)g.m[0][1] * k;  t[8]  = (float)(short)g.m[0][2] * k;
    t[1] = (float)(short)g.m[1][0] * k;  t[5] = (float)(short)g.m[1][1] * k;  t[9]  = (float)(short)g.m[1][2] * k;
    t[2] = (float)(short)g.m[2][0] * k;  t[6] = (float)(short)g.m[2][1] * k;  t[10] = (float)(short)g.m[2][2] * k;
    t[3] = 0.0f;                         t[7] = 0.0f;                         t[11] = 0.0f;
    t[12] = (float)g.t[0];               t[13] = (float)g.t[1];               t[14] = (float)g.t[2];
    t[15] = 1.0f;

    DWORD prev[11];
    DWORD out[11];
    int baseIdx = count * 3;
    for (int i = 0; i < 3; i++) {
        objB->GetVertex(baseIdx + i, prev);

        float x = *(float*)&prev[0];
        float y = *(float*)&prev[1];
        float z = *(float*)&prev[2];

        // FUN_0048c820(&v, &t): v = rotation columns . v, then + translation.
        float nx = t[0] * x + t[4] * y + t[8]  * z + t[12];
        float ny = t[1] * x + t[5] * y + t[9]  * z + t[13];
        float nz = t[2] * x + t[6] * y + t[10] * z + t[14];

        for (int f = 0; f < 11; f++) {
            out[f] = prev[f];
        }
        *(float*)&out[0] = nx;
        *(float*)&out[1] = ny;
        *(float*)&out[2] = nz;

        objA->SetVertex(baseIdx + i, out);
    }

    objA->Unlock();
    objB->Unlock();
}

// ===========================================================================
// AsyncTrailDraw (FUN_00485850) - build the slot's 0x84-byte D3D draw entry
// in INT_ARRAY_00922f00 and hand it to the driver.
//
// Entry layout (dword indices within idx*0x21):
//   [4..6]/[8..A]/[C..E]  GTE rotation rows scaled by 1/4096
//   [7]/[B]/[F]           0
//   [10..12]              translation ints as floats   [13] = 1.0f
//   [17]                  the slot's driver handle
//   [19..20]/[1D..20]     the slot's UV scale pair, twice
// The entry (+8) is then registered via CMarniDirect3D vtable[10]
// SetTexture(data, 10).
// ===========================================================================
void AsyncTrailDraw(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL || !pD3D->m_isInitialized) {
        return;
    }

    int idx = *(int*)((char*)s_trailDrawObj + 0x0C);
    if (idx < 0) {
        return;
    }

    // FUN_004483b0(pD3D, e[0x17], &poolA[idx]): the original validates the
    // slot handle against the driver table here; the DX11 port has no exposed
    // handle table, so only the initialized gate applies.

    int* e = &INT_ARRAY_00922f00[idx * 0x21];

    const float k = 0.00024414063f;              // 1/4096, const @0x004af2d0
    const MATRIX& g = g_gteRotTransMatrix;
    // The entry's matrix at objData+0x08 (e[4..0x13]) is read back by
    // FlushTmdObjects as 16 FLOATS (`const float* M = objData+0x08`), so the
    // GTE fixed-point values must be stored as float BITS, not as truncated
    // ints. Storing the int value (e.g. 1 for 0x1000*1/4096) made the whole
    // matrix read as denormal/NaN floats and every trail triangle collapsed to
    // world (0,0,0) / NaN - which is why the gore strips never appeared.
    #define TRAIL_STORE_FLOAT(dst, val) do { float f_ = (val); (dst) = *(int*)&f_; } while (0)
    TRAIL_STORE_FLOAT(e[4],  (float)(short)g.m[0][0] * k);
    TRAIL_STORE_FLOAT(e[5],  (float)(short)g.m[1][0] * k);
    TRAIL_STORE_FLOAT(e[6],  (float)(short)g.m[2][0] * k);
    TRAIL_STORE_FLOAT(e[8],  (float)(short)g.m[0][1] * k);
    TRAIL_STORE_FLOAT(e[9],  (float)(short)g.m[1][1] * k);
    TRAIL_STORE_FLOAT(e[0xA], (float)(short)g.m[2][1] * k);
    TRAIL_STORE_FLOAT(e[0xC], (float)(short)g.m[0][2] * k);
    TRAIL_STORE_FLOAT(e[0xD], (float)(short)g.m[1][2] * k);
    TRAIL_STORE_FLOAT(e[0xE], (float)(short)g.m[2][2] * k);
    e[7]  = 0;
    e[0xB] = 0;
    e[0xF] = 0;
    TRAIL_STORE_FLOAT(e[0x10], (float)g.t[0]);
    TRAIL_STORE_FLOAT(e[0x11], (float)g.t[1]);
    TRAIL_STORE_FLOAT(e[0x12], (float)g.t[2]);
    TRAIL_STORE_FLOAT(e[0x13], 1.0f);
    #undef TRAIL_STORE_FLOAT

    e[0x19] = (int)g_trailSlotUV[idx][0];
    e[0x1A] = (int)g_trailSlotUV[idx][1];
    e[0x1B] = (int)g_trailSlotUV[idx][2];
    e[0x1C] = (int)g_trailSlotUV[idx][3];
    e[0x1D] = (int)g_trailSlotUV[idx][0];
    e[0x1E] = (int)g_trailSlotUV[idx][1];
    e[0x1F] = (int)g_trailSlotUV[idx][2];
    e[0x20] = (int)g_trailSlotUV[idx][3];

    // The original hands the entry to CMarniDirect3D vtable[10] (OT insert,
    // type 10). The DX11 port queues it through the same TMD pipeline that
    // draws the room/entity models - TmdQueueComplexObject resolves the
    // texture handle, the unlit colour scales and the GTE matrix (objData+8
    // = e[4..0x13]) the same way FlushTmdObjects does for a TMD slot, and
    // sorts its triangles into the scene depth walk. objData = e + 8 to match
    // the creator's entry layout.
    extern void TmdQueueComplexObject(void* objData, void* elem, int depth);
    if (pD3D != NULL && pD3D->m_isInitialized) {
        TmdQueueComplexObject((BYTE*)e + 8, TRAIL_POOL_A(idx), 10);
    }
}

// ===========================================================================
// AsyncTrailReleaseSlot (0x00485a30) - free one trail slot: delete the
// recorded driver handle (vtable[9] DeleteObjectHandle), Release() both pool
// viewports (their vtable[0]) and clear the in-use flag.
// ===========================================================================
void AsyncTrailReleaseSlot(void)
{
    int idx = *(int*)((char*)s_trailReleaseObj + 0x0C);
    if (idx < 0) {
        return;
    }

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[9] != NULL) {
        int handle = INT_ARRAY_00922f00[idx * 0x21 + 0x17];
        ((void(*)(void*, int))pD3D->vtable[9])(pD3D, handle);
        INT_ARRAY_00922f00[idx * 0x21 + 0x17] = 0;
    }

    TRAIL_POOL_A(idx)->CMarniViewport2::Release();
    TRAIL_POOL_B(idx)->CMarniViewport2::Release();
    g_trailSlotUsed[idx] = 0;
}
