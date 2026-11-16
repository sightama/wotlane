// ABOUTME: Tests for web/loading.js: the loading text over the game while the disc streams in (percent of the expected download,
// ABOUTME: then nothing once the downloads go quiet). Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { LoadingProgress } = createRequire(import.meta.url)('./loading.js');
const MB = 1 << 20;

test('nothing to show before the first chunk arrives', () => {
  assert.equal(new LoadingProgress(10 * MB).text(0), 'Downloading game data 0% (0 of 10 MB). Only the first visit downloads this much.');
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
