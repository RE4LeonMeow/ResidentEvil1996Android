// Plant42Roots.cpp - Plant 42 roots (enemy type 14, model em100e).
//
// Original PC addresses:
//   plant42_roots_update    0x0047e1c0   per-frame entry (dispatch table [14])
//   state table             0x004c5a28   10 entries, indexed by Entity+0x84
//     [0] roots_init          0x0047e290
//     [1] roots_idle          0x0047e3b0
//     [2] roots_hit_reset     0x0047e720
//     [3] roots_none          0x0047e750   (bare RET)
//     [4] roots_none          0x0047e760   (bare RET)
//     [5] roots_none          0x0047e770   (bare RET)
//     [6] roots_phase_start   0x0047e4e0
//     [7] roots_phase_rise    0x0047e550
//     [8] roots_phase_shrink  0x0047e580
//     [9] roots_phase_sink    0x0047e640
//   move behaviour A        0x0047e400   idle sub-behaviour, behavior_flags == 0
//   move behaviour B        0x0047e6a0   retract sub-behaviour, behavior_flags 1..0x7F
//   SCA collision record    0x004c5a18   one terminator-flagged cylinder
//
// Spawn site
// ----------
// Only rooms 40F0 (Chris) and 40F1 (Jill) ever run cmd_enemy_set with type 14 -
// three mutually exclusive branches of the init script's nested if/else chain,
// all writing enemy SLOT 0 at (0x1A27, -5500, 0x0B50), yaw 0x0C00, two SCA
// records, death flag 0xFF. The branches differ only in the behavior_flags
// byte (Entity+2):
//   0x80  dormant  - state 1 skips both sub-behaviours, just runs Joint_move
//   0x01  retracted - behaviour B fires once on the first frame
// Nothing in the game rewrites behavior_flags afterwards (only cmd_enemy_set and
// the computer-lab arms ever write that field), so which branch fired at room
// load is what the entity does for its whole life. Behaviour A (the full
// rise/writhe/sink cycle below) is reachable only if something had set the
// field to 0 - transcribed faithfully even though no shipped room does.
//
// What the entity is
// ------------------
// A stationary root mass rendered through the ordinary entity path (its
// em100e joints animate through Joint_move every frame in state 1). On top of
// that it casts a ground shadow: state 0 builds the fade-sprite quad at
// Entity+0xE4 with FUN_004565f0 - half-size 0xC00 x 0xC00, offset (0, 0, -0x50),
// tint 0x404040 in the 0x00be0dfc scratch (grey texels over a dark tint, the
// same sole-colour-carrier rule as every kage-page consumer) - and the main
// update re-submits it through entity_add_fade_sprite every frame while the
// position sits inside the camera's switch zone. It also blocks the player
// (SetEntityScaHitData / ResolveEntityScaCollision / HandleEnemyPlayerCollisions
// every frame) and raises status bits 0xE0 each frame in state 1, which makes
// it a valid weapon target.
//
// Invulnerability-by-indifference
// -------------------------------
// apply_weapon_damage writes state 2 while health survives the hit and state 3
// when it kills. Here state 2 just resets back to idle (and clears the phase
// counter, restarting cycle A from phase 0 if it was mid-cycle), and states
// 3/4/5 are bare RETs - the roots cannot die. Health starts at 1 and both
// sub-behaviours slam it to -1, but nothing ever reads it.
//
// Cycle A (behaviour 0) - the four phases reuse state-table slots 6..9
// --------------------------------------------------------------------
// roots_move_a dispatches `table[6 + phase]()` for phase 0..3, so the phase
// handlers ARE main-table entries 6-9 (that overlap is why the table has ten
// entries although the state byte only ever reaches 5). Each handler bumps
// Entity+0x85 itself when it is done:
//   phase 0  start   one-shot setup: writhe velocity/amplitude cleared,
//                    flash countdown 6, intangible bit raised, health -1,
//                    Snd_em(0x18)
//   phase 1  rise    amplitude += 0x200/frame until it reaches 0x1600
//   phase 2  shrink  joint-scale word (Entity+0xCA) drops 8/frame from 0x1000;
//                    while above 0x9C4 the shadow shrinks 10/frame and every
//                    8th frame flashes scd_model_tint_apply(0,-1,-2) six times
//   phase 3  sink    amplitude bleeds off 0x70/frame, wobble word (Entity+0x17E,
//                    seeded 2 in init) ticks up every 4th frame, and once it
//                    bottoms out Flg_on(g_EnemiesFlags, death_event_id) fires
// While cycling, roots_move_a bobs the altitude: t[1] += velocity>>8, then the
// velocity reloads itself from +/-amplitude every frame (a violent ±22-unit
// shake at full amplitude). Two random root groans play: Play3DSnd(2, 0x19)
// when the 12..26-frame sound timer expires, and again on a 1-in-32 roll.
//
// The frozen position
// -------------------
// Init stores t[0]/t[2] into Entity+0x174/+0x178 (two DWORDS - splatter_flag +
// bob_speed and subpixel_pos_x in the port's struct). Whenever behavior_flags
// is non-zero the main update snaps the live position back to those, pinning
// the mass to its spawn point regardless of what anything else did to it.
#include "EntityCommon.h"
#include "../../Globals.h"
#include <cstdlib>

extern void ResetJointTransforms(void);                                    // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);                   // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* position, void* zoneData);    // 0x00462d90 - Room.cpp
// Both tint entry points take SIGNED 16-bit deltas and UNSIGNED 16-bit queue
// words - see the Plant42.cpp note. A byte-wide declaration truncates -1/-2/-6/-12.
extern void scd_model_tint_apply(short p1, short p2, short p3,
                                 unsigned short p4, unsigned short p5,
                                 unsigned char p6);                        // 0x00473b10
extern void FUN_00473d10(short p1, short p2, short p3,
                         unsigned short p4, unsigned short p5, char p6);   // 0x00473d10

namespace {

// ---------------------------------------------------------------------------
// Raw field access. These offsets are reused at widths the generic Entity
// fields do not have (words across byte pairs, dwords across pairs of bytes),
// same situation as Cerberus/Plant 42 - naming them through struct fields
// would misstate the width.
// ---------------------------------------------------------------------------
#define PR_HEALTH      (*(short*)          ((char*)ENTITY + 0x88))  // word, starts 1, slammed to -1
#define PR_SCALE       (*(unsigned short*) ((char*)ENTITY + 0xCA))  // joint world-matrix scale, 0x1000 = 1.0
#define PR_VELOCITY    (*(short*)          ((char*)ENTITY + 0x16C)) // writhe velocity (splatter_flag+bob_speed)
#define PR_AMPLITUDE   (*(short*)          ((char*)ENTITY + 0x16E)) // writhe amplitude (texBank+seq_counter)
#define PR_FLASHES     (*(short*)          ((char*)ENTITY + 0x170)) // remaining tint flashes (angle_turn_delta+move_timer)
#define PR_TICKS       (*(unsigned short*) ((char*)ENTITY + 0xC4))  // flash-interval counter, word
#define PR_STORED_X    (*(int*)            ((char*)ENTITY + 0x174))
#define PR_STORED_Z    (*(int*)            ((char*)ENTITY + 0x178))
#define PR_SND_TIMER   (*(short*)          ((char*)ENTITY + 0x17C)) // frames to next groan (action_speed+hit_threshold)
#define PR_WOBBLE      (*(short*)          ((char*)ENTITY + 0x17E)) // sink wobble tick (behavior_step+action_counter)

// Entity+0x85 doubles as the cycle-A phase counter, 0..4.
static inline unsigned char& pr_phase(void) { return ENTITY->ignore_player_flag; }

// Entity+0x84 written/read as one DWORD in init (state|ignore|beh|action).
static inline void set_state_word(unsigned int v)
{
    *(unsigned int*)((char*)ENTITY + 0x84) = v;
}

// ---------------------------------------------------------------------------
// 0x004c5a18 - the SCA collision record state 0 points Sca_info at. One
// terminator-flagged cylinder (bit 15 of word 0 set = last record), centred on
// the entity, radius 0x07D0, half-height 0x1770 - a fat two-metre trunk. em_set
// had already pooled two records from the shared table; this overwrite is why
// the pool advance still says two.
// ---------------------------------------------------------------------------
const short s_rootsScaInfo[8] = {
    (short)0x8000, 0, 0, 0, 0x1770, 0x07d0, 0, 0,
};

// ---------------------------------------------------------------------------
// Phase handlers - state-table entries 6..9, dispatched as table[6 + phase].
// ---------------------------------------------------------------------------

// roots_phase_start @ 0x0047e4e0 (entry 6)
void roots_phase_start(void)
{
    pr_phase() += 1;
    PR_VELOCITY = 0;
    PR_AMPLITUDE = 0;
    PR_TICKS = 8;
    PR_FLASHES = 6;
    ENTITY->status_flags |= 0x02;               // intangible
    PR_HEALTH = -1;
    Snd_em(0x18);
}

// roots_phase_rise @ 0x0047e550 (entry 7)
void roots_phase_rise(void)
{
    PR_AMPLITUDE = (short)(PR_AMPLITUDE + 0x200);
    if (PR_AMPLITUDE >= 0x1600) {
        PR_AMPLITUDE = 0x1600;
        pr_phase() += 1;
    }
}

// roots_phase_shrink @ 0x0047e580 (entry 8)
void roots_phase_shrink(void)
{
    if (PR_SCALE <= 0x9c4) {                    // JA in the original: unsigned compare
        pr_phase() += 1;
        PR_TICKS = 1;
        PR_FLASHES = 1;
        return;
    }

    BillboardAdjSize(&ENTITY->pushVelocity, -10, -10);
    PR_SCALE = (unsigned short)(PR_SCALE - 8);

    short ticks = (short)PR_TICKS;
    PR_TICKS = (unsigned short)(ticks - 1);
    if (ticks == 0 && PR_FLASHES != 0) {
        scd_model_tint_apply(0, -1, -2, 0, 0x100, ENTITY->id);
        PR_TICKS = 8;
        PR_FLASHES = (short)(PR_FLASHES - 1);
    }
}

// roots_phase_sink @ 0x0047e640 (entry 9)
void roots_phase_sink(void)
{
    PR_AMPLITUDE = (short)(PR_AMPLITUDE - 0x70);
    PR_TICKS = (unsigned short)(PR_TICKS + 1);
    if (((unsigned char)PR_TICKS & 3) == 0) {
        PR_WOBBLE = (short)(PR_WOBBLE + 1);
    }
    if (PR_AMPLITUDE <= 0) {
        pr_phase() += 1;
        // death_event_id is 0xFF here (em_set flag byte) - flag 255 of the room bank.
        Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);
    }
}

// ---------------------------------------------------------------------------
// roots_move_a @ 0x0047e400 - the behavior_flags == 0 sub-behaviour: run the
// current phase, then bob the altitude on the velocity/amplitude pair and
// roll the two groan sounds.
// ---------------------------------------------------------------------------
void roots_move_a(void)
{
    if (pr_phase() > 3) {                       // JA: unsigned, phases 4+ stop the cycle
        return;
    }

    static void (*const s_phaseDispatch[4])(void) = {
        roots_phase_start,
        roots_phase_rise,
        roots_phase_shrink,
        roots_phase_sink,
    };
    // The original CALLs [ECX*4 + 0x4c5a40], i.e. main-table slots 6..9 - the
    // same function pointers listed there. This local table is that slice.
    s_phaseDispatch[pr_phase()]();

    ENTITY->scaMatrixData.localMatrix.t[1] += PR_VELOCITY >> 8;   // SAR AX,8 then sign-extend
    short velocityFlip = PR_AMPLITUDE;
    if (PR_VELOCITY > 0) {
        velocityFlip = (short)-velocityFlip;
    }
    PR_VELOCITY = velocityFlip;

    PR_SND_TIMER = (short)(PR_SND_TIMER - 1);
    if (PR_SND_TIMER == 0) {
        Play3DSnd(2, 0x19, 0, (int)&ENTITY->scaMatrixData.localMatrix.t[0]);
        PR_SND_TIMER = (short)(((rand() & 7) + (rand() & 7)) + 0xc);
    }
    if ((rand() & 0x1f) == 1) {
        Play3DSnd(2, 0x19, 0, (int)&ENTITY->scaMatrixData.localMatrix.t[0]);
    }
}

// ---------------------------------------------------------------------------
// roots_move_b @ 0x0047e6a0 - the behavior_flags 1..0x7F sub-behaviour, the
// one the shipped rooms actually select (flags 0x01). One-shot: collapse the
// shadow to nothing, go intangible, darken the model a touch and park the
// scale word below the phase-2 exit threshold. Every later frame falls
// straight through the phase latch.
// ---------------------------------------------------------------------------
void roots_move_b(void)
{
    if (pr_phase() != 0) {
        return;
    }
    pr_phase() = 1;
    PR_SCALE = 0x9c4;
    ENTITY->status_flags |= 0x02;               // intangible
    PR_HEALTH = -1;
    FUN_00473d10(0, -6, -12, 0, 0x100, (char)ENTITY->id);
    BillboardAdjSize(&ENTITY->pushVelocity, -2000, -2000);
}

// ---------------------------------------------------------------------------
// State handlers proper.
// ---------------------------------------------------------------------------

// roots_init @ 0x0047e290 (state 0)
void roots_init(void)
{
    set_state_word(1);                          // -> state 1, ignore/beh/action zeroed

    PR_HEALTH = 1;
    ENTITY->Sca_info = (unsigned int)(uintptr_t)s_rootsScaInfo;

    ENTITY->status_flags &= 0x1F;
    ENTITY->status_flags |= 0x04;

    ENTITY->animationId        = 0;
    ENTITY->animation_frame_id = 0;
    ENTITY->timing_control     = 0;
    ENTITY->blend_counter      = 0;

    ResetJointTransforms();

    *(unsigned short*)ENTITY->pad_ca = 0x1000;  // full joint scale
    ENTITY->hit_state = 1;

    // Freeze the spawn point for the whole life of the entity.
    PR_STORED_X = (int)ENTITY->scaMatrixData.localMatrix.t[0];
    PR_STORED_Z = (int)ENTITY->scaMatrixData.localMatrix.t[2];

    *(short*)((char*)ENTITY + 0x172) = 0;
    PR_WOBBLE    = 2;
    PR_SND_TIMER = 0x28;                        // 40 frames to the first groan

    // Ground shadow quad: offset (0, 0, -0x50), dark-grey tint, 0xC00 x 0xC00.
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = -0x50;
    g_animFrameIdSave = 0x00404040;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 0xc00, 0xc00);
}

// roots_idle @ 0x0047e3b0 (state 1)
void roots_idle(void)
{
    ENTITY->status_flags |= 0xE0;               // weapon-targetable every frame

    if (ENTITY->behavior_flags != 0x80) {
        if (ENTITY->behavior_flags == 0) {
            roots_move_a();
        } else {
            roots_move_b();
        }
    }

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x100);
}

// roots_hit_reset @ 0x0047e720 (state 2) - entered by apply_weapon_damage on a
// survived hit; drops straight back to idle and rewinds the phase latch.
void roots_hit_reset(void)
{
    ENTITY->state     = 1;
    pr_phase()        = 0;
    ENTITY->hit_state = 0;
}

// roots_none @ 0x0047e750 / 0x0047e760 / 0x0047e770 (states 3..5) - bare RETs.
// apply_weapon_damage writes state 3 on a killing blow; the roots simply stay.
void roots_none(void)
{
}

// 0x004c5a28 - ten entries. States only ever written are 0-3 (init, idle,
// weapon damage, kill), and 6..9 double as the cycle-A phase slice, so all
// ten are live; the bounds check below only guards a corrupt slot.
void (*const s_rootsStateTable[10])(void) = {
    roots_init,
    roots_idle,
    roots_hit_reset,
    roots_none,
    roots_none,
    roots_none,
    roots_phase_start,
    roots_phase_rise,
    roots_phase_shrink,
    roots_phase_sink,
};

} // namespace

// ============================================================================
// plant42_roots_update @ 0x0047e1c0 - enemies_update_functions_tbl[14]
// ============================================================================
void plant42_roots_update(void)
{
    if ((g_message_flags & 4) != 0) {
        unsigned char state = ENTITY->state;
        if (state < 10 && s_rootsStateTable[state] != NULL) {
            s_rootsStateTable[state]();
        }

        SetEntityScaHitData(ENTITY);
        ResolveEntityScaCollision((Entity*)&g_playerEntityPointer, ENTITY);
        HandleEnemyPlayerCollisions();
    }

    // Non-zero behavior_flags pins the mass to the position init froze.
    if (ENTITY->behavior_flags != 0) {
        ENTITY->scaMatrixData.localMatrix.t[0] = PR_STORED_X;
        ENTITY->scaMatrixData.localMatrix.t[2] = PR_STORED_Z;
    }

    ENTITY->scaMatrixData.field_00 = 0;
    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0], g_CurrentRdtDataTypePtr);

    if (ENTITY->has_enter_switch_zone != 0) {
        entity_add_fade_sprite(
            (VECTOR*)&ENTITY->scaMatrixData.localMatrix.t[0],
            (short*)&ENTITY->pushVelocity,
            0,
            ENTITY->angle);
    }
}
