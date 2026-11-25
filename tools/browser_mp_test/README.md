# Browser multiplayer test

Drives 1 to 8 players in separate desktop-browser contexts (each with its own OPFS, so its own saves, mods and creations) through the
web front panel and a match, on a local `wotm_server` or on the live site, and writes a results table. A ninth player at a full lobby
must be refused. The exit code is 1 when any row fails.

## Run

```
sh tools/test_net.sh notest                 # builds native/net/build/wotm_server
sh tools/build_web.sh                       # builds <runtime>/web_dist (see docs/passos/WEB.md)
npm install --prefix tools/browser_mp_test  # playwright (or have it in the global npm root; run.mjs falls back to it)
node tools/browser_mp_test/run.mjs --players 1,2,3,4,5,6,7,8 --disc <extracted disc folder>
node tools/browser_mp_test/run.mjs --target live --players 1,2,4   # shorter confirmation pass
node tools/browser_mp_test/run.mjs --target live --players 2 --browser chrome --firefox 1   # live smoke after a deploy: Chrome with Firefox
node tools/browser_mp_test/run.mjs --target live --players 2 --browser brave                # ... and Brave (the supported browsers)
node --test tools/browser_mp_test/test/     # the log readers
```

`--help` lists every option. Local runs start their own `wotm_server` and `tools/serve_web.py` on free ports and stop them at the end.
Use `--site <dir>` to test a `web_dist` copy and `--data <dir>` for the folder with `game.iso` and `SCUS_971.97`. Results (logs,
screenshots, `results.md`, `results.json`) go to `<runtime>/browser_mp_test/<time>/`, or `--out`.

`node tools/browser_mp_test/quit_starved.mjs --browser firefox|chromium|chrome [--hidden]` loads `web/reload.js` in a page whose `setTimeout`, `setInterval` and `requestAnimationFrame` are disabled and checks that the reload still happens (QUIT must not wait on a timer). `timer_heartbeat.mjs` prints how late `setTimeout` fires on each of several tabs under a game-like load.

`run.mjs --diag-out <file>` (not `desync.mjs` or `latency.mjs`) appends every `DIAG` line each tab sends on its lobby socket (`web/diag.js`: the join report, the periodic ones, the last
one) to the file, one per line as `<time> <player name> <line>`. It reads Playwright's WebSocket frame events, so it works on the live site,
where the server's own diagnostics files (`wotm_server --diag-dir`) are out of reach, and gives a match per player.

A live run targets https://playwotm.com/ (`--url` for another site; `--password` or `WOTM_SITE_PASSWORD` only for a site behind the password gate). Every tab downloads the game's data (57 MB), so keep live
runs short, and use your own lobbies (each run makes its own). The live server's log is out of reach, so a live run judges by the tabs'
logs only (the server-side DESYNC and "left the match" checks are local only).

## What each session checks

| session | rows |
|---|---|
| `match` | `lobby` (host, join, every tab lists every member, the code), `lobby_errors` (duplicate name, wrong and short code), `cap_refused` (N=8: the ninth join), `lobby_leave` (N<8: Leave brings back the Host/Join choice without a reload and the others see the member go), `lobby_chat` (a chat line typed in one tab reaches every tab as plain text with the sender's name; a burst is refused with a message), `lobby_ping` (every tab shows each member's ping in ms while the lobby waits), `lobby_list` (a player on the Host/Join choice sees the lobby with its owner and n/8, and Join on the row puts them in; at N=8 Join is off), `invite_link` (N<=6: Copy invite link gives `/?join=CODE`; a new profile that opens it finds the code filled in and joins after typing a name, a profile with a remembered name joins by itself, and the address loses the code), `picks` (a monster change and the owner's stage and wins reach every tab), `match_start` (every tab starts as player i of N), `inputs` (every tab sees a held button from every player in `WOTM_PADLOG`), `sync` (state hashes equal on every shared frame, no server DESYNC), `health`, `perf`, `late_join`, `leave` (a closed tab: the others go on), `refresh` (a reloaded tab: front panel, cannot rejoin mid-match), `refusal_message_stays` |
| `round` | `round_end`, `win_screen` (wins 1 and the `vencer` cheat), `rematch` (PLAY AGAIN, round 2 in sync), `quit` (QUIT returns every tab to the front panel), `rematch_lobby` (quick rematch: every page loads again and is back in its lobby by itself, the owner's button reads Rematch), `rematch_match` (Rematch starts the next match on every tab) |
| `stall` | `stall_detected`, `away_note` (hidden tab), `stall_prompt` (60 s), `keep_waiting`, `stall_recovery`, `connection_lost`, `reconnect` |
| `creations` | a created map and skin imported in every tab, picked in the lobby and played (N=2 and 4); `creations_missing`: one tab without them is refused with an explanation |
| `saves` | the Settings card's Saves tab on seeded saves, without booting the game: `saves_list`, `saves_backup` (a zip download), `saves_delete` (with its automatic backup), `saves_restore`, `saves_refused` (a zip that is not a save changes nothing) |
| `movies` | the startup movies on Single player: `movie_plays_during_boot` (the movie shows while the game's boot runs), `movie_skippable` (Esc skips each of the two), `movie_game_runs`, `movie_original` (keys do nothing), `movie_off` (no movie file requested); skipped without `data/movies/` (`tools/web_movies.py`) |
| `single` | `settings_page` (Resolution and Anti-aliasing set on the Settings page reach the game's log) and `single_player` (the front panel's Single player button boots the game) |
| `remap` | `remap_card` (a key button waits for a key and Esc cancels it), `remap_mouse` (a mouse button is taken as a key and the next click still works), `remap_conflict` (a key another button has swaps, and the card says so), `remap_file` (settings.ini has KB_LAYOUT and the KB_ lines) and `remap_game` (a key no default uses, held in a match, is the button it was given to) |
| `audio` | a 2-player match where the last player set effects and music volume to 0 on the Settings page and the first left them at 100: `audio_settings_reach_the_game` (each game logs the volumes it was given and no tab crashed), `audio_loud_is_heard` (the first player's audio output has sound in most windows), `audio_off_is_silent` (the last player's output stays at digital silence over the fight; the probe is `lib/audio.mjs`). Run with `--firefox 2` for Firefox on both |

`stall` and `creations` run only at the `--deep-n` player counts (default 2, 4, 8); `audio` runs at `--players 2`.

## How it works

- `lib/player.mjs` is one player: a context, the front panel's buttons, the page log (`window.wotmLog`, which the game's stdout and stderr
  fill), the keyboard (the game's default layout: arrows d-pad, WASD stick, Space X, E circle; it reads it as pad 1), a held network (`freezeNetwork`), a hidden tab (`hide`) and a dropped connection.
  A test-only `Module` setter adds `WOTM_PADLOG` and `WOTM_CHEATS` to the game's environment: nothing in `web/` changes.
- Sync: the game prints `[net] hash of frame N` for one in 16 of its state hashes (all 32 MB of guest RAM); every tab must print the same
  hash for a frame. On a local server `wotm_server`'s own DESYNC line is checked as well.
- Frame rate: the slope of those hash lines' frame index over time. A lockstep match runs at the slowest tab's pace, so one number
  describes all tabs. CPU: cumulative CPU time of every browser process (`SystemInfo.getProcessInfo`) over the play window; `perf` says
  when the machine itself is the limit (fps under 50 with the browser using over 75% of the cores).
- Chromium (headless shell, software GL) is the default; `--browser chrome` uses the installed Chrome with the GPU, `--headed` shows the
  windows. `--isolation browser` starts one browser per player. `--firefox K` makes the last K players of every lobby use Playwright's Firefox
  (a mixed lobby; needs a build with Firefox support). The lobby gets its own window height (`--lobby-height`; the card is taller than 400 px with 8
  members and scrolls) and the game its own size (`--height`).
- `PS2X_PAD` scripted pads are not used: the page address doesn't pass `PS2X_PAD` to the game (`web/pageenv.js` allowlist), and the harness drives the keyboard. In the browser build a script's seconds count from the first pad read (`g_ps2PadClock`, `native/runtime/wotm_pad.inc`), because raylib's `GetTime()` is a wall-clock time there.

## Desync matrix

`node tools/browser_mp_test/desync.mjs --cases 3,4,5,8+2,5+5 --scenarios baseline,pause,personal,pause_away,away,stall,refresh,leave,mix --reps 3`
plays full matches to the win screen (`WOTM_CHEATS=vencer`, wins 1) after a scripted trigger and compares every tab's state hashes (and
locally the server's DESYNC lines). A case `A+B` runs lobbies of A and B at the same time on one server. `--firefox K` and `--mods
jump,fast,turbo` apply to every lobby; `--target live` needs no password on the open site. A problem with no state difference at a host
load over twice the cores is `OVERLOAD` (inconclusive), not a failure. Results: `results.md` and `results.json` in `--out`, with each trial's stage and
seed. In `away` and `pause_away` (and `mix`) the server takes the hidden player out if it held everyone back for over 3 s in the background
(`native/net/wotm_net_holdback.hpp`): it then leaves the win screen and the comparison of the frames after it went, and otherwise it
must reach the win screen in step like the others;
any other stop fails the trial. Run it under `caffeinate -dis` (display sleep freezes tabs),
from the tree the `web_dist` was built from. See
`docs/investigacoes/2026-11-16/desync-matrix/`.

## Input latency

`node tools/browser_mp_test/latency.mjs --players 4 --rtt 60,60,70,80 --jitter 15 --spike 4:6:150:400 --secs 120 --browser chrome --unlock-frames`
plays a local AI match with every tab behind its own `tools/net_lagproxy.py --tcp` link (round trip, jitter, and a periodic spike on
one player), taps Space in every tab every 0.35 to 1 s (`--press-ms`), and prints each player's input latency from the server's
diagnostics files (`lat_` fields, `docs/ONLINE.md` "Diagnostics"): presses, the total (press to the first present that draws it) and its
stages, the delays and pings seen. `--browser brave` runs Brave (`WOTM_BRAVE`: its binary) with Shields at their defaults. `--browser chrome` uses the GPU; the default headless Chromium draws in software, at a few frames a
second on a busy machine, which the present stage then shows. Results: `summary.json`, the diag files, `server.log` and `proxy.log` in
`--out`. The summary reader is `lib/latency.mjs` (tested in `test/latency.test.mjs`). The keys are Playwright's, sent straight to the page:
the time the system and the browser take to deliver a real key is not in the numbers.
`--http-mbps 20 --lobby-secs 30` models a slow line and a lobby that waits: each Chromium tab's downloads are limited once its page
has loaded, so a match start that waits on its disc download shows in the first windows' `disc_net_mb` and the server's `in_gap_max_ms`.

## Disc prefetch

`node tools/browser_mp_test/prefetch_probe.mjs --flow page --browser chrome --mbps 20 [--live https://playwotm.com] [--timeline]`
measures the whole-game prefetch (`web/game_prefetch.js`) in fresh browser profiles of their own (a disk HTTP cache like a real one):
a fight opened straight away (the prefetch must not slow the first load: compare with a dist without it), then a profile left on
the front panel until the page logs how its prefetch ended, and a fight after it (0 MiB from the network once it is done). `--mbps`
throttles a Chromium page's network (DevTools emulation), `--timeline` prints the disc channel per 5 s (game downloads, cache hits,
game reads started, background downloads), and each fight line counts the chunk file requests seen. `--flow fps --secs 60 --headed`
opens a fight and compares the frame times of the seconds with background downloads against those without. `--flow visits
--visits 3 --live https://playwotm.com` opens the same fight in one profile again and again: each visit's seconds to the game's
runtime start and to the fight (the seconds count from boot.js's start), and in Chromium how `wotm.wasm` came (a 200 of 0 bytes
came from a browser cache, memory or disk; Firefox's requests are not seen). `--flow probe` fetches the
whole set from the script instead, and reports the storage quota and what `persist()` answers. The local server sends the page with
`no-store`, so locally every visit downloads the wasm again; `--live` gives the real caching. Muted like every check here.

## Rollback synctest

`node tools/browser_mp_test/synctest.mjs --browser chrome|firefox --back 2 [--step 2 --match 3,1,2,4,5,7,8,9,1] --minutes 30` serves a
built `web_dist` and runs one AI vs AI direct fight with `WOTM_SYNCTEST` (`docs/passos/NATIVO.md`): every fight frame goes n frames back
and is redone, and the whole state is compared with the first run. It prints the game's `[synctest]` lines (cost every 300 frames,
mismatches) and exits 1 on a mismatch; the game's log goes to `runtime/browser_synctest/`, and `python3 tools/synctest_cost.py <logs>`
turns the cost lines into a table. `--env K=V` adds game variables (`WOTM_SYNCTEST_SKIP=sound`, `WOTM_FRAMESPIN=8`). See
`docs/investigacoes/2026-11-18/rollback-synctest/`.
