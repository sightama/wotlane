# Browser touch and gamepad test

Drives the page in a phone-sized Chromium (998x448 CSS pixels, device scale 2.25, a Pixel 9 Pro XL in landscape; touch and multi-touch
through the DevTools `Input.dispatchTouchEvent`) and with a mocked Gamepad API, against the real game. The game's own pad log
(`PS2X_PADLOG`) and monster log (`WOTM_TURNLOG`) say what it read and what moved. The exit code is 1 when any check fails.

## Run

```
sh tools/build_web.sh                                  # builds <runtime>/web_dist (docs/passos/WEB.md)
node tools/browser_touch_test/run.mjs --site <web_dist> --data <folder with game.iso and SCUS_971.97>
node tools/browser_touch_test/run.mjs --only gamepads,fight_control
```

`--help` lists the options. It starts `tools/serve_web.py` on a free port and stops it at the end. Playwright comes from
`tools/browser_mp_test/node_modules` or the global npm root. Results (`results.md`, `results.json`, screenshots) go to
`<runtime>/browser_touch_test/<time>/`, or `--out`. Run it on a quiet machine: a fight is CPU-bound and a loaded one starves the page.

## Scenarios

| scenario | checks |
|---|---|
| `front_phone` | an Android phone is not refused, the Controls card lists the touch controls, no controls over the front panel; an iPhone gets a plain reason and disabled play buttons |
| `multitouch` | stick and Cross held together, a three-finger chord, release of everything, the game's pad log sees the stick, a 20 ms tap is read, the look zone is the right stick, no pointer lock |
| `fight_control` | in a fight against the AI the player's monster stands still, moves and turns when the touch stick is held, and stops when it is released |
| `overlay_behaviour` | a key press hides the controls and frees the pad slot, a touch brings them back, the gear panel resizes them and the choice is remembered |
| `gamepads` | a standard pad (toast, player 1, the game reads its stick), a second non-standard pad (player 2, its layout mapped, its hat as the d-pad), unplugging (toast), the touch controls return |

A test-only `Module` setter adds the game's environment; nothing in `web/` changes. A locked pointer zeroes every touch's coordinates in
Chromium, which is why `multitouch` checks that the page never lets the game take it.
