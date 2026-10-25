// DebugSaveLoad.cpp - port-added slot save/load for the F1 debug menu
// (DebugMenu.cpp's QUICK ACCESS save/load lists, driven by GameLoop.cpp's
// quick-access load machine).
//
// Not in the original binary. Deliberately self-contained so that
// SaveLoadScreen.cpp stays a faithful port of the original 0x00493310 state
// machine: the save-file layout below is a COPY of what that file does
// (STATE_PERFORM_SAVE's assembly and RestoreSaveBlock, original
// 0x0049392B-0x004939EC / 0x00493D47-0x00493E3C) and has to be mirrored here if
// the format ever changes.
//
// Two deliberate differences from the real paths, both because the quick load
// restarts gameplay in place (GameLoop.cpp chains game_start from inside
// game_loop, so game_loop never returns and game_start's end-of-game resets
// never run):
//
//   1. DebugAdoptLoadedCharacter() sets OR clears MSF_CHAR_VARIANT from the
//      loaded card. The original's load handler (0x00493E83) only ever sets it,
//      which is safe there because game_start zeroes g_main_state_flags when
//      game_loop returns (0x0048072D) - the only way back to the title screen
//      short of the F9 reset. A quick load skips that, so without the clear a
//      Chris card loaded after a Jill session kept the Jill RDT variant
//      ("Jill events with Chris").
//   2. The quick load applies game_start's msf2 mask
//      (`and [0xbe41c4], 0x20080000` at 0x00480745/0x0048075C). RoomInit only
//      ever SETS MSF2_COSTUME_VARIANT (0x00477888) and LoadEntityEMD then
//      forces the alternate-outfit EMD (g_bCostumeVariant + 0x33) for any
//      character, so a stale bit made a regular-outfit save come back in the
//      previous session's costume.
//
// The quick save also writes the full 0xA82 file image rather than just the
// 0x800 bio-card block: RestoreSaveBlock size-gates everything past 0x800, so
// a short file could not restore the pad bindings, room BGM state or costume
// variant.
// ============================================================================
#include "../Globals.h"
#include "../DebugPrint.h"
#include "../system/AssetPath.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// Save file layout - copies of SaveLoadScreen.cpp's constants. The file is
// 0xA82 = 2690 bytes; the first 0x800 are the original's bio-card block, which
// the port models as g_BioCard plus the input-config globals that follow it.
// ============================================================================
#define DBG_SAVE_FILE_SIZE      0xA82
#define DBG_SAVE_BLOCK_SIZE     0x800
#define DBG_OFFSET_PAD_REMAP    0x41C   // 32 bytes  (g_padRemapSubTable3)
#define DBG_OFFSET_CONTROLLER   0x43C   // 1 byte    (g_controllerConfig)
#define DBG_OFFSET_KEY_BINDINGS 0x800   // 32 bytes  (g_keyBindingData)
#define DBG_OFFSET_JOY_REMAP    0x820   // 256 bytes (g_JoyRemapTbl)
#define DBG_OFFSET_JOY_BACKUP   0x8A0   // 128 bytes (g_joyRemapBackupJoy)
#define DBG_OFFSET_ROOM_BGM     0x920   // 224 bytes (g_roomBgmState)
#define DBG_OFFSET_SIDEWINDER   0xA00   // 1 byte    (g_bPadConnected)
#define DBG_OFFSET_COSTUME      0xA01   // 1 byte    (g_bCostumeVariant)
#define DBG_OFFSET_KEY_BACKUP   0xA02   // 128 bytes (g_joyRemapBackupKey)

// Copy of SaveLoadScreen.cpp's JoyRemapTableIsEmpty - see that file for why a
// pre-pad save can carry an all-zero joy table.
static bool DebugJoyRemapTableIsEmpty(const void* table)
{
    const DWORD* p = (const DWORD*)table;
    for (int i = 0; i < 32; i++) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}

// Restore a save file buffer into the bio card / input-config globals.
// Mirror of SaveLoadScreen.cpp's RestoreSaveBlock (same size gates, same
// pre-pad-save guards) so a debug quick load lands in exactly the state a real
// load would.
static void DebugRestoreSaveBlock(const char* fileBuffer, int fileSize)
{
    DWORD liveJoyRemap[32];
    memcpy(liveJoyRemap, g_JoyRemapTbl[1], sizeof(liveJoyRemap));

    memcpy(g_BioCardData, fileBuffer, sizeof(BioCardLayout));
    memcpy(g_padRemapSubTable3, fileBuffer + DBG_OFFSET_PAD_REMAP,
           sizeof(g_padRemapSubTable3));
    g_controllerConfig = fileBuffer[DBG_OFFSET_CONTROLLER];
    if (fileSize > DBG_SAVE_BLOCK_SIZE) {
        memcpy(g_keyBindingData, fileBuffer + DBG_OFFSET_KEY_BINDINGS, 0x20);
        memcpy(g_JoyRemapTbl, fileBuffer + DBG_OFFSET_JOY_REMAP, 0x100);
        InitInputKeyBindings();
        if (fileSize > 0x920) {
            memcpy(g_roomBgmState, fileBuffer + DBG_OFFSET_ROOM_BGM, 0xE0);
            if (fileSize > 0xA00) {
                // file[0xA00] holds the saved sidewinder flag; the original
                // loads it into a dead stack local.
                memcpy(&g_bCostumeVariant, fileBuffer + DBG_OFFSET_COSTUME, 1);
                if (fileSize > 0xA02) {
                    memcpy(g_joyRemapBackupKey,
                           fileBuffer + DBG_OFFSET_KEY_BACKUP, 0x80);
                    memcpy(g_joyRemapBackupJoy,
                           fileBuffer + DBG_OFFSET_JOY_BACKUP, 0x80);
                    memcpy(g_JoyRemapTbl[1],
                           g_bPadConnected
                               ? g_joyRemapBackupJoy : g_joyRemapBackupKey,
                           0x80);
                }
            }
        }

        if (DebugJoyRemapTableIsEmpty(g_JoyRemapTbl[1])) {
            const void* pOther = g_bPadConnected
                               ? (const void*)g_joyRemapBackupKey
                               : (const void*)g_joyRemapBackupJoy;
            if (!DebugJoyRemapTableIsEmpty(pOther)) {
                memcpy(g_JoyRemapTbl[1], pOther, 0x80);
            } else {
                memcpy(g_JoyRemapTbl[1], liveJoyRemap, 0x80);
            }
        }

        InstallPadDefaultBindings();
    }
}

// Assemble the full 0xA82 save file image. Mirror of STATE_PERFORM_SAVE's
// assembly; the live joy remap goes in first and the saved backup overwrites
// its upper half (DBG_OFFSET_JOY_BACKUP == DBG_OFFSET_JOY_REMAP + 0x80).
static void DebugAssembleSaveFile(char* fileBuffer)
{
    memcpy(g_bPadConnected ? g_joyRemapBackupJoy : g_joyRemapBackupKey,
           g_JoyRemapTbl[1], 0x80);

    memset(fileBuffer, 0, DBG_SAVE_FILE_SIZE);
    memcpy(fileBuffer + 0x000, g_BioCardData, sizeof(BioCardLayout));
    memcpy(fileBuffer + DBG_OFFSET_PAD_REMAP, g_padRemapSubTable3,
           sizeof(g_padRemapSubTable3));
    fileBuffer[DBG_OFFSET_CONTROLLER] = g_controllerConfig;
    memcpy(fileBuffer + DBG_OFFSET_KEY_BINDINGS, g_keyBindingData, 0x20);
    memcpy(fileBuffer + DBG_OFFSET_JOY_REMAP, g_JoyRemapTbl, 0x100);
    memcpy(fileBuffer + DBG_OFFSET_ROOM_BGM, g_roomBgmState, 0xE0);
    fileBuffer[DBG_OFFSET_SIDEWINDER] = (char)g_bPadConnected;
    fileBuffer[DBG_OFFSET_COSTUME]    = (char)g_bCostumeVariant;
    memcpy(fileBuffer + DBG_OFFSET_JOY_BACKUP, g_joyRemapBackupJoy, 0x80);
    memcpy(fileBuffer + DBG_OFFSET_KEY_BACKUP, g_joyRemapBackupKey, 0x80);
}

// Adopt the character recorded in the restored card: player entity id plus
// MSF_CHAR_VARIANT, which LoadRoomRdt (RoomInit.cpp 0x00477d90) turns into the
// RDT filename's variant digit (0 = Chris, 1 = Jill). Set OR clear, unlike the
// original's 0x00493E75 which only sets - see the file header.
static void DebugAdoptLoadedCharacter(void)
{
    g_playerEntityPointer.id = g_SelectedCharactedId;
    if ((g_SelectedCharactedId & 3) != 0) {
        g_main_state_flags |= MSF_CHAR_VARIANT;
    } else {
        g_main_state_flags &= ~MSF_CHAR_VARIANT;
    }
}

// Write the current game to savedat<slot+1>.dat. Mirrors STATE_PERFORM_SAVE's
// player snapshot and file image; no ink ribbon is consumed (the debug menu is
// frozen over the game, so nothing else can be running).
void DebugQuick_SaveSlot(int slot)
{
    g_PlayerPosXCopy         = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_PlayerPosZCopy         = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
    g_SelectedCharactedId    = g_playerEntity.id;
    g_PlayerHealthStatusCopy = g_playerEntity.healthStatusFlags;
    g_PlayerDirAngleCopy     = g_playerEntity.directionAngle;

    EnsureDirectoryExists(GetSaveRoot());
    sprintf(g_saveFileName, "%ssavedat%d.dat", GetSaveRoot(), slot + 1);

    char fileBuffer[DBG_SAVE_FILE_SIZE + 8];
    DebugAssembleSaveFile(fileBuffer);
    int written = FileWrite(g_saveFileName, fileBuffer, DBG_SAVE_FILE_SIZE);
    if (g_SavesCounter + 1 < 100) {
        g_SavesCounter = g_SavesCounter + 1;
    }

    dbg_printf("[debugmenu] quick save: slot %d -> '%s' (%s, %d bytes)\n",
               slot + 1, g_saveFileName, (written < 0) ? "FAILED" : "OK", written);
}

// Restore savedat<slot+1>.dat and arm the continue path. The caller
// (GameLoop.cpp's quick-access load machine) then chains game_start, whose
// InitializeGame continue branch rebuilds the saved stage from the card.
// Returns 1 when the slot was restored, 0 when it was refused (missing file, or
// a DC TRAINING/ADVANCED save in OG mode) - the caller must not chain
// game_start on 0.
int DebugQuick_LoadSlot(int slot)
{
    char fileBuffer[DBG_SAVE_FILE_SIZE + 8];
    sprintf(g_saveFileName, "%ssavedat%d.dat", GetSaveRoot(), slot + 1);
    int fileSize = ReadSaveFile(g_saveFileName, fileBuffer);
    if (fileSize < 0x200) {
        dbg_printf("[debugmenu] quick load: slot %d has no valid save\n", slot + 1);
        return 0;
    }
    // Port-only: in OG mode a DC TRAINING/ADVANCED save cannot be loaded (its
    // rules do not exist here). Refuse before touching any live state; the debug
    // list greys these out, this is the belt-and-braces backstop.
    if (!g_bDcMode && (((unsigned char)fileBuffer[0x233] & 3) != DC_DIFFICULTY_STANDARD)) {
        dbg_printf("[debugmenu] quick load: slot %d is a DC mode-%d save; "
                   "refused in OG mode\n",
                   slot + 1, (unsigned char)fileBuffer[0x233] & 3);
        return 0;
    }
    DebugRestoreSaveBlock(fileBuffer, fileSize);
    g_main_state_flags |= MSF_CONTINUE_GAME;   // InitializeGame continue branch
    DebugAdoptLoadedCharacter();
    g_main_state_flags2 &= MSF2_RESET_KEEP_MASK;
    dbg_printf("[debugmenu] quick load: slot %d restored from '%s' (%d bytes), "
               "char %d variant %d\n",
               slot + 1, g_saveFileName, fileSize, g_SelectedCharactedId,
               (g_main_state_flags & MSF_CHAR_VARIANT) ? 1 : 0);
    return 1;
}
