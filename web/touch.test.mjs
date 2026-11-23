// ABOUTME: Tests for web/touch.js: the on-screen layout (every PS2 button reachable, nothing overlapping or off screen on phone screens),
// ABOUTME: the remembered settings and the Controls card's touch rows. The overlay's DOM and touches are tested in the browser. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { LAYOUT, SCALE_RANGE, unitFor, rectOf, clampSettings, loadSettings, saveSettings, touchRows, ZONE, isTap, ICONS } = require('./touch.js');
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
  assert.deepEqual(clampSettings({ scale: 9, opacity: -1, haptics: 'yes' }), { scale: SCALE_RANGE.max, opacity: 0.2, haptics: true, mirror: false, place: {} });
  assert.deepEqual(clampSettings({ scale: 0.1, opacity: 5 }), { scale: SCALE_RANGE.min, opacity: 1, haptics: true, mirror: false, place: {} });
  assert.deepEqual(clampSettings(null), { scale: 1, opacity: 0.6, haptics: true, mirror: false, place: {} });
  assert.deepEqual(clampSettings({ scale: 'big' }), { scale: 1, opacity: 0.6, haptics: true, mirror: false, place: {} });
});

test('settings are remembered in the browser\'s storage; storage that throws or holds rubbish leaves the defaults', () => {
  const store = new Map();
  const storage = { getItem: (k) => store.get(k) ?? null, setItem: (k, v) => store.set(k, v) };
  saveSettings(storage, { scale: 1.2, opacity: 0.4, haptics: false });
  assert.deepEqual(loadSettings(storage), { scale: 1.2, opacity: 0.4, haptics: false, mirror: false, place: {} });
  store.set('wotlane.touch', '{not json');
  assert.deepEqual(loadSettings(storage), { scale: 1, opacity: 0.6, haptics: true, mirror: false, place: {} });
  const broken = { getItem() { throw new Error('blocked'); }, setItem() { throw new Error('blocked'); } };
  assert.deepEqual(loadSettings(broken), { scale: 1, opacity: 0.6, haptics: true, mirror: false, place: {} });
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

const FACE = ['cross', 'circle', 'square', 'triangle'];

test('the four face buttons are drawn as the same kind of icon, one SVG each, not text glyphs from different fonts', () => {
  for (const id of FACE) {
    const item = LAYOUT.find((b) => b.id === id);
    assert.equal(item.icon, id, `${id} names its icon`);
    assert.match(ICONS[id], /^<svg[^>]*viewBox="0 0 24 24"/, `${id} is an SVG`);
  }
  const strokes = new Set(FACE.map((id) => /stroke-width="([\d.]+)"/.exec(ICONS[id])[1]));
  assert.equal(strokes.size, 1, 'the same line weight');
});

test('the face buttons are a little smaller than a thumb pad of 1.6 units, and grab smaller still', () => {
  for (const id of FACE) {
    const { w, h } = LAYOUT.find((b) => b.id === id);
    assert.equal(w, h);
    assert.ok(w >= 1.1 && w <= 1.4, `${id} is ${w}`);
  }
  assert.ok(LAYOUT.find((b) => b.id === 'circle').w < LAYOUT.find((b) => b.id === 'cross').w);
});

const { layoutFor, placeFromPoint, zonesFor } = require('./touch.js');
const plain = { scale: 1, opacity: 0.6, haptics: true, mirror: false, place: {} };

test('settings keep a left handed switch and placed controls, and drop what is not a placeable control or a number', () => {
  assert.deepEqual(clampSettings(null).place, {});
  assert.equal(clampSettings(null).mirror, false);
  assert.equal(clampSettings({ mirror: true }).mirror, true);
  assert.equal(clampSettings({ mirror: 'yes' }).mirror, false);
  const s = clampSettings({ place: { cross: { x: 3, y: 2 }, gear: { x: 1, y: 1 }, nope: { x: 1, y: 1 }, circle: { x: 'a', y: 1 }, l1: { x: 99, y: 2 } } });
  assert.deepEqual(s.place, { cross: { x: 3, y: 2 } }, 'the gear stays put, unknown ids and non-numbers and far-off places are dropped');
});

test('left handed mirrors every control to the other side and keeps its height; the middle controls swap sides', () => {
  const left = layoutFor({ ...plain, mirror: true });
  for (const [i, b] of LAYOUT.entries()) {
    assert.equal(left[i].id, b.id);
    assert.equal(left[i].y, b.y);
    if (b.anchor === 'tc') assert.equal(left[i].x, -b.x);
    else assert.equal(left[i].anchor, { br: 'bl', bl: 'br', tr: 'tl', tl: 'tr' }[b.anchor]);
  }
  assert.deepEqual(layoutFor(plain), LAYOUT);
  const [w, h] = [915, 412], u = unitFor(h, 1);
  const cross = rectOf(left.find((b) => b.id === 'cross'), w, h, u), right = rectOf(LAYOUT.find((b) => b.id === 'cross'), w, h, u);
  assert.ok(Math.abs(cross.left - (w - right.right)) < 1e-6, 'the mirror image');
});

for (const [w, h] of SCREENS) {
  test(`${w}x${h}: left handed controls stay on screen and never overlap`, () => {
    const u = unitFor(h, 1);
    const shapes = layoutFor({ ...plain, mirror: true }).map((b) => ({ id: b.id, round: ['cross', 'square', 'triangle', 'circle', 'chord'].includes(b.cls), ...rectOf(b, w, h, u) }));
    for (const r of shapes) assert.ok(r.left >= 0 && r.top >= 0 && r.right <= w && r.bottom <= h, `${r.id} is on screen`);
    for (let i = 0; i < shapes.length; i++)
      for (let j = i + 1; j < shapes.length; j++) assert.ok(!touching(shapes[i], shapes[j]), `${shapes[i].id} and ${shapes[j].id} overlap`);
  });
}

test('a placed control uses its own place, in either hand', () => {
  for (const mirror of [false, true]) {
    const one = layoutFor({ ...plain, mirror, place: { circle: { x: 5, y: 6 } } }).find((b) => b.id === 'circle');
    assert.equal(one.x, 5);
    assert.equal(one.y, 6);
    assert.equal(one.anchor, mirror ? 'bl' : 'br');
  }
});

test('dragging a control to a point puts its centre there: the place read back gives the same rectangle', () => {
  const [w, h] = [915, 412], u = unitFor(h, 1);
  for (const item of LAYOUT.filter((b) => b.id !== 'gear')) {
    for (const mirrored of [item, ...layoutFor({ ...plain, mirror: true }).filter((b) => b.id === item.id)]) {
      const cx = 300, cy = 200;
      const place = placeFromPoint(mirrored, w, h, u, cx, cy);
      const r = rectOf({ ...mirrored, ...place }, w, h, u);
      assert.ok(Math.abs((r.left + r.right) / 2 - cx) <= 0.05 * u && Math.abs((r.top + r.bottom) / 2 - cy) <= 0.05 * u, `${item.id} ${mirrored.anchor}`);
    }
  }
});

test('a control dragged past an edge stays whole on screen', () => {
  const [w, h] = [915, 412], u = unitFor(h, 1);
  for (const [cx, cy] of [[-50, -50], [w + 50, h + 50], [0, h], [w, 0]]) {
    for (const item of LAYOUT.filter((b) => b.id !== 'gear')) {
      const r = rectOf({ ...item, ...placeFromPoint(item, w, h, u, cx, cy) }, w, h, u);
      assert.ok(r.left >= -0.05 * u && r.top >= -0.05 * u && r.right <= w + 0.05 * u && r.bottom <= h + 0.05 * u, `${item.id} at ${cx},${cy}`);
    }
  }
});

test('left handed swaps the stick and look zones; the screen is covered once either way', () => {
  const [w, h] = [915, 412];
  const right = zonesFor(false, w, h), left = zonesFor(true, w, h);
  assert.equal(right.stick.left, 0);
  assert.equal(left.look.left, 0);
  assert.equal(left.stick.left, left.look.width);
  for (const z of [right, left]) assert.ok(Math.abs(z.stick.width + z.look.width - w) < 1e-6);
  assert.equal(right.stick.top, ZONE.top * h);
});

const { fitOnScreen } = require('./touch.js');

test('a place far from its anchor survives the settings (a wide screen) and one too far for any screen does not', () => {
  assert.deepEqual(clampSettings({ place: { cross: { x: 20, y: 3 } } }).place, { cross: { x: 20, y: 3 } });
  assert.deepEqual(clampSettings({ place: { cross: { x: 500, y: 3 } } }).place, {});
});

test('a placed control comes back on screen when the screen or the size shrinks; one already on screen is not moved', () => {
  const placed = { ...LAYOUT.find((b) => b.id === 'cross'), x: 3, y: 12 };
  const big = unitFor(448, SCALE_RANGE.max), small = unitFor(360, SCALE_RANGE.min);
  const before = rectOf(placed, 998, 448, big);
  const fit = fitOnScreen(placed, 800, 360, small * 1.7);   // a smaller screen at a larger size than the place was made for
  const r = rectOf(fit, 800, 360, small * 1.7);
  assert.ok(r.left >= -0.05 * small && r.top >= -0.05 * small && r.right <= 800 + 0.05 * small && r.bottom <= 360 + 0.05 * small);
  assert.ok(before.bottom <= 448);
  const fine = { ...placed, y: 2 };
  assert.deepEqual(fitOnScreen(fine, 998, 448, unitFor(448, 1)), fine);
});
