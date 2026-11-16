# Browser multiplayer test, 1 to 8 players (2026-11-15)

Tool: `tools/browser_mp_test` (README there). Browser: Playwright Chromium 156 headless (software GL), one browser, one context per
player (own OPFS). Host: the Mac with 10 cores, load average 12 to 34 during the runs (other workers were using it).

## Runs

| run | target | build | players | results |
|---|---|---|---|---|
| local pass | local `wotm_server` + `serve_web.py` | wl-82c5b21887ec9e7f | 1 to 8, every session | `local_n1-8_results.md` |
| local stall rerun | same | same | 5 and 8, stall only | `local_stall_n5-8_results.md` |
| Firefox mix | same | same | 2 and 4, the last player in Firefox 157.0 (its `health` row failed on a harness bug in the Firefox page, since fixed, not rerun) | `local_firefox_mix_results.md` |
| live pass | https://35-185-58-174.sslip.io/ | wl-82c5b21887ec9e7f, after the #76/#78/#79 fixes | 1, 2, 4 | `live_n1-2-4_results.md` |
| live rerun | live | same | 2, round only | `live_round_rerun_results.md` |

Evidence (tab logs, server log slice per lobby, screenshots; about 110 MB) is outside the repo in
`~/wotlane-evidence/browser-mp-2026-11-15/<run>/n<N>/<session>/` (`full3` is the local pass, `live1` and `live2` the live ones,
`full2` the same pass on the build before it, `ff1`, `stall2`, and `video4`: a local N=4 match with one webm recording per tab, in the order the
tabs were opened, P1 to P4 plus the spare). The paths in each `results.md` are relative to the run folder.

## Result by player count (local pass, wl-82c5b21887ec9e7f)

Every row passes for N = 1 to 8 except:

| row | N | cause |
|---|---|---|
| `refusal_message_stays` | all | #76: a refused join's reason is replaced by "Lost the connection to the game server" (fixed on the live site afterwards: passes in the live runs) |
| `rejoin_after_match` | 2 to 8 | #78: "Your last lobby was X: Join puts you back in it" for a lobby the server has already closed (fixed on the live site: passes) |
| `stall_recovery`, `connection_lost` | 8 | #81: the stalled tab never resumes at 8 players; 2, 4 and 5 recover |

At N=8 the front panel's Start button is out of reach in a 400 px high window (#79, fixed on the live site); the harness gives the lobby a
tall window. The ninth join is refused with "lobby is full" and the other eight tabs are untouched (`cap_refused`, pass).

What each row proves, per player count: the lobby lists every member on every tab; monster and stage changes reach every tab; every tab
starts as player i of N; each player's own held button shows up in every tab's pad log (`WOTM_PADLOG`: input from every player
registers everywhere); the state hashes (`[net] hash of frame N`, all 32 MB of guest RAM) are equal on every shared frame and the
server logs no DESYNC; no crash, stop or page error; a late join is refused with a clear message; a closed tab and a reloaded tab
mid-match leave the others playing; wins=1 with the `vencer` cheat ends the round on every tab, opens the win dialog, PLAY AGAIN plays
round 2 in sync, QUIT returns every tab to the front panel; a frozen player is named in the others' wait text, with "(their game tab is
in the background)" while its tab is hidden, the prompt offers Keep waiting or Leave after 60 s, and a dropped connection shows the way
back (N=2, 4, 8; N=5 in the rerun).

Created map and skin: imported in every tab, picked in the lobby and played at N=2 and N=4, same state; at N=2 the tab without them is
refused with "game configuration differs" and the page explains it.

## Frame rate and CPU (local pass, one machine, per match)

The game runs at 60 Hz for 1 and 2 players and in its own 30 Hz mode from 3 (the server's step 2), so full speed is 60 and 30. All tabs of a
lockstep match run at the slowest tab's pace, so one fps number per match.

| N | game fps (full speed) | browser CPU (cores of 10) | host load avg | network waits, worst tab |
|---|---|---|---|---|
| 1 | 60.0 (60) | 8.2 | 13 | 0 ms |
| 2 | 58.4 (60) | 8.9 | 12 | 3.3 s |
| 3 | 29.7 (30) | 6.3 | 34 | 1.3 s |
| 4 | 28.9 (30) | 5.8 | 33 | 0.3 s |
| 5 | 29.3 (30) | 9.4 | 17 | 0.7 s |
| 6 | 27.9 (30) | 8.5 | 17 | 6.4 s |
| 7 | 27.5 (30) | 8.7 | 17 | 6.2 s |
| 8 | 24.3 (30) | 7.9 | 25 | 14.8 s |

Each software-rendered tab costs about 0.7 of a core in its renderer plus the shared GPU process (SwiftShader), and the other workers on
this Mac kept the load average above 12. N=8 at 24 fps with 15 s of network waits is the machine, not the game: the CPU was saturated, the
state stayed equal and nothing stopped. The live runs (N=2: 42 fps, N=4: 27 fps) are lower than the local ones for the same reason (load 15 to 23).
A GPU run (`--browser chrome`) was not made.

## Bugs

- #76 refused-join reason overwritten (fixed);
- #78 "Join puts you back in it" for a closed lobby (fixed);
- #79 lobby card not scrollable, Start out of reach at 8 members in a short window (fixed);
- #81 8 players: a tab stalled about a minute never resumes (open; the CPU saturation may be part of it);
- #82 `PS2X_PAD` never fires in the browser build (`GetTime()` is wall-clock time there; open, low priority).

One unexplained flake: in the first live run the post-match Join at N=2 did not settle within 15 s (no message); the rerun and N=4
showed "no such lobby (check the code)" as expected.

## Not tested

- Safari and Firefox on their own (Firefox only as the last player of a Chromium lobby, N=2 and 4, locally); Edge is not installed here.
- A real GPU, or one player per machine: all tabs share this Mac.
- The 3-player pause desync of #35 (the test never pauses).
- Players 5 to 8 on a built-in map other than stage 1, modifiers (jump, fast, turbo) in a match, and created maps or skins above N=4.
- More than 8 players: not supported by design; the ninth is refused.
- A network outage proper: the stall is simulated by holding the page's WebSocket (`freezeNetwork`), the Away note by forcing
  `document.hidden` (headless Chromium keeps a background tab visible).
- Mac and Windows native players in a browser lobby (the Mac worker's cross-play tests).
