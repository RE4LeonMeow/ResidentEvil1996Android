// Marni3DObject.cpp - 3D object / execute buffer / polyhedra / TMD implementations
// All functions decompiled from Ghidra with original addresses
#include "Marni3DObject.h"
#include "DebugPrint.h"
// Safe ODS wrapper (raw OutputDebugStringA fail-fasts on switched task stacks)
#ifndef OutputDebugStringA
#define OutputDebugStringA(s) dbg_safe_str((const char*)(s))
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

extern void* operator_new(size_t size);
extern void  operator_delete(void* ptr);

// ============================================================================
// CDirect3DObject base class
// ============================================================================

// CDirect3DObject constructor
// Original: FUN_00430e80 (calls FUN_004272e0 base init, sets CMarniViewport3_vtable)
CDirect3DObject::CDirect3DObject()
{
    // 0x00430e80 / 0x004272e0 - base initialization
    vtable               = 0;
    m_pVertexBuffer      = 0;
    m_pIndexBuffer       = 0;
    m_bHasBuffers        = 0;
    m_flag10             = 0;
    m_locked             = 0;
    m_flag18             = 0;
    m_unknown1C          = 1;   // m_flag18 equivalent: set to 1 by base init
    m_unknown20          = 0;
    m_vertexCount        = 0;
    m_listCount          = 0;
    m_primitiveType      = 0;
    m_vertexCapacity     = 0;
    m_listCapacity       = 0;
}

// Virtual destructor
// Covers the original Direct3DObject_BaseDestructor (0x00430ef0): swaps the
// vtable to CMarniViewport3_vtable and free()s the pointer at +0x38. In the
// shipped binary nothing ever allocates +0x38 for these objects (the base
// ctor at 0x00430e80 zeroes it and no writer exists), so that free is a
// no-op; the buffers that DO carry allocations (+0x04 vertices, +0x08
// indices) are released by Release() below, which the port keeps.
CDirect3DObject::~CDirect3DObject()
{
    Release();
}

// ============================================================================
// CDirect3DObject::Release (vtable[0]) - 0x00427270
// ============================================================================
int CDirect3DObject::Release()
{
    // 0x00427270
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked != 0) {
        // Original called Unlock via vtable[7]; direct call is equivalent
        Unlock();
    }
    if (m_flag10 != 0 && m_flag18 != 0) {
        free(m_pVertexBuffer);
        free(m_pIndexBuffer);
    }
    m_pVertexBuffer   = 0;
    m_pIndexBuffer    = 0;
    m_locked          = 0;
    m_flag10          = 0;
    m_bHasBuffers     = 0;
    m_flag18          = 0;
    m_unknown20       = 0;
    m_listCount       = 0;
    m_vertexCount     = 0;
    m_primitiveType   = 0;
    m_vertexCapacity  = 0;
    m_listCapacity    = 0;
    return 1;
}

// ============================================================================
// CDirect3DObject::CreateWork (vtable[1]) - 0x00415e90
// ============================================================================
int CDirect3DObject::CreateWork(int vtxCount, int listCount, int primType)
{
    // 0x00415e90
    // Call Release (vtable[0]) to free existing buffers
    Release();

    DWORD indexSize;
    if (primType == 3) {
        indexSize = listCount * 8;    // 3 indices × 2 bytes + 2-byte opcode
    } else if (primType == 4) {
        indexSize = listCount * 16;   // 2 tri × (3 indices × 2 + 2-byte opcode)
    } else {
        // Original: printf(&DAT_004b47b0, "Direct3DObject::CreateWork")
        return 0;
    }

    m_pIndexBuffer = operator_new(indexSize);
    if (m_pIndexBuffer == 0) {
        // Original: printf(&DAT_004b478c, "Direct3DObject::CreateWork")
        return 0;
    }

    m_pVertexBuffer = operator_new(vtxCount * 32);
    if (m_pVertexBuffer == 0) {
        // Original: printf(&DAT_004b4768, "Direct3DObject::CreateWork")
        return 0;
    }

    m_primitiveType  = primType;
    m_bHasBuffers    = 1;
    m_flag10         = 1;
    m_flag18         = 1;
    m_vertexCount    = vtxCount;
    m_vertexCapacity = vtxCount;
    m_listCount      = listCount;
    m_listCapacity   = listCount;
    return 1;
}

// ============================================================================
// CDirect3DObject::GetVertex (vtable[2]) - 0x00415a80
// ============================================================================
int CDirect3DObject::GetVertex(int index, DWORD* outData)
{
    // 0x00415a80
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_vertexCapacity <= (DWORD)index) {
        return 0;
    }

    int offset = index * 8;  // 32 bytes per vertex / 4 = 8 DWORDs
    DWORD* vtx = (DWORD*)m_pVertexBuffer;

    outData[0]  = vtx[offset + 0];   // sx
    outData[1]  = vtx[offset + 1];   // sy
    outData[2]  = vtx[offset + 2];   // sz
    outData[3]  = vtx[offset + 3];   // rhw
    outData[4]  = vtx[offset + 4];   // color
    outData[5]  = vtx[offset + 5];   // specular
    outData[9]  = vtx[offset + 6];   // tu
    outData[10] = vtx[offset + 7];   // tv
    return 1;
}

// ============================================================================
// CDirect3DObject::SetVertex (vtable[3]) - 0x00415b40
// ============================================================================
int CDirect3DObject::SetVertex(int index, DWORD* data)
{
    // 0x00415b40
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_vertexCapacity <= (DWORD)index) {
        return 0;
    }

    int offset = index * 8;
    DWORD* vtx = (DWORD*)m_pVertexBuffer;

    vtx[offset + 0] = data[0];   // sx
    vtx[offset + 1] = data[1];   // sy
    vtx[offset + 2] = data[2];   // sz
    vtx[offset + 3] = data[3];   // rhw
    vtx[offset + 4] = data[4];   // color
    vtx[offset + 5] = data[5];   // specular
    vtx[offset + 6] = data[9];   // tu
    vtx[offset + 7] = data[10];  // tv
    return 1;
}

// ============================================================================
// CDirect3DObject::GetList (vtable[4]) - 0x00415c10
// ============================================================================
int CDirect3DObject::GetList(int index, WORD* outIndices)
{
    // 0x00415c10
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }

    WORD* idx = (WORD*)m_pIndexBuffer;

    if (m_primitiveType == 3) {
        // Triangle: 8 bytes per entry = 4 WORDs
        if (m_listCapacity <= (DWORD)index) {
            return 0;
        }
        int off = index * 4;
        outIndices[0] = idx[off + 0];
        outIndices[1] = idx[off + 1];
        outIndices[2] = idx[off + 2];
        return 1;
    }

    if (m_primitiveType == 4) {
        // Quad: 16 bytes per entry = 8 WORDs (two triangles)
        if (m_listCapacity <= (DWORD)index) {
            return 0;
        }
        int off = index * 8;
        outIndices[0] = idx[off + 0];   // v0
        outIndices[1] = idx[off + 1];   // v1
        outIndices[2] = idx[off + 2];   // v2
        outIndices[3] = idx[off + 6];   // v3 (offset 12 bytes = WORD index 6)
        return 1;
    }

    return 1;  // Unknown primitive type: no-op
}

// ============================================================================
// CDirect3DObject::SetList (vtable[5]) - 0x00415d20
// ============================================================================
int CDirect3DObject::SetList(int index, WORD* indices)
{
    // 0x00415d20
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_listCapacity <= (DWORD)index) {
        return 0;
    }

    WORD* idx = (WORD*)m_pIndexBuffer;

    if (m_primitiveType == 3) {
        // Triangle: write 3 vertex indices + 0x700 execute buffer command marker
        int off = index * 4;
        idx[off + 0] = indices[0];
        idx[off + 1] = indices[1];
        idx[off + 2] = indices[2];
        idx[off + 3] = 0x700;   // Marni execute buffer opcode
        return 1;
    }

    if (m_primitiveType == 4) {
        // Quad: split into 2 triangles (v0,v1,v2) and (v0,v2,v3), each with 0x700 marker
        int off = index * 8;
        idx[off + 0] = indices[0];    // tri1: v0
        idx[off + 1] = indices[1];    // tri1: v1
        idx[off + 2] = indices[2];    // tri1: v2
        idx[off + 3] = 0x700;         // tri1: opcode
        idx[off + 4] = indices[0];    // tri2: v0
        idx[off + 5] = indices[2];    // tri2: v2
        idx[off + 6] = indices[3];    // tri2: v3
        idx[off + 7] = 0x700;         // tri2: opcode
        return 1;
    }

    return 1;  // Unknown type: no-op
}

// ============================================================================
// CDirect3DObject::Lock (vtable[6]) - 0x004159e0
// ============================================================================
int CDirect3DObject::Lock(void** outVtx, void** outIdx)
{
    // 0x004159e0
    if (m_bHasBuffers == 0) {
        // Original: printf("MarniPolyhedra::Lock: no buffers")
        return 0;
    }
    if (m_locked != 0) {
        // Original: printf("MarniPolyhedra::Lock: already locked")
        return 0;
    }
    if (outVtx != 0) {
        *outVtx = m_pVertexBuffer;
    }
    if (outIdx != 0) {
        *outIdx = m_pIndexBuffer;
    }
    m_locked = 1;
    return 1;
}

// ============================================================================
// CDirect3DObject::Unlock (vtable[7]) - 0x00415a50
// ============================================================================
int CDirect3DObject::Unlock()
{
    // 0x00415a50
    if (m_locked == 0) {
        return 1;
    }
    m_locked = 0;
    return 1;
}

// ============================================================================
// CMarniExecuteBuffer
// ============================================================================

// 0x00415f70 - constructor
CMarniExecuteBuffer::CMarniExecuteBuffer()
    : CDirect3DObject()
{
    // Original: calls CDirect3DObject constructor (FUN_00430e80)
    // then sets vtable to CMarniExecuteBuffer_vtable (0x004af090)
}

// 0x00415fd0 - destructor
CMarniExecuteBuffer::~CMarniExecuteBuffer()
{
    // Original: sets vtable to CMarniExecuteBuffer_vtable
    // calls base destructor (FUN_00416017 -> FUN_00430ef0 -> FUN_00427320 -> Release)
}

// ============================================================================
// CMarniPolyhedra
// ============================================================================

CMarniPolyhedra::CMarniPolyhedra()
    : CDirect3DObject()
{
}

CMarniPolyhedra::~CMarniPolyhedra()
{
}

// ============================================================================
// CMarniPolyhedra::CopyFrom - 0x00426600
// Copies vertex/index data setup from source polyhedra.
// Original accesses fields via DWORD indexing on a >0x44 byte object.
// Two code paths: initial setup (m_flag10 == 0) and copy path (buffers exist).
// ============================================================================
int CMarniPolyhedra::CopyFrom(CMarniPolyhedra* src)
{
    // 0x00426600 - Cast to int* for byte-exact field access matching Ghidra
    DWORD* pdst = (DWORD*)this;
    DWORD* psrc = (DWORD*)src;

    // pdst[4] = m_flag10 at offset 0x10
    if (pdst[4] == 0) {
        // Initial setup: dest not yet initialized
        // psrc[6] = m_flag18 at offset 0x18
        if (psrc[6] != 0) {
            pdst[8]  = psrc[8];     // m_unknown20 (0x20)
            pdst[1]  = psrc[1];     // m_pVertexBuffer (0x04)
            pdst[2]  = psrc[2];     // m_pIndexBuffer (0x08)
            pdst[3]  = 1;           // m_bHasBuffers (0x0C) = 1
            pdst[6]  = 1;           // m_flag18 (0x18) = 1
            pdst[5]  = 0;           // m_locked (0x14) = 0
            pdst[4]  = 0;           // m_flag10 (0x10) = 0
            pdst[9]  = psrc[9];     // m_vertexCount (0x24)
            pdst[12] = psrc[12];    // m_vertexCapacity (0x30)
            pdst[16] = psrc[16];    // extended field at 0x40 (beyond base class)
            pdst[13] = psrc[13];    // m_listCapacity (0x34)
            pdst[11] = psrc[11];    // m_primitiveType (0x2C)
            pdst[7]  = psrc[7];     // m_unknown1C (0x1C)
            return 1;
        }
        return 0;
    }

    // Update path: both have buffers
    // pdst[3] = m_bHasBuffers, psrc[3] = src->m_bHasBuffers
    if (pdst[3] == 0 || psrc[3] == 0) {
        return 0;
    }

    pdst[9]  = psrc[9];      // m_vertexCount
    pdst[16] = psrc[16];     // extended field at 0x40
    pdst[7]  = 1;            // m_unknown1C = 1
    pdst[8]  = 0;            // m_unknown20 = 0

    return 1;
}

// ============================================================================
// CMarniViewport2 static vtable (original: 0x004af0f8)
// The embedded elements inside a Direct3DTMD slot live in raw BSS memory
// (g_tmdObjectBuffer) and are never C++-constructed, so the original stored
// a real vtable pointer at element+0x00. Raw call sites (CleanupObjects,
// ~CMarniDirect3DTMD) invoke vtable[0] with the element as a plain stack
// argument, so these adapters are __stdcall free functions taking `self`
// as the first parameter.
// ============================================================================
static int __stdcall VP2_Release_Adapter(CMarniViewport2* self)                          { return self->CMarniViewport2::Release(); }
static int __stdcall VP2_CreateWork_Adapter(CMarniViewport2* self, int a, int b, int c)  { return self->CMarniViewport2::CreateWork(a, b, c); }
static int __stdcall VP2_GetVertex_Adapter(CMarniViewport2* self, int i, DWORD* o)       { return self->CMarniViewport2::GetVertex(i, o); }
static int __stdcall VP2_SetVertex_Adapter(CMarniViewport2* self, int i, DWORD* d)       { return self->CMarniViewport2::SetVertex(i, d); }
static int __stdcall VP2_GetList_Adapter(CMarniViewport2* self, int i, WORD* o)          { return self->CMarniViewport2::GetList(i, o); }
static int __stdcall VP2_SetList_Adapter(CMarniViewport2* self, int i, WORD* d)          { return self->CMarniViewport2::SetList(i, d); }
static int __stdcall VP2_Lock_Adapter(CMarniViewport2* self, void** a, void** b)         { return self->CMarniViewport2::Lock(a, b); }
static int __stdcall VP2_Unlock_Adapter(CMarniViewport2* self)                           { return self->CMarniViewport2::Unlock(); }

static void* g_CMarniViewport2VTable[8] = {
    (void*)VP2_Release_Adapter,     // [0] 0x00427270
    (void*)VP2_CreateWork_Adapter,  // [1] 0x00427100
    (void*)VP2_GetVertex_Adapter,   // [2] 0x00426d60
    (void*)VP2_SetVertex_Adapter,   // [3] 0x00426df0
    (void*)VP2_GetList_Adapter,     // [4] 0x00426e80
    (void*)VP2_SetList_Adapter,     // [5] 0x00426f70
    (void*)VP2_Lock_Adapter,        // [6] 0x004271e0
    (void*)VP2_Unlock_Adapter       // [7] 0x00427250
};

// ============================================================================
// MarniViewport2_InitEntry - raw constructor for a CMarniViewport2 element in
// unconstructed BSS memory. The original ctor (0x004272e0) stores vtable
// 0x004af0f8 at +0x00, zeroes +0x04..+0x34, and sets +0x1C (m_unknown1C,
// the base-init marker) to 1. The 256-entry object list at 0x008fc430 and the
// embedded TMD slot elements are never C++-constructed in this port, so the
// same byte pattern is applied on first use.
// ============================================================================
void MarniViewport2_InitEntry(void* entry)
{
    DWORD* e = (DWORD*)entry;
    e[0] = (DWORD)g_CMarniViewport2VTable;   // 0x004af0f8
    for (int i = 1; i < 14; i++) e[i] = 0;   // +0x04 .. +0x34
    e[7] = 1;                                // m_unknown1C (+0x1C)
}

// Debug print helper (matches MarniDebugPrint used by the original)
static void PSXObjDebugPrint(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';
    // Raw OutputDebugStringA is banned in task code: without a debugger the
    // DBG_PRINTEXCEPTION it raises cannot be dispatched on the scheduler's
    // switched stack and Windows fail-fasts the process. See DebugPrint.h.
    dbg_safe_str(buf);
}

// ============================================================================
// CMarniDirect3DTMD
// ============================================================================

// 0x00415910 - constructor
// Original calls Direct3DTMD_BaseInit (0x00446cb0) which constructs 16 embedded
// CDirect3DObject variants (stride 0x4C = 19 DWORDs) at offset 0, then sets
// m_objectCount=0, m_flag4C4=0, m_unknown4C8=0x400. Then zeros m_initialized,
// m_objectData, and m_objectHandles.
CMarniDirect3DTMD::CMarniDirect3DTMD()
{
    // Initialize 16 embedded CDirect3DObject elements at stride 0x4C (76 bytes)
    // These occupy m_pad1, m_pMaterialData, and m_pad2 in our layout
    DWORD* embeddedBase = (DWORD*)this;
    for (int i = 0; i < 16; i++) {
        DWORD* elem = embeddedBase + (i * 19);  // 0x4C / 4 = 19 DWORDs
        // FUN_004272e0 base init pattern (CMarniViewport2_vtable at 0x004af0f8)
        elem[0]  = (DWORD)g_CMarniViewport2VTable;   // vtable
        elem[1]  = 0;   // m_pVertexBuffer
        elem[2]  = 0;   // m_pIndexBuffer
        elem[3]  = 0;   // m_bHasBuffers
        elem[4]  = 0;   // m_flag10
        elem[5]  = 0;   // m_locked
        elem[6]  = 0;   // m_flag18
        elem[7]  = 1;   // m_unknown1C = 1 (base init marker)
        elem[8]  = 0;   // m_unknown20
        elem[9]  = 0;   // m_vertexCount
        elem[10] = 0;   // m_listCount
        elem[11] = 0;   // m_primitiveType
        elem[12] = 0;   // m_vertexCapacity
        elem[13] = 0;   // m_listCapacity
    }

    m_objectCount  = 0;
    m_flag4C4      = 0;
    m_unknown4C8   = 0x400;
    m_pD3DContext  = 0;

    m_initialized  = 0;
    memset(m_objectData,     0, sizeof(m_objectData));
    memset(m_objectDataCopy, 0, sizeof(m_objectDataCopy));
    memset(m_objectHandles,  0, sizeof(m_objectHandles));
}

// 0x00415990 - destructor
// The original's Direct3DTMD_BaseCleanup (0x00446de0) is the cleanup leg of
// this path: it calls FUN_004450a0 (release all 16 embedded elements and
// clear the +0x4C0..+0x4C8 management fields) inside an SEH frame; the port
// folds the SEH glue away and performs the equivalent release here.
CMarniDirect3DTMD::~CMarniDirect3DTMD()
{
    // Original calls base destructor chain for embedded CDirect3DObject elements
    // Each embedded element: set vtable, call FUN_00427320 -> FUN_00427270 (Release)
    DWORD* embeddedBase = (DWORD*)this;
    for (int i = 0; i < 16; i++) {
        DWORD* elem = embeddedBase + (i * 19);  // 0x4C bytes stride
        void** eVtable = (void**)elem[0];
        if (eVtable != 0) {
            // Call Release (vtable[0]) on embedded element
            ((int (__stdcall *)(void*))eVtable[0])(elem);
        }
    }
}

// ============================================================================
// CMarniDirect3DTMD::Create - 0x00415650
// Sets up TMD objects, creates D3D handles, configures per-object data
// ============================================================================
int CMarniDirect3DTMD::Create(void* d3dContext, void* materialContext, void* param3)
{
    // 0x00415650 - decompiled faithfully from Ghidra
    void** d3dVtable = *(void***)d3dContext;

    // Store material context pointer
    m_pD3DContext = d3dContext;

    // Clean up any existing objects first
    Destroy(d3dContext);

    // Validate flags: m_flag4C4 must be set and the D3D context must be ready.
    // The original read d3dContext+0x348 ("context ready"); the port's
    // CMarniDirect3D keeps that flag at m_isInitialized (+0x3C) instead.
    // Port's CMarniDirect3D readiness flag lives at +0x3C (m_isInitialized);
    // the original used +0x348.
    int ctxReady = *(int*)((BYTE*)d3dContext + 0x3C);
    if (m_flag4C4 == 0 || ctxReady == 0) {
        m_initialized = 0;
        return 0;
    }

    // Zero per-object data and handle arrays
    memset(m_objectData,    0, sizeof(m_objectData));
    memset(m_objectHandles, 0, sizeof(m_objectHandles));

    DWORD objCount = m_objectCount;
    DWORD* handles = m_objectHandles;

    // vtable[7] on d3dContext = CreateObjectHandle(embeddedObj, flags)
    typedef int (*CreateObjFn)(void*, void*, void*);
    CreateObjFn createObj = (CreateObjFn)d3dVtable[7];

    if (objCount != 0) {
        // Each embedded CDirect3DObject is at stride 0x4C from base
        BYTE* embeddedBase = (BYTE*)this;
        for (DWORD i = 0; i < objCount; i++) {
            void* elemPtr = embeddedBase + (i * 0x4C);
            int handle = createObj(d3dContext, elemPtr, param3);
            handles[i] = handle;
            if (handle == 0) {
                // Original: printf(&DAT_004b4664, "MarniSystem Direct3DTMD::Create")
                m_initialized = 0;
                return 0;
            }
        }
    }

    // --- Set up per-object TMD data (Ghidra loop at 0x004157e0) ---
    // puVar4 = (DWORD*)(m_objectData + 0x5C) — start offset within per-object data
    // piVar5 = (int*)(this + 0x48) — material data pointer (m_pMaterialData)
    // local_8 = m_objectHandles
    // Loop iterates objCount times, puVar4 advances by 0x84, piVar5 advances by 0x4C

    if (objCount != 0) {
        // puVar4 starts at m_objectData + 0x5C (offset 0x5C into first TMDObjectData)
        DWORD* puVar4 = (DWORD*)(m_objectData + 0x5C);
        // piVar5 = material data entries, stride 0x4C (19 DWORDs)
        DWORD* piVar5 = (DWORD*)(((BYTE*)this) + 0x48);
        // Material context: texture entries at +0x5C, count at +0x340, texture IDs at +0x34C
        BYTE* matCtx = (BYTE*)materialContext;

        for (DWORD i = 0; i < objCount; i++) {
            // puVar4[-0x17] = offset 0x5C - 0x5C = 0x00 of this entry: set primitive type = 4
            puVar4[-0x17] = 4;
            // puVar4[-2] = offset 0x5C - 0x08 = 0x54: store object handle
            puVar4[-2] = handles[i];
            // puVar4[0..2] = scale factors at 0x5C-0x64: set to 1.0f (0x3f800000)
            puVar4[0] = 0x3f800000;  // scaleX = 1.0f
            puVar4[1] = 0x3f800000;  // scaleY = 1.0f
            puVar4[2] = 0x3f800000;  // scaleZ = 1.0f
            puVar4[3] = 0;
            // puVar4[-5]..[-3] = scaleX,Y,Z copy at offset 0x48-0x50
            puVar4[-5] = 0x3f800000;
            puVar4[-4] = 0x3f800000;
            puVar4[-3] = 0x3f800000;
            // puVar4[4..7] = copy of scale/zero
            puVar4[4] = puVar4[0];
            puVar4[5] = puVar4[1];
            puVar4[6] = puVar4[2];
            puVar4[7] = puVar4[3];

            // Texture ID lookup from material context
            // piVar5 = material entry at stride 0x4C
            // *piVar5 == 0 means no material data
            if (*piVar5 == 0) {
                puVar4[-1] = 0;  // textureId = 0
            } else {
                DWORD texCount = *(DWORD*)(matCtx + 0x340);
                DWORD j;
                for (j = 0; j < texCount; j++) {
                    // Texture entries at matCtx + 0x5C, stride 0x68 (26 DWORDs = 0x1A * 4)
                    DWORD* texEntry = (DWORD*)(matCtx + 0x5C + j * 0x68);
                    // Compare material fields: piVar5[-2], piVar5[-1], piVar5[-4], piVar5[-3]
                    // against texEntry fields
                    if (texEntry[0]  == piVar5[-2] &&
                        texEntry[1]  == piVar5[-1] &&
                        texEntry[-2] == piVar5[-4] &&
                        texEntry[-1] == piVar5[-3]) {
                        // Texture ID lookup table at matCtx + 0x34C + j*4
                        puVar4[-1] = *(DWORD*)(matCtx + 0x34C + j * 4);
                        break;
                    }
                }
                if (j == texCount) {
                    // Original: printf(&DAT_004b4638, "MarniSystem Direct3DTMD::Create")
                    m_initialized = 0;
                    return 0;
                }
            }

            // Advance: puVar4 += 0x21 DWORDs = 0x84 bytes, piVar5 += 0x13 DWORDs = 0x4C bytes
            puVar4 += 0x21;
            piVar5 += 0x13;
        }
    }

    // --- Copy per-object data to double-buffer copy ---
    // Copy from m_objectData to m_objectDataCopy (0x21 DWORDs * objCount)
    if (objCount != 0) {
        DWORD* src = (DWORD*)m_objectData;
        DWORD* dst = (DWORD*)m_objectDataCopy;
        for (DWORD i = 0; i < objCount; i++) {
            for (int k = 0; k < 0x21; k++) {  // 0x21 = 33 DWORDs = 0x84 bytes
                dst[k] = src[k];
            }
            src += 0x21;
            dst += 0x21;
        }
    }

    m_initialized = 1;
    return 1;
}

// ============================================================================
// CMarniDirect3DTMD::Transform - 0x00415520
// Queues all TMD objects for rendering with the given OT depth and stores
// the per-object transform matrix.
// Original signature/behavior (from 0x00415520):
//   Transform(ctx, depth, matrix, doubleBuffer)
//     for each object: vtable[10](objData, depth)      // OT_InsertPrimitive
//     if (matrix): copy 16 floats from matrix into objData + 8
// The matrix stored in objData+8 is what the OT renderer later uses to
// transform the object's vertices.
// ============================================================================
int CMarniDirect3DTMD::Transform(void* d3dContext, void* depth, void* matrix, int doubleBuffer)
{
    // 0x00415520
    void** d3dVtable = *(void***)d3dContext;
    int*   ctxFields = (int*)d3dContext;

    if (m_initialized == 0) {
        // Original: printf("Direct3DTMD::Trans: not initialized")
        return 0;
    }

    // Check context readiness: the original used +0x348; the port's
    // CMarniDirect3D keeps it at +0x3C (m_isInitialized).
    if (*(int*)((BYTE*)d3dContext + 0x3C) == 0) {
        // Original: printf("MarniSystem Direct3DTMD::Trans: context not ready")
        return 0;
    }

    DWORD objCount = m_objectCount;

    // vtable[10] = OT insert (original CMarniDirect3D::SetTexture, 0x00448300)
    typedef int (*OTInsertFn)(void*, void*, unsigned int);
    OTInsertFn otInsert = (OTInsertFn)d3dVtable[10];

    // uVar2 = ((doubleBuffer == 0) - 1) & 0x10  (entries, 0x84 bytes each)
    // doubleBuffer==0 -> m_objectData, != 0 -> m_objectDataCopy
    int shift = (doubleBuffer == 0) ? 0 : 16;  // 16 * 0x84 = 0x840

    if (objCount != 0) {
        BYTE* dataPtr = m_objectData + shift * 0x84;
        for (DWORD i = 0; i < objCount; i++) {
            otInsert(d3dContext, dataPtr, (unsigned int)(size_t)depth);
            dataPtr += 0x84;
        }
    }

    // Store the transform matrix into each object at +0x08 (16 floats).
    // NOTE: the original copies FROM the caller's matrix INTO objData+8;
    // an earlier revision of this code copied in the wrong direction.
    if (matrix != 0 && objCount != 0) {
        DWORD* dst = (DWORD*)(m_objectData + 0x08 + shift * 0x84);
        DWORD* src = (DWORD*)matrix;
        for (DWORD i = 0; i < objCount; i++) {
            for (int k = 0; k < 16; k++) {
                dst[k] = src[k];
            }
            dst += 0x21;  // 33 DWORDs stride (0x84 bytes)
        }
    }

    return 1;
}

// ============================================================================
// CMarniDirect3DTMD::Destroy - 0x00415880
// Releases all D3D object handles
// ============================================================================
int CMarniDirect3DTMD::Destroy(void* d3dContext)
{
    // 0x00415880
    if (d3dContext == NULL) {
        return 0;
    }

    void** d3dVtable = *(void***)d3dContext;

    DWORD objCount = m_objectCount;
    DWORD* handles = m_objectHandles;

    // vtable[9] = DeleteObjectHandle
    typedef void (*DeleteObjFn)(void*, DWORD);
    DeleteObjFn deleteObj = (DeleteObjFn)d3dVtable[9];

    if (objCount != 0) {
        for (DWORD i = 0; i < objCount; i++) {
            deleteObj(d3dContext, handles[i]);
            handles[i] = 0;
        }
    }

    m_initialized = 0;
    return 1;
}

// ============================================================================
// CMarniDirect3DTMD::CleanupObjects - 0x004158e0
// Destroys all objects and cleans up embedded CDirect3DObject elements
// ============================================================================
int CMarniDirect3DTMD::CleanupObjects(void* param)
{
    // 0x004158e0 (VideoDriver_CleanupObjects)

    // Destroy all object handles
    Destroy(m_pD3DContext);

    // FUN_004450a0: cleanup the 16 embedded CDirect3DObject elements
    // Each element at stride 0x4C = 19 DWORDs
    // Calls vtable[0] (Release) on each, zeros extended fields
    DWORD* embeddedBase = (DWORD*)this;
    for (int i = 0; i < 16; i++) {
        DWORD* elem = embeddedBase + (i * 19);  // 19 DWORDs = 0x4C stride

        // Call vtable[0] = Release on the embedded element
        void** eVtable = (void**)elem[0];
        if (eVtable != 0) {
            ((int (__stdcall *)(void*))eVtable[0])(elem);
        }

        // Zero extended fields (offsets 0x38, 0x3C, 0x40 within element)
        elem[14] = 0;  // offset 0x38
        elem[15] = 0;  // offset 0x3C
        elem[16] = 0;  // offset 0x40
    }

    // Original: param_1[0x130] = 0; param_1[0x131] = 0;
    m_objectCount = 0;
    m_flag4C4     = 0;

    m_initialized = 0;
    return 1;
}

// ============================================================================
// CMarniDirect3DTMD::ClearObjectData
// Zeroes all per-object data arrays
// ============================================================================
void CMarniDirect3DTMD::ClearObjectData()
{
    memset(m_objectData,     0, sizeof(m_objectData));
    memset(m_objectDataCopy, 0, sizeof(m_objectDataCopy));
    memset(m_objectHandles,  0, sizeof(m_objectHandles));
}

// ============================================================================
// CMarniViewport2 — 3D viewport with float vertices (11 floats = 0x2C bytes)
// VTable at 0x004af0f8
// ============================================================================

// 0x004272e0 — constructor
CMarniViewport2::CMarniViewport2()
{
    // Ghidra shows: *param_1 = &CMarniViewport2_vtable (0x004af0f8);
    // then zero fields [1..13], set [7]=1
    vtable               = g_CMarniViewport2VTable;
    m_pVertexBuffer      = 0;
    m_pIndexBuffer       = 0;
    m_bHasBuffers        = 0;
    m_flag10             = 0;
    m_locked             = 0;
    m_flag18             = 0;
    m_unknown1C          = 1;   // render mode flag: 1 = strip mode (field[7] in Ghidra)
    m_unknown20          = 0;
    m_vertexCount        = 0;
    m_listCount          = 0;
    m_primitiveType      = 0;
    m_vertexCapacity     = 0;
    m_listCapacity       = 0;
    m_renderStyle        = 0;
    m_converted          = 0;
    m_padding40          = 0;
}

// 0x00427320 — destructor
CMarniViewport2::~CMarniViewport2()
{
    // Sets vtable, then calls Release(this)
    // We call Release() directly (same effect)
    Release();
}

// ============================================================================
// CMarniViewport2::Release [vtable 0] — 0x00427270
// ============================================================================
int CMarniViewport2::Release()
{
    // 0x00427270 — identical logic to CDirect3DObject::Release but uses own vtable
    // Accesses fields via param_1[int*] indexing
    DWORD* p = (DWORD*)this;

    if (p[3] == 0) {  // m_bHasBuffers
        return 0;
    }
    if (p[5] != 0) {  // m_locked
        Unlock();
    }
    if ((p[4] != 0) && (p[6] != 0)) {  // m_flag10 && m_flag18
        free(m_pVertexBuffer);
        free(m_pIndexBuffer);
    }
    p[1]  = 0;   // m_pVertexBuffer
    p[2]  = 0;   // m_pIndexBuffer
    p[5]  = 0;   // m_locked
    p[4]  = 0;   // m_flag10
    p[3]  = 0;   // m_bHasBuffers
    p[6]  = 0;   // m_flag18
    p[8]  = 0;   // m_unknown20
    p[10] = 0;   // m_listCount
    p[9]  = 0;   // m_vertexCount
    p[11] = 0;   // m_primitiveType
    p[12] = 0;   // m_vertexCapacity
    p[13] = 0;   // m_listCapacity
    // Note: p[7] (m_unknown1C/render mode) is NOT zeroed
    // Note: p[14], p[15], p[16] (extended fields) are NOT zeroed
    return 1;
}

// ============================================================================
// CMarniViewport2::CreateWork [vtable 1] — 0x00427100
// ============================================================================
int CMarniViewport2::CreateWork(int vtxCount, int polyCount, int polyType)
{
    // 0x00427100 — allocates vertex (0x2C/vtx) + index (type*2/poly) buffers
    // Call Release first to free existing buffers
    Release();

    if (polyType < 3 || polyType > 4) {
        return 0;
    }

    DWORD indexSize = polyType * polyCount * 2;  // bytes: 3*2=6 or 4*2=8 per poly
    m_pIndexBuffer = operator_new(indexSize);
    if (m_pIndexBuffer == 0) {
        return 0;
    }

    m_pVertexBuffer = operator_new(vtxCount * 0x2C);  // 44 bytes per vertex
    if (m_pVertexBuffer == 0) {
        free(m_pIndexBuffer);
        m_pIndexBuffer = 0;
        return 0;
    }

    m_primitiveType  = polyType;
    m_bHasBuffers    = 1;
    m_flag10         = 1;
    m_flag18         = 1;
    m_vertexCount    = vtxCount;
    m_vertexCapacity = vtxCount;
    m_listCount      = polyCount;
    m_listCapacity   = polyCount;
    return 1;
}

// ============================================================================
// CMarniViewport2::GetVertex [vtable 2] — 0x00426d60
// ============================================================================
int CMarniViewport2::GetVertex(int index, DWORD* outData)
{
    // 0x00426d60 — reads 11 floats (0x2C bytes) from vertex buffer
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_vertexCapacity <= (DWORD)index) {
        return 0;
    }

    DWORD* vtx = (DWORD*)((BYTE*)m_pVertexBuffer + index * 0x2C);
    for (int i = 0; i < 11; i++) {
        outData[i] = vtx[i];
    }
    return 1;
}

// ============================================================================
// CMarniViewport2::SetVertex [vtable 3] — 0x00426df0
// ============================================================================
int CMarniViewport2::SetVertex(int index, DWORD* data)
{
    // 0x00426df0 — writes 11 floats to vertex buffer
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_vertexCapacity <= (DWORD)index) {
        return 0;
    }

    DWORD* vtx = (DWORD*)((BYTE*)m_pVertexBuffer + index * 0x2C);
    for (int i = 0; i < 11; i++) {
        vtx[i] = data[i];
    }
    return 1;
}

// ============================================================================
// CMarniViewport2::GetList [vtable 4] — 0x00426e80
// ============================================================================
int CMarniViewport2::GetList(int index, WORD* outIndices)
{
    // 0x00426e80 — reads WORD indices from index buffer
    // Index buffer layout: type 3 = 3 WORDs (6 bytes/poly), type 4 = 4 WORDs (8 bytes/poly)
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_listCapacity <= (DWORD)index) {
        return 0;
    }

    WORD* idx = (WORD*)m_pIndexBuffer;

    if (m_primitiveType == 3) {
        int off = index * 3;   // 3 WORDs × 2 bytes = 6 bytes per entry
        outIndices[0] = idx[off + 0];
        outIndices[1] = idx[off + 1];
        outIndices[2] = idx[off + 2];
        return 1;
    }

    if (m_primitiveType == 4) {
        int off = index * 4;   // 4 WORDs × 2 bytes = 8 bytes per entry
        outIndices[0] = idx[off + 0];
        outIndices[1] = idx[off + 1];
        outIndices[2] = idx[off + 2];
        outIndices[3] = idx[off + 3];
        return 1;
    }

    m_bHasBuffers = 0;
    return 0;
}

// ============================================================================
// CMarniViewport2::SetList [vtable 5] — 0x00426f70
// ============================================================================
int CMarniViewport2::SetList(int index, WORD* indices)
{
    // 0x00426f70 — writes WORD indices with bounds checking
    // Validates all indices < m_vertexCount; calls Release on out-of-bounds
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked == 0) {
        return 0;
    }
    if (m_listCapacity <= (DWORD)index) {
        return 0;
    }

    WORD* idx = (WORD*)m_pIndexBuffer;

    if (m_primitiveType == 3) {
        // Validate all 3 indices < vertexCount
        if (indices[0] >= m_vertexCount || indices[1] >= m_vertexCount || indices[2] >= m_vertexCount) {
            Release();
            m_bHasBuffers = 0;
            return 0;
        }
        int off = index * 3;
        idx[off + 0] = indices[0];
        idx[off + 1] = indices[1];
        idx[off + 2] = indices[2];
        return 1;
    }

    if (m_primitiveType == 4) {
        // Validate all 4 indices < vertexCount
        if (indices[0] >= m_vertexCount || indices[1] >= m_vertexCount ||
            indices[2] >= m_vertexCount || indices[3] >= m_vertexCount) {
            Release();
            m_bHasBuffers = 0;
            return 0;
        }
        int off = index * 4;
        idx[off + 0] = indices[0];
        idx[off + 1] = indices[1];
        idx[off + 2] = indices[2];
        idx[off + 3] = indices[3];
        return 1;
    }

    Release();
    m_bHasBuffers = 0;
    return 0;
}

// ============================================================================
// CMarniViewport2::Lock [vtable 6] — 0x004271e0
// ============================================================================
int CMarniViewport2::Lock(void** outVtx, void** outIdx)
{
    // 0x004271e0 — returns buffer pointers, sets lock flag
    if (m_bHasBuffers == 0) {
        return 0;
    }
    if (m_locked != 0) {
        return 0;
    }
    if (outVtx != 0) {
        *outVtx = m_pVertexBuffer;
    }
    if (outIdx != 0) {
        *outIdx = m_pIndexBuffer;
    }
    m_locked = 1;
    return 1;
}

// ============================================================================
// CMarniViewport2::Unlock [vtable 7] — 0x00427250
// ============================================================================
int CMarniViewport2::Unlock()
{
    // 0x00427250 — clears lock flag
    if (m_locked == 0) {
        return 1;
    }
    m_locked = 0;
    return 1;
}

// ============================================================================
// CMarniViewport2::CopyFrom — 0x00426600
// Copies vertex/index data from source viewport.
// Three code paths:
//   1. Dest not initialized (m_flag10==0): shallow copy (share pointers)
//   2. Same render mode: direct copy via Lock/GetVertex/SetVertex
//   3. Different render mode: convert strip<->flat
// ============================================================================
int CMarniViewport2::CopyFrom(CMarniViewport2* src)
{
    // 0x00426600 — use DWORD-indexed access to match Ghidra field layout exactly
    DWORD* pdst = (DWORD*)this;
    DWORD* psrc = (DWORD*)src;

    // Branch 1: Dest not yet initialized (m_flag10 == 0)
    if (pdst[4] == 0) {
        if (psrc[6] != 0) {  // src has buffers (m_flag18)
            // Shallow copy: share pointers, copy counts
            pdst[8]  = psrc[8];   // m_unknown20
            pdst[1]  = psrc[1];   // m_pVertexBuffer (shared)
            pdst[2]  = psrc[2];   // m_pIndexBuffer (shared)
            pdst[3]  = 1;         // m_bHasBuffers = 1
            pdst[6]  = 1;         // m_flag18 = 1
            pdst[5]  = 0;         // m_locked = 0
            pdst[4]  = 0;         // m_flag10 = 0
            pdst[9]  = psrc[9];   // m_vertexCount
            pdst[12] = psrc[12];  // m_vertexCapacity
            pdst[16] = psrc[16];  // m_padding40 (extended field)
            pdst[13] = psrc[13];  // m_listCapacity
            pdst[11] = psrc[11];  // m_primitiveType
            pdst[7]  = psrc[7];   // m_unknown1C (render style)
            return 1;
        }
        return 0;
    }

    // Branch 2: Both have buffers
    if (pdst[3] == 0 || psrc[3] == 0) {
        return 0;
    }

    // Branch 2a: Same render style — direct copy
    if (pdst[7] == psrc[7]) {
        // Reallocate if capacity insufficient or type mismatch
        if ((int)pdst[11] != (int)psrc[11] ||
            (int)pdst[12] < (int)psrc[9] ||
            (int)pdst[13] < (int)psrc[10])
        {
            Release();
            if (!CreateWork(psrc[9], psrc[10], psrc[11])) {
                return 0;
            }
        }

        void* dstVtx, *dstIdx;
        void* srcVtx, *srcIdx;
        if (!Lock(&dstVtx, &dstIdx) || !src->Lock(&srcVtx, &srcIdx)) {
            return 0;
        }

        // Copy vertices (11 DWORDs each)
        for (DWORD i = 0; i < psrc[9]; i++) {
            DWORD vtxData[11];
            src->GetVertex(i, vtxData);
            SetVertex(i, vtxData);
        }

        // Copy index lists
        for (DWORD i = 0; i < psrc[10]; i++) {
            WORD idxData[4] = {0, 0, 0, 0};
            src->GetList(i, idxData);
            SetList(i, idxData);
        }

        Unlock();
        src->Unlock();
        pdst[9]  = psrc[9];   // m_vertexCount
        pdst[10] = psrc[10];  // m_listCount
        pdst[8]  = psrc[8];   // m_unknown20
        pdst[16] = psrc[16];  // m_padding40
    }
    // Branch 2b: Convert strip -> flat (dest flat=0, src strip=1)
    else if (pdst[7] == 0 && psrc[7] == 1) {
        int neededVerts = psrc[11] * psrc[10];  // polyType * listCount
        if ((int)pdst[11] != (int)psrc[11] ||
            (int)pdst[12] < neededVerts ||
            (int)pdst[13] < (int)psrc[10])
        {
            Release();
            if (!CreateWork(neededVerts, psrc[10], psrc[11])) {
                return 0;
            }
        }

        pdst[9]  = neededVerts;   // m_vertexCount
        pdst[10] = psrc[10];      // m_listCount
        pdst[7]  = 0;             // render mode: flat

        void* dstVtx, *dstIdx;
        void* srcVtx, *srcIdx;
        if (!Lock(&dstVtx, &dstIdx) || !src->Lock(&srcVtx, &srcIdx)) {
            return 0;
        }

        int vertIdx = 0;
        for (DWORD poly = 0; poly < psrc[10]; poly++) {
            WORD idx[4] = {0, 0, 0, 0};
            src->GetList(poly, idx);

            if (psrc[11] == 3) {
                for (int v = 0; v < 3; v++) {
                    DWORD vtx[11];
                    src->GetVertex(idx[v], vtx);
                    SetVertex(vertIdx + v, vtx);
                }
                WORD flatIdx[4] = {0, 0, 0, 0};
                flatIdx[0] = (WORD)vertIdx;
                flatIdx[1] = (WORD)(vertIdx + 1);
                flatIdx[2] = (WORD)(vertIdx + 2);
                SetList(poly, flatIdx);
                vertIdx += 3;
            }
            else if (psrc[11] == 4) {
                for (int v = 0; v < 4; v++) {
                    DWORD vtx[11];
                    src->GetVertex(idx[v], vtx);
                    SetVertex(vertIdx + v, vtx);
                }
                WORD flatIdx[4] = {0, 0, 0, 0};
                flatIdx[0] = (WORD)vertIdx;
                flatIdx[1] = (WORD)(vertIdx + 1);
                flatIdx[2] = (WORD)(vertIdx + 2);
                flatIdx[3] = (WORD)(vertIdx + 3);
                SetList(poly, flatIdx);
                vertIdx += 4;
            }
        }

        Unlock();
        src->Unlock();
        pdst[8] = 0;  // m_unknown20 = 0
    }
    // Branch 2c: Convert flat -> strip (dest strip=1, src flat=0)
    else {
        // pdst[7]==1, psrc[7]==0 — convert flat to strip
        if ((int)pdst[11] != (int)psrc[11] ||
            (int)pdst[12] < (int)psrc[9] ||
            (int)pdst[13] < (int)psrc[10])
        {
            Release();
            if (!CreateWork(psrc[9], psrc[10], psrc[11])) {
                return 0;
            }
        }

        pdst[9]  = psrc[9];
        pdst[10] = psrc[10];
        pdst[7]  = 1;  // strip mode

        void* dstVtx, *dstIdx;
        void* srcVtx, *srcIdx;
        if (!Lock(&dstVtx, &dstIdx) || !src->Lock(&srcVtx, &srcIdx)) {
            return 0;
        }

        // Direct copy vertices and indices (flat→strip just changes mode flag)
        for (DWORD i = 0; i < psrc[9]; i++) {
            DWORD vtx[11];
            src->GetVertex(i, vtx);
            SetVertex(i, vtx);
        }
        for (DWORD i = 0; i < psrc[10]; i++) {
            WORD idx[4] = {0, 0, 0, 0};
            src->GetList(i, idx);
            SetList(i, idx);
        }

        Unlock();
        src->Unlock();
    }

    return 1;
}

// ============================================================================
// CMarniViewport2::Convert0 — 0x004262e0
// Converts strip indices to flat triangle lists by creating a temp viewport,
// copying the source data, then expanding geometry.
// ============================================================================
int CMarniViewport2::Convert0(CMarniViewport2* src)
{
    // 0x004262e0 — this is called on 'this' (the output), src is the input
    if (src == 0 || src->m_bHasBuffers == 0) {
        return 0;
    }

    // Create a temporary viewport to hold the source data
    CMarniViewport2 temp;
    temp.CreateWork(src->m_vertexCount, src->m_listCount, src->m_primitiveType);

    // Copy source data into temp
    if (!temp.CopyFrom(src)) {
        return 0;
    }

    // Release destination and reallocate with expanded capacity for flat triangles
    int polyType = src->m_primitiveType;
    int expandedVertCount = src->m_listCount * polyType;   // Ghidra: local_34 * local_30
    int newListCount = src->m_listCount;                    // Ghidra: local_34

    Release();
    if (!CreateWork(expandedVertCount, newListCount, polyType)) {
        return 0;
    }

    // Lock destination and source (temp)
    void* dstVtx, *dstIdx;
    Lock(&dstVtx, &dstIdx);

    void* tmpVtx, *tmpIdx;
    temp.Lock(&tmpVtx, &tmpIdx);

    m_unknown1C = 0;  // Set flat mode

    int vertIdx = 0;
    for (int poly = 0; poly < src->m_listCount; poly++) {
        WORD idx[4] = {0, 0, 0, 0};
        temp.GetList(poly, idx);

        if (polyType == 3) {
            // Read 3 vertices from temp, write 3 vertices to dest consecutively
            for (int v = 0; v < 3; v++) {
                DWORD vtxData[11];
                temp.GetVertex(idx[v], vtxData);
                SetVertex(vertIdx + v, vtxData);
            }
            // Write flat triangle indices: one SetList with 3 consecutive indices
            WORD triIdx[4] = {0, 0, 0, 0};
            triIdx[0] = (WORD)vertIdx;
            triIdx[1] = (WORD)(vertIdx + 1);
            triIdx[2] = (WORD)(vertIdx + 2);
            SetList(poly, triIdx);
            vertIdx += 3;
        }
        else if (polyType == 4) {
            // Read 4 vertices, write 4 consecutively
            for (int v = 0; v < 4; v++) {
                DWORD vtxData[11];
                temp.GetVertex(idx[v], vtxData);
                SetVertex(vertIdx + v, vtxData);
            }
            // Write flat quad indices: one SetList with 4 consecutive indices
            WORD quadIdx[4] = {0, 0, 0, 0};
            quadIdx[0] = (WORD)vertIdx;
            quadIdx[1] = (WORD)(vertIdx + 1);
            quadIdx[2] = (WORD)(vertIdx + 2);
            quadIdx[3] = (WORD)(vertIdx + 3);
            SetList(poly, quadIdx);
            vertIdx += 4;
        }
    }

    Unlock();
    temp.Unlock();

    m_unknown20 = 0;  // Reset conversion state
    return 1;
}

// ============================================================================
// TriangleDivideMarniPolyhedra — 0x00425c10
// Subdivides triangle polyhedra by inserting edge midpoints.
// Each triangle is subdivided into 6 smaller triangles.
// ============================================================================
int TriangleDivideMarniPolyhedra(CMarniViewport2* polyArray, int count)
{
    // 0x00425c10 — takes array of CMarniViewport2 + count
    // Validates primitive type == 3 (triangles only) and count < 9

    if (polyArray == 0 || count == 0) {
        return 0;
    }

    if (polyArray->m_primitiveType != 3) {
        return 0;
    }

    if (count >= 9) {
        return 0;
    }

    // Create a temporary viewport and copy source data
    CMarniViewport2 tempWork;
    tempWork.CreateWork(polyArray->m_vertexCount, polyArray->m_listCount, 3);

    if (!tempWork.CopyFrom(polyArray)) {
        return 0;
    }

    // Convert to flat triangles
    tempWork.Convert0(&tempWork);

    // Save original counts before releasing
    int origListCount = polyArray->m_listCount;

    // Release source and reallocate with doubled capacity
    // Each source triangle produces 6 subdivided triangles (2 index lists of 3 each)
    int newVtxCount = origListCount * 6;
    int newListCount = origListCount * 2;

    polyArray->Release();
    if (!polyArray->CreateWork(newVtxCount, newListCount, 3)) {
        return 0;
    }

    polyArray->Lock(0, 0);

    void* tmpVtx, *tmpIdx;
    tempWork.Lock(&tmpVtx, &tmpIdx);

    int outputVertIdx = 0;
    int outputListIdx = 0;
    DWORD baseIndex = 0;

    for (int poly = 0; poly < tempWork.m_listCount; poly++) {
        WORD idx[4] = {0, 0, 0, 0};
        tempWork.GetList(poly, idx);

        // Read 3 triangle vertices (11 DWORDs each)
        DWORD vtx0[11], vtx1[11], vtx2[11];
        tempWork.GetVertex(idx[0], vtx0);
        tempWork.GetVertex(idx[1], vtx1);
        tempWork.GetVertex(idx[2], vtx2);

        // Copy original vertices to local arrays for reference
        DWORD vtxA[11], vtxB[11], vtxC[11];
        for (int i = 0; i < 11; i++) {
            vtxA[i] = vtx0[i];  // v0
            vtxB[i] = vtx1[i];  // v1
            vtxC[i] = vtx2[i];  // v2
        }

        // Compute edge midpoints as float values
        // Half constant: 0x3f000000 = 0.5f (from _DAT_004af0f4)
        float half = 0.5f;
        float* v0f = (float*)vtxA;
        float* v1f = (float*)vtxB;
        float* v2f = (float*)vtxC;

        DWORD mid01[11], mid12[11], mid02[11];

        for (int i = 0; i < 11; i++) {
            // Copy non-position fields from source vertex
            if (i >= 3) {
                mid01[i] = vtxA[i];
                mid12[i] = vtxB[i];
                mid02[i] = vtxA[i];
            }
        }

        // Compute midpoint positions (fields 0,1,2 are position x,y,z as floats)
        float* m01f = (float*)mid01;
        float* m12f = (float*)mid12;
        float* m02f = (float*)mid02;
        m01f[0] = (v0f[0] + v1f[0]) * half;
        m01f[1] = (v0f[1] + v1f[1]) * half;
        m01f[2] = (v0f[2] + v1f[2]) * half;
        m12f[0] = (v1f[0] + v2f[0]) * half;
        m12f[1] = (v1f[1] + v2f[1]) * half;
        m12f[2] = (v1f[2] + v2f[2]) * half;
        m02f[0] = (v0f[0] + v2f[0]) * half;
        m02f[1] = (v0f[1] + v2f[1]) * half;
        m02f[2] = (v0f[2] + v2f[2]) * half;

        // Determine which edge is longest (squared distance comparison)
        float dx01 = v0f[0] - v1f[0];
        float dy01 = v0f[1] - v1f[1];
        float dz01 = v0f[2] - v1f[2];
        float dist01 = dx01 * dx01 + dy01 * dy01 + dz01 * dz01;

        float dx12 = v1f[0] - v2f[0];
        float dy12 = v1f[1] - v2f[1];
        float dz12 = v1f[2] - v2f[2];
        float dist12 = dx12 * dx12 + dy12 * dy12 + dz12 * dz12;

        float dx02 = v0f[0] - v2f[0];
        float dy02 = v0f[1] - v2f[1];
        float dz02 = v0f[2] - v2f[2];
        float dist02 = dx02 * dx02 + dy02 * dy02 + dz02 * dz02;

        // Write 6 subdivided vertices: order depends on longest edge
        if (dist01 >= dist12 && dist01 >= dist02) {
            // v0→v1 is longest: order v0, m01, v2, v1, v2, m01
            polyArray->SetVertex(outputVertIdx + 0, vtxA);
            polyArray->SetVertex(outputVertIdx + 1, mid01);
            polyArray->SetVertex(outputVertIdx + 2, vtxC);
            polyArray->SetVertex(outputVertIdx + 3, vtxB);
            polyArray->SetVertex(outputVertIdx + 4, vtxC);
            polyArray->SetVertex(outputVertIdx + 5, mid01);
        }
        else if (dist12 >= dist01 && dist12 >= dist02) {
            // v1→v2 is longest: order v0, v1, m12, m01, v2, m12
            polyArray->SetVertex(outputVertIdx + 0, vtxA);
            polyArray->SetVertex(outputVertIdx + 1, vtxB);
            polyArray->SetVertex(outputVertIdx + 2, mid12);
            polyArray->SetVertex(outputVertIdx + 3, mid01);
            polyArray->SetVertex(outputVertIdx + 4, vtxC);
            polyArray->SetVertex(outputVertIdx + 5, mid12);
        }
        else {
            // v0→v2 is longest: order v0, m01, v2, v1, v2, m01
            // Actually from Ghidra: v0, m02, v1, m12, v2, m02
            polyArray->SetVertex(outputVertIdx + 0, vtxA);
            polyArray->SetVertex(outputVertIdx + 1, mid02);
            polyArray->SetVertex(outputVertIdx + 2, vtxB);
            polyArray->SetVertex(outputVertIdx + 3, mid12);
            polyArray->SetVertex(outputVertIdx + 4, vtxC);
            polyArray->SetVertex(outputVertIdx + 5, mid02);
        }

        // Write 2 triangle index lists
        // First triangle: local indices 0, 1, 2
        WORD tri1[4] = {0, 0, 0, 0};
        tri1[0] = (WORD)(baseIndex + 0);
        tri1[1] = (WORD)(baseIndex + 1);
        tri1[2] = (WORD)(baseIndex + 2);
        polyArray->SetList(outputListIdx, tri1);

        // Second triangle: local indices 3, 4, 5
        WORD tri2[4] = {0, 0, 0, 0};
        tri2[0] = (WORD)(baseIndex + 3);
        tri2[1] = (WORD)(baseIndex + 4);
        tri2[2] = (WORD)(baseIndex + 5);
        polyArray->SetList(outputListIdx + 1, tri2);

        outputVertIdx += 6;
        outputListIdx += 2;
        baseIndex += 6;
    }

    polyArray->Unlock();
    tempWork.Unlock();

    return 1;
}

// ============================================================================
// MarniSystem PSXObject — TMD model parser
// ============================================================================
// Layout of a Direct3DTMD slot (0x1594 bytes):
//   +0x000  16 embedded CMarniViewport2 elements, stride 0x4C (19 DWORDs)
//   +0x4C0  m_objectCount   (param_1[0x130] in Ghidra)
//   +0x4C4  m_flag4C4       (param_1[0x131])
//   +0x4C8  m_unknown4C8    (resize threshold, constructor sets 0x400)
// Embedded element extension fields (beyond CMarniViewport2's 0x44 bytes):
//   elem[0x0E] (+0x38) = tpage material key lo   ((tpage & 0x3F) << 4)
//   elem[0x0F] (+0x3C) = tpage material key hi   (tpage >> 6)
//   elem[0x10] (+0x40) = clut material key lo
//   elem[0x11] (+0x44) = clut material key hi
//   elem[0x12] (+0x48) = has-texture flag
// The material keys at +0x38/+0x3C are matched against texture-page material
// entries by CreateTmdObjectInternal (FUN_00483910).
// ============================================================================

// Kind-table entry (5 DWORDs), built by PSXObject_EnumKind (FUN_00444f60):
//   [0] = primitive count in this kind
//   [1] = tpage (low 16 bits)
//   [2] = full flags of the first packet seen
//   [3] = vertex mode: 1 = triangle (3 verts/poly), 0 = quad (4 verts/poly)
//   [4] = clut  (low 16 bits)

// 11-float vertex written via CMarniViewport2::SetVertex
struct PSXObjVtx {
    float x, y, z;      // [0..2]  position (Y negated on load)
    float nx, ny, nz;   // [3..5]  normal (fixed-point /4096, NY negated)
    float r, g, b;      // [6..8]  color
    float u, v;         // [9..10] texture UV
};

// Lazy-initialize the 16 embedded elements of a raw BSS slot.
// The original did this in the Direct3DTMD constructor (0x00415910 ->
// FUN_00446cb0); our slots live in g_tmdObjectBuffer and are never
// C++-constructed, so do it on first use.
static void PSXObject_InitSlotElements(BYTE* slot)
{
    // The constructor also seeds the trailer, and m_unknown4C8 (0x4C8) is the
    // vertex-count threshold PSXObject_Resize subdivides an element at. Left at
    // zero it matches every element, so Resize kept splitting until the object
    // count passed 16 and Store failed with "too many objects" - after having
    // written a 17th element straight over this trailer.
    if (*(DWORD*)(slot + 0x4C8) == 0) {
        *(DWORD*)(slot + 0x4C8) = 0x400;
    }

    for (int i = 0; i < 16; i++) {
        DWORD* elem = (DWORD*)(slot + i * 0x4C);
        if (elem[0] == 0) {
            elem[0]  = (DWORD)g_CMarniViewport2VTable;  // 0x004af0f8
            elem[1]  = 0;   // m_pVertexBuffer
            elem[2]  = 0;   // m_pIndexBuffer
            elem[3]  = 0;   // m_bHasBuffers
            elem[4]  = 0;   // m_flag10
            elem[5]  = 0;   // m_locked
            elem[6]  = 0;   // m_flag18
            elem[7]  = 1;   // m_unknown1C (base init marker)
            elem[8]  = 0;   // m_unknown20
            elem[9]  = 0;   // m_vertexCount
            elem[10] = 0;   // m_listCount
            elem[11] = 0;   // m_primitiveType
            elem[12] = 0;   // m_vertexCapacity
            elem[13] = 0;   // m_listCapacity
            elem[14] = 0;   // +0x38
            elem[15] = 0;   // +0x3C
            elem[16] = 0;   // +0x40
            elem[17] = 0;   // +0x44
            elem[18] = 0;   // +0x48
        }
    }
}

// FUN_004450a0 (0x004450a0) - release all embedded elements, clear ext fields
static int PSXObject_CleanupElements(BYTE* slot)
{
    for (int i = 0; i < 16; i++) {
        CMarniViewport2* elem = (CMarniViewport2*)(slot + i * 0x4C);
        elem->Release();    // original: call vtable[0]
        DWORD* raw = (DWORD*)elem;
        raw[0x11] = 0;
        raw[0x10] = 0;
        raw[0x0F] = 0;
        raw[0x0E] = 0;
    }
    *(DWORD*)(slot + 0x4C0) = 0;
    *(DWORD*)(slot + 0x4C4) = 0;
    return 1;
}

// FUN_00444e10 (0x00444e10) - initialize a new kind-table entry
static int PSXObject_KindDataSet(int* entry, unsigned int flags, short tpage, short clut)
{
    *(short*)(entry + 1) = tpage;
    entry[2] = (int)flags;
    flags &= 0x3DFFFFFF;
    *(short*)(entry + 4) = clut;
    entry[0] = 1;

    // Triangle kinds (3 vertices per poly) -> entry[3] = 1
    switch (flags) {
    case 0x20040506: case 0x20000304:
    case 0x24000507: case 0x21010304:
    case 0x25010607:
    case 0x30040606: case 0x30000406:
    case 0x34000609: case 0x31010506:
    case 0x35010809:
        entry[3] = 1;
        return 1;
    // Quad kinds (4 vertices per poly) -> entry[3] = 0
    case 0x28000405:
    case 0x29010305: case 0x28040708:
    case 0x2D010709: case 0x2C000709:
    case 0x38000508:
    case 0x39010608: case 0x38040808:
    case 0x3C00080C: case 0x3D010A0C:
        entry[3] = 0;
        return 1;
    default:
        PSXObjDebugPrint("MarniSystem PSXObject kinddataset: unknown kind %x\n", flags);
        return 0;
    }
}

// FUN_00444db0 (0x00444db0) - advance packet cursor to next packet of `kind`
static unsigned int* PSXObject_FindPacket(unsigned int* pkt, int* kind)
{
    while ((((*pkt ^ (unsigned int)kind[2]) & 0x3DFFFFFF) != 0) ||
           (((unsigned int)kind[2] & 0x4000000) != 0 &&
            (((short)(pkt[1] >> 0x10) != *(short*)(kind + 1)) ||
             (((unsigned short)(pkt[2] >> 0x10) & 0x1F) != *(unsigned short*)(kind + 4))))) {
        pkt = (unsigned int*)((int)pkt + ((*pkt & 0xFF00) >> 6) + 4);
    }
    return pkt;
}

// FUN_00444f60 (0x00444f60) - PSXObject::EnumKind
// Groups the TMD primitive packets into distinct kinds (flags/tpage/clut).
// Returns the kind count, or 0 on error.
static int PSXObject_EnumKind(int maxKinds, int* table, unsigned int* pkt, int numPackets)
{
    memset(table, 0, maxKinds * 0x14);

    int kindCount = 0;
    int processed = 0;
    if (numPackets > 0) {
        int* newEntry = table;
        do {
            int i = 0;
            if (kindCount > 0) {
                unsigned int* kp = (unsigned int*)(table + 2);
                do {
                    if (*kp == *pkt) {
                        if ((*pkt & 0x4000000) != 0) {
                            if ((pkt[2] & 0x1800000) != 0x800000) {
                                PSXObjDebugPrint("MarniSystem PSXObject::EnumKind: clut range error\n");
                                return 0;
                            }
                            if (((short)(pkt[1] >> 0x10) != (short)kp[-1]) ||
                                (((unsigned short)(pkt[2] >> 0x10) & 0x1F) != (unsigned short)kp[2])) {
                                goto noMatch;
                            }
                        }
                        table[i * 5] = table[i * 5] + 1;
                        break;
                    }
                noMatch:
                    kp = kp + 5;
                    i = i + 1;
                } while (i < kindCount);
            }
            if (i == kindCount) {
                kindCount = kindCount + 1;
                PSXObject_KindDataSet(newEntry, *pkt,
                                      (short)(pkt[1] >> 0x10),
                                      (short)((pkt[2] >> 0x10) & 0x1F));
                newEntry = newEntry + 5;
            }
            if (maxKinds < kindCount) {
                PSXObjDebugPrint("MarniSystem PSXObject::EnumKind: too many kinds\n");
                return 0;
            }
            processed = processed + 1;
            pkt = (unsigned int*)((int)pkt + ((*pkt & 0xFF00) >> 6) + 4);
        } while (processed < numPackets);
    }
    return kindCount;
}

// FUN_00444790 (0x00444790) - DivideMarniPolyhedra
// Splits src's geometry in two: src keeps floor(count/2) polys,
// dst receives ceil(count/2) polys. Verified against the original asm:
// the vertex write index advances by 3 per poly even for quads.
static int DivideMarniPolyhedraElem(CMarniViewport2* src, CMarniViewport2* dst)
{
    CMarniViewport2 temp;                                   // 0x004272e0
    temp.CreateWork(src->m_vertexCount, src->m_listCount, src->m_primitiveType);  // 0x00427100
    temp.CopyFrom(src);                                     // 0x00426600

    dst->Release();                                         // vtable[0]
    src->Release();                                         // vtable[0]

    int polyCount = temp.m_listCount;
    int primType  = temp.m_primitiveType;
    int half      = polyCount / 2;
    int second    = half + (polyCount & 1);

    src->CreateWork(primType * half, half, primType);       // vtable[1]
    if (src->m_bHasBuffers == 0) {
        PSXObjDebugPrint("DivideMarniPolyhedra: src CreateWork failed\n");
        return 0;
    }
    dst->CreateWork(primType * second, second, primType);   // vtable[1]
    if (dst->m_bHasBuffers == 0) {
        PSXObjDebugPrint("DivideMarniPolyhedra: dst CreateWork failed\n");
        return 0;
    }

    temp.Lock(0, 0);                                        // 0x004271e0
    src->Lock(0, 0);                                        // vtable[6]

    DWORD vtxBuf[11];
    WORD  listBuf[4];
    int vi = 0;
    for (int p = 0; p < half; p++) {
        WORD seq[4] = { (WORD)(primType * p), (WORD)(primType * p + 1),
                        (WORD)(primType * p + 2), (WORD)(primType * p + 3) };
        src->SetList(p, seq);                               // vtable[5]
        temp.GetList(p, listBuf);                           // 0x00426e80
        if (primType == 3) {
            temp.GetVertex(listBuf[0], vtxBuf); src->SetVertex(vi,     vtxBuf);
            temp.GetVertex(listBuf[1], vtxBuf); src->SetVertex(vi + 1, vtxBuf);
            temp.GetVertex(listBuf[2], vtxBuf); src->SetVertex(vi + 2, vtxBuf);
        }
        else if (primType == 4) {
            temp.GetVertex(listBuf[0], vtxBuf); src->SetVertex(vi,     vtxBuf);
            temp.GetVertex(listBuf[1], vtxBuf); src->SetVertex(vi + 1, vtxBuf);
            temp.GetVertex(listBuf[2], vtxBuf); src->SetVertex(vi + 2, vtxBuf);
            temp.GetVertex(listBuf[3], vtxBuf); src->SetVertex(vi + 3, vtxBuf);
        }
        else {
            PSXObjDebugPrint("DivideMarniPolyhedra: bad primitive type %d\n", primType);
            return 0;
        }
        vi += 3;    // original advances by 3 even for quads (verified in asm)
    }

    src->Unlock();                                          // vtable[7]
    dst->Lock(0, 0);                                        // vtable[6]

    vi = 0;
    for (int p = 0; p < second; p++) {
        WORD seq[4] = { (WORD)(primType * p), (WORD)(primType * p + 1),
                        (WORD)(primType * p + 2), (WORD)(primType * p + 3) };
        dst->SetList(p, seq);                               // vtable[5]
        temp.GetList(half + p, listBuf);                    // 0x00426e80
        if (primType == 3) {
            temp.GetVertex(listBuf[0], vtxBuf); dst->SetVertex(vi,     vtxBuf);
            temp.GetVertex(listBuf[1], vtxBuf); dst->SetVertex(vi + 1, vtxBuf);
            temp.GetVertex(listBuf[2], vtxBuf); dst->SetVertex(vi + 2, vtxBuf);
        }
        else if (primType == 4) {
            temp.GetVertex(listBuf[0], vtxBuf); dst->SetVertex(vi,     vtxBuf);
            temp.GetVertex(listBuf[1], vtxBuf); dst->SetVertex(vi + 1, vtxBuf);
            temp.GetVertex(listBuf[2], vtxBuf); dst->SetVertex(vi + 2, vtxBuf);
            temp.GetVertex(listBuf[3], vtxBuf); dst->SetVertex(vi + 3, vtxBuf);
        }
        else {
            PSXObjDebugPrint("DivideMarniPolyhedra: bad primitive type %d\n", primType);
            return 0;
        }
        vi += 3;
    }

    dst->Unlock();                                          // vtable[7]
    temp.Unlock();                                          // 0x00427250

    src->m_unknown1C = temp.m_unknown1C;
    dst->m_unknown1C = temp.m_unknown1C;
    return 1;
}

// FUN_00444ca0 (0x00444ca0) - PSXObject::Resize
// Subdivides any embedded element whose vertex count reached the +0x4C8
// threshold (constructor value 0x400) into a new element at the end.
static int PSXObject_Resize(BYTE* slot)
{
    if (*(int*)(slot + 0x4C4) == 0) {
        PSXObjDebugPrint("PSXObject::Resize: not stored\n");
        return 0;
    }
    int i = 0;
    if (*(int*)(slot + 0x4C0) > 0) {
        do {
            BYTE* elem = slot + i * 0x4C;
            if (*(int*)(slot + 0x4C8) <= *(int*)(slot + 0x24 + i * 0x4C)) {
                int count = *(int*)(slot + 0x4C0);
                // Faithful to the original: it allows count == 16 here, which
                // then places the new element at slot+0x4C0 - on top of this
                // trailer. Unreachable while the threshold above is 0x400,
                // since no single kind carries 1024 vertices.
                if (count > 0x10) {
                    PSXObjDebugPrint("PSXObject::Resize: too many objects\n");
                    return 0;
                }
                BYTE* newElem = slot + count * 0x4C;
                DivideMarniPolyhedraElem((CMarniViewport2*)elem, (CMarniViewport2*)newElem);
                *(DWORD*)(newElem + 0x38) = *(DWORD*)(elem + 0x38);
                *(DWORD*)(newElem + 0x3C) = *(DWORD*)(elem + 0x3C);
                *(DWORD*)(newElem + 0x40) = *(DWORD*)(elem + 0x40);
                *(DWORD*)(newElem + 0x44) = *(DWORD*)(elem + 0x44);
                *(DWORD*)(newElem + 0x48) = *(DWORD*)(elem + 0x48);
                i = -1;     // restart the scan
                *(int*)(slot + 0x4C0) = count + 1;
            }
            i = i + 1;
        } while (i < *(int*)(slot + 0x4C0));
    }
    return 1;
}

// TMD vertex/normal readers (8-byte short4 entries)
static void PSXObjReadVertex(PSXObjVtx* v, int vertBase, unsigned int idx)
{
    short* p = (short*)(vertBase + idx * 8);
    v->x = (float)(int)p[0];
    v->y = -(float)(int)p[1];
    v->z = (float)(int)p[2];
}

// 0x25010607 reads positions without negating Y
static void PSXObjReadVertexRawY(PSXObjVtx* v, int vertBase, unsigned int idx)
{
    short* p = (short*)(vertBase + idx * 8);
    v->x = (float)(int)p[0];
    v->y = (float)(int)p[1];
    v->z = (float)(int)p[2];
}

static void PSXObjReadNormal(PSXObjVtx* v, int normBase, unsigned int idx)
{
    short* p = (short*)(normBase + idx * 8);
    v->nx = (float)(int)p[0] * 0.00024414063f;
    v->ny = (float)(int)p[1] * -0.00024414063f;
    v->nz = (float)(int)p[2] * 0.00024414063f;
}

// FUN_004450e0 (0x004450e0) - MarniSystem PSXObject::Store
// Parses a PSX TMD model into the Direct3DTMD slot.
//   tmdHdr      - patched TMD header (magic 0x41, [1]=1 absolute mode, [2]=1 count)
//   objIndex    - object index (original passes 0)
//   bankOrTpage - selected texture page (-1 = auto-detect); original passes the depth
//   texRef      - UV divisor (texture page width, from texBank + 0x2C)
int PSXObject_Store(CMarniDirect3DTMD* self, int* tmdHdr, int objIndex,
                    int bankOrTpage, int texRef)
{
    BYTE* slot = (BYTE*)self;

    PSXObject_InitSlotElements(slot);
    PSXObject_CleanupElements(slot);                        // FUN_004450a0

    if (tmdHdr[0] != 0x41) {
        PSXObjDebugPrint("a difference of header\nMarniSystem PSXObject::Store\n");
        return 0;
    }
    if (tmdHdr[2] <= objIndex) {
        PSXObjDebugPrint("the object that you are specified is wrong. %d, %d\n",
                         tmdHdr[2], objIndex);
        return 0;
    }

    int* objEntry = tmdHdr + objIndex * 7 + 3;
    int primPtr, vertBase, normBase;
    if ((*(BYTE*)(tmdHdr + 1) & 1) == 0) {
        // Offset mode: table entries hold header-relative offsets
        primPtr  = (int)(((unsigned)(tmdHdr[objIndex * 7 + 7] + 0xC) & 0xFFFFFFFC) + (int)tmdHdr);
        vertBase = (int)(((unsigned)(*objEntry + 0xC) & 0xFFFFFFFC) + (int)tmdHdr);
        normBase = (int)(((unsigned)(objEntry[2] + 0xC) & 0xFFFFFFFC) + (int)tmdHdr);
    }
    else {
        // Absolute mode: table entries hold direct pointers
        primPtr  = objEntry[4];
        vertBase = *objEntry;
        normBase = objEntry[2];
    }

    // Enumerate distinct primitive kinds (max 100 entries of 5 DWORDs)
    int kindTable[100 * 5];
    int kindCount = PSXObject_EnumKind(100, kindTable, (unsigned int*)primPtr, objEntry[5]);
    if (kindCount >= 0x11 || kindCount == 0) {
        PSXObjDebugPrint("MarniSystem PSXObject::Store: kind enumeration failed\n");
        return 0;
    }

    // Select the texture page (CLUT) to bind
    int selectedPage;
    if (bankOrTpage == -1) {
        unsigned int maxClut = 0;
        unsigned int minClut = 4000;
        if (kindCount > 0) {
            unsigned short* cp = (unsigned short*)(kindTable + 4);
            for (int i = kindCount; i != 0; i--) {
                if ((*(unsigned int*)(cp - 4) & 0x4000000) != 0) {
                    unsigned int c = *cp;
                    if (maxClut < c) maxClut = c;
                    if (c < minClut) minClut = c;
                }
                cp = cp + 10;
            }
        }
        if (maxClut == 0 && minClut == 4000) {
            minClut = 0;
        }
        if ((int)(maxClut - minClut) > 1) {
            PSXObjDebugPrint("MarniSystem PSXObject::Store: clut range %d, %d\n",
                             maxClut, minClut);
            return 0;
        }
        selectedPage = (int)minClut;
    }
    else {
        selectedPage = bankOrTpage;
    }

    float texW = (float)texRef;
    int processed = 0;

    *(DWORD*)(slot + 0x4C0) = (DWORD)kindCount;            // m_objectCount

    if (kindCount > 0) {
        int* kind = kindTable;
        CMarniViewport2* elem = (CMarniViewport2*)slot;

        while (true) {
            int pktCursor = primPtr;
            int count = *kind;
            int polyType, vtxTotal;
            if (kind[3] == 0) {
                polyType = 4;
                vtxTotal = count * 4;
            }
            else {
                polyType = 3;
                vtxTotal = count * 3;
            }

            // Material keys from the kind's tpage / clut
            unsigned int tpage = (unsigned short)*(short*)(kind + 1);
            ((DWORD*)elem)[0x0E] = (tpage & 0x3F) << 4;
            ((DWORD*)elem)[0x0F] = tpage >> 6;
            unsigned int clut = (unsigned short)*(short*)(kind + 4);
            if (clut < 0x10) {
                ((DWORD*)elem)[0x10] = clut << 6;
                ((DWORD*)elem)[0x11] = 0;
            }
            else {
                ((DWORD*)elem)[0x10] = (clut - 0x10) * 0x40;
                ((DWORD*)elem)[0x11] = 0x100;
            }

            if (!elem->CreateWork(vtxTotal, count, polyType)) {     // vtable[1]
                PSXObjDebugPrint("MarniSystem PSXObject::Store: CreateWork failed\n");
                return 0;
            }
            if (!elem->Lock(0, 0)) {                                // vtable[6]
                PSXObjDebugPrint("MarniSystem PSXObject::Store: Lock failed\n");
                return 0;
            }

            int primIdx = 0;
            int vertIdx = 0;    // SetVertex write index
            int listBase = 0;   // SetList index base
            if (count > 0) {
                do {
                    unsigned int* pkt = PSXObject_FindPacket((unsigned int*)pktCursor, kind);
                    unsigned int type = (unsigned int)kind[2] & 0x3DFFFFFF;

                    PSXObjVtx v0, v1, v2, v3;
                    WORD idx[4];

                    switch (type) {
                    case 0x20000304:    // flat triangle, single normal, untextured
                        PSXObjReadVertex(&v0, vertBase, pkt[2] >> 0x10);
                        PSXObjReadVertex(&v1, vertBase, pkt[3] & 0xFFFF);
                        PSXObjReadVertex(&v2, vertBase, pkt[3] >> 0x10);
                        PSXObjReadNormal(&v0, normBase, pkt[2] & 0xFFFF);
                        v1.nx = v1.ny = v1.nz = 0.0f;
                        v2.nx = v2.ny = v2.nz = 0.0f;
                        v0.r = v0.g = v0.b = 1.0f;
                        v1.r = v1.g = v1.b = 1.0f;
                        v2.r = v2.g = v2.b = 1.0f;
                        v0.u = v0.v = 0.0f; v1.u = v1.v = 0.0f; v2.u = v2.v = 0.0f;
                        idx[0] = (WORD)(listBase + 0);
                        idx[1] = (WORD)(listBase + 1);
                        idx[2] = (WORD)(listBase + 2);
                        if (!elem->SetVertex(vertIdx,     (DWORD*)&v0) ||
                            !elem->SetVertex(vertIdx + 1, (DWORD*)&v1) ||
                            !elem->SetVertex(vertIdx + 2, (DWORD*)&v2) ||
                            !elem->SetList(primIdx, idx)) {
                            goto storeFail;
                        }
                        ((DWORD*)elem)[7]    = 0;
                        ((DWORD*)elem)[0x12] = 0;
                        break;

                    case 0x24000507: {  // textured flat triangle, single normal
                        PSXObjReadVertex(&v0, vertBase, pkt[4] >> 0x10);
                        PSXObjReadVertex(&v1, vertBase, pkt[5] & 0xFFFF);
                        PSXObjReadVertex(&v2, vertBase, pkt[5] >> 0x10);
                        PSXObjReadNormal(&v0, normBase, pkt[4] & 0xFFFF);
                        v1.nx = v1.ny = v1.nz = 0.0f;
                        v2.nx = v2.ny = v2.nz = 0.0f;
                        float ubase = (*(unsigned short*)(kind + 4) == (unsigned int)selectedPage) ? 0.0f : 0.5f;
                        v0.u = (float)(pkt[1] & 0xFF) / texW + ubase;
                        v0.v = (float)*(BYTE*)((BYTE*)pkt + 5) * 0.00390625f;
                        v1.u = (float)(pkt[2] & 0xFF) / texW + ubase;
                        v1.v = (float)*(BYTE*)((BYTE*)pkt + 9) * 0.00390625f;
                        v2.u = (float)(pkt[3] & 0xFF) / texW + ubase;
                        v2.v = (float)*(BYTE*)((BYTE*)pkt + 0xD) * 0.00390625f;
                        if (v0.u == v1.u || v2.u == v1.u || v0.u == v2.u) v1.u += 0.003f;
                        if (v0.v == v1.v || v2.v == v1.v || v0.v == v2.v) v1.v += 0.003f;
                        v0.r = v0.g = v0.b = 1.0f;
                        v1.r = v1.g = v1.b = 1.0f;
                        v2.r = v2.g = v2.b = 1.0f;
                        idx[0] = (WORD)(listBase + 0);
                        idx[1] = (WORD)(listBase + 1);
                        idx[2] = (WORD)(listBase + 2);
                        if (!elem->SetVertex(vertIdx,     (DWORD*)&v0) ||
                            !elem->SetVertex(vertIdx + 1, (DWORD*)&v1) ||
                            !elem->SetVertex(vertIdx + 2, (DWORD*)&v2) ||
                            !elem->SetList(primIdx, idx)) {
                            goto storeFail;
                        }
                        ((DWORD*)elem)[7]    = 0;
                        ((DWORD*)elem)[0x12] = 1;
                        break;
                    }

                    case 0x25010607: {  // textured triangle, normal = (0,0,-1), raw Y
                        PSXObjReadVertexRawY(&v0, vertBase, pkt[5] & 0xFFFF);
                        PSXObjReadVertexRawY(&v1, vertBase, pkt[5] >> 0x10);
                        PSXObjReadVertexRawY(&v2, vertBase, pkt[6] & 0xFFFF);
                        v0.nx = 0.0f; v0.ny = 0.0f; v0.nz = -1.0f;
                        v1.nx = v1.ny = v1.nz = 0.0f;
                        v2.nx = v2.ny = v2.nz = 0.0f;
                        float ubase = (*(unsigned short*)(kind + 4) == (unsigned int)selectedPage) ? 0.0f : 0.5f;
                        v0.u = (float)(pkt[1] & 0xFF) / texW + ubase;
                        v0.v = (float)*(BYTE*)((BYTE*)pkt + 5) * 0.00390625f;
                        v1.u = (float)(pkt[2] & 0xFF) / texW + ubase;
                        v1.v = (float)*(BYTE*)((BYTE*)pkt + 9) * 0.00390625f;
                        v2.u = (float)(pkt[3] & 0xFF) / texW + ubase;
                        v2.v = (float)*(BYTE*)((BYTE*)pkt + 0xD) * 0.00390625f;
                        if (v0.u == v1.u || v2.u == v1.u || v0.u == v2.u) v1.u += 0.003f;
                        if (v0.v == v1.v || v2.v == v1.v || v0.v == v2.v) v1.v += 0.003f;
                        v0.r = v0.g = v0.b = 1.0f;
                        v1.r = v1.g = v1.b = 1.0f;
                        v2.r = v2.g = v2.b = 1.0f;
                        idx[0] = (WORD)(listBase + 0);
                        idx[1] = (WORD)(listBase + 1);
                        idx[2] = (WORD)(listBase + 2);
                        if (!elem->SetVertex(vertIdx,     (DWORD*)&v0) ||
                            !elem->SetVertex(vertIdx + 1, (DWORD*)&v1) ||
                            !elem->SetVertex(vertIdx + 2, (DWORD*)&v2) ||
                            !elem->SetList(primIdx, idx)) {
                            goto storeFail;
                        }
                        ((DWORD*)elem)[7]    = 0;
                        ((DWORD*)elem)[0x12] = 1;
                        break;
                    }

                    case 0x30000406:    // gouraud-normal triangle, flat color, untextured
                        PSXObjReadVertex(&v0, vertBase, pkt[2] >> 0x10);
                        PSXObjReadVertex(&v1, vertBase, pkt[3] >> 0x10);
                        PSXObjReadVertex(&v2, vertBase, pkt[4] >> 0x10);
                        PSXObjReadNormal(&v0, normBase, pkt[2] & 0xFFFF);
                        PSXObjReadNormal(&v1, normBase, pkt[3] & 0xFFFF);
                        PSXObjReadNormal(&v2, normBase, pkt[4] & 0xFFFF);
                        v0.r = (float)(pkt[1] & 0xFF) * 0.0009765625f;
                        v0.g = (float)*(BYTE*)((BYTE*)pkt + 5) * 0.0009765625f;
                        v0.b = (float)((pkt[1] & 0xFF0000) >> 0x10) * 0.0009765625f;
                        v1.r = v0.r; v1.g = v0.g; v1.b = v0.b;
                        v2.r = v0.r; v2.g = v0.g; v2.b = v0.b;
                        v0.u = v0.v = 0.0f; v1.u = v1.v = 0.0f; v2.u = v2.v = 0.0f;
                        idx[0] = (WORD)(listBase + 0);
                        idx[1] = (WORD)(listBase + 1);
                        idx[2] = (WORD)(listBase + 2);
                        if (!elem->SetVertex(vertIdx,     (DWORD*)&v0) ||
                            !elem->SetVertex(vertIdx + 1, (DWORD*)&v1) ||
                            !elem->SetVertex(vertIdx + 2, (DWORD*)&v2) ||
                            !elem->SetList(primIdx, idx)) {
                            goto storeFail;
                        }
                        ((DWORD*)elem)[7]    = 1;
                        ((DWORD*)elem)[0x12] = 0;
                        break;

                    case 0x34000609: {  // textured gouraud triangle
                        // Gouraud packets pack (vertexIndex << 16) | normalIndex
                        // per dword, so every vertex index comes from the HIGH
                        // half. Reading v1 from the low half used its normal
                        // index as a vertex index, displacing one corner of
                        // every triangle in the model: the winding came out
                        // random (so no cull orientation could work) and the
                        // shading with it.
                        PSXObjReadVertex(&v0, vertBase, pkt[4] >> 0x10);
                        PSXObjReadVertex(&v1, vertBase, pkt[5] >> 0x10);
                        PSXObjReadVertex(&v2, vertBase, pkt[6] >> 0x10);
                        PSXObjReadNormal(&v0, normBase, pkt[4] & 0xFFFF);
                        PSXObjReadNormal(&v1, normBase, pkt[5] & 0xFFFF);
                        PSXObjReadNormal(&v2, normBase, pkt[6] & 0xFFFF);
                        float ubase = (*(unsigned short*)(kind + 4) == (unsigned int)selectedPage) ? 0.0f : 0.5f;
                        v0.u = (float)(pkt[1] & 0xFF) / texW + ubase;
                        v0.v = (float)*(BYTE*)((BYTE*)pkt + 5) * 0.00390625f;
                        v1.u = (float)(pkt[2] & 0xFF) / texW + ubase;
                        v1.v = (float)*(BYTE*)((BYTE*)pkt + 9) * 0.00390625f;
                        v2.u = (float)(pkt[3] & 0xFF) / texW + ubase;
                        v2.v = (float)*(BYTE*)((BYTE*)pkt + 0xD) * 0.00390625f;
                        if (v0.u == v1.u || v2.u == v1.u || v0.u == v2.u) v1.u += 0.0001f;
                        if (v0.v == v1.v || v2.v == v1.v || v0.v == v2.v) v1.v += 0.0001f;
                        v0.r = v0.g = v0.b = 1.0f;
                        v1.r = v1.g = v1.b = 1.0f;
                        v2.r = v2.g = v2.b = 1.0f;
                        idx[0] = (WORD)(listBase + 0);
                        idx[1] = (WORD)(listBase + 1);
                        idx[2] = (WORD)(listBase + 2);
                        if (!elem->SetVertex(vertIdx,     (DWORD*)&v0) ||
                            !elem->SetVertex(vertIdx + 1, (DWORD*)&v1) ||
                            !elem->SetVertex(vertIdx + 2, (DWORD*)&v2) ||
                            !elem->SetList(primIdx, idx)) {
                            goto storeFail;
                        }
                        ((DWORD*)elem)[7]    = 1;
                        ((DWORD*)elem)[0x12] = 1;
                        break;
                    }

                    case 0x3C00080C: {  // textured gouraud quad
                        // Same as 0x34000609: all four vertex indices are in the
                        // high halves, alongside their normal index.
                        PSXObjReadVertex(&v0, vertBase, pkt[5] >> 0x10);
                        PSXObjReadVertex(&v1, vertBase, pkt[6] >> 0x10);
                        PSXObjReadVertex(&v2, vertBase, pkt[7] >> 0x10);
                        PSXObjReadVertex(&v3, vertBase, pkt[8] >> 0x10);
                        PSXObjReadNormal(&v0, normBase, pkt[5] & 0xFFFF);
                        PSXObjReadNormal(&v1, normBase, pkt[6] & 0xFFFF);
                        PSXObjReadNormal(&v2, normBase, pkt[7] & 0xFFFF);
                        PSXObjReadNormal(&v3, normBase, pkt[8] & 0xFFFF);
                        float ubase = (*(unsigned short*)(kind + 4) == (unsigned int)selectedPage) ? 0.0f : 0.5f;
                        v0.u = (float)(pkt[1] & 0xFF) / texW + ubase;
                        v0.v = (float)*(BYTE*)((BYTE*)pkt + 5) * 0.00390625f;
                        v1.u = (float)(pkt[2] & 0xFF) / texW + ubase;
                        v1.v = (float)*(BYTE*)((BYTE*)pkt + 9) * 0.00390625f;
                        v2.u = (float)(pkt[3] & 0xFF) / texW + ubase;
                        v2.v = (float)*(BYTE*)((BYTE*)pkt + 0xD) * 0.00390625f;
                        v3.u = (float)(pkt[4] & 0xFF) / texW + ubase;
                        v3.v = (float)*(BYTE*)((BYTE*)pkt + 0x11) * 0.00390625f;
                        if (v0.u == v1.u || v2.u == v1.u || v0.u == v2.u) v1.u += 0.003f;
                        if (v0.v == v1.v || v2.v == v1.v || v0.v == v2.v) v1.v += 0.003f;
                        v0.r = v0.g = v0.b = 1.0f;
                        v1.r = v1.g = v1.b = 1.0f;
                        v2.r = v2.g = v2.b = 1.0f;
                        v3.r = v3.g = v3.b = 1.0f;
                        idx[0] = (WORD)(primIdx * 4 + 0);
                        idx[1] = (WORD)(primIdx * 4 + 1);
                        idx[2] = (WORD)(primIdx * 4 + 3);
                        idx[3] = (WORD)(primIdx * 4 + 2);
                        if (!elem->SetVertex(vertIdx,     (DWORD*)&v0) ||
                            !elem->SetVertex(vertIdx + 1, (DWORD*)&v1) ||
                            !elem->SetVertex(vertIdx + 2, (DWORD*)&v2) ||
                            !elem->SetVertex(vertIdx + 3, (DWORD*)&v3) ||
                            !elem->SetList(primIdx, idx)) {
                            goto storeFail;
                        }
                        ((DWORD*)elem)[7]    = 1;
                        ((DWORD*)elem)[0x12] = 1;
                        break;
                    }

                    default:
                        PSXObjDebugPrint("MarniSystem PSXObject::Store: unknown kind %x\n",
                                         kindTable[processed * 5]);
                        return 0;
                    }

                    pktCursor = (int)pkt + ((*pkt & 0xFF00) >> 6) + 4;
                    if (type == 0x3C00080C) {
                        vertIdx += 4;       // quad: 4 vertices per poly (iStack_7dc)
                    }
                    else {
                        vertIdx  += 3;      // triangle: 3 vertices per poly
                        listBase += 3;
                    }
                    primIdx  += 1;
                } while (primIdx < *kind);
            }

            elem->Unlock();                                 // vtable[7]
            processed = processed + 1;
            kind = kind + 5;
            elem = (CMarniViewport2*)((DWORD*)elem + 0x13);
            if (kindCount <= processed) break;
        }
    }

    // Final pass: override the clut material key for kinds whose clut does
    // not match the selected page
    if (kindCount > 0) {
        unsigned short* cp = (unsigned short*)(kindTable + 4);
        DWORD* e = (DWORD*)(slot + 0x40);                   // elem[0x10]
        for (int i = kindCount; i != 0; i--) {
            if (*cp != (unsigned int)selectedPage) {
                if (selectedPage < 0x10) {
                    e[0] = (DWORD)(selectedPage << 6);
                    e[1] = 0;
                }
                else {
                    e[0] = (DWORD)((selectedPage - 0x10) * 0x40);
                    e[1] = 0x100;
                }
            }
            e  = e + 0x13;
            cp = cp + 10;
        }
    }

    *(DWORD*)(slot + 0x4C4) = 1;                            // m_flag4C4
    *(DWORD*)(slot + 0x4C0) = (DWORD)kindCount;             // m_objectCount

    if (PSXObject_Resize(slot) != 0) {                      // FUN_00444ca0
        return 1;
    }

    PSXObjDebugPrint("MarniSystem PSXObject::Store: resize failed\n");
    *(DWORD*)(slot + 0x4C4) = 0;
    return 0;

storeFail:  // LAB_00446c5e
    PSXObjDebugPrint("MarniSystem PSXObject::Store: vertex write failed\n");
    *(DWORD*)(slot + 0x4C4) = 0;
    return 0;
}
