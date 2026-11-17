// ABOUTME: The browser's save folder kept in memory (wotm_websave.cpp): unpacks the files the page reads from OPFS, and finds what to
// ABOUTME: write back (files new or changed by content hash, files removed) between two looks at the folder. Pure; tested in native/tests.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace wotm::savemirror
{
struct File
{
    std::string path;   // relative to the save folder, '/'-separated
    std::vector<uint8_t> bytes;
};

// path -> content hash, for every file in the folder
using Snapshot = std::map<std::string, uint64_t>;

struct Changes
{
    std::vector<std::string> changed, removed;   // in path order
};

// the page's pack: per file [u32 path length][path][u32 size][bytes], little-endian; false if it is cut short or a path is empty,
// absolute or has a ".." part
inline bool unpack(const uint8_t *data, size_t size, std::vector<File> &files)
{
    files.clear();
    size_t at = 0;
    const auto u32 = [&](uint32_t &n) {
        if (size - at < 4)
            return false;
        n = static_cast<uint32_t>(data[at]) | static_cast<uint32_t>(data[at + 1]) << 8 | static_cast<uint32_t>(data[at + 2]) << 16 |
            static_cast<uint32_t>(data[at + 3]) << 24;
        at += 4;
        return true;
    };
    while (at < size)
    {
        File f;
        uint32_t n = 0;
        if (!u32(n) || size - at < n)
            return false;
        f.path.assign(reinterpret_cast<const char *>(data + at), n);
        at += n;
        if (!u32(n) || size - at < n)
            return false;
        f.bytes.assign(data + at, data + at + n);
        at += n;
        if (f.path.empty() || f.path[0] == '/' || ("/" + f.path + "/").find("/../") != std::string::npos)
            return false;
        files.push_back(std::move(f));
    }
    return true;
}

// FNV-1a, 64 bits
inline uint64_t hash(const std::vector<uint8_t> &bytes)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint8_t b : bytes)
        h = (h ^ b) * 0x100000001b3ull;
    return h;
}

inline Changes diff(const Snapshot &before, const Snapshot &after)
{
    Changes c;
    for (const auto &[path, h] : after)
    {
        const auto it = before.find(path);
        if (it == before.end() || it->second != h)
            c.changed.push_back(path);
    }
    for (const auto &entry : before)
        if (!after.count(entry.first))
            c.removed.push_back(entry.first);
    return c;
}
} // namespace wotm::savemirror
