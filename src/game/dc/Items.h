// Items.h - the item ids the Director's Cut repurposes, and the two item-id
// derivations the engine makes that change with them.
//
// The DC did not renumber the item table - it took USA slots that the retail
// game never hands out and gave them real items (docs/DC_PORT.md 3a,
// Types.h carries the same meanings as comments on the USA constants). Engine
// code that names one of those slots by its USA meaning therefore names some
// other item in a DC session, which is not a content difference: it is a wrong
// item id handed to the message system, the inventory, or the ammo stack.
#pragma once

#define DC_ITEM_BERETTA_CUSTOM   0x04   // USA: Colt Python w/ DumDum rounds (unobtainable)
#define DC_ITEM_LOCKPICK         0x0D   // USA: DumDum rounds (the DC dropped that ammo)
#define DC_ITEM_MOON_CREST_LEFT  0x31   // USA: Lock Pick
#define DC_ITEM_MOON_CREST_RIGHT 0x32   // USA: Oil
#define DC_ITEM_COMM_RADIO       0x4C   // USA: Pick Axe model (replace it with a nameless com radio model)

// The lockpick's item id in the build in play.
//
// Jill substitutes her lockpick for the SWORD KEY on a mansion door
// (door_try_enter 0x0041b474) and for the SMALL KEY on a desk
// (check_desk_state 0x0041c330), both by setting g_selectedItemId, which the
// following message prints as its name - and both flows then skip consuming
// the item (use_room_action_item 0x00451700, Rendering's message action 1).
// Naming the USA's 0x31 in a DC session printed "MOON CREST used." on a
// lockpick door and would have consumed Jill's lockpick, which the DC put at
// 0x0D.
unsigned char lockpick_item_id(void);

// True when itemId is the lockpick in the build in play - the "don't consume
// this" test the two use paths need.
int is_lockpick_item(unsigned char itemId);

// The ammo item a weapon fires from (fire_consume_ammo_stack 0x0045a580, the
// held-ammo test at 0x0045a530, and the viewer's "loaded" text). The PC build
// uses weaponId + 9 throughout. The DC's item 4 is the Beretta M92FS custom -
// the ADVANCED starting handgun, "loaded with 9mm bullets" like the plain
// Beretta - but 4 + 9 names 0x0D, which the DC gave to the LOCKPICK: firing it
// consumed lockpicks, and with none in the inventory the stack search found no
// slot and cleared the first one instead.
unsigned char weapon_ammo_item_id(unsigned char weaponId);

// The Director's Cut's unlimited Colt Python. An ADVANCED run that reaches the
// best ending raises DC_SCENARIO_FLAG_INF_COLT_PYTHON (0x7A) and hands the
// magnum to the next cycle; while that flag is set the gun never runs out.
// The DC reads it in four places (SLUS_005.51): the ammo check FUN_8004228c
// (0x80042348) refills an empty cylinder to 6, the item viewer FUN_8005436c
// (0x8005440c) draws the infinity glyph instead of digits, and the two
// empty-click paths FUN_8003f008 (0x8003f234) and 0x8003fc6c (0x8003fcb8)
// stay silent. itemId is the equipped item id (a slot's item-id byte).
int dc_is_infinite_colt_python(unsigned char itemId);

// The Director's Cut's item bonus: ammo stacks and ink ribbons picked up are
// DOUBLED in TRAINING and ADVANCED* (the green "secret" Advance mode). PS1
// IncludeCurrentItem (SLUS_005.51 0x8002ce28) shifts the record's quantity
// left once when g_status_flags & 0x50000 - bit 18 (TRAINING) or bit 16
// (ADVANCED_HOLD) - for an item in the ammo range 0x0B..0x12 or the ink ribbon
// (0x2F). The item viewer's fit pre-check (FUN_8002e1a4) repeats it so the
// "you got it" / "no room" message agrees with the inventory. The USA build
// has neither branch, so this returns quantity unchanged outside DC mode and
// outside the two modes. itemId is the item being taken (g_selectedItemId in
// the pickup path, the viewer's selection in the viewer path).
unsigned char dc_item_pickup_quantity(unsigned char itemId, unsigned char quantity);
