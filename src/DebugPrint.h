// DebugPrint.h - diagnostic output that is actually visible.
//
// The project links with /SUBSYSTEM:WINDOWS, so there is no stdout: every printf
// in the port has been silently discarded. Diagnostics have to go through the
// platform debug sink (OutputDebugString on Windows), which shows up in the
// Visual Studio Output window.
//
// IMPORTANT: OutputDebugStringA raises DBG_PRINTEXCEPTION_CANCELED even when no
// debugger is attached. On a task running on the scheduler's switched stack
// (g_TasksESP) the exception dispatch fails and Windows fail-fasts the whole
// process (0xC0000409) - which killed every standalone (non-VS) run of the
// Release build on its first BGM update. Gate all output on
// plat_is_debugger_present: under a debugger you get the same trace as before,
// standalone runs are safe.
#pragma once
#include "platform/platform.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

// Shared standalone sink: appends to re1_debug.log when RE1_DEBUGLOG=1.
static inline FILE* dbg_log_file(void)
{
    static int logToFile = -1;
    if (logToFile < 0) {
        char v[4] = {0};
        logToFile = plat_env_get("RE1_DEBUGLOG", v, 4) ? 1 : 0;
    }
    if (!logToFile) return NULL;
    static FILE* f = fopen("re1_debug.log", "a");
    return f;
}

// Safe replacement for raw debug-output calls. Raw use is BANNED in task code:
// without a debugger the DBG_PRINTEXCEPTION raised by every call cannot be
// dispatched on the scheduler's switched stack and Windows fail-fasts the
// process (0xC0000409) - see docs note in this header.
static inline void dbg_safe_str(const char* s)
{
    if (plat_is_debugger_present()) {
        plat_debug_output(s);
        return;
    }
    FILE* f = dbg_log_file();
    if (f && s && *s) {
        fputs(s, f);
        if (s[strlen(s) - 1] != '\n') fputc('\n', f);
        fflush(f);
    }
}

static inline void dbg_printf(const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    if (plat_is_debugger_present()) {
        plat_debug_output(buf);
        return;
    }

    FILE* f = dbg_log_file();
    if (f) {
        fputs(buf, f);
        fputc('\n', f);
        fflush(f);
    }
}
