// Marni3DObject.h - Direct3D Object / ExecuteBuffer / Polyhedra classes
// 3D geometry management for the Marni System
// Original: Direct3DObject, MarniExecuteBuffer, MarniPolyhedra, Direct3DTMD
#pragma once
#include "../platform/types.h"
#include <new>

// ============================================================================
// CDirect3DObject - Base class for 3D vertex/index buffer management
// Original vtable: 0x004af090 (CMarniExecuteBuffer) / Direct3DExecuteBuffer_VTable
// Object size: 0x38 bytes (14 DWORDs)
//
// NOTE: these classes are intentionally NOT virtual. The original binaries
// stored the vtable pointer as a plain field at offset 0x00 and methods were
// ordinary member functions. Declaring C++ virtuals here would insert a
// hidden vptr at 0x00 and shift every field by 4 bytes, breaking the raw
// DWORD-indexed code paths that mirror the original memory layout.
// ============================================================================
class CDirect3DObject {
public:
    // --- VTable pointer (offset 0x00) ---
    void** vtable;                    // 0x00

    // --- Buffer pointers (offset 0x04 - 0x0B) ---
    void*  m_pVertexBuffer;           // 0x04 - vertex data (32 bytes per vertex)
    void*  m_pIndexBuffer;            // 0x08 - index list data (8/16 bytes per primitive)

    // --- State flags (offset 0x0C - 0x23) ---
    DWORD  m_bHasBuffers;             // 0x0C - buffers allocated flag
    DWORD  m_flag10;                  // 0x10
    DWORD  m_locked;                  // 0x14 - lock state (0=unlocked, 1=locked)
    DWORD  m_flag18;                  // 0x18
    DWORD  m_unknown1C;               // 0x1C
    DWORD  m_unknown20;               // 0x20

    // --- Counts (offset 0x24 - 0x37) ---
    DWORD  m_vertexCount;             // 0x24 - current vertex count
    DWORD  m_listCount;               // 0x28 - current primitive count
    DWORD  m_primitiveType;           // 0x2C - 3=triangle, 4=quad
    DWORD  m_vertexCapacity;          // 0x30 - max vertices
    DWORD  m_listCapacity;            // 0x34 - max primitives

public:
    CDirect3DObject();                                   // Base constructor (FUN_00430e80)
    ~CDirect3DObject();

    // VTable methods (8 entries at 0x004af090)
    int Release();                               // [0] 0x00427270 - free buffers
    int CreateWork(int vtxCount, int listCount, int primType); // [1] 0x00415e90 - allocate buffers
    int GetVertex(int index, DWORD* outData);    // [2] 0x00415a80 - read vertex
    int SetVertex(int index, DWORD* data);       // [3] 0x00415b40 - write vertex
    int GetList(int index, WORD* outIndices);    // [4] 0x00415c10 - read index list
    int SetList(int index, WORD* indices);       // [5] 0x00415d20 - write index list
    int Lock(void** outVtx, void** outIdx);      // [6] 0x004159e0 - lock, return pointers
    int Unlock();                                // [7] 0x00415a50 - unlock
};

// ============================================================================
// CMarniExecuteBuffer - Execute buffer wrapper
// Inherits: CDirect3DObject
// Original vtable: 0x004af090
// Constructor: 0x00415f70
// ============================================================================
class CMarniExecuteBuffer : public CDirect3DObject {
public:
    CMarniExecuteBuffer();                               // 0x00415f70
    ~CMarniExecuteBuffer();                              // 0x00415fd0
};

// ============================================================================
// CMarniPolyhedra - 3D polyhedra geometry container
// Shares layout with CDirect3DObject base
// Lock: 0x004159e0, Unlock: 0x00415a50
// Constructor via FUN_00426600 (MarniPolyhedra::CopyFrom)
// ============================================================================
class CMarniPolyhedra : public CDirect3DObject {
public:
    CMarniPolyhedra();
    ~CMarniPolyhedra();

    // CopyFrom: copy vertex/index data from source (0x00426600)
    int CopyFrom(CMarniPolyhedra* src);
};

// ============================================================================
// CMarniViewport2 - 3D viewport with float vertices (11 floats = 0x2C bytes)
// Original vtable: 0x004af0f8 (Type 2 - Direct3DViewport_VTable)
// Constructor: 0x004272e0  Destructor: 0x00427320
// Total size: 0x4C bytes (19 DWORDs) — extends CDirect3DObject's 0x38 bytes
// m_unknown1C (0x1C) serves as render style flag (0=flat, 1=strip)
// m_unknown20 (0x20) serves as conversion state flag
// ============================================================================
class CMarniViewport2 : public CDirect3DObject {
public:
    // Additional fields beyond CDirect3DObject (0x38 - 0x48)
    DWORD  m_renderStyle;            // 0x38 - D3D render state value
    DWORD  m_converted;              // 0x3C - conversion state flag
    DWORD  m_padding40;              // 0x40 - extended field (used in CopyFrom)

public:
    CMarniViewport2();                                   // 0x004272e0
    ~CMarniViewport2();                                  // 0x00427320

    // Viewport2-specific implementations (called via vtable in the original)
    int Release();                      // [0] 0x00427270
    int CreateWork(int vtxCount, int polyCount, int polyType); // [1] 0x00427100
    int GetVertex(int index, DWORD* outData);    // [2] 0x00426d60
    int SetVertex(int index, DWORD* data);       // [3] 0x00426df0
    int GetList(int index, WORD* outIndices);    // [4] 0x00426e80
    int SetList(int index, WORD* indices);       // [5] 0x00426f70
    int Lock(void** outVtx, void** outIdx);      // [6] 0x004271e0
    int Unlock();                                 // [7] 0x00427250

    // CopyFrom: copy vertex/index data from source viewport (0x00426600)
    // Handles format conversion (strip↔flat), normal recalculation
    int CopyFrom(CMarniViewport2* src);

    // Convert0: convert strip indices to flat triangle lists (0x004262e0)
    int Convert0(CMarniViewport2* src);
};

// Free function: subdivides triangle polyhedra (0x00425c10)
int TriangleDivideMarniPolyhedra(CMarniViewport2* polyArray, int count);

// ============================================================================
// MarniViewport2_InitEntry - raw constructor for a CMarniViewport2 element in
// unconstructed BSS memory. Mirrors the original ctor (0x004272e0): vtable
// 0x004af0f8, all fields zeroed, m_unknown1C = 1.
// Used to seed the g_objectListPtrArray entries (original: placement-new of
// CMarniViewport2[256] at 0x008fc430) and the embedded TMD slot elements.
// ============================================================================
void MarniViewport2_InitEntry(void* entry);

// ============================================================================
// CMarniDirect3DTMD - TMD 3D model renderer
// Original class: MarniSystem::Direct3DTMD
// Object size: 0x1594+ bytes
// Constructor: 0x00415910
// ============================================================================

// Per-object data within TMD (stride 0x84 bytes, 0x21 DWORDs)
struct TMDObjectData {
    float scaleX, scaleY, scaleZ;    // +0x00 - scale factors
    DWORD objectHandle;              // +0x0C - D3D object handle
    DWORD textureId;                 // +0x10 - texture/material ID
    float matrix[12];                // +0x14 - 3x4 transform matrix (or 4x3)
};

class CMarniDirect3DTMD {
public:
    // --- VTable pointer (offset 0x00) ---
    void** vtable;                    // 0x00

    // --- Unknown/padding (0x04 - 0x47) ---
    BYTE   m_pad1[0x44];             // 0x04 - 0x47

    // --- Material data pointer (0x48) ---
    void*  m_pMaterialData;          // 0x48 - material/texture descriptor array (0x4C stride)

    // --- Padding to object management fields ---
    BYTE   m_pad2[0x474];            // 0x4C - 0x4BF

    // --- Object management (0x4C0 - 0x4CF) ---
    DWORD  m_objectCount;            // 0x4C0 - number of TMD mesh objects (max 16)
    DWORD  m_flag4C4;                // 0x4C4
    DWORD  m_unknown4C8;             // 0x4C8
    void*  m_pD3DContext;            // 0x4CC - pointer to MarniSystem Direct3D context

    // --- Per-object data array (0x4D0 - 0xD0F) ---
    // 16 objects × 0x84 bytes each = 0x840 bytes
    BYTE   m_objectData[0x840];      // 0x4D0 - per-object transform data

    // --- Per-object data copy (0xD10 - 0x154F) ---
    // Working copy for double-buffering
    BYTE   m_objectDataCopy[0x840];  // 0xD10 - copy of per-object data

    // --- Object handles (0x1550 - 0x158F) ---
    // 16 handles × 4 bytes each = 64 bytes
    DWORD  m_objectHandles[16];      // 0x1550 - D3D object handles

    // --- State flag (0x1590) ---
    DWORD  m_initialized;            // 0x1590 - 0=not initialized, 1=ready

public:
    CMarniDirect3DTMD();                                  // 0x00415910
    ~CMarniDirect3DTMD();                                 // 0x00415990

    // Create: set up TMD objects from material context (0x00415650)
    int Create(void* d3dContext, void* materialContext, void* param3);

    // Transform: apply transforms and render objects (0x00415520)
    int Transform(void* d3dContext, void* renderData, void* outData, int doubleBuffer);

    // Destroy: release all object handles (0x00415880)
    int Destroy(void* d3dContext);

    // CleanupObjects: destroy + clean up resources (0x004158e0)
    int CleanupObjects(void* param);

    // Helper to clear the object data arrays
    void ClearObjectData();
};

// ============================================================================
// MarniSystem PSXObject::Store (FUN_004450e0, 0x004450e0)
// Parses a PSX TMD model into a Direct3DTMD object slot: enumerates the
// primitive "kinds" (grouped by flags/tpage/clut), then for each kind fills
// one embedded CMarniViewport2 element (stride 0x4C) with vertices/indices.
//   self        - Direct3DTMD slot (ECX in the original; g_tmdObjectBuffer entry)
//   tmdHdr      - TMD header pointer (the 0x41-magic patched header)
//   objIndex    - TMD object index (original always passes 0)
//   bankOrTpage - selected texture page (0xFFFFFFFF = auto-detect from CLUTs)
//   texRef      - texture page width used as UV divisor (from texBank + 0x2C)
// Returns 1 on success, 0 on failure.
// ============================================================================
int PSXObject_Store(CMarniDirect3DTMD* self, int* tmdHdr, int objIndex,
                    int bankOrTpage, int texRef);

// ============================================================================
// Known debug strings:
//   "MarniSystem Direct3DTMD::Trans"    at 0x004b45e8
//   "MarniSystem Direct3DTMD::Create"   at 0x004b46b8
//   "MarniPolyhedra::Lock"              at 0x004b46e8
//   "Direct3DObject::GetVertex"         at 0x004b4700
//   "Direct3DObject::SetVertex"         at 0x004b471c
//   "Direct3DObject::GetList"           at 0x004b4738
//   "Direct3DObject::SetList"           at 0x004b4750
//   "Direct3DObject::CreateWork"        at 0x004b47c0
//   "Direct3DTMD::Trans"                at 0x004b4624
// ============================================================================
