// TitleScreen.cpp - Title screen rendering and state management
// All functions decompiled from Ghidra with original addresses
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "FileLoader.h"
#include "SpriteRenderer.h"
#include "SFXIds.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>          // memcpy, for the DC title sheet assembly
#include "../system/AssetPath.h"

extern void logos_state(void);

// ============================================================================
// set_display_resolution (0x00401000)
// ============================================================================
void set_display_resolution(int w, int h, int mode)
{
    g_displayWidth = w;
    g_displayHeight = h;
    g_displayMode = mode;
}

// ============================================================================
// check_save_files_exist (0x00494190)
// Returns 1 if any save files exist, 0 otherwise.
// ============================================================================
int check_save_files_exist(void)
{
    char path[260];
    for (int i = 1; i <= 8; i++) {
        sprintf(path, "%ssavedat%d.dat", GetSaveRoot(), i);
        FILE* f = fopen(path, "rb");
        if (f != NULL) {
            fclose(f);
            return 1;
        }
    }
    return 0;
}

// ============================================================================
// title_setup_texture_pages (0x00470970)
// Creates texture pages for button prompt images.
// ============================================================================
void title_setup_texture_pages(int slot, int mode)
{
    // Same legacy-descriptor caveat as TextureLoader: these tables are indexed
    // by slot * 0x37C and the port's stand-ins are a few KB, so the room-load
    // calls (load_room_bg passes the camera index, 0..7) run off the end. The
    // DX11 path takes page state from the g_TexturePage* arrays, so reads that
    // fall out of range can yield 0 - but the destroy loop's writes must not
    // happen at all.
    int slotBase = slot * 0x37C;
    const bool cntOk   = (size_t)slotBase / sizeof(DWORD)
                         < sizeof(g_VideoDriverArray_814) / sizeof(DWORD);
    const bool tableOk = (size_t)slotBase + 8 * sizeof(DWORD) <= sizeof(g_TexturePageTable_DAT);
    const bool dataOk  = (size_t)slotBase + 2 * 0x68 <= sizeof(g_VideoDriverArray_4d0);

    int pageCount = cntOk ? g_VideoDriverArray_814[slotBase / 4] : 0;
    if (pageCount != 0 && tableOk) {
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + slotBase);
        for (int i = 0; i < pageCount; i++) {
            if ((size_t)slotBase + (size_t)(i + 1) * sizeof(DWORD)
                > sizeof(g_TexturePageTable_DAT)) {
                break;
            }
            if (pageTable[i] != 0) {
                destroy_texture_page(pageTable[i]);
                pageTable[i] = 0;
            }
        }
    }

    if (dataOk) {
        BYTE* pageData = (BYTE*)&g_VideoDriverArray_4d0 + slotBase;
        for (int i = 0; i < 2; i++) {
            int handle = create_texture_page(pageData, (mode != 0) ? 26 : 10);
            if (handle == 0) break;
            pageData += 0x68;
        }
    }

    g_titleTextureSlotId = slot;
}

// ============================================================================
// init_title_screen (0x004306e0)

// ============================================================================
// Director's Cut title option sheets, assembled from the disc's own
// DATA/BT367OAB.TIM.
//
// The DC keeps its seven 256x80 option cells in one file, 0x2840 bytes each:
//
//   +0x00  u32 0x10, u32 8            magic, then 4bpp-with-CLUT
//   +0x08  u32 44, s16 x, s16 y,      CLUT block: 16 entries, one row
//          u16 16, u16 1, 16 x u16
//   +0x34  u32 10252, s16 x, s16 y,   image block: width is in 16-bit units,
//          u16 64, u16 80             so 64 means 256 pixels
//   +0x40  10240 bytes                4bpp pixels, two per byte, low nibble
//                                     is the left pixel
//
// 7 x 80 = 560 rows cannot live on one texture page - TextureDesc.texV is a
// byte, so a page tops out at 256 rows. The cells are therefore split across
// two pages. That split used to happen offline, in tools/port_dc_assets.py,
// which wrote t_dc.tim and t_dc2.tim; doing it here instead means the DC asset
// tree holds the file the player's own disc has rather than two invented ones.
//
//   page A (slot 12)  cells 0,1,2 at full height  -> 256x240, the main menu
//   page B (slot 13)  cells 3,4,5,6 cropped to 64 -> 256x256, the difficulty
//                                                    submenu
//
// The crop keeps each cell's row 0, so drawing every row at screenY 0 (plus the
// port's +38) lands where the PS1 draws it. Cell 6 is the ADVANCED* frame: the
// same art as cell 5 but green, and since the port keeps ONE palette per page,
// its colour 1 is remapped onto the shared palette's unused entry 5 rather than
// carried as a second CLUT row.
// ============================================================================
#define DC_TITLE_CHUNK      0x2840
#define DC_TITLE_CELL_W     256
#define DC_TITLE_CELL_H     80
#define DC_TITLE_ROW_BYTES  (DC_TITLE_CELL_W / 2)   /* 4bpp: 128 B per row */
#define DC_TITLE_CLUT_OFF   0x14
#define DC_TITLE_PIX_OFF    0x40
#define DC_TITLE_GREEN_SLOT 5

// Build one 4bpp TIM page into `out`. `cells` lists the source cell indices,
// `rows` how many rows of each to take, `palFrom` which of them supplies the
// page palette, and `greenFrom` which (an index INTO cells, or -1) needs the
// colour-1 -> colour-5 remap described above. Returns the bytes written.
static unsigned int dc_title_build_page(const unsigned char* file,
                                        const int* cells, int count, int rows,
                                        int palFrom, int greenFrom,
                                        unsigned char* out)
{
    const int h = rows * count;
    const unsigned int pixBytes = (unsigned int)(DC_TITLE_CELL_W * h / 2);
    unsigned char* p = out;

    *(unsigned int*)p = 0x10;      p += 4;
    *(unsigned int*)p = 8;         p += 4;      // 4bpp + CLUT
    *(unsigned int*)p = 12 + 32;   p += 4;      // CLUT block length
    *(short*)p = 0;                p += 2;
    *(short*)p = 0x1E0;            p += 2;      // the PS1's own clutY
    *(unsigned short*)p = 16;      p += 2;
    *(unsigned short*)p = 1;       p += 2;
    memcpy(p, file + cells[palFrom] * DC_TITLE_CHUNK + DC_TITLE_CLUT_OFF, 32);
    if (greenFrom >= 0) {
        // the ADVANCED* cell's own colour 1 - pure green - into the spare slot
        *(unsigned short*)(p + DC_TITLE_GREEN_SLOT * 2) =
            *(const unsigned short*)(file + cells[greenFrom] * DC_TITLE_CHUNK
                                     + DC_TITLE_CLUT_OFF + 2);
    }
    p += 32;

    *(unsigned int*)p = 12 + pixBytes;                     p += 4;
    *(short*)p = 0;                                        p += 2;
    *(short*)p = 0;                                        p += 2;
    *(unsigned short*)p = (unsigned short)(DC_TITLE_CELL_W / 4); p += 2;
    *(unsigned short*)p = (unsigned short)h;               p += 2;

    for (int i = 0; i < count; i++) {
        const unsigned char* cell =
            file + cells[i] * DC_TITLE_CHUNK + DC_TITLE_PIX_OFF;
        unsigned int n = (unsigned int)(rows * DC_TITLE_ROW_BYTES);
        memcpy(p, cell, n);
        if (i == greenFrom) {
            for (unsigned int k = 0; k < n; k++) {
                unsigned char lo = (unsigned char)(p[k] & 0xF);
                unsigned char hi = (unsigned char)(p[k] >> 4);
                if (lo == 1) lo = DC_TITLE_GREEN_SLOT;
                if (hi == 1) hi = DC_TITLE_GREEN_SLOT;
                p[k] = (unsigned char)(lo | (hi << 4));
            }
        }
        p += n;
    }
    return (unsigned int)(p - out);
}

// ============================================================================
void init_title_screen(void)
{
    g_bGameActive = 0;
    set_display_resolution(320, 240, 0);

    g_roomCameraId = 0;

    // The Director's Cut's own title art ("DIRECTOR'S CUT") is its overlay's
    // title.pix, so this stays one unconditional load - the base tree's file is
    // never touched and OG still gets today's screen.
    LoadFile(GAME_DATA_ROOT "data\\title.pix", g_TimImageBuffer__bitmap, 0x20);
    display_image(0, g_TimImageBuffer__bitmap, 320, 240);

    title_setup_texture_pages(0, 1);

    //empty_00470960(0);

    // The DC's cells come from one file; stage it whole, because page B below
    // needs it again. Everything else loads a ready-made sheet.
    if (g_bDcMode) {
        static const int kTitlePageA[3] = { 0, 1, 2 };
        LoadFile(GAME_DATA_ROOT "data\\bt367oab.tim", g_bgPakLoadBuffer, 0x20);
        dc_title_build_page(g_bgPakLoadBuffer, kTitlePageA, 3, DC_TITLE_CELL_H,
                            1, -1, g_TimImageBuffer__bitmap);
    } else {
        const char* buttonTexPath = !g_bPadConnected
            ? GAME_DATA_ROOT "data\\t_press.tim"
            : GAME_DATA_ROOT "data\\t_start.tim";
        LoadFile(buttonTexPath, g_TimImageBuffer__bitmap, 0x20);
    }

    g_titleTexturePageData[4] = 26;
    g_titleTexturePageData[0] = 8;
    g_TextureCurrentPage = 26;
    g_TextureBankID = 8;
    LoadTexturePage(g_TimImageBuffer__bitmap, 8, 0, 12, 4, 0, 0, 0);

    g_titleTexturePageData[1] = g_TextureBankID;
    g_titleLoopFlag = 1;
    g_titleTexturePageData[5] = g_TextureCurrentPage;
    g_titleTexturePageData[2] = g_titleTexturePageData[1];
    g_titleTexturePageData[6] = g_titleTexturePageData[5];

    // The DC submenu sheet is a second texture page: slot 13 -> SRV 28, beside
    // the main sheet's slot 12 -> SRV 27. Rows 3-6 draw from it (see the table).
    if (g_bDcMode) {
        static const int kTitlePageB[4] = { 3, 4, 5, 6 };
        dc_title_build_page(g_bgPakLoadBuffer, kTitlePageB, 4, 64, 0, 3,
                            g_TimImageBuffer__bitmap);
        LoadTexturePage(g_TimImageBuffer__bitmap, 9, 0, 13, 0, 0, 0, 0);
        g_titleTexturePageData[3] = 9;
        g_titleTexturePageData[4] = 9;
        g_titleTexturePageData[5] = 9;
        g_titleTexturePageData[6] = 9;
    }

    {
        char dbg[256];
        sprintf(dbg, "[INIT] LoadTexturePage done, SRV[27]=%p\n", g_TexturePageSRV[27]);
        OutputDebugStringA(dbg);
    }


    if (check_save_files_exist()) {
        g_titleSelectionId = 2;
        g_main_state_flags &= ~MSF_SCREEN_MODE_MASK;
        return;
    }
    g_titleSelectionId = 1;
    g_main_state_flags &= ~MSF_SCREEN_MODE_MASK;
}

// ============================================================================
// set_scene_render_param (0x0040a8e0)
// ============================================================================
void set_scene_render_param(int value)
{
    g_sceneRenderParam = value;
}

// ============================================================================
// title_exit_loop (0x00430e10)
// ============================================================================
void title_exit_loop(void)
{
    g_titleLoopFlag = 0;
    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
}

// ============================================================================
// fade_update (0x0047b950)
// ============================================================================
void fade_update(void)
{
    if (g_fading_state <= 0 && g_fading_counter != 0) {
        if (g_fading_counter <= 0) {
            g_fading_state = 0x7FFF;
        } else {
            g_fading_state = 0;
        }
    }
}

// ============================================================================
// read_sidewinder_pad (0x00497e30)
// Original: return g_pMasterInputState.field466_0x200 (joystick[0].currPress)
// ============================================================================
int read_sidewinder_pad(void)
{
    return g_pMasterInputState.joysticks[0].currPress;
}

// ============================================================================
// TitleTextPosData and UpdateTitleTextSprite
// ============================================================================
struct TitleTextPosData {
    int vramY;
    int sprHeight;
    int screenY;
    int slot;       // display_texture SRV slot (12 = the main sheet, 13 = the DC submenu sheet)
};

// USA sheet (data\t_press.tim / data\t_start.tim, 256x256): the PC build
// re-cropped the three frames out of the PS1's 80-row cells, hence the per-row
// screenY and the shorter heights.
static const TitleTextPosData g_titleTextPosTableUsa[3] = {
    { 0,  54, 24, 12 },  // index 0: "PRESS ANY BUTTON" / "PRESS START BUTTON"
    { 83, 70,  8, 12 },  // index 1: "NEW GAME"
    { 175,70,  8, 12 },  // index 2: "LOAD GAME"
};

// Director's Cut (data\t_dc.tim + data\t_dc2.tim, built by
// tools/port_dc_assets.py). The PS1 draws every option as a whole 256x80 cell at
// a fixed screenX -130 / screenY 38 (title_draw_option, TITLE.EXE 0x800e14d8),
// so screenY is 0 here and the +38 in UpdateTitleTextSprite supplies it; the
// cells keep their internal offsets. Rows 0-2 are the main menu (t_dc.tim);
// 3-6 are the STANDARD/TRAINING/ADVANCED submenu (t_dc2.tim, cropped to 64 rows
// per cell) - row 6 is ADVANCED held, drawn with the green palette entry.
static const TitleTextPosData g_titleTextPosTableDc[7] = {
    {   0, 80, 0, 12 },  // 0: PRESS ANY BUTTON
    {  80, 80, 0, 12 },  // 1: NEW GAME
    { 160, 80, 0, 12 },  // 2: LOAD GAME
    {   0, 64, 0, 13 },  // 3: STANDARD
    {  64, 64, 0, 13 },  // 4: TRAINING
    { 128, 64, 0, 13 },  // 5: ADVANCED
    { 192, 64, 0, 13 },  // 6: ADVANCED, confirm held
};

// ============================================================================
// UpdateTitleTextSprite (0x00430d40)
// ============================================================================
void UpdateTitleTextSprite(unsigned char brightness, unsigned char selectionId)
{
    TextureDesc* td = &g_TextureDesc;

    td->flags = 0x10000000;
    if (brightness != 0x80) {
        td->flags = 0x40000000;
    }

    const TitleTextPosData* entry;
    if (g_bDcMode) {
        if (selectionId > 6) selectionId = 0;
        entry = &g_titleTextPosTableDc[selectionId];
    } else {
        if (selectionId > 2) selectionId = 0;
        entry = &g_titleTextPosTableUsa[selectionId];
    }

    td->texU = 0;
    td->screenX = -130;
    td->texturePage = g_titleTexturePageData[selectionId];
    td->width = 256;
    td->texV = (unsigned char)entry->vramY;
    td->height = entry->sprHeight;
    td->screenY = entry->screenY + 38;
    // g_titleCurrentSprH = (float)entry->sprHeight;

    td->colorMulR = brightness;
    td->clutX = 0;
    td->colorMulG = brightness;
    td->pivotX = 0;
    td->pivotY = 0;
    td->colorMulB = brightness;

    td->clutY = 0x1E0;

    display_texture(td, 2, entry->slot, 1);
}

// ============================================================================
// update_title_options (0x00430810)
// ============================================================================
void update_title_options(void)
{
	DWORD sidewinderPress = 0;
	DWORD sidewinderState = 0;
	if (g_bPadConnected) {
		sidewinderState = read_sidewinder_pad();
		sidewinderPress = sidewinderState & 0x10000 & ~g_PlayerPadHeldPrev;
	}
	g_PlayerPadHeldPrev = sidewinderState;

    if (g_titleMode != 0) {
        if (g_titleMode != 1) return;

        switch (g_titleOptionsFading) {
        case 10:
            // DC difficulty submenu (PS1 TITLE.EXE g_titleState 10). Up/down cycle
            // STANDARD(3) / TRAINING(4) / ADVANCED(5); holding confirm on
            // ADVANCED shows the green ADVANCED*(6) cell; confirm writes
            // g_DcDifficulty and then runs the original new-game exit.
            if (g_PlayerPadPressed & 0x5100) {
                if (!(g_PlayerPadPressed & 0x1000)) {
                    if (g_titleSelectionId < 5) g_titleSelectionId++;
                    else g_titleSelectionId = 3;
                } else {
                    if (g_titleSelectionId < 4) g_titleSelectionId = 5;
                    else g_titleSelectionId--;
                }
                g_titleHoldTimer = 0x5A;
                g_titleDemoTime = 0x708;
            }

            if ((g_PlayerPadPressed & 0xeff) || sidewinderPress) {
                play_sfx(SFX_BANKS, SFX_TITLE_EVIL01);
                play_sfx(SFX_BANKS, 1); // null sfx
                // The PS1 keys ADVANCED* off the hold timer, not the choice (it
                // drops the choice back to 5 after drawing the green cell).
                if (g_titleSelectionId == 5) {
                    g_DcDifficulty = (g_titleHoldTimer == 0) ? DC_DIFFICULTY_ADVANCED_HOLD
                                                            : DC_DIFFICULTY_ADVANCED;
                } else {
                    g_DcDifficulty = g_titleSelectionId - 3;   // 3 -> STANDARD, 4 -> TRAINING
                }
                // Keep the submenu choice: the PS1 leaves g_titleMenuChoice at
                // 3..5, so the exit fade-out keeps drawing the difficulty cell.
                // The exit switch maps 3..6 to the new-game action.
                g_titleOptionsFading = 6;
                g_fade_type_id = 1;
                g_fading_counter = 0x7F00;
                fade_update();
                g_bGameActive = 2;
                return;
            }

            // Hold Right on ADVANCED to reach the green ADVANCED* cell; the PS1
            // draws the green cell once and drops back to 5 so the mode stays
            // ADVANCED until confirm. The DC reads the *held* pad word
            // (0x800cf844); g_PlayerPadHeld is the edge-detected word here, so
            // use g_button_pressed_id (== g_RawPadHeld, continuous) with the raw
            // d-pad layout: 0x1000 up, 0x2000 right, 0x4000 down, 0x8000 left.
            if (g_titleSelectionId == 5 && (g_button_pressed_id & 0x2000)) {
                if (g_titleHoldTimer != 0) g_titleHoldTimer--;
            }
            if (g_titleHoldTimer == 0) g_titleSelectionId = 6;

            UpdateTitleTextSprite(128, g_titleSelectionId);
            if (g_titleSelectionId == 6) g_titleSelectionId = 5;

            g_titleDemoTime--;
            if (g_titleDemoTime == 0) {
                g_titleOptionsFading = 3;
                g_fade_type_id = 2;
                g_fading_counter = 0x400;
                fade_update();
            }
            return;

        case 0:
            g_titleOptionsFading = 1;
            g_fade_type_id = 2;
            g_fading_counter = 0xFC00;
            g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
            fade_update();
            return;

        case 1:
            if (g_fading_state < 0) {
                g_titleOptionsFading = 2;
                g_titleDemoTime = 0x708;
            }
            UpdateTitleTextSprite(128, g_titleSelectionId);
            return;

	case 2:
		UpdateTitleTextSprite(128, g_titleSelectionId);

		// DC: confirming NEW GAME opens the STANDARD/TRAINING/ADVANCED submenu
		// (state 10 below) instead of starting the game. The PS1 is silent here:
		// the title sfx plays on the *submenu* confirm, or on LOAD GAME, which
		// falls through to the original path below.
		if (g_bDcMode && ((g_PlayerPadPressed & 0xeff) || sidewinderPress) &&
		    g_titleSelectionId == 1) {
			g_titleOptionsFading = 10;
			g_titleSelectionId = 3;          // STANDARD
			g_titleDemoTime = 0x708;
			g_titleHoldTimer = 0x5A;
			return;
		}

		if ((g_PlayerPadPressed & 0xeff) || sidewinderPress) {
			play_sfx(SFX_BANKS, SFX_TITLE_EVIL01);
			play_sfx(SFX_BANKS, 1); // null sfx
			g_titleOptionsFading = 6;
			g_fade_type_id = 1;
			g_fading_counter = 0x7F00;
			fade_update();
			g_bGameActive = 2;
			return;
		}

		if (g_PlayerPadPressed & 0x5100) {
			if (!(g_PlayerPadPressed & 0x1100)) {
				if (g_titleSelectionId == 2) g_titleSelectionId = 0;
				g_titleSelectionId++;
			} else {
				g_titleSelectionId--;
				if (g_titleSelectionId == 0) {
					g_titleSelectionId = 2;
					g_titleDemoTime = 0x708;
					goto demo_reset;
				}
			}
			g_titleDemoTime = 0x708;
		}

	demo_reset:
            g_titleDemoTime--;
            if (g_titleDemoTime != 0) break;

            g_titleOptionsFading = 3;
            g_fade_type_id = 2;
            g_fading_counter = 0x400;
            fade_update();
            return;

        case 3:
            if (g_fading_state < 0) {
                g_titleSelectionId = 0;
                title_exit_loop();
                return;
            }
            UpdateTitleTextSprite(128, g_titleSelectionId);
            if ((g_PlayerPadPressed & 0xeff) == 0) {
                return;
            }
            g_titleOptionsFading = 0;
            g_fading_counter = 0xF000;
            return;

        case 4:
            if (g_fading_state < 0) {
                title_exit_loop();
                g_main_state_flags &= MSF_SCREEN_MODE_MASK;
                return;
            }
            UpdateTitleTextSprite(128, g_titleSelectionId);

        case 6:
            if (g_fading_state < 0) {
                g_titleOptionsFading = 7;
                g_fade_type_id = 1;
                g_fading_counter = 0xC000;
                fade_update();
                UpdateTitleTextSprite(128, g_titleSelectionId);
                return;
            }

        case 8:
            if (g_fading_state < 0) {
                g_titleOptionsFading = 9;
                g_fade_type_id = 1;
                g_fading_counter = 0xF800;
                fade_update();
                UpdateTitleTextSprite(128, g_titleSelectionId);
                return;
            }
            break;

        case 7:
            if (g_fading_state < 0) {
                g_titleOptionsFading = 8;
                g_fade_type_id = 1;
                g_fading_counter = 0x8000;
                fade_update();
                UpdateTitleTextSprite(0x80, g_titleSelectionId);
                return;
            }

        case 9:
            if (g_fading_state < 0) {
                g_titleOptionsFading = 4;
                g_fade_type_id = 2;
                g_fading_counter = 0x270;
                fade_update();
                UpdateTitleTextSprite(0x80, g_titleSelectionId);
                return;
            }

        default:
            break;
        }

        UpdateTitleTextSprite(0x80, g_titleSelectionId);
        return;
    }

    switch (g_titleOptionsFading) {
        case 0:
            g_titleOptionsFading = 1;
            g_titleDemoTime = 0x80;
            goto option_selected;
        case 1:
    option_selected:
            g_titleDemoTime -= 4;
            UpdateTitleTextSprite(-0x80 - (char)g_titleDemoTime, 0);
            if (g_titleDemoTime == 0) {
                g_titleOptionsFading = 2;
                g_titleDemoTime = 0x708;
            }
            if ((g_PlayerPadPressed & 0xeff) || sidewinderPress) {
                g_titleOptionsFading = 2;
                g_titleDemoTime = 0x708;
            }
            break;

        case 2:
            g_titleDemoTime--;
            UpdateTitleTextSprite(0x80, 0);
            if (g_titleDemoTime == 0) {
                g_titleOptionsFading = 3;
                g_fade_type_id = 2;
                g_fading_counter = 0x400;
                fade_update();
            }
            if ((g_PlayerPadPressed & 0xeff) || sidewinderPress) {
                g_titleMode = 1;
                g_titleOptionsFading = 2;
            }
            break;

        case 3:
            if (g_fading_state > 0x7B80) {
                g_fading_state = 0x7FFF;
                g_fading_counter = 0;
                g_titleSelectionId = 0;
                g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
                title_exit_loop();
            }
            if ((g_PlayerPadPressed & 0xeff) || sidewinderPress) {
                g_titleOptionsFading = 2;
                g_fading_state = -1;
                g_titleDemoTime = 0x708;
            }
            UpdateTitleTextSprite(0x80, 0);
            break;

        case 4:
            UpdateTitleTextSprite(0x80, 0);
            if (g_fading_state > 0x7B80) {
                g_titleMode = 1;
                g_titleOptionsFading = 0;
                g_fading_state = 0x7FFF;
                g_fading_counter = 0;
                g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
            }
            break;
    }
}

// ============================================================================
// title_state (0x00430470)
// Title screen state: displays title, waits for player selection,
// then chains to game_start, logos_state, or characterSelectionScreen.
// ============================================================================
void title_state(void)
{
    int i;

	g_main_state_flags &= ~MSF_INTENSITY_RAMP;
	g_PlayerPadHeldPrev = 0;
	g_PlayerPadHeld = 0;
	g_RawPadHeld = 0;
	g_PlayerPadPressed = 0;
	g_playingGameFlag = 0;
	g_menu_choice_id = 0;

    setMenuScreenOffset(320, 240, 0, 0, 0);
    CenterScreenOrigin();
    clear_textures();
    set_scene_render_param(0xc0);
    sounds_reset();

    g_loadDataDestPointer = g_DataBuffer;
    // The USA title_state (0x00430470) loads bank 12 (EVIL01 - the "Resident
    // Evil" voice), the Japanese one (FUN_0048dc20, 0x0048dc20 in
    // Biohazard.exe) loads bank 11 (BIO01 - the "Bio Hazard" voice). Both banks
    // have slot 0 as the logo voice and 13/14/15 as Cancel/Type01/Type02, so the
    // play_sfx ids in update_title_options are unchanged; only the bank differs.
    LoadSoundBank(GetAssetVersion() != 0 ? BANK_BIO : BANK_TITLE, g_DataBuffer);

    g_fading_state = -1;
    g_titleLoopFlag = 0;
    g_SpecialRoomLightState = 0xFFFF;
    g_titleMode = 0;
    g_titleOptionsFading = 0;

    init_title_screen();

    // empty_00497c10(0);
    // empty_0040abb0((void*)0, 0, 0, 0);

    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
    // 0x0047b950 call site: the original calls 0x00483510 here, a stub that
    // just returns 0 - call dropped

    setMenuScreenOffset(320, 240, 0, 0, 1);
    Task_sleep(1);

    if (g_fmvPlayCount < 1) {
        g_selectedFmvId = 0;
        g_fmvDataPointer = g_loadDataDestPointer;
        g_fmvPlayCount = 0x10;
        g_main_state_flags = g_main_state_flags | MSF_FMV_REQUEST;
        Task_sleep(1);
    }

    g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_REBUILD;
    Task_sleep(1);

    do {
        update_title_options();
        Task_sleep(1);
    } while (g_titleLoopFlag != 0);

    // legacy gpu wait
    if (g_GPU_VENDOR_ID == 1) {
        for (i = 180; i != 0; i--) {
            Task_sleep(1);
        }
    }

    cleanup_texture_slot(12);

    // DC: the difficulty submenu leaves the choice at 3..5 (and 6 for ADVANCED*
    // while the green cell is drawn). The PS1's title_state groups 1 and 3..6
    // together - chain the character select and let it start the game, exactly
    // like NEW GAME. Without this the choice 3 would fall into the USA build's
    // load-game case below.
    if (g_bDcMode && g_titleSelectionId >= 3) {
        nullsub_0047eb80();
        Task_chain((void*)characterSelectionScreen);
        return;
    }

    switch (g_titleSelectionId) {
    case 0:
        nullsub_0047eb80();
        g_main_state_flags2 |= MSF2_ATTRACT_DEMO;
        Task_chain((void*)game_start);
        Task_chain((void*)logos_state);
        return;

    case 1:
        nullsub_0047eb80();
        Task_chain((void*)characterSelectionScreen);

        // The character select starts the game itself (CharacterSelectionScreen
        // chains game_start once the player confirms a character), so the DC
        // path stops here. Falling into the load screen below would show it for
        // a NEW GAME, and that screen's exit option chains back to title_state -
        // which is what made the DC difficulty confirm bounce back to the
        // NEW GAME / LOAD GAME menu.
        //
        // On the PS1 the new-game branch chains the SELECT overlay and then
        // calls its save-state loader; that loader is NOT the interactive
        // screen, but the port only has the interactive one, so it must not run
        // for a new game.
        if (g_bDcMode) {
            return;
        }

    case 2:
    case 3:
        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
        LoadSaveGameState(1, 0x80180000, 0, 1, 0);
        g_loadSaveStateFlag = 0;
        Game_timer = g_gameTimerSnapshot;
        g_main_state_flags = (g_main_state_flags & ~MSF_SCREEN_MODE_MASK) | MSF_SCREEN_STANDALONE;
        nullsub_0047eb80();
        Task_chain((void*)game_start);

    default:
        return;
    }
}

// nullsub_0047eb80 - empty no-op in the original PC build.
// likely a PS1 version function stripped during the PC port.
void nullsub_0047eb80(void) { }
