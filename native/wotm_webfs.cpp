// ABOUTME: Browser build only: mounts the streamed disc (/disc, read lazily from data/) and the save folder (/save, OPFS), paces the
// ABOUTME: render loop on requestAnimationFrame (JSPI suspends it between frames), and shares the tab and network state with the page.
#ifdef __EMSCRIPTEN__
#include "wotm_web.h"
#include "wotm_discchunks.hpp"

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unistd.h>

namespace
{
    constexpr unsigned kDiscChunkBytes = 1u << 20;   // one range request per MiB the game touches; each chunk is fetched once
    constexpr const char *kDiscFiles[] = {"SCUS_971.97", "game.iso"};
    std::atomic<bool> g_hidden{false};
    std::atomic<double> g_stallMs{0};
    std::mutex g_stallMu;
    std::string g_stallText;
}

// globalThis.wotmFrames: frames presented, time spent in the loop's own work and time waiting for the browser (the page shows the rate)
EM_ASYNC_JS(void, wotm_web_wait_frame, (), {
    const s = globalThis.wotmFrames = globalThis.wotmFrames || {frames: 0, workMs: 0, waitMs: 0, last: performance.now()};
    const t0 = performance.now();
    s.workMs += t0 - s.last;
    await new Promise(resolve => requestAnimationFrame(resolve));
    s.last = performance.now();
    s.waitMs += s.last - t0;
    s.frames++;
});

void wotmWebPresent() { wotm_web_wait_frame(); }

bool wotmWebHidden() { return g_hidden.load(); }

void wotmWebQuit()
{
    MAIN_THREAD_EM_ASM({ if (globalThis.wotmQuit) globalThis.wotmQuit(); });
}

void wotmWebPageSize(int &w, int &h)
{
    w = MAIN_THREAD_EM_ASM_INT({ return window.innerWidth; });
    h = MAIN_THREAD_EM_ASM_INT({ return window.innerHeight; });
}

void wotmWebNetStall(double ms, const char *text)
{
    std::lock_guard<std::mutex> lk(g_stallMu);
    g_stallText = text;
    g_stallMs = ms;
}

// called by the page (boot.js)
extern "C"
{
    EMSCRIPTEN_KEEPALIVE void wotm_web_set_hidden(int hidden) { g_hidden = hidden != 0; }
    EMSCRIPTEN_KEEPALIVE double wotm_web_net_stall_ms() { return g_stallMs.load(); }
    // the text stays valid until the next call (the page reads it right away)
    EMSCRIPTEN_KEEPALIVE const char *wotm_web_net_stall_text()
    {
        static std::string copy;
        std::lock_guard<std::mutex> lk(g_stallMu);
        copy = g_stallText;
        return copy.c_str();
    }
}

void wotmWebMount()
{
    // WOTM_DISC_SIZES (the page sets it where a worker's fetch stalls while the main thread waits: Firefox): range reads that
    // never block the main thread (wotm_webdisc.cpp). Otherwise the WASMFS fetch backend
    uint64_t sizes[2] = {};
    if (wotm::discchunks::parseSizes(std::getenv("WOTM_DISC_SIZES"), kDiscFiles, 2, sizes))
    {
        if (!wotmWebMountRangeDisc("/disc", kDiscFiles, sizes, 2))
            std::fprintf(stderr, "[web] couldn't mount /disc (range reads)\n");
        else
            std::fprintf(stderr, "[web] /disc: range reads\n");
    }
    else
    {
        const backend_t disc = wasmfs_create_fetch_backend("data", kDiscChunkBytes);
        if (wasmfs_create_directory("/disc", 0555, disc) != 0)
            std::fprintf(stderr, "[web] couldn't mount /disc\n");
        for (const char *name : kDiscFiles)
        {
            char path[64];
            std::snprintf(path, sizeof path, "/disc/%s", name);
            if (wasmfs_create_file(path, 0444, disc) < 0)
                std::fprintf(stderr, "[web] couldn't create %s\n", path);
        }
    }
    if (wasmfs_create_directory("/save", 0777, wasmfs_create_opfs_backend()) != 0)
        std::fprintf(stderr, "[web] couldn't mount /save (saves won't persist)\n");
    if (chdir("/save") != 0)
        std::fprintf(stderr, "[web] couldn't enter /save\n");
}
#endif
