#pragma once
// PS1 .BSS room backgrounds -> the PC port's .pak files.

#include "core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

// Byte offsets of each MDEC frame slot in a .BSS (0x8000 stride).
std::vector<size_t> bssFrameOffsets(const std::vector<uint8_t>& data);

// Decode every frame of one .BSS and write RC<stage><room><cam>.pak into
// `outDir`. Returns the written file names in `written`. Honours cancellation
// between frames.
bool convertBss(const std::vector<uint8_t>& data, char stageDigit, int roomId,
                const std::string& outDir, const Progress& progress,
                std::vector<std::string>* written, std::string* error);

}  // namespace re1
