// ABOUTME: Tests for web/support.js: the plain message a player gets when their browser can't run the game (only Chromium-based
// ABOUTME: browsers can: Firefox and Safari freeze while the game reads the disc), or none. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { supportProblem } = createRequire(import.meta.url)('./support.js');
const chrome = { chromium: true, jspi: true, isolated: true, mobile: false };

test('desktop Chrome or Edge with everything: no message', () => {
  assert.equal(supportProblem(chrome), null);
});

test('Firefox or Safari: says which browsers work', () => {
  assert.match(supportProblem({ ...chrome, chromium: false }), /Chrome or Edge/);
});

test('a phone or tablet: says it needs a computer', () => {
  assert.match(supportProblem({ ...chrome, mobile: true }), /computer/);
});

test('an old Chrome without JSPI: asks to update', () => {
  assert.match(supportProblem({ ...chrome, jspi: false }), /update/);
});

test('the page not isolated (a proxy dropped its headers): says to reload', () => {
  assert.match(supportProblem({ ...chrome, isolated: false }), /reload/i);
});
