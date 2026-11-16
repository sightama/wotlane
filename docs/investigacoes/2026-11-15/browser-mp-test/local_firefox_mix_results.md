# Browser multiplayer results

- target: local http://127.0.0.1:55725/
- build: wl-82c5b21887ec9e7f
- browser: chromium headless 156.0.8078.4, plus Firefox 157.0 for the last 1 player(s) of each lobby
- isolation: context
- host: 10 cores, load 25.1
- started: 2026-11-16T03:32:24.742Z
- players: 2,4

| check | N=2 | N=4 |
|---|---|---|
| lobby | pass | pass |
| lobby_errors | pass | pass |
| picks | pass | pass |
| match_start | pass | pass |
| inputs | pass | pass |
| sync | pass | pass |
| health | FAIL | FAIL |
| perf | info | info |
| late_join | pass | pass |
| leave | pass | pass |
| refusal_message_stays | FAIL | FAIL |
| refresh |  | pass |

## Details

- **N=2 lobby** PASS: 2 tabs list 2 members, code JCS9
- **N=2 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=2 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=2 match_start** PASS: all 2 tabs started (delay 2 frames)
- **N=2 inputs** PASS: every tab saw each of the 2 players' own held button (first input applied on 2/2 tabs)
- **N=2 sync** PASS: 6 frames compared, all equal on 2 tabs
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
  - n2/match/server.log
- **N=2 health** FAIL: P2: page error: MutationObserver.observe: Argument 1 is not an object.
- **N=2 perf** INFO: game fps (full speed 60) min 35.2 avg 35.2 (lockstep: all tabs run at the slowest pace); browser CPU 4.66 of 10 cores (renderers 0.00,0.44; other processes browser 0.02, GPU 4.18, network.mojom.NetworkService 0.01, audio.mojom.AudioService 0.01); host load 22.0; network waits 31164 ms max per tab
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
  - n2/match/server.log
- **N=2 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=2 leave** PASS: after P2 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=2 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=4 lobby** PASS: 4 tabs list 4 members, code WRSG
- **N=4 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=4 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=4 match_start** PASS: all 4 tabs started (delay 2 frames)
- **N=4 inputs** PASS: every tab saw each of the 4 players' own held button (first input applied on 4/4 tabs)
- **N=4 sync** PASS: 3 frames compared, all equal on 4 tabs
  - n4/match/P1.log
  - n4/match/P1.png
  - n4/match/P2.log
  - n4/match/P2.png
  - n4/match/P3.log
  - n4/match/P3.png
  - n4/match/P4.log
  - n4/match/P4.png
  - n4/match/server.log
- **N=4 health** FAIL: P4: page error: MutationObserver.observe: Argument 1 is not an object.
- **N=4 perf** INFO: game fps (full speed 30) min 19.5 avg 20.1 (lockstep: all tabs run at the slowest pace); browser CPU 3.62 of 10 cores (renderers 0.00,0.42,0.42,0.42; other processes browser 0.05, GPU 2.27, network.mojom.NetworkService 0.03, audio.mojom.AudioService 0.01); host load 37.2; network waits 42383 ms max per tab
  - n4/match/P1.log
  - n4/match/P1.png
  - n4/match/P2.log
  - n4/match/P2.png
  - n4/match/P3.log
  - n4/match/P3.png
  - n4/match/P4.log
  - n4/match/P4.png
  - n4/match/server.log
- **N=4 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=4 leave** PASS: after P4 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=4 refresh** PASS: after reload: front panel true, message "Your last lobby was WRSG: Join puts you back in it."; rejoin mid-match: joined=false "lobby is in a match; wait for it to end" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=4 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
