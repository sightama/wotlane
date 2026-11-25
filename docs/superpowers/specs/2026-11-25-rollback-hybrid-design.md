# Rollback hybrid: delay floor plus a per-device rollback window (issue #30)

Lane reopened #30 on 2026-11-25 ("make this game BEAUTIFUL AND AWESOME"). The input delay is 74% of the live press-to-screen latency
(`docs/investigacoes/2026-11-25/input-latency-breakdown.md`). This spec turns the synctest (`docs/investigacoes/2026-11-18/rollback-synctest/`)
into netcode: a player's input is applied sooner, and the other PCs predict it and correct late inputs by restore and redo.

## What the numbers allow

A rollback of k frames at a frame boundary costs restore + k redone frames, on top of the frame's own work and a save. From the synctest
(1 vs 1, 60 Hz, M1 Max, ms): Chrome played 5.8, redo 5.5, save 1.4, restore 1.4; Firefox 11.1 / 10.7 / 3.1 / 3.0 (before the VU0
rounding change, which took 10 to 14% off a frame); native 3.0 / 2.5 / 1.4 / 1.0. So a fast Chrome or a native build can afford one or
two frames, Firefox none. The window is therefore per device and measured, and a device that can afford none still gains when its
opponent can (below).

## Decisions

1. **The window is the receiver's, the delay cut is the sender's.** Player X's input applied k frames sooner reaches every other PC k
   frames late; those PCs pay the redo. So each PC measures its own **budget R** (how many frames it can run past its newest confirmed
   input) and tells the server; the server advises player X the delay `max(1, path_X - min over the others j of R_j)`. A Firefox player
   (R = 0) facing a fast Chrome (R = 1) gets one frame less delay; the Chrome player keeps its delay. A PC that declares nothing
   (rollback off, or an older build) counts as R = 0, so nobody's delay is cut against it: today's lockstep, unchanged.
2. **Wire: one additive client-to-server message, `Window {frames}`.** The server already ignores unknown match messages, and it sends
   nothing new (the cut travels in the existing `Pace`), so no protocol bump and no wire level: the level only decides what the server
   may send a member (`docs/ONLINE.md` "Wire versions").
3. **Floor 1 frame.** The cut never takes a delay under 1 (a sample is always applied at least one frame later). The budget, not the
   floor, bounds how far behind the truth a PC runs.
4. **Budget R from measured costs, steady state.** `R = max w <= cap with played + save + restore + w x redo <= 0.9 x frame`. Steady
   state (a rollback every frame) because a remote player moving a stick or the mouse makes nearly every prediction wrong. Costs are
   medians of 300-frame windows on the game thread; before the first real rollback, save and restore come from a no-op save plus
   restore at the fight start and the redo is taken as a played frame. R drops at once when a window is over budget and rises only
   after two windows with room. The cap is the setting's value (1 to 3).
5. **Prediction: repeat the player's newest confirmed input.** Mouse motion included (a moving mouse keeps moving).
6. **Correct at the top of the boundary, draw the corrected frame.** At rtMain's frame boundary, before the drawing capture, the PC
   compares the predictions with the inputs that arrived; on a wrong one it restores the slot at the start of that frame and redoes up to
   the present (no capture, no sound output, no pacing, no sampling, the redo-skip functions), then captures the corrected frame. A frame
   that runs long with a rollback is absorbed by the next one's slack (the pacing waits for the next vsync on the grid, it does not
   lose one), which is why the steady-state rule in 4 has a margin rather than a worst case.
7. **Save only when a rollback could need it:** at a boundary where the next frame runs on a prediction or the frame just played is not
   final. With every input on time a frame costs nothing extra.
8. **The desync hash covers final frames only.** A frame is final once every input up to it has arrived and its predictions were
   checked. The 8 blocks of a frame are hashed from the live RAM when it is final at its own boundary, else from its slot when it becomes
   final. The server compares the same frames as today.
9. **Dialog loops run in lockstep.** A dialog frame (pause, round end) waits for every input up to it. If a frame before the dialog ran
   on a wrong prediction, the PC restores the rtMain slot from inside the dialog loop: the hook returns with the restored pc and the
   recompiled callers unwind (the runtime's resume by pc). The synctest proves this path before the hybrid relies on it
   (`WOTM_SYNCTEST` restores from the first dialog frame). Stage loads stay barriers.
10. **Behind a setting, off by default:** `WOTM_ROLLBACK=<cap>` (match environment; the page passes it through). Not in the config
    hash: PCs with and without it play together (decision 1). Default-on is proposed only with the numbers of "Done when".
11. **Redone frames get cheaper only by work proven not to change state:** every skip keeps the synctest at 0 mismatches. The host-side
    capture hooks that still run in a redo (fonts, flares, letterbox, HUD) and the drawing lists they fill are skipped or reset in a
    redo; guest functions are skipped only with a 0-mismatch synctest.

## Done when

- `WOTM_SYNCTEST` 0 mismatches (1 vs 1 and 8 monsters, plus restores from dialog frames).
- The desync matrix (2/3/4 players x baseline/pause/stall) 0 desyncs with rollback on, including mixed on/off and Chrome x Firefox.
- Live latency before and after (`latency.mjs`), the redo cost and the chosen R per browser (Chrome, Firefox, a slow game thread via
  `WOTM_FRAMESPIN`, since CDP throttling does not reach the game thread), and no frame-rate loss on Firefox or the slow case.

## Not done here

- 3 to 8 players get the same mechanism, but at 30 Hz and with many remote players mispredictions are constant; their budgets will
  mostly be 0 outside native and fast Chrome.
- No frame-advantage time sync: the delay cut is advised per player by the server, as the delay is today.
