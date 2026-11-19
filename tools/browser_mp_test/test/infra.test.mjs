// ABOUTME: Tests for lib/infra.mjs's pure helpers (what the runs' command lines turn into before a browser starts).
// ABOUTME: Run: node --test tools/browser_mp_test/test/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { firefoxPref } from '../lib/infra.mjs';

test('firefoxPref reads a boolean, a number or a string value', () => {
  assert.deepEqual(firefoxPref('javascript.options.wasm_baselinejit=false'), ['javascript.options.wasm_baselinejit', false]);
  assert.deepEqual(firefoxPref('a.b=true'), ['a.b', true]);
  assert.deepEqual(firefoxPref('a.b=12'), ['a.b', 12]);
  assert.deepEqual(firefoxPref('a.b=x=y'), ['a.b', 'x=y']);
});

test('firefoxPref rejects a spec without a name or a value', () => {
  assert.throws(() => firefoxPref('a.b'), /NAME=VALUE/);
  assert.throws(() => firefoxPref('=1'), /NAME=VALUE/);
});
