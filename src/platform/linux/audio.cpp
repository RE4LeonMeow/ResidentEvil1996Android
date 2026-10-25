// audio.cpp - SDL2 audio backend (Phase 6).
//
// Counterpart of src/marni/MarniSound.cpp (XAudio2). It defines the same
// DirectSound class API, so the shared wrappers in src/game/SoundApi.cpp and
// every caller in src/game/ work unchanged.
//
// XAudio2 mixes one source voice per bank in hardware; SDL2 gives a single
// output stream, so this backend carries a small software mixer in the audio
// callback. The contract preserved from the Windows side:
//   - bank indices 1..80, one per loaded WAV (0 == none)
//   - PlaySound(bank, slot): slot != 0 loops forever, slot == 0 plays once
//   - volume is DirectSound millibels (0 full scale, -10000 silence) and maps
//     to amplitude through 10^(mB/2000) - see the note in MarniSound.cpp
//   - GetStatus returns 1 while the bank is playing, 0 otherwise
#include "Globals.h"
#include "platform/platform.h"
#include "marni/MarniSound.h"
#include "system/AssetPath.h"
#include "system/AudioFile.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   // access() - the silent-null-device check
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, WebMovieAudioPlay, (const short* pcm,long frames,long start,long end,int rate), {
    globalThis.RE1MovieAudio.play(HEAP16.subarray(pcm>>>1,(pcm>>>1)+frames*2),rate,start,end,pcm);
});
EM_JS(void, WebMovieAudioStop, (), {globalThis.RE1MovieAudio?.stop();});
EM_JS(long, WebMovieAudioPosition, (), {return globalThis.RE1MovieAudio?.position()?? -1;});
#endif

// Mixing rate: 22050 Hz like the Windows mastering voice, the rate of every
// PC WAV; 44100 Hz when the tree carries the asset migrator's PS1 audio
// (37.8/44.1 kHz files). Set in InitializeSoundSystem, after the config (and so
// the tree) is known.
static int s_outRate = 22050;
#define OUT_RATE     s_outRate
#define MAX_BANKS    81        // 1..80

namespace {

struct Bank {
    bool     active;
    bool     playing;
    bool     loop;
    BYTE*    wavData;          // owned whole-file buffer
    BYTE*    pcm;              // pointer into wavData
    DWORD    pcmSize;          // bytes of PCM
    int      sampleRate;
    int      channels;
    int      bitsPerSample;
    int      vol;              // millibels
    int      pan;              // -10000..10000
    int      slot;
    int      status;           // 1 playing / 0 stopped
    double   pos;              // fractional frame index
    // Port-added, the asset migrator's PS1 audio files only (AudioFile.h): the
    // loop region in frames ([loopBegin, loopEnd), loopEnd 0 = loop the whole
    // buffer) and linear interpolation, which the nearest-frame stepping the
    // PC's 22 kHz files get would alias on 37.8 kHz material.
    size_t   loopBegin;
    size_t   loopEnd;
    bool     ps1;
    char     name[64];
};

Bank s_banks[MAX_BANKS];

SDL_AudioDeviceID s_dev = 0;
int s_suspended = 0;

float MillibelsToAmplitude(int mB)
{
    if (mB <= -10000) return 0.0f;
    if (mB >= 0) return 1.0f;
    return (float)pow(10.0, (double)mB / 2000.0);
}

Sint32 ReadSample(const Bank& b, size_t frame, int channel)
{
    size_t idx = frame * (size_t)b.channels + (size_t)channel;
    if (b.bitsPerSample == 8) {
        // 8-bit WAV PCM is unsigned, centred on 128.
        return ((Sint32)b.pcm[idx] - 128) << 8;
    }
    // 16-bit little-endian signed.
    return (Sint32)(Sint16)((unsigned)b.pcm[idx * 2] | ((unsigned)b.pcm[idx * 2 + 1] << 8));
}

// Accumulation scratch. SDL serialises callback invocations, so one static
// buffer is safe; the bound is a guard against an absurd callback size.
static Sint32 s_acc[8192 * 2];

// ---------------------------------------------------------------------------
// FMV audio stream (Phase 7)
//
// The movie's audio is decoded up front into one PCM buffer (S16 stereo at
// OUT_RATE) and mixed here rather than through a bank. Two reasons: the video
// backend needs a clock that cannot drift from the sound card, and that clock
// is this stream's play position; and a movie's audio is not a loopable bank.
// The buffer is owned by video.cpp, which frees it after stopping
// the stream. `s_streamPos` is written on the audio thread and read on the
// main thread - a long read/write is atomic on x86, and the value is a
// monotonically increasing cursor, so a torn read is not possible.
// ---------------------------------------------------------------------------
static const short* s_streamPcm = NULL;
static long s_streamEnd   = 0;   // one past the last frame of the segment
static long s_streamPos   = 0;   // absolute frame index played so far
static int  s_streamOn    = 0;

void MixCallback(void* /*userdata*/, Uint8* stream, int len)
{
    memset(stream, 0, (size_t)len);
    if (s_suspended) return;

    const int frames = len / 4;   // stereo S16 = 4 bytes per frame
    if (frames <= 0 || frames > 8192) return;

    Sint32* out = s_acc;
    memset(out, 0, (size_t)frames * 2 * sizeof(Sint32));

    // Movie audio, if any, mixes like a bank but reads its cursor as the clock.
    if (s_streamOn && s_streamPcm != NULL) {
        long pos = s_streamPos;
        for (int f = 0; f < frames; ++f) {
            if (pos >= s_streamEnd) { s_streamOn = 0; break; }
            out[f * 2 + 0] += s_streamPcm[pos * 2 + 0];
            out[f * 2 + 1] += s_streamPcm[pos * 2 + 1];
            pos++;
        }
        s_streamPos = pos;
    }

    for (int b = 1; b < MAX_BANKS; ++b) {
        Bank& bank = s_banks[b];
        if (!bank.active || !bank.playing || bank.pcm == NULL) continue;

        const int bytesPerSample = bank.bitsPerSample / 8;
        const size_t totalFrames = (bytesPerSample > 0 && bank.channels > 0)
            ? bank.pcmSize / ((size_t)bytesPerSample * (size_t)bank.channels)
            : 0;
        if (totalFrames == 0) { bank.playing = false; bank.status = 0; continue; }

        const double ratio = (double)bank.sampleRate / (double)OUT_RATE;
        const float amp = MillibelsToAmplitude(bank.vol);
        const float panL = (float)(10000 - bank.pan) / 20000.0f;
        const float panR = (float)(10000 + bank.pan) / 20000.0f;

        const size_t end = (bank.loop && bank.loopEnd != 0) ? bank.loopEnd : totalFrames;
        for (int f = 0; f < frames; ++f) {
            size_t i = (size_t)bank.pos;
            if (i >= end) {
                if (bank.loop) {
                    const size_t begin = bank.loopEnd != 0 ? bank.loopBegin : 0;
                    bank.pos -= (double)(end - begin);
                    i = (size_t)bank.pos;
                    if (i >= end) { bank.pos = (double)begin; i = begin; }
                }
                else { bank.playing = false; bank.status = 0; break; }
            }

            Sint32 l, r;
            if (bank.ps1) {
                // Next frame for the interpolation: the loop start at the
                // loop's end, silence past a one-shot's.
                size_t j = i + 1;
                if (j >= end) j = bank.loop ? (bank.loopEnd != 0 ? bank.loopBegin : 0) : i;
                const double t = bank.pos - (double)i;
                const int ch1 = bank.channels == 1 ? 0 : 1;
                l = (Sint32)((1.0 - t) * ReadSample(bank, i, 0) + t * ReadSample(bank, j, 0));
                r = (Sint32)((1.0 - t) * ReadSample(bank, i, ch1) + t * ReadSample(bank, j, ch1));
            } else if (bank.channels == 1) {
                l = r = ReadSample(bank, i, 0);
            } else {
                l = ReadSample(bank, i, 0);
                r = ReadSample(bank, i, 1);
            }

            out[f * 2 + 0] += (Sint32)((float)l * amp * panL);
            out[f * 2 + 1] += (Sint32)((float)r * amp * panR);
            bank.pos += ratio;
        }
    }

    // Saturate the 32-bit accumulator down to the S16 output stream.
    Sint16* dst = (Sint16*)stream;
    for (int i = 0; i < frames * 2; ++i) {
        Sint32 v = out[i];
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        dst[i] = (Sint16)v;
    }
}

void StopBank(Bank& bank)
{
    bank.playing = false;
    bank.status = 0;
    bank.pos = 0.0;
}

void FreeBank(Bank& bank)
{
    StopBank(bank);
    free(bank.wavData);
    bank.wavData = NULL;
    bank.pcm = NULL;
    bank.pcmSize = 0;
    bank.active = false;
    bank.name[0] = '\0';
}

}  // namespace

// ---------------------------------------------------------------------------
// DirectSound
// ---------------------------------------------------------------------------

DirectSound::DirectSound(HWND hwnd)
{
    (void)hwnd;
    memset(this, 0, sizeof(*this));
    // The shared wrappers gate on the DWORD at +0x10 (see SoundApi.cpp); the
    // named field at +0x14 is the original's own initialised flag.
    *(int*)((BYTE*)this + 0x10) = 1;
    m_bInitialized = 1;
}

void DirectSound::compact(void) {}

int DirectSound::GetStatus(int bank)
{
    if (bank <= 0 || bank >= MAX_BANKS) return 0;
    return s_banks[bank].status;
}

void DirectSound::Release(void)
{
    s_suspended = 1;
}

void DirectSound::Reload(void)
{
    s_suspended = 0;
}

void DirectSound::DestroySound(int bank)
{
    if (bank <= 0 || bank >= MAX_BANKS) return;
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    FreeBank(s_banks[bank]);
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
}

int DirectSound::NewDirectSoundBuffer(DWORD* /*wavBlk*/)
{
    return 1;
}

void DirectSound::StopSound(int bank)
{
    if (bank <= 0 || bank >= MAX_BANKS) return;
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    StopBank(s_banks[bank]);
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
}

void DirectSound::PlaySound(int bank, unsigned int slot)
{
    if (bank <= 0 || bank >= MAX_BANKS) return;
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);

    Bank& b = s_banks[bank];
    if (b.active && b.pcm != NULL) {
        b.pos = 0.0;
        b.slot = (int)slot;
        b.loop = (slot != 0);
        b.playing = true;
        b.status = 1;
    }

    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
}

void DirectSound::SetVol(int bank, int vol)
{
    if (bank <= 0 || bank >= MAX_BANKS) return;
    if (vol > -1) vol = -1;
    if (vol < -9999) vol = -9999;
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    s_banks[bank].vol = vol;
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
}

void DirectSound::SetPan(int bank, int pan)
{
    if (bank <= 0 || bank >= MAX_BANKS) return;
    if (pan > 10000) pan = 10000;
    if (pan < -10000) pan = -10000;
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    s_banks[bank].pan = pan;
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
}

int DirectSound::GetVol(int bank)
{
    if (bank <= 0 || bank >= MAX_BANKS) return 0;
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    int vol = s_banks[bank].vol;
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
    return vol;
}

int DirectSound::CreateSound(const char* wavName)
{
    if (wavName == NULL) return 0;

    // Callers build the path with GAME_DATA_ROOT, which is the compile-time
    // root - remap it to the configured one (config.ini [Assets] Path/Version)
    // exactly as the XAudio2 backend does. Without this the banks are looked
    // for under the build directory's own ./assets/USA/ and every load fails,
    // leaving FMV audio (which resolves its path in VideoPlayback) as the only
    // thing audible.
    char resolved[260];
    const char* actualPath = ResolveAssetRoot(wavName, resolved, sizeof(resolved));

    // Same loader as the Windows backend (system/AudioFile): the WAV, or the
    // .ogg the asset migrator wrote in its place.
    char found[260];
    AudioFileData af;
    if (!AudioFile_Find(actualPath, found, sizeof(found)) || !AudioFile_Load(found, &af)) {
        // Bounded: a missing asset tree would otherwise print one line per slot.
        static int s_failLogged = 0;
        if (s_failLogged < 4) {
            fprintf(stderr, "[AUDIO] could not load bank: %s\n", actualPath);
            if (++s_failLogged == 4) {
                fprintf(stderr, "[AUDIO] (further bank load failures suppressed)\n");
            }
        }
        return 0;
    }
    BYTE* wavData = af.buffer;

    if (s_dev != 0) SDL_LockAudioDevice(s_dev);

    int bank = 0;
    for (int i = 1; i < MAX_BANKS; ++i) {
        if (!s_banks[i].active) { bank = i; break; }
    }
    if (bank == 0) {
        if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
        free(wavData);
        return 0;
    }

    Bank& b = s_banks[bank];
    FreeBank(b);
    b.wavData       = wavData;
    b.pcm           = af.pcm;
    b.pcmSize       = af.pcmSize;
    b.sampleRate    = af.sampleRate;
    b.channels      = af.channels;
    b.bitsPerSample = af.bitsPerSample;
    b.vol           = 0;
    b.pan           = 400;      // same default as the Windows backend
    b.slot          = 0;
    b.status        = 0;
    b.playing       = false;
    b.loop          = false;
    b.pos           = 0.0;
    b.active        = true;
    b.ps1           = af.ps1 != 0;
    b.loopBegin     = af.loopBegin;
    b.loopEnd       = af.loopEnd > af.loopBegin ? af.loopEnd : 0;

    const char* slash = strrchr(wavName, '/');
    const char* backslash = strrchr(wavName, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
    strncpy(b.name, slash ? slash + 1 : wavName, sizeof(b.name) - 1);
    b.name[sizeof(b.name) - 1] = '\0';

    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
    return bank;
}

void DirectSound::ErrorRoutine(int code)
{
    (void)code;
}

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

void InitializeSoundSystem(void)
{
    DirectSound* mgr = new DirectSound(NULL);
    g_pDirectSound = mgr;
    g_SoundManager = (void*)mgr;

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[AUDIO] SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return;
    }

    s_outRate = AudioFile_TreeHasPs1Audio() ? 44100 : 22050;

    SDL_AudioSpec want = {};
    want.freq     = OUT_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = MixCallback;

    SDL_AudioSpec have = {};
    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_dev == 0) {
        // No output device (common in a headless WSL session). Banks still
        // load and report status; there is simply nothing to hear.
        fprintf(stderr, "[AUDIO] no output device: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(s_dev, 0);

    const char* driver = SDL_GetCurrentAudioDriver();
    fprintf(stderr, "[AUDIO] SDL device open: %d Hz, %d ch driver=%s\n",
            have.freq, have.channels, driver ? driver : "?");

    // A silent ALSA fallback is easy to mistake for a port bug. WSLg routes
    // sound through PulseAudio; when that bridge is down SDL falls back to
    // ALSA, whose default device is often `type null` (WSL ships that in
    // ~/.asoundrc), so everything "opens" and nothing is audible.
    if (driver != NULL && strcmp(driver, "alsa") == 0 && access("/proc/asound/cards", F_OK) != 0) {
        fprintf(stderr,
                "[AUDIO] WARNING: no sound card found - ALSA is using a null device, "
                "so audio will be silent. Under WSLg this usually means the PulseAudio "
                "bridge died; restart WSL (`wsl --shutdown`) and try again.\n");
    }
}

void CleanupSoundManagerResources(void)
{
    if (s_dev != 0) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
    }
    for (int i = 1; i < MAX_BANKS; ++i) FreeBank(s_banks[i]);
}

// ---------------------------------------------------------------------------
// FMV audio stream - see the note on s_streamPcm above.
// ---------------------------------------------------------------------------

// Sample rate the stream must be decoded at (the device rate).
int plat_audio_stream_rate(void)
{
    return OUT_RATE;
}

// Start playing `pcm` (S16 stereo at OUT_RATE, `totalFrames` frames) from
// `startFrame` up to (not including) `endFrame`. The buffer stays owned by the
// caller and must outlive the stream.
void plat_audio_stream_play(const short* pcm, long totalFrames,
                            long startFrame, long endFrame)
{
#ifdef __EMSCRIPTEN__
    // SDL's main-thread ScriptProcessor can deliver old callbacks after a
    // decode/read stall. Movie PCM uses its own clocked WebAudio source.
    WebMovieAudioPlay(pcm,totalFrames,startFrame,endFrame,OUT_RATE);
#else
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    s_streamPcm = pcm;
    s_streamEnd = (endFrame <= 0 || endFrame > totalFrames) ? totalFrames : endFrame;
    s_streamPos = (startFrame < 0) ? 0 : startFrame;
    s_streamOn  = (pcm != NULL && s_streamPos < s_streamEnd) ? 1 : 0;
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
#endif
}

void plat_audio_stream_stop(void)
{
#ifdef __EMSCRIPTEN__
    WebMovieAudioStop();
#else
    if (s_dev != 0) SDL_LockAudioDevice(s_dev);
    s_streamOn  = 0;
    s_streamPcm = NULL;
    if (s_dev != 0) SDL_UnlockAudioDevice(s_dev);
#endif
}

// Frames played so far, or -1 when the stream is not running. This is the FMV
// video clock: native SDL uses the mixer cursor; web uses the scheduled source
// timeline, which also survives main-thread stalls and suspension.
long plat_audio_stream_pos(void)
{
#ifdef __EMSCRIPTEN__
    return WebMovieAudioPosition();
#else
    if (!s_streamOn) return -1;
    return s_streamPos;
#endif
}
