#include "core/DiscImage.h"

#include "core/Util.h"

#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace re1 {

namespace {

uint32_t u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

std::string stripVersion(const std::string& name) {
    const size_t semi = name.find(';');
    return semi == std::string::npos ? name : name.substr(0, semi);
}

std::string dirName(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? std::string() : path.substr(0, p);
}

bool hasCd001(const uint8_t* d, size_t n, size_t off) {
    return off + 6 <= n && std::memcmp(d + off, "CD001", 5) == 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// MappedFile
// ---------------------------------------------------------------------------
MappedFile::~MappedFile() { close(); }

bool MappedFile::open(const std::string& path, std::string* error) {
    close();
#ifdef _WIN32
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    LARGE_INTEGER li;
    if (!GetFileSizeEx(f, &li) || li.QuadPart <= 0) {
        CloseHandle(f);
        if (error) *error = "cannot size " + path;
        return false;
    }
    HANDLE m = CreateFileMappingA(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m) {
        CloseHandle(f);
        if (error) *error = "cannot map " + path;
        return false;
    }
    const uint8_t* d =
        (const uint8_t*)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    if (!d) {
        CloseHandle(m);
        CloseHandle(f);
        if (error) *error = "cannot view " + path;
        return false;
    }
    m_file = f;
    m_map = m;
    m_data = d;
    m_size = (size_t)li.QuadPart;
    return true;
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        ::close(fd);
        if (error) *error = "cannot size " + path;
        return false;
    }
    void* d = mmap(nullptr, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (d == MAP_FAILED) {
        ::close(fd);
        if (error) *error = "cannot map " + path;
        return false;
    }
    m_fd = fd;
    m_data = (const uint8_t*)d;
    m_size = (size_t)st.st_size;
    return true;
#endif
}

void MappedFile::close() {
#ifdef _WIN32
    if (m_data) UnmapViewOfFile((LPCVOID)m_data);
    if (m_map) CloseHandle((HANDLE)m_map);
    if (m_file) CloseHandle((HANDLE)m_file);
    m_data = nullptr;
    m_map = nullptr;
    m_file = nullptr;
#else
    if (m_data) munmap((void*)m_data, m_size);
    if (m_fd >= 0) ::close(m_fd);
    m_data = nullptr;
    m_fd = -1;
#endif
    m_size = 0;
}

// ---------------------------------------------------------------------------
// DiscImage
// ---------------------------------------------------------------------------
bool DiscImage::open(const std::string& path, std::string* error) {
    m_entries.clear();
    m_path = path;

    std::string imagePath = path;
    bool forcedRaw = false;
    uint32_t forcedOffset = 0;
    bool haveForced = false;

    if (toLower(extensionOf(path)) == ".cue") {
        std::vector<uint8_t> cue;
        if (!re1::readFile(path, &cue)) {
            if (error) *error = "cannot read cue sheet " + path;
            return false;
        }
        std::istringstream in(std::string(cue.begin(), cue.end()));
        std::string line;
        while (std::getline(in, line)) {
            const std::string up = toUpper(line);
            if (!haveForced && up.find("FILE ") != std::string::npos) {
                const size_t q1 = line.find('"');
                const size_t q2 = q1 == std::string::npos
                                      ? std::string::npos
                                      : line.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    imagePath = joinPath(dirName(path),
                                         line.substr(q1 + 1, q2 - q1 - 1));
                }
            }
            if (!haveForced && up.find("TRACK ") != std::string::npos) {
                if (up.find("2048") != std::string::npos) {
                    forcedRaw = false;
                    forcedOffset = 0;
                } else if (up.find("2352") != std::string::npos) {
                    forcedRaw = true;
                    forcedOffset = up.find("MODE2") != std::string::npos ? 24 : 16;
                } else {
                    continue;
                }
                haveForced = true;
            }
        }
    }

    if (!m_file.open(imagePath, error)) return false;

    if (haveForced) {
        m_raw = forcedRaw;
        m_userOffset = forcedOffset;
    } else {
        const uint8_t* d = m_file.data();
        const size_t n = m_file.size();
        if (hasCd001(d, n, 16 * 2048 + 1)) {
            m_raw = false;
            m_userOffset = 0;
        } else if (hasCd001(d, n, 16 * 2352 + 16 + 1)) {
            m_raw = true;
            m_userOffset = 16;
        } else if (hasCd001(d, n, 16 * 2352 + 24 + 1)) {
            m_raw = true;
            m_userOffset = 24;
        } else if (n % 2352 == 0) {
            m_raw = true;
            m_userOffset = 24;
        } else {
            m_raw = false;
            m_userOffset = 0;
        }
    }

    return parseIso(error);
}

bool DiscImage::readUserData(uint32_t lba, uint8_t* out, size_t n) const {
    if (!m_file.isOpen() || n == 0) return false;
    const size_t stride = m_raw ? 2352u : 2048u;
    const size_t off = (size_t)lba * stride + (m_raw ? m_userOffset : 0);
    if (off + n > m_file.size()) return false;
    std::memcpy(out, m_file.data() + off, n);
    return true;
}

bool DiscImage::readRawSector(uint32_t lba, uint8_t* out2352) const {
    if (!m_file.isOpen() || !m_raw) return false;
    const size_t off = (size_t)lba * 2352u;
    if (off + 2352 > m_file.size()) return false;
    std::memcpy(out2352, m_file.data() + off, 2352);
    return true;
}

bool DiscImage::readDirData(uint32_t lba, uint32_t length,
                            std::vector<uint8_t>* out) const {
    out->clear();
    out->resize(length);
    uint32_t done = 0;
    uint32_t sector = 0;
    while (done < length) {
        uint8_t buf[2048];
        if (!readUserData(lba + sector, buf, 2048)) return false;
        const uint32_t take = std::min<uint32_t>(2048, length - done);
        std::memcpy(out->data() + done, buf, take);
        done += take;
        ++sector;
    }
    return true;
}

void DiscImage::parseDir(uint32_t lba, uint32_t length,
                         const std::string& prefix) {
    std::vector<uint8_t> data;
    if (!readDirData(lba, length, &data)) return;
    uint32_t i = 0;
    while (i < length) {
        const uint8_t n = data[i];
        if (n == 0) {
            i = (i / 2048 + 1) * 2048;
            continue;
        }
        if (i + n > data.size()) break;
        const uint8_t* rec = data.data() + i;
        const uint8_t nameLen = rec[32];
        if (33 + nameLen > n) {
            i += n;
            continue;
        }
        const std::string rawName((const char*)rec + 33, nameLen);
        const uint32_t extLba = u32le(rec + 2);
        const uint32_t size = u32le(rec + 10);
        const uint8_t flags = rec[25];
        // The first two records of a directory are "." and ".." (name length 1,
        // byte 0x00 / 0x01). Recursing into them loops forever.
        const bool selfOrParent =
            nameLen == 1 && (rec[33] == 0x00 || rec[33] == 0x01);
        if (!selfOrParent) {
            const bool isDir = (flags & 2) != 0;
            // File names carry an ISO9660 ";1" version suffix; drop it.
            const std::string name = isDir ? rawName : stripVersion(rawName);
            DiscEntry e;
            e.path = prefix + "/" + name;
            e.lba = extLba;
            e.size = size;
            e.directory = isDir;
            m_entries.push_back(e);
            if (e.directory) {
                parseDir(extLba, size, e.path);
            }
        }
        i += n;
    }
}

bool DiscImage::parseIso(std::string* error) {
    uint8_t pvd[2048];
    if (!readUserData(16, pvd, sizeof(pvd)) ||
        std::memcmp(pvd + 1, "CD001", 5) != 0) {
        if (error) *error = "no ISO9660 filesystem found in " + m_path;
        return false;
    }
    const uint8_t* root = pvd + 156;
    const uint32_t lba = u32le(root + 2);
    const uint32_t length = u32le(root + 10);
    parseDir(lba, length, "");
    return true;
}

const DiscEntry* DiscImage::find(const std::string& path) const {
    std::string want = toLower(path);
    if (!want.empty() && want[0] == '/') want.erase(0, 1);
    for (const auto& e : m_entries) {
        std::string p = toLower(e.path);
        if (!p.empty() && p[0] == '/') p.erase(0, 1);
        if (p == want) return &e;
    }
    return nullptr;
}

const DiscEntry* DiscImage::findInFolder(const std::string& folder,
                                         const std::string& name) const {
    const std::string tail = "/" + toUpper(folder) + "/" + toUpper(name);
    for (const auto& e : m_entries) {
        if (e.directory) continue;
        const std::string p = "/" + toUpper(e.path);
        if (p.size() >= tail.size() &&
            p.compare(p.size() - tail.size(), tail.size(), tail) == 0)
            return &e;
    }
    return nullptr;
}

bool DiscImage::readFile(const DiscEntry& entry,
                         std::vector<uint8_t>* out) const {
    out->clear();
    out->resize(entry.size);
    uint32_t done = 0;
    uint32_t sector = 0;
    while (done < entry.size) {
        uint8_t buf[2048];
        if (!readUserData(entry.lba + sector, buf, 2048)) return false;
        const uint32_t take = std::min<uint32_t>(2048, entry.size - done);
        std::memcpy(out->data() + done, buf, take);
        done += take;
        ++sector;
    }
    return true;
}

}  // namespace re1
