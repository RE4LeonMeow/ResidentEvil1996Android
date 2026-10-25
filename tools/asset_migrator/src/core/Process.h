#pragma once
// Minimal child-process helper used to drive ffmpeg / ffprobe. stdin can be
// streamed (for raw video), stdout/stderr are captured to temp files so a
// chatty child can never deadlock on a full pipe.

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

class ChildProcess {
public:
    ChildProcess() = default;
    ~ChildProcess();
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    // `captureStdout` redirects stdout to a temp file readable with
    // stdoutText() after wait(). stderr is always captured.
    bool start(const std::string& exe, const std::vector<std::string>& args,
               bool captureStdout, std::string* error);

    bool writeStdin(const void* data, size_t size);
    void closeStdin();

    // Wait for exit. Returns false only if waiting itself failed.
    bool wait(int* exitCode);
    bool running() const { return m_running; }
    void terminate();

    std::string stdoutText() const;
    std::string stderrText() const;

    // Run to completion without stdin, returning the exit code.
    static bool run(const std::string& exe, const std::vector<std::string>& args,
                    bool captureStdout, int* exitCode, std::string* out,
                    std::string* err, std::string* error);

    static std::string quoteArg(const std::string& arg);

private:
    void cleanup();

    bool m_running = false;
    bool m_captureStdout = false;
    std::string m_outPath;
    std::string m_errPath;
#ifdef _WIN32
    void* m_proc = nullptr;      // HANDLE
    void* m_stdin = nullptr;     // HANDLE
#else
    int m_pid = -1;
    int m_stdin = -1;
#endif
};

}  // namespace re1
