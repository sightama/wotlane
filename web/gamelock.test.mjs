// ABOUTME: Tests for web/gamelock.js: another tab's game is told from the lock list, and boot.js takes the lock gamelock.js looks for.
// ABOUTME: Run: node --test web/*.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { gameRunningElsewhere, GAME_LOCK } from './gamelock.js';

test('a game in another tab is one lock held under the game lock name', async () => {
  assert.equal(await gameRunningElsewhere({ query: async () => ({ held: [{ name: GAME_LOCK }] }) }), true);
  assert.equal(await gameRunningElsewhere({ query: async () => ({ held: [{ name: 'other' }], pending: [] }) }), false);
  assert.equal(await gameRunningElsewhere({ query: async () => ({ held: [] }) }), false);
});

test('a browser without locks, or a lock query that fails, says no game is running', async () => {
  assert.equal(await gameRunningElsewhere(undefined), false);
  assert.equal(await gameRunningElsewhere({ query: async () => { throw new Error('x'); } }), false);
});

test('boot.js takes the lock when the game starts', () => {
  const boot = readFileSync(new URL('./boot.js', import.meta.url), 'utf8');
  assert.ok(boot.includes(`locks?.request('${GAME_LOCK}'`), 'boot.js does not request the game lock');
});
