# Online Experience End-to-End Audit (2026-11-23)

End-to-end measurement of `playwotm.com` (live build `wl-49d9dd9d3239c091`) and local builds in real Chrome (158) and Firefox (157) on macOS (M1 Max, muted). Every finding below is backed by captured diagnostics, server logs, or timed Playwright runs, and ranked by player impact.

## Measurement Summary

| Flow | Live prod (`playwotm.com`) | Local controlled (`wotm_server`) |
|---|---|---|
| Page load (`domcontentloaded` -> `#host`) | 1068 ms | - |
| Lobby create (`Host online` -> 4-char code) | 299-412 ms (3 runs: `TMCZ`, `L6PZ`, `J6GG`, `AVV4`) | - |
| Lobby join (`Join` -> 2 members visible) | 361-1050 ms | - |
| Cold-cache `START` -> first fight frame (`frame >= 1`) | Chrome 6.1-10.8 s; Firefox 6.4-14.6 s (fast fiber; ~17-30+ s on 20 Mbps) | - |
| Warm-cache `Rematch` -> first fight frame (`frame >= 1`) | Chrome 4.46 s; Firefox 4.85 s (`AVV4`) | - |
| Steady 2p 60 Hz latency (Chrome / Firefox) | `H24X` `seq=11` (RTT 38-49 ms, delay 4): Chrome 122.1 ms (p95 134.2), Firefox 114.1 ms (p95 117.4) | `DHEK` `seq=6` (RTT 40/60 ms, delay 5): Chrome 110.4 ms, Firefox 122.7 ms |
| 2p 60 Hz latency under jitter + spikes | `H24X` full match (real internet + host load): Chrome 239 ms (p95 1596), Firefox 166 ms (p95 492) | `K7S9` (RTT 40/60 ms, jitter 15 ms, 150 ms/400 ms spike every 12 s): Chrome 193.5 ms (p95 1111.6), Firefox 165.0 ms (p95 569.3) |
| Stall / away / pause / disconnect checks | `H24X` + `T97F`: 22/22 PASS (`match`, `round`, `leave`, `late_join`, `rematch`) | `local_cf_stall`: 8/8 PASS (away detected in 956 ms, stall prompt, recovery with 0 desync) |

Latency breakdown in steady 2p 60 Hz live prod (`H24X` `seq=11`, delay 4):
- Chrome (`P1-8428`): `122.1 ms = sample 18.0 + delay 59.4 + stall 0.0 + sim 8.8 + present 26.9 + blend 0.0`
- Firefox (`P2-8428`): `114.1 ms = sample 4.8 + delay 64.5 + stall 3.0 + sim 13.8 + present 21.4 + blend 0.0`

---

## Findings Ranked by Player Impact

### 1. Quick rematch breaks on staggered exit and locks the player out of their own name for 120 s
- **Impact:** High (broken core loop after matches). Any player who clicks Quit, clicks "Leave the match", or reloads even 100-500 ms before the last player's match socket closes fails quick rematch and cannot manually rejoin with their own name for 2 minutes.
- **Evidence:**
  - Live prod timing runs (`TMCZ`, `J6GG`, `L6PZ`, `AVV4`): even on simultaneous `Promise.all([p1.reload(), p2.reload()])` across Chrome + Firefox, quick rematch failed 2 of 4 times (50%) when Chrome's reload beat Firefox's socket close.
  - Root cause in `native/net/wotm_net_server.cpp:505-506` and `native/net/wotm_net_server.cpp:475-476`: `Server::rejoinLobby()` calls `admitError()`, which rejects with `ERROR lobby is in a match; wait for it to end` while `l.match` is still alive.
  - Because `web/rematch.js:18` (`takeRejoin`) deletes `wotlane.rejoin` from `sessionStorage` before sending `REJOIN`, the token is permanently lost on that rejection.
  - Because `rejoinLobby` returns before `l.ghosts.erase(g)` (`wotm_net_server.cpp:515`), the player's ghost stays in `l.ghosts` for `LINGER_MS = 120000` ms, so a manual `JOIN` with their own name fails with `ERROR name already taken in the lobby` (`wotm_net_server.cpp:457-459`).
- **Proposed fix:**
  1. In `Server::rejoinLobby()` (`native/net/wotm_net_server.cpp`): allow a valid ghost to rejoin its own lobby even while `l.match` is winding down (its seat in `l.match` is already gone or closing), or if a match is still active, keep the player in the lobby waiting state until `match_over` broadcasts `MEMBERS` (and block `START` while `l.match != nullptr`).
  2. In `web/rematch.js` / `web/front.js`: only clear `wotlane.rejoin` on terminal errors (`can't rejoin`, `the lobby is gone`) or after `REJOIN` succeeds; on transient `lobby is in a match`, retry `REJOIN` briefly (with a "Waiting for the match to finish..." status) rather than dropping to the Host/Join screen.
- **Expected gain:** Quick rematch reliability goes from ~50% (and 0% on staggered exits) to ~100%, and eliminates the 120 s self-lockout bug.
- **Ownership:** Ours (netcode + `rematch.js`/`front.js` logic, no UI styling changes).

### 2. Boot/load main-thread stalls poison server RTT and spike input delay to 8-15 frames at match start
- **Impact:** High (first 10-30 s of a match feel sluggish with 133-250 ms of pure input delay).
- **Evidence:**
  - Live prod match `UQAA` (`20261123-232125-UQAA-match.log`): during the first 3 s after `START`, Chrome hit a 415 ms main-thread long task while booting/loading. That delayed WebSocket PONG, causing `wotm_server` to read `rtt=979 ms` and push BOTH players' input delay from `5 -> 7 -> 8 -> 15` frames (250 ms) right as the fight began.
  - Live prod `H24X` (`20261123-235403-H24X-match.log`): started at `delay=4` from boot-time `rtt=58,42 ms` when steady RTT was `38-43 ms` (delay 3).
  - Root cause in `native/net/wotm_net_server.cpp:1154-1173` and `native/net/wotm_net_server.cpp:1478-1507`: `pace()` starts sampling `s.rtt.add(rttHighOf(s.peer))` immediately after `m->started = true` (even while clients are still at `frame=0` doing level load and shader compile), and `startMatch` starts the match clock before clients finish loading.
- **Proposed fix:**
  1. Do not feed `RttWindow` or send `Pace` delay raises while a player has not yet submitted active fight frames (e.g., `nextMissing(i) < kWarmupFrames`, ~30 frames / 0.5 s of live gameplay) or while `in_gap` indicates a boot load stall.
  2. Pair with Task 7b (start-clock barrier: send `Start` / start `pace()` once all clients report ready / frame 0 loaded, or hold `pace()` during load) so boot-time PONG delays never pollute `RttWindow`.
- **Expected gain:** Eliminates 8-15 frame (133-250 ms) false delay spikes at match start; cuts start-of-match delay by 1-11 frames (17-183 ms) on real connections.
- **Ownership:** Ours (server netcode `wotm_net_server.cpp`, `wotm_net_proto.hpp`).

### 3. `adviseDelay` hysteresis deadband traps matches +1 frame of input delay indefinitely
- **Impact:** High (permanent +16.7 ms latency tax at 60 Hz, +33.3 ms at 30 Hz, on normal 40-50 ms connections).
- **Evidence:**
  - Live prod `H24X` (`20261123-235403-H24X-match.log`): started at `delay=4` (`rtt=58,42 ms`), ran for 58 s with steady RTT 38-43 ms (which only needs `delay=3`), and NEVER dropped to `delay=3` until `23:55:06Z`, 1 second AFTER P2 left the match!
  - Exact math in `native/net/wotm_net_proto.hpp:466-471`:
    - `pathFrames([40, 40], 0, 16.667) = (40 + 3.0) / 16.667 = 2.58` -> `up = ceil(2.58) = 3`.
    - `down = ceil(pathFrames(fallMs) + 0.5) = ceil(2.58 + 0.5) = ceil(3.08) = 4`.
    - Because `+0.5` frames (8.3 ms) is stacked on top of `fallLevel` (the 94th percentile, 15th of 16 samples), two players must stay at or under `38.67 ms` for 15 of 16 samples just to drop from delay 4 to delay 3. Any match with 40-46 ms RTT that ever touches delay 4 stays trapped at delay 4 forever.
- **Proposed fix:**
  - In `native/net/wotm_net_proto.hpp`: since `fallLevel` (15th of 16 samples over 8 s) ALREADY provides strong temporal hysteresis against spikes, reduce the extra spatial margin on `down` from `0.5` frames to `0.15-0.20` frames (2.5-3.3 ms at 60 Hz) or use `riseLevel` with `0.25` frames hysteresis, and validate with unit tests + `latency.mjs` spike scenarios that delay drops promptly after spikes without flapping.
- **Expected gain:** Reclaims 1 full frame (16.7 ms at 60 Hz, 33.3 ms at 30 Hz) of input latency on typical 40-48 ms regional matches.
- **Ownership:** Ours (server netcode `wotm_net_proto.hpp`).

### 4. Missing time sync (frame-advantage pacing) causes 30-75 lockstep stalls every 5 s even at 55-60 fps
- **Impact:** High (visible micro-stutters, audio gaps, and +10-30 ms of `lat_st_ms` / `lat_pr_ms` jitter even in healthy matches).
- **Evidence:**
  - Live prod `H24X` steady windows (`seq=6, 7, 11`, 55-58 fps): Chrome (`ee_busy_ms=7.8-9.0 ms`) hit **56-73 waits per 5 s** (`wait_ms=662-830 ms`, 13-17% of wall time blocked); Firefox (`ee_busy_ms=12.9-14.0 ms`, `behind_avg=1.3`) hit **30-43 waits per 5 s** (`wait_ms=373-613 ms`). Full match: Chrome blocked 15.0 s of 82 s (18%), Firefox 11.8 s of 62 s (19%).
  - Local controlled `K7S9` steady windows (`seq=4, 6, 8`, 51-55 fps): **39-75 waits per 5 s** (`wait_ms=466-911 ms`), `behind_max=3-6` frames.
  - Root cause: Task 8 of `docs/superpowers/plans/2026-11-17-online-seamless.md` is not implemented yet. Without GGRS-style frame-advantage pacing, the faster peer runs full speed ahead until it hits `netWaitFrame()`, stalls for 15-30 ms, calls `shiftHostDeadlines()`, and repeats every few frames.
- **Proposed fix:**
  - Implement Task 8 (time sync): server includes each player's smoothed frame advantage (`behind` delta) in `Pace` or `Inputs`, and a leading client stretches its fixed-step frame period by up to ~5-10% (`min(0.10, advantage / 30)`) so both peers stay in lockstep smoothly without slamming into `netWaitFrame()`.
- **Expected gain:** Cuts steady-state lockstep waits from ~50-70 per 5 s down near 0, eliminating micro-hitches and smoothing `lat_st_ms` / `lat_pr_ms` spikes.
- **Ownership:** Ours (netcode + runtime pacing `wotm_sync.inc` / `wotm_net_server.cpp`).

### 5. Unphased presentation adds 21-27 ms of `lat_pr_ms` on 60 Hz displays (Task 5)
- **Impact:** Medium-High (accounts for ~20% of total steady-state input-to-screen latency).
- **Evidence:**
  - Live prod `H24X` steady window `seq=11` (60 Hz): `lat_pr_ms = 26.9 ms` (Chrome), `21.4 ms` (Firefox).
  - Local controlled `K7S9` steady window `seq=4` (120 Hz display): `lat_pr_ms = 10.9-11.4 ms`.
  - Root cause: the EE game thread's vblank clock runs at an arbitrary phase relative to the browser's `requestAnimationFrame` cadence, so a finished frame waits half a display period (plus compositor) on average before being drawn.
- **Proposed fix:**
  - Implement Task 5 (phase-locked present): pass the page's latest `requestAnimationFrame` timestamp into the runtime (`wotm_framepace.hpp` / vblank clock) and gently slew the game-frame deadline so frame capture lands 2-3 ms before the next rAF.
- **Expected gain:** -8 to -12 ms average `lat_pr_ms` on 60 Hz displays (bringing steady 60 Hz live latency from ~114-122 ms down toward ~102-110 ms, or ~85-95 ms combined with Finding #3).
- **Ownership:** Ours (runtime pacing + `web/` rAF timestamp bridge). Note: coordinate with deploy owner (`ctx_33885a8e6952`) regarding their "input-to-photon floor" work so we do not duplicate effort.

### 6. Players stare at a black screen for 4.5-14.6 s after `START`/`Rematch` with wrong status text (`"Choose how to play"`, then `"Running"`)
- **Impact:** Medium-High (every single match start and rematch feels frozen or broken, especially for first-time players).
- **Evidence:**
  - Live prod `AVV4` status timeline captured every 200 ms:
    - `0..1.9 s` (cold) / `0..1.5 s` (warm): screen is black and `#status` still reads **`"Choose how to play"`** (left over from the front page because `boot.js` blocks on `wotmMeasureRefresh(60)` + `syncMods` + `mainThreadFiles` before loading `wotm.js`).
    - `1.9..6.4 s` (cold) / `1.5..4.85 s` (warm): screen is STILL completely black while `#status` reads **`"Running"`** (no indication that the game is connecting to `/match`, waiting for the other player, or loading the stage).
  - `web/boot.js:51` passes cache hits (`e.data.hit`) into `LoadingProgress.add()`, and `web/loading.js:21-29` prints `"Downloading game data 0% (0 of 57 MB). Only the first visit downloads this much."` before the first chunk arrives or during warm-cache loads.
  - `web/boot.js:126-134,198-202`: `wotmMeasureRefresh(60)` blocks loading `wotm.js` for ~1000 ms (up to 2000 ms) on EVERY online match start and EVERY quick-rematch reload, even though online matches disable frame interpolation by default (`#145`) and the refresh rate can be cached in `sessionStorage` or skipped when online.
- **Proposed fix:**
  1. Skip or cache `wotmMeasureRefresh(60)` in `sessionStorage` for online matches and rematch reloads (`web/boot.js`), saving ~1.0 s of dead time on every match start and rematch.
  2. Set `#status` immediately on `START`/`Rematch` to `"Starting match..."`, then update through `"Connecting to match..."` -> `"Waiting for <names>..."` -> `"Loading stage..."` (clearing to `"Running"` only once `frame >= 1`).
  3. In `web/boot.js:51` / `web/loading.js`: only count network bytes (`!e.data.hit`) toward `"Downloading game data"`, and show `"Loading from cache..."` when chunks are cache hits.
- **Expected gain:** -1.0 s off every online match start and every rematch (warm rematch 4.5-4.9 s -> ~3.5-3.8 s), plus clear live progress instead of a 5-15 s silent black screen saying `"Choose how to play"` / `"Running"`.
- **Ownership:** Ours for `boot.js` / `loading.js` / `front.js` state & text wiring (keep CSS/layout untouched so we stay out of UI worker `ctx_961c202a7992`'s styling work).

### 7. Overlapping items owned by Deploy Owner (`ctx_33885a8e6952`) - reported for handoff
These two high-impact bottlenecks appeared clearly in our measurements and belong to the deploy owner's active scope ("zero waits" plan, Firefox tiering warm-up, input-to-photon floor):
- **7a. Firefox main-thread WebGL + game-thread CPU cost is 2-3x Chrome:**
  - Live prod `H24X`: Firefox `ee_busy_ms=12.9-17.7 ms` (vs Chrome `7.8-9.7 ms`), `work_ms=7.5-30.4 ms` (vs Chrome `2.8-5.4 ms`), `fps=14-39` (vs Chrome `57-60`), `late=763` frames (vs Chrome `64`), `au_gaps=141` (longest 899 ms, vs Chrome `13`). Because lockstep runs at the slowest peer, Firefox dragged the match down to 40-55 gfps and triggered 18-19% wait time on both peers.
- **7b. Cold-cache `START` downloads 27-67 MB of disc chunks + 40 MB wasm synchronously during boot:**
  - Live prod `H24X` `seq=1`: Firefox downloaded `disc_net_mb=27.0` + `17.0` MB during the first 10 s; `T97F` `seq=1`: Firefox downloaded `disc_net_mb=56.0` + `11.0` MB (Chrome `27.0` MB), leaving both clients at `frame=0, gfps=0.0` for >5 s. Because `match_prefetch.js` waits 1000 ms (`delayMs=1000`) and only fetches stage+monster chunks (not base boot chunks or wasm), quick starts or cold visits still stall on network I/O.

---

## Suggested Stage 2 PR Order (Our Scope)

1. **PR 1 (Finding #1):** Fix quick-rematch `REJOIN` race and 120 s ghost name lockout (`native/net/wotm_net_server.cpp`, `web/rematch.js`, `web/front.js`).
2. **PR 2 (Finding #2 + #3):** Ignore boot-load RTT spikes in `pace()` and fix `adviseDelay` hysteresis deadband (`native/net/wotm_net_server.cpp`, `native/net/wotm_net_proto.hpp`).
3. **PR 3 (Finding #6):** Skip/cache `wotmMeasureRefresh(60)` for online starts/rematches and show accurate boot/connect/load status text (`web/boot.js`, `web/loading.js`, `web/front.js`).
4. **PR 4 (Finding #4):** Smooth frame-advantage time sync (Task 8) to eliminate steady-state lockstep wait-storms (`native/net/`, `native/runtime/wotm_sync.inc`).
5. **PR 5 (Finding #5):** Phase-locked present (Task 5) if not already covered by deploy owner's input-to-photon floor work.
