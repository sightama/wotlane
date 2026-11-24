// ABOUTME: Browser build only: the disc read in byte ranges without blocking the page's main thread, for browsers that don't serve a
// ABOUTME: worker's fetch while the main thread waits (Firefox). Main thread: awaits fetch through JSPI; other threads: synchronous XHR.
#ifdef __EMSCRIPTEN__
#include "wotm_web.h"
#include "wotm_discchunks.hpp"

#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/wasmfs.h>

// WASMFS internals (system/lib/wasmfs): a file whose reads this layer implements
#include "backend.h"
#include "file.h"
#include "wasmfs.h"

#include <cerrno>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr uint32_t kChunkBytes = 1u << 20;   // one range request per MiB the game touches, as the fetch backend does

// Both copy bytes [start, end) of `url` to dst and return how many arrived, or -1. On the main thread the page's fetch is
// web/data_retry.js's, which already retries network and 5xx errors and reports each chunk to web/loading.js; the synchronous
// XHR of the other threads retries itself and reports bytes only when web/disc_cache.js syncDiscXHR did not already report the read.
EM_ASYNC_JS(int, web_range_async, (const char *url, double start, double end, uint8_t *dst), {
    try {
        const r = await fetch(UTF8ToString(url), {headers: {Range: `bytes=${start}-${end - 1}`}});
        if (r.status !== 206 && r.status !== 200) return -1;
        let b = new Uint8Array(await r.arrayBuffer());
        if (r.status === 200) b = b.subarray(start, end);
        HEAPU8.set(b, dst);
        return b.length;
    } catch (e) {
        return -1;
    }
});

EM_JS(int, web_range_sync, (const char *url, double start, double end, uint8_t *dst), {
    const u = UTF8ToString(url);
    for (let attempt = 1; attempt <= 5; attempt++) {
        try {
            const x = new XMLHttpRequest();
            x.open('GET', u, false);
            x.responseType = 'arraybuffer';
            x.setRequestHeader('Range', `bytes=${start}-${end - 1}`);
            x.send();
            if (x.status === 206 || x.status === 200) {
                let b = new Uint8Array(x.response);
                if (x.status === 200) b = b.subarray(start, end);
                HEAPU8.set(b, dst);
                if (!x.wotmReported) {
                    globalThis.wotmDiscChannel = globalThis.wotmDiscChannel || new BroadcastChannel('wotlane-disc');
                    globalThis.wotmDiscChannel.postMessage(b.length);
                }
                return b.length;
            }
            if (x.status < 500 && x.status !== 429) return -1;
        } catch (e) {
        }
    }
    return -1;
});

class RangeFile : public wasmfs::DataFile
{
public:
    RangeFile(mode_t mode, wasmfs::backend_t backend, std::string url, uint64_t size)
        : DataFile(mode, backend), url_(std::move(url)), size_(size) {}

private:
    int open(wasmfs::oflags_t) override { return 0; }
    int close() override { return 0; }
    ssize_t write(const uint8_t *, size_t, off_t) override { return -EROFS; }
    int setSize(off_t) override { return -EROFS; }
    int flush() override { return 0; }
    off_t getSize() override { return static_cast<off_t>(size_); }

    ssize_t read(uint8_t *buf, size_t len, off_t offset) override
    {
        namespace dc = wotm::discchunks;
        const dc::Span s = dc::span(static_cast<uint64_t>(offset), len, size_, kChunkBytes);
        if (s.len == 0)
            return 0;
        std::lock_guard<std::mutex> lk(mu_);
        // the missing chunks of the read in one request: from the first missing one to the last
        uint64_t lo = s.first + s.count, hi = s.first;
        for (uint64_t i = s.first; i < s.first + s.count; ++i)
            if (!chunks_.count(i))
                lo = std::min(lo, i), hi = std::max(hi, i + 1);
        if (lo < hi && !fetchChunks(lo, hi))
        {
            std::fprintf(stderr, "[web] disc read of %s failed (offset %lld)\n", url_.c_str(), static_cast<long long>(offset));
            return -EIO;
        }
        dc::copyOut(buf, static_cast<uint64_t>(offset), s.len, kChunkBytes,
                    [&](uint64_t i) -> const std::vector<uint8_t> & { return chunks_[i]; });
        return static_cast<ssize_t>(s.len);
    }

    bool fetchChunks(uint64_t lo, uint64_t hi)
    {
        const uint64_t start = lo * kChunkBytes, end = std::min<uint64_t>(size_, hi * kChunkBytes);
        std::vector<uint8_t> bytes(static_cast<size_t>(end - start));
        const double s = static_cast<double>(start), e = static_cast<double>(end);
        const int got = emscripten_is_main_browser_thread() ? web_range_async(url_.c_str(), s, e, bytes.data())
                                                            : web_range_sync(url_.c_str(), s, e, bytes.data());
        if (got != static_cast<int>(bytes.size()))
            return false;
        for (uint64_t i = lo; i < hi; ++i)
        {
            const size_t at = static_cast<size_t>((i - lo) * kChunkBytes);
            chunks_[i].assign(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                              bytes.begin() + static_cast<std::ptrdiff_t>(std::min(bytes.size(), at + kChunkBytes)));
        }
        return true;
    }

    std::string url_;
    uint64_t size_;
    std::mutex mu_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> chunks_;
};

// makes RangeFiles for one URL; only files are created through it (the folder is a memory one)
class RangeBackend : public wasmfs::Backend
{
public:
    RangeBackend(std::string url, uint64_t size) : url_(std::move(url)), size_(size) {}
    std::shared_ptr<wasmfs::DataFile> createFile(mode_t mode) override { return std::make_shared<RangeFile>(mode, this, url_, size_); }
    std::shared_ptr<wasmfs::Directory> createDirectory(mode_t) override { return nullptr; }
    std::shared_ptr<wasmfs::Symlink> createSymlink(std::string) override { return nullptr; }

private:
    std::string url_;
    uint64_t size_;
};
} // namespace

bool wotmWebMountRangeDisc(const char *dir, const char *const *names, const uint64_t *sizes, int count)
{
    if (wasmfs_create_directory(dir, 0555, wasmfs_create_memory_backend()) != 0)
        return false;
    for (int i = 0; i < count; ++i)
    {
        // the C API's backend_t names the same object as wasmfs::backend_t (WASMFS's own C functions pass it across the same way)
        const backend_t b = reinterpret_cast<backend_t>(
            wasmfs::wasmFS.addBackend(std::make_unique<RangeBackend>(std::string("data/") + names[i], sizes[i])));
        const std::string path = std::string(dir) + "/" + names[i];
        if (wasmfs_create_file(path.c_str(), 0444, b) < 0)
            return false;
    }
    return true;
}
#endif
