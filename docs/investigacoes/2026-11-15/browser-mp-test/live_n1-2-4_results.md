# Browser multiplayer results

- target: live https://35-185-58-174.sslip.io/
- build: wl-82c5b21887ec9e7f
- browser: chromium headless 156.0.8078.4
- isolation: context
- host: 10 cores, load 23.1
- started: 2026-11-16T03:48:07.550Z
- players: 1,2,4

| check | N=1 | N=2 | N=4 |
|---|---|---|---|
| single_player | pass |  |  |
| lobby | pass | pass | pass |
| lobby_errors | pass | pass | pass |
| picks | skip | pass | pass |
| match_start | pass | pass | pass |
| inputs | pass | pass | pass |
| sync | pass | pass | pass |
| health | pass | pass | pass |
| perf | info | info | info |
| late_join | pass | pass | pass |
| refusal_message_stays | pass | pass | pass |
| round | skip |  |  |
| leave |  | pass | pass |
| round_end |  | pass | pass |
| win_screen |  | pass | pass |
| rematch |  | pass | pass |
| quit |  | pass | pass |
| rejoin_after_match |  | pass | pass |
| stall_detected |  | pass |  |
| away_note |  | pass |  |
| stall_prompt |  | pass |  |
| keep_waiting |  | pass |  |
| stall_recovery |  | pass |  |
| connection_lost |  | pass |  |
| reconnect |  | pass |  |
| creations |  | pass |  |
| creations_missing |  | pass |  |
| refresh |  |  | pass |

## Details

- **N=1 single_player** PASS: status "Running"; no crash or page error
- **N=1 lobby** PASS: 1 tabs list 1 members, code JHWQ
- **N=1 lobby_errors** PASS: duplicate name: "name already taken in the lobby"; wrong code: "no such lobby (check the code)"; short code: "The code has 4 characters (letters and digits)."
- **N=1 picks** SKIP: needs a guest
- **N=1 match_start** PASS: all 1 tabs started (delay 2 frames)
- **N=1 inputs** PASS: every tab saw each of the 1 players' own held button (first input applied on 1/1 tabs)
- **N=1 sync** PASS: 9 frames compared, all equal on 1 tabs
  - n1/match/P1.log
  - n1/match/P1.png
- **N=1 health** PASS: no crash, stop or page error on any tab
- **N=1 perf** INFO: game fps (full speed 60) min 60.0 avg 60.0 (lockstep: all tabs run at the slowest pace); browser CPU 4.65 of 10 cores (renderers 0.00,0.44; other processes browser 0.03, GPU 4.15, network.mojom.NetworkService 0.02, audio.mojom.AudioService 0.01); host load 15.1; network waits 0 ms max per tab
  - n1/match/P1.log
  - n1/match/P1.png
- **N=1 late_join** PASS: late join: joined=false message "lobby is in a match; wait for it to end"
- **N=1 refusal_message_stays** PASS: every refusal stayed on screen
- **N=1 round** SKIP: the cheat that ends a round knocks out the other players: needs another one
- **N=2 lobby** PASS: 2 tabs list 2 members, code KD5M
- **N=2 lobby_errors** PASS: duplicate name: "name already taken in the lobby"; wrong code: "no such lobby (check the code)"; short code: "The code has 4 characters (letters and digits)."
- **N=2 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=2 match_start** PASS: all 2 tabs started (delay 3 frames)
- **N=2 inputs** PASS: every tab saw each of the 2 players' own held button (first input applied on 2/2 tabs)
- **N=2 sync** PASS: 6 frames compared, all equal on 2 tabs
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
- **N=2 health** PASS: no crash, stop or page error on any tab
- **N=2 perf** INFO: game fps (full speed 60) min 42.0 avg 42.5 (lockstep: all tabs run at the slowest pace); browser CPU 3.08 of 10 cores (renderers 0.00,0.54,0.55; other processes browser 0.06, GPU 1.86, network.mojom.NetworkService 0.05, audio.mojom.AudioService 0.01); host load 23.2; network waits 13888 ms max per tab
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
- **N=2 late_join** PASS: late join: joined=false message "lobby is in a match; wait for it to end"
- **N=2 leave** PASS: after P2 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=2 refusal_message_stays** PASS: every refusal stayed on screen
- **N=2 round_end** PASS: round 1 ended on 2/2 tabs
- **N=2 win_screen** PASS: the win dialog opened on all 2 tabs
- **N=2 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 2 tabs
- **N=2 quit** PASS: QUIT took every tab back to the front panel
- **N=2 rejoin_after_match** PASS: front panels say ""; Join on the old code: joined=false "TimeoutError: page.waitForFunction: Timeout 15000ms exceeded." (shown after: "TimeoutError: page.waitForFunction: Timeout 15000ms exceeded.")
- **N=2 stall_detected** PASS: P1 says "Waiting for P2 (their game tab is in the background)" after 973 ms
- **N=2 away_note** PASS: wait text "Waiting for P2 (their game tab is in the background)" (tab hidden: emulated)
- **N=2 stall_prompt** PASS: "Waiting for P2 (their game tab is in the background)." with Keep waiting and "Leave the match"
- **N=2 keep_waiting** PASS: the prompt went away and stayed away
- **N=2 stall_recovery** PASS: every tab moved on by two hashes: true; longest wait now 0 ms; 2 frames compared, all equal on 2 tabs
- **N=2 connection_lost** PASS: after the socket dropped: prompt "The match stopped: connection to the server dropped.", button "Back to the front page"
- **N=2 reconnect** PASS: way back: front panel says ""; Join mid-match: joined=false "lobby is in a match; wait for it to end"
- **N=2 refusal_message_stays** PASS: every refusal stayed on screen
- **N=2 creations** PASS: skin shown in the member list: true; skin and map applied on 2/2 tabs; 4 frames compared, all equal on 2 tabs
- **N=2 creations_missing** PASS: reasons ["rejected by the server: game configuration differs from the others: the mods (creations) differ (acc6449abd650c75:81c394b5346b78e8 vs acc6449abd650c75:e194505b1bb74d68)",""]; the page says "Your game setup differs from the host's (creations or settings). Make sure you both have the same creations imported (Your creations, on the front panel), then join again."
- **N=4 lobby** PASS: 4 tabs list 4 members, code DAEX
- **N=4 lobby_errors** PASS: duplicate name: "name already taken in the lobby"; wrong code: "no such lobby (check the code)"; short code: "The code has 4 characters (letters and digits)."
- **N=4 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=4 match_start** PASS: all 4 tabs started (delay 2 frames)
- **N=4 inputs** PASS: every tab saw each of the 4 players' own held button (first input applied on 4/4 tabs)
- **N=4 sync** PASS: 4 frames compared, all equal on 4 tabs
  - n4/match/P1.log
  - n4/match/P1.png
  - n4/match/P2.log
  - n4/match/P2.png
  - n4/match/P3.log
  - n4/match/P3.png
  - n4/match/P4.log
  - n4/match/P4.png
- **N=4 health** PASS: no crash, stop or page error on any tab
- **N=4 perf** INFO: game fps (full speed 30) min 26.7 avg 26.7 (lockstep: all tabs run at the slowest pace); browser CPU 8.73 of 10 cores (renderers 0.00,0.53,0.53,0.53,0.53; other processes browser 0.07, GPU 6.47, network.mojom.NetworkService 0.06, audio.mojom.AudioService 0.01); host load 14.8; network waits 10932 ms max per tab
  - n4/match/P1.log
  - n4/match/P1.png
  - n4/match/P2.log
  - n4/match/P2.png
  - n4/match/P3.log
  - n4/match/P3.png
  - n4/match/P4.log
  - n4/match/P4.png
- **N=4 late_join** PASS: late join: joined=false message "lobby is in a match; wait for it to end"
- **N=4 leave** PASS: after P4 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=4 refresh** PASS: after reload: front panel true, message "Your last lobby DAEX is in a match: Join works once it ends."; rejoin mid-match: joined=false "lobby is in a match; wait for it to end" (shown after: "lobby is in a match; wait for it to end")
- **N=4 refusal_message_stays** PASS: every refusal stayed on screen
- **N=4 round_end** PASS: round 1 ended on 4/4 tabs
- **N=4 win_screen** PASS: the win dialog opened on all 4 tabs
- **N=4 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 4 tabs
- **N=4 quit** PASS: QUIT took every tab back to the front panel
- **N=4 rejoin_after_match** PASS: front panels say ""; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "no such lobby (check the code)")
