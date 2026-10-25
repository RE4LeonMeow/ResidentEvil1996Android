#pragma once
#include "EntityCommon.h"

// ============================================================================
// Zombie enemy type - Resident Evil 1
//
// Enemy IDs: 0 (standard zombie), 1 (naked zombie)
//
// Entity-generic helpers (angle/LOS maths, SCA collision, wander/pathfind,
// joint effects) and the entity type dispatch table live in EntityCommon.h -
// they are shared with every other monster and with CharacterNpc.cpp.
//
// Original addresses from Ghidra:
//   zombie_update          @ 0x004338c0
//   zombie_init            @ 0x00433440
//   zombie_state_check     @ 0x00433ae0
//   zombie_states_table    @ 0x004bb2c8
//   zombie_behavior_tbl    @ 0x004bb2f0  (second view of zombie_states_table)
//   zombie_health_tbl      @ 0x004bb288
//   zombie_anim_id_tbl     @ 0x004bb298  (initial animationId per behavior)
//   zombie_stagger_tbl     @ 0x004bb2a8  (poise budget)
// ============================================================================

// Enemy ID for zombies
#define ENEMY_ID_ZOMBIE        0
#define ENEMY_ID_NAKED_ZOMBIE  1

// Zombie states (entity->state at 0x84)
enum ZombieState {
    ZOMBIE_STATE_INIT           = 0,  // zombie_init - one-time init
    ZOMBIE_STATE_IDLE           = 1,  // zombie_state_check - main idle/behavior dispatch
    ZOMBIE_STATE_DAMAGED        = 2,  // zombie_damaged - hit reaction
    ZOMBIE_STATE_DIE            = 3,  // zombie_die - death sequence
    ZOMBIE_STATE_DEAD_ANIM      = 4,  // no_action @ 0x004342d0 (bare RET); the
                                      // corpse tail is zombie_dead_animation @ 0x00437740
    ZOMBIE_STATE_ATTACK         = 5,  // zombie_attack - attacking player
    ZOMBIE_STATE_6              = 6,  // (unused - NULL in table)
    ZOMBIE_STATE_7              = 7,  // (unused - NULL in table)
    ZOMBIE_STATE_ACTION_UPDATE  = 8,  // zombie_action_update - action dispatch
    ZOMBIE_STATE_9              = 9,  // (unused - NULL in table)
    ZOMBIE_STATE_CHASE          = 10, // zombie_chase_player - chasing
    ZOMBIE_STATE_CHASE2         = 11, // zombie_chase_player - chasing (mirror)
    ZOMBIE_STATE_PUSHED_BACK    = 12, // zombie_pushed_back - knockback
    ZOMBIE_STATE_PUSHED_BACK2   = 13, // zombie_pushed_back - knockback (mirror)
    ZOMBIE_STATE_RANDOM_CHASE   = 14, // zombie_random_chase - staggered chase
    ZOMBIE_STATE_EATING         = 15, // zombie_eating - eating animation
    ZOMBIE_STATE_COUNT          = 16
};

// Zombie behavior types (entity->behavior_flags & 0x0F, used to index behavior table)
enum ZombieBehavior {
    ZOMBIE_BEH_NORMAL           = 0,
    ZOMBIE_BEH_1                = 1,
    ZOMBIE_BEH_2                = 2,  // laying down
    ZOMBIE_BEH_3                = 3,
    ZOMBIE_BEH_4                = 4,
    ZOMBIE_BEH_5                = 5,  // laying down eating
    ZOMBIE_BEH_6                = 6,  // dead (laying down, doing nothing)
    ZOMBIE_BEH_7                = 7,  // laying down eating
    ZOMBIE_BEH_8                = 8,
    ZOMBIE_BEH_9                = 9,
    ZOMBIE_BEH_10               = 10, // lying on floor
    // Director's Cut only - the one entry its zombie_states_table adds, which
    // lands on behaviour 11 (the port's spare states[21]). dc_standup_lunge.
    ZOMBIE_BEH_DC_STANDUP       = 11,
    // The DC's three spawn markers. zombie_init rewrites them in place into
    // behaviour 0 or 1 and sets Entity::dc_double_step, so nothing ever
    // dispatches on these values. (docs/PSX_DC_ENEMY_AI.md)
    ZOMBIE_BEH_DC_FAST_0        = 12, // -> behaviour 0, double step
    ZOMBIE_BEH_DC_FAST_1        = 13, // -> behaviour 1, double step
    ZOMBIE_BEH_DC_FAST_TOUGH    = 14, // -> behaviour 0, double step, +100 HP
};

// Behavior flags (entity->behavior_flags bits)
#define ZOMBIE_FLAG_LAYING_DOWN      0x02  // bit 1: zombie is on the ground
#define ZOMBIE_FLAG_INACTIVE         0x04  // bit 2: disabled (dead on floor)
#define ZOMBIE_FLAG_VOMITING         0x40  // bit 6: vomiting state
#define ZOMBIE_FLAG_SCD_CONTROLLED   0x80  // bit 7: SCD event is controlling zombie

// g_pZombieScaInfo @ 0x004bb280 - per-id SCA record (collision radius at +0x0A);
// zombie_init stores one of these two into ENTITY->Sca_info.
// [2] is Director's Cut only (behaviour 11); the PC build's table has two.
extern const short* const g_pZombieScaInfo[3];

// zombie_states_table @ 0x004bb2c8 - indexed by entity->state. 22 entries, not
// 16: entries 16-21 belong to the behaviour view below, which shares the block.
extern void* zombie_states_table[22];

// zombie_behavior_tbl @ 0x004bb2f0 - second view of zombie_states_table, based
// 10 entries in, indexed by behavior_flags & 0x0F. Only 0-11 are valid.
extern void** const zombie_behavior_tbl;

// 0x004bb288 - base health per random index (zombie_init subtracts rand % 22)
extern const unsigned char zombie_health_tbl[16];

// 0x004bb298 - initial animationId per behavior type
extern const unsigned char zombie_anim_id_tbl[16];

// 0x004bb2a8 - stagger_timer (poise) budget per random index
extern const unsigned char zombie_stagger_tbl[32];

// Zombie state handler functions
extern void zombie_init(void);            // 0x00433440
extern void zombie_state_check(void);     // 0x00433ae0
extern void zombie_damaged(void);         // 0x00433db0
extern void zombie_die(void);             // 0x004340e0
extern void zombie_dead_animation(void);  // 0x004342d0
extern void zombie_attack(void);          // 0x004342e0
extern void zombie_action_update(void);   // 0x00454ab0
extern void zombie_chase_player(void);    // 0x00433bb0
extern void zombie_pushed_back(void);     // 0x00433c60
extern void zombie_random_chase(void);    // 0x00433cf0
extern void zombie_eating(void);          // 0x00436690

// Zombie helper functions
extern void zombie_check_player_distance(void);  // 0x004345b0
