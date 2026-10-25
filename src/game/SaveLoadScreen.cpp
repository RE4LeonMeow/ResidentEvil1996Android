// SaveLoadScreen.cpp - Save/Load game state screen
// Decompiled from Ghidra at 0x00493310
// Dependencies: FileWrite(0x004122a0), ReadSaveFile(0x004120c0),
//               EnsureDirectoryExists(0x0040b700), GetSaveLocationIndex(0x00494000),
//               DrawSaveCursor(0x00493fa0), InitInputKeyBindings(0x00497c20),
//               Flg_ck(0x00473f40), use_room_action_item(0x004631f0),
//               rearrange_item_slots(0x00451510), cut_set(0x004628c0),
//               StMask(0x00497690), Task_sleep(0x004201e0), Task_chain(0x00420210)
#include "../Globals.h"
#include "../platform/platform.h"
#include "FileLoader.h"
#include "SpriteRenderer.h"
#include "SFXIds.h"
#include "PrintText.h"
#include "dc/Items.h"    // lockpick item id (is_lockpick_item)
#include <cstdio>
#include <cstring>
#include "../system/AssetPath.h"

extern void logos_state(void);
extern void title_state(void);
extern void game_start(void);

// ============================================================================
// Save slot info — matches the original's 20-byte stack entries.
// Field order is load order: charId(+0), count(+4), stage(+8), room(+0xc),
// hasData(+0x10). The byte offsets are into the save file (the file's first
// 0x800 bytes are the g_BioCard block, 1:1 with memory).
// ============================================================================
typedef struct SaveSlotInfo
{
    uint32_t characterId;   // +0x00  file[0x22B]  (g_BioCard.selectedCharactedId)
    uint32_t savesCount;    // +0x04  file[0x228]  (g_BioCard.savesCounter)
    uint32_t stageId;       // +0x08  file[0x200]  (g_BioCard.stageId)
    uint32_t roomId;        // +0x0C  file[0x201]  (g_BioCard.roomId)
    uint32_t hasData;       // +0x10  file exists flag
    // Director's Cut only: file[0x233], the mode the save was made in
    // (g_BioCard.dcGameMode). The DC colour-codes each slot row by it.
    uint32_t dcGameMode;
} SaveSlotInfo;

typedef enum {
    STATE_INIT = 0,
    STATE_IDLE = 1,
    STATE_INPUT_DELAY = 2,
    STATE_SAVE_SLOT_SELECTED = 3,   // save menu: slot chosen
    STATE_LOAD_SLOT_SELECTED = 4,   // load menu: slot chosen
    STATE_CONFIRM_OVERWRITE = 5,    // confirm overwrite dialog
    STATE_PERFORM_SAVE = 6,         // write save file
    STATE_SAVE_ANIM_STEP1 = 7,
    STATE_SAVE_ANIM_STEP2 = 8,
    STATE_ERROR_MSG = 9             // "NOT ENOUGH FREE SPACE"
} MenuState;

// Save file constants (the file is 0xA82 = 2690 bytes)
#define SAVE_SLOT_COUNT      8
#define SAVE_FILE_SIZE       0xA82
// The original's 0x800 "block" is one contiguous region (0xbe9620..0xbe9e20)
// holding g_BioCard plus the input-config globals that follow it. The port
// models those as separate globals, so the copies below are sized per global
// instead of one 0x800 memcpy (g_BioCard is only sizeof(BioCardLayout)).
#define SAVE_BLOCK_SIZE      0x800   // original block region size (for gating)
#define OFFSET_STAGE_ID      0x200
#define OFFSET_ROOM_ID       0x201
#define OFFSET_SAVES_COUNT   0x228
#define OFFSET_CHARACTER_ID  0x22B
#define OFFSET_DC_MODE       0x233   // Director's Cut mode byte (g_DcGameMode)
#define OFFSET_PAD_REMAP     0x41C   // 32 bytes  (g_padRemapSubTable3)
#define OFFSET_CONTROLLER_CFG 0x43C  // 1 byte   (g_controllerConfig)
#define OFFSET_KEY_BINDINGS  0x800   // 32 bytes  (g_keyBindingData)
#define OFFSET_JOY_REMAP     0x820   // 256 bytes (g_JoyRemapTbl)
#define OFFSET_JOY_BACKUP    0x8A0   // 128 bytes (g_joyRemapBackupJoy)
#define OFFSET_ROOM_BGM      0x920   // 224 bytes (g_roomBgmState)
#define OFFSET_SIDEWINDER    0xA00   // 1 byte
#define OFFSET_COSTUME_VARIANT 0xA01   // 1 byte  (g_bCostumeVariant)
#define OFFSET_KEY_BACKUP    0xA02   // 128 bytes (g_joyRemapBackupKey)

// ============================================================================
// PrintFormattedText encoded data - byte-identical to the original tables at
// the addresses in the comments. tools/verify_save_screen_tables.py re-encodes
// every string below and diffs it against the executable.
//
// Encoding: 0x00 = a blank cell (advances, draws nothing), 0x01 = end of text,
// 0xFB = a no-op that neither draws nor advances. The original pads a glyph
// with 0xFB to make it occupy a whole two-byte cell, which is what STR_CELL()
// does - see PrintText.h.
//
// The names and locations MUST be cell-encoded: the save reveal copies
// fixed-length slices out of them (10 and 39 bytes) and lengthens the slice two
// bytes a frame, so a cell that is not two bytes wide desynchronises every cell
// after it. The rest of the tables pad only their blanks, or nothing at all -
// the original is not consistent, and the bytes are what they are.
//
// A slot row is 28 cells: name(5) '\' count(2) '\' location(19).
// ============================================================================

// A padded blank cell: 0x00 advances one cell without drawing, STR_PAD adds the
// 0xFB the original writes after it. Ordinary literal, so it also concatenates
// into the u8"" literals of the Japanese tables further down.
#define PAD_SP  " " STR_PAD

// Cell counts of the two tables the save reveal slices. The trailing blanks
// that pad the location names to a fixed width are invisible in the source, so
// the static_asserts below are what actually holds them in place.
static constexpr int kUsaNameCells = 5;
static constexpr int kUsaLocCells  = 19;

// --- Character names (indexed by characterId & 3) ---
// Jill's trailing blank is one of the five cells, not padding: drop it and the
// reveal copies the terminator into the middle of the line.

static constexpr auto s_pftChrisName = STR_CELL("CHRIS");   // DAT_004d40f8
static constexpr auto s_pftJillName  = STR_CELL("JILL ");   // DAT_004d4108
static_assert(s_pftChrisName.len == kUsaNameCells * 2, "USA name must be 5 cells");
static_assert(s_pftJillName.len  == kUsaNameCells * 2, "USA name must be 5 cells");

// Pointer table at PTR_DAT_004d4118
static const unsigned char* s_pftCharNameTable[] = {
    s_pftChrisName, s_pftJillName
};

// --- Location names (indexed by GetSaveLocationIndex) ---
// 19 cells each, right-padded with blanks to that fixed width. The two leading
// blanks are why the empty-slot template's location half starts with two.

static constexpr auto s_pftLocRoom1F     = STR_CELL("  M.Room 1F        ");   // DAT_004d41a0
static constexpr auto s_pftLocHall1F     = STR_CELL("  M.Hall 1F        ");   // DAT_004d41c8
static constexpr auto s_pftLocCourtyard  = STR_CELL("  Courtyard Room B1");   // DAT_004d41f0
static constexpr auto s_pftLocGuardhouse = STR_CELL("  Guardhouse 1F    ");   // DAT_004d4218
static constexpr auto s_pftLocLaboratory = STR_CELL("  Laboratory B3    ");   // DAT_004d4240
static constexpr auto s_pftLocStoreroom  = STR_CELL("  M.Storeroom 1F   ");
static constexpr auto s_pftLocCourtyard2 = STR_CELL("  Courtyard Path B1");   // DAT_004d4290
static_assert(s_pftLocRoom1F.len     == kUsaLocCells * 2, "USA location must be 19 cells");
static_assert(s_pftLocHall1F.len     == kUsaLocCells * 2, "USA location must be 19 cells");
static_assert(s_pftLocCourtyard.len  == kUsaLocCells * 2, "USA location must be 19 cells");
static_assert(s_pftLocGuardhouse.len == kUsaLocCells * 2, "USA location must be 19 cells");
static_assert(s_pftLocLaboratory.len == kUsaLocCells * 2, "USA location must be 19 cells");
static_assert(s_pftLocStoreroom.len  == kUsaLocCells * 2, "USA location must be 19 cells");
static_assert(s_pftLocCourtyard2.len == kUsaLocCells * 2, "USA location must be 19 cells");

// Pointer table at 0x004d42b8
static const unsigned char* s_pftLocNameTable[] = {
    s_pftLocRoom1F, s_pftLocHall1F, s_pftLocCourtyard,
    s_pftLocGuardhouse, s_pftLocLaboratory, s_pftLocStoreroom,
    s_pftLocCourtyard2
};

// --- Slot templates ---
// Filled (DAT_004d4058): 9 cells - separators at 5 and 8, the rest blanks the
// name and count overlay. Only the blanks carry the 0xFB pad here.
static constexpr auto s_pftFilledSlot = STR(    // "     \  \"
    PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP "\\"
    PAD_SP PAD_SP "\\");
// Empty (DAT_004d4070): the whole 28-cell row, unpadded. Cells 9-27 are the two
// blanks and 17 dashes that stand in for a 19-cell location name - the count
// has to match, or the dashed row overhangs the filled ones.
static constexpr auto s_pftEmptySlot = STR("-----\\--\\  -----------------");

// --- Mode title strings (drawn as two overlapping parts per original) ---
// Bottom: "DO NOT" over "       SAVE"/"       LOAD" -> "DO NOT SAVE"/"DO NOT LOAD"
// Header: "SAVE"/"LOAD" under "     GAME" -> "SAVE GAME"/"LOAD GAME"

static constexpr auto s_pftDoNot    = STR("DO NOT");   // DAT_004d40b8
static constexpr auto s_pftExitSave =                  // DAT_004d40d8
    STR(PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP "SAVE");
static constexpr auto s_pftExitLoad =                  // DAT_004d40c0
    STR(PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP "LOAD");
// Pointer table at 0x004d40f0
static const unsigned char* s_pftExitTable[] = {
    s_pftExitSave, s_pftExitLoad
};
static constexpr auto s_pftSave = STR("SAVE");   // DAT_004d40a8
static constexpr auto s_pftLoad = STR("LOAD");   // DAT_004d40a0
// Pointer table at 0x004d40b0
static const unsigned char* s_pftHeaderTable[] = {
    s_pftSave, s_pftLoad
};
static constexpr auto s_pftGame = STR("     GAME");   // DAT_004d4090

// --- Confirmation dialog (state 5) ---

static constexpr auto s_pftOverwritePrompt =    // DAT_004d4170
    STR("OK TO OVERWRITE THE DATA?");
// DAT_004d4190. The cursor sits on the blank before each answer, five cells
// apart, which is what L.confirmStride encodes.
static constexpr auto s_pftYesNo = STR(PAD_SP "YES" PAD_SP PAD_SP "NO ");

// --- Error messages (state 9) ---
// Both lines are 34 cells: the original right-pads the first with blanks and
// indents the second by 20 of them.

static constexpr auto s_pftNoFreeSpace =        // DAT_004d4120
    STR("NOT ENOUGH FREE SPACE             ");
static constexpr auto s_pftOnHardDrive =        // DAT_004d4148
    STR("                    ON HARD DRIVE.");

// ============================================================================
// Japanese (Biohazard.exe) screen data - FUN_00435af0, the JPN counterpart of
// LoadSaveGameState.
//
// The Japanese release runs the SAME state machine over its own strings, so
// only the data and the column positions differ. Two things drive every one of
// those differences:
//
//   * FONT.TIM's game-text glyph is 14px wide, not 8, so every X in the screen
//     is a different multiple of the glyph cell (see PrintFormattedText in
//     PrintText.cpp, which already switches its advance on GetAssetVersion).
//   * A Japanese slot line is 15 glyph cells instead of the USA's 28, laid out
//     name(3) '/' count(2) '/' location(8) rather than name(5) '\\' count(2)
//     '\\' location(19).
//
// Every string below encodes byte-for-byte to the table at the address in its
// comment; tools/verify_jpn_save_screen.py re-checks that against the
// executable. Note that glyph 0x38 is a backslash in fontus.tim and a forward
// slash in FONT.TIM, so the separator byte is the same in both releases and
// only looks different.
//
// Three spellings appear here, and which one a string needs is not cosmetic:
//
//   STR_JP_CELL  the character-name and location tables, because the save
//                reveal copies FIXED-LENGTH slices out of them (6 and 17
//                bytes) and lengthens the slice two bytes a frame. The encoder
//                pads every glyph to a two-byte cell so a slice can never end
//                inside a kanji; the static_asserts below pin the widths.
//   PAD_SP        a blank cell in a table the original padded (the filled-slot
//                template and the yes/no line pad their spaces but not their
//                other glyphs, so neither encoder reproduces them on its own).
//   STR_JP       everything else - the original leaves those unpadded.
// ============================================================================

// Cell counts of the two sliced tables. The reveal reads name bytes
// [0, nameLen) and location bytes [locBase, locBase + locLen), and the
// location slice includes the 0x01 terminator - hence the odd length.
static constexpr int kJpnNameCells = 3;
static constexpr int kJpnLocCells  = 8;

// --- Character names (indexed by characterId & 3) ---
// Three cells each. Jill's trailing space is one of them, not padding: drop it
// and the reveal copies the terminator into the middle of the line.

static constexpr auto s_jpnChrisName = STR_JP_CELL(u8"クリス");   // 0x004b1010
static constexpr auto s_jpnJillName  = STR_JP_CELL(u8"ジル ");    // 0x004b1018
static_assert(s_jpnChrisName.len == kJpnNameCells * 2, "JPN name must be 3 cells");
static_assert(s_jpnJillName.len  == kJpnNameCells * 2, "JPN name must be 3 cells");

// Pointer table at 0x004b1020
static const unsigned char* s_jpnCharNameTable[] = {
    s_jpnChrisName, s_jpnJillName
};

// --- Location names (indexed by GetSaveLocationIndex) ---
// Eight cells each. The '/' in cell 5 of the text lands on the third separator
// of the filled-slot template, exactly as the USA strings' backslash does.

static constexpr auto s_jpnLocRoom1F     = STR_JP_CELL(u8"館 小部屋/1F");    // 0x004b1068
static constexpr auto s_jpnLocHall1F     = STR_JP_CELL(u8"館 ホール/1F");    // 0x004b1080
static constexpr auto s_jpnLocCourtyard  = STR_JP_CELL(u8"中庭 部屋/B1");    // 0x004b1098
static constexpr auto s_jpnLocGuardhouse = STR_JP_CELL(u8" 寄宿舎 /1F");    // 0x004b10b0
static constexpr auto s_jpnLocLaboratory = STR_JP_CELL(u8" 研究所 /B3");    // 0x004b10c8
static constexpr auto s_jpnLocStoreroom  = STR_JP_CELL(u8"館  物置/1F");    // 0x004b10e0
static constexpr auto s_jpnLocCourtyard2 = STR_JP_CELL(u8"中庭 通路/B1");    // 0x004b10f8
static_assert(s_jpnLocRoom1F.len     == kJpnLocCells * 2, "JPN location must be 8 cells");
static_assert(s_jpnLocHall1F.len     == kJpnLocCells * 2, "JPN location must be 8 cells");
static_assert(s_jpnLocCourtyard.len  == kJpnLocCells * 2, "JPN location must be 8 cells");
static_assert(s_jpnLocGuardhouse.len == kJpnLocCells * 2, "JPN location must be 8 cells");
static_assert(s_jpnLocLaboratory.len == kJpnLocCells * 2, "JPN location must be 8 cells");
static_assert(s_jpnLocStoreroom.len  == kJpnLocCells * 2, "JPN location must be 8 cells");
static_assert(s_jpnLocCourtyard2.len == kJpnLocCells * 2, "JPN location must be 8 cells");

// Pointer table at 0x004b1110
static const unsigned char* s_jpnLocNameTable[] = {
    s_jpnLocRoom1F, s_jpnLocHall1F, s_jpnLocCourtyard,
    s_jpnLocGuardhouse, s_jpnLocLaboratory, s_jpnLocStoreroom,
    s_jpnLocCourtyard2
};

// --- Slot templates (15 cells) ---
// Filled (0x004b0fa0): separators at cells 3, 6 and 12, everything else a
// blank the name / count / location overlays land on. Only the blanks carry
// the 0xFB pad in the original, so they are spelled with PAD_SP and the
// separators plainly.
static constexpr auto s_jpnFilledSlot = STR_JP(     // "   /  /     /  "
    PAD_SP PAD_SP PAD_SP u8"/"
    PAD_SP PAD_SP u8"/"
    PAD_SP PAD_SP PAD_SP PAD_SP PAD_SP u8"/"
    PAD_SP PAD_SP);
// Empty (0x004b0fc0): the dash is the katakana long-vowel mark, which is what
// index 0x3B is in FONT.TIM (it is '-' in fontus.tim).
static constexpr auto s_jpnEmptySlot = STR_JP(u8"ーーー/ーー/ーーーーー/ーー");   // 0x004b0fc0

// --- Exit line ---
// Drawn as two overlapping parts at the same position, like the USA's
// "DO NOT" + "       SAVE": the verb takes cells 0-2 and the suffix's three
// leading blanks skip over it.
static constexpr auto s_jpnExitSave = STR_JP(u8"セーブ");   // 0x004b1004
static constexpr auto s_jpnExitLoad = STR_JP(u8"ロード");   // 0x004b1000
// Pointer table at 0x004b1008
static const unsigned char* s_jpnExitTable[] = {
    s_jpnExitSave, s_jpnExitLoad
};
static constexpr auto s_jpnExitSuffix = STR_JP(u8"   しない");   // 0x004b0ff8

// The header is Latin in both releases and the two fonts share their latin
// rows, so 0x004b0fe8/0x004b0fe0/0x004b0fd0 are byte-identical to s_pftSave/
// s_pftLoad/s_pftGame - only the X moves (the 9-cell "SAVE GAME" is centred
// at 124 for an 8px font and at 97 for a 14px one).

// --- Confirmation dialog (state 5) ---
static constexpr auto s_jpnOverwritePrompt =        // 0x004b1130
    STR_JP(u8"データを上書きしてよろしいですか");
// 0x004b1148. The cursor sits on the blank before each answer, four cells
// apart, which is what L.confirmStride encodes.
static constexpr auto s_jpnYesNo = STR_JP(PAD_SP u8"はい" PAD_SP PAD_SP u8"いいえ");

// --- Error messages (state 9) ---
// The Japanese message fits on one line, so the second line is 34 blanks and a
// terminator - it draws nothing. Kept because the original still issues the
// call, and because dropping it would make the two versions structurally
// different for no gain.
static constexpr auto s_jpnNoFreeSpace =            // 0x004b1028
    STR_JP(u8"ハードディスクの空きがたりません");
static constexpr auto s_jpnErrLine2 =               // 0x004b1040, 34 blanks
    STR_JP(u8"                                  ");

// ============================================================================
// Per-version screen layout.
//
// Everything that differs between LoadSaveGameState (USA, 0x00493310) and its
// Japanese counterpart (0x00435af0) is a string, a column, or a length of the
// save-reveal string. Collecting them here keeps ONE state machine: the two
// versions differ in data, not in control flow, and a behavioural fix made to
// the screen cannot land on only one of them.
// ============================================================================
typedef struct SaveScreenLayout
{
    const unsigned char* const* charNames;    // [characterId & 3]
    const unsigned char* const* locNames;     // [GetSaveLocationIndex(...)]
    const unsigned char* const* exitNames;    // [mode]
    const unsigned char* const* headerNames;  // [mode]
    const unsigned char* filledSlot;
    const unsigned char* emptySlot;
    const unsigned char* exitSuffix;          // overlays the exit verb
    const unsigned char* headerSuffix;        // "     GAME"
    const unsigned char* overwritePrompt;
    const unsigned char* yesNo;
    const unsigned char* errLine1;
    const unsigned char* errLine2;
    short countX;         // PrintText8x14 X for the two save-count digits
    short locX;           // location-name X
    short headerX;        // "SAVE GAME" / "LOAD GAME" X
    short cursorX;        // slot cursor X (one glyph cell left of the text)
    short confirmStride;  // YES -> NO cursor step, five glyph cells
    int   nameLen;        // bytes of character name the reveal string holds
    int   locLen;         // bytes of location name it holds
} SaveScreenLayout;

// Reveal-string layout, shared by both versions:
//   [0 .. nameLen)            character name
//   [nameLen]                 separator, [+1] 0xFB
//   [nameLen+2], [nameLen+4]  save-count tens / units digit, each + 0xFB
//   [nameLen+6]               separator, [+1] 0xFB
//   [nameLen+8 ...]           location name (locLen bytes, terminator included)
#define SAVE_REVEAL_LOC_BASE(L)  ((L).nameLen + 8)
#define SAVE_REVEAL_LEN(L)       (SAVE_REVEAL_LOC_BASE(L) + (L).locLen)

static SaveScreenLayout GetSaveScreenLayout(void)
{
    SaveScreenLayout L;

    if (GetAssetVersion() != 0) {
        L.charNames       = s_jpnCharNameTable;
        L.locNames        = s_jpnLocNameTable;
        L.exitNames       = s_jpnExitTable;
        L.filledSlot      = s_jpnFilledSlot;
        L.emptySlot       = s_jpnEmptySlot;
        L.exitSuffix      = s_jpnExitSuffix;
        L.overwritePrompt = s_jpnOverwritePrompt;
        L.yesNo           = s_jpnYesNo;
        L.errLine1        = s_jpnNoFreeSpace;
        L.errLine2        = s_jpnErrLine2;
        L.countX          = 0x6f;   // cell 4 of a 14px grid starting at 55
        L.locX            = 0x99;   // cell 7
        L.headerX         = 0x61;
        L.cursorX         = 0x29;   // 55 - 14
        L.confirmStride   = 56;     // 4 cells: " はい" -> "  いいえ"
        L.nameLen         = kJpnNameCells * 2;
        L.locLen          = kJpnLocCells * 2 + 1;   // the slice takes the 0x01 too
    } else {
        L.charNames       = s_pftCharNameTable;
        L.locNames        = s_pftLocNameTable;
        L.exitNames       = s_pftExitTable;
        L.filledSlot      = s_pftFilledSlot;
        L.emptySlot       = s_pftEmptySlot;
        L.exitSuffix      = s_pftDoNot;
        L.overwritePrompt = s_pftOverwritePrompt;
        L.yesNo           = s_pftYesNo;
        L.errLine1        = s_pftNoFreeSpace;
        L.errLine2        = s_pftOnHardDrive;
        L.countX          = 103;    // cell 6 of an 8px grid starting at 55
        L.locX            = 127;    // cell 9
        L.headerX         = 124;
        L.cursorX         = 47;     // 55 - 8
        L.confirmStride   = 40;     // 5 cells: " YES" -> " NO"
        L.nameLen         = kUsaNameCells * 2;
        L.locLen          = kUsaLocCells * 2 + 1;   // the slice takes the 0x01 too
    }

    // Identical in both releases: the two fonts share their latin rows, so the
    // USA-encoded "SAVE"/"LOAD"/"     GAME" ARE the bytes at 0x004b0fe8 /
    // 0x004b0fe0 / 0x004b0fd0.
    L.headerNames  = s_pftHeaderTable;
    L.headerSuffix = s_pftGame;

    return L;
}

// ============================================================================
// FileWrite (0x004122a0)
// Write a buffer to a file. Returns bytes written or -1 on failure.
// ============================================================================
int FileWrite(const char* name, void* buf, int len)
{
    FILE* fp = fopen(name, "wb");
    if (fp == NULL) return -1;
    size_t written = fwrite(buf, 1, len, fp);
    fclose(fp);
    if ((int)written != len) return -1;
    return len;
}

// ============================================================================
// ReadSaveFile (wraps FUN_004120c0)
// Read a save file into a buffer. Returns file size or -1 on failure.
// Original: 0x004120c0 — reads file with install path fallback and async support.
// ============================================================================
int ReadSaveFile(const char* path, void* buffer)
{
    // Try the direct path first
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        // Registry-install fallback (only meaningful with a real install entry).
        // Without it a missing file must return -1 here: falling through to the
        // fseek below dereferences a NULL FILE* and segfaults, which is what an
        // absent save slot did on Linux (no install path, so the fallback never
        // applied). Callers treat a negative size as "slot empty".
        if (g_szInstallPath[0] == '\0') return -1;
        char fullPath[260];
        sprintf(fullPath, "%s%s", g_szInstallPath, path);
        fp = fopen(fullPath, "rb");
        if (fp == NULL) return -1;
    }

    fseek(fp, 0, SEEK_END);
    int fileSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    size_t bytesRead = fread(buffer, 1, fileSize, fp);
    fclose(fp);

    if ((int)bytesRead != fileSize) return -1;
    return fileSize;
}

// ============================================================================
// EnsureDirectoryExists (0x0040b700)
// Ensures the directory portion of a file path exists.
// ============================================================================
void EnsureDirectoryExists(const char* path)
{
    char dirPath[260];
    sprintf(dirPath, "%s", path);
    // Accept either separator: the Windows build only ever sees '\\', but the
    // path layer normalises to '/' on other hosts (docs/LINUX_PORT.md Phase 0).
    char* lastSlash = strrchr(dirPath, '\\');
    char* lastFwd = strrchr(dirPath, '/');
    if (lastFwd != NULL && (lastSlash == NULL || lastFwd > lastSlash)) {
        lastSlash = lastFwd;
    }
    if (lastSlash != NULL) {
        *lastSlash = '\0';
        plat_mkdir(dirPath);
    }
}

// ============================================================================
// DcSaveRowColour (port-only)
// The Director's Cut colour-codes each save slot by the mode it was made in
// (PS1 0x800191b4 passes the slot record's +0xA33 byte - the port's
// g_BioCard.dcGameMode - as the colour argument of the row's text print).
//
// The two builds pack that colour differently. The PS1 font CLUT is 17 palettes
// wide by 3 rows at VRAM (0x100, 0x1E0), and the print maps the byte to
// `clutX = 0x100 + (colour & 3) * 0x10, clutY = 0x1E0 + (colour >> 2)`, so the
// mode picks a palette COLUMN: 0 = the ordinary cream, 1 = green, 2 = red,
// 3 = a dimmer grey. The port's PrintFormattedText keeps clutX at 0x100 (this
// build's TextureDesc.clutX is write-only) and selects on the low nibble as a
// palette ROW instead. So DC mode ships those same four palettes as four CLUT
// rows in its own `Data/fontus.tim` (tools/port_dc_assets.py, step `font`; the
// PC font's 16 colours are byte-identical to the PS1 CLUT's column 0, so
// row 0 is the file the port already had) and the mode byte is the row index.
//
// ADVANCED* is drawn at the ADVANCED colour plus the extra pass the caller
// makes at 0x11 - brightness 1 in the TRAINING row.
// ============================================================================
static unsigned char DcSaveRowColour(unsigned int dcGameMode)
{
    if (!g_bDcMode) {
        return 0;                       // the USA build's only colour
    }
    unsigned char mode = (unsigned char)(dcGameMode & 3);
    if (mode == DC_DIFFICULTY_ADVANCED_HOLD) {
        mode = DC_DIFFICULTY_ADVANCED;
    }
    return mode;
}

// ============================================================================
// SaveSlotLoadBlocked (port-only)
// In OG mode a slot written by a Director's Cut TRAINING / ADVANCED / ADVANCED*
// game cannot be loaded: those modes bring rules the OG game does not have
// (double pick-ups, the arranged damage/health tables). The row is drawn grey
// and the load is refused, but the slot can still be chosen in the SAVE menu
// and overwritten - only the load side is gated. An OG save and a DC STANDARD
// save both carry mode 0, so they keep loading. DC mode loads everything.
// ============================================================================
static int SaveSlotLoadBlocked(const SaveSlotInfo* slot)
{
    if (g_bDcMode || !slot->hasData) {
        return 0;
    }
    return (slot->dcGameMode & 3) != DC_DIFFICULTY_STANDARD;
}

// ============================================================================
// GetSaveLocationIndex (0x00494000)
// Maps stageId/roomId to a location name table index (0-6).
// ============================================================================
int GetSaveLocationIndex(int stageId, int roomId)
{
    int locIdx = stageId % 5; // get zero-indexed absolute stage id
    if (locIdx == STAGE_MANSION_1F) {
        if (roomId == ROOM_MAIN_HALL) locIdx = 1;
        if (locIdx == STAGE_MANSION_1F && roomId == ROOM_MANSION_STOREROOM) locIdx = 5;
    }
    if (locIdx == STAGE_COURTYARD && roomId == ROOM_UNDERGROUND_ENTRY) locIdx = 6;
    return locIdx;
}

// ============================================================================
// DrawSaveCursor (0x00493fa0)
// Draws a blinking cursor/highlight sprite at the given position.
// mode==0 draws the cursor, mode!=0 draws nothing.
// Uses g_TextureDesc — only overwrites geometry/UV fields, inherits
// flags/colorMul/pivot from the previous PrintFormattedText call.
// ============================================================================
void DrawSaveCursor(short x, short y, int mode)
{
    if (mode == 0) {
        // The cursor glyph is character-table index 2 (the arrowhead) on row 2
        // of the sheet, so its U is 2 glyph widths in and its V is a fixed 28.
        // That is 2*8 for fontus.tim and 2*14 for FONT.TIM - the Japanese
        // DrawSaveCursor (0x004366e0) writes width/height 14 and texU/texV
        // 0x1C/0x1C, which is the same cell one font over.
        const int glyphW = (GetAssetVersion() != 0) ? 14 : 8;
        g_TextureDesc.width = (unsigned short)glyphW;
        g_TextureDesc.height = 14;
        g_TextureDesc.texturePage = 0x1E;
        g_TextureDesc.texU = (unsigned char)(2 * glyphW);
        g_TextureDesc.texV = 28;
        g_TextureDesc.clutX = 0x100;
        g_TextureDesc.clutY = 0x1E0;
        g_TextureDesc.screenX = x - g_ScreenOffsetX;
        g_TextureDesc.screenY = y - g_ScreenOffsetY;
        AddTintSprite(&g_TextureDesc, 2);
    }
}

// ============================================================================
// Flg_ck (0x00473f40)
// Checks a bit flag in a flag array.
// baseAddr: base address of the flag array
// bitIndex: bit position to check
// Returns non-zero if the bit is set, 0 otherwise.
// ============================================================================
unsigned int Flg_ck(int baseAddr, unsigned int bitIndex)
{
    // Original: *(uint*)(((bitIndex & 0xffffffe7) >> 3) + baseAddr) & (0x80000000 >> (bitIndex & 0x1f))
    unsigned int wordIndex = (bitIndex & 0xFFFFFFE7) >> 3;
    unsigned int bitMask = 0x80000000 >> (bitIndex & 0x1F);
    unsigned int* flagWord = (unsigned int*)((unsigned char*)baseAddr + wordIndex);
    return *flagWord & bitMask;
}

// ============================================================================
// use_room_action_item (0x004631f0)
// Uses the currently selected item (e.g., ink ribbon, key, weapon).
// Consumes the item from inventory and handles related game state.
// ============================================================================
// NOTE: the original reads/writes g_ItemSlotsPointer (0x00d22768) directly and
// has no NULL guard. The port previously used g_firstItemSlotPointer (a .gwipe
// save-overlay global that is never assigned) and bailed when it was NULL -
// which silently made door-key consumption a no-op. Fixed to the original's
// pointer; the guard was the dead-pointer crutch, not part of the original.
void use_room_action_item(void)
{
    unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;

    g_usedItemId = g_selectedItemId;

    // Lockpick doesn't consume
    if (is_lockpick_item(g_selectedItemId)) return;

    // Find the item in inventory
    unsigned char index = 0;
    unsigned char itemId = slots[0];
    while (itemId != g_selectedItemId) {
        index++;
        itemId = slots[index * 2];
    }

    // If item is a weapon (ID < 0x0B): unequip and remove
    if (g_selectedItemId < ITEM_CLIP) {
        slots[index * 2] = 0;
        if ((unsigned int)g_EquippedItemId - (unsigned int)index == 1) {
            g_EquippedItemId = 0;
        }
        rearrange_item_slots();
        return;
    }

    // For consumable items: decrement quantity
    unsigned char quantity = slots[index * 2 + 1];
    if (quantity != 0) {
        slots[index * 2 + 1] = quantity - 1;
        if (slots[index * 2 + 1] == 0) {
            // Item depleted
            if (g_selectedItemId > ITEM_OIL && g_selectedItemId < ITEM_DESK_KEY) { // is door key
                // Key item depleted — display drop message
                g_main_state_flags |= MSF_MENU_MODE_KEY_DEPLETED;
                return;
            }
            slots[index * 2] = 0;
            rearrange_item_slots();
        }
    }
}

// ============================================================================
// rearrange_item_slots (0x00451510)
// Compacts the item inventory by removing empty gaps left by consumed items.
// The original reads/writes g_ItemSlotsPointer, g_TotalHeldItems,
// g_ItemSlotsBitmask and g_ItemSlotIndices directly (no NULL guard). An
// earlier port used g_firstItemSlotPointer / g_totalHeldItems /
// g_heItemsX2Less1 / g_itemSlotIndices - .gwipe overlay globals that are
// never assigned - which made this function a silent no-op and left the
// slot->sheet-row mapping (g_ItemSlotIndices) stale after consumption.
// ============================================================================
void rearrange_item_slots(void)
{
    unsigned char equippedSlotIdx = g_EquippedItemId - 1;
    unsigned char readIdx = 0;
    unsigned char writeIdx = 0;
    // Chris (0) has 6 slots, Jill (1) has 8 slots
    int maxSlots = (4 - (((g_playerEntity.id & 3) != 1) ? 1 : 0)) * 2;
    int remaining = maxSlots;

    do {
        unsigned char itemId = ((unsigned char*)g_ItemSlotsPointer)[readIdx * 2];
        if (itemId == 0) {
            // Empty slot — skip, clear held-items bit
            if (readIdx < g_TotalHeldItems) {
                g_ItemSlotsBitmask &= ~(1u << (g_ItemSlotIndices[readIdx] & 0x1F));
            }
        } else {
            // Has item — compact
            if (writeIdx != readIdx) {
                ((unsigned char*)g_ItemSlotsPointer)[writeIdx * 2] = itemId;
                ((unsigned char*)g_ItemSlotsPointer)[writeIdx * 2 + 1] =
                    ((unsigned char*)g_ItemSlotsPointer)[readIdx * 2 + 1];
                g_ItemSlotIndices[writeIdx] = g_ItemSlotIndices[readIdx];
                if (equippedSlotIdx == readIdx) {
                    equippedSlotIdx = writeIdx;
                }
            }
            writeIdx++;
        }
        readIdx++;
        remaining--;
    } while (remaining != 0);

    g_EquippedItemId = equippedSlotIdx + 1;
    g_TotalHeldItems = writeIdx;

    // Zero remaining empty slots
    for (int i = maxSlots - writeIdx; i > 0; i--) {
        ((unsigned char*)g_ItemSlotsPointer)[writeIdx * 2] = 0;
        ((unsigned char*)g_ItemSlotsPointer)[writeIdx * 2 + 1] = 0;
        writeIdx++;
    }
}

// True when a joystick remap table carries no bindings at all. Save files
// written before the port had working pad support store an all-zero table:
// g_joyRemapBackupJoy was never refreshed (the sidewinder flag was hard-wired
// false, so only the KEY backup was ever written), and the save assembly
// writes that empty joy backup over the g_JoyRemapTbl[1] region of the file
// (OFFSET_JOY_BACKUP == OFFSET_JOY_REMAP + 0x80). Restoring such a table
// blanks every pad binding, and because it is a global the pad stays dead for
// the rest of the session - including back on the title screen.
static bool JoyRemapTableIsEmpty(const void* table)
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
// Used by STATE_LOAD_SLOT_SELECTED. The block
// restore always runs; the extra areas past 0x800 are size-gated so older
// (smaller) save files still load. The original restores the block with one
// 0x800 memcpy; the port models that region as g_BioCard + the input-config
// globals, so each part is copied into its own global (the unmodeled
// tail 0x43D..0x800 is discarded).
static void RestoreSaveBlock(const char* fileBuffer, int fileSize)
{
    // Keep the live pad bindings; they are the last-resort fallback if the
    // save turns out to carry nothing usable.
    DWORD liveJoyRemap[32];
    memcpy(liveJoyRemap, g_JoyRemapTbl[1], sizeof(liveJoyRemap));

    memcpy(g_BioCardData, fileBuffer, sizeof(BioCardLayout));
    memcpy(g_padRemapSubTable3, fileBuffer + OFFSET_PAD_REMAP,
           sizeof(g_padRemapSubTable3));
    g_controllerConfig = fileBuffer[OFFSET_CONTROLLER_CFG];
    if (fileSize > SAVE_BLOCK_SIZE) {
        memcpy(g_keyBindingData, fileBuffer + OFFSET_KEY_BINDINGS, 0x20);
        memcpy(g_JoyRemapTbl, fileBuffer + OFFSET_JOY_REMAP, 0x100);
        InitInputKeyBindings();
        if (fileSize > 0x920) {
            memcpy(g_roomBgmState, fileBuffer + OFFSET_ROOM_BGM, 0xE0);
            if (fileSize > 0xA00) {
                // file[0xA00] holds the saved sidewinder flag; the
                // original loads it into a dead stack local.
                memcpy(&g_bCostumeVariant, fileBuffer + OFFSET_COSTUME_VARIANT, 1);
                if (fileSize > 0xA02) {
                    memcpy(g_joyRemapBackupKey, fileBuffer + OFFSET_KEY_BACKUP, 0x80);
                    memcpy(g_joyRemapBackupJoy, fileBuffer + OFFSET_JOY_BACKUP, 0x80);
                    memcpy(g_JoyRemapTbl[1],
                           g_bPadConnected
                               ? g_joyRemapBackupJoy : g_joyRemapBackupKey,
                           0x80);
                }
            }
        }

        // Compatibility guard for saves written before pad support: if the
        // restore left the pad with no bindings at all, take the other backup,
        // and failing that keep what was live before the load. Without this a
        // pre-pad save silently disables the controller everywhere.
        if (JoyRemapTableIsEmpty(g_JoyRemapTbl[1])) {
            const void* pOther = g_bPadConnected
                               ? (const void*)g_joyRemapBackupKey
                               : (const void*)g_joyRemapBackupJoy;
            if (!JoyRemapTableIsEmpty(pOther)) {
                memcpy(g_JoyRemapTbl[1], pOther, 0x80);
            } else {
                memcpy(g_JoyRemapTbl[1], liveJoyRemap, 0x80);
            }
        }

        // Saves predating pad support carry the original layout, which has no
        // OPTIONS binding - swap in the port defaults. A configured table is
        // recognised and left as it was saved.
        InstallPadDefaultBindings();
    }
}

// ============================================================================
// LoadSaveGameState (0x00493310)
//
// Main save/load screen state machine.
// Parameters:
//   mode          — 0 = save mode, 1 = load mode. Controls the exit behavior
//                   (0 returns in-game, anything else chains to title_state)
//                   and the slot-selection state (state = mode + 3).
//   flags         — unused by the screen itself (the original never reads it).
//   useInkRibbon  — non-zero to consume ink ribbon when saving. Read only by
//                   state 6; it is NOT a play_sfx bank.
//   sfxBank       — play_sfx bank for every sound the screen makes: cursor
//                   move, confirm, cancel and the save reveal ticks. Every
//                   one of the six calls loads the bank byte from THIS
//                   argument (arg4 at [ESP+0x17f4], e.g. 0x004934af,
//                   0x0049353a, 0x00493b76). 2 from the typewriter -> the
//                   room's g_emSndBanks records 29/30/31 (cancel/type01/
//                   type02, see g_RoomSndData); 1 from the title/ending
//                   screens -> g_SfxBanks 29/30/31, which play_sfx folds to
//                   13/14/15 of the loaded bank (g_st12/g_st14 hold
//                   cancel/type01/type02 there).
//   cutsceneReset — controls cut_set/StMask on exit (0 = do reset).
//
// Called from:
//   title_state:        LoadSaveGameState(1, 0x80180000, 0, 1, 0)  — Load Game
//   check_typewriter:   LoadSaveGameState(0, flags, ribbon+1, 2, 0) — Save Game
// ============================================================================
void LoadSaveGameState(int mode, int flags, int useInkRibbon, int sfxBank, int cutsceneReset)
{
    // Save slot info table (9 entries: 8 file slots + 1 exit option)
    SaveSlotInfo save_slots[SAVE_SLOT_COUNT + 1];
    MenuState state = STATE_INIT;
    int selected_slot = 0;              // current cursor position (0..7 = slots, 8 = exit)
    int confirm_choice = 0;             // 0 = YES, 1 = NO (confirmation dialog)
    int blink_timer = 5;
    int blink_state = 0;                // 0 = cursor visible, 1 = invisible
    int input_delay = 0;
    int anim_counter = 0;               // reveal length counter
    int anim_timer = 5;                 // reveal pause between steps

    char saveBuffer[SAVE_FILE_SIZE + 8];    // slot-scan / load buffer
    char fileBuffer[SAVE_FILE_SIZE + 8];    // save-assembly buffer
    char displayStr[64];                    // save-animation reveal string (57 bytes USA / 31 JPN)
    char animBuf[64];                       // reveal buffer (terminated at anim_counter)

    // Strings and columns for the running asset version. Everything below is
    // shared; only this table differs between the USA screen (0x00493310) and
    // the Japanese one (0x00435af0).
    const SaveScreenLayout L = GetSaveScreenLayout();

    // Set save/load active flag
    g_loadSaveStateFlag = 1;

    LoadFile(GAME_DATA_ROOT "data\\type00.tim", g_TimImageBuffer, 0x20);
    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
    display_image(8, g_TimImageBuffer__bitmap, 320, 240);
    title_setup_texture_pages(8, 1);
    // empty_00470960(8): empty in the original - call dropped

    // Main loop
    do {
        switch (state) {

        // ================================================================
        // State 0: Scan save files — build slot info table
        // ================================================================
        case STATE_INIT:
        {
            state = STATE_IDLE;

            for (int slotIndex = 0; slotIndex < 9; slotIndex++)
            {
                sprintf(g_saveFileName, "%ssavedat%d.dat", GetSaveRoot(), slotIndex + 1);
                FILE* fp = fopen(g_saveFileName, "r");
                if (fp == NULL) {
                    save_slots[slotIndex].hasData = 0;
                } else {
                    ReadSaveFile(g_saveFileName, saveBuffer);

                    save_slots[slotIndex].hasData       = 1;
                    save_slots[slotIndex].characterId   = saveBuffer[OFFSET_CHARACTER_ID];
                    save_slots[slotIndex].savesCount    = saveBuffer[OFFSET_SAVES_COUNT];
                    save_slots[slotIndex].stageId       = saveBuffer[OFFSET_STAGE_ID];
                    save_slots[slotIndex].roomId        = saveBuffer[OFFSET_ROOM_ID];
                    save_slots[slotIndex].dcGameMode    = (unsigned char)saveBuffer[OFFSET_DC_MODE];

                    fclose(fp);
                }
            }
            // Fall through to state 1
        }

        // ================================================================
        // State 1: Main navigation — cursor movement and selection
        // ================================================================
        case STATE_IDLE:
        {
            // Blink cursor handling (matches original: test old value before decrement)
            if (blink_timer == 0) {
                blink_state = !blink_state;
                blink_timer = 5;
            } else {
                blink_timer--;
            }

            // UP Pressed (use g_RawPadHeld for continuous held detection)
            if ((g_RawPadHeld & 0x1000) != 0) {
                blink_state = 0;
                blink_timer = 5;
                input_delay = 6;
                if (--selected_slot < 0) selected_slot = 8;
                state = STATE_INPUT_DELAY;
                play_sfx(sfxBank, 30);
            }

            // Down pressed (use g_RawPadHeld for continuous held detection)
            if ((g_RawPadHeld & 0x4000) != 0) {
                blink_state = 0;
                blink_timer = 5;
                input_delay = 6;
                if (++selected_slot > 8) selected_slot = 0;
                state = STATE_INPUT_DELAY;
                play_sfx(sfxBank, 30);
            }

            // SideWinder pad check
            DWORD sidewinderBtn = 0;
            if (g_bPadConnected) {
                sidewinderBtn = read_sidewinder_pad() & 0x10000;
            }

            // PAD_CROSS pressed (confirm) or SideWinder start
            if (((g_PlayerDpadPressed & 0x4000) != 0) || (sidewinderBtn != 0)) {
                if (selected_slot == SAVE_SLOT_COUNT) {
                    // Exit option selected
                    play_sfx(sfxBank, 29);
                    if (mode == 0) {
                        // In-game: return to gameplay
                        if (!cutsceneReset) {
                            cut_set();
                            g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
                            StMask(1, 0);
                        }
                        g_loadSaveStateFlag = 0;
                        return;
                    }
                    // From title: chain back to title_state
                    Task_sleep(4);
                    Task_chain((void*)title_state);
                } else {
                    // Port-only: a DC TRAINING/ADVANCED slot is greyed and
                    // cannot be loaded in OG mode. Saving over it is still
                    // allowed, so only the load path (mode 1) is refused.
                    if (mode == 1 && SaveSlotLoadBlocked(&save_slots[selected_slot])) {
                        play_sfx(sfxBank, 29);
                        break;
                    }
                    // Non-exit slot selected → go to mode-specific state
                    // (original: state = mode + 3 → 3 = save, 4 = load)
                    play_sfx(sfxBank, 31);
                    state = (MenuState)(mode + 3);
                }
            }

            // (cancel/back)
            if ((g_PlayerDpadPressed & 0x8000) != 0) {

                play_sfx(sfxBank, 29);

                if (mode == 0) {
                    // In-game: return to gameplay
                    if (!cutsceneReset) {
                        cut_set();
                        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
                        StMask(1, 0);
                    }
                    g_loadSaveStateFlag = 0;
                    return;
                } else {
                    // From title: chain back to title_state
                    Task_sleep(4);
                    Task_chain((void*)title_state);
                }
            }

            break;
        }

        // ================================================================
        // State 2: Input repeat delay — prevents rapid cursor movement
        // ================================================================
        case STATE_INPUT_DELAY:
        {
            // no direction held (use g_RawPadHeld for continuous held detection)
            if ((g_RawPadHeld & (0x1000 | 0x4000)) == 0) {
                input_delay = 0;
            }

            // Original tests the old value, then decrements; on old==0 → idle
            if (input_delay == 0) {
                state = STATE_IDLE;
            } else {
                input_delay--;
            }

            // still blink (matches original: test old value before decrement)
            if (blink_timer == 0) {
                blink_state = !blink_state;
                blink_timer = 5;
            } else {
                blink_timer--;
            }
            break;
        }

        // ================================================================
        // State 3: Save mode — check if slot already has data
        // ================================================================
        case STATE_SAVE_SLOT_SELECTED:
        {
            if (save_slots[selected_slot].hasData) {
                // Slot hasData -> confirmation dialog
                blink_state = 1;
                input_delay = 0;
                state = STATE_CONFIRM_OVERWRITE;
            } else {
                // Empty slot -> save immediately
                state = STATE_PERFORM_SAVE;
            }
            break;
        }

        // ================================================================
        // State 4: Load mode — load game from save slot
        // ================================================================
        case STATE_LOAD_SLOT_SELECTED:
        {
            // Belt and braces: refused here too in case the slot list changed.
            if (SaveSlotLoadBlocked(&save_slots[selected_slot])) {
                state = STATE_IDLE;
                break;
            }
            if (save_slots[selected_slot].hasData) {
                sprintf(g_saveFileName, "%ssavedat%d.dat", GetSaveRoot(), selected_slot + 1);
                EnsureDirectoryExists(GetSaveRoot());
                int fileSize = ReadSaveFile(g_saveFileName, fileBuffer);
                RestoreSaveBlock(fileBuffer, fileSize);

                g_main_state_flags |= MSF_CONTINUE_GAME;
                g_playerEntityPointer.id = g_SelectedCharactedId;
                if ((g_SelectedCharactedId & 3) != 0) {
                    g_main_state_flags |= MSF_CHAR_VARIANT;
                }
                g_loadSaveStateFlag = 0;
                return;
            }
            // Empty slot – go back
            state = STATE_IDLE;
            break;
        }

        // ================================================================
        // State 5: Confirmation dialog (Yes/No for overwrite)
        // ================================================================
        case STATE_CONFIRM_OVERWRITE:
        {
            PrintFormattedText(49, 193, 1, L.overwritePrompt);
            PrintFormattedText(118, 209, 0, L.yesNo);

            // no direction held → reset delay
            if ((g_RawPadHeld & 0x5000) == 0) {
                input_delay = 0;
            }

            if (input_delay == 0) {
                // blink cursor
                if (blink_timer == 0) {
                    blink_state = !blink_state;
                    blink_timer = 5;
                } else {
                    blink_timer--;
                }

                // Left arrow → YES (0), right arrow → NO (1)
                if ((g_RawPadHeld & 0x8000) != 0) {
                    blink_state = 0;
                    blink_timer = 5;
                    input_delay = 6;
                    confirm_choice = 0;
                }
                if ((g_RawPadHeld & 0x2000) != 0) {
                    blink_state = 0;
                    blink_timer = 5;
                    input_delay = 6;
                    confirm_choice = 1;
                }
                // Confirm: YES → perform save, NO → back to navigation
                if ((g_PlayerDpadPressed & 0x4000) != 0) {
                    state = (confirm_choice == 1) ? STATE_IDLE : STATE_PERFORM_SAVE;
                }
                // Cancel → back to navigation
                if ((g_PlayerDpadPressed & 0x8000) != 0) {
                    state = STATE_IDLE;
                }
            } else {
                input_delay--;
            }
            break;
        }

        // ================================================================
        // State 6: Execute save — build save data and write file
        // ================================================================
        case STATE_PERFORM_SAVE:
        {
            EnsureDirectoryExists(GetSaveRoot());
            // (The original seeds the display string with a copy of the save
            // directory and truncates at a backslash — dead, the reveal
            // string is fully rebuilt below.)

            // Consume the ink ribbon. Jill needs the ribbon flag (bit 0x7B),
            // Chris always spends one when the typewriter offers it.
            if ((useInkRibbon != 0) &&
                (((g_playerEntityPointer.id & 3) != 1) ||
                 (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) != 0))) {
                g_selectedItemId = ITEM_INK_RIBBONS;
                use_room_action_item();
            }

            // Snapshot the current player state into the save block
            // (g_BioCard +0x22B..0x232).
            g_PlayerPosXCopy         = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[0];
            g_PlayerPosZCopy         = (short)g_playerEntityPointer.scaMatrixData.localMatrix.t[2];
            g_SelectedCharactedId    = g_playerEntityPointer.id;
            g_PlayerHealthStatusCopy = g_playerEntityPointer.healthStatusFlags;
            g_PlayerDirAngleCopy     = g_playerEntityPointer.directionAngle;

            sprintf(g_saveFileName, "%ssavedat%d.dat", GetSaveRoot(), selected_slot + 1);

            // Refresh the joystick-remap backup; the file stores both tables,
            // and the sidewinder-dependent one receives the live bindings.
            memcpy(g_bPadConnected ? g_joyRemapBackupJoy : g_joyRemapBackupKey,
                   g_JoyRemapTbl[1], 0x80);

            // Assemble the save file (2690 bytes). The 0x820..0x91F region is
            // written twice on purpose (joy remap, then the joy backup over
            // its upper half) — same order as the original. The buffer is
            // zeroed first so the block tail the port does not model
            // (0x43D..0x800) stays deterministic instead of stack garbage.
            memset(fileBuffer, 0, SAVE_FILE_SIZE);
            memcpy(fileBuffer + 0x000, g_BioCardData, sizeof(BioCardLayout));
            memcpy(fileBuffer + OFFSET_PAD_REMAP, g_padRemapSubTable3,
                   sizeof(g_padRemapSubTable3));
            fileBuffer[OFFSET_CONTROLLER_CFG] = g_controllerConfig;
            memcpy(fileBuffer + OFFSET_KEY_BINDINGS, g_keyBindingData, 0x20);
            memcpy(fileBuffer + OFFSET_JOY_REMAP, g_JoyRemapTbl, 0x100);
            memcpy(fileBuffer + OFFSET_ROOM_BGM, g_roomBgmState, 0xE0);
            fileBuffer[OFFSET_SIDEWINDER] = (char)g_bPadConnected;
            fileBuffer[OFFSET_COSTUME_VARIANT]   = (char)g_bCostumeVariant;
            memcpy(fileBuffer + OFFSET_JOY_BACKUP, g_joyRemapBackupJoy, 0x80);
            memcpy(fileBuffer + OFFSET_KEY_BACKUP, g_joyRemapBackupKey, 0x80);
            FileWrite(g_saveFileName, fileBuffer, SAVE_FILE_SIZE);

            // Build the save-animation reveal string:
            //   name + separator + count(2) + separator + location.
            // 57 bytes for the USA screen (10 + 8 + 39), 31 for the Japanese
            // one (6 + 8 + 17); glyph 0x38 is a backslash in fontus.tim and a
            // forward slash in FONT.TIM, which is why the two look different
            // while the byte is the same.
            const unsigned char* nameStr = L.charNames[g_SelectedCharactedId & 3];
            for (int i = 0; i < L.nameLen; i++) {
                displayStr[i] = (char)nameStr[i];
            }
            displayStr[L.nameLen + 0] = (char)0x38;
            displayStr[L.nameLen + 1] = (char)0xFB;
            displayStr[L.nameLen + 2] = (char)((g_SavesCounter / 10) + 0x0C);
            displayStr[L.nameLen + 3] = (char)0xFB;
            displayStr[L.nameLen + 4] = (char)((g_SavesCounter % 10) + 0x0C);
            displayStr[L.nameLen + 5] = (char)0xFB;
            displayStr[L.nameLen + 6] = (char)0x38;
            displayStr[L.nameLen + 7] = (char)0xFB;
            int locIdx = GetSaveLocationIndex(g_stageId, g_roomId);
            const unsigned char* locStr = L.locNames[locIdx];
            for (int i = 0; i < L.locLen; i++) {
                displayStr[SAVE_REVEAL_LOC_BASE(L) + i] = (char)locStr[i];
            }

            g_SavesCounter = (g_SavesCounter + 1 >= 100) ? 99 : (unsigned char)(g_SavesCounter + 1);

            // Start save animation
            anim_counter = 2;
            state = STATE_SAVE_ANIM_STEP1;
            break;
        }

        // ================================================================
        // State 7: Save animation — text reveal effect
        // Reveals the save slot text (char name + count + location) character
        // by character using PrintFormattedText with the reveal buffer.
        // ================================================================
        case STATE_SAVE_ANIM_STEP1:
        {
            state = STATE_SAVE_ANIM_STEP2;
            anim_timer = 5;

            // Copy the reveal prefix into the anim buffer
            int copyLen = (anim_counter > 0) ? anim_counter : 0;
            if (copyLen > 0) {
                memcpy(animBuf, displayStr, copyLen);
            }
            animBuf[copyLen] = 1;           // STR terminator
            anim_counter += 2;

            // The reveal walks two bytes at a time and stops once the counter
            // steps past the string: 58 for the USA's 57 bytes, 32 for the
            // Japanese screen's 31 (0x20 at 0x004362e9).
            if (anim_counter > SAVE_REVEAL_LEN(L) + 1) {
                // Animation finished
                if (!cutsceneReset) {
                    cut_set();
                    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
                    StMask(1, 0);
                }
                g_loadSaveStateFlag = 0;
                return;
            }

            PrintFormattedText(55, (short)(16 * selected_slot + 45),
                               DcSaveRowColour((unsigned int)g_DcGameMode),
                               (unsigned char*)animBuf);

            // "Typewriter" tick — plays for every revealed non-space char
            // (the char two back from the copy end; spaces are 0x00).
            if (animBuf[copyLen - 2] != 0) {
                play_sfx(sfxBank, 31);
            }
            break;
        }

        // ================================================================
        // State 8: Save animation delay — pause between text reveals
        // ================================================================
        case STATE_SAVE_ANIM_STEP2:
        {
            PrintFormattedText(55, (short)(16 * selected_slot + 45),
                               DcSaveRowColour((unsigned int)g_DcGameMode),
                               (unsigned char*)animBuf);

            // Original tests the old value, then decrements; on old<=0 → step 1
            if (anim_timer <= 0) {
                state = STATE_SAVE_ANIM_STEP1;
            } else {
                anim_timer--;
            }
            break;
        }

        // ================================================================
        // State 9: Error messages — "NOT ENOUGH FREE SPACE" / "ON HARD DRIVE."
        // Shown when save fails. Waits for any button to return to navigation.
        // ================================================================
        case STATE_ERROR_MSG:
        {
            PrintFormattedText(49, 209, 0, L.errLine1);
            PrintFormattedText(49, 225, 0, L.errLine2);
            if (((g_RawPadHeld & 0xF000) != 0) ||
                ((g_PlayerDpadPressed & 0xC000) != 0)) {
                g_RawPadHeld = 0;
                g_PlayerDpadPressed = 0;
                state = STATE_IDLE;
            }
            break;
        }

        } // end switch

        // ================================================================
        // Render save slots using PrintFormattedText (STR encoding)
        // Positions from disassembly: slot X=55, save count X=103, location X=127
        // ================================================================
        int y = 45;
        for (int i = 0; i < 8; i++, y += 16) {
            if ((state == STATE_SAVE_ANIM_STEP1 || state == STATE_SAVE_ANIM_STEP2) && selected_slot == i)
                continue; // handled separately during animation

            const SaveSlotInfo* slot = &save_slots[i];

            if (slot->hasData) {
                // The row's colour is the mode the save was made in (DC only;
                // 0 everywhere else, which is what the USA build always draws).
                unsigned char rowColour = DcSaveRowColour(slot->dcGameMode);
                const int loadBlocked = SaveSlotLoadBlocked(slot);
                if (loadBlocked) {
                    // Grey tint (AddTintSprite maps clutY 0x1E3 to 204,204,204).
                    rowColour = 3;
                }
                unsigned char charIdx = (unsigned char)(save_slots[i].characterId & 3);

                // ADVANCED* draws the name once more first, in the TRAINING
                // palette at brightness 1 (PS1 0x800191b4 truncates its row
                // buffer after the five name cells and prints it at colour
                // 0x11, then prints the whole row at colour 2). Skipped for a
                // greyed blocked row.
                if (!loadBlocked && rowColour != 0 && (slot->dcGameMode & 3) == DC_DIFFICULTY_ADVANCED_HOLD) {
                    PrintFormattedText(55, (short)y, 0x11, L.charNames[charIdx]);
                }

                // Filled slot: draw the separator template, then overlay the
                // character name on it
                PrintFormattedText(55, (short)y, rowColour, L.filledSlot);

                // Character name overlay at same X=55 (from PTR_DAT_004d4118 /
                // the JPN 0x004b1020)
                PrintFormattedText(55, (short)y, rowColour, L.charNames[charIdx]);

                // Save count (sprintf + PrintText8x14 from assembly)
                int saveNum = save_slots[i].savesCount % 100;
                sprintf(PRINT_TEXT_BUFFER, "%02d", saveNum);
                PrintText8x14(L.countX, (short)y, rowColour, 0);

                // Location name (from PTR_DAT_004d42b8 / the JPN 0x004b1110)
                int locIdx = GetSaveLocationIndex(save_slots[i].stageId, save_slots[i].roomId);
                PrintFormattedText(L.locX, (short)y, rowColour, L.locNames[locIdx]);
            } else {
                // Empty slot: full dash template
                PrintFormattedText(55, (short)y, 0, L.emptySlot);
            }
        }

        // Print exit option — assembly draws two parts at same position:
        // 1. the verb, in the first cells of the line
        // 2. the suffix, whose leading spaces skip over the verb
        // Result: "DO NOT SAVE"/"DO NOT LOAD", or "セーブしない"/"ロードしない"
        // on the Japanese screen, which puts the verb first and the negation
        // after it (0x004b1008 + 0x004b0ff8).
        {
            int y = 45 + SAVE_SLOT_COUNT * 16;
            PrintFormattedText(55, (short)y, 0, L.exitNames[mode]);
            PrintFormattedText(55, (short)y, 0, L.exitSuffix);
        }

        // Print header — assembly draws two parts at same position:
        // 1. "SAVE"/"LOAD"
        // 2. "     GAME" — spaces don't overwrite, "GAME" follows
        // Result: "SAVE GAME" / "LOAD GAME". Latin in both releases; only the
        // X moves, so the 9-cell word stays centred for either glyph width.
        PrintFormattedText(L.headerX, 13, 0, L.headerNames[mode]);
        PrintFormattedText(L.headerX, 13, 0, L.headerSuffix);

        // Draw cursor arrow(s) — matching assembly at 0x00493ca8
        // blinkToggle: 0=cursor visible, 1=cursor hidden (toggles every 5 frames)
        // confirmChoice: 0=YES position, 1=NO position (state 5 only)
        {
            if (state == STATE_CONFIRM_OVERWRITE) {
                // State 5: draw slot cursor (always visible) + confirm cursor (blinks)
                DrawSaveCursor(L.cursorX, (short)(45 + selected_slot * 16), 0);
                int confirmX = confirm_choice * L.confirmStride + 118;
                DrawSaveCursor((short)confirmX, 209, blink_state);
            } else {
                // States 1-4,9: cursor at slot position, blinks with blinkToggle
                DrawSaveCursor(L.cursorX, (short)(45 + selected_slot * 16), blink_state);
            }
        }

        Task_sleep(1);

    } while (true);
}

// ============================================================================
// DebugSaveMenu (0x00494050) - F5 debug save picker, driven as a task from
// game_loop's debug branch (GameLoop.cpp 0x00480f70).
//
// Draws an 8-slot list at the screen's left edge (rows y=10..122 step 16,
// entries "1".."8", the selected row prefixed with ">"). Confirm/cancel use
// the same remapped d-pad edges (0x4000 / 0x8000) as every other menu in
// the game - the same keys that confirm/cancel the regular save screen.
// On confirm it snapshots the player's position/angle/character into the bio
// card and writes the whole 0x800-byte bio card block to
// GetSaveRoot() savedat<N>.dat - exactly what LoadSaveGameState writes, so
// the slot shows up on the load screen.
// Every frame logs both pressed words under "[DBGSAVE]" so input issues are
// diagnosable from the debugger output.
//
// Original quirk kept: it patched the '1' in "savedat1.dat" by the selection
// instead of formatting the number (0x004d441c template + byte add).
// ============================================================================
void DebugSaveMenu(void)
{
    int selected = 0;
    for (;;) {
        int row = 10;
        int entry = 0;
        do {
            sprintf(PRINT_TEXT_BUFFER,
                    (selected == entry) ? "> %d" : "  %d",   // 0x004d4414 / 0x004d440c
                    entry + 1);
            PrintText8x8(0, (short)row, 0, 1);
            row += 0x10;
            entry++;
        } while (row < 0x8a);

#ifdef _DEBUG
        {
            char dbg[96];
            sprintf(dbg, "[DBGSAVE] raw=%04X dpad=%04X sel=%d\n",
                    (unsigned)(WORD)g_PlayerPadPressed,
                    (unsigned)(WORD)g_PlayerDpadPressed, selected);
            OutputDebugStringA(dbg);
        }
#endif

        if ((g_PlayerPadPressed & 0x1000) != 0 && 0 < selected) {
            selected--;
        }
        if ((g_PlayerPadPressed & 0x4000) != 0 && selected < 7) {
            selected++;
        }
        if ((g_PlayerDpadPressed & 0x4000) != 0) {   // confirm (same edge as the save screen)
            break;
        }
        if ((g_PlayerDpadPressed & 0x8000) != 0) {   // cancel
            OutputDebugStringA("[DBGSAVE] cancelled\n");
            return;
        }
        Task_sleep(1);
    }

    g_PlayerPosXCopy      = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_PlayerPosZCopy      = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
    g_SelectedCharactedId = g_playerEntity.id;
    g_PlayerDirAngleCopy  = g_playerEntity.directionAngle;

    // Write through GetSaveRoot() so the slot lands where the load screen
    // scans (the original hardcoded "SAVE\\" because its installer created
    // that folder; debug builds keep saves under .\assets\save\).
    EnsureDirectoryExists(GetSaveRoot());

    char path[260];
    sprintf(path, "%ssavedat%d.dat", GetSaveRoot(), selected + 1);
    int written = FileWrite(path, g_BioCardData, 0x800);
    {
        char dbg[320];
        sprintf(dbg, "[DBGSAVE] confirm sel=%d write '%s' -> %s (%d bytes)\n",
                selected, path, (written < 0) ? "FAILED" : "OK", written);
        OutputDebugStringA(dbg);
    }
    g_SavesCounter++;
}

