// RoomCollision.cpp - room boundary collision (decompiled from Ghidra)
//
// The room's collision geometry lives in the RDT at +0x4C (the ".blk" block).
// It is a 0x18-byte header followed by an array of 0x0C-byte boundary records:
//
//   header  short cellX, cellZ                 quadrant split point
//           int   count[5]                     per-quadrant record counts
//   record  short xMax, zMax, xMin, zMin       AABB, MAX corner FIRST
//           u16   type                         low byte = shape index
//           u16   flags
//
// Room_SetupCollisionCallbacks rewrites count[] in place into five absolute
// pointers, so quadrant q afterwards spans [group[q], group[q+1]).
// ChkOutsideCell picks the quadrant from the position's side of (cellX, cellZ);
// a record that straddles the split is duplicated into every quadrant it
// touches, so one quadrant's list is all a point ever needs to test.
//
// NOTE ON THE RECORD LAYOUT: the first coordinate pair is the box MAXIMUM and
// the second is the minimum - not (x, z, width, depth) as the RE2-era notes on
// this format claim. Verified two ways: boundary_point_outside accepts a point
// only when xMin-r <= px <= xMax+r, which is an empty interval for any box
// wider than 2r under the width reading; and across all 320 shipped RDTs every
// record's [pair2 .. pair1] span agrees with the set of quadrants that record
// was duplicated into. Only shapes 1, 3, 4 and 5 occur in the shipped data.
#include "../Globals.h"
#include <cstdlib>                   // abs() - MSVC got this via <windows.h>
#include "Types.h"
#include "Entities.h"

extern int SquareRoot0(int val);

// ---------------------------------------------------------------------------
// The 0x00be0de0-0x00be0df4 block is general-purpose scratch that the original
// shares between the collision push, the zombie AI and the player animations
// (see the declarations in Zombie.cpp / PlayerAnimations.cpp). The push writes
// its intermediate depths there; nothing in the collision path reads them back,
// but the clobber is part of the original's observable behaviour.
// ---------------------------------------------------------------------------
extern int          player_distance_z;   // 0x00be0de4
extern int          g_scaled_down_dist;  // 0x00be0de8
extern unsigned int g_entity_bkp;        // 0x00be0df4

int g_collPushDepthZHi = 0;              // 0x00be0dec
int g_collPushDepthZLo = 0;              // 0x00be0df0

// 0x00ac9c00 - boundary shape handler table, indexed by (record->type & 0xff).
// Slots 1/3/4/5 are installed by Room_SetupCollisionCallbacks; slots 0 and 2
// are never written by the original and never selected by the shipped data.
CollisionShapeHandler g_CollisionShapeHandlers[6] = {};

// ===========================================================================
// ChkOutsideCell (0x0047d270)
// Which of the four boundary quadrants does `position + offset` fall in?
// bit 0 = the point is on the low side of cellX, bit 1 = low side of cellZ.
// ===========================================================================
unsigned int ChkOutsideCell(VECTOR* position, SVECTOR* offset, int cellX, int cellZ)
{
    unsigned int bitX = (unsigned int)((offset->x - cellX) + position->x) >> 31;
    unsigned int bitZ = ((unsigned int)((offset->z + position->z) - cellZ) & 0xbfffffffu) >> 30;
    return bitZ | bitX;
}

// ===========================================================================
// boundary_point_outside (0x0047d2a0)
// Is g_playerPosScratch + *offset outside the box [xLo..xHi] x [zLo..zHi]?
//
// The original folds the four sign bits into bits 28-31 of one word through a
// shift-and-or chain (the low mask bits are shifted back out again by the end);
// only "is the result zero" is ever tested.
// ===========================================================================
static unsigned int boundary_point_outside(SVECTOR* offset,
                                           int xHi, int zHi, int xLo, int zLo)
{
    int ox = offset->x;
    int oz = offset->z;

    unsigned int a = (unsigned int)((g_playerPosScratch.x - xLo) + ox) & 0x80000007u;
    unsigned int b = (unsigned int)((xHi - ox) - g_playerPosScratch.x) & 0x80000003u;
    unsigned int c = (unsigned int)((g_playerPosScratch.z - zLo) + oz) & 0x80000001u;
    unsigned int d = (unsigned int)((zHi - oz) - g_playerPosScratch.z) & 0x80000000u;

    return ((((((a >> 1) | b) >> 1) | c) >> 1) | d);
}

// ===========================================================================
// boundary_classify (0x0047d1b0)
// Test one boundary record against g_playerPosScratch, grown by `radius`.
// Returns the record's shape index when the point is inside it, 0xffff if not.
// ===========================================================================
static unsigned short boundary_classify(SVECTOR* offset, RDT_Boundary* rec,
                                        unsigned int radius)
{
    int r = (int)(radius & 0xffff);

    // The original zero-extends each coordinate from 16 bits, then does the
    // grow/shrink as a signed 32-bit add - xMin - r may legitimately go < 0.
    unsigned int outside = boundary_point_outside(
        offset,
        (int)rec->xMax + r,
        (int)rec->zMax + r,
        (int)rec->xMin - r,
        (int)rec->zMin - r);

    if (outside != 0) return 0xffff;
    return (unsigned short)(rec->type & 0xff);
}

// ===========================================================================
// boundary_classify_flags (0x0047d210)
// Same test as boundary_classify, but returns the record's flag bits
// (flags & 0xff00) in place instead of the shape index.
// ===========================================================================
unsigned short boundary_classify_flags(SVECTOR* offset, RDT_Boundary* rec,
                                       unsigned int radius)
{
    int r = (int)(radius & 0xffff);

    unsigned int outside = boundary_point_outside(
        offset,
        (int)rec->xMax + r,
        (int)rec->zMax + r,
        (int)rec->xMin - r,
        (int)rec->zMin - r);

    if (outside != 0) return 0xffff;
    return (unsigned short)(rec->flags & 0xff00);
}

// ===========================================================================
// collision_flag_set (0x0047e1b0)
// Shape 4: a soft zone. No push - it only raises the "touched a boundary" bit.
// ===========================================================================
static void collision_flag_set(short* bounds, int* pos, short* prevPos)
{
    (void)bounds; (void)pos; (void)prevPos;
    ENTITY->collisionFlags |= 0x08;
}

// ===========================================================================
// collision_push_rect (0x0047df10)
// Shapes 1 and 5: push the entity out of a rectangular obstacle.
//
// Computes the four exit depths (entity radius plus an 18-unit skin), keeps the
// shallower one per axis, then picks the axis to resolve on by comparing the
// sign of this frame's movement against the sign of the push: an axis whose
// push opposes the movement is the face the entity came in through.
// ===========================================================================
static void collision_push_rect(short* bounds, int* pos, short* prevPos)
{
    short radius = *(short*)(ENTITY->Sca_info + 10);

    // The original does all four subtractions in 16 bits, against only the low
    // word of the 32-bit position. `bounds` is signed here even though the
    // coordinates are unsigned (see RDT_Boundary): the adds are modular and the
    // result is truncated to a short, so the two readings give the same answer -
    // which is why the original can get away with 16-bit registers throughout.
    short posX = (short)pos[0];
    short posZ = (short)pos[2];

    short pushXHi = (short)((radius - posX) + bounds[0] + 0x12);    // exit via +x
    g_playerDisplacement = pushXHi;
    short pushXLo = (short)(((bounds[2] - radius) - posX) - 0x12);  // exit via -x
    player_distance_z = pushXLo;

    short pushX = pushXHi;
    if (-player_distance_z < g_playerDisplacement) {
        pushX = pushXLo;                       // -x face is the nearer exit
        g_scaled_down_dist = player_distance_z;
    }

    short pushZHi = (short)(((radius + bounds[1]) - posZ) + 0x12);  // exit via +z
    g_collPushDepthZHi = pushZHi;
    short pushZLo = (short)(((bounds[3] - radius) - posZ) - 0x12);  // exit via -z
    g_collPushDepthZLo = pushZLo;

    short pushZ = pushZHi;
    if (-g_collPushDepthZLo < g_collPushDepthZHi) {
        pushZ = pushZLo;
        g_entity_bkp = (unsigned int)g_collPushDepthZLo;
    }

    int prevX = prevPos[0];
    int prevZ = prevPos[2];

    // Selector bit 0: movement and push disagree in sign on X; bit 1: on Z.
    unsigned char sel = (unsigned char)(
        (((unsigned char)((pos[2] - prevZ) >> 14) ^ (unsigned char)(pushZ >> 14)) & 2) |
        (((unsigned char)((pos[0] - prevX) >> 15) ^ (unsigned char)(pushX >> 15)) & 1));

    if (prevX == pos[0] && prevZ == pos[2]) {
        g_animFrameIdSave = 0;
    }

    switch (sel) {
    case 0:
        // Both axes say the push would shove the entity further in, so it did
        // not enter through either face this frame: undo the whole move.
        pos[0] = prevX;
        pos[2] = prevZ;
        return;

    case 1:
        if ((unsigned short)(pushX + 0x190) <= 0x320) {
            ENTITY->scaMatrixData.localMatrix.t[0] += pushX;
            ENTITY->status_flags &= 0xef;
            return;
        }
        break;

    case 2:
        if ((unsigned short)(pushZ + 0x190) <= 0x320) {
            ENTITY->scaMatrixData.localMatrix.t[2] += pushZ;
            ENTITY->status_flags |= 0x10;
            return;
        }
        break;

    case 3:
        break;

    default:
        return;
    }

    // Ambiguous, or a single-axis push deeper than 400 units: resolve on
    // whichever axis needs the smaller correction.
    int absX = pushX < 0 ? -pushX : pushX;
    int absZ = pushZ < 0 ? -pushZ : pushZ;
    if (absZ > absX) {
        ENTITY->scaMatrixData.localMatrix.t[0] += pushX;
        ENTITY->status_flags &= 0xef;
    } else {
        ENTITY->scaMatrixData.localMatrix.t[2] += pushZ;
        ENTITY->status_flags |= 0x10;
    }
}

// ===========================================================================
// collision_push_circle (0x0047e0e0)
// Shape 3: push the entity out of a circular obstacle. The circle's radius
// comes from the record's X extent alone; its centre is the box centre.
// The original ignores prevPos; it is present only to match the table type.
// ===========================================================================
static void collision_push_circle(short* bounds, int* pos, short* prevPos)
{
    (void)prevPos;
    unsigned short* b = (unsigned short*)bounds;
    unsigned int radius = (unsigned int)*(unsigned short*)(ENTITY->Sca_info + 10);

    // (width / 2) + entity radius, then the offsets from the circle centre.
    int reach = (int)(((unsigned int)b[0] - (unsigned int)b[2]) + radius * 2) / 2;
    int dz = ((pos[2] - (int)(unsigned int)b[3]) - reach) + (int)radius;
    int dx = ((pos[0] - (int)(unsigned int)b[2]) - reach) + (int)radius;

    int dist = SquareRoot0(dx * dx + dz * dz);
    if (dist < 0) dist = -dist;

    int penetration = reach - dist;
    if (penetration < 1) return;

    // The original divides unguarded, so dead centre would fault. Push along
    // +x instead, which is where any degenerate approach direction lands.
    if (dist == 0) {
        ENTITY->scaMatrixData.localMatrix.t[0] += penetration;
        ENTITY->status_flags &= 0xef;
        return;
    }

    int pushX = (penetration * dx) / dist;
    int pushZ = (penetration * dz) / dist;

    ENTITY->scaMatrixData.localMatrix.t[2] += pushZ;
    ENTITY->scaMatrixData.localMatrix.t[0] += pushX;

    int absX = pushX < 0 ? -pushX : pushX;
    int absZ = pushZ < 0 ? -pushZ : pushZ;
    if (absX < absZ) {
        ENTITY->status_flags |= 0x10;
    } else {
        ENTITY->status_flags &= 0xef;
    }
}

// ===========================================================================
// Room_SetupCollisionCallbacks (0x0047d140)
// Turn the RDT's per-quadrant record counts into absolute range pointers and
// install the boundary shape handlers. Called once per room from room_set,
// immediately after LoadRoomRdt - the rewrite is in place and not idempotent.
// ===========================================================================
void Room_SetupCollisionCallbacks(void)
{
    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    RDT_Boundary* base = (RDT_Boundary*)((char*)hdr + 0x18);

    int running = (int)(size_t)hdr->group[0];   // still the quadrant-0 count
    hdr->group[0] = base;

    g_playerDisplacement = 0;
    do {
        int count = (int)(size_t)hdr->group[g_playerDisplacement + 1];
        hdr->group[g_playerDisplacement + 1] = base + running;
        g_playerDisplacement++;
        running += count;
    } while (g_playerDisplacement < 4);

    g_CollisionShapeHandlers[1] = collision_push_rect;
    g_CollisionShapeHandlers[5] = collision_push_rect;
    g_CollisionShapeHandlers[3] = collision_push_circle;
    g_CollisionShapeHandlers[4] = collision_flag_set;
}

// ---------------------------------------------------------------------------
// Shared tail of check_room_collision: accept the current position, record how
// far the entity actually travelled, and advance the rollback point.
// ---------------------------------------------------------------------------
static unsigned char collision_accept(short floorStep, unsigned char clearResult)
{
    int* t = ENTITY->scaMatrixData.localMatrix.t;
    int dx = t[0] - (int)ENTITY->position.x;
    int dz = t[2] - (int)ENTITY->position.z;

    g_tempVar = (void*)(size_t)(unsigned int)SquareRoot0(dz * dz + dx * dx);

    ENTITY->position.x = (short)t[0];
    ENTITY->position.y = (short)t[1];
    ENTITY->position.z = (short)t[2];

    if (floorStep != 0) {
        g_animFrameIdSave = (unsigned int)(int)floorStep;
        return 3;
    }
    return clearResult;
}

// ===========================================================================
// check_room_collision (0x0047d310)
// Resolve ENTITY against the room boundaries.
//
// Pass 1 tests the incoming position and lets each hit record's handler push
// the entity out. Pass 2 re-tests the pushed position: if it is still inside a
// blocking record the frame's movement is rolled back to `position`
// (entity+0x6C), otherwise the rollback point advances to here.
//
// Returns 0 = clear, 1 = pushed out and now clear, 2 = still stuck (position
// reverted), 3 = a floor/step zone was crossed, with its step value left in
// g_animFrameIdSave. g_tempVar receives the distance actually travelled.
// ===========================================================================
unsigned char check_room_collision(VECTOR* position, short radius)
{
    SVECTOR offset;
    unsigned short hitBits = 0;
    short floorStep = 0;

    offset.x = 0;
    offset.y = 0;
    offset.z = 0;
    offset.pad = 0;

    ENTITY->collisionFlags &= 0xf7;
    if ((ENTITY->status_flags & 4) != 0) return 0;

    // The original dereferences unconditionally; room_set always installs the
    // boundary pointers before anything can run, but the port reaches this from
    // menu/attract paths where no room is loaded.
    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return 0;

    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    short cell = (short)ChkOutsideCell(position, &offset, hdr->cellX, hdr->cellZ);
    RDT_Boundary* first = hdr->group[cell];
    RDT_Boundary* last  = hdr->group[cell + 1];

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_playerPosScratch = *position;
    g_animFrameIdSave = 0;

    // ---- pass 1: push out of everything the incoming position is inside ----
    for (RDT_Boundary* rec = first; rec < last; rec++) {
        unsigned char cf = ENTITY->collisionFlags;

        if ((cf & 4) != 0 && (rec->flags & 0x100) == 0) {
            // Floor / step zone: the record carries a height step instead of a
            // shape, reported to the caller through g_animFrameIdSave.
            short step = (short)((((rec->type & 0x7f00) >> 8) * 10
                                  + (rec->flags & 0xff)) * 100);
            floorStep = (short)(((rec->type & 0x8000) ? -(int)step : 0) | 1);
            continue;
        }

        if ((rec->type & 0xff) == 5 && (cf & 0x10) != 0) continue;

        unsigned short shape = boundary_classify(&g_svecScratch, rec,
                                                (unsigned short)radius);
        if (shape == 0xffff) continue;

        if (shape < 6 && g_CollisionShapeHandlers[shape] != NULL) {
            g_CollisionShapeHandlers[shape]((short*)rec,
                                            ENTITY->scaMatrixData.localMatrix.t,
                                            &ENTITY->position.x);
        }
        hitBits |= (unsigned short)((rec->flags & 0x300) >> 8);
    }

    if (hitBits == 0) {
        return collision_accept(floorStep, 0);
    }

    // ---- pass 2: is the pushed position clear? ----
    // The original copies four dwords from entity+0x34, so `pad` picks up the
    // first word of worldMatrix - it is never read, only overwritten.
    g_playerPosScratch.x = ENTITY->scaMatrixData.localMatrix.t[0];
    g_playerPosScratch.y = ENTITY->scaMatrixData.localMatrix.t[1];
    g_playerPosScratch.z = ENTITY->scaMatrixData.localMatrix.t[2];
    hitBits = 0;
    g_animFrameIdSave = 0;

    for (RDT_Boundary* rec = first; rec < last; rec++) {
        unsigned short t = (unsigned short)(rec->type & 0xff);
        if (t == 4) continue;
        if (t == 5 && (ENTITY->collisionFlags & 0x10) != 0) continue;
        if ((ENTITY->collisionFlags & 4) != 0 && (rec->flags & 0x100) == 0) continue;

        unsigned short shape = boundary_classify(&g_svecScratch, rec,
                                                (unsigned short)radius);
        if ((shape & 0x8000) != 0) continue;

        unsigned short bits = (unsigned short)(rec->flags & 0x300);
        hitBits |= (unsigned short)(bits >> 8);
        if (shape == 3 && bits != 0) {
            g_animFrameIdSave = 1;
        }
    }

    if (g_animFrameIdSave != 0) {
        // Wedged against a circular obstacle - accept the push as it stands.
        return collision_accept(floorStep, 0);
    }
    if (hitBits == 0) {
        return collision_accept(floorStep, 1);
    }

    // Still inside something: roll this frame's movement back.
    g_tempVar = (void*)0;
    ENTITY->scaMatrixData.localMatrix.t[0] = (int)ENTITY->position.x;
    ENTITY->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
    ENTITY->scaMatrixData.localMatrix.t[2] = (int)ENTITY->position.z;

    if (floorStep != 0) {
        g_animFrameIdSave = (unsigned int)(int)floorStep;
        return 3;
    }
    return 2;
}

// ===========================================================================
// room_collision_check_0047da50 (0x0047da50)
// Point query with no push: which boundary flag bits does `position + offset`
// land in? Returns 1 as soon as a fully-blocking record (flags 0x300) contains
// the point, otherwise the OR of the flag bits of every record it is inside.
// ===========================================================================
short room_collision_check_0047da50(VECTOR* position, VECTOR* offset)
{
    unsigned short bits = 0;

    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return 0;

    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    short cell = (short)ChkOutsideCell(position, (SVECTOR*)offset,
                                       hdr->cellX, hdr->cellZ);
    g_playerPosScratch = *position;

    RDT_Boundary* first = hdr->group[cell];
    RDT_Boundary* last  = hdr->group[cell + 1];

    for (RDT_Boundary* rec = first; rec < last; rec++) {
        unsigned short f = boundary_classify_flags((SVECTOR*)offset, rec, 0);
        if ((short)f == -1) continue;
        if ((unsigned char)((f & 0x300) >> 8) == 3) return 1;
        bits |= (unsigned short)(f & 0x300);
    }

    return (short)bits;
}

// ---------------------------------------------------------------------------
// 0x00ac9cc8 - the group start pointer room_check_sight_blocked was last called
// with. The original stores it on every call; the only xref to the address is
// that write, so nothing ever reads it back. Kept so the store is not silently
// dropped from the port.
// ---------------------------------------------------------------------------
static RDT_Boundary* g_lastSightGroup = NULL;

// ===========================================================================
// One segment-vs-segment test, factored out of room_check_sight_blocked: does
// the ray (ent -> ent + dir) cross the box diagonal A -> B?
//
// Both halves are the standard 2D straddle test on the sign of the XZ cross
// product (vectorMul3's .y): A and B must fall on opposite sides of the ray,
// and the ray's two ends must fall on opposite sides of the diagonal.
//
// `normalizeA` reproduces the lone VectorNormal call the original makes, in the
// first of its two diagonal tests only (0x0047dda2). Scaling a vector to length
// 4096 cannot change a cross product's sign, so the asymmetry does not make the
// two tests disagree - it is reproduced rather than dropped because it is a real
// call in the original's control flow.
//
// Note the second half crosses against the UNDIVIDED `dir`, while its other
// operand is in /18 space. Mixed scale is harmless for a sign test.
// ===========================================================================
static int ray_crosses_diagonal(VECTOR* dir, int entX, int entZ, int dirX, int dirZ,
                                int ax, int az, int bx, int bz, int normalizeA)
{
    VECTOR p0, p1;

    // ---- do A and B straddle the ray? ----
    // g_playerPosScratch (0x00be11b0) is the original's scratch for the edge.
    g_playerPosScratch.x = bx - ax;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = bz - az;

    p1.x = (entX + dirX) - ax;  p1.y = 0;  p1.z = (entZ + dirZ) - az;
    p0.x = entX - ax;           p0.y = 0;  p0.z = entZ - az;

    vectorMul3(&g_playerPosScratch, &p1, &p1);
    vectorMul3(&g_playerPosScratch, &p0, &p0);

    if ((((unsigned int)p0.y ^ (unsigned int)p1.y) & 0x80000000u) == 0) return 0;

    // ---- ...and do the ray's ends straddle the diagonal? ----
    p1.x = bx - entX;  p1.y = 0;  p1.z = bz - entZ;
    vectorMul3(dir, &p1, &p1);

    p0.x = ax - entX;  p0.y = 0;  p0.z = az - entZ;
    if (normalizeA) VectorNormal(&p0, &p0);
    vectorMul3(dir, &p0, &p0);

    return (((unsigned int)p0.y ^ (unsigned int)p1.y) & 0x80000000u) != 0;
}

// ===========================================================================
// room_check_sight_blocked (0x0047db90)
// Does the straight line from ENTITY to ENTITY + `delta` cross a sight-blocking
// boundary record? `cell` is the quadrant index, so only one quadrant's list is
// walked - the same trick ChkOutsideCell sets up for the push path.
//
// Callers: check_line_of_sight (0x0048a4b0), check_weapon_line_of_sight
// (0x0048a530) and entity_check_angular_los (0x00489c60).
//
// The per-record test is the cheap "does the segment cross the box" trick: a
// chord that enters and leaves an axis-aligned box must cross at least one of
// its two diagonals, so instead of clipping against four edges the original runs
// two segment-vs-segment tests, one per diagonal:
//
//     (xMax, zMin) -> (xMin, zMax)        diagonal 1 (with the VectorNormal)
//     (xMin, zMin) -> (xMax, zMax)        diagonal 2
//
// EVERY coordinate is scaled down by 18 first, and that is not cosmetic:
// boundary coordinates are unsigned and reach 35677 in the shipped data (see the
// RDT_Boundary note in Types.h), so a cross product of two raw spans would
// overflow a signed 32-bit int. /18 keeps the products under ~1.4e7.
// ===========================================================================
unsigned int room_check_sight_blocked(VECTOR* delta, unsigned char cell)
{
    cell &= 0x1f;

    // The original dereferences unconditionally, as the other two entry points
    // in this file used to; the port reaches the entity code from paths with no
    // room loaded.
    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return 0;

    // NOT bounds-checked, matching the original. group[cell] / group[cell+1]
    // only bound a quadrant for cell 0-3, yet the mask above admits 0-31 and
    // entity_check_angular_los passes the pathfind counter at entity+0x164
    // straight through. What keeps it in range is an invariant, not a test:
    // entity_pathfind_update only calls that path while the counter is <= 3,
    // check_line_of_sight passes ChkOutsideCell's 0-3, and
    // check_weapon_line_of_sight loops 3 down to 0. Break any of those and both
    // the original and this port read a wild `first`/`last` pair out of the
    // record array and walk it - so if this ever crashes here, the bug is in the
    // caller's counter, not in the missing check.
    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    RDT_Boundary* first = hdr->group[cell];
    RDT_Boundary* last  = hdr->group[cell + 1];

    g_lastSightGroup = first;

    // The ray, in /18 space: entity -> entity + delta. These divides are signed
    // (IDIV at 0x0047dbc9); the record's are not.
    int entX = ENTITY->scaMatrixData.localMatrix.t[0] / 18;
    int entZ = ENTITY->scaMatrixData.localMatrix.t[2] / 18;
    int dirX = delta->x / 18;
    int dirZ = delta->z / 18;

    for (RDT_Boundary* rec = first; rec < last; rec++) {
        // 16-bit UNSIGNED divides (DIV at 0x0047dc0f, not IDIV). That is what
        // keeps the 222 shipped records with a coordinate above 32767 from
        // inverting into a negative box.
        int xMax = (int)(rec->xMax / 18u);
        int zMax = (int)(rec->zMax / 18u);
        int xMin = (int)(rec->xMin / 18u);
        int zMin = (int)(rec->zMin / 18u);

        // The original masks the record's flags down to the two blocking bits
        // and WRITES THE RESULT BACK (0x0047dc4a), discarding the low byte's
        // floor/step fine value for the rest of the room's life. Reproduced:
        // check_room_collision reads that low byte, so the clobber is
        // observable, and it is the sort of thing a "tidier" port would drop.
        unsigned short blocking = (unsigned short)(rec->flags & 0x300);
        rec->flags = blocking;

        // Only fully-blocking records occlude sight - except for Yawn (ids 13
        // and 18), for which every record occludes. Same pair of ids that
        // HandleEnemyPlayerCollisions special-cases.
        if (blocking != 0x300 && ENTITY->id != 13 && ENTITY->id != 18) continue;

        // Compared as the FULL 16-bit type, not (type & 0xff) as everywhere else
        // in this file, so a shape-4 or shape-5 record that also carries a floor
        // step in its high bits is not skipped here. Quirk of the original.
        if (rec->type == 4 || rec->type == 5) continue;

        if (ray_crosses_diagonal(delta, entX, entZ, dirX, dirZ,
                                 xMax, zMin, xMin, zMax, 1)) {
            return 1;
        }
        if (ray_crosses_diagonal(delta, entX, entZ, dirX, dirZ,
                                 xMin, zMin, xMax, zMax, 0)) {
            return 1;
        }
    }

    return 0;
}

// ===========================================================================
// check_room_collision_two_point (0x0047d6f0)
// Two-point boundary push for a PRONE body: rotate both ends by the entity
// yaw (endA first, endB second - the callers pass the -600/-800 end first),
// push each against its quadrant's boundary list with the SCA radius, and if
// either end is still inside afterwards roll the whole body back, position
// AND angle, from the backups at entity+0x6c..0x70 and +0x7e.
//
// Returns 0 = clear, 1 = pushed clear, 0x80 = still stuck (rolled back).
// A zombie on the floor is ~1200 units long, so the single-point
// check_room_collision test is not enough for it - this is what makes the
// laying-down branches of zombie_update / zombie_dead_animation work.
//
// endB's records are only re-walked when endA came out blocked; the per-end
// result bytes are the (flags & 0x300) >> 8 bits, ORed the same way
// check_room_collision reports them.
// ===========================================================================
unsigned char check_room_collision_two_point(SVECTOR* endA, SVECTOR* endB)
{
    // Status bit 2 set: entity is deactivated - nothing to push.
    if ((ENTITY->status_flags & 0x04) != 0) return 0;
    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return 0;

    // World-rotated ends: identity rotated by the entity yaw (0x74), the two
    // offsets applied as full matrices (local_20 / local_10 in the original).
    g_matrixScratch = g_identityMatrixData;
    RotMatrixY((int)ENTITY->angle, &g_matrixScratch);
    VECTOR rotatedA, rotatedB;
    ApplyMatrix(&g_matrixScratch, endA, &rotatedA);
    ApplyMatrix(&g_matrixScratch, endB, &rotatedB);

    // Position-relative ends: identity rotated by the MIRROR angle (+0x7E),
    // then the entity position folded in - these are what the collision
    // handlers write the pushed result into.
    g_matrixScratch = g_identityMatrixData;
    RotMatrixY((int)*(short*)((char*)ENTITY + 0x7E), &g_matrixScratch);
    SVECTOR worldA, worldB;
    ApplyMatrixSV(&g_matrixScratch, endA, &worldA);
    ApplyMatrixSV(&g_matrixScratch, endB, &worldB);
    worldA.x += ENTITY->position.x;
    worldA.z += ENTITY->position.z;
    worldB.x += ENTITY->position.x;
    worldB.z += ENTITY->position.z;

    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    unsigned short bitsA = 0;   // end A (param_1) result
    unsigned short bitsB = 0;   // end B (param_2) result
    unsigned int cellA = 0;
    unsigned int cellB = 0;

    // End B is pushed FIRST in the original (the loop counter starts at 1),
    // so bitsB / cellB fill before bitsA / cellA.
    for (int which = 1; which >= 0; which--) {
        VECTOR* centre = which ? &rotatedB : &rotatedA;
        SVECTOR* world = which ? &worldB : &worldA;
        unsigned short* bits = which ? &bitsB : &bitsA;
        unsigned int* cell = which ? &cellB : &cellA;

        centre->x += ENTITY->scaMatrixData.localMatrix.t[0];
        centre->z += ENTITY->scaMatrixData.localMatrix.t[2];

        g_svecScratch.x = 0;
        g_svecScratch.y = 0;
        g_svecScratch.z = 0;

        *cell = ChkOutsideCell(centre, &g_svecScratch, hdr->cellX, hdr->cellZ);

        // 0x0047d80a: a straight 16-byte copy of this endpoint into the point
        // boundary_classify actually tests. Ghidra splits it into five stores
        // through short halves, which reads as scratch bookkeeping and is easy
        // to drop - it was missing here, so every classify below tested
        // whatever position the LAST check_room_collision had left behind
        // (i.e. the player's), not this endpoint. The two-point body test then
        // reported "blocked" or "clear" based on where the player happened to
        // be standing, which for a pushed room object made the push succeed or
        // fail depending purely on which side it was approached from.
        g_playerPosScratch = *centre;

        RDT_Boundary* first = hdr->group[*cell];
        RDT_Boundary* last  = hdr->group[*cell + 1];

        for (RDT_Boundary* rec = first; rec < last; rec++) {
            unsigned char shape = (unsigned char)(rec->type & 0xFF);
            if (shape == 4 || shape == 5) continue;

            unsigned short s = boundary_classify(&g_svecScratch, rec,
                (unsigned short)*(short*)(ENTITY->Sca_info + 10));
            if (s == 0xFFFF) continue;

            g_CollisionShapeHandlers[s]((short*)rec, (int*)centre, &world->x);
            *bits |= (unsigned short)((rec->flags & 0x300) >> 8);
        }
    }

    if (bitsA == 0) {
        // End A clear: keep the push, mirror the position/angle backup.
        ENTITY->position.x = (short)ENTITY->scaMatrixData.localMatrix.t[0];
        ENTITY->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
        ENTITY->position.z = (short)ENTITY->scaMatrixData.localMatrix.t[2];
        *(short*)((char*)ENTITY + 0x7E) = ENTITY->angle;
        return (unsigned char)bitsB;
    }

    // End A blocked: re-walk end B's records and OR any further hits into
    // bitsB. Quirk of the original: the re-test walks the PREVIOUS quadrant -
    // the saved slot holds &group[cell], so the loop is [group[cell-1],
    // group[cell]) - not end B's own quadrant, which pass 1 already covered.
    // For cell 0 the original reads the group[-1] slot (the cellX/cellZ dword)
    // as a start pointer; that garbage is not reproduced, the walk just runs
    // empty. The scratch offset is NOT re-zeroed here, exactly like the
    // original.
    if (cellB > 0) {
        // 0x0047d8ec: end B's endpoint is copied into the tested point again
        // before the re-walk, the same 16-byte copy as in the loop above.
        g_playerPosScratch = rotatedB;

        RDT_Boundary* first = hdr->group[cellB - 1];
        RDT_Boundary* last  = hdr->group[cellB];
        for (RDT_Boundary* rec = first; rec < last; rec++) {
            unsigned char shape = (unsigned char)(rec->type & 0xFF);
            if (shape == 4 || shape == 5) continue;

            unsigned short s = boundary_classify(&g_svecScratch, rec,
                (unsigned short)*(short*)(ENTITY->Sca_info + 10));
            if ((s & 0x8000) != 0) continue;

            bitsB |= (unsigned short)((rec->flags & 0x300) >> 8);
        }
    }

    if (bitsB != 0) {
        // Still stuck: roll position AND angle back from the backups.
        ENTITY->scaMatrixData.localMatrix.t[0] = (int)ENTITY->position.x;
        ENTITY->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
        ENTITY->scaMatrixData.localMatrix.t[2] = (int)ENTITY->position.z;
        ENTITY->angle = *(short*)((char*)ENTITY + 0x7E);
        return 0x80;
    }

    ENTITY->position.x = (short)ENTITY->scaMatrixData.localMatrix.t[0];
    ENTITY->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
    ENTITY->position.z = (short)ENTITY->scaMatrixData.localMatrix.t[2];
    *(short*)((char*)ENTITY + 0x7E) = ENTITY->angle;
    return 1;
}

// ===========================================================================
// Room 3D-object collision, pushing and climbing.
//
// This is a whole subsystem that was missing: update_room_objects (0x00474090)
// was an empty stub in EngineStubs.cpp, so nothing in the port ever collided
// an entity with a room object and nothing ever raised g_main_state_flags bit
// 0x40 - the sole trigger for the push behaviour (0x10).
//
// Ghidra names 0x00474090 `update_sounds`; that is wrong, it touches no sound
// code at all. Renamed here and in the Ghidra database.
//
// Objects are the 0xA4-byte blocks cmd_omodel_set fills in, held in
// g_omodel_table[0 .. RDT.omodel_slot_count). They are laid out like
// the head of an Entity - Sca_info at +4, localMatrix.t at +0x34, position at
// +0x6C, yaw at +0x74 - which is exactly why the entity helpers below can take
// one on either side. Everything is addressed by raw offset because a 0xA4
// block is far shorter than a real Entity and must never be dereferenced as
// one.
//
// Object flag byte (record+0, SCD operand 2):
//   0x01 active   0x02 intangible   0x08 no collision
//   0x20 not pushable                0x40 climbable
// ===========================================================================

extern void update_player_position(PlayerEntity* ent, int mask); // 0x0041c060
extern int  ChkPlReachEntity(int obj);                           // 0x00474a20

// ===========================================================================
// ChkEntitySlide (0x00474330)
// Resolve one entity against one object along the shallower penetration axis.
//
//   ent        the moving actor - the player or an enemy. Its Sca_info (+4)
//              supplies the body extents and its pSca_hit_data (+8) the
//              per-part offsets.
//   obj        the room object. Its Sca_info supplies the box half-extents.
//   moveObject 0 = push `ent` out of `obj` (the real collision response)
//              1 = move `obj` instead, and return how many axes were moved.
//
// Mode 1 is how update_room_objects asks "is the player pressing into this?":
// the caller saves obj's position first and restores it afterwards, using only
// the return count. Mode 0 is the response that actually makes objects solid.
//
// Two quirks of the original, both reproduced deliberately:
//  - the loop advances the offsets pointer by 6 bytes and the size pointer by
//    0xC per part, but re-reads the extents from the FIRST size record every
//    iteration (0x004743b6 loads [ECX+4], not the advanced copy). Only the
//    part offsets really vary.
//  - the terminating `size[0] < 0` test happens after the part is processed,
//    so a list is always walked at least once. omodel records set +0x88 to
//    0x8000, i.e. exactly one part.
// ===========================================================================
int ChkEntitySlide(unsigned char* ent, unsigned char* obj, int moveObject)
{
    int moved = 0;

    if ((ent[0] & 0x08) != 0) return 0;     // entity has collision disabled
    if ((obj[0] & 0x02) != 0) return 0;     // object is intangible

    short* offsets = *(short**)(ent + 8);   // pSca_hit_data - per-part x,y,z
    short* sizes   = *(short**)(ent + 4);   // Sca_info - the walked size list

    // PORT GUARD, no equivalent in the original: an entity whose SCA data has
    // not been bound yet would walk a null list. The original is always called
    // after SetEntityScaHitData; the port has more stubbed init paths.
    if (offsets == NULL || sizes == NULL || *(short**)(obj + 4) == NULL) return 0;

    for (;;) {
        short* entSize = *(short**)(ent + 4);   // always the base record
        short* objSize = *(short**)(obj + 4);

        int objX = *(int*)(obj + 0x34);
        int objZ = *(int*)(obj + 0x3c);

        int dx = (objX - (int)offsets[0]) - *(int*)(ent + 0x34);
        int dy = (*(int*)(obj + 0x38) - (int)offsets[1]) - *(int*)(ent + 0x38);
        int dz = (objZ - (int)offsets[2]) - *(int*)(ent + 0x3c);

        // Summed half-extents. The entity contributes its radius (+0xA) on both
        // horizontal axes and its height (+0x8) on Y; the object contributes
        // its own per-axis half-extents (+2/+4/+6).
        int extX = (int)objSize[1] + (int)(unsigned short)entSize[5];
        int extY = (int)objSize[2] + (int)(unsigned short)entSize[4];
        int extZ = (int)objSize[3] + (int)(unsigned short)entSize[5];

        // The unsigned-wrap containment test used everywhere in this engine:
        // (d + ext) as unsigned <= 2*ext rejects both sides of the box at once.
        if ((unsigned int)(dx + extX) <= (unsigned int)(extX * 2) &&
            (unsigned int)(dy + extY) <= (unsigned int)(extY * 2) &&
            (unsigned int)(dz + extZ) <= (unsigned int)(extZ * 2)) {

            // Resolve along whichever axis is cheaper to escape. The cross
            // products compare the penetration depths without a divide.
            int crossX = extX * dz;
            int crossZ = extZ * dx;
            if (abs(crossX) < abs(crossZ)) {
                int push = extX;
                if (moveObject == 0) {
                    if (dx >= 0) push = -push;
                    *(int*)(ent + 0x34) = objX + push;
                } else {
                    if (dx < 0) push = -push;
                    moved++;
                    *(int*)(obj + 0x34) = (int)entSize[1] + *(int*)(ent + 0x34) + push;
                }
            } else {
                int push = extZ;
                if (moveObject == 0) {
                    if (dz >= 0) push = -push;
                    *(int*)(ent + 0x3c) = objZ + push;
                } else {
                    if (dz < 0) push = -push;
                    moved++;
                    *(int*)(obj + 0x3c) = (int)entSize[3] + *(int*)(ent + 0x3c) + push;
                }
            }
        }

        if (sizes[0] < 0) break;
        offsets += 3;   // 6 bytes
        sizes   += 6;   // 0xC bytes
    }

    return moved;
}

// ===========================================================================
// ChkObjSlide (0x00474500)
// Object-versus-object shove: if `mover` overlaps `other`, displace `other`
// out of it along the shallower axis. Both boxes use the Sca_info half-extents
// (+2 for X, +6 for Z); Y is not considered. Returns 1 when a shove happened.
//
// update_room_objects uses it twice: as a veto before a push starts (a crate
// already touching another crate cannot be pushed) and afterwards, to carry
// the shove on to whatever the moved object ran into.
// ===========================================================================
static int ChkObjSlide(unsigned char* mover, unsigned char* other)
{
    if (((other[0] | mover[0]) & 0x08) != 0) return 0;

    int dx = *(int*)(other + 0x34) - *(int*)(mover + 0x34);
    int dz = *(int*)(other + 0x3c) - *(int*)(mover + 0x3c);

    short* moverSize = *(short**)(mover + 4);
    short* otherSize = *(short**)(other + 4);

    int extX = (int)otherSize[1] + (int)moverSize[1];
    int extZ = (int)otherSize[3] + (int)moverSize[3];

    if ((unsigned int)(dx + extX) > (unsigned int)(extX * 2)) return 0;
    if ((unsigned int)(dz + extZ) > (unsigned int)(extZ * 2)) return 0;

    // The +1 / -1-minus is the original's: it parks the pushed object one unit
    // clear of the touching distance so the next frame does not re-trigger.
    if (abs(extX * dz) < abs(extZ * dx)) {
        int place = (dx < 0) ? (-1 - extX) : (extX + 1);
        *(int*)(other + 0x34) = *(int*)(mover + 0x34) + place;
    } else {
        int place = (dz < 0) ? (-1 - extZ) : (extZ + 1);
        *(int*)(other + 0x3c) = *(int*)(mover + 0x3c) + place;
    }
    return 1;
}

// ===========================================================================
// update_room_objects (0x00474090) - Ghidra `update_sounds`
// One pass over every active room object, run each frame from game_loop.
// Does five things per object, in this order:
//
//   1. resolve every active enemy out of it
//   2. count how long the player has been walking into it (the push probe)
//   3. at nine frames, start a push unless something vetoes it
//   4. resolve the PLAYER out of it - this is what makes objects solid
//   5. if it moved, shove the other objects and commit its new position
//
// A started push raises g_main_state_flags bit 0x40 for the whole frame;
// player_input_to_behavior turns that into action_behavior 0x10 and
// behavior_10_push runs until the bit drops, which happens the moment the
// player stops walking into the object.
//
// Note the enemy loops: the original decrements the counter only for ACTIVE
// entities while advancing the pointer unconditionally, so an inactive slot
// extends the walk. Reproduced as-is.
// ===========================================================================
void update_room_objects(void)
{
    int pushStarted = 0;

    for (int i = 0; i < (int)(unsigned char)g_RdtPointer->omodel_slot_count; i++) {
        unsigned char* obj = (unsigned char*)g_omodel_table[i];
        if (obj == NULL || (obj[0] & 1) == 0) continue;

        // ---- 1. enemies get pushed out of the object ----
        {
            Entity*      em      = g_EnemiesList;
            unsigned int emCount = (unsigned int)g_enemy_count;
            if (emCount != 0) {
                do {
                    if ((em->status_flags & 1) != 0) {
                        emCount--;
                        ChkEntitySlide((unsigned char*)em, obj, 0);
                    }
                    em++;
                } while ((int)emCount > 0);
            }
        }

        // The push probe below moves the object; these are the values to put
        // back once the probe has answered.
        int savedX = *(int*)(obj + 0x34);
        int savedZ = *(int*)(obj + 0x3c);

        // ---- 2. the push probe ----
        // Overlapping is not enough: the player must also be holding forward
        // and have the object inside the 470-unit reach box. Anything else
        // resets the counter, so the nine frames have to be consecutive.
        // The three tests short-circuit in the original, and that matters:
        // ChkPlReachEntity writes g_svecScratch as a side effect, so it must
        // not run when the first two have already failed.
        if (ChkEntitySlide((unsigned char*)&g_playerEntity, obj, 1) == 0 ||
            ((unsigned char)g_PlayerDpadHeld & 1) == 0 ||
            ChkPlReachEntity((int)obj) == 0) {
            obj[0x86] = 0;
            obj[0x87] = 0;
        } else {
            *(short*)(obj + 0x86) = (short)(*(short*)(obj + 0x86) + 1);
        }

        // ---- 3. start the push on the ninth frame ----
        // The probe's displacement is normally thrown away again: an object
        // only really moves on the frames where the push animation is ALREADY
        // running (action_behavior 0x10). The counter is reset to 8 rather than
        // 0 on a successful start, so it climbs back to 9 every frame the
        // player keeps leaning in and this block re-runs for the whole push.
        int restorePosition = 1;
        if ((obj[0] & 0x20) == 0 && *(short*)(obj + 0x86) == 9) {
            ENTITY = (Entity*)obj;

            // The object's own floor probe. Flag bit 0x04 means "this thing
            // never needs one" (objects that cannot leave their footprint).
            int blockedByRoom = 0;
            if ((obj[0] & 0x04) == 0) {
                if (check_room_collision_two_point((SVECTOR*)(obj + 0x94),
                                 (SVECTOR*)(obj + 0x9c)) != 0) {
                    blockedByRoom = 1;
                }
            }

            if (blockedByRoom) {
                // Parked at 10: the counter never reaches 9 again until the
                // player lets go, so a blocked object cannot re-trigger.
                obj[0x86] = 10;
                obj[0x87] = 0;
                DAT_00ae9ee8 = (unsigned int)obj;
            } else {
                pushStarted = 1;
                obj[0x86] = 8;
                obj[0x87] = 0;
                DAT_00ae9ee8 = (unsigned int)obj;

                // Snap the player square to the object before the animation.
                g_playerEntity.directionAngle =
                    (short)(((unsigned int)g_playerEntity.directionAngle + 0x200u) & 0xc00);

                // An enemy standing where the object would go vetoes the push.
                Entity*      em      = g_EnemiesList;
                unsigned int emCount = (unsigned int)g_enemy_count;
                if (emCount != 0) {
                    do {
                        if ((em->status_flags & 1) != 0) {
                            emCount--;
                            if (ChkEntitySlide((unsigned char*)em, obj, 1) != 0) {
                                obj[0x86] = 10;
                                obj[0x87] = 0;
                            }
                        }
                        em++;
                    } while ((int)emCount > 0);
                }

                // So does another object in the way - and that one aborts the
                // whole push, jumping straight to the position restore.
                int vetoed = 0;
                for (int j = 0; j < (int)(unsigned char)g_RdtPointer->omodel_slot_count; j++) {
                    unsigned char* other = (unsigned char*)g_omodel_table[j];
                    if (other == NULL || (other[0] & 1) == 0 || other == obj) continue;
                    if (ChkObjSlide(other, obj) != 0) {
                        obj[0x86] = 10;
                        obj[0x87] = 0;
                        DAT_00ae9ee8 = (unsigned int)obj;
                        vetoed = 1;
                        break;
                    }
                }

                if (!vetoed) {
                    if (g_playerEntity.isBeingAttackedFlag == 0 &&
                        g_playerEntity.action_behavior != 0x10) {
                        // The starting frame: lock the menu out and undo the
                        // probe - the object holds still while the wind-up
                        // animation plays.
                        g_message_flags &= 0xffbf;
                    } else {
                        // The push animation is already running (or the player
                        // is otherwise committed). This is the only path that
                        // keeps the probe's displacement, and it is what
                        // actually slides the object across the floor.
                        restorePosition = 0;
                    }
                }
            }
        }

        if (restorePosition) {
            *(int*)(obj + 0x34) = savedX;
            *(int*)(obj + 0x3c) = savedZ;
        }

        // ---- 4. the player is resolved out of the object ----
        ChkEntitySlide((unsigned char*)&g_playerEntity, obj, 0);

        // ---- 5. commit a moved object, shoving whatever it ran into ----
        if ((int)*(short*)(obj + 0x6c) != *(int*)(obj + 0x34) ||
            (int)*(short*)(obj + 0x70) != *(int*)(obj + 0x3c)) {
            for (int j = 0; j < (int)(unsigned char)g_RdtPointer->omodel_slot_count; j++) {
                unsigned char* other = (unsigned char*)g_omodel_table[j];
                if (other == NULL || (other[0] & 1) == 0 || other == obj) continue;
                ChkObjSlide(obj, other);
            }
            *(short*)(obj + 0x6c) = (short)*(int*)(obj + 0x34);
            *(short*)(obj + 0x70) = (short)*(int*)(obj + 0x3c);
        }

        // Mask 4 selects the event entries that probe against an object rather
        // than the player (game_loop passes mask 1 for the player's own pass).
        update_player_position((PlayerEntity*)obj, 4);
    }

    if (pushStarted) {
        g_main_state_flags |= MSF_OBJECT_PUSH;
        return;
    }
    if ((g_main_state_flags & MSF_OBJECT_PUSH) != 0) {
        // The push just ended: behavior_10_push watches this bit drop to run
        // its release state, and the menu bit comes back with it.
        g_main_state_flags &= ~MSF_OBJECT_PUSH;
        g_message_flags |= 0x40;
    }
}
