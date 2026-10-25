#include "core/Assets.h"

#include "core/Util.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace re1 {
namespace {

std::string isoParent(const std::string& path) {
    const size_t p = path.find_last_of('/');
    return p == std::string::npos ? std::string() : path.substr(0, p);
}
std::string isoBase(const std::string& path) {
    const size_t p = path.find_last_of('/');
    return p == std::string::npos ? path : path.substr(p + 1);
}
std::string stripSlash(std::string s) {
    if (!s.empty() && s[0] == '/') s.erase(0, 1);
    return s;
}
bool startsWithCI(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    return toLower(s.substr(0, prefix.size())) == toLower(prefix);
}

int countAssetFoldersInDir(const std::string& dir) {
    int n = 0;
    if (!isDirectory(dir)) return 0;
    for (const auto& f : listDirectory(dir)) {
        if (isDirectory(joinPath(dir, f)) && isAssetFolderName(f)) ++n;
    }
    return n;
}

bool copyDirRecursive(const std::string& src, const std::string& dest,
                      const Progress& progress, size_t* count,
                      std::string* error) {
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(src, ec)) {
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        if (e.is_directory(ec)) continue;
        const std::string rel = fs::relative(e.path(), src, ec).generic_string();
        std::vector<uint8_t> data;
        if (!readFile(e.path().string(), &data)) continue;
        if (!writeFile(joinPath(dest, rel), data)) {
            if (error) *error = "cannot write " + joinPath(dest, rel);
            return false;
        }
        ++*count;
        if (*count % 200 == 0)
            progress.phase(-1, -1, std::to_string(*count) + " files copied");
    }
    return true;
}

bool extractDirFromImage(const DiscImage& img, const std::string& dirPath,
                         const std::string& destDir, const Progress& progress,
                         size_t* count, std::string* error) {
    std::string prefix = dirPath;
    if (prefix.empty()) prefix = "/";
    else prefix += "/";
    for (const auto& e : img.entries()) {
        if (e.directory || !startsWithCI(e.path, prefix)) continue;
        if (progress.isCancelled()) {
            if (error) *error = "cancelled";
            return false;
        }
        const std::string rel = stripSlash(e.path.substr(prefix.size()));
        if (rel.empty()) continue;
        std::vector<uint8_t> data;
        if (!img.readFile(e, &data)) continue;
        if (!writeFile(joinPath(destDir, rel), data)) {
            if (error) *error = "cannot write " + joinPath(destDir, rel);
            return false;
        }
        ++*count;
        if (*count % 200 == 0)
            progress.phase(-1, -1, std::to_string(*count) + " files extracted");
    }
    return true;
}

}  // namespace

bool findAssetRootInDir(const std::string& dir, std::string* root,
                        std::string* error) {
    if (!isDirectory(dir)) {
        if (error) *error = "not a folder: " + dir;
        return false;
    }
    if (countAssetFoldersInDir(dir) > 0) {
        *root = dir;
        return true;
    }
    for (const auto& f : listDirectory(dir)) {
        const std::string child = joinPath(dir, f);
        if (isDirectory(child) && countAssetFoldersInDir(child) > 0) {
            *root = child;
            return true;
        }
    }
    if (error)
        *error = "no asset folders (Data, Stage1, Enemy, ...) found under " +
                 dir;
    return false;
}

bool copyAssetsFromDir(const std::string& srcRoot, const std::string& destRoot,
                       const Progress& progress, std::string* error) {
    size_t count = 0;
    bool any = false;
    for (const auto& f : listDirectory(srcRoot)) {
        const std::string child = joinPath(srcRoot, f);
        if (!isDirectory(child) || !isAssetFolderName(f)) continue;
        any = true;
        const std::string dest = joinPath(destRoot, canonicalAssetFolder(f));
        progress.info("  " + f + " -> " + canonicalAssetFolder(f));
        if (!copyDirRecursive(child, dest, progress, &count, error)) return false;
    }
    if (!any) {
        if (error) *error = "no asset folders found in " + srcRoot;
        return false;
    }
    progress.info("  " + std::to_string(count) + " file(s) copied");
    return true;
}

bool findAssetPrefixInImage(const DiscImage& img, std::string* prefix,
                            std::string* error) {
    std::vector<std::string> candidates;
    candidates.push_back("");
    for (const auto& e : img.entries())
        if (e.directory) candidates.push_back(stripSlash(e.path));

    int best = 0;
    std::string bestPath;
    for (const auto& p : candidates) {
        int n = 0;
        for (const auto& e : img.entries()) {
            if (!e.directory) continue;
            if (stripSlash(isoParent(e.path)) != p) continue;
            if (isAssetFolderName(isoBase(e.path))) ++n;
        }
        if (n > best) {
            best = n;
            bestPath = p;
        }
    }
    if (best == 0) {
        if (error)
            *error = "no asset folders (DATA, STAGE1, ENEMY, ...) found in the "
                     "image";
        return false;
    }
    *prefix = bestPath;
    return true;
}

bool extractFoldersFromImage(const DiscImage& img,
                             const std::vector<std::string>& folders,
                             const std::string& destRoot,
                             const Progress& progress, std::string* error) {
    // The folders may sit at the image root or under a wrapper directory (the
    // retail DC disc nests everything under /PSX), so locate that first.
    std::string prefix;
    if (!findAssetPrefixInImage(img, &prefix, error)) return false;

    size_t count = 0;
    size_t found = 0;
    for (const auto& folder : folders) {
        const DiscEntry* dir = nullptr;
        for (const auto& e : img.entries()) {
            if (!e.directory) continue;
            if (stripSlash(isoParent(e.path)) != prefix) continue;
            if (toLower(isoBase(e.path)) != toLower(folder)) continue;
            dir = &e;
            break;
        }
        if (!dir) continue;
        ++found;
        progress.info("  " + folder);
        if (!extractDirFromImage(img, dir->path, joinPath(destRoot, folder),
                                 progress, &count, error))
            return false;
    }
    if (found == 0) {
        if (error)
            *error = "none of the expected asset folders were found in the "
                     "image; is it the right disc?";
        return false;
    }
    progress.info("  " + std::to_string(count) + " file(s) extracted");
    return true;
}

bool extractAssetsFromImage(const DiscImage& img, const std::string& destRoot,
                            const Progress& progress, std::string* error) {
    std::string prefix;
    if (!findAssetPrefixInImage(img, &prefix, error)) return false;
    if (!prefix.empty()) progress.info("  asset folder: " + prefix);

    size_t count = 0;
    bool any = false;
    for (const auto& e : img.entries()) {
        if (!e.directory) continue;
        if (stripSlash(isoParent(e.path)) != prefix) continue;
        const std::string name = isoBase(e.path);
        if (!isAssetFolderName(name)) continue;
        any = true;
        const std::string dest = joinPath(destRoot, canonicalAssetFolder(name));
        progress.info("  " + name + " -> " + canonicalAssetFolder(name));
        if (!extractDirFromImage(img, e.path, dest, progress, &count, error))
            return false;
    }
    if (!any) {
        if (error) *error = "no asset folders found in the image";
        return false;
    }
    progress.info("  " + std::to_string(count) + " file(s) extracted");
    return true;
}

}  // namespace re1
