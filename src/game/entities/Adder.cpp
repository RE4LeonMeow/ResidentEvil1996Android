// Adder.cpp - Adder (the regular-size venomous snakes, entity type 10,
//             enemy/em100a.emd)
//
// Original PC addresses:
//   adder_update                 0x004727f0   per-frame entry, dispatch table [10]
//   adder_sca_info               0x004c3620   SCA collision record
//   adder_sca_info_alt           0x004c3630   second record, unreachable (see below)
//   g_pAdderScaInfoTable         0x004c3640   pointer table -> the two records
//   adder_state_table            0x004c3648   FOUR entries (a real CALL table)
//   adder_behavior_jumptable     0x004c3658   SEVEN entries (a compiler switch)
//   adder_decide_table           0x004c3678   FOUR entries (a real CALL table)
//
// The id is 10 and the model is em100a.emd: g_emdPathTable index (id + 4) = 14
// is "enemy/em100a.emd", the adder. Id 13/18 are the Yawn (em100d/em1012), the
// giant snake, which is a completely separate boss in Yawn.cpp.
//
// ---------------------------------------------------------------------------
// THREE tables in one 0x38-byte block at 0x004c3648
// ---------------------------------------------------------------------------
// This is the trap in this entity. A naive read of 0x004c3648 sees eleven
// function pointers followed by a NULL and calls it an eleven-state table. It
// is not. Counting the readers instead of the NULLs:
//
//   0x0047280a  CALL dword ptr [ECX*4 + 0x4c3648]   ECX = Entity+0x84 state
//   0x00472ab6  JMP  dword ptr [ECX*4 + 0x4c3658]   ECX = Entity+0x86 behaviour
//   0x00472b18  CALL dword ptr [ECX*4 + 0x4c3678]   ECX = Entity+0x02 spawn kind
//
// so the state table is only FOUR entries long (0x004c3648..0x004c3654) and the
// "states 4-10" are really the seven case bodies of adder_behavior_dispatch's
// switch, whose jumptable starts at 0x004c3658. The NULL at 0x004c3674 is the
// pad between that jumptable and the third table. There is no bounds check
// before the JMP - the compiler proved the behaviour is 0-6.
//
// Ghidra renders adder_behavior_dispatch with cases 1-6 inline plus a fat
// trailing "default" block, because case 0's target (0x00472d40) had been
// auto-created as a function. That trailing block IS case 0 - the chase - and
// this file spells it out as adder_behavior_chase.
//
// ---------------------------------------------------------------------------
// Shape of the AI - FOUR levels
// ---------------------------------------------------------------------------
//   Entity+0x84 state             init / run / damaged / death.
//   Entity+0x85 ignore_player_flag 0 = the decision layer runs this frame,
//                                 1 = the behaviour owns the snake outright,
//                                 anything else = frozen (nothing runs).
//   Entity+0x02 behavior_flags    the SPAWN KIND, and it doubles as the index
//                                 into adder_decide_table. 0 = a snake already
//                                 on the floor, 1/2 = hanging from the ceiling
//                                 (2 drops right on top of the player), 3 = a
//                                 snake hidden in the scenery that emerges.
//                                 Behaviours 1 and 6 reset it to 0, which is
//                                 how an ambush snake becomes a floor snake.
//   Entity+0x86 action_behavior   0-6, the thing the snake is doing.
//   Entity+0x87 action_state      the per-behaviour animation sub-state.
//
// ---------------------------------------------------------------------------
// Field aliases
// ---------------------------------------------------------------------------
// The port's Entity struct names these bytes from the zombie's point of view.
// The adder reuses them at other widths, so they are reached by offset per the
// "offset writes, not nearest field" rule:
//
//   +0x38 int    localMatrix.t[1], the snake's ALTITUDE. PS1 Y is negative up,
//                so a ceiling snake starts at -8120 and the drop ADDS to it.
//   +0x72 short  rotation SVECTOR .x (the port's position.pad)
//   +0x76 short  rotation SVECTOR .z - the spin applied while falling; the drop
//                bleeds it off 0x55 per frame and clamps at 0.
//   +0xCA short  body tilt; init sets 0x3000 and the retreat winds it down.
//   +0x16C word  entity_pathfind_update result; only bit 0 is used.
//   +0x16E word  ResolveEntityScaCollision result (the snake touched the player)
//   +0x170 ushort manhattan distance to the player, |dx| + |dz|, or 4999 when
//                the player is on another floor.
//   +0x172 word  check_room_collision result
//   +0x174 word  collision/attack enable. A WORD, not a byte: adder_update
//                gates its whole SCA block on `CMP word ptr [ECX+0x174],0x0`.
//   +0x176 ushort frames spent pinned against geometry
//
// All original addresses from Ghidra.
// ============================================================================
#include "EntityCommon.h"
#include "../../Globals.h"
#include "../BioCard.h"
#include <cstdlib>

extern void ResetJointTransforms(void);                            // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);           // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90 - Room.cpp
// 0x0043d8a0. Named for the zombie because that is where it was first found,
// but it is entity-generic: a heavy weapon cancels the flinch every fifth
// frame. adder_damaged_flinch calls it at 0x00473510.
extern void zombie_check_special_weapon(void);                     // 0x0043d8a0

// 0x00be0df4 - shared entity scratch, defined in EntityCommon.cpp.
extern unsigned int g_entity_bkp;                                  // 0x00be0df4

// ============================================================================
// Offset accessors. Every width here is one the original actually uses.
// ============================================================================
#define AD_Y          (*(int*)            ((char*)ENTITY + 0x38))
#define AD_STATE_W    (*(short*)          ((char*)ENTITY + 0x84))
#define AD_STATE_DW   (*(unsigned int*)   ((char*)ENTITY + 0x84))
#define AD_BEH_W      (*(unsigned short*) ((char*)ENTITY + 0x86))
#define AD_ROT_X      (*(short*)          ((char*)ENTITY + 0x72))
#define AD_SPIN       (*(short*)          ((char*)ENTITY + 0x76))
#define AD_UNK_C1     (*(unsigned char*)  ((char*)ENTITY + 0xC1))
#define AD_TILT       (*(short*)          ((char*)ENTITY + 0xCA))
#define AD_PATH_W     (*(unsigned short*) ((char*)ENTITY + 0x16C))
#define AD_PATH_B     (*(unsigned char*)  ((char*)ENTITY + 0x16C))
#define AD_TOUCH      (*(short*)          ((char*)ENTITY + 0x16E))
#define AD_DIST       (*(unsigned short*) ((char*)ENTITY + 0x170))
#define AD_COLL       (*(unsigned short*) ((char*)ENTITY + 0x172))
#define AD_ACTIVE     (*(short*)          ((char*)ENTITY + 0x174))
#define AD_STUCK      (*(unsigned short*) ((char*)ENTITY + 0x176))

// g_tempVar (0x00be0df8) is declared `void*` in Globals.h; adder_state_run
// parks a sign-extended short in it, exactly as the Yawn does.
#define AD_TURN_SLOW  (*(int*)&g_tempVar)

// The player's translation vector, the target of every steering call here.
#define ADDER_PLAYER_T ((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t)

namespace {

// ============================================================================
// 0x004c3620 / 0x004c3630 - the SCA collision records, six shorts each, the
// same layout as the zombie's and the crow's: [0] id/terminator, [1..3] local
// x/y/z, [4] half-height, [5] radius. A snake is flat on the floor, so its
// half-height is 0 and only the 200-unit radius matters - check_room_collision
// pulls that back out of Entity->Sca_info + 10 in adder_update.
//
// 0x004c3640 holds a POINTER to the first record and adder_state_init loads the
// pointer (`MOV EDX,[0x004c3640]`), not the address of the slot. The slot right
// after it at 0x004c3644 points at a second, all-zero record with id 0x8001 and
// has NO readers anywhere in the exe - a leftover second collision profile. It
// is reproduced here so the table keeps its original shape.
// ============================================================================
const short adder_sca_info[6]     = { (short)0x8000, 0, 0, 0, 0, 200 };
const short adder_sca_info_alt[6] = { (short)0x8001, 0, 0, 0, 0, 0 };

const short* const adder_sca_info_table[2] = {
    adder_sca_info,      // 0x004c3640
    adder_sca_info_alt   // 0x004c3644 - never read
};

void adder_state_init(void);       // 0x00472620
void adder_state_run(void);        // 0x004728e0
void adder_state_damaged(void);    // 0x004729e0
void adder_state_death(void);      // 0x00472a40

void adder_decide_action(void);          // 0x00472ac0
void adder_decide_ground(void);          // 0x00472b30
void adder_decide_ceiling_near(void);    // 0x00472c10
void adder_decide_ceiling_ambush(void);  // 0x00472c50
void adder_decide_hidden(void);          // 0x00472d00

void adder_behavior_dispatch(void);   // 0x00472a70
void adder_behavior_chase(void);      // 0x00472d40
void adder_behavior_drop(void);       // 0x00472e70
void adder_behavior_turn_away(void);  // 0x00472fb0
void adder_behavior_bite(void);       // 0x00473070
void adder_behavior_victory(void);    // 0x00473270
void adder_behavior_idle(void);       // 0x00473320
void adder_behavior_emerge(void);     // 0x00473330

void adder_damaged_flinch(void);   // 0x00473420
void adder_death_writhe(void);     // 0x00473560
void adder_death_retreat(void);    // 0x004737b0

// ============================================================================
// adder_state_table @ 0x004c3648
// FOUR entries, indexed by Entity->state. See the header comment for why the
// pointers that follow are NOT states 4-10.
//
// State 2 and state 3 are the entry points the damage system uses:
// weapon_apply_damage (WeaponDamage.cpp) sets state 3 outright and demotes it
// to state 2 when the health survives.
// ============================================================================
void* const adder_state_table[4] = {
    (void*)adder_state_init,     // [0] spawn / respawn
    (void*)adder_state_run,      // [1] main AI driver
    (void*)adder_state_damaged,  // [2] hit reaction
    (void*)adder_state_death     // [3] death
};

// ============================================================================
// adder_decide_table @ 0x004c3678
// FOUR entries, indexed by Entity->behavior_flags - the spawn kind. This is the
// decision layer: it only picks the next action_behavior and never animates,
// and it only runs while ignore_player_flag is 0.
// ============================================================================
void* const adder_decide_table[4] = {
    (void*)adder_decide_ground,          // [0] already on the floor
    (void*)adder_decide_ceiling_near,    // [1] ceiling, drops when close
    (void*)adder_decide_ceiling_ambush,  // [2] ceiling, drops ONTO the player
    (void*)adder_decide_hidden           // [3] hidden, emerges when close
};

// ============================================================================
// adder_behavior_jumptable @ 0x004c3658
// The seven case bodies of adder_behavior_dispatch's switch on action_behavior.
// In the original these are JMP targets inside one function, not a call table;
// they are separate functions here because each case body ends in its own RET,
// which makes the two forms equivalent.
// ============================================================================
void* const adder_behavior_jumptable[7] = {
    (void*)adder_behavior_chase,      // [0] approach the player
    (void*)adder_behavior_drop,       // [1] fall from the ceiling
    (void*)adder_behavior_turn_away,  // [2] recoil / turn 180
    (void*)adder_behavior_bite,       // [3] the poison bite
    (void*)adder_behavior_victory,    // [4] the player is dead
    (void*)adder_behavior_idle,       // [5] nothing at all (a bare RET)
    (void*)adder_behavior_emerge      // [6] rise out of the scenery
};

// ---------------------------------------------------------------------------
// Every effect the adder spawns seeds its position from the 4-dword block at
// g_deadMoveValue + 0x14, the same block the zombie's, the dog's and the crow's
// billboard code copies. All four dwords are copied verbatim.
// ---------------------------------------------------------------------------
void adder_seed_effect_pos(void)
{
    const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
    g_playerPosScratch.x   = seed[0];
    g_playerPosScratch.y   = seed[1];
    g_playerPosScratch.z   = seed[2];
    g_playerPosScratch.pad = seed[3];
}

// ============================================================================
// adder_state_init @ 0x00472620
// State 0. Runs once on spawn - and again after adder_death_retreat, which is
// how a shot ceiling snake comes back.
//
// The health roll is FOUR rand() calls, the first of which is thrown away:
// 10 + 2*(r&7) + 2*(r&7) + 2*(r&7), so 10..52.
// ============================================================================
void adder_state_init(void)
{
    AD_STATE_W = 1;                          // state = 1, ignore_player_flag = 0
    AD_BEH_W   = 0;                          // behaviour + sub-state
    ENTITY->scaMatrixData.field_00 = 0;
    AD_UNK_C1  = 0;
    ENTITY->action_ticks_counter = 0;
    ENTITY->hit_state = 0;
    ENTITY->status_flags = 1;
    ResetJointTransforms();

    // The ground shadow quad. g_animFrameIdSave carries the tint the builder
    // copies into the quad header - 0x202020 here, a very dark snake shadow.
    g_svecScratch.z = 0;
    g_svecScratch.y = 0;
    g_svecScratch.x = 0;
    g_animFrameIdSave = 0x202020;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 0, 0);

    rand();                                                  // discarded
    unsigned short h0 = (unsigned short)rand() & 7;
    unsigned short h1 = (unsigned short)rand() & 7;
    unsigned short h2 = (unsigned short)rand() & 7;
    ENTITY->health = (short)(h0 * 2 + h1 * 2 + h2 * 2 + 10);

    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->animationId        = 0;
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);

    ENTITY->Sca_info = (unsigned int)(uintptr_t)adder_sca_info_table[0];
    AD_TILT   = 0x3000;
    AD_STUCK  = 0;
    AD_ACTIVE = 1;

    // Any non-zero spawn kind starts the snake in the air, spun 180 degrees
    // about Z, with a 180..240 frame fuse on the ambush timer.
    if (ENTITY->behavior_flags != 0) {
        ENTITY->action_ticks_counter =
            (unsigned short)(short)((short)(0x18 - (rand() & 6)) * 10);
        AD_ROT_X = 0;
        AD_SPIN  = 0x800;
        if (AD_Y == 0) {
            AD_Y = -0x1fb8;                  // -8120, the ceiling
        }
    }

    // Spawn kind 3 is the snake hidden in the scenery: on the floor, upright,
    // intangible, and hit_state = 1 marks it as not yet emerged.
    if (ENTITY->behavior_flags == 3) {
        AD_SPIN   = 0;
        AD_Y      = 0;
        AD_ACTIVE = 0;
        ENTITY->hit_state = 1;
    }
}

// ============================================================================
// adder_state_run @ 0x004728e0
// State 1, the main driver. Both turn_toward_target probes are taken every
// frame and parked in the shared scratch for the behaviours to read back:
//   g_tempVar     the 0x100 (slow) step - adder_decide_ground's "am I lined up
//                 with the player" test.
//   g_entity_bkp  the 0x200 (fast) step - adder_behavior_bite's aim gate.
// ============================================================================
void adder_state_run(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    AD_TURN_SLOW = (int)(short)turn_toward_target(ADDER_PLAYER_T, 0x100);
    g_entity_bkp = (unsigned int)(int)(short)turn_toward_target(ADDER_PLAYER_T, 0x200);

    // ignore_player_flag 0 falls THROUGH into the behaviour dispatcher; 1 skips
    // straight to it; anything else runs neither.
    if (ENTITY->ignore_player_flag == 0) {
        adder_decide_action();
        adder_behavior_dispatch();
    } else if (ENTITY->ignore_player_flag == 1) {
        adder_behavior_dispatch();
    }

    ENTITY->status_flags &= 0x1f;

    // Only a snake standing on the same floor as the player can notice it.
    if (AD_Y == 0 && g_playerEntity.scaMatrixData.localMatrix.t[1] == 0) {
        entity_check_visual_range(5000);
    }

    // The ground shadow tracks the HEAD JOINT's world height, not the entity
    // origin, and it is skipped entirely above 4000 units. Y is negative up, so
    // the arithmetic shift is what makes the shadow shrink as the snake rises.
    if (AD_Y > -4000) {
        g_collPushDepthZHi = (joints[0].world.t[1] >> 4) + 500;
        if (g_collPushDepthZHi < 0) {
            g_collPushDepthZHi = 300;
        }
        BillboardSetSize(&ENTITY->pushVelocity,
                         (short)(g_collPushDepthZHi + 500),
                         (short)g_collPushDepthZHi);
    }

    if (AD_COLL != 0) {
        AD_STUCK = (unsigned short)(AD_STUCK + 1);
    } else {
        AD_STUCK = 0;
    }
}

// ============================================================================
// adder_state_damaged @ 0x004729e0
// State 2, the hit reaction. Only behaviour 0 has a flinch; every other
// behaviour just keeps its shadow updated while the state runs out.
// ============================================================================
void adder_state_damaged(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    if (ENTITY->action_behavior == 0) {
        adder_damaged_flinch();
    }

    g_collPushDepthZHi = (joints[0].world.t[1] >> 4) + 400;
    if (g_collPushDepthZHi < 0) {
        g_collPushDepthZHi = 200;
    }
    BillboardSetSize(&ENTITY->pushVelocity,
                     (short)(g_collPushDepthZHi + 400),
                     (short)g_collPushDepthZHi);
}

// ============================================================================
// adder_state_death @ 0x00472a40
// State 3. Two death paths and a terminal behaviour:
//   behaviour 0  adder_death_writhe  - thrash, bleed, raise the room event flag
//   behaviour 3  adder_death_retreat - slither off and come back (see below)
//   behaviour 4  nothing runs; the corpse just lies there
// ============================================================================
void adder_state_death(void)
{
    if (ENTITY->action_behavior == 0) {
        adder_death_writhe();
        return;
    }
    if (ENTITY->action_behavior != 3) {
        return;
    }
    adder_death_retreat();
}

// ============================================================================
// adder_decide_action @ 0x00472ac0
// Refresh the distance to the player, then hand off to the decision handler for
// this snake's spawn kind.
//
// The distance is a manhattan |dx| + |dz|. Both absolute values are taken on
// the FULL 32-bit differences (CDQ/XOR/SUB at 0x00472ae0 and 0x00472aef) and
// only the sum is truncated to 16 bits by `ADD SI,AX`. Taking the absolute
// value of the truncated halves instead - which is how the decompiler renders
// it - gives a different answer for any pair more than 32767 units apart.
//
// A player on another floor reports a flat 4999, which sits between the 2000
// bite threshold and the 5000/10000 notice thresholds: far enough not to be
// attacked, close enough that the ceiling ambush still arms.
// ============================================================================
void adder_decide_action(void)
{
    if (g_playerEntity.scaMatrixData.localMatrix.t[1] == 0) {
        int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
               - ENTITY->scaMatrixData.localMatrix.t[2];
        if (dz < 0) dz = -dz;
        int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
               - ENTITY->scaMatrixData.localMatrix.t[0];
        if (dx < 0) dx = -dx;
        AD_DIST = (unsigned short)(dz + dx);
    } else {
        AD_DIST = 4999;
    }

    ((void (*)(void))adder_decide_table[ENTITY->behavior_flags])();
}

// ============================================================================
// adder_decide_ground @ 0x00472b30
// Spawn kind 0 - a snake already on the floor.
//
// Note the order: the "player is close but I cannot path to it" test can pick
// behaviour 2 and then be overridden in the same frame by the bite test below
// it, which is why a cornered snake still strikes.
// ============================================================================
void adder_decide_ground(void)
{
    // Close enough to react, but entity_pathfind_update says the way is blocked
    // -> spin in place instead of walking into the wall.
    if (AD_DIST < 10000 && (AD_PATH_B & 1) == 0) {
        ENTITY->ignore_player_flag = 1;
        AD_BEH_W = 2;
    }

    // Within biting range and lined up (the slow turn step came back 0), or the
    // SCA pass says the snake is already touching the player.
    if ((AD_DIST < 2000 && AD_TURN_SLOW == 0) || AD_TOUCH != 0) {
        ENTITY->ignore_player_flag = 1;
        AD_BEH_W = 3;
        AD_TOUCH = 0;
    } else {
        // Pinned against geometry for a full second - turn around and try again.
        if (AD_STUCK > 0x3c) {
            ENTITY->ignore_player_flag = 1;
            AD_BEH_W = 2;
            AD_STUCK = 0;
        }
        if (g_playerEntity.health < 0) {
            ENTITY->ignore_player_flag = 1;
            AD_BEH_W = 4;
        }
    }
}

// ============================================================================
// adder_decide_ceiling_near @ 0x00472c10
// Spawn kind 1 - hangs still until the player walks under it.
// ============================================================================
void adder_decide_ceiling_near(void)
{
    AD_BEH_W = 5;
    if (AD_DIST < 3000) {
        ENTITY->ignore_player_flag = 1;
        AD_BEH_W = 1;
    }
}

// ============================================================================
// adder_decide_ceiling_ambush @ 0x00472c50
// Spawn kind 2 - the scripted drop. It waits for the player to hold the
// door-opening pose (action_behavior 0x14, action_state 0) within 5000 units,
// or for the init fuse to burn out, and then TELEPORTS itself to within 100
// units of the player before falling, so the snake always lands on them.
// ============================================================================
void adder_decide_ceiling_ambush(void)
{
    AD_BEH_W = 5;

    bool drop;
    if (g_playerEntity.action_behavior == 0x14 && g_playerEntity.action_state == 0
        && AD_DIST < 5000) {
        drop = true;
    } else {
        // The fuse only ticks on the frames the pose test fails.
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        drop = (ticks == 0);
    }

    if (drop) {
        ENTITY->scaMatrixData.localMatrix.t[0] =
            (rand() & 1) * 100 + g_playerEntity.scaMatrixData.localMatrix.t[0];
        ENTITY->scaMatrixData.localMatrix.t[2] =
            (rand() & 1) * 100 + g_playerEntity.scaMatrixData.localMatrix.t[2];
        ENTITY->ignore_player_flag = 1;
        AD_BEH_W = 1;
    }
}

// ============================================================================
// adder_decide_hidden @ 0x00472d00
// Spawn kind 3 - lies intangible in the scenery until the player is within 4500.
// ============================================================================
void adder_decide_hidden(void)
{
    AD_BEH_W = 5;
    if (AD_DIST < 0x1194) {              // 4500
        ENTITY->ignore_player_flag = 1;
        AD_BEH_W = 6;
    }
}

// ============================================================================
// adder_behavior_dispatch @ 0x00472a70
// Refresh the pathfinder, then JMP into the behaviour case body.
//
// entity_pathfind_update returns a small bitfield; the adder only keeps bit 0,
// and only when no other bit is set - `TEST ECX,0xfffffffe` at 0x00472a79. The
// AND is a BYTE write and the OR that follows is a WORD write, both to +0x16C.
// ============================================================================
void adder_behavior_dispatch(void)
{
    unsigned char path = (unsigned char)entity_pathfind_update();
    g_animFrameIdSave = (unsigned int)path;

    if ((path & 0xfe) == 0) {
        AD_PATH_B = (unsigned char)(AD_PATH_B & 0xfe);
        AD_PATH_W = (unsigned short)(AD_PATH_W | ((unsigned short)g_animFrameIdSave & 1));
    }

    ((void (*)(void))adder_behavior_jumptable[ENTITY->action_behavior])();
}

// ============================================================================
// adder_behavior_chase @ 0x00472d40   (jumptable case 0)
// Slither toward the player. Inside 8000 units it steers with the per-snake turn
// rate rolled into action_ticks_counter, scaled by the pathfinder bit so a snake
// with no route holds its heading; beyond that it uses the flat 0x40 slew.
//
// The extra (rand & 0xf) + (rand & 0xf) added to the yaw every frame is what
// gives the adder its wobbly, non-committal approach.
// ============================================================================
void adder_behavior_chase(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->action_state       = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->animationId        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->move_speed_current   = (unsigned short)(short)(0x32 - (rand() & 0xf));
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0xf) + 0x10);
    } else if (st != 1) {
        return;
    }

    if (AD_DIST < 8000) {
        short turn = (short)turn_toward_target(ADDER_PLAYER_T,
                                              (short)ENTITY->action_ticks_counter);
        ENTITY->angle = (short)(ENTITY->angle + (short)(turn * (short)(AD_PATH_W & 1)));
    } else {
        entity_rotate_toward_target(ADDER_PLAYER_T, 0x40);
    }

    short wobble = (short)(rand() & 0xf);
    wobble = (short)(wobble + (short)(rand() & 0xf));
    ENTITY->angle = (short)(ENTITY->angle + wobble);

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    Add_speedXZ(0);
}

// ============================================================================
// adder_behavior_drop @ 0x00472e70   (jumptable case 1)
// The fall from the ceiling. Y accelerates as ticks^2 * 8 - a quadratic, not a
// running velocity - and the 180-degree Z spin from init bleeds off 0x55 per
// frame, so the snake rights itself on the way down.
//
// The landing test reads the TOP BYTE of the altitude (`TEST byte ptr
// [ENTITY+0x3B],0x80`): Y is negative while airborne, so the frame its sign bit
// clears is the frame it reaches the floor.
// ============================================================================
void adder_behavior_drop(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->action_state       = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->animationId        = 0;
        ENTITY->action_ticks_counter = 0;
        if (AD_Y < -0x1fb8) {
            AD_Y = -0x1fb8;              // never start higher than 8120 units
        }
        // Becoming a floor snake: from here on the decision layer is
        // adder_decide_ground.
        ENTITY->behavior_flags = 0;
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        Snd_em(2);                       // the landing hiss
        ENTITY->ignore_player_flag = 0;
        AD_BEH_W = 0;
        return;
    }

    int ticks = (int)(short)ENTITY->action_ticks_counter;
    AD_Y += (ticks * ticks) << 3;
    ENTITY->action_ticks_counter = (unsigned short)(ENTITY->action_ticks_counter + 1);

    AD_SPIN = (short)(AD_SPIN - 0x55);
    if (AD_SPIN < 0) {
        AD_SPIN = 0;
    }

    if ((*(unsigned char*)((char*)ENTITY + 0x3B) & 0x80) == 0) {
        AD_Y    = 0;
        AD_SPIN = 0;
        ENTITY->action_state = 2;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// adder_behavior_turn_away @ 0x00472fb0   (jumptable case 2)
// One of the two coil animations (1 or 2, rolled), and on completion a flat
// 180-degree snap before handing control back to the decision layer.
// ============================================================================
void adder_behavior_turn_away(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state       = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->animationId        = (unsigned char)((rand() & 1) + 1);
        ENTITY->action_ticks_counter = (unsigned short)(rand() & 0x3f);
        ENTITY->blend_counter      = 3;
    } else if (ENTITY->action_state != 1) {
        return;
    }

    char done = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    if (done != 0) {
        ENTITY->angle = (short)(ENTITY->angle + 0x800);
        ENTITY->ignore_player_flag = 0;
        AD_BEH_W = 0;
    }
}

// ============================================================================
// adder_behavior_bite @ 0x00473070   (jumptable case 3)
// The poison bite. The reach test runs every frame against joint 1's world
// matrix (jointsStructs + 0xC0 = joint[1].world) with a 900-unit square box,
// and its answer is parked in g_collPushDepthZLo.
//
// The hit lands only on animation frame 11, and only when all five gates agree:
// the fast turn probe reads 0 (the snake is aimed), the reach box hit, the
// player is not already being attacked, and the pathfinder bit is set. Poison
// is a double coin flip - (rand & 1) * (rand & 1) - so one bite in four
// envenomates, with 150 in the player's poison timer at +0x174.
// ============================================================================
void adder_behavior_bite(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->animationId        = 3;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->move_speed_current = 0x32;
        Snd_em(0);
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        ENTITY->ignore_player_flag = 1;
        AD_BEH_W = 2;                    // recoil out of the strike
        return;
    }

    adder_seed_effect_pos();
    g_collPushDepthZLo = (int)FUN_0048ae00(&ENTITY->jointsStructs[1].world,
                                          &g_playerPosScratch, 900,
                                          g_playerEntity.scaMatrixData.localMatrix.t);

    char adv = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);

    if (ENTITY->animation_frame_id == 0xb && g_entity_bkp == 0
        && g_collPushDepthZLo != 0
        && g_playerEntity.isBeingAttackedFlag == 0 && (AD_PATH_B & 1) != 0) {
        Snd_em(1);

        unsigned int c0 = (unsigned int)rand() & 1;
        unsigned int c1 = (unsigned int)rand() & 1;
        if (c0 * c1 != 0) {
            g_playerEntity.healthStatusFlags |= 2;   // poisoned
            g_playerEntity.pad_174 = 0x96;           // 150-frame poison timer
        }

        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.action_behavior     = 100;
        g_playerEntity.health = (short)(g_playerEntity.health - 6);
        Play3DSnd(3, 0, 0, (int)g_playerEntity.scaMatrixData.localMatrix.t);

        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -520;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 1, g_playerEntity.directionAngle,
                               &g_playerEntity.scaMatrixData.localMatrix,
                               &g_playerPosScratch, 0);
    }

    Add_speedXZ(0);
}

// ============================================================================
// adder_behavior_victory @ 0x00473270   (jumptable case 4)
// The player is dead. Same setup as the bite but with no reach test and no
// exit: the snake loops the strike animation forever.
// ============================================================================
void adder_behavior_victory(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->animationId        = 3;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->move_speed_current = 0x32;
        Snd_em(0);
    } else if (ENTITY->action_state != 1) {
        return;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    Add_speedXZ(0);
}

// ============================================================================
// adder_behavior_idle @ 0x00473320   (jumptable case 5)
// A bare RET in the original - the hold state for a snake that has not been
// triggered yet. Not a stub: 0x00473320 is one instruction long.
// ============================================================================
void adder_behavior_idle(void)
{
}

// ============================================================================
// adder_behavior_emerge @ 0x00473330   (jumptable case 6)
// Spawn kind 3 rising out of the scenery: 60 frames of the walk animation,
// after which the snake becomes tangible (AD_ACTIVE = 1, hit_state cleared) and
// hands over to the ground decision layer.
// ============================================================================
void adder_behavior_emerge(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->action_state       = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->animationId        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->move_speed_current   = 0x1e;
        ENTITY->action_ticks_counter = 0x3c;
        ENTITY->behavior_flags       = 0;
    } else if (st != 1) {
        return;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    Add_speedXZ(0);

    short ticks = (short)ENTITY->action_ticks_counter;
    ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
    if (ticks == 0) {
        ENTITY->hit_state = 0;
        AD_ACTIVE = 1;
        ENTITY->ignore_player_flag = 0;
        AD_BEH_W = 0;
    }
}

// ============================================================================
// adder_damaged_flinch @ 0x00473420
// State 2's only behaviour. Plays the strike animation as a recoil and hides
// the four TAIL joints (8..11) for the duration, which is what makes a hit
// snake look like it has been shortened.
//
// The loop counter lives in the g_animFrameIdSave scratch and the addressing is
// `joints + 0x554 - i*0x7C` with i counting 3 down to 0 - 0x554 is 11 * 0x7C,
// so the joints touched are 8, 9, 10 and 11.
// ============================================================================
void adder_damaged_flinch(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->animationId        = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1f) + 0x10);

        JointStruct* joints = ENTITY->jointsStructs;
        g_animFrameIdSave = 3;
        for (;;) {
            *(unsigned char*)((char*)joints + 0x554
                              - (int)g_animFrameIdSave * 0x7c) = 0;
            unsigned int i = g_animFrameIdSave;
            g_animFrameIdSave = g_animFrameIdSave - 1;
            if (i == 0) break;
        }
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        if (ticks != 0) {
            return;
        }
        AD_STATE_W = 1;                  // state = 1, ignore_player_flag = 0
        AD_BEH_W   = 0;
        ENTITY->hit_state = 0;
        return;
    }

    char adv = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);
    zombie_check_special_weapon();
}

// ============================================================================
// adder_death_writhe @ 0x00473560
// State 3, behaviour 0 - the death, and the fork that decides which death.
//
// The weapon that killed the snake is in the top five bits of hit_state:
// weapon_apply_damage ORs `weapon_id << 3` into it (WeaponDamage.cpp), so
// hit_state & 0xF8 is the weapon id times 8.
//
//   == 0x38  weapon 7, the explosive bazooka round: gore effect on all eleven
//            joints, no shadow, straight to behaviour 4, event flag raised.
//   >  8     weapon 2 and up, i.e. any firearm: behaviour 3, the retreat.
//   <= 8     the knife (or an unarmed kill): fall into the writhe animation
//            below, which ends in the blood pool and behaviour 4.
// ============================================================================
void adder_death_writhe(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    unsigned char st = ENTITY->action_state;
    if (st == 0) {
        ENTITY->animationId        = 4;
        ENTITY->animation_frame_id = 0x1e;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->status_flags |= 2;
        ENTITY->status_flags |= 8;       // ENTITY_STATUS_DEAD
        ENTITY->move_speed_current   = 0x28;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0xf) + 0x10);

        int weapon = (int)ENTITY->hit_state & 0xf8;

        if (weapon == 0x38) {
            g_animFrameIdSave = 10;
            for (;;) {
                joint_setup_attack_effect(
                    (int)(intptr_t)&joints[g_animFrameIdSave], 8, 5, 3);
                unsigned int i = g_animFrameIdSave;
                g_animFrameIdSave = g_animFrameIdSave - 1;
                if (i == 0) break;
            }
            BillboardSetSize(&ENTITY->pushVelocity, 0, 0);
            ENTITY->action_behavior = 4;
            Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
            return;
        }

        if (weapon > 8) {
            BillboardSetSize(&ENTITY->pushVelocity, 0, 0);
            ENTITY->action_behavior = 3;
            return;
        }
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        // The blood pool spreading under the corpse.
        BillboardAdjSize(&ENTITY->pushVelocity, 0x16, 0xb);
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        if (ticks != 0) {
            return;
        }
        ENTITY->action_behavior = 4;
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        return;
    }

    if ((char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400) != 0) {
        // The shadow becomes the blood pool: recoloured, reseeded small, then
        // grown by BillboardAdjSize above.
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00FFFF50);
        BillboardSetSize(&ENTITY->pushVelocity, 0x14, 10);
        ENTITY->action_state = 2;
    }

    // Unsigned compare: the writhe stops sliding once the animation timer
    // passes 0x23.
    if (ENTITY->timing_control < 0x23) {
        Add_speedXZ(0);
    }
}

// ============================================================================
// adder_death_retreat @ 0x004737b0
// State 3, behaviour 3 - what a shot snake actually does. It is NOT a corpse
// handler: the snake sprays blood, thrashes, hides all eleven joints, sinks to
// Y = -20000, waits out a 210..241 frame cooldown and then RESETS ITSELF -
// joints back on, spawn kind forced to a ceiling variant, state back to 0,
// which re-runs adder_state_init and rolls fresh health. This is the mechanism
// behind the rooms where the snakes never stop coming.
//
// The spawn kind it comes back as is 2 (drop on the player) everywhere except
// g_roomId 1, which gets 1 (drop when close).
//
// The switch has TWO fall-throughs, both real: case 0 runs on into case 1 on
// its first frame, and case 3 runs on into case 4.
// ============================================================================
void adder_death_retreat(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    switch (ENTITY->action_state) {
    case 0: {
        ENTITY->animationId        = 4;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1f) + 0x10);

        // Two blood sprays, at the head joint and at joint 5 (jointsStructs +
        // 0x44 and + 0x2B0), both aimed back along the player's facing.
        g_playerPosScratch.z = 0;
        g_playerPosScratch.y = 0;
        g_playerPosScratch.x = 0;
        Effect_CreateBillboard(0, 7,
            (short)(g_playerEntity.directionAngle - ENTITY->angle + 0x800),
            &joints[0].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 7,
            (short)(g_playerEntity.directionAngle - ENTITY->angle + 0x800),
            &joints[5].world, &g_playerPosScratch, 0);
    }
        // fall through
    case 1: {
        AD_TILT = (short)(AD_TILT - 0xf5);
        char adv = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);
        return;
    }

    case 2:
        // Hide every joint - the snake vanishes rather than leaving a corpse.
        g_animFrameIdSave = 10;
        for (;;) {
            joints[g_animFrameIdSave].flags = 0;
            unsigned int i = g_animFrameIdSave;
            g_animFrameIdSave = g_animFrameIdSave - 1;
            if (i == 0) break;
        }
        ENTITY->action_state = 3;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1f) + 0xd2);
        return;

    case 3:
        AD_Y = -20000;                   // park it out of sight, high up
        ENTITY->action_state = 4;
        // fall through
    case 4: {
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        if (ticks == 0) {
            g_animFrameIdSave = 10;
            for (;;) {
                joints[g_animFrameIdSave].flags = 3;
                unsigned int i = g_animFrameIdSave;
                g_animFrameIdSave = g_animFrameIdSave - 1;
                if (i == 0) break;
            }
            ENTITY->behavior_flags = 2;
            if (g_roomId == ROOM_WATER_GATE) {
                ENTITY->behavior_flags = 1;
            }
            // state = 0, ignore_player_flag = 0, behaviour = 0, sub-state = 0,
            // written as ONE dword: `MOV dword ptr [EAX+0x84],0x0`.
            AD_STATE_DW = 0;
        }
        return;
    }

    default:
        return;
    }
}

}  // namespace

// ============================================================================
// adder_update @ 0x004727f0
// Per-frame entry, enemies_update_functions_tbl[10].
//
// The AI half is gated on g_message_flags bit 2 - clear while a message box or
// menu is up - so a snake freezes mid-strike rather than carrying on behind the
// text.
//
// The whole SCA/collision block is gated a second time on AD_ACTIVE (+0x174 as
// a WORD). That is how a hidden spawn-kind-3 snake and a snake in its emerge
// window are intangible: the state handler still animates them, but no volume
// is built, nothing resolves against the player and no room collision is asked
// for.
// ============================================================================
void adder_update(void)
{
    if ((g_message_flags & 4) != 0) {
        ((void (*)(void))adder_state_table[ENTITY->state])();

        if (AD_ACTIVE != 0) {
            SetEntityScaHitData(ENTITY);
            AD_TOUCH = (short)(unsigned char)ResolveEntityScaCollision(
                (Entity*)&g_playerEntity, ENTITY);
            HandleEnemyPlayerCollisions();
            AD_COLL = (unsigned short)check_room_collision(
                (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
                *(short*)((char*)(uintptr_t)ENTITY->Sca_info + 10));
        }
    }

    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0], g_CurrentRdtDataTypePtr);

    // Any snake that has not become a floor snake yet - hanging from the
    // ceiling or still hidden - is excluded from the camera switch zones, and
    // that same flag suppresses its ground shadow.
    if (ENTITY->behavior_flags != 0) {
        ENTITY->has_enter_switch_zone = 0;
    }

    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite((VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
                               (short*)&ENTITY->pushVelocity, 0, ENTITY->angle);
    }
}
