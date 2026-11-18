// ABOUTME: Tests for web/device_notice.js: the note on a player's own front panel when their browser draws without the graphics card.
// ABOUTME: Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { frontNotice, SOFTWARE_GL_NOTICE } from './device_notice.js';

test('drawing in software, in any browser, gets the notice; otherwise (Brave with the graphics card too) no notice', () => {
  assert.equal(frontNotice({ softwareGl: 1 }), SOFTWARE_GL_NOTICE);
  assert.equal(frontNotice({ softwareGl: 0 }), '');
  assert.equal(frontNotice({ softwareGl: undefined }), '');
});

test('the software notice says what it costs and what to turn on', () => {
  assert.match(SOFTWARE_GL_NOTICE, /graphics card/);
  assert.match(SOFTWARE_GL_NOTICE, /acceleration/);
});
