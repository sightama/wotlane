# playwotm.com: Modern Web Guidance audit (2026-11-23)

Scope: the page files in `web/` on `origin/wip/browser` (commit `0b331cf6`): `index.html`, `front.js`, `settings_ui.js`, `saves_ui.js`, `report_ui.js`, `lobby.js` and friends. The game itself is a canvas driven by the Emscripten module, so this audit covers the page around it, not the in-game UI. Read-only, no code changed.
Paths are relative to the repo root.

## Method and tooling

- Tool: Modern Web Guidance 1.0.2 (`GoogleChrome/modern-web-guidance-src`), run with `DISABLE_TELEMETRY=1`.
- `legacy-scan web --json`: 37 files scanned, 3 findings (listed below). It covers about 20% of the guide corpus, so a short list is not a clean bill of health.
- Guides read in full for this report: `dialog` (as retrieved for the cifdt audit), `ime-safe-enter-submit`, `accessibility`, `motion`. Search-description only: `tabs`, `dark-mode`, `js`. Where I cite a guide I name the section.
- Live checks in Chromium (muted with `--mute-audio`; the page was loaded and the intro movie started once for screenshots, with no audio): element sizes, `labels` collections, computed styles. I did not use a screen reader.
- "Guide: none" means no Modern Web Guidance guide covers it; it is my own observation or a WAI-ARIA Authoring Practices point.

Severity: **High** blocks or misleads real users, **Medium** degrades a real flow, **Low** polish.

## Summary

| # | Sev | Area | Where | Guide |
|---|-----|------|-------|-------|
| 1 | High | Two of three lobby rule checkboxes have no label (verified live) | `web/index.html:157-158` | `accessibility` (sec 3) |
| 2 | High | Positive `tabindex="1"` on the game canvas | `web/index.html:122` | `accessibility` (sec 5, Mandatory) |
| 3 | Medium | Chat Enter-to-submit ignores IME composition | `web/front.js:577` | `ime-safe-enter-submit` |
| 4 | Low | Join code Enter handler has the same shape | `web/front.js:563` | `ime-safe-enter-submit` |
| 5 | Medium | "Delete all saves" confirm focuses the destructive button; focus is lost on dismiss | `web/saves_ui.js:29-38` | `dialog`, `accessibility` (sec 5) |
| 6 | Medium | Report card and settings card do not return focus consistently | `web/report_ui.js:44-55` | `accessibility` (sec 5) |
| 7 | Medium | Contrast: dim text 4.2:1, white on accent 3.7:1 (verified) | `web/index.html:32,55,66,69` | `color` (pointer), detector |
| 8 | Medium | Bottom status bar at half opacity: about 2:1 effective contrast, 23px controls | `web/index.html:50-52,126-127` | `accessibility` (target sizes) |
| 9 | Medium | Touch targets 23 to 26px high on a phone (verified) | `web/index.html:53,55` | `accessibility` (target sizes) |
| 10 | Medium | No `color-scheme: dark`; native controls render light | `web/index.html:32-35` | `dark-mode` (pointer) |
| 11 | Low | No `<main>` landmark; three `<h1>`s across hidden views | `web/index.html:129-228` | `accessibility` (sec 1) |
| 12 | Low | `role="status"` with `aria-live="off"` | `web/index.html:122` | `accessibility` (sec 2) |
| 13 | Low | Settings tabs: no Home/End keys | `web/settings_ui.js:139-146` | none (APG tabs pattern) |
| 14 | Low | Hand-rolled `role="alertdialog"` inline confirm | `web/index.html:186` | `dialog` |
| 15 | Low | Reduced motion: nothing to handle on the page; question for cutscenes | `web/index.html:51`, `web/movies.js` | `motion` |
| 16 | Low | Copy-then-sort array pattern | `web/rate.js:22` | `js` |
| 17 | Low | 17 classic `<script src>` tags, none `defer`/`async` | `web/index.html:230-246` | none |

## Findings

### 1. Rule checkboxes: only "Jump" has a label (High, verified live)

`web/index.html:157-158` wraps all three checkboxes in one `<label>`: `<label><span><input id="mod_jump"> Jump <input id="mod_fast"> Fast <input id="mod_turbo"> Turbo</span></label>`. A label labels only its first labelable descendant. In Chromium on the live page, `mod_jump.labels.length` is 1 and `mod_fast.labels.length` and `mod_turbo.labels.length` are both 0, so Fast and Turbo are announced as unnamed checkboxes by assistive tech. Clicking the words "Fast" or "Turbo" also toggles Jump instead. `accessibility` guide section 3: every interactive element needs an accessible name. Fix: one `<label>` per checkbox inside a `<fieldset><legend>Rules</legend>` (mockup 2).

### 2. Positive tabindex on the canvas (High)

`web/index.html:122`: `<canvas id="canvas" tabindex="1" ...>`. `accessibility` guide section 5: "Mandatory - Don't use positive `tabindex` values" (WCAG 2.4.3). A value of 1 puts the canvas first in the tab order ahead of the whole front panel, on every page state, including while the front panel is covering it (`#front` is `position: fixed` over `#stage`). Use `tabindex="0"` or `-1` and call `canvas.focus()` when the game starts. I did not check whether `front.js` or the game depends on the canvas being first; test before changing.

### 3. Chat Enter submits during IME composition (Medium)

`web/front.js:577`: `$('chatText').addEventListener('keydown', (e) => { if (e.key === 'Enter') sendChat(); });`. Flagged by `legacy-scan`. Japanese, Chinese and Korean players pressing Enter to confirm a conversion send a half-built chat message to the lobby. `ime-safe-enter-submit` guide: check `event.isComposing` before submitting (and `preventDefault()`), or use a `<form>` and let native implicit submission handle it. The chat input at `web/index.html:163` is not inside a form today.

### 4. Join code Enter handler (Low)

`web/front.js:563` has the same pattern on `#joinCode`. The code is 4 Latin characters (`maxlength="4"`, uppercased), so IME impact is small; fix it with the same one-line guard for consistency.

### 5. Destructive confirm focuses the destructive action (Medium)

`web/saves_ui.js:29-35`: `confirm(action)` always runs `$('savesConfirmOk').focus()`, including for "Delete all saves" (`web/index.html:185`). `dialog` guide: "DO put `autofocus` on the safe action (such as Cancel or Keep) in a destructive confirmation." Also `settle()` (`:36-39`) hides the confirm block without moving focus, so focus falls back to the page after Cancel or OK; the guide says focus should return to the invoker (`accessibility` section 5: "when dismissed, returns to the element that originally triggered"). Fix: focus Cancel for delete and restore focus to the button that opened the confirm.

### 6. Inconsistent focus return between cards (Medium)

`web/settings_ui.js:121` returns focus to `#openSettings` on close, which is correct. `web/report_ui.js:49-55`'s `close()` does not restore focus at all: after "Back" the focused button is hidden and focus resets to the document. The report card opens from two places (`index.html:169` and `:197`, `report_ui.js:53-54`), so the opener has to be remembered (`from` already is, for visibility). Similarly the front panel views (`#home`, `#settings`, `#report`) swap with the `hidden` attribute and do not announce the change. Native `<dialog>` for settings and report would give focus return and Esc for free (see `dialog` guide), though the settings card also handles Esc itself (`settings_ui.js:165`) and has a rule about select/input Esc that would need preserving.

### 7. Contrast (Medium, verified)

Computed from the page's CSS variables (`web/index.html:32`):
- Dim text `#7a7a82` on panel `#16161a`: 4.24:1; on page bg `#0b0b0d`: 4.62:1. Used for labels, notes, hints and the legal footer (`:66`, `:69`, `:99`, `:106`, `:114`), mostly at 11 to 12px. Needs 4.5:1; fails on the panel.
- White on accent `#e0592a` (`.main` buttons, `:55`): 3.73:1, below 4.5:1 for 14px semi-bold text.
- Impeccable's live scan agrees: the same three pairs plus the `CODE` placeholder (4.2:1).
- Suggested values from my own calculation: dim `#8b8b94` gives 5.34:1 on the panel; button background `#c8461a` gives 4.83:1 with white text; the original `#e0592a` is still fine as text/ornament on the panel (4.84:1).

### 8. Status bar is nearly invisible (Medium, verified)

`web/index.html:50-52`: `#bar` has `opacity: .5` and dim text, only reaching full opacity on hover or focus-within. Effective contrast of the dim text at half opacity over the dark page is about 2.0:1 by my calculation (see the near-invisible "Choose how to play / Fullscreen / L" in the bottom corner of `screenshots/home-phone.png`). Its buttons measure 23px high (`Fullscreen` 76x23, `L` 25x23) in Chromium at 390px. On touch there is no hover, so the bar stays dim. During loading it is also the only place `#status` appears. Mockup 3 shows a full-opacity bar with 44px controls.

### 9. Target sizes (Medium, verified)

`accessibility` guide, target sizes: at least 24x24 CSS px (WCAG 2.5.8), bumped on `@media (pointer: coarse)`. Measured at 390px: Host online 91x26, Join 45x26, Refresh 68x26, Settings 71x26, status-bar buttons 23px high. `button { padding: 2px 8px }` at `web/index.html:53` is the root cause; `.main` is 38px, so no button reaches 44px. Fix in one place: `button { min-height: 44px }` inside `@media (pointer: coarse)`, or globally.

### 10. Native controls render light (Medium)

`document.documentElement` computed `color-scheme` is `normal` on the live page. The page is dark by hand-set colors (`web/index.html:32-35`), but select popups, checkboxes, scrollbars and the file inputs follow the light scheme unless declared. `dark-mode` guide (read by description only): declare `color-scheme: dark` on `:root`. One line.

### 11. Landmarks and headings (Low)

No `<main>` (`accessibility` section 1: structure with landmarks); `front` is a `div`. There are three `<h1>`s (`index.html:131,178,221`), one per view, only one visible at a time, which is acceptable, but the lobby view has no heading for its content at all.

### 12. `role="status"` with `aria-live="off"` (Low)

`web/index.html:122` (`#streaming`): `role="status"` implies polite live announcements, and `aria-live="off"` turns them off, so the role is inert. If silence is intended, drop the role; if the text matters (it appears when the game waits on a load), let it announce at most once per phase.

### 13. Settings tabs lack Home/End (Low)

`web/settings_ui.js:127,139-146` is a good roving-tabindex tablist (Left/Right wrap, `aria-selected`, `tabIndex` 0 or -1, panels labelled by tabs). The WAI-ARIA Authoring Practices tab pattern also recommends Home and End; not present. No MWG guide found for this (the `tabs` guide I found is about responsive tab layout).

### 14. Inline `role="alertdialog"` (Low)

`web/index.html:186` is a `div role="alertdialog"` shown with `hidden`. It does not trap focus or make the rest inert, which is acceptable for an inline confirm, but `role="alertdialog"` promises a modal. Either make it a real modal `<dialog>` (`dialog` guide, Rule 1) or use `role="group"` with a live message.

### 15. Reduced motion (Low, question)

The only CSS motion in the page is `transition: opacity .2s` on `#bar` (`index.html:51`), which `motion` guide treats as safe (non-spatial). There is no `prefers-reduced-motion` handling anywhere in `web/` (I searched `*.js` and `index.html`). The game's cutscenes play in `<video id="movieVideo">` (`index.html:123`), skippable with Esc, Space, Enter or a tap. I do not know whether any movie has flashing content, or whether captions or a transcript exist; `accessibility` guide media section would ask for captions on the dialogue movies. Not checked.

### 16. Copy-then-sort (Low)

`web/rate.js:22`: `[...xs].sort((a, b) => a - b)` becomes `xs.toSorted((a, b) => a - b)` (`js/copying-array-methods`).

### 17. Scripts without defer (Low)

`web/index.html:230-246` loads 17 classic scripts at the end of `<body>`, then `front.js` as a module. At the end of the body they do not block first paint of the markup, and I did not find the load order is relied on elsewhere, so this is hygiene only. No MWG guide covers it.

## What is already good

- Settings tablist is a proper roving-tabindex pattern, panels have `aria-labelledby` (`settings_ui.js:127-146`, `index.html:180-210`).
- Name, monster and chat inputs have visible labels or `aria-label`s (`index.html:135-143,151,163,194,206-207`); `autocomplete="nickname"` is set.
- Live regions for lobby warnings, chat log and report status (`index.html:153,162,226`); `[hidden]` is enforced (`:34`).
- `lang`, viewport, description, canonical, OG and JSON-LD are all in place (`index.html:4-29`); `overscroll-behavior: none` and `playsinline` are set.
- Restrained, stable CSS with custom properties and no framework weight.

## Reproduce

```sh
export DISABLE_TELEMETRY=1
npx -y modern-web-guidance@latest legacy-scan web --json
npx -y modern-web-guidance@latest retrieve ime-safe-enter-submit
```
