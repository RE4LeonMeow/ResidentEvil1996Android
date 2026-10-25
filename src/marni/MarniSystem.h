// MarniSystem.h - Marni System wrapper interface
//
// The original Marni System is Capcom's abstraction above DirectX 5
// (DirectDraw/Direct3D/DirectSound/DirectInput) that allowed PS1 game code
// to run on Windows with minimal changes. On current Windows the DX5 APIs
// are unavailable, so the whole backend is re-routed through a MarniDX
// D3D11 shim layer that keeps the same C++ class ABI (vtable at offset 0,
// field names/offsets unchanged) while hiding every ID3D11*/DXGI COM pointer
// behind opaque MarniHandle handles.
//
// Game-layer code must NOT include <d3d11.h> — only MarniSystem.h.
#pragma once

#include "../platform/types.h"
#include "MarniBits.h"
#include "MarniDX.h"

// ============================================================================
// CMarniDirect3D - Capcom's original D3D wrapper class (vtable at 0x004af230)
// Object size: 0x21DC (8676 bytes) — preserved via trailing padding.
//
// Field offsets MUST match the original binary exactly, because game-layer
// code reads them by offset: WindowProc.cpp walks the vtable by index,
// Rendering.cpp / PrintText.cpp / TmdAnimation.cpp / ObjectManager.cpp read
// m_width, m_height, m_isInitialized, m_isFullScreen, m_isActive,
// m_deviceType, m_currentMode, and m_scratch by name through the
// CMarniDirect3D* cast from g_pMarniDirect3D.
//
// All D3D11 ownership now lives inside the MarniDX* m_pDX member — there is
// not a single ID3D11*/DXGI type visible from this header.
// ============================================================================
class CMarniDirect3D {
public:
    // VTable pointer at offset 0x00
    void** vtable;

    // Known-access fields (must keep exact offsets and names)
    // ---
    DWORD  m_hWnd;                   // 0x04 — original: window handle (base ctor 0x0044efc0)
    DWORD  m_logicalWidth;           // 0x08 — LOGICAL render resolution; SetVideoResolution (0x00497f30) writes this
    DWORD  m_logicalHeight;          // 0x0C — LOGICAL render resolution; SetVideoResolution (0x00497f30) writes this
    DWORD  m_width;                  // 0x10 — PHYSICAL backbuffer width (read by PrintText/Rendering/VideoPlayback)
    DWORD  m_height;                 // 0x14 — PHYSICAL backbuffer height
    DWORD  m_bitDepth;                 // 0x18 — the original bit-depth setting

    // 0x1C - 0x3B: gap from original binary analysis (32 bytes)
    BYTE   m_pad1[0x20];
    BOOL   m_isInitialized;            // 0x3C — tested in many places; TRUE after Create succeeds

    // 0x40 - 0x67: gap (40 bytes)
    BYTE   m_pad2[0x28];
    BOOL   m_isFullScreen;             // 0x68 — read by InitializeMarniSystem / Cleanup
    BYTE   m_pad_6C_73[8];             // 0x6C..0x73 (unused)
    BOOL   m_isActive;                 // 0x74 — set/read by WindowProc (vtable[5] handler)
    DWORD  m_selectedMode;             // 0x78 — original display-mode index

    // 0x7C - 0x30B: gap (656 bytes — the original DirectDraw/D3D/DDsurface
    //                  COM pointer region lives inside this span)
    BYTE   m_pad3[0x290];

    DWORD  m_deviceType;               // 0x30C — renderer/adapter type (0-6, 5=software)
    // 0x310: implicit gap (4 bytes)
    DWORD  m_currentMode;              // 0x314 — active display-mode index after change
    BYTE   m_pad4[0x0C];               // 0x318..0x323 (filled from original binary offsets)
    DWORD  m_scratch;                  // 0x324 — scratch/state field (read in several paths)

    // -----------------------------------------------------------------------
    // The rest of the 0x21DC block is now just padding. All D3D11 state moved
    // to a heap-allocated MarniDX owned by m_pDX. The game layer only ever
    // accesses CMarniDirect3D through the fields above + the 12-entry vtable.
    // -----------------------------------------------------------------------

    // Single D3D11-backend owner (populated during construction and destroyed
    // alongside this object). Game code never dereferences this pointer
    // directly — it's for the MarniSystem.cpp vtable + wrapper functions.
    MarniDX* m_pDX;

    // Font texture (created during ProcessTextureImage / bank 0x1E).
    // Dimensions cached here so PrintText.cpp / Rendering.cpp can look them
    // up without including d3d11.h.
    MarniHandle m_FontTexHandle;       // 0-based handle from MarniDX
    int         m_FontTexWidth;        // pixels
    int         m_FontTexHeight;       // pixels

    // Ensure total instance size = 0x21DC (the `operator_new(0x21DC)`
    // allocation in InitializeMarniSystem). The leading members sum to
    // 0x334; 0x21DC - 0x334 = 0x1EA8 (7848 bytes). Verified by static_assert
    // in MarniSystem.cpp.
    BYTE   m_pad_endfix[0x1EA8];

    // ---- C'tor / d'tor (keep same signatures as before) ----------------
    CMarniDirect3D(HWND hWnd, int width, int height, int modeID, int adapterID);
    ~CMarniDirect3D();

    // ---- VTable (12 entries, indexed 0-11 — must stay in sync with
    // MarniSystem.cpp g_CMarniDirect3D_VTable) --------------------------
    // [0] RequestVideoMemory  (0x00448630)
    // [1] ChangeDisplayMode   (0x00449300)
    // [2] SetD3DRenderer      (0x0044a0d0)
    // [3] Clear               (0x0044b320)
    // [4] Present             (0x00448ff0)
    // [5] HandleWindowMessage (0x00448b60)
    // [6] CreateTextureHandle (0x0044c900)
    // [7] CreateObjectHandle  (0x0044af90)
    // [8] DeleteTextureHandle (0x0044b220)
    // [9] DeleteObjectHandle  (0x0044b1c0)
    // [10] SetTexture          (0x00448300)
    // [11] ResetTextures       (0x00448380)
};

// ============================================================================
// Global Marni system functions (unchanged ABI)
// ============================================================================

void* CMarniDirect3D_Constructor(void* self, HWND hWnd, int width,
                                  int height, int modeID, int adapterID);

BOOL  IsGraphicsSystemReadyForOperation(void);
void  InitializeMarniSystem(void);
void  EnumerateDisplayModes(void);
void  EnumerateD3DRenderers(void);
int   GetDirect3DDriverCount(void);           // 0x004486e0
const char* GetDirect3DDriverName(int index); // 0x00448710
void  InitJoysticks(void);
int   IsSideWinderPadConnected(void);
void  CreateLights(int numLights);
void  UpdateVideoPlayback(void);

void  MarniPresent(void);             // -> vtable[4] Present
void  MarniClear(void);               // -> vtable[3] Clear
void  MarniDrawRect(int x, int y, int w, int h, DWORD color);
void  MarniDrawLine(float x0, float y0, float x1, float y1,
                    float thickness, DWORD color);
void  MarniDrawSprite(float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1,
                      DWORD color, MarniHandle tex);

// Draw perspective-correct textured triangles. verts: triCount*3 vertices,
// each 10 floats { x, y (screen px, Y-down), z (NDC [0,1], used when
// depthTest), w (view-space Z), u, v, r, g, b, a (0..1) }. Used for the
// ground-shadow quads, whose clipped projected polygon an axis-aligned
// sprite cannot reproduce and whose screen-space (affine) interpolation
// sheared the two halves oppositely.
//
// depthTest: clip against the model geometry's depth buffer (test only, no
// write) - what the original's Z-buffered viewport quads did. Without it a
// shadow whose ordering-table key collapsed (the flag!=1 placement records
// force alpha = forceAlpha+1, a near-constant key) would paint over every
// model and mask in the scene.
void  MarniDrawTrianglesPersp(const float* verts, int triCount, MarniHandle tex,
                               BOOL depthTest = FALSE);

// Game-space (320x240) -> real D3D11 backbuffer scale factors. Use this for
// ALL game-space -> screen-space conversion; never derive the scale from
// CMarniDirect3D::m_width/m_height (SetVideoResolution stomps those with the
// logical video resolution while the swapchain keeps the window size).
void  MarniGetRenderScale(float* outScaleX, float* outScaleY);

// Create a texture from raw host pixels; returns an opaque MarniHandle.
// bpp may be 4, 8, 16, 24, or 32. On success the handle is written to
// *outTex (if non-NULL) and the function returns TRUE, otherwise FALSE.
BOOL  MarniCreateTexture(int width, int height, int bpp, const void* pixelData,
                         MarniHandle* outTex);

// STP knock-out twin of a model texture handle: a copy of the same texture
// holding ONLY its opaque texels (every texel whose palette entry carries the
// PS1 STP bit forced to palette index 0, i.e. fully transparent). Created by
// [6] CreateTextureHandle for a material whose palette marks some - but not
// all - of its used texels STP, and destroyed with its source texture by [8]
// DeleteTextureHandle.
//
// FlushTmdObjects draws a semi-transparent TMD object once through the texture
// and once through this twin at full alpha, which is how the per-texel rule
// the PS1 GPU applies (ABE texels blend, non-ABE texels stay opaque) is
// reproduced without per-texel blending. See the note above MarniStpKnockoutTwin's
// definition in MarniSystem.cpp and the two-pass note in TmdRenderer.cpp.
//
// Returns MARNI_NULL_HANDLE when the texture has no twin.
MarniHandle MarniStpKnockoutTwin(MarniHandle tex);

void* MarniGetDevice(void);
void  PresentFrame(void);             // alias for MarniPresent
void  ClearScreen(void);             // alias for MarniClear

void* operator_new(size_t size);
void  operator_delete(void* ptr);
