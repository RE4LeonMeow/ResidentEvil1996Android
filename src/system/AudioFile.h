// AudioFile.h - port-added: one loader for the sound banks of both audio
// backends (XAudio2 in marni/MarniSound.cpp, SDL in platform/linux/audio.cpp).
//
// The original only ever opens "<root>sound\<name>.wav" / "voice\<name>.wav".
// The asset migrator can replace those WAVs with the PS1 disc's audio, either
// as WAV or as Ogg Vorbis, so a ".wav" path is first looked for as ".ogg".
// A PC tree has no .ogg files, so for it nothing changes.
//
// Loop regions: the PS1 sequencer loops a BGM's loop body and plays the intro
// once; the PC build always loops the whole buffer. Files written by the
// migrator carry their loop region (WAV `smpl` chunk / Vorbis LOOPSTART +
// LOOPLENGTH) plus a marker, and the region is reported ONLY for marked files:
// the PC tree's own BGM_33.WAV has a `smpl` chunk the original never used.
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AudioFileData {
    unsigned char* buffer;     // malloc'd, owns the PCM; release with free()
    unsigned char* pcm;        // points into `buffer`
    unsigned int   pcmSize;    // bytes
    int            sampleRate;
    int            channels;
    int            bitsPerSample;  // 8 or 16
    int            ps1;        // written by the asset migrator (PS1 audio)
    unsigned int   loopBegin;  // frames; loopEnd > loopBegin only when set
    unsigned int   loopEnd;    // frames, exclusive
} AudioFileData;

// Which file `path` (already run through ResolveAssetRoot) names on disk: for a
// ".wav" path the ".ogg" beside it wins when it exists. Writes the normalised
// path to `out`; 0 when neither exists.
int AudioFile_Find(const char* path, char* out, size_t outSize);

// Load `path` (as AudioFile_Find resolves it) to PCM. 0 on failure; `out` is
// zeroed then.
int AudioFile_Load(const char* path, AudioFileData* out);

// 1 when the active tree's Sound folder carries the migrator's PS1 audio
// marker (sound\ps1audio.txt). The backends then mix at 44.1 kHz: the PS1
// files are 37.8/44.1 kHz, which the PC's 22.05 kHz mix would throw away.
int AudioFile_TreeHasPs1Audio(void);

#ifdef __cplusplus
}
#endif
