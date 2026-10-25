// SoundApi.cpp - platform-neutral DirectSound API wrappers.
//
// These lived in marni/MarniSound.cpp (the XAudio2 backend) until the Linux
// port needed the same wrappers over a different backend. They only call the
// DirectSound class API and touch game globals, so they are backend-agnostic:
// the backend is whichever DirectSound implementation is linked -
// marni/MarniSound.cpp (XAudio2) or platform/linux/audio.cpp (SDL2).
//
// Moved verbatim in Phase 6 of docs/LINUX_PORT.md; behaviour unchanged.
#include "../Globals.h"
#include "../platform/platform.h"
#include "../system/AssetPath.h"
#include "../system/AudioFile.h"
#include "marni/MarniSound.h"

// ---------------------------------------------------------------------
// SFX filename sub-tables (game data, shared by both backends).
// Moved here from marni/MarniSound.cpp in Phase 6.
// ---------------------------------------------------------------------
#define SFX_SUBTABLE_SIZE 16

static const char* g_st0[SFX_SUBTABLE_SIZE] = {
    "Knife01","Knife02","Knife03",NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
};
static const char* g_st2[SFX_SUBTABLE_SIZE] = {
    NULL,   NULL,   NULL,   NULL,
    NULL,   "Gun04",NULL,   "Gun01",
    NULL,   "Gun02","Gun03",NULL,
    NULL,   NULL,   NULL,   NULL,
};
static const char* g_st3[SFX_SUBTABLE_SIZE] = {
    NULL,   NULL,   NULL,   NULL,
    NULL,   NULL,   NULL,   "Shot01",
    NULL,   "Shot04","Shot02","Shot03",
    NULL,   NULL,   NULL,   NULL,
};
static const char* g_st4[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     "Magnum01",
    NULL,     "Magnum02","Magnum03",NULL,
    NULL,     NULL,     NULL,     NULL,
};
static const char* g_st8[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     NULL,
    NULL,     "Gracid01",NULL,   "Gracid02",
    NULL,     "Gracid04",NULL,   NULL,
    NULL,     "Gracid03",NULL,   NULL,
};
static const char* g_st7[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     NULL,
    NULL,     "Grbomb01",NULL,   "Grbomb02",
    NULL,     "Grbomb04",NULL,   NULL,
    "Grbomb03",NULL,    NULL,    NULL,
};
static const char* g_st9[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     NULL,
    NULL,     "Grfire01",NULL,   "Grfire02",
    NULL,     "Grfire04",NULL,   NULL,
    NULL,     NULL,     "Grfire03",NULL,
};
static const char* g_st10[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     "Rocket01",
    NULL,     "Rocket02",NULL,    NULL,
    NULL,     NULL,     NULL,     "Rocket03",
};
static const char* g_st11[SFX_SUBTABLE_SIZE] = {
    "Bio01",  NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     "cancel", "type01", "type02",
};
static const char* g_st12[SFX_SUBTABLE_SIZE] = {
    "Evil01", NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     "cancel", "type01", "type02",
};
static const char* g_st13[SFX_SUBTABLE_SIZE] = {
    "Select06","Select05",NULL,   NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
};
static const char* g_st14[SFX_SUBTABLE_SIZE] = {
    "Ending07",NULL,   "Ending06",NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
    NULL,     "cancel", "type01", "type02",
};
static const char* g_st6[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     "Flame01",
    "Flame02",NULL,     NULL,     NULL,
    NULL,     "Flame03",NULL,     NULL,
    NULL,     NULL,     NULL,     NULL,
};
static const char* g_st15[SFX_SUBTABLE_SIZE] = {
    NULL,     NULL,     NULL,     NULL,
    NULL,     NULL,     NULL,     "Win95_mg",
    NULL,     NULL,     "A_mcn03",NULL,
    NULL,     NULL,     NULL,     NULL,
};

const char** g_SoundBanksTable[SFX_SUBTABLE_SIZE] = {
    g_st0,  g_st0,  g_st2,  g_st3,  g_st4,  g_st4,  g_st6,  g_st7,
    g_st8,  g_st9,  g_st10, g_st11, g_st12, g_st13, g_st14, g_st15,
};


// ============================================================================
// Wrappers using g_pDirectSound
// ============================================================================
int loadSndBankFromWav(const char* path)
{
    // Callers build the path with GAME_DATA_ROOT, so it is already correct for the
    // build configuration - nothing to rewrite here.
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        // Call CreateSound synchronously — the original ExecAsync was an empty
        // stub (FUN_00420290). The async wrapper would overwrite the single
        // callback slot on each call, so batch loading via LoadSoundBank
        // would lose all but the last callback.
        g_sndload_bank_index = g_pDirectSound->CreateSound(path);
    } else {
        OutputDebugStringA("[DEBUG] loadSndBankFromWav: g_pDirectSound NOT READY\n");
        g_sndload_bank_index = 0;
    }
    return g_sndload_bank_index;
}

void destroySndBank(int bank)
{
    if (g_pDirectSound != NULL && bank != 0) {
        g_pDirectSound->DestroySound(bank);
    }
}

void setSndStop(int bank)
{
    if (g_pDirectSound != NULL && bank != 0) {
        g_pDirectSound->StopSound(bank);
    }
}

void playSnd(int bank, int slot)
{
    g_CurSlot = (short)slot;
    g_CurBank = bank;
    if (g_pDirectSound != NULL && bank != 0) {
        g_pDirectSound->PlaySound(bank, (unsigned int)slot);
    }
}

void SetSndSlot(int bank, int slot)
{
    g_CurSlot = (short)slot;
    g_CurBank = bank;
    if (g_pDirectSound != NULL && bank != 0) {
        g_pDirectSound->PlaySound(bank, (unsigned int)slot);
    }
}

void set_volume(int bank, int volume)
{
    if (g_pDirectSound == NULL || *(int*)((BYTE*)g_pDirectSound + 0x10) == 0) return;
    if (bank == 0) return;
    int vol = volume;
    if (vol > -1) vol = -1;
    if (vol < -9999) vol = -9999;
    g_pDirectSound->SetVol(bank, vol);
}

void pan_set(int bank, int pan)
{
    if (g_pDirectSound == NULL || *(int*)((BYTE*)g_pDirectSound + 0x10) == 0) return;
    if (bank == 0) return;
    int p = pan;
    if (p > 10000) p = 10000;
    if (p < -10000) p = -10000;
    g_pDirectSound->SetPan(bank, p);
}

int getSndVol(int bank)
{
    if (g_pDirectSound == NULL || *(int*)((BYTE*)g_pDirectSound + 0x10) == 0) return 0;
    if (bank == 0) return 0;
    return g_pDirectSound->GetVol(bank);
}

static void apply_vol_delta(int* bankPtr, int delta)
{
    if (bankPtr == NULL || *bankPtr == 0) return;
    int vol = getSndVol(*bankPtr);
    if (vol == 2000) vol = -9999;
    vol += delta;
    if (vol > -1) vol = -1;
    if (vol < -9999) vol = -9999;
    set_volume(*bankPtr, vol);
}

// Record counts for UpdateSoundFade's four bank sweeps, read off the original's
// loop bounds at 0x004802f5-0x00480423 (base -> exclusive end, stride 8):
//   g_SfxBanks          0x00ac9b80 -> 0x00ac9c00   16 records (32 ints)
//   g_RoomSfxBanks      0x00ac9910 -> 0x00ac9920    2 records ( 4 ints)
//   g_CharacterSfxBanks 0x00ac9950 -> 0x00ac9998    9 records (18 ints)
//   g_emSndBanks        0x00ac99f0 -> 0x00ac9b70   48 records (96 ints)
// The 9 is deliberate: load_character_sfx fills 16 records, and the original
// only ever fades the first 9 (see the note on its loop bound in SoundSystem.cpp).
void UpdateSoundFade(int steps)
{
    if (g_BgmSoundBank != 0) apply_vol_delta(&g_BgmSoundBank, steps);
    for (int* p = g_SfxBanks; p < g_SfxBanks + 32; p += 2) apply_vol_delta(p, steps);
    // Was `p < &g_SndRampDirection`. In the original that address happens to be
    // the end of this array; in our .bss it is five globals later, so the sweep
    // ran off g_RoomSfxBanks through g_CharacterSfxBanks, g_emSndBanks and the
    // scalars behind them, feeding whatever it found to DirectSound::GetVol as a
    // bank index - GetVol's BANK_BASE is `this + bank * 0xA2C`, so a stray -1
    // (g_SfxVolume / g_EnemySndVolume both idle at -1) is an instant access
    // violation. See docs/MEMORY_LAYOUT.md: never bound a loop with the address
    // of a neighbouring global.
    for (int* p = g_RoomSfxBanks; p < g_RoomSfxBanks + 4; p += 2) apply_vol_delta(p, steps);
    for (int* p = g_CharacterSfxBanks; p < g_CharacterSfxBanks + 18; p += 2) apply_vol_delta(p, steps);
    for (int* p = g_emSndBanks; p < g_emSndBanks + 96; p += 2) apply_vol_delta(p, steps);
    // Original bound is 0x00ac99e8, i.e. 3 records - not 24 (the old `+ 48` on an
    // int* over-ran the array by 21 entries).
    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0 && g_SndFadeStepTbl[i] > 0) {
            apply_vol_delta(&g_SndBank[i].handle, steps);
            g_SndFadeStepTbl[i]--;
        }
    }

    // Original (0x004802a0): once ALL three step counts run out, move the fade
    // into its negative ramp. The byte store 0xE2 is -30 as a signed byte;
    // UpdateSoundFadeState then counts -30 -> -29 -> ... -> -1 -> 0, at which
    // point the fade is done. Without this, g_SndFadeType stays at 0x2B
    // forever and die_state's `while (g_SndFadeType != 0)` hangs on a black
    // screen after the game-over fade-out.
    if ((g_SndFadeStepTbl[0] < 1) && (g_SndFadeStepTbl[1] < 1) && (g_SndFadeStepTbl[2] < 1)) {
        g_SndFadeType = (signed char)0xE2;
    }
}

// ============================================================================
// Async callbacks
// ============================================================================
void getCurSndStat(void)
{
    g_setVolResult = 1;
}

void setCurSndPan(void)
{
    g_SndPanSet_result = 0;
}

void setCurSndVol(void)
{
    g_setVolResult = 0;
}

void loadWav(void)
{
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_sndload_bank_index = g_pDirectSound->CreateSound(g_wavName);
    }
}

void destroyCurSndBank(void)
{
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_pDirectSound->DestroySound(g_CurBank);
    }
}

void StopCurSnd(void)
{
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_pDirectSound->StopSound(g_CurBank);
    }
}

void playCurSnd(void)
{
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_pDirectSound->PlaySound(g_CurBank, (unsigned int)g_CurSlot);
    }
}

void PlayCurSnd(void)
{
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_pDirectSound->PlaySound(g_CurBank, (unsigned int)g_CurSlot);
    }
}

void getCurSndBankVol(void)
{
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_SoundPanVol = g_pDirectSound->GetVol(g_CurBank);
    } else {
        g_SoundPanVol = 0;
    }
}

// ============================================================================
// SndCompactCallback (0x0041d050) / SndCompactAsync (0x0041d070)
//
// room_transition_load calls SndCompactAsync once per room load, right after the
// BGM change and the door SFX. It is the DirectSound heap defragmenter: the
// callback checks the manager's initialised flag at +0x10 and, if the device is
// up, runs DirectSound::compact - SetCooperativeLevel(EXCLUSIVE),
// IDirectSound::Compact(), SetCooperativeLevel(PRIORITY). Retail needed it
// because a room's worth of freed hardware sound buffers left the on-card
// mixer heap fragmented.
//
// In this port it is inert by construction, and deliberately so: neither
// backend gets a real DirectSound device pointer (the constructor memsets the
// whole block, so compact's `dev == NULL` guard returns immediately). XAudio2
// owns its own voice memory and has nothing corresponding to compact; the SDL2
// backend keeps its banks in plain host memory.
//
// Wired up rather than stubbed so the call site reads like the original.
// ============================================================================
void SndCompactCallback(void)
{
    // 0x0041d050 - the +0x10 initialised flag, not the vtable slot.
    if (g_pDirectSound != NULL && *(int*)((BYTE*)g_pDirectSound + 0x10) != 0) {
        g_pDirectSound->compact();
    }
}

void SndCompactAsync(void)
{
    ExecAsync((void*)SndCompactCallback);
}

int findAndOpenFile(char* path)
{
    // Callers build the path with GAME_DATA_ROOT, so remap its root to the
    // configured tree (config.ini [Assets] Path/Version) - the same rewrite
    // DirectSound::CreateSound applies before opening. This is the existence
    // probe that gates voice loading (SoundSystem.cpp load_voice), so without
    // it a configured asset root makes every voice line look missing.
    char rooted[260];
    const char* src = ResolveAssetRoot(path, rooted, sizeof(rooted));

    // AudioFile_Find: the .wav, or the .ogg the asset migrator wrote instead.
    char found[260];
    return AudioFile_Find(src, found, sizeof(found));
}
