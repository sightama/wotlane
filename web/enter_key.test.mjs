// ABOUTME: Tests for isSubmitEnter: a plain Enter submits, an Enter that confirms an IME conversion does not.
// ABOUTME: Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { isSubmitEnter } from './enter_key.js';

test('a plain Enter submits', () => {
  assert.equal(isSubmitEnter({ key: 'Enter', isComposing: false, keyCode: 13 }), true);
});

test('other keys do not submit', () => {
  assert.equal(isSubmitEnter({ key: 'a', isComposing: false, keyCode: 65 }), false);
});

test('an Enter that confirms an IME conversion does not submit', () => {
  assert.equal(isSubmitEnter({ key: 'Enter', isComposing: true, keyCode: 229 }), false);
});

test('Safari reports the confirming Enter after composition ended: keyCode 229 still blocks it', () => {
  assert.equal(isSubmitEnter({ key: 'Enter', isComposing: false, keyCode: 229 }), false);
});
