# Resident Evil 1 PC - Functions Reference

This document provides detailed documentation for all implemented functions in the decompilation project.

---

## Table of Contents

1. [WinMain.cpp Functions](#winmaincpp-functions)
2. [MarniSystem.cpp Functions](#marnisystemcpp-functions)
3. [MainLoop.cpp Functions](#mainLoopcpp-functions)
4. [Rendering.cpp Functions](#renderingcpp-functions)
5. [Cleanup.cpp Functions](#cleanupcpp-functions)
6. [SystemChecks.cpp Functions](#systemcheckscpp-functions)
7. [Installation.cpp Functions](#installationcpp-functions)
8. [DisplayConfig.cpp Functions](#displayconfigcpp-functions)
9. [WindowProc.cpp Functions](#windowproccpp-functions)
10. [VideoPlayback.cpp Functions](#videoplaybackcpp-functions)

---

## WinMain.cpp Functions

### WinMain

**Address:** `0x00441350`

**Purpose:** Main entry point for the game. Handles system initialization, installation verification, display configuration, window creation, and the main message loop.

**Signature:**
```cpp
int PASCAL WinMain(
    HINSTANCE hInstance,      // Application instance handle
    HINSTANCE hPrevInstance,  // Previous instance (always NULL on Win32)
    LPSTR lpCmdLine,          // Command line arguments
    int nCmdShow              // Window show state
);
```

**Return Value:** 
- `0` - Normal exit
- `1` - Color depth error
- `2` - No CD-ROM found
- `3` - Game already running
- `4` - Setup already running
- `5` - Uninstall already running
- `6` - User cancelled installation
- `7` - Setup completed, restart needed
- `9` - Software rendering memory error
- `10` - Software rendering map error

**Dependencies:**
- `GetFreeDiskSpaceMB()` - Check disk space
- `EnumerateDriveTypes()` - Find CD-ROM
- `ShowMessageBox()` - Display errors
- `IsGameInstalled()` - Check installation
- `LoadInstallationConfiguration()` - Load settings
- `CheckVideoCapabilities()` - Check video
- `EnumerateAndSelectDisplayMode()` - Display config
- `InitializeMarniSystem()` - Init graphics
- `main_loop()` - Main game loop
- `DestroyAllSoundBanks()` - Cleanup
- `CleanupAsyncTasks()` - Cleanup
- `CleanupVideoConfigAndSaveAllSettings()` - Cleanup
- `CleanupSharedMemory()` - Cleanup
- `WindowProc()` - Window procedure

**Phases:**
1. System checks (memory, color depth, CD-ROM)
2. Single instance check (mutexes)
3. Installation verification
4. Display configuration
5. Software rendering setup (if needed)
6. Window creation
7. Game initialization
8. Main game loop

---

## MarniSystem.cpp Functions

### IsGraphicsSystemReadyForOperation

**Address:** `0x00497060`

**Purpose:** Check if the Marni Direct3D graphics system is properly initialized and ready for rendering.

**Signature:**
```cpp
BOOL IsGraphicsSystemReadyForOperation(void);
```

**Return Value:**
- `TRUE` - Graphics system is ready
- `FALSE` - Graphics system not initialized or error

**Implementation:**
```cpp
BOOL IsGraphicsSystemReadyForOperation(void)
{
    if (g_pMarniDirect3D != NULL)
    {
        // Check initialization flag at offset 0x3C
        if (*(int*)((char*)g_pMarniDirect3D + 0x3C) != 0)
        {
            return TRUE;
        }
        printf(FORMAT_STR_VIDEO_SUBSYSTEM_ERROR, s_trans_cpp_004d4798);
        return FALSE;
    }
    
    if (g_hWnd == NULL)
    {
        return TRUE;
    }
    
    printf(FORMAT_STR_FALLBACK_SYSTEM_ERROR, s_trans_cpp_004d4798);
    return FALSE;
}
```

**Dependencies:** None

---

### InitializeMarniSystem

**Address:** `0x004970c0`

**Purpose:** Initialize the Marni graphics system, including Direct3D, input devices, and lighting.

**Signature:**
```cpp
void InitializeMarniSystem(
    HWND hWnd,           // Window handle
    int displayModeID,   // Selected display mode
    int adapterID        // Display adapter ID (unused)
);
```

**Parameters:**
- `hWnd` - Handle to the game window
- `displayModeID` - Index of selected display mode
- `adapterID` - Display adapter index (not used in original)

**Operations:**
1. Validate and set screen dimensions
2. Allocate CMarniDirect3D object (8676 bytes)
3. Call CMarniDirect3D constructor
4. Verify graphics system is ready
5. Initialize joysticks
6. Check for SideWinder gamepad
7. Create lights for 3D rendering
8. Hide cursor in fullscreen mode
9. Record initialization time

**Dependencies:**
- `CMarniDirect3D_Constructor()` - Create graphics object
- `IsGraphicsSystemReadyForOperation()` - Verify init
- `InitJoysticks()` - Enumerate WinMM devices + start the XInput backend
  (forwards to `CMarniDirectInput::InitJoysticks`; see `docs/GAMEPAD_INPUT.md`)
- `IsSideWinderPadConnected()` - Legacy SideWinder probe. The pad capability
  flag is `g_bPadConnected`, set here from `MarniPadIsConnected()`. Do NOT set
  `g_isSideWinderConnected`: that is the one-shot START injection `main_loop`
  consumes
- `CreateLights()` - Create lighting
- `ShowMessageBox()` - Display errors

---

### EnumerateD3DRenderers

**Address:** `0x004977f0`

**Purpose:** Enumerate available Direct3D hardware renderers and store their information.

**Signature:**
```cpp
void EnumerateD3DRenderers(void);
```

**Operations:**
1. Check if Marni Direct3D is available
2. Get count of D3D renderers
3. Enumerate each renderer's name

**Dependencies:**
- `GetDirect3DDriverCount()` - Get renderer count
- `GetDirect3DDriverName()` - Get renderer name

---

## MainLoop.cpp Functions

### main_loop

**Address:** `0x00428eb0`

**Purpose:** Main game loop that handles input, game state updates, rendering, and state transitions.

**Signature:**
```cpp
int main_loop(void);
```

**Return Value:**
- `1` - Continue game loop
- `0` - Exit game (never reached in normal operation)

**Operations:**
1. Initialize game on first run
2. Update input state
3. Check for special key combinations
4. Handle input flags
5. Check FMV playback state
6. Handle menu dialogs (return to title, exit game)
7. Update demo timer
8. Reset screen panning
9. Update sound fade/decay
10. Update task scheduler
11. Handle screen fading effects
12. Apply room-specific lighting
13. Handle screenshot mode
14. Apply screen shake
15. Present frame

**Dependencies:**
- `init_and_start_game()` - Initialize game
- `InputUpdate()` - Update input
- `PlayerPad_Update()` - Update player input
- `TaskScheduler_Update()` - Update tasks
- `draw_rect()` - Draw rectangles
- `PrintText8x14()` - Print text
- `PauseSounds()` - Pause audio
- `ResumePausedSounds()` - Resume audio
- `Task_suspend()` - Suspend task
- `Task_Resume()` - Resume task
- Many more helper functions

**State Flags Checked:**
- `g_main_state_flags` - Various game states
- `g_displayReturnToTitleScreen_Flag` - Return dialog
- `g_displayExitGameScreen_flag` - Exit dialog
- `g_fading_state` - Screen fade state

---

## Rendering.cpp Functions

### PrintText8x14

**Address:** `0x00455520`

**Purpose:** Render text using 8×14 pixel mono-spaced font. Characters are laid out 18 per row in the font atlas (`fontus.tim`). Uses `PRINT_TEXT_BUFFER` as source string.

**Signature:**
```cpp
void PrintText8x14(short x, short y, unsigned char color, char flags);
```

**Parameters:**
- `x, y` — Screen position in 320×240 game coordinates (with `g_ScreenOffsetX/Y` applied)
- `color` — Upper nibble = brightness (0→2, else raw; bit 7 = max brightness 30), lower nibble = CLUT tint index
- `flags` — `0` = normal text, `!= 0` = draw shadow pass first (black, offset +1,+1)

**Usage:**
```cpp
sprintf(PRINT_TEXT_BUFFER, "Hello World");
PrintText8x14(16, 100, 128, 1);  // white text with shadow at (16,100)
PrintText8x14(16, 116, 128, 0);  // white text without shadow
```

**Rendering:** Calls `AddTintSprite()` per character through the `g_pendingSprites[]` queue. Characters rendered in insertion order; to ensure text appears above background rects, the rect must be drawn **before** the text.

**Dependencies:** `PRINT_TEXT_BUFFER`, `AddTintSprite()`, `g_ScreenOffsetX/Y`, `g_stageId`, `g_roomId`, `g_roomCameraId`

---

### PrintText8x8

**Address:** `0x00455420`

**Purpose:** Render text using 8×8 pixel mono-spaced font. Characters offset from ASCII 0x20. Same pipeline as PrintText8x14.

**Signature:**
```cpp
void PrintText8x8(short x, short y, unsigned char color, char shadow);
```

**Parameters:**
- `x, y` — Screen position
- `color` — Same encoding as PrintText8x14
- `shadow` — `0` = no shadow, `!= 0` = draw shadow pass first

**Dependencies:** Same as PrintText8x14

---

### PrintFormattedText

**Address:** `0x00455190`

**Purpose:** Control-code-based formatted text renderer for debug/menu output.

**Signature:**
```cpp
void PrintFormattedText(short x, short y, unsigned char color, unsigned char* data);
```

**Parameters:**
- `x, y` — Screen position
- `color` — Same encoding as PrintText8x14
- `data` — Byte stream with opcodes:

| Opcode | Description |
|--------|-------------|
| `0x00` | Advance X by 8 (space) |
| `0x01`/`0x07` | End of string |
| `0xF8` | Next byte = char code, row = next/18 + 15 |
| `0xF9` | Next byte = char code, row = next/18 |
| `0xFA` | Next byte = char code, row = next/18 + 14 |
| `0xFB` | Skip byte (no-op) |
| `0xFF` | Advance X by 4 (half-width space) |
| *default* | Direct char code, row = ch/18 + 2 |

**Dependencies:** `g_TextureDepth`, `AddTintSprite()`

---

### AddTintSprite

**Address:** `0x0046e0a0`

**Purpose:** Build and enqueue a tinted font character sprite. Called by all text rendering functions.

**Signature:**
```cpp
int AddTintSprite(TextureDesc* texDesc, unsigned short brightness);
```

**Parameters:**
- `texDesc` — Texture descriptor (global `g_texPrintState`) containing position, UV, color tint
- `brightness` — 0–30 range (maps to alpha 0–255). Values ≤2 map to alpha 17 (faint)

**Operations:**
1. Reads `g_TexturePrintX/Y`, `g_TextureVramX/Y`, `g_PrintTintR/G/B` from globals
2. Computes screen-space position with scaling: `(gameX * scaleX, gameY * scaleY)`
3. Computes UV coordinates from font atlas dimensions
4. Builds `DWORD color`: alpha = `brightness * 255/30`, RGB = `tint * 2` (clamped to 255)
5. Enqueues a `PendingSprite` in `g_pendingSprites[]` using `m_pFontSRV`

**Dependencies:** `g_TexturePrintX/Y`, `g_TextureVramX/Y`, `g_PrintTintR/G/B`, `g_ScreenOffsetX/Y`, `CMarniDirect3D`

---

### draw_rect

**Address:** `0x00470350`

**Purpose:** Draw a solid-color (or textured) rectangle. Used for menu backgrounds, fade overlays, and debug screens. Implements `GetTextureVariant` blend modes from the original game to control alpha transparency.

**Signature:**
```cpp
void draw_rect(RectDrawDesc* rect, int blend, int flags);
```

**Parameters:**
- `rect` — Rectangle descriptor:
  - `x, y` — Top-left position in 320×240 game coords
  - `w, h` — Size in pixels
  - `r, g, b` — Color components (0–255)
  - `textureId` — Selects blend variant via `GetTextureVariant()`:
    - `0` → Variant 0: Opaque fill (`g_window_rect`, pause screens)
    - `0x50000000` → Variant 2: White flash overlay (`fade_type_id=1`), alpha = max(r,g,b)
    - `0x60000000` → Variant 3: Black fade overlay (`fade_type_id=2`), alpha = max(r,g,b), color forced to black
    - `0x40000000` → Variant 1: Semi-transparent tinted overlay (special room lighting)
- `blend` — Controls OT depth sort value (higher = further back). `flags==0` → `blend+450`, else `blend*16+500`.
- `flags` — Depth sort mode selector

**Blend Variants (matching original `GetTextureVariant` at `0x0046d950`):**

| Variant | textureId | Behavior | Original Case |
|---------|-----------|----------|---------------|
| 0 | `0` or no high bits | Fully opaque (alpha=255) | case 0 |
| 1 | `0x40000000` | Semi-transparent tinted (alpha = max component) | case 1 |
| 2 | `0x50000000` | White flash (r=g=b=255, alpha = brightness) | case 2 |
| 3 | `0x60000000` | Black fade (r=g=b=0, alpha = brightness) | case 3 |

**Important:** When alpha = 0 (brightness = 0), the draw is skipped entirely, allowing the background image to show through. Rects with `draw_rect` use `g_pendingSprites[]` with depth-sorted rendering in `FrameRateGovernor`.

**Dependencies:** `g_ScreenOffsetX/Y`, `CMarniDirect3D`, `m_pWhiteSRV`, `GetTextureVariant()`

---

### display_texture

**Address:** `0x0046e8d0`

**Purpose:** Enqueue a textured sprite using the texture page table. Used for title screen button prompts and other texture atlas sprites.

**Signature:**
```cpp
void display_texture(TextureDesc* texture, unsigned short depth, int slot, int pageCount);
```

**Parameters:**
- `texture` — Texture descriptor (flags, position, UV, color)
- `depth` — Depth sort priority (multiplied by 16 + 500)
- `slot` — Texture page slot index (shifted by +0xF)
- `pageCount` — Unused in current port

**Dependencies:** `g_SpriteCommandBuffer`, `BuildSpriteRenderFlags()`, `GetTextureVariant()`, `g_TexturePageSRV[]`

---

### OT_InsertPrimitive

**Address:** `0x004402f0`

**Purpose:** Insert the title screen background image into the pending sprite queue at index 0 (highest priority rendering). Called by `FUN_0040a8f0` at the start of `FrameRateGovernor`.

**Signature:**
```cpp
void OT_InsertPrimitive(void* prim, unsigned int depth);
```

**Operations:**
1. Validates prim type = 1 and depth = 0xFFF
2. Shifts all existing `g_pendingSprites` right by one
3. Inserts `g_displayImageSRV` as a fullscreen sprite at index 0
4. Increments `g_pendingSpriteCount`

**Dependencies:** `g_displayImageSRV`, `g_pendingSprites[]`, `g_main_state_flags & 0x40000000`

---

### FrameRateGovernor

**Address:** `0x004973d0`

**Purpose:** Core frame timing, rendering, and presentation function. Called once per frame from `main_loop()`.

**Flow:**
1. **Frame timing**: Tracks frame deltas in a 4-slot circular buffer, computes target frame time
2. **Budget check**: If `g_frameTimeAccumulator < g_frameTargetTime`, skips rendering (drops frame)
3. **Rendering**: `MarniClear()` → `FUN_0040a8f0()` (insert title BG) → render in depth-split order → `MarniPresent()`
4. **Post-present**: Resets sprite queues, updates timers, handles screen/render access flags

**Rendering Order (depth-split):**

The original game puts all sprites (background, game objects, text, fade overlays) in one `g_SpriteCommandBuffer` sorted by depth. The decomp uses two queues — `g_pendingSprites[]` (from `draw_rect`, `OT_InsertPrimitive`) and `g_SpriteCommandBuffer` (from `display_texture`). To match the original depth ordering, rendering is split into three phases:

| Phase | Depth Range | Contents | Rendered By |
|-------|-------------|----------|-------------|
| 1 — Background | ≥ 500 | Title BG image (0xFFF), pause overlays (2100) | `MarniDrawSprite` on `g_pendingSprites` |
| 2 — Game objects | varies | Title text, game sprites, textures | `FlushSpriteCommands` on `g_SpriteCommandBuffer` |
| 3 — Screen effects | < 500 | Fade overlays (450), color tinting (490), room lighting (470–499) | `MarniDrawSprite` on `g_pendingSprites` |

This ensures fade overlays correctly cover title text and game objects, matching the original game where the fade rect at depth 450 sorted on top of the title text at depth 532.

**Dependencies:** `MarniClear()`, `MarniPresent()`, `FlushSpriteCommands()`, `MarniDrawSprite()`, `FUN_0040a8f0()`, `SpriteQueue_Reset()`

---



### CleanupSharedMemory

**Address:** `0x00442230`

**Purpose:** Release shared memory used for inter-process communication with the setup program.

**Signature:**
```cpp
void CleanupSharedMemory(void);
```

**Operations:**
1. Signal cleanup by writing 0 to first byte
2. Wait for acknowledgment (byte 1 != 1)
3. Sleep for 1 second
4. Unmap shared memory view
5. Close file mapping handle

**Dependencies:** None

---

### UpdateGameStatus

**Address:** `0x00442350`

**Purpose:** Update game status flag in shared memory to indicate the game is running.

**Signature:**
```cpp
void UpdateGameStatus(void);
```

**Operations:**
- Set byte 5 of shared memory to 1

**Dependencies:** None

---

### CleanupVideoConfigAndSaveAllSettings

**Address:** `0x00497ea0`

**Purpose:** Perform final cleanup of video system and save all settings to registry.

**Signature:**
```cpp
void CleanupVideoConfigAndSaveAllSettings(void);
```

**Operations:**
1. Check guard flag to prevent double cleanup
2. Save settings to registry
3. Call CMarniDirect3D cleanup/destructor
4. Free allocated memory
5. Set global pointer to NULL

**Dependencies:**
- `SaveGameSettingsToRegistry()` - Save settings
- `CVideoSystem_Cleanup()` - Cleanup graphics

---

### DestroyAllSoundBanks

**Address:** `0x004801c0`

**Purpose:** Destroy all loaded sound banks and free associated resources.

**Signature:**
```cpp
void DestroyAllSoundBanks(void);
```

**Operations:**
1. Destroy BGM sound bank
2. Destroy all SFX banks
3. Destroy room SFX banks
4. Destroy character SFX banks
5. Destroy enemy SFX banks
6. Destroy general sound banks

**Dependencies:**
- `destroySndBank()` - Destroy individual bank

---

### CleanupAsyncTasks

**Address:** `0x0041d0b0`

**Purpose:** Initiate cleanup of asynchronous operations.

**Signature:**
```cpp
void CleanupAsyncTasks(void);
```

**Operations:**
- Execute async callback to destroy sound manager

**Dependencies:**
- `ExecAsync()` - Execute async callback
- `DestroySoundManagerAsync()` - Callback function

---

## SystemChecks.cpp Functions

### GetFreeDiskSpaceMB

**Address:** `0x0040c510`

**Purpose:** Calculate free disk space on the drive containing the specified path.

**Signature:**
```cpp
DWORD GetFreeDiskSpaceMB(
    LPCSTR lpPath    // Path to check (only first char used)
);
```

**Parameters:**
- `lpPath` - Path string; only the drive letter (first character) is used

**Return Value:** Free disk space in megabytes

**Implementation:**
```cpp
DWORD GetFreeDiskSpaceMB(LPCSTR lpPath)
{
    CHAR szRootPath[4];
    DWORD dwSectorsPerCluster, dwBytesPerSector;
    DWORD dwNumberOfFreeClusters, dwTotalNumberOfClusters;
    
    szRootPath[0] = lpPath[0];
    szRootPath[1] = ':';
    szRootPath[2] = '\\';
    szRootPath[3] = '\0';
    
    GetDiskFreeSpaceA(szRootPath, &dwSectorsPerCluster,
        &dwBytesPerSector, &dwNumberOfFreeClusters,
        &dwTotalNumberOfClusters);
    
    return (dwNumberOfFreeClusters * dwBytesPerSector * 
            dwSectorsPerCluster) >> 20;
}
```

**Dependencies:** None (Win32 API only)

---

### EnumerateDriveTypes

**Address:** `0x0047b830`

**Purpose:** Enumerate all logical drives and store their types for CD-ROM detection.

**Signature:**
```cpp
BOOL EnumerateDriveTypes(void);
```

**Return Value:**
- `TRUE` - At least one drive found
- `FALSE` - No drives found

**Operations:**
1. Clear drive type buffer
2. Get all logical drive strings
3. Parse each drive string
4. Get drive type using GetDriveTypeA
5. Store in global arrays

**Dependencies:** None (Win32 API only)

---

### ShowMessageBox

**Address:** `0x00497ee0`

**Purpose:** Display a message box with automatic cursor visibility handling.

**Signature:**
```cpp
int ShowMessageBox(
    HWND hWndOwner,       // Owner window
    LPCSTR lpMessageText, // Message text
    LPCSTR lpCaptionText, // Caption text
    UINT uMessageBoxType  // Message box style
);
```

**Parameters:**
- `hWndOwner` - Handle to owner window
- `lpMessageText` - Message to display
- `lpCaptionText` - Window caption
- `uMessageBoxType` - MB_ flags

**Return Value:** Result from MessageBoxA (IDOK, IDCANCEL, etc.)

**Operations:**
1. Show cursor if hidden
2. Display message box
3. Restore cursor state

**Dependencies:** None (Win32 API only)

---

## Installation.cpp Functions

### IsGameInstalled

**Address:** `0x0040ae50`

**Purpose:** Check if the game is properly installed by verifying registry key existence.

**Signature:**
```cpp
BOOL IsGameInstalled(void);
```

**Return Value:**
- `TRUE` - Game is installed (registry key exists)
- `FALSE` - Game is not installed

**Registry Key:** `HKEY_CURRENT_USER\Software\CAPCOM\RESIDENT EVIL`

**Dependencies:** None (Win32 API only)

---

### LoadInstallationConfiguration

**Address:** `0x0040aea0`

**Purpose:** Load all game settings from the Windows registry.

**Signature:**
```cpp
BOOL LoadInstallationConfiguration(
    BYTE* pInstallPath    // Buffer to receive install path
);
```

**Parameters:**
- `pInstallPath` - Buffer to receive the full installation path

**Return Value:**
- `TRUE` - Configuration loaded successfully
- `FALSE` - Error loading configuration

**Registry Values Read:**
| Value | Type | Variable |
|-------|------|----------|
| Install Path | REG_SZ | Path buffer |
| Create Directory | REG_SZ | Appended to path |
| X Size | REG_DWORD | `g_dwScreenWidth` |
| Y Size | REG_DWORD | `g_dwScreenHeight` |
| Bit Depth | REG_DWORD | `g_dwBitDepth` |
| FullScreen? | REG_DWORD | `g_bFullScreen` |
| Play Number | REG_DWORD | `g_dwPlayCount` |
| Clear Number | REG_DWORD | `g_dwClearCount` |
| Key Def | REG_BINARY | `g_keyBindingData` |
| Side Def | REG_BINARY | `g_joystickBindingData` |
| Joy Def | REG_BINARY | `g_joystickBindingData` |
| Display Driver | REG_DWORD | `g_dwSelectedDisplayAdapterID` |
| Install Flag | REG_DWORD | `g_InstallFlagData` |
| Display Mode | REG_DWORD | `g_dwSelectedDisplayModeID` |

**Dependencies:**
- `IsSideWinderPadConnected()` - Check for SideWinder. Selects which binding
  blob is read and sets `g_bPadConnected` (the capability), not the one-shot
  `g_isSideWinderConnected`

---

## DisplayConfig.cpp Functions

### EnumDisplayModesCallback

**Address:** `0x00442930`

**Purpose:** DirectDraw callback function for enumerating available display modes.

**Signature:**
```cpp
HRESULT CALLBACK EnumDisplayModesCallback(
    LPDDSURFACEDESC lpDDSurfaceDesc,  // Surface description
    LPVOID lpContext                   // User context
);
```

**Parameters:**
- `lpDDSurfaceDesc` - DirectDraw surface description for the mode
- `lpContext` - Pointer to EnumDisplayModesContext structure

**Return Value:** `DDENUMRET_OK` to continue enumeration

**Operations:**
1. Extract mode info from surface description
2. Store in display mode buffer
3. Increment mode count

**Dependencies:** None (DirectDraw API)

---

### EnumerateAndSelectDisplayMode

**Address:** `0x00442870`

**Purpose:** Enumerate display modes and show selection dialog.

**Signature:**
```cpp
INT_PTR EnumerateAndSelectDisplayMode(void);
```

**Return Value:**
- Dialog result from selection
- `-1` on failure

**Operations:**
1. Create DirectDraw object
2. Clear display mode buffer
3. Enumerate all display modes
4. Release DirectDraw
5. Display selection dialog

**Dependencies:**
- `EnumDisplayModesCallback()` - Enumeration callback
- `DisplayModeDialogProc()` - Dialog procedure

---

### EnumerateDisplayModes

**Address:** `0x004976c0`

**Purpose:** Enumerate display modes through the Marni Direct3D interface.

**Signature:**
```cpp
void EnumerateDisplayModes(void);
```

**Operations:**
1. Check if Marni Direct3D is available
2. Get display mode count
3. For each mode, get mode info and store

**Dependencies:**
- `GetDisplayModeCount()` - Get mode count
- `GetDisplayModeRect()` - Get mode info

---

### GetDisplayModeCount

**Address:** `0x00448770`

**Purpose:** Get the number of available display modes.

**Signature:**
```cpp
int GetDisplayModeCount(void);
```

**Return Value:** Number of display modes

**Status:** Implemented (DXGI-backed) in src/system/DisplayConfig.cpp - enumerates adapters/modes instead of DirectDraw.

---

### GetDisplayModeRect

**Address:** `0x004487a0`

**Purpose:** Get display mode information by index.

**Signature:**
```cpp
void GetDisplayModeRect(
    int modeIndex,    // Mode index
    DWORD* pRect      // Output buffer (5 DWORDs)
);
```

**Parameters:**
- `modeIndex` - Index of the display mode
- `pRect` - Buffer to receive: [width, height, bpp, refresh, flags]

**Status:** Implemented (DXGI-backed) in src/system/DisplayConfig.cpp.

---

## WindowProc.cpp Functions

### WindowProc

**Address:** `0x00441170`

**Purpose:** Main window procedure handling all window messages.

**Signature:**
```cpp
LRESULT CALLBACK WindowProc(
    HWND hWnd,      // Window handle
    UINT uMsg,      // Message ID
    WPARAM wParam,  // WPARAM
    LPARAM lParam   // LPARAM
);
```

**Parameters:**
- `hWnd` - Window handle
- `uMsg` - Message identifier
- `wParam` - Message-specific parameter
- `lParam` - Message-specific parameter

**Return Value:** Message-dependent result

**Messages Handled:**

| Message | Handler |
|---------|---------|
| `WM_CREATE` | Initialize sound system |
| `WM_DESTROY` | Cleanup and quit |
| `WM_ACTIVATE` | Handle activation state |
| `WM_KEYDOWN` | Process key input |
| `WM_KEYUP` | Check for Print Screen |
| `WM_PAINT` | Begin/End paint |
| `WM_SYSCOMMAND` | Handle restore for software mode |
| `MM_MCINOTIFY` | MCI video notification |

**Dependencies:**
- `ProbeWaveOutDevicesAndCacheVolume()` - Cache audio volume
- `StartSoundSystemAsync()` - Initialize sound
- `PauseSounds()` - Pause audio
- `ResumePausedSounds()` - Resume audio
- `CleanupVideoConfigAndSaveAllSettings()` - Cleanup
- `RestoreWaveOutVolume()` - Restore volume
- `OnKeyDown()` - Key handler
- `CreateTimestampedLogFile()` - Debug log

---

## VideoPlayback.cpp Functions

### UpdateVideoPlayback

**Address:** `0x00474e00`

**Purpose:** FMV playback state machine for both hardware and software rendering modes. Handles MCI AVI playback, skip detection, and cleanup.

**Signature:**
```cpp
void UpdateVideoPlayback(void);
```

**State Machine:**

#### Hardware Rendering Path

| State | Operation |
|-------|-----------|
| 0 | Initialize: `ClearScreen()` × 2, `MarniPresent()`, open MCI device, pause sounds, store FMV path |
| 1 | Start playback: `MCI_OpenAndPlay(path, playTo)` (`playTo` = the prologue cut point for Jill, else 0), `InputUpdate()` + `PlayerPad_Update()` to seed `g_videoSkipInput`, set `g_videoSkipCounter = 100` |
| 2 | Playing: each call decrements `g_videoSkipCounter`, polls input, checks skip edge, watches `g_bMCIVideoEvent` (which resumes the prologue's second chunk, see below), transitions to state 3 when `g_mciVideoDeviceID == 0` |
| 3 | Cleanup: `MCI_CloseAll()`, resume sounds, `StMask(3, 0)` |

#### Software Rendering Path

| State | Operation |
|-------|-----------|
| 0 | Initialize: Clear screen, launch external player |
| 1 | Playing: Check for skip, wait for completion |
| 2 | Cleanup: Restore window, resume audio |

#### Prologue Scenario Cut (FMV 1)

The post-character-select prologue (`PU.avi` USA / `PJ.avi` JPN) is authored for the Chris
scenario. The PC movies cut frames **1778..1884** at 10 fps; the PS1/DC STR
stream uses frames **2430..2590** at 15 fps. The port converts those two frame
ranges to time points so the generated MP4s cut at the same source beat.

`main_loop` latches `g_FmvCharacterId = g_SelectedCharactedId` (`0x008f879c`) when it consumes
the `MSF_FMV_REQUEST` flag. When that id is non-zero (Jill), `UpdateVideoPlayback` plays the
movie as two MCI_PLAY commands instead of one and drops the range:

| Original call | Port equivalent | Effect |
|---|---|---|
| `video_mci_window_helper(0, 0x6f2)` in state 1 | `MCI_OpenAndPlay(path, 0x6f2)` -> `play movie from 0 to 1778 notify` | first chunk, stops before the beat |
| `video_mci_window_helper(0x75d, 0)` on the first `MM_MCINOTIFY` | `MCI_PlayFrom(0x75d)` -> `play movie from 1885 notify` | remainder, to the end |

Both calls are gated on `g_CurrentFMVID == 1 && g_FmvCharacterId != 0`; the second is
additionally gated on `g_videoFlagA4` (`DAT_008f87a4`, set in state 0 and cleared by the first
notification), so only the *first* notification resumes and the second ends the FMV. Chris
(`g_FmvCharacterId == 0`) plays the movie whole with a single un-ranged `play movie notify`.

The PC cut points remain the default in OG mode. In DC mode the shared state
machine uses the PS1 15-fps frame points instead, because the generated DC MP4s
retain the STR's CD-sector timing.

#### FMV Skip Mechanism

Skip is per-FMV and gated by both a skip mask and a 100-call grace period.

- **Per-FMV skip mask** — Each entry in `g_FMVTable` (declared in `src/video/VideoPlayback.cpp`, data sourced from Ghidra at `0x004c39dc`) carries a `field_4` WORD. `0x0fff` = skippable (all action buttons), `0x0000` = un-skippable. Verified against the original binary at `0x004c39d8` (8-byte stride, ptr + DWORD mask).
- **Counter (`g_videoSkipCounter`)** — Initialized to 100 when state 1 starts, decremented each state-2 call. Must reach 0 before any skip is accepted (prevents accidental skip at video start).
- **Edge detection** — State 1 seeds `g_videoSkipInput = (WORD)PlayerPad_Update()`. State 2 computes `currentInput = (WORD)PlayerPad_Update()` and checks `(skipMask & ~g_videoSkipInput & currentInput) != 0 && g_videoSkipCounter == 0`. Only a rising edge of a masked button while the counter is 0 triggers the skip.
- **Accept key** — Default keymap binds PC bit 11 to `'C'` (action/confirm), which `g_JoyRemapTbl[0]` maps to PSX `0x0080` (R1). Any bit in `0x0fff` (Cross, Circle, Square, Triangle, L1, L2, R1, R2, Select, Start, L3, R3) will skip.

**Input polling note:** During FMV playback `UpdateVideoPlayback()` runs in place of `main_loop()` (which normally calls `InputUpdate()` + `PlayerPad_Update()`). State 1 and state 2 therefore poll input themselves. `GetAsyncKeyState` works regardless of which window has focus, so the MCI child window receiving focus does not block skip input.

**Skippable FMVs (mask 0x0fff):** 0 (OU.avi opening), 1 (PU.avi), 3-9, 11 (DMB.avi), 12 (DMC.avi), 13 (DMD.avi), 23 (capcom.avi logo), 28 (vlogo.avi logo).

**Un-skippable FMVs (mask 0x0000):** 2 (DMF.avi), 14 (DME.avi), 15-22 (endings ED1-ED8), 24-27 (stfc_r/stfj_r/stfz_r/staf_r staff intros).

**Dependencies:**
- `ClearScreen()` - Clear display (alias for `MarniClear()`)
- `OpenMCIAviVideo()` - Open MCI device
- `MCI_OpenAndPlay()` - Open + start MCI playback
- `MCI_CloseAll()` - Close MCI device
- `MCISend()` - Send MCI command string
- `CheckVideoFileExists()` - Verify video file
- `setMenuScreenOffset()` - Set screen offset
- `CenterScreenOrigin()` - Center screen
- `InputUpdate()` - Poll keyboard + joysticks
- `PlayerPad_Update()` - Edge-detect pad, returns `g_button_pressed_id`
- `PauseGameSoundsAsync()` - Pause audio
- `ResumeGameSoundsAsync()` - Resume audio
- `ShowMessageBox()` - Display errors
- `StMask()` - State mask function
- Many software player functions

---

## Function Index by Address

| Address | Function | File |
|---------|----------|------|
| `0x0040ae50` | `IsGameInstalled` | Installation.cpp |
| `0x0040aea0` | `LoadInstallationConfiguration` | Installation.cpp |
| `0x0040c510` | `GetFreeDiskSpaceMB` | SystemChecks.cpp |
| `0x0041d0b0` | `CleanupAsyncTasks` | Cleanup.cpp |
| `0x00428eb0` | `main_loop` | MainLoop.cpp |
| `0x004402f0` | `OT_InsertPrimitive` | Rendering.cpp |
| `0x00441170` | `WindowProc` | WindowProc.cpp |
| `0x00441350` | `WinMain` | WinMain.cpp |
| `0x00442230` | `CleanupSharedMemory` | Cleanup.cpp |
| `0x00442350` | `UpdateGameStatus` | Cleanup.cpp |
| `0x00442870` | `EnumerateAndSelectDisplayMode` | DisplayConfig.cpp |
| `0x00442930` | `EnumDisplayModesCallback` | DisplayConfig.cpp |
| `0x00448770` | `GetDisplayModeCount` | DisplayConfig.cpp |
| `0x004487a0` | `GetDisplayModeRect` | DisplayConfig.cpp |
| `0x00455190` | `PrintFormattedText` | Rendering.cpp |
| `0x00455420` | `PrintText8x8` | Rendering.cpp |
| `0x00455520` | `PrintText8x14` | Rendering.cpp |
| `0x0046e0a0` | `AddTintSprite` | Rendering.cpp |
| `0x0046e8d0` | `display_texture` | Rendering.cpp |
| `0x00470350` | `draw_rect` | Rendering.cpp |
| `0x00474e00` | `UpdateVideoPlayback` | VideoPlayback.cpp |
| `0x0047b830` | `EnumerateDriveTypes` | SystemChecks.cpp |
| `0x004801c0` | `DestroyAllSoundBanks` | Cleanup.cpp |
| `0x00497060` | `IsGraphicsSystemReadyForOperation` | MarniSystem.cpp |
| `0x004970c0` | `InitializeMarniSystem` | MarniSystem.cpp |
| `0x004973d0` | `FrameRateGovernor` | Rendering.cpp |
| `0x004976c0` | `EnumerateDisplayModes` | DisplayConfig.cpp |
| `0x004977f0` | `EnumerateD3DRenderers` | MarniSystem.cpp |
| `0x00497ea0` | `CleanupVideoConfigAndSaveAllSettings` | Cleanup.cpp |
| `0x00497ee0` | `ShowMessageBox` | SystemChecks.cpp |

---
