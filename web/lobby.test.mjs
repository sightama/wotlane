// ABOUTME: Tests for web/lobby.js: lobby lines from wotm_server, the game environment for a MATCH line (the same mapping as
// ABOUTME: tools/net_lobby.py's match_env) and the lobby client's requests and replies over a fake WebSocket. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { PROTOCOL, parseLine, parseMembers, matchEnv, lobbySocketUrl, lastLobbyNotice, LobbyClient } from './lobby.js';

const MATCH_LINE = 'MATCH lobby=QZXW players=2 stage=9 wins=2 mods=jump,turbo monsters=5,1 names=Ana,Bia ' +
  'seed=12345 step=1 you=1 token=00aa11bb22cc33dd port=7777';

test('parseLine splits the command, its words and its key=value pairs', () => {
  const m = parseLine(MATCH_LINE);
  assert.equal(m.cmd, 'MATCH');
  assert.equal(m.fields.token, '00aa11bb22cc33dd');
  assert.equal(m.fields.mods, 'jump,turbo');
  const e = parseLine('ERROR no such lobby (check the code)');
  assert.equal(e.cmd, 'ERROR');
  assert.equal(e.rest, 'no such lobby (check the code)');
  assert.deepEqual(parseLine('CREATED ABCD\r').words, ['ABCD']);
});

test('parseMembers reads the owner and each member with their monster and skin', () => {
  assert.deepEqual(parseMembers(parseLine('MEMBERS QZXW Ana Ana:5,Bia:1')),
    { code: 'QZXW', owner: 'Ana', members: [{ name: 'Ana', monster: 5, skin: '' }, { name: 'Bia', monster: 1, skin: '' }] });
  assert.deepEqual(parseMembers(parseLine('MEMBERS QZXW Ana Ana:3:green,Bia:1')).members,
    [{ name: 'Ana', monster: 3, skin: 'green' }, { name: 'Bia', monster: 1, skin: '' }]);
  assert.deepEqual(parseMembers(parseLine('MEMBERS QZXW - -')), { code: 'QZXW', owner: '', members: [] });
});

test('matchEnv equals the Python mapping, with the match WebSocket as WOTM_NET', () => {
  // expected values: tools/net_lobby.py match_env on the same line (host replaced by the WebSocket URL)
  assert.deepEqual(matchEnv(parseLine(MATCH_LINE).fields, 'wss://example.org/match'), {
    WOTM_FIXEDSTEP: '1',
    WOTM_KILLTARGET: '2',
    WOTM_LOCALPLAYER: '2',
    WOTM_MATCH: '5,1,9',
    WOTM_MODIFIERS: 'jump,turbo',
    WOTM_NET: 'wss://example.org/match',
    WOTM_NETNAME: 'Bia',
    WOTM_NETTOKEN: '00aa11bb22cc33dd',
    WOTM_PLAYERS: '2',
    WOTM_SEED: '12345',
  });
});

test('lobbySocketUrl follows the page: wss on https, ws on http, same host', () => {
  assert.equal(lobbySocketUrl({ protocol: 'https:', host: 'example.org' }, '/lobby'), 'wss://example.org/lobby');
  assert.equal(lobbySocketUrl({ protocol: 'http:', host: '127.0.0.1:8080' }, '/match'), 'ws://127.0.0.1:8080/match');
});

// a WebSocket stand-in: records what the client sends; the test plays the server with reply()
class FakeSocket {
  constructor() { this.sent = []; this.readyState = 0; }
  send(text) { this.sent.push(text); }
  close() { this.readyState = 3; this.onclose?.({}); }
  open() { this.readyState = 1; this.onopen?.({}); }
  reply(line) { this.onmessage?.({ data: line }); }
}

async function connected(build = 'wl-1') {
  const ws = new FakeSocket();
  const client = new LobbyClient(ws, build);
  const ready = client.ready;
  ws.open();
  assert.equal(ws.sent[0], `HELLO ${PROTOCOL} wl-1`);
  ws.reply('WELCOME 3');
  await ready;
  return { ws, client };
}

test('LobbyClient says HELLO with the protocol and build, then creates a lobby and gets its code', async () => {
  const { ws, client } = await connected();
  const code = client.create('Ana', 5);
  assert.equal(ws.sent[1], 'CREATE Ana Ana 5');
  ws.reply('CREATED QZXW');
  ws.reply('JOINED QZXW');
  ws.reply('SETTINGS QZXW stage=1 wins=0 mods=');
  assert.equal(await code, 'QZXW');
  assert.deepEqual(client.settings, { stage: '1', wins: '0', mods: '' });
});

test('LobbyClient join is refused with the server reason', async () => {
  const { ws, client } = await connected();
  const join = client.join('ZZZZ', 'Bia', 1);
  assert.equal(ws.sent[1], 'JOIN ZZZZ Bia 1');
  ws.reply('ERROR no such lobby (check the code)');
  await assert.rejects(join, /no such lobby/);
});

test('a MATCH with a skin and a saved map goes into WOTM_MATCH as the game reads it', () => {
  const env = matchEnv({ you: '0', token: 't', names: 'Ana,Bia', monsters: '3:green,1', stage: 't1', players: '2', step: '1', seed: '7' },
    'ws://h/match');
  assert.equal(env.WOTM_MATCH, '3:green,1,t1');
});

test('the lobby speaks protocol 5 (map ids and skins)', () => {
  assert.equal(PROTOCOL, 5);
});

test('LobbyClient reports members, settings and the match as they arrive', async () => {
  const { ws, client } = await connected();
  const seen = [];
  client.onMembers = (m) => seen.push(['members', m.members.length]);
  client.onSettings = (s) => seen.push(['settings', s.stage]);
  client.onMatch = (m) => seen.push(['match', m.you]);
  ws.reply('MEMBERS QZXW Ana Ana:5,Bia:1');
  ws.reply('SETTINGS QZXW stage=9 wins=2 mods=');
  ws.reply(MATCH_LINE);
  assert.deepEqual(seen, [['members', 2], ['settings', '9'], ['match', '1']]);
});

test('LobbyClient set, monster and start send the owner commands', async () => {
  const { ws, client } = await connected();
  const set = client.set({ stage: 9, wins: 2, mods: 'jump' });
  assert.equal(ws.sent[1], 'SET stage=9 wins=2 mods=jump');
  ws.reply('SETTINGS QZXW stage=9 wins=2 mods=jump');
  assert.deepEqual(await set, { stage: '9', wins: '2', mods: 'jump' });
  const monster = client.monster(7);
  ws.reply('MEMBERS QZXW Ana Ana:7');
  await monster;
  const start = client.start();
  assert.deepEqual(ws.sent.slice(2), ['MONSTER 7', 'START']);
  ws.reply(MATCH_LINE);
  await start;
});

test('LobbyClient fails pending requests when the connection closes', async () => {
  const { ws, client } = await connected();
  let closed = false;
  client.onClose = () => { closed = true; };
  const join = client.join('QZXW', 'Bia', 1);
  ws.close();
  await assert.rejects(join, /connection/);
  assert.ok(closed);
});

test('LobbyClient: a refused START rejects the start itself', async () => {
  const { ws, client } = await connected();
  const start = client.start();
  ws.reply('ERROR only the lobby owner can start the match');
  await assert.rejects(start, /only the lobby owner/);
});

test('LobbyClient: a refused MONSTER is charged to the monster, not to a SET sent after it', async () => {
  const { ws, client } = await connected();
  const monster = client.monster(7);
  const set = client.set({ mods: 'fast' });
  ws.reply('ERROR monster has no free costume in the lobby (all its costumes are taken)');
  ws.reply('SETTINGS QZXW stage=1 wins=0 mods=fast');
  await assert.rejects(monster, /no free costume/);
  assert.deepEqual(await set, { stage: '1', wins: '0', mods: 'fast' });
});

test('LobbyClient reports an ERROR no request waits for to onError', async () => {
  const { ws, client } = await connected();
  const errors = [];
  client.onError = (text) => errors.push(text);
  ws.reply('ERROR the server is restarting');
  assert.deepEqual(errors, ['the server is restarting']);
});

test('LobbyClient: closing it on purpose does not report a lost connection', async () => {
  const { ws, client } = await connected();
  let closed = false;
  client.onClose = () => { closed = true; };
  client.close();
  assert.equal(closed, false);
  assert.equal(ws.readyState, 3);
});

test('lastLobbyNotice speaks only of a lobby the server still lists', () => {
  const lobbies = [{ code: 'ABCD', playing: false }, { code: 'WXYZ', playing: true }];
  assert.match(lastLobbyNotice('ABCD', lobbies), /ABCD.*Join/);
  assert.match(lastLobbyNotice('WXYZ', lobbies), /in a match/);
  assert.equal(lastLobbyNotice('QQQQ', lobbies), '');
});

test('LobbyClient.list reads the LOBBY lines up to END', async () => {
  const { ws, client } = await connected();
  const list = client.list();
  assert.equal(ws.sent.at(-1), 'LIST');
  ws.reply('LOBBY ABCD waiting 1/8 open Ana Ana Ana');
  ws.reply('LOBBY WXYZ playing 2/8 locked Bob Bob Bob,Cy');
  ws.reply('END');
  assert.deepEqual(await list, [{ code: 'ABCD', playing: false }, { code: 'WXYZ', playing: true }]);
});

test('LobbyClient: a refused HELLO closes the socket quietly, so the refusal stays on screen', async () => {
  const ws = new FakeSocket();
  const client = new LobbyClient(ws, 'wl-1');
  let closed = false;
  client.onClose = () => { closed = true; };
  ws.open();
  ws.reply('ERROR build differs');
  await assert.rejects(client.ready, /build differs/);
  assert.equal(ws.readyState, 3);
  assert.equal(closed, false);
});
