// ABOUTME: Browser build only: the save folder in memory, for browsers that don't serve a worker's OPFS calls while the main thread
// ABOUTME: waits (Firefox). Loaded from OPFS at boot, then each look at the folder writes new, changed and removed files back, on the page.
#ifdef __EMSCRIPTEN__
#include "wotm_web.h"
#include "wotm_savemirror.hpp"

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace
{
namespace sm = wotm::savemirror;
namespace fs = std::filesystem;

// Reads every file in OPFS into globalThis.wotmSavePack (savemirror::unpack's format) and returns its size, or -1
EM_ASYNC_JS(double, web_save_read_all, (), {
    const enc = new TextEncoder();
    const parts = [];
    async function walk(dir, prefix) {
        for await (const [name, h] of dir.entries()) {
            if (h.kind === "directory") await walk(h, prefix + name + "/");
            else parts.push(enc.encode(prefix + name), new Uint8Array(await (await h.getFile()).arrayBuffer()));
        }
    }
    try {
        await walk(await navigator.storage.getDirectory(), "");
    } catch (e) {
        console.error("[web] reading the saved files failed: " + e);
        return -1;
    }
    const pack = new Uint8Array(parts.reduce((n, p) => n + 4 + p.length, 0));
    const dv = new DataView(pack.buffer);
    let at = 0;
    for (const p of parts) {
        dv.setUint32(at, p.length, true);
        pack.set(p, at + 4);
        at += 4 + p.length;
    }
    globalThis.wotmSavePack = pack;
    return pack.length;
});

EM_JS(void, web_save_take_pack, (uint8_t *dst), {
    HEAPU8.set(globalThis.wotmSavePack, dst);
    delete globalThis.wotmSavePack;
});

// Queues a write (data) or a removal (data null) of `path` in OPFS; the queue keeps them in order, and a failure is logged
EM_JS(void, web_save_put, (const char *path, const uint8_t *data, int len), {
    const p = UTF8ToString(path);
    const bytes = data ? HEAPU8.slice(data, data + len) : null;   // a copy: the game may change the memory meanwhile
    globalThis.wotmSaveQueue = (globalThis.wotmSaveQueue || Promise.resolve()).then(async () => {
        const parts = p.split("/");
        let dir = await navigator.storage.getDirectory();
        for (const part of parts.slice(0, -1)) dir = await dir.getDirectoryHandle(part, {create: bytes !== null});
        if (bytes === null) return dir.removeEntry(parts[parts.length - 1]);
        const w = await (await dir.getFileHandle(parts[parts.length - 1], {create: true})).createWritable();
        await w.write(bytes);
        await w.close();
    }).catch((e) => { if (bytes !== null || e.name !== "NotFoundError") console.error("[web] saving " + p + " failed: " + e); });
});

std::string g_root;                                     // the folder in memory ("/save"), empty while it isn't mounted
sm::Snapshot g_snapshot;                                // content hash of each file as OPFS has it
std::map<std::string, std::pair<double, off_t>> g_seen; // last modified (ms) and size of each, to hash only what changed

double mtimeMs(const struct stat &st) { return st.st_mtim.tv_sec * 1000.0 + st.st_mtim.tv_nsec / 1e6; }

std::vector<uint8_t> readFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
} // namespace

bool wotmWebMountSaveMemory(const char *dir)
{
    if (wasmfs_create_directory(dir, 0777, wasmfs_create_memory_backend()) != 0)
        return false;
    g_root = dir;
    const double size = web_save_read_all();
    std::vector<sm::File> files;
    if (size > 0)
    {
        std::vector<uint8_t> pack(static_cast<size_t>(size));
        web_save_take_pack(pack.data());
        if (!sm::unpack(pack.data(), pack.size(), files))
            std::fprintf(stderr, "[web] the saved files didn't unpack; starting from an empty save folder\n");
    }
    for (const sm::File &f : files)
    {
        const fs::path path = fs::path(g_root) / f.path;
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(f.bytes.data()), static_cast<std::streamsize>(f.bytes.size()));
        g_snapshot[f.path] = sm::hash(f.bytes);
    }
    std::fprintf(stderr, "[web] /save: in memory, %zu file(s) from the browser's storage\n", files.size());
    wotmWebSaveSync();   // records each file's time and size (and writes nothing: the contents match)
    return true;
}

void wotmWebSaveSync()
{
    if (g_root.empty())
        return;
    sm::Snapshot now;
    std::map<std::string, std::vector<uint8_t>> read;   // contents read this time (changed files only)
    std::error_code ec;
    for (fs::recursive_directory_iterator it(g_root, ec), end; !ec && it != end; it.increment(ec))
    {
        struct stat st;
        if (stat(it->path().c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        const std::string rel = it->path().lexically_relative(g_root).generic_string();
        const std::pair<double, off_t> stamp{mtimeMs(st), st.st_size};
        const auto seen = g_seen.find(rel);
        const auto known = g_snapshot.find(rel);
        if (seen != g_seen.end() && seen->second == stamp && known != g_snapshot.end())
            now[rel] = known->second;
        else
        {
            std::vector<uint8_t> bytes = readFile(it->path().string());
            now[rel] = sm::hash(bytes);
            read[rel] = std::move(bytes);
        }
        g_seen[rel] = stamp;
    }
    if (ec)
        return;   // a look that failed halfway would read as removals
    const sm::Changes c = sm::diff(g_snapshot, now);
    for (const std::string &p : c.changed)
        web_save_put(p.c_str(), read[p].data(), static_cast<int>(read[p].size()));
    for (const std::string &p : c.removed)
    {
        web_save_put(p.c_str(), nullptr, 0);
        g_seen.erase(p);
    }
    g_snapshot = std::move(now);
}
#endif
