// CollisionDebug.cpp - debug-only overlay that draws the room's RDT collision
// boundaries on top of the pre-rendered background.
//
// Starts off; F8 toggles it at runtime (WindowProc.cpp) while debug features
// are enabled ([Debug] EnableDebug in config.ini). Not part of the original
// game; this is a diagnostic
// for comparing the collision data against the background photo and against
// where the player actually stops. It is drawn between the background and the
// 3D models, so characters occlude it.
//
// What is drawn:
//   green  outline  shape 1/5 rectangle, as stored in the RDT
//   cyan   outline  shape 3 circle, as stored
//   yellow outline  shape 4 soft zone (no push)
//   red    outline  the same record grown by the player's radius + 18 - this is
//                   where the player's CENTRE is stopped
//   blue   circle   the player's collision radius at his current position
//
// Every record in the room is drawn, from all four quadrant lists, deduplicated:
// a record whose box straddles a quadrant split is stored once per quadrant it
// touches, and drawing only the player's own quadrant made geometry pop in and
// out as he crossed a split. Note that at runtime check_room_collision still
// only tests the player's quadrant - the overlay shows the whole room because
// that is what is useful to look at.
#include "../Globals.h"
#include "Types.h"
#include "Entities.h"
#include "SpriteRenderer.h"
#include "../marni/MarniDX.h"
#include "../marni/MarniSystem.h"
#include <cmath>

// Compiled into both configurations; only active while g_debugFeaturesEnabled
// is set (Globals.h). The F8 toggle (WindowProc) and the [Debug]
// ShowCollision/CollisionY config reads (main.cpp) are gated with the same
// flag, and CollisionDebug_Draw early-returns on g_bShowCollisionDebug.

// Set from config.ini by LoadIniConfiguration (main.cpp).
BOOL g_bShowCollisionDebug = FALSE;
int  g_iCollisionDebugY    = 0;    // world Y of the plane the overlay is drawn on

// MarniDX::DrawTriangles takes at most 1024 triangles per call, so the batch is
// flushed as it fills rather than silently dropping the tail.
#define COLL_DBG_MAX_TRIS 1024

static float    s_tris[COLL_DBG_MAX_TRIS * 3 * 8];
static int      s_triCount   = 0;
static int      s_vtxCursor  = 0;   // in floats
static MarniDX* s_dx         = NULL;
static float    s_viewW      = 640.0f;
static float    s_viewH      = 480.0f;

// ---------------------------------------------------------------------------
// Projection.
//
// This builds its own orthonormal basis straight from the RDT camera rather than
// reusing g_RoomCameraData. That matrix's rows live in a Y-FLIPPED frame (which
// is what m[2][1] = -dy/len means) and its translation mixes conventions, so
// feeding raw world points into it puts the overlay at the wrong height in a
// depth-dependent way. Deriving the basis here is both simpler and verifiable:
// the camera's look-at point projects to the screen centre by construction, and
// this is the same construction whose y=0 grid lines up with the pre-rendered
// checkerboard.
//
// It also makes the overlay an INDEPENDENT reference: if the outlines agree with
// the background but the character does not, the fault is in the game's own
// camera path, not in the collision data.
//
//   n = (to - from) / |to - from|        forward
//   r = (dz, 0, -dx) / hypot(dx, dz)     horizontal right
//   u = n x r                            up-ish; u.y > 0 because PS1 -Y is up
//   sx = cx + (r . (p-from)) * f / (n . (p-from))
//   sy = cy + (u . (p-from)) * f / (n . (p-from))
// ---------------------------------------------------------------------------
#define COLL_DBG_NEAR 64.0f

struct CollDbgView {
    float cx, cy, f;
    float fromX, fromY, fromZ;
    float n[3], r[3], u[3];
};

static bool CollDbg_BuildView(CollDbgView& V, float scaleX)
{
    if (g_RdtPointer == NULL) return false;
    if ((unsigned int)g_roomCameraId >= (unsigned int)g_RdtPointer->cameras_count) return false;

    RDT_Camera* cams = (RDT_Camera*)((char*)g_RdtPointer + sizeof(RDT));
    RDT_Camera* C = &cams[g_roomCameraId];

    float dx = (float)(C->cam_to_x - C->cam_from_x);
    float dy = (float)(C->cam_to_y - C->cam_from_y);
    float dz = (float)(C->cam_to_z - C->cam_from_z);

    float L = sqrtf(dx*dx + dy*dy + dz*dz);
    float h = sqrtf(dx*dx + dz*dz);
    if (L < 1.0f || h < 1.0f) return false;   // degenerate / straight down

    V.fromX = (float)C->cam_from_x;
    V.fromY = (float)C->cam_from_y;
    V.fromZ = (float)C->cam_from_z;

    V.n[0] = dx / L;   V.n[1] = dy / L;   V.n[2] = dz / L;
    V.r[0] = dz / h;   V.r[1] = 0.0f;     V.r[2] = -dx / h;
    V.u[0] = V.n[1]*V.r[2] - V.n[2]*V.r[1];
    V.u[1] = V.n[2]*V.r[0] - V.n[0]*V.r[2];
    V.u[2] = V.n[0]*V.r[1] - V.n[1]*V.r[0];

    // The camera's own fov field is what set_scene_render_param loads into
    // g_sceneRenderParam, in 320x240 space.
    V.f  = (float)C->fov * scaleX;
    return true;
}

static float CollDbg_Depth(const CollDbgView& V, float x, float y, float z)
{
    return V.n[0]*(x - V.fromX) + V.n[1]*(y - V.fromY) + V.n[2]*(z - V.fromZ);
}

static void CollDbg_ProjectAt(const CollDbgView& V, float x, float y, float z,
                              float vz, float* outX, float* outY)
{
    float dx = x - V.fromX, dy = y - V.fromY, dz = z - V.fromZ;
    float iz = V.f / vz;
    *outX = V.cx + (V.r[0]*dx + V.r[1]*dy + V.r[2]*dz) * iz;
    // u points down in PS1 coords and screen Y grows down, so this adds.
    *outY = V.cy + (V.u[0]*dx + V.u[1]*dy + V.u[2]*dz) * iz;
}

// ---------------------------------------------------------------------------
// Screen-space line as a quad, appended to the triangle batch.
// ---------------------------------------------------------------------------
static void CollDbg_Flush(void)
{
    if (s_triCount > 0 && s_dx != NULL) {
        s_dx->DrawTriangles(s_tris, s_triCount, MARNI_NULL_HANDLE,
                            MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA);
    }
    s_triCount  = 0;
    s_vtxCursor = 0;
}

static void CollDbg_Vtx(float x, float y, float r, float g, float b)
{
    float* p = &s_tris[s_vtxCursor];
    p[0] = x; p[1] = y;
    p[2] = 0.0f; p[3] = 0.0f;
    p[4] = r; p[5] = g; p[6] = b; p[7] = 1.0f;
    s_vtxCursor += 8;
}

static void CollDbg_Line(float x0, float y0, float x1, float y1,
                         float r, float g, float b, float width)
{
    // Room-spanning records project to enormous screen coordinates. D3D clips
    // them anyway, but rejecting the ones that cannot cross the viewport keeps
    // the batch for the ones that can. Only reject when BOTH endpoints are
    // outside the same edge - that is the only case where no crossing is
    // possible.
    if ((x0 < 0.0f && x1 < 0.0f) || (x0 > s_viewW && x1 > s_viewW) ||
        (y0 < 0.0f && y1 < 0.0f) || (y0 > s_viewH && y1 > s_viewH)) return;

    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.01f) return;

    if (s_triCount + 2 > COLL_DBG_MAX_TRIS) CollDbg_Flush();

    float nx = -dy / len * (width * 0.5f);
    float ny =  dx / len * (width * 0.5f);

    CollDbg_Vtx(x0 + nx, y0 + ny, r, g, b);
    CollDbg_Vtx(x1 + nx, y1 + ny, r, g, b);
    CollDbg_Vtx(x1 - nx, y1 - ny, r, g, b);
    CollDbg_Vtx(x0 + nx, y0 + ny, r, g, b);
    CollDbg_Vtx(x1 - nx, y1 - ny, r, g, b);
    CollDbg_Vtx(x0 - nx, y0 - ny, r, g, b);
    s_triCount += 2;
}

// ---------------------------------------------------------------------------
// A world-space segment on the y = wy plane, clipped to the near plane.
//
// Dropping any segment with an endpoint behind the camera (which is what this
// did before) silently deleted exactly the records the player is most likely to
// be standing against: a room-bounding wall spans the whole room, so one of its
// corners is usually behind the camera. That is why the left wall in one shot
// and the right wall in another were missing even while being collided with.
// Clipping the segment to the near plane keeps the visible part.
// ---------------------------------------------------------------------------
static void CollDbg_WorldLine(const CollDbgView& V, int wy,
                              int x0, int z0, int x1, int z1,
                              float r, float g, float b, float width)
{
    float ax = (float)x0, az = (float)z0;
    float bx = (float)x1, bz = (float)z1;
    float wyf = (float)wy;

    float za = CollDbg_Depth(V, ax, wyf, az);
    float zb = CollDbg_Depth(V, bx, wyf, bz);

    if (za < COLL_DBG_NEAR && zb < COLL_DBG_NEAR) return;   // wholly behind

    if (za < COLL_DBG_NEAR) {
        float t = (COLL_DBG_NEAR - za) / (zb - za);
        ax += (bx - ax) * t;
        az += (bz - az) * t;
        za  = COLL_DBG_NEAR;
    } else if (zb < COLL_DBG_NEAR) {
        float t = (COLL_DBG_NEAR - zb) / (za - zb);
        bx += (ax - bx) * t;
        bz += (az - bz) * t;
        zb  = COLL_DBG_NEAR;
    }

    float sx0, sy0, sx1, sy1;
    CollDbg_ProjectAt(V, ax, wyf, az, za, &sx0, &sy0);
    CollDbg_ProjectAt(V, bx, wyf, bz, zb, &sx1, &sy1);
    CollDbg_Line(sx0, sy0, sx1, sy1, r, g, b, width);
}

// An axis-aligned XZ rectangle on the y = wy plane.
static void CollDbg_WorldRect(const CollDbgView& V, int wy,
                              int xMin, int zMin, int xMax, int zMax,
                              float r, float g, float b, float width)
{
    CollDbg_WorldLine(V, wy, xMin, zMin, xMax, zMin, r, g, b, width);
    CollDbg_WorldLine(V, wy, xMax, zMin, xMax, zMax, r, g, b, width);
    CollDbg_WorldLine(V, wy, xMax, zMax, xMin, zMax, r, g, b, width);
    CollDbg_WorldLine(V, wy, xMin, zMax, xMin, zMin, r, g, b, width);
}

// A circle on the y = wy plane, as a polyline.
static void CollDbg_WorldCircle(const CollDbgView& V, int wy,
                                int cxw, int czw, int radius,
                                float r, float g, float b, float width)
{
    const int SEG = 16;
    int px = 0, pz = 0;
    for (int i = 0; i <= SEG; i++) {
        float t = (float)i * 6.2831853f / (float)SEG;
        int x = cxw + (int)(cosf(t) * (float)radius);
        int z = czw + (int)(sinf(t) * (float)radius);
        if (i > 0) CollDbg_WorldLine(V, wy, px, pz, x, z, r, g, b, width);
        px = x; pz = z;
    }
}

// ---------------------------------------------------------------------------
// A record is stored once per quadrant its box touches, so the same geometry
// appears up to four times in the array. Compare the whole 12-byte record.
// ---------------------------------------------------------------------------
static bool CollDbg_IsDuplicate(RDT_Boundary* rec, RDT_Boundary* first)
{
    for (RDT_Boundary* p = first; p < rec; p++) {
        if (p->xMax == rec->xMax && p->zMax == rec->zMax &&
            p->xMin == rec->xMin && p->zMin == rec->zMin &&
            p->type == rec->type && p->flags == rec->flags) {
            return true;
        }
    }
    return false;
}

// ===========================================================================
// CollisionDebug_Draw - called once per frame from the render flush.
// ===========================================================================
void CollisionDebug_Draw(void)
{
    if (!g_bShowCollisionDebug) return;
    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return;

    s_dx = Marni_DX();
    if (s_dx == NULL) return;

    // Only meaningful once room_set has turned the counts into pointers.
    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    if (hdr->group[0] == NULL) return;

    float scaleX, scaleY;
    MarniGetRenderScale(&scaleX, &scaleY);
    s_viewW = 320.0f * scaleX;
    s_viewH = 240.0f * scaleY;

    CollDbgView V;
    V.cx = (float)g_SubpixelOffsetX * scaleX;
    V.cy = (float)g_SubpixelOffsetY * scaleY;
    if (!CollDbg_BuildView(V, scaleX)) return;

    const int* pt = g_playerEntity.scaMatrixData.localMatrix.t;
    int playerX = pt[0], playerZ = pt[2];

    // Draw on the room's FLOOR plane, not at the entity's Y. localMatrix.t[1] is
    // the character's origin, which sits at his waist (the SCA hit box runs
    // -1530..+1530 around it), so using it floated the whole overlay at chest
    // height. RDT boundaries are 2D in XZ and the floor is y = 0 - projecting a
    // world grid at y = 0 is what lines up with the pre-rendered checkerboard.
    // A room on a raised level can be nudged with [Debug] CollisionY.
    int floorY = g_iCollisionDebugY;

    int radius = 0;
    if (g_playerEntity.Sca_info != 0) {
        radius = *(short*)(g_playerEntity.Sca_info + 10);
    }
    int skin = radius + 0x12;   // what collision_push_rect actually clears

    // Every record in the room: group[0] is the first and group[4] the end of
    // the last quadrant, so this spans the whole array.
    RDT_Boundary* first = hdr->group[0];
    RDT_Boundary* last  = hdr->group[4];

    s_triCount  = 0;
    s_vtxCursor = 0;

    const float LW = 1.5f * scaleX;

    // Pass 1: the grown barriers, so the raw records draw over them, not under.
    for (RDT_Boundary* rec = first; rec < last; rec++) {
        int shape = rec->type & 0xff;
        if (shape == 4) continue;             // soft zone: no push, no barrier
        if (CollDbg_IsDuplicate(rec, first)) continue;
        if (shape == 3) {
            int cr = (rec->xMax - rec->xMin) / 2;
            CollDbg_WorldCircle(V, floorY, rec->xMin + cr, rec->zMin + cr,
                                cr + skin, 1.0f, 0.25f, 0.25f, LW);
        } else {
            CollDbg_WorldRect(V, floorY,
                              rec->xMin - skin, rec->zMin - skin,
                              rec->xMax + skin, rec->zMax + skin,
                              1.0f, 0.25f, 0.25f, LW);
        }
    }

    // Pass 2: the records exactly as stored in the RDT.
    for (RDT_Boundary* rec = first; rec < last; rec++) {
        int shape = rec->type & 0xff;
        if (CollDbg_IsDuplicate(rec, first)) continue;

        float r = 0.2f, g = 1.0f, b = 0.2f;
        if (shape == 3)      { r = 0.2f; g = 1.0f; b = 1.0f; }
        else if (shape == 4) { r = 1.0f; g = 1.0f; b = 0.2f; }

        if (shape == 3) {
            // collision_push_circle takes the circle radius from the X extent
            // alone and the centre from the box centre.
            int cr = (rec->xMax - rec->xMin) / 2;
            CollDbg_WorldCircle(V, floorY, rec->xMin + cr, rec->zMin + cr, cr,
                               r, g, b, LW);
        } else {
            CollDbg_WorldRect(V, floorY, rec->xMin, rec->zMin, rec->xMax, rec->zMax,
                              r, g, b, LW);
        }
    }

    // The player's own collision circle, on the same plane, so the residual gap
    // between him and a barrier is directly readable.
    if (radius > 0) {
        CollDbg_WorldCircle(V, floorY, playerX, playerZ, radius,
                            0.35f, 0.55f, 1.0f, LW);
    }

    CollDbg_Flush();
}
