// ArrangeStages.h - which stage a room's FILES come from in the Director's Cut.
//
// The arrange rooms are not reached by a door. A door's destination byte packs
// the stage as `(dest >> 5) - 1` (DoorSystem.cpp), which tops out at stage 6 -
// there is no room in it for stages 7-13 at all. Instead the DC re-points the
// FILE lookup per room: PS1 SLUS_005.51 0x80043fb4, called from exactly two
// places, LoadRoomRdt (0x8004484c) and the background loader (0x80017c2c), and
// from nowhere else. g_StageId itself is never changed, which is why every
// stage-indexed table keeps reading its base-stage row with no help.
//
// So an ADVANCED session walks the mansion with g_stageId 0-6 as always, and a
// room that has an arrange version simply loads it: stage 1 room 3 reads
// Stage9/ROOM9030.RDT instead of Stage2/ROOM2030.RDT, cameras, script, models
// and all.
#pragma once

// The stage whose folder this room's files live in: g_stageId, or g_stageId + 7
// when the DC is in ADVANCED and this room has an arrange version. A g_stageId
// already in the arrange range (7-13) is returned unchanged.
unsigned char room_file_stage(void);

// True when the given base stage/room pair has an arrange version at all -
// the table lookup on its own, with no mode test. Used by the debug menu and
// the verifier.
int dc_room_has_arrange(unsigned char stageId, unsigned char roomId);

// True when this room has a second, character-specific RDT (the `...1` file).
// The PC release ships both variants for every room, so its loader can always
// append the variant digit; the DC disc shares one file wherever the PS1 does,
// and asking for the absent one is what crashed Jill's entry into arrange 2F.
// `stageId` is the FILE stage - room_file_stage() - not g_stageId.
int dc_room_has_char_variant(unsigned char stageId, unsigned char roomId);

// The four effect-sprite page indices this room's effects use - a stand-in for
// the row of g_RoomEffectSpriteTable, whose four bytes are the pages at texY
// 0x18..0x1B and where 0xFF means "this room has no such page" (EffectSystem's
// effect_depth_record drops the effect outright on 0xFF, and FUN_0047d0e0 then
// never creates the page). Used for the arrange rooms, which have no row of
// their own - see the .cpp for why one is needed and where the value comes from.
const unsigned char* room_effect_page_entry(void);
