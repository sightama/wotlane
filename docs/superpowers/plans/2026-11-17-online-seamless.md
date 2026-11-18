# Seamless Online Play Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task (one PR per task into `wip/browser`, TDD, CI green, two independent reviewers, desync matrix 0, then a serialized deploy). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** online matches feel immediate and run at full rate for every player: low input latency, no delay flapping, slow and odd browsers (Firefox on a 60 Hz screen, Brave) not holding everyone back.

**Architecture:** keep deterministic lockstep (the game is the referee on every PC) and remove the latency and pacing losses the measurement found, in order of gain per risk; then decide rollback on the evidence of a single-player synctest. Changes to the game's own timing are allowed (Lane, 2026-11-17: "We no longer care about the game being maintained as it used to be ... I want improved performance") as long as every PC computes the same frames.

**Tech Stack:** C++20 runtime and netcode (`native/`, PS2Recomp seam), wasm via Emscripten 7, page scripts (`web/`), `wotm_server` on the GCP VM, coord `scripts/match_diag.sh`.

**Spec:** this document (decisions delegated by Lane through the coordinator) plus the evidence in PR #133, issue #30 and `docs/investigacoes/2026-11-16/rollback-spike/README.md`.

## Where the time goes today (PR #133, `lat_*` diag fields)

4 players, 30 Hz, rtt 60 to 80 ms with one player spiking (68LN-like, `tools/browser_mp_test/latency.mjs`), steady windows:

| stage | ms | cause |
|---|---|---|
| sample | 17 | half a 33 ms frame (inherent to the frame rate) |
| delay | 133 | 4 frames x 33.3; 3 would cover the path, spikes push it to 4 to 7 |
| sim | 31 | `timer1Handler` waits for the next vsync after the frame is done, before the capture |
| present | 25 | capture to the animation frame after the draw |
| blend | up to 33 | frame interpolation shows the scene one game frame late (on by default above 30 Hz) |
| **total** | **~210 to 245**, 270 to 310 in spike windows | |

Live: 68LN and VKA5 ran at 22 to 25 of 30 game frames a second because one player (Jake: Firefox, 60 Hz screen, 5.5 ms main-thread work per present) computed only ~24; the game thread's own busy time is not reported yet, so the cause is not named. Two Brave 157 players (5QMC, VKA5) ran 0 to 24 game frames a second with 600 ms long tasks and animation-frame gaps; Brave farbles `gpu` and `cores` in the report, so it can't tell software GL from a real GPU. Every client showed a 2.6 to 4.8 s input gap at match start (one client's boot).

## Decisions (made for Lane; each is the best-practice call, with the reason)

- **Lockstep stays; rollback waits for the synctest.** Rollback adds most where the delay is long and CPU is plentiful; here the browser's resimulation budget is ~2 frames at 60 Hz on an M1 Max (spike README), and the measured losses below are bigger and cheaper to remove. Rollback is approved to build if the synctest says GO (task 9).
- **The delay covers the round trip a player has 4/5 of the last 8 s** (not the maximum of the last 4 s): a spike then costs a short wait instead of a frame of delay for everyone (task 1).
- **Fixed-step frames are paced on a host deadline, not on vblank ticks:** game time is already virtual in a match, so the vsync waits inside the frame only add latency and, on a slow device, round a 35 ms frame up to 50 ms (task 3).
- **Online at 30 Hz, interpolation is off by default; the HUD always shows the newest frame.** A fighting game trades smoothness for response; a player can turn it back on (task 4).
- **The network stays on the main thread for now.** `lag_ms` (task lag on the page's queue, which the WebSocket shares) is 0.1 ms median and at most 1 ms on Jake's device; a worker would buy almost nothing.
- **Brave is reproduced, not guessed:** install Brave on the Mac and run the harness in it (task 6).

## Global Constraints

- Every change that touches what the game computes passes `tools/browser_mp_test/desync.mjs --cases 2,3,4 --scenarios baseline,pause,stall` with 0 desyncs before merge.
- English only; no em or en dashes anywhere; reference docs in the present tense (`CLAUDE.md`).
- Deploys to the VM are serialized with the other workers: message them before and after; server before page when a protocol or diag field changes.
- Every PR states the before and after numbers from `latency.mjs` or live `match_diag.sh`.

## Review Focus

- A player whose ping really rises for good: the delay must follow within ~2 s, and nobody may stall for more than that.
- A device that can't make the frame rate: it must slow the match smoothly (no 30/20 fps alternation) and keep every PC's state equal.
- A match start where one client boots slowly: nobody's first seconds should be a freeze followed by a burst.
- Pause, personal pause and stalls (network held, tab hidden) after the pacing change: they resume without a burst of frames.
- Native (Windows, Mac) players in the same match as browsers: the pacing change must hold for them too (same `wotm_sync.inc`).

---

### Task 1: Delay covers 4/5 of the last 8 s of round trips (server only)

Gain: no delay flaps on spikes (68LN: dozens of 3 to 4 to 5 changes for all members); 33 to 66 ms back during spike periods. Cost: small. Risk: a short wait per spike. Measure: `event=delay` count per minute and `lat_dl_ms` in `match_diag.sh`, `waits` per window.

**Files:** Modify `native/net/wotm_net_proto.hpp` (`RttWindow`), `native/net/wotm_net_server.cpp` (`rttLevels`, diag `rtt_level_ms`), docs `docs/ONLINE.md`, `docs/passos/WEB.md`. Test `native/net/wotm_net_test.cpp`.

**Interfaces:** Produces `RttWindow::kSamples = 16`, `uint32_t RttWindow::level() const` (nearest-rank 80th percentile); diag key `rtt_level_ms` on the per-player line, the `delay` and `start` events.

- [x] Test `proto_rtt_window_covers_four_fifths_of_the_recent_round_trips`: 3 spikes of 230 among 60s give 60; a fourth gives 230; back to 60 once at most 3 are left.
- [x] Implement; `sh tools/test_net.sh` passes.
- [ ] `latency.mjs --players 4 --rtt 60,60,70,80 --jitter 15 --spike 4:6:150:400 --secs 120`: delay changes and `lat_dl_ms` against the PR #133 run; desync matrix; PR; coord `match_diag.py` reads `rtt_level_ms`.

### Task 2: The game thread's own time per frame in the diag

Gain: names Jake's (and Brave's) bottleneck in one live match. Cost: small. Risk: none (read only).

**Files:** Modify `native/runtime/wotm_sync.inc` (`netPace` publishes it), `native/wotm_webfs.cpp` (`wotm_web_net_stat` index 5 and 6), `web/diag.js` (`NET_STAT`, fields), docs `docs/ONLINE.md`. Tests `web/diag.test.mjs`.

**Interfaces:** Consumes the runtime's idle accounting (`g_wotmIdleUs`, `EeSchedulerGame.cpp wotmIdleWait`). Produces diag fields `ee_busy_ms` (mean game-thread busy ms per game frame in the window) and `ee_idle_ms`.

- [ ] Test in `diag.test.mjs`: `windowFields` turns cumulative busy and idle µs and frames into `ee_busy_ms`/`ee_idle_ms` per frame; absent counters leave them out.
- [ ] Implement, `sh tools/test_web.sh`; one local Firefox match shows plausible numbers (busy well under 33 ms on the M1).

### Task 3: Fixed-step frames paced on a host deadline; capture as soon as the frame is done

Gain: `lat_sim_ms` from ~16 (60 Hz) / ~31 (30 Hz) to the logic's own 4 to 8 ms; a device whose frame takes 35 ms runs at ~28 instead of alternating 30 and 20 (Jake's 24 if his busy time is near 33 ms, task 2 says). Cost: medium. Risk: determinism (game time is virtual in fixed step: none expected) and pause/stall resumes.

**Files:** Modify `native/runtime/wotm_overrides.cpp` (`waitMinRateByVblanks`, `timer1HandlerNative`), `native/runtime/wotm_sync.inc` (`syncVParity`), a new pure `native/wotm_framepace.hpp` (the deadline grid), docs `docs/passos/NATIVO.md`, `docs/passos/WEB.md`. Test `native/tests/wotm_native_test.cpp`.

**Interfaces:** Produces `wotm::framepace::Grid` with `void start(double nowMs, double periodMs)`, `double next(double nowMs)` (the next frame's start: the grid point after the last, or `nowMs` when the frame ran past it, restarting the grid after a stall over 4 periods) and `void resume(double nowMs)` (after a network wait or pause).

- [ ] Tests `framepace_*`: on time keeps the grid; a 35 ms frame at 33.3 starts the next at 35 (not 50); a 200 ms stall restarts the grid from now (no burst); resume after a wait does not wait again.
- [ ] In fixed step only: `sceGsSyncV` called from `timer1Handler` returns the parity without waiting; at the entry of `timerWaitForMinUpdateRate` the capture happens first, then the wait until `Grid::next`, then `wotm_sync::frame` (sample, network). Single player (no fixed step) unchanged.
- [ ] `latency.mjs` 1 vs 1 and 4 players: `lat_sim_ms`, gfps; `python3 tools/determinism_check.py`; the desync matrix with `--firefox 1` added.

### Task 4: Online 30 Hz without interpolation lag; HUD from the newest frame

Gain: up to 33 ms (`lat_bl_ms`) at 30 Hz on screens above 30 Hz. Cost: small. Risk: visual (30 fps motion on 144 Hz screens; the setting brings blending back).

**Files:** Modify `native/runtime/wotm_live.inc` (the `want` interpolation condition; HUD pair), `native/wotm_settings.hpp` (online default), settings UI copy in `web/settings_ui.js` if the option is shown there. Test `native/tests/wotm_native_test.cpp` (settings profile for network matches).

- [ ] Test: the network-match settings profile at 30 Hz has interpolation off unless the player turned it on; at 60 Hz unchanged.
- [ ] Implement; `latency.mjs` shows `lat_bl_ms` 0 and `lat_ms` down by the blend.

### Task 5: Present in phase with the game's frame end

Gain: ~8 ms average of `lat_pr_ms` (the capture lands at a random point of the display's frame today). Cost: medium. Risk: low (presentation only).

- [ ] Anchor the task 3 grid to the page's animation-frame timestamps (the page passes the last rAF time; the grid's phase moves so the frame is captured a few ms before the next rAF). Pure phase math in `wotm_framepace.hpp` with tests; measure `lat_pr_ms`.

### Task 6: Brave root cause

Gain: Brave players stop dropping a match to 0 to 24 frames a second. Cost: unknown until reproduced. Risk: none to others.

- [ ] Install Brave on the Mac (`brew install --cask brave-browser`), run `latency.mjs` and `tools/web_fps.mjs` with Brave's binary (Playwright `executablePath`), Shields on and off. Check: WebGL renderer (software?), `performance.now()` resolution, `Atomics.wait` wake latency, long tasks. Fix in the page (a Brave notice exists as a stopgap) or the build according to the finding.

### Task 7: Match start without a boot stall for everyone

Gain: no 2.6 to 4.8 s freeze at the start. Cost: small to medium. Risk: low.

- [ ] Find what the slow client was doing in those seconds (its page log: disc chunks, shader compile, level load) and start the match clock once every client reports its first frame ready (or preload what it was missing). Measure `in_gap_max_ms` of the first server window.

### Task 8: Smooth catch-up instead of stalls (time sync)

Gain: fewer visible hitches when one PC runs ahead (GGRS frame advantage: the leader slows its frame by up to ~10% instead of stopping). Cost: medium. Risk: low (pacing only, no state).

- [ ] With task 3's grid: the server already knows `behind` per player; each client stretches its period by `min(10%, advantage/30 frames)`. Measure `waits` and `wait_ms` per window.

### Task 9: Rollback go/no-go (synctest), then hybrid if GO

- [ ] `WOTM_SYNCTEST=<n>` in the browser build: full snapshot (guest RAM, used upper RAM, scratchpad, main context, host statics, sound engine), every frame restore n back, resimulate with the same inputs, compare hashes; resimulated frames skip capture, vsync and audio. Run 1 vs 1 (60 Hz) and 4 to 8 monsters (30 Hz) in Chrome and Firefox; report mismatches and the resimulation cost per frame.
- [ ] GO: write the hybrid design (delay floor 1 to 2 frames + rollback window sized from the measured cost, prediction = repeat the last input, barriers at dialogs and stage loads) with expected latency, then build it behind a setting, off by default. NO-GO: name exactly what blocks it.

## Order

1, 2 (both small; 2 ships with the next web deploy), 3, 4, 6, 7, 5, 8, 9. Each is its own PR; each PR's description carries its before and after numbers.
