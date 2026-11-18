// ABOUTME: Tests for web/lobby.js: lobby lines from wotm_server, the game environment for a MATCH line (the same mapping as
// ABOUTME: tools/net_lobby.py's match_env) and the lobby client's requests and replies over a fake WebSocket. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { PROTOCOL, parseLine, parseMembers, matchEnv, lobbySocketUrl, lastLobbyNotice, inviteLink, lobbyRow, parsePings, parseHealth, parseChat, LobbyClient, halfRateDevice, phoneDevice } from './lobby.js';

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
  assert.deepEqual(await list, [
    { code: 'ABCD', playing: false, count: 1, max: 8, locked: false, name: 'Ana', host: 'Ana', names: ['Ana'] },
    { code: 'WXYZ', playing: true, count: 2, max: 8, locked: true, name: 'Bob', host: 'Bob', names: ['Bob', 'Cy'] },
  ]);
});

test('LobbyClient on a device that cannot compute 60 game frames a second says so in its HELLO (hz=30)', async () => {
  const ws = new FakeSocket();
  const client = new LobbyClient(ws, 'wl-1', { halfRate: true });
  const ready = client.ready;
  ws.open();
  assert.equal(ws.sent[0], `HELLO ${PROTOCOL} wl-1 hz=30`);
  ws.reply('WELCOME 0');
  await ready;
});

test('halfRateDevice: a phone or tablet plays online at 30 Hz whatever it plays with, a desktop at 60', () => {
  const coarse = (q) => ({ matches: q === '(pointer: coarse)' });
  const none = () => ({ matches: false });
  const desktopUa = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36';
  const pixelUa = 'Mozilla/5.0 (Linux; Android 15; Pixel 9 Pro XL) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Mobile Safari/537.36';
  // the browser says it is a mobile device (User-Agent Client Hints), even with a pad connected and no touch reported
  assert.equal(halfRateDevice({ matchMedia: none, navigator: { userAgentData: { mobile: true }, userAgent: desktopUa } }), true);
  // an Android or mobile User-Agent
  assert.equal(halfRateDevice({ matchMedia: none, navigator: { userAgent: pixelUa, maxTouchPoints: 0 } }), true);
  // a touch screen that is the primary pointer
  assert.equal(halfRateDevice({ matchMedia: coarse, navigator: { userAgent: desktopUa, maxTouchPoints: 5 } }), true);
  // a desktop, with or without a touch screen next to its mouse
  assert.equal(halfRateDevice({ matchMedia: none, navigator: { userAgentData: { mobile: false }, userAgent: desktopUa, maxTouchPoints: 10 } }), false);
  assert.equal(halfRateDevice({ matchMedia: none, navigator: { userAgent: desktopUa } }), false);
  assert.equal(halfRateDevice({}), false);
  // a desktop this browser measured unable to hold 60 game frames a second in its last 60 Hz match (web/rate.js)
  assert.equal(halfRateDevice({ matchMedia: none, navigator: { userAgent: desktopUa }, localStorage: { getItem: (k) => (k === 'wotlane.slowDevice' ? '1' : null) } }), true);
  assert.equal(halfRateDevice({ matchMedia: none, navigator: { userAgent: desktopUa }, localStorage: { getItem: () => '0' } }), false);
  // that desktop is still not a phone: it keeps its own drawing settings (settings.js lightMatchDrawing)
  assert.equal(phoneDevice({ matchMedia: none, navigator: { userAgent: desktopUa }, localStorage: { getItem: () => '1' } }), false);
  assert.equal(phoneDevice({ matchMedia: none, navigator: { userAgent: pixelUa } }), true);
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

test('leave() sends LEAVE and resolves with the code on LEFT', async () => {
  const { ws, client } = await connected();
  const left = client.leave();
  assert.equal(ws.sent[1], 'LEAVE');
  ws.reply('LEFT QZXW');
  assert.equal(await left, 'QZXW');
});

test('leave() is refused with the server reason', async () => {
  const { ws, client } = await connected();
  const left = client.leave();
  ws.reply('ERROR join a lobby first');
  await assert.rejects(left, /join a lobby first/);
});

test('inviteLink gives the page address with only the lobby code', () => {
  assert.equal(inviteLink({ origin: 'https://x.example', pathname: '/' }, 'ABCD'), 'https://x.example/?join=ABCD');
  assert.equal(inviteLink({ origin: 'https://x.example', pathname: '/play/' }, 'QZ2W'), 'https://x.example/play/?join=QZ2W');
});

test('LobbyClient.list reads a lobby with no owner and no names', async () => {
  const { ws, client } = await connected();
  const list = client.list();
  ws.reply('LOBBY ABCD waiting 0/8 open Ana - -');
  ws.reply('END');
  assert.deepEqual(await list, [{ code: 'ABCD', playing: false, count: 0, max: 8, locked: false, name: 'Ana', host: '', names: [] }]);
});

test('lobbyRow names the lobby after its name when it has no owner', () => {
  assert.equal(lobbyRow({ code: 'ABCD', playing: false, count: 0, max: 8, locked: false, name: 'Ana', host: '', names: [] }).text, 'Ana: 0/8 (), waiting');
});

test('lobbyRow says who hosts, how full it is and whether Join works', () => {
  const open = { code: 'ABCD', playing: false, count: 2, max: 8, locked: false, name: 'Ana', host: 'Ana', names: ['Ana', 'Bo'] };
  assert.deepEqual(lobbyRow(open), { text: 'Ana: 2/8 (Ana, Bo), waiting', joinable: true });
  assert.deepEqual(lobbyRow({ ...open, playing: true }), { text: 'Ana: 2/8 (Ana, Bo), in a match', joinable: false });
  assert.deepEqual(lobbyRow({ ...open, count: 8 }), { text: 'Ana: 8/8 (Ana, Bo), full', joinable: false });
  assert.deepEqual(lobbyRow({ ...open, locked: true }), { text: 'Ana: 2/8 (Ana, Bo), waiting, needs a password', joinable: false });
});

test('parsePings reads each member\'s round trip, null where the server has none', () => {
  assert.deepEqual(parsePings(parseLine('PINGS QZXW Ana:-,Bia:23,Cy:0')), { Ana: null, Bia: 23, Cy: 0 });
});

test('LobbyClient hands PINGS lines to onPings', async () => {
  const { ws, client } = await connected();
  const seen = [];
  client.onPings = (p) => seen.push(p);
  ws.reply('PINGS QZXW Ana:-,Bia:23');
  assert.deepEqual(seen, [{ Ana: null, Bia: 23 }]);
});

test('parseChat splits the sender from the text, which keeps its spaces', () => {
  assert.deepEqual(parseChat(parseLine('CHAT QZXW Ana hello  there, all')), { name: 'Ana', text: 'hello  there, all' });
});

test('LobbyClient hands CHAT lines to onChat and sends chat without waiting for a reply', async () => {
  const { ws, client } = await connected();
  const seen = [];
  client.onChat = (m) => seen.push(m);
  client.chat('hi all');
  assert.equal(ws.sent.at(-1), 'CHAT hi all');
  ws.reply('CHAT QZXW Ana hi all');
  assert.deepEqual(seen, [{ name: 'Ana', text: 'hi all' }]);
  assert.equal(client.waiting.length, 0);
});

test('LobbyClient sends a diagnostics line without waiting for a reply', async () => {
  const { ws, client } = await connected();
  assert.equal(client.diag('DIAG v=1 why=join'), true);
  assert.equal(ws.sent.at(-1), 'DIAG v=1 why=join');
  assert.equal(client.waiting.length, 0);
});

test('a server that does not know DIAG answers ERROR: that refuses no request, and nothing more is sent to it', async () => {
  const { ws, client } = await connected();
  const errors = [];
  client.onError = (text) => errors.push(text);
  const created = client.create('Ana', 5);
  client.diag('DIAG v=1 why=join');
  ws.reply('ERROR unknown command: DIAG');   // the old server's answer to the line, which arrives while CREATE waits
  assert.equal(client.waiting.length, 1);    // CREATE still waits for its own reply
  ws.reply('CREATED QZXW');
  ws.reply('JOINED QZXW');
  ws.reply('SETTINGS QZXW stage=1 wins=0 mods=');
  assert.equal(await created, 'QZXW');
  assert.deepEqual(errors, []);
  assert.equal(client.diagUnsupported, true);
  const before = ws.sent.length;
  assert.equal(client.diag('DIAG v=1 why=periodic'), false);
  assert.equal(ws.sent.length, before);
});

test('a diagnostics line is delivered once the server has answered a LIST sent after it', async () => {
  const { ws, client } = await connected();
  const done = client.sendDiag('DIAG v=1 why=report note=laggy');
  assert.equal(ws.sent.at(-2), 'DIAG v=1 why=report note=laggy');
  assert.equal(ws.sent.at(-1), 'LIST');
  ws.reply('END');
  assert.equal(await done, true);
  const old = await connected();
  old.client.diagUnsupported = true;
  assert.equal(await old.client.sendDiag('DIAG v=1'), false);   // nothing sent, nothing to wait for
});

test('parseHealth reads each member\'s own frames a second, work per frame and game Hz', () => {
  assert.deepEqual(parseHealth(parseLine('HEALTH QZXW Ana:60.0:4.5:30,Bia:28.5:13.2:60')),
    { Ana: { fps: 60, workMs: 4.5, hz: 30 }, Bia: { fps: 28.5, workMs: 13.2, hz: 60 } });
  assert.deepEqual(parseHealth(parseLine('HEALTH QZXW')), {});
});

test('parseHealth reads a member drawing in software, measured or not yet', () => {
  assert.deepEqual(parseHealth(parseLine('HEALTH QZXW Ana:5.3:40.0:60:sw,Bia:-:-:-:sw')),
    { Ana: { fps: 5.3, workMs: 40, hz: 60, softwareGl: true }, Bia: { softwareGl: true } });
});

test('LobbyClient hands HEALTH lines to onHealth and sends its own without waiting for a reply', async () => {
  const { ws, client } = await connected();
  const seen = [];
  client.onHealth = (h) => seen.push(h);
  client.health('HEALTH 27.4 13.2 60');
  assert.equal(ws.sent.at(-1), 'HEALTH 27.4 13.2 60');
  assert.equal(client.waiting.length, 0);
  ws.reply('HEALTH QZXW Bia:27.4:13.2:60');
  assert.deepEqual(seen, [{ Bia: { fps: 27.4, workMs: 13.2, hz: 60 } }]);
});

test('a server that does not know HEALTH answers ERROR: that refuses no request, and nothing more is sent to it', async () => {
  const { ws, client } = await connected();
  const pending = client.list();
  client.health('HEALTH 27.4 13.2 60');
  ws.reply('ERROR unknown command: HEALTH');
  ws.reply('END');
  assert.deepEqual(await pending, []);
  const sent = ws.sent.length;
  client.health('HEALTH 27.4 13.2 60');
  assert.equal(ws.sent.length, sent);
});

test('a server from before "sw" refuses that HEALTH with an ERROR: that refuses no request', async () => {
  const { ws, client } = await connected();
  const pending = client.list();
  client.health('HEALTH sw');
  ws.reply('ERROR HEALTH <own fps> <work ms> <hz>: numbers in range');
  ws.reply('END');
  assert.deepEqual(await pending, []);
});

test('LobbyClient kick and handOver send the owner commands and wait for the new member list', async () => {
  const { ws, client } = await connected();
  const kicked = client.kick('Bia');
  assert.equal(ws.sent.at(-1), 'KICK Bia');
  ws.reply('MEMBERS QZXW Ana Ana:5');
  await kicked;
  const handed = client.handOver('Cy');
  assert.equal(ws.sent.at(-1), 'OWNER Cy');
  ws.reply('MEMBERS QZXW Cy Ana:5,Cy:1');
  await handed;
});

test('LobbyClient: a refused KICK is rejected with the server reason', async () => {
  const { ws, client } = await connected();
  const kicked = client.kick('Zed');
  ws.reply('ERROR Zed is not in this lobby');
  await assert.rejects(kicked, /Zed is not in this lobby/);
});

test('LobbyClient hands KICKED to onKicked with the ban in seconds, and forgets the lobby', async () => {
  const { ws, client } = await connected();
  const seen = [];
  client.onKicked = (k) => seen.push(k);
  client.code = 'QZXW';
  ws.reply('KICKED QZXW 60');
  assert.deepEqual(seen, [{ code: 'QZXW', seconds: 60 }]);
  assert.equal(client.code, '');
});
