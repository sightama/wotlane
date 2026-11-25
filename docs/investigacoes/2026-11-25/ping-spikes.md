# Where the online ping spikes come from (2026-11-25)

The input-latency breakdown (`input-latency-breakdown.md`, same folder) found that the live smoke's 5 frames of input delay were most
likely set by ping spikes, not the median ping. The coordinator asked where the spikes come from: the player's path, the server, or the
Mac the smokes run on. Short answer: **this Mac's home internet link, and only while a match runs on it.** wotm_server, Caddy and
Cloudflare add no spikes; real players on other networks have a rise level within 0 to 6 ms of their median round trip, so spikes cost
them about nothing. **Live smokes from this Mac overstate the input delay**: they are not what players see, and the delay rule is judged
with the local lag proxy or from another network instead.

## Method

`tools/ws_ping.py` sends a WebSocket ping frame every 100 ms and prints each pong's round trip; wotm_server answers ping frames itself,
so it measures what the server's own pings of a browser measure (the path plus the server's loop). Series were taken at the same time:

| series | from | to |
|---|---|---|
| server | the VM | `ws://127.0.0.1:7780/lobby` (wotm_server alone) |
| Caddy | the VM | `wss://playwotm.com/lobby` at 127.0.0.1 (Caddy, then wotm_server) |
| Cloudflare | this Mac | `wss://playwotm.com/lobby` |
| origin | this Mac | the same, `--connect-to` the VM's IP (Cloudflare left out) |

They ran idle and during muted live smokes (`tools/browser_mp_test/run.mjs --target live --players 2`, Chrome and Firefox tabs on this
Mac), with ICMP pings from this Mac to the home router and to 8.8.8.8 and this Mac's Wi-Fi interface counters (`netstat -ibn`) sampled
every second. A spike is a pong over the series' median plus 25 ms. "Rise level" replays the server's rule on a series (the latest round
trip every 0.5 s, the 4th highest of the last 16: `RttWindow::riseLevel`). The prod diag files of the last 7 days (264 files) give the
server's view of real players.

## Idle (3 minutes)

| series | median ms | p99 | max | spikes | rise level, median |
|---|---|---|---|---|---|
| server | 3.0 | 5.8 | 6.1 | 0% | 4.2 |
| Cloudflare | 29.6 | 69.0 | 537 | 1.2% | 31.2 |
| origin | 32.2 | 46.5 | 299 | 0.5% | 33.6 |

Every spike of the origin series (8) came within 0.3 s of one in the Cloudflare series: both paths share this Mac's link, and the
spikes are rare enough that the rise level stays within 2 ms of the median.

## During a match on this Mac

Three smokes, each with the server series on the VM (one also with Caddy):

| series | median ms | p90 | p99 | max | spikes |
|---|---|---|---|---|---|
| server | 3.1 to 3.2 | 5.2 to 5.3 | 5.6 to 5.7 | 6.1 | 0% |
| Caddy | 3.6 | 5.7 | 6.2 | 6.7 | 0% |
| Cloudflare | 29.8 to 46.2 | 256 to 270 | 349 to 460 | 403 to 634 | 31 to 46% |
| origin | 32.9 | 256 | 446 | 644 | 31% |

- The spikes on the two paths from this Mac coincide (533 of 536 within 0.3 s), and the VM's series stay clean (VM load 0.1), so neither
  Cloudflare nor Caddy nor wotm_server makes them.
- ICMP from this Mac during a match: the home router 2.8 ms median, 12.6 ms max (the Wi-Fi link is fine); 8.8.8.8 22 ms median, 482 ms
  p90, 842 ms max. The queue is past the router, on the home internet link.
- They start with the match and stop when it ends. The match itself is light: ~300 kbit/s in and ~500 out, ~350 packets a second in and
  ~500 out. The downloads before it (100 to 280 Mbit/s for a few seconds) come with few spikes. This Mac's CPU stayed at 85 to 480% of
  1000%.
- The same packet rate without the game does not reproduce it: 500 UDP packets a second for 30 s gave no ping to 8.8.8.8 over 34 ms;
  four WebSockets pinging at 60 Hz through Cloudflare (~330 packets a second in, ~510 out) gave 6 of 375 pings over 100 ms (max 164)
  and 6% spikes on the probe. What in the match triggers the home link's queue is not known.

The smokes' own matches show the cost: lobby GK4T's delay went to the 15-frame cap (server round trips 100 to 600 ms), and J7XR's 5
frames (`input-latency-breakdown.md`) are this too.

## Real players (prod diag files, 7 days)

| players | matches | server round trip, median ms | rise level minus median | delay |
|---|---|---|---|---|
| two Windows PCs' tabs (RTX 3070, GTX 980), 2026-11-18 | 8 | 53 to 69 | 0 to 5 ms | 5 (their median ping) |
| kero and ko (Windows), 2026-11-24 | 3 | 265 to 272 | 1 to 6 ms | 15 (the cap) |

For players on other networks the rise level is the median ping plus a few ms: spikes cost them about nothing, and the delay is their
base round trip. kero and ko's path is (266 + 266) / 2 + 3 = 269 ms, 16.1 frames, above the cap of 15, so they also wait every frame;
where they are is not in the logs (the server records no addresses).

## wotm_server's loop adds 0 to 5 ms

The server's localhost series has a median of 3 ms and a max of 6 ms with nothing else going on: `wotm_server.cpp` runs `server.poll(5)`,
whose `enet_host_service` wait watches only the UDP socket, so a browser's WebSocket data (its pongs, and its inputs) waits up to 5 ms
for the loop to read it. That is in every browser round trip the delay rule uses and in every relayed input: about 2.5 ms on average,
~0.15 frames.

## What follows

- Live smokes from this Mac are not used to judge the input delay or the delay rule: the lag proxy (`tools/net_lagproxy.py`,
  `latency.mjs`) or another network instead.
- The rule's slack (the 4/5 level, the 3 ms margin, the rounding) is measured locally with recorded jitter: for players on other
  networks, spikes are not what sets their delay.
- Server placement: kero and ko show distant players exist. #215 logs each member's country (Cloudflare's `CF-IPCountry`, no
  addresses) in the match diag, so placement is decided on data.
- #214 makes the server's loop wait on its WebSocket sockets too, which takes the 0 to 5 ms out of every browser's round trip and
  input.
