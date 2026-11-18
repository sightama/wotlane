// ABOUTME: Tests for web/device_notice.js: the note on a player's own front panel about their browser (drawing without the graphics card,
// ABOUTME: or Brave). Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { frontNotice, SOFTWARE_GL_NOTICE } from './device_notice.js';
import { BRAVE_NOTICE } from './brave.js';

test('drawing in software comes first, in any browser; Brave alone gets the Brave notice; otherwise no notice', () => {
  assert.equal(frontNotice({ brave: false, softwareGl: 1 }), SOFTWARE_GL_NOTICE);
  assert.equal(frontNotice({ brave: true, softwareGl: 1 }), SOFTWARE_GL_NOTICE);
  assert.equal(frontNotice({ brave: true, softwareGl: 0 }), BRAVE_NOTICE);
  assert.equal(frontNotice({ brave: false, softwareGl: undefined }), '');
});

test('the software notice says what it costs and what to turn on', () => {
  assert.match(SOFTWARE_GL_NOTICE, /graphics card/);
  assert.match(SOFTWARE_GL_NOTICE, /acceleration/);
});
