#pragma once
// Shared value types for the asset migrator core. The core is plain C++17 with
// no Qt dependency so it can be unit-tested and reused headlessly.

#include <cstdint>
#include <functional>
#include <string>

namespace re1 {

// Which retail tree an asset set belongs to. Selects the destination subfolder
// (`<target>/USA` or `<target>/JPN`) and, for the DC overlay, the base tree the
// overlay is built on top of.
enum class AssetVersion { USA, JPN };

inline const char* versionName(AssetVersion v) {
    return v == AssetVersion::JPN ? "JPN" : "USA";
}

// Which Director's Cut backgrounds to decode (mirrors bss_to_pak.py --all /
// --arrange / none).
enum class Backgrounds { All, Arrange, None };

// Progress/cancellation sink handed to every core entry point. The callbacks are
// invoked from the worker thread; the GUI marshals them onto its own thread.
struct Progress {
    std::function<void(const std::string&)> log;
    // done/total may be -1 when the total is not known up front.
    std::function<void(int done, int total, const std::string& label)> step;
    std::function<bool()> cancelled;

    void info(const std::string& s) const {
        if (log) log(s);
    }
    void phase(int done, int total, const std::string& label) const {
        if (step) step(done, total, label);
    }
    bool isCancelled() const { return cancelled && cancelled(); }
};

}  // namespace re1
