// PrintText.cpp - Font rendering functions for fontus.tim
//
// ============================================================================
// FONT TEXTURE STRUCTURE (fontus.tim - 256x256 4-bit TIM)
// ============================================================================
//
// The fontus.tim texture is a 256x256 4-bit TIM image containing three font
// regions used for all in-game text rendering.
//
// --- Region 1: 8x8 ASCII font (PrintText8x8) ---
//   Position: (u=0, v=0)
//   Size:     256 x 24 pixels
//   Glyph:    8x8 pixels
//   Grid:     32 columns x 3 rows = 96 glyphs
//   Encoding: Standard ASCII starting at 0x20 (space)
//   Mapping:  texU = (char * 8) & 0xFF   (unsigned char overflow handles mod 32)
//             texV = ((char - 0x20) & 0xE3) >> 2   (equivalent to ((char-0x20)/32)*8)
//
//   Layout (32 chars per row):
//     Row 0 (v=0):   ` !"#$%&'()*+,-./0123456789:;<=>?`
//     Row 1 (v=8):   `©ABCDEFGHIJKLMNOPQRSTUVWXYZ[¥]^_`
//     Row 2 (v=16):  ` abcdefghijklmnopqrstuvwxyz{|}~ `
//
// --- Region 2: 8x14 game text font (PrintText8x14, PrintFormattedText) ---
//   Position: (u=0, v=28)
//   Size:     144 x 112 pixels
//   Glyph:    8x14 pixels
//   Grid:     18 columns x 8 rows = 144 character indices
//   Encoding: Custom index-based encoding (NOT standard ASCII)
//   Mapping:  texU = (index % 18) * 8
//             texV = (index / 18) * 14
//
//   Character table (index 0-143, 18 chars per row):
//     Row 0 (  0- 17): [ ][ ][►][L2][R2][L1][R1][△][○][×][□][▼][0][1][2][3][4][5]
//     Row 1 ( 18- 35): [6][7][8][9][:][;][,][”][!][?][‽][A][B][C][D][E][F][G]
//     Row 2 ( 36- 53): [H][I][J][K][L][M][N][O][P][Q][R][S][T][U][V][W][X][Y]
//     Row 3 ( 54- 71): [Z][(][\][)]['][-][·][a][b][c][d][e][f][g][h][i][j][k]
//     Row 4 ( 72- 89): [l][m][n][o][p][q][r][s][t][u][v][w][x][y][z][Ä][ä][Ö]
//     Row 5 ( 90-107): [ö][Ü][ü][ß][À][à][Â][â][È][è][É][é][Ê][ê][Ï][ï][Î][î]
//     Row 6 (108-125): [Ô][ô][Ù][ù][Û][û][Ç][ç][S.][T.][A.][R.][“][.][…][–][–][+]
//     Row 7 (126-143): [=][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]
//
//   Special: ASCII '(' (0x28=40) and ')' (0x29=41) in text data are remapped
//   to controller button symbols at (56,224) and (70,224) respectively.
//
// --- Region 3: 14x14 controller symbols (used via PrintFormattedText 0xF8) ---
//   Position: (u=0, v=140)
//   Size:     168 x 14 pixels
//   Glyph:    14x14 pixels
//   Grid:     12 columns x 1 row = 12 glyphs
//   Encoding: Accessed via PrintFormattedText escape 0xF8 + byte
//   Mapping:  texU = (byte % 18) * 8
//             texV = ((byte / 18) + 15) * 14
//
//   Layout:
//     [L2][R2][L1][R1][△][○][×][□][U.][R.][D.][L.]
//
// ============================================================================
// JAPANESE FONT (data\FONT.TIM - 768x256 4-bit TIM)
// ============================================================================
//
// The Japanese release ships a wider sheet with 14x14 game-text glyphs. The
// 8x8 ASCII region is unchanged; the game-text glyphs occupy TWO 256-wide
// texture pages, both 18 columns:
//
//   left  page  u   0..251, v = 28 + row*14, rows 0..15
//   right page  u 256..507, v =      row*14, rows 0..17   (kanji)
//
// The renderers below already compute the row/column exactly as the Japanese
// originals do (PrintFormattedText 0x00491490, PrintText8x14 0x00491830,
// message_render_chars 0x00492360) - what selects the page is TextureDesc's
// `depth`: 0x1E is the left page and 0x1F the right one, which is why the
// 0xF9/0xFA cases set 31. `texU` is a byte and cannot reach 256, so
// AddTintSprite adds the page offset (src/game/Rendering.cpp). fontus.tim is a
// single page wide and never sets 0x1F, so the USA path is untouched.
//
// Character map, escapes and the STR_JP() macro: docs/TEXT_ENCODING.md §8 and
// tools/jpn_font_table.py.
// ============================================================================

#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include "../system/AssetPath.h"
#include "SpriteRenderer.h"

// ============================================================================
// PrintText8x8 (0x00455420)
// Renders a null-terminated string from PRINT_TEXT_BUFFER using the 8x8 ASCII
// font region of fontus.tim.
//
// color:  upper 4 bits = brightness/fade (0=default 2, range 0-30)
//         lower 4 bits = CLUT tint index
// shadow: 0 = normal text, 1 = draw with shadow (offsets CLUT index by +8)
// ============================================================================
void PrintText8x8(short x, short y, unsigned char color, char shadow)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL) return;
    if (pD3D->m_FontTexHandle == MARNI_NULL_HANDLE || pD3D->m_FontTexWidth <= 0 || pD3D->m_FontTexHeight <= 0) return;

    unsigned char brightness = color >> 4;
    if (brightness == 0) brightness = 2;

    unsigned char clutIndex = color & 0xF;

    g_TextureDesc.flags = (unsigned int)(shadow != 0) * 0x40000000 + 0x40;
    g_TextureDesc.width = 8;
    g_TextureDesc.height = 8;
    g_TextureDesc.texturePage = 30;
    g_TextureDesc.screenX = x - g_ScreenOffsetX;
    g_TextureDesc.screenY = y - g_ScreenOffsetY;
    g_TextureDesc.colorMulR = 128;
    g_TextureDesc.colorMulG = 128;
    g_TextureDesc.colorMulB = 128;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.pivotY = 0;
    unk_00be1180 = 0;

    if (shadow != 0) {
        clutIndex = clutIndex + 8;
    }

    g_TextureDesc.clutX = 0x100;
    g_TextureDesc.clutY = clutIndex + 0x1E0;

    for (int i = 0; PRINT_TEXT_BUFFER[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)PRINT_TEXT_BUFFER[i];

        if (ch == ' ') {
            g_TextureDesc.screenX += 8;
            continue;
        }

        // texU: (ch * 8) stored as unsigned char — overflow wraps correctly
        // because the font has 32 columns (32*8 = 256 = 0 mod 256)
        g_TextureDesc.texU = (unsigned char)(ch * 8);

        // texV: ((ch - 0x20) & 0xE3) >> 2 — compiler optimization for
        // ((ch - 0x20) / 32) * 8, selects the correct row (0, 8, or 16)
        g_TextureDesc.texV = (unsigned char)(((ch - 0x20) & 0xE3) >> 2);

        AddTintSprite(&g_TextureDesc, brightness);

        g_TextureDesc.screenX += 8;
    }
}

// ============================================================================
// PrintText8x14 (0x00455520)
// Renders a null-terminated string from PRINT_TEXT_BUFFER using the 8x14
// game text font region of fontus.tim.
//
// color:  bit 7 = if set, use maximum brightness (30)
//         bits 4-6 = brightness/fade (0=default 2, range 0-30)
//         bits 0-3 = CLUT tint index
// flags:  0 = normal, nonzero = add shadow flag (0x40000000)
//
// The text data uses the custom fontus.tim encoding (NOT ASCII).
// Special: '(' (40) and ')' (41) are remapped to controller symbols.
// ============================================================================
void PrintText8x14(short x, short y, unsigned char color, char flags)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL) return;

    if (pD3D->m_FontTexHandle == MARNI_NULL_HANDLE || pD3D->m_FontTexWidth <= 0 || pD3D->m_FontTexHeight <= 0) {
        OutputDebugStringA("[TEXT] Font not loaded yet, skipping\n");
        return;
    }

    unsigned char brightness;
    if ((color & 0x80) == 0) {
        brightness = color >> 4;
        if (brightness == 0) brightness = 2;
    } else {
        brightness = 30;
    }

    // The game-text glyph width differs by region: the USA/GOG font (fontus.tim)
    // uses 8px-wide glyphs in an 18-column grid, while the Japanese font (FONT.TIM)
    // uses 14px-wide glyphs in the same 18-column grid (see Biohazard.exe's JPN
    // renderers FUN_004912c0/FUN_00491490 - texU=(ch%18)*14, advance 14).
    const int glyphW = (GetAssetVersion() != 0) ? 14 : 8;

    g_TextureDesc.flags  = ((flags != 0) ? 0x40000000U : 0U) + 0x40;

    g_TextureDesc.width  = glyphW;
    g_TextureDesc.height = 14;
    g_TextureDesc.texturePage = 30;

    g_TextureDesc.screenX = x - g_ScreenOffsetX;
    g_TextureDesc.screenY = y - g_ScreenOffsetY;

    g_TextureDesc.colorMulR = 128;
    g_TextureDesc.colorMulG = 128;
    g_TextureDesc.clutX = 256;
    g_TextureDesc.colorMulB = 128;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.pivotY = 0;
    g_TextureDesc.clutY = (color & 0xF) + 0x1E0;

    unk_00be1180 = 0;

    for (int i = 0; PRINT_TEXT_BUFFER[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)PRINT_TEXT_BUFFER[i];

        if (ch == ' ') {
            g_TextureDesc.screenX += glyphW;
            continue;
        }

        g_TextureDesc.texU = (ch % 18) * glyphW;
        g_TextureDesc.texV = (ch / 18) * 14;

        // ASCII '(' and ')' are remapped to controller button symbols at
        // (56,224)/(70,224) in BOTH the USA and Japanese fonts.
        if (ch == 40) {
            g_TextureDesc.texU = 56;
            g_TextureDesc.texV = 224;
        }
        if (ch == 41) {
            g_TextureDesc.texU = 70;
            g_TextureDesc.texV = 224;
        }

        unsigned char finalBrightness = brightness;
        if ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_CONTROL_ROOM) && (g_roomCameraId == 4)) {
            finalBrightness = 0;
        }

        AddTintSprite(&g_TextureDesc, finalBrightness);

        g_TextureDesc.screenX += glyphW;
    }
}

// ============================================================================
// PrintFormattedText (0x00455190)
// Renders formatted text using the 8x14 font from fontus.tim.
// Unlike PrintText8x14, this reads from a caller-provided byte array with
// special control codes for extended characters and layout.
//
// Control codes:
//   0x00    - End of visible text (advance cursor, continue parsing)
//   0x01    - End of text block (return)
//   0x07    - End of text block (return)
//   0xF8 nn - 14x14 controller symbol (nn indexes into font region 3)
//   0xF9 nn - 8x14 character with depth=31 (nn = encoding byte)
//   0xFA nn - 8x14 character with depth=31, row offset +14
//   0xFB    - True no-op (no glyph, no advance). The original strings pad
//             every glyph with 0xFB spacers; it must not move the cursor.
//   0xFF    - Half-space advance (4 pixels instead of 8)
//
// color:  upper 4 bits = brightness/fade (0=default 2)
//         lower 4 bits = CLUT tint index
// ============================================================================
void PrintFormattedText(short x, short y, unsigned char color, const unsigned char* data)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D == NULL) return;
    if (pD3D->m_FontTexHandle == MARNI_NULL_HANDLE || pD3D->m_FontTexWidth <= 0 || pD3D->m_FontTexHeight <= 0) return;
    if (data == NULL) return;

    unsigned char brightness = color >> 4;
    if (brightness == 0) brightness = 2;

    // Glyph width is 8px for the USA/GOG font and 14px for the Japanese font,
    // both in an 18-column grid (Biohazard.exe JPN renderers FUN_004912c0/
    // FUN_00491490 use texU=(b%18)*14 and a +14 cursor advance).
    const int glyphW = (GetAssetVersion() != 0) ? 14 : 8;

    g_TextureDesc.height = 14;
    g_TextureDesc.flags = 0x40;
    g_TextureDesc.screenX = x - g_ScreenOffsetX;
    g_TextureDesc.clutX = 0x100;
    g_TextureDesc.screenY = y - g_ScreenOffsetY;
    g_TextureDesc.clutY = (color & 0xF) + 0x1E0;
    g_TextureDesc.width = glyphW;
    g_TextureDesc.colorMulR = 128;
    g_TextureDesc.colorMulG = 128;
    g_TextureDesc.colorMulB = 128;
    g_TextureDesc.pivotX = 0;
    g_TextureDesc.pivotY = 0;
    unk_00be1180 = 0;

    unsigned char chr = *data;

    while (chr != 1) {
        chr = *data;

        if (chr == 0) goto next_char;

        switch (chr) {
            case 1:
            case 7:
                return;

            case 0xF8: {
                data++;
                unsigned char nextByte = *data;
                chr = nextByte / 18 + 15;
                g_TextureDesc.texturePage = 30;
                g_TextureDesc.texV = chr * 14;
                g_TextureDesc.texU = (nextByte % 18) * glyphW;
                AddTintSprite(&g_TextureDesc, brightness);
                break;
            }

            case 0xF9: {
                unsigned char nextByte = data[1];
                chr = nextByte / 18;
                g_TextureDesc.texturePage = 31;
                data++;
                g_TextureDesc.texV = chr * 14;
                g_TextureDesc.texU = (*data % 18) * glyphW;
                AddTintSprite(&g_TextureDesc, brightness);
                break;
            }

            case 0xFA: {
                unsigned char nextByte = data[1];
                chr = nextByte / 18 + 14;
                g_TextureDesc.texturePage = 31;
                data++;
                g_TextureDesc.texV = chr * 14;
                g_TextureDesc.texU = (*data % 18) * glyphW;
                AddTintSprite(&g_TextureDesc, brightness);
                break;
            }

            case 0xFB:
                // True no-op: skip without advancing (matches the original's
                // `case 0xfb: break;` — no cursor move, no glyph).
                data++;
                chr = *data;
                continue;

            case 0xFF:
                g_TextureDesc.screenX += glyphW / 2;
                data++;
                chr = *data;
                continue;

            default:
                chr = chr / 18 + 2;
                g_TextureDesc.texturePage = 30;
                g_TextureDesc.texV = chr * 14;
                g_TextureDesc.texU = (*data % 18) * glyphW;
                AddTintSprite(&g_TextureDesc, brightness);
                break;
        }

    next_char:
        g_TextureDesc.screenX += glyphW;
        data++;
        chr = *data;
    }
}
