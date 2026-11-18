// ABOUTME: Tests for web/rate.js: this device's own game frame rate from the diag windows of a 60 Hz match, and the remembered
// ABOUTME: verdict that it can't hold 60 (its next matches ask for 30 Hz).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ownRate, RateMemory, SLOW_KEY, rememberedSlow } from './rate.js';

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

test('RateMemory: a device that can not even hold 30 in a 30 Hz match (5QMC: ~24) is remembered as slow; holding 30 there says nothing about 60', () => {
  const store = new Map();
  const m = new RateMemory({ get: (k) => store.get(k) ?? null, set: (k, v) => store.set(k, v) });
  feed(m, { hz: 30, gfps: 18.4, waitMs: 1100, windows: 8 });   // 18.4 / (1 - 0.22) = ~23.6 of 30
  assert.equal(store.get(SLOW_KEY), '1');
  const held = new Map([[SLOW_KEY, '1']]);
  const m2 = new RateMemory({ get: (k) => held.get(k) ?? null, set: (k, v) => held.set(k, v) });
  feed(m2, { hz: 30, gfps: 18, waitMs: 2000, windows: 8 });     // 18 / 0.6 = 30: kept up at 30 Hz
  assert.equal(held.get(SLOW_KEY), '1');
});
