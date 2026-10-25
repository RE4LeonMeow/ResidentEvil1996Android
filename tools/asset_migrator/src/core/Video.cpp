#include "core/Video.h"

#include "core/Mdec.h"
#include "core/Process.h"
#include "core/Util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace re1 {
namespace {

constexpr size_t kSector = 0x800;
// SLUS_001.70 _fmvPlay @ 0x8005A5B8 uses CdlModeSpeed (150 sectors/second).
constexpr double kPsxCdsSpeed = 150.0;
const uint8_t kMagic[4] = {0x60, 0x01, 0x01, 0x80};

double sourceDuration(const Movie& mov) {
    const size_t sectors = mov.videoSectorEnd > 0
        ? mov.videoSectorEnd
        : mov.sectorCount;
    return sectors > 0
        ? static_cast<double>(sectors) / kPsxCdsSpeed
        : 0.0;
}

double sourceFrameRate(size_t frames, size_t sectors) {
    if (frames == 0 || sectors == 0) return 0.0;
    const double raw = static_cast<double>(frames) * kPsxCdsSpeed /
        static_cast<double>(sectors);
    const double rates[] = {7.5, 10.0, 15.0, 20.0, 30.0};
    for (double rate : rates) {
        if (std::abs(raw - rate) < 0.25) {
            return rate == 20.0 ? 20000.0 / 1001.0 : rate;
        }
    }
    return raw;
}

// ffmpeg xa_adpcm_table (cdrom XA predictor coefficients, filter 0..4).
const int kXaTable[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

inline int sign4(int v) { return v > 7 ? v - 16 : v; }
inline int clamp16(int v) {
    return v > 32767 ? 32767 : (v < -32768 ? -32768 : v);
}

struct XaParams {
    int f0 = 0, f1 = 0, shift = 12;
};

XaParams xaParams(int param) {
    XaParams p;
    p.shift = 12 - (param & 0x0F);
    int filt = param >> 4;
    if (filt > 4 || p.shift < 0) {
        filt = 0;
        p.shift = 12;
    }
    p.f0 = kXaTable[filt][0];
    p.f1 = kXaTable[filt][1];
    return p;
}

// Decode 28 samples for one unit/channel; updates `hist` (s1,s2).
void xa28(const uint8_t* g, int i, bool high, int hist[2], std::vector<int>* out) {
    const XaParams p = xaParams(g[(high ? 5 : 4) + i * 2]);
    int s1 = hist[0], s2 = hist[1];
    for (int j = 0; j < 28; ++j) {
        const uint8_t d = g[16 + i + j * 4];
        const int t = sign4(high ? ((d >> 4) & 0x0F) : (d & 0x0F));
        const int s = t * (1 << p.shift) + ((s1 * p.f0 + s2 * p.f1 + 32) >> 6);
        s2 = s1;
        s1 = clamp16(s);
        out->push_back(s1);
    }
    hist[0] = s1;
    hist[1] = s2;
}

std::string ffprobeFor(const std::string& ffmpeg) {
    const std::string dir = fs::path(ffmpeg).parent_path().string();
    if (!dir.empty()) return joinPath(dir, "ffprobe");
    return "ffprobe";
}

void writeWav(const std::string& path, const std::vector<int16_t>& samples,
              int rate) {
    std::vector<uint8_t> hdr;
    auto put16 = [&](uint16_t v) {
        hdr.push_back((uint8_t)(v & 0xFF));
        hdr.push_back((uint8_t)(v >> 8));
    };
    auto put32 = [&](uint32_t v) {
        put16((uint16_t)(v & 0xFFFF));
        put16((uint16_t)(v >> 16));
    };
    const uint32_t dataBytes = (uint32_t)(samples.size() * 2);
    const char* riff = "RIFF";
    hdr.insert(hdr.end(), riff, riff + 4);
    put32(36 + dataBytes);
    const char* wave = "WAVEfmt ";
    hdr.insert(hdr.end(), wave, wave + 8);
    put32(16);
    put16(1);       // PCM
    put16(2);       // stereo
    put32((uint32_t)rate);
    put32((uint32_t)(rate * 2 * 2));
    put16(4);
    put16(16);
    const char* data = "data";
    hdr.insert(hdr.end(), data, data + 4);
    put32(dataBytes);

    std::vector<uint8_t> bytes;
    bytes.reserve(hdr.size() + dataBytes);
    bytes.insert(bytes.end(), hdr.begin(), hdr.end());
    const uint8_t* p = (const uint8_t*)samples.data();
    bytes.insert(bytes.end(), p, p + dataBytes);
    writeFile(path, bytes);
}

bool demuxVideoSector(const uint8_t* data, size_t len, Movie* mov,
                      std::unordered_map<uint32_t, size_t>* byFrame,
                      std::vector<uint32_t>* order) {
    if (len < 0x20 || std::memcmp(data, kMagic, 4) != 0) return false;
    uint32_t magic32, frameNo, used;
    uint16_t chunk, chunks, w, h, n, magic16, qscale, version;
    std::memcpy(&magic32, data + 0, 4);
    std::memcpy(&chunk, data + 4, 2);
    std::memcpy(&chunks, data + 6, 2);
    std::memcpy(&frameNo, data + 8, 4);
    std::memcpy(&used, data + 12, 4);
    std::memcpy(&w, data + 16, 2);
    std::memcpy(&h, data + 18, 2);
    std::memcpy(&n, data + 20, 2);
    std::memcpy(&magic16, data + 22, 2);
    std::memcpy(&qscale, data + 24, 2);
    std::memcpy(&version, data + 26, 2);
    (void)chunk;
    (void)chunks;
    (void)used;
    (void)n;
    (void)qscale;
    if (magic32 != 0x80010160 || magic16 != 0x3800) return false;
    size_t idx;
    auto it = byFrame->find(frameNo);
    if (it == byFrame->end()) {
        idx = mov->frames.size();
        (*byFrame)[frameNo] = idx;
        mov->frames.push_back({});
        order->push_back(frameNo);
    } else {
        idx = it->second;
    }
    mov->frames[idx].insert(mov->frames[idx].end(), data + 0x20, data + kSector);
    mov->width = w;
    mov->height = h;
    mov->version = version;
    return true;
}

std::vector<int16_t> decodeXaStereo(const std::vector<uint8_t>& groups,
                                    size_t* nframes) {
    std::vector<int> a, b;
    int left[2] = {0, 0}, right[2] = {0, 0};
    for (size_t off = 0; off + 128 <= groups.size(); off += 128) {
        const uint8_t* g = groups.data() + off;
        for (int i = 0; i < 4; ++i) {
            xa28(g, i, false, left, &a);
            xa28(g, i, true, right, &b);
        }
    }
    const size_t n = std::min(a.size(), b.size());
    std::vector<int16_t> out(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = (int16_t)a[i];
        out[i * 2 + 1] = (int16_t)b[i];
    }
    if (nframes) *nframes = n;
    return out;
}

bool encodeFrames(const Movie& mov, const std::string& outPath,
                  const std::string& ffmpeg, double fps,
                  const std::string& wavPath, double asetrate,
                  const Progress& progress, std::string* error) {
    char sizeStr[32], fpsStr[32];
    std::snprintf(sizeStr, sizeof(sizeStr), "%dx%d", mov.width, mov.height);
    std::snprintf(fpsStr, sizeof(fpsStr), "%g", fps);

    std::vector<std::string> args = {"-hide_banner", "-loglevel", "error", "-y",
                                     "-f",           "rawvideo",  "-pix_fmt",
                                     "rgb24",        "-s",        sizeStr,
                                     "-r",           fpsStr,      "-i",
                                     "-"};
    if (!wavPath.empty()) {
        args.push_back("-i");
        args.push_back(wavPath);
        if (asetrate > 0) {
            char f[64];
            std::snprintf(f, sizeof(f), "asetrate=%d,aresample=22050",
                          (int)std::lround(asetrate));
            args.push_back("-filter:a");
            args.push_back(f);
        }
    }
    args.insert(args.end(), {"-c:v", "libx264", "-preset", "slow", "-crf", "16",
                             "-pix_fmt", "yuv420p"});
    if (!wavPath.empty())
        args.insert(args.end(),
                    {"-c:a", "aac", "-b:a", "192k", "-ar", "22050", "-ac", "2"});
    args.push_back(outPath);

    ChildProcess proc;
    if (!proc.start(ffmpeg, args, false, error)) return false;

    std::vector<uint8_t> rgb;
    for (const auto& frame : mov.frames) {
        if (progress.isCancelled()) {
            proc.terminate();
            proc.wait(nullptr);
            if (error) *error = "cancelled";
            return false;
        }
        std::string mdecErr;
        if (!mdecDecodeFrame(frame.data(), frame.size(), 0, mov.width,
                             mov.height, &rgb, &mdecErr)) {
            proc.terminate();
            proc.wait(nullptr);
            if (error) *error = "MDEC decode failed: " + mdecErr;
            return false;
        }
        if (!proc.writeStdin(rgb.data(), rgb.size())) break;
    }
    proc.closeStdin();
    int code = -1;
    proc.wait(&code);
    if (code != 0) {
        std::string err = proc.stderrText();
        if (error) *error = "ffmpeg failed: " + err;
        return false;
    }
    return true;
}

// PC release name aliases (tools/str_to_video.py PC_ALIASES).
const std::unordered_map<std::string, std::string>& pcAliases() {
    static const std::unordered_map<std::string, std::string> kAliases = {
        {"oj", "ou"},   {"pj", "pu"},   {"ed4", "eu4"},  {"ed5", "eu5"},
        {"stfc", "stfc_r"}, {"stfj", "stfj_r"}, {"stfz", "stfz_r"},
        {"staf", "staf_r"}};
    return kAliases;
}

}  // namespace

double probeDuration(const std::string& ffprobe, const std::string& path) {
    int code = -1;
    std::string out;
    std::string err;
    if (!ChildProcess::run(ffprobe,
                           {"-v", "error", "-show_entries", "format=duration",
                            "-of", "csv=p=0", path},
                           true, &code, &out, &err, nullptr))
        return -1;
    if (code != 0) return -1;
    try {
        return std::stod(out);
    } catch (...) {
        return -1;
    }
}

bool strParseFromDisc(const DiscImage& img, const DiscEntry& entry, Movie* out,
                      std::string* error) {
    if (entry.size == 0 || entry.size % 2048 != 0) {
        if (error) *error = "STR size is not a multiple of 2048";
        return false;
    }
    Movie mov;
    std::unordered_map<uint32_t, size_t> byFrame;
    std::vector<uint32_t> order;
    const uint32_t nsec = entry.size / 2048;
    mov.sectorCount = nsec;
    for (uint32_t s = 0; s < nsec; ++s) {
        uint8_t raw[2352];
        if (img.isRaw() && img.readRawSector(entry.lba + s, raw)) {
            const uint8_t submode = raw[18];
            if (submode & 0x04) {
                mov.audio.insert(mov.audio.end(), raw + 24, raw + 24 + 2304);
            } else if (submode & 0x02) {
                if (demuxVideoSector(raw + 24, 2048, &mov, &byFrame, &order))
                    mov.videoSectorEnd = s + 1;
            }
        } else {
            uint8_t buf[2048];
            if (!img.readUserData(entry.lba + s, buf, 2048)) continue;
            if (std::memcmp(buf, kMagic, 4) == 0) {
                if (demuxVideoSector(buf, 2048, &mov, &byFrame, &order))
                    mov.videoSectorEnd = s + 1;
            } else {
                mov.audio.insert(mov.audio.end(), buf, buf + 2048);
                mov.truncatedAudio = true;
            }
        }
    }
    *out = std::move(mov);
    return true;
}

bool strParseExtract(const std::vector<uint8_t>& data, Movie* out,
                     std::string* error) {
    if (data.size() % kSector != 0) {
        if (error) *error = "STR extract is not a multiple of 2048";
        return false;
    }
    Movie mov;
    std::unordered_map<uint32_t, size_t> byFrame;
    std::vector<uint32_t> order;
    const size_t nsec = data.size() / kSector;
    mov.sectorCount = nsec;
    for (size_t s = 0; s < nsec; ++s) {
        const uint8_t* chunk = data.data() + s * kSector;
        if (std::memcmp(chunk, kMagic, 4) == 0) {
            if (demuxVideoSector(chunk, kSector, &mov, &byFrame, &order))
                mov.videoSectorEnd = s + 1;
        } else {
            mov.audio.insert(mov.audio.end(), chunk, chunk + kSector);
            mov.truncatedAudio = true;
        }
    }
    *out = std::move(mov);
    return true;
}

bool convertStrMovie(const DiscImage& img, const DiscEntry& entry,
                     const std::string& outDir, const std::string& ffmpeg,
                     const Progress& progress, std::string* error,
                     const std::string& outStem) {
    Movie mov;
    if (!strParseFromDisc(img, entry, &mov, error)) return false;

    const std::string stem =
        fs::path(entry.path).stem().string();  // "DM1.STR" -> "DM1"
    mov.name = stem;

    std::string note = "  " + stem + ": " + std::to_string(mov.frames.size()) +
                       " frames, " + std::to_string(mov.width) + "x" +
                       std::to_string(mov.height) + ", v" +
                       std::to_string(mov.version) + ", " +
                       std::to_string(mov.audio.size() / 128) + " audio group(s)";
    if (mov.truncatedAudio) note += "  [TRUNCATED 2048-B source]";
    progress.info(note);

    const std::string key = toLower(stem);
    const double ps1dur = sourceDuration(mov);
    const size_t durationSectors = mov.videoSectorEnd > 0
        ? mov.videoSectorEnd
        : mov.sectorCount;
    const double sourceFps = sourceFrameRate(mov.frames.size(), durationSectors);
    double audioDur = 0;
    std::vector<int16_t> pcm;
    size_t nframes = 0;
    std::string wavPath;
    if (!mov.audio.empty()) {
        pcm = decodeXaStereo(mov.audio, &nframes);
        audioDur = (double)nframes / 37800.0;
        char b[128];
        std::snprintf(b, sizeof(b), "  audio: %.2f s @ 37800 Hz stereo", audioDur);
        progress.info(b);
        if (!progress.isCancelled()) {
            wavPath = joinPath(fs::temp_directory_path().string(),
                               "re1am_" + stem + ".wav");
            writeWav(wavPath, pcm, 37800);
        }
    }

    if (ps1dur > 0) {
        char b[160];
        std::snprintf(b, sizeof(b),
                      "  source timing: %.2f s (%zu/%zu sectors, %.3f fps)",
                      ps1dur, durationSectors, mov.sectorCount, sourceFps);
        progress.info(b);
    }

    double target = 0, asetrate = 0;
    if (target > 0 && audioDur > 0) asetrate = (double)nframes / target;

    double fps;
    if (target > 0) {
        fps = mov.frames.size() / target;
        char b[160];
        std::snprintf(b, sizeof(b),
                      "  pc movie: %.2f s (retiming from %.2f s, audio pitch "
                      "%+.1f%%)",
                      target, audioDur, (asetrate / 37800.0 - 1.0) * 100.0);
        progress.info(b);
    } else {
        fps = sourceFps > 0
            ? sourceFps
            : (audioDur > 0 ? mov.frames.size() / audioDur : 15.0);
    }

    makeDirs(outDir);
    const std::string outPath =
        joinPath(outDir, (outStem.empty() ? stem : outStem) + ".mp4");
    bool ok = encodeFrames(mov, outPath, ffmpeg, fps,
                           progress.isCancelled() ? std::string() : wavPath,
                           asetrate, progress, error);
    if (!wavPath.empty()) {
        std::error_code ec;
        fs::remove(wavPath, ec);
    }
    if (ok) progress.info("  wrote " + outPath);
    return ok;
}

bool convertPcMovies(const std::string& movieDir, const std::string& ffmpeg,
                     bool keepAvi, const Progress& progress,
                     std::string* error) {
    if (!isDirectory(movieDir)) return true;
    for (const auto& f : listDirectory(movieDir)) {
        if (extensionOf(f) != ".avi") continue;
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        const std::string inPath = joinPath(movieDir, f);
        const std::string outPath =
            joinPath(movieDir, fs::path(f).stem().string() + ".mp4");
        progress.info("  " + f + " -> " + fs::path(outPath).filename().string());
        int code = -1;
        std::string err;
        const bool ran = ChildProcess::run(
            ffmpeg,
            {"-hide_banner", "-loglevel", "error", "-y", "-i", inPath, "-c:v",
             "libx264", "-preset", "slow", "-crf", "18", "-pix_fmt", "yuv420p",
             "-c:a", "aac", "-b:a", "192k", outPath},
            false, &code, nullptr, &err, error);
        if (!ran) return false;
        if (code != 0) {
            if (error) *error = "ffmpeg failed for " + f + ": " + err;
            return false;
        }
        if (!keepAvi) {
            std::error_code ec;
            fs::remove(inPath, ec);
        }
    }
    return true;
}

}  // namespace re1
