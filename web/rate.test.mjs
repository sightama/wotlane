// ABOUTME: Tests for web/rate.js: this device's own game frame rate from the diag windows of a 60 Hz match, and the remembered
// ABOUTME: verdict that it can't hold 60 (its next matches ask for 30 Hz).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ownRate, RateMemory, SLOW_KEY, rememberedSlow, HEALTH_KEY, rememberedHealth, healthLine } from './rate.js';

// a diag window as windowFields writes it, as [key, value] pairs
const win = (fields) => Object.entries(fields);

test('ownRate: frames over the time not spent waiting for the network', () => {
  assert.equal(ownRate({ frames: 150, seconds: 5, waitMs: 2500 }), 60);   // a fast PC behind a slow one: half the time waiting
  assert.equal(ownRate({ frames: 150, seconds: 5, waitMs: 0 }), 30);      // the slow one itself
  assert.equal(ownRate({ frames: 10, seconds: 5, waitMs: 4500 }), undefined);   // under a second of its own: no reading
});

function feed(memory, { gfps, waitMs, windows, hz = 60, from = 20, hidden = false }) {
  for (let i = 0; i < windows; ++i)
    memory.add(win({ hz, up_s: from + 5 * (i + 1), gfps, wait_ms: waitMs, ...(hidden ? { hid_s: 2 } : {}) }));
}

test('RateMemory: a device that computes ~30 game frames/s in a 60 Hz match is remembered as slow', () => {
  const store = new Map();
  const m = new RateMemory({ get: (k) => store.get(k) ?? null, set: (k, v) => store.set(k, v) });
  feed(m, { gfps: 30, waitMs: 50, windows: 8 });
  assert.equal(store.get(SLOW_KEY), '1');
  assert.equal(rememberedSlow({ getItem: (k) => store.get(k) ?? null }), true);
});

test('RateMemory: the fast device in the same slow match (it waits half the time) is remembered as able to hold 60', () => {
  const store = new Map([[SLOW_KEY, '1']]);
  const m = new RateMemory({ get: (k) => store.get(k) ?? null, set: (k, v) => store.set(k, v) });
  feed(m, { gfps: 30, waitMs: 2500, windows: 8 });
  assert.equal(store.get(SLOW_KEY), '0');
});

test('RateMemory: no verdict from a 30 Hz match, a hidden tab, the loading seconds or under 30 s of play', () => {
  for (const opts of [{ hz: 30, windows: 8 }, { hidden: true, windows: 8 }, { from: -40, windows: 8 }, { windows: 5 }]) {
    const store = new Map();
    const m = new RateMemory({ get: (k) => store.get(k) ?? null, set: (k, v) => store.set(k, v) });
    feed(m, { gfps: 30, waitMs: 0, ...opts });
    assert.equal(store.has(SLOW_KEY), false, JSON.stringify(opts));
  }
});

test('rememberedSlow: nothing stored, or storage that throws, is not slow', () => {
  assert.equal(rememberedSlow({ getItem: () => null }), false);
  assert.equal(rememberedSlow({ getItem: () => { throw new Error('denied'); } }), false);
  assert.equal(rememberedSlow(undefined), false);
});

test('RateMemory keeps the device health of a match (own frames a second, ms of work per frame, game Hz)', () => {
  for (const hz of [60]) {
    const store = new Map();
    const m = new RateMemory({ get: (k) => store.get(k) ?? null, set: (k, v) => store.set(k, v) });
    for (let i = 0; i < 6; ++i) m.add(win({ hz, up_s: 25 + 5 * i, gfps: 27, wait_ms: 0, work_ms: 13.2 }));
    assert.deepEqual(rememberedHealth({ getItem: (k) => store.get(k) ?? null }), { fps: 27, workMs: 13.2, hz });
  }
});

test('RateMemory keeps no health from a hidden tab, the loading seconds or a match too short to say', () => {
  for (const opts of [{ hidden: true, windows: 8 }, { from: -40, windows: 8 }, { windows: 3 }]) {
    const store = new Map();
    const m = new RateMemory({ get: (k) => store.get(k) ?? null, set: (k, v) => store.set(k, v) });
    feed(m, { gfps: 30, waitMs: 0, ...opts });
    assert.equal(store.has(HEALTH_KEY), false, JSON.stringify(opts));
  }
});

test('rememberedHealth: nothing stored, junk or storage that throws gives null', () => {
  assert.equal(rememberedHealth({ getItem: () => null }), null);
  assert.equal(rememberedHealth({ getItem: () => 'not json' }), null);
  assert.equal(rememberedHealth({ getItem: () => '{"fps":"x","workMs":1,"hz":60}' }), null);
  assert.equal(rememberedHealth({ getItem: () => { throw new Error('denied'); } }), null);
  assert.equal(rememberedHealth(undefined), null);
});

test('healthLine is the HEALTH command for a lobby', () => {
  assert.equal(healthLine({ fps: 27.4, workMs: 13.24, hz: 60 }), 'HEALTH 27.4 13.2 60');
});

test('rememberedHealth refuses numbers the server would refuse', () => {
  for (const h of [{ fps: 1001, workMs: 1, hz: 60 }, { fps: -1, workMs: 1, hz: 60 }, { fps: 30, workMs: 1, hz: 0 }, { fps: 30, workMs: 1, hz: 30.5 }, { fps: 30, workMs: 1, hz: 300 }])
    assert.equal(rememberedHealth({ getItem: () => JSON.stringify(h) }), null, JSON.stringify(h));
});
