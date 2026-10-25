// ComputerArms.cpp - em1014 / em1015, the player's forearms at the lab terminal.
//
// Entity types 20 and 21 (0x00427330 and 0x0040b760 in the original) are not
// enemies: they are the two hands you see typing while the room-5060 computer
// state is up. They are spawned into enemy slots 0 and 1 by the room script and
// driven entirely by ComputerLab.cpp, which writes a command into
// Entity::behavior_flags and polls bit 0x20 for "command finished".
//
// Command protocol (behavior_flags):
//   bit 0x80  hidden / parked - the driver does nothing
//   bit 0x40  new command; the driver consumes it, clears it, and resets the
//             sub-state so the command starts from the top
//   bit 0x20  set BY the arm when the command completes; the terminal polls it
//   bits 0-3  the command index
//
// Dispatch. Each arm has its OWN set of tables: em1014 at 0x004ba5a0
// (state), 0x004ba5a8 (commands) and 0x004ba5c8 (the shared step chain), and
// em1015 at 0x004b3418 / 0x004b3420 / 0x004b3448. They are not the same code
// with different constants - commands 4..7 differ outright, and the reaches
// use per-arm offsets. arm_state_run dispatches on entity id; everything from
// here to the em1015 block below describes em1014.
//
// Commands 1..7 each point at a tiny stub that re-dispatches on the sub-state
// into the chain at a DIFFERENT BASE - verified from the jump instructions:
//   cmd 1 -> [ecx*4 + 0x4ba5c8]   chain + 0
//   cmd 2 -> [ecx*4 + 0x4ba5d0]   chain + 2
//   cmd 3 -> [ecx*4 + 0x4ba5d8]   chain + 4
//   cmd 4 -> [ecx*4 + 0x4ba5e0]   chain + 6
//   cmd 5 -> [ecx*4 + 0x4ba5f0]   chain + 10   (Jill skips this command)
//   cmd 6 -> [ecx*4 + 0x4ba600]   chain + 14
//   cmd 7 -> [ecx*4 + 0x4ba608]   chain + 16
// so a command is just an entry point into one long sequence of steps, and the
// odd chain slots are the shared "interpolate and wait for the animation" step.
//
// em1014's command table stops at index 7: 0x004ba5a8 + 8*4 is exactly
// 0x004ba5c8, the first entry of the chain, so an index of 8 falls off the end
// of the table into the chain. That is NOT a ninth command - em1014 is only ever
// sent commands 0..7. Command 8 belongs to em1015, which has its own ninth
// entry and its own 3-step chain; see s_armLeverChain.
//
// Slot assignment. The two arms are NOT interchangeable: the terminal sends
// command 8 to slot 0 only, and only em1015 implements it, so slot 0 is em1015
// (entity id 21) and slot 1 is em1014 (id 20).
// ============================================================================
#include "../../Globals.h"
#include <cstdlib>                   // rand() - MSVC got this via <windows.h>
#include "../Types.h"
#include "../Entities.h"
#include "EntityCommon.h"

extern int  is_entity_in_switch_zone(VECTOR* position, void* zoneData);   // 0x00462d90
extern void ResetJointTransforms(void);                                   // EntityModelLoader.cpp
extern void play_sound_and_voice_effect(int bank, int id);

// Fields the generic Entity layout names for other enemies but that the arms
// use as fixed-point motion state. Addressed by offset on purpose - see the
// "offset writes, not nearest field" rule; the nearest named fields here
// (attacking_direction at 0x16C, splatter_flag at 0x174) are bytes.
#define ARM_VEL_X(e)  (*(int*)((unsigned char*)(e) + 0x16C))
#define ARM_VEL_Z(e)  (*(int*)((unsigned char*)(e) + 0x170))
#define ARM_POS_X(e)  (*(int*)((unsigned char*)(e) + 0x174))   // 16.16
#define ARM_POS_Z(e)  (*(int*)((unsigned char*)(e) + 0x178))   // 16.16
#define ARM_SCA_00(e) (*(unsigned int*)((unsigned char*)(e) + 0x1C))

// em1015-only scratch. The original gives the left arm a 3D fixed-point layout
// (vel 0x16C/0x170/0x174, pos 0x178/0x17C/0x180) because its command 7 drops
// the hand to the floor; the port keeps em1014's 2D layout for both arms and
// borrows two enemy-only fields for the Y pair. Nothing but this file reads
// them, so the aliasing is invisible - same reasoning as the "fields are
// private scratch" note below.
#define ARM_Y(e)      (*(int*)((unsigned char*)(e) + 0x17C))   // 16.16
#define ARM_VEL_Y(e)  (*(int*)((unsigned char*)(e) + 0x180))   // 16.16
// 0x0040bb90 stores the "keep integrating until this animation frame" gate as a
// WORD at +0x184, which also clears the byte at +0x185; arm_step_typing15 reads
// it back signed. state_mirror/ignore_player_flag_mirror are zombie-only.
#define ARM_GATE(e)   (*(short*)((unsigned char*)(e) + 0x184))

#define ARM_X(e)      ((e)->scaMatrixData.localMatrix.t[0])
#define ARM_Z(e)      ((e)->scaMatrixData.localMatrix.t[2])
#define ARM_HOME_X(e) ((e)->scd_pos_x)      // 0xCC
#define ARM_HOME_Z(e) ((e)->scd_pos_z)      // 0xD4
#define ARM_SUB(e)    ((e)->ignore_player_flag)   // 0x85
#define ARM_STEP(e)   ((e)->action_behavior)      // 0x86
#define ARM_TIMER(e)  (*(short*)&(e)->action_ticks_counter)  // 0xC4, read signed

// Is the player Jill? Several steps have a different reach and a different
// voice line for her. 0x00be62e5 is g_playerEntity.id.
static inline bool arm_is_jill(void) { return (g_playerEntity.id & 1) != 0; }

// ============================================================================
// 0x00427e80 - aim the fixed-point velocity so the hand covers (from -> to) in
// `frames` frames.
// ============================================================================
static void arm_set_velocity(Entity* e, int fromX, int fromZ, int toX, int toZ,
                             unsigned int frames)
{
    const int n = (int)(frames & 0xffff);
    if (n == 0) { ARM_VEL_X(e) = 0; ARM_VEL_Z(e) = 0; return; }
    ARM_VEL_X(e) = ((toX - fromX) * 0x10000) / n;
    ARM_VEL_Z(e) = ((toZ - fromZ) * 0x10000) / n;
}

// ============================================================================
// 0x00427da0 - start a 15-frame reach to (home + dx, home + dz) and play the
// matching arm animation. Animations 2 and 3 are the two "reach across"
// variants; if the arm is already in one of them the other is chosen so the
// hand does not snap.
// ============================================================================
static void arm_begin_reach(Entity* e, int dx, int dz, char anim)
{
    const char cur = (char)e->animationId;
    if (anim == 2 && cur == 1)      anim = 3;
    else if (anim == 3 && cur == 3) anim = 2;

    e->animationId        = (unsigned char)anim;
    e->animation_frame_id = 0;
    e->timing_control     = 0;
    e->blend_counter      = 7;

    ARM_VEL_X(e) = ((ARM_HOME_X(e) - ARM_X(e) + dx) * 0x10000) / 15;
    ARM_VEL_Z(e) = ((ARM_HOME_Z(e) - ARM_Z(e) + dz) * 0x10000) / 15;
    arm_set_velocity(e, ARM_X(e), ARM_Z(e),
                     ARM_HOME_X(e) + dx, ARM_HOME_Z(e) + dz, 15);
}

// 0x00427ed0 - advance the 16.16 position by the current velocity.
static void arm_integrate(Entity* e)
{
    ARM_POS_X(e) += ARM_VEL_X(e);
    ARM_POS_Z(e) += ARM_VEL_Z(e);
    ARM_X(e) = ARM_POS_X(e) >> 16;
    ARM_Z(e) = ARM_POS_Z(e) >> 16;
}

static inline unsigned int arm_joint_move(Entity* e, short blend)
{
    return Joint_move(0, e->animHeader, e->animBase, blend);
}

static void arm_set_anim(Entity* e, unsigned char id, unsigned char frame,
                         unsigned char blend)
{
    e->animationId        = id;
    e->animation_frame_id = frame;
    e->timing_control     = 0;
    e->blend_counter      = blend;
}

static void arm_command_done(Entity* e)
{
    e->behavior_flags |= 0x20;
    ARM_SUB(e) = 0;
}

// ============================================================================
// The shared step chain (0x004ba5c8)
//
// Commands 1..3 are the three typing reaches. Their offsets are PER-ARM: the
// left arm's table (0x0040b9e0 / 0x0040baf0 / 0x0040bb40) mirrors the right
// arm's Z and lengthens X, which is what keeps each hand on its own half of the
// keyboard - the two arms spawn 700 units apart in Z (ROOM5060's enemy_set
// records: em1015 at z 8322, em1014 at z 9022). The animation index is chosen
// per CHARACTER at every site with the same `SBB AL,AL; ADD AL,K` trick, so
// Chris gets K-1 and Jill K:
//   reach A (cmd 1): anim 1 for both       (0x004275e0 / 0x0040b9e0)
//   reach B (cmd 2): Chris 1, Jill 2       (0x004276f0 / 0x0040baf0)
//   reach C (cmd 3): Chris 2, Jill 3       (0x00427740 / 0x0040bb40)
// ============================================================================

struct ArmReach { short dx, dz; };

static const ArmReach s_reach1014[3] = {     // em1014, entity id 20
    { -0x28,  0x28 }, { 0x28, 0 }, { -0x14, -0x28 },
};
static const ArmReach s_reach1015[3] = {     // em1015, entity id 21
    { -0x32, -0x32 }, { 0x32, 0 }, { -0x14,  0x32 },
};

// K - (Chris ? 1 : 0), with K = 1, 2, 3 for commands 1, 2, 3.
static char arm_reach_anim(int index)
{
    if (index == 0) return 1;
    if (index == 1) return (char)(arm_is_jill() ? 2 : 1);
    return (char)(arm_is_jill() ? 3 : 2);
}

static void arm_step_reach(Entity* e, int index)
{
    ARM_SUB(e)++;
    const ArmReach* r = (e->id == 21) ? &s_reach1015[index] : &s_reach1014[index];
    arm_begin_reach(e, r->dx, r->dz, arm_reach_anim(index));
}

// chain[0] (0x004275e0 / 0x0040b9e0) - reach left-and-forward.
static void arm_step_reach_a(Entity* e) { arm_step_reach(e, 0); }

// chain[2] (0x004276f0 / 0x0040baf0) - reach right.
static void arm_step_reach_b(Entity* e) { arm_step_reach(e, 1); }

// chain[4] (0x00427740 / 0x0040bb40) - reach left-and-back.
static void arm_step_reach_c(Entity* e) { arm_step_reach(e, 2); }

// chain[6] (caseD_e) - the log-in gesture: settle onto the keyboard and say
// the line. g_main_state_flags bit 0x20000 is the handshake with the terminal's
// logo/voice sequence; the arm raises it and waits for it to clear.
static void arm_step_login(Entity* e)
{
    ARM_SUB(e)++;
    ARM_STEP(e) = 0;
    arm_set_anim(e, 4, 0, 7);

    if (!arm_is_jill()) {
        arm_set_velocity(e, ARM_X(e), ARM_Z(e), ARM_X(e), ARM_HOME_Z(e) + 200, 0x12);
        play_sound_and_voice_effect(1, 0xb8);
    } else {
        play_sound_and_voice_effect(1, 0xb9);
    }
    g_main_state_flags |= MSF_VOICE_PLAYING;
}

// Forward declarations for the two steps chain[7] re-dispatches into.
static void arm_step_press(Entity* e);
static void arm_step_wait_voice(Entity* e);

// chain[7] (caseD_f) - Jill just plays the animation out; Chris re-dispatches
// on ARM_STEP into the press/wait pair.
static void arm_step_typing(Entity* e)
{
    if (arm_is_jill()) {
        if (ARM_STEP(e) == 0) {
            if (arm_joint_move(e, 0x200) != 0) ARM_STEP(e)++;
        } else if ((g_main_state_flags & MSF_VOICE_PLAYING) == 0) {
            arm_command_done(e);
        }
        return;
    }

    if (ARM_STEP(e) == 0) arm_step_press(e);
    else                  arm_step_wait_voice(e);
}

// chain[8] (0x00427870) - the keypress itself. Frames 0x15 and 0x22 are the
// two contact points in the animation.
static void arm_step_press(Entity* e)
{
    const unsigned char frame = e->animation_frame_id;
    if (frame < 0x12) arm_integrate(e);
    if (frame == 0x15 || frame == 0x22) play_sfx(2, 0x1a, 0);

    if (arm_joint_move(e, 0x200) != 0) {
        ARM_STEP(e)++;
        ARM_TIMER(e) = 4;
    }
}

// chain[9] (0x004278e0)
static void arm_step_wait_voice(Entity* e)
{
    if ((g_main_state_flags & MSF_VOICE_PLAYING) == 0) arm_command_done(e);
}

// chain[10] (0x00427980)
static void arm_step_lift(Entity* e)
{
    ARM_SUB(e)++;
    ARM_TIMER(e) = 4;
    arm_set_velocity(e, ARM_X(e), ARM_Z(e), ARM_HOME_X(e), ARM_HOME_Z(e) + 0x50, 4);
}

// chain[11] (0x004279d0)
static void arm_step_lift_wait(Entity* e)
{
    arm_integrate(e);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) == 0) {
        ARM_SUB(e)++;
        arm_set_anim(e, 5, 0, 7);
    }
}

// chain[12] (0x00427a30)
static void arm_step_withdraw(Entity* e)
{
    if (!arm_is_jill() && e->animation_frame_id == 0x0f) play_sfx(2, 0x1c, 0);

    if (arm_joint_move(e, 0x200) != 0) {
        ARM_SUB(e)++;
        ARM_TIMER(e) = 4;
        arm_set_velocity(e, ARM_X(e), ARM_Z(e), ARM_HOME_X(e), ARM_HOME_Z(e), 4);
        arm_set_anim(e, 0, 0, 7);
    }
}

// chain[13] (0x00427b00)
static void arm_step_settle(Entity* e)
{
    arm_integrate(e);
    arm_joint_move(e, 0x200);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) == 0) arm_command_done(e);
}

// chain[14] (0x00427b80) - em1014 only; em1015's command 6 reaches down to the
// lever instead (arm15_cmd6_reach). Anim: Chris 2, Jill 3.
static void arm_step_reach_far(Entity* e)
{
    ARM_SUB(e)++;
    arm_begin_reach(e, -0x14, 0xa0, (char)(arm_is_jill() ? 3 : 2));
}

// chain[16] (0x00427bd0)
static void arm_step_hold(Entity* e)
{
    ARM_SUB(e)++;
    ARM_TIMER(e) = 0x96;
    e->blend_counter = (unsigned char)((e->animationId == 0) ? 0 : 7);
    arm_set_anim(e, 0, 0, e->blend_counter);
}

// chain[17] (0x00427c30)
static void arm_step_hold_wait(Entity* e)
{
    arm_joint_move(e, 0x200);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) != 0) return;

    ARM_SUB(e)++;
    e->animationId        = 4;
    e->animation_frame_id = (unsigned char)(arm_is_jill() ? 0 : 0x28);
    e->timing_control     = 0;
    e->blend_counter      = 0x1f;
    ARM_TIMER(e)          = 0x18;
    arm_set_velocity(e, ARM_X(e), ARM_Z(e),
                     ARM_HOME_X(e) - 100,
                     ARM_HOME_Z(e) + 0x8c + (arm_is_jill() ? 0 : -0x46),
                     0x18);
}

// chain[18] (0x00427d10)
static void arm_step_hold_move(Entity* e)
{
    arm_joint_move(e, 0x80);
    if (!arm_is_jill())                    e->animation_frame_id = 0x28;
    else if (e->animation_frame_id > 5)    e->animation_frame_id = 5;

    ARM_TIMER(e)--;
    if (ARM_TIMER(e) != 0) { arm_integrate(e); return; }
    ARM_SUB(e)++;
}

// chain[19] (0x00427d90) - terminal state, nothing to do.
static void arm_step_idle_end(Entity* /*e*/) { }

// ============================================================================
// Command 8 - em1015 ONLY (0x0040c350 -> its own 3-step chain at 0x004b34b0).
//
// This is the arm reaching DOWN off the keyboard to the release lever after the
// door menu has been confirmed, and it is the command the terminal sends to
// slot 0 from cl_menu_wait_close when the SECOND door is picked.
//
// em1014's command table (0x004ba5a8) only has entries 0..7: index 8 lands on
// 0x004ba5c8, which is the FIRST ENTRY OF ITS SHARED STEP CHAIN, because the
// two tables abut. So on em1014 a command 8 re-runs arm_step_reach_a every
// frame forever and NEVER raises the 0x20 "done" bit. em1015's table has a real
// ninth entry followed by a NULL terminator, and its own chain, so command 8 is
// exclusively em1015's - which is what identifies slot 0 as em1015 (entity id
// 21) and slot 1 as em1014 (id 20): slot 0 is the only slot the terminal ever
// sends 0x48 to. Routing command 8 through em1014's tables is what hung the
// terminal on the second door.
// ============================================================================

// chain8[0] (0x0040c370) - start the reach. The 15-frame velocity that
// arm_begin_reach installs is immediately replaced by a 4-frame drop, so only
// the animation (6) and the blend it sets survive.
static void arm_step_lever_reach(Entity* e)
{
    ARM_SUB(e)++;
    arm_begin_reach(e, -0x28, 0, 6);
    arm_set_velocity(e, ARM_X(e), ARM_Z(e),
                     ARM_HOME_X(e) - 0x28, ARM_HOME_Z(e) - 0x8c, 4);
    ARM_TIMER(e) = 4;
}

// chain8[1] (0x0040bf70) - glide down, then restart animation 6 from frame 0
// as the pull proper.
static void arm_step_lever_move(Entity* e)
{
    arm_integrate(e);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) == 0) {
        ARM_SUB(e)++;
        arm_set_anim(e, 6, 0, 7);
    }
    arm_joint_move(e, 0x200);
}

// chain8[2] (0x0040c040) - play it out. Note this step raises 0x20 WITHOUT
// clearing the sub-state (0x0040c06b is a bare `or byte [eax+2],0x20`, not the
// arm_command_done pair) - arm_state_run resets it when the next 0x40 arrives.
static void arm_step_lever_pull(Entity* e)
{
    if (arm_joint_move(e, 0x200) != 0) e->behavior_flags |= 0x20;
    if (e->animation_frame_id == 9) play_sfx(2, 0x18, 0);
}

// ============================================================================
// em1015's own chains (0x004b3418).
//
// The left arm is NOT the right arm with mirrored constants: commands 4..7 are
// entirely different chains, and command 5 is the one that gives Jill a success
// gesture at all (em1014's command 5 returns immediately for her). The port
// drove both arms through em1014's tables, which is what left the left hand
// sitting in the right hand's poses while typing and left Jill with no hand
// gesture when the terminal accepted or rejected her password.
// ============================================================================

// em1015 chain[6] (0x0040bb90) - the log-in gesture. No voice line and no
// msf 0x20000 handshake: those belong to em1014, which is the arm the terminal
// speaks through. This one slides the hand down over `gate` frames instead -
// home_z - 200 for Jill, - 300 for Chris - and the same gate is the frame at
// which arm_step_typing15 stops integrating.
static void arm_step_login15(Entity* e)
{
    ARM_SUB(e)++;
    ARM_STEP(e) = 0;
    arm_set_anim(e, 4, 0, 7);

    const int gate = arm_is_jill() ? 0x0a : 0x12;
    ARM_GATE(e) = (short)gate;
    arm_set_velocity(e, ARM_X(e), ARM_Z(e), ARM_X(e),
                     ARM_HOME_Z(e) - (arm_is_jill() ? 200 : 300),
                     (unsigned int)gate);
}

// em1015 chain[7] (0x0040bc30) - play animation 4 out. Integrates only while
// the frame is below the gate, so the hand stops travelling part way through
// the animation. Jill gets a click on frame 0x0d (em1014 uses 0x18/0x1a).
static void arm_step_typing15(Entity* e)
{
    if ((int)e->animation_frame_id < (int)ARM_GATE(e)) arm_integrate(e);

    if (arm_joint_move(e, 0x200) != 0) arm_command_done(e);

    if (arm_is_jill() && e->animation_frame_id == 0x0d) play_sfx(2, 0x1b, 0);
}

// em1015 chain[8] (0x0040bcb0) - command 4 never reaches it (arm_step_typing15
// ends the command itself), but it is in the table.
static void arm_step_settle15(Entity* e)
{
    arm_integrate(e);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) == 0) arm_command_done(e);
}

// ---------------------------------------------------------------------------
// Command 5 (0x004b3470) - the success gesture. em1014 does nothing here for
// Jill; em1015 gives both characters one, and they differ.
// ---------------------------------------------------------------------------

// chain[0] (0x0040bd10)
static void arm15_cmd5_begin(Entity* e)
{
    ARM_SUB(e)++;
    ARM_STEP(e) = 0;
}

// 0x0040beb0 - Jill's first step: straight into animation 5.
static void arm15_cmd5_jill_start(Entity* e)
{
    ARM_STEP(e)++;
    arm_set_anim(e, 5, 0, 7);
}

// 0x0040bd60 - Chris's first step: slide the hand down 0x50 over 4 frames.
static void arm15_cmd5_chris_slide(Entity* e)
{
    ARM_STEP(e)++;
    ARM_TIMER(e) = 4;
    arm_set_velocity(e, ARM_X(e), ARM_Z(e),
                     ARM_HOME_X(e), ARM_HOME_Z(e) - 0x50, 4);
}

// 0x0040bdb0 - then start animation 5.
static void arm15_cmd5_chris_wait(Entity* e)
{
    arm_integrate(e);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) == 0) {
        ARM_STEP(e)++;
        arm_set_anim(e, 5, 0, 7);
        ARM_TIMER(e) = 4;
    }
}

// 0x0040be50 - shared final step: drift out any remaining timer, play the
// animation out, done. Also em1015 command 5 sub-state 4.
static void arm15_cmd5_play(Entity* e)
{
    if (ARM_TIMER(e) != 0) {
        arm_integrate(e);
        ARM_TIMER(e)--;
    }
    if (arm_joint_move(e, 0x200) != 0) arm_command_done(e);
}

// 0x0040bd30 - command 5 sub-state 1 re-dispatches on ARM_STEP into the two
// per-character tables at 0x004b3478 (Chris) and 0x004b3488 (Jill).
static void arm15_cmd5_dispatch(Entity* e)
{
    const int step = (int)ARM_STEP(e);
    if (arm_is_jill()) {
        if (step == 0) arm15_cmd5_jill_start(e);
        else           arm15_cmd5_play(e);
        return;
    }
    if (step == 0)      arm15_cmd5_chris_slide(e);
    else if (step == 1) arm15_cmd5_chris_wait(e);
    else                arm15_cmd5_play(e);
}

// ---------------------------------------------------------------------------
// Command 6 (0x004b3490) - reach down-left and pull. Steps 1 and 2 are shared
// with command 8 (0x0040bf70 / 0x0040c040).
// ---------------------------------------------------------------------------

// 0x0040bf10 - like the lever reach but stopping at home_z - 0x28.
static void arm15_cmd6_reach(Entity* e)
{
    ARM_SUB(e)++;
    arm_begin_reach(e, -0x28, 0, 6);
    arm_set_velocity(e, ARM_X(e), ARM_Z(e),
                     ARM_HOME_X(e) - 0x28, ARM_HOME_Z(e) - 0x28, 4);
    ARM_TIMER(e) = 4;
}

// ---------------------------------------------------------------------------
// Command 7 (0x004b34a0) - hold, then drop the hand off the keyboard and let it
// hang, looping a per-character animation. Four steps: the sub-state stops at 3
// because arm15_cmd7_loop never advances it, so the entries past it in the
// original's table belong to command 8.
// ---------------------------------------------------------------------------

// 0x0040c0b0
static void arm15_cmd7_hold(Entity* e)
{
    ARM_SUB(e)++;
    ARM_TIMER(e) = 0xa0;
    e->blend_counter = (unsigned char)((e->animationId == 0) ? 0 : 7);
    arm_set_anim(e, 0, 0, e->blend_counter);
}

// 0x0040c110 - after the hold, seed the Y drop and start the 12-frame slide to
// (home_x - 100, home_z - 400).
static void arm15_cmd7_drop(Entity* e)
{
    arm_joint_move(e, 0x200);
    ARM_TIMER(e)--;
    if (ARM_TIMER(e) != 0) return;

    ARM_SUB(e)++;
    ARM_TIMER(e) = 0xc;
    ARM_Y(e)     = (int)((unsigned int)e->scaMatrixData.localMatrix.t[1] << 16);
    ARM_VEL_Y(e) = -0xC0000;
    arm_set_velocity(e, ARM_X(e), ARM_Z(e),
                     ARM_HOME_X(e) - 0x64, ARM_HOME_Z(e) - 0x190, 0xc);
}

// 0x0040c1c0 - gravity on Y while the slide runs, clamped to the spawn height
// (the "floor"), then animation 2 from frame 0x0e.
static void arm15_cmd7_fall(Entity* e)
{
    arm_joint_move(e, 0x80);

    if (ARM_TIMER(e) == 0) {
        ARM_SUB(e)++;
        e->scaMatrixData.localMatrix.t[1] = (int)(unsigned int)e->scd_pos_y;
        arm_set_anim(e, 2, 0x0e, 7);
        ARM_TIMER(e) = 0x5a;
        return;
    }

    arm_integrate(e);
    ARM_Y(e) += ARM_VEL_Y(e);
    ARM_VEL_Y(e) += 0x28000;

    int y = ARM_Y(e) >> 16;
    if (y > (int)(unsigned int)e->scd_pos_y) y = (int)(unsigned int)e->scd_pos_y;
    e->scaMatrixData.localMatrix.t[1] = y;
    ARM_TIMER(e)--;
}

// 0x0040c2b0 - the hanging loop. Joint_move only runs while the frame is under
// a per-character clamp, and the timer restarts the animation from frame 0.
static void arm15_cmd7_loop(Entity* e)
{
    const unsigned char clamp = (unsigned char)(arm_is_jill() ? 0x0a : 0x0e);
    if (e->animation_frame_id < clamp) arm_joint_move(e, 0x200);

    ARM_TIMER(e)--;
    if (ARM_TIMER(e) != 0) return;

    ARM_TIMER(e) = (short)(arm_is_jill() ? 0x0a : (rand() & 0x1f) + 0x0a);
    arm_set_anim(e, e->animationId, 0, 7);
}

// 0x0040b8e0 - command 0 for em1015. Same as em1014's except there is no
// "already within 200 units of home, skip the glide" shortcut.
static void arm15_cmd_rest(Entity* e)
{
    if (ARM_SUB(e) == 0) {
        ARM_SUB(e) = 1;
        e->blend_counter = (unsigned char)((e->animationId == 0) ? 0 : 7);
        arm_set_anim(e, 0, 0, e->blend_counter);
        ARM_TIMER(e) = 8;
        arm_set_velocity(e, ARM_X(e), ARM_Z(e), ARM_HOME_X(e), ARM_HOME_Z(e), 8);
    }
    if (ARM_TIMER(e) != 0) {
        arm_integrate(e);
        ARM_TIMER(e)--;
    }
    arm_joint_move(e, 0x200);
}

// chain[odd] (0x00427600) - the shared "interpolate and wait for the reach
// animation to finish" step. Completing the animation ends the command.
static void arm_step_move(Entity* e)
{
    ARM_POS_X(e) += ARM_VEL_X(e);
    ARM_POS_Z(e) += ARM_VEL_Z(e);
    ARM_X(e) = ARM_POS_X(e) >> 16;
    ARM_Z(e) = ARM_POS_Z(e) >> 16;

    if (arm_joint_move(e, 0x200) != 0) arm_command_done(e);

    // The keystroke click, on whichever animation frame makes contact.
    if (!arm_is_jill()) {
        if (e->animation_frame_id == 8) play_sfx(2, 0x18, 0);
    } else if (e->animation_frame_id == 3 || e->animation_frame_id == 13) {
        play_sfx(2, 0x18, 0);
    }
}

typedef void (*ArmStep)(Entity*);

// 0x004ba5c8. The odd slots between the three opening reaches, plus slot 15,
// are all the shared move step.
static const ArmStep s_armChain[20] = {
    arm_step_reach_a,     //  0
    arm_step_move,        //  1
    arm_step_reach_b,     //  2
    arm_step_move,        //  3
    arm_step_reach_c,     //  4
    arm_step_move,        //  5
    arm_step_login,       //  6
    arm_step_typing,      //  7
    arm_step_press,       //  8
    arm_step_wait_voice,  //  9
    arm_step_lift,        // 10
    arm_step_lift_wait,   // 11
    arm_step_withdraw,    // 12
    arm_step_settle,      // 13
    arm_step_reach_far,   // 14
    arm_step_move,        // 15
    arm_step_hold,        // 16
    arm_step_hold_wait,   // 17
    arm_step_hold_move,   // 18
    arm_step_idle_end,    // 19
};

// 0x004b34b0 - em1015's command-8 chain, indexed by the sub-state. Steps 1 and
// 2 are shared with its command 6.
static const ArmStep s_armLeverChain[3] = {
    arm_step_lever_reach,   // 0
    arm_step_lever_move,    // 1
    arm_step_lever_pull,    // 2
};

// 0x004b3448 - em1015's shared chain. 0..5 are the three reaches and their
// move steps, 6 the log-in gesture, 7 the typing play-out, 8 the settle step
// command 4 never reaches.
static const ArmStep s_arm15Chain[9] = {
    arm_step_reach_a,     // 0 0x0040b9e0
    arm_step_move,        // 1 0x0040ba00
    arm_step_reach_b,     // 2 0x0040baf0
    arm_step_move,        // 3
    arm_step_reach_c,     // 4 0x0040bb40
    arm_step_move,        // 5
    arm_step_login15,     // 6 0x0040bb90
    arm_step_typing15,    // 7 0x0040bc30
    arm_step_settle15,    // 8 0x0040bcb0
};

// 0x004b3470 - command 5, indexed by the sub-state. Only 0 and 1 are reachable
// that way; 2..4 are the same steps the ARM_STEP dispatch in sub-state 1 calls.
static const ArmStep s_arm15Cmd5[5] = {
    arm15_cmd5_begin,       // 0 0x0040bd10
    arm15_cmd5_dispatch,    // 1 0x0040bd30
    arm15_cmd5_chris_slide, // 2 0x0040bd60
    arm15_cmd5_chris_wait,  // 3 0x0040bdb0
    arm15_cmd5_play,        // 4 0x0040be50
};

// 0x004b3490 - command 6, indexed by the sub-state.
static const ArmStep s_arm15Cmd6[3] = {
    arm15_cmd6_reach,     // 0 0x0040bf10
    arm_step_lever_move,  // 1 0x0040bf70
    arm_step_lever_pull,  // 2 0x0040c040
};

// 0x004b34a0 - command 7, indexed by the sub-state. Sub-state 3 loops forever.
static const ArmStep s_arm15Cmd7[4] = {
    arm15_cmd7_hold,   // 0 0x0040c0b0
    arm15_cmd7_drop,   // 1 0x0040c110
    arm15_cmd7_fall,   // 2 0x0040c1c0
    arm15_cmd7_loop,   // 3 0x0040c2b0
};

// Entry point into s_arm15Chain per command 0..4.
static const signed char s_arm15CmdEntry[5] = {
    -1,   // 0  handled separately (arm15_cmd_rest)
     0,   // 1
     2,   // 2
     4,   // 3
     6,   // 4
};

// Entry point into s_armChain per command, from the dispatch stubs' jump
// instructions. -1 means "not a chained command".
static const signed char s_armCmdEntry[8] = {
    -1,   //  0  handled separately (return to rest)
     0,   //  1
     2,   //  2
     4,   //  3
     6,   //  4
    10,   //  5  Jill returns immediately
    14,   //  6
    16,   //  7
};

// ============================================================================
// Command 0 (0x004274a0) - return to rest and idle there.
// The original's command dispatch is a jump table at 0x00427483 whose cases
// are separate Ghidra functions; the port implements each as arm_cmd_* below:
//   caseD_1 0x004275c0  caseD_2 0x004276d0  caseD_3 0x00427720
//   caseD_4 0x00427770  caseD_5 0x00427960  caseD_6 0x00427b60
//   caseD_7 0x00427bb0
// ============================================================================
static void arm_cmd_rest(Entity* e)
{
    if (ARM_SUB(e) == 0) {
        ARM_SUB(e) = 1;
        e->blend_counter = (unsigned char)((e->animationId == 0) ? 0 : 7);
        e->animationId        = 0;
        e->animation_frame_id = 0;
        e->timing_control     = 0;
        ARM_TIMER(e)          = 8;
        arm_set_velocity(e, ARM_X(e), ARM_Z(e), ARM_HOME_X(e), ARM_HOME_Z(e), 8);

        // Already home (within 200 units, Manhattan): skip the glide.
        const int dz = ARM_Z(e) - ARM_HOME_Z(e);
        const int dx = ARM_X(e) - ARM_HOME_X(e);
        const int dist = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);
        if (dist < 200) ARM_TIMER(e) = 0;
    }

    if (ARM_TIMER(e) != 0) {
        arm_integrate(e);
        ARM_TIMER(e)--;
    }
    arm_joint_move(e, 0x200);
}

// ============================================================================
// 0x00427380 - state 0, one-shot init.
// ============================================================================
static void arm_state_init(Entity* e)
{
    e->state++;
    ARM_SUB(e)  = 0;
    e->health   = 1;
    e->status_flags = (unsigned char)((e->status_flags & 0x1f) | 4);
    e->animationId        = 0;
    e->animation_frame_id = 0;
    e->timing_control     = 0;
    e->blend_counter      = 0;

    ResetJointTransforms();
    arm_joint_move(e, 0x200);

    ARM_HOME_X(e) = ARM_X(e);
    ARM_HOME_Z(e) = ARM_Z(e);
    ARM_POS_X(e)  = ARM_X(e) << 16;
    ARM_POS_Z(e)  = ARM_Z(e) << 16;

    // em1015's init (0x0040b840) also records the spawn Y at +0xD0, which its
    // command-7 drop uses as the floor. em1014's init does not.
    if (e->id == 21) e->scd_pos_y = (int)e->scaMatrixData.localMatrix.t[1];
}

// ============================================================================
// 0x00427450 - state 1, the command driver.
// ============================================================================
static void arm_state_run(Entity* e)
{
    // 0x80 = parked, 0x20 = command already finished. Either way, idle.
    if ((e->behavior_flags & 0xa0) != 0) {
        ARM_SUB(e) = 0;
        return;
    }
    // 0x40 = a command the terminal has just issued; consume the latch.
    if ((e->behavior_flags & 0x40) != 0) {
        ARM_SUB(e) = 0;
        e->behavior_flags &= 0xbf;
    }

    const int cmd = e->behavior_flags & 0x0f;
    const int sub = (int)ARM_SUB(e);

    // The two arms are separate implementations, not one shared one: slot 0 is
    // em1015 (id 21) and runs the 0x004b3418 tables. It is also the only arm
    // with command 8, which is what identifies the slot - see the header.
    if (e->id == 21) {
        if (cmd == 0) { arm15_cmd_rest(e); return; }
        if (cmd == 5) { if (sub < 5) s_arm15Cmd5[sub](e); return; }
        if (cmd == 6) { if (sub < 3) s_arm15Cmd6[sub](e); return; }
        if (cmd == 7) { if (sub < 4) s_arm15Cmd7[sub](e); return; }
        if (cmd == 8) { if (sub < 3) s_armLeverChain[sub](e); return; }
        if (cmd > 4)  return;

        const int slot = s_arm15CmdEntry[cmd] + sub;
        if (slot >= 0 && slot < 9) s_arm15Chain[slot](e);
        return;
    }

    if (cmd == 0) { arm_cmd_rest(e); return; }

    // em1014 has no separate animation for command 5 when the player is Jill.
    if (cmd == 5 && arm_is_jill()) return;

    // em1014 has no command 8: its command table stops at 7 and index 8 lands
    // on the first entry of the step chain. The terminal only ever sends 0x48
    // to slot 0.
    if (cmd > 8) return;

    const int slot = s_armCmdEntry[cmd] + sub;
    if (slot < 0 || slot >= 20) return;
    s_armChain[slot](e);
}

// ============================================================================
// computer_arm_update (0x00427330 for em1014, 0x0040b760 for em1015)
//
// em1015 has its own tables at 0x004b3418 / 0x004b3420 / 0x004b3448 with the
// same shape (an init state, a driver, a command table and a shared chain),
// and its own step implementations at 0x0040b7b0..0x0040c4c0, all ported below.
// Its fixed-point scratch offsets differ from em1014's (see ARM_Y/ARM_VEL_Y);
// the port keeps em1014's 2D layout for both arms because the fields are
// private scratch and nothing else reads them.
// ============================================================================
void computer_arm_update(void)
{
    Entity* e = ENTITY;

    if (e->state == 0) arm_state_init(e);
    else               arm_state_run(e);

    ARM_SCA_00(e) = 0;

    if ((e->behavior_flags & 0x80) != 0) {
        e->has_enter_switch_zone = 0;
        return;
    }
    e->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)&e->scaMatrixData.localMatrix.t[0], g_CurrentRdtDataTypePtr);
}
