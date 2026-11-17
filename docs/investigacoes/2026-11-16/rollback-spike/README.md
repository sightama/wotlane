# Rollback netcode spike (issue #30), 2026-11-16

Question: should wotlane replace or extend its delay-based lockstep with rollback (GGPO-style), and how? This is a design spike for
batman (Lane) to decide on; nothing here is merged code. The only code written is a measurement hook on the scratch branch
`sightama/rollback-spike-bench` (`WOTM_SNAPBENCH`, not for merge).

**Short answer:** not full rollback now. Tune the lockstep first (cheap, about one frame back at every ping). If rollback is still
wanted after that, build it as a hybrid (a small input delay plus a short rollback window), and only after a single-player
"synctest" proves that save, restore and resimulation give identical state. The snapshot itself is cheap in the browser (under 1 ms
each way); what limits rollback is the cost of resimulating frames on the EE thread and the amount of state that lives outside RAM
(the sound engine above all).

## Setup

- Browser build (wasm) built from `85f51e6e` plus the bench, Chrome 157 (Playwright, headless), Apple M1 Max, 10 cores, 32 GB.
  Other Orca workers were using the machine at the same time.
- Direct AI fights with the online settings: `WOTM_FIXEDSTEP=1` (1 vs 1, 60 Hz logic) and `WOTM_FIXEDSTEP=2` with 8 monsters
  (`WOTM_MATCH=3,1,2,4,5,7,8,9,1`, 30 Hz logic, which is what the server uses for 3 or more players), `WOTM_SEED=7`, about
  100 s each.
- `WOTM_SNAPBENCH=1` runs at every frame boundary (the entry of `timerWaitForMinUpdateRate`, where the lockstep already hashes):
  compares the 64 MB host RAM with a shadow copy page by page (4 KB), copies the 32 MB guest RAM into a slot, copies it back (same
  bytes), and reports the EE thread's busy time (frame interval minus the scheduler's idle wait). `WOTM_SNAPBENCH=2` skips the
  compare, so the fight runs at full speed. Raw lines: `snapbench_runs.txt` (the `web1` run has a bug in its busy column, counted
  twice; the other runs are fixed).

## 1. Input latency today

The server fixes the delay once, at match start, from the WebSocket or ENet round trips (`proto::pickDelay`,
`native/net/wotm_net_proto.hpp`): half the slowest round trip plus half the second slowest, in frames, plus 1 frame of slack,
clamped to 2..15. 1 vs 1 runs at 60 Hz (16.7 ms frames); 3 to 8 players run at 30 Hz (`step=2`, 33.3 ms frames). The delay is
what the netcode adds on top of single-player input lag (input sampled at the frame boundary, frame interpolation) and is the
same for every player in the match.

| everyone's ping to the server | 1 vs 1 (60 Hz): delay | added lag | 3 to 8 players (30 Hz): delay | added lag |
|---|---|---|---|---|
| 20 ms | 3 frames | 50 ms | 2 frames | 67 ms |
| 60 ms | 5 frames | 83 ms | 3 frames | 100 ms |
| 120 ms | 9 frames | 150 ms | 5 frames | 167 ms |

Computed from the code (checked against the unit tests in `wotm_net_test.cpp`), not measured on a wire. The two slowest players
set it for everyone: one player at 120 ms and one at 20 ms in a 1 vs 1 gives 6 frames (100 ms).

Reading the relay path (`Server::sendInputs` and the match loop in `wotm_net_server.cpp`): an input that reaches the server is
forwarded to a player when that player's own next packet arrives, or 16 ms after the last send, inside a 5 ms poll. That adds about
8 ms on average (up to about 20 ms) that the delay formula does not count; the "+1 frame of slack" absorbs it. Not measured.

## 2. Snapshot and restore cost (browser)

Averages of 300-frame windows; max is the worst frame in a window, leaving out the first window of a fight (stage start).

| | 1 vs 1, 60 Hz | 8 monsters, 30 Hz |
|---|---|---|
| copy 32 MB guest RAM into a slot | 0.80 ms (max 1 to 2, once 5.2) | 0.81 ms (max 1.2 to 2.7) |
| restore 32 MB from the slot | 0.89 ms (max 1.1 to 2, once 6.8) | 0.87 ms (max 1.3 to 1.8) |
| compare 64 MB with a shadow, copy changed pages | 6.85 ms | 6.92 ms |
| 4 KB pages changed per frame, below 32 MB | 180 to 200 (about 0.75 MB) | 270 to 295 (about 1.15 MB) |
| 64 KB blocks changed per frame | 46 to 49 | 60 to 64 |
| 4 KB pages changed in the last 8 frames | 188 to 208 | 292 to 311 |
| 4 KB pages changed per frame, above 32 MB | 22 to 23 | 93 to 96 |
| EE busy per logic frame | 4.2 to 4.7 ms (max 5 to 7, rare spikes to 20 to 32) | 7.3 to 7.7 ms (max 9 to 10.5) |
| frame rate with `WOTM_SNAPBENCH=2` | 60 | 30 |

What this says:

- **A full copy is cheap.** 32 MB in under 1 ms each way in wasm. The used part above 32 MB (monsters and textures end 3.7 MB above
  32 MB in the 1 vs 1 and 10.9 MB above in the 8-monster fight) would add 0.1 to 0.3 ms by the same rate. A whole snapshot is
  about 1 to 1.2 ms to save and the same to restore.
- **Incremental snapshots do not pay in the browser.** The working set is small and stable (the same ~200 to 300 pages every frame),
  but finding it costs more than copying everything: wasm has no `mprotect`, so dirty tracking means either comparing (7 ms, measured)
  or a store barrier in every recompiled write (a cost on every frame of every player, for a saving of under 1 ms). Full copies win.
- **RAM above 32 MB changes during a fight** (22 pages per frame in a 1 vs 1, 94 with 8 monsters). The lockstep hash covers only the
  lower 32 MB, so a rollback snapshot (and the desync hash) has to include the used upper region too.
- **The limit is resimulation.** A rollback of N frames costs one restore plus N frames of EE work. At 60 Hz the EE thread has
  16.7 ms: 4.3 for the current frame, about 1.2 to save and 1.2 to restore leaves room for about 2 resimulated frames on this
  machine (3 at the frame's own max). At 30 Hz with 8 monsters: 33.3 ms, 7.5 per frame, about 3 resimulated frames (100 ms of
  hidden latency). Busy time includes the native scene capture, which a resimulated frame would skip, so these are upper bounds.
  A typical laptop is 2 to 3 times slower than an M1 Max in Chrome, which leaves 0 to 1 frame at 60 Hz.

## 3. What breaks determinism on resimulation

The lockstep only needs every PC to compute the same thing from the same inputs, which the hashes already check. Rollback also needs
a restored state to be complete: everything the game reads back must be saved. From reading the runtime (PS2Recomp `ps2xRuntime`,
the patches in `tools/runtime_patch/`, and `native/`):

Must be in the snapshot besides the 32 MB:
- **RAM above 32 MB**, the used part (see above).
- **Scratchpad, 16 KB:** the EE stack lives there during a stage.
- **The main thread's `R5900Context`** (about 1.6 KB). The game runs one guest thread; at the frame boundary nothing is parked in a
  host call stack (`sceGsSyncV` waits in place), and in wasm the EE is a plain pthread (JSPI is only on the page's main thread). So
  save and restore at that same hook need no stack serialization. Restoring at a different hook (the dialog loop's frame boundary
  in the `sceGsSyncV` wrapper) would need an unwind-and-resume by pc; simplest is to never roll back across a dialog.
- **Host statics the game reads:** the newlib `rand()` seed (`s_newlibRandNext`, `Stubs/LibC.cpp`; the game's own Mersenne
  Twister is in RAM), the fixed-step counters (`g_virtualTics`, `g_ticCalls`, `g_syncCalls`, `g_index`), libpad port state,
  the orbit camera (`g_cam`, `g_in` in `wotm_orbit.inc`), modifier state (`g_fastClips` and friends in `wotm_modifiers.inc`),
  VU0 data memory and the DMA registers as a precaution. About 23 KB in all.
- **The native sound engine** (`wotm_snd.hpp` `Engine`): live sounds, voices, handles, its own RNG and tick. The game polls it
  (`isPlaying`, `getTick`, `sramFree`, `getMasterVolume`), and in fixed step it advances once per game frame (`wotmSoundFrame`),
  so it is game state. It holds polymorphic `shared_ptr<Voice>` objects: it needs real serialization code, the largest single job.
- **Not audited:** the 3 and 4 player screen and training globals (`wotm_split4.inc`, `wotm_modes4.inc`, `wotm_training.inc`),
  `wotm_skins.inc`, and whether the game's `malloc` stubs (`guestMalloc`, a host-side block table) are used in a fight.

Must be suppressed while resimulating (output or pacing, not state):
- **Audio output:** a resimulated frame must not play sounds again; sounds started in a mispredicted frame must be stopped or
  faded. Usual approach: the engine state rolls back, the mixer only starts a voice for a (frame, sound) pair it has not already
  started.
- **Drawing:** `wotmCaptureScene`, `wotmCaptureFrame`, particles and the interpolation snapshots run only for the last frame.
- **Pacing:** the vsync waits (`waitVSyncHere`, the `frameVsyncs > 1` loop in `wotm_overrides.cpp`) and `netWaitFrame` are skipped.
- **Wall clock:** in fixed step the game's time is already virtual (`callTics`), and `sceCdReadClock` is fixed. Outside fixed step
  the timer reads `steady_clock`; rollback requires fixed step, as the lockstep does today.
- **Movies:** none play inside a match (they are skipped in the browser anyway).
- **Disc reads:** synchronous, nothing in flight at the boundary. A stage reload (REBUILD LEVEL) must be a barrier: no rollback across it.
- **Local UI:** the personal pause and the panels already only change the input that is sent, which is exactly what rollback needs.
  The menu-only `std::mt19937` seeded from `random_device` stays outside matches.

The test that proves all of this is GGPO's "synctest": in single player, every frame, restore the state from N frames back and
resimulate with the same inputs, then compare a hash of the whole snapshot. It needs no network, runs in the AI vs AI batch
setup, and any state missing from the snapshot shows up as a mismatch on the first frame that reads it.

## 4. Options

| | adaptive lockstep | rollback (GGPO-style) | hybrid (delay floor + rollback window) |
|---|---|---|---|
| what it is | keep waiting for inputs; tune the delay and the relay | no delay; predict remote inputs, roll back and resimulate when they differ | a small fixed delay plus rollback for the rest of the path |
| payoff, 1 vs 1 at 60 ms | 5 to about 4 frames (83 to 67 ms) | 0 delay only if 5 frames fit in a frame's budget: they do not (about 2 fit here) | 3 delay + 2 rollback: 50 ms here; little or nothing on slow laptops |
| payoff, 3 to 8 players at 60 ms | 3 to about 2 frames (100 to 67 ms) | with 7 remote players a misprediction is almost every frame, so the full window is spent all the time | 1 delay + 2 rollback: about 33 ms here; CPU-bound on slow machines |
| effort | days | months | weeks to months (most of rollback's work, minus pure prediction tuning) |
| risk | low: same model, same tests | high: every missed piece of state is a desync that only shows when a rollback happens; visual pops on mispredictions | high, but bounded: falls back to delay when the window does not fit |
| browser cost | none | up to the whole EE budget | capped by a measured budget |

Lockstep tuning that is concrete:
- Relay an input to the others as soon as it arrives (today it waits for the receiver's next packet or 16 ms): about 8 ms back.
- Size the delay from the measured relay path, not RTT/2 + 1 frame of slack, and re-pick it during the match (both sides switch at
  an agreed frame, repeating or dropping one frame of local input): the delay follows the real ping instead of the first one.
- Log the stalls per player in the match summary, so the tuning is measured on real matches.

## Recommendation

1. **Do the lockstep tuning now.** It is low risk, helps every player at every ping, and does not touch game state.
2. **Do not start full rollback.** On the primary platform the resimulation budget is about 2 frames at 60 Hz on a fast Mac and less
   on an ordinary laptop, so rollback can hide at most about 33 ms in a 1 vs 1, and only for players with fast machines. That is a
   real but small gain for the largest piece of work and the largest ongoing risk in the project.
3. **If batman still wants rollback after step 1, start with the synctest (phase 1 below) as a go/no-go.** It is also the proof
   that `TODO.md`'s "Discarded" entry asks for ("it would require proving that the logic always gives the same result from the same
   inputs").

## Phased plan

- **Phase 0, lockstep tuning (days).** Immediate relay; delay from the measured path; mid-match delay change; per-player stall
  numbers in the server log. Measure with `tools/browser_mp_test` and the cross-play scripts at 20/60/120 ms of added latency.
- **Phase 1, synctest (1 to 2 weeks).** A `WOTM_SYNCTEST=<n>` mode in the browser and native builds: full snapshot (lower 32 MB,
  used upper RAM, scratchpad, context, the host statics above, the sound engine), resimulate n frames every frame with the same
  inputs, compare hashes. Resimulation mode skips capture, vsync waits and audio output. Exit: 30 minutes of 1 vs 1 and of
  8 monsters without a mismatch, and the resimulation cost measured on an ordinary laptop in Chrome and Firefox.
- **Phase 2, hybrid netcode (2 to 4 weeks).** Prediction (repeat the last input), rollback window sized at runtime from the measured
  frame cost, delay floor for the rest, barriers at dialogs and stage loads, desync hash extended to the upper RAM and scratchpad,
  sound deduplication. The server relay stays the same.
- **Phase 3, 3 to 8 players.** Tune the window for 30 Hz and frequent mispredictions; fall back to pure delay on a machine that
  cannot keep up.

## Limits of this spike

- Measured in Chrome on an M1 Max only; no Firefox and no slow laptop. The "typical laptop is 2 to 3 times slower" figure is an
  estimate, and phase 1 must measure it.
- The upper RAM and the 23 KB of host state were not added to the copy timing; they are sized by the same rate.
- The latency table is computed from `pickDelay`, not measured on a network with added latency.
- `TODO.md` lists "Our own netcode (rollback)" under "Discarded by the owner". This spike does not change that line; the lockstep
  tuning item it adds is not rollback.

## Follow-up, 2026-11-17: phase 0 done (PR #104)

- Correction to section 1: the server already relayed each input as soon as it arrived. The 16 ms timer only resends what an ENet
  player still lacks; nothing waits for the receiver's next packet. So "immediate relay" needed no change.
- Each player's delay is now their own path (half their round trip plus half the slowest other player's, at the high end of the last
  4 s, plus 3 ms), re-picked every 500 ms and stepped one frame at a time (`Pace`, `LocalSchedule`).
- Measured AI vs AI with `tools/net_lagproxy.py` (jitter a quarter of the ping): 1 vs 1 at 20 ms went from 3 to 2 frames, 4 players with
  one at 120 ms and three at 20 ms from 4 to 3; 60 ms and the other 4 player cases stayed the same, and 1 vs 1 at 120 ms with 30 ms of
  jitter went from 9 to 10 (the jitter allowance, twice ENet's variation, costs what the old fixed frame of slack did). The full table,
  with the load of each run, is in PR #104. 8 players passed `tools/browser_mp_test` (match, round, stall) in sync.
- Rollback is still parked; the synctest above stays the go/no-go if Lane wants it.
