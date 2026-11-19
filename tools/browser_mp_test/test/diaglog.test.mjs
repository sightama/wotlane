// ABOUTME: Tests for lib/diaglog.mjs: picking the DIAG lines out of what a page sends on its lobby socket, and the line --diag-out
// ABOUTME: saves for each. Run: node --test tools/browser_mp_test/test/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { diagLineOf, diagRecord } from '../lib/diaglog.mjs';

test('a DIAG frame gives its line', () => {
  assert.equal(diagLineOf('DIAG build=wl-1 lobby=ABCD fps=59.8'), 'DIAG build=wl-1 lobby=ABCD fps=59.8');
});

test('a binary frame holding a DIAG line works too, and the trailing newline goes', () => {
  assert.equal(diagLineOf(Buffer.from('DIAG fps=60\n')), 'DIAG fps=60');
});

test('the other lobby traffic is not a DIAG line', () => {
  for (const frame of ['HELLO 5 wl-1', 'CHAT DIAG is a word', 'JOIN ABCD P1-ab12', 'DIAGNOSTIC x=1', '']) assert.equal(diagLineOf(frame), null);
});

test('a saved line starts with the time and the player name', () => {
  assert.equal(diagRecord(new Date('2026-11-19T20:00:05.250Z'), 'P2-ab12', 'DIAG fps=60'), '2026-11-19T20:00:05.250Z P2-ab12 DIAG fps=60\n');
});
