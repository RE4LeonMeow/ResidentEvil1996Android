// Neptune.cpp - Neptune (entity type 11, enemy/em100b.emd) - the shark
//
// Original PC addresses:
//   neptune_update              0x0043d8d0   per-frame entry, dispatch table [11]
//   neptune_clear_hit_state     0x0043d8a0   heavy-weapon hit latch reset
//   neptune_sca_info_big        0x004bc958   TWO-box SCA array
//   neptune_sca_info_small      0x004bc970   TWO-box SCA array (half scale)
//   neptune_capture_matrix      0x004bc988   the devour drag matrix (32 bytes)
//   neptune_state_table         0x004bc9a8   SIX entries (a real CALL table)
//   neptune_behavior_jumptable  0x004bc9c0   SIX entries (a compiler switch)
//   neptune_swim_actions        0x004bc9d8   THREE entries + NULL pad
//   neptune_retreat_actions     0x004bc9e8   *** TWO entries only ***
//   neptune_circle_actions      0x004bc9f0   THREE entries + NULL pad
//   neptune_devour_actions      0x004bca00   SEVEN entries + NULL pad
//   neptune_bite_actions        0x004bca20   SIX entries
//   neptune_death_kind_table    0x004bca38   TWO entries
//   neptune_death_actions       0x004bca40   THREE entries + NULL pad
//   neptune_boss_death_actions  0x004bca50   FIVE entries + NULL pad
//
// The id is 11 and the model is em100b.emd: g_emdPathTable index (id + 4) = 15.
//
// ---------------------------------------------------------------------------
// TEN dispatch tables in one 0xC8-byte block - and Ghidra merges four of them
// ---------------------------------------------------------------------------
// This is the worst instance of the "overlapping dispatch tables" trap in the
// game so far. Ten tables sit back to back from 0x004bc9a8 to 0x004bca68, and
// Ghidra's switch recovery runs straight off the end of the short ones into
// their neighbours. Decompiling neptune_behavior_dispatch (0x0043de20) renders
// ONE function containing a nine-case switch plus a second eleven-case switch,
// stitched together out of four different tables' bodies.
//
// Counting the READERS, not the NULL runs, settles every extent:
//
//   0x0043d8fe  CALL [EAX*4 + 0x4bc9a8]   EAX = Entity+0x84 state       -> 6
//   0x0043de2d  JMP  [ECX*4 + 0x4bc9c0]   ECX = Entity+0x86 behaviour   -> 6
//   0x0043de4f  CALL [ECX*4 + 0x4bc9d8]   ECX = Entity+0x87 action      -> 3
//   0x0043e1ad  JMP  [ECX*4 + 0x4bc9e8]   ECX = Entity+0x87 action      -> 2
//   0x0043e26d  CALL [ECX*4 + 0x4bc9f0]   ECX = Entity+0x87 action      -> 3
//   0x0043e3ff  CALL [ECX*4 + 0x4bca00]   ECX = Entity+0x87 action      -> 7
//   0x0043f00d  JMP  [ECX*4 + 0x4bca20]   ECX = Entity+0x87 action      -> 6
//   0x0043f9a4  JMP  [ECX*4 + 0x4bca38]   ECX = Entity+0x02 & 1         -> 2
//   0x0043f9be  CALL [ECX*4 + 0x4bca40]   ECX = Entity+0x86 behaviour   -> 3
//   0x0043fd1d  JMP  [ECX*4 + 0x4bca50]   ECX = Entity+0x86 behaviour   -> 5
//
// and each table ends exactly where the next one's reader points. The block
// stops at 0x004bca68, which is the identity MATRIX that g_deadMoveValue holds.
//
// **neptune_retreat_actions (0x004bc9e8) is TWO entries.** Ghidra decompiles
// FUN_0043e1a0's switch with five cases - 0 and 1 are real, and cases "2", "3"
// and "4" are the three bodies of neptune_circle_actions at 0x004bc9f0, read
// through the wrong base. The retreat's action_state only ever goes 0 -> 1, and
// entry 1 resets the whole state block, so it can never reach index 2. The same
// over-read makes neptune_bite_actions (six entries) decompile as eleven.
// See [[count-pointer-tables-with-aligned-reads]] - re-read base + idx*4 and
// write one entry per line before believing any of it.
//
// ---------------------------------------------------------------------------
// Shape of the AI
// ---------------------------------------------------------------------------
//   Entity+0x84 state              init / run / death / three bare RETs.
//                                  States 3, 4 and 5 are each a single `RET`
//                                  in the original (body_size 1) - real table
//                                  slots that deliberately do nothing, not
//                                  missing code.
//   Entity+0x85 ignore_player_flag 0 = the drift/steer layer in
//                                  neptune_choose_attack runs this frame,
//                                  1 = the behaviour owns the shark.
//   Entity+0x02 behavior_flags     the SPAWN KIND, a real bitfield here (not a
//                                  state machine like the wasp's):
//                                    bit 0 (0x01) the aquarium BOSS variant -
//                                        never gets the "aligned" flag, and
//                                        takes the five-stage beaching death
//                                        instead of the three-stage sinking one
//                                    bit 1 (0x02) the SMALL variant - half-size
//                                        shadow and half-size SCA boxes
//                                    bit 7 (0x80) once state != 0, skip the
//                                        whole update
//                                  init also tests `behavior_flags == 2` for
//                                  EQUALITY (`CMP byte [+2],2`, 0x0043dc96) to
//                                  raise status bit 1 - a whole-byte compare,
//                                  not a bit test.
//   Entity+0x86 action_behavior    0-5 while alive, and the DEATH stage while
//                                  dying (both death paths index their own
//                                  table with it, not with action_state).
//   Entity+0x87 action_state       the per-behaviour animation sub-state.
//
// ---------------------------------------------------------------------------
// The two attacks, and why the shark eats you only when you are nearly dead
// ---------------------------------------------------------------------------
// neptune_choose_attack (0x0043dd50) picks between them, and the test is on the
// PLAYER's health (0x0043ddc0 `CMP word [0x00be636c],0x19` / `JG`):
//
//   player health <= 25  and distance < 3500  ->  behaviour 4, DEVOUR
//   player health >  25  and distance < 3000  ->  behaviour 5, BITE
//
// Behaviour 5 is the ordinary attack: it chews for a while, costs 7 health (30
// if g_ScenarioFlags bit SCENARIO_FLAG_SECOND_PLAYTHROUGH is set), floors the result at 1 so it can never
// kill, and lets go. Behaviour 4 is the swallow: neptune_devour_swallow sets
// `player health = -1` outright and hides the player's joints a few at a time
// as the shark works them down. So the instant-kill is gated on already being
// nearly dead - that is the design, not a missing health check.
//
// Both are only reachable while the player is not already grabbed
// (`isBeingAttackedFlag == 0`) and the shark is not the small variant.
//
// ---------------------------------------------------------------------------
// SCA collision is a TWO-BOX array, not one record
// ---------------------------------------------------------------------------
// Entity+4 gets 0x004bc958 or 0x004bc970, and each is TWO 6-short records - a
// front box and a rear box 1000 units apart, because the shark's body is long.
// The first short is the flags word and 0x8000 marks the LAST entry, exactly as
// wasp_sca_info uses it; the array is walked until that bit shows up.
//
//   big   {0,  500, -800, 0, 800, 600}, {0x8000, -500, -800, 0, 800, 600}
//   small {0,  250, -400, 0, 400, 300}, {0x8000, -250, -400, 0, 400, 600}
//
// The small array's second record ends in 600, not the 300 its first record and
// every other field's halving would predict. Byte-checked twice against
// 0x004bc97c - it really is 0x0258. Ported verbatim; do not "fix" it.
//
// ---------------------------------------------------------------------------
// Field aliases
// ---------------------------------------------------------------------------
// The port's Entity struct names these bytes from the zombie's point of view.
// Neptune reuses several at other widths, so they are reached by offset per the
// "offset writes, not nearest field" rule:
//
//   +0x1C  int    scaMatrixData's first dword, cleared as ONE dword by init
//                 (`MOV dword ptr [EAX+0x1c],EBX`, 0x0043da58).
//   +0x84  uint   the whole state block, written as a DWORD in three places.
//                 init's is 0x00010101 (0x0043dbb9) = state 1, ignore 1,
//                 behaviour 1, action 0 - NOT 0x01000001. The other two,
//                 0x01000001 (0x0043f836 and 0x0043eecd), are state 1,
//                 ignore 0, behaviour 0, action **1**, so the shark resumes at
//                 neptune_swim_far rather than re-running neptune_swim_begin.
//   +0xC1  byte   cleared by init; inside the port's pad_c0[2].
//   +0xCA  short  body pitch, set to 0x0800 for the small variant only
//                 (`MOV word ptr [ECX+0xca],0x800`) - a WORD, and the port
//                 calls those two bytes pad_ca.
//   +0x16C short  the struggle timer. SIGNED and 16-bit: seeded to 0x5A, then
//                 `-= (input ? 8 : 1)` per frame, and the grab ends when it
//                 goes NEGATIVE. The port names +0x16C/+0x16D as two separate
//                 bytes (attacking_direction / dir_control_flags).
//   +0x16E ushort check_room_collision's per-frame result, stored by
//                 neptune_update as a WORD (`MOV word ptr [ESI+0x16e],DX`) and
//                 read back as the movement_dist argument to
//                 entity_update_wander_turn. The port calls this byte texBank.
//   +0x172 char   an accumulator, not a boolean: Joint_move's return is ADDED
//                 to it each frame and the swim cue fires when it reaches 3.
//
// ---------------------------------------------------------------------------
// The devour drag matrix at 0x004bc988
// ---------------------------------------------------------------------------
// A 32-byte MATRIX of zeros in the image. neptune_devour_bite fills it via
// 0x0048c530 (the shared "express the player's transform in a joint's frame"
// helper the Yawn and Plant 42 grabs also use), and then
// neptune_devour_swallow walks its translation every frame with
//   `SUB dword ptr [0x004bc99c],0x41`   t[0] -= 65
//   `ADD dword ptr [0x004bc9a0],0xd`    t[1] += 13
// - DWORD operations on t[0] and t[1], dragging the player into the mouth and
// down. player_anim_death_billboard (0x00440230) reads the same matrix for the
// player half of the animation, which is why it is defined here and not made
// static: that function's action_state == 2 branch is still a TODO in
// PlayerAnimations.cpp and this is the matrix it needs.
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

// 0x00be0de4 - shared entity scratch, defined in EntityCommon.cpp. The devour
// grab writes the Z half of the room-collision correction through it.
extern int player_distance_z;                                      // 0x00be0de4

// 0x0040a250 / 0x0048c530 - transpose a MATRIX's 3x3 short block, and build a
// capture matrix that expresses the player's transform in a joint's local
// frame. Both are STATIC PER TRANSLATION UNIT in the original, which is why
// MainMenu.cpp, Plant42.cpp and Yawn.cpp each carry their own copy rather than
// sharing one - Yawn's pair even sits inside an anonymous namespace, so it has
// internal linkage and cannot be reached from here at all. Neptune keeps its
// own copy for the same reason.
static void neptune_matrix_transpose(const MATRIX* src, MATRIX* dst)
{
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            ((short*)dst)[j * 3 + i] = src->m[i][j];
}

static void neptune_capture_setup(const MATRIX* joint, MATRIX* playerMtx, MATRIX* out)
{
    MATRIX transposed;
    neptune_matrix_transpose(joint, &transposed);
    MulMatrix0(&transposed, playerMtx, out);

    VECTOR rel;
    rel.x = playerMtx->t[0] - joint->t[0];
    rel.y = playerMtx->t[1] - joint->t[1];
    rel.z = playerMtx->t[2] - joint->t[2];
    ApplyMatrixLV(&transposed, &rel, &rel);
    out->t[0] = rel.x;
    out->t[1] = rel.y;
    out->t[2] = rel.z;
}

// ============================================================================
// Offset accessors. Every width here is one the original actually uses.
// ============================================================================
#define NE_STATE_BLOCK  (*(unsigned int*)  ((char*)ENTITY + 0x84))
#define NE_SCA_FIELD00  (*(int*)           ((char*)ENTITY + 0x1C))
#define NE_UNK_C1       (*(unsigned char*) ((char*)ENTITY + 0xC1))
#define NE_PITCH        (*(short*)         ((char*)ENTITY + 0xCA))
#define NE_STRUGGLE     (*(short*)         ((char*)ENTITY + 0x16C))
#define NE_ROOM_HIT     (*(unsigned short*)((char*)ENTITY + 0x16E))
#define NE_SWIM_CUE     (*(signed char*)   ((char*)ENTITY + 0x172))

// The entity's own local matrix and its position, as the original addresses them.
#define NE_MATRIX       ((MATRIX*)((char*)ENTITY + 0x20))
#define NE_POS          ((VECTOR*)((char*)ENTITY + 0x34))
#define NE_ROT          ((SVECTOR*)((char*)ENTITY + 0x72))

// The player's position pair, the argument every reach test takes (0x00be6318).
#define PLAYER_T        ((VECTOR*)g_playerEntityPointer.scaMatrixData.localMatrix.t)
#define PLAYER_MATRIX   (&g_playerEntityPointer.scaMatrixData.localMatrix)

// ============================================================================
// neptune_sca_info_big @ 0x004bc958 / neptune_sca_info_small @ 0x004bc970
// Two boxes each; 0x8000 in the flags word of the second marks the end of the
// array. See the header comment for the small array's odd trailing 600.
// ============================================================================
const short neptune_sca_info_big[2][6] = {
    { 0,               500, -800, 0, 800, 600 },
    { (short)0x8000,  -500, -800, 0, 800, 600 }
};

const short neptune_sca_info_small[2][6] = {
    { 0,               250, -400, 0, 400, 300 },
    { (short)0x8000,  -250, -400, 0, 400, 600 }
};

// ============================================================================
// neptune_capture_matrix @ 0x004bc988
// The devour drag matrix. Shared with player_anim_death_billboard - see the
// header comment.
// ============================================================================
// All 32 bytes are zero in the image; neptune_devour_bite fills it in.
MATRIX neptune_capture_matrix = {};

// ---------------------------------------------------------------------------
// Forward declarations, in address order.
// ---------------------------------------------------------------------------
// 0x0043d8a0 - external linkage: the Black Tiger's web-build (0x004502e0)
// calls the same shared function, so BlackTiger.cpp declares it extern.
void neptune_clear_hit_state(void);              // 0x0043d8a0
static void neptune_state_init(void);           // 0x0043da40
static void neptune_state_run(void);            // 0x0043dcb0
static void neptune_choose_attack(void);        // 0x0043dd50
static void neptune_behavior_dispatch(void);    // 0x0043de20

static void neptune_behavior_swim(void);        // 0x0043de40
static void neptune_swim_begin(void);           // 0x0043dfb0
static void neptune_swim_far(void);             // 0x0043e030
static void neptune_swim_near(void);            // 0x0043e060

static void neptune_behavior_lunge(void);       // 0x0043e090
static void neptune_behavior_retreat(void);     // 0x0043e1a0
static void neptune_retreat_begin(void);        // 0x0043e1c0
static void neptune_retreat_run(void);          // 0x0043e210

static void neptune_behavior_circle(void);      // 0x0043e260
static void neptune_circle_begin(void);         // 0x0043e2a0
static void neptune_circle_left(void);          // 0x0043e310
static void neptune_circle_right(void);         // 0x0043e380

static void neptune_behavior_devour(void);      // 0x0043e3f0
static void neptune_devour_seize(void);         // 0x0043e4c0
static void neptune_devour_carry(void);         // 0x0043e560
static void neptune_devour_bite(void);          // 0x0043e7b0
static void neptune_devour_swallow(void);       // 0x0043e860
static void neptune_devour_finish(void);        // 0x0043ec20
static void neptune_devour_spit(void);          // 0x0043ec40
static void neptune_devour_recover(void);       // 0x0043eee0

static void neptune_behavior_bite(void);        // 0x0043f000
static void neptune_bite_begin(void);           // 0x0043f020
static void neptune_bite_close(void);           // 0x0043f080
static void neptune_bite_seize(void);           // 0x0043f2f0
static void neptune_bite_shake(void);           // 0x0043f3d0
static void neptune_bite_release(void);         // 0x0043f6c0
static void neptune_bite_recover(void);         // 0x0043f840

static void neptune_state_death(void);          // 0x0043f970
static void neptune_death_normal(void);         // 0x0043f9b0
static void neptune_death_turn(void);           // 0x0043fa90
static void neptune_death_thrash(void);         // 0x0043fbd0
static void neptune_death_sink(void);           // 0x0043fc70

static void neptune_death_boss(void);           // 0x0043fd10
static void neptune_boss_death_begin(void);     // 0x0043fd30
static void neptune_boss_death_charge(void);    // 0x0043fe10
static void neptune_boss_death_charge2(void);   // 0x0043fef0
static void neptune_boss_death_beach(void);     // 0x0043ffd0
static void neptune_boss_death_fade(void);      // 0x00440110

static void neptune_state_3(void);              // 0x00440190
static void neptune_state_4(void);              // 0x004401a0
static void neptune_state_5(void);              // 0x004401b0

// ============================================================================
// neptune_state_table @ 0x004bc9a8 - SIX entries, indexed by Entity->state.
// The last three are bare RETs in the original; see the header comment.
// ============================================================================
void* const neptune_state_table[6] = {
    (void*)neptune_state_init,   // [0] spawn
    (void*)neptune_state_run,    // [1] main AI driver
    (void*)neptune_state_death,  // [2] death (splits on behavior_flags bit 0)
    (void*)neptune_state_3,      // [3] RET
    (void*)neptune_state_4,      // [4] RET
    (void*)neptune_state_5       // [5] RET
};

// ============================================================================
// neptune_behavior_jumptable @ 0x004bc9c0 - SIX entries on action_behavior.
// ============================================================================
void* const neptune_behavior_jumptable[6] = {
    (void*)neptune_behavior_swim,     // [0] cruise, with the distance sub-states
    (void*)neptune_behavior_lunge,    // [1] the fast pass, random burst speed
    (void*)neptune_behavior_retreat,  // [2] back off and hand control back
    (void*)neptune_behavior_circle,   // [3] circle the player, alternating sides
    (void*)neptune_behavior_devour,   // [4] the swallow (player health <= 25)
    (void*)neptune_behavior_bite      // [5] the ordinary bite
};

// ============================================================================
// Per-behaviour action tables. The originals are all unchecked indirect
// CALL/JMP, so these are indexed directly too - the compiler proved each range.
// ============================================================================
void* const neptune_swim_actions[3] = {      // 0x004bc9d8
    (void*)neptune_swim_begin,               // [0] pick the swim animation
    (void*)neptune_swim_far,                 // [1] far away: watch for a chance
    (void*)neptune_swim_near                 // [2] closed in: drop back to [1]
};

// TWO entries. Not five - see the header comment.
void* const neptune_retreat_actions[2] = {   // 0x004bc9e8
    (void*)neptune_retreat_begin,            // [0] start the turn-away animation
    (void*)neptune_retreat_run               // [1] run it, then reset to state 1
};

void* const neptune_circle_actions[3] = {    // 0x004bc9f0
    (void*)neptune_circle_begin,             // [0] seed the circling timer
    (void*)neptune_circle_left,              // [1] +0x20 per frame
    (void*)neptune_circle_right              // [2] -0x20 per frame
};

void* const neptune_devour_actions[7] = {    // 0x004bca00
    (void*)neptune_devour_seize,             // [0] grab: player anim 7
    (void*)neptune_devour_carry,             // [1] carry; reach test at frame 15
    (void*)neptune_devour_bite,              // [2] capture matrix + hide 9 joints
    (void*)neptune_devour_swallow,           // [3] player health = -1, drag down
    (void*)neptune_devour_finish,            // [4] back to state 1
    (void*)neptune_devour_spit,              // [5] the MISSED grab recovery
    (void*)neptune_devour_recover            // [6] settle, then resume swimming
};

void* const neptune_bite_actions[6] = {      // 0x004bca20
    (void*)neptune_bite_begin,               // [0] anim 7, start closing
    (void*)neptune_bite_close,               // [1] close in; reach test each frame
    (void*)neptune_bite_seize,               // [2] clamp on, seed the struggle timer
    (void*)neptune_bite_shake,               // [3] chew; mashing shortens it
    (void*)neptune_bite_release,             // [4] let go, restore the player
    (void*)neptune_bite_recover              // [5] settle, then resume swimming
};

// ============================================================================
// Death tables. The kind table is indexed by `behavior_flags & 1`, and both
// death paths then index their own table with action_behavior.
// ============================================================================
void* const neptune_death_kind_table[2] = {  // 0x004bca38
    (void*)neptune_death_normal,             // [0] the ordinary shark
    (void*)neptune_death_boss                // [1] the aquarium boss
};

void* const neptune_death_actions[3] = {     // 0x004bca40
    (void*)neptune_death_turn,               // [0] turn to face, or give up
    (void*)neptune_death_thrash,             // [1] the death thrash
    (void*)neptune_death_sink                // [2] sink, then back to state 1
};

void* const neptune_boss_death_actions[5] = {// 0x004bca50
    (void*)neptune_boss_death_begin,         // [0] raise the room flag, anim 2
    (void*)neptune_boss_death_charge,        // [1] the first charge
    (void*)neptune_boss_death_charge2,       // [2] the second charge
    (void*)neptune_boss_death_beach,         // [3] beach itself, recolour
    (void*)neptune_boss_death_fade           // [4] shrink away
};

// ---------------------------------------------------------------------------
// Shared effect helper. Nine call sites spawn the same three-billboard spray
// seeded from the g_deadMoveValue + 0x14 block, so it is factored out here; the
// original open-codes it every time.
// ---------------------------------------------------------------------------
static void neptune_seed_from_dead_move(void)
{
    const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
    g_playerPosScratch.x   = seed[0];
    g_playerPosScratch.y   = seed[1];
    g_playerPosScratch.z   = seed[2];
    g_playerPosScratch.pad = seed[3];
}

// The bubble trail: one plume at the tail, two at the flanks. `joints` is the
// entity's joint array base, as the original passes it.
static void neptune_spawn_bubbles(int joints)
{
    neptune_seed_from_dead_move();
    g_playerPosScratch.y = -100;
    Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x614), &g_playerPosScratch, 0);
    g_playerPosScratch.y = 0;
    Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x32C), &g_playerPosScratch, 0);
    Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
}

// The blood cloud the shark leaves while chewing: two puffs at randomised
// offsets around its own matrix.
static void neptune_spawn_blood_cloud(void)
{
    g_playerPosScratch.y = -1500;
    g_playerPosScratch.x = (int)(rand() & 0x3FF) - 300;
    g_playerPosScratch.z = (int)(rand() & 0x3FF) - 750;
    Effect_CreateBillboard(0x17, 10, 0, NE_MATRIX, &g_playerPosScratch, 0);

    g_playerPosScratch.y = -1500;
    g_playerPosScratch.x = (int)(rand() & 0x3FF) - 1450;
    g_playerPosScratch.z = (int)(rand() & 0x3FF) - 750;
    Effect_CreateBillboard(0x17, 9, 0, NE_MATRIX, &g_playerPosScratch, 0);
}

// ============================================================================
// neptune_lfsr_bit @ 0x0048ae90
// A 16-bit LFSR step on g_RandSeed, called from exactly one place in the game
// (neptune_behavior_lunge, 0x0043e0c0), so it lives here rather than in the
// shared helpers.
//
// Ghidra renders this as an unreadable pile of CONCAT11/_1_1_ byte surgery. The
// assembly is five instructions:
//
//   MOV AX,[g_RandSeed]        ; the value BEFORE the shift
//   SHR word [g_RandSeed],1
//   AND AX,0x202               ; isolate bit 1 and bit 9
//   CMP AH,AL                  ; do those two taps agree?
//   JNZ +                      ;   no  -> leave the top bit clear
//   OR byte [g_RandSeed+1],0x80 ;  yes -> feed a 1 back into bit 15
// + MOV AL,[g_RandSeed]        ; return the low byte of the SHIFTED seed
//
// so it is a Fibonacci LFSR with taps at bits 1 and 9, and the caller keeps
// only bit 0 of the result to choose between two swim animations.
// ============================================================================
static unsigned char neptune_lfsr_bit(void)
{
    unsigned short prev = (unsigned short)g_RandSeed;
    g_RandSeed = (unsigned short)(prev >> 1);

    unsigned short taps = (unsigned short)(prev & 0x0202);
    if ((unsigned char)(taps >> 8) == (unsigned char)(taps & 0xFF)) {
        g_RandSeed = (unsigned short)(g_RandSeed | 0x8000);
    }
    return (unsigned char)(g_RandSeed & 0xFF);
}

// ============================================================================
// neptune_clear_hit_state @ 0x0043d8a0
// Clears the damage latch every fifth animation frame, but only while the
// player is holding a heavy weapon (`equippedWeaponId > 0x6e`). That is what
// lets the rocket launcher and the flamethrower keep landing hits on a shark
// that is already dying - without it the latch would swallow every hit after
// the first. Called from the death thrash and the sink, and (external linkage)
// from the Black Tiger's web-build.
// ============================================================================
void neptune_clear_hit_state(void)
{
    if (g_playerEntityPointer.equippedWeaponId > 0x6E &&
        (ENTITY->animation_frame_id % 5) == 0) {
        ENTITY->hit_state = 0;
    }
}

// ============================================================================
// neptune_state_init @ 0x0043da40
// One-shot spawn. Rolls health, builds the shadow quad and installs the SCA
// box array for the size variant, then nudges the position 1000 units along the
// shark's own facing so it does not start clipped into the wall behind it.
// ============================================================================
static void neptune_state_init(void)
{
    // A DWORD write: state 1, ignore 0, behaviour 0, action 0.
    NE_STATE_BLOCK = 1;
    NE_SCA_FIELD00 = 0;
    NE_UNK_C1 = 0;
    ENTITY->action_ticks_counter = 15;
    ENTITY->hit_state = 0;
    ResetJointTransforms();

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_animFrameIdSave = 0x404040;

    if ((ENTITY->behavior_flags & 2) != 0) {
        // The small variant: half-size shadow, half-size boxes, pitched down.
        FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 1000, 150);
        NE_PITCH = 0x0800;
        ENTITY->Sca_info = (unsigned int)(uintptr_t)neptune_sca_info_small;
    } else {
        FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 2000, 300);
        ENTITY->Sca_info = (unsigned int)(uintptr_t)neptune_sca_info_big;
    }

    // FOUR rand() calls and the FIRST is DISCARDED (0x0043db1a) - the same
    // shape as the crow's and the wasp's health rolls. Health is 40..61.
    rand();
    int r1 = rand();
    int r2 = rand();
    int r3 = rand();
    ENTITY->health = (short)(((unsigned short)r1 & 7) +
                             ((unsigned short)r2 & 7) +
                             ((unsigned short)r3 & 7) + 0x28);

    ENTITY->animationId = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3F;

    if ((ENTITY->behavior_flags & 1) != 0) {
        // The aquarium boss starts already circling: animation 2, and a DWORD
        // state write of 0x00010101 - state 1, ignore_player 1, behaviour 1,
        // action 0. Verified at 0x0043dbb9; it is NOT 0x01000001.
        ENTITY->animationId = 2;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0;
        NE_STATE_BLOCK = 0x00010101;
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);

    ENTITY->move_timer = 7;
    ENTITY->move_max_steps = 5;
    ENTITY->splatter_flag = 0;
    ENTITY->angle_turn_delta = 0;

    // Push 1000 units along the shark's facing. The y/z/pad terms come from the
    // g_deadMoveValue + 0x14 block, .x is replaced - the same seeded-probe
    // idiom the dog and the crow use.
    neptune_seed_from_dead_move();
    g_playerPosScratch.x = 1000;
    RotMatrix(NE_ROT, &g_matrixScratch);
    ApplyMatrixLV(&g_matrixScratch, &g_playerPosScratch, &g_playerPosScratch);
    // WORD adds into the SVECTOR position, not the int matrix translation.
    ENTITY->position.x = (short)(ENTITY->position.x + (short)g_playerPosScratch.x);
    ENTITY->position.z = (short)(ENTITY->position.z + (short)g_playerPosScratch.z);

    // A whole-byte EQUALITY test, not a bit test (`CMP byte [+2],2`).
    if (ENTITY->behavior_flags == 2) {
        ENTITY->status_flags |= 2;
    }
}

// ============================================================================
// neptune_state_run @ 0x0043dcb0
// The per-frame driver: distance, flags, then the steer layer and the
// behaviour.
// ============================================================================
static void neptune_state_run(void)
{
    zone_path_find(PLAYER_T->x, PLAYER_T->z,
                 (int*)&ENTITY->player_pos_x, (int*)&ENTITY->player_pos_z);

    // Manhattan distance, computed and summed entirely in 32 bits - the two
    // CDQ/XOR/SUB absolute values are added as DWORDs (0x0043dd02 `ADD ESI,EAX`
    // on the full register). This does NOT wrap at 16 bits the way the wasp's
    // equivalent does, so do not copy that note over.
    int dx = PLAYER_T->x - NE_POS->x;
    int dz = PLAYER_T->z - NE_POS->z;
    g_playerDisplacement = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);

    ENTITY->status_flags &= 0x1F;
    // The aquarium boss never gets the "aligned" bit - it is always hostile.
    if ((ENTITY->behavior_flags & 1) == 0 && g_playerDisplacement < 10000) {
        ENTITY->status_flags |= 0x40;
    }

    entity_check_visual_range(4000);

    if (ENTITY->ignore_player_flag == 0) {
        neptune_choose_attack();
    }
    neptune_behavior_dispatch();
}

// ============================================================================
// neptune_choose_attack @ 0x0043dd50
// The drift/steer layer, plus the attack decision. Runs only while the
// behaviour has not claimed the shark (ignore_player_flag == 0).
// ============================================================================
static void neptune_choose_attack(void)
{
    g_playerPosScratch.x = (int)ENTITY->player_pos_x;
    g_playerPosScratch.z = (int)ENTITY->player_pos_z;
    entity_update_wander_turn(NE_ROOM_HIT, &ENTITY->splatter_flag,
                              &ENTITY->angle_turn_delta, 0x20, 60);

    // Never interrupt a grab already in progress, and the small variant never
    // attacks at all.
    if (g_playerEntityPointer.isBeingAttackedFlag != 0) return;
    if ((ENTITY->behavior_flags & 2) != 0) return;

    // Lined up within 768 (a 0x300 half-window)?
    if ((short)turn_toward_target(PLAYER_T, 768) != 0) return;

    if (g_playerEntityPointer.health < 26) {
        // Nearly dead: the swallow, and from further out.
        if (g_playerDisplacement < 3500) {
            ENTITY->ignore_player_flag = 1;
            // A WORD write: behaviour 4, action_state 0.
            *(unsigned short*)&ENTITY->action_behavior = 4;
        }
    } else if (g_playerDisplacement < 3000) {
        ENTITY->ignore_player_flag = 1;
        *(unsigned short*)&ENTITY->action_behavior = 5;
    }
}

// ============================================================================
// neptune_behavior_dispatch @ 0x0043de20
// The bare indirect JMP through neptune_behavior_jumptable. Everything Ghidra
// renders as this function's body belongs to the six case bodies below.
// ============================================================================
static void neptune_behavior_dispatch(void)
{
    ((void (*)(void))neptune_behavior_jumptable[ENTITY->action_behavior])();
}

// ============================================================================
// neptune_behavior_swim @ 0x0043de40 - behaviour 0
// The cruise. Dispatches the distance sub-state, advances the animation, fires
// the swim cue every third completed animation, and trails bubbles.
// ============================================================================
static void neptune_behavior_swim(void)
{
    ((void (*)(void))neptune_swim_actions[ENTITY->action_state])();

    // Joint_move's return is ADDED to the cue counter, not assigned.
    NE_SWIM_CUE = (signed char)(NE_SWIM_CUE +
        (signed char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40));
    if (NE_SWIM_CUE == 3) {
        Snd_em(0);
        NE_SWIM_CUE = 0;
    }
    Add_speedXZ(0);

    // The small variant leaves no wake.
    if ((ENTITY->behavior_flags & 2) != 0) return;

    ENTITY->move_timer--;
    if (ENTITY->move_timer != 0) return;

    int joints = (int)ENTITY->jointsStructs;
    g_playerPosScratch.x = 500;
    g_playerPosScratch.y = 0;
    g_playerPosScratch.z = 0;
    Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x32C), &g_playerPosScratch, 0);
    if ((rand() & 1) == 0) {
        g_playerPosScratch.x = 400;
        Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0xC0), &g_playerPosScratch, 0);
    }
    g_playerPosScratch.x = -100;
    g_playerPosScratch.y = -400;
    g_playerPosScratch.z = 0;
    Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x614), &g_playerPosScratch, 0);
    if ((rand() & 3) == 0) {
        g_playerPosScratch.y = 500;
        Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x1B8), &g_playerPosScratch, 0);
    }
    ENTITY->move_timer = 4;
}

// ---- neptune_swim_actions ------------------------------------------------

// 0x0043dfb0 - pick the swim animation and the cruise speed. The poison flag
// (g_ScenarioFlags bit SCENARIO_FLAG_SECOND_PLAYTHROUGH) makes the shark noticeably slower, which is what
// gives the poisoned player a chance to reach the drain valve.
static void neptune_swim_begin(void)
{
    ENTITY->animationId = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3F;
    ENTITY->action_state++;
    ENTITY->move_speed_current = (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) ? 0x78 : 100;
    NE_SWIM_CUE = 3;
}

// 0x0043e030 - far away. Speed up and move to the near sub-state once the
// player is beyond 10000 units AND running (player action_behavior == 13).
static void neptune_swim_far(void)
{
    if (g_playerDisplacement > 10000 && g_playerEntityPointer.action_behavior == 0xD) {
        ENTITY->action_state++;
        ENTITY->move_speed_current = 0x82;
    }
}

// 0x0043e060 - the mirror of the above; drop back as soon as the player is
// inside 6000 or stops running.
static void neptune_swim_near(void)
{
    if (g_playerDisplacement < 6000 || g_playerEntityPointer.action_behavior != 0xD) {
        ENTITY->action_state--;
        ENTITY->move_speed_current = 100;
    }
}

// ============================================================================
// neptune_behavior_lunge @ 0x0043e090 - behaviour 1
// The fast pass. Coin-flips between two swim animations, then swims with a
// RANDOM per-frame speed while steering at the player through a +/-0x400 yaw
// bias - the bias is what makes the shark bank around the player instead of
// homing straight in.
//
// Note the sign order: the yaw is biased DOWN by 0x400 before the rotate and
// back UP afterwards (`SUB word [+0x74],0x400` at 0x0043e171, `ADD` at
// 0x0043e18c). Every other bias site in this file is the other way round.
// ============================================================================
static void neptune_behavior_lunge(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->action_state = 1;
        ENTITY->move_speed_current = 0x32;
        ENTITY->animationId = (unsigned char)((neptune_lfsr_bit() & 1) + 2);
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3F;
        Snd_em(4);
    } else if (ENTITY->action_state != 1) {
        return;
    }

    if (ENTITY->animationId == 2 && ENTITY->animation_frame_id == 0x19) {
        Snd_em(5);
    }

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40) != 0) {
        ENTITY->action_state--;
    }

    Add_speedXZ(rand() & 0xFFF);
    ENTITY->angle = (short)(ENTITY->angle - 0x400);
    entity_rotate_toward_target(PLAYER_T, 4);
    ENTITY->angle = (short)(ENTITY->angle + 0x400);
}

// ============================================================================
// neptune_behavior_retreat @ 0x0043e1a0 - behaviour 2
// TWO action states. See the header comment for why Ghidra shows five.
// ============================================================================
static void neptune_behavior_retreat(void)
{
    ((void (*)(void))neptune_retreat_actions[ENTITY->action_state])();
}

// 0x0043e1c0 - start the turn-away animation. Note this one does NOT call
// Add_speedXZ; only the run state does.
static void neptune_retreat_begin(void)
{
    ENTITY->action_state++;
    ENTITY->move_speed_current = 0x96;
    ENTITY->animationId = 6;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3F;
}

// 0x0043e210 - run it, and at frame 23 reset the whole state block back to
// plain state 1. That DWORD write is why action_state can never reach 2 here.
static void neptune_retreat_run(void)
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
    if (ENTITY->animation_frame_id == 0x17) {
        NE_STATE_BLOCK = 1;
    }
    Add_speedXZ(0);
}

// ============================================================================
// neptune_behavior_circle @ 0x0043e260 - behaviour 3
// Circle the player, alternating sides on a randomised timer.
// ============================================================================
static void neptune_behavior_circle(void)
{
    ((void (*)(void))neptune_circle_actions[ENTITY->action_state])();
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
    Add_speedXZ(0);
}

// 0x0043e2a0 - seed the first (longer) leg: 90..121 frames.
static void neptune_circle_begin(void)
{
    ENTITY->animationId = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3F;
    ENTITY->action_state++;
    ENTITY->move_speed_current = 100;
    ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1F) + 0x5A);
    ENTITY->bob_speed = 0x20;
}

// 0x0043e310 - circle one way; hand over to the other side after 30..61 frames.
static void neptune_circle_left(void)
{
    entity_update_wander_turn(NE_ROOM_HIT, &ENTITY->splatter_flag,
                              &ENTITY->angle_turn_delta, 0x20, 0x3C);
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        ENTITY->action_state++;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1F) + 0x1E);
    }
}

// 0x0043e380 - and back, with the step negated. 0xFFE0 is -0x20 as the
// unsigned short the helper takes.
static void neptune_circle_right(void)
{
    entity_update_wander_turn(NE_ROOM_HIT, &ENTITY->splatter_flag,
                              &ENTITY->angle_turn_delta, 0xFFE0, 0x3C);
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        ENTITY->action_state--;
        ENTITY->action_ticks_counter = (unsigned short)((rand() & 0x1F) + 0x5A);
    }
}

// ============================================================================
// neptune_behavior_devour @ 0x0043e3f0 - behaviour 4
// The swallow. After the action body runs, the shark is pushed out of room
// geometry and the SAME correction is applied to the player - position, the
// animation base offsets and the SVECTOR position all move together, so the
// player stays in the shark's mouth while the pair is shoved clear of a wall.
// ============================================================================
static void neptune_behavior_devour(void)
{
    ((void (*)(void))neptune_devour_actions[ENTITY->action_state])();

    int beforeZ = NE_POS->z;
    int beforeX = NE_POS->x;
    check_room_collision(NE_POS, 500);
    g_playerDisplacement = NE_POS->x - beforeX;
    player_distance_z    = NE_POS->z - beforeZ;

    ENTITY->unk_c6 = (unsigned short)(ENTITY->unk_c6 + (short)g_playerDisplacement);
    ENTITY->unk_c8 = (unsigned short)(ENTITY->unk_c8 + (short)player_distance_z);
    g_playerEntityPointer.unk_c6 =
        (unsigned short)(g_playerEntityPointer.unk_c6 + (short)g_playerDisplacement);
    g_playerEntityPointer.unk_c8 =
        (unsigned short)(g_playerEntityPointer.unk_c8 + (short)player_distance_z);
    ENTITY->position.x = (short)(ENTITY->position.x + (short)g_playerDisplacement);
    ENTITY->position.z = (short)(ENTITY->position.z + (short)player_distance_z);
    g_playerEntityPointer.position.x =
        (short)(g_playerEntityPointer.position.x + (short)g_playerDisplacement);
    g_playerEntityPointer.position.z =
        (short)(g_playerEntityPointer.position.z + (short)player_distance_z);
}

// ---- neptune_devour_actions ----------------------------------------------

// 0x0043e4c0 - seize. Snaps the player into animation 7 frame 11 and takes over
// their facing. `status_flags |= 2` is set twice in the original, once before
// and once after the sound - harmless, kept for fidelity.
static void neptune_devour_seize(void)
{
    entity_rotate_toward_target(PLAYER_T, 0x100);
    ENTITY->animationId = 6;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3F;
    ENTITY->action_state++;
    ENTITY->status_flags |= 2;
    ENTITY->move_speed_current = 0x96;
    Snd_em(2);
    ENTITY->status_flags |= 2;

    g_playerEntityPointer.animationId = 7;
    g_playerEntityPointer.animFrameId = 0xB;
    g_playerEntityPointer.action_behavior = 0;
    g_playerEntityPointer.action_state = 0;
    g_playerEntityPointer.flags |= 6;
    g_playerEntityPointer.directionAngle = ENTITY->angle;
}

// 0x0043e560 - carry. At animation frame 15 the mouth reach is tested with a
// generous 1000-unit box: a hit snaps the player onto the shark and advances,
// a MISS jumps straight to action_state 5 (the spit) and hands the player back.
static void neptune_devour_carry(void)
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);

    neptune_seed_from_dead_move();
    g_playerPosScratch.x = 200;

    if (ENTITY->animation_frame_id == 0xF) {
        if (FUN_0048ae00((MATRIX*)((int)ENTITY->jointsStructs + 0xC0),
                         &g_playerPosScratch, 1000, (int*)PLAYER_T) == 0) {
            // The grab missed. Hand the player back and skip to the spit.
            g_playerEntityPointer.animation_frame_id = 0;
            g_playerEntityPointer.unk_bf = 0;
            g_playerEntityPointer.move_speed_current = 0;
            g_playerEntityPointer.attackAnim = 0;
            g_playerEntityPointer.unk_8c = 0;
            // A DWORD write of 0x01000001: anim 1, frame 0, behaviour 0,
            // action_state 1.
            *(unsigned int*)&g_playerEntityPointer.animationId = 0x01000001;
            g_playerEntityPointer.attackDirection = 100;
            g_playerEntityPointer.directionAngle =
                (short)(g_playerEntityPointer.directionAngle - 0x800);
            ENTITY->action_state = 5;
            g_playerEntityPointer.isBeingAttackedFlag = 0;
            g_playerEntityPointer.flags &= 0xF9;
            return;
        }
        // Hit: the player is now carried at the shark's own position.
        g_playerEntityPointer.scaMatrixData.localMatrix.t[0] = NE_POS->x;
        g_playerEntityPointer.scaMatrixData.localMatrix.t[2] = NE_POS->z;
        ENTITY->action_state++;
    }

    if (ENTITY->animation_frame_id < 3) {
        neptune_spawn_blood_cloud();
    }

    if (ENTITY->animation_frame_id > 3 && ENTITY->animation_frame_id < 5) {
        int joints = (int)ENTITY->jointsStructs;
        neptune_seed_from_dead_move();
        g_playerPosScratch.y = 500;
        Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x614), &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x51C), &g_playerPosScratch, 0);
    }

    Add_speedXZ(0);
}

// 0x0043e7b0 - the bite lands. Builds the drag matrix from the mouth joint and
// the player's transform, bumps the player's action_state, and hides NINE of
// the player's joints in one go: the limbs disappear as they go in.
static void neptune_devour_bite(void)
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
    if (ENTITY->animation_frame_id != 0x13) return;

    ENTITY->action_state++;
    neptune_capture_setup((const MATRIX*)((int)ENTITY->jointsStructs + 0xC0),
                          PLAYER_MATRIX, &neptune_capture_matrix);

    JointStruct* pj = g_playerEntityPointer.jointsStructs;
    g_playerEntityPointer.zoneFlags |= 0x40;
    g_playerEntityPointer.action_state++;

    // Written in exactly this order in the original.
    pj[1].flags  &= 0xFE;
    pj[0].flags  &= 0xFE;
    pj[9].flags  &= 0xFE;
    pj[12].flags &= 0xFE;
    pj[10].flags &= 0xFE;
    pj[13].flags &= 0xFE;
    pj[11].flags &= 0xFE;
    pj[14].flags &= 0xFE;
    pj[2].flags  &= 0xFE;
}

// 0x0043e860 - the swallow proper. Sets the player's health to -1 as a WORD,
// drags the capture matrix in and down every frame, hides two more joint pairs
// at frames 41 and 59, and sprays blood on a set of frame-modulo schedules.
static void neptune_devour_swallow(void)
{
    ENTITY->action_state = (unsigned char)(ENTITY->action_state +
        (unsigned char)Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40));

    g_playerEntityPointer.health = -1;

    // DWORD arithmetic on the drag matrix's t[0] and t[1] (0x0043e89c).
    neptune_capture_matrix.t[0] -= 0x41;
    neptune_capture_matrix.t[1] += 0x0D;

    JointStruct* pj = g_playerEntityPointer.jointsStructs;
    if (ENTITY->animation_frame_id == 0x29) {
        pj[3].flags &= 0xFE;
        pj[6].flags &= 0xFE;
    }
    if (ENTITY->animation_frame_id == 0x3B) {
        pj[4].flags &= 0xFE;
        pj[7].flags &= 0xFE;
        pj[5].flags &= 0xFE;
        pj[8].flags &= 0xFE;
    }

    unsigned char frame = ENTITY->animation_frame_id;
    int joints = (int)ENTITY->jointsStructs;

    if (frame < 0x38) {
        if (frame > 2 && frame < 5) {
            neptune_spawn_blood_cloud();
        }
        if ((ENTITY->animation_frame_id & 7) == 0 && ENTITY->animation_frame_id > 7) {
            neptune_seed_from_dead_move();
            g_playerPosScratch.y = 200;
            g_playerPosScratch.x = (int)(rand() & 0x1FF) - 0x100;
            g_playerPosScratch.z = (int)(rand() & 0x1FF) - 0x100;
            Effect_CreateBillboard(0x17, 10, 0, (void*)(joints + 0x2B0),
                                   &g_playerPosScratch, 0);
            neptune_seed_from_dead_move();
            Effect_CreateBillboard(0x17, 9, 0, (void*)(joints + 0xC0),
                                   &g_playerPosScratch, 0);
        }
        if ((ENTITY->animation_frame_id & 7) == 0 && ENTITY->animation_frame_id > 0xA) {
            neptune_seed_from_dead_move();
            g_playerPosScratch.y = 200;
            g_playerPosScratch.x = (int)(rand() & 0x1FF) - 0x100;
            g_playerPosScratch.z = (int)(rand() & 0x1FF) - 0x100;
            Effect_CreateBillboard(0x17, 10, 0, (void*)(joints + 0x614),
                                   &g_playerPosScratch, 0);
        }
    } else if ((frame & 3) == 0) {
        neptune_spawn_bubbles(joints);
    }

    // An unsigned 8-bit DIV; the remainder is the test (0x0043ebbf).
    if ((ENTITY->animation_frame_id % 5) == 0 && ENTITY->animation_frame_id > 1) {
        neptune_seed_from_dead_move();
        Effect_CreateBillboard(0, 0, 0, (void*)(joints + 0xC0), &g_playerPosScratch, 0);
    }
}

// 0x0043ec20 - done eating. Drop the grab bit and go back to plain state 1.
static void neptune_devour_finish(void)
{
    ENTITY->status_flags &= 0xFD;
    NE_STATE_BLOCK = 1;
}

// 0x0043ec40 - the MISSED grab. The shark thrashes, spits, and at frame 23
// releases the animation lock and moves to the recover state.
static void neptune_devour_spit(void)
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);

    if (ENTITY->animation_frame_id == 0x17) {
        ENTITY->action_state++;
        ENTITY->action_ticks_counter = 0x1E;
        ENTITY->status_flags &= 0xFD;
        ENTITY->animationId = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
    }

    if (ENTITY->animation_frame_id > 2 && ENTITY->animation_frame_id < 5) {
        neptune_spawn_blood_cloud();
    }

    if (ENTITY->animation_frame_id > 4 && (ENTITY->animation_frame_id & 3) == 0) {
        int joints = (int)ENTITY->jointsStructs;
        neptune_seed_from_dead_move();
        g_playerPosScratch.y = 200;
        Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x44), &g_playerPosScratch, 0);
        g_playerPosScratch.x = 800 - (int)(rand() & 0x3FF);
        Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x614), &g_playerPosScratch, 0);
    }

    if ((ENTITY->animation_frame_id & 7) == 0) {
        int joints = (int)ENTITY->jointsStructs;
        neptune_spawn_bubbles(joints);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -1500;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0x17, 13, 0, PLAYER_MATRIX, &g_playerPosScratch, 0);
    }
}

// 0x0043eee0 - settle after a miss, then resume swimming. The DWORD write here
// is 0x01000001 - action_state ONE, so the shark re-enters neptune_swim_far
// rather than re-initialising the animation.
static void neptune_devour_recover(void)
{
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);
    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        NE_STATE_BLOCK = 0x01000001;
        ENTITY->move_speed_current = 0x78;
    }
    Add_speedXZ(0);

    ENTITY->move_timer--;
    if (ENTITY->move_timer == 0) {
        neptune_spawn_bubbles((int)ENTITY->jointsStructs);
        ENTITY->move_timer = 0xB;
    }
}

// ============================================================================
// neptune_behavior_bite @ 0x0043f000 - behaviour 5
// The ordinary attack. Six action states; see the header comment for why Ghidra
// shows eleven.
// ============================================================================
static void neptune_behavior_bite(void)
{
    ((void (*)(void))neptune_bite_actions[ENTITY->action_state])();
}

// 0x0043f020 - start closing.
static void neptune_bite_begin(void)
{
    ENTITY->action_state++;
    ENTITY->move_speed_current = 100;
    ENTITY->animationId = 7;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x1F;
    ENTITY->status_flags |= 2;
}

// 0x0043f080 - close in. Tests the mouth reach EVERY frame with a tight
// 300-unit box (the devour's test is 1000 and fires once), and if the animation
// runs out first the attack is abandoned to action_state 5.
static void neptune_bite_close(void)
{
    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
        ENTITY->action_state = 5;
        ENTITY->status_flags &= 0xFD;
        ENTITY->action_ticks_counter = 0x1E;
        ENTITY->animationId = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        return;
    }

    // Wake spray on even frames; which pair depends on how far in we are.
    unsigned char frame = ENTITY->animation_frame_id;
    if ((frame & 1) == 0) {
        int joints = (int)ENTITY->jointsStructs;
        if (frame < 0x1E) {
            g_playerPosScratch.x = 500;
            g_playerPosScratch.y = 200;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0xC0),
                                   &g_playerPosScratch, 0);
        } else {
            neptune_seed_from_dead_move();
            g_playerPosScratch.y = -100;
            Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x614),
                                   &g_playerPosScratch, 0);
            g_playerPosScratch.y = 0;
            Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x32C),
                                   &g_playerPosScratch, 0);
            Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x44),
                                   &g_playerPosScratch, 0);
        }
    }

    entity_rotate_toward_target(PLAYER_T, 0x20);

    neptune_seed_from_dead_move();
    g_playerPosScratch.x = 200;
    if (FUN_0048ae00((MATRIX*)((int)ENTITY->jointsStructs + 0xC0),
                     &g_playerPosScratch, 300, (int*)PLAYER_T) != 0) {
        ENTITY->action_state++;
        ENTITY->animationId = 8;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;

        // Snap the player onto the shark - both the animation base offsets and
        // the facing - and start the "held in the jaws" animation 6 frame 11.
        ENTITY->unk_c6 = (unsigned short)(short)NE_POS->x;
        ENTITY->unk_c8 = (unsigned short)(short)NE_POS->z;
        g_playerEntityPointer.unk_c6 = (unsigned short)(short)NE_POS->x;
        g_playerEntityPointer.unk_c8 = (unsigned short)(short)NE_POS->z;
        g_playerEntityPointer.directionAngle = ENTITY->angle;
        g_playerEntityPointer.flags |= 2;
        g_playerEntityPointer.animationId = 6;
        g_playerEntityPointer.animFrameId = 0xB;
        g_playerEntityPointer.action_behavior = 0;
        g_playerEntityPointer.action_state = 0;
    }
    Add_speedXZ(0);
}

// 0x0043f2f0 - clamped on. Seeds the 90-frame struggle timer.
static void neptune_bite_seize(void)
{
    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
        ENTITY->action_state++;
        Snd_em(1);
        ENTITY->animationId = 9;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        g_playerEntityPointer.animation_frame_id = 0;
        g_playerEntityPointer.attackAnim = 1;
        g_playerEntityPointer.unk_8c = 3;
        g_playerEntityPointer.unk_bf = 0;
        NE_STRUGGLE = 0x5A;
    }

    if ((ENTITY->animation_frame_id & 3) == 0) {
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -1500;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0x17, 13, 0, PLAYER_MATRIX, &g_playerPosScratch, 0);
    }
}

// 0x0043f3d0 - the chew. Mashing the controller costs the timer 8 per frame
// instead of 1, so the struggle is genuinely shortened by input. Each completed
// animation cycle takes 7 health, or 30 if the player is poisoned - and the
// result is FLOORED AT 1, so this attack can never kill.
static void neptune_bite_shake(void)
{
    NE_STRUGGLE = (short)(NE_STRUGGLE - ((GetPlayerInputMasked() != 0) ? 8 : 1));

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
        if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 7);
        } else {
            g_playerEntityPointer.health = (short)(g_playerEntityPointer.health - 0x1E);
        }

        if ((ENTITY->animation_frame_id & 3) == 0) {
            int joints = (int)ENTITY->jointsStructs;
            neptune_seed_from_dead_move();
            g_playerPosScratch.y = -100;
            Effect_CreateBillboard(0x17, 10, 0, (void*)(joints + 0x614),
                                   &g_playerPosScratch, 0);
            g_playerPosScratch.y = 0;
            Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x32C),
                                   &g_playerPosScratch, 0);
            Effect_CreateBillboard(0x17, 8, 0,
                                   &g_playerEntityPointer.jointsStructs[0xB].world,
                                   &g_playerPosScratch, 0);
        }

        if (g_playerEntityPointer.health < 0) {
            g_playerEntityPointer.health = 1;
        }
    }

    if ((ENTITY->animation_frame_id & 0xF) == 0) {
        int joints = (int)ENTITY->jointsStructs;
        neptune_seed_from_dead_move();
        g_playerPosScratch.y = -100;
        Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x614),
                               &g_playerPosScratch, 0);
        g_playerPosScratch.y = 0;
        g_playerPosScratch.x = 200;
        Effect_CreateBillboard(0x17, 13, 0,
                               &g_playerEntityPointer.jointsStructs[1].world,
                               &g_playerPosScratch, 0);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -1500;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0x17, 13, 0, PLAYER_MATRIX, &g_playerPosScratch, 0);
    }

    ENTITY->move_timer--;
    if (ENTITY->move_timer == 0) {
        g_playerPosScratch.x = 600;
        g_playerPosScratch.y = -200;
        g_playerPosScratch.z = -1300;
        Effect_CreateBillboard(0x17, 13, 0, NE_MATRIX, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, NE_MATRIX, &g_playerPosScratch, 0);
        ENTITY->move_timer = 0xB;
    }

    // NEGATIVE, not zero - the timer is a signed short and the mashing path
    // can step it past 0 in one frame.
    if (NE_STRUGGLE < 0) {
        ENTITY->action_state++;
        ENTITY->move_speed_current = 100;
        ENTITY->animationId = 10;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;
        g_playerEntityPointer.animation_frame_id = 0;
        g_playerEntityPointer.unk_bf = 0;
        g_playerEntityPointer.attackAnim = 2;
        g_playerEntityPointer.unk_8c = 0;
        Snd_em(1);
    }
}

// 0x0043f6c0 - let go and give the player back their own animation. 0xFFF0 is
// -0x10 as the unsigned short the rotate helper takes.
static void neptune_bite_release(void)
{
    entity_rotate_toward_target(PLAYER_T, 0xFFF0);

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80) != 0) {
        ENTITY->action_state++;
        ENTITY->action_ticks_counter = 0x1E;
        ENTITY->status_flags &= 0xFD;
        ENTITY->animationId = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x1F;

        g_playerEntityPointer.animationId = 1;
        g_playerEntityPointer.animFrameId = 0;
        g_playerEntityPointer.action_behavior = 0;
        g_playerEntityPointer.action_state = 0;
        g_playerEntityPointer.flags &= 0xFD;
        g_playerEntityPointer.isBeingAttackedFlag = 0;
    }

    if ((ENTITY->animation_frame_id & 7) == 0) {
        neptune_spawn_bubbles((int)ENTITY->jointsStructs);
        g_playerPosScratch.x = 0;
        g_playerPosScratch.y = -1500;
        g_playerPosScratch.z = 0;
        Effect_CreateBillboard(0x17, 13, 0, PLAYER_MATRIX, &g_playerPosScratch, 0);
    }
    Add_speedXZ(0);
}

// 0x0043f840 - settle, then resume swimming at action_state 1 (the 0x01000001
// DWORD write again). 0xFFE0 is -0x20.
static void neptune_bite_recover(void)
{
    entity_rotate_toward_target(PLAYER_T, 0xFFE0);
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x80);

    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        NE_STATE_BLOCK = 0x01000001;
        ENTITY->move_speed_current = 0x78;
    }
    Add_speedXZ(0);

    ENTITY->move_timer--;
    if (ENTITY->move_timer == 0) {
        neptune_spawn_bubbles((int)ENTITY->jointsStructs);
        ENTITY->move_timer = 0xF;
    }
}

// ============================================================================
// neptune_state_death @ 0x0043f970 - state 2
// Entered by weapon_apply_damage. Claims the shark on the first frame, then
// splits on `behavior_flags & 1`: the ordinary shark sinks in three stages, the
// aquarium boss beaches itself in five.
// ============================================================================
static void neptune_state_death(void)
{
    if (ENTITY->ignore_player_flag == 0) {
        ENTITY->ignore_player_flag = 1;
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 0;
    }
    ((void (*)(void))neptune_death_kind_table[ENTITY->behavior_flags & 1])();
}

// ---- the ordinary death --------------------------------------------------

// 0x0043f9b0 - run the stage, and keep trailing bubbles on a 7-frame cycle.
static void neptune_death_normal(void)
{
    ((void (*)(void))neptune_death_actions[ENTITY->action_behavior])();

    if ((ENTITY->behavior_flags & 2) != 0) return;

    ENTITY->move_timer--;
    if (ENTITY->move_timer == 0) {
        ENTITY->move_timer = 7;
        neptune_spawn_bubbles((int)ENTITY->jointsStructs);
    }
}

// 0x0043fa90 - turn to face the player for one last look. If the shark cannot
// line up it gives up and jumps to stage 2 (the sink) instead of thrashing.
// Note the stage is stepped to 1 FIRST and then overwritten with 2 on failure.
static void neptune_death_turn(void)
{
    ENTITY->action_behavior++;
    ENTITY->move_speed_current = 0x50;
    ENTITY->action_ticks_counter = 0x1E;

    if ((short)turn_toward_target(PLAYER_T, 0x400) != 0) {
        ENTITY->action_behavior = 2;
        ENTITY->move_speed_current = 0x78;
        return;
    }

    ENTITY->animationId = 1;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 7;
    Snd_em(2);
    ENTITY->move_timer = 7;

    // The death burst: a heavier spray, with lightFactor 0x1E rather than 0.
    int joints = (int)ENTITY->jointsStructs;
    neptune_seed_from_dead_move();
    g_playerPosScratch.y = -100;
    Effect_CreateBillboard(0x17, 13, 0, (void*)(joints + 0x614), &g_playerPosScratch, 0x1E);
    g_playerPosScratch.y = 0;
    Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x13C), &g_playerPosScratch, 0x1E);
    Effect_CreateBillboard(0x17, 8, 0, (void*)(joints + 0x4A0), &g_playerPosScratch, 0x1E);

    // Runs the thrash's first frame immediately.
    neptune_death_thrash();
}

// 0x0043fbd0 - the thrash. Swims hard (0x800) and, when the animation ends,
// spins the shark a full half-turn - `(angle + 0x800) & 0xFFF`, masked, so it
// stays inside the 12-bit angle space.
static void neptune_death_thrash(void)
{
    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x200) != 0) {
        ENTITY->action_behavior++;
        ENTITY->move_speed_current = 0x78;
        ENTITY->animationId = 0;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3F;
        ENTITY->angle = (short)((unsigned short)(ENTITY->angle + 0x800) & 0xFFF);
    }
    neptune_clear_hit_state();
    Add_speedXZ(0x800);
}

// 0x0043fc70 - sink away, then RESET to state 1. Like the wasp's death, this is
// a respawn path, not a removal: the shark comes back as a live entity with a
// cleared hit latch. The DWORD write is a plain 1 (action_state 0), so it does
// re-run neptune_swim_begin.
static void neptune_death_sink(void)
{
    g_playerPosScratch.x = (int)ENTITY->player_pos_x;
    g_playerPosScratch.z = (int)ENTITY->player_pos_z;
    entity_rotate_toward_target(PLAYER_T, 0xFFE0);
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
    Add_speedXZ(0);

    ENTITY->action_ticks_counter--;
    if (ENTITY->action_ticks_counter == 0) {
        NE_STATE_BLOCK = 1;
        ENTITY->blend_counter = 0x3F;
        ENTITY->hit_state = 0;
    }
    neptune_clear_hit_state();
}

// ---- the aquarium boss death --------------------------------------------

// 0x0043fd10 - the boss's five-stage beaching, indexed by action_behavior.
static void neptune_death_boss(void)
{
    ((void (*)(void))neptune_boss_death_actions[ENTITY->action_behavior])();
}

// 0x0043fd30 - raise the room event flag that tells the script the boss is
// dead, spawn the big one-shot effect, and fall straight into the first charge.
static void neptune_boss_death_begin(void)
{
    Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
    int joints = (int)ENTITY->jointsStructs;

    ENTITY->action_behavior++;
    ENTITY->move_speed_current = 0x96;

    neptune_seed_from_dead_move();
    g_playerPosScratch.x = -100;
    Effect_CreateBillboard(0, 0, 3, (void*)(joints + 0x3A8), &g_playerPosScratch, 0);

    ENTITY->action_ticks_counter = 0x2D;
    ENTITY->animationId = 2;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control = 0;
    ENTITY->blend_counter = 0x3F;
    Snd_em(4);

    neptune_boss_death_charge();
}

// 0x0043fe10 - the first charge. Between frames 13 and 24 the shark surges:
// 0xC00 when it is already lined up, 0x400 while it is still turning. 0xFFF8
// is -8.
static void neptune_boss_death_charge(void)
{
    entity_rotate_toward_target(PLAYER_T, 0xFFF8);

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40) != 0) {
        ENTITY->action_behavior++;
        ENTITY->animationId = 3;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3F;
        ENTITY->move_speed_current = 100;
    }

    if (ENTITY->animation_frame_id > 0xC && ENTITY->animation_frame_id < 0x19) {
        ENTITY->angle = (short)(ENTITY->angle + 0x400);
        int aligned = turn_toward_target(PLAYER_T, 0x400);
        ENTITY->angle = (short)(ENTITY->angle - 0x400);
        Add_speedXZ(((short)aligned != 0) ? 0x400 : 0xC00);
    }
}

// 0x0043fef0 - the second charge. Same shape, but the surge is UNCONDITIONAL
// (no frame window) and the speed is zeroed at the end rather than set to 100.
static void neptune_boss_death_charge2(void)
{
    entity_rotate_toward_target(PLAYER_T, 0xFFF8);

    if (Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40) != 0) {
        ENTITY->action_behavior++;
        ENTITY->animationId = 4;
        ENTITY->animation_frame_id = 0;
        ENTITY->timing_control = 0;
        ENTITY->blend_counter = 0x3F;
        ENTITY->move_speed_current = 0;
        ENTITY->action_ticks_counter = 0x2D;
    }

    ENTITY->angle = (short)(ENTITY->angle + 0x400);
    int aligned = turn_toward_target(PLAYER_T, 0x400);
    ENTITY->angle = (short)(ENTITY->angle - 0x400);
    Add_speedXZ(((short)aligned != 0) ? 0x400 : 0xC00);
}

// 0x0043ffd0 - beached. Waits out the timer while the death cry repeats, then
// recolours the shadow quad to 0x00FFFF50 and SHRINKS it by 100 - the carcass
// settling. `&DAT_00ffff50` in Ghidra is the colour immediate, not a pointer.
static void neptune_boss_death_beach(void)
{
    if (ENTITY->action_state == 0) {
        if (ENTITY->animation_frame_id == 0) {
            Snd_em(4);
        }
        ENTITY->action_ticks_counter--;
        // A SIGNED test: the timer is allowed to go past zero and the stage
        // only advances on the frame the animation also happens to be at 8.
        if ((short)ENTITY->action_ticks_counter < 0 && ENTITY->animation_frame_id == 8) {
            ENTITY->action_state++;
            ENTITY->animationId = 5;
            ENTITY->animation_frame_id = 0;
            ENTITY->timing_control = 0;
            ENTITY->blend_counter = 0x3F;
            ENTITY->action_ticks_counter = 0x46;
        }
    } else if (ENTITY->action_state == 1) {
        ENTITY->action_behavior++;
        ENTITY->action_state = 0;
        ENTITY->animationId = 5;
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00FFFF50);
        BillboardAdjSize(&ENTITY->pushVelocity, -100, -100);
        ENTITY->status_flags |= 2;
        ENTITY->action_ticks_counter = 0x5A;
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
}

// 0x00440110 - fade out. Health is set to -1 as a WORD, and the quad grows 6
// per frame until the timer runs out. Nothing ever leaves this stage: the
// script removes the entity.
static void neptune_boss_death_fade(void)
{
    if (ENTITY->action_state == 0) {
        ENTITY->health = -1;
        BillboardAdjSize(&ENTITY->pushVelocity, 6, 6);
        ENTITY->action_ticks_counter--;
        if (ENTITY->action_ticks_counter == 0) {
            ENTITY->action_state++;
        }
    }
    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x40);
}

// ============================================================================
// States 3, 4 and 5 @ 0x00440190 / 0x004401a0 / 0x004401b0
// Each is a single RET in the original (body_size 1). Real slots that do
// nothing - the same shape as the Tyrant's FUN_004259f0.
// ============================================================================
static void neptune_state_3(void) { }
static void neptune_state_4(void) { }
static void neptune_state_5(void) { }

// ============================================================================
// neptune_update @ 0x0043d8d0 - enemies_update_functions_tbl[11]
//
// The gate is an OR, so it reads backwards at a glance: the body runs when the
// state is 0 **or** behavior_flags bit 7 is clear. Only a spawned shark
// (state != 0) that also carries 0x80 is skipped entirely.
//
// The collision probe is done at a point 800 units AHEAD of the shark and then
// rolled back, so the body is pushed out of geometry by its nose rather than
// its centre. The result is OR'd into splatter_flag (the wander-turn control
// byte) and check_room_collision's own output word is latched for next frame's
// entity_update_wander_turn.
// ============================================================================
void neptune_update(void)
{
    if (ENTITY->state != 0 && (ENTITY->behavior_flags & 0x80) != 0) {
        return;
    }

    if ((g_message_flags & 4) != 0) {
        ((void (*)(void))neptune_state_table[ENTITY->state])();

        SetEntityScaHitData(ENTITY);
        ResolveEntityScaCollision((Entity*)&g_playerEntityPointer, ENTITY);
        HandleEnemyPlayerCollisions();

        // Probe 800 units along the shark's own facing.
        VECTOR probe;
        const int* seed = (const int*)((char*)g_deadMoveValue + 0x14);
        probe.y   = seed[1];
        probe.z   = seed[2];
        probe.pad = seed[3];
        probe.x   = 800;
        RotMatrix(NE_ROT, &g_matrixScratch);
        ApplyMatrixLV(&g_matrixScratch, &probe, &probe);

        NE_POS->x += probe.x;
        NE_POS->z += probe.z;
        ENTITY->splatter_flag |= check_room_collision(NE_POS, 600);
        NE_ROOM_HIT = (unsigned short)(uintptr_t)g_tempVar;
        NE_POS->x -= probe.x;
        NE_POS->z -= probe.z;
    }

    ENTITY->has_enter_switch_zone =
        (unsigned char)is_entity_in_switch_zone(NE_POS, g_CurrentRdtDataTypePtr);
    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite(NE_POS, (short*)&ENTITY->pushVelocity, 0, ENTITY->angle);
    }
}
