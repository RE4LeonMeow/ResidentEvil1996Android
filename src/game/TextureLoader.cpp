// TextureLoader.cpp - PSX TIM/PIX texture processing
#include "../Globals.h"
#include "../marni/PSXTexture.h"
#include "../marni/MarniSystem.h"
#include "../marni/MarniBits.h"
#include "../DebugPrint.h"
#include <stdio.h>
#include <cstdlib>
#include <cstring>

#undef LoadImage  // Win32 WinUser.h macro conflicts with Marni LoadImage

// ============================================================================
// CLUT cache — stores parsed pixel data + all CLUT palettes per texture slot
// so we can rebuild the SRV with a different CLUT palette index.
// ============================================================================
struct TextureCLUTCache {
    BYTE*  pixelData;     // copy of the indexed pixel data (4bpp nibble-packed or 8bpp)
    int    pixelDataSize; // size in bytes
    WORD*  clutData;      // copy of all CLUT palettes (clutW * clutH entries)
    int    clutDataSize;  // size in bytes
    int    numCLUTs;      // number of CLUT palettes (from psxTex.m_NumCLUTs)
    int    clutEntries;   // entries per CLUT (16 for 4bpp, 256 for 8bpp)
    int    bpp;           // bit depth (4 or 8)
};
static TextureCLUTCache g_CLUTCache[256];

// ============================================================================
// LoadEffectTextureSheet — parse one effect-sprite sheet TIM (256-wide PSX
// 4bpp strip from core00.etm or effspr\*.tim) and install its D3D11 SRV at an
// EXPLICIT texture-page slot.
//
// The effect sheets use slots 3-10 (weapon FX) and 11-14 (room esp), which no
// other subsystem touches: the global textures own 0-2, the menu/item images
// own 15-30 and 43-46. LoadTexturePage cannot be used - it adds 0xF to the
// slot and would land on the menu's textures - so the SRV is built directly,
// mirroring the conversion in LoadTexturePage's tail.
// ============================================================================
void LoadEffectTextureSheet(int slot, void* timData)
{
    if (slot < 0 || slot >= 256) return;
    if (timData == NULL) return;

    PSXTexture psxTex;
    if (psxTex.Store((int*)timData, 1) == 0) return;

    int w = psxTex.m_WidthPixels;
    int h = psxTex.m_Height;
    int bpp = psxTex.m_BitDepth;
    if (w <= 0 || h <= 0 || psxTex.m_pPixelData == NULL) return;

    DWORD* rgba = new DWORD[w * h];
    if (bpp == 4 || bpp == 8) {
        int numClutEntries = (bpp == 4) ? 16 : 256;
        WORD* clut = psxTex.m_pCLUTData;
        DWORD* clutRGBA = new DWORD[numClutEntries];
        for (int c = 0; c < numClutEntries; c++) {
            WORD clr = clut[c];
            DWORD r = ((clr >> 0)  & 0x1F) * 255 / 31;
            DWORD g = ((clr >> 5)  & 0x1F) * 255 / 31;
            // STP (bit 15) is NOT a per-texel alpha - the PC build has no such
            // thing. Translucency is per primitive; see the long note in the
            // CLUT loop of LoadTexturePage.
            //
            // The key is the COLOUR, and it is 0x0000 EXACTLY - not the index,
            // and not "black with the STP bit masked off". This is the PS1
            // GPU's own rule for an opaque textured primitive: a texel of
            // 0x0000 is not drawn, and every other value is, STP-black 0x8000
            // included, which is how the art asks for a genuinely black texel.
            //
            // Both halves of that matter, and each one was a visible bug:
            //
            //  - Keying on the INDEX (`c == 0`) is right for every asset the PC
            //    release ships - all 872 objspr mask pages have exactly one
            //    black entry and it is index 0 - but not for the Director's
            //    Cut's arrange rooms, whose mask TIMs live inside the RDT and
            //    put the cut-out on a different index: 0x88 in ROOM80B0 camera
            //    1, 61.6% of the page, with index 0 unused. That field drew as
            //    opaque black rectangles across the stairway. 256 of the 289
            //    arrange mask cameras carry such an index.
            //
            //  - Keying on `(clr & 0x7FFF) == 0` then cut out the STP-black
            //    entries too - another 4.3% of that same page, and 11.1% of
            //    camera 3 - which is the banister's dark wood. The rail and
            //    spindles turned into holes and stopped occluding the player.
            //
            // The PC's own CMarniDirect3D::CreateTextureHandle (0x0044c900)
            // does mask the top bit off, because it tests the already-converted
            // 24-bit colour. That is indistinguishable on PC art (no shipped
            // page uses 0x8000) and wrong on PS1 art, so the GPU rule is the
            // one to keep. Verified: against the original index-0 rule this
            // changes not one texel in any of the 954 PC pages and sheets.
            DWORD a = (clr == 0x0000) ? 0x00 : 0xFF;
            DWORD b = ((clr >> 10) & 0x1F) * 255 / 31;
            clutRGBA[c] = (a << 24) | (b << 16) | (g << 8) | r;
        }
        if (bpp == 4) {
            BYTE* src = (BYTE*)psxTex.m_pPixelData;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int byteIdx = y * (w / 2) + x / 2;
                    BYTE nibble = (x & 1) ? (src[byteIdx] >> 4) : (src[byteIdx] & 0xF);
                    rgba[y * w + x] = clutRGBA[nibble];
                }
            }
        } else {
            BYTE* src = (BYTE*)psxTex.m_pPixelData;
            for (int i = 0; i < w * h; i++) {
                rgba[i] = clutRGBA[src[i]];
            }
        }
        delete[] clutRGBA;
    } else {
        for (int i = 0; i < w * h; i++) rgba[i] = 0xFF000000;
    }

    if (g_TexturePageSRV[slot] != MARNI_NULL_HANDLE) {
        Marni_DX()->DestroyTexture(g_TexturePageSRV[slot]);
        g_TexturePageSRV[slot] = MARNI_NULL_HANDLE;
    }
    MarniCreateTexture(w, h, 32, rgba, &g_TexturePageSRV[slot]);
    delete[] rgba;

    g_TexturePageWidth[slot] = w;
    g_TexturePageHeight[slot] = h;
    g_TexturePageBpp[slot] = bpp;
}

// ============================================================================
// LoadEffectTextureSheetVariants — bake one SRV per CLUT row of an effect
// sheet TIM (port-only companion to LoadEffectTextureSheet).
//
// The weapon-FX sheets in core00.etm carry MULTIPLE 16-entry CLUT rows, and
// the rows are palette VARIANTS of the same art, selected per spawn by the
// tint index (clutY = depthGroup >> 3). The blood sheet (esp index 0,
// etm offset 0x8200) is the proof: row 0 dark red (zombie), row 1 green
// (hunter), row 2 orange, row 3 white/lavender - and Plant 42's damage
// splashes spawn with depthGroup 0x18/0x1B/0x1C (Plant42.cpp), i.e. tint 3,
// which is why its blood is white in the original game while a zombie's is
// red. LoadEffectTextureSheet bakes only row 0, so every tint drew red.
//
// Row r of the sheet lands in SRV baseSlot + r. Returns the number of rows
// baked (0 if the TIM has no CLUT or is not 4bpp).
// ============================================================================
int LoadEffectTextureSheetVariants(int baseSlot, void* timData, int maxRows)
{
    if (baseSlot < 0 || baseSlot + 4 > 256 || maxRows <= 0) return 0;
    if (timData == NULL) return 0;

    DWORD* tim = (DWORD*)timData;
    if (tim[0] != 0x10) return 0;              // TIM magic
    DWORD flags = tim[1];
    if ((flags & 0x8) == 0) return 0;          // no CLUT block
    if ((flags & 0x3) != 0) return 0;          // effect sheets are 4bpp

    BYTE* b = (BYTE*)timData;
    // CLUT block: +8 size(4), +12 origin x(2), +14 origin y(2), +16 w(2),
    // +18 h(2), +20 entries. (The first draft read w/h at +24/+26 - those are
    // CLUT data bytes - so clutW read as 0 and every bake bailed out.)
    WORD clutW = *(WORD*)(b + 16);
    WORD clutH = *(WORD*)(b + 18);
    if (clutW != 16 || clutH == 0) return 0;
    WORD* clut = (WORD*)(b + 20);

    // Image block follows the CLUT data: size, origin(2), w(words), h.
    BYTE* img = b + 20 + clutW * clutH * 2;
    WORD imgW = *(WORD*)(img + 8);
    WORD imgH = *(WORD*)(img + 10);
    int w = imgW * 4;                          // 4bpp: 2 px per byte, 4 px per word
    int h = imgH;
    if (w <= 0 || h <= 0) return 0;
    BYTE* pix = img + 12;

    int rows = clutH < maxRows ? clutH : maxRows;
    for (int row = 0; row < rows; row++) {
        DWORD* clutRGBA = new DWORD[16];
        for (int c = 0; c < 16; c++) {
            WORD clr = clut[row * 16 + c];
            DWORD r = ((clr >> 0) & 0x1F) * 255 / 31;
            DWORD g = ((clr >> 5) & 0x1F) * 255 / 31;
            DWORD bl = ((clr >> 10) & 0x1F) * 255 / 31;
            // Same colour-key rule as LoadEffectTextureSheet: index 0 is the
            // transparent key, STP is not alpha.
            DWORD a = (c == 0) ? 0x00 : 0xFF;
            clutRGBA[c] = (a << 24) | (bl << 16) | (g << 8) | r;
        }
        DWORD* rgba = new DWORD[w * h];
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                BYTE nibblePair = pix[y * (w / 2) + x / 2];
                int idx = (x & 1) ? (nibblePair >> 4) : (nibblePair & 0xF);
                rgba[y * w + x] = clutRGBA[idx];
            }
        }
        delete[] clutRGBA;

        int slot = baseSlot + row;
        if (g_TexturePageSRV[slot] != MARNI_NULL_HANDLE) {
            Marni_DX()->DestroyTexture(g_TexturePageSRV[slot]);
            g_TexturePageSRV[slot] = MARNI_NULL_HANDLE;
        }
        MarniCreateTexture(w, h, 32, rgba, &g_TexturePageSRV[slot]);
        delete[] rgba;
        g_TexturePageWidth[slot] = w;
        g_TexturePageHeight[slot] = h;
        g_TexturePageBpp[slot] = 4;
    }
    return rows;
}

// ============================================================================
// RebuildTextureSRV — Rebuild the D3D11 SRV for a slot using a different CLUT
// palette index. Uses cached pixel + CLUT data (no re-parsing).
// Returns 1 on success, 0 on failure.
// ============================================================================
int RebuildTextureSRV(int slotIndex, int clutIndex)
{
    if (slotIndex < 0 || slotIndex >= 256) return 0;
    TextureCLUTCache* cache = &g_CLUTCache[slotIndex];
    if (cache->pixelData == NULL || cache->numCLUTs <= 1) return 0;
    if (clutIndex < 0 || clutIndex >= cache->numCLUTs) return 0;

    int w = g_TexturePageWidth[slotIndex];
    int h = g_TexturePageHeight[slotIndex];
    if (w <= 0 || h <= 0) return 0;

    int bpp = cache->bpp;
    int entriesPerCLUT = cache->clutEntries;

    // Build RGBA palette from the selected CLUT
    WORD* selectedCLUT = cache->clutData + clutIndex * entriesPerCLUT;
    DWORD* clutRGBA = new DWORD[entriesPerCLUT];
    for (int c = 0; c < entriesPerCLUT; c++) {
        WORD clr = selectedCLUT[c];
        DWORD a = (c == 0) ? 0x00 : 0xFF;
        DWORD r = ((clr >> 0)  & 0x1F) * 255 / 31;
        DWORD g = ((clr >> 5)  & 0x1F) * 255 / 31;
        DWORD b = ((clr >> 10) & 0x1F) * 255 / 31;
        clutRGBA[c] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    DWORD* rgba = new DWORD[w * h];
    if (bpp == 4) {
        BYTE* src = cache->pixelData;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int byteIdx = y * (w / 2) + x / 2;
                BYTE nibble = (x & 1) ? (src[byteIdx] >> 4) : (src[byteIdx] & 0xF);
                rgba[y * w + x] = clutRGBA[nibble];
            }
        }
    } else {
        BYTE* src = cache->pixelData;
        for (int i = 0; i < w * h; i++) {
            rgba[i] = clutRGBA[src[i]];
        }
    }

    // Create new SRV first, then swap (avoids NULL SRV during render pass)
    MarniHandle newTex = MARNI_NULL_HANDLE;
    MarniCreateTexture(w, h, 32, rgba, &newTex);

    if (newTex != MARNI_NULL_HANDLE) {
        if (g_TexturePageSRV[slotIndex] != MARNI_NULL_HANDLE) {
            Marni_DX()->DestroyTexture(g_TexturePageSRV[slotIndex]);
        }
        g_TexturePageSRV[slotIndex] = newTex;
    }

    delete[] rgba;
    delete[] clutRGBA;

    return (newTex != MARNI_NULL_HANDLE) ? 1 : 0;
}

int GetTextureNumCLUTs(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 256) return 0;
    return g_CLUTCache[slotIndex].numCLUTs;
}

// 0x0046c130 - async texture page creation worker
// Calls CMarniDirect3D vtable[6] = CreateTextureHandle
void AsyncCreateTexturePage(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[6] != NULL) {
        g_texturePageHandle = ((int(*)(void*, void*, int, void*))pD3D->vtable[6])(
            pD3D, &g_MarniBitsWorkBuffer, g_texturePageMode, &g_MarniBitsOutput);
    }
}

// destroy_texture_page_callback - async texture page deletion worker
// Calls CMarniDirect3D vtable[8] = DeleteTextureHandle
void AsyncDestroyTexturePage(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[8] != NULL) {
        g_AsyncResult = ((int(*)(void*, int))pD3D->vtable[8])(pD3D, g_texturePageHandle);
    }
}

// 0x0046c210 - async object creation worker
// Calls CMarniDirect3D vtable[7] = CreateObjectHandle
void AsyncCreateObject(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[7] != NULL) {
        g_ExecuteBufferHandle = ((unsigned int(*)(void*, void*, unsigned char))pD3D->vtable[7])(
            pD3D, &g_ObjectWorkBuffer, 0);
    }
}

// 0x0046c260 - async object deletion worker
// Calls CMarniDirect3D vtable[9] = DeleteObjectHandle
void AsyncDeleteObject(void)
{
    CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
    if (pD3D != NULL && pD3D->vtable != NULL && pD3D->vtable[9] != NULL) {
        g_AsyncResult = ((int(*)(void*, int))pD3D->vtable[9])(pD3D, g_ExecuteBufferHandle);
    }
}

static void VideoDriver_ClearArrayD0(void) { /* stub */ }

// 0x0046c1b0 - destroy_texture_page
// Sets handle, queues async deletion callback, returns result
void destroy_texture_page(int id)
{
    g_texturePageHandle = id;
    ExecAsync((void*)AsyncDestroyTexturePage);
}

// 0x0046cfd0 - cleanup_texture_slot
// Frees a texture slot's page set. The original adds 0xF to the caller's slot
// (pages live under slot+0xF), zeroes the 9-word texture descriptor at
// g_VideoDriverArray_838 + slot*0x37C, then — gated by the flag at
// g_VideoDriverArray_814 + off and bounded by the count at
// g_VideoDriverArray_810 + off — destroys every live page handle in the
// slot's run of g_TexturePageTable_DAT. Finally it releases the slot's stored
// PSXTexture copy (thiscall VideoDriver_ClearArrayD0 on &DAT_008ed4d0 + off);
// the port keeps no such shadow copy (see create_texture_page), so that
// release is a no-op here, same as in ProcessTextureImage.
void cleanup_texture_slot(int slot)
{
    int    p   = slot + 0xF;
    size_t off = (size_t)p * 0x37C;

    if (off + 9 * sizeof(WORD) <= sizeof(g_VideoDriverArray_838)) {
        WORD* desc = (WORD*)((BYTE*)&g_VideoDriverArray_838 + off);
        for (int i = 0; i < 9; i++) {
            desc[i] = 0;
        }
    }

    if (off + sizeof(DWORD) > sizeof(g_VideoDriverArray_814) ||
        off + sizeof(DWORD) > sizeof(g_VideoDriverArray_810) ||
        off + sizeof(DWORD) > sizeof(g_TexturePageTable_DAT)) {
        return;
    }

    DWORD* gate  = (DWORD*)((BYTE*)&g_VideoDriverArray_814 + off);
    DWORD  count = *(DWORD*)((BYTE*)&g_VideoDriverArray_810 + off);
    DWORD* pages = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + off);

    if (*gate == 0 || count == 0) {
        return;
    }

    size_t room = (sizeof(g_TexturePageTable_DAT) - off) / sizeof(DWORD);
    if ((size_t)count > room) count = (DWORD)room;
    for (DWORD i = 0; i < count; i++) {
        if (pages[i] != 0) {
            destroy_texture_page((int)pages[i]);
            pages[i] = 0;
        }
    }
}

// 0x0046c160 - create_texture_page
// Copies PSXTexture data into work buffer, stores flags, queues async creation
// Persistent snapshot of the last submitted surface (see the copy note below).
static BYTE*  s_pagePixelCopy = NULL;
static size_t s_pagePixelCopyCap = 0;
static WORD*  s_pagePaletteCopy = NULL;
static size_t s_pagePaletteCopyCap = 0;

int create_texture_page(void* psxTexData, int flags)
{
    // The original always passes a slot's stored PSXTexture work buffer
    // (&DAT_008ed4d0 + slot*0x37c). The port keeps no such copy - the
    // D3D11 SRVs persist instead - and TexturePage_Create/Refresh call here
    // with NULL. CopyFrom would dereference NULL+0x40, so treat it as a
    // no-op that leaves the existing page untouched.
    if (psxTexData == NULL) return 0;

    // Diagnostic only: VTable_CreateTextureHandle runs on the scheduler task, so
    // its "[TEXPAGE] BAD CLUT" report has no way back to the call site. Stash
    // the source descriptor so the log names the page that went wrong.
    g_texturePageSrcDesc = psxTexData;

    CMarniBits_CopyFrom(&g_MarniBitsWorkBuffer, psxTexData);

    // CopyFrom ALIASES the caller's pixel/palette pointers, and the async
    // worker only reads them later, on the scheduler task. The original gets
    // away with that because every caller hands it the slot's persistent
    // descriptor; the port keeps no per-slot copy, and ProcessTextureImage
    // passes a local PSXTexture whose pixels die with the frame - ASan caught
    // the worker reading 8KB of freed heap. Snapshot the surface instead so
    // the work buffer owns what the worker reads.
    {
        CMarniBits* src = (CMarniBits*)psxTexData;
        if (src->m_pPixelData != NULL && src->m_height > 0) {
            size_t pitch = (size_t)src->m_pitch;
            if (pitch == 0) {
                pitch = (src->m_bitDepth == 4) ? (size_t)(src->m_width / 2)
                      : (src->m_bitDepth == 8) ? (size_t)src->m_width
                                               : (size_t)src->m_width * 2;
            }
            size_t need = pitch * (size_t)src->m_height;
            if (need > 0) {
                if (need > s_pagePixelCopyCap) {
                    free(s_pagePixelCopy);
                    s_pagePixelCopy = (BYTE*)malloc(need);
                    s_pagePixelCopyCap = (s_pagePixelCopy != NULL) ? need : 0;
                }
                if (s_pagePixelCopy != NULL) {
                    memcpy(s_pagePixelCopy, src->m_pPixelData, need);
                    g_MarniBitsWorkBuffer.m_pPixelData = s_pagePixelCopy;
                }
            }
        }
        if (src->m_pPalette != NULL && (src->m_bitDepth == 4 || src->m_bitDepth == 8)) {
            size_t need = ((src->m_bitDepth == 4) ? 16 : 256) * sizeof(WORD);
            if (need > s_pagePaletteCopyCap) {
                free(s_pagePaletteCopy);
                s_pagePaletteCopy = (WORD*)malloc(need);
                s_pagePaletteCopyCap = (s_pagePaletteCopy != NULL) ? need : 0;
            }
            if (s_pagePaletteCopy != NULL) {
                memcpy(s_pagePaletteCopy, src->m_pPalette, need);
                g_MarniBitsWorkBuffer.m_pPalette = s_pagePaletteCopy;
            }
        }
    }

    g_texturePageMode = flags;
    ExecAsync((void*)AsyncCreateTexturePage);
    return g_texturePageHandle;
}

// ============================================================================
// ProcessTextureImage (0x0046c5f0)
// Auto-positioned font texture loader. Computes X/Y from bank ID.
// ============================================================================
void ProcessTextureImage(void* imageBuffer, short textureBankID, short pageOffset, int slotIndex)
{
    int slotOffset = slotIndex * 0x37C;

    // Same legacy-descriptor bounds problem as LoadTexturePage, and this one
    // indexes ALL the tables by slotOffset (slot * 0x37C = 892 bytes per slot),
    // so it leaves the 1024-byte tables after slot 1: the boot font load
    // (slotIndex 2 -> offset 1784) already wrote past three of them.
    const bool descOk   = (size_t)slotOffset + 0x24 <= sizeof(g_VideoDriverArray_838);
    const bool tableOk  = (size_t)slotOffset + sizeof(DWORD) <= sizeof(g_TexturePageTable_DAT);
    const bool cnt814Ok = (size_t)slotOffset + sizeof(int) <= sizeof(g_VideoDriverArray_814);
    const bool cnt810Ok = (size_t)slotOffset + sizeof(int) <= sizeof(g_VideoDriverArray_810);

    int textureCount = cnt814Ok
                         ? *(int*)((BYTE*)&g_VideoDriverArray_814 + slotOffset)
                         : 0;
    if (textureCount != 0 && tableOk) {
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + slotOffset);
        size_t room = (sizeof(g_TexturePageTable_DAT) - (size_t)slotOffset) / sizeof(DWORD);
        if ((size_t)textureCount > room) textureCount = (int)room;
        for (int i = 0; i < textureCount; i++) {
            if (pageTable[i] != 0) { destroy_texture_page(pageTable[i]); pageTable[i] = 0; }
        }
        VideoDriver_ClearArrayD0();
    }

    if (imageBuffer == NULL) {
        OutputDebugStringA("[TEX] ProcessTextureImage: imageBuffer is NULL!\n");
        return;
    }

    PSXTexture psxTex;
    if (psxTex.Store((int*)imageBuffer, 1) == 0) return;

    short baseX = (textureBankID >= 0x10) ? 0x400 : 0;
    short baseY = (textureBankID >= 0x10) ? 0x100 : 0;

    if (descOk) {
        WORD* texDesc = (WORD*)((BYTE*)&g_VideoDriverArray_838 + slotOffset);
        texDesc[0] = textureBankID * 0x40 - baseX;
        texDesc[1] = baseY;
        *(short*)(texDesc + 2) = (short)psxTex.m_WidthPixels;
        *(short*)(texDesc + 3) = (short)psxTex.m_Height;
        texDesc[4] = 0;
        texDesc[5] = pageOffset + 0x1E0;
        texDesc[6] = 0;
        texDesc[7] = 1;
        texDesc[8] = textureBankID;

        if (psxTex.m_BitDepth == 4)
            texDesc[2] = (short)(((int)texDesc[2] + ((int)texDesc[2] >> 31 & 3)) >> 2);
        else if (psxTex.m_BitDepth == 8)
            texDesc[2] = texDesc[2] / 2;
    }

    if (tableOk) {
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + slotOffset);
        pageTable[0] = create_texture_page(&psxTex, 2);
    } else {
        create_texture_page(&psxTex, 2);
    }

    if (cnt810Ok) *(DWORD*)((BYTE*)&g_VideoDriverArray_810 + slotOffset) = 1;
    if (cnt814Ok) *(DWORD*)((BYTE*)&g_VideoDriverArray_814 + slotOffset) = 1;

    // --- Create D3D11 font texture for text rendering ---
    if (textureBankID == 0x1E && psxTex.m_pPixelData != NULL) {
        CMarniDirect3D* pD3D = (CMarniDirect3D*)g_pMarniDirect3D;
        OutputDebugStringA("[TEX] Creating font D3D11 texture...\n");
        if (pD3D != NULL) {
            OutputDebugStringA("[TEX] pD3D valid\n");

            int w = psxTex.m_WidthPixels;
            int h = psxTex.m_Height;
            int bpp = psxTex.m_BitDepth;
            void* srcData = psxTex.m_pPixelData;

            // For paletted formats (4bpp/8bpp), pre-convert to RGBA using CLUT
            DWORD* clutRGBA = NULL;
            DWORD* rgbaOut = NULL;
            int useDirect = 0;

            if (bpp == 4 || bpp == 8) {
                // Build CLUT RGBA palette. The palette dimensions are locals in
                // PSXTexture::Store and are not kept in the object, so derive
                // the entry count: colours per row (by bit depth) x CLUT rows.
                int rows = (int)psxTex.m_NumCLUTs;
                if (rows < 1) rows = 1;
                int numClutEntries = ((bpp == 4) ? 16 : 256) * rows;

                if (numClutEntries > 256) numClutEntries = 256;
                if (numClutEntries < 16)  numClutEntries = 16;

                clutRGBA = new DWORD[numClutEntries];
                WORD* clut = psxTex.m_pCLUTData;   // heap CLUT copy (PSXTexture::Store)

                for (int i = 0; i < numClutEntries; i++) {
                    WORD c = clut[i];
                    if (i == 0) {
                        // Index 0 = transparent
                        clutRGBA[i] = 0x00000000;
                    } else {
                        // PS1 15-bit colour: bits 4-0 red, 9-5 green, 14-10 blue
                        // (bit 15 = STP mask). Was reading red and blue swapped.
                        DWORD a = (c & 0x8000) ? 0xFF : 0xFF; // Always opaque for non-zero indices
                        DWORD r = ((c >> 0)  & 0x1F) * 255 / 31;
                        DWORD g = ((c >> 5)  & 0x1F) * 255 / 31;
                        DWORD b = ((c >> 10) & 0x1F) * 255 / 31;
                        // R8G8B8A8_UNORM wants R in the lowest byte
                        clutRGBA[i] = (a << 24) | (b << 16) | (g << 8) | r;
                    }
                }

                // Convert pixel indices to RGBA using CLUT
                int totalPixels = w * h;
                rgbaOut = new DWORD[totalPixels];

                if (bpp == 4) {
                    WORD* src = (WORD*)srcData;
                    int totalWords = totalPixels / 4;
                    for (int i = 0; i < totalWords; i++) {
                        WORD word = src[i];
                        int p0 = (word >> 0)  & 0xF;
                        int p1 = (word >> 4)  & 0xF;
                        int p2 = (word >> 8)  & 0xF;
                        int p3 = (word >> 12) & 0xF;
                        int base = i * 4;
                        rgbaOut[base]     = clutRGBA[p0];
                        rgbaOut[base + 1] = clutRGBA[p1];
                        rgbaOut[base + 2] = clutRGBA[p2];
                        rgbaOut[base + 3] = clutRGBA[p3];
                    }
                } else { // bpp == 8
                    BYTE* src = (BYTE*)srcData;
                    for (int i = 0; i < totalPixels; i++) {
                        rgbaOut[i] = clutRGBA[src[i]];
                    }
                }

                // Use pre-converted RGBA data
                useDirect = 1;
            }

            if (useDirect) {
                MarniCreateTexture(w, h, 32, rgbaOut, &pD3D->m_FontTexHandle);
                delete[] rgbaOut;
                delete[] clutRGBA;
            } else {
                MarniCreateTexture(w, h, bpp, srcData, &pD3D->m_FontTexHandle);
            }

            if (pD3D->m_FontTexHandle != MARNI_NULL_HANDLE) {
                OutputDebugStringA("[TEX] Font SRV created OK\n");
                pD3D->m_FontTexWidth = w;
                pD3D->m_FontTexHeight = h;
            } else OutputDebugStringA("[TEX] Font SRV creation FAILED\n");
        } else {
            OutputDebugStringA("[TEX] pD3D NULL - cannot create font texture\n");
        }
    }
}

// ============================================================================
// LoadTexturePage (0x0046c870)
// General-purpose texture loader with explicit positioning and flag control.
// Slot shifted by +0xF; uses alternate 0xDF scaling for descriptor table lookups.
// ============================================================================
void LoadTexturePage(void* imageBuffer, short texId, short pageOffset, int slotIndex,
                     int unused, short posX, short posY, unsigned int flags)
{
    slotIndex = slotIndex + 0xF;
    int slotOffset = slotIndex * 0x37C;
    int texCheckOffset = slotIndex * 0xDF;

    // ---- Legacy Marni page descriptors (vestigial in this port) ----------
    // These tables are BYTE-indexed by the shifted slot (slot * 0xDF and
    // slot * 0x37C). The original's span ~393KB (0x008ed4d0..0x008f7890); the
    // port's stand-ins are 0.5-8KB, so from slot 4 upward EVERY access lands
    // outside its own array. The DX11 render path takes page state exclusively
    // from the g_TexturePage* arrays at the end of this function, so reads that
    // fall out of range can safely yield 0 - but the WRITES were corrupting
    // whatever the linker placed after these arrays. Only the _838 write was
    // guarded; the table and _4d0 writes were not.
    //
    // Opening the map tab is slot 0xC -> shifted 27 -> texCheckOffset 6021 into
    // a 1024-byte table and slotOffset 24084 into a 4096-byte one. That wiped
    // the page metadata behind the character portrait (slot 9) and the blue
    // inventory background (slot 10) - both drew black, because display_texture
    // bails when a page's width/height are unknown - and left the equipped
    // weapon box sampling a stale page as diagonal stripes.
    //
    // g_TexturePageTable_DAT is doubly dangerous: SpriteRenderer indexes it by
    // ELEMENT (0-255) while this function indexes it by BYTE offset, so an
    // out-of-range write here also lands in live entries. See the
    // byte-offset-indexed-globals trap.
    const bool descOk   = (size_t)slotOffset + 0x24 <= sizeof(g_VideoDriverArray_838);
    const bool tableOk  = (size_t)texCheckOffset + 8 * sizeof(DWORD) <= sizeof(g_TexturePageTable_DAT);
    const bool cnt814Ok = (size_t)texCheckOffset + sizeof(int) <= sizeof(g_VideoDriverArray_814);
    const bool cnt810Ok = (size_t)texCheckOffset + sizeof(int) <= sizeof(g_VideoDriverArray_810);

    int textureCount = cnt814Ok
                         ? *(int*)((BYTE*)&g_VideoDriverArray_814 + texCheckOffset)
                         : 0;
    if (textureCount != 0 && tableOk) {
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);
        for (int i = 0; i < textureCount; i++) {
            if (pageTable[i] != 0) { destroy_texture_page(pageTable[i]); pageTable[i] = 0; }
        }
        VideoDriver_ClearArrayD0();
    }

    if (imageBuffer == NULL) {
        OutputDebugStringA("[TEX] LoadTexturePage: imageBuffer is NULL!\n");
        return;
    }

    PSXTexture psxTex;
    int storeResult = psxTex.Store((int*)imageBuffer, 1);
    if (storeResult == 0) return;

    // Same rule as above: only write the descriptor when the slot's offset is
    // actually inside the port's fragment. The low slots (0-3, used by
    // LoadImage/TitleScreen) keep the original behaviour.
    if (descOk) {
        WORD* texDesc = (WORD*)((BYTE*)&g_VideoDriverArray_838 + slotOffset);
        texDesc[0] = posX;
        texDesc[1] = posY;
        *(short*)(texDesc + 2) = (short)psxTex.m_WidthPixels;
        *(short*)(texDesc + 3) = (short)psxTex.m_Height;
        texDesc[4] = 0;
        texDesc[5] = pageOffset + 0x1E0;
        texDesc[6] = 0;
        texDesc[7] = (short)textureCount;
        texDesc[8] = texId;

        BYTE bpp = *(BYTE*)((BYTE*)&g_VideoDriverArray_FA + texCheckOffset);
        if (bpp == 4)
            texDesc[2] = (short)(((int)texDesc[2] + ((int)texDesc[2] >> 31 & 3)) >> 2);
        else if (bpp == 8)
            texDesc[2] = texDesc[2] / 2;
    }

    int pageIndex = 0;
    // An out-of-range read here used to hand the loop a garbage page count, so
    // it wrote thousands of DWORDs past both arrays.
    int pageCount = cnt810Ok
                      ? *(int*)((BYTE*)&g_VideoDriverArray_810 + texCheckOffset)
                      : 0;
    if (pageCount > 0 && tableOk) {
        BYTE* pageData = (BYTE*)&g_VideoDriverArray_4d0 + slotOffset;
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);
        for (int i = 0; i < pageCount; i++) {
            if ((size_t)slotOffset + (size_t)i * 0x68 + 0x54 > sizeof(g_VideoDriverArray_4d0)) {
                break;
            }
            if ((flags & 1) == 0) {
                *(DWORD*)(pageData + 0x50) = 1;
                int mode = ((flags & 2) == 0) ? 2 : 0x12;
                *pageTable = (DWORD)create_texture_page(pageData, mode);
            } else {
                *pageTable = 0;
            }
            pageData += 0x68;
            pageTable++;
        }
        pageIndex = pageCount;
    }

    if (pageIndex < 8 && tableOk) {
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset + pageIndex * sizeof(DWORD));
        for (int i = 8 - pageIndex; i > 0; i--) { *pageTable = 0; pageTable++; }
    }

    // 0x0046c870: Create D3D11 SRV from the loaded PSXTexture
    // Store SRV indexed by shifted slot index (slotIndex = original + 0xF)
    if (psxTex.m_pPixelData != NULL && psxTex.m_WidthPixels > 0 && psxTex.m_Height > 0) {
        int w = psxTex.m_WidthPixels;
        int h = psxTex.m_Height;
        int bpp = psxTex.m_BitDepth;

        if (bpp == 4 || bpp == 8) {
            int numClutEntries = (bpp == 4) ? 16 : 256;
            WORD* clut = psxTex.m_pCLUTData;   // heap CLUT copy (PSXTexture::Store)
            DWORD* clutRGBA = new DWORD[numClutEntries];
            // PS1 15-bit colour is MBBBBBGGGGGRRRRR: bit 15 = STP mask,
            // bits 14-10 = blue, bits 9-5 = green, bits 4-0 = RED. This site
            // used to read red from bits 10-14 and blue from bits 0-4, which
            // swapped the two channels for every CLUT-based model texture -
            // reddish surfaces came out blue. Every other conversion in the
            // tree already uses this layout.
            //
            // ALPHA IS A PURE COLOUR KEY ON BLACK. Bit 15 (STP) must NOT become
            // a per-pixel alpha here. The PC build has no per-texel
            // semi-transparency at all: CMarniDirect3D::CreateTextureHandle
            // (0x0044c900) only ever tests `(colour & 0xffffff) == 0` when it
            // builds a page's transparency - it never looks at bit 15 - and
            // CMarniBits::BltFast's colorkey flag keys on the same test. PS1
            // semi-transparency arrives PER PRIMITIVE instead, as the variant
            // level the producers write to TextureDraw+0x2c (see
            // TextureDraw::variantAlpha in SpriteRenderer.h).
            //
            // Baking STP in as alpha 0x80 applied it unconditionally, whatever
            // blend the primitive asked for. umb00.tim / umb01.tim - the lab
            // terminal's UI sheets - carry STP on 255 of their 256 CLUT
            // entries, so the whole terminal (login window, keyboard, typed
            // text) rendered at 50% even though cl_draw_windows and cl_draw_text
            // draw a settled window with the variant bits OFF, i.e. opaque.
            // The map screen keeps its 50% grid because g_MapZoomDesc[1]/[2]
            // really do set 0x40000000; it never needed the per-texel rule.
            //
            // Which texel is the key is unchanged from before, and is still
            // decided per page: nearly every RE1 page puts the key at index 0,
            // but a page whose index 0 is a REAL colour is not using the index
            // as a key at all - the filem_*.pix document pages are exactly that
            // ([0] = 0xffff, the bright text; [7] = 0x0000, the background), and
            // keying on the index punched holes through the brightest text.
            // Both 0x0000 and 0x8000 count as black: the top bit is masked out
            // of the comparison, so STP-black is cut out too.
            bool indexZeroIsKey = (clut[0] == 0x0000 || clut[0] == 0x8000);
            for (int c = 0; c < numClutEntries; c++) {
                WORD clr = clut[c];
                DWORD r = ((clr >> 0)  & 0x1F) * 255 / 31;
                DWORD g = ((clr >> 5)  & 0x1F) * 255 / 31;
                DWORD a = indexZeroIsKey
                            ? ((c == 0) ? 0x00 : 0xFF)
                            : (((clr & 0x7FFF) == 0) ? 0x00 : 0xFF);
                DWORD b = ((clr >> 10) & 0x1F) * 255 / 31;
                // R8G8B8A8_UNORM wants R in the lowest byte
                clutRGBA[c] = (a << 24) | (b << 16) | (g << 8) | r;
            }

            DWORD* rgba = new DWORD[w * h];
            int opaqueCount = 0;
            if (bpp == 4) {
                BYTE* src = (BYTE*)psxTex.m_pPixelData;
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        int byteIdx = y * (w / 2) + x / 2;
                        BYTE nibble = (x & 1) ? (src[byteIdx] >> 4) : (src[byteIdx] & 0xF);
                        rgba[y * w + x] = clutRGBA[nibble];
                        if (nibble != 0) opaqueCount++;
                    }
                }
            } else {
                BYTE* src = (BYTE*)psxTex.m_pPixelData;
                for (int i = 0; i < w * h; i++) {
                    rgba[i] = clutRGBA[src[i]];
                    if (src[i] != 0) opaqueCount++;
                }
            }

            if (slotIndex >= 0 && slotIndex < 256) {
                if (g_TexturePageSRV[slotIndex] != MARNI_NULL_HANDLE) {
                    Marni_DX()->DestroyTexture(g_TexturePageSRV[slotIndex]);
                    g_TexturePageSRV[slotIndex] = MARNI_NULL_HANDLE;
                }
                MarniCreateTexture(w, h, 32, rgba, &g_TexturePageSRV[slotIndex]);
                g_TexturePageWidth[slotIndex] = w;
                g_TexturePageHeight[slotIndex] = h;
                g_TexturePageBpp[slotIndex] = bpp;
                g_TexturePageOriginX[slotIndex] = posX;
                g_TexturePageOriginY[slotIndex] = posY;
                g_TexturePageId[slotIndex] = texId;
                g_TexturePageClutBase[slotIndex] = pageOffset + 0x1E0;

                // Cache pixel + CLUT data for CLUT palette cycling in texture viewer
                if (psxTex.m_NumCLUTs > 1 && psxTex.m_pPixelData != NULL) {
                    // Free previous cache if any
                    if (g_CLUTCache[slotIndex].pixelData != NULL) {
                        delete[] g_CLUTCache[slotIndex].pixelData;
                    }
                    if (g_CLUTCache[slotIndex].clutData != NULL) {
                        delete[] g_CLUTCache[slotIndex].clutData;
                    }

                    int entriesPerCLUT = (bpp == 4) ? 16 : 256;
                    int totalCLUTEntries = (int)psxTex.m_NumCLUTs * entriesPerCLUT;
                    int pixelBytes = (bpp == 4) ? (w / 2) * h : w * h;

                    // Copy indexed pixel data
                    g_CLUTCache[slotIndex].pixelData = new BYTE[pixelBytes];
                    memcpy(g_CLUTCache[slotIndex].pixelData, psxTex.m_pPixelData, pixelBytes);
                    g_CLUTCache[slotIndex].pixelDataSize = pixelBytes;

                    // Copy all CLUT palettes from the raw TIM buffer
                    // (the heap CLUT copy in PSXTexture only holds the parsed
                    //  copy; reading the file data keeps every palette row)
                    int* hdr = (int*)imageBuffer;
                    WORD* rawCLUT = (WORD*)(hdr + 5);  // CLUT data starts at imageData[5]
                    g_CLUTCache[slotIndex].clutData = new WORD[totalCLUTEntries];
                    memcpy(g_CLUTCache[slotIndex].clutData, rawCLUT, totalCLUTEntries * sizeof(WORD));
                    g_CLUTCache[slotIndex].clutDataSize = totalCLUTEntries * sizeof(WORD);

                    g_CLUTCache[slotIndex].numCLUTs = (int)psxTex.m_NumCLUTs;
                    g_CLUTCache[slotIndex].clutEntries = entriesPerCLUT;
                    g_CLUTCache[slotIndex].bpp = bpp;
                }
            }
            delete[] rgba;
            delete[] clutRGBA;
        } else if (bpp == 16) {
            DWORD* rgba = new DWORD[w * h];
            WORD* src = (WORD*)psxTex.m_pPixelData;
            for (int i = 0; i < w * h; i++) {
                WORD px = src[i];
                // PS1 BGR555 with STP bit (bit 15). This is a TEXTURE PAGE, and
                // the original keyed those on black: CMarniBits::BltFast's
                // colorkey flag (bit 0) skips a source pixel when
                // (color & 0xFFFFFF) == 0 — i.e. black, with the top bit
                // ignored, so both 0x0000 and 0x8000 are cut out. Mirror that
                // exactly:
                //   0x0000 / 0x8000  -> fully transparent (cut-out)
                //   otherwise        -> opaque
                //
                // STP set on a NON-BLACK texel is not a per-texel alpha here:
                // the PC build's page upload never tests bit 15 (0x0044c900),
                // and semi-transparency is a per-primitive level instead - see
                // the note in the CLUT loop above.
                //
                // In the 16bpp pages that reach here the cut-outs are stored as
                // 0x8000 (STP set, colour black): Select_b.tim's round card
                // corners and cursor-arrow surrounds, Optkey03.tim's widget
                // surrounds. None of them contains a 0x0000 texel or an STP
                // texel with a non-zero colour. Black artwork is stored as a
                // near-black colour instead, so keying here does not punch
                // holes in the card's legitimate dark regions — bar a handful
                // of isolated pure-black texels in dithered art (18 in
                // Select_b, 169 in Optkey03's Japanese help text) which the
                // retail PC build dropped out the same way.
                //
                // Do NOT copy this rule into display_image: a background is
                // blitted without the colorkey flag and its black is real
                // artwork. See the note there.
                DWORD a = ((px & 0x7FFF) == 0) ? 0x00 : 0xFF;
                DWORD r = ((px >> 0)  & 0x1F) * 255 / 31;
                DWORD g = ((px >> 5)  & 0x1F) * 255 / 31;
                DWORD b = ((px >> 10) & 0x1F) * 255 / 31;
                rgba[i] = (a << 24) | (b << 16) | (g << 8) | r;
            }
            if (slotIndex >= 0 && slotIndex < 256) {
                if (g_TexturePageSRV[slotIndex] != MARNI_NULL_HANDLE) {
                    Marni_DX()->DestroyTexture(g_TexturePageSRV[slotIndex]);
                    g_TexturePageSRV[slotIndex] = MARNI_NULL_HANDLE;
                }
                MarniCreateTexture(w, h, 32, rgba, &g_TexturePageSRV[slotIndex]);
                g_TexturePageWidth[slotIndex] = w;
                g_TexturePageHeight[slotIndex] = h;
                g_TexturePageBpp[slotIndex] = 16;
                g_TexturePageOriginX[slotIndex] = posX;
                g_TexturePageOriginY[slotIndex] = posY;
                g_TexturePageId[slotIndex] = texId;
                g_TexturePageClutBase[slotIndex] = pageOffset + 0x1E0;
            }
            delete[] rgba;
        }
    }
}

// The single colour every non-transparent CLUT entry is flattened to before
// the page is built (0x0046cd88). RGB555 (15,15,15) - a 48% grey, which is
// exactly how dark the ground shadow can ever make the floor.
#define SHADOW_PAGE_COLOUR  0x3DEF

// ============================================================================
// LoadShadowMaskTexture (0x0046ccd0)
// Shadow/mask texture loader. Reads the 16-bit palette from image offset
// +0x14, flattens every non-transparent colour to SHADOW_PAGE_COLOUR, then
// creates shadow-optimized texture pages (mode=1). Slot shifted by +0x2F.
// ============================================================================
void LoadShadowMaskTexture(void* imageBuffer, int slotBase)
{
    if (imageBuffer == NULL) {
        OutputDebugStringA("[TEX] LoadShadowMaskTexture: imageBuffer is NULL!\n");
        return;
    }

    int slotIndex = slotBase + 0x2F;
    int slotOffset = slotIndex * 0x37C;
    int texCheckOffset = slotIndex * 0xDF;

    // The legacy page tables below are byte-offset indexed by slot (0xDF per
    // slot for the counts/handles, 0x37C for the work buffers), but the
    // port's arrays are only 1-4KB fragments of the original's ~393KB
    // descriptor region. Slot 0x2F's offsets (texCheckOffset 10481,
    // slotOffset 41924) exceed every one of them, so the destruction block
    // would read a count ~9KB out of bounds and call destroy_texture_page on
    // arbitrary DWORDs found there - the mechanism that can kill the boot-time
    // menu textures (status.tim @15, statface.tim @24, blue.tim @25,
    // staitem.tim @45), which nothing ever reloads. The port's D3D11 SRVs
    // live in g_TexturePageSRV, not these tables, so the legacy block is only
    // run when its offsets actually land inside the arrays.
    int legacyInRange = (texCheckOffset >= 0) &&
        (texCheckOffset + 8 * (int)sizeof(DWORD) <= (int)sizeof(g_TexturePageTable_DAT)) &&
        (texCheckOffset + 8 * (int)sizeof(DWORD) <= (int)sizeof(g_VideoDriverArray_814)) &&
        (slotOffset + 8 * 0x68 <= (int)sizeof(g_VideoDriverArray_4d0));

    int textureCount = 0;
    if (legacyInRange) {
        textureCount = *(int*)((BYTE*)&g_VideoDriverArray_814 + texCheckOffset);
    }
    if (textureCount != 0 && legacyInRange) {
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);
        for (int i = 0; i < textureCount; i++) {
            if (pageTable[i] != 0) { destroy_texture_page(pageTable[i]); pageTable[i] = 0; }
        }
        VideoDriver_ClearArrayD0();
    }

    // Read color palette from image offset +0x14
    WORD paletteEntries[256] = {0};
    WORD* srcPalette = (WORD*)((BYTE*)imageBuffer + 0x14);
    for (int i = 0; i < 256; i++) {
        WORD color = srcPalette[i];
        paletteEntries[i] = color;
        // Every non-transparent entry collapses to the single page colour
        // 0x3DEF. That is NOT black: RGB555 0x3DEF is (15,15,15) of 31, a
        // 48% grey - the shade the shadow multiplies the floor down to. See
        // the SRV build below, which needs it.
        if (color != 0 && color != 0x8000) {
            srcPalette[i] = SHADOW_PAGE_COLOUR;
        }
    }

    PSXTexture psxTex;
    if (psxTex.Store((int*)imageBuffer, 1) == 0) return;

    // --- D3D11 SRV at the page slot (0x2F) ---
    // The legacy create_texture_page call below only updates the PSX handle
    // table; nothing built the D3D11 SRV DrawFadeSpr samples, and a NULL SRV
    // makes FlushSpriteCommandsRange skip the sprite (no ground shadows, no
    // death blood pool).
    //
    // The original (0x0046ce90) builds this page in two steps. It first draws
    // the BLACKENED image into a work bitmap - every disc pixel becomes the
    // flat 48% grey 0x3DEF, everything outside stays the 0xFFFFFF the bitmap
    // was filled with - and then overwrites each pixel's ALPHA (declared 8
    // bits at shift 24 by the m_alphaShift/Mask/Width stores) from the SAVED,
    // UNBLACKENED palette:
    //     alpha = ((255 - r*8) * 2) & 0xFF          (LEA ECX*2 / SHL 0x18)
    // Note every value in KAGE wraps that mask, so it is a contrast stretch:
    // alpha = 254 - 16r, running 254 at the transparent border down to 62 at
    // the disc core. It is a COVERAGE value, not the shade: the page colour
    // is what the destination gets multiplied toward, and the alpha says how
    // far. So the multiplier the mode-1 page applies is
    //     F = lerp(1.0, 15/31, coverage),   coverage = (255 - alpha) / 255
    // i.e. the shadow bottoms out at 48% of the background, reaching 61% at
    // the disc core (coverage 0.757).
    //
    // This port draws it as a src-alpha sprite over a near-black texel, where
    // out = src*A + dst*(1-A) ~= dst*(1-A), so A must be 1 - F. Baking the
    // raw coverage instead (A = 255 - alpha, giving out = dst*alpha/255 = 24%
    // at the core) threw away the page colour entirely and made the shadow
    // about 2.4x too opaque.
    //
    // The page's RGB is WHITE on purpose: the kage page is a COVERAGE mask
    // only, and the primitive tint (AddFadePoly's rgb triple) is the sole
    // colour carrier - in the original the mode-1 record takes FUN_00446e40's
    // direct path (record+0x80 bit 1 set), whose vertex diffuse is the tint
    // scaled by 127 and nothing else. Baking the flattened 48%-grey into the
    // texels here multiplied it into that tint a second time: the death blood
    // pool's red (80/256) came out at 4% and read as a washed-out grey patch.
    // The shadow tint collapses to ~black before this multiply either way, so
    // shadows are unaffected.
    {
        int w = psxTex.m_WidthPixels;
        int h = psxTex.m_Height;
        BYTE* src = (BYTE*)psxTex.m_pPixelData;
        if (w > 0 && h > 0 && src != NULL) {
            // 1 - 15/31, as 5-bit levels: how much of the destination the
            // page colour can take away at full coverage.
            const unsigned int darken = 31u - (SHADOW_PAGE_COLOUR & 0x1Fu);
            DWORD* rgba = new DWORD[w * h];
            for (int i = 0; i < w * h; i++) {
                unsigned int r = (unsigned int)paletteEntries[src[i]] & 0x1F;
                unsigned int alpha = ((255u - r * 8u) * 2u) & 0xFFu;
                unsigned int coverage = 255u - alpha;
                rgba[i] = (((coverage * darken) / 31u) << 24) | 0x00FFFFFFu;
            }
            if (g_TexturePageSRV[slotIndex] != MARNI_NULL_HANDLE) {
                Marni_DX()->DestroyTexture(g_TexturePageSRV[slotIndex]);
                g_TexturePageSRV[slotIndex] = MARNI_NULL_HANDLE;
            }
            MarniCreateTexture(w, h, 32, rgba, &g_TexturePageSRV[slotIndex]);
            delete[] rgba;

            g_TexturePageWidth[slotIndex]  = w;
            g_TexturePageHeight[slotIndex] = h;
            g_TexturePageBpp[slotIndex]    = 8;
        }
    }

    // Create shadow-optimized texture pages with mode=1. Same bounds guard as
    // the destruction block above: slot 0x2F's legacy offsets are far past the
    // port's arrays, and these writes would land in unrelated .bss globals.
    int pageIndex = 0;
    int pageCount = 1;
    if (legacyInRange) {
        BYTE* pageData = (BYTE*)&g_VideoDriverArray_4d0 + slotOffset;
        DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);

        for (int page = 0; page < pageCount; page++) {
            *(DWORD*)(pageData + 0x50) = 1;
            *(DWORD*)(pageData + 0x54) = 0;
            *(DWORD*)(pageData + 0x58) = 0;
            *(DWORD*)(pageData + 0x5C) = 0;
            *(DWORD*)(pageData + 0x60) = 0;
            *pageTable = (DWORD)create_texture_page(pageData, 1);  // mode=1 = shadow/alpha blend
            pageData += 0x68;
            pageTable++;
            pageIndex++;
        }

        if (pageIndex < 8) {
            DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset + pageIndex * sizeof(DWORD));
            for (int i = 8 - pageIndex; i > 0; i--) { *pageTable = 0; pageTable++; }
        }

        *(DWORD*)((BYTE*)&g_VideoDriverArray_810 + slotOffset) = pageIndex;
        *(DWORD*)((BYTE*)&g_VideoDriverArray_814 + slotOffset) = 1;
    }
}

// ============================================================================
// CreateTexturedQuad (0x0046fb50, renamed from FUN_0046fb50)
//
// Creates a renderable textured quad primitive in the Marni 3D viewport system.
// Sets up 4 vertices with position/UV coordinates from the provided vertex data,
// forms two triangles, and creates an execute buffer for GPU rendering.
//
// Parameters:
//   viewportSlot   - slot index (scaled internally by 0x40)
//   texturePageId  - texture page handle stored at DAT_008ed06c[slot]
//   vertexData     - array of 24 ints, read COLUMN-MAJOR at stride 4: element [i]
//                    is vertex i's x, [i+4] its y, [i+8] its z, [i+12] its raw u
//                    and [i+16] its raw v. UVs are normalised by the texture page
//                    dimensions at DAT_008ed4fc/DAT_008ed500[texturePageId].
//                    [20],[21],[22] are a single normal shared by all four
//                    vertices; [23] is unread.
//
// Each vertex handed to CMarniViewport2::SetVertex is 11 floats (0x2C bytes):
//   {x, y, z, nx, ny, nz, 1.0f, 1.0f, 1.0f, u, v}
// and the index list is the quad 0, 1, 3, 2.
//
// NOTE: the object handle this produces (g_VideoDriverArray_068[slot], via
// FUN_0046c230) is what AddFadePoly requires to be non-zero; it returns 0 early
// otherwise. While the viewport calls below are stubbed, ground shadows cannot
// draw no matter what the fade-sprite queue contains. See docs/SCD_WORK_PLAN.md.
//
// Sub-functions (Marni viewport API, stubbed until full implementation):
//   FUN_0046c280(handle)              - release old execute buffer
//   FUN_00427270()                    - viewport cleanup
//   FUN_00427100(4, 1, 4)            - init viewport (vertices, mode, type)
//   FUN_004271e0(0, 0)               - get background color
//   FUN_00426df0(id, &vertex)        - register vertex in viewport
//   FUN_00426f70(0, &indices)        - apply lighting/material
//   FUN_00427250()                    - finalize background
//   FUN_0046c230(&DAT_008ed030[slot]) - create execute buffer → returns handle
// ============================================================================
void CreateTexturedQuad(int viewportSlot, int texturePageId, int* vertexData)
{
    // 0x0046fb50: Scale slot to byte offset
    int slotOffset = viewportSlot * 0x40;

    // Same stand-in caveat as the other legacy-descriptor users: the original's
    // per-slot arrays are far larger than these fragments, so every access is
    // range-checked before it happens. An out-of-range read yields "absent".
    const size_t slotElem = (size_t)slotOffset / sizeof(DWORD);
    const bool slotOk = slotElem < sizeof(g_VideoDriverArray_03c) / sizeof(DWORD);
    const bool dataOk = (size_t)slotOffset + 0x40 <= sizeof(g_VideoDriverArray_D0);
    const bool pageOk = (size_t)texturePageId < sizeof(g_VideoDriverArray_4fc) / sizeof(DWORD);
    const bool tableOk = (size_t)texturePageId * 0xDF + sizeof(DWORD)
                         <= sizeof(g_TexturePageTable_DAT);

    // 0x0046fb60: If slot is in use, release old resources
    if (slotOk && g_VideoDriverArray_03c[slotElem] != 0) {
        int oldHandle = g_VideoDriverArray_068[slotElem];
        if (oldHandle != 0) {
            FUN_0046c280(oldHandle);
        }
        // FUN_00427270();  // Viewport cleanup - stubbed
    }

    // 0x0046fb90: Initialize viewport: 4 vertices, mode 1, type 4
    // FUN_00427100(4, 1, 4);  // - stubbed

    // 0x0046fba0: Clear the surface flag
    if (slotOk) g_VideoDriverArray_04c[slotElem] = 0;

    // 0x0046fbb0: Store the texture page ID for this slot
    if (slotOk) g_VideoDriverArray_06c[slotElem] = texturePageId;

    // 0x0046fbc0: Check if the texture page exists in the page table
    int texCheckOffset = texturePageId * 0xDF;
    DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);

    if (tableOk && *pageTable != 0) {
        // 0x0046fbd0: Get and check background color
        // int bgColor = FUN_004271e0(0, 0);  // - stubbed
        int bgColor = 1;  // Assume non-zero for now

        if (bgColor != 0) {
            // 0x0046fbf0: Set up 4 vertices
            for (int i = 0; i < 4; i++) {
                // Each vertex entry in vertexData contains position, UV, and a placeholder
                // vertexData layout:
                //   [0]  = v0.x,  [1]  = v1.x,  [2]  = v2.x,  [3]  = v3.x
                //   [4]  = v0.y,  ...  (interleaved by stride 4 in the reverse direction)
                // Actually, from the decompilation: *piVar4 directly accesses each group
                // piVar4[0]  = x for vertex i
                // piVar4[4]  = y for vertex i
                // piVar4[8]  = z for vertex i
                // piVar4[12] = u for vertex i
                // piVar4[16] = v for vertex i
                // param_3[20] = r, param_3[21] = g, param_3[22] = b

                float x = (float)vertexData[i];           // Position X
                float y = (float)vertexData[i + 4];        // Position Y  
                float z = (float)vertexData[i + 8];        // Position Z
                float r = 1.0f;                             // Color R (0x3f800000 = 1.0f)
                float g = 1.0f;                             // Color G
                float b = 1.0f;                             // Color B

                // Texture coordinates normalized by page dimensions
                float texW = pageOk ? (float)g_VideoDriverArray_4fc[texturePageId] : 0.0f;
                float texH = pageOk ? (float)g_VideoDriverArray_500[texturePageId] : 0.0f;
                float u = (texW != 0) ? (float)vertexData[i + 12] / texW : 0.0f;
                float v = (texH != 0) ? (float)vertexData[i + 16] / texH : 0.0f;

                // Vertex descriptor structure (matches local_2c layout)
                float vertexDesc[10] = { x, y, z, r, g, b, 0.0f, 0.0f, u, v };
                // FUN_00426df0(i, vertexDesc);  // Register vertex - stubbed
            }

            // 0x0046fc80: Triangle indices for quad: 0,1,3,2
            WORD indices[4] = { 0, 1, 3, 2 };
            // FUN_00426f70(0, indices);  // Light vertices - stubbed

            // 0x0046fca0: Finalize background
            // FUN_00427250();  // - stubbed

            // 0x0046fcb0: Create execute buffer for this quad
            if (dataOk && slotOk) {
                int handle = FUN_0046c230(&g_VideoDriverArray_D0 + slotOffset);
                g_VideoDriverArray_068[slotElem] = handle;
            }
        }
    }
}

// ============================================================================
// SetupTexturePageHandles (0x0046d0e0, renamed from FUN_0046d0e0)
//
// Manages GPU texture handle creation for previously loaded texture pages.
// Called after LoadTexturePage to actually register the handles in the page table.
//
// Two modes:
//   pageIndex == 0:  Rebuild ALL texture page handles for the slot.
//                    Iterates through each page descriptor, creates GPU handles
//                    via create_texture_page with mode 2, and registers them.
//                    Fills remaining 8 slots with zeros.
//
//   pageIndex != 0:  Clear all 8 slots, then create a SINGLE page handle
//                    at slot[pageIndex - 1]. Used for multi-CLUT textures
//                    where only one specific page needs updating.
//
// Both modes apply the same 0xF slot shift as LoadTexturePage.
// ============================================================================
void SetupTexturePageHandles(int slotIndex, int pageIndex)
{
    // 0x0046d0e0: Shift slot by 0xF (matching LoadTexturePage)
    slotIndex = slotIndex + 0xF;

    // Same legacy-descriptor caveat as LoadTexturePage: these tables are
    // addressed by the shifted slot and the port's stand-ins are far smaller
    // than the original's 393KB span, so every slot >= 4 is out of range. The
    // render path never reads them, but the writes used to land in whatever
    // followed the arrays in .bss - at startup the slot-15 write (offset 3345
    // into a 1024-byte table) landed in g_imageBufferDataB. Every access below
    // is range-checked; out-of-range stores are skipped and the count reads
    // yield 0, which is what makes the whole block a no-op in this port.
    int slotOffset = slotIndex * 0x37C;
    int texCheckOffset = slotIndex * 0xDF;

    // g_TexturePageTable_DAT is indexed two incompatible ways - by ELEMENT in
    // SpriteRenderer.cpp and by BYTE offset here - so both forms need the check.
    const bool tableOk = (size_t)texCheckOffset + 8 * sizeof(DWORD)
                         <= sizeof(g_TexturePageTable_DAT);
    const bool elemOk  = (size_t)texCheckOffset < sizeof(g_TexturePageTable_DAT) / sizeof(DWORD);
    const bool cntOk   = (size_t)texCheckOffset < sizeof(g_VideoDriverArray_810) / sizeof(DWORD);
    const bool descOk  = (size_t)slotOffset + 0x68 <= sizeof(g_VideoDriverArray_4d0);

    if (pageIndex == 0) {
        // --- Rebuild all pages ---
        int pageCount = cntOk ? g_VideoDriverArray_810[texCheckOffset] : 0;
        int count = 0;

        if (pageCount > 0 && tableOk && descOk) {
            BYTE* pageData = (BYTE*)&g_VideoDriverArray_4d0 + slotOffset;
            DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);

            for (int i = 0; i < pageCount; i++) {
                if ((size_t)slotOffset + (size_t)i * 0x68 + 0x54 > sizeof(g_VideoDriverArray_4d0)) {
                    break;
                }
                // 0x0046d120: Set page flag and create GPU handle with mode 2
                *(DWORD*)(pageData + 0x50) = 1;
                int handle = create_texture_page(pageData, 2);
                *pageTable = (DWORD)handle;

                // 0x0046d140: Commit/flush if debug mode enabled
                // if (DAT_008ed470 != 0) { (**(code**)(*pageData + 0x18))(); }

                count++;
                pageData += 0x68;     // Next page descriptor (0x1A dwords = 0x68 bytes)
                pageTable++;
            }
        }

        // 0x0046d170: Fill remaining 8 slots with zero
        if (count < 8 && tableOk) {
            DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset + count * sizeof(DWORD));
            for (int i = 8 - count; i > 0; i--) {
                *pageTable = 0;
                pageTable++;
            }
        }
    } else {
        // --- Setup single page at index (pageIndex - 1) ---

        // 0x0046d1a0: Clear all 8 page table slots to zero
        if (tableOk) {
            DWORD* pageTable = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + texCheckOffset);
            for (int i = 0; i < 8; i++) {
                *pageTable = 0;
                pageTable++;
            }
        }

        // 0x0046d1c0: Compute offset for the specific page
        int pageOffset = slotOffset + (pageIndex - 1) * 0x68;

        // 0x0046d1d0: Set flag and create GPU handle
        if ((size_t)pageOffset + 0x68 <= sizeof(g_VideoDriverArray_4d0)) {
            if ((size_t)pageOffset / sizeof(DWORD) < sizeof(g_VideoDriverArray_520) / sizeof(DWORD)) {
                g_VideoDriverArray_520[pageOffset / sizeof(DWORD)] = 1;
            }
            int handle = create_texture_page((BYTE*)&g_VideoDriverArray_4d0 + pageOffset, 2);

            // Store handle in the correct page table slot
            int pageSlot = texCheckOffset + pageIndex - 1;
            if (elemOk && (size_t)pageSlot < sizeof(g_TexturePageTable_DAT) / sizeof(DWORD)) {
                ((DWORD*)&g_TexturePageTable_DAT)[pageSlot] = (DWORD)handle;
            }
        }
    }
}

// ============================================================================
// LoadImage (0x0046d3b0)
// Marni System PSYQ LoadImage equivalent.
// Copies raw pixel data from main memory into a CMarniBits surface within the
// texture page table, then creates texture page handles.
//
// Parameters (from Ghidra + assembly analysis):
//   srcData  - pointer to raw pixel data (e.g. 16-bit RGB555)
//   srcSlot  - source texture slot index (template CMarniBits for format)
//   dstSlot  - destination texture slot index
//   format   - pixel format index (1 = 16-bit, bpp=2; indexed into table at 0x4c2d78)
//   x        - X position in destination surface (pixels)
//   y        - Y position in destination surface (pixels)
//   width    - width in pixels to copy
//   height   - height in pixels to copy
//   mode     - CMarniBits sub-page index within the source slot
// ============================================================================
void LoadImage(int srcData, int srcSlot, int dstSlot, short format,
               short x, short y, short width, short height, int mode) // 0x0046d3b0
{
    int destSlot = dstSlot + 0xF;
    int destCheck = destSlot * 0xDF;
    int destOff = destSlot * 0x37C;
    int srcOff = (srcSlot + 0xF) * 0x37C + mode * 0x68;

    // Bytes per pixel from format index (table at 0x4c2d78)
    static const int bppTable[] = { 1, 2, 4 };
    int bpp = (format >= 0 && format < 3) ? bppTable[format] : 2;

    // --- legacy Marni descriptor bounds --------------------------------------
    // Same problem LoadTexturePage/ProcessTextureImage were already guarded for,
    // and this function had it worse. The original's per-slot descriptor arrays
    // give every slot 0xDF DWORDs (0x37C bytes) and span ~393KB of Marni memory
    // (see display_texture's note); the port's stand-ins hold roughly ONE slot's
    // worth. LoadImage then biases every slot by 0xF, so the smallest possible
    // destCheck is 0xF*0xDF = 3345 and the smallest destOff is 0xF*0x37C =
    // 13332 - i.e. every single call indexed tens of KB past the end of
    // DWORD[256]/DWORD[1024] arrays. The destroy loop below is the dangerous
    // one: both the "is the slot live" flag and the page COUNT came from
    // whatever .bss happened to sit at that offset (for the item box that lands
    // in g_bgPakLoadBuffer, i.e. raw file bytes), and it then zeroed that many
    // DWORDs starting tens of KB into .bss. The item box drives the largest
    // offsets in the game - itembox_draw_slot_icon passes dstSlot 0xF..0x16, so
    // destSlot 30..37 and destOff up to 33004 - which is how live globals near
    // g_ItemSlotsPointer got zeroed and menu_draw_inventory faulted reading
    // ITEM_SLOTS[equipped*2-2] at a NULL base.
    //
    // The D3D11 VRAM-page path below is what actually puts these images on
    // screen (it keys off destSlot, which is range-checked separately), so
    // skipping the legacy descriptor work costs nothing today. The checks are
    // written against the real array sizes rather than hardcoded so the code
    // starts working again if the stand-ins are ever sized properly.
    const int  srcCheck    = (srcSlot + 0xF) * 0xDF;
    const bool destCntOk   = (size_t)(destCheck + 1) * sizeof(DWORD) <= sizeof(g_VideoDriverArray_814) &&
                             (size_t)(destCheck + 1) * sizeof(DWORD) <= sizeof(g_VideoDriverArray_810);
    const bool srcCntOk    = srcCheck >= 0 &&
                             (size_t)(srcCheck + 1) * sizeof(DWORD) <= sizeof(g_VideoDriverArray_814);
    const bool destTableOk = (size_t)(destCheck + 1) * sizeof(DWORD) <= sizeof(g_TexturePageTable_DAT);
    const bool srcBitsOk   = srcOff  >= 0 && (size_t)srcOff  + sizeof(CMarniBits) <= sizeof(g_VideoDriverArray_4d0);
    const bool destBitsOk  = destOff >= 0 && (size_t)destOff + sizeof(CMarniBits) <= sizeof(g_VideoDriverArray_4d0);
    const bool destDescOk  = (size_t)destOff + 0x24 <= sizeof(g_VideoDriverArray_838);
    const bool srcDescOk   = (size_t)(srcSlot + 0xF) * 0x37C + 0x24 <= sizeof(g_VideoDriverArray_838);

    // Destroy existing texture pages at destination slot
    if (destCntOk && destTableOk && g_VideoDriverArray_814[destCheck] != 0) {
        DWORD pageCount = g_VideoDriverArray_810[destCheck];
        DWORD room = (DWORD)(sizeof(g_TexturePageTable_DAT) / sizeof(DWORD)) - (DWORD)destCheck;
        if (pageCount > room) pageCount = room;
        DWORD* handles = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + destCheck * sizeof(DWORD));
        for (DWORD i = 0; i < pageCount; i++) {
            if (handles[i] != 0) {
                destroy_texture_page(handles[i]);
                handles[i] = 0;
            }
        }
        VideoDriver_ClearArrayD0();
    }

    // VRAM page simulation: maintain a persistent RGBA buffer per source page
    // (identified by srcOff). LoadImage composites pixel data at position (x, y)
    // within this buffer, just like the PS1 BIOS LoadImage copies to VRAM.
    // The D3D11 SRV is then created from the full VRAM page buffer.
    // Multiple destSlots may share the same source VRAM page (e.g. item images
    // composited at different y-offsets all share srcSlot=0, mode=2). We track
    // all destSlots per VRAM page so the SRV is updated everywhere it's needed.
    #define VRAM_PAGE_W 256
    #define VRAM_PAGE_H 256
    #define VRAM_PAGE_MAX_SLOTS 64
    #define VRAM_MAX_DESTS 16
    struct VRAMPage {
        DWORD* rgba;
        int destSlots[VRAM_MAX_DESTS];
        int numDests;
    };
    static VRAMPage s_vramPages[VRAM_PAGE_MAX_SLOTS];
    static int s_vramInit = 0;
    if (!s_vramInit) { memset(s_vramPages, 0, sizeof(s_vramPages)); s_vramInit = 1; }

    int vramIdx = srcOff & (VRAM_PAGE_MAX_SLOTS - 1);
    if (srcData != 0 && bpp == 2 && (int)width > 0 && (int)height > 0 && vramIdx >= 0 && vramIdx < VRAM_PAGE_MAX_SLOTS) {
        VRAMPage* vp = &s_vramPages[vramIdx];
        if (vp->rgba == NULL) {
            vp->rgba = new DWORD[VRAM_PAGE_W * VRAM_PAGE_H]();
            vp->numDests = 0;
        }

        if (destSlot >= 0 && destSlot < 256) {
            int found = 0;
            for (int i = 0; i < vp->numDests; i++) {
                if (vp->destSlots[i] == destSlot) { found = 1; break; }
            }
            if (!found && vp->numDests < VRAM_MAX_DESTS) {
                vp->destSlots[vp->numDests++] = destSlot;
            }
        }

        DWORD* page = vp->rgba;
        int dstX = ((int)x & 0x3F) * 2;
        int dstY = (int)y;

        // item_all.pix is 8bpp indexed data. The CLUT comes from status.tim
        // (slot 15), loaded earlier by LoadTexturePage with 8bpp + CLUT.
        // LoadImage format=1 (16-bit) copies raw bytes; at 8bpp this means
        // width*2 bytes per row = width*2 pixels per row.
        TextureCLUTCache* clut = &g_CLUTCache[15];
        if (clut->clutData != NULL && clut->bpp == 8 && clut->clutEntries >= 256) {
            BYTE* src8 = (BYTE*)srcData;
            int pixW = (int)width * 2;
            int pixH = (int)height;
            // Use CLUT from clutY: 0x1E4 → X=4, 0x1E0 → X=0.
            // CLUT X is in units of 16 halfwords. For 256-entry CLUT (512 bytes
            // = 256 halfwords), CLUT index = X / 16. 0x1E4 X=4 → CLUT 0,
            // but items use 0x1E4 for normal and 0x1E0 for special.
            // Try CLUT 0 first; the correct palette is the one that matches
            // the original PS1 game.
            int clutIndex = 2;
            WORD* clutPalette = clut->clutData + clutIndex * 256;

            for (int row = 0; row < pixH; row++) {
                for (int col = 0; col < pixW; col++) {
                    BYTE idx = src8[row * pixW + col];
                    WORD clr = clutPalette[idx];
                    DWORD r = ((clr >> 0)  & 0x1F) * 255 / 31;
                    DWORD g = ((clr >> 5)  & 0x1F) * 255 / 31;
                    DWORD b = ((clr >> 10) & 0x1F) * 255 / 31;
                    DWORD a = (idx == 0) ? 0x00 : 0xFF;
                    DWORD rgba = (a << 24) | (b << 16) | (g << 8) | r;
                    int px = dstX + col;
                    int py = dstY + row;
                    if (px >= 0 && px < VRAM_PAGE_W && py >= 0 && py < VRAM_PAGE_H) {
                        page[py * VRAM_PAGE_W + px] = rgba;
                    }
                }
            }
        } else {
            WORD* pixels = (WORD*)srcData;
            int w = (int)width;
            int h = (int)height;

            for (int row = 0; row < h; row++) {
                for (int col = 0; col < w; col++) {
                    WORD pixel = pixels[row * w + col];
                    DWORD r = ((pixel >> 0)  & 0x1F) * 255 / 31;
                    DWORD g = ((pixel >> 5)  & 0x1F) * 255 / 31;
                    DWORD b = ((pixel >> 10) & 0x1F) * 255 / 31;
                    DWORD a = (r == 0 && g == 0 && b == 0) ? 0x00 : 0xFF;
                    DWORD rgba = (a << 24) | (b << 16) | (g << 8) | r;
                    for (int sx = 0; sx < 2; sx++) {
                        int px = dstX + col * 2 + sx;
                        int py = dstY + row;
                        if (px >= 0 && px < VRAM_PAGE_W && py >= 0 && py < VRAM_PAGE_H) {
                            page[py * VRAM_PAGE_W + px] = rgba;
                        }
                    }
                }
            }
        }

        for (int d = 0; d < vp->numDests; d++) {
            int slot = vp->destSlots[d];
            if (slot >= 0 && slot < 256) {
                if (g_TexturePageSRV[slot] != MARNI_NULL_HANDLE) {
                    Marni_DX()->DestroyTexture(g_TexturePageSRV[slot]);
                    g_TexturePageSRV[slot] = MARNI_NULL_HANDLE;
                }
                MarniCreateTexture(VRAM_PAGE_W, VRAM_PAGE_H, 32, page, &g_TexturePageSRV[slot]);
                g_TexturePageWidth[slot] = VRAM_PAGE_W;
                g_TexturePageHeight[slot] = VRAM_PAGE_H;
                g_TexturePageBpp[slot] = 16;
                // Set VRAM page metadata so display_texture can locate this
                // SRV by depth/UV bounds. Items are rendered at depth=0x1d
                // with texU=88, texV=slot*32, clutY=0x1e4.
                g_TexturePageId[slot] = 0x1d;
                g_TexturePageClutBase[slot] = 0x1e0;
                g_TexturePageOriginX[slot] = 0;
                g_TexturePageOriginY[slot] = 0;
            }
        }
    }

    // Everything from here on works through the legacy per-slot descriptors, so
    // it only runs when every one of them is actually in range. Note this is not
    // merely a bad-pointer read: srcBits->Lock() hands back m_pPixelData read out
    // of an unrelated global, and the copy loop below then writes width*height
    // 16-bit pixels through it.
    if (!srcBitsOk || !destBitsOk || !destDescOk || !srcDescOk ||
        !destCntOk || !srcCntOk || !destTableOk) {
#ifdef _DEBUG
        // One line per distinct slot pair, not per call - this fires every frame
        // the item box is open otherwise.
        static int s_reported[64];
        static int s_reportCount = 0;
        int key = (dstSlot << 8) | (srcSlot & 0xFF);
        bool seen = false;
        for (int i = 0; i < s_reportCount; i++) if (s_reported[i] == key) { seen = true; break; }
        if (!seen && s_reportCount < 64) {
            s_reported[s_reportCount++] = key;
            dbg_printf("[TEX] LoadImage: legacy descriptors out of range, skipped "
                       "(srcSlot=%d dstSlot=%d destCheck=%d destOff=%d srcOff=%d)\n",
                       srcSlot, dstSlot, destCheck, destOff, srcOff);
        }
#endif
        return;
    }

    // Lock source CMarniBits to get its pixel data buffer
    CMarniBits* srcBits = (CMarniBits*)((BYTE*)&g_VideoDriverArray_4d0 + srcOff);
    void* lockedPixels = NULL;
    void* lockedPalette = NULL;
    if (!srcBits->Lock(&lockedPixels, (DWORD*)&lockedPalette))
        return;

    // Copy raw pixel data from srcData into the locked CMarniBits buffer.
    // The copy writes width*height 16-bit pixels starting at position (x, y)
    // in the destination surface, row by row.
    // Row stride comes from the source's m_width, NOT m_pitch: the original
    // divides *(src+0x2C) by bpp at 0x0046d47f and again at 0x0046d4d4 to
    // advance a row. bpp here is pixels-per-16-bit-word (1/2/4 for 16/8/4 bpp),
    // so m_width / bpp is the number of 16-bit words in a row - exactly the
    // unit dstOffset is counted in below.
    BYTE* destPixels = (BYTE*)lockedPixels;
    int rowStride = (int)srcBits->m_width / bpp;  // row stride in 16-bit words
    int dstOffset = rowStride * (int)y + (int)x;  // starting offset in words
    BYTE* srcPtr = (BYTE*)srcData;

    for (int row = 0; row < (int)height; row++) {
        if ((int)width > 0) {
            int byteOff = dstOffset * 2;
            for (int col = 0; col < (int)width; col++) {
                *(WORD*)(destPixels + byteOff) = *(WORD*)srcPtr;
                srcPtr += 2;
                byteOff += 2;
            }
        }
        dstOffset += rowStride;
    }

    // CalcAddress on source, then Unlock source.
    // 0x0046d4ea..0x0046d4ff: push [esp+0x1c] (y), then imul bpp by [esp+0x20]
    // (x) and push that - i.e. CalcAddress(bpp * x, y). Those are the same two
    // stack slots dstOffset is built from above, so whatever the arguments are
    // really named, they must match the ones used for dstOffset. This read
    // dstSlot/format instead, which contradicted the dstOffset transcription.
    void* calcAddr = srcBits->CalcAddress(bpp * (int)x, (int)y);
    srcBits->Unlock();

    // SetAddress on destination CMarniBits (first sub-page at destOff)
    CMarniBits* dstBits = (CMarniBits*)((BYTE*)&g_VideoDriverArray_4d0 + destOff);
    dstBits->SetAddress(calcAddr, lockedPixels);

    // Copy pixel format descriptor from source to dest (6 DWORDs + 1 WORD = 26 bytes)
    BYTE* srcDesc = (BYTE*)srcBits + 0x10;
    BYTE* dstDesc = (BYTE*)dstBits + 0x10;
    memcpy(dstDesc, srcDesc, 26);

    // Copy bitDepth and paletteFormat from source
    dstBits->m_bitDepth = srcBits->m_bitDepth;
    dstBits->m_paletteFormat = srcBits->m_paletteFormat;

    // Set destination surface dimensions. The original writes all three of
    // +0x2C/+0x30/+0x34 at 0x0046d564/0x0046d56a/0x0046d586:
    //   m_width  = width * bpp        (bpp = pixels per 16-bit word, so this is
    //                                  the width in PIXELS)
    //   m_height = height
    //   m_pitch  = srcBits->m_pitch   (inherited - the destination is a window
    //                                  into the source surface, so it keeps the
    //                                  source's row stride)
    // This previously put width*bpp into m_pitch and left m_width holding a
    // stale value from whatever last used the slot. Everything downstream that
    // sizes a texture from m_width then read garbage; VTable_CreateTextureHandle
    // walked off the end of the pixel buffer and faulted.
    dstBits->m_width  = (int)width * bpp;
    dstBits->m_height = (int)height;
    dstBits->m_pitch  = srcBits->m_pitch;
    dstBits->m_field38 = srcBits->m_field38;

    // Set flags
    dstBits->m_isValid = 1;
    dstBits->m_dataSource = 0;
    dstBits->m_hasPalette = 1;
    dstBits->m_ownsPalette = 1;
    dstBits->m_flag50 = 1;

    // Set texture descriptor (VRAM position and size)
    WORD* texDesc = (WORD*)((BYTE*)&g_VideoDriverArray_838 + destOff);
    short sign = x >> 15;
    short wrappedX = (short)((((x ^ sign) - sign) & 0x3F) ^ sign) - sign;
    texDesc[0] = (WORD)wrappedX;
    texDesc[1] = (WORD)y;
    texDesc[2] = (WORD)width;
    texDesc[3] = (WORD)height;

    // Copy additional descriptor fields from source slot
    WORD* srcTexDesc = (WORD*)((BYTE*)&g_VideoDriverArray_838 + (srcSlot + 0xF) * 0x37C);
    texDesc[4] = srcTexDesc[4];
    texDesc[5] = srcTexDesc[5];
    texDesc[6] = srcTexDesc[6];
    texDesc[7] = srcTexDesc[7];
    texDesc[8] = srcTexDesc[8] + (short)(((int)(short)x + ((int)(short)x >> 31 & 0x3F)) >> 6);

    // Set slot metadata: 1 page, copy flag from source
    g_VideoDriverArray_810[destCheck] = 1;
    g_VideoDriverArray_814[destCheck] = g_VideoDriverArray_814[(srcSlot + 0xF) * 0xDF];

    // Create texture page handles for each CMarniBits sub-page
    DWORD* pageHandles = (DWORD*)((BYTE*)&g_TexturePageTable_DAT + destCheck * sizeof(DWORD));
    BYTE* pageData = (BYTE*)dstBits + 0x50; // start at m_flag50 of first CMarniBits
    for (DWORD i = 0; i < g_VideoDriverArray_810[destCheck]; i++) {
        *(DWORD*)pageData = 1; // set m_flag50
        int handle = create_texture_page(pageData - 0x50, 2); // pass CMarniBits base
        pageHandles[i] = (DWORD)handle;
        pageData += 0x68; // next CMarniBits
    }

}

// LoadPSXImage - thin wrapper around PSXTexture::Store
void LoadPSXImage(PSXTexture* tex, void* buf, int mode)
{
    tex->Store((int*)buf, mode);
}

// ============================================================================
// SetupTextureBankData (0x00473a30) - Process texture queue bank data
// Sets up texture bank pointers and copies initial texture state when the
// texture queue has entries. Called during room initialization.
// param_1: texture bank ID (short, typically _g_TextureBankID >> 8)
// ============================================================================
void SetupTextureBankData(short param_1)
{
    // 0x00473a30: Skip if no texture queue entries
    if (DAT_00ae9f04 == 0) return;

    // 0x00473a3e: Calculate bank count and pointers
    DAT_00ae9f06 = (DWORD)(param_1 - 10);
    DAT_00ae9f00 = (DWORD)g_loadDataDestPointer;
    DAT_00ae9efc = (DWORD)DAT_00ae9f06 * 0x200 + (DWORD)g_loadDataDestPointer;

    // 0x00473a6d: Advance load pointer
    g_loadDataDestPointer = (char*)g_loadDataDestPointer + (DWORD)DAT_00ae9f06 * 0x400;

    // 0x00473a7e: Process pending texture operations. The original calls
    // 0x00483510 here, a stub that just returns 0 - call dropped

    // 0x00473a86: Copy texture data to secondary buffer
    unsigned short idx = 0;
    if (DAT_00ae9f06 != 0) {
        do {
            unsigned int i = (unsigned int)idx;
            idx = idx + 1;
            *(DWORD*)(DAT_00ae9efc + i * 4) = *(DWORD*)(DAT_00ae9f00 + i * 4);
        } while ((unsigned int)idx < (DWORD)DAT_00ae9f06 * 0x80);
    }
}

