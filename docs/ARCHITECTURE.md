# Resident Evil 1 PC - Architecture Documentation

This document describes the overall architecture of Resident Evil 1 PC, including the Marni System, rendering pipeline, game loop structure, and key subsystems.

---

## Table of Contents

1. [Overview](#overview)
2. [Marni System (Modern D3D11 Port)](#marni-system-modern-d3d11-port)
3. [Rendering Pipeline](#rendering-pipeline)
4. [Sprite System](#sprite-system)
5. [Font & Text Rendering](#font--text-rendering)
6. [Texture Loading (PSX TIM/PIX)](#texture-loading-psx-timpix)
7. [Game Loop Structure](#game-loop-structure)
8. [Task Scheduler](#task-scheduler)
9. [Display Configuration](#display-configuration)
10. [Installation and Registry](#installation-and-registry)
11. [Memory Management](#memory-management)
12. [Subsystem Architecture](#subsystem-architecture)

---

## Overview

Resident Evil 1 PC is a port of the PlayStation original, released in 1997. The game uses a custom wrapper system called **Marni System** that provides a PSYQ-compatible API (PlayStation SDK) implemented on top of DirectX.

This decompilation project ports the original DirectX 5.0 implementation to **Direct3D 11**, **XAudio2**, and **XInput** for compatibility with modern Windows. The original PSYQ API surface is preserved where possible.

### Project Status

**Functional-complete.** The entire original game is playable - every room,
enemy, boss, cutscene, FMV, menu, save system and ending - and behaves like
the 1997 release under extensive playtesting.

Measured against the Ghidra project (**2393 functions** in the original
binary):

| Stream | Count | Status |
|---|---|---|
| Game logic implemented in `src\` | 1716 of 1717 | done |
| Marni System DirectX internals -> DX11 layer | 84 of 84 | done |
| CRT / MSVC runtime (provided by toolchain) | 290 | out of scope |
| Compiler SEH / static-init glue (absorbed by real C++) | 177 | out of scope |
| Raw D3D5 API paths (replaced by MarniDX) | 108 | out of scope |
| Software-FMV shared-memory player (replaced by native MCI) | 11 | out of scope |
| Import thunks (loader-provided) | 6 | out of scope |

In-scope coverage: **1800 / 1801 functions = 99.9%**. The single documented
remainder is a trivial animation/velocity setter whose caller is undefined
even in Ghidra. Regenerate this table any time with
`tools/progress_report.py <ghidra_dump.txt> src`.

### Key Architectural Decisions

1. **PSYQ Compatibility Layer**: The Marni System allows most PlayStation code to run on Windows with minimal changes
2. **32-bit Architecture**: The game is strictly 32-bit, using Win32 API
3. **Modern Graphics Backend**: DirectX 5.0 → Direct3D 11 with HLSL shaders, quad-based sprite rendering
4. **Task-Based Game Logic**: Game logic is organized into tasks that are scheduled each frame
5. **Fixed Resolution Rendering**: Base game resolution is 320×240, scaled to display resolution
6. **Sprite Queue System**: All 2D rendering goes through a pending sprite queue, rendered during `game_frame_present`
7. **Address Traceability**: every rewritten function comments its original Ghidra address; globals comment their original variable address, so any line traces back to the binary

The port is **function-complete** (99.9% of in-scope functions); remaining work is behavioral polish, not missing code.

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      WinMain Entry Point                     │
│                      (0x00441350)                            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    System Initialization                     │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ System      │  │ Installation │  │ Display          │   │
│  │ Checks      │  │ Verification │  │ Configuration    │   │
│  └─────────────┘  └──────────────┘  └──────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Marni System Init                         │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ D3D11       │  │ XAudio2      │  │ XInput           │   │
│  │ Wrapper     │  │ Wrapper      │  │ Wrapper          │   │
│  └─────────────┘  └──────────────┘  └──────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Main Message Loop                       │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  PeekMessage → Translate/Dispatch → main_loop()     │    │
│  │                                            │         │    │
│  │  ┌──────────────────────────────────────────┴───┐    │    │
│  │  │  Input → Tasks → Rendering → Present        │    │    │
│  │  └──────────────────────────────────────────────┘    │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Marni System (Modern D3D11 Port)

The Marni System is Capcom's PSYQ-to-DirectX wrapper. The original used DirectX 5.0 (DirectDraw/Direct3D 5/DirectSound/DirectInput). This decomp port replaces it with modern equivalents.

### Purpose

The Marni System was created to:
1. Port PlayStation games to Windows with minimal code changes
2. Provide a consistent API across different hardware
3. Handle the PS1 ↔ PC graphics/sound architecture differences

### Core Components

#### 1. CMarniDirect3D (Main Graphics Class)

**File:** `src/marni/MarniSystem.h`, `src/marni/MarniSystem.cpp`
**Original size:** `0x21DC` bytes (8676 bytes)
**VTable:** `0x004af230`

```
┌─────────────────────────────────────────────────────────────┐
│                    CMarniDirect3D                           │
├─────────────────────────────────────────────────────────────┤
│ VTable Functions:                                           │
│   [0] RequestVideoMemory    - Query available video RAM     │
│   [1] ChangeDisplayMode     - Change resolution/mode        │
│   [2] SetD3DRenderer        - Select D3D renderer           │
│   [3] Clear                 - Clear render target           │
│   [4] Present               - Swap buffers                  │
│   [5] HandleWindowMessage   - Handle WM_ACTIVATE etc.       │
│   [6] CreateTextureHandle   - Create GPU texture            │
│   [7] CreateObjectHandle    - Create 3D render object       │
│   [8] DeleteTextureHandle   - Delete GPU texture            │
│   [9] DeleteObjectHandle    - Delete 3D render object       │
│  [10] SetTexture            - Submit sprite for rendering   │
│  [11] ResetTextures         - Reset texture bindings        │
├─────────────────────────────────────────────────────────────┤
│ Modern D3D11 Members (replacing original DD/D3D5):          │
│   ID3D11Device*           m_pD3DDevice                      │
│   ID3D11DeviceContext*    m_pD3DContext                     │
│   IDXGISwapChain*         m_pSwapChain                      │
│   ID3D11RenderTargetView* m_pRenderTargetView               │
│   ID3D11VertexShader*     m_pQuadVS       (sprite VS)       │
│   ID3D11PixelShader*      m_pQuadPS       (sprite PS)       │
│   ID3D11Buffer*           m_pQuadVB       (reusable VB)     │
│   ID3D11Buffer*           m_pSpriteCB     (MVP constant)     │
│   ID3D11BlendState*       m_pBlendAlpha   (alpha blend)     │
│   ID3D11SamplerState*     m_pSamplerLinear                  │
│   ID3D11SamplerState*     m_pSamplerPoint  (pixelated fonts)│
│   ID3D11DepthStencilState*m_pDepthDisabled (2D rendering)   │
│   ID3D11ShaderResourceView* m_pFontSRV    (font atlas)      │
│   ID3D11ShaderResourceView* m_pWhiteSRV   (1×1 white fill)  │
│   int m_FontTexWidth, m_FontTexHeight                       │
├─────────────────────────────────────────────────────────────┤
│ Original fields preserved for compatibility:                │
│   +0x10: m_width (DWORD)                                    │
│   +0x14: m_height (DWORD)                                   │
│   +0x18: m_bitDepth (DWORD)                                 │
│   +0x3C: m_isInitialized (BOOL)                             │
│   +0x68: m_isFullScreen (BOOL)                              │
│   +0x74: m_isActive (BOOL)                                  │
│  +0x30C: m_deviceType (DWORD)                               │
└─────────────────────────────────────────────────────────────┘
```

#### 2. CMarniBits (Texture/Surface Class)

**File:** `src/marni/MarniBits.h`
**VTable:** `0x004af008`

Handles surface operations:
- `Blt()` / `BltFast()` — Surface blitting (pixel copy with format conversion)
- `Lock()` / `Unlock()` — Direct surface access
- `SetAddress()` — Set pixel/palette data pointers
- `CopyFrom()` — Copy surface data
- `Release()` — Free owned pixel/palette memory

#### 3. PSXTexture (PS1 TIM/PIX Loader)

**File:** `src/marni/PSXTexture.h`, `src/marni/PSXTexture.cpp`

Parses PS1 texture formats and manages CLUT (Color Look-Up Table) entries:
- `Store(int* imageData, int copyData)` — Parse TIM/PIX from buffer
- `LoadFromFile(const char* filename)` — Open and load TIM from disk
- `CopyFrom(PSXTexture* src)` — Copy all 8 embedded CLUT entries
- 8 embedded CMarniBits sub-objects at 0x68-byte intervals (slots for multi-CLUT)

#### 4. Marni System Helpers

| Function | Description |
|----------|-------------|
| `MarniDrawSprite(x,y,w,h,u0,v0,u1,v1,color,srv)` | Draw textured quad at screen coords |
| `MarniDrawRect(x,y,w,h,color)` | Draw solid-color rectangle |
| `MarniCreateTexture(w,h,bpp,data,&tex,&srv)` | Create D3D11 texture from pixel data |
| `MarniClear()` | Clear render target (→ vtable[3]) |
| `MarniPresent()` | Present frame (→ vtable[4]) |

### Quad-Based Sprite Rendering

All 2D rendering uses a reusable quad vertex buffer:

```
QuadVertex structure:
  float x, y          → Screen position
  float u, v          → Texture coordinates
  float r, g, b, a    → Color tint (0.0–1.0)
```

The vertex shader transforms positions with an orthographic projection matrix (screen coords with top-left origin). The pixel shader samples the texture and multiplies by the vertex color, enabling tinted text, fading, and alpha blending.

### PSYQ → Modern API Mapping

| PSYQ Function | Original DirectX 5 | Modern D3D11 |
|---------------|-------------------|--------------|
| `DrawPrim()` | `IDirect3DDevice::DrawPrimitive()` | `ID3D11DeviceContext::Draw()` |
| `LoadImage()` | `CMarniBits::Blt()` | (unused; TIM→D3D11 via `MarniCreateTexture`) |
| `StoreImage()` | `CMarniBits::Lock()` | (unused; parsed directly) |
| `AddPrim()` | vtable[10] (SetTexture) | `AddTintSprite()` → pending sprite queue |
| Sprite present | vtable[4] (Present) | `MarniPresent()` → `IDXGISwapChain::Present()` |

---

## Rendering Pipeline

### Per-Frame Rendering Flow

```
┌───────────────────────────────────────────────────────────────┐
│                      main_loop()                              │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  1. InputUpdate() / PlayerPad_Update()                        │
│                         │                                      │
│  2. State checks (fade, FMV, special)                         │
│                         │                                      │
│  3. TaskScheduler_Update()                                    │
│     │                                                        │
│     ├─► Task functions call:                                 │
│     │   ├─ PrintText8x14() → AddTintSprite()                 │
│     │   ├─ display_texture()                                  │
│     │   └─ draw_rect()                                       │
│     │                                                        │
│     │   Each queues PendingSprite entries                     │
│     │   in g_pendingSprites[]                                 │
│     │                                                        │
│  4. Screen effects (shake, fade rects via draw_rect)         │
│                         │                                      │
│  5. FrameRateGovernor()                                      │
│     │                                                        │
│     ├─ 5a. MarniClear()                                      │
│     │      Clear render target to dark blue-gray             │
│     │                                                        │
│     ├─ 5b. OT_InsertPrimitive() — insert title BG           │
│     │      into g_pendingSprites[0]                               │
│     │                                                        │
│     ├─ 5c. Sort g_pendingSprites by depth                        │
│     │      5d. Phase 1: pending sprites depth >= 500                  │
│     │        (background, pause overlays)                     │
│     │      5e. Phase 2: FlushSpriteCommands()                          │
│     │                                                        │
│     └─ 5f. Phase 3: pending sprites depth < 500                                    │
│            (fade overlays, color tinting, room lighting)                                 │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Key Rules

1. **No direct D3D11 calls from task functions** — Tasks run during step 3. If they called `MarniDrawSprite` directly, the draws would be wiped by `MarniClear` in step 5a. All rendering MUST go through the pending sprite queue.

2. **Sprite queue renders after Clear** — The queue is rendered in step 5c, AFTER `MarniClear`. This ensures all queued sprites are visible.

3. **Scaling**: Game coordinates (320×240 base resolution) are scaled to screen resolution inside each sprite-builder function (`AddTintSprite`, `draw_rect`, `display_texture`) before queueing.

4. **Depth-split rendering**: `FrameRateGovernor` renders in three phases to match the original game's depth-sorted ordering:
   - **Phase 1** (depth ≥ 500): Background and scene elements from `g_pendingSprites` (e.g., title BG at 0xFFF, pause overlays at 2100)
   - **Phase 2**: Game objects and text from `g_SpriteCommandBuffer` via `FlushSpriteCommands` (e.g., title text at 532)
   - **Phase 3** (depth < 500): Screen effects from `g_pendingSprites` (e.g., fade overlays at 450, color tinting at 490, room lighting at 470–499)

   This ensures fade overlays correctly cover text and game objects, matching the original game where all sprites shared one depth-sorted command buffer.

### Pending Sprite Queue

**File:** `src/game/Rendering.cpp`

The `g_pendingSprites[]` queue is a depth-sorted array used by `AddTintSprite` (text), `draw_rect` (menu rectangles, fade overlays), and `OT_InsertPrimitive` (title screen background). Sprites are sorted by `depth` in `FrameRateGovernor` and rendered in split passes (see rule 4 above).

```cpp
#define MAX_PENDING_SPRITES 300

struct PendingSprite {
    float x, y, w, h;            // Screen-space position & size
    float u0, v0, u1, v1;         // Texture UV coordinates
    DWORD color;                  // RGBA color (A in high byte)
    ID3D11ShaderResourceView* srv;// Texture to sample
    BOOL valid;                   // Set to TRUE when queued
    unsigned int depth;           // OT depth sort value (lower = closer = on top)
};

static PendingSprite g_pendingSprites[MAX_PENDING_SPRITES];
static int 5e. Phase 2: FlushSpriteCommands();
```

### GDI/Sprite Command Buffer

**File:** `src/game/SpriteRenderer.cpp`

The original `g_SpriteCommandBuffer[600]` (double-buffered) handles textured quads via `FlushSpriteCommands()`. Each command has type=10 (textured quad) and includes position, UV, color, depth sorting, and texture page references. Commands are rendered through `MarniDrawSprite` after the `g_pendingSprites` queue.

### MarniClear / MarniPresent

- `MarniClear` → `CMarniDirect3D::vtable[3]` → `ClearRenderTargetView()` with `g_debugClearR/G/B` (dark blue-gray, ~5% brightness)
- `MarniPresent` → `CMarniDirect3D::vtable[4]` → `IDXGISwapChain::Present(1, 0)` (VSync)

---

## Sprite System

The original game uses a sprite command buffer (`g_SpriteCommandBuffer[600]`, double-buffered) to queue 2D primitives. Each sprite command (type 10 = textured quad) is built by builder functions, then submitted via vtable[10] (`SetTexture`).

In the modern port, this is replaced by the `PendingSprite` queue. The builder functions have been reimplemented to follow the original Ghidra decompilation while targeting the new queue.

### Builder Functions

#### AddTintSprite (0x0046e0a0)

The primary tinted-sprite builder. Used by all font rendering functions.

**Parameters:** `(void* pak, unsigned short brightness)`

Reads from the [`TexturePrintState`](#textureprintstate-struct) struct to determine:
- Screen position (`printPosX/Y + g_ScreenOffsetX/Y`)
- Character size (`vramWidth × vramHeight`, e.g. 8×14 for 8x14 font)
- Texture UV position (`vramAreaX/Y` in font atlas)
- Tint color (`tintR/G/B`, brightness → alpha)

Builds a `PendingSprite` and increments `g_pendingSpriteCount`.

#### draw_rect (0x00470350)

Draws a solid-color rectangle. Used for menu backgrounds, fade overlays, color tinting, and debug screens. Implements the original game's `GetTextureVariant` blend modes via the `textureId` field.

**Parameters:** `(RectDrawDesc* rect, int blend, int flags)`

- **Position**: `rect.x/y + g_ScreenOffsetX/Y`, scaled from 320×240 to screen
- **Size**: `rect.w × rect.h`, scaled
- **Color/Alpha**: Determined by `GetTextureVariant(rect->textureId)`:
  - **Variant 0** (textureId = 0): Fully opaque fill (alpha=255) — used by `g_window_rect`, pause screens
  - **Variant 1** (textureId = `0x40000000`): Semi-transparent tinted overlay — used by special room lighting
  - **Variant 2** (textureId = `0x50000000`): White flash — r=g=b=255, alpha = max(r,g,b) from brightness — used by `fade_type_id=1`
  - **Variant 3** (textureId = `0x60000000`): Black fade — r=g=b=0, alpha = max(r,g,b) from brightness — used by `fade_type_id=2`
  - When alpha = 0, the draw is skipped (background shows through)
- **blend**: Controls OT depth sort — `flags==0` → `blend + 450`, else `blend * 16 + 500`
- **Depth < 500**: Rendered in Phase 3 (after game objects/text), so fade overlays cover everything beneath them

#### display_texture (0x0046e8d0)

Queues a textured sprite using the texture page table. Used for title screen button prompts and other texture-atlas sprites.

**Parameters:** `(void* buffer, unsigned short depth, int slot, int pageCount)`

Looks up `g_TexturePageSRV[slot + 0xF]` and builds a `PendingSprite` with the UV region defined by `g_TextureVramX/Y`, `g_TexturePrintX/Y`, and `g_titleCurrentSprH`.

#### AddSprite (0x0046ddc0)

Full-featured sprite builder with texture page variant selection, depth sorting, and reverse fade. Used for in-game sprite rendering (not yet fully implemented in the modern port).

**Parameters:** `(uint* pak, short depth, int tpage, int fade)`

AddSprite supports:
- Multiple texture page variants (via CLUT/page offset tables)
- Reverse fade flag (`g_ReverseFadeFlag`)
- Depth sort override (`g_DepthSortOverride`)
- Texture variant checking via `GetTextureVariant()`

---

## Font & Text Rendering

All font rendering uses a single font texture atlas (`fontus.tim`, loaded into `m_pFontSRV`). The atlas contains glyphs in multiple layouts for different font sizes.

> **Text encoding:** game text is not ASCII — it is an index-based encoding
> into the 8×14 font region. The `STR()` compile-time encoder and the encoded
> string tables (item names, global messages, item descriptions) are documented
> in [TEXT_ENCODING.md](TEXT_ENCODING.md).

### PrintText8x14 (0x00455520)

8×14 pixel mono-spaced font. Characters laid out **18 per row** by raw ASCII value.

```
Glyph layout:  col = ch % 18,  row = ch / 18
Atlas cell:    8×14 pixels per glyph
Special chars: '(' = col 56,row 224   ')' = col 70,row 224
```

1. Computes brightness from `color >> 4` (clamped to [2, 30])
2. Sets `TexturePrintState` globals (char size 8×14, VRAM area, tint white)
3. If `flags ≠ 0`: draws shadow pass (offset +1,+1, black tint, brightness=10)
4. Iterates `PRINT_TEXT_BUFFER`, computing atlas position per char, calling `AddTintSprite()`
5. Darkness override: `STAGE_ID==3 && ROOM_ID==17 && camera==4` → brightness=0 (invisible)

### PrintText8x8 (0x00455420)

8×8 pixel mono-spaced font. Characters offset from ASCII 0x20 (space).

```
Glyph layout:  col = (ch - 0x20) * 8  (byte-wrapped)
               row = ((ch - 0x20) & 0xE3) >> 2   (≈ index / 4)
Atlas cell:    8×8 pixels per glyph
Shadow:        CLUT tint + 8 (darker palette index)
```

Same `AddTintSprite` pipeline as PrintText8x14, with char size 8×8.

### PrintFormattedText (0x00455190)

Control-code-based formatted text renderer for debug/menu output.

**Parameters:** `(short x, short y, unsigned char color, unsigned char* data)`

Iterates a byte stream with opcodes:

| Opcode | Description |
|--------|-------------|
| `0x00` | Advance X by 8 (space) |
| `0x01`/`0x07` | Return |
| `0xF8` | Next byte = char, TEXTURE_DEPTH=0x1E, row = next/18 + 15 |
| `0xF9` | Next byte = char, TEXTURE_DEPTH=0x1F, row = next/18 |
| `0xFA` | Next byte = char, TEXTURE_DEPTH=0x1F, row = next/18 + 14 |
| `0xFB` | No-op (advance pointer) |
| `0xFF` | Advance X by 4 (half-width space) |
| *default* | TEXTURE_DEPTH=0x1E, col = ch%18, row = ch/18 + 2 |

Used extensively by `LoadSaveGameState` for menu rendering.

The struct is exactly `0x22` (34) bytes — verified to match the original binary. Individual fields are exposed as macros for backward compatibility:

```cpp
#define g_TexturePrintX   g_texPrintState.printPosX
#define g_TextureVramX    g_texPrintState.vramAreaX
#define g_PrintTintR      g_texPrintState.tintR
// ... etc.
```

**Global instance:** `extern TexturePrintState g_texPrintState;`

### Font Loading

The font texture (`fontus.tim`) is loaded by `ProcessTextureImage()` (0x0046c5f0):

1. `LoadFile(".\\usa\\data\\fontus.tim", buffer, 0x20)` — loads raw TIM bytes
2. `PSXTexture::Store(buffer, 1)` — parses TIM header + CLUT + pixel data
3. Converts 4/8 bpp paletted pixels → RGBA8888 via CLUT
4. Creates D3D11 texture & SRV: `MarniCreateTexture(w, h, 32, rgba, &fontTex, &fontSRV)`
5. Stores SRV in `pD3D->m_pFontSRV`, dimensions in `pD3D->m_FontTexWidth/Height`

The bank ID `0x1E` (30) is the font bank — this is checked by the VRAM area in each text renderer (`TEXTURE_DEPTH = 0x1E`).

---

## Texture Loading (PSX TIM/PIX)

### File Formats

**TIM (Tagged Image Format):**
```
Offset 0x00: Magic number (0x00000010)
Offset 0x04: Flags (bit 3 = has CLUT)
If CLUT present:
  Offset 0x08: CLUT data size
  Offset 0x0C: CLUT origin X (high word), Y (low word)
  Offset 0x10: CLUT width (high), height (low)
  Offset 0x14: CLUT color data (size - 12 bytes)
Image section:
  Offset +0:  Image data size
  Offset +4:  Image origin X/Y
  Offset +8:  Image width (16-bit words), height
  Offset +12: Pixel data
```

**PIX:** Same as TIM without the magic header. Raw pixel data.

### Bit Depths

| Flag | BPP | Description |
|------|-----|-------------|
| 0 | 4 | 16-color CLUT palette |
| 1 | 8 | 256-color CLUT palette |
| 2 | 16 | Direct RGB555 color (no CLUT) |

### Loading Pipeline

#### LoadTexturePage (0x0046c870)

General-purpose texture loader:
1. `PSXTexture::Store(imageBuffer, 1)` — parse TIM
2. Store descriptor in `g_VideoDriverArray_838[slot * 0x37C]`
3. Convert paletted pixels → RGBA8888 via CLUT
4. `MarniCreateTexture(w, h, 32, rgba, &tex, &g_TexturePageSRV[slot + 0xF])`
5. Store dimensions in `g_TexturePageWidth[slot + 0xF]`, `g_TexturePageHeight[slot + 0xF]`

**Slot indexing:** The `slotIndex` parameter is shifted by `+0xF` internally. The `texCheckOffset` uses `slotIndex * 0xDF` for video driver array lookups.

#### ProcessTextureImage (0x0046c5f0)

Auto-positioned loader for font/bank textures:
1. Computes VRAM label position from `textureBankID`:
   - Bank ≥ 0x10: baseX=0x400, baseY=0x100
   - Bank < 0x10: baseX=0, baseY=0
   - Final: `labelX = bankID * 0x40 - baseX`, `labelY = baseY`
2. Creates texture page with mode=2
3. Special handling for bank 0x1E (font): creates `m_pFontSRV` directly on the D3D device

#### LoadShadowMaskTexture (0x0046ccd0)

Shadow/mask loader:
1. Reads palette from image offset +0x14
2. Replaces non-transparent, non-zero colors with black (0x3DEF)
3. Creates texture pages with mode=1 (shadow/alpha blend)

#### display_image (0x00470770)

Title screen background image loader:
1. Takes raw 16-bit PS1 pixel data (ABGR1555)
2. Converts to RGBA8888
3. Creates D3D11 texture → `g_titleImageSRV`
4. Rendered as full-screen background in `game_frame_present()` when debug overlay flag is clear

### CLUT Conversion (ABGR1555 → RGBA8888)

PS1 palette entries are 16-bit ABGR1555:
```
Bit 15:     Alpha flag (0 = opaque, 1 = semi-transparent)
Bits 10-14: Blue  (0–31)
Bits 5-9:   Green (0–31)
Bits 0-4:   Red   (0–31)
```

Converted to RGBA8888 per channel: `(value * 255) / 31`

In `LoadTexturePage`, index 0 of the CLUT is always treated as fully transparent (`alpha=0`), matching PS1 behavior where palette index 0 is the transparent color for 4/8 bpp textures.

---

## Game Loop Structure

### Main Loop Flow

The game loop is implemented in `main_loop()` at `0x00428eb0`.

```
┌─────────────────────────────────────────────────────────────┐
│                      main_loop()                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ 1. INITIALIZATION (first run only)                   │   │
│  │    - init_and_start_game()                           │   │
│  │    - Set init_game_flag = 1                          │   │
│  └──────────────────────────────────────────────────────┘   │
│                         │                                    │
│                         ▼                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ 2. INPUT UPDATE                                      │   │
│  │    - InputUpdate()                                   │   │
│  │    - PlayerPad_Update()                              │   │
│  │    - Check for special key combinations              │   │
│  └──────────────────────────────────────────────────────┘   │
│                         │                                    │
│                         ▼                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ 3. STATE BRANCHING                                   │   │
│  │    ┌─────────────────────────────────────────────┐   │   │
│  │    │ g_main_state_flags & 0x40000?               │   │   │
│  │    │  YES → FMV Playback path                    │   │   │
│  │    │  NO  → Normal game path                     │   │   │
│  │    └─────────────────────────────────────────────┘   │   │
│  └──────────────────────────────────────────────────────┘   │
│                         │                                    │
│           ┌─────────────┴─────────────┐                     │
│           ▼                           ▼                      │
│  ┌─────────────────┐        ┌─────────────────┐             │
│  │ FMV Playback    │        │ Normal Game     │             │
│  │ - draw_rect     │        │ - Task update   │             │
│  │ - StMask()      │        │ - Menu handling │             │
│  │ - UpdateVideo   │        │ - Fading        │             │
│  │   Playback()    │        │ - Room lighting │             │
│  │   (polls input  │        │ - Screen shake  │             │
│  │    itself)      │        │ - Debug overlay │             │
│  └─────────────────┘        └─────────────────┘             │
│                         │                                    │
│                         ▼                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ 4. FRAME END                                         │   │
│  │    - game_frame_present()                            │   │
│  │      MarniClear → Draw sprites → MarniPresent        │   │
│  │    - Update timers                                   │   │
│  │    - Return 1 (continue game loop)                   │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Game State Flags

| Bit | Mask | Description |
|-----|------|-------------|
| 16 | `0x10000` | Pause screen active |
| 17 | `0x20000` | Menu active |
| 18 | `0x40000` | FMV playback active (triggers `UpdateVideoPlayback()` in main loop instead of `main_loop()`; FMV state machine polls its own input) |
| 19 | `0x80000` | Reset screen panning |
| 23 | `0x800000` | Special room lighting |
| 29 | `0x20000000` | Fade/pause state transition |
| 30 | `0x40000000` | Debug overlay mode (blocks title image rendering) |
| 31 | `0x80000000` | Alternate overlay mode |

### Fading State Machine

The fading system uses signed integer arithmetic:

- `g_fading_state < 0`: Fading out (increasingly transparent)
- `g_fading_state >= 0`: Fading in (increasingly opaque)
- `g_fading_state = 0x7FFF` (32767): Fully faded (complete)
- `g_fading_state = -1`: Initial state (no fade)
- `g_fading_counter`: Signed step value per frame (negative = fade out, positive = fade in)

**Important:** Many fade counter values in the original are 16-bit signed hex literals that must be sign-extended to 32-bit in the C++ port:
- `0xFC00` → `(short)0xFC00` = -1024
- `0xF000` → `(short)0xF000` = -4096
- `0x7F00` → = 32512 (positive, no cast needed)

### Frame Timing

The game targets approximately 60 FPS (16ms per frame). Frame skipping logic in `RunMessageLoop()` throttles the loop when the frame time is below the target.

---

## Task Scheduler

**File:** `src/game/TaskScheduler.cpp`
**Documentation:** `docs/task_scheduler.md`

The task scheduler is the core of the game's cooperative multitasking system. Game logic runs as coroutine-like tasks that yield (via `Task_sleep`) and resume on subsequent frames.

### Key Functions

| Function | Description |
|----------|-------------|
| `TaskScheduler_Init()` | Initialize the 3-slot task table |
| `TaskScheduler_Update()` | Run the current task slot each frame |
| `TaskScheduler_Reset()` | Clear all task slots |
| `Task_execute(id, func)` | Start a new task in a slot |
| `Task_sleep(frames)` | Suspend current task for N frames |
| `Task_chain(func)` | Replace current task with a new function |
| `Task_exit()` | Terminate current task |

### Task Slots

The scheduler maintains 3 task slots (`g_TasksTable[3]`), each supporting one active task at a time. Tasks save/restore context (stack pointer, instruction pointer) for cooperative switching.

### Task Flow (Startup)

```
init_and_start_game()
  │
  └─► Task_execute(0, load_global_assets)
        │
        ├─ Initialize MarniSystem (D3D11 + XAudio2)
        ├─ Load item images, font textures, status screen textures
        ├─ Create shadow texture quad
        │
        └─► Task_chain(debug_state)          ← SFX Player debug menu
              │                                 Press ESC to continue
              │
              └─► Task_chain(logos_state)
                    │
                    ├─ (FMV playback — see Video Playback Subsystem)
                    │   Skippable: 0x0fff mask (most intros, Capcom/Virgin logos)
                    │   Un-skippable: 0x0000 mask (endings, staff intros)
                    │
                    └─► Task_chain(title_state)
                          │
                          ├─ LoadSoundBank(BANK_EVIL, g_DataBuffer)
                          ├─ Title menu loop (attract → main menu)
                          │
                          └─► Task_chain(game_start)
```

---

## Display Configuration

### Configuration Flow

```
┌─────────────────────────────────────────────────────────────┐
│                  Display Configuration                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Load config.ini                                       │
│     - Read FullScreen, Width, Height, BitDepth              │
│     - Dev mode: skip registry/shared memory                 │
│                                                              │
│  2. Create game window                                       │
│     - Fullscreen: WS_POPUP | WS_EX_TOPMOST                  │
│     - Windowed: WS_OVERLAPPEDWINDOW, centered               │
│     - Register "RESIDENT EVIL" window class                 │
│                                                              │
│  3. Initialize Marni System                                  │
│     - Create D3D11 device + swap chain                      │
│     - Compile HLSL shaders (quad VS/PS)                     │
│     - Create pipeline states (blend, rasterizer, samplers)  │
│     - Create white fallback texture (1×1)                   │
│                                                              │
│  4. Enumerate display modes (for future config UI)          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Resolution Scaling

The game renders at a logical 320×240 resolution. All drawing functions scale to the actual screen resolution:

```
scaleX = screenWidth / 320.0f
scaleY = screenHeight / 240.0f
```

The font atlas texture is 256×256 pixels and UV coordinates are normalized by the texture dimensions (not the screen).

### Color Depth

- The game supports 16-bit and 32-bit display modes
- Textures are always created as RGBA8888 internally
- PS1 16-bit ABGR1555 is converted to 32-bit RGBA8888 during texture creation
- PS1 4/8 bpp paletted textures are expanded to RGBA8888 via CLUT

---

## Installation and Registry

### Registry Structure

The game stores configuration in the Windows Registry:

**Key:** `HKEY_CURRENT_USER\Software\CAPCOM\RESIDENT EVIL`

| Value Name | Type | Description |
|------------|------|-------------|
| `Install Path` | REG_SZ | Game installation directory |
| `Create Directory` | REG_SZ | Subdirectory for saves |
| `X Size` | REG_DWORD | Screen width |
| `Y Size` | REG_DWORD | Screen height |
| `Bit Depth` | REG_DWORD | Color depth (16, 24, 32) |
| `FullScreen?` | REG_DWORD | Fullscreen mode (0/1) |
| `Play Number` | REG_DWORD | Play count |
| `Clear Number` | REG_DWORD | Game completion count |
| `Key Def` | REG_BINARY | Key bindings (32 bytes) |
| `Side Def` | REG_BINARY | SideWinder bindings (128 bytes). Read into `g_joystickBindingData`, which nothing consumes; the live pad table is `g_JoyRemapTbl[1]` |
| `Joy Def` | REG_BINARY | Joystick bindings (128 bytes) |
| `Display Driver` | REG_DWORD | Selected display adapter |
| `Install Flag` | REG_DWORD | Installation status |
| `Display Mode` | REG_DWORD | Selected display mode |

### Config file

`config.ini` is the settings store for both configurations (see
`src/system/ConfigFile.h`): the game creates it with documented defaults when it
is missing and rewrites the `[Display]`, `[Player]` and `[Input]` keys on exit,
leaving comments and other sections alone. The registry is still read once at
startup so an install that predates the switch keeps its bindings, but it is no
longer written. The file is gitignored — it is runtime state, not source.

In debug builds (`USE_ASSET_PATH_REMAP=1`):
- Remaps asset paths: `.\usa\data\*` → `.\assets\USA\data\*`
- Skips CD-ROM and installation checks
- Skips shared memory for display config

---

## Memory Management

### Global Static Allocation

Large structures are allocated as globals:

```c
DisplayModeInfo g_DisplayModeBuffer[100];       // 2000 bytes
g_VideoDriverArray_4d0[1024];                   // 4096 bytes
g_VideoDriverArray_838[2048];                   // 8192 bytes
g_VideoDriverArray_520[2048];                   // 8192 bytes
g_TexturePageSRV[256];                          // 1024 bytes (pointers)
g_KeyBindingConfig[32];                         // 128 bytes
```

### Dynamic Allocation

The CMarniDirect3D object is dynamically allocated:

```c
// operator_new(size_t size) → malloc(size)
void* pNewObject = operator_new(0x21DC);  // 8676 bytes
g_pMarniDirect3D = CMarniDirect3D_Constructor(pNewObject, ...);
```

### Asset Loading Buffers

```c
static BYTE g_DataBuffer[832728]; // general purpose buffer, usually used to load textures, sound banks and room data
static BYTE g_TimImageBuffer[187180]; // TIM Images buffer, used mainly for background images
static BYTE g_ItemsImageBuffer[91200];  // item image texture atlas (ITEM_ALL.PIX) at 0x00bcb430; sized for the Director's Cut's 76-row sheet (the USA one is 86400 / 72 rows)
```

### PSXTexture Ownership

`PSXTexture::Store(buffer, 1)` allocates pixel data via `operator_new()` and owns the memory. The destructor cleans up. Ownership flags (`m_dataSource`, `m_ownsPalette`) are guarded against double-free in the destructor.

---

## Subsystem Architecture

### Input Subsystem

Full detail — pad mask bit contract, slot indexing, default bindings, the
defects fixed in 2026-09 — is in **`docs/GAMEPAD_INPUT.md`**.

```
┌─────────────────────────────────────────────────────────────┐
│                    Input Subsystem                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │ XInput + WinMM  │    │ Keyboard        │                │
│  │ -> joysticks[0] │    │ State Array     │                │
│  └────────┬────────┘    └────────▲────────┘                │
│           │                      │                          │
│           ▼                      │                          │
│  ┌─────────────────┐    ┌────────┴────────┐                │
│  │ InitJoysticks() │───▶│ InputUpdate()   │                │
│  └─────────────────┘    └────────┬────────┘                │
│                                  │                          │
│                                  ▼                          │
│                        ┌─────────────────┐                  │
│                        │ PlayerPad_      │                  │
│                        │ Update()        │                  │
│                        └────────┬────────┘                  │
│                                  │                          │
│                                  ▼                          │
│                        ┌─────────────────┐                  │
│                        │ g_RawPadPressed │                  │
│                        │ g_main_state_flags2    │                  │
│                        │ button_pressed  │                  │
│                        └─────────────────┘                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Sound Subsystem

```
┌─────────────────────────────────────────────────────────────┐
│                    Sound Subsystem                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │           DirectSound class                          │    │
│  │  src/marni/MarniSound.h, src/marni/MarniSound.cpp     │    │
│  │                                                      │    │
│  │  Preserves original Ghidra method names:              │    │
│  │  - DirectSound(HWND)     constructor                  │    │
│  │  - CreateSound(wavName)  WAV loader + RIFF parser    │    │
│  │  - DestroySound(bank)    free bank + WAV data        │    │
│  │  - PlaySound(bank,slot)  XAudio2 source voice start  │    │
│  │  - StopSound(bank)       XAudio2 voice stop + flush  │    │
│  │  - SetVol(bank,vol)      XAudio2 voice volume        │    │
│  │  - SetPan(bank,pan)      pan value (+0x924 — stub)   │    │
│  │  - GetVol(bank)          get stored volume (+0x928)  │    │
│  │  - GetStatus(bank)       query XAudio2 voice state   │    │
│  │  - compact()             defrag device memory (stub) │    │
│  │  - Release() / Reload()  pause/resume all audio      │    │
│  │  - ErrorRoutine(code)    maps HRESULT → debug string │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  XAudio2 Backend (modern replacement for DirectSound):       │
│  - g_pXAudio2              IXAudio2 engine instance          │
│  - g_pMasterVoice          mastering voice (2ch, 22050Hz)    │
│  - g_BankVoices[81]        per-bank IXAudio2SourceVoice*     │
│  - Initialized in InitializeSoundSystem()                    │
│  - Shutdown via CleanupSoundManagerResources()               │
│                                                              │
│  Sound bank groups (game-level):                            │
│  g_BgmSoundBank       - Background music (stub)             │
│  g_SfxBanks[]         - Sound effects (16 entries × 2 ints) │
│  g_RoomSfxBanks[]     - Room-specific sounds                │
│  g_CharacterSfxBanks[] - Character sounds                   │
│  g_emSndBanks[]       - Enemy sounds                        │
│  g_SndBank[]          - General purpose banks               │
│                                                              │
│  g_SoundBanksTable[16] - SFX filename sub-tables:            │
│    0,1=Knife   2=Gun     3=Shotgun  4,5=Magnum              │
│    6=Flame     7=Grenade  8=Acid     9=Fire                  │
│    10=Rocket   11=Bio    12=Evil    13=Select                │
│    14=Ending   15=Win95                                      │
│                                                              │
│  SFXIds.h constants: SFX_<BANK>_<NAME> for each entry.       │
│  Bank IDs: BANK_KNIFE(0) ... BANK_WIN95(15).                 │
│                                                              │
│  Volume mapping: -1=max, -9999=min, -10000=mute.             │
│  → XAudio2: vol = 1.0 - (-marniVol / 10000.0)               │
│                                                              │
│  Async execution: ExecAsync(callback) queues operations      │
│  via the TaskScheduler. ⚠️ Never call ExecAsync functions     │
│  unconditionally in a task loop — causes scheduler re-entry. │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### SFX Player Debug State

**File:** `src/game/DebugScreens.cpp`

The `debug_state` task function provides an interactive sound test menu that launches at startup (chained from `load_global_assets` before `logos_state`). It uses `GetAsyncKeyState` for direct PC keyboard input.

**Controls:**

| Key | Action |
|-----|--------|
| `LEFT`/`RIGHT` | Select sound bank (0-15) |
| `ENTER` | Load the selected bank via `LoadSoundBank()` |
| `UP`/`DOWN` | Select SFX entry within the loaded bank |
| `SPACE` | Play selected SFX (respects loop toggle) |
| `S` | Stop selected SFX |
| `L` | Toggle loop mode on/off |
| `W`/`X` | Volume up/down |
| `ESC` | Exit to `logos_state` |

**Architecture notes:**
- State variables (`curBank`, `curEntry`, `loadedBank`, `prevKeys`) are `static` to survive task stack context switches.
- No `ExecAsync`-calling functions (`getSndStat`, `getSndVol`) are called unconditionally in the per-frame loop — this avoids the scheduler infinite re-entry bug.
- Bank loading calls `LoadSoundBank()` which triggers `set_volume()` (async), but only on key-press edge, not every frame.
- Two-column display shows 16 SFX entries (0-7 left, 8-15 right) with handle IDs and loaded status.
- Background rect uses `draw_rect(bg, 100, 1)` matching the title screen pattern (depth=2100 behind text).
┌─────────────────────────────────────────────────────────────┐
│                    Sound Subsystem                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │           DirectSound class                          │    │
│  │  src/marni/MarniSound.h, src/marni/MarniSound.cpp     │    │
│  │                                                      │    │
│  │  Preserves original Ghidra method names:              │    │
│  │  - DirectSound(HWND)     constructor                  │    │
│  │  - DestroySound(bank)    free bank + WAV data        │    │
│  │  - StopSound(bank)       stop playback               │    │
│  │  - PlaySound(bank,slot)  start playback              │    │
│  │  - SetVol(bank,vol)      set volume (+0x928)         │    │
│  │  - SetPan(bank,pan)      set pan   (+0x924)         │    │
│  │  - GetVol(bank)          get stored volume           │    │
│  │  - CreateSound(wavName)  WAV loader + RIFF parser    │    │
│  │  - ErrorRoutine(code)    maps HRESULT → debug string │    │
│  │  - GetStatus(bank)       query buffer status          │    │
│  │  - compact()             defrag device memory         │    │
│  │  - Release() / Reload()  pause/resume all audio      │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  Bank structure (per bank, 0xA2C bytes each):               │
│  +0x1C: WAV raw data pointer    +0x924: pan value           │
│  +0x20: sample data size        +0x928: volume value        │
│  +0x24: sample rate             +0x92C: slot number          │
│  +0x28: channels                +0x930: playing status       │
│  +0x2A: bits per sample         +0x93C: DS buffer ptr        │
│  +0x920: playback rate          +0x940: filename              │
│  +0xA44: active flag                                        │
│                                                              │
│  Sound bank groups (game-level):                            │
│  g_BgmSoundBank      - Background music                    │
│  g_SfxBanks[]        - Sound effects                       │
│  g_RoomSfxBanks[]    - Room-specific sounds                │
│  g_CharacterSfxBanks[] - Character sounds                  │
│  g_emSndBanks[]      - Enemy sounds                        │
│  g_SndBank[]         - General purpose banks               │
│                                                              │
│  g_SoundBanksTable[16][16] - SFX filename tables            │
│                                                              │
│  Async execution: ExecAsync(callback) queues operations     │
│  via the TaskScheduler for non-blocking audio ops.          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Font Sampler (Point vs Linear)

The D3D11 pipeline uses two samplers:
- **`m_pSamplerLinear`** (`D3D11_FILTER_MIN_MAG_MIP_LINEAR`): Used for all textured sprites, backgrounds, and UI elements. Provides smooth bilinear filtering.
- **`m_pSamplerPoint`** (`D3D11_FILTER_MIN_MAG_MIP_POINT`): Used for font rendering only. Provides sharp pixelated text matching the original PS1 appearance.

`MarniDrawSprite` automatically selects the point sampler when detecting the font SRV (`pBindSRV == pD3D->m_pFontSRV`).

### Video Playback Subsystem

**File:** `src/video/VideoPlayback.cpp` — FMV state machine at `0x00474e00`

```
┌─────────────────────────────────────────────────────────────┐
│                  Video Playback Subsystem                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  MCI-based AVI playback (mciSendStringA)                    │
│                                                              │
│  FMV State Machine (g_FMVPlaybackState):                    │
│  - State 0: Initialize                                       │
│      ClearScreen() × 2, MarniPresent(),                     │
│      OpenMCIAviVideo(), PauseGameSoundsAsync()              │
│  - State 1: Start playback                                  │
│      MCI_OpenAndPlay(),                                     │
│      InputUpdate() + PlayerPad_Update() → g_videoSkipInput, │
│      g_videoSkipCounter = 100                               │
│  - State 2: Playing (check for end/skip)                    │
│      Each call:                                             │
│        - decrement g_videoSkipCounter (if > 0)              │
│        - InputUpdate() + PlayerPad_Update()                 │
│        - skip = (skipMask & ~prev & curr) && counter==0     │
│        - if skip: stop MCI, g_mciVideoDeviceID = 0          │
│        - if g_mciVideoDeviceID == 0 → state 3               │
│  - State 3: Cleanup                                         │
│      MCI_CloseAll(), ResumeGameSoundsAsync(),               │
│      StMask(3, 0)                                           │
│                                                              │
│  FMV Skip Mechanism:                                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ g_FMVTable[i].field_4  (per-FMV WORD mask)          │    │
│  │   0x0fff = skippable (all action buttons)           │    │
│  │   0x0000 = un-skippable (endings, staff intros)     │    │
│  │   Sourced from Ghidra 0x004c39dc (8-byte entries)   │    │
│  ├─────────────────────────────────────────────────────┤    │
│  │ g_videoSkipCounter  (grace period)                  │    │
│  │   Starts at 100 when state 1 runs                   │    │
│  │   Decrements each state-2 call                      │    │
│  │   Skip only fires when counter reaches 0            │    │
│  ├─────────────────────────────────────────────────────┤    │
│  │ Edge detection                                      │    │
│  │   skip = (mask & ~g_videoSkipInput & currentInput)  │    │
│  │   Only rising edges of masked buttons trigger skip  │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  Note: During FMV playback, UpdateVideoPlayback() replaces │
│  main_loop() in the main message loop (g_bMCINotifyEnabled │
│  branch at main.cpp:600). State 1 and state 2 must poll   │
│  input themselves — InputUpdate() + PlayerPad_Update()    │
│  are normally only called by main_loop().                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Source File Map

98 source files. Every module documents its original address range in its
header comment.

```
src/
├── main.cpp                    # WinMain entry point (0x00441350)
├── Globals.h / Globals.cpp     # Global variables + structs (with original addresses)
├── WindowProc.cpp              # Window message handler
├── DebugPrint.h                # Debug output helper
│
├── marni/                      # Marni System compatibility layer
│   ├── MarniDX.h/.cpp          # DX11/XAudio2 backend: device, shaders, textures,
│   │                           #   DrawRect/DrawTriangles*, adapters (replaces D3D5)
│   ├── MarniSystem.h/.cpp      # CMarniDirect3D class, present/clear, EnumerateD3DRenderers,
│   │                           #   GetDirect3DDriverCount/Name, SetVideoResolution
│   ├── MarniBits.h/.cpp        # CMarniBits surface class (2D blit/palette ops)
│   ├── PSXTexture.h/.cpp       # TIM/PIX texture parser, CLUT management
│   ├── Marni3DObject.h/.cpp    # CDirect3DObject / CMarniDirect3DTMD /
│   │                           #   CMarniExecuteBuffer / viewport classes (fully ported)
│   ├── MarniSound.h/.cpp       # XAudio2 audio backend + DirectSound-compatible API
│   └── MarniInput.h/.cpp       # Input state (XInput-backed)
│
├── game/
│   ├── MainLoop.cpp            # main_loop (0x00428eb0): per-frame orchestration
│   ├── GameLoop.cpp            # entity update dispatch, render_entity calls
│   ├── GameInit.cpp            # init_and_start_game + load_global_assets
│   ├── LogosScreen.cpp         # logos_state (0x00442bb0)
│   ├── DebugScreens.cpp        # debug/test states: input test, texture viewer, SFX player
│   ├── GameStart.cpp           # game_start + InitializeGame + player/inventory init
│   ├── TitleScreen.cpp         # title screen state machine
│   ├── CharacterSelectionScreen.cpp  # Chris/Jill selection (0x00491xxx)
│   ├── MainMenu.cpp            # inventory/item screen incl. item 3D viewer
│   ├── MenuData.cpp            # menu data tables
│   ├── OptionsMenu.cpp/.h      # options screen
│   ├── SaveLoadScreen.cpp      # save/load screens
│   ├── DeathScreen.cpp         # "You died" screen
│   ├── EndingScreen.cpp        # endings
│   ├── InteractiveScreen.cpp   # interactive-screen dispatcher (0x0042a030)
│   ├── ComputerLab.cpp         # lab terminal state machine (room 5060)
│   ├── LabSlides.cpp           # slide projector screen
│   ├── DoorSystem.cpp          # door open/close + door script opcodes
│   ├── Room.cpp / RoomInit.cpp / RoomEvents.cpp / RoomCollision.cpp
│   ├── ObjectManager.cpp       # room object lifecycle
│   ├── EntityModelLoader.cpp   # EMD model loading
│   ├── Entities.h              # entity struct + type table
│   ├── entities/               # one file per enemy/NPC AI:
│   │   ├── Zombie.cpp/.h  Hunter.cpp  Cerberus.cpp  Crow.cpp  Chimera.cpp
│   │   ├── Adder.cpp  BlackTiger.cpp  Neptune.cpp  Wasp.cpp  SpiderWeb.cpp
│   │   ├── WebSpinner.cpp  Yawn.cpp  Tyrant.cpp  Plant42.cpp
│   │   ├── Plant42Roots.cpp  MonsterPlant.cpp  CharacterNpc.cpp
│   │   ├── ComputerArms.cpp    # lab-terminal forearm entities
│   │   └── EntityCommon.cpp/.h # shared enemy helpers
│   ├── PlayerAnimations.cpp    # player action behaviors (auto-aim fire, etc.)
│   ├── WeaponDamage.cpp        # weapon damage tables/logic
│   ├── TmdRenderer.cpp/.h      # TMD render pipeline + OT queue
│   ├── TmdAnimation.cpp        # TMD animation interpolation
│   ├── GteMatrix.cpp           # PS1 GTE matrix/trig emulation
│   ├── EffectSystem.cpp        # billboard effect slots
│   ├── EffectSprites.cpp       # effect sprite rendering
│   ├── FadeSprite.cpp          # fade-in/out sprites
│   ├── PathTrail.cpp           # pathfinding trails
│   ├── CollisionDebug.cpp      # collision visualization (dev)
│   ├── CmdFunctions.cpp        # SCD script command implementations
│   ├── Rendering.cpp           # frame present, draw_rect, OT_InsertPrimitive
│   ├── SpriteRenderer.cpp/.h   # sprite command buffer
│   ├── PrintText.cpp/.h        # text rendering (replaces DirectFont class)
│   ├── TextureLoader.cpp       # PSX texture -> D3D11 pipeline
│   ├── FileLoader.cpp/.h       # asset loading with path resolution
│   ├── SoundSystem.cpp         # bank loading, play_sfx, fade/decay
│   ├── SoundTables.cpp/.h      # sound data tables
│   ├── SFXIds.h                # named SFX IDs per bank
│   ├── TaskScheduler.cpp       # task coroutine scheduler
│   ├── Types.h                 # shared types (incl. D3DRendererInfo)
│   └── BioCard.h                 # Biocard struct
│
├── system/
│   ├── AssetPath.h             # path remapping (.\\usa\\ -> .\\assets\\USA\\)
│   ├── Cleanup.cpp             # shutdown & resource cleanup
│   ├── DisplayConfig.cpp       # display mode enumeration
│   ├── Installation.cpp        # registry & installation checks
│   └── SystemChecks.cpp        # memory, CD-ROM, color depth checks
│
└── video/
    └── VideoPlayback.cpp       # native MCI FMV playback state machine
```

### Superseded original subsystems

These parts of the 1997 binary are intentionally not ported line-by-line;
their job is done differently (and better) by the modern layer:

| Original | Replacement |
|---|---|
| DirectX 5 / DirectDraw surfaces & blits | `MarniDX` DX11 device + dynamic textures |
| Execute buffers / D3D retain-mode | `MarniDX::DrawTriangles*` batched draws |
| CMarniBits software rasterizers (triangles, gouraud, gradient lines, fills) | GPU rasterization via the same MarniDX draws |
| Software-FMV player process + shared memory (`LaunchSoftwareVideoPlayer`, …) | Native MCI playback (`src/video/VideoPlayback.cpp`) |
| DirectSound buffers (`GetSoundBufferStatus`, wave/mmio loaders) | XAudio2 backend in `MarniSound` |
| DirectFont bitmap-font class (`directfont_*`, 0x0040c460-0x0040c8bd) | `PrintText.cpp` text renderer |
| MSVC CRT / SEH runtime (~470 functions) | Provided by the toolchain |

---

## References

- [DirectX 5.0 Documentation (Archived)](https://docs.microsoft.com/en-us/previous-versions/windows/desktop/bb219735(v=vs.85))
- [PSYQ SDK Documentation](https://psx.arthus.net/sdk/Psy-Q/)
- [MCI Command Strings](https://docs.microsoft.com/en-us/windows/win32/multimedia/mci-command-strings)
- Original Ghidra project: function addresses referenced throughout code via `ghidraMcp_*` tools
