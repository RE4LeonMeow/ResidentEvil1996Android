// platform.cpp - Windows implementation of the platform services.
//
// Every body here was moved verbatim out of a src/game/ file during Phase 0
// (docs/LINUX_PORT.md). The behaviour is unchanged; only the location is.
//
// This file is one of the few places allowed to include <windows.h> directly.
#include "../platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>   // malloc/free for plat_file_read_all

// Cached system wave-out volume, owned by Globals.cpp. Declared here rather
// than pulled in through Globals.h: that header redefines OutputDebugStringA
// (see the fail-fast note in src/DebugPrint.h) and would break plat_debug_output.
extern DWORD g_CachedWaveOutVolume;

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

// --- Scripted input (Windows counterpart of the Linux --press hook) ---
// RE1_TEST_KEYS="vk:startMs-durMs,vk:startMs-durMs,..." holds each virtual key
// for the given window, measured from the first plat_key_state call. Test
// tooling only - it lets an automated run drive the game (menu navigation,
// movement) the same way main.cpp's --press does, so the two builds can
// be exercised identically. Unset by default; the parse runs once.
struct PlatTestKey { int vk; DWORD from; DWORD to; };
static PlatTestKey s_testKeys[16];
static int   s_testKeyCount = -1;
static DWORD s_testT0 = 0;

static void PlatLoadTestKeys(void)
{
    s_testKeyCount = 0;
    const char* spec = getenv("RE1_TEST_KEYS");
    if (spec == NULL) return;

    const char* p = spec;
    while (*p != '\0' && s_testKeyCount < 16) {
        int vk = (int)strtol(p, (char**)&p, 0);
        if (*p != ':') break;
        p++;
        DWORD from = (DWORD)strtoul(p, (char**)&p, 10);
        if (*p != '-') break;
        p++;
        DWORD dur = (DWORD)strtoul(p, (char**)&p, 10);
        s_testKeys[s_testKeyCount].vk = vk;
        s_testKeys[s_testKeyCount].from = from;
        s_testKeys[s_testKeyCount].to = from + dur;
        s_testKeyCount++;
        if (*p == ',') p++; else break;
    }
}

int plat_key_state(int vk)
{
    if (s_testKeyCount < 0) {
        s_testT0 = timeGetTime();
        PlatLoadTestKeys();
    }
    if (s_testKeyCount > 0) {
        DWORD now = timeGetTime() - s_testT0;
        for (int i = 0; i < s_testKeyCount; i++) {
            if (s_testKeys[i].vk == vk && now >= s_testKeys[i].from &&
                now <= s_testKeys[i].to) {
                return (int)0x8000;
            }
        }
    }
    return (int)GetAsyncKeyState(vk);
}

void plat_key_event(int keycode, BOOL down)
{
    // Windows reads the real keyboard state directly in plat_key_state, so
    // there is nothing to track here. Present for interface symmetry.
    (void)keycode; (void)down;
}

void plat_key_flush(void)
{
    // Query 0..255 once each, exactly like ResetGetAsyncKeyStateFlags.
    GetAsyncKeyState(0);
    for (BYTE vk = 1; vk != 0; vk++) {
        GetAsyncKeyState(vk);
    }
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

DWORD plat_time_ms(void)
{
    return timeGetTime();
}

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------

const char* plat_normalize_path(const char* path, char* buffer, size_t size)
{
    if (path == NULL || buffer == NULL || size == 0) return path;
    strncpy(buffer, path, size - 1);
    buffer[size - 1] = '\0';
    return buffer;
}

BOOL plat_exe_dir(char* out, size_t size)
{
    if (out == NULL || size == 0) return FALSE;
    out[0] = '\0';

    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return FALSE;

    char* slash = strrchr(exe, '\\');
    if (slash == NULL) slash = strrchr(exe, '/');
    if (slash == NULL) return FALSE;
    *slash = '\0';   // drop the file name

    if (strlen(exe) >= size) return FALSE;
    strcpy(out, exe);
    return TRUE;
}

BOOL plat_path_is_absolute(const char* path)
{
    if (path == NULL || path[0] == '\0') return FALSE;
    if (path[0] == '/') return TRUE;                       // POSIX form
    if (path[0] == '\\' && path[1] == '\\') return TRUE;   // UNC
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
        && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
        return TRUE;
    }
    return FALSE;
}

BOOL plat_mkdir(const char* path)
{
    if (CreateDirectoryA(path, NULL)) return TRUE;
    return (GetLastError() == ERROR_ALREADY_EXISTS) ? TRUE : FALSE;
}

BOOL plat_file_write(const char* path, const void* data, size_t size)
{
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, (DWORD)size, &written, NULL);
    CloseHandle(hFile);
    return (ok && written == (DWORD)size) ? TRUE : FALSE;
}

size_t plat_readable_bytes(const void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    return (size_t)((const BYTE*)mbi.BaseAddress + mbi.RegionSize -
                    (const BYTE*)p);
}

void* plat_file_read_all(const char* path, size_t* outSize)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return NULL;
    }

    // Original rounds up to 4-byte alignment + 16 bytes safety.
    DWORD alignedSize = (fileSize & 0xFFFFFFFC) + 0x10;
    void* buffer = malloc(alignedSize);
    if (buffer == NULL) {
        CloseHandle(hFile);
        return NULL;
    }

    DWORD bytesRead = 0;
    BOOL readResult = ReadFile(hFile, buffer, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!readResult || bytesRead != fileSize) {
        free(buffer);
        return NULL;
    }
    if (outSize != NULL) *outSize = (size_t)fileSize;
    return buffer;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void plat_debug_output(const char* s)
{
    // Parenthesised: globals may have redefined OutputDebugStringA as a macro.
    (OutputDebugStringA)(s);
}

BOOL plat_is_debugger_present(void)
{
    return IsDebuggerPresent() ? TRUE : FALSE;
}

DWORD plat_env_get(const char* name, char* buffer, DWORD size)
{
    return GetEnvironmentVariableA(name, buffer, size);
}

// ---------------------------------------------------------------------------
// Audio device volume
//
// Moved from SoundSystem.cpp (ProbeWaveOutDevicesAndCacheVolume /
// RestoreWaveOutVolume, 0x0047...). Enumerates wave-out devices, opens each
// with a format the device advertises, and reads or writes the master volume.
// ---------------------------------------------------------------------------

static void WaveOutVisitDevices(BOOL restore)
{
    UINT numDevs = waveOutGetNumDevs();
    HWAVEOUT hWaveOut = NULL;

    for (UINT devId = 0; devId < numDevs; devId++) {
        WAVEOUTCAPSA caps;
        if (waveOutGetDevCapsA(devId, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            WAVEFORMATEX wfx = {};
            wfx.wFormatTag = WAVE_FORMAT_PCM;
            wfx.wBitsPerSample = 8;

            if ((caps.dwFormats & WAVE_FORMAT_48S16) != 0) {
                wfx.nSamplesPerSec = 44100;
                wfx.nChannels = 2;
            } else {
                wfx.nSamplesPerSec = 22050;
                wfx.nChannels = caps.wChannels;
            }
            wfx.nBlockAlign = (WORD)((wfx.nChannels * wfx.wBitsPerSample) / 8);
            wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
            wfx.cbSize = 0;

            if (waveOutOpen(&hWaveOut, devId, &wfx, 0, 0, 0) == MMSYSERR_NOERROR) {
                if (restore) {
                    waveOutSetVolume(hWaveOut, g_CachedWaveOutVolume);
                } else {
                    waveOutGetVolume(hWaveOut, &g_CachedWaveOutVolume);
                }
            }
        }
        if (hWaveOut != NULL) {
            waveOutClose(hWaveOut);
            hWaveOut = NULL;
        }
    }
}

void plat_audio_probe_and_cache_volume(void)
{
    WaveOutVisitDevices(FALSE);
}

void plat_audio_restore_volume(void)
{
    WaveOutVisitDevices(TRUE);
}

// ---------------------------------------------------------------------------
// Task stacks
//
// Moved from TaskScheduler_Init. Reserve one contiguous region and mark every
// guard page PAGE_NOACCESS, so a task that overruns its slot faults on the
// guard with the culprit's instruction pointer in crash.log instead of
// silently corrupting a neighbouring task's saved registers.
// ---------------------------------------------------------------------------

void* plat_alloc_guarded_stacks(int count, size_t stackSize, size_t guardSize)
{
    SIZE_T total = (SIZE_T)count * (stackSize + guardSize) + guardSize;
    BYTE* mem = (BYTE*)VirtualAlloc(NULL, total, MEM_RESERVE | MEM_COMMIT,
                                    PAGE_READWRITE);
    if (mem == NULL) return NULL;

    // [G0][S0][G1][S1]...[Gcount]
    for (int i = 0; i <= count; i++) {
        BYTE* guard = mem + (SIZE_T)i * (stackSize + guardSize);
        DWORD oldProt = 0;
        VirtualProtect(guard, guardSize, PAGE_NOACCESS, &oldProt);
    }
    return mem + guardSize;  // skip G0 -> start of S0
}

void plat_fatal(const char* message)
{
    MessageBoxA(NULL, message, "RESIDENT EVIL", MB_OK);
    ExitProcess(1);
}

// ---------------------------------------------------------------------------
// Window / cursor lifecycle
// ---------------------------------------------------------------------------

void plat_window_destroy(HWND window)
{
    if (window != NULL) DestroyWindow(window);
}

void plat_cursor_show(BOOL show)
{
    ShowCursor(show);
}
