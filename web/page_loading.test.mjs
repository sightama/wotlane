// ABOUTME: Tests for the loading screen and the front page's look in web/index.html: the screen's parts, that its art is the page's own
// ABOUTME: inline drawing, that motion is opt-in, contrast and touch size of its text and button. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (name) => readFileSync(new URL(`./${name}`, import.meta.url), 'utf8');
const html = read('index.html');
const css = html.slice(html.indexOf('<style>'), html.indexOf('</style>'));
const body = html.slice(html.indexOf('<body>'));
const loading = body.slice(body.indexOf('<div id="loading"'), body.indexOf('<div id="streaming"'));

const vars = Object.fromEntries([...css.matchAll(/--([\w-]+):\s*(#[0-9a-f]{6})/gi)].map((m) => [m[1], m[2]]));
const rgb = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
const lum = (c) => {
  const [r, g, b] = c.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; });
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
};
const ratio = (a, b) => {
  const [hi, lo] = [lum(rgb(a)), lum(rgb(b))].sort((x, y) => y - x);
  return (hi + 0.05) / (lo + 0.05);
};
const rule = (selector) => {
  const m = css.match(new RegExp(`(?:^|\\n)\\s*${selector.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\s*\\{([^}]*)\\}`));
  assert.ok(m, `no CSS rule for ${selector}`);
  return m[1];
};

test('the loading screen has a headline, a progress bar, the amount and time left, a note and a tip with a button for another', () => {
  for (const id of ['loadHeadline', 'loadAmount', 'loadEta', 'loadNote', 'loadTip', 'loadNextTip']) assert.ok(loading.includes(`id="${id}"`), `#${id}`);
  assert.match(loading, /<progress id="loadBar"[^>]*max="100"[^>]*aria-labelledby="loadHeadline"/);
  assert.match(loading, /<button id="loadNextTip"[^>]*type="button"/);
  assert.match(loading, /id="loadHeadline"[^>]*role="status"/);
});

test('boot.js draws the loading screen from the progress view, and the scripts load in an order that works', () => {
  assert.match(read('boot.js'), /progress\.view\(/);
  const order = ['loading.js', 'controls.js', 'loading_screen.js', 'boot.js'].map((f) => body.indexOf(`<script src="${f}">`));
  assert.ok(order.every((i) => i >= 0), 'all four are loaded');
  assert.ok(order[0] < order[2] && order[1] < order[2] && order[2] < order[3], 'the screen needs the progress and the controls, and boot.js needs the screen');
});

test('the art is drawn inline: no pictures, no files, nothing from the game', () => {
  assert.doesNotMatch(html, /<img\b/);
  assert.doesNotMatch(css, /url\((?!["']?#)/);
  assert.match(body, /<svg[^>]*aria-hidden="true"[^>]*>[\s\S]*?<symbol id="skyline"/);
  for (const m of body.matchAll(/<svg\b[^>]*class="art[^>]*>/g)) assert.match(m[0], /aria-hidden="true"/);
  assert.ok((body.match(/<use href="#skyline"/g) ?? []).length >= 2, 'the loading screen and the front page both draw it');
});

test('motion is opt-in: no animation outside prefers-reduced-motion: no-preference, but the progress bar\'s own', () => {
  const plain = css.replace(/@media \(prefers-reduced-motion: no-preference\) \{(?:[^{}]|\{[^{}]*\})*\}/g, '');
  const loose = [...plain.matchAll(/(?:^|\n)\s*([^{}\n@][^{}]*)\{[^}]*\banimation(?:-name)?:[^;}]*;?/g)].map((m) => m[1].trim());
  assert.deepEqual(loose.filter((s) => !/^progress/.test(s)), []);
});

test('nothing but the progress bar loops: an ambient animation plays once', () => {
  const rules = [...css.matchAll(/(?:^|\n)\s*([^{}\n@][^{}]*)\{([^}]*)\}/g)];
  const looping = rules.filter(([, , decls]) => /animation:[^;]*\binfinite\b/.test(decls)).map(([, sel]) => sel.trim());
  assert.deepEqual(looping.filter((s) => !/progress/.test(s)), []);
});

test('text on the loading screen is AA against what is behind it', () => {
  const card = rule('.loadcard').match(/background:\s*rgba\(11, 11, 13, ([\d.]+)\)/);
  assert.ok(card && Number(card[1]) >= 0.8, 'the card is a near solid dark panel, so text never sits on the art');
  assert.ok(ratio(vars.fg, vars.bg) >= 4.5);
  assert.ok(ratio(vars.dim, vars.bg) >= 4.5);
  assert.ok(ratio(vars.accent, vars.bg) >= 4.5);
});

test('the progress bar fill stands out from its track by 3:1, and the button is a touch target', () => {
  assert.ok(ratio(vars['bar-fill'], vars['bar-track']) >= 3, 'fill against track');
  assert.ok(ratio(vars['bar-track'], vars.bg) >= 1.2 || /border/.test(rule('progress')), 'the track is visible or has an edge');
  assert.match(css, /#loadNextTip[^{]*\{[^}]*min-height:\s*var\(--target\)/);
  assert.match(css, /#loadNextTip[^{]*\{[^}]*min-width:\s*var\(--target\)/);
});

test('the loading screen sits under the movie and the streaming note, and takes presses only on its card', () => {
  assert.match(rule('#loading'), /pointer-events:\s*none/);
  assert.match(rule('.loadcard'), /pointer-events:\s*auto/);
  assert.doesNotMatch(rule('#loading'), /z-index/);   // #movie (z-index 5) and #streaming (6) stack above it
  assert.match(rule('#movie'), /z-index:\s*5/);
});

test('the front card has a banner of the art above the title, hidden from assistive technology', () => {
  const home = body.slice(body.indexOf('<div id="home"'), body.indexOf('<div id="profile"'));
  assert.match(home, /<div class="banner" aria-hidden="true">/);
  assert.ok(home.indexOf('class="banner"') < home.indexOf('<h1>'));
});

test('the bar\'s fill slides to a new value only where motion is allowed', () => {
  const plain = css.replace(/@media \(prefers-reduced-motion: no-preference\) \{(?:[^{}]|\{[^{}]*\})*\}/g, '');
  assert.doesNotMatch(plain, /transition:/);
  assert.match(css, /no-preference\) \{\s*progress::-webkit-progress-value \{ transition: inline-size/);
});
