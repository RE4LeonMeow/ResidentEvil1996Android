#include "core/Ps1Audio.h"

#include "core/Process.h"
#include "core/Ps1AudioManifest.h"
#include "core/Spu.h"
#include "core/Util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace re1 {
namespace {

// Level calibration against the PC release, so the PS1 files drop into the
// mix the PC build was balanced for:
//  - SFX: the PC renders carry the tone's volume through libsd's squared
//    law, (tone vol * program vol / 127^2)^2 - measured over 152 confident
//    matches, r = 0.957 against that curve. spu::toneGain applies it.
//  - Voices: PC / PS1 RMS, median 0.575 (p10 0.458, p90 0.741) over 84 clips.
//  - BGM: each render is levelled to its PC file's RMS (Ps1BgmEntry::pcRms).
//    The PC normalised every BGM on its own, 0.66..3.5x a raw SPU render.
const double kVoiceGain = 0.575;
// A BGM with no PC file to level against: the median PC / render RMS ratio
// over the 35 channels whose one-pass length matches the PC file.
const double kBgmGain = 1.5;

// VOICE<n>.XAS: 16 channels interleaved sector by sector, 37.8 kHz mono.
const int kXaRate = 37800;
const int kXaChannels = 16;

void put16(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)(v & 0xFF));
    b.push_back((uint8_t)((v >> 8) & 0xFF));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
void putTag(std::vector<uint8_t>& b, const char* t) { b.insert(b.end(), t, t + 4); }

// A decoded sound, interleaved 16-bit. loopStart/loopEnd are frames
// ([start, end)), -1 when the sound does not loop.
struct Clip {
    std::vector<int16_t> pcm;
    int channels = 1;
    int rate = 44100;
    long loopStart = -1;
    long loopEnd = -1;
    size_t frames() const { return pcm.size() / (size_t)channels; }
    bool loops() const { return loopStart >= 0 && loopEnd > loopStart; }
};

// 16-bit PCM WAV with the migrator's marker chunk "re1p" and, for a looping
// clip, a `smpl` chunk. The game honours the loop only in marked files (the
// PC tree's own BGM_33.WAV has a smpl chunk the original ignored); see
// src/system/AudioFile.h.
std::vector<uint8_t> buildWav(const Clip& c) {
    const uint32_t dataBytes = (uint32_t)(c.pcm.size() * 2);
    std::vector<uint8_t> b;
    b.reserve(56 + dataBytes + 68);
    putTag(b, "RIFF");
    put32(b, 0);  // patched below
    putTag(b, "WAVE");
    putTag(b, "fmt ");
    put32(b, 16);
    put16(b, 1);
    put16(b, (uint32_t)c.channels);
    put32(b, (uint32_t)c.rate);
    put32(b, (uint32_t)(c.rate * c.channels * 2));
    put16(b, (uint32_t)(c.channels * 2));
    put16(b, 16);
    putTag(b, "re1p");
    put32(b, 4);
    put32(b, 1);
    putTag(b, "data");
    put32(b, dataBytes);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(c.pcm.data());
    b.insert(b.end(), p, p + dataBytes);
    if (c.loops()) {
        putTag(b, "smpl");
        put32(b, 36 + 24);
        put32(b, 0);                                  // manufacturer
        put32(b, 0);                                  // product
        put32(b, (uint32_t)(1000000000.0 / c.rate));  // sample period, ns
        put32(b, 60);                                 // MIDI unity note
        put32(b, 0);                                  // pitch fraction
        put32(b, 0);                                  // SMPTE format
        put32(b, 0);                                  // SMPTE offset
        put32(b, 1);                                  // loops
        put32(b, 0);                                  // sampler data
        put32(b, 0);                                  // cue point id
        put32(b, 0);                                  // forward loop
        put32(b, (uint32_t)c.loopStart);
        put32(b, (uint32_t)(c.loopEnd - 1));          // inclusive
        put32(b, 0);                                  // fraction
        put32(b, 0);                                  // play count: forever
    }
    const uint32_t riff = (uint32_t)(b.size() - 8);
    std::memcpy(b.data() + 4, &riff, 4);
    return b;
}

// Ogg Vorbis through ffmpeg (raw PCM on stdin). The loop and the marker go in
// as Vorbis comments: LOOPSTART / LOOPLENGTH (frames, the usual convention)
// and RE1PS1=1.
std::string encodeOgg(const std::string& ffmpeg, int quality, const Clip& c,
                      const std::string& out) {
    std::vector<std::string> args = {
        "-hide_banner", "-loglevel", "error", "-y", "-f", "s16le",
        "-ar", std::to_string(c.rate), "-ac", std::to_string(c.channels), "-i", "-",
        "-c:a", "libvorbis", "-q:a", std::to_string(quality),
        "-metadata", "RE1PS1=1"};
    if (c.loops()) {
        args.insert(args.end(), {"-metadata", "LOOPSTART=" + std::to_string(c.loopStart),
                                 "-metadata",
                                 "LOOPLENGTH=" + std::to_string(c.loopEnd - c.loopStart)});
    }
    args.push_back(out);
    ChildProcess proc;
    std::string err;
    if (!proc.start(ffmpeg, args, false, &err)) return "cannot start ffmpeg: " + err;
    proc.writeStdin(c.pcm.data(), c.pcm.size() * 2);
    proc.closeStdin();
    int code = -1;
    proc.wait(&code);
    if (code != 0) {
        std::string msg = proc.stderrText();
        if (msg.find("libvorbis") != std::string::npos || msg.find("Unknown encoder") != std::string::npos)
            msg += " (the ffmpeg build needs the libvorbis encoder)";
        return "ffmpeg failed for " + out + ": " + msg;
    }
    return std::string();
}

// Writes clips into the tree's Sound/Voice folders under the name the tree
// already uses, replacing the PC file: a WAV overwrites it, an OGG replaces it
// (the .wav is deleted once the .ogg is written, since the game prefers the
// .ogg anyway and two copies would only waste space). Records every file for
// Sound/PS1AUDIO.TXT.
class Writer {
public:
    Writer(const std::string& tree, const Ps1AudioOptions& o) : m_tree(tree), m_opts(o) {
        const unsigned hw = std::thread::hardware_concurrency();
        m_maxJobs = std::max(1u, std::min(hw ? hw : 4u, 8u));
    }

    bool init(std::string* error) {
        for (int v = 0; v < 2; ++v) {
            m_dir[v] = childDir(m_tree, v ? "Voice" : "Sound");
            if (!makeDirs(m_dir[v])) {
                if (error) *error = "cannot create " + m_dir[v];
                return false;
            }
            for (const auto& f : listDirectory(m_dir[v])) {
                const std::string ext = extensionOf(f);
                if (ext == ".wav" || ext == ".ogg")
                    m_existing[v][toLower(fs::path(f).stem().string())].push_back(f);
            }
        }
        return true;
    }

    bool write(bool voice, const std::string& name, Clip&& clip, std::string* error) {
        const int v = voice ? 1 : 0;
        const std::vector<std::string>& have = m_existing[v][toLower(name)];
        // Keep the tree's spelling so a case-sensitive tree ends up with one
        // file per name.
        std::string stem = toUpper(name);
        std::string wavSpelling;
        for (const auto& f : have) {
            stem = fs::path(f).stem().string();
            if (extensionOf(f) == ".wav") wavSpelling = f;
        }
        const bool ogg = m_opts.format == AudioFormat::Ogg;
        const std::string file = ogg ? stem + ".ogg" : (wavSpelling.empty() ? stem + ".WAV" : wavSpelling);
        std::vector<std::string> stale;
        for (const auto& f : have)
            if (f != file) stale.push_back(joinPath(m_dir[v], f));
        const std::string out = joinPath(m_dir[v], file);
        m_list.insert(std::string(voice ? "Voice/" : "Sound/") + file);

        if (!ogg) {
            if (!writeFile(out, buildWav(clip))) {
                if (error) *error = "cannot write " + out;
                return false;
            }
            removeAll(stale);
            return true;
        }
        if (!drain(m_maxJobs - 1, error)) return false;
        auto c = std::make_shared<Clip>(std::move(clip));
        const std::string ffmpeg = m_opts.ffmpegPath;
        const int q = m_opts.oggQuality;
        m_jobs.push_back({std::async(std::launch::async,
                                     [ffmpeg, q, c, out]() { return encodeOgg(ffmpeg, q, *c, out); }),
                          stale});
        return true;
    }

    // Wait for the encoders, then write the list (merged with an earlier
    // run's, for the files still on disk).
    bool finish(std::string* error) {
        if (!drain(0, error)) return false;
        const std::string listPath = joinPath(m_dir[0], ps1AudioListName());
        std::vector<uint8_t> old;
        if (readFile(listPath, &old)) {
            std::istringstream in(std::string(old.begin(), old.end()));
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty() || line[0] == ';') continue;
                if (isRegularFile(joinPath(m_tree, line))) m_list.insert(line);
            }
        }
        std::string text =
            "; Written by the RE1 asset migrator: the PS1 disc's audio, which replaced\n"
            "; the files below. Its presence makes the game mix at 44.1 kHz. Running the\n"
            "; PC migration again deletes these files and restores the PC release's.\n";
        for (const auto& f : m_list) text += f + "\n";
        if (!writeFile(listPath, reinterpret_cast<const uint8_t*>(text.data()), text.size())) {
            if (error) *error = "cannot write " + listPath;
            return false;
        }
        return true;
    }

    void abandon() {
        std::string ignored;
        drain(0, &ignored);
    }

private:
    struct Job {
        std::future<std::string> result;
        std::vector<std::string> stale;
    };

    static void removeAll(const std::vector<std::string>& paths) {
        std::error_code ec;
        for (const auto& p : paths) fs::remove(p, ec);
    }

    // Collect finished encodes until at most `keep` are still running.
    bool drain(size_t keep, std::string* error) {
        bool ok = true;
        while (m_jobs.size() > keep) {
            Job j = std::move(m_jobs.front());
            m_jobs.pop_front();
            const std::string err = j.result.get();
            if (!err.empty()) {
                if (ok && error) *error = err;
                ok = false;
                continue;
            }
            removeAll(j.stale);
        }
        return ok;
    }

    static std::string childDir(const std::string& dir, const std::string& name) {
        for (const auto& e : listDirectory(dir))
            if (toLower(e) == toLower(name) && isDirectory(joinPath(dir, e)))
                return joinPath(dir, e);
        return joinPath(dir, name);
    }

    std::string m_tree;
    Ps1AudioOptions m_opts;
    std::string m_dir[2];
    std::map<std::string, std::vector<std::string>> m_existing[2];
    std::set<std::string> m_list;
    std::deque<Job> m_jobs;
    size_t m_maxJobs = 4;
};

struct Bank {
    std::vector<uint8_t> hdrFile;   // .HED or the whole RDT
    std::vector<uint8_t> bodyFile;  // .VB (HED banks only)
    std::vector<uint8_t> records;   // the slot table, 4 bytes per slot
    spu::Vab vab;
    bool ok = false;
};

std::unique_ptr<Bank> loadBank(const DiscImage& img, const std::string& folder,
                               const std::string& file) {
    auto b = std::make_unique<Bank>();
    std::string err;
    if (folder == "SOUND") {
        const DiscEntry* h = img.findInFolder("SOUND", file + ".HED");
        const DiscEntry* v = img.findInFolder("SOUND", file + ".VB");
        if (!h || !v || !img.readFile(*h, &b->hdrFile) || !img.readFile(*v, &b->bodyFile))
            return b;
        const std::vector<uint8_t>& d = b->hdrFile;
        if (d.size() < 16) return b;
        // The Capcom wrapper: the slot table, then the VAB header at the
        // offset the file's last-but-one word gives.
        const uint32_t vo = readU32(d.data(), d.size(), d.size() - 8);
        if (vo == 0 || vo >= d.size()) return b;
        b->records.assign(d.begin(), d.begin() + vo);
        b->ok = b->vab.parse(d.data() + vo, d.size() - vo, b->bodyFile.data(),
                             b->bodyFile.size(), &err);
    } else {
        const DiscEntry* e = img.findInFolder(folder, file);
        if (!e || !img.readFile(*e, &b->hdrFile)) return b;
        const std::vector<uint8_t>& d = b->hdrFile;
        if (d.size() < 0x94) return b;
        // RDT +0x88 slot table, +0x8C VAB header, +0x90 VAB body.
        const uint32_t a88 = readU32(d.data(), d.size(), 0x88);
        const uint32_t a8c = readU32(d.data(), d.size(), 0x8C);
        const uint32_t a90 = readU32(d.data(), d.size(), 0x90);
        if (!(a88 < a8c && a8c < a90 && a90 <= d.size())) return b;
        b->records.assign(d.begin() + a88, d.begin() + a88 + 48 * 4);
        b->ok = b->vab.parse(d.data() + a8c, a90 - a8c, d.data() + a90,
                             d.size() - a90, &err);
    }
    return b;
}

// slot -> tone, the same rule the generator applies. An all-zero record is
// program 0 / tone 0, a real sound; a tone past its program's count is empty.
const spu::Tone* slotTone(const Bank& b, int slot, const spu::Program** prog) {
    if ((size_t)slot * 4 + 4 > b.records.size()) return nullptr;
    const int p = b.records[slot * 4 + 1];
    const int t = b.records[slot * 4 + 2];
    const spu::Program* pr = b.vab.program(p);
    if (!pr || t >= (int)pr->tones.size()) return nullptr;
    *prog = pr;
    return &pr->tones[t];
}

bool exportSfx(const DiscImage& img, Writer& w, const Progress& progress,
               Ps1AudioStats* st, std::string* error) {
    std::map<std::string, std::unique_ptr<Bank>> banks;
    // A name's rows are consecutive, best source first (see Ps1SfxEntry).
    for (size_t i = 0; i < kPs1SfxCount;) {
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        const Ps1SfxEntry& e = kPs1Sfx[i];
        size_t next = i + 1;
        while (next < kPs1SfxCount && std::strcmp(kPs1Sfx[next].pcName, e.pcName) == 0)
            ++next;
        struct Pick {
            const spu::Tone* tone = nullptr;
            const spu::Program* prog = nullptr;
            const uint8_t* vag = nullptr;
            size_t size = 0;
            std::string from;
        } exact, any;
        for (size_t k = i; k < next && !exact.vag; ++k) {
            const Ps1SfxEntry& a = kPs1Sfx[k];
            const std::string key = std::string(a.folder) + "/" + a.file;
            auto it = banks.find(key);
            if (it == banks.end())
                it = banks.emplace(key, loadBank(img, a.folder, a.file)).first;
            const Bank& b = *it->second;
            Pick p;
            p.tone = b.ok ? slotTone(b, a.slot, &p.prog) : nullptr;
            p.vag = p.tone ? b.vab.vagData(p.tone->vag, &p.size) : nullptr;
            p.from = key;
            if (!p.vag) continue;
            if (p.size == a.vagBytes) exact = p;
            else if (!any.vag) any = p;
        }
        const Pick& pick = exact.vag ? exact : any;
        i = next;
        if (!pick.vag) {
            progress.info(std::string("  ") + e.pcName +
                          ": not found on this disc, kept the PC file");
            ++st->skipped;
            continue;
        }
        if (!exact.vag)
            progress.info(std::string("  ") + e.pcName + ": " + pick.from +
                          " holds a re-encoded copy; used it (no original on this disc)");
        long loop = -1;
        bool repeats = false;
        Clip c;
        c.pcm = spu::decodeVag(pick.vag, pick.size, &loop, &repeats);
        const double g = spu::toneGain(*pick.tone, *pick.prog);
        for (int16_t& s : c.pcm) s = (int16_t)std::lround(s * g);
        c.rate = (int)std::lround(spu::toneRate(*pick.tone, pick.tone->minNote));
        if (repeats && loop >= 0) {
            c.loopStart = loop;
            c.loopEnd = (long)c.pcm.size();
        }
        if (!w.write(false, e.pcName, std::move(c), error)) return false;
        ++st->sfx;
        progress.phase((int)i, (int)kPs1SfxCount, "sound effects");
    }
    return true;
}

bool exportVoices(const DiscImage& img, Writer& w, const Progress& progress,
                  Ps1AudioStats* st, std::string* error) {
    if (!img.isRaw()) {
        progress.info("  voices skipped: the image has 2048-byte sectors, so the "
                      "CD-XA channels cannot be read; use a raw .bin/.cue");
        return true;
    }
    for (size_t i = 0; i < kPs1VoiceCount; ++i) {
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        const Ps1VoiceEntry& e = kPs1Voices[i];
        const DiscEntry* xas =
            img.findInFolder("VOICE", "VOICE" + std::to_string(e.xas) + ".XAS");
        if (!xas) {
            ++st->skipped;
            continue;
        }
        std::vector<std::vector<uint8_t>> raw;
        std::vector<const uint8_t*> sectors;
        raw.reserve(e.end - e.start);
        bool bad = false;
        for (uint32_t c = e.start; c < e.end; ++c) {
            raw.emplace_back(2352);
            const uint32_t lba = xas->lba + c * kXaChannels + e.channel;
            // Subheader: file, channel, submode (bit 2 = audio), coding.
            if (!img.readRawSector(lba, raw.back().data()) ||
                raw.back()[17] != e.channel || !(raw.back()[18] & 0x04)) {
                bad = true;
                break;
            }
        }
        if (bad) {
            progress.info(std::string("  ") + e.pcName +
                          ": XA sectors do not match the expected channel, kept the PC file");
            ++st->skipped;
            continue;
        }
        for (const auto& r : raw) sectors.push_back(r.data());
        Clip c;
        c.pcm = spu::decodeXaMono(sectors);
        c.rate = kXaRate;
        for (int16_t& s : c.pcm) s = (int16_t)std::lround(s * kVoiceGain);
        if (!w.write(true, e.pcName, std::move(c), error)) return false;
        ++st->voices;
        progress.phase((int)i + 1, (int)kPs1VoiceCount, "voices");
    }
    return true;
}

bool renderOne(const DiscImage& img, const Ps1BgmEntry& e,
               std::map<int, std::vector<uint8_t>>& cache, Clip* out, std::string* error) {
    auto it = cache.find(e.group);
    if (it == cache.end()) {
        char name[16];
        std::snprintf(name, sizeof(name), "SEP%02X.HSB", e.group);
        const DiscEntry* d = img.findInFolder("SOUND", name);
        std::vector<uint8_t> data;
        if (!d || !img.readFile(*d, &data)) {
            if (error) *error = std::string("cannot read SOUND/") + name;
            return false;
        }
        it = cache.emplace(e.group, std::move(data)).first;
    }
    spu::RenderedTrack tr;
    if (!spu::renderSepTrack(it->second.data(), it->second.size(), e.track, &tr, error))
        return false;
    // Level to the PC file (RMS of the mono mix, which is what the PC's mono
    // files are), but never past full scale: the gain is capped so the loudest
    // sample lands at 32767 and nothing is clipped.
    double sum = 0.0;
    int32_t peak = 1;
    for (size_t i = 0; i + 1 < tr.mix.size(); i += 2) {
        const double m = 0.5 * (tr.mix[i] + tr.mix[i + 1]);
        sum += m * m;
        peak = std::max(peak, std::max(std::abs(tr.mix[i]), std::abs(tr.mix[i + 1])));
    }
    const double rms = tr.mix.empty() ? 0.0 : std::sqrt(sum / (tr.mix.size() / 2));
    double gain = (e.pcRms > 0 && rms > 0) ? e.pcRms / rms : kBgmGain;
    gain = std::min(gain, 32767.0 / peak);
    out->pcm.resize(tr.mix.size());
    for (size_t i = 0; i < tr.mix.size(); ++i)
        out->pcm[i] =
            (int16_t)std::max(-32768L, std::min(32767L, std::lround(tr.mix[i] * gain)));
    out->channels = 2;
    out->rate = tr.rate;
    out->loopStart = tr.loopStart;
    out->loopEnd = tr.loopEnd;
    return true;
}

bool exportBgm(const DiscImage& img, Writer& w, const Progress& progress,
               Ps1AudioStats* st, std::string* error) {
    std::map<int, std::vector<uint8_t>> cache;
    for (size_t i = 0; i < kPs1BgmCount; ++i) {
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        const Ps1BgmEntry& e = kPs1Bgm[i];
        progress.phase((int)i, (int)kPs1BgmCount, std::string("BGM ") + e.pcName);
        std::string err;
        Clip c;
        if (!renderOne(img, e, cache, &c, &err)) {
            progress.info(std::string("  ") + e.pcName + ": " + err + ", kept the PC file");
            ++st->skipped;
            continue;
        }
        if (!w.write(false, e.pcName, std::move(c), error)) return false;
        ++st->bgm;
    }
    return true;
}

}  // namespace

bool migratePs1Audio(const DiscImage& img, const std::string& treeRoot,
                     const Ps1AudioOptions& opts, const Progress& progress,
                     std::string* error, Ps1AudioStats* stats) {
    Ps1AudioStats local;
    Ps1AudioStats* st = stats ? stats : &local;
    if (!img.findInFolder("SOUND", "BIO.HED")) {
        if (error) *error = "the image has no SOUND/BIO.HED; is it a Resident Evil PS1 disc?";
        return false;
    }
    Writer w(treeRoot, opts);
    if (!w.init(error)) return false;
    const char* fmt = opts.format == AudioFormat::Ogg ? "Ogg Vorbis" : "WAV";
    bool ok = true;
    if (ok && opts.sfx) {
        progress.info(std::string("PS1 sound effects: VAB banks -> Sound (") + fmt + ")");
        ok = exportSfx(img, w, progress, st, error);
    }
    if (ok && opts.voices) {
        progress.info(std::string("PS1 voices: CD-XA -> Voice (") + fmt + ")");
        ok = exportVoices(img, w, progress, st, error);
    }
    if (ok && opts.bgm) {
        progress.info(std::string("PS1 BGM: rendering the SEP sequences -> Sound (") + fmt + ")");
        ok = exportBgm(img, w, progress, st, error);
    }
    if (!ok) {
        w.abandon();
        return false;
    }
    if (!w.finish(error)) return false;
    progress.info("  " + std::to_string(st->sfx) + " sound effect(s), " +
                  std::to_string(st->voices) + " voice(s), " + std::to_string(st->bgm) +
                  " BGM channel(s) replaced; " + std::to_string(st->skipped) +
                  " left to the PC file");
    return true;
}

int removePs1Audio(const std::string& treeRoot, const Progress& progress) {
    std::string soundDir;
    for (const auto& e : listDirectory(treeRoot))
        if (toLower(e) == "sound" && isDirectory(joinPath(treeRoot, e)))
            soundDir = joinPath(treeRoot, e);
    if (soundDir.empty()) return 0;
    std::string listPath;
    for (const auto& e : listDirectory(soundDir))
        if (toUpper(e) == ps1AudioListName()) listPath = joinPath(soundDir, e);
    std::vector<uint8_t> data;
    if (listPath.empty() || !readFile(listPath, &data)) return 0;
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line;
    int removed = 0;
    std::error_code ec;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';') continue;
        if (fs::remove(joinPath(treeRoot, line), ec)) ++removed;
    }
    fs::remove(listPath, ec);
    progress.info("  removed " + std::to_string(removed) +
                  " PS1 audio file(s) from an earlier migration");
    return removed;
}

bool renderPs1Bgm(const DiscImage& img, const std::string& pcName,
                  const std::string& wavPath, std::string* error) {
    std::map<int, std::vector<uint8_t>> cache;
    for (size_t i = 0; i < kPs1BgmCount; ++i) {
        if (toUpper(kPs1Bgm[i].pcName) == toUpper(pcName)) {
            Clip c;
            if (!renderOne(img, kPs1Bgm[i], cache, &c, error)) return false;
            if (!writeFile(wavPath, buildWav(c))) {
                if (error) *error = "cannot write " + wavPath;
                return false;
            }
            return true;
        }
    }
    if (error) *error = "no BGM channel named " + pcName;
    return false;
}

}  // namespace re1
