// SystemChecks.cpp - System verification functions
// Functions: GetFreeDiskSpaceMB (0x0040c510), EnumerateDriveTypes (0x0047b830), ShowMessageBox (0x00497ee0)
// Adapted from Ghidra decompilation
#include "Globals.h"

// ============================================================================
// GetFreeDiskSpaceMB - Calculate free disk space in MB (0x0040c510)
// ============================================================================
DWORD GetFreeDiskSpaceMB(LPCSTR lpPath)
{
    // 0x0040c510
    // GetDiskFreeSpaceEx accepts a full path or a root, so it also works when
    // lpPath is a full file path (e.g. install path + filename) rather than a
    // bare drive root like the original GetDiskFreeSpaceA-based logic required.
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExA(lpPath, &freeBytes, NULL, NULL)) {
        return (DWORD)(freeBytes.QuadPart >> 20); // bytes -> MB
    }

    // Fallback: original logic using the drive root (e.g. "C:\")
    CHAR szRootPath[4];
    DWORD dwSectorsPerCluster, dwBytesPerSector;
    DWORD dwNumberOfFreeClusters, dwTotalNumberOfClusters;

    szRootPath[0] = lpPath[0];
    szRootPath[1] = ':';
    szRootPath[2] = '\\';
    szRootPath[3] = '\0';

    if (!GetDiskFreeSpaceA(szRootPath, &dwSectorsPerCluster,
            &dwBytesPerSector, &dwNumberOfFreeClusters,
            &dwTotalNumberOfClusters)) {
        return 0;
    }

    return (dwNumberOfFreeClusters * dwBytesPerSector *
            dwSectorsPerCluster) >> 20; // Divide by 1MB (2^20)
}

// ============================================================================
// EnumerateDriveTypes - Find all logical drives and their types (0x0047b830)
// ============================================================================
BOOL EnumerateDriveTypes(void)
{
    // 0x0047b830
    // Clear drive type arrays
    for (int i = 0; i < MAX_DRIVES; i++) {
        g_DriveTypes[i] = DRIVE_UNKNOWN;
        g_DriveLetterBuffer[i * 2] = '\0';
    }

    // Get all logical drive strings
    CHAR drives[256] = {};
    GetLogicalDriveStringsA(sizeof(drives), drives);

    int driveCount = 0;
    int charIndex = 0;

    while (drives[charIndex] != '\0') {
        // Parse drive letter from string like "C:\"
        char driveLetter[4] = {};
        int letterIdx = 0;
        while (drives[charIndex] != '\0' && drives[charIndex] != '\\') {
            if (drives[charIndex] >= 'A' && drives[charIndex] <= 'Z') {
                driveLetter[letterIdx++] = drives[charIndex];
            } else if (drives[charIndex] >= 'a' && drives[charIndex] <= 'z') {
                driveLetter[letterIdx++] = drives[charIndex];
            }
            charIndex++;
        }

        // Build full path for GetDriveType
        char fullPath[4];
        fullPath[0] = drives[charIndex - 3]; // Drive letter
        fullPath[1] = ':';
        fullPath[2] = '\\';
        fullPath[3] = '\0';

        // Store drive letter
        g_DriveLetterBuffer[driveCount * 4] = fullPath[0];
        g_DriveLetterBuffer[driveCount * 4 + 1] = ':';
        g_DriveLetterBuffer[driveCount * 4 + 2] = '\\';
        g_DriveLetterBuffer[driveCount * 4 + 3] = '\0';

        UINT type = GetDriveTypeA(fullPath);
        g_DriveTypes[driveCount] = type;

        driveCount++;
        charIndex++; // Skip null terminator
    }

    return (driveCount > 0) ? TRUE : FALSE;
}

// ============================================================================
// ShowMessageBox - Display message with cursor handling (0x00497ee0)
// ============================================================================
int ShowMessageBox(HWND hWndOwner, LPCSTR lpMessageText, LPCSTR lpCaptionText, UINT uMessageBoxType)
{
    // 0x00497ee0
    if (g_isGameCursorHiddenFlag) {
        ShowCursor(TRUE);
    }

    int result = MessageBoxA(hWndOwner, lpMessageText, lpCaptionText, uMessageBoxType);

    if (g_isGameCursorHiddenFlag) {
        ShowCursor(FALSE);
    }

    return result;
}
