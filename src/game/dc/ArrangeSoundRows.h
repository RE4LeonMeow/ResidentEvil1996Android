// ArrangeSoundRows.h - where a DC room's enemy sound bank differs from what the
// port loads for it. The table itself is GENERATED: run
// tools/gen_dc_arrange_sound_rows.py after changing it.
//
// A room's enemy sound bank is 48 WAV slots, filled at room load by
// Room_LoadEnemySoundBanks from g_RoomSoundNameTable, indexed
// stage_data_row() * 29 + roomId. An arrange (ADVANCED) room is the SAME
// PHYSICAL ROOM as its base stage's, so it takes the base row on purpose -
// right for the footsteps, the doors and the props, wrong for everything
// ADVANCED changed. The loader applies these overrides afterwards.
//
// The overrides are not inferred from the enemy that spawns. They are read out
// of the arrange RDT's OWN embedded sound bank (RDT+0x88 slot->tone table,
// +0x8C VAB header, +0x90 VAB body), and each slot is named by finding the same
// VAG bytes in a base room whose PC row names that slot. See the generator's
// header comment for the method and for the cases it could not settle that way.
//
// Four things this table can express that a per-enemy group table cannot:
//   * WHICH zombie. The base rooms carry at least four zombie variants and the
//     arrange rooms pick among them per room - room 212's ADVANCED zombie is
//     the z_osou/z_unaruA set, not the z_k01..z_k03 one.
//   * A slot that is not part of any enemy group, like room 101's and 718's
//     window glass, or room 113's bathtub cue.
//   * CLEARING a slot. `name == NULL` means the arrange room really does leave
//     it empty, because what ADVANCED puts there has fewer sounds than what it
//     replaces (room 71A's hunter) or is not there at all (room 212 drops the
//     crows' RVpatA/RVpatB).
//   * A room with NO arrange RDT whose enemy the PC row never covered - see
//     DC_SND_ANY_DC below. Those rooms are invisible to an arrange-vs-base
//     diff, because there is no arrange file to diff against.
#pragma once

// When an entry applies.
enum DcSndScope {
    // Only while the ARRANGE file is loaded. The base room's own row is right
    // in every other mode, so applying these outside ADVANCED would be wrong -
    // room 101 is zombies in ORIGINAL and cerberus in ADVANCED, out of one RDT
    // apiece.
    DC_SND_ARRANGE = 0,

    // Whenever DC mode is on. These rooms have NO arrange RDT - they keep their
    // own file in every mode - and the PC's row simply has nothing for the
    // enemy the DC spawns in them. Room 116, the shotgun room, is the case: its
    // init spawns three zombies behind a `bit_test` on MSF2_DC_ADVANCED, the
    // RDT's own bank fills slots 0-9, and g_RoomSndData names none of them, so
    // the zombies played nothing at all. Same file in both modes, so the bank
    // is right in both.
    DC_SND_ANY_DC  = 1,
};

// One slot of one room. `stage`/`room` are the BASE ids (g_stageId / g_roomId),
// because that is what the room is still running as; `slot` is the index into
// g_emSndBanks; `name` is a WAV under ./sound with no extension, or NULL to
// clear the slot.
struct DcArrangeSndSlot {
    unsigned char stage;
    unsigned char room;
    unsigned char slot;
    unsigned char scope;   // DcSndScope
    const char*   name;
};

// The generated table and its length.
const DcArrangeSndSlot* dc_arrange_snd_slots(int* count);
