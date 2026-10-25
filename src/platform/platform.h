// platform.h - the OS services the game calls.
//
// This is the boundary described in docs/LINUX_PORT.md Phase 0. Game code
// (src/game/) and the Marni layer call these functions; each OS provides an
// implementation under src/platform/<os>/. Nothing here exposes an OS type
// beyond the opaque handles in types.h, so a new platform is a new directory,
// not a new set of #ifdefs.
//
// Adding to this interface: only add a call that a game file actually makes
// directly today. This is not a general-purpose OS abstraction layer.
#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
// Keyboard - raw virtual-key polling.
//
// Semantics match Win32 GetAsyncKeyState exactly, because the callers depend on
// both bits: bit 0x8000 = key is currently down, bit 0x0001 = key went down
// since the previous query of that key. OptionsMenu's key-rebinding scan reads
// the low bit; everything else reads 0x8000.
// ---------------------------------------------------------------------------
int  plat_key_state(int vk);

// Feed a raw key transition from the platform window loop. `keycode` is the
// platform's own key identifier (SDL scancode on Linux). Real events and
// synthetic ones (the --press test hook) both go through here, which is what
// makes plat_key_state independent of the windowing library's internal state.
void plat_key_event(int keycode, BOOL down);

// Drain the "pressed since last query" low bit for every key, exactly like
// ResetGetAsyncKeyStateFlags (0x00497e60) does by querying 0..255.
void plat_key_flush(void);

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

// Milliseconds since boot, monotonic. Replaces timeGetTime().
DWORD plat_time_ms(void);

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------

// Normalize a path into `buffer` for this platform and return it. On Windows
// this is a copy (the paths are already native). On a case-sensitive
// filesystem it converts '\' to '/' and resolves each *existing* component
// case-insensitively, because the game's path literals were written against
// NTFS (`data\fontus.tim` vs the shipped `Data/fontus.tim`). Components that
// do not exist yet (a directory about to be created) are left as written.
const char* plat_normalize_path(const char* path, char* buffer, size_t size);

// Absolute directory holding the running executable, with no trailing
// separator. This is the anchor every relative asset/save path is resolved
// against, so the game behaves the same whatever the working directory is.
// Returns FALSE (and leaves `out` empty) when it cannot be determined.
BOOL plat_exe_dir(char* out, size_t size);

// TRUE when `path` is absolute on this platform (`/...`, `X:\...`, `\\...`).
BOOL plat_path_is_absolute(const char* path);

// Create a directory if it does not exist. Replaces CreateDirectoryA with a
// NULL security descriptor. Returns TRUE if the directory exists afterwards.
BOOL plat_mkdir(const char* path);

// Write a buffer to a file, creating or truncating it. Returns TRUE on success.
// Replaces the CreateFileA/WriteFile pair in MarniBits::SaveBitmapToFile.
BOOL plat_file_write(const char* path, const void* data, size_t size);

// How many bytes are readable from `p` up to the end of the committed,
// accessible memory region containing it. Returns 0 when the address is not
// readable. Replaces the VirtualQuery calls that clamp a truncated TIM page.
size_t plat_readable_bytes(const void* p);

// Read a whole file into a freshly allocated buffer, rounded up the same way
// the original loader did ((size & ~3) + 0x10). Returns the buffer and sets
// *outSize to the file size, or NULL on failure. Free with free().
// Replaces the CreateFileA/GetFileSize/ReadFile sequence in
// PSXTexture::LoadFromFile.
void* plat_file_read_all(const char* path, size_t* outSize);

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Emit a diagnostic string to the platform's debug sink (OutputDebugString on
// Windows). Callers must have already gated on plat_is_debugger_present() --
// see the fail-fast note in src/DebugPrint.h.
void plat_debug_output(const char* s);

BOOL plat_is_debugger_present(void);

// Read an environment variable into buffer. Returns the length written, or 0
// if unset. Matches GetEnvironmentVariableA.
DWORD plat_env_get(const char* name, char* buffer, DWORD size);

// ---------------------------------------------------------------------------
// Audio device volume
//
// The original caches the system wave-out volume at start and restores it on
// exit, so the game's own volume changes do not outlive the session.
// ---------------------------------------------------------------------------
void plat_audio_probe_and_cache_volume(void);
void plat_audio_restore_volume(void);

// ---------------------------------------------------------------------------
// Task stacks
//
// Allocate `count` stacks of `stackSize` bytes, each preceded and followed by a
// `guardSize` guard page that faults on touch. Returns a pointer to the first
// *usable* stack (i.e. just past the leading guard), or NULL on failure. The
// layout is [G][S][G][S]...[G] -- see TaskScheduler.cpp for why the guards
// exist (a task that overruns its slot must fault instead of corrupting its
// neighbour's saved registers).
void* plat_alloc_guarded_stacks(int count, size_t stackSize, size_t guardSize);

// Report a fatal error to the user and terminate the process.
void plat_fatal(const char* message);

// ---------------------------------------------------------------------------
// Window / cursor lifecycle
//
// Used by the Marni init failure path. The platform window layer owns the real
// window; these are the only two operations game code performs on it directly.
// ---------------------------------------------------------------------------
void plat_window_destroy(HWND window);
void plat_cursor_show(BOOL show);

// ---------------------------------------------------------------------------
// FMV playback backend (Phase 7)
//
// src/video/VideoPlayback.cpp owns the 4-state machine, the per-FMV skip masks,
// the g_videoSkipCounter grace period and the prologue scenario cut; the
// decoder is platform-specific. On Windows MCI opens the AVI and renders it
// into the game window itself, or Media Foundation decodes a modern container
// into a texture (video.cpp). On Linux ffmpeg decodes into a texture that the
// backend uploads, draws as a full-screen quad and presents (video.cpp).
//
// Cut points are MILLISECONDS, not frame indices: the PC prologue uses frames
// 1778/1885 at 10 fps, while the DC STR stream uses its 15-fps frame points.
// Each backend turns the selected time into its own stream frames.
// ---------------------------------------------------------------------------

// Called each presented movie frame with the frame's texture and its stream
// time in milliseconds, after the backbuffer is cleared. TRUE = the callback
// drew the frame itself, so the backend skips its full-screen quad.
typedef BOOL (*PlatVideoOverlayCallback)(DWORD32 frameTexture, int frameMs);
void plat_video_set_overlay_callback(PlatVideoOverlayCallback callback);

// Probe the decoder backend. FALSE means "no FMV available, skip the movie".
BOOL plat_video_init(void);

// Open `path` and start playing. playToMs > 0 stops at that time (the
// prologue's Chris-only beat); 0 plays through to the end.
BOOL plat_video_open_and_play(const char* path, int playToMs);

// Resume the already-open movie from fromMs through to the end.
void plat_video_play_from(int fromMs);

// Stop playback (user skip) and close the movie.
void plat_video_stop(void);

// Release the decoder and everything it holds. Safe to call twice.
void plat_video_close(void);

// TRUE while a movie is open and not finished.
BOOL plat_video_is_active(void);

// One-shot: TRUE once when the current segment (or the whole movie) ended.
// Consumes the event.
BOOL plat_video_take_end_event(void);

// Per-tick pump: decode and present the frame due now. No-op on Windows, where
// MCI paints the window itself.
void plat_video_tick(void);

// Switch the window between game and movie presentation. Windows adjusts the
// window style so MCI's overlay is not clipped; Linux is a no-op.
void plat_video_set_window_mode(BOOL forVideo);

// ---------------------------------------------------------------------------
// Persistent settings (Phase 8)
//
// Both builds now keep the player's settings in config.ini, written by
// src/system/ConfigFile.cpp. Windows used to save them to
// HKCU\Software\CAPCOM\RESIDENT EVIL; it still READS that key once as a
// fallback for existing installs (system/Installation.cpp), but config.ini is
// authoritative and is created with documented defaults when missing.
// ---------------------------------------------------------------------------

// TRUE when this is the only running instance. Windows uses a named mutex
// (main.cpp's CheckSingleInstance); Linux takes a lock file.
BOOL plat_single_instance_check(void);

// Crash diagnostics: append a symbolized trace to crash.log on a fatal signal.
// Windows implements these in system/CrashLog.cpp; see crash.cpp here.
void crashlog_install(void);
void crashlog_mark(const char* step);
