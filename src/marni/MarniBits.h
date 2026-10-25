// MarniBits.h - CMarniBits surface class declaration
// Original vtable at: 0x004af008
// Class size: 0x54 bytes (fields up to offset 0x50)
#pragma once

#include "../platform/types.h"

class CMarniBits {
public:
    // --- VTable pointer (offset 0x00) ---
    void** vtable;                    // 0x00

    // --- Pixel data (offset 0x04 - 0x0B) ---
    void*  m_pPixelData;              // 0x04
    void*  m_pPalette;                // 0x08

    // --- Lock state (offset 0x0C) ---
    DWORD  m_locked;                  // 0x0C

    // --- Pixel format descriptor (0x10 - 0x27, 24 bytes) ---
    BYTE   m_redShift;                // 0x10
    BYTE   m_pad11;                   // 0x11
    WORD   m_redMask;                 // 0x12
    BYTE   m_redWidth;                // 0x14
    BYTE   m_pad15;                   // 0x15
    BYTE   m_greenShift;              // 0x16
    BYTE   m_pad17;                   // 0x17
    WORD   m_greenMask;               // 0x18
    BYTE   m_greenWidth;              // 0x1A
    BYTE   m_pad1B;                   // 0x1B
    BYTE   m_blueShift;               // 0x1C
    BYTE   m_pad1D;                   // 0x1D
    WORD   m_blueMask;                // 0x1E
    BYTE   m_blueWidth;               // 0x20
    BYTE   m_pad21;                   // 0x21
    BYTE   m_alphaShift;              // 0x22
    BYTE   m_pad23;                   // 0x23
    WORD   m_alphaMask;               // 0x24
    BYTE   m_alphaWidth;              // 0x26
    BYTE   m_pad27;                   // 0x27

    // --- Padding (0x28 - 0x29) ---
    WORD   m_pad28;                   // 0x28

    // --- Bit depth / palette format (0x2A - 0x2B) ---
    BYTE   m_bitDepth;                // 0x2A
    BYTE   m_paletteFormat;           // 0x2B

    // --- Dimensions (0x2C - 0x37) ---
    DWORD  m_width;                   // 0x2C
    DWORD  m_height;                  // 0x30
    DWORD  m_pitch;                   // 0x34

    // --- Misc fields (0x38 - 0x3F) ---
    DWORD  m_field38;                 // 0x38
    DWORD  m_field3C;                 // 0x3C

    // --- Flags (0x40 - 0x53) ---
    DWORD  m_isValid;                 // 0x40
    DWORD  m_dataSource;              // 0x44
    DWORD  m_hasPalette;              // 0x48
    DWORD  m_ownsPalette;             // 0x4C
    DWORD  m_flag50;                  // 0x50

public:
    CMarniBits();                     // 0x00404910
    ~CMarniBits();

    void DestructorBody();            // 0x00404960

    // VTable methods
    int Blt(void* srcRect, CMarniBits* srcSurface);           // [0] 0x00403090
    int BltFast(void* dstRect, CMarniBits* srcSurface, void* srcRect2, DWORD flags, DWORD flags2, void* palette); // [1] 0x00402020
    int UnlockStub();                                         // [2] 0x00401ee0
    int PalBlt(CMarniBits* srcSurface, DWORD param3, int numEntries); // [3] 0x00401ef0
    int Lock(void** outData, DWORD* outPitch);                // [4] 0x00403450
    int Unlock();                                             // [5] 0x004034c0
    int Release();                                            // [6] 0x00404970

    // Surface operations
    void* CalcAddress(int x, int y);                          // 0x00403860
    int SetAddress(void* pixelData, void* paletteData);       // 0x004033f0
    int SetColor(int x, int y, DWORD value, DWORD flags);     // 0x00403c90
    int GetColor(int x, int y, DWORD* outColor);              // 0x00404120
    int SetPaletteColor(int index, DWORD color, int param4);  // 0x00403b00
    int GetPaletteColor(int index, DWORD* outColor);          // 0x00404390
    int GetCurrentColor(int x, int y, DWORD* outColor);       // 0x00404210
    int GetIndexColor(int x, int y, DWORD* outColor);         // 0x004044d0
    int SetIndexColor(int x, int y, DWORD color, DWORD flags); // 0x004039b0
    int SetCurrentColor(int x, int y, DWORD color, DWORD flags); // 0x00403e20
    int CopyFrom(CMarniBits* src);                            // 0x004034d0
    int CreateWork(int width, int height, int bitDepth, DWORD paletteFlags); // 0x00404690
    int SaveBitmapToFile(const char* filename);               // 0x00403210

    static void DestructorCallWrapper();       // 0x004033d2
    static void ExceptionHandlerWrapper();     // 0x004033da
};
