// TmdRenderer.cpp - 3D TMD object render queue (DX11 replacement for the
// original DX5 ordering-table 3D path). See TmdRenderer.h for the data flow.
//
// Per-object data layout inside a CMarniDirect3DTMD slot (0x84-byte entries,
// base at slot+0x4D0 for m_objectData / slot+0xD10 for m_objectDataCopy):
//   +0x00  DWORD  primitive type (4 = TMD mesh object)
//   +0x08  float[16] model->view transform, written by
//          CMarniDirect3DTMD::Transform from the FUN_00483080 matrix. The GTE
//          rotation is stored so that row r of g_gteRotTransMatrix lands in
//          M[r], M[r+4], M[r+8] (matching the original store order at
//          0x004830ef), translation in M[12..14], and the FUN_00486190 view is
//          already folded in. A transformed position is therefore
//          vx = M[0]*x + M[4]*y + M[8]*z + M[12], i.e. the D3D row-vector
//          convention.
//   +0x54  DWORD  D3D object handle (unused in the DX11 port)
//   +0x58  DWORD  texture handle (MarniHandle via VTable_CreateTextureHandle)
//   +0x80  DWORD  render flags. Bit 2 = UNLIT: the original's renderer tests
//          it at 0x00446e99 (skip transforming the light directions) and
//          0x00447043 (skip accumulating the lights - write the vertex colour
//          straight into the primitive). Only the door animation sets it, in
//          DoorAsyncCreateTmd; the door is meant to be full-bright rather than
//          shaded by whatever room lighting was last in the globals.
//
// Geometry lives in the slot's embedded CMarniViewport2 elements
// (slot + objIndex*0x4C): m_pVertexBuffer holds 11-float vertices
// {x,y,z, nx,ny,nz, r,g,b, u,v}, m_pIndexBuffer holds WORD indices
// (3 per triangle, 4 per quad in PS1 order 0,1,3,2). Both position and normal
// have Y negated by PSXObject_Store (PS1 +Y is down, D3D +Y is up).
#include "TmdRenderer.h"
#include "SpriteRenderer.h"
#include "../Globals.h"
#include "../marni/MarniDX.h"
#include "../marni/MarniSystem.h"
#include "../marni/Marni3DObject.h"
#include <cstdlib>
#include <cstring>

// Forward declarations for dependencies defined elsewhere
extern unsigned int AsyncCreateTmdObject(unsigned int param1, unsigned int param2, unsigned int param3);
extern void FUN_004896c0(void* joint, short p1, short p2, int p3); // 0x004896c0 (GteMatrix.cpp)
extern void FUN_0048a210(void* joint);                      // 0x0048a210 (PathTrail.cpp)
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90 (Room.cpp)
extern void FUN_00483580(int* joint, MATRIX* out);          // 0x00483580 item_viewer_compose_matrix (MainMenu.cpp)
#include <algorithm>
#include <cmath>
#include "../DebugPrint.h"

// ============================================================================
// Tuning constants for the projection. The original DX5 renderer projected
// with a perspective viewport; the GTE-equivalent pinhole model uses
//   sx = cx + vx * f / vz
//   sy = cy - vy * f / vz
// where (vx,vy,vz) is the objData-matrix view position, (cx,cy) = subpixel
// offset (screen centre) and f = g_sceneRenderParam (the RDT camera fov /
// title render param).
//
// vz is POSITIVE in front of the camera: FUN_00483080 stores the GTE depth
// (g_gteRotTransMatrix.t[2], positive in front) in the matrix translation and
// FUN_00486190 only rotates it. The Y term is subtracted because
// SetRotAndTransMatrix negates the GTE Y translation while screen Y grows
// downwards.
// ============================================================================
// TMD_NEAR_Z is only the depth-buffer range's lower edge and a floor under the
// real near plane; the near CLIP itself is 2 * g_sceneRenderParam, computed per
// frame in FlushTmdObjects because the fov changes with the camera.
#define TMD_NEAR_Z          (1.0f)
#define TMD_FAR_Z      (131072.0f)    // depth-buffer range; every scene z fits
#define TMD_MAX_QUEUE       2048
#define TMD_MAX_TRIS_FLUSH  1024      // MarniDX::DrawTriangles3D per-call cap
#define TMD_MAX_TRIS_COLLECT 8192     // per-frame triangle pool for the depth sort

// View-space Z -> normalised [0,1] depth for the depth buffer. Linear: a D24
// buffer over TMD_FAR_Z still resolves better than a hundredth of a world unit,
// and a linear ramp keeps distant room geometry from collapsing into one value
// the way a 1/z ramp would.
static inline float TmdDepthNdc(float vz)
{
    float d = (vz - TMD_NEAR_Z) * (1.0f / (TMD_FAR_Z - TMD_NEAR_Z));
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    return d;
}

// Shared with the fade-poly (ground shadow / blood pool) path in
// SpriteRenderer.cpp: its type-12 quads carry the same per-corner view-space
// Z and must land on the SAME [0,1] ramp the model triangles write, or the
// depth comparison between the two is meaningless.
float TmdViewZToNdc(float vz)
{
    return TmdDepthNdc(vz);
}

struct TmdDrawEntry {
    BYTE* slot;        // owning CMarniDirect3DTMD slot in g_tmdObjectBuffer,
                       // NULL for the complex-object pool (see FUN_00486df0)
    BYTE* objData;     // the queued 0x84-byte object entry (matrix at +0x08)
    BYTE* elem;        // geometry element (CMarniViewport2 / CDirect3DObject).
                       // Held explicitly because the complex-object pool keeps
                       // its elements in a SEPARATE 0x38-stride array rather
                       // than embedded in the owning slot at stride 0x4C.
    int   objIndex;    // embedded CMarniViewport2 index (slot-owned entries)
    int   depth;       // original OT depth (gte t[2] >> shift); kept for
                       // reference - ordering is per-triangle, see FlushTmdObjects
};

// Lighting state AS OF QUEUE TIME, one record per queue slot. Unlike the
// transform (which the caller writes after insertion, see TmdQueueObject) the
// lights are already set when an object is queued: render_entity calls
// update_entity_lighting for the entity, then SetLightMatrix per joint, and only
// then queues. On real hardware the driver latched the light state per
// DrawPrimitive, so each entity kept its own lighting; reading the globals at
// flush time instead shaded EVERY entity with whatever was set last - the
// player, since the render loop draws enemies first and the player after. A
// corpse lying still then appeared to be lit by a lamp the player was carrying.
//
// This lives on the heap rather than beside g_tmdQueue on purpose: 152KB of
// extra .bss shifts every static that follows it, and this port has globals
// whose addresses other code derives arithmetically. Keeping the fix out of
// .bss keeps the layout byte-identical to before it.
struct TmdLightState {
    float dir[3][3];   // g_d3dLightData[i*12 + 3..5]
    float col[3][3];   // g_d3dLightData[i*12 + 6..8]
    DWORD ambient;     // g_d3dAmbientColor
};

// Pinned so a change here cannot silently grow the .bss footprint unnoticed.
// 20 rather than 16 since the geometry element is now carried explicitly: the
// complex-object pool (FUN_00486df0) keeps its elements outside any TMD slot,
// so `slot + objIndex * 0x4C` can no longer stand in for it.
static_assert(sizeof(TmdDrawEntry) == 20, "TmdDrawEntry must stay 20 bytes");

static TmdDrawEntry  g_tmdQueue[TMD_MAX_QUEUE];
static int           g_tmdQueueCount = 0;
static TmdLightState* g_tmdLight = NULL;   // TMD_MAX_QUEUE records, heap, never freed

// Per-frame triangle pool. Triangles from every queued object are gathered here
// and submitted only after a global depth sort (see FlushTmdObjects).
// 3 vertices x {x, y, z(ndc), w, u, v, r, g, b, a}. `w` is the vertex's
// view-space Z: DrawTriangles3D's vertex shader divides by it so the UVs and
// colours interpolate perspective-correctly instead of affinely (see
// g_Model3DVS_Source). Without it, large near polygons - the door panel in the
// room transition above all - swim their texture as they turn.
#define TMD_VERT_FLOATS  10
#define TMD_TRI_FLOATS   (TMD_VERT_FLOATS * 3)
struct TmdTri {
    float v[TMD_TRI_FLOATS];
    float depth;   // mean view-space Z (larger = farther)
    DWORD tex;
    float alpha;   // opacity this triangle was emitted with (1 - the record's
                   // +0x68 background weight); < 1 marks translucent geometry
                   // (water, glass), which must NOT write depth or it hides
                   // the fade polys (ground shadows / blood pools) behind it -
                   // see room40E0 flooded, where the water plane silenced
                   // every shadow once they became depth-tested.
    int   otDepth; // the ordering-table depth its object was queued at. Only
                   // used to break exact `depth` ties - see the sort below.
};
static TmdTri g_tmdTris[TMD_MAX_TRIS_COLLECT];
static int    g_tmdTriOrder[TMD_MAX_TRIS_COLLECT];

// ============================================================================
// TmdQueue_Reset
// ============================================================================
void TmdQueue_Reset(void)
{
    g_tmdQueueCount = 0;
}

// ============================================================================
// TmdQueueObject (called from CMarniDirect3D vtable[10], original 0x00448300)
// Recovers the owning TMD slot + object index from the objData pointer and
// records a reference to it for this frame.
//
// Only the POINTER may be recorded here, never a copy of the object's render
// state: CMarniDirect3DTMD::Transform (0x00415520) inserts every object into
// the ordering table FIRST and writes the model->view matrix to objData + 0x08
// afterwards, exactly as the original does. Snapshotting the matrix at queue
// time therefore yields the previous frame's transform - or an all-zero matrix
// the first time a slot is used, which collapses every vertex onto the near
// plane and draws nothing at all.
// ============================================================================
void TmdQueueObject(void* objData, int depth)
{
    if (objData == NULL || g_tmdQueueCount >= TMD_MAX_QUEUE) return;

    if (g_tmdLight == NULL) {
        g_tmdLight = (TmdLightState*)calloc(TMD_MAX_QUEUE, sizeof(TmdLightState));
        if (g_tmdLight == NULL) return;
    }

    BYTE* p = (BYTE*)objData;
    const int slotStride = TMD_SLOT_STRIDE;

    // Resolve the pointer to (owning slot, offset within it). Every region that
    // can own a CMarniDirect3DTMD slot is listed here; they are disjoint, so
    // order does not matter. Each bound is the region's own size - notably the
    // main buffer's is its capacity, NOT TMD_CLEANUP_SLOT_COUNT, which is a
    // fact about what the cleanup destroys rather than about what is
    // addressable. Tying those two together is what broke the door animation.
    static const struct { BYTE* base; size_t size; } kSlotRegions[] = {
        { g_renderStateTMD,     sizeof(g_renderStateTMD)     },  // item examine / render state
        { g_doorTmdSlotBuffer,  sizeof(g_doorTmdSlotBuffer)  },  // door animation
        { g_itemTmdSlotBuffer,  sizeof(g_itemTmdSlotBuffer)  },  // item viewer
        { g_itemSharedTmdSlot,  sizeof(g_itemSharedTmdSlot)  },  // item viewer, shared transparent
        { g_tmdObjectBuffer,    sizeof(g_tmdObjectBuffer)    },  // entities, room objects
    };

    BYTE* slotBase = NULL;
    ptrdiff_t within = 0;
    for (size_t r = 0; r < sizeof(kSlotRegions) / sizeof(kSlotRegions[0]); r++) {
        BYTE* base = kSlotRegions[r].base;
        if (p < base || p >= base + kSlotRegions[r].size) continue;
        ptrdiff_t diff = p - base;
        ptrdiff_t off  = (diff / slotStride) * slotStride;
        slotBase = base + off;
        within   = diff - off;
        break;
    }
    if (slotBase == NULL) return;

    int objIndex;
    if (within >= 0x4D0 && within < 0x4D0 + 16 * 0x84) {
        objIndex = (int)(within - 0x4D0) / 0x84;
    }
    else if (within >= 0xD10 && within < 0xD10 + 16 * 0x84) {
        objIndex = (int)(within - 0xD10) / 0x84;
    }
    else {
        return;
    }

    int idx = g_tmdQueueCount++;
    TmdDrawEntry* e = &g_tmdQueue[idx];
    e->slot     = slotBase;
    e->objData  = p;
    e->elem     = slotBase + objIndex * 0x4C;
    e->objIndex = objIndex;
    e->depth    = depth;

    // Latch the light state for this object (see the note on TmdLightState).
    TmdLightState* ls = &g_tmdLight[idx];
    const float* lights = (const float*)g_d3dLightData;
    for (int i = 0; i < 3; i++) {
        const float* L = lights + i * 12;
        ls->dir[i][0] = L[3]; ls->dir[i][1] = L[4]; ls->dir[i][2] = L[5];
        ls->col[i][0] = L[6]; ls->col[i][1] = L[7]; ls->col[i][2] = L[8];
    }
    ls->ambient = g_d3dAmbientColor;
}

// ============================================================================
// Queue one entry from the complex-object pool (FUN_00486df0). Same contract as
// TmdQueueObject, except the geometry element is passed in: those elements live
// in g_objectListPtrArray at stride 0x38, not embedded in a TMD slot at 0x4C,
// so there is no slot to resolve the pointer against.
// ============================================================================
void TmdQueueComplexObject(void* objData, void* elem, int depth)
{
    if (objData == NULL || elem == NULL || g_tmdQueueCount >= TMD_MAX_QUEUE) return;

    if (g_tmdLight == NULL) {
        g_tmdLight = (TmdLightState*)calloc(TMD_MAX_QUEUE, sizeof(TmdLightState));
        if (g_tmdLight == NULL) return;
    }

    int idx = g_tmdQueueCount++;
    TmdDrawEntry* e = &g_tmdQueue[idx];
    e->slot     = NULL;
    e->objData  = (BYTE*)objData;
    e->elem     = (BYTE*)elem;
    e->objIndex = 0;
    e->depth    = depth;

    TmdLightState* ls = &g_tmdLight[idx];
    const float* lights = (const float*)g_d3dLightData;
    for (int i = 0; i < 3; i++) {
        const float* L = lights + i * 12;
        ls->dir[i][0] = L[3]; ls->dir[i][1] = L[4]; ls->dir[i][2] = L[5];
        ls->col[i][0] = L[6]; ls->col[i][1] = L[7]; ls->col[i][2] = L[8];
    }
    ls->ambient = g_d3dAmbientColor;
}

// ============================================================================
// Lighting. g_d3dLightData holds 3 lights x 12 DWORDs:
//   [3..5] = direction float3 (GTE camera space, written by SetLightMatrix)
//   [6..8] = color float3 0..1 (written by FUN_0040ac80)
// Normals are rotated by the objData matrix, which is the GTE joint rotation
// composed with the FUN_00486190 view rotation. That view is a small tilt away
// from identity (it only leans by the subpixel offset), so the light directions
// are used in GTE camera space as-is: both the normals and the light vectors go
// through the same camera rotation, and a rotation preserves the dot product.
// A previous revision negated X and Z here to cancel out an incorrect 180 degree
// view flip in FUN_00486190.
// ============================================================================
static void TmdComputeLight(const TmdLightState* ls, const float* n, const float* rot,
                            float* outR, float* outG, float* outB)
{
    // Normal into view space (rotation part of the objData matrix)
    float nx = rot[0] * n[0] + rot[4] * n[1] + rot[8]  * n[2];
    float ny = rot[1] * n[0] + rot[5] * n[1] + rot[9]  * n[2];
    float nz = rot[2] * n[0] + rot[6] * n[1] + rot[10] * n[2];

    // Ambient from the latched g_d3dAmbientColor (packed r<<16|g<<8|b, 0..255)
    // - NOT the live global, see TmdLightState.
    float r = (float)((ls->ambient >> 16) & 0xFF) / 255.0f;
    float g = (float)((ls->ambient >> 8)  & 0xFF) / 255.0f;
    float b = (float)( ls->ambient        & 0xFF) / 255.0f;

    for (int i = 0; i < 3; i++) {
        // L[3..5] = direction, L[6..8] = colour, from the latched copy.
        const float L[9] = {
            0.0f, 0.0f, 0.0f,
            ls->dir[i][0], ls->dir[i][1], ls->dir[i][2],
            ls->col[i][0], ls->col[i][1], ls->col[i][2],
        };
        // SetLightMatrix writes pLight[0] = 2 (D3DLIGHT_DIRECTIONAL) and stores
        // the normalised light POSITION in the direction field, so [3..5] is a
        // D3D dvDirection: the direction the light travels. D3D's diffuse term
        // is dot(N, -dvDirection), hence the negation. Dotting without it lit
        // only the faces pointing away from the camera - the options-menu lights
        // all sit just behind the model (z = -780 with the model at z = -700),
        // so the visible side came out at ambient level (~10%).
        float d = -(nx * L[3] + ny * L[4] + nz * L[5]);
        if (d > 0.0f) {
            r += d * L[6];
            g += d * L[7];
            b += d * L[8];
        }
    }

    *outR = (r > 1.0f) ? 1.0f : r;
    *outG = (g > 1.0f) ? 1.0f : g;
    *outB = (b > 1.0f) ? 1.0f : b;
}

// ============================================================================
// FlushTmdObjects - transform, light and draw every queued TMD object.
//
// The queue entry's `depth` (the original OT depth) is not used for ordering:
// triangles from all objects go into one pool and are sorted individually by
// view-space Z, which is what the original's depth buffer did within an object.
//
// The scene sprites - the room background masks and the entities' ground
// shadows - are merged into that same far-to-near walk. The original put them
// and the entity primitives in one ordering table, and their keys are directly
// comparable with a triangle's view-space Z, because this port stores every
// scene primitive at depthSort = OT index * 16 while an entity enters the table
// at t[2] >> 4 (FUN_00483250 passes depthShift 4). Drawing the whole TMD pass
// first and the sprites afterwards - which is what this did - put every mask in
// front of the player unconditionally, whichever side of the wall she was
// standing on, and every shadow in front of every mask.
//
// Scene sprites do not write depth, so the interleave is a pure painter's
// order: every triangle behind one is already down before it paints over them,
// and every triangle in front of it is submitted after. Triangle-vs-triangle
// ordering still comes from the depth buffer, untouched.
// ============================================================================
// 255 room sprites (the count is a byte) + shadows + the 2D billboard effects.
// Above MAX_SPRITE_COMMANDS (300) on purpose: the whole queue is the ceiling on
// how many scene depths can exist, so this bound can never actually be reached.
#define TMD_MAX_SCENE_DEPTHS  320

void FlushTmdObjects(void)
{
    int queued = g_tmdQueueCount;

    unsigned int maskDepths[TMD_MAX_SCENE_DEPTHS];
    const int    maskCount  = SpriteQueue_CollectSceneDepths(maskDepths, TMD_MAX_SCENE_DEPTHS);
    int          maskIdx    = 0;
    unsigned int maskCursor = 0xFFFFFFFFu;

    if (queued > 0 && g_tmdLight != NULL && Marni_DX() != NULL) {
        float scaleX, scaleY;
        MarniGetRenderScale(&scaleX, &scaleY);
        float cx = (float)g_SubpixelOffsetX * scaleX;
        float cy = (float)g_SubpixelOffsetY * scaleY;
        // Project in GAME-SPACE (320x240) exactly like the original GTE
        // (GteMatrix.cpp: sx = x*param/z + 160, sy = ... + 120) and only then
        // apply the render scales - X and Y SEPARATELY. The old code folded
        // scaleX into f and applied it to both axes, which was fine while the
        // backbuffer shared the logical 4:3 (640x480 of 320x240) but inflates
        // the models by scaleX/scaleY on the 16:9 native-resolution
        // fullscreen backbuffer (models render ~1.33x too tall, i.e. "closer
        // to the camera"). The pre-rendered backgrounds stretch anisotropically,
        // so the models must too.
        float fg = (float)g_sceneRenderParam;

        // Near plane. The original marks a vertex clipped when its view-space Z
        // is under TWICE the projection distance (0x00447023-0x0044703c):
        //     MOV EAX, [EBP + 0x44]   ; the projection distance (g_primParam,
        //                             ; which FUN_0040a8f0 loads from
        //                             ; g_sceneRenderParam - the RDT camera fov)
        //     ADD EAX, EAX            ; 2 * f
        //     CMP EAX, EBX            ; EBX = (int)vz
        //     JLE  -> [vtx+0x18] = 0  ; keep
        //             [vtx+0x18] = 1  ; clip
        // and drops a primitive whose vertices' clip flags do not sum to zero
        // (the `... + ... + ... == 0` test guarding every AddPrim in
        // FUN_00446e40). This port used 1.0, i.e. "behind the eye" only, so
        // anything that came within a few hundred units of the camera still
        // projected - blown up until it filled the frame. That is the boulder
        // pinning itself against the wall the room-30F0 camera looks out of,
        // and the crank-driven bridge wall as it swings through the eye point.
        //
        // vz is in world units, so this must use the UNSCALED parameter: f
        // above carries the render-resolution scale purely to turn view space
        // into pixels.
        float nearZ = 2.0f * (float)g_sceneRenderParam;
        if (nearZ < TMD_NEAR_Z) nearZ = TMD_NEAR_Z;   // fov never read / zero

        // Is the original's OWN per-texel path aimed at something this frame?
        // FUN_00484c40 knocks the STP texels out of a COPY of the page and
        // FUN_00484dc0 draws the model through g_renderStateTMD unblended - a
        // whole mechanism with exactly ONE call site in the game, the
        // aquarium's tank shell (cmd_omodel_set, MANSION_2F / ROOM_STUDY_2F
        // slot 0).
        //
        // What the original gives that object is an UNBLENDED draw, so the
        // shell's authored look is full-bright (its USA counterpart, which has
        // no ABE prims at all, renders exactly that way in Mode=OG). Relighting
        // the shell through the generic twin therefore fights the original:
        // measured, it came out dark brown against the PS1's cream.
        //
        // The TWIN itself must stay, though, and this is the important half: in
        // this port that render-state pass is not covering the shell at all
        // (suppressing the twin left the tank fully see-through), so the twin's
        // opaque copy is the only thing making the DC shell solid. Without it,
        // the shell's own ABE blend at +0x68 = 0.5 is all there is.
        bool renderStateModelActive = false;
        for (int i = 0; i < queued; i++) {
            if (g_tmdQueue[i].slot == g_renderStateTMD) {
                renderStateModelActive = true;
                break;
            }
        }
        bool uniformBlend = (g_stageId % 5 == STAGE_MANSION_2F &&
                             g_roomId == ROOM_FRONT_LESSON_ROOM);

        // Triangles are collected across every queued object and submitted only
        // after a per-triangle depth sort. The original inserts each TMD object
        // into the ordering table at a single depth and lets the D3D depth
        // buffer resolve the triangles inside it; MarniDX::DrawTriangles carries
        // no Z, so without this the far side of each limb paints over its own
        // near side and the model renders as a dark shell.
        int collected = 0;
#ifdef __EMSCRIPTEN__
        // Port metadata lives separately; keep TmdTri and the original pools
        // unchanged. Slot/element/primitive identity survives depth sorting and
        // normalizes the alternating object-data/copy transform buffers.
        static MarniTriangleKey* frameKeys=nullptr;
        bool collectKeys=Marni_DX()->WantsInterpolation();
        if(collectKeys&&!frameKeys)frameKeys=static_cast<MarniTriangleKey*>(malloc(TMD_MAX_TRIS_COLLECT*sizeof(MarniTriangleKey)));
        collectKeys=collectKeys&&frameKeys;
#endif

        for (int i = 0; i < queued; i++) {
            TmdDrawEntry* e = &g_tmdQueue[i];

            // TmdQueueObject only ever stores a slot inside g_tmdObjectBuffer,
            // so a null slot means this record was never filled - i.e. the
            // count outran the writes. Dereferencing it read address 4 and
            // faulted; skip it and say so instead.
            if (e->elem == NULL) {
                dbg_printf("FlushTmdObjects: unfilled queue entry %d of %d "
                           "(objData=%p objIndex=%d)\n",
                           i, queued, (void*)e->objData, e->objIndex);
                continue;
            }

            CMarniViewport2* elem = (CMarniViewport2*)e->elem;

            const float* vbuf = (const float*)elem->m_pVertexBuffer;
            const WORD*  ibuf = (const WORD*)elem->m_pIndexBuffer;
            int vtxCount = (int)elem->m_vertexCount;
            int listCount = (int)elem->m_listCount;
            int primType = (int)elem->m_primitiveType;
            if (vbuf == NULL || ibuf == NULL || vtxCount <= 0 || listCount <= 0)
                continue;
            if (primType != 3 && primType != 4)
                continue;

            // Read the transform and texture handle now (see TmdQueueObject):
            // both are written to the object entry after it was queued.
            const float* M = (const float*)(e->objData + 0x08);
            // A NULL slot marks a complex-pool entry. Those are only ever built
            // from textured triangle primitives (ComplexTmdObjectSetup filters
            // on flags == 0x34000609) and their 0x38-byte element has no +0x48
            // flag to read, so the texture is unconditional there.
            DWORD tex = (e->slot == NULL || *(DWORD*)(e->elem + 0x48) != 0)
                        ? *(DWORD*)(e->objData + 0x58) : 0;
            // Render flags at +0x80: bit 2 = unlit, take the vertex colour as
            // it stands (see the layout note at the top of this file).
            bool unlit = (*(DWORD*)(e->objData + 0x80) & 2) != 0;

            // Per-object colour scale. Marni3DObject's store loop writes 1.0f
            // to all three at load (Marni3DObject.cpp, `puVar4[0..2]`), and
            // scd_model_tint_apply -> TmdObjectTintSet (0x00485fa0) overwrites
            // them with `1.0 + delta` to tint a whole model at runtime.
            const float objScaleR = *(float*)(e->objData + 0x5C);
            const float objScaleG = *(float*)(e->objData + 0x60);
            const float objScaleB = *(float*)(e->objData + 0x64);

            // Blend weight at +0x68, INDEPENDENT of the unlit flag.
            // CMarniDirect3DTMD::Create zeroes the field (puVar4[3] = 0 at
            // 0x00415650), so an object nobody marked semi-transparent reads 0
            // here. Three writers set it:
            //   CreateTmdObjectInternal (0x00483910) - room/entity TMDs; it
            //     happens to set the unlit bit as well, which is what made
            //     gating on that bit look right,
            //   FUN_00483270 (0x004834d2), which re-stamps the anim object's
            //     +0x14 into +0x68/+0x78 of all 31 records every frame, and
            //   the item viewer (0x0048467f) - it stamps DAT_004d2c10 into
            //     +0x68/+0x78 of every record and sets NO flag at all.
            // Gating on the unlit bit therefore dropped the examine screen's
            // 50% pass entirely: the glass bottles rendered solid.
            //
            // The value is the weight of the BACKGROUND, not the opacity of
            // the model - the two are inverted. The proof is the per-texel
            // semi-transparency pass (FUN_00484dc0): its whole job is to draw
            // the opaque part of a model back over the translucent copy, and
            // `Create` leaves its +0x68 at 0. Read as opacity, 0 makes that
            // pass invisible; read as a background weight, 0 is exactly "let
            // none of the background through". The same reading is what makes
            // `blendMode == 0 -> +0x14 = 0` mean opaque in
            // CreateTmdObjectInternal, and it puts abr 0 - the common case,
            // table value 0x80 - on 0.5*src + 0.5*dst, the PS1 B/2 + F/2 rule
            // exactly.
            //
            // 0.5 maps to 0.5 either way, so this only moves objects that are
            // NOT half-transparent. In practice that is room 20A0's tank water:
            // cmd_omodel_set writes 0x004d2be0 = 0x30 for that one model
            // (0x00461df2, the only site in the game), which becomes 0.1875
            // background = 81% opaque. It had been rendering at 19% opacity - a
            // dark wash you could read the wallpaper through instead of a solid
            // green tank.
            float triAlpha = 1.0f;
            {
                float bgWeight = *(float*)(e->objData + 0x68);
                if (bgWeight < 0.0f)
                    triAlpha = 1.0f + bgWeight;
                else if (bgWeight > 0.0f && bgWeight <= 1.0f)
                    triAlpha = 1.0f - bgWeight;
                if (triAlpha < 0.0f) triAlpha = 0.0f;
                if (triAlpha > 1.0f) triAlpha = 1.0f;
            }

            // Per-texel semi-transparency. The PS1 blends a texel only when its
            // palette entry carries the STP bit, so a model whose primitives are
            // all ABE-flagged still has opaque parts - room 107's display case
            // is 80 ABE prims over a palette with just two STP entries, so its
            // glass top blends and its wooden frame must not. Blending the whole
            // record at `triAlpha` (what this did) washes the frame out.
            //
            // MarniStpKnockoutTwin hands back a copy of the same texture holding
            // only the opaque texels, so the geometry can be emitted a second
            // time at full alpha: that pass covers the blended one wherever the
            // texture is opaque, leaving the blend visible only where the PS1
            // would have blended it. Both passes carry the same vertices and so
            // a bit-identical depth; the LESS_EQUAL depth test lets the opaque
            // copy through because the blended one wrote no depth, and the sort
            // keeps each opaque copy immediately after the blended copy it
            // covers - the ordering matters where a translucent part of the
            // model sits in front of an opaque one, as the case's glass top
            // does. Room 107's display case is the model this exists for.
            MarniHandle knockTex = MARNI_NULL_HANDLE;
            if (triAlpha < 0.999f && !uniformBlend)
                knockTex = MarniStpKnockoutTwin((MarniHandle)tex);

            // The same ABE path also forces the model full-bright - bit 2 IS the
            // renderer's unlit path (0x00446e99 skips the light transform,
            // 0x00447043 skips the accumulation), and CreateTmdObjectInternal ORs
            // it into all 32 records of a transparent TMD at 0x00483c43. The PS1
            // shades those polygons normally.
            //
            // Where the knock-out pass exists the model is a lit object with a
            // glass insert - the case in rooms 107/108, whose TMD carries 41
            // gouraud normals that nothing was using - so follow the PS1 and
            // shade it. Doors and the uniformly-blended washes are unaffected:
            // a door's +0x68 is 0 and an all-STP palette gets no twin, so
            // neither reaches here, and bit 2 keeps its original meaning for
            // them - as it does for the tank shell, whose unblended
            // render-state draw means full-bright is the authored look (see the
            // note on renderStateModelActive above).
            if (knockTex != MARNI_NULL_HANDLE && !renderStateModelActive)
                unlit = false;

            // Transform + project + light every vertex
            // (heap-allocate per object; vertex counts are small)
            float* sx = (float*)malloc(sizeof(float) * vtxCount * 4);
            float* sy = sx + vtxCount;
            float* vzArr = sx + vtxCount * 2;   // view-space Z, for the depth sort
            float* cr = (float*)malloc(sizeof(float) * vtxCount * 3);
            float* cg = cr + vtxCount;
            float* cb = cg + vtxCount;
            char* clipped = (char*)malloc(vtxCount);
            if (!sx || !cr || !clipped) {
                free(sx); free(cr); free(clipped);
                continue;
            }

            // First normal (flat-shaded primitives only fill vertex 0)
            float flatN[3] = { vbuf[3], vbuf[4], vbuf[5] };

            for (int v = 0; v < vtxCount; v++) {
                const float* vtx = vbuf + v * 11;
                float x = vtx[0], y = vtx[1], z = vtx[2];
                float vx = M[0] * x + M[4] * y + M[8]  * z + M[12];
                float vy = M[1] * x + M[5] * y + M[9]  * z + M[13];
                float vz = M[2] * x + M[6] * y + M[10] * z + M[14];

                vzArr[v] = vz;
                if (vz < nearZ) {
                    clipped[v] = 1;
                    sx[v] = sy[v] = 0.0f;
                }
                else {
                    clipped[v] = 0;
                    float iz = fg / vz;
                    sx[v] = cx + vx * iz * scaleX;
                    sy[v] = cy - vy * iz * scaleY;
                }

                if (unlit) {
                    // The original's else-branch at 0x00447043: no ambient, no
                    // lights, the vertex colour becomes the primitive colour.
                    // PSXObject_Store writes 1.0 for textured primitives, so a
                    // door renders at full texture brightness every time.
                    //
                    // The death-wound tint (JointSetColorTint) writes the
                    // object entry's colour floats at +0x5C and sets the unlit
                    // bit at +0x80 at runtime; the vertex buffer is built once
                    // at load and never re-synced, so an unlit entry with a
                    // tinted colour reads it here (the wounds on the corpse).
                    // Untinted entries keep 0 there and fall back to the
                    // buffered colour.
                    float tr = *(float*)(e->objData + 0x5C);
                    float tg = *(float*)(e->objData + 0x60);
                    float tb = *(float*)(e->objData + 0x64);
                    if (tr != 0.0f || tg != 0.0f || tb != 0.0f) {
                        cr[v] = tr; cg[v] = tg; cb[v] = tb;
                    } else {
                        cr[v] = vtx[6]; cg[v] = vtx[7]; cb[v] = vtx[8];
                    }
                }
                else {
                    // Lighting: vertices without a normal (flat prims) reuse
                    // the first vertex's normal.
                    const float* n = vtx + 3;
                    if (n[0] == 0.0f && n[1] == 0.0f && n[2] == 0.0f) n = flatN;
                    TmdComputeLight(&g_tmdLight[i], n, M, &cr[v], &cg[v], &cb[v]);
                    cr[v] *= vtx[6]; cg[v] *= vtx[7]; cb[v] *= vtx[8];
                    // The original multiplies the lit colour by the object's
                    // colour scale before the 0..255 clamp: 0x00447169
                    // (`FLD [EDI+0x5c]` then `FIMUL`), 0x0044719d for +0x60 and
                    // 0x004471b7 for +0x64. Only TWO of this function's SIX
                    // reads of +0x5C sit behind the `TEST [EDI+0x80],2` unlit
                    // gate at 0x00446e99 / 0x00447043 - this one does not.
                    //
                    // Without it a LIT model can never be tinted, because the
                    // only other reader is the unlit branch below. That is why
                    // the monster plant's poison pulse accumulated a correct
                    // (2,-3,0) into all 15 joint objects of all six plants and
                    // nothing changed on screen. Untinted objects hold 1.0f
                    // here, so this is a no-op for them.
                    cr[v] *= objScaleR; cg[v] *= objScaleG; cb[v] *= objScaleB;
                }
            }

            // Emit triangles
            for (int pIdx = 0; pIdx < listCount; pIdx++) {
                WORD idx[4];
                int vertsInPrim = (primType == 3) ? 3 : 4;
                const WORD* ip = ibuf + pIdx * vertsInPrim;
                idx[0] = ip[0]; idx[1] = ip[1]; idx[2] = ip[2];
                if (vertsInPrim == 4) idx[3] = ip[3];

                bool oob = false;
                for (int v = 0; v < vertsInPrim; v++)
                    if (idx[v] >= vtxCount) { oob = true; break; }
                if (oob) continue;

                // Both the near-clip and the backface test are decided ONCE per
                // primitive and gate all of its triangles, exactly as the
                // original does - a quad never loses only one of its halves.
                //
                // Near clip: the original's guard is
                // `clip[a] + clip[b] + clip[c] (+ clip[d]) == 0`, i.e. one
                // clipped vertex drops the whole packet.
                int clipSum = 0;
                for (int v = 0; v < vertsInPrim; v++) clipSum += clipped[idx[v]];
                if (clipSum != 0) continue;

                // Backface cull. The original tests
                //     0 < (ya - yb) * (xc - xb) + (yc - yb) * (xb - xa)
                // over the packet's FIRST THREE screen vertices (the condition
                // in front of every AddPrim in FUN_00446e40), which expands
                // term for term to the signed area below - so there is no
                // winding convention left to choose. This port previously drew
                // both sides and leaned on the depth buffer; that is what let
                // an object the camera sits inside paint its far shell over the
                // whole frame, since every surviving triangle there is a back
                // face. The index buffer feeding this is byte-identical to the
                // original's (PSXObject_Store writes quads as base+0,+1,+3,+2),
                // so the vertex order the test sees is the original's too.
                float x0 = sx[idx[0]], y0 = sy[idx[0]];
                float x1 = sx[idx[1]], y1 = sy[idx[1]];
                float x2 = sx[idx[2]], y2 = sy[idx[2]];
                if ((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0) <= 0.0f)
                    continue;

                // Triangulate: (0,1,2) + (0,2,3) for quads
                for (int t = 0; t < vertsInPrim - 2; t++) {
                    WORD i0 = idx[0];
                    WORD i1 = idx[t + 1];
                    WORD i2 = idx[t + 2];

                    x0 = sx[i0]; y0 = sy[i0];
                    x1 = sx[i1]; y1 = sy[i1];
                    x2 = sx[i2]; y2 = sy[i2];

                    if (collected >= TMD_MAX_TRIS_COLLECT) continue;

                    TmdTri* t3 = &g_tmdTris[collected];
                    float* o = t3->v;
                    const float* v0 = vbuf + i0 * 11;
                    const float* v1 = vbuf + i1 * 11;
                    const float* v2 = vbuf + i2 * 11;
                    o[0]  = x0; o[1]  = y0; o[2]  = TmdDepthNdc(vzArr[i0]);
                    o[3]  = vzArr[i0];
                    o[4]  = v0[9];  o[5]  = v0[10];
                    o[6]  = cr[i0]; o[7]  = cg[i0]; o[8]  = cb[i0]; o[9]  = triAlpha;
                    o[10] = x1; o[11] = y1; o[12] = TmdDepthNdc(vzArr[i1]);
                    o[13] = vzArr[i1];
                    o[14] = v1[9];  o[15] = v1[10];
                    o[16] = cr[i1]; o[17] = cg[i1]; o[18] = cb[i1]; o[19] = triAlpha;
                    o[20] = x2; o[21] = y2; o[22] = TmdDepthNdc(vzArr[i2]);
                    o[23] = vzArr[i2];
                    o[24] = v2[9];  o[25] = v2[10];
                    o[26] = cr[i2]; o[27] = cg[i2]; o[28] = cb[i2]; o[29] = triAlpha;
                    t3->depth = (vzArr[i0] + vzArr[i1] + vzArr[i2]) * (1.0f / 3.0f);
                    t3->tex   = tex;
                    t3->alpha = triAlpha;
                    t3->otDepth = e->depth;
#ifdef __EMSCRIPTEN__
                    if(collectKeys)frameKeys[collected]={uint32_t(uintptr_t(e->slot?e->slot:e->objData)),uint32_t(uintptr_t(e->elem)),unsigned(pIdx*2+t)};
#endif
                    g_tmdTriOrder[collected] = collected;
                    collected++;

                    // Second, opaque pass over the same geometry with the STP
                    // texels knocked out (see knockTex above). It carries the
                    // same depth and the same otDepth as its blended copy, so
                    // the sort's `a < b` tail places it immediately after it -
                    // nothing else can fall between two adjacent collection
                    // indices.
                    //
                    // `alpha` is the DEPTH-WRITE class, not the opacity: the VS
                    // takes the opacity from each vertex's own alpha, and the
                    // two are deliberately split here. This copy must inherit
                    // its blended partner's class (`triAlpha`, always < 1 on
                    // this path) and write NO depth. It is a colour-only overlay
                    // of geometry that is already in the buffer at the same Z -
                    // giving it a depth footprint would let a model that never
                    // wrote depth start occluding whatever the painter walk
                    // reaches later, and the interior of a case this size is
                    // exactly where a mis-ordered triangle would then punch a
                    // hole instead of just painting in the wrong order.
                    if (knockTex != MARNI_NULL_HANDLE &&
                        collected < TMD_MAX_TRIS_COLLECT) {
                        TmdTri* k3 = &g_tmdTris[collected];
                        memcpy(k3->v, t3->v, sizeof(k3->v));
                        for (int v = 0; v < 3; v++)
                            k3->v[v * TMD_VERT_FLOATS + 9] = 1.0f;
                        k3->depth   = t3->depth;
                        k3->tex     = (DWORD)knockTex;
                        k3->alpha   = triAlpha;
                        k3->otDepth = e->depth;
#ifdef __EMSCRIPTEN__
                        if(collectKeys)frameKeys[collected]={uint32_t(uintptr_t(e->slot?e->slot:e->objData)),uint32_t(uintptr_t(e->elem)),unsigned(pIdx*2+t)};
#endif
                        g_tmdTriOrder[collected] = collected;
                        collected++;
                    }
                }
            }

            free(sx); free(cr); free(clipped);
        }

        // The depth buffer resolves which face wins, so the sort is no longer
        // load-bearing for opaque geometry - it stays because it keeps the
        // alpha-blended triangles blending back-to-front and it groups runs of
        // one texture together, which halves the draw calls.
        // Ties on `depth` are not a curiosity - they are how the original draws
        // per-texel semi-transparency. It cannot blend individual texels, so it
        // queues the SAME geometry twice at two ordering-table depths: the whole
        // model at its blend weight, and a second, opaque copy whose texture has
        // every STP-flagged palette entry punched out to index 0. Room 20A0's
        // water tank is the room-object case (FUN_00484d90 / FUN_00484e40 /
        // FUN_00485000, the only site in the game); the item examine screen is
        // the other. Both passes project through the same matrix, so every
        // triangle's mean view Z is bit-identical and the primary key cannot
        // separate them - which left introsort's partitioning to decide which of
        // the two blended first.
        //
        // The ordering table is what decides it in the original: it is walked
        // from the highest index down to 0, so the LARGER depth draws first. The
        // tank's translucent pass sits at OT 469 and its opaque pass at OT 117 -
        // wash first, opaque over the top, punched texels keeping the wash. The
        // collection index is the last resort, so the whole order is
        // deterministic frame to frame.
        std::sort(g_tmdTriOrder, g_tmdTriOrder + collected, [](int a, int b) {
            const TmdTri& ta = g_tmdTris[a];
            const TmdTri& tb = g_tmdTris[b];
            if (ta.depth != tb.depth)     return ta.depth > tb.depth;
            if (ta.otDepth != tb.otDepth) return ta.otDepth > tb.otDepth;
            return a < b;
        });

        static float triVerts[TMD_MAX_TRIS_FLUSH * TMD_TRI_FLOATS];
        int   triCount = 0;
        DWORD triTex   = 0;
        bool  triWrite = true;   // depth-WRITE mode of the batch in flight
#ifdef __EMSCRIPTEN__
        MarniTriangleKey batchKeys[TMD_MAX_TRIS_FLUSH];
#endif
        const auto flushTriangles=[&](){
#ifdef __EMSCRIPTEN__
            if(collectKeys){
                Marni_DX()->DrawTriangles3DWithKeys(triVerts,triCount,triTex,
                    MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA,triWrite,batchKeys);
                return;
            }
#endif
            Marni_DX()->DrawTriangles3D(triVerts,triCount,triTex,
                MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA,triWrite);
        };

        for (int k = 0; k < collected; k++) {
            const TmdTri* t3 = &g_tmdTris[g_tmdTriOrder[k]];
            const bool triWriteThis = t3->alpha >= 0.999f;

            // Every scene sprite farther than this triangle has to be on
            // screen before it. The batch in flight is behind them too, so it
            // goes down first.
            while (maskIdx < maskCount && (float)maskDepths[maskIdx] > t3->depth) {
                if (triCount > 0) {
                    flushTriangles();
                    triCount = 0;
                }
                FlushSpriteCommandsRange(maskDepths[maskIdx], maskCursor, SPRITE_CLASS_SCENE);
                maskCursor = maskDepths[maskIdx];
                maskIdx++;
            }

            if ((triCount > 0 && (t3->tex != triTex || triWriteThis != triWrite)) ||
                triCount >= TMD_MAX_TRIS_FLUSH) {
                flushTriangles();
                triCount = 0;
            }
            triTex = t3->tex;
            triWrite = triWriteThis;
#ifdef __EMSCRIPTEN__
            if(collectKeys)batchKeys[triCount]=frameKeys[g_tmdTriOrder[k]];
#endif
            memcpy(triVerts + triCount * TMD_TRI_FLOATS, t3->v, sizeof(t3->v));
            triCount++;
        }
        if (triCount > 0) {
            flushTriangles();
        }
    }

    // The scene sprites nearer than the last triangle - and, when no entity was
    // queued at all, every one of them. Nothing else draws SPRITE_CLASS_SCENE,
    // so this drain is what guarantees they are not silently dropped.
    if (maskIdx < maskCount) {
        FlushSpriteCommandsRange(0, maskCursor, SPRITE_CLASS_SCENE);
    }

    g_tmdQueueCount = 0;
}

// (0x00481660) - Update entity lighting from RDT point lights
// Recomputes the 3 D3D lights based on the entity's distance to each RDT
// light. Lights with lightType == 0 are point lights with radial falloff
// (direction = light->entity, color attenuated by distance); the others are
// used as-is (directional).
void update_entity_lighting(VECTOR* entityPos)
{
    if (g_RdtPointer == NULL) return;

    for (int i = 0; i < 3; i++) {
        RDT_Light* light = &g_RdtPointer->lights[i];
        if (light->lightType == 0) {
            struct { int x, y, z; unsigned char r, g, b; } pointLight;
            pointLight.x = entityPos->x - light->pos_x;
            pointLight.y = entityPos->y - light->pos_y;
            pointLight.z = entityPos->z - light->pos_z;

            int atten = (int)(unsigned short)light->radius -
                        SquareRoot0(pointLight.z * pointLight.z +
                                    pointLight.x * pointLight.x);
            if (atten < 0) atten = 0;

            if ((unsigned short)light->radius == 0) {
                pointLight.r = pointLight.g = pointLight.b = 0;
            }
            else {
                pointLight.r = (unsigned char)((light->red   * atten) / (int)(unsigned short)light->radius);
                pointLight.g = (unsigned char)((light->green * atten) / (int)(unsigned short)light->radius);
                pointLight.b = (unsigned char)((light->blue  * atten) / (int)(unsigned short)light->radius);
            }
            FUN_0040ac80(i, &pointLight);
        }
        else {
            FUN_0040ac80(i, light);
        }
    }
}

// (0x0048c350) - render_entity: the in-game character renderer.
// Ghidra called this calc_entity_lighting, but the lighting maths lives in
// update_entity_lighting (0x00481660), which this calls once for the entity
// (and again per joint for types 0x0D/0x12) before drawing. The body is a
// per-joint loop: composes the camera matrix with the joint's world matrix,
// sets the light/rot matrices, and queues the joint's TMD object. Joints
// flagged 0x20 go to the path-trail step instead, flag 4 to the severed-limb
// ballistic step, and flags 0x74 render only inside the camera switch zone.
// Skipped for entity types 0x0D/0x12 with sub-type 1 (they render elsewhere).
// The menu-side twin is options_render_entity (0x004775b0).
void render_entity(Entity* ent)
{
    int param_1 = (int)ent;
    unsigned char* entBytes = (unsigned char*)ENTITY;

    if (((entBytes[1] == 0x0D) || (entBytes[1] == 0x12)) && (entBytes[2] == 1)) {
        return;
    }

    g_animFrameIdSave = (unsigned int)((*(unsigned char*)(param_1 + 3) & 0x7f) == 0);

    unsigned char jointIdx = *(char*)(param_1 + 0x8d) - 1;
    MATRIX* pJoint = (MATRIX*)((unsigned int)jointIdx * 0x7c + *(int*)(param_1 + 0x98));

    update_entity_lighting((VECTOR*)(param_1 + 0x34));

    do {
        short jointFlags = pJoint->m[0][0];

        if ((entBytes[1] == 0x0D) || (entBytes[1] == 0x12)) {
            update_entity_lighting((VECTOR*)(pJoint[2].t + 1));
        }

        if ((jointFlags & 4) != 0) {
            g_svecScratch.x = 0;
            g_svecScratch.z = 0;
            g_svecScratch.y = 0x1e;
            pJoint->m[0][2] = -0x14;
            pJoint->m[1][1] = 0;
            pJoint->m[1][0] = 200;
            FUN_004896c0(pJoint, (short)0xffdd, (short)0xff9c, 1);
        }

        if ((jointFlags & 1) == 0) {
            if ((jointFlags & 0x20) != 0) {
                FUN_0048a210(pJoint);
            }
        }
        else {
            MATRIX localMatrix;
            ApplyLVAndMul0Matrix(&g_RoomCameraData, pJoint[2].m[0] + 2, &localMatrix);

            // Copy g_lightMatrix to g_matrixScratch
            MATRIX* src = &g_lightMatrix;
            MATRIX* dst = &g_matrixScratch;
            for (int i = 8; i != 0; i--) {
                *(unsigned int*)dst->m[0] = *(unsigned int*)src->m[0];
                src = (MATRIX*)(src->m[0] + 2);
                dst = (MATRIX*)(dst->m[0] + 2);
            }

            if (g_animFrameIdSave == 0) {
                if ((jointFlags & 0x74) != 0) goto checkSwitchZone;
doRender:
                if (((entBytes[1] != 18) || (get_stage_id() != STAGE_MANSION_RETURN_2F)) ||
                    ((g_roomId != ROOM_LESSON_ROOM) || (g_roomCameraId != 3))) {
                    g_entityJointPosX = pJoint->t[0];
                    SetLightMatrix(&g_matrixScratch);
                    SetRotAndTransMatrix(&localMatrix);
                    FUN_00483250(0, 0, 0, pJoint->t[1], 0, 4,
                        (BYTE*)&g_spriteAnimSlots[2] + (unsigned int)g_spriteAnimActive * 0x14);
                }
            }
            else if ((jointFlags & 0x74) != 0) {
checkSwitchZone:
                if (is_entity_in_switch_zone((VECTOR*)(pJoint[2].t + 1), g_CurrentRdtDataTypePtr) != 0) {
                    goto doRender;
                }
            }
        }

        pJoint = (MATRIX*)(pJoint[-4].m[0] + 2);
        bool done = (jointIdx == 0);
        jointIdx--;
        if (!done) continue;
        return;
    } while (true);
}


// (0x00483250) - Entity sprite rendering helper
// Forwards joint sprite data and depth shift to the TMD renderer.
// NOTE: 0x00483230 (tmd_render_object_cb) is a byte-identical twin of this
// thunk used by tyrant_draw_heart / FUN_00429d50 / FUN_00469d20 call sites;
// the port serves both from this single implementation.
void FUN_00483250(int p0, int p1, int p2, int p3, int p4, int p5, void* p6)
{
    // Assembly: MOV EAX,[ESP+0x18]; MOV ECX,[ESP+0x10]; PUSH EAX; PUSH ECX; CALL FUN_00483080
    // (also covers 0x00483230)
    FUN_00483080((void*)p3, p5);
}

// (0x0048cc50) - Build view matrix from eye/target positions
static unsigned int FUN_0048cc50(float* eyeTarget, float* eyePos, float* outMatrix)
{
    float dx = eyePos[0] - eyeTarget[0];
    float dy = eyePos[1] - eyeTarget[1];
    float dz = eyePos[2] - eyeTarget[2];
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len == 0.0f) len = 1.0f;
    float invLen = 1.0f / len;
    float ny = -(dy * invLen);
    float horiz = sqrtf(1.0f - ny * ny);
    float nx, nz;
    if (horiz == 0.0f) {
        nx = 0.0f;
        nz = 1.0f;
    } else {
        nx = -((dx * invLen) / horiz);
        nz = (dz * invLen) / horiz;
    }
    outMatrix[0] = nz;      outMatrix[4] = 0.0f;  outMatrix[8]  = nx;
    outMatrix[1] = -(nx * ny); outMatrix[5] = horiz; outMatrix[9]  = nz * ny;
    outMatrix[2] = -(nx * horiz); outMatrix[6] = -ny; outMatrix[10] = nz * horiz;
    outMatrix[12] = outMatrix[0] * dx + outMatrix[8] * dz;
    outMatrix[13] = outMatrix[1] * dx + outMatrix[5] * dy + outMatrix[9] * dz;
    outMatrix[14] = outMatrix[2] * dx + outMatrix[6] * dy + outMatrix[10] * dz;
    outMatrix[3] = 0.0f; outMatrix[7] = 0.0f; outMatrix[11] = 0.0f; outMatrix[15] = 1.0f;
    return 1;
}

// (0x0048c730) - 4x4 matrix multiply (rotation part only, 3x3)
static void FUN_0048c730(float* a, float* b, float* out)
{
    out[0]  = a[0]*b[0] + a[1]*b[4] + a[2]*b[8];
    out[1]  = a[0]*b[1] + a[1]*b[5] + a[2]*b[9];
    out[2]  = a[0]*b[2] + a[1]*b[6] + a[2]*b[10];
    out[4]  = a[4]*b[0] + a[5]*b[4] + a[6]*b[8];
    out[5]  = a[4]*b[1] + a[5]*b[5] + a[6]*b[9];
    out[6]  = a[4]*b[2] + a[5]*b[6] + a[6]*b[10];
    out[8]  = a[8]*b[0] + a[9]*b[4] + a[10]*b[8];
    out[9]  = a[8]*b[1] + a[9]*b[5] + a[10]*b[9];
    out[10] = a[8]*b[2] + a[9]*b[6] + a[10]*b[10];
}

// (0x0048c820) - Transform translation vector by rotation matrix
static void FUN_0048c820(float* translation, float* rotMatrix)
{
    float x = translation[0], y = translation[1], z = translation[2];
    translation[0] = rotMatrix[0]*x + rotMatrix[4]*y + rotMatrix[8]*z;
    translation[1] = rotMatrix[1]*x + rotMatrix[5]*y + rotMatrix[9]*z;
    translation[2] = rotMatrix[2]*x + rotMatrix[6]*y + rotMatrix[10]*z;
}

// (0x00486190) - Camera/projection matrix setup
// Builds view matrix from camera parameters and composites with the model matrix.
// The original builds the two input vectors as
//   from = (0, 0, -g_sceneRenderParam)
//   to   = (0xA0 - subpixelX, subpixelY - 0x78, 0)
// so the direction handed to FUN_0048cc50 (to - from) has a POSITIVE Z of
// g_sceneRenderParam. An earlier revision folded -g_sceneRenderParam into `to`
// and left `from` at the origin, which negated the Z axis (an extra 180 degree
// yaw) and only happened to look right while the subpixel offset was exactly
// the screen centre.
static void FUN_00486190(float* modelMatrix)
{
    float from[3];
    from[0] = 0.0f;
    from[1] = 0.0f;
    from[2] = (float)-g_sceneRenderParam;

    float to[3];
    to[0] = (float)(0xA0 - g_SubpixelOffsetX);
    to[1] = (float)(g_SubpixelOffsetY + (-0x78));
    to[2] = 0.0f;

    float viewMatrix[16];
    FUN_0048cc50(from, to, viewMatrix);
    FUN_0048c730(modelMatrix, viewMatrix, modelMatrix);
    FUN_0048c820(modelMatrix + 12, viewMatrix);
}

// ============================================================================
// FUN_00486df0 (0x00486df0) - draw the "complex" object pool.
//
// ComplexTmdObjectSetup (0x00486990) explodes one TMD into up to 256 standalone
// textured-triangle objects: geometry into the 0x38-stride element array at
// g_objectListPtrArray, per-object render state into the 0x84-stride array at
// g_complexTmdObjectData. FUN_00483080 routes any animation object it has
// processed (spriteData[4] == 1) here instead of through the normal
// AsyncCreateTmdObject path.
//
// This was an empty stub in EngineStubs.cpp, so everything built through that
// path drew nothing at all - visibly, Plant 42's curtain of hanging tendrils in
// room 40C0 (the plant's 3D limbs come through the ordinary path and did show).
//
// Unlike FUN_00483080 the original does NOT fold the FUN_00486190 view rotation
// into these matrices; the raw GTE rotation/translation is written straight into
// each object entry. That is deliberate and is preserved here.
// ============================================================================
void FUN_00486df0(void* spriteData)
{
    if (g_objectListCleanupFlag != 1) return;
    if (spriteData == NULL || ((int*)spriteData)[4] != 1) return;

    // Base ordering-table depth: GTE t[2] / 4, clamped to 3000.
    int t2 = g_gteRotTransMatrix.t[2];
    int baseDepth = (t2 + (t2 >> 31 & 3)) >> 2;
    if (baseDepth >= 3000) baseDepth = 3000;

    const int count = g_objectListCleanupCount;
    if (count <= 0) return;

    // Depth-key every object from the face centroid ComplexTmdObjectSetup
    // stashed for it (0x008fb8b0, one SVECTOR each).
    const int maxFaces = (int)(sizeof(g_faceNormalBuffer) / 8);
    const int keyed = (count < maxFaces) ? count : maxFaces;
    for (int i = 0; i < keyed; i++) {
        SVECTOR out;
        ApplyMatrixSV(&g_gteRotTransMatrix, (SVECTOR*)(g_faceNormalBuffer + i * 8), &out);
        g_complexTmdObjectIds[i]   = i;
        g_complexTmdObjectArray[i] = (int)out.z;
    }
    for (int i = keyed; i < count; i++) {
        g_complexTmdObjectIds[i]   = i;
        g_complexTmdObjectArray[i] = 0;
    }

    // Selection sort, near to far (the original's nested loop, verbatim).
    for (int i = 0; i < count - 1; i++) {
        for (int j = i; j < count; j++) {
            if (g_complexTmdObjectArray[j] < g_complexTmdObjectArray[i]) {
                int d = g_complexTmdObjectArray[i];
                g_complexTmdObjectArray[i] = g_complexTmdObjectArray[j];
                g_complexTmdObjectArray[j] = d;
                int id = g_complexTmdObjectIds[i];
                g_complexTmdObjectIds[i] = g_complexTmdObjectIds[j];
                g_complexTmdObjectIds[j] = id;
            }
        }
    }

    const float scale = 0.00024414063f;  // 1/4096
    for (int i = 0; i < count; i++) {
        int id = g_complexTmdObjectIds[i];
        if (id < 0 || id >= 256) continue;

        BYTE*  entry = g_complexTmdObjectData + id * 0x84;
        float* M     = (float*)(entry + 0x08);

        M[0]  = (float)g_gteRotTransMatrix.m[0][0] * scale;
        M[4]  = (float)g_gteRotTransMatrix.m[0][1] * scale;
        M[8]  = (float)g_gteRotTransMatrix.m[0][2] * scale;
        M[1]  = (float)g_gteRotTransMatrix.m[1][0] * scale;
        M[5]  = (float)g_gteRotTransMatrix.m[1][1] * scale;
        M[9]  = (float)g_gteRotTransMatrix.m[1][2] * scale;
        M[2]  = (float)g_gteRotTransMatrix.m[2][0] * scale;
        M[6]  = (float)g_gteRotTransMatrix.m[2][1] * scale;
        M[10] = (float)g_gteRotTransMatrix.m[2][2] * scale;
        M[12] = (float)g_gteRotTransMatrix.t[0];
        M[13] = (float)g_gteRotTransMatrix.t[1];
        // Each object is nudged one unit further back than the previous one so
        // the sorted order survives into the ordering table.
        M[14] = (float)i + (float)g_gteRotTransMatrix.t[2];
        M[3]  = 0.0f;
        M[7]  = 0.0f;
        M[11] = 0.0f;
        M[15] = 1.0f;

        // +0x54 is the object's D3D handle; zero means it was never created.
        if (*(DWORD*)(entry + 0x54) == 0) continue;

        TmdQueueComplexObject(entry, &g_objectListPtrArray[id * 0x0E], baseDepth + i);
    }
}

// (0x00482fa0) - Copy light data to TMD render object and insert into ordering table
static void FUN_00482fa0(void* spriteData, int depthShift)
{
    if (spriteData == NULL || g_gteRotTransMatrix.t[2] < 0) return;

    int depth = g_gteRotTransMatrix.t[2] >> (depthShift & 0x1F);
    int* data = (int*)spriteData;
    float* lightDst = (float*)((unsigned char*)data + 0x24);
    DWORD* pLight = g_d3dLightData;

    for (int i = 0; i < 3; i++) {
        memcpy(lightDst, pLight, 12 * sizeof(DWORD));
        if (data[6] != 0) {
            lightDst[6] = (float)((data[6] & 0xFF0000) >> 16);
            lightDst[7] = (float)((data[6] >> 8) & 0xFF);
            lightDst[8] = (float)(data[6] & 0xFF);
        }
        OT_InsertPrimitive(lightDst, depth);
        pLight += 12;
        lightDst += 12;
    }
}

// (0x00483080) - Main TMD entity render function
// Reads GTE state buffers, creates TMD object, builds transform matrix, renders
void FUN_00483080(void* spriteData, int depthShift)
{
    int depthField = g_gteRotTransMatrix.t[2];
    if (spriteData == NULL || depthField < 0) {
        return;
    }

    int depth = depthField >> (depthShift & 0x1F);
    int* data = (int*)spriteData;

    FUN_00482fa0(spriteData, depthShift);

    // data[1] is the minimum CLUT depth of the animation slot (FindMinClutDepth)
    // and doubles as the texture bank id; zero means the object carries no
    // textured primitives and is not rendered.
    if (data[1] == 0) {
        return;
    }

    if (data[4] == 1) {
        FUN_00486df0(spriteData);
        return;
    }

    unsigned int tmdObj = AsyncCreateTmdObject(data[1], data[0], (unsigned int)spriteData);
    data[8] = tmdObj;
    if (tmdObj == 0) {
        return;
    }

    // Build 4x4 transform matrix from GTE rotation/translation buffer.
    // Column layout, matching the original store order at 0x004830ef:
    // GTE row 0 (m[0][0..2]) lands in M[0], M[4], M[8], so the consumer reads
    // a transformed X as M[0]*x + M[4]*y + M[8]*z. An earlier revision wrote
    // m[0][1] to M[1] etc., i.e. the transposed (inverse) rotation.
    float transformMatrix[16];
    float scale = 0.00024414063f; // 1/4096

    transformMatrix[0]  = (float)g_gteRotTransMatrix.m[0][0] * scale;
    transformMatrix[4]  = (float)g_gteRotTransMatrix.m[0][1] * scale;
    transformMatrix[8]  = (float)g_gteRotTransMatrix.m[0][2] * scale;
    transformMatrix[1]  = (float)g_gteRotTransMatrix.m[1][0] * scale;
    transformMatrix[5]  = (float)g_gteRotTransMatrix.m[1][1] * scale;
    transformMatrix[9]  = (float)g_gteRotTransMatrix.m[1][2] * scale;
    transformMatrix[2]  = (float)g_gteRotTransMatrix.m[2][0] * scale;
    transformMatrix[6]  = (float)g_gteRotTransMatrix.m[2][1] * scale;
    transformMatrix[10] = (float)g_gteRotTransMatrix.m[2][2] * scale;
    transformMatrix[12] = (float)g_gteRotTransMatrix.t[0];
    transformMatrix[13] = (float)g_gteRotTransMatrix.t[1];
    transformMatrix[14] = (float)depthField;
    transformMatrix[3]  = 0.0f;
    transformMatrix[7]  = 0.0f;
    transformMatrix[11] = 0.0f;
    transformMatrix[15] = 1.0f;

    FUN_00486190(transformMatrix);

    // Call CMarniDirect3DTMD::Transform(ctx, depth, matrix, doubleBuffer=0)
    // Original: Direct3DTMD_Transform(g_pMarniDirect3D, iVar2, &local_40, 0)
    // (ECX = [spriteData+0x20] = the TMD object handle; depth is the OT depth,
    // NOT the matrix — an earlier revision passed the matrix as arg 2, which
    // left every object with a garbage depth and no stored transform).
    CMarniDirect3DTMD* tmd = (CMarniDirect3DTMD*)(void*)tmdObj;
    tmd->Transform(g_pMarniDirect3D, (void*)(size_t)depth, transformMatrix, 0);
}

// ============================================================================
// Room item / 3D object per-frame rendering (0x00473ff0 -> 0x004745f0 ->
// 0x00483270). This is the per-frame pass the original runs between the player
// update and the entity render in game_loop; the port had it as an empty stub
// in EngineStubs.cpp, so the RDT's item models (g_omodel_table) and
// obstacle models (g_item_model_table) were never queued and FlushTmdObjects
// only ever drew entities.
//
// Item/model record layout (0xA4 bytes, one per RDT model slot):
//   +0x00  flags byte (bit 0 = visible, bit 7 = coarse depth shift 10)
//   +0x01  model type id (low 6 bits)
//   +0x04  pointer to the +0x88 sub-record
//   +0x0C  anim field: [0] = 0x40000000 flags, [1] = ScaMatrixData ptr,
//          [2] = AnimSlot ptr (written by SetAnimSlot via FUN_00473ea0),
//          [3] = spriteData ptr (written by CreateAnimObject)
//   +0x1C  ScaMatrixData (field_00 = "world recomputed" dirty flag)
//   +0x20  rotation MATRIX (rebuilt from the +0x72 SVECTOR every frame)
//   +0x34  position VECTOR (doubles as the matrix translation)
//   +0x54  position used for the camera switch-zone cull
//   +0x72  rotation SVECTOR (RotMatrix input)
// ============================================================================

// (0x00483270) - Render one room object into the TMD queue
// objPtr points at the record's anim field (+0x0C); the spriteData block hangs
// off its +0x0C (record +0x18) where CreateAnimObject stored it. Reads the GTE
// rotation/translation buffer (set by SetRotAndTransMatrix in the caller) for
// the transform, exactly like the entity render FUN_00483080. Depth is normally
// GTE t[2] >> shift; DAT_00ae9ef8 / DAT_00ae9ee4 pin it to the fixed 0x32/0x33
// ordering-table slot for the rooms that need it.
static void FUN_00483270(unsigned char* objPtr, int depthShift)
{
    // spriteData = *(record + 0x18): the animation object CreateAnimObject
    // built when the SCD command bound the TMD (FUN_00473ea0).
    int* spriteData = *(int**)(objPtr + 0xc);

    if (spriteData == NULL) return;

    int depth = (DAT_00ae9ee4 != 0) ? 0x33 : 0x32;
    if (DAT_00ae9ef8 == 0) {
        int depthField = g_gteRotTransMatrix.t[2];
        if (depthField < 0) return;
        depth = depthField >> (depthShift & 0x1F);

    }

    // 0x004832cc-0x00483358: per-light colour override records copied into
    // spriteData+0x24 and OT_InsertPrimitive'd at `depth`. The DX11 port's
    // ordering table only consumes depth 0xFFF (the background) and the flush
    // latches the live g_d3dLightData at queue time (TmdQueueObject), so the
    // copy is a no-op here — same call as FUN_00482fa0 in the entity path.

    // data[1] is the minimum CLUT depth (texture bank id); zero means the
    // object carries no textured primitives and is not rendered.
    if (spriteData[1] == 0) return;

    if (spriteData[4] == 1) {
        FUN_00486df0(spriteData);
        return;
    }

    unsigned int tmdObj = AsyncCreateTmdObject(spriteData[1], spriteData[0], (unsigned int)spriteData);
    spriteData[8] = (int)tmdObj;
    if (tmdObj == 0) return;

    // Build the transform from the GTE buffer — same layout as FUN_00483080.
    float m[16];
    float scale = 0.00024414063f; // 1/4096
    m[0]  = (float)g_gteRotTransMatrix.m[0][0] * scale;
    m[4]  = (float)g_gteRotTransMatrix.m[0][1] * scale;
    m[8]  = (float)g_gteRotTransMatrix.m[0][2] * scale;
    m[1]  = (float)g_gteRotTransMatrix.m[1][0] * scale;
    m[5]  = (float)g_gteRotTransMatrix.m[1][1] * scale;
    m[9]  = (float)g_gteRotTransMatrix.m[1][2] * scale;
    m[2]  = (float)g_gteRotTransMatrix.m[2][0] * scale;
    m[6]  = (float)g_gteRotTransMatrix.m[2][1] * scale;
    m[10] = (float)g_gteRotTransMatrix.m[2][2] * scale;
    m[12] = (float)g_gteRotTransMatrix.t[0];
    m[13] = (float)g_gteRotTransMatrix.t[1];
    m[14] = (float)g_gteRotTransMatrix.t[2];
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;

    FUN_00486190(m);

    CMarniDirect3DTMD* tmd = (CMarniDirect3DTMD*)(void*)tmdObj;
    tmd->Transform(g_pMarniDirect3D, (void*)(size_t)depth, m, 0);

    // 0x004834d2: copy the anim object's live blend weight (spriteData[5] ==
    // spriteData + 0x14, what scd_model_tint_apply -> TmdObjectSetLightScale
    // (0x004870a0) writes) into the +0x68/+0x78 blend-weight fields of all 31
    // records of the first object buffer, every frame. Without this the flush's
    // `bgWeight` read (e->objData + 0x68) keeps the value CreateTmdObjectInternal
    // stamped at creation (0 for an opaque model), so any runtime alpha change -
    // room 20B's lighter-lit map fade over the 2F map - never left the object
    // solid. The loop runs i = 0x84..0xFF8 (ESI += 0x84, ESI < 0x1080), which
    // lands on record k's +0x68/+0x78 for k = 0..30 of m_objectData.
    for (int rec = 0; rec < 31; rec++) {
        *(int*)((unsigned char*)tmdObj + 0x4D0 + rec * 0x84 + 0x68) = spriteData[5];
        *(int*)((unsigned char*)tmdObj + 0x4D0 + rec * 0x84 + 0x78) = spriteData[5];
    }
}

// (0x00484eb0) 
// FUN_00486190 view rotation — the original transforms the raw GTE matrix.
static void FUN_00484eb0(void)
{
    int depth = g_gteRotTransMatrix.t[2] >> 6;
    if (depth < 0) depth = 0;
    if (depth > 0xffa) depth = 0xffa;

    float m[16];
    float scale = 0.00024414063f; // 1/4096
    m[0]  = (float)g_gteRotTransMatrix.m[0][0] * scale;
    m[4]  = (float)g_gteRotTransMatrix.m[0][1] * scale;
    m[8]  = (float)g_gteRotTransMatrix.m[0][2] * scale;
    m[1]  = (float)g_gteRotTransMatrix.m[1][0] * scale;
    m[5]  = (float)g_gteRotTransMatrix.m[1][1] * scale;
    m[9]  = (float)g_gteRotTransMatrix.m[1][2] * scale;
    m[2]  = (float)g_gteRotTransMatrix.m[2][0] * scale;
    m[6]  = (float)g_gteRotTransMatrix.m[2][1] * scale;
    m[10] = (float)g_gteRotTransMatrix.m[2][2] * scale;
    m[12] = (float)g_gteRotTransMatrix.t[0];
    m[13] = (float)g_gteRotTransMatrix.t[1];
    m[14] = (float)g_gteRotTransMatrix.t[2];
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;

    CMarniDirect3DTMD* tmd = (CMarniDirect3DTMD*)g_renderStateTMD;
    tmd->Transform(g_pMarniDirect3D, (void*)(size_t)depth, m, 0);
}

// (0x00485000) - schedule the dining-hall table render asynchronously
static void FUN_00485000(void)
{
    ExecAsync((void*)FUN_00484eb0);
}

// (0x004745f0) - Render one item/model record: compose matrices, set lights,
// cull against the camera switch zones, and queue the object's TMD.
static void RoomObjectRender(unsigned char* obj)
{
    MATRIX localMatrix;

    // 0x004745fd: per-object lighting from its world position (record +0x34,
    // which doubles as the +0x20 rotation matrix's translation).
    update_entity_lighting((VECTOR*)(obj + 0x34));

    // 0x0047460e: compose the ScaMatrixData chain (rooted at record +0x10)
    // into localMatrix, then fold in the camera.
    FUN_00483580(*(int**)(obj + 0x10), &localMatrix);

    // 0x00474624: object light matrix = g_lightMatrix * obj rotation matrix
    MulMatrix0(&g_lightMatrix, (MATRIX*)(obj + 0x20), &g_matrixScratch);
    SetLightMatrix(&g_matrixScratch);

    // 0x0047465f: heliport item models 1-4 (pass 0 only) sit 1000 units
    // further along the view axis — the room's shelf displays.
    if ((g_stageId == STAGE_COURTYARD) && (g_roomId == ROOM_HELIPORT) && (DAT_008f8688 == 0)) {
        int t = obj[1] & 0x3f;
        if (t != 0 && t < 5) {
            localMatrix.t[2] += 1000;
        }
    }

    // 0x0047466e: DAT_00ae9ee4 — the mansion-2F study / front lesson room
    // open-lid model renders at the fixed depth 0x33 instead of 0x32.
    DAT_00ae9ee4 = 0;
    int stageMod = g_stageId % 5; // get mansion absolute index
    if (stageMod == STAGE_MANSION_2F) {
        if ((g_roomId == ROOM_STUDY_2F) && (DAT_008f8688 == 0) && ((obj[1] & 0x3f) == 1)) DAT_00ae9ee4 = 1;
        if ((g_roomId == ROOM_FRONT_LESSON_ROOM) && (DAT_008f8688 == 0) && ((obj[1] & 0x3f) == 1)) DAT_00ae9ee4 = 1;
    }

    // 0x004746d2-0x004747b7: room-specific objects that must not render
    // (mirror/door-frame stand-ins the SCD keeps for interaction but that
    // have their own model elsewhere, e.g. the guardhouse room 002 mirror).
    bool skip = false;
    if ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_002) && (g_roomCameraId == 4) &&
        (DAT_008f8688 == 0) && ((obj[1] & 0x3f) == 0)) {
        skip = true;
    } else if ((g_stageId == STAGE_LABORATORY) && (g_roomId == ROOM_LAB_B3_PRIVATE_ROOM_B) &&
               ((g_roomCameraId == 0) || (g_roomCameraId == 4)) &&
               (DAT_008f8688 == 0) && ((obj[1] & 0x3f) == 0)) {
        skip = true;
    } else if ((stageMod == STAGE_MANSION_1F) && (g_roomId == ROOM_TRAP_ROOM) && (g_roomCameraId == 0) &&
               (DAT_008f8688 == 0) && ((obj[1] & 0x3f) == 0) &&
               (*(int*)(obj + 0x34) == 0x12fc) &&
               (*(int*)(obj + 0x38) == -0x2828) &&
               (*(int*)(obj + 0x3c) == 0x12fc)) {
        skip = true;
    } else if ((stageMod == STAGE_COURTYARD) && (g_roomId == ROOM_BOULDER_2_PASSAGE) && (g_roomCameraId == 3) &&
               (DAT_008f8688 == 0) && ((obj[1] & 0x3f) == 0) &&
               (*(int*)(obj + 0x34) > 0x7274)) {
        // 0x004747ab: CMP dword ptr [EDI], 0x7274 / JG <return>, with EDI at
        // record + 0x34 - the model's world X. The sense is "skip once X passes
        // the threshold", and this port had it inverted, so the object was
        // hidden exactly when it should have drawn and drawn exactly when it
        // should have been hidden.
        //
        // This is the boulder in room 30F0 (g_stageId is 0-based, so the stage-3
        // file name is id 2). Camera 3 sits at x = 32940 looking back down the
        // corridor; 0x7275 = 29301, so the original drops the model as soon as
        // the boulder rolls within ~3600 units of the eye - which is the whole
        // point of the rule, and why it filled the screen here instead.
        //
        // The type is compared against DAT_004c3694 in the original, but that
        // global has exactly one xref (this read) and holds 0, so it is a
        // build-time constant and the literal below is faithful.
        skip = true;
    }
    if (skip) return;

    // 0x004747b7-0x0047488b: DAT_00ae9ef8 — keep the fixed ordering-table
    // depth for flooded rooms instead of sorting by GTE t[2].
    DAT_00ae9ef8 = 0;
    int stageModP1 = (g_stageId + 1) % 5; // get 1-indexed stage id
    if (stageModP1 == GUARDHOUSE) {
        if ((g_roomId == ROOM_WATER_TANK_ENTRY) || (g_roomId == ROOM_SECURITY_ROOM) ||
            (g_roomId == ROOM_WATER_TANK) || (g_roomId == ROOM_ARMS_STOREHOUSE) ||
            (g_roomId == ROOM_CONTROL_ROOM)) {
            DAT_00ae9ef8 = 1;
        }
    }
    if ((stageModP1 == COURTYARD) && (g_roomId == ROOM_WATER_GATE)) DAT_00ae9ef8 = 1;
    if ((stageMod == MANSION_1F) && (g_roomId == ROOM_1F_RIGHT_STAIRS) && (DAT_008f8688 == 0) && ((obj[1] & 0x3f) < 2)) DAT_00ae9ef8 = 1;

    // 0x00474890: push the composed matrix into the GTE rotation/translation
    // buffer — FUN_00483270 reads the transform from there.
    SetRotAndTransMatrix(&localMatrix);

    // 0x004748a2: cull objects outside the current camera's switch-zone group.
    if (is_entity_in_switch_zone((VECTOR*)(obj + 0x54), g_CurrentRdtDataTypePtr) == 0) return;

    // 0x004748d2
    // path (FUN_00485000) with a clamped depth.
    if ((stageModP1 == MANSION_2F) && (g_roomId == ROOM_STUDY_2F) && ((obj[1] & 0x3f) == 0)) {
        FUN_00485000();
    }

    // 0x004748d7: records with bit 7 set use the coarse depth shift (10);
    // everything else uses 4.
    if ((obj[0] & 0x80) != 0) {
        FUN_00483270(obj + 0xc, 10);
    } else {
        FUN_00483270(obj + 0xc, 4);
    }
}

// (0x00473ff0) - render_room_objects
// Per-frame draw of the room's own 3D content - despite the Ghidra name
// (room_camera_and_lighting_update) it touches neither the camera nor the
// lights. Pass 0 walks the omodel records (g_omodel_table, count = RDT
// omodel_slot_count), pass 1 the item models (g_item_model_table, count = RDT
// item_count). Each visible record with a bound model gets its rotation matrix
// rebuilt from its +0x72 SVECTOR, its ScaMatrixData marked dirty for the
// compose, and is handed to RoomObjectRender. Called from game_loop while
// g_dwRoomObjectRenderEnabled is set; was an empty stub, which is why room
// items and 3D objects never appeared.
void render_room_objects(void)
{
    for (int pass = 0; pass < 2; pass++) {
        int count = (pass == 0)
            ? g_RdtPointer->omodel_slot_count
            : g_RdtPointer->item_count;
        DAT_008f8688 = pass;

        for (int i = 0; i < count; i++) {
            unsigned char* obj = (pass == 0)
                ? (unsigned char*)g_omodel_table[i]
                : (unsigned char*)g_item_model_table[i];

            // Visible (bit 0) and with a bound model (AnimSlot ptr at +0x14).
            if ((obj != NULL) && ((*obj & 1) != 0) && (*(int*)(obj + 0x14) != 0)) {
                // 0x0047405b: rebuild the rotation matrix from the record's
                // SVECTOR; 0x00474063: mark the ScaMatrixData dirty so
                // FUN_00483580 recomposes it.
                RotMatrix((SVECTOR*)(obj + 0x72), (MATRIX*)(obj + 0x20));
                *(int*)(obj + 0x1c) = 0;
                RoomObjectRender(obj);
            }
        }
    }
}

// ============================================================================
// Float vector / matrix helpers (original cluster 0x0048c5b0..0x0048cad0).
// Self-contained float math used by the viewport normal recalculation in
// CMarniViewport2::CopyFrom/Convert0 and by entity camera/web code. Angles
// are expressed in "turns" (1/360 of a circle), matching the original's
// atan * (180/pi) / 360 pipeline.
// ============================================================================

// (0x0048c5b0) - Normalize a 3-float vector in place.
void vec3_normalize(float* v)
{
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

// (0x0048c690) - atan-based angle in turns: atan(tangent), negated when
// signRef is negative (the original tests the raw sign bit of the first
// stack argument), scaled by 180/pi then 1/360.
static float angle_atan_turns(float signRef, float tangent)
{
    float deg = atanf(tangent) * 57.29577951f;
    if (signRef < 0.0f) deg = -deg;
    return deg * (1.0f / 360.0f);
}

// (0x0048c6d0) - Angle between two 2D points (x1,y1)-(x2,y2) in turns,
// measured from the Y axis (atan(dx/r) with the dy sign as quadrant fix).
float angle_between_points_turns(int x1, int y1, int x2, int y2)
{
    float dx = (float)(y2 - y1);
    float dz = (float)(x2 - x1);
    float r = sqrtf(dx * dx + dz * dz);
    return angle_atan_turns(dx / r, dz / r);
}

// (0x0048c8f0) - Rotate one matrix row's [1]/[2] float pair by `turns`
// (fraction of a full circle). Row is addressed as {float a; float y; float z;}
// at base+0/+4/+8.
static void rot_row_yz(float turns, float* row)
{
    float y = row[1];
    float angle = turns * 6.2831855f;
    float s = sinf(angle);
    float c = cosf(angle);
    row[1] = s * row[2] + c * y;
    row[2] = c * row[2] - s * y;
}

// (0x0048c950) - Same rotation for the [0]/[2] pair of a float triple.
static void rot_row_xz(float turns, float* row)
{
    float x = row[0];
    float angle = turns * 6.2831855f;
    float s = sinf(angle);
    float c = cosf(angle);
    row[0] = s * row[2] + c * x;
    row[2] = c * row[2] - s * x;
}

// (0x0048ca10) - Rotate the three rows of a 3x4 float matrix about the axis
// handled by rot_row_yz (each row's components 1 and 2).
void matrix_rotate_rows_yz(float turns, float* m)
{
    rot_row_yz(turns, m + 0);   // row 0: elements 0,1,2
    rot_row_yz(turns, m + 4);   // row 1: elements 4,5,6
    rot_row_yz(turns, m + 8);   // row 2: elements 8,9,10
}

// (0x0048cad0) - Rotate the three rows of a 3x4 float matrix about the axis
// handled by rot_row_xz (each row's components 0 and 2).
void matrix_rotate_rows_xz(float turns, float* m)
{
    rot_row_xz(turns, m + 0);
    rot_row_xz(turns, m + 4);
    rot_row_xz(turns, m + 8);
}

