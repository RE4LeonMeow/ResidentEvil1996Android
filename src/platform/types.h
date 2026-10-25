// types.h - portable Win32 type surface.
//
// The decompiled sources were written against <windows.h>. So that the same
// sources build on a non-Windows host, this header supplies the Win32 type
// names and the MSVC safe-CRT calls they use. On Windows it is a thin wrapper
// over <windows.h>, so the MSVC build sees exactly the types it always did.
//
// Rule (see docs/LINUX_PORT.md, Phase 0): code under src/game/ includes this
// header, never <windows.h> directly. OS *services* (keyboard, time, files,
// dialogs) live behind src/platform/platform.h; this file is types only.
//
// Deliberately NOT shimmed here: anything whose size depends on the pointer
// width in a way the game relies on, or any Win32 API. If a game file needs a
// new type, add it to the non-Windows branch below.
#pragma once

#ifdef _WIN32

// ---------------------------------------------------------------------------
// Windows: the real thing.
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#else  // !_WIN32

// ---------------------------------------------------------------------------
// Non-Windows: the portable subset the game actually uses.
//
// Every type here is fixed-width or pointer-sized, matching the Win32
// definitions. Nothing in this branch may include a Windows header.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define _stricmp  strcasecmp
#define _strnicmp strncasecmp

typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef uint32_t  DWORD32;
typedef int32_t   LONG;
typedef uint32_t  ULONG;
typedef uint32_t  UINT;
typedef int32_t   INT;
typedef int16_t   SHORT;
typedef uint16_t  USHORT;
typedef int32_t   BOOL;

typedef intptr_t   INT_PTR;
typedef uintptr_t  UINT_PTR;
typedef intptr_t   LONG_PTR;
typedef uintptr_t  ULONG_PTR;
typedef uintptr_t  DWORD_PTR;
typedef size_t     SIZE_T;

// MSVC's sized integer spellings.
typedef signed char __int8;
typedef short       __int16;
typedef int         __int32;
typedef long long   __int64;

typedef struct tagRECT  { LONG left, top, right, bottom; } RECT;
typedef struct tagPOINT { LONG x, y; } POINT;

inline void SetRect(RECT* r, int l, int t, int rr, int b)
{
    r->left = l; r->top = t; r->right = rr; r->bottom = b;
}

// Pointer-sized message parameters. On 32-bit targets these are 32 bits, which
// is all this port needs; they exist so the window-procedure signatures in
// Globals.h parse.
typedef uintptr_t  WPARAM;
typedef intptr_t   LPARAM;
typedef intptr_t   LRESULT;

typedef const char* LPCSTR;
typedef char*       LPSTR;
typedef const void* LPCVOID;
typedef void*       LPVOID;

// Calling-convention keywords. They matter on Windows (the vtable adapter
// conventions are load-bearing there); on a single-ABI target they are no-ops.
// See docs/CLASSES_AND_VTABLES.md for why the Windows side must keep them.
#define CALLBACK
#define WINAPI
#define APIENTRY
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall

// Opaque OS handles. The game only ever stores and compares these; it never
// dereferences them. platform/platform.h owns their real meaning.
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HANDLE;
typedef void* HDC;
typedef void* HBRUSH;
typedef void* HMODULE;
typedef void* HICON;
typedef void* HCURSOR;
typedef uint32_t MCIERROR;

#define TRUE  1
#define FALSE 0
#define MAX_PATH 260

// Word/byte packing helpers.
#define LOWORD(l)  ((WORD)(((uintptr_t)(l)) & 0xffff))
#define HIWORD(l)  ((WORD)((((uintptr_t)(l)) >> 16) & 0xffff))
#define LOBYTE(w)  ((BYTE)(w))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))

// Window messages handled by the Marni layer's vtable[5] adapter. Values match
// Win32; on non-Windows the platform window layer delivers its own events.
#define WM_DESTROY   0x0002
#define WM_SIZE      0x0005
#define WM_ACTIVATE  0x0006
#define WM_CREATE    0x0001
#define WA_INACTIVE  0

// Message-box flags (see plat_dialog / ShowMessageBox).
#define MB_OK        0x0000
#define MB_OKCANCEL  0x0001
#define MB_ICONSTOP  0x0010
#define MB_ICONERROR 0x0010
#define IDCANCEL     2
#define IDRETRY      4

// WinMM joystick state, used by MasterInputState (0x34 bytes on Win32). Only
// the layout matters here; the fields are filled by the platform input layer.
typedef struct joyinfoex_tag {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwXpos;
    DWORD dwYpos;
    DWORD dwZpos;
    DWORD dwRpos;
    DWORD dwUpos;
    DWORD dwVpos;
    DWORD dwButtons;
    DWORD dwButtonNumber;
    DWORD dwPOV;
    DWORD dwReserved1;
    DWORD dwReserved2;
} JOYINFOEX;

// Virtual-key codes used by the game (key bindings, debug screens, the F-key
// menu combos). Values match Win32 so the binding tables stay meaningful.
#define VK_SHIFT     0x10
#define VK_CONTROL   0x11
#define VK_RETURN    0x0D
#define VK_ESCAPE    0x1B
#define VK_SPACE     0x20
#define VK_LEFT      0x25
#define VK_UP        0x26
#define VK_RIGHT     0x27
#define VK_DOWN      0x28
#define VK_SNAPSHOT  0x2C
#define VK_F1        0x70
#define VK_F6        0x75
#define VK_F8        0x77
#define VK_F9        0x78

// ---------------------------------------------------------------------------
// MSVC safe-CRT shims.
//
// The decomp uses the _s variants in a handful of places. glibc has the C11
// bounds-checked versions only when __STDC_WANT_LIB_EXT1__ is requested, so
// provide the four used, with MSVC semantics (truncate, never overflow, return
// 0 on success / non-zero on failure).
// ---------------------------------------------------------------------------
inline int sprintf_s(char* buffer, size_t sizeOfBuffer, const char* format, ...)
{
    if (buffer == NULL || sizeOfBuffer == 0) return -1;
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buffer, sizeOfBuffer, format, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeOfBuffer) {
        buffer[sizeOfBuffer - 1] = '\0';
        return -1;
    }
    return 0;
}

inline int strcpy_s(char* dest, size_t destSize, const char* src)
{
    if (dest == NULL || destSize == 0) return -1;
    if (src == NULL) { dest[0] = '\0'; return -1; }
    size_t n = strlen(src);
    if (n + 1 > destSize) { dest[0] = '\0'; return -1; }
    memcpy(dest, src, n + 1);
    return 0;
}

inline int strcat_s(char* dest, size_t destSize, const char* src)
{
    if (dest == NULL || destSize == 0) return -1;
    if (src == NULL) { dest[0] = '\0'; return -1; }
    size_t d = strlen(dest);
    size_t n = strlen(src);
    if (d + n + 1 > destSize) { dest[0] = '\0'; return -1; }
    memcpy(dest + d, src, n + 1);
    return 0;
}

inline int _vsnprintf_s(char* buffer, size_t sizeOfBuffer, size_t count,
                        const char* format, va_list argptr)
{
    if (buffer == NULL || sizeOfBuffer == 0) return -1;
    size_t cap = (count < sizeOfBuffer) ? count : sizeOfBuffer - 1;
    int n = vsnprintf(buffer, cap + 1, format, argptr);
    buffer[sizeOfBuffer - 1] = '\0';
    return n;
}

#define _snprintf  snprintf
#define _vsnprintf vsnprintf
#define _TRUNCATE ((size_t)-1)

#endif  // _WIN32
