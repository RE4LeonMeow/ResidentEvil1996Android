// Zombie.cpp - Zombie enemy type update logic
// All functions decompiled from Ghidra with original addresses.
// Enemy IDs: 0 (standard zombie), 1 (naked zombie)
#include "Zombie.h"
#include "../../DebugPrint.h"
#include <cstdlib>

// Forward declarations for cross-TU functions
extern int is_entity_in_switch_zone(VECTOR* pos, void* zoneData);
extern void ResetJointTransforms(void);
extern void SetRotAndTransMatrix(MATRIX* m);

// ============================================================================
// Forward declarations for zombie state/behavior functions
// (defined later in this file; needed for the dispatch tables below)
// ============================================================================
static void zombie_idle(void);              // 0x004349d0
static void zombie_slow_walk(void);         // 0x00434cd0
static void zombie_idling(void);             // 0x00434730
static void zombie_walk2(void);              // 0x00434750
static void zombie_no_action(void);          // 0x004342d0 - a bare RET
static void zombie_attack_withdraw(void);
static void zombie_attack_head_bite(void);
static void zombie_attack_vomit(void);
static void zombie_attack_headless_death(void);

// ============================================================================
// Data tables (original addresses from Ghidra)
// ============================================================================

// enemies_update_functions_tbl (0x004d3c90), the entity type dispatch table, is
// defined in EntityCommon.cpp - it names every entity type, not just the zombie.

// ---------------------------------------------------------------------------
// The 0x004bb260-0x004bb31f block: one contiguous run of data that the original
// addresses through several different bases. Read from the exe (all values below
// verified byte-for-byte against 0x004bb260):
//
//   0x004bb260  ScaInfo record, standard zombie   (16 bytes)
//   0x004bb270  ScaInfo record, naked zombie      (16 bytes)
//   0x004bb280  g_pZombieScaInfo[2] = { 0x004bb260, 0x004bb270 }
//   0x004bb288  = g_pZombieScaInfo + 0x08   health base    [16]
//   0x004bb298  = g_pZombieScaInfo + 0x18   initial anim   [16]
//   0x004bb2a8  = g_pZombieScaInfo + 0x28   stagger/poise  [32]
//   0x004bb2c8  = g_pZombieScaInfo + 0x48   zombie_states_table
//   0x004bb2f0  = zombie_states_table + 10  zombie_behavior_tbl
//   0x004bb31f                              zombie_damage_action_tbl
//
// zombie_init reads the three byte tables as raw offsets off the POINTER table
// (`*(byte *)((int)g_pZombieScaInfo + (rand & 0xf) + 8)`), which is why the old
// pass filed them at 0x004bb290 / 0x004bb2a0 / 0x004bb2b0 - each 8 bytes too far
// - and then invented the contents. Same trap as the three-views table in
// CharacterNpc.cpp.
// ---------------------------------------------------------------------------

// ScaInfo layout matches CharacterNpc.cpp's CharScaInfo: field_04 is the hit-box
// extent and +0x0A is the collision radius that check_room_collision reads.
// Standard zombie r=422, naked zombie r=322.
//
// The third record is Director's Cut only: the DC's pointer table has three
// entries (PS1 0x80121244 -> 0x80121220 / 0x8012122c / 0x80121238, 12 bytes
// each) where the OG's and the PC's have two, and zombie_init hands it to a
// behaviour-11 zombie. Its collision radius is 1000 against the standard
// zombie's 422, and +0x02 is 1000 instead of 0. Nothing selects it with
// Mode=OG. (docs/PSX_DC_ENEMY_AI.md)
static const short zombie_sca_info[3][8] = {
    // 0x004bb260 / PS1 0x80121220
    { (short)0x8001, 0x0000, (short)0xfa06, 0x0000, 0x05fa, 0x01a6, 0x0000, 0x0000 },
    // 0x004bb270 / PS1 0x8012122c
    { (short)0x8001, 0x0000, (short)0xfa06, 0x0000, 0x05fa, 0x0142, 0x0000, 0x0000 },
    // PS1 DC 0x80121238 - no PC counterpart
    { (short)0x8001, 0x03e8, (short)0xfa06, 0x0000, 0x05fa, 0x03e8, 0x0000, 0x0000 },
};

// g_pZombieScaInfo @ 0x004bb280 - indexed by entity->id (0 standard, 1 naked).
// This is what zombie_init stores into ENTITY->Sca_info. The old code stored the
// address of THIS table instead of the record it points at, so
// check_room_collision read Sca_info+10 out of the health table and got a
// collision radius of 15183 instead of 422.
const short* const g_pZombieScaInfo[3] = {
    zombie_sca_info[0],
    zombie_sca_info[1],
    zombie_sca_info[2],   // DC only - behaviour 11
};

// 0x004bb288 (g_pZombieScaInfo + 8) - health base, index = random & 0xF.
// zombie_init then subtracts rand % 22, so a zombie spawns with 17..99 HP.
const unsigned char zombie_health_tbl[16] = {
    59, 59, 79, 59, 59, 39, 59, 79,
    79, 99, 59, 79, 59, 59, 79, 59
};

// 0x004bb298 (g_pZombieScaInfo + 0x18) - initial animationId (0xBD),
// index = behavior_flags & 0xF.
const unsigned char zombie_anim_id_tbl[16] = {
    0, 0, 9, 9, 0, 12, 9, 29,
    0, 0, 9, 0, 0, 0, 0, 0
};

// The Director's Cut's copy (PS1 0x80121260) differs in exactly one byte:
// index 11 is 29, the animation its new behaviour-11 handler plays. Indices
// 12-15 are past the DC's table (its stagger table starts at 0x8012126c) and
// are carried over from the port's, which never reaches them either.
static const unsigned char zombie_anim_id_tbl_dc[16] = {
    0, 0, 9, 9, 0, 12, 9, 29,
    0, 0, 9, 29, 0, 0, 0, 0
};

// 0x004bb2a8 (g_pZombieScaInfo + 0x28) - stagger_timer (0x188) poise budget,
// index = g_RandSeed & 0x1F.
const unsigned char zombie_stagger_tbl[32] = {
    4, 3, 5, 3, 4, 4, 3, 4,
    3, 5, 4, 4, 5, 3, 4, 5,
    4, 3, 3, 4, 4, 3, 4, 4,
    5, 3, 5, 3, 4, 3, 3, 4
};

// ---------------------------------------------------------------------------
// zombie_states_table @ 0x004bb2c8, indexed by entity->state (0x84), AND
// zombie_behavior_tbl @ 0x004bb2f0, indexed by behavior_flags & 0x0F.
//
// These are ONE pointer block read through two bases: 0x004bb2f0 is
// 0x004bb2c8 + 10*4, so behavior[n] IS state[n + 10]. Declared as one array with
// a second view rather than two arrays, so the overlap stays honest - the same
// call the original makes at 0x00433af6, `CALL [ECX*4 + 0x4bb2f0]`, is what
// established the behaviour base.
//
// Entries [22] onward are the bytes of zombie_damage_action_tbl, so the array
// stops at 22.
//
// Length and base verified against both PS1 builds (docs/PSX_DC_ENEMY_AI.md):
// the OG STAGE1 overlay's table is 20 entries with its behaviour view at
// states+9, and this one is that table with a NULL inserted at index 9 and
// another appended at index 21 - which is why the PC's behaviour base is
// states+10 while the PS1's is states+9, and why behaviour k is the same
// handler in both. The Director's Cut adds one entry, and it lands on
// behaviour 11 = [21], the trailing NULL below; dc_apply_zombie_tables()
// fills it in DC mode.
// ---------------------------------------------------------------------------
void* zombie_states_table[22] = {
    (void*)zombie_init,            // [0]  init
    (void*)zombie_state_check,     // [1]  idle / behavior dispatch
    (void*)zombie_damaged,         // [2]  hit reaction
    (void*)zombie_die,             // [3]  death sequence
    (void*)zombie_no_action,       // [4]  0x004342d0 - a bare RET
    (void*)zombie_attack,          // [5]  attacking player (thunk 0x004342e0)
    NULL,                          // [6]  unused
    NULL,                          // [7]  unused
    (void*)zombie_action_update,   // [8]  SCD action dispatch (0x00454ab0)
    NULL,                          // [9]  unused
    // ---- from here the behaviour view aliases the same entries ----
    (void*)zombie_chase_player,    // [10] = behavior[0]
    (void*)zombie_chase_player,    // [11] = behavior[1]
    (void*)zombie_pushed_back,     // [12] = behavior[2]
    (void*)zombie_pushed_back,     // [13] = behavior[3]
    (void*)zombie_random_chase,    // [14] = behavior[4]
    (void*)zombie_eating,          // [15] = behavior[5]
    // ---- behaviour-only tail (past the 16 states) ----
    (void*)zombie_chase_player,    // [16] = behavior[6]
    (void*)zombie_eating,          // [17] = behavior[7]
    (void*)zombie_chase_player,    // [18] = behavior[8]
    NULL,                          // [19] = behavior[9]
    (void*)zombie_pushed_back,     // [20] = behavior[10]
    NULL                           // [21] = behavior[11] - DC: dc_standup_lunge
};

// zombie_behavior_tbl @ 0x004bb2f0 - the second view of the block above.
// Only 0-11 are pointers; 12-15 would read into zombie_damage_action_tbl's
// bytes. The original does not bounds-check, so behavior_flags & 0xF is never
// 12-15 in shipped data - and in DC mode zombie_init masks the DC's new
// 0xC/0xD/0xE nibbles down to 0 or 1 before anything dispatches on them.
void** const zombie_behavior_tbl = &zombie_states_table[10];

// ============================================================================
// zombie_update @ 0x004338c0
// Main per-frame zombie update.
// Dispatches to the state handler, handles collision, movement, and
// camera switch zone checks.
// ============================================================================
void zombie_update(void)
{
    // 0x004338c3-0x004338fb: the two ends of a PRONE zombie's body, as offsets
    // along its own local X axis. Ghidra shows these as eight separate shorts at
    // [ESP+0xc..0x1a]; they are two adjacent 8-byte SVECTORs, +600 first and
    // -600 second, and only the laying-down branch below uses them.
    //
    // check_room_collision_two_point rotates each by the entity's yaw, adds the entity position,
    // and runs the full boundary push at BOTH points - a zombie on the floor is
    // ~1200 units long, so the single-point check_room_collision test above is
    // not enough. If either end is still inside geometry afterwards it rolls the
    // whole body back, position AND angle, and returns 0x80.
    SVECTOR bodyEndFront = { 600, 0, 0, 0 };
    SVECTOR bodyEndBack  = { -600, 0, 0, 0 };

    int joint = (int)ENTITY->jointsStructs;

    // 0x00433906-0x00433914: Only update if message system allows (g_message_flags & 0x04)
    if ((g_message_flags & 0x0004) != 0) {
        // 0x00433914: `CALL [ECX*4 + 0x4bb2c8]` - unconditional, no NULL test.
        // States 6, 7 and 9 are NULL in the table; the original faults on them
        // too, so nothing here guards against it.
        ((void(*)())zombie_states_table[ENTITY->state])();

        // 0x0043391b-0x00433927: Mirror state fields (for animation blending)
        ENTITY->state_mirror = ENTITY->state;
        ENTITY->ignore_player_flag_mirror = ENTITY->ignore_player_flag;
        ENTITY->action_behavior_mirror = ENTITY->action_behavior;
        ENTITY->attack_behavior_mirror = ENTITY->action_state;

        // 0x0043392d-0x00433943: Decrement internal timer
        if (ENTITY->internal_timer != 0) {
            ENTITY->internal_timer--;
        }

        // 0x00433948-0x0043394f: Skip collision if in attack state (5)
        if (ENTITY->state != ZOMBIE_STATE_ATTACK) {
            // Director's Cut: behaviour 11 skips the three entity-collision
            // calls below outright. DC STAGE1 0x8010615c is
            // `lbu 2($v0)` / `andi 0xf` / `beq 0xb` / branch to the
            // collisionFlags clear at 0x801061c0 - the DC edits the call site,
            // it does not touch status_flags. The OG overlay has no such test:
            // its state!=5 branch (0x80106014) falls straight into them from
            // 0x8010601c.
            //
            // This is what keeps the Forest zombie's body from being shoved
            // around while it lies on the 2F balcony. Its spawn (zombie_init)
            // sets status bit 2 only, and that bit is the ROOM-collision gate
            // (check_room_collision / check_room_collision_two_point) - the
            // entity pair test in ResolveEntityScaCollision reads bit 1, which
            // nothing about behaviour 11 sets. So without this branch the
            // player pushes the prone body (and the body pushes back), which is
            // the reported defect. Note the trio is skipped, not just its push:
            // HandleEnemyPlayerCollisions is how an enemy pushes the player, so
            // the prone body stops being solid in both directions - exactly
            // what the DC looks like.
            //
            // Once dc_standup_lunge completes it clears behavior_flags, so the
            // standing zombie collides normally from that frame on.
            if (!(g_bDcMode && (ENTITY->behavior_flags & 0x0F) == ZOMBIE_BEH_DC_STANDUP)) {
                // 0x00433955: Set up SCA hit data for collision
                SetEntityScaHitData(ENTITY);

                // 0x0043395e-0x0043396e: Resolve collision vs player
                ResolveEntityScaCollision((Entity*)&g_playerEntity, ENTITY);

                // 0x00433971: Handle enemy-player collision response
                HandleEnemyPlayerCollisions();
            }

            // 0x0043397b: Clear collision push flag (bit 3)
            ENTITY->collisionFlags &= ~0x08;

            // 0x00433987-0x00433a0f: Room collision check
            if ((ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) == 0) {
                // 0x004339dc-0x00433a0f: Not laying down - standard collision
                unsigned char collisionResult = check_room_collision(
                    (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t,
                    *(short*)(ENTITY->Sca_info + 10));
                ENTITY->dir_control_flags |= collisionResult;
                // WORD store at 0x17a (MOV word [EDI+0x17a],DX), not a byte.
                *(unsigned short*)((char*)ENTITY + 0x17a) = (unsigned short)(unsigned int)g_tempVar;
            } else {
                // 0x0043398d-0x004339da: Laying down - also check floor
                unsigned char collisionResult = check_room_collision(
                    (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t,
                    *(short*)(ENTITY->Sca_info + 10));
                ENTITY->dir_control_flags |= collisionResult;
                // WORD store at 0x17a (MOV word [EDI+0x17a],DX), not a byte.
                *(unsigned short*)((char*)ENTITY + 0x17a) = (unsigned short)(unsigned int)g_tempVar;

                // Argument order is the original's: the -600 end goes first.
                unsigned char floorResult = check_room_collision_two_point(&bodyEndBack, &bodyEndFront);
                ENTITY->dir_control_flags |= floorResult;
            }
        }

        // 0x00433a14: CMP word ptr [EAX+0x174],0x0 - a WORD test, so bob_speed
        // at 0x175 counts too, not just splatter_flag.
        if (*(unsigned short*)&ENTITY->splatter_flag != 0) {
            blood_splatter_physics(joint + 0xf8, 6);
        }
    }

    // 0x00433a2f-0x00433a4c: Camera switch zone check
    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t,
        g_CurrentRdtDataTypePtr);

    // 0x00433a4f-0x00433a74: Apply push/velocity physics if visible
    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite(
            (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t,
            (short*)&ENTITY->pushVelocity,
            0,
            *(unsigned short*)&ENTITY->angle);
    }

    // 0x00433a77-0x00433aca: Joint-based secondary collision (weapon/hand joint)
    joint = (int)ENTITY->jointsStructs;
    if (((*(unsigned char*)(joint + 0x1f0) & 4) != 0) &&
        (*(int*)(joint + 0x24c) == -100))
    {
        int jointVis = is_entity_in_switch_zone(
            (VECTOR*)(joint + 0x248),
            g_CurrentRdtDataTypePtr);
        if (jointVis != 0) {
            entity_add_fade_sprite(
                (VECTOR*)(joint + 0x248),
                // MOV EDX,[ECX+0x15c] - the stored pointer's VALUE, not its
                // address (the call above does take an address, at +0xe4).
                (short*)ENTITY->sca_data_ptr,
                0,
                *(unsigned short*)(joint + 0x1f6));
        }
    }
}

// ============================================================================
// zombie_init @ 0x00433440
// One-time initialization for a zombie entity.
// Sets up health, behavior, animation, and SCA collision data.
// Called as state 0 in the zombie state machine.
// ============================================================================
void zombie_init(void)
{
    // 0x00433440-0x0043352d: Initialize local behavior table
    // This table maps random seed to behavior modifier values.
    // Values for the second half (at offset 32) are for non-standard difficulty.
    unsigned char local_40[64];
    local_40[0x20] = 1; local_40[0x21] = 2; local_40[0x22] = 3; local_40[0x23] = 2;
    local_40[0x24] = 3; local_40[0x25] = 3; local_40[0x26] = 2; local_40[0x27] = 3;
    local_40[0x28] = 3; local_40[0x29] = 2; local_40[0x2a] = 2; local_40[0x2b] = 3;
    local_40[0x2c] = 2; local_40[0x2d] = 3; local_40[0x2e] = 2; local_40[0x2f] = 3;
    local_40[0x30] = 2; local_40[0x31] = 3; local_40[0x32] = 2; local_40[0x33] = 4;
    local_40[0x34] = 2; local_40[0x35] = 2; local_40[0x36] = 2; local_40[0x37] = 2;
    local_40[0x38] = 3; local_40[0x39] = 2; local_40[0x3a] = 2; local_40[0x3b] = 3;
    local_40[0x3c] = 2; local_40[0x3d] = 2; local_40[0x3e] = 2; local_40[0x3f] = 3;
    local_40[0] = 3;   local_40[1] = 2;   local_40[2] = 3;   local_40[3] = 3;
    local_40[4] = 2;   local_40[5] = 3;   local_40[6] = 4;   local_40[7] = 3;
    local_40[8] = 2;   local_40[9] = 3;   local_40[10] = 3;  local_40[0xb] = 4;
    local_40[0xc] = 3; local_40[0xd] = 2;  local_40[0xe] = 3; local_40[0xf] = 3;
    local_40[0x10] = 3; local_40[0x11] = 2; local_40[0x12] = 3; local_40[0x13] = 3;
    local_40[0x14] = 3; local_40[0x15] = 3; local_40[0x16] = 2; local_40[0x17] = 3;
    local_40[0x18] = 3; local_40[0x19] = 2; local_40[0x1a] = 3; local_40[0x1b] = 4;
    local_40[0x1c] = 3; local_40[0x1d] = 4; local_40[0x1e] = 3; local_40[0x1f] = 2;

    // 0x0043352d-0x0043353d: Set initial state to idle (1)
    ENTITY->state = ZOMBIE_STATE_IDLE;
    ENTITY->ignore_player_flag = 0;
    ENTITY->action_behavior = 0;
    ENTITY->action_state = 0;

    // 0x0043353d-0x00433582: Zero out SCA matrix data base fields
    *(int*)(&ENTITY->scaMatrixData) = 0;

    // 0x00433582-0x004335b5: Clear counters and flags
    ENTITY->pad_c0[1] = 0;
    // zombie[0xc4] = 0; zombie[0xc5] = 0 - the two halves of the 16-bit counter.
    ENTITY->action_ticks_counter = 0;
    ENTITY->death_timer = 0;
    ENTITY->hit_state = 0;

    // 0x004335b5-0x004335c2: SCA record for the standard zombie.
    // `*(void **)(_ENTITY + 4) = g_pZombieBehaviorTbl[0]` - the DEREFERENCED
    // entry, i.e. 0x004bb260. Storing 0x004bb280 (the table itself) made
    // check_room_collision read Sca_info+10 out of the health table.
    ENTITY->Sca_info = (unsigned int)g_pZombieScaInfo[0];

    // 0x004335c2-0x004335f0: Initialize SCA hit/joint data
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    ENTITY->sca_data_ptr = (unsigned int)g_loadDataDestPointer;
    set_next_entity_data_buffer(2);

    // The shadow tint goes in the scratch at 0x00be0dfc (g_animFrameIdSave),
    // which is the dword FUN_004565f0 copies into the quad header - NOT
    // g_tempVar at 0x00be0df8. Writing the wrong one left both shadows untinted.
    //
    // `MOV dword ptr [0x00be0dfc],0xffff50` (0x004335e5) and `...,0x808080`
    // (0x00433626) are IMMEDIATE VALUES. Ghidra prints them as &DAT_00ffff50 /
    // &DAT_00808080 because the numbers look like addresses, and taking the
    // address of the port's placeholder globals fed the quad a garbage RGB
    // instead - which is why a corpse's ground shadow ramped from a wrong dark
    // tint into the bright 0xffff50 the death path sets.
    g_animFrameIdSave = 0x00ffff50;
    FUN_004565f0(&g_svecScratch, *(SVECTOR**)&ENTITY->sca_data_ptr, 400, 400);
    ResetJointTransforms();

    g_animFrameIdSave = 0x00808080;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 700, 900);

    // 0x004335f0-0x00433680: Calculate random health
    {
        unsigned char health_base;
        unsigned short health_variance;
        unsigned int randVal;

        if ((g_main_state_flags2 & MSF2_ATTRACT_DEMO) == 0) {
            randVal = rand();
            health_base = zombie_health_tbl[randVal & 0xF];
            randVal = rand();
            health_variance = (unsigned short)(randVal % 22);
        } else {
            health_base = zombie_health_tbl[g_RandSeed & 0xF];
            health_variance = g_RandSeed % 22;
        }
        ENTITY->health = health_base - health_variance;
    }

    // 0x00433680-0x004336a3: Laying down eating check
    if ((ENTITY->behavior_flags & 0xF) == ZOMBIE_BEH_5) {
        ENTITY->action_state = 2;
    }

    // Director's Cut: behaviour 11 spawns with collision off and the DC's third
    // ScaInfo record (radius 1000). It sits HERE, before the naked-zombie check,
    // because the DC puts it there and that check can overwrite Sca_info again -
    // a naked zombie with behaviour 11 ends up on the naked record.
    if (g_bDcMode && (ENTITY->behavior_flags & 0x0F) == ZOMBIE_BEH_DC_STANDUP) {
        ENTITY->status_flags |= 0x04;
        ENTITY->Sca_info = (unsigned int)g_pZombieScaInfo[2];
    }

    // 0x004336a3-0x004336b0: naked zombie swaps in the narrower record (r=322)
    if (ENTITY->id == ENEMY_ID_NAKED_ZOMBIE) {
        ENTITY->Sca_info = (unsigned int)g_pZombieScaInfo[1];
    }

    // 0x004336b0-0x00433700: Clear movement and splatter flags
    ENTITY->is_moving = 0;
    ENTITY->move_max_steps = 0;
    ENTITY->bob_speed = 0;               // 0x175
    // Byte write at 0x176 only: 0x177 is ATTACK_TIMER and the original leaves
    // it alone here.
    ((unsigned char*)&ENTITY->reaction_timer)[0] = 0;
    ENTITY->splatter_flag = 0;
    ENTITY->dir_control_flags = 0;
    ((unsigned char*)&ENTITY->subpixel_pos_x)[1] = 0;  // 0x179
    ENTITY->action_speed = 0;

    // 0x00433700-0x0043374f: Set behavior type based on difficulty
    {
        unsigned char behVal;
        if (g_bDcMode) {
            // DC: both threshold stores are unconditional - the overlay's
            // zombie_init (STAGE1.EXE 0x80105ad8) and hit handler
            // (0x80107000-20) always index the hard table with `seed & 0x1f`;
            // there is no scenario-flag branch and no first-playthrough row.
            behVal = local_40[g_RandSeed & 0x1F];
        } else if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            behVal = local_40[(g_RandSeed & 0x1F) + 32];
        } else {
            behVal = local_40[g_RandSeed & 0x1F];
        }
        ENTITY->hit_threshold = behVal;
    }

    // 0x0043374f-0x0043379f: Set misc behavior/timing values
    ENTITY->stagger_timer = zombie_stagger_tbl[g_RandSeed & 0x1F];
    ((unsigned char*)&ENTITY->subpixel_pos_x)[0] = 0;  // 0x178
    ENTITY->behavior_step = 0;
    ENTITY->move_speed = 45;

    // Director's Cut: three new spawn nibbles, rewritten in place. The DC puts
    // this block exactly here, between the move_speed and turn_speed stores -
    // which is also what keeps it zombie-only, since a cerberus spawned with
    // nibble 0xC (STAGE3/ROOM3040, in BOTH builds) means something else and
    // never reaches this function. The three tests are sequential rather than a
    // switch, as in the DC, but each masks its own nibble away so they stay
    // disjoint. Only 0xC and 0xD appear in the shipped DC data for zombie ids;
    // 0xE is ported for completeness. (docs/PSX_DC_ENEMY_AI.md)
    ENTITY->dc_double_step = 0;
    if (g_bDcMode) {
        if ((ENTITY->behavior_flags & 0x0F) == 0x0C) {
            ENTITY->dc_double_step = 1;
            ENTITY->behavior_flags &= 0xF0;          // -> behaviour 0
        }
        if ((ENTITY->behavior_flags & 0x0F) == 0x0D) {
            ENTITY->dc_double_step = 1;
            ENTITY->behavior_flags &= 0xF1;          // -> behaviour 1
        }
        if ((ENTITY->behavior_flags & 0x0F) == 0x0E) {
            ENTITY->dc_double_step = 1;
            ENTITY->behavior_flags &= 0xF0;          // -> behaviour 0
            ENTITY->health = (short)(ENTITY->health + 100);
        }
    }

    ENTITY->turn_speed = 24;
    ENTITY->action_counter = 0;
    ENTITY->internal_timer = 0;
    ENTITY->blend_counter = 0;

    // 0x0043379f-0x004337cf: Set initial animation ID
    // The DC's copy of this table differs in one byte: index 11 is 29.
    ENTITY->animationId = (g_bDcMode ? zombie_anim_id_tbl_dc
                                     : zombie_anim_id_tbl)[ENTITY->behavior_flags & 0xF];
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;

    // 0x004337cf-0x004337e5: Initialize joint animation
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    // 0x004337e5-0x00433818: a laying-down zombie sits at Y = 1.
    // The four byte stores at entity+0x38..0x3b are the dword localMatrix.t[1]
    // (MATRIX = 18 bytes of m[][] + 2 pad + t[3], so t[1] is at 0x20+0x18=0x38).
    // The old code wrote m[1][0..3] at 0x26, corrupting the rotation instead.
    if ((ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) != 0) {
        ENTITY->scaMatrixData.localMatrix.t[1] = 1;
    }

    // 0x00433818-0x0043387f: Dead zombie (behavior == 6: laying down, doing nothing)
    // Full-byte compare in the original (CMP AL,0x6), not (flags & 0xF).
    if (ENTITY->behavior_flags == ZOMBIE_BEH_6) {
        // `*_ENTITY | 10` is DECIMAL 10 = 0x0A = bits 1 and 3. Bit 1 is the one
        // ResolveEntityScaCollision tests to skip a deactivated entity; the old
        // 0x09 set bit 0 instead, so corpses still shoved the player around.
        ENTITY->status_flags |= 0x0A;
        ENTITY->health = -1;          // 0xff, 0xff short
        ENTITY->state = ZOMBIE_STATE_DIE;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 4;
        ENTITY->action_state = 0;
    }

    // 0x0043387f-0x004338b0: Lying-on-floor zombie (behavior == 10)
    // Full-byte compare in the original (CMP AL,0xa), not (flags & 0xF).
    if (ENTITY->behavior_flags == ZOMBIE_BEH_10) {
        int jointPtr = (int)ENTITY->jointsStructs;
        ENTITY->status_flags |= 0x04;
        // Disable joints (set bit 0 to 0 at specific offsets)
        *(unsigned char*)(jointPtr + 0x45c) &= 0xFE;
        *(unsigned char*)(jointPtr + 0x4d8) &= 0xFE;
        *(unsigned char*)(jointPtr + 0x554) &= 0xFE;
        *(unsigned char*)(jointPtr + 0x5d0) &= 0xFE;
        *(unsigned char*)(jointPtr + 0x64c) &= 0xFE;
        *(unsigned char*)(jointPtr + 0x6c8) &= 0xFE;
    }

    // 0x004338b0-0x004338be: Vomiting state check
    if ((ENTITY->behavior_flags & ZOMBIE_FLAG_VOMITING) != 0) {
        ENTITY->state = ZOMBIE_STATE_ACTION_UPDATE;
    }
}

// ============================================================================
// zombie_state_check @ 0x00433ae0
// Main behavior dispatcher for idle zombies.
// Checks distance to player and determines whether to chase, idle, or wander.
// ============================================================================
void zombie_state_check(void)
{
    // 0x00433ae0-0x00433aea: Skip if SCD-controlled (bit 7)
    if ((ENTITY->behavior_flags & ZOMBIE_FLAG_SCD_CONTROLLED) != 0) {
        return;
    }

    // 0x00433af6: `CALL [ECX*4 + 0x4bb2f0]` - unconditional. behavior 9 and 11
    // are NULL and 12-15 are not pointers at all; the original relies on the
    // shipped data never producing those, and so does this.
    ((void(*)())zombie_behavior_tbl[ENTITY->behavior_flags & 0x0F])();

    // 0x00433afd-0x00433b06: Clear status flags (keep lower 5 bits)
    ENTITY->status_flags &= 0x1F;

    // 0x00433b06-0x00433b30: Check long-range player detection
    if (((ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) == 0) &&
        ((ENTITY->action_speed & 0x80) == 0))
    {
        ENTITY->status_flags |= ENTITY_STATUS_ALIGNED;
        entity_check_alert_range(3000);
    }

    // 0x00433b2b-0x00433b35: Check medium-range player detection
    entity_check_visual_range(4500);

    // 0x00433b35-0x00433b73: Check player vertical position
    g_playerDisplacement =
        (int)g_playerEntity.scaMatrixData.localMatrix.t[1] -
        (int)ENTITY->scaMatrixData.localMatrix.t[1];

    if ((g_playerDisplacement < -100) || (g_playerDisplacement > 100)) {
        ENTITY->status_flags &= 0x1F;
        if (g_playerDisplacement < 0) {
            ENTITY->status_flags |= ENTITY_STATUS_PLAYER_ABOVE;
        } else {
            ENTITY->status_flags |= ENTITY_STATUS_PLAYER_BELOW;
        }
    }

    // 0x00433b73-0x00433b98: Eating zombies (behavior 5 or 7) check
    if ((ENTITY->behavior_flags == (ZOMBIE_FLAG_LAYING_DOWN | 0x05)) ||
        (ENTITY->behavior_flags == ZOMBIE_BEH_5))
    {
        ENTITY->status_flags &= 0x1F;
        entity_check_visual_range(4000);
    }

    // 0x00433b98-0x00433ba9: behavior_step bit 2 = follow the player without
    // attacking; force the "aligned" bit so the chase behaviours engage.
    if ((ENTITY->behavior_step & 0x04) != 0) {
        ENTITY->status_flags |= ENTITY_STATUS_ALIGNED;
    }
}

// ============================================================================
// zombie_damage_action_tbl @ 0x004bb31f -> action_behavior
// Index = (hit_state & 7) + (behavior_flags & 2) * 3, i.e. the laying-down
// stride is SIX, not three: 0x00434037 is `LEA ESI,[ECX + ECX*2]` applied to the
// already-masked 0x02, which gives 6. With a stride of 3 a prone zombie picked
// the standing reaction. Max index is 7 + 6 = 13, so the table is 14 long.
// (Its first byte overlaps the last byte of zombie_states_table[21], which is
// NULL - that is why the address is unaligned.)
static const unsigned char zombie_damage_action_tbl[14] = {
    0, 0, 5, 0, 2, 0, 0, 4,
    4, 0, 0, 0, 0, 0
};

// ---------------------------------------------------------------------------
// zombie_damage_behavior_tbl @ 0x004bb330 - indexed by action_behavior (0x86),
// dispatched unconditionally at 0x004340cd. Read from the exe.
//
// The old code had a five-entry local table starting with zombie_falldown.
// zombie_falldown is not in this table at all - entry 0 is short_push_back - and
// six of the twelve targets are not decompiled yet.
// ---------------------------------------------------------------------------

// Forward declarations for zombie state helper dependencies.
// The entity-generic helpers this file calls (turn_toward_target,
// entity_pathfind_update, entity_check_angular_los, ...) are declared in
// EntityCommon.h, included via Zombie.h.
static void zombie_no_action(void);
extern void zombie_dead_animation(void);
extern void zombie_update_player_distance(void);
extern void update_zombie_action(void);
extern void magnum_shot_pushback(void);
extern void zombie_pushback_idle(void);
extern void zombie_pushback_stagger(void);
extern void zombie_pushback_action(void);
extern void zombie_check_special_weapon(void);    // 0x0043d8a0
extern void zombie_body_part_physics(unsigned char param);
extern void Flg_on(int baseAddr, unsigned int bitIndex);
extern void BillboardSetColor(void* quad, int unused1, int unused2, unsigned int color);

// Shared scratch globals defined in EntityCommon.cpp
extern unsigned int g_entity_bkp;
extern void* _ENTITY_SAVE;
extern int g_scaled_down_dist;
extern int player_distance_z;

// Accessor for byte at entity+0x177 (high byte of reaction_timer used as attack countdown)
#define ATTACK_TIMER  (((unsigned char*)&ENTITY->reaction_timer)[1])

// Zombie damage behavior handlers (dispatched from zombie_damaged)
// 0x004368a0, not 0x00436af0 - that was an address inside the body. Note this
// is NOT in zombie_damage_behavior_tbl; its real call site is unidentified.
extern void zombie_falldown(void);    // 0x004368a0
extern void short_push_back(void);    // 0x00436c80 - short hit reaction / stagger backwards
extern void push_and_stagger(void);   // 0x00436f00 - push back and stagger

// ---------------------------------------------------------------------------
// push_and_drop @ 0x00437450
// zombie_damage_behavior_tbl[4]: knocked down onto the floor (animation 15),
// with a moan gated behind internal_timer so it cannot retrigger for 150 frames.
// On completion it returns to behaviour 1 and, if it was behaviour 3 and not
// wall-pushed, downgrades the behaviour byte to 2.
// ---------------------------------------------------------------------------
static void push_and_drop(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 15;
        if (ENTITY->internal_timer == 0) {
            Snd_em(9);                      // falldown moan
            ENTITY->internal_timer = 150;   // and a cooldown before the next
        }
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 1024) != 0) {
        // MOV dword [_ENTITY+0x84],0x10001 - state=1, ignore=0, behaviour=1.
        ENTITY->state              = ZOMBIE_STATE_IDLE;
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior    = 1;
        ENTITY->action_state       = 0;
        ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        ENTITY->hit_state = 0;

        // Only when not being shoved out of a wall this frame.
        if ((ENTITY->collisionFlags & 8) == 0) {
            if ((ENTITY->behavior_flags & 0x20) == 0
                && (ENTITY->behavior_flags & 0x0F) == 3) {
                ENTITY->behavior_flags = 2;   // plain store, not a masked merge
            }
        }
    }

    zombie_check_special_weapon();
}
// ---------------------------------------------------------------------------
// explode_leg_and_drop @ 0x00437050
// zombie_damage_behavior_tbl[5]: a leg is blown off and the zombie goes down.
// Sprays the two leg joints, plays the explode + moan cues, then runs the same
// corpse tail as zombie_dead_animation - including the same "gets back up"
// escape, here reached only while health is still positive.
//
// The 4-dword block copied out of g_deadMoveValue (0x00d1fdd0 + 0x14) is the
// billboard's spawn offset; the original copies it wholesale into the
// g_playerPosScratch word, pad included.
// ---------------------------------------------------------------------------
static void explode_leg_and_drop(void)
{
    // Same prone-body probe pair as zombie_dead_animation.
    SVECTOR bodyEndFront = { 800, 0, 0, 0 };
    SVECTOR bodyEndBack  = { -800, 0, 0, 0 };

    switch (ENTITY->action_state) {
    case 0: {
        ENTITY->action_state = 1;
        ENTITY->death_timer = 70;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 8;
        ENTITY->move_speed_current = 0x14;

        int joint = (int)ENTITY->jointsStructs;
        joint_setup_attack_effect(joint + 0x4D8, 0x14, 5, 3);
        joint_setup_attack_effect(joint + 0x554, 0x14, 5, 3);

        // g_deadMoveValue (0x00d1fdd0) HOLDS a pointer; the original copies the
        // 4 dwords at *(g_deadMoveValue + 0x14). The old `&g_deadMoveValue`
        // read the dword's own .bss storage instead.
        const int* spawn = (const int*)((char*)g_deadMoveValue + 0x14);
        g_playerPosScratch.x   = spawn[0];
        g_playerPosScratch.y   = spawn[1];
        g_playerPosScratch.z   = spawn[2];
        g_playerPosScratch.pad = spawn[3];

        Effect_CreateBillboard(0, 0, 0, (void*)(joint + 0x4A0), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, (void*)(joint + 0x51C), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, (void*)(joint + 0x690), &g_playerPosScratch, 0);
        Snd_em(6);                              // limb explode

        if (ENTITY->internal_timer == 0) {
            Snd_em(9);                          // falldown moan, once per 150 frames
            ENTITY->internal_timer = 150;
        }

        // Losing a leg does not kill: clamp a negative health back to 1.
        if (ENTITY->health < 0) {
            ENTITY->health = 1;
        }
        ENTITY->scaMatrixData.localMatrix.t[1] = 1;   // dword at 0x38
        // FALLS THROUGH into sub-state 1.
    }
    case 1:
        ENTITY->action_state += (char)Joint_move(0, ENTITY->animHeader,
                                                ENTITY->animBase, 1024);
        if (ENTITY->timing_control == 1) {
            if (ENTITY->animation_frame_id == 8)    Snd_em(1);
            if (ENTITY->animation_frame_id == 0x1A) Snd_em(0);
        }
        break;

    case 2: {
        // Probe for floor space without letting the probe move anything.
        VECTOR saved;
        saved.x   = ENTITY->scaMatrixData.localMatrix.t[0];
        saved.y   = ENTITY->scaMatrixData.localMatrix.t[1];
        saved.z   = ENTITY->scaMatrixData.localMatrix.t[2];
        saved.pad = *(int*)((char*)ENTITY + 0x40);

        ENTITY->status_flags |= 0x0E;
        g_playerDisplacement = check_room_collision_two_point(&bodyEndBack, &bodyEndFront);

        ENTITY->scaMatrixData.localMatrix.t[0] = saved.x;
        ENTITY->scaMatrixData.localMatrix.t[1] = saved.y;
        ENTITY->scaMatrixData.localMatrix.t[2] = saved.z;
        *(int*)((char*)ENTITY + 0x40) = saved.pad;

        // Crawls away instead of dying: needs behaviour != 4, not stage/room
        // 0x0201, clear floor, and health still above zero.
        if (ENTITY->behavior_flags != 4
            && *(unsigned short*)&g_stageId != (STAGE_MANSION_2F | (ROOM_DINING_ROOM_2F << 8))
            && g_playerDisplacement == 0
            && ENTITY->health > 0)
        {
            // 2 normally, 3 on a 1-in-4 roll: `SETZ`-style `((rand & 3) == 0) + 2`.
            ENTITY->behavior_flags = (unsigned char)(((g_RandSeed & 3) == 0) + 2);
            ENTITY->state              = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_behavior    = 0;
            ENTITY->action_state       = 0;
            ENTITY->hit_state = 0;
            ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
            ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
            ENTITY->status_flags &= 0xF1;   // clear bits 1-3
            return;
        }

        ENTITY->health = -1;
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00ffff50);
        BillboardAdjSize(&ENTITY->pushVelocity, -100, -100);
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        ENTITY->action_state = 3;
        ENTITY->status_flags |= 0x0E;
        ENTITY->move_speed_current = 0;
        // FALLS THROUGH into sub-state 3.
    }
    case 3:
        BillboardAdjSize(&ENTITY->pushVelocity, 6, 6);
        ENTITY->death_timer--;
        if (ENTITY->death_timer == 0) {
            ENTITY->action_state = 4;
        }
        break;
    }

    Add_speedXZ(0);
}
// ---------------------------------------------------------------------------
// long_push_back @ 0x00437540
// zombie_damage_behavior_tbl[6]: the long shove. Enters animation 7 already
// twelve frames in with timing_control pre-set, rides it out, then idles for a
// few frames before handing back to behaviour 2 with the player as the waypoint.
//
// Sub-states 0 and 1 share the animation block; 2 and up skip it entirely and
// only fall through to the Add_speedXZ at the end (the `goto LAB_0043767c`).
// ---------------------------------------------------------------------------
static void long_push_back(void)
{
    unsigned char st = ENTITY->action_state;

    if (st == 0) {
        ENTITY->action_state = 1;
        // Note: NOT reset to zero - the animation is entered mid-way, at frame
        // 12 with timing_control already 1.
        ENTITY->animation_frame_id = 0x0C;
        ENTITY->timing_control = 1;
        ENTITY->hit_state = 1;
        ENTITY->animationId = 7;
        ENTITY->move_speed_current = 0x1E;
    } else if (st != 1) {
        if (st == 2) {
            // Value-before-decrement.
            short ticks = (short)ENTITY->action_ticks_counter;
            ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
            if (ticks == 0) {
                // dword 0x02020001 at 0x84 (Ghidra: &g_enemy_state).
                ENTITY->state              = ZOMBIE_STATE_IDLE;
                ENTITY->ignore_player_flag = 0;
                ENTITY->action_behavior    = 2;
                ENTITY->action_state       = 2;
                ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
                ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
                ENTITY->hit_state = 0;
            }
        }
        Add_speedXZ(2048);
        return;
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) == 0) {
        // Mid-animation: drop the hit flag once past frame 20 so further hits
        // register again.
        if (ENTITY->animation_frame_id == 0x14) {
            ENTITY->hit_state = 0;
        }
    } else {
        ENTITY->action_state = 2;
        ENTITY->action_ticks_counter = (unsigned short)(g_RandSeed & 7);
        ENTITY->move_speed_current = 15;
    }

    zombie_check_special_weapon();
    Add_speedXZ(2048);
}
// ---------------------------------------------------------------------------
// benddown_and_standup @ 0x00437690
// zombie_damage_behavior_tbl[7]: play animation 13 once, then reset to the idle
// state on behaviour 3. This is where a zombie that runs out of poise lands
// (zombie_damaged stores action_behavior = 7 for exactly this).
// ---------------------------------------------------------------------------
static void benddown_and_standup(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0x0D;
    } else if (ENTITY->action_state != 1) {
        return;
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        // MOV dword [_ENTITY+0x84],0x30101 - state=1, ignore=1, behaviour=3.
        ENTITY->state              = ZOMBIE_STATE_IDLE;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior    = 3;
        ENTITY->action_state       = 0;
        // Plain byte stores, and a WORD compare spanning g_stageId + g_roomId.
        ENTITY->behavior_flags = 0;
        // lab naked zombie behavior on o-passage room
        if (*(unsigned short*)&g_stageId == (STAGE_LABORATORY | (ROOM_LAB_B3_O_PASSAGE << 8))) {
            ENTITY->behavior_flags = 4;
        }
        ENTITY->hit_state = 0;
    }

    zombie_check_special_weapon();
}
// ---------------------------------------------------------------------------
// zombie_chase_walk @ 0x00434eb0
// The walking chase: ease into the walk cycle, then hold it while periodically
// weaving off-course, and freeze for a second if the player is already being
// grabbed nearby. Reached as zombie_action_tbl[2] and
// zombie_damage_behavior_tbl[10].
//
// Sub-states 0 and 2 fall into the next one; every path ends at Add_speedXZ(0).
// ---------------------------------------------------------------------------
static void zombie_chase_walk(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->move_speed_current = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 2;
        ENTITY->blend_counter = 3;
        // FALLS THROUGH into sub-state 1.
    case 1:
        ENTITY->action_state += (char)Joint_move(0, ENTITY->animHeader,
                                                ENTITY->animBase, 0x400);
        break;

    case 2:
        ENTITY->move_speed_current = (unsigned short)ENTITY->move_speed;
        ENTITY->action_state = 3;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 3;
        ENTITY->blend_counter = 3;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x7F) + 10);
        ENTITY->next_turn_timer = 0;
        // FALLS THROUGH into sub-state 3.
    case 3:
        if (ENTITY->timing_control == 1) {
            if (ENTITY->animation_frame_id == 8)  Snd_em(1);   // footstep
            if (ENTITY->animation_frame_id == 29) Snd_em(1);   // footstep
        }
        {
            // Value-before-decrement: when it hits zero, arm a weave of
            // 20..35 frames and reset the walk timer past it.
            short ticks = (short)ENTITY->action_ticks_counter;
            ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
            if (ticks == 0) {
                ENTITY->next_turn_timer = (unsigned short)((g_RandSeed & 0x0F) + 0x14);
                ENTITY->action_ticks_counter =
                    (unsigned short)((short)ENTITY->next_turn_timer + (g_RandSeed & 0x7F));
            }
        }
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        // Director's Cut fast zombie: advance the walk cycle a second time.
        if (g_bDcMode && ENTITY->dc_double_step != 0) {
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        }
        ENTITY->move_speed_current = 45;
        // A DC addition that is inert in the shipped data: zombie_init sets
        // move_speed to 45 and nothing raises it. Ported as written.
        if (g_bDcMode && ENTITY->move_speed > 45) {
            ENTITY->move_speed_current = (unsigned short)ENTITY->move_speed;
        }

        if ((short)ENTITY->next_turn_timer != 0) {
            // Weaving: turn at MINUS twice turn_speed and crawl at 10.
            g_animFrameIdSave = 7;
            entity_update_wander_turn(*(unsigned short*)((char*)ENTITY + 0x17a),
                                      &ENTITY->dir_control_flags,
                                      (unsigned char*)((char*)ENTITY + 0x179),
                                      (unsigned short)(ENTITY->turn_speed * -2), 60);
            ENTITY->move_speed_current = 10;
            ENTITY->next_turn_timer = (unsigned short)((short)ENTITY->next_turn_timer - 1);
        }

        {
            int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
                   - ENTITY->scaMatrixData.localMatrix.t[2];
            int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
                   - ENTITY->scaMatrixData.localMatrix.t[0];
            int absDz = (dz ^ (dz >> 31)) - (dz >> 31);
            int absDx = (dx ^ (dx >> 31)) - (dx >> 31);
            g_playerDisplacement = absDz - (dx >> 31) + absDx;
        }

        // Someone else already has the player: stand still for 60 frames.
        if (g_playerEntity.isBeingAttackedFlag != 0 && g_playerDisplacement < 800) {
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_state++;
            ENTITY->action_ticks_counter = 60;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->animationId = 0;
            ENTITY->blend_counter = 3;
            ENTITY->move_speed_current = 0;
        }
        break;

    case 4:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (g_bDcMode && ENTITY->dc_double_step != 0) {
            Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        }
        if (--ENTITY->action_ticks_counter == 0) {
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_state = 0;
        }
        break;
    }

    Add_speedXZ(0);
    // ...and apply the frame's XZ motion a second time. Two Joint_move calls
    // plus two Add_speedXZ calls per frame is the whole "fast zombie" - the DC
    // introduces no new speed constant for it.
    if (g_bDcMode && ENTITY->dc_double_step != 0) {
        Add_speedXZ(0);
    }
}
// ---------------------------------------------------------------------------
// fast_player_facing @ 0x00435c60
// Spin to face the player fast (0x54 per frame) for a random 30..93 frames. If
// the turn completes or the timer runs out, hand over to behaviour 2 with the
// player's current position as the new waypoint.
//
// Reached as zombie_action_tbl[3] and zombie_damage_behavior_tbl[11].
// ---------------------------------------------------------------------------
static void fast_player_facing(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->hit_state = 0;
        // Return value deliberately discarded here; this call only exists for
        // its side effect on the angle bookkeeping inside turn_toward_target.
        turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x400);
        ENTITY->animationId = 3;
        ENTITY->action_state = 1;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x3F) + 0x1E);
    }

    int step = turn_toward_target((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x54);
    g_animFrameIdSave = (unsigned int)(int)(short)step;

    // Value-before-decrement test (0x00435d2x).
    short ticks = (short)ENTITY->action_ticks_counter;
    ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);

    if (ticks != 0 && (int)g_animFrameIdSave != 0) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->angle = ENTITY->angle + (short)g_animFrameIdSave;
        return;
    }

    // `MOV dword [EAX+0x84],0x2020001` - Ghidra prints the immediate as
    // `&g_enemy_state` because the constant looks like an address. It is just
    // state=1, ignore=0, action_behavior=2, action_state=2.
    ENTITY->state              = ZOMBIE_STATE_IDLE;
    ENTITY->ignore_player_flag = 0;
    ENTITY->action_behavior    = 2;
    ENTITY->action_state       = 2;
    ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
}

static void* const zombie_damage_behavior_tbl[12] = {
    (void*)short_push_back,        // [0]  0x00436c80
    (void*)short_push_back,        // [1]  0x00436c80
    (void*)push_and_stagger,       // [2]  0x00436f00
    (void*)push_and_stagger,       // [3]  0x00436f00
    (void*)push_and_drop,          // [4]  0x00437450
    (void*)explode_leg_and_drop,   // [5]  0x00437050
    (void*)long_push_back,         // [6]  0x00437540
    (void*)benddown_and_standup,   // [7]  0x00437690 - the eating-zombie case
    (void*)zombie_idle,            // [8]  0x004349d0
    (void*)zombie_slow_walk,       // [9]  0x00434cd0
    (void*)zombie_chase_walk,      // [10] 0x00434eb0
    (void*)fast_player_facing      // [11] 0x00435c60
};

// ============================================================================
// zombie_damaged @ 0x00433db0
// Zombie hit reaction state. Dispatches damage behavior based on hit flags,
// increments hit counter, triggers falldown when threshold exceeded, or
// when stagger_timer expires during sustained damage.
// ============================================================================
// 0x00433db0 builds a 64-byte hit-threshold table on the stack from
// immediates, then indexes it with `g_RandSeed & 0x1F`: the second half
// (offsets 0x20-0x3f, lower thresholds) while SCENARIO_FLAG 0x7B is clear
// (first playthrough), the first half (offsets 0x00-0x1f, higher thresholds)
// when it is set (second playthrough).
static const unsigned char zombie_hit_threshold_normal_tbl[32] = {
    1, 2, 3, 2, 3, 3, 2, 3,
    3, 2, 2, 3, 2, 3, 2, 3,
    2, 3, 2, 4, 2, 2, 2, 2,
    3, 2, 2, 3, 2, 2, 2, 3
};

static const unsigned char zombie_hit_threshold_hard_tbl[32] = {
    3, 2, 3, 3, 2, 3, 4, 3,
    2, 3, 3, 4, 3, 2, 3, 3,
    3, 2, 3, 3, 3, 3, 2, 3,
    3, 2, 3, 4, 3, 4, 3, 2
};

void zombie_damaged(void)
{
    if (ENTITY->ignore_player_flag == 0) {
        unsigned char behType = ENTITY->behavior_flags & 0x0F;

        // 0x00433dd0: If behavior_step bit 2 set, restore previous state
        if ((ENTITY->behavior_step & 0x04) != 0) {
            *(unsigned int*)(&ENTITY->state) = *(unsigned int*)(&ENTITY->state_mirror);
            ENTITY->hit_state = 0;
            return;
        }

        // 0x00433f1d: `MOV dword [EAX+0x84],0x3070101` - one dword store across
        // state / ignore_player_flag / action_behavior / action_state:
        //   state = 1 (IDLE), ignore = 1, action_behavior = 7, action_state = 3
        // The old code set state = 3 (DIE) and never wrote action_behavior, so a
        // zombie that ran out of poise entered the death sequence instead of the
        // benddown_and_standup reaction.
        if ((ENTITY->action_speed & 0x80) != 0) {
            ENTITY->state              = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior    = 7;
            ENTITY->action_state       = 3;
            ENTITY->blend_counter = 0;
            ENTITY->hit_state = 1;
            if ((ENTITY->behavior_step & 1) == 0) {
                Snd_em(9);  // falldown moan
            }
            return;
        }

        // 0x00433e54: Increment hit counter if hit-with-bullet flag set
        if ((ENTITY->hit_state & 0x78) == 0x08) {
            ENTITY->action_speed++;
            if ((signed char)ENTITY->hit_threshold <= (signed char)ENTITY->action_speed) {
                if ((ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) == 0) {
                    // hit threshold exceeded → trigger falldown
                    ENTITY->action_speed = 0x80;
                    if (g_bDcMode) {
                        // DC: the overlay's reload (STAGE1.EXE 0x80107000-20)
                        // is unconditional - always the hard table (see the
                        // note on zombie_init's threshold store).
                        ENTITY->hit_threshold = zombie_hit_threshold_hard_tbl[g_RandSeed & 0x1F];
                    } else if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)
                        ENTITY->hit_threshold = zombie_hit_threshold_normal_tbl[g_RandSeed & 0x1F];
                    else
                        ENTITY->hit_threshold = zombie_hit_threshold_hard_tbl[g_RandSeed & 0x1F];
                }
            }
        }

        // 0x00433ecf: stagger_timer countdown during sustained damage
        if ((ENTITY->hit_state & 0x78) == 0x10
            && ENTITY->action_state == 0
            && --ENTITY->stagger_timer == 0
            && (ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) == 0)
        {
            ENTITY->action_speed = 0x80;
        }

        // 0x00433f00: Determine damage behavior from table
        ENTITY->action_behavior = zombie_damage_action_tbl[
            (ENTITY->hit_state & 7)
            + (unsigned)(ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) * 3];

        // 0x00433f30: Adjust behavior based on hit direction
        if ((ENTITY->hit_state - 1) & 0x02) {
            unsigned int angle = turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 1024);
            ENTITY->action_behavior += ((angle >> 10) & 1);
        }

        // 0x00434089-0x004340b4: eating zombie. The original sets
        // action_behavior = 7 for behaviour 7/5 REGARDLESS of the animation id
        // (Ghidra folds the store into the condition as a comma expression);
        // only action_state = 1 is gated on animationId == 13.
        if (behType == 7 || behType == 5) {
            ENTITY->action_behavior = 7;
            if (ENTITY->animationId == 13) {
                ENTITY->action_state = 1;
            }
        }

        ENTITY->ignore_player_flag = 1;
    }

    // 0x004340cd: `CALL [ECX*4 + 0x4bb330]` - unconditional, no `< 5` bound.
    ((void(*)())zombie_damage_behavior_tbl[ENTITY->action_behavior])();
}

// ============================================================================
// zombie_die @ 0x004340e0
// Zombie death sequence state machine. Handles fall-back animation,
// headshot/magnum pushback, and laying-down death.
// ============================================================================
void zombie_die(void)
{
    if (ENTITY->ignore_player_flag == 0) {
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 0;

        // 0x00434110: Determine death behavior
        // `CMP CL,0x5` at 0x00434118 - the whole byte, not (flags & 0xF).
        if ((ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN)
            || (ENTITY->action_speed & 0x80)
            || ENTITY->behavior_flags == ZOMBIE_BEH_5)
        {
            ENTITY->action_behavior = 2;
        }

        // 0x00434151: Headshot death if hit from behind
        if ((ENTITY->hit_state & 7) == 4) {
            int angle = turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 1024);
            if ((short)angle == 0) {
                ENTITY->action_behavior = 1;
            }
        }

        // 0x0043419b: Magnum/explosive death if headshot joint flag set
        if (ENTITY->action_behavior == 1
            && (*(unsigned char*)((int)ENTITY->jointsStructs + 0xF8) & 0x40)
            && (g_RandSeed & 1))
        {
            ENTITY->action_behavior = 3;
        }

        ENTITY->behavior_step &= ~0x04;
        // `PUSH 0xbe987c` at 0x004341a8 - that is g_EnemiesFlags, NOT
        // g_roomItemsFlags (0x00be989c). Raising the death bit in the item bank
        // meant a killed zombie never fired its room event and scribbled on
        // item state instead.
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);  // 0x163
    }

    // 0x004341f0: Death behavior dispatch
    switch (ENTITY->action_behavior) {
    case 0:
        // 0x004341d2: standard death. The old code had case 1's body copied in
        // here: it dropped the second sound cue, added case 1's
        // `frame < 5 -> +90` speed bump that this branch does not have, and
        // called Add_speedXZ(2048) where the original passes 0.
        ENTITY->animationId = 8;
        ENTITY->move_speed_current = 20;
        if (ENTITY->timing_control == 1) {
            if (ENTITY->animation_frame_id == 8)    Snd_em(1);
            if (ENTITY->animation_frame_id == 0x1a) Snd_em(0);
        }
        zombie_dead_animation();
        Add_speedXZ(0);
        return;
    case 1:
        // 0x00434239: headshot death
        ENTITY->animationId = 10;
        ENTITY->move_speed_current = 20;
        if (ENTITY->animation_frame_id == 18 && ENTITY->timing_control == 1)
            Snd_em(0);
        if (ENTITY->animation_frame_id < 5)
            ENTITY->move_speed_current = (unsigned short)(ENTITY->move_speed_current + 90);
        zombie_dead_animation();
        Add_speedXZ(2048);
        return;
    case 2:
        // 0x00434246: Lay-down death (already on ground)
        ENTITY->animationId = 16;
        zombie_dead_animation();
        return;
    case 3:
        // 0x00434251: Magnum shot pushback
        magnum_shot_pushback();
        return;
    }
}

// ============================================================================
// no_action @ 0x004342d0 - one instruction, a RET. It is zombie_states_table[4].
// The old pass named this slot zombie_dead_animation, which is a different and
// much larger function at 0x00437740 (implemented further down).
// No-op — the dead animation is handled by joint/sprite system directly.
// ============================================================================
static void zombie_no_action(void) { }

// ============================================================================
// zombie_dead_animation @ 0x00437740
// The tail of every zombie death: run the death animation out, then hold the
// corpse for death_timer frames while its shadow shrinks, raise the room event
// flag, and finally park it. Also the ONE place a "dead" zombie can get back up.
//
// The old pass had this as an empty `{ }` stub filed at 0x004342d0 (which is
// actually no_action), so nothing ever counted down death_timer, nothing raised
// the death room-event flag from here, and no corpse ever settled or revived.
//
// Called from zombie_die cases 0/1/2, plus zombie_headshot (0x00454d20) and
// zombie_dying (0x00454ed0) in the SCD action path.
// ============================================================================
void zombie_dead_animation(void)
{
    // Same prone-body probe pair as zombie_update, at +/-800 instead of +/-600.
    SVECTOR bodyEndFront = { 800, 0, 0, 0 };
    SVECTOR bodyEndBack  = { -800, 0, 0, 0 };

    int joint = (int)ENTITY->jointsStructs;

    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->death_timer = 70;
        ENTITY->blend_counter = 3;
        ENTITY->hit_state = 1;
        // Skip the death moan if behavior_step bit 0 is set, or the head joint
        // carries any of 0xCC (already blown apart).
        if ((ENTITY->behavior_step & 1) == 0
            && (*(unsigned char*)(joint + 0xF8) & 0xCC) == 0) {
            Snd_em(5);
        }
        // fall through
    case 1:
        ENTITY->action_state += (char)Joint_move(0, ENTITY->animHeader,
                                                ENTITY->animBase, 0x400);
        break;

    case 2: {
        // Probe whether the prone body has somewhere to lie, WITHOUT letting the
        // probe move anything: the original saves entity+0x34..0x43 (localMatrix
        // t[0..2] plus the first word of worldMatrix) and the angle, then puts
        // them all back.
        short savedAngle = ENTITY->angle;
        VECTOR savedPos;
        savedPos.x   = ENTITY->scaMatrixData.localMatrix.t[0];
        savedPos.y   = ENTITY->scaMatrixData.localMatrix.t[1];
        savedPos.z   = ENTITY->scaMatrixData.localMatrix.t[2];
        savedPos.pad = *(int*)((char*)ENTITY + 0x40);

        unsigned char blocked = check_room_collision_two_point(&bodyEndBack, &bodyEndFront);
        g_playerDisplacement = blocked;

        ENTITY->scaMatrixData.localMatrix.t[0] = savedPos.x;
        ENTITY->scaMatrixData.localMatrix.t[1] = savedPos.y;
        ENTITY->scaMatrixData.localMatrix.t[2] = savedPos.z;
        *(int*)((char*)ENTITY + 0x40) = savedPos.pad;
        ENTITY->angle = savedAngle;

        // The "it gets back up" path. Every one of these has to hold:
        //   not SCD-controlled (0x40), behaviour byte != 4, head intact,
        //   not stage/room 0x0201, died with the plain fall animation (8),
        //   the prone body has room, 1-in-4 on the rand, and
        //   g_main_state_flags bit 16 clear.
        // The stage/room test is a WORD compare at 0x00be9820, which spans
        // g_stageId and g_roomId - low byte stage 1, high byte room 2.
        if ((ENTITY->behavior_flags & 0x40) == 0
            && ENTITY->behavior_flags != 0x04
            && (*(unsigned char*)((int)ENTITY->jointsStructs + 0xF8) & 0xCC) == 0
            && *(unsigned short*)&g_stageId != (STAGE_MANSION_2F | (ROOM_DINING_ROOM_2F << 8))
            && ENTITY->animationId == 8
            && g_playerDisplacement == 0
            && (g_RandSeed & 3) == 0
            && (g_main_state_flags & MSF_INTENSITY_RAMP) == 0)
        {
            ENTITY->behavior_flags = 3;
            ENTITY->health = 1;
            // MOV dword [EDX+0x84],1 - state=1, ignore=0, behavior=0, sub=0
            ENTITY->state = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
            ENTITY->hit_state = 0;
            ENTITY->scaMatrixData.localMatrix.t[1] = 1;   // dword at 0x38
            ENTITY->behavior_step &= ~0x04;
            ENTITY->status_flags &= 0x1F;
            return;
        }

        ENTITY->health = -1;
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00ffff50);
        BillboardAdjSize(&ENTITY->pushVelocity, -100, -100);
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        ENTITY->action_state = 3;
        ENTITY->status_flags |= 0x0E;
        // fall through
    }
    case 3:
        ENTITY->move_speed_current = 0;
        BillboardAdjSize(&ENTITY->pushVelocity, 6, 6);
        ENTITY->death_timer--;
        if (ENTITY->death_timer == 0) {
            ENTITY->action_state = 4;
            return;
        }
        break;

    case 4:
        ENTITY->move_speed_current = 0;
        if ((ENTITY->behavior_flags & 0x40) != 0) {
            // SCD-spawned corpse: tell the script it is done.
            Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
            // MOV word [ECX+0x86],SI - clears action_behavior AND action_state.
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
        }
        return;
    }
}

// ============================================================================
// Zombie attack data tables
// ============================================================================

// ---------------------------------------------------------------------------
// Zombie attack data tables - all read from the exe.
//
// The three animation tables are ONE 12-byte block at 0x004bb3a0 read through
// three overlapping bases (attack anim +0, player anim +1, keyframe +2) - the
// same multi-view trick as zombie_states_table / zombie_behavior_tbl. Each row
// is one attacking_direction (0..3): standing/not-facing, standing/facing,
// laying/not-facing, laying/facing. The old pass declared three separate
// 3-row tables at invented "0x004bb4??" addresses whose values matched nothing
// in the exe - every attack played the wrong animation.
// ---------------------------------------------------------------------------
static const unsigned char zombie_attack_anim_tbl[12] = {
    0x11, 0x00, 0x00,   // dir 0: attack anim 17, player anim 0,  keyframe 0
    0x14, 0x03, 0x00,   // dir 1: attack anim 20, player anim 3,  keyframe 0
    0x17, 0x06, 0x06,   // dir 2: attack anim 23, player anim 6,  keyframe 6
    0x1A, 0x09, 0x0E    // dir 3: attack anim 26, player anim 9,  keyframe 14
};

// 0x004bb3a1 / 0x004bb3a2 - the player-anim and keyframe views of the block above.
static const unsigned char* const zombie_attack_player_anim_tbl = &zombie_attack_anim_tbl[1];
static const unsigned char* const zombie_attack_keyframe_tbl   = &zombie_attack_anim_tbl[2];

// 0x004bb3b0 - player attackDirection per direction. 0x7FFF is the "keep the
// player's facing" sentinel; the original reads these as shorts.
static const short attack_dir_offset_tbl[4] = { 0x7FFF, 0x0000, 0x7FFF, 0x0000 };

// 0x004bb3c8 - first-playthrough ("normal") damage. Behaviours 2/3 (laying)
// bite for less; behaviours 9+ never attack, so their rows are zero.
static const unsigned char zombie_damage_normal_tbl[16] = {
    10, 10, 6, 6, 10, 10, 10, 10,
    10, 0, 0, 0, 0, 0, 0, 0
};

// 0x004bb3d8 - second-playthrough ("hard") damage.
static const unsigned char zombie_damage_hard_tbl[16] = {
    12, 12, 9, 9, 12, 12, 12, 12,
    12, 0, 0, 0, 0, 0, 0, 0
};

// ---------------------------------------------------------------------------
// player_death_animations_tbl @ 0x004bb3b8 - indexed by attacking_direction.
// Each entry tints a pair of the player's joints (the death wound) after the
// zombie kills the player mid-attack. The original swaps ENTITY to the player
// for the tint (JointApplyColorTint pairs the weapon joint against ENTITY),
// then restores it. Entries 0 and 1 are the same code twice in the original.
// ---------------------------------------------------------------------------
static void player_death_anim_tint(void* joints, int jointOffA, int jointOffB)
{
    g_entity_bkp = (unsigned int)ENTITY;
    ENTITY = (Entity*)&g_playerEntityPointer;
    JointApplyColorTint((JointStruct*)((char*)joints + jointOffA), 0x30, 0x80820, &DAT_00606060);
    JointApplyColorTint((JointStruct*)((char*)joints + jointOffB), 0x30, 0x80820, &DAT_00606060);
    ENTITY = (Entity*)g_entity_bkp;
}

static void player_death_anim_0(void* joints) { player_death_anim_tint(joints, 0x5D0, 0x7C); }  // 0x00435ae0
static void player_death_anim_1(void* joints) { player_death_anim_tint(joints, 0x5D0, 0x7C); }  // 0x00435b40 - same code as [0]
static void player_death_anim_2(void* joints) { player_death_anim_tint(joints, 0x1F0, 0x26C); }  // 0x00435ba0
static void player_death_anim_3(void* joints) { player_death_anim_tint(joints, 0x3E0, 0x45C); }  // 0x00435c00

static void* const player_death_animations_tbl[4] = {
    (void*)player_death_anim_0,
    (void*)player_death_anim_1,
    (void*)player_death_anim_2,
    (void*)player_death_anim_3
};

// ---------------------------------------------------------------------------
// zombie_attack sub-states 5, 6, 8 and the shared 7/9 tail. Factored out of the
// switch so case 4 can fall straight into them, as the original's gotos do.
// ---------------------------------------------------------------------------
static void zombie_attack_withdraw(void)
{
    entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->state = ZOMBIE_STATE_DAMAGED;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 6;
        ENTITY->action_state = 0;
        // AND byte [_ENTITY],0xf5 - clears bits 1 and 3 (0x0A), not bit 0.
        // Bit 1 is the SCA "deactivated" bit; clearing bit 0 instead left the
        // zombie non-colliding after every attack.
        ENTITY->status_flags &= 0xF5;
        ENTITY->hit_state = 1;
    }
}

static void zombie_attack_head_bite(void)
{
    // ---- HEAD BITE: grab player's head, apply effects ----
    entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
    ENTITY->action_state += (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    if (zombie_attack_keyframe_tbl[ENTITY->attacking_direction * 3]
        == ENTITY->animation_frame_id)
    {
        ENTITY->action_state = 10;
        int jointPtr = (int)ENTITY->jointsStructs;

        joint_setup_attack_effect(jointPtr + 0xF8, 0x1E, 0, 3);

        // 0x004354f9: the spawn offset is the 4-dword block at
        // *(g_deadMoveValue + 0x14), staged through g_playerPosScratch exactly
        // like explode_leg_and_drop - NOT a zeroed local. With (0,0,0) the
        // billboards were placed at the joint's own origin.
        {
            const int* spawn = (const int*)((char*)g_deadMoveValue + 0x14);
            g_playerPosScratch.x   = spawn[0];
            g_playerPosScratch.y   = spawn[1];
            g_playerPosScratch.z   = spawn[2];
            g_playerPosScratch.pad = spawn[3];
        }

        // ENTITY + 0x20 is scaMatrixData.localMatrix, not scaMatrixData: the
        // effect renderer memcpy's 0x20 bytes from this pointer straight into
        // the slot's MATRIX (EffectActor_UpdateAndRender), so the bare
        // scaMatrixData address shifted every row by 4 bytes and the
        // translation came out of localMatrix.m[7..8] - garbage coordinates.
        Effect_CreateBillboard(3, 0, 0, (void*)(jointPtr + 0x13C), &g_playerPosScratch, 0);
        g_playerPosScratch.x += 500;
        Effect_CreateBillboard(4, 0, 0x800, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 1, 0x5E8, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 2, 0x9F4, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 4, 0xB84, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 2, 0xDB8, &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);

        JointApplyColorTint((JointStruct*)(jointPtr + 0x174), 0x30, 0x80820, &DAT_00606060);
        JointApplyColorTint((JointStruct*)(jointPtr + 0x2E8), 0x30, 0x80820, &DAT_00606060);
        Snd_em(6);
    }
}

static void zombie_attack_vomit(void)
{
    // ---- VOMITING ATTACK: vomit on player ----
    entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
    {
        char looped = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state += looped;
    }

    {
        int jointPtr = (int)ENTITY->jointsStructs;
        if (zombie_attack_keyframe_tbl[ENTITY->attacking_direction * 3]
            == ENTITY->animation_frame_id)
        {
            unsigned char* jointFlag = (unsigned char*)(jointPtr + 0xF8);
            *jointFlag |= 0x88;
            JointApplyColorTint((JointStruct*)jointFlag, 0x30, 0x80820, &DAT_00606060);
            *(unsigned short*)(jointPtr + 0xFC) = 0xFED4;
            *(unsigned short*)(jointPtr + 0xFE) = 0xFA;
            *(unsigned char*)(jointPtr + 0xFA) = 0;
            *(unsigned char*)(jointPtr + 0xFB) = 0;

            // Same g_deadMoveValue spawn block as the head-bite path
            // (0x004356f2); the old zeroed local put the spray at the joint
            // origin.
            const int* spawn = (const int*)((char*)g_deadMoveValue + 0x14);
            g_playerPosScratch.x   = spawn[0];
            g_playerPosScratch.y   = spawn[1];
            g_playerPosScratch.z   = spawn[2];
            g_playerPosScratch.pad = spawn[3];
            Effect_CreateBillboard(0, 0, 0, (void*)(jointPtr + 0x13C), &g_playerPosScratch, 0);
        }

        if ((*(unsigned char*)(jointPtr + 0xF8) & 0x40) != 0) {
            ENTITY->splatter_flag = 1;
            ENTITY->bob_speed = 0;
        }
    }

    if (g_playerEntity.animation_frame_id == 6) {
        Snd_em(7);
    }
}

// zombie_attack cases 7 and 9 are the same code twice in the original, only the
// order of two stores differs. Both park a headless corpse.
static void zombie_attack_headless_death(void)
{
    BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00ffff50);
    Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
    ENTITY->death_timer = 0x46;
    ENTITY->state              = ZOMBIE_STATE_DIE;
    ENTITY->ignore_player_flag = 1;
    ENTITY->action_behavior    = 2;
    ENTITY->action_state       = 3;
    ENTITY->status_flags |= 0x0E;
    ENTITY->health = -1;
    ENTITY->hit_state = 1;
}

// ============================================================================
// zombie_attack @ 0x004342e0 (thunk) -> 0x00435200 (real)
// Full zombie attack FSM — 11 states controlling bite/grab/vomit attacks.
// Manages player reaction animation, damage application, head explosion,
// vomiting effects, and attack withdrawal.
// ============================================================================
void zombie_attack(void)
{
    char reduce = 0;  // declared before switch to avoid case-label initialization errors

    switch (ENTITY->action_state) {
    case 0:
        // ---- INIT: setup attack type ----
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->action_state = 1;
        ENTITY->blend_counter = 3;
        ENTITY->status_flags |= (ENTITY_STATUS_ACTIVE | ENTITY_STATUS_DEAD);
        Snd_em(4);                      // attack roar

        // `(behavior_flags & 2) + is_facing_toward_entity(player)` - 0..3:
        // standing/not-facing, standing/facing, laying/not-facing, laying/facing.
        // The old `? 0 : 0` dropped the facing term, so a laying zombie played
        // the standing attack rows and directions 2/3 were unreachable.
        ENTITY->attacking_direction =
            (ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN)
            + (unsigned char)is_facing_toward_entity(&g_playerEntity);

        ENTITY->animationId =
            zombie_attack_anim_tbl[ENTITY->attacking_direction * 3];
        g_playerEntity.attackAnim =
            zombie_attack_player_anim_tbl[ENTITY->attacking_direction * 3];
        g_playerEntity.attackDirection =
            attack_dir_offset_tbl[ENTITY->attacking_direction];

        ENTITY->hit_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;

        snap_player_to_grab_position(&g_playerEntity);
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.animationId = 5;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.directionAngle = ENTITY->angle;
        break;

    case 1:
        // ---- WIND-UP: play attack wind-up animation ----
        entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
        ENTITY->action_state += (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        break;

    case 2:
        // ---- BITE START: transition to damage loop ----
        ENTITY->action_state = 3;
        ENTITY->animationId = ENTITY->animationId + 1;
        ENTITY->timing_control = 0;
        ENTITY->action_ticks_counter = 0;
        ATTACK_TIMER = 105;
        // The player attacking during the grab cuts the bite to 30 frames
        // (0x0043539f: CMP word [player+0xE2]). Missing in the old pass, so
        // the bite always ran the full 105 frames.
        if (g_playerEntity.attackTimer != 0) {
            ATTACK_TIMER = 30;
        }
        break;

    case 3:
        // ---- DAMAGE LOOP: deal damage every 19 frames ----
        // Value-before-increment: the original tests the OLD counter (0 on the
        // first frame of the loop, so the first bite lands immediately), then
        // stores old+1. The old pass incremented first, shifting every bite one
        // cycle later - first hit at frame 19 instead of frame 0.
        {
            short tick = (short)ENTITY->action_ticks_counter;
            ENTITY->action_ticks_counter = (unsigned short)(tick + 1);
            if (tick % 19 == 0) {
                unsigned char damage;
                if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)
                    damage = zombie_damage_normal_tbl[ENTITY->behavior_flags & 0x0F];
                else
                    damage = zombie_damage_hard_tbl[ENTITY->behavior_flags & 0x0F];
                g_playerEntity.health -= damage;
                Snd_em(3);

                Effect_CreateBillboard(0, 0,
                    g_playerEntity.directionAngle + 2048,
                    (void*)g_deadMoveValue,
                    (void*)((int)ENTITY->jointsStructs + 0x150), 0);

                if (g_playerEntity.health < 0
                    && (ENTITY->attacking_direction & 2) != 0) {
                    g_playerEntity.health = 1;
                }
            }
        }

        entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

        reduce = reduce_attack_time_by_btn_press();
        ATTACK_TIMER -= ((unsigned char)reduce + 1);

        if ((char)ATTACK_TIMER < 0) {
            ENTITY->action_state = 4;
            g_playerEntity.action_state = 3;
        }

        // Player died during attack
        if (g_playerEntity.health < 0) {
            ENTITY->state = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior = 5;
            ENTITY->action_state = 0;
            ENTITY->action_behavior +=
                (((int)ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) ? -5 : 0);

            g_playerEntity.animationId = 1;
            g_playerEntity.action_behavior = 200;

            // Blood spray from the zombie's mouth joint (0x00435a0a), then the
            // player's death pose per attack direction (0x00435aa1). The old
            // pass stopped at the anim fields, so a player killed mid-bite kept
            // the grab pose with no wound tint. (The 4-dword spawn block is
            // copied out of *(g_deadMoveValue + 0x14) like explode_leg_and_drop.)
            {
                const int* spawn = (const int*)((char*)g_deadMoveValue + 0x14);
                g_playerPosScratch.x   = spawn[0];
                g_playerPosScratch.y   = spawn[1];
                g_playerPosScratch.z   = spawn[2];
                g_playerPosScratch.pad = spawn[3];
            }
            Effect_CreateBillboard(0, 0, 0x200,
                (void*)((int)ENTITY->jointsStructs + 0x13C),
                &g_playerPosScratch, 0);
            ((void(*)(void*))player_death_animations_tbl[ENTITY->attacking_direction])(
                g_playerEntityPointer.jointsStructs);
        }
        break;

    case 4:
        // ---- RELEASE: end attack or transition to vomit/headbite ----
        // The original does NOT break out of any of the three paths here: at
        // 0x004353xx each one is a `goto` straight into the case 5 / 6 / 8 body,
        // so the chosen follow-up runs in this same frame. Breaking instead cost
        // one frame on every release. The three bodies are factored out below so
        // the fallthrough is expressible - a goto into those cases will not
        // compile, they declare locals.
        ENTITY->action_state = 5;
        ENTITY->animationId = ENTITY->animationId + 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;

        if ((ENTITY->behavior_flags & ZOMBIE_FLAG_LAYING_DOWN) == 0) {
            zombie_attack_withdraw();       // goto case 5
            break;
        }
        if ((g_playerEntity.id & 1) == 0 || ENTITY->attacking_direction != 2) {
            ENTITY->action_state = 6;
            zombie_attack_head_bite();      // goto case 6
            break;
        }
        ENTITY->action_state = 8;
        zombie_attack_vomit();              // falls into case 8
        break;

    case 5:
        zombie_attack_withdraw();
        break;

    case 6:
        zombie_attack_head_bite();
        break;

    case 8:
        zombie_attack_vomit();
        break;

    case 7:
        // ---- POST HEAD-EXPLOSION: enter dead-headless state ----
        zombie_attack_headless_death();
        break;

    case 9:
        // ---- POST-VOMIT: cleanup, enter dead-headless state ----
        zombie_attack_headless_death();
        break;

    case 10:
        // ---- POST HEAD-BITE: finish animation, transition to head-exploded ----
        entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            ENTITY->action_state = 7;
        }
        break;
    }
}

// ============================================================================
// zombie_action_update @ 0x00454ab0
// Per-behavior action dispatcher. Indexes into zombie_action_tbl by
// action_behavior (0x86) to run the current behavior's update logic.
// Behavior 1 (chase walk) further dispatches by action_state (0x87).
// ---------------------------------------------------------------------------
// zombie_action_update @ 0x00454ab0 - zombie_states_table[8], the SCD-driven
// path. TWENTY BYTES of real code, the same tail-jump shape as
// update_zombie_action:
//     MOV EAX,[_ENTITY] ; XOR ECX,ECX ; MOV CL,[EAX+0x86]
//     JMP dword ptr [ECX*4 + 0x4c05c8]
//
// The table at 0x004c05c8 is mostly NULL - only six slots are live, and the SCD
// only ever writes those action_behavior values. The old pass had a hand-rolled
// switch here with cases 0/1/2/4/5 that matched none of them (and whose inner
// braces were tangled enough that a `case 3:` sat inside `case 2:`).
// ---------------------------------------------------------------------------

// zombie_aggresive_roar @ 0x00454ae0 - table[2]. Roar, then walk to the SCD
// target at 0xC6/0xC8 and raise the script flag on arrival.
static void zombie_aggresive_roar(void)
{
    switch ((char)ENTITY->action_state) {
    case 0:
        ENTITY->action_state++;
        ENTITY->move_speed_current = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 2;
        ENTITY->blend_counter = 3;
        Snd_em(4);                          // roar
        // FALLS THROUGH into sub-state 1.
    case 1:
        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 1024) != 0) {
            ENTITY->action_state++;
            ENTITY->move_speed_current = 0x23;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->animationId = 3;
            ENTITY->blend_counter = 3;
        }
        break;

    case 2:
        g_playerPosScratch.x = (int)ENTITY->unk_c6;   // SCD target X
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = (int)ENTITY->unk_c8;   // SCD target Z
        entity_rotate_toward_target(&g_playerPosScratch, 64);

        if (ENTITY->animation_frame_id == 8)    Snd_em(1);   // footstep
        if (ENTITY->animation_frame_id == 0x1D) Snd_em(1);   // footstep

        // The reverse flag comes from scd_entity_flags bit 0, not a constant 0.
        Joint_move((char)(ENTITY->scd_entity_flags & 1),
                   ENTITY->animHeader, ENTITY->animBase, 0x400);
        {
            int dz = ENTITY->scaMatrixData.localMatrix.t[2] - (int)ENTITY->unk_c8;
            int dx = ENTITY->scaMatrixData.localMatrix.t[0] - (int)ENTITY->unk_c6;
            if (SquareRoot0(dz * dz + dx * dx) < 150) {
                ENTITY->action_state++;
            }
        }
        break;

    case 3:
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        if ((ENTITY->collisionFlags & 0x80) == 0) {
            // MOV word [_ENTITY+0x86],0 - action_behavior and action_state.
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
        } else {
            ENTITY->action_state = 2;       // loop back and keep walking
        }
        break;
    }

    Add_speedXZ(0);
}

// zombie_headshot @ 0x00454d20 - table[10]. The head bursts: spray the head
// joint, throw five billboards away from the player, then run the corpse tail.
static void zombie_headshot(void)
{
    ENTITY->animationId = 8;
    ENTITY->move_speed_current = 0x14;

    if (ENTITY->action_state == 0) {
        int joint = (int)ENTITY->jointsStructs;
        short away = (short)(g_playerEntity.directionAngle - ENTITY->angle);

        g_playerPosScratch.x = 100;
        g_playerPosScratch.y = -0xA3C;
        g_playerPosScratch.z = 0;
        joint_setup_attack_effect(joint + 0xF8, 30, 2, 3);
        Effect_CreateBillboard(3, 0, (short)(away + 0x800),
                               &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);

        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -600;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 3, 0, (void*)(joint + 0x44), &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 0, (short)(away + 1536),
                               &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 2, (short)(away + 1792),
                               &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(4, 3, (short)(away + 2304),
                               &ENTITY->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Snd_em(6);                          // head explode
    }

    if (ENTITY->animation_frame_id == 8)  Snd_em(1);
    if (ENTITY->animation_frame_id == 26) Snd_em(0);

    zombie_dead_animation();
    Add_speedXZ(0);
}

// zombie_dying @ 0x00454ed0 - table[11]. Crawl-and-die; note this one uses
// Add_speedXZ(2048) where zombie_headshot passes 0.
static void zombie_scd_dying(void)
{
    ENTITY->animationId = 10;
    ENTITY->move_speed_current = 0x14;

    if (ENTITY->animation_frame_id == 0x12) {
        Snd_em(2);                          // crawl
    }
    if (ENTITY->animation_frame_id < 5) {
        ENTITY->move_speed_current = (unsigned short)(ENTITY->move_speed_current + 90);
    }

    zombie_dead_animation();
    Add_speedXZ(2048);
}

// zombie_vomiting @ 0x00454f30 and zombie_vomiting2 @ 0x00454f80 - table[12] and
// [13]. Identical but for the animation id. Named with an scd_ prefix because
// zombie_vomiting is already taken by 0x00436520 on the non-SCD path.
static void zombie_scd_vomiting_common(unsigned char animId)
{
    ENTITY->animationId = animId;
    short_push_back();
    if ((ENTITY->action_state & 2) != 0) {
        Flg_on((int)g_SysFlags, ENTITY->scd_anim_param);
        // MOV word [_ENTITY+0x86],0
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 0;
    }
}

static void zombie_scd_vomiting(void)  { zombie_scd_vomiting_common(4); }
static void zombie_scd_vomiting2(void) { zombie_scd_vomiting_common(5); }

// zombie_action_tbl_scd @ 0x004c05c8, indexed by action_behavior. Slot 0 is the
// bare RET at 0x00454ad0 (a second no_action, distinct from 0x004342d0).
static void* const zombie_action_tbl_scd[14] = {
    (void*)zombie_no_action,       // [0]  0x00454ad0 - bare RET
    NULL,                          // [1]
    (void*)zombie_aggresive_roar,  // [2]  0x00454ae0
    NULL,                          // [3]
    NULL,                          // [4]
    NULL,                          // [5]
    NULL,                          // [6]
    NULL,                          // [7]
    NULL,                          // [8]
    NULL,                          // [9]
    (void*)zombie_headshot,        // [10] 0x00454d20
    (void*)zombie_scd_dying,       // [11] 0x00454ed0
    (void*)zombie_scd_vomiting,    // [12] 0x00454f30
    (void*)zombie_scd_vomiting2    // [13] 0x00454f80
};

void zombie_action_update(void)
{
    unsigned char behavior = ENTITY->action_behavior;
    // The original jumps unconditionally and would fault on the NULL slots; the
    // SCD never emits those values. Bounds-checked here only because indices
    // past 13 read whatever follows the table in .data.
    if (behavior < 14 && zombie_action_tbl_scd[behavior] != NULL) {
        ((void(*)())zombie_action_tbl_scd[behavior])();
    }
}

// ============================================================================
// dc_standup_lunge - Director's Cut behaviour 11 (PS1 STAGE1 0x8010cdf8).
//
// The DC's zombie_states_table has one entry more than the PS1 OG's, inserted
// so that it lands on behaviour 11; the port's table already carries that slot
// as a trailing NULL ([21]), and dc_apply_zombie_tables() points it here.
// Nothing in the USA data ever produces behaviour 11, and with Mode=OG the
// slot stays NULL exactly as the PC build ships it.
//
// A scripted entrance. zombie_init has already given this zombie the DC's third
// ScaInfo record (radius 1000) and cleared its collision (status_flags bit 2),
// so it plays animation 29 through without pushing anything. When Joint_move
// reports the animation finished it lunges - 487 straight ahead, then 991 at a
// 0x400 offset - turns 90 degrees, takes the standard hit box and collision
// back, and hands itself to the ordinary chase behaviour with behavior_flags
// cleared, so the next frame dispatches through behaviour 0.
//
// The only shipped user is entity id 0x16 (the Forest zombie) in the arrange
// balcony rooms STAGE9/ROOM9120+9121 and STAGEE/ROOME120+E121, which arrive
// with the DC RDTs.
// ============================================================================
static void dc_standup_lunge(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 29;
        // The DC sets this and never reads it back in this handler.
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x0F) + 0x2D);
    } else if (ENTITY->action_state != 1) {
        return;
    }

    if (ENTITY->animation_frame_id == 0x14) {
        Snd_em(4);
    }

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        ENTITY->move_speed_current = 0x1E7;   // 487
        Add_speedXZ(0);
        ENTITY->move_speed_current = 0x3DF;   // 991
        Add_speedXZ(0x400);
        ENTITY->angle = (short)(ENTITY->angle + 0x400);
        ENTITY->behavior_flags = 0;
        ENTITY->state = ZOMBIE_STATE_IDLE;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 3;
        ENTITY->action_state = 0;
        ENTITY->Sca_info = (unsigned int)g_pZombieScaInfo[0];
        ENTITY->status_flags &= 0xFB;         // collision back on
    }
}

// ============================================================================
// dc_apply_zombie_tables (port-only)
// Install the Director's Cut's two zombie dispatch edits: the extra state entry
// that lands on behaviour 11, and entity id 0x16 - the Forest zombie - which
// the DC drives with the ordinary zombie update. Called once per game start
// from InitializeGame, next to the other dc_apply_* hooks; a no-op with
// Mode=OG.
//
// Id 0x16 is read off the DC's own per-stage entity tables: only DC STAGE2
// (0x80120a00) and DC STAGE7 (0x8012aa0c) register slot 0x16, both with the
// same handler their slots 0/1/0x11 carry - the zombie. Neither OG overlay
// registers it at all. Those two are the overlays the arrange balcony rooms
// run under (STAGE9 folds onto stage 1 and STAGEE onto stage 6), which is
// exactly where the id 0x16 + behaviour 0xB records live.
// ============================================================================
void dc_apply_zombie_tables(void)
{
    if (!g_bDcMode) {
        return;
    }
    zombie_states_table[21] = (void*)dc_standup_lunge;   // = behavior[11]
    enemies_update_functions_tbl[ENEMY_ZOMBIE_FOREST] = (void*)zombie_update;
}

// ============================================================================
// zombie_chase_player @ 0x00433bb0
// Chases the player. Checks if close enough to attack, otherwise follows.
// ============================================================================
void zombie_chase_player(void)
{
    if ((ENTITY->behavior_step & 0x04) == 0
        && (ENTITY->action_speed & 0x80) == 0)
    {
        unsigned char canAttack = checkAngularViewAndDistance(700, 1500,
            (VECTOR*)&g_playerEntity.scaMatrixData.localMatrix.t);

        if (canAttack) {
            unsigned char lineOfSight = check_line_of_sight((VECTOR*)&g_playerEntity.scaMatrixData.localMatrix.t);
            if (!lineOfSight && g_playerEntity.isBeingAttackedFlag == 0) {
                ENTITY->angle = getAngleTowardsTarget(
                    g_playerEntity.scaMatrixData.localMatrix.t[0],
                    g_playerEntity.scaMatrixData.localMatrix.t[2]);
                // MOV word [_ENTITY+0x84],5 - a WORD store, so it also clears
                // ignore_player_flag at 0x85.
                ENTITY->state = ZOMBIE_STATE_ATTACK;
                ENTITY->ignore_player_flag = 0;
                ENTITY->action_state = 0;
                zombie_attack();
                return;
            }
        }
    }

    if (ENTITY->ignore_player_flag == 0) {
        zombie_update_player_distance();
    }
    update_zombie_action();
}

// ============================================================================
// zombie_pushed_back @ 0x00433c60
// Knockback state. Can transition to attack if close enough, otherwise
// continues pushback recovery.
// ============================================================================
void zombie_pushed_back(void)
{
    unsigned char canAttack = checkAngularViewAndDistance(512, 2200,
        (VECTOR*)&g_playerEntity.scaMatrixData.localMatrix.t);

    if (canAttack) {
        unsigned char lineOfSight = check_line_of_sight((VECTOR*)&g_playerEntity.scaMatrixData.localMatrix.t);
        if (!lineOfSight && g_playerEntity.isBeingAttackedFlag == 0) {
            ENTITY->angle = getAngleTowardsTarget(
                g_playerEntity.scaMatrixData.localMatrix.t[0],
                g_playerEntity.scaMatrixData.localMatrix.t[2]);
            // MOV word [_ENTITY+0x84],5 - a WORD store, so ignore_player_flag
            // at 0x85 is cleared with it.
            ENTITY->state = ZOMBIE_STATE_ATTACK;
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_state = 0;
            zombie_attack();
            return;
        }
    }

    if (ENTITY->ignore_player_flag == 0) {
        zombie_update_player_distance();
    }
    zombie_pushback_action();
}

// ============================================================================
// zombie_random_chase @ 0x00433cf0
// Staggered chase. Similar to chase but requires collision push flag clear.
// ============================================================================
void zombie_random_chase(void)
{
    if ((ENTITY->behavior_step & 0x04) == 0
        && (ENTITY->action_speed & 0x80) == 0
        && (ENTITY->collisionFlags & 0x08) == 0)
    {
        unsigned char canAttack = checkAngularViewAndDistance(700, 1500,
            (VECTOR*)&g_playerEntity.scaMatrixData.localMatrix.t);

        if (canAttack) {
            unsigned char lineOfSight = check_line_of_sight((VECTOR*)&g_playerEntity.scaMatrixData.localMatrix.t);
            if (!lineOfSight && g_playerEntity.isBeingAttackedFlag == 0) {
                ENTITY->angle = getAngleTowardsTarget(
                    g_playerEntity.scaMatrixData.localMatrix.t[0],
                    g_playerEntity.scaMatrixData.localMatrix.t[2]);
                // MOV word [_ENTITY+0x84],5 - a WORD store, so ignore_player_flag
                // at 0x85 is cleared with it.
                ENTITY->state = ZOMBIE_STATE_ATTACK;
                ENTITY->ignore_player_flag = 0;
                ENTITY->action_state = 0;
                zombie_attack();
                return;
            }
        }
    }

    if (ENTITY->ignore_player_flag == 0) {
        zombie_update_player_distance();
    }
    update_zombie_action();
}

// ============================================================================
// zombie_eating @ 0x00436690
// Eating corpse animation. Loops eating animation until player gets within
// 3000 units, then stands up and enters die state.
// ============================================================================
void zombie_eating(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = (unsigned char)g_RandSeed & 0x1F;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 0x1E;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0xF) + 0x2D);
        // FALLS THROUGH into case 1 in the original - no break at 0x004366e9.
    case 1:
        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400)) {
            ENTITY->animationId = (g_RandSeed & 1) ? 0x1D : 0x1E;
        }
        if (ENTITY->animation_frame_id == 0x19) {
            VECTOR eatPos = { 800, -300, 0, 0 };
            Effect_CreateBillboard(0, 0, 0, (void*)&ENTITY->scaMatrixData.localMatrix, &eatPos, 0);
            Snd_em(3);
        }
        {
            // The old `absDx - absDz + absDz` cancelled the Z term outright and
            // never published the result. The original is the same Manhattan
            // distance every other zombie routine computes, into
            // g_playerDisplacement.
            int dx = (int)g_playerEntity.scaMatrixData.localMatrix.t[0]
                   - (int)ENTITY->scaMatrixData.localMatrix.t[0];
            int dz = (int)g_playerEntity.scaMatrixData.localMatrix.t[2]
                   - (int)ENTITY->scaMatrixData.localMatrix.t[2];
            int absDx = (dx ^ (dx >> 31)) - (dx >> 31);
            int absDz = (dz ^ (dz >> 31)) - (dz >> 31);
            g_playerDisplacement = absDz - (dx >> 31) + absDx;
            if (g_playerDisplacement < 3000) {
                ENTITY->action_state++;
                return;
            }
        }
        break;
    case 2:
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0x0D;
        // FALLS THROUGH into case 3 in the original.
    case 3:
        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400)) {
            // Plain byte stores, not read-modify-write: behavior_flags is set to
            // 0, then to 4 outright in the stage/room special case.
            ENTITY->behavior_flags = 0;
            // CMP word [0x00be9820],0x504 - a WORD compare spanning g_stageId
            // (low byte, 4) and g_roomId (high byte, 5). The old 32-bit read
            // dragged in roomCameraId and attractMode_RoomCameraId as well.
            if (*(unsigned short*)&g_stageId == (STAGE_LABORATORY | (ROOM_LAB_B3_O_PASSAGE << 8))) {
                ENTITY->behavior_flags = 4;
            }
            // MOV dword [_ENTITY+0x84],0x30101 - state=1, ignore=1,
            // action_behavior=3, action_state=0. The old code set state=3 (DIE)
            // and never wrote action_behavior, so a zombie that finished eating
            // dropped into the death sequence instead of behaviour 3.
            ENTITY->state              = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior    = 3;
            ENTITY->action_state       = 0;
            // dword at 0x38 is localMatrix.t[1], not m[1][0].
            ENTITY->scaMatrixData.localMatrix.t[1] = 0;
        }
        break;
    }
}

// ============================================================================
// zombie_idle @ 0x004349d0
// Idle behavior: standing still with random head turns side to side.
// Cycles through looking left, right, and center with randomized durations.
// ============================================================================
// `sVar = *ticks; *ticks = sVar - 1; return sVar == 0` - the original's
// value-BEFORE-decrement timer test, which fires a frame later than
// `--ticks == 0` and lets the counter wrap through 0xFFFF.
static int zombie_tick_expired(void)
{
    short ticks = (short)ENTITY->action_ticks_counter;
    ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
    return ticks == 0;
}

static void zombie_idle(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x7F) + 50);
        ENTITY->blend_counter = 3;
        // FALLS THROUGH into sub-state 1 (no break at 0x00434a40).
    case 1:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        // `DEC word [ECX+0xc4]` then compare - this one IS test-after-decrement.
        if (--ENTITY->action_ticks_counter == 0) {
            // MOV word [ECX+0x86],1 - action_behavior AND action_state.
            ENTITY->action_behavior = 1;  // switch to slow walk
            ENTITY->action_state = 0;
            return;
        }
        break;
    case 2:
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0;
        ENTITY->blend_counter = 3;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0xF) + 8);
        // FALLS THROUGH into sub-state 3.
    case 3:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        // Sub-states 3-6 all use the value-BEFORE-decrement form:
        // `CX = *0xc4; TEST CX,CX; LEA EDX,[ECX-1]; MOV word[EAX],DX; JNZ`.
        if (zombie_tick_expired()) {
            ENTITY->action_state = 4;
            ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0xF) + 8);
        }
        ENTITY->angle = ENTITY->angle + 12;  // turn right
        break;
    case 4:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (zombie_tick_expired()) {
            ENTITY->action_state = 5;
            ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0xF) + 4);
        }
        ENTITY->angle = ENTITY->angle - 24;  // turn left (faster)
        break;
    case 5:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (zombie_tick_expired()) {
            ENTITY->action_state = 6;
            ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0xF) + 8);
        }
        ENTITY->angle = ENTITY->angle - 32;  // turn left (fastest)
        break;
    case 6:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (zombie_tick_expired()) {
            ENTITY->action_state = 7;
            ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0xF) + 4);
        }
        ENTITY->angle = ENTITY->angle + 24;  // turn right (faster)
        break;
    case 7:
        // MOV word [EAX+0x86],0 - clears action_state with it.
        ENTITY->action_behavior = 0;  // restart idle loop
        ENTITY->action_state = 0;
        break;
    }
}

// ============================================================================
// zombie_slow_walk @ 0x00434cd0
// Slow walk behavior. Walks toward a random target position, plays footsteps.
// Returns to idle after timer expires (~300 frames).
// ============================================================================
static void zombie_slow_walk(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->move_speed_current = 20;
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 1;
        ENTITY->blend_counter = 3;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x7F) + 300);

        // Waypoint 5000 units ahead. At 0x00434d1a the original copies 8 dwords
        // FROM the address g_deadMoveValue (0x00d1fdd0) HOLDS - it is a pointer
        // variable, and GameStart parks &g_identityMatrixData in it, so the
        // seed is the identity. `&g_deadMoveValue` copied the pointer's own
        // .bss storage plus 28 bytes of whatever globals follow it, and since
        // RotMatrixY computes Ry(r) * m from the EXISTING m[0][*]/m[2][*] the
        // composed rotation was garbage: ApplyMatrixSV then sent the 5000-unit
        // waypoint off in a nonsense direction, which is the wander target
        // zombie_walk2 feeds to entity_pathfind_update. Same form as
        // CharacterNpc.cpp and Yawn.cpp, which already deref correctly.
        memcpy(&g_matrixScratch, (const void*)g_deadMoveValue, 32);
        g_svecScratch.x = 5000;
        g_svecScratch.z = 0;
        g_svecScratch.y = 0;
        RotMatrixY(ENTITY->angle + 8, &g_matrixScratch);
        ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);
        // 16-bit waypoint stores (see the note in Entities.h).
        ENTITY->player_pos_x = (short)((short)ENTITY->scaMatrixData.localMatrix.t[0] + g_svecScratch.x);
        ENTITY->player_pos_z = (short)((short)ENTITY->scaMatrixData.localMatrix.t[2] + g_svecScratch.z);
    }

    // Footstep sounds
    if (ENTITY->timing_control == 1) {
        if (ENTITY->animation_frame_id == 13)
            Snd_em(1);  // walk sfx
        if (ENTITY->animation_frame_id == 0x15)
            Snd_em(2);  // drag leg sfx
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    if (--ENTITY->action_ticks_counter == 0) {
        // MOV word [_ENTITY+0x86],0x200 - action_behavior 0, action_state TWO,
        // so the idle it returns to resumes at its second sub-state.
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 2;
    }

    // 0x00be0dfc is g_animFrameIdSave, not g_tempVar (0x00be0df8).
    g_animFrameIdSave = 7;
    entity_update_wander_turn(*(unsigned short*)&((unsigned char*)&ENTITY->subpixel_pos_x)[2],
                     &ENTITY->dir_control_flags,
                     &((unsigned char*)&ENTITY->subpixel_pos_x)[1],
                     ENTITY->turn_speed, 60);
    if (ENTITY->animation_frame_id > 15) {
        Add_speedXZ(0);
    }
}

// ============================================================================
// zombie_idling @ 0x00434730
// Simple idling behavior — resets to idle if action_behavior isn't idle.
// ============================================================================
static void zombie_idling(void)
{
    if (ENTITY->action_behavior != 0) {
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 0;
    }
    entity_pathfind_update();
}

// ============================================================================
// zombie_walk2 @ 0x00434750
// Walk type 2 — similar to slow walk but with player awareness.
// Transitions to chase if player is close and facing the entity.
// ============================================================================
static void zombie_walk2(void)
{
    int px = (int)*(short*)&ENTITY->player_pos_x;
    int pz = (int)*(short*)&ENTITY->player_pos_z;
    VECTOR walkTarget = { px, 0, pz, 0 };

    // 0x00be0dfc = g_animFrameIdSave gets the mode; 0x00be0df8 = g_tempVar gets
    // "action_behavior on entry". The old code set g_tempVar to 6 and kept the
    // behaviour in a LOCAL, so the three `g_tempVar != 2` gates below compared
    // the wrong thing entirely.
    g_animFrameIdSave = 6;
    g_tempVar = (void*)(unsigned int)ENTITY->action_behavior;

    if (*(unsigned short*)&ENTITY->is_moving == 0) {
        unsigned int result = entity_pathfind_update();
        unsigned int isBlocked = result;
        if ((result & 0xFE) == 0) {
            ENTITY->is_moving &= 0xFE;
            *(unsigned short*)&ENTITY->is_moving |= (unsigned short)(isBlocked & 1);
        }
    }

    if (ENTITY->action_behavior != 0 && ENTITY->action_behavior != 1) {
        g_animFrameIdSave = 7;
        entity_update_wander_turn(*(unsigned short*)((char*)ENTITY + 0x17a),
                         &ENTITY->dir_control_flags,
                         (unsigned char*)((char*)ENTITY + 0x179),
                         ENTITY->turn_speed, 60);
    }

    // Knocked down by damage: `MOV dword [_ENTITY+0x84],0x70101`. The old code
    // sent it to state 3 (DIE) with action_state 1 and never wrote
    // action_behavior - a different outcome entirely.
    if ((ENTITY->action_speed & 0x80) != 0) {
        ENTITY->state              = ZOMBIE_STATE_IDLE;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior    = 7;
        ENTITY->action_state       = 0;
        return;
    }

    // Player in range → start chasing
    if (g_playerDisplacement < 3000) {
        int angle = turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x400);
        if ((short)angle != 0) {
            ENTITY->ignore_player_flag = 1;
            // MOV word [_ENTITY+0x86],3 - clears action_state too.
            ENTITY->action_behavior = 3;
            ENTITY->action_state = 0;
            return;
        }
    }

    // Player close and facing → attack
    if ((ENTITY->collisionFlags & 8) && g_playerDisplacement < 2000) {
        int angle = turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x200);
        if ((short)angle == 0) {
            ENTITY->ignore_player_flag = 1;
            // MOV word [_ENTITY+0x86],6 - action_state cleared, then overwritten
            // with 2 if someone already has the player.
            ENTITY->action_behavior = 6;
            ENTITY->action_state = 0;
            if (g_playerEntity.isBeingAttackedFlag != 0) {
                ENTITY->action_state = 2;
            }
            return;
        }
    }

    // Blocked by player → switch to chase walk
    if ((*(unsigned short*)&ENTITY->is_moving) != 0 && g_playerEntity.isBeingAttackedFlag == 0) {
        if (g_tempVar != (void*)2) {
            ENTITY->action_state = 0;
            ENTITY->blend_counter = 3;
        }
        ENTITY->action_behavior = 2;
        ENTITY->ignore_player_flag = 0;
    }

    // Player being attacked → move toward and eat
    if ((g_playerEntity.isBeingAttackedFlag & 0x80) != 0) {
        // The behaviour switch is GATED on the pathfinder's bit 0 here. The old
        // code dropped the gate, tested action_behavior instead of the saved
        // g_tempVar, and had the comparison inverted (== 2 rather than != 2) -
        // so it reset the sub-state in exactly the wrong half of the cases.
        if ((g_entity_bkp & 1) != 0) {
            if (g_tempVar != (void*)2) {
                ENTITY->action_state = 0;
                ENTITY->blend_counter = 3;
            }
            ENTITY->action_behavior = 2;
            ENTITY->ignore_player_flag = 0;
        }

        if (!(ENTITY->collisionFlags & 8) && g_playerDisplacement < 1200) {
            int angle = turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 0x2C8);
            if ((short)angle == 0) {
                ENTITY->ignore_player_flag = 1;
                // MOV word [_ENTITY+0x86],4
                ENTITY->action_behavior = 4;  // bend down and eat
                ENTITY->action_state = 0;
            }
        }
    }
}

// ============================================================================
// zombie_check_player_distance @ 0x004345b0
// Periodic player distance check for eating/laying zombies.
// Slowly rotates toward the player; if close enough and facing, stands up.
// ============================================================================
void zombie_check_player_distance(void)
{
    ENTITY->action_ticks_counter++;

    // The original steers through g_playerPosScratch (0x00be11b0), not a local -
    // the clobber is observable, RoomCollision.cpp uses the same word.
    g_playerPosScratch.x = (int)ENTITY->player_pos_x;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = (int)ENTITY->player_pos_z;

    int angle = turn_toward_target(&g_playerPosScratch, 4);
    ENTITY->angle = ENTITY->angle + (short)angle;

    // Both arms below wind behavior_flags down by one and snap 7 -> 4. The
    // compare is on the WHOLE byte (`CMP byte [ECX+2],0x7`) and the store is a
    // plain `= 4`, not a masked read-modify-write.
    unsigned int result = entity_pathfind_update();
    if ((result & 1) == 0 || g_playerDisplacement > 8999) {
        if (ENTITY->action_behavior != 0 || g_playerDisplacement < 3000) {
            ENTITY->behavior_flags--;
            if (ENTITY->behavior_flags == ZOMBIE_BEH_7) {
                ENTITY->behavior_flags = 4;
            }
        }
    } else {
        ENTITY->behavior_flags--;
        if (ENTITY->behavior_flags == ZOMBIE_BEH_7) {
            ENTITY->behavior_flags = 4;
        }
    }
}

// zombie_attack_data_tbl @ 0x004bb3e8
// Falling attack data per attacking_direction: {speed_offset, timer, angle_offset}
static const unsigned short zombie_attack_data_tbl[12] = {
    0x0004, 0x0020, 0x0020,     // [0] facing player
    0x0004, 0x0055, 0x000C,     // [1] laying front
    0x0233, 0x0041, 0xFFE0,     // [2] laying back
    0x0000, 0x0A0A, 0x090C      // [3]
};
// ---------------------------------------------------------------------------
// zombie_move_behavior_tbl @ 0x004bb370 - zombie_update_player_distance's
// dispatch table, indexed by behavior_flags & 0x0F. Read from the exe.
// Also serves as update_zombie_action's entries 8-12 (see the note there).
//
// Note there are TWO distinct zombie_slow_walk functions: 0x00434cd0 (reached
// by action_behavior 1) and 0x00434660 (reached by behaviour 2). The port only
// has the first; the second is stubbed below.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// zombie_walk1 @ 0x004343b0
// The waypoint-follow behaviour: run the pathfinder toward the stored waypoint,
// then decide whether to keep wandering, break into a chase, or bend down and
// bite. Reached as zombie_move_behavior_tbl[0] (behavior_flags & 0xF == 0) and
// as zombie_action_tbl[8].
//
// g_collPushDepthZLo (0x00be0df0) is reused here as "action_behavior on entry",
// purely so the two hand-off sites below can tell whether the behaviour is
// actually changing before they reset action_state. Ghidra labels the second
// read g_tempVar, but `CMP dword [0x00be0df0],0x2` at 0x004344f3 shows both
// touch the same scratch word.
// ---------------------------------------------------------------------------
static void zombie_walk1(void)
{
    g_playerPosScratch.x = (int)ENTITY->player_pos_x;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = (int)ENTITY->player_pos_z;
    g_animFrameIdSave = 6;
    g_collPushDepthZLo = (int)ENTITY->action_behavior;

    // WORD test over is_moving + move_max_steps (0x172/0x173).
    if (*(unsigned short*)&ENTITY->is_moving == 0) {
        unsigned char result = (unsigned char)entity_pathfind_update();
        g_entity_bkp = result;
        if ((result & 0xFE) == 0) {
            ENTITY->is_moving &= 0xFE;                        // byte AND
            *(unsigned short*)&ENTITY->is_moving |=           // then WORD OR
                (unsigned short)(g_entity_bkp & 1);
        }
    }

    if ((ENTITY->action_speed & 0x80) != 0) {
        // Knocked down by damage: `MOV dword [EAX+0x84],0x70101`.
        ENTITY->state              = ZOMBIE_STATE_IDLE;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior    = 7;
        ENTITY->action_state       = 0;
        return;
    }

    // Anything other than idle (0) or slow walk (1) gets the wander-turn jitter.
    if (ENTITY->action_behavior != 0 && ENTITY->action_behavior != 1) {
        g_animFrameIdSave = 7;
        entity_update_wander_turn(*(unsigned short*)((char*)ENTITY + 0x17a),
                                  &ENTITY->dir_control_flags,
                                  (unsigned char*)((char*)ENTITY + 0x179),
                                  ENTITY->turn_speed, 60);
    }

    // Close enough and still needing to turn -> break into the chase.
    if (g_playerDisplacement < 3000) {
        int step = turn_toward_target(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 1024);
        if ((short)step != 0) {
            ENTITY->ignore_player_flag = 1;
            // MOV word [EAX+0x86],3 - action_behavior and action_state.
            ENTITY->action_behavior = 3;
            ENTITY->action_state = 0;
            return;
        }
    }

    // Waypoint reached and the player is not already being attacked -> walk.
    if (*(unsigned short*)&ENTITY->is_moving != 0
        && g_playerEntity.isBeingAttackedFlag == 0) {
        if (g_collPushDepthZLo != 2) {
            ENTITY->action_state = 0;
            ENTITY->blend_counter = 3;
        }
        ENTITY->action_behavior = 2;
        ENTITY->ignore_player_flag = 0;
    }

    if ((g_playerEntity.isBeingAttackedFlag & 0x80) != 0) {
        if ((g_entity_bkp & 1) != 0) {
            if (g_collPushDepthZLo != 2) {
                ENTITY->action_state = 0;
                ENTITY->blend_counter = 3;
            }
            ENTITY->action_behavior = 2;
            ENTITY->ignore_player_flag = 0;
        }
        if (g_playerDisplacement < 1200) {
            int step = turn_toward_target(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, 712);
            if ((short)step == 0) {
                ENTITY->ignore_player_flag = 1;
                // MOV word [EAX+0x86],4 - bend down and bite.
                ENTITY->action_behavior = 4;
                ENTITY->action_state = 0;
            }
        }
    }
}
// ---------------------------------------------------------------------------
// zombie_slow_walk_alt @ 0x00434660
// The SECOND zombie_slow_walk - Ghidra gives this name to both 0x00434660 and
// 0x00434cd0. This one is reached from zombie_move_behavior_tbl[2] (that is,
// behavior_flags & 0xF == 2); the other from zombie_action_tbl[1].
//
// Measures how far the waypoint still is, runs the pathfinder, and once the
// waypoint is reached hands over to action_behavior 1.
// ---------------------------------------------------------------------------
static void zombie_slow_walk_alt(void)
{
    // Manhattan distance from the entity to its waypoint, into the collision
    // scratch pair rather than g_playerDisplacement.
    int dz = (int)ENTITY->player_pos_z - ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = (int)ENTITY->player_pos_x - ENTITY->scaMatrixData.localMatrix.t[0];
    int absDz = (dz ^ (dz >> 31)) - (dz >> 31);
    int absDx = (dx ^ (dx >> 31)) - (dx >> 31);
    g_scaled_down_dist = absDz - (dx >> 31) + absDx;
    player_distance_z = (int)ENTITY->action_behavior;

    // WORD test over is_moving + move_max_steps.
    if (*(unsigned short*)&ENTITY->is_moving == 0) {
        unsigned char result = (unsigned char)entity_pathfind_update();
        g_entity_bkp = result;
        if ((result & 0xFE) == 0) {
            ENTITY->is_moving &= 0xFE;                        // byte AND
            *(unsigned short*)&ENTITY->is_moving |=           // then WORD OR
                (unsigned short)(g_entity_bkp & 1);
            return;
        }
        return;
    }

    // Waypoint reached: switch to slow walk, resetting the sub-state only if the
    // behaviour is actually changing.
    if (player_distance_z != 1) {
        ENTITY->action_state = 0;
        ENTITY->blend_counter = 3;
    }
    ENTITY->action_behavior = 1;
    ENTITY->ignore_player_flag = 0;
}

static void* const zombie_move_behavior_tbl[12] = {
    (void*)zombie_walk1,                  // [0]  0x004343b0
    (void*)zombie_check_player_distance,  // [1]  0x004345b0
    (void*)zombie_slow_walk_alt,          // [2]  0x00434660
    (void*)zombie_idling,                 // [3]  0x00434730
    (void*)zombie_walk2,                  // [4]  0x00434750
    (void*)zombie_check_player_distance,  // [5]  0x004345b0
    (void*)zombie_check_player_distance,  // [6]  0x004345b0
    (void*)zombie_check_player_distance,  // [7]  0x004345b0
    (void*)zombie_check_player_distance,  // [8]  0x004345b0
    NULL,                                 // [9]
    (void*)zombie_idling,                 // [10] 0x00434730
    NULL                                  // [11]
};
// ============================================================================
// zombie_update_player_distance @ 0x00434330
// Computes Manhattan distance to player and dispatches via the zombie move
// behavior table indexed by behavior_flags & 0x0F. If is_moving != 0, also
// calls zone_path_find to update the distance-based pathfinding.
//
// Named entity_update_player_distance in the first pass; the switch below is the
// zombie move behaviour table (0x004bb280), so it is zombie logic and stays here
// rather than moving to EntityCommon.cpp. Renamed in Ghidra to match.
// ============================================================================
void zombie_update_player_distance(void)
{
    if (*(unsigned short*)&ENTITY->is_moving != 0) {
        zone_path_find(
            g_playerEntityPointer.scaMatrixData.localMatrix.t[0],
            g_playerEntityPointer.scaMatrixData.localMatrix.t[2],
            (int*)&ENTITY->player_pos_x,
            (int*)&ENTITY->player_pos_z);
    }

    int dz = (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[2]
           - *(int*)&ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[0]
           - *(int*)&ENTITY->scaMatrixData.localMatrix.t[0];
    int absDz = (dz ^ (dz >> 31)) - (dz >> 31);
    int absDx = (dx ^ (dx >> 31)) - (dx >> 31);
    g_playerDisplacement = absDz - (dx >> 31) + absDx;

    // 0x004343a2: `JMP dword ptr [ECX*4 + 0x4bb370]` on behavior_flags & 0x0F.
    // All four arms the old switch had were mapped to the wrong function.
    unsigned char behavior = ENTITY->behavior_flags & 0x0F;
    if (behavior < 12 && zombie_move_behavior_tbl[behavior] != NULL) {
        ((void(*)())zombie_move_behavior_tbl[behavior])();
    }
}
// ---------------------------------------------------------------------------
// The three behaviour bodies that used to be inlined as update_zombie_action
// cases 4, 5 and 6. They are real, separately addressed functions - Ghidra
// inlines them into its update_zombie_action listing because the dispatcher
// reaches them by tail JMP, which is what made the old pass write them out as
// switch arms.
// ---------------------------------------------------------------------------

// benddown_and_eat @ 0x00435e00 - bend down over a corpse and feed
static void benddown_and_eat(void)
{
    // ---- LAYING-DOWN APPROACH ----
    // Only sub-states 2 and 4 fall into the next one in the same frame; 0, 1
    // and 3 return. `st` is a snapshot so that state 3 incrementing the field
    // cannot make sub-state 4 run in the same frame, and so 0 setting the
    // field to 3 cannot cascade into 3/4/5.
    {
    unsigned char st = ENTITY->action_state;
    if (st == 0) {
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 0x0B;
    }
    if (st == 1) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (--ENTITY->action_ticks_counter == 0) {
            ENTITY->action_state++;
        }
    }
    if (st == 2) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->action_state++;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 0x0B;
        st = 3;                 // fall into sub-state 3
    }
    if (st == 3) {
        int result = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if ((char)result != 0) {
            ENTITY->action_state++;
        }
        int turnStep = turn_toward_target(
            (VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 0x20);
        ENTITY->angle = ENTITY->angle + (short)turnStep;
        return;                 // the original returns out of sub-state 3
    }
    if (st == 4) {
        ENTITY->action_state++;
        ENTITY->animationId++;
        ENTITY->timing_control = 0;
        ENTITY->action_ticks_counter = 120;
        st = 5;                 // fall into sub-state 5
    }
    if (st == 5) {
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if ((ENTITY->animation_frame_id & 7) == 0) {
            VECTOR eatPos = { 800, -300, 0, 0 };
            Effect_CreateBillboard(0, 0, 0, &ENTITY->scaMatrixData.localMatrix, &eatPos, 0);
            Snd_em(3);
        }
        if (ENTITY->animation_frame_id == 0x0E) {
            _ENTITY_SAVE = &g_playerEntityPointer;
            JointApplyColorTint(g_playerEntityPointer.jointsStructs, 0x30, 0x80820, &DAT_00606060);
            VECTOR pos = { 100, -700, 0, 0 };
            Effect_CreateBillboard(0, 0, 0, &g_playerEntityPointer.jointsStructs->world, &pos, 0);
            JointApplyColorTint(g_playerEntityPointer.jointsStructs + 2, 0x30, 0x80820, &DAT_00606060);
            Effect_CreateBillboard(0, 0, 0, &g_playerEntityPointer.jointsStructs[2].world, &pos, 0);
        }
        // `sVar1 = *0xc4; *0xc4 = sVar1 - 1; if (sVar1 == 0)` - the test is
        // on the value BEFORE the decrement, so it fires one frame later
        // than `--counter == 0` and the counter wraps through 0xFFFF.
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        if (ticks == 0) {
            ENTITY->action_state++;
            ENTITY->animationId++;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 3;
        }
    }
    if (st == 6) {
        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            // MOV dword [_ENTITY+0x84],1 - state=1 and the other three zero.
            ENTITY->state = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 0;
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
        }
    }
    }
}

// turn_towards_player @ 0x00436120 - the lunge: drive forward along speed while turning in
static void turn_towards_player(void)
{
    // ---- FALLING ATTACK ----
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 3;
        ENTITY->move_speed_current = 45;
        // Byte offset = direction * 6 (three ushorts per row: speed, timer,
        // angle). The old `&tbl + direction*6` was pointer arithmetic on the
        // ARRAY type - direction*144 bytes - so the timer and angle reads were
        // always out of bounds.
        Add_speedXZ(zombie_attack_data_tbl[ENTITY->attacking_direction * 3]);
        ENTITY->action_ticks_counter =
            zombie_attack_data_tbl[ENTITY->attacking_direction * 3 + 1];
    }
    if (ENTITY->timing_control == 1) {
        if (ENTITY->animation_frame_id == 8) Snd_em(1);
        if (ENTITY->animation_frame_id == 0x1D) Snd_em(1);
    }
    ENTITY->angle = ENTITY->angle
        - (short)zombie_attack_data_tbl[ENTITY->attacking_direction * 3 + 2];
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    {
        // Value-before-decrement test again (0x004355xx).
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        if (ticks == 0) {
            ENTITY->ignore_player_flag = 1;
            // MOV word [_ENTITY+0x86],4 - action_behavior AND action_state.
            ENTITY->action_behavior = 4;
            ENTITY->action_state = 0;
        }
    }
    // These three were missing entirely, so the falling attack never
    // travelled: the original adds the whole speed SVECTOR at 0x78 into
    // localMatrix.t every frame of it.
    ENTITY->scaMatrixData.localMatrix.t[0] += (int)ENTITY->speed.x;
    ENTITY->scaMatrixData.localMatrix.t[1] += (int)ENTITY->speed.y;
    ENTITY->scaMatrixData.localMatrix.t[2] += (int)ENTITY->speed.z;
}

// zombie_vomiting @ 0x00436520 - vomit, then recover to standing
static void zombie_vomiting(void)
{
    // ---- VOMITING/EATING ----
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 5;
        {
            VECTOR vPos = { 500, -2500, 0, 0 };
            // ENTITY + 0x20 = scaMatrixData.localMatrix. Passing
            // &scaMatrixData handed the effect renderer a MATRIX pointer 4
            // bytes early, so the 0x20-byte memcpy in
            // EffectActor_UpdateAndRender built the slot transform from
            // field_00 + localMatrix.m[0..6] and read the translation out of
            // localMatrix.m[7]/m[8]/pad. The vomit blob spawned at a nonsense
            // world position: is_entity_in_switch_zone culled the sprite (no
            // FX) and effect_projectile_hit_check's 600-unit splash test never
            // came near the player (no damage).
            Effect_CreateBillboard(0x20, 0, 0, &ENTITY->scaMatrixData.localMatrix, &vPos, 0);
        }
        Snd_em(7);
        // FALLS THROUGH into sub-state 1 in the original.
    case 1:
        ENTITY->action_state += (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        return;                 // sub-state 1 does NOT fall into 2
    case 2:
        ENTITY->action_state = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 0;
        ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x1F) + 20);
        ENTITY->blend_counter = 3;
        // FALLS THROUGH into sub-state 3 in the original.
    case 3:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (--ENTITY->action_ticks_counter == 0) {
            ENTITY->ignore_player_flag = 0;
            // MOV word [_ENTITY+0x86],0 - clears action_state too.
            ENTITY->action_behavior = 0;
            ENTITY->action_state = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// update_zombie_action @ 0x004342f0
//
// FOURTEEN BYTES of real code. The whole function is:
//     MOV EAX,[_ENTITY] ; XOR ECX,ECX ; MOV CL,[EAX+0x86]
//     JMP dword ptr [ECX*4 + 0x4bb350]
// a tail-jump dispatcher on action_behavior, nothing else. Ghidra shows it as
// a 500-line switch because it inlines every jump target; the old pass took
// that listing at face value and wrote a hand-rolled switch whose arms 2 and 3
// went to the wrong functions entirely.
//
// The table at 0x004bb350 holds only EIGHT entries. Indices 8-12 run off the
// end into zombie_update_player_distance_tbl at 0x004bb370, which is
// 0x004bb350 + 8*4 - the same overlapping-tables trick as
// zombie_states_table / zombie_behavior_tbl. Both views are spelled out below.
// ---------------------------------------------------------------------------
static void* const zombie_action_tbl[8] = {
    (void*)zombie_idle,           // [0] 0x004349d0
    (void*)zombie_slow_walk,      // [1] 0x00434cd0
    (void*)zombie_chase_walk,     // [2] 0x00434eb0 - NOT zombie_chase_player
    (void*)fast_player_facing,    // [3] 0x00435c60 - NOT a no-op
    (void*)benddown_and_eat,      // [4] 0x00435e00
    (void*)turn_towards_player,   // [5] 0x00436120
    (void*)zombie_vomiting,       // [6] 0x00436520
    (void*)zombie_falldown        // [7] 0x004368a0
};

void update_zombie_action(void)
{
    unsigned char behavior = ENTITY->action_behavior;
    if (behavior < 8) {
        ((void(*)())zombie_action_tbl[behavior])();
        return;
    }
    // 8-12 alias zombie_update_player_distance_tbl[0-4]. The original just
    // indexes past the end of its own table; spelled out rather than hidden.
    if (behavior <= 12) {
        ((void(*)())zombie_move_behavior_tbl[behavior - 8])();
    }
}
// ============================================================================
// zombie_pushback_action @ 0x00434310
// Dispatcher for pushback behaviors: calls zombie_pushback_idle if
// action_behavior == 0 (simple idle stagger), otherwise calls
// zombie_pushback_stagger (active backward walk with stagger).
// ============================================================================
void zombie_pushback_action(void)
{
    if (ENTITY->action_behavior == 0) {
        zombie_pushback_idle();
    } else {
        zombie_pushback_stagger();
    }
}
// ============================================================================
// magnum_shot_pushback @ 0x00436b70
// Magnum/explosive headshot death — the zombie is violently pushed back.
// Plays animation 3 with speed 45, orienting away from the player.
// On completion, transitions to dead state with the die animation.
// ============================================================================
void magnum_shot_pushback(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->behavior_step |= 0x01;
        ENTITY->move_speed_current = 45;
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 3;
        ENTITY->blend_counter = 3;
        return;
    }

    if (ENTITY->action_state == 1) {
        entity_rotate_toward_target((VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t, 32);

        if (ENTITY->timing_control == 1) {
            if (ENTITY->animation_frame_id == 8) Snd_em(1);
            if (ENTITY->animation_frame_id == 29) Snd_em(1);
        }

        if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
            *(unsigned int*)(&ENTITY->state) = 0x103;  // die state: state=3, ignore=1, behavior=0, sub=3
        }
    }

    Add_speedXZ(0);
}

// Zombie damage behavior stubs (dispatched from zombie_damaged)
// zombie_recovery_timer_tbl @ 0x004bb400
// Random recovery time multipliers (×30 frames) before zombie stands up after falldown.
static const unsigned char zombie_recovery_timer_tbl[16] = {
    2, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 9, 9, 9
};

// ============================================================================
// zombie_falldown @ 0x004368a0
// Falls down to the ground after a strong hit. Plays fall animation, waits
// a random time on the ground, then attempts to stand up. If stagger_timer
// is 0, gives up and enters damaged state; otherwise restarts cycle.
// ============================================================================
void zombie_falldown(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->animationId = 8;
        ENTITY->blend_counter = 7;
        ENTITY->move_speed_current = 20;
        Snd_em(5);
        // dword at 0x38 is localMatrix.t[1], not m[1][0] (0x26, rotation).
        ENTITY->scaMatrixData.localMatrix.t[1] = 1;  // set laying-down height
        break;

    case 1:
        ENTITY->behavior_step &= ~0x04;
        ENTITY->hit_state = 1;
        ENTITY->action_speed |= 0x80;

        if (ENTITY->animation_frame_id < 4) {
            // Still falling forward: keep the "down" bit (0x80) clear, and set
            // the falling bit + clear hit_state so a hit mid-fall restores the
            // previous state instead of re-reacting (0x0043694d-0x00436965).
            // The old pass had the branch inverted and dropped both extras.
            ENTITY->action_speed &= ~0x80;
            ENTITY->behavior_step |= 0x04;
            ENTITY->hit_state = 0;
        }

        if (ENTITY->timing_control == 1) {
            if (ENTITY->animation_frame_id == 8)
                Snd_em(1);  // walk SFX
            if (ENTITY->animation_frame_id == 0x1A)
                Snd_em(0);  // thud SFX
        }

        if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
            ENTITY->hit_state = 0;
            ENTITY->action_ticks_counter =
                (unsigned short)zombie_recovery_timer_tbl[g_RandSeed & 0x0F] * 30;
            ENTITY->action_state = 2;
        }
        Add_speedXZ(0);
        break;

    case 2:
        if (--ENTITY->action_ticks_counter == 0) {
            ENTITY->action_state = 3;
            ENTITY->blend_counter = 0;
            ENTITY->hit_state = 1;
            Snd_em(5);  // get-up SFX
            return;
        }
        break;

    case 3:
        ENTITY->behavior_step &= ~0x04;
        ENTITY->hit_state = 1;
        ENTITY->action_speed |= 0x80;

        if (ENTITY->animation_frame_id > 0x1A) {
            // Almost stood up (0x00436a96-0x00436aae): same flag trio as case 1.
            ENTITY->action_speed &= ~0x7F;
            ENTITY->behavior_step |= 0x04;
            ENTITY->hit_state = 0;
        }

        if ((char)Joint_move(1, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
            if (ENTITY->stagger_timer == 0) {
                // Poise spent: replenish from the stagger table and clear the
                // "down" bit so damage can trigger again (0x00436af8). The old
                // pass had this branch swapped with the else and never
                // replenished the timer.
                ENTITY->stagger_timer = zombie_stagger_tbl[g_RandSeed & 0x1F];
                ENTITY->action_speed &= ~0x80;
            } else {
                ENTITY->action_speed = 0;
            }
            ENTITY->hit_state = 0;
            // MOV dword [EAX+0x84],0x30101 - state=1 IDLE, ignore=1,
            // action_behavior=3, action_state=0. The old pass sent it to
            // state 3 (DIE), so every successful get-up played the death
            // sequence instead of returning to behaviour 3.
            ENTITY->state = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior = 3;
            ENTITY->action_state = 0;
            ENTITY->behavior_step &= ~0x04;
            ENTITY->scaMatrixData.localMatrix.t[1] = 0;  // clear laying height
        }
        Add_speedXZ(0x800);
        break;
    }
}

// ============================================================================
// short_push_back @ 0x00436c80
// Short hit reaction — the zombie staggers backward. Plays either stagger
// animation (5) or vomit stagger (4). Sets up joint damage visual effects
// (limb severing with blood billboards), plays moan SFX, and transitions
// back to damaged state when the animation finishes.
// ============================================================================
void short_push_back(void)
{
    if ((ENTITY->behavior_flags & ZOMBIE_FLAG_VOMITING) == 0)
        ENTITY->animationId = 5 - (ENTITY->action_behavior == 0);
    ENTITY->move_speed_current = 15;

    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->move_speed_current = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;

        if ((ENTITY->behavior_flags & ZOMBIE_FLAG_VOMITING) != 0) {
            // The original steers this through g_playerPosScratch (0x00be11b0),
            // not a local: the clobber is shared with the weapon-hit path.
            g_playerPosScratch.x = 100;
            g_playerPosScratch.y = -2620;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 0, 0, (void*)&ENTITY->scaMatrixData.localMatrix,
                                   &g_playerPosScratch, 0);
        }

        int jointPtr = (int)ENTITY->jointsStructs;
        if ((ENTITY->hit_state & 7) == 3) {
            unsigned char* jointFlag = (unsigned char*)(jointPtr + 0x1F0);
            if ((*jointFlag & 4) == 0) {
                unsigned char randBit = (0x40 >> ((unsigned char)g_RandSeed & 7)) & 1;
                if (randBit != 0) {
                    *jointFlag |= 12;  // disable and flag as severed
                    g_playerPosScratch.x = 0;
                    g_playerPosScratch.y = 0;
                    g_playerPosScratch.z = 0;
                    Effect_CreateBillboard(0, 0, 0, (void*)(jointPtr + 0x234),
                                           &g_playerPosScratch, 0);
                    Effect_CreateBillboard(0, 0, 0, (void*)0, (void*)(jointPtr + 0x248), 0);
                    *(unsigned char*)(jointPtr + 0x26C) |= 0x10;
                    JointApplyColorTint((JointStruct*)(jointPtr + 0x1F0), 0x30, 0x80820, &DAT_00606060);
                    JointApplyColorTint((JointStruct*)(jointPtr + 0x174), 0x30, 0x80820, &DAT_00606060);
                }
            }
        }

        if (ENTITY->internal_timer == 0) {
            Snd_em(9);  // moan SFX
            ENTITY->internal_timer = 150;
        }
    }

    char done = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 1024);
    if (done != 0) {
        ENTITY->action_state++;

        // 0x00436e61: the SCD-controlled zombie (0x40) keeps the incremented
        // action_state and stays in state 8 - that is how zombie_scd_vomiting
        // detects completion. Everyone else gets the dword store below.
        if ((ENTITY->behavior_flags & ZOMBIE_FLAG_VOMITING) == 0) {
            // Plain store, not a masked clear (`MOV byte [EAX],0x0`).
            if ((ENTITY->behavior_flags & 0x0F) == ZOMBIE_BEH_5)
                ENTITY->behavior_flags = 0;
            // `MOV dword ptr [EAX+0x84],0x30101` at 0x00436e76 - state = 1
            // (IDLE), ignore = 1, action_behavior = 3, action_state = 0.
            // The old pass wrote state = 3 (DIE) and left action_behavior alone,
            // so EVERY hit reaction ended in the death sequence: one handgun
            // shot killed the zombie, and because action_state was still 2 the
            // corpse skipped straight to zombie_dead_animation's hold state -
            // it never played the fall, which is the "dies and freezes" bug.
            ENTITY->state              = ZOMBIE_STATE_IDLE;
            ENTITY->ignore_player_flag = 1;
            ENTITY->action_behavior    = 3;
            ENTITY->action_state       = 0;
        }

        // 16-bit waypoint stores (`MOV word ptr [ECX+0x166],AX`), not bytes.
        ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        ENTITY->hit_state = 0;
    }

    zombie_check_special_weapon();

    if ((ENTITY->hit_state & 1) != 0) {
        ENTITY->action_counter++;
        if (ENTITY->action_counter == 2) {
            ENTITY->move_speed = 20;
            ENTITY->turn_speed = 14;
        }
    }

    Add_speedXZ(0);
}

// ============================================================================
// push_and_stagger @ 0x00436f00
// Strong pushback — staggers backward harder. Uses animation 6 (front push)
// or 7 (side push). Plays moan SFX and transitions to damaged state on
// completion.
// ============================================================================
void push_and_stagger(void)
{
    // 0x00436f06-0x00436f2b: the side push (behaviour 2) also selects the 0x800
    // speed argument for the Add_speedXZ at the end; the front push passes 0.
    // The old pass hard-coded 0x800 on both, so a front shove slid the zombie.
    unsigned short speedArg;
    if (ENTITY->action_behavior == 2) {
        ENTITY->animationId = 7;
        speedArg = 0x800;
    } else {
        ENTITY->animationId = 6;
        speedArg = 0;
    }
    ENTITY->move_speed_current = 15;

    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 6;

        if (ENTITY->internal_timer == 0) {
            Snd_em(9);  // moan SFX
            ENTITY->internal_timer = 150;
        }
    }

    // 0x00436fc0-0x0043703a. Both arms converge on the same tail; only the
    // hit_state clear is conditional, and there is no early return.
    char done = Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 1024);
    if (done != 0) {
        // Plain store (`MOV byte [EAX],0x0`), not a masked clear.
        if ((ENTITY->behavior_flags & 0x0F) == ZOMBIE_BEH_5)
            ENTITY->behavior_flags = 0;
        // `MOV dword ptr [EAX+0x84],0x30101` at 0x00436fdf - state = 1 (IDLE),
        // ignore = 1, action_behavior = 3, action_state = 0. The old pass wrote
        // state = 3 (DIE), so a strong shove killed the zombie outright.
        ENTITY->state              = ZOMBIE_STATE_IDLE;
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior    = 3;
        ENTITY->action_state       = 0;
        // 16-bit waypoint stores, not bytes.
        ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        ENTITY->hit_state = 0;
    } else {
        if (ENTITY->animation_frame_id < 5)
            ENTITY->move_speed_current = (unsigned short)(ENTITY->move_speed_current + 90);
        // `CMP byte [EAX+0xbe],0x14; JNZ` - equality, not `< 0x14`.
        if (ENTITY->animation_frame_id == 0x14)
            ENTITY->hit_state = 0;
    }

    zombie_check_special_weapon();
    Add_speedXZ(speedArg);
}

// ============================================================================
// zombie_pushback_idle @ 0x00435d90
// Idle stagger animation (animation 9). Just plays the animation with
// no movement — used when the zombie is pushed back while idling.
// ============================================================================
void zombie_pushback_idle(void)
{
    ENTITY->animationId = 9;

    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// zombie_pushback_stagger @ 0x004362a0
// Active stagger backward walk. 4-state FSM: init (animation 14), wait for
// frame 5, walk backward with body-part physics and SFX, then decelerate
// with random timer.
//
// The turn accumulator at +0x170 is a WORD here, not a byte: the original
// stores it with `MOV word ptr [ECX + 0x170],AX` (0x0043641c), negates it with
// a 16-bit `NEG CX` (0x0043643b) and adds it to the angle with
// `ADD word ptr [ECX + 0x74],AX` (0x00436479). Entities.h has to declare
// angle_turn_delta as a byte because CharacterNpc genuinely uses that slot as
// one, so this path reaches it at full width instead - truncating it stored
// turn_toward_target's -8 as 0xF8 and turned +248 (~22 deg per frame) the wrong
// way every time the crawl needed to steer counter-clockwise.
// ============================================================================
#define ZOMBIE_TURN_DELTA(e) (*(short*)((char*)(e) + 0x170))

void zombie_pushback_stagger(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->action_state = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 3;
        ENTITY->animationId = 0x0E;
        break;

    case 1:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        if (ENTITY->animation_frame_id == 5) {
            ENTITY->action_state = 2;
            ENTITY->blend_counter = 3;
            ENTITY->move_speed_current = 20;
            ENTITY->action_ticks_counter = 1;
        }
        break;

    case 2:
        {
        if ((ENTITY->animation_frame_id == 7 || ENTITY->animation_frame_id == 0x18)
            && ENTITY->timing_control == 1) {
            Snd_em(2);  // walk-drag SFX
        }

        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        zombie_body_part_physics(1);

        int jointPtr = (int)ENTITY->jointsStructs;
        if ((*(unsigned char*)(jointPtr + 0x1F0) & 4) != 0
            && ENTITY->animation_frame_id > 0x11) {
            return;
        }

        {
            VECTOR target;
            target.x = (int)*(short*)&ENTITY->player_pos_x;
            target.z = (int)*(short*)&ENTITY->player_pos_z;
            target.y = 0;
            int turnStep = turn_toward_target(&target, 8);
            ZOMBIE_TURN_DELTA(ENTITY) = (short)turnStep;

            if ((ENTITY->dir_control_flags & 0x80) != 0) {
                ZOMBIE_TURN_DELTA(ENTITY) = -ZOMBIE_TURN_DELTA(ENTITY);
                ENTITY->action_state = 3;
                ENTITY->action_ticks_counter = (unsigned short)((g_RandSeed & 0x1F) + 30);
            }
        }

        ENTITY->angle = ENTITY->angle + ZOMBIE_TURN_DELTA(ENTITY);
        Add_speedXZ(0);
        }
        break;
    case 3:
        Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        zombie_body_part_physics(1);
        ENTITY->angle = ENTITY->angle + ZOMBIE_TURN_DELTA(ENTITY);
        Add_speedXZ(0);

        if (--ENTITY->action_ticks_counter == 0)
            ENTITY->action_state = 3;  // loop
        break;
    }

    if ((ENTITY->collisionFlags & 8) != 0) {
        ENTITY->behavior_flags++;
    }
}

// ============================================================================
// zombie_check_special_weapon @ 0x0043d8a0
// If special weapon equipped (>110) and anim frame % 5 == 0, clears hit_state
// to prevent stagger effect during special weapon damage.
// ============================================================================
void zombie_check_special_weapon(void)
{
    if (g_playerEntityPointer.equippedWeaponId > 0x6E
        && ENTITY->animation_frame_id % 5 == 0) {
        ENTITY->hit_state = 0;
    }
}

// ============================================================================
// zombie_body_part_physics @ 0x00437c30
// Computes movement speed from body-part joint physics. Rotates entity
// matrix, applies to body part joints with reverse scaling, extracts
// displacement, computes its magnitude via SquareRoot0, and stores it
// in move_speed_current.
// ============================================================================
void zombie_body_part_physics(unsigned char param)
{
    int jointPtr = (int)ENTITY->jointsStructs;

    RotMatrix((SVECTOR*)((char*)ENTITY + 0x72), (MATRIX*)((char*)ENTITY + 0x20));
    ApplyLVAndMul0Matrix((void*)((char*)ENTITY + 0x20), (void*)(jointPtr + 0x24), &g_matrixScratch);
    ApplyLVAndMulMatrix(&g_matrixScratch, (MATRIX*)(jointPtr + 0xA0));

    unsigned char count = 3;
    int base = jointPtr + (unsigned int)param * 0x174 + 0x26C;
    do {
        ApplyLVAndMulMatrix(&g_matrixScratch, (MATRIX*)((unsigned int)count * -0x7C + base + 0xA0));
        count--;
    } while (count != 0);

    g_matrixScratch.t[0] = g_matrixScratch.t[0] - *(int*)(base + 0x58);
    g_matrixScratch.t[1] = 0;
    g_matrixScratch.t[2] = g_matrixScratch.t[2] - *(int*)(base + 0x60);

    VECTOR result;
    FUN_0040a380((VECTOR*)g_matrixScratch.t, &result);

    unsigned int mag = SquareRoot0(result.z + result.x);
    ENTITY->move_speed_current = (unsigned short)mag;
}

// ============================================================================
// Where the rest of this file's dependencies live
//
//   Entity-generic helpers + their remaining stubs  EntityCommon.cpp
//   check_room_collision           @ 0x0047d310     RoomCollision.cpp
//   entity_add_fade_sprite         @ 0x00456810     FadeSprite.cpp
//   BillboardSetColor              @ 0x00456710     PlayerAnimations.cpp
//
// entity_add_fade_sprite is worth a note: the version that used to live here
// built the matrices and then dropped the sprite on the floor - the queue write
// was the missing half.
// ============================================================================
