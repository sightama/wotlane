# Time sync (frame-advantage stretch): not shipped

Question: the 2026-11-23 online audit counted 30 to 75 lockstep waits per 5 s on both players of live matches running at 55-58 game
frames per second (Chrome with Firefox, `H24X`), and proposed Task 8 of `docs/superpowers/plans/2026-11-17-online-seamless.md`: the
server tells each player how many frames it leads the slowest one, and a leading client stretches its frames by
`min(10%, lead / 30 frames)` instead of hitting `netWaitFrame`.

Answer: it does not remove the waits, and the variant that does cut some of them slows the match. Decision (Lane, 2026-11-24): not
shipped. The code stays on branch `sightama/time-sync-frame-stretch` for reference (server `proto::Ahead` at 10 Hz,
`native/net/wotm_net_timesync.hpp`, the client's `netStretchPace` after the fixed-step vblank wait).

## What the branch had

- `bfa39de5` (Gemini Neon worker): the server estimates each player's frame as `nextMissing - delay + rtt / 2 / frame` once the
  match has warmed up, sends the lead over the slowest player; the client smooths it (0.25 s), applies a 0.5 frame deadband and
  stretches up to 10% with a deadline pacer that rides over the vblank grid.
- A bug in it: every `netWaitFrame` wait cleared the server's value and the smoothed lead. A leader that waits every few frames (the
  case time sync is for) restarted from 0 each time and never got past the deadband, so it never stretched. Fixed on the branch
  (`1ef15826`, with a test) before the measurements below.

## Why the lead is capped

A leader can only be ahead by the slack the input delay leaves over the path between the players: with delay 4 at 60 Hz (66.7 ms) and
a 50 ms path (round trips 40 and 60 ms), about 1 frame. Past that it waits. So the lead the server reports never grows past about 1 to
1.5 frames, and `lead / 30` never gets past 3 to 5% of stretch, while a peer running at 55 fps needs the leader 7.5% slower. The
proportional stretch stays below the gap, and the waits stay.

An integral term (the stretch keeps growing while the lead stays above a target) does reach the gap, but the lead it acts on is noisy
(input packets arrive in bursts, round trips jitter), so it overshoots: the slow peer starts waiting for the leader, and every wait of
the slowest device is game time lost for everyone. With jitter and no rate gap, there is nothing to correct and the stretch only adds
waits.

Either way the leader has to give up the slow peer's rate gap, as waits or as stretch. On a 60 Hz display both show as about the same
repeated frames, and the game speed is capped by the slowest device whatever the leader does.

## Simulation

`sh tools/timesync_sim/run.sh` (`--help` for one scenario): two peers on 60 Hz vblank grids, the model of
`wotm_overrides.cpp` and `wotm_sync.inc` (work, vblank wait, stretch, input for frame+1+delay sent in order with latency and jitter,
wait for the other peer's input relayed by the server, grid shifted by every wait), round trips 40 and 60 ms, delay 4. P1 works
7 ms per frame (Chrome-like), P2 13 to 18.3 ms with 4% of frames 12 ms longer. `off`: no time sync; `p`: the Task 8 stretch;
`pi`: the same plus an integral toward a 0.5 frame lead (gain 0.05 per frame of lead per second). Means of 5 seeds:

| scenario | sync | gfps | waits/5 s P1 | waits/5 s P2 | wait share P1 |
|---|---|---|---|---|---|
| P2 13 ms (keeps 60) | off | 59.9 | 2.8 | 0.0 | 0.2% |
| | p | 59.9 | 3.3 | 0.0 | 0.2% |
| | pi | 59.8 | 4.9 | 0.4 | 0.3% |
| P2 16.5 ms (~58 fps) | off | 57.8 | 45.4 | 0.0 | 3.7% |
| | p | 57.8 | 41.6 | 0.1 | 3.6% |
| | pi | 57.1 | 28.9 | 4.8 | 2.6% |
| P2 17.5 ms (~55 fps) | off | 55.3 | 87.1 | 0.0 | 7.9% |
| | p | 55.2 | 82.3 | 0.0 | 7.9% |
| | pi | 54.6 | 32.4 | 3.9 | 3.2% |
| P2 18.3 ms (~53 fps) | off | 53.1 | 113.7 | 0.0 | 11.6% |
| | p | 53.1 | 110.7 | 0.0 | 11.5% |
| | pi | 53.0 | 75.9 | 0.3 | 8.1% |
| P2 13 ms, 30 ms jitter | off | 59.8 | 4.4 | 6.3 | 0.3% |
| | p | 59.5 | 11.2 | 7.5 | 0.8% |
| | pi | 59.1 | 16.0 | 10.5 | 1.3% |
| P2 13 ms, 40 ms jitter | off | 59.0 | 23.0 | 22.1 | 1.7% |
| | p | 58.4 | 28.4 | 19.8 | 2.6% |
| | pi | 57.9 | 30.2 | 19.4 | 3.0% |

The rate-gap rows match the live pattern (a fast leader waiting 45 to 115 times per 5 s while the match runs at 53-58 fps). The
proportional stretch changes almost nothing; PI cuts the leader's waits but costs 0.1 to 1.1 gfps (slow motion for everyone) and turns
the slow peer into a waiter; with jitter both make it worse. A sweep of the PI target (0.5, 0.75, 1 frame) and gain (0.02 to 0.15)
only moves along the same trade: fewer leader waits for a slower match.

## Real browsers

Chrome with Firefox through `tools/browser_mp_test/latency.mjs --players 2 --browser chrome --firefox 1 --rtt 40,60 --jitter 15`,
the branch's build against `82d12d81`, read with `python3 tools/diag_waits.py <run dir>`. The Mac was at load average 66 (other
workers' browser suites): the base run sank from 50 to 11 gfps during the match and has no steady window, and the time sync run had
its own 66 and 96 waits per 5 s at 58 gfps. No comparison could be made; the A/B was stopped for a quiet window and not resumed once
the simulation answered the question.

## What reduces the waits

The waits are the leader paying for the slowest device's rate and for jitter larger than the delay's slack. The levers are that
device's cost per frame (Firefox's game thread and WebGL, audit finding 7a) and the delay and jitter margin the server advises
(`proto::adviseDelay`, `RttWindow`).
