#pragma once

// Voice offset data per stage (uint16 entries, indexed by voice clip ID)
// Bit 15 = extra entry flag (entry consumes 2 uint16s), bits 0-14 = offset index (*16)
extern const unsigned short* g_StageVoiceOffsetTable[8]; // 0x004b3288

// Voice filename data per stage (9-byte records: 7-8 char name + null + pad)
// Each stage's pointer points to an array of 9-byte records indexed by room ID
extern const char* g_StageVoiceNamesTable[8]; // 0x004b2d88

// Room BGM tables (see SoundTables.cpp section 4). Read by bgm_load_and_start;
// g_bgmDataTable points at g_BgmRoomData and indexes it as a flat byte array.
extern const char* const   g_BgmNameTable[57][4];   // 0x004d07b8 -> 0x004d0428
extern const unsigned char g_BgmLoopTable[57][4];   // 0x004d0980 -> 0x004d089c
extern const unsigned char g_BgmRoomData[7][32][4]; // 0x004d0c30

// Per-room enemy sound name table (indexed by stageId * 29 + roomId)
// Each entry points to an array of const char* sound name strings (or NULL)
// Up to 48 entries per room, accessed as soundTable[iVar6 / 4] where iVar6 = 0..96
extern const char** g_RoomSoundNameTable[203]; // 0x004cfae0 - 7 stages x 29 rooms
