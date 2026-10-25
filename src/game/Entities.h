#pragma once
#include "Types.h"

// ============================================================================
// AnimSlot (0x1c bytes each, inside EMD file data)
// After ResolveAnimPointers resolves relative offsets to absolute pointers.
// ============================================================================
#pragma pack(push, 1)
struct AnimSlot {
    void* data0;            // +0x00: pointer to animation data block 0
    int   pad_04;           // +0x04
    void* data1;            // +0x08: pointer to animation data block 1
    int   pad_0c;           // +0x0C
    void* data2;            // +0x10: pointer to animation data block 2 (TMD/texture data)
    int   entryCount;       // +0x14: number of entries in data2
    int   pad_18;           // +0x18
};
#pragma pack(pop)
static_assert(sizeof(AnimSlot) == 0x1c, "AnimSlot size mismatch");

// ============================================================================
// AnimDataHeader - Layout of the animation data header pointed to by
// Entity.modelLoadBuffer. The slots[] array starts at offset +0x0C.
// ============================================================================
#pragma pack(push, 1)
struct AnimDataHeader {
    int            unknown_00;    // +0x00
    unsigned char  resolved;      // +0x04: bit 0 = pointers resolved
    unsigned char  pad_05[3];     // +0x05
    int            slotCount;     // +0x08
    AnimSlot       slots[];       // +0x0C
};
#pragma pack(pop)

// ============================================================================
// PlayerEntity (0x00be62e4) - 0x180 bytes
// Main player entity. Shares base layout with Entity up to 0x6C.
// ============================================================================
#pragma pack(push, 1)
struct PlayerEntity {
    // ---- Header (0x00 - 0x0B) ----
    unsigned char  flags;               // 0x00
    unsigned char  id;                  // 0x01
    unsigned char  equippedWeaponId;    // 0x02
    unsigned char  zoneFlags;           // 0x03 - interaction/zone state bitfield:
                                        //        0x01 = inside current camera switch zone
                                        //        0x10 = door swings other way / ladder variant
                                        //        0x20 = inside stairs/ladder/door zone
                                        //        0x40 = door direction modifier (with angle bit 0x400)
                                        //        0x80 = grabbed/captured by enemy
    unsigned int   Sca_info;            // 0x04
    unsigned int   pSca_hit_data;       // 0x08

    // ---- SCA data + model (0x0C - 0x1F) ----
    short          unk_0c;              // 0x0C
    short          unk_0e;              // 0x0E
    unsigned char  unk_10;              // 0x10
    unsigned char  unk_11;              // 0x11
    unsigned char  unk_12;              // 0x12
    unsigned char  unk_13;              // 0x13
    unsigned int   modelLoadBuffer;     // 0x14
    unsigned int   unk_18;              // 0x18

    // ---- SCA matrix data (0x1C - 0x6B, 0x50 bytes) ----
    // Embedded ScaMatrixData struct. localMatrix at entity+0x20 is the transform matrix.
    // worldMatrix at entity+0x40. Total: field_00(4) + localMatrix(32) + worldMatrix(32)
    // + field_44(4) + owner(4) + field_4c(4) = 0x50 bytes.
    ScaMatrixData  scaMatrixData;       // 0x1C

    // ---- Position + movement (0x6C - 0x83) ----
    SVECTOR        position;            // 0x6C (8 bytes)
    short          directionAngle;      // 0x74
    SVECTOR        speed;               // 0x76 (8 bytes)
    unsigned int   unk_7e;              // 0x7E
    unsigned short unk_82;              // 0x82

    // ---- State (0x84 - 0x8F) ----
    unsigned char  animationId;         // 0x84
    unsigned char  animFrameId;         // 0x85
    unsigned char  action_behavior;      // 0x86 - animation behavior index (0=idle, 200=death timer)
    unsigned char  action_state;         // 0x87 - animation sub-state counter
    short          health;              // 0x88
    unsigned char  isBeingAttackedFlag; // 0x8A
    unsigned char  unk_8b;              // 0x8B
    unsigned char  unk_8c;              // 0x8C
    unsigned char  jointCount;          // 0x8D
    unsigned short posY;                // 0x8E - player Y height, kept in sync with position.y / localMatrix.t[1]; floor reference for effects

    // ---- Model/animation pointers (0x90 - 0xA3) ----
    unsigned int   animHeader;          // 0x90
    unsigned int   animBase;            // 0x94
    JointStruct*   jointsStructs;       // 0x98
    unsigned int   weaponPartAnimSlot;  // 0x9C
    unsigned int   weaponPartAnimObject;// 0xA0

    // ---- Padding + combat state (0xA4 - 0xCB) ----
    unsigned char  pad_a4[0x14];        // 0xA4-0xB7
    unsigned int   unk_b8;              // 0xB8
    unsigned char  unk_bc;              // 0xBC
    unsigned char  attackAnim;          // 0xBD
    unsigned char  animation_frame_id;  // 0xBE
    unsigned char  unk_bf;              // 0xBF
    unsigned char  unk_c0;              // 0xC0
    unsigned char  unk_c1;              // 0xC1
    unsigned short move_speed_current;  // 0xC2 - current translation speed during animation/behavior
    unsigned short attackDirection;     // 0xC4
    unsigned short unk_c6;              // 0xC6
    unsigned short unk_c8;              // 0xC8
    unsigned short unk_ca;              // 0xCA

    // ---- Status (0xCC - 0xE3) ----
    // ---- Look-at / head tracking (0xCC - 0xDA) ----
    // Same layout as Entity; see the Entity declarations below for the
    // lookAtFlags bit meanings.
    int            lookAtTargetX;       // 0xCC
    int            lookAtTargetY;       // 0xD0
    int            lookAtTargetZ;       // 0xD4
    unsigned char  lookAtFlags;         // 0xD8
    unsigned char  lookAtYawStep;       // 0xD9
    unsigned char  lookAtPitchStep;     // 0xDA
    unsigned char  scd_anim_param;      // 0xDB - SCD event animation parameter;
                                        //        the player's pose-completion code
                                        //        raises Flg_on(g_SysFlags, this) when
                                        //        an SCD-driven animation ends
    unsigned char  healthStatusFlags;   // 0xDC
    unsigned char  lookAtJointIdx;      // 0xDD
    unsigned short unk_de;              // 0xDE
    unsigned short unk_e0;              // 0xE0
    unsigned short attackTimer;         // 0xE2

    // ---- Physics (0xE4 - 0xEB) ----
    SVECTOR        pushVelocity;        // 0xE4

    // ---- Large padding (0xEC - 0x15B) ----
    unsigned char  pad_ec[0x70];        // 0xEC-0x15B

    // ---- Joint movement data (0x15C - 0x173) ----
    unsigned int   jointMoveData0;      // 0x15C
    unsigned int   jointMoveData1;      // 0x160
    unsigned int   jointMoveData2;      // 0x164
    unsigned int   jointMoveData3;      // 0x168
    DWORD          emdScratchPtr1;      // 0x16C
    DWORD          emdScratchPtr2;      // 0x170

    // ---- Tail (0x174 - 0x17F) ----
    unsigned char  pad_174;             // 0x174
    unsigned char  maxHealth;           // 0x175
    unsigned char  weaponAimFlags;      // 0x176 - aim direction: 0x20 up, 0x40 neutral, 0x80
                                        //        down (gun family); also mirrored into the
                                        //        flags byte @ +0x00 for hit detection
    unsigned char  pad_177;             // 0x177
    unsigned short weaponAimState;      // 0x178 - reticle scan state: 0 none, 1 target,
                                        //        bit 1 set = scan result armed
    unsigned char  pad_17a[6];          // 0x17A-0x17F
};
#pragma pack(pop)
static_assert(sizeof(PlayerEntity) == 0x180, "PlayerEntity size mismatch");

// ============================================================================
// Entity (0x00be6464) - 0x18C bytes
// Generic entity (enemies, player when cast). Shares base layout with
// PlayerEntity up to 0x6C.
// ============================================================================
#pragma pack(push, 1)
struct Entity {
    // ---- Header (0x00 - 0x0B) ----
    unsigned char  status_flags;        // 0x00
    unsigned char  id;                  // 0x01
    unsigned char  behavior_flags;      // 0x02
    unsigned char  has_enter_switch_zone; // 0x03
    unsigned int   Sca_info;            // 0x04
    unsigned int   pSca_hit_data;       // 0x08

    // ---- SCA data + model (0x0C - 0x1F) ----
    short          unk_0c;              // 0x0C
    short          unk_0e;              // 0x0E
    unsigned char  unk_10;              // 0x10
    unsigned char  unk_11;              // 0x11
    unsigned char  unk_12;              // 0x12
    unsigned char  unk_13;              // 0x13
    unsigned int   modelLoadBuffer;     // 0x14
    unsigned int   unk_18;              // 0x18

    // ---- SCA matrix data (0x1C - 0x6B, 0x50 bytes) ----
    // Embedded ScaMatrixData struct. localMatrix at entity+0x20 is the transform matrix.
    // worldMatrix at entity+0x40. Total: field_00(4) + localMatrix(32) + worldMatrix(32)
    // + field_44(4) + owner(4) + field_4c(4) = 0x50 bytes.
    ScaMatrixData  scaMatrixData;       // 0x1C

    // ---- Position + movement (0x6C - 0x83) ----
    SVECTOR        position;            // 0x6C (8 bytes)
    // 0x72, 0x74 and 0x76 are the three components of the rotation SVECTOR that
    // RotMatrix reads from entity+0x72 (see EntityComputeJointWorldMatrices);
    // 0x72 is position.pad above. `angle` is the yaw and MUST be 16 bits: every
    // place the original touches it does so as *(short *)(_ENTITY + 0x74).
    // Modelling it as one `int` spanning 0x74-0x77 made plain
    // `ENTITY->angle = ...` a 32-bit write that clobbered angle_z, and made
    // `angleStep - ENTITY->angle` pull angle_z into the arithmetic. That is what
    // stopped entity_rotate_toward_target converging: it stepped a constant
    // amount every frame forever, walking the actor in a perfect circle.
    short          angle;               // 0x74 - yaw (rotation SVECTOR .y)
    short          angle_z;             // 0x76 - rotation SVECTOR .z
    SVECTOR        speed;               // 0x78 (8 bytes)
    short          move_step_x;         // 0x80 - movement step X (used by SCD event state 2)
    short          move_step_z;         // 0x82 - movement step Z (used by SCD event state 2)

    // ---- State (0x84 - 0x8F) ----
    unsigned char  state;               // 0x84
    unsigned char  ignore_player_flag;  // 0x85
    unsigned char  action_behavior;     // 0x86
    unsigned char  action_state;        // 0x87
    short          health;              // 0x88
    unsigned char  hit_state;           // 0x8A - damage hit state (low 3 bits = type/dir, bits 3-6 = reaction phase)
    unsigned char  pad_8b;              // 0x8B
    unsigned char  blend_counter;       // 0x8C - animation blend counter (decrements in player anims, set to bitmask in zombie)
    unsigned char  jointCount;          // 0x8D
    unsigned char  pad_8e[2];           // 0x8E-0x8F

    // ---- Model/animation pointers (0x90 - 0xA3) ----
    unsigned int   animHeader;          // 0x90
    unsigned int   animBase;            // 0x94
    JointStruct*   jointsStructs;       // 0x98
    unsigned int   weaponPartAnimSlot;  // 0x9C
    unsigned int   weaponPartAnimObject;// 0xA0

    // ---- Padding (0xA4 - 0xBC) ----
    unsigned char  pad_a4[8];           // 0xA4-0xAB
    unsigned int   weaponJointsPtr;     // 0xAC - pointer to weapon part joint data
    // ---- SCD event data (0xB0 - 0xBC) ----
    unsigned char  pad_b0[8];           // 0xB0-0xB7
    unsigned int   scd_target_ptr;      // 0xB8 - SCD event target pointer (entity/item/model)
    unsigned char  death_timer;          // 0xBC - countdown after death until entity removal (70 = 2.3s)

    // ---- Animation fields (0xBD - 0xBF) ----
    unsigned char  animationId;         // 0xBD
    unsigned char  animation_frame_id;  // 0xBE
    unsigned char  timing_control;      // 0xBF

    // ---- Empty gap (0xC0 - 0xC3) ----
    unsigned char  pad_c0[2];           // 0xC0-0xC1
    unsigned short move_speed_current;  // 0xC2 - current translation speed during animation/behavior

    // ---- Tick counter (0xC4 - 0xC5) ----
    // SIXTEEN bits, not eight. Every access in the original is a word:
    // `*(ushort *)(_ENTITY + 0xc4) = (g_RandSeed & 0x7f) + 0x32`,
    // `sVar1 = *(short *)(_ENTITY + 0xc4)`, and zombie_init clears 0xC4 and
    // 0xC5 together. Declared as a byte with pad_c5 after it, every one of the
    // ~39 `(unsigned short)` stores in Zombie.cpp silently truncated, so idle
    // and attack timers wrapped at 256 instead of 65536 - e.g. the 120-frame
    // eat timer and the (rand & 0x7f) + 300 walk timer were both mangled.
    unsigned short action_ticks_counter; // 0xC4

    // ---- Position offsets for animation (0xC6 - 0xDC) ----
    unsigned short unk_c6;              // 0xC6 - base X position offset for animation
    unsigned short unk_c8;              // 0xC8 - base Z position offset for animation
    // ---- SCD event movement data (0xCA - 0xDB) ----
    unsigned char  pad_ca[2];           // 0xCA-0xCB
    // Look-at target the tracking joint aims at. Written by the SCD look-at
    // opcode (RoomEvents.cpp); when lookAtFlags has 0x80 these are reloaded
    // from *(scd_target_ptr) + 0x34/0x38/0x3C on every update instead.
    int            scd_pos_x;           // 0xCC - look-at target X
    int            scd_pos_y;           // 0xD0 - look-at target Y
    int            scd_pos_z;           // 0xD4 - look-at target Z

    // 0xD8 is a look-at control bitfield, NOT an SCD "behavior type":
    //   0x01 enable yaw      0x02 enable pitch
    //   0x10 enable slewing  0x20 target is already absolute (skip recompute)
    //   0x80 reload target from *(scd_target_ptr)+0x34/0x38/0x3C
    // Every value the original writes (0x10, 0x11, 0x13, 0x33, 0x93) carries
    // 0x10. EntityUpdateLookAtAngles (0x00459eb0) gates on & 0x10 while
    // EntityApplyLookAtRotation (0x0045a2e0) gates on the whole byte != 0, so
    // clearing 0x10 freezes the look-at at its current angles rather than
    // snapping it back.
    unsigned char  lookAtFlags;         // 0xD8
    unsigned char  lookAtYawStep;       // 0xD9 - max yaw change per update (SCD default 0xC0)
    unsigned char  lookAtPitchStep;     // 0xDA - max pitch change per update (SCD default 0x40)
    unsigned char  scd_anim_param;      // 0xDB - SCD event animation parameter
    unsigned char  collisionFlags;      // 0xDC - collision callback flags (bit 3 = wall push)

    // ---- Look-at joint (0xDD) ----
    // Index (stride 0x7C) of the joint the look-at rotation is applied to.
    // ResetJointTransforms sets 1 (the player's tracking joint); other entity
    // types use 2, and the SCD opcode can set it per-entity.
    unsigned char  lookAtJointIdx;      // 0xDD

    // ---- SCD event timing (0xDE - 0xE1) ----
    // ONE 16-bit timer, not a lo/hi pair. Every access in the exe is a word -
    // 17 of them, zero byte accesses: `MOV word ptr [EDX + 0xde], 0x28` in
    // scd_event_state1_anim, and `MOV AX, word ptr [EAX + 0xde]` in each
    // npc_scd_behavior_*. The "lo/hi" split was an artifact of the wrong width.
    //
    // The port's writes happened to be correct anyway - storing lo then hi on
    // little-endian lands the same bytes - so this was latent rather than live.
    // A single-byte READ of scd_timer_lo would have truncated it, and the NPC
    // behaviours all read it as a word.
    unsigned short scd_timer;           // 0xDE - SCD event timer (frames)
    unsigned short scd_entity_flags;    // 0xE0 - SCD event entity flags

    // ---- Timer (0xE2 - 0xE3) ----
    // 16-bit, like action_ticks_counter and the waypoint pair. zombie_chase_walk
    // reads and writes it as `*(short *)(_ENTITY + 0xe2)` throughout, e.g.
    // `*(ushort *)(_ENTITY + 0xe2) = (g_RandSeed & 0xf) + 0x14`. Nothing in the
    // port referenced it yet, so widening it costs nothing.
    unsigned short next_turn_timer;     // 0xE2 - frames of wander-turn left

    // ---- Physics (0xE4 - 0xEB) ----
    SVECTOR        pushVelocity;        // 0xE4

    // ---- Large padding (0xEC - 0x15B) ----
    unsigned char  pad_ec[0x70];        // 0xEC-0x15B

    // ---- Data buffer + player tracking (0x15C - 0x16B) ----
    unsigned int   sca_data_ptr;         // 0x15C - pointer to entity's allocation in SCA data pool (dword)
    unsigned char  pad_160[3];           // 0x160-0x162
    unsigned char  death_event_id;       // 0x163 - room event index to trigger when entity is killed
    unsigned char  pad_164[2];           // 0x164-0x165
    // The movement WAYPOINT, 16 bits each - not bytes. Proven both ways in the
    // same pair of functions: zombie_walk1 reads it with
    // `MOVSX ECX, word ptr [EAX + 0x166]` (0x004343b5) and fast_player_facing
    // writes it with `MOV word ptr [EAX + 0x166], DX` (0x00435d69). Declared as
    // bytes, entity_pathfind_update's stores truncated the player's position to
    // its low 8 bits, so every waypoint-following behaviour steered at a point
    // within 256 units of the room origin.
    short          player_pos_x;        // 0x166 - waypoint X
    short          player_pos_z;        // 0x168 - waypoint Z
    unsigned char  pad_16a;             // 0x16A
    // 0x16B - Director's Cut only. zombie_init sets it to 1 for the DC's new
    // behaviour_flags nibbles 0xC/0xD/0xE and zombie_chase_walk reads it in
    // three places, each time repeating the call above it (two Joint_move and
    // two Add_speedXZ per frame) - which is the whole "fast zombie". Unused and
    // always zero with DcMode=0. (docs/PSX_DC_ENEMY_AI.md)
    unsigned char  dc_double_step;      // 0x16B

    // ---- Extended movement state (0x16C - 0x173) ----
    unsigned char  attacking_direction; // 0x16C
    unsigned char  dir_control_flags;   // 0x16D
    unsigned char  texBank;             // 0x16E
    unsigned char  seq_counter;         // 0x16F - action sequence counter/timer (decrements as idle delay, increments as attack phase step)
    unsigned char  angle_turn_delta;    // 0x170
    unsigned char  move_timer;          // 0x171 - effect countdown (footsteps/blood), decrements each frame
    unsigned char  is_moving;           // 0x172
    unsigned char  move_max_steps;      // 0x173 - max steps/phase count for current movement sequence

    // ---- Extended combat state (0x174 - 0x18B) ----
    unsigned char  splatter_flag;       // 0x174
    unsigned char  bob_speed;            // 0x175 - signed rotation speed for bobbing/weaving (8 / -8)
    unsigned short reaction_timer;       // 0x176 - 16-bit countdown timer for hit reaction / damage recovery
    int             subpixel_pos_x;     // 0x178 - 16.16 fixed-point X position accumulator
    unsigned char   action_speed;       // 0x17C - signed approach/retreat speed (×16); also hit counter in damaged state
    unsigned char   hit_threshold;      // 0x17D - max hits before falldown (compared to action_speed hit counter)
    unsigned char  behavior_step;       // 0x17E - sub-phase counter within current behavior sequence (0-9)
    unsigned char  action_counter;      // 0x17F - generic per-behavior flag/counter (0/1 boolean, or 10-15 phase values)
    unsigned char  move_speed;           // 0x180 - default move/chase speed (45 for zombies, 20 for pushback)
    unsigned char  turn_speed;           // 0x181
    unsigned char  internal_timer;      // 0x182
    unsigned char  pad_183;             // 0x183
    unsigned char  state_mirror;        // 0x184
    unsigned char  ignore_player_flag_mirror; // 0x185
    unsigned char  action_behavior_mirror;    // 0x186
    unsigned char  attack_behavior_mirror;    // 0x187
    unsigned char  stagger_timer;       // 0x188 - poise countdown; decrements while taking hits, falldown when zero
    unsigned char  pad_189[3];          // 0x189-0x18B
};
#pragma pack(pop)
static_assert(sizeof(Entity) == 0x18C, "Entity size mismatch");
