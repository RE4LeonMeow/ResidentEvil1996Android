// Crow.cpp - Crow (entity type 5, em1005.emd)
//
// Original PC addresses:
//   crow_update              0x0042e520   per-frame entry (dispatch table [5])
//   crow_state_table         0x004badd0   NINE entries, then a NULL pad
//   behaviour jumptable      0x004badf8   FOURTEEN entries (a compiler switch)
//   crow_sca_info            0x004badc0   SCA collision record
//   pointer to the record    0x004badcc   -> 0x004badc0
//
// The type id is 5, not 3. The dispatch table comment in EntityCommon.cpp and
// the `ENEMY_CROW = 3` line in EntityCommon.h disagreed; the model table settles
// it, because g_emdPathTable index (id + 4) = 9 is em1005.emd. Id 3 is the web
// spinner (0x00478310).
//
// ---------------------------------------------------------------------------
// Shape of the AI
// ---------------------------------------------------------------------------
// THREE levels, and the middle one is easy to miss:
//
//   Entity+0x84 state             picks init / driver / recoil / death / SCD.
//                                 crow_update CALLs crow_state_table[state].
//   Entity+0x85 ignore_player_flag 0 = perched (watch for the take-off cue),
//                                 1 = airborne. Both fall through into the
//                                 behaviour dispatcher; anything else skips it.
//   Entity+0x86 action_behavior   0-13, the flight behaviour. Dispatched by
//                                 crow_behavior_dispatch.
//   Entity+0x87 action_state      the per-behaviour animation sub-state, driven
//                                 by the four crow_anim_* helpers below.
//
// 0x004badf8 is a compiler SWITCH JUMPTABLE, not an installable dispatch table:
// the site at 0x0042ee6b is `JMP dword ptr [ECX*0x4 + 0x4badf8]`, a JMP into the
// middle of crow_behavior_dispatch, not a CALL. Do NOT create functions at its
// targets in Ghidra - they are case bodies. (Case 9's body at 0x0042f660 had
// been auto-created as FUN_0042f660, which is why the decompiler rendered that
// case as trailing "default" code.) Note there is no bounds check before the
// JMP; the compiler proved 0-13 and emitted none.
//
// 0x004badd0 by contrast IS a real call table - `CALL dword ptr [ECX*0x4 +
// 0x4badd0]` at 0x0042e53a - and stops at nine entries: index 9 is NULL, and
// the second table starts immediately after it at 0x004badf8.
//
// ---------------------------------------------------------------------------
// Field aliases
// ---------------------------------------------------------------------------
// The port's Entity struct names these bytes from the zombie's point of view.
// The crow reuses them at other widths, so they are reached by offset per the
// "offset writes, not nearest field" rule:
//
//   +0x38 int    localMatrix.t[1], the crow's ALTITUDE. PS1 Y is negative up,
//                so the flight envelope is -100 (floor) to -30000 (ceiling) and
//                every "climb" in this file SUBTRACTS from it.
//   +0x84 short  state + ignore_player_flag read as one word (crow_update)
//   +0x86 short  action_behavior + action_state written as one word - storing
//                the behaviour this way also clears the sub-state
//   +0x8E short  floor step reported by check_room_collision through
//                g_animFrameIdSave
//   +0xC6 ushort SCD fly-to target X   +0xC8 ushort SCD fly-to target Z
//   +0x16C short per-frame turn rate for the banking turn
//   +0x16E ushort pathfind result; only bit 0 is used
//   +0x170 short ResolveEntityScaCollision result (touching the player)
//   +0x172 short lower altitude bound rolled for behaviour 6
//   +0x174 short upper altitude bound rolled for behaviour 5
//   +0x176 short vertical velocity, and the hit-recovery timer
//   +0x178 ushort manhattan distance to the player (|dx| + |dz|)
//   +0x17A ushort check_room_collision result
//   +0x17C short frames spent against geometry
//   +0x17E short set to 100 by init and never read again
//   +0x182 short swerve angle held by entity_swerve_around_obstacle
//   +0x184 byte  swerve latch (bit 7 idle, bits 0-6 frames left)
//   +0x186 schar grab struggle counter - SIGNED, it is tested `< 0`
//   +0x18A short altitude bias; -400 for the behaviour_flags bit 0 variant
//
// All original addresses from Ghidra.
// ============================================================================
#include "EntityCommon.h"
#include "../../Globals.h"
#include <cstdlib>

extern void ResetJointTransforms(void);                            // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);           // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* pos, void* zoneData); // 0x00462d90 - Room.cpp

// 0x00be0de8 / 0x00be0df4 - shared entity scratch, defined in EntityCommon.cpp.
extern unsigned int g_entity_bkp;                                  // 0x00be0df4

// ============================================================================
// Offset accessors. Every width here is one the original actually uses.
// ============================================================================
#define CR_Y            (*(int*)           ((char*)ENTITY + 0x38))
#define CR_STATE_W      (*(short*)         ((char*)ENTITY + 0x84))
#define CR_BEH_W        (*(unsigned short*)((char*)ENTITY + 0x86))
#define CR_FLOOR_STEP   (*(short*)         ((char*)ENTITY + 0x8E))
#define CR_UNK_C1       (*(unsigned char*) ((char*)ENTITY + 0xC1))
#define CR_HOME_X       (*(unsigned short*)((char*)ENTITY + 0xC6))
#define CR_HOME_Z       (*(unsigned short*)((char*)ENTITY + 0xC8))
#define CR_TURN_RATE    (*(short*)         ((char*)ENTITY + 0x16C))
#define CR_PATH_W       (*(unsigned short*)((char*)ENTITY + 0x16E))
#define CR_PATH_B       (*(unsigned char*) ((char*)ENTITY + 0x16E))
#define CR_TOUCH        (*(short*)         ((char*)ENTITY + 0x170))
#define CR_FLOOR_LIMIT  (*(short*)         ((char*)ENTITY + 0x172))
#define CR_CEIL_LIMIT   (*(short*)         ((char*)ENTITY + 0x174))
#define CR_VY           (*(short*)         ((char*)ENTITY + 0x176))
#define CR_DIST         (*(unsigned short*)((char*)ENTITY + 0x178))
#define CR_COLL         (*(unsigned short*)((char*)ENTITY + 0x17A))
#define CR_STUCK        (*(unsigned short*)((char*)ENTITY + 0x17C))
#define CR_PHASE        (*(short*)         ((char*)ENTITY + 0x17E))
#define CR_SWERVE       (*(short*)         ((char*)ENTITY + 0x182))
#define CR_SWERVE_LATCH (*(unsigned char*) ((char*)ENTITY + 0x184))
#define CR_STRUGGLE     (*(signed char*)   ((char*)ENTITY + 0x186))
#define CR_ALT_BIAS     (*(short*)         ((char*)ENTITY + 0x18A))

// The player's translation vector, the target of every steering call here.
#define CROW_PLAYER_T ((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t)

// ---------------------------------------------------------------------------
// The original writes state / ignore_player_flag / action_behavior /
// action_state as ONE dword: `MOV dword ptr [EAX+0x84], 0x70101`. Little-endian
// that is 01 01 07 00, i.e. state=1 ignore=1 behaviour=7 substate=0 - so the
// write always clears the sub-state as a side effect. Spelling it out as four
// named bytes keeps that visible.
// ---------------------------------------------------------------------------
static inline void crow_set_state(unsigned char state, unsigned char airborne,
                                  unsigned char behaviour, unsigned char substate)
{
    ENTITY->state              = state;
    ENTITY->ignore_player_flag = airborne;
    ENTITY->action_behavior    = behaviour;
    ENTITY->action_state       = substate;
}

// ---------------------------------------------------------------------------
// Every effect the crow spawns seeds its position from the 4-dword block at
// g_deadMoveValue + 0x14 - the same block the zombie's and the dog's billboard
// code copies. All four dwords are copied verbatim; callers that need a
// specific height overwrite .y afterwards.
// ---------------------------------------------------------------------------
static void crow_seed_effect_pos(void)
{
    const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
    g_playerPosScratch.x   = seed[0];
    g_playerPosScratch.y   = seed[1];
    g_playerPosScratch.z   = seed[2];
    g_playerPosScratch.pad = seed[3];
}

// Two feather puffs 180 degrees apart - the crow's signature flap effect.
static void crow_spawn_feathers(void)
{
    crow_seed_effect_pos();
    Effect_CreateBillboard(0x1c, 0, ENTITY->angle,
                           &ENTITY->scaMatrixData.localMatrix,
                           &g_playerPosScratch, 0);
    Effect_CreateBillboard(0x1c, 0, (short)(ENTITY->angle + 0x400),
                           &ENTITY->scaMatrixData.localMatrix,
                           &g_playerPosScratch, 0);
}

// ============================================================================
// 0x004badc0 - the SCA collision record, six shorts like the zombie's and the
// dog's: [0] terminator, [1..3] local x/y/z, [4] half-height, [5] radius.
// Only [5] (200) is read from outside SetEntityScaHitData - check_room_collision
// pulls it back out of Entity->Sca_info + 10.
//
// 0x004badcc holds a POINTER to this record, and crow_state_init loads the
// pointer (`MOV EAX,[0x004badcc]`), not the address of the slot.
// ============================================================================
static const short crow_sca_info[6] = {
    (short)0x8000, 0, 0, 0, 180, 200
};

static void crow_state_init(void);      // 0x0042e320
static void crow_state_run(void);       // 0x0042e6c0
static void crow_state_recoil(void);    // 0x0042e9d0
static void crow_state_death(void);     // 0x0042eb30
static void crow_state_scd(void);       // 0x00430270

// ============================================================================
// crow_state_table @ 0x004badd0
// Indexed by Entity->state. Nine entries; index 9 is a NULL pad that separates
// this table from the behaviour jumptable at 0x004badf8.
//
// State 2 and state 3 are the entry points the damage system uses:
// weapon_apply_damage (WeaponDamage.cpp) sets state 3 outright and demotes it to
// state 2 when health survives. The crow also drives itself into state 2 from
// crow_state_run after it rams the player, which is why the recoil handler
// serves both.
// ============================================================================
static void* const crow_state_table[9] = {
    (void*)crow_state_init,     // [0] spawn / respawn
    (void*)crow_state_run,      // [1] main AI driver
    (void*)crow_state_recoil,   // [2] struck, or just rammed the player
    (void*)crow_state_death,    // [3] death
    (void*)crow_state_scd,      // [4] SCD-driven flight
    (void*)crow_state_scd,      // [5]
    (void*)crow_state_scd,      // [6]
    (void*)crow_state_scd,      // [7]
    (void*)crow_state_scd       // [8] the behavior_flags bit 1 spawn lands here
};

// ============================================================================
// Animation helpers
// ============================================================================

// ---------------------------------------------------------------------------
// crow_anim_hold @ 0x0042f970
// Start the current animation, then LOOP it forever - action_state is never
// advanced past 1, so the behaviour has to move itself on.
// ---------------------------------------------------------------------------
static void crow_anim_hold(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state        = 1;
        ENTITY->animation_frame_id  = 0;
        ENTITY->timing_control      = 0;
        ENTITY->death_timer         = 0;
        ENTITY->blend_counter       = 3;
    } else if (ENTITY->action_state != 1) {
        return;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ---------------------------------------------------------------------------
// crow_anim_step @ 0x0042f9f0
// As above, but Joint_move's return is ADDED to action_state, so the sub-state
// reaches 2 on the frame the animation completes. At 2 the crow drops back to
// the perched behaviour (ignore_player_flag = 0, behaviour word = 0).
// ---------------------------------------------------------------------------
static void crow_anim_step(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->action_state        = 1;
        ENTITY->animation_frame_id  = 0;
        ENTITY->timing_control      = 0;
        ENTITY->death_timer         = 0;
        ENTITY->blend_counter       = 3;
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        ENTITY->ignore_player_flag = 0;
        CR_BEH_W = 0;                       // behaviour AND sub-state
        return;
    }
    char adv = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);
}

// ---------------------------------------------------------------------------
// crow_anim_turn @ 0x0042fa90
// The banking-turn animation: loop the flap while rotating toward the player at
// CR_TURN_RATE, for a random 80..142 frames or until the turn completes,
// whichever comes first. On completion it hands control back to state 1 and
// latches the player's position as the waypoint.
// ---------------------------------------------------------------------------
static void crow_anim_turn(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->action_ticks_counter = (unsigned short)(((rand() & 0x1f) * 2) + 0x50);
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        crow_set_state(1, 0, 0, 0);
        ENTITY->player_pos_x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        ENTITY->player_pos_z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        return;
    }

    // The turn delta is parked in the g_animFrameIdSave scratch, then read back
    // twice - as the completion test and as the yaw to apply.
    g_animFrameIdSave =
        (unsigned int)(int)(short)turn_toward_target(CROW_PLAYER_T, CR_TURN_RATE);

    short ticks = (short)ENTITY->action_ticks_counter;
    ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
    if (ticks == 0 || (int)g_animFrameIdSave == 0) {
        ENTITY->action_state = 2;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->angle = (short)(ENTITY->angle + (short)g_animFrameIdSave);
}

// ---------------------------------------------------------------------------
// crow_anim_grab @ 0x0042fbc0
// The crow is latched onto the player, pecking. CR_STRUGGLE starts at 100 and
// loses 1 per frame, or 9 per frame while the player is mashing a button
// (GetPlayerInputMasked) - so button mashing shakes it off about nine times
// faster. It is a SIGNED char and the release test is `< 0`.
//
// The grab also ends if the player walks far enough away (> 1500 units) AND is
// no longer aligned with the crow.
// ---------------------------------------------------------------------------
static void crow_anim_grab(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        CR_STRUGGLE                = 100;
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        // Release: peel off at speed, snap 180 degrees half the time, and climb.
        ENTITY->move_speed_current =
            (unsigned short)((4 - (rand() & 1)) * 0x32 + CR_DIST / 100);
        ENTITY->angle_z = (short)0xff00;
        ENTITY->angle   = (short)(ENTITY->angle + (short)((rand() & 1) * 0x800));
        CR_Y -= 100;
        crow_set_state(1, 1, 6, 0);
        ENTITY->hit_state = 0;
        return;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    if (g_playerEntity.health < 0) {
        crow_set_state(1, 1, 12, 0);
        g_playerEntity.animationId     = 3;
        g_playerEntity.animFrameId     = 0;
        g_playerEntity.action_behavior = 200;
        g_playerEntity.action_state    = 0;
        return;
    }

    short mashed = GetPlayerInputMasked();
    CR_STRUGGLE = (signed char)(CR_STRUGGLE - (((mashed != 0) ? 8 : 0) + 1));

    if (CR_STRUGGLE < 0 && g_playerEntity.action_state == 3) {
        ENTITY->action_state        = 2;
        g_playerEntity.action_state = 4;
        return;
    }
    if (CR_DIST > 0x5dc) {
        if ((short)turn_toward_target(CROW_PLAYER_T, 0x100) != 0) {
            ENTITY->action_state        = 2;
            g_playerEntity.action_state = 4;
            return;
        }
    }

    // A peck lands every 20 player animation frames.
    if (g_playerEntity.animation_frame_id % 0x14 == 0) {
        g_playerPosScratch.x = 100;
        g_playerPosScratch.y = -0x974;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0, 1, 0x200,
                               &g_playerEntity.scaMatrixData.localMatrix,
                               &g_playerPosScratch, 0);
        crow_seed_effect_pos();
        Effect_CreateBillboard(0x1c, 0, ENTITY->angle,
                               &ENTITY->scaMatrixData.localMatrix,
                               &g_playerPosScratch, 0);
        // Flag 0x7B is the difficulty/"normal mode" bit the whole damage system
        // reads; the crow pecks for 4 instead of 3 when it is set.
        g_playerEntity.health =
            (short)(g_playerEntity.health -
                    ((Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) ? 3 : 4));
        Snd_em(3);
    }
}

// ============================================================================
// Behaviour case bodies (the 0x004badf8 jumptable targets)
// ============================================================================

// ---------------------------------------------------------------------------
// Behaviour 0 - perched @ 0x0042fe50 (reached through the JMP thunk at
// 0x0042ee80). Sit still for a random 40..102 frames, then play one of three
// idle animations - 11, or 12, or (a quarter of the time) 2 with a caw - and
// return to state 1 when it finishes.
//
// Cases 0 and 2 fall THROUGH here in the original; that is deliberate, not a
// missing break.
// ---------------------------------------------------------------------------
static void crow_beh_perch(void)
{
    switch (ENTITY->action_state) {
    case 0:
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        ENTITY->action_ticks_counter = (unsigned short)(((rand() & 0x1f) * 2) + 0x28);
        // fall through
    case 1: {
        short ticks = (short)ENTITY->action_ticks_counter;
        ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
        if (ticks == 0 || g_playerEntity.health < 0) {
            ENTITY->action_state = 2;
            return;
        }
        break;
    }
    case 2:
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 3;
        ENTITY->animationId        = 0xb;
        if ((rand() & 1) != 0) {
            ENTITY->animationId = 0xc;
            if ((rand() & 1) != 0) {
                ENTITY->animationId = 2;
                Snd_em(1);
            }
        }
        // fall through
    case 3: {
        char adv = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);
        break;
    }
    case 4:
        crow_set_state(1, 0, 0, 0);
        return;
    }
}

// ---------------------------------------------------------------------------
// Behaviour 1 - wingbeat @ 0x0042ee90
// The post-launch / post-recoil beat. TWO vertical terms that pull opposite
// ways: a flat 30/frame climb (`SUB [ECX+0x38],0x1e`), and, once the animation
// passes frame 8, a SINK of (frame + 52) (`ADD [ECX+0x38],EDX`). So the crow
// rises for the first eight frames and then falls faster than it rose, which is
// what gives the flap its arc.
// Ends in the descending glide (5), or the strike run (7) if it is already low.
// ---------------------------------------------------------------------------
static void crow_beh_wingbeat(void)
{
    ENTITY->animationId = 1;
    ENTITY->move_speed_current = (unsigned short)(((rand() & 1) * 0x10) + 0x32);
    crow_anim_step();
    Add_speedXZ(0);
    CR_Y -= 30;                      // climb
    if (ENTITY->animation_frame_id > 8) {
        CR_VY = (short)(ENTITY->animation_frame_id + 0x34);
        CR_Y += (int)CR_VY;          // ... and sink harder past frame 8
    }
    ENTITY->angle_z = (short)(ENTITY->angle_z + 0x15);

    if (ENTITY->ignore_player_flag == 0 || CR_Y > -1500) {
        ENTITY->angle_z = 0x100;
        crow_set_state(1, 1, 5, 0);
        if (CR_Y > -800) {
            crow_set_state(1, 1, 7, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 2 - dive @ 0x0042ef60
// Nose down and drop toward the player. Speed bleeds off as the animation
// advances (200 - 6 per frame) and the crow stops thrusting once it goes
// negative. Descends by CR_CEIL_LIMIT/40 per frame - the bound behaviour 5
// rolled for this dive.
// ---------------------------------------------------------------------------
static void crow_beh_dive(void)
{
    ENTITY->animationId = 4;
    crow_anim_step();
    ENTITY->move_speed_current =
        (unsigned short)(200 - (unsigned short)ENTITY->animation_frame_id * 6);
    if ((short)ENTITY->move_speed_current > 0) {
        Add_speedXZ(0);
    }
    CR_Y -= (int)(short)(CR_CEIL_LIMIT / 0x28);
    ENTITY->angle_z = (short)(ENTITY->angle_z - 6);

    if (ENTITY->ignore_player_flag == 0 || CR_Y > -450) {
        ENTITY->move_speed_current = 0;
        ENTITY->angle_z            = 0;
        CR_Y                       = -450;
        crow_set_state(1, 1, 3, 0);
        // g_entity_bkp is the wider (0x200) turn-toward-target result cached at
        // the top of the dispatcher: zero means the crow is already lined up.
        if (g_entity_bkp == 0) {
            CR_BEH_W = 4;
        }
        if (g_playerEntity.health < 0) {
            CR_BEH_W = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 3 - banking turn @ 0x0042f050
// Circle back around for another pass at a random turn rate of 0x40, 0x60,
// 0x80 or 0xA0 per frame.
// ---------------------------------------------------------------------------
static void crow_beh_bank_turn(void)
{
    ENTITY->animationId = 3;
    int r1 = rand();
    int r2 = rand();
    CR_TURN_RATE = (short)(((r1 & 1) * 0x20) - ((r2 & 1) * 0x40) + 0x80);
    crow_anim_turn();

    if (ENTITY->ignore_player_flag == 0) {
        crow_set_state(1, 1, 3, 0);
        if (g_entity_bkp == 0) {
            CR_BEH_W = 4;
        }
        if (g_playerEntity.health < 0) {
            CR_BEH_W = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 4 - strike run @ 0x0042f0e0
// The committed attack run. Past animation frame 8 the crow computes the climb
// velocity it will need on the way out (see the note on the divide) and hands
// over to the low hover with a caw.
// ---------------------------------------------------------------------------
static void crow_beh_strike_run(void)
{
    ENTITY->animationId = 7;
    crow_anim_step();
    ENTITY->angle_z = (short)(ENTITY->angle_z - 0x19);

    if (ENTITY->animation_frame_id > 8) {
        unsigned short dist = CR_DIST;
        // 2600 * (dist/100 + 200) / dist. The inner divide is a 16-bit UNSIGNED
        // DIV, the outer one a 32-bit signed IDIV. The crow can only reach this
        // with the player at a real distance, but a zero would fault the CPU,
        // so the port skips the frame instead.
        if (dist != 0) {
            int scaled = ((int)(unsigned int)(unsigned short)(dist / 100) + 200) * 2600;
            CR_VY = (short)(scaled / (int)(unsigned int)dist);
            if (CR_VY > 500) {
                CR_VY = 200;
            }
            ENTITY->move_speed_current = (unsigned short)((dist / 100) + 200);
            ENTITY->angle_z = (short)0xff00;
            crow_set_state(1, 1, 6, 0);
            Snd_em(4);
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 5 - descending glide @ 0x0042f1c0
// Sink toward the player. Y is negative up and this ADDS the vertical velocity
// (`ADD [ECX+0x38],EDX` at 0x0042f26b), so the crow loses height every frame
// while the velocity itself ramps 1/frame up to 500 and then snaps back to 200.
//
// Steering is scaled by bit 0 of the pathfind result, so the crow only corrects
// course on frames where the path is clear. It rolls a fresh floor bound every
// frame; once it drops past that, or simply gets under -1500, it commits to the
// strike run (7) or - if it is lined up and not the scripted variant - a full
// dive (2). Passing head height while aligned turns it into a peck (9).
// ---------------------------------------------------------------------------
static void crow_beh_descend(void)
{
    ENTITY->animationId = 0;
    short turn = (short)turn_toward_target(CROW_PLAYER_T, 0x10);
    ENTITY->angle = (short)(ENTITY->angle + (short)(turn * (short)(CR_PATH_W & 1) * 2));

    if (CR_DIST < 2000 && ENTITY->animation_frame_id % 9 == 0) {
        Snd_em(5);
    }
    crow_anim_hold();
    Add_speedXZ(0);

    CR_VY = (short)(CR_VY + 1);
    if (CR_VY > 500) {
        CR_VY = 200;
    }
    CR_Y += (int)CR_VY;              // negative-up: this SINKS

    CR_CEIL_LIMIT = (short)(((((rand() & 1) * -500) + CR_ALT_BIAS) * 2) - 1500);

    if ((int)CR_CEIL_LIMIT < CR_Y || CR_Y > -1500) {
        crow_set_state(1, 1, 7, 0);
        if (g_entity_bkp != 0 && (ENTITY->behavior_flags & 1) == 0) {
            crow_set_state(1, 1, 2, 0);
        }
    }
    // Close enough, lined up, and at head height -> peck.
    if (CR_DIST < 0x4b0) {
        if ((short)turn_toward_target(CROW_PLAYER_T, 0x100) == 0
            && (unsigned int)(-2500 - CR_Y) < 300) {
            crow_set_state(1, 1, 9, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 6 - climb out @ 0x0042f330
// The mirror of behaviour 5: identical steering and caw, but it SUBTRACTS the
// vertical velocity (`SUB [ECX+0x38],EDX` at 0x0042f3db), so the crow gains
// height. It rolls a ceiling bound of roughly -4500 or -5500 each frame and
// hands over to the level-out (10) once it climbs past it.
// ---------------------------------------------------------------------------
static void crow_beh_ascend(void)
{
    ENTITY->animationId = 5;
    short turn = (short)turn_toward_target(CROW_PLAYER_T, 0x10);
    ENTITY->angle = (short)(ENTITY->angle + (short)(turn * (short)(CR_PATH_W & 1) * 2));

    if (CR_DIST < 2000 && ENTITY->animation_frame_id % 9 == 0) {
        Snd_em(5);
    }
    crow_anim_hold();
    Add_speedXZ(0);

    CR_VY = (short)(CR_VY + 1);
    if (CR_VY > 500) {
        CR_VY = 200;
    }
    CR_Y -= (int)CR_VY;              // negative-up: this CLIMBS

    CR_FLOOR_LIMIT = (short)(((rand() & 1) * -1000) + CR_ALT_BIAS - 0x1194);

    if (CR_Y < (int)CR_FLOOR_LIMIT) {
        crow_set_state(1, 1, 10, 0);
    }
    if (CR_DIST < 0x4b0) {
        if ((short)turn_toward_target(CROW_PLAYER_T, 0x100) == 0
            && (unsigned int)(-2500 - CR_Y) < 300) {
            crow_set_state(1, 1, 9, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 7 - dive attack @ 0x0042f470
// Speed is set from the distance so the crow arrives fast from far away. The
// run ends after 18 animation frames, or the instant the player is hit; landing
// the hit while the player is in reaction 4 also knocks the crow back and spins
// it half the time. The behavior_flags bit 0 variant recomputes its exit climb
// velocity - the same 2600 * (dist/100 + 200) / dist as behaviour 4.
// ---------------------------------------------------------------------------
static void crow_beh_dive_attack(void)
{
    ENTITY->animationId = 7;
    crow_anim_step();
    ENTITY->move_speed_current =
        (unsigned short)((short)((4 - (rand() & 1)) * 0x32) + (short)(CR_DIST / 100));
    Add_speedXZ(0);
    ENTITY->angle_z = (short)(ENTITY->angle_z - 0x19);

    if (ENTITY->animation_frame_id > 0x12
        || (g_playerEntity.isBeingAttackedFlag & 1) != 0) {

        if ((g_playerEntity.isBeingAttackedFlag & 1) != 0
            && g_playerEntity.action_state == 4) {
            CR_Y -= 50;
            ENTITY->angle = (short)(ENTITY->angle + (short)((rand() & 1) * 0x800));
        }
        ENTITY->angle_z = (short)0xff00;
        crow_set_state(1, 1, 6, 0);

        if ((ENTITY->behavior_flags & 1) != 0) {
            unsigned short dist = CR_DIST;
            if (dist != 0) {
                int scaled =
                    (((int)(unsigned int)(unsigned short)(dist / 100) * 0x145) + 65000) * 8;
                CR_VY = (short)(scaled / (int)(unsigned int)dist);
                if (CR_VY > 500) {
                    CR_VY = 200;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 8 - approach @ 0x0042f610
// A short turn-and-close before the climb-out takes over.
// ---------------------------------------------------------------------------
static void crow_beh_approach(void)
{
    ENTITY->animationId = 5;
    entity_rotate_toward_target(CROW_PLAYER_T, 0x40);
    crow_anim_hold();
    Add_speedXZ(0);
    if (ENTITY->animation_frame_id > 8) {
        crow_set_state(1, 1, 6, 0);
    }
}

// ---------------------------------------------------------------------------
// Behaviour 9 - peck @ 0x0042f660
// Hover at head height and jab. This is the case Ghidra rendered as the
// dispatcher's trailing "default" block, because 0x0042f660 had been created as
// its own function; it is case 9, and nothing else reaches it.
//
// Being hit by the player (reaction > 3) throws the crow into the low hover.
// Otherwise, once the flap animation ends and the crow is lined up and its path
// is clear, it bites for 6 (or 16 on the harder setting) and latches on -
// behaviour 11.
// ---------------------------------------------------------------------------
static void crow_beh_peck(void)
{
    ENTITY->animationId = 8;
    entity_rotate_toward_target(CROW_PLAYER_T, 0x30);
    crow_anim_step();

    if ((g_playerEntity.isBeingAttackedFlag & 1) != 0
        && g_playerEntity.action_state > 3) {
        crow_set_state(1, 1, 6, 0);
        ENTITY->move_speed_current =
            (unsigned short)((short)((4 - (rand() & 1)) * 0x32) + (short)(CR_DIST / 100));
        ENTITY->angle_z = (short)0xff00;
        ENTITY->angle   = (short)(ENTITY->angle + (short)((rand() & 1) * 0x800));
        CR_Y -= 100;
    }

    if (ENTITY->action_state == 0) {
        ENTITY->angle_z = 0x100;
        crow_set_state(1, 1, 7, 0);

        crow_seed_effect_pos();
        Effect_CreateBillboard(0x1c, 0, ENTITY->angle,
                               &ENTITY->scaMatrixData.localMatrix,
                               &g_playerPosScratch, 0);

        if (CR_DIST < 0x5dc) {
            if ((short)turn_toward_target(CROW_PLAYER_T, 0x100) == 0
                && (CR_PATH_B & 1) != 0) {
                // The behaviour word is set BEFORE the hit test in the original
                // (it is folded into the && chain), so a crow that reaches here
                // while the player is already being attacked still parks on 9.
                CR_BEH_W = 9;
                if (g_playerEntity.isBeingAttackedFlag == 0) {
                    g_playerEntity.health =
                        (short)(g_playerEntity.health -
                                ((Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) ? 6 : 16));
                    g_playerEntity.isBeingAttackedFlag = 1;
                    g_playerEntity.animationId     = 6;
                    g_playerEntity.animFrameId     = 5;
                    g_playerEntity.action_behavior = 0;
                    g_playerEntity.action_state    = 0;
                    // `PUSH 0xbe6318` - the player's TRANSLATION VECTOR
                    // (player + 0x34), not the entity base at 0xbe62e4.
                    Play3DSnd(3, 0, 0,
                              (int)(intptr_t)&g_playerEntity.scaMatrixData.localMatrix.t[0]);

                    crow_seed_effect_pos();
                    g_playerPosScratch.y = -0x9d8;
                    Effect_CreateBillboard(0, 1, g_playerEntity.directionAngle,
                                           &g_playerEntity.scaMatrixData.localMatrix,
                                           &g_playerPosScratch, 0);
                    ENTITY->hit_state = 1;
                    ENTITY->angle_z   = 0;
                    CR_BEH_W = 0xb;         // latch on
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Behaviour 10 - level out @ 0x0042f5a0
// Entered when the crow tops out against the ceiling clamp in crow_update, and
// from the climb-out's ceiling bound. Climbs 25/frame but sheds 50 while the
// nose is up - and it pitches the nose up 25/frame - so after the first frame
// or two the net is a 25/frame descent. One animation cycle later crow_anim_step
// clears ignore_player_flag and it hands back to the descending glide.
// ---------------------------------------------------------------------------
static void crow_beh_level_out(void)
{
    ENTITY->animationId = 5;
    crow_anim_step();
    CR_Y -= 25;
    if (ENTITY->angle_z > 0) {
        CR_Y += 50;
    }
    Add_speedXZ(0);
    ENTITY->angle_z = (short)(ENTITY->angle_z + 0x19);

    if (ENTITY->ignore_player_flag == 0) {
        ENTITY->angle_z = 0x100;
        crow_set_state(1, 1, 5, 0);
    }
}

// ---------------------------------------------------------------------------
// Behaviour 11 - latched on @ 0x0042f8a0
// Ride the player while crow_anim_grab runs the struggle timer. Add_speedXZ
// gets a random 0x400 or 0xC00 offset so the crow flutters around the player
// rather than tracking a fixed side.
// ---------------------------------------------------------------------------
static void crow_beh_grab(void)
{
    ENTITY->animationId = 8;
    entity_rotate_toward_target(CROW_PLAYER_T, 0x80);
    ENTITY->move_speed_current = 0x32;
    crow_anim_grab();
    Add_speedXZ((int)(((unsigned int)rand() & 1) * 0x800 + 0x400));
}

// ---------------------------------------------------------------------------
// Behaviour 12 - victory circle @ 0x0042f8f0
// Entered from the grab the moment the player dies. Drops back down toward the
// body (`ADD [ECX+0x38],EDX`) until it is under -3000, then resumes diving.
// ---------------------------------------------------------------------------
static void crow_beh_victory_descend(void)
{
    ENTITY->animationId = 0;
    entity_rotate_toward_target(CROW_PLAYER_T, 0x80);
    crow_anim_hold();
    ENTITY->move_speed_current = 200;
    Add_speedXZ(0);
    CR_VY = (short)(CR_VY + 1);
    CR_Y += (int)CR_VY;
    if (CR_Y > -3000) {
        crow_set_state(1, 1, 2, 0);
    }
}

// ---------------------------------------------------------------------------
// Behaviour 13 - take off @ 0x0042ffd0 (thunk at 0x0042f960)
// The perched crow launching. Waits a random 0, 3, 8 or 11 frames - staggering
// a whole flock so they do not all leave the ground on the same frame - raises
// behavior_flags bit 4 to mark itself airborne, then caws, throws feathers and
// enters the climb.
// ---------------------------------------------------------------------------
static void crow_beh_takeoff(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
        ENTITY->action_state       = 1;
        int r1 = rand();
        int r2 = rand();
        ENTITY->action_ticks_counter =
            (unsigned short)(((r2 & 1) * 3) + ((r1 & 1) * 8));
        ENTITY->ignore_player_flag = 1;
        ENTITY->behavior_flags |= 0x10;
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        crow_set_state(1, 1, 1, 0);
        Snd_em(4);
        crow_spawn_feathers();
        return;
    }

    short ticks = (short)ENTITY->action_ticks_counter;
    ENTITY->action_ticks_counter = (unsigned short)(ticks - 1);
    if (ticks == 0) {
        ENTITY->action_state = 2;
    }
}

// ============================================================================
// crow_behavior_dispatch @ 0x0042edf0
// Runs the pathfinder, caches two turn-toward-target results in the shared
// scratch, then jumps to the current behaviour.
//
// The two cached turns are read back by the behaviours: g_entity_bkp (the wide
// 0x200 step) is the "already lined up" test that behaviours 2, 3 and 5 branch
// on. g_tempVar takes the 0x100 step and is never read by the crow - the store
// is real, so it stays.
// ============================================================================
static void crow_behavior_dispatch(void)
{
    unsigned char path = (unsigned char)entity_pathfind_update();
    g_animFrameIdSave = (unsigned int)path;
    if ((path & 0xfe) == 0) {
        CR_PATH_B &= 0xfe;
        CR_PATH_W = (unsigned short)(CR_PATH_W | ((unsigned short)g_animFrameIdSave & 1));
    }

    g_tempVar    = (void*)(int)(short)turn_toward_target(CROW_PLAYER_T, 0x100);
    g_entity_bkp = (unsigned int)(int)(short)turn_toward_target(CROW_PLAYER_T, 0x200);

    // The original is a bare `JMP [ECX*4 + 0x4badf8]` with NO range check - the
    // compiler proved action_behavior stays inside 0..13. Anything outside that
    // would be a wild jump there; here it is simply ignored.
    switch (ENTITY->action_behavior) {
    case 0:  crow_beh_perch();          break;
    case 1:  crow_beh_wingbeat();       break;
    case 2:  crow_beh_dive();           break;
    case 3:  crow_beh_bank_turn();      break;
    case 4:  crow_beh_strike_run();     break;
    case 5:  crow_beh_descend();        break;
    case 6:  crow_beh_ascend();         break;
    case 7:  crow_beh_dive_attack();    break;
    case 8:  crow_beh_approach();       break;
    case 9:  crow_beh_peck();           break;
    case 10: crow_beh_level_out();      break;
    case 11: crow_beh_grab();           break;
    case 12: crow_beh_victory_descend();break;
    case 13: crow_beh_takeoff();        break;
    default: break;
    }
}

// ============================================================================
// crow_state_init @ 0x0042e320  (state 0)
// Spawn. behavior_flags decides which of three ways the crow enters the room:
//   bit 4 set  - already airborne: start in the high hover with a random cruise
//                speed and the nose up.
//   bit 1 set  - state 8, i.e. under SCD control.
//   otherwise  - perched, waiting for the take-off cue in crow_state_run.
//
// bit 0 marks the scripted variant: no health at all (it cannot be killed, it
// is set-dressing) and a -400 altitude bias that makes it fly lower.
// ============================================================================
static void crow_state_init(void)
{
    crow_set_state(1, 0, 0, 0);
    ENTITY->scaMatrixData.field_00 = 0;
    CR_UNK_C1                      = 0;
    ENTITY->action_ticks_counter   = 0;
    ENTITY->animationId            = 2;
    ENTITY->death_timer            = 0;
    ENTITY->hit_state              = 0;
    // Bit 2 tells check_room_collision to report floor/step zones instead of
    // pushing out of them - the crow flies over the geometry and only wants the
    // height, which comes back in g_animFrameIdSave.
    ENTITY->collisionFlags |= 4;

    if ((ENTITY->behavior_flags & 0x10) != 0) {
        CR_VY = 200;
        ENTITY->move_speed_current = (unsigned short)(((rand() & 1) * 0x10) + 0x32);
        ENTITY->angle_z            = 0x100;
        crow_set_state(1, 1, 5, 0);
        ENTITY->animationId = 0;
    }
    if ((ENTITY->behavior_flags & 2) != 0) {
        crow_set_state(8, 0, 0, 0);
        ENTITY->animationId = 0;
    }

    CR_STUCK        = 0;
    CR_PHASE        = 100;
    CR_SWERVE_LATCH = 0;

    ResetJointTransforms();
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);

    // The ground shadow. g_animFrameIdSave carries the tint the quad builder
    // copies into its header - 0x808080, i.e. half brightness.
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_animFrameIdSave = 0x808080;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 200, 200);

    // FOUR rand() calls; the first result is thrown away. Health is
    // 10 + 2*((r&7) + (r&7) + (r&7)), i.e. 10..52.
    rand();
    unsigned short ra = (unsigned short)rand();
    unsigned short rb = (unsigned short)rand();
    unsigned short rc = (unsigned short)rand();
    ENTITY->health = (short)(((rb & 7) * 2) + ((ra & 7) * 2) + ((rc & 7) * 2) + 10);

    CR_ALT_BIAS = 0;
    if ((ENTITY->behavior_flags & 1) != 0) {
        ENTITY->health = 0;
        CR_ALT_BIAS    = -400;
    }

    // 0x004badcc holds a pointer to the record, so this loads 0x004badc0.
    ENTITY->Sca_info = (unsigned int)(uintptr_t)crow_sca_info;
    *(unsigned short*)((char*)ENTITY + 0xCA) = 0x14cc;
}

// ============================================================================
// crow_state_run @ 0x0042e6c0  (state 1)
// The driver. Three things happen here before the behaviour runs:
//
//  1. Contact resolution. If the crow is touching the player and the player is
//     not already reacting, it either latches on (the behavior_flags bit 0
//     variant, behaviour 9) or - if it is moving fast enough and the player is
//     facing away - rams them, which is what state 2 handles.
//  2. Obstacle steering. While check_room_collision keeps reporting a hit the
//     stuck counter climbs; past 30 frames the crow either rams or asks
//     entity_swerve_around_obstacle for a way around.
//  3. Distance. CR_DIST is the manhattan |dx| + |dz| to the player, recomputed
//     every frame and read by half the behaviours.
//
// The trailing block re-derives the alert/visual status bits from the crow's
// ALTITUDE, so a crow near the ceiling is invisible to the rest of the game.
// ============================================================================
static void crow_state_run(void)
{
    unsigned char bflags = ENTITY->behavior_flags;
    if ((bflags & 0x80) != 0) {
        return;
    }

    if ((bflags & 0x10) != 0) {
        if (CR_TOUCH != 0 && g_playerEntity.isBeingAttackedFlag == 0
            && ENTITY->action_behavior != 9) {

            if ((bflags & 1) != 0) {
                // Unsigned compare: only pull the crow down to head height when
                // it is more than 300 units above it.
                if ((unsigned int)(-2500 - CR_Y) > 300) {
                    CR_Y = -2500;
                }
                crow_set_state(1, 1, 9, 0);
                return;
            }
            if ((short)ENTITY->move_speed_current > 300
                && (char)is_facing_toward_entity(&g_playerEntity) == 0) {
                crow_set_state(2, 0, 0, 0);
                g_playerEntity.isBeingAttackedFlag = 1;
                g_playerEntity.action_behavior     = 100;
                return;
            }
        }

        bool reacting = false;
        if (CR_COLL == 0) {
            reacting = (g_playerEntity.isBeingAttackedFlag != 0);
        } else {
            CR_STUCK = (unsigned short)(CR_STUCK + 1);
            if (CR_STUCK > 5 && (ENTITY->behavior_flags & 1) != 0) {
                ENTITY->angle = (short)(ENTITY->angle
                                        + (short)turn_toward_target(CROW_PLAYER_T, 0x40));
            }
            if (CR_STUCK <= 0x1e) {
                reacting = (g_playerEntity.isBeingAttackedFlag != 0);
            } else if (g_playerEntity.isBeingAttackedFlag != 0) {
                reacting = true;
            } else {
                CR_STUCK = 0;
                if ((short)ENTITY->move_speed_current > 300
                    && g_playerEntity.isBeingAttackedFlag == 0
                    && (ENTITY->behavior_flags & 1) == 0) {
                    crow_set_state(2, 0, 0, 0);
                    return;
                }
                g_playerPosScratch.x = g_playerEntity.scaMatrixData.localMatrix.t[0];
                g_playerPosScratch.y = g_playerEntity.scaMatrixData.localMatrix.t[1];
                g_playerPosScratch.z = g_playerEntity.scaMatrixData.localMatrix.t[2];
                // The swerve length is handed over in g_animFrameIdSave: four
                // frames of held turn.
                g_animFrameIdSave = 4;
                ENTITY->angle = (short)(ENTITY->angle
                    + entity_swerve_around_obstacle(0x40, (char)CR_COLL,
                                                    &CR_SWERVE, &CR_SWERVE_LATCH));
                reacting = (g_playerEntity.isBeingAttackedFlag != 0);
            }
        }

        if (reacting) {
            ENTITY->angle = (short)(ENTITY->angle
                                    + (short)turn_toward_target(CROW_PLAYER_T, 0x40));
            if (g_playerEntity.action_state > 3) {
                // `CMP AX,1 / SBB / NEG` - a one-unit nudge on the frames the
                // crow is already perfectly lined up, so it never sits exactly
                // on the boundary.
                ENTITY->angle = (short)(ENTITY->angle
                    + ((short)turn_toward_target(CROW_PLAYER_T, 0x80) == 0 ? 1 : 0));
            }
            if (g_playerEntity.health < 0 && g_playerEntity.action_behavior == 200
                && ENTITY->action_behavior == 9) {
                crow_set_state(1, 1, 6, 0);
            }
        }
    }

    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2]
             - ENTITY->scaMatrixData.localMatrix.t[2];
    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0]
             - ENTITY->scaMatrixData.localMatrix.t[0];
    CR_DIST = (unsigned short)((unsigned short)(dz < 0 ? -dz : dz)
                             + (unsigned short)(dx < 0 ? -dx : dx));

    bool dispatch = true;
    if (ENTITY->ignore_player_flag == 0) {
        // The player's whole state dword at +0x84: animationId 1, animFrameId 3,
        // action_behavior 0x14, action_state 0 or 3. That is the cue a perched
        // crow watches for; it scatters with a random half-turn.
        unsigned int playerState = *(unsigned int*)((char*)&g_playerEntity + 0x84);
        if ((playerState == 0x140301 || playerState == 0x3140301)
            && (ENTITY->behavior_flags & 0x10) == 0) {
            ENTITY->angle = (short)(ENTITY->angle + (short)((rand() & 1) * 0x40));
            crow_set_state(1, 1, 13, 0);
        }
    } else if (ENTITY->ignore_player_flag != 1) {
        dispatch = false;
    }
    if (dispatch) {
        crow_behavior_dispatch();
    }

    // Altitude gates the crow's presence: it is only "aligned" below -4500, only
    // visible at 2000 units below -2000, and only visible at 5000 below -1500.
    ENTITY->status_flags &= 0x1f;
    entity_check_alert_range(5000);
    if (CR_Y > -4500) {
        ENTITY->status_flags &= 0x1f;
        ENTITY->status_flags |= 0x40;
        if (CR_Y > -2000) {
            entity_check_visual_range(2000);
        }
    }
    if (CR_Y > -1500) {
        ENTITY->status_flags &= 0x1f;
        entity_check_visual_range(5000);
    }
}

// ============================================================================
// crow_state_recoil @ 0x0042e9d0  (state 2)
// Two entries, one handler: the damage system parks a surviving crow here, and
// crow_state_run does the same after the crow rams the player. Either way it
// beats hard upward (200/frame, plus a thrust burst while touching), throws
// feathers on the first animation frame, and once it clears -450 plays the
// recovery flap and rejoins the AI in the banking turn.
// ============================================================================
static void crow_state_recoil(void)
{
    if (ENTITY->action_behavior == 0) {
        ENTITY->animationId        = 6;
        crow_anim_hold();
        ENTITY->move_speed_current = 200;
        CR_Y += 200;
        if (CR_TOUCH != 0) {
            Add_speedXZ(0x800);
        }
        if (ENTITY->animation_frame_id == 1) {
            Snd_em(2);
            crow_spawn_feathers();
        }
        if (CR_Y > -450) {
            CR_BEH_W = 1;               // behaviour 1, sub-state 0
            CR_Y     = -450;
        }
    } else if (ENTITY->action_behavior == 1) {
        ENTITY->animationId = 9;
        crow_anim_step();
        if (ENTITY->action_state == 0) {
            crow_set_state(1, 1, 3, 0);
            ENTITY->hit_state = 0;
            ENTITY->status_flags &= 0x1f;
            entity_check_visual_range(5000);
        }
    }
}

// ============================================================================
// crow_state_death @ 0x0042eb30  (state 3)
// The crow drops. Sub-state 0 rides it down; which exit it takes depends on the
// floor step check_room_collision reported:
//
//   step != 100 - it hit real floor: fade the shadow to yellow, shrink it, and
//                 play the landing animation (sub-state 1).
//   step == 100 - it went off the map (the guard at the top rewrites anything
//                 below -2000 to 100): skip straight to the burst (sub-state 2),
//                 which pops an effect on every joint and removes the entity.
//
// Sub-state 2 falls THROUGH into 4 in the original - the jumptable at 0x0042edd4
// sends case 3 to the bare RET, and case 4 is the two status_flags ORs that mark
// the entity dead (bit 1) and free for removal (bit 3).
// ============================================================================
static void crow_state_death(void)
{
    if (CR_FLOOR_STEP < -2000) {
        CR_FLOOR_STEP = 100;
    }

    switch (ENTITY->action_behavior) {
    case 0:
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        ENTITY->animationId = 6;
        // hit_state bit 0 means the killing blow already knocked it sideways;
        // in that case it just tumbles instead of flapping.
        if ((ENTITY->hit_state & 1) == 0) {
            ENTITY->move_speed_current = 200;
            short turn = (short)turn_toward_target(CROW_PLAYER_T, 0x400);
            Add_speedXZ((turn == 0) ? 0x800 : 0);
        }
        crow_anim_hold();
        CR_Y += 200;
        ENTITY->angle_z = (short)(ENTITY->angle_z + 0x80);

        if (ENTITY->animation_frame_id == 1) {
            Snd_em(0);
            crow_spawn_feathers();
        }
        if (CR_Y > -450) {
            if (CR_FLOOR_STEP != 100) {
                CR_BEH_W = 1;
                CR_Y     = -450;
                ENTITY->angle_z = 0;
                BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0xffff50);
                BillboardAdjSize(&ENTITY->pushVelocity, -100, -100);
            } else {
                CR_BEH_W = 2;
            }
        }
        break;

    case 1:
        ENTITY->animationId = 0xa;
        crow_anim_step();
        BillboardAdjSize(&ENTITY->pushVelocity, 6, 6);
        if (ENTITY->action_state == 0) {
            CR_BEH_W = 4;
        }
        break;

    case 2: {
        JointStruct* joints = ENTITY->jointsStructs;
        BillboardSetSize(&ENTITY->pushVelocity, 0, 0);
        // `while (n-- != 0)` over the joints, high index first, with the counter
        // living in the g_animFrameIdSave scratch rather than a register.
        g_animFrameIdSave = (unsigned int)ENTITY->jointCount;
        while (g_animFrameIdSave-- != 0) {
            joint_setup_attack_effect((int)(intptr_t)&joints[g_animFrameIdSave],
                                      0x1e, 0x1e, 3);
        }
        Snd_em(0);
        CR_BEH_W = 4;
    }
        // fall through - case 3 is the bare RET, case 4 is the two ORs below
    case 4:
        ENTITY->status_flags |= 2;
        ENTITY->status_flags |= 8;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// crow_scd_fly_to_target @ 0x004302c0
// The SCD flight leg: cruise toward the point in +0xC6/+0xC8, accelerating from
// 200 up to 400 and then resetting, climbing while below -1500, until the crow
// is within 150 units of it.
// ---------------------------------------------------------------------------
static void crow_scd_fly_to_target(void)
{
    char st = (char)ENTITY->action_state;
    if (st == 0) {
        ENTITY->animationId        = 5;
        ENTITY->move_speed_current = 200;
        ENTITY->action_state       = (unsigned char)(ENTITY->action_state + 1);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->blend_counter      = 3;
        Snd_em(4);
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        CR_BEH_W = 0;
        return;
    }

    if (ENTITY->animation_frame_id % 9 == 0) {
        if ((rand() & 1) != 0 && (rand() & 1) != 0) {
            Snd_em(5);
        }
    }

    g_playerPosScratch.x = (int)CR_HOME_X;
    g_playerPosScratch.z = (int)CR_HOME_Z;
    g_playerPosScratch.y = 0;
    entity_rotate_toward_target(&g_playerPosScratch, 0x40);

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200);

    ENTITY->move_speed_current = (unsigned short)(ENTITY->move_speed_current + 1);
    if ((short)ENTITY->move_speed_current > 400) {
        ENTITY->move_speed_current = 200;
    }
    Add_speedXZ(0);
    if (CR_Y > -1500) {
        CR_Y -= 500;
    }

    int dz = ENTITY->scaMatrixData.localMatrix.t[2] - (int)CR_HOME_Z;
    int dx = ENTITY->scaMatrixData.localMatrix.t[0] - (int)CR_HOME_X;
    if (SquareRoot0(dz * dz + dx * dx) < 0x96) {
        ENTITY->action_state = (unsigned char)(ENTITY->action_state + 1);
    }
}

// ============================================================================
// crow_state_scd @ 0x00430270  (states 4 through 8)
// All five slots share one handler. Behaviour 1 is the "stand down" command: it
// resets to state 0 and rewrites behavior_flags to 0x11, so the next init puts
// the crow back in the air under normal AI.
// ============================================================================
static void crow_state_scd(void)
{
    if ((ENTITY->behavior_flags & 0x80) != 0) {
        return;
    }
    unsigned char beh = ENTITY->action_behavior;
    if (beh == 0 || beh == 2) {
        crow_scd_fly_to_target();
        ENTITY->status_flags &= 0x1f;
        return;
    }
    if (beh == 1) {
        crow_set_state(0, 0, 0, 0);
        ENTITY->behavior_flags = 0x11;
    }
    ENTITY->status_flags &= 0x1f;
}

// ============================================================================
// crow_update @ 0x0042e520
// Per-frame entry, enemies_update_functions_tbl[5].
//
// The whole AI half is gated on g_message_flags bit 2 - the flag that is clear
// while a message box or menu is up, so a crow freezes mid-flight rather than
// carrying on behind the text.
//
// The altitude clamps are the crow's flight envelope, and they are the reason
// this function looks like it double-handles state: dropping below the floor
// (Y > 0) or punching through the ceiling (Y < -30000) rewrites the state block
// directly rather than letting the behaviour notice.
// ============================================================================
void crow_update(void)
{
    if ((g_message_flags & 4) != 0) {
        ((void (*)(void))crow_state_table[ENTITY->state])();

        SetEntityScaHitData(ENTITY);
        CR_TOUCH = (short)(unsigned char)ResolveEntityScaCollision(
            (Entity*)&g_playerEntity, ENTITY);
        HandleEnemyPlayerCollisions();
        CR_COLL = (unsigned short)check_room_collision(
            (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
            *(short*)((char*)(uintptr_t)ENTITY->Sca_info + 10));
        // check_room_collision reports the floor/step height it crossed through
        // g_animFrameIdSave; the crow keeps it as its ground reference.
        CR_FLOOR_STEP = (short)g_animFrameIdSave;
    }

    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0], g_CurrentRdtDataTypePtr);

    if (CR_Y > 0) {
        CR_VY = 200;
        CR_Y  = -100;
        crow_set_state(1, 1, 7, 0);
    }
    if (CR_Y < -30000) {
        CR_VY = 200;
        CR_Y  = -29000;
        crow_set_state(1, 1, 10, 0);
    }

    // The ground shadow shrinks with altitude: (Y >> 4) + 400, floored at 50.
    // Y is negative up, so the arithmetic shift is what makes it shrink.
    g_collPushDepthZHi = (CR_Y >> 4) + 400;
    if (g_collPushDepthZHi < 0) {
        g_collPushDepthZHi = 50;
    }
    BillboardSetSize(&ENTITY->pushVelocity, (short)g_collPushDepthZHi,
                     (short)g_collPushDepthZHi);

    // CR_STATE_W is state and ignore_player_flag read as ONE word, so `!= 1`
    // means "not (perched and alive)" - a perched crow casts no shadow.
    if (ENTITY->has_enter_switch_zone != 0 && CR_FLOOR_STEP > -100
        && CR_STATE_W != 1) {
        entity_add_fade_sprite((VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
                               (short*)&ENTITY->pushVelocity, 0, ENTITY->angle);
    }
}
