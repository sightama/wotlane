// ABOUTME: Tests for the front panel's accessibility in web/index.html: every rule checkbox has its own label, nothing sets a
// ABOUTME: positive tabindex, and the game canvas is focusable by script but not a tab stop. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const html = readFileSync(new URL('./index.html', import.meta.url), 'utf8');

test('each rule checkbox sits in its own label', () => {
  for (const [id, word] of [['mod_jump', 'Jump'], ['mod_fast', 'Fast'], ['mod_turbo', 'Turbo']]) {
    const own = new RegExp(`<label[^>]*>\\s*<input type="checkbox" id="${id}">\\s*${word}\\s*</label>`);
    assert.match(html, own, `${id} needs a label that holds only it and "${word}"`);
  }
});

test('the rule checkboxes form a group named Rules', () => {
  assert.match(html, /<fieldset[^>]*>\s*<legend>Rules<\/legend>[\s\S]*?id="mod_turbo"[\s\S]*?<\/fieldset>/);
});

test('no element has a positive tabindex', () => {
  assert.doesNotMatch(html, /tabindex="[1-9]\d*"/i);
});

test('the stage starts inert, so Tab starts in the front panel that covers it', () => {
  assert.match(html, /<div id="stage" inert>/);
});

test('the canvas can be focused by script but is not in the tab order', () => {
  assert.match(html, /<canvas id="canvas" tabindex="-1"/);
});
