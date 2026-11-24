# QUIT never returned to the front panel (Firefox, live smoke BJJU)

## What happened
In live smoke BJJU the Firefox player's (P2) page stayed in the game after QUIT. `wotmQuit` reloaded only when
`Promise.all([wotmSaveQueue, finishDiag()])` settled, and `finishDiag` waits for the lobby socket to drain in `flushSocket` with
`setTimeout(20)` steps. That page's timers were not running, so the promise never settled and `location.reload()` never ran.

## Harness faults that hid it
- The quick rematch check waited for `#inLobby` to be visible. `#inLobby` sits inside `#front`, so it passed with the game still
  covering a hidden front panel. It now needs `#front` visible too (`backInLobbyScript`).
- After QUIT, `hasStart` read the whole log when the page had not reloaded, so the old match's start line counted as the rematch.
  A rematch start now needs the reload marker (`startMatch(players, { afterReload: true })`).
- `toGameViewport` swallowed `setViewportSize` failures. It now throws, naming the player and the size.

## Fix
`web/reload.js` (`wotmReloadWhenDone`): reload when the work settles or after 2 s, the deadline checked from MessageChannel
tasks. Used by `wotmQuit` and Leave the match. `tools/browser_mp_test/quit_starved.mjs` proves it in Firefox, Chromium and Chrome
with `setTimeout`, `setInterval` and `requestAnimationFrame` disabled (reload after about 2 s each, also with a second tab in front).

## Why the timers were starved: not established
Tried to reproduce with `tools/browser_mp_test/timer_heartbeat.mjs` (a 100 ms `setTimeout` heartbeat beside a requestAnimationFrame
loop that burns 10, 17 or 40 ms a frame), Playwright's Firefox, two contexts in one browser, 30 to 45 s each:
- headless, 2 tabs, burn 10 / 17 / 40 ms: mean lateness 11 to 23 ms, max 40 to 119 ms. No throttling, no starvation.
- headless, tab 1 brought to the front when tab 2 opens: the same.
- headed, 2 tabs: mean 16 ms, max 231 ms, `document.hidden` false on both.
So background-tab throttling and a saturated main thread, at least in this layout on this Mac, do not starve Firefox's timers. What is
left: a window that is covered or minimised for a long time in a headed run, the machine at load above 60 while several browsers
play, or Firefox's own power throttling. The diag now has a probe that answers it in the next live run: every report carries
`tmr_ms` and `tmr_max_ms` (how late a `setTimeout(0)` fires), beside `lag_ms` and `lag_max_ms` (a MessageChannel task). A starved
page shows a large `tmr_max_ms` with a small `lag_max_ms`.
