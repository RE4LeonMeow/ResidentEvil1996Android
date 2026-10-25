// Installation.cpp - Registry-based installation checks and configuration loading
// Functions: IsGameInstalled (0x0040ae50), LoadInstallationConfiguration (0x0040aea0)
// Adapted from Ghidra decompilation
#include "Globals.h"

// ============================================================================
// IsGameInstalled - Check if game is installed via registry key (0x0040ae50)
// ============================================================================
BOOL IsGameInstalled(void)
{
    // 0x0040ae50: Open registry key
    HKEY hKey;
    LSTATUS result = RegOpenKeyExA(HKEY_CURRENT_USER, REGKEY_PATH, 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        // Key doesn't exist - game not installed
        if (hKey) RegCloseKey(hKey);
        return FALSE;
    }

    RegCloseKey(hKey);
    return TRUE;
}

// ============================================================================
// LoadInstallationConfiguration - Load all game settings from registry (0x0040aea0)
// ============================================================================
BOOL LoadInstallationConfiguration(BYTE* pInstallPath)
{
    // 0x0040aea0
    HKEY hKey;
    LSTATUS result = RegOpenKeyExA(HKEY_CURRENT_USER, REGKEY_PATH, 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        if (hKey) RegCloseKey(hKey);
        return FALSE;
    }

    // --- Load Install Path (REG_SZ) ---
    DWORD dataSize = MAX_PATH;
    DWORD dataType = REG_SZ;
    BYTE pathBuffer[MAX_PATH] = {};
    result = RegQueryValueExA(hKey, "Install Path", NULL, &dataType, pathBuffer, &dataSize);

    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // Validate path format (must start with X:\)
    if (!isalpha(pathBuffer[0]) || pathBuffer[1] != ':' || pathBuffer[2] != '\\') {
        RegCloseKey(hKey);
        return FALSE;
    }

    // Copy install path to output buffer
    size_t pathLen = strlen((char*)pathBuffer);

    // 0x0040af8b: Load Create Directory and append to path
    dataSize = MAX_PATH;
    dataType = REG_SZ;
    BYTE createDirBuffer[260] = {};
    result = RegQueryValueExA(hKey, "Create Directory", NULL, &dataType, createDirBuffer, &dataSize);

    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // Concatenate: installPath + createDir
    strcpy((char*)pInstallPath, (char*)pathBuffer);
    strcat((char*)pInstallPath, (char*)createDirBuffer);

    // 0x0040b06f: Load X Size (REG_DWORD)
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "X Size", NULL, &dataType, (LPBYTE)&g_dwScreenWidth, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b09f: Load Y Size
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "Y Size", NULL, &dataType, (LPBYTE)&g_dwScreenHeight, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b0cf: Load Bit Depth
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "Bit Depth", NULL, &dataType, (LPBYTE)&g_dwBitDepth, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b0ff: Load FullScreen?
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "FullScreen?", NULL, &dataType, (LPBYTE)&g_bFullScreen, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b12f: Load Play Number
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "Play Number", NULL, &dataType, (LPBYTE)&g_dwPlayCount, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b15f: Load Clear Number
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "Clear Number", NULL, &dataType, (LPBYTE)&g_dwClearCount, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b18f: Load Key Def (REG_BINARY, 32 bytes)
    dataSize = 32;
    dataType = REG_BINARY;
    result = RegQueryValueExA(hKey, "Key Def", NULL, &dataType, g_keyBindingData, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    // 0x0040b1e0: Load joystick bindings (Side Def or Joy Def depending on SideWinder)
    int isSideWinder = IsSideWinderPadConnected();
    if (isSideWinder == 0) {
        g_bPadConnected = TRUE;
        dataSize = 128;
        dataType = REG_BINARY;
        result = RegQueryValueExA(hKey, "Side Def", NULL, &dataType, g_joystickBindingData, &dataSize);
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return FALSE;
        }
    } else {
        g_bPadConnected = FALSE;
        dataSize = 128;
        dataType = REG_BINARY;
        result = RegQueryValueExA(hKey, "Joy Def", NULL, &dataType, g_joystickBindingData, &dataSize);
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return FALSE;
        }
    }

    // 0x0040b287: Load Display Driver
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    RegQueryValueExA(hKey, "Display Driver", NULL, &dataType,
        (LPBYTE)&g_dwSelectedDisplayAdapterID, &dataSize);
    g_dwSelectedDisplayAdapterID = 1; // Override default

    // 0x0040b2c3: Load Install Flag
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    result = RegQueryValueExA(hKey, "Install Flag", NULL, &dataType, (LPBYTE)&g_InstallFlagData, &dataSize);
    if (result != ERROR_SUCCESS) {
        g_dwSelectedDisplayAdapterID = 0;
    }

    // 0x0040b329: Load Display Mode
    dataSize = sizeof(DWORD);
    dataType = REG_DWORD;
    RegQueryValueExA(hKey, "Display Mode", NULL, &dataType,
        (LPBYTE)&g_dwSelectedDisplayModeID, &dataSize);
    g_dwSelectedDisplayModeID = 0; // Override default

    RegCloseKey(hKey);
    return TRUE;
}
