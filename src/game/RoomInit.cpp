// RoomInit.cpp - Room initialization (decompiled from Ghidra)
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include "SpriteRenderer.h"
#include "dc/ItemDescriptions.h"
#include "dc/EntityModels.h"    // the arrange wardrobe's two outfit flags
#include "../system/AssetPath.h"
#include <cstdio>

// Forward declarations for helpers defined in other files
extern void SetupCharacterData(void);
extern void LoadSoundBank(int, void*);
extern void load_character_sfx(unsigned char);

extern void Object_DeleteAll(int);
extern void object_delete_00442170(int category);
extern void texture_queue_reset(void);
extern void Flg_on(int baseAddr, unsigned int bitIndex);

// Room sub-functions (see Globals.h for the full set)
extern void room_events_check(void);
extern void load_room_bg(void);
extern void load_room_bg_masks(void);
extern void check_camera_switch(int param);
extern void display_room_camera_bg(void);
extern void Room_SetupCollisionCallbacks(void);
extern void Room_LoadEnemySoundBanks(void);

#include "dc/ArrangeStages.h"   // room_file_stage()

// Entity model loading functions from EntityModelLoader.cpp
extern void LoadEntityEMD(Entity* em, unsigned char entity_id);
extern void Entity_SetJoints(Entity* em, unsigned int param2);
extern void InitAnimStructure(void* animHeaderValue);
extern unsigned int SetupJointStructures(unsigned int param1);

// SCD script runner
extern void run_command_functions(unsigned short* scd_opcodes);

// Texture helpers defined elsewhere
extern void delete_texture_set_secondary(unsigned char);
extern void TexturePage_DeleteSet(unsigned char);

// ============================================================================
// set_message_display (0x00455670)
// Sets up message display state. Called when the game needs to show a text
// message (loading screens, item descriptions, door prompts, etc.).
//
// msg_id: Message identifier
//   - bit 7 (0x80): speed up flag
//   - bit 6 (0x40): if set, use global_messages table; if clear, use RDT messages
//   - bits 0-5: message index within the table
// pause_game: if non-zero, pause game flags during message display
//
// Returns: 0 = message display started, 1 = already displaying (rejected)
// ============================================================================
unsigned int set_message_display(unsigned short msg_id, unsigned short pause_game)
{
    short screenY;

    if ((g_menu_choice_id & 0x80) != 0) {
        return 1;
    }

    g_menu_choice_id = 0x80;

    g_messageFlagsBackup = g_message_flags;
    g_PauseGameInMsgFlag = pause_game;
    g_message_flags = g_message_flags & ~pause_game;

    if ((msg_id & 0x40) == 0) {
        // Room-specific message from RDT data
        // messages pointer at RDT+0x74 points to a message table:
        //   [offset_table: uint16[msg_count]] followed by [message_data...]
        if (g_RdtPointer != NULL && g_RdtPointer->messages != NULL) {
            unsigned char* msgBase = g_RdtPointer->messages;
            unsigned short offset = *(unsigned short*)(msgBase + (msg_id & 0x3F) * 2);
            g_MessagePtr = msgBase + offset;
        }
    } else {
        // Global message from the global_messages lookup table. The Japanese
        // release keeps its own copy of the table (Biohazard.exe 0x004cde58,
        // its set_message_display at 0x00491980), so the region that owns the
        // asset tree owns the text too — the JPN glyph encoding only makes
        // sense against data\FONT.TIM.
        if (GetAssetVersion() != 0) {
            g_MessagePtr = global_messages_jpn[msg_id & 0x3F];
        } else {
            g_MessagePtr = global_messages[msg_id & 0x3F];
        }
    }

    g_MessageStateCounter = 0;

    if ((g_main_state_flags & MSF_MENU_BYTE) == 0) {
        screenY = 181;
    } else {
        screenY = 186;
    }

    g_MessageScreenY = screenY - (short)g_ScreenOffsetY;
    g_lastScanCodeOrMsgID = (DWORD)msg_id;
    g_MessageSpeedUpFlag = (unsigned char)(msg_id & 0x80);

    return 0;
}

// DC + JPN description table. The Japanese release has no Director's Cut table
// of its own, and the DC's is English, so in DC + JPN the DC's edits are
// applied to the Japanese table instead: item 0x04 takes the Japanese Beretta
// text (there is no JPN "custom edition" wording in the port's data) and the
// two MOON CREST halves take the Japanese "nothing important" string, which is
// exactly what the DC uses there. Built once, on first use.
static unsigned char** dc_jpn_item_descriptions(void)
{
    static unsigned char* table[79];
    static int built = 0;
    if (!built) {
        for (int i = 0; i < 79; ++i) {
            table[i] = g_ItemDescriptionsJpn[i];
        }
        table[0x04 - 1] = g_ItemDescriptionsJpn[0x02 - 1];   // Beretta
        table[0x31 - 1] = g_ItemDescriptionsJpn[0x1d - 1];   // Nothing important
        table[0x32 - 1] = g_ItemDescriptionsJpn[0x1d - 1];   // Nothing important
        built = 1;
    }
    return table;
}

// DC + USA description table. The DC has no sub machine guns, so its table's
// last real entry is 0x4C ("The battery is still charged.") and 0x4D/0x4E - the
// two indices the item viewer passes for the INGRAM / MINIMI examine - hold
// stale pointers outside the string pool (0x80080F3C / 0x80031604), which the
// generator's tail fallback turns into another copy of 0x4C's text. The port
// keeps both PC weapons in DC mode, so those two indices take the USA text.
// Built once, on first use.
static unsigned char** dc_usa_item_descriptions(void)
{
    static unsigned char* table[DC_ITEM_DESCRIPTION_COUNT];
    static int built = 0;
    if (!built) {
        for (int i = 0; i < DC_ITEM_DESCRIPTION_COUNT; ++i) {
            table[i] = g_dcItemDescriptions[i];
        }
        table[0x4d] = g_ItemDescriptions[0x4d];   // A sub machine gun loaded with 9mm bullets.
        table[0x4e] = g_ItemDescriptions[0x4e];   // A full-automatic light-weight machine gun.
        built = 1;
    }
    return table;
}

// ============================================================================
// set_item_description_message (0x00455730)
// The item viewer's own message setter: shows the examine description of the
// item whose 3D model is on screen. Unlike set_message_display() the index is
// a plain index into g_ItemDescriptions (itemId - 1) - no 0x40 table-select
// bit, no RDT lookup - the speed-up flag is always on, and the text is placed
// at the item viewer's line (0xba) instead of the room dialogue line.
//
// descIndex: index into g_ItemDescriptions (item id - 1)
// pauseGame: flags to clear in g_message_flags while the text is up
//
// Returns: 0 = message display started, 1 = rejected (already displaying)
// ============================================================================
unsigned int set_item_description_message(unsigned short descIndex, unsigned short pauseGame)
{
    if ((g_menu_choice_id & 0x80) != 0) {
        return 1;
    }

    // The Japanese release keeps its own description table (0x004c9370, read
    // by its set_item_description_message at 0x00491a40). The Director's Cut has
    // its own too (SLUS_005.51 0x8008E9C4): item 0x04 reads "A beretta M92FS
    // Automatic. / Custom edition." and the two MOON CREST halves read
    // "Nothing important." - and it drops the two sub machine gun entries, which
    // dc_usa_item_descriptions() puts back (the port keeps the PC weapons).
    //
    // Its strings must use STR()'s DOUBLED parser escapes (see
    // tools/gen_dc_item_descriptions.py); a single "\n" became raw 0x0A and hung
    // the message renderer on 2026-09-14.
    //
    // The version picks the language first, so DC + JPN applies the DC's edits
    // to the Japanese table rather than switching to the English one.
    const int jpn = (GetAssetVersion() != 0);
    unsigned char** descriptions = jpn
        ? (g_bDcMode ? dc_jpn_item_descriptions() : g_ItemDescriptionsJpn)
        : g_bDcMode ? dc_usa_item_descriptions()
                    : g_ItemDescriptions;

    // The original indexes the table unchecked; the entries past the last item
    // are the zero padding that follows it, so an out-of-range id would set a
    // NULL g_MessagePtr and fault in UpdateMessageDisplay. Refuse instead.
    if (descIndex >= (sizeof(g_ItemDescriptions) / sizeof(g_ItemDescriptions[0])) ||
        descriptions[descIndex] == NULL) {
        return 1;
    }

    g_menu_choice_id = 0x80;

    g_messageFlagsBackup = g_message_flags;
    g_PauseGameInMsgFlag = pauseGame;
    g_message_flags = g_message_flags & ~pauseGame;

    g_MessageStateCounter = 0;
    g_MessageSpeedUpFlag = 0x80;
    g_lastScanCodeOrMsgID = (DWORD)descIndex;
    g_MessagePtr = descriptions[descIndex];
    g_MessageScreenY = 0xba - (short)g_ScreenOffsetY;

    return 0;
}

// ============================================================================
// display_game_loading_message (0x00481930)
// Task function: displays the appropriate loading message based on game state.
// Runs as a parallel task during InitializeGame.
//
// Message IDs (bit 6 set = global message table):
//   0x5B = "New game" / initial loading message
//   0x5C = "Loading game" / loading saved game message
//   0x5D = "Soft reset" / attract mode transition message
// ============================================================================
void display_game_loading_message(void)
{
    if ( (g_main_state_flags2 & MSF2_ATTRACT_DEMO) != 0 ) {
        set_message_display(0x5d, 0);
        Task_exit();
    }
    if ( (g_main_state_flags & MSF_CONTINUE_GAME) != 0 ) {
        set_message_display(0x5c, 0);
        Task_exit();
    }
    set_message_display(0x5b, 0);
    Task_exit();
}

// ============================================================================
// init_room (0x00409990)
// Initializes room state: sets up stage data pointer, room relations table,
// animation function pointers, and calls room_set for full room initialization.
// ============================================================================
void init_room(void)
{
    g_AttractMode_RoomCameraId = 0x1f;
    g_BGM_STATE = 0xFF;
    g_loadDataDestPointer = g_DataBuffer;
    // Both tables are indexed by the BASE stage row, so a DC arrange stage
    // (id 7-13) shares its base stage's voice offsets and BGM state rather than
    // running off the end of either. g_roomBgmState is 224 bytes inside
    // g_BioCard, so an unfolded stage 7 would have written into the save block's
    // next field. (Globals.h, get_stage_id)
    g_StageDataPtr = (void*)g_StageVoiceOffsetTable[get_stage_id()];
    g_RoomBgmStatePtr = &g_roomBgmState[get_stage_id() * 32];

    set_player_animations_functions();

    g_RdtLoadDataBackup = g_loadDataDestPointer;

    if ( (g_main_state_flags2 & MSF2_ATTRACT_DEMO) != 0 ){
        // 0x004099f6: Force silence for this room's BGM (0xFF = no BGM)
        g_RoomBgmStatePtr[g_roomId] = 0xFF;
    }
    room_set();
}

// ============================================================================
// room_action_table_reset (0x00477f80)
// Makes all 24 room action slots inert and rewinds the tail. It zeroes ONLY
// byte 0 of each slot - the room_check_actions handler index, which the probe
// loops test first - so the other 11 bytes still hold the previous room's
// values until a door_set / room_action_set / item_model_set overwrites them.
// Called on room load, before the room's init SCD rebuilds the table.
// ============================================================================
void room_action_table_reset(void) {
    unsigned char* p = g_RoomActionTable;
    do {
        *p = 0;
        p += 12;
    } while (p < g_RoomActionTable + 288);
    g_RoomActionTail = g_RoomActionTable;
}

// ============================================================================
// room_set_visited_flag (0x00488570)
// Sets the visited flag for the current room in the room flags bitfield.
// Uses g_StageRoomFlagOffset[stageId % 5] + roomId as the bit index.
// ============================================================================
void room_set_visited_flag(unsigned char stageId, unsigned char roomId) {
    Flg_on((int)g_RoomFlags, (unsigned int)g_StageRoomFlagOffset[stageId % 5] + roomId);
}

// ============================================================================
// room_state_reset (0x00475700)
// Resets room state: clears SCD system flags word 1, the used-item id, the
// event-item id and g_fwdPosActionId. Called from room_set and game_loop. Note:
// room_set also clears SysFlags word 0.
//
// Original (00475700):
//   XOR EAX,EAX
//   MOV [0x00be9833],AL    <- g_pickedItemId (event item id) = 0
//   MOV [0x00be9832],AL    <- g_usedItemId                = 0
//   MOV [0x00be41cc],EAX   <- g_SysFlags[1]               = 0
//   MOV [0x00be9830],EAX   <- g_fwdPosActionId            = 0
// The port previously omitted the first two clears, so g_usedItemId stayed set
// after any item use (menu "use", use_room_action_item) and the room SCD's
// obj10_test item-use dispatch kept firing on later frames - in room10f1 the
// piano cutscene re-triggered every frame instead of playing once.
// ============================================================================
void room_state_reset(void) {
    g_pickedItemId = 0;
    g_usedItemId = 0;
    g_SysFlags[1] = 0;
    g_fwdPosActionId = 0;
}

// ============================================================================
// lab_slides_reset (0x0042a020)
// Resets lab slides function index and state to zero.
// ============================================================================
void lab_slides_reset(void) {
    g_labSlidesFuncIndex = 0;
    g_labSlidesState = 0;
}

// ============================================================================
// room_set (0x00477720)
// Full room initialization: deletes old textures/objects, loads RDT, sets up
// enemies, cameras, collision, sound, BGM, events, and player state.
// ============================================================================
void room_set(void)
{
    Entity* pPrevEntity;
    unsigned int i;

    // 0x00477720: g_AttractModeIdleTimer = 1
    g_AttractModeIdleTimer = 1;

    printf("room_set start\n");

    // Delete secondary texture sets 0x17-0x1d
    delete_texture_set_secondary(0x1d);
    delete_texture_set_secondary(0x1c);
    delete_texture_set_secondary(0x1b);
    delete_texture_set_secondary(0x1a);
    delete_texture_set_secondary(0x19);
    delete_texture_set_secondary(0x18);
    delete_texture_set_secondary(0x17);

    // Delete main texture sets 0x30, 0x2e
    TexturePage_DeleteSet(0x30);
    TexturePage_DeleteSet(0x2e);

    printf("sprite delete end\n");

    // Delete objects (category 1)
    object_delete_00442170(1);
    Object_DeleteAll(1);

    printf("object delete end\n");

    // 0x0047780d: Clear lower nibble of main state flags and input flags
    g_main_state_flags &= ~MSF_ROOM_RESET_MASK;
    g_main_state_flags2 &= ~MSF2_ROOM_RESET_MASK;

    // 0x0047782e: Restore g_loadDataDestPointer from backup
    g_loadDataDestPointer = g_RdtLoadDataBackup;

    // 0x00477833: Reset texture queue
    texture_queue_reset();

    // 0x00477838: Reset the room action table
    room_action_table_reset();

    // 0x0047783d: Zero SCD system flags (flag bank 4, both DWORDs)
    g_SysFlags[0] = 0;
    g_SysFlags[1] = 0;

    // 0x0047785b: Clear SCD event active flags (8 entries)
    for (int j = 0; j < 8; j++) {
        g_ScdEventTable[j].active = 0;
    }

    // 0x00477879: Clear bit 0x100000 of g_main_state_flags
    g_main_state_flags &= ~MSF_CAMERA_LOCK;

    // 0x0047788d: Reset player entity fields
    g_playerEntity.unk_e0 = 0;
    g_playerEntity.healthStatusFlags &= 0xbf;

    // 0x004778a1: Set room visited flag
    room_set_visited_flag(g_stageId, g_roomId);

    // 0x004778b8: g_specialRoomLightState = 0xffff
    g_SpecialRoomLightState = 0xffff;

    // 0x004778bd: Reset room state counters
    room_state_reset();

    // 0x004778d2: Clear saved texture bank ID (low byte = validity flag)
    g_SavedTextureBankID &= 0xff00;

    // 0x004778d7: Reset lab slides state
    lab_slides_reset();

    printf("etc set end\n");

    // 0x00477904: Character model switching logic
    if (g_playerEntity.id != g_CharacterModelId) {
        if ((g_CharacterModelId & 8) == 0) {
            if ((g_CharacterModelId < 4) && (g_playerEntity.id < 4)) {
                g_playerEntity.id = g_CharacterModelId;
                if (g_CharacterModelId == 3) {
                    // 0x004778c6: Switch to Rebecca
                    HEALTH_STATUS_BKP = (unsigned short)g_playerEntity.healthStatusFlags;
                    HEALTH_BKP = g_playerEntity.health;
                    g_playerEntity.health = 88;
                    g_playerEntity.maxHealth = 88;
                    if (g_bDcMode) {
                        // DC room_set: Rebecca gets the mode's own health too
                        // (PS1 0x8c / 0x40), health and max together.
                        if (g_DcDifficulty == DC_DIFFICULTY_TRAINING) {
                            g_playerEntity.health = 140;
                            g_playerEntity.maxHealth = 140;
                        } else if (g_DcDifficulty >= DC_DIFFICULTY_ADVANCED) {
                            g_playerEntity.health = 64;
                            g_playerEntity.maxHealth = 64;
                        }
                    }
                    g_RoomItemBackup = g_EquippedItemId;
                    g_playerEntity.healthStatusFlags = 0;
                    g_EquippedItemId = 0;
                    g_ItemSlotsPointer = g_RebeccaItemSlots;
                } else {
                    // 0x00477914: Restore from backup
                    g_playerEntity.health = HEALTH_BKP;
                    g_playerEntity.healthStatusFlags = (unsigned char)HEALTH_STATUS_BKP;
                    g_EquippedItemId = g_RoomItemBackup;
                    g_playerEntity.maxHealth = 140;
                    if (g_bDcMode) {
                        // Restore to the mode's max, not the stock 140. The DC
                        // uses the flat per-mode value here (no character
                        // adjust) exactly as the original does - only Chris's
                        // scenario reaches the Rebecca swap.
                        if (g_DcDifficulty == DC_DIFFICULTY_TRAINING) {
                            g_playerEntity.maxHealth = 180;
                        } else if (g_DcDifficulty >= DC_DIFFICULTY_ADVANCED) {
                            g_playerEntity.maxHealth = 100;
                        }
                    }
                    g_ItemSlotsPointer = g_ItemsSlots;
                    g_RoomItemBackup = 0;
                }
            }
        } else {
            // 0x00477888 sets the costume bit unconditionally, which is right
            // for the USA: its alternate outfit is a one-way unlock.
            //
            // The Director's Cut's arrange wardrobe can put the player BACK in
            // the arrange-exclusive outfit, so its room_set (SLUS_005.51
            // 0x80044138) makes the same store conditional:
            //
            //     if ((g_status_flags & 0x20000) == 0)          // not ADVANCED
            //         g_status_flags |= 0x4000000;
            //     else if (Flg_ck(&g_gameOptionsFlags, 0x6f) ||
            //              Flg_ck(&g_gameOptionsFlags, 0x70))
            //         g_status_flags |= 0x4000000;
            //     else
            //         g_status_flags &= 0xfbffffff;             // CLEAR it
            //
            // i.e. in ADVANCED the bit tracks the two wardrobe flags, and with
            // neither set the player is in the arrange outfit. Without the
            // clear, LoadEntityEMD keeps taking its costume branch forever:
            // the index becomes DC_EMD_INDEX_COSTUME and, with OUTFIT_A also
            // cleared by the script, nothing maps it back down - so changing
            // out of an OG outfit landed on the OG ALTERNATE instead of the
            // arrange one. The wardrobe writes characterModelId = 8 for every
            // one of the three choices, so this store is the only thing that
            // tells them apart.
            if ((g_main_state_flags2 & MSF2_DC_ADVANCED) != 0 &&
                Flg_ck((int)g_ScenarioFlags, DC_SCENARIO_FLAG_OUTFIT_A) == 0 &&
                Flg_ck((int)g_ScenarioFlags, DC_SCENARIO_FLAG_OUTFIT_B) == 0) {
                g_main_state_flags2 &= ~MSF2_COSTUME_VARIANT;
            } else {
                g_main_state_flags2 |= MSF2_COSTUME_VARIANT;
            }
            g_CharacterModelId &= 7;
        }

        // 0x00477950: Common character setup (reached when bit3 set or ids < 4)
        if ((g_CharacterModelId & 8) != 0 || (g_CharacterModelId < 4 && g_playerEntity.id <= 3)) {
            LoadHeldItemsImages();
            g_playerEntity.animationId = 0;
            g_playerEntity.animFrameId = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state = 0;
            SetupCharacterData();
            LoadSoundBank(g_playerEntity.equippedWeaponId, g_loadDataDestPointer);
        }

        // 0x00477982: Update player id and load character SFX
        g_playerEntity.id = g_CharacterModelId;
        load_character_sfx(g_CharacterModelId);
    }

    // 0x0047799c:
    printf("player set end\n");

    // 0x004779b4: object_delete_00442170(2)
    object_delete_00442170(2);

    // 0x004779c7: LoadRoomRdt()
    LoadRoomRdt();

    // 0x004779d6: object_delete_00442170(3)
    object_delete_00442170(3);

    // 0x004779f3: Set room event flags for room 0x13 camera 4. The original
    // tests the raw room id with no stage guard; the Tyrant-room (main lab)
    // is the only one with a camera 4.
    if ((g_roomCameraId == 4) && (g_roomId == ROOM_MAIN_LAB)) {
        Flg_on((int)&g_EnemiesFlags, 0x52);
        Flg_on((int)&g_EnemiesFlags, 0x53);
        Flg_on((int)&g_EnemiesFlags, 0x54);
    }

    printf("after of Room load\n");

    // 0x00477a37: Setup collision boundaries and 3D sound callbacks
    Room_SetupCollisionCallbacks();

    // 0x00477a45: object_delete_00442170(4)
    object_delete_00442170(4);

    // 0x00477a58: Load per-room enemy sound banks
    Room_LoadEnemySoundBanks();

    // 0x00477a66: object_delete_00442170(5)
    object_delete_00442170(5);

    printf("after of Sound rom set\n");

    // 0x00477a92: InitRoomEffSprite()
    InitRoomEffSprite();

    printf("after of Init eff sprite\n");

    // 0x00477ac5: Sound bank loading loop
    i = 0;
    object_delete_00442170(6);

    g_loadDataDestPointer = g_RdtPointer->vab_sound_file;
    if (g_RdtPointer->omodel_slot_count != 0) {
        do {
            g_omodel_table[i] = g_loadDataDestPointer;
            *(unsigned char*)g_loadDataDestPointer = 0;
            g_loadDataDestPointer = (char*)g_loadDataDestPointer + 0xa4;
            i++;
        } while (i < g_RdtPointer->omodel_slot_count);
    }

    // Item model record slots
    i = 0;
    if (g_RdtPointer->item_count != 0) {
        do {
            g_item_model_table[i] = g_loadDataDestPointer;
            *(unsigned char*)g_loadDataDestPointer = 0;
            g_loadDataDestPointer = (char*)g_loadDataDestPointer + 0xa4;
            i++;
        } while (i < g_RdtPointer->item_count);
    }

    // 0x00477bc8: Reset enemy count and model state
    i = 0;
    g_enemy_count = 0;
    g_omodelCount = 0;
    g_LastEnemyModelId = 0xff;
    g_TextureBankID = 0x06;
    g_TextureCurrentPage = 0x0a;
    g_ItemModelCount = 0;

    object_delete_00442170(7);

    printf("after of model\n");

    // 0x00477c0f: run_command_functions(g_RoomInitScd)
    run_command_functions((unsigned short*)g_RoomInitScd);

    // 0x00477c21: g_message_flags |= 0x80
    g_message_flags |= 0x80;

    printf("after of Scenario check\n");

    // 0x00477c45: room_events_check()
    room_events_check();

    // 0x00477c54: g_message_flags &= ~0x80
    g_message_flags &= ~0x80;

    object_delete_00442170(8);

    printf("after of Event\n");

    object_delete_00442170(9);

    // 0x00477c84: Set player joint movement data from RDT
    g_playerEntity.jointMoveData2 = (unsigned int)g_RdtPointer->player_anim_header;
    g_playerEntity.jointMoveData3 = (unsigned int)g_RdtPointer->player_anim_base;

    // 0x00477ca0: Reset enemy model cache and set up entity loop
    g_LastEnemyModelId = 0xff;

    // 0x00477cc3: Enemy entity loading loop
    ENTITY = g_EnemiesList;
    pPrevEntity = NULL;
    if (g_enemy_count != 0) {
        do {
            if ((ENTITY->status_flags & 1) != 0) {
                if (g_LastEnemyModelId == ENTITY->id) {
                    // 0x00477bf7: Reuse previous enemy model data
                    ENTITY->animHeader = pPrevEntity->animHeader;
                    ENTITY->animBase = pPrevEntity->animBase;
                    ENTITY->modelLoadBuffer = pPrevEntity->modelLoadBuffer - 0xc;
                } else {
                    // 0x00477bdc: Load new enemy model
                    g_LastEnemyModelId = ENTITY->id;
                    LoadEntityEMD(ENTITY, ENTITY->id + 4);
                }
                Entity_SetJoints(ENTITY, 0x7c);
                InitAnimStructure((void*)ENTITY->modelLoadBuffer);
                g_loadDataDestPointer = (void*)SetupJointStructures((unsigned int)g_loadDataDestPointer);
                pPrevEntity = ENTITY;
                if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
                    SetupEntityJointAnimation();
                }
                i++;
            }
            ENTITY++;
        } while (i < (unsigned int)g_enemy_count);
    }

    // 0x00477c89: Setup texture bank data
    SetupTextureBankData((short)g_TextureCurrentPage);
    // 0x00477ca0: Save current texture bank ID for cutscene restoration
    g_SavedTextureBankID = *(unsigned short*)&g_TextureBankCell;
    g_CurrentRdtDataTypePtr = g_RdtPointer->cam_switch_zones;

    object_delete_00442170(10);

    printf("after of model set\n");

    // 0x00477e7c: Special case for lab visual data room (slide projector)
    if ((g_stageId == STAGE_LABORATORY) && (g_roomId == ROOM_VISUAL_DATA_ROOM)) {
        load_slides_images();
    }

    // 0x00477e9f: load_room_bg()
    load_room_bg();

    printf("after of Bg room set\n");

    object_delete_00442170(0xb);

    // 0x00477ec8: load_room_bg_masks()
    load_room_bg_masks();

    object_delete_00442170(0xc);

    // 0x00477ef6: Camera setup
    if ((g_main_state_flags & MSF_CAMERA_LOCK) == 0) {
        g_roomCameraId = 0;
        check_camera_switch(1);
    } else {
        display_room_camera_bg();
    }

    // 0x00477f26: Special case for the courtyard heliport
    if ((g_stageId == STAGE_COURTYARD) && (g_roomId == ROOM_HELIPORT)) {
        DAT_00d213c0 = g_loadDataDestPointer;
    }

    object_delete_00442170(0xd);

    g_AttractModeIdleTimer = 0;

    printf("room_set end\n");
}

// ============================================================================
// RDT loading and background colour (moved here from GameState.cpp)
// ============================================================================

extern void SetSpriteBufferFlag(void);
extern void empty_40ae40(int);

// (0x00477d90) - Load RDT file for current room
// Loads the Room Definition Table for the current stage/room, resolves internal
// relative pointers to absolute addresses, and sets up SCD script pointers.
void LoadRoomRdt(void)
{
    static const char hexDigits[] = "0123456789abcdef";

    // 0x00477d97: Set g_RdtPointer to the current load buffer
    g_RdtPointer = (RDT*)g_loadDataDestPointer;

    // 0x00477d9c: ESI = start of camera data (past RDT header)
    unsigned char* cameras = (unsigned char*)(g_RdtPointer + 1);

    // 0x00477da4-0x00477e02: Build RDT file path
    // Format: ./usa/stageX/roomXYYZ.rdt where X=stage, YY=room, Z=flag
    //
    // The stage digit is room_file_stage(), not g_stageId: in ADVANCED a room
    // that has an arrange version loads it out of STAGE8-E instead (PS1
    // 0x80043fb4, whose LoadRoomRdt calls it for exactly this). The identity
    // otherwise, so OG and the other two difficulties build the path they
    // always did.
    const unsigned char fileStage = room_file_stage();

    // The variant digit. The PC release ships a `...1` file for every room, so
    // this build could always append the character bit and find something; the
    // DC shares one file wherever the PS1 does and its own table says which
    // (SLUS_005.51 0x80090f48, read by its LoadRoomRdt for exactly this). Jill
    // entering arrange 2F asked for a ROOM9011 that is not on the disc.
    int variant = (g_main_state_flags & MSF_CHAR_VARIANT) ? 1 : 0;
    if (g_bDcMode && !dc_room_has_char_variant(fileStage, g_roomId)) {
        variant = 0;
    }

    sprintf(FILE_PATH, GAME_DATA_ROOT "stage%c\\room%c%c%c%c.rdt",
            hexDigits[fileStage + 1],
            hexDigits[fileStage + 1],
            hexDigits[g_roomId >> 4],
            hexDigits[g_roomId & 0xF],
            hexDigits[variant]);

    SetSpriteBufferFlag();

    // A failed load leaves the buffer holding the PREVIOUS room, and everything
    // below relocates pointers through it: counts come out of stale bytes and
    // the omodel walk runs off into unmapped memory, which is the access
    // violation a missing RDT produced rather than a clean error. The original
    // never meets one; an overlay can (§0 of the DC port plan calls a
    // half-populated mode tree out as a hazard), so it is checked here.
    if (LoadFile(FILE_PATH, g_RdtPointer, 1) == (size_t)-1) {
        dbg_printf("[room] %s could not be loaded - keeping the previous room's"
                   " RDT rather than relocating through it\n", FILE_PATH);
        return;
    }

    // 0x00477e2a-0x00477e46: Resolve camera pointers
    // Each camera has 2 relative pointer fields (mask_pointer, tim_mask_pointer)
    // that need to be converted to absolute addresses.
    int cameraCount = g_RdtPointer->cameras_count;
    for (int i = 0; i < cameraCount; i++) {
        *(int*)(cameras) += (int)g_RdtPointer;
        *(int*)(cameras + 4) += (int)g_RdtPointer;
        cameras += 0x2C; // sizeof(RDT_Camera)
    }

    // 0x00477e48-0x00477e75: Resolve RDT pointer fields (offset 0x48 to 0x93)
    // These are relative offsets stored as ints, converted to absolute pointers.
    int* ptrField = (int*)((unsigned char*)g_RdtPointer + 0x48);
    int* ptrEnd = (int*)((unsigned char*)g_RdtPointer + 0x94);
    while (ptrField < ptrEnd) {
        *ptrField += (int)g_RdtPointer;
        ptrField++;
    }

    // 0x00477e7e-0x00477ec0: Resolve omodel (room object) model pointers
    // Iterates forward through entries, zeros table backward
    int* omodelPtr = (int*)g_RdtPointer->object_models;
    int omodelCount = g_RdtPointer->omodel_slot_count;
    for (int i = omodelCount; i > 0; i--) {
        // Zero out table entry (reverse order: table[count-1] down to table[0])
        ((int*)g_omodel_table)[i - 1] = 0;
        if (omodelPtr[0] != 0) omodelPtr[0] += (int)g_RdtPointer;
        if (omodelPtr[1] != 0) omodelPtr[1] += (int)g_RdtPointer;
        omodelPtr += 2;
    }

    // 0x00477ec9-0x00477f0b: Resolve item model pointers
    // Same pattern: forward through entries, backward through table
    int* itemPtr = (int*)g_RdtPointer->item_models;
    int itemCount = g_RdtPointer->item_count;
    for (int i = itemCount; i > 0; i--) {
        ((int*)g_item_model_table)[i - 1] = 0;
        if (itemPtr[0] != 0) itemPtr[0] += (int)g_RdtPointer;
        if (itemPtr[1] != 0) itemPtr[1] += (int)g_RdtPointer;
        itemPtr += 2;
    }

    // 0x00477f12-0x00477f27: Set up SCD script pointers
    g_RoomInitScd = g_RdtPointer->initialization_scd;
    g_RoomScdOpcodes = g_RdtPointer->scd_opcodes;
    g_RoomEventScripts = g_RdtPointer->scd_opcodes2;

    // 0x00477f2d-0x00477f3f: Resolve EVT script relative offsets
    int* evtPtr = (int*)g_RoomEventScripts;
    while (*evtPtr != 0) {
        *evtPtr += (int)g_RoomEventScripts;
        evtPtr++;
    }

    // 0x00477f49-0x00477f64: Set back color from ambient light.
    //
    // ambient_light is a COLOR of three SHORTS (Ghidra: COLOR at RDT+6, size 6)
    // and setBackColor takes 12-bit PS1 channels, scaling by 255/4096. Casting to
    // unsigned char first was an 8x underexposure of the ambient term on every
    // room: the mansion hall's 1775 became 239, so the GTE background colour came
    // out 14 instead of 110 and every character model rendered near-black.
    setBackColor(
        (unsigned short)g_RdtPointer->ambient_light_r,
        (unsigned short)g_RdtPointer->ambient_light_g,
        (unsigned short)g_RdtPointer->ambient_light_b);

    // 0x00477f6e: empty_40ae40(0)
    empty_40ae40(0);
}

// (0x0040ada0) - Set background clear color
void setBackColor(unsigned short r, unsigned short g, unsigned short b) {
    // 0x0040ada0-0x0040ae36: Clamp PS1 12-bit color (0-4095) and convert to 8-bit (0-255)
    if (r > 0xFFF) r = 0x1000;
    g_red_color = (unsigned char)((r * 255) / 4096);

    if (g > 0xFFF) g = 0x1000;
    g_green_color = (unsigned char)((g * 255) / 4096);

    if (b > 0xFFF) b = 0x1000;
    g_blue_color = (unsigned char)((b * 255) / 4096);
}

// (0x0040ae40) - Empty function called by LoadRoomRdt
void empty_40ae40(int param) { }
