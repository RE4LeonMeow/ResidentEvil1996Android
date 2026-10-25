#pragma once
// Movie conversion: PS1 .STR (MDEC video + CD-XA audio) -> MP4, and the PC
// release's Cinepak AVI -> MP4. Port of tools/str_to_video.py.

#include "core/DiscImage.h"
#include "core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

struct Movie {
    std::string name;
    int width = 0;
    int height = 0;
    int version = 0;
    size_t sectorCount = 0;
    size_t videoSectorEnd = 0;  // exclusive end of valid video sectors
    std::vector<std::vector<uint8_t>> frames;  // demultiplexed bitstreams
    std::vector<uint8_t> audio;                // 128-byte XA sound groups
    bool truncatedAudio = false;
};

// Parse a .STR straight out of a raw image (keeps the Form 2 CD-XA audio).
bool strParseFromDisc(const DiscImage& img, const DiscEntry& entry, Movie* out,
                      std::string* error);

// Parse a 2048-byte-per-sector extract (audio is degraded).
bool strParseExtract(const std::vector<uint8_t>& data, Movie* out,
                     std::string* error);

// ffprobe duration of a file, or -1.
double probeDuration(const std::string& ffprobe, const std::string& path);

// Convert one .STR movie (read from `img`) into `<outDir>/<name>.mp4`, or
// `<outDir>/<outStem>.mp4` when `outStem` is given.
bool convertStrMovie(const DiscImage& img, const DiscEntry& entry,
                     const std::string& outDir, const std::string& ffmpeg,
                     const Progress& progress, std::string* error,
                     const std::string& outStem = std::string());

// Transcode every .avi in `movieDir` to .mp4.
bool convertPcMovies(const std::string& movieDir, const std::string& ffmpeg,
                     bool keepAvi, const Progress& progress,
                     std::string* error);

}  // namespace re1
