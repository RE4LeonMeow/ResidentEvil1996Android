// FileLoader.cpp - Asset file loading implementation
// Original function: LoadFile at 0x00411e10 (Ghidra)
// Handles fopen/fread with retry logic and path resolution
#include "../Globals.h"
#include "../platform/platform.h"
#include "../system/AssetPath.h"
#include "FileLoader.h"
#include <stdio.h>

// ============================================================================
// LoadFile - Load a game asset file into memory (0x00411e10)
//
// The original function has retry logic:
//   1. If flag bit 5 is set: prepend install directory to path (skipped in dev)
//   2. Try fopen(path, "rb")
//   3. If fails, retry up to 2 more times (sprite buffer flag / Task_sleep)
//   4. On success: fseek to end, ftell for size, fseek back, fread into buffer
//   5. On failure after retries: show error message, return -1
// ============================================================================
size_t LoadFile(const char* path, void* buffer, unsigned char flags)
{
    // Paths arrive already rooted at GAME_DATA_ROOT (see system/AssetPath.h).
    // ResolveAssetRoot remaps that compile-time root to the config-selected one
    // (config.ini [Assets] Version), a no-op unless the JPN tree is active.
    char resolved[260];
    const char* filePath = ResolveAssetRoot(path, resolved, sizeof(resolved));

    // Platform path fix-up: separator conversion and, on a case-sensitive
    // filesystem, case-insensitive component matching (docs/LINUX_PORT.md).
    char normalized[260];
    filePath = plat_normalize_path(filePath, normalized, sizeof(normalized));

    // Original: if (DAT_004b3998 & flags) prepend the registry install
    // directory for flagged loads. The port's GAME_DATA_ROOT (system/AssetPath.h)
    // already yields a valid working-directory-relative path in both build
    // configs, and a standalone run has no registry install entry - prepending
    // an empty one while skipping the first 8 characters truncated every
    // 0x20-flagged path ("ta\staitem.tim") and black-screened the Release
    // title flow. The flag is therefore accepted and ignored here.
    (void)flags;

    int retryCount = 0;
    BOOL showError = FALSE;

    do {
        // Try to open the file
        FILE* fp = fopen(filePath, "rb");
        if (fp != NULL) {
            // Get file size
            fseek(fp, 0, SEEK_END);
            size_t fileSize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // Read into buffer
            size_t bytesRead = fread(buffer, 1, fileSize, fp);
            fclose(fp);

            if (bytesRead == fileSize) {
                return fileSize;
            }

            // Partial read - treat as failure
            return (size_t)-1;
        }

        // File open failed
        if (showError) {
            // Original: sprintf + display error message
            char errorMsg[256];
            sprintf(errorMsg, "Could not open file: %s", filePath + 2);
            // Original calls FUN_00497e90() which shows a dialog
            // In dev mode, just OutputDebugString for simplicity
            OutputDebugStringA("[LOAD] FAILED: ");
            OutputDebugStringA(filePath);
            OutputDebugStringA("\n");
            return (size_t)-1;
        }

        // Retry: the original sets some global flag and tries again
        // DAT_004d4690 = 0xFF - some retry indicator
        showError = TRUE;
        retryCount++;

    } while (retryCount < 2);

    return (size_t)-1;
}

// ============================================================================
// LZW Decompression (unpack_pakfile_ at 0x00425ab0)
// Helper functions and main decompression routine for PAK files.
// ============================================================================

// FUN_00425a70 - Reset LZW decompression dictionary
static void pak_decomp_reset(void)
{
    // 0x00425a70-0x00425a5e: set field +0 of every 12-byte record to -1. Note
    // this clears the record's UNUSED word, not the prefix — the decoder never
    // reads it, so the reset is effectively vestigial. Reproduced as-is.
    for (int i = 0; i < PAK_DICT_ENTRIES; i++) {
        g_pakDict[i].unused = -1;
    }
    g_pakDecompNextCode = 0x103;
    g_pakDecompCodeSize = 9;
    g_pakDecompMaxCode = 0x1ff;   // 0x00425a68: _DAT_00d2b0a0
}

// FUN_00425a00 - Read a code of 'codeSize' bits from the input bitstream
static unsigned int pak_decomp_read_code(void* src, unsigned int codeSize)
{
    unsigned int result = 0;
    unsigned int bit = 1 << (codeSize - 1);

    while (bit != 0) {
        // 0x00425a10: Refill bit buffer when empty
        if (g_pakDecompBitMask == 0x80) {
            g_pakDecompCurByte = ((unsigned char*)src)[g_pakDecompInputPos];
            g_pakDecompInputPos++;
        }
        // 0x00425a30: Test current bit
        if ((g_pakDecompCurByte & g_pakDecompBitMask) != 0) {
            result |= bit;
        }
        // 0x00425a48: Advance to next bit
        g_pakDecompBitMask >>= 1;
        bit >>= 1;
        if (g_pakDecompBitMask == 0) {
            g_pakDecompBitMask = 0x80;
        }
    }
    return result;
}

// FUN_00425bc0 - Decode a string from the LZW dictionary into g_pakStringBuf
// Returns the count of characters written (starting from param_1)
static int pak_decomp_decode_string(int startPos, unsigned int code)
{
    if (code > 0xFF) {
        // Multi-character: walk the prefix chain, emitting characters in reverse
        int pos = startPos;
        do {
            unsigned int idx = code;
            code = (unsigned int)g_pakDict[idx].prefix;
            g_pakStringBuf[pos] = g_pakDict[idx].ch;
            pos++;
        } while (code > 0xFF);
        g_pakStringBuf[pos] = (char)code;
        return pos + 1;
    }
    // Single character
    g_pakStringBuf[startPos] = (char)code;
    return startPos + 1;
}

// unpack_pakfile_ (0x00425ab0) - LZW decompression of a PAK file
// src: pointer to compressed PAK data
// dst: pointer to output buffer for decompressed data
// Returns: number of bytes written to dst
int unpack_pakfile_(void* src, void* dst)
{
    int outPos = 0;
    g_pakDecompInputPos = 0;
    g_pakDecompBitMask = 0x80;
    g_pakDecompCurByte = 0;

    do {
        // 0x00425abf: Reset dictionary
        pak_decomp_reset();

        // 0x00425ac4: Read first code
        unsigned int curCode = pak_decomp_read_code(src, g_pakDecompCodeSize);
        if (curCode == 0x100) {
            return outPos;
        }

        // 0x00425ade: Output first character
        ((unsigned char*)dst)[outPos] = (unsigned char)curCode;
        outPos++;
        unsigned int prevCode = curCode;

        // The original tracks the first character of the PREVIOUSLY decoded
        // string separately from the previous code (local_4 vs local_8). They
        // only coincide while codes are single characters, so they must not be
        // conflated — the KwKwK case below appends this character.
        unsigned int prevFirstChar = curCode;

        // 0x00425aee: Main decompression loop
        while (true) {
            curCode = pak_decomp_read_code(src, g_pakDecompCodeSize);

            // 0x100 = end of data
            if (curCode == 0x100) {
                return outPos;
            }
            // 0x102 = reset dictionary (restart outer loop)
            if (curCode == 0x102) {
                break;
            }
            // 0x101 = increase code size
            if (curCode == 0x101) {
                g_pakDecompCodeSize++;
                continue;
            }

            // 0x00425b20: KwKwK case — the code is not in the table yet, so
            // decode the PREVIOUS string and append its first character. That
            // trailing character goes in stringBuf[0], which the reversed output
            // loop below emits last.
            unsigned int lookupCode = curCode;
            bool special = (g_pakDecompNextCode <= curCode);
            if (special) {
                g_pakStringBuf[0] = (char)prevFirstChar;
                lookupCode = prevCode;
            }

            // 0x00425b3d: Decode string (reversed into g_pakStringBuf)
            int charCount = pak_decomp_decode_string(special ? 1 : 0, lookupCode);

            // 0x00425b50: The decoded string is reversed, so its first character
            // is the last one written. The original reads this uniformly, with no
            // special-case branch.
            char firstChar = g_pakStringBuf[charCount - 1];
            prevFirstChar = (unsigned int)firstChar;

            // 0x00425b64: Emit the string forwards by walking the buffer back
            // down to index 0 (which is the appended char in the KwKwK case).
            for (int i = charCount; i != 0; i--) {
                ((unsigned char*)dst)[outPos] = (unsigned char)g_pakStringBuf[i - 1];
                outPos++;
            }

            // 0x00425b90: Add the new dictionary entry. Its prefix is the code
            // from the PREVIOUS iteration, so prevCode must not be advanced
            // until after this write.
            unsigned int newIdx = g_pakDecompNextCode;
            g_pakDecompNextCode = newIdx + 1;
            g_pakDict[newIdx].prefix = (int)prevCode;
            g_pakDict[newIdx].ch = firstChar;

            // 0x00425ba9: Update state for next iteration
            prevCode = curCode;
        }
    } while (true);
}
