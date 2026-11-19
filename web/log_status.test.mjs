// ABOUTME: Tests for web/log_status.js: the status line a game log line calls for (a disc read that failed, a match refused because
// ABOUTME: the setups differ, for the refused player and for the ones already in). Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// log_status.js is a classic script (boot.js runs before any module): run it against a stand-in window
const window = {};
vm.runInNewContext(readFileSync(new URL('./log_status.js', import.meta.url), 'utf8'), { window });
const status = (line) => window.wotmLogStatus(line);

test('the refused player is told their setup differs from the players already in (the first to join sets it, not always the host)', () => {
  assert.match(status('[net] stopped: rejected by the server: game configuration differs from the others: the mods (creations) differ (a:b vs a:c)'),
    /^Your game setup differs from the other players' \(creations or settings\)/);
});

test('the players already in are told whose setup differs, and that the match did not start', () => {
  const s = status('[net] stopped: rejected by the server: the match did not start: Bia could not join (game configuration differs from the others: the mods (creations) differ (a:b vs a:c))');
  assert.match(s, /^Bia's game setup differs from yours/);
  assert.match(s, /creations/);
  assert.match(s, /then join again\.$/);   // a guest already in can't start the match
});

test('a disc read that failed for good says the connection was lost', () => {
  assert.match(status('[disc] LSN 12 (ISO) read FAILED after 4 tries'), /^Lost the connection to the game server/);
});

test('any other line, or another refusal, leaves the status alone', () => {
  assert.equal(status('[net] hash of frame 543: 37db1a1ab837d29f'), null);
  assert.equal(status('[net] stopped: rejected by the server: the match did not start: Bia could not join (the match is full)'), null);
});

test('boot.js asks log_status.js for each log line, and the page loads it first', () => {
  const boot = readFileSync(new URL('./boot.js', import.meta.url), 'utf8');
  assert.ok(boot.includes('window.wotmLogStatus(text)'), 'boot.js does not ask wotmLogStatus');
  const page = readFileSync(new URL('./index.html', import.meta.url), 'utf8');
  assert.ok(page.indexOf('<script src="log_status.js">') >= 0 && page.indexOf('<script src="log_status.js">') < page.indexOf('<script src="boot.js">'));
});
