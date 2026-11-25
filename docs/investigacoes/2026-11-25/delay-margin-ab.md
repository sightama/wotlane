# The input delay's 3 ms margin: A/B against none (2026-11-25)

After `ping-spikes.md` (same folder), the coordinator asked whether the delay rule's own slack is worth taking out. The rule is half of
each player's round trip plus `kDelayMarginMs` (3 ms), rounded up to whole frames, at the 4/5 level of the last 8 s
(`native/net/wotm_net_proto.hpp`). The margin only changes the delay when the path falls within 3 ms of a frame boundary, so the A/B sits
on one. Short answer: **keep the 3 ms.** Without it the delay is a frame shorter, but the match waits 2.5 times as often and the game
runs 4% slow.

## Setup

- `tools/browser_mp_test/latency.mjs --players 2 --rtt 57 --jitter 10 --secs 90`, two headless Chromium tabs on this Mac. The game
  measured pings of 49 to 70 ms, which puts the rise level at about 65 ms. That is 5 frames with the margin and 4 without.
  (At `--rtt 52` both variants settle on 4 frames.)
- Two `wotm_server` builds of wip/browser 1aad89ef, the one deployed: `kDelayMarginMs` 3.0 (as shipped) and 0.0.
- Three runs of each, alternating. Each run started once this Mac's 1-minute load was under 4, and the two tabs took it to about 10.
  Waits come from `python3 tools/diag_waits.py <run dir>`, over the steady windows.

## Result (6 player rows per variant)

| | margin 3 ms | margin 0 ms |
|---|---|---|
| delay (frames) | 4 and 5 | 4 |
| delay stage `lat_dl_ms` | 74.8 to 80.8 (mean 78.3) | 59.5 to 61.9 (mean 61.1) |
| waits stage `lat_st_ms` | 13.0 to 20.4 (mean 16.5) | 24.9 to 30.6 (mean 26.6) |
| share of the time waiting | 4.4 to 9.0% | 12.1 to 22.0% |
| game frames a second | 58.0 to 58.6 | 56.0 to 56.4 |

Without the margin, delay plus waits per press is about 7 ms lower. But inputs keep arriving just after the frame that needs them, so
the match waits two and a half times as often and runs at 56 instead of 58 game frames a second. Players see that as stutter and slow
motion. The rule's other slack (the 4/5 level and rounding up to whole frames) trades the same way, delay for waits.

The tabs were headless and presented at about 10 fps with software GL, so the present stage and the totals are not meaningful here.
The delay, waits and game rate come from the netcode and are.
