# Where the online input latency goes (2026-11-25, after #210)

The coordinator asked, after the present-phase deploy (#210, build id wl-7e3c4e8e1dc25fd4), to break the ~131 ms press-to-screen latency
of a live 1 vs 1 into its stages, to pick what to attack next. Short answer, from a small live sample (4 presses) and local runs with ~70
presses each that agree on the order: **the lockstep input delay is the largest stage by far** (live median 96.3 against a 131 ms median
total, 74%; local ~50 of 78 to 87 ms, 57 to 64%); everything the page and the game thread do is the rest. The delay is set by the players'
ping level, and the live match's 5 frames (3 at its median pings) were most likely set by ping spikes (inferred, below).

## The stages (what the diag measures, `docs/ONLINE.md` "Diagnostics")

A press is followed from the moment the page saw it (`keydown` time) to the first animation frame after the draw that includes the game
frame applying it:

| stage | diag field | what it is |
|---|---|---|
| sample | `lat_in_ms` | page saw the key until the game sampled it (the next game frame's input sample) |
| delay | `lat_dl_ms` | the sample until the game frame that applies it starts: the **input delay** the server picks (frames) |
| waits | `lat_st_ms` | waits for the other players' input in between (a late packet) |
| sim | `lat_sim_ms` | that frame's start until its capture for drawing |
| present | `lat_pr_ms` | the capture until the animation frame after the draw that first includes it |
| (not seen) | none | compositor and display scan-out after that animation frame: a page cannot measure it |

Sending the input to the server and the server relaying it are not a stage of their own: by design they happen inside the delay, which
is chosen so the input reaches every other player before the frame that applies it (half the sender's ping plus half the slowest peer's,
at the level the ping stays at or under 4/5 of the last 8 s, plus 3 ms, rounded up to whole frames: `docs/ONLINE.md` "Input delay",
`pathFrames` and `kDelayMarginMs` in `native/net/wotm_net_proto.hpp`). A packet that misses that shows up as waits.

## Live: playwotm.com, lobby J7XR (match log 20261125-035006-J7XR, the #210 deploy's second smoke)

Two tabs on this Mac (Chrome 158 headless and Firefox 157, from the smoke's log; `tools/browser_mp_test/run.mjs --target live`) against
the live server (VM zone us-east1-b), 60 Hz. From
`match_diag.sh`, per player: ping medians 38 and 53 ms, maxima 97 and 108; game fps ~57 for both; input delay 2 rising to 5 frames as
the server measured the round trip; about 9% of the time waiting.

| stage | median ms | share of the stage medians' sum |
|---|---|---|
| sample | 7.0 | 5% |
| delay | 96.3 | 70% |
| waits | 11.5 | 8% |
| sim | 0.4 | 0% |
| present | 21.4 | 16% |
| sum of the stage medians | 136.6 | |
| **median of the per-press totals (4 presses)** | **131.0** | |

Each stage is a median over the presses, so the stages need not add up to the median total. `match_diag.sh J7XR --all`. Only one diag
window had presses (the smoke's scripted taps), all from player 1, the headless Chrome tab, so this is a small sample from one browser.

Why 5 frames: with the median pings (38 and 53 ms) the rule gives (38 + 53) / 2 + 3 = 48.5 ms, 2.9 frames, so 3. Rising to 5 frames
needs a ping level averaging over ~64 ms per player (path over 4 frames), and once there the delay holds at 5 while the fall level
averages over ~60 ms (path plus the 0.2 frame margin over 4 frames). Both are above the medians and below the maxima (97 and 108 ms, which
may span the whole match rather than this window); the level is the 4/5 point of the last 8 s, so spikes that come often enough raise
it. That spikes set the 5 is inferred from these numbers; the per-sample pings, and so the level itself, were not looked at.

The window reported that delay of 5 frames, 83 ms at 60 Hz, yet `lat_dl_ms` is 96.3 ms (5.8 frames). `lat_dl_ms` leaves out the
waits, so the ~57 game fps (which includes them) does not explain the ~13 ms gap. It is not explained here: the delay may have changed
within the window (the diag reports its value at the window's end), and 4 presses is a small sample. The live sim (0.4 ms)
is also far below the local runs' 5.5 ms; with one headless tab and 4 presses this is not looked into further here.

## Local: `latency.mjs`, 1 vs 1, round trip 20 ms (2026-11-24/25, present phase on, headed Chrome, 120 Hz display)

| stage | median ms (4 tabs over 2 runs, 68 to 70 presses per tab) |
|---|---|
| sample | 8.1 to 10.6 |
| delay | 49.3 to 50.1 |
| waits | 0.0 to 1.0 |
| sim | 5.5 to 5.8 |
| present | 13.1 to 20.4 |
| total | 78 to 87 |

At a 20 ms round trip the delay is still ~60% of the total: these runs settled at 3 frames (50 ms). The pings the game measured were
13 to 40 ms against the proxy's 20 ms setting, and the rule reaches 3 frames once the two players' level averages over ~30 ms: at the
players' highest pings in one run (34 and 40 ms), (34 + 40) / 2 + 3 = 40 ms is 2.4 frames, so 3.

## What this says to attack next

1. **The input delay (70% of the live stage sum).** Each frame of delay is 16.7 ms for every press. It is set by the ping level, so
   the levers are:
   - *Ping spikes*: live, the spikes rather than the median pings most likely set 5 frames instead of 3 (inferred, above). Where
     they come from (the path, the server, or this Mac running two browsers and the harness) is not measured; the server's journal and
     `match_diag.sh` per-report pings over a few real matches would tell.
   - *Server placement*: the round trip to us-east1 sets the delay; players far from it pay more frames. A second region, or picking
     the region by the lobby's players, cuts frames for them.
   - *The delay rule*: #199 already tightened the margin before the delay falls (0.5 to 0.2 frames); the remaining slack is the 3 ms
     margin (~0.2 frames), the rounding to whole frames (under one frame) and the 4/5 level, which decides how much of the spikes counts
     (up to 2 frames in the live case). Lowering any of them is paid for in more waits.
   - Rollback and online time sync are under "Discarded" in `TODO.md` (the owner's decision), so they are not levers here.
2. **The present (16% live, 21.4 ms).** This smoke's Chrome tab was headless, whose animation frames are presumably not tied to a
   display's refresh (inferred, not checked), so its present is likely not the #210 case. With a real 60 Hz display the model after
   #210 is ~18.7 ms (2 ms margin + one display frame); before it, ~25 ms on average (half a display frame of waiting for the next one,
   plus one display frame). Neither is measured at 60 Hz yet. The display frame in both is there by construction:
   the draw happens on an animation frame and shows on the next. The same model gives ~10.3 ms at 120 Hz, below the 13.1 to 20.4 ms
   the local 120 Hz runs measured, so the model is not confirmed by these runs either.
3. **Waits (8%).** Late packets: jitter on the path, and on this Mac two browsers plus the harness, which may inflate it compared with
   players on their own machines (not measured).
4. **Sample and sim (5%).** Already small: the sample waits for the next game frame (half a frame on average) and the frame is captured
   right after it is computed.

So the largest remaining chunk is the input delay. Within lockstep, the next thing to look at is what raises the ping level above the
median (the spikes); server placement helps players far from us-east1; the margin and the rounding are worth under a frame each, and
the page stages (present the largest, 1.3 frames live) about a frame each.
