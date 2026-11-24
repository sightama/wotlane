// ABOUTME: Tests for web/rematch.js: the lobby code and rejoin token this tab keeps across the reload after a match (quick rematch),
// ABOUTME: given back only while the server would still hold the place, and cleared on leaving. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { storeRejoin, takeRejoin, peekRejoin, clearRejoin, leavingPage, isInMatchError, retryRejoin, LINGER_MS } from './rematch.js';

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
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 1000, { name: 'Ana', monster: '7:green' });
  assert.deepEqual(takeRejoin(s, 1000 + LINGER_MS - 5000), { code: 'QZXW', token: 'ab'.repeat(16), name: 'Ana', monster: '7:green' });
  assert.equal(takeRejoin(s, 1000 + LINGER_MS - 4000), null, 'single use: the server answers with a new token');
});

test('an old token is dropped (the server has let the place go)', () => {
  const s = storage();
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 0, { name: 'Ana', monster: '5' });
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
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 0, { name: 'Ana', monster: '5' });   // the token comes at the join
  leavingPage(s, 10 * LINGER_MS, { name: 'Ana', monster: '9' });                 // the match ends long after, and the page reloads
  assert.deepEqual(takeRejoin(s, 10 * LINGER_MS + 3000), { code: 'QZXW', token: 'ab'.repeat(16), name: 'Ana', monster: '9' });
  const none = storage();
  leavingPage(none, 5);   // nothing kept: nothing to restart
  assert.equal(none.getItem('wotlane.rejoin'), null);
  assert.doesNotThrow(() => leavingPage(storage({ broken: true }), 5));
});

test('a code or token not in the server\'s form is not sent', () => {
  for (const [code, token] of [['QZ W', 'ab'.repeat(16)], ['QZXW', 'ab'.repeat(16) + '\nCHAT x'], ['qzxw', 'ab'.repeat(16)], ['QZXW', 'AB'.repeat(16)], ['QZXW', 'ab']]) {
    const s = storage();
    storeRejoin(s, code, token, 0, { name: 'Ana', monster: '5' });
    assert.equal(takeRejoin(s, 1), null, JSON.stringify([code, token]));
  }
});

test('the name and monster come from this tab, not from what another tab of the browser remembers; an odd one is not given back', () => {
  for (const who of [{ name: 'A b', monster: '5' }, { name: '', monster: '5' }, { name: 'Ana', monster: 'x' }, { name: 'Ana', monster: 5 }, { name: 'Ana', monster: '5:a b' }, { name: 'A'.repeat(17), monster: '5' }]) {
    const s = storage();
    storeRejoin(s, 'QZXW', 'ab'.repeat(16), 0, who);
    assert.equal(takeRejoin(s, 1), null, JSON.stringify(who));
  }
});

test('peekRejoin reads a valid token without removing it, and returns null when expired', () => {
  const s = storage();
  storeRejoin(s, 'QZXW', 'ab'.repeat(16), 1000, { name: 'Ana', monster: '7:green' });
  const expected = { code: 'QZXW', token: 'ab'.repeat(16), name: 'Ana', monster: '7:green' };
  assert.deepEqual(peekRejoin(s, 2000), expected);
  assert.deepEqual(peekRejoin(s, 3000), expected, 'peek keeps the token for a later manual Join or reload');
  assert.equal(peekRejoin(s, 1000 + LINGER_MS + 1), null, 'expired token is ignored');
});

test('isInMatchError matches only the transient match-still-ending refusal', () => {
  assert.equal(isInMatchError('lobby is in a match; wait for it to end'), true);
  assert.equal(isInMatchError('ERROR lobby is in a match; wait for it to end'), true);
  assert.equal(isInMatchError("can't rejoin (that place is gone or was already used); join again with the code"), false);
  assert.equal(isInMatchError('the lobby is gone; host or join a new one'), false);
  assert.equal(isInMatchError(null), false);
});

test('retryRejoin retries while the match is finishing and succeeds once it ends', async () => {
  let clock = 0;
  let tries = 0;
  let waited = 0;
  const sleeps = [];
  const res = await retryRejoin(async () => {
    tries++;
    if (tries < 3) throw new Error('lobby is in a match; wait for it to end');
    return 'QZXW';
  }, {
    waitMs: 2000,
    stepMs: 350,
    now: () => clock,
    sleep: async (ms) => { sleeps.push(ms); clock += ms; },
    onWait: () => { waited++; },
  });
  assert.equal(res, 'QZXW');
  assert.equal(tries, 3);
  assert.equal(waited, 1, 'onWait fires once when entering the wait');
  assert.deepEqual(sleeps, [350, 350]);
});

test('retryRejoin stops immediately on a terminal error without waiting', async () => {
  let tries = 0;
  let waited = 0;
  await assert.rejects(() => retryRejoin(async () => {
    tries++;
    throw new Error("can't rejoin (that place is gone or was already used); join again with the code");
  }, {
    waitMs: 2000,
    stepMs: 350,
    now: () => 0,
    sleep: async () => { throw new Error('must not sleep on terminal error'); },
    onWait: () => { waited++; },
  }), /can't rejoin/);
  assert.equal(tries, 1);
  assert.equal(waited, 0);
});

test('retryRejoin gives up after waitMs when the match is still running', async () => {
  let clock = 0;
  let tries = 0;
  await assert.rejects(() => retryRejoin(async () => {
    tries++;
    throw new Error('lobby is in a match; wait for it to end');
  }, {
    waitMs: 1000,
    stepMs: 400,
    now: () => clock,
    sleep: async (ms) => { clock += ms; },
  }), /lobby is in a match/);
  assert.equal(clock, 1000);
  assert.equal(tries, 4);   // t=0, 400, 800, 1000
});

