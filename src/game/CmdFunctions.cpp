// CmdFunctions.cpp - SCD command dispatch functions (decompiled from Ghidra)
// All functions in this file are entries in the script_command_funcs_table[81].
// Each function takes no parameters and returns int (0 = stop, 1 = continue).
#include "../Globals.h"
#include <cstdio>
#include <cstring>
#include "../DebugPrint.h"
#include "dc/ArrangeStages.h"   // room_file_stage() - see cmd_state_word_set

// Forward declarations for functions defined in other files
extern unsigned int set_message_display(unsigned short msg_id, unsigned short pause_game);
extern void cut_set(void);
extern void Play3DSnd(int bank, int id, int vol, int pos);
extern void play_sfx(int bank, int soundId);
extern void play_sound_and_voice_effect(int type, int id);
extern void SetSndSlot(int bank, int slot);
extern void setSndStop(int bank);
extern void BuildSndFadeTbl(char distSteps, int fadeType);
extern void set_volume(int bank, int vol);
// Effect_CreateBillboard is declared in Globals.h with the real signature
// (void* spriteInfo, void* pos). Do NOT re-declare it here with MATRIX*/int* -
// that created a second overload that resolved to an empty stub.
extern int get_item_slot(unsigned char itemId);
extern void rearrange_item_slots(void);
extern unsigned int Flg_ck(int baseAddr, unsigned int bitIndex);
extern void FUN_00473f10(int* baseAddr, unsigned int bitIndex);
extern int SquareRoot0(int val);
extern void FUN_004805d0(short param1, unsigned int param2, unsigned int param3, unsigned int param4);
extern void FUN_004804a0(short param1, unsigned int param2, short param3, unsigned int param4);
extern void FUN_0047cf80(int param1, unsigned int param2, unsigned int param3, unsigned int param4, MATRIX* param5);
extern void FUN_00473ea0(int param1, void* param2, ScaMatrixData* param3);
extern void scd_model_tint_apply(short p1, short p2, short p3, unsigned short p4, unsigned short p5, unsigned char p6);
extern void FUN_00473d10(short p1, short p2, short p3, unsigned short p4, unsigned short p5, char p6);
extern void FUN_00473d60(char p1, unsigned char p2, unsigned char p3);
extern void RoomSpr_SetInactive(char id);  // 0x00476130
extern void RoomSpr_SetActive(char id);    // 0x00476170
extern void setBackColor(unsigned short r, unsigned short g, unsigned short b);
extern void FUN_0048bfe0(void);
extern void FUN_0048c020(int param);
extern void FUN_004870d0(int param);
extern void FUN_00484d90(int param1, unsigned char param2, unsigned char param3);
extern void FUN_00484e40(int param1, unsigned char param2, unsigned char param3);
extern void FUN_00473e40(int param);
extern int FUN_0048f330(unsigned char param);
extern void ProcessTmdAsync(unsigned int tmdData);
extern unsigned int ProcessTmdTextures(char mode, unsigned int* tmdBase, int bank, int depth);
extern void ClearTmdProcessingFlag(void);
extern void InitScaMatrix(int parentPtr, ScaMatrixData* matrix);
extern unsigned char QueueTextureForProcessing(char bank, unsigned char depth);
extern void SetupEntityJointAnimation(void);
extern void ScdEventEntry_Create(unsigned int slot, int scriptIndex);  // RoomEvents.cpp

// Externs for globals used by cmd functions
extern void*          g_RoomInitScd;
// g_message_flags already declared in Globals.h
// g_menu_choice_id is now a macro to g_BioCard.menu_choice_id (see BioCard.h)
// DAT_00be9830 (g_fwdPosActionId) is now a macro to g_BioCard.fwdPosActionId (see BioCard.h)
extern unsigned int   g_itemUseFlags[2];

// Last used room action entry, inclusive (used for bounds checking)
extern void*          g_RoomActionTail;         // 0x00d91bc0

// ============================================================================
// Helper: read 16-bit value from SCD opcode stream
// ============================================================================
static inline unsigned short scd_read_u16(int offset) {
    return *(unsigned short*)(g_ScdOpcodes + offset);
}

static inline short scd_read_s16(int offset) {
    return *(short*)(g_ScdOpcodes + offset);
}

// ============================================================================
// 0x00 - cmd_block_end (0x004604d0)
// Ends the current script BLOCK. It clears g_ScriptContinueFlag and returns 0
// without advancing the stream, so run_command_functions leaves the inner loop
// AND skips the branch-stack unwind, moving straight on to the next block.
// Other commands return 0 to mean "condition failed" and DO get unwound - only
// this one zeroes the flag. Blocks are 4-byte aligned with 0x00 padding, so the
// trailing zeros a disassembler shows after it are never executed.
// ============================================================================
int cmd_block_end(void)
{
    g_ScriptContinueFlag = 0;
    return 0;
}

// ============================================================================
// 0x01 - cmd_if (0x004604e0)
// Push current position + offset onto the call stack for conditional.
// ============================================================================
int cmd_if(void)
{
    unsigned short val = scd_read_u16(0);
    g_ScdOpcodes += 2;
    *g_CmdOpcodesPointer = (unsigned int)(val >> 8) + (unsigned int)g_ScdOpcodes;
    g_CmdOpcodesPointer++;
    g_ScriptContinueFlag++;
    return 1;
}

// ============================================================================
// 0x02 - cmd_else (0x00460520)
// Pop call stack and jump to else branch.
// ============================================================================
int cmd_else(void)
{
    g_CmdOpcodesPointer--;
    g_ScriptContinueFlag--;
    g_ScdOpcodes = g_ScdOpcodes + (unsigned int)g_ScdOpcodes[1];
    return 1;
}

// ============================================================================
// 0x03 - cmd_end_if (0x00460550)
// End of if/else block - pop call stack.
// ============================================================================
int cmd_end_if(void)
{
    g_ScriptContinueFlag--;
    g_CmdOpcodesPointer--;
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x04 - cmd_bit_test (0x00460570)
// Test a bit in a flag bank. Returns 1 if condition matches, 0 otherwise.
// Flag banks: 
//		0=g_ScenarioFlags, 
//		1=g_ScenarioFlags2, 
//		2=g_LocksFlags,
//		3=g_EnemiesFlags, 
//		4=g_SysFlags, 
//		5=g_main_state_flags,      
//		6=g_message_flags, 
//		7=g_roomItemsFlags, 
//		8=g_RoomFlags, 
//		9=g_itemUseFlags
//
// ============================================================================
int cmd_bit_test(void)
{
    unsigned int* flagBank;
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;

    switch (op1 >> 8) {
    case 0: flagBank = (unsigned int*)&g_ScenarioFlags; break;
    case 1: flagBank = (unsigned int*)&g_ScenarioFlags2; break;
    case 2: flagBank = (unsigned int*)g_LocksFlags; break;   // 0x00be9874 - bank 2 IS the door/desk lock flags (g_BioCard.locksFlags); door_try_enter checks the same array
    case 3: flagBank = (unsigned int*)g_EnemiesFlags; break;
    case 4: flagBank = (unsigned int*)g_SysFlags; break;
    case 5: flagBank = (unsigned int*)g_MainStateFlagBank; break;   // both dwords: sel 0x20+ is msf2
    case 6: flagBank = (unsigned int*)&g_message_flags; break;
    case 7: flagBank = (unsigned int*)&g_roomItemsFlags; break;
    case 8: flagBank = (unsigned int*)&g_RoomFlags; break;
    case 9: flagBank = (unsigned int*)g_itemUseFlags; break;
    default: return 0;
    }

    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned int bitOffset = (op2 & 0xe0) >> 3;
    unsigned int bitIndex = op2 & 0x1f;
    int* target = (int*)((char*)flagBank + bitOffset);
    unsigned int condition = op2 >> 8;

    // The original shifts the word left and tests the sign bit. Shifting a
    // negative int is UB, so test the same bit through an unsigned value.
    return ((((unsigned int)*target << bitIndex) & 0x80000000u) != 0) ^ condition;
}

// ============================================================================
// 0x05 - cmd_bit_op (0x00460650)
// Set/clear/toggle a bit in a flag bank.
// Operation: 0=OR(set), 1=AND(clear), 2=XOR(toggle)
// ============================================================================
int cmd_bit_op(void)
{
    unsigned int* flagBank;
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 4;

    switch (op1 >> 8) {
    case 0: flagBank = (unsigned int*)&g_ScenarioFlags; break;
    case 1: flagBank = (unsigned int*)&g_ScenarioFlags2; break;
    case 2: flagBank = (unsigned int*)g_LocksFlags; break;   // 0x00be9874 - bank 2 IS the door/desk lock flags (g_BioCard.locksFlags); door_try_enter checks the same array
    case 3: flagBank = (unsigned int*)g_EnemiesFlags; break;
    case 4: flagBank = (unsigned int*)g_SysFlags; break;
    case 5: flagBank = (unsigned int*)g_MainStateFlagBank; break;   // both dwords: sel 0x20+ is msf2
    case 6: flagBank = (unsigned int*)&g_message_flags; break;
    case 7: flagBank = (unsigned int*)&g_roomItemsFlags; break;
    case 8: flagBank = (unsigned int*)&g_RoomFlags; break;
    case 9: flagBank = (unsigned int*)g_itemUseFlags; break; // per-frame item-use flags (bit itemId-0x1B usable, 0x3F radio transmission)
    default: return 0;
    }

    unsigned short op2 = scd_read_u16(-2);
    unsigned int operation = op2 >> 8;
    unsigned int bitOffset = (op2 & 0xe0) >> 3;
    unsigned int bitIndex = op2 & 0x1f;
    unsigned int* target = (unsigned int*)((char*)flagBank + bitOffset);
    unsigned int mask = 0x80000000U >> bitIndex;

    if (operation == 0) {
        *target |= mask;
    } else if (operation == 1) {
        *target &= ~mask;
    } else if (operation == 2) {
        *target ^= mask;
    } else {
        return 0;
    }
    return 1;
}

// ============================================================================
// 0x06 - cmd_state_byte_test (0x00460760)
// Compare one byte of the BioCard state block against a constant. The index is
// a byte offset from g_stageId (BioCard +0x200): 0 stageId, 1 roomId,
// 2 roomCameraId, 3 attractMode_RoomCameraId, 4 cutId, 5 menu_choice_id,
// 6 selectedItemId, 7 totalHeldItems, 8 specialRoomLightR, 9 characterModelId,
// 10 scdLastEnemyFlags, 11 bulletEffectId, 12-14 pickupQtyA/B/C,
// 16 fwdPosActionId, 17 entPosActionId, 18 usedItemId, 19 pickedItemId.
// Sampled scripts test indices 2 (268 sites), 3, 5, 9, 16, 17, 18 and 19 -
// nothing room-specific, which is why this is not a "room state" test.
// ============================================================================
int cmd_state_byte_test(void)
{
    unsigned short op1 = scd_read_u16(0);
    unsigned short op2 = scd_read_u16(2);
    g_ScdOpcodes += 4;

    unsigned char stateVal = ((unsigned char*)&g_stageId)[op1 >> 8];
    unsigned short compareVal = op2 >> 8;
    unsigned char mode = (unsigned char)op2;

    switch (mode) {
    case 0: return compareVal == stateVal;
    case 1: return compareVal < stateVal;
    case 2: return compareVal <= stateVal;
    case 3: return stateVal < compareVal;
    case 4: return stateVal <= compareVal;
    case 5: return compareVal != stateVal;
    default: return 0;
    }
}

// ============================================================================
// 0x07 - cmd_state_word_test (0x00460800)
// Compare one SHORT of the BioCard state block against a constant. The index
// counts shorts from g_fading_state (BioCard +0x214): 0 fadingState,
// 1 specialRoomLightState, 2 specialRoomLightDelta, 3 randSeed,
// 4 countdownTimer, 5 playerHealthCopy, 6 playerDpadHeld, 7 playerDpadPressed.
// Of the 221 uses reachable in the shipped RDTs, 214 read index 3 (randSeed -
// this is how scripts roll dice) and 7 read index 4 (the lab countdown); none
// reads index 0 at all. See the survey caveat above.
// ============================================================================
int cmd_state_word_test(void)
{
    unsigned int op1 = *(unsigned int*)g_ScdOpcodes;
    g_ScdOpcodes += 6;

    unsigned short stateVal = ((short*)&g_fading_state)[(op1 & 0xff0000) >> 0x10];
    // Original: MOV AX,word ptr [EDX+0x4] - the compare value is at +4, and the
    // pointer has already advanced 6, so it is at -2. Reading -4 gave the +2 pad.
    unsigned short compareVal = scd_read_u16(-2);
    unsigned char mode = (unsigned char)(op1 >> 0x18);

    switch (mode) {
    case 0: return compareVal == stateVal;
    case 1: return stateVal > compareVal;
    case 2: return stateVal >= compareVal;
    case 3: return compareVal > stateVal;
    case 4: return compareVal >= stateVal;
    case 5: return compareVal != stateVal;
    default: return 0;
    }
}

// ============================================================================
// 0x08 - cmd_state_byte_set (0x004608a0)
// Write one byte of the BioCard state block; same index space as 0x06. The
// index is unbounded and scripts use that: ROOM1130 writes index 82, which
// lands in scenarioFlags2[30], and ROOM40A0 writes index 57 (scenarioFlags2[5]).
// ============================================================================
int cmd_state_byte_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    unsigned short op2 = scd_read_u16(2);
    g_ScdOpcodes += 4;
    (&g_stageId)[op1 >> 8] = (unsigned char)op2;
    return 1;
}

// ============================================================================
// 0x09 - cmd_cut_lock_set (0x00460920)
// Set camera cut and disable camera changes.
// ============================================================================
int cmd_cut_lock_set(void)
{
    g_ScdOpcodes++;
    g_cutId = g_roomCameraId;
    g_roomCameraId = *g_ScdOpcodes;

    // Walk cam_switch_zones to find matching camera
    unsigned short camId = *(unsigned short*)((char*)g_RdtPointer->cam_switch_zones + 2);
    unsigned int zonePtr = (unsigned int)g_RdtPointer->cam_switch_zones;
    while (camId != g_roomCameraId) {
        g_CurrentRdtDataTypePtr = (void*)(zonePtr + 0x14);
        camId = *(unsigned short*)(zonePtr + 0x16);
        zonePtr = (unsigned int)g_CurrentRdtDataTypePtr;
    }
    g_CurrentRdtDataTypePtr = (void*)zonePtr;
    cut_set();
    g_ScdOpcodes++;
    g_main_state_flags |= MSF_CAMERA_LOCK;
    return 1;
}

// ============================================================================
// 0x0A - cmd_current_cut_set (0x00460990)
// Restore camera to previous cut and enable camera changes.
// ============================================================================
int cmd_current_cut_set(void)
{
    g_roomCameraId = g_cutId;
    unsigned short camId = *(unsigned short*)((char*)g_RdtPointer->cam_switch_zones + 2);
    unsigned int zonePtr = (unsigned int)g_RdtPointer->cam_switch_zones;
    while (camId != g_cutId) {
        g_CurrentRdtDataTypePtr = (void*)(zonePtr + 0x14);
        camId = *(unsigned short*)(zonePtr + 0x16);
        zonePtr = (unsigned int)g_CurrentRdtDataTypePtr;
    }
    g_CurrentRdtDataTypePtr = (void*)zonePtr;
    cut_set();
    g_main_state_flags &= ~MSF_CAMERA_LOCK;
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x0B - cmd_message_set (0x004609f0)
// Display a message with optional pause.
// ============================================================================
int cmd_message_set(void)
{
    // 4-byte instruction (original: two ADD ...,0x2). The pause argument is a
    // WORD at +2 (MOV CX,word ptr [EAX]), not a byte - reading it as a byte and
    // advancing only 1 left the stream one byte short for everything after it.
    unsigned short msgId = scd_read_u16(0);
    g_ScdOpcodes += 2;
    set_message_display(msgId >> 8, scd_read_u16(0));
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x0C - cmd_door_set (0x004611b0)
// Set up a door trigger zone in the room action table.
// ============================================================================
int cmd_door_set(void)
{
    dbg_printf("DOOR_AT_SET START %s\n", "door_at_set");
    unsigned char doorNumber = g_ScdOpcodes[1];
    int tableOffset = (unsigned int)doorNumber * 0xc;
    unsigned char* entry = &g_RoomActionTable[tableOffset];
    if (entry > (unsigned char*)g_RoomActionTail) {
        g_RoomActionTail = entry;
    }
    entry[0] = 1;
    entry[1] = g_ScdOpcodes[0x19];
    *(unsigned short*)(entry + 2) = (unsigned short)doorNumber;
    *(unsigned int*)(entry + 8) = (unsigned int)(g_ScdOpcodes + 2);
    g_ScdOpcodes += 0x1a;
    dbg_printf("DOOR_AT_SET END %s\n", "door_at_set");
    return 1;
}

// ============================================================================
// 0x0D - cmd_room_action_set (0x00461130)
// Build one room action (AOT) entry from scratch: an 8-byte zone box plus an
// explicit room_check_actions handler index, probe flags and three parameter
// words. The generic sibling of cmd_door_set, which hardcodes handler 1.
//
// Operands: +1 slot, +2..+9 zone (u16 x, z, width, depth), +0xA handler,
// +0xB flags, +0xC/+0xE/+0x10 the entry words at +2/+4/+6. Width 18.
// ============================================================================
int cmd_room_action_set(void)
{
    unsigned char itemSlot = g_ScdOpcodes[1];
    int tableOffset = (unsigned int)itemSlot * 0xc;
    unsigned char* entry = &g_RoomActionTable[tableOffset];
    if (entry > (unsigned char*)g_RoomActionTail) {
        g_RoomActionTail = entry;
    }
    entry[0] = g_ScdOpcodes[10];
    entry[1] = g_ScdOpcodes[11];
    *(unsigned short*)(entry + 2) = *(unsigned short*)(g_ScdOpcodes + 12);
    *(unsigned short*)(entry + 4) = *(unsigned short*)(g_ScdOpcodes + 14);
    *(unsigned short*)(entry + 6) = *(unsigned short*)(g_ScdOpcodes + 16);
    *(unsigned int*)(entry + 8) = (unsigned int)(g_ScdOpcodes + 2);
    g_ScdOpcodes += 18;
    return 1;
}

// ============================================================================
// 0x0E - cmd_skip_2bytes_opcode (0x00460900)
// Skip 2 bytes in the opcode stream.
// ============================================================================
int cmd_skip_2bytes_opcode(void)
{
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x0F - cmd_mirror_set (0x004610b0)
// Arm the room mirror: operand byte 1 goes into main_state_flags bits 0-1
// (bit 0 enables the pass, bit 1 picks the plane axis), followed by the three
// extent words. Re-runs the player joint animation so the mirrored copy exists.
// ============================================================================
int cmd_mirror_set(void)
{
    Entity* entityBkp = ENTITY;
    g_main_state_flags = (g_main_state_flags & ~(MSF_MIRROR_ENABLE | MSF_MIRROR_PLANE_X)) |
                         g_ScdOpcodes[1];
    *(unsigned short*)&g_mirrorExtentMin = *(unsigned short*)(g_ScdOpcodes + 2);
    *(unsigned short*)&g_mirrorExtentMax = *(unsigned short*)(g_ScdOpcodes + 4);
    *(unsigned short*)&g_mirrorPlaneCoord = *(unsigned short*)(g_ScdOpcodes + 6);
    ENTITY = (Entity*)&g_playerEntity;
    SetupEntityJointAnimation();
    FUN_0048bfe0();
    FUN_0048c020(0xe);
    g_ScdOpcodes += 8;
    ENTITY = entityBkp;
    return 1;
}

// ============================================================================
// 0x10 - cmd_used_item_test (0x00460f30)
// Test if the used item ID matches a value.
// ============================================================================
int cmd_used_item_test(void)
{
    unsigned char testVal = g_ScdOpcodes[1];
    g_ScdOpcodes += 2;
    return testVal == g_usedItemId;
}

// ============================================================================
// 0x11 - cmd_picked_item_test (0x00460f10)
// Test if the last picked-up item id matches a value. The id is recorded by
// room_event_item_pickup / pickup_key_event into g_pickedItemId
// (BioCardLayout.pickedItemId at 0x00be9833).
// ============================================================================
int cmd_picked_item_test(void)
{
    unsigned char testVal = g_ScdOpcodes[1];
    g_ScdOpcodes += 2;
    return testVal == g_pickedItemId;
}

// ============================================================================
// 0x12 - cmd_room_action_reset (0x00460fc0)
// Rewrite bytes [0..7] of a room action entry, leaving the SCD record pointer
// at +8 (and so the zone geometry) alone.
// ============================================================================
int cmd_room_action_reset(void)
{
    int base = (unsigned int)g_ScdOpcodes[1] * 0xc;
    g_ScdOpcodes += 10;
    g_RoomActionTable[base]     = g_ScdOpcodes[-8]; // param at +2 from original
    g_RoomActionTable[base + 1] = g_ScdOpcodes[-7]; // param at +3
    *(unsigned short*)(&g_RoomActionTable[base + 2]) = *(unsigned short*)(g_ScdOpcodes - 6);
    *(unsigned short*)(&g_RoomActionTable[base + 4]) = *(unsigned short*)(g_ScdOpcodes - 4);
    *(unsigned short*)(&g_RoomActionTable[base + 6]) = *(unsigned short*)(g_ScdOpcodes - 2);
    return 1;
}

// ============================================================================
// 0x13 - cmd_room_action_arm (0x00461010)
// Arm, disarm or re-type one room action entry: writes only byte 0 (the
// room_check_actions handler index) and byte 1 (the probe flags). Scripts use
// it in if/else pairs to toggle a zone that door_set/room_action_set already
// built -
// handler 0 (no_room_action), or a flags byte whose low three bits miss every
// prober's mask, makes the zone dead.
// ============================================================================
int cmd_room_action_arm(void)
{
    int base = (unsigned int)g_ScdOpcodes[1] * 0xc;
    g_ScdOpcodes += 4;
    g_RoomActionTable[base]     = g_ScdOpcodes[-2];
    g_RoomActionTable[base + 1] = g_ScdOpcodes[-1];
    return 1;
}

// ============================================================================
// 0x14 - cmd_scd_event_create (0x00461040)
// Create a new SCD event from the command stream.
// ============================================================================
int cmd_scd_event_create(void)
{
    // The original reads a 16-bit operand here (g_ScdOpcodes is a ushort* in
    // this function): low byte = slot, high byte = event script index. Reading
    // it a byte at a time made scriptIndex constant 0.
    g_ScdOpcodes += 2;
    unsigned short param = scd_read_u16(0);
    ScdEventEntry_Create(param & 0xFF, param >> 8);
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x15 - cmd_bgm_play (0x00460a80)
// Start a BGM sound track.
// ============================================================================
int cmd_bgm_play(void)
{
    // Original: EAX = (val & 0xffffff1f) >> 5, i.e. (val >> 8) * 8 - a BYTE offset
    // into the 8-byte g_SndBank records, so the channel index is just val >> 8.
    // Transcribing the mask literally as (val & 0x1f) >> 5 is a constant 0, and
    // the old int[64] view had no way to reach the slot byte at record +5.
    unsigned short val = scd_read_u16(0);
    unsigned int ch = val >> 8;
    g_ScdOpcodes += 2;
    if (g_SndBank[ch].handle != 0) {
        SetSndSlot(g_SndBank[ch].handle, (int)g_SndBank[ch].slot);
    }
    g_BGM_STATE |= 1 << ((char)ch + 3);
    return 1;
}

// ============================================================================
// 0x16 - cmd_bgm_stop (0x00460c70)
// Stop and reset volume for a sound bank.
// ============================================================================
int cmd_bgm_stop(void)
{
    // Same (val >> 8) channel index as opcode 0x15; see the note there.
    unsigned short val = scd_read_u16(0);
    unsigned int ch = val >> 8;
    g_ScdOpcodes += 2;
    unsigned int bit = 1 << ((char)ch + 3);
    if ((g_BGM_STATE & bit) != 0) {
        if (g_SndBank[ch].handle != 0) {
            setSndStop(g_SndBank[ch].handle);
        }
        g_BGM_STATE &= ~bit;
        set_volume(g_SndBank[ch].handle, -1);
    }
    return 1;
}

// ============================================================================
// 0x17 - cmd_sfx_3d_play (0x00460d80)
// Play a 3D sound effect at various positions.
// ============================================================================
int cmd_sfx_3d_play(void)
{
    unsigned short op1 = scd_read_u16(0);
    unsigned short op2 = scd_read_u16(2);
    unsigned short op3 = scd_read_u16(4);
    g_ScdOpcodes += 6;

    unsigned char sndType = (unsigned char)(op1 >> 8);
    unsigned char sndId = (unsigned char)op2;
    char vol = (char)(op2 >> 8);
    unsigned char posType = (unsigned char)op3;

    // Positional sources are the 3-int scaMatrixData.localMatrix.t (entity +0x34),
    // NOT the packed SVECTOR `position` (+0x6C) - Play3DSnd reads three ints.
    switch (posType) {
    case 0:
        // Original writes the global scratch VECTOR at 0x00be11b0 as three ints
        // and passes its address; it does not build a local SVECTOR of shorts.
        g_playerPosScratch.x = (int)scd_read_s16(0);
        g_playerPosScratch.y = 0;
        g_playerPosScratch.z = (int)scd_read_s16(2);
        g_ScdOpcodes += 4;
        Play3DSnd(sndType, sndId, (int)vol, (unsigned int)&g_playerPosScratch);
        break;
    case 1:
        g_ScdOpcodes += 4;
        Play3DSnd(sndType, sndId, (int)vol,
            (unsigned int)g_playerEntity.scaMatrixData.localMatrix.t);
        break;
    case 2:
        g_ScdOpcodes += 4;
        Play3DSnd(sndType, sndId, (int)vol,
            (unsigned int)g_EnemiesList[op3 >> 8].scaMatrixData.localMatrix.t);
        break;
    case 3:
        // Original pushes a third (unused) argument; play_sfx reads only two.
        g_ScdOpcodes += 4;
        play_sfx(sndType, sndType);
        break;
    }
    return 1;
}

// ============================================================================
// 0x18 - cmd_item_model_set (0x00461220)
// Set up one of the room's item models (the pick-up you see lying in the room,
// or the one a desk close-up reveals) in g_item_model_table.
// ============================================================================
int cmd_item_model_set(void)
{
    dbg_printf("ITEM MODEL START %s\n", "imodel_set");

    // skip ink ribbon model set if jill's first playthrough
    if ((char)g_ScdOpcodes[10] == ITEM_INK_RIBBONS && (g_playerEntity.id & 3) == 1) {
        if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            FUN_00473f10((int*)&g_roomItemsFlags, g_ScdOpcodes[0x16]);
            g_ScdOpcodes += 0x1a;
            return 1;
        }
    }

    unsigned int slotIdx = (unsigned int)(g_ScdOpcodes[1] & 0x7f);
    int tableOffset = slotIdx * 0xc;
    unsigned char* entry = &g_RoomActionTable[tableOffset];
    unsigned char itemType = g_ScdOpcodes[10];

    // Determine entry visibility based on flag check and item type. entry[0] is
    // the room_check_actions index, so the id range picks the pickup handler:
    // a map (ITEM_MAP_FIRST..ITEM_MAP_LAST) gets 0x0F = pickup_key_event, which
    // raises the map's ROOM_FLAG_MAP_BASE bit; ordinary items get 4 and
    // documents (> ITEM_MAP_LAST) get 0xD. A clear roomItems flag zeroes it,
    // i.e. the item is already taken and the entry is inactive.
    unsigned char visFlag;
    if (itemType <= ITEM_MAP_LAST) {
        if (itemType < ITEM_MAP_FIRST) {
            visFlag = (Flg_ck((int)&g_roomItemsFlags, g_ScdOpcodes[0x16]) == 0) - 1;
            visFlag &= 4;
        } else {
            visFlag = (Flg_ck((int)&g_roomItemsFlags, g_ScdOpcodes[0x16]) == 0) - 1;
            visFlag &= 0xf;
        }
    } else {
        visFlag = (Flg_ck((int)&g_roomItemsFlags, g_ScdOpcodes[0x16]) == 0) - 1;
        visFlag &= 0xd;
    }
    entry[0] = visFlag;

    // The original READS the word at +0x18, then WRITES BACK `word & 1` into the
    // opcode stream, and stores that masked value in entry+2. Everything after
    // keeps using the pre-mask value (flags18). The write-back and the mask were
    // both missing, so entry+2 received the full 16-bit value.
    unsigned short flags18 = scd_read_u16(0x18);
    *(unsigned short*)(g_ScdOpcodes + 0x18) = flags18 & 1;
    entry[1] = g_ScdOpcodes[0x17];
    *(unsigned short*)(entry + 2) = scd_read_u16(0x18);
    *(unsigned short*)(entry + 4) = (unsigned short)g_ScdOpcodes[0xc];
    *(unsigned short*)(entry + 6) = (unsigned short)g_ScdOpcodes[0x16];
    *(unsigned int*)(entry + 8) = (unsigned int)(g_ScdOpcodes + 2);
    if (entry > (unsigned char*)g_RoomActionTail) {
        g_RoomActionTail = entry;
    }

    char* modelPtr = (char*)g_item_model_table[g_ScdOpcodes[0xc]];
    int* itemModelData = (int*)((char*)g_RdtPointer->item_models + (unsigned int)g_ScdOpcodes[0xc] * 8);

    // spriteInfo is chosen per branch below (it is NOT always modelPtr+0x20).
    MATRIX* spriteInfo = (MATRIX*)(modelPtr + 0x20);

    if (*itemModelData == 0) {
        modelPtr[0x14] = 0; modelPtr[0x15] = 0;
        modelPtr[0x16] = 0; modelPtr[0x17] = 0;
    } else {
        if (((char)g_ItemModelCount == 0 || DAT_00bca0d0[1] != itemModelData[1]) && itemModelData[1] != 0) {
            DAT_008e1c78 = g_TextureBankID;
            DAT_008e1c70 = g_TextureCurrentPage;
            ClearTmdProcessingFlag();
            // The two map items - guardhouse (0x52) and courtyard (0x50) - get
            // their 256-entry 5551 palette darkened: each 5-bit channel drops by
            // 9, clamped at 0, bit 15 kept. This loop was missing entirely.
            if ((char)g_ScdOpcodes[10] == ITEM_MAP_GUARDHOUSE ||
                (char)g_ScdOpcodes[10] == ITEM_MAP_COURTYARD) {
                unsigned short* pal = (unsigned short*)(itemModelData[1] + 0x14);
                for (int n = 0; n < 256; n++) {
                    unsigned short c = *pal;
                    unsigned char r = (unsigned char)(c & 0x1f);
                    unsigned char g = (unsigned char)((c >> 5) & 0x1f);
                    unsigned char b = (unsigned char)((c >> 10) & 0x1f);
                    r = (r < 10) ? 0 : (unsigned char)(r - 9);
                    g = (g < 10) ? 0 : (unsigned char)(g - 9);
                    b = (b < 10) ? 0 : (unsigned char)(b - 9);
                    *pal = (unsigned short)((c & 0x8000) | (b << 10) | (g << 5) | r);
                    pal++;
                }
            }
            ProcessTmdAsync((unsigned int)itemModelData[1]);
        }
        if ((char)g_ItemModelCount == 0 || *itemModelData != *DAT_00bca0d0) {
            ProcessTmdTextures(2, (unsigned int*)(unsigned int)*itemModelData, DAT_008e1c78, DAT_008e1c70);
        }
        FUN_00473ea0(*itemModelData, modelPtr + 0xc, (ScaMatrixData*)(modelPtr + 0x1c));
        if ((char)g_ScdOpcodes[10] == ITEM_CRANK_HEX) {
            FUN_004870d0(*(int*)(modelPtr + 0x18));
        }

        // SCA parent: what the pos operands below are RELATIVE TO. They land in
        // modelPtr+0x34/38/3C, which is localMatrix.t[] (ScaMatrixData at +0x1C,
        // its localMatrix at +0x20, a MATRIX's t[] at +0x14 inside that), so they
        // are this item's own LOCAL translation and the parent supplies the frame
        // it composes against:
        //   0xFF  no parent   -> local == world, pos is absolute room coords (570
        //                        of the 584 uses in the shipped RDTs)
        //   0xFE  the player  -> pos is an offset from the player, item follows
        //   else  omodel      -> pos is an offset in g_omodel_table[value]'s space,
        //                        so the item rides that room object (a desk, a
        //                        drawer, the item box lid). (0,0,0) = exactly at it.
        // Unlike cmd_omodel_set there is no 0x80 split here - every other value is
        // an omodel index. Full write-up in docs/SCD_COMMAND_OPCODES.md (0x18).
        unsigned char parentType = g_ScdOpcodes[0xd];
        if (parentType == 0xff) {
            modelPtr[100] = 0; modelPtr[0x65] = 0;
            modelPtr[0x66] = 0; modelPtr[0x67] = 0;
            spriteInfo = (MATRIX*)(modelPtr + 0x20);
        } else if (parentType == 0xfe) {
            *(int*)(modelPtr + 100) = (int)&g_playerEntity + 0x1c;
            spriteInfo = &g_playerEntity.scaMatrixData.localMatrix;
        } else {
            *(int*)(modelPtr + 100) = (int)g_omodel_table[parentType] + 0x1c;
            spriteInfo = (MATRIX*)((int)g_omodel_table[parentType] + 0x20);
        }
        InitScaMatrix(*(int*)(modelPtr + 100), (ScaMatrixData*)(modelPtr + 0x1c));
    }

    modelPtr[0x86] = 0;
    modelPtr[0x87] = 0;

    int flagResult = Flg_ck((int)&g_roomItemsFlags, g_ScdOpcodes[0x16]);
    if (flagResult != 0 && (flags18 & 0x8000) != 0) {
        unsigned int animType = (unsigned int)(flags18 & 0xf00);
        unsigned char effectId;
        if (animType < 0x101) {
            effectId = (animType == 0x100) ? 0x0b : ((flags18 & 0xf00) == 0 ? 0x03 : 0x14);
        } else if (animType < 0x301) {
            effectId = (animType == 0x300) ? 0x1b : (animType == 0x200 ? 0x13 : 0x14);
        } else if (animType < 0x501) {
            effectId = (animType == 0x500) ? 0x0c : (animType == 0x400 ? 0x04 : 0x14);
        } else {
            effectId = (animType == 0x600) ? 0x14 : (animType == 0x700 ? 0x1c : 0x14);
        }

        // The 0xFF test mirrors the SCA parent branch above, because spriteInfo
        // differs between the two: unparented, it is the item's OWN matrix, so
        // (0, heightBias, 0) already sits on the item; parented, it is the PARENT's
        // matrix, so the pos operands have to be repeated to bring the sparkle back
        // onto the item. Both paths land in the same world position.
        //
        // The original writes the global scratch VECTOR at 0x00be11b0 as three ints
        // and passes its address - not a local SVECTOR of shorts.
        if ((char)g_ScdOpcodes[0xd] == -1) {
            g_playerPosScratch.x = 0;
            g_playerPosScratch.z = 0;
            g_playerPosScratch.y = (int)(flags18 & 0xf0) * -2;
        } else {
            g_playerPosScratch.x = (int)scd_read_s16(0xe);
            g_playerPosScratch.y = (int)scd_read_s16(0x10) + (int)(flags18 & 0xf0) * -2;
            g_playerPosScratch.z = (int)scd_read_s16(0x12);
        }
        // spriteInfo is the one selected by the parent-type branch above.
        unsigned char effResult = Effect_CreateBillboard(0x0b, effectId, 0, spriteInfo, &g_playerPosScratch, 0);
        *(short*)(modelPtr + 0x86) = (short)(char)effResult;
    }

    // These two writes target the item model's byte 0 (`*pcVar5` in the original),
    // not the room action entry - the entry's byte 0 was already set from
    // visFlag further up.
    flagResult = Flg_ck((int)&g_roomItemsFlags, g_ScdOpcodes[0x16]);
    modelPtr[0] = (char)(1 - (flagResult == 0));

    if ((char)g_ScdOpcodes[10] == ITEM_INK_RIBBONS && (g_playerEntity.id & 3) == CHAR_JILL) {
        if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
            modelPtr[0] = 0;
        }
    }

    modelPtr[1] = (char)g_ItemModelCount;
    modelPtr[0x72] = 0; modelPtr[0x73] = 0;
    *(unsigned short*)(modelPtr + 0x74) = scd_read_u16(0x14);
    modelPtr[0x76] = 0; modelPtr[0x77] = 0;
    modelPtr[0xc] = 0; modelPtr[0xd] = 0;
    modelPtr[0xe] = 0; modelPtr[0xf] = 0x40;

    if (g_ScdOpcodes[1] & 0x80) {
        modelPtr[0xc] = 0x40; modelPtr[0xd] = 0;
        modelPtr[0xe] = 0;   modelPtr[0xf] = 0x40;
    }

    *(int*)(modelPtr + 0x34) = (int)scd_read_s16(0xe);
    *(int*)(modelPtr + 0x38) = (int)scd_read_s16(0x10);
    *(int*)(modelPtr + 0x3c) = (int)scd_read_s16(0x12);

    if (g_stageId == STAGE_MANSION_RETURN_2F && g_roomId == ROOM_TROPHY_ROOM && (modelPtr[1] & 0x3f) == 1) {
        *(int*)(modelPtr + 0x3c) = scd_read_s16(0x12) - 0x96;
    }

    *(char*)&g_ItemModelCount = (char)g_ItemModelCount + 1;
    g_ScdOpcodes += 0x1a;
    DAT_00bca0d0 = itemModelData;
    dbg_printf("ITEM MODEL END %s\n", "imodel_set");
    return 1;
}

// ============================================================================
// 0x19 - cmd_model_flag_set (0x00460f50)
// Set a byte on an interactable room model record (g_item_model_table).
// ============================================================================
int cmd_model_flag_set(void)
{
    unsigned char modelIdx = g_ScdOpcodes[1];
    unsigned char value = g_ScdOpcodes[2];
    g_ScdOpcodes += 4;
    *(unsigned char*)g_item_model_table[modelIdx] = value;
    return 1;
}

// ============================================================================
// 0x1A - cmd_item_search (0x00460f80)
// Search player inventory for an item. Returns 1 if found.
// ============================================================================
int cmd_item_search(void)
{
    unsigned char searchId = g_ScdOpcodes[1];
    g_ScdOpcodes += 2;
    if (g_TotalHeldItems != 0) {
        unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
        for (unsigned int i = 0; i < g_TotalHeldItems; i++) {
            if (*slots == searchId) return 1;
            slots += 2;
        }
    }
    return 0;
}

// ============================================================================
// 0x1B - cmd_enemy_set (0x004617d0)
// Set up an enemy entity in the room.
// ============================================================================
int cmd_enemy_set(void)
{
    dbg_printf("ENEMY SET START %s\n", "enemy_set");

    if ((char)g_ScdOpcodes[3] != -1) {
        if (Flg_ck((int)g_EnemiesFlags, (unsigned char)g_ScdOpcodes[3]) != 0) {
            g_ScdOpcodes += 0x16;
            dbg_printf("ENEMY SET END %s\n", "enemy_set");
            return 1;
        }
    }

    unsigned char enemySlot = g_ScdOpcodes[0x12] & 0xf;
    ENTITY = &g_EnemiesList[enemySlot];
    g_EnemiesList[enemySlot].scaMatrixData.localMatrix.t[1] = (int)scd_read_s16(0xe);
    // Original target is entity +0x161, i.e. pad_160[1] - not pad_164[0] (+0x164).
    // Low nibble = enemy slot, high nibble = the byte at +0x15, bit 7 set below.
    g_EnemiesList[enemySlot].pad_160[1] = g_ScdOpcodes[0x12] & 0xf;
    g_EnemiesList[enemySlot].pad_160[1] |= (char)g_ScdOpcodes[0x15] << 4;

    if ((char)g_ScdOpcodes[4] != 0) {
        ENTITY->pad_160[1] |= 0x80;
    }

    bool shouldInit = true;
    if ((char)g_ScdOpcodes[4] == 0) {
        if (FUN_0048f330(g_ScdOpcodes[0x12]) != 0) shouldInit = false;
    }

    if (shouldInit) {
        ENTITY->status_flags = 1;
        ENTITY->behavior_flags = g_ScdOpcodes[2];
        *(unsigned short*)&ENTITY->angle = scd_read_u16(8);
        ENTITY->scaMatrixData.localMatrix.t[0] = (unsigned int)scd_read_u16(0xc);
        ENTITY->scaMatrixData.localMatrix.t[2] = (unsigned int)scd_read_u16(0x10);
        ENTITY->position.x = (short)ENTITY->scaMatrixData.localMatrix.t[0];
        ENTITY->position.y = (short)ENTITY->scaMatrixData.localMatrix.t[1];
        ENTITY->position.z = (short)ENTITY->scaMatrixData.localMatrix.t[2];
        ENTITY->animationId = g_ScdOpcodes[0x13];
        ENTITY->animation_frame_id = g_ScdOpcodes[0x14];
        ENTITY->timing_control = 1;
    }

    if (ENTITY->status_flags & 1) {
        ENTITY->state = 0;
        ENTITY->ignore_player_flag = 0;
        ENTITY->action_behavior = 0;
        ENTITY->action_state = 0;
        ENTITY->id = g_ScdOpcodes[1];
        // Original writes entity +0x163 = death_event_id, not pad_167 (+0x167).
        ENTITY->death_event_id = g_ScdOpcodes[3];
        ENTITY->position.pad = scd_read_s16(6);
        *((unsigned short*)&ENTITY->angle + 1) = scd_read_u16(10);
        ENTITY->hit_state = 0;
        *(unsigned short*)&ENTITY->pad_ca[0] = 0;
        ENTITY->collisionFlags = 0;
        // Original clears entity +0xD8 = lookAtFlags, not death_timer (+0xBC).
        ENTITY->lookAtFlags = 0;
        // 0x004619e4: `MOV EAX, ds:0x4d4540` - the VALUE of g_scaDataTable[0]
        // (the Chris SCA record), not the address of the table. Storing the
        // address made every entity whose own init does not override Sca_info
        // (static models like Kenneth's corpse) read its collision radius out
        // of the pointer bytes: radius = high word of a pointer. The original
        // got away with it because its table sits at 0x004d4540, so the high
        // word was 0x004d = 77; the port's addresses are ~0x5675xxxx, so the
        // same read yielded a radius of 22133 and the next corpse-vs-zombie
        // push teleported the zombie ~22000 units across the room.
        ENTITY->Sca_info = g_scaDataTable[0];
        ENTITY->pSca_hit_data = g_scaPoolPtr;
        g_enemy_count++;
        g_scaPoolPtr += (unsigned int)g_ScdOpcodes[5] * 6;
    }

    g_ScdOpcodes += 0x16;
    dbg_printf("ENEMY SET END %s\n", "enemy_set");
    return 1;
}

// ============================================================================
// 0x1C - cmd_room_light_fade_set (0x00462210)
// Set up special room lighting effects.
// ============================================================================
int cmd_room_light_fade_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    g_SpecialRoomLightR = (char)(op1 >> 8);
    g_SpecialRoomLightDelta = scd_read_s16(0);
    g_ScdOpcodes += 2;
    unsigned short flags = scd_read_u16(0);

    g_SpecialB1 = 0;
    g_SpecialG1 = 0;
    g_SpecialR1 = 0;
    if (flags & 1) g_SpecialB1 = 0xff;
    if (flags & 2) g_SpecialG1 = 0xff;
    if (flags & 4) g_SpecialR1 = 0xff;

    g_ScdOpcodes += 2;
    if (g_SpecialRoomLightDelta != 0) {
        g_SpecialRoomLightState = 0;
        if (g_SpecialRoomLightDelta < 1) {
            g_SpecialRoomLightState = 0x7fff;
        }
    }
    return 1;
}

// ============================================================================
// 0x1D - cmd_equipped_item_test (0x00460ee0)
// Compare the item id in the currently equipped inventory slot against a
// constant. Not weapon-specific - every sampled use tests 0, i.e. "nothing
// equipped".
// ============================================================================
int cmd_equipped_item_test(void)
{
    unsigned char testVal = g_ScdOpcodes[1];
    g_ScdOpcodes += 2;
    return ((unsigned char*)g_ItemSlotsPointer)[-2 + (unsigned int)g_EquippedItemId * 2] == testVal;
}

// ============================================================================
// 0x1E - cmd_voice_play (0x00461a80)
// Start or end a cutscene VOICE line - 0x17 is the sound-effect command.
// play_sound_and_voice_effect type 1 loads and plays the line, type 2 ends it,
// resets the mixer and clears the wait flag. Raising MSF_VOICE_PLAYING here is
// what event-VM opcode 0xF7 blocks on, so this is what gates a line advancing.
// ============================================================================
int cmd_voice_play(void)
{
    unsigned short sndId = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short param = scd_read_u16(0);
    g_ScdOpcodes += 2;
    play_sound_and_voice_effect(sndId >> 8, param);
    g_main_state_flags |= MSF_VOICE_PLAYING;
    return 1;
}

// ============================================================================
// 0x1F - cmd_omodel_set (0x00461ac0)
// Set up an object model (furniture, decorations, etc.).
// This is a very complex function with many stage/room-specific overrides.
// ============================================================================
int cmd_omodel_set(void)
{
    dbg_printf("OMODEL SET START %s\n", "omodel_set");

    unsigned int slotIdx = (unsigned int)(g_ScdOpcodes[1] & 0x3f);
    char* objPtr = (char*)g_omodel_table[slotIdx];
    int* modelData = (int*)((char*)g_RdtPointer->object_models + slotIdx * 8);

    // Declared before the goto so the jump cannot cross an initialisation
    // (GCC rejects that; MSVC allowed it). Assigned at their use site below.
    unsigned char parentByte;
    unsigned int parentIdx;

    if (*modelData == 0) {
        *(int*)(objPtr + 0x14) = 0;
        goto setupObject;
    }

    // Model TMD processing
    if (((char)g_omodelCount == 0 || DAT_00bca0d4[1] != modelData[1]) && modelData[1] != 0) {
        DAT_008e1c7c = g_TextureBankID;
        DAT_008e1c74 = g_TextureCurrentPage;

        // Stage 5 room-specific texture bank overrides.
        //
        // The original's control flow here has THREE outcomes, not two - a bank
        // override falls straight past both the palette block and ProcessTmdAsync:
        //   stage5/room4  bank==9        -> override, no palette, NO async
        //   stage5/room4  bank!=9        -> jmp LAB_00461d35: async only
        //   stage5/room6  bank in 7/9/0B -> override, no palette, NO async
        //   stage5/room6  other bank     -> jmp LAB_00461d35: async only
        //   stage5/other room            -> jmp LAB_00461bfe: palette + async
        //   stage != 4                   -> palette + async
        // The port previously nested ProcessTmdAsync inside the `else`, so stage 4
        // never called it at all, and stage-5 rooms other than 4/6 also skipped
        // ClearTmdProcessingFlag.
        bool doPaletteBlock = true;
        bool doProcessAsync = true;

        if (g_stageId == STAGE_LABORATORY) {
            doPaletteBlock = false;
            if (g_roomId == ROOM_VISUAL_DATA_ROOM) {
                if (g_TextureBankID == 9) {
                    g_TextureBankID = 0x0e;
                    g_TextureCurrentPage = 0x13;
                    doProcessAsync = false;
                }
            } else if (g_roomId == ROOM_SMALL_LABORATORY) {
                if (g_TextureBankID == 7) {
                    g_TextureBankID = 9;
                    g_TextureCurrentPage++;
                    doProcessAsync = false;
                } else if (g_TextureBankID == 9) {
                    g_TextureBankID = 0x0b;
                    g_TextureCurrentPage++;
                    doProcessAsync = false;
                } else if (g_TextureBankID == 0x0b) {
                    g_TextureBankID = 0x0d;
                    g_TextureCurrentPage++;
                    doProcessAsync = false;
                }
            } else {
                doPaletteBlock = true;
            }
        }

        if (doPaletteBlock) {
            // Stage 1 room 12: adjust TMD colors
            if (g_stageId == STAGE_MANSION_1F && g_roomId == ROOM_GREENHOUSE && (g_ScdOpcodes[1] & 0x3f) == 0) {
                unsigned short* colorPtr = (unsigned short*)(modelData[1] + 0x14);
                for (int i = 0; i < 256; i++) {
                    unsigned short c = *colorPtr;
                    unsigned short r = (c & 0x1f) + 3;
                    unsigned short g = ((c & 0x3e0) >> 5) + 2;
                    unsigned short b = ((c & 0x7c00) >> 10) + 2;
                    if (r > 0x1f) r = 0x1f;
                    if (g > 0x1f) g = 0x1f;
                    if (b > 0x1f) b = 0x1f;
                    *colorPtr = (g << 5) | (b << 10) | r | (c & 0x8000);
                    colorPtr++;
                }
            }

            if (!(g_stageId == STAGE_MANSION_RETURN_2F && g_roomId == ROOM_LARGE_LIBRARY && (g_ScdOpcodes[1] & 0x3f) == 0)) {
                ClearTmdProcessingFlag();
            }

            // Mansion 2F (stages 2/7) room 11: fix transparent colors.
            // The original zeroes palette entry 0 before the scan; that was missing.
            if ((get_stage_id() + 1) % 5 == 2 && g_roomId == ROOM_FRONT_LESSON_ROOM && (g_ScdOpcodes[1] & 0x3f) == 0) {
                unsigned short* colorPtr = (unsigned short*)(modelData[1] + 0x14);
                *colorPtr = 0;
                for (int i = 0; i < 256; i++) {
                    if ((*colorPtr & 0x7fff) == 0x7fff) *colorPtr = 0x4e73;
                    colorPtr++;
                }
            }
        }

        if (doProcessAsync) {
            ProcessTmdAsync((unsigned int)modelData[1]);
        }
    }

    // TMD texture processing
    if ((char)g_omodelCount == 0 || *DAT_00bca0d4 != *modelData) {
        unsigned int texResult = ProcessTmdTextures(2, (unsigned int*)(unsigned int)*modelData, DAT_008e1c7c, DAT_008e1c74);
        if (g_ScdOpcodes[1] & 0x80) {
            QueueTextureForProcessing((char)DAT_008e1c74,
                (unsigned char)(((unsigned int)texResult & 0xFFFFFF00) | (g_ScdOpcodes[1] & 0xBF)));
        }
        if (g_stageId == STAGE_COURTYARD && g_roomId == ROOM_HELIPORT && (g_ScdOpcodes[1] & 0x3f) < 5) {
            FUN_00473e40(*modelData);
        }
    }

    // Stage-specific adjustments
    if ((get_stage_id() + 1) % 5 == MANSION_2F && g_roomId == ROOM_STUDY_2F && (g_ScdOpcodes[1] & 0x3f) == 1) {
        DAT_004d2be0 = 0x30;
    }
    if (g_stageId == STAGE_MANSION_1F && g_roomId == ROOM_TIGER_STATUE_ROOM && (g_ScdOpcodes[1] & 0x3f) == 1) {
        FUN_00473e40(*modelData);
    }
    if ((get_stage_id() + 1) % 5 == MANSION_2F && g_roomId == ROOM_STUDY_2F && (g_ScdOpcodes[1] & 0x3f) == 0) {
        FUN_00484d90(modelData[1], DAT_008e1c7c, DAT_008e1c74);
        FUN_00484e40(*modelData, DAT_008e1c7c, DAT_008e1c74);
    }

    FUN_00473ea0(*modelData, objPtr + 0xc, (ScaMatrixData*)(objPtr + 0x1c));
    DAT_004d2bdc = 0;
    DAT_004d2be0 = -1;

    // Set up SCA parent reference
    parentByte = g_ScdOpcodes[3];
    parentIdx = (unsigned int)parentByte;
    if (parentIdx == 0xfe) {
        *(int*)(objPtr + 100) = (int)&g_playerEntity + 0x1c;
    } else if (parentIdx == 0xff) {
        *(int*)(objPtr + 100) = 0;
    } else if (parentByte < 0x80) {
        *(int*)(objPtr + 100) = (int)g_omodel_table[parentIdx] + 0x1c;
    } else {
        // Original: (parent & 0x7F) * 0x18C + 0xBE6480, i.e. the scaMatrixData (+0x1C)
        // of g_EnemiesList[parent & 0x7F] - g_EnemiesList is at 0x00BE6464. Must be
        // computed from the symbol: a literal 0xBE6480 does not point at the port's
        // array.
        *(unsigned int*)(objPtr + 100) =
            (unsigned int)&g_EnemiesList[parentByte & 0x7f].scaMatrixData;
    }
    InitScaMatrix(*(int*)(objPtr + 100), (ScaMatrixData*)(objPtr + 0x1c));

setupObject:
    *(int*)(objPtr + 0xc) = 0x40000000;
    if (g_ScdOpcodes[2] & 0x10) {
        *(int*)(objPtr + 0xc) = 0x40000040;
    }

    objPtr[0] = g_ScdOpcodes[2];
    objPtr[1] = g_ScdOpcodes[1] & 0x7f;
    *(unsigned short*)(objPtr + 0x72) = 0;
    unsigned short flags0a = scd_read_u16(0xa);
    *(unsigned short*)(objPtr + 0x7e) = flags0a;
    *(unsigned short*)(objPtr + 0x74) = flags0a;
    *(unsigned short*)(objPtr + 0x76) = 0;

    // Set up entry data pointer
    *(unsigned int*)(objPtr + 4) = (unsigned int)(objPtr + 0x88);
    *(unsigned short*)(objPtr + 0x88) = 0x8000;
    *(unsigned short*)(objPtr + 0x8a) = scd_read_u16(0x18);
    *(unsigned short*)(objPtr + 0x8c) = scd_read_u16(0x16);
    *(unsigned short*)(objPtr + 0x8e) = scd_read_u16(0x1a);
    *(unsigned short*)(objPtr + 0x90) = scd_read_u16(0x16);
    *(unsigned short*)(objPtr + 0x92) = scd_read_u16(0x14);
    *(unsigned short*)(objPtr + 0x94) = scd_read_u16(0xc);
    *(unsigned short*)(objPtr + 0x98) = scd_read_u16(0xe);
    *(unsigned short*)(objPtr + 0x9c) = scd_read_u16(0x10);
    *(unsigned short*)(objPtr + 0xa0) = scd_read_u16(0x12);

    // Set position
    short posX = scd_read_s16(4);
    *(short*)(objPtr + 0x6c) = posX;
    *(int*)(objPtr + 0x34) = (int)posX;
    short posY = scd_read_s16(6);
    *(short*)(objPtr + 0x6e) = posY;
    *(int*)(objPtr + 0x38) = (int)posY;
    short posZ = scd_read_s16(8);
    *(short*)(objPtr + 0x70) = posZ;
    *(int*)(objPtr + 0x3c) = (int)posZ;

    // Stage/room-specific position adjustments
    if (g_stageId == STAGE_MANSION_1F && g_roomId == ROOM_DINING_ROOM && g_playerEntity.id == 1 && (g_ScdOpcodes[1] & 0x3f) == 1) {
        *(short*)(objPtr + 0x6e) = -5;
        *(int*)(objPtr + 0x38) = -5;
    }
    if ((get_stage_id() + 1) % 5 == MANSION_2F && g_roomId == ROOM_FRONT_LESSON_ROOM) {
        if ((g_ScdOpcodes[1] & 0x3f) == 0) {
            short adjZ = scd_read_s16(8) + 10;
            *(short*)(objPtr + 0x70) = adjZ;
            *(int*)(objPtr + 0x3c) = (int)adjZ;
        }
        if ((g_ScdOpcodes[1] & 0x3f) == 1) {
            short adjZ = scd_read_s16(8) - 0x28;
            *(short*)(objPtr + 0x70) = adjZ;
            *(int*)(objPtr + 0x3c) = (int)adjZ;
        }
    }
    if (g_stageId == STAGE_GUARDHOUSE) {
        if (g_roomId == ROOM_GUARDHOUSE_SAVE_ROOM && (g_ScdOpcodes[1] & 0x3f) == 0) {
            *(short*)(objPtr + 0x6c) = scd_read_s16(4) + 0x1e;
            *(int*)(objPtr + 0x34) = (int)*(short*)(objPtr + 0x6c);
            *(short*)(objPtr + 0x6e) = scd_read_s16(6) - 10;
            *(int*)(objPtr + 0x38) = (int)*(short*)(objPtr + 0x6e);
            *(short*)(objPtr + 0x70) = scd_read_s16(8) + 0x82;
            *(int*)(objPtr + 0x3c) = (int)*(short*)(objPtr + 0x70);
        }
        if (g_roomId == ROOM_SECURITY_ROOM && (g_ScdOpcodes[1] & 0x3f) == 0) {
            *(short*)(objPtr + 0x6e) = scd_read_s16(6) - 0x15e;
            *(int*)(objPtr + 0x38) = (int)*(short*)(objPtr + 0x6e);
        }
    }

    *(char*)&g_omodelCount = (char)g_omodelCount + 1;
    g_ScdOpcodes += 0x1c;
    DAT_00bca0d4 = modelData;
    dbg_printf("OMODEL SET END %s\n", "omodel_set");
    return 1;
}

// ============================================================================
// 0x20 - cmd_player_pos_set (0x00430f60)
// Set the player's position, rotation, and speed from SCD data.
// ============================================================================
int cmd_player_pos_set(void)
{
    // Original: AND word ptr [player+0xE0],0xfff3 - clears bits 2 AND 3.
    g_playerEntity.unk_e0 &= 0xFFF3;
    g_playerEntity.position.pad = scd_read_s16(2);
    g_playerEntity.directionAngle = scd_read_s16(4);
    g_playerEntity.speed.x = scd_read_s16(6);
    g_playerEntity.position.x = scd_read_s16(8);
    g_playerEntity.scaMatrixData.localMatrix.t[0] = (int)scd_read_s16(8);
    g_playerEntity.position.y = scd_read_s16(10);
    g_playerEntity.scaMatrixData.localMatrix.t[1] = (int)scd_read_s16(10);
    short posZ = scd_read_s16(0xc);
    g_ScdOpcodes += 0xe;
    g_playerEntity.position.z = posZ;
    g_playerEntity.scaMatrixData.localMatrix.t[2] = (int)posZ;
    return 1;
}

// ============================================================================
// 0x21 - cmd_enemy_pos_set (0x00430fe0)
// Set an enemy's position, rotation from SCD data.
// ============================================================================
int cmd_enemy_pos_set(void)
{
    // 14-byte instruction (original advances 0xE, not 7), same operand layout as
    // cmd_player_pos_set (0x20):
    //   +0  [op, enemyIdx]   +2  position.pad
    //   +4  angle low half   +6  angle high half
    //   +8  position.x -> t[0]   +10 position.y -> t[1]   +12 position.z -> t[2]
    // enemyIdx comes from an arithmetic shift of the signed word (SAR AX,0x8).
    int enemyIdx = scd_read_s16(0) >> 8;
    Entity* ent = &g_EnemiesList[enemyIdx];

    // Original: AND word ptr [ent+0xE0],0xfff3 - that is scd_entity_flags, and it
    // clears bits 2 AND 3. The old code masked ~8 into pad_ec[0x70], which
    // resolves to entity offset 0x15C - a completely different field.
    ent->scd_entity_flags &= 0xFFF3;

    ent->position.pad = scd_read_s16(2);
    *(short*)&ent->angle = scd_read_s16(4);
    *((short*)&ent->angle + 1) = scd_read_s16(6);
    short posX = scd_read_s16(8);
    ent->position.x = posX;
    ent->scaMatrixData.localMatrix.t[0] = (int)posX;
    short posY = scd_read_s16(10);
    ent->position.y = posY;
    ent->scaMatrixData.localMatrix.t[1] = (int)posY;
    short posZ = scd_read_s16(12);
    ent->position.z = posZ;
    ent->scaMatrixData.localMatrix.t[2] = (int)posZ;

    g_ScdOpcodes += 14;
    return 1;
}

// ============================================================================
// 0x22 - cmd_item_count_test (0x00431100)
// Sum the quantities of an item GROUP held in the inventory and compare the
// total against a value. searchId selects the group (0x0A = any, 0x0B = item 2,
// 0x0C = item 3, 0x0D = items 4/5, 0x0F = item 6, 0x10-0x12 = items 7/8/9);
// op2 low byte selects the comparison. Returns 0 when the group is not held.
// ============================================================================
int cmd_item_count_test(void)
{
    unsigned short op1 = scd_read_u16(0);
    int count = 0;
    g_ScdOpcodes += 4;
    unsigned int searchId = (unsigned int)(op1 >> 8);
    unsigned int totalVal = 0;

    if (g_TotalHeldItems != 0) {
        unsigned char* slots = (unsigned char*)g_ItemSlotsPointer;
        for (unsigned int i = 0; i < (unsigned int)g_TotalHeldItems; i++) {
            unsigned char itemId = slots[0];
            if (searchId == itemId || (unsigned char)(itemId - searchId) == (unsigned char)-0xb) {
                totalVal = (unsigned int)slots[1];
            }
            switch (searchId) {
            case 10:
                totalVal = (unsigned int)slots[1];
                count++;
                break;
            case 0xb:
                if (itemId == 2) { totalVal += slots[1]; count++; }
                break;
            case 0xc:
                if (itemId == 3) { totalVal += slots[1]; count++; }
                break;
            case 0xd:
                if (itemId == 5 || itemId == 4) { totalVal += slots[1]; count++; }
                break;
            case 0xf:
                if (itemId == 6) { totalVal += slots[1]; count++; }
                break;
            case 0x10: case 0x11: case 0x12:
                if (itemId == 7 || itemId == 8 || itemId == 9) { totalVal += slots[1]; count++; }
                break;
            }
            slots += 2;
        }
    }

    unsigned short op2 = scd_read_u16(-2);
    unsigned int compareVal = (unsigned int)(op2 >> 8);
    if (count == 0) return 0;

    switch ((unsigned char)op2) {
    case 0: return totalVal == compareVal;
    case 1: return compareVal < totalVal;
    case 2: return compareVal <= totalVal;
    case 3: return totalVal < compareVal;
    case 4: return totalVal <= compareVal;
    case 5: return totalVal != compareVal;
    default: return 0;
    }
}

// ============================================================================
// 0x23 - cmd_cut_lock_write (0x00431280)
// WRITE the camera lock from the operand: 0 clears MSF_CAMERA_LOCK, anything
// else sets it. It is not a toggle - the sampled scripts pass a literal 1
// (16 sites) or 0 (14 sites), never a toggle request.
// ============================================================================
int cmd_cut_lock_write(void)
{
    if ((char)g_ScdOpcodes[1] == 0) {
        g_main_state_flags &= ~MSF_CAMERA_LOCK;
    } else {
        g_main_state_flags |= MSF_CAMERA_LOCK;
    }
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x24 - cmd_room_action (0x004312b0)
// Execute a room action callback from the room_check_actions table.
// Also invoked directly by cmd_got_item (opcode 0x2D), which is why this keeps
// the original symbol name rather than an _impl suffix.
// ============================================================================
int cmd_room_action(void)
{
    unsigned char slotIdx = g_ScdOpcodes[1];
    unsigned char actionIdx = g_ScdOpcodes[2];
    g_ScdOpcodes += 4;
    typedef void (*RoomActionFunc)(void*);
    // The original indexes room_check_actions (0x004b9340) unguarded. Guard it
    // here until all 18 handlers are decompiled: an unimplemented entry is a
    // no-op instead of a jump through a null/out-of-range slot.
    if (actionIdx >= ROOM_CHECK_ACTION_COUNT || room_check_actions[actionIdx] == nullptr) {
        return 1;
    }
    ((RoomActionFunc)room_check_actions[actionIdx])(
        &g_RoomActionTable[(unsigned int)slotIdx * 0xc]);
    return 1;
}

// ============================================================================
// 0x25 - cmd_room_sprite_set (0x004621d0)
// Enable or disable a room sprite by ID (SCD room object visibility command).
// If high byte of opcode word is 0, enables the sprite; otherwise disables it.
// ============================================================================
int cmd_room_sprite_set(void)
{
    g_ScdOpcodes += 2;
    // The original reads a WORD here and tests its high byte. On a byte pointer
    // `*g_ScdOpcodes & 0xff00` is always 0, so this always took the Active branch.
    unsigned short v = scd_read_u16(0);
    if ((v & 0xFF00) == 0) {
        RoomSpr_SetActive((char)(v & 0xFF));
    } else {
        RoomSpr_SetInactive((char)(v & 0xFF));
    }
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x26 - cmd_dead_slot_hang_26 (0x00460ce0)
// DEAD TABLE SLOT - hangs the interpreter, faithfully.
//
// The original is a bare RET that never touches EAX. The dispatcher leaves the
// opcode value there (XOR EAX,EAX / MOV AL,[ECX] / CALL [EAX*4+table]), so this
// returns 0x26 - non-zero - while consuming no opcode bytes. run_command_functions
// loops `do { r = f(); } while (r != 0)`, so the original spins forever on this
// opcode. It is therefore never emitted by any shipped script.
//
// Returning the opcode byte reproduces that exactly. Do NOT "fix" this to return
// 0: that would silently diverge from the original by ending the block instead.
// ============================================================================
int cmd_dead_slot_hang_26(void)
{
    return *g_ScdOpcodes;
}

// ============================================================================
// 0x27 - cmd_snd_fade_set (0x00460cf0)
// Set up sound fade.
// ============================================================================
int cmd_snd_fade_set(void)
{
    BuildSndFadeTbl((char)(scd_read_u16(0) >> 8), 0x7f);
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x28 - cmd_enemy_prop_set (0x004312f0)
// Modify enemy entity properties (behavior, state, flags, etc.).
// ============================================================================
int cmd_enemy_prop_set(void)
{
    unsigned short op1 = scd_read_u16(2);
    unsigned int enemyIdx = op1 & 0xff;
    Entity* ent = &g_EnemiesList[enemyIdx];
    unsigned char subCmd = (unsigned char)(op1 >> 8);
    unsigned short param1 = scd_read_u16(4);

    switch (subCmd) {
    case 0:
        ent->behavior_flags = (unsigned char)param1;
        g_ScdOpcodes += 6;
        return 1;
    case 1:
        ent->state = 2;
        ent->ignore_player_flag = 0;
        ent->action_behavior = 0;
        ent->action_state = 0;
        ent->health = param1;
        ent->hit_state = g_ScdOpcodes[6];
        g_ScdOpcodes += 8;
        return 1;
    case 2:
        ent->action_behavior = (unsigned char)param1;
        ent->action_state = 0;
        g_ScdOpcodes += 6;
        return 1;
    case 3: {
        char mode = (char)(param1 >> 8);
        unsigned char flagVal = (unsigned char)param1;
        if (mode == 0) ent->status_flags = flagVal;
        else if (mode == 1) ent->status_flags |= flagVal;
        else if (mode == 2) ent->status_flags ^= flagVal;
        g_ScdOpcodes += 6;
        return 1;
    }
    case 5:
        *(unsigned short*)&ent->angle = param1;
        g_ScdOpcodes += 6;
        return 1;
    case 6:
        ent->blend_counter = 0;
        g_ScdOpcodes += 4;
        return 1;
    case 8:
        ent->state = 9;
        ent->ignore_player_flag = 0;
        ent->action_behavior = 0;
        ent->action_state = 0;
        g_ScdOpcodes += 4;
        return 1;
    case 9: {
        JointStruct* joints = ent->jointsStructs;
        for (unsigned short bits = param1; bits != 0; bits >>= 1) {
            joints->flags ^= (unsigned char)bits & 1;
            joints++;
        }
        g_ScdOpcodes += 6;
        return 1;
    }
    case 10:
        ent->action_state = (unsigned char)param1;
        g_ScdOpcodes += 6;
        return 1;
    default:
        return 1;
    }
}

// ============================================================================
// 0x29 - cmd_fmv_set (0x00461a40)
// Set up FMV (full motion video) playback.
// ============================================================================
int cmd_fmv_set(void)
{
    g_main_state_flags |= MSF_FMV_REQUEST;
    *(unsigned char*)&g_selectedFmvId = (unsigned char)(scd_read_u16(0) >> 8);
    g_fmvDataPointer = g_loadDataDestPointer;
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x2A - cmd_effect_spawn (0x004316c0)
// Spawn a billboard effect at a position.
// ============================================================================
int cmd_effect_spawn(void)
{
    unsigned short typeParam = scd_read_u16(0);
    unsigned short parentParam = scd_read_u16(2);
    // ONE VECTOR, not three separate ints.
    //
    // Effect_CreateBillboard casts this argument to VECTOR* and reads x/y/z (and
    // the pad) at +0/+4/+8/+12. The original's three locals (`local_10`,
    // `local_c`, `local_8` at 0x004316c0) are ADJACENT stack dwords, so passing
    // &local_10 is a valid VECTOR. Three separate `int` locals here are not
    // guaranteed adjacent - and under /RTC MSVC inserts guard bytes between
    // them, so `vPos->y` and `vPos->z` read the uninitialised-stack fill
    // instead of the operands. That is exactly what ROOM1000 showed:
    //   world=(4420,-13108,7)   with -13108 == (short)0xCCCC
    // X was correct and Y/Z were garbage, which put the effect outside every
    // camera switch zone and got it culled before it could ever draw. The
    // effects that always worked are the ones passing the global
    // g_playerPosScratch rather than a local.
    // Same trap as the packed screen-coordinate pair in
    // EffectActor_UpdateAndRender - see the note there.
    VECTOR spawnPos;
    spawnPos.x = (int)scd_read_s16(4);
    spawnPos.y = (int)scd_read_s16(6);
    spawnPos.z = (int)scd_read_s16(8);
    spawnPos.pad = 0;   // the original leaves this stack slot uninitialised;
                        // it only ends up in the unread Effect::spawnPosW
    unsigned short effectFlags = scd_read_u16(10);
    g_ScdOpcodes += 12;

    MATRIX* spriteInfo;
    unsigned int parentType = (unsigned int)(parentParam >> 8);
    if (parentType == 0) {
        spriteInfo = &g_identityMatrixData;
    } else if (parentType == 1) {
        spriteInfo = &g_playerEntity.scaMatrixData.localMatrix;
    } else if ((parentParam & 0x8000) == 0) {
        // parentType >= 2 means ENEMY parentType - 2, and the space is that enemy's
        // localMatrix (ENTITY+0x20) - NOT the effect pool.
        //
        // Ghidra renders this branch as
        //   `g_effectPool[parentType * 3 + 0x3D].pAnimHeader + 0x10`
        // because it picked the wrong containing symbol for a base that is BELOW the
        // array it belongs to. The assembly at 0x00431729 is
        //   LEA EDI,[EAX + EAX*4]      ; parentType * 5
        //   LEA EDI,[EDI + EDI*8]      ; * 9   -> * 45
        //   SUB EDI,EAX                ; -> * 44
        //   LEA EAX,[EDI + EDI*8 + 0xbe616c]   ; * 9 -> parentType * 0x18C + 0xbe616c
        // 0x18C is sizeof(Entity) and 0xbe616c == &g_EnemiesList[-2] + 0x20
        // (0x00be6464 - 2 * 0x18C + 0x20), so the address is exactly
        // g_EnemiesList[parentType - 2].scaMatrixData.localMatrix. It only LOOKS like
        // a pool overrun because 3 effect slots (3 * 0x84) happen to equal one Entity
        // and the folded base lands inside g_effectPool. The sibling branches confirm
        // the reading: parentType 1 loads 0xbe6304 == g_playerEntity + 0x20.
        //
        // Taking the decompile literally read a MATRIX out of whatever the port's own
        // .bss ordering put at that offset (the port gives g_playerPosX & co. separate
        // storage the original overlaid on g_playerEntity, so the adjacency the
        // folded base relies on does not survive - see the .bss-adjacency note in
        // docs/MEMORY_LAYOUT.md). Effect_UpdateActor memcpy's 0x20 bytes from here
        // into eff->transform, so a garbage matrix placed the billboard outside every
        // camera zone and it was culled before drawing: ROOM7060's hunter-kills-
        // Rebecca cutscene spawns its first-slash blood with parentType 3 (enemy 1 =
        // Rebecca) and no splash ever appeared, while the decapitation blood - spawned
        // from Hunter.cpp with a real joint matrix - looked fine.
        spriteInfo = (MATRIX*)((char*)g_EnemiesList +
                               (int)(parentType - 2) * sizeof(Entity) + 0x20);
    } else {
        spriteInfo = (MATRIX*)((int)g_omodel_table[(parentParam & 0x7f00) >> 8] + 0x20);
    }

    Effect_CreateBillboard(
        (unsigned char)(typeParam >> 8),
        (unsigned char)parentParam,
        effectFlags,
        spriteInfo,
        &spawnPos,
        0);
    return 1;
}

// ============================================================================
// 0x2B - cmd_attack_anim_set (0x00431990)
// Set player animation state for scripted actions.
// ============================================================================
int cmd_attack_anim_set(void)
{
    short val = scd_read_s16(0);
    unsigned short animParam = scd_read_u16(2);
    g_ScdOpcodes += 4;

    unsigned short adj = (unsigned short)(val + 0x200) & 0xff00;
    g_playerEntity.attackAnim = (unsigned char)animParam;
    g_playerEntity.action_behavior = (unsigned char)adj;
    g_playerEntity.action_state = (unsigned char)(adj >> 8);
    g_playerEntity.animation_frame_id = (unsigned char)(animParam >> 8);
    g_playerEntity.unk_bf = 0;
    g_playerEntity.unk_8c = 0;
    g_playerEntity.animationId = 8;
    return 1;
}

// ============================================================================
// 0x2C - cmd_item_remove (0x004319e0)
// Remove an item from the player's inventory.
// ============================================================================
int cmd_item_remove(void)
{
    unsigned char itemId = g_ScdOpcodes[1];
    g_ScdOpcodes += 2;
    int slot = get_item_slot(itemId);
    if (slot >= 0) {
        ((unsigned char*)g_ItemSlotsPointer)[slot * 2] = 0;
        rearrange_item_slots();
        return 1;
    }
    return 0;
}

// ============================================================================
// 0x2D - cmd_got_item (0x00431a20)
// Trigger the "got item" room action and menu.
// ============================================================================
int cmd_got_item(void)
{
    cmd_room_action();
    g_main_state_flags |= MSF_MENU_MODE_GOT_ITEM;
    g_main_state_flags ^= MSF_MENU_MODE_ITEM_VIEW;
    return 0;
}

// ============================================================================
// 0x2E - cmd_dead_slot_hang_2e (0x00460a70)
// DEAD TABLE SLOT - bare RET, same as cmd_dead_slot_hang_26 (0x26). See the note there.
// ============================================================================
int cmd_dead_slot_hang_2e(void)
{
    return *g_ScdOpcodes;
}

// ============================================================================
// 0x2F - cmd_snd_pan_vol_set (0x00460c00)
// Set the pan and volume of one sound channel and mirror the pair into
// g_SndPanVol[ch].
// ============================================================================
int cmd_snd_pan_vol_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    // Original: EAX = (op1 & 0xffffff1f) >> 5 = (op1 >> 8) * 8, a byte offset into
    // the 8-byte g_SndPanVol records - so the channel index is op1 >> 8. The old
    // (op1 & 0x1f) >> 5 was a constant 0, and the two overlapping DAT_00ac98e0 /
    // DAT_00ac98e4 arrays could not represent the interleaved pan/volume pair.
    unsigned int ch = op1 >> 8;
    FUN_004805d0((short)(unsigned char)DAT_00bf07ef, ch, op2 & 0xff, (unsigned int)(op2 >> 8));
    g_SndPanVol[ch].pan    = op2 & 0xff;
    g_SndPanVol[ch].volume = (unsigned int)(op2 >> 8);
    return 1;
}

// ============================================================================
// 0x30 - cmd_boundary_set (0x00431a40)
// Modify collision boundary data.
// ============================================================================
int cmd_boundary_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;

    unsigned short* boundary = (unsigned short*)(
        (op2 & 0xff) * 0xc +
        *(int*)(((op1 >> 6) & 0xfffffffc) + 4 + (unsigned int)g_RdtPointer->boundaries));

    if ((op2 & 0xff00) != 0) {
        unsigned short flags = boundary[5];
        boundary[5] = (op2 & 0xf00) | (flags & 0xf0ff);
    }

    boundary[2] = scd_read_u16(0); g_ScdOpcodes += 2;
    boundary[3] = scd_read_u16(0); g_ScdOpcodes += 2;
    boundary[0] = scd_read_u16(0); g_ScdOpcodes += 2;
    boundary[1] = scd_read_u16(0); g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x31 - cmd_state_word_set (0x004608d0)
// Write one SHORT of the BioCard state block; same index space as 0x07. Known
// users: index 4 (the lab countdown, ROOM60A0) and indices 1 and 2 in ROOM4110,
// which drives the flashing red emergency light by writing specialRoomLightState
// and specialRoomLightDelta - the special-room-light system opcode 0x1C arms.
// Note ROOM4110's init desyncs in the disassembler, so a script survey alone
// will not find that second user.
// ============================================================================
int cmd_state_word_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    unsigned short value = scd_read_u16(2);
    g_ScdOpcodes += 4;
    // Original: MOV word ptr [ECX + 0xbe9834],AX with ECX = (op1 >> 7) & ~1.
    // That is a BYTE offset ((op1 >> 8) * 2), not an element index - indexing a
    // short* with it doubled the offset and wrote into the wrong field.
    //
    // The index space is confirmed against the PS1 build, which does the same
    // thing as an element index into a ushort array: cmd_state_word_set
    // (SLUS_001.70 0x8004553c) is `(&g_fadeOutCounter)[op1 >> 8] = value`, and
    // the three globals sit at 0x800c8674 / 0x800c8676 / 0x800c8678 - i.e.
    // index 1 is specialRoomLightState and index 2 is specialRoomLightDelta,
    // exactly as here.
    const unsigned int byteOffset = ((unsigned int)op1 >> 7) & 0xFFFFFFFEu;

    // PORT-ONLY, Director's Cut. The arrange wardrobe's outfit-change script
    // (Stage8/ROOM81C0 event 1) ramps the special-room-light overlay to full
    // black over 15 frames, freezes it with `state_word_set index 2 = 0`
    // (delta), and then one frame later writes `index 1 = 0` (state) - which
    // drops the overlay and un-blanks the screen.
    //
    // That is correct on a PlayStation: the console is blocked on the CD for
    // the whole "Just a moment please." interlude (~300 frames of clothing
    // SFX), so it keeps showing the black frame it last presented and the
    // write is just post-load bookkeeping. This port loads instantly and keeps
    // rendering, so the room came back with its 20 masks under the message -
    // the grey screen with overlays showing through.
    //
    // Nothing in either original suppresses that render, so hold the blank
    // instead: ignore the write that would clear a fade the script has already
    // driven to full and frozen. The overlay then keeps painting opaque black
    // until room re-init (RoomInit.cpp sets specialRoomLightState = 0xffff),
    // which is exactly where the sequence ends (`room_action [00 01 81]`).
    //
    // Scoped so nothing else can reach it: DC mode only, arrange rooms only
    // (so the stage-3 ROOM4110 flashing emergency light, the other script that
    // writes these two fields, is untouched), and only when the fade really is
    // parked at full black with its delta already zeroed.
    if (byteOffset == 2 && value == 0 && g_bDcMode &&
        room_file_stage() >= STAGE_ARRANGE_FIRST &&
        g_SpecialRoomLightDelta == 0 &&
        (short)g_SpecialRoomLightState >= 0x7F00) {
        return 1;
    }

    *(unsigned short*)((char*)&g_fading_state + byteOffset) = value;
    return 1;
}

// ============================================================================
// 0x32 - cmd_skip_4bytes (0x00431b00)
// Skip 4 bytes in the opcode stream.
// ============================================================================
int cmd_skip_4bytes(void)
{
    g_ScdOpcodes += 4;
    return 1;
}

// ============================================================================
// 0x33 - cmd_player_prop_set (0x004314b0)
// Multi-subcommand PLAYER property setter - the twin of cmd_enemy_prop_set
// (0x28). Subcommands: 0 clear equipped weapon, 1 enter the being-attacked
// animation, 3 write/or/xor player.flags, 4 force action 1/6, 5 directionAngle,
// 6 clear unk_8c, 7 reset to idle, 8 write/or/xor healthStatusFlags, 9 xor joint
// flags, 10 set/clear unk_e0 bit 0x40. The sampled scripts use only 10, 8 and 0
// - subcommand 1, the only damage-adjacent one, never turned up.
// ============================================================================
int cmd_player_prop_set(void)
{
    unsigned short* params = (unsigned short*)(g_ScdOpcodes + 2);
    unsigned char subCmd = (unsigned char)(scd_read_u16(0) >> 8);

    switch (subCmd) {
    case 0:
        g_EquippedItemId = 0;
        g_playerEntity.equippedWeaponId = 0;
        g_ScdOpcodes += 2;
        return 1;
    case 1:
        g_playerEntity.isBeingAttackedFlag = (unsigned char)*params;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackDirection = 100;
        g_ScdOpcodes += 4;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.attackAnim = 0;
        *(unsigned int*)&g_playerEntity.animationId = 0x01000001;
        g_playerEntity.unk_8c = 3;
        return 1;
    case 3: {
        char mode = (char)(*params >> 8);
        unsigned char val = (unsigned char)*params;
        if (mode == 0) g_playerEntity.flags = val;
        else if (mode == 1) g_playerEntity.flags |= val;
        else if (mode == 2) g_playerEntity.flags ^= val;
        g_ScdOpcodes += 4;
        return 1;
    }
    case 4:
        g_playerEntity.action_behavior = 1;
        g_playerEntity.action_state = 6;
        g_ScdOpcodes += 2;
        return 1;
    case 5:
        g_playerEntity.directionAngle = *params;
        g_ScdOpcodes += 4;
        return 1;
    case 6:
        g_playerEntity.unk_8c = 0;
        g_ScdOpcodes += 2;
        return 1;
    case 7:
        g_playerEntity.animationId = 1;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 2;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 0;
        g_playerEntity.unk_8c = 3;
        g_ScdOpcodes += 2;
        return 1;
    case 8: {
        char mode = (char)(*params >> 8);
        unsigned char val = (unsigned char)*params;
        if (mode == 0) g_playerEntity.healthStatusFlags = val;
        else if (mode == 1) g_playerEntity.healthStatusFlags |= val;
        else if (mode == 2) g_playerEntity.healthStatusFlags ^= val;
        g_ScdOpcodes += 4;
        return 1;
    }
    case 9: {
        JointStruct* joints = g_playerEntity.jointsStructs;
        for (unsigned short bits = *params; bits != 0; bits >>= 1) {
            joints->flags ^= (unsigned char)bits & 1;
            joints++;
        }
        g_ScdOpcodes += 4;
        return 1;
    }
    case 10:
        if ((*params & 0xff00) == 0) {
            *(unsigned short*)&g_playerEntity.unk_e0 &= ~0x40u;
        } else {
            *(unsigned short*)&g_playerEntity.unk_e0 |= 0x40;
        }
        g_ScdOpcodes += 4;
        return 1;
    default:
        return 1;
    }
}

// ============================================================================
// 0x34 - cmd_model_tint_set (0x00431b10)
// Modify lighting/texture parameters.
// ============================================================================
int cmd_model_tint_set(void)
{
    g_ScdOpcodes++;
    char param1 = (char)*g_ScdOpcodes; g_ScdOpcodes++;
    char param2 = (char)*g_ScdOpcodes - 0x80; g_ScdOpcodes++;
    unsigned char p3 = *g_ScdOpcodes; g_ScdOpcodes++;
    unsigned char p4 = *g_ScdOpcodes; g_ScdOpcodes++;
    unsigned char p5 = *g_ScdOpcodes; g_ScdOpcodes++;
    unsigned short p6 = *g_ScdOpcodes; g_ScdOpcodes++;
    unsigned short p7 = *g_ScdOpcodes; g_ScdOpcodes++;

    if (param1 == 0) {
        scd_model_tint_apply(p5, p6, p7, p3, p4, param2);
    } else if (param1 == 1) {
        FUN_00473d10(p5, p6, p7, p3, p4, param2);
    } else if (param1 == 2) {
        FUN_00473d60(param2, p3, p4);
    }
    return 1;
}

// ============================================================================
// 0x35 - cmd_obj_flag_set (0x00431bf0)
// Modify object entity flags/state.
// ============================================================================
int cmd_obj_flag_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;

    // Special case: stage 3 room 13 object 5
    if (g_stageId == STAGE_GUARDHOUSE && g_roomId == ROOM_WATER_TANK_ENTRY && ((unsigned char)op2 & 0x3f) == 5) {
        *(unsigned char*)g_omodel_table[op2 & 0xff] = 0;
        return 1;
    }

    unsigned char value = (unsigned char)(op2 >> 8);
    if (op1 >> 8 == 0) {
        *(unsigned char*)g_omodel_table[op2 & 0xff] = value;
    } else if (op1 >> 8 == 1) {
        *(unsigned char*)g_item_model_table[op2 & 0xff] = value;
    }
    return 1;
}

// ============================================================================
// 0x36 - cmd_obj_field_test (0x00431c90)
// Compare an object entity field against a value.
// ============================================================================
int cmd_obj_field_test(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;

    unsigned short* objField = (unsigned short*)(
        *(int*)((int)&g_omodel_table + ((op1 >> 6) & 0xfffffffc)) + 0x86);
    unsigned short fieldVal = *objField;
    unsigned short compareVal = op2 >> 8;

    switch (op2 & 0xff) {
    case 0: return compareVal == fieldVal;
    case 1: return fieldVal > compareVal;
    case 2: return fieldVal >= compareVal;
    case 3: return compareVal > fieldVal;
    case 4: return compareVal >= fieldVal;
    case 5: return compareVal != fieldVal;
    default: return 0;
    }
}

// ============================================================================
// 0x37 - cmd_room_bgm_state_set (0x00460a30)
// Set room BGM state data.
// ============================================================================
int cmd_room_bgm_state_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    // Original: AND ECX,0xffffff07 / SHR ECX,0x3 -> (op1 >> 8) * 32, a byte offset
    // into g_roomBgmState (BYTE[224] = 7 stages x 32 rooms). Transcribing the mask
    // literally as (op1 & 0x07) >> 3 is a constant 0, so every stage wrote row 0.
    unsigned int idx = (((unsigned int)op1 & 0xFFFFFF07u) >> 3) + (op2 & 0xFF);
    g_roomBgmState[idx] = (unsigned char)(op2 >> 8);
    return 1;
}

// ============================================================================
// 0x38 - cmd_dpad_test (0x00431dc0)
// Test player D-pad held state.
// ============================================================================
int cmd_dpad_test(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;

    if ((op1 & 0xff00) != 0) {
        return ((g_PlayerDpadHeld & op2) == 0);
    }
    return g_PlayerDpadHeld & op2;
}

// ============================================================================
// 0x39 - cmd_enemy_flags_get (0x00431e10)
// Read enemy behavior_flags into g_scdLastEnemyFlags (DAT_00be982a).
// ============================================================================
int cmd_enemy_flags_get(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    g_scdLastEnemyFlags = g_EnemiesList[op1 >> 8].behavior_flags;
    return 1;
}

// ============================================================================
// 0x3A - cmd_cut_zone_set (0x00431e50)
// Modify camera switch zone entries.
// ============================================================================
int cmd_cut_zone_set(void)
{
    unsigned char zoneIdx = g_ScdOpcodes[1];
    *(unsigned short*)((unsigned int)zoneIdx * 0x14 + 2 + (unsigned int)g_RdtPointer->cam_switch_zones) =
        (unsigned short)g_ScdOpcodes[2];
    *(unsigned short*)((unsigned int)zoneIdx * 0x14 + (unsigned int)g_RdtPointer->cam_switch_zones) =
        (unsigned short)g_ScdOpcodes[3];
    g_ScdOpcodes += 4;
    return 1;
}

// ============================================================================
// 0x3B - cmd_obj_rotation_set (0x00431ea0)
// Set object animation parameters.
// ============================================================================
int cmd_obj_rotation_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;

    char* objPtr;
    if (op1 < 0x8000) {
        objPtr = (char*)g_item_model_table[op1 >> 8];
    } else {
        objPtr = (char*)g_omodel_table[(op1 >> 8) & 0x7f];
    }
    if (*objPtr != 0) {
        *(unsigned short*)(objPtr + 0x72) = scd_read_u16(0);
        g_ScdOpcodes += 2;
        *(unsigned short*)(objPtr + 0x76) = scd_read_u16(0);
        g_ScdOpcodes += 2;
    } else {
        g_ScdOpcodes += 4;
    }
    return 1;
}

// ============================================================================
// 0x3C - cmd_player_dist_test (0x00431f20)
// Test distance between player and an entity/object.
// ============================================================================
int cmd_player_dist_test(void)
{
    g_ScdOpcodes += 6;
    unsigned short targetSpec = scd_read_u16(-4);
    unsigned short maxDist = scd_read_u16(-2);

    int* targetPos;
    if ((targetSpec & 0xff) == 0) {
        targetPos = g_EnemiesList[targetSpec >> 8].scaMatrixData.localMatrix.t;
    } else if ((targetSpec & 0xff) == 1) {
        targetPos = (int*)(*(int*)((int)&g_omodel_table + ((targetSpec >> 6) & 0xfffffffc)) + 0x34);
    } else if ((targetSpec & 0xff) == 2) {
        targetPos = (int*)(*(int*)((int)&g_item_model_table + ((targetSpec >> 6) & 0xfffffffc)) + 0x34);
    } else {
        return 0;
    }

    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - targetPos[0];
    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - targetPos[2];
    unsigned int dist = SquareRoot0(dz * dz + dx * dx);
    return dist <= (unsigned int)maxDist;
}

// ============================================================================
// 0x3D - cmd_bullet_effect_spawn (0x00431770)
// Spawn a bullet/hit effect.
// ============================================================================
int cmd_bullet_effect_spawn(void)
{
    unsigned short typeParam = scd_read_u16(0);
    unsigned short parentParam = scd_read_u16(2);
    // Unlike opcode 0x2A, the original ZERO-extends the position words here
    // (`(int)g_ScdOpcodes[2]` on a ushort*, not `(int)(short)...`). Sign-extending
    // them made negative coordinates spawn effects at the wrong place.
    //
    // ONE VECTOR, not three separate ints - see the note in cmd_effect_spawn.
    // Effect_CreateBillboard reads this as a VECTOR*, and separate locals are not
    // guaranteed adjacent, so y/z would read the uninitialised-stack fill.
    VECTOR spawnPos;
    spawnPos.x = (int)scd_read_u16(4);
    spawnPos.y = (int)scd_read_u16(6);
    spawnPos.z = (int)scd_read_u16(8);
    spawnPos.pad = 0;
    unsigned short effectFlags = scd_read_u16(10);
    g_ScdOpcodes += 12;

    MATRIX* spriteInfo;
    if ((parentParam >> 8) == 0) {
        spriteInfo = &g_identityMatrixData;
    } else if ((parentParam >> 8) == 1) {
        spriteInfo = &g_playerEntity.scaMatrixData.localMatrix;
    } else if ((parentParam & 0x8000) == 0) {
        // As in 0x2A this is the ADDRESS effect + 0x14 (pAnimHeader is the array at
        // +0x04), not the `animDataBase` field at +0x7C. The index degenerates to
        // 0x3D because the original shifts the already-shifted high byte again.
        spriteInfo = (MATRIX*)((char*)&g_effectPool[((unsigned int)(typeParam >> 8) >> 8) * 3 + 0x3d] + 0x14);
    } else {
        // The original indexes with the ALREADY-shifted high byte (`uVar2 >> 6` where
        // uVar2 == typeParam >> 8), so the byte offset is always 0 - i.e. itembox 0.
        // Using `typeParam >> 6` instead picked a different object entirely.
        spriteInfo = (MATRIX*)(*(int*)((char*)&g_omodel_table +
                        (((unsigned int)(typeParam >> 8) >> 6) & 0xFFFFFFFC)) + 0x20);
    }

    unsigned char effectType = (unsigned char)(typeParam >> 8);
    Effect_CreateBillboard(effectType, (unsigned char)parentParam, effectFlags, spriteInfo, &spawnPos, 0);
    g_bulletEffectId = effectType;
    DAT_00bf0a34 = (int)spriteInfo;
    return 1;
}

// ============================================================================
// 0x3E - cmd_bullet_effect_clear (0x00431840)
// FREE every effect slot matching the last bullet effect spawned by 0x3D:
// FUN_0047cf80 criteria mask 9 = effectType AND spriteInfo. It clears effects,
// it does not spawn one.
// ============================================================================
int cmd_bullet_effect_clear(void)
{
    FUN_0047cf80(9, g_bulletEffectId, 0, 0, (MATRIX*)DAT_00bf0a34);
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x3F - cmd_player_dir_test (0x00431fd0)
// Condition: test if player direction is within a wrap-aware angle range.
// ============================================================================
int cmd_player_dir_test(void)
{
    unsigned short minAngle = scd_read_u16(2);
    unsigned short maxAngle = scd_read_u16(4);
    g_ScdOpcodes += 6;
    unsigned short playerDir = g_playerEntity.directionAngle;
    return (int)(unsigned short)(playerDir - minAngle) <= (int)((unsigned int)maxAngle - (unsigned int)minAngle);
}

// ============================================================================
// 0x40 - cmd_light_param_set (0x00432010)
// Modify a light source in the RDT.
// ============================================================================
int cmd_light_param_set(void)
{
    short* params = (short*)g_ScdOpcodes;
    g_ScdOpcodes += 16;
    int lightIdx = params[0] >> 8;
    int* light = (int*)((char*)&g_RdtPointer[1].lights + lightIdx * 0x2c - 4);
    light[0] = (int)params[1];
    light[1] = (int)params[2];
    light[2] = (int)params[3];
    light[3] = (int)params[4];
    light[4] = (int)params[5];
    light[5] = (int)params[6];
    light[8] = (int)params[7];
    return 1;
}

// ============================================================================
// 0x41 - cmd_entity_posy_set (0x00432090)
// Set an entity posY field.
// ============================================================================
int cmd_entity_posy_set(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short value = scd_read_u16(0);
    g_ScdOpcodes += 2;
    if ((op1 & 0xff00) == 0) {
        *(unsigned short*)&g_playerEntity.posY = value;
    } else {
        *(unsigned short*)((char*)&g_playerEntity + (unsigned int)(op1 >> 8) * 0x18c + 0x82) = value;
    }
    return 1;
}

// ============================================================================
// 0x42 - cmd_effect_clear_typed (0x00431870)
// FREE every effect slot matching a type/depth pair: FUN_0047cf80 criteria
// mask 3 = effectType AND depthGroup. The leading 3 is that mask, not an
// effect type.
// ============================================================================
int cmd_effect_clear_typed(void)
{
    unsigned short type = scd_read_u16(0);
    unsigned short param = scd_read_u16(2);
    g_ScdOpcodes += 4;
    FUN_0047cf80(3, type >> 8, param, 0, 0);
    return 1;
}

// ============================================================================
// 0x43 - cmd_bgm_volume_ramp (0x00460d20)
// Modify BGM sound parameters if track is playing.
// ============================================================================
int cmd_bgm_volume_ramp(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned int bit = 1 << ((char)(op1 >> 8) + 3);
    if ((g_BGM_STATE & bit) != 0) {
        FUN_004804a0((short)(unsigned char)DAT_00bf07ef, op1 >> 8, (short)(char)op2, op2 >> 8);
    }
    return 1;
}

// ============================================================================
// 0x44 - cmd_scd_event_kill (0x00461080)
// Deactivate a specific SCD event slot.
// ============================================================================
int cmd_scd_event_kill(void)
{
    g_ScdEventTable[scd_read_u16(0) >> 8].active = 0;
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x45 - cmd_player_posy_add (0x004320f0)
// Add a signed byte to the PLAYER's posY. Unlike 0x41 it has no entity
// selector - g_playerEntity is the only target it can reach.
// ============================================================================
int cmd_player_posy_add(void)
{
    unsigned short val = scd_read_u16(0);
    g_ScdOpcodes += 2;
    *(short*)&g_playerEntity.posY += (char)(val >> 8);
    return 1;
}

// ============================================================================
// 0x46 - cmd_room_lights_set (0x00432110)
// Set all room lights from SCD data.
// ============================================================================
int cmd_room_lights_set(void)
{
    // 44-byte instruction: 2 header + 3 lights x 12 + 3 words x 2.
    // The original advances 2 here, not 4.
    g_ScdOpcodes += 2;
    for (unsigned int offset = 0; offset < 0x3c; offset += 0x14) {
        int* light = (int*)((char*)&g_RdtPointer->lights[0].pos_x + offset);
        light[0] = (int)scd_read_s16(0);
        light[1] = (int)scd_read_s16(2);
        light[2] = (int)scd_read_s16(4);
        ((unsigned char*)light)[12] = g_ScdOpcodes[6];
        ((unsigned char*)light)[13] = g_ScdOpcodes[7];
        ((unsigned char*)light)[14] = g_ScdOpcodes[8];
        // Word write at +0x10 spans zero2/zero3; radius is at +0x12, not +0x14.
        // Writing +0x14 landed in the NEXT RDT_Light's pos_x (and past lights[2]
        // for the last iteration) - a silent out-of-bounds corruption.
        *(unsigned short*)((char*)light + 0x10) = (unsigned short)g_ScdOpcodes[9];
        *(short*)((char*)light + 0x12) = scd_read_s16(10);
        g_ScdOpcodes += 12;
    }
    // Ghidra rendered this as a walk from RDT+0x03; the three shorts it writes
    // are RDT+0x06/0x08/0x0A, i.e. the ambient light colour read just below.
    for (unsigned int i = 0; i < 6; i += 2) {
        *(short*)((char*)&g_RdtPointer->ambient_light_r + i) = scd_read_s16(0);
        g_ScdOpcodes += 2;
    }
    // RDT ambient_light is a COLOR of three SHORTS (Ghidra: COLOR at RDT+6,
    // size 6), and setBackColor takes the full 16-bit values - the menu passes
    // 0x199 (409) here, well over 255. Casting to unsigned char truncated the
    // room ambient: this room's 1775 became 239, which is why every character
    // rendered far too dark.
    setBackColor((unsigned short)g_RdtPointer->ambient_light_r,
                 (unsigned short)g_RdtPointer->ambient_light_g,
                 (unsigned short)g_RdtPointer->ambient_light_b);
    return 1;
}

// ============================================================================
// 0x47 - cmd_obj_transform_set (0x00431080)
// Set object position and rotation offsets.
// ============================================================================
int cmd_obj_transform_set(void)
{
    unsigned short slot = scd_read_u16(0);
    g_ScdOpcodes += 14;
    int objBase = (int)g_omodel_table[slot >> 8];
    *(short*)(objBase + 0x72) = scd_read_s16(-12);
    *(short*)(objBase + 0x74) = scd_read_s16(-10);
    *(short*)(objBase + 0x76) = scd_read_s16(-8);
    short posX = scd_read_s16(-6);
    *(short*)(objBase + 0x6c) = posX;
    *(int*)(objBase + 0x34) = (int)posX;
    short posY = scd_read_s16(-4);
    *(short*)(objBase + 0x6e) = posY;
    *(int*)(objBase + 0x38) = (int)posY;
    short posZ = scd_read_s16(-2);
    *(short*)(objBase + 0x70) = posZ;
    *(int*)(objBase + 0x3c) = (int)posZ;
    return 1;
}

// ============================================================================
// 0x48 - cmd_effect_pool_clear (0x004318a0)
// Clear all effect pool entries.
// ============================================================================
int cmd_effect_pool_clear(void)
{
    g_freeEffectSlots = 0;
    g_ScdOpcodes += 2;   // original advances 2, not 4
    do {
        g_effectPool[g_freeEffectSlots].updateId = 0;
        g_effectPool[g_freeEffectSlots].animId = g_effectPool[g_freeEffectSlots].updateId;
        g_freeEffectSlots++;
    } while (g_freeEffectSlots < 0x40);
    return 1;
}

// ============================================================================
// 0x49 - cmd_room_sprite_hide (0x00432290)
// Queue a room sprite id for hiding. Room_ApplySpriteFlags (0x00432220) walks
// the mask and sets active = 0 on every room sprite whose id is a set bit, then
// clears it. Operand 0xFF wipes the pending mask instead of adding to it.
// ============================================================================
int cmd_room_sprite_hide(void)
{
    unsigned short val = scd_read_u16(0);
    g_ScdOpcodes += 2;
    if ((val >> 8) == 0xff) {
        DAT_00d22770 = 0;
    } else {
        DAT_00d22770 |= 1 << ((unsigned char)(val >> 8) & 0x1f);
    }
    return 1;
}

// ============================================================================
// 0x4A - cmd_bgm_restore (0x00460ae0)
// Restore and play sound slots.
// ============================================================================
int cmd_bgm_restore(void)
{
    // Original advances 2 (ADD dword ptr [g_ScdOpcodes],0x2), not 4, and the
    // guard is CMP byte ptr [g_targetBgmState],0xff - it reads the same byte
    // update_room_bgm writes, not a separate int that was always -1.
    g_ScdOpcodes += 2;
    if (g_targetBgmState != 0xFF) {
        g_BGM_STATE = g_BGM_STATE >> 8;
        for (int ch = 0; ch < 3; ch++) {
            if ((g_BGM_STATE & (8u << ch)) != 0 && g_SndBank[ch].handle != 0) {
                SetSndSlot(g_SndBank[ch].handle, (int)g_SndBank[ch].slot);
            }
        }
    }
    return 1;
}

// ============================================================================
// 0x4B - cmd_bgm_stop_all (0x00460b80)
// Stop all sound banks and shift BGM state.
// ============================================================================
int cmd_bgm_stop_all(void)
{
    // Original advances 2, not 4; same g_targetBgmState byte guard as 0x4A.
    g_ScdOpcodes += 2;
    if (g_targetBgmState != 0xFF) {
        for (int ch = 0; ch < 3; ch++) {
            if (g_SndBank[ch].handle != 0) setSndStop(g_SndBank[ch].handle);
        }
        if (g_BgmSoundBank != 0) setSndStop(g_BgmSoundBank);
        g_BGM_STATE = g_BGM_STATE << 8;
    }
    return 1;
}

// ============================================================================
// 0x4C - cmd_item_record_transfer (0x004322d0)
// Move a pick-up QUANTITY between a room action record and one of the BioCard
// state bytes. Record byte 8 is the item id and byte 9 its quantity, so mode 0
// remembers the count, mode 1 restores it and mode 2 loads the quantity the
// player is actually carrying into both. The three uses found are all mode 1 on
// pickupQtyA/B/C: ROOM1160 restores a shotgun (7 shells), ROOM30B0 and ROOM3080
// a flamethrower (240 fuel each) - matching the 7/0xF0/0xF0 SetInitialItems
// seeds exactly, which is what confirms the three-byte mapping.
// ============================================================================
int cmd_item_record_transfer(void)
{
    unsigned short op1 = scd_read_u16(0);
    unsigned short op2 = scd_read_u16(2);
    g_ScdOpcodes += 4;

    unsigned int mode = op1 >> 8;
    unsigned int slotIdx = op2 & 0xff;
    unsigned int fieldIdx = op2 >> 8;

    // Entry +8 holds a POINTER to the originating SCD record (cmd_door_set /
    // cmd_room_action_set store g_ScdOpcodes + 2 there). The original dereferences it and
    // reads/writes bytes +8 and +9 *inside that record*:
    //   (&DAT_00d91aa8)[slot * 3]   ==  *(u8**)(table + slot * 0xC + 8)
    // The old code indexed the table entry itself at +9 / +8, i.e. it read and wrote
    // the bytes of the pointer instead of following it.
    unsigned char* rec = *(unsigned char**)&g_RoomActionTable[slotIdx * 0xc + 8];

    if (mode == 0) {
        (&g_stageId)[fieldIdx] = rec[9];
    } else if (mode == 1) {
        rec[9] = (&g_stageId)[fieldIdx];
    } else if (mode == 2) {
        int itemSlot = get_item_slot(rec[8]);
        if (itemSlot >= 0) {
            unsigned char val = ((unsigned char*)g_ItemSlotsPointer)[itemSlot * 2 + 1];
            (&g_stageId)[fieldIdx] = val;
            rec[9] = val;
        }
    }
    return 1;
}

// ============================================================================
// 0x4D - cmd_player_joint_tint (0x004323a0)
// Reset player entity lighting/palette to default grey.
// ============================================================================
int cmd_player_joint_tint(void)
{
    // Applies a joint colour tint to the player via JointApplyColorTint
    // (0x0048a190). The tint actually applied is the SECOND argument, 0x30
    // (r=0x30,g=0,b=0) - JointSetColorTint reads only two arguments, so the
    // 0x00080820 and 0x00606060 pushed here are dead. An earlier comment here
    // described this as "reset to 0x606060 medium grey", which is wrong.
    //
    // The base is `MOV ESI,dword ptr [0x00be637c]` - the POINTER stored at
    // g_playerEntity+0x98, i.e. jointsStructs - not the address of the entity
    // itself. The old code offset from &g_playerEntity and so tinted unrelated
    // entity fields while leaving the joints untouched.
    //
    // The 14 offsets are joints 0-13 at the JointStruct stride of 0x7c. The odd
    // visiting order (0,1,2,9,12,3,4,5,6,7,8,10,11,13) is the original's.
    extern void FUN_0048a190(void* ptr, int size, int flags, int color);
    char* joints = (char*)g_playerEntity.jointsStructs;
    static const int kJointOrder[14] = { 0, 1, 2, 9, 12, 3, 4, 5, 6, 7, 8, 10, 11, 13 };
    for (int i = 0; i < 14; i++) {
        FUN_0048a190(joints + kJointOrder[i] * 0x7c, 0x30, 0x00080820, 0x00606060);
    }
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x4E - cmd_effect_flags_modify (0x00431910)
// Modify flags on all active effect pool entries.
// ============================================================================
int cmd_effect_flags_modify(void)
{
    unsigned short op1 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned short op2 = scd_read_u16(0);
    g_ScdOpcodes += 2;
    unsigned int mode = op1 >> 8;

    // The flag word is at effect +0x0E (inside animHeader), not +0x02. The original
    // walks with a cursor at effect+0x0E and reaches animId/updateId via [-0xE]/[-0xD];
    // starting the cursor at +0x02 made this OR/AND/XOR into `type`/`lightFactor`.
    unsigned char* ptr = (unsigned char*)&g_effectPool[0] + 0x0E;
    while (ptr < (unsigned char*)&g_playerEntity.unk_0e) {
        Effect* eff = (Effect*)(ptr - 0x0E);
        if (eff->animId != 0 || eff->updateId != 0) {
            unsigned short* flagsPtr = (unsigned short*)ptr;
            if (mode == 0) *flagsPtr |= op2;
            else if (mode == 1) *flagsPtr &= ~op2;
            else if (mode == 2) *flagsPtr ^= op2;
        }
        ptr += 0x84;
    }
    return 1;
}

// ============================================================================
// 0x4F - cmd_costume_variant_set (0x004622b0)
// Store the operand's low bit in g_bCostumeVariant, which LoadEntityEMD reads
// to pick the alternate character model.
// ============================================================================
int cmd_costume_variant_set(void)
{
    extern void FUN_0040c560(int param);
    FUN_0040c560(scd_read_u16(0) >> 8);
    g_ScdOpcodes += 2;
    return 1;
}

// ============================================================================
// 0x50 - cmd_costume_variant_test (0x004622e0)
// Condition: return g_bCostumeVariant, the bit opcode 0x4F wrote. The original
// consumes the 2-byte instruction and returns the byte directly - there is no
// callee, and the operand byte is ignored.
// ============================================================================
int cmd_costume_variant_test(void)
{
    g_ScdOpcodes += 2;
    return (int)g_bCostumeVariant;
}

// ============================================================================
// script_command_funcs_table (0x004c1110)
// SCD command dispatch table - 81 entries (opcodes 0x00-0x50).
// ============================================================================
typedef int (*ScdCmdFunc)(void);

void* script_command_funcs_table[256] = {
    /* 0x00 */ (void*)cmd_block_end,              // 0x004604d0
    /* 0x01 */ (void*)cmd_if,                     // 0x004604e0
    /* 0x02 */ (void*)cmd_else,                   // 0x00460520
    /* 0x03 */ (void*)cmd_end_if,                 // 0x00460550
    /* 0x04 */ (void*)cmd_bit_test,               // 0x00460570
    /* 0x05 */ (void*)cmd_bit_op,                 // 0x00460650
    /* 0x06 */ (void*)cmd_state_byte_test,        // 0x00460760
    /* 0x07 */ (void*)cmd_state_word_test,        // 0x00460800
    /* 0x08 */ (void*)cmd_state_byte_set,         // 0x004608a0
    /* 0x09 */ (void*)cmd_cut_lock_set,           // 0x00460920
    /* 0x0A */ (void*)cmd_current_cut_set,        // 0x00460990
    /* 0x0B */ (void*)cmd_message_set,            // 0x004609f0
    /* 0x0C */ (void*)cmd_door_set,               // 0x004611b0
    /* 0x0D */ (void*)cmd_room_action_set,        // 0x00461130
    /* 0x0E */ (void*)cmd_skip_2bytes_opcode,     // 0x00460900
    /* 0x0F */ (void*)cmd_mirror_set,             // 0x004610b0
    /* 0x10 */ (void*)cmd_used_item_test,         // 0x00460f30
    /* 0x11 */ (void*)cmd_picked_item_test,       // 0x00460f10
    /* 0x12 */ (void*)cmd_room_action_reset,      // 0x00460fc0
    /* 0x13 */ (void*)cmd_room_action_arm,        // 0x00461010
    /* 0x14 */ (void*)cmd_scd_event_create,       // 0x00461040
    /* 0x15 */ (void*)cmd_bgm_play,               // 0x00460a80
    /* 0x16 */ (void*)cmd_bgm_stop,               // 0x00460c70
    /* 0x17 */ (void*)cmd_sfx_3d_play,            // 0x00460d80
    /* 0x18 */ (void*)cmd_item_model_set,         // 0x00461220
    /* 0x19 */ (void*)cmd_model_flag_set,         // 0x00460f50
    /* 0x1A */ (void*)cmd_item_search,            // 0x00460f80
    /* 0x1B */ (void*)cmd_enemy_set,              // 0x004617d0
    /* 0x1C */ (void*)cmd_room_light_fade_set,    // 0x00462210
    /* 0x1D */ (void*)cmd_equipped_item_test,     // 0x00460ee0
    /* 0x1E */ (void*)cmd_voice_play,             // 0x00461a80
    /* 0x1F */ (void*)cmd_omodel_set,             // 0x00461ac0
    /* 0x20 */ (void*)cmd_player_pos_set,         // 0x00430f60
    /* 0x21 */ (void*)cmd_enemy_pos_set,          // 0x00430fe0
    /* 0x22 */ (void*)cmd_item_count_test,        // 0x00431100
    /* 0x23 */ (void*)cmd_cut_lock_write,         // 0x00431280
    /* 0x24 */ (void*)cmd_room_action,            // 0x004312b0
    /* 0x25 */ (void*)cmd_room_sprite_set,        // 0x004621d0
    /* 0x26 */ (void*)cmd_dead_slot_hang_26,      // 0x00460ce0 - dead slot, hangs (faithful)
    /* 0x27 */ (void*)cmd_snd_fade_set,           // 0x00460cf0
    /* 0x28 */ (void*)cmd_enemy_prop_set,         // 0x004312f0
    /* 0x29 */ (void*)cmd_fmv_set,                // 0x00461a40
    /* 0x2A */ (void*)cmd_effect_spawn,           // 0x004316c0
    /* 0x2B */ (void*)cmd_attack_anim_set,        // 0x00431990
    /* 0x2C */ (void*)cmd_item_remove,            // 0x004319e0
    /* 0x2D */ (void*)cmd_got_item,               // 0x00431a20
    /* 0x2E */ (void*)cmd_dead_slot_hang_2e,      // 0x00460a70 - dead slot, hangs (faithful)
    /* 0x2F */ (void*)cmd_snd_pan_vol_set,        // 0x00460c00
    /* 0x30 */ (void*)cmd_boundary_set,           // 0x00431a40
    /* 0x31 */ (void*)cmd_state_word_set,         // 0x004608d0
    /* 0x32 */ (void*)cmd_skip_4bytes,            // 0x00431b00
    /* 0x33 */ (void*)cmd_player_prop_set,        // 0x004314b0
    /* 0x34 */ (void*)cmd_model_tint_set,         // 0x00431b10
    /* 0x35 */ (void*)cmd_obj_flag_set,           // 0x00431bf0
    /* 0x36 */ (void*)cmd_obj_field_test,         // 0x00431c90
    /* 0x37 */ (void*)cmd_room_bgm_state_set,     // 0x00460a30
    /* 0x38 */ (void*)cmd_dpad_test,              // 0x00431dc0
    /* 0x39 */ (void*)cmd_enemy_flags_get,        // 0x00431e10
    /* 0x3A */ (void*)cmd_cut_zone_set,           // 0x00431e50
    /* 0x3B */ (void*)cmd_obj_rotation_set,       // 0x00431ea0
    /* 0x3C */ (void*)cmd_player_dist_test,       // 0x00431f20
    /* 0x3D */ (void*)cmd_bullet_effect_spawn,    // 0x00431770
    /* 0x3E */ (void*)cmd_bullet_effect_clear,    // 0x00431840
    /* 0x3F */ (void*)cmd_player_dir_test,        // 0x00431fd0
    /* 0x40 */ (void*)cmd_light_param_set,        // 0x00432010
    /* 0x41 */ (void*)cmd_entity_posy_set,        // 0x00432090
    /* 0x42 */ (void*)cmd_effect_clear_typed,     // 0x00431870
    /* 0x43 */ (void*)cmd_bgm_volume_ramp,        // 0x00460d20
    /* 0x44 */ (void*)cmd_scd_event_kill,         // 0x00461080
    /* 0x45 */ (void*)cmd_player_posy_add,        // 0x004320f0
    /* 0x46 */ (void*)cmd_room_lights_set,        // 0x00432110
    /* 0x47 */ (void*)cmd_obj_transform_set,      // 0x00431080
    /* 0x48 */ (void*)cmd_effect_pool_clear,      // 0x004318a0
    /* 0x49 */ (void*)cmd_room_sprite_hide,       // 0x00432290
    /* 0x4A */ (void*)cmd_bgm_restore,            // 0x00460ae0
    /* 0x4B */ (void*)cmd_bgm_stop_all,           // 0x00460b80
    /* 0x4C */ (void*)cmd_item_record_transfer,   // 0x004322d0
    /* 0x4D */ (void*)cmd_player_joint_tint,      // 0x004323a0
    /* 0x4E */ (void*)cmd_effect_flags_modify,    // 0x00431910
    /* 0x4F */ (void*)cmd_costume_variant_set,    // 0x004622b0
    /* 0x50 */ (void*)cmd_costume_variant_test,   // 0x004622e0
    // Opcodes 0x51-0xFF: left nullptr. scd_dispatch reports and aborts on a NULL
    // rather than calling through, because a bad opcode means the stream itself
    // desynced. The original table has no padding and no bound.
    // (entries 0x51-0xF5 may be accessed; 0xF6-0xFF are handled by room_events_check)
};

// ============================================================================
// SCD command helper implementations (moved here from GameState.cpp).
// These back the script_command_funcs_table entries above.
// ============================================================================

extern void ResolveAnimPointers(unsigned char* data);        // TmdAnimation.cpp

// ---------------------------------------------------------------------------
// Flg_on (0x00473ef0)
// Sets a bit flag in a flag array.
// baseAddr: base address of the flag array
// bitIndex: bit position to set
// ---------------------------------------------------------------------------
void Flg_on(int baseAddr, unsigned int bitIndex)
{
    unsigned int* flagWord = (unsigned int*)(((bitIndex & 0xffffffe7) >> 3) + baseAddr);
    *flagWord = *flagWord | (0x80000000U >> ((unsigned char)bitIndex & 0x1f));
}

// ============================================================================
// FUN_00473f10 (0x00473f10) - Clear a bit flag
// Exact counterpart of Flg_ck (0x00473f40): same byte-offset idiom
// ((bitIndex & 0xFFFFFFE7) >> 3, i.e. (bitIndex >> 5) * 4) and the same MSB-first
// bit order within the dword.
// ============================================================================
void FUN_00473f10(int* baseAddr, unsigned int bitIndex)
{
    unsigned int byteOffset = (bitIndex & 0xFFFFFFE7u) >> 3;
    unsigned int bitMask    = 0x80000000u >> (bitIndex & 0x1F);
    unsigned int* flagWord  = (unsigned int*)((unsigned char*)baseAddr + byteOffset);
    *flagWord &= ~bitMask;
}

// ============================================================================
// memset_ (0x0047cf60) - Zero N dwords
// The original's 2-arg helper (distinct from _memset): writes 0 over
// `dwordCount` consecutive dwords. Used by room_event_item_pickup to clear an
// effect slot (0x21 dwords = one 0x84-byte Effect).
// ============================================================================
void memset_(unsigned int* dst, int dwordCount)
{
    for (; dwordCount != 0; dwordCount--) {
        *dst = 0;
        dst++;
    }
}

// ============================================================================
// Model colour-tint helpers used by SCD opcode 0x34 variant 0.
//
// All three walk the SAME per-object array JointSetColorTint (0x00485ac0) walks:
// modelObj+0x20 is the CMarniDirect3DTMD, its object count is the dword at
// +0x4C0, and the objects start at +0x4D0 with a stride of 0x84. The loop bound
// is count * 2 (each object has a mirrored copy). Within an object,
// +0x5C/+0x60/+0x64 are the R/G/B tint multipliers as floats and
// +0x6C/+0x70/+0x74 are the second copy the renderer actually samples.
//
// Like the already-ported JointSetColorTint, only the `modelObj+0x10 == 0`
// branch is transcribed. The original's else-branch drives the complex-TMD
// staging buffer g_abComplexTmdObjectData (0x008ffd1c), which this port does not
// model at all - the same omission, and made for the same reason.
// ============================================================================

// 1/31, the fixed-point scale the two tint helpers share (float at 0x004af2e8).
static const float kTintScale = 0.032258064f;

// ============================================================================
// TmdObjectTintAdd (0x00485c60)
// Adds a signed RGB delta to every object of a model, rebased so the brighter of
// the R/G deltas becomes zero:
//
//   fr = r/31,  fg = g/31,  fmax = max(fr, fg)
//   dR = fr - fmax,  dG = fg - fmax,  dB = b/31 - fmax
//
// so the tint only ever darkens. Each component is accumulated onto the existing
// multiplier and clamped into [0, 1]; the second copy at +0x6C/+0x70/+0x74 is set
// to a constant 2.0f rather than mirrored (that is the original's behaviour, not
// a transcription slip). Green is then forced to 0 unconditionally, and blue is
// snapped to 0 below 0.36 (double at 0x004af2f0).
//
// On the FIRST object only, the resulting tint is packed back into modelObj+0x18
// as 0x00RRGGBB with each channel scaled by 255.0 (float at 0x004af2f8).
// ============================================================================
static void TmdObjectTintAdd(void* modelObj, int r, int g, int b)
{
    unsigned char* obj = (unsigned char*)modelObj;
    if (obj == NULL || *(int*)(obj + 0x10) != 0) {
        return;   // complex-TMD path, not modelled (see the note above)
    }

    unsigned char* tmd = *(unsigned char**)(obj + 0x20);
    if (tmd == NULL || (*(unsigned int*)(tmd + 0x4C0) & 0x7FFFFFFF) == 0) {
        return;
    }

    float fr   = (float)r * kTintScale;
    float fg   = (float)g * kTintScale;
    float fmax = (fr <= fg) ? fg : fr;
    float dR   = fr - fmax;
    float dG   = fg - fmax;
    float dB   = (float)b * kTintScale - fmax;

    unsigned char* rec = tmd + 0x4D0;
    unsigned int count = (unsigned int)(*(int*)(tmd + 0x4C0) * 2);

    for (unsigned int i = 0; i < count; i++) {
        *(unsigned int*)(rec + 0x80) |= 2;

        *(float*)(rec + 0x5C) += dR;
        *(unsigned int*)(rec + 0x6C) = 0x40000000u;   // 2.0f
        *(float*)(rec + 0x60) += dG;
        *(unsigned int*)(rec + 0x70) = 0x40000000u;   // 2.0f
        *(float*)(rec + 0x64) += dB;
        *(unsigned int*)(rec + 0x74) = 0x40000000u;   // 2.0f

        // The clamps are integer compares on the float bit patterns, exactly as
        // the original: signed `> 0x3F800000` catches anything above 1.0f, and
        // unsigned `> 0x80000000` catches any negative value except -0.0f.
        for (int off = 0x5C; off <= 0x64; off += 4) {
            if (*(int*)(rec + off) > 0x3F800000) {
                *(unsigned int*)(rec + off) = 0x3F800000u;   // 1.0f
            }
            if (*(unsigned int*)(rec + off) > 0x80000000u) {
                *(unsigned int*)(rec + off) = 0;
            }
        }

        *(unsigned int*)(rec + 0x60) = 0;                    // 0x00485db3
        if (*(float*)(rec + 0x64) < 0.36f) {
            *(unsigned int*)(rec + 0x64) = 0;
        }

        if (i == 0) {
            unsigned int pr = (unsigned int)(int)(*(float*)(rec + 0x5C) * 255.0f);
            unsigned int pg = (unsigned int)(int)(*(float*)(rec + 0x60) * 255.0f);
            unsigned int pb = (unsigned int)(int)(*(float*)(rec + 0x64) * 255.0f);
            *(unsigned int*)(obj + 0x18) =
                ((pr & 0xFF) << 16) | ((pg << 8) & 0xFF00) | (pb & 0xFF);
        }

        rec += 0x84;
    }
}

// ============================================================================
// TmdObjectTintSet (0x00485fa0)
// Sets (rather than accumulates) the RGB multipliers from a signed delta scaled
// by 5/31. The three values are rebased so that every positive component is
// subtracted from all three - repeated for R, then G, then B - which drives the
// brightest channel to exactly 0 and leaves the others negative. The stored
// multiplier is `1.0f + delta`, so the result only ever darkens.
//
// Unlike TmdObjectTintAdd this branch does NOT set the +0x80 dirty bit, and it
// writes the second copy (+0x6C/+0x70/+0x74) with the same value as the first.
// ============================================================================
static void TmdObjectTintSet(void* modelObj, int r, int g, int b)
{
    unsigned char* obj = (unsigned char*)modelObj;
    if (obj == NULL || *(int*)(obj + 0x10) != 0) {
        return;   // complex-TMD path, not modelled
    }

    unsigned char* tmd = *(unsigned char**)(obj + 0x20);
    if (tmd == NULL || (*(unsigned int*)(tmd + 0x4C0) & 0x7FFFFFFF) == 0) {
        return;
    }

    float fr = (float)(r * 5) * kTintScale;
    float fg = (float)(g * 5) * kTintScale;
    float fb = (float)(b * 5) * kTintScale;

    if (fr > 0.0f) { fg -= fr; fb -= fr; fr = 0.0f; }
    if (fg > 0.0f) { fr -= fg; fb -= fg; fg = 0.0f; }
    if (fb > 0.0f) { fr -= fb; fg -= fb; fb = 0.0f; }

    unsigned char* rec = tmd + 0x4D0;
    unsigned int count = (unsigned int)(*(int*)(tmd + 0x4C0) * 2);

    for (unsigned int i = 0; i < count; i++) {
        *(float*)(rec + 0x5C) = fr + 1.0f;
        *(float*)(rec + 0x6C) = fr + 1.0f;
        *(float*)(rec + 0x60) = fg + 1.0f;
        *(float*)(rec + 0x70) = fg + 1.0f;
        *(float*)(rec + 0x64) = fb + 1.0f;
        *(float*)(rec + 0x74) = fb + 1.0f;
        rec += 0x84;
    }
}

// ============================================================================
// TmdObjectSetLightScale (0x004870a0)
// Stores a single negated 1/32-scaled value at modelObj+0x14. The original is
// called with FOUR arguments but its body reads only two - the same dead-argument
// pattern as JointApplyColorTint / JointSetColorTint.
// ============================================================================
static void TmdObjectSetLightScale(void* modelObj, int value)
{
    if (modelObj != NULL) {
        *(float*)((unsigned char*)modelObj + 0x14) = (float)(-value) * 0.03125f;
    }
}

// ============================================================================
// scd_model_tint_apply (0x00473b10) - Accumulate a colour tint on a queue entry and
// apply it to the live model. SCD opcode 0x34 variant 0.
//
// Was an empty stub, so variant 0 of opcode 0x34 - the only variant that
// actually tints anything - did nothing at all. Variants 1 and 2
// (FUN_00473d10 / FUN_00473d60) only ever rewrote the queue entry.
//
// Finds the g_textureQueueData entry whose id byte matches p6, ADDS the three
// signed deltas onto bytes +3/+4/+5 (clamping each into [-31, +31]), stores the
// two 16-bit parameters at +6/+8 and arms the entry via +1. It then pushes the
// tint straight into the live model:
//
//   p6 bit 7 clear -> an ENEMY id. Scan up to 30 entries of g_EnemiesList for a
//     matching id and tint every joint. Ids 8, 0x0F and 0x12 use the absolute
//     TmdObjectTintSet with the CLAMPED queue bytes; every other id uses the
//     accumulating TmdObjectTintAdd with the RAW deltas.
//   p6 bit 7 set   -> an object index into g_omodel_table. When all
//     three clamped bytes are equal the tint is a pure luminance change and goes
//     through TmdObjectSetLightScale; otherwise TmdObjectTintAdd.
//
// Two original quirks preserved deliberately:
//   - the enemy bound is `if (0x1d < g_enemy_count) count = 0x1e`, i.e. clamp to
//     30, not 32.
//   - the two object branches mask the index differently (0x7f for the
//     luminance path, 0x3f for the tint path). That asymmetry is the
//     original's; it is not a transcription slip.
// ============================================================================
void scd_model_tint_apply(short p1, short p2, short p3, unsigned short p4, unsigned short p5, unsigned char p6)
{
    unsigned char* e = g_textureQueueData;
    unsigned char slot = 0;
    while (*e != (unsigned char)p6) {
        e += 10;
        slot++;
        if (slot > 3) {
            return;
        }
    }

    e[3] = (unsigned char)(e[3] + (char)p1);
    e[4] = (unsigned char)(e[4] + (char)p2);
    e[5] = (unsigned char)(e[5] + (char)p3);
    *(unsigned short*)(e + 6) = p4;
    *(unsigned short*)(e + 8) = p5;

    for (int i = 3; i <= 5; i++) {
        if ((char)e[i] < -0x1F) e[i] = 0xE1;    // -31
        if ((char)e[i] >  0x1F) e[i] = 0x1F;    // +31
    }
    e[1] = 1;

    if (((unsigned char)p6 & 0x80) == 0) {
        unsigned char count = g_enemy_count;
        if (count > 0x1D) {
            count = 0x1E;
        }
        for (unsigned int n = 0; n < (unsigned int)count; n++) {
            Entity* enemy = &g_EnemiesList[n];
            if (enemy->id != (unsigned char)p6) {
                continue;
            }
            JointStruct* joints = enemy->jointsStructs;
            if (enemy->id == 8 || enemy->id == 0x0F || enemy->id == 0x12) {
                for (int j = 0; j < (int)(unsigned int)enemy->jointCount; j++) {
                    TmdObjectTintSet(joints[j].anim_object,
                                     (int)(char)e[3], (int)(char)e[4], (int)(char)e[5]);
                }
            } else {
                for (int j = 0; j < (int)(unsigned int)enemy->jointCount; j++) {
                    TmdObjectTintAdd(joints[j].anim_object, (int)p1, (int)p2, (int)p3);
                }
            }
        }
        return;
    }

    if (e[3] == e[4] && e[3] == e[5]) {
        int obj = (int)g_omodel_table[(unsigned char)p6 & 0x7F];
        TmdObjectSetLightScale(*(void**)((unsigned char*)obj + 0x18), (int)(char)e[3]);
    } else {
        int obj = (int)g_omodel_table[(unsigned char)p6 & 0x3F];
        TmdObjectTintAdd(*(void**)((unsigned char*)obj + 0x18), (int)p1, (int)p2, (int)p3);
    }
}

// ============================================================================
// FUN_00473d10 (0x00473d10) - Retarget a texture-queue entry with explicit bytes
// Same scan as FUN_00473d60 (match the id byte at +0 against p6, arm via +1), but
// stores p1/p2/p3 into bytes +3/+4/+5 instead of clearing them, and p4/p5 into the
// words at +6/+8. SCD opcode 0x34 variant 1.
// Every source is a byte in cmd_model_tint_set, so the low byte of the wider parameters is
// what the original actually stores - the declared widths differ from Ghidra's
// inferred ones but the stored values are identical.
// ============================================================================
void FUN_00473d10(short p1, short p2, short p3, unsigned short p4, unsigned short p5, char p6)
{
    unsigned char* e = g_textureQueueData;
    for (unsigned char i = 0; i < 4; i++) {
        if ((char)e[0] == p6) {
            e[3] = p1;
            e[4] = (unsigned char)p2;
            e[5] = (unsigned char)p3;
            *(unsigned short*)(e + 6) = p4;
            *(unsigned short*)(e + 8) = p5;
            e[1] = 1;
            return;
        }
        e += 10;
    }
}

// ============================================================================
// FUN_00473d60 (0x00473d60) - Retarget an existing texture-queue entry
// Scans the 4 entries of g_textureQueueData (10 bytes each, 0x00d22740) for one
// whose id byte matches p1; on a match, clears bytes +3..+5, stores the two
// 16-bit parameters at +6 and +8, and arms the entry by setting +1 to 1.
// No match = no-op. SCD opcode 0x34 variant 2.
// ============================================================================
void FUN_00473d60(char p1, unsigned char p2, unsigned char p3)
{
    unsigned char* e = g_textureQueueData;
    for (unsigned char i = 0; i < 4; i++) {
        if ((char)e[0] == p1) {
            e[3] = 0;
            e[4] = 0;
            e[5] = 0;
            *(unsigned short*)(e + 6) = p2;
            *(unsigned short*)(e + 8) = p3;
            e[1] = 1;
            return;
        }
        e += 10;
    }
}

// ============================================================================
// FUN_00473e40 (0x00473e40) - Force semi-transparency on a TMD's textured prims
// Resolves the TMD's animation pointers if needed, then walks each primitive
// group (7 dwords per group, count at +8; prim list pointer at group+0x10, prim
// count at group+0x14). Any primitive whose command dword has bit 0x04000000 set
// also gets 0x02000000 set. Primitive stride is ((cmd >> 8) & 0xFF) + 1 dwords.
// The original returns *param_1; every caller ignores it. SCD opcode 0x1F.
// ============================================================================
void FUN_00473e40(int param)
{
    unsigned int* p = (unsigned int*)param;
    if (p[1] == 0) {
        ResolveAnimPointers((unsigned char*)(p + 1));
    }
    unsigned int* group = p + 3;
    for (int groups = (int)p[2]; groups != 0; groups--) {
        unsigned int* prim = (unsigned int*)group[4];
        for (int prims = (int)group[5]; prims != 0; prims--) {
            unsigned int cmd = *prim;
            if ((cmd & 0x04000000) != 0) {
                *prim = cmd | 0x02000000;
            }
            prim += ((cmd >> 8) & 0xFF) + 1;
        }
        group += 7;
    }
}

// ============================================================================
// FUN_00473ea0 (0x00473ea0) - Bind a TMD to an object's animation slot
// Resolves the TMD's animation pointers if needed, links the object's anim slot
// to the TMD's slot table, stores the SCA matrix pointer at param2+4, zeroes
// param2+0, and allocates the animation object from the load arena.
// SCD opcodes 0x18 and 0x1F.
// ============================================================================
void FUN_00473ea0(int param1, void* param2, ScaMatrixData* param3)
{
    extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);
    extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2);

    if (*(int*)(param1 + 4) == 0) {
        ResolveAnimPointers((unsigned char*)(param1 + 4));
    }
    SetAnimSlot((AnimSlot*)(param1 + 0xc), (int)param2, 0);
    ((unsigned int*)param2)[1] = (unsigned int)param3;
    *((unsigned int*)param2) = 0;
    g_loadDataDestPointer = CreateAnimObject((int)param2, (unsigned int*)g_loadDataDestPointer);
}

// ============================================================================
// FUN_0047cf80 (0x0047cf80) - Free every effect slot matching selected criteria
// param1 is a criteria MASK; each set bit enables one comparison, and a slot is
// freed only when every enabled comparison matches (the original builds an
// accumulator and tests `accumulator == mask`, so bits above 3 make it unmatchable):
//   bit 0 -> effect->effectType     == (u8)param2   (+0x26)
//   bit 1 -> effect->depthGroup     == (u8)param3   (+0x27)
//   bit 2 -> effect->animHeader[2]  == (u8)param4   (+0x06, unnamed field)
//   bit 3 -> effect->spriteInfo     == (int)param5  (+0x64)
// Freeing = bump g_freeEffectSlots and zero updateId then animId (animId 0 marks
// the slot free), the same two bytes SCD opcode 0x48 clears.
//
// The original walks the 64 slots backwards (index 63 down to 0), pointing at
// effect+0x26 and stepping by -0x84; reproduced here as an index loop.
// Callers: SCD opcode 0x3E (mask 9 = type + spriteInfo) and 0x42 (mask 3 = type +
// depthGroup).
// ============================================================================
void FUN_0047cf80(int param1, unsigned int param2, unsigned int param3, unsigned int param4, MATRIX* param5)
{
    unsigned char mask = (unsigned char)param1;

    for (int i = 63; i >= 0; i--) {
        Effect* e = &g_effectPool[i];
        unsigned char matched = 0;

        if ((mask & 1) != 0 && e->effectType == (unsigned char)param2) {
            matched = 1;
        }
        if ((mask & 2) != 0 && e->depthGroup == (unsigned char)param3) {
            matched |= 2;
        }
        if ((mask & 4) != 0 && e->animHeader[2] == (unsigned char)param4) {
            matched |= 4;
        }
        if ((mask & 8) != 0 && e->spriteInfo == (int)param5) {
            matched |= 8;
        }

        if (matched == mask) {
            g_freeEffectSlots++;
            e->updateId = 0;
            e->animId   = 0;
        }
    }
}

// ============================================================================
// FUN_004870d0 (0x004870d0) - Set flag bit 1 on 32 consecutive 0x84-byte records
// param is a pointer whose +0x20 field holds the base of a record array; each
// record is 0x84 bytes and the flag dword sits at +0x4CC relative to that base.
// The original increments the offset BEFORE using it, so the first record touched
// is base+0x4CC+0x84 and the last is base+0x4CC+0x1080 (32 iterations).
// Called from SCD opcode 0x18 when the item type is 0x1E.
// Field names are left as raw offsets: the pointed-to type is not yet modeled.
// ============================================================================
void FUN_004870d0(int param)
{
    int base = *(int*)(param + 0x20);
    int offset = 0;
    do {
        offset += 0x84;
        unsigned int* flags = (unsigned int*)(base + 0x4CC + offset);
        *flags |= 2;
    } while (offset < 0x1080);
}

// ============================================================================
// FUN_0048a190 / JointApplyColorTint (0x0048a190)
// Marks a joint dirty (bit 0x80 of its first byte), publishes its vertex count
// doubled into g_playerDisplacement, and applies a colour tint to its model
// object. When g_main_state_flags bit 0 is set the same is repeated on the
// mirrored weapon-joint copy, reached by adding
// (ENTITY->weaponJointsPtr - ENTITY->jointsStructs) to the joint pointer.
//
// NOTE: JointSetColorTint (0x00485ac0) reads only TWO arguments - verified by
// disassembly: it takes arg2 from [ESP+8] at entry and arg1 from [ESP+0x20], and
// never references arg3/arg4. The original pushes four and cleans 0x10, so
// param3/param4 are DEAD. The colour actually applied is param2, so SCD opcode
// 0x4D tints with 0x30 (r=0x30,g=0,b=0), not with the 0x00606060 it also pushes.
// ============================================================================
void FUN_0048a190(void* param1, int param2, int param3, int param4)
{
    (void)param3;   // pushed by the original, never read by JointSetColorTint
    (void)param4;

    unsigned char* joint = (unsigned char*)param1;
    *joint |= 0x80;
    g_playerDisplacement = *(int*)(*(int*)(joint + 0x14) + 0x14) * 2;
    JointSetColorTint(*(int*)(joint + 0x18), (unsigned int)param2);

    if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
        joint += (*(int*)((unsigned char*)ENTITY + 0xac) -
                  *(int*)((unsigned char*)ENTITY + 0x98));
        g_tempVar = joint;
        *joint |= 0x80;
        g_playerDisplacement = *(int*)(*(int*)(joint + 0x14) + 0x14) * 2;
        JointSetColorTint(*(int*)(joint + 0x18), (unsigned int)param2);
    }
}

// ============================================================================
// FUN_0048bfe0 (0x0048bfe0) - Allocate two work buffers for the current entity
// Carves 0x7A00 and 0x1A00 bytes off the load arena and stores the two pointers
// at entity +0xB0 and +0xB4. Both offsets fall inside the unnamed padding of
// Entity/PlayerEntity (pad_b0 / pad_a4), so they are written by offset rather
// than invented field names. Called from SCD opcode 0x0F.
// ============================================================================
void FUN_0048bfe0(void)
{
    unsigned char* ent = (unsigned char*)ENTITY;
    *(void**)(ent + 0xB0) = g_loadDataDestPointer;
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + 0x7A00;
    *(void**)(ent + 0xB4) = g_loadDataDestPointer;
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + 0x1A00;
}

// ============================================================================
// FUN_0048c020 (0x0048c020) - Clone one joint's animation into the weapon-joint copy
// param is a joint index (SCD opcode 0x0F passes 0x0E). Copies the source joint's
// animation slot table into the first buffer allocated by FUN_0048bfe0
// (entity +0xB0), points the destination joint at that buffer and at the second
// buffer (+0xB4), relinks the slot, fixes up the relocated animation-data pointer
// by the buffer delta, reverses the frame order, and builds the anim object.
//
//   source joint      = ENTITY->jointsStructs   (+0x98) + index * 0x7C
//   destination joint = ENTITY->weaponJointsPtr (+0xAC) + index * 0x7C
//
// The copy length is *animSlot - animSlot: the slot table stores its own end
// pointer in the first dword.
// ============================================================================
void FUN_0048c020(int param)
{
    extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);
    extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2);
    extern void reverse_anim_frame_data(int animFieldAddr);

    unsigned char* ent = (unsigned char*)ENTITY;
    int dst = *(int*)(ent + 0xac) + (unsigned int)(unsigned char)param * 0x7c;
    int src = *(int*)(ent + 0x98) + (unsigned int)(unsigned char)param * 0x7c;

    int* animSlot = *(int**)(src + 0x14);
    void* buf0 = *(void**)(ent + 0xb0);
    void* buf1 = *(void**)(ent + 0xb4);
    memcpy(buf0, animSlot, (size_t)(*animSlot - (int)animSlot));

    int slotPtr = dst + 0xc;
    *(void**)(dst + 0x14) = buf0;
    *(void**)(dst + 0x18) = buf1;
    SetAnimSlot((AnimSlot*)buf0, slotPtr, 0);

    int* fixup = (int*)(*(int*)(dst + 0x14) + 0x10);
    *fixup += *(int*)(dst + 0x14) - *(int*)(src + 0x14);

    reverse_anim_frame_data(slotPtr);
    CreateAnimObject(slotPtr, (unsigned int*)buf1);
}

// ============================================================================
// FUN_0040c560 (0x0040c560) - Store the low bit of the parameter into g_bCostumeVariant
// SCD opcode 0x4F writes this flag; opcode 0x50 (cmd_costume_variant_test) returns it as its
// condition result, so a script can set a flag with 0x4F and branch on it later.
// ============================================================================
void FUN_0040c560(int param)
{
    g_bCostumeVariant = (unsigned char)param & 1;
}

// ============================================================================
// g_ScdAnimRemap (0x004bec80)
// Animation remap table for SCD event state-1 opcode 0x89 (set animation frame).
// 16 (actionStateBase, animationId) pairs indexed by the entity's incoming
// animationId; the handler writes action_state = pair[0] + 1 and
// animationId = pair[1]. Only applied for entity ids < 0x20 and animationId
// <= 0x0F - the handler forces action_state = 3 above that, which is why the
// table is 32 bytes with pairs 10-15 left zero in the original.
//
//   anim: 0     1     2     3     4     5     6     7     8     9
//   pair: (0,0) (0,1) (0,2) (0,3) (0,4) (1,0) (1,1) (1,2) (1,3) (1,4)
// ============================================================================
extern const unsigned char g_ScdAnimRemap[32] = {
    0, 0,   0, 1,   0, 2,   0, 3,   0, 4,
    1, 0,   1, 1,   1, 2,   1, 3,   1, 4,
    0, 0,   0, 0,   0, 0,   0, 0,   0, 0,   0, 0,
};
