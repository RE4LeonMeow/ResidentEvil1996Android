#include "core/DcOverlay.h"

#include "core/Bss.h"
#include "core/Ps1Manifest.h"
#include "core/Tim.h"
#include "core/Util.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <set>
#include <unordered_set>

namespace fs = std::filesystem;

namespace re1 {
namespace {

constexpr size_t kChunk = 0x2840;  // one BT367OAB.TIM option cell
const char* kDcItemViewFiles[] = {"I00V_S1.IVM", "I60V_L.IVM", "I60V_R.IVM",
                                  "I99V.IVM"};
const int kFontClutColours = 16;
const int kFontClutRows = 4;
// fontus.tim / FONT.TIM glyphs only use palette indices 0-8.
const int kFontGlyphColours = 9;
const char* kFontClutRowNames[] = {"STANDARD (the PC font's own)",
                                   "TRAINING green", "ADVANCED red",
                                   "ADVANCED* grey"};

std::string joinParts(const std::string& base,
                      const std::vector<std::string>& parts) {
    std::string p = base;
    for (const auto& s : parts) p = joinPath(p, s);
    return p;
}

// Case-insensitive lookup of a child of `dir`.
std::string findChildCI(const std::string& dir, const std::string& name) {
    if (!isDirectory(dir)) return {};
    const std::string low = toLower(name);
    for (const auto& f : listDirectory(dir)) {
        if (toLower(f) == low) return joinPath(dir, f);
    }
    return {};
}

std::vector<std::string> filesWithExt(const std::string& dir,
                                      const std::string& ext) {
    std::vector<std::string> out;
    if (!isDirectory(dir)) return out;
    for (const auto& f : listDirectory(dir)) {
        if (!isRegularFile(joinPath(dir, f))) continue;
        if (!ext.empty() && extensionOf(f) != ext) continue;
        out.push_back(f);
    }
    return out;
}

std::vector<std::string> subDirectories(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_directory(ec)) out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Relative paths (with '/') of every file under `dir`.
std::vector<std::string> walkFiles(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        out.push_back(fs::relative(e.path(), dir, ec).generic_string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

struct Ctx {
    const DcOverlayOptions& o;
    const Progress& p;
    size_t written = 0;
    size_t skipped = 0;
    size_t shared = 0;
    size_t removed = 0;  // stale overlay files an earlier run left behind
};

bool writeOverlay(Ctx& c, const std::vector<std::string>& rel,
                  const std::vector<uint8_t>& data) {
    const std::string path = joinParts(c.o.overlayDir, rel);
    if (isRegularFile(path)) {
        std::vector<uint8_t> cur;
        if (readFile(path, &cur) && cur == data) {
            ++c.skipped;
            return true;
        }
    }
    if (!writeFile(path, data)) {
        c.p.info("  cannot write " + path);
        return false;
    }
    ++c.written;
    return true;
}

bool differsFromBase(const Ctx& c, const std::vector<std::string>& rel,
                     const std::vector<uint8_t>& data) {
    const std::string bp = joinParts(c.o.baseTreeDir, rel);
    std::vector<uint8_t> cur;
    if (!readFile(bp, &cur)) return true;
    return cur != data;
}

bool readSource(const Ctx& c, const std::vector<std::string>& rel,
                std::vector<uint8_t>* out) {
    return readFile(joinParts(c.o.sourceDir, rel), out);
}

// ---------------------------------------------------------------------------
// Steps
// ---------------------------------------------------------------------------
bool stepArrangeRdt(Ctx& c) {
    c.p.info("arrange_rdt: STAGE8-E/*.RDT");
    size_t n = 0;
    for (char sd : std::string("89ABCDE")) {
        const std::string srcDir =
            findChildCI(c.o.sourceDir, std::string("STAGE") + sd);
        if (srcDir.empty()) continue;
        const auto names = filesWithExt(srcDir, ".rdt");
        for (const auto& name : names) {
            std::vector<uint8_t> data;
            if (!readFile(joinPath(srcDir, name), &data)) continue;
            if (!writeOverlay(c, {"Stage" + std::string(1, sd), name}, data))
                return false;
        }
        if (!names.empty()) {
            c.p.info("  Stage" + std::string(1, sd) + ": " +
                     std::to_string(names.size()) + " room(s)");
        }
        n += names.size();
    }
    c.p.info("  " + std::to_string(n) + " arrange RDT(s) total");
    return true;
}

// FNV-1a-64. Must match fnv1a64() in tools/gen_ps1_manifest.py byte for byte,
// or the baked table stops recognising anything and the filter silently turns
// into a no-op.
uint64_t fnv1a64(const std::vector<uint8_t>& data) {
    // Hex, because the decimal spelling of the offset basis is 20 digits and a
    // dropped one still compiles: every hash comes out wrong, nothing in the
    // table ever matches, and the filter turns into a silent no-op.
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint8_t b : data) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    return h;
}

// The 1996 release's entry for `folder`/`name`, or null. kPs1Manifest is sorted
// by (folder, name) with both upper case, so this is a binary search.
const Ps1ManifestEntry* ps1Lookup(const std::string& folder,
                                  const std::string& name) {
    const std::string up = toUpper(name);
    size_t lo = 0, hi = kPs1ManifestCount;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const Ps1ManifestEntry& e = kPs1Manifest[mid];
        int cmp = folder.compare(e.folder);
        if (cmp == 0) cmp = up.compare(e.name);
        if (cmp == 0) return &e;
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return nullptr;
}

// True when `data` IS the 1996 file of that name - i.e. one the DC shipped
// unchanged. Size is checked first so the hash is only paid for on a candidate.
bool isPs1Original(const std::string& ps1Folder, const std::string& name,
                   const std::vector<uint8_t>& data) {
    const Ps1ManifestEntry* e = ps1Lookup(ps1Folder, name);
    return e != nullptr && e->size == data.size() && e->hash == fnv1a64(data);
}

// True when this file can be left out of the overlay: the DC never touched it
// AND the base tree has one of the same name to fall back to.
//
// The second half is not a nicety. ITEM_M2 holds document pages that both PS1
// discs ship and the PC tree does not have at all - FILEMARR.PIX is one - and
// they are only ever going to come from the overlay, so dropping one for being
// "not DC content" would just delete it. Unchanged-from-PS1 means "the base
// tree's copy is the better one", which presupposes there is a copy.
bool leaveToBaseTree(const std::string& ps1Folder, const std::string& baseDir,
                     const std::string& name,
                     const std::vector<uint8_t>& data) {
    if (ps1Folder.empty()) return false;
    if (!isPs1Original(ps1Folder, name, data)) return false;
    return !baseDir.empty() && !findChildCI(baseDir, name).empty();
}

// Drop a file the overlay should not be carrying, left by a run from before the
// filter existed.
//
// No content check: the overlay's copy is usually NOT byte-identical to the
// disc source, because stepTransparency rewrites the RDTs, EMDs, IVMs and TIMs
// in place afterwards to move them onto the PC's index-0 transparency key. A
// byte compare would spare exactly those, which is most of the ones worth
// clearing. Nor would the check protect a hand-edit: writeOverlay already
// overwrites any overlay file whose content differs, so an ordinary run would
// have flattened it regardless. The overlay is this tool's to rebuild.
void dropStaleOverlay(Ctx& c, const std::vector<std::string>& rel) {
    const std::string path = joinParts(c.o.overlayDir, rel);
    if (!isRegularFile(path)) return;
    std::error_code ec;
    if (fs::remove(path, ec)) {
        ++c.removed;
    } else {
        c.p.info("  cannot remove " + path);
    }
}

bool copyFolder(Ctx& c, const std::string& srcDir, const std::string& dstName,
                const std::string& ext, const std::string& ps1Folder = {}) {
    if (srcDir.empty()) {
        c.p.info("  " + dstName + ": not on the disc");
        return true;
    }
    size_t n = 0;
    size_t unchanged = 0;
    const std::string baseFolder = findChildCI(c.o.baseTreeDir, dstName);
    for (const auto& name : filesWithExt(srcDir, ext)) {
        std::vector<uint8_t> data;
        if (!readFile(joinPath(srcDir, name), &data)) continue;
        if (leaveToBaseTree(ps1Folder, baseFolder, name, data)) {
            // Not DC content. Leave it to the base tree, and clear out any copy
            // an earlier run put there.
            dropStaleOverlay(c, {dstName, name});
            ++unchanged;
            continue;
        }
        if (!writeOverlay(c, {dstName, name}, data)) return false;
        ++n;
    }
    if (unchanged) {
        c.p.info("  " + dstName + ": " + std::to_string(unchanged) +
                 " file(s) unchanged from the 1996 PS1 disc, left to the base "
                 "tree");
    }
    const std::string baseDir = findChildCI(c.o.baseTreeDir, dstName);
    if (!baseDir.empty()) {
        std::set<std::string> have;
        for (const auto& f : listDirectory(srcDir)) {
            have.insert(toLower(fs::path(f).stem().string()));
        }
        std::set<std::string> want;
        for (const auto& f : listDirectory(baseDir)) {
            want.insert(toLower(fs::path(f).stem().string()));
        }
        std::vector<std::string> shortNames;
        for (const auto& w : want)
            if (!have.count(w)) shortNames.push_back(w);
        c.p.info("  " + dstName + ": " + std::to_string(n) + " file(s); " +
                 std::to_string(shortNames.size()) +
                 " base name(s) the DC does not have");
    } else {
        c.p.info("  " + dstName + ": " + std::to_string(n) + " file(s)");
    }
    return true;
}

bool stepRooms(Ctx& c) {
    c.p.info("rooms: STAGE1-7/*.RDT");
    for (char sd : std::string("1234567")) {
        const std::string srcDir =
            findChildCI(c.o.sourceDir, std::string("STAGE") + sd);
        if (!copyFolder(c, srcDir, "Stage" + std::string(1, sd), ".rdt",
                        std::string("STAGE") + sd))
            return false;
    }
    return true;
}

bool stepModels(Ctx& c) {
    c.p.info("models: ENEMY, PLAYERS, ITEM_M1, ITEM_M2");
    const std::pair<const char*, const char*> folders[] = {
        {"ENEMY", "Enemy"},
        {"PLAYERS", "Players"},
        {"ITEM_M1", "Item_m1"},
        {"ITEM_M2", "Item_m2"},
    };
    for (const auto& f : folders) {
        if (!copyFolder(c, findChildCI(c.o.sourceDir, f.first), f.second, "",
                        f.first))
            return false;
    }
    return true;
}

bool stepData(Ctx& c) {
    c.p.info("data: DATA/*");
    const std::string srcDir = findChildCI(c.o.sourceDir, "DATA");
    const std::string baseDir = findChildCI(c.o.baseTreeDir, "Data");
    if (srcDir.empty() || baseDir.empty()) {
        c.p.info("  source or base Data folder missing, skipped");
        return true;
    }
    const std::unordered_set<std::string> own = {
        "title.pix", "item_all.pix", "item_mix.pix", "font.tim"};
    std::unordered_set<std::string> baseNames;
    for (const auto& f : listDirectory(baseDir)) baseNames.insert(toLower(f));
    // This step already restricts itself to names the base tree has, so the
    // fallback always exists and leaveToBaseTree's second test is a formality.
    size_t n = 0;
    size_t unchanged = 0;
    for (const auto& name : listDirectory(srcDir)) {
        if (!isRegularFile(joinPath(srcDir, name))) continue;
        const std::string low = toLower(name);
        if (own.count(low) || !baseNames.count(low)) continue;
        std::vector<uint8_t> data;
        if (!readFile(joinPath(srcDir, name), &data)) continue;
        if (leaveToBaseTree("DATA", baseDir, name, data)) {
            dropStaleOverlay(c, {"Data", name});
            ++unchanged;
            continue;
        }
        if (!writeOverlay(c, {"Data", name}, data)) return false;
        ++n;
    }
    if (unchanged) {
        c.p.info("  " + std::to_string(unchanged) +
                 " file(s) unchanged from the 1996 PS1 disc, left to the base "
                 "tree");
    }
    c.p.info("  " + std::to_string(n) + " shared DATA file(s)");
    return true;
}

bool stepEndingCredits(Ctx& c) {
    c.p.info("ending_credits: STAFF.STF, STAFF2.STF, BIO.TIM, EN05.TIM, EN07.TIM, CLIS01.PIX, JILL01.PIX");
    const std::pair<const char*, const char*> files[] = {
        {"STAFF.STF", "staff.stf"},
        {"STAFF2.STF", "staff2.stf"},
        {"BIO.TIM", "bio.tim"},
        {"EN05.TIM", "en05.tim"},
        {"EN07.TIM", "en07.tim"},
        {"CLIS01.PIX", "clis01.pix"},
        {"JILL01.PIX", "jill01.pix"},
    };
    for (const auto& f : files) {
        std::vector<uint8_t> data;
        if (!readSource(c, {"DATA", f.first}, &data)) {
            c.p.info(std::string("  ") + f.first + " not on the disc, skipped");
            continue;
        }
        if (!writeOverlay(c, {"Data", f.second}, data)) return false;
    }
    return true;
}

bool stepTitleBg(Ctx& c) {
    c.p.info("title_bg: TITLE.PIX -> Data/title.pix");
    std::vector<uint8_t> data;
    if (!readSource(c, {"DATA", "TITLE.PIX"}, &data)) {
        c.p.info("  DATA/TITLE.PIX not on the disc, skipped");
        return true;
    }
    if (!differsFromBase(c, {"Data", "title.pix"}, data))
        c.p.info("  note: identical to the base tree's title.pix");
    return writeOverlay(c, {"Data", "title.pix"}, data);
}

bool stepTitleMenu(Ctx& c) {
    c.p.info("title_menu: BT367OAB.TIM (copied; the engine splits it)");
    std::vector<uint8_t> data;
    if (!readSource(c, {"DATA", "BT367OAB.TIM"}, &data)) {
        c.p.info("  not on the disc, skipped");
        return true;
    }
    if (data.size() != 7 * kChunk) {
        c.p.info("  BT367OAB.TIM is " + std::to_string(data.size()) +
                 " B, expected 7 cells of " + std::to_string(kChunk));
        return false;
    }
    return writeOverlay(c, {"Data", "BT367OAB.TIM"}, data);
}

bool stepItemSprites(Ctx& c) {
    c.p.info("item_sprites: ITEM_ALL.PIX, ITEM_MIX.PIX");
    const std::pair<const char*, const char*> files[] = {
        {"ITEM_ALL.PIX", "item_all.pix"}, {"ITEM_MIX.PIX", "item_mix.pix"}};
    for (const auto& f : files) {
        std::vector<uint8_t> data;
        if (!readSource(c, {"DATA", f.first}, &data)) {
            c.p.info(std::string("  ") + f.first + " not on the disc, skipped");
            continue;
        }
        if (!differsFromBase(c, {"Data", f.second}, data))
            c.p.info(std::string("  note: ") + f.second +
                     " is identical to the base tree's");
        if (!writeOverlay(c, {"Data", f.second}, data)) return false;
    }
    return true;
}

bool stepItemModels(Ctx& c) {
    c.p.info("item_models: ITEM_M2/*.IVM");
    for (const char* name : kDcItemViewFiles) {
        std::vector<uint8_t> data;
        if (!readSource(c, {"ITEM_M2", name}, &data)) {
            c.p.info(std::string("  ") + name + " not on the disc, skipped");
            continue;
        }
        if (!writeOverlay(c, {"Item_m2", name}, data)) return false;
    }
    return true;
}

bool stepFont(Ctx& c) {
    const std::string name = c.o.baseName == "JPN" ? "FONT.TIM" : "fontus.tim";
    c.p.info("font: " + name + " + four CLUT rows for the DC save colours");
    const std::string pcFont = findChildCI(joinPath(c.o.baseTreeDir, "Data"), name);
    const std::string psFont = joinPath(c.o.sourceDir, "DATA/FONT.TIM");
    if (pcFont.empty() || !isRegularFile(psFont)) {
        c.p.info("  missing the base font or the DC FONT.TIM, skipped");
        return true;
    }
    std::vector<uint8_t> pc, ps;
    if (!readFile(pcFont, &pc) || !readFile(psFont, &ps)) {
        c.p.info("  cannot read a font, skipped");
        return true;
    }
    TimClut pcClut, psClut;
    std::string err;
    if (!timReadClut(pc, &pcClut, &err) || !timReadClut(ps, &psClut, &err)) {
        c.p.info("  " + err + ", skipped");
        return true;
    }
    if (pcClut.w % kFontClutColours != 0) {
        c.p.info("  the base font CLUT is not a whole number of 16-colour "
                 "palettes");
        return false;
    }
    // Row 0 is always the base font's own palette (what every non-DC string
    // already draws with); rows 1-3 are the PS1 colour columns 1-3. A JPN base
    // ships the PS1's wide CLUT itself, so it is its own source.
    std::vector<uint16_t> srcRow;
    std::string srcName;
    if (pcClut.w >= kFontClutRows * kFontClutColours) {
        srcRow = pcClut.rows[0];
        srcName = fs::path(pcFont).filename().string();
    } else {
        if (pcClut.x != psClut.x || pcClut.y != psClut.y) {
            c.p.info("  the two fonts put their CLUT at different VRAM spots");
            return false;
        }
        // Sanity check that the disc's font is the same colour layout: its
        // column 0 must carry the base palette. Only the entries the glyphs
        // use are compared, and without the STP bit - the USA DC disc's
        // column 0 is the PC palette verbatim, the Biohazard DC disc's has
        // STP clear and zeros past entry 8, and both have identical columns
        // 1-3.
        const auto& p0 = pcClut.rows[0];
        const auto& s0 = psClut.rows[0];
        bool same = (int)p0.size() >= kFontGlyphColours &&
                    (int)s0.size() >= kFontGlyphColours;
        for (int i = 0; same && i < kFontGlyphColours; ++i)
            same = (p0[i] & 0x7FFF) == (s0[i] & 0x7FFF);
        if (!same) {
            c.p.info("  the base font's palette is not the PS1 CLUT's column 0");
            return false;
        }
        srcRow = psClut.rows[0];
        srcName = "the DC FONT.TIM";
    }
    if ((int)srcRow.size() < kFontClutRows * kFontClutColours) {
        c.p.info("  " + srcName + " has too few CLUT entries");
        return false;
    }
    std::vector<std::vector<uint16_t>> rows(kFontClutRows);
    rows[0] = std::vector<uint16_t>(pcClut.rows[0].begin(),
                                    pcClut.rows[0].begin() + kFontClutColours);
    for (int r = 1; r < kFontClutRows; ++r)
        rows[r] = std::vector<uint16_t>(
            srcRow.begin() + r * kFontClutColours,
            srcRow.begin() + (r + 1) * kFontClutColours);
    const std::vector<uint8_t> out = timWithClut(pc, pcClut, rows);
    c.p.info("  colours from " + srcName);
    return writeOverlay(c, {"Data", fs::path(pcFont).filename().string()}, out);
}

bool stepTransparency(Ctx& c) {
    c.p.info("transparency: PS1 colour-key -> PC index-0 key");
    if (!isDirectory(c.o.overlayDir)) {
        c.p.info("  no overlay yet, skipped");
        return true;
    }
    size_t files = 0, texels = 0;
    for (const auto& rel : walkFiles(c.o.overlayDir)) {
        if (c.p.isCancelled()) {
            c.p.info("  cancelled");
            return false;
        }
        const std::string path = joinPath(c.o.overlayDir, rel);
        std::vector<uint8_t> data;
        if (!readFile(path, &data)) continue;
        // An overlay file byte-identical to the base tree's is not DC art.
        const std::string bp = joinPath(c.o.baseTreeDir, rel);
        std::vector<uint8_t> baseData;
        if (readFile(bp, &baseData) && baseData == data) {
            ++c.shared;
            continue;
        }
        std::vector<uint8_t> buf = data;
        int moved = 0;
        for (const auto& blk : iterTimBlocks(data))
            moved += foldTimTransparency(&buf, blk);
        if (moved == 0) continue;
        ++files;
        texels += moved;
        if (!writeFile(path, buf)) {
            c.p.info("  cannot write " + path);
            return false;
        }
    }
    c.p.info("  " + std::to_string(files) + " file(s), " +
             std::to_string(texels) + " texel(s) moved onto index 0 (" +
             std::to_string(c.shared) + " shared with the base left alone)");
    return true;
}

// ---------------------------------------------------------------------------
// Backgrounds
// ---------------------------------------------------------------------------
bool stepBackgrounds(Ctx& c) {
    if (c.o.backgrounds == Backgrounds::None) {
        c.p.info("backgrounds: skipped");
        return true;
    }
    c.p.info("backgrounds: STAGE*/**.BSS -> Stage*/RC*.pak");
    std::string stages = "1234567";
    if (c.o.backgrounds == Backgrounds::All) stages += "89ABCDE";
    size_t total = 0;
    size_t unchangedRooms = 0;
    for (char sd : stages) {
        const std::string srcDir =
            findChildCI(c.o.sourceDir, std::string("STAGE") + sd);
        if (srcDir.empty()) continue;
        const std::string outDir = joinPath(c.o.overlayDir,
                                            "Stage" + std::string(1, sd));
        // Arrange stages (8-E) are not in the manifest at all, so nothing
        // matches there and the filter sits them out.
        const std::string ps1Folder = std::string("STAGE") + sd;
        const std::string baseStageDir =
            findChildCI(c.o.baseTreeDir, "Stage" + std::string(1, sd));
        for (const auto& bss : filesWithExt(srcDir, ".bss")) {
            if (c.p.isCancelled()) {
                c.p.info("  cancelled");
                return false;
            }
            const std::string stem = fs::path(bss).stem().string();
            if (stem.size() < 7) continue;
            const char stageDigit = stem[4];
            int roomId = 0;
            {
                auto hex = [](char ch) -> int {
                    if (ch >= '0' && ch <= '9') return ch - '0';
                    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                    return -1;
                };
                const int hi = hex(stem[5]);
                const int lo = hex(stem[6]);
                if (hi < 0 || lo < 0) continue;
                roomId = hi * 16 + lo;
            }
            std::vector<uint8_t> data;
            if (!readFile(joinPath(srcDir, bss), &data)) continue;

            // A .BSS the DC never touched holds the 1996 shots, and the base
            // tree's paks are the PC release's own encode of those same shots,
            // so ours would only shadow them.
            //
            // The room is converted first and judged after, because the deal
            // only holds if the base tree covers EVERY camera. The PS1 has
            // angles the PC release dropped - stage 3 room 06 camera 4, six in
            // stage 4, two in stage 5 - and those nine paks exist nowhere else,
            // so a room is kept whole rather than left half-covered. Deciding
            // up front would mean guessing the camera count; convertBss reports
            // it, and the whole pass costs seconds.
            //
            // Coverage is judged by NAME, not content: the overlay's paks come
            // out of this converter and the base tree's came out of Capcom's,
            // so they never match byte for byte even for the same photograph.
            const bool unchanged = !baseStageDir.empty() &&
                                   isPs1Original(ps1Folder, bss, data);
            // What the overlay already had for this room, so the summary counts
            // only the paks an earlier run left behind, not the ones this pass
            // wrote and then withdrew.
            std::set<std::string> preexisting;
            if (unchanged) {
                const std::string prefix = toLower(
                    std::string("rc") + stageDigit + stem.substr(5, 2));
                for (const auto& f : listDirectory(outDir)) {
                    const std::string low = toLower(f);
                    if (low.rfind(prefix, 0) == 0 && extensionOf(low) == ".pak")
                        preexisting.insert(low);
                }
            }

            std::vector<std::string> written;
            std::string err;
            if (!convertBss(data, stageDigit, roomId, outDir, c.p, &written,
                            &err)) {
                c.p.info("  " + err);
                return false;
            }

            if (unchanged) {
                bool baseHasAll = !written.empty();
                for (const auto& f : written) {
                    if (findChildCI(baseStageDir, f).empty()) {
                        baseHasAll = false;
                        break;
                    }
                }
                if (baseHasAll) {
                    for (const auto& f : written) {
                        std::error_code rec;
                        if (fs::remove(joinPath(outDir, f), rec) &&
                            preexisting.count(toLower(f)))
                            ++c.removed;
                    }
                    ++unchangedRooms;
                    continue;
                }
                c.p.info("  " + bss +
                         ": unchanged from the 1996 disc, but the base tree is "
                         "short a camera - keeping all " +
                         std::to_string(written.size()) + " pak(s)");
            }

            total += written.size();
            c.p.info("  " + bss + " -> " + std::to_string(written.size()) +
                     " pak(s)");
        }
    }
    if (unchangedRooms) {
        c.p.info("  " + std::to_string(unchangedRooms) +
                 " room(s) unchanged from the 1996 PS1 disc, left to the base "
                 "tree");
    }
    c.p.info("  " + std::to_string(total) + " background pak(s)");
    return true;
}

// ---------------------------------------------------------------------------
// Verify
// ---------------------------------------------------------------------------
const char* kStageDigits = "123456789ABCDE";

std::string bgStageDigit(int stageId) {
    int row = stageId >= 7 ? stageId - 7 : stageId;
    int sid = stageId;
    if (row > 4) sid -= 5;
    if (sid < 0) sid = 0;
    if (sid > 13) sid = 13;
    return std::string(1, kStageDigits[sid]);
}

bool checkCoverage(Ctx& c, std::string* error) {
    c.p.info("1. base-tree files the DC tree does not carry");
    if (!isDirectory(c.o.overlayDir)) {
        if (error) *error = "overlay missing";
        return false;
    }
    if (!isDirectory(c.o.baseTreeDir)) {
        c.p.info("   base tree missing, skipped");
        return true;
    }
    const std::set<std::string> fallback = {"sound", "voice", "movie", "effspr",
                                            "objspr"};
    const std::set<std::string> partial = {"data", "item_m2", "players"};
    const std::vector<std::string> ignoreExt = {".scd", ".bak", ".ppm",
                                                ".tmext", ".tga", ".bin"};
    std::vector<std::string> problems;

    for (const auto& folder : subDirectories(c.o.baseTreeDir)) {
        const std::string low = toLower(folder);
        const std::string baseDir = joinPath(c.o.baseTreeDir, folder);
        const std::string ovDir = findChildCI(c.o.overlayDir, folder);
        std::set<std::string> have;
        if (!ovDir.empty())
            for (const auto& f : listDirectory(ovDir)) have.insert(toLower(f));
        std::set<std::string> want;
        for (const auto& f : listDirectory(baseDir)) {
            const std::string lf = toLower(f);
            bool skip = false;
            for (const auto& e : ignoreExt)
                if (lf.size() >= e.size() &&
                    lf.compare(lf.size() - e.size(), e.size(), e) == 0)
                    skip = true;
            if (!skip) want.insert(lf);
        }
        std::vector<std::string> missing;
        for (const auto& w : want)
            if (!have.count(w)) missing.push_back(w);

        if (fallback.count(low)) {
            c.p.info("   " + folder + ": " + std::to_string(missing.size()) +
                     " missing (expected - falls back)");
        } else if (partial.count(low)) {
            c.p.info("   " + folder + ": " + std::to_string(missing.size()) +
                     " missing (known partial)");
        } else if (low.rfind("stage", 0) == 0) {
            std::vector<std::string> rooms, others;
            for (const auto& m : missing) {
                if (m.rfind("room", 0) == 0 && m.size() >= 4 &&
                    m.compare(m.size() - 4, 4, ".rdt") == 0)
                    rooms.push_back(m);
                else if (m.rfind("rc", 0) != 0)
                    others.push_back(m);
            }
            if (!rooms.empty())
                c.p.info("   " + folder + ": " + std::to_string(rooms.size()) +
                         " room(s) the DC does not ship - fall back");
            if (!others.empty()) {
                c.p.info("   " + folder + ": " +
                         std::to_string(others.size()) + " unexpected missing");
                problems.push_back(folder + " is short " +
                                   std::to_string(others.size()) + " file(s)");
            }
            if (rooms.empty() && others.empty())
                c.p.info("   " + folder + ": complete");
        } else if (!missing.empty()) {
            // A gap here is the normal outcome, not a half-populated overlay:
            // the DC never changed those files, so they are deliberately left
            // to the base tree.
            c.p.info("   " + folder + ": " + std::to_string(missing.size()) +
                     " missing (expected - not DC content, falls back)");
        } else {
            c.p.info("   " + folder + ": complete");
        }
    }
    if (!problems.empty()) {
        if (error) *error = problems[0];
        return false;
    }
    return true;
}

bool checkBackgrounds(Ctx& c, std::string* error) {
    c.p.info("2. every DC room resolves to a background");
    std::vector<std::string> missing;
    int checked = 0;
    for (int stageId = 0; stageId < 14; ++stageId) {
        const std::string digit(1, kStageDigits[stageId]);
        const std::string stageDir = joinPath(c.o.overlayDir, "Stage" + digit);
        if (!isDirectory(stageDir)) continue;
        const std::string src = bgStageDigit(stageId);
        std::set<int> rooms;
        for (const auto& f : listDirectory(stageDir)) {
            const std::string up = toUpper(f);
            if (up.size() != 12 || up.rfind("ROOM", 0) != 0 ||
                up.compare(8, 4, ".RDT") != 0)
                continue;
            auto hex = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            const int hi = hex(up[5]), lo = hex(up[6]);
            if (hi >= 0 && lo >= 0) rooms.insert(hi * 16 + lo);
        }
        for (int rid : rooms) {
            ++checked;
            char name[32];
            std::snprintf(name, sizeof(name), "RC%s%02X0.pak", src.c_str(), rid);
            if (isRegularFile(joinPath(joinPath(c.o.overlayDir, "Stage" + src),
                                       name)))
                continue;
            // A room whose backgrounds the DC never changed has no pak of its
            // own and resolves to the base tree at runtime, which is the point.
            // It only counts as missing when neither tree has it.
            {
                const std::string baseStage =
                    findChildCI(c.o.baseTreeDir, "Stage" + src);
                if (!baseStage.empty() && !findChildCI(baseStage, name).empty())
                    continue;
            }
            missing.push_back("Stage" + digit + " room 0x" +
                              std::to_string(rid) + " -> " + name);
        }
    }
    c.p.info("   " + std::to_string(checked) +
             " room(s) checked, " + std::to_string(missing.size()) +
             " without a camera-0 background");
    if (!missing.empty()) {
        if (error) *error = missing[0];
        return false;
    }
    return true;
}

}  // namespace

bool buildDcOverlay(const DcOverlayOptions& opts, const Progress& progress,
                    std::string* error) {
    Ctx c{opts, progress};
    if (!makeDirs(opts.overlayDir)) {
        if (error) *error = "cannot create " + opts.overlayDir;
        return false;
    }
    const std::pair<const char*, bool (*)(Ctx&)> steps[] = {
        {"arrange_rdt", stepArrangeRdt}, {"rooms", stepRooms},
        {"models", stepModels},          {"data", stepData},
        {"title_bg", stepTitleBg},       {"title_menu", stepTitleMenu},
        {"font", stepFont},              {"item_sprites", stepItemSprites},
        {"item_models", stepItemModels}, {"transparency", stepTransparency},
        {"ending_credits", stepEndingCredits},
    };
    for (const auto& s : steps) {
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        if (!s.second(c)) {
            if (error) *error = "step " + std::string(s.first) + " failed";
            return false;
        }
    }
    if (!stepBackgrounds(c)) {
        if (error) *error = "backgrounds failed";
        return false;
    }
    std::string summary = "overlay: wrote " + std::to_string(c.written) +
                          " file(s), " + std::to_string(c.skipped) +
                          " up to date";
    if (c.removed) {
        summary += ", removed " + std::to_string(c.removed) +
                   " that an earlier run took from the 1996 PS1 disc";
    }
    progress.info(summary);
    return true;
}

bool verifyDcOverlay(const DcOverlayOptions& opts, const Progress& progress,
                     std::string* error) {
    Ctx c{opts, progress};
    bool ok = checkCoverage(c, error);
    ok = checkBackgrounds(c, error) && ok;
    if (!ok) {
        progress.info("FAIL: the DC tree is incomplete");
        return false;
    }
    progress.info("ok: the DC tree covers everything it owns");
    return true;
}

}  // namespace re1
