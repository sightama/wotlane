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

test('after a QUIT a start line only counts once the page has reloaded', async () => {
  const { hasStart } = await import('../lib/session.mjs');
  const { RELOADED } = await import('../lib/logs.mjs');
  const start = '1s [net] start: player 1 of 2, delay 2 frames\n';
  assert.equal(hasStart({ logText: start }), true);
  assert.equal(hasStart({ logText: start }, { afterReload: true }), false, 'the old match start is not the rematch');
  assert.equal(hasStart({ logText: `${start}${RELOADED}\n` }, { afterReload: true }), false);
  assert.equal(hasStart({ logText: `${start}${RELOADED}\n2s [net] start: player 1 of 2, delay 2 frames\n` }, { afterReload: true }), true);
});

test('a window that will not resize is reported, not ignored', async () => {
  const { Player } = await import('../lib/player.mjs');
  const p = Object.create(Player.prototype);
  p.name = 'P2-ab12';
  p.gameViewport = { width: 640, height: 400 };
  p.page = { setViewportSize: async () => { throw new Error('Target closed'); } };
  await assert.rejects(p.toGameViewport(), /P2-ab12.*640x400.*Target closed/);
  p.page = { setViewportSize: async (v) => { p.set = v; } };
  await p.toGameViewport();
  assert.deepEqual(p.set, { width: 640, height: 400 });
});

test('the front panel is back only when #front itself is visible, not just the lobby card inside it', async () => {
  const { backInLobbyScript } = await import('../lib/session.mjs');
  const dom = (frontHidden, lobbyHidden, code) => ({
    getElementById: (id) => ({ front: { hidden: frontHidden }, inLobby: { hidden: lobbyHidden }, code: { textContent: ` ${code} ` } }[id]),
  });
  const run = (frontHidden) => backInLobbyScript.call(null, 'ABCD', dom(frontHidden, false, 'ABCD'));
  assert.equal(run(false), true);
  assert.equal(run(true), false, 'the game still covers the front panel');
  assert.equal(backInLobbyScript('ABCD', dom(false, true, 'ABCD')), false);
  assert.equal(backInLobbyScript('ABCD', dom(false, false, 'WXYZ')), false);
});
