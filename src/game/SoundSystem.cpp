// SoundSystem.cpp - Sound system implementation
// All functions decompiled from Ghidra with original addresses
#include "../Globals.h"
#include "../platform/platform.h"
#include "../marni/MarniSound.h"
#include "Entities.h"
#include "SoundTables.h"
#include "dc/ArrangeStages.h"     // room_file_stage(), STAGE_ARRANGE_FIRST
#include "dc/ArrangeSoundRows.h"  // dc_arrange_snd_slots()
#include "../DebugPrint.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../system/AssetPath.h"

// ============================================================================
// sounds_reset (0x0047eb70)
// Destroys all sound banks and resets all sound state.
// ============================================================================
void sounds_reset(void)
{
    if (g_BgmSoundBank != 0) {
        destroySndBank(g_BgmSoundBank);
    }
    g_BgmSoundBank = 0;

    for (int i = 0; i < 64; i += 2) {
        if (g_SfxBanks[i] != 0) {
            destroySndBank(g_SfxBanks[i]);
            g_SfxBanks[i] = 0;
        }
        g_SfxBanks[i + 1] = 0;
    }
    for (int i = 0; i < 64; i += 2) {
        if (g_RoomSfxBanks[i] != 0) {
            destroySndBank(g_RoomSfxBanks[i]);
            g_RoomSfxBanks[i] = 0;
        }
        g_RoomSfxBanks[i + 1] = 0;
    }
    for (int i = 0; i < 64; i += 2) {
        if (g_CharacterSfxBanks[i] != 0) {
            destroySndBank(g_CharacterSfxBanks[i]);
            g_CharacterSfxBanks[i] = 0;
        }
        g_CharacterSfxBanks[i + 1] = 0;
    }
    // 48 records, i.e. 96 ints - the array is record-strided, not int-strided
    for (int i = 0; i < 96; i += 2) {
        if (g_emSndBanks[i] != 0) {
            destroySndBank(g_emSndBanks[i]);
            g_emSndBanks[i] = 0;
        }
        g_emSndBanks[i + 1] = 0;
    }
    // Original: stride 8, bound 0x00ac99e8 -> 3 records; clears handle, field_04,
    // slot and (uniquely for this array) paused.
    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0) {
            destroySndBank(g_SndBank[i].handle);
        }
        g_SndBank[i].handle   = 0;
        g_SndBank[i].field_04 = 0;
        g_SndBank[i].slot     = 0;
        g_SndBank[i].paused   = 0;
    }

    g_SfxVolume = -1;
    g_EnemySndVolume = -1;
    g_roomSfxVolume = -1;
    g_charSfxVolume = -1;
    g_bgmDefaultVolume = -1;
    DAT_00bf07ef = 0xFF;
    g_BGM_STATE = 0xFF;
    // 0x0047eb45. Never restoring this pointer is what silenced every room's BGM:
    // bgm_load_and_start and update_room_bgm both bail out when it is NULL.
    g_bgmDataTable = &g_BgmRoomData[0][0][0];
}

// ============================================================================
// LoadSoundBank (0x0047ed30)
// Loads a sound effects bank from file into memory.
// sound_bank_id: 0-110, selects a sub-table of up to 16 filenames.
// Each non-null entry in the sub-table is loaded into a g_SfxBanks slot.
// ============================================================================
void LoadSoundBank(int sound_bank_id, void* buffer)
{
    if (sound_bank_id > 110) {
        sound_bank_id = 15;
    }

    const char** subtable = NULL;
    if (sound_bank_id >= 0 && sound_bank_id < 16) {
        subtable = g_SoundBanksTable[sound_bank_id];
    }
    if (subtable == NULL) {
        OutputDebugStringA("[DEBUG] LoadSoundBank: subtable is NULL\n");
        return;
    }

    char dbg[256];

    int iVar6 = 0;
    int* bankPtr = g_SfxBanks;
    int* endPtr = g_SfxBanks + 32;  // end of original g_SfxBanks area

    while (bankPtr < endPtr) {
        if (bankPtr[0] != 0) {
            destroySndBank(bankPtr[0]);
            bankPtr[0] = 0;
        }
        *((char*)&bankPtr[1]) = 0;
        *((char*)&bankPtr[1] + 1) = 0;

        const char* filename = *(const char**)((BYTE*)subtable + iVar6);
        if (filename != NULL) {
            char path[256];
            sprintf(path, GAME_DATA_ROOT "sound\\%s.wav", filename);

            int bank = loadSndBankFromWav(path);

            bankPtr[0] = bank;
            if (bank != 0) {
                pan_set(bank, 0);
                set_volume(bank, g_SfxVolume);
            }
        } else {
            sprintf(dbg, "[DEBUG] LoadSoundBank: slot is NULL\n");
            OutputDebugStringA(dbg);
        }

        iVar6 += 4;
        bankPtr += 2;
    }
}

// ============================================================================
// play_sfx (0x0047fb10)
// Plays a sound effect from the specified bank type.
// bank: 0=room, 1=sfx, 2=enemy, 3=character, 4=special
// ============================================================================
void play_sfx(int bank, int soundId)
{
    int handle = 0;

    switch (bank) {
    case 0:
        if (soundId > 1) return;
        handle = g_RoomSfxBanks[soundId * 2];
        break;

    case 1:
        if ((g_main_state_flags2 & MSF2_SFX_BANK1_HALF) != 0) {
            if (soundId > 15) return;
        } else {
            if (soundId > 31) return;
            if (soundId > 15) soundId -= 16;
        }
        handle = g_SfxBanks[soundId * 2];
        break;

    case 2:
        if (soundId > 47) return;
        handle = g_emSndBanks[soundId * 2];
        break;

    case 3:
        if (soundId > 15) return;
        handle = g_CharacterSfxBanks[soundId * 2];
        break;

    case 4:
        if (soundId < 48 && g_SndBank[0].handle != 0) {
            SetSndSlot(g_SndBank[0].handle, g_SndBank[0].slot);
        }
        return;

    default:
        return;
    }

    if (handle != 0) {
        SetSndSlot(handle, 0);
    }
}

// ============================================================================
// PauseSounds (0x0047b...)
// Pauses all currently playing sounds.
// ============================================================================
void PauseSounds(void)
{
    if (g_BgmSoundBank != 0) {
        if (getSndStat(g_BgmSoundBank) == 1) {
            setSndStop(g_BgmSoundBank);
            g_BgmPaused = 1;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0) {
            if (getSndStat(g_SndBank[i].handle) == 1) {
                setSndStop(g_SndBank[i].handle);
                g_SndBank[i].paused = 1;
            }
        }
    }
}

// ============================================================================
// ResumePausedSounds (0x0047b...)
// Resumes all paused sounds.
// ============================================================================
void ResumePausedSounds(void)
{
    if (g_BgmSoundBank != 0 && g_BgmPaused == 1) {
        playSnd(g_BgmSoundBank, 0);
        g_BgmPaused = 0;
    }

    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0 && g_SndBank[i].paused == 1) {
            playSnd(g_SndBank[i].handle, (int)g_SndBank[i].slot);
            g_SndBank[i].paused = 0;
        }
    }
}

// ============================================================================
// UpdateSoundFadeState (0x0047b...)
// Handles sound fade state machine.
// ============================================================================
void UpdateSoundFadeState(void)
{
    if (g_SndFadeType < 0) {
        g_SndFadeType++;
        if (g_SndFadeType == -1) {
            UpdateSoundFade(-1);
            g_SndFadeType = 0;
            return;
        }
        if (g_SndFadeType == -2) {
            for (int i = 0; i < 3; i++) {
                if (g_SndBank[i].handle != 0) {
                    setSndStop(g_SndBank[i].handle);
                    destroySndBank(g_SndBank[i].handle);
                    g_SndBank[i].handle = 0;
                }
            }
            return;
        }
        if (g_SndFadeType == -4) {
            if (g_BgmSoundBank != 0) {
                setSndStop(g_BgmSoundBank);
            }
            return;
        }
        if (g_SndFadeType == -29 && g_BGM_STATE != 0xFF) {
            for (int i = 0; i < 3; i++) {
                if (g_SndBank[i].handle != 0) {
                    setSndStop(g_SndBank[i].handle);
                }
            }
            return;
        }
    } else {
        UpdateSoundFade(g_SndDistSteps);
    }
}

// ============================================================================
// UpdateSoundDecay (0x0047b...)
// Updates sound volume ramp (decay/fade-out over time).
// ============================================================================
void UpdateSoundDecay(void)
{
    // Original: MOV EAX,dword ptr [EAX*0x8 + 0xac99d0] - a record index, not *3.
    int bank = g_SndBank[g_SndRampBankIndex].handle;
    if (bank != 0) {
        int vol = getSndVol(bank);
        if (vol == 2000) {
            vol = -9999;
        }
        g_SndRampFramesLeft--;
        g_SndRampCurrentVolume = vol + (400 / (g_SndRampFramesLeft + 1) * g_SndRampDirection) / 100;
        set_volume(bank, g_SndRampCurrentVolume);

        if (g_SndRampCurrentVolume < -10000 || g_SndRampFramesLeft <= 0) {
            g_SndRampDirection = 0;
            g_SndRampFramesLeft = 0;
            g_SndRampCurrentVolume = 0;
            setSndStop(bank);
            set_volume(bank, -1);
        }
    }
}

// ============================================================================
// UpdateMusicWaitState (0x0047b...)
// Checks if BGM has finished playing; clears wait flag if done.
// ============================================================================
void UpdateMusicWaitState(void)
{
    if (g_BgmSoundBank != 0 && getSndStat(g_BgmSoundBank) == 1) {
        return;
    }
    g_main_state_flags &= ~MSF_VOICE_PLAYING;
    g_WaitForMusicTimer = 0;
}

// ============================================================================
// PauseGameSoundsAsync (0x0047...)
// Queues async callback to pause all game sounds (used during FMV).
// ============================================================================
void PauseGameSoundsAsync(void)
{
    PauseSounds();
}

// ============================================================================
// ResumeGameSoundsAsync (0x0047...)
// Resumes all game sounds (after FMV).
// ============================================================================
void ResumeGameSoundsAsync(void)
{
    ResumePausedSounds();
}

// ============================================================================
// ProbeWaveOutDevicesAndCacheVolume (0x0047...)
// Enumerates wave out devices and caches the current volume setting.
//
// The device walk itself is a Win32 waveOut path; it moved to the platform
// layer in Phase 0 (src/platform/win32/platform.cpp). Behaviour unchanged.
// ============================================================================
void ProbeWaveOutDevicesAndCacheVolume(void)
{
    plat_audio_probe_and_cache_volume();
}

// ============================================================================
// StartSoundSystemAsync (0x0047...)
// Sets the main window handle and queues async sound system initialization.
// ============================================================================
void StartSoundSystemAsync(HWND hwnd)
{
    g_MainWindowHandle = hwnd;
    OutputDebugStringA("[DEBUG] StartSoundSystemAsync: calling InitializeSoundSystem directly\n");
    // Original ExecAsync(InitializeSoundSystem) was a no-op (stub at 0x00420290).
    // TaskScheduler_Reset() would clear the callback anyway, so call it directly.
    InitializeSoundSystem();
}

// ============================================================================
// RestoreWaveOutVolume (0x0047...)
// Enumerates wave out devices and restores the cached volume.
//
// Device walk moved to the platform layer in Phase 0; behaviour unchanged.
// ============================================================================
void RestoreWaveOutVolume(void)
{
    plat_audio_restore_volume();
}

// ============================================================================
// getSndStat (0x0047...)
// Returns the status of a sound bank (0=stopped, 1=playing).
// ============================================================================
int getSndStat(int bank)
{
    if (g_pDirectSound == NULL || bank == 0) return 0;
    return g_pDirectSound->GetStatus(bank);
}

// ============================================================================
// load_room_sfx (0x0047eba0)
// Loads room-specific sound effects for the current stage.
// Original addresses: 0x0047eba0
// Destroys any previously loaded room SFX banks, then loads up to 2
// room sound effect WAVs from ./usa/sound/<name>.wav into g_RoomSfxBanks.
// ============================================================================

// --- Room sound effects sub-tables (0x004d01d0 area, 2 entries each) ---
// Each sub-table contains up to 2 WAV filenames (without extension).
// Entry layout: [0] = primary sound, [1] = alternate sound (or NULL).

static const char* g_roomSfx_00[2] = { "Dr_wd01",  "Dr_wd02"  };   // 0x004d01d0
static const char* g_roomSfx_01[2] = { "Dr_mtl01", "Dr_mtl02" };   // 0x004d01d8
static const char* g_roomSfx_02[2] = { NULL,       "Dr_brk01" };   // 0x004d01e0
static const char* g_roomSfx_03[2] = { "Dr_reb01", "Dr_reb02" };   // 0x004d01e8
static const char* g_roomSfx_04[2] = { "St_wcp01", NULL       };   // 0x004d0208
static const char* g_roomSfx_05[2] = { "St_wd01",  NULL       };   // 0x004d0210
static const char* g_roomSfx_06[2] = { "Ev_mv",    "Ev_mv"    };   // 0x004d01f0
static const char* g_roomSfx_07[2] = { "Ev_new01", "Ev_new02" };   // 0x004d01f8
static const char* g_roomSfx_08[2] = { "Dr_gat01", "Dr_gat02" };   // 0x004d0220
static const char* g_roomSfx_09[2] = { "St_mtl01", NULL       };   // 0x004d0218
static const char* g_roomSfx_10[2] = { "Ev_old01", "Ev_old02" };   // 0x004d0200
static const char* g_roomSfx_11[2] = { "Ladder01", NULL       };   // 0x004d0228
static const char* g_roomSfx_12[2] = { "Dr_air01", "Dr_air02" };   // 0x004d0230
static const char* g_roomSfx_13[2] = { "Ev_lab01", NULL       };   // 0x004d0238
static const char* g_roomSfx_14[2] = { "Ev_ftn01", "Ev_ftn02" };   // 0x004d0240

// --- room_sound_effects table (0x004d0248) ---
// Indexed by soundTableIndex. Terminated by NULL sentinel.
static const char** g_roomSoundEffectsTable[] = {
    g_roomSfx_00,  // 0x004d0248: index  0 - Dr_wd01/02 (mansion door open/close)
    g_roomSfx_01,  // 0x004d024c: index  1 - Dr_mtl01/02 (metal door open/close)
    g_roomSfx_02,  // 0x004d0250: index  2 - null, Dr_brk01 (trap room falling and closing the door)
    g_roomSfx_03,  // 0x004d0254: index  3 - Dr_reb01/02 (Rebecca saying "Chris!")
    g_roomSfx_04,  // 0x004d0258: index  4 - St_wcp01 (wooden floor footstep 1)
    g_roomSfx_05,  // 0x004d025c: index  5 - St_wd01 (wooden floor footstep 2)
    g_roomSfx_06,  // 0x004d0260: index  6 - Ev_mv (small elevator)
    g_roomSfx_07,  // 0x004d0264: index  7 - Ev_new01/02 (desk open/close?)
    g_roomSfx_08,  // 0x004d0268: index  8 - Dr_gat01/02 (exterior gate open/close)
    g_roomSfx_09,  // 0x004d026c: index  9 - St_mtl01 (metal floor footstep)
    g_roomSfx_10,  // 0x004d0270: index 10 - Ev_old01/02 (elevator doors open/close)
    g_roomSfx_11,  // 0x004d0274: index 11 - Ladder01 (ladder)
    g_roomSfx_12,  // 0x004d0278: index 12 - Dr_air01/02 (Lab gates open/close)
    g_roomSfx_13,  // 0x004d027c: index 13 - Ev_lab01 (Lab elevator door open)
    g_roomSfx_14,  // 0x004d0280: index 14 - Ev_ftn01/02 (Fountain elevetor door open/close)
    NULL,          // 0x004d0284: terminator
};

void load_room_sfx(unsigned char soundTableIndex)
{
    int iVar6 = 0;
    int* piVar7 = g_RoomSfxBanks;

    do {
        // destroy existing bank if loaded
        if (*piVar7 != 0) {
            destroySndBank(*piVar7);
        }
        *piVar7 = 0;
        *((unsigned char*)&piVar7[1]) = 0;

        // get sub-table pointer for this sound table index
        const char** puVar2 = g_roomSoundEffectsTable[soundTableIndex];
        *((unsigned char*)&piVar7[1] + 1) = 0;

        if (puVar2 != NULL) {
            const char* filename = *(const char**)((BYTE*)puVar2 + iVar6);
            if (filename != NULL) {
                char path[260];
                sprintf(path, GAME_DATA_ROOT "sound\\%s.wav", filename);
                findAndOpenFile(path);

                int bank = loadSndBankFromWav(path);
                *piVar7 = bank;
                if (bank != 0) {
                    pan_set(bank, 0);
                    set_volume(bank, g_roomSfxVolume);
                }
            }
        }

        iVar6 += 4;
        piVar7 += 2;
    } while (piVar7 <= (int*)&g_RoomSfxBanks[2]);
}

// ============================================================================
// load_character_sfx (0x0047f070)
// Loads character-specific sound effects (footsteps, voice, etc.).
// Original addresses: 0x0047f070
// Destroys any previously loaded character SFX banks, then loads up to 2
// character sound effect WAVs from ./usa/sound/<name>.wav into g_CharacterSfxBanks.
// ============================================================================

// --- Character sound effects sub-tables (0x004d0288 area, 16 entries each, 0x40 stride) ---
// Shared entries across all characters: [4]=cursor, [5]=cancel, [6]=decide,
// [7]=Chris08, [8]=Chris10, [9]=Chris09, [10]=Mapled, [11-15]=NULL

static const char* g_charSfx_00[16] = {  // 0x004d0288 - Chris base
    "Chris01",  "Chris02",  "Chris03",  "Chris04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_01[16] = {  // 0x004d0308 - Jill base
    "Jill01",   "Jill02",   "Jill03",   "Jill04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_02[16] = {  // 0x004d0388 - Rebecca base
    "Reb01",    "Reb02",    "Reb03",    "Reb04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_03[16] = {  // 0x004d0388 dup - Rebecca base
    "Reb01",    "Reb02",    "Reb03",    "Reb04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_04[16] = {  // 0x004d02c8 - Chris (echoed)
    "Ch_ef01",  "Ch_ef02",  "Ch_ef03",  "Ch_ef04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_05[16] = {  // 0x004d0348 - Jill (echoed)
    "Jill_ef01","Jill_ef02","Jill_ef03","Jill_ef04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_06[16] = {  // 0x004d03c8 - Rebecca (echoed)
    "Reb_ef01", "Reb_ef02", "Reb_ef03", "Reb_ef04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};
static const char* g_charSfx_07[16] = {  // 0x004d03c8 dup - Rebecca (echoed)
    "Reb_ef01", "Reb_ef02", "Reb_ef03", "Reb_ef04",
    "cursor",   "cancel",   "decide",   "Chris08",
    "Chris10",  "Chris09",  "Mapled",   NULL,
    NULL,       NULL,       NULL,       NULL,
};

// --- characters_sfx_table (0x004d0408) ---
// Indexed by char_id. 8 entries total.
static const char** g_charactersSfxTable[] = {
    g_charSfx_00,  // 0x004d0408: index 0 - Chris base
    g_charSfx_01,  // 0x004d040c: index 1 - Jill base
    g_charSfx_02,  // 0x004d0410: index 2 - Rebecca base
    g_charSfx_03,  // 0x004d0414: index 3 - Rebecca base (dup)
    g_charSfx_04,  // 0x004d0418: index 4 - Chris alternate
    g_charSfx_05,  // 0x004d041c: index 5 - Jill alternate
    g_charSfx_06,  // 0x004d0420: index 6 - Rebecca alternate
    g_charSfx_07,  // 0x004d0424: index 7 - Rebecca alternate (dup)
};

void load_character_sfx(unsigned char charId)
{
    int iVar6 = 0;
    int* piVar7 = g_CharacterSfxBanks;

    do {
        // destroy existing bank if loaded
        if (*piVar7 != 0) {
            destroySndBank(*piVar7);
        }
        *piVar7 = 0;
        *((unsigned char*)&piVar7[1]) = 0;

        // get sub-table pointer for this character
        const char** puVar2 = g_charactersSfxTable[charId];
        *((unsigned char*)&piVar7[1] + 1) = 0;

        if (puVar2 != NULL) {
            const char* filename = *(const char**)((BYTE*)puVar2 + iVar6);
            if (filename != NULL) {
                char path[260];
                sprintf(path, GAME_DATA_ROOT "sound\\%s.wav", filename);
                findAndOpenFile(path);

                int bank = loadSndBankFromWav(path);
                *piVar7 = bank;
                if (bank != 0) {
                    pan_set(bank, 0);
                    set_volume(bank, g_charSfxVolume);
                }
            }
        }

        iVar6 += 4;
        piVar7 += 2;
        // This bound is the authoritative size of g_CharacterSfxBanks: stride 8,
        // last record at base+120, i.e. 16 records spanning 0x00ac9950-0x00ac99cf
        // (ending exactly where g_SndBank begins). Original: `ptr <= 0xac99cf`.
        // Note sounds_reset / DestroyAllSoundBanks / UpdateSoundFade stop at
        // 0xac9998 in the original and so only cover the first 9 - that is a
        // Capcom bug (records 9-15 leak); see docs/SCD_SCRIPT_SYSTEM.md 6g.
    } while (piVar7 <= (int*)&g_CharacterSfxBanks[30]);
}

// ============================================================================
// bgm_fade_out_all (0x004800e0)
// Fades out and destroys all BGM and secondary sound banks.
// ============================================================================
static void bgm_fade_out_all(void)
{
    // mute all secondary sound banks
    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0) {
            set_volume(g_SndBank[i].handle, 0xffffd8f1);
        }
    }
    Task_sleep(1);

    // stop all secondary sound banks
    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0) {
            setSndStop(g_SndBank[i].handle);
        }
    }

    // wait 4 frames
    for (int i = 0; i < 4; i++) {
        Task_sleep(1);
    }

    // stop and destroy main BGM bank
    if (g_BgmSoundBank != 0) {
        setSndStop(g_BgmSoundBank);
        destroySndBank(g_BgmSoundBank);
        g_BgmSoundBank = 0;
    }

    Task_sleep(1);

    // destroy all secondary banks
    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle != 0) {
            destroySndBank(g_SndBank[i].handle);
            g_SndBank[i].handle = 0;
        }
    }
}

// ----------------------------------------------------------------------------
// The group lookup bgm_load_and_start and update_room_bgm share, as one
// expression. The bounds check has no counterpart in the original: with a 0xFF
// state the low three bits are 7, which walks past the end of the last room's
// 4-byte row and into whatever .rdata follows the table. Every caller either
// tests for 0xFF first or only uses the result when it isn't, so 0xFF is the
// right answer for the out-of-range case.
// ----------------------------------------------------------------------------
static unsigned char bgm_group_for(unsigned char roomId, unsigned char bgmState)
{
    unsigned int idx = (get_stage_id() * 0x20 + roomId) * 4 + (bgmState & 7);
    if (g_bgmDataTable == NULL || idx >= sizeof(g_BgmRoomData)) {
        return 0xFF;
    }
    return g_bgmDataTable[idx];
}

// ============================================================================
// bgm_load_and_start (0x0047f200)
// Loads the three BGM channel banks for the current room. Despite the name it
// only LOADS - nothing is audible until bgm_start_secondary_slots (or SCD opcode
// 0x15) plays a channel, which is gated on bits 3-5 of the room's BGM state.
//
// Tables (see SoundTables.cpp section 4):
//   group    = g_bgmDataTable[(stage * 0x20 + room) * 4 + (bgmState & 7)]
//   filename = g_BgmNameTable[group][channel]     (0x004d07b8 -> 0x004d0428)
//   loopFlag = g_BgmLoopTable[group][channel]     (0x004d0980 -> 0x004d089c)
//
// Note the retail binary nops out the alternate-path branch guarded by
// g_SoundSystemFlags bit 3 (0x0047f42d-0x0047f470): it builds a second path from
// g_SoundAltPathPrefix, discards it, and falls into the normal path regardless.
// ============================================================================
// ============================================================================
// BGM LOOPING - whole buffer, by design. Do not "fix" this.
//
// A looping BGM channel repeats its ENTIRE wav from sample 0; there is no
// loop-region mechanism anywhere in the PC build. Confirmed end to end:
//   * the wav loader (0x00463110) is the stock mmio WaveLoadFile and reads only
//     `fmt ` and `data`; no BGM wav carries a `smpl` chunk,
//   * g_BgmLoopTable holds a 1-bit flag, not an offset, and the exe has no
//     loop-offset table (the string "Bgm_24" appears exactly once, in the name
//     pool at 0x004d0428),
//   * playback is DirectSound Play(0, 0, DSBPLAY_LOOPING), which can only repeat
//     the whole buffer.
//
// This is audible on Bgm_24, the Tyrant battle: the wav is 43.8 s of which the
// first 33.5 s is a build-up, so the build-up comes back round every cycle. That
// is correct for the PC release - the PS1 version loops only the 10.3 s battle
// section, and the assets still ship the two halves separately (bgm_24a.wav
// 739454 samples + bgm_24b.wav 227316 samples = bgm_24.wav 966470 samples, the
// only such pair among the 61 BGM files). The PC build simply plays the joined
// file and loops all of it.
// ============================================================================

static void bgm_load_and_start(unsigned char bgmState)
{
    int slotCount = 3;

    // 0x0047f206: stage 2 room 7 drops the third channel unless Jill is here with
    // one exact flag combination. Every jump in that chain but the last targets
    // the slotCount=2 path, so 3 channels survive only for
    //   (id & 3) == 1 && Flg_ck(0x5c) && Flg_ck(0x48) && !Flg_ck(0x55)
    // and the two-channel case is the default, not the exception. The previous
    // transcription had the condition inverted and never reached it for Chris.
    //
    // The flag base the original pushes is 0x00be9854 = g_ScenarioFlags2 (SCD
    // bank 1) - the same bank the room's init script tests to spawn Barry and
    // schedule the cutscene. An earlier revision tested g_ScenarioFlags
    // (0x00be98c0, bank 0) here; bank 0 never carries 0x5c/0x48 at that point,
    // so the condition always failed, g_SndBank[2] was destroyed and the third
    // channel's wav (g_BgmNameTable[0x26][2] = "V110_00", the underground
    // Jill/Barry cutscene's mixed dialogue track) never loaded. The scene's
    // SCD opcode 0x15 (cmd_bgm_play, operand 0x0215) then started an empty
    // channel and the cutscene ran with no voices.
    if (g_stageId == STAGE_COURTYARD && g_roomId == ROOM_UNDERGROUND_ENTRY &&
        !((g_playerEntity.id & 3) == 1 &&
          Flg_ck((int)&g_ScenarioFlags2, SCENARIO2_FLAG_PROGRESS_5C) &&
          Flg_ck((int)&g_ScenarioFlags2, SCENARIO2_FLAG_PROGRESS_48) &&
          !Flg_ck((int)&g_ScenarioFlags2, SCENARIO2_FLAG_PROGRESS_55))) {
        slotCount = 2;
        if (g_SndBank[2].handle != 0) {
            destroySndBank(g_SndBank[2].handle);
        }
        g_SndBank[2].handle   = 0;
        g_SndBank[2].field_04 = 0;
        g_SndBank[2].slot     = 0;
        g_SndBank[2].paused   = 0;
    }

    unsigned char group = bgm_group_for(g_roomId, bgmState);

    for (int idx = 0; idx < slotCount; idx++) {
        SndBankSlot* ch = &g_SndBank[idx];
        if (ch->handle != 0) {
            destroySndBank(ch->handle);
        }
        ch->handle   = 0;
        ch->field_04 = 0;
        ch->slot     = 0;
        ch->paused   = 0;

        if (group == 0xFF) continue;

        const char* filename = g_BgmNameTable[group][idx];
        if (filename == NULL) continue;

        char path[260];
        sprintf(path, GAME_DATA_ROOT "sound\\%s.wav", filename);
        findAndOpenFile(path);

        int bank = loadSndBankFromWav(path);
        ch->handle = bank;
        if (bank == 0) {
            dbg_printf("[bgm] stage %u room %u ch%d group %02X '%s' FAILED TO LOAD\n",
                       (unsigned int)g_stageId, (unsigned int)g_roomId, idx,
                       (unsigned int)group, filename);
            continue;
        }

        ch->slot = (signed char)g_BgmLoopTable[group][idx];
        pan_set(bank, 0);

        // 0x0047f4df: three tracks load muted and are faded up later by the SCD
        // volume opcodes; everything else comes in at the default BGM volume.
        if (_stricmp(filename, "Se_01") != 0 &&
            _stricmp(filename, "Se_4d") != 0 &&
            _stricmp(filename, "Se_42") != 0) {
            set_volume(bank, g_bgmDefaultVolume);
        } else {
            set_volume(bank, -9999);
        }

        dbg_printf("[bgm] stage %u room %u ch%d group %02X '%s' loop=%d\n",
                   (unsigned int)g_stageId, (unsigned int)g_roomId, idx,
                   (unsigned int)group, filename, (int)ch->slot);
    }
}

// ============================================================================
// bgm_start_secondary_slots (0x0047f560)
// Starts playing secondary sound slots if their bits are set in the target BGM state.
// ============================================================================
static void bgm_start_secondary_slots(void)
{
    if (g_targetBgmState == 0xFF) return;

    if (g_targetBgmState & 8) {
        if (g_SndBank[0].handle != 0) {
            SetSndSlot(g_SndBank[0].handle, g_SndBank[0].slot);
        }
        g_BGM_STATE |= 8;
    }
    if (g_targetBgmState & 0x10) {
        if (g_SndBank[1].handle != 0) {
            SetSndSlot(g_SndBank[1].handle, g_SndBank[1].slot);
        }
        g_BGM_STATE |= 0x10;
    }
    if (g_targetBgmState & 0x20) {
        if (g_SndBank[2].handle != 0) {
            SetSndSlot(g_SndBank[2].handle, g_SndBank[2].slot);
        }
        g_BGM_STATE |= 0x20;
    }
}

// ============================================================================
// update_room_bgm (0x0047f620)
// Manages BGM state transitions when entering/loading a room.
// Reads the target BGM state from g_RoomBgmStatePtr[g_roomId], compares with
// the current g_BGM_STATE, and fades/starts BGM tracks as needed.
// ============================================================================
void update_room_bgm(void)
{
    g_targetBgmState = g_RoomBgmStatePtr[g_roomId];
    g_prevBgmState = (unsigned char)g_BGM_STATE;

    dbg_printf("[bgm] update: stage=%u room=%u from=%u target=%02X prev=%02X\n",
               (unsigned int)g_stageId, (unsigned int)g_roomId,
               (unsigned int)g_AttractMode_RoomCameraId,
               (unsigned int)g_targetBgmState, (unsigned int)g_prevBgmState);

    if (g_targetBgmState == 0xFF) {
        if ((unsigned char)g_BGM_STATE != 0xFF) {
            bgm_fade_out_all();
            g_BGM_STATE = 0xFF;
        }
        g_main_state_flags2 &= ~MSF2_SND_BUSY;
        return;
    }

    // 0x0047f670: the "group" byte the outgoing and incoming rooms resolve to.
    // g_AttractMode_RoomCameraId holds the room we came FROM (room_transition_load
    // stores g_roomId into it before overwriting g_roomId with the destination).
    const unsigned char newGroup = bgm_group_for(g_roomId, g_targetBgmState);
    const unsigned char oldGroup = bgm_group_for(g_AttractMode_RoomCameraId, g_prevBgmState);

    // 0x0047f6ba: a different track is coming and something is currently playing.
    // This only tears the old banks down - the reload happens in the switch below.
    // The original tests the group compare first; the 0xFF test is hoisted here
    // because a 0xFF prev makes the old index (& 7 == 7) run off the end of the row.
    if (g_prevBgmState != 0xFF && (g_prevBgmState & 0x38) != 0 && newGroup != oldGroup) {
        bgm_fade_out_all();
        g_main_state_flags2 &= ~MSF2_SND_BUSY;
    }

    bool startSecondary = false;
    unsigned char bgmType = g_targetBgmState >> 6;

    if (bgmType == 0) {
        if (g_prevBgmState == 0xFF) {
            // nothing was playing: straight load
            g_main_state_flags2 &= ~MSF2_SND_BUSY;
            bgm_load_and_start(g_targetBgmState);
            startSecondary = true;
        } else if (newGroup != oldGroup) {
            bgm_fade_out_all();
            g_main_state_flags2 &= ~MSF2_SND_BUSY;
            bgm_load_and_start(g_targetBgmState);
            startSecondary = true;
        } else {
            // Same track continues across the transition - keep the loaded banks
            // and only toggle the channels whose enable bit changed.
            g_main_state_flags2 &= ~MSF2_SND_BUSY;
            unsigned char changed = g_targetBgmState ^ g_prevBgmState;
            for (int i = 0; i < 3; i++) {
                unsigned char bit = (unsigned char)(8 << i);
                if ((changed & bit) == 0) continue;
                if (g_SndBank[i].handle == 0) continue;
                if (g_targetBgmState & bit) {
                    SetSndSlot(g_SndBank[i].handle, g_SndBank[i].slot);
                } else {
                    setSndStop(g_SndBank[i].handle);
                }
            }
        }
    } else if (bgmType == 1) {
        // always reload, never auto-start (0x0047f8ae falls through to done)
        bgm_fade_out_all();
        g_main_state_flags2 &= ~MSF2_SND_BUSY;
        bgm_load_and_start(g_targetBgmState);
    } else if (bgmType == 2) {
        // force restart: bit 7 is consumed here, so the (& 0xC0) test below always
        // passes and the channels are started immediately
        g_targetBgmState &= 0x7F;
        bgm_fade_out_all();
        g_main_state_flags2 &= ~MSF2_SND_BUSY;
        bgm_load_and_start(g_targetBgmState);
        startSecondary = (g_targetBgmState & 0xC0) == 0;
    }
    // bgmType == 3 is unused: it falls straight through to done.

    if (startSecondary) {
        bgm_start_secondary_slots();
    }

    g_BGM_STATE = g_targetBgmState;
    g_main_state_flags2 &= ~MSF2_SND_BUSY;
}

// ============================================================================
// SquareRoot0 (0x0040a500)
// PS1 GTE square root function. Returns integer square root of the value.
// ============================================================================
int SquareRoot0(int val)
{
    if (val <= 0) return 0;
    return (int)sqrt((double)val);
}

// ============================================================================
// GetAngleQuadrantValue (0x0040a930)
// Maps a fixed-point slope (delta_z * 4096 / delta_x) to an angle value
// in the game's 12-bit angle system (0-4095 = full circle).
// Returns atan(slope / 4096) scaled to game angle units.
// ============================================================================
int GetAngleQuadrantValue(int slope)
{
    if (slope == 0) return 0;
    double radians = atan((double)slope / 4096.0);
    return (int)(radians * (2048.0 / 3.14159265358979323846));
}

// ============================================================================
// CalculateAngleBetweenPointsXZ (0x004603e0)
// Calculates the 2D direction angle from point 1 to point 2 on the XZ plane.
// Returns a 12-bit angle (0-4095) representing a full circle.
// ============================================================================
unsigned short CalculateAngleBetweenPointsXZ(int pos1_x, int pos1_z, int pos2_x, int pos2_z)
{
    short dx = (short)pos2_x - (short)pos1_x;
    if (dx != 0) {
        short dz = (short)pos2_z - (short)pos1_z;
        // * 4096, not << 12: dz is a signed short, and shifting a negative
        // value left is UB (the original's SHL wrapped instead).
        int slope = ((int)dz * 4096) / (int)dx;
        short angle = (short)GetAngleQuadrantValue(slope);
        return (unsigned short)(-((unsigned short)(dx < 0) * 0x800 + angle)) & 0xFFF;
    }
    short dz = (short)pos2_z - (short)pos1_z;
    return (unsigned short)((0 < dz) * 0x800 + 0x400);
}

// ============================================================================
// Calc3DSndPan (0x0047fd20)
// Computes 3D spatial audio panning based on the sound source position
// relative to the current camera. Sets g_snd_pan_right (right) and
// g_snd_pan_left (left) pan values (0x1E-0x7F range).
// Distance attenuates both channels. Angle determines stereo balance.
// ============================================================================
void Calc3DSndPan(VECTOR* pos) // 0x0047fd20
{
    RDT_Camera* cameras = (RDT_Camera*)((char*)g_RdtPointer + 0x94);
    RDT_Camera* cam = &cameras[g_roomCameraId];

    short dx = (short)cam->cam_from_x - (short)pos->x;
    short dz = (short)cam->cam_from_z - (short)pos->z;

    int horizDistSq = (int)dx * (int)dx + (int)dz * (int)dz;
    int dy = *(int*)((char*)&MATRIX_00d22680 + 4) - (int)pos->y;

    int horizDist = SquareRoot0(horizDistSq);
    int distSq3D = horizDist * horizDist + dy * dy;
    int dist3D = SquareRoot0(distSq3D);

    unsigned short angleToSound = CalculateAngleBetweenPointsXZ(
        (short)cam->cam_from_x, (short)cam->cam_from_z,
        (short)pos->x, (short)pos->z);

    unsigned short angleToTarget = CalculateAngleBetweenPointsXZ(
        (short)cam->cam_from_x, (short)cam->cam_from_z,
        cam->cam_to_x, cam->cam_to_z);

    unsigned short angleDiff = (angleToSound - angleToTarget) & 0xFFF;

    if (angleDiff != 0 && angleDiff != 0x1000 && angleDiff != 0x800) {
        bool isRightSide = angleDiff < 0x801;
        unsigned short absAngle = angleDiff;
        if (!isRightSide) {
            absAngle = (~absAngle) & 0x7FF;
        }
        if (absAngle > 0x400) {
            absAngle = ~absAngle;
        }

        if ((absAngle & 0x7FF) > 0x40) {
            short panOffset = (short)((int)(short)(absAngle & 0x7FF) / (dist3D / 2000 + 0x18));
            short rightPan = panOffset + 0x7F;
            short leftPan = 0x7F - panOffset;

            // Clamp right pan to [0x1E, 0x7F]
            if (rightPan > 0x7F) rightPan = 0x7F;
            if (rightPan < 0) rightPan = 0;

            // Clamp left pan to [0x1E, 0x7F]
            if (leftPan < 0x1E) leftPan = 0x1E;

            if (isRightSide) {
                g_snd_pan_right = (unsigned short)rightPan;
                g_snd_pan_left = (unsigned short)leftPan;
            } else {
                g_snd_pan_right = (unsigned short)leftPan;
                g_snd_pan_left = (unsigned short)rightPan;
            }
            goto applyDistance;
        }
    }

    g_snd_pan_right = 0x7F;
    g_snd_pan_left = 0x7F;

applyDistance:
    // Attenuate both channels by distance
    g_snd_pan_left = g_snd_pan_left + (unsigned short)(dist3D / -500);
    g_snd_pan_right = g_snd_pan_right + (unsigned short)(dist3D / -500);

    if (g_snd_pan_left < 0x1E) {
        g_snd_pan_left = 0x1E;
    }
    if (g_snd_pan_right < 0x1E) {
        g_snd_pan_right = 0x1E;
    }

    g_snd_pan_left = g_snd_pan_left & 0x7F;
    g_snd_pan_right = g_snd_pan_right & 0x7F;
}

// ============================================================================
// CalcPanVolume (0x00480610)
// Converts 3D sound pan values to a DirectSound volume (hundredths of dB).
// The average of both pan channels determines the volume level.
// Range: -10000 (silence) to ~0 (full volume).
// ============================================================================
int CalcPanVolume(int panL, int panR) // 0x00480610
{
    int avg = (panL + panR) / 2;
    if (avg < 0x20) {
        return avg * 0x103 - 10000;
    }
    return avg * 0x12 - 0x8E4;
}

// ============================================================================
// Play3DSnd (0x0047f9c0)
// Plays a sound effect with 3D spatial audio positioning.
// Computes stereo panning and volume attenuation based on the sound source's
// position relative to the current camera.
//
// bank: 0=room, 1=sfx, 2=enemy, 3=character, 4=special/BGM
// soundId: index into the selected bank array
// vol: unused (reserved)
// pos: pointer to a VECTOR containing the sound source world position
//      (typically entity.transform.t)
// ============================================================================
void Play3DSnd(int bank, int soundId, int vol, int pos) // 0x0047f9c0
{
    VECTOR* soundPos = (VECTOR*)pos;
    int handle = 0;

    switch (bank) {
    case 0:
        if (soundId > 1) return;
        handle = g_RoomSfxBanks[soundId * 2];
        break;

    case 1:
        if ((g_main_state_flags2 & MSF2_SFX_BANK1_HALF) == 0) {
            if (soundId > 0x2F) return;
        } else {
            if (soundId > 0x0F) return;
        }
        handle = g_SfxBanks[soundId * 2];
        break;

    case 2:
        if (soundId > 0x2F) return;
        handle = g_emSndBanks[soundId * 2];
        break;

    case 3:
        if (soundId > 0x0F) return;
        handle = g_CharacterSfxBanks[soundId * 2];
        break;

    case 4:
        if (soundId < 0x30) {
            Calc3DSndPan(soundPos);
            if (g_SndBank[0].handle != 0) {
                pan_set(g_SndBank[0].handle, (g_snd_pan_right - g_snd_pan_left) * 0x4E);
                SetSndSlot(g_SndBank[0].handle, (int)g_SndBank[0].slot);
            }
        }
        return;

    default:
        return;
    }

    Calc3DSndPan(soundPos);
    if (handle != 0) {
        int dsVol = CalcPanVolume(g_snd_pan_right, g_snd_pan_left);
        set_volume(handle, dsVol);
        SetSndSlot(handle, 0);
    }
}

// ============================================================================
// SCD voice playback (opcode 0x1E, cmd_voice_play)
//
// Cutscene dialogue is one WAV per line under .\usa\voice\, loaded into
// g_BgmSoundBank. The handshake with the script matters: cmd_voice_play SETS
// g_main_state_flags bit 17 (0x20000) after asking for a voice, and
// play_sound_and_voice_effect type 2 CLEARS it. Event-VM opcode 0xF7 waits on
// that bit, so the voice is what gates a cutscene line advancing. While
// play_sound_and_voice_effect was an empty stub the bit was only ever cleared as
// a side effect of UpdateMusicWaitState finding no BGM playing - which is why
// scripted lines advanced silently instead of hanging.
//
// Voice state block at 0x00ae9ec8-0x00ae9ee0. None of these existed in the port.
// ============================================================================
static unsigned char g_voiceActive       = 0;  // 0x00ae9ec8
static unsigned char g_voiceExtraCount   = 0;  // 0x00ae9ec9
static int           g_voiceChannelCount = 0;  // 0x00ae9ecc
static int           g_voiceBankBase     = 0;  // 0x00ae9ec4
static int           g_voiceCursor       = 0;  // 0x00ae9ec0
static int           g_voiceBankLimit    = 0;  // 0x00ae9ed8
static int           g_voiceFinished     = 0;  // 0x00ae9ed4
static unsigned char g_voicePanMode      = 0;  // 0x00ae9ee0
static unsigned char g_voicePan[4]       = {0};// 0x00ae9edc-0x00ae9edf
static int           g_voiceMixerReset   = 0;  // 0x008f87ac
static int           g_voiceVolume       = 0;  // 0x00ac9900 - 0 in the image (no attenuation)

// ============================================================================
// voice_set_pan (0x00475640)
// Fills the four per-channel pan bytes. Mode 1 puts the raw value on channels 0
// and 2 with 0 between; any other mode halves it across all four.
// ============================================================================
static void voice_set_pan(unsigned char value)
{
    if (g_voicePanMode == 1) {
        g_voicePan[0] = value;
        g_voicePan[1] = 0;
        g_voicePan[2] = value;
        g_voicePan[3] = 0;
        return;
    }
    g_voicePan[0] = (unsigned char)(value >> 1);
    g_voicePan[1] = g_voicePan[0];
    g_voicePan[2] = g_voicePan[0];
    g_voicePan[3] = g_voicePan[0];
}

// ============================================================================
// voice_mixer_reset (0x004756b0)
// One store. The original is called as FUN_004756b0(9,0,0) but takes no
// parameters - the three pushes are dead, the same pattern as rotate_entity's
// fourth argument and PlayEntitySnd's second.
// ============================================================================
static void voice_mixer_reset(void)
{
    g_voiceMixerReset = 0;
}

// ============================================================================
// voice_load_and_play (0x004753c0)
// Loads the WAV for voice line `id` and starts it.
//
// The long inlined strcpy/strlen chains in the decompilation are the compiler
// expanding three strcats: voice directory, then the 9-byte name from
// g_StageVoiceNamesTable[stageId] + id*9, then ".wav".
//
// g_StageDataPtr is walked as 16-bit entries; an entry with bit 15 set consumes
// an extra word and bumps a counter. That counter and the two bank offsets it
// feeds are stored but nothing on this path reads them back - FUN_004753b0, whose
// return value seeds them, is an empty function in the original. Kept for
// fidelity, marked as inert.
// ============================================================================
static void voice_load_and_play(unsigned int id)
{
    int extra = 0;
    unsigned short* p = (unsigned short*)g_StageDataPtr;

    for (unsigned int i = 0; i < id; i++) {
        if ((*p & 0x8000) != 0) {
            extra++;
            p++;
        }
        p++;
    }

    g_voiceExtraCount = (unsigned char)extra;
    g_voiceActive     = 1;

    // 0x004753b0 is an empty function; these three stay inert.
    g_voiceBankLimit = 0;
    g_voiceBankBase  = g_voiceBankLimit + (*p & 0x7fff) * 0x10 + extra;
    g_voiceCursor    = 0;
    g_voiceBankLimit = g_voiceBankLimit + (p[1] & 0x7fff) * 0x10 + extra;

    // Wait for the previous voice to finish, then release its bank.
    if (g_BgmSoundBank != 0) {
        // PORT-ONLY BOUND: the original spins here with no yield, which is safe
        // in its cooperative scheduler only because getSndStat eventually stops
        // returning 1. Bounding it turns a whole-game hang into a diagnostic.
        int spin = 0;
        while (getSndStat(g_BgmSoundBank) == 1) {
            if (++spin > 100000) {
                dbg_printf("[voice] bank %d never reported finished - abandoning wait\n",
                           g_BgmSoundBank);
                break;
            }
        }
        destroySndBank(g_BgmSoundBank);
        g_BgmSoundBank = 0;
    }

    const unsigned int voiceRow = get_stage_id();
    if (voiceRow >= 8 || g_StageVoiceNamesTable[voiceRow] == NULL) {
        return;
    }

    // Port-added: a PS1-only voice clip, voice\P<row>_<id>.wav, which the asset
    // migrator writes where the PS1 plays audio at this id that no PC file
    // holds - an id sharing its name with a different clip (stage 0 ids 99
    // and 116 are both "V007_0d" and are not the same line), or an id whose
    // PC name record is empty (stage 4 ids 181/182). `row` is the first table
    // row sharing this name table, since rows 5/6 reuse rows 0/1. A PC tree
    // has no such files, so this falls straight through to the original.
    char path[260];
    unsigned int row = voiceRow;
    for (unsigned int r = 0; r < voiceRow; r++) {
        if (g_StageVoiceNamesTable[r] == g_StageVoiceNamesTable[voiceRow]) {
            row = r;
            break;
        }
    }
    sprintf(path, GAME_DATA_ROOT "voice\\P%u_%03X.wav", row, (unsigned int)id);

    if (findAndOpenFile(path) == 0) {
        const char* name = g_StageVoiceNamesTable[voiceRow] + id * 9;
        if (name[0] == '\0') {
            return;
        }

        sprintf(path, "%s%s%s", GAME_DATA_ROOT "voice\\", name, ".wav");

        if (findAndOpenFile(path) == 0) {
            dbg_printf("[voice] could not open file: %s\n", path);
            return;
        }
    }

    g_BgmSoundBank = loadSndBankFromWav(path);
    if (g_BgmSoundBank != 0) {
        pan_set(g_BgmSoundBank, 0);
        int volume = g_voiceVolume;
        // 0x0047558?: one line in stage 0 is mixed well down
        if ((id == 0x33) && (g_stageId == STAGE_MANSION_1F)) {
            volume = -300;
        }
        set_volume(g_BgmSoundBank, volume);
        SetSndSlot(g_BgmSoundBank, 0);
    }
}

// ============================================================================
// play_sound_and_voice_effect (0x00475340)
// SCD opcode 0x1E dispatches here. Type 1 starts a voice line; type 2 ends the
// current one, resets the mixer and clears the 0x20000 wait flag the script is
// blocked on. Any other type is ignored.
// ============================================================================
void play_sound_and_voice_effect(int type, int id)
{
    if (type == 1) {
        voice_load_and_play((unsigned int)id);
        return;
    }
    if (type != 2) {
        return;
    }

    for (int i = g_voiceChannelCount; i != 0; i--) {
        voice_set_pan((unsigned char)i);
    }
    voice_set_pan(0);
    voice_mixer_reset();
    g_voiceFinished = 0;
    g_main_state_flags &= ~MSF_VOICE_PLAYING;
}

// ============================================================================
// LookupFootstepZone (0x00460480)
// Scans RDT footstep_sound_zones table for the zone containing (posX, posZ).
// Returns packed byte: high = zone type, low = sound offset for that zone.
// ============================================================================
unsigned short LookupFootstepZone(short posX, short posZ) // 0x00460480
{
    // footstep_sound_zones starts with a 2-byte header, then 5-usht entries:
    //   [0]=baseX [1]=baseZ [2]=width [3]=height [4]=soundData
    unsigned short* entry = (unsigned short*)(g_RdtPointer->footstep_sound_zones + 2);

    // Scan until position falls within zone bounds.
    //
    // The original has no terminator check either - it relies on the last zone in
    // every room's table being a catch-all that always matches, so the loop is
    // guaranteed to stop. That holds only while the entity is somewhere inside the
    // room. An entity that walks out of bounds makes this scan run off the end of
    // the RDT and fault (seen crashing at 0x0140F000 with the player overshooting
    // his scripted run target).
    //
    // PORT-ONLY GUARD, no behavioural change: in correct operation the catch-all
    // matches long before the cap, so this never fires. It exists so an
    // out-of-bounds entity produces a diagnostic instead of an access violation,
    // which would otherwise mask whatever actually moved the entity out of the
    // room. Remove it only once out-of-bounds movement is impossible.
    int guard = 0;
    while ((unsigned int)entry[2] <= (int)posX - (unsigned int)entry[0] ||
           (unsigned int)entry[3] <= (int)posZ - (unsigned int)entry[1]) {
        entry += 5;
        if (++guard > 256) {
            dbg_printf("[footstep] no zone contains (%d,%d) after %d entries - "
                       "entity is outside the room\n", (int)posX, (int)posZ, guard);
            return 0;
        }
    }

    // Pack high byte of height field and low byte of sound data
    return (unsigned short)(((entry[3] >> 8) & 0xFF) << 8 | (entry[4] & 0xFF));
}

// ============================================================================
// PlayEntitySnd (0x0047fbf0)
// Plays a 3D positioned entity sound effect. The base sound type (0-2) is
// offset by the current room's footstep zone data. If a special state flag
// is set, uses a fixed offset instead. Sound is spatially positioned using
// Calc3DSndPan.
// ============================================================================
void PlayEntitySnd(unsigned char soundType) // 0x0047fbf0
{
    if (soundType >= 3) return;

    if (((g_main_state_flags & MSF_DOOR_TRANSITION) == 0) ||
        (g_playerEntity.posY != (unsigned short)0xF8F8)) {
        // Normal path: look up zone-based sound offset
        unsigned short zoneData = LookupFootstepZone(
            (short)ENTITY->scaMatrixData.localMatrix.t[0], (short)ENTITY->scaMatrixData.localMatrix.t[2]);
        char zoneOffset = (char)(zoneData & 0xFF);

        // Add input modifier: if g_main_state_flags2 bit 0 is set, add 0xFD (suppresses/wraps sound)
        unsigned int inputMod = (((g_main_state_flags2 & MSF2_EFFECT_ZONE) == 0) - 1U) & 0xFD;
        soundType = (unsigned char)((int)soundType + (int)zoneOffset + (int)inputMod);
    } else {
        // Special state: fixed offset 0x23
        soundType = soundType + 0x23;
    }

    if (soundType >= 0x30) {
        dbg_printf("[entsnd] type=%u out of range (>=0x30) - no sound\n",
                   (unsigned int)soundType);
        return;
    }

    Calc3DSndPan((VECTOR*)ENTITY->scaMatrixData.localMatrix.t);

    int handle = g_emSndBanks[(unsigned int)soundType * 2];

    if (handle != 0) {
        int volume = CalcPanVolume(g_snd_pan_right, g_snd_pan_left);
        set_volume(handle, volume);
        SetSndSlot(handle, 0);
    }
}

// ============================================================================
// Snd_em (0x0047fca0)
// Plays a 3D positioned enemy sound effect using the current entity's
// translation vector. Enemy sound IDs are offset by the entity's
// sound group (bits 4-6 of field_0x161) * 10.
// ============================================================================
void Snd_em(unsigned char em_snd_id) // 0x0047fca0
{
    if (em_snd_id >= 10) return;

    // `MOV AL, byte ptr [EAX + 0x161]` at 0x0047fcb2 - the sound-bank GROUP is
    // in the high nibble of entity+0x161, which cmd_enemy_set fills from the SCD
    // enemy record's byte 0x15 (`OR byte ptr [EAX+0x161],CL` after `SHL CL,4`).
    // The old code read entity+0x10, part of the model/SCA header: whatever bits
    // happened to sit in 0x70 there scaled the id by 10 and pushed it past the
    // 47-record table, so Snd_em bailed out and no enemy ever made a sound.
    em_snd_id = em_snd_id + ((ENTITY->pad_160[1] & 0x70) >> 4) * 10;
    if (em_snd_id >= 48) return;

    Calc3DSndPan((VECTOR*)ENTITY->scaMatrixData.localMatrix.t);

    int handle = g_emSndBanks[em_snd_id * 2];
    if (handle != 0) {
        pan_set(handle, (g_snd_pan_right - g_snd_pan_left) * 0x4E);
        SetSndSlot(g_emSndBanks[em_snd_id * 2], 0);
    }
}

// collision_flag_set / collision_push_rect / collision_push_circle and
// Room_SetupCollisionCallbacks now live in RoomCollision.cpp, with the rest of
// the room boundary system. They were only ever here because the original's
// callback table sits next to the sound bank globals.

// Load one enemy-sound record from ./sound/<name>.wav. `slot` is a record index
// into g_emSndBanks (two ints per record). The body of Room_LoadEnemySoundBanks'
// loop, shared with the arrange-room override pass at the end of it. A NULL
// filename leaves the slot empty, which is how a slot gets cleared.
static void emsnd_load_slot(int slot, const char* filename)
{
    int* piVar7 = &g_emSndBanks[slot * 2];

    if (*piVar7 != 0) {
        destroySndBank(*piVar7);
    }
    *piVar7 = 0;
    *((unsigned char*)(piVar7 + 1)) = 0;
    *((unsigned char*)(piVar7 + 1) + 1) = 0;

    if (filename == NULL) return;

    char path[260];
    _snprintf(path, sizeof(path), GAME_DATA_ROOT "sound\\%s.wav", filename);
    path[sizeof(path) - 1] = '\0';
    findAndOpenFile(path);

    int bank = loadSndBankFromWav(path);
    *piVar7 = bank;
    if (bank != 0) {
        pan_set(bank, 0);
        set_volume(bank, g_EnemySndVolume);
    }
}

// ============================================================================
// Room_LoadEnemySoundBanks (0x0047eed0)
// Loads per-room enemy sound banks. Iterates through g_emSndBanks, destroys
// existing banks, and loads WAV files from ./usa/sound/<name>.wav using the
// g_RoomSoundNameTable lookup table (indexed by stageId * 29 + roomId).
//
// The lookup is bounds-checked because the table used to be short. It was mined
// as 145 rows (5 stages), but the real one at 0x004cfae0 is 203 - the pointers
// run contiguously with a 0xC0 stride to 0x004cfa20 and the first NULL is at
// index 203 = 7 stages x 29. Stage 5 room 26 (room61A0) is index 171, so it read
// a garbage row pointer, then a garbage char* out of it, and the sprintf below
// copied whatever that pointed at into a 260-byte stack buffer until it hit a
// NUL - smashing the frame, which is why the fault landed on `*piVar7 = bank`
// with piVar7 = 0xB9E381AF rather than anywhere near the actual bug. The table
// is complete now; the guard and the snprintf are here so a future short table
// reports itself instead of corrupting the stack.
// ============================================================================
void Room_LoadEnemySoundBanks(void) {
    int iVar6 = 0;
    int* piVar7 = g_emSndBanks;

    // Base row: an arrange room is the same physical room as its base stage's,
    // so it takes that room's enemy bank list. That is right for the footsteps,
    // the doors and the props, and wrong for everything ADVANCED changed - the
    // per-room override table at the end of this function puts those right.
    const unsigned int roomIdx = get_stage_id() * 29 + (unsigned int)g_roomId;
    const int roomIdxValid =
        roomIdx < sizeof(g_RoomSoundNameTable) / sizeof(g_RoomSoundNameTable[0]);
    if (!roomIdxValid) {
        dbg_printf("[emsnd] stage %u room %u -> index %u is past the %u-row sound"
                   " name table; loading no entity SFX for this room\n",
                   (unsigned int)g_stageId, (unsigned int)g_roomId, roomIdx,
                   (unsigned int)(sizeof(g_RoomSoundNameTable)
                                  / sizeof(g_RoomSoundNameTable[0])));
    }

    do {
        // Look up per-room sound name table
        const char** soundTable = roomIdxValid ? g_RoomSoundNameTable[roomIdx] : NULL;
        emsnd_load_slot(iVar6 / 4, soundTable ? soundTable[iVar6 / 4] : NULL);

        iVar6 += 4;
        piVar7 += 2;
        // The original bounds this with an absolute address:
        //   piVar7 walks from g_emSndBanks (0x00ac99f0) in 8-byte steps and the
        //   loop ends once piVar7 > 0xac9b6f, which is records 0 through 47.
        // &g_emSndBanks[48] is only base+192 bytes - element 48 of an int array,
        // not record 48 - so this stopped after 25 records and left every enemy
        // and footstep sound from id 25 up with a null bank. That is why no SFX
        // played: PlayEntitySnd looks up g_emSndBanks[soundType * 2], finds 0 and
        // silently returns.
    } while (piVar7 < (int*)&g_emSndBanks[96]);

    // The row above is the base room's, which is right for everything the room
    // shares and wrong for everything the DC changed: a swapped enemy, a cue
    // only ADVANCED uses, a sound the arrange room drops, or an enemy the PC
    // row never covered at all. Each of those slots is read out of the RDT's
    // own embedded sound bank; see dc/ArrangeSoundRows.h. A NULL name is
    // meaningful - it CLEARS the slot.
    //
    // DC_SND_ARRANGE entries need the arrange file to be the one loaded, since
    // the base room's row is correct without it. DC_SND_ANY_DC entries belong
    // to rooms with no arrange file at all - same RDT in every mode, so they
    // apply throughout DC mode.
    if (g_bDcMode) {
        const int arrangeLoaded = room_file_stage() >= STAGE_ARRANGE_FIRST;
        int extra = 0, applied = 0;
        const DcArrangeSndSlot* list = dc_arrange_snd_slots(&extra);
        for (int i = 0; i < extra; i++) {
            if (list[i].stage != g_stageId || list[i].room != g_roomId ||
                list[i].slot >= 48) {
                continue;
            }
            if (list[i].scope == DC_SND_ARRANGE && !arrangeLoaded) {
                continue;
            }
            emsnd_load_slot(list[i].slot, list[i].name);
            applied++;
        }
        // A silent room is either "the table has no row for it" or "the row is
        // there and the sound still did not play"; those need different fixes,
        // so say which.
        if (applied || arrangeLoaded) {
            dbg_printf("[emsnd] DC stage %u room %u (%s): %d slot override(s)\n",
                       (unsigned int)g_stageId, (unsigned int)g_roomId,
                       arrangeLoaded ? "arrange" : "base file", applied);
        }
    }
}

// (0x0047f870) - 3-param play_sfx overload (mode parameter)
void play_sfx(int bank, int soundId, int mode) { play_sfx(bank, soundId); }

// ============================================================================
// BGM channel helpers (moved here from GameState.cpp)
// ============================================================================

// ============================================================================
// FUN_004804a0 (0x004804a0) - Start a volume ramp on one BGM channel
// param1 is unused by the original. param2 is the channel index and IS bounds
// checked (`param_2 < 3`, signed) before touching g_SndBank. SCD opcode 0x43.
// NOTE: the original divides by param4 with no zero check, so a script passing
// 0 there would fault. Reproduced faithfully.
// ============================================================================
void FUN_004804a0(short param1, unsigned int param2, short param3, unsigned int param4)
{
    (void)param1;
    if ((short)param2 < 3 && g_SndBank[param2].handle != 0) {
        g_SndRampBankIndex   = (short)param2;
        g_SndRampDirection   = (short)((int)param3 / (int)param4) * 0x4E;
        g_SndRampFramesLeft  = (int)param4 * 2;
    }
}

// ============================================================================
// FUN_004805d0 (0x004805d0) - snd_set_channel_pan_volume
// Applies a pan/volume pair to one BGM channel. param1 is unused by the original.
// param2 is the channel index (indexed as [EAX*0x8 + g_SndBank], i.e. a record
// index into SndBankSlot[3]). SCD opcode 0x2F.
// ============================================================================
void FUN_004805d0(short param1, unsigned int param2, unsigned int param3, unsigned int param4)
{
    (void)param1;   // pushed by callers, never read by the original
    int handle = g_SndBank[param2].handle;
    if (handle != 0) {
        set_volume(handle, CalcPanVolume((int)(short)param3, (int)(short)param4));
    }
}

// ============================================================================
// BuildSndFadeTbl (0x0047ff90)
// For each of the 3 BGM channels, computes how many g_SndDistSteps-sized volume
// steps it takes to drive that channel from its current volume down to inaudible
// (-10000), and stores the count in g_SndFadeStepTbl (clamped at 0).
// Parameter 1 is the distance-step count (scaled by 0x4E), parameter 2 the fade
// type - the previous stub had these names inverted. SCD opcode 0x27 passes
// (op >> 8, 0x7F).
// NOTE: divides by g_SndDistSteps with no zero check, exactly as the original.
// ============================================================================
void BuildSndFadeTbl(char distSteps, int fadeType)
{
    DAT_00ac98f8   = 0;
    g_SndDistSteps = distSteps * 0x4E;
    g_SndFadeType  = (unsigned char)fadeType;

    for (int i = 0; i < 3; i++) {
        if (g_SndBank[i].handle == 0) {
            g_SndFadeStepTbl[i] = 0;
        } else {
            g_SndFadeStepTbl[i] = (-10000 - getSndVol(g_SndBank[i].handle)) / g_SndDistSteps;
        }
        if (g_SndFadeStepTbl[i] < 0) {
            g_SndFadeStepTbl[i] = 0;
        }
    }
}

