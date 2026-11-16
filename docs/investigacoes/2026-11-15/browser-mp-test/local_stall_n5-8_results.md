# Browser multiplayer results

- target: local http://127.0.0.1:56386/
- build: wl-82c5b21887ec9e7f
- browser: chromium headless 156.0.8078.4
- isolation: context
- host: 10 cores, load 23.3
- started: 2026-11-16T03:38:06.719Z
- players: 5,8

| check | N=5 | N=8 |
|---|---|---|
| stall_detected | FAIL | pass |
| away_note | FAIL | pass |
| stall_prompt | pass | pass |
| keep_waiting | pass | pass |
| stall_recovery | pass | FAIL |
| connection_lost | pass | FAIL |
| reconnect | pass | pass |
| refusal_message_stays | FAIL | FAIL |

## Details

- **N=5 stall_detected** FAIL: P1 says "Waiting for P3" after 3082 ms
- **N=5 away_note** FAIL: wait text "Waiting for P3" (tab hidden: emulated)
- **N=5 stall_prompt** PASS: "Waiting for P5 (their game tab is in the background)." with Keep waiting and "Leave the match"
- **N=5 keep_waiting** PASS: the prompt went away and stayed away
- **N=5 stall_recovery** PASS: every tab moved on by two hashes: true; longest wait now 0 ms; 2 frames compared, all equal on 5 tabs
- **N=5 connection_lost** PASS: after the socket dropped: prompt "The match stopped: connection to the server dropped.", button "Back to the front page"
- **N=5 reconnect** PASS: way back: front panel says "Your last lobby was K5MR: Join puts you back in it."; Join mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=5 refusal_message_stays** FAIL: rejoin after dropped connection: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
- **N=8 stall_detected** PASS: P1 says "Waiting for P8 (their game tab is in the background)" after 907 ms
- **N=8 away_note** PASS: wait text "Waiting for P8 (their game tab is in the background)" (tab hidden: emulated)
- **N=8 stall_prompt** PASS: "Waiting for P8 (their game tab is in the background)." with Keep waiting and "Leave the match"
- **N=8 keep_waiting** PASS: the prompt went away and stayed away
- **N=8 stall_recovery** FAIL: every tab moved on by two hashes: false; longest wait now 0 ms; only 1 frames compared (the match ran too short or too slowly)
- **N=8 connection_lost** FAIL: after the socket dropped: prompt "Waiting for P1, P4, P5, P6, P7.", button "Leave the match"
- **N=8 reconnect** PASS: way back: front panel says "Your last lobby was ZZAZ: Join puts you back in it."; Join mid-match: joined=false "Lost the connection to the game server. Reload the page to try again."
- **N=8 refusal_message_stays** FAIL: rejoin after dropped connection: the server said "lobby is in a match; wait for it to end", the panel then showed "Lost the connection to the game server. Reload the page to try again."
