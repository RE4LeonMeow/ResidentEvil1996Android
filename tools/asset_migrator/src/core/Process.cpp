#include "core/Process.h"

#include "core/Util.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace re1 {
namespace {

std::string makeTempPath(const char* suffix) {
    static std::atomic<int> counter{0};
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    if (ec) dir = fs::current_path(ec);
#ifdef _WIN32
    const unsigned long pid = GetCurrentProcessId();
#else
    const unsigned long pid = (unsigned long)getpid();
#endif
    const int n = counter.fetch_add(1);
    return (dir / ("re1am_" + std::to_string(pid) + "_" + std::to_string(n) +
                   suffix))
        .string();
}

std::string readWhole(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string s((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
    return s;
}

}  // namespace

std::string ChildProcess::quoteArg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    if (arg.find_first_of(" \t\"") == std::string::npos) return arg;
    std::string out = "\"";
    size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            ++backslashes;
        } else if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
        } else {
            out.append(backslashes, '\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    out.append(backslashes * 2, '\\');
    out.push_back('"');
    return out;
}

ChildProcess::~ChildProcess() {
    if (m_running) terminate();
    cleanup();
}

bool ChildProcess::start(const std::string& exe,
                         const std::vector<std::string>& args,
                         bool captureStdout, std::string* error) {
    cleanup();
    m_captureStdout = captureStdout;
    m_errPath = makeTempPath(".err");
    if (captureStdout) m_outPath = makeTempPath(".out");

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE outH = INVALID_HANDLE_VALUE;
    if (captureStdout) {
        outH = CreateFileA(m_outPath.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    }
    HANDLE errH = CreateFileA(m_errPath.c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);

    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        if (error) *error = "CreatePipe failed";
        if (outH != INVALID_HANDLE_VALUE) CloseHandle(outH);
        if (errH != INVALID_HANDLE_VALUE) CloseHandle(errH);
        return false;
    }
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);

    std::string cmd = quoteArg(exe);
    for (const auto& a : args) {
        cmd.push_back(' ');
        cmd += quoteArg(a);
    }
    std::vector<char> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = rd;
    si.hStdOutput =
        captureStdout ? outH : (errH != INVALID_HANDLE_VALUE ? errH : nullptr);
    si.hStdError = errH != INVALID_HANDLE_VALUE ? errH : nullptr;

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr,
                                   TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                                   &pi);
    CloseHandle(rd);
    if (outH != INVALID_HANDLE_VALUE) CloseHandle(outH);
    if (errH != INVALID_HANDLE_VALUE) CloseHandle(errH);
    if (!ok) {
        CloseHandle(wr);
        if (error) *error = "cannot start " + exe;
        return false;
    }
    CloseHandle(pi.hThread);
    m_proc = pi.hProcess;
    m_stdin = wr;
    m_running = true;
    return true;
#else
    int outFd = -1;
    if (captureStdout)
        outFd = ::open(m_outPath.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
    const int errFd = ::open(m_errPath.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);

    int fds[2];
    if (pipe(fds) != 0) {
        if (outFd >= 0) ::close(outFd);
        if (errFd >= 0) ::close(errFd);
        if (error) *error = "pipe failed";
        return false;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        if (outFd >= 0) ::close(outFd);
        if (errFd >= 0) ::close(errFd);
        if (error) *error = "fork failed";
        return false;
    }
    if (pid == 0) {
        dup2(fds[0], 0);
        if (captureStdout && outFd >= 0)
            dup2(outFd, 1);
        else if (errFd >= 0)
            dup2(errFd, 1);
        if (errFd >= 0) dup2(errFd, 2);
        ::close(fds[0]);
        ::close(fds[1]);
        if (outFd >= 0) ::close(outFd);
        if (errFd >= 0) ::close(errFd);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(exe.c_str(), argv.data());
        _exit(127);
    }
    ::close(fds[0]);
    if (outFd >= 0) ::close(outFd);
    if (errFd >= 0) ::close(errFd);
    m_pid = pid;
    m_stdin = fds[1];
    m_running = true;
    return true;
#endif
}

bool ChildProcess::writeStdin(const void* data, size_t size) {
    if (!m_running || size == 0) return m_running;
#ifdef _WIN32
    if (!m_stdin) return false;
    const uint8_t* p = (const uint8_t*)data;
    size_t left = size;
    while (left) {
        DWORD written = 0;
        const DWORD chunk = (DWORD)(left > 1u << 20 ? 1u << 20 : left);
        if (!WriteFile((HANDLE)m_stdin, p, chunk, &written, nullptr)) return false;
        p += written;
        left -= written;
    }
    return true;
#else
    if (m_stdin < 0) return false;
    const uint8_t* p = (const uint8_t*)data;
    size_t left = size;
    while (left) {
        const ssize_t n = ::write(m_stdin, p, left);
        if (n <= 0) return false;
        p += n;
        left -= (size_t)n;
    }
    return true;
#endif
}

void ChildProcess::closeStdin() {
#ifdef _WIN32
    if (m_stdin) {
        CloseHandle((HANDLE)m_stdin);
        m_stdin = nullptr;
    }
#else
    if (m_stdin >= 0) {
        ::close(m_stdin);
        m_stdin = -1;
    }
#endif
}

bool ChildProcess::wait(int* exitCode) {
    if (!m_running) return false;
#ifdef _WIN32
    WaitForSingleObject((HANDLE)m_proc, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess((HANDLE)m_proc, &code);
    if (exitCode) *exitCode = (int)code;
    CloseHandle((HANDLE)m_proc);
    m_proc = nullptr;
#else
    int status = 0;
    waitpid(m_pid, &status, 0);
    if (exitCode) *exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    m_pid = -1;
#endif
    m_running = false;
    return true;
}

void ChildProcess::terminate() {
    if (!m_running) return;
#ifdef _WIN32
    if (m_proc) TerminateProcess((HANDLE)m_proc, 1);
#else
    if (m_pid > 0) kill(m_pid, SIGKILL);
#endif
}

std::string ChildProcess::stdoutText() const { return readWhole(m_outPath); }
std::string ChildProcess::stderrText() const { return readWhole(m_errPath); }

void ChildProcess::cleanup() {
    closeStdin();
#ifdef _WIN32
    if (m_proc) {
        CloseHandle((HANDLE)m_proc);
        m_proc = nullptr;
    }
#endif
    if (!m_outPath.empty()) {
        std::error_code ec;
        fs::remove(m_outPath, ec);
        m_outPath.clear();
    }
    if (!m_errPath.empty()) {
        std::error_code ec;
        fs::remove(m_errPath, ec);
        m_errPath.clear();
    }
}

bool ChildProcess::run(const std::string& exe,
                       const std::vector<std::string>& args, bool captureStdout,
                       int* exitCode, std::string* out, std::string* err,
                       std::string* error) {
    ChildProcess p;
    if (!p.start(exe, args, captureStdout, error)) return false;
    p.closeStdin();
    if (!p.wait(exitCode)) {
        if (error) *error = "wait failed";
        return false;
    }
    if (out) *out = p.stdoutText();
    if (err) *err = p.stderrText();
    return true;
}

}  // namespace re1
