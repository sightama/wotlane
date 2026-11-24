// ABOUTME: Tests for the front panel's colour contrast (WCAG AA, 4.5:1 for text), the status bar's visibility and the target sizes in
// ABOUTME: web/index.html, computed from the page's own CSS. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const html = readFileSync(new URL('./index.html', import.meta.url), 'utf8');
const css = html.slice(html.indexOf('<style>'), html.indexOf('</style>'));

const vars = Object.fromEntries([...css.matchAll(/--([\w-]+):\s*(#[0-9a-f]{6})/gi)].map((m) => [m[1], m[2]]));
const rgb = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
const lum = (c) => {
  const [r, g, b] = c.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; });
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
};
const ratio = (a, b) => {
  const [hi, lo] = [lum(a), lum(b)].sort((x, y) => y - x);
  return (hi + 0.05) / (lo + 0.05);
};
// the colour of `fg` painted at `alpha` over `under`
const over = (fg, alpha, under) => fg.map((v, i) => v * alpha + under[i] * (1 - alpha));
const rule = (selector) => {
  const m = css.match(new RegExp(`(?:^|\\n)\\s*${selector.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\s*\\{([^}]*)\\}`));
  assert.ok(m, `no CSS rule for ${selector}`);
  return m[1];
};
const AA = 4.5;

test('dim text reads at 4.5:1 on the panel, the page and the input fields', () => {
  const dim = rgb(vars.dim);
  const input = rgb(rule('input, select, textarea').match(/background:\s*(#[0-9a-f]{6})/i)[1]);
  for (const [name, under] of [['panel', rgb(vars.panel)], ['page', rgb(vars.bg)], ['input', input]])
    assert.ok(ratio(dim, under) >= AA, `dim on ${name} is ${ratio(dim, under).toFixed(2)}:1`);
});

test('input placeholders use the dim text colour', () => {
  assert.match(rule('::placeholder'), /color:\s*var\(--dim\)/);
});

test('white on the main button reads at 4.5:1', () => {
  const body = rule('button.main');
  const fill = body.match(/background:\s*var\(--([\w-]+)\)/)[1];
  assert.ok(ratio(rgb('#ffffff'), rgb(vars[fill])) >= AA, `white on ${fill} is ${ratio(rgb('#ffffff'), rgb(vars[fill])).toFixed(2)}:1`);
});

test('the accent used as text reads at 4.5:1 on the panel', () => {
  assert.ok(ratio(rgb(vars.accent), rgb(vars.panel)) >= AA, `accent on panel is ${ratio(rgb(vars.accent), rgb(vars.panel)).toFixed(2)}:1`);
});

test('the status bar is fully opaque and its text reads at 4.5:1 over even a white game frame', () => {
  const bar = rule('#bar');
  assert.doesNotMatch(bar, /opacity/, 'the bar must not be faded');
  const [r, g, b, a] = bar.match(/background:\s*rgba\((\d+),\s*(\d+),\s*(\d+),\s*([\d.]+)\)/).slice(1).map(Number);
  const worst = over([r, g, b], a, [255, 255, 255]);
  const color = bar.match(/color:\s*(#[0-9a-f]{6})/i)[1];
  assert.ok(ratio(rgb(color), worst) >= AA, `bar text on a white frame is ${ratio(rgb(color), worst).toFixed(2)}:1`);
});

test('controls are 24px tall at least, and 44px on a touch screen', () => {
  assert.match(css, /:root\s*\{[^}]*--target:\s*24px/);
  assert.match(css, /@media \(pointer: coarse\)(?:, \(any-pointer: coarse\))?\s*\{\s*:root\s*\{\s*--target:\s*44px/);
  assert.match(css, /min-height:\s*var\(--target\)/);
  assert.match(css, /min-width:\s*var\(--target\)/, 'a one-letter button such as L needs the width too');
});

test('the target size reaches the front panel, the status bar and the stall prompt but not the touch controls panel', () => {
  const lists = [...css.matchAll(/([^{}]*)\{[^}]*min-height:\s*var\(--target\)/g)].map((m) => m[1].split(/,(?![^(]*\))/).map((x) => x.trim()));
  const selectors = lists.flat();
  for (const part of ['#front button:not(.key, .keyclear)', '#front select', '#bar button', '#stall button']) assert.ok(selectors.includes(part), `${part} is not sized`);
  assert.ok(selectors.every((x) => !x.startsWith('#front button') || x.includes(':not(.key, .keyclear)')), 'the key remap buttons stay compact: 24 rows of them would overflow a phone');
  assert.ok(!selectors.includes('button'), 'a bare button rule would resize the touch layout panel (web/touch.js)');
});
