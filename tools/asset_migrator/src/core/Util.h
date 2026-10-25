#pragma once
// Small filesystem/string helpers shared by the core. Kept Qt-free.

#include <string>
#include <vector>

namespace re1 {

// The canonical asset folder names the PC engine reads, in the order they are
// reported. Case is only a hint: lookups are case-insensitive.
const std::vector<std::string>& assetFolderNames();

// True when `name` matches one of assetFolderNames() case-insensitively.
bool isAssetFolderName(const std::string& name);

// Canonical spelling of an asset folder name ("" when not one).
std::string canonicalAssetFolder(const std::string& name);

// Recursively create `path`. Returns false on failure.
bool makeDirs(const std::string& path);

// Join two path fragments with '/'.
std::string joinPath(const std::string& a, const std::string& b);

// Lower-cased copy.
std::string toLower(std::string s);

// Upper-cased copy.
std::string toUpper(std::string s);

// File name (with extension) of a path.
std::string baseName(const std::string& path);

// Extension of a path, lower-cased, including the dot ("" when none).
std::string extensionOf(const std::string& path);

// True when the path exists and is a directory.
bool isDirectory(const std::string& path);

// True when the path exists and is a regular file.
bool isRegularFile(const std::string& path);

// Directory entries (names only, no "."/".."), unsorted.
std::vector<std::string> listDirectory(const std::string& path);

// Read an entire file. Returns false on failure.
bool readFile(const std::string& path, std::vector<uint8_t>* out);

// Write an entire file, creating parent folders. Returns false on failure.
bool writeFile(const std::string& path, const uint8_t* data, size_t size);

inline bool writeFile(const std::string& path, const std::vector<uint8_t>& d) {
    return writeFile(path, d.data(), d.size());
}

// Read a little-endian unsigned 32-bit value at `off` (0 when out of range).
uint32_t readU32(const uint8_t* data, size_t size, size_t off);

}  // namespace re1
