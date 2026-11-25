# Decisions

Choices made without asking, with the reason. Newest last.

## Loading screen and front page (#226)
- **The tip changes on a button, never by itself.** An auto-rotating tip is moving content that lasts longer than five seconds next to other content (WCAG 2.2.2), and it would need a pause control. A fixed random tip plus "Another tip" needs neither.
- **Time left comes from the last 5 s of network chunks.** Under one second of samples, once the expected size is in, and from the cache it says nothing instead of guessing. The expected size is the measured first-visit download (27 MB title, 57 MB fight), so the number is "about".
- **The tips for controls are built from `web/controls.js`.** A rebound default cannot make a tip lie, and a test fails when a tip names keys the table does not list.
- **One inline SVG sprite, no files.** `tools/build_web.sh` copies only `*.html`, `*.js` and `*.json`, and a drawing in the page keeps rule 1 (no game art) easy to audit.
- **The front card fades in and does not move.** The page checks hold that no control moves after first paint, so a translate on the card failed them.
