// ABOUTME: Tests for web/gameinput.js: Tab belongs to the game while it runs (the browser must not move focus), and the mouse cursor
// ABOUTME: over the game hides after a moment without movement and shows again on the next. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { isGameTab, IdleCursor, installGameInput, IDLE_MS } = createRequire(import.meta.url)('./gameinput.js');

test('isGameTab: Tab, with or without Shift, and no other key or shortcut', () => {
  assert.equal(isGameTab({ key: 'Tab', ctrlKey: false, altKey: false, metaKey: false }), true);
  assert.equal(isGameTab({ key: 'Tab', shiftKey: true, ctrlKey: false, altKey: false, metaKey: false }), true);
  assert.equal(isGameTab({ key: 'Tab', ctrlKey: true }), false);   // the browser's own tab switching
  assert.equal(isGameTab({ key: 'Tab', altKey: true }), false);    // Alt+Tab is the system's
  assert.equal(isGameTab({ key: 'Tab', metaKey: true }), false);
  assert.equal(isGameTab({ key: 'Escape' }), false);
  assert.equal(isGameTab({ key: 'a' }), false);
});

// a window stand-in that records listeners and lets the test fire them
function fakeWindow() {
  const listeners = {};
  return {
    listeners,
    addEventListener: (type, fn) => { (listeners[type] ??= []).push(fn); },
    fire: (type, event) => (listeners[type] ?? []).forEach((fn) => fn(event)),
  };
}

test('installGameInput stops the browser moving focus on Tab, only while the game runs', () => {
  const win = fakeWindow();
  let running = false;
  installGameInput({ win, canvas: fakeCanvas(), running: () => running, idleCursor: false });
  const tab = () => { const e = { key: 'Tab', defaultPrevented: false, preventDefault() { this.defaultPrevented = true; } }; win.fire('keydown', e); return e.defaultPrevented; };
  assert.equal(tab(), false);   // the front panel: Tab moves between its controls
  running = true;
  assert.equal(tab(), true);
});

test('installGameInput leaves Tab to the page\'s own overlay (the stall prompt\'s buttons) while the game runs', () => {
  const win = fakeWindow();
  installGameInput({ win, canvas: fakeCanvas(), running: () => true, idleCursor: false });
  const press = (target) => { const e = { key: 'Tab', target, defaultPrevented: false, preventDefault() { this.defaultPrevented = true; } }; win.fire('keydown', e); return e.defaultPrevented; };
  assert.equal(press({ closest: (sel) => (sel === '#stall' ? {} : null) }), false);   // a button inside #stall
  assert.equal(press({ closest: () => null }), true);                                  // the canvas
  assert.equal(press(undefined), true);
});

function fakeCanvas() {
  const listeners = {};
  return {
    style: { cursor: '' },
    addEventListener: (type, fn) => { (listeners[type] ??= []).push(fn); },
    fire: (type) => (listeners[type] ?? []).forEach((fn) => fn({})),
  };
}

function fakeTimers() {
  let next = 1;
  const pending = new Map();
  return {
    set: (fn, ms) => { pending.set(next, { fn, ms }); return next++; },
    clear: (id) => pending.delete(id),
    pending,
    runAll() { for (const [id, t] of [...pending]) { pending.delete(id); t.fn(); } },
  };
}

test('IdleCursor hides the cursor after IDLE_MS without a mouse move and shows it on the next move', () => {
  const canvas = fakeCanvas();
  const timers = fakeTimers();
  const cursor = new IdleCursor({ canvas, setTimer: timers.set, clearTimer: timers.clear });
  cursor.start();
  assert.equal(IDLE_MS, 2000);
  assert.deepEqual([...timers.pending.values()].map((t) => t.ms), [IDLE_MS]);   // armed at the start
  timers.runAll();
  assert.equal(canvas.style.cursor, 'none');
  canvas.fire('mousemove');
  assert.equal(canvas.style.cursor, '');
  assert.equal(timers.pending.size, 1);   // armed again
  timers.runAll();
  assert.equal(canvas.style.cursor, 'none');
});

test('IdleCursor: each move restarts the wait, so a moving mouse keeps its cursor', () => {
  const canvas = fakeCanvas();
  const timers = fakeTimers();
  new IdleCursor({ canvas, setTimer: timers.set, clearTimer: timers.clear }).start();
  canvas.fire('mousemove');
  canvas.fire('mousemove');
  assert.equal(timers.pending.size, 1);   // the earlier waits were cancelled
  assert.equal(canvas.style.cursor, '');
});

test('IdleCursor leaves the cursor alone on a touch screen (installGameInput with a coarse pointer)', () => {
  const canvas = fakeCanvas();
  const timers = fakeTimers();
  const win = Object.assign(fakeWindow(), { matchMedia: () => ({ matches: true }), setTimeout: timers.set, clearTimeout: timers.clear });
  installGameInput({ win, canvas, running: () => true });
  assert.equal(timers.pending.size, 0);
  assert.equal(canvas.style.cursor, '');
});

test('installGameInput starts the idle cursor on a desktop', () => {
  const canvas = fakeCanvas();
  const timers = fakeTimers();
  const win = Object.assign(fakeWindow(), { matchMedia: () => ({ matches: false }), setTimeout: timers.set, clearTimeout: timers.clear });
  installGameInput({ win, canvas, running: () => true });
  assert.equal(timers.pending.size, 1);
});
