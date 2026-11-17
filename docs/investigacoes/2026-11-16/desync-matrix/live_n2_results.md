# Browser multiplayer results

- target: live https://playwotm.com/
- build: wl-3e9ee2e7bacf48de
- browser: chromium headless 156.0.8078.4
- isolation: context
- host: 10 cores, load 10.9
- started: 2026-11-17T21:22:05.176Z
- players: 2

| check | N=2 |
|---|---|
| lobby | pass |
| lobby_errors | pass |
| lobby_ping | pass |
| lobby_chat | pass |
| lobby_list | pass |
| lobby_leave | pass |
| invite_link | pass |
| picks | pass |
| match_start | pass |
| inputs | pass |
| sync | pass |
| health | pass |
| perf | info |
| late_join | pass |
| leave | pass |
| refusal_message_stays | pass |
| round_end | pass |
| win_screen | pass |
| rematch | pass |
| quit | pass |
| rejoin_after_match | pass |

## Details

- **N=2 lobby** PASS: 2 tabs list 2 members, code ZKC7
- **N=2 lobby_errors** PASS: duplicate name: "Someone in that lobby already has that name. Pick another."; wrong code: "No lobby has that code. Check the code and try again."; short code: "The code has 4 characters (letters and digits)."
- **N=2 lobby_ping** PASS: every tab shows a ping for all 2 members: true,true; ["P1 (Togera), 27 ms, owner","P2 (Congar), 42 ms"]
- **N=2 lobby_chat** PASS: every tab shows the line: true,true; markup stayed text: true; a burst is refused with a message: true
- **N=2 lobby_list** PASS: rows: [{"text":"P1: 2/8 (P1, P2), waiting","join":true}]; listed with the right Join state: true; Join from the list worked: true
- **N=2 lobby_leave** PASS: joined: true; the choice is back: true; no reload: true; the others saw it go: true; buttons usable again: true
- **N=2 invite_link** PASS: link "https://playwotm.com/?join=ZKC7" right: true; code filled in with a message: true; address cleaned: true; new profile joined after typing a name: true ("Waiting for the owner to start."); profile with a name joined by itself: true
- **N=2 picks** PASS: monster change on all tabs: true; guests see stage and wins: true (Stage: Gamblers Gulch, wins: 3)
- **N=2 match_start** PASS: all 2 tabs started (delay 2 frames)
- **N=2 inputs** PASS: every tab saw each of the 2 players' own held button (first input applied on 2/2 tabs)
- **N=2 sync** PASS: 9 frames compared, all equal on 2 tabs
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
- **N=2 health** PASS: no crash, stop or page error on any tab
- **N=2 perf** INFO: game fps (full speed 60) min 55.8 avg 55.8 (lockstep: all tabs run at the slowest pace); browser CPU 7.47 of 10 cores (renderers 0.00,0.54,0.54; other processes browser 0.04, GPU 6.31, network.mojom.NetworkService 0.03, audio.mojom.AudioService 0.01); host load 12.9; network waits 6051 ms max per tab
  - n2/match/P1.log
  - n2/match/P1.png
  - n2/match/P2.log
  - n2/match/P2.png
- **N=2 late_join** PASS: late join: joined=false message "That lobby is in a match. Wait for it to end, then join."
- **N=2 leave** PASS: after P2 closed its tab: others advanced=true, stopped=[], waiting=, server logged the leave=true
- **N=2 refusal_message_stays** PASS: every refusal stayed on screen
- **N=2 round_end** PASS: round 1 ended on 2/2 tabs
- **N=2 win_screen** PASS: the win dialog opened on all 2 tabs
- **N=2 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 2 tabs
- **N=2 quit** PASS: QUIT took every tab back to the front panel
- **N=2 rejoin_after_match** PASS: front panels say ""; Join on the old code: joined=false "No lobby has that code. Check the code and try again." (shown after: "No lobby has that code. Check the code and try again.")
