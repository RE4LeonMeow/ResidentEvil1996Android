#include "core/Util.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace re1 {

const std::vector<std::string>& assetFolderNames() {
    static const std::vector<std::string> kNames = {
        "Data",   "Effspr", "Enemy", "Item_m1", "Item_m2", "Movie",
        "Objspr", "Players", "Sound", "Voice",   "Stage1",  "Stage2",
        "Stage3", "Stage4", "Stage5", "Stage6",  "Stage7",
    };
    return kNames;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return s;
}

bool isAssetFolderName(const std::string& name) {
    const std::string low = toLower(name);
    for (const auto& n : assetFolderNames()) {
        if (toLower(n) == low) return true;
    }
    return false;
}

std::string canonicalAssetFolder(const std::string& name) {
    const std::string low = toLower(name);
    for (const auto& n : assetFolderNames()) {
        if (toLower(n) == low) return n;
    }
    return {};
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const char last = a.back();
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}

std::string baseName(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string extensionOf(const std::string& path) {
    return toLower(fs::path(path).extension().string());
}

bool isDirectory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool isRegularFile(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool makeDirs(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec || fs::is_directory(path, ec);
}

std::vector<std::string> listDirectory(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(path, ec)) {
        out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool readFile(const std::string& path, std::vector<uint8_t>* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    out->resize(n < 0 ? 0 : (size_t)n);
    if (!out->empty()) f.read(reinterpret_cast<char*>(out->data()), n);
    return f.good() || f.eof();
}

bool writeFile(const std::string& path, const uint8_t* data, size_t size) {
    const std::string dir = fs::path(path).parent_path().string();
    if (!dir.empty() && !makeDirs(dir)) return false;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (size) f.write(reinterpret_cast<const char*>(data), (std::streamsize)size);
    return f.good();
}

uint32_t readU32(const uint8_t* data, size_t size, size_t off) {
    if (off + 4 > size) return 0;
    return (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8) |
           ((uint32_t)data[off + 2] << 16) | ((uint32_t)data[off + 3] << 24);
}

}  // namespace re1
