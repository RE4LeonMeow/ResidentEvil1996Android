#pragma once
// PS1 audio -> the PC's sound files: sound effects out of the VAB banks,
// voices out of the CD-XA streams, BGM rendered from the SEP sequences. Which
// PS1 sound becomes which PC file is baked into Ps1AudioManifest.h by
// tools/gen_ps1_audio_manifest.py; this only decodes and writes.
//
// The files REPLACE the tree's own ones in <tree>/Sound and <tree>/Voice, as
// WAV or Ogg Vorbis. Everything written is listed in Sound/PS1AUDIO.TXT, which
// is also what tells the game to mix at 44.1 kHz; removePs1Audio() deletes the
// listed files so a PC copy can restore the originals cleanly. Names the PS1
// has no counterpart for keep the tree's file.

#include "core/DiscImage.h"
#include "core/Types.h"

#include <string>

namespace re1 {

enum class AudioFormat { Wav, Ogg };

struct Ps1AudioOptions {
    bool sfx = true;
    bool voices = true;
    bool bgm = true;
    AudioFormat format = AudioFormat::Wav;
    // libvorbis -q:a (0..10). 6 is ~190 kbit/s stereo, perceptually
    // transparent for 4-bit ADPCM sources, about a fifth of the WAV size.
    int oggQuality = 6;
    std::string ffmpegPath = "ffmpeg";
};

struct Ps1AudioStats {
    int sfx = 0;
    int voices = 0;
    int bgm = 0;
    int skipped = 0;
};

// The list the game and removePs1Audio() read, inside <tree>/Sound.
inline const char* ps1AudioListName() { return "PS1AUDIO.TXT"; }

// Replace the sounds of the tree at `treeRoot` (<target>/USA or JPN). Voices
// need a raw 2352-byte image; from a 2048-byte one they are skipped with a
// warning. OGG needs an ffmpeg with libvorbis.
bool migratePs1Audio(const DiscImage& img, const std::string& treeRoot,
                     const Ps1AudioOptions& opts, const Progress& progress,
                     std::string* error, Ps1AudioStats* stats = nullptr);

// Delete every file a previous migratePs1Audio() listed, and the list. Returns
// the number of files removed (0 when the tree has no PS1 audio).
int removePs1Audio(const std::string& treeRoot, const Progress& progress);

// Render one BGM channel (g_BgmNameTable name, e.g. "BGM_00") to a WAV at
// `wavPath`, for the headless self-test.
bool renderPs1Bgm(const DiscImage& img, const std::string& pcName,
                  const std::string& wavPath, std::string* error);

}  // namespace re1
