// ABOUTME: Tests for lib/logs.mjs: the readers of the game's page log (what wotmLog holds) and of wotm_server's log that the
// ABOUTME: browser multiplayer checks judge by. Run: node --test tools/browser_mp_test/test/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  netStart, netStopped, hashes, compareHashes, gameFps, lastWaits, desyncs, padButtons, hasLine, serverLobbyLines, roundsEnded,
} from '../lib/logs.mjs';

const log = [
  '3.48s [net] start: player 2 of 4, delay 2 frames, ping 0 ms',
  '9.87s [net] hash of frame 31: 94253ea71aea054b',
  '19.12s [net] hash of frame 543: fc56ea395d6a927a',
  '27.67s [net] hash of frame 1055: 50bff411167fbf40',
  '28.00s [net] frame 1100: 12 waits for the network, 340 ms in total',
  '30.00s [net] frame 1200: 15 waits for the network, 410 ms in total',
].join('\n');

test('netStart reads the local player, the player count and the delay', () => {
  assert.deepEqual(netStart(log), { player: 2, of: 4, delay: 2 });
  assert.equal(netStart('nothing here'), null);
});

test('netStopped returns the reason, null while the match runs', () => {
  assert.equal(netStopped(log), null);
  assert.equal(netStopped(log + '\n40.0s [net] stopped: connection lost'), 'connection lost');
});

test('hashes maps frame to hash', () => {
  const h = hashes(log);
  assert.equal(h.size, 3);
  assert.equal(h.get(543), 'fc56ea395d6a927a');
});

test('compareHashes counts the frames every log has and lists the ones that differ', () => {
  const a = hashes(log);
  const b = hashes(log.replace('50bff411167fbf40', '0000000000000000'));
  const r = compareHashes([a, b]);
  assert.equal(r.common, 3);
  assert.deepEqual(r.mismatches, [{ frame: 1055, hashes: ['50bff411167fbf40', '0000000000000000'] }]);
  assert.equal(compareHashes([a, a]).mismatches.length, 0);
});

test('compareHashes ignores frames a log lacks (a player that left earlier)', () => {
  const a = hashes(log);
  const short = hashes('9.87s [net] hash of frame 31: 94253ea71aea054b');
  const r = compareHashes([a, short]);
  assert.equal(r.common, 1);
  assert.equal(r.mismatches.length, 0);
});

test('gameFps is the slope of frame against time between the first and last hash lines', () => {
  // frames 31..1055 in 27.67 - 9.87 = 17.8 s
  assert.ok(Math.abs(gameFps(log) - 1024 / 17.8) < 0.01);
  assert.equal(gameFps('9.87s [net] hash of frame 31: 94253ea71aea054b'), null);
});

test('lastWaits reads the last stall counters', () => {
  assert.deepEqual(lastWaits(log), { frame: 1200, waits: 15, ms: 410 });
  assert.equal(lastWaits('x'), null);
});

test('desyncs lists the server DESYNC lines', () => {
  const server = '[server] lobby ABCD: DESYNC state differs on frame 64: J1 vs J2; blocks 3 (0x0030000)\n[server] lobby ABCD: P1 left';
  assert.equal(desyncs(server).length, 1);
  assert.match(desyncs(server)[0], /frame 64/);
  assert.equal(desyncs('[server] lobby ABCD: start').length, 0);
});

test('padButtons maps each player (port + 2 * slot) to the buttons its scePadRead showed held', () => {
  const lines = [
    '5.0s [padlog] frame 10 scePadRead(port 0, slot 0, 00000000, 00000000) -> 1 data ffff7300 80808080',
    '5.0s [padlog] frame 10 scePadRead(port 1, slot 0, 00000000, 00000000) -> 1 data efff7300 80808080',
    '5.0s [padlog] frame 11 scePadRead(port 0, slot 2, 00000000, 00000000) -> 1 data fffb7300 80808080',
    '5.0s [padlog] frame 12 scePadRead(port 0, slot 2, 00000000, 00000000) -> 1 data bffb7300 80808080',
    '5.0s [padlog] frame 11 scePadGetState(port 0, slot 3, 00000000, 00000000) -> 6',
  ].join('\n');
  const held = padButtons(lines);
  assert.deepEqual([...held.keys()].sort(), [1, 4]);
  assert.equal(held.get(1), 0x1000);
  assert.equal(held.get(4), 0x0004 | 0x4000);
});

test('hasLine and roundsEnded read single events', () => {
  assert.equal(hasLine(log, /\[net\] hash of frame 31/), true);
  assert.equal(hasLine(log, /DESYNC/), false);
  assert.equal(roundsEnded('1.0s [match] round 1 ended (rtMain returned 0): x\n9.0s [match] round 2 ended (rtMain returned 0): y'), 2);
  assert.equal(roundsEnded(''), 0);
});

test('serverLobbyLines keeps the lines that name one lobby code', () => {
  const server = ['[server] lobby ABCD: start', '[server] lobby WXYZ: start', '[server] lobby 0: P1 reached the match (player 1)'].join('\n');
  assert.deepEqual(serverLobbyLines(server, 'ABCD'), ['[server] lobby ABCD: start']);
});
