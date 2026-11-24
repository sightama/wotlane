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
| A | **VU0 programs, round toward zero two lanes at a time** (`wotm_rtz_lanes.hpp`, #173, live) | VU0 per call, page bench: Firefox 1140 -> 912 ns (-20%), Chrome 385 -> 366 ns (-5%); bit-exact on 840 recorded calls, native and wasm | Firefox ~5% of a 1 vs 1 frame, ~8% with 8 monsters; Chrome ~1% (from the shares above) | built | none seen: same bits on the 840 calls and in the unit tests; desync matrix 9/9 with a Firefox player |
| B | **VU0 programs, flags only where read:** the generated code classifies every operation's result and assembles its MAC and status flags (`fin4`, `classify`), with no pass that drops the MAC flags nothing reads before the next overwrite (`grep -c` on the generated `.inc`: 537 flag commits for 547 operations) | page bench (the 840 calls), the SIMD build against itself with the flag bookkeeping removed (results kept, flags wrong, timing only), alternating in one run: Chrome 365 to 378 -> 289 to 296 ns per call (-22%, three rounds), Firefox 1174 -> 994 ns (-15%, one round; the same build measured 912 ns in item A's quieter run, so only the ratio counts) | at most ~5% of a Chrome frame (22% of VU0's 22.7%) and ~4% of a Firefox 1 vs 1; less in practice: the status register's sticky bits OR every operation's class, so only the MAC assembly of operations whose flags nothing reads can go, and how many those are is not counted yet | medium (the generator, then the bench) | none if the bench stays bit-exact (registers, memory, the flags that are read) |
| C | **Firefox lazy wasm tiering** (`javascript.options.wasm_lazy_tiering=false`) | profiles: 19% less Firefox game thread with eager tiering; synctest played frame 6.7 to 8.1 ms default, 5.4 to 7.4 eager (`firefox-cost.md`) | Firefox ~19% | a tip on the page, or a Mozilla bug | none; a tip is Lane's decision |
| D | **Main thread renderer on the GPU:** per-vertex lighting, skinning and the dust texture as shaders instead of CPU loops | ~25 to 30% of the main thread's work (above) | main thread time; it matters where the main thread limits presenting (slow devices; not measured on one yet) | medium to large | none: drawing only, never read by the game |
| E | **Coordinate system transforms recomputed every frame** (`hierTraceCsList`) for every object, moving or not | 18 to 22% (1 vs 1), 39% (8 monsters, Firefox) | a share of it, depending on how many are static (not counted yet) | medium; game code | high: skipping the function in a redo spreads to 76 blocks, so the game reads the transforms; a cache must give the same bits as today |
| F | **Disc chunks fetched and hashed in JavaScript on the game thread the first time:** a chunk the page's helper cache doesn't have is fetched by the game worker's synchronous reader (`syncChunkReader` in `web/disc_chunks.js`) and checked with a JS SHA-256; `RangeFile` (`native/wotm_webdisc.cpp`) then keeps it in memory, so each chunk pays once per page load | ~2.8 s of game thread in the first 6 s of the 8-monster direct fight (Firefox profile: `block`, `rotr` under `readLsn`, under the sound engine's reads; that these are first reads is inferred from `RangeFile` keeping every chunk); an online match's lobby prefetch (#149) puts the fight's chunks in the Cache API store the helper reads (`cachingFetch`, `cacheName` in `web/disc_cache.js`) first | a faster first fight start after a cold cache (offline, or a lobby started at once); small online | small to medium: a faster hash (SHA-256 in wasm) or the helper checking with SubtleCrypto | none (page only) |
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
  local page server, two 2953-sector (6 MB) music streams (LSN 335038 and 337999) took 333 ms and 282 ms, a 543-sector one 59 ms. The first read of each chunk pays item F.

## Memory

The page reserves 512 MB of wasm memory at start (`INITIAL_MEMORY` in the runtime patch, growing to 2 GB): the guest's 64 MB, the
recompiled code's data, the renderer's buffers and the disc cache's working set. Not measured further here; a phone that can't reserve
512 MB fails at start, which is the case to look at if one is reported.

## Order of work (coordinator, 2026-11-19)

A and C are live (#173; the Firefox tip, #175). B, J, E and I were measured next and none is worth building or shipping (the last
section). D waits for a measurement that some players are main-thread bound (phones); F waits (the first-read hash fixes in its
row are not scheduled); the native renderer's shadows (H, the main-thread part) can become a quality setting. VU0 rounding to
nearest (the last section) is built (Lane's go, 2026-11-23; results in its own section below). G and K are not scheduled.

## What the next items gave (2026-11-19, later)

Measured on the 840 recorded VU0 calls in page benches (Chrome and Firefox, three alternating rounds each) and with Chrome traces (20 s
each); none of these shipped. Frame shares are derived: VU0's share of the game thread (the table above) times the per-call change.

- **B, VU0 MAC flags only where read:** a generator pass dropped the MAC of 460 of 547 flag-producing operations, bit-exact (`vu0bench`
  native and wasm). Gain within noise: native 227 to 230 ns per call against 229 to 231, Chrome -1.5 to -5.5%, Firefox +0 to +2%. The
  costly part of the flags (classifying each result, which the results need anyway, the product's sticky check, the status, the commit)
  can't go without changing the VU state. B's earlier -22% (the B row) was an upper bound with all flag bookkeeping removed and the flags wrong. Not shipped.
- **J, the VU0 call's register copies:** copying only the VF registers each program touches (a mask from the generator; `vu0bench`
  checked it with the other registers poisoned, and a mask missing one register gave 275 mismatches) moved the wrapper's own share of
  the Chrome game thread from 2.6% to 2.5%. The copies are not where the wrapper's cost is. Not shipped.
- **E, the world transform cache:** inside `hierTraceCsList`, VU0 programs and their wrapper are ~62% of the time, the assembly walk
  (`hierTraverseAsm`) 15%, `hierCsUpdateAsm` 3%, VU1 packets 4% (`chrome_60hz_1m.json`, self time under `hierTraceCsList`). Every object's walk starts from its position plus the
  world eye offset and the view frustum normals (`hierCsUpdate` in `src/common/hier.cpp`), which change whenever the camera moves (and
  the object's own matrix whenever it moves); by that reasoning, not a measurement, a cache that gives the same bits would only hit
  with a still camera and a still object. Not built.
- **I, off-screen civilians:** `PedGroup` (the civilians) is 1.5% (Firefox) to 2.1% (Chrome) of the game thread in total (the 1 vs 1
  profiles `tier_default.json` and `chrome_60hz_1m.json`). `VehicleNavigator` is the steering helper of the AI-driven tank and missile
  truck pickups (`include/game/vehicle_navigator.h`) and `EnemyInfo` the monsters' pair table: neither is scenery. With a margin around
  every view only part of the civilians could be simplified at a time; an estimate of half gives about 1%. Not built.
- **VU0 rounding to nearest instead of the PS2's round toward zero**, on every build so every PC still agrees (a timing variant of
  the browser's VU0 code with plain IEEE arithmetic, in the page bench, three alternating rounds; its results were not checked, they
  are by design not the PS2's): Chrome 361 -> 228 ns per call (-37%), Firefox 887 -> 520 ns (-41%); with VU0 at 23 to 41% of the game thread, about
  8% of a Chrome frame, 10% of a Firefox 1 vs 1 and up to 17% with 8 monsters. It makes the game's VU0 math differ from the PS2's in
  the last bit (transforms, physics, collision) and old input recordings replay differently; the check becomes native equal to wasm
  instead of equal to the PS2 interpreter (`vu0bench` would compare native with wasm; the VU0 part of the PS2-reference checks is
  lost), the unit-test digests are measured again, and the desync matrix and a gameplay check go with it. Lane approved it on 2026-11-23 (below).

## VU0 round to nearest (2026-11-23)

Built on Lane's go: the VU0 microprograms round to nearest on every build (`wotmVu0NativeRun` sets no rounding mode; the browser
uses the same SSE code as the native builds). The software round toward zero (`native/wotm_rtz.hpp`, `native/wotm_rtz_lanes.hpp`)
and its four `rtz_*` unit tests are removed (Lane's call: nothing uses them; git history keeps them).

- **Same bits everywhere:** `sh tools/vu0bench.sh` now replays the recorded calls through the native and the wasm build and fails
  when they differ. On 840 recorded calls (M1 Max): 0 differ; 755 differ from the PS2 interpreter (686 on another recording), which
  is the rounding. On that second recording the round-toward-zero build checked against the new results also gives 686 (it equals
  the interpreter), so the check sees a change. `native/tests/wotm_vu0_test.cpp` (unit tests, run by CI on Linux, Windows and wasm) covers what recordings
  rarely reach: a madd whose float product or sum rounds past the largest float saturates instead of leaving an infinity.
- **Per VU0 call** (`vu0bench --time 50 --ref origin/wip/browser`, 3 alternating rounds): native 186/235/217 -> 180/148/205 ns
  (noise), wasm under node 516/505/515 -> 393/330/337 ns (-24 to -35%).
- **Game thread in real browsers** (`latency.mjs --players 2 --rtt 40 --secs 90 --browser chrome --firefox 1`, muted, median of
  the diag windows' `ee_busy_ms`, first two windows left out, two alternating rounds against the live build `wl-49d9dd9d3239c091`):
  Chrome 158 7.10 / 6.70 -> 6.20 / 6.00 ms per frame (-10 to -13%); Firefox 157 11.25 / 10.40 -> 9.70 / 9.15 ms (-12 to -14%).
  The page bench's estimate was ~8% (Chrome) and ~10% (Firefox).
- **Determinism:** desync matrix (`desync.mjs --cases 2,3,4 --scenarios baseline,pause,stall --firefox 1`) 9 trials, 0 desync
  (one 4-player pause trial OVERLOAD at host load 30 to 75, its 4 frames compared equal). Native synctest 2 frames back: 1 vs 1
  8 400 frames and 8 monsters 3 000 frames, 0 mismatches.
- **Gameplay:** a native AI vs AI fight plays to a round win; Chrome draws the fight normally (`tools/web_fps.mjs`, 116 fps
  presented at 120 Hz).
- **What changes for players:** VU0 results differ from the PS2's in the last bit (transforms, physics, collision); an input
  recording replays the same fight only on a build with the same VU0 code (`docs/ONLINE.md`). Native players need a build with
  this change to share a lobby with browsers (the build id changes).
