// ABOUTME: Tests for web/loading.js: the loading text over the game while the disc streams in (percent of the expected download,
// ABOUTME: then nothing once the downloads go quiet). Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { LoadingProgress } = createRequire(import.meta.url)('./loading.js');
const MB = 1 << 20;

test('nothing to show before the first chunk arrives', () => {
  assert.equal(new LoadingProgress(10 * MB).text(0), 'Downloading game data 0%');
});

test('the percent follows the bytes against the expected size', () => {
  const p = new LoadingProgress(10 * MB);
  p.add(3 * MB, 100);
  p.add(MB / 2, 200);
  assert.equal(p.text(250), 'Downloading game data 35%');
});

test('more than expected stays under 100% while chunks keep coming', () => {
  const p = new LoadingProgress(10 * MB);
  p.add(12 * MB, 100);
  assert.equal(p.text(200), 'Downloading game data 99%');
});

test('the text goes away once no chunk arrived for a while', () => {
  const p = new LoadingProgress(10 * MB, { quietMs: 1500 });
  p.add(4 * MB, 1000);
  assert.equal(p.text(2400), 'Downloading game data 40%');
  assert.equal(p.text(2600), null);
  assert.equal(p.done, true);
});
