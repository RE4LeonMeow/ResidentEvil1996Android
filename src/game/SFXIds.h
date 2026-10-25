// SFXIds.h - Named constants for all sound effect IDs
// Each bank is loaded via LoadSoundBank(bankId) which populates g_SfxBanks[]
// from the corresponding g_SoundBanksTable[bankId] sub-table.
// The SFX ID passed to play_sfx(1, id) is the 0-based index into that sub-table.
#pragma once

#define SFX_BANKS       1

// ============================================================================
// Sound Bank IDs (passed to LoadSoundBank or used as bank display index)
// ============================================================================
#define BANK_KNIFE      0
#define BANK_KNIFE2     1
#define BANK_GUN        2
#define BANK_SHOTGUN    3
#define BANK_MAGNUM     4
#define BANK_MAGNUM2    5
#define BANK_FLAME      6
#define BANK_GRENADE    7
#define BANK_ACID       8
#define BANK_FIRE       9
#define BANK_ROCKET     10
#define BANK_BIO        11
#define BANK_TITLE      12
#define BANK_SELECT     13
#define BANK_ENDING     14
#define BANK_WIN95      15

// ============================================================================
// Bank 0/1 — Knife (g_st0)
// ============================================================================
#define SFX_KNIFE_KNIFE01  0
#define SFX_KNIFE_KNIFE02  1
#define SFX_KNIFE_KNIFE03  2

// ============================================================================
// Bank 2 — Gun (g_st2)
// ============================================================================
#define SFX_GUN_GUN04  5
#define SFX_GUN_GUN01  7
#define SFX_GUN_GUN02  9
#define SFX_GUN_GUN03  10

// ============================================================================
// Bank 3 — Shotgun (g_st3)
// ============================================================================
#define SFX_SHOTGUN_SHOT01  7
#define SFX_SHOTGUN_SHOT04  9
#define SFX_SHOTGUN_SHOT02  10
#define SFX_SHOTGUN_SHOT03  11

// ============================================================================
// Bank 4/5 — Magnum (g_st4)
// ============================================================================
#define SFX_MAGNUM_MAGNUM01  7
#define SFX_MAGNUM_MAGNUM02  9
#define SFX_MAGNUM_MAGNUM03  10

// ============================================================================
// Bank 6 — Flame (g_st6)
// ============================================================================
#define SFX_FLAME_FLAME01  3
#define SFX_FLAME_FLAME02  4
#define SFX_FLAME_FLAME03  9

// ============================================================================
// Bank 7 — Grenade Bomb (g_st7)
// ============================================================================
#define SFX_GRBOMB_GRBOMB01  5
#define SFX_GRBOMB_GRBOMB02  7
#define SFX_GRBOMB_GRBOMB04  9
#define SFX_GRBOMB_GRBOMB03  12

// ============================================================================
// Bank 8 — Grenade Acid (g_st8)
// ============================================================================
#define SFX_GRACID_GRACID01  5
#define SFX_GRACID_GRACID02  7
#define SFX_GRACID_GRACID04  9
#define SFX_GRACID_GRACID03  13

// ============================================================================
// Bank 9 — Grenade Fire (g_st9)
// ============================================================================
#define SFX_GRFIRE_GRFIRE01  5
#define SFX_GRFIRE_GRFIRE02  7
#define SFX_GRFIRE_GRFIRE04  9
#define SFX_GRFIRE_GRFIRE03  14

// ============================================================================
// Bank 10 — Rocket (g_st10)
// ============================================================================
#define SFX_ROCKET_ROCKET01  7
#define SFX_ROCKET_ROCKET02  9
#define SFX_ROCKET_ROCKET03  15

// ============================================================================
// Bank 11 — Bio (g_st11)
// ============================================================================
#define SFX_BIO_BIO01    0
#define SFX_BIO_CANCEL   13
#define SFX_BIO_TYPE01   14
#define SFX_BIO_TYPE02   15

// ============================================================================
// Bank 12 — Evil / Title screen (g_st12)
// ============================================================================
#define SFX_TITLE_EVIL01  0
#define SFX_TITLE_CANCEL  13
#define SFX_TITLE_TYPE01  14
#define SFX_TITLE_TYPE02  15

// ============================================================================
// Bank 13 — Select (g_st13)
// ============================================================================
#define SFX_SELECT_SELECT06  0
#define SFX_SELECT_SELECT05  1

// ============================================================================
// Bank 14 — Ending (g_st14)
// ============================================================================
#define SFX_ENDING_ENDING07  0
#define SFX_ENDING_ENDING06  2
#define SFX_ENDING_CANCEL    13
#define SFX_ENDING_TYPE01    14
#define SFX_ENDING_TYPE02    15

// ============================================================================
// Bank 15 — Win95 / Misc (g_st15)
// ============================================================================
#define SFX_WIN95_WIN95_MG  7
#define SFX_WIN95_A_MCN03   10
