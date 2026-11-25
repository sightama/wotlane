# Where the online input latency goes (2026-11-25, after #210)

The coordinator asked, after the present-phase deploy (#210, wl-7e3c4e8e1dc25fd4), to break the ~131 ms press-to-screen latency of a live
1 vs 1 into its stages, to pick what to attack next. Short answer: **the lockstep input delay is about three quarters of it**; everything
the page and the game thread do is the rest.

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

Sending the input to the server and the server relaying it are not a stage of their own: they happen inside the delay. The delay is
chosen so the input reaches every other player before the frame that applies it (half the sender's ping plus half the slowest peer's,
at the level the ping stays at or under 4/5 of the last 8 s, `docs/ONLINE.md` "Input delay"). A packet that misses that shows up as waits.

## Live: playwotm.com, lobby J7XR (2026-11-25 03:50Z, the deploy's second smoke)

Two tabs on this Mac (Chrome 158 headless and Firefox 157) against the live server in us-east1, 60 Hz. Pings 27 to 97 ms (median 38
and 53), input delay 2 rising to 5 frames as the server measured the round trip, game fps ~57, 9% of the time waiting.

| stage | ms | share |
|---|---|---|
| sample | 7.0 | 5% |
| delay | 96.3 | 74% |
| waits | 11.5 | 9% |
| sim | 0.4 | 0% |
| present | 21.4 | 16% |
| **total (median of 4 presses)** | **131.0** | |

`match_diag.sh J7XR --all`. Only one diag window had presses (the smoke's scripted taps), so this is a small sample; the local runs
below have ~70 presses each and the same shape.

## Local: `latency.mjs`, 1 vs 1, round trip 20 ms (2026-11-24/25, present phase on, headed Chrome, 120 Hz display)

| stage | ms (4 tabs over 2 runs) |
|---|---|
| sample | 8.1 to 10.6 |
| delay | 49.3 to 50.1 |
| waits | 0.0 to 1.0 |
| sim | 5.5 to 5.8 |
| present | 13.1 to 20.4 |
| total | 78 to 87 |

At a 20 ms round trip the delay is still ~60% of the total: 3 frames at 60 Hz (50 ms) is the floor the delay rule reaches with a small
ping, because half of each player's ping plus a margin rounds up to whole frames.

## What this says to attack next

1. **The input delay (74% live).** Each frame of delay is 16.7 ms for every press. It is set by the round trip, so the levers are:
   - *Rollback* (issue #30, synctest GO on correctness, paused): predicts the remote input and corrects, so local input applies at the
     next frame instead of after the delay. This is the only change that removes most of the 96 ms; its cost is re-simulating frames
     (the frame-cost work, #193, made that cheaper).
   - *Server placement*: the round trip to us-east1 sets the delay; players far from it pay more frames. A second region, or picking
     the region by the lobby's players, cuts frames for them.
   - *The delay rule's margin*: #199 already lowered how slowly the delay falls; the rounding to whole frames and the 4/5 level are the
     remaining slack, each worth at most a frame and paid for in more waits.
2. **The present (16% live, 21.4 ms).** This smoke's Chrome tab was headless (animation frames not tied to a display), so its present
   is not the #210 case. With a real 60 Hz display the model after #210 is ~18.7 ms (2 ms margin + one display frame); before it,
   ~25 ms on average. The rest is one display frame by construction (the draw happens on an animation frame and shows on the next).
3. **Waits (9%).** Late packets: jitter on the path, and on this Mac two browsers plus the harness. Real players on their own machines
   should wait less; the Firefox quit fix (#204) and the boot-stall fix (#201) already removed the worst ones.
4. **Sample and sim (5%).** Already small: the sample waits for the next game frame (half a frame on average) and the frame is captured
   right after it is computed.

So the next big step is rollback; everything else left is worth a frame or less each.
