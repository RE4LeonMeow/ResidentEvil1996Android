# Resident Evil 1 PC - Classes and VTables Documentation

This document provides a comprehensive analysis of all C++ classes and virtual tables (vtables) identified in the Resident Evil 1 PC decompilation project.

## Overview

The game uses a custom wrapper system called **Marni System** that wraps DirectDraw/DirectSound/Direct3D functionality, providing a PSYQ-compatible interface (PS1 SDK).

---

## Identified VTables

### 1. MarniSystem_Direct3D_VTable (Main Graphics Class)

**Address:** `0x004af230`

**Class Name:** `MarniSystem::Direct3D` (or `CMarniDirect3D`)

**Object Size:** `0x21dc` bytes (8676 bytes)

**Global Instance:** `g_VideoDriver` at `0x00ac4028`

**Constructor:** `FUN_0044baf0` at `0x0044baf0`

**Destructor:** `CVideoSystem_Cleanup` at `0x0044b940`

#### Virtual Functions:

| Index | Address | Name (Suggested) | Description |
|-------|---------|------------------|-------------|
| 0 | `0x00448630` | `RequestVideoMemory` | Requests video memory from Direct3D device |
| 1 | `0x00449300` | `ChangeDisplayMode` | Changes the display mode (resolution) |
| 2 | `0x0044a0d0` | `SetD3DRenderer` | Sets the Direct3D renderer by index |
| 3 | `0x0044b320` | `Clear` | Clears the Z-buffer and/or render target |
| 4 | `0x00448ff0` | `Present` | Presents/Flips the back buffer to front |
| 5 | `0x00448b60` | `HandleWindowMessage` | Handles window messages (activation, etc.) |
| 6 | `0x0044c900` | `CreateTextureHandle` | Creates a texture handle from pixel data |
| 7 | `0x0044af90` | `CreateObjectHandle` | Creates a 3D object handle for rendering |
| 8 | `0x0044b220` | `DeleteTextureHandle` | Deletes a texture handle by index |
| 9 | `0x0044b1c0` | `DeleteObjectHandle` | Deletes a 3D object handle by index |
| 10 | `0x00448300` | `SetTexture` | Sets the current texture for rendering |
| 11 | `0x00448380` | `ResetTextures` | Resets texture state |

#### Member Variables:

| Offset | Type | Name (Suggested) | Description |
|--------|------|------------------|-------------|
| 0x00 | void** | `vtable` | Pointer to virtual function table |
| 0x10 | DWORD | `m_width` | Screen width |
| 0x14 | DWORD | `m_height` | Screen height |
| 0x18 | DWORD | `m_bitDepth` | Color depth (16, 24, 32) |
| 0x3c | BOOL | `m_isInitialized` | Initialization flag |
| 0x68 | BOOL | `m_isFullScreen` | Fullscreen mode flag |
| 0x74 | BOOL | `m_isActive` | Window active flag |
| 0x78 | DWORD | `m_selectedMode` | Selected display mode index |
| 0x30c | DWORD | `m_deviceType` | Device type (0-6, 5=software) |
| 0x314 | DWORD | `m_currentMode` | Current display mode index |
| 0x761 | LPDIRECTDRAW | `m_lpDD` | DirectDraw interface |
| 0x762 | LPDIRECTDRAW2 | `m_lpDD2` | DirectDraw2 interface |
| 0x7b4 | LPDIRECTDRAWSURFACE | `m_lpDDS_Front` | Front buffer surface |
| 0x7b5 | LPDIRECTDRAWSURFACE | `m_lpDDS_Back` | Back buffer surface |
| 0x7b8 - 0x7e8 | float[4][4] | `m_viewMatrix` | View matrix (4x4) |
| 0x808 | float[4][4] | `m_projectionMatrix` | Projection matrix (4x4) |
| 0x8e0 | void** | `m_objectArray` | Array of 3D object pointers |
| 0x8e4 | DWORD | `m_objectArraySize` | Size of object array |
| 0x1d84 | LPDIRECT3DDEVICE | `m_lpD3DDevice` | Direct3D device |
| 0x1ed4 | LPDIRECT3DVIEWPORT | `m_lpViewport` | Direct3D viewport |
| 0x1ed8 | LPDIRECT3DVIEWPORT2 | `m_lpViewport2` | Direct3D viewport 2 |
| 0x2064 | void* | `m_pUnknown` | Unknown interface pointer |
| 0x21d8 | DWORD | `m_rendererIndex` | Current D3D renderer index |
| 0x238 | void* | `m_pTextureArray` | Texture handle array |
| 0x239 | DWORD | `m_textureArraySize` | Texture array size (0x200) |

---

### 2. MarniBits_VTable (Texture/Surface Class)

**Address:** `0x004af008`

**Class Name:** `CMarniBits`

**Object Size:** `0x54` bytes (84 bytes)

**Constructor:** `CMarniBits_Constructor` at `0x00404910`

#### Virtual Functions:

| Index | Address | Ghidra Name | Method | Description |
|-------|---------|-------------|--------|-------------|
| 0 | `0x00403090` | `CMarniBits_Blt` | `Blt` | Bit-block transfer from source with clipping |
| 1 | `0x00402020` | `CMarniBits_BltFast` | `BltFast` | Pixel-format-converting blit (scaling/mirror/alpha) |
| 2 | `0x00401ee0` | `CMarniBits_UnlockStub` | `UnlockStub` | Stub (returns 1) |
| 3 | `0x00401ef0` | `CMarniBits_PalBlt` | `PalBlt` | Palette copy between surfaces |
| 4 | `0x00403450` | `CMarniBits_Lock` | `Lock` | Lock surface, return pixel pointer + pitch |
| 5 | `0x004034c0` | `CMarniBits_Unlock` | `Unlock` | Unlock surface |
| 6 | `0x00404970` | `CMarniBits_Release` | `Release` | Free pixel/palette buffers, reset fields |

#### Member Variables:

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0x00 | void** | `vtable` | VTable pointer |
| 0x04 | void* | `m_pPixelData` | Pixel/surface data pointer |
| 0x08 | void* | `m_pPalette` | Palette (CLUT) data pointer |
| 0x0C | DWORD | `m_locked` | Lock state (0=unlocked, 1=locked) |
| 0x10 | BYTE | `m_redShift` | Red component bit shift |
| 0x12 | WORD | `m_redMask` | Red component bitmask |
| 0x14 | BYTE | `m_redWidth` | Red component bit width |
| 0x16 | BYTE | `m_greenShift` | Green shift |
| 0x18 | WORD | `m_greenMask` | Green mask |
| 0x1A | BYTE | `m_greenWidth` | Green width |
| 0x1C | BYTE | `m_blueShift` | Blue shift |
| 0x1E | WORD | `m_blueMask` | Blue mask |
| 0x20 | BYTE | `m_blueWidth` | Blue width |
| 0x22 | BYTE | `m_alphaShift` | Alpha shift |
| 0x24 | WORD | `m_alphaMask` | Alpha mask |
| 0x26 | BYTE | `m_alphaWidth` | Alpha width |
| 0x2A | BYTE | `m_bitDepth` | Bits per pixel (4/8/16/24/32) |
| 0x2B | BYTE | `m_paletteFormat` | Palette format (8/16/32-bit) |
| 0x2C | DWORD | `m_width` | Surface width in pixels |
| 0x30 | DWORD | `m_height` | Surface height |
| 0x34 | DWORD | `m_pitch` | Row stride in bytes |
| 0x40 | DWORD | `m_isValid` | Surface valid flag |
| 0x44 | DWORD | `m_dataSource` | 0=external, 1=owned |
| 0x48 | DWORD | `m_hasPalette` | Has palette flag |
| 0x4C | DWORD | `m_ownsPalette` | Palette ownership flag |
| 0x50 | DWORD | `m_flag50` | Additional flag |

---

### 3. Direct3DExecuteBuffer_VTable

**Address:** `0x004af090`

**Class Name:** `CDirect3DObject` / `CMarniExecuteBuffer`

**Constructor:** `CMarniExecuteBuffer_Constructor` at `0x00415f70`

**Base Constructor:** `Direct3DObject_BaseConstructor` at `0x00430e80`

#### Virtual Functions:

| Index | Address | Ghidra Name | Method | Description |
|-------|---------|-------------|--------|-------------|
| 0 | `0x00427270` | `CMarniViewport2_Release` | `Release` | Free vertex/index buffers |
| 1 | `0x00415e90` | `Direct3DObject_CreateWork` | `CreateWork` | Alloc 32-byte vertex + 8/16-byte index buffers |
| 2 | `0x00415a80` | `Direct3DObject_GetVertex` | `GetVertex` | Read 8 DWORDs from vertex buffer |
| 3 | `0x00415b40` | `Direct3DObject_SetVertex` | `SetVertex` | Write 8 DWORDs to vertex buffer |
| 4 | `0x00415c10` | `Direct3DObject_GetList` | `GetList` | Read tri (3×WORD) / quad (4×WORD) indices |
| 5 | `0x00415d20` | `Direct3DObject_SetList` | `SetList` | Write indices; quads split to 2 tris with 0x700 opcodes |
| 6 | `0x004159e0` | `MarniPolyhedra_Lock` | `Lock` | Return buffer pointers, set lock=1 |
| 7 | `0x00415a50` | `MarniPolyhedra_Unlock` | `Unlock` | Clear lock flag |

---

### 4. Direct3DViewport_VTable (Type 1)

**Address:** `0x004af0d0`

**Class Name:** `CMarniViewport` (or similar)

#### Virtual Functions:

| Index | Address | Name (Suggested) | Description |
|-------|---------|------------------|-------------|
| 0 | `0x00432b90` | `Release` | Releases the viewport |
| 1 | `0x00432530` | `Initialize` | Initializes viewport |
| 2 | `0x00401ee0` | `Lock` | Locks viewport |
| 3 | `0x00401ef0` | `Unlock` | Unlocks viewport |
| 4 | `0x00432d50` | `SetViewport` | Sets viewport parameters |
| 5 | `0x00432f70` | `GetViewport` | Gets viewport parameters |
| 6 | `0x0041f990` | `Clear` | Clears viewport |

---

### 5. Direct3DViewport_VTable (Type 2)

**Address:** `0x004af0f8`

**Class Name:** `CMarniViewport2`

**Constructor:** `CMarniViewport2_Constructor` at `0x004272e0`

**Destructor:** `CMarniViewport2_Destructor` at `0x00427320`

**Vertex Format:** 0x2C bytes/vertex (11 floats: position.xyz, normal.xyz, uv.xy, color.rgb)

#### Virtual Functions:

| Index | Address | Ghidra Name | Method | Description |
|-------|---------|-------------|--------|-------------|
| 0 | `0x00427270` | `CMarniViewport2_Release` | `Release` | Free vertex+index buffers, zero fields |
| 1 | `0x00427100` | `CMarniViewport2_CreateWork` | `CreateWork` | Alloc 0x2C/vertex + type*2/poly buffers |
| 2 | `0x00426d60` | `CMarniViewport2_GetVertex` | `GetVertex` | Read 11 floats (0x2C bytes) |
| 3 | `0x00426df0` | `CMarniViewport2_SetVertex` | `SetVertex` | Write 11 floats |
| 4 | `0x00426e80` | `CMarniViewport2_GetList` | `GetList` | Read 3 (tri) / 4 (quad) WORD indices |
| 5 | `0x00426f70` | `CMarniViewport2_SetList` | `SetList` | Write indices with bounds checking |
| 6 | `0x004271e0` | `CMarniViewport2_Lock` | `Lock` | Return buffer pointers |
| 7 | `0x00427250` | `CMarniViewport2_Unlock` | `Unlock` | Clear lock flag |

#### Non-Virtual Methods:

| Address | Ghidra Name | Method | Description |
|---------|-------------|--------|-------------|
| `0x00426600` | `CMarniViewport2_CopyFrom` | `CopyFrom` | Deep copy with strip↔flat conversion |
| `0x004262e0` | `CMarniViewport2_Convert0` | `Convert0` | Convert strips to flat triangle lists |

---

### 6. Direct3DViewport_VTable (Type 3)

**Address:** `0x004af170`

**Class Name:** `CMarniViewport3` (or similar)

#### Virtual Functions:

| Index | Address | Name (Suggested) | Description |
|-------|---------|------------------|-------------|
| 0 | `0x00427270` | `Release` | Releases the viewport |
| 1 | `0x00427100` | `Initialize` | Initializes viewport |
| 2 | `0x00426d60` | `SetViewport` | Sets viewport parameters |
| 3 | `0x00426df0` | `GetViewport` | Gets viewport parameters |
| 4 | `0x00426e80` | `TransformVertices` | Transforms vertices |
| 5 | `0x00426f70` | `LightVertices` | Lights vertices |
| 6 | `0x004271e0` | `SetBackground` | Sets background color |
| 7 | `0x00427250` | `GetBackground` | Gets background color |

---

### 7. Direct3DViewport_VTable (Type 4)

**Address:** `0x004af198`

**Class Name:** `CMarniViewport4` (or similar)

#### Virtual Functions:

| Index | Address | Name (Suggested) | Description |
|-------|---------|------------------|-------------|
| 0 | `0x00432b90` | `Release` | Releases the viewport |
| 1 | `0x00432530` | `Initialize` | Initializes viewport |
| 2 | `0x00401ee0` | `Lock` | Locks viewport |
| 3 | `0x00401ef0` | `Unlock` | Unlocks viewport |
| 4 | `0x00432d50` | `SetViewport` | Sets viewport parameters |
| 5 | `0x00432f70` | `GetViewport` | Gets viewport parameters |
| 6 | `0x00432d10` | `Clear` | Clears viewport |

---

## Identified Classes (Non-Virtual)

### DirectSound Class

**Class Name:** `CMarniDirectSound`

**Constructor:** `InitDirectSoundSystem` at `0x0041f3b0`

**Object Size:** `0x32dc0` bytes (208,192 bytes) - includes embedded buffers

#### Member Variables:

| Offset | Type | Name (Suggested) | Description |
|--------|------|------------------|-------------|
| 0x00 | LPDIRECTSOUND | `m_lpDS` | DirectSound interface |
| 0x04 | HRESULT | `m_lastError` | Last error code |
| 0x10 | BOOL | `m_isInitialized` | Initialization flag |
| 0x14 | DWORD | `m_unknown` | Unknown |
| 0x18 | HWND | `m_hWnd` | Window handle |
| 0x20 | WORD | `m_formatTag` | Audio format tag (1=PCM) |
| 0x22 | WORD | `m_channels` | Number of channels |
| 0x24 | DWORD | `m_sampleRate` | Sample rate (22050) |
| 0x28 | DWORD | `m_byteRate` | Bytes per second |
| 0x2c | WORD | `m_blockAlign` | Block alignment |
| 0x2e | WORD | `m_bitsPerSample` | Bits per sample |
| 0x93c | LPDIRECTSOUNDBUFFER | `m_lpPrimaryBuffer` | Primary sound buffer |

---

## Marni System Classes Summary

The following Marni System classes were identified from debug strings:

| Class Name | Description | String Reference |
|------------|-------------|------------------|
| `MarniSystem::DirectFont` | Font rendering | `0x004b34dc` |
| `MarniSystem::DirectString` | String rendering | `0x004b3594` |
| `MarniSystem::DirectDraw` | DirectDraw wrapper | `0x004b6f38` |
| `MarniSystem::DirectInput` | DirectInput wrapper | `0x004ba154` |
| `MarniSystem::Direct3D` | Direct3D wrapper | `0x004beb58` |
| `MarniSystem::DirectSound` | DirectSound wrapper | `0x004b9a38` |
| `MarniSystem::PriorityList` | Priority list container | `0x004bcaf6` |
| `MarniSystem::Direct3DSurface` | Direct3D surface | `0x004b9f00` |
| `MarniSystem::Direct3DTMD` | 3D model data | `0x004b45e8` |
| `MarniSystem::Direct3DTIM` | Texture image data | `0x004ba220` |
| `MarniSystem::PSXTexture` | PSX texture wrapper | `0x004b9fc0` |
| `MarniSystem::PSXObject` | PSX object wrapper | `0x004bdbb8` |
| `MarniPolyhedra` | 3D polyhedra geometry | `0x004ba3bc` |
| `MarniBits` | Bit manipulation/BLT | `0x004b1290` |

---

## Display Mode Structure

**Address:** `0x007d8f28`

**Size:** `0x114` bytes per entry (276 bytes)

This structure stores display mode information:

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | DWORD | `width` | Screen width |
| 0x04 | DWORD | `height` | Screen height |
| 0x08 | DWORD | `bitDepth` | Color depth |
| 0x0c | DWORD | `refreshRate` | Refresh rate |
| 0x10 | DWORD | `flags` | Mode flags |

---

## Key Global Variables

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `0x00ac4028` | CMarniDirect3D* | `g_VideoDriver` | Main video driver object |
| `0x007d9148` | DWORD | `g_dwSelectedDisplayModeID` | Selected display mode |
| `0x007e0e08` | DWORD | `g_NumD3DRenderersAvailable` | Number of D3D renderers |
| `0x007e0e10` | D3DRendererInfo | `g_D3DRenderers` | Array of D3D renderer info |
| `0x007e139c` | HRESULT | `g_lastDirectDrawError` | Last DirectDraw error |

---

## Inheritance Relationships

Based on the analysis, the following inheritance relationships are suggested:

```
CMarniDirect3D (Main graphics class)
├── Contains CMarniBits (texture operations)
├── Contains CMarniExecuteBuffer (3D rendering)
├── Contains CMarniViewport variants
└── Contains DirectDraw/Direct3D interfaces

CMarniDirectSound (Sound system)
├── Contains DirectSound interface
└── Contains primary/secondary sound buffers
```

---

## Notes

1. The game uses a software renderer (device type 5) when hardware acceleration is not available.
2. The Marni System provides a PSYQ-compatible API for easier porting from PlayStation.
3. Multiple viewport vtables suggest different rendering paths or feature levels.
4. The `g_VideoDriver` object is the central graphics management object.

---

## VTable Calling Conventions

The decomp project uses static vtable adapter functions (not virtual C++ methods)
because the original binary stored vtable pointers as plain fields at offset 0x00.
Each vtable family uses a specific adapter calling convention:

### CMarniDirect3D vtable (g_CMarniDirect3D_VTable)
- **Convention:** `__cdecl` — caller cleans stack
- **Adapter pattern:** `static int VTable_XXX(void* self, ...)` — `self` is first stack arg
- **Impl:** `src/marni/MarniSystem.cpp`

### CDirect3DObject / CMarniViewport2 vtable (g_CMarniViewport2VTable)
- **Convention:** `__stdcall` — callee cleans stack (`RET N`)
- **Adapter pattern:** `static int __stdcall XXX_Adapter(CMarniViewport2* self, ...)`
- **Impl:** `src/marni/Marni3DObject.cpp:392-399`
- **Raw calls must use `__stdcall` function pointer types**, e.g.:
  ```c
  ((int (__stdcall *)(void*))eVtable[0])(elem);   // Release
  ```
  Using `__cdecl` here causes ESP mismatch (adapter does `RET 4`, caller then does `add esp, 4`).

### CMarniBits vtable (CMarniBits_vtable)
- **Convention:** `__cdecl` — caller cleans stack
- **Adapter pattern:** `static int VTable_XXX(void* self, ...)` — `self` is first stack arg
- **Impl:** `src/marni/MarniBits.cpp:77-85`
- **Lock second output** (`outPitch`): reads `[ECX+0x08]` (m_pPalette), NOT m_pitch (0x34).
  ```c
  *outPitch = (DWORD)(ULONG_PTR)m_pPalette;  // correct
  *outPitch = m_pitch;                       // WRONG (offset 0x34)
  ```

### Common pitfall: calling vtable functions through raw function pointers
When the original code uses `__thiscall` (`this` in ECX, stack args), but the decomp
uses `__cdecl` static wrappers (`self` as first stack arg), raw calls must pass `self`
explicitly. Omitting `self` causes the wrapper to read garbage from the stack:
- `CheckTextureRecreation` (FUN_00483f40), `FixClutVertexData` (FUN_00483eb0):
  both call `CMarniBits::Lock`/`Unlock` through the vtable with `matEntry` as `self`.

---

## Task System

The game uses a task-based architecture for game logic:

### TaskEntry Structure

**Size:** Unknown (at least 4 bytes for state)

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | DWORD | `state` | Task state flags |

**Global Array:** `g_TasksTable` - Array of task entries

### Task Functions

| Address | Name | Description |
|---------|------|-------------|
| `0x0041d0e0` | `Task_suspend` | Suspend a task by ID |
| `0x0041d100` | `Task_Resume` | Resume a suspended task |
| `0x004200c0` | `TaskScheduler_Update` | Update all active tasks |

---

## Window Rectangle Structure

Used for drawing operations:

### WindowRect Structure

**Size:** 28 bytes

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | int | `w` | Width |
| 0x04 | int | `textureId` | Texture ID |
| 0x08 | int | `r` | Red color component |
| 0x0C | int | `g` | Green color component |
| 0x10 | int | `b` | Blue color component |
| 0x14 | int | `x` | X position |
| 0x18 | int | `h` | Height |
| 0x1C | int | `y` | Y position |

**Global Instance:** `g_window_rect`

---

## Display Mode Structure

### DisplayModeInfo Structure

**Size:** 20 bytes

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | DWORD | `dwWidth` | Screen width |
| 0x04 | DWORD | `dwHeight` | Screen height |
| 0x08 | DWORD | `dwBPP` | Bits per pixel |
| 0x0C | DWORD | `dwRefreshRate` | Refresh rate |
| 0x10 | DWORD | `dwFlags` | Mode flags |

**Global Array:** `g_DisplayModeBuffer[100]` at `0x007dfd28`

---

## Input System

### Input State Variables

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `g_RawPadPressed` | DWORD | Raw pad input state |
| `g_main_state_flags2` | DWORD | Input state flags |
| `button_pressed_id` | unsigned short | Button identifier |

### Input Functions

| Address | Name | Description |
|---------|------|-------------|
| `0x00497c00` | `InputUpdate` | Update input state |
| `0x0044e000` | `PlayerPad_Update` | Update player pad state |

---

## Sound System

### Sound Bank Arrays

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `g_BgmSoundBank` | int | BGM sound bank |
| `g_SfxBanks` | int* | SFX banks array |
| `g_RoomSfxBanks` | int* | Room SFX banks |
| `g_CharacterSfxBanks` | int* | Character SFX banks |
| `g_emSndBanks` | int* | Enemy SFX banks |
| `g_SndBank` | int* | General sound banks |

### Sound Functions

| Address | Name | Description |
|---------|------|-------------|
| `0x00480150` | `destroySndBank` | Destroy a sound bank |
| `0x00480640` | `PauseSounds` | Pause all sounds |
| `0x004806b0` | `ResumePausedSounds` | Resume paused sounds |

---

## Next Steps

1. Rename all unnamed functions in Ghidra based on this documentation
2. Define proper C++ class structures in the decompilation
3. Implement the virtual function tables correctly
4. Document remaining member variables as they are discovered
