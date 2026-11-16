// ABOUTME: Tests for web/touch.js: the on-screen layout (every PS2 button reachable, nothing overlapping or off screen on phone screens),
// ABOUTME: the remembered settings and the Controls card's touch rows. The overlay's DOM and touches are tested in the browser. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { LAYOUT, SCALE_RANGE, unitFor, rectOf, clampSettings, loadSettings, saveSettings, touchRows, ZONE, isTap } = require('./touch.js');
const { STD } = require('./pads.js');

const buttons = LAYOUT.filter((b) => b.press.length > 0);
const SCREENS = [[998, 448], [915, 412], [800, 360], [667, 375], [1280, 720]];   // landscape phones (Pixel 9 Pro XL first), a desktop window

// round controls are circles (a thumb hits the circle, not its bounding box); the rest are rectangles
function touching(a, b) {
  const centre = (r) => [(r.left + r.right) / 2, (r.top + r.bottom) / 2];
  if (a.round && b.round) {
    const [ax, ay] = centre(a), [bx, by] = centre(b);
    return Math.hypot(ax - bx, ay - by) < (a.right - a.left) / 2 + (b.right - b.left) / 2;
  }
  if (!a.round && !b.round) return !(a.right <= b.left || b.right <= a.left || a.bottom <= b.top || b.bottom <= a.top);
  const [c, r] = a.round ? [a, b] : [b, a];
  const [cx, cy] = centre(c);
  const nx = Math.min(r.right, Math.max(r.left, cx)), ny = Math.min(r.bottom, Math.max(r.top, cy));
  return Math.hypot(cx - nx, cy - ny) < (c.right - c.left) / 2;
}

test('every button of the PS2 pad the game reads is on screen, alone or as a chord', () => {
  const pressed = new Set(buttons.flatMap((b) => b.press));
  for (const name of ['CROSS', 'CIRCLE', 'SQUARE', 'TRIANGLE', 'L1', 'R1', 'L2', 'R2', 'START', 'SELECT'])
    assert.ok(pressed.has(STD[name]), `${name} has a control`);
});

test('the chords are the game\'s own combinations: the specials are two face buttons, lock-on is L1 and R1', () => {
  const chord = (id) => buttons.find((b) => b.id === id).press.slice().sort((a, b) => a - b);
  assert.deepEqual(chord('special1'), [STD.CIRCLE, STD.TRIANGLE].sort((a, b) => a - b));
  assert.deepEqual(chord('special2'), [STD.CROSS, STD.SQUARE].sort((a, b) => a - b));
  assert.deepEqual(chord('lock'), [STD.L1, STD.R1]);
});

test('ids are unique, every control says what it does, and presses are standard button indexes', () => {
  assert.equal(new Set(LAYOUT.map((b) => b.id)).size, LAYOUT.length);
  for (const b of LAYOUT) assert.ok(b.does && b.label, `${b.id} has a label and a description`);
  for (const b of buttons) for (const i of b.press) assert.ok(Number.isInteger(i) && i >= 0 && i <= 16, `${b.id}: ${i}`);
});

for (const [w, h] of SCREENS) {
  for (const scale of [SCALE_RANGE.min, 1, SCALE_RANGE.max]) {
    test(`${w}x${h} at size ${scale}: controls stay on screen and never overlap each other`, () => {
      const u = unitFor(h, scale);
      const shapes = LAYOUT.map((b) => ({ id: b.id, round: ['cross', 'square', 'triangle', 'circle', 'chord'].includes(b.cls), ...rectOf(b, w, h, u) }));
      for (const r of shapes) assert.ok(r.left >= 0 && r.top >= 0 && r.right <= w && r.bottom <= h, `${r.id} is on screen`);
      for (let i = 0; i < shapes.length; i++)
        for (let j = i + 1; j < shapes.length; j++)
          assert.ok(!touching(shapes[i], shapes[j]), `${shapes[i].id} and ${shapes[j].id} overlap`);
    });
  }
}

test('the stick zone is the left of the screen and the look zone the right, both below the top row of buttons', () => {
  assert.ok(ZONE.split > 0.3 && ZONE.split < 0.6);
  assert.ok(ZONE.top > 0 && ZONE.top < 0.3);
});

test('settings are clamped to what the overlay can show, and a broken record falls back to the defaults', () => {
  assert.deepEqual(clampSettings({ scale: 9, opacity: -1, haptics: 'yes' }), { scale: SCALE_RANGE.max, opacity: 0.2, haptics: true });
  assert.deepEqual(clampSettings({ scale: 0.1, opacity: 5 }), { scale: SCALE_RANGE.min, opacity: 1, haptics: true });
  assert.deepEqual(clampSettings(null), { scale: 1, opacity: 0.6, haptics: true });
  assert.deepEqual(clampSettings({ scale: 'big' }), { scale: 1, opacity: 0.6, haptics: true });
});

test('settings are remembered in the browser\'s storage; storage that throws or holds rubbish leaves the defaults', () => {
  const store = new Map();
  const storage = { getItem: (k) => store.get(k) ?? null, setItem: (k, v) => store.set(k, v) };
  saveSettings(storage, { scale: 1.2, opacity: 0.4, haptics: false });
  assert.deepEqual(loadSettings(storage), { scale: 1.2, opacity: 0.4, haptics: false });
  store.set('wotlane.touch', '{not json');
  assert.deepEqual(loadSettings(storage), { scale: 1, opacity: 0.6, haptics: true });
  const broken = { getItem() { throw new Error('blocked'); }, setItem() { throw new Error('blocked'); } };
  assert.deepEqual(loadSettings(broken), { scale: 1, opacity: 0.6, haptics: true });
  assert.doesNotThrow(() => saveSettings(broken, { scale: 1, opacity: 0.6, haptics: true }));
});

test('the Controls card gets one row per control in plain words', () => {
  const rows = touchRows();
  assert.ok(rows.length >= buttons.length);
  assert.ok(rows.some(([does, how]) => /Jump/.test(does) && /✕/.test(how)));
  assert.ok(rows.some(([does]) => /Move/.test(does)));
  assert.ok(rows.some(([does]) => /camera/i.test(does)));
});

test('a tap is a quick touch that hardly moved; a held touch or a drag is not', () => {
  assert.equal(isTap({ ms: 90, distance: 3 }), true);
  assert.equal(isTap({ ms: 400, distance: 3 }), false, 'resting a thumb is not a tap');
  assert.equal(isTap({ ms: 90, distance: 40 }), false, 'a flick is a drag');
});
