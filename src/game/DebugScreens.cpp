// DebugScreens.cpp - Debug-only interactive screens
// All functions decompiled from Ghidra with original addresses
#include "../Globals.h"
#include "../platform/platform.h"
#include "../DebugPrint.h"
#include "../marni/MarniSystem.h"
#include "../marni/MarniSound.h"
#include "../marni/PSXTexture.h"
#include "../marni/Marni3DObject.h"
#include "FileLoader.h"
#include "SpriteRenderer.h"
#include <cstdio>

extern void logos_state(void);   // LogosScreen.cpp

// ============================================================================
// input_test_state — Input system verification screen
// Displays all input globals each frame to verify the pipeline:
//   InputUpdate → ReadPadBoth → PlayerPad_Update
//
// After PlayerPad_Update:
//   g_RawPadState       = raw newHeldRaw low 16 (NOT edge-detected)
//   g_PlayerPadHeld     = edge-detected full 32-bit (== g_PlayerPadPressed)
//   g_PlayerPadPressed  = edge-detected full 32-bit
//   g_button_pressed_id = full 32-bit copy of g_PlayerPadHeld (pre-overwrite)
//
// Controls:
//   F1 → proceed to logos_state
// ============================================================================
void input_test_state(void)
{
    OutputDebugStringA("[INPUT] input_test_state - entering\n");

    static int prevF1 = 0;

    while (1) {
        // Run the full input pipeline
        InputUpdate();
        PlayerPad_Update();

        // --- F1 detection (direct GetAsyncKeyState, not via PS1 pipeline) ---
        int f1Down = (plat_key_state(VK_F1) & 0x8000) ? 1 : 0;
        int f1Pressed = f1Down && !prevF1;
        prevF1 = f1Down;

        // --- Display ---
        g_window_rect.w = 320;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.h = 240;
        g_window_rect.y = -g_ScreenOffsetY;
        draw_rect(&g_window_rect, 100, 1);

        int y = 2;
        int col1 = 2;
        int col2 = 170;

        sprintf(PRINT_TEXT_BUFFER, "=== INPUT TEST ===");
        PrintText8x14(col1, y, 0x8F, 0); y += 16;

        // Raw pipeline output (NOT edge-detected)
        sprintf(PRINT_TEXT_BUFFER, "PadBtnWord:   0x%04X", (WORD)g_PadBtnWord);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "RawPadHeld:   0x%08X  (raw, for menu nav)", g_RawPadHeld);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "RawPadState:  0x%04X  (newHeldRaw lo16)", g_RawPadState);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "RawPadPressed:0x%04X  (edge-det via Prev)", g_padEdgeDetectedWord);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "HeldPrev:     0x%04X  (prev RawPadState)", (WORD)g_PlayerPadHeldPrev);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;

        y += 4;

        // Edge-detected full 32-bit state
        sprintf(PRINT_TEXT_BUFFER, "--- Edge-detected (32-bit) ---");
        PrintText8x14(col1, y, 0x8F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "PadHeld:      0x%08X  (edge-det, gameplay)", g_PlayerPadHeld);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "PadPressed:   0x%08X", g_PlayerPadPressed);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "btn_pressed:  0x%08X  (copy of RawPadHeld)", g_button_pressed_id);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;

        y += 4;

        // DPad states (remapped via padRemapTable)
        sprintf(PRINT_TEXT_BUFFER, "--- DPad (remapped) ---");
        PrintText8x14(col1, y, 0x8F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "DpadHeld:     0x%04X", (WORD)g_PlayerDpadHeld);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "DpadPressed:  0x%04X", g_PlayerDpadPressed);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;
        sprintf(PRINT_TEXT_BUFFER, "DpadPrev:     0x%04X", g_PlayerDpadHeldPrev);
        PrintText8x14(col1, y, 0x7F, 0); y += 14;

        y += 4;

        // Button status (right column)
        y = 2;
        sprintf(PRINT_TEXT_BUFFER, "--- PS1 Buttons ---");
        PrintText8x14(col2, y, 0x8F, 0); y += 14;

        WORD held = (WORD)g_RawPadHeld;
        WORD pressed = (WORD)g_PlayerPadPressed;
        WORD dpad = (WORD)g_PlayerDpadHeld;

        #define BTN(name, mask, cy) \
            sprintf(PRINT_TEXT_BUFFER, "%-10s held=%c press=%c dpad=%c", name, \
                    (held & mask) ? 'X' : '-', \
                    (pressed & mask) ? 'X' : '-', \
                    (dpad & mask) ? 'X' : '-'); \
            PrintText8x14(col2, cy, (held & mask) ? 0x8F : 0x5F, 0)

        BTN("UP",       0x0010, y); y += 14;
        BTN("DOWN",     0x0040, y); y += 14;
        BTN("LEFT",     0x0080, y); y += 14;
        BTN("RIGHT",    0x0020, y); y += 14;
        BTN("TRIANGLE", 0x1000, y); y += 14;
        BTN("CIRCLE",   0x2000, y); y += 14;
        BTN("CROSS",    0x4000, y); y += 14;
        BTN("SQUARE",   0x8000, y); y += 14;
        BTN("L1",       0x0004, y); y += 14;
        BTN("R1",       0x0008, y); y += 14;
        BTN("L2",       0x0002, y); y += 14;
        BTN("R2",       0x0001, y); y += 14;
        BTN("START",    0x0800, y); y += 14;
        #undef BTN

        y = 220;
        sprintf(PRINT_TEXT_BUFFER, "F1=proceed  Arrows=Dpad  Enter=START+CROSS  Space=CROSS  Esc=SQUARE");
        PrintText8x14(2, y, 0x5F, 0);

        // Exit condition: F1 pressed → proceed to logos_state
        if (f1Pressed) break;

        Task_sleep(1);
    }

    OutputDebugStringA("[INPUT] input_test_state - exiting, chaining to logos_state\n");
    Task_chain((void*)logos_state);
}

// ============================================================================
// texture_viewer_state — Texture page verification screen
// Scans g_TexturePageSRV for loaded textures and displays them visually.
// Shows texture info and a preview of each loaded page.
//
// Controls:
//   LEFT/RIGHT      : Navigate textures
//   UP/DOWN         : Navigate textures
//   A+ARROWS        : Move texture preview offset
//   Z+LEFT/RIGHT    : Cycle CLUT palette (for multi-CLUT textures)
//   R               : Reset offset to 0
//   F1              : Proceed to logos_state
//   ESC             : Proceed to logos_state
// ============================================================================
void texture_viewer_state(void)
{
    OutputDebugStringA("[TEX] texture_viewer_state - entering\n");

    static int selectedPage = 0;
    static int prevKeys = 0;
    static int foundCount = 0;
    static int validPages[256];
    static float texOffsetX = 0.0f;
    static float texOffsetY = 0.0f;
    static int clutIndex[256];  // current CLUT palette index per slot

    // Scan for loaded texture pages once on entry
    foundCount = 0;
    for (int i = 0; i < 256; i++) {
        clutIndex[i] = 0;
        if (g_TexturePageSRV[i] != NULL) {
            validPages[foundCount] = i;
            foundCount++;
        }
    }

    char dbg[128];
    sprintf(dbg, "[TEX] texture_viewer_state: found %d loaded texture pages\n", foundCount);
    OutputDebugStringA(dbg);

    if (foundCount > 0) selectedPage = 0;
    else selectedPage = -1;

    while (1) {
        // --- Background (drawn via draw_rect which queues PendingSprites) ---
        g_window_rect.w = 320;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.h = 240;
        g_window_rect.y = -g_ScreenOffsetY;
        draw_rect(&g_window_rect, 100, 1);

        // Title line
        sprintf(PRINT_TEXT_BUFFER, "TEXTURE VIEWER [%d]", foundCount);
        PrintText8x14(2, 2, 0x8F, 0);

        if (foundCount == 0) {
            sprintf(PRINT_TEXT_BUFFER, "No textures loaded!");
            PrintText8x14(2, 18, 0x4F, 0);
        } else {
            int pageIdx = validPages[selectedPage];
            int texW = g_TexturePageWidth[pageIdx];
            int texH = g_TexturePageHeight[pageIdx];
            int texBpp = g_TexturePageBpp[pageIdx];
            int numCLUTs = GetTextureNumCLUTs(pageIdx);

            // Info line: slot, index, dimensions, bpp, offset, CLUT
            if (numCLUTs > 1) {
                sprintf(PRINT_TEXT_BUFFER, "[%d/%d] %dx%d %dbpp clut=%d/%d off=(%d,%d)",
                        selectedPage + 1, foundCount, texW, texH, texBpp,
                        clutIndex[pageIdx] + 1, numCLUTs,
                        (int)texOffsetX, (int)texOffsetY);
            } else {
                sprintf(PRINT_TEXT_BUFFER, "[%d/%d] %dx%d %dbpp off=(%d,%d)",
                        selectedPage + 1, foundCount, texW, texH, texBpp,
                        (int)texOffsetX, (int)texOffsetY);
            }
            PrintText8x14(2, 18, 0x7F, 0);

            // Page list: show rows of page indices, 5 columns
            int listY = 34;
            int cols = 5;
            for (int i = 0; i < foundCount && i < 25; i++) {
                int col = i % cols;
                int row = i / cols;
                unsigned char color = (i == selectedPage) ? 0x8F : 0x5F;
                sprintf(PRINT_TEXT_BUFFER, "%d", validPages[i]);
                PrintText8x14((short)(2 + col * 60), (short)(listY + row * 14), color, 0);
            }
            int listRows = (foundCount + cols - 1) / cols;
            if (listRows > 3) listRows = 3;
            int previewY = listY + listRows * 14 + 6;

            // Texture preview: queue a PendingSprite with the texture's SRV
            // Coordinates must be in game-space (320x240)
            if (texW > 0 && texH > 0) {
                // Available preview area in game coords
                float availW = 310.0f;
                float availH = 236.0f - (float)previewY;

                // Show textures at native pixel size (1:1): never upscale
                // small textures, only downscale large ones to fit the area.
                float scX = availW / (float)texW;
                float scY = availH / (float)texH;
                float sc = (scX < scY) ? scX : scY;
                if (sc > 1.0f) sc = 1.0f;

                float drawW = (float)texW * sc;
                float drawH = (float)texH * sc;
                float drawX = 5.0f + (availW - drawW) * 0.5f + texOffsetX;
                float drawY = (float)previewY + texOffsetY;

                QueueTexturedSprite(drawX, drawY, drawW, drawH,
                                    g_TexturePageSRV[pageIdx], 200);
            }
        }

        // Controls line at bottom
        sprintf(PRINT_TEXT_BUFFER, "Arrows:Nav A+Arrows:Move Z+LR:CLUT R:Reset F1/ESC:Exit");
        PrintText8x14(2, 228, 0x5F, 0);

        // --- Input handling (direct GetAsyncKeyState with edge detection) ---
        int keys = 0;
        int aHeld = (plat_key_state('A') & 0x8000) ? 1 : 0;
        int zHeld = (plat_key_state('Z') & 0x8000) ? 1 : 0;
        if (plat_key_state(VK_LEFT)   & 0x8000) keys |= 0x001;
        if (plat_key_state(VK_RIGHT)  & 0x8000) keys |= 0x002;
        if (plat_key_state(VK_UP)     & 0x8000) keys |= 0x004;
        if (plat_key_state(VK_DOWN)   & 0x8000) keys |= 0x008;
        if (plat_key_state('R')       & 0x8000) keys |= 0x010;
        if (plat_key_state(VK_F1)     & 0x8000) keys |= 0x020;
        if (plat_key_state(VK_ESCAPE) & 0x8000) keys |= 0x040;

        int newKeys = keys & ~prevKeys;
        prevKeys = keys;

        if (aHeld) {
            // A+Arrow: move texture offset (hold for continuous movement)
            float moveSpeed = 1.0f;
            if (keys & 0x001) texOffsetX -= moveSpeed;
            if (keys & 0x002) texOffsetX += moveSpeed;
            if (keys & 0x004) texOffsetY -= moveSpeed;
            if (keys & 0x008) texOffsetY += moveSpeed;
        } else if (zHeld && foundCount > 0) {
            // Z+Left/Right: cycle CLUT palette
            int pageIdx = validPages[selectedPage];
            int numCLUTs = GetTextureNumCLUTs(pageIdx);
            if (numCLUTs > 1 && (newKeys & (0x001 | 0x002))) {
                if (newKeys & 0x001) {
                    clutIndex[pageIdx]--;
                    if (clutIndex[pageIdx] < 0) clutIndex[pageIdx] = numCLUTs - 1;
                }
                if (newKeys & 0x002) {
                    clutIndex[pageIdx]++;
                    if (clutIndex[pageIdx] >= numCLUTs) clutIndex[pageIdx] = 0;
                }
                if (RebuildTextureSRV(pageIdx, clutIndex[pageIdx])) {
                    sprintf(dbg, "[TEX] Rebuilt SRV[%d] with CLUT %d/%d\n",
                            pageIdx, clutIndex[pageIdx], numCLUTs);
                    OutputDebugStringA(dbg);
                }
            }
        } else {
            // Arrow keys: navigate pages
            if (foundCount > 0) {
                if (newKeys & 0x001) { selectedPage--; if (selectedPage < 0) selectedPage = foundCount - 1; }
                if (newKeys & 0x002) { selectedPage++; if (selectedPage >= foundCount) selectedPage = 0; }
                if (newKeys & 0x004) { selectedPage--; if (selectedPage < 0) selectedPage = foundCount - 1; }
                if (newKeys & 0x008) { selectedPage++; if (selectedPage >= foundCount) selectedPage = 0; }
            }
        }

        // R: reset offset (edge-triggered)
        if (newKeys & 0x010) { texOffsetX = 0.0f; texOffsetY = 0.0f; }

        if (newKeys & 0x020) break;  // F1
        if (newKeys & 0x040) break;  // ESC

        Task_sleep(1);
    }

    OutputDebugStringA("[TEX] texture_viewer_state - exiting, chaining to logos_state\n");
    Task_chain((void*)logos_state);
}

// ============================================================================
// texture_viewer_overlay — per-frame in-game texture inspector (F6, debug only)
//
// Rendered as an overlay on top of the normal frame: the game loop calls this
// once per frame while g_debugTextureViewerOpen is set; it draws the loaded
// texture-page list and a preview through the pending-sprite queue, which
// FrameRateGovernor flushes with the rest of the frame. Unlike the boot-time
// texture_viewer_state this returns to the caller each frame — gameplay
// resumes as soon as F6/ESC closes it (no task chain).
//
// Controls (GetAsyncKeyState, edge-detected like texture_viewer_state):
//   Arrows        : navigate the page list
//   A+Arrows      : move the preview offset
//   R             : reset the preview offset
//   F6 / ESC      : close and return to gameplay
//
// Returns 1 while the overlay should stay open, 0 when it closed. While open
// the game's own pad state is zeroed so the player does not move underneath.
// ============================================================================
int texture_viewer_overlay(void)
{
    static int      initialized = 0;
    static int      selectedPage = 0;
    static int      validPages[256];
    static int      foundCount = 0;
    static float    texOffsetX = 0.0f;
    static float    texOffsetY = 0.0f;
    static int      prevKeys = 0;

    if (!initialized) {
        // Rescan the SRV table on entry so slots loaded after boot (item
        // icons, room pages) show up.
        foundCount = 0;
        for (int i = 0; i < 256; i++) {
            if (g_TexturePageSRV[i] != MARNI_NULL_HANDLE) {
                validPages[foundCount++] = i;
            }
        }
        selectedPage = (foundCount > 0) ? 0 : -1;
        texOffsetX = 0.0f;
        texOffsetY = 0.0f;
        prevKeys = 0;
        initialized = 1;
    }

    // Freeze the player while the overlay is up: the frame's pad state was
    // already latched, so blank the movement/action inputs for this frame.
    g_PlayerDpadHeld = 0;
    g_PlayerPadHeld = 0;
    g_PlayerPadPressed = 0;
    g_button_pressed_id = 0;

    // --- Background (draw_rect goes through the pending sprite queue) ---
    g_window_rect.w = 320;
    g_window_rect.textureId = 0;
    g_window_rect.r = 0;
    g_window_rect.g = 0;
    g_window_rect.b = 0;
    g_window_rect.x = -g_ScreenOffsetX;
    g_window_rect.h = 240;
    g_window_rect.y = -g_ScreenOffsetY;
    draw_rect(&g_window_rect, 100, 1);

    sprintf(PRINT_TEXT_BUFFER, "TEXTURE VIEWER [%d]  F6/ESC:exit", foundCount);
    PrintText8x14(2, 2, 0x8F, 0);

    if (foundCount == 0) {
        sprintf(PRINT_TEXT_BUFFER, "No textures loaded!");
        PrintText8x14(2, 18, 0x4F, 0);
    } else {
        int pageIdx = validPages[selectedPage];
        int texW = g_TexturePageWidth[pageIdx];
        int texH = g_TexturePageHeight[pageIdx];
        int texBpp = g_TexturePageBpp[pageIdx];

        sprintf(PRINT_TEXT_BUFFER, "[%d/%d] slot=%d %dx%d %dbpp off=(%d,%d)",
                selectedPage + 1, foundCount, pageIdx, texW, texH, texBpp,
                (int)texOffsetX, (int)texOffsetY);
        PrintText8x14(2, 18, 0x7F, 0);

        // Page list: rows of slot numbers, 6 columns
        int listY = 34;
        for (int i = 0; i < foundCount && i < 60; i++) {
            int col = i % 6;
            int row = i / 6;
            unsigned char color = (i == selectedPage) ? 0x8F : 0x5F;
            sprintf(PRINT_TEXT_BUFFER, "%d", validPages[i]);
            PrintText8x14((short)(2 + col * 52), (short)(listY + row * 14), color, 0);
        }

        // Preview: native size, downscaled only to fit
        if (texW > 0 && texH > 0) {
            float availW = 310.0f;
            float availH = 236.0f - (float)listY - 70.0f;
            float scX = availW / (float)texW;
            float scY = availH / (float)texH;
            float sc = (scX < scY) ? scX : scY;
            if (sc > 1.0f) sc = 1.0f;
            if (sc < 0.01f) sc = 0.01f;

            float drawW = (float)texW * sc;
            float drawH = (float)texH * sc;
            float drawX = 5.0f + (availW - drawW) * 0.5f + texOffsetX;
            float drawY = (float)listY + 70.0f + texOffsetY;

            QueueTexturedSprite(drawX, drawY, drawW, drawH,
                                g_TexturePageSRV[pageIdx], 200);
        }
    }

    // --- Input (direct GetAsyncKeyState with edge detection) ---
    int keys = 0;
    int aHeld = (plat_key_state('A') & 0x8000) ? 1 : 0;
    if (plat_key_state(VK_LEFT)   & 0x8000) keys |= 0x001;
    if (plat_key_state(VK_RIGHT)  & 0x8000) keys |= 0x002;
    if (plat_key_state(VK_UP)     & 0x8000) keys |= 0x004;
    if (plat_key_state(VK_DOWN)   & 0x8000) keys |= 0x008;
    if (plat_key_state('R')       & 0x8000) keys |= 0x010;
    if (plat_key_state(VK_F6)     & 0x8000) keys |= 0x020;
    if (plat_key_state(VK_ESCAPE) & 0x8000) keys |= 0x040;

    int newKeys = keys & ~prevKeys;
    prevKeys = keys;

    if (aHeld) {
        float moveSpeed = 1.0f;
        if (keys & 0x001) texOffsetX -= moveSpeed;
        if (keys & 0x002) texOffsetX += moveSpeed;
        if (keys & 0x004) texOffsetY -= moveSpeed;
        if (keys & 0x008) texOffsetY += moveSpeed;
    } else if (foundCount > 0) {
        if (newKeys & 0x001) { selectedPage--; if (selectedPage < 0) selectedPage = foundCount - 1; }
        if (newKeys & 0x002) { selectedPage++; if (selectedPage >= foundCount) selectedPage = 0; }
        if (newKeys & 0x004) { selectedPage--; if (selectedPage < 0) selectedPage = foundCount - 1; }
        if (newKeys & 0x008) { selectedPage++; if (selectedPage >= foundCount) selectedPage = 0; }
    }

    if (newKeys & 0x010) { texOffsetX = 0.0f; texOffsetY = 0.0f; }

    if (newKeys & (0x020 | 0x040)) {   // F6 or ESC: close
        initialized = 0;
        return 0;
    }
    return 1;
}

// ============================================================================
// debug_state — SFX Player Debug Menu
// Tests the MarniSound system interactively: load sound banks from
// g_SoundBanksTable, browse SFX entries, play/stop/loop individual sounds.
//
// Two-column layout showing 16 SFX slots per bank (0-7 left, 8-15 right).
// Uses direct GetAsyncKeyState for PC keyboard input with edge detection.
//
// Controls:
//   LEFT/RIGHT    : Change sound bank (0-15)
//   UP/DOWN       : Select SFX entry
//   ENTER         : Load selected bank
//   SPACE         : Play selected SFX (respects loop toggle)
//   S             : Stop selected SFX
//   L             : Toggle loop mode on/off
//   W/X           : Volume up/down
//   ESC           : Exit to logos_state
// ============================================================================
void debug_state(void)
{
    // Original address: 0x00429a40 continuation (debug_state inserted at chain point)
    static const char* bankNames[] = {
        "Knife",     "Knife2",   "Gun",      "Shotgun",
        "Magnum",    "Magnum2",  "Flame",    "Grenade",
        "Acid",      "Fire",     "Rocket",   "Bio",
        "Evil",      "Select",   "Ending",   "Win95"
    };

    static int curBank = 0;
    static int curEntry = 0;
    static int loadedBank = -1;
    static int sfxVolume = -1;     // -1 = max, -9999 = min
    static int loopMode = 0;       // 0 = one-shot, 1 = looping
    static int prevKeys = 0;

    OutputDebugStringA("[DEBUG] SFX Player - entering debug_state\n");

    while (1) {
        // --- Draw background (title screen pattern: blend=100, flags=1 → depth=2100 behind text) ---
        g_window_rect.w = 320;
        g_window_rect.textureId = 0;
        g_window_rect.r = 0;
        g_window_rect.g = 0;
        g_window_rect.b = 0;
        g_window_rect.x = -g_ScreenOffsetX;
        g_window_rect.h = 240;
        g_window_rect.y = -g_ScreenOffsetY;
        draw_rect(&g_window_rect, 100, 1);

        int y = 2;

        // Title
        sprintf(PRINT_TEXT_BUFFER, "=== SFX PLAYER ===");
        PrintText8x14(2, y, 0x8F, 0); y += 16;

        // Sound system status
        int dsReady = (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10));
        sprintf(PRINT_TEXT_BUFFER, "DS: %-7s  Vol: %-5d  Loop: %s",
                dsReady ? "READY" : "OFF", sfxVolume, loopMode ? "ON" : "OFF");
        PrintText8x14(2, y, 0x7F, 0); y += 16;

        // Bank selection
        sprintf(PRINT_TEXT_BUFFER, "Bank [%d]: %s  %s",
                curBank, bankNames[curBank],
                (curBank == loadedBank) ? "(loaded)" : "");
        PrintText8x14(2, y, 0x8F, 0); y += 18;

        // Separator
        sprintf(PRINT_TEXT_BUFFER, "----------------------------------------");
        PrintText8x14(2, y, 0x5F, 0); y += 16;

        // SFX entries — two columns: 0-7 on left, 8-15 on right
        if (loadedBank >= 0) {
            const char** tbl = g_SoundBanksTable[loadedBank];
            int startY = y;
            for (int i = 0; i < 8; i++) {
                int col2Idx = i + 8;

                // Left column (entries 0-7)
                {
                    const char* fn = tbl[i];
                    unsigned char col = (i == curEntry) ? 0x8F : 0x7F;
                    if (fn) {
                        int hnd = g_SfxBanks[i * 2];
                        sprintf(PRINT_TEXT_BUFFER, " %c[%2d] %-12s h=%d",
                                (i == curEntry) ? '>' : ' ', i, fn, hnd);
                        PrintText8x14(2, startY, col, 0);
                    } else {
                        sprintf(PRINT_TEXT_BUFFER, "  [%2d] ---", i);
                        PrintText8x14(2, startY, 0x5F, 0);
                    }
                }

                // Right column (entries 8-15)
                {
                    const char* fn = tbl[col2Idx];
                    unsigned char col = (col2Idx == curEntry) ? 0x8F : 0x7F;
                    if (fn) {
                        int hnd = g_SfxBanks[col2Idx * 2];
                        sprintf(PRINT_TEXT_BUFFER, " %c[%2d] %-12s h=%d",
                                (col2Idx == curEntry) ? '>' : ' ', col2Idx, fn, hnd);
                        PrintText8x14(160, startY, col, 0);
                    } else {
                        sprintf(PRINT_TEXT_BUFFER, "  [%2d] ---", col2Idx);
                        PrintText8x14(160, startY, 0x5F, 0);
                    }
                }

                startY += 14;
            }
            y = startY + 4;

            // Played entry status (no async calls — ExecAsync creates scheduler loop)
            int hnd = g_SfxBanks[curEntry * 2];
            sprintf(PRINT_TEXT_BUFFER, "Entry %d: handle=%d  %s",
                    curEntry, hnd,
                    (hnd != 0) ? "READY" : "(not loaded)");
            PrintText8x14(2, y, 0x8F, 0);
            y += 16;
        }

        // Controls legend at bottom
        y = 215;
        sprintf(PRINT_TEXT_BUFFER, "L/R:Bank ENT:Load UP/DN:Sel SPC:Play S:Stop L:Loop W/X:Vol ESC:Exit");
        PrintText8x14(2, y, 0x5F, 0);

        // --- Input handling with edge detection ---
        int keys = 0;
        if (plat_key_state(VK_UP)     & 0x8000) keys |= 0x001;
        if (plat_key_state(VK_DOWN)   & 0x8000) keys |= 0x002;
        if (plat_key_state(VK_LEFT)   & 0x8000) keys |= 0x004;
        if (plat_key_state(VK_RIGHT)  & 0x8000) keys |= 0x008;
        if (plat_key_state(VK_RETURN) & 0x8000) keys |= 0x010;
        if (plat_key_state(VK_SPACE)  & 0x8000) keys |= 0x020;
        if (plat_key_state('S')       & 0x8000) keys |= 0x040;
        if (plat_key_state('W')       & 0x8000) keys |= 0x080;
        if (plat_key_state('X')       & 0x8000) keys |= 0x100;
        if (plat_key_state('L')       & 0x8000) keys |= 0x200;
        if (plat_key_state(VK_ESCAPE) & 0x8000) keys |= 0x400;

        int newKeys = keys & ~prevKeys;
        prevKeys = keys;

        if (newKeys & 0x001) { if (curEntry > 0)  curEntry--; }
        if (newKeys & 0x002) { if (curEntry < 15) curEntry++; }
        if (newKeys & 0x004) { if (curBank > 0)   { curBank--; loadedBank = -1; curEntry = 0; } }
        if (newKeys & 0x008) { if (curBank < 15)  { curBank++; loadedBank = -1; curEntry = 0; } }

        if (newKeys & 0x010) {  // ENTER — load bank
            if (dsReady) {
                char dbg[128];
                sprintf(dbg, "[DEBUG] SFX Player: loading bank %d (%s)\n", curBank, bankNames[curBank]);
                OutputDebugStringA(dbg);
                LoadSoundBank(curBank, NULL);
                loadedBank = curBank;
                g_SfxVolume = sfxVolume;
                curEntry = 0;
            }
        }

        if (newKeys & 0x020) {  // SPACE — play SFX
            if (loadedBank >= 0) {
                int hnd = g_SfxBanks[curEntry * 2];
                if (hnd != 0) {
                    char dbg[128];
                    sprintf(dbg, "[DEBUG] SFX Player: play entry %d (hnd=%d, loop=%d)\n",
                            curEntry, hnd, loopMode);
                    OutputDebugStringA(dbg);
                    set_volume(hnd, sfxVolume);
                    playSnd(hnd, loopMode ? 1 : 0);
                }
            }
        }

        if (newKeys & 0x040) {  // S — stop SFX
            if (loadedBank >= 0) {
                int hnd = g_SfxBanks[curEntry * 2];
                if (hnd != 0) {
                    OutputDebugStringA("[DEBUG] SFX Player: stop\n");
                    setSndStop(hnd);
                }
            }
        }

        if (newKeys & 0x080) {  // W — volume up
            sfxVolume += 500;
            if (sfxVolume > -1) sfxVolume = -1;
            g_SfxVolume = sfxVolume;
            if (loadedBank >= 0) {
                int hnd = g_SfxBanks[curEntry * 2];
                if (hnd != 0) set_volume(hnd, sfxVolume);
            }
        }

        if (newKeys & 0x100) {  // X — volume down
            sfxVolume -= 500;
            if (sfxVolume < -9999) sfxVolume = -9999;
            g_SfxVolume = sfxVolume;
            if (loadedBank >= 0) {
                int hnd = g_SfxBanks[curEntry * 2];
                if (hnd != 0) set_volume(hnd, sfxVolume);
            }
        }

        if (newKeys & 0x200) { loopMode ^= 1; }   // L — toggle loop

        if (newKeys & 0x400) { break; }           // ESC — exit

        Task_sleep(1);
    }

    // Cleanup: destroy loaded bank before leaving
    if (loadedBank >= 0) {
        for (int i = 0; i < 32; i += 2) {
            if (g_SfxBanks[i] != 0) {
                destroySndBank(g_SfxBanks[i]);
                g_SfxBanks[i] = 0;
            }
            g_SfxBanks[i + 1] = 0;
        }
        OutputDebugStringA("[DEBUG] SFX Player: banks cleaned up, exiting\n");
    }

    Task_chain((void*)logos_state);
}
