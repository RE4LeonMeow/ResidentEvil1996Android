// TmdAnimation.cpp - TMD animation pipeline (decompiled from Ghidra)
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/Marni3DObject.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include "../DebugPrint.h"
#include <cstdio>

// ============================================================================
// TMD texture header struct (output of FUN_00482d40 / ParseTmdTextureHeader)
// Packed struct matching the original byte layout (28 bytes = 0x1C)
// ============================================================================
#pragma pack(push, 1)
struct TmdTextureHeader {
    int   count;       // offset 0x00: (*data & 0xF) sign-extended
    short field_04;    // offset 0x04: first short from aligned sub-section
    short field_06;    // offset 0x06: second short
    short field_08;    // offset 0x08: third short (used for page count calc)
    short field_0A;    // offset 0x0A: fourth short
    int   dataPtr;     // offset 0x0C: pointer into sub-section data
    short field_10;    // offset 0x10: first short from data+8 (CLUT descriptor low)
    short field_12;    // offset 0x12: second short from data+8
    short field_14;    // offset 0x14: third short from data+8
    short field_16;    // offset 0x16: fourth short from data+8 (depth increment)
    int   ptr_10;      // offset 0x18: data + 0x10 pointer
};
#pragma pack(pop)

// ============================================================================
// FUN_00482dc0 (0x00482dc0) - Resolve relative pointers in animation data
// Given a pointer to an animation data block header, resolves all internal
// relative pointers (offsets) into absolute addresses.
// ============================================================================
void ResolveAnimPointers(unsigned char* data)
{
    int iVar1;
    unsigned char* pbVar2;

    if ((*data & 1) != 0) return;

    data[0] = 1;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    iVar1 = *(int*)(data + 4);
    unsigned char* basePtr = data + 8;
    pbVar2 = basePtr;
    if (iVar1 > 0) {
        do {
            *(unsigned char**)pbVar2 = basePtr + *(int*)pbVar2;
            *(unsigned char**)(pbVar2 + 8) = basePtr + *(int*)(pbVar2 + 8);
            *(unsigned char**)(pbVar2 + 0x10) = basePtr + *(int*)(pbVar2 + 0x10);
            iVar1--;
            pbVar2 += 0x1c;
        } while (iVar1 != 0);
    }
}

// ============================================================================
// FUN_00482ef0 (0x00482ef0) - Set animation slot data
// Links an animation data slot to a specific index within the animation header.
// ============================================================================
void SetAnimSlot(AnimSlot* slots, int slotPtr, int index)
{
    *(int*)(slotPtr + 8) = (int)&slots[index];
    g_animSlotIndex = index;
}

// ============================================================================
// FUN_00483da0 (0x00483da0) - Find minimum CLUT depth in animation
// Scans animation data entries to find the lowest CLUT depth value.
// Returns 0 if no valid depth found.
// ============================================================================
unsigned int FindMinClutDepth(AnimSlot* slot)
{
    unsigned int minDepth = 0xFFFFFFFF;
    unsigned int depth;

    for (int i = 0; i < 1; i++) {
        unsigned int* puVar5 = (unsigned int*)slot->data2;
        int iVar2 = slot->entryCount;
        if (iVar2 > 0) {
            do {
                if ((*puVar5 & 0x4000000) != 0) {
                    depth = (puVar5[2] >> 16) & 0x1f;
                    if (depth < minDepth) {
                        minDepth = depth;
                    }
                }
                iVar2--;
                puVar5 = puVar5 + ((*puVar5 >> 8) & 0xff) + 1;
            } while (iVar2 != 0);
        }
        slot++;
    }
    if (minDepth == 0xFFFFFFFF) {
        minDepth = 0;
    }
    return minDepth;
}

// FUN_00483eb0 - declared here (defined below) so SetupTextureBank can call it
unsigned int FixClutVertexData(BYTE* texPtr);

// ============================================================================
// FUN_00483fc0 (0x00483fc0) - Setup texture bank for rendering
// Prepares a PSXTexture bank entry for D3D rendering.
// param_1: pointer to receive the resolved texture bank pointer
// param_2: texture bank ID (index into g_objectCountArray)
// ============================================================================
void SetupTextureBank(DWORD** param_1, int param_2)
{
    int count = g_objectCountArray[param_2];
    if (count == 0) return;

    if (count > 1) {
        *param_1 = (DWORD*)&g_psxTextureArray[(param_2 * 8 + 1) * 0x36C];
        return;
    }

    g_objectCountArray[param_2] = count + 1;
    int slotIndex = count + param_2 * 8;
    BYTE* dstTex = &g_psxTextureArray[slotIndex * 0x36C];
    BYTE* srcTex = &g_psxTextureArray[(slotIndex - 1) * 0x36C];

    VideoDriver_ClearState348(dstTex, g_pMarniDirect3D);

    PSXTexture* dst = (PSXTexture*)dstTex;
    PSXTexture* src = (PSXTexture*)srcTex;
    // Deep copy — the original 0x00484030 calls PSXTexture::operator=
    // (0x0041f9c0), which copies each ACTIVE CMarniBits sub-object through
    // CMarniBits_CopyFrom (0x004034d0) so dst gets its own pixel/CLUT heap
    // buffers. A raw struct copy would ALIAS src's heap pointers into dst;
    // the room-exit cleanup then freed the same pixel data twice
    // (RtlValidateHeap: invalid address on leaving ROOM40F0).
    dst->CopyFrom(src);

    DWORD* dstFlag = (DWORD*)(dstTex + 0x348);
    DWORD* srcFlag = (DWORD*)(srcTex + 0x348);
    *dstFlag = *srcFlag;

    DWORD* dstHandles = (DWORD*)(dstTex + 0x34C);
    DWORD* srcHandles = (DWORD*)(srcTex + 0x34C);
    for (int i = 0; i < 8; i++) {
        dstHandles[i] = srcHandles[i];
    }

    *param_1 = (DWORD*)dstTex;

    DWORD* clearHandles = dstHandles;
    for (int i = 0; i < 8; i++) {
        clearHandles[i] = 0;
    }
    *dstFlag = 0;

    // Original 0x00484088 calls FUN_00483eb0 (FixClutVertexData) here, NOT
    // ResolveAnimPointers. The bank slot is a PSXTexture (vtable, pixel
    // pointer at +0x04), not an animation header — feeding it to
    // ResolveAnimPointers read the heap pixel pointer as the entry count and
    // walked 2M entries past the array (AV at 0x01B8C7B0 on ROOM40F0 entry).
    FixClutVertexData(dstTex);

    int matCount = *(int*)(dstTex + 0x340);
    if (matCount != 0) {
        void** vtable = *(void***)g_pMarniDirect3D;
        typedef DWORD (*CreateTextureFn)(void*, void*, int, int);
        CreateTextureFn createTex = (CreateTextureFn)vtable[6];

        DWORD* handlePtr = dstHandles;
        BYTE* matPtr = dstTex;
        for (unsigned int i = 0; i < (unsigned int)matCount; i++) {
            DWORD handle = createTex(g_pMarniDirect3D, matPtr, 0x21, 0);
            *handlePtr = handle;
            handlePtr++;
            matPtr += 0x68;
        }
    }
    *dstFlag = 1;
}

// ============================================================================
// FUN_00483cf0 (0x00483cf0) - Check if TMD has transparent polygons
// Returns 1 if any polygon has the transparency bit (0x2000000) set.
// ============================================================================
unsigned int CheckTmdTransparency(int param_1)
{
    unsigned int count = 0;
    unsigned int* objData = *(unsigned int**)(param_1 + 0x10);
    unsigned int objCount = *(unsigned int*)(param_1 + 0x14);

    for (unsigned int i = 0; i < objCount; i++) {
        if ((*objData & 0x2000000) != 0) {
            return 1;
        }
        count++;
        objData = objData + ((*objData >> 8) & 0xFF) + 1;
    }
    return 0;
}

// Blend table at 0x004d2c00, indexed by the primitive's tsb ABR field. The
// value is the 8-bit weight the BACKGROUND keeps behind a semi-transparent
// TMD - not the model's opacity, see the note in FlushTmdObjects:
//   abr 0 "B/2 + F/2"  -> 0x80, half the background = the PS1 rule exactly
//   abr 1 "B + F"      -> 0x00, no background, i.e. opaque
//   abr 2 "B - F"      -> 0x80
//   abr 3 "B + F/4"    -> 0xD0, mostly background
static const int g_TmdBlendModeTable[4] = { 0x80, 0x00, 0x80, 0xD0 };

// ============================================================================
// FUN_00483d30 (0x00483d30) - Get TMD blend mode
// Returns blend mode from DAT_004d2be0 override or from polygon flags.
// ============================================================================
int GetTmdBlendMode(int param_1)
{
    unsigned int* objData = *(unsigned int**)(param_1 + 0x10);
    unsigned int objCount = *(unsigned int*)(param_1 + 0x14);

    for (unsigned int i = 0; i < objCount; i++) {
        if ((*objData & 0x2000000) != 0) {
            if (DAT_004d2be0 >= 0 && DAT_004d2be0 < 256) {
                return DAT_004d2be0;
            }
            // 0x00483d7d-0x00483d8a:
            //   MOV EAX,[ESI+8] / AND EAX,0x600000 / SHR EAX,0x13
            //   MOV EAX,[EAX + 0x4d2c00]
            // objData[2] is the packet's (u1,v1,tsb) dword, so bits 21-22 are
            // the tsb ABR field and `>> 0x13` already yields abr*4 - a BYTE
            // offset into the table, which must NOT be scaled again. The base
            // is the table at 0x004d2c00, not &DAT_004d2be0 (0x20 bytes
            // earlier); with both errors the abr-0 case happened to read
            // DAT_004d2be0 (-1, "non-zero") and only looked right.
            return g_TmdBlendModeTable[(objData[2] & 0x600000) >> 0x15];
        }
        objData = objData + ((*objData >> 8) & 0xFF) + 1;
    }
    return 0;
}

// ============================================================================
// FUN_00483eb0 (0x00483eb0) - Fix CLUT vertex data for PSXTexture
// Scans 8-bit textures and adjusts CLUT vertex coordinates.
// ============================================================================
unsigned int FixClutVertexData(BYTE* texPtr)
{
    unsigned int hasChanges = 0;
    unsigned int matCount = *(unsigned int*)(texPtr + 0x340);

    for (unsigned int i = 0; i < matCount; i++) {
        BYTE* matEntry = texPtr + i * 0x68;
        if (*(char*)(matEntry + 0x2A) == 8) {
            // FUN_00483eb0: vtable[4] = CMarniBits::Lock, vtable[5] = CMarniBits::Unlock
            // Original passes matEntry as this (ECX) for __thiscall. The decomp
            // vtable wrappers are __cdecl with self as the first argument.
            int* vtable = *(int**)matEntry;
            void* outData = NULL;    // receives m_pPixelData (offset 0x04)
            DWORD clutPtr = 0;       // receives m_pPalette  (offset 0x08)
            typedef int (*LockFn)(void* self, void** outData, DWORD* outPitch);
            typedef int (*UnlockFn)(void* self);
            int result = ((LockFn)vtable[4])(matEntry, &outData, &clutPtr);
            if (result != 0) {
                short* ptr = (short*)(ULONG_PTR)clutPtr;
                for (int j = 0; j < 256; j++) {
                    if (*ptr == (short)0x8000) {
                        *ptr = (short)0x8001;
                    }
                    if (*ptr == 0) {
                        hasChanges = 1;
                    }
                    ptr++;
                }
            }
            ((UnlockFn)vtable[5])(matEntry);
            // Original clobbers outer loop counter after a successful Lock
            // (MOV ESI,EAX at 0x00483efb), so only the first 8-bit material
            // is ever processed.
            break;
        }
    }
    return hasChanges;
}

// ============================================================================
// FUN_00483f40 (0x00483f40) - Check if PSXTexture needs re-creation
// Returns 1 if any 8-bit texture has a zero CLUT entry.
// ============================================================================
unsigned int CheckTextureRecreation(BYTE* texPtr)
{
    unsigned int matCount = *(unsigned int*)(texPtr + 0x340);
    unsigned int found = 0;

    for (unsigned int i = 0; i < matCount; i++) {
        BYTE* matEntry = texPtr + i * 0x68;
        if (*(char*)(matEntry + 0x2A) == 8) {
            // FUN_00483f40: vtable[4] = CMarniBits::Lock, vtable[5] = CMarniBits::Unlock
            // Original passes matEntry as this (ECX) for __thiscall. The decomp
            // vtable wrappers are __cdecl with self as the first argument.
            int* vtable = *(int**)matEntry;
            void* outData = NULL;    // receives m_pPixelData (offset 0x04)
            DWORD clutPtr = 0;       // receives m_pPalette  (offset 0x08)
            typedef int (*LockFn)(void* self, void** outData, DWORD* outPitch);
            typedef int (*UnlockFn)(void* self);
            int result = ((LockFn)vtable[4])(matEntry, &outData, &clutPtr);
            if (result != 0) {
                short* ptr = (short*)(ULONG_PTR)clutPtr;
                for (int j = 0; j < 256; j++) {
                    if (*ptr == 0) {
                        found = 1;
                    }
                    ptr++;
                }
            }
            ((UnlockFn)vtable[5])(matEntry);
            // Original clobbers outer loop counter after a successful Lock
            // (MOV EBX,EAX at 0x00483f8b), so only the first 8-bit material
            // is ever checked. Match that to avoid scanning uninitialized
            // palette pointers on subsequent materials.
            break;
        }
    }
    return found;
}

// ============================================================================
// FUN_00483910 (0x00483910) - Create TMD object (internal)
// Sets up a TMD object with texture, vertex, and rendering data.
// param_1: texture depth/bank ID
// param_2: TMD data pointer (from animation header)
// param_3: animation object pointer
// Returns: pointer to g_tmdObjectBuffer entry, or NULL on failure
// ============================================================================
BYTE* CreateTmdObjectInternal(int depth, int tmdDataPtr, int animObjPtr)
{
    bool hasTransparency = false;

    // Check if this object is already cached. The index comes from the
    // animation object's own +8 field, which a fresh object has not written
    // yet - the original indexes the table with it unmasked (ASan caught a
    // read 4 bytes before g_tmdObjectSlotAnimPtrs), so range-check it.
    int slotIndex = *(int*)(animObjPtr + 8);
    if (slotIndex >= 0 && slotIndex < 251 &&
        g_objectDeletePtr && g_objectDeletePtr[slotIndex] == animObjPtr) {
        return (BYTE*)&g_tmdObjectBuffer[slotIndex * 0x1594];
    }

    // Find a free slot (original scans up to 0xFA = 250 entries)
    int freeSlot = 0;
    int* slotPtr = (int*)g_objectDeletePtr;
    do {
        if (*slotPtr == animObjPtr) goto slotFound;
        slotPtr++;
        freeSlot++;
    } while (freeSlot < 250);
    if (g_objectDeleteFlag >= 250) {
        return NULL;
    }
    freeSlot = g_objectDeleteFlag;

slotFound:
    // Resolve texture bank redirect
    int bankID = depth;
    if (g_textureBankRedirect[bankID] != 0) {
        bankID = g_textureBankRedirect[bankID];
    }

    // Save the original header fields and patch the header for
    // PSXObject::Store (magic 0x41, absolute-pointer mode, 1 object)
    DWORD* hdrPtr = (DWORD*)(tmdDataPtr - 0xC);
    DWORD saved0 = hdrPtr[0];
    DWORD saved1 = hdrPtr[1];
    DWORD saved2 = hdrPtr[2];
    hdrPtr[0] = 0x41;
    hdrPtr[1] = 1;
    hdrPtr[2] = 1;

    int iVar8 = freeSlot * 0x1594;
    BYTE* objSlot = (BYTE*)&g_tmdObjectBuffer[iVar8];

    // Texture reference (UV divisor) from the bank header at +0x2C
    DWORD texRef = *(DWORD*)(&g_psxTextureArray[bankID * 0x1b60] + 0x2C);

    // FUN_004450e0: parse the TMD into the slot's embedded viewport elements
    // (ECX = objSlot in the original; args: hdr, 0, 1, bankID, texRef)
    PSXObject_Store((CMarniDirect3DTMD*)objSlot, (int*)hdrPtr, 0, bankID, (int)texRef);

    // Restore header
    hdrPtr[0] = saved0;
    hdrPtr[1] = saved1;
    hdrPtr[2] = saved2;

    // Walk the texture pages of this bank looking for a material that matches
    // one of the parsed embedded objects (keys at slot+0x38/+0x3C, stride 0x4C)
    BYTE* pagePtr = &g_psxTextureArray[bankID * 0x1b60];
    int pageCount = g_objectCountArray[bankID];     // DAT_008ffc40[bankID]

    // The match below compares each parsed object's CLUT material key
    // (elem+0x38/+0x3C, VRAM X/Y decoded from the primitive's CLUT word) against
    // the page's CLUT descriptor (page+0x54/+0x58). Both sides derive from the
    // same TIM, so they only line up if PSXTexture stores that descriptor as
    // DWORDs - see the note in PSXTexture.h.

    for (int page = 0; page < pageCount; page++) {
        int matCount = *(int*)(pagePtr + 0x340);
        if (matCount != 0) {
            int* matPtr = (int*)(pagePtr + 0x54);
            for (int m = 0; m < matCount; m++) {
                DWORD objCount = *(DWORD*)(objSlot + 0x4C0);
                if (objCount != 0) {
                    DWORD* objEntry = (DWORD*)(objSlot + 0x38);
                    for (DWORD o = 0; o < objCount; o++) {
                        if (objEntry[0] == (DWORD)matPtr[0] && objEntry[1] == (DWORD)matPtr[1]) {
                            // --- Material match found ---
                            *(DWORD*)(animObjPtr + 0x14) = 0;

                            // Transparency / blend mode
                            if (CheckTmdTransparency(tmdDataPtr) != 0 && DAT_004d2bdc == 0) {
                                if (GetTmdBlendMode(tmdDataPtr) == 0) {
                                    *(DWORD*)(animObjPtr + 0x14) = 0;
                                } else {
                                    *(DWORD*)(animObjPtr + 0x14) = 0x3F000000; // 0.5f
                                    if (DAT_004d2be0 >= 0 && DAT_004d2be0 < 0x100 && DAT_004d2be0 == 0x30) {
                                        *(DWORD*)(animObjPtr + 0x14) = 0x3E4CCCCD; // 0.2f
                                    }
                                }
                                hasTransparency = true;
                            }

                            // Texture setup for this page
                            if (*(int*)(pagePtr + 0x348) == 0) {
                                if (g_tmdTextureAllocated[bankID] == 0) {
                                    if (CheckTextureRecreation(pagePtr) != 0 && DAT_004d2bdc == 0) {
                                        FixClutVertexData(pagePtr);

                                        // Create D3D texture handles for each material
                                        void** vtable = *(void***)g_pMarniDirect3D;
                                        typedef DWORD (*CreateTextureFn)(void*, BYTE*, int, int);
                                        CreateTextureFn createTex = (CreateTextureFn)vtable[6];

                                        DWORD texCount = *(DWORD*)(pagePtr + 0x340);
                                        DWORD* handlePtr = (DWORD*)(pagePtr + 0x34C);
                                        BYTE* matBase = pagePtr;
                                        for (DWORD j = 0; j < texCount; j++) {
                                            *handlePtr = createTex(g_pMarniDirect3D, matBase, 0x21, 0);
                                            handlePtr++;
                                            matBase += 0x68;
                                        }
                                        *(int*)(pagePtr + 0x348) = 1;
                                        goto doCreate;
                                    }
                                }
                                // FUN_00421070: Direct3DTIM::Create (ECX = pagePtr)
                                Direct3DTIM_Create(pagePtr, g_pMarniDirect3D);
                            }

                        doCreate:
                            // CMarniDirect3DTMD::Create (0x00415650)
                            // (ECX = objSlot; args: d3d, pagePtr, 1)
                            if (((CMarniDirect3DTMD*)objSlot)->Create(g_pMarniDirect3D, pagePtr, (void*)1) == 0) {
                                return NULL;
                            }

                            if (hasTransparency) {
                                // Mark the records transparent (render-flag bit 2 at
                                // +0x80) and stash the blend alpha at +0x68, where
                                // FlushTmdObjects reads them. Do it for both buffers
                                // (m_objectData and m_objectDataCopy, 0x840 apart) so
                                // the draw works whichever side Transform queues. An
                                // earlier loop treated the 32 records as contiguous,
                                // which ran the last write into m_objectHandles[11]
                                // and 4 bytes past the slot.
                                int recordCount = *(int*)(objSlot + 0x4C0); // m_objectCount
                                if (recordCount < 0 || recordCount > 16) recordCount = 16;
                                for (int buf = 0; buf < 2; buf++) {
                                    DWORD* flags = (DWORD*)(objSlot + 0x4D0 + buf * 0x840 + 0x80);
                                    float* alpha = (float*)(objSlot + 0x4D0 + buf * 0x840 + 0x68);
                                    for (int k = 0; k < recordCount; k++) {
                                        *flags = *flags | 2;
                                        // Bitwise reinterpret, NOT a numeric cast:
                                        // animObjPtr+0x14 holds float bits (0x3F000000
                                        // = 0.5f). (float)0x3F000000 converts the DWORD
                                        // VALUE (1056964608.0f), which trips the draw's
                                        // a>0 && a<=1 guard and leaves the water opaque.
                                        *alpha = *(float*)(animObjPtr + 0x14);
                                        flags += 0x21;
                                        alpha += 0x21;
                                    }
                                }
                            }

                            // Register the slot in the object delete list
                            ((int*)g_objectDeletePtr)[freeSlot] = animObjPtr;
                            if (freeSlot == g_objectDeleteFlag) {
                                g_objectDeleteFlag++;
                            }
                            *(int*)(animObjPtr + 8) = freeSlot;
                            return objSlot;
                        }
                        objEntry += 0x13;   // 0x4C / 4
                    }
                }
                matPtr += 0x1A;             // 0x68 / 4
            }
        }
        pagePtr += 0x36C;
    }

    return NULL;
}

// ============================================================================
// LAB_00483c90 - Async callback for CreateTmdObject
// Loads stored globals and calls CreateTmdObjectInternal, stores result.
// ============================================================================
void AsyncCreateTmdCallback(void)
{
    g_asyncTmdResult = (DWORD)CreateTmdObjectInternal(
        (int)g_asyncTmdDepth,
        (int)g_asyncTmdDataPtr,
        (int)g_asyncTmdObjectPtr
    );
}

// ============================================================================
// FUN_00483cc0 (0x00483cc0) - Async TMD object setup
// Schedules async TMD object creation. Returns an object handle.
// ============================================================================
unsigned int AsyncCreateTmdObject(unsigned int param1, unsigned int param2, unsigned int param3)
{
    g_asyncTmdDepth = param1;
    g_asyncTmdDataPtr = param2;
    g_asyncTmdObjectPtr = param3;
    ExecAsync((void*)AsyncCreateTmdCallback);
    return g_asyncTmdResult;
}

// ============================================================================
// FUN_00486990 (0x00486990) - Complex TMD object setup
// Processes TMD model objects for Direct3D rendering with vertex/UV/normal data.
// Called when DAT_004d2bd8 (special model flag) is set.
// param_1: pointer to animation object data (the param2 from CreateAnimObject)
// ============================================================================
void ComplexTmdObjectSetup(int* param_1)
{
    if (g_objectListCleanupFlag != 0) return;
    g_objectListCleanupFlag = 1;

    // Clear tracking arrays (0x100 DWORDs each)
    memset(g_complexTmdObjectArray, 0, sizeof(g_complexTmdObjectArray));
    for (int i = 0; i < 256; i++) g_complexTmdObjectIds[i] = -1;

    // Seed the 256 object-list entries once. The original placement-news
    // CMarniViewport2[256] at 0x008fc430 at boot (ctor 0x004272e0 sets the
    // 0x004af0f8 vtable, zeroes the fields, m_unknown1C = 1); the port's raw
    // BSS array is never C++-constructed, and *ptrArray is dereferenced as a
    // function table below (NULL here was the ROOM40F0 crash at vtable[0]).
    if (g_objectListPtrArray[0] == 0) {
        for (int i = 0; i < 256; i++) {
            MarniViewport2_InitEntry(&g_objectListPtrArray[i * 0x0E]);
        }
    }

    // Get TMD data from the animation object
    int* tmdData = (int*)*param_1;
    int vertexBase = *tmdData;                                // [ESI+0x00] vertex data base
    unsigned int* objTable = (unsigned int*)tmdData[4];       // [ESI+0x10] object table
    int objCount = tmdData[5];                                // [ESI+0x14] object count

    // Resolve texture bank
    unsigned int bankID = (objTable[2] & 0x1F0000) >> 16;
    if (g_textureBankRedirect[bankID] != 0) {
        bankID = g_textureBankRedirect[bankID];
    }
    BYTE* texBank = &g_psxTextureArray[bankID * 0x1b60];

    // Setup texture bank
    DWORD* texBankPtr = (DWORD*)texBank;
    SetupTextureBank(&texBankPtr, bankID);
    texBank = (BYTE*)texBankPtr;

    if (objCount <= 0) {
        g_objectListCleanupCount = 0;
        param_1[4] = 1;
        return;
    }

    // Output array pointers
    BYTE* faceData = g_faceNormalBuffer;                           // face normal output
    DWORD* ptrArray = g_objectListPtrArray;                        // function table pointers
    // D3D object data area (original DAT_008ffcc0; first entry's D3D handle
    // at DAT_008ffd14 = +0x54). Must NOT be addressed as &g_objectCountArray[32]:
    // that's past the end of the count array and only adjacent in the ORIGINAL
    // memory layout — in this build it corrupted neighboring globals.
    DWORD* objDataBase = (DWORD*)g_complexTmdObjectData;
    DWORD* d3dHandle = objDataBase + 0x15;                         // first entry's D3D handle

    int outCount = 0;

    for (int objIdx = 0; objIdx < objCount; objIdx++) {
        unsigned int flags = *objTable;

        if ((flags & 0xFDFFFFFF) == 0x34000609) {
            // Textured triangle primitive - process for D3D rendering

            // Release existing D3D handle
            void** d3dVtable = *(void***)g_pMarniDirect3D;
            typedef void (*DeleteHandleFn)(void*, DWORD);
            DeleteHandleFn deleteHandle = (DeleteHandleFn)d3dVtable[9];
            deleteHandle(g_pMarniDirect3D, *d3dHandle);
            *d3dHandle = 0;

            // Set up rendering state from function table. The table lives in
            // the CMarniViewport2 entry (this = the entry at *ptrArray, ECX in
            // the original, e.g. 0x00486aba) — the vtable methods read the
            // entry's buffers/flags, so self MUST be ptrArray, not funcPtrs.
            void** funcPtrs = (void**)*ptrArray;

            // Call Release/reset [vtable[0]] (0x00486aba: ecx = entry)
            typedef void (__stdcall *ReleaseFn)(void*);
            ((ReleaseFn)funcPtrs[0])(ptrArray);

            // Call CreateWork(3 vertices, 2 primitives, type 3=triangles) [vtable[1]]
            typedef int (__stdcall *CreateWorkFn)(void*, int, int, int);
            ((CreateWorkFn)funcPtrs[1])(ptrArray, 3, 2, 3);

            // Call Lock(NULL, NULL) [vtable[6]]
            typedef int (__stdcall *LockFn)(void*, void*, void*);
            ((LockFn)funcPtrs[6])(ptrArray, NULL, NULL);

            // Find matching texture/material entry.
            // texID comes from the TMD OBJECT entry's texture word
            // (0x00486ae3: mov esi, [objTable+4]; shr esi, 0x10), NOT from the
            // ptrArray entry (its +4 is m_pVertexBuffer, always 0 here — that
            // source made the matcher always pick material 0).
            unsigned int texID = objTable[1] >> 16;
            int matCount = *(int*)(texBank + 0x340) - 1;
            int matIdx = 0;
            int matOffset = 0;

            if (matCount > 0) {
                int* matPtr = (int*)(texBank + 0x54);
                while (matIdx < matCount) {
                    if (*matPtr == (int)((texID & 0x3F) << 4) || matPtr[1] == (int)(texID >> 6))
                        break;
                    matPtr += 0x1A;  // stride 0x68 / 4
                    matIdx++;
                    matOffset += 0x68;
                }
            }

            // Get vertex callback [vtable[3]]
            typedef int (__stdcall *SetVertexFn)(void*, int, float*);
            SetVertexFn setVertex = (SetVertexFn)funcPtrs[3];

            // Process 3 vertices
            short vertexPositions[3][3];  // 3 vertices × 3 coordinates (x, y, z)
            int vertexCount = 0;
            unsigned int* vertData = objTable + 1;  // vertex data starts after object header

            // The UV data and vertex indices are interleaved in the object data
            // Each vertex has: UV word (at vertData[n]) + vertex index (at vertData[n+3])
            DWORD* uvData = (DWORD*)(objTable + 1);
            DWORD* idxData = (DWORD*)(objTable + 4);

            for (int v = 0; v < 3; v++) {
                // Get vertex position from TMD vertex data
                unsigned int vertIdx = idxData[v] >> 16;
                short* pos = (short*)(vertexBase + vertIdx * 8);

                float x = (float)(int)pos[0];
                float y = -(float)(int)pos[1];
                float z = (float)(int)pos[2];

                // Build vertex data with position + defaults
                float vertBuf[12];
                vertBuf[0] = x;
                vertBuf[1] = y;
                vertBuf[2] = z;
                for (int s = 3; s < 9; s++) vertBuf[s] = 1.0f;
                vertBuf[9] = 1.0f;
                vertBuf[10] = 1.0f;
                vertBuf[11] = 1.0f;

                // UV coordinates from the lower bytes of the UV data word
                unsigned int uVal = uvData[v] & 0xFF;
                unsigned int vVal = (uvData[v] >> 8) & 0xFF;
                unsigned int texWidth = *(unsigned int*)(texBank + matOffset + 0x2C);
                unsigned int texHeight = *(unsigned int*)(texBank + matOffset + 0x30);
                if (texWidth > 0) vertBuf[9] = (float)uVal / (float)texWidth;
                if (texHeight > 0) vertBuf[10] = (float)vVal / (float)texHeight;

                // Set vertex (this = the entry, matching the original)
                setVertex(ptrArray, vertexCount, vertBuf);

                // Store position for normal calculation
                vertexPositions[v][0] = pos[0];
                vertexPositions[v][1] = pos[1];
                vertexPositions[v][2] = pos[2];

                vertexCount++;
            }

            // Calculate face normal as average of 3 vertex positions
            short* normalOut = (short*)faceData;
            normalOut[0] = (short)((int)vertexPositions[0][0] + (int)vertexPositions[1][0] + (int)vertexPositions[2][0]) / 3;
            normalOut[1] = (short)((int)vertexPositions[0][1] + (int)vertexPositions[1][1] + (int)vertexPositions[2][1]) / 3;
            normalOut[2] = (short)((int)vertexPositions[0][2] + (int)vertexPositions[1][2] + (int)vertexPositions[2][2]) / 3;

            // Call SetList for both primitives [vtable[5]]
            //
            // The indices are CONSTANTS, not the TMD data. The original builds
            // them once in a stack blob before the loop -
            //   ebp-0x60: 0x00010000, then 2  -> {0, 1, 2}
            //   ebp-0x58: 0, 2, 1              -> {0, 2, 1}
            // - i.e. the same 3 vertices wound both ways, which is what
            // "CreateWork(3 vertices, 2 primitives, type 3)" describes: one
            // double-sided triangle.
            //
            // Passing `objTable` here instead handed SetList the primitive
            // header (0x34000609) as vertex indices. SetList bounds-checks them
            // against m_vertexCount and calls Release() when they fail, so every
            // element freed its own buffers and zeroed its counts the instant
            // after it was built - the whole pool ended up empty and Plant 42's
            // tendrils drew nothing.
            static const WORD kTriIndices[2][3] = { { 0, 1, 2 }, { 0, 2, 1 } };
            typedef int (__stdcall *SetListFn)(void*, int, const void*);
            SetListFn setList = (SetListFn)funcPtrs[5];
            setList(ptrArray, 0, kTriIndices[0]);
            setList(ptrArray, 1, kTriIndices[1]);

            // Call Unlock [vtable[7]] (this = the entry)
            ((ReleaseFn)funcPtrs[7])(ptrArray);

            // Set object properties
            d3dHandle[-0x15] = 4;   // object type = 4
            d3dHandle[0x0B] = 2;    // flags = 2

            // Create D3D object handle via g_pMarniDirect3D vtable[7]
            typedef DWORD (*CreateHandleFn)(void*, DWORD, DWORD);
            CreateHandleFn createHandle = (CreateHandleFn)d3dVtable[7];
            DWORD handle = createHandle(g_pMarniDirect3D, 0, 0);
            *d3dHandle = handle;
            d3dHandle[1] = *(DWORD*)(texBank + matIdx * 4 + 0x34C);

            // Set scale to 1.0
            d3dHandle[-3] = 0x3F800000;  // scaleX = 1.0f
            d3dHandle[-2] = 0x3F800000;  // scaleY = 1.0f
            d3dHandle[-1] = 0x3F800000;  // scaleZ = 1.0f
            d3dHandle[2] = 0x3F800000;
            d3dHandle[3] = 0x3F800000;
            d3dHandle[4] = 0x3F800000;

            // Copy scale to secondary location
            d3dHandle[6] = d3dHandle[2];
            d3dHandle[7] = d3dHandle[3];
            d3dHandle[8] = d3dHandle[4];
            d3dHandle[9] = d3dHandle[5];

            // Copy 0x21 DWORDs (0x84 bytes) of object data to secondary buffer
            // In original binary: from EBP-0x54 to EBP+0x83ac (256 entries ahead)
            DWORD* src = d3dHandle - 0x15;
            DWORD* dst = d3dHandle + 0x20EB;  // 0x83ac / 4 = 0x20EB
            for (int c = 0; c < 0x21; c++) {
                dst[c] = src[c];
            }

            // Advance output pointers
            faceData += 8;
            ptrArray += 0x0E;     // 0x38 / 4 = 0x0E
            d3dHandle += 0x21;    // 0x84 / 4 = 0x21
            outCount++;
        }

        // Check bounds on pointer array
        if (ptrArray >= (DWORD*)((BYTE*)g_objectListPtrArray + 0x3800)) break;

        // Advance to next TMD object (variable-size entries)
        unsigned int advance = ((flags & 0xFF00) >> 6) + 4;
        objTable = (unsigned int*)((BYTE*)objTable + advance);
    }

    g_objectListCleanupCount = outCount;
    param_1[4] = 1;

}

// ============================================================================
// FUN_00482f20 (0x00482f20) - Create animation object entry
// Sets up an animation object with data from the animation header.
// The result is written to param2 and returned as param2 + 0x2d.
// ============================================================================
unsigned int* CreateAnimObject(int slotPtr, unsigned int* param2)
{
    *(unsigned int**)(slotPtr + 0xc) = param2;
    AnimSlot* slot = *(AnimSlot**)(slotPtr + 8);
    *param2 = (unsigned int)slot;
    unsigned int uVar1 = FindMinClutDepth(slot);
    param2[1] = uVar1;
    param2[2] = 0xFFFFFFFF;
    param2[3] = 0xFFFFFFFF;
    param2[4] = 0;
    param2[6] = 0;
    param2[5] = 0;

    if (DAT_004d2bd8 != 0) {
        ComplexTmdObjectSetup((int*)param2);
        DAT_004d2bd8 = 0;
        return param2 + 0x2d;
    }

    param2[8] = 0;
    unsigned int uVar2 = AsyncCreateTmdObject(param2[1], *param2, (unsigned int)param2);
    param2[8] = uVar2;
    return param2 + 0x2d;
}

// ============================================================================
// FUN_00473da0 (0x00473da0) - Process TMD texture entries
// Iterates through TMD texture entries and applies texture bank/depth settings.
// param1: mode (0=no change, 1=set depth, 2=set bank+depth)
// param2: pointer to TMD data
// param3: texture bank ID
// param4: texture depth byte
// ============================================================================
unsigned int ProcessTmdTextures(char param1, unsigned int* param2, int param3, int param4)
{
    unsigned int uVar1 = *param2;

    if (param2[1] == 0) {
        ResolveAnimPointers((unsigned char*)(param2 + 1));
    }

    int iVar3 = param2[2];
    unsigned int* cursor = param2 + 3;

    while (iVar3 != 0) {
        unsigned int* puVar2 = (unsigned int*)cursor[4];
        for (int iVar4 = cursor[5]; iVar4 != 0; iVar4--) {
            if ((*puVar2 & 0x4000000) != 0) {
                if (param1 == 0) {
                    puVar2[2] = puVar2[2] + param3 * 0x10000;
                } else {
                    if (param1 == 1) {
                        puVar2[1] = puVar2[1] + param4 * 0x400000;
                    } else if (param1 == 2) {
                        puVar2[2] = puVar2[2] + param3 * 0x10000;
                        puVar2[1] = puVar2[1] + param4 * 0x400000;
                    }
                }
            }
            puVar2 = puVar2 + ((*puVar2 >> 8) & 0xff) + 1;
        }
        cursor += 7;
        iVar3--;
    }
    return uVar1;
}

// ============================================================================
// FUN_00483900 (0x00483900) - Clear TMD processing flag
// ============================================================================
void ClearTmdProcessingFlag(void)
{
    DAT_004d2bf4 = 0;
}

// ============================================================================
// FUN_00411e00 (0x00411e00) - Set sprite buffer flag
// Sets g_SpriteBufferFlag = 1, indicating sprite data needs processing.
// ============================================================================
void SetSpriteBufferFlag(void)
{
    g_SpriteBufferFlag = 1;
}

// ============================================================================
// FUN_00473ad0 (0x00473ad0) - Queue texture for processing
// Adds a texture reference to the processing queue.
// Returns the queue index on success, 0xFF if queue is full.
// ============================================================================
unsigned char QueueTextureForProcessing(char param1, unsigned char param2)
{
    if (DAT_00ae9f04 > 3) {
        return 0xFF;
    }
    unsigned int idx = DAT_00ae9f04;
    // BYTE stores, not words. The original is `MOV byte ptr [EAX+0xD22740],DL`
    // and `MOV byte ptr [EAX+0xD22742],DL` (0x00473aec / 0x00473af9). Writing
    // 16 bits clobbered the two bytes after each: entry[1] is the "armed" flag,
    // and entry[3] is the RED tint accumulator - `param1 - 10` is negative for
    // param1 < 10, so its high byte seeded entry[3] to 0xFF (-1) and knocked the
    // whole death/fade colour ramp one step out.
    unsigned char* e = g_textureQueueData + idx * 10;
    e[0] = param2;
    e[2] = (unsigned char)(param1 - 10);
    DAT_00ae9f04++;
    return (unsigned char)idx;
}

// ============================================================================
// FUN_00482d40 (0x00482d40) - Parse TMD texture header
// Parses EMD/TMD texture data header into a structured form.
// param_1: pointer to texture data section at offset 4 of the TMD buffer
// header:  output struct (28 bytes)
// ============================================================================
void ParseTmdTextureHeader(void* data, TmdTextureHeader* header)
{
    DWORD* d = (DWORD*)data;

    header->count = (int)(short)(*(short*)data & 0xF);

    short* src = (short*)((BYTE*)data + 8);
    header->field_10 = *src++;
    header->field_12 = *src++;
    header->field_14 = *src++;
    header->field_16 = *src++;

    header->ptr_10 = (int)((BYTE*)data + 0x10);

    DWORD offset = d[1] & 0xFFFFFFFC;
    src = (short*)((BYTE*)data + offset + 8);
    header->field_04 = *src++;
    header->field_06 = *src++;
    header->field_08 = *src++;
    header->field_0A = *src++;

    header->dataPtr = (int)((BYTE*)data + offset + 0x10);
}

// ============================================================================
// FUN_00483770 (0x00483770) - TMD processing callback
// Processes TMD texture data asynchronously. Called via ExecAsync.
// Handles texture page allocation, CLUT setup, and PSXTexture loading.
// ============================================================================
void TmdProcessingCallback(void)
{
    BYTE* tmdData = (BYTE*)g_tmdAsyncData;
    TmdTextureHeader header;

    ParseTmdTextureHeader(tmdData + 4, &header);

    void* psvTex;
    if (g_TextureBankID == 0x16) {
        g_objectCountArray[22] = 1;
        psvTex = &g_psxTextureArray[0x16 * 0x1b60];
    } else {
        int count = g_objectCountArray[g_TextureBankID];
        if (count >= 8) {
            return;
        }
        g_objectCountArray[g_TextureBankID] = count + 1;
        psvTex = &g_psxTextureArray[(count + g_TextureBankID * 8) * 0x36C];
    }

    DWORD* puVar7 = (DWORD*)(tmdData + 0xC);
    DWORD savedVar7 = *puVar7;

    DWORD subOffset = *(DWORD*)(tmdData + 8) & 0xFFFFFFFC;
    DWORD* puVar1 = (DWORD*)(tmdData + subOffset + 0xC);
    DWORD savedVar1 = *puVar1;

    *puVar7 = ((unsigned int)(g_TextureCurrentPage + 0x1E0) << 16) | (header.field_10 & 0xFFFF);
    *puVar1 = ((unsigned int)(g_TextureBankID & 0x10) << 20) | ((unsigned int)(g_TextureBankID & 0xF) << 6);

    VideoDriver_ClearState348(psvTex, g_pMarniDirect3D);
    ((PSXTexture*)psvTex)->Store((int*)tmdData, 1);

    *puVar7 = savedVar7;
    *puVar1 = savedVar1;

    int pageCount = (int)(((header.field_08 & 0xFFFF) + 0x3F) >> 6);

    if (pageCount != 0) {
        DWORD* allocPtr = &g_tmdTextureAllocated[g_TextureBankID];
        for (int i = pageCount; i != 0; i--) {
            *allocPtr++ = DAT_004d2bf4;
        }

        DWORD* redirectPtr = &g_textureBankRedirect[g_TextureBankID];
        for (int i = pageCount; i != 0; i--) {
            *redirectPtr++ = g_TextureBankID;
        }
    }

    g_TextureBankID = g_TextureBankID + (unsigned char)pageCount;
    g_TextureCurrentPage = g_TextureCurrentPage + (unsigned char)(header.field_16 & 0xFF);
    DAT_004d2bf4 = 1;
}

// ============================================================================
// FUN_004838e0 (0x004838e0) - Async TMD processing
// Schedules TMD texture processing asynchronously.
// ============================================================================
void ProcessTmdAsync(unsigned int param1)
{
    g_tmdAsyncData = param1;
    ExecAsync((void*)TmdProcessingCallback);
}

// ============================================================================
// reverse_anim_frame_data (0x0048bea0) - Reverse animation frame data order
// Swaps animation entries to reverse the playback order.
// param_1: pointer to joint anim_field (offset 0x0C within JointStruct)
// Externally visible: FUN_0048c020 (SCD opcode 0x0F) also calls this.
// ============================================================================
void reverse_anim_frame_data(int param_1)
{
    AnimSlot* slot = *(AnimSlot**)(param_1 + 8);
    unsigned short count = slot->entryCount;
    int baseAddr = count * 0x1c + (int)slot->data2;

    short* pRot = (short*)(baseAddr - 0x14);
    int* pTiming = (int*)(baseAddr - 8);

    do {
        short tmpRot = pRot[0];
        pRot[0] = pRot[2];
        pRot[2] = tmpRot;

        int tmpTiming = pTiming[0];
        pTiming[0] = pTiming[1];
        pTiming[1] = tmpTiming;

        count = count - 1;
        pRot = (short*)((int)pRot - 0x1c);
        pTiming = (int*)((int)pTiming - 0x1c);
    } while (count != 0);
}

// ============================================================================
// SetupEntityJointAnimation (0x0048bef0) - Entity joint animation copy and setup
// Copies entity joint data to the load buffer, resolves animation pointers,
// and creates animation objects for each joint.
// ============================================================================
void SetupEntityJointAnimation(void)
{
    // 0x0048bef0: Save load data pointer to entity weapon joints ptr
    ENTITY->weaponJointsPtr = (unsigned int)g_loadDataDestPointer;
    int jointBase = (int)g_loadDataDestPointer;

    // 0x0048bf05: Advance load pointer past joint data
    unsigned char jointCount = ENTITY->jointCount;
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + (unsigned int)jointCount * 0x7c;

    // 0x0048bf1e: Copy animation slot data
    JointStruct* joints = ENTITY->jointsStructs;
    int* animSlotSrc = (int*)joints->anim_slot_ptr;
    int animEnd = *animSlotSrc;
    memcpy(g_loadDataDestPointer, animSlotSrc, animEnd - (int)animSlotSrc);

    // 0x0048bf37: Copy joint structs
    memcpy((void*)jointBase, joints, (unsigned int)jointCount * 0x7c);

    // 0x0048bf4d: Set up new animation slot base
    DAT_00be0e00 = (int)g_loadDataDestPointer;
    *(int*)(jointBase + 0x14) = (int)g_loadDataDestPointer;
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + (animEnd - (int)animSlotSrc & 0xFFFFFFFCU);

    // 0x0048bf6c: Save new and original anim slot pointers for delta fixup
    int newAnimSlotPtr = *(int*)(jointBase + 0x14);
    unsigned int origAnimSlotPtr = (unsigned int)joints->anim_slot_ptr;

    // 0x0048bf7e: Process each joint
    unsigned char j = 0;
    if (jointCount != 0) {
        unsigned char nextJ;
        do {
            int animFieldAddr = jointBase + 0x0c;
            nextJ = j + 1;

            SetAnimSlot((AnimSlot*)DAT_00be0e00, animFieldAddr, j);

            // Point data_ptr to &scale_flag
            *(int*)(jointBase + 0x10) = jointBase + 0x20;

            // Fix up animation data pointer with relocation delta
            int* fixupPtr = (int*)(*(int*)(jointBase + 0x14) + 0x10);
            *fixupPtr = *fixupPtr + (newAnimSlotPtr - (int)origAnimSlotPtr);

            reverse_anim_frame_data(animFieldAddr);

            g_loadDataDestPointer = CreateAnimObject(animFieldAddr, (unsigned int*)g_loadDataDestPointer);

            jointBase = jointBase + 0x7c;
            j = nextJ;
        } while (nextJ < jointCount);
    }
}
