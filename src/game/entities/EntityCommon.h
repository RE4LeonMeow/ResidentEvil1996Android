#pragma once
#include "../../Globals.h"

// ============================================================================
// EntityCommon.h - Declarations shared by every entity type
//
// The functions here are the entity-generic half of what used to live in
// Zombie.cpp: angle/line-of-sight helpers, SCA collision, the wander/pathfind
// state machines and the joint effect helpers. Every monster update function
// (ids 0-21) and the shared human-character driver (ids 22-47) calls into
// them, and so does the player code in PlayerAnimations.cpp - none of it is
// zombie-specific. Zombie.h now covers only the zombie state machine.
//
// The definitions are in EntityCommon.cpp.
// ============================================================================

// PS1 angle encoding: full circle = 0x1000 (4096), 180 deg = 0x800 (2048)
#define ANGLE_FULL_CIRCLE       0x1000
#define ANGLE_HALF_CIRCLE       0x0800
#define ANGLE_SEMI_TOLERANCE    0x0801  // 180 deg + 1, boundary in turn_toward_target

// Status flags (entity->status_flags bits) - shared by every entity type
#define ENTITY_STATUS_ACTIVE         0x01  // bit 0: entity is active/visible
#define ENTITY_STATUS_DEAD           0x08  // bit 3: entity is dead
#define ENTITY_STATUS_PLAYER_ABOVE   0x20  // bit 5: player above (vertical) / in visual range (distance)
#define ENTITY_STATUS_PLAYER_BELOW   0x80  // bit 7: player below (vertical) / in alert range (distance)
#define ENTITY_STATUS_ALIGNED        0x40  // bit 6: entity is aligned with player

// ============================================================================
// Entity type IDs used by enemies_update_functions_tbl.
// These are the entity->id values set by cmd_omodel_set / cmd_enemy_set in SCD
// scripts. Ids 0-21 are the monsters; 22-47 are all handled by the shared
// human-character driver in CharacterNpc.cpp.
// ============================================================================

#define ENEMY_ZOMBIE                0x00 // white coat variant
#define ENEMY_ZOMBIE_NAKED          0x01 // lab naked zombie variant
#define ENEMY_CERBERUS              0x02 // zombie dog
#define ENEMY_WEB_SPINNER           0x03 // big tarantula
#define ENEMY_BLACK_TIGER           0x04 // giant tarantula (Underground Boss)
#define ENEMY_CROW                  0x05
#define ENEMY_HUNTER                0x06
#define ENEMY_WASP                  0x07
#define ENEMY_PLANT42               0x08 // Guardhouse boss
#define ENEMY_CHIMERA               0x09 // Lab humanoid-ape like mutant
#define ENEMY_ADDER                 0x0a // regular size venomous snake
#define ENEMY_NEPTUNE               0x0b // shark
#define ENEMY_TYRANT_1              0x0c // Main Lab Tyrant battle
#define ENEMY_YAWN_1                0x0d // First battle
#define ENEMY_PLANT42_ROOTS         0x0e // not an enemy, Plant42 is weakened if v-jolt is used on it. Cannot be damaged by weapons
#define ENEMY_MONSTER_PLANT         0x0f // Plant tentacle, unkillable
#define ENEMY_TYRANT_2              0x10 // Super Tyrant mode, Final boss
#define ENEMY_ZOMBIE_VARIANT        0x11 // Green coat variant
#define ENEMY_YAWN_2                0x12 // Second battle

// Not an enemy: the web that blocks the door in room 30C0. Only the knife
// and the fire/explosive weapons can burn it away - see SpiderWeb.cpp.
#define ENEMY_SPIDER_WEB            0x13 // Second battle

// NPCs

#define NPC_CHRIS_LEFT_ARM          0x14 // chris's left arm used in lab computer mode
#define NPC_CHRIS_RIGHT_ARM         0x15 // chris's right arm used in lab computer mode

// Director's Cut only: the Forest zombie (EM1016 / EM1116), spawned by the
// arrange balcony rooms STAGE9/ROOM9120+9121 and STAGEE/ROOME120+E121 with
// behaviour 0xB - the scripted stand-up entrance in Zombie.cpp. The USA build
// leaves this a filler NPC slot; dc_apply_zombie_tables() points it at
// zombie_update in DC mode, as the DC's own STAGE2/STAGE7 entity tables do.
#define ENEMY_ZOMBIE_FOREST         0x16

#define NPC_CHRIS                   0x20
#define NPC_JILL                    0x21
#define NPC_BARRY                   0x22
#define NPC_REBECCA                 0x23
#define NPC_WESKER                  0x24
#define NPC_KENNETH_CORPSE          0x25
#define NPC_FOREST_CORPSE           0x26
#define NPC_RICHARD                 0x27
#define NPC_ENRICO                  0x28


#define NPC_ENTITIES_IDS            (ENEMY_SPIDER_WEB+1)

// ============================================================================
// enemies_update_functions_tbl @ 0x004d3c90 - entity type dispatch table
// Indexed by entity->id. 48 entries: 0-21 monsters, 22-47 the shared human
// character driver. See the definition in EntityCommon.cpp for why 32 was wrong.
// Not const: DC mode re-points slot 22 (ENEMY_ZOMBIE_FOREST) at zombie_update.
// ============================================================================
extern void* enemies_update_functions_tbl[48];

// ---- Per-type update functions referenced by the dispatch table ----
void zombie_update(void);          // 0x004338c0 - Zombie.cpp
void character_npc_update(void);   // 0x0046acf0 - CharacterNpc.cpp
void plant42_update(void);         // 0x00464d10 - Plant42.cpp
void plant42_roots_update(void);   // 0x0047e1c0 - Plant42Roots.cpp
void yawn_update(void);            // 0x004051e0 - Yawn.cpp (ids 13 and 18)
void spiderweb_update(void);       // 0x00443640 - SpiderWeb.cpp (id 19)
void computer_arm_update(void);    // 0x00427330 / 0x0040b760 - ComputerArms.cpp (ids 20, 21)
void cerberus_update(void);        // 0x00497fb0 - Cerberus.cpp (id 2)
void crow_update(void);            // 0x0042e520 - Crow.cpp (id 5)
void adder_update(void);           // 0x004727f0 - Adder.cpp (id 10)
void wasp_update(void);            // 0x0048daf0 - Wasp.cpp (id 7)
void neptune_update(void);         // 0x0043d8d0 - Neptune.cpp (id 11, the shark)
void hunter_update(void);          // 0x004161f0 - Hunter.cpp (id 6)
void chimera_update(void);         // 0x00438a70 - Chimera.cpp (id 9)
void web_spinner_update(void);     // 0x00478310 - WebSpinner.cpp (id 3, the big spider)
void black_tiger_update(void);     // 0x0044f300 - BlackTiger.cpp (id 4, the giant spider boss)

// ============================================================================
// Angle / line-of-sight helpers
// ============================================================================
extern unsigned short getAngleTowardsTarget(int px, int pz);                 // 0x00460450
extern int  turn_toward_target(VECTOR* target_pos, short angle_step);        // 0x00489960
extern void entity_rotate_toward_target(VECTOR* pos, unsigned short angleStep); // 0x004899b0
extern unsigned char check_line_of_sight(VECTOR* targetPos);                // 0x0048a4b0
extern unsigned int entity_check_angular_los(short fovHalfAngle, VECTOR* targetPos); // 0x00489c60
extern unsigned char checkAngularViewAndDistance(short fovHalfAngle, short maxDistance, VECTOR* targetPos); // 0x00489cf0

// Sets status_flags bit 0x20 / 0x80 when the player is inside `range`.
// Both RETURN the distance they computed - SquareRoot0's result is still in EAX
// at the RET, and yawn_state_check (0x004056b7) reads it back with
// `CMP EAX, 0xfa0` to decide whether to clear the "aligned" bit. Declaring
// entity_check_alert_range void made that comparison unwritable.
extern unsigned int entity_check_visual_range(unsigned int range);  // 0x0043bfa0
extern unsigned int entity_check_alert_range(unsigned int range);   // 0x0043bfe0

// ============================================================================
// Movement / pathfinding state machines
// ============================================================================
extern unsigned int entity_pathfind_update(void);                            // 0x0048ad10
extern unsigned int entity_update_wander_turn(unsigned int movement_dist,
                                              unsigned char* control_flags,
                                              unsigned char* turn_counter,
                                              unsigned short angle_step,
                                              unsigned char turn_limit);     // 0x00489800
// 0x00489a50 - obstacle-avoidance steering, defined in EntityCommon.cpp.
// Only crow_state_run calls it, but it lives in the shared 0x00489xxx helper
// block. `latch` bit 7 = idle, bits 0-6 = swerve frames left, seeded from
// g_animFrameIdSave (the caller sets it right before the call).
extern short entity_swerve_around_obstacle(short angleStep, char blocked,
                                           short* swerve, unsigned char* latch);

// ============================================================================
// SCA collision
// ============================================================================
extern void SetEntityScaHitData(Entity* ent);                                // 0x0041b2c0
extern unsigned int ResolveEntityScaCollision(Entity* a, Entity* b);         // 0x0041b0a0
extern unsigned int HandleEnemyPlayerCollisions(void);                       // 0x00489e10
// 0x0047d6f0 - two-point boundary push for a prone body: rotates each offset
// by the entity yaw, pushes at both, rolls position AND angle back on failure.
// Returns 0 = clear, 1 = pushed clear, 0x80 = still stuck (rolled back).
// Defined in RoomCollision.cpp (needs the static boundary_classify there).
extern unsigned char check_room_collision_two_point(SVECTOR* endA, SVECTOR* endB);
// check_room_collision (0x0047d310), ChkOutsideCell (0x0047d270) and
// room_check_sight_blocked (0x0047db90) live in RoomCollision.cpp and are
// declared in Globals.h. Do NOT re-declare them here: this header used to carry
// a `ChkOutsideCell(SVECTOR*, ...)` stub whose first parameter type differed from
// the real `VECTOR*` one, so it linked as a separate overload and every
// line-of-sight call in this file bound to the do-nothing version.

// ============================================================================
// Joint / effect helpers
// ============================================================================
extern void blood_splatter_physics(int jointData, short gravityStep);         // 0x00437d20
extern void snap_player_to_grab_position(void* player);                       // 0x00489ee0
// entity_apply_anim_vertex is declared in Globals.h with its real signature
// (Entity* first parameter). Do NOT redeclare it with a different parameter
// type here: that made the old void* stub an OVERLOAD, the trap this header
// documents below.
extern void joint_setup_attack_effect(int joint, unsigned char effectType,
                                      unsigned short timer, unsigned short frameMatch); // 0x0048a070
extern void joint_enable_special_effect(int joint, unsigned char a, int b, unsigned char c);
extern void FUN_004565f0(SVECTOR* pos, SVECTOR* quad, int halfW, int halfH);  // 0x004565f0 - shadow quad builder
extern void entity_add_fade_sprite(VECTOR* pos, short* velocity, short yOffset, short angle); // 0x00456810 - FadeSprite.cpp

// ============================================================================
// Zone-graph pathfinding (zone_path_find + helpers, all in EntityCommon.cpp)
// ============================================================================
extern unsigned char zone_path_find(int px, int pz, int* a, int* b);            // 0x0045f970
extern unsigned int walk_zone_find(short x, short z);                           // 0x00460230
// Returns 0 when the shared edge runs along X (g_playerDisplacement = crossing
// x), 1 when along Z or not adjacent - the flag npc_walk_choose_heading uses.
extern unsigned char walk_zone_shared_edge(unsigned int zoneA, unsigned int zoneB);    // 0x004602b0

// ============================================================================
// Joint reach hit test (0x0048ae00) - defined in EntityCommon.cpp
// Composes a joint's world matrix with a translation of `pos` (callers pass
// a zero vector for the joint's own position, or an offset along its local X
// axis), then tests both horizontal axes of the player position against
// `radius` - a square reach box, not a circle. Returns 1 when both axes hit.
// Used by the unported monster attack states (Yawn 0x00405e00, tyrant
// 0x00422c70, hunter 0x00417xxx); the zombie uses its own grab path instead.
// ============================================================================
extern unsigned char FUN_0048ae00(MATRIX* jointMtx, VECTOR* pos, short radius, int* playerT);

// ============================================================================
// entity_ballistic_step (0x004895f0) - defined in EntityCommon.cpp
// One frame of a projectile arc: `fwdStep` along the entity's own yaw, and a
// vertical velocity of `vy0 + Entity[0xBC] * gravity` subtracted from
// localMatrix.t[1], clamped at `groundY`. Entity+0xBC (the port's death_timer)
// is the airborne frame counter and only advances while the entity is still
// above the ground. Shared by the cerberus's leaps and the hunter's jumps.
// ============================================================================
extern unsigned int entity_ballistic_step(short fwdStep, short vy0,
                                          short gravity, short groundY);

// ============================================================================
// Remaining engine dependencies (still stubs pending decompilation)
// ============================================================================
extern void FUN_0040a380(VECTOR* v0, VECTOR* v1);
extern unsigned int is_facing_toward_entity(void* player);
extern char reduce_attack_time_by_btn_press(void);
extern void set_next_entity_data_buffer(int count);                           // 0x00457070
// The mirror pass. mirror_point_visible lives in EffectSystem.cpp; the other
// two are defined in EntityCommon.cpp.
extern unsigned char mirror_point_visible(void* light, unsigned char param2, int param3); // 0x0048bd00
extern void entity_draw_mirror_reflection(void);                              // 0x0048bda0
extern void entity_build_mirror_joints(void);                                 // 0x0048c0d0
extern void FlipSprite(int* src, MATRIX* dst, unsigned char mirror, unsigned int width); // 0x0048bca0 - EffectSystem.cpp
extern void Matrix_MulMatrix(MATRIX* a, MATRIX* b);                           // 0x0040a210 - EffectSystem.cpp
