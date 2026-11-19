# Performance audit of the browser game, 2026-11-19

Lane's question: we own this fork, so where does the time go and what can we change, the game's own design included? This ranks
every cost found by what it would give back, with the measured share in real browsers, the effort and the risk to determinism (a
change to what the game computes is safe online only when every PC runs the same build, which the build id enforces, and the desync
matrix stays at 0).

## How it was measured

- **Game thread (EE), per subsystem:** Firefox 160 with the Gecko profiler (`tools/browser_mp_test/profile_firefox.mjs`,
  `tools/gecko_profile_top.py`) and Chrome 157 with V8's CPU profiler in a trace (`tools/browser_mp_test/profile_chrome.mjs`,
  `tools/chrome_trace_top.py`), both on a page built with wasm names, an offline AI vs AI direct fight (`WOTM_MATCH=3,1,1`, 60 Hz; and
  8 monsters at 30 Hz in Firefox), muted. Shares are of the samples where the game thread works: waits (the vblank clock's futex,
  Firefox's wake-ups from it) are left out, and so are the first 10 s (stage load). A subsystem's share counts every sample with that
  function anywhere on the stack, so rows overlap (the hierarchy walk calls the VU0 programs).
- **VU0 programs alone:** `sh tools/vu0bench.sh --wasm --time 3000 --ref <commit>` replays 840 recorded VU0 calls through two
  versions of the code; the same bench built for a page (scratch harness, samples preloaded) gives Chrome and Firefox their own numbers.
- **Live, Windows:** `match_diag.sh` per player (`ee_busy_ms`): Firefox 19 to 25 ms of game thread per 1 vs 1 frame, Chrome 15 to 17,
  Brave 12.5 to 13 (a 60 Hz frame is 16.7). The Mac numbers below are 2 to 3 times lower; the shares are what carries over.
- M1 Max shared with other workers (load 7 to 12), so absolute times are noisy; every A/B below alternates its two versions.

## Where a frame goes

Game thread, share of its work:

| subsystem | Chrome 1 vs 1 | Firefox 1 vs 1 | Firefox 8 monsters |
|---|---|---|---|
| hierarchy walk (`hier`): transforms, culling, VU1 packets, shadows | 35.6% | 40.0% | 63.1% |
| of which the coordinate systems' world transforms (`hierTraceCsList`) | 17.8% | 20.6% | 39.1% |
| of which the stage's objects (`hierTraceWorld`) | 8.1% | 9.7% | 12.9% |
| of which shadows (`hierShadow`) | 3.1% | 3.1% | 10.0% |
| of which VU1 packets and object culling (`pktAddVu1ObjAsm`, `psObjectCull`) | 3.5% | 3.7% | 3.9% |
| VU0 microprograms (called from the walk and the game) | 22.7% | 24.7% | 41.1% |
| game update (`TheGame::Update`) | 31.0% | 32.5% | 24.4% |
| of which monsters | 16.8% | 16.9% | 17.6% |
| of which collision (`hd*`) | 11.0% | 11.2% | 12.0% |
| of which civilians and vehicles (`PedGroup`, `VehicleNavigator`, `EnemyInfo`) | 4.5% | 4.7% | 2.0% |
| animation | 7.4% | 7.9% | 7.7% |
| particles | 4.3% | 5.1% | 1.2% |
| sound mixing (`snd::Engine::render`) | 4.1% | 2.8% | 1.1% |
| native drawing capture (`wotmCaptureScene`) | 3.9% | 3.1% | 1.3% |
| runtime: function lookup, scheduler checks, IO register writes | 3.1% | 3.4% | 3.7% |

Page's main thread (Chrome 1 vs 1, busy 23% of the time): the native renderer's CPU work, by self time: `Scene::drawList` 29%,
per-vertex lighting `Scene::vertexRgb` 14%, the dust texture rasterized on the CPU `Scene::dustTexture` 11%, shadow silhouettes
`Scene::shadowTexture` 6%, CPU skinning `Scene::skinVertexPM` 3%. Firefox: the same functions in the same order.

## Ranked

| rank | what | measured | expected gain | effort | determinism risk |
|---|---|---|---|---|---|
| 1 | **VU0 programs, round toward zero two lanes at a time** (`wotm_rtz_lanes.hpp`) | VU0 per call, page bench: Firefox 1140 -> 912 ns (-20%), Chrome 385 -> 366 ns (-4%); bit-exact on 840 recorded calls (native and wasm) | Firefox ~5% of a 1 vs 1 frame, ~8% with 8 monsters; Chrome ~1% | done (PR) | none: same bits (bench, tests) |
| 2 | **VU0 programs, flags only where read:** the generated code classifies every operation's result and assembles its MAC and status flags (`fin4`, `classify`; 537 flag commits for 547 operations), with no pass that drops flags nothing reads before the next overwrite; `tools/vu0native.py` can add one | VU0 is 23 to 41% of the game thread; the flags' share of it is not measured yet (next: the bench timed with the flag work removed) | unknown until measured; every 10% of VU0 is 2 to 4% of a frame | medium (the generator, then the bench) | none if the bench stays bit-exact (registers, memory, live flags) |
| 3 | **Firefox lazy wasm tiering** (`javascript.options.wasm_lazy_tiering=false`) | Firefox played frame 8.3 -> 6.6 ms, 19% less game thread; the first minute ~10 ms | Firefox ~19% | a page tip or a Mozilla bug | none (Lane's call: no tip without his OK) |
| 4 | **Main thread renderer on the GPU:** per-vertex lighting, skinning and the dust texture as shaders instead of CPU loops | ~30% of the main thread's work (Chrome) | the main thread's time on slow devices (presenting at 60 needs it under 16.7 ms); the game thread is untouched | medium to large | none: drawing only, never read by the game |
| 5 | **Coordinate system transforms recomputed every frame** (`hierTraceCsList`) for every object, moving or not | 18 to 21% (1 vs 1), 39% (8 monsters, Firefox) | a share of it, depending on how many are static (unmeasured: count them first) | medium; game code | high: the game reads the transforms (collision, cameras); every PC must agree, and the redo-skip test spreads to 76 blocks |
| 6 | **Disc chunks re-hashed in JavaScript on every Firefox read:** the game worker's synchronous reads (`syncChunkReader` in `web/disc_chunks.js`) hash each 1 MiB chunk with a JS SHA-256 every time, verified or not | ~2.8 s of game thread in the first 6 s of an 8-monster fight (Firefox profile: `block`, `rotr` under `readLsn`) | a faster fight start in Firefox and no hitch when a stream starts mid-fight; Chrome uses SubtleCrypto off the game thread | small: remember verified chunks (bytes or their hash) per URL | none (page only) |
| 7 | **VU1 packets and culling the native renderer does not read** (`pktAddVu1ObjAsm`, `psObjectCull`) | 3.5 to 3.9% | ~3.5% | small to skip, medium to prove | medium: the buffers are in the state hash, and nothing may read them (item G3 of `723b110:docs/investigacoes/2026-10-23/performance_vistas.md`) |
| 8 | **Shadows computed by the game** (`hierShadow`) next to the native renderer's own silhouettes | 3% (1 vs 1), 10% (8 monsters) | up to 10% with 8 monsters | medium | high: with 8 monsters the shadow pass writes state the game reads (redo-skip test) |
| 9 | **Civilians and vehicles updated off screen** (`PedGroup`, `VehicleNavigator`, `EnemyInfo`) | 4.5% (1 vs 1), 2% (8 monsters) | a few % | medium; game design | medium: changes what the game does (all PCs alike, but the city behaves differently) |
| 10 | **Runtime seams:** `lookupFunction` (hash map per indirect call), `checkpointDue` out of line, `writeIORegister` allocating a vector under a lock on stores from the walk, `executeVU0Microprogram` copying ~1 KB of state each way | 0.6 to 2%, 1.3%, 1.2 to 1.4%, ~1% | ~4% together | small each (PS2Recomp patch) | low |
| 11 | **Sound mixing on the game thread** | 1 to 4% | that, if the mix moved to the audio thread | medium | medium: the game polls the engine (`isPlaying`, `getTick`), so the state must stay on the game thread |

Not on the list: animation, monsters' logic and collision are the game itself; nothing measured points at waste there yet.

## Loading and streaming

- **wasm:** 41 MB, downloaded again after every deploy (17 s at 20 Mbps before brotli). Brotli copies ship with the next deploy (the
  browser worker measures the before and after).
- **Fight data:** ~22 MiB per match, prefetched while the lobby waits (#149: start 16.5 -> 3.35 s on a 20 Mbps line).
- **Streams:** `Engine::playStream` reads a whole stream file on the game thread before it plays (music up to ~6 MB: 333 ms the first
  time in Firefox, from the disc chunks). With the chunks cached the read is fast in Chrome; in Firefox each read pays item 6.

## Memory

The page reserves 512 MB of wasm memory at start (`INITIAL_MEMORY`, grows to 2 GB): the guest's 64 MB, the recompiled code's data, the
renderer's buffers and the disc cache's working set. Not measured further here; a phone that can't reserve 512 MB fails at start, which
is the case to look at if one is reported.

## What is being done

Item 1 is built and measured (PR from `sightama/vu0-simd-rtz`); item 2 is next, then 6 (small, page only), then 4. Items 5, 8 and 9
change what the game does and need Lane's go before work starts; item 3 is Lane's call.
