#pragma once
// Director's Cut overlay builder: a C++ port of tools/port_dc_assets.py plus
// the background (.BSS -> .pak) conversion from tools/bss_to_pak.py.

#include "core/Types.h"

#include <string>

namespace re1 {

struct DcOverlayOptions {
    std::string sourceDir;    // extracted PS1 Director's Cut disc root
    std::string overlayDir;   // <target>/DC
    std::string baseTreeDir;  // <target>/<USA|JPN>; may be missing
    std::string baseName;     // "USA" or "JPN"
    Backgrounds backgrounds = Backgrounds::All;
};

// Run every overlay step, then the backgrounds, writing into overlayDir. The
// base tree is only ever read.
bool buildDcOverlay(const DcOverlayOptions& opts, const Progress& progress,
                    std::string* error);

// Coverage + background check (the manifest verifier). False when the overlay
// is incomplete in a way that matters.
bool verifyDcOverlay(const DcOverlayOptions& opts, const Progress& progress,
                     std::string* error);

}  // namespace re1
