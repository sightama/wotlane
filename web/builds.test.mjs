// ABOUTME: Tests for web/builds.js: the newest retained build of a simulation id from /b/index.json, and where a page goes when a
// ABOUTME: lobby runs another build (its retained copy, with the lobby code), or that it stays when none is kept. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildForSim, lobbyBuildUrl, siteRoot } from './builds.js';

const index = { builds: [
  { full: 'wl-aaaa000000000001', sim: 'wl-5111111111111111', time: '2026-11-24T10:00:00Z' },
  { full: 'wl-aaaa000000000002', sim: 'wl-5111111111111111', time: '2026-11-25T10:00:00Z' },
  { full: 'wl-bbbb000000000001', sim: 'wl-5222222222222222', time: '2026-11-25T11:00:00Z' },
] };

test('the newest retained build with a simulation id', () => {
  assert.equal(buildForSim(index, 'wl-5111111111111111'), 'wl-aaaa000000000002');
  assert.equal(buildForSim(index, 'wl-5222222222222222'), 'wl-bbbb000000000001');
  assert.equal(buildForSim(index, 'wl-5333333333333333'), null);
  assert.equal(buildForSim({}, 'wl-5111111111111111'), null);
  assert.equal(buildForSim(null, 'wl-5111111111111111'), null);
});

test('the site root above a build address', () => {
  assert.equal(siteRoot('/b/wl-aaaa000000000002/'), '/');
  assert.equal(siteRoot('/play/b/wl-aaaa000000000002/'), '/play/');
  assert.equal(siteRoot('/'), '/');
  assert.equal(siteRoot('/play/'), '/play/');
});

test('a lobby on another build sends the page to that build with the code', () => {
  const err = "build differs from the lobby's (wl-5111111111111111); everyone needs the same wotlane";
  const at = { pathname: '/b/wl-bbbb000000000001/', origin: 'https://x.example' };
  assert.equal(lobbyBuildUrl(err, 'QZ2W', index, at), 'https://x.example/b/wl-aaaa000000000002/?join=QZ2W');
});

test('the move keeps the page address\'s other parameters (a local test\'s ?server=)', () => {
  const err = "build differs from the lobby's (wl-5111111111111111); everyone needs the same wotlane";
  const at = { pathname: '/b/wl-bbbb000000000001/', origin: 'http://127.0.0.1:8000', search: '?server=ws://127.0.0.1:9000&join=OLD1' };
  assert.equal(lobbyBuildUrl(err, 'QZ2W', index, at), 'http://127.0.0.1:8000/b/wl-aaaa000000000002/?server=ws%3A%2F%2F127.0.0.1%3A9000&join=QZ2W');
});

test('no move: another error, a build not kept, the page already there, or no code', () => {
  const at = { pathname: '/b/wl-bbbb000000000001/', origin: 'https://x.example' };
  assert.equal(lobbyBuildUrl('lobby is full', 'QZ2W', index, at), null);
  assert.equal(lobbyBuildUrl("build differs from the lobby's (wl-5333333333333333); everyone", 'QZ2W', index, at), null);
  const there = { pathname: '/b/wl-aaaa000000000002/', origin: 'https://x.example' };
  assert.equal(lobbyBuildUrl("build differs from the lobby's (wl-5111111111111111); everyone", 'QZ2W', index, there), null);
  assert.equal(lobbyBuildUrl("build differs from the lobby's (wl-5111111111111111); everyone", '', index, at), null);
  // a build id is only letters, digits and dashes: anything else in the message is not followed
  assert.equal(lobbyBuildUrl("build differs from the lobby's (../x); everyone", 'QZ2W', index, at), null);
});
