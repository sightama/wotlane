// ABOUTME: Tests for web/report_ui.js: how a hand-sent report reaches the server (over the open lobby connection or a short-lived one), what
// ABOUTME: the player is told, and that every element the page scripts look up by id is in index.html. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { deliverReport, reportMessage } from './report_ui.js';
import { flushSocket } from './diag.js';

const fakeClient = (answer = true) => ({ sent: [], closed: false, async sendDiag(line) { this.sent.push(line); return answer; }, close() { this.closed = true; } });

test('a report goes over the lobby connection the player already has, which stays open', async () => {
  const lobby = fakeClient();
  const connect = async () => { throw new Error('no second connection needed'); };
  assert.equal(await deliverReport({ line: 'DIAG v=1', lobby, connect }), 'sent');
  assert.deepEqual(lobby.sent, ['DIAG v=1']);
  assert.equal(lobby.closed, false);
});

test('without a lobby connection a short-lived one carries the report and is closed afterwards', async () => {
  const own = fakeClient();
  assert.equal(await deliverReport({ line: 'DIAG v=1', lobby: null, connect: async () => own }), 'sent');
  assert.deepEqual(own.sent, ['DIAG v=1']);
  assert.equal(own.closed, true);
});

test('a server that cannot take reports is said so, and the connection is still closed', async () => {
  const own = fakeClient(false);
  assert.equal(await deliverReport({ line: 'DIAG v=1', lobby: null, connect: async () => own }), 'unsupported');
  assert.equal(own.closed, true);
});

test('a server that never answers ends the attempt after the time limit, with the connection closed', async () => {
  const own = { closed: false, sendDiag: () => new Promise(() => {}), close() { this.closed = true; } };
  await assert.rejects(deliverReport({ line: 'DIAG v=1', lobby: null, connect: async () => own, timeoutMs: 20 }), /did not answer/);
  assert.equal(own.closed, true);
  await assert.rejects(deliverReport({ line: 'DIAG v=1', lobby: null, connect: () => new Promise(() => {}), timeoutMs: 20 }), /did not answer/);
});

test('a refused connection is an error the player reads in plain words', async () => {
  await assert.rejects(deliverReport({ line: 'DIAG v=1', lobby: null, connect: async () => { throw new Error('lost the connection to the game server'); } }), /lost the connection/);
});

test('the message tells what happened, and what was sent', () => {
  assert.equal(reportMessage('sent'), 'Report sent. Thank you.');
  assert.match(reportMessage('unsupported'), /can't take reports yet/);
  assert.match(reportMessage(new Error('lost the connection to the game server')), /Could not send the report: lost the connection/);
});

test('flushSocket waits until the socket has sent what it holds, and gives up after the time limit', async () => {
  let pending = 3;
  const socket = { get bufferedAmount() { return pending > 0 ? pending-- : 0; } };
  let waits = 0;
  await flushSocket(socket, { ms: 1000, wait: async () => { ++waits; } });
  assert.equal(waits, 3);
  let slept = 0;
  await flushSocket({ bufferedAmount: 10 }, { ms: 100, wait: async (n) => { slept += n; } });
  assert.ok(slept >= 100 && slept < 200);
  await flushSocket({}, { ms: 100, wait: async () => { throw new Error('never waits for a socket with nothing buffered'); } });
});

test('every element the page scripts look up by id is in index.html', () => {
  const html = readFileSync(new URL('./index.html', import.meta.url), 'utf8');
  const ids = new Set([...html.matchAll(/\bid="([^"]+)"/g)].map((m) => m[1]));
  for (const file of ['report_ui.js', 'front.js']) {
    const source = readFileSync(new URL('./' + file, import.meta.url), 'utf8');
    for (const m of source.matchAll(/\$\('([A-Za-z]+)'\)/g)) assert.ok(ids.has(m[1]), `${file} looks up #${m[1]}, which index.html does not have`);
  }
});
