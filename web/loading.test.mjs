// ABOUTME: Tests for web/loading.js: the loading text over the game while the disc streams in (percent of the expected download,
// ABOUTME: then nothing once the downloads go quiet), and the note while a later load waits on the network. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { LoadingProgress, StreamActivity, BootStatus } = createRequire(import.meta.url)('./loading.js');
const MB = 1 << 20;

test('shows the initial label before the first chunk arrives instead of 0% downloading', () => {
  assert.equal(new LoadingProgress(10 * MB).text(0), 'Starting...');
  assert.equal(new LoadingProgress(10 * MB, { initial: 'Starting match...' }).text(0), 'Starting match...');
});

test('cache hits do not count as downloaded bytes and show loading from cache', () => {
  const p = new LoadingProgress(10 * MB, { quietMs: 1500 });
  p.addCacheHit(4 * MB, 100);
  p.addCacheHit(6 * MB, 200);
  assert.equal(p.text(300), 'Loading from cache...');
  // if a network chunk arrives afterwards, only network bytes count toward the download percent
  p.add(2 * MB, 400);
  assert.equal(p.text(500), 'Downloading game data 20% (2 of 10 MB). Only the first visit downloads this much.');
  // once quiet, it finishes
  assert.equal(p.text(2000), null);
  assert.equal(p.done, true);
});

test('finish ends loading text immediately before the quiet timeout', () => {
  const p = new LoadingProgress(10 * MB, { quietMs: 1500 });
  p.addCacheHit(2 * MB, 100);
  assert.equal(p.text(200), 'Loading from cache...');
  p.finish();
  assert.equal(p.text(250), null);
  assert.equal(p.done, true);
});

test('the percent follows the bytes against the expected size', () => {
  const p = new LoadingProgress(10 * MB);
  p.add(3 * MB, 100);
  p.add(MB / 2, 200);
  assert.equal(p.text(250), 'Downloading game data 35% (3 of 10 MB). Only the first visit downloads this much.');
});

test('more than expected stays at 99%, shows no more MB than expected and drops the first-visit sentence', () => {
  const p = new LoadingProgress(10 * MB);
  p.add(12 * MB, 100);
  assert.equal(p.text(200), 'Downloading game data 99% (10 of 10 MB).');
});

test('the text goes away once no chunk arrived for a while', () => {
  const p = new LoadingProgress(10 * MB, { quietMs: 1500 });
  p.add(4 * MB, 1000);
  assert.equal(p.text(2400), 'Downloading game data 40% (4 of 10 MB). Only the first visit downloads this much.');
  assert.equal(p.text(2600), null);
  assert.equal(p.done, true);
});

test('the text rounds the downloaded MB down and takes the expected MB from the constructor', () => {
  const p = new LoadingProgress(27 * MB);
  p.add(9 * MB + MB / 2, 100);
  assert.equal(p.text(200), 'Downloading game data 35% (9 of 27 MB). Only the first visit downloads this much.');
});

test('the first-visit sentence stays while the download is under 100%', () => {
  const p = new LoadingProgress(10 * MB);
  p.add(10 * MB - 1, 100);
  assert.match(p.text(200), /99%.*Only the first visit/);
  p.add(1, 150);
  assert.equal(p.text(200), 'Downloading game data 99% (10 of 10 MB).');
});

// after boot: a stage or monster that isn't cached yet streams in while the game waits (issue #112: a black screen for seconds)
test('a later download shows a note once it has gone on for a second, with its own MB', () => {
  const a = new StreamActivity();
  a.add(MB, 10000);
  assert.equal(a.text(10500), null);   // a quick top-up never flashes the note
  a.add(2 * MB, 10900);
  assert.equal(a.text(11100), 'Loading game data (3 MB)');
});

test('the note goes away once the downloads go quiet; its count goes on within the same load and starts over after a long quiet', () => {
  const a = new StreamActivity({ quietMs: 1500, resetMs: 10000 });
  a.add(MB, 0);
  a.add(MB, 1200);
  assert.equal(a.text(1300), 'Loading game data (2 MB)');
  assert.equal(a.text(2800), null);
  a.add(MB, 6000);   // 4.8 s later: the same load, the count goes on
  a.add(MB, 7100);
  assert.equal(a.text(7200), 'Loading game data (4 MB)');
  a.add(MB, 30000);   // a new load
  a.add(MB, 31100);
  assert.equal(a.text(31200), 'Loading game data (2 MB)');
});

test('the note rounds the MB down and leaves the number out under one', () => {
  const a = new StreamActivity();
  a.add(MB / 2, 0);
  a.add(MB / 4, 1100);
  assert.equal(a.text(1200), 'Loading game data');
});

test('by default the note stays across the game\'s short gaps between the files of one load', () => {
  const a = new StreamActivity();
  a.add(MB, 0);
  a.add(MB, 1100);
  assert.equal(a.text(4000), 'Loading game data (2 MB)');   // 2.9 s without a chunk: still the same load
  a.add(MB, 4200);
  assert.equal(a.text(4300), 'Loading game data (3 MB)');
  assert.equal(a.text(7800), null);
});

test('BootStatus advances through online match phases monotonically until frame 1', () => {
  const s = new BootStatus({ online: true, otherNames: ['Bob'] });
  assert.equal(s.text, 'Starting match...');
  assert.equal(s.setEmscriptenStatus('Downloading...'), 'Downloading...');
  assert.equal(s.setEmscriptenStatus(''), 'Connecting to match...');
  // a late non-empty Emscripten status must not overwrite Connecting to match
  assert.equal(s.setEmscriptenStatus('Preparing...'), null);
  assert.equal(s.onLog('[net] connecting to wss://host/match as Ana (build x, config y)', null), 'Waiting for Bob...');
  // a late Emscripten setStatus('') must not regress back to Connecting
  assert.equal(s.setEmscriptenStatus(''), null);
  assert.equal(s.setEmscriptenStatus('Preparing...'), null);
  assert.equal(s.onLog('[net] start: player 1 of 2, delay 3 frames, ping 28 ms', null), 'Loading stage...');
  // frame 0 does not yet flip to Running
  assert.equal(s.onFrame(0), null);
  assert.equal(s.onFrame(1), 'Running');
  assert.equal(s.onFrame(2), null);
});

test('BootStatus falls back to Waiting for other players when no peer names were given', () => {
  const s = new BootStatus({ online: true });
  assert.equal(s.onLog('[net] connecting to wss://host/match as Ana', null), 'Waiting for other players...');
});

test('BootStatus transitions single player to Running when Emscripten clears status', () => {
  const s = new BootStatus({ online: false });
  assert.equal(s.text, 'Starting...');
  assert.equal(s.setEmscriptenStatus('Preparing...'), 'Preparing...');
  assert.equal(s.setEmscriptenStatus(''), 'Running');
  assert.equal(s.onLog('[net] connecting to wss://host/match as Ana', null), null);
});

test('BootStatus locks terminal log errors and aborts against later updates', () => {
  const s = new BootStatus({ online: true, otherNames: ['Bob'] });
  assert.equal(s.onLog('[disc] LSN 12 (ISO) read FAILED', 'Lost the connection.'), 'Lost the connection.');
  assert.equal(s.setEmscriptenStatus(''), null);
  assert.equal(s.onLog('[net] start: player 1 of 2', null), null);
  assert.equal(s.onFrame(5), null);

  const a = new BootStatus({ online: true });
  assert.equal(a.onAbort('oom'), 'Stopped: oom');
  assert.equal(a.onFrame(1), null);
});
