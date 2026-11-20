// ABOUTME: Tests for web/rematch.js: the lobby code and rejoin token this tab keeps across the reload after a match (quick rematch),
// ABOUTME: given back only while the server would still hold the place, and cleared on leaving. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { storeRejoin, takeRejoin, clearRejoin, leavingPage, LINGER_MS } from './rematch.js';

function storage({ broken = false } = {}) {
  const data = new Map();
  const guard = () => { if (broken) throw new Error('SecurityError'); };
  return {
    getItem: (k) => { guard(); return data.has(k) ? data.get(k) : null; },
    setItem: (k, v) => { guard(); data.set(k, String(v)); },
    removeItem: (k) => { guard(); data.delete(k); },
  };
}

test('a stored token comes back once, while the server keeps the place', () => {
  const s = storage();
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 1000);
  assert.deepEqual(takeRejoin(s, 1000 + LINGER_MS - 5000), { code: 'QZXW', token: 'ab'.repeat(16) });
  assert.equal(takeRejoin(s, 1000 + LINGER_MS - 4000), null, 'single use: the server answers with a new token');
});

test('an old token is dropped (the server has let the place go)', () => {
  const s = storage();
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 0);
  assert.equal(takeRejoin(s, LINGER_MS + 1), null);
});

test('leaving clears it, and broken or odd storage gives nothing without throwing', () => {
  const s = storage();
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 0);
  clearRejoin(s);
  assert.equal(takeRejoin(s, 1), null);
  const b = storage({ broken: true });
  assert.doesNotThrow(() => storeRejoin(b, 'QZXW', 'ab'.repeat(16), 0));
  assert.equal(takeRejoin(b, 1), null);
  assert.doesNotThrow(() => clearRejoin(b));
  const odd = storage();
  odd.setItem('wotlane.rejoin', '{not json');
  assert.equal(takeRejoin(odd, 1), null);
});

test('the server counts from the page going away, so leavingPage restarts the clock: a long lobby and match keep the place', () => {
  const s = storage();
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 0);   // the token comes at the join
  leavingPage(s, 10 * LINGER_MS);                 // the match ends long after, and the page reloads
  assert.deepEqual(takeRejoin(s, 10 * LINGER_MS + 3000), { code: 'QZXW', token: 'ab'.repeat(16) });
  const none = storage();
  leavingPage(none, 5);   // nothing kept: nothing to restart
  assert.equal(none.getItem('wotlane.rejoin'), null);
  assert.doesNotThrow(() => leavingPage(storage({ broken: true }), 5));
});

test('a code or token not in the server\'s form is not sent', () => {
  for (const [code, token] of [['QZ W', 'ab'.repeat(16)], ['QZXW', 'ab'.repeat(16) + '\nCHAT x'], ['qzxw', 'ab'.repeat(16)], ['QZXW', 'AB'.repeat(16)], ['QZXW', 'ab']]) {
    const s = storage();
    storeRejoin(s, code, token, 0);
    assert.equal(takeRejoin(s, 1), null, JSON.stringify([code, token]));
  }
});
