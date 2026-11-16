# Browser multiplayer results

- target: live https://35-185-58-174.sslip.io/
- build: wl-82c5b21887ec9e7f
- browser: chromium headless 156.0.8078.4
- isolation: context
- host: 10 cores, load 11.4
- started: 2026-11-16T04:00:16.639Z
- players: 2

| check | N=2 |
|---|---|
| round_end | pass |
| win_screen | pass |
| rematch | pass |
| quit | pass |
| rejoin_after_match | pass |

## Details

- **N=2 round_end** PASS: round 1 ended on 2/2 tabs
- **N=2 win_screen** PASS: the win dialog opened on all 2 tabs
- **N=2 rematch** PASS: round 2 ended and its win dialog opened on all tabs; 4 frames compared, all equal on 2 tabs
- **N=2 quit** PASS: QUIT took every tab back to the front panel
- **N=2 rejoin_after_match** PASS: front panels say ""; Join on the old code: joined=false "no such lobby (check the code)" (shown after: "no such lobby (check the code)")
