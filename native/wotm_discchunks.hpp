// ABOUTME: Chunk arithmetic for the browser's disc range reads (wotm_webdisc.cpp): which fixed-size chunks a read needs, clamped to the file,
// ABOUTME: the copy of the requested bytes out of them, and the file sizes the page passes. Pure; tested in native/tests.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace wotm::discchunks
{
// a read of `len` bytes at `offset` in a file of `size` bytes: chunks [first, first + count) and the bytes actually there (len)
struct Span
{
    uint64_t first = 0, count = 0;
    size_t len = 0;
};

inline Span span(uint64_t offset, size_t len, uint64_t size, uint32_t chunk)
{
    Span s;
    if (offset >= size || len == 0)
        return s;
    s.len = static_cast<size_t>(std::min<uint64_t>(len, size - offset));
    s.first = offset / chunk;
    s.count = (offset + s.len - 1) / chunk - s.first + 1;
    return s;
}

// copies bytes [offset, offset + len) out of the chunks; get(i) gives chunk i (chunk i starts at i * chunk; the last may be short)
template <typename Get> void copyOut(uint8_t *dst, uint64_t offset, size_t len, uint32_t chunk, Get get)
{
    while (len > 0)
    {
        const std::vector<uint8_t> &c = get(offset / chunk);
        const size_t at = static_cast<size_t>(offset % chunk);
        const size_t n = std::min(len, c.size() > at ? c.size() - at : size_t{0});
        if (n == 0)
            return;   // the chunk is shorter than the file says: stop rather than read past it
        std::memcpy(dst, c.data() + at, n);
        dst += n, offset += n, len -= n;
    }
}

// "name:bytes,name:bytes" (WOTM_DISC_SIZES, from the page's HEAD requests) -> the size of each of `names`, in their order; false
// if one is missing or not a number
inline bool parseSizes(const char *text, const char *const *names, int count, uint64_t *sizes)
{
    const std::string all = text ? text : "";
    for (int i = 0; i < count; ++i)
    {
        const std::string key = std::string(names[i]) + ":";
        size_t at = 0;   // the start of the "name:" entry
        while (all.compare(at, key.size(), key) != 0)
        {
            at = all.find(',', at);
            if (at == std::string::npos)
                return false;
            ++at;
        }
        const size_t from = at + key.size(), to = std::min(all.find(',', from), all.size());
        if (from == to || all.find_first_not_of("0123456789", from) < to)
            return false;
        sizes[i] = std::stoull(all.substr(from, to - from));
    }
    return true;
}
} // namespace wotm::discchunks
