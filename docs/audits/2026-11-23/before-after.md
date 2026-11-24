# playwotm.com UI pass 2: before and after (2026-11-23)

BEFORE is the page that is live on playwotm.com now (`web/` at `f1dfb377`, the deploy of #190 and #191). AFTER is this branch. Both sets were taken
by `tools/page_shots.mjs` against a local lobby server, with a second player in the lobby, in muted browsers, so the pictures differ only in
the page. Firefox is the main browser here; the touch layout is shot in Chrome, because Firefox's fullscreen request (made on the first tap)
changes the size of its emulated phone window. Safari is WebKit, which cannot run the game.

Phone is 390x844 at 2x; desktop is 1280x800; the touch layout is a phone held sideways, 844x390.

## Home

| | BEFORE | AFTER | What changed |
|---|---|---|---|
| Phone, Firefox | <img src="before-after/before-firefox-phone-home.png" width="240"> | <img src="before-after/after-firefox-phone-home.png" width="240"> | The long Firefox tip no longer sits above the controls (it pushed them down 114px when it appeared); one filled Single player button; the online choices sit under their own "Play online" heading; 44px controls. |
| Desktop, Firefox | <img src="before-after/before-firefox-desktop-home.png" width="420"> | <img src="before-after/after-firefox-desktop-home.png" width="420"> | Same structure, fields fill the card, buttons and fields are 34px with 8px corners, nothing moves while the page loads, and the status bar is no longer drawn under the panel. |

## Lobby

| | BEFORE | AFTER | What changed |
|---|---|---|---|
| Phone, Firefox | <img src="before-after/before-firefox-phone-lobby.png" width="240"> | <img src="before-after/after-firefox-phone-lobby.png" width="240"> | The lobby code is large with a label, next to Copy invite link; members are rows with their chips; Stage and Wins sit side by side; Rules is a labelled box with 20px checkboxes; Start (wide) is beside Leave; the chat comes last. |
| Desktop, Firefox | <img src="before-after/before-firefox-desktop-lobby.png" width="420"> | <img src="before-after/after-firefox-desktop-lobby.png" width="420"> | Same layout on the wider card; Start is the one filled button of the lobby. |

## Settings

| | BEFORE | AFTER | What changed |
|---|---|---|---|
| Phone, Firefox, Saves | <img src="before-after/before-firefox-phone-settings.png" width="240"> | <img src="before-after/after-firefox-phone-settings.png" width="240"> | Content is unchanged. Tabs and buttons are 44px, rounded, with visible field borders; the card sits at the top of the screen instead of the middle. |
| Desktop, Firefox, Saves | <img src="before-after/before-firefox-desktop-settings.png" width="420"> | <img src="before-after/after-firefox-desktop-settings.png" width="420"> | Same, with desktop-sized controls. |
| Phone, Firefox, Controls | <img src="before-after/before-firefox-phone-controls.png" width="240"> | <img src="before-after/after-firefox-phone-controls.png" width="240"> | Selects have 3:1 borders and a dark scheme; the key remap buttons keep their compact size, so the table does not run off a phone. |
| Desktop, Firefox, Controls | <img src="before-after/before-firefox-desktop-controls.png" width="420"> | <img src="before-after/after-firefox-desktop-controls.png" width="420"> | Same. |

The Mods tab (your creations, map editor) is not redesigned; it only picks up the shared control sizes, borders and the dark scheme.

## Touch layout

| | BEFORE | AFTER | What changed |
|---|---|---|---|
| Layout panel, Chrome | <img src="before-after/before-chrome-phone-touch-layout.png" width="420"> | <img src="before-after/after-chrome-phone-touch-layout.png" width="420"> | The panel's buttons are larger (8px corners, more padding), its checkboxes follow the dark scheme, and on a phone shorter than the panel it scrolls inside the screen instead of running past the bottom. The on-screen controls themselves are unchanged. |
| Move buttons, Chrome | <img src="before-after/before-chrome-phone-touch-layout-moving.png" width="420"> | <img src="before-after/after-chrome-phone-touch-layout-moving.png" width="420"> | Same panel in Move buttons mode (dashed outlines on the controls). |

## Safari (cannot run the game)

| | BEFORE | AFTER | What changed |
|---|---|---|---|
| iPhone | <img src="before-after/before-webkit-phone-home.png" width="240"> | <img src="before-after/after-webkit-phone-home.png" width="240"> | A dead form (empty monster list, dimmed buttons, one grey line) becomes the reason in a bordered note and nothing to press. |
| Safari on a Mac | <img src="before-after/before-webkit-desktop-home.png" width="420"> | <img src="before-after/after-webkit-desktop-home.png" width="420"> | Same: the message ("This game runs in Chrome, Firefox or Edge...") is the whole panel. |

## Measured

| | BEFORE | AFTER |
|---|---|---|
| Firefox, controls that move after first paint | name, monster, Single player, Host, Join, Settings (86px desktop, 114px phone, at ~800ms) | none (only the footer, below everything, when the Firefox tip appears) |
| Chrome, controls that move after first paint | the lobby list moved everything 34px (desktop) and 53px (phone) | none |
| Dead presses | Single player, Host and Join were clickable from first paint, before their handlers attached (~100 to 800ms) | disabled until the handlers are attached |
| Field borders against the panel | 1.27:1 | 3.4:1 |
| Controls on a touch device | 24 to 38px | 44px |
| axe-core (4.10.2), home, lobby and six settings tabs, Firefox and Chrome | `region` (no landmark) on every view | no violations |
| `tools/page_check.mjs` | 39 of 58 in Firefox, 39 of 58 in Chrome, 4 of 8 in WebKit | 58 of 58 in Firefox, Chrome and Brave; 8 of 8 in WebKit |

## What was not tested

- No real screen reader (VoiceOver, NVDA, TalkBack). The accessibility tree was read through Playwright's snapshot and axe, which finds a subset of problems.
- A touch laptop (mouse as the main pointer, touch screen as another): no browser here can emulate that pair, so the `any-pointer: coarse` rule is checked by a text test only.
- Real phones, real Safari and real touch drags. The phones are emulated; WebKit is Playwright's build.
- The game itself (the pages were shot without starting it) and an online match.
