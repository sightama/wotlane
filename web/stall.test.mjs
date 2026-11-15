// ABOUTME: Tests for web/stall.js: when the online match has waited a long time for another player, the page offers to keep waiting
// ABOUTME: or leave; keeping waiting asks again after the same time. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { StallPrompt } = createRequire(import.meta.url)('./stall.js');

test('no prompt before the limit', () => {
  const p = new StallPrompt(60000);
  assert.equal(p.show(0), false);
  assert.equal(p.show(59999), false);
});

test('the prompt shows once the wait passes the limit', () => {
  assert.equal(new StallPrompt(60000).show(60000), true);
});

test('keep waiting hides it until the wait grows by the limit again', () => {
  const p = new StallPrompt(60000);
  assert.equal(p.show(61000), true);
  p.keepWaiting(61000);
  assert.equal(p.show(90000), false);
  assert.equal(p.show(121000), true);
});

test('a new wait after the match moved on starts from zero', () => {
  const p = new StallPrompt(60000);
  p.keepWaiting(61000);
  assert.equal(p.show(0), false);   // the wait ended
  assert.equal(p.show(60000), true);
});

test('a match that stopped (the game reports a negative wait) shows at once, and keep waiting does not hide it', () => {
  const p = new StallPrompt(60000);
  assert.equal(p.stopped(-1), true);
  assert.equal(p.show(-1), true);
  p.keepWaiting(-1);
  assert.equal(p.show(-1), true);
  assert.equal(p.stopped(5000), false);
});
