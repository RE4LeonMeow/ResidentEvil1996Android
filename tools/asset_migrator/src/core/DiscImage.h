#pragma once
// Disc image reader: ISO9660 over a 2048-byte .iso or a raw 2352-byte .bin
// (Mode 1 / Mode 2), with .cue resolution. Also exposes raw sectors so the
// CD-XA audio of a PS1 .STR can be read without the truncation a 2048-byte
// "content copy" causes.

#include <cstdint>
#include <string>
#include <vector>

namespace re1 {

// A whole file mapped read-only into memory.
class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    bool open(const std::string& path, std::string* error);
    void close();
    bool isOpen() const { return m_data != nullptr; }
    const uint8_t* data() const { return m_data; }
    size_t size() const { return m_size; }

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
#ifdef _WIN32
    void* m_file = nullptr;   // HANDLE
    void* m_map = nullptr;    // HANDLE
#else
    int m_fd = -1;
#endif
};

struct DiscEntry {
    std::string path;      // '/'-separated, as stored (uppercase), version cut
    uint32_t lba = 0;
    uint32_t size = 0;     // bytes, from the ISO9660 directory record
    bool directory = false;
};

class DiscImage {
public:
    bool open(const std::string& path, std::string* error);
    bool isOpen() const { return m_file.isOpen(); }

    // True when sectors are 2352 bytes, i.e. the CD-XA audio is intact.
    bool isRaw() const { return m_raw; }
    const std::string& path() const { return m_path; }
    const std::vector<DiscEntry>& entries() const { return m_entries; }

    // Read `n` bytes (<= 2048) of user data starting at `lba`.
    bool readUserData(uint32_t lba, uint8_t* out, size_t n) const;

    // Read a whole raw 2352-byte sector. False on a 2048-byte image.
    bool readRawSector(uint32_t lba, uint8_t* out2352) const;

    // Read a file's user data (its ISO9660 `size` bytes) from its LBA.
    bool readFile(const DiscEntry& entry, std::vector<uint8_t>* out) const;

    // Case-insensitive lookup of a '/'-separated path.
    const DiscEntry* find(const std::string& path) const;

    // The file `<any folder>/<folder>/<name>`, case-insensitive. The disc nests
    // its files differently per release (root, /PSX/DATA, ...), so only the last
    // two path components are matched. Null when the disc has no such file.
    const DiscEntry* findInFolder(const std::string& folder,
                                  const std::string& name) const;

private:
    bool parseIso(std::string* error);
    void parseDir(uint32_t lba, uint32_t length, const std::string& prefix);
    bool readDirData(uint32_t lba, uint32_t length,
                     std::vector<uint8_t>* out) const;

    MappedFile m_file;
    std::string m_path;
    bool m_raw = false;
    uint32_t m_userOffset = 0;  // 0 = 2048 ISO, 16 = Mode 1, 24 = Mode 2
    std::vector<DiscEntry> m_entries;
};

}  // namespace re1
