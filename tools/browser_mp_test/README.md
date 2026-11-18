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
node --test tools/browser_mp_test/test/     # the log readers
```

`--help` lists every option. Local runs start their own `wotm_server` and `tools/serve_web.py` on free ports and stop them at the end.
Use `--site <dir>` to test a `web_dist` copy and `--data <dir>` for the folder with `game.iso` and `SCUS_971.97`. Results (logs,
screenshots, `results.md`, `results.json`) go to `<runtime>/browser_mp_test/<time>/`, or `--out`.

A live run targets https://playwotm.com/ (`--url` for another site; `--password` or `WOTM_SITE_PASSWORD` only for a site behind the password gate). Every tab downloads the game's data (57 MB), so keep live
runs short, and use your own lobbies (each run makes its own). The live server's log is out of reach, so a live run judges by the tabs'
logs only (the server-side DESYNC and "left the match" checks are local only).

## What each session checks

| session | rows |
|---|---|
| `match` | `lobby` (host, join, every tab lists every member, the code), `lobby_errors` (duplicate name, wrong and short code), `cap_refused` (N=8: the ninth join), `lobby_leave` (N<8: Leave brings back the Host/Join choice without a reload and the others see the member go), `lobby_chat` (a chat line typed in one tab reaches every tab as plain text with the sender's name; a burst is refused with a message), `lobby_ping` (every tab shows each member's ping in ms while the lobby waits), `lobby_list` (a player on the Host/Join choice sees the lobby with its owner and n/8, and Join on the row puts them in; at N=8 Join is off), `invite_link` (N<=6: Copy invite link gives `/?join=CODE`; a new profile that opens it finds the code filled in and joins after typing a name, a profile with a remembered name joins by itself, and the address loses the code), `picks` (a monster change and the owner's stage and wins reach every tab), `match_start` (every tab starts as player i of N), `inputs` (every tab sees a held button from every player in `WOTM_PADLOG`), `sync` (state hashes equal on every shared frame, no server DESYNC), `health`, `perf`, `late_join`, `leave` (a closed tab: the others go on), `refresh` (a reloaded tab: front panel, cannot rejoin mid-match), `refusal_message_stays` |
| `round` | `round_end`, `win_screen` (wins 1 and the `vencer` cheat), `rematch` (PLAY AGAIN, round 2 in sync), `quit` (QUIT returns every tab to the front panel), `rejoin_after_match` |
| `stall` | `stall_detected`, `away_note` (hidden tab), `stall_prompt` (60 s), `keep_waiting`, `stall_recovery`, `connection_lost`, `reconnect` |
| `creations` | a created map and skin imported in every tab, picked in the lobby and played (N=2 and 4); `creations_missing`: one tab without them is refused with an explanation |
| `saves` | the Settings card's Saves tab on seeded saves, without booting the game: `saves_list`, `saves_backup` (a zip download), `saves_delete` (with its automatic backup), `saves_restore`, `saves_refused` (a zip that is not a save changes nothing) |
| `movies` | the startup movies on Single player: `movie_plays_during_boot` (the movie shows while the game's boot runs), `movie_skippable` (Esc skips each of the two), `movie_game_runs`, `movie_original` (keys do nothing), `movie_off` (no movie file requested); skipped without `data/movies/` (`tools/web_movies.py`) |
| `single` | `settings_page` (Resolution and Anti-aliasing set on the Settings page reach the game's log) and `single_player` (the front panel's Single player button boots the game) |
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
- `PS2X_PAD` scripted pads are not used: in the browser build `GetTime()` is a wall-clock time, so a script's seconds never match.

## Desync matrix

`node tools/browser_mp_test/desync.mjs --cases 3,4,5,8+2,5+5 --scenarios baseline,pause,personal,pause_away,away,stall,refresh,leave,mix --reps 3`
plays full matches to the win screen (`WOTM_CHEATS=vencer`, wins 1) after a scripted trigger and compares every tab's state hashes (and
locally the server's DESYNC lines). A case `A+B` runs lobbies of A and B at the same time on one server. `--firefox K` and `--mods
jump,fast,turbo` apply to every lobby; `--target live` needs no password on the open site. A problem with no state difference at a host
load over twice the cores is `OVERLOAD` (inconclusive), not a failure. Results: `results.md` and `results.json` in `--out`, with each trial's stage and
seed. Run it under `caffeinate -dis` (display sleep freezes tabs), from the tree the `web_dist` was built from. See
`docs/investigacoes/2026-11-16/desync-matrix/`.

## Input latency

`node tools/browser_mp_test/latency.mjs --players 4 --rtt 60,60,70,80 --jitter 15 --spike 4:6:150:400 --secs 120 --browser chrome --unlock-frames`
plays a local AI match with every tab behind its own `tools/net_lagproxy.py --tcp` link (round trip, jitter, and a periodic spike on
one player), taps Space in every tab every 0.35 to 1 s (`--press-ms`), and prints each player's input latency from the server's
diagnostics files (`lat_` fields, `docs/ONLINE.md` "Diagnostics"): presses, the total (press to the first present that draws it) and its
stages, the delays and pings seen. `--browser chrome` uses the GPU; the default headless Chromium draws in software, at a few frames a
second on a busy machine, which the present stage then shows. Results: `summary.json`, the diag files, `server.log` and `proxy.log` in
`--out`. The summary reader is `lib/latency.mjs` (tested in `test/latency.test.mjs`). The keys are Playwright's, sent straight to the page:
the time the system and the browser take to deliver a real key is not in the numbers.
`--http-mbps 20 --lobby-secs 30` models a slow line and a lobby that waits: each Chromium tab's downloads are limited once its page
has loaded, so a match start that waits on its disc download shows in the first windows' `disc_net_mb` and the server's `in_gap_max_ms`.
