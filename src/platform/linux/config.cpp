// config.cpp - single-instance guard and the settings-save hook (Phase 8).
//
// Settings themselves live in config.ini now and are handled by the shared
// src/system/ConfigFile.cpp, which both builds use (see ConfigFile.h). What is
// left here is the Linux half of the platform services:
//   - plat_single_instance_check: a lock file, replacing the Windows mutex
//   - CleanupVideoConfigAndSaveAllSettings: the exit hook that writes the file
#include "../platform.h"

#include "../../Globals.h"
#include "../../system/ConfigFile.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>

namespace {
int s_lockFd = -1;
}

// ---------------------------------------------------------------------------
// CleanupVideoConfigAndSaveAllSettings (0x00497ea0)
//
// The shared call sites (main loop exit, window close) land here on Linux.
// Windows also tears the Direct3D object down; the GL backend is released by
// main.cpp's own teardown, so this is just the settings write.
// ---------------------------------------------------------------------------
void CleanupVideoConfigAndSaveAllSettings(void)
{
    if (g_bHasFinalizedSettings) {
        return;
    }
    g_bHasFinalizedSettings = TRUE;
    ConfigFile_Save();
}

// ---------------------------------------------------------------------------
// plat_single_instance_check
//
// A lock file beside the game root. The lock is released by the kernel when
// the process exits, so a crash cannot leave a stale lock behind.
// ---------------------------------------------------------------------------
BOOL plat_single_instance_check(void)
{
    s_lockFd = open("re1.lock", O_RDWR | O_CREAT, 0666);
    if (s_lockFd < 0) {
        return TRUE;   // cannot create the lock: do not block the game
    }
    if (flock(s_lockFd, LOCK_EX | LOCK_NB) != 0) {
        close(s_lockFd);
        s_lockFd = -1;
        return FALSE;
    }
    return TRUE;
}
