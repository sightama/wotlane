// ABOUTME: Browser build only: mounts the streamed disc (/disc, read lazily from data/ with HTTP range requests) and the persistent
// ABOUTME: save folder (/save, OPFS), and paces the render loop on requestAnimationFrame (JSPI suspends the loop between frames).
#ifdef __EMSCRIPTEN__
#include "wotm_web.h"

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <cstdio>
#include <unistd.h>

namespace
{
    constexpr unsigned kDiscChunkBytes = 1u << 20;   // one range request per MiB the game touches; each chunk is fetched once
    constexpr const char *kDiscFiles[] = {"SCUS_971.97", "game.iso"};
}

// globalThis.wotmFrames: frames presented, time spent in the loop's own work and time waiting for the browser (the page shows the rate)
EM_ASYNC_JS(void, wotm_web_wait_frame, (), {
    const s = globalThis.wotmFrames ??= {frames: 0, workMs: 0, waitMs: 0, last: performance.now()};
    const t0 = performance.now();
    s.workMs += t0 - s.last;
    await new Promise(resolve => requestAnimationFrame(resolve));
    s.last = performance.now();
    s.waitMs += s.last - t0;
    s.frames++;
});

void wotmWebPresent() { wotm_web_wait_frame(); }

void wotmWebMount()
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
    if (wasmfs_create_directory("/save", 0777, wasmfs_create_opfs_backend()) != 0)
        std::fprintf(stderr, "[web] couldn't mount /save (saves won't persist)\n");
    if (chdir("/save") != 0)
        std::fprintf(stderr, "[web] couldn't enter /save\n");
}
#endif
