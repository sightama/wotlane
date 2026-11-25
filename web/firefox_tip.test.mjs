// ABOUTME: Tests for web/firefox_tip.js: the front panel's performance tip for Firefox players (a pref that has the game's code fully
// ABOUTME: optimized right away), Android's caveat, and the dismissal this browser remembers. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { firefoxTip, dismissFirefoxTip, FIREFOX_TIP, ANDROID_CAVEAT } from './firefox_tip.js';

// a Storage stand-in; `broken` throws like a private window's or a blocked one
function storage({ broken = false } = {}) {
  const data = new Map();
  const guard = () => { if (broken) throw new Error('SecurityError'); };
  return {
    getItem: (k) => { guard(); return data.has(k) ? data.get(k) : null; },
    setItem: (k, v) => { guard(); data.set(k, String(v)); },
  };
}

test('Firefox on a computer gets the tip, naming the pref and the restart', () => {
  const tip = firefoxTip({ firefox: true, android: false, storage: storage() });
  assert.equal(tip, FIREFOX_TIP);
  assert.match(tip, /about:config/);
  assert.match(tip, /javascript\.options\.wasm_lazy_tiering/);
  assert.match(tip, /false/);
  assert.match(tip, /restart Firefox/i);
  assert.match(tip, /accept the warning/);
  assert.match(tip, /every website/, 'the pref is browser-wide: the player should know');
  assert.match(tip, /undo/);
  assert.match(tip, /8 monsters/, 'where it helps, as measured');
  assert.match(tip, /1 vs 1/, 'and where it does not, so nobody flips a browser-wide setting for nothing');
});

test('Firefox on Android gets the tip with the caveat that about:config opens only in Beta or Nightly there', () => {
  assert.equal(firefoxTip({ firefox: true, android: true, storage: storage() }), FIREFOX_TIP + ' ' + ANDROID_CAVEAT);
  assert.match(ANDROID_CAVEAT, /Beta or Nightly/);
});

test('other browsers get nothing', () => {
  assert.equal(firefoxTip({ firefox: false, android: false, storage: storage() }), '');
  assert.equal(firefoxTip({ firefox: false, android: true, storage: storage() }), '');
});

test('once dismissed, this browser no longer shows it', () => {
  const s = storage();
  dismissFirefoxTip(s);
  assert.equal(firefoxTip({ firefox: true, android: false, storage: s }), '');
});

test('storage that refuses (a private window) still shows the tip, and dismissing there does not throw', () => {
  const s = storage({ broken: true });
  assert.equal(firefoxTip({ firefox: true, android: false, storage: s }), FIREFOX_TIP);
  assert.doesNotThrow(() => dismissFirefoxTip(s));
});
