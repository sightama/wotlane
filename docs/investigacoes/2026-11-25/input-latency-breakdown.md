# Where the online input latency goes (2026-11-25, after #210)

The coordinator asked, after the present-phase deploy (#210, build id wl-7e3c4e8e1dc25fd4), to break the ~131 ms press-to-screen latency
of a live 1 vs 1 into its stages, to pick what to attack next. Short answer, from a small live sample (4 presses) and local runs with ~70
presses each that agree: **the lockstep input delay is the largest stage by far, about 60 to 70% of the total**; everything the page and
the game thread do is the rest.

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
at the level the ping stays at or under 4/5 of the last 8 s, `docs/ONLINE.md` "Input delay"). A packet that misses that shows up as waits.

## Live: playwotm.com, lobby J7XR (2026-11-25 03:50Z, the deploy's second smoke)

Two tabs on this Mac (Chrome 158 headless and Firefox 157) against the live server in us-east1, 60 Hz. Pings 27 to 97 ms (median 38
and 53), input delay 2 rising to 5 frames as the server measured the round trip, game fps ~57, 9% of the time waiting.

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

The window reported an input delay of 5 frames, 83 ms at 60 Hz, yet `lat_dl_ms` is 96.3 ms (5.8 frames). The game ran at ~57 frames a
second in that window (5 frames of ~17.5 ms are ~88 ms), which explains part of it; the rest is not explained by these numbers (the delay
may have changed within the window: the diag reports its value at the window's end). The live sim (0.4 ms) is also far below the local
runs' 5.5 ms; with one headless tab and 4 presses this is not looked into further here.

## Local: `latency.mjs`, 1 vs 1, round trip 20 ms (2026-11-24/25, present phase on, headed Chrome, 120 Hz display)

| stage | median ms (4 tabs over 2 runs, 68 to 70 presses per tab) |
|---|---|
| sample | 8.1 to 10.6 |
| delay | 49.3 to 50.1 |
| waits | 0.0 to 1.0 |
| sim | 5.5 to 5.8 |
| present | 13.1 to 20.4 |
| total | 78 to 87 |

At a 20 ms round trip the delay is still ~60% of the total: these runs settled at 3 frames (50 ms). Why 3 and not the 1 to 2 that half of
each player's round trip would suggest was not measured here (the lag proxy adds its own time, and the rule rounds to whole frames).

## What this says to attack next

1. **The input delay (70% of the live stage sum).** Each frame of delay is 16.7 ms for every press. It is set by the round trip, so the levers are:
   - *Rollback* (issue #30, paused): predicts the remote input and corrects mispredictions, so local input can apply after a small delay
     instead of one covering the round trip. It is the largest candidate lever on this stage (by how lockstep works, not measured here);
     its cost is re-simulating frames.
   - *Server placement*: the round trip to us-east1 sets the delay; players far from it pay more frames. A second region, or picking
     the region by the lobby's players, cuts frames for them.
   - *The delay rule's margin*: #199 already tightened the margin before the delay falls (0.5 to 0.2 frames); the rounding to whole
     frames and the 4/5 level are the remaining slack, each worth at most a frame and paid for in more waits.
2. **The present (16% live, 21.4 ms).** This smoke's Chrome tab was headless (animation frames not tied to a display), so its present
   is not the #210 case. With a real 60 Hz display the model after #210 is ~18.7 ms (2 ms margin + one display frame); before it,
   ~25 ms on average (half a display frame of waiting for the next one, plus one display frame). Neither is measured at 60 Hz yet.
   The display frame in both is there by construction: the draw happens on an animation frame and shows on the next.
3. **Waits (8%).** Late packets: jitter on the path, and on this Mac two browsers plus the harness, which may inflate it compared with
   players on their own machines (not measured).
4. **Sample and sim (5%).** Already small: the sample waits for the next game frame (half a frame on average) and the frame is captured
   right after it is computed.

So the largest remaining chunk is the input delay, and the candidate that could remove much of it is rollback (unmeasured; it trades
delay for re-simulation and visible corrections); the other levers are worth about a frame or less each.
