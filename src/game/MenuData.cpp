// MenuData.cpp - Read-only data tables for the main menu system
// These tables are extracted from the original binary's .rdata section.
// Addresses correspond to the original binary layout.

#include "../Globals.h"
#include "../system/AssetPath.h"
#include "PrintText.h"

// ============================================================================
// Contiguous menu frame/rect data block (original binary 0x004c26d0..0x004c2998, 712 bytes)
//
// The original game stores ALL menu frame, border, decoration, and rect
// outline data as one contiguous 720-byte block. The code reads entries
// BACKWARDS from various label pointers that sit WITHIN this block:
//
//   0x004c26d0 (offset   0)  g_MainMenuFramesPos  — bottom-frame end marker
//   0x004c27d8 (offset 264)  g_MainMenuFrames2Pos  — bottom frame decoration
//   0x004c2808 (offset 312)  g_MainMenuTopOptionsPos — tab button frame parts
//   0x004c2840 (offset 368)  g_MainMenuFrames3Pos  — frame border segments (tiled)
//   0x004c28c0 (offset 496)  g_MainMenuFrames4Pos  — alternate border (Chris/Jill)
//   0x004c2940 (offset 624)  DAT_004c2940  — rect outline end marker
//   0x004c2960 (offset 656)  g_inventorySlotsPos — inventory slot positions
//
// Splitting these into separate C arrays broke the backward reads because
// the linker places them at arbitrary addresses. Merging them into one
// block restores the original contiguous layout.
// ============================================================================
extern const unsigned char g_MenuFrameDataBlock[712] = {
    // 0x004c26d0: g_MainMenuFramesPos (bottom-frame end marker, 24 shorts)
    // 264bytes

    // screenX(short), screenY(short), width(ushort), height(ushort), texU(byte), texV(byte)

    // 0x8, 0xc, 0x8, 0x8, 0x50 (80), 0x78 (128)
    0x08,0x00,0x0c,0x00,0x08,0x00,0x08,0x00,0x50,0x00,0x78,0x00, // main frame corner top-left
    0x08,0x00,0x84,0x00,0x08,0x00,0x08,0x00,0x58,0x00,0x78,0x00,
    0xC0,0x00,0x0C,0x00,0x08,0x00,0x08,0x00,0x60,0x00,0x70,0x00,
    0xC0,0x00,0x84,0x00,0x08,0x00,0x08,0x00,0x60,0x00,0x78,0x00,
    0x10,0x00,0x0C,0x00,0x10,0x00,0x08,0x00,0x30,0x00,0x78,0x00,
    0x10,0x00,0x84,0x00,0x10,0x00,0x08,0x00,0x40,0x00,0x78,0x00,
    0x08,0x00,0x14,0x00,0x18,0x00,0x70,0x00,0x98,0x00,0x00,0x00,
    0xC0,0x00,0x14,0x00,0x08,0x00,0x70,0x00,0xB0,0x00,0x00,0x00,
    0x20,0x00,0x0C,0x00,0xA0,0x00,0x0C,0x00,0x00,0x00,0x98,0x00,
    0x08,0x00,0x8C,0x00,0x80,0x00,0x28,0x00,0x00,0x00,0xB8,0x00,
    0x94,0x00,0x8E,0x00,0x3C,0x00,0x26,0x00,0x84,0x00,0xBA,0x00,
    0xCC,0x00,0x0C,0x00,0x68,0x00,0x04,0x00,0x00,0x00,0x80,0x00,
    0xCC,0x00,0x30,0x00,0x68,0x00,0x04,0x00,0x00,0x00,0x84,0x00,
    0xD0,0x00,0x92,0x00,0x08,0x00,0x1E,0x00,0xB8,0x00,0x2E,0x00,
    0xD8,0x00,0xB0,0x00,0x04,0x00,0x08,0x00,0x68,0x00,0x80,0x00,
    0x2C,0x01,0xB0,0x00,0x04,0x00,0x08,0x00,0x7C,0x00,0x80,0x00,
    0xD0,0x00,0xB0,0x00,0x08,0x00,0x08,0x00,0x88,0x00,0x50,0x00,
    0x30,0x01,0xB0,0x00,0x08,0x00,0x08,0x00,0x90,0x00,0x50,0x00,
    0x00,0x00,0xB6,0x00,0xC0,0x00,0x02,0x00,0x00,0x00,0xA6,0x00,
    0xC0,0x00,0xB6,0x00,0x80,0x00,0x02,0x00,0x00,0x00,0xA6,0x00,
    0x00,0x00,0xD8,0x00,0x78,0x00,0x10,0x00,0x00,0x00,0x88,0x00,
    0x78,0x00,0xD8,0x00,0xC8,0x00,0x10,0x00,0x00,0x00,0xA8,0x00,


    // 0x004c27d8: g_MainMenuFrames2Pos (bottom frame decoration, 24 shorts)
    0xd0,0x00,0x10,0x00,0x30,0x00,0x10,0x00,0x00,0x00,0x00,0x00,
    0x00,0x01,0x10,0x00,0x30,0x00,0x10,0x00,0x00,0x00,0x20,0x00,
    0xd0,0x00,0x20,0x00,0x30,0x00,0x10,0x00,0x00,0x00,0x10,0x00,
    0x00,0x01,0x20,0x00,0x30,0x00,0x10,0x00,0x00,0x00,0x30,0x00,

    // 0x004c2808: g_MainMenuTopOptionsPos (tab button frame parts, 28 shorts)
    0x20,0x00,0x80,0x00,
    0xa0,0x00,0x0c,0x00,
    0x00,0x00,0x98,0x00,

    0x40,0x01,0xcc,0x00,
    0x10,0x00,0x04,0x00,
    0x10,0x00,0x78,0x00,

    0x88,0x00,0x00,0x82,
    0x30,0x01,0x10,0x00,
    0x04,0x00,0x10,0x00,

    0x7c,0x00,0x88,0x00,
    0x00,0x82,0xdc,0x00,
    0xb0,0x00,0x10,0x00,

    0x08,0x00,0x6c,0x00,

    // 0x004c283c DAT_004c283c
    0x80, 0x00,

    // 0x004c283e DAT_004c283e
    0x00, 0x05,

    // 0x004c2840: g_MainMenuFrames3Pos (frame border segments, 52 shorts)
    0xd0,0x00,0x4e,0x00,
    0x08,0x00,0x08,0x00,
    0x60,0x00,0x58,0x00,
    0x00,0x01,0xd8,0x00,
    0x4e,0x00,0x04,0x00,
    0x08,0x00,0x68,0x00,
    0x58,0x00,0x00,0x01,
    0xdc,0x00,0x4e,0x00,
    0x10,0x00,0x08,0x00,
    0x6c,0x00,0x58,0x00,
    0x00,0x05,0x2c,0x01,
    0x4e,0x00,0x04,0x00,
    0x08,0x00,0x7c,0x00,
    0x58,0x00,0x00,0x01,
    0x30,0x01,0x4e,0x00,
    0x08,0x00,0x08,0x00,
    0x80,0x00,0x50,0x00,
    0x00,0x01,0xd0,0x00,
    0x56,0x00,0x08,0x00,
    0x0f,0x00,0xb8,0x00,
    0x28,0x00,0x00,0x84,
    0xd8,0x00,0x56,0x00,
    0x04,0x00,0x1e,0x00,
    0x68,0x00,0x60,0x00,
    0x00,0x83,0x2c,0x01,
    0x56,0x00,0x04,0x00,
    0x1e,0x00,0x7c,0x00,
    0x60,0x00,0x00,0x83,
    0x30,0x01,0x56,0x00,
    0x08,0x00,0x0f,0x00,
    0x60,0x00,

    // 0x004c28ba DAT_004c28ba
    0x60, 0x00,

    // 0x004c28bc DAT_004c28bc
    0x00, 0x86, 0x00, 0x00,

    // 0x004c28c0: g_MainMenuFrames4Pos (alternate border, 52 shorts)
    0xd0,0x00,0x30,0x00,
    0x08,0x00,0x08,0x00,
    0x60,0x00,0x58,0x00,
    0x00,0x01,0xd8,0x00,
    0x30,0x00,0x04,0x00,
    0x08,0x00,0x68,0x00,
    0x58,0x00,0x00,0x01,
    0xdc,0x00,0x30,0x00,
    0x10,0x00,0x08,0x00,
    0x6c,0x00,0x58,0x00,
    0x00,0x05,0x2c,0x01,
    0x30,0x00,0x04,0x00,
    0x08,0x00,0x7c,0x00,
    0x58,0x00,0x00,0x01,
    0x30,0x01,0x30,0x00,
    0x08,0x00,0x08,0x00,
    0x80,0x00,0x50,0x00,
    0x00,0x01,0xd0,0x00,
    0x38,0x00,0x08,0x00,
    0x0f,0x00,0xb8,0x00,
    0x28,0x00,0x00,0x86,
    0xd8,0x00,0x38,0x00,
    0x04,0x00,0x1e,0x00,
    0x68,0x00,0x60,0x00,
    0x00,0x84,0x2c,0x01,
    0x38,0x00,0x04,0x00,
    0x1e,0x00,0x7c,0x00,
    0x60,0x00,0x00,0x84,
    0x30,0x01,0x38,0x00,
    0x08,0x00,0x0f,0x00,
    0x60,0x00,0x60,0x00,

    // 0x004c293c
    0x00, 0x88, 0x00, 0x00,

    // 0x004c2940: DAT_004c2940 (rect outline end marker, 16 shorts)
    0x00,0x00,0x00,0x00,0x40,0x01,0x0c,0x00,
    0x00,0x00,0x0c,0x00,0x09,0x00,0x80,0x00,
    0xc7,0x00,0x0c,0x00,0x79,0x00,0x80,0x00,
    0x00,0x00,0x8c,0x00,0x40,0x01,0x64,0x00,


    // +656 0x004c2960: g_inventorySlotsPos (inventory slot positions
    0xd0, 0x00,

    0x10,0x00,0x00,0x01,
    0x10,0x00,0xd0,0x00,
    0x20,0x00,0x00,0x01,

    0x20,0x00,0xdc,0x00,
    0x38,0x00,0x04,0x01,
    0x38,0x00,0xdc,0x00,

    0x56,0x00,0x04,0x01,
    0x56,0x00,0xdc,0x00,
    0x74,0x00,0x04,0x01,

    0x74,0x00,

    0xdc,0x00,
    0x92,0x00,
    0x04,0x01,

    0x92,0x00,0x01,0x02,0x03,0x04,0x04,0x00,0x00,0x00
};


// ============================================================================
// ============================================================================
// Menu data tables (extracted from the original binary .rdata section).
// ============================================================================

// g_ItemNameStrings (0x004BECC8, 984 bytes) (RE1 font encoding: 0x00=space,
// 0x07=end of string, 0xF8/0xF9/0xFA = control codes)
//
// Item names, decoded from the original binary. Each name is STR()-encoded
// with an explicit trailing \x07: the item-name terminator doubles as the
// message state machine's "return from item name" tag (case 7 in
// UpdateMessageDisplay, 0x004557b0). The 0x01 the STR macro appends is never
// read — every consumer stops at the 0x07. The comments give each name's
// original offset within the 984-byte block.
static constexpr auto s_itemCombatKnife   = STR("COMBAT KNIFE\x07");    // +0x000
static constexpr auto s_itemBeretta       = STR("BERETTA\x07");         // +0x00d
static constexpr auto s_itemShotgun       = STR("SHOTGUN\x07");         // +0x015
static constexpr auto s_itemColtPython    = STR("COLT PYTHON\x07");     // +0x01d
static constexpr auto s_itemFlamethrower  = STR("FLAMETHROWER\x07");    // +0x029
static constexpr auto s_itemBazooka       = STR("BAZOOKA\x07");         // +0x036
static constexpr auto s_itemRLauncher     = STR("R. LAUNCHER\x07");     // +0x03e
static constexpr auto s_itemClip          = STR("CLIP\x07");            // +0x04a
static constexpr auto s_itemShells        = STR("SHELLS\x07");          // +0x04f
static constexpr auto s_itemDumdumRounds  = STR("DUMDUM ROUNDS\x07");   // +0x056
static constexpr auto s_itemMagnumRounds  = STR("MAGNUM ROUNDS\x07");   // +0x064
static constexpr auto s_itemFuel          = STR("FUEL\x07");            // +0x072
static constexpr auto s_itemExplosiveR    = STR("EXPLOSIVE R.\x07");    // +0x077
static constexpr auto s_itemAcidRounds    = STR("ACID ROUNDS\x07");     // +0x084
static constexpr auto s_itemFlameRounds   = STR("FLAME ROUNDS\x07");    // +0x090
static constexpr auto s_itemEmptyBottle   = STR("EMPTY BOTTLE\x07");    // +0x09d
static constexpr auto s_itemWater         = STR("WATER\x07");           // +0x0aa
static constexpr auto s_itemUmbNo2        = STR("UMB No.2\x07");        // +0x0b0
static constexpr auto s_itemUmbNo4        = STR("UMB No.4\x07");        // +0x0b9
static constexpr auto s_itemUmbNo7        = STR("UMB No.7\x07");        // +0x0c2
static constexpr auto s_itemUmbNo13       = STR("UMB No.13\x07");       // +0x0cb
static constexpr auto s_itemYellow6       = STR("Yellow-6\x07");        // +0x0d5
static constexpr auto s_itemNp003         = STR("NP-003\x07");          // +0x0de
static constexpr auto s_itemVJolt         = STR("V-JOLT\x07");          // +0x0e5
static constexpr auto s_itemBrokenShotgun = STR("BROKEN SHOTGUN\x07");  // +0x0ec
static constexpr auto s_itemCrank         = STR("CRANK\x07");           // +0x0fb
static constexpr auto s_itemSquareCrank   = STR("SQUARE CRANK\x07");    // +0x101
static constexpr auto s_itemHexCrank      = STR("HEX. CRANK\x07");      // +0x10e
static constexpr auto s_itemEmblem        = STR("EMBLEM\x07");          // +0x119
static constexpr auto s_itemGoldEmblem    = STR("GOLD EMBLEM\x07");     // +0x120
static constexpr auto s_itemBlueJewel     = STR("BLUE JEWEL\x07");      // +0x12c
static constexpr auto s_itemRedJewel      = STR("RED JEWEL\x07");       // +0x137
static constexpr auto s_itemMusicNotes    = STR("MUSIC NOTES\x07");     // +0x141
static constexpr auto s_itemWolfMedal     = STR("WOLF MEDAL\x07");      // +0x14d
static constexpr auto s_itemEagleMedal    = STR("EAGLE MEDAL\x07");     // +0x158
static constexpr auto s_itemChemical      = STR("CHEMICAL\x07");        // +0x164
static constexpr auto s_itemHerbicide     = STR("HERBICIDE\x07");       // +0x16d
static constexpr auto s_itemBattery       = STR("BATTERY\x07");         // +0x177
static constexpr auto s_itemMoDisk        = STR("MO DISK\x07");         // +0x17f
static constexpr auto s_itemWindCrest     = STR("WIND CREST\x07");      // +0x187
static constexpr auto s_itemFlare         = STR("FLARE\x07");           // +0x192
static constexpr auto s_itemSlides        = STR("SLIDES\x07");          // +0x198
static constexpr auto s_itemMoonCrest     = STR("MOON CREST\x07");      // +0x19f
static constexpr auto s_itemStarCrest     = STR("STAR CREST\x07");      // +0x1aa
static constexpr auto s_itemSunCrest      = STR("SUN CREST\x07");       // +0x1b5
static constexpr auto s_itemInkRibbon     = STR("INK RIBBON\x07");      // +0x1bf
static constexpr auto s_itemLighter       = STR("LIGHTER\x07");         // +0x1ca
static constexpr auto s_itemLockpick      = STR("LOCKPICK\x07");        // +0x1d2
static constexpr auto s_itemEmpty         = STR("\x07");                // +0x1db (empty name)
static constexpr auto s_itemMansionKey    = STR("MANSION KEY\x07");     // +0x1dc
static constexpr auto s_itemSwordKey      = STR("SWORD KEY\x07");       // +0x1e8
static constexpr auto s_itemArmorKey      = STR("ARMOR KEY\x07");       // +0x1f2
static constexpr auto s_itemShieldKey     = STR("SHIELD KEY\x07");      // +0x1fc
static constexpr auto s_itemHelmetKey     = STR("HELMET KEY\x07");      // +0x207
static constexpr auto s_itemMasterKey     = STR("MASTER KEY\x07");      // +0x212
static constexpr auto s_itemClosetKey     = STR("CLOSET KEY\x07");      // +0x21d
static constexpr auto s_itemDormitoryKey  = STR("DORMITORY KEY\x07");   // +0x228
static constexpr auto s_item002Key        = STR("002 KEY\x07");         // +0x236
static constexpr auto s_item003Key        = STR("003 KEY\x07");         // +0x23e
static constexpr auto s_itemCRoomKey      = STR("C. ROOM KEY\x07");     // +0x246
static constexpr auto s_itemPRoomKey      = STR("P. ROOM KEY\x07");     // +0x252
static constexpr auto s_itemSmallKey      = STR("SMALL KEY\x07");       // +0x25e
static constexpr auto s_itemDeskKey       = STR("DESK KEY\x07");        // +0x268
static constexpr auto s_itemLabKey        = STR("LAB KEY\x07");         // +0x271
static constexpr auto s_itemSpecialKey    = STR("SPECIAL KEY\x07");     // +0x279
static constexpr auto s_itemRedBook       = STR("RED BOOK\x07");        // +0x285
static constexpr auto s_itemBlankBook     = STR("BLANK BOOK\x07");      // +0x28e
static constexpr auto s_itemDoomBook2     = STR("DOOM BOOK 2\x07");     // +0x299
static constexpr auto s_itemDoomBook1     = STR("DOOM BOOK 1\x07");     // +0x2a5
static constexpr auto s_itemFAidSpray     = STR("F.-AID SPRAY\x07");    // +0x2b1
static constexpr auto s_itemSerum         = STR("SERUM\x07");           // +0x2be
static constexpr auto s_itemRedHerb       = STR("RED HERB\x07");        // +0x2c4
static constexpr auto s_itemGreenHerb     = STR("GREEN HERB\x07");      // +0x2cd
static constexpr auto s_itemBlueHerb      = STR("BLUE HERB\x07");       // +0x2d8
static constexpr auto s_itemMixedHerbs    = STR("MIXED HERBS\x07");     // +0x2e2
static constexpr auto s_itemComRadio      = STR("COM. RADIO\x07");      // +0x2ef

// --- File (document) names, PTR_DAT_004bf0a0 entries 77-127 (item ids 0x4e-0x80) ---
// The original's pointer table extends to 128 entries; the files the book
// list refers to (item ids 0x5f-0x70) sit at entries 94-111. Missing entries
// made the "has been filed" \i insert read out of bounds and crash
// message_render_chars. Offsets are relative to the string block.
static constexpr auto s_itemResearchersWill = STR("RESEARCHER'S WILL\x07");  // +0x2f1
static constexpr auto s_itemKeepersDiary    = STR("KEEPER'S DIARY\x07");     // +0x303
static constexpr auto s_itemOrders          = STR("ORDERS\x07");             // +0x312
static constexpr auto s_itemPassNumber      = STR("PASS NUMBER\x07");        // +0x319
static constexpr auto s_itemPlant42Report   = STR("PLANT42 REPORT\x07");     // +0x325
static constexpr auto s_itemFax             = STR("FAX\x07");                // +0x334
static constexpr auto s_itemScrapbook       = STR("SCRAPBOOK\x07");          // +0x338
static constexpr auto s_itemSecuritySystem  = STR("SECURITY SYSTEM\x07");    // +0x342
static constexpr auto s_itemResearchersLtr  = STR("RESEARCHER'S LETTER\x07");// +0x352
static constexpr auto s_itemVJoltReport     = STR("\\oV-JOLT\" REPORT\x07");// +0x366
static constexpr auto s_itemBarrysPicture   = STR("BARRY'S PICTURE\x07");    // +0x376
static constexpr auto s_itemPassCode01      = STR("PASS CODE01\x07");        // +0x386
static constexpr auto s_itemPassCode02      = STR("PASS CODE02\x07");        // +0x392
static constexpr auto s_itemPassCode03      = STR("PASS CODE03\x07");        // +0x39e
static constexpr auto s_itemBotanyBook      = STR("BOTANY BOOK\x07");        // +0x3aa
static constexpr auto s_itemIngram          = STR("INGRAM\x07");             // +0x3b6
static constexpr auto s_itemMinimi          = STR("MINIMI\x07");             // +0x3bd

// PTR_DAT_004bf0a0: item name pointers, indexed by (itemId - 1)
extern const unsigned char* g_ItemNamePointers[128] = {
    (unsigned char*)s_itemCombatKnife.bytes,     // [ 0] COMBAT KNIFE (+0x000)
    (unsigned char*)s_itemBeretta.bytes,         // [ 1] BERETTA (+0x00d)
    (unsigned char*)s_itemShotgun.bytes,         // [ 2] SHOTGUN (+0x015)
    (unsigned char*)s_itemColtPython.bytes,      // [ 3] COLT PYTHON (+0x01d)
    (unsigned char*)s_itemColtPython.bytes,      // [ 4] COLT PYTHON (+0x01d)
    (unsigned char*)s_itemFlamethrower.bytes,    // [ 5] FLAMETHROWER (+0x029)
    (unsigned char*)s_itemBazooka.bytes,         // [ 6] BAZOOKA (+0x036)
    (unsigned char*)s_itemBazooka.bytes,         // [ 7] BAZOOKA (+0x036)
    (unsigned char*)s_itemBazooka.bytes,         // [ 8] BAZOOKA (+0x036)
    (unsigned char*)s_itemRLauncher.bytes,       // [ 9] R. LAUNCHER (+0x03e)
    (unsigned char*)s_itemClip.bytes,            // [10] CLIP (+0x04a)
    (unsigned char*)s_itemShells.bytes,          // [11] SHELLS (+0x04f)
    (unsigned char*)s_itemDumdumRounds.bytes,    // [12] DUMDUM ROUNDS (+0x056)
    (unsigned char*)s_itemMagnumRounds.bytes,    // [13] MAGNUM ROUNDS (+0x064)
    (unsigned char*)s_itemFuel.bytes,            // [14] FUEL (+0x072)
    (unsigned char*)s_itemExplosiveR.bytes,      // [15] EXPLOSIVE R. (+0x077)
    (unsigned char*)s_itemAcidRounds.bytes,      // [16] ACID ROUNDS (+0x084)
    (unsigned char*)s_itemFlameRounds.bytes,     // [17] FLAME ROUNDS (+0x090)
    (unsigned char*)s_itemEmptyBottle.bytes,     // [18] EMPTY BOTTLE (+0x09d)
    (unsigned char*)s_itemWater.bytes,           // [19] WATER (+0x0aa)
    (unsigned char*)s_itemUmbNo2.bytes,          // [20] UMB No.2 (+0x0b0)
    (unsigned char*)s_itemUmbNo4.bytes,          // [21] UMB No.4 (+0x0b9)
    (unsigned char*)s_itemUmbNo7.bytes,          // [22] UMB No.7 (+0x0c2)
    (unsigned char*)s_itemUmbNo13.bytes,         // [23] UMB No.13 (+0x0cb)
    (unsigned char*)s_itemYellow6.bytes,         // [24] Yellow-6 (+0x0d5)
    (unsigned char*)s_itemNp003.bytes,           // [25] NP-003 (+0x0de)
    (unsigned char*)s_itemVJolt.bytes,           // [26] V-JOLT (+0x0e5)
    (unsigned char*)s_itemBrokenShotgun.bytes,   // [27] BROKEN SHOTGUN (+0x0ec)
    (unsigned char*)s_itemSquareCrank.bytes,     // [28] SQUARE CRANK (+0x101)
    (unsigned char*)s_itemHexCrank.bytes,        // [29] HEX. CRANK (+0x10e)
    (unsigned char*)s_itemEmblem.bytes,          // [30] EMBLEM (+0x119)
    (unsigned char*)s_itemGoldEmblem.bytes,      // [31] GOLD EMBLEM (+0x120)
    (unsigned char*)s_itemBlueJewel.bytes,       // [32] BLUE JEWEL (+0x12c)
    (unsigned char*)s_itemRedJewel.bytes,        // [33] RED JEWEL (+0x137)
    (unsigned char*)s_itemMusicNotes.bytes,      // [34] MUSIC NOTES (+0x141)
    (unsigned char*)s_itemWolfMedal.bytes,       // [35] WOLF MEDAL (+0x14d)
    (unsigned char*)s_itemEagleMedal.bytes,      // [36] EAGLE MEDAL (+0x158)
    (unsigned char*)s_itemHerbicide.bytes,       // [37] HERBICIDE (+0x16d)
    (unsigned char*)s_itemBattery.bytes,         // [38] BATTERY (+0x177)
    (unsigned char*)s_itemMoDisk.bytes,          // [39] MO DISK (+0x17f)
    (unsigned char*)s_itemWindCrest.bytes,       // [40] WIND CREST (+0x187)
    (unsigned char*)s_itemFlare.bytes,           // [41] FLARE (+0x192)
    (unsigned char*)s_itemSlides.bytes,          // [42] SLIDES (+0x198)
    (unsigned char*)s_itemMoonCrest.bytes,       // [43] MOON CREST (+0x19f)
    (unsigned char*)s_itemStarCrest.bytes,       // [44] STAR CREST (+0x1aa)
    (unsigned char*)s_itemSunCrest.bytes,        // [45] SUN CREST (+0x1b5)
    (unsigned char*)s_itemInkRibbon.bytes,       // [46] INK RIBBON (+0x1bf)
    (unsigned char*)s_itemLighter.bytes,         // [47] LIGHTER (+0x1ca)
    (unsigned char*)s_itemLockpick.bytes,        // [48] LOCKPICK (+0x1d2)
    (unsigned char*)s_itemEmpty.bytes,           // [49] (empty name, +0x1db)
    (unsigned char*)s_itemSwordKey.bytes,        // [50] SWORD KEY (+0x1e8)
    (unsigned char*)s_itemArmorKey.bytes,        // [51] ARMOR KEY (+0x1f2)
    (unsigned char*)s_itemShieldKey.bytes,       // [52] SHIELD KEY (+0x1fc)
    (unsigned char*)s_itemHelmetKey.bytes,       // [53] HELMET KEY (+0x207)
    (unsigned char*)s_itemMasterKey.bytes,       // [54] MASTER KEY (+0x212)
    (unsigned char*)s_itemClosetKey.bytes,       // [55] CLOSET KEY (+0x21d)
    (unsigned char*)s_item002Key.bytes,          // [56] 002 KEY (+0x236)
    (unsigned char*)s_item003Key.bytes,          // [57] 003 KEY (+0x23e)
    (unsigned char*)s_itemCRoomKey.bytes,        // [58] C. ROOM KEY (+0x246)
    (unsigned char*)s_itemPRoomKey.bytes,        // [59] P. ROOM KEY (+0x252)
    (unsigned char*)s_itemDeskKey.bytes,         // [60] DESK KEY (+0x268)
    (unsigned char*)s_itemBlankBook.bytes,       // [61] BLANK BOOK (+0x28e)
    (unsigned char*)s_itemDoomBook2.bytes,       // [62] DOOM BOOK 2 (+0x299)
    (unsigned char*)s_itemDoomBook1.bytes,       // [63] DOOM BOOK 1 (+0x2a5)
    (unsigned char*)s_itemFAidSpray.bytes,       // [64] F.-AID SPRAY (+0x2b1)
    (unsigned char*)s_itemSerum.bytes,           // [65] SERUM (+0x2be)
    (unsigned char*)s_itemRedHerb.bytes,         // [66] RED HERB (+0x2c4)
    (unsigned char*)s_itemGreenHerb.bytes,       // [67] GREEN HERB (+0x2cd)
    (unsigned char*)s_itemBlueHerb.bytes,        // [68] BLUE HERB (+0x2d8)
    (unsigned char*)s_itemMixedHerbs.bytes,      // [69] MIXED HERBS (+0x2e2)
    (unsigned char*)s_itemMixedHerbs.bytes,      // [70] MIXED HERBS (+0x2e2)
    (unsigned char*)s_itemMixedHerbs.bytes,      // [71] MIXED HERBS (+0x2e2)
    (unsigned char*)s_itemMixedHerbs.bytes,      // [72] MIXED HERBS (+0x2e2)
    (unsigned char*)s_itemMixedHerbs.bytes,      // [73] MIXED HERBS (+0x2e2)
    (unsigned char*)s_itemMixedHerbs.bytes,      // [74] MIXED HERBS (+0x2e2)
    (unsigned char*)s_itemEmpty.bytes,           // [75] (empty name, +0x2ee)
    (unsigned char*)s_itemComRadio.bytes,        // [76] COM. RADIO (+0x2ef)
    // [77]-[93]: unexamined/special items and the first Doom Book medal
    // file (item 0x5d) - all empty names in the original (pointers into the
    // 0x07 padding before "RESEARCHER'S WILL").
    (unsigned char*)s_itemEmpty.bytes,           // [77]
    (unsigned char*)s_itemEmpty.bytes,           // [78]
    (unsigned char*)s_itemEmpty.bytes,           // [79]
    (unsigned char*)s_itemEmpty.bytes,           // [80]
    (unsigned char*)s_itemEmpty.bytes,           // [81]
    (unsigned char*)s_itemEmpty.bytes,           // [82]
    (unsigned char*)s_itemEmpty.bytes,           // [83]
    (unsigned char*)s_itemEmpty.bytes,           // [84]
    (unsigned char*)s_itemEmpty.bytes,           // [85]
    (unsigned char*)s_itemEmpty.bytes,           // [86]
    (unsigned char*)s_itemEmpty.bytes,           // [87]
    (unsigned char*)s_itemEmpty.bytes,           // [88]
    (unsigned char*)s_itemEmpty.bytes,           // [89]
    (unsigned char*)s_itemEmpty.bytes,           // [90]
    (unsigned char*)s_itemEmpty.bytes,           // [91]
    (unsigned char*)s_itemEmpty.bytes,           // [92] item 0x5d (second medal file)
    (unsigned char*)s_itemEmpty.bytes,           // [93] item 0x5e
    // [94]-[111]: the files (documents), item ids 0x5f-0x70
    (unsigned char*)s_itemResearchersWill.bytes, // [94] 0x5f RESEARCHER'S WILL
    (unsigned char*)s_itemResearchersWill.bytes, // [95] 0x60 (same name in the original)
    (unsigned char*)s_itemKeepersDiary.bytes,    // [96] 0x61 KEEPER'S DIARY
    (unsigned char*)s_itemOrders.bytes,          // [97] 0x62 ORDERS
    (unsigned char*)s_itemPassNumber.bytes,      // [98] 0x63 PASS NUMBER
    (unsigned char*)s_itemPlant42Report.bytes,   // [99] 0x64 PLANT42 REPORT
    (unsigned char*)s_itemFax.bytes,             // [100] 0x65 FAX
    (unsigned char*)s_itemScrapbook.bytes,       // [101] 0x66 SCRAPBOOK
    (unsigned char*)s_itemSecuritySystem.bytes,  // [102] 0x67 SECURITY SYSTEM
    (unsigned char*)s_itemResearchersLtr.bytes,  // [103] 0x68 RESEARCHER'S LETTER
    (unsigned char*)s_itemVJoltReport.bytes,     // [104] 0x69 "V-JOLT" REPORT
    (unsigned char*)s_itemBarrysPicture.bytes,   // [105] 0x6a BARRY'S PICTURE
    (unsigned char*)s_itemPassCode01.bytes,      // [106] 0x6b PASS CODE01
    (unsigned char*)s_itemPassCode02.bytes,      // [107] 0x6c PASS CODE02
    (unsigned char*)s_itemPassCode03.bytes,      // [108] 0x6d PASS CODE03
    (unsigned char*)s_itemBotanyBook.bytes,      // [109] 0x6e BOTANY BOOK
    (unsigned char*)s_itemIngram.bytes,          // [110] 0x6f INGRAM
    (unsigned char*)s_itemMinimi.bytes,          // [111] 0x70 MINIMI
    // [112]-[127]: later special items reuse earlier name strings
    (unsigned char*)s_itemCrank.bytes,           // [112] CRANK (original: 0x004bedc3)
    (unsigned char*)s_itemCrank.bytes,           // [113] CRANK
    (unsigned char*)s_itemChemical.bytes,        // [114] CHEMICAL (original: 0x004bee2c)
    (unsigned char*)s_itemMansionKey.bytes,      // [115] MANSION KEY
    (unsigned char*)s_itemMansionKey.bytes,      // [116]
    (unsigned char*)s_itemMansionKey.bytes,      // [117]
    (unsigned char*)s_itemMansionKey.bytes,      // [118]
    (unsigned char*)s_itemLabKey.bytes,          // [119] LAB KEY
    (unsigned char*)s_itemSpecialKey.bytes,      // [120] SPECIAL KEY
    (unsigned char*)s_itemDormitoryKey.bytes,    // [121] DORMITORY KEY
    (unsigned char*)s_itemDormitoryKey.bytes,    // [122]
    (unsigned char*)s_itemLabKey.bytes,          // [123] LAB KEY
    (unsigned char*)s_itemSmallKey.bytes,        // [124] SMALL KEY
    (unsigned char*)s_itemRedBook.bytes,         // [125] RED BOOK
    (unsigned char*)s_itemDoomBook2.bytes,       // [126] DOOM BOOK 2
    (unsigned char*)s_itemDoomBook1.bytes,       // [127] DOOM BOOK 1
};

// PTR_DAT_004bf260: unexamined-item generic names (category = item lookup byte 2)
extern const unsigned char* g_UnknownItemNamePointers[16] = {
    (unsigned char*)s_itemCrank.bytes,          // [ 0] CRANK (+0x0fb)
    (unsigned char*)s_itemCrank.bytes,          // [ 1] CRANK (+0x0fb)
    (unsigned char*)s_itemChemical.bytes,       // [ 2] CHEMICAL (+0x164)
    (unsigned char*)s_itemMansionKey.bytes,     // [ 3] MANSION KEY (+0x1dc)
    (unsigned char*)s_itemMansionKey.bytes,     // [ 4] MANSION KEY (+0x1dc)
    (unsigned char*)s_itemMansionKey.bytes,     // [ 5] MANSION KEY (+0x1dc)
    (unsigned char*)s_itemMansionKey.bytes,     // [ 6] MANSION KEY (+0x1dc)
    (unsigned char*)s_itemLabKey.bytes,         // [ 7] LAB KEY (+0x271)
    (unsigned char*)s_itemSpecialKey.bytes,     // [ 8] SPECIAL KEY (+0x279)
    (unsigned char*)s_itemDormitoryKey.bytes,   // [ 9] DORMITORY KEY (+0x228)
    (unsigned char*)s_itemDormitoryKey.bytes,   // [10] DORMITORY KEY (+0x228)
    (unsigned char*)s_itemLabKey.bytes,         // [11] LAB KEY (+0x271)
    (unsigned char*)s_itemSmallKey.bytes,       // [12] SMALL KEY (+0x25e)
    (unsigned char*)s_itemRedBook.bytes,        // [13] RED BOOK (+0x285)
    (unsigned char*)s_itemDoomBook2.bytes,      // [14] DOOM BOOK 2 (+0x299)
    (unsigned char*)s_itemDoomBook1.bytes,      // [15] DOOM BOOK 1 (+0x2a5)
};

// g_ItemModelFileNames (0x004BD348, 600 bytes) (75 x 8-byte ASCII model file
// names, indexed by item image type; each record is a null-padded name, e.g.
// "i05v". Consumed as a flat record table: (char*)g_ItemModelFileNames + idx*8.
// The Director's Cut's items reach image type 0x4B (past this table - the USA
// build's largest is 0x4A) and rename two more, so MainMenu.cpp's
// item_model_file_name() consults ItemModels.cpp for those.
extern const unsigned char g_ItemModelFileNames[75][8] = {
    "",     // [ 0]
    "i05v", // [ 1]
    "i00v", // [ 2]
    "i02v", // [ 3]
    "i04v", // [ 4]
    "i01v", // [ 5]
    "i08v", // [ 6]
    "i07v", // [ 7]
    "i26v", // [ 8]
    "i16v", // [ 9]
    "i06v", // [10]
    "i15v", // [11]
    "i11v", // [12]
    "i14v", // [13]
    "i12v", // [14]
    "i13v", // [15]
    "i50v", // [16]
    "i21v", // [17]
    "i22v", // [18]
    "i48v", // [19]
    "i58v", // [20]
    "i47v", // [21]
    "i49v", // [22]
    "i62v", // [23]
    "i09v", // [24]
    "i03v", // [25]
    "i19v", // [26]
    "i72v", // [27]
    "i38v", // [28]
    "i39v", // [29]
    "i41v", // [30]
    "i42v", // [31]
    "i45v", // [32]
    "i24v", // [33]
    "i25v", // [34]
    "i43v", // [35]
    "i54v", // [36]
    "i37v", // [37]
    "i44v", // [38]
    "i55v", // [39]
    "i46v", // [40]
    "i60v", // [41]
    "i61v", // [42]
    "i59v", // [43]
    "i56v", // [44]
    "i17v", // [45]
    "i18v", // [46]
    "i40v", // [47]
    "i30v", // [48]
    "i28v", // [49]
    "i29v", // [50]
    "i27v", // [51]
    "i34v", // [52]
    "i73v", // [53]
    "i32v", // [54]
    "i31v", // [55]
    "i36v", // [56]
    "i33v", // [57]
    "i63v", // [58]
    "i53v", // [59]
    "i51v", // [60]
    "i52v", // [61]
    "i23v", // [62]
    "i20v", // [63]
    "i66v", // [64]
    "i64v", // [65]
    "i65v", // [66]
    "i69v", // [67]
    "i67v", // [68]
    "i68v", // [69]
    "i70v", // [70]
    "i71v", // [71]
    "i75v", // [72]
    "i10v", // [73]
    "i57v", // [74]
};

// Special model file names for items 0x6F (ING) / 0x70 (MINI) (0x004BD5A0)
extern const unsigned char g_ItemModelFileNameING[8] = "ING";
extern const unsigned char g_ItemModelFileNameMINI[8] = "MINI";

// PTR_DAT_004bd768: item combine tables, indexed by item image type (byte 1 of
// g_ItemImageLookupTable). Each table: count byte + 4-byte records
// {otherItem, newForCursor, newForTarget, effect}.
extern unsigned char* g_ItemCombinePtrs[38] = {
    g_ItemCombineData + 0, g_ItemCombineData + 5, g_ItemCombineData + 10, g_ItemCombineData + 15,
    g_ItemCombineData + 20, g_ItemCombineData + 25, g_ItemCombineData + 38, g_ItemCombineData + 51,
    g_ItemCombineData + 64, g_ItemCombineData + 73, g_ItemCombineData + 82, g_ItemCombineData + 91,
    g_ItemCombineData + 100, g_ItemCombineData + 109, g_ItemCombineData + 126, g_ItemCombineData + 143,
    g_ItemCombineData + 160, g_ItemCombineData + 161, g_ItemCombineData + 190, g_ItemCombineData + 219,
    g_ItemCombineData + 248, g_ItemCombineData + 277, g_ItemCombineData + 306, g_ItemCombineData + 335,
    g_ItemCombineData + 364, g_ItemCombineData + 365, g_ItemCombineData + 370, g_ItemCombineData + 379,
    g_ItemCombineData + 400, g_ItemCombineData + 413, g_ItemCombineData + 418, g_ItemCombineData + 427,
    g_ItemCombineData + 436, g_ItemCombineData + 437, g_ItemCombineData + 438,
    // The three Director's Cut entries (the crest recipes) land at 35..37; in the
    // USA build these point at the zeroed tail and are never indexed.
    g_ItemCombineData + 440, g_ItemCombineData + 441, g_ItemCombineData + 442,
};

// g_ItemCombineData (0x004BD5B0, 440 bytes; 454 to hold the DC's three crest
// recipes, see dc_apply_item_tables)
extern unsigned char g_ItemCombineData[454] = {
    0x01,0x0B,0x02,0x0B,0x01,0x01,0x0C,0x03,0x0C,0x01,0x01,0x0D,0x04,0x0D,0x01,0x01,
    0x0E,0x05,0x0E,0x01,0x01,0x0F,0x06,0x0F,0x01,0x03,0x10,0x07,0x10,0x01,0x11,0x08,
    0x11,0x06,0x12,0x09,0x12,0x06,0x03,0x10,0x07,0x10,0x06,0x11,0x08,0x11,0x01,0x12,
    0x09,0x12,0x06,0x03,0x10,0x07,0x10,0x06,0x11,0x08,0x11,0x06,0x12,0x09,0x12,0x01,
    0x02,0x02,0x0B,0x02,0x02,0x0B,0x0B,0x0B,0x03,0x02,0x03,0x0C,0x03,0x02,0x0C,0x0C,
    0x0C,0x03,0x02,0x04,0x0D,0x04,0x02,0x0D,0x0D,0x0D,0x03,0x02,0x05,0x0E,0x05,0x02,
    0x0E,0x0E,0x0E,0x03,0x02,0x06,0x0F,0x06,0x02,0x0F,0x0F,0x0F,0x03,0x04,0x07,0x10,
    0x07,0x02,0x08,0x10,0x07,0x07,0x09,0x10,0x07,0x07,0x10,0x10,0x10,0x03,0x04,0x07,
    0x11,0x08,0x07,0x08,0x11,0x08,0x02,0x09,0x11,0x08,0x07,0x11,0x11,0x11,0x03,0x04,
    0x07,0x12,0x09,0x07,0x08,0x12,0x09,0x07,0x09,0x12,0x09,0x02,0x12,0x12,0x12,0x03,
    0x00,0x07,0x14,0x13,0x14,0x00,0x15,0x13,0x1A,0x00,0x16,0x13,0x13,0x05,0x17,0x13,
    0x13,0x04,0x18,0x13,0x13,0x05,0x19,0x13,0x17,0x00,0x1A,0x13,0x16,0x00,0x07,0x14,
    0x13,0x1A,0x00,0x15,0x13,0x15,0x00,0x16,0x13,0x19,0x00,0x17,0x13,0x13,0x05,0x18,
    0x13,0x13,0x04,0x19,0x13,0x13,0x04,0x1A,0x13,0x13,0x05,0x07,0x14,0x13,0x13,0x05,
    0x15,0x13,0x19,0x00,0x16,0x13,0x16,0x00,0x17,0x13,0x13,0x05,0x18,0x13,0x13,0x04,
    0x19,0x13,0x13,0x05,0x1A,0x13,0x17,0x00,0x07,0x14,0x13,0x13,0x04,0x15,0x13,0x13,
    0x05,0x16,0x13,0x13,0x05,0x17,0x13,0x17,0x00,0x18,0x13,0x13,0x04,0x19,0x13,0x18,
    0x00,0x1A,0x13,0x13,0x05,0x07,0x14,0x13,0x13,0x05,0x15,0x13,0x13,0x04,0x16,0x13,
    0x13,0x04,0x17,0x13,0x13,0x04,0x18,0x13,0x18,0x00,0x19,0x13,0x13,0x04,0x1A,0x13,
    0x1B,0x00,0x07,0x14,0x13,0x17,0x00,0x15,0x13,0x13,0x04,0x16,0x13,0x13,0x05,0x17,
    0x13,0x18,0x00,0x18,0x13,0x13,0x04,0x19,0x13,0x19,0x00,0x1A,0x13,0x13,0x05,0x07,
    0x14,0x13,0x16,0x00,0x15,0x13,0x13,0x05,0x16,0x13,0x17,0x00,0x17,0x13,0x13,0x05,
    0x18,0x13,0x1B,0x00,0x19,0x13,0x13,0x05,0x1A,0x13,0x1A,0x00,0x00,0x01,0x2F,0x2F,
    0x2F,0x03,0x02,0x44,0x46,0x00,0x00,0x48,0x49,0x00,0x00,0x05,0x43,0x46,0x00,0x00,
    0x44,0x47,0x00,0x00,0x45,0x48,0x00,0x00,0x47,0x4A,0x00,0x00,0x48,0x4B,0x00,0x00,
    0x03,0x44,0x48,0x00,0x00,0x46,0x49,0x00,0x00,0x47,0x4B,0x00,0x00,0x01,0x45,0x49,
    0x00,0x00,0x02,0x44,0x4A,0x00,0x00,0x45,0x4B,0x00,0x00,0x02,0x43,0x49,0x00,0x00,
    0x44,0x4B,0x00,0x00,0x00,0x00,0x00,0x00,
    // Spare tail for the Director's Cut combine tables (unused in the USA build).
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

// g_ItemMaxQty (0x004BD81C, 448 bytes) (max quantity for itemId, byte at +itemId*4)
extern unsigned char g_ItemMaxQty[448] = {
    0x00,0x00,0x00,0x00,0x00,0x01,0x80,0x80,0x0F,0x02,0x00,0x80,0x07,0x03,0x01,0x80,
    0x06,0x04,0x02,0x80,0x06,0x04,0x03,0x80,0xF0,0x05,0x04,0x80,0x06,0x06,0x05,0x80,
    0x06,0x06,0x06,0x80,0x06,0x06,0x07,0x80,0x04,0x07,0x80,0x80,0x0F,0x08,0x08,0x80,
    0x07,0x09,0x09,0x80,0x06,0x0A,0x0A,0x80,0x06,0x0B,0x0B,0x80,0xF0,0x0C,0x0C,0x80,
    0x06,0x0D,0x0D,0x80,0x06,0x0E,0x0E,0x80,0x06,0x0F,0x0F,0x80,0x01,0x10,0x10,0x80,
    0x01,0x11,0x11,0x80,0x01,0x12,0x12,0x80,0x01,0x13,0x13,0x80,0x01,0x14,0x14,0x80,
    0x01,0x15,0x15,0x80,0x01,0x16,0x16,0x80,0x01,0x17,0x17,0x80,0x01,0x18,0x18,0x80,
    0x01,0x19,0x80,0x80,0x00,0x1A,0x80,0x00,0x00,0x1B,0x80,0x01,0x01,0x1C,0x80,0x80,
    0x01,0x1D,0x80,0x80,0x01,0x1E,0x80,0x80,0x01,0x1F,0x80,0x80,0x01,0x20,0x80,0x80,
    0x01,0x21,0x80,0x80,0x01,0x22,0x80,0x80,0x01,0x23,0x80,0x02,0x01,0x24,0x80,0x80,
    0x01,0x25,0x80,0x80,0x01,0x26,0x80,0x80,0x01,0x27,0x80,0x80,0x01,0x28,0x80,0x80,
    0x01,0x29,0x80,0x80,0x01,0x2A,0x80,0x80,0x01,0x2B,0x80,0x80,0x03,0x2C,0x19,0x80,
    0x00,0x2D,0x80,0x80,0x00,0x2E,0x80,0x80,0x02,0x2F,0x80,0x80,0x00,0x30,0x80,0x03,
    0x00,0x31,0x80,0x04,0x00,0x32,0x80,0x05,0x00,0x33,0x80,0x06,0x00,0x34,0x80,0x07,
    0x00,0x35,0x80,0x08,0x00,0x36,0x80,0x09,0x00,0x37,0x80,0x0A,0x00,0x38,0x80,0x80,
    0x00,0x39,0x80,0x0B,0x01,0x3A,0x80,0x0C,0x01,0x3B,0x80,0x0D,0x01,0x3C,0x80,0x0E,
    0x01,0x3D,0x80,0x0F,0x01,0x3E,0x80,0x80,0x01,0x3F,0x80,0x80,0x01,0x40,0x1A,0x80,
    0x01,0x41,0x1B,0x80,0x01,0x42,0x1C,0x80,0x01,0x43,0x1D,0x80,0x01,0x44,0x1E,0x80,
    0x01,0x45,0x1F,0x80,0x01,0x46,0x20,0x80,0x01,0x47,0x21,0x80,0x01,0x48,0x22,0x80,
    0x00,0x49,0x80,0x80,0x00,0x4A,0x80,0x80,0x00,0x00,0x00,0x00,0x0B,0x13,0x14,0x1B,
    0x33,0x3D,0x3E,0x41,0x4C,0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x10,0x00,0x01,
    0x20,0x03,0x02,0x21,0x23,0x03,0x22,0x00,0x00,0x00,0x00,0x00,0xEB,0xEC,0xEF,0xE1,
    0xE2,0xE3,0xE4,0xE9,0xEA,0xE5,0xE6,0xE8,0xE7,0xED,0xEE,0xEE,0x00,0x00,0x00,0x00,
    0x2C,0x01,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x00,
    0x30,0x02,0x00,0x08,0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x00,0x30,0x02,0x00,0x00,
    0x00,0x00,0x00,0x00,0x90,0x01,0x00,0x04,0x00,0x00,0x00,0x00,0x10,0x10,0x00,0x11,
    0x11,0x11,0x11,0x00,0x00,0x11,0x11,0x21,0x00,0x13,0x13,0x13,0x00,0x80,0x44,0xFD,
    0x00,0x00,0x00,0x00,0x00,0x00,0xF4,0x01,0x00,0x00,0x00,0x00,0x00,0x80,0x0C,0xFE,
};

// g_ItemImageTypeTable (0x004BD7F8, 35 bytes; 38 to hold the Director's Cut's)
// - indexed by the item lookup record's byte 1, giving the 1-based row of the
// item's icon inside data/item_mix.pix (18 x 1200-byte 20x30 images = the
// file's 21600 bytes).
// A previous revision cut this at 32 bytes, but the last three live entries
// (0x10/0x11/0x12) sit at indices 32-34 and belong to the mixed herbs
// 0x49/0x4A/0x4B (green+red+blue, 3x green, 2x green+red - their lookup byte 1
// is 32/33/34). Mixing into any of those read past the array, so
// menu_item_combine_refresh fed LoadItemImage a garbage row index and painted
// junk from beyond the .pix over the slot's sprite.
//
// Not const: the DC's table (3 more entries, herb rows +3) differs from byte 26
// on, so dc_apply_item_tables copies it over this one. The USA data below fills
// indices 0..34 and the spare tail stays zero.
extern unsigned char g_ItemImageTypeTable[38] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x03,0x04,0x05,
    0x06,0x00,0x00,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x00,0x00,0x00,0x00,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,
};

// g_ItemModelExtIVM (0x004C29A0) ".ivm" extension, appended to the model file
// name by menu_load_item_model (strcat). In the original binary this string
// sits immediately before g_ItemModelDir (0x004C29A8); only the
// null-terminated ".ivm" prefix is ever read.
extern const unsigned char g_ItemModelExtIVM[8] = ".ivm";

// g_ItemModelDir (0x004C29A8) "./usa/item_m2/" (not used by this port — the
// menu code builds the path from GAME_DATA_ROOT + "item_m2/" instead)
extern const unsigned char g_ItemModelDir[24] = "./usa/item_m2/";

// g_ItemMixPixPath (0x004B10D4) ".\usa\data\item_mix.pix" - the sprite sheet
// menu_item_combine_refresh reloads a combined slot's icon from. Rooted at
// GAME_DATA_ROOT like every other asset path in this port (see
// system/AssetPath.h); the literal retail root only resolves in a release
// build, so LoadFile silently failed on it and no combine ever got a new icon.
extern const unsigned char g_ItemMixPixPath[32] = GAME_DATA_ROOT "data\\item_mix.pix";

// The Director's Cut's own item_mix.pix (3 extra mixed-herb rows) lives in its
// overlay under this same name, so there is no DC twin of this path: the
// overlay search in ResolveAssetRoot picks the right file.

// g_MedalPixPath (0x004BF330) "./usa/data/medal.pix" - the two 20x30 slot icons
// (wolf medal row 0, eagle medal row 1) the doom-book examine swaps in. Rooted
// at GAME_DATA_ROOT like every other asset path in this port.
extern const unsigned char g_MedalPixPath[32] = GAME_DATA_ROOT "data\\medal.pix";

// g_ItemHealTable (0x004BD927, 97 bytes) - indexed by itemId (DAT_004bd927 + itemId)
// Low nibble: heal amount (1=1/3, 2=2/3, 3=full). High nibble: status cure
// (0x10 = poison flag 0x20, 0x20 = poison flag 0x2).
//
// The table overlaps g_ItemImageLookupTable (it starts inside record 66) and
// carries a second payload: [0x51 + flagIndex] is the examine message id for
// each of the 16 examinable items, running 0x004BD978..0x004BD987 - one byte
// PAST the 96 Ghidra labels the array with. Doom Book 1 (item 0x40) has
// flagIndex 15, i.e. index 0x60, so the 96-byte version read off the end of the
// array and set_message_display got whatever followed in .rdata instead of
// 0xEE ("There was a medal in the book.").
extern const unsigned char g_ItemHealTable[0x61] = {
    0x80,0x01,0x40,0x1A,0x80,0x01,0x41,0x1B,0x80,0x01,0x42,0x1C,0x80,0x01,0x43,0x1D,
    0x80,0x01,0x44,0x1E,0x80,0x01,0x45,0x1F,0x80,0x01,0x46,0x20,0x80,0x01,0x47,0x21,
    0x80,0x01,0x48,0x22,0x80,0x00,0x49,0x80,0x80,0x00,0x4A,0x80,0x80,0x00,0x00,0x00,
    0x00,0x0B,0x13,0x14,0x1B,0x33,0x3D,0x3E,0x41,0x4C,0x4C,0x00,0x00,0x00,0x00,0x00,
    0x00,0x03,0x10,0x00,0x01,0x20,0x03,0x02,0x21,0x23,0x03,0x22,0x00,0x00,0x00,0x00,
    0x00,0xEB,0xEC,0xEF,0xE1,0xE2,0xE3,0xE4,0xE9,0xEA,0xE5,0xE6,0xE8,0xE7,0xED,0xEE,
    0xEE,   // [0x60] examine message for flagIndex 15 (Doom Book 1, item 0x40)
};

// Health bar EKG color table (RGB triplets for each health status)
// ============================================================================
extern const unsigned char DAT_004b92c8[] = {
    0xC0, 0x00, 0x00,
    0xFF, 0x7F, 0x00,
    0xD8, 0xD8, 0x00,
    0x00, 0xFF, 0x00,
    0x00, 0x00, 0x00
};

// ============================================================================

// ============================================================================
// Item examination tables (FUN_0044ed40 / FUN_0044ef60)
// ============================================================================

// DAT_004bd988: item examine rotation combo records (12 bytes each:
// {targetX, tolX, targetY, tolY, targetZ, tolZ} in GTE angle units).
extern const unsigned char g_ItemExamineCombos[48] = {
    0x00,0x00,0x00,0x00,0x2C,0x01,0x00,0x0C,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x00,0x30,0x02,0x00,0x08,
    0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x00,0x30,0x02,0x00,0x00,
    0x00,0x00,0x00,0x00,0x90,0x01,0x00,0x04,0x00,0x00,0x00,0x00,
};

// DAT_004bd9b8: examine type per item flag index (byte at +flagIndex).
// High nibble = combo record count, low nibble = combo record index.
extern const unsigned char g_ItemExamineTypes[48] = {
    0x10,0x10,0x00,0x11,0x11,0x11,0x11,0x00,0x00,0x11,0x11,0x21,
    0x00,0x13,0x13,0x13,0x00,0x80,0x44,0xFD,0x00,0x00,0x00,0x00,
    0x00,0x00,0xF4,0x01,0x00,0x00,0x00,0x00,0x00,0x80,0x0C,0xFE,
    0x00,0x00,0x00,0x00,0x00,0x00,0xF4,0x01,0x00,0x00,0x00,0x00,
};
// EKG face animation frame tables
// ============================================================================
extern const unsigned char DAT_004b92d8[] = {
    0x0C, 0x11, 0x14, 0x3C, 0x3F, 0x44, 0x50, 0x00
};

extern const unsigned char DAT_004b92e0[] = {
    0x03, 0x07, 0x21, 0x25, 0x2B, 0x2F, 0x49, 0x4D,
    0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

extern const unsigned char DAT_004b92f0[] = {
    0x00, 0x03, 0x02, 0x01, 0x02, 0x03, 0x00, 0x00
};

// DAT_004b92f8 - danger/poison word flash variants, indexed by the EKG sweep
// position (FUN_004387e0 over DAT_004b92e0). The scan's maximum index is 8
// (entry [8] = 0x50 is never consumed), and menu_draw_health_bar indexes this
// table with it. The original image keeps 16 zero bytes at 0x004b9300 right
// after the 8 entries, so index 8 reads 0 = "don't draw the word" (part of
// the blink pattern). Without the tail the index-8 read picks up the first
// byte of ekg_wave_00 (0x1C) and the word tile wraps to the wrong row:
// danger drew a "Fine" tile (texV 0x10) and poison a caution/danger flash
// tile (texV 0x20) for the last two frames of every EKG sweep.
extern const unsigned char DAT_004b92f8[24] = {
    0x00, 0x02, 0x01, 0x02, 0x00, 0x02, 0x01, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x004b9300
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00    // 0x004b9308
};

// ============================================================================
// EKG wave data tables (records of 4 signed bytes {X1, X0, Y1, slope}, 0x00
// terminator). Extracted from the original binary at PTR_DAT_004b9278.
// ============================================================================
static const char ekg_wave_00[] = { 28,47,0,0, 27,28,-1,1, 26,27,-3,2, 25,26,0,-3, 24,25,2,-2, 23,24,1,1, 22,23,-1,2, 21,22,-4,3, 20,21,0,-4, 19,20,2,-2, 18,19,0,2, 0,18,0,0};
static const char ekg_wave_01[] = { 28,47,0,0, 27,28,-1,1, 26,27,-3,2, 24,26,1,-2, 23,24,0,1, 21,23,-4,2, 20,21,-1,-3, 19,20,1,2, 18,19,0,1, 0,18,0,0};
static const char ekg_wave_02[] = { 28,47,0,0, 27,28,-2,2, 25,27,2,-2, 24,25,1,1, 23,24,-1,2, 22,23,-4,3, 20,22,2,-3, 19,20,0,2, 0,19,0,0};
static const char ekg_wave_03[] = { 28,47,0,0, 27,28,-1,1, 26,27,-3,2, 24,26,1,-2, 23,24,-1,2, 22,23,-4,3, 21,22,-1,-3, 20,21,1,-2, 19,20,0,1, 0,19,0,0};
static const char ekg_wave_04[] = { 29,47,0,0, 28,29,-1,1, 27,28,-3,2, 26,27,0,-3, 25,26,2,-2, 24,25,1,1, 23,24,-1,2, 21,23,-7,3, 20,21,-3,-4, 19,20,0,-3, 18,19,2,-2, 17,18,0,2, 0,17,0,0};
static const char ekg_wave_05[] = { 28,47,0,0, 27,28,-1,1, 26,27,-3,2, 24,26,1,-2, 23,24,-1,2, 21,23,-7,3, 19,21,1,-4, 18,19,0,1, 0,18,0,0};
static const char ekg_wave_06[] = { 29,47,0,0, 27,29,-4,2, 25,27,2,-3, 24,25,1,1, 23,24,-1,2, 21,23,-7,3, 18,21,2,-3, 17,18,0,2, 0,17,0,0};
static const char ekg_wave_07[] = { 29,47,0,0, 28,29,-1,1, 27,28,-3,2, 25,27,1,-2, 24,25,-1,2, 22,24,-7,3, 20,22,-1,-3, 19,20,1,-2, 18,19,0,1, 0,18,0,0};
static const char ekg_wave_08[] = { 31,47,0,0, 29,31,-4,2, 28,29,-1,-3, 26,28,3,-2, 25,26,2,1, 21,25,-10,3, 20,21,-5,-5, 18,20,3,-3, 17,18,1,2, 16,17,0,1, 0,16,0,0};
static const char ekg_wave_09[] = { 31,47,0,0, 30,31,-2,2, 29,30,-5,3, 28,29,-2,-3, 26,28,2,-2, 23,26,-4,2, 21,23,-10,3, 17,21,2,-3, 16,17,0,2, 0,16,0,0};
static const char ekg_wave_10[] = { 31,47,0,0, 30,31,-1,1, 29,30,-2,2, 26,29,3,-2, 25,26,2,1, 24,25,0,2, 21,24,-9,3, 18,21,3,-4, 17,18,1,2, 16,17,0,1, 0,16,0,0};
static const char ekg_wave_11[] = { 31,47,0,0, 29,31,-2,2, 26,29,2,-2, 25,26,1,1, 24,25,-1,2, 21,24,-10,3, 17,21,2,-3, 16,17,0,2, 0,16,0,0};
static const char ekg_wave_12[] = { 32,47,0,0, 29,32,-6,2, 26,29,3,-3, 25,26,2,1, 21,25,-14,4, 19,21,-4,-5, 18,19,0,-4, 17,18,3,-3, 16,17,1,2, 15,16,0,1, 0,15,0,0};
static const char ekg_wave_13[] = { 32,47,0,0, 30,32,-6,3, 29,30,-4,-2, 26,29,2,-3, 24,26,-2,2, 21,24,-14,4, 17,21,2,-4, 16,17,0,2, 0,16,0,0};
static const char ekg_wave_14[] = { 31,47,0,0, 29,31,-4,2, 27,29,0,-2, 26,27,3,-3, 25,26,2,1, 24,25,-1,3, 21,24,-13,4, 19,21,-3,-5, 17,19,3,-3, 16,17,1,2, 15,16,0,1, 0,15,0,0};
static const char ekg_wave_15[] = { 32,47,0,0, 30,32,-4,2, 27,30,2,-2, 26,27,1,1, 21,26,-14,3, 17,21,2,-4, 16,17,0,2, 0,16,0,0};
static const char ekg_wave_16[] = { 38,47,0,0, 36,38,-6,3, 34,36,4,-5, 33,34,0,4, 30,33,-15,5, 27,30,3,-6, 26,27,1,2, 25,26,0,1, 22,25,0,0, 20,22,-4,2, 18,20,4,-4, 14,18,-12,4, 11,14,3,-5, 10,11,1,2, 9,10,0,1, 0,9,0,0};
static const char ekg_wave_17[] = { 38,47,0,0, 36,38,-4,2, 34,36,4,-4, 30,34,-12,4, 27,30,3,-5, 26,27,1,2, 25,26,0,1, 22,25,0,0, 20,22,-6,3, 18,20,4,-5, 17,18,0,4, 14,17,-15,5, 11,14,3,-6, 10,11,1,2, 9,10,0,1, 0,9,0,0};
static const char ekg_wave_18[] = { 37,47,0,0, 36,37,-1,1, 34,36,-7,3, 31,34,5,-4, 27,31,-15,5, 24,27,3,-6, 23,24,0,3, 21,23,0,0, 20,21,-2,2, 19,20,-5,3, 17,19,3,-4, 14,17,-12,5, 11,14,3,-5, 10,11,0,3, 0,10,0,0};
static const char ekg_wave_19[] = { 37,47,0,0, 36,37,-2,2, 35,36,-5,3, 33,35,3,-4, 30,33,-12,5, 27,30,3,-5, 26,27,0,3, 24,26,0,0, 23,24,-1,1, 21,23,-7,3, 18,21,5,-4, 14,18,-15,5, 11,14,3,-6, 10,11,0,3, 0,10,0,0};

extern const char* PTR_DAT_004b9278[] = {
    ekg_wave_00, ekg_wave_01, ekg_wave_02, ekg_wave_03,
    ekg_wave_04, ekg_wave_05, ekg_wave_06, ekg_wave_07,
    ekg_wave_08, ekg_wave_09, ekg_wave_10, ekg_wave_11,
    ekg_wave_12, ekg_wave_13, ekg_wave_14, ekg_wave_15,
    ekg_wave_16, ekg_wave_17, ekg_wave_18, ekg_wave_19
};
