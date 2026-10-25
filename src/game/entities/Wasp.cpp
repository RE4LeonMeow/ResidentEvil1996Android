// Wasp.cpp - Wasp (entity type 7, enemy/em1007.emd)
//
// Original PC addresses:
//   wasp_update                0x0048daf0   per-frame entry, dispatch table [7]
//   wasp_sca_info              0x004d3c38   SCA collision record
//   wasp_sca_info_alt          0x004d3c48   second record, unreachable (see below)
//   wasp_sca_info_table        0x004d3c58   pointer table -> the two records
//   wasp_state_table           0x004d3c60   FOUR entries (a real CALL table)
//   wasp_behavior_jumptable    0x004d3c70   SEVEN entries (a compiler switch)
//
// The id is 7 and the model is em1007.emd: g_emdPathTable index (id + 4) = 11
// is "enemy/em1007.emd", the wasp.
//
// ---------------------------------------------------------------------------
// THREE tables in one 0x38-byte block at 0x004d3c58
// ---------------------------------------------------------------------------
// Same trap as the adder's 0x004c3648 block. A naive read of 0x004d3c60 sees
// eleven function pointers followed by a NULL and calls it an eleven-state
// table. It is not. Counting the READERS instead of the NULLs:
//
//   0x0048db0a  CALL dword ptr [ECX*4 + 0x4d3c60]   ECX = Entity+0x84 state
//   0x0048dedb  JMP  dword ptr [ECX*4 + 0x4d3c70]   ECX = Entity+0x86 behaviour
//   0x0048f12b  CALL dword ptr [EAX*4 + 0x4d3c90]   enemies_update_functions_tbl
//
// so the state table is only FOUR entries long (0x004d3c60..0x004d3c6c) and the
// "states 4-10" are really the seven case bodies of wasp_behavior_dispatch's
// switch, whose jumptable starts at 0x004d3c70. The NULL at 0x004d3c8c is the
// pad between that jumptable and enemies_update_functions_tbl, which starts at
// 0x004d3c90 - the two blocks are ADJACENT. There is no bounds check before the
// JMP; the compiler proved the behaviour is 0-6.
//
// wasp_behavior_dispatch (0x0048de60) is only the switch header - the seven case
// bodies live past its RET-less `JMP [table]`, each with its own epilogue, so
// Ghidra shows the whole thing as ONE function. Case 5 (0x0048e6b0) is a bare
// `JMP 0x0048e810`, which is why the grab handler reads as a separate function.
//
// A second jumptable, for wasp_state_damaged's switch on action_state, sits at
// 0x0048ecc8 - INSIDE the code section, four entries, immediately before
// wasp_state_death at 0x0048ece0. It is a compiler switch table, not an
// installable one.
//
// ---------------------------------------------------------------------------
// Shape of the AI - FOUR levels
// ---------------------------------------------------------------------------
//   Entity+0x84 state             init / run / knocked-down / death.
//   Entity+0x85 ignore_player_flag 0 = the hover drift layer in wasp_state_run
//                                 runs this frame, 1 = the behaviour owns the
//                                 wasp outright, anything else = frozen.
//   Entity+0x02 behavior_flags    the NEST state machine, not a plain spawn
//                                 kind. The high nibble decides which half of
//                                 wasp_behavior_nest runs:
//                                   0x00-0x0F  loose wasp, wakes on proximity
//                                   0x10       dormant in the nest, counting
//                                   0x11       cleared to emerge (bit 0)
//                                   0x12       the tenth respawn
//                                   >= 0x20    roused; go straight to hover
//                                 Bit 1 (0x02) is consumed by wasp_state_init
//                                 as the BIG-WASP flag and subtracted out.
//   Entity+0x86 action_behavior   0-6, the thing the wasp is doing.
//   Entity+0x87 action_state      the per-behaviour animation sub-state.
//
// ---------------------------------------------------------------------------
// Field aliases
// ---------------------------------------------------------------------------
// The port's Entity struct names these bytes from the zombie's point of view.
// The wasp reuses them at other widths, so they are reached by offset per the
// "offset writes, not nearest field" rule:
//
//   +0x38 int    localMatrix.t[1], the ALTITUDE. PS1 Y is negative up, so the
//                wasp cruises at about -4000 and the ground is 0.
//   +0xCA short  body tilt; the ceiling-spawn variant starts at 0x2000 and the
//                death crawl parks it at 1.
//   +0xC2 ushort wingbeat/translation speed (move_speed_current). The hover
//                layer drifts it and clamps at 400.
//   +0xC4 ushort dwell counter: the nest countdown, and the emerge/knockdown
//                frame count.
//   +0x16C short frames spent hovering before the next steering correction.
//   +0x16E word  entity_pathfind_update result; only bit 0 is used.
//   +0x170 word  ResolveEntityScaCollision result (the wasp touched the player)
//   +0x178 ushort manhattan distance to the player, |dx| + |dz|. Both terms are
//                computed in 32 bits and then added as WORDS (`ADD SI,AX`), so
//                the sum wraps at 16 bits exactly as written here.
//   +0x17A word  check_room_collision result
//   +0x17C word  vertical direction: 0 = sink, 1 = climb. A WORD, not a byte -
//                every one of the eight writes is `MOV word ptr [..+0x17c]`.
//   +0x17E short general-purpose countdown (nest arm timer, death fade timer)
//   +0x180 word  Joint_move result, i.e. "the animation finished this frame"
//   +0x182 byte  the BIG-WASP flag. Doubles the shadow, doubles the health,
//                doubles the sting damage, and blocks the grab attack entirely.
//   +0x184 byte  respawn counter; the tenth respawn comes back as kind 0x12.
//   +0x18A byte  Snd_em latch, so the death cry only plays once.
//
// The vertical bob step lives in +0x176 (reaction_timer). It ramps 0x23 -> 0x96
// one unit per frame and snaps back to 0x23, and the sign comes from +0x17C.
//
// ---------------------------------------------------------------------------
// The four wing joints
// ---------------------------------------------------------------------------
// jointsStructs[1..4].flags are written 0 at the top of the hover/victory
// behaviours and 3 every seventh animation frame - that is the wing blur being
// switched on for one frame in seven, not a visibility bug.
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

// 0x00be0df4 - shared entity scratch, defined in EntityCommon.cpp.
extern unsigned int g_entity_bkp;                                  // 0x00be0df4

// ============================================================================
// Offset accessors. Every width here is one the original actually uses.
// ============================================================================
#define WA_X          (*(int*)            ((char*)ENTITY + 0x34))
#define WA_Y          (*(int*)            ((char*)ENTITY + 0x38))
#define WA_Z          (*(int*)            ((char*)ENTITY + 0x3C))
#define WA_STATE_W    (*(short*)          ((char*)ENTITY + 0x84))
#define WA_STATE_DW   (*(unsigned int*)   ((char*)ENTITY + 0x84))
#define WA_BEH_W      (*(unsigned short*) ((char*)ENTITY + 0x86))
#define WA_SPEED      (*(short*)          ((char*)ENTITY + 0xC2))
#define WA_DWELL      (*(short*)          ((char*)ENTITY + 0xC4))
#define WA_TILT       (*(short*)          ((char*)ENTITY + 0xCA))
#define WA_DRIFT      (*(short*)          ((char*)ENTITY + 0x16C))
#define WA_PATH_W     (*(unsigned short*) ((char*)ENTITY + 0x16E))
#define WA_PATH_B     (*(unsigned char*)  ((char*)ENTITY + 0x16E))
#define WA_TOUCH      (*(short*)          ((char*)ENTITY + 0x170))
#define WA_BOB        (*(short*)          ((char*)ENTITY + 0x176))
#define WA_DIST       (*(unsigned short*) ((char*)ENTITY + 0x178))
#define WA_COLL       (*(unsigned short*) ((char*)ENTITY + 0x17A))
#define WA_CLIMB      (*(short*)          ((char*)ENTITY + 0x17C))
#define WA_TIMER      (*(short*)          ((char*)ENTITY + 0x17E))
#define WA_ANIM_DONE  (*(unsigned short*) ((char*)ENTITY + 0x180))
#define WA_BIG        (*(unsigned char*)  ((char*)ENTITY + 0x182))
#define WA_RESPAWNS   (*(unsigned char*)  ((char*)ENTITY + 0x184))
#define WA_SND_LATCH  (*(unsigned char*)  ((char*)ENTITY + 0x18A))

// g_tempVar (0x00be0df8) is declared `void*` in Globals.h; the behaviour
// dispatch parks a sign-extended short in it, exactly as the adder does.
#define WA_TURN_SLOW  (*(int*)&g_tempVar)

// The player's translation vector, the target of every steering call here.
#define WASP_PLAYER_T ((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t)

namespace {

// ============================================================================
// 0x004d3c38 / 0x004d3c48 - the SCA collision records, six shorts each, the
// same layout as the zombie's, the crow's and the adder's: [0] id/terminator,
// [1..3] local x/y/z, [4] half-height, [5] radius.
//
// The wasp's radius is ZERO. wasp_update still feeds it to check_room_collision
// (`MOV AX, word ptr [EDX+0xa]` off Entity->Sca_info), so the wasp is never
// pushed out of geometry - it flies through walls by design and only the SCA
// hit box against the player matters.
//
// 0x004d3c58 holds a POINTER to the first record and wasp_state_init loads the
// pointer (`MOV EDX,[0x004d3c58]`), not the address of the slot. The slot right
// after it at 0x004d3c5c points at a second record with id 0x8001 whose only
// non-zero field is a local Y of 8000, and it has NO readers anywhere in the
// exe - a leftover collision profile. It is reproduced here so the table keeps
// its original shape.
// ============================================================================
const short wasp_sca_info[6]     = { (short)0x8000, 0, 0,    0, 0, 0 };
const short wasp_sca_info_alt[6] = { (short)0x8001, 0, 8000, 0, 0, 0 };

const short* const wasp_sca_info_table[2] = {
    wasp_sca_info,      // 0x004d3c58
    wasp_sca_info_alt   // 0x004d3c5c - never read
};

void wasp_state_init(void);      // 0x0048d930
void wasp_state_run(void);       // 0x0048dbe0
void wasp_state_damaged(void);   // 0x0048ea20
void wasp_state_death(void);     // 0x0048ece0

void wasp_behavior_dispatch(void);  // 0x0048de60
void wasp_behavior_nest(void);      // 0x0048def0   (jumptable case 0)
void wasp_behavior_takeoff(void);   // 0x0048e070   (case 1)
void wasp_behavior_hover(void);     // 0x0048e110   (case 2)
void wasp_behavior_sting(void);     // 0x0048e3d0   (case 3)
void wasp_behavior_victory(void);   // 0x0048e510   (case 4)
void wasp_behavior_grab(void);      // 0x0048e810   (case 5, via 0x0048e6b0)
void wasp_behavior_emerge(void);    // 0x0048e6c0   (case 6)

void wasp_animate(void);            // 0x0048e780

// ============================================================================
// wasp_state_table @ 0x004d3c60
// FOUR entries, indexed by Entity->state. See the header comment for why the
// pointers that follow are NOT states 4-10.
//
// State 2 and state 3 are the entry points the damage system uses:
// weapon_apply_damage (WeaponDamage.cpp) sets state 3 outright and demotes it
// to state 2 when the health survives. The wasp also drives itself into state 3
// from its own attacks - both the sting and the grab set a NEGATIVE health, so
// a wasp that stings the player dies of it.
// ============================================================================
void* const wasp_state_table[4] = {
    (void*)wasp_state_init,     // [0] spawn / respawn
    (void*)wasp_state_run,      // [1] main AI driver
    (void*)wasp_state_damaged,  // [2] shot down, thrashing on the floor
    (void*)wasp_state_death     // [3] death
};

// ============================================================================
// wasp_behavior_jumptable @ 0x004d3c70
// The seven case bodies of wasp_behavior_dispatch's switch on action_behavior.
// wasp_behavior_dispatch indexes this array directly, matching the original's
// unchecked indirect JMP.
// ============================================================================
void* const wasp_behavior_jumptable[7] = {
    (void*)wasp_behavior_nest,     // [0] dormant / in the nest
    (void*)wasp_behavior_takeoff,  // [1] spiralling up off the surface
    (void*)wasp_behavior_hover,    // [2] the cruise: bob, close in, look for an opening
    (void*)wasp_behavior_sting,    // [3] the contact sting
    (void*)wasp_behavior_victory,  // [4] the player is dead; circle forever
    (void*)wasp_behavior_grab,     // [5] the pin-and-sting cutscene attack
    (void*)wasp_behavior_emerge    // [6] rising out of the nest
};

// ---------------------------------------------------------------------------
// The hover entry speed, spelled out because four sites compute it the same
// way: an UNSIGNED 16-bit divide of the manhattan distance
// (`MOV CX,0x64 / SUB DX,DX / DIV CX / ADD AX,0xc8`), so a far-away wasp starts
// its cruise faster.
// ---------------------------------------------------------------------------
short wasp_hover_speed(void)
{
    return (short)(unsigned short)((unsigned short)(WA_DIST / 100) + 200);
}

// ---------------------------------------------------------------------------
// The common "start cruising" block. The original repeats it inline at
// 0x0048df24 (nest), 0x0048e0d1 (takeoff), 0x0048e2c8 (hover re-entry) and
// 0x0048e73c (emerge); each site adds its own extras around it, so only the
// four writes they all share live here.
// ---------------------------------------------------------------------------
void wasp_enter_hover(void)
{
    ENTITY->ignore_player_flag = 0;
    WA_BEH_W = 2;      // behaviour AND sub-state in one word
    WA_BOB   = 0x23;
    WA_CLIMB = 1;
}

// ---------------------------------------------------------------------------
// Zero the shared effect-position scratch. Every Effect_CreateBillboard call in
// this file spawns at the joint's own origin, so all three components go to 0.
// ---------------------------------------------------------------------------
void wasp_clear_effect_pos(void)
{
    g_playerPosScratch.x = 0;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = 0;
}

// ============================================================================
// wasp_state_init @ 0x0048d930
// Fresh spawn, and also the landing pad for the respawn in wasp_state_death.
//
// Two traps in the arithmetic here:
//   * `AND AX,0x9` (0x0048da34) really is a mask of NINE, not 0xF or 7. The
//     dwell roll is therefore 800, 770, 560 or 530 - only four outcomes,
//     because only bits 0 and 3 survive.
//   * FOUR rand() calls feed the health roll and the FIRST result is thrown
//     away (0x0048daa0). Health = 20*(big+1) + 2*(r&7)*3, i.e. 20..62 for a
//     normal wasp and 40..82 for a big one.
// ============================================================================
void wasp_state_init(void)
{
    WA_STATE_W = 0x101;      // state = 1, ignore_player_flag = 1
    WA_BEH_W   = 0;          // behaviour = 0, sub-state = 0
    ENTITY->scaMatrixData.field_00 = 0;
    WA_DWELL   = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->death_timer        = 0;
    ENTITY->hit_state          = 0;
    ENTITY->status_flags       = 1;
    WA_TOUCH = 0;
    WA_DRIFT = 0;
    WA_TIMER = 0x5A;
    WA_SND_LATCH = 0;

    ResetJointTransforms();
    ENTITY->Sca_info = (unsigned int)(uintptr_t)wasp_sca_info_table[0];
    WA_TILT = 0;

    // A wasp placed with no altitude cruises at 4000 units up (Y is negative).
    if (WA_Y == 0) {
        WA_Y = -4000;
    }

    // Bit 1 of the spawn kind is the BIG-WASP flag; it is consumed here (the
    // original SUBTRACTS 2 rather than masking) and re-read all over the file.
    WA_BIG = 0;
    if ((ENTITY->behavior_flags & 2) != 0) {
        WA_TILT = 0x2000;
        ENTITY->behavior_flags = (unsigned char)(ENTITY->behavior_flags - 2);
        WA_BIG = 1;
    }

    // Kind 0x10 exactly - a wasp asleep in the nest - gets a randomised dwell
    // before it is cleared to emerge.
    if (ENTITY->behavior_flags == 0x10) {
        WA_DWELL = (short)(((int)((unsigned short)rand() & 9)) * -30 + 800);
    }

    // The ground shadow. 0x00be0dfc takes a packed 0xRRGGBB triple here, not a
    // frame id: `MOV dword ptr [0x00be0dfc],0x606060` is an IMMEDIATE colour.
    g_svecScratch.z = 0;
    g_svecScratch.y = 0;
    g_svecScratch.x = 0;
    g_animFrameIdSave = 0x606060;
    {
        short half = (short)((WA_BIG * 4 + 4) * 25);
        FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, half, half);
    }

    rand();                                     // discarded
    unsigned short ra = (unsigned short)rand();
    unsigned short rb = (unsigned short)rand();
    unsigned short rc = (unsigned short)rand();
    ENTITY->health = (short)(((rb & 7) * 2) + ((ra & 7) * 2) + ((rc & 7) * 2)
                             + (unsigned short)((WA_BIG + 1) * 20));
}

// ============================================================================
// wasp_state_run @ 0x0048dbe0
// The per-frame driver. Three layers:
//   1. a dead player forces behaviour 4 (the victory circle),
//   2. while ignore_player_flag is 0 the wasp DRIFTS - it counts hover frames,
//      creeps its speed up, corrects its heading toward the player every 29th
//      frame and re-rolls the speed whenever it stalls below 30,
//   3. the behaviour dispatch runs when ignore_player_flag is 0 or 1.
//
// Then the alert/visual range flags and the ground shadow, whose half-size is
// the root joint's WORLD Y >> 4 plus a per-size floor - the shadow shrinks as
// the wasp climbs, and falls back to the minimum once the sum goes negative.
// ============================================================================
void wasp_state_run(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    // A dead player and a wasp not already celebrating: switch to behaviour 4.
    if (g_playerEntity.health < 0 && ENTITY->action_behavior != 4) {
        WA_BOB   = 0x23;
        WA_CLIMB = 1;
        WA_SPEED = wasp_hover_speed();
        ENTITY->ignore_player_flag = 0;
        WA_BEH_W = 4;
    }

    // Bounced off geometry last frame - swing back toward the player.
    if (WA_COLL != 0) {
        ENTITY->angle = (short)(ENTITY->angle
                                + (short)turn_toward_target(WASP_PLAYER_T, 0x80));
    }

    WA_DIST = (unsigned short)
              ((unsigned short)abs(g_playerEntity.scaMatrixData.localMatrix.t[2] - WA_Z)
             + (unsigned short)abs(g_playerEntity.scaMatrixData.localMatrix.t[0] - WA_X));

    if (ENTITY->ignore_player_flag == 0) {
        WA_DRIFT = (short)(WA_DRIFT + 1);
        WA_SPEED = (short)(WA_SPEED + 2);

        // Every frame past the 29th, if the wasp is not already aimed, nudge the
        // heading and pay 8 units of speed for the turn.
        if (WA_DRIFT > 0x1C
            && (short)turn_toward_target(WASP_PLAYER_T, 0x100) != 0) {
            ENTITY->angle = (short)(ENTITY->angle
                                    + (short)turn_toward_target(WASP_PLAYER_T, 0x80));
            WA_SPEED = (short)(WA_SPEED - 8);
        }

        // Stalled: re-roll the cruise speed and steer AWAY (the sign is a SUB
        // at 0x0048dd81, the only place in the file that subtracts a turn).
        if (WA_SPEED < 0x1E) {
            WA_SPEED = wasp_hover_speed();
            ENTITY->angle = (short)(ENTITY->angle
                                    - (short)turn_toward_target(WASP_PLAYER_T, 0x200));
        }

        if (WA_SPEED > 400) {
            WA_SPEED = 400;
        }
        wasp_behavior_dispatch();
    } else if (ENTITY->ignore_player_flag == 1) {
        wasp_behavior_dispatch();
    }

    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
    ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | ENTITY_STATUS_ALIGNED);

    // Above 4000 units the wasp registers as "alerted"; below 1000 as "seen".
    if (WA_Y < -4000) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        entity_check_alert_range(4000);
    }
    if (WA_Y > -1000) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        entity_check_visual_range(4000);
    }

    g_collPushDepthZHi = (int)((WA_BIG * 4 + 0xC) * 25) + (joints[0].world.t[1] >> 4);
    if (g_collPushDepthZHi < 0) {
        g_collPushDepthZHi = (int)((WA_BIG * 4 + 4) * 25);
    }
    BillboardSetSize(&ENTITY->pushVelocity, (short)g_collPushDepthZHi,
                     (short)g_collPushDepthZHi);
}

// ============================================================================
// wasp_behavior_dispatch @ 0x0048de60
// Runs the pathfinder, parks the two turn probes in the shared scratch globals
// (g_tempVar and g_entity_bkp, both sign-extended shorts) and jumps into the
// behaviour. Only the pathfinder's bit 0 is kept, and only when no other bit is
// set - `TEST ECX,0xfffffffe` gates the whole latch.
// ============================================================================
void wasp_behavior_dispatch(void)
{
    unsigned char path = (unsigned char)entity_pathfind_update();
    g_animFrameIdSave = path;
    if ((path & 0xFE) == 0) {
        WA_PATH_B = (unsigned char)(WA_PATH_B & 0xFE);
        WA_PATH_W = (unsigned short)(WA_PATH_W | (unsigned short)(g_animFrameIdSave & 1));
    }

    WA_TURN_SLOW = (int)(short)turn_toward_target(WASP_PLAYER_T, 0x100);
    g_entity_bkp = (unsigned int)(int)(short)turn_toward_target(WASP_PLAYER_T, 0x200);

    // Indexed, not switched, because the original is a bare
    // `JMP dword ptr [ECX*4 + 0x4d3c70]` with NO bounds check - the compiler
    // proved action_behavior is 0-6.
    ((void (*)(void))wasp_behavior_jumptable[ENTITY->action_behavior])();
}

// ============================================================================
// wasp_animate @ 0x0048e780
// Start the current animation, then LOOP it - action_state is never advanced
// past 1, so the behaviour has to move itself on. The completion flag lands in
// WA_ANIM_DONE, which is the only thing wasp_behavior_sting waits on.
// ============================================================================
void wasp_animate(void)
{
    if ((char)ENTITY->action_state == 0) {
        ENTITY->action_state       = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->death_timer        = 0;
        ENTITY->blend_counter      = 3;
    } else if ((char)ENTITY->action_state != 1) {
        return;
    }
    WA_ANIM_DONE = (unsigned short)(unsigned char)
                   Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ============================================================================
// wasp_behavior_nest @ 0x0048def0   (jumptable case 0)
// The dormant state, and the nest state machine. The high nibble of
// behavior_flags splits it in two; see the header comment for the encoding.
//
// Ghidra renders this as the fat trailing "default" block of
// wasp_behavior_dispatch, because it is the jumptable's entry [0] and sits
// physically after the switch header. It is case 0, not a default.
// ============================================================================
void wasp_behavior_nest(void)
{
    ENTITY->hit_state   = 1;      // intangible while dormant
    ENTITY->animationId = 0;
    wasp_animate();

    unsigned char kind = ENTITY->behavior_flags;

    if ((kind & 0xF0) != 0) {
        // ---- in or around the nest ----
        if (kind > 0x1F) {
            // Already roused (bit 5 was set by the loose-wasp path below).
            ENTITY->hit_state = 0;
            wasp_enter_hover();
            WA_SPEED = wasp_hover_speed();
            return;
        }
        if ((kind & 1) != 0) {
            // Cleared to emerge.
            WA_BEH_W = 6;
            WA_SPEED = 0x1E;
            WA_DWELL = 0;
            ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
            return;
        }
        // Dormant: run the dwell down, and arm the emerge either when it
        // expires or as soon as the player comes within 4000 units.
        short dwell = WA_DWELL;
        WA_DWELL = (short)(dwell - 1);
        if (dwell == 0 || WA_DIST < 4000) {
            ENTITY->behavior_flags = 0x11;
        }
        return;
    }

    // ---- a loose wasp, not nest-bound ----
    if (kind == 0) {
        if (WA_DIST >= 5000) {
            return;
        }
    } else {
        short t = WA_TIMER;
        WA_TIMER = (short)(t - 1);
        if (t == 0) {
            return;
        }
    }

    ENTITY->hit_state = 0;
    WA_BEH_W = 1;                 // take off
    WA_TIMER = 0x5A;
    ENTITY->behavior_flags = (unsigned char)(ENTITY->behavior_flags | 0x20);
    WA_SPEED = 0x32;
}

// ============================================================================
// wasp_behavior_takeoff @ 0x0048e070   (case 1)
// A 24-frame spiral off the surface: 85 units of yaw per frame while the
// forward speed argument decays from 0x800 by 85 a frame, so the wasp corkscrews
// outward and levels off into the cruise.
// ============================================================================
void wasp_behavior_takeoff(void)
{
    ENTITY->animationId = 1;
    WA_SPEED = (short)(WA_SPEED + 1);
    wasp_animate();
    ENTITY->angle = (short)(ENTITY->angle + 0x55);
    Add_speedXZ((int)(0x800 - (unsigned int)ENTITY->animation_frame_id * 0x55));

    if (ENTITY->animation_frame_id == 0x18) {
        wasp_enter_hover();
    }
}

// ============================================================================
// wasp_behavior_hover @ 0x0048e110   (case 2)
// The cruise. Four things happen every frame:
//   * the four wing joints are blanked, then re-enabled on every seventh
//     animation frame (that is also when the wingbeat SFX fires, but only
//     within 1000 units),
//   * the bob step ramps 0x23..0x96 and is applied to the altitude with the
//     sign in WA_CLIMB,
//   * WA_CLIMB flips at randomised ceilings/floors: sink below
//     (-7 - r) * 500 and climb above (-3 - r) * 500, i.e. roughly -3500/-4000
//     and -1500/-2000,
//   * if the wasp is touching the player, within 1000 units, aimed, and its
//     altitude is inside the 600-unit band just under -2300, it commits to an
//     attack.
//
// The band test is an UNSIGNED compare of `-2300 - y` against 600
// (0x0048e283-0x0048e296), which is how one instruction pair expresses
// "-2900 < y <= -2300".
//
// The attack pick: normally the contact sting (behaviour 3), but a NORMAL-sized
// wasp that catches the player from behind and dead-on gets the pin-and-sting
// (behaviour 5). A big wasp can never grab.
// ============================================================================
void wasp_behavior_hover(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    joints[1].flags = 0;
    joints[2].flags = 0;
    joints[3].flags = 0;
    joints[4].flags = 0;

    ENTITY->animationId = 2;
    wasp_animate();

    if (ENTITY->animation_frame_id % 7 == 0) {
        joints[1].flags = 3;
        joints[2].flags = 3;
        joints[3].flags = 3;
        joints[4].flags = 3;
        if (WA_DIST < 1000) {
            Snd_em(0);
        }
    }

    Add_speedXZ(0);

    WA_BOB = (short)(WA_BOB + 1);
    if (WA_BOB > 0x96) {
        WA_BOB = 0x23;
    }

    if ((int)((-7 - ((unsigned int)rand() & 1)) * 500) > WA_Y) {
        WA_CLIMB = 0;
    }
    if ((int)((-3 - ((unsigned int)rand() & 1)) * 500) < WA_Y) {
        WA_CLIMB = 1;
    }
    if (WA_CLIMB != 0) {
        WA_Y = WA_Y - (int)WA_BOB;
    } else {
        WA_Y = WA_Y + (int)WA_BOB;
    }

    if (WA_TOUCH == 0 || WA_DIST >= 1000
        || (short)turn_toward_target(WASP_PLAYER_T, 0x100) != 0
        || (unsigned int)(-2300 - WA_Y) >= 600) {
        return;
    }

    WA_DRIFT = 0;
    joints[1].flags = 3;
    joints[2].flags = 3;
    joints[3].flags = 3;
    joints[4].flags = 3;
    wasp_enter_hover();
    WA_SPEED = wasp_hover_speed();

    if (g_playerEntity.isBeingAttackedFlag != 0 || (WA_PATH_W & 1) == 0) {
        return;
    }

    ENTITY->ignore_player_flag = 1;
    WA_BEH_W = 3;                        // the contact sting

    if ((char)is_facing_toward_entity(&g_playerEntity) == 0
        && (short)turn_toward_target(WASP_PLAYER_T, 0xC0) == 0
        && WA_BIG == 0) {
        WA_BEH_W = 5;                    // the pin-and-sting
        ENTITY->angle = (short)getAngleTowardsTarget(
            g_playerEntity.scaMatrixData.localMatrix.t[0],
            g_playerEntity.scaMatrixData.localMatrix.t[2]);
        g_playerEntity.isBeingAttackedFlag = 1;
        ENTITY->hit_state = 1;
    }
}

// ============================================================================
// wasp_behavior_sting @ 0x0048e3d0   (case 3)
// The contact sting. Tracks the player slowly (0x30 a frame) and does nothing
// until the animation completes; on that frame it either lands the hit or just
// falls through to the recovery, and either way it hands back to behaviour 1.
//
// Damage is (-1 - big) * 4, or * 10 when player flag 0x7B (the difficulty /
// "hard" bit) is set: 4 or 10 for a normal wasp, 8 or 20 for a big one. Note
// this is the raw contact sting - no poison. Only the pin-and-sting envenomates.
//
// The player is never killed outright by a normal wasp: health saturates at 1.
// A BIG wasp is allowed to finish the job, and drives the player's death pose
// itself (animationId 3, action_behavior 200, health -1).
// ============================================================================
void wasp_behavior_sting(void)
{
    ENTITY->animationId = 3;
    ENTITY->angle = (short)(ENTITY->angle
                            + (short)turn_toward_target(WASP_PLAYER_T, 0x30));
    wasp_animate();

    if (WA_ANIM_DONE == 0) {
        return;
    }

    if (g_playerEntity.isBeingAttackedFlag == 0
        && WA_DIST < 0x5DC
        && (short)turn_toward_target(WASP_PLAYER_T, 0x100) == 0
        && (WA_PATH_B & 1) != 0) {

        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.action_behavior     = 100;

        short dmg = (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)
                    ? (short)((-1 - (unsigned short)WA_BIG) * 4)
                    : (short)((-1 - (unsigned short)WA_BIG) * 10);
        g_playerEntity.health = (short)(g_playerEntity.health + dmg);

        if (g_playerEntity.health < 0) {
            g_playerEntity.health = 1;
            if (WA_BIG != 0) {
                g_playerEntity.animationId    = 3;
                g_playerEntity.animFrameId    = 0;
                g_playerEntity.action_behavior = 200;
                g_playerEntity.action_state    = 0;
                g_playerEntity.health          = -1;
                return;
            }
        }
    }

    WA_SPEED = 100;
    ENTITY->ignore_player_flag = 1;
    WA_BEH_W = 1;      // back up into the takeoff spiral
}

// ============================================================================
// wasp_behavior_victory @ 0x0048e510   (case 4)
// The player is dead. Structurally the hover minus the attack: same wing
// blanking, same bob ramp, same randomised altitude flips (this one also resets
// the bob on every flip), and the wasp closes in only while further than 2000
// units away.
//
// The forward speed argument at 0x0048e67c decompiles as a modulo-2 of
// `health << 11`. That term is provably always zero - the shift clears bit 0
// before the sign-preserving `% 2` idiom runs - so the call is Add_speedXZ(0x400)
// and the arithmetic is dead code the compiler kept.
// ============================================================================
void wasp_behavior_victory(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    joints[1].flags = 0;
    joints[2].flags = 0;
    joints[3].flags = 0;
    joints[4].flags = 0;

    ENTITY->animationId = 2;

    // The steering is scaled by the pathfinder bit, so a wasp with no path
    // holds its heading: turn * (path & 1) * 2.
    {
        short turn = (short)turn_toward_target(WASP_PLAYER_T, 0x10);
        short gate = (short)(WA_PATH_W & 1);
        ENTITY->angle = (short)(ENTITY->angle + (short)(turn * gate) * 2);
    }

    wasp_animate();

    if (ENTITY->animation_frame_id % 7 == 0) {
        joints[1].flags = 3;
        joints[2].flags = 3;
        joints[3].flags = 3;
        joints[4].flags = 3;
    }

    WA_BOB = (short)(WA_BOB + 1);
    if (WA_BOB > 0x96) {
        WA_BOB = 0x23;
    }

    if ((int)((-7 - ((unsigned int)rand() & 1)) * 500) > WA_Y) {
        WA_CLIMB = 0;
        WA_BOB   = 0x23;
    }
    if ((int)((-3 - ((unsigned int)rand() & 1)) * 500) < WA_Y) {
        WA_CLIMB = 1;
        WA_BOB   = 0x23;
    }
    if (WA_CLIMB != 0) {
        WA_Y = WA_Y - (int)WA_BOB;
    } else {
        WA_Y = WA_Y + (int)WA_BOB;
    }

    if (WA_DIST >= 2000) {
        Add_speedXZ(0);
        return;
    }
    Add_speedXZ(0x400);
}

// ============================================================================
// wasp_behavior_grab @ 0x0048e810   (case 5, reached through the bare
//                                   `JMP 0x0048e810` at jumptable entry 5)
// The pin-and-sting. The wasp snaps to the player's own facing, drops its
// altitude to 0 and plays a paired animation - the wasp's animation is
// (player.id & 1) + 4 and the player's is 5/7, so Jill and Chris get different
// takes of the same scripted attack. The hit frame is likewise per-character:
// (player.id & 1) * 2 + 0x22.
//
// The payout at sub-state 2 is the expensive one: a coin-flip poisoning, 8 or 15
// damage (player flag 0x7B again), and the wasp sets its OWN health to -44, so
// it always dies of the attack.
// ============================================================================
void wasp_behavior_grab(void)
{
    unsigned char st = ENTITY->action_state;

    if (st == 0) {
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        ENTITY->action_state       = 1;
        ENTITY->blend_counter      = 3;
        ENTITY->angle              = g_playerEntity.directionAngle;
        WA_Y = 0;
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags | 2);
        ENTITY->animationId  = (unsigned char)((g_playerEntity.id & 1) + 4);

        ENTITY->unk_c6 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        ENTITY->unk_c8 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        g_playerEntity.unk_c8 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        g_playerEntity.unk_c6 = (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[0];

        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.animationId    = 5;      // written as ONE word, 0x0705
        g_playerEntity.animFrameId    = 7;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state    = 0;
    } else if (st != 1) {
        if (st != 2) {
            return;
        }
        // ---- the payout ----
        ENTITY->health = -44;      // the wasp dies of its own sting

        if (((unsigned int)rand() & 1) != 0) {
            g_playerEntity.healthStatusFlags =
                (unsigned char)(g_playerEntity.healthStatusFlags | 2);   // poisoned
            g_playerEntity.pad_174 = 0x96;                               // 150-frame timer
        }

        if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            g_playerEntity.health = (short)(g_playerEntity.health - 8);
        } else {
            g_playerEntity.health = (short)(g_playerEntity.health - 15);
        }
        if (g_playerEntity.health < 0) {
            g_playerEntity.health = 1;
        }

        WA_STATE_W = 3;    // state = 3, ignore_player_flag = 0
        WA_BEH_W   = 0;
        return;
    }

    entity_apply_anim_vertex(ENTITY, ENTITY->animHeader, ENTITY->animBase);
    char adv = (char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
    ENTITY->action_state = (unsigned char)(ENTITY->action_state + adv);
    ENTITY->death_timer  = (unsigned char)(ENTITY->death_timer + 1);

    if (ENTITY->animation_frame_id == 4) {
        Play3DSnd(3, 0, 0, (int)g_playerEntity.scaMatrixData.localMatrix.t);
    }
    if ((unsigned char)((g_playerEntity.id & 1) * 2 + 0x22) == ENTITY->animation_frame_id) {
        ENTITY->action_state = 2;
    }
}

// ============================================================================
// wasp_behavior_emerge @ 0x0048e6c0   (case 6)
// Rising out of the nest. The altitude gains 20 a frame unconditionally and a
// SECOND 20 once it is above -2800 - so the climb accelerates as it clears the
// nest mouth - and past 20 frames it loses 60, which is what makes the wasp
// settle rather than shoot through the ceiling. At 26 frames it clears its own
// no-collide bit and joins the cruise.
// ============================================================================
void wasp_behavior_emerge(void)
{
    ENTITY->animationId = 2;
    WA_SPEED = (short)(WA_SPEED + 1);
    wasp_animate();

    WA_Y = WA_Y + 0x14;
    if (WA_Y > -0xAF0) {          // -2800
        WA_Y = WA_Y + 0x14;
        Add_speedXZ(0);
        WA_DWELL = (short)(WA_DWELL + 1);
        if (WA_DWELL > 0x14) {
            WA_Y = WA_Y - 0x3C;
        }
    }

    if (WA_DWELL > 0x19) {
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0xFD);
        ENTITY->hit_state = 0;
        wasp_enter_hover();
    }
}

// ============================================================================
// wasp_state_damaged @ 0x0048ea20
// Shot down but not killed: the wasp drops to the floor, thrashes, and lies
// there until either the player steps on it or it recovers. Ghidra reaches this
// through the jumptable at 0x0048ecc8; case 0 falls THROUGH into case 1, which
// is real - the drop starts on the same frame the state is entered.
//
//   0 -> hide the shadow, one blood puff at the root joint, and for a BIG wasp
//        an attack effect on each of the four wing joints
//   1 -> fall at 200 a frame until the floor (Y > -100), then tint the shadow
//        red and start growing it
//   2 -> grow the shadow for 30 frames (a big wasp gets 20 more), then go limp
//   3 -> lie there; if the player walks within 400 units while moving, the wasp
//        is CRUSHED: health -4 and straight into the death state
// ============================================================================
void wasp_state_damaged(void)
{
    switch (ENTITY->action_state) {
    case 0: {
        WA_SPEED = 0;
        ENTITY->action_state       = 1;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control     = 0;
        WA_DWELL = 0x1E;
        ENTITY->blend_counter = 3;
        ENTITY->hit_state     = 1;
        ENTITY->status_flags  = (unsigned char)(ENTITY->status_flags | 2);
        ENTITY->status_flags  = (unsigned char)(ENTITY->status_flags | ENTITY_STATUS_DEAD);
        BillboardSetSize(&ENTITY->pushVelocity, 0, 0);

        JointStruct* joints = ENTITY->jointsStructs;
        wasp_clear_effect_pos();
        Effect_CreateBillboard(0, 0x11, 0, &joints[0].world, &g_playerPosScratch, 0);

        if (WA_BIG != 0) {
            // Joints 1..4 - the wings. The loop counter is the shared scratch
            // global, and it runs 3 down to 0 inclusive.
            g_animFrameIdSave = 3;
            for (;;) {
                joint_setup_attack_effect((int)(intptr_t)&joints[g_animFrameIdSave + 1],
                                          0x1E, 10, 3);
                unsigned int i = g_animFrameIdSave;
                g_animFrameIdSave = g_animFrameIdSave - 1;
                if (i == 0) break;
            }
        }
    }
        // fall through
    case 1:
        WA_Y = WA_Y + 200;
        if (WA_Y > -100) {
            WA_Y = 0;
            ENTITY->action_state = 2;
            WA_TIMER = 0x1E;
            BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0xFF3030);
        }
        break;

    case 2: {
        BillboardAdjSize(&ENTITY->pushVelocity, 8, 8);
        short t = WA_TIMER;
        WA_TIMER = (short)(t - 1);
        if (t == 0) {
            ENTITY->action_state = 3;
            ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
            ENTITY->hit_state    = 0;
            if (WA_BIG != 0) {
                WA_TIMER = 0x14;
                return;
            }
        }
        break;
    }

    case 3:
        ENTITY->status_flags = (unsigned char)(ENTITY->status_flags & 0x1F);
        entity_check_visual_range(4000);

        WA_DIST = (unsigned short)
                  ((unsigned short)abs(g_playerEntity.scaMatrixData.localMatrix.t[2] - WA_Z)
                 + (unsigned short)abs(g_playerEntity.scaMatrixData.localMatrix.t[0] - WA_X));

        if (WA_DIST < 400 && g_playerEntity.move_speed_current != 0) {
            ENTITY->hit_state = 1;
            ENTITY->health    = -4;
            if (WA_SND_LATCH == 0) {
                Snd_em(2);
                WA_SND_LATCH = 1;
            }
            WA_STATE_W = 3;    // state = 3, ignore_player_flag = 0
            WA_BEH_W   = 0;
            return;
        }
        break;

    default:
        return;
    }
}

// ============================================================================
// wasp_state_death @ 0x0048ece0
// Sub-state 0 is the kill frame; 1 and 2 are the two ways the corpse leaves.
//
// The split at 0x0048edcd / 0x0048ee6d is the WEAPON FILTER: `hit_state & 0xF8`
// is the weapon id shifted left 3, so `> 0x30` means weapon id > 6 - the heavy
// weapons. A big wasp, or any wasp hit by a heavy weapon, is torn apart: attack
// effects on all ten joints, the death cry, and it is removed immediately
// (sub-state 4 + the room-event flag). Anything else leaves a corpse that crawls
// for 20 frames and then RESPAWNS.
//
// Sub-state 4 is the terminal "gone" value; there is no handler for it, so the
// switch header at 0x0048ecef falls straight to the RET.
//
// The respawn (sub-state 2) is the mechanism behind the wasp room never
// emptying: behavior_flags goes back to 0x10 (dormant in the nest), the
// position is reset to the FIXED nest coordinates, the whole 0x84 dword is
// zeroed - state, ignore, behaviour and sub-state all at once - and
// wasp_state_init runs again with fresh health. Every TENTH respawn comes back
// as kind 0x12 instead.
// ============================================================================
void wasp_state_death(void)
{
    unsigned char st = ENTITY->action_state;

    if (st == 1) {
        BillboardAdjSize(&ENTITY->pushVelocity, 8, 8);
        short t = WA_TIMER;
        WA_TIMER = (short)(t - 1);
        if (t == 0) {
            ENTITY->action_state = 4;
            Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
        }
        return;
    }

    if (st == 2) {
        WA_TILT = 1;
        short t = WA_TIMER;
        WA_TIMER = (short)(t - 1);
        if (t != 0) {
            return;
        }

        ENTITY->behavior_flags = 0x10;
        if (WA_RESPAWNS > 10) {
            WA_RESPAWNS = 0;
        }
        if (WA_RESPAWNS == 10) {
            ENTITY->behavior_flags = 0x12;
        }
        WA_RESPAWNS = (unsigned char)(WA_RESPAWNS + 1);

        ENTITY->angle = 0x400;
        // The nest, hard-coded: X 18000/17900, Y -4000/-4100, Z 22900/22800.
        ENTITY->scaMatrixData.localMatrix.t[0] = (int)((0xB4 - ((unsigned int)rand() & 1)) * 100);
        ENTITY->scaMatrixData.localMatrix.t[1] = (int)((-0x28 - ((unsigned int)rand() & 1)) * 100);
        ENTITY->scaMatrixData.localMatrix.t[2] = (int)((0xE5 - ((unsigned int)rand() & 1)) * 100);
        WA_STATE_DW = 0;   // state / ignore / behaviour / sub-state, one dword
        return;
    }

    if (st != 0) {
        return;    // sub-state 4: already gone
    }

    // ---- the kill frame ----
    WA_SPEED = 0;
    ENTITY->action_state       = 4;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    WA_DWELL = 0x1E;
    ENTITY->blend_counter = 3;
    ENTITY->status_flags  = (unsigned char)(ENTITY->status_flags | 2);
    ENTITY->status_flags  = (unsigned char)(ENTITY->status_flags | ENTITY_STATUS_DEAD);

    if (WA_SND_LATCH == 0) {
        Snd_em(1);
    }

    JointStruct* joints = ENTITY->jointsStructs;
    wasp_clear_effect_pos();
    Effect_CreateBillboard(0, 0x11, 0, &joints[0].world, &g_playerPosScratch, 0);
    Effect_CreateBillboard(0, 0x11, 0, &joints[9].world, &g_playerPosScratch, 0);

    // Big wasp, or killed by a heavy weapon (hit_state >> 3 > 6).
    if (WA_BIG != 0 || (int)(ENTITY->hit_state & 0xF8) > 0x30) {
        g_animFrameIdSave = 9;
        for (;;) {
            joint_setup_attack_effect((int)(intptr_t)&joints[g_animFrameIdSave],
                                      0x1E, 10, 3);
            unsigned int i = g_animFrameIdSave;
            g_animFrameIdSave = g_animFrameIdSave - 1;
            if (i == 0) break;
        }
        if (WA_SND_LATCH == 0) {
            Snd_em(2);
        }
    }

    // Already knocked down (wasp_state_damaged parked 0x14 here for a big
    // wasp): skip the corpse and just fade the shadow out.
    if (WA_TIMER == 0x14) {
        ENTITY->action_state = 1;
        return;
    }

    BillboardSetSize(&ENTITY->pushVelocity, 0, 0);

    if (WA_BIG == 0 && (int)(ENTITY->hit_state & 0xF8) <= 0x30) {
        // Light weapon, normal wasp: two more blood puffs and a 20-frame crawl
        // before the respawn.
        wasp_clear_effect_pos();
        Effect_CreateBillboard(0, 0x11, 0, &joints[7].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0x11, 0, &joints[8].world, &g_playerPosScratch, 0);
        ENTITY->action_state = 2;
        WA_TIMER = 0x14;
        return;
    }

    ENTITY->action_state = 4;
    Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
}

}  // namespace

// ============================================================================
// wasp_update @ 0x0048daf0
// The dispatch-table entry. The SCA block is gated on THREE things: a non-zero
// altitude, and a behaviour that is neither 0 (dormant in the nest) nor 6
// (emerging) - so a wasp inside its nest cannot be touched or collided with.
//
// The shadow is unconditional apart from the switch-zone test; unlike the crow
// there is no altitude gate on it, because wasp_state_run has already sized it
// from the root joint's world Y.
// ============================================================================
void wasp_update(void)
{
    if ((g_message_flags & 4) != 0) {
        ((void (*)(void))wasp_state_table[ENTITY->state])();

        if (WA_Y != 0
            && (char)ENTITY->action_behavior != 0
            && (char)ENTITY->action_behavior != 6) {
            SetEntityScaHitData(ENTITY);
            WA_TOUCH = (short)(unsigned char)ResolveEntityScaCollision(
                (Entity*)&g_playerEntity, ENTITY);
            HandleEnemyPlayerCollisions();
            WA_COLL = (unsigned short)(unsigned char)check_room_collision(
                (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
                *(short*)((char*)(uintptr_t)ENTITY->Sca_info + 10));
        }
    }

    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0], g_CurrentRdtDataTypePtr);

    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite((VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
                               (short*)&ENTITY->pushVelocity, 0, ENTITY->angle);
    }
}
