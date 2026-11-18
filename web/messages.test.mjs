// ABOUTME: Tests for web/messages.js: every ERROR the lobby server can send is shown as a plain sentence, a text the page doesn't know
// ABOUTME: passes through unchanged. The server's ERROR texts are read from native/net/wotm_net_server.cpp. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';

const { friendlyError } = createRequire(import.meta.url)('./messages.js');
const server = readFileSync(new URL('../native/net/wotm_net_server.cpp', import.meta.url), 'utf8');

test('the lobby\'s ERROR texts are found in the server source', () => {
  const sent = [...server.matchAll(/"ERROR ([^"]*)"/g)].map((m) => m[1]);
  assert.ok(sent.length >= 20, `found ${sent.length}`);
});

test('every ERROR the server sends has a plain sentence', () => {
  // the literal part of each text; the pieces the server appends (a number, a name, a reason) don't matter to the match
  const sent = [...server.matchAll(/"ERROR ([^"]*)"/g)].map((m) => m[1]);
  // the reasons that follow a bare "ERROR " + err
  const reasons = ['invalid monster (no files on the disc)', 'that skin is already taken in the lobby (pick another skin or costume)',
    'monster has no free costume in the lobby (all its costumes are taken)'];
  // the texts whose literal ends before their number
  const whole = (s) => (s === 'the server already has ' ? `${s}4 lobbies open; join one of them` : s.endsWith('try again in ') ? `${s}42 s` : s);
  for (const text of [...sent.filter((s) => s.trim()).map(whole), ...reasons]) {
    const shown = friendlyError(text);
    assert.notEqual(shown, text, `no sentence for: ${text}`);
    assert.doesNotMatch(shown, /;/, `still reads like a server message: ${shown}`);
  }
});

test('the server\'s lobby limit says how many lobbies are busy', () => {
  assert.equal(friendlyError('the server already has 4 lobbies open; join one of them'), 'All 4 lobbies are busy. Try again soon.');
});

test('the refusals a friend meets most read as plain sentences', () => {
  assert.equal(friendlyError('no such lobby (check the code)'), 'No lobby has that code. Check the code and try again.');
  assert.equal(friendlyError('lobby is in a match; wait for it to end'), 'That lobby is in a match. Wait for it to end, then join.');
  assert.equal(friendlyError('lobby is full'), 'That lobby is full.');
  assert.equal(friendlyError('name already taken in the lobby'), 'Someone in that lobby already has that name. Pick another.');
  assert.equal(friendlyError('monster has no free costume in the lobby (all its costumes are taken)'),
    'Every costume of that monster is taken in this lobby. Pick another monster.');
});

test('"lobby is in a match; change monster" and "wait for it to end" are told apart', () => {
  assert.match(friendlyError('lobby is in a match; change monster after it ends'), /another monster/);
  assert.match(friendlyError('lobby is in a match; wait for it to end'), /Wait for it to end/);
});

test('an unknown text passes through unchanged', () => {
  assert.equal(friendlyError('something new the server says'), 'something new the server says');
  assert.equal(friendlyError(''), '');
});

test('the owner controls read as plain sentences', () => {
  assert.equal(friendlyError('only the lobby owner can remove a member'), 'Only the owner of the lobby can remove someone.');
  assert.equal(friendlyError('only the lobby owner can hand the lobby to someone else'), 'Only the owner of the lobby can hand it to someone else.');
  assert.equal(friendlyError('you can\'t remove yourself; leave instead'), 'To go, press Leave.');
  assert.equal(friendlyError('you are the owner already'), 'You are the owner already.');
  assert.equal(friendlyError('the match has started'), 'The match has started. That works again once it ends.');
  assert.equal(friendlyError('you were removed from this lobby; try again in 42 s'), 'The owner removed you, or someone on your connection, from that lobby. You can join it again in 42 seconds.');
});
