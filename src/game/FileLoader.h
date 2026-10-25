// FileLoader.h - Asset file loading (replaces original LoadFile at 0x00411e10)
// Handles path resolution and file I/O for game assets
#pragma once
#include "../platform/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// LoadFile - Load a game asset file into memory
//   path:     Built with GAME_DATA_ROOT, e.g. GAME_DATA_ROOT "data\\fontus.tim"
//   buffer:   Destination buffer for file data
//   flags:    Load flags (bit 5 = use install directory prefix)
// Returns: File size on success, -1 (0xFFFFFFFF) on failure
// Original address: 0x00411e10
size_t LoadFile(const char* path, void* buffer, unsigned char flags);

#ifdef __cplusplus
}
#endif
