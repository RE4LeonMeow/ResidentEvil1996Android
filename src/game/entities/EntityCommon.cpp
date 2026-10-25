// EntityCommon.cpp - Entity-generic runtime shared by every entity type.
//
// These functions were originally all decompiled into Zombie.cpp because the
// zombie was the first entity ported, but none of them are zombie logic: they
// operate on whatever ENTITY currently points at. The monster update functions
// (ids 0-21), the shared human-character driver (ids 22-47, CharacterNpc.cpp)
// and the player code in PlayerAnimations.cpp all call into them.
//
// Anything that reads a zombie table or dispatches to a zombie behaviour stays
// in Zombie.cpp - including zombie_update_player_distance (0x00434330), which
// was named entity_* until its switch over the zombie move behaviour table
// (0x004bb280) gave it away.
//
// All original addresses from Ghidra.
// ============================================================================
#include "EntityCommon.h"
#include "../../Globals.h"
#include <cstring>

// Plant 42 boss update (0x00464d10).
extern void plant42_update(void);
// Yawn (giant snake) boss update (0x004051e0) - Yawn.cpp.
extern void yawn_update(void);
// Spider web (room 30C0 door blocker) update (0x00443640) - SpiderWeb.cpp.
extern void spiderweb_update(void);
// Tyrant boss update (0x00421990) - Tyrant.cpp. Ids 12 and 16 share it.
extern void tyrant_update(void);
// Cerberus (zombie dog) update (0x00497fb0) - Cerberus.cpp.
extern void cerberus_update(void);
// Monster plant (em100f) update (0x0045abb0) - MonsterPlant.cpp.
extern void monster_plant_update(void);
// Crow (em1005) update (0x0042e520) - Crow.cpp.
extern void crow_update(void);
// Hunter (em1006) update (0x004161f0) - Hunter.cpp.
extern void hunter_update(void);
// Adder (em100a, the regular-size snakes) update (0x004727f0) - Adder.cpp.
extern void adder_update(void);
// Wasp (em1007) update (0x0048daf0) - Wasp.cpp.
extern void wasp_update(void);
// Neptune (em100b, the shark) update (0x0043d8d0) - Neptune.cpp.
extern void neptune_update(void);
// Plant 42 roots (em100e) update (0x0047e1c0) - Plant42Roots.cpp.
extern void plant42_roots_update(void);
// Black Tiger (giant spider boss) update (0x0044f300) - BlackTiger.cpp.
extern void black_tiger_update(void);
// Chimera (em1009, the ceiling-hanging ape mutant) update (0x00438a70) - Chimera.cpp.
extern void chimera_update(void);

// ============================================================================
// Shared scratch globals (0x00be0de4 onward)
// Also written by RoomCollision.cpp, PlayerAnimations.cpp and WeaponDamage.cpp,
// which each carry their own extern for them. Declared in address order.
// ============================================================================
int          player_distance_z = 0;   // 0x00be0de4
int          g_scaled_down_dist = 0;  // 0x00be0de8
unsigned int g_entity_bkp = 0;        // 0x00be0df4
void*        _ENTITY_SAVE = NULL;

// ============================================================================
// enemies_update_functions_tbl @ 0x004d3c90
// Per-entity-type update function dispatch table, indexed by entity->id.
//
// **48 entries**. Ids 0-21 are the monsters; ids 22-47 all point at
// 0x0046acf0, the shared human-character driver (character_npc_update in
// CharacterNpc.cpp). 
//
// ============================================================================
void* enemies_update_functions_tbl[48] = {
    (void*)zombie_update,           // [0]  zombie (white coat)
    (void*)zombie_update,           // [1]  zombie (naked)
    (void*)cerberus_update,         // [2]  cerberus (zombie dog) (0x00497fb0)
    (void*)web_spinner_update,      // [3]  web spinner / big spider (0x00478310)
    (void*)black_tiger_update,      // [4]  black tiger (Giant spider boss)  (0x0044f300)
    (void*)crow_update,             // [5]  crow  (0x0042e520)
    (void*)hunter_update,           // [6]  hunter (0x004161f0) - Hunter.cpp
    (void*)wasp_update,             // [7]  wasp / bee  (0x0048daf0)
    (void*)plant42_update,          // [8]  plant 42  (0x00464d10)
    (void*)chimera_update,          // [9]  chimera  (0x00438a70)
    (void*)adder_update,            // [10] adder (regular size snakes) (0x004727f0)
    (void*)neptune_update,          // [11] neptune (shark) (0x0043d8d0)
    (void*)tyrant_update,           // [12] tyrant 1 - em100C, the lab slab (0x00421990)
    (void*)yawn_update,             // [13] yawn 1 (Giant snake) (0x004051e0)
    (void*)plant42_roots_update,    // [14] plant 42 roots (0x0047e1c0)
    (void*)monster_plant_update,    // [15] monster plant (0x0045abb0)
    (void*)tyrant_update,           // [16] tyrant 2 - em1010 (0x00421990)
    (void*)zombie_update,           // [17] zombie (green coat variant)
    (void*)yawn_update,             // [18] yawn 2 (0x004051e0)
    (void*)spiderweb_update,        // [19] spider web (not an enemy, but a spider web that blocks a door in room30Cx) (0x00443640)
    (void*)computer_arm_update,     // [20] em1014 - right forearm, lab terminal (0x00427330)
    (void*)computer_arm_update,     // [21] em1015 - left forearm, lab terminal (0x0040b760)
    (void*)character_npc_update,    // [22] filler: resolves to em100a placeholder EMDs.
                                    //      DC: the Forest zombie (id 0x16) -
                                    //      dc_apply_zombie_tables() swaps this
                                    //      slot to zombie_update.
    (void*)character_npc_update,    // [23] filler
    (void*)character_npc_update,    // [24] filler
    (void*)character_npc_update,    // [25] filler
    (void*)character_npc_update,    // [26] filler
    (void*)character_npc_update,    // [27] filler
    (void*)character_npc_update,    // [28] filler
    (void*)character_npc_update,    // [29] filler
    (void*)character_npc_update,    // [30] filler
    (void*)character_npc_update,    // [31] filler
    (void*)character_npc_update,    // [32] chris (cutscene actor)
    (void*)character_npc_update,    // [33] jill
    (void*)character_npc_update,    // [34] barry
    (void*)character_npc_update,    // [35] rebecca
    (void*)character_npc_update,    // [36] wesker
    (void*)character_npc_update,    // [37] Kenneth's corpse
    (void*)character_npc_update,    // [38] Forest's corpse
    (void*)character_npc_update,    // [39] richard
    (void*)character_npc_update,    // [40] enrico
    (void*)character_npc_update,    // [41] Kenneth's corpse, devoured state (em1029)
    (void*)character_npc_update,    // [42] Barry's dying, cutscene variant (em102a)
    (void*)character_npc_update,    // [43] Barry, cutscene variant (em102b)
    (void*)character_npc_update,    // [44] Rebecca, hunter cutscene variant (em102c)
    (void*)character_npc_update,    // [45] Barry, cutscene variant (em102d)
    (void*)character_npc_update,    // [46] Wesker, cutscene variant (em102e)
    (void*)character_npc_update     // [47] unused by scripts; em1030 = Chris alt
                                    //      outfit, only via costume swap
};

// ============================================================================
// getAngleTowardsTarget @ 0x00460450
// Returns the PS1 angle (0-0xFFF, 4096 = 360 deg) from the current entity's
// position to the target (px, pz). Wraps CalculateAngleBetweenPointsXZ.
// ============================================================================
unsigned short getAngleTowardsTarget(int px, int pz)
{
    return CalculateAngleBetweenPointsXZ(
        *(int*)&ENTITY->scaMatrixData.localMatrix.t[0],  // entity pos X
        *(int*)&ENTITY->scaMatrixData.localMatrix.t[2],  // entity pos Z
        px, pz);
}

// ============================================================================
// turn_toward_target @ 0x00489960
// Returns an angular step (+step, -step, or 0) to rotate the entity toward
// the target position. Uses getAngleTowardsTarget to compute the desired
// angle, then returns the shortest signed step to reduce the delta.
// If already facing the target within angle_step*2, returns 0.
// ============================================================================
int turn_toward_target(VECTOR* target_pos, short angle_step)
{
    unsigned short targetAngle = getAngleTowardsTarget(target_pos->x, target_pos->z);
    int delta = ((short)targetAngle - (short)(unsigned short)ENTITY->angle) + angle_step;
    delta &= 0xFFF;

    if (delta < (int)(unsigned short)(angle_step * 2)) {
        return 0;                    // already facing target
    }
    if (delta < ANGLE_HALF_CIRCLE + 1) {
        return angle_step;           // turn clockwise (shorter path right)
    }
    return -angle_step;              // turn counter-clockwise (shorter path left)
}

// ============================================================================
// entity_rotate_toward_target @ 0x004899b0
// Smoothly rotates the entity toward or away from a target position by
// angular steps. Bit 15 of angleStep flips the direction (face away).
// Computes desired angle via getAngleTowardsTarget, then adjusts entity
// angle by +angleStep (toward) or -angleStep + 180 deg (away).
// ============================================================================
void entity_rotate_toward_target(VECTOR* pos, unsigned short angleStep)
{
    short targetAngle = getAngleTowardsTarget(pos->x, pos->z);
    unsigned short baseAngle = (unsigned short)targetAngle;

    if ((angleStep & 0x8000) != 0) {
        angleStep = -angleStep;
        baseAngle = (baseAngle + 0x800) & 0xFFF;  // +180 deg
    }

    unsigned short delta = ((unsigned short)(angleStep - ENTITY->angle) + baseAngle) & 0xFFF;

    // The original widens the step to int BEFORE doubling: (short)param_2 * 2.
    // Casting the product back to short instead would wrap a step above 0x3FFF
    // negative and make the "close enough, snap to target" test never fire.
    if ((int)delta < (int)(short)angleStep * 2) {
        ENTITY->angle = (short)baseAngle;
        return;
    }

    ENTITY->angle = ENTITY->angle - (short)angleStep;
    if (delta < ANGLE_HALF_CIRCLE + 1) {
        ENTITY->angle = ENTITY->angle + (short)(angleStep * 2);
    }
}

// ============================================================================
// check_line_of_sight @ 0x0048a4b0
// Is the entity's view of `targetPos` blocked? Picks the boundary quadrant the
// target falls in with ChkOutsideCell, then hands the entity->target delta to
// room_check_sight_blocked. Returns 0 if the path is clear, 1 if blocked.
//
// targetPos is a VECTOR, not the SVECTOR Ghidra's stack-arg guess says: the
// original reads three ints out of it, at +0, +4 and +8.
// ============================================================================
unsigned char check_line_of_sight(VECTOR* targetPos)
{
    g_svecScratch.z = 0;
    g_svecScratch.y = 0;
    g_svecScratch.x = 0;

    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    unsigned int cell = ChkOutsideCell(targetPos, &g_svecScratch,
                                       hdr->cellX, hdr->cellZ);

    VECTOR delta;
    delta.x = targetPos->x - ENTITY->scaMatrixData.localMatrix.t[0];
    delta.y = targetPos->y - ENTITY->scaMatrixData.localMatrix.t[1];
    delta.z = targetPos->z - ENTITY->scaMatrixData.localMatrix.t[2];

    unsigned int result = room_check_sight_blocked(&delta, (unsigned char)cell);
    player_distance_z = result & 0xFF;
    return (unsigned char)(result & 0xFF);
}

// ============================================================================
// entity_check_angular_los @ 0x00489c60
// Checks if the target position is within the entity's angular FOV (half-angle
// param_1) AND has a clear line of sight. Returns 0 if path is clear, non-zero
// if blocked or outside the angular wedge.
// ============================================================================
unsigned int entity_check_angular_los(short fovHalfAngle, VECTOR* targetPos)
{
    short targetAngle = getAngleTowardsTarget(targetPos->x, targetPos->z);
    unsigned short delta = ((unsigned short)(fovHalfAngle - ENTITY->angle) + (unsigned short)targetAngle) & 0xFFF;

    // 0x00489c7?: param_1 * 2 is a signed int compare, not truncated to 16 bits
    if (fovHalfAngle * 2 < (int)delta)
        return 1;  // outside angular FOV

    VECTOR dir;
    dir.x = targetPos->x - *(int*)&ENTITY->scaMatrixData.localMatrix.t[0];
    dir.z = targetPos->z - *(int*)&ENTITY->scaMatrixData.localMatrix.t[2];
    dir.y = 0;

    // entity+0x164 is entity_pathfind_update's counter byte, passed through raw.
    // room_check_sight_blocked masks it to 5 bits and only 0-3 index a real
    // boundary quadrant; the caller only reaches here while the counter is <= 3.
    unsigned char boundaryIndex = *(unsigned char*)((char*)ENTITY + 0x164);
    return room_check_sight_blocked(&dir, boundaryIndex);
}

// ============================================================================
// checkAngularViewAndDistance @ 0x00489cf0
// Checks whether a target position is within the entity's angular field of
// view (a wedge defined by fovHalfAngle) and within maxDistance. Creates two
// edge vectors from entity angle +/- fovHalfAngle, then uses 2D cross products
// to test if the direction to target lies between them. Returns true if the
// target is both within range and within the FOV wedge.
// ============================================================================
unsigned char checkAngularViewAndDistance(short fovHalfAngle, short maxDistance, VECTOR* targetPos)
{
    VECTOR referenceForward = { 2000, 0, 0, 0 };

    VECTOR dirToTarget;
    dirToTarget.x = targetPos->x - *(int*)&ENTITY->scaMatrixData.localMatrix.t[0];
    dirToTarget.z = targetPos->z - *(int*)&ENTITY->scaMatrixData.localMatrix.t[2];
    dirToTarget.y = 0;

    int absDx = (dirToTarget.x ^ (dirToTarget.x >> 31)) - (dirToTarget.x >> 31);
    int absDz = (dirToTarget.z ^ (dirToTarget.z >> 31)) - (dirToTarget.z >> 31);
    if ((int)(unsigned short)maxDistance < absDx - (dirToTarget.x >> 31) + absDz)
        return 0;

    MATRIX local_20;
    local_20 = g_identityMatrixData;

    VECTOR leftEdge, rightEdge;
    RotMatrixY(ENTITY->angle - (int)fovHalfAngle, &local_20);
    ApplyMatrixLV(&local_20, &referenceForward, &leftEdge);

    RotMatrixY(fovHalfAngle * 2, &local_20);
    ApplyMatrixLV(&local_20, &referenceForward, &rightEdge);

    vectorMul3(&leftEdge, &dirToTarget, &leftEdge);
    vectorMul3(&rightEdge, &dirToTarget, &rightEdge);

    return (unsigned char)((leftEdge.y & 0x80000000U) < (rightEdge.y & 0x80000000U));
}

// ============================================================================
// entity_check_visual_range @ 0x0043bfa0
// Computes Euclidean distance from entity to player via SquareRoot0.
// If distance < range, sets status_flags bit 5 (0x20) - "player in visual range".
// ============================================================================
unsigned int entity_check_visual_range(unsigned int range)
{
    int dx = (int)g_playerEntity.scaMatrixData.localMatrix.t[0]
           - (int)ENTITY->scaMatrixData.localMatrix.t[0];
    int dz = (int)g_playerEntity.scaMatrixData.localMatrix.t[2]
           - (int)ENTITY->scaMatrixData.localMatrix.t[2];
    unsigned int distance = SquareRoot0(dx * dx + dz * dz);

    if (distance < range) {
        ENTITY->status_flags |= ENTITY_STATUS_PLAYER_ABOVE;
    }
    return distance;   // still in EAX at the original's RET; Yawn reads it back
}

// ============================================================================
// entity_check_alert_range @ 0x0043bfe0
// Computes Euclidean distance from entity to player via SquareRoot0.
// If distance < range, sets status_flags bit 7 (0x80) - "player in alert range".
// ============================================================================
unsigned int entity_check_alert_range(unsigned int range)
{
    int dx = (int)g_playerEntity.scaMatrixData.localMatrix.t[0]
           - (int)ENTITY->scaMatrixData.localMatrix.t[0];
    int dz = (int)g_playerEntity.scaMatrixData.localMatrix.t[2]
           - (int)ENTITY->scaMatrixData.localMatrix.t[2];
    unsigned int distance = SquareRoot0(dx * dx + dz * dz);

    if (distance < range) {
        ENTITY->status_flags |= ENTITY_STATUS_PLAYER_BELOW;
    }
    return distance;   // still in EAX at the original's RET; Yawn reads it back
}

// ============================================================================
// entity_pathfind_update @ 0x0048ad10
// Obstacle-detection pathfinding state machine. Uses entity+0x164 as a
// 3-bit counter (bits 0-4, clamped to 15) + direction flag (bit 5).
// Returns: 0 = no target, 1 = target acquired/path clear, 2 = waiting.
// When counter == 3, stores player position as new movement target.
// ============================================================================
unsigned int entity_pathfind_update(void)
{
    unsigned char* state = (unsigned char*)ENTITY + 0x164;  // pathfind_state
    unsigned char val = *state;
    unsigned char counter = val & 0x1F;

    // Every advance in the original is `INC byte ptr [...]` on the WHOLE byte
    // (0x0048adc9, 0x0048adab, 0x0048adc0, 0x0048ad97), not `counter + 1`.
    // Bit 5 carries the "line of sight was blocked" result, OR-ed in on each of
    // frames 0-2 and tested once on frame 3; rebuilding the byte from the
    // counter dropped it every frame, so only the last frame's LOS result
    // counted and the waypoint refreshed on the wrong frames.
    if (counter > 3) {
        ++*state;
        if ((*state & 0x1F) > 0x0F)
            *state &= 0xC0;  // clamp counter
        return 2;
    }

    char result = entity_check_angular_los(1512, (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t);
    *state = (result << 5) | val;

    val = *state;
    counter = val & 0x1F;

    if (counter == 3) {
        if ((val & 0x20) == 0) {
            // 16-bit stores - see the waypoint note in Entities.h.
            ENTITY->player_pos_x = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[0];
            ENTITY->player_pos_z = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[2];
            ++*state;
            *state &= ~0x20;
            return 1;
        }
        ++*state;
        *state &= ~0x20;
        return 0;
    }

    ++*state;
    return 2;
}

// ============================================================================
// entity_update_wander_turn @ 0x00489800
// Controls randomized wandering turns when the entity gets stuck. If movement
// distance falls below a threshold derived from the entity's speed divider,
// a turn counter increments. When it exceeds turn_limit, a random turn
// (direction from g_RandSeed bit 6) activates. Also applies angular rotation
// toward the current waypoint using getAngleTowardsTarget.
// ============================================================================
unsigned int entity_update_wander_turn(unsigned int movement_dist, unsigned char* control_flags, unsigned char* turn_counter, unsigned short angle_step, unsigned char turn_limit)
{
    unsigned short* entity_angle = (unsigned short*)&ENTITY->angle;
    // `MOVSX EAX, word ptr [EDX + 0xc2]` at 0x00489834 and 0x00489870: the
    // threshold scales with move_speed_current, a SIGNED 16-bit field at 0xC2.
    // The old code read an unsigned byte at 0x61 - inside scaMatrixData, i.e.
    // matrix bytes - so the stuck-detection threshold was garbage.
    short speedDiv = (short)ENTITY->move_speed_current;

    if ((*control_flags & 0x80) != 0) {
        *entity_angle = *entity_angle
            + (1 - (unsigned short)((*control_flags & 0x40) >> 5)) * angle_step;

        int speedDiv3 = speedDiv * 3;
        int threshold = (speedDiv3 + (speedDiv3 >> 31 & 3)) >> 2;

        if ((unsigned int)threshold < movement_dist) {
            unsigned char newCount = *turn_counter - 1;
            *turn_counter = newCount;
            if (newCount == 0) {
                *control_flags = 0;
                *turn_counter = 0;
            }
        }
        return 1;
    }

    if (movement_dist < (unsigned int)((speedDiv * 2) / 3)) {
        unsigned char newCount = *turn_counter + 1;
        *turn_counter = newCount;
        if (turn_limit < newCount) {
            unsigned char flags = *control_flags;
            *control_flags = flags | 0x80;
            *control_flags = ((unsigned char)g_RandSeed & 0x40) | flags | 0x80;
            *turn_counter = turn_limit / 6;
        }
    } else {
        *control_flags = 0;
        *turn_counter = 0;
    }

    // 0x004898c2: `MOVSX ECX, word ptr [EAX + 0x168]` / `MOVSX EDX, word ptr
    // [EAX + 0x166]` then `getAngleTowardsTarget(x, z)` - the movement WAYPOINT,
    // as sign-extended shorts. The old code read unsigned bytes at 0xB3/0xB4,
    // which are inside pad_b0 and therefore always zero: every entity that
    // relies on this to steer turned toward the room origin instead of its
    // waypoint. That is why zombies wandered instead of closing on the player -
    // zombie_walk1 calls this every frame of the chase.
    short waypointAngle = getAngleTowardsTarget((int)ENTITY->player_pos_x,
                                               (int)ENTITY->player_pos_z);
    unsigned short targetAngle = (unsigned short)waypointAngle;

    if ((angle_step & 0x8000) != 0) {
        angle_step = -angle_step;
        targetAngle = (targetAngle + 0x800) & 0xFFF;
    }

    unsigned short delta = ((unsigned short)(angle_step - *entity_angle) + targetAngle) & 0xFFF;

    if ((int)delta < (short)(angle_step * 2)) {
        *entity_angle = targetAngle;
        return 0;
    }

    *entity_angle = *entity_angle - angle_step;
    if (delta < 0x801) {
        *entity_angle = *entity_angle + angle_step * 2;
    }
    return 0;
}

// ============================================================================
// entity_swerve_around_obstacle @ 0x00489a50
// Obstacle-avoidance steering. The ONLY caller is crow_state_run (0x0042e84a),
// but the function sits in the shared 0x00489xxx entity-helper block alongside
// entity_update_wander_turn, so it lives here rather than in Crow.cpp.
//
// Returns the yaw delta to add to Entity->angle this frame.
//
//   angleStep  the magnitude of the turn (the crow passes 0x40)
//   blocked    non-zero while the entity is up against room geometry
//              (check_room_collision's return value)
//   swerve     [in/out] the yaw delta being held for the current swerve
//   latch      [in/out] bit 7 = "not currently swerving", bits 0-6 = frames of
//              swerve left. The frame count is seeded from g_animFrameIdSave,
//              which the caller loads immediately before the call (the crow
//              writes 4 at 0x0042e82c) - it is an argument passed through a
//              scratch global, not a leftover.
//
// Three paths:
//   A. blocked and not already latched -> pick a swerve direction (toward the
//      target's X or Z depending on status_flags bit 4), arm the latch, and
//      return the delta, folded through the quadrant fix-up below.
//   B. blocked resolved but frames remain -> keep steering the same way and
//      count the latch down.
//   C. latch exhausted -> clear the swerve and fall back to a plain
//      turn-toward-target step (0, +angleStep or -angleStep).
//
// The quadrant fix-up in path A (`AND CH,0xc` / `AND DH,0xc` at 0x00489b45)
// compares bits 10-11 of the current yaw against bits 10-11 of yaw+swerve: when
// the swerve would cross a quadrant boundary the delta is rewritten so the turn
// lands ON the boundary instead of overshooting past it.
// ============================================================================
short entity_swerve_around_obstacle(short angleStep, char blocked,
                                    short* swerve, unsigned char* latch)
{
    // The original copies g_playerPosScratch into a 16-byte local FIRST, then
    // reads the target out of the copy - so later writes to the global (there
    // are none on this path, but the copy is what the code indexes) cannot move
    // the target mid-call.
    VECTOR target = g_playerPosScratch;

    // AX survives across the whole function and is reused in path C; Ghidra
    // surfaces it as `extraout_AX`.
    short baseAngle = (short)getAngleTowardsTarget(target.x, target.z);

    if (blocked == 0 && (*latch & 0x80) == 0) {
        *latch |= 0x80;
    }

    if (blocked != 0 && (*latch & 0x80) == 0) {
        // ---- path A: newly blocked, choose a direction ----
        if (*swerve == 0) {
            // Aim at the target's X with our own Z (or, with status bit 4
            // clear, our own X with the target's Z) - a 90-degree sidestep.
            *swerve = (short)getAngleTowardsTarget(
                target.x, ENTITY->scaMatrixData.localMatrix.t[2]);
            if ((ENTITY->status_flags & 0x10) == 0) {
                *swerve = (short)getAngleTowardsTarget(
                    ENTITY->scaMatrixData.localMatrix.t[0], target.z);
            }
            unsigned short d =
                (unsigned short)((*swerve - ENTITY->angle) + angleStep) & 0xFFF;
            *swerve = (short)-angleStep;
            if (d <= 0x800) {
                *swerve = angleStep;
            }
        }
        *latch = (unsigned char)g_animFrameIdSave;

        short sw = *swerve;
        unsigned short yaw = (unsigned short)ENTITY->angle;
        int sum = (int)ENTITY->angle + (int)sw;

        // Quadrant unchanged - use the swerve as-is.
        if (((yaw >> 8) & 0x0C) == (((unsigned int)sum >> 8) & 0x0C)) {
            return sw;
        }

        g_scaled_down_dist = (int)sw - (int)(((unsigned int)sum & 0xFFF) & 0x3FF);
        if (sw < 0) {
            g_scaled_down_dist = (int)((unsigned short)yaw & 0xFFF & 0x3FF) + (int)sw;
        }
        return (short)g_scaled_down_dist;
    }

    if ((*latch & 0x7F) != 0) {
        // ---- path B: hold the swerve, count down ----
        // The DEC is on the whole byte, so bit 7 rides along with the counter.
        *latch = (unsigned char)(*latch - 1);
        unsigned short d =
            (unsigned short)((*swerve - ENTITY->angle) + angleStep) & 0xFFF;
        *swerve = (short)-angleStep;
        if (d <= 0x800) {
            *swerve = angleStep;
        }
        return *swerve;
    }

    // ---- path C: no swerve pending, plain turn-toward-target ----
    *swerve = 0;
    *latch = 0;
    unsigned short d =
        (unsigned short)((angleStep - ENTITY->angle) + baseAngle) & 0xFFF;
    if ((int)angleStep * 2 >= (int)(unsigned int)d) {
        return 0;
    }
    if (d <= 0x800) {
        return angleStep;
    }
    return (short)-angleStep;
}

// ============================================================================
// SetEntityScaHitData @ 0x0041b2c0
// Converts the entity's local SCA collision points into world-space hit
// coordinates by rotating them around the Y-axis using the entity's angle.
// Iterates the SCA volume list (6 shorts per entry, terminated by negative
// first short), rotating each point so the collision/hit-check system can
// operate in world space.
// ============================================================================
void SetEntityScaHitData(Entity* ent)
{
    short* srcVol = *(short**)((char*)ent + 4);
    short* dstVol = *(short**)((char*)ent + 8);

    g_svecScratch.y = ent->angle;
    g_svecScratch.z = 0;
    g_svecScratch.x = 0;

    RotMatrix(&g_svecScratch, &g_matrixScratch);

    // The terminating `srcVol[0] < 0` test happens AFTER the entry is written,
    // so a record is always walked at least once. The player/zombie records
    // pack their single volume into the same 16 bytes as the terminator (slot 0
    // is 0x8000|id), so a test-first loop would never write anything at all.
    for (;;) {
        SVECTOR localVertex;
        localVertex.x = srcVol[1];
        localVertex.z = srcVol[3];
        localVertex.y = srcVol[2];

        // ApplyMatrix writes three ints (see its note in GteMatrix.cpp); the
        // destination has to be a VECTOR, and the truncation to short happens here.
        VECTOR worldVertex;
        ApplyMatrix(&g_matrixScratch, &localVertex, &worldVertex);

        dstVol[0] = (short)worldVertex.x;
        dstVol[1] = srcVol[2];
        dstVol[2] = (short)worldVertex.z;

        if (srcVol[0] < 0) break;
        dstVol += 3;
        srcVol += 6;
    }
}

// ============================================================================
// ResolveEntityScaCollision @ 0x0041b0a0
// Resolves SCA (Sphere/Cylinder Area) collision between two entities.
// Iterates both entities' SCA volume lists, checks each pair for overlap
// using Euclidean distance + SquareRoot0, and pushes the second entity
// away from the first by the penetration depth. Returns 1 if collision
// occurred, 0 otherwise.
// ============================================================================
unsigned int ResolveEntityScaCollision(Entity* entA, Entity* entB)
{
    if (entB->state == 4) return 0;       // eating/headless state - skip collision
    if ((entA->status_flags | entB->status_flags) & 2) return 0;  // one is deactivated

    // The world-space geometry lives at +8 (pSca_hit_data, rotated by
    // SetEntityScaHitData) while the radius/height and the terminator live at
    // +4 (Sca_info). Like SetEntityScaHitData, the terminating tests happen
    // AFTER each volume pair is processed, so a record is always walked at
    // least once - the player/zombie records carry their single volume in the
    // same 16 bytes as the terminator.
    short* volAWorld = *(short**)((char*)entA + 8);
    short* volALocal = *(short**)((char*)entA + 4);
    unsigned char hitFlag = 0;

    for (;;) {
        short* volBWorld = *(short**)((char*)entB + 8);
        short* volBLocal = *(short**)((char*)entB + 4);

        for (;;) {
            int dx = ((int)volBWorld[0] - (int)volAWorld[0])
                   - *(int*)&entA->scaMatrixData.localMatrix.t[0]
                   + *(int*)&entB->scaMatrixData.localMatrix.t[0];
            int dz = ((int)volBWorld[2] - (int)volAWorld[2])
                   - *(int*)&entA->scaMatrixData.localMatrix.t[2]
                   + *(int*)&entB->scaMatrixData.localMatrix.t[2];

            unsigned short radiusA = volALocal[5];  // cylinder radius
            unsigned short radiusB = volBLocal[5];
            unsigned short heightA = volALocal[4];  // cylinder half-height
            unsigned short heightB = volBLocal[4];

            int dist = SquareRoot0(dz * dz + dx * dx);
            int penetration = (unsigned int)(radiusA + radiusB) - (dist + 1);

            if (penetration > 0) {
                int dy = (int)volBWorld[1]
                       + (*(int*)&entB->scaMatrixData.localMatrix.t[1] - (int)volAWorld[1])
                       - *(int*)&entA->scaMatrixData.localMatrix.t[1];

                int maxHeight = (unsigned int)(unsigned short)heightA
                              + (unsigned int)(unsigned short)heightB;

                if (-maxHeight < dy && dy < maxHeight) {
                    int pushX = (penetration * dx) / (dist + 1);
                    int pushZ = (penetration * dz) / (dist + 1);

                    int dy2 = (int)volBWorld[1]
                            + ((int)entB->position.y - (int)volAWorld[1])
                            - *(int*)&entA->scaMatrixData.localMatrix.t[1];

                    if (dy2 <= -maxHeight || maxHeight <= dy2) {
                        int posXA = *(int*)&entA->scaMatrixData.localMatrix.t[0];
                        if ((entB->position.x < posXA && posXA < *(int*)&entB->scaMatrixData.localMatrix.t[0])
                         || (posXA < entB->position.x && *(int*)&entB->scaMatrixData.localMatrix.t[0] < posXA)) {
                            if (-pushX < 1)
                                pushX = -(-pushX + (unsigned int)(unsigned short)radiusA * 2);
                            else
                                pushX = (unsigned int)(unsigned short)radiusA * 2 + pushX;
                        }
                        int posZA = *(int*)&entA->scaMatrixData.localMatrix.t[2];
                        if ((entB->position.z < posZA && posZA < *(int*)&entB->scaMatrixData.localMatrix.t[2])
                         || (posZA < entB->position.z && *(int*)&entB->scaMatrixData.localMatrix.t[2] < posZA)) {
                            if (-pushZ < 1)
                                pushZ = -(-pushZ + (unsigned int)(unsigned short)radiusA * 2);
                            else
                                pushZ = (unsigned int)(unsigned short)radiusA * 2 + pushZ;
                        }
                    }

                    *(int*)&entB->scaMatrixData.localMatrix.t[0] += pushX;
                    *(int*)&entB->scaMatrixData.localMatrix.t[2] += pushZ;
                    hitFlag = 1;
                }
            }

            if (*volBLocal < 0) break;
            volBWorld += 3;       // next volume: world geometry advances 3 shorts
            volBLocal += 6;       // local metadata advances 6 shorts
        }

        if (*volALocal < 0) break;
        volAWorld += 3;
        volALocal += 6;
    }

    return hitFlag;
}

// ============================================================================
// HandleEnemyPlayerCollisions @ 0x00489e10
// Resolves SCA collisions between all active enemies and the current ENTITY,
// computing a combined hit flag. Also handles Yawn-specific player pushback:
// if player moved >450 units and Yawn (ID 13/18) is active, pushes the player
// back by 1/4 of the displacement.
// ============================================================================
unsigned int HandleEnemyPlayerCollisions(void)
{
    unsigned char hitFlag = 0;
    Entity* enemies = g_EnemiesList;
    signed char count = g_enemy_count;

    while (count != 0) {
        if (enemies->status_flags != 0 && enemies != ENTITY) {
            hitFlag |= (unsigned char)ResolveEntityScaCollision(enemies, ENTITY);
        }
        count--;
        enemies = (Entity*)((char*)enemies + sizeof(Entity));
    }

    int dx = (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[0]
           - (int)g_playerEntityPointer.position.x;
    int dz = (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[2]
           - (int)g_playerEntityPointer.position.z;

    int absDx = (dx ^ (dx >> 31)) - (dx >> 31);
    int absDz = (dz ^ (dz >> 31)) - (dz >> 31);
    g_playerDisplacement = absDx - (dz >> 31) + absDz;

    if (g_playerDisplacement > 450
        && (g_playerEntityPointer.flags & 2) == 0
        && (g_EnemiesList[0].id == 13 || g_EnemiesList[0].id == 18))
    {
        g_playerDisplacement = dx >> 4;
        player_distance_z = dz >> 4;
        g_playerEntityPointer.scaMatrixData.localMatrix.t[0] =
            g_playerEntityPointer.position.x + g_playerDisplacement;
        g_playerEntityPointer.scaMatrixData.localMatrix.t[2] =
            g_playerEntityPointer.position.z + player_distance_z;
    }

    return hitFlag;
}

// ============================================================================
// blood_splatter_physics @ 0x00437d20
// Blood drop physics after a hit. Moves the blood joint downward with
// gravity, checks room collision for wall/floor hits, creates blood
// billboards at impact points, plays impact SFX, and decrements the
// speed parameter. Called from zombie_update for the hand joint.
// ============================================================================
void blood_splatter_physics(int jointData, short gravityStep)
{
    if (*(int*)(jointData + 0x5C) >= -100 && (*(unsigned char*)(jointData + 3) & 0x1F) >= 6)
        return;

    unsigned char jointFlag = *(unsigned char*)(jointData + 3);
    unsigned char animFrame = jointFlag & 0x1F;

    SVECTOR splatterDir;
    splatterDir.z = 0x40;
    splatterDir.y = (6 - animFrame) * 0x10;
    splatterDir.x = (6 - animFrame) * 8;

    RotMatrix(&splatterDir, &g_matrixScratch);
    MulMatrix((MATRIX*)(jointData + 0x44), &g_matrixScratch);

    g_matrixScratch = g_identityMatrixData;
    RotMatrixY(ENTITY->angle, &g_matrixScratch);

    VECTOR* jointPos = (VECTOR*)(jointData + 0x58);
    SVECTOR local_18;
    ApplyMatrixSV(&g_matrixScratch, (SVECTOR*)(jointData + 4), &local_18);

    int savedX = jointPos->x;
    int savedZ = *(int*)(jointData + 0x60);

    jointPos->x += (1 - (unsigned int)((jointFlag & 0x40) >> 5)) * (int)local_18.x;
    *(int*)(jointData + 0x60) += (1 - (unsigned int)((jointFlag & 0xBF) >> 6)) * (int)local_18.z;

    g_svecScratch.z = 0; g_svecScratch.y = 0; g_svecScratch.x = 0;
    short collision = room_collision_check_0047da50(jointPos, (VECTOR*)&g_svecScratch);

    if (collision != 0) {
        jointFlag ^= 0x40;
        jointPos->x = savedX;
        *(int*)(jointData + 0x60) = savedZ;
        *(unsigned char*)(jointData + 3) = jointFlag;

        jointPos->x = (1 - (unsigned int)((jointFlag & 0x40) >> 5)) * (int)local_18.x + savedX;
        *(int*)(jointData + 0x60) = (1 - (unsigned int)((jointFlag & 0xBF) >> 6)) * (int)local_18.z + savedZ;

        collision = room_collision_check_0047da50(jointPos, (VECTOR*)&g_svecScratch);
        if (collision != 0) {
            *(unsigned char*)(jointData + 3) ^= 0xC0;
        }

        jointPos->x = savedX;
        *(int*)(jointData + 0x60) = savedZ;
        *(short*)(jointData + 4) >>= 1;

        VECTOR zero = { 0, 0, 0, 0 };
        Effect_CreateBillboard(0, 0, 0, (void*)(jointData + 0x44), &zero, 0);
    }

    short accel = *(short*)(jointData + 6) - (unsigned short)*(unsigned char*)(jointData + 2) * gravityStep;
    *(short*)(jointData + 6) = accel;
    *(int*)(jointData + 0x5C) -= (int)accel;

    if (*(int*)(jointData + 0x5C) > -0x65) {
        // -100 (`MOV dword ptr [EBX+0x5c],0xffffff9c`), not -99. The resting
        // height has to land on exactly -100: that is the value zombie_update
        // compares world.t[1] against before it draws the part's ground shadow,
        // and the first guard in this function uses it as the at-rest test.
        *(int*)(jointData + 0x5C) = -100;
        *(unsigned char*)(jointData + 2) = 0;
        *(short*)(jointData + 6) = -accel;
        *(unsigned char*)(jointData + 3) += 1;
        *(short*)(jointData + 4) += 0x28;
        *(short*)(jointData + 6) = -accel >> 2;

        VECTOR zero = { 0, 0, 0, 0 };
        Effect_CreateBillboard(0, 0, 0, (void*)(jointData + 0x44), &zero, 0);
        Snd_em(8);
    }

    if (*(short*)(jointData + 4) > 0)
        *(short*)(jointData + 4) = 0;

    *(unsigned char*)(jointData + 2) += 1;
}

// ============================================================================
// snap_player_to_grab_position @ 0x00489ee0
// Positions the player at the entity's grab point by extracting the current
// animation vertex for the entity's attacking joint, rotating it to world
// space, and computing the target position. Sets the player's grab-target
// coordinates so the player model snaps to the bite/grab spot.
// ============================================================================
void snap_player_to_grab_position(void* player)
{
    entity_extract_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase, 0);

    g_matrixScratch = g_identityMatrixData;
    RotMatrixY(ENTITY->angle, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);

    *(short*)((char*)ENTITY + 0xC6) =
        (short)(*(int*)&ENTITY->scaMatrixData.localMatrix.t[0]) - g_svecScratch.x;
    *(short*)((char*)ENTITY + 0xC8) =
        (short)(*(int*)&ENTITY->scaMatrixData.localMatrix.t[2]) - g_svecScratch.z;

    *(short*)((char*)player + 0xC6) = *(short*)((char*)ENTITY + 0xC6);
    *(short*)((char*)player + 0xC8) = *(short*)((char*)ENTITY + 0xC8);
}

// ============================================================================
// joint_setup_attack_effect @ 0x0048a070
// Configures a joint for an attack special effect (blood, bite mark, etc.).
// Sets size parameters (0x28 standard or 0x30 for alt costumes), a timer
// at +0x70, effect type at +3, and frame match at +0x72. Also applies to
// the weapon-part joint if g_main_state_flags has bit 0 set.
// ============================================================================
void joint_setup_attack_effect(int joint, unsigned char effectType, unsigned short timer, unsigned short frameMatch)
{
    if ((*(unsigned char*)(joint + 2) & 0x80) != 0) return;

    int sizeVal = 0x28;
    unsigned char sizeB = 0x60;
    unsigned char sizeC = 0x28;

    // Larger effect size for alternate costumes (entity ID 3 or 4)
    if (*(char*)((char*)ENTITY + 1) == 3 || *(char*)((char*)ENTITY + 1) == 4) {
        sizeVal = 0x30;
        sizeB = 0x18;
        sizeC = 0x18;
    }

    // Apply effect to the main joint
    joint_enable_special_effect(joint, sizeB, sizeVal, sizeC);
    *(unsigned short*)(joint + 0x70) = timer;
    *(unsigned char*)(joint + 3) = effectType;
    *(unsigned short*)(joint + 0x72) = frameMatch;

    // Also apply to weapon-part joint if active
    if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
        int weaponJoint = (*(int*)((char*)ENTITY + 0xAC) - *(int*)&ENTITY->jointsStructs) + joint;
        joint_enable_special_effect(weaponJoint, sizeB, sizeVal, sizeC);
        *(unsigned char*)(weaponJoint + 3) = effectType;
        *(unsigned short*)(weaponJoint + 0x70) = timer;
        *(unsigned short*)(weaponJoint + 0x72) = frameMatch;
    }
}

// ============================================================================
// FUN_004565f0 (0x004565f0)
// Builds an entity's ground-shadow billboard. `pos` is the world offset (from
// g_svecScratch at every call site) and `quad` is the entity's sprite block at
// entity+0xE4; halfW/halfH are the shadow's half-extents.
//
// Despite the old "SCA init helper" label this is the shadow quad builder, and it
// was an empty stub - so every character's shadow block stayed all zeros and there
// was nothing for the sprite pass to draw.
//
// Layout, from the disassembly at 0x004565f0 (SVECTOR = 8 bytes):
//   quad[0]        world offset, copied wholesale from `pos`
//   quad[1]        primitive header; .z/.pad take the packed 0xRRGGBB tint that
//                  the caller left in scratch global 0x00be0dfc as ONE dword
//   quad[2].pad    CLUT      (GteClutBuild(0, 487))
//   quad[3].pad    tpage     (GteTpageBuild(1, 2, 384, 256))
//   quad[2..5].z   the four UV pairs: (0x51,0xC8) (0x6B,0xC8) (0x51,0xE5) (0x6B,0xE5)
//   quad[0xB..0xE] the four corners in the XZ plane, y = 0:
//                    (-halfW, 0, +halfH) (+halfW, 0, +halfH)
//                    (-halfW, 0, -halfH) (+halfW, 0, -halfH)
//                  Byte offsets 0x58/0x60/0x68/0x70 - exactly the offsets
//                  BillboardAdjSize and BillboardSetSize patch.
//   quad[6..0xA]   a 40-byte copy of quad[1..5]: the second half of the quad.
//
// One deliberate difference: the original pairs each corner's .z with an
// uninitialised stack word, so .pad receives garbage. Nothing reads it; zero is
// written here instead of reproducing indeterminate values.
// ============================================================================
void FUN_004565f0(SVECTOR* pos, SVECTOR* quad, int halfW, int halfH)
{
    // 0x004565f3: single dword store of the tint scratch into quad[1].z/.pad
    *(unsigned int*)&quad[1].z = g_animFrameIdSave;

    GteSpriteHeaderInit(&quad[1]);

    quad[3].pad = (short)GteTpageBuild(1, 2, 384, 256);
    quad[2].pad = (short)GteClutBuild(0, 487);

    unsigned char* uv = (unsigned char*)quad;
    uv[0x14] = 0x51;  uv[0x15] = 0xC8;   // quad[2].z
    uv[0x1C] = 0x6B;  uv[0x1D] = 0xC8;   // quad[3].z
    uv[0x24] = 0x51;  uv[0x25] = 0xE5;   // quad[4].z
    uv[0x2C] = 0x6B;  uv[0x2D] = 0xE5;   // quad[5].z

    short w = (short)halfW;
    short h = (short)halfH;

    quad[0xB].x = (short)-w;  quad[0xB].y = 0;  quad[0xB].z =  h;  quad[0xB].pad = 0;
    quad[0xC].x =         w;  quad[0xC].y = 0;  quad[0xC].z =  h;  quad[0xC].pad = 0;
    quad[0xD].x = (short)-w;  quad[0xD].y = 0;  quad[0xD].z = (short)-h; quad[0xD].pad = 0;
    quad[0xE].x =         w;  quad[0xE].y = 0;  quad[0xE].z = (short)-h; quad[0xE].pad = 0;

    quad[0] = *pos;

    memcpy(&quad[6], &quad[1], 40);
}

// ============================================================================
// FUN_0048ae00 @ 0x0048ae00
// "Can the player be hit here" test used by monster attack behaviours
// (enemy-type-9 states 0x00439ad0, tyrant claw 0x00422c70, hunters 0x00417xxx,
// Yawn 0x00405e00 - none of those state machines are ported yet).
//
// Composes the joint's world matrix with a translation of `pos`, then tests
// whether the player is within `radius` of the composed point on BOTH
// horizontal axes - a square reach box, not a circle. Callers pass either a
// zero vector (tyrant claw: test against the joint's own world position) or
// an offset along the joint's local X axis (Yawn bite: (1000,0,0), the point
// 1000 units in front of the head). Returns 1 when both axes hit, 0 otherwise.
//
// The old stub returned 0 unconditionally, so no monster attack could ever
// connect; changing the parameter types at the same time is safe because no
// ported caller existed (see the overload trap note in CharacterNpc.h).
// ============================================================================
unsigned char FUN_0048ae00(MATRIX* jointMtx, VECTOR* pos, short radius, int* playerT)
{
    // 0x0048ae03-0x0048ae14: g_matrixScratch = g_identityMatrixData (8 dwords)
    for (int i = 0; i < 8; i++) {
        ((int*)&g_matrixScratch)[i] = ((int*)&g_identityMatrixData)[i];
    }

    g_matrixScratch.t[0] = pos->x;
    g_matrixScratch.t[1] = pos->y;
    g_matrixScratch.t[2] = pos->z;

    MATRIX local;
    ApplyLVAndMul0Matrix(jointMtx, &g_matrixScratch, &local);

    // Both subtractions run in 16-bit ALU (SUB AX / ADD AX before the MOVZX),
    // so each sum truncates through (unsigned short); the compare against
    // radius*2 is then signed. Both quirks are load-bearing for exact parity.
    int dx = (unsigned short)((short)playerT[0] - (short)local.t[0] + radius);
    if (radius * 2 < dx) return 0;

    int dz = (unsigned short)((short)playerT[2] - (short)local.t[2] + radius);
    return (unsigned char)(dz <= radius * 2);
}

// ============================================================================
// Zone-graph pathfinding scratch (0x00be0ee0-0x00be0f03), used by zone_path_find
// and the two walkers below. One contiguous block in the original, with the
// byte arrays overlapping by one byte (0xbe0ee0[i] also reads as
// 0xbe0edf[i+1], which the walkers use to reach the previous step's zone);
// the port spells that out as explicit [i-1] indices instead. Sizes match the
// original block, except the short pairs which are grown from the original
// two steps so longer paths do not run off the end.
// ============================================================================
unsigned char g_zonePathIdx[0x10] = {};    // 0x00be0ee0 - zones on the current walk
unsigned char g_zonePathDir[0x10] = {};    // 0x00be0ee1 - per-step scan bound
unsigned char g_zonePathBest[0x0C] = {};   // 0x00be0ef0 - best-path zones
short         g_zonePathStep[16][2] = {};  // 0x00be0efc - per-step walk X/Z (stride 4)
short         g_zonePathPrev[16][2] = {};  // 0x00be0f00 - per-step previous-position X/Z

// ============================================================================
// walk_zone_find (0x00460230)
// Which zone of the RDT walk_zones grid contains (x, z)? Entry layout (verified
// against the shipped RDTs): {x1, z1, x2, z2, field, flags} at a 0xC-byte
// stride, count byte at the table base; the test is x in [x1, x2), z in
// [z1, z2). On a hit the record's +8/+10 fields land in
// g_playerDisplacement / player_distance_z (a side effect the zombie path
// never reads back - walk_zone_shared_edge overwrites them) and the zone index is
// returned. Returns 0xFF when the point is outside every zone.
// ============================================================================
unsigned int walk_zone_find(short x, short z)
{
    unsigned char* zoneBase = g_RdtPointer->walk_zones;
    unsigned char count = *zoneBase;

    int i = count - 1;
    while (i >= 0) {
        unsigned char* entry = zoneBase + 2 + i * 0xC;
        short x1 = *(short*)(entry + 0);
        short z1 = *(short*)(entry + 2);
        short x2 = *(short*)(entry + 4);
        short z2 = *(short*)(entry + 6);
        if ((unsigned short)(x - x1) < (unsigned short)(x2 - x1) &&
            (unsigned short)(z - z1) < (unsigned short)(z2 - z1)) {
            g_playerDisplacement = (unsigned int)*(unsigned short*)(entry + 8);
            player_distance_z = (unsigned int)*(unsigned short*)(entry + 10);
            return (unsigned int)i;
        }
        i--;
    }
    return 0xFF;
}

// ============================================================================
// walk_zone_shared_edge (0x004602b0)
// Midpoint of the shared edge between two adjacent zones (given as zone
// indices), into g_playerDisplacement / player_distance_z. Zones sharing an
// X edge get the midpoint of the overlapping Z span and vice versa.
// Returns the edge orientation flag the original leaves in AL: 0 when the
// shared edge runs along X (g_playerDisplacement is the crossing x), 1 when
// it runs along Z or the zones are not adjacent. The state-9 walk heading
// (npc_walk_choose_heading) branches on it.
// ============================================================================
unsigned char walk_zone_shared_edge(unsigned int zoneA, unsigned int zoneB)
{
    unsigned short* a = (unsigned short*)(g_RdtPointer->walk_zones + 2 + (zoneA & 0xFFFF) * 0xC);
    unsigned short* b = (unsigned short*)(g_RdtPointer->walk_zones + 2 + (zoneB & 0xFFFF) * 0xC);

    if (b[2] == a[0]) {                     // B sits left of A, sharing x = a.x1
        g_playerDisplacement = (unsigned int)a[0];
    } else if (a[2] == b[0]) {              // A sits left of B, sharing x = b.x1
        g_playerDisplacement = (unsigned int)b[0];
    } else {                                // shared along Z (or not at all)
        if (b[3] == a[1]) {
            player_distance_z = (unsigned int)a[1];   // B above A, shared z = a.z1
        } else if (a[3] == b[1]) {
            player_distance_z = (unsigned int)b[1];   // A above B, shared z = b.z1
        }

        unsigned short lo = a[0] > b[0] ? a[0] : b[0];
        unsigned short hi = a[2] < b[2] ? a[2] : b[2];
        g_playerDisplacement = (unsigned int)((lo + hi) >> 1);
        return 1;
    }

    unsigned short lo = a[1] > b[1] ? a[1] : b[1];
    unsigned short hi = a[3] < b[3] ? a[3] : b[3];
    player_distance_z = (unsigned int)((lo + hi) >> 1);
    return 0;
}

// ============================================================================
// zone_walk_ccw (0x0045fdb0) / zone_walk_cw (0x0045fae0)
// The two zone-graph walks behind zone_path_find. Starting from
// g_zonePathIdx[0] (the entity's zone, set by the caller), each probes
// adjacent zones - descending (ccw) or ascending (cw) index order - until the
// target's zone is reached, keeping the shortest total path in
// g_zonePathBest. `zoneStart` is unused inside both: the state comes entirely
// from the scratch arrays, exactly like the original. Returns the first
// step's zone index, or 0xFF when no path exists.
//
// The original's candidate scans were not monotone: `1 << (z & 0x1f)` aliases
// zone indices past the table (z = 34, 224+b, ...), and the byte wrap let a
// backtrack re-probe a candidate it had already consumed - which reads the
// RDT data past the zone table and, with the port's different memory layout,
// loops forever (a zombie chase froze on ROOM1010/1030/1050/...). The scans
// here are bounded to the valid zone range 0..count-1 and never wrap, so each
// position's candidates are consumed monotonically and the walk terminates;
// the CCW walk additionally marks fresh positions (g_zonePathIdx = count) so
// its descending scan starts at the top of the range instead of wrapping.
//
// THE GOAL-STEP ALIASING (fixed 2026-08-11): the original's scratch arrays
// overlap - idx[1] IS dir[0] - so when the target is directly adjacent to the
// start zone (goal at step 0), the goal branch's `best[1] = idx[1]` reads the
// target zone the branch just wrote into dir[0]. The split arrays lost that
// and returned a STALE leftover from the previous walk as the first step; the
// follow-the-player behaviours then steered at a point from an old path and
// zigzagged between zones. The record loop now spells the aliasing out
// (best[step+1] = zoneTarget). Verified with tools/sim_zone_walk.py, which
// previously compared the original model against a HARDCODED start zone 0 -
// that is fixed too, and the original now agrees with the port on every
// direct-adjacency pair in every shipped RDT. The remaining divergences are
// the long-way-around-ring cases, where the original's wrapping scan finds a
// shorter route through zones past the table; the bounded scan returns a
// longer valid path or 0xFF (the caller falls back to the direct heading) -
// both still reach the target zone.
// ============================================================================
static unsigned char zone_walk_ccw(unsigned int zoneStart, unsigned char zoneTarget,
                                   short targetX, short targetZ)
{
    unsigned char* zoneBase = g_RdtPointer->walk_zones;
    unsigned char count = *zoneBase;

    // Fresh-position marker: every position 1..15 starts as "never probed"
    // (an invalid zone index). The original relied on the 0 -> 255 byte wrap
    // of its candidate scan here, which aliased into zone indices past the
    // table and walked off it; the marker gives the descending scan a clean
    // top (count-1) to start from and a clean exhaustion test, so a
    // backtrack can never re-probe a consumed candidate.
    for (int k = 1; k < 0x10; k++) g_zonePathIdx[k] = count;

    int step = 0;
    unsigned int best = 0xFFFFFFFF;
    unsigned int dist = 0;

    for (;;) {
        int i = step;

        // The zone's adjacency bitmask (record +10, verified against the RDTs).
        unsigned short flags = *(unsigned short*)(zoneBase + 0xC
                                + (unsigned int)g_zonePathIdx[i] * 0xC);

        if ((flags & (1u << (zoneTarget & 0x1F))) == 0) {
            // ---- target not adjacent: extend the walk ----
            if ((flags & ((1u << (g_zonePathDir[i] & 0x1F)) - 1u)) == 0) {
                // Dead end: no untried flag bits below the scan bound.
                if (i != 0) {
                    int dz = (int)g_zonePathStep[i][1] - (int)g_zonePathPrev[i][1];
                    int dx = (int)g_zonePathStep[i][0] - (int)g_zonePathPrev[i][0];
                    dist -= (unsigned int)SquareRoot0(dz * dz + dx * dx);
                }
                goto backtrack;
            }

            int newLen = step + 1;
            if (newLen >= 0x10) {
                // Longer than the scratch arrays hold: keep the best so far.
                if (best == 0xFFFFFFFF) return 0xFF;
                return g_zonePathBest[1];
            }
            step = newLen;   // the original folds this into the scan-loop condition

            // Monotone descending scan of the valid zones 0..count-1: each
            // probe is idx-1, and a probe that leaves the range means every
            // candidate at this position has been tried. The scan never
            // wraps, so a backtrack cannot re-probe a consumed candidate
            // (the original's 0 -> 255 wrap did exactly that and cycled).
            unsigned char zone = 0;
            int matched = 0;
            for (;;) {
                zone = (unsigned char)(g_zonePathIdx[newLen] - 1);
                if (zone >= count) break;      // past the last valid zone
                unsigned int bit = 1u << (zone & 0x1F);
                g_zonePathIdx[newLen] = zone;
                if (flags & bit) { matched = 1; break; }
            }
            if (!matched) {
                // Exhausted: no candidate at this step. Dead end - backtrack
                // past it (step = newLen-1 is the step whose extension failed).
                if (i != 0) {
                    int dz = (int)g_zonePathStep[i][1] - (int)g_zonePathPrev[i][1];
                    int dx = (int)g_zonePathStep[i][0] - (int)g_zonePathPrev[i][0];
                    dist -= (unsigned int)SquareRoot0(dz * dz + dx * dx);
                }
                step = newLen - 2;
                goto exit_check;
            }

            // A zone already on the path: truncate there instead of looping.
            for (int back = step - 1; back >= 0; back--) {
                if (g_zonePathIdx[back] == zone) goto backtrack;
            }

            walk_zone_shared_edge(g_zonePathIdx[newLen], g_zonePathIdx[newLen - 1]);
            dist += (unsigned int)SquareRoot0(
                (g_zonePathStep[newLen][0] - g_playerDisplacement) * (g_zonePathStep[newLen][0] - g_playerDisplacement) +
                (g_zonePathStep[newLen][1] - player_distance_z) * (g_zonePathStep[newLen][1] - player_distance_z));
            if (best <= dist) {
                dist -= (unsigned int)SquareRoot0(
                    (g_zonePathStep[newLen][0] - g_playerDisplacement) * (g_zonePathStep[newLen][0] - g_playerDisplacement) +
                    (g_zonePathStep[newLen][1] - player_distance_z) * (g_zonePathStep[newLen][1] - player_distance_z));
                goto backtrack;
            }

            g_zonePathPrev[newLen][0] = (short)g_playerDisplacement;
            g_zonePathPrev[newLen][1] = (short)player_distance_z;
            g_zonePathDir[newLen] = count;
            continue;   // walk advanced - no backtrack this frame
        }

        // ---- target adjacent: goal step ----
        g_zonePathDir[i] = zoneTarget;
        walk_zone_shared_edge(g_zonePathIdx[i], zoneTarget);
        dist += (unsigned int)SquareRoot0(
            (g_zonePathPrev[i][1] - player_distance_z) * (g_zonePathPrev[i][1] - player_distance_z) +
            (g_zonePathPrev[i][0] - g_playerDisplacement) * (g_zonePathPrev[i][0] - g_playerDisplacement));
        dist += (unsigned int)SquareRoot0(
            (player_distance_z - targetZ) * (player_distance_z - targetZ) +
            (g_playerDisplacement - targetX) * (g_playerDisplacement - targetX));
        if (dist < best) {
            int n = step + 1;
            do {
                // The original's scratch arrays overlap: idx[n] aliases
                // dir[n-1], and the goal write above just set dir[i] to
                // zoneTarget - so best[step+1] comes out as the target zone
                // itself. Once the arrays were split, reading g_zonePathIdx[n]
                // here returned a STALE leftover from the previous walk - the
                // walker then steered at a point from an old path, and the
                // follow behaviours zigzagged. Spell the aliasing out.
                g_zonePathBest[n] = (n == step + 1) ? zoneTarget : g_zonePathIdx[n];
                best = dist;
                n--;
            } while (n != 0);
        }
        if (step != 0) {
            dist -= (unsigned int)SquareRoot0(
                (player_distance_z - targetZ) * (player_distance_z - targetZ) +
                (g_playerDisplacement - targetX) * (g_playerDisplacement - targetX));
            dist -= (unsigned int)SquareRoot0(
                (g_zonePathPrev[i][1] - player_distance_z) * (g_zonePathPrev[i][1] - player_distance_z) +
                (g_zonePathPrev[i][0] - g_playerDisplacement) * (g_zonePathPrev[i][0] - g_playerDisplacement));
            int dz = (int)g_zonePathStep[i][1] - (int)g_zonePathPrev[i][1];
            int dx = (int)g_zonePathStep[i][0] - (int)g_zonePathPrev[i][0];
            dist -= (unsigned int)SquareRoot0(dz * dz + dx * dx);
        }

backtrack:
        step--;
exit_check:
        if (step < 0) {
            if (best == 0xFFFFFFFF) return 0xFF;
            return g_zonePathBest[1];
        }
    }
}

static unsigned char zone_walk_cw(unsigned int zoneStart, unsigned char zoneTarget,
                                  short targetX, short targetZ)
{
    unsigned char* zoneBase = g_RdtPointer->walk_zones;
    unsigned char count = *zoneBase;

    int step = 0;
    unsigned int best = 0xFFFFFFFF;
    unsigned int dist = 0;

    for (;;) {
        int i = step;

        unsigned short flags = *(unsigned short*)(zoneBase + 0xC
                                + (unsigned int)g_zonePathIdx[i] * 0xC);

        if ((flags & (1u << (zoneTarget & 0x1F))) == 0) {
            // ---- target not adjacent: extend the walk ----
            if ((flags & ~((1u << ((g_zonePathDir[i] + 1) & 0x1F)) - 1u)) == 0) {
                // Dead end: no untried flag bits above the scan bound.
                if (i != 0) {
                    int dz = (int)g_zonePathStep[i][1] - (int)g_zonePathPrev[i][1];
                    int dx = (int)g_zonePathStep[i][0] - (int)g_zonePathPrev[i][0];
                    dist -= (unsigned int)SquareRoot0(dz * dz + dx * dx);
                }
                goto backtrack;
            }

            int newLen = step + 1;
            if (newLen >= 0x10) {
                // Longer than the scratch arrays hold: keep the best so far.
                if (best == 0xFFFFFFFF) return 0xFF;
                return g_zonePathBest[1];
            }
            step = newLen;

            // Monotone ascending scan of the valid zones: each probe is
            // idx+1 (a fresh position starts at 1, so zone 0 is skipped like
            // the original's scan), and a probe that leaves 0..count-1 means
            // every candidate here has been tried. The scan never wraps, so a
            // backtrack cannot re-probe a consumed candidate.
            unsigned char zone = 0;
            int matched = 0;
            for (;;) {
                zone = (unsigned char)(g_zonePathIdx[newLen] + 1);
                if (zone >= count) break;      // past the last valid zone
                unsigned int bit = 1u << (zone & 0x1F);
                g_zonePathIdx[newLen] = zone;
                if (flags & bit) { matched = 1; break; }
            }
            if (!matched) {
                // Exhausted: no candidate at this step. Dead end - backtrack
                // past it (step = newLen-1 is the step whose extension failed).
                if (i != 0) {
                    int dz = (int)g_zonePathStep[i][1] - (int)g_zonePathPrev[i][1];
                    int dx = (int)g_zonePathStep[i][0] - (int)g_zonePathPrev[i][0];
                    dist -= (unsigned int)SquareRoot0(dz * dz + dx * dx);
                }
                step = newLen - 2;
                goto exit_check;
            }

            // A zone already on the path: truncate there instead of looping.
            for (int back = step - 1; back >= 0; back--) {
                if (g_zonePathIdx[back] == zone) goto backtrack;
            }

            walk_zone_shared_edge(g_zonePathIdx[newLen], g_zonePathIdx[newLen - 1]);
            dist += (unsigned int)SquareRoot0(
                (g_zonePathStep[newLen][1] - player_distance_z) * (g_zonePathStep[newLen][1] - player_distance_z) +
                (g_zonePathStep[newLen][0] - g_playerDisplacement) * (g_zonePathStep[newLen][0] - g_playerDisplacement));
            if (best <= dist) {
                dist -= (unsigned int)SquareRoot0(
                    (g_zonePathStep[newLen][1] - player_distance_z) * (g_zonePathStep[newLen][1] - player_distance_z) +
                    (g_zonePathStep[newLen][0] - g_playerDisplacement) * (g_zonePathStep[newLen][0] - g_playerDisplacement));
                goto backtrack;
            }

            g_zonePathStep[newLen][0] = (short)g_playerDisplacement;
            g_zonePathStep[newLen][1] = (short)player_distance_z;
            g_zonePathDir[newLen] = 0xFF;
            continue;
        }

        // ---- target adjacent: goal step ----
        g_zonePathDir[i] = zoneTarget;
        walk_zone_shared_edge(g_zonePathIdx[i], zoneTarget);
        dist += (unsigned int)SquareRoot0(
            (g_zonePathPrev[i][1] - player_distance_z) * (g_zonePathPrev[i][1] - player_distance_z) +
            (g_zonePathPrev[i][0] - g_playerDisplacement) * (g_zonePathPrev[i][0] - g_playerDisplacement));
        dist += (unsigned int)SquareRoot0(
            (player_distance_z - targetZ) * (player_distance_z - targetZ) +
            (g_playerDisplacement - targetX) * (g_playerDisplacement - targetX));
        if (dist < best) {
            int n = step + 1;
            do {
                // The original's scratch arrays overlap: idx[n] aliases
                // dir[n-1], and the goal write above just set dir[i] to
                // zoneTarget - so best[step+1] comes out as the target zone
                // itself. Once the arrays were split, reading g_zonePathIdx[n]
                // here returned a STALE leftover from the previous walk - the
                // walker then steered at a point from an old path, and the
                // follow behaviours zigzagged. Spell the aliasing out.
                g_zonePathBest[n] = (n == step + 1) ? zoneTarget : g_zonePathIdx[n];
                best = dist;
                n--;
            } while (n != 0);
        }
        if (step != 0) {
            dist -= (unsigned int)SquareRoot0(
                (player_distance_z - targetZ) * (player_distance_z - targetZ) +
                (g_playerDisplacement - targetX) * (g_playerDisplacement - targetX));
            dist -= (unsigned int)SquareRoot0(
                (g_zonePathPrev[i][1] - player_distance_z) * (g_zonePathPrev[i][1] - player_distance_z) +
                (g_zonePathPrev[i][0] - g_playerDisplacement) * (g_zonePathPrev[i][0] - g_playerDisplacement));
            int dz = (int)g_zonePathStep[i][1] - (int)g_zonePathPrev[i][1];
            int dx = (int)g_zonePathStep[i][0] - (int)g_zonePathPrev[i][0];
            dist -= (unsigned int)SquareRoot0(dz * dz + dx * dx);
        }

backtrack:
        step--;
exit_check:
        if (step < 0) {
            if (best == 0xFFFFFFFF) return 0xFF;
            return g_zonePathBest[1];
        }
    }
}

// ============================================================================
// zone_path_find (0x0045f970)
// Zone-graph waypoint recompute - the chase-target updater behind
// zombie_update_player_distance. Locates the entity's zone and the target's
// zone in the RDT+0x58 grid, then either hands back the target point (same
// zone) or runs the CW/CCW graph walk and returns the midpoint of the first
// path segment. Returns the target zone (bit 4 set when taken directly), the
// first step's zone after a walk, or 0xFF when no path exists.
// ============================================================================
unsigned char zone_path_find(int pos1_x, int pos1_z, int* pos2_x, int* pos2_z)
{
    unsigned char* zoneBase = g_RdtPointer->walk_zones;
    unsigned char count = *zoneBase;

    unsigned char startZone = (unsigned char)walk_zone_find(
        (short)ENTITY->scaMatrixData.localMatrix.t[0],
        (short)ENTITY->scaMatrixData.localMatrix.t[2]);
    unsigned short targetZ = (unsigned short)pos1_z;
    unsigned char targetZone;

    if ((short)pos1_z == 0) {
        // z == 0: pos1_x is a zone index - the target is that zone's midpoint.
        targetZone = (unsigned char)((unsigned int)pos1_x & 0xFF);
        unsigned char* e = zoneBase + ((unsigned int)pos1_x & 0xFF) * 0xC;
        pos1_x = ((int)*(unsigned short*)(e + 2) + (int)*(unsigned short*)(e + 6)) >> 1;
        targetZ = (unsigned short)(((int)*(unsigned short*)(e + 4) + (int)*(unsigned short*)(e + 8)) >> 1);
    } else {
        targetZone = (unsigned char)walk_zone_find((short)pos1_x, (short)targetZ);
    }

    if (targetZone == startZone) {
        *(unsigned short*)pos2_x = (unsigned short)pos1_x;
        *(short*)pos2_z = (short)targetZ;
        return (unsigned char)(startZone | 0x10);
    }

    // Direction of the graph walk: the shortest way around the zone ring.
    g_zonePathDir[0] = count;
    int delta = (int)targetZone - (int)startZone;
    if (delta < 0) delta += (int)count;
    char dir = (char)(((count >> 1) < delta) * 2 - 1);
    if (startZone == 0) dir = (char)(~dir + 1);

    g_zonePathPrev[0][0] = (short)ENTITY->scaMatrixData.localMatrix.t[0];
    g_zonePathPrev[0][1] = (short)ENTITY->scaMatrixData.localMatrix.t[2];
    g_zonePathIdx[0] = startZone;
    g_zonePathBest[0] = startZone;

    unsigned char firstStep;
    if (dir < 1) {
        firstStep = zone_walk_ccw(startZone, targetZone, (short)pos1_x, (short)targetZ);
    } else {
        g_zonePathDir[0] = 0xFF;
        firstStep = zone_walk_cw(startZone, targetZone, (short)pos1_x, (short)targetZ);
    }
    if (firstStep == 0xFF) return 0xFF;

    walk_zone_shared_edge(startZone, firstStep);
    *(short*)pos2_x = (short)g_playerDisplacement;
    *(short*)pos2_z = (short)player_distance_z;
    return firstStep;
}

// ============================================================================
// is_facing_toward_entity (0x0048a040)
// 1 when the subject's direction (its yaw at +0x74) is within +-0x800 of
// ENTITY's. Feeds zombie_attack's attacking_direction (directions 1 and 3 =
// "player facing the zombie").
// ============================================================================
unsigned int is_facing_toward_entity(void* player)
{
    int diff = (int)*(short*)((char*)player + 0x74) - (int)ENTITY->angle;
    return ((unsigned int)(diff + 0x400) & 0xFFF) < 0x800;
}

// ============================================================================
// FUN_0040a380 (0x0040a380)
// Squares each component - the magnitude helper behind zombie_body_part_physics.
// ============================================================================
void FUN_0040a380(VECTOR* v0, VECTOR* v1)
{
    v1->x = v0->x * v0->x;
    v1->y = v0->y * v0->y;
    v1->z = v0->z * v0->z;
}

// ============================================================================
// reduce_attack_time_by_btn_press (0x00437fb0)
// How much the player shortens the zombie bite by mashing: 3 while directional
// controls (pad byte 1) are held, +2 for buttons (pad byte 0). Subtracted
// from ATTACK_TIMER every frame of zombie_attack case 3.
// ============================================================================
char reduce_attack_time_by_btn_press(void)
{
    char reduce = 0;
    if (((g_PlayerPadHeld >> 8) & 0xF0) != 0) reduce = 3;
    if ((g_PlayerPadHeld & 0xF0) != 0) reduce += 2;
    return reduce;
}

// ============================================================================
// joint_enable_special_effect (0x0048a140)
// Marks a joint as effect-enabled: clears bit 0 and sets 0x28 on its flags,
// then reports the animation slot displacement. The original also dispatches
// an async TMD tint (FUN_004855d0 -> the 0x004850d0 worker), which is what
// allocates the trail/attack-effect slot and builds the gore-spurt strip
// geometry (port: PathTrail.cpp). That pipeline is now ported, so both halves
// run.
// ============================================================================
void joint_enable_special_effect(int joint, unsigned char a, int b, unsigned char c)
{
    unsigned char* flags = (unsigned char*)joint;
    if ((*flags & 1) != 0) {
        unsigned char f = *flags & 0xFE;
        *flags = f;
        *flags = f | 0x28;
        g_playerDisplacement = *(int*)(*(int*)(joint + 0x14) + 0x14) * 2;
        extern void FUN_004855d0(void*, int, int, int);
        FUN_004855d0(*(void**)(joint + 0x18), a, (int)b, (int)c);
    }
}

// ============================================================================
// set_next_entity_data_buffer (0x00457070)
// Advances the entity data load cursor by `count` 0x78-byte slots. Called from
// zombie_init to step past the shadow quad buffer. (The old comment claimed
// 0x00488f90; that is the queue allocator, a different function.)
// ============================================================================
void set_next_entity_data_buffer(int count)
{
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + (unsigned int)count * 0x78;
}

// ============================================================================
// The mirror (planar reflection) pass - entity_build_mirror_joints and
// entity_draw_mirror_reflection.
//
// A room configures the mirror through SCD opcode 0x0F (cmd_mirror_set,
// CmdFunctions.cpp), which writes four values:
//
//   g_main_state_flags bits 0-1 - bit 0 = mirror active, bit 1 = plane axis
//                                 (0 = the plane is Z = k, 1 = the plane is X = k)
//   g_mirrorPlaneCoord               - k, the mirror plane coordinate
//   g_mirrorExtentMin / g_mirrorExtentMax - the mirror's extent along the OTHER axis
//
// ROOM1120 (plane Z) issues `0f 01 04 10 10 27 44 16` - bit 0 and bit 1 in one
// go: on, plane Z = 5700, span X = 4100..10000.
//
// The plane-X rooms need TWO commands, because the axis bit alone leaves bit 0
// clear. ROOM1110 issues:
//
//   0f 02 a0 0f 9c 18 d4 30    ; axis X, plane X = 12500, span Z = 4000..6300
//   05 05 1f 00                ; bit_op bank 5, OR, mask 0x80000000 >> 31 = bit 0
//
// That second command is easy to misread. cmd_bit_op counts its bit index from
// the MSB (`mask = 0x80000000U >> bitIndex`), so index 31 is bit 0 - the enable.
// It also has no immediate operand, so searching the exe for `OR [flags], 1`
// finds nothing and the enable looks absent. Ten rooms use the mirror:
//
//   plane Z, flags 0x01 in one command  - 1120/1121, 1130/1131, 6120/6121, 6130/6131
//   plane X, flags 0x02 + a bit_op      - 1110/1111, 40B0/40B1, 6110/6111
//
// Once bit 0 is up, update_entities and update_player_anim call
// entity_draw_mirror_reflection after the normal draw. It reflects the room
// camera about the plane, flips the handedness, and submits the entity a SECOND
// time - that second submission is the reflection. mirror_point_visible decides
// per joint whether the segment from the camera to that joint actually crosses
// the mirror rectangle.
//
// Nothing here is menu-related, despite what the older comments on the
// effect-side twin (effect_draw_mirror_reflection, EffectSystem.cpp) claimed:
// bits 0-1 of g_main_state_flags have exactly one writer, and it is the room
// script.
//
// The enable gate is bit 0 only: update_entities loads EBX = 1 at 0x0048f10c
// and does TEST [0x00be41c0],EBX at 0x0048f132; update_player_anim and both
// update_2d_effects sites use an immediate TEST byte ptr [0x00be41c0],0x1.
// Nothing anywhere tests bit 1 - it is read only as the axis argument, inside
// the bit-0-gated blocks.
// ============================================================================

// ============================================================================
// entity_build_mirror_joints (0x0048c0d0) - build the reflected copy of the joint array.
//
// Walks the joints from the last one down to joint 0 (jointCount iterations,
// stride 0x7C) and copies two things out of the live array at +0x98 into the
// mirror array at +0xAC: the flag byte (+0x00) and the composite world matrix
// (+0x44, 8 dwords). Everything else in the mirror array was already filled in
// by SetupEntityJointAnimation, so only the per-frame transform needs refreshing.
//
// A joint whose flags carry 0x40 is then probed: mirror_point_visible against the
// joint's world translation (+0x58, which is world.t and therefore inside the
// block just copied) sets or clears bit 0 of the COPY's flag byte. Bit 0 is
// what render_entity's `(jointFlags & 1)` gate tests, so bit 0 means
// "draw this joint in the reflection". It is forced clear again when the source
// joint is not being drawn at all, so a hidden joint can never show up in the
// mirror.
//
// The copy exists because render_entity WRITES into the joints it draws
// (the `jointFlags & 4` branch patches m[0][2], m[1][0], m[1][1]); running the
// reflection pass over the live array would corrupt the next real frame.
// ============================================================================
void entity_build_mirror_joints(void)
{
    unsigned char count = ENTITY->jointCount;

    // Guards, not in the original: its DEC/JNZ on a byte counter would run 256
    // times for a zero count, and the mirror array is only allocated once the
    // entity's joints have been set up.
    if (count == 0 || ENTITY->jointsStructs == NULL || ENTITY->weaponJointsPtr == 0) {
        return;
    }

    unsigned char* src = (unsigned char*)ENTITY->jointsStructs
                         + (unsigned int)count * 0x7c - 0x7c;
    unsigned char* dst = (unsigned char*)ENTITY->weaponJointsPtr
                         + (unsigned int)count * 0x7c - 0x7c;

    do {
        JointStruct* s = (JointStruct*)src;
        JointStruct* d = (JointStruct*)dst;

        // 0x0048c10d-0x0048c11d: flag byte, then the 8-dword world matrix.
        d->flags = s->flags;
        d->world = s->world;

        // 0x0048c11f: only joints marked 0x40 are mirror-tested.
        if ((d->flags & 0x40) != 0) {
            unsigned char visible = mirror_point_visible(
                (void*)((char*)g_RdtPointer + 0x9c
                        + (unsigned int)g_roomCameraId * 0x2c),
                (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
                (int)d->world.t);

            if (visible != 0) {
                d->flags |= 0x01;
            } else {
                d->flags &= 0xfe;
            }

            // 0x0048c169: and never mirror a joint the live pass is hiding.
            if ((s->flags & 0x01) == 0) {
                d->flags &= 0xfe;
            }
        }

        src -= 0x7c;
        dst -= 0x7c;
        count--;
    } while (count != 0);
}

// ============================================================================
// entity_draw_mirror_reflection (0x0048bda0) - draw the entity's reflection.
//
// Called from update_entities (0x0048f177) and update_player_anim (0x00494ea5),
// both already gated on g_main_state_flags bit 0 - those two sites are the
// function's only callers.
//
//   1. entity_build_mirror_joints refreshes the mirror joint array and its visibility bits.
//   2. ENTITY->jointsStructs (+0x98) is pointed at that array, the real pointer
//      parked in g_tempVar.
//   3. FlipSprite reflects the RDT camera record - the pointer is aimed at the
//      camera's posX, so its dwords are posX/posY/posZ/toX/toY/toZ/roll/light -
//      about the mirror plane, folding both the eye and the look-at target.
//   4. MatrixToCamera installs it; composing an identity with a negated m[0][0]
//      into g_RoomCameraData flips the handedness the reflection introduced.
//   5. render_entity (0x0048c350) submits the model through that camera.
//   6. The real camera and joint pointer are restored.
//
// ENTITY is re-read from the global at each step, as the original does.
// ============================================================================
void entity_draw_mirror_reflection(void)
{
    MATRIX mirrorCam;

    entity_build_mirror_joints();

    // 0x0048bdaa-0x0048bdcd: swap in the mirrored joints.
    g_tempVar = (void*)ENTITY->jointsStructs;
    ENTITY->jointsStructs = (JointStruct*)ENTITY->weaponJointsPtr;

    int* camera = (int*)((char*)g_RdtPointer + 0x9c
                         + (unsigned int)g_roomCameraId * 0x2c);

    // 0x0048bdcf-0x0048be1d: reflect the camera and install it.
    FlipSprite(camera, &mirrorCam,
               (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
               g_mirrorPlaneCoord);
    MatrixToCamera(&mirrorCam);

    // 0x0048be25-0x0048be4a: negate X to undo the mirrored handedness.
    g_matrixScratch = g_identityMatrixData;
    g_matrixScratch.m[0][0] = -g_matrixScratch.m[0][0];
    Matrix_MulMatrix(&g_matrixScratch, &g_RoomCameraData);

    // 0x0048be4d: the reflection itself.
    render_entity(ENTITY);

    // 0x0048be5c-0x0048be91: restore the real camera and joint array.
    MatrixToCamera((MATRIX*)camera);
    ENTITY->jointsStructs = (JointStruct*)g_tempVar;
}

// ============================================================================
// entity_ballistic_step @ 0x004895f0
// One frame of a projectile arc for whatever entity is airborne: the cerberus's
// leaps and knockdowns, and the hunter's jump attacks (0x00417ba0, 0x00418220,
// 0x00418a50, 0x00419540, 0x00417e40).
//
// Horizontally it walks `fwdStep` units along the entity's own yaw. Vertically
// it subtracts `vy0 + airTicks * gravity` from localMatrix.t[1], where airTicks
// is the byte at Entity+0xBC - the port's `death_timer`, which every ballistic
// caller reuses as "frames spent in the air". That byte is only incremented
// while the entity is still ABOVE groundY, so the arc accelerates until it
// lands and then stops.
//
// groundY is a ceiling on t[1] in screen terms (Y grows downward), so the
// landing test is `t[1] > groundY`. Callers pass 0 for the room floor.
// ============================================================================
unsigned int entity_ballistic_step(short fwdStep, short vy0, short gravity, short groundY)
{
    SVECTOR v;
    MATRIX  m;

    // Yaw-only rotation matrix: [0x00489600-0x00489618] zeroes x/z and takes
    // .y straight from Entity+0x74.
    v.x = 0;
    v.y = ENTITY->angle;
    v.z = 0;
    RotMatrix(&v, &m);

    v.x = fwdStep;
    v.y = 0;
    v.z = 0;
    ApplyMatrixSV(&m, &v, &v);

    ENTITY->scaMatrixData.localMatrix.t[0] += (int)v.x;
    ENTITY->scaMatrixData.localMatrix.t[2] += (int)v.z;

    // `MOVZX AX,[EDX+0xbc] / IMUL AX,gravity / ADD AX,vy0` - a 16-bit multiply,
    // so it wraps at 16 bits exactly as the original does.
    short vy = (short)((short)(unsigned short)((unsigned short)ENTITY->death_timer * (unsigned short)gravity)
                       + vy0);
    ENTITY->scaMatrixData.localMatrix.t[1] -= (int)vy;

    if (ENTITY->scaMatrixData.localMatrix.t[1] > (int)groundY) {
        ENTITY->scaMatrixData.localMatrix.t[1] = (int)groundY;
        return (unsigned int)(unsigned short)vy;
    }
    ENTITY->death_timer++;
    return 0;
}

// ---------------------------------------------------------------------------
// Implemented elsewhere, listed here so the split stays legible:
//   check_room_collision             @ 0x0047d310 - RoomCollision.cpp
//   ChkOutsideCell                   @ 0x0047d270 - RoomCollision.cpp
//   room_collision_check_0047da50    @ 0x0047da50 - RoomCollision.cpp
//   room_check_sight_blocked         @ 0x0047db90 - RoomCollision.cpp
//   vectorMul3                       @ 0x0040a550 - GteMatrix.cpp
//   VectorNormal                     @ 0x0040a5c0 - GteMatrix.cpp
//   entity_add_fade_sprite           @ 0x00456810 - FadeSprite.cpp
//   BillboardSetColor                @ 0x00456710 - PlayerAnimations.cpp
// ---------------------------------------------------------------------------

// (0x0048f0f0) - Update all enemy entities per-frame
// Iterates through g_EnemiesList, calls the per-type update function from
// enemies_update_functions_tbl for each active entity, and - in a room whose
// script turned the mirror on - draws each entity's reflection.
void update_entities(void)
{
    // 0x0048f0f0-0x0048f102: Set current entity pointer to start of list
    ENTITY = g_EnemiesList;
    int em_counter = 0;

    // 0x0048f104-0x0048f10f: Only process if there are active enemies
    if (g_enemy_count == 0) {
        return;
    }

    do {
        // 0x0048f10f: Safety guard - max 30 entities (array size)
        if (em_counter > 29) {
            return;
        }

        // 0x0048f114-0x0048f12b: Only update active entities (status_flags bit 0)
        if ((ENTITY->status_flags & 0x01) != 0) {
            // 0x0048f12b: Call per-type update function from dispatch table
            void* updateFunc = enemies_update_functions_tbl[ENTITY->id];
            if (updateFunc != NULL) {
                ((void(*)())updateFunc)();
            }

            // 0x0048f131-0x0048f197: the mirror pass. Bit 0 of g_main_state_flags
            // is set only by SCD opcode 0x0F, i.e. only by a room with a mirror
            // in it. The probe asks whether the segment from the camera to this
            // entity's origin crosses the mirror rectangle; if it does,
            // entity_draw_mirror_reflection submits the entity again through
            // the reflected camera.
            if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
                unsigned char lightCheck = mirror_point_visible(
                    (void*)((int)g_RdtPointer[1].lights + (unsigned int)g_roomCameraId * 44 - 4),
                    (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
                    (int)ENTITY->scaMatrixData.localMatrix.t);
                if (lightCheck != 0) {
                    entity_draw_mirror_reflection();
                }
            }

            // 0x0048f197: Increment processed entity counter
            em_counter = em_counter + 1;
        }

        // 0x0048f19a: Advance to next entity (sizeof(Entity) = 0x18C)
        ENTITY = (Entity*)((char*)ENTITY + sizeof(Entity));

    } while (em_counter < (int)(unsigned int)g_enemy_count);
}
