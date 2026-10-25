// MarniSystem.cpp - Marni System wrapper implementation.
//
// Originally this file contained a mixed DirectX 5.0-API / D3D11 backend with
// raw ID3D11* pointers scattered everywhere. Every D3D11 resource has been
// moved into the MarniDX shim (MarniDX.cpp), and this file now only forwards
// the CMarniDirect3D 12-entry vtable + C-style Marni* wrappers to MarniDX.
// The game layer sees the same CMarniDirect3D class ABI and C functions as
// before, but <d3d11.h> is no longer transitively included from here.
//
// Original class: CMarniDirect3D at vtable 0x004af230, size 0x21DC
// Original functions referenced by address in comments.

#include <cstdlib>                   // malloc/free - MSVC got this via <windows.h>
#include "../platform/platform.h"
#include "MarniSystem.h"
#include "MarniBits.h"
#include "MarniDX.h"
#include "MarniInput.h"
#include "MarniXInput.h"
#include "Globals.h"
#include "../game/TmdRenderer.h"
#include <cstdio>
#include <cstring>
#include <new>

// Verify the struct size is exactly what the original binary expects.
// operator_new(0x21DC) in InitializeMarniSystem must match sizeof.
static_assert(sizeof(CMarniDirect3D) == 0x21DC, "CMarniDirect3D size mismatch — padding fix needed");

// ============================================================================
// VTable function pointer types
// (unchanged — WindowProc.cpp / TmdAnimation.cpp cast vtable slots to these)
// ============================================================================
typedef int  (*PFN_RequestVideoMemory)(void* self);
typedef int  (*PFN_ChangeDisplayMode)(void* self, int mode);
typedef void (*PFN_SetD3DRenderer)(void* self, int renderer);
typedef int  (*PFN_Clear)(void* self);
typedef int  (*PFN_Present)(void* self);
typedef int  (*PFN_HandleWindowMessage)(void* self, HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam);
typedef int  (*PFN_CreateTextureHandle)(void* self, void* texDesc,
                                        unsigned int flags, void* outHandle);
typedef unsigned int (*PFN_CreateObjectHandle)(void* self, void* objDesc,
                                                unsigned char flags);
typedef int  (*PFN_DeleteTextureHandle)(void* self, int handle);
typedef int  (*PFN_DeleteObjectHandle)(void* self, int handle);
typedef int  (*PFN_SetTexture)(void* self, void* texData, unsigned int param);
typedef int  (*PFN_ResetTextures)(void* self);

// ============================================================================
// Forward declarations of vtable function implementations
// ============================================================================
static int  VTable_RequestVideoMemory(void* self);
static int  VTable_ChangeDisplayMode(void* self, int mode);
static void VTable_SetD3DRenderer(void* self, int renderer);
static int  VTable_Clear(void* self);
static int  VTable_Present(void* self);
static int  VTable_HandleWindowMessage(void* self, HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam);
static int  VTable_CreateTextureHandle(void* self, void* texDesc,
                                       unsigned int flags, void* outHandle);
static unsigned int VTable_CreateObjectHandle(void* self, void* objDesc,
                                              unsigned char flags);
static int  VTable_DeleteTextureHandle(void* self, int handle);
static int  VTable_DeleteObjectHandle(void* self, int handle);
static int  VTable_SetTexture(void* self, void* texData, unsigned int param);
static int  VTable_ResetTextures(void* self);

// ============================================================================
// Static vtable (12 entries, matching original layout at 0x004af230)
// The callers in WindowProc.cpp / TmdAnimation.cpp / ObjectManager.cpp walk
// this by index, so order is frozen.
// ============================================================================
static void* g_CMarniDirect3D_VTable[12] = {
    (void*)VTable_RequestVideoMemory,   // [0] 0x00448630
    (void*)VTable_ChangeDisplayMode,    // [1] 0x00449300
    (void*)VTable_SetD3DRenderer,       // [2] 0x0044a0d0
    (void*)VTable_Clear,                // [3] 0x0044b320
    (void*)VTable_Present,              // [4] 0x00448ff0
    (void*)VTable_HandleWindowMessage,  // [5] 0x00448b60
    (void*)VTable_CreateTextureHandle,  // [6] 0x0044c900
    (void*)VTable_CreateObjectHandle,   // [7] 0x0044af90
    (void*)VTable_DeleteTextureHandle,  // [8] 0x0044b220
    (void*)VTable_DeleteObjectHandle,   // [9] 0x0044b1c0
    (void*)VTable_SetTexture,           // [10] 0x00448300
    (void*)VTable_ResetTextures,        // [11] 0x00448380
};

// ============================================================================
// Constructor / Destructor
// Original C'tor: 0x0044baf0
// ============================================================================
static CMarniDirect3D* MarniDirect3D_Construct(CMarniDirect3D* pThis,
    HWND hWnd, int width, int height, int modeID, int adapterID)
{
    if (!pThis) return NULL;

    // Set vtable
    pThis->vtable = g_CMarniDirect3D_VTable;

    // Basic fields (matching original offsets)
    // Original base ctor (0x0044efc0): field_0x8/0xc (logical resolution) are
    // initialized to the same width/height as the physical surface (0x10/0x14);
    // SetVideoResolution later toggles ONLY the logical pair.
    pThis->m_hWnd         = (DWORD)hWnd;
    pThis->m_logicalWidth = (DWORD)width;
    pThis->m_logicalHeight= (DWORD)height;
    pThis->m_width        = (DWORD)width;
    pThis->m_height       = (DWORD)height;
    pThis->m_bitDepth     = (g_dwBitDepth == 16) ? 16 : 32;
    pThis->m_isInitialized = FALSE;
    pThis->m_isFullScreen  = g_bFullScreen;
    pThis->m_isActive      = TRUE;
    pThis->m_selectedMode  = (DWORD)modeID;
    pThis->m_deviceType    = (DWORD)adapterID;
    pThis->m_currentMode   = (DWORD)modeID;
    pThis->m_scratch       = 0;

    // Font fields
    pThis->m_FontTexHandle = MARNI_NULL_HANDLE;
    pThis->m_FontTexWidth  = 0;
    pThis->m_FontTexHeight = 0;

    // Clamp
    if (pThis->m_width  < 320) pThis->m_width  = 640;
    if (pThis->m_height < 240) pThis->m_height = 480;

    // Delegate D3D11 creation to MarniDX
    OutputDebugStringA("[Marni] Creating D3D11 device...\n");
    pThis->m_pDX = MarniDX_Create();
    if (pThis->m_pDX) {
        int actualW, actualH;
        if (pThis->m_pDX->Create(hWnd, (int)pThis->m_width,
                                 (int)pThis->m_height,
                                 pThis->m_isFullScreen,
                                 &actualW, &actualH)) {
            pThis->m_width  = (DWORD)actualW;
            pThis->m_height = (DWORD)actualH;
            pThis->m_isInitialized = TRUE;
            OutputDebugStringA("[Marni] D3D11 device created OK\n");
        } else {
            OutputDebugStringA("[Marni] D3D11 device creation FAILED\n");
        }
    } else {
        OutputDebugStringA("[Marni] MarniDX_Create failed\n");
    }

    // Pre-validate the framebuffer proxy surface so SaveBitmapToFile will
    // capture the D3D11 backbuffer when called on it (original: the
    // framebuffer CMarniBits at g_pMarniDirect3D + 0x2064 was always
    // populated by software rendering).
    g_MarniFrameBuffer.m_isValid = 1;

    return pThis;
}

// ============================================================================
// Memory operators (unchanged from original: 0x00433370, 0x004333e0)
// ============================================================================
void* operator_new(size_t size)
{
    return malloc(size);
}

void operator_delete(void* ptr)
{
    if (ptr) free(ptr);
}

// ============================================================================
// CMarniDirect3D_Constructor — C-style constructor wrapper (0x0044baf0)
// ============================================================================
void* CMarniDirect3D_Constructor(void* self, HWND hWnd, int width, int height,
                                  int modeID, int adapterID)
{
    return MarniDirect3D_Construct((CMarniDirect3D*)self, hWnd,
                                    width, height, modeID, adapterID);
}

// ============================================================================
// VTable function implementations — all forward to MarniDX.
// ============================================================================

// [0] RequestVideoMemory — 0x00448630
static int VTable_RequestVideoMemory(void* self)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;
    if (!pD3D || !pD3D->m_isInitialized) return 0;
    return (int)pD3D->m_pDX->QueryVideoMemory(pD3D->m_deviceType == 5);
}

// [1] ChangeDisplayMode — 0x00449300
static int VTable_ChangeDisplayMode(void* self, int mode)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;
    if (!pD3D || !pD3D->m_isInitialized) return 0;
    if (mode < 0 || mode >= g_NumDisplayModes) return 0;

    DisplayModeInfo* pMode = &g_DisplayModeBuffer[mode];
    int result = pD3D->m_pDX->ChangeDisplayMode(
        pMode->dwWidth, pMode->dwHeight, (pMode->dwFlags & 1) != 0);
    if (result) {
        pD3D->m_width      = pMode->dwWidth;
        pD3D->m_height     = pMode->dwHeight;
        pD3D->m_currentMode = (DWORD)mode;
        pD3D->m_isFullScreen = (pMode->dwFlags & 1) != 0;
        g_dwScreenWidth     = pMode->dwWidth;
        g_dwScreenHeight    = pMode->dwHeight;
    }
    return result;
}

// [2] SetD3DRenderer — 0x0044a0d0 (no-op in modern D3D11)
static void VTable_SetD3DRenderer(void* self, int renderer)
{
    (void)self; (void)renderer;
}

// [3] Clear — 0x0044b320
static int VTable_Clear(void* self)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;
    if (!pD3D || !pD3D->m_isInitialized) return 0;
    if (!pD3D->m_isActive) return 0;

    float r, g, b;
    if (g_debugClearR != 0.0f || g_debugClearG != 0.0f || g_debugClearB != 0.0f) {
        r = g_debugClearR / 255.0f;
        g = g_debugClearG / 255.0f;
        b = g_debugClearB / 255.0f;
    } else {
        r = 0.0f; g = 0.0f; b = 0.05f; // slight blue tint
    }
    pD3D->m_pDX->Clear(r, g, b, 1.0f);
    return 1;
}

// [4] Present — 0x00448ff0
static int VTable_Present(void* self)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;
    if (!pD3D || !pD3D->m_isInitialized) return 1;
    pD3D->m_pDX->Present();
    return 1;
}

// [5] HandleWindowMessage — 0x00448b60
static int VTable_HandleWindowMessage(void* self, HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;

    switch (msg) {
    case WM_ACTIVATE:
        pD3D->m_isActive = (LOWORD(wParam) != WA_INACTIVE);
        return 1;
    case WM_SIZE:
    case WM_DESTROY:
        if (msg == WM_DESTROY) pD3D->m_isActive = FALSE;
        // Forward resize to MarniDX; it will also update pD3D->m_width/height
        // through GetBackBufferSize, but the WM_SIZE handler in MarniDX does
        // its own resize — read back dims afterwards.
        pD3D->m_pDX->HandleWindowMessage(hwnd, msg, wParam, lParam);
        if (msg == WM_SIZE && LOWORD(lParam) > 0) {
            DWORD nw = 0, nh = 0;
            pD3D->m_pDX->GetBackBufferSize(&nw, &nh);
            if (nw > 0)  pD3D->m_width  = nw;
            if (nh > 0)  pD3D->m_height = nh;
        }
        return 1;
    default:
        return 1;
    }
}

// ============================================================================
// STP knock-out twins — per-texel semi-transparency for TMD models.
//
// The PS1 GPU blends PER TEXEL: on a polygon with ABE set, a texel whose
// palette entry carries the STP bit blends with the framebuffer, and a texel
// whose entry does not carry it draws opaque. The DC/PS1 room models rely on
// this - room 107's display case is 80 ABE-flagged primitives over a palette
// where only entries 63 and 66 are STP, so its glass top blends and its wooden
// frame stays solid. This build has no per-texel mode, and it cannot use the
// item viewer's trick of knocking the STP texels out of the page itself
// (FUN_004842c0 / ItemStpPage_KnockOutStpTexels, MainMenu.cpp): a model page is
// shared, and every other primitive drawn from it wants those texels intact.
//
// So the knock-out becomes a SECOND TEXTURE rather than a second page. For a
// material whose palette marks some - but not most - of its used texels STP,
// [6] CreateTextureHandle also builds a copy of the texture holding only the
// opaque texels, every STP texel forced to palette index 0 - which the
// conversion below already maps to alpha 0, exactly as the item page's knock-out
// relies on. Both bounds matter:
//
//   no STP texels at all      nothing to knock out, and nothing to blend;
//   ALL used texels STP       the whole primitive blends uniformly, which the
//                             record's blend weight (+0x68) already reproduces
//                             and whose no-depth-write it depends on. The twin
//                             would be an EMPTY copy that the opaque pass would
//                             then depth-write on behalf of a surface that had
//                             never written depth - the flooded-room water
//                             regression in reverse. (Scanned every shipped
//                             RDT: the all-STP models are ROOM10D0/60D0/80D0/
//                             D0D0 omodel[2] and ROOM7150 item[2], 32761 of
//                             32768 texels STP, plus ROOM2050/7050 omodel[0] at
//                             60%.)
//
// With both bounds the only model in the game that gains a pass is room 107's
// display case and its counterparts in rooms 108/607/608/807/D07 - and the
// aquarium shell in room 20A0, which already draws unblended through
// g_renderStateTMD (FUN_00484c40's punched page), so its pixels do not move
// either.
//
// FlushTmdObjects then draws a semi-transparent TMD object twice - the whole
// model through the texture at its blend weight, then the same geometry through
// this twin at full alpha. The opaque pass covers the blended one everywhere
// the texture is opaque, so the blend survives only where the PS1 would have
// blended: the arrangement FUN_004844c0 uses for the examine screen.
//
// Registration is by public handle, so a twin is destroyed with its source
// texture in vtable[8]. The table is a heap array rather than .bss for the same
// reason as g_tmdLight in TmdRenderer.cpp: a static this size shifts every
// global that follows it in the link.
// ============================================================================
#define MARNI_HANDLE_MAX 2048            // == MarniDX.cpp's MARNI_MAX_TEXTURES
static unsigned int* s_stpKnock = NULL;  // [MARNI_HANDLE_MAX], lazily allocated

static void MarniStpKnock_Set(unsigned int src, unsigned int twin)
{
    if (src == 0 || src >= MARNI_HANDLE_MAX) return;
    if (s_stpKnock == NULL) {
        s_stpKnock = (unsigned int*)calloc(MARNI_HANDLE_MAX, sizeof(unsigned int));
        if (s_stpKnock == NULL) return;
    }
    s_stpKnock[src] = twin;
}

// Detach the twin of `src` and return it (0 if none). The caller destroys it:
// this file has no back-pointer from a handle to the MarniDX that owns it.
static unsigned int MarniStpKnock_Take(unsigned int src)
{
    if (s_stpKnock == NULL || src == 0 || src >= MARNI_HANDLE_MAX) return 0;
    unsigned int twin = s_stpKnock[src];
    s_stpKnock[src] = 0;
    return twin;
}

MarniHandle MarniStpKnockoutTwin(MarniHandle tex)
{
    if (s_stpKnock == NULL) return MARNI_NULL_HANDLE;
    if ((unsigned int)tex == 0 || (unsigned int)tex >= MARNI_HANDLE_MAX)
        return MARNI_NULL_HANDLE;
    return (MarniHandle)s_stpKnock[(unsigned int)tex];
}

// [6] CreateTextureHandle — 0x0044c900
// Converts a CMarniBits surface (PSX VRAM-format pixel data + RGB555 CLUT)
// into a D3D11 texture and returns its MarniHandle (>= 1), exactly how the
// original returned a texture slot index. Writes 1 to outHandle on success.
static int VTable_CreateTextureHandle(void* self, void* texDesc,
                                       unsigned int flags, void* outHandle)
{
    (void)flags;
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;
    CMarniBits* bits = (CMarniBits*)texDesc;
    if (!pD3D || !pD3D->m_pDX || !bits) return 0;

    int w = (int)bits->m_width;
    int h = (int)bits->m_height;
    int bpp = (int)bits->m_bitDepth;
    const WORD* pixels = (const WORD*)bits->m_pPixelData;
    const WORD* clut = (const WORD*)bits->m_pPalette;
    if (w <= 0 || h <= 0 || !pixels) return 0;

    // Row stride comes from the descriptor, exactly as CMarniBits::CalcAddress
    // (0x00403860) computes it: base + m_pitch * y + (x/2 | x | x*2) by depth.
    // PSXTexture::Store sets m_pitch = imgW*2 and m_width = imgW*4 (4bpp), so a
    // packed w*bpp/8 stride happens to agree - but only for well-formed TIMs.
    // Trust m_pitch, falling back to the packed stride when it is unset.
    const BYTE* base = (const BYTE*)pixels;
    int pitch = (int)bits->m_pitch;
    if (pitch <= 0) pitch = (bpp == 4) ? (w / 2) : (bpp == 8) ? w : (w * 2);

    // The dimensions come straight out of the TIM header, and m_pPixelData
    // aliases the loaded file buffer rather than a sized allocation, so a
    // truncated or mis-parsed image walks off the end of committed memory.
    // Ask the OS how much is actually readable and clamp instead of faulting.
    {
        int lastByteInRow = (bpp == 4) ? ((w - 1) / 2)
                          : (bpp == 8) ? (w - 1)
                                       : ((w - 1) * 2 + 1);
        SIZE_T need = (SIZE_T)pitch * (h - 1) + lastByteInRow + 1;
        SIZE_T avail = plat_readable_bytes(base);
        if (avail < need) {
            char dbg[224];
            sprintf_s(dbg, sizeof(dbg),
                      "[TEXPAGE] TRUNCATED: w=%d h=%d bpp=%d pitch=%d base=%p "
                      "clut=%p need=%zu avail=%zu\n",
                      w, h, bpp, pitch, (const void*)base, (const void*)clut,
                      need, avail);
            OutputDebugStringA(dbg);
            // Keep only the rows that are fully readable.
            int safeRows = (avail >= (SIZE_T)lastByteInRow + 1)
                         ? (int)((avail - lastByteInRow - 1) / (SIZE_T)pitch) + 1
                         : 0;
            if (safeRows <= 0) return 0;
            if (safeRows < h) h = safeRows;
        }
    }

    // The palette pointer needs the same treatment as the pixel base, and never
    // had it. m_pPalette is whatever PSXTexture::Store parsed out of the TIM /
    // whatever CopyFrom carried into g_MarniBitsWorkBuffer; the loops below
    // index it by a full pixel byte (0-255 at 8bpp, 0-15 at 4bpp) with no
    // check, so one mis-parsed descriptor faults on `clut[idx]` deep inside the
    // async task instead of failing the page. Verify the whole CLUT is readable
    // and drop to the untextured path if it is not.
    if (clut != NULL && (bpp == 4 || bpp == 8)) {
        SIZE_T need = (bpp == 4) ? 16 * sizeof(WORD) : 256 * sizeof(WORD);
        SIZE_T avail = plat_readable_bytes(clut);
        if (avail < need) {
            char dbg[224];
            sprintf_s(dbg, sizeof(dbg),
                      "[TEXPAGE] BAD CLUT: w=%d h=%d bpp=%d pitch=%d base=%p "
                      "clut=%p need=%zu avail=%zu src=%p\n",
                      w, h, bpp, pitch, (const void*)base, (const void*)clut,
                      need, avail, g_texturePageSrcDesc);
            OutputDebugStringA(dbg);
            return 0;
        }
    }

    // --- STP accounting for the knock-out twin (see the note above) ---
    // Index-only. The colour conversion below is left alone and the twin is
    // then built from the finished RGBA by clearing the alpha of the texels
    // this marks, so the two can never disagree about a texel's colour.
    const bool clutKeyed = (clut != NULL) && (bpp == 4 || bpp == 8);
    int  stpTexels = 0, opaqueTexels = 0;
    BYTE stpIndex[256];
    memset(stpIndex, 0, sizeof(stpIndex));
    if (clutKeyed) {
        const int entries = (bpp == 4) ? 16 : 256;
        for (int i = 0; i < entries; i++)
            stpIndex[i] = (clut[i] & 0x8000) ? 1 : 0;
        for (int y = 0; y < h; y++) {
            const BYTE* row = base + (size_t)y * pitch;
            if (bpp == 8) {
                for (int x = 0; x < w; x++) {
                    int idx = row[x];
                    if (stpIndex[idx])   stpTexels++;
                    else if (idx != 0)   opaqueTexels++;
                }
            }
            else {
                for (int x = 0; x < w; x += 2) {
                    int lo = row[x >> 1] & 0x0F;
                    int hi = (row[x >> 1] >> 4) & 0x0F;
                    if (stpIndex[lo]) stpTexels++; else if (lo != 0) opaqueTexels++;
                    if (x + 1 < w) {
                        if (stpIndex[hi]) stpTexels++; else if (hi != 0) opaqueTexels++;
                    }
                }
            }
        }
    }

    // Output is uploaded as DXGI_FORMAT_R8G8B8A8_UNORM, so each DWORD must be
    // 0xAABBGGRR - red in the lowest byte. PS1 15-bit source colour is
    // MBBBBBGGGGGRRRRR, i.e. red in bits 0-4. Packing 0xAARRGGBB here (the
    // Win32 ARGB habit) swapped red and blue on every model texture.
    DWORD* rgba = (DWORD*)operator_new((size_t)w * h * sizeof(DWORD));
    if (!rgba) return 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            DWORD color = 0xFF000000; // opaque black default
            if (bpp == 4 && clut) {
                BYTE twoPix = base[y * pitch + x / 2];
                int nib = (x & 1) ? (twoPix >> 4) : (twoPix & 0xF);
                WORD c = clut[nib];
                DWORD a = (nib == 0) ? 0x00 : 0xFF;
                DWORD r = (c & 0x1F) * 255 / 31;
                DWORD g = ((c >> 5) & 0x1F) * 255 / 31;
                DWORD b = ((c >> 10) & 0x1F) * 255 / 31;
                color = (a << 24) | (b << 16) | (g << 8) | r;
            }
            else if (bpp == 8 && clut) {
                int idx = base[y * pitch + x];
                WORD c = clut[idx];
                DWORD a = (idx == 0) ? 0x00 : 0xFF;
                DWORD r = (c & 0x1F) * 255 / 31;
                DWORD g = ((c >> 5) & 0x1F) * 255 / 31;
                DWORD b = ((c >> 10) & 0x1F) * 255 / 31;
                color = (a << 24) | (b << 16) | (g << 8) | r;
            }
            else if (bpp == 16) {
                WORD c = *(const WORD*)(base + y * pitch + x * 2);
                DWORD a = (c == 0) ? 0x00 : 0xFF;
                DWORD r = (c & 0x1F) * 255 / 31;
                DWORD g = ((c >> 5) & 0x1F) * 255 / 31;
                DWORD b = ((c >> 10) & 0x1F) * 255 / 31;
                color = (a << 24) | (b << 16) | (g << 8) | r;
            }
            rgba[y * w + x] = color;
        }
    }

    MarniHandle tex = pD3D->m_pDX->CreateTexture(w, h, 32, rgba, NULL, NULL);
    if (tex == MARNI_NULL_HANDLE) { operator_delete(rgba); return 0; }

    // Knock-out twin - the same image with every STP texel made fully
    // transparent, i.e. holding only the opaque texels. Built from the finished
    // RGBA so the colours are identical by construction; only the alpha moves.
    // Built only when the knock-out still carries MOST of the material, which
    // is the glass-pane shape this emulates - see the note above.
    if (clutKeyed && stpTexels > 0 && opaqueTexels > stpTexels) {
        DWORD* knock = (DWORD*)operator_new((size_t)w * h * sizeof(DWORD));
        if (knock != NULL) {
            for (int y = 0; y < h; y++) {
                const BYTE* row = base + (size_t)y * pitch;
                for (int x = 0; x < w; x++) {
                    int idx;
                    if (bpp == 8)   idx = row[x];
                    else if (x & 1) idx = (row[x >> 1] >> 4) & 0x0F;
                    else            idx = row[x >> 1] & 0x0F;
                    DWORD c = rgba[y * w + x];
                    if (stpIndex[idx]) c &= 0x00FFFFFFu;   // alpha 0, colour kept
                    knock[y * w + x] = c;
                }
            }
            MarniHandle twin = pD3D->m_pDX->CreateTexture(w, h, 32, knock, NULL, NULL);
            operator_delete(knock);
            if (twin != MARNI_NULL_HANDLE)
                MarniStpKnock_Set((unsigned int)tex, (unsigned int)twin);
        }
    }

    operator_delete(rgba);
    if (outHandle) *(unsigned int*)outHandle = 1;
    return (int)tex;
}

// [7] CreateObjectHandle — 0x0044af90
// The DX5 original built an execute buffer per 3D object and returned a slot
// index. The DX11 port draws straight from the CMarniViewport2 vertex/index
// buffers, so the handle is only an opaque "created ok" token — return a
// unique non-zero id.
static unsigned int VTable_CreateObjectHandle(void* self, void* objDesc,
                                               unsigned char flags)
{
    (void)self; (void)objDesc; (void)flags;
    static unsigned int s_nextObjectHandle = 1;
    unsigned int h = s_nextObjectHandle++;
    if (s_nextObjectHandle == 0) s_nextObjectHandle = 1;
    return h;
}

// [8] DeleteTextureHandle — 0x0044b220
static int VTable_DeleteTextureHandle(void* self, int handle)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)self;
    if (!pD3D || !pD3D->m_pDX) return 1;
    if (handle > 0) {
        // The knock-out twin is not reachable from its source texture, so it
        // has to die with it here. Without this its slot leaks AND a later
        // texture allocated the same handle would inherit the stale twin.
        unsigned int twin = MarniStpKnock_Take((unsigned int)handle);
        if (twin != 0) pD3D->m_pDX->DestroyTexture((MarniHandle)twin);
        pD3D->m_pDX->DestroyTexture((MarniHandle)handle);
    }
    return 1;
}

// [9] DeleteObjectHandle — 0x0044b1c0
static int VTable_DeleteObjectHandle(void* self, int handle)
{
    (void)self; (void)handle;
    return 1;
}

// [10] SetTexture — 0x00448300
// Despite the legacy name, in the original this is the TMD draw-queue entry
// point: CMarniDirect3DTMD::Transform calls it per object as
// vtable[10](objData, depth) and it inserted the object into the ordering
// table. The DX11 port queues it into the per-frame TMD draw list.
static int VTable_SetTexture(void* self, void* texData, unsigned int param)
{
    (void)self;
    TmdQueueObject(texData, (int)param);
    return 1;
}

// [11] ResetTextures — 0x00448380
static int VTable_ResetTextures(void* self)
{
    (void)self;
    return 1;
}

// ============================================================================
// Global Marni System functions
// ============================================================================

// IsGraphicsSystemReadyForOperation — 0x00497060
BOOL IsGraphicsSystemReadyForOperation(void)
{
    if (g_pMarniDirect3D) {
        CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
        if (pD3D->m_isInitialized) return TRUE;
        char dbg[128];
        sprintf(dbg, "[Marni] Graphics system: ptr=%p vt=%p init=%d\n",
                (void*)g_pMarniDirect3D, (void*)pD3D->vtable, pD3D->m_isInitialized);
        OutputDebugStringA(dbg);
        OutputDebugStringA("[Marni] Graphics system not initialized\n");
        return FALSE;
    }
    if (g_hWnd == NULL) return TRUE;
    OutputDebugStringA("[Marni] Graphics system unavailable (fallback)\n");
    return FALSE;
}

// InitializeMarniSystem — 0x004970c0
void InitializeMarniSystem(void)
{
    if (g_dwScreenWidth  < 320)  g_dwScreenWidth  = 640;
    if (g_dwScreenHeight < 240)  g_dwScreenHeight = 480;
    if ((int)g_dwSelectedDisplayModeID < 0) g_dwSelectedDisplayModeID = 0;

    void* pMem = operator_new(0x21DC);
    if (!pMem) {
        OutputDebugStringA("[Marni] Failed to allocate CMarniDirect3D\n");
        g_pMarniDirect3D = NULL;
        return;
    }

    g_pMarniDirect3D = CMarniDirect3D_Constructor(
        pMem, g_hWnd, g_dwScreenWidth, g_dwScreenHeight,
        g_dwSelectedDisplayModeID, g_dwSelectedDisplayAdapterID);

    if (!IsGraphicsSystemReadyForOperation()) {
        ShowMessageBox(NULL,
            "Failed to initialize the Graphics System",
            "RESIDENT EVIL", MB_OK | MB_ICONSTOP);
        CleanupVideoConfigAndSaveAllSettings();
        plat_window_destroy(g_hWnd);
        return;
    }

    InitJoysticks();
    // Capability flag only. g_isSideWinderConnected is the one-shot START
    // injection main_loop consumes and must NOT be raised here - doing so pops
    // the inventory open on the first frame after a pad is detected.
    g_bPadConnected = MarniPadIsConnected();
    CreateLights(3);

    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D && pD3D->m_isFullScreen) {
        plat_cursor_show(FALSE);
        g_isGameCursorHiddenFlag = FALSE;
    }

    g_GameInitTime = plat_time_ms();
}

// EnumerateDisplayModes — 0x004976c0
// Now delegates directly to MarniDX (DXGI stays inside marni/).
void EnumerateDisplayModes(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized) return;

    pD3D->m_pDX->EnumerateDisplayModes(g_DisplayModeBuffer,
        MAX_DISPLAY_MODES, &g_NumDisplayModes);
}

// EnumerateD3DRenderers — 0x004977f0
void EnumerateD3DRenderers(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D) return;
    pD3D->m_pDX->EnumerateAdapters(g_D3DRenderers, 8,
        &g_NumD3DRenderersAvailable);
}

// GetDirect3DDriverCount — 0x004486e0
// Original: __fastcall on the CMarniDirect3D object; guards on +0x3C
// (m_isInitialized) and prints "Direct3D::RequestDriverCount" when the
// renderer was never initialized, otherwise returns the adapter count
// filled in by EnumerateD3DRenderers (0x004977f0).
int GetDirect3DDriverCount(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized) {
        printf("Direct3D::RequestDriverCount\n");
        return 0;
    }
    return g_NumD3DRenderersAvailable;
}

// GetDirect3DDriverName — 0x00448710
// Original: __thiscall; same m_isInitialized guard ("Direct3D::RequestDriverName"),
// then bounds-checks index > 4 (the original driver table holds 5 entries of
// 0x11C bytes at 0x007e0e10, name string first) and returns a pointer to that
// entry's name. Port keeps the exact >4 check; g_D3DRenderers holds the names.
const char* GetDirect3DDriverName(int index)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized || index > 4) {
        printf("Direct3D::RequestDriverName\n");
        return NULL;
    }
    return g_D3DRenderers[index].name;
}

// InitJoysticks — 0x00420770
// Enumerates the WinMM devices and brings up the XInput backend. This used to
// be a stand-in that only probed XInput slot 0 and printed the result, which
// left CMarniDirectInput::InitJoysticks (the real implementation) with no
// caller at all - so joystickCount stayed 0, the per-frame poll loop never
// ran, and no pad input could reach ReadPadBoth or read_sidewinder_pad.
void InitJoysticks(void)
{
    CMarniDirectInput::InitJoysticks(&g_pMasterInputState);
}

// IsSideWinderPadConnected — 0x0040b610
int IsSideWinderPadConnected(void)
{
    return 1; // No legacy SideWinder on modern PC
}

// CreateLights — 0x00448440
void CreateLights(int numLights)
{
    (void)numLights;
}

// ============================================================================
// Drawing Functions
// ============================================================================

void MarniPresent(void)   { VTable_Present(g_pMarniDirect3D); }
void MarniClear(void)     { VTable_Clear(g_pMarniDirect3D); }
void PresentFrame(void)   { MarniPresent(); }
// Original ClearScreen (0x004298c0) ends with clearscreen_present_tail
// (0x00497640): vtable[3] Clear, FUN_0040a8f0 background quad insert,
// vtable[4] Present, then state = 3. The port's MarniClear + the governor's
// background-quad path cover that tail; this alias is the entry point.
void ClearScreen(void)    { MarniClear(); }

void* MarniGetDevice(void)
{
    return g_pMarniDirect3D;
}

void MarniDrawRect(int x, int y, int w, int h, DWORD color)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized) return;
    pD3D->m_pDX->DrawRect(x, y, w, h, color);
}

void MarniDrawLine(float x0, float y0, float x1, float y1,
                   float thickness, DWORD color)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized) return;
    pD3D->m_pDX->DrawLine(x0, y0, x1, y1, thickness, color);
}

void MarniDrawSprite(float x, float y, float w, float h,
                     float u0, float v0, float u1, float v1,
                     DWORD color, MarniHandle tex)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized) return;
    pD3D->m_pDX->DrawSprite(x, y, w, h, u0, v0, u1, v1,
                             color, tex, MARNI_SAMPLER_POINT,
                             MARNI_BLEND_ALPHA);
}

// Only the ground shadow / blood pool (AddFadePoly, type 12) draws through
// here, and it stretches a 26x29 gradient over a quad several times that size.
// Point sampling turned the soft blob into visible texel blocks with straight
// faceted edges; the original's D3D7 device filtered it, which is what makes
// the shadow in the retail game a smooth oval. Everything else in the sprite
// path stays POINT on purpose - that is the PS1 look for the 2D art.
void MarniDrawTrianglesPersp(const float* verts, int triCount, MarniHandle tex,
                              BOOL depthTest)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized) return;
    pD3D->m_pDX->DrawTrianglesPersp(verts, triCount, tex,
                                    MARNI_SAMPLER_LINEAR, MARNI_BLEND_ALPHA,
                                    depthTest != FALSE);
}

// ============================================================================
// MarniGetRenderScale
// Game-space -> backbuffer scale factors. The original Marni layer applied
// this at draw time as physical/logical (FUN_0042ba60 built the transform
// matrix with (field_0x10 / field_0x8) and (field_0x14 / field_0xc)):
//   physical = real surface dims (field_0x10/0x14 = our m_width/m_height)
//   logical  = render resolution set by SetVideoResolution (field_0x8/0xc)
// With a 640x480 surface and the game's logical 320x240, everything is
// scaled x2 at draw time — filling the window exactly like the original.
// The decomp applies the same ratio at sprite-queue time instead.
// ============================================================================
void MarniGetRenderScale(float* outScaleX, float* outScaleY)
{
    DWORD bw = 0, bh = 0;
    DWORD lw = 320, lh = 240;
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D) {
        if (pD3D->m_pDX) pD3D->m_pDX->GetBackBufferSize(&bw, &bh);
        if (pD3D->m_logicalWidth  >= 320) lw = pD3D->m_logicalWidth;
        if (pD3D->m_logicalHeight >= 240) lh = pD3D->m_logicalHeight;
    }
    if (bw < 320) bw = 320;
    if (bh < 240) bh = 240;
    if (outScaleX) *outScaleX = (float)bw / (float)lw;
    if (outScaleY) *outScaleY = (float)bh / (float)lh;
}

BOOL MarniCreateTexture(int width, int height, int bpp, const void* pixelData,
                        MarniHandle* outTex)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (!pD3D || !pD3D->m_isInitialized || !outTex) return FALSE;

    *outTex = pD3D->m_pDX->CreateTexture(width, height, bpp, pixelData,
                                         NULL, NULL);
    return (*outTex != MARNI_NULL_HANDLE);
}
