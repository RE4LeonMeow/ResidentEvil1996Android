// MarniBits.cpp - CMarniBits surface class implementation
// Faithful C++ port from decompiled Ghidra code (Resident Evil 1 PC)
//
// Original vtable at: 0x004af008
// Class size: 0x54 bytes (fields up to offset 0x50)
//
// CMarniBits is the 2D surface/pixel buffer class used throughout the
// Marni System. It wraps a raw pixel buffer and optional palette (CLUT),
// providing Lock/Unlock, pixel read/write, palette fill, and blit ops.

#include "MarniBits.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>
#include <cstring>
#include "../platform/types.h"
#include "../platform/platform.h"

#include "MarniSystem.h"

extern void* g_pMarniDirect3D;

// ============================================================================
// External memory operators (defined in MarniSystem.cpp / Globals)
// ============================================================================
extern void* operator_new(size_t size);
extern void operator_delete(void* ptr);

// ============================================================================
// Static helpers
// ============================================================================

static void MarniDebugPrint(const char* fmt, ...) {
    // stub - in original it printed to debug output
    (void)fmt;
}

// FUN_0048c5f0 - Rectangle clipping helper (stub)
static int ClipRect(RECT* src, float* dstRect, RECT* outRect) {
    // Stub - returns 0 for now (complex clipping logic)
    // The original intersects two rectangles and computes clipped result
    (void)src;
    (void)dstRect;
    (void)outRect;
    return 0;
}

// CMarniBits is declared in MarniBits.h

// ============================================================================
// VTable function type definitions (for the static vtable array)
// ============================================================================
typedef int  (*PFN_CMarniBits_Blt)(void* self, void* srcRect, CMarniBits* srcSurface);
typedef int  (*PFN_CMarniBits_BltFast)(void* self, void* dstRect, CMarniBits* srcSurface, void* srcRect2, DWORD flags, DWORD flags2, void* palette);
typedef int  (*PFN_CMarniBits_UnlockStub)(void* self);
typedef int  (*PFN_CMarniBits_PalBlt)(void* self, CMarniBits* srcSurface, DWORD param3, int numEntries);
typedef int  (*PFN_CMarniBits_Lock)(void* self, void** outData, DWORD* outPitch);
typedef int  (*PFN_CMarniBits_Unlock)(void* self);
typedef int  (*PFN_CMarniBits_Release)(void* self);

// ============================================================================
// Forward declarations of vtable static wrapper functions
// ============================================================================
static int VTable_Blt(void* self, void* srcRect, CMarniBits* srcSurface);
static int VTable_BltFast(void* self, void* dstRect, CMarniBits* srcSurface, void* srcRect2, DWORD flags, DWORD flags2, void* palette);
static int VTable_UnlockStub(void* self);
static int VTable_PalBlt(void* self, CMarniBits* srcSurface, DWORD param3, int numEntries);
static int VTable_Lock(void* self, void** outData, DWORD* outPitch);
static int VTable_Unlock(void* self);
static int VTable_Release(void* self);

// ============================================================================
// CMarniBits vtable (original at 0x004af008)
// 7 entries: Blt, BltFast, UnlockStub, PalBlt, Lock, Unlock, Release
// ============================================================================
static void* CMarniBits_vtable[7] = {
    (void*)VTable_Blt,         // [0] 0x00403090
    (void*)VTable_BltFast,     // [1] 0x00402020
    (void*)VTable_UnlockStub,  // [2] 0x00401ee0
    (void*)VTable_PalBlt,      // [3] 0x00401ef0
    (void*)VTable_Lock,        // [4] 0x00403450
    (void*)VTable_Unlock,      // [5] 0x004034c0
    (void*)VTable_Release,     // [6] 0x00404970 (FUN_00404970)
};

// ============================================================================
// VTable static wrapper function implementations
// ============================================================================

static int VTable_Blt(void* self, void* srcRect, CMarniBits* srcSurface) {
    return ((CMarniBits*)self)->Blt(srcRect, srcSurface);
}

static int VTable_BltFast(void* self, void* dstRect, CMarniBits* srcSurface, void* srcRect2, DWORD flags, DWORD flags2, void* palette) {
    return ((CMarniBits*)self)->BltFast(dstRect, srcSurface, srcRect2, flags, flags2, palette);
}

static int VTable_UnlockStub(void* self) {
    return ((CMarniBits*)self)->UnlockStub();
}

static int VTable_PalBlt(void* self, CMarniBits* srcSurface, DWORD param3, int numEntries) {
    return ((CMarniBits*)self)->PalBlt(srcSurface, param3, numEntries);
}

static int VTable_Lock(void* self, void** outData, DWORD* outPitch) {
    return ((CMarniBits*)self)->Lock(outData, outPitch);
}

static int VTable_Unlock(void* self) {
    return ((CMarniBits*)self)->Unlock();
}

static int VTable_Release(void* self) {
    return ((CMarniBits*)self)->Release();
}

// ============================================================================
// Constructor  - 0x00404910
// ============================================================================
CMarniBits::CMarniBits() {
    // *param_1 = &CMarniBits_vtable
    vtable = CMarniBits_vtable;

    // param_1[2] = 0   →   offset 0x08
    m_pPalette = NULL;
    // param_1[1] = 0   →   offset 0x04
    m_pPixelData = NULL;
    // param_1[3] = 0   →   offset 0x0C
    m_locked = 0;

    // Pixel format descriptors: zero 6 DWORDs at offsets 0x10-0x27
    // Red
    m_redShift = 0;   m_pad11 = 0;
    m_redMask = 0;    m_redWidth = 0;   m_pad15 = 0;
    // Green
    m_greenShift = 0; m_pad17 = 0;
    m_greenMask = 0;  m_greenWidth = 0; m_pad1B = 0;
    // Blue
    m_blueShift = 0;  m_pad1D = 0;
    m_blueMask = 0;   m_blueWidth = 0;  m_pad21 = 0;
    // Alpha
    m_alphaShift = 0; m_pad23 = 0;
    m_alphaMask = 0;  m_alphaWidth = 0; m_pad27 = 0;

    // Padding at 0x28
    m_pad28 = 0;
    // Bit depth / palette format at 0x2A-0x2B
    m_paletteFormat = 0;
    m_bitDepth = 0;

    // Dimensions
    m_pitch = 0;
    m_height = 0;
    m_width = 0;

    // Flags
    m_flag50 = 0;
    m_hasPalette = 0;
    m_dataSource = 0;
    m_isValid = 0;
    m_ownsPalette = 0;
    m_field3C = 0;
    m_field38 = 0;
}

// ============================================================================
// Destructor body  - 0x00404960 (FUN_00404960)
// Original: *param_1 = &CMarniBits_vtable; FUN_00404970();
// ============================================================================
void CMarniBits::DestructorBody() {
    // Reset vtable to base to prevent further virtual dispatch during destruction
    vtable = CMarniBits_vtable;
    Release();
}

// ============================================================================
// Destructor
// ============================================================================
CMarniBits::~CMarniBits() {
    DestructorBody();
}

// ============================================================================
// Release  - 0x00404970 (FUN_00404970)
// Frees owned pixel and palette data, zeros all fields.
// ============================================================================
int CMarniBits::Release() {
    // If locked, unlock directly (not through vtable — the vtable may be
    // corrupted if this CMarniBits lives inside a PSXTexture allocated in a
    // zero-initialised global buffer whose constructor was never called).
    if (m_locked != 0) {
        Unlock();
    }

    // If owns data, free pixel and palette buffers
    if (m_dataSource != 0 && m_ownsPalette != 0) {
        free(m_pPixelData);
        free(m_pPalette);
    }

    // Zero all fields (same pattern as constructor)
    m_pPalette = NULL;
    m_pPixelData = NULL;
    m_locked = 0;

    // Pixel format
    m_redShift = 0;   m_pad11 = 0;
    m_redMask = 0;    m_redWidth = 0;   m_pad15 = 0;
    m_greenShift = 0; m_pad17 = 0;
    m_greenMask = 0;  m_greenWidth = 0; m_pad1B = 0;
    m_blueShift = 0;  m_pad1D = 0;
    m_blueMask = 0;   m_blueWidth = 0;  m_pad21 = 0;
    m_alphaShift = 0; m_pad23 = 0;
    m_alphaMask = 0;  m_alphaWidth = 0; m_pad27 = 0;

    m_pad28 = 0;
    m_paletteFormat = 0;
    m_bitDepth = 0;

    m_pitch = 0;
    m_height = 0;
    m_width = 0;
    m_flag50 = 0;
    m_hasPalette = 0;
    m_dataSource = 0;
    m_isValid = 0;
    m_ownsPalette = 0;
    m_field3C = 0;
    m_field38 = 0;

    return 1;
}

// ============================================================================
// Lock  - 0x00403450 (vtable[4])
// Locks the surface for pixel access. Returns pixel data pointer and pitch.
// Note: offset 0x08 is used as m_pPalette when unlocked, and as pitch when locked.
// The Lock function reinterprets offset 0x08 as pitch.
// ============================================================================
int CMarniBits::Lock(void** outData, DWORD* outPitch) {
    if (m_isValid == 0) {
        printf("invalid class: MarniBits::Lock\n");
        return 0;
    }
    if (m_locked == 1) {
        printf("already locked: MarniBits::Lock\n");
        return 0;
    }
    if (outData != NULL) {
        *outData = m_pPixelData;
    }
    if (outPitch != NULL) {
        // Original (0x00403450): MOV EDX,[ECX+0x08] — returns m_pPalette
        // (offset 0x08), NOT m_pitch (offset 0x34). Callers like
        // CheckTextureRecreation and TextureLoader use this to get the CLUT
        // palette pointer, not the row pitch.
        *outPitch = (DWORD)(ULONG_PTR)m_pPalette;
    }
    m_locked = 1;
    return 1;
}

// ============================================================================
// Unlock  - 0x004034c0 (vtable[5])
// Unlocks the surface.
// ============================================================================
int CMarniBits::Unlock() {
    m_locked = 0;
    return 1;
}

// ============================================================================
// UnlockStub  - 0x00401ee0 (vtable[2])
// Stub method - just returns 1.
// ============================================================================
int CMarniBits::UnlockStub() {
    return 1;
}

// ============================================================================
// CalcAddress  - 0x00403860 (FUN_00403860)
// Computes the byte pointer to pixel (x, y) based on bit depth.
// ============================================================================
void* CMarniBits::CalcAddress(int x, int y) {
    if (m_isValid == 0) {
        printf("this Bits is invalid but you are calling CalcAddress.\n");
        return NULL;
    }
    if (m_locked == 0) {
        printf("this Bits doesn't be the Lock !! \n");
        return NULL;
    }
    if (x >= (int)m_width || y >= (int)m_height || x < 0 || y < 0) {
        MarniDebugPrint("the coordinate you specified is wrong. %d, %d\n", x, y);
        return NULL;
    }

    switch (m_bitDepth) {
    case 4:
        return (BYTE*)m_pPixelData + x / 2 + m_pitch * y;
    case 8:
        return (BYTE*)m_pPixelData + x + m_pitch * y;
    case 16:
        return (BYTE*)m_pPixelData + m_pitch * y + x * 2;
    case 24:
        return (BYTE*)m_pPixelData + x * 3 + m_pitch * y;
    case 32:
        return (BYTE*)m_pPixelData + m_pitch * y + x * 4;
    default:
        MarniDebugPrint("this BitPixel isn't supported.%d\n", m_bitDepth);
        return NULL;
    }
}

// ============================================================================
// SetAddress  - 0x004033f0
// Sets pixel data and palette data pointers for externally-managed buffers.
// ============================================================================
int CMarniBits::SetAddress(void* pixelData, void* paletteData) {
    if (m_isValid != 0) {
        printf("already valid: MarniBits::SetAddress\n");
        return 0;
    }
    if (m_dataSource == 1) {
        printf("cannot set address on copied data: MarniBits::SetAddress\n");
        return 0;
    }
    m_pPixelData = pixelData;
    m_pPalette = paletteData;
    m_dataSource = 0;
    return 1;
}

// ============================================================================
// SetColor  - 0x00403c90 (FUN_00403c90)
// Sets a single pixel value (raw/indexed). Handles 4, 8, 16 bit.
// flags: bit 1 = AND mode, bit 2 = OR mode, neither = overwrite.
// ============================================================================
int CMarniBits::SetColor(int x, int y, DWORD value, DWORD flags) {
    if (flags & 8) {
        printf("invalid flag 8: MarniBits::SetColor\n");
        return 0;
    }

    void* ptr = CalcAddress(x, y);
    if (!ptr) {
        printf("CalcAddress failed: MarniBits::SetColor\n");
        return 0;
    }

    BYTE bVal = (BYTE)value;

    switch (m_bitDepth) {
    case 4: {
        BYTE* bp = (BYTE*)ptr;
        BYTE existing = *bp;
        BYTE newVal;

        if (m_flag50 == 0) {
            if ((x & 1) != 0) {
                // Odd pixel: low nibble
                newVal = (existing & 0xF0) | (bVal & 0x0F);
            } else {
                // Even pixel: high nibble
                newVal = (existing & 0x0F) | ((bVal & 0x0F) << 4);
            }
        } else {
            if ((x & 1) == 0) {
                if (m_flag50 == 0) {
                    newVal = (existing & 0xF0) | (bVal & 0x0F);
                } else {
                    newVal = (existing & 0x0F) | ((bVal & 0x0F) << 4);
                }
            } else {
                newVal = (existing & 0xF0) | (bVal & 0x0F);
            }
        }

        if (flags & 2) {
            *(BYTE*)ptr = existing & newVal;
        } else if (flags & 4) {
            *(BYTE*)ptr = existing | newVal;
        } else {
            *(BYTE*)ptr = newVal;
        }
        return 1;
    }
    case 8:
        if (flags & 2) {
            *(BYTE*)ptr &= bVal;
        } else if (flags & 4) {
            *(BYTE*)ptr |= bVal;
        } else {
            *(BYTE*)ptr = bVal;
        }
        return 1;
    case 16: {
        WORD wVal = (WORD)value;
        if (flags & 2) {
            *(WORD*)ptr &= wVal;
        } else if (flags & 4) {
            *(WORD*)ptr |= wVal;
        } else {
            *(WORD*)ptr = wVal;
        }
        return 1;
    }
    default:
        printf("unsupported bit depth: MarniBits::SetColor\n");
        return 1;
    }
}

// ============================================================================
// GetColor  - 0x00404120
// Gets raw pixel value (palette index for paletted modes).
// ============================================================================
int CMarniBits::GetColor(int x, int y, DWORD* outColor) {
    void* ptr = CalcAddress(x, y);
    if (!ptr) {
        printf("CalcAddress failed: MarniBits::GetColor\n");
        return 0;
    }

    DWORD val;
    switch (m_bitDepth) {
    case 4: {
        BYTE b = *(BYTE*)ptr;
        if (m_flag50 == 0) {
            if ((x & 1) != 0) {
                val = b & 0x0F;
            } else {
                val = (b >> 4) & 0x0F;
            }
        } else {
            if ((x & 1) == 0) {
                if (m_flag50 == 0) {
                    val = b & 0x0F;
                } else {
                    val = (b >> 4) & 0x0F;
                }
            } else {
                val = b & 0x0F;
            }
        }
        break;
    }
    case 8:
        val = *(BYTE*)ptr;
        break;
    case 16:
        val = *(WORD*)ptr;
        break;
    case 32:
        val = *(DWORD*)ptr;
        break;
    default:
        MarniDebugPrint("unsupported bit depth: %d\n", m_bitDepth);
        return 0;
    }

    *outColor = val;
    return 1;
}

// ============================================================================
// SetPaletteColor  - 0x00403b00
// Sets a palette entry from 24-bit RGB color.
// ============================================================================
int CMarniBits::SetPaletteColor(int index, DWORD color, int param4) {
    if (m_isValid == 0) {
        printf("invalid class: MarniBits::SetPaletteColor\n");
        return 0;
    }
    if (param4 != 0) {
        printf("param4 must be 0: MarniBits::SetPaletteColor\n");
        return 0;
    }
    if (m_locked == 0) {
        printf("not locked: MarniBits::SetPaletteColor\n");
        return 0;
    }
    if (m_hasPalette == 0) {
        printf("no palette: MarniBits::SetPaletteColor\n");
        return 0;
    }
    if ((1u << (m_bitDepth & 0x1F)) <= (DWORD)index) {
        printf("palette index out of range: MarniBits::SetPaletteColor\n");
        return 0;
    }

    DWORD r = (color >> 16) & 0xFF;
    DWORD g = (color >> 8) & 0xFF;
    DWORD b = color & 0xFF;

    if (m_paletteFormat == 0x10) {
        // 16-bit palette entry
        WORD entry = (WORD)(
            ((r >> (8 - (m_redWidth & 0x1F))) & m_redMask) << (m_redShift & 0x1F) |
            ((g >> (8 - (m_greenWidth & 0x1F))) & m_greenMask) << (m_greenShift & 0x1F) |
            ((b >> (8 - (m_blueWidth & 0x1F))) & m_blueMask) << (m_blueShift & 0x1F)
        );
        ((WORD*)m_pPalette)[index] = entry;
    } else if (m_paletteFormat == 0x20) {
        // 32-bit palette entry
        DWORD entry =
            ((r >> (8 - (m_redWidth & 0x1F))) & m_redMask) << (m_redShift & 0x1F) |
            ((g >> (8 - (m_greenWidth & 0x1F))) & m_greenMask) << (m_greenShift & 0x1F) |
            ((b >> (8 - (m_blueWidth & 0x1F))) & m_blueMask) << (m_blueShift & 0x1F);
        ((DWORD*)m_pPalette)[index] = entry;
    } else {
        printf("unsupported palette format: MarniBits::SetPaletteColor\n");
    }

    return 1;
}

// ============================================================================
// GetPaletteColor  - 0x00404390
// Gets a palette entry as 24-bit RGB color.
// ============================================================================
int CMarniBits::GetPaletteColor(int index, DWORD* outColor) {
    if (m_isValid == 0) {
        printf("invalid class: MarniBits::GetPaletteColor\n");
        return 0;
    }
    if (m_locked == 0) {
        printf("not locked: MarniBits::GetPaletteColor\n");
        return 0;
    }
    if (m_hasPalette == 0) {
        printf("no palette: MarniBits::GetPaletteColor\n");
        return 0;
    }
    if ((1u << (m_bitDepth & 0x1F)) <= (DWORD)index) {
        printf("palette index out of range: MarniBits::GetPaletteColor\n");
        return 0;
    }

    DWORD packed;
    if (m_paletteFormat == 0x10) {
        packed = ((WORD*)m_pPalette)[index];
    } else if (m_paletteFormat == 0x20) {
        packed = ((DWORD*)m_pPalette)[index];
    } else {
        printf("unsupported palette format: MarniBits::GetPaletteColor\n");
        return 0;
    }

    // Extract components using pixel format descriptor
    DWORD r = ((packed >> (m_redShift & 0x1F)) & m_redMask) << (8 - (m_redWidth & 0x1F));
    DWORD g = ((packed >> (m_greenShift & 0x1F)) & m_greenMask) << (8 - (m_greenWidth & 0x1F));
    DWORD b = ((packed >> (m_blueShift & 0x1F)) & m_blueMask) << (8 - (m_blueWidth & 0x1F));

    *outColor = (r << 16) | (g << 8) | b;
    return 1;
}

// ============================================================================
// GetCurrentColor  - 0x00404210
// Gets pixel color as 32-bit ARGB. If paletted, delegates to GetIndexColor.
// For direct-color modes, converts packed format to ARGB using pixel format descriptor.
// ============================================================================
int CMarniBits::GetCurrentColor(int x, int y, DWORD* outColor) {
    // If has palette, delegate to GetIndexColor
    if (m_hasPalette != 0) {
        return GetIndexColor(x, y, outColor);
    }

    void* ptr = CalcAddress(x, y);
    if (!ptr) {
        printf("CalcAddress failed: MarniBits::GetCurrentColor\n");
        return 0;
    }

    DWORD raw;
    switch (m_bitDepth) {
    case 4: {
        BYTE b = *(BYTE*)ptr;
        if (m_flag50 == 0) {
            if ((x & 1) != 0) raw = b & 0x0F;
            else raw = (b >> 4) & 0x0F;
        } else {
            if ((x & 1) == 0) {
                if (m_flag50 == 0) raw = b & 0x0F;
                else raw = (b >> 4) & 0x0F;
            } else raw = b & 0x0F;
        }
        break;
    }
    case 8:  raw = *(BYTE*)ptr; break;
    case 16: raw = *(WORD*)ptr; break;
    case 32: raw = *(DWORD*)ptr; break;
    default:
        printf("unsupported: MarniBits::GetColor\n");
        return 0;
    }

    // Convert from packed format to ARGB using pixel format descriptor
    DWORD a = ((raw >> (m_alphaShift & 0x1F)) & m_alphaMask) << (8 - (m_alphaWidth & 0x1F));
    DWORD r = ((raw >> (m_redShift & 0x1F)) & m_redMask) << (8 - (m_redWidth & 0x1F));
    DWORD g = ((raw >> (m_greenShift & 0x1F)) & m_greenMask) << (8 - (m_greenWidth & 0x1F));
    DWORD b = ((raw >> (m_blueShift & 0x1F)) & m_blueMask) << (8 - (m_blueWidth & 0x1F));

    *outColor = (a << 24) | (r << 16) | (g << 8) | b;
    return 1;
}

// ============================================================================
// GetIndexColor  - 0x004044d0
// Gets pixel color from paletted surface. Reads palette index, looks up ARGB.
// ============================================================================
int CMarniBits::GetIndexColor(int x, int y, DWORD* outColor) {
    void* ptr = CalcAddress(x, y);
    if (!ptr) {
        printf("CalcAddress failed: MarniBits::GetIndexColor\n");
        return 0;
    }
    if (m_hasPalette == 0) {
        printf("no palette: MarniBits::GetIndexColor\n");
        return 0;
    }

    DWORD index;
    switch (m_bitDepth) {
    case 4: {
        BYTE b = *(BYTE*)ptr;
        if (m_flag50 == 0) {
            if ((x & 1) != 0) index = b & 0x0F;
            else index = (b >> 4) & 0x0F;
        } else {
            if ((x & 1) == 0) {
                if (m_flag50 == 0) index = b & 0x0F;
                else index = (b >> 4) & 0x0F;
            } else index = b & 0x0F;
        }
        break;
    }
    case 8:  index = *(BYTE*)ptr; break;
    case 16: index = *(WORD*)ptr; break;
    case 32: index = *(DWORD*)ptr; break;
    default:
        printf("unsupported: MarniBits::GetIndexColor\n");
        return 0;
    }

    // Look up in palette
    DWORD packed;
    if (m_paletteFormat == 8) {
        packed = ((BYTE*)m_pPalette)[index];
    } else if (m_paletteFormat == 0x10) {
        packed = ((WORD*)m_pPalette)[index];
    } else if (m_paletteFormat == 0x20) {
        packed = ((DWORD*)m_pPalette)[index];
    } else {
        printf("unsupported palette format: MarniBits::GetIndexColor\n");
        return 0;
    }

    DWORD a = ((packed >> (m_alphaShift & 0x1F)) & m_alphaMask) << (8 - (m_alphaWidth & 0x1F));
    DWORD r = ((packed >> (m_redShift & 0x1F)) & m_redMask) << (8 - (m_redWidth & 0x1F));
    DWORD g = ((packed >> (m_greenShift & 0x1F)) & m_greenMask) << (8 - (m_greenWidth & 0x1F));
    DWORD b = ((packed >> (m_blueShift & 0x1F)) & m_blueMask) << (8 - (m_blueWidth & 0x1F));

    *outColor = (a << 24) | (r << 16) | (g << 8) | b;
    return 1;
}

// ============================================================================
// SetIndexColor  - 0x004039b0
// Sets a pixel on a paletted surface by finding the closest palette match.
// ============================================================================
int CMarniBits::SetIndexColor(int x, int y, DWORD color, DWORD flags) {
    if (m_isValid == 0) {
        printf("invalid class: MarniBits::SetIndexColor\n");
        return 0;
    }
    if (m_locked == 0) {
        printf("not locked: MarniBits::SetIndexColor\n");
        return 0;
    }
    if (m_hasPalette == 0) {
        printf("no palette: MarniBits::SetIndexColor\n");
        return 0;
    }

    // Find palette entry closest to the given color
    int bestIndex = 0;
    float bestDist = 1000.0f;
    int numEntries = 1 << (m_bitDepth & 0x1F);
    DWORD rTarget = (color >> 16) & 0xFF;
    DWORD gTarget = (color >> 8) & 0xFF;
    DWORD bTarget = color & 0xFF;

    for (int i = 0; i < numEntries; i++) {
        DWORD palColor;
        GetPaletteColor(i, &palColor);
        DWORD pr = (palColor >> 16) & 0xFF;
        DWORD pg = (palColor >> 8) & 0xFF;
        DWORD pb = palColor & 0xFF;

        int dr = (int)pr - (int)rTarget;
        int dg = (int)pg - (int)gTarget;
        int db = (int)pb - (int)bTarget;
        float dist = sqrtf((float)(dr*dr + dg*dg + db*db));

        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }

    return SetColor(x, y, bestIndex, flags);
}

// ============================================================================
// SetCurrentColor  - 0x00403e20
// Sets a pixel from ARGB color. For paletted surfaces, converts to indexed.
// For direct-color, packs ARGB into surface format. Handles alpha blending (flag 8).
// ============================================================================
int CMarniBits::SetCurrentColor(int x, int y, DWORD color, DWORD flags) {
    // If has palette, convert to indexed and use SetIndexColor
    if (m_hasPalette != 0) {
        return SetIndexColor(x, y, color, flags);
    }

    void* ptr = CalcAddress(x, y);
    if (!ptr) {
        printf("CalcAddress failed: MarniBits::SetCurrentColor\n");
        return 0;
    }

    DWORD a = (color >> 24) & 0xFF;
    DWORD r = (color >> 16) & 0xFF;
    DWORD g = (color >> 8) & 0xFF;
    DWORD b = color & 0xFF;

    // Handle alpha blending if flag 8 is set
    if (flags & 8) {
        if (a == 0xFF) {
            // Fully opaque: read existing color and blend
            DWORD existing;
            GetCurrentColor(x, y, &existing);
            b = existing & 0xFF;
            r = (existing >> 16) & 0xFF;
            g = (existing >> 8) & 0xFF;
        } else if (a != 0) {
            DWORD existing;
            GetCurrentColor(x, y, &existing);
            DWORD invA = 256 - a;
            r = ((((existing >> 16) & 0xFF) * a) >> 8) + ((invA * r) >> 8);
            g = ((((existing >> 8) & 0xFF) * a) >> 8) + ((invA * g) >> 8);
            b = (((existing & 0xFF) * a) >> 8) + ((invA * b) >> 8);
        }
    }

    // Pack color into surface format
    DWORD packed =
        ((r >> (8 - (m_redWidth & 0x1F))) & m_redMask) << (m_redShift & 0x1F) |
        ((g >> (8 - (m_greenWidth & 0x1F))) & m_greenMask) << (m_greenShift & 0x1F) |
        ((b >> (8 - (m_blueWidth & 0x1F))) & m_blueMask) << (m_blueShift & 0x1F) |
        ((a >> (8 - (m_alphaWidth & 0x1F))) & m_alphaMask) << (m_alphaShift & 0x1F);

    // Write to surface
    switch (m_bitDepth) {
    case 8:
        if (flags & 2) *(BYTE*)ptr &= (BYTE)packed;
        else if (flags & 4) *(BYTE*)ptr |= (BYTE)packed;
        else *(BYTE*)ptr = (BYTE)packed;
        break;
    case 16:
        if (flags & 2) *(WORD*)ptr &= (WORD)packed;
        else if (flags & 4) *(WORD*)ptr |= (WORD)packed;
        else *(WORD*)ptr = (WORD)packed;
        break;
    case 24: {
        BYTE* bp = (BYTE*)ptr;
        BYTE bHigh = (BYTE)(packed >> 16);
        if (flags & 2) {
            *(WORD*)bp &= (WORD)packed;
            bp[2] &= bHigh;
        } else if (flags & 4) {
            *(WORD*)bp |= (WORD)packed;
            bp[2] |= bHigh;
        } else {
            *(WORD*)bp = (WORD)packed;
            bp[2] = bHigh;
        }
        break;
    }
    case 32:
        if (flags & 2) *(DWORD*)ptr &= packed;
        else if (flags & 4) *(DWORD*)ptr |= packed;
        else *(DWORD*)ptr = packed;
        break;
    default:
        MarniDebugPrint("unsupported bit depth: %d\n", m_bitDepth);
        break;
    }

    return 1;
}

// ============================================================================
// PalBlt  - 0x00401ef0 (vtable[3])
// Copies palette entries from source surface to this surface.
// ============================================================================
int CMarniBits::PalBlt(CMarniBits* srcSurface, DWORD param3, int numEntries) {
    (void)param3;

    if (m_isValid == 0) {
        printf("invalid class: MarniBits::PalBlt\n");
        return 0;
    }
    if (srcSurface->m_hasPalette == 0 || m_hasPalette == 0) {
        printf("no palette: MarniBits::PalBlt\n");
        return 0;
    }

    if (numEntries == 0) {
        BYTE srcBpp = srcSurface->m_bitDepth;
        BYTE dstBpp = m_bitDepth;
        if (dstBpp < srcBpp) srcBpp = dstBpp;
        numEntries = 1 << (srcBpp & 0x1F);
    }

    if ((1 << (m_bitDepth & 0x1F)) < numEntries) {
        printf("not enough palette entries: MarniBits::PalBlt\n");
        return 0;
    }

    // Lock both surfaces
    Lock(NULL, NULL);
    if (srcSurface->Lock(NULL, NULL) == 0) {
        printf("lock failed: MarniBits::operator=\n");
        return 0;
    }

    // Copy palette entries
    for (int i = 0; i < numEntries; i++) {
        DWORD srcColor;
        srcSurface->GetPaletteColor(i, &srcColor);
        SetPaletteColor(i, srcColor, 0);
    }

    Unlock();
    srcSurface->Unlock();
    return 1;
}

// ============================================================================
// Blt  - 0x00403090 (vtable[0])
// Copies pixel data from source surface to this surface.
// srcRect: source rectangle in source surface coordinates (NULL = full).
// srcSurface: source surface to copy from.
// ============================================================================
int CMarniBits::Blt(void* srcRect, CMarniBits* srcSurface) {
    if (m_isValid == 0) {
        printf("invalid class: MarniBits::Blt\n");
        return 0;
    }

    RECT dstRect, srcRc;
    int srcX = 0, srcY = 0;

    if (srcRect == NULL) {
        SetRect(&dstRect, 0, 0, (int)m_width - 1, (int)m_height - 1);
        SetRect(&srcRc, 0, 0, (int)srcSurface->m_width - 1, (int)srcSurface->m_height - 1);
    } else {
        RECT tmpRect;
        SetRect(&tmpRect, 0, 0, (int)m_width - 1, (int)m_height - 1);
        // Original calls ClipRect(&tmpRect, srcRect, &dstRect)
        // For now use a simple copy of the provided rect
        float* sr = (float*)srcRect;
        dstRect.left   = (int)sr[0];
        dstRect.top    = (int)sr[1];
        dstRect.right  = (int)sr[2];
        dstRect.bottom = (int)sr[3];

        // Compute source rectangle from same coordinates
        SetRect(&srcRc, dstRect.left, dstRect.top, dstRect.right, dstRect.bottom);

        srcX = dstRect.left;
        srcY = dstRect.top;
    }

    // Lock both surfaces
    void* dstData; DWORD dstPitch;
    void* srcData; DWORD srcPitch;

    if (Lock(&dstData, &dstPitch) == 0) {
        printf("lock failed: MarniBits::Blt\n");
        return 0;
    }
    if (srcSurface->Lock(&srcData, &srcPitch) == 0) {
        printf("lock failed: MarniBits::Blt\n");
        Unlock();
        return 0;
    }

    int dstWidth = dstRect.right - dstRect.left + 1;
    int dstHeight = dstRect.bottom - dstRect.top + 1;

    // Pixel-by-pixel copy
    for (int dy = 0; dy < dstHeight; dy++) {
        for (int dx = 0; dx < dstWidth; dx++) {
            DWORD color;
            srcSurface->GetCurrentColor(srcX + dx, srcY + dy, &color);
            SetCurrentColor(dstRect.left + dx, dstRect.top + dy, color, 0);
        }
    }

    Unlock();
    srcSurface->Unlock();
    return 1;
}

// ============================================================================
// BltFast  - 0x00402020 (vtable[1])
// Fast blit with format conversion and scaling support.
// This is the main rendering function used by the PSX graphics pipeline.
//
// Parameters:
//   dstRect  - Destination rectangle in this surface (NULL = full)
//   srcSurface - Source surface
//   srcRect2 - Source rectangle (NULL = full)
//   flags    - Bit flags: 1=colorkey(black transparent), 0x10=mirror X, 0x20=mirror Y
//   flags2   - Alpha blending multiplier (0=none)
//   palette  - Optional palette override (NULL = use surface palette)
// ============================================================================
int CMarniBits::BltFast(void* dstRect, CMarniBits* srcSurface, void* srcRect2, DWORD flags, DWORD flags2, void* palette) {
    if (m_isValid == 0 || srcSurface->m_isValid == 0) {
        printf("invalid surface: MarniBits::Blt\n");
        return 0;
    }

    // Parse destination rectangle
    RECT dstRc, srcRc;

    if (dstRect == NULL) {
        SetRect(&dstRc, 0, 0, (int)m_width - 1, (int)m_height - 1);
    } else {
        int* dr = (int*)dstRect;
        dstRc.left   = dr[0];
        dstRc.top    = dr[1];
        dstRc.right  = dr[2];
        dstRc.bottom = dr[3];
    }

    if (srcRect2 == NULL) {
        SetRect(&srcRc, 0, 0, (int)srcSurface->m_width - 1, (int)srcSurface->m_height - 1);
    } else {
        int* sr = (int*)srcRect2;
        srcRc.left   = sr[0];
        srcRc.top    = sr[1];
        srcRc.right  = sr[2];
        srcRc.bottom = sr[3];
    }

    // Lock both surfaces
    void* lock1, *lock2;
    if (Lock(&lock1, NULL) == 0 || srcSurface->Lock(&lock2, NULL) == 0) {
        printf("lock failed: MarniBits::Blt\n");
        Unlock();
        srcSurface->Unlock();
        return 0;
    }

    // Compute dimensions
    int dstW = dstRc.right - dstRc.left + 1;
    int dstH = dstRc.bottom - dstRc.top + 1;
    int srcW = srcRc.right - srcRc.left + 1;
    int srcH = srcRc.bottom - srcRc.top + 1;

    // Scaling factors
    float scaleX = (float)srcW / (float)dstW;
    float scaleY = (float)srcH / (float)dstH;

    // Pixel-by-pixel blit
    for (int dy = 0; dy < dstH; dy++) {
        int sy = srcRc.top + (int)((float)dy * scaleY);
        for (int dx = 0; dx < dstW; dx++) {
            int sx = srcRc.left + (int)((float)dx * scaleX);

            // Handle mirroring
            int finalSX = sx;
            int finalSY = sy;
            if (flags & 0x10) finalSX = srcRc.right - (sx - srcRc.left);
            if (flags & 0x20) finalSY = srcRc.bottom - (sy - srcRc.top);

            DWORD color;
            srcSurface->GetCurrentColor(finalSX, finalSY, &color);

            // Handle colorkey (black = transparent)
            if ((flags & 1) && (color & 0xFFFFFF) == 0) continue;

            // Handle alpha blending for flags2
            if (flags2 != 0) {
                DWORD a = (color >> 24) & 0xFF;
                DWORD r = (color >> 16) & 0xFF;
                DWORD g = (color >> 8) & 0xFF;
                DWORD b = color & 0xFF;
                // Clamp
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                if (a > 255) a = 255;
                color = (a << 24) | (r << 16) | (g << 8) | b;
            }

            SetCurrentColor(dstRc.left + dx, dstRc.top + dy, color, flags);
        }
    }

    Unlock();
    srcSurface->Unlock();
    return 1;
}

// ============================================================================
// CopyFrom / operator=  - 0x004034d0
// Copies data from another surface. Handles pointer ownership and format conversion.
// ============================================================================
int CMarniBits::CopyFrom(CMarniBits* src) {
    if (src->m_isValid == 0) {
        printf("invalid source: MarniBits::operator=\n");
        return 0;
    }

    if (m_dataSource == 0) {
        // Destination has no owned data - just copy pointers
        if (src->m_ownsPalette == 0) {
            printf("cannot take ownership of pointer: MarniBits::operator=\n");
            return 0;
        }

        m_pPixelData = src->m_pPixelData;
        m_pPalette = src->m_pPalette;

        // Copy pixel format
        m_redShift = src->m_redShift;   m_pad11 = src->m_pad11;
        m_redMask = src->m_redMask;     m_redWidth = src->m_redWidth;   m_pad15 = src->m_pad15;
        m_greenShift = src->m_greenShift; m_pad17 = src->m_pad17;
        m_greenMask = src->m_greenMask; m_greenWidth = src->m_greenWidth; m_pad1B = src->m_pad1B;
        m_blueShift = src->m_blueShift; m_pad1D = src->m_pad1D;
        m_blueMask = src->m_blueMask;   m_blueWidth = src->m_blueWidth;  m_pad21 = src->m_pad21;
        m_alphaShift = src->m_alphaShift; m_pad23 = src->m_pad23;
        m_alphaMask = src->m_alphaMask; m_alphaWidth = src->m_alphaWidth; m_pad27 = src->m_pad27;

        m_bitDepth = src->m_bitDepth;
        m_paletteFormat = src->m_paletteFormat;
        m_width = src->m_width;
        m_height = src->m_height;
        m_pitch = src->m_pitch;
        m_hasPalette = src->m_hasPalette;
        m_flag50 = src->m_flag50;
        m_dataSource = 0;
        m_locked = 0;
        m_isValid = 1;
        m_ownsPalette = 1;
        return 1;
    }

    if (m_isValid == 0) return 0;

    // If source has no palette but dest does, fill dest palette with grayscale
    if (m_hasPalette != 0 && src->m_hasPalette == 0) {
        Lock(NULL, NULL);
        int numEntries = 1 << (m_bitDepth & 0x1F);
        for (DWORD i = 0; i < (DWORD)numEntries; i++) {
            DWORD gray = ((i & 0x1C) << 5 | (i & 3)) << 6 | ((i & 0xE0) << 0x10);
            SetPaletteColor(i, gray, 0);
        }
        Unlock();
    } else if (m_hasPalette != 0 && src->m_hasPalette != 0) {
        // Check if formats match for optimization
        if (m_bitDepth != src->m_bitDepth) {
            printf("bit depth mismatch: MarniBits::operator=\n");
            return 0;
        }
        // Copy palette via PalBlt
        PalBlt(src, 0, 0);
    }

    // Check if direct copy is possible (same format, same dimensions, no palette)
    bool fastCopy = true;
    BYTE* fields1 = (BYTE*)&m_redShift;
    BYTE* fields2 = (BYTE*)&src->m_redShift;
    for (int i = 0; i < 0x1A; i++) {
        if (fields1[i] != fields2[i]) { fastCopy = false; break; }
    }

    if (fastCopy && m_width == src->m_width && m_height == src->m_height &&
        m_hasPalette == 0 && src->m_hasPalette == 0 &&
        m_bitDepth == src->m_bitDepth) {

        Lock(NULL, NULL);
        src->Lock(NULL, NULL);

        for (DWORD y = 0; y < m_height; y++) {
            void* dstPtr = CalcAddress(0, (int)y);
            void* srcPtr = src->CalcAddress(0, (int)y);
            DWORD copySize;

            switch (m_bitDepth) {
            case 8:
                copySize = m_width;
                memcpy(dstPtr, srcPtr, copySize);
                break;
            case 16:
                copySize = m_width * 2;
                memcpy(dstPtr, srcPtr, copySize);
                break;
            case 32:
                copySize = m_width * 4;
                memcpy(dstPtr, srcPtr, copySize);
                break;
            default:
                printf("unsupported: MarniBits::operator=\n");
                Unlock();
                src->Unlock();
                return 0;
            }
        }

        Unlock();
        src->Unlock();
        return 1;
    }

    // Fallback: full Blt
    RECT dstFull, srcFull;
    SetRect(&dstFull, 0, 0, (int)m_width - 1, (int)m_height - 1);
    SetRect(&srcFull, 0, 0, (int)src->m_width - 1, (int)src->m_height - 1);

    int result = Blt(NULL, src);
    if (!result) {
        printf("Blt failed: MarniBits::operator=\n");
        return 0;
    }

    return 1;
}

// ============================================================================
// CreateWork  - 0x00404690
// Allocates pixel buffer and optional palette. Sets up pixel format descriptor.
//
// Parameters:
//   width, height - surface dimensions
//   bitDepth - 4, 8, 16, or 32 bits per pixel
//   paletteFlags - palette format (0=none, 1=create palette)
//
// The pixel format for 4/8/16-bit modes is:
//   Red:   shift=11, mask=0x1F, width=5
//   Green: shift=5,  mask=0x3F, width=6
//   Blue:  shift=0,  mask=0x1F, width=5
//   Alpha: shift=0,  mask=0x00, width=0
//
// For 32-bit:
//   ARGB:  Red shift=16, Green shift=8, Blue shift=0, Alpha shift=24
//          All masks=0xFF, All widths=8
// ============================================================================
int CMarniBits::CreateWork(int width, int height, int bitDepth, DWORD paletteFlags) {
    // Release existing resources first
    Release();

    m_width = (DWORD)width;
    m_height = (DWORD)height;
    m_bitDepth = (BYTE)bitDepth;

    // Allocate pixel buffer based on bit depth
    size_t pixelSize;
    int pitch;

    switch (bitDepth) {
    case 4:
        pixelSize = (size_t)(height * width) / 2;
        pitch = width / 2;
        break;
    case 8:
        pixelSize = (size_t)height * (size_t)width;
        pitch = width;
        break;
    case 16:
        pixelSize = (size_t)height * (size_t)width * 2;
        pitch = width * 2;
        break;
    case 32:
        pixelSize = (size_t)height * (size_t)width * 4;
        pitch = width * 4;
        break;
    default:
        MarniDebugPrint("unsupported bit depth: %d\n", bitDepth);
        return 0;
    }

    m_pPixelData = operator_new(pixelSize);
    m_pitch = (DWORD)pitch;

    if (m_pPixelData == NULL) {
        printf("allocation failed: MarniBits::CreateWork\n");
        return 0;
    }

    m_paletteFormat = (BYTE)paletteFlags;

    // Allocate palette if needed
    if (paletteFlags != 0) {
        m_hasPalette = 1;
        size_t paletteSize;
        if (bitDepth == 4) {
            paletteSize = (paletteFlags & 0xFFFFFFF8) * 2;
        } else if (bitDepth == 8) {
            paletteSize = (paletteFlags & 0xFFFFFFF8) << 5;
        } else {
            MarniDebugPrint("unsupported palette bit depth: %d\n", bitDepth);
            free(m_pPixelData);
            m_pPixelData = NULL;
            return 0;
        }

        m_pPalette = operator_new(paletteSize);
        if (m_pPalette == NULL) {
            printf("palette alloc failed: MarniBits::CreateWork\n");
            free(m_pPixelData);
            m_pPixelData = NULL;
            return 0;
        }
    }

    // Set up pixel format descriptor
    switch (bitDepth) {
    case 4:
    case 8:
    case 16:
        // PSX-style 16-bit format: R5:G6:B5 with custom shift layout
        {
            BYTE* fmt = (BYTE*)this;
            // Red: shift=11, mask=0x1F, width=5
            *(WORD*)(fmt + 0x10) = 0x000B;
            *(WORD*)(fmt + 0x12) = 0x001F;
            *(WORD*)(fmt + 0x14) = 0x0005;
            // Green: shift=5, mask=0x3F, width=6
            *(WORD*)(fmt + 0x16) = 0x0005;
            *(WORD*)(fmt + 0x18) = 0x003F;
            *(WORD*)(fmt + 0x1A) = 0x0006;
            // Blue: shift=0, mask=0x1F, width=5
            *(WORD*)(fmt + 0x1C) = 0x0000;
            *(WORD*)(fmt + 0x1E) = 0x001F;
            *(WORD*)(fmt + 0x20) = 0x0005;
            // Alpha: shift=0, mask=0, width=0
            *(WORD*)(fmt + 0x22) = 0x0000;
            *(WORD*)(fmt + 0x24) = 0x0000;
            *(WORD*)(fmt + 0x26) = 0x0000;
        }
        break;
    case 32:
        // Standard ARGB: A8:R8:G8:B8
        {
            BYTE* fmt = (BYTE*)this;
            // Red: shift=16, mask=0xFF, width=8
            *(WORD*)(fmt + 0x10) = 0x0010;
            *(WORD*)(fmt + 0x12) = 0x00FF;
            *(WORD*)(fmt + 0x14) = 0x0008;
            // Green: shift=8, mask=0xFF, width=8
            *(WORD*)(fmt + 0x16) = 0x0008;
            *(WORD*)(fmt + 0x18) = 0x00FF;
            *(WORD*)(fmt + 0x1A) = 0x0008;
            // Blue: shift=0, mask=0xFF, width=8
            *(WORD*)(fmt + 0x1C) = 0x0000;
            *(WORD*)(fmt + 0x1E) = 0x00FF;
            *(WORD*)(fmt + 0x20) = 0x0008;
            // Alpha: shift=24, mask=0xFF, width=8
            *(WORD*)(fmt + 0x22) = 0x0018;
            *(WORD*)(fmt + 0x24) = 0x00FF;
            *(WORD*)(fmt + 0x26) = 0x0008;
        }
        break;
    }

    m_isValid = 1;
    m_dataSource = 1;
    m_ownsPalette = 1;

    return 1;
}

// ============================================================================
// SaveBitmapToFile  - 0x00403210
// Saves surface as a 24-bit BMP file.
// When the surface has no pixel data (m_pPixelData == NULL) but is marked
// valid with dimensions, it captures the D3D11 backbuffer as the source.
// This mirrors the original design where the framebuffer CMarniBits at
// g_pMarniDirect3D + 0x2064 was always populated by software rendering;
// in the modern D3D11 pipeline we read back the swap chain on demand.
// ============================================================================
int CMarniBits::SaveBitmapToFile(const char* filename) {
    // If no pixel data, capture from the D3D11 backbuffer into this surface.
    // This handles the framebuffer proxy case (original: g_pMarniDirect3D + 0x2064).
    // The surface may not be marked valid yet (m_isValid == 0 from constructor),
    // so this check must come before the m_isValid guard below.
    bool captured = false;
    if (m_pPixelData == NULL) {
        void* backbufferRGBA = NULL;
        DWORD bw = 0, bh = 0;
        if (Marni_DX()->CaptureBackbufferToRGBA(&backbufferRGBA, &bw, &bh)) {
            if (backbufferRGBA) {
                DWORD w = bw;
                DWORD h = bh;
                BYTE* rgba = (BYTE*)backbufferRGBA;

                m_redShift   = 0;    m_pad11 = 0;
                            m_redMask    = 0xFF;
                            m_redWidth   = 8;    m_pad15 = 0;
                            m_greenShift = 8;    m_pad17 = 0;
                            m_greenMask  = 0xFF;
                            m_greenWidth = 8;    m_pad1B = 0;
                            m_blueShift  = 16;   m_pad1D = 0;
                            m_blueMask   = 0xFF;
                            m_blueWidth  = 8;    m_pad21 = 0;
                            m_alphaShift = 24;   m_pad23 = 0;
                            m_alphaMask  = 0xFF;
                            m_alphaWidth = 8;    m_pad27 = 0;
                            m_bitDepth      = 32;
                            m_paletteFormat = 0;
                            m_width         = w;
                            m_height        = h;
                            m_pitch         = w * 4;
                            m_pPixelData    = rgba;
                            m_pPalette      = NULL;
                            m_hasPalette    = 0;
                m_ownsPalette   = 1;
                m_isValid       = 1;
                m_dataSource    = 1;

                captured = true;
            }
        }
    }

    if (m_isValid == 0) {
        printf("invalid class: MarniBits::FileOut\n");
        return 0;
    }

    // Calculate BMP size: header (0x3A) + pixel data (width * height * 3)
    DWORD bmpSize = m_width * m_height * 3 + 0x3A;
    BYTE* buffer = (BYTE*)operator_new(bmpSize);
    if (!buffer) {
        if (captured) { free(m_pPixelData); m_pPixelData = NULL; }
        printf("allocation failed: MarniBits::FileOut\n");
        return 0;
    }

    // BITMAPFILEHEADER (14 bytes)
    *(WORD*)(buffer + 0)  = 0x4D42;       // 'BM' signature
    *(DWORD*)(buffer + 2)  = bmpSize;     // file size
    *(WORD*)(buffer + 6)  = 0;            // reserved1
    *(WORD*)(buffer + 8)  = 0;            // reserved2
    *(DWORD*)(buffer + 10) = 0x3A;        // offset to pixel data

    // BITMAPINFOHEADER (40 bytes, starts at offset 14)
    *(DWORD*)(buffer + 14) = 0x28;        // header size (40)
    *(DWORD*)(buffer + 18) = m_width;     // width
    *(DWORD*)(buffer + 22) = m_height;    // height
    *(WORD*)(buffer + 26)  = 1;           // planes
    *(WORD*)(buffer + 28)  = 24;          // bits per pixel
    *(DWORD*)(buffer + 30) = 0;           // compression (BI_RGB)
    *(DWORD*)(buffer + 34) = 0;           // image size (0 = auto)
    *(DWORD*)(buffer + 38) = 0;           // X pixels per meter
    *(DWORD*)(buffer + 42) = 0;           // Y pixels per meter
    *(DWORD*)(buffer + 46) = 0;           // colors used
    *(DWORD*)(buffer + 50) = 0;           // colors important

    // Construct a temporary 24-bit surface that uses the BMP buffer as pixel data.
    // The original code constructs a CMarniBits on the stack, manually sets up
    // its pixel format descriptor for 24-bit BGR output, and points its pixel
    // buffer at the BMP header+data allocation (via SetAddress).
    CMarniBits tempBmp;

    // Pixel format for 24-bit BGR (BMP native byte order):
    //   Red:   shift=16, mask=0xFF, width=8
    //   Green: shift=8,  mask=0xFF, width=8
    //   Blue:  shift=0,  mask=0xFF, width=8
    //   Alpha: shift=0,  mask=0x00, width=0 (left at constructor zero)
    tempBmp.m_redShift   = 16;   tempBmp.m_pad11 = 0;
    tempBmp.m_redMask    = 0xFF;
    tempBmp.m_redWidth   = 8;    tempBmp.m_pad15 = 0;
    tempBmp.m_greenShift = 8;    tempBmp.m_pad17 = 0;
    tempBmp.m_greenMask  = 0xFF;
    tempBmp.m_greenWidth = 8;    tempBmp.m_pad1B = 0;
    tempBmp.m_blueShift  = 0;    tempBmp.m_pad1D = 0;
    tempBmp.m_blueMask   = 0xFF;
    tempBmp.m_blueWidth  = 8;    tempBmp.m_pad21 = 0;
    // Alpha fields already zeroed by constructor

    tempBmp.m_bitDepth      = 24;
    tempBmp.m_paletteFormat = 0;
    tempBmp.m_width         = m_width;
    tempBmp.m_height        = m_height;
    tempBmp.m_pitch         = m_width * 3;

    // Set pixel data to point into the BMP buffer (past the headers)
    tempBmp.SetAddress(buffer + 0x3A, NULL);

    // Mark the temp surface as valid with owned pixel data
    tempBmp.m_ownsPalette = 1;
    tempBmp.m_isValid     = 1;
    tempBmp.m_dataSource  = 1;

    // Blt from source (this) to tempBmp (24-bit BMP buffer).
    // flags=0x20 mirrors Y because BMP stores rows bottom-to-top.
    tempBmp.BltFast(NULL, this, NULL, 0x20, 0, NULL);
    tempBmp.m_dataSource = 0;

    // Write to file
    plat_file_write(filename, buffer, bmpSize);

    free(buffer);

    // Clean up: if we captured from D3D11, free the temporary pixel data
    if (captured) {
        free(m_pPixelData);
        m_pPixelData    = NULL;
        m_width         = 0;
        m_height        = 0;
        m_pitch         = 0;
        m_bitDepth      = 0;
        m_isValid       = 0;
        m_dataSource    = 0;
        m_ownsPalette   = 0;
    }

    return 1;
}

// ============================================================================
// Static wrappers for SEH / exception handling (0x004033d2, 0x004033da)
// Used by the original's exception handling frame.
// ============================================================================

void CMarniBits::DestructorCallWrapper() {
    // Original: calls FUN_00404960 (destructor body)
    // In practice, 'this' would be passed via the exception frame context.
    // This stub exists for API compatibility.
}

void CMarniBits::ExceptionHandlerWrapper() {
    // Original: calls ___CxxFrameHandler (SEH exception handler)
    // Not needed in modern C++ with regular try/catch.
    // This stub exists for API compatibility.
}
