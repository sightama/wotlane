# Browser multiplayer results

- target: local http://127.0.0.1:60077/
- build: wl-82c5b21887ec9e7f
- browser: chromium headless 156.0.8078.4
- isolation: context
- host: 10 cores, load 11.8
- started: 2026-11-16T02:50:55.529Z
- players: 1,2,3,4,5,6,7,8

| check | N=1 | N=2 | N=3 | N=4 | N=5 | N=6 | N=7 | N=8 |
|---|---|---|---|---|---|---|---|---|
| single_player | pass |  |  |  |  |  |  |  |
| lobby | pass | pass | pass | pass | pass | pass | pass | pass |
| lobby_errors | pass | pass | pass | pass | pass | pass | pass | pass |
| picks | skip | pass | pass | pass | pass | pass | pass | pass |
| match_start | pass | pass | pass | pass | pass | pass | pass | pass |
| inputs | pass | pass | pass | pass | pass | pass | pass | pass |
| sync | pass | pass | pass | pass | pass | pass | pass | pass |
| health | pass | pass | pass | pass | pass | pass | pass | pass |
| perf | info | info | info | info | info | info | info | info |
| late_join | pass | pass | pass | pass | pass | pass | pass | pass |
| refusal_message_stays | FAIL | FAIL | FAIL | FAIL | FAIL | FAIL | FAIL | FAIL |
| round | skip |  |  |  |  |  |  |  |
| leave |  | pass | pass | pass | pass | pass | pass | pass |
| round_end |  | pass | pass | pass | pass | pass | pass | pass |
| win_screen |  | pass | pass | pass | pass | pass | pass | pass |
| rematch |  | pass | pass | pass | pass | pass | pass | pass |
| quit |  | pass | pass | pass | pass | pass | pass | pass |
| rejoin_after_match |  | FAIL | FAIL | FAIL | FAIL | FAIL | FAIL | FAIL |
| stall_detected |  | pass |  | pass |  |  |  | pass |
| away_note |  | pass |  | pass |  |  |  | pass |
| stall_prompt |  | pass |  | pass |  |  |  | pass |
| keep_waiting |  | pass |  | pass |  |  |  | pass |
| stall_recovery |  | pass |  | pass |  |  |  | FAIL |
| connection_lost |  | pass |  | pass |  |  |  | FAIL |
| reconnect |  | pass |  | pass |  |  |  | pass |
| creations |  | pass |  | pass |  |  |  |  |
| creations_missing |  | pass |  |  |  |  |  |  |
| refresh |  |  | pass | pass | pass | pass | pass | pass |
| cap_refused |  |  |  |  |  |  |  | pass |

## Details

- **N=1 single_player** PASS: status "Running"; no crash or page error
- **N=1 lobby** PASS: 1 tabs list 1 members, code NN4H
- **N=1 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=1 picks** SKIP: needs a guest
- **N=1 match_start** PASS: all 1 tabs started (delay 2 frames)
- **N=1 inputs** PASS: every tab saw each of the 1 players' own held button (first input applied on 1/1 tabs)
- **N=1 sync** PASS: 9 frames compared, all equal on 1 tabs
  - n1/match/P1.log
  - n1/match/P1.png
  - n1/match/server.log
- **N=1 health** PASS: no crash, stop or page error on any tab
- **N=1 perf** INFO: game fps (full speed 60) min 60.0 avg 60.0 (lockstep: all tabs run at the slowest pace); browser CPU 8.22 of 10 cores (renderers 0.00,0.41; other processes browser 0.02, GPU 7.77, network.mojom.NetworkService 0.01, audio.mojom.AudioService 0.01); host load 13.2; network waits 0 ms max per tab
  - n1/match/P1.log
  - n1/match/P1.png
  - n1/match/server.log
- **N=1 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=1 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=1 round** SKIP: the cheat that ends a round knocks out the other players: needs another one
- **N=2 lobby** PASS: 2 tabs list 2 members, code 648B
- **N=2 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=2 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=2 match_start** PASS: all 2 tabs started (delay 2 frames)
- **N=2 inputs** PASS: every tab saw each of the 2 players' own held button (first input applied on 2/2 tabs)
- **N=2 sync** PASS: 9 frames compared, all equal on 2 tabs
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
  - n2/match/server.log
- **N=2 health** PASS: no crash, stop or page error on any tab
- **N=2 perf** INFO: game fps (full speed 60) min 58.4 avg 58.4 (lockstep: all tabs run at the slowest pace); browser CPU 8.92 of 10 cores (renderers 0.00,0.52,0.53; other processes browser 0.03, GPU 7.80, network.mojom.NetworkService 0.02, audio.mojom.AudioService 0.01); host load 12.0; network waits 3337 ms max per tab
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
  - n2/match/server.log
- **N=2 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=2 leave** PASS: after P2 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=2 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=2 round_end** PASS: round 1 ended on 2/2 tabs
- **N=2 win_screen** PASS: the win dialog opened on all 2 tabs
- **N=2 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 2 tabs
- **N=2 quit** PASS: QUIT took every tab back to the front panel
- **N=2 rejoin_after_match** FAIL: front panels say "Your last lobby was CYKQ: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=2 stall_detected** PASS: P1 says "Waiting for P2 (their game tab is in the background)" after 944 ms
- **N=2 away_note** PASS: wait text "Waiting for P2 (their game tab is in the background)" (tab hidden: emulated)
- **N=2 stall_prompt** PASS: "Waiting for P2 (their game tab is in the background)." with Keep waiting and "Leave the match"
- **N=2 keep_waiting** PASS: the prompt went away and stayed away
- **N=2 stall_recovery** PASS: every tab moved on by two hashes: true; longest wait now 0 ms; 3 frames compared, all equal on 2 tabs
- **N=2 connection_lost** PASS: after the socket dropped: prompt "The match stopped: connection to the server dropped.", button "Back to the front page"
- **N=2 reconnect** PASS: way back: front panel says "Your last lobby was ENJU: Join puts you back in it."; Join mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=2 refusal_message_stays** FAIL: rejoin after dropped connection: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=2 creations** PASS: skin shown in the member list: true; skin and map applied on 2/2 tabs; 5 frames compared, all equal on 2 tabs
- **N=2 creations_missing** PASS: reasons ["rejected by the server: game configuration differs from the others: the mods (creations) differ (dfbec06d778e678c:81c394b5346b78e8 vs dfbec06d778e678c:e194505b1bb74d68)",""]; the page says "Your game setup differs from the host's (creations or settings). Make sure you both have the same creations imported (Your creations, on the front panel), then join again."
- **N=3 lobby** PASS: 3 tabs list 3 members, code DTVQ
- **N=3 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=3 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=3 match_start** PASS: all 3 tabs started (delay 2 frames)
- **N=3 inputs** PASS: every tab saw each of the 3 players' own held button (first input applied on 3/3 tabs)
- **N=3 sync** PASS: 5 frames compared, all equal on 3 tabs
  - n3/match/P1.log
  - n3/match/P1.png
  - n3/match/P2.log
  - n3/match/P2.png
  - n3/match/P3.log
  - n3/match/P3.png
  - n3/match/server.log
- **N=3 health** PASS: no crash, stop or page error on any tab
- **N=3 perf** INFO: game fps (full speed 30) min 29.7 avg 29.7 (lockstep: all tabs run at the slowest pace); browser CPU 6.30 of 10 cores (renderers 0.00,0.46,0.46,0.46; other processes browser 0.05, GPU 4.83, network.mojom.NetworkService 0.03, audio.mojom.AudioService 0.01); host load 33.9; network waits 1306 ms max per tab
  - n3/match/P1.log
  - n3/match/P1.png
  - n3/match/P2.log
  - n3/match/P2.png
  - n3/match/P3.log
  - n3/match/P3.png
  - n3/match/server.log
- **N=3 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=3 leave** PASS: after P3 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=3 refresh** PASS: after reload: front panel true, message "Your last lobby was DTVQ: Join puts you back in it."; rejoin mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=3 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=3 round_end** PASS: round 1 ended on 3/3 tabs
- **N=3 win_screen** PASS: the win dialog opened on all 3 tabs
- **N=3 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 3 tabs
- **N=3 quit** PASS: QUIT took every tab back to the front panel
- **N=3 rejoin_after_match** FAIL: front panels say "Your last lobby was LARU: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=4 lobby** PASS: 4 tabs list 4 members, code QECZ
- **N=4 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=4 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=4 match_start** PASS: all 4 tabs started (delay 2 frames)
- **N=4 inputs** PASS: every tab saw each of the 4 players' own held button (first input applied on 4/4 tabs)
- **N=4 sync** PASS: 5 frames compared, all equal on 4 tabs
  - n4/match/P1.log
  - n4/match/P1.png
  - n4/match/P2.log
  - n4/match/P2.png
  - n4/match/P3.log
  - n4/match/P3.png
  - n4/match/P4.log
  - n4/match/P4.png
  - n4/match/server.log
- **N=4 health** PASS: no crash, stop or page error on any tab
- **N=4 perf** INFO: game fps (full speed 30) min 28.9 avg 28.9 (lockstep: all tabs run at the slowest pace); browser CPU 5.78 of 10 cores (renderers 0.00,0.62,0.62,0.62,0.62; other processes browser 0.06, GPU 3.18, network.mojom.NetworkService 0.04, audio.mojom.AudioService 0.01); host load 32.9; network waits 251 ms max per tab
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
- **N=4 refresh** PASS: after reload: front panel true, message "Your last lobby was QECZ: Join puts you back in it."; rejoin mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=4 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=4 round_end** PASS: round 1 ended on 4/4 tabs
- **N=4 win_screen** PASS: the win dialog opened on all 4 tabs
- **N=4 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 4 tabs
- **N=4 quit** PASS: QUIT took every tab back to the front panel
- **N=4 rejoin_after_match** FAIL: front panels say "Your last lobby was YTQ6: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=4 stall_detected** PASS: P1 says "Waiting for P4 (their game tab is in the background)" after 2672 ms
- **N=4 away_note** PASS: wait text "Waiting for P4 (their game tab is in the background)" (tab hidden: emulated)
- **N=4 stall_prompt** PASS: "Waiting for P4 (their game tab is in the background)." with Keep waiting and "Leave the match"
- **N=4 keep_waiting** PASS: the prompt went away and stayed away
- **N=4 stall_recovery** PASS: every tab moved on by two hashes: true; longest wait now 0 ms; 2 frames compared, all equal on 4 tabs
- **N=4 connection_lost** PASS: after the socket dropped: prompt "The match stopped: connection to the server dropped.", button "Back to the front page"
- **N=4 reconnect** PASS: way back: front panel says "Your last lobby was FHU7: Join puts you back in it."; Join mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=4 refusal_message_stays** FAIL: rejoin after dropped connection: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=4 creations** PASS: skin shown in the member list: true; skin and map applied on 4/4 tabs; 3 frames compared, all equal on 4 tabs
- **N=5 lobby** PASS: 5 tabs list 5 members, code M35N
- **N=5 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=5 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=5 match_start** PASS: all 5 tabs started (delay 2 frames)
- **N=5 inputs** PASS: every tab saw each of the 5 players' own held button (first input applied on 5/5 tabs)
- **N=5 sync** PASS: 5 frames compared, all equal on 5 tabs
  - n5/match/P1.log
  - n5/match/P1.png
  - n5/match/P2.log
  - n5/match/P2.png
  - n5/match/P3.log
  - n5/match/P3.png
  - n5/match/P4.log
  - n5/match/P4.png
  - n5/match/P5.log
  - n5/match/P5.png
  - n5/match/server.log
- **N=5 health** PASS: no crash, stop or page error on any tab
- **N=5 perf** INFO: game fps (full speed 30) min 29.3 avg 29.3 (lockstep: all tabs run at the slowest pace); browser CPU 9.35 of 10 cores (renderers 0.00,0.64,0.63,0.63,0.63,0.63; other processes browser 0.08, GPU 6.05, network.mojom.NetworkService 0.05, audio.mojom.AudioService 0.01); host load 16.6; network waits 718 ms max per tab
  - n5/match/P1.log
  - n5/match/P1.png
  - n5/match/P2.log
  - n5/match/P2.png
  - n5/match/P3.log
  - n5/match/P3.png
  - n5/match/P4.log
  - n5/match/P4.png
  - n5/match/P5.log
  - n5/match/P5.png
  - n5/match/server.log
- **N=5 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=5 leave** PASS: after P5 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=5 refresh** PASS: after reload: front panel true, message "Your last lobby was M35N: Join puts you back in it."; rejoin mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=5 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=5 round_end** PASS: round 1 ended on 5/5 tabs
- **N=5 win_screen** PASS: the win dialog opened on all 5 tabs
- **N=5 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 5 tabs
- **N=5 quit** PASS: QUIT took every tab back to the front panel
- **N=5 rejoin_after_match** FAIL: front panels say "Your last lobby was N9KY: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=6 lobby** PASS: 6 tabs list 6 members, code QGML
- **N=6 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=6 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=6 match_start** PASS: all 6 tabs started (delay 2 frames)
- **N=6 inputs** PASS: every tab saw each of the 6 players' own held button (first input applied on 6/6 tabs)
- **N=6 sync** PASS: 5 frames compared, all equal on 6 tabs
  - n6/match/P1.log
  - n6/match/P1.png
  - n6/match/P2.log
  - n6/match/P2.png
  - n6/match/P3.log
  - n6/match/P3.png
  - n6/match/P4.log
  - n6/match/P4.png
  - n6/match/P5.log
  - n6/match/P5.png
  - n6/match/P6.log
  - n6/match/P6.png
  - n6/match/server.log
- **N=6 health** PASS: no crash, stop or page error on any tab
- **N=6 perf** INFO: game fps (full speed 30) min 27.9 avg 28.0 (lockstep: all tabs run at the slowest pace); browser CPU 8.49 of 10 cores (renderers 0.00,0.72,0.73,0.70,0.70,0.69,0.71; other processes browser 0.09, GPU 4.05, network.mojom.NetworkService 0.07, audio.mojom.AudioService 0.02); host load 16.6; network waits 6352 ms max per tab
  - n6/match/P1.log
  - n6/match/P1.png
  - n6/match/P2.log
  - n6/match/P2.png
  - n6/match/P3.log
  - n6/match/P3.png
  - n6/match/P4.log
  - n6/match/P4.png
  - n6/match/P5.log
  - n6/match/P5.png
  - n6/match/P6.log
  - n6/match/P6.png
  - n6/match/server.log
- **N=6 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=6 leave** PASS: after P6 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=6 refresh** PASS: after reload: front panel true, message "Your last lobby was QGML: Join puts you back in it."; rejoin mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=6 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=6 round_end** PASS: round 1 ended on 6/6 tabs
- **N=6 win_screen** PASS: the win dialog opened on all 6 tabs
- **N=6 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 6 tabs
- **N=6 quit** PASS: QUIT took every tab back to the front panel
- **N=6 rejoin_after_match** FAIL: front panels say "Your last lobby was XULQ: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=7 lobby** PASS: 7 tabs list 7 members, code MUYK
- **N=7 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=7 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=7 match_start** PASS: all 7 tabs started (delay 2 frames)
- **N=7 inputs** PASS: every tab saw each of the 7 players' own held button (first input applied on 7/7 tabs)
- **N=7 sync** PASS: 5 frames compared, all equal on 7 tabs
  - n7/match/P1.log
  - n7/match/P1.png
  - n7/match/P2.log
  - n7/match/P2.png
  - n7/match/P3.log
  - n7/match/P3.png
  - n7/match/P4.log
  - n7/match/P4.png
  - n7/match/P5.log
  - n7/match/P5.png
  - n7/match/P6.log
  - n7/match/P6.png
  - n7/match/P7.log
  - n7/match/P7.png
  - n7/match/server.log
- **N=7 health** PASS: no crash, stop or page error on any tab
- **N=7 perf** INFO: game fps (full speed 30) min 27.5 avg 27.7 (lockstep: all tabs run at the slowest pace); browser CPU 8.74 of 10 cores (renderers 0.00,0.74,0.75,0.76,0.74,0.73,0.73,0.74; other processes browser 0.10, GPU 3.36, network.mojom.NetworkService 0.07, audio.mojom.AudioService 0.02); host load 17.3; network waits 6193 ms max per tab
  - n7/match/P1.log
  - n7/match/P1.png
  - n7/match/P2.log
  - n7/match/P2.png
  - n7/match/P3.log
  - n7/match/P3.png
  - n7/match/P4.log
  - n7/match/P4.png
  - n7/match/P5.log
  - n7/match/P5.png
  - n7/match/P6.log
  - n7/match/P6.png
  - n7/match/P7.log
  - n7/match/P7.png
  - n7/match/server.log
- **N=7 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=7 leave** PASS: after P7 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=7 refresh** PASS: after reload: front panel true, message "Your last lobby was MUYK: Join puts you back in it."; rejoin mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=7 refusal_message_stays** FAIL: duplicate name: the server said "name already taken in the lobby", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=7 round_end** PASS: round 1 ended on 7/7 tabs
- **N=7 win_screen** PASS: the win dialog opened on all 7 tabs
- **N=7 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 7 tabs
- **N=7 quit** PASS: QUIT took every tab back to the front panel
- **N=7 rejoin_after_match** FAIL: front panels say "Your last lobby was DZPK: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=8 lobby** PASS: 8 tabs list 8 members, code SES5
- **N=8 lobby_errors** PASS: duplicate name: "Lost the connection to the game server. Reload the page to try again."; wrong code: "Lost the connection to the game server. Reload the page to try again."; short code: "The code has 4 characters (letters and digits)."
- **N=8 cap_refused** PASS: 9th join: "Lost the connection to the game server. Reload the page to try again."; other tabs still list 8 members: true
- **N=8 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=8 match_start** PASS: all 8 tabs started (delay 2 frames)
- **N=8 inputs** PASS: every tab saw each of the 8 players' own held button (first input applied on 8/8 tabs)
- **N=8 sync** PASS: 4 frames compared, all equal on 8 tabs
  - n8/match/P1.log
  - n8/match/P1.png
  - n8/match/P2.log
  - n8/match/P2.png
  - n8/match/P3.log
  - n8/match/P3.png
  - n8/match/P4.log
  - n8/match/P4.png
  - n8/match/P5.log
  - n8/match/P5.png
  - n8/match/P6.log
  - n8/match/P6.png
  - n8/match/P7.log
  - n8/match/P7.png
  - n8/match/P8.log
  - n8/match/P8.png
  - n8/match/server.log
- **N=8 health** PASS: no crash, stop or page error on any tab
- **N=8 perf** INFO: game fps (full speed 30) min 24.3 avg 25.7 (lockstep: all tabs run at the slowest pace); browser CPU 7.86 of 10 cores (renderers 0.00,0.72,0.72,0.72,0.72,0.71,0.72,0.71,0.71; other processes browser 0.10, GPU 1.93, network.mojom.NetworkService 0.07, audio.mojom.AudioService 0.02); host load 24.6; network waits 14847 ms max per tab; LIKELY CPU-BOUND on this machine, not a game bug
  - n8/match/P1.log
  - n8/match/P1.png
  - n8/match/P2.log
  - n8/match/P2.png
  - n8/match/P3.log
  - n8/match/P3.png
  - n8/match/P4.log
  - n8/match/P4.png
  - n8/match/P5.log
  - n8/match/P5.png
  - n8/match/P6.log
  - n8/match/P6.png
  - n8/match/P7.log
  - n8/match/P7.png
  - n8/match/P8.log
  - n8/match/P8.png
  - n8/match/server.log
- **N=8 late_join** PASS: late join: joined=false message "Lost the connection to the game server. Reload the page to try again."
- **N=8 leave** PASS: after P8 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=8 refresh** PASS: after reload: front panel true, message "Your last lobby was SES5: Join puts you back in it."; rejoin mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=8 refusal_message_stays** FAIL: duplicate name: the server said "lobby is full", the panel then showed "Lost the connection to the game server. Reload the page to try again."; wrong code: the server said "no such lobby (check the code)", the panel then showed "Lost the connection to the game server. Reload the page to try again."; ninth player: the server said "lobby is full", the panel then showed "Lost the connection to the game server. Reload the page to try again."; late join: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."; rejoin after refresh: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=8 round_end** PASS: round 1 ended on 8/8 tabs
- **N=8 win_screen** PASS: the win dialog opened on all 8 tabs
- **N=8 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 8 tabs
- **N=8 quit** PASS: QUIT took every tab back to the front panel
- **N=8 rejoin_after_match** FAIL: front panels say "Your last lobby was 7TTZ: Join puts you back in it."; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "Lost the connection to the game server. Reload the page to try again.")
- **N=8 stall_detected** PASS: P1 says "Waiting for P8 (their game tab is in the background)" after 948 ms
- **N=8 away_note** PASS: wait text "Waiting for P8 (their game tab is in the background)" (tab hidden: emulated)
- **N=8 stall_prompt** PASS: "Waiting for P8 (their game tab is in the background)." with Keep waiting and "Leave the match"
- **N=8 keep_waiting** PASS: the prompt went away and stayed away
- **N=8 stall_recovery** FAIL: every tab moved on by two hashes: false; longest wait now 0 ms; only 1 frames compared (the match ran too short or too slowly)
- **N=8 connection_lost** FAIL: after the socket dropped: prompt "Waiting for P3.", button "Leave the match"
- **N=8 reconnect** PASS: way back: front panel says "Your last lobby was 3Z9U: Join puts you back in it."; Join mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=8 refusal_message_stays** FAIL: rejoin after dropped connection: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
