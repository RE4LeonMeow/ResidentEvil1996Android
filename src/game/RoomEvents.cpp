// RoomEvents.cpp - SCD room event system (decompiled from Ghidra)
// Implements room_events_check and all direct dependencies.
#include "../Globals.h"
#include "../DebugPrint.h"
#include "dc/Items.h"    // lockpick item id the DC moved to 0x0D
#include <cstring>

// Forward declaration
void run_command_functions(unsigned short* scd_opcodes);

// ============================================================================
// ScdEventEntry_Init (0x0041d620)
// Initializes an SCD event entry: sets active, clears state, loads script
// pointer from the event scripts table, and assigns the current entity.
// ============================================================================
static void ScdEventEntry_Init(ScdEventEntry* entry, int scriptIndex)
{
    entry->active = 1;
    entry->state = 0;
    entry->scriptPtr = ((unsigned char**)g_RoomEventScripts)[scriptIndex];
    entry->stackDepth = 0xFF;
    entry->entity = ENTITY;
}

// ============================================================================
// ScdEventEntry_Create (0x0041d650)
// Creates a new SCD event entry. If slot > 7, finds the first free slot.
// Externally visible: SCD command opcode 0x14 (cmd_scd_event_create) calls this. It must
// NOT be static - an empty placeholder in GameState.cpp used to satisfy that
// call instead, so opcode 0x14 silently created no event.
// ============================================================================
void ScdEventEntry_Create(unsigned int slot, int scriptIndex)
{
    if (slot > 7) {
        slot = 0;
        if (g_ScdEventTable[0].active != 0) {
            unsigned char* pActive = &g_ScdEventTable[0].active;
            do {
                if (pActive == &g_ScdEventTable[7].active) break;
                pActive += sizeof(ScdEventEntry);
                slot++;
            } while (*pActive != 0);
        }
    }
    ScdEventEntry_Init(&g_ScdEventTable[slot], scriptIndex);
}

// ============================================================================
// scd_event_cmd_set_entity (0x0041e520) - SCD opcode 0x04, state 0
// Sets the current entity pointer for the event.
// ============================================================================
static void scd_event_cmd_set_entity(void)
{
    g_pScdEventCurrent->scriptPtr++;
    unsigned char* p = g_pScdEventCurrent->scriptPtr;
    switch (*p) {
    case 0: // Player entity
        g_pScdEventCurrent->entity = (Entity*)&g_playerEntity;
        break;
    case 1: // Enemy/NPC entity
        g_pScdEventCurrent->entity = &g_EnemiesList[p[1]];
        break;
    case 2: // Room OBJECT model (g_omodel_table / RDT object_models) - furniture,
            // statues, boxes, obstacles, doors, the item box's lid, etc.
        g_pScdEventCurrent->entity = (Entity*)g_omodel_table[p[1]];
        break;
    case 3: // Item model (g_item_model_table) - the room's pick-up models
        g_pScdEventCurrent->entity = (Entity*)g_item_model_table[p[1]];
        break;
    }
    g_pScdEventCurrent->scriptPtr += 2;
}

// ============================================================================
// scd_event_cmd_create (0x0041e5d0) - SCD opcode 0x05, state 0
// Creates a new event entry from script data.
// ============================================================================
static void scd_event_cmd_create(void)
{
    g_pScdEventCurrent->scriptPtr++;
    ScdEventEntry_Create(
        *g_pScdEventCurrent->scriptPtr,
        g_pScdEventCurrent->scriptPtr[1]
    );
    g_pScdEventCurrent->scriptPtr += 3;
}

// ============================================================================
// scd_event_cmd_init (0x0041e600) - SCD opcode 0x08, state 0
// Re-initializes the current event entry with a new script.
// ============================================================================
static void scd_event_cmd_init(void)
{
    ScdEventEntry_Init(g_pScdEventCurrent, g_pScdEventCurrent->scriptPtr[1]);
}

// ============================================================================
// run_command_functions (0x00473f60)
// Main SCD script interpreter. Processes blocks of SCD opcodes from the room
// initialization script. Each block starts with a size word. Opcodes are
// dispatched through the script_command_funcs_table.
// ============================================================================
// g_ScdBranchStack (0x00bf0808) - resume-address stack for cmd_if / cmd_else /
// cmd_end_if. 16 dwords in the original: the region runs from 0x00bf0808 up to
// g_pScdEventCurrent at 0x00bf0848 with no other global in between. An earlier
// revision declared only 8, so a script nesting more than 8 conditionals deep
// would have written past the end.
// The original's table has no padding and no bound: opcodes above 0x50 run whatever
// bytes follow it. The port declares 256 entries with 0x51-0xFF nullptr, so a bad
// opcode faults at address 0 instead. Report which opcode and where, because a NULL
// call here means the SCD stream itself is wrong (stale pointer, unloaded room, or a
// desynced instruction pointer) - the opcode is a symptom, not the cause.
static int scd_dispatch(const char* site)
{
    typedef int (*ScdCmdFunc)(void);
    unsigned char op = *g_ScdOpcodes;
    void* fn = script_command_funcs_table[op];
    if (fn == nullptr) {
        dbg_printf("[scd] NULL command 0x%02X at %s (g_ScdOpcodes=%p) - aborting stream\n",
                   (unsigned int)op, site, (void*)g_ScdOpcodes);
        return 0;
    }
    return ((ScdCmdFunc)fn)();
}

static unsigned int g_ScdBranchStack[16];

void run_command_functions(unsigned short* scd_opcodes)
{
    typedef int (*ScdCmdFunc)(void);

    g_CmdOpcodesPointer = g_ScdBranchStack;
    g_ScriptContinueFlag = 0;
    unsigned short blockSize = *scd_opcodes;

    while (blockSize != 0) {
        g_ScdOpcodes = (unsigned char*)(scd_opcodes + 1);

        while (true) {
            int cmdResult;
            do {
                cmdResult = scd_dispatch("run_command_functions");
            } while (cmdResult != 0);

            if (g_ScriptContinueFlag == 0) break;

            g_CmdOpcodesPointer--;
            g_ScdOpcodes = (unsigned char*)*g_CmdOpcodesPointer;
            g_ScriptContinueFlag--;
        }

        scd_opcodes = (unsigned short*)((unsigned char*)scd_opcodes + (unsigned int)blockSize);
        blockSize = *scd_opcodes;
    }
    g_ScdOpcodes = (unsigned char*)scd_opcodes;
}

// ============================================================================
// scd_event_cmd_run_scd (0x0041e620) - SCD opcode 0x06, state 0
// Runs inline SCD commands via run_command_functions.
// ============================================================================
static void scd_event_cmd_run_scd(void)
{
    run_command_functions((unsigned short*)(g_pScdEventCurrent->scriptPtr + 2));
    g_pScdEventCurrent->scriptPtr += g_pScdEventCurrent->scriptPtr[1];
}

// ============================================================================
// scd_event_cmd_exec (0x0041e650) - SCD opcode 0x07, state 0
// Executes a single SCD command from the event script.
// Sets up g_ScdOpcodes from the event script data, advances the script
// pointer past the command block, and dispatches through the SCD command
// function table.
// ============================================================================
static void scd_event_cmd_exec(void)
{
    unsigned char* scriptData = g_pScdEventCurrent->scriptPtr;
    unsigned short header = *(unsigned short*)scriptData;

    g_ScdOpcodes = scriptData + 2;
    g_pScdEventCurrent->scriptPtr = scriptData + (header >> 8);

    typedef int (*ScdCmdFunc)(void);
    scd_dispatch("scd_event_cmd_exec");
}

// ============================================================================
// scd_event_state3_set_behavior (0x0041e150) - State 3 handler
// Sets the entity's action behavior from the script byte.
// ============================================================================
static int scd_event_state3_set_behavior(void)
{
    g_PlayerDpadHeld = 0xFF;
    if (*g_pScdEventCurrent->scriptPtr != g_pScdEventCurrent->entity->action_behavior) {
        g_pScdEventCurrent->entity->action_state = 0;
    }
    g_pScdEventCurrent->entity->action_behavior = *g_pScdEventCurrent->scriptPtr;
    g_pScdEventCurrent->scriptPtr += 2;
    return 1;
}

// ============================================================================
// scd_event_state2_movement (0x0041e1a0) - State 2 handler
// Processes entity movement/position commands from the SCD event script.
// ============================================================================
static void scd_event_state2_movement(void)
{
    unsigned short* opcodes = (unsigned short*)g_pScdEventCurrent->scriptPtr;
    Entity* ent = g_pScdEventCurrent->entity;

    switch ((unsigned char)*opcodes) {
    case 0x00: // NOP
        g_pScdEventCurrent->scriptPtr++;
        return;

    case 0x01: // Return to state 0
        g_pScdEventCurrent->state = 0;
        g_pScdEventCurrent->scriptPtr++;
        return;

    case 0x02: // Apply speed to position
        ent->scaMatrixData.localMatrix.t[0] += (int)ent->speed.x;
        ent->scaMatrixData.localMatrix.t[1] += (int)ent->speed.y;
        ent->scaMatrixData.localMatrix.t[2] += (int)ent->speed.z;
        g_pScdEventCurrent->scriptPtr++;
        return;

    case 0x03: // Apply rotation steps
        ent->position.pad += ent->move_step_x;
        *(short*)&ent->angle = *(short*)&ent->angle + ent->move_step_z;
        *((short*)&ent->angle + 1) = *((short*)&ent->angle + 1) + *(short*)&ent->state;
        g_pScdEventCurrent->scriptPtr++;
        return;

    case 0x04: // Apply speed AND rotation steps
        ent->scaMatrixData.localMatrix.t[0] += (int)ent->speed.x;
        ent->scaMatrixData.localMatrix.t[1] += (int)ent->speed.y;
        ent->scaMatrixData.localMatrix.t[2] += (int)ent->speed.z;
        ent->position.pad += ent->move_step_x;
        *(short*)&ent->angle = *(short*)&ent->angle + ent->move_step_z;
        *((short*)&ent->angle + 1) = *((short*)&ent->angle + 1) + *(short*)&ent->state;
        g_pScdEventCurrent->scriptPtr++;
        return;

    case 0x05: // Set speed (3 signed bytes)
        ent->speed.x = (short)(signed char)g_pScdEventCurrent->scriptPtr[1];
        ent->speed.y = (short)(signed char)g_pScdEventCurrent->scriptPtr[2];
        ent->speed.z = (short)(signed char)g_pScdEventCurrent->scriptPtr[3];
        g_pScdEventCurrent->scriptPtr += 4;
        return;

    case 0x06: // Set rotation steps + state (4 signed bytes)
        ent->move_step_x = (short)(signed char)g_pScdEventCurrent->scriptPtr[1];
        ent->move_step_z = (short)(signed char)g_pScdEventCurrent->scriptPtr[2];
        {
            unsigned char b = g_pScdEventCurrent->scriptPtr[3];
            ent->state = (unsigned char)(short)(signed char)b;
            ent->ignore_player_flag = (unsigned char)((unsigned short)(short)(signed char)b >> 8);
        }
        g_pScdEventCurrent->scriptPtr += 4;
        return;

    case 0x07: // Set absolute position (3 shorts)
        ent->scaMatrixData.localMatrix.t[0] = (int)(short)opcodes[1];
        ent->scaMatrixData.localMatrix.t[1] = (int)*(short*)(g_pScdEventCurrent->scriptPtr + 4);
        ent->scaMatrixData.localMatrix.t[2] = (int)*(short*)(g_pScdEventCurrent->scriptPtr + 6);
        g_pScdEventCurrent->scriptPtr += 8;
        return;

    case 0x08: // Store one byte into the entity's state block
        // 0x0041e3b6:
        //   MOV DL,byte ptr [ECX + 0x2]          ; value  = script[2]
        //   MOVZX EBX,byte ptr [ECX + 0x1]       ; offset = script[1]
        //   MOV ECX,dword ptr [EAX + 0x4]        ; ECX    = entity
        //   MOV byte ptr [EBX + ECX*0x1 + 0x84],DL
        // A single BYTE store at entity + 0x84 + script[1] - the state block
        // (state / ignore_player_flag / action_behavior / action_state / health /
        // hit_state / ...). The old code used base 0x34 (localMatrix.t) and a
        // 32-bit store, so this opcode overwrote four bytes of the transform
        // instead of one state byte.
        ((unsigned char*)ent)[0x84 + g_pScdEventCurrent->scriptPtr[1]] =
            g_pScdEventCurrent->scriptPtr[2];
        g_pScdEventCurrent->scriptPtr += 3;
        return;

    case 0x09: // Set hit_state
        ent->hit_state = g_pScdEventCurrent->scriptPtr[1];
        g_pScdEventCurrent->scriptPtr += 2;
        return;

    case 0x0A: // Store one entity field selected by the operand byte
        // Inner switch at 0x0041e3fa: selector = script[1] (`MOV AX,[ECX] / SHR
        // AX,0x8`), value = the word at script+2. Selectors 0-2 store a
        // SIGN-EXTENDED 32-bit value (`MOVSX EAX,word ptr [ECX+0x2]`); selectors
        // 3-5 store the raw 16-bit word (`MOV word ptr [EDX],AX`). Every path
        // advances 4.
        switch (*opcodes >> 8) {
        case 0: ent->scaMatrixData.localMatrix.t[0] = (int)(short)opcodes[1]; break;
        case 1: ent->scaMatrixData.localMatrix.t[1] = (int)(short)opcodes[1]; break;
        case 2: ent->scaMatrixData.localMatrix.t[2] = (int)(short)opcodes[1]; break;
        case 3: ent->health = (short)opcodes[1];   break;   // entity +0x88
        case 4: ent->unk_c6 = opcodes[1];          break;   // entity +0xC6
        case 5: ent->unk_c8 = opcodes[1];          break;   // entity +0xC8
        default:
            // `CMP EAX,0x5 / JA 0x0041e46e`, and 0x0041e46e is
            // `MOV EDX,dword ptr [ESP+0x4]` - the destination pointer is read
            // from a stack slot that is NEVER written on this path (only the
            // in-range cases 0-2 reach the store, and they set EDX directly).
            // The original therefore stores through an uninitialised pointer for
            // any selector above 5. Deliberately NOT reproduced: the port skips
            // the store and advances like every other path.
            break;
        }
        g_pScdEventCurrent->scriptPtr += 4;
        return;

    case 0x0B: // Set the entity's rotation SVECTOR absolutely
        // 0x0041e486: three WORD stores at entity +0x72 / +0x74 / +0x76 from the
        // script words at +2 / +4 / +6, then `ADD dword ptr [EAX+0x8],0x8`.
        // This is the rotation sibling of case 0x07 (absolute position) and is a
        // case of its OWN - it does not share case 0x0A's field-selector path.
        // The old code ran 0x0A's parametric store first and then advanced
        // 4 + 8 = 12, desyncing the script stream by 4 bytes for every use.
        ent->position.pad               = opcodes[1];   // +0x72 rotation .x
        *(unsigned short*)&ent->angle   = opcodes[2];   // +0x74 rotation .y (yaw)
        *(unsigned short*)&ent->angle_z = opcodes[3];   // +0x76 rotation .z
        g_pScdEventCurrent->scriptPtr += 8;
        return;

    default:
        return;
    }
}

// ============================================================================
// scd_event_state1_anim (0x0041da30) - State 1 handler
// Processes animation/movement commands while entity is animating.
// Returns 1 to continue processing, 0 to stop (wait for animation).
// ============================================================================
static int scd_event_state1_anim(void)
{
    unsigned short* opcodes = (unsigned short*)g_pScdEventCurrent->scriptPtr;
    Entity* ent = g_pScdEventCurrent->entity;

    switch ((unsigned char)*opcodes) {
    case 0x00: // Advance script pointer
        g_pScdEventCurrent->scriptPtr++;
        return 1;

    case 0x80: // End animation wait + clear ignore_player_flag
        ent->ignore_player_flag = 0;
        // fall through
    case 0x8B: // End animation wait
        g_pScdEventCurrent->pad_01 = 0;
        g_pScdEventCurrent->scriptPtr++;
        g_pScdEventCurrent->state = 0;
        break;

    case 0x81: // Set entity behavior type and target
        {
            unsigned short val = *opcodes;
            g_pScdEventCurrent->scriptPtr = (unsigned char*)(opcodes + 1);
            ent->lookAtFlags = (unsigned char)(val >> 8);

            if ((ent->lookAtFlags & 0xF) != 0) {
                g_pScdEventCurrent->scriptPtr += 8;

                if (ent->lookAtFlags == 0x93) {
                    // Target mode: set scd_target_ptr based on type.
                    //
                    // The selector is the word at +2 and the target index is the
                    // SIGNED word at +4 (0x0041dad1: MOVSX EAX,[EDX] with
                    // EDX = base+2, then LEA ECX,[EDX+2] for the index). An
                    // earlier revision read the index from +10 - the position
                    // the script pointer has already been advanced to - which
                    // indexed the enemy list with unrelated script bytes.
                    //
                    // Type 0 stores the player entity's own address: the original
                    // is MOV dword ptr [ESI+0xb8],0xbe62e4, and 0x00be62e4 is
                    // g_playerEntity's base (update_player_anim reaches its
                    // position field as 0x00be6318 = base+0x34). Ghidra renders
                    // that as &g_playerEntityPointer only because it named the
                    // entity object itself "g_playerEntityPointer".
                    short targetIndex = (short)opcodes[2];
                    switch (*(opcodes + 1)) {
                    case 0:
                        ent->scd_target_ptr = (unsigned int)&g_playerEntity;
                        break;
                    case 1:
                        ent->scd_target_ptr = (unsigned int)&g_EnemiesList[targetIndex];
                        break;
                    case 2:
                        ent->scd_target_ptr = (unsigned int)g_omodel_table[targetIndex];
                        break;
                    case 3:
                        ent->scd_target_ptr = (unsigned int)g_item_model_table[targetIndex];
                        break;
                    }
                } else {
                    // Position mode: set scd_pos_x/y/z
                    if ((ent->lookAtFlags & 0x20) == 0) {
                        ent->scd_pos_x = (int)(short)*(opcodes + 1);
                        ent->scd_pos_y = (int)(short)opcodes[2];
                    } else {
                        int x = (int)(short)*(opcodes + 1);
                        if (x < 0) x += 0x1000;
                        ent->scd_pos_x = x;
                        int y = (int)(short)opcodes[2];
                        if (y < 0) y += 0x1000;
                        ent->scd_pos_y = y;
                    }
                    ent->scd_pos_z = (int)(short)opcodes[3];
                }

                // Set step size and flags. Both bytes come from the word at
                // +8: the original walks a separate cursor (ECX) that ends at
                // base+8 in every branch, while scriptPtr is already at +10.
                unsigned short stepFlags = opcodes[4];
                unsigned char step = (unsigned char)stepFlags;
                ent->lookAtYawStep = step ? step : 0xC0;

                if ((stepFlags & 0xFF00) != 0) {
                    ent->lookAtPitchStep = (unsigned char)(stepFlags >> 8);
                } else {
                    ent->lookAtPitchStep = 0x40;
                }
                return 1;
            }
        }
        break;

    case 0x82: // Clear behavior type bit 4
        ent->lookAtFlags &= ~0x10;
        g_pScdEventCurrent->scriptPtr++;
        return 1;

    case 0x83: // Set entity state + animation with full params
        ent->state = 8;
        ent->ignore_player_flag = 0;
        {
            unsigned short val = *opcodes;
            unsigned short hi = val >> 8;
            if ((val & 0x1000) == 0) {
                ent->action_behavior = (unsigned char)hi;
                ent->action_state = (unsigned char)(hi >> 8);
                ent->collisionFlags &= 0x7F;
            } else {
                if ((ent->collisionFlags & 0x80) == 0) {
                    ent->action_behavior = (unsigned char)(hi & 0xF);
                    ent->action_state = (unsigned char)((hi & 0xF) >> 8);
                    ent->collisionFlags |= 0x80;
                } else {
                    ent->action_behavior = (unsigned char)(hi & 0xF);
                }
            }
        }
        g_pScdEventCurrent->scriptPtr += 2;
        // Read animation parameters (3 more pairs of bytes)
        ent->unk_c6 = g_pScdEventCurrent->scriptPtr[0] | (g_pScdEventCurrent->scriptPtr[1] << 8);
        g_pScdEventCurrent->scriptPtr += 2;
        ent->unk_c8 = g_pScdEventCurrent->scriptPtr[0] | (g_pScdEventCurrent->scriptPtr[1] << 8);
        g_pScdEventCurrent->scriptPtr += 2;
        ent->scd_anim_param = *g_pScdEventCurrent->scriptPtr;
        g_pScdEventCurrent->scriptPtr++;
        ent->scd_timer = 0x28;
        ent->scd_entity_flags = 0;
        return 1;

    case 0x84: // Set entity state + simple animation
        ent->state = 8;
        ent->ignore_player_flag = 0;
        ent->action_behavior = 1;
        ent->action_state = 0;
        {
            unsigned short animParam = *(unsigned short*)g_pScdEventCurrent->scriptPtr;
            g_pScdEventCurrent->scriptPtr += 2;
            unsigned short animData = *(unsigned short*)g_pScdEventCurrent->scriptPtr;
            g_pScdEventCurrent->scriptPtr += 2;
            ent->animationId = (unsigned char)(animParam >> 8);
            ent->scd_anim_param = (unsigned char)animData;
            ent->scd_entity_flags = (unsigned short)((animData >> 6) & 0x3FC);
            // 0x0041ddae: MOV word ptr [EDI+0xde],0 - clears both timer bytes
            ent->scd_timer = 0;
        }
        return 1;

    case 0x85: // Set entity state + behavior + animation
        {
            unsigned short val = *opcodes;
            g_pScdEventCurrent->scriptPtr = (unsigned char*)(opcodes + 1);
            unsigned short animParam = *(unsigned short*)g_pScdEventCurrent->scriptPtr;
            g_pScdEventCurrent->scriptPtr += 2;
            ent->state = 8;
            ent->ignore_player_flag = 0;
            ent->action_behavior = (unsigned char)(val >> 8);
            ent->action_state = (unsigned char)((val >> 8) >> 8);
            ent->animationId = (unsigned char)animParam;
            ent->scd_anim_param = (unsigned char)(animParam >> 8);
            ent->scd_timer = 0;
            ent->scd_entity_flags = 0;
        }
        return 1;

    case 0x86: // Set entity state to idle
        ent->state = 1;
        ent->ignore_player_flag = 0;
        ent->action_behavior = 0;
        ent->action_state = 0;
        ent->hit_state = 0;
        g_pScdEventCurrent->scriptPtr++;
        return 1;

    case 0x87: // Set entity flags (OR/SET/XOR)
        {
            unsigned short val = *opcodes;
            g_pScdEventCurrent->scriptPtr = (unsigned char*)(opcodes + 1);
            unsigned short mode = val >> 8;
            unsigned short flagVal = *(unsigned short*)g_pScdEventCurrent->scriptPtr;
            g_pScdEventCurrent->scriptPtr += 2;
            if (mode == 0) {
                ent->scd_entity_flags |= flagVal;
            } else if (mode == 1) {
                ent->scd_entity_flags = flagVal;
            } else if (mode == 2) {
                ent->scd_entity_flags ^= flagVal;
            }
        }
        return 1;

    case 0x88: // Set the SCD animation timer (16-bit, little-endian in script)
        g_pScdEventCurrent->scriptPtr = (unsigned char*)(opcodes + 1);
        ent->scd_timer = (unsigned short)(g_pScdEventCurrent->scriptPtr[0]
                                       | (g_pScdEventCurrent->scriptPtr[1] << 8));
        g_pScdEventCurrent->scriptPtr += 2;
        return 1;

    case 0x89: // Set animation frame with entity-specific behavior
        {
            unsigned short val = *opcodes;
            g_pScdEventCurrent->scriptPtr = (unsigned char*)(opcodes + 1);
            ent->animation_frame_id = (unsigned char)(val >> 8);
            ent->timing_control = 0;
            ent->blend_counter = 7;
            ent->move_speed_current = 0;
            if ((ent->scd_entity_flags & 0x20) != 0) {
                ent->blend_counter = 0;
            }
            if (ent->id < 0x20) {
                if (ent->animationId > 0x0F) {
                    ent->action_state = 3;
                } else {
                    // g_ScdAnimRemap (0x004bec80): (actionStateBase, animationId) pairs
                    ent->action_state = g_ScdAnimRemap[(unsigned int)ent->animationId * 2] + 1;
                    ent->animationId = g_ScdAnimRemap[(unsigned int)ent->animationId * 2 + 1];
                }
            } else {
                ent->action_state = 1;
            }
        }
        return 1;

    case 0x8A: // Set animation frame (simple, no lookup)
        {
            unsigned short val = *opcodes;
            g_pScdEventCurrent->scriptPtr = (unsigned char*)(opcodes + 1);
            ent->animation_frame_id = (unsigned char)(val >> 8);
            ent->timing_control = 0;
            ent->blend_counter = 7;
            ent->move_speed_current = 0;
            if ((ent->scd_entity_flags & 0x20) != 0) {
                ent->blend_counter = 0;
                return 1;
            }
        }
        return 1;

    default:
        break;
    }
    return 1;
}

// ============================================================================
// room_events_check (0x0041d6a0)
// Main SCD event processing loop. Called every frame from game_loop and once
// during room_set. Iterates through all 8 event entries and processes their
// SCD scripts based on current state.
//
// Control flow opcodes (0xF6-0xFF):
//   0xF6 - Push loop counter (fall through to F7)
//   0xF7 - Wait: block while MSF_VOICE_PLAYING (0x20000) or a message is up
//   0xF8 - Push counter with value from script
//   0xF9 - Decrement counter, skip if zero
//   0xFA - Loop start with counter
//   0xFB - Loop end (decrement/branch)
//   0xFC - Call subroutine
//   0xFD - Call SCD command from call stack
//   0xFE - Advance script pointer (NOP)
//   0xFF - Deactivate event
//
// State 0 opcodes (0x00-0x09):
//   0x00 - NOP
//   0x01 - Enter state 1 (wait animation)
//   0x02 - Enter state 2 (movement) + setup entity
//   0x03 - Enter state 2 (movement)
//   0x04 - Set current entity
//   0x05 - Create new event
//   0x06 - Run SCD commands inline
//   0x07 - Execute SCD command
//   0x08 - Reinitialize current event
//   0x09 - Deactivate another event
// ============================================================================

void room_events_check(void)
{
    int result;

    // 0x0041d6a0: Check if message display flag bit 7 is set
    if ((g_message_flags & 0x80) == 0) {
        return;
    }

    ScdEventEntry* entry = g_ScdEventTable;

    do {
        g_pScdEventCurrent = entry;

        if (entry->active != 0) {
            // Main event processing loop.
            //
            // event_dispatch corresponds to switchD_0041d6f9_default in the
            // original, which sits at the TOP of this loop: every opcode that
            // completes without yielding re-enters the dispatcher and runs the
            // NEXT opcode of the SAME entry in the same frame. An earlier
            // revision put the label down at the entry advance, so each event
            // executed exactly one opcode per frame and the end-of-script check
            // never ran - which is what made scripted sequences crawl or stall.
            // Only 0xF6-0xF9, 0xFE, 0xFF and state-0 opcode 0x08 yield the frame.
event_dispatch:
            do {
                unsigned char* scriptByte = g_pScdEventCurrent->scriptPtr;

                // Process control flow opcodes (0xF6-0xFF)
                switch (*scriptByte) {
                case 0xF6: // Push loop counter (fall through to wait)
                    g_pScdEventCurrent->stackDepth++;
                    g_pScdEventCurrent->scriptPtr++;
                    // fall through
                case 0xF7: // Wait for the voice line / message to finish
                    // 0x0041d918: TEST byte ptr [0x00be41c2],0x2 - byte 2 of
                    // g_main_state_flags, so the polled bit is 0x00020000 =
                    // MSF_VOICE_PLAYING, the flag cmd_voice_play (0x1E) raises and
                    // UpdateMusicWaitState / play_sound_and_voice_effect type 2
                    // clear.
                    if (((g_main_state_flags & MSF_VOICE_PLAYING) == 0) &&
                        ((g_menu_choice_id & 0x80) == 0)) {
                        g_pScdEventCurrent->scriptPtr++;
                        g_pScdEventCurrent->stackDepth--;
                    }
                    goto event_next_entry;

                case 0xF8: // Push counter with value
                    g_pScdEventCurrent->stackDepth++;
                    g_pScdEventCurrent->counterStack[(signed char)g_pScdEventCurrent->stackDepth] =
                        *(short*)(g_pScdEventCurrent->scriptPtr + 2);
                    g_pScdEventCurrent->scriptPtr++;
                    // fall through
                case 0xF9: // Decrement counter
                    g_pScdEventCurrent->counterStack[(signed char)g_pScdEventCurrent->stackDepth]--;
                    if (g_pScdEventCurrent->counterStack[(signed char)g_pScdEventCurrent->stackDepth] == 0) {
                        g_pScdEventCurrent->scriptPtr += 3;
                        g_pScdEventCurrent->stackDepth--;
                    }
                    goto event_next_entry;

                case 0xFA: // Loop start with counter
                    g_pScdEventCurrent->stackDepth++;
                    g_pScdEventCurrent->counterStack[(signed char)g_pScdEventCurrent->stackDepth] =
                        *(short*)(g_pScdEventCurrent->scriptPtr + 2);
                    g_pScdEventCurrent->scriptPtr += 4;
                    g_pScdEventCurrent->returnStack[(signed char)g_pScdEventCurrent->stackDepth] =
                        (unsigned int)g_pScdEventCurrent->scriptPtr;
                    goto event_dispatch;

                case 0xFB: // Loop end
                    g_pScdEventCurrent->counterStack[(signed char)g_pScdEventCurrent->stackDepth]--;
                    if (g_pScdEventCurrent->counterStack[(signed char)g_pScdEventCurrent->stackDepth] == 0) {
                        g_pScdEventCurrent->scriptPtr++;
                        g_pScdEventCurrent->stackDepth--;
                    } else {
                        g_pScdEventCurrent->scriptPtr =
                            (unsigned char*)g_pScdEventCurrent->returnStack[(signed char)g_pScdEventCurrent->stackDepth];
                    }
                    goto event_dispatch;

                case 0xFC: // Call subroutine
                    g_pScdEventCurrent->stackDepth++;
                    g_pScdEventCurrent->callStack[(signed char)g_pScdEventCurrent->stackDepth] =
                        (unsigned int)(g_pScdEventCurrent->scriptPtr + 2);
                    g_pScdEventCurrent->scriptPtr =
                        g_pScdEventCurrent->scriptPtr + g_pScdEventCurrent->scriptPtr[1];
                    g_pScdEventCurrent->returnStack[(signed char)g_pScdEventCurrent->stackDepth] =
                        (unsigned int)g_pScdEventCurrent->scriptPtr;
                    goto event_dispatch;

                case 0xFD: // Call SCD command from call stack
                    g_ScdOpcodes = (unsigned char*)g_pScdEventCurrent->callStack[(signed char)g_pScdEventCurrent->stackDepth];
                    typedef int (*ScdCmdFunc)(void);
                    result = scd_dispatch("event 0xFD");
                    if (result == 0) {
                        g_pScdEventCurrent->scriptPtr++;
                        g_pScdEventCurrent->stackDepth--;
                    } else {
                        g_pScdEventCurrent->scriptPtr =
                            (unsigned char*)g_pScdEventCurrent->returnStack[(signed char)g_pScdEventCurrent->stackDepth];
                    }
                    goto event_dispatch;

                case 0xFF: // Deactivate event
                    g_pScdEventCurrent->active = 0;
                    // fall through
                case 0xFE: // Advance pointer (NOP)
                    g_pScdEventCurrent->scriptPtr++;
                    goto event_next_entry;
                }

                // Process state-specific opcodes
                switch (g_pScdEventCurrent->state) {
                case 0: {
                    switch (*scriptByte) {
                    case 0x00: // NOP
                        g_pScdEventCurrent->scriptPtr++;
                        break;
                    case 0x01: // Enter state 1 (wait animation)
                        g_pScdEventCurrent->state = 1;
                        g_pScdEventCurrent->scriptPtr++;
                        break;
                    case 0x02: // Enter state 2 + setup entity
                        g_pScdEventCurrent->state = 2;
                        g_pScdEventCurrent->scriptPtr++;
                        g_pScdEventCurrent->entity->ignore_player_flag = 2;
                        g_pScdEventCurrent->entity->action_behavior = 0;
                        g_pScdEventCurrent->entity->action_state = 0;
                        break;
                    case 0x03: // Enter state 2
                        g_pScdEventCurrent->state = 2;
                        g_pScdEventCurrent->scriptPtr++;
                        break;
                    case 0x04: // Set entity
                        scd_event_cmd_set_entity();
                        break;
                    case 0x05: // Create event
                        scd_event_cmd_create();
                        break;
                    case 0x06: // Run SCD inline
                        scd_event_cmd_run_scd();
                        break;
                    case 0x07: // Execute SCD command
                        scd_event_cmd_exec();
                        break;
                    case 0x08: // Reinitialize event
                        scd_event_cmd_init();
                        goto event_next_entry;
                    case 0x09: // Deactivate another event
                        g_ScdEventTable[scriptByte[1]].active = 0;
                        g_pScdEventCurrent->scriptPtr += 2;
                        break;
                    default: // Unknown opcode: deactivate
                        g_pScdEventCurrent->active = 0;
                        return;
                    }
                    goto event_dispatch;
                }
                case 1: // Wait animation state
                    result = scd_event_state1_anim();
                    break;
                case 2: // Movement state
                    scd_event_state2_movement();
                    goto event_dispatch;
                case 3: // Set behavior state
                    result = scd_event_state3_set_behavior();
                    break;
                default:
                    goto event_dispatch;
                }
            } while (result != 0);

        event_next_entry:
            // 0x0041d99d: check for the end-of-script marker before yielding
            if (*g_pScdEventCurrent->scriptPtr == 0xFF) {
                g_pScdEventCurrent->active = 0;
            }
        }

        entry++;
        // Termination: iterate all 8 entries
        if (entry > &g_ScdEventTable[7]) {
            return;
        }
    } while (true);
}

// ============================================================================
// Helpers shared by the room-event state machines below
// ============================================================================
extern void FUN_00473f10(int* baseAddr, unsigned int bitIndex);   // 0x00473f10 CmdFunctions.cpp
extern void Flg_on(int baseAddr, unsigned int bitIndex);          // 0x00473ef0 CmdFunctions.cpp
extern void memset_(unsigned int* dst, int dwordCount);           // 0x0047cf60 CmdFunctions.cpp
extern int  get_item_slot(unsigned char itemId);                  // 0x004516a0 GameStart.cpp
extern void LoadHeldItemsImages(void);                            // 0x00451640 GameStart.cpp
extern void display_room_camera_bg(void);                         // Room.cpp

// ============================================================================
// room_check_actions (0x004b9340)
// Dispatch table for SCD command opcode 0x24 (cmd_room_action) and 0x2D
// (cmd_got_item). 18 real entries (0x00-0x11) followed by two NULL slots in the
// original. Each handler takes a pointer to a 12-byte g_RoomActionTable entry.
//
// Sized correctly here so cmd_room_action's bounds check works; the handler
// bodies live in PlayerAnimations.cpp. Deliberately left nullptr rather than
// filled with empty placeholders - a placeholder with the real name would
// silently overload the real implementation once it lands (see the
// ScdEventEntry_Create / cmd_room_action incident above).
//
// Original entries, in table order:
//   0x00 0041c050  no_room_action          0x01 0041b400  use_mansion_key
//   0x02 0041b630  display_msg_0041b630    0x03 0041b650  include_key
//   0x04 0041b6a0  set_key_flag            0x05 0041b6d0  check_door
//   0x06 0041b790  FUN_0041b790            0x07 0041b850  FUN_0041b850
//   0x08 0041b990  open_itembox            0x09 0041b9e0  FUN_0041b9e0
//   0x0A 0041ba00  FUN_0041ba00            0x0B 0041ba10  FUN_0041ba10
//   0x0C 0041baa0  FUN_0041baa0            0x0D 0041bae0  FUN_0041bae0
//   0x0E 0041bb10  check_desk              0x0F 0041be70  (not yet analyzed)
//   0x10 0041bed0  (not yet analyzed)      0x11 0041bf90  (not yet analyzed)
// ============================================================================
// Entries land here as they are transcribed. The remaining slots stay nullptr on
// purpose: cmd_room_action and update_player_position both null-check, so a missing
// handler is an inert no-op with a diagnostic rather than a jump through garbage.
//
// [1] door_try_enter is the one the dining-room door needs (its event entry has
// act=1). Ghidra called it `use_mansion_key`, which is misleading - that is only one
// of the messages it can emit (0xc3). What it actually does is:
//   - reject the door for the wrong character (lock bit 0x40 + player id&3 == 3)
//   - run the room transition when the door is open, or already unlocked per
//     Flg_ck(g_LocksFlags, lockBits & 0x3f)
//   - otherwise look up the required item at record+0x16 and either consume it and
//     Flg_on the lock, or emit "locked" / "locked from the other side"
// The transition itself is an instant blackout - full-screen black draw_rect,
// Task_sleep(1), StMask(0,0) - not a fade.
void* room_check_actions[ROOM_CHECK_ACTION_COUNT] = {
    /* 0x00 */ (void*)no_room_action,        // 0x0041c050 - returns 0, does nothing
    /* 0x01 */ (void*)door_try_enter,        // 0x0041b400
    /* 0x02 */ (void*)display_msg_room_action, // 0x0041b630
    /* 0x03 */ (void*)include_key,           // 0x0041b650
    /* 0x04 */ (void*)set_key_flag,          // 0x0041b6a0
    /* 0x05 */ (void*)check_door,            // 0x0041b6d0
    /* 0x06 */ (void*)check_door_side,       // 0x0041b790
    /* 0x07 */ (void*)flag_bank_set,         // 0x0041b850
    /* 0x08 */ (void*)open_itembox,          // 0x0041b990
    /* 0x09 */ (void*)create_room_event,     // 0x0041b9e0
    /* 0x0A */ (void*)room_action_noop10,    // 0x0041ba00
    /* 0x0B */ (void*)room_action_effect,    // 0x0041ba10
    /* 0x0C */ (void*)set_stairs_zone,       // 0x0041baa0
    /* 0x0D */ (void*)set_room_event_flag,   // 0x0041bae0
    /* 0x0E */ (void*)check_desk,            // 0x0041bb10
    /* 0x0F */ (void*)pickup_key_event,      // 0x0041be70
    /* 0x10 */ (void*)check_typewriter,      // 0x0041bed0
    /* 0x11 */ (void*)stairs_height_update,  // 0x0041bf90
    /* 0x12 */ nullptr,                      // NULL in the original
    /* 0x13 */ nullptr,                      // NULL in the original
};

// ============================================================================
// room_event_item_pickup (0x00451700)
// Adds the armed room event's item to the inventory. Called by the message
// system (handle_message_post_action, message action 10/0) after the pickup
// prompt is dismissed.
//
// Reads the record at g_pRoomActionEntry+8: +8 = item id, +9 = quantity,
// +0x14 = roomItems flag index, +10 = item model slot. The entry itself is
// deactivated (first byte 0) and the model's opened flag cleared.
//
// Stackable items (ids 0x0b-0x12 and 0x2f) merge into an existing slot first:
// up to the character's slot count ((4 - (id&3)!=1) * 2 - Chris 8, Jill 6),
// capping a slot at 0xfa and spilling the overflow into a new slot. A fresh
// slot records the first free index in g_ItemSlotIndices and raises its bit in
// g_ItemSlotsBitmask, then the menu images are rebuilt.
// ============================================================================
void room_event_item_pickup(void)
{
    unsigned char* evt = (unsigned char*)g_pRoomActionEntry;
    unsigned char* record = *(unsigned char**)(evt + 8);

    *evt = 0;                                     // deactivate the event entry
    ((unsigned char*)g_item_model_table[record[10]])[0] = 0;
    unsigned short fxSlot =
        *(unsigned short*)((char*)g_item_model_table[record[10]] + 0x86);
    if (fxSlot != 0) {
        g_freeEffectSlots++;
        // The original addresses the slot as pool_base + slot*0x21 dwords
        // (0x0045175b: MOV EAX,ECX / SHL ECX,5 / ADD ECX,EAX / LEA EAX,[ECX*4+pool]),
        // i.e. a full 0x84-byte Effect per slot. A dword-stride index cleared bytes
        // 4*slot..4*slot+0x84 - 33x too low - so picking an item zeroed unrelated
        // pool bytes and the item's sparkle billboard kept rendering until the room
        // reload wiped the pool (e.g. ROOM1000's sword key, slot 63).
        memset_((unsigned int*)&g_effectPool[fxSlot], 0x21);
    }
    FUN_00473f10((int*)&g_roomItemsFlags, record[0x14]);

    g_pickedItemId = record[8];
    unsigned char itemId = record[8];
    unsigned char quantity = record[9];
    // ink ribbon always pickup 3 units
    if (itemId == ITEM_INK_RIBBONS) {
        quantity = 3;
    }
    // DC TRAINING and ADVANCED* double the pick-up: the DC's IncludeCurrentItem
    // (SLUS_005.51 0x8002ce28) shifts the record quantity left once for ammo and
    // ink ribbons when the mode bits are set. The USA build has no such branch.
    quantity = dc_item_pickup_quantity(g_selectedItemId, quantity);

    // if ammo or ink ribbon
    if (((ITEM_ROCKET_LAUNCHER < g_selectedItemId) && (g_selectedItemId < ITEM_EMPTY_BOTTLE)) ||
        (g_selectedItemId == ITEM_INK_RIBBONS)) {
        // Stackable: merge into an existing slot of the same item id.
        unsigned char slotCount = (unsigned char)((4 - ((g_playerEntity.id & 3) != 1)) * 2);
        unsigned char idx = 0;
        while (slotCount != 0) {
            unsigned char* slot = (unsigned char*)g_ItemSlotsPointer + (unsigned int)idx * 2;
            if (slot[0] == itemId) {
                unsigned short merged = (unsigned short)(slot[1] + (unsigned short)quantity);
                if (merged < 0xfb) {
                    slot[1] = (unsigned char)merged;
                    return;
                }
                if (g_TotalHeldItems < slotCount) {
                    quantity = (unsigned char)(merged + 6);
                    slot[1] = 0xfa;
                    break;
                }
            }
            slotCount--;
            idx++;
        }
    }

    // New slot.
    ((unsigned char*)g_ItemSlotsPointer)[(unsigned int)g_TotalHeldItems * 2] = itemId;
    ((unsigned char*)g_ItemSlotsPointer)[1 + (unsigned int)g_TotalHeldItems * 2] = quantity;

    unsigned char freeIdx = 0;
    if ((g_ItemSlotsBitmask & 1) != 0) {
        do {
            freeIdx++;
        } while ((g_ItemSlotsBitmask & (1u << (freeIdx & 0x1f))) != 0);
    }
    unsigned int held = (unsigned int)g_TotalHeldItems;
    g_TotalHeldItems++;
    g_ItemSlotIndices[held] = freeIdx;
    g_ItemSlotsBitmask |= 1u << (freeIdx & 0x1f);
    LoadHeldItemsImages();
    StMask(0, 1);
}

// ============================================================================
// room_event_take_item (0x004631c0, was FUN_004631c0)
// Awards the armed room event's item. Either the RADIO (id 0x4D = 'M'), which
// is not an inventory item at all and only raises the player flag that unlocks
// the menu's RADIO tab, or an ordinary item through room_event_item_pickup.
//
// This is the ONLY thing that hands the player an item from a script-driven
// award: the item viewer calls it directly when it closes in menu mode 4
// (item_viewer_update 0x0044e5b7), the mode SCD opcode 0x2D `got_item` selects.
// Mode 3 - the walk-up-and-take pickup - goes the other way round, through
// global message 0xc0's trailing [skip][action 10][case 0] bytes and
// handle_message_post_action, which is why plain pickups kept working while
// every scripted award silently did nothing: this function was an empty
// placeholder in EngineStubs.cpp. The chemical containers in room 4090 consume
// the EMPTY BOTTLE via SCD `item_remove` and award the filled one via
// `got_item`, so the bottle vanished and nothing came back.
// ============================================================================
void room_event_take_item(void)
{
    unsigned char* record = *(unsigned char**)((char*)g_pRoomActionEntry + 8);
    if ((char)record[8] == ITEM_COMM_RADIO) {
        Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_HAS_RADIO);
        return;
    }
    room_event_item_pickup();
}

// ============================================================================
// check_event_item_usage (0x0041c490)
// Per-frame: after a door or desk consumed a key item (g_eventItemUsedFlag
// raised by door_try_enter / use_room_action_item), once the prompt message
// is dismissed, physically remove the item from the inventory.
// ============================================================================
void check_event_item_usage(void)
{
    if ((g_eventItemUsedFlag == 1) && ((g_menu_choice_id & 0x80) == 0)) {
        use_room_action_item();
        g_eventItemUsedFlag = 0;
    }
}

// use_room_action_item (0x004631f0) is implemented in SaveLoadScreen.cpp.

// ============================================================================
// check_itembox_state (0x0041c240)
// Per-frame itembox lid animation. State 1 arms the travel accumulator and
// latches the lid omodel (g_omodel_table[entry+4]); states 2/3
// rotate the lid open past -199 then let it settle back; state 4 (the box
// menu closed) resets. The lid angle lives at omodel+0x76, the step at
// g_counter_increase (reversed at the -199 stop so the lid eases back).
// ============================================================================
void check_itembox_state(void)
{
    switch (g_itembox_state) {
    case 1:
        g_short_itembox_open_timer = 1;
        g_counter_increase = 1;
        g_itembox_state = 2;
        g_itembox_cover_pointer =
            g_omodel_table[*(unsigned short*)((char*)g_pRoomActionEntry + 4)];
        // fall through
    case 2:
        *(short*)((char*)g_itembox_cover_pointer + 0x76) -= g_short_itembox_open_timer;
        g_short_itembox_open_timer = (unsigned short)(g_short_itembox_open_timer + g_counter_increase);
        if (*(short*)((char*)g_itembox_cover_pointer + 0x76) < -199) {
            g_itembox_state = 3;
            g_counter_increase = -g_counter_increase;
        }
        break;
    case 3:
        *(short*)((char*)g_itembox_cover_pointer + 0x76) -= g_short_itembox_open_timer;
        g_short_itembox_open_timer = (unsigned short)(g_short_itembox_open_timer + g_counter_increase);
        if (g_short_itembox_open_timer < 1) {
            // Lid settled: the box menu may open.
            g_main_state_flags |= MSF_MENU_MODE_ITEMBOX;
            g_message_flags = 0xffff;
            g_itembox_state = 4;
            return;
        }
        break;
    case 4:
        g_itembox_state = 0;
        *(short*)((char*)g_itembox_cover_pointer + 0x76) = 0;
        return;
    }
}

// ============================================================================
// check_desk_state (0x0041bc90)
// Per-frame desk flow. States:
//   1/2  - desk is locked: prompt to use the small key (0x3d) or lockpick (0x31)
//   3    - key prompt answered: yes unlocks (LocksFlags bit at entry+2, "key
//          turned" message 0xc3), no just closes
//   4    - desk menu closed: restore the room camera, clear the opened flag
//   5    - open the take-item menu over the desk, re-arm the entry
//   35   - desk camera pan (counts down one per frame through `default`)
// Guardhouse room 003 resets the flow until ScenarioFlags bit 0x7b.
// ============================================================================
extern unsigned int Flg_ck(int baseAddr, unsigned int bitIndex);

void check_desk_state(void)
{
    if ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_003) && ((g_playerEntity.id & 3) == 1) &&
        (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)) {
        g_desk_check_state = 0;
    }

    switch (g_desk_check_state) {
    case 0:
        break;
    case 1:
    case 2:
        // The original stores get_item_slot's result in a write-only scratch
        // global (has_desk_key @ 0x004d6eb4); the call itself is kept for its
        // g_pCurrentItemSlot side effect.
        (void)get_item_slot(ITEM_DESK_KEY);
        g_selectedItemId = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_HAS_LOCKPICK) ?
                           lockpick_item_id() : (unsigned char)ITEM_DESK_KEY;
        set_message_display(0xd9, 0xff);
        g_desk_check_state = 3;
        return;
    case 3:
        if ((g_menu_choice_id & 0x80) == 0) {
            if ((g_menu_choice_id & 1) == 0) {
                // "Yes": unlock and show the key-turned message.
                Flg_on((int)g_LocksFlags, *(unsigned short*)((char*)g_pRoomActionEntry + 2));
                play_sfx(2, 0x26, 0);
                g_selectedItemId = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_HAS_LOCKPICK) ?
                                   lockpick_item_id() : (unsigned char)ITEM_DESK_KEY;
                set_message_display(0xc3, 0xff);
            }
            g_desk_check_state = 0;
            return;
        }
        break;
    case 4:
        display_room_camera_bg();
        g_desk_check_state = 0;
        ((unsigned char*)g_item_model_table[*(unsigned short*)((char*)g_pRoomActionEntry + 4)])[0] &=
            0xfe;
        return;
    case 5:
        // Open the take-item menu; the entry re-arms to the desk's item entry.
        g_main_state_flags |= MSF_MENU_MODE_ITEM_VIEW;
        ((unsigned char*)&g_message_flags)[0] |= 0x45;
        g_desk_check_state = 4;
        g_roomCameraId = g_cutId;
        g_pRoomActionEntry =
            &g_RoomActionTable[*(unsigned short*)((char*)g_pRoomActionEntry + 4) * 12];
        return;
    case 35:
        StMask(0, 1);
        display_room_camera_bg();
        // fall through
    default:
        // Counts the desk camera pan down to 0 (35 -> 0).
        g_desk_check_state--;
        break;
    }
}

// ============================================================================
// check_typewriter_state (0x0041c330)
// Per-frame save-point flow. State 1 prompts "use ink ribbon?" (223) or, for
// Chris before ScenarioFlags bit 0x7b, "save your progress?" (224); state 2
// waits for the choice - no closes, yes fades out; state 3 calls
// LoadSaveGameState with the ribbon slot (entry+2); state 4 waits for the
// fade back in and closes.
// ============================================================================
void check_typewriter_state(void)
{
    switch (g_typewriter_state) {
    case 1:
        g_typewriter_id = *(unsigned short*)((char*)g_pRoomActionEntry + 2);
        if (((g_playerEntity.id == 1) || (g_playerEntity.id == 5)) &&
            (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0)) {
            set_message_display(224, 0xff);   // "Will you save your progress?"
        } else {
            set_message_display(223, 0xff);   // "Will you use the INK RIBBON?"
        }
        g_typewriter_state = 2;
        return;
    case 2:
        if ((g_menu_choice_id & 0x80) == 0) {
            if ((g_menu_choice_id & 1) != 0) {
                g_typewriter_state = 0;
                ((unsigned char*)&g_message_flags)[0] |= 0x45;
                return;
            }
            // "Yes": fade out and load the save screen.
            g_fade_type_id = 2;
            g_fading_counter = 0x1000;
            fade_update();
            g_typewriter_state = 3;
            return;
        }
        break;
    case 3:
        if ((short)g_fading_state < 0) {
            LoadSaveGameState(0, (int)g_loadDataDestPointer, (int)g_typewriter_id + 1, 2, 0);
            g_loadSaveStateFlag = 0;
            cut_set();
            g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
            StMask(1, 0);
            g_fade_type_id = 2;
            g_fading_counter = 0xf000;
            fade_update();
            g_typewriter_state = 4;
            return;
        }
        break;
    case 4:
        if ((short)g_fading_state < 0) {
            g_typewriter_state = 0;
            ((unsigned char*)&g_message_flags)[0] |= 0x45;
        }
        break;
    }
}
