# Online slow motion (game night 2026-11-17)

Lane's phone vs a desktop, then desktops vs desktops, played in slow motion with broken sound. Reference for how things work now:
`docs/ONLINE.md` ("30 Hz with a phone or a slow computer", "A player who holds the match back is taken out", "Diagnostics") and
`docs/passos/WEB.md` ("Phones"). This file is what was measured and decided; delete it once its open items are in `TODO.md` or done.

## Root cause
An online match runs in fixed step: every game frame is exactly 1/60 s (30 Hz mode: 2/60) of game time, so every PC computes the same
thing. The vblank grid catches up late frames, so game speed = frames the slowest member computes per second / the match's rate, for
everyone (lockstep). Sound is mixed per game frame in fixed step, so it underruns at the same ratio. Offline the game measures real
time and its own variable step drops to 30 updates/s, which is why a slow device looks fine in single player.

Evidence: PEA5 (phone vs Mac): delay 3 at 37 ms ping, ~26 frames/s with the phone, 55 after it left. V4WP (two PCs): 30.5 frames/s
together, 60.2 for Lane alone after the other left. 5QMC (4 PCs, 30 Hz): every page at 16-18 of 30; the one the others waited on
(Brave, own rate ~20-24 of 30, 178 long tasks, 17 ms task lag) set the pace; one hidden tab froze all four for 9 s.

Ruled out (measured): network or the delay controller (desktop vs desktop on the live site: 57 frames/s), the 120 Hz pace guard and
display rate (Firefox forced to 144 Hz keeps 57; logic never waits on drawing, audio or disc reads), main-thread throttling alone
(4x CDP throttle with a phone viewport: 54).

## What shipped (PRs into wip/browser, copies on master)
- 30 Hz for slow devices: phones by the page (`halfRateDevice`, UA-CH mobile, Android/Mobi UA, coarse pointer) and by the server
  (lobby WebSocket User-Agent); computers by a remembered verdict (`web/rate.js`: own rate = game frames over the time not spent
  waiting, median of 5 s windows, under 55/60 of the match's rate; a 30 Hz match only ever marks slow). #115-#118, #122-#123, #125.
- Light drawing for those devices in a match: 60 Hz, 75% render scale, unless the player set them. #120, #125.
- Diagnostics fields: `raf_med_ms`, `focus_pct`, `lag_ms`, `gpu`. #122. `match_diag.sh` flags only the member everyone waits on
  (coord #4).
- Auto-drop (`native/net/wotm_net_holdback.hpp`, #127): pace under 75% of the match's rate for 15 s and one member alone at the
  back in 60% of samples; away over 3 s while holding everyone back; grace 10 s from the first frame played; judged anyway after
  60 s of no movement; trail excused by a longer path capped at 2 frames; never down to one player. First live drop: VKA5,
  StephenMiller at 13.6 of 30, after which the others ran ~24-25 of 30.

## Lessons
- Lockstep numbers: every page's game frames/s is the match's pace; the bottleneck is the one with the least waiting (own rate =
  gfps / (1 - wait share)). A device can't be judged slow from its gfps alone.
- Emulation on the Mac: CDP CPU throttling doesn't reach the game's worker threads; pinning a renderer to the efficiency cores gives
  ~7 frames/s from timer slack. Real slow devices are the only true test; the efficiency-core tab is still a fair "severe" case.
- The first holdback rules passed their unit tests and failed in real browsers twice (trail depth is capped by the input delay;
  real 100+ ms sampling never filled a 15 s window). Run real browsers before trusting thresholds.
- Deploys: one VM deploy at a time, announced before and after; the page JS must ship with the wasm built from the same tree (a
  native change means a rebuild); the site build id is checked per lobby, so a mid-session deploy splits nobody's match, but the
  group must reload together. Several workers handed the same deploy back and forth tonight; name one owner per deploy.
- Firefox reports a generic GPU ("GTX 980") and Brave masks it, so `gpu` only means something in Chrome and Edge.

## Open
- Switch a running match to 30 Hz when a member falls behind (issue #124).
- Why both severe bottlenecks were Brave 157 on Windows (timer precision or fingerprint protections are suspects).
- Game-thread busy time per frame is not in the diagnostics; it would tell whether a page like Jake's (24 of 30, 5.5 ms of drawing)
  is CPU-bound.
