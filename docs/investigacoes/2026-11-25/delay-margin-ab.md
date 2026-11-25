# The input delay's 3 ms margin: A/B against none (2026-11-25)

After `ping-spikes.md` (same folder), the coordinator asked whether the delay rule's own slack is worth taking out. The rule takes half
the player's round trip plus half the slowest other player's, adds `kDelayMarginMs` (3 ms) and rounds up to whole frames, at the 4/5
level of the last 8 s. It falls only once the path leaves 0.2 frames to spare (`kDelayFallMarginFrames`, `native/net/wotm_net_proto.hpp`).
The margin only changes the delay when the path falls within 3 ms of a frame boundary, so the A/B sits on one. Short answer: **keep the
3 ms.** Without it the delay is up to a frame shorter, but the match spends 2.5 times as much of its time waiting and the game runs about
2 frames a second slower.

## Setup

- `tools/browser_mp_test/latency.mjs --players 2 --rtt 57 --jitter 10 --secs 90`, two headless Chromium tabs on this Mac. The game
  measured pings of 49 to 70 ms, which puts the rise level at roughly 65 ms (inferred from that range, not read off the server): at
  60 Hz the 4-frame boundary is a path of 66.7 ms, crossed at a level of 63.7 ms with the margin and 66.7 ms without, so 5 frames with
  the margin and 4 without. At `--rtt 52` the level would be about 60 ms, 4 frames for both (computed, not run).
- Two `wotm_server` builds of wip/browser 1aad89ef (the server deployed 2026-11-25 05:49Z): `kDelayMarginMs` 3.0 (as shipped) and 0.0.
- Three runs of each, alternating. Each run started once this Mac's 1-minute load was under 4; sampled every 5 s, it peaked at 9.2
  to 10.8 during the runs.
  The stages (`lat_dl_ms`, `lat_st_ms`) are latency.mjs's per-player summary (the diag windows' medians averaged by presses); the
  wait share and game rate come from `python3 tools/diag_waits.py <run dir>`, over the steady windows.

## Result (6 player rows per variant)

| | margin 3 ms | margin 0 ms |
|---|---|---|
| delay (frames) | 4 and 5 | 4 |
| delay stage `lat_dl_ms` | 74.8 to 80.8 (mean 78.3) | 59.5 to 61.9 (mean 61.1) |
| waits stage `lat_st_ms` | 13.0 to 20.4 (mean 16.5) | 24.9 to 30.6 (mean 26.6) |
| share of the time waiting | 4.4 to 9.0% (mean 6.4%) | 12.1 to 22.0% (mean 16.2%) |
| game frames a second | 58.0 to 58.6 | 56.0 to 56.4 |

Without the margin, delay plus waits per press is about 7 ms lower (from the stage means). But the match spends two and a half times
as much of its time waiting (the means of the wait shares) and runs at 56 instead of 58 game frames a second, which fits inputs
arriving just after the frame that needs them; for a player that would likely read as stutter and slow motion. With the margin every
player saw both 4 and 5 frames in each run, consistent with a fall needing the 0.2 frames to spare, and its delay stage sits between the
two. The rule's other slack (the 4/5 level and rounding up to whole frames) would trade the same way, delay for waits (inferred, not
tested).

The 6 rows are 3 runs of 2 players on one machine, not 6 independent samples, though the wait shares do not overlap. The tabs were
headless and presented 8.8 to 11.0 frames a second (software GL), so the present stage and the totals are not meaningful here. The game
thread's busy time per frame was the same in both (`ee_busy` 5.2 to 5.4 ms) and the variants alternated, so the gap in game rate is the
waiting rather than the load.
