# Decisions

Choices made without asking, with the reason. Newest last.

## Loading screen and front page (#226)
- **The tip changes on a button, never by itself.** An auto-rotating tip is moving content that lasts longer than five seconds next to other content (WCAG 2.2.2), and it would need a pause control. A fixed random tip plus "Another tip" needs neither.
- **Time left comes from the last 5 s of network chunks.** Under one second of samples, once the expected size is in, and from the cache it says nothing instead of guessing. The expected size is the measured first-visit download (27 MB title, 57 MB fight), so the number is "about".
- **The tips for controls are built from `web/controls.js`.** A rebound default cannot make a tip lie, and a test fails when a tip names keys the table does not list.
- **One inline SVG sprite, no files.** `tools/build_web.sh` copies only `*.html`, `*.js` and `*.json`, and a drawing in the page keeps rule 1 (no game art) easy to audit.
- **The front card fades in and does not move.** The page checks hold that no control moves after first paint, so a translate on the card failed them.
- **Tab reaches Another tip while the screen is up.** Tab is the game's pause key and the page swallows it, so `web/gameinput.js` moves focus to the screen's button on Tab while `#loading` is visible (nothing can be paused then) and leaves Tab alone inside it.
- **The progress bar's indeterminate sweep slows to 10 s under reduced motion instead of stopping.** A stopped bar reads as a hang and the sweep ends with the first chunk; it is status, not decoration.
- **A short screen gets a tighter card.** On a phone held sideways at 568x320 the full card was 368 px tall; the kicker goes and the type and gaps shrink.
- **No time left once any data came from the cache.** The network rate says nothing about what remains then. The first-visit sentence stays (its #64 wording and tests).
- **The START tip is left out.** Tab is START by default and the loading screen gives Tab to its button.
- **`LoadingProgress.text()` stays.** It is the old one-line form with its tests; the screen uses `view()`.

## Match result card (#229)
- **Only counters the game stores.** Rounds won per fighter, the wins target and rounds played are in the game's state; damage dealt, KOs and a best combo are not (Hud::comboHits is the current combo). Reading them per drawn frame would differ between clients, so the card has none of them.
- **The score is read where presentation already runs.** `Scene::takeSnapshot` is in `PRESENTATION`, and `wotm_matchresult.hpp` joins it there, so no simulation file changes and the simulation id stays. `native/wotm_webfs.cpp` is hashed into it and is left alone.
- **No player names on the card.** The issue forbids a user name, and the monsters' names say who is who. Two fighters of one monster are numbered by slot.
- **The button, not an automatic card.** A button reads game state only when pressed and does nothing to the quit and rematch path (#196), which reloads the page.
- **The final frame is copied in the next animation frame.** A WebGL buffer is only valid until it is presented; a blank copy gives a card without a picture rather than a black one.
