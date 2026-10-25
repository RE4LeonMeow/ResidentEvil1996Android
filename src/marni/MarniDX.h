// MarniDX.h - Direct3D shim layer above the original DX5/DX5-era Marni surface.
//
// This is the Windows/D3D11 implementation of the graphics backend that the
// original game spoke to through the CMarniDirect3D 12-entry vtable. Per the
// project's porting rules (AGENTS.md), DirectX 5.0 (DirectDraw/Direct3D) does
// not operate on current Windows, so the entire D3D11 pipeline is hidden
// behind this single owner class. Callers see only opaque 32-bit texture
// handles (MarniHandle) and a small set of high-level drawing entry points;
// <d3d11.h> / ID3D11* / DXGI types NEVER leak out of the marni/ layer.
//
// Public header — MUST NOT include d3d11.h, dxgi.h, d3dcompiler.h, xaudio2.h,
// or xinput.h. Only <windows.h> + MarniBits.h are permitted.
#pragma once

#include "../platform/types.h"
#include "MarniBits.h"
#ifdef __EMSCRIPTEN__
#include "MarniRenderKey.h"
#endif

// ============================================================================
// MarniHandle - Opaque texture handle.
//
// Replaces raw ID3D11ShaderResourceView* / ID3D11Texture2D* pointers that
// previously leaked into the game layer (g_TexturePageSRV[], g_displayImageSRV,
// print/draw sprite sinks). Handles are 1-based: 0 == MARNI_NULL_HANDLE (no
// texture / sentinel). Mapping to the underlying D3D11 objects lives entirely
// inside MarniDX.cpp; callers must only ever use the accessors below.
// ============================================================================
typedef DWORD32 MarniHandle;
#define MARNI_NULL_HANDLE  ((MarniHandle)0)

// Texture filtering applied at sprite draw time.
typedef enum MarniSampler : DWORD32 {
    MARNI_SAMPLER_LINEAR = 0,   // smooth bilinear (legacy default for fonts/UI)
    MARNI_SAMPLER_POINT  = 1,   // nearest-neighbour, matches original PSX pixel art
} MarniSampler;

// Per-sprite blend mode requested by callers (AddTintSprite/AddFadePoly/etc.).
typedef enum MarniBlend : DWORD32 {
    MARNI_BLEND_ALPHA    = 0,   // src*srcA + dst*(1-srcA) — standard alpha blend
    MARNI_BLEND_ADD      = 1,   // src + dst (light/flash effects)
    MARNI_BLEND_DISABLE = 2,   // no blending — straight overwrite
} MarniBlend;

// ============================================================================
// MarniDX - Singleton D3D11 backend owner.
//
// Lifetime: created by InitializeMarniSystem (the CMarniDirect3D constructor
// delegates here) and destroyed alongside CMarniDirect3D. A global accessor
// Marni_DX() returns the active instance (or NULL before init / after cleanup).
// All methods are safe to call when the device is NULL (they no-op), which
// preserves the original DX5 behavior of silently failing when uninitialised.
//
// Ownership of every D3D11 resource (device, context, swap chain, render
// target view, depth/stencil, shaders, vertex/constant buffers, sampler/
// blend/rasterizer/depth-stencil states, and the texture-handle table) lives
// exclusively in this class. Game code never sees a single COM pointer.
// ============================================================================
class MarniDX {
public:
    MarniDX();
    ~MarniDX();

    // ----------------------------------------------------------------------
    // Initialization / lifecycle
    // ----------------------------------------------------------------------

    // Create the D3D11 device + swap chain and all pipeline state.
    // Returns TRUE on success and fills outWidth/outHeight with the actual
    // back-buffer dimensions (clamped to >= 320x240).
    BOOL Create(HWND hWnd, int width, int height, BOOL fullScreen,
                int* outWidth, int* outHeight);

    // Release every owned D3D11 resource. Idempotent.
    void Destroy();

    // TRUE once Create() succeeded and the device is still valid.
    BOOL IsReady() const;

    // Returns the current back-buffer dimensions (0,0 if not ready).
    void GetBackBufferSize(DWORD* outWidth, DWORD* outHeight) const;

    // Apply a new display mode (resize the swap chain). 0 on failure.
    int  ChangeDisplayMode(DWORD newWidth, DWORD newHeight, BOOL fullScreen);

    // Handle a window message that the original vtable[5] forwarded here.
    // Returns 1 to continue default processing, 0 to suppress it.
    int  HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Report available dedicated video memory (bytes), or sentinel 0x75bcd15
    // for the software-render path. Returns 128MiB if queryable unavailable.
    unsigned int QueryVideoMemory(BOOL softwareRenderer) const;

    // Enumerate DXGI outputs into the global g_DisplayModeBuffer / count.
    // Implemented in MarniDX.cpp so DXGI stays out of the game layer.
    void EnumerateDisplayModes(struct DisplayModeInfo* outModes, int maxModes,
                              int* outCount);

    // Enumerate DXGI adapters into g_D3DRenderers (names + flags).
    void EnumerateAdapters(struct D3DRendererInfo* outRenderers, int max,
                          int* outCount);

    // ----------------------------------------------------------------------
    // Per-frame operations (forwarded from the CMarniDirect3D vtable)
    // ----------------------------------------------------------------------

    // Clear the render target (and depth/stencil if present) with the given
    // RGBA float color. (was vtable[3] Clear / 0x0044b320)
    void Clear(float r, float g, float b, float a);

    // Flip the back buffer to the front (was vtable[4] Present / 0x00448ff0).
    void Present();

    // ----------------------------------------------------------------------
    // Texture handle management
    // ----------------------------------------------------------------------

    // Create a 2D texture + SRV from raw host pixels. Same input semantics as
    // the original MarniCreateTexture: bpp may be 4/8/16/24/32. Returns an
    // opaque MarniHandle (>0) or MARNI_NULL_HANDLE on failure.
    MarniHandle CreateTexture(int width, int height, int bpp, const void* pixelData,
                              int* outWidth /*=NULL*/, int* outHeight /*=NULL*/);

    // Create/refresh a texture from an existing CMarniBits software surface
    // (used by the original "software framebuffer" -> present path). The bits'
    // host pixel data is uploaded each call.
    MarniHandle CreateTextureFromBits(CMarniBits* bits);

    // Replace the pixels of an existing handle (same width/height). Used by
    // the CharacterSelectionScreen card-mask compositing path which previously
    // did its own staging texture + Map/CopyResource on ID3D11 contexts.
    // Returns TRUE on success.
    BOOL UpdateTexturePixels(MarniHandle tex, const void* pixelData,
                             int width, int height, int bpp);

    // Return the dimensions of a live handle (fills *w/*h). No-op if invalid.
    void GetTextureSize(MarniHandle tex, int* outWidth, int* outHeight) const;

    // Release a texture handle and its D3D11 objects. Safe to pass
    // MARNI_NULL_HANDLE. Mirrors vtable[8] DeleteTextureHandle / 0x0044b220.
    void DestroyTexture(MarniHandle tex);

    // The single White 1x1 opaque fallback, created once at init. Used when a
    // caller wants a solid-color quad (rect) without supplying a texture.
    MarniHandle WhiteTexture() const;

    // ----------------------------------------------------------------------
    // Drawing
    // ----------------------------------------------------------------------

    // Draw a textured, tinted quad at screen coordinates (top-left origin,
    // pixels, Y-down). Was MarniDrawSprite. tex==MARNI_NULL_HANDLE -> white.
    void DrawSprite(float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1,
                    DWORD color, MarniHandle tex,
                    MarniSampler sampler = MARNI_SAMPLER_POINT,
                    MarniBlend  blend   = MARNI_BLEND_ALPHA);

    // Draw a solid-color rectangle. Was MarniDrawRect.
    void DrawRect(int x, int y, int w, int h, DWORD color);

    // Draw a solid-color line segment between two points (thickness in
    // pixels). Used by the in-game menu EKG health bar (FUN_00470c60).
    void DrawLine(float x0, float y0, float x1, float y1,
                  float thickness, DWORD color);

    // Draw a batch of textured triangles with per-vertex tint.
    // verts: triCount*3 vertices, each 8 floats in QuadVertex layout:
    //   { x, y (screen px, Y-down), u, v, r, g, b, a (0..1) }.
    // Used by the TMD 3D path (FlushTmdObjects) after CPU-side transform.
    // tex==MARNI_NULL_HANDLE -> white. Max 1024 triangles per call.
    void DrawTriangles(const float* verts, int triCount, MarniHandle tex,
                       MarniSampler sampler = MARNI_SAMPLER_POINT,
                       MarniBlend  blend   = MARNI_BLEND_ALPHA);

    // Perspective-correct textured triangles. verts use the Model3DVertex
    // layout { x, y (screen px), z (normalised [0,1] depth), w (view-space
    // Z), u, v, r, g, b, a }: the perspective VS multiplies the ortho-mapped
    // screen position by w and hands the rasteriser that w, so after the
    // divide the position is unchanged but UV/colour interpolate
    // perspective-correctly. Used by the ground-shadow quads, where
    // screen-space (affine) interpolation sheared the two halves oppositely.
    //
    // depthTest=false (default) disables the depth test outright. true clips
    // against the model geometry like the original's Z-buffered viewport
    // quads - test only, no write - and requires real NDC z in each vertex.
    void DrawTrianglesPersp(const float* verts, int triCount, MarniHandle tex,
                            MarniSampler sampler = MARNI_SAMPLER_POINT,
                            MarniBlend  blend   = MARNI_BLEND_ALPHA,
                            bool depthTest = false);

    // Depth-buffered variant of DrawTriangles for 3D models: 10 floats per
    // vertex, { x, y (screen px, Y-down), z (normalised [0,1] depth), w
    // (view-space Z), u, v, r, g, b, a } - same layout as DrawTrianglesPersp.
    // Tests and writes the depth buffer (LESS_EQUAL) when
    // depthWrite is set (default) so faces of the same model resolve correctly
    // whatever order they arrive in; with depthWrite=false it only TESTS, for
    // translucent geometry (water, glass) that must not hide the depth-tested
    // fade polys (ground shadows / blood pools) behind it. Either way the
    // depth-disabled state the 2D path assumes is restored afterwards. The
    // depth buffer is cleared once per frame by Clear(). Max 1024 triangles
    // per call.
    void DrawTriangles3D(const float* verts, int triCount, MarniHandle tex,
                         MarniSampler sampler = MARNI_SAMPLER_POINT,
                         MarniBlend  blend   = MARNI_BLEND_ALPHA,
                         bool depthWrite = true);
#ifdef __EMSCRIPTEN__
    bool WantsInterpolation() const;
    void DrawTriangles3DWithKeys(const float* verts, int triCount, MarniHandle tex,
                                MarniSampler sampler, MarniBlend blend,
                                bool depthWrite, const MarniTriangleKey* keys);
#endif

    // ----------------------------------------------------------------------
    // Backbuffer readback (used by CMarniBits::SaveBitmapToFile, the original
    // +0x2064 framebuffer-proxy path). Allocates a contiguous RGBA8 buffer via
    // operator_new and returns it in *outPixels + dimensions. Caller owns the
    // memory (free with operator_delete). Returns FALSE if unavailable.
    // ----------------------------------------------------------------------
    BOOL CaptureBackbufferToRGBA(void** outPixels, DWORD* outWidth, DWORD* outHeight);

    // ----------------------------------------------------------------------
    // CPU readback of a texture handle into a contiguous RGBA8 buffer
    // allocated with operator_new. Mirrors the old card/mask staging logic in
    // CharacterSelectionScreen.cpp. Returns FALSE if tex is invalid.
    // ----------------------------------------------------------------------
    BOOL ReadTextureRGBA(MarniHandle tex, void** outPixels,
                         int* outWidth, int* outHeight);

    // ----------------------------------------------------------------------
    // Font-texture API (was pD3D->m_pFontTexture / m_pFontSRV on CMarniDirect3D)
    // The font is special-cased: its handle/dims are cached here so callers
    // can fetch them without holding any D3D11 pointer.
    // ----------------------------------------------------------------------
    void SetFontTexture(MarniHandle tex, int width, int height);
    MarniHandle FontTexture() const;
    int  FontTextureWidth()  const;
    int  FontTextureHeight() const;

    // PImpl-style internals live in the .cpp; this class only exposes the
    // stable handle-based surface. All D3D11 state pointers are hidden inside
    // the impl to guarantee the public header stays D3D11-free.
    struct Impl;
    Impl* m_pImpl;
};

// Global accessor for the active backend. Returns NULL when uninitialised.
MarniDX* Marni_DX();

// Allocates and assigns the global MarniDX singleton (called by the
// CMarniDirect3D constructor). Returns the new instance or NULL on failure.
MarniDX* MarniDX_Create();

// Destroys and clears the global MarniDX singleton (called by CMarniDirect3D
// destructor / CleanupVideoConfigAndSaveAllSettings).
void MarniDX_DestroyGlobal();
