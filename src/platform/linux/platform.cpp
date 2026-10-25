// platform.cpp - Linux implementation of the platform services.
//
// Counterpart of src/platform/win32/platform.cpp. SDL2 provides the window,
// keyboard and timing; everything else is plain POSIX.
//
// The keyboard path reproduces Win32 GetAsyncKeyState's two bits: 0x8000 when
// the key is held, 0x0001 when it went down since the previous query of that
// key. The second bit is why the OptionsMenu rebinding scan works, so it is
// tracked per virtual key here rather than derived from SDL's own edge state.
#include "../platform.h"

#include <SDL2/SDL.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

// Map a Win32 virtual-key code onto an SDL scancode. The game's bindings use
// letters/digits (ASCII == VK) plus a small set of named keys.
static SDL_Scancode VkToScancode(int vk)
{
    if (vk >= 'A' && vk <= 'Z')  return (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A'));
    if (vk >= '0' && vk <= '9')  return (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '0'));

    switch (vk) {
    case VK_SHIFT:    return SDL_SCANCODE_LSHIFT;
    case VK_CONTROL:  return SDL_SCANCODE_LCTRL;
    case VK_RETURN:   return SDL_SCANCODE_RETURN;
    case VK_ESCAPE:   return SDL_SCANCODE_ESCAPE;
    case VK_SPACE:    return SDL_SCANCODE_SPACE;
    case VK_LEFT:     return SDL_SCANCODE_LEFT;
    case VK_UP:       return SDL_SCANCODE_UP;
    case VK_RIGHT:    return SDL_SCANCODE_RIGHT;
    case VK_DOWN:     return SDL_SCANCODE_DOWN;
    case VK_SNAPSHOT: return SDL_SCANCODE_PRINTSCREEN;
    case VK_F1:       return SDL_SCANCODE_F1;
    case VK_F6:       return SDL_SCANCODE_F6;
    case VK_F8:       return SDL_SCANCODE_F8;
    case VK_F9:       return SDL_SCANCODE_F9;
    default:          return SDL_SCANCODE_UNKNOWN;
    }
}

// Key state is tracked here rather than read from SDL_GetKeyboardState: SDL
// only updates that array from events its *backend* delivers, so a synthetic
// SDL_PushEvent (used by the --press test hook) would never show up. The window
// loop forwards every key event through plat_key_event instead.
static unsigned char s_keyDown[SDL_NUM_SCANCODES];
static unsigned char s_keyPrev[256];

void plat_key_event(int scancode, BOOL down)
{
#ifdef __EMSCRIPTEN__
    const char* browserInput=getenv("RE1_BROWSER_INPUT");
    if(browserInput && browserInput[0]=='1')return;
#endif
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) return;
    s_keyDown[scancode] = down ? 1 : 0;
}

int plat_key_state(int vk)
{
    if (vk < 0 || vk > 255) return 0;

    SDL_Scancode sc = VkToScancode(vk);
    int down = (sc != SDL_SCANCODE_UNKNOWN) ? s_keyDown[sc] : 0;
    int edge = (down && !s_keyPrev[vk]) ? 1 : 0;
    s_keyPrev[vk] = (unsigned char)down;

    return (down ? 0x8000 : 0) | edge;
}

void plat_key_flush(void)
{
    // Drop every pending "went down" edge, like querying 0..255 on Win32.
    for (int vk = 0; vk < 256; vk++) {
        plat_key_state(vk);   // updates s_keyPrev and discards the edge
    }
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

DWORD plat_time_ms(void)
{
    return (DWORD)SDL_GetTicks();
}

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Path normalization
//
// The game's path literals were written for NTFS: backslash separators and
// whatever case the retail installer used ("data\fontus.tim"), while the asset
// tree on disk is "./assets/USA/Data/fontus.tim". Convert separators and match
// each existing component case-insensitively; leave the tail alone so a
// directory can still be created by its requested name.
// ---------------------------------------------------------------------------

static std::string ResolveCaseInsensitive(const std::string& input)
{
    std::string path = input;
    for (char& c : path) {
        if (c == '\\') c = '/';
    }

    std::string out;
    size_t i = 0;
    if (path.rfind("./", 0) == 0) { out = "./"; i = 2; }
    else if (!path.empty() && path[0] == '/') { out = "/"; i = 1; }

    while (i <= path.size()) {
        size_t slash = path.find('/', i);
        size_t end = (slash == std::string::npos) ? path.size() : slash;
        std::string comp = path.substr(i, end - i);

        if (!comp.empty() && comp != "..") {
            std::string candidate = out + comp;
            if (access(candidate.c_str(), F_OK) != 0) {
                const char* dir = out.empty() ? "." : out.c_str();
                DIR* d = opendir(dir);
                if (d != NULL) {
                    struct dirent* e;
                    while ((e = readdir(d)) != NULL) {
                        if (strcasecmp(e->d_name, comp.c_str()) == 0) {
                            candidate = out + e->d_name;
                            break;
                        }
                    }
                    closedir(d);
                }
            }
            out = candidate;
        } else if (!comp.empty()) {
            out += comp;   // ".."
        }

        if (slash == std::string::npos) break;
        out += '/';
        i = slash + 1;
    }
    return out;
}

const char* plat_normalize_path(const char* path, char* buffer, size_t size)
{
    if (path == NULL || buffer == NULL || size == 0) return path;

    static std::unordered_map<std::string, std::string> s_cache;
    auto it = s_cache.find(path);
    if (it == s_cache.end()) {
        it = s_cache.emplace(path, ResolveCaseInsensitive(path)).first;
    }

    strncpy(buffer, it->second.c_str(), size - 1);
    buffer[size - 1] = '\0';
    return buffer;
}

BOOL plat_exe_dir(char* out, size_t size)
{
    if (out == NULL || size == 0) return FALSE;
    out[0] = '\0';

#ifdef __EMSCRIPTEN__
    if (size < 6) return FALSE;
    strcpy(out, "/game");
    return TRUE;
#else
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return FALSE;
    exe[n] = '\0';

    char* slash = strrchr(exe, '/');
    if (slash == NULL) return FALSE;
    *slash = '\0';   // drop the file name

    if (strlen(exe) >= size) return FALSE;
    strcpy(out, exe);
    return TRUE;
#endif
}

BOOL plat_path_is_absolute(const char* path)
{
    if (path == NULL || path[0] == '\0') return FALSE;
    if (path[0] == '/') return TRUE;                       // POSIX
    if (path[0] == '\\' && path[1] == '\\') return TRUE;   // UNC
    // Windows drive form, accepted on either platform so a config.ini written
    // on Windows still resolves sensibly.
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
        && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
        return TRUE;
    }
    return FALSE;
}

BOOL plat_mkdir(const char* path)
{
    char norm[1024];
    if (mkdir(plat_normalize_path(path, norm, sizeof(norm)), 0755) == 0) return TRUE;
    return (errno == EEXIST) ? TRUE : FALSE;
}

BOOL plat_file_write(const char* path, const void* data, size_t size)
{
    char norm[1024];
    FILE* f = fopen(plat_normalize_path(path, norm, sizeof(norm)), "wb");
    if (f == NULL) return FALSE;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return (written == size) ? TRUE : FALSE;
}

void* plat_file_read_all(const char* path, size_t* outSize)
{
    char norm[1024];
    FILE* f = fopen(plat_normalize_path(path, norm, sizeof(norm)), "rb");
    if (f == NULL) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long fileSize = ftell(f);
    if (fileSize < 0) { fclose(f); return NULL; }
    rewind(f);

    // Same rounding as the original loader.
    size_t alignedSize = (size_t)((fileSize & ~3L) + 0x10);
    void* buffer = malloc(alignedSize);
    if (buffer == NULL) { fclose(f); return NULL; }

    size_t got = fread(buffer, 1, (size_t)fileSize, f);
    fclose(f);
    if (got != (size_t)fileSize) { free(buffer); return NULL; }

    if (outSize != NULL) *outSize = (size_t)fileSize;
    return buffer;
}

size_t plat_readable_bytes(const void* p)
{
#ifdef __EMSCRIPTEN__
    // Linear memory is bounds checked by WebAssembly. Clamp at its current end.
    extern size_t re1_web_memory_size();
    size_t address = (size_t)p, end = re1_web_memory_size();
    return address != 0 && address < end ? end - address : 0;
#else
    // Walk /proc/self/maps for the mapping that contains p and report how much
    // of it is readable.
    FILE* f = fopen("/proc/self/maps", "r");
    if (f == NULL) return 0;

    unsigned long addr = (unsigned long)p;
    size_t avail = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) != NULL) {
        unsigned long lo = 0, hi = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s", &lo, &hi, perms) != 3) continue;
        if (addr >= lo && addr < hi) {
            if (perms[0] == 'r') avail = (size_t)(hi - addr);
            break;
        }
    }
    fclose(f);
    return avail;
#endif
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void plat_debug_output(const char* s)
{
    if (s == NULL) return;
    fputs(s, stderr);
}

BOOL plat_is_debugger_present(void)
{
    // A debugger attached under ptrace shows up as a non-zero TracerPid.
    FILE* f = fopen("/proc/self/status", "r");
    if (f == NULL) return FALSE;

    char line[256];
    BOOL traced = FALSE;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            traced = (atoi(line + 10) != 0) ? TRUE : FALSE;
            break;
        }
    }
    fclose(f);
    return traced;
}

DWORD plat_env_get(const char* name, char* buffer, DWORD size)
{
    const char* v = getenv(name);
    if (v == NULL || buffer == NULL || size == 0) return 0;
    size_t n = strlen(v);
    if (n >= (size_t)size) n = (size_t)size - 1;
    memcpy(buffer, v, n);
    buffer[n] = '\0';
    return (DWORD)n;
}

// ---------------------------------------------------------------------------
// Audio device volume
//
// Windows caches and restores the system wave-out volume. There is no
// equivalent system-wide volume to save on Linux; the audio backend (Phase 6)
// owns mixing. No-ops until then.
// ---------------------------------------------------------------------------

void plat_audio_probe_and_cache_volume(void) {}
void plat_audio_restore_volume(void) {}

// ---------------------------------------------------------------------------
// Task stacks
// ---------------------------------------------------------------------------

void* plat_alloc_guarded_stacks(int count, size_t stackSize, size_t guardSize)
{
    size_t total = (size_t)count * (stackSize + guardSize) + guardSize;
#ifdef __EMSCRIPTEN__
    // wasm cannot mprotect individual pages; fibers have separate bounded stacks.
    unsigned char* mem = (unsigned char*)calloc(1, total);
    return mem ? mem + guardSize : NULL;
#else
    void* mem = mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL;

    // [G0][S0][G1][S1]...[Gcount]: make every stack usable, leave guards
    // PROT_NONE so an overflow faults immediately.
    for (int i = 0; i < count; i++) {
        unsigned char* slot = (unsigned char*)mem + (size_t)i * (stackSize + guardSize)
                                                     + guardSize;
        if (mprotect(slot, stackSize, PROT_READ | PROT_WRITE) != 0) {
            munmap(mem, total);
            return NULL;
        }
    }
    return (unsigned char*)mem + guardSize;   // skip G0 -> start of S0
#endif
}

void plat_fatal(const char* message)
{
    fprintf(stderr, "RESIDENT EVIL: %s\n", message ? message : "(fatal)");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RESIDENT EVIL", message, NULL);
    exit(1);
}

// ---------------------------------------------------------------------------
// Window / cursor lifecycle
// ---------------------------------------------------------------------------

void plat_window_destroy(HWND window)
{
    if (window != NULL) SDL_DestroyWindow((SDL_Window*)window);
}

void plat_cursor_show(BOOL show)
{
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}
