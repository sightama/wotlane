// ABOUTME: Tests for web/support.js: the plain message a player gets when their browser can't run the game (only Chromium-based
// ABOUTME: browsers can: Firefox and Safari freeze while the game reads the disc), or none. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { supportProblem, mainThreadFiles, fileSize } = createRequire(import.meta.url)('./support.js');
const chrome = { chromium: true, firefox: false, jspi: true, isolated: true, mobile: false };
const firefox = { ...chrome, chromium: false, firefox: true };
const safari = { ...chrome, chromium: false, firefox: false };

test('desktop Chrome or Edge with everything: no message', () => {
  assert.equal(supportProblem(chrome), null);
});

test('Firefox with JSPI runs it (on range reads); without JSPI it asks to update', () => {
  assert.equal(supportProblem(firefox), null);
  assert.match(supportProblem({ ...firefox, jspi: false }), /update Firefox/);
});

test('Safari: says which browsers work', () => {
  assert.match(supportProblem(safari), /Chrome, Edge or Firefox/);
});

test('the disc and saves read on the main thread everywhere but Chromium (which keeps its proven worker backends)', () => {
  assert.equal(mainThreadFiles(chrome), false);
  assert.equal(mainThreadFiles(firefox), true);
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

test('a disc file size from a HEAD content-length, else from a range probe content-range total', () => {
  assert.equal(fileSize('7414808', null), 7414808);
  assert.equal(fileSize(null, 'bytes 0-0/1258815488'), 1258815488);
  assert.equal(fileSize('0', 'bytes 0-0/12'), 12);   // a HEAD with no body length says 0
  assert.equal(fileSize(null, 'bytes 0-0/*'), null);
  assert.equal(fileSize(null, null), null);
  assert.equal(fileSize('abc', null), null);
});
