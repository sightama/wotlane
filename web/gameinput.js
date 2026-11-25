// ABOUTME: The page's own handling of the keyboard and mouse while the game runs: Tab is the game's (it opens the pause), so the browser
// ABOUTME: must not move focus with it, and the mouse cursor over the game hides after a moment without movement.
'use strict';

const IDLE_MS = 2000;
// the page's overlays over the game with buttons of their own: the stall prompt and the loading screen
const OVERLAYS = '#stall, #loading';

// Tab, with or without Shift: not the browser's or the system's own shortcuts (Ctrl+Tab, Alt+Tab, Cmd+Tab)
function isGameTab(e) {
  return e.key === 'Tab' && !e.ctrlKey && !e.altKey && !e.metaKey;
}

// Hides the cursor over `canvas` once the mouse has not moved for IDLE_MS, and shows it again on the next move. Mouse movement is the
// only thing it reads: a pointer lock (the game's camera) hides the cursor by itself
class IdleCursor {
  constructor({ canvas, setTimer, clearTimer }) {
    Object.assign(this, { canvas, setTimer, clearTimer });
    this.timer = 0;
  }

  start() {
    this.canvas.addEventListener('mousemove', () => this.wake());
    this.arm();
  }

  arm() {
    this.clearTimer(this.timer);
    this.timer = this.setTimer(() => { this.canvas.style.cursor = 'none'; }, IDLE_MS);
  }

  wake() {
    this.canvas.style.cursor = '';
    this.arm();
  }
}

// `running()`: whether the game has been started (the front panel is on screen until then, and Tab moves between its controls);
// `idleCursor`: false leaves the cursor alone (tests); it is also left alone on a touch screen
function installGameInput({ win, canvas, running, idleCursor = true }) {
  win.addEventListener('keydown', (e) => {
    if (!running() || !isGameTab(e) || e.target?.closest?.(OVERLAYS)) return;   // an overlay's buttons need Tab
    e.preventDefault();
    win.document?.querySelector('#loading:not([hidden]) button')?.focus();   // the loading screen's button: nothing to pause yet
  }, true);
  if (idleCursor && !win.matchMedia?.('(pointer: coarse)').matches)
    new IdleCursor({ canvas, setTimer: (fn, ms) => win.setTimeout(fn, ms), clearTimer: (id) => win.clearTimeout(id) }).start();
}

if (typeof module !== 'undefined' && module.exports) module.exports = { IDLE_MS, isGameTab, IdleCursor, installGameInput };
else Object.assign(globalThis, { wotmInstallGameInput: installGameInput });
