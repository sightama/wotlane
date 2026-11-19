# Performance audit of the browser game, 2026-11-19

Lane's question: we own this fork, so where does the time go and what can we change, the game's own design included? This lists every
cost found with its measured share in real browsers, the expected gain, the effort and the risk to determinism (a change to what the
game computes is safe online only when every PC runs the same build, which the build id enforces, and the desync matrix stays at 0).
The order of work is at the end; it is not the order of the table.

## How it was measured

- **Game thread (EE), per subsystem:** Firefox 160 with the Gecko profiler (`tools/browser_mp_test/profile_firefox.mjs`,
  `tools/gecko_profile_top.py`) and Chrome 157 with V8's CPU profiler in a trace (`tools/browser_mp_test/profile_chrome.mjs`,
  `tools/chrome_trace_top.py`), both on a page built with wasm names, an offline AI vs AI direct fight, muted. Profiles (local, not in
  the repo for their size): Chrome 1 vs 1 at 60 Hz, 15 s recorded after 30 s of fight (`chrome_60hz_1m.json`); Firefox 1 vs 1 at 60
  Hz with default settings, the last 60 s of a 120 s fight (`tier_default.json`); Firefox 8 monsters at 30 Hz, the fight after its
  first 10 s (`30hz_8m.json`, 45 s). Shares are of the samples where the game thread works: waits (the vblank clock's futex in Chrome,
  Firefox's wake-ups from it, counted as CPU) are left out, and so is the stage load.
- **How rows count:** a row counts every sample with a matching function anywhere on the stack. So rows overlap and don't add up: a VU0
  call made inside the hierarchy walk counts in both, and the indented rows are found by name anywhere (collision is called from the
  walk and from the update), not only under the row above them.
- These shares differ from the earlier table in `../2026-11-18/rollback-synctest/firefox-cost.md`, which kept Firefox's wake-ups as
  busy time and, with 8 monsters, the fight start in its window (the sound engine's disc reads alone were 22% of its samples); that
  table is what the redo-skip work used, this one is what to plan from.
- **VU0 programs alone:** `sh tools/vu0bench.sh --wasm --time 3000 --ref <commit>` replays 840 recorded VU0 calls through two
  versions of the code; the same bench built for a page (scratch harness, samples preloaded, three alternating rounds) gives Chrome's
  and Firefox's own numbers.
- **Live, Windows:** `match_diag.sh` per player (`ee_busy_ms`, the coordinator's summary of the last matches): Firefox 19 to 25 ms of
  game thread per 1 vs 1 frame, Chrome 15 to 17, Brave 12.5 to 13 (a 60 Hz frame is 16.7). The Mac's are 2 to 3 times lower; the
  shares are what carries over.
- M1 Max shared with other workers (load 7 to 12), so absolute times are noisy; every A/B below alternates its two versions.

## Where a frame goes

Game thread, share of its work:

| subsystem | Chrome 1 vs 1 | Firefox 1 vs 1 | Firefox 8 monsters |
|---|---|---|---|
| hierarchy walk (`hier`): transforms, culling, VU1 packets, shadows | 35.6% | 39.3% | 63.1% |
| &nbsp;&nbsp;coordinate systems' world transforms (`hierTraceCsList`) | 17.8% | 22.1% | 39.1% |
| &nbsp;&nbsp;the stage's objects (`hierTraceWorld`) | 8.1% | 8.9% | 12.9% |
| &nbsp;&nbsp;shadows (`hierShadow`) | 3.1% | 2.6% | 10.0% |
| &nbsp;&nbsp;VU1 packets and object culling (`pktAddVu1ObjAsm`, `psObjectCull`) | 3.5% | 3.8% | 3.9% |
| VU0 microprograms (called from the walk and the game) | 22.7% | 24.0% | 41.1% |
| game update (`TheGame::Update`) | 31.0% | 34.2% | 24.4% |
| &nbsp;&nbsp;monsters (functions of `Monster`) | 16.8% | 18.2% | 17.6% |
| &nbsp;&nbsp;collision (`hd*`) | 11.0% | 12.0% | 12.0% |
| &nbsp;&nbsp;civilians and vehicles (`PedGroup`, `VehicleNavigator`, `EnemyInfo`) | 4.5% | 3.8% | 2.0% |
| animation | 7.4% | 7.5% | 7.7% |
| particles | 4.3% | 4.6% | 1.2% |
| sound engine (`snd::`: the mix and the game's calls into it) | 4.1% | 3.4% | 1.1% |
| native drawing capture (`wotmCaptureScene`) | 3.9% | 3.0% | 1.3% |
| runtime: function lookup, scheduler checks, IO register writes | 3.1% | 4.2% | 3.7% |

The page's main thread, share of its work by self time (Chrome 1 vs 1, busy 23% of the time; Firefox 1 vs 1 after its wake-ups):
`Scene::drawList` 29% and 24%, per-vertex lighting `Scene::vertexRgb` 14% and 8%, the dust texture rasterized on the CPU
`Scene::dustTexture` 11% and 13%, shadow silhouettes `Scene::shadowTexture` 6% and 5%, CPU skinning `Scene::skinVertexPM` 3% (Chrome).

## The list

| item | what | measured | expected gain | effort | determinism risk |
|---|---|---|---|---|---|
| A | **VU0 programs, round toward zero two lanes at a time** (`wotm_rtz_lanes.hpp`) | VU0 per call, page bench: Firefox 1140 -> 912 ns (-20%), Chrome 385 -> 366 ns (-5%); bit-exact on 840 recorded calls, native and wasm | Firefox ~5% of a 1 vs 1 frame, ~8% with 8 monsters; Chrome ~1% (from the shares above) | built | none seen: same bits on the 840 calls and in the unit tests; the desync matrix with a Firefox player runs before merge |
| B | **VU0 programs, flags only where read:** the generated code classifies every operation's result and assembles its MAC and status flags (`fin4`, `classify`), with no pass that drops flags nothing reads before the next overwrite (`grep -c` on the generated `.inc`: 537 flag commits for 547 operations) | VU0 is 23 to 41% of the game thread; the flags' share of it is not measured yet (next: the bench timed with the flag work removed) | unknown until measured; every 10% of VU0 is 2 to 4% of a frame | medium (the generator, then the bench) | none if the bench stays bit-exact (registers, memory, the flags that are read) |
| C | **Firefox lazy wasm tiering** (`javascript.options.wasm_lazy_tiering=false`) | profiles: 19% less Firefox game thread with eager tiering; synctest played frame 6.7 to 8.1 ms default, 5.4 to 7.4 eager (`firefox-cost.md`) | Firefox ~19% | a tip on the page, or a Mozilla bug | none; a tip is Lane's decision |
| D | **Main thread renderer on the GPU:** per-vertex lighting, skinning and the dust texture as shaders instead of CPU loops | ~25 to 30% of the main thread's work (above) | main thread time; it matters where the main thread limits presenting (slow devices; not measured on one yet) | medium to large | none: drawing only, never read by the game |
| E | **Coordinate system transforms recomputed every frame** (`hierTraceCsList`) for every object, moving or not | 18 to 22% (1 vs 1), 39% (8 monsters, Firefox) | a share of it, depending on how many are static (not counted yet) | medium; game code | high: skipping the function in a redo spreads to 76 blocks, so the game reads the transforms; a cache must give the same bits as today |
| F | **Disc chunks fetched and hashed in JavaScript on the game thread the first time:** a chunk the page's helper cache doesn't have is fetched by the game worker's synchronous reader (`syncChunkReader` in `web/disc_chunks.js`) and checked with a JS SHA-256; `RangeFile` (`native/wotm_webdisc.cpp`) then keeps it in memory, so each chunk pays once per page load | ~2.8 s of game thread in the first 6 s of the 8-monster direct fight (Firefox profile: `block`, `rotr` under `readLsn`, the sound engine's first reads); an online match's lobby prefetch (#149) puts the fight's chunks in the helper's cache first | a faster first fight start after a cold cache (offline, or a lobby started at once); small online | small to medium: a faster hash (SHA-256 in wasm) or the helper checking with SubtleCrypto | none (page only) |
| G | **VU1 packets and culling the native renderer does not read** (`pktAddVu1ObjAsm`, `psObjectCull`) | 3.5 to 3.9% | ~3.5% | small to skip, medium to prove | medium: the buffers are in the state hash, and nothing may read them (item G3 of `723b110:docs/investigacoes/2026-10-23/performance_vistas.md`) |
| H | **Shadows computed by the game** (`hierShadow`) next to the native renderer's own silhouettes | 2.6 to 3.1% (1 vs 1), 10% (8 monsters) | ~3% where skipping it is safe (1 vs 1: the difference stays in 2 buffer blocks); with 8 monsters the pass writes state the game reads (21 blocks), so it can't be skipped per device | medium | high for the game's pass, unless it becomes a match setting (the host picks it, every PC runs it alike, in the match's config hash: an option, not planned); none for the native renderer's own silhouettes (main thread, `shadowTexture`), which a per-device quality setting can drop |
| I | **Civilians and vehicles updated off screen** (`PedGroup`, `VehicleNavigator`, `EnemyInfo`) | 3.8 to 4.5% (1 vs 1), 2% (8 monsters) | a few % | medium; game design | medium: changes what the game does (all PCs alike, but the city behaves differently) |
| J | **Runtime seams:** `lookupFunction` (hash map per indirect call), `checkpointDue` out of line, `writeIORegister` allocating a vector under a lock on stores from the walk, `executeVU0Microprogram` copying ~1 KB of state each way | together 3 to 4% (row above), each 1 to 2% | ~3% together | small each (PS2Recomp patch) | low |
| K | **Sound mixing on the game thread** | within the sound engine's 1 to 4% | that share, if the mix moved to the audio thread | medium | medium: the game polls the engine (`isPlaying`, `getTick`), so its state must stay on the game thread |

Not on the list: animation, monsters' logic and collision are the game itself; nothing measured points at waste there yet.

## Loading and streaming

- **wasm:** 41 MB, downloaded again after every deploy (17 s at 20 Mbps, `docs/superpowers/plans/2026-11-17-online-seamless.md`).
  Brotli copies are live since the deploy of 2026-11-19 23:24 UTC; the browser worker measures the cold start before and after.
- **Fight data:** ~22 MiB per match, prefetched while the lobby waits (#149: start 16.5 -> 3.35 s on a 20 Mbps line).
- **Streams:** `Engine::playStream` reads a whole stream file on the game thread before it plays: in a Firefox synctest log against a
  local page server, a 2953-sector (6 MB) music stream took 333 ms and 282 ms, a 543-sector one 59 ms. The first read of each chunk pays item F.

## Memory

The page reserves 512 MB of wasm memory at start (`INITIAL_MEMORY` in the runtime patch, growing to 2 GB): the guest's 64 MB, the
recompiled code's data, the renderer's buffers and the disc cache's working set. Not measured further here; a phone that can't reserve
512 MB fails at start, which is the case to look at if one is reported.

## Order of work (coordinator, 2026-11-19)

A (its PR, after the desync matrix), then D and B (bench first); F waits (keeping verified chunks in the reader turned out redundant with `RangeFile`, PR #172). Then E as a cache that gives the same bits as today (desync matrix 0
and equal state hashes against the current build), and the native renderer's shadows (H, the main-thread part) as a quality setting.
Then I (Lane: off-screen civilians and vehicles may be simplified if they switch back to full simulation before any player can see
them: a margin around every view, deterministic on every PC, checked by a recording of them entering and leaving view). C waits for Lane.
