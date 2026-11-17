# Desync matrix for browser lobbies, 3 to 10 players (2026-11-16)

Tool: `tools/browser_mp_test/desync.mjs` (new, next to `run.mjs`; it shares `lib/`). Each trial forms a lobby of N browser players
(one context each, own OPFS), plays a scripted trigger, then lets the match run to the win screen (`WOTM_CHEATS=vencer=70`, wins 1) and
compares the state hashes of every tab. Locally `wotm_server` also checks the full 32 MB of guest RAM every 32 frames, 256 blocks of
128 KB each, and logs a `DESYNC` line naming the differing blocks: that is the per-block comparison; the tabs print one hash in 16
(about one per 17 s at 30 fps), which is all a live run can compare. A trial passes when no hash differs, the server logs no DESYNC,
no tab crashes or stops, and the win dialog opens on every tab that stayed.

Host: the Mac with 10 cores, shared with other workers: load average 4 to 65 during the runs. Chromium 156 headless shell (software GL),
Firefox 157 for the mixed rows. Local build `wl-d08db85ec26a734c` (web_dist of the browser worker's tree, with `web/*.js` from
`f641de48`, harness from the same commit); live site `wl-3e9ee2e7bacf48de`.

## Result

100 local trials: **0 desyncs**, 85 pass, 14 inconclusive (host overloaded), 1 fail. Every trial's stage and seed is in `trials.md`
(seed from the server's `match with ... seed N` line; the stage changes per repetition). Evidence (tab logs and server log slice per trial)
is outside the repo in `~/wotlane-evidence/desync-matrix-2026-11-16/<run>/n<N>/<scenario>-r<rep>[-a|-b]/`.

Scenarios: `baseline` (play to the win), `pause` (the #35 suspect: three times a different player opens the personal menu, then Start
and PAUSE MATCH FOR EVERYONE; another player presses Start, then the player who paused picks CONTINUE; players 2, 3, 4 ... in turn),
`personal` (personal menus opened and closed by every player in turn, some overlapping), `pause_away` (a shared pause with one tab
hidden for 8 s inside it), `away` (a tab hidden 12 s), `stall` (a player's network held 6 s, then delivered), `refresh` (the last
player reloads its tab mid-match), `leave` (it closes the tab), `mix` (pause, away, stall and a leave in one match).

| lobbies | Chromium scenarios run | trials | pass | inconclusive (overload) | fail | desync |
|---|---|---|---|---|---|---|
| 3 | baseline, pause, personal, pause_away, away, stall, refresh, leave (3 reps each) | 24 | 24 | 0 | 0 | 0 |
| 4 | the same (2 reps each) | 16 | 16 | 0 | 0 | 0 |
| 5 | the same (2 reps each) | 16 | 16 | 0 | 0 | 0 |
| 8 and 2 at once | baseline, pause, mix (2 reps each) | 12 | 11 | 0 | 1 | 0 |
| 5 and 5 at once | baseline, pause, mix (2 reps each) | 12 | 10 | 2 | 0 | 0 |
| 3, 4 with Firefox as the last player | baseline, pause, away (2 reps) | 12 | 3 | 9 | 0 | 0 |
| 3, 4 with jump, fast and turbo on | baseline, pause (2 reps) | 8 | 5 | 3 | 0 | 0 |

(Each lobby of a simultaneous pair counts as one trial.) The per-scenario table and every trial are in
`trials.md`. Live: `live_n2_results.md`, N=2 on https://playwotm.com, 21 rows all pass (see below).

## Findings

- **#35 does not reproduce.** The 3-player pause matrix passed 3 of 3 (shared pause by players 2, 3 and 1, resumed by another player each
  time), the personal menu 3 of 3, a pause with a tab hidden 3 of 3, with 4 and 5 players the same, and 8+2 and 5+5 with pauses by
  players 2 to 5: no hash differs and the server logs no DESYNC in any of them. What the matrix did not test is #35's original
  conditions: native Windows players mixed in, a live server and real network delay (all tabs share one Mac, ping 6 ms), and the
  build the game night ran. The pause there happened at frame 511 with ping 60 ms; a held network (`stall`, 6 s) and Away ran in
  the same lobbies without a difference.
- Another player's Start does not resume a shared pause; the player who paused has to choose CONTINUE. This is how the game works, not
  a bug. Two harness bugs came from it (see below).
- **One unexplained stall, 8+2 `pause` rep 2, the 8-player lobby** (stage 16, seed 691003750): all eight tabs stayed in the shared
  pause (`[dialog] 0 opened`, never closed; the frame counter kept running at the pause loop's 60 per second) and no win came in 360 s.
  The 41 frames compared were equal on all 8 tabs, so it is not a desync, and the other 11 pause trials resumed. The harness has no
  signal for "the fight runs again", so a lost key press (the CONTINUE tap at a load of 19, 8 tabs) is the likely cause, but it is not
  proven. Not filed as a bug; the trial is the thing to repeat on a quiet machine.
- Firefox as the last player and the modifiers at N=3 are mostly inconclusive: match starts timed out or tabs froze (watchdog `FROZE`
  at g_frame 1, waits of 37 to 200 s) at load 15 to 38. The trials that ran under a load below the core count passed (Firefox N=3
  baseline 2 of 2, mods N=4 4 of 4), with no state difference anywhere. Repeat both on a quiet machine before relying on them.
- Not covered: created maps and skins (the desync harness does not build them; `run.mjs --checks creations` covers N=2 and 4 and
  passed in the 2026-11-15 pass), a real GPU, more than one machine, native players, and the hidden tab is emulated (`document.hidden`
  forced; headless Chromium keeps a background tab visible), as in the 2026-11-15 pass.

## Load and the machine

Concurrent lobbies (8+2 and 5+5, ten tabs) ran at 28 to 30 fps for the large lobby and 54 to 57 for the pair at load 15 to 25, so two lobbies
on one server do not disturb each other: sync equal in every pair. The cases that failed to start or froze all had host load above 15
(up to 65); a trial that shows no state difference and a problem at load over twice the cores is recorded as inconclusive, not as a
failure. A frame rate under the full speed with a high load is the machine: all tabs run at the slowest tab's pace.

Environment problems met on the way (not game bugs, listed so the next run avoids them): the Mac's display sleep froze tabs at
"opening the window" (run under `caffeinate -dis`); other workers' `pkill` on `wotm_server` and `serve_web.py` took down the local server in
the middle of runs (the harness copies were renamed for the later runs); `run.mjs`/`desync.mjs` at a newer tree than the `web_dist` they
serve do not start a match (the front panel changed), so run the harness from the tree the dist was built from.

## Live pass (playwotm.com, build wl-3e9ee2e7bacf48de)

N=2, `match,round` sessions, load average 12.9 to 15: all 21 rows pass: lobby, errors, ping, chat, list, leave, invite link, picks,
`match_start` (the lobby worker saw it time out at load 58: the machine, not the site), `inputs`, `sync` (9 frames equal), `health`, `perf`
(55.8 fps of 60 at load 12.9), late join, leave, round end, win screen, rematch (equal), quit, rejoin. The site's data needs no password
now. The live N=3 and above were not run: the live server is being capped at 4 lobbies and the local matrix covers them.

## The `inputs` flake (live N=2, seen once by the browser worker)

Local: 8 of 8 runs pass (N=2 five times, N=3 three times, load 10 to 14), and the live run above passed. Not reproduced. The check holds
one key per player for 10 s and reads each tab's `WOTM_PADLOG`, which logs only the first 480 frames (`--padlog-frames`): on a slow start
the held key can fall outside that window, so a harness-timing cause is likelier than a dropped input; unproven.

## Harness bugs found

- `pause_away` first left the match in the shared pause (the pauser did not pick CONTINUE; the hash lines still advanced because the
  pause loop counts frames), so every trial timed out with equal state: fixed (`resume` in `lib/desync.mjs`).
- A watchdog freeze at a load over twice the cores counted as a crash: now `OVERLOAD`.
