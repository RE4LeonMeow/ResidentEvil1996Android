// MarniSound.cpp - DirectSound class implementation
// All functions implemented from Ghidra decompilations.
#include "MarniSound.h"
#include "../Globals.h"
#include "../system/AssetPath.h"
#include "../system/AudioFile.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <xaudio2.h>        // NOLINT: XAudio2 (DirectSound replacement)
#include <memory>            // NOLINT: std::unique_ptr for XAudio2 cleanup

// XAudio2 engine globals (PSYQ DirectSound stub replacement)
static IXAudio2*         g_pXAudio2 = NULL;
static IXAudio2MasteringVoice* g_pMasterVoice = NULL;
static IXAudio2SourceVoice*    g_BankVoices[81];  // bank handles 1-80
// Port-added: a bank's loop region, in sample frames (length 0 = loop the whole
// buffer, as the original always does). Only set for the asset migrator's PS1
// audio files - see AudioFile.h.
static UINT32                  g_BankLoopBegin[81];
static UINT32                  g_BankLoopLength[81];

// ============================================================================
// SFX filename sub-tables
// ============================================================================
#define SFX_SUBTABLE_SIZE 16


// ------------------------------------------------------------------------
// Bank offset helpers
// ------------------------------------------------------------------------
#define BANK_BASE(bank)       ((BYTE*)this + (int)(bank) * 0xA2C)
#define BANK_WAV_PTR(bank)    (*(BYTE**)(BANK_BASE(bank) + 0x1C))
#define BANK_DATA_SZ(bank)    (*(DWORD*)(BANK_BASE(bank) + 0x20))
#define BANK_SR(bank)         (*(DWORD*)(BANK_BASE(bank) + 0x24))
#define BANK_CH(bank)         (*(WORD*)(BANK_BASE(bank) + 0x28))
#define BANK_BPS(bank)        (*(WORD*)(BANK_BASE(bank) + 0x2A))
#define BANK_PCM_PTR(bank)    (*(BYTE**)(BANK_BASE(bank) + 0x2C))
#define BANK_PCM_SZ(bank)     (*(DWORD*)(BANK_BASE(bank) + 0x30))
#define BANK_PLAYSR(bank)     (*(int*)(BANK_BASE(bank) + 0x920))
#define BANK_PAN(bank)        (*(int*)(BANK_BASE(bank) + 0x924))
#define BANK_VOL(bank)        (*(int*)(BANK_BASE(bank) + 0x928))
#define BANK_SLOT(bank)       (*(int*)(BANK_BASE(bank) + 0x92C))
#define BANK_STATUS(bank)     (*(int*)(BANK_BASE(bank) + 0x930))
#define BANK_DSBUF(bank)      (*(int**)(BANK_BASE(bank) + 0x93C))
#define BANK_NAME(bank)       ((char*)(BANK_BASE(bank) + 0x940))
#define BANK_ACTIVE(bank)     (*(int*)(BANK_BASE(bank) + 0xA44))

// Internal sentinel to identify stubbed DS buffers (no real COM buffer created)
#define DS_STUB_BUF ((int*)0x00000001)

// ============================================================================
// DirectSound volume -> XAudio2 amplitude
//
// Every volume in this game is an IDirectSoundBuffer::SetVolume argument:
// attenuation in HUNDREDTHS OF A DECIBEL (millibels), 0 = full scale,
// DSBVOLUME_MIN = -10000 = silence. IXAudio2Voice::SetVolume takes a LINEAR
// amplitude multiplier instead, so the conversion is
//
//     amplitude = 10 ^ (millibels / 2000)      (mB -> dB -> amplitude)
//
// This used to be `1.0f - (-vol / 10000.0f)`, i.e. linear IN THE MILLIBEL
// NUMBER, which is a completely different curve and squashes the whole useful
// range into the top of the scale. The values the game actually uses land
// between roughly -800 and -2300 mB, and the old map turned that entire span
// into 0.92..0.77 amplitude - about half a decibel from end to end, when the
// original spans some 15 dB.
//
// Everything that varies volume was affected: 3D distance attenuation
// (CalcPanVolume), the SCD per-camera BGM levels (opcode 0x2F) and the volume
// ramps (opcode 0x43) all became near-inaudible nudges. The visible symptom
// that turned this up was the stage 2 waterfall: rooms 2 and 4 share BGM group
// 0x24, so channel 1 (Se_42) survives the transition and each room's per-frame
// script re-levels it per camera - courtyard about -1070 mB, next room about
// -1376..-1646 mB. Under the old curve that was 0.89 vs 0.84 and the waterfall
// sounded exactly as loud from the next room.
// ============================================================================
static float DsVolumeToAmplitude(int millibels)
{
    if (millibels <= -10000) return 0.0f;
    if (millibels >= 0)      return 1.0f;
    return powf(10.0f, (float)millibels / 2000.0f);
}

// ------------------------------------------------------------------------
// DirectSound constructor (0x0041f3b0)
// ------------------------------------------------------------------------
DirectSound::DirectSound(HWND hwnd)
{
    memset(this, 0, 0x32DDC);
    *(int**)((BYTE*)this + 0x10) = (int*)1;  // m_bInitialized = 1
    *(int*)((BYTE*)this + 0x18) = (int)hwnd;  // hwnd
}

// ============================================================================
// DirectSound::compact (0x0041e680) — compact/restore buffers
// ============================================================================
void DirectSound::compact(void)
{
    int* dev = *(int**)this;
    if (dev == NULL) return;
    int iVar1 = ((int(*)(int*, int, int))dev[6])(dev, *(int*)((BYTE*)this + 0x18), 3);
    if (iVar1 != 0) {
        printf("DirectSound::compact: set_cooperative_level error\n");
        return;
    }
    iVar1 = ((int(*)(int*))dev[7])(dev);
    if (iVar1 != 0) {
        printf("DirectSound::compact: compact error\n");
        return;
    }
    iVar1 = ((int(*)(int*, int, int))dev[6])(dev, *(int*)((BYTE*)this + 0x18), 2);
    if (iVar1 != 0) {
        printf("DirectSound::compact: restore_cooperative_level error\n");
    }
}

// ============================================================================
// DirectSound::GetStatus (0x0041e700)
// ============================================================================
int DirectSound::GetStatus(int bank)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return 0;
    if (BANK_ACTIVE(bank) == 0) return 0;
    int* buf = BANK_DSBUF(bank);
    if (buf == NULL) return 0;

    if (buf == DS_STUB_BUF) {
        if (g_BankVoices[bank] != NULL) {
            XAUDIO2_VOICE_STATE state = {};
            g_BankVoices[bank]->GetState(&state);
            if (state.BuffersQueued > 0) {
                BANK_STATUS(bank) = 1;
                return 1;
            }
        }
        BANK_STATUS(bank) = 0;
        return 0;
    }

    DWORD status = 0;
    int hr = ((int(*)(int*, DWORD*))buf[9])(buf, &status);
    *(int*)((BYTE*)this + 4) = hr;
    if (hr != 0) return 0;
    if (status != 0 && BANK_STATUS(bank) == 1) return 1;
    return 0;
}

// ============================================================================
// DirectSound::Release (0x0041e7c0)
// ============================================================================
void DirectSound::Release(void)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) {
        printf("DirectSound::Release: not initialized\n");
        return;
    }
    if (*(int*)((BYTE*)this + 0x14) != 0) {
        printf("DirectSound::Release: already released\n");
        return;
    }
    *(int*)((BYTE*)this + 0x14) = 1;
    *(int*)((BYTE*)this + 0x10) = 0;

    // Stop primary buffer
    int* prim = *(int**)((BYTE*)this + 0x93C);
    if (prim != NULL) {
        ((void(*)(int*))prim[18])(prim);
        ((void(*)(int*))prim[2])(prim);
        *(int**)((BYTE*)this + 0x93C) = NULL;
    }

    // Destroy all bank buffers
    for (int bank = 1; bank <= 80; bank++) {
        int* buf = BANK_DSBUF(bank);
        if (BANK_ACTIVE(bank) != 0 && buf != NULL) {
            ((void(*)(int*))buf[2])(buf);
            BANK_DSBUF(bank) = NULL;
        }
    }

    // Release DirectSound device
    int* dev = *(int**)this;
    if (dev != NULL) {
        ((void(*)(int*))dev[2])(dev);
        *(int**)this = NULL;
    }
}

// ============================================================================
// DirectSound::Reload (0x0041e880) — re-init after Release
// ============================================================================
void DirectSound::Reload(void)
{
    if (*(int*)((BYTE*)this + 0x14) == 0) {
        printf("DirectSound::Reload: not released\n");
        return;
    }

    *(int*)((BYTE*)this + 0x10) = 1;

    // Re-activate all previously loaded banks
    for (int bank = 1; bank <= 80; bank++) {
        if (BANK_ACTIVE(bank) == 0) continue;
        BANK_ACTIVE(bank) = 1;
    }

    *(int*)((BYTE*)this + 0x14) = 0;
}

// ============================================================================
// DirectSound::DestroySound (0x0041eab0)
// ============================================================================
void DirectSound::DestroySound(int bank)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return;
    if (BANK_ACTIVE(bank) == 0) return;

    int* buf = BANK_DSBUF(bank);
    if (buf != NULL && buf != DS_STUB_BUF) {
        ((void(*)(int*))buf[2])(buf);
    }

    // Release XAudio2 voice if any
    if (g_BankVoices[bank] != NULL) {
        g_BankVoices[bank]->Stop(0);
        g_BankVoices[bank]->DestroyVoice();
        g_BankVoices[bank] = NULL;
    }

    BANK_DSBUF(bank) = NULL;

    BANK_ACTIVE(bank) = 0;
    BANK_STATUS(bank) = 0;
    m_bankSlots[bank - 1] = 0;

    BYTE* wavData = BANK_WAV_PTR(bank);
    if (wavData != NULL) {
        free(wavData);
        BANK_WAV_PTR(bank) = NULL;
    }
}

// ============================================================================
// DirectSound::NewDirectSoundBuffer (0x0041eb50)
// Creates/registers a DirectSound secondary buffer slot for the given bank.
// The actual DirectX 5.0 COM buffer creation is stubbed; we set a sentinel
// so that PlaySound/StopSound/SetVol/SetPan can update bank state correctly.
// ============================================================================
int DirectSound::NewDirectSoundBuffer(DWORD* wavBlk)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) {
        OutputDebugStringA("[DS] NewDirectSoundBuffer: not initialized\n");
        return 0;
    }

    char dbg[256];
    // sprintf(dbg, "[DEBUG] NewDirectSoundBuffer: wavBlk=%p (bank offset=0x%X)\n",
    //         wavBlk, (DWORD)((BYTE*)wavBlk - (BYTE*)this));
    // OutputDebugStringA(dbg);

    // Set a flag at bank+0x91C (original code writes to wavBlk[0x247])
    wavBlk[0x247] = 1;
    if ((wavBlk[1] & 4) == 0) wavBlk[0x247] = 0;

    // Store stub sentinel so PlaySound etc. know this bank has a "buffer"
    // In the stub mode, methods update bank state fields without real COM.
    int bank = ((BYTE*)wavBlk - (BYTE*)this) / 0xA2C;
    BANK_DSBUF(bank) = DS_STUB_BUF;

    // sprintf(dbg, "[DEBUG] NewDirectSoundBuffer OK: bank=%d, DS_BUF=%p\n", bank, DS_STUB_BUF);
    // OutputDebugStringA(dbg);

    return 1;
}

// ============================================================================
// DirectSound::StopSound (0x0041edb0)
// ============================================================================
void DirectSound::StopSound(int bank)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return;
    if (BANK_ACTIVE(bank) == 0) return;
    int* buf = BANK_DSBUF(bank);
    if (buf == NULL) return;

    if (buf == DS_STUB_BUF) {
        IXAudio2SourceVoice* voice = g_BankVoices[bank];
        if (voice != NULL) {
            voice->Stop(0);
            voice->FlushSourceBuffers();
        }
        BANK_STATUS(bank) = 0;
        return;
    }

    int hr = ((int(*)(int*))buf[18])(buf);
    *(int*)((BYTE*)this + 4) = hr;
    if (hr != 0) {
        ErrorRoutine(*(int*)((BYTE*)this + 4));
    } else {
        BANK_STATUS(bank) = 0;
    }
}

// ============================================================================
// DirectSound::PlaySound (0x0041ee60)
// ============================================================================
void DirectSound::PlaySound(int bank, unsigned int slot)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return;
    if (BANK_ACTIVE(bank) == 0) return;

    BANK_SLOT(bank) = (int)slot;

    int* buf = BANK_DSBUF(bank);
    if (buf == NULL) return;

    if (buf == DS_STUB_BUF) {
        // XAudio2 playback — reuse pre-created source voice from CreateSound
        IXAudio2SourceVoice* voice = g_BankVoices[bank];
        if (voice == NULL || g_pXAudio2 == NULL || g_pMasterVoice == NULL) {
            BANK_STATUS(bank) = 0;
            return;
        }

        // Stop and flush any existing playback on the reusable voice
        voice->Stop(0);
        voice->FlushSourceBuffers();

        // Use PCM data pointer stored during CreateSound (avoids re-parsing WAV)
        BYTE* pcmData = BANK_PCM_PTR(bank);
        DWORD pcmSize = BANK_PCM_SZ(bank);

        XAUDIO2_BUFFER xa2buf = {};
        xa2buf.Flags = 0;
        xa2buf.AudioBytes = pcmSize;
        xa2buf.pAudioData = pcmData;
        xa2buf.PlayBegin = 0;
        xa2buf.PlayLength = 0;
        xa2buf.LoopBegin = 0;
        xa2buf.LoopLength = 0;
        // LoopBegin/LoopLength stay 0 for every PC file: DSBPLAY_LOOPING, which
        // this replaces, always repeats the WHOLE buffer, and the PC build has
        // no loop-region mechanism anywhere. See the note in SoundSystem.cpp's
        // bgm_load_and_start. The one exception is port-added: a PS1 audio
        // file from the asset migrator loops only its loop region, the intro
        // once, as the PS1 sequencer does.
        if (slot != 0 && g_BankLoopLength[bank] != 0) {
            xa2buf.LoopBegin = g_BankLoopBegin[bank];
            xa2buf.LoopLength = g_BankLoopLength[bank];
        }
        xa2buf.LoopCount = (slot != 0) ? XAUDIO2_LOOP_INFINITE : 0;

        HRESULT hr = voice->SubmitSourceBuffer(&xa2buf, NULL);
        if (FAILED(hr)) {
            BANK_STATUS(bank) = 0;
            return;
        }

        hr = voice->Start(0);
        if (FAILED(hr)) {
            BANK_STATUS(bank) = 0;
            return;
        }

        // Apply current volume
        voice->SetVolume(DsVolumeToAmplitude(BANK_VOL(bank)));

        BANK_STATUS(bank) = 1;
        return;
    }

    // Real COM path (not yet operational)
    DWORD status = 0;
    int hr = ((int(*)(int*, DWORD*))buf[9])(buf, &status);
    *(int*)((BYTE*)this + 4) = hr;
    if (hr != 0) {
        OutputDebugStringA("[DS] PlaySound: GetStatus failed\n");
        return;
    }

    if ((status & 1) != 0 && bank != 0) {
        hr = ((int(*)(int*, int))buf[13])(buf, 0);
        *(int*)((BYTE*)this + 4) = hr;
        if (hr != 0) {
            OutputDebugStringA("[DS] PlaySound: SetCurrentPosition failed\n");
            return;
        }
    }

    hr = ((int(*)(int*, int, int, int))buf[12])(buf, 0, 0, slot != 0);
    *(int*)((BYTE*)this + 4) = hr;
    if (hr != 0) {
        OutputDebugStringA("[DS] PlaySound: Play failed\n");
        return;
    }
    BANK_STATUS(bank) = 1;
}

// ============================================================================
// DirectSound::SetVol (0x0041efa0)
// ============================================================================
void DirectSound::SetVol(int bank, int vol)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return;
    if (vol > 0 || vol < -10000) return;
    if (BANK_ACTIVE(bank) == 0) return;
    int* buf = BANK_DSBUF(bank);
    if (buf == NULL) return;

    if (buf == DS_STUB_BUF) {
        IXAudio2SourceVoice* voice = g_BankVoices[bank];
        if (voice != NULL) {
            voice->SetVolume(DsVolumeToAmplitude(vol));
        }
        BANK_VOL(bank) = vol;
        return;
    }

    int hr = ((int(*)(int*, int))buf[15])(buf, vol);
    *(int*)((BYTE*)this + 4) = hr;
    if (hr != 0) {
        ErrorRoutine(*(int*)((BYTE*)this + 4));
    } else {
        BANK_VOL(bank) = vol;
    }
}

// ============================================================================
// DirectSound::SetPan (0x0041f080)
// ============================================================================
void DirectSound::SetPan(int bank, int pan)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return;
    if (pan > 10000 || pan < -10000) return;
    if (BANK_ACTIVE(bank) == 0) return;
    int* buf = BANK_DSBUF(bank);
    if (buf == NULL) return;

    if (buf == DS_STUB_BUF) {
        BANK_PAN(bank) = pan;
        return;
    }

    int hr = ((int(*)(int*, int))buf[16])(buf, pan);
    *(int*)((BYTE*)this + 4) = hr;
    if (hr != 0) {
        ErrorRoutine(*(int*)((BYTE*)this + 4));
    } else {
        BANK_PAN(bank) = pan;
    }
}

// ============================================================================
// DirectSound::GetVol (0x0041f160)
// ============================================================================
int DirectSound::GetVol(int bank)
{
    if (*(int*)((BYTE*)this + 0x10) == 0) return 0;
    if (BANK_ACTIVE(bank) == 0) return 0;
    return BANK_VOL(bank);
}

// ============================================================================
// DirectSound::CreateSound (0x0041f1c0)
// ============================================================================
int DirectSound::CreateSound(const char* wavName)
{
    if (wavName == NULL || *(int*)((BYTE*)this + 0x10) == 0) {
        OutputDebugStringA("[DEBUG] CreateSound: not initialized or null name\n");
        return 0;
    }

    int bank;
    for (bank = 1; bank <= 80; bank++) {
        if (m_bankSlots[bank - 1] == 0) break;
    }
    if (bank > 80) {
        OutputDebugStringA("[DEBUG] CreateSound: no free bank slots\n");
        return 0;
    }

    char dbg[256];

    // Callers build the path with GAME_DATA_ROOT. Remap its root to the config-
    // selected version (config.ini [Assets] Version); a no-op unless JPN is active.
    char resolved[260];
    const char* actualPath = ResolveAssetRoot(wavName, resolved, sizeof(resolved));

    // Port-added: AudioFile opens the WAV - or, where the asset migrator wrote
    // the PS1 audio as Ogg Vorbis, the .ogg beside it - and hands back PCM.
    // Its buffer takes the place of the original's whole-file malloc and is
    // released the same way (free(BANK_WAV_PTR) in DestroySound).
    char found[260];
    AudioFileData af;
    if (!AudioFile_Find(actualPath, found, sizeof(found)) || !AudioFile_Load(found, &af)) {
        _snprintf(dbg, sizeof(dbg), "[DEBUG]   FAILED to load %s\n", actualPath);
        dbg[sizeof(dbg) - 1] = '\0';
        OutputDebugStringA(dbg);
        return 0;
    }
    const WORD channels = (WORD)af.channels;
    const DWORD sampleRate = (DWORD)af.sampleRate;
    const WORD bitsPerSample = (WORD)af.bitsPerSample;

    BANK_WAV_PTR(bank) = af.buffer;
    BANK_DATA_SZ(bank) = af.pcmSize;
    BANK_PCM_PTR(bank) = af.pcm;
    BANK_PCM_SZ(bank) = af.pcmSize;
    BANK_SR(bank) = sampleRate;
    BANK_CH(bank) = channels;
    BANK_BPS(bank) = bitsPerSample;
    BANK_PLAYSR(bank) = (int)sampleRate;
    BANK_PAN(bank) = 400;
    BANK_VOL(bank) = 0;
    BANK_STATUS(bank) = 0;
    BANK_ACTIVE(bank) = 1;

    // A PS1 file's loop region (AudioFile reports one only for files the
    // migrator marked; the PC tree's BGM_33.WAV has a smpl chunk the original
    // ignored).
    g_BankLoopBegin[bank] = af.loopBegin;
    g_BankLoopLength[bank] = af.loopEnd > af.loopBegin ? af.loopEnd - af.loopBegin : 0;

    // Create the DirectSound buffer for this bank
    NewDirectSoundBuffer((DWORD*)BANK_BASE(bank));

    // Pre-create XAudio2 source voice now (avoids expensive CreateSourceVoice during playback)
    if (g_pXAudio2 != NULL) {
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = channels;
        wfx.nSamplesPerSec = sampleRate;
        wfx.wBitsPerSample = bitsPerSample;
        wfx.nBlockAlign = (channels * bitsPerSample) / 8;
        wfx.nAvgBytesPerSec = sampleRate * wfx.nBlockAlign;
        wfx.cbSize = 0;

        HRESULT hrVoice = g_pXAudio2->CreateSourceVoice(&g_BankVoices[bank], &wfx, 0,
                                                         XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
        if (FAILED(hrVoice)) {
            sprintf(dbg, "[XA2] CreateSound: CreateSourceVoice failed bank=%d hr=0x%08X\n", bank, hrVoice);
            OutputDebugStringA(dbg);
            g_BankVoices[bank] = NULL;
        }
    }

    char* nameDest = BANK_NAME(bank);
    size_t nameLen = strlen(wavName);
    if (nameLen > 255) nameLen = 255;
    memcpy(nameDest, wavName, nameLen);
    nameDest[nameLen] = '\0';

    m_bankSlots[bank - 1] = (int)BANK_BASE(bank);

    return bank;
}

// ============================================================================
// DirectSound::ErrorRoutine (0x0041f570)
// ============================================================================
void DirectSound::ErrorRoutine(int code)
{
    unsigned int c = (unsigned int)code;
    if (c < 0x80004002) {
        if (c == 0x80004001) { printf("DS: not implemented\n"); return; }
        if (c == 0) { printf("DS: OK\n"); return; }
        return;
    }
    if (c < 0x80040111) {
        if (c == 0x80040110) { printf("DS: buffer lost\n"); return; }
        if (c == 0x80004005) { printf("DS: E_FAIL\n"); return; }
        return;
    }
    if (c > 0x80070057) {
        switch (c) {
        case 0x8878000a: printf("DS: buffer too small\n"); return;
        case 0x8878001e: printf("DS: bad format\n"); return;
        case 0x88780032: printf("DS: buffer lost\n"); return;
        case 0x88780046: printf("DS: format not supported\n"); return;
        case 0x88780064: printf("DS: invalid call\n"); return;
        case 0x88780078: printf("DS: priority level high\n"); return;
        case 0x88780082: printf("DS: buffer already locked\n"); return;
        case 0x88780096: printf("DS: no hardware\n"); return;
        }
        return;
    }
    if (c == 0x80070057) { printf("DS: invalid parameter\n"); return; }
    if (c == 0x8007000e) { printf("DS: out of memory\n"); return; }
}

void InitializeSoundSystem(void)
{
    OutputDebugStringA("[DS] InitializeSoundSystem: creating DirectSound instance\n");
    DirectSound* mgr = new DirectSound(g_MainWindowHandle);
    g_pDirectSound = mgr;
    g_SoundManager = (void*)mgr;

    // Initialize XAudio2 as DirectSound replacement
    char dbg[128];
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
        OutputDebugStringA("[XA2] CoInitializeEx failed\n");
    }

    hr = XAudio2Create(&g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        sprintf(dbg, "[XA2] XAudio2Create failed: 0x%08X\n", hr);
        OutputDebugStringA(dbg);
        g_pXAudio2 = NULL;
    } else {
        // 22050 Hz, the rate every PC WAV is at. A tree the asset migrator
        // gave the PS1 audio (37.8 / 44.1 kHz) mixes at 44.1 kHz instead, or
        // the extra bandwidth would be thrown away.
        hr = g_pXAudio2->CreateMasteringVoice(&g_pMasterVoice, 2,
                                              AudioFile_TreeHasPs1Audio() ? 44100 : 22050,
                                              0, NULL, NULL);
        if (FAILED(hr)) {
            sprintf(dbg, "[XA2] CreateMasteringVoice failed: 0x%08X\n", hr);
            OutputDebugStringA(dbg);
            g_pMasterVoice = NULL;
        } else {
            OutputDebugStringA("[XA2] XAudio2 engine ready\n");
        }
    }

    memset(g_BankVoices, 0, sizeof(g_BankVoices));
    OutputDebugStringA("[DS] InitializeSoundSystem OK\n");
}

void CleanupSoundManagerResources(void)
{
    OutputDebugStringA("[DS] CleanupSoundManagerResources\n");
    if (g_pDirectSound == NULL) return;
    for (int i = 1; i <= 79; i++) {
        g_pDirectSound->DestroySound(i);
    }

    // Shutdown XAudio2
    if (g_BankVoices) {
        for (int i = 1; i <= 80; i++) {
            if (g_BankVoices[i] != NULL) {
                g_BankVoices[i]->Stop(0);
                g_BankVoices[i]->DestroyVoice();
                g_BankVoices[i] = NULL;
            }
        }
    }
    if (g_pMasterVoice != NULL) {
        g_pMasterVoice->DestroyVoice();
        g_pMasterVoice = NULL;
    }
    if (g_pXAudio2 != NULL) {
        g_pXAudio2->Release();
        g_pXAudio2 = NULL;
    }
    CoUninitialize();
}

void PauseGameSoundsCallback(void) { PauseSounds(); }
void ResumeGameSoundsCallback(void) { ResumePausedSounds(); }

