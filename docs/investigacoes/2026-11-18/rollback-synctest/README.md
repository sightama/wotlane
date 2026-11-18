# Rollback synctest, go/no-go (issue #30, phase 1), 2026-11-18

Question (plan `docs/superpowers/plans/2026-11-17-online-seamless.md`, task 9): can wotlane save the whole game state at a frame,
restore it and redo frames with the same result, and what does a redone frame cost in the browser? That is the go/no-go the rollback
spike (`docs/investigacoes/2026-11-16/rollback-spike/README.md`) set before building a hybrid netcode.

**Answer: GO on correctness, with a small payoff.** With the snapshot below, every fight frame restored 2 or 3 frames back and redone
gives the same state byte for byte: 0 mismatches over the runs listed in "Runs", once the stream blobs compared by content (Chrome and Firefox, 1 vs 1 at 60 Hz and 8 monsters at
30 Hz, native Mac, AI and a human player pressing buttons and turning the camera). Two determinism leaks the lockstep could not see were
found and fixed on the way. The cost is what limits the payoff: a redone frame costs about as much as a played one (no drawing, but the
same game logic), so the rollback window that fits a frame is 1 frame in Chrome on a fast Mac at 60 Hz (0 with 8 monsters at 30 Hz)
and 0 in Firefox or on a slower machine (see "Cost" and "Expected latency").

## The synctest

`WOTM_SYNCTEST=<n>` (native and browser, fixed step, offline), in `native/runtime/wotm_rollback.inc` with the pieces in
`native/wotm_rollback.hpp`: at rtMain's frame boundary (entry of `timerWaitForMinUpdateRate`, right before `wotm_sync::frame`) the
state is saved into a ring of n + 1 slots; after n frames of history, every frame restores the slot from n frames back and the game
runs those frames again with the same inputs (the input timeline is not part of the snapshot). Back at the frame where it started, the
whole snapshot is compared byte by byte with the one saved there. A byte compare is stronger than the hash the brief asked for and names
the piece, the address and every differing range. Redone frames skip the frame and fade-step drawing capture, the chase HUD, the pacing wait, sound output
and input sampling. The smaller capture hooks (fonts, flares, letterbox, HUD pieces) still queue into the pending drawing lists in a
redo, so after a rollback the next drawn frame carries n frames of them: harmless in the synctest, to be reset in a hybrid. Stage loads, frames of a dialog loop and sound bank changes are barriers (the history starts over).

Restoring at the boundary needs no host stack work: the guest has one thread, nothing is parked in a host call there, and the restored
CPU context resumes at the same pc and return address, so the recompiled `rtMain` carries on. `WOTM_SYNCTEST_TRACE=<frame>` records the
guest calls (target, return address, register hashes) of a frame's first run and of its redo and prints where they part ways; it found
the first leak in one run.

Tools: `sh tools/synctest.sh` (native, AI vs AI or `--human`), `sh tools/synctest_pieces.sh` (each piece left out in turn),
`node tools/browser_mp_test/synctest.mjs` (Chrome, Chromium or Firefox) and `python3 tools/synctest_cost.py <logs>` (the cost tables).

## What the snapshot holds (39.7 MiB in a 1 vs 1, 46.9 MiB with 8 monsters)

| piece | size | what its absence breaks (first mismatch, 1 vs 1, 2 frames back unless noted) |
|---|---|---|
| guest RAM, 32 MB | 32 MB | everything |
| upper RAM in use: 0x2000000 to the end of the stage load, the map reserve and the top-of-RAM slots | 7.7 MiB (1 vs 1), 14.9 MiB (8 monsters) | frame 2: guest RAM from 0x2b4540 on (the monsters' models and textures live above 32 MB) |
| scratchpad (the EE stack in a stage) | 16 KB | frame 106: guest RAM 0x446690 |
| CPU context (`R5900Context`) | 1648 B | nothing in 75 s: rtMain's registers at the boundary are the same every frame; kept, the pc and return address are what the restore resumes from |
| VU0 data memory | 4 KB | nothing in 75 s; kept as a precaution |
| newlib `rand()` seed (`ps2_stubs::hostStateBlocks`, a new seam in the runtime patch) | 4 B | frame 2: guest RAM 0x2bbac0, 0x732a3c, 0x8278cc |
| pad ports, pad frame count (same seam) | 0.5 KB | nothing seen (the input is overridden every read; `scePadGetFrameCount` unused in a fight); kept |
| fixed-step virtual clock (`wotm_sync::g_virtualTics`) | 4 B | frame 2: guest RAM 0x6f8dcc (next to the timer's field counters) |
| fixed-step vsync parity (`g_syncCalls`) | 4 B | frame 1 with an odd n (`--back 1`): guest RAM 0x9ffe90 (stack), 0x51c844, 0x6e23e4; with an even n the parity comes back by itself |
| fixed-step frame index (`g_index`) | 4 B | the redo reads other frames' input |
| clock calls this frame (`g_ticCalls`) | 4 B | nothing: `frame()` zeroes it before the game reads it; kept |
| orbit camera: cameras (`orbit::g_cam`) | 0.5 KB | frame 2 with a human player (`--human`): guest RAM from 0x256bc0 on; nothing with the AI playing the player (its view does not use the modern camera) |
| orbit camera: inputs (`orbit::g_in`) | 0.1 KB | nothing seen; kept (the right stick of the last frame) |
| modifiers: Fast Monsters' clip table (`g_fastClips`) | 8 B per clip | frame 350 with `WOTM_MODIFIERS=jump,fast`: guest RAM 0x732b48 |
| modifiers: gravity written | 4 B | nothing (written once at the stage load) |
| intro skip armed (`wotm_training::g_introArmed`) | 1 B | frame 3: the intro's skip (see leak 1) |
| sound engine (`wotm::snd::Engine::rollbackPiece`) | about 4 KB at the fight's start, plus 4 shared stream blobs | frame 255: guest RAM 0x8147d8 (the game polls the engine: `isPlaying`, `getTick`) |

Not in the snapshot and not needed: the input timeline (inputs are not state), the native drawing's per-frame lists, the sound
output ring, the page and lobby. Not audited because they are not reachable in a network match: training mode (`wotm_training.inc`,
offline only), the 3 and 4 player split screen (`wotm_split4.inc`, `wotm_modes4.inc`: a network match gives each player one view).
The map creator's maps are covered by the map reserve range.

The sound engine needed real serialization (`native/wotm_snd.hpp` `saveState`/`loadState`): sounds (bank sounds with their children,
LFOs, registers and grain position; streams with their channels), voices (ADPCM position and history, envelope, FIFO, pitch, volume),
handles, the engine's random generator, tick and tick phase, the global grain registers, and the last choice RAND_PLAY and PLAY_CYCLE
keep in the bank's grains. A voice's samples are saved as (bank, offset) or as a stream blob: a stream's samples become shared,
immutable data, so a snapshot does not copy them. Unit tests in `native/tests/wotm_native_test.cpp` replay 24 frames after a restore
and compare handles, ticks, playing flags, mixed samples and the snapshot; dropping the grain state or the random generator from the
restore makes them fail.

## Leaks found

1. **The intro skip of a direct fight was host state.** `wotm_training.inc` `cinemaUpdate` skips the fight's intro once (every network
   match is a direct fight) and clears `g_introArmed`. The redo of the frame that skipped it saw the flag already clear and played the
   intro on: the call trace showed `TheGame::fadeOutAndIn` after `Monster::updateCinema` only in the first run. The flag is now in the
   snapshot. The lockstep never saw it, since every PC runs each frame once.
2. **The host's vblanks leaked into the field counter at the boundary.** `waitMinRateByVblanks` wrote the fields the host measured into
   `D_006F8DBC` right before the boundary, in fixed step too, where `wotm_sync::frame` overwrites it at once with the frame's exact length.
   No guest code ran in between, so the game never read it, but the snapshot did (mismatches around frame 450 to 490 whenever a frame and
   its redo took a different number of vblanks). The write is skipped in fixed step.

And one false alarm: a music stream started inside the window is read again by the redo, giving another blob with the same bytes. Blobs
now compare by content.

## Runs

M1 Max (10 cores), Chrome 157 (the real one, `channel: chrome`), Playwright's Firefox, the native Mac build; other Orca workers were
using the machine at the same time, and runs went two at a time. The synctest slows the game (each frame also saves, restores, redoes n
frames and compares 40 MB), so 30 minutes of wall clock is fewer game minutes; the frame counts below are the frames checked.

| run | back | wall clock | frames checked | mismatches |
|---|---|---|---|---|
| Chrome, 1 vs 1, 60 Hz | 2 | 30 min | 55 800 | 0 |
| Firefox, 8 monsters, 30 Hz | 2 | 30 min | 18 300 | 0 |
| Chrome, 8 monsters, 30 Hz | 2 | 30 min | 24 900 | 0 |
| Firefox, 1 vs 1, 60 Hz | 2 | 30 min | 20 400 | 0 |
| native, 1 vs 1, 60 Hz | 3 | 60 min | 84 900 | 0 |
| native, 1 vs 1, human pressing buttons and turning the camera (`--human`, `PS2X_PAD`, `WOTM_ORBITMOUSE`) | 3 | 3.3 min | 5 700 | 0 |
| native, 1 vs 1 / 8 monsters, before the blob fix | 2 | 4 min each | 3 600 / 3 600 | 0 until a stream restarted in the window |

## Cost

Median of the 300-frame window averages / worst window, ms (`tools/synctest_cost.py`). "Redone frame" is one resimulated frame (game
logic, no drawing capture); "played frame" is the frame's own work without the pacing wait (the log and `synctest_cost.py` call them "resimulated
frame" and "present frame"). The compare only exists in the synctest.

| | save | restore | redone frame | played frame |
|---|---|---|---|---|
| Chrome, 1 vs 1, 60 Hz | 1.39 / 24 | 1.36 / 54 | 5.51 / 77 | 5.77 / 97 |
| Chrome, 8 monsters, 30 Hz | 2.06 / 36 | 2.00 / 25 | 14.75 / 215 | 15.16 / 114 |
| Firefox, 1 vs 1, 60 Hz | 3.11 / 275 | 3.03 / 337 | 10.70 / 655 | 11.05 / 720 |
| Firefox, 8 monsters, 30 Hz | 3.49 / 239 | 3.33 / 139 | 24.06 / 544 | 25.19 / 542 |
| native, 1 vs 1, 60 Hz | 1.88 | 1.82 | 4.78 | 4.73 |
| native, 8 monsters, 30 Hz | 2.85 | 2.20 | 9.70 | 10.59 |

These come from the long runs, with two browsers and a native game at a time on one machine, so they are high. The native 60-minute 1 vs
1 run (3 frames back) averages save 1.71, restore 1.66, redone frame 4.08 and played frame 4.44 ms; its last windows, with the machine
quieter, 1.0, 1.0, 2.0 to 2.5 and 2.7 to 3.2 ms. In every case a redone frame costs about as much as a played one: the drawing capture is cheap
next to the game's own logic.

## Expected latency (hybrid)

The window W that fits is the largest with played + save + restore + W x redone under the frame (16.7 ms at 60 Hz, 33.3 ms at 30 Hz),
with some margin, since a frame that runs long slows every player in a fixed-step match:

| | today's delay stage | W that fits | delay with W (floor 2 frames at 60 Hz, 1 at 30 Hz) | gain |
|---|---|---|---|---|
| 1 vs 1, rtt 40, Chrome, fast Mac | ~65 ms (4 frames) | 1 (5.9 + 2.8 + W x 5.6 <= 16.7: 14.3 with W = 1, 19.9 with W = 2) | 3 frames | 17 ms of ~111 ms total |
| 1 vs 1, Firefox, any machine | ~65 ms | 0 (a played frame is ~19 ms already, PR #144) | unchanged | 0 |
| 1 vs 1, an ordinary laptop (2 to 3 times slower) | ~65 ms | 0 | unchanged | 0 |
| 4 to 8 players, 30 Hz, Chrome, fast Mac | ~133 ms (4 frames) | 0 (15.2 + 4.1 + 14.8 = 34.1, over 33.3) | unchanged | 0 |
| 4 to 8 players, 30 Hz, native, fast Mac | ~133 ms (4 frames) | 1 (10.6 + 5 + 9.7 <= 33.3) | 3 frames | 33 ms of ~210 to 245 ms |
| 4 to 8 players, Firefox | ~133 ms | 0 (25.9 + 7 already near 33.3) | unchanged | 0 |

Rollback also costs every frame a save (1.4 ms in Chrome, 3.5 ms in Firefox with 8 monsters) whether or not anything is mispredicted,
and with 7 remote players a misprediction lands on most frames, so the whole window is spent most of the time.

## Hybrid design

- **Setting.** The lobby owner picks ROLLBACK off (default), 1 or 2 frames; it travels like the modifiers (`SET rollback=`, the
  `SETTINGS` and `MATCH` lines, `WOTM_ROLLBACK` in the match environment) and is in the config hash: every PC must use the same W.
- **Delay.** Each client applies `max(floor, Pace advice - W)` frames to its own input (floor 2 at 60 Hz, 1 at 30 Hz, about 33 ms).
  The server's Pace advice, holdback and "behind" measures stay as they are: every client shortens its delay by the same W.
- **Prediction.** A remote input that hasn't arrived repeats that player's last confirmed input. Frame F runs once every player's input
  up to F - W is confirmed; past that it waits as today (the stall message, `netWaitFrame`).
- **Rollback.** At each boundary the client records which inputs each frame used. When a confirmed input differs from the prediction a
  frame used, it restores the slot before that frame and redoes the frames up to the present with the corrected inputs, saving each
  slot again. Only the last redone frame is drawn: the per-frame drawing lists are reset at the other redone boundaries, and its sound
  is the only one heard (a sound that started in a mispredicted frame is cut, one that should have started is heard from where it is).
- **Desync check.** A frame is final once every input up to it is confirmed (at most W frames after it ran). The hash blocks are taken
  from its slot at that point instead of from the live RAM, so the server keeps comparing final states only.
- **Barriers.** A stage load is a barrier. A dialog loop (pause, round end) is the hard case: the frame before it may have run on a
  prediction, and a dialog frame is not rtMain's boundary. The plan: dialog-loop frames run in lockstep (they wait for every input), and
  a misprediction found there restores the rtMain slot through the runtime's unwind and resume by pc (the hook returns without calling
  the original and the recompiled callers unwind, as at a scheduler checkpoint); the synctest is extended to restore from dialog
  frames before the hybrid relies on it. A redo that reaches a dialog ends there: the dialog frame is the present.
- **Budget guard.** Each client measures its own played and redone frame times (the synctest's numbers, from the same code); when W
  does not fit, it logs it and the diagnostics report it, so a match that slows down names the device.

## Limits

- Measured on one M1 Max shared with other workers; no slow laptop. The "2 to 3 times slower" laptop is the spike's estimate.
- The orbit camera's inputs and the pad state showed nothing missing in these runs; a recorded human match (`WOTM_INPUTPLAY`) would
  exercise more paths than a pad script.
- Dialog loops are barriers in this synctest; restoring from them is the next step.
- The runtime scheduler's queued invocations (vblank, DMAC handler) are not in the snapshot; none was queued at a boundary in these
  runs (no mismatch), but a pass does not prove it can't happen.
