// CrashLog.cpp - startup crash diagnostics for standalone (non-debugger) runs.
//
// The Release build died at boot with 0xC0000409 (fail-fast) raised through
// KERNELBASE!RaiseFailFastException. Fail-fast itself cannot be intercepted,
// but its usual triggers can: CRT invalid parameters, pure virtual calls,
// abort()/SIGABRT and ordinary unhandled exceptions all pass through the
// handlers registered here before the CRT escalates to fail-fast. Each one
// appends a symbolized stack trace to crash.log next to the exe, so a
// standalone run tells us exactly where it died.
//
// Dev tooling - no original counterpart.

#include "Globals.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>
#include <csignal>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

static void write_stack(FILE* f)
{
    void* frames[24];
    USHORT n = CaptureStackBackTrace(1, 24, frames, NULL);

    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);

    SYMBOL_INFO* sym = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + 256);
    sym->MaxNameLen = 255;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (USHORT i = 0; i < n; i++) {
        DWORD64 addr = (DWORD64)frames[i];
        DWORD64 disp = 0;
        if (SymFromAddr(proc, addr, &disp, sym)) {
            fprintf(f, "  #%02u %s+0x%llx (0x%08llX)\n",
                    (unsigned)i, sym->Name, (unsigned long long)disp,
                    (unsigned long long)addr);
        } else {
            fprintf(f, "  #%02u 0x%08llX\n", (unsigned)i, (unsigned long long)addr);
        }
    }
    free(sym);
}

static void report(const char* kind, FILE* f)
{
    fprintf(f, "\n==== %s ====\n", kind);
    write_stack(f);
    fflush(f);
    fclose(f);
}

static FILE* open_log(void)
{
    // Write next to the EXE, not the process CWD. A shortcut or a launcher can
    // change the working directory to somewhere the user never looks, which
    // made it look like "no crash log written" when the log was actually going
    // to the shortcut's start-in folder.
    static char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *slash = '\0';
    strcat_s(path, "\\crash.log");
    return fopen(path, "a");
}

static void __cdecl on_invalid_parameter(const wchar_t*, const wchar_t*,
                                         const wchar_t*, unsigned int, uintptr_t)
{
    FILE* f = open_log();
    if (f) report("CRT INVALID PARAMETER", f);
}

static void __cdecl on_purecall(void)
{
    FILE* f = open_log();
    if (f) report("PURE VIRTUAL CALL", f);
}

static void on_sigabrt(int)
{
    FILE* f = open_log();
    if (f) report("SIGABRT (abort())", f);
    // let the CRT continue to its own reporting
    signal(SIGABRT, SIG_DFL);
}

static LONG WINAPI on_unhandled(EXCEPTION_POINTERS* ep)
{
    // The vectored handler sees EVERY exception, including benign ones raised
    // by the graphics driver and CRT internals (0x406D1388 GPU throttling,
    // 0x40010006 debugger message, 0x40000015/0x4000001E debug events) that
    // Windows intentionally passes around and nobody ever "handles" - they are
    // not crashes. Only record the codes that actually kill the process.
    switch (ep->ExceptionRecord->ExceptionCode) {
    case 0xC0000005:  // access violation
    case 0xC00000FD:  // stack overflow
    case 0xC000001D:  // illegal instruction
    case 0xC0000094:  // integer divide by zero
    case 0xC0000096:  // privileged instruction
    case 0x80000003:  // breakpoint
    case 0x00000000:  // anything else genuinely odd
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    FILE* f = open_log();
    if (f) {
        fprintf(f, "\n==== UNHANDLED EXCEPTION 0x%08lX at 0x%08llX ====\n",
                (unsigned long)ep->ExceptionRecord->ExceptionCode,
                (unsigned long long)(uintptr_t)ep->ExceptionRecord->ExceptionAddress);
        if (ep->ContextRecord) {
            fprintf(f, "EIP=%08llX ESP=%08llX EBP=%08llX EAX=%08llX EBX=%08llX "
                       "ECX=%08llX EDX=%08llX ESI=%08llX EDI=%08llX\n",
                    (unsigned long long)ep->ContextRecord->Eip,
                    (unsigned long long)ep->ContextRecord->Esp,
                    (unsigned long long)ep->ContextRecord->Ebp,
                    (unsigned long long)ep->ContextRecord->Eax,
                    (unsigned long long)ep->ContextRecord->Ebx,
                    (unsigned long long)ep->ContextRecord->Ecx,
                    (unsigned long long)ep->ContextRecord->Edx,
                    (unsigned long long)ep->ContextRecord->Esi,
                    (unsigned long long)ep->ContextRecord->Edi);
        }
        // Resolve the faulting instruction against the PDB
        HANDLE proc = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        if (SymInitialize(proc, NULL, TRUE)) {
            SYMBOL_INFO* sym = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + 256);
            sym->MaxNameLen = 255;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            DWORD64 disp = 0;
            DWORD64 eip = (DWORD64)ep->ContextRecord->Eip;
            if (SymFromAddr(proc, eip, &disp, sym)) {
                fprintf(f, "FAULTING FUNCTION: %s+0x%llx (module base 0x%llX)\n",
                        sym->Name, (unsigned long long)disp,
                        (unsigned long long)sym->ModBase);
            } else {
                fprintf(f, "FAULTING FUNCTION: <no symbol for 0x%08llX>\n", (unsigned long long)eip);
            }
            free(sym);
        }
        write_stack(f);
        fclose(f);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void crashlog_mark(const char* step)
{
    FILE* f = open_log();
    if (f) {
        fprintf(f, "[step] %s\n", step);
        fflush(f);
        fclose(f);
    }
}

void crashlog_install(void)
{
    FILE* f = open_log();
    if (f) {
        fprintf(f, "\n---- session start (pid %lu) ----\n", (unsigned long)GetCurrentProcessId());
        fclose(f);
    }
    _set_invalid_parameter_handler(on_invalid_parameter);
    _set_purecall_handler(on_purecall);
    signal(SIGABRT, on_sigabrt);
    AddVectoredExceptionHandler(1, on_unhandled);
    SetUnhandledExceptionFilter(on_unhandled);
}
