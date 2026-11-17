// ABOUTME: Tests for web/support.js: the plain message a player gets when their browser can't run the game (only Chromium-based
// ABOUTME: browsers can: Firefox and Safari freeze while the game reads the disc), or none. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { supportProblem, mainThreadFiles, fileSize, guardPointerLock } = createRequire(import.meta.url)('./support.js');
const chrome = { chromium: true, firefox: false, jspi: true, isolated: true, ios: false };
const firefox = { ...chrome, chromium: false, firefox: true };
const safari = { ...chrome, chromium: false, firefox: false };

test('desktop Chrome or Edge with everything: no message', () => {
  assert.equal(supportProblem(chrome), null);
});

test('Firefox with JSPI runs it (on range reads); without JSPI it asks to update', () => {
  assert.equal(supportProblem(firefox), null);
  assert.match(supportProblem({ ...firefox, jspi: false }), /update Firefox/);
});

test('an unsupported browser (Safari): says which browsers work', () => {
  assert.match(supportProblem(safari), /Chrome, Firefox or Edge on a computer or an Android phone/);
});

test('the disc and saves read on the main thread everywhere but Chromium (which keeps its proven worker backends)', () => {
  assert.equal(mainThreadFiles(chrome), false);
  assert.equal(mainThreadFiles(firefox), true);
});

test('an iPhone or iPad: says why it can not run, and what does', () => {
  const msg = supportProblem({ ...safari, ios: true });
  assert.match(msg, /iPhone/);
  assert.match(msg, /Android/);
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

test('a pointer lock request on a tab that is not focused neither throws (Firefox) nor leaves a rejected promise (Chrome)', async () => {
  const calls = [];
  const throwing = { requestPointerLock(...a) { calls.push(a); throw new Error('WrongDocumentError'); } };
  guardPointerLock(throwing);
  assert.equal(throwing.requestPointerLock({ unadjustedMovement: true }), undefined);
  assert.deepEqual(calls, [[{ unadjustedMovement: true }]]);

  const rejecting = { requestPointerLock() { return Promise.reject(new Error('NotAllowedError')); } };
  guardPointerLock(rejecting);
  await rejecting.requestPointerLock();   // would be an unhandled rejection without the guard
});

test('a pointer lock request that works keeps its result and its this', () => {
  const el = { requestPointerLock() { return this === el ? 'locked' : 'wrong this'; } };
  guardPointerLock(el);
  assert.equal(el.requestPointerLock(), 'locked');
});

test('an element without pointer lock is left alone', () => {
  const el = {};
  guardPointerLock(el);
  assert.equal('requestPointerLock' in el, false);
});
