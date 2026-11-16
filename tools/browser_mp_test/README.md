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
node tools/browser_mp_test/run.mjs --target live --password <site password> --players 1,2,4   # shorter confirmation pass
node --test tools/browser_mp_test/test/     # the log readers
```

`--help` lists every option. Local runs start their own `wotm_server` and `tools/serve_web.py` on free ports and stop them at the end.
Use `--site <dir>` to test a `web_dist` copy and `--data <dir>` for the folder with `game.iso` and `SCUS_971.97`. Results (logs,
screenshots, `results.md`, `results.json`) go to `<runtime>/browser_mp_test/<time>/`, or `--out`.

A live run needs the site's password (`--password` or `WOTM_SITE_PASSWORD`). Every tab downloads the game's data (57 MB), so keep live
runs short, and use your own lobbies (each run makes its own). The live server's log is out of reach, so a live run judges by the tabs'
logs only (the server-side DESYNC and "left the match" checks are local only).

## What each session checks

| session | rows |
|---|---|
| `match` | `lobby` (host, join, every tab lists every member, the code), `lobby_errors` (duplicate name, wrong and short code), `cap_refused` (N=8: the ninth join), `lobby_leave` (N<8: Leave brings back the Host/Join choice without a reload and the others see the member go), `invite_link` (N<=6: Copy invite link gives `/?join=CODE`; a new profile that opens it finds the code filled in and joins after typing a name, a profile with a remembered name joins by itself, and the address loses the code), `picks` (a monster change and the owner's stage and wins reach every tab), `match_start` (every tab starts as player i of N), `inputs` (every tab sees a held button from every player in `WOTM_PADLOG`), `sync` (state hashes equal on every shared frame, no server DESYNC), `health`, `perf`, `late_join`, `leave` (a closed tab: the others go on), `refresh` (a reloaded tab: front panel, cannot rejoin mid-match), `refusal_message_stays` |
| `round` | `round_end`, `win_screen` (wins 1 and the `vencer` cheat), `rematch` (PLAY AGAIN, round 2 in sync), `quit` (QUIT returns every tab to the front panel), `rejoin_after_match` |
| `stall` | `stall_detected`, `away_note` (hidden tab), `stall_prompt` (60 s), `keep_waiting`, `stall_recovery`, `connection_lost`, `reconnect` |
| `creations` | a created map and skin imported in every tab, picked in the lobby and played (N=2 and 4); `creations_missing`: one tab without them is refused with an explanation |
| `single` | the front panel's Single player button boots the game |

`stall` and `creations` run only at the `--deep-n` player counts (default 2, 4, 8).

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
