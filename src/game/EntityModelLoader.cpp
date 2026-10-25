// EntityModelLoader.cpp - Entity/player model and animation loading (decompiled)
#include "../Globals.h"
#include "../DebugPrint.h"   // dbg_printf
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include <cstdio>
#include "../system/AssetPath.h"
#include "dc/EntityModels.h"
#include "dc/Items.h"        // DC_ITEM_BERETTA_CUSTOM

// ============================================================================
// Extern data declarations (not yet extracted to Globals.h)
// ============================================================================
// g_entityModelBuffer / g_entityModelBuffer2 are declared in Globals.h as
// references into one contiguous 108544-byte region - see the note there.
extern DWORD g_animObjectBuffer[0x680];        // 0x00c133c0
extern DWORD DAT_004d2bd8;                     // 0x004d2bd8 - special model flag
extern DWORD DAT_004d2bf4;                     // 0x004d2bf4 - TMD processing flag
extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);         // TmdAnimation.cpp
extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2); // TmdAnimation.cpp
extern DWORD g_tmdAsyncData;                   // 0x008fc424 - TMD async data
extern DWORD DAT_004c1a2c;                     // 0x004c1a2c
extern DWORD DAT_00ae9f04;                     // 0x00ae9f04
extern BYTE  g_textureQueueData[40];           // 0x00d22740
extern DWORD g_animSlotIndex;                  // 0x008f8c78
extern DWORD g_textureBankRedirect[32];        // 0x00aae2b0
extern DWORD g_bCostumeVariant;                     // 0x004d6444

// Player/weapon angle globals
extern int g_weaponAngle_Special;              // 0x004c2028
extern int g_weaponAngle_PrimX;                // 0x004c202c
extern int g_weaponAngle_PrimY;                // 0x004c2030
extern int g_weaponAngle_PrimZ;                // 0x004c2034
extern int g_weaponAngle_Sec1X;                // 0x004c2038
extern int g_weaponAngle_Sec1Y;                // 0x004c203c
extern int g_weaponAngle_Sec1Z;                // 0x004c2040
extern int g_weaponAngle_Sec2X;                // 0x004c2044
extern int g_weaponAngle_Sec2Y;                // 0x004c2048
extern int g_weaponAngle_Sec2Z;                // 0x004c204c

extern DWORD g_scaDataTable[4];                // SCA collision data table

// Forward declarations for functions in other files
void InitPlayerEntity(void);
void Object_DeleteAll(int a);
void ComplexTmdObjectSetup(int* param_1);
unsigned int AsyncCreateTmdObject(unsigned int param1, unsigned int param2, unsigned int param3);
int __stdcall VideoDriver_ClearState348(void* obj, void* context);

// ============================================================================
// EMD model path table (0x004c1320)
// 1802 bytes total = 106 entries of 17 bytes, two 53-entry blocks
// (block 0 = Chris scenario, block 1 = Jill scenario).
// Indexed by: base + ((characterId & 1) * 0x35 + entity_id) * 0x11
//
// Each block is 4 player models + 22 enemy models + **10** filler entries +
// 17 character models. The filler run matters: an earlier revision had 9, which
// shifted every character model from index 35 up by one (entity id 37 - Jill -
// loaded Barry's em1022) and put the second block's base one entry early, so the
// Jill scenario was misaligned throughout. Verified against 0x004c1320: index 26
// through 35 are em100a, index 36 is em1020, index 53 is char10, index 79
// through 88 are em110a and index 89 is em1020.
// ============================================================================
static const char g_emdPathTable[106][17] = {
    //  Player
    "enemy/char10.emd", // chris
    "enemy/char11.emd", // jill
    "enemy/char12.emd", // barry
    "enemy/char13.emd", // rebecca
    // Enemies
    "enemy/em1000.emd", // zombie (white coat)
    "enemy/em1001.emd", // naked zombie
    "enemy/em1002.emd", // cerberus
    "enemy/em1003.emd", // Web spinner (big spider)
    "enemy/em1004.emd", // Black Tiger (giant spider)
    "enemy/em1005.emd", // crow
    "enemy/em1006.emd", // hunter
    "enemy/em1007.emd", // wasp
    "enemy/em1008.emd", // Plant 42
    "enemy/em1009.emd", // chimera
    "enemy/em100a.emd", // adder (regular size snake enemy)
    "enemy/em100b.emd", // neptune (zombie shark)
    "enemy/em100c.emd", // Tyrant
    "enemy/em100d.emd", // Yawn (Giant snake)
    "enemy/em100e.emd", // Plant 42 roots
    "enemy/em100f.emd", // Monster plant
    "enemy/em1010.emd", // Tyrant 2
    "enemy/em1011.emd", // Zombie (green coat)
    "enemy/em1012.emd", // Yawn (second encounter)
    "enemy/em1013.emd", // Spider web
    // NPCs
    "enemy/em1014.emd", // Chris's right arm (used for computer keyboard typing)
    "enemy/em1015.emd", // Chris's left arm (used for computer keyboard typing)
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd",
    "enemy/em100a.emd", // 10th filler - index 35
    "enemy/em1020.emd", // chris (regular model) - index 36
    "enemy/em1021.emd", // jill (regular model)
    "enemy/em1022.emd", // barry (regular model)
    "enemy/em1023.emd", // rebecca (regular model)
    "enemy/em1024.emd", // wesker (regular model)
    "enemy/em1025.emd", // Kenneth's corpse
    "enemy/em1026.emd", // Forest's corpse
    "enemy/em1027.emd", // Richard
    "enemy/em1028.emd", // Enrico
    "enemy/em1029.emd", // Kenneth's corpse, devoured state - ROOM1041 intro swaps
                        // slot 1 between ids 37/41/37 at the same X/Z; texture is
                        // em1025's face with extra gore (hanging eye)
    "enemy/em102a.emd", // Barry dying model (Lab exit cutscene)
    "enemy/em102b.emd", // Barry, cutscene anim variant (texture identical to em1022)
    "enemy/em102c.emd", // Rebecca, cutscene anim variant (texture identical to em1023)
    "enemy/em102d.emd", // Barry, cutscene anim variant (texture identical to em1022)
    "enemy/em102e.emd", // Wesker, cutscene anim variant (texture identical to em1024)
    "enemy/em1030.emd", // Chris, alternative outfit 1
    "enemy/em1032.emd", // Chris, alternative outfit 2

    // Jill offset
    "enemy/char10.emd",
    "enemy/char11.emd",
    "enemy/char12.emd",
    "enemy/char13.emd",
    "enemy/em1100.emd", // zombie (white coat)
    "enemy/em1101.emd",
    "enemy/em1102.emd",
    "enemy/em1103.emd",
    "enemy/em1104.emd",
    "enemy/em1105.emd",
    "enemy/em1106.emd",
    "enemy/em1107.emd",
    "enemy/em1108.emd",
    "enemy/em1109.emd",
    "enemy/em110a.emd",
    "enemy/em110b.emd",
    "enemy/em110c.emd",
    "enemy/em110d.emd",
    "enemy/em110e.emd",
    "enemy/em110f.emd",
    "enemy/em1110.emd",
    "enemy/em1111.emd",
    "enemy/em1112.emd",
    "enemy/em1113.emd",
    "enemy/em1114.emd",
    "enemy/em1115.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd",
    "enemy/em110a.emd", // 10th filler - index 88
    "enemy/em1020.emd", // index 89
    "enemy/em1021.emd",
    "enemy/em1022.emd",
    "enemy/em1023.emd",
    "enemy/em1024.emd",
    "enemy/em1025.emd",
    "enemy/em1026.emd",
    "enemy/em1027.emd",
    "enemy/em1028.emd",
    "enemy/em1029.emd",
    "enemy/em102a.emd",
    "enemy/em102b.emd",
    "enemy/em102c.emd",
    "enemy/em102d.emd",
    "enemy/em102e.emd",
    "enemy/em1031.emd", // Jill (alternative outfit 1)
    "enemy/em1033.emd", // Jill (alternative outfit 2)
};

// ============================================================================
// Weapon animation path table (0x004c1ca8)
// 0xe entries per character block, each entry is 0x10 (16) bytes
// Block 0: Chris, Block 1: Jill, Block 2: ?, Block 3: Rebecca
// Indexed by: base + (weapon_id + (characterId & 3) * 0xe) * 0x10
// ============================================================================
static const char g_weaponPathTable[][0xe][16] = {
    {   // Chris block (characterId & 3 == 0)
        "players/w00.emw",
        "players/w01.emw",
        "players/w02.emw",
        "players/w03.emw",
        "players/w04.emw",
        "players/w04.emw",
        "players/w05.emw",
        "players/w06.emw",
        "players/w06.emw",
        "players/w06.emw",
        "players/w07.emw",
        "players/w0b.emw",
        "players/w18.emw",
        "players/w08.emw",
    },
    {   // Jill block (characterId & 3 == 1)
        "players/w10.emw",
        "players/w11.emw",
        "players/w12.emw",
        "players/w13.emw",
        "players/w14.emw",
        "players/w14.emw",
        "players/w15.emw",
        "players/w16.emw",
        "players/w16.emw",
        "players/w16.emw",
        "players/w17.emw",
        "players/w1b.emw",
        "players/w18.emw",
        "players/w08.emw",
    },
    {   // characterId & 3 == 2 (unused in RE1)
        "players/w00.emw",
        "players/w01.emw",
        "players/w02.emw",
        "players/w03.emw",
        "players/w04.emw",
        "players/w04.emw",
        "players/w05.emw",
        "players/w06.emw",
        "players/w06.emw",
        "players/w06.emw",
        "players/w07.emw",
        "players/w0b.emw",
        "players/w18.emw",
        "players/w08.emw",
    },
    {   // Rebecca block (characterId & 3 == 3)
        "players/w30.emw",
        "players/w11.emw",
        "players/w32.emw",
        "players/w13.emw",
        "players/w14.emw",
        "players/w14.emw",
        "players/w15.emw",
        "players/w16.emw",
        "players/w16.emw",
        "players/w16.emw",
        "players/w17.emw",
        "players/w1b.emw",
        "players/w18.emw",
        "players/w08.emw",
    }
};

// TMD texture header struct — defined in TmdAnimation.cpp, declared here for extern visibility
#pragma pack(push, 1)
struct TmdTextureHeader {
    int   count;       short field_04;    short field_06;
    short field_08;    short field_0A;    int   dataPtr;
    short field_10;    short field_12;    short field_14;
    short field_16;    int   ptr_10;
};
#pragma pack(pop)

// ============================================================================
// TMD animation functions — defined in TmdAnimation.cpp
// ============================================================================
extern void ResolveAnimPointers(unsigned char* data);
extern void SetAnimSlot(AnimSlot* slots, int slotPtr, int index);
extern unsigned int FindMinClutDepth(AnimSlot* slot);
extern unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2);
extern unsigned int ProcessTmdTextures(char param1, unsigned int* param2, int param3, int param4);
extern void ClearTmdProcessingFlag(void);
extern void SetSpriteBufferFlag(void);
extern unsigned char QueueTextureForProcessing(char param1, unsigned char param2);
extern void ParseTmdTextureHeader(void* data, TmdTextureHeader* header);
extern void TmdProcessingCallback(void);
extern void ProcessTmdAsync(unsigned int param1);

// ============================================================================
// FUN_00462790 (0x00462790) - Adjust weapon animation positions
// Bakes a fixed rotation into the three special-weapon aim motions (5 = aim
// neutral, 6 = aim down, 7 = aim up) of the freshly loaded w18/w08.EMW so the
// rocket launcher / machinegun arms line up with the weapon model.
// param1: 0 = w18 (rocket launcher, joints 10+11), 1 = w08 (MINIMI, one axis).
//
// EMW/EMR header (all u16): +0 armature offset, +2 FRAME DATA BASE, +4 joint
// count, +6 FRAME STRIDE. For every w*.EMW that is 100 / 176 / 15 / 104, so
// frame f lives at animBuffer + 104*f + 176 and the +0x42..+0x4c / +0x64
// writes below are joint angles inside that frame.
// ============================================================================
void AdjustWeaponAnimationPositions(int param1)
{
    unsigned int animBuffer = g_playerEntity.jointMoveData0;
    unsigned int animEnd = g_playerEntity.jointMoveData1;
    int local_8 = 5;

    do {
        unsigned int uVar5 = *(unsigned int*)(animEnd + local_8 * 4);
        unsigned int* puVar6 = (unsigned int*)((((int)uVar5 >> 16) & 0xFFFFFFFC) + animEnd);
        for (unsigned int count = uVar5 & 0xFFFF; count != 0; count--) {
            unsigned int uVar4 = *puVar6;
            puVar6++;
            // 0x004627db/0x004627e4: MOV EDX,[ECX+4] / SAR EDX,0x10 and
            // MOV EAX,[ECX] / SAR EAX,0x10 - the original loads the DWORDs at
            // +4 and +0 and keeps their HIGH halves, i.e. the u16 at +6 (frame
            // stride, 104) and the u16 at +2 (frame base, 176). Ghidra renders
            // those as "*(short *)(animBuffer + 2) >> 16", which as C is a
            // short promoted to int then shifted right 16 - always 0 - so every
            // frame collapsed onto offset 0 and the angles were added to the
            // EMR armature header instead of to the aim frames.
            int stride = (int)*(short*)(animBuffer + 6);
            int base   = (int)*(short*)(animBuffer + 2);
            int offset = (stride * (int)(uVar4 & 0xFFFF) + base) & 0xFFFFFFFC;
            if (param1 == 0) {
                short* psVar1;
                psVar1 = (short*)(animBuffer + offset + 0x42);
                *psVar1 = *psVar1 + (short)((g_weaponAngle_PrimX << 12) / 360);
                psVar1 = (short*)(animBuffer + offset + 0x44);
                *psVar1 = *psVar1 + (short)((g_weaponAngle_PrimY << 12) / 360);
                psVar1 = (short*)(animBuffer + offset + 0x46);
                *psVar1 = *psVar1 + (short)((g_weaponAngle_PrimZ << 12) / 360);
                if (local_8 == 6) {
                    psVar1 = (short*)(animBuffer + offset + 0x48);
                    *psVar1 = *psVar1 + (short)((g_weaponAngle_Sec2X << 12) / 360);
                    psVar1 = (short*)(animBuffer + offset + 0x4a);
                    *psVar1 = *psVar1 + (short)((g_weaponAngle_Sec2Y << 12) / 360);
                    psVar1 = (short*)(animBuffer + offset + 0x4c);
                    *psVar1 = *psVar1 + (short)((g_weaponAngle_Sec2Z << 12) / 360);
                } else {
                    psVar1 = (short*)(animBuffer + offset + 0x48);
                    *psVar1 = *psVar1 + (short)((g_weaponAngle_Sec1X << 12) / 360);
                    psVar1 = (short*)(animBuffer + offset + 0x4a);
                    *psVar1 = *psVar1 + (short)((g_weaponAngle_Sec1Y << 12) / 360);
                    psVar1 = (short*)(animBuffer + offset + 0x4c);
                    *psVar1 = *psVar1 + (short)((g_weaponAngle_Sec1Z << 12) / 360);
                }
            } else {
                short* psVar1 = (short*)(animBuffer + offset + 100);
                *psVar1 = *psVar1 + (short)((g_weaponAngle_Special << 12) / 360);
            }
        }
        local_8++;
    } while (local_8 < 8);
}

// ============================================================================
// FUN_0048bc60 (0x0048bc60) - Set joint count and joint structs pointer
// Reads the joint count from the animation header (+4 byte) and sets up
// the joints_structs pointer at the current g_loadDataDestPointer.
// ============================================================================
void Entity_SetJoints(Entity* em, unsigned int param2)
{
    unsigned char jointCount = *(unsigned char*)(em->animHeader + 4);
    em->jointCount = jointCount;
    em->jointsStructs = (JointStruct*)g_loadDataDestPointer;
    g_loadDataDestPointer = (void*)((unsigned int)g_loadDataDestPointer + (param2 & 0xFFFC) * jointCount);
}

// ============================================================================
// FUN_0048b6b0 (0x0048b6b0) - Initialize animation structure
// Resolves pointers in animation data and sets up initial animation state.
// ============================================================================
void InitAnimStructure(void* animHeaderValue)
{
    AnimDataHeader* header = (AnimDataHeader*)animHeaderValue;
    ResolveAnimPointers(&header->resolved);
    SetAnimSlot(header->slots, (int)&ENTITY->unk_0c, 0);
    *(DWORD*)&ENTITY->unk_10 = (DWORD)&ENTITY->scaMatrixData;
    ENTITY->blend_counter = 0;
}

// ============================================================================
// FUN_0048b9e0 (0x0048b9e0) - Set up joint structures
// Iterates through all joints and initializes them with animation data.
// Returns the next available pointer after animation object data.
// ============================================================================
unsigned int SetupJointStructures(unsigned int param1)
{
    unsigned char local_1 = 0;
    JointStruct* joint = ENTITY->jointsStructs;
    unsigned int uVar2 =     ENTITY->modelLoadBuffer;
    unsigned char jointCount = ENTITY->jointCount;

    if (jointCount == 0) {
        return param1;
    }

    do {
        SetAnimSlot((AnimSlot*)uVar2, (int)&joint->anim_field, local_1);
        joint->index = local_1;
        joint->flags = 3;
        joint->data_ptr = &joint->scale_flag;
        joint->field_1c = 0;
        joint->scale_flag = 1;
        joint->anim_object = 0;
        joint->field_02 = 0;
        joint->anim_field = 0;
        param1 = (unsigned int)CreateAnimObject((int)&joint->anim_field, (unsigned int*)param1);

        if (ENTITY->id == 0x29) {
            switch (local_1) {
                case 4:
                case 5:
                case 7:
                case 8:
                    joint->flags = 0;
                    break;
            }
        }

        joint++;
        local_1++;
    } while (local_1 < jointCount);

    return param1;
}

// ============================================================================
// FUN_0048bad0 (0x0048bad0) - Reset joint transforms to identity
// Sets all joint transforms to identity matrix and initial joint positions.
// ============================================================================
void ResetJointTransforms(void)
{
    unsigned char jointCount = ENTITY->jointCount;
    short* psVar2 = (short*)(ENTITY->animHeader + 8);
    JointStruct* joint = ENTITY->jointsStructs;

    ENTITY->lookAtJointIdx = 1;

    for (; jointCount != 0; jointCount--) {
        joint->transform = g_identityMatrixData;
        joint->transform.t[0] = (int)*psVar2;
        joint->transform.t[1] = (int)psVar2[1];
        joint->transform.t[2] = (int)psVar2[2];
        joint->rotation.x = 0;
        joint->rotation.y = 0;
        joint->rotation.z = 0;
        joint->rotDeltaX = 0;
        joint->rotDeltaY = 0;
        joint->rotDeltaZ = 0;
        psVar2 += 3;
        joint++;
    }
}

// ============================================================================
// InitScaMatrix (0x00483520) - Initialize SCA collision matrix data
// Sets up a 3x3 SCA collision response matrix with identity values.
// param1: owner pointer (set as back-reference at scaMatrixData->owner if non-zero)
// param2: destination ScaMatrixData buffer (scaMatrixData field of Entity/PlayerEntity)
// ============================================================================
void InitScaMatrix(int param1, ScaMatrixData* scaData)
{
    scaData->field_00 = 0;
    scaData->owner = (unsigned int)param1;
    if (param1 != 0) {
        *(unsigned int**)(param1 + 0x4c) = (unsigned int*)scaData;
    }
    scaData->field_4c = 0;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            unsigned short val = (unsigned short)(-(i == j) & 0x1000);
            scaData->localMatrix.m[i][j] = val;
            scaData->worldMatrix.m[i][j] = val;
        }
        scaData->localMatrix.t[i] = 0;
        scaData->worldMatrix.t[i] = 0;
    }
}

// ============================================================================
// LoadEntityEMD (0x00462370) - Load entity EMD model file
// Loads the EMD 3D model for the player or enemy entity.
// Sets up animation data, texture references, and model geometry pointers.
// ============================================================================
void LoadEntityEMD(Entity* em, unsigned char entity_id)
{
    unsigned char bVar5 = g_TextureBankID;
    unsigned char bVar6 = g_TextureCurrentPage;

    if ((g_main_state_flags2 & MSF2_COSTUME_VARIANT) != 0 && entity_id < 2) {
        entity_id = (unsigned char)g_bCostumeVariant + 0x33;
    }

    // DC ADVANCED remaps the player and three cutscene NPCs onto its own
    // outfit models, after the costume fixup exactly as the PS1 orders it.
    if (g_bDcMode) {
        entity_id = dc_emd_advanced_index(entity_id);
    }

    // The port's table is the OG's; in DC mode a handful of indices name a
    // different file (dc/EntityModels.cpp). NULL = the table entry stands.
    const char* emdPath = g_emdPathTable[(g_playerEntity.id & 1) * 53 + entity_id];
    if (g_bDcMode) {
        const char* dcPath = dc_emd_path(entity_id, (unsigned char)(g_playerEntity.id & 1));
        if (dcPath != NULL) {
            emdPath = dcPath;
        }
    }

    sprintf(FILE_PATH, "%s%s", GAME_DATA_ROOT, emdPath);
    SetSpriteBufferFlag();

    unsigned int fileSize = LoadFile(FILE_PATH, g_loadDataDestPointer, 32);
    int data_pointer = (int)g_loadDataDestPointer;

    if ((g_main_state_flags2 & MSF2_COSTUME_VARIANT) != 0 && (unsigned int)g_bCostumeVariant - entity_id == -51) {
        entity_id = g_playerEntity.id & 1;
    }

    // The DC's post-load half of the same ADVANCED block: it can rewrite the
    // entity's id, and does so before the id is read just below.
    if (g_bDcMode) {
        dc_emd_post_load(entity_id);
    }

    unsigned int* puVar2 = (unsigned int*)(((fileSize & 0xFFFFFFFC) - 0x14) + data_pointer);
    g_loadDataDestPointer = (void*)(data_pointer + (*(unsigned int*)(((fileSize & 0xFFFFFFFC) - 4) + data_pointer) & 0xFFFFFFFC));

    unsigned char entityType = ENTITY->id;
    if (entityType > 0x1f) {
        unsigned short uVar7 = (unsigned short)g_TextureCurrentPage;
        ENTITY->attacking_direction = (char)uVar7;
        ENTITY->dir_control_flags = (char)(uVar7 >> 8);
        *(unsigned short*)&ENTITY->texBank = (unsigned short)g_TextureBankID;
    }

    DAT_004d2bd8 = 0;
    switch (entity_id) {
        case 7:
        case 8:
        case 11:
        case 13:
        case 23:
            ClearTmdProcessingFlag();
            break;
        case 18:
            DAT_004d2bd8 = 1;
            break;
    }

    ProcessTmdAsync((unsigned int)g_loadDataDestPointer);

    if ((DAT_004c1a2c >> (entityType & 0x1f) & 1) != 0) {
        QueueTextureForProcessing(bVar6, entityType);
    }

    int texDataPtr = (puVar2[3] & 0xFFFFFFFC) + data_pointer;
    em->modelLoadBuffer = texDataPtr;
    ProcessTmdTextures(2, (unsigned int*)texDataPtr, bVar5, bVar6);
    em->animBase = (puVar2[2] & 0xFFFFFFFC) + data_pointer;
    em->animHeader = (puVar2[1] & 0xFFFFFFFC) + data_pointer;

    if (*puVar2 != 0) {
        g_playerEntity.emdScratchPtr1 = data_pointer;
        g_playerEntity.emdScratchPtr2 = (*puVar2 & 0xFFFFFFFC) + data_pointer;
    }
}

// ============================================================================
// LoadEntityModel (0x0048b630) - Load entity 3D model
// Loads the EMD model for the current entity, sets up joint data,
// animation structures, and resets transforms.
// ============================================================================
void LoadEntityModel(void)
{
    void* data_pointer_bkp = g_loadDataDestPointer;

    g_playerEntity.modelLoadBuffer = (DWORD)&g_entityModelBuffer;
    g_loadDataDestPointer = &g_entityModelBuffer;

    LoadEntityEMD(ENTITY, g_playerEntity.id & 3);

    Entity_SetJoints(ENTITY, sizeof(JointStruct));

    g_loadDataDestPointer = data_pointer_bkp;

    InitAnimStructure((void*)g_playerEntity.modelLoadBuffer);

    g_playerEntity.jointCount++;

    SetupJointStructures((unsigned int)&g_entityModelBuffer2);

    g_playerEntity.jointCount--;

    ResetJointTransforms();
}

// ============================================================================
// LoadEquippedWeaponAnimation (0x00462620)
// Loads the animation file for the currently equipped weapon.
// Sets up weapon model geometry, texture, and animation data.
// ============================================================================
void LoadEquippedWeaponAnimation(unsigned char weapon_id, unsigned char param_2, unsigned int anim_buffer, unsigned int param_4)
{
    JointStruct* joint = &g_playerEntity.jointsStructs[param_2];

    if (weapon_id > 0x6e) {
        weapon_id = 0xd - (weapon_id == 0x6f);
    }

    // The Director's Cut's ADVANCED handgun remap (PS1 0x80023e38):
    //
    //     if ((g_status_flags & 0x20000) && (player.id & 3) < 2
    //         && (param_1 == 2 || param_1 == 4)) { param_1 = 0xc; equipped = 2; }
    //
    // In ADVANCED, for Chris and Jill, the plain Beretta (2) AND the Beretta
    // M92FS custom (4) both load the custom's in-hand model - the DC's weapon
    // table index 0xC, i.e. Players/W0F.EMW / W1F.EMW - and the equipped weapon
    // id is forced to 2.
    //
    // That second half is the load-bearing part, and the port was missing it.
    // equippedWeaponId feeds EVERY per-weapon lookup downstream, so leaving it
    // at 4 sent the custom Beretta through weapon slot 3 - the USA's DumDum
    // Python row: 50 damage instead of the handgun's 9, the Python's ejected
    // case, the Python's fire frames, and the slot-3 guaranteed head-explode in
    // enemy_hit_reaction_zombie, which killed every standing zombie outright.
    // With the remap it is weapon 2 throughout, and the ONLY thing that makes
    // it a custom Beretta is the ~1/8 roll in apply_weapon_damage that promotes
    // weaponAdj 1 to slot 3 for a single shot. That roll is also why the remap
    // has to exist: it tests weaponAdj == 1 under the same ADVANCED gate, and
    // would be unreachable code if ADVANCED only ever held weapon 4.
    //
    // The PS1 substitutes model index 0xC and keeps using it for the rest of
    // the function; the port names the file directly (the PC's index 0xC is a
    // special weapon) and carries weapon_id 2 on, which leaves the remaining
    // `weapon_id == 0` / `> 0xb` tests behaving exactly as the PS1's 0xC does.
    const char* weaponModel = 0;
    if (g_bDcMode && (g_main_state_flags2 & MSF2_DC_ADVANCED) != 0
        && (g_playerEntity.id & 3) < 2
        && (weapon_id == ITEM_BERETTA || weapon_id == DC_ITEM_BERETTA_CUSTOM)) {
        weaponModel = dc_weapon_model_path(g_playerEntity.id & 3,
                                           (unsigned char)DC_ITEM_BERETTA_CUSTOM);
        weapon_id = ITEM_BERETTA;
        g_playerEntity.equippedWeaponId = ITEM_BERETTA;
    }
    sprintf(FILE_PATH, "%s%s",
            GAME_DATA_ROOT,
            weaponModel != 0 ? weaponModel
                             : g_weaponPathTable[g_playerEntity.id & 3][weapon_id]);
    SetSpriteBufferFlag();

    unsigned int fileSize = LoadFile(FILE_PATH, (void*)anim_buffer, 32);
    g_playerEntity.jointMoveData0 = anim_buffer;

    unsigned int* puVar1 = (unsigned int*)(((fileSize & 0xFFFFFFFC) - 8) + (int)anim_buffer);
    g_playerEntity.jointMoveData1 = (*puVar1 & 0xFFFFFFFC) + (int)anim_buffer;

    if (weapon_id > 0xb) {
        AdjustWeaponAnimationPositions(weapon_id - 0xc);
    }

    if (weapon_id == 0) {
        joint->anim_slot_ptr = g_playerEntity.weaponPartAnimSlot;
        joint->anim_object = (void*)g_playerEntity.weaponPartAnimObject;
    } else {
        joint->anim_slot_ptr = (puVar1[1] & 0xFFFFFFFC) + (int)anim_buffer;
        unsigned char prevPage = g_TextureCurrentPage;
        unsigned char prevBank = g_TextureBankID;
        g_TextureCurrentPage = 7;
        g_TextureBankID = 0x16;
        ProcessTmdTextures(2, (unsigned int*)joint->anim_slot_ptr, 0x16, 7);
        g_TextureBankID = prevBank;
        g_TextureCurrentPage = prevPage;
        joint->anim_slot_ptr += 0xc;
        joint->anim_object = (void*)param_4;
    }

    CreateAnimObject((int)&joint->anim_field, (unsigned int*)joint->anim_object);
}

// ============================================================================
// FUN_00459da0 (0x00459da0) - Set body part pointers for current weapon
// Sets the weaponPartAnimSlot/weaponPartAnimObject body part data from the joints at the given index.
// param1: joint index (0xe for player, selects which body part handles weapon)
// ============================================================================
void SetWeaponBodyParts(unsigned char param1)
{
    JointStruct* joint = &ENTITY->jointsStructs[param1];
    ENTITY->weaponPartAnimSlot = joint->anim_slot_ptr;
    ENTITY->weaponPartAnimObject = (unsigned int)joint->anim_object;
}

// ============================================================================
// FUN_00429d30 (0x00429d30) - Clear animation timing values
// Clears two 16-bit values at 0x7b8 and 0x7ba within the joint data buffer.
// ============================================================================
void ClearAnimTiming(void)
{
    JointStruct* joints = g_playerEntity.jointsStructs;
    joints[15].velZ = 0;
    joints[15].rotDeltaX = 0;
}

// ============================================================================
// SetupCharacterData (0x00494fc0)
// Sets up the player character: loads the player model, weapon animation,
// initializes position from last safe position, and sets up SCA collision data.
// Called from InitializeGame() after loading bio_card.dat and item images.
// ============================================================================
void SetupCharacterData(void)
{
    ENTITY = reinterpret_cast<Entity*>(&g_playerEntity);
    InitPlayerEntity();
    g_TextureCurrentPage = 7;
    g_TextureBankID = 0x16;
    Object_DeleteAll(0);
    LoadEntityModel();
    InitScaMatrix(0, &g_playerEntity.scaMatrixData);
    SetWeaponBodyParts(0xe);
    g_playerEntity.equippedWeaponId = 0;
    if (g_EquippedItemId != 0) {
        // this is faithfull to original code
        // itembox slots length is 48 slots, the character item slots is expected to be after the itembox slots array
        g_playerEntity.equippedWeaponId = g_itemboxSlots[g_EquippedItemId + 47].Id;
    }
    LoadEquippedWeaponAnimation(
        g_playerEntity.equippedWeaponId, 0xe, (unsigned int)g_animationBuffer, (unsigned int)&g_animObjectBuffer);

    // The original copies the position into the matrix translation here
    // (0x0049508e: t[0] = position.x, t[2] = position.z, t[1] = 0) — this was
    // missing from the port. Without it the matrix keeps InitScaMatrix's zero
    // translation, and the first-frame player_state_init sync (position = t[0])
    // overwrites the position set by the save load with (0,0).
    g_playerEntity.scaMatrixData.localMatrix.t[0] = (long)g_playerEntity.position.x;
    g_playerEntity.scaMatrixData.localMatrix.t[2] = (long)g_playerEntity.position.z;
    g_playerEntity.posY = 0;
    g_playerEntity.scaMatrixData.localMatrix.t[1] = 0;
    g_playerEntity.jointsStructs[1].rotDeltaX = 0;
    g_playerEntity.jointsStructs[1].rotDeltaY = 0;
    g_playerEntity.jointsStructs[1].rotDeltaZ = 0x10;
    g_playerEntity.Sca_info = g_scaDataTable[(g_playerEntity.id & 1) * 2];
    ClearAnimTiming();
}

// ============================================================================
// set_player_animations_functions (0x00409a00)
// Populates the player animation function pointer table at g_playerAnimFunctions
// These function pointers are indexed by g_playerEntity.animFrameId * 4
// and dispatched by FUN_00495290 during gameplay.
// ============================================================================
void set_player_animations_functions(void)
{
    g_playerAnimFunctions[0]   = (void*)player_anim_attack_recoil;       // 0x00437a80 @ offset 0x00
    g_playerAnimFunctions[40]  = (void*)player_anim_simple_recovery;     // 0x0049abb0 @ offset 0xA0
    g_playerAnimFunctions[24]  = (void*)player_anim_multi_attack;        // 0x00430130 @ offset 0x60
    g_playerAnimFunctions[44]  = (void*)player_anim_dispatch_4c2ac8;     // 0x004196d0 @ offset 0xB0
    g_playerAnimFunctions[7]   = (void*)player_anim_crawling;            // 0x0048f060 @ offset 0x1C
    g_playerAnimFunctions[27]  = (void*)player_anim_set_attacked_flag;   // 0x00469400 @ offset 0x6C
    g_playerAnimFunctions[46]  = (void*)player_anim_dispatch_4ba360;     // 0x00468e10 @ offset 0xB8
    g_playerAnimFunctions[28]  = (void*)player_anim_dispatch_4c10b0;     // 0x0043b980 @ offset 0x70
    g_playerAnimFunctions[30]  = (void*)player_anim_poison_death;        // 0x004401c0 @ offset 0x78
    g_playerAnimFunctions[49]  = (void*)player_anim_death_billboard;     // 0x00440230 @ offset 0xC4
    g_playerAnimFunctions[31]  = (void*)player_anim_limb_physics;        // 0x00424fb0 @ offset 0x7C
    g_playerAnimFunctions[50]  = (void*)player_anim_enemy_interact;      // 0x00424de0 @ offset 0xC8
    g_playerAnimFunctions[51]  = (void*)player_anim_death_alt;           // 0x004088f0 @ offset 0xCC
    g_playerAnimFunctions[34]  = (void*)player_anim_dispatch_4b1a90;     // 0x0045c460 @ offset 0x88
    Task_sleep(1);
}

// (0x0048b9e0) - Set up joint animation structures for an entity
// Iterates through all joints, initializes animation slots, and creates
// animation objects. Used by main_menu before Joint_move to set up the
// player model's joint data from the EMD animation header.
void SetupJointStructures(void* buf)
{
    unsigned int* paramBuf = (unsigned int*)buf;
    unsigned char jointIdx = 0;
    JointStruct* joint = ENTITY->jointsStructs;
    void* modelLoadBuffer = (void*)ENTITY->modelLoadBuffer;
    unsigned char count = ENTITY->jointCount;

    if (count == 0) return;

    do {
        // Link animation slot data for this joint
        SetAnimSlot((AnimSlot*)modelLoadBuffer, (int)&joint->anim_field, jointIdx);

        // Initialize joint fields
        joint->index = jointIdx;
        joint->flags = 3;
        joint->data_ptr = &joint->scale_flag;
        joint->field_1c = 0;
        joint->scale_flag = 1;
        joint->anim_object = NULL;
        joint->field_02 = 0;
        joint->anim_field = 0;

        // Create animation object in the buffer
        paramBuf = CreateAnimObject((int)&joint->anim_field, paramBuf);

        // Special case: entity ID 0x29 disables certain joints
        if (ENTITY->id == 0x29) {
            switch (jointIdx) {
            case 4: case 5: case 7: case 8:
                joint->flags = 0;
            }
        }

        joint++;
        jointIdx++;
    } while (jointIdx < count);
}

