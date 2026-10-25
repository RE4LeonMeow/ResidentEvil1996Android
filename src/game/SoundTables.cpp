#include "SoundTables.h"
#include "../platform/types.h"

// ============================================================
// Section 1: Voice Offset Data Tables (5 stages)
// ============================================================

// Stage 0 (0x004b2da8): 152 entries
static const unsigned short g_VoiceOffsetData_Stage0[152] = { // 0x004b2da8
    0x0000, 0x0016, 0x0050, 0x0060, 0x00A7, 0x00C1, 0x00DB, 0x00FF, 0x0122, 0x814A, 0x015E,
    0x0000, 0x0026, 0x005D, 0x00CA, 0x810B, 0x0155,
    0x0000, 0x0030, 0x0043, 0x009E, 0x00C0, 0x00DB, 0x010C, 0x813A, 0x015D,
    0x0000, 0x006A, 0x0086, 0x00BE, 0x80E2, 0x0116,
    0x0000, 0x008E, 0x00AC, 0x00C1, 0x00E2, 0x0103, 0x813D, 0x015A,
    0x0000, 0x002C, 0x0041, 0x0062, 0x0082, 0x00AD, 0x00C0, 0x00FA, 0x0115, 0x8143, 0x0166,
    0x0000, 0x0013, 0x002C, 0x004F, 0x0062, 0x0088, 0x00B2, 0x00D4, 0x00E5, 0x00F0, 0x813C, 0x0181,
    0x0000, 0x004C, 0x8066, 0x00B2, 0x8000, 0x018B,
    0x0000, 0x0029, 0x0066, 0x009D, 0x00BF, 0x00EE, 0x00FC, 0x010B, 0x0116, 0x0138, 0x8158, 0x0164,
    0x0000, 0x0011, 0x002B, 0x0044, 0x0062, 0x006A, 0x0083, 0x008D, 0x009B, 0x00BD, 0x00CE, 0x00DF, 0x00F2, 0x0102, 0x012A, 0x814B, 0x0169,
    0x0000, 0x000B, 0x001A, 0x0027, 0x003C, 0x0067, 0x0093, 0x009F, 0x00AA, 0x00B6, 0x00F1, 0x0100, 0x010B, 0x0129, 0x813F, 0x0169,
    0x0000, 0x0020, 0x0081, 0x00A7, 0x00C6, 0x00D6, 0x0106, 0x8133, 0x0158,
    0x0000, 0x0021, 0x0035, 0x0063, 0x00A1, 0x00CA, 0x0124, 0x8141, 0x0157,
    0x0000, 0x0032, 0x0040, 0x008D, 0x009F, 0x00B7, 0x00D2, 0x00F8, 0x8121, 0x015B,
    0x0000, 0x0023, 0x002C, 0x007C, 0x0092, 0x016B, 0x824C, 0x026A,
    0x0000, 0x0000
};

// Stage 1 (0x004b2ed8): 112 entries
static const unsigned short g_VoiceOffsetData_Stage1[112] = { // 0x004b2ed8
    0x0000, 0x000A, 0x0027, 0x0056, 0x008C, 0x80B4, 0x00C5,
    0x0000, 0x0047, 0x005F, 0x006A, 0x00A4, 0x00B3, 0x80D2, 0x00DE,
    0x0000, 0x002B, 0x0057, 0x0062, 0x0086, 0x0096, 0x80B2, 0x00DA,
    0x0000, 0x0025, 0x0056, 0x0067, 0x008F, 0x80C8, 0x00D5,
    0x0000, 0x0027, 0x0036, 0x0040, 0x005A, 0x0062, 0x0085, 0x80B7, 0x00D2,
    0x0000, 0x8016, 0x00EF, 0x8000, 0x00E1,
    0x0000, 0x000C, 0x0047, 0x008D, 0x80A0, 0x00DA,
    0x0000, 0x002E, 0x0058, 0x0068, 0x0076, 0x8096, 0x00B0,
    0x0000, 0x0037, 0x0064, 0x008A, 0x80B3, 0x0101,
    0x0000, 0x001F, 0x005D, 0x0075, 0x0085, 0x8095, 0x00AD,
    0x0000, 0x0042, 0x0066, 0x009A, 0x80C5, 0x00D6,
    0x0000, 0x0024, 0x0041, 0x004D, 0x007F, 0x80BB, 0x00CF,
    0x0000, 0x002E, 0x003E, 0x0058, 0x0071, 0x0096, 0x00B9, 0x80BF, 0x00EE,
    0x0000, 0x0007, 0x0026, 0x0039, 0x006C, 0x007E, 0x00A9, 0x80B8, 0x00EF,
    0x0000, 0x0026, 0x0040, 0x0056, 0x006F, 0x0092, 0x00A7, 0x80E3, 0x0101,
    0x0000, 0x0000
};

// Stage 2 (0x004b2fb8): 68 entries
static const unsigned short g_VoiceOffsetData_Stage2[68] = { // 0x004b2fb8
    0x0000, 0x0017, 0x004D, 0x8075, 0x0097,
    0x0000, 0x0038, 0x0060, 0x8090, 0x00CA,
    0x0000, 0x0036, 0x8073, 0x00C7,
    0x0000, 0x0017, 0x0032, 0x005A, 0x0093, 0x80B0, 0x00C4,
    0x0000, 0x0025, 0x0042, 0x806D, 0x00BC,
    0x0000, 0x0019, 0x8084, 0x00BB,
    0x0000, 0x0031, 0x808A, 0x00B7,
    0x0000, 0x005F, 0x808F, 0x00E9,
    0x0000, 0x0052, 0x805D, 0x00C1,
    0x0000, 0x0010, 0x0053, 0x807D, 0x00CF,
    0x0000, 0x0013, 0x0077, 0x808D, 0x00D0,
    0x0000, 0x0033, 0x006A, 0x80B0, 0x00C5,
    0x8000, 0x0050, 0x8000, 0x026C,
    0x0000, 0x8011, 0x0073, 0x8000, 0x0010,
    0x0000, 0x0000
};

// Stage 3 (0x004b3040): 88 entries
static const unsigned short g_VoiceOffsetData_Stage3[88] = { // 0x004b3040
    0x8000, 0x00F6,
    0x0000, 0x000F, 0x0030, 0x8070, 0x00BF,
    0x0000, 0x8074, 0x00B2,
    0x0000, 0x0026, 0x0030, 0x0038, 0x808D, 0x0097,
    0x0000, 0x0064, 0x807E, 0x00AC,
    0x0000, 0x0045, 0x005D, 0x006F, 0x0091, 0x80A0, 0x00DF,
    0x0000, 0x0021, 0x003C, 0x8094, 0x00AB,
    0x0000, 0x003B, 0x0076, 0x0095, 0x80BA, 0x00CC,
    0x0000, 0x000A, 0x0024, 0x8067, 0x00D4,
    0x0000, 0x000A, 0x0012, 0x0025, 0x0038, 0x8051, 0x00A4,
    0x0000, 0x8059, 0x00BA,
    0x0000, 0x0065, 0x006F, 0x007A, 0x0085, 0x809D, 0x00D6,
    0x0000, 0x005A, 0x8080, 0x00A9,
    0x0000, 0x0038, 0x0058, 0x0078, 0x8087, 0x00CE,
    0x0000, 0x0029, 0x0040, 0x004B, 0x8095, 0x00BB,
    0x0000, 0x001F, 0x002F, 0x005F, 0x0072, 0x0097, 0x00B8, 0x00CC, 0x80FA, 0x0110,
    0x0000, 0x0000
};

// Stage 4 (0x004b30f0): 204 entries
static const unsigned short g_VoiceOffsetData_Stage4[204] = { // 0x004b30f0
    0x0000, 0x0008, 0x0043, 0x0067, 0x009C, 0x00A6, 0x00B4, 0x00C2, 0x00EC, 0x0112, 0x013B, 0x0157, 0x0162, 0x0174, 0x01A4, 0x01D3, 0x01F1, 0x01FC, 0x8204, 0x020D,
    0x0000, 0x0061, 0x00BF, 0x8161, 0x01C8,
    0x0000, 0x0077, 0x00C9, 0x0137, 0x8183, 0x0227,
    0x0000, 0x0029, 0x0061, 0x006D, 0x009E, 0x0100, 0x0165, 0x01A0, 0x01C8, 0x01D3, 0x01E9, 0x81F3, 0x0217,
    0x0000, 0x0014, 0x0025, 0x00DF, 0x010A, 0x0129, 0x0157, 0x0168, 0x017F, 0x819D, 0x01F2,
    0x0000, 0x0052, 0x0096, 0x00B0, 0x00BA, 0x00E4, 0x00ED, 0x0131, 0x014D, 0x0176, 0x018C, 0x01AC, 0x01C8, 0x01D6, 0x81ED, 0x0211,
    0x0000, 0x0022, 0x007B, 0x00E8, 0x00F5, 0x0106, 0x0117, 0x0127, 0x015D, 0x0193, 0x01B1, 0x81FB, 0x0230,
    0x0000, 0x0022, 0x002E, 0x0052, 0x006B, 0x00C9, 0x00F4, 0x00FF, 0x812A, 0x01AC,
    0x0000, 0x0111, 0x0125, 0x0145, 0x0152, 0x0163, 0x0174, 0x0180, 0x0190, 0x01A6, 0x81F0, 0x0209,
    0x0000, 0x0037, 0x0050, 0x00B7, 0x00EC, 0x011A, 0x0198, 0x01B2, 0x01BB, 0x81E3, 0x0218,
    0x0000, 0x007B, 0x0097, 0x00EB, 0x0100, 0x0164, 0x017F, 0x01A4, 0x01B6, 0x01C0, 0x81CF, 0x023D,
    0x0000, 0x0028, 0x0061, 0x0068, 0x00AD, 0x0101, 0x0125, 0x0141, 0x0173, 0x018E, 0x019D, 0x01B1, 0x01D9, 0x81E7, 0x01FC,
    0x0000, 0x0070, 0x00A2, 0x00D1, 0x015C, 0x0176, 0x01C5, 0x81D8, 0x0228,
    0x0000, 0x001E, 0x0044, 0x0051, 0x007B, 0x008A, 0x00A0, 0x00DB, 0x00ED, 0x00FB, 0x015B, 0x0186, 0x01AF, 0x01E0, 0x01E7, 0x81FD, 0x020E,
    0x0000, 0x0022, 0x0047, 0x0081, 0x009A, 0x00C6, 0x00EF, 0x010C, 0x0123, 0x014B, 0x0163, 0x017D, 0x018E, 0x81D7, 0x01EA,
    0x0000, 0x005E, 0x006F, 0x00EB, 0x0110, 0x0149, 0x0169, 0x0197, 0x01AF, 0x01DE, 0x0205, 0x0263, 0x026B, 0x0275, 0x02C3, 0x02C9, 0x82D3, 0x02F9,
    0x0000
};

// 0x004b3288 - Per-stage voice offset data pointers (indexed by g_stageId)
const unsigned short* g_StageVoiceOffsetTable[8] = {
    g_VoiceOffsetData_Stage0, g_VoiceOffsetData_Stage1, g_VoiceOffsetData_Stage2,
    g_VoiceOffsetData_Stage3, g_VoiceOffsetData_Stage4, g_VoiceOffsetData_Stage0,
    g_VoiceOffsetData_Stage1, (const unsigned short*)0x30
};

// ============================================================
// Section 2: Voice Name Tables
// ============================================================
// Voice filename data per stage (9 bytes per record: 7-char name + null + pad)
// Each stage's pointer points to an array of 9-byte records indexed by room ID
// 0x004b2d88: PTR_s_V001_00_004b2d88

// 0x004b1aa8 - Stage 0 voice filename records (9 bytes each) - Main mansion
static const char g_VoiceNameData_Stage0[][9] = {
    "V001_00", "V001_01", "V001_02", "V001_03", "V001_04", "V001_05", "V001_06",
    "V003_00", "V003_01",
    "V004_00", "V004_01", "V004_02", "V004_03", "V004_04", "V004_05", "V004_06",
    "V004_07", "V004_08", "V004_09", "V004_0a", "V004_0b", "V004_0c", "V004_0d",
    "V004_0e", "V004_0f",
    "V101_00", "V101_01", "V101_02", "V101_03", "V101_04",
    "V102_00", "V102_01", "V102_02", "V102_03", "V102_04", "V102_05", "V102_06",
    "V102_07", "V102_08", "V102_09", "V102_0a", "V102_0b", "V102_0c", "V102_0d",
    "V102_0e", "V102_0a", "V102_10", "V102_0e",
    "V105_00", "V105_01", "V105_02", "V105_03", "V105_04", "V105_05", "V105_06",
    "V105_07", "V105_08", "V105_09",
    "V103_00", "V103_01", "V103_0a", "V103_0b", "V103_0c", "V103_0d",
    "V107_00",
    "V006_00", "V006_01", "V006_02", "V006_03", "V006_04", "V006_05", "V006_06",
    "V006_07", "V006_08", "V006_09", "V006_0a", "V006_0b", "V006_0c", "V006_0d",
    "V006_0e", "V006_0f", "V006_10", "V006_11", "V006_12",
    "V00C_00", "V00C_01", "V00C_10", "V00C_11", "V00C_12", "V00C_13", "V00C_14",
    "V00C_15", "V00C_16", "V00C_17", "V00C_18", "V00C_19", "V00C_1a", "V00C_1b",
    "V00C_1c",
    "V007_0d",
    "V00C_20", "V00C_21", "V00C_22",
    "V007_00", "V007_01", "V007_02", "V007_03", "V007_04", "V007_05", "V007_06",
    "V007_07", "V007_08", "V007_09", "V007_0a", "V007_0b", "V007_0c", "V007_0d",
    "V007_0e", "V007_0f",
    "V00B_00", "V00B_01",
    "V008_00", "V008_01", "V008_02", "V008_03", "V008_04", "V008_05", "V008_06",
    "VA04_00", "VA04_01",
    "V106_00", "V106_01", "V106_02",
    "VA00_00"
};

// 0x004b1f60 - Stage 1 voice filename records (9 bytes each) - Dormitory
static const char g_VoiceNameData_Stage1[][9] = {
    "V104_00", "V104_01", "V104_02", "V104_03", "V104_04", "V104_05", "V104_06",
    "V104_07", "V104_08", "V104_09", "V104_0a",
    "V00C_30", "V00C_31",
    "VA06_00", "VA07_00",
    "V008_10", "V008_11", "V008_12", "V008_13", "V008_14", "V008_15", "V008_16",
    "V008_17", "V008_18", "V008_19",
    "V108_00", "V108_01", "V108_02", "V108_03", "V108_04", "V108_05", "V108_06",
    "V108_07", "V108_08",
    "V106_00", "V106_01", "V106_02",
    "V005_00", "V005_01", "V005_02", "V005_03", "V005_04", "V005_05", "V005_06",
    "V005_07", "V005_08", "V005_09", "V005_0a", "V005_0b", "V005_0c", "V005_0d",
    "V005_0e", "V005_0f", "V005_10", "V005_11", "V005_12", "V005_13", "V005_14",
    "VA01_00", "VA01_01", "VA01_02", "VA01_03", "VA01_04", "VA01_05", "VA01_06",
    "VA01_07", "VA01_08", "VA01_09", "VA01_0a", "VA01_0b", "VA01_0c", "VA01_0d",
    "V10D_00", "V10D_01", "V10D_02", "V10D_03", "V10D_04", "V10D_05", "V10D_06",
    "V10D_07", "V10D_10", "V10D_11", "V10D_12", "V10D_13", "V10D_14", "V10D_15",
    "V10D_16", "V10D_17", "V10D_18", "V10D_19",
    "VB00_00", "VB00_01", "VB00_02",
    "VA00_00"
};

// 0x004b22b0 - Stage 2 voice filename records (9 bytes each) - Tunnel
static const char g_VoiceNameData_Stage2[][9] = {
    "V00D_00", "V00D_01", "V00D_02", "V00D_03", "V00D_04", "V00D_05",
    "V10F_00", "V10F_02", "V10F_04", "V10F_05", "V10F_06", "V10F_07", "V10F_08",
    "V10F_10", "V10F_12", "V10F_15", "V10F_16", "V10F_17", "V10F_18", "V10F_19",
    "V10F_1a", "V10F_1b",
    "V10E_00", "V10E_01", "V10E_02", "V10E_03", "V10E_04", "V10E_05", "V10E_06",
    "V10E_07",
    "V009_00", "V009_01", "V009_02", "V009_03", "V009_04", "V009_05",
    "V009_00", "VA05_01", "V009_02", "VA05_03", "V009_04", "VA05_05",
    "V016_05", "V11B_03",
    "VB00_30", "VB00_31a",
    "V110_00", "V110_01", "VA00_01", "V10E_08"
};

// 0x004b2478 - Stage 3 voice filename records (9 bytes each) - Laboratory
static const char g_VoiceNameData_Stage3[][9] = {
    "V109_00", "V109_10", "V109_11", "V109_12", "V109_13", "V109_14", "V109_15",
    "V00A_00", "V00A_01", "V00A_10", "V00A_11", "V00A_12", "V00A_13", "V00A_14",
    "V00A_20", "V00A_21", "V00A_22", "V00A_23", "V00A_24", "V00A_25", "V00A_26",
    "V00A_27", "V00A_28", "V00A_29", "V00A_2a", "V00A_2b", "V00A_2c",
    "V10C_00", "V10C_01", "V10C_02", "V10C_03", "V10C_04", "V10C_08", "V10C_06",
    "V10C_07",
    "VA02_00", "VA02_01", "VA02_02", "VA02_03", "VA02_04", "VA02_05", "VA02_06",
    "VA02_07", "VA02_08", "VA02_09",
    "V10A_00", "V10A_01", "V10A_02", "V10A_03", "V10A_04", "V10A_05", "V10A_06",
    "V10A_07", "V10A_08", "V10A_09", "V10A_0a", "V10A_0b", "V10A_0c",
    "VA00_02",
    "VA08_00", "VA08_01", "VA08_02", "VA08_03", "VA08_04", "VA08_05", "VA08_06",
    "VA08_07", "VA08_08", "VA08_09", "VA08_0a"
};

// 0x004b26f0 - Stage 4 voice filename records (9 bytes each) - Guardhouse+extra
static const char g_VoiceNameData_Stage4[][9] = {
    "VA09_00", "VA09_01", "VA09_02", "VA09_03", "VA09_04", "VA09_05", "VA09_06",
    "VA09_07", "VA09_08", "VA09_09", "VA09_0a", "VA09_0b",
    "V118_00", "V118_01",
    "V00F_00", "V00F_01", "V00F_02", "V00F_10", "V00F_11", "V00F_12",
    "V11B_00", "V11B_01", "V11B_02",
    "V016_00", "V016_01", "V016_02", "V016_03", "V016_04",
    "V012_00", "V012_01", "V012_02", "V012_03", "V012_04", "V012_05", "V012_06",
    "V012_07", "V012_08", "V012_09", "V012_0a", "V012_0b", "V012_0c", "V012_0d",
    "V012_0e", "V012_0f", "V012_10", "V012_11", "V012_12",
    "V011_00", "V011_01", "V011_02", "V011_03", "V011_04", "V011_05", "V011_06",
    "V011_07", "V011_08", "V011_09", "V011_0a",
    "V015_00", "V015_01", "V015_02", "V015_03", "V015_04", "V015_05", "V015_06",
    "V015_07",
    "V119_00", "V119_01",
    "VA00_03", "VA00_04", "VA00_05",
    "V013_00", "V013_01", "V013_02", "V013_03", "V013_04", "V013_05", "V013_06",
    "V013_07", "V013_08", "V013_09",
    "VB00_11", "V016_06",
    "V11A_00", "V11A_01", "V11A_02", "V11A_03", "V11A_04", "V11A_05",
    "VA00_03", "VA00_04", "VA00_05",
    // 35 entries, not 29: the run goes all the way to V115_22. It was cut at
    // V115_1c (a suspiciously round 29), and v115_1d..v115_22.wav all exist.
    "V115_00", "V115_01", "V115_02", "V115_03", "V115_04", "V115_05", "V115_06",
    "V115_07", "V115_08", "V115_09", "V115_0a", "V115_0b", "V115_0c", "V115_0d",
    "V115_0e", "V115_0f", "V115_10", "V115_11", "V115_12", "V115_13", "V115_14",
    "V115_15", "V115_16", "V115_17", "V115_18", "V115_19", "V115_1a", "V115_1b",
    "V115_1c", "V115_1d", "V115_1e", "V115_1f", "V115_20", "V115_21", "V115_22",
    "VA03_00", "VA03_01", "VA03_02", "VA03_03", "VA03_04", "VA03_05", "VA03_06",
    "VA03_07", "VA03_08", "VA03_09", "VA03_0a", "VA03_0b", "VA03_0c", "VA03_0d",
    "VA03_0e",
    "V014_00", "V014_01", "V014_02", "V014_03", "V014_04", "V014_05", "V014_06",
    "V014_07", "V014_08", "V014_09", "V014_0a",
    "V112_10", "V112_11", "V112_12", "V112_13", "V112_14", "V112_15", "V112_16",
    "V117_00", "V117_01", "V117_02", "V117_03", "V117_04", "V117_05",
    "V117_10", "V117_11", "V117_12", "V117_13",
    "VB00_11", "VA09_0b",
    "V116_00", "V116_01", "V116_02", "V116_03", "V116_04", "V116_05",
    "V116_10",
    "VB00_10", "VB00_11",
    // TWO EMPTY RECORDS. 0x004b2d4d and 0x004b2d56 are eighteen zero bytes, not
    // padding - they are ids 181 and 182 and they hold the run's indices apart.
    // Dropping them slid everything after down by two, and combined with the
    // truncated V115 run it put the whole tail eight entries out of place, so
    // the lab terminal's boot jingle (id 0xb7 = 183 = VB00_20) indexed PAST the
    // end of the array and read whatever followed in .rdata - which is where the
    // "[voice] could not open file: .\assets\USA\voice\e02.wav" came from.
    "",        "",
    "VB00_20", "VB00_21", "VB00_22", "VB00_40"
};

// 187 records (0..186) in the original, ending at 0x004b2d7a + 9 with five bytes
// of padding before the pointer table at 0x004b2d88. Locked down because the
// highest id any caller uses (0xb7) is only three short of the end: a truncation
// here does not fail loudly, it silently reads adjacent .rdata as a filename.
static_assert(sizeof(g_VoiceNameData_Stage4) / 9 == 187,
              "stage 4 voice name table must hold 187 records");

// 0x004b2d88 - Per-stage voice name data pointers (indexed by g_stageId)
const char* g_StageVoiceNamesTable[8] = {
    g_VoiceNameData_Stage0[0], g_VoiceNameData_Stage1[0], g_VoiceNameData_Stage2[0],
    g_VoiceNameData_Stage3[0], g_VoiceNameData_Stage4[0], g_VoiceNameData_Stage0[0],
    g_VoiceNameData_Stage1[0], NULL
};

// ============================================================
// Section 3: Room Sound Name Table
// ============================================================
// Section 3: Room Sound Name Table  (0x004cfae0)
//
// Per-room entity/footstep sound names, extracted from the original with a PE
// walk over assets/ResidentEvil.exe: the outer table at 0x004cfae0 holds 145
// pointers (indexed stageId * 29 + roomId), each to an array of 48 char* slots.
// 102 of the 145 rooms have at least one name; 1232 names in total, 318 distinct.
//
// Room_LoadEnemySoundBanks loads slot N into g_emSndBanks record N as
// GAME_DATA_ROOT "sound\<name>.wav", and PlayEntitySnd / Snd_em index that
// array. This table was previously an all-zero placeholder, so every lookup
// found NULL and no entity SFX - footsteps included - could play, however
// correct the loader was.
//
// g_stageId is 0-based here: the mansion main hall is stage 0 room 0 (index 0),
// which is why its footstep names sit in slots 45-47. The RDT and background
// path builders confirm the bias by using g_stageId + 1.
//
// Trailing NULL slots are omitted per row; C zero-fills the remainder.
// ============================================================
static const char* g_RoomSndData[203][48] = {
    /*   0  stage 0 room  0 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "spray",
        NULL, "R_chris", NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*   1  stage 0 room  1 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_k02", "z_k01",
        "z_head", "z_haki", "z_sanj", "z_k03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_floA", "ft_floB", "taore_wd", "ft_stwd",
    },
    /*   2  stage 0 room  2 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*   3  stage 0 room  3 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi02", "z_isi01",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*   4  stage 0 room  4 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_mika02", "z_mika01",
        "z_head", "z_Hkick", "z_Ugoron", "z_mika03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*   5  stage 0 room  5 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_mika02", "z_mika01",
        "z_head", "z_Hkick", "z_Ugoron", "z_mika03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "emblemA",
        "magnum01", NULL, "ClkDX4LR", "mv_clk", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*   6  stage 0 room  6 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, "S&W+1",
        NULL, "Dr_wd01", "Dr_wd02", NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", "key_door", NULL,
        NULL, NULL, NULL, "ft_haA", "ft_haB", "taore_st",
        "ft_cpA", "ft_cpB", "taore_cp", "ft_stwp",
    },
    /*   7  stage 0 room  7 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_aoya02", "z_aoya01",
        "z_head", "z_Hkick", "z_Ugoron", "z_aoya03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, "mv_show", "mv_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*   8  stage 0 room  8 */ {
        "cer_foot", "cer_taoA", "cer_unar", "cer_bite", "cer_cryA", "cer_taoB",
        "cer_jkMX", "cer_kamu", "cer_cryB", "cer_runMX", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_show", "glass",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*   9  stage 0 room  9 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, "ceilpres", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_mtr", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  10  stage 0 room 10 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi202", "z_isi201",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi203", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  11  stage 0 room 11 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_k02", "z_k01",
        "z_head", "z_haki", "z_sanj", "z_k03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_stwd",
    },
    /*  12  stage 0 room 12 */ {
        "VN_whip", "VN_hitA", "VN_hitB", "VN_sime", "VN_OUT", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ch_sime",
        "ji_sime", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  13  stage 0 room 13 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "TIGEREYE",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  14  stage 0 room 14 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "clst_op",
        "clst_hit", "clst_bad", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  15  stage 0 room 15 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_show", "mv_wall",
        NULL, "walldown", "emblemA", "emblemB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  16  stage 0 room 16 */ {},
    /*  17  stage 0 room 17 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_simo02", "z_simo01",
        "z_head", "z_Hkick", "z_Ugoron", "z_simo03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  18  stage 0 room 18 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_aoya02", "z_aoya01",
        "z_head", "z_Hkick", "z_Ugoron", "z_aoya03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  19  stage 0 room 19 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "bathMIX",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  20  stage 0 room 20 */ {
        "cer_foot", "cer_taoA", "cer_unar", "cer_bite", "cer_cryA", "cer_taoB",
        "cer_jkMX", "cer_kamu", "cer_cryB", "cer_runMX", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  21  stage 0 room 21 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, "kickdoor",
        "lock_mix", "ceilpres", NULL, NULL, NULL, "tao_wall",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  22  stage 0 room 22 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "gatan",
        "sw_trap", "ceilpres", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  23  stage 0 room 23 */ {
        "RVcar1", "RVpat", "RVcar2", "RVwing1", "RVwing2", "RVfryed",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "sw_btn",
        "frame_ga", "frame_fa", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  24  stage 0 room 24 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  25  stage 0 room 25 */ {},
    /*  26  stage 0 room 26 */ {
        "cer_foot", "cer_taoA", "cer_unar", "cer_bite", "cer_cryA", "cer_taoB",
        "cer_jkMX", "cer_kamu", "cer_cryB", "cer_runMX", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, "sw_medal",
        "lockout", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  27  stage 0 room 27 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_step", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  28  stage 0 room 28 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "kigae1",
        "kigae2", "Zipper3", "Zipper1", "Zipper2", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  29  stage 1 room  0 */ {},
    /*  30  stage 1 room  1 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_suzu02", "z_suzu01",
        "z_head", "z_haki", "z_sanj", "z_suzu03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_floA", "ft_floB", "taore_wd", "ft_stwd",
    },
    /*  31  stage 1 room  2 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "BRK_stn",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  32  stage 1 room  3 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_haki", "z_sanj", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_stwp",
    },
    /*  33  stage 1 room  4 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi302", "z_isi301",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi303", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  34  stage 1 room  5 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "macha",
        "shuter", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  35  stage 1 room  6 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  36  stage 1 room  7 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_suzu02", "z_suzu01",
        "z_head", "z_haki", "z_sanj", "z_suzu03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", "key_door", NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
        "ft_wdA", "ft_wdB", "taore_wd", "ft_stwd",
    },
    /*  37  stage 1 room  8 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi302", "z_isi301",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi303", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  38  stage 1 room  9 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  39  stage 1 room 10 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", "mv_cp",
        "sw_push", "aquarium", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  40  stage 1 room 11 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  41  stage 1 room 12 */ {},
    /*  42  stage 1 room 13 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /*  43  stage 1 room 14 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_k02", "z_k01",
        "z_head", "z_haki", "z_sanj", "z_k03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, "zuruzuru",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_stwd",
    },
    /*  44  stage 1 room 15 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  45  stage 1 room 16 */ {
        "GV_move", "GV_dino", "GV_p_at", "GV_swing", "GV_bite", "GV_blood",
        "GV_gulpA", "GV_gulpB", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ch_nom",
        "ji_nom", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  46  stage 1 room 17 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /*  47  stage 1 room 18 */ {
        "RVcar1", "RVpat", "RVcar2", "RVwing1", "RVwing2", "RVfryed",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "RVpatA", "RVpatB", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  48  stage 1 room 19 */ {},
    /*  49  stage 1 room 20 */ {},
    /*  50  stage 1 room 21 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_step", "sw_btn",
        "TIGEREYE", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  51  stage 1 room 22 */ {},
    /*  52  stage 1 room 23 */ {},
    /*  53  stage 1 room 24 */ {},
    /*  54  stage 1 room 25 */ {},
    /*  55  stage 1 room 26 */ {},
    /*  56  stage 1 room 27 */ {},
    /*  57  stage 1 room 28 */ {},
    /*  58  stage 2 room  0 */ {
        "cer_foot", "cer_taoA", "cer_unar", "cer_bite", "cer_cryA", "cer_taoB",
        "cer_jkMX", "cer_kamu", "cer_cryB", "cer_runMX", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "call",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "Elv1", "Elv2", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  59  stage 2 room  1 */ {
        "PY_mena", "PY_hit2", "PY_fall", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "chakuchi",
        "crank", "watergt", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "Elv1", "Elv2", "ft_lad",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_concA", "ft_concB", "taore_st", "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  60  stage 2 room  2 */ {
        "cer_foot", "cer_taoA", "cer_unar", "cer_bite", "cer_cryA", "cer_taoB",
        "cer_jkMX", "cer_kamu", "cer_cryB", "cer_runMX", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "battery",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ELVX1", NULL, "ELVX2",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_concA", "ft_concB", "taore_st", "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  61  stage 2 room  3 */ {
        "TY_foot", "TY_kaze", "TY_slice", "TY_HIT", "TY_trust", "TY_slef",
        NULL, "TY_nage", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "Rancher", NULL, NULL, "Ty_bomb", "VB00_31a", "VB00_31b",
        "VB00_31c", "TY_sube", "TY_crash", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /*  62  stage 2 room  4 */ {
        "cer_foot", "cer_taoA", "cer_unar", "cer_bite", "cer_cryA", "cer_taoB",
        "cer_jkMX", "cer_kamu", "cer_cryB", "cer_runMX", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "call",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  63  stage 2 room  5 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "sw_medal2",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ELV1", "ELV2", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_sts", NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  64  stage 2 room  6 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "sw_push2",
        "crank", "R_pass", "undergat", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  65  stage 2 room  7 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "R_pass",
        "crank", NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  66  stage 2 room  8 */ {
        "He_walkA", "He_walkB", "He_jump", "He_att", "He_land", "He_smash",
        "He_dam", "He_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, "hookmix",
        "lockoutA", "lockoutB", "magnum", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  67  stage 2 room  9 */ {
        "He_walkA", "He_walkB", "He_jump", "He_att", "He_land", "He_smash",
        "He_dam", "He_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "magnum",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  68  stage 2 room 10 */ {
        "He_walkA", "He_walkB", "He_jump", "He_att", "He_land", "He_smash",
        "He_dam", "He_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "gun_pf",
        NULL, "yk_30a", "taore_ca", "v00d_02", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  69  stage 2 room 11 */ {
        "He_walkA", "He_walkB", "He_jump", "He_att", "He_land", "He_smash",
        "He_dam", "He_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, "hookmix",
        "lockoutA", "lockoutB", "rck_hitA", "rck_hitB", "rck_brok", NULL,
        "rck_stop", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  70  stage 2 room 12 */ {
        "kuasi_A", "kuasi_B", "kuasi_C", "sp_rakk", "sp_atck", "sp_bomb",
        "sp_fumu", "sp_Doku", "poison", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_spA", "ft_spB", "taore_sp",
    },
    /*  71  stage 2 room 13 */ {
        "PYe_mena", "PYe_hit", "PYe_fall", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, "mv_cp", "hookmix",
        "lockoutA", "lockoutB", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  72  stage 2 room 14 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /*  73  stage 2 room 15 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "R_pass",
        "crank", NULL, "rck_hitA", "rck_hitB", NULL, NULL,
        "rck_stop", NULL, NULL, "ELV1", "ELV2", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_caveA", "ft_caveB", "taore_ca",
    },
    /*  74  stage 2 room 16 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_plaA", "ft_plaB", "taore_pl",
    },
    /*  75  stage 2 room 17 */ {},
    /*  76  stage 2 room 18 */ {},
    /*  77  stage 2 room 19 */ {},
    /*  78  stage 2 room 20 */ {},
    /*  79  stage 2 room 21 */ {},
    /*  80  stage 2 room 22 */ {},
    /*  81  stage 2 room 23 */ {},
    /*  82  stage 2 room 24 */ {},
    /*  83  stage 2 room 25 */ {},
    /*  84  stage 2 room 26 */ {},
    /*  85  stage 2 room 27 */ {},
    /*  86  stage 2 room 28 */ {},
    /*  87  stage 3 room  0 */ {
        "VN_kazea", "VN_hitA", "VN_hitB", "VN_sime", "VN_OUT", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "ch_sime",
        "ji_sime", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  88  stage 3 room  1 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_simo02", "z_simo01",
        "z_head", "z_Hkick", "z_Ugoron", "z_simo03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  89  stage 3 room  2 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "bathMIX",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  90  stage 3 room  3 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  91  stage 3 room  4 */ {
        "kuasi_A", "kuasi_B", "kuasi_C", "sp_rakk", "sp_atck", "sp_bomb",
        "sp_fumu", "sp_Doku", "sp_sanj2", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  92  stage 3 room  5 */ {
        "bee4_ed", "hatinage", "bee_fumu", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, "mv_stn", "gun_pf",
        NULL, "yk_405_", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_apar", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  93  stage 3 room  6 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_lad",
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  94  stage 3 room  7 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_aoya02", "z_aoya01",
        "z_head", "z_Hkick", "z_Ugoron", "z_aoya03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  95  stage 3 room  8 */ {
        "bee4_ed", "hatinage", "bee_fumu", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, "BEEP",
        "keyopen", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_apar", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  96  stage 3 room  9 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  97  stage 3 room 10 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "slide_bk",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  98  stage 3 room 11 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_aoya02", "z_aoya01",
        "z_head", "z_Hkick", "z_Ugoron", "z_aoya03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /*  99  stage 3 room 12 */ {
        "VN_air", "VN_kazeA", "VN_hitA", "VN_kazeB", "VN_hitL", "VN_hitB",
        "VN_sime", "VN_OUT", "VN_fall", "VN_OUT2", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ch_sime",
        "ji_sime", "taore_s1", "taore_wa", "filefall", "VN_body", "sliding",
        "blaze", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 100  stage 3 room 13 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", "inwater",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_lad",
        NULL, NULL, NULL, "ft_kibA", "ft_kibB", "taore_st",
        "ft_swimA", "ft_swimB", "taore_st", "ft_concA", "ft_concB", "taore_st",
    },
    /* 101  stage 3 room 14 */ {
        "nep_attB", "nep_attA", "nep_nomu", "nep_tura", "nep_twis", "nep_jump",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_mtr", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_swimA", "ft_swimB", "taore_st", "ft_concA", "ft_concB", "taore_st",
    },
    /* 102  stage 3 room 15 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "V_JOLT",
        "pakiA", "pakiB", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_swimA", "ft_swimB", "taore_st", "ft_concA", "ft_concB", "taore_st",
    },
    /* 103  stage 3 room 16 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 104  stage 3 room 17 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "Sw_lever",
        "Sw_411", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_swimA", "ft_swimB", "taore_st", "ft_concA", "ft_concB", "taore_st",
    },
    /* 105  stage 3 room 18 */ {},
    /* 106  stage 3 room 19 */ {},
    /* 107  stage 3 room 20 */ {},
    /* 108  stage 3 room 21 */ {},
    /* 109  stage 3 room 22 */ {},
    /* 110  stage 3 room 23 */ {},
    /* 111  stage 3 room 24 */ {},
    /* 112  stage 3 room 25 */ {},
    /* 113  stage 3 room 26 */ {},
    /* 114  stage 3 room 27 */ {},
    /* 115  stage 3 room 28 */ {},
    /* 116  stage 4 room  0 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_mtr", "ft_lad",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_mtnA", "ft_mtnB", "taore_pl", "ft_concA", "ft_concB", "taore_st",
    },
    /* 117  stage 4 room  1 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "HUNTER",
        "battery", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 118  stage 4 room  2 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "item02", "item01", NULL, "ft_lad",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 119  stage 4 room  3 */ {
        "z_taore", "ze_ftL", "z_ftR", "ze_kamu", "ze_tomo2", "ze_tomo1",
        "ze_head", "ze_haki", "ze_sanj", "ze_tomo3", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_mtnA", "ft_mtnB", "taore_pl",
        "ft_concA", "ft_concB", "taore_st", "ft_stmt",
    },
    /* 120  stage 4 room  4 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "panel01",
        "panel02", "pillar", "slide1", "slide2", "slide3", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 121  stage 4 room  5 */ {
        "z_taore", "zep_ftL", "z_ftR", "ze_kamu", "z_nisi2", "z_nisi1",
        "ze_head", "ze_haki", "ze_sanj", "z_nisi3", "FL_walk", "FL_jump",
        "steam_b", "FL_ceil", "FL_fall", "FL_slash", "FL_att", "FL_dam",
        "FL_out", NULL, "DM_gacha", NULL, NULL, "kns_tetu",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_concA", "ft_concB", "taore_st", "ft_stmt",
    },
    /* 122  stage 4 room  6 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "power-on",
        "sl_click", "sl_crmov", "Dhit_ch", "Dhit_ji", "Gutspose", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 123  stage 4 room  7 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, "mv_cp", "mv_step",
        "type02", "chakuchi", "sw_push3", NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", NULL, "ft_step",
        "drw_c_op", "drw_c_sh", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 124  stage 4 room  8 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, "dlbeep",
        "dbrock", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 125  stage 4 room  9 */ {
        "z_taore", "zep_ftL", "z_ftR", "ze_kamu", "z_nisi2", "z_nisi1",
        "ze_head", "ze_haki", "ze_sanj", "z_nisi3", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "type02", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 126  stage 4 room 10 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", "sw_btn",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opmt", "drw_shmt", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 127  stage 4 room 11 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_mtr", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 128  stage 4 room 12 */ {
        "z_taore", "ze_ftL", "z_ftR", "ze_kamu", "ze_tomo2", "ze_tomo1",
        "ze_head", "ze_haki", "ze_sanj", "ze_tomo3", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "sw_btn", "ELV_ON", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 129  stage 4 room 13 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "escELV",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_plaA", "ft_plaB", "taore_pl",
    },
    /* 130  stage 4 room 14 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 131  stage 4 room 15 */ {
        "FL_walk", "FL_jump", "steam_b", "FL_ceil", "FL_fall", "FL_slash",
        "FL_att", "FL_dam", "FL_out", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "conpane",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_mtnA", "ft_mtnB", "taore_pl",
        "ft_plaA", "ft_plaB", "taore_pl", "ft_concA", "ft_concB", "taore_st",
    },
    /* 132  stage 4 room 16 */ {
        "FL_walk", "FL_jump", "steam_b", "FL_ceil", "FL_fall", "FL_slash",
        "FL_att", "FL_dam", "FL_out", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "type02",
        "steam_a", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_mtnA", "ft_mtnB", "taore_pl",
        "ft_plaA", "ft_plaB", "taore_pl", "ft_concA", "ft_concB", "taore_st",
    },
    /* 133  stage 4 room 17 */ {
        "FL_walk", "FL_jump", "steam_b", "FL_ceil", "FL_fall", "FL_slash",
        "FL_att", "FL_dam", "FL_out", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "panel02",
        "steam_a", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_mtnA", "ft_mtnB", "taore_pl",
        "ft_plaA", "ft_plaB", "taore_pl", "ft_concA", "ft_concB", "taore_st",
    },
    /* 134  stage 4 room 18 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
    /* 135  stage 4 room 19 */ {
        "TY_foot", "TY_kaze", "TY_slice", "TY_HIT", "TY_trust", NULL,
        "TY_taore", "TY_nage", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "bubble_S",
        "bubble_L", "crackMIX", "glass", NULL, NULL, "conpane",
        "key_lost", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_mtnA", "ft_mtnB", "taore_pl", "ft_plaA", "ft_plaB", "taore_pl",
    },
    /* 136  stage 4 room 20 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "40S&W", NULL, NULL, "smash", "taore_we", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_mtnA", "ft_mtnB", "taore_pl", "ft_concA", "ft_concB", "taore_st",
    },
    /* 137  stage 4 room 21 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "Elv515",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_plaA", "ft_plaB", "taore_pl",
    },
    /* 138  stage 4 room 22 */ {},
    /* 139  stage 4 room 23 */ {},
    /* 140  stage 4 room 24 */ {},
    /* 141  stage 4 room 25 */ {},
    /* 142  stage 4 room 26 */ {},
    /* 143  stage 4 room 27 */ {},
    /* 144  stage 4 room 28 */ {},
    // ---- stages 5 and 6 (rows 145-202) ----
    // Added 2026-08-16. The outer table at 0x004cfae0 is 203 pointers, not
    // 145: they run contiguously to 0x004cfa20 with a 0xC0 stride and the
    // first NULL is at index 203, which is exactly 7 stages x 29 rooms (the
    // same 7 stages g_BgmRoomData covers). Room_LoadEnemySoundBanks indexes
    // stageId * 29 + roomId with NO stage folding - the original does not
    // fold either - so stage 5 room 26 is index 171 and read 26 pointers
    // past the end of the truncated table. See the crash note on the loader.
    /* 145  stage 5 room  0 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 146  stage 5 room  1 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, "Reb01", "Reb02",
        "Reb03", "Reb04", "reb_scr", "slback_c", NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_floA", "ft_floB", "taore_wd", "ft_stwd",
    },
    /* 147  stage 5 room  2 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 148  stage 5 room  3 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 149  stage 5 room  4 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 150  stage 5 room  5 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /* 151  stage 5 room  6 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, "ft_haA", "ft_haB", "taore_st",
        "ft_cpA", "ft_cpB", "taore_cp", "ft_stwp",
    },
    /* 152  stage 5 room  7 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, "mv_show", "mv_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 153  stage 5 room  8 */ {
        "kuasi_A", "kuasi_B", "kuasi_C", "sp_rakk", "sp_atck", "sp_bomb",
        "sp_fumu", "sp_Doku", "sp_sanj2", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_show", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 154  stage 5 room  9 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "DM_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_mtr", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 155  stage 5 room 10 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 156  stage 5 room 11 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_stwd",
    },
    /* 157  stage 5 room 12 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 158  stage 5 room 13 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "TIGEREYE",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 159  stage 5 room 14 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "clst_op",
        "clst_hit", "clst_bad", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 160  stage 5 room 15 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 161  stage 5 room 16 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_stwd",
    },
    /* 162  stage 5 room 17 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_simo02", "z_simo01",
        "z_head", "z_Hkick", "z_Ugoron", "z_simo03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 163  stage 5 room 18 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 164  stage 5 room 19 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "bathMIX",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 165  stage 5 room 20 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi02", "z_isi01",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 166  stage 5 room 21 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, "kickdoor",
        "lock_mix", "ceilpres", NULL, NULL, NULL, "tao_wall",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 167  stage 5 room 22 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "gatan",
        "sw_trap", "ceilpres", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 168  stage 5 room 23 */ {
        "RVcar1", "RVpat", "RVcar2", "RVwing1", "RVwing2", "RVfryed",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "sw_btn",
        "frame_ga", "frame_fa", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 169  stage 5 room 24 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "cancel",
        "type01", "type02", "item02", "item01", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 170  stage 5 room 25 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "sw_btn",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 171  stage 5 room 26 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /* 172  stage 5 room 27 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_step", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_rmA", "ft_rmB", "taore_st",
    },
    /* 173  stage 5 room 28 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "kigae1",
        "kigae2", "Zipper3", "Zipper1", "Zipper2", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 174  stage 6 room  0 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 175  stage 6 room  1 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, "BEEP",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_floA", "ft_floB", "taore_wd", "ft_stwd",
    },
    /* 176  stage 6 room  2 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "BRK_stn",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 177  stage 6 room  3 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_stwp",
    },
    /* 178  stage 6 room  4 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 179  stage 6 room  5 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "macha",
        "shuter", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 180  stage 6 room  6 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, "Reb01", "Reb02",
        "Reb03", "Reb04", "reb_scr", "slback_c", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 181  stage 6 room  7 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", "key_door", NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
        "ft_wdA", "ft_wdB", "taore_wd", "ft_stwd",
    },
    /* 182  stage 6 room  8 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "d_gigi",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 183  stage 6 room  9 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 184  stage 6 room 10 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", "mv_cp",
        "sw_push", "aquarium", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 185  stage 6 room 11 */ {
        "HU_walkA", "HU_walkB", "HU_jump", "HU_att", "HU_land", "HU_smash",
        "HU_dam", "HU_Nout", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "key_door", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 186  stage 6 room 12 */ {
        "GV_move", "GV_dino", "GV_p_at", "GV_swing", "GV_bite", "GV_blood",
        "GV_gulpA", "GV_gulpB", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ch_nom",
        "ji_nom", "GV_hakai", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_lad",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_caveA", "ft_caveB", "taore_ca",
    },
    /* 187  stage 6 room 13 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 188  stage 6 room 14 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_linA", "ft_linB", "taore_st", "ft_stwd",
    },
    /* 189  stage 6 room 15 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_wdA", "ft_wdB", "taore_wd", "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 190  stage 6 room 16 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 191  stage 6 room 17 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_cpA", "ft_cpB", "taore_cp",
    },
    /* 192  stage 6 room 18 */ {
        "RVcar1", "RVpat", "RVcar2", "RVwing1", "RVwing2", "RVfryed",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "RVpatA", "RVpatB", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 193  stage 6 room 19 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 194  stage 6 room 20 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi301", "z_isi302",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi303", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 195  stage 6 room 21 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_step", "sw_btn",
        "TIGEREYE", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_step",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "ft_cpA", "ft_cpB", "taore_cp", "ft_wdA", "ft_wdB", "taore_wd",
    },
    /* 196  stage 6 room 22 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_osou", "z_unaruA",
        "z_head", "z_Hkick", "z_Ugoron", "z_unaruB", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_cp", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        "drw_opwd", "drw_shwd", "key_desk", NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 197  stage 6 room 23 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "mv_stn", "panel02",
        "slide_b2", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 198  stage 6 room 24 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_linA", "ft_linB", "taore_st",
    },
    /* 199  stage 6 room 25 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_floA", "ft_floB", "taore_wd",
    },
    /* 200  stage 6 room 26 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_suzu02", "z_suzu01",
        "z_head", "z_Hkick", "z_Ugoron", "z_suzu03", "z_taore", "z_ftL",
        "z_ftR", "z_kamu", "z_suzu02", "z_suzu01", "z_head", "z_Hkick",
        "z_Ugoron", "z_suzu03", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, "ft_lad",
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_coefA", "ft_coefB", "taore_st",
    },
    /* 201  stage 6 room 27 */ {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, "z_taore", "z_ftL",
        "z_ftR", "z_kamu", "z_k02", "z_k01", "z_head", "z_Hkick",
        "z_Ugoron", "z_k03", NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "key_indr", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_coefA", "ft_coefB", "taore_st",
    },
    /* 202  stage 6 room 28 */ {
        "z_taore", "z_ftL", "z_ftR", "z_kamu", "z_isi02", "z_isi01",
        "z_head", "z_Hkick", "z_Ugoron", "z_isi03", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, "D_gacha", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, "ft_concA", "ft_concB", "taore_st",
    },
};

// 0x004cfae0 - Main table
const char** g_RoomSoundNameTable[203] = {
    g_RoomSndData[0],    g_RoomSndData[1],    g_RoomSndData[2],    g_RoomSndData[3],
    g_RoomSndData[4],    g_RoomSndData[5],    g_RoomSndData[6],    g_RoomSndData[7],
    g_RoomSndData[8],    g_RoomSndData[9],    g_RoomSndData[10],   g_RoomSndData[11],
    g_RoomSndData[12],   g_RoomSndData[13],   g_RoomSndData[14],   g_RoomSndData[15],
    g_RoomSndData[16],   g_RoomSndData[17],   g_RoomSndData[18],   g_RoomSndData[19],
    g_RoomSndData[20],   g_RoomSndData[21],   g_RoomSndData[22],   g_RoomSndData[23],
    g_RoomSndData[24],   g_RoomSndData[25],   g_RoomSndData[26],   g_RoomSndData[27],
    g_RoomSndData[28],   g_RoomSndData[29],   g_RoomSndData[30],   g_RoomSndData[31],
    g_RoomSndData[32],   g_RoomSndData[33],   g_RoomSndData[34],   g_RoomSndData[35],
    g_RoomSndData[36],   g_RoomSndData[37],   g_RoomSndData[38],   g_RoomSndData[39],
    g_RoomSndData[40],   g_RoomSndData[41],   g_RoomSndData[42],   g_RoomSndData[43],
    g_RoomSndData[44],   g_RoomSndData[45],   g_RoomSndData[46],   g_RoomSndData[47],
    g_RoomSndData[48],   g_RoomSndData[49],   g_RoomSndData[50],   g_RoomSndData[51],
    g_RoomSndData[52],   g_RoomSndData[53],   g_RoomSndData[54],   g_RoomSndData[55],
    g_RoomSndData[56],   g_RoomSndData[57],   g_RoomSndData[58],   g_RoomSndData[59],
    g_RoomSndData[60],   g_RoomSndData[61],   g_RoomSndData[62],   g_RoomSndData[63],
    g_RoomSndData[64],   g_RoomSndData[65],   g_RoomSndData[66],   g_RoomSndData[67],
    g_RoomSndData[68],   g_RoomSndData[69],   g_RoomSndData[70],   g_RoomSndData[71],
    g_RoomSndData[72],   g_RoomSndData[73],   g_RoomSndData[74],   g_RoomSndData[75],
    g_RoomSndData[76],   g_RoomSndData[77],   g_RoomSndData[78],   g_RoomSndData[79],
    g_RoomSndData[80],   g_RoomSndData[81],   g_RoomSndData[82],   g_RoomSndData[83],
    g_RoomSndData[84],   g_RoomSndData[85],   g_RoomSndData[86],   g_RoomSndData[87],
    g_RoomSndData[88],   g_RoomSndData[89],   g_RoomSndData[90],   g_RoomSndData[91],
    g_RoomSndData[92],   g_RoomSndData[93],   g_RoomSndData[94],   g_RoomSndData[95],
    g_RoomSndData[96],   g_RoomSndData[97],   g_RoomSndData[98],   g_RoomSndData[99],
    g_RoomSndData[100],  g_RoomSndData[101],  g_RoomSndData[102],  g_RoomSndData[103],
    g_RoomSndData[104],  g_RoomSndData[105],  g_RoomSndData[106],  g_RoomSndData[107],
    g_RoomSndData[108],  g_RoomSndData[109],  g_RoomSndData[110],  g_RoomSndData[111],
    g_RoomSndData[112],  g_RoomSndData[113],  g_RoomSndData[114],  g_RoomSndData[115],
    g_RoomSndData[116],  g_RoomSndData[117],  g_RoomSndData[118],  g_RoomSndData[119],
    g_RoomSndData[120],  g_RoomSndData[121],  g_RoomSndData[122],  g_RoomSndData[123],
    g_RoomSndData[124],  g_RoomSndData[125],  g_RoomSndData[126],  g_RoomSndData[127],
    g_RoomSndData[128],  g_RoomSndData[129],  g_RoomSndData[130],  g_RoomSndData[131],
    g_RoomSndData[132],  g_RoomSndData[133],  g_RoomSndData[134],  g_RoomSndData[135],
    g_RoomSndData[136],  g_RoomSndData[137],  g_RoomSndData[138],  g_RoomSndData[139],
    g_RoomSndData[140],  g_RoomSndData[141],  g_RoomSndData[142],  g_RoomSndData[143],
    g_RoomSndData[144],  g_RoomSndData[145],  g_RoomSndData[146],  g_RoomSndData[147],
    g_RoomSndData[148],  g_RoomSndData[149],  g_RoomSndData[150],  g_RoomSndData[151],
    g_RoomSndData[152],  g_RoomSndData[153],  g_RoomSndData[154],  g_RoomSndData[155],
    g_RoomSndData[156],  g_RoomSndData[157],  g_RoomSndData[158],  g_RoomSndData[159],
    g_RoomSndData[160],  g_RoomSndData[161],  g_RoomSndData[162],  g_RoomSndData[163],
    g_RoomSndData[164],  g_RoomSndData[165],  g_RoomSndData[166],  g_RoomSndData[167],
    g_RoomSndData[168],  g_RoomSndData[169],  g_RoomSndData[170],  g_RoomSndData[171],
    g_RoomSndData[172],  g_RoomSndData[173],  g_RoomSndData[174],  g_RoomSndData[175],
    g_RoomSndData[176],  g_RoomSndData[177],  g_RoomSndData[178],  g_RoomSndData[179],
    g_RoomSndData[180],  g_RoomSndData[181],  g_RoomSndData[182],  g_RoomSndData[183],
    g_RoomSndData[184],  g_RoomSndData[185],  g_RoomSndData[186],  g_RoomSndData[187],
    g_RoomSndData[188],  g_RoomSndData[189],  g_RoomSndData[190],  g_RoomSndData[191],
    g_RoomSndData[192],  g_RoomSndData[193],  g_RoomSndData[194],  g_RoomSndData[195],
    g_RoomSndData[196],  g_RoomSndData[197],  g_RoomSndData[198],  g_RoomSndData[199],
    g_RoomSndData[200],  g_RoomSndData[201],  g_RoomSndData[202],
};

// ============================================================
// Section 4: Room BGM tables
//
// update_room_bgm / bgm_load_and_start read three tables:
//
//   g_BgmRoomData[stage][room][n]  (0x004d0c30) - up to 4 BGM group ids per
//       room; n is bits 0-2 of the room's BGM state byte. 0xFF = no BGM.
//       g_bgmDataTable points at this and is indexed as a flat byte array:
//       ((stage * 0x20 + room) * 4 + n).
//   g_BgmNameTable[group][slot]   (0x004d07b8 -> 0x004d0428) - the wav basename
//       loaded into g_SndBank[slot] (3 slots used, the 4th is always NULL).
//   g_BgmLoopTable[group][slot]   (0x004d0980 -> 0x004d089c) - the value stored
//       in g_SndBank[slot].slot and handed to SetSndSlot/playSnd. 1 = loop.
// ============================================================

// 0x004d07b8 -> 0x004d0428 - per-group wav basenames, one per g_SndBank slot
const char* const g_BgmNameTable[57][4] = {
    { "Bgm_00",   "Se_01",    "Bgm_05",   NULL        }, //  0 @004d0428
    { "Bgm_02",   NULL,       NULL,       NULL        }, //  1 @004d0438
    { "Se_03",    "Bgm_29",   NULL,       NULL        }, //  2 @004d0448
    { "Bgm_04",   "chain1",   NULL,       NULL        }, //  3 @004d0458
    { "Se_06",    NULL,       NULL,       NULL        }, //  4 @004d0468
    { "Bgm_07",   NULL,       NULL,       NULL        }, //  5 @004d0478
    { "Bgm_08",   NULL,       NULL,       NULL        }, //  6 @004d0488
    { "Bgm_09",   NULL,       NULL,       NULL        }, //  7 @004d0498
    { "Bgm_0a",   "Bgm_0b",   "Se_3c",    NULL        }, //  8 @004d04a8
    { NULL,       NULL,       NULL,       NULL        }, //  9 @004d04b8
    { "Bgm_40",   "Bgm_0c",   NULL,       NULL        }, // 10 @004d04c8
    { "Bgm_13",   NULL,       NULL,       NULL        }, // 11 @004d04d8
    { "Bgm_0e",   NULL,       NULL,       NULL        }, // 12 @004d04e8
    { "Bgm_10",   "Bgm_11",   "Bgm_12",   NULL        }, // 13 @004d04f8
    { "Bgm_1f",   "Bgm_2b",   NULL,       NULL        }, // 14 @004d0508
    { "Bgm_31",   NULL,       NULL,       NULL        }, // 15 @004d0518
    { "Bgm_14",   NULL,       NULL,       NULL        }, // 16 @004d0528
    { "Bgm_34",   NULL,       NULL,       NULL        }, // 17 @004d0538
    { "Bgm_36",   "Bgm_08",   NULL,       NULL        }, // 18 @004d0548
    { "Bgm_2c",   "Bgm_3e",   NULL,       NULL        }, // 19 @004d0558
    { "Bgm_26",   NULL,       NULL,       NULL        }, // 20 @004d0568
    { "Bgm_26",   NULL,       NULL,       NULL        }, // 21 @004d0578
    { "Bgm_0f",   NULL,       NULL,       NULL        }, // 22 @004d0588
    { "Bgm_15",   NULL,       NULL,       NULL        }, // 23 @004d0598
    { "Bgm_18",   NULL,       "Bgm_05",   NULL        }, // 24 @004d05a8
    { "Bgm_19",   NULL,       NULL,       NULL        }, // 25 @004d05b8
    { "Bgm_1a",   NULL,       NULL,       NULL        }, // 26 @004d05c8
    { "Bgm_3d",   "Bgm_3f",   NULL,       NULL        }, // 27 @004d05d8
    { "Se_41",    NULL,       NULL,       NULL        }, // 28 @004d05e8
    { NULL,       NULL,       NULL,       NULL        }, // 29 @004d05f8
    { "Se_4c",    "Se_4d",    NULL,       NULL        }, // 30 @004d0608
    { "Se_4e",    NULL,       NULL,       NULL        }, // 31 @004d0618
    { "Se_44",    "Bgm_37",   "Bgm_38",   NULL        }, // 32 @004d0628
    { "Bgm_1b",   "Se_45",    NULL,       NULL        }, // 33 @004d0638
    { "Se_44",    "Se_45",    "Bgm_1c",   NULL        }, // 34 @004d0648
    { "Bgm_23",   NULL,       NULL,       NULL        }, // 35 @004d0658
    { "Se_03",    "Se_42",    "Se_43",    NULL        }, // 36 @004d0668
    { "Se_46",    "Se_39lp",  "Bgm_25",   NULL        }, // 37 @004d0678
    { "Se_44",    "Bgm_3a",   "V110_00",  NULL        }, // 38 @004d0688
    { "Bgm_23",   NULL,       NULL,       NULL        }, // 39 @004d0698
    { "Bgm_28",   "Bgm_57",   NULL,       NULL        }, // 40 @004d06a8
    { "Bgm_17",   "Se_4b",    NULL,       NULL        }, // 41 @004d06b8
    { NULL,       NULL,       NULL,       NULL        }, // 42 @004d06c8
    { "Bgm_16",   "Bgm_48",   NULL,       NULL        }, // 43 @004d06d8
    { "Bgm_33",   NULL,       NULL,       NULL        }, // 44 @004d06e8
    { "Bgm_49",   NULL,       NULL,       NULL        }, // 45 @004d06f8
    { "Bgm_4a",   "Bgm_32",   NULL,       NULL        }, // 46 @004d0708
    { "Bgm_1e",   "Se_4e",    NULL,       NULL        }, // 47 @004d0718
    { "Bgm_0e",   "Bgm_56",   NULL,       NULL        }, // 48 @004d0728
    { "Se_53",    "Se_54",    NULL,       NULL        }, // 49 @004d0738
    { "Bgm_2e",   "Bgm_2f",   NULL,       NULL        }, // 50 @004d0748
    { "Bgm_2e",   "Bgm_3b",   NULL,       NULL        }, // 51 @004d0758
    { "Se_55",    "Bgm_20",   "Bgm_24",   NULL        }, // 52 @004d0768
    { "Bgm_30",   "Se_50",    "Se_51",    NULL        }, // 53 @004d0778
    { "Bgm_1d",   "Bgm_2d",   "Se_4f",    NULL        }, // 54 @004d0788
    { "Se_59",    "Se_5a",    NULL,       NULL        }, // 55 @004d0798
    { NULL,       NULL,       NULL,       NULL        }, // 56 @004d07a8
};

// 0x004d0980 -> 0x004d089c - per-slot loop flag (g_SndBank[slot].slot)
const unsigned char g_BgmLoopTable[57][4] = {
    { 1, 1, 1, 0 }, //  0
    { 1, 0, 0, 0 }, //  1
    { 1, 0, 0, 0 }, //  2
    { 1, 1, 0, 0 }, //  3
    { 1, 0, 0, 0 }, //  4
    { 1, 0, 0, 0 }, //  5
    { 1, 0, 0, 0 }, //  6
    { 1, 0, 0, 0 }, //  7
    { 1, 0, 1, 0 }, //  8
    { 0, 0, 0, 0 }, //  9
    { 0, 0, 0, 0 }, // 10
    { 1, 0, 0, 0 }, // 11
    { 1, 0, 0, 0 }, // 12
    { 0, 1, 0, 0 }, // 13
    { 1, 0, 0, 0 }, // 14
    { 1, 0, 0, 0 }, // 15
    { 0, 0, 0, 0 }, // 16
    { 0, 0, 0, 0 }, // 17
    { 0, 1, 0, 0 }, // 18
    { 0, 1, 0, 0 }, // 19
    { 0, 0, 0, 0 }, // 20
    { 0, 0, 0, 0 }, // 21
    { 0, 0, 0, 0 }, // 22
    { 0, 0, 0, 0 }, // 23
    { 1, 0, 1, 0 }, // 24
    { 1, 0, 0, 0 }, // 25
    { 1, 0, 0, 0 }, // 26
    { 1, 0, 0, 0 }, // 27
    { 0, 0, 0, 0 }, // 28
    { 0, 0, 0, 0 }, // 29
    { 1, 1, 0, 0 }, // 30
    { 1, 0, 0, 0 }, // 31
    { 1, 1, 0, 0 }, // 32
    { 1, 1, 0, 0 }, // 33
    { 1, 1, 1, 0 }, // 34
    { 1, 0, 0, 0 }, // 35
    { 1, 1, 0, 0 }, // 36
    { 1, 1, 1, 0 }, // 37
    { 1, 0, 0, 0 }, // 38
    { 1, 0, 0, 0 }, // 39
    { 1, 1, 0, 0 }, // 40
    { 1, 0, 0, 0 }, // 41
    { 0, 0, 0, 0 }, // 42
    { 1, 0, 0, 0 }, // 43
    { 1, 0, 0, 0 }, // 44
    { 1, 0, 0, 0 }, // 45
    { 0, 1, 0, 0 }, // 46
    { 1, 1, 0, 0 }, // 47
    { 1, 1, 0, 0 }, // 48
    { 1, 1, 0, 0 }, // 49
    { 1, 0, 0, 0 }, // 50
    { 1, 0, 0, 0 }, // 51
    { 1, 1, 1, 0 }, // 52
    { 1, 1, 1, 0 }, // 53
    { 1, 0, 0, 0 }, // 54
    { 0, 0, 0, 0 }, // 55
    { 0, 0, 0, 0 }, // 56
};

// 0x004d0c30 - per-room BGM group ids (7 stages x 32 rooms x 4)
const unsigned char g_BgmRoomData[7][32][4] = {
    { // stage 0
        { 0x0B, 0x0F, 0xFF, 0xFF }, // room 00
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 01
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 02
        { 0x00, 0xFF, 0xFF, 0xFF }, // room 03
        { 0x00, 0x0D, 0x0E, 0xFF }, // room 04
        { 0x00, 0xFF, 0xFF, 0xFF }, // room 05
        { 0x00, 0x01, 0x09, 0xFF }, // room 06
        { 0x00, 0xFF, 0xFF, 0xFF }, // room 07
        { 0x00, 0xFF, 0xFF, 0xFF }, // room 08
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 09
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 0A
        { 0x00, 0xFF, 0xFF, 0xFF }, // room 0B
        { 0x1E, 0xFF, 0xFF, 0xFF }, // room 0C
        { 0x1C, 0xFF, 0xFF, 0xFF }, // room 0D
        { 0x00, 0xFF, 0xFF, 0xFF }, // room 0E
        { 0x0D, 0x0E, 0xFF, 0xFF }, // room 0F
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 10
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 11
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 12
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 13
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 14
        { 0x03, 0xFF, 0xFF, 0xFF }, // room 15
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 16
        { 0x05, 0xFF, 0xFF, 0xFF }, // room 17
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 18
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 19
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
    { // stage 1
        { 0x37, 0xFF, 0xFF, 0xFF }, // room 00
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 01
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 02
        { 0x09, 0xFF, 0xFF, 0xFF }, // room 03
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 04
        { 0x1F, 0xFF, 0xFF, 0xFF }, // room 05
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 06
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 07
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 08
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 09
        { 0x11, 0x07, 0xFF, 0xFF }, // room 0A
        { 0x04, 0xFF, 0xFF, 0xFF }, // room 0B
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 0C
        { 0x10, 0x16, 0x17, 0x07 }, // room 0D
        { 0x14, 0x15, 0x0F, 0xFF }, // room 0E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 0F
        { 0x06, 0xFF, 0xFF, 0xFF }, // room 10
        { 0x08, 0xFF, 0xFF, 0xFF }, // room 11
        { 0x08, 0xFF, 0xFF, 0xFF }, // room 12
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 13
        { 0x07, 0xFF, 0xFF, 0xFF }, // room 14
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 15
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 16
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 17
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 18
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 19
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
    { // stage 2
        { 0x24, 0xFF, 0xFF, 0xFF }, // room 00
        { 0x24, 0xFF, 0xFF, 0xFF }, // room 01
        { 0x24, 0xFF, 0xFF, 0xFF }, // room 02
        { 0x25, 0xFF, 0xFF, 0xFF }, // room 03
        { 0x24, 0xFF, 0xFF, 0xFF }, // room 04
        { 0x24, 0xFF, 0xFF, 0xFF }, // room 05
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 06
        { 0x26, 0xFF, 0xFF, 0xFF }, // room 07
        { 0x20, 0xFF, 0xFF, 0xFF }, // room 08
        { 0x21, 0xFF, 0xFF, 0xFF }, // room 09
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 0A
        { 0x22, 0xFF, 0xFF, 0xFF }, // room 0B
        { 0x23, 0xFF, 0xFF, 0xFF }, // room 0C
        { 0x21, 0xFF, 0xFF, 0xFF }, // room 0D
        { 0x0F, 0xFF, 0xFF, 0xFF }, // room 0E
        { 0x21, 0xFF, 0xFF, 0xFF }, // room 0F
        { 0x37, 0xFF, 0xFF, 0xFF }, // room 10
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 11
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 12
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 13
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 14
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 15
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 16
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 17
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 18
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 19
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
    { // stage 3
        { 0x2B, 0xFF, 0xFF, 0xFF }, // room 00
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 01
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 02
        { 0x0F, 0xFF, 0xFF, 0xFF }, // room 03
        { 0x2B, 0xFF, 0xFF, 0xFF }, // room 04
        { 0x2D, 0x2C, 0xFF, 0xFF }, // room 05
        { 0x0C, 0x2E, 0xFF, 0xFF }, // room 06
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 07
        { 0x2B, 0xFF, 0xFF, 0xFF }, // room 08
        { 0x1F, 0xFF, 0xFF, 0xFF }, // room 09
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 0A
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 0B
        { 0x27, 0x28, 0xFF, 0xFF }, // room 0C
        { 0x29, 0xFF, 0xFF, 0xFF }, // room 0D
        { 0x29, 0xFF, 0xFF, 0xFF }, // room 0E
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 0F
        { 0x29, 0xFF, 0xFF, 0xFF }, // room 10
        { 0x29, 0xFF, 0xFF, 0xFF }, // room 11
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 12
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 13
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 14
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 15
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 16
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 17
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 18
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 19
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
    { // stage 4
        { 0x36, 0x35, 0xFF, 0xFF }, // room 00
        { 0x35, 0xFF, 0xFF, 0xFF }, // room 01
        { 0x36, 0x35, 0x26, 0xFF }, // room 02
        { 0x36, 0x35, 0xFF, 0xFF }, // room 03
        { 0x36, 0x35, 0xFF, 0xFF }, // room 04
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 05
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 06
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 07
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 08
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 09
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 0A
        { 0x30, 0x35, 0xFF, 0xFF }, // room 0B
        { 0x2F, 0x35, 0xFF, 0xFF }, // room 0C
        { 0x35, 0xFF, 0xFF, 0xFF }, // room 0D
        { 0x0F, 0x35, 0xFF, 0xFF }, // room 0E
        { 0x31, 0x35, 0xFF, 0xFF }, // room 0F
        { 0x31, 0x35, 0xFF, 0xFF }, // room 10
        { 0x31, 0x35, 0xFF, 0xFF }, // room 11
        { 0x30, 0x35, 0xFF, 0xFF }, // room 12
        { 0x34, 0x35, 0xFF, 0xFF }, // room 13
        { 0x32, 0x33, 0x35, 0xFF }, // room 14
        { 0x35, 0xFF, 0xFF, 0xFF }, // room 15
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 16
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 17
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 18
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 19
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
    { // stage 5
        { 0x0F, 0x0A, 0xFF, 0xFF }, // room 00
        { 0x13, 0x1B, 0xFF, 0xFF }, // room 01
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 02
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 03
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 04
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 05
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 06
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 07
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 08
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 09
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 0A
        { 0x18, 0x02, 0xFF, 0xFF }, // room 0B
        { 0x1E, 0xFF, 0xFF, 0xFF }, // room 0C
        { 0x1C, 0xFF, 0xFF, 0xFF }, // room 0D
        { 0x18, 0xFF, 0xFF, 0xFF }, // room 0E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 0F
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 10
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 11
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 12
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 13
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 14
        { 0x03, 0xFF, 0xFF, 0xFF }, // room 15
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 16
        { 0x05, 0x02, 0xFF, 0xFF }, // room 17
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 18
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 19
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0x02, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
    { // stage 6
        { 0x37, 0xFF, 0xFF, 0xFF }, // room 00
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 01
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 02
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 03
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 04
        { 0x1F, 0xFF, 0xFF, 0xFF }, // room 05
        { 0x13, 0x1B, 0xFF, 0xFF }, // room 06
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 07
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 08
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 09
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 0A
        { 0x04, 0xFF, 0xFF, 0xFF }, // room 0B
        { 0x0C, 0x12, 0xFF, 0xFF }, // room 0C
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 0D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 0E
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 0F
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 10
        { 0x08, 0xFF, 0xFF, 0xFF }, // room 11
        { 0x08, 0xFF, 0xFF, 0xFF }, // room 12
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 13
        { 0x19, 0xFF, 0xFF, 0xFF }, // room 14
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 15
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 16
        { 0x0C, 0xFF, 0xFF, 0xFF }, // room 17
        { 0x1D, 0xFF, 0xFF, 0xFF }, // room 18
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 19
        { 0x1A, 0xFF, 0xFF, 0xFF }, // room 1A
        { 0x1A, 0xFF, 0xFF, 0xFF }, // room 1B
        { 0x1A, 0xFF, 0xFF, 0xFF }, // room 1C
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1D
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1E
        { 0xFF, 0xFF, 0xFF, 0xFF }, // room 1F
    },
};
