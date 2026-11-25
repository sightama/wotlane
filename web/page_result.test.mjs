// ABOUTME: Tests for how the result card is wired into web/index.html and web/boot.js: the Save button's place, size and visibility, the
// ABOUTME: scripts' order, and that the card code only reads the game (no writes, no address, no network). Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (name) => readFileSync(new URL(`./${name}`, import.meta.url), 'utf8');
const html = read('index.html');
const css = html.slice(html.indexOf('<style>'), html.indexOf('</style>'));
const body = html.slice(html.indexOf('<body>'));
const rule = (selector) => {
  const m = css.match(new RegExp(`(?:^|\\n)\\s*${selector.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\s*\\{([^}]*)\\}`));
  assert.ok(m, `no CSS rule for ${selector}`);
  return m[1];
};

test('the Save button is in the stage, hidden until there is a result, and a touch target', () => {
  const stage = body.slice(body.indexOf('<div id="stage"'), body.indexOf('<main id="front">'));
  assert.match(stage, /<button id="resultSave" type="button" hidden>Save result card<\/button>/);
  assert.match(css, /#resultSave[^{]*\{[^}]*min-height:\s*var\(--target\)/);
  assert.match(rule('#resultSave'), /position:\s*absolute/);
  assert.match(rule('#resultSave'), /z-index:\s*6/);   // over the touch controls (5), under the stall prompt (7)
});

test('it sits at the bottom middle, clear of the touch pad\'s top buttons and the corner bar', () => {
  const r = rule('#resultSave');
  assert.match(r, /bottom:/);
  assert.match(r, /left:\s*50%/);
  assert.doesNotMatch(r, /\btop:/);
});

test('the scripts load in the order the code needs, before boot.js', () => {
  const at = (f) => body.indexOf(`<script src="${f}">`);
  const [m, u, b] = ['match_result.js', 'result_card_ui.js', 'boot.js'].map(at);
  assert.ok(m >= 0 && m < u && u < b);
});

test('boot.js reads the score through wotm_web_result and starts the button once the game runs', () => {
  const boot = read('boot.js');
  assert.match(boot, /_wotm_web_result/);
  assert.match(boot, /window\.wotmResultUi[\s\S]*ui\.watchResult\(/);
  assert.match(boot, /resultSave/);
});

test('the card code reads the game and draws: nothing is written to the game or sent anywhere', () => {
  for (const file of ['match_result.js', 'result_card_ui.js']) {
    const src = read(file);
    assert.doesNotMatch(src, /\bfetch\(|XMLHttpRequest|WebSocket|sendBeacon|location\b|userAgent|localStorage|sessionStorage|document\.cookie/, `${file} reaches outside the page`);
    assert.doesNotMatch(src, /Module\.|_wotm_web_(?!result)/, `${file} touches the game other than through the cells`);
  }
});

test('reading the score is the only thing the page asks of the game for the card', () => {
  const m = read('boot.js').match(/_wotm_web_result[^\n]*/g);
  assert.ok(m && m.every((line) => /_wotm_web_result\?\.\(/.test(line)), 'called, never assigned');
});
