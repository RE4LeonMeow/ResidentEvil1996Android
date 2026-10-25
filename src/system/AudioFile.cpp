// AudioFile.cpp - see AudioFile.h. Port-added; no original counterpart.
#include "AudioFile.h"
#include "AssetPath.h"
#include "../platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// stb_vorbis (public domain / MIT, v1.22), compiled into this translation unit
// only. Memory decoding is all the banks need.
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"

namespace {

// The marker the asset migrator puts in every WAV it writes: a RIFF chunk
// "re1p" (4 bytes, value 1). Vorbis files carry RE1PS1=1 instead.
const unsigned int kChunkFmt  = 0x20746D66;  // 'fmt '
const unsigned int kChunkData = 0x61746164;  // 'data'
const unsigned int kChunkSmpl = 0x6C706D73;  // 'smpl'
const unsigned int kChunkMark = 0x70316572;  // 're1p'

unsigned int rd32(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int FileExists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

int EndsWithWav(const char* path, size_t n)
{
    if (n < 4) return 0;
    const char* e = path + n - 4;
    return e[0] == '.' && (e[1] == 'w' || e[1] == 'W') && (e[2] == 'a' || e[2] == 'A') &&
           (e[3] == 'v' || e[3] == 'V');
}

int LoadWav(unsigned char* data, size_t size, AudioFileData* out)
{
    if (size < 44 || memcmp(data, "RIFF", 4) != 0) return 0;
    size_t offs = 12;
    int channels = 1, rate = 22050, bits = 8;
    unsigned char* pcm = NULL;
    unsigned int pcmSize = 0;
    unsigned int loopStart = 0, loopLast = 0;
    int hasLoop = 0, marked = 0;
    while (offs + 8 <= size) {
        const unsigned int id = rd32(data + offs);
        const unsigned int len = rd32(data + offs + 4);
        const unsigned char* body = data + offs + 8;
        if (id == kChunkFmt && len >= 16 && offs + 8 + 16 <= size) {
            channels = body[2] | (body[3] << 8);
            rate = (int)rd32(body + 4);
            bits = body[14] | (body[15] << 8);
        } else if (id == kChunkData) {
            pcm = (unsigned char*)body;
            pcmSize = len;
            if (offs + 8 + (size_t)pcmSize > size) pcmSize = (unsigned int)(size - offs - 8);
        } else if (id == kChunkSmpl && len >= 60 && offs + 8 + 60 <= size &&
                   rd32(body + 28) != 0) {
            // First loop record: start and INCLUSIVE end, in sample frames.
            loopStart = rd32(body + 36 + 8);
            loopLast = rd32(body + 36 + 12);
            hasLoop = 1;
        } else if (id == kChunkMark) {
            marked = 1;
        }
        offs += 8 + (size_t)len + (len & 1);
    }
    if (pcm == NULL || pcmSize == 0 || channels <= 0 || (bits != 8 && bits != 16)) return 0;
    out->buffer = data;
    out->pcm = pcm;
    out->pcmSize = pcmSize;
    out->sampleRate = rate;
    out->channels = channels;
    out->bitsPerSample = bits;
    out->ps1 = marked;
    const unsigned int frames = pcmSize / (unsigned int)(channels * (bits / 8));
    if (marked && hasLoop && loopStart < loopLast && loopLast < frames) {
        out->loopBegin = loopStart;
        out->loopEnd = loopLast + 1;
    }
    return 1;
}

// A Vorbis comment "KEY=value" as an unsigned number, or -1.
long CommentValue(const stb_vorbis_comment& c, const char* key)
{
    const size_t n = strlen(key);
    for (int i = 0; i < c.comment_list_length; ++i) {
        const char* s = c.comment_list[i];
        if (s == NULL) continue;
        size_t j = 0;
        while (j < n && s[j] != '\0' &&
               ((s[j] >= 'a' && s[j] <= 'z') ? s[j] - 32 : s[j]) == key[j])
            ++j;
        if (j == n && s[n] == '=') return strtol(s + n + 1, NULL, 10);
    }
    return -1;
}

int LoadOgg(const unsigned char* data, size_t size, AudioFileData* out)
{
    int err = 0;
    stb_vorbis* v = stb_vorbis_open_memory(data, (int)size, &err, NULL);
    if (v == NULL) return 0;
    const stb_vorbis_info info = stb_vorbis_get_info(v);
    const unsigned int frames = stb_vorbis_stream_length_in_samples(v);
    if (info.channels <= 0 || info.channels > 2 || frames == 0) {
        stb_vorbis_close(v);
        return 0;
    }
    const size_t bytes = (size_t)frames * info.channels * 2;
    unsigned char* pcm = (unsigned char*)malloc(bytes);
    if (pcm == NULL) {
        stb_vorbis_close(v);
        return 0;
    }
    unsigned int got = 0;
    while (got < frames) {
        const int n = stb_vorbis_get_samples_short_interleaved(
            v, info.channels, (short*)pcm + (size_t)got * info.channels,
            (int)((frames - got) * info.channels));
        if (n <= 0) break;
        got += (unsigned int)n;
    }
    const stb_vorbis_comment c = stb_vorbis_get_comment(v);
    const long loopStart = CommentValue(c, "LOOPSTART");
    const long loopLength = CommentValue(c, "LOOPLENGTH");
    const int marked = CommentValue(c, "RE1PS1") == 1;
    stb_vorbis_close(v);
    if (got == 0) {
        free(pcm);
        return 0;
    }
    out->buffer = pcm;
    out->pcm = pcm;
    out->pcmSize = got * (unsigned int)info.channels * 2;
    out->sampleRate = (int)info.sample_rate;
    out->channels = info.channels;
    out->bitsPerSample = 16;
    out->ps1 = marked;
    if (marked && loopStart >= 0 && loopLength > 0 &&
        (unsigned long)(loopStart + loopLength) <= got) {
        out->loopBegin = (unsigned int)loopStart;
        out->loopEnd = (unsigned int)(loopStart + loopLength);
    }
    return 1;
}

}  // namespace

int AudioFile_Find(const char* path, char* out, size_t outSize)
{
    if (path == NULL || out == NULL || outSize == 0) return 0;
    const size_t n = strlen(path);
    if (EndsWithWav(path, n) && n < 260) {
        char ogg[260];
        memcpy(ogg, path, n - 3);
        memcpy(ogg + n - 3, "ogg", 4);
        const char* p = plat_normalize_path(ogg, out, outSize);
        if (p != out) snprintf(out, outSize, "%s", p);
        if (FileExists(out)) return 1;
    }
    const char* p = plat_normalize_path(path, out, outSize);
    if (p != out) snprintf(out, outSize, "%s", p);
    return FileExists(out);
}

int AudioFile_Load(const char* path, AudioFileData* out)
{
    memset(out, 0, sizeof(*out));
    size_t size = 0;
    unsigned char* data = (unsigned char*)plat_file_read_all(path, &size);
    if (data == NULL) return 0;
    if (size >= 4 && memcmp(data, "OggS", 4) == 0) {
        const int ok = LoadOgg(data, size, out);
        free(data);  // the decoded PCM is a buffer of its own
        if (!ok) memset(out, 0, sizeof(*out));
        return ok;
    }
    if (!LoadWav(data, size, out)) {
        free(data);
        memset(out, 0, sizeof(*out));
        return 0;
    }
    return 1;
}

int AudioFile_TreeHasPs1Audio(void)
{
    char rooted[260];
    const char* p = ResolveAssetRoot(GAME_DATA_ROOT "sound/ps1audio.txt", rooted, sizeof(rooted));
    char norm[260];
    return FileExists(plat_normalize_path(p, norm, sizeof(norm)));
}
