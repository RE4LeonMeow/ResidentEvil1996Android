// Headless self-test / verification driver for the asset migrator core.
// Console build (not WIN32) so it can be scripted and compared against the
// Python tools.
//
//   re1am_selftest info <image>
//   re1am_selftest dump <image> <path-in-image> <outfile>
//   re1am_selftest bss <image> <path-in-image> <outdir>
//   re1am_selftest strinfo <image> [filter]
//   re1am_selftest strconvert <image> <outdir> <ffmpeg> [filter] [limit]
//   re1am_selftest extract <image> <outdir> [folders-csv]
//   re1am_selftest jimaku <image> <outdir>
//   re1am_selftest pc <source|-> <target> <USA|JPN> [--image] [--movies]
//                     [--ps1 <image>] [--no-ps1-credits] [--no-ps1-subs]
//                     [--no-ps1-movies] [--ps1-replace]
//   re1am_selftest dc <image> <target> <USA|JPN> [--no-bg] [--no-movies]
//                     [--no-subs] [--no-verify]
//   re1am_selftest ps1audio <image> <tree> [--no-sfx] [--no-voices] [--no-bgm]
//                     [--ogg] [--ogg-q N] [--ffmpeg <path>]
//   re1am_selftest bgm <image> <BGM name> <out.wav>

#include "core/Assets.h"
#include "core/Bss.h"
#include "core/DiscImage.h"
#include "core/Jimaku.h"
#include "core/Migrate.h"
#include "core/Ps1Audio.h"
#include "core/Util.h"
#include "core/Video.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace re1;
namespace fs = std::filesystem;

namespace {

re1::Progress consoleProgress() {
    re1::Progress p;
    p.log = [](const std::string& s) { std::printf("%s\n", s.c_str()); };
    p.step = [](int d, int t, const std::string& l) {
        if (!l.empty()) std::printf("  [%d/%d] %s\n", d, t, l.c_str());
        return true;
    };
    p.cancelled = [] { return false; };
    return p;
}

int cmdInfo(const std::string& image, const std::string& filter = "") {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    std::printf("path : %s\n", img.path().c_str());
    std::printf("raw  : %s\n", img.isRaw() ? "yes (2352)" : "no (2048)");
    std::printf("files: %zu\n", img.entries().size());
    int shown = 0;
    for (const auto& e : img.entries()) {
        if (!filter.empty() &&
            toUpper(e.path).find(toUpper(filter)) == std::string::npos)
            continue;
        if (shown++ >= 40) break;
        std::printf("  %-40s %s %8u\n", e.path.c_str(),
                    e.directory ? "DIR" : "   ", e.size);
    }
    return 0;
}

int cmdDump(const std::string& image, const std::string& path,
            const std::string& out) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    const DiscEntry* e = img.find(path);
    if (!e) {
        std::printf("ERROR: %s not in the image\n", path.c_str());
        return 1;
    }
    std::vector<uint8_t> data;
    if (!img.readFile(*e, &data) || !writeFile(out, data)) {
        std::printf("ERROR: cannot write %s\n", out.c_str());
        return 1;
    }
    std::printf("wrote %s (%zu B)\n", out.c_str(), data.size());
    return 0;
}

int cmdBss(const std::string& image, const std::string& path,
           const std::string& outDir) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    const DiscEntry* e = img.find(path);
    if (!e) {
        std::printf("ERROR: %s not in the image\n", path.c_str());
        return 1;
    }
    std::vector<uint8_t> data;
    if (!img.readFile(*e, &data)) return 1;
    const std::string stem = fs::path(e->path).stem().string();
    if (stem.size() < 7) {
        std::printf("ERROR: bad BSS name %s\n", stem.c_str());
        return 1;
    }
    auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    const int hi = hex(stem[5]), lo = hex(stem[6]);
    if (hi < 0 || lo < 0) return 1;
    re1::Progress p = consoleProgress();
    std::vector<std::string> written;
    if (!convertBss(data, stem[4], hi * 16 + lo, outDir, p, &written, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    for (const auto& w : written)
        std::printf("wrote %s\n", (fs::path(outDir) / w).string().c_str());
    return 0;
}

int cmdStrInfo(const std::string& image, const std::string& filter) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    int n = 0;
    for (const auto& e : img.entries()) {
        if (e.directory || extensionOf(e.path) != ".str") continue;
        if (!filter.empty() &&
            toUpper(e.path).find(toUpper(filter)) == std::string::npos)
            continue;
        Movie mov;
        if (!strParseFromDisc(img, e, &mov, &err)) {
            std::printf("ERROR: %s\n", err.c_str());
            return 1;
        }
        std::printf("%-24s frames=%-5zu %dx%d v%d audio=%zu sectors=%zu video_end=%zu%s\n",
                    fs::path(e.path).stem().string().c_str(), mov.frames.size(),
                    mov.width, mov.height, mov.version, mov.audio.size(),
                    mov.sectorCount, mov.videoSectorEnd,
                    mov.truncatedAudio ? " TRUNCATED" : "");
        ++n;
    }
    std::printf("%d movie(s)\n", n);
    return 0;
}

int cmdStrConvert(const std::string& image, const std::string& outDir,
                  const std::string& ffmpeg, const std::string& filter,
                  int limit) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    re1::Progress p = consoleProgress();
    int n = 0;
    for (const auto& e : img.entries()) {
        if (e.directory || extensionOf(e.path) != ".str") continue;
        if (!filter.empty() &&
            toUpper(e.path).find(toUpper(filter)) == std::string::npos)
            continue;
        if (limit > 0 && n >= limit) break;
        if (!convertStrMovie(img, e, outDir, ffmpeg, p, &err)) {
            std::printf("ERROR: %s\n", err.c_str());
            return 1;
        }
        ++n;
    }
    return 0;
}

int cmdExtract(const std::string& image, const std::string& outDir,
               const std::string& foldersCsv) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    re1::Progress p = consoleProgress();
    if (foldersCsv.empty()) {
        return extractAssetsFromImage(img, outDir, p, &err) ? 0 : 1;
    }
    std::vector<std::string> folders;
    std::string cur;
    for (char c : foldersCsv) {
        if (c == ',') {
            if (!cur.empty()) folders.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) folders.push_back(cur);
    if (!extractFoldersFromImage(img, folders, outDir, p, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    return 0;
}

int cmdJimaku(const std::string& image, const std::string& outDir) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    re1::Progress p = consoleProgress();
    int written = 0;
    if (!makeDirs(outDir) ||
        !convertJimakuSubtitles(img, outDir, p, &written, &err)) {
        std::printf("ERROR: %s\n", err.empty() ? "cannot create the output "
                                               "directory" : err.c_str());
        return 1;
    }
    std::printf("%d subtitle plane(s)\n", written);
    return written > 0 ? 0 : 1;
}

int cmdPc(int argc, char** argv) {
    PcMigrationOptions o;
    o.sourcePath = std::strcmp(argv[2], "-") == 0 ? "" : argv[2];
    o.targetRoot = argv[3];
    o.version = std::string(argv[4]) == "JPN" ? AssetVersion::JPN
                                              : AssetVersion::USA;
    for (int i = 5; i < argc; ++i) {
        if (std::strcmp(argv[i], "--image") == 0) o.sourceIsImage = true;
        if (std::strcmp(argv[i], "--movies") == 0) o.convertMovies = true;
        if (std::strcmp(argv[i], "--ps1") == 0 && i + 1 < argc)
            o.ps1ImagePath = argv[++i];
        if (std::strcmp(argv[i], "--no-ps1-credits") == 0) o.ps1Credits = false;
        if (std::strcmp(argv[i], "--no-ps1-subs") == 0)
            o.ps1FmvSubtitles = false;
        if (std::strcmp(argv[i], "--no-ps1-movies") == 0) o.ps1Movies = false;
        if (std::strcmp(argv[i], "--ps1-replace") == 0) o.ps1ReplaceMovies = true;
        if (std::strcmp(argv[i], "--ps1-audio") == 0) o.ps1Audio = true;
        if (std::strcmp(argv[i], "--ps1-ogg") == 0) o.ps1AudioOgg = true;
        if (std::strcmp(argv[i], "--ffmpeg") == 0 && i + 1 < argc) o.ffmpegPath = argv[++i];
    }
    re1::Progress p = consoleProgress();
    std::string err;
    return migratePcAssets(o, p, &err) ? 0 : 1;
}

int cmdDc(int argc, char** argv) {
    DcMigrationOptions o;
    o.imagePath = argv[2];
    o.targetRoot = argv[3];
    o.base = std::string(argv[4]) == "JPN" ? AssetVersion::JPN
                                           : AssetVersion::USA;
    for (int i = 5; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-bg") == 0)
            o.backgrounds = Backgrounds::None;
        if (std::strcmp(argv[i], "--no-movies") == 0) o.convertMovies = false;
        if (std::strcmp(argv[i], "--no-subs") == 0) o.fmvSubtitles = false;
        if (std::strcmp(argv[i], "--no-verify") == 0) o.verify = false;
    }
    re1::Progress p = consoleProgress();
    std::string err;
    return migrateDcAssets(o, p, &err) ? 0 : 1;
}

int cmdPs1Audio(int argc, char** argv) {
    DiscImage img;
    std::string err;
    if (!img.open(argv[2], &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    Ps1AudioOptions o;
    for (int i = 4; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-sfx") == 0) o.sfx = false;
        if (std::strcmp(argv[i], "--no-voices") == 0) o.voices = false;
        if (std::strcmp(argv[i], "--no-bgm") == 0) o.bgm = false;
        if (std::strcmp(argv[i], "--ogg") == 0) o.format = AudioFormat::Ogg;
        if (std::strcmp(argv[i], "--ogg-q") == 0 && i + 1 < argc) o.oggQuality = std::atoi(argv[++i]);
        if (std::strcmp(argv[i], "--ffmpeg") == 0 && i + 1 < argc) o.ffmpegPath = argv[++i];
    }
    re1::Progress p = consoleProgress();
    p.step = [](int, int, const std::string&) { return true; };
    if (!migratePs1Audio(img, argv[3], o, p, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    return 0;
}

int cmdBgm(const std::string& image, const std::string& name,
           const std::string& out) {
    DiscImage img;
    std::string err;
    if (!img.open(image, &err) || !renderPs1Bgm(img, name, out, &err)) {
        std::printf("ERROR: %s\n", err.c_str());
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: re1am_selftest <command> ...\n");
        return 2;
    }
    const std::string cmd = argv[1];
    if (cmd == "info" && argc >= 3)
        return cmdInfo(argv[2], argc >= 4 ? argv[3] : "");
    if (cmd == "dump" && argc >= 5) return cmdDump(argv[2], argv[3], argv[4]);
    if (cmd == "bss" && argc >= 5) return cmdBss(argv[2], argv[3], argv[4]);
    if (cmd == "strinfo" && argc >= 3)
        return cmdStrInfo(argv[2], argc >= 4 ? argv[3] : "");
    if (cmd == "strconvert" && argc >= 5)
        return cmdStrConvert(argv[2], argv[3], argv[4],
                             argc >= 6 ? argv[5] : "MOVIE",
                             argc >= 7 ? std::atoi(argv[6]) : 0);
    if (cmd == "extract" && argc >= 4)
        return cmdExtract(argv[2], argv[3], argc >= 5 ? argv[4] : "");
    if (cmd == "jimaku" && argc >= 4) return cmdJimaku(argv[2], argv[3]);
    if (cmd == "pc" && argc >= 5) return cmdPc(argc, argv);
    if (cmd == "dc" && argc >= 5) return cmdDc(argc, argv);
    if (cmd == "ps1audio" && argc >= 4) return cmdPs1Audio(argc, argv);
    if (cmd == "bgm" && argc >= 5) return cmdBgm(argv[2], argv[3], argv[4]);
    std::printf("unknown or incomplete command\n");
    return 2;
}
