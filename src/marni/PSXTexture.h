// PSXTexture.h - MarniSystem PSXTexture class
// Handles PS1 TIM/PIX texture format loading and conversion to D3D textures
// Original class: MarniSystem::PSXTexture
// Store method address: 0x0041fb60 (LoadPSXImage in Ghidra)
#pragma once
#include "../platform/types.h"
#include "MarniBits.h"

// PSXTexture object size: 0x348 bytes
// The class uses explicit padding arrays to force field offsets to match
// the original game's layout, where embedded CMarniBits sub-objects are
// placed at fixed 0x68-byte intervals (8 slots: 0x00, 0x68, 0xD0, ... 0x2D8).
// Each slot is 0x68 bytes: 0x54 bytes for CMarniBits + 0x14 bytes for CLUT fields.
class PSXTexture {
public:
    // ========================================================================
    // Slot 0: CMarniBits base + PSXTexture fields (0x00 - 0x67)
    // Overlaps with the first CMarniBits sub-object (this IS the CMarniBits
    // at offset 0x00). PSXTexture fields at 0x00-0x53 mirror CMarniBits layout,
    // and 0x54-0x67 contain CLUT descriptor fields.
    // ========================================================================

    // CMarniBits header (offset 0x00 - 0x0B)
    void*  vtable;                    // 0x00 (4 bytes)
    void*  m_pPixelData;              // 0x04 (4 bytes)
    union {
        DWORD  m_Pitch;               // 0x08 — pitch while locked
        WORD*  m_pCLUTData;           // 0x08 — CMarniBits::m_pPalette: CLUT pointer after Store
    };

    BYTE   pad_0C[4];                 // 0x0C-0x0F — CMarniBits m_locked + part of pixel format gap

    // Clipping / region rects (offset 0x10 - 0x27)
    WORD   m_clipX;                   // 0x10
    WORD   m_clipY;                   // 0x12
    WORD   m_clipW;                   // 0x14
    WORD   m_clipH;                   // 0x16
    WORD   m_dispX;                   // 0x18
    WORD   m_dispY;                   // 0x1A
    WORD   m_dispW;                   // 0x1C
    WORD   m_dispH;                   // 0x1E
    WORD   m_rectX;                   // 0x20
    WORD   m_rectY;                   // 0x22
    WORD   m_rectW;                   // 0x24
    WORD   m_rectH;                   // 0x26

    BYTE   pad_28[2];                 // 0x28-0x29 padding to 0x2A

    // Pixel format (offset 0x2A - 0x37)
    BYTE   m_BitDepth;                // 0x2A
    BYTE   m_FormatFlags;             // 0x2B
    DWORD  m_WidthPixels;             // 0x2C
    DWORD  m_Height;                  // 0x30
    DWORD  m_RowStride;               // 0x34

    BYTE   pad_38[8];                 // 0x38-0x3F padding to 0x40 (CMarniBits fields)

    // Status flags (offset 0x40 - 0x53)
    DWORD  m_IsLocked;                // 0x40
    DWORD  m_DataSource;              // 0x44
    DWORD  m_HasCLUT;                 // 0x48
    DWORD  m_Flag4C;                  // 0x4C
    DWORD  m_Flag50;                  // 0x50

    // CLUT descriptor (offset 0x54 - 0x67): five DWORDs, exactly as
    // PSXTexture::Store (0x0041fb60) writes them. These are NOT 16-bit fields:
    // CreateTmdObjectInternal (0x00483910) matches a TMD object against a
    // texture page by comparing the full DWORDs at +0x54 and +0x58 with the
    // object's CLUT material keys. Declaring X/Y as packed WORDs made +0x54
    // read back as (Y << 16) | X, so no model ever matched its texture page and
    // no TMD render object was ever created.
    DWORD  m_CLUT_X;                  // 0x54 - CLUT VRAM X origin
    DWORD  m_CLUT_Y;                  // 0x58 - CLUT VRAM Y origin
    DWORD  m_ImgFlagsLo;              // 0x5C - image section dword, low half
    DWORD  m_ImgFlagsHi;              // 0x60 - image section dword, high half
    DWORD  m_Flag64;                  // 0x64
    // The palette dimensions are locals in the original Store and are not kept
    // in the object; callers that need them derive the entry count from
    // m_BitDepth and m_NumCLUTs.

    // ========================================================================
    // Slots 1-7: additional CMarniBits sub-objects + multi-CLUT descriptor
    // array (0x68 - 0x33F).
    //
    // IMPORTANT: the original keeps NO CLUT color data inside the object.
    // Store() (0x0041fb60) copies the palette to a HEAP buffer and 0xC0 is
    // the multi-CLUT descriptor array (0x68-byte stride per entry). Storing
    // the palette inline here gets it stomped by the multi-CLUT descriptor
    // writes whenever the TIM has more than one CLUT row (e.g. status.tim
    // with 3 palettes), corrupting the first ~21 palette entries.
    // ========================================================================
    BYTE   pad_68_33F[0x2D8];         // 0x68 - 0x33F (slots 1-7 + descriptors)

    // ========================================================================
    // Trailing fields (0x340 - 0x347)
    // ========================================================================
    DWORD  m_NumCLUTs;                // 0x340
    DWORD  m_IsInitialized;           // 0x344

    // Methods
    PSXTexture();
    ~PSXTexture();

    // Store: Load a PSX TIM/PIX image from a buffer
    //   imageData: pointer to raw file data (TIM format)
    //   copyData:  1 = allocate and copy pixel data, 0 = use in-place
    // Returns: 1 on success, 0 on failure
    // Original address: 0x0041fb60 (LoadPSXImage)
    int Store(int* imageData, int copyData);

    // Set pixel data address
    int SetAddress(void* pixelData, void* clutData);

    // operator=: Copy a PSXTexture (all 8 embedded CLUT entries) (0x0041f9c0)
    //   src: source PSXTexture to copy from
    // Returns: 1 on success, 0 on failure
    // Original: "MarniSystem PSXTexture::operator =" at 0x004b9f9c
    int CopyFrom(PSXTexture* src);

    // LoadFromFile: Open a TIM file and call Store (0x0041fa60)
    //   filename: path to TIM file
    // Returns: 1 on success, 0 on failure
    int LoadFromFile(const char* filename);

    // ClearCLUTEntries: Release all embedded CMarniBits sub-objects (0x0041fb10)
    // Iterates over up to 8 embedded CMarniBits at 0x68-byte intervals,
    // calls Release on each, then zeroes m_NumCLUTs and m_IsInitialized.
    void ClearCLUTEntries();

    // PSXTextureArray_Clear: Reset the PSXTexture array (0x00420000)
    // Calls ClearCLUTEntries then destroys embedded CMarniBits sub-objects.
    static void ArrayClear(PSXTexture* tex);

    // Element constructor wrapper (0x0041ff60)
    // Constructs a single embedded CMarniBits sub-object.
    static void ConstructElement(void* element);

    // Element destructor wrapper (0x0041ffb0)
    // Destroys a single embedded CMarniBits sub-object.
    static void DestroyElement(void* element);
};

// Known debug strings for this class:
// "MarniSystem PSXTexture::Store" at 0x004b9fc0
// "MarniSystem PSXTexture::operator =" at 0x004b9f9c

// Free function: copies CMarniBits data (0x004034d0 wrapper)
int CMarniBits_CopyFrom(void* dest, void* src);
