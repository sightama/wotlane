// ABOUTME: Tests for web/display.js: the display's refresh rate from animation frame times, and the canvas size in device pixels.
// ABOUTME: Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { refreshFromTimes, measureRefresh, devicePixels, readCachedRefresh, writeCachedRefresh } = createRequire(import.meta.url)('./display.js');

const frames = (n, periodMs, start = 1000) => Array.from({ length: n }, (_, i) => start + i * periodMs);

test('a steady 120 Hz display reads 120', () => {
  assert.equal(refreshFromTimes(frames(61, 1000 / 120)).hz, 120);
});

test('a steady 60 Hz display reads 60', () => {
  assert.equal(refreshFromTimes(frames(61, 1000 / 60)).hz, 60);
});

test('a rate near a common one snaps to it', () => {
  const r = refreshFromTimes(frames(61, 1000 / 143.2));
  assert.equal(r.hz, 144);
  assert.ok(Math.abs(r.raw - 143.2) < 0.1);
});

test('a rate far from every common one is rounded', () => {
  assert.equal(refreshFromTimes(frames(61, 1000 / 110)).hz, 110);
});

test('a few late frames do not change the reading', () => {
  const t = frames(61, 1000 / 120);
  for (const i of [10, 30, 50]) for (let j = i; j < t.length; j++) t[j] += 8;   // three dropped frames
  assert.equal(refreshFromTimes(t).hz, 120);
});

test('too few frames read 0 (unknown)', () => {
  assert.equal(refreshFromTimes(frames(4, 1000 / 120)).hz, 0);
});

test('the canvas size in device pixels follows the pixel ratio', () => {
  assert.deepEqual(devicePixels(1512, 945, 2), { w: 3024, h: 1890 });
  assert.deepEqual(devicePixels(1280, 720, 1), { w: 1280, h: 720 });
  assert.deepEqual(devicePixels(1001, 500, 1.25), { w: 1251, h: 625 });
});

test('a missing or odd pixel ratio counts as 1', () => {
  assert.deepEqual(devicePixels(800, 600, 0), { w: 800, h: 600 });
  assert.deepEqual(devicePixels(800, 600, undefined), { w: 800, h: 600 });
});

function memStorage({ broken = false } = {}) {
  const data = new Map();
  const guard = () => { if (broken) throw new Error('SecurityError'); };
  return {
    getItem: (k) => { guard(); return data.has(k) ? data.get(k) : null; },
    setItem: (k, v) => { guard(); data.set(k, String(v)); },
  };
}

test('cached refresh rate round-trips valid integer rates and ignores invalid or broken storage', () => {
  const s = memStorage();
  assert.equal(readCachedRefresh(s), 0);
  writeCachedRefresh(s, 120);
  assert.equal(readCachedRefresh(s), 120);
  writeCachedRefresh(s, 0);
  assert.equal(readCachedRefresh(s), 120, 'invalid hz does not overwrite a cached value');
  for (const bad of ['0', '15', '2000', 'abc', '60.5', '']) {
    const t = memStorage();
    t.setItem('wotlane.displayHz', bad);
    assert.equal(readCachedRefresh(t), 0, `rejects ${JSON.stringify(bad)}`);
  }
  const b = memStorage({ broken: true });
  assert.doesNotThrow(() => writeCachedRefresh(b, 60));
  assert.equal(readCachedRefresh(b), 0);
});

test('measureRefresh stops early and returns unknown when cancelled', async () => {
  let cancel = false;
  let scheduled = 0;
  const origRaf = globalThis.requestAnimationFrame;
  globalThis.requestAnimationFrame = (cb) => {
    scheduled++;
    if (scheduled === 3) cancel = true;
    setImmediate(() => cb(scheduled * 16.67));
  };
  try {
    const r = await measureRefresh(60, { cancelled: () => cancel });
    assert.deepEqual(r, { hz: 0, raw: 0 });
    assert.equal(scheduled, 3, 'stops scheduling rAF once cancelled');
  } finally {
    if (origRaf === undefined) delete globalThis.requestAnimationFrame;
    else globalThis.requestAnimationFrame = origRaf;
  }
});
