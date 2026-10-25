// MarniSound.h - DirectSound class (PSYQ-like sound API wrapper)
// Original Ghidra names preserved for each method.
#pragma once
#include "../platform/types.h"
#ifdef _WIN32
// <mmsystem.h> defines PlaySound as a macro (PlaySoundA), which would rename
// this class's PlaySound method at every call site. Include it first and drop
// the macro so the member name is stable in every translation unit.
#include <mmsystem.h>
#undef PlaySound
#undef PlaySoundA
#endif

#pragma pack(push, 1)
class DirectSound {
public:
    // +0x00: vtable pointer
    void** vtable;

    // Padding / internal state
    BYTE  m_pad1[0x10];  // 0x04-0x13
    int   m_bInitialized; // 0x14 - initialized flag (checked at +0x10 from base)

    BYTE  m_pad2[0x1458]; // 0x18-0x146F
    int   m_bankSlots[80]; // 0x1470 - bank allocation slots (80 entries)

    // Raw bank data (banks 1-79, each 0xA2C bytes, plus slack to reach 0x32DDC total)
    BYTE  m_bankData[0x32DDC - 0x15B0];

    DirectSound(HWND hwnd);                                    // 0x0041f3b0
    void  compact(void);                                        // 0x0041e680
    int   GetStatus(int bank);                                  // 0x0041e700
    void  Release(void);                                        // 0x0041e7c0
    void  Reload(void);                                         // 0x0041e880
    void  DestroySound(int bank);                               // 0x0041eab0
    int   NewDirectSoundBuffer(DWORD* wavBlk);                  // 0x0041eb50
    void  StopSound(int bank);                                  // 0x0041edb0
    void  PlaySound(int bank, unsigned int slot);               // 0x0041ee60
    void  SetVol(int bank, int vol);                            // 0x0041efa0
    void  SetPan(int bank, int pan);                            // 0x0041f080
    int   GetVol(int bank);                                     // 0x0041f160
    int   CreateSound(const char* wavName);                     // 0x0041f1c0
    void  ErrorRoutine(int code);                               // 0x0041f570
};
#pragma pack(pop)

static_assert(sizeof(DirectSound) == 0x32DDC, "DirectSound size must be 0x32DDC bytes");

// Global sound manager
extern DirectSound* g_pDirectSound;

// Sound bank management wrappers (use g_pDirectSound internally)
int  loadSndBankFromWav(const char* path);
void destroySndBank(int bank);
void setSndStop(int bank);
void playSnd(int bank, int slot);
void SetSndSlot(int bank, int slot);
void set_volume(int bank, int volume);
void pan_set(int bank, int pan);
int  getSndVol(int bank);
void UpdateSoundFade(int steps);
void InitializeSoundSystem(void);
void CleanupSoundManagerResources(void);

// Async callbacks
void getCurSndStat(void);
void setCurSndPan(void);
void setCurSndVol(void);
void loadWav(void);
void destroyCurSndBank(void);
void StopCurSnd(void);
void playCurSnd(void);
void PlayCurSnd(void);
void getCurSndBankVol(void);
void PauseGameSoundsCallback(void);
void ResumeGameSoundsCallback(void);
void SndCompactCallback(void);   // 0x0041d050
void SndCompactAsync(void);      // 0x0041d070 - per-room-load sound heap compaction

// File path resolution
int  findAndOpenFile(char* path);

// Data tables
extern const char** g_SoundBanksTable[16];
