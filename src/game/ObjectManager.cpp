// ObjectManager.cpp - Object lifecycle management (decompiled from Ghidra)
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "SpriteRenderer.h"
#include "TmdRenderer.h"     // TMD_CLEANUP_SLOT_COUNT - see the slot-map note there

// ============================================================================
// ObjectList_Cleanup (0x00487040)
// Cleans up the object list: releases object handles and calls destructors
// ============================================================================
void ObjectList_Cleanup(void)
{
    if (g_objectListCleanupFlag == 1) {
        DWORD* basePtr = g_objectListPtrArray;
        DWORD* countPtr = (DWORD*)g_complexTmdObjectData;   // DAT_008ffcc0 (object data area)
        int i = 0;
        if (0 < g_objectListCleanupCount) {
            do {
                i = i + 1;

                // Call vtable[9] (DeleteObjectHandle) on countPtr[0x15]
                void** d3dVtable = *(void***)g_pMarniDirect3D;
                ((void(*)(void*))d3dVtable[9])((void*)countPtr[0x15]);
                countPtr[0x15] = 0;

                // Call Release (vtable[0]) on the entry. Original 0x0048707f:
                // mov ecx, edi (this = the entry); call [vtable]. The port's
                // adapters are __stdcall with self as the first stack argument,
                // so self must be basePtr (the entry), not the vtable.
                void** funcPtr = (void**)*basePtr;
                if (funcPtr) {
                    ((void (__stdcall*)(void*))*funcPtr)(basePtr);
                }

                basePtr = basePtr + 0xe;     // advance by 14 DWORDs (0x38 bytes)
                countPtr = countPtr + 0x21;  // advance by 33 DWORDs (0x84 bytes)
            } while (i < g_objectListCleanupCount);
        }
        g_objectListCleanupCount = 0;
        g_objectListCleanupFlag = 0;
    }
}

// ============================================================================
// VideoDriver_ReleaseResources (0x00421150)
// Releases the 8 texture handles at this+0x34C via vtable[8] (DeleteTextureHandle),
// zeroes the array and clears this+0x348. ECX = obj in the original.
// ============================================================================
int VideoDriver_ReleaseResources(void* obj, void* context)
{
    if (context == NULL) {
        return 0;
    }
    void** ctxVtable = *(void***)context;
    if (ctxVtable == NULL) {
        return 0;
    }

    // Release 8 texture handles stored at obj+0x34C
    DWORD* handleArray = (DWORD*)((BYTE*)obj + 0x34C);
    typedef void (*DeleteTextureFn)(void* self, DWORD handle);
    DeleteTextureFn deleteTex = (DeleteTextureFn)ctxVtable[8];
    for (int i = 0; i < 8; i++) {
        deleteTex(context, handleArray[i]);
    }
    for (int i = 0; i < 8; i++) {
        handleArray[i] = 0;
    }
    *(DWORD*)((BYTE*)obj + 0x348) = 0;
    return 1;
}

// ============================================================================
// Direct3DTIM_Create (FUN_00421070, 0x00421070)
// Creates the D3D texture handles for a texture page (up to 8 materials).
// ECX = pagePtr in the original; param_2 = g_pMarniDirect3D.
// ============================================================================
int Direct3DTIM_Create(void* pagePtr, void* context)
{
    VideoDriver_ReleaseResources(pagePtr, context);

    if (*(int*)((BYTE*)pagePtr + 0x344) == 0) {
        // Original: printf(&DAT_004b4684, "MarniSystem Direct3DTIM::Create")
        *(DWORD*)((BYTE*)pagePtr + 0x348) = 0;
        return 0;
    }
    if (*(DWORD*)((BYTE*)pagePtr + 0x340) >= 9) {
        // Original: printf(&DAT_004ba1fc, "MarniSystem Direct3DTIM::Create")
        *(DWORD*)((BYTE*)pagePtr + 0x348) = 0;
        return 0;
    }

    DWORD* handlePtr = (DWORD*)((BYTE*)pagePtr + 0x34C);
    for (int i = 0; i < 8; i++) {
        handlePtr[i] = 0;
    }

    if (*(int*)((BYTE*)pagePtr + 0x340) != 0) {
        void** vtable = *(void***)context;
        typedef DWORD (*CreateTextureFn)(void*, BYTE*, unsigned int, void*);
        CreateTextureFn createTex = (CreateTextureFn)vtable[6];

        BYTE* matEntry = (BYTE*)pagePtr;
        DWORD k = 0;
        do {
            DWORD handle = createTex(context, matEntry, 0x29, matEntry + 0x64);
            handlePtr[k] = handle;
            if (handle == 0) {
                // Original: printf(&DAT_004b4664, "MarniSystem Direct3DTIM::Create")
                *(DWORD*)((BYTE*)pagePtr + 0x348) = 0;
                return 0;
            }
            matEntry += 0x68;
            k++;
        } while (k < *(DWORD*)((BYTE*)pagePtr + 0x340));
    }

    *(DWORD*)((BYTE*)pagePtr + 0x348) = 1;
    return 1;
}

// ============================================================================
// VideoDriver_ClearState348 (0x004211b0)
// Clears texture state at this+0x348: releases handles then zeros the array
// ============================================================================
int __stdcall VideoDriver_ClearState348(void* obj, void* context)
{
    // Guard: if context is NULL, there is no D3D system to release handles with
    if (context == NULL) {
        return 0;
    }

    // VideoDriver_ReleaseResources (0x00421150): release the 8 handles at
    // obj+0x34C via vtable[8], zero them, clear obj+0x348
    if (VideoDriver_ReleaseResources(obj, context) == 0) {
        return 0;
    }

    // VideoDriver_ClearArrayD0 — call PSXTexture::ClearCLUTEntries on obj
    ((PSXTexture*)obj)->ClearCLUTEntries();

    return 1;
}

// ============================================================================
// ObjectCleanupCallback (0x00483e00)
// Async callback for Object_DeleteAll: iterates object arrays and cleans up
// ============================================================================
void ObjectCleanupCallback(void)
{
    g_objectDeleteFlag = 0;

    OutputDebugStringA("[ObjectCleanup] start first pass\n");

    // First pass: iterate g_objectCountArray and clear the texture pages of
    // each bank. The original walks 0x00a75168 (g_psxTextureArray), NOT the
    // TMD object buffer — using g_tmdObjectBuffer here corrupted TMD slots.
    for (int i = 0; i < 32; i++) {
        if (i == 22) {
            g_objectCountArray[22] = 1;
        } else {
            int count = g_objectCountArray[i];
            char* basePtr = (char*)&g_psxTextureArray[0] + i * 0x1b60;
            for (int j = 0; j < count; j++) {
                VideoDriver_ClearState348(basePtr + j * 0x36c, g_pMarniDirect3D);
            }
            g_objectCountArray[i] = 0;
        }
    }

    OutputDebugStringA("[ObjectCleanup] start second pass\n");

    // Second pass: zero memory and cleanup TMD objects.
    //
    // 250 slots exactly, matching the original's `MOV EDI,0x923b50` /
    // `CMP ESI,0x3e8` loop.
    //
    // This must stay confined to g_tmdObjectBuffer. A stage-changing transition
    // runs this cleanup while the door animation task is still drawing, so
    // anything it reaches gets destroyed mid-animation - which is why the door
    // slots are their own region (g_doorTmdSlotBuffer, the original's
    // 0x009104c8) and not carved out of the buffer swept here.
    g_objectDeleteCounter = 0;
    char* tmdBase = (char*)&g_tmdObjectBuffer[0];
    for (int i = 0; i < TMD_CLEANUP_SLOT_COUNT; i++) {
        if (g_objectDeletePtr) {
            g_objectDeletePtr[i] = 0;
        }

        // Call VideoDriver_CleanupObjects on each CMarniDirect3DTMD
        // Equivalent to CMarniDirect3DTMD::CleanupObjects
        CMarniDirect3DTMD* tmd = (CMarniDirect3DTMD*)(tmdBase + i * 0x1594);
        tmd->CleanupObjects(g_pMarniDirect3D);
    }

    OutputDebugStringA("[ObjectCleanup] end second pass, calling ObjectList_Cleanup\n");

    ObjectList_Cleanup();

    OutputDebugStringA("[ObjectCleanup] done\n");
}

// ============================================================================
// FUN_00484e70 (0x00484e70)
// Called by CleanupWrapper: clears specific render-state objects
// ============================================================================
void FUN_00484e70(void)
{
    // Clear a specific PSXTexture+aux object at g_renderStateTex
    VideoDriver_ClearState348(g_renderStateTex, g_pMarniDirect3D);

    // Cleanup a specific TMD object at g_renderStateTMD
    CMarniDirect3DTMD* tmd = (CMarniDirect3DTMD*)g_renderStateTMD;
    tmd->CleanupObjects(g_pMarniDirect3D);
}

// ============================================================================
// CleanupWrapper (0x00484ea0)
// Wrapper that schedules FUN_00484e70 asynchronously
// ============================================================================
void CleanupWrapper(void)
{
    ExecAsync((void*)FUN_00484e70);
}

// ============================================================================
// Object_DeleteAll (0x00483680)
// Schedules cleanup callbacks, then jumps to CleanupWrapper
// ============================================================================
void Object_DeleteAll(int a)
{
    ExecAsync((void*)ObjectCleanupCallback);
    CleanupWrapper();
}

// ============================================================================
// setSomeColor (0x00470a50)
// Sets the global color values used by sprite/effect rendering
// ============================================================================
void setSomeColor(int r, int g, int b)
{
    g_color_r = (float)r * 0.0078125;
    g_color_g = (float)g * 0.0078125;
    g_color_b = (float)b * 0.0078125;
}

// ============================================================================
// object_delete_00442170 (0x00442170)
//
// A single RET in the retail exe - verified at the byte level, not just an
// empty Ghidra decompilation. room_set calls it thirteen times with a category
// number that only ever counts up (1..0xd, one after each stage of the load),
// and room_transition_load calls it once more with 0. That is the signature of
// a debug or profiling checkpoint whose body was compiled out of the release
// build, not of an object deleter: nothing reads the argument.
//
// The name is Ghidra's guess and is kept only so the call sites stay greppable.
// ============================================================================
void object_delete_00442170(int category) { (void)category; }
