#pragma once
#include "Types.h"
#include "Entities.h"

// ============================================================================
// Bio Card packed layout (0x00be9620 - 0x00BE9A3C, 1052 bytes)
// bio_card.dat is loaded and memcpy'd here by InitializeGame.
// ALL fields MUST be in the correct order, size, and position.
// ============================================================================
#pragma pack(push, 1)
struct BioCardLayout {
    // 0x000: save-card header (0x00be9620-0x00be9820).
    // Initialized EXE .data swept into the card dump; byte-identical between
    // bio_card.dat and every shipped save. Layout: "SC" magic + 13 01 at +0,
    // a 26-byte constant table ending in 00 80 at +0x64, then three identical
    // 128-byte tiles of ee-filled graphics data at +0x80. No code references
    // it by field - only the block base is touched by the load/save memcpys.
    BYTE          prefix[0x200];             // 0x000 - raw bio_card prefix data
    unsigned char stageId;                   // 0x200
    unsigned char roomId;                    // 0x201
    unsigned char roomCameraId;              // 0x202
    unsigned char attractMode_RoomCameraId;  // 0x203
    unsigned char cutId;                     // 0x204
    unsigned char menu_choice_id;            // 0x205
    unsigned char selectedItemId;            // 0x206
    unsigned char totalHeldItems;            // 0x207
    unsigned char specialRoomLightR;         // 0x208 (1 byte, NOT short)
    unsigned char characterModelId;          // 0x209
    unsigned char scdLastEnemyFlags;         // 0x20A (DAT_00be982a, enemy behavior_flags fetched by SCD cmd_enemy_flags_get)
    unsigned char bulletEffectId;            // 0x20B (DAT_00be982b, type of the last billboard effect spawned by cmd_bullet_effect_spawn)
    // 0x20C-0x20E - carried quantity of three specific room pick-ups. SetInitialItems
    // seeds them (7 / 0xF0 / 0xF0) and SCD opcode 0x4C moves them in and out of the
    // pick-up's own record byte 9, so the count survives leaving the room and saving.
    // The three shipped users each restore their weapon's ammo: ROOM1160 a shotgun
    // (item 3, 7 shells), ROOM30B0 and ROOM3080 a flamethrower (item 6, 240 fuel).
    // Opcode 0x4C reaches them as indices 12/13/14 of the byte array based at stageId.
    unsigned char pickupQtyA;                // 0x20C (DAT_00be982c)
    unsigned char pickupQtyB;                // 0x20D (DAT_00be982d)
    unsigned char pickupQtyC;                // 0x20E (DAT_00be982e)
    unsigned char pad_0x20f;                 // 0x20F (no references in the binary)
    unsigned char fwdPosActionId;            // 0x210 (DAT_00be9830, index+1 of the action-zone entry hit by the forward probed player pos)
    unsigned char entPosActionId;            // 0x211 (DAT_00be9831, same but probed against the player entity matrix pos)
    unsigned char usedItemId;                // 0x212
    unsigned char pickedItemId;              // 0x213 (g_pickedItemId, item id last picked up; read by SCD cmd_picked_item_test)
    short         fadingState;               // 0x214
    short         specialRoomLightState;     // 0x216
    short         specialRoomLightDelta;     // 0x218
    short         randSeed;                  // 0x21A
    short         countdownTimer;            // 0x21C
    short         playerHealthCopy;          // 0x21E
    short         playerDpadHeld;            // 0x220
    short         playerDpadPressed;         // 0x222
    DWORD         gameTimerSnapshot;         // 0x224
    unsigned char savesCounter;              // 0x228
    unsigned char equippedItemId;            // 0x229
    unsigned char roomItemBackup;            // 0x22A
    unsigned char selectedCharactedId;       // 0x22B
    short         playerPosXCopy;            // 0x22C
    short         playerPosZCopy;            // 0x22E
    short         playerDirAngleCopy;        // 0x230
    unsigned char playerHealthStatusCopy;    // 0x232
    // 0x233: padding in the USA build (no references in the binary) and the
    // Director's Cut's mode byte - PS1 g_abDcGameMode, state block +0x33, which
    // the DC's load path (SLUS_005.51 0x80019638) turns back into its
    // g_status_flags mode bits. Same values as g_DcDifficulty: 0 STANDARD,
    // 1 TRAINING, 2 ADVANCED, 3 ADVANCED*. A USA save has 0 here, so it loads
    // as STANDARD.
    unsigned char dcGameMode;                // 0x233

    // flags
    unsigned char scenarioFlags2[32];        // 0x234
    unsigned char locksFlags[8];             // 0x254
    unsigned char enemiesFlags[32];          // 0x25C
    unsigned char roomItemsFlags[32];        // 0x27C
    unsigned char itemExaminedFlags[4];      // 0x29C - "item examined" flags: bit per examinable item (indexed via g_ItemImageLookupTable)
    unsigned char scenarioFlags[16];         // 0x2A0
    unsigned char roomFlags[20];             // 0x2B0

    // items slots area
    ItemSlot    itemboxSlots[48];            // 0x2C4
    ItemSlot    itemsSlots[6];               // 0x324
    ItemSlot    rebeccaItemsSlots[6];        // 0x330

    // g_roomBgmState
    unsigned char roomBgmState[224];         // 0x33C
};
static_assert(sizeof(BioCardLayout) == 0x41C, "BioCardLayout size mismatch");
#pragma pack(pop)

// ============================================================================
// Bio Card macro aliases (must match BioCardLayout field order)
// These macros reference fields of the global g_BioCard instance.
// ============================================================================
#define g_BioCardData             ((BYTE*)&g_BioCard)                           // 0x00be9620
#define g_stageId                 (g_BioCard.stageId)                           // BYTE 0x00be9820
#define g_roomId                  (g_BioCard.roomId)                            // BYTE 0x00be9821
#define g_roomCameraId            (g_BioCard.roomCameraId)                      // BYTE 0x00be9822
#define g_AttractMode_RoomCameraId  (g_BioCard.attractMode_RoomCameraId)      // BYTE 0x00be9823
#define g_cutId                   (g_BioCard.cutId)                             // BYTE 0x00be9824
#define g_menu_choice_id          (g_BioCard.menu_choice_id)                  // BYTE 0x00be9825
#define g_selectedItemId          (g_BioCard.selectedItemId)                    // BYTE 0x00be9826
#define g_TotalHeldItems          (g_BioCard.totalHeldItems)                    // BYTE 0x00be9827
#define g_SpecialRoomLightR       (g_BioCard.specialRoomLightR)                 // BYTE 0x00be9828
#define g_CharacterModelId        (g_BioCard.characterModelId)                  // BYTE 0x00be9829
#define g_scdLastEnemyFlags       (g_BioCard.scdLastEnemyFlags)                 // BYTE 0x00be982a - enemy behavior_flags copied by cmd_enemy_flags_get
#define g_bulletEffectId          (g_BioCard.bulletEffectId)                    // BYTE 0x00be982b - last billboard effect spawned by cmd_bullet_effect_spawn
#define g_pickupQtyA              (g_BioCard.pickupQtyA)                        // BYTE 0x00be982c - remembered pick-up quantity, SCD opcode 0x4C index 12
#define g_pickupQtyB              (g_BioCard.pickupQtyB)                        // BYTE 0x00be982d - remembered pick-up quantity, SCD opcode 0x4C index 13
#define g_pickupQtyC              (g_BioCard.pickupQtyC)                        // BYTE 0x00be982e - remembered pick-up quantity, SCD opcode 0x4C index 14
#define g_fwdPosActionId          (g_BioCard.fwdPosActionId)                    // BYTE 0x00be9830 - action-zone entry hit at the forward probed player pos; cleared by room_state_reset
#define g_entPosActionId          (g_BioCard.entPosActionId)                    // BYTE 0x00be9831 - action-zone entry hit at the player entity pos
#define g_usedItemId              (g_BioCard.usedItemId)                        // BYTE 0x00be9832
#define g_pickedItemId            (g_BioCard.pickedItemId)                      // BYTE 0x00be9833 - item id last picked up, read by SCD cmd_picked_item_test; cleared by room_state_reset
#define g_fading_state            (g_BioCard.fadingState)                       // SHORT 0x00be9834
#define g_SpecialRoomLightState   (g_BioCard.specialRoomLightState)             // SHORT 0x00be9836
#define g_SpecialRoomLightDelta   (g_BioCard.specialRoomLightDelta)             // SHORT 0x00be9838
#define g_RandSeed                (g_BioCard.randSeed)                          // SHORT 0x00be983A
#define g_CountdownTimer          (g_BioCard.countdownTimer)                    // SHORT 0x00be983C
#define g_PlayerHealthCopy        (g_BioCard.playerHealthCopy)                  // SHORT 0x00be983E
#define g_PlayerDpadHeld          (g_BioCard.playerDpadHeld)                    // SHORT 0x00be9840
#define g_PlayerDpadPressed       (g_BioCard.playerDpadPressed)                 // SHORT 0x00be9842
// The live play timer. main_loop mirrors Game_timer into it every frame
// (0x004298a5) and title_state restores Game_timer from it after a load
// (0x00430686), so it has to be the BioCard field the save block writes -
// a separate global here would drop the saved play time on every load.
#define g_gameTimerSnapshot       (g_BioCard.gameTimerSnapshot)                 // DWORD 0x00be9844
#define g_SavesCounter            (g_BioCard.savesCounter)                      // BYTE 0x00be9848
#define g_EquippedItemId          (g_BioCard.equippedItemId)                    // BYTE 0x00be9849
#define g_RoomItemBackup          (g_BioCard.roomItemBackup)                    // BYTE 0x00be984A
#define g_SelectedCharactedId     (g_BioCard.selectedCharactedId)               // BYTE 0x00be984B
#define g_PlayerPosXCopy          (g_BioCard.playerPosXCopy)                    // SHORT 0x00be984C
#define g_PlayerPosZCopy          (g_BioCard.playerPosZCopy)                    // SHORT 0x00be984E
#define g_PlayerDirAngleCopy      (g_BioCard.playerDirAngleCopy)                // SHORT 0x00be9850
#define g_PlayerHealthStatusCopy  (g_BioCard.playerHealthStatusCopy)            // BYTE 0x00be9852
#define g_DcGameMode              (g_BioCard.dcGameMode)                        // BYTE 0x00be9853 - Director's Cut mode, saved with the card (PS1 g_abDcGameMode)

#define g_ScenarioFlags2          (g_BioCard.scenarioFlags2)                    // BYTE[32] 0x00be9854
#define g_LocksFlags              (g_BioCard.locksFlags)                        // BYTE[8] 0x00be9874
#define g_EnemiesFlags            (g_BioCard.enemiesFlags)                      // BYTE[32] 0x00be987c
#define g_roomItemsFlags          (g_BioCard.roomItemsFlags)                    // BYTE[32] 0x00be989c
#define g_itemExaminedFlags       (g_BioCard.itemExaminedFlags)                 // BYTE[4] 0x00be98bc - "item examined" flag bank (real vs generic item names)
#define g_ScenarioFlags           (g_BioCard.scenarioFlags)                     // BYTE[16] 0x00be98c0
#define g_RoomFlags               (g_BioCard.roomFlags)                         // BYTE[20] 0x00be98d0

#define g_itemboxSlots            (g_BioCard.itemboxSlots)                      // ItemSlot[48] 0x00be98e4
#define g_ItemsSlots              (g_BioCard.itemsSlots)                        // ItemSlot[6] 0x00be9944
#define g_RebeccaItemSlots        (g_BioCard.rebeccaItemsSlots)                 // ItemSlot[6] 0x00be9950

#define g_roomBgmState            (g_BioCard.roomBgmState)                      // BYTE[224] 0x00be995c

// ============================================================================
// Scenario flag bit constants (banks 0 and 1).
// Meanings verified from ported code; full tables with set-by/read-by:
// docs/SCENARIO_FLAGS.md. Bits used only by room SCD scripts (RDT data) have
// no constant yet - add one here (and a row in the doc) when a bit is traced.
// ============================================================================
// --- g_ScenarioFlags (bank 0, 0x00be98c0) ---
#define SCENARIO_FLAG_STAGE_VARIANT       0x00  // character room variant flag (0: chris, 1: jill)
#define SCENARIO_FLAG_YAWN_BITE           0x10  // Yawn bite event (set by the Yawn attack entry)
#define SCENARIO_FLAG_ITEM13_USE_LOCK     0x13  // item 0x13 use rejected while set (progression gate)
#define SCENARIO_FLAG_CHEMICAL_COMBINE    0x16  // chemical combine performed (combine effect 4)
#define SCENARIO_FLAG_PANEL_VARIANT_A     0x1E  // passcode-panel initial-state variant selector
#define SCENARIO_FLAG_PANEL_VARIANT_B     0x1F  // passcode-panel initial-state variant selector
#define SCENARIO_FLAG_INTERACTIVE_SCREEN  0x20  // interactive screen active gate
#define SCENARIO_FLAG_PANEL_SOLVED        0x21  // lab passcode panel solved
#define SCENARIO_FLAG_PLANT42_DEAD        0x29  // Plant 42 defeated
#define SCENARIO_FLAG_ALTERNATE_OUTFIT    0x2A  // alternate outfit (model id + 8)
#define SCENARIO_FLAG_WESKER_VARIANT      0x37  // Wesker later-animation variant
#define SCENARIO_FLAG_YAWN_SERUM          0x47  // Yawn serum marker - first Yawn poisons only while clear
// 0x48/0x49/0x4A acknowledge an objective for the map highlight (see
// docs/SCENARIO_FLAGS.md), but each has other setters and readers, so they
// keep generic names.
#define SCENARIO_FLAG_PROGRESS_48         0x48  // acks SCENARIO2_FLAG_PLANT42_OBJ (the BGM condition is bank 1's 0x48, not this one)
#define SCENARIO_FLAG_PROGRESS_49         0x49  // acks serum objective 1 - set by the serum room's init script
#define SCENARIO_FLAG_PROGRESS_4A         0x4A  // acks serum objective 2 - set by the serum room's init script
#define SCENARIO_FLAG_MONSTER_PLANT_PROG  0x5B  // monster plant combat progression (3+ hits)
// Director's Cut only (PS1 ENDING overlay 0x800e1914, DC-exclusive): raised
// when an ADVANCED run reaches ending 6 or 7. The USA build never sets or reads
// it; DC mode does, alongside the magnum it grants for the next cycle.
#define DC_SCENARIO_FLAG_INF_COLT_PYTHON  0x7A  // ADVANCED cleared on a best ending
#define SCENARIO_FLAG_SECOND_PLAYTHROUGH  0x7B  // second playthrough ("hard mode") - set after clearing
#define SCENARIO_FLAG_HAS_LOCKPICK        0x7C  // has the lockpick (Jill)
#define SCENARIO_FLAG_MENU_FADE_LATCH     0x7D  // fade-in latch, cleared when the menu closes
#define SCENARIO_FLAG_INF_R_LAUNCHER      0x7E  // infinite rocket launcher flag
#define SCENARIO_FLAG_HAS_RADIO           0x7F  // has the radio (item 0x4D)

// --- g_ScenarioFlags2 (bank 1, 0x00be9854) ---
#define SCENARIO2_FLAG_JILL_FIRST_RUN     0x0B  // Jill first-playthrough marker (with roomItemsFlags 0x34)
#define SCENARIO2_FLAG_PROGRESS_22        0x22  // radio-tab availability gate (character id & 3 == 3)
// 0x23/0x24 do Jill's two serum objectives in ROOM1001 the way 0x2D/0x2E do
// Chris's, but they are NOT dedicated to that: ROOM3070 (a Chris file) sets
// 0x23 in a block with 0x30/0x33/0xC0, and ROOM3030 clears 0x22/0x23/0x24
// together with audio side effects. Reused bits - left generic on purpose.
#define SCENARIO2_FLAG_PROGRESS_23        0x23  // Jill serum objective 1 in ROOM1001; reused in stages 3/6
#define SCENARIO2_FLAG_PROGRESS_24        0x24  // Jill serum objective 2 in ROOM1001; reused in stages 3/6
#define SCENARIO2_FLAG_SERUM_OBJ1_CHRIS   0x2D  // Chris: serum objective 1 outstanding (pillar-passage cutscene)
#define SCENARIO2_FLAG_SERUM_OBJ2_CHRIS   0x2E  // Chris: serum objective 2 outstanding (front-of-attic cutscene)
#define SCENARIO2_FLAG_PLANT42_OBJ        0x38  // Plant 42 objective outstanding (V-JOLT hunt); cleared with SCENARIO_FLAG_PLANT42_DEAD. Also gates Rebecca's radio tab
#define SCENARIO2_FLAG_YAWN_POISONED      0x43  // poisoned by Yawn (cleared by the serum)
#define SCENARIO2_FLAG_PROGRESS_48        0x48  // stage 2 room 7 keeps 2 BGM channels when set
#define SCENARIO2_FLAG_SECOND_SURVIVOR    0x4B  // ending "second survivor" bit
#define SCENARIO2_FLAG_PROGRESS_55        0x55  // stage 2 room 7 Barry/Hunter cutscene already played
#define SCENARIO2_FLAG_PROGRESS_5C        0x5C  // stage 2 room 7 third BGM channel condition (Jill)
#define SCENARIO2_FLAG_PARTNER_ALIVE      0xC0  // ending "partner survived" bit

// --- g_RoomFlags (bank 8, 0x00be98d0, 20 bytes = 160 bits) ---
// Three separate bit blocks share this bank, each indexed off its own base.
// Every accessor is a two-line wrapper around Flg_on/Flg_ck, so the base IS the
// only thing that distinguishes them:
//
//   0x00..0x7B  rooms VISITED, g_StageRoomFlagOffset[stageId % 5] + roomId
//               (room_set_visited_flag 0x00488570; read by the map screen).
//               Per map group: 1F 0..31, 2F 32..62, courtyard+underground
//               63..81, guardhouse 82..99, laboratory 100..123 - the bases are
//               the running sum of g_MapRoomCounts, so the block ends exactly
//               where the map block begins.
//   0x7C..0x81  MAP owned, ROOM_FLAG_MAP_BASE + map index
//               (set_room_item_seen_flag 0x004885a0, read by map_area_known
//               0x004885c0). See ITEM_MAP_* / MAP_INDEX_* in Types.h.
//   0x82..0x91  FILE collected, ROOM_FLAG_FILE_BASE + (itemId - 0x5F), the 16
//               documents (0x00488660, read by pickup_item_seen 0x00488680).
//   0x92..0x9F  unused.
//
// NOTE: the numeric overlap is a coincidence of the two bases - decimal 82 is
// the guardhouse VISITED base, hex 0x82 is the FILE base. They are different
// blocks. 0x00488660 takes a file index, not a room id - it was named
// map_set_room_flag; both Ghidra and the port now call it
// file_set_collected_flag.
//
// Room SCD scripts reach the whole bank as flag bank 8 (cmd_bit_test/cmd_bit_op
// and the flag_bank_set room action at 0x0041b918), so scripts can touch bits in
// any of these blocks.
#define ROOM_FLAG_MAP_BASE                 0x7C  // + MAP_INDEX_* -> that map is owned
#define ROOM_FLAG_FILE_BASE                0x82  // + (itemId - 0x5F) -> that file has been collected




