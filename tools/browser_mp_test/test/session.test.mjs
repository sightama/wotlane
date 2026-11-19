// ABOUTME: Tests for the player names lib/session.mjs gives a run's tabs: a run tag keeps them apart from the lobbies earlier runs
// ABOUTME: left listed on the live site. Run: node --test tools/browser_mp_test/test/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { newRunTag, playerName } from '../lib/session.mjs';

test('two runs get different tags', () => {
  assert.notEqual(newRunTag(), newRunTag());
});

test('a player name carries its number and the run tag', () => {
  assert.equal(playerName('ab12', 0), 'P1-ab12');
  assert.equal(playerName('ab12', 7), 'P8-ab12');
});

test('players of different runs never share a name', () => {
  assert.notEqual(playerName(newRunTag(), 0), playerName(newRunTag(), 0));
});

test('a player name fits the front panel field (16 characters), spare included', () => {
  const tag = newRunTag();
  for (let i = 0; i <= 8; i++) assert.ok(playerName(tag, i).length <= 16);
});
