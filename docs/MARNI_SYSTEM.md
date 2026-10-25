# Marni System — Complete Implementation Reference

## Table of Contents

1. [Overview](#overview)
2. [CMarniDirect3D — Main Graphics Class](#1-cmarnidirect3d--main-graphics-class)
3. [CMarniBits — Surface/Bitmap Class](#2-cmarnibits--surfacebitmap-class)
4. [PSXTexture — PS1 TIM/PIX Texture Loader](#3-psxtexture--ps1-timpix-texture-loader)
5. [CDirect3DObject / CMarniExecuteBuffer — 3D Object Classes](#3a-cdirect3dobject--cmarniexecutebuffer--3d-object-classes)
6. [CMarniDirect3DTMD — TMD 3D Model Renderer](#3b-cmarnidirect3dtmd--tmd-3d-model-renderer)
7. [CMarniViewport2 — 3D Viewport Class](#3c-cmarniviewport2--3d-viewport-class)
8. [DirectInput — Marni Input System](#3d-directinput--marni-input-system)
9. [PSYQ GPU Emulation — Sprite Renderer](#4-psyq-gpu-emulation--sprite-renderer)
10. [Texture Page Management](#5-texture-page-management)
11. [Global Arrays (Static Initialization)](#6-global-arrays-static-initialization)
12. [ExecAsync — Async Task System](#7-execasync--async-task-system)
13. [Data Flow Diagram](#8-data-flow-diagram)
14. [Source File Map](#9-source-file-map)
15. [PSYQ-to-DirectX Mapping](#10-psyq-to-directx-mapping)

---

## Overview

The Marni System is Capcom's PSYQ-to-DirectX compatibility layer that allows PlayStation 1 game code to run on Windows PC with minimal changes. It wraps PS1 graphics, input, and audio APIs behind DirectX interfaces.

**Status: fully ported.** Every Marni System function of the original binary
is accounted for: the class methods and helpers are implemented in this layer,
and the raw DirectDraw/D3D5 device paths plus the software rasterizers were
replaced by the DX11 equivalents described here (tagged `d3d5-superseded` in
the Ghidra project; see `docs/ARCHITECTURE.md`, "Superseded original
subsystems").

| Aspect | Original (1997) | Modern Port |
|--------|-----------------|-------------|
| Graphics | DirectX 5.0 (DirectDraw, Direct3D 5) | Direct3D 11 (`src/marni/MarniDX.*`) |
| Audio | DirectSound | XAudio2 (`src/marni/MarniSound.*`) |
| Input | Custom `GetAsyncKeyState` + WinMM wrapper (Capcom named it "DirectInput") | Same wrapper, plus an XInput backend (`src/marni/MarniInput.*`, `src/marni/MarniXInput.*`) |
| Source Files | `src/marni/*`, `src/game/TextureLoader.cpp`, `src/game/SpriteRenderer.*` |

### Key Architectural Decisions

1. **PSYQ Compatibility Layer**: The Marni System lets most PlayStation code run on Windows unchanged
2. **32-bit Architecture**: The game is strictly 32-bit, using Win32 API (no 64-bit types or functions)
3. **VTable Compatibility**: All original class vtables are preserved at their original addresses to maintain binary layout compatibility with the original code that calls through function pointers
4. **Task-Based Game Logic**: Game logic is organized into tasks scheduled each frame
5. **Backend Isolation**: no ported game code calls raw DirectX APIs - everything routes through the Marni classes, which forward to `MarniDX`. This is what made the D3D5 -> DX11 swap possible without touching game code.

### Entry Point

The game entry point is `main` at `0x00441350`.

---

## 1. CMarniDirect3D — Main Graphics Class

**Files:** `src/marni/MarniSystem.h`, `src/marni/MarniSystem.cpp`  
**Original VTable Address:** `0x004af230`  
**Object Size:** `0x21DC` bytes (8676 bytes)  
**Global Instance:** `g_pMarniDirect3D` at `0x00ac4028`

### VTable (12 Entries)

| Slot | Address | Function | Signature | Description |
|------|---------|----------|-----------|-------------|
| [0] | `0x00448630` | `RequestVideoMemory` | `int(void* self)` | Allocate video memory from D3D device |
| [1] | `0x00449300` | `ChangeDisplayMode` | `int(void* self, int mode)` | Change display resolution / mode |
| [2] | `0x0044a0d0` | `SetD3DRenderer` | `void(void* self, int renderer)` | Select D3D renderer by index (HW/SW) |
| [3] | `0x0044b320` | `Clear` | `int(void* self)` | Clear Z-buffer and/or render target |
| [4] | `0x00448ff0` | `Present` | `int(void* self)` | Present/flip back buffer to front |
| [5] | `0x00448b60` | `HandleWindowMessage` | `int(void* self, HWND, UINT, WPARAM, LPARAM)` | Handle WM_ACTIVATE and window messages |
| [6] | `0x0044c900` | `CreateTextureHandle` | `int(void* self, void* texDesc, uint flags, void* out)` | Create texture handle from pixel data |
| [7] | `0x0044af90` | `CreateObjectHandle` | `uint(void* self, void* objDesc, byte flags)` | Create 3D object handle for rendering |
| [8] | `0x0044b220` | `DeleteTextureHandle` | `int(void* self, int handle)` | Delete texture handle by index |
| [9] | `0x0044b1c0` | `DeleteObjectHandle` | `int(void* self, int handle)` | Delete 3D object handle by index |
| [10] | `0x00448300` | `SetTexture` | `int(void* self, void* texData, uint param)` | TMD draw-queue insert (original: `OT_InsertPrimitive(objData, depth)`; port: `TmdQueueObject`) |
| [11] | `0x00448380` | `ResetTextures` | `int(void* self)` | Reset texture state |

### VTable Calling Convention

The original game passes `this` explicitly as the first argument (not via `__thiscall`). Each vtable entry is typed accordingly:

```cpp
// Example: VTable_Present wrapper
static PFN_Present VTable_Present_g = NULL;
static int VTable_Present(void* self) {
    return ((PFN_Present)(((void***)self)[0][4]))(self);
}
```

### Original Member Variables (offsets from Ghidra)

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | void** | `vtable` | VTable pointer (`0x004af230`) |
| 0x04 | DWORD | `m_hWnd` | Window handle (base ctor arg, 0x0044efc0) |
| 0x08 | DWORD | `m_logicalWidth` | **Logical** render resolution (written by `SetVideoResolution`/0x00497f30) |
| 0x0C | DWORD | `m_logicalHeight` | **Logical** render resolution (written by `SetVideoResolution`/0x00497f30) |
| 0x10 | DWORD | `m_width` | **Physical** backbuffer/surface width (set once at construction; updated on display-mode change / WM_SIZE) |
| 0x14 | DWORD | `m_height` | **Physical** backbuffer/surface height |
| 0x18 | DWORD | `m_bitDepth` | Color bit depth |
| 0x2C | DWORD | `m_halfLogicalWidth` | `m_logicalWidth / 2` (set in base ctor 0x0044efc0) |
| 0x30 | DWORD | `m_halfLogicalHeight` | `m_logicalHeight / 2` (set in base ctor 0x0044efc0) |
| 0x34 | float | `m_scaleX` | Scale factor (physical/logical, *=2 for 320, *=0.5 for 640 in SetVideoResolution) |
| 0x38 | float | `m_scaleY` | Scale factor (physical/logical, *=2 for 320, *=0.5 for 640 in SetVideoResolution) |
| 0x3C | BOOL | `m_isInitialized` | Initialization flag |
| 0x68 | BOOL | `m_isFullScreen` | Fullscreen mode flag |
| 0x74 | BOOL | `m_isActive` | Window active flag |
| 0x78 | DWORD | `m_selectedMode` | Display mode index |
| 0x30C | DWORD | `m_deviceType` | Device type (0-6, 5=Software) |
| 0x314 | DWORD | `m_currentMode` | Current display mode |
| 0x324 | DWORD | `m_scratch` | Scratch/state field |

### Logical vs Physical Resolution

The original CMarniDirect3D maintains TWO resolution pairs:

| Pair | Offsets | Field Names | Written By | Meaning |
|------|---------|-------------|------------|---------|
| **Logical** | 0x08/0x0C | `m_logicalWidth/m_logicalHeight` | `SetVideoResolution` (0x00497f30) | The coordinate space the game draws in (320×240 during gameplay) |
| **Physical** | 0x10/0x14 | `m_width/m_height` | Constructor / `ChangeDisplayMode` / `WM_SIZE` | The actual backbuffer dimensions (e.g. 640×480 from config.ini) |

At draw time, the original Marni layer built a D3D transform matrix with the ratio `physical/logical` (FUN_0042ba60 in the original). This is why game-space (logical 320×240) correctly fills a 640×480 window: every primitive is scaled ×2 at render time.

**`SetVideoResolution`** (0x00497f30) is the function that toggles the logical resolution for FMV transitions. It **never** touches the physical dims — it only writes `m_logicalWidth/m_logicalHeight` (0x08/0x0C) and adjusts `m_scaleX/m_scaleY` (0x34/0x38) by ×2 for 320×240 or ×0.5 for 640×480. Ports of this function must be careful not to write `m_width/m_height` (0x10/0x14), which are the physical surface dimensions.

The decomp's `MarniGetRenderScale()` helper computes the same `physical/logical` ratio from these fields and is called by `AddTintSprite`, `draw_rect`, `QueueTexturedSprite`, `FlushSpriteCommands`, and `OT_InsertPrimitive` to scale sprite coordinates from game-space to screen-space.

### VTable Calling Convention

### Modern D3D11 Members (attached at high offsets)

These replace the original DirectDraw/Direct3D 5 interface pointers (`m_lpDD`, `m_lpDD2`, `m_lpDDS_Front`, `m_lpDDS_Back`, etc.):

```cpp
// D3D11 Device & Pipeline
ID3D11Device*           m_pD3DDevice;           // D3D11 device
ID3D11DeviceContext*    m_pD3DContext;          // Immediate context
IDXGISwapChain*         m_pSwapChain;           // Swap chain (back buffer presentation)
ID3D11RenderTargetView* m_pRenderTargetView;     // Back buffer RTV
ID3D11Texture2D*        m_pDepthStencil;         // Depth/stencil buffer
ID3D11DepthStencilView* m_pDepthStencilView;     // DSV
ID3D11RasterizerState*  m_pRasterStateScissor;   // Rasterizer with scissor enabled

// Shaders & Pipeline State
ID3D11VertexShader*     m_pQuadVS;               // Quad vertex shader
ID3D11PixelShader*      m_pQuadPS;               // Quad pixel shader (textured + tint)
ID3D11InputLayout*      m_pQuadInputLayout;      // Input layout for QuadVertex
ID3D11Buffer*           m_pQuadVB;               // Reusable full-screen quad VB
ID3D11Buffer*           m_pSpriteCB;             // Sprite constant buffer (MVP matrix)

// Blend & Sampler State
ID3D11BlendState*       m_pBlendAlpha;            // Alpha blending state
ID3D11SamplerState*     m_pSamplerLinear;         // Linear texture sampler
ID3D11DepthStencilState* m_pDepthDisabled;        // Depth-disabled state (for 2D sprites)

// Fallback Textures
ID3D11Texture2D*        m_pFontTexture;           // Font texture (from TIM bank 0x1E)
ID3D11ShaderResourceView* m_pFontSRV;             // Font SRV
int                     m_FontTexWidth;            // Font texture width
int                     m_FontTexHeight;           // Font texture height
ID3D11Texture2D*        m_pWhiteTex;              // 1x1 white fallback texture
ID3D11ShaderResourceView* m_pWhiteSRV;            // White SRV (for solid-color quads)
```

### Embedded HLSL Shaders

Shaders are compiled at runtime using `D3DCompile`:

```hlsl
// g_QuadVS_Source - Vertex Shader
cbuffer SpriteCB : register(b0) {
    row_major float4x4 g_MVP;
};

struct VS_INPUT {
    float2 pos : POSITION;
    float2 tex : TEXCOORD0;
    float4 col : COLOR0;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 col : COLOR0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = mul(float4(input.pos, 0.0f, 1.0f), g_MVP);
    output.tex = input.tex;
    output.col = input.col;
    return output;
}

// g_QuadPS_Source - Pixel Shader
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 texColor = g_Texture.Sample(g_Sampler, input.tex);
    return texColor * input.col;
}
```

### QuadVertex Structure

Used for all sprite/quad rendering:

```cpp
struct QuadVertex {
    float x, y;         // Screen position
    float u, v;         // Texture coordinates
    float r, g, b, a;   // Color (0.0 - 1.0)
};
```

### Global Functions

| Function | Description | Original Address |
|----------|-------------|------------------|
| `InitializeMarniSystem()` | Creates CMarniDirect3D instance, inputs, lights, joysticks | — |
| `MarniPresent()` | Presents frame via `vtable[4]` → `IDXGISwapChain::Present` | `0x00448ff0` |
| `MarniClear()` | Clears render target via `vtable[3]` → `ClearRenderTargetView`. Also exposed as `ClearScreen()` (alias, matches the Ghidra decomp name used in `UpdateVideoPlayback` state 0) | `0x0044b320` |
| `MarniDrawSprite()` | Draws textured colored quad at screen coordinates via D3D11 | — |
| `MarniDrawRect()` | Debug helper: filled rectangle | — |
| `MarniCreateTexture()` | Creates `ID3D11Texture2D` + SRV from raw pixel data | — |
| `MarniGetDevice()` | Returns `g_pMarniDirect3D` pointer | — |
| `IsGraphicsSystemReadyForOperation()` | Checks initialization state | — |
| `EnumerateDisplayModes()` | Lists available display modes | — |
| `EnumerateD3DRenderers()` | Lists available D3D renderers (HW/SW) | — |
| `InitJoysticks()` | Enumerates WinMM devices and brings up the XInput backend; forwards to `CMarniDirectInput::InitJoysticks` | — |
| `IsSideWinderPadConnected()` | Checks for Microsoft SideWinder pad | — |
| `CreateLights(int numLights)` | Creates 3D scene lights | — |
| `UpdateVideoPlayback()` | FMV playback state machine (state 0 init / state 1 start / state 2 play+skip / state 3 cleanup). State 1 and state 2 call `InputUpdate()` + `PlayerPad_Update()` themselves because `main_loop()` is bypassed during FMV playback | — |

---

## 2. CMarniBits — Surface/Bitmap Class

**Files:** `src/marni/MarniBits.h`, `src/marni/MarniBits.cpp`  
**Original VTable Address:** `0x004af008`  
**Object Size:** `0x54` bytes (84 bytes of fields up to offset `0x50`)

CMarniBits is the 2D surface/pixel buffer class. It wraps a raw pixel buffer and optional palette (CLUT), providing Lock/Unlock, pixel read/write, palette fill, and blit operations.

### VTable (7 Entries)

| Slot | Address | Ghidra Name | Method | Signature |
|------|---------|-------------|--------|-----------|
| [0] | `0x00403090` | `CMarniBits_Blt` | `Blt` | `int(void* self, void* srcRect, CMarniBits* srcSurface)` |
| [1] | `0x00402020` | `CMarniBits_BltFast` | `BltFast` | `int(void* self, void* dstRect, CMarniBits* src, void* srcRect2, DWORD flags, DWORD flags2, void* palette)` |
| [2] | `0x00401ee0` | `CMarniBits_UnlockStub` | `UnlockStub` | `int(void* self)` — returns 1 |
| [3] | `0x00401ef0` | `CMarniBits_PalBlt` | `PalBlt` | `int(void* self, CMarniBits* src, DWORD param3, int numEntries)` |
| [4] | `0x00403450` | `CMarniBits_Lock` | `Lock` | `int(void* self, void** outData, DWORD* outPitch)` |
| [5] | `0x004034c0` | `CMarniBits_Unlock` | `Unlock` | `int(void* self)` |
| [6] | `0x00404970` | `CMarniBits_Release` | `Release` | `int(void* self)` |

### VTable Implementation

```cpp
static void* CMarniBits_vtable[7] = {
    (void*)VTable_Blt,          // [0] 0x00403090
    (void*)VTable_BltFast,      // [1] 0x00402020
    (void*)VTable_UnlockStub,   // [2] 0x00401ee0
    (void*)VTable_PalBlt,       // [3] 0x00401ef0
    (void*)VTable_Lock,         // [4] 0x00403450
    (void*)VTable_Unlock,       // [5] 0x004034c0
    (void*)VTable_Release,      // [6] 0x00404970
};
```

### Member Variables

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | void** | `vtable` | VTable pointer (`0x004af008`) |
| 0x04 | void* | `m_pPixelData` | Pixel/surface data pointer |
| 0x08 | void* | `m_pPalette` | Palette (CLUT) data pointer |
| 0x0C | DWORD | `m_locked` | Lock state (0=unlocked, 1=locked) |
| 0x10 | BYTE | `m_redShift` | Red component bit shift |
| 0x12 | WORD | `m_redMask` | Red component bitmask |
| 0x14 | BYTE | `m_redWidth` | Red component bit width |
| 0x16 | BYTE | `m_greenShift` | Green component bit shift |
| 0x18 | WORD | `m_greenMask` | Green component bitmask |
| 0x1A | BYTE | `m_greenWidth` | Green component bit width |
| 0x1C | BYTE | `m_blueShift` | Blue component bit shift |
| 0x1E | WORD | `m_blueMask` | Blue component bitmask |
| 0x20 | BYTE | `m_blueWidth` | Blue component bit width |
| 0x22 | BYTE | `m_alphaShift` | Alpha component bit shift |
| 0x24 | WORD | `m_alphaMask` | Alpha component bitmask |
| 0x26 | BYTE | `m_alphaWidth` | Alpha component bit width |
| 0x2A | BYTE | `m_bitDepth` | Bits per pixel (4/8/16/24/32) |
| 0x2B | BYTE | `m_paletteFormat` | Palette depth (0x08=8bit, 0x10=16bit, 0x20=32bit) |
| 0x2C | DWORD | `m_width` | Surface width in pixels |
| 0x30 | DWORD | `m_height` | Surface height in pixels |
| 0x34 | DWORD | `m_pitch` | Row stride in bytes |
| 0x40 | DWORD | `m_isValid` | Surface valid flag |
| 0x44 | DWORD | `m_dataSource` | 0=external pointer, 1=owned allocation |
| 0x48 | DWORD | `m_hasPalette` | Has palette (non-zero = indexed/CLUT) |
| 0x4C | DWORD | `m_ownsPalette` | Palette ownership flag |
| 0x50 | DWORD | `m_flag50` | Additional flag |

### Non-Virtual Methods (14 Total)

| Address | Ghidra Name | C++ Method | Signature |
|---------|-------------|------------|-----------|
| `0x00403860` | `CMarniBits_CalcAddress` | `CalcAddress(x, y)` | `void*(int x, int y)` |
| `0x004033f0` | `CMarniBits_SetAddress` | `SetAddress(pixels, clut)` | `int(void* pixels, void* clut)` |
| `0x00403c90` | `CMarniBits_SetColor` | `SetColor(x, y, val, flags)` | `int(int x, int y, DWORD val, DWORD flags)` |
| `0x00404120` | `CMarniBits_GetColor` | `GetColor(x, y, &out)` | `int(int x, int y, DWORD* out)` |
| `0x00403b00` | `CMarniBits_SetPaletteColor` | `SetPaletteColor(idx, color, flags)` | `int(int idx, DWORD color, int flags)` |
| `0x00404390` | `CMarniBits_GetPaletteColor` | `GetPaletteColor(idx, &out)` | `int(int idx, DWORD* out)` |
| `0x00404210` | `CMarniBits_GetCurrentColor` | `GetCurrentColor(x, y, &out)` | `int(int x, int y, DWORD* out)` |
| `0x004044d0` | `CMarniBits_GetIndexColor` | `GetIndexColor(x, y, &out)` | `int(int x, int y, DWORD* out)` |
| `0x004039b0` | `CMarniBits_SetIndexColor` | `SetIndexColor(x, y, color, flags)` | `int(int x, int y, DWORD color, DWORD flags)` |
| `0x00403e20` | `CMarniBits_SetCurrentColor` | `SetCurrentColor(x, y, color, flags)` | `int(int x, int y, DWORD color, DWORD flags)` |
| `0x004034d0` | `CMarniBits_CopyFrom` | `CopyFrom(src)` | `int(CMarniBits* src)` |
| `0x00404690` | `MarniBits::CreateWork` | `CreateWork(w, h, bpp, palFlags)` | `int(int w, int h, int bpp, DWORD palFlags)` |
| `0x00403210` | `SaveBitmapToFile` | `SaveBitmapToFile(filename)` | `int(const char* filename)` |
| `0x00404910` | `CMarniBits_Constructor` | Constructor | Zeroes all fields, sets vtable |
| `0x00404960` | `CMarniBits_DestructorBody` | `DestructorBody()` | Resets vtable, calls Release |

### Key Method Descriptions

#### Blt (vtable[0]) — `0x00403090`
Copies pixels from a source surface with clipping. Uses target rectangle defined by the `m_clip*` fields. Validates dimensions and delegates pixel copy.

#### BltFast (vtable[1]) — `0x00402020`
Pixel-format-converting blit with support for:
- Scaling (stretch/shrink)
- Mirror/flip
- Color key (transparency)
- Alpha blending
- DDBLTFAST flags: `DDBLTFAST_NOCOLORKEY`, `DDBLTFAST_SRCCOLORKEY`, `DDBLTFAST_WAIT`

#### Lock / Unlock (vtable[4]/[5]) — `0x00403450` / `0x004034c0`
Lock returns a pointer to pixel data and the row pitch. Unlock must be called (once) after Lock. Nested locks are not supported; Lock returns an error if already locked.

#### SetCurrentColor — `0x00403e20`
Writes an ARGB color value at (x, y). For indexed surfaces (4bpp/8bpp), it finds the nearest CLUT match via Euclidean RGB distance. For direct-color surfaces (16bpp+), it converts to the native pixel format. Supports alpha blending with the existing pixel value when a non-zero blend parameter is specified.

#### CreateWork — `0x00404690`
Allocates pixel and palette buffers:
- `w * h * (bpp / 8)` bytes for pixel data
- `numColors * paletteEntrySize` bytes for palette
- Sets `m_dataSource = 1` (owned) and `m_isValid = 1`

#### SaveBitmapToFile — `0x00403210`
Exports the surface as a 24-bit BMP file. Handles all pixel format conversions internally.

### Pixel Format Support

| bpp | Type | CLUT Required | Palette Entry Size | Description |
|-----|------|---------------|--------------------|-------------|
| 4 | Indexed | Yes | 2 bytes (RGB555) | 16-color (PS1 4bpp TIM) |
| 8 | Indexed | Yes | 2 bytes (RGB555) | 256-color (PS1 8bpp TIM) |
| 16 | Direct | No | — | PSX RGB 5:5:5 (ABGR 1:5:5:5 or 5:6:5) |
| 24 | Direct | No | — | RGB 8:8:8 |
| 32 | Direct | No | — | ARGB 8:8:8:8 |

---

## 3. PSXTexture — PS1 TIM/PIX Texture Loader

**Files:** `src/marni/PSXTexture.h`, `src/marni/PSXTexture.cpp`  
**Object Size:** `0x348` bytes (840 bytes) — 8 × 0x68-byte CMarniBits sub-objects  
**Embedded:** 8 × CMarniBits sub-objects at 0x68-byte intervals

### Layout

PSXTexture embeds 8 CMarniBits sub-objects using placement-new at construction.
The original stores **no CLUT color data inside the object** — `Store()` copies the palette to a heap buffer (`operator_new`), and the 0xC0 region is the multi-CLUT **descriptor** array (not palette storage).

| Offset | Content |
|--------|---------|
| 0x000 | CMarniBits #0 (main PSX texture surface) |
| 0x068 | CMarniBits #1 (CLUT entry 1) |
| 0x0D0 | CMarniBits #2 (CLUT entry 2) |
| 0x138 | CMarniBits #3 (CLUT entry 3) |
| 0x1A0 | CMarniBits #4 (CLUT entry 4) |
| 0x208 | CMarniBits #5 (CLUT entry 5) |
| 0x270 | CMarniBits #6 (CLUT entry 6) |
| 0x2D8 | CMarniBits #7 (CLUT entry 7) |
| 0x340 | `m_NumCLUTs` (DWORD) — active CLUT count |
| 0x344 | `m_IsInitialized` (DWORD) — initialized flag |

### Critical Layout Constraint — CLUT Must Not Be Inline

An earlier version of the decomp stored an inline `DWORD m_CLUT_Data[512]` at offset **0xC0**, colliding with the multi-CLUT descriptor array. When a TIM has more than one CLUT row (`m_NumCLUTs > 1`, e.g. `status.tim` with its three 256-colour palettes), `Store()`'s multi-CLUT block wrote descriptor metadata directly over the first ~21 palette entries, corrupting them:

| Corrupted palette entry | Should be (RGB555) | After corruption | Visual result |
|-------------------------|-------------------|-------------------|---------------|
| 0x13 (19, frame edge) | R=14 G=17 B=14 greyish-green | `0x0000` black | Black where greyish-green should be |
| 0x0C (12) | R=4 G=6 B=4 grey | `0x03E0` solid green | Green splotches |
| 0x14 (20) / 0x15 (21) | mid-greys | `0x0000` black | Blacks where greys should be |

The inline array also bloated `sizeof(PSXTexture)` to 0xAC8, so `Store()` on `g_psxTextureArray` slots (stride 0x36C) corrupted adjacent texture banks.

**Fix:** `Store()` allocates the CLUT copy on the heap with `operator_new()`, exactly like the original `LoadPSXImage` at 0x0041fb60. The palette pointer lives at `m_pCLUTData` (offset 0x08, aliasing `CMarniBits::m_pPalette`), and the destructor frees it when owned (`copyData=1`). The struct is back to 0x348, enforced by `static_assert`.

### Member Variables

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | void** | `vtable` | CMarniBits vtable |
| 0x04 | void* | `m_pPixelData` | Pointer to pixel data buffer |
| 0x08 | `union {DWORD m_Pitch; WORD* m_pCLUTData;}` | Pitch or heap CLUT pointer (CMarniBits::m_pPalette alias) |
| 0x0C | DWORD | `m_locked` | CMarniBits lock state |
| 0x10-0x27 | — | (CMarniBits pixel format descriptor) |
| 0x2A | BYTE | `m_BitDepth` | Pixel bit depth (4, 8, 16) |
| 0x2B | BYTE | `m_FormatFlags` | Format flags from TIM header |
| 0x2C | DWORD | `m_WidthPixels` | Width in pixels (image‑specific formula, not always pixel count — depends on bpp) |
| 0x30 | DWORD | `m_Height` | Height in pixels |
| 0x34 | DWORD | `m_RowStride` | Bytes per row (imgW × 2) |
| 0x40 | DWORD | `m_IsValid` (CMarniBits) / lock flag (PSXTexture alias) |
| 0x44 | DWORD | `m_DataSource` | 0=in‑place, 1=owned copy |
| 0x48 | DWORD | `m_HasCLUT` | Has colour lookup table |
| 0x4C | DWORD | `m_Flag4C` | Overlaps CMarniBits::m_ownsPalette; set to 1 by Store |
| 0x54 | WORD | `m_CLUT_X` | CLUT X origin in VRAM |
| 0x56 | WORD | `m_CLUT_Y` | CLUT Y origin in VRAM |
| 0x58 | WORD | `m_CLUT_W` | CLUT width (colours per palette) |
| 0x5A | WORD | `m_CLUT_H` | CLUT height (palette rows, 1–8 for 8bpp) |
| 0x5C-0x63 | WORD[4] | `m_CLUT_W2/H2/W3/H3` | Extra CLUT dimension fields (original stores W at 0x5C as DWORD) |

### TIM File Format — CLUT Parse Corrections

The `Store` method at `0x0041fb60` parses the PS1 TIM image format.
The original field order in the CLUT header (verified against 0x0041fba0) is:

```
Offset 0x0C: CLUT origin    (low 16 = X, high 16 = Y)
Offset 0x10: CLUT dimensions (low 16 = colours per palette WIDTH, high 16 = palette row HEIGHT)
```

Earlier versions of the decomp had these LABEL-swapped (reading bits 16‑31 for X and low bits for Y). Because the product `clutW × clutH` is commutative this only caused subtle problems: `m_NumCLUTs` got `clutH` instead of `clutW` (harmless for 256×1 CLUTs but wrong for multi‑palette TIMs like `status.tim`'s 256×3), and the multi-CLUT descriptor entries read swapped X/Y values. The current code uses the correct field order.

### Methods

---

## 3A. CDirect3DObject / CMarniExecuteBuffer — 3D Object Classes

**File:** `src/marni/Marni3DObject.h`, `src/marni/Marni3DObject.cpp`

### CDirect3DObject (Base Class)
**Size:** 0x38 bytes (14 DWORDs)  
**VTable:** 0x004af090 (8 entries)  
**Vertex Format:** 0x20 bytes/vertex (8 DWORDs: sx, sy, sz, rhw, color, specular, tu, tv)

| Slot | Address | Ghidra Name | Method | Description |
|------|---------|-------------|--------|-------------|
| [0] | 0x00427270 | `CMarniViewport2_Release` | `Release()` | Free vertex/index buffers |
| [1] | 0x00415e90 | `Direct3DObject_CreateWork` | `CreateWork(vtx,list,type)` | Alloc 32-byte vertex + 8/16-byte index buffers |
| [2] | 0x00415a80 | `Direct3DObject_GetVertex` | `GetVertex(idx,out)` | Read 8 DWORDs from vertex buffer |
| [3] | 0x00415b40 | `Direct3DObject_SetVertex` | `SetVertex(idx,data)` | Write 8 DWORDs to vertex buffer |
| [4] | 0x00415c10 | `Direct3DObject_GetList` | `GetList(idx,out)` | Read tri (3×WORD) or quad (4×WORD) indices |
| [5] | 0x00415d20 | `Direct3DObject_SetList` | `SetList(idx,data)` | Write indices; quads split to 2 tris with `0x700` opcodes |
| [6] | 0x004159e0 | `MarniPolyhedra_Lock` | `Lock(&vtx,&idx)` | Return buffer pointers, set lock=1 |
| [7] | 0x00415a50 | `MarniPolyhedra_Unlock` | `Unlock()` | Clear lock flag |

### CMarniExecuteBuffer (inherits CDirect3DObject)
**Constructor:** 0x00415f70  
**Destructor:** 0x00415fd0  
Created via `CMarniDirect3D::CreateObjectHandle` (vtable[7]). Manages D3D execute buffers for 3D rendering commands.

### CMarniPolyhedra (inherits CDirect3DObject)
Adds `CopyFrom()` (0x00426600) for copying vertex/index data between polyhedra. Used by sprite rendering pipeline for 3D geometry.

### Member Layout (CDirect3DObject)
| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | void** | `vtable` | VTable pointer |
| 0x04 | void* | `m_pVertexBuffer` | Vertex data (0x20 bytes/vertex) |
| 0x08 | void* | `m_pIndexBuffer` | Index list data (8/16 bytes/primitive) |
| 0x0C | DWORD | `m_bHasBuffers` | Buffers allocated flag |
| 0x14 | DWORD | `m_locked` | Lock state |
| 0x24 | DWORD | `m_vertexCount` | Current vertex count |
| 0x28 | DWORD | `m_listCount` | Current primitive count |
| 0x2C | DWORD | `m_primitiveType` | 3=triangle, 4=quad |
| 0x30 | DWORD | `m_vertexCapacity` | Max vertices |
| 0x34 | DWORD | `m_listCapacity` | Max primitives |

---

## 3B. CMarniDirect3DTMD — TMD 3D Model Renderer

**File:** `src/marni/Marni3DObject.h`, `src/marni/Marni3DObject.cpp`  
**Object Size:** 0x1594+ bytes (~5524 bytes)  
**Constructor:** 0x00415910  
**Debug String:** `"MarniSystem Direct3DTMD"` at 0x004b45e8

Manages up to 16 3D mesh objects with per-object transform matrices and texture/material lookups.

| Method | Address | Description |
|--------|---------|-------------|
| `Constructor` | 0x00415910 | Zeros object data arrays + handle array |
| `Create(ctx,mats)` | 0x00415650 | Creates per-object D3D handles, sets up matrices/textures from material context |
| `Transform(ctx,depth,matrix,db)` | 0x00415520 | Queues all objects via vtable[10] with the OT depth, then stores the 16-float model→view matrix into each objData+8 |
| `Destroy(ctx)` | 0x00415880 | Releases all D3D object handles |
| `CleanupObjects(param)` | 0x004158e0 | Destroy + additional resource cleanup |

### Per-Object Data (stride 0x84 bytes)
| Offset | Type | Field |
|--------|------|-------|
| 0x00 | DWORD | Primitive type (4 = TMD mesh object) |
| 0x08 | float[16] | Model→view transform (written by `Transform` from the FUN_00483080 matrix) |
| 0x54 | DWORD | D3D object handle (opaque token in the DX11 port) |
| 0x58 | DWORD | Texture handle (MarniHandle from vtable[6] CreateTextureHandle) |

### DX11 render path (this port)

The original walked the ordering table at present time and drew each queued
object through D3D5 execute buffers, with a **depth buffer** resolving the
triangles inside a single object. The port replaces the OT with a flat per-frame
queue in `src/game/TmdRenderer.cpp` and substitutes a per-triangle depth sort
for that depth buffer.

Full chain for one entity joint:

| Step | Function | File | What it does |
|------|----------|------|--------------|
| 1 | `render_entity` (0x0048c350) / `options_render_entity` (0x004775b0) | EngineStubs.cpp / OptionsMenu.cpp | Per joint: composes the camera matrix with `joint->world` (`ApplyLVAndMul0Matrix`), pushes it through `SetLightMatrix` + `SetRotAndTransMatrix`, then calls `FUN_00483250` with the joint's `anim_object` |
| 2 | `FUN_00483250` (0x00483250) | EngineStubs.cpp | Thin forwarder: `FUN_00483080(animObject, depthShift)` |
| 3 | `FUN_00483080` (0x00483080) | EngineStubs.cpp | Bails if `g_gteRotTransMatrix.t[2] < 0` (behind camera) or `data[1] == 0` (no textured prims). Calls `AsyncCreateTmdObject` → builds the model→view float matrix from `g_gteRotTransMatrix` → `FUN_00486190` folds the view in → `Transform` |
| 4 | `CreateTmdObjectInternal` (0x00483910) | TmdAnimation.cpp | Finds/reuses a `CMarniDirect3DTMD` slot, calls `PSXObject_Store` to parse the TMD into the slot's embedded `CMarniViewport2` elements, then matches each parsed object against a texture page and calls `Create` |
| 5 | `CMarniDirect3DTMD::Transform` (0x00415520) | Marni3DObject.cpp | Inserts every object into the queue via `vtable[10]`, **then** writes the 16-float matrix to `objData+0x08` |
| 6 | `TmdQueueObject` | TmdRenderer.cpp | Records `(slot, objData, objIndex, depth)` only — never a copy of the render state |
| 7 | `FlushTmdObjects` | TmdRenderer.cpp | Transforms/projects/lights every vertex, collects triangles, depth-sorts them, submits via `MarniDX::DrawTriangles` |

#### Projection

`FUN_00483080` stores the GTE depth (`g_gteRotTransMatrix.t[2]`, **positive** in
front of the camera) as the matrix Z translation, and `FUN_00486190` only rotates
it, so the flush projects with:

```
sx = cx + vx * f / vz
sy = cy - vy * f / vz          // vy is negated by SetRotAndTransMatrix
```

with `cx,cy = g_SubpixelOffset{X,Y} * renderScale` and `f = g_sceneRenderParam *
renderScale`. Vertices with `vz <= 1` are dropped.

#### Depth resolution and culling

Triangles from **all** queued objects are gathered into one per-frame pool with
their mean view-space Z, sorted far-to-near, then submitted with adjacent
same-texture runs merged. There is **no backface culling**: the PS1 GPU draws
both sides and the depth sort makes the nearer face win, which removes any
dependence on the TMD winding convention. Only zero-area triangles are dropped.

> Re-enabling backface culling would roughly halve the submitted triangle count
> (~609 → ~305 for the player model). It is safe now that the vertex indices are
> read correctly, but it is not required for correctness.

#### Lighting

`g_d3dLightData` holds 3 lights × 12 DWORDs; `[3..5]` is the direction written by
`SetLightMatrix`, `[6..8]` the colour written by `FUN_0040ac80`. `SetLightMatrix`
sets `pLight[0] = 2` (`D3DLIGHT_DIRECTIONAL`) and stores the normalised light
**position**, so the field is a D3D `dvDirection` and the diffuse term is
`dot(N, -direction)` — the negation matters. Ambient comes from
`g_d3dAmbientColor`, which `setBackColor` fills as `value * 255 / 4096` (the
options menu's `setBackColor(409,409,409)` is only ~10% grey, so unlit faces are
genuinely near-black).

#### Textures

TMD material textures are real D3D11 textures created by
`VTable_CreateTextureHandle` (PSX VRAM 4/8/16bpp + 15-bit CLUT → RGBA8) and reach
the object via `CreateTmdObjectInternal` → `objData+0x58`.

### Pipeline invariants (each of these was a real bug)

Things that must hold for a model to render correctly. All were violated at some
point in this port and each produced a distinct, misleading symptom:

| Invariant | Symptom when broken |
|-----------|--------------------|
| `PSXTexture`'s CLUT descriptor at `+0x54..+0x67` is **five DWORDs** (`0x54` = CLUT VRAM X, `0x58` = Y). `CreateTmdObjectInternal` matches those DWORDs against each object's key at `elem+0x38/+0x3C` | No key ever matches → `AsyncCreateTmdObject` returns 0 → nothing 3D renders anywhere |
| A raw BSS `CMarniDirect3DTMD` slot must have `m_unknown4C8` (`slot+0x4C8`) seeded to `0x400` — the constructor's subdivide threshold | `PSXObject_Resize` splits every element until the object count passes 16, `Store` fails with "too many objects", and a 17th element is written over the slot trailer |
| The queue may store only the objData **pointer**; the matrix must be read at flush time | `Transform` writes the matrix *after* queueing, so a snapshot yields the previous frame's transform — or an all-zero matrix on first use, collapsing every vertex to one point |
| Gouraud primitives (`0x30000406`, `0x34000609`, `0x3C00080C`) pack `(vertexIndex << 16) \| normalIndex` per dword: every vertex index comes from the **high** half. Flat primitives (`0x20000304`, `0x24000507`) genuinely use the low half for v1 | One corner of every triangle is displaced → winding random relative to the normals, so no cull orientation works and shading is patchy with black faces |
| `FUN_00483080` must store the GTE rotation in the original's column order (`m[0][0..2]` → `M[0], M[4], M[8]`) | Transposed (inverse) rotation; model mis-oriented |
| `FUN_00486190` builds the view from `from = (0,0,-fov)`, `to = (offsetX, offsetY, 0)` so the direction has **positive** Z | Negating Z adds a 180° yaw. It cancels out only while the subpixel offset is exactly screen centre, so the main menu breaks and the options menu does not |
| `Display_SetParams` (0x00470750) writes `0x004c335c/0x004c3360`, **not** the subpixel offset (`0x004d2bd0/0x004d2bd4`) | `ResetScreenAndRebuildSprites` calls it with (0,0) every frame, so the projection centre is permanently 0 and everything is projected around the top-left corner |
| Trig tables are **14-bit** (`sin/cos * 16384`, saturated to ±0x3FFF), not 12-bit — `GteRotationMatrixCalc` combines two lookups with `>> 14` and `RotMatrix` shifts down 2 more to reach 4.12 | Rotation matrices come out 4–16× too small and compound through the joint hierarchy, so deep joints end up with an all-zero rotation |
| `GteSin` is **0x004409f0** (reads `[0x004bcacc]`, the `fsin` table) and `GteCos` is **0x00440a10** (reads `[0x004bcad0]`, the `fcos` table). The Ghidra symbols were originally attached the other way round | Swapping them makes `RotMatrix` emit `m[1][1] = -cos` instead of `1` — a mirrored/garbage permutation rather than a rotation |
| The Euler helpers (`GteRotationMatrixCalc` 0x004406a0, `GteRotationMatrixYXZ` 0x00440b70, `FUN_004403c0`) output an **`int[9]`** — nine dwords at `+0x00`…`+0x20`, row-major 3x3, 14-bit amplitude. Their `RotMatrix*` wrappers then `>>2` each into the nine shorts of `m->m`. A `MATRIX*` is only 32 bytes and cannot hold nine ints | Typing the out-param as `MATRIX*` and writing through `*(int*)&m->m[i][j]` *almost* works — 8 of 9 int offsets coincide with short offsets — but `m[1][0]` sits at byte 6, not 32. Reading back as `short` then yields a degenerate matrix: a pure 90° yaw comes out with two all-zero rows |
| `DXGI_FORMAT_R8G8B8A8_UNORM` needs red in the **lowest** byte: pack `(a<<24)\|(b<<16)\|(g<<8)\|r` | Red and blue swapped — reddish surfaces render blue |

Note that several conversion sites in the tree read "red" from bits 10-14 *and*
pack it high (`MarniDX::ConvertToRGBA8`, `MarniBits`); those two errors cancel,
so the variable names are misleading but the output is correct. Check both the
bit source and the byte position before changing one.

---

## 3C. CMarniViewport2 — 3D Viewport Class

**File:** `src/marni/Marni3DObject.h`, `src/marni/Marni3DObject.cpp`  
**VTable:** 0x004af0f8 (Type 2)  
**Object Size:** 0x40 bytes (16 DWORDs)  
**Constructor:** 0x004272e0  
**Vertex Format:** 0x2C bytes/vertex (11 floats: position.xyz, normal.xyz, uv.xy, color.rgb)

Share the same memory layout as CDirect3DObject but with a different vtable and larger vertex stride.

| VTable | Address | Method | Description |
|--------|---------|--------|-------------|
| [0] | 0x00427270 | `Release()` | Free vertex+index buffers, zero all fields |
| [1] | 0x00427100 | `CreateWork(vtx,poly,type)` | Alloc 0x2C/vertex + type*2/poly buffers |
| [2] | 0x00426d60 | `GetVertex(idx,out)` | Read 11 floats (0x2C bytes) |
| [3] | 0x00426df0 | `SetVertex(idx,data)` | Write 11 floats |
| [4] | 0x00426e80 | `GetList(idx,out)` | Read 3 (tri) / 4 (quad) WORD indices |
| [5] | 0x00426f70 | `SetList(idx,data)` | Write indices with bounds checking |
| [6] | 0x004271e0 | `Lock(&vtx,&idx)` | Return buffer pointers, set lock=1 |
| [7] | 0x00427250 | `Unlock()` | Clear lock flag |

**Non-Virtual Methods:**
| Address | Method | Description |
|---------|--------|-------------|
| 0x00426600 | `CopyFrom(src)` | Deep copy with strip↔flat conversion, normal recalculation |
| 0x004262e0 | `Convert0(src)` | Convert strip indices to flat triangle lists |
| 0x00425c10 | `TriangleDivide(arr,n)` | Subdivide triangle polyhedra (free function) |

**Member Layout (additional fields beyond CDirect3DObject):**
| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x38 | DWORD | `m_renderStyle` | Strip (0) vs flat (1) mode |
| 0x3C | DWORD | `m_converted` | Converted flag |

---

## 3D. DirectInput — Marni Input System

**File:** `src/marni/MarniInput.h`, `src/marni/MarniInput.cpp`  
**Debug String:** `"MarniSystem DirectInput Class"` at 0x004ba154

NOT the real DirectInput API — Capcom's custom wrapper using Win32 `GetAsyncKeyState` for keyboard and WinMM `joyGetPosEx` for joysticks. Maps PS1 controller semantics to PC input.

> **Pad support: see `docs/GAMEPAD_INPUT.md`.** The port adds an XInput backend
> alongside the WinMM sweep, both publishing into joystick slot 0. That document
> also covers the pad mask bit contract, the 0-based slot indexing deviation
> described below, the default binding tables, and the save-file compatibility
> guard. The whole pad path was dead before 2026-09-03.

| Method | Address | Description |
|--------|---------|-------------|
| `UpdateKeyboardInputState(pState)` | 0x004202f0 | Polls 32 VK codes via `GetAsyncKeyState`, builds 32-bit button bitmask |
| `UpdateAllInputStates(pState)` | 0x00420570 | Keyboard state transitions (prev→curr→repeat), then the XInput pad into slot 0, then the WinMM sweep (axis / POV / button parsing) over the remaining devices |
| `SetDefaultKeyMapping(keyMap)` | 0x00420720 | Default PS1 layout: E/X/S/D + arrows + 0-9 |
| `InitJoysticks(pState)` | 0x00420770 | `MarniXInput::Init()`, then `joyGetNumDevs()` → validate each with `joyGetDevCapsA` + `joyGetPosEx`. Publishes `joyCaps.wCaps` into `povFlags` (the POV hat read is gated on `JOYCAPS_HASPOV` in that field) |
| `MarniPadIsConnected()` | port addition | True when slot 0 carries a usable pad — XInput, or a WinMM device that survived validation |

### MasterInputState Struct
**Size:** ~0x3B2C bytes

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | BYTE[32] | `keyMap` | Virtual key codes for 32 PS1 buttons |
| 0x24 | DWORD | `keyboardCurr` | Current frame pressed keys (bitmask) |
| 0x28 | DWORD | `keyboardPrev` | Previous frame pressed keys |
| 0x2C | DWORD | `keyboardNewPress` | Newly pressed this frame (~prev & curr) |
| 0x30 | DWORD | `keyboardRepeat` | Repeat latch |
| 0x200 | JoystickEntry[32] | `joysticks` | Up to 32 joystick entries (0x1D8 bytes each). **Re-based**: the original array starts at 0x28 and is indexed from 1, so its `entry[1]` — the only one the game reads — sits at 0x200. Here that entry is `joysticks[0]` and indexing is 0-based |
| — | DWORD | `joystickCount` | WinMM device count. The original stores count + 1 at 0x3B28 to suit its 1-based indexing |

### Default Key Mapping
| PS1 Button | VK Code | Key |
|-----------|---------|-----|
| Action/Confirm | 0x45 | E |
| Cross | 0x58 | X |
| Square | 0x53 | S |
| Triangle | 0x44 | D |
| D-Pad Up | 0x26 | ↑ |
| D-Pad Down | 0x28 | ↓ |
| D-Pad Left | 0x25 | ← |
| D-Pad Right | 0x27 | → |
| Functions 0-9 | 0x30-0x39 | 0-9 |

### Call Chain
```
InputUpdate()  [0x00497c00]
  ├─ UpdateAllInputStates(&g_pMasterInputState)
  │    ├─ UpdateKeyboardInputState()     [GetAsyncKeyState per VK]
  │    ├─ MarniXInput::Poll()            [slot 0, when an XInput pad is present]
  │    └─ joyGetPosEx() per device       [WinMM sweep, skips slot 0 if XInput owns it]
  └─ refresh g_bPadConnected / g_NumControllers, seed pad defaults
```

---

## 4. PSYQ GPU Emulation — Sprite Renderer

**Files:** `src/game/SpriteRenderer.h`, `src/game/SpriteRenderer.cpp`

### Architecture

The original PS1 used an "ordering table" (OT) where GPU packets were linked by Z-depth. The PC port emulated this with a sprite command buffer (`g_SpriteCommandBuffer[300]`) and an ordering table (`g_OT[32]`).

### Global Variables

| Variable | Type | Address | Description |
|----------|------|---------|-------------|
| `g_SpriteCommandBuffer` | `SpriteCommand[300]` | `0x008ec900` (approx) | Sprite draw command queue |
| `g_OT` | `OTEntry[32]` | `0x008ed000` (approx) | Ordering table entries |
| `g_RenderBufferIndex` | int | `0x004c3310` | Current render buffer index |
| `g_RenderDisableFlags` | int | `0x004c3314` | Render state flags |
| `g_SubpixelOffsetX` | int | `0x004c3358` | Sub-pixel scroll offset X |
| `g_SubpixelOffsetY` | int | `0x004c335c` | Sub-pixel scroll offset Y |
| `g_MaxFadeValue` | int | `0x004c3368` | Maximum fade value (4095) |
| `g_DepthSortOverride` | int | `0x004c2d14` | Depth sorting override |
| `g_ColorScaleFactor` | float | `0x004af2ac` | Color conversion factor (1.0f / 255.0f) |

### SpriteCommand Structure

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | int | `type` | Primitive type (10 = sprite) |
| 0x04 | float | `r` | Render flags / red component |
| 0x08 | float | `g` | Green / brightness |
| 0x0C | float | `b` | Blue |
| 0x10 | float | `blend` | Alpha blend |
| 0x14 | float | `x0` | Left screen X |
| 0x18 | float | `y0` | Top screen Y |
| 0x1C | float | `x1` | Right screen X |
| 0x20 | float | `y1` | Bottom screen Y |
| 0x24 | int | `depthSort` | Depth sorting key |
| 0x28 | short | `u0` | Left texture U coordinate |
| 0x2A | short | `v0` | Top texture V coordinate |
| 0x2C | short | `u1` | Right texture U coordinate |
| 0x2E | short | `v1` | Bottom texture V coordinate |
| 0x30 | int | `texturePage` | Texture page handle |
| 0x34 | int | `extraFlags` | Extra flags / D3D SRV slot index |

### Sprite Functions

| Address | Ghidra Name | C++ Function | Signature |
|---------|-------------|--------------|-----------|
| `0x0046d960` | `BuildSpriteRenderFlags` | `BuildSpriteRenderFlags` | `void(uint texFlags, uint* out)` |
| `0x0046d940` | `GetTextureVariant` | `GetTextureVariant` | `int(uint texFlags)` |
| `0x0046d990` | `SpriteQueue_Reset` | `SpriteQueue_Reset` | `void()` |
| `0x0046e5a0` | `draw_texture` | `draw_texture` | `int(uint* pak, ushort depth)` |
| `0x0046df60` | `AddSprite` | `AddSprite` | `int(uint* pak, short depth, int tpage, int fade)` |
| `0x0046e200` | `AddTintSprite` | `AddTintSprite` | `int(uint* pak, ushort fade)` |
| `0x0046dc00` | `SubmitEffectSprite` | `SubmitEffectSprite` | `int(void* buf, int depth, int texId, byte r, byte g, byte b, int scaleX, int scaleY, int blend, short bright)` |
| `0x0046edb0` | `SubmitEffectSprite_Ex` | (variant) | Extended effect sprite |
| `0x0046f280` | `AddSprite_Ex` | (variant) | Extended sprite with sub-pixel scrolling |
| `0x0046f8a0` | `AddTintSprite_Ex` | (variant) | Extended tint sprite |
| — | (new) | `FlushSpriteCommands` | Converts buffer → D3D11 draw calls |

### Polygon Functions

| Address | Function | Description |
|---------|----------|-------------|
| `0x0046fcf0` | `DrawPrim_SpriteLarge` | Large full-screen primitive |
| `0x0046d9c0` | `AddFadePoly` | Translucent polygon with Z-sort |

### Rendering Pipeline

```
PS1 Game Code (SPR/DR_MODE GPU packets)
    ↓
AddSprite / draw_texture / AddTintSprite / SubmitEffectSprite
    ↓
g_SpriteCommandBuffer[300] (queued per frame)
    ↓
FlushSpriteCommands()  [called in game_frame_present]
    ↓
MarniDrawSprite()  [D3D11 textured quad via Map/Unmap on quad VB]
    ↓
IDXGISwapChain::Present()
```

### FlushSpriteCommands — New D3D11 Implementation

```cpp
void FlushSpriteCommands(void) {
    // Applies screen-scale transform (logical 320×240 → physical backbuffer)
    // via MarniGetRenderScale() which returns physical/logical ratio
    // (the same ratio the original Marni layer used in its D3D transform
    //  matrix at FUN_0042ba60).
    // For each valid sprite command:
    //   1. Scales position/size to D3D11 screen coordinates
    //   2. Looksup texture SRV from g_TexturePageSRV[] by command's texturePage
    //   3. Calls MarniDrawSprite(x, y, w, h, u0, v0, u1, v1, color, srv)
}
```

### MarniGetRenderScale — Game‑Space Scaling

This helper computes the scale from game‑space (logical 320×240) to the real D3D11 backbuffer.
It is the decomp's equivalent of the original draw‑time ratio `physical/logical`:

```cpp
void MarniGetRenderScale(float* outScaleX, float* outScaleY) {
    DWORD bw = 0, bh = 0;    // physical backbuffer size (from MarniDX)
    DWORD lw = 320, lh = 240; // logical resolution (CMarniDirect3D::m_logicalWidth/Height)
    // ...
    *outScaleX = (float)bw / (float)lw;
    *outScaleY = (float)bh / (float)lh;
}
```

Called by `AddTintSprite`, `draw_rect`, `QueueTexturedSprite`, `FlushSpriteCommands`, and `OT_InsertPrimitive`. The scale must always be derived from the **real** D3D11 backbuffer size rather than `CMarniDirect3D::m_width/m_height`, because `SetVideoResolution` (0x00497f30) writes the logical resolution (320×240 during gameplay) — writing the physical fields instead was the root cause of the frame being pinned to the top‑left quadrant of the window.

### BuildSpriteRenderFlags — `0x0046d960`

Extracts mirror/scale flags from PS1 GPU texture flags word:
- Bit 22 (`0x400000`) → Mirror vertically (flag `0x20`)
- Bit 23 (`0x800000`) → Mirror horizontally (flag `0x10`)

### GetTextureVariant — `0x0046d940`

Extracts texture variant (1-4) from flags bit 28-29 when bit 30 is set:
```cpp
if (textureFlags & 0x40000000)
    return ((textureFlags & 0x30000000) >> 28) + 1;
return 0;
```

---

## 5. Texture Page Management

**File:** `src/game/TextureLoader.cpp`

### Async Workers

| Address | Function | Description |
|---------|----------|-------------|
| `0x0046c130` | `AsyncCreateTexturePage` | Calls `CMarniDirect3D::vtable[6]` = `CreateTextureHandle` |
| `0x0046c1b0` | `destroy_texture_page` | Sets handle target, queues `AsyncDestroyTexturePage` |
| `0x0046c210` | `AsyncCreateObject` | Calls `CMarniDirect3D::vtable[7]` = `CreateObjectHandle` |
| `0x0046c260` | `AsyncDeleteObject` | Calls `CMarniDirect3D::vtable[9]` = `DeleteObjectHandle` |

### Core Texture Functions

| Address | Function | Description |
|---------|----------|-------------|
| `0x0046c160` | `create_texture_page` | Copies PSX data → work buffer, queues async texture creation |
| `0x0046c1b0` | `destroy_texture_page` | Queues async texture deletion by handle |
| `0x0046c5f0` | `ProcessTextureImage` | Auto-positioned font texture loader (bank 0x1E → D3D11 font SRV) |
| `0x0046c870` | `LoadTexturePage` | Load PSX texture page with slot-shifted D3D11 SRV creation |
| `0x0046ccd0` | `LoadShadowMaskTexture` | Load and process shadow mask texture (alpha-mask) |
| `0x0046c2a0` | `TexturePage_Load` | Load PSX image into a texture slot |
| `0x0046c360` | `TexturePage_ClearAll` | Clear all texture pages |
| `0x0046c3c0` | `TexturePage_Create` | Create texture page for a slot |
| `0x0046c410` | `TexturePage_SetupFull` | Full texture page setup |
| `0x0046d250` | `TexturePage_Refresh` | Refresh a texture page |
| `0x0046d2d0` | `TexturePage_RefreshCLUT` | Refresh CLUT-based page |
| `0x0046d690` | `TexturePage_LoadImage` | Load and process full image |
| `0x0046d080` | `delete_texture_set_secondary` | Delete texture set (shifted slot) |
| `0x0046fb50` | `CreateTexturedQuad` | Create 4-vertex textured quad for 3D viewport |
| `0x0046d0e0` | `SetupTexturePageHandles` | Rebuild all pages or update single page |

### D3D11 TexturePageSRV Array

During `LoadTexturePage`, `ProcessTextureImage`, and `LoadShadowMaskTexture`, PSX pixel data is converted to RGBA and uploaded to D3D11 via `MarniCreateTexture`:

```cpp
// From LoadTexturePage (0x0046c870)
if (slotIndex >= 0 && slotIndex < 256) {
    ID3D11Texture2D* tex = NULL;
    MarniCreateTexture(w, h, 32, rgba, &tex, &g_TexturePageSRV[slotIndex]);
    if (tex) tex->Release();  // Keep only the SRV
    g_TexturePageWidth[slotIndex] = w;
    g_TexturePageHeight[slotIndex] = h;
}
```

The `g_TexturePageSRV[256]` array is the D3D11 equivalent of the original PS1 texture page table. Each slot holds a shader resource view ready for sprite rendering.

### create_texture_page — `0x0046c160`

```cpp
int create_texture_page(void* psxTexData, int flags) {
    CMarniBits_CopyFrom(&g_MarniBitsWorkBuffer, psxTexData);
    g_texturePageMode = flags;
    ExecAsync((void*)AsyncCreateTexturePage);
    return g_texturePageHandle;
}
```

### destroy_texture_page — `0x0046c1b0`

```cpp
void destroy_texture_page(int id) {
    g_texturePageHandle = id;
    ExecAsync((void*)AsyncDestroyTexturePage);
}
```

---

## 6. Global Arrays (Static Initialization)

### Texture Array (`g_TextureArray`)

| Property | Value |
|----------|-------|
| Address | `0x008ed4d0` |
| Size | 51 elements × 892 bytes each (PSXTexture) |
| Init Chain | `TextureArray_Init` → `TextureArray_ConstructElements` → `_eh_vector_constructor_iterator_` |
| Cleanup | Registered via `_atexit(TextureArray_Cleanup)` |

### Viewport Array (`g_ViewportArray`)

| Property | Value |
|----------|-------|
| Address | `0x008ed030` |
| Size | 16 elements × 64 bytes each (CMarniViewport2) |
| Init Chain | `ViewportArray_Init` → `ViewportArray_ConstructElements` |
| Details | Constructor sets vtable + zeros 13 fields, sets `field[7] = 1` |

### Page Table Array (`DAT_008eca00`)

| Property | Value |
|----------|-------|
| Address | `0x008eca00` |
| Size | 9 elements × 176 bytes each (2 × CMarniBits per element) |
| Init Chain | `PageTableArray_Init` → `PageTableArray_ConstructElements` |

### Initialization Functions Grouped

| Group | Init Function | Constructor | Register Cleanup | Cleanup Function |
|-------|---------------|-------------|------------------|------------------|
| Texture Array | `TextureArray_Init` | `TextureArray_ConstructElements` | `TextureArray_RegisterCleanup` | `TextureArray_Cleanup` |
| Viewport Array | `ViewportArray_Init` | `ViewportArray_ConstructElements` | `ViewportArray_RegisterCleanup` | `ViewportArray_Cleanup` |
| Page Table | `PageTableArray_Init` | `PageTableArray_ConstructElements` | `PageTableArray_RegisterCleanup` | `PageTableArray_Cleanup` |
| Global Bits | `GlobalMarniBits_Init` | `GlobalMarniBits_Constructor` | `GlobalMarniBits_RegisterCleanup` | `GlobalMarniBits_Cleanup` |
| Global Viewport | `GlobalViewport_Init` | `GlobalViewport_Constructor` | `GlobalViewport_RegisterCleanup` | `GlobalViewport_Cleanup` |

### Single Object Globals

| Instance | Type | Init Function | Description |
|----------|------|---------------|-------------|
| Global work buffer | CMarniBits | `GlobalMarniBits_Init` | Reusable work buffer for texture ops |
| Global viewport | CMarniViewport2 | `GlobalViewport_Init` | Standalone viewport object |

Both register cleanup via `_atexit`.

---

## 7. ExecAsync — Async Task System

Texture and object creation/deletion use `ExecAsync` to queue work items that run on the main thread:

```
create_texture_page()   → ExecAsync(AsyncCreateTexturePage)
destroy_texture_page()  → ExecAsync(AsyncDestroyTexturePage)
FUN_0046c230()          → ExecAsync(AsyncCreateObject)
FUN_0046c280()          → ExecAsync(AsyncDeleteObject)
```

The async queue processes items on the main thread during the render loop, ensuring D3D11 API calls happen on the correct thread.

---

## 8. Data Flow Diagram

```
TIM/PIX File (PS1 Texture)
    ↓
PSXTexture::LoadFromFile() → PSXTexture::Store()
    ↓ Parse TIM header, extract pixel data + CLUT
CMarniBits::SetAddress() / CMarniBits::CopyFrom()
    ↓ Copy to work buffer
create_texture_page() → ExecAsync() → CMarniDirect3D::vtable[6] CreateTextureHandle()
    ↓
D3D11 Texture2D + SRV → g_TexturePageSRV[256]
    ↓
══════════════════════════════════════════
Game Logic (Tasks) per frame
    ↓
AddSprite / draw_texture / AddTintSprite / SubmitEffectSprite
    ↓ PS1 GPU packet → SpriteCommand
g_SpriteCommandBuffer[300]
    ↓ Queue all sprites for this frame
FlushSpriteCommands()
    ↓ Scale to screen coords, lookup SRV by slot
MarniDrawSprite() → D3D11 Map/Unmap quad VB, DrawIndexed
    ↓
IDXGISwapChain::Present()
    ↓
Screen
```

---

## 9. Source File Map

| File | Contents |
|------|----------|
| `src/marni/MarniSystem.h` | CMarniDirect3D class declaration, global Marni API |
| `src/marni/MarniSystem.cpp` | CMarniDirect3D vtable impl, D3D11 device, HLSL shaders |
| `src/marni/MarniBits.h` | CMarniBits class declaration (24 methods) |
| `src/marni/MarniBits.cpp` | CMarniBits vtable + surface operations |
| `src/marni/PSXTexture.h` | PSXTexture class declaration, CLUT management |
| `src/marni/PSXTexture.cpp` | PSXTexture TIM/PIX parser, Store, operator= |
| `src/marni/Marni3DObject.h` | CDirect3DObject, CMarniExecuteBuffer, CMarniPolyhedra, CMarniDirect3DTMD, CMarniViewport2 |
| `src/marni/MarniInput.h` | CMarniDirectInput class, MasterInputState struct |
| `src/marni/Marni3DObject.cpp` | 3D object class implementations (all vtable + non-virtual methods) |
| `src/marni/MarniInput.cpp` | DirectInput keyboard/joystick polling |
| `src/game/TextureLoader.cpp` | Texture page creation, async workers, page management |
| `src/game/SpriteRenderer.h` | SpriteCommand struct, OT entry, sprite functions |
| `src/game/SpriteRenderer.cpp` | PSYQ GPU sprite emulation (19 functions) |
| `src/Globals.h` | Global variable declarations for all Marni subsystems |
| `src/marni/MarniDX.h` | Backend interface: device/shader setup, texture create/destroy, DrawRect/DrawTriangles*, adapter enumeration |
| `src/marni/MarniDX.cpp` | The actual DX11 implementation behind every vtable method |
| `src/marni/MarniSound.h/.cpp` | DirectSound-compatible sound API on XAudio2 (banks, play_sfx, fade/decay live in `src/game/SoundSystem.*`) |

Every original address in the `0x0041xxxx-0x0049xxxx` Marni region is either
implemented above or triaged out of scope (compiler SEH/static-init glue,
raw-API plumbing replaced by MarniDX) - see `tools/progress_report.py`.

---

## 10. PSYQ-to-DirectX Mapping

| PSYQ Operation | Original D3D5 Implementation | Modern D3D11 Implementation |
|----------------|------------------------------|-----------------------------|
| `LoadImage(cr, rect, img)` | `CMarniBits::Blt()` → `IDirectDrawSurface::Blt()` | `CMarniBits::Blt()` → CPU pixel copy |
| `StoreImage(cr, rect, img)` | `CMarniBits::Lock()` → `IDirectDrawSurface::Lock()` | `CMarniBits::Lock()` → CPU buffer access |
| `DrawPrim(prim)` | PSYQ GPU emulation → OT → D3D5 draw | `AddSprite()` → `SprCmd` → `MarniDrawSprite()` → D3D11 |
| `AddPrim(spr, ot)` | `AddSprite()` / `AddTintSprite()` → sprite queue | Same pattern → `g_SpriteCommandBuffer[300]` |
| `DrawOTag(ot)` | Flush sprite queue → batch draw | `FlushSpriteCommands()` → batch `MarniDrawSprite` |
| `LoadImage(tim)` | `CMarniBits::Blt()` → surface load | `PSXTexture::Store()` → `create_texture_page()` → D3D11 SRV |
| `ResetGraph(mode)` | Reset OT, clear buffers | `SpriteQueue_Reset()`, `MarniClear()` |
| `MoveImage(cr, x, y)` | `IDirectDrawSurface::Blt()` with DDBLT_WAIT | `CMarniBits::BltFast()` / CPU pixel copy |
| `LoadTPage(p, p, t)` | `SetTexture` → `IDirect3DDevice3::SetTexture()` | `MarniDrawSprite()` with SRV from `g_TexturePageSRV[]` |

### Key Differences Between Original and Modern Implementation

1. **Surface Management**: Original used `IDirectDrawSurface` COM objects for all surface operations. Modern uses CPU-side pixel buffers (CMarniBits) for pixel manipulation and D3D11 textures only for GPU rendering.

2. **Sprite Rendering**: Original used `IDirect3DDevice3::DrawPrimitive` with execute buffers. Modern uses a single shared quad vertex buffer, mapped each frame with sprite vertices, drawn via `DrawIndexed`.

3. **Texture Atlas**: Original stored textures in a PS1-style 256-slot "texture page" table on VRAM. Modern stores them as D3D11 shader resource views in `g_TexturePageSRV[256]`.

4. **Palette Handling**: Original CLUT was stored as a D3D palette object. Modern converts indexed pixel data to RGBA on CPU upload, eliminating the need for palette textures.

5. **Async Creation**: Original used the Marni async system to defer texture creation to the main thread (D3D APIs must be called from the creation thread). This pattern is preserved with `ExecAsync`.

6. **Software rasterizers retired**: the original's CPU pixel pipelines -
   CMarniBits triangle/gouraud/gradient-line rasterizers, sprite blitters
   with CLUT and color-multiply paths, fill rects, and the TransAlpha
   polygon/rect execute-buffer paths - are all replaced by the GPU draws of
   `MarniDX::DrawTriangles*` / `MarniDX::DrawRect`. Their original functions
   are documented in Ghidra (names like `bits_sprite_blit_dispatch`,
   `d3d_transalpha_polygon`) but intentionally not ported.

7. **Driver/adapter enumeration**: the original walked DirectDraw/D3D5
   enumeration callbacks into a 0x11C-byte-stride table at `0x007e0e10`
   (`md3d_detect_drivers`, `d3d_enum_mode_callback`). Modern:
   `EnumerateD3DRenderers` (0x004977f0) fills `g_D3DRenderers` via DXGI, and
   `GetDirect3DDriverCount/Name` (0x004486e0/0x00448710) read it back with
   their original guards ("Direct3D::RequestDriverCount" /
   "Direct3D::RequestDriverName").
