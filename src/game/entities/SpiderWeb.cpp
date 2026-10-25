// SpiderWeb.cpp - Spider web (entity type 19), the web that blocks the door
// in room 30C0.
//
// Original PC addresses:
//   spiderweb_update         0x00443640   per-frame entry (dispatch table [19])
//   state table              0x004bd9f0   5 entries, indexed by Entity+0x84
//     [0] spiderweb_init     0x004436c0
//     [1] spiderweb_idle     0x004437d0
//     [2] spiderweb_damaged  0x004437f0
//     [3] spiderweb_destroy  0x004438d0
//     [4] spiderweb_gone     0x00443940   (a bare RET)
//   SCA record pointers      0x004bd9e8 -> 0x004bd9c8 (Chris) / 0x004bd9d8 (Jill)
//
// It is not an enemy: it never moves, never attacks and has no behaviour table.
// All it does is stand in the doorway as a solid SCA cylinder and burn away
// under the weapons that can cut or burn it.
//
// How the "only knife / fire" rule works
// --------------------------------------
// apply_weapon_damage (0x0043c020) stores `weapon_id << 3` into the hit byte at
// Entity+0x8A, so `hit_state & 0xF8` recovers the weapon that landed the hit.
// spiderweb_damaged only reacts when that field is 0x08 (weapon 1, the knife) or
// greater than 0x28 (weapons 6 and up: flamethrower, the three grenade-launcher
// rounds and the rocket launcher). Weapons 2-5 - handgun, shotgun, python,
// magnum - fall through and are ignored.
//
// The damage table backs the same rule up: row 19 of g_weaponHitRecordsFirstRun
// (WeaponDamage.cpp) is 10 damage for the knife, 0 for weapons 2-5, 2 for the
// flamethrower and 30/30/30/900 for the launchers, against 0x37 = 55 health.
// So the knife takes six hits, a grenade round two, a rocket one, and bullets
// do literally nothing.
//
// Burning away
// ------------
// The web model has six joints. Each damage tick re-derives how many of them
// should be gone from the remaining health and clears their flags byte, which
// drops them out of render_entity's render loop (it gates on joint
// flags & 1). Joints disappear from the top down - 5 first, then 4, 3, 2, 1 -
// and the final destroy pass clears all six including joint 0.
//
// Once destroyed the web raises status_flags bit 1, which is the "intangible"
// bit ResolveEntityScaCollision tests, so the player walks through; and raises
// its death_event_id in g_EnemiesFlags, which is what the room script polls to
// let the door open.
#include "EntityCommon.h"
#include "../../Globals.h"

extern void ResetJointTransforms(void);                    // 0x0048bad0
extern void Flg_on(int baseAddr, unsigned int bitIndex);   // 0x00473ef0
extern int  is_entity_in_switch_zone(VECTOR* position, void* zoneData); // 0x00462d90 - Room.cpp

namespace {

// ---------------------------------------------------------------------------
// 0x004bd9c8 / 0x004bd9d8 - the two SCA collision records, one per playable
// character, selected by `g_playerEntityPointer.id & 1`. Six shorts in the
// layout SetEntityScaHitData walks: [0] id/terminator (bit 15 set = last
// record), [1..3] local x/y/z, [4] half-height, [5] radius.
//
// Only the local X offset differs between the two: the web sits 700 units to
// one side of the entity origin for Chris and 500 for Jill, because the two
// scenarios place the doorway entity differently. Half-height is 0, so the
// vertical overlap test is decided entirely by the player's own half-height.
//
// Sixteen bytes each in the original and contiguous, so they stay one object.
// ---------------------------------------------------------------------------
static const short s_spiderWebScaInfo[2][8] = {
    { (short)0x8000, (short)-700, 0, 0, 0, 500, 0, 0 },   // 0x004bd9c8 - Chris
    { (short)0x8000, (short)-500, 0, 0, 0, 500, 0, 0 },   // 0x004bd9d8 - Jill
};

// Joint stride, and the byte offset of joint 5's flags byte (5 * 0x7C). The
// original hard-codes 0x26C as the base of the damage loop's backwards walk.
const unsigned int JOINT_STRIDE   = 0x7C;
const unsigned int JOINT5_FLAGS   = 5 * JOINT_STRIDE;   // 0x26C
const int          SPIDERWEB_JOINTS = 6;

// The flags byte at joint+0x00. Clearing it drops the joint from the render
// loop in render_entity (0x0048c350), which gates on `flags & 1`.
static inline unsigned char& joint_flags(int index)
{
    return *((unsigned char*)ENTITY->jointsStructs + (unsigned int)index * JOINT_STRIDE);
}

// Entity+0x84 is written as one DWORD (state | ignore<<8 | behavior<<16 |
// action<<24) in both places the original touches it here.
static inline void set_state_word(unsigned int v)
{
    *(unsigned int*)((char*)ENTITY + 0x84) = v;
}

// ---------------------------------------------------------------------------
// spiderweb_init - state 0 (0x004436c0)
// ---------------------------------------------------------------------------
void spiderweb_init(void)
{
    set_state_word(1);                                  // -> state 1, everything else 0

    // The X and Z components of the rotation SVECTOR at Entity+0x72. These must
    // NOT be written through the nearest named fields: +0x72 is position.pad and
    // +0x76 is angle_z, and the yaw at +0x74 between them is the placement angle
    // the SCD script already wrote.
    *(short*)((char*)ENTITY + 0x72) = 0;
    *(short*)((char*)ENTITY + 0x76) = 0;

    ResetJointTransforms();

    // Ground shadow quad, built into the 120-byte block at Entity+0xE4. The
    // tint goes in the 0x00be0dfc scratch (g_animFrameIdSave) that
    // FUN_004565f0 copies into the quad header - 0xFFFFFF here, i.e. untinted.
    // 10 x 1000 is a long thin strip: the web spans the doorway.
    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_animFrameIdSave = 0x00FFFFFF;
    FUN_004565f0(&g_svecScratch, (SVECTOR*)&ENTITY->pushVelocity, 10, 1000);

    ENTITY->health = 0x37;                              // 55 - exactly six knife hits
    *(unsigned short*)ENTITY->pad_ca = 0x1000;          // joint world-matrix scale

    // 0x00443745: `MOV AL,[0x00be62e5]` - the player entity's id byte.
    ENTITY->Sca_info =
        (unsigned int)(uintptr_t)s_spiderWebScaInfo[g_playerEntityPointer.id & 1];
    SetEntityScaHitData(ENTITY);

    ENTITY->animationId         = 0;
    ENTITY->animation_frame_id  = 0;
    ENTITY->timing_control      = 0;
    ENTITY->blend_counter       = 0;

    Joint_move(0, ENTITY->animHeader, ENTITY->animBase, 0x400);
}

// ---------------------------------------------------------------------------
// spiderweb_idle - state 1 (0x004437d0)
//
// The whole idle state. Keeping bits 0-4 preserves the intangible bit (0x02)
// once spiderweb_destroy has raised it; forcing bit 6 (ENTITY_STATUS_ALIGNED)
// unconditionally is what makes the web a valid weapon target every frame -
// apply_weapon_damage's filter is `status_flags & player.flags & 0xE0`.
// ---------------------------------------------------------------------------
void spiderweb_idle(void)
{
    ENTITY->status_flags &= 0x1F;
    ENTITY->status_flags |= ENTITY_STATUS_ALIGNED;
}

// ---------------------------------------------------------------------------
// spiderweb_damaged - state 2 (0x004437f0)
//
// Entered by apply_weapon_damage whenever health survives the hit. Returns to
// state 1 immediately; the only lasting effect is burning joints away.
// ---------------------------------------------------------------------------
void spiderweb_damaged(void)
{
    JointStruct* joints = ENTITY->jointsStructs;
    set_state_word(1);                                  // straight back to idle

    // hit_state's top five bits are the weapon id shifted left by 3
    // (apply_weapon_damage: `hitState |= weapon_id << 3`). 0x08 is the knife;
    // anything above 0x28 is weapon 6 or higher - flamethrower, the three
    // grenade-launcher rounds, the rocket launcher. 0x10-0x28 (handgun through
    // magnum) falls through and leaves the web untouched.
    unsigned int weaponBits = (unsigned int)ENTITY->hit_state & 0xF8;
    if (weaponBits == 0x08 || weaponBits > 0x28) {
        // How much of the web has burned away, re-derived from health each
        // time rather than counted. 55 health, 10 per knife hit, so the five
        // thresholds land one joint per hit.
        short hp = ENTITY->health;
        int burnt = 0;
        if (hp < 0x32) burnt = 1;    // < 50
        if (hp < 0x28) burnt = 2;    // < 40
        if (hp < 0x1E) burnt = 3;    // < 30
        if (hp < 0x14) burnt = 4;    // < 20
        if (hp < 0x0A) burnt = 5;    // < 10

        // The original walks a decrementing counter and indexes
        // `joints + 0x26C - counter * 0x7C`, i.e. joint 5 downwards, so the
        // strands vanish from the top. Joint 0 is never cleared here - only
        // spiderweb_destroy takes it.
        for (int v = burnt - 1; v >= 0; v--) {
            *((unsigned char*)joints + JOINT5_FLAGS - (unsigned int)v * JOINT_STRIDE) = 0;
        }
    }

    // Cleared unconditionally, including for the ignored weapons: hit_state
    // must return to 0 or apply_weapon_damage's `candidate->hit_state == 0`
    // gate would never let the web be hit again.
    ENTITY->hit_state = 0;
}

// ---------------------------------------------------------------------------
// spiderweb_destroy - state 3 (0x004438d0)
//
// Entered by apply_weapon_damage when the hit drove health negative.
// ---------------------------------------------------------------------------
void spiderweb_destroy(void)
{
    for (int j = SPIDERWEB_JOINTS - 1; j >= 0; j--) {
        joint_flags(j) = 0;                             // the last strand goes too
    }

    // Bit 1 is the "intangible" bit ResolveEntityScaCollision (0x0041b0a0)
    // tests: `(entA->status_flags | entB->status_flags) & 2` returns no
    // collision. This is what stops the web blocking the doorway. state 1 only
    // masks with 0x1F, so the bit survives even though nothing runs state 1
    // again after this.
    ENTITY->status_flags |= 0x02;

    // The room script polls this flag to open the door.
    Flg_on((int)g_EnemiesFlags, ENTITY->death_event_id);

    set_state_word(4);                                  // -> the do-nothing state
}

// ---------------------------------------------------------------------------
// spiderweb_gone - state 4 (0x00443940)
//
// A bare RET in the original. The entity stays in the slot, invisible and
// intangible, until the room is unloaded.
// ---------------------------------------------------------------------------
void spiderweb_gone(void)
{
}

// 0x004bd9f0 - five entries plus a NULL sixth. `state` only ever holds 0-4
// (spiderweb_init and spiderweb_destroy write it, and apply_weapon_damage
// writes 2 or 3), so the NULL is unreachable; the bounds check below only
// guards against a corrupt slot.
void (* const s_spiderWebStates[6])(void) = {
    spiderweb_init,
    spiderweb_idle,
    spiderweb_damaged,
    spiderweb_destroy,
    spiderweb_gone,
    NULL,
};

} // namespace

// ============================================================================
// spiderweb_update @ 0x00443640 - enemies_update_functions_tbl[19]
//
// Unlike every other entity update this one has no g_message_flags gate: the
// state machine runs even while a message box is up. It costs nothing, since
// four of the five states do nothing on their own.
// ============================================================================
void spiderweb_update(void)
{
    unsigned char state = ENTITY->state;
    if (state < 6 && s_spiderWebStates[state] != NULL) {
        s_spiderWebStates[state]();
    }

    ENTITY->scaMatrixData.field_00 = 0;
    ENTITY->has_enter_switch_zone = (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)ENTITY->scaMatrixData.localMatrix.t, g_CurrentRdtDataTypePtr);

    if (ENTITY->has_enter_switch_zone != 0) {
        // yOffset is a literal 0 here (`PUSH 0x0` at 0x00443698), not the
        // entity's world Y the way character_npc_update passes it.
        entity_add_fade_sprite(
            (VECTOR*)ENTITY->scaMatrixData.localMatrix.t,
            (short*)&ENTITY->pushVelocity,
            0,
            ENTITY->angle);
    }
}
