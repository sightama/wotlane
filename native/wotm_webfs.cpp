// ABOUTME: Browser build only: mounts the streamed disc (/disc, read lazily from data/) and the save folder (/save, OPFS or memory), paces the
// ABOUTME: render loop on requestAnimationFrame (JSPI suspends it between frames), and shares the tab and network state with the page.
#ifdef __EMSCRIPTEN__
#include "wotm_web.h"
#include "wotm_discchunks.hpp"
#include "wotm_latency.hpp"
#include "wotm_framephase.hpp"

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
    // the match's counters for the page's diagnostics (wotmWebNetStats): index = web/diag.js NET_STAT
    std::atomic<double> g_netStats[13];
    std::atomic<double> g_inputAt{0};   // the earliest button or key down not taken yet (wotm_web_input_at, 0: none)
    wotm::latency::Window g_latency;    // the window the page took last (wotm_web_latency_take); main thread only
    wotm::framephase::Display g_display;   // main thread only (the present)
    std::atomic<double> g_rafLastMs{0}, g_rafPeriodMs{0};   // g_display's, for any thread
    std::mutex g_stallMu;
    std::string g_stallText;
}

// globalThis.wotmFrames: frames presented, time spent in the loop's own work and time waiting for the browser (the page shows the rate).
// Returns the animation frame's own time on emscripten_get_now's clock (performance.timeOrigin + performance.now(), as boot.js gives key times)
EM_ASYNC_JS(double, wotm_web_wait_frame, (), {
    const s = globalThis.wotmFrames = globalThis.wotmFrames || {frames: 0, workMs: 0, waitMs: 0, last: performance.now()};
    const t0 = performance.now();
    s.workMs += t0 - s.last;
    const at = await new Promise(resolve => requestAnimationFrame(resolve));
    s.last = performance.now();
    s.waitMs += s.last - t0;
    s.frames++;
    return performance.timeOrigin + at;
});

void wotmWebPresent()
{
    static unsigned frames = 0;
    if (++frames % 60 == 0)
        wotmWebSaveSync();   // every 60 frames, what the game saved goes to the browser's storage (WOTM_SAVE_MEMORY)
    const double rafMs = wotm_web_wait_frame();
    wotm::latency::tracker().presented(wotm::latency::nowMs());   // the frame drawn before this animation frame is on its way out
    g_display.raf(rafMs);
    g_rafPeriodMs.store(g_display.periodMs(), std::memory_order_relaxed);
    g_rafLastMs.store(rafMs, std::memory_order_relaxed);
}

void wotmWebDisplayPhase(double &lastRafMs, double &periodMs)
{
    lastRafMs = g_rafLastMs.load(std::memory_order_relaxed);
    periodMs = g_rafPeriodMs.load(std::memory_order_relaxed);
}

double wotmWebTakeInputAt() { return g_inputAt.exchange(0); }

bool wotmWebHidden() { return g_hidden.load(); }

void wotmWebQuit()
{
    MAIN_THREAD_EM_ASM({ if (globalThis.wotmQuit) globalThis.wotmQuit(); });
}

void wotmWebPageSize(int &w, int &h)
{
    w = MAIN_THREAD_EM_ASM_INT({ return wotmDevicePixels(window.innerWidth, window.innerHeight, window.devicePixelRatio).w; });
    h = MAIN_THREAD_EM_ASM_INT({ return wotmDevicePixels(window.innerWidth, window.innerHeight, window.devicePixelRatio).h; });
}

double wotmWebPixelRatio()
{
    return MAIN_THREAD_EM_ASM_DOUBLE({ return window.devicePixelRatio > 0 ? window.devicePixelRatio : 1; });
}

void wotmWebNetStall(double ms, const char *text)
{
    std::lock_guard<std::mutex> lk(g_stallMu);
    g_stallText = text;
    g_stallMs = ms;
}

void wotmWebNetStats(uint32_t frame, uint32_t waits, double waitMs, uint32_t delay, uint32_t pingMs, double idleMs, const uint32_t periods[4],
                     double vblankLateMs, double vblankWakes, double vblankSlowWakes)
{
    g_netStats[0] = frame;
    g_netStats[1] = waits;
    g_netStats[2] = waitMs;
    g_netStats[3] = delay;
    g_netStats[4] = pingMs;
    g_netStats[5] = idleMs;
    for (int k = 0; k < 4; ++k)
        g_netStats[6 + k] = periods[k];
    g_netStats[10] = vblankLateMs;
    g_netStats[11] = vblankWakes;
    g_netStats[12] = vblankSlowWakes;
}

// called by the page (boot.js)
extern "C"
{
    // the match's counters, as wotmWebNetStats set them: 0 frame, 1 waits, 2 wait ms, 3 delay, 4 ping, 5 game thread idle ms, 6 to 9
    // frames by length, 10 vblank wake lateness ms, 11 vblank wakes, 12 wakes over half a vblank late (anything else: 0)
    EMSCRIPTEN_KEEPALIVE double wotm_web_net_stat(int which) { return which >= 0 && which < 13 ? g_netStats[which].load() : 0; }
    // a button or key went down at `ms` (performance.timeOrigin + the event's time, the clock of emscripten_get_now): kept unless an
    // earlier one is still waiting for the game's next sample (wotmWebTakeInputAt)
    EMSCRIPTEN_KEEPALIVE void wotm_web_input_at(double ms)
    {
        double cur = g_inputAt.load();
        while (ms > 0 && (cur == 0 || cur > ms) && !g_inputAt.compare_exchange_weak(cur, ms)) {}
    }
    // the input latency since the last take (wotm_latency.hpp), read with wotm_web_latency: 0 presses, 1 total median, 2 total p95,
    // 3 total max, 4 sample, 5 delay, 6 stall, 7 sim, 8 present (medians, ms), 9 presses with no page time, 10 blend (median, ms; web/diag.js
    // LATENCY; anything else: 0)
    EMSCRIPTEN_KEEPALIVE void wotm_web_latency_take() { g_latency = wotm::latency::tracker().take(); }
    EMSCRIPTEN_KEEPALIVE double wotm_web_latency(int which)
    {
        const wotm::latency::Window &w = g_latency;
        const double v[] = {double(w.n), w.total, w.p95, w.max, w.sample, w.delay, w.stall, w.sim, w.present, double(w.unknownPress), w.blend};
        return which >= 0 && which < 11 ? v[which] : 0;
    }
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
    // WOTM_SAVE_MEMORY=1 (the page sets it where a worker's OPFS calls stall while the main thread waits): the folder in memory,
    // written back to OPFS as it changes (wotm_websave.cpp). Otherwise the WASMFS OPFS backend
    const char *saveMemory = std::getenv("WOTM_SAVE_MEMORY");
    if (saveMemory && saveMemory[0] == '1')
    {
        if (!wotmWebMountSaveMemory("/save"))
            std::fprintf(stderr, "[web] couldn't mount /save (saves won't persist)\n");
    }
    else if (wasmfs_create_directory("/save", 0777, wasmfs_create_opfs_backend()) != 0)
        std::fprintf(stderr, "[web] couldn't mount /save (saves won't persist)\n");
    if (chdir("/save") != 0)
        std::fprintf(stderr, "[web] couldn't enter /save\n");
}
#endif
