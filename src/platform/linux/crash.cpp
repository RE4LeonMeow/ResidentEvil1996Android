// crash.cpp - crash.log diagnostics (Phase 8).
//
// Counterpart of src/system/CrashLog.cpp. A fatal signal appends a symbolized
// stack trace to crash.log at the game root, so a standalone run tells us where
// it died without a debugger attached. `-rdynamic` is set on the link line, so
// backtrace_symbols_fd() prints function names rather than bare addresses.
//
// Only async-signal-safe calls are made from the handler: open/write/close,
// backtrace() and backtrace_symbols_fd().
#include "../platform.h"

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CRASH_LOG "crash.log"

namespace {

int OpenLog(void)
{
    return open(CRASH_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
}

const char* SignalName(int sig)
{
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGABRT: return "SIGABRT";
    case SIGBUS:  return "SIGBUS";
    case SIGILL:  return "SIGILL";
    case SIGFPE:  return "SIGFPE";
    default:      return "signal";
    }
}

void OnFatalSignal(int sig)
{
    const int fd = OpenLog();
    if (fd >= 0) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "\n==== FATAL %s (%d) pid %d ====\n",
                         SignalName(sig), sig, (int)getpid());
        if (n > 0) {
            ssize_t ignored = write(fd, buf, (size_t)n);
            (void)ignored;
        }
        void* frames[32];
        const int count = backtrace(frames, 32);
        backtrace_symbols_fd(frames, count, fd);
        close(fd);
    }

    // Restore the default disposition and re-raise so the exit status and any
    // core dump behave normally.
    signal(sig, SIG_DFL);
    raise(sig);
}

}  // namespace

void crashlog_install(void)
{
    const int fd = OpenLog();
    if (fd >= 0) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "\n---- session start (pid %d) ----\n",
                         (int)getpid());
        if (n > 0) {
            ssize_t ignored = write(fd, buf, (size_t)n);
            (void)ignored;
        }
        close(fd);
    }

    signal(SIGSEGV, OnFatalSignal);
    signal(SIGABRT, OnFatalSignal);
    signal(SIGBUS,  OnFatalSignal);
    signal(SIGILL,  OnFatalSignal);
    signal(SIGFPE,  OnFatalSignal);
}

void crashlog_mark(const char* step)
{
    if (step == NULL) return;
    const int fd = OpenLog();
    if (fd < 0) return;
    static const char kPrefix[] = "[step] ";
    ssize_t ignored = write(fd, kPrefix, sizeof(kPrefix) - 1);
    ignored = write(fd, step, strlen(step));
    ignored = write(fd, "\n", 1);
    (void)ignored;
    close(fd);
}
