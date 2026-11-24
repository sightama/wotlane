// ABOUTME: Tests for how the front panel loads and behaves under a press, read from web/index.html and web/touch.js: nothing moves after
// ABOUTME: first paint, a press never lands on a dead control, the module graph is preloaded, fields keep 3:1 borders. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (name) => readFileSync(new URL(`./${name}`, import.meta.url), 'utf8');
const html = read('index.html');
const css = html.slice(html.indexOf('<style>'), html.indexOf('</style>'));
const body = html.slice(html.indexOf('<body>'));

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
const rule = (selector) => {
  const m = css.match(new RegExp(`(?:^|\\n)\\s*${selector.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\s*\\{([^}]*)\\}`));
  assert.ok(m, `no CSS rule for ${selector}`);
  return m[1];
};
const indexOf = (id) => {
  const i = body.indexOf(`id="${id}"`);
  assert.ok(i >= 0, `no element #${id}`);
  return i;
};

// ---- loading

test('every module front.js loads, directly or not, is preloaded', () => {
  const seen = new Set();
  const walk = (file) => {
    if (seen.has(file)) return;
    seen.add(file);
    for (const m of read(file).matchAll(/\bfrom\s*['"]\.\/([\w.-]+\.js)['"]|\bimport\s*['"]\.\/([\w.-]+\.js)['"]/g)) walk(m[1] ?? m[2]);   // imports may span lines
  };
  walk('front.js');
  const preloaded = new Set([...html.matchAll(/<link rel="modulepreload" href="([\w.-]+\.js)">/g)].map((m) => m[1]));
  assert.ok(seen.size > 5, 'the import walk found the graph');
  assert.deepEqual([...seen].filter((f) => !preloaded.has(f)).sort(), [], 'modules the page loads but does not preload');
  assert.deepEqual([...preloaded].filter((f) => !seen.has(f)).sort(), [], 'preloaded modules nothing imports');
});

test('Single player, Host and Join start disabled and front.js enables them once its handlers are attached', () => {
  for (const id of ['single', 'host', 'join']) assert.match(html, new RegExp(`<button id="${id}"[^>]*\\bdisabled\\b`), `#${id} must not be pressable before front.js runs`);
  // after the last handler, and not while a rejoin has them off for itself
  assert.match(read('front.js'), /\$\('leave'\)\.addEventListener\('click'[\s\S]*?\n  \}\);\n[^\n]*\n  if \(!entering\) for \(const id of \['single', 'host', 'join'\]\) \$\(id\)\.disabled = false;\n/);
});

// ---- nothing moves

test('the card is anchored to the top, so content that appears does not recentre what is above it', () => {
  assert.doesNotMatch(rule('#front > .card'), /margin:\s*auto/);
});

test('the device note and the Firefox tip sit below the actions, so they push down only the footer', () => {
  const actions = indexOf('openSettings');
  assert.ok(indexOf('deviceNote') > actions && indexOf('firefoxTip') > actions, 'notes come before the actions');
  assert.ok(indexOf('deviceNote') < indexOf('legal') && indexOf('firefoxTip') < indexOf('legal'));
});

test('the open lobbies area never leaves the page once shown: a join under way greys it, a failed list says so inside it', () => {
  const front = read('front.js');
  assert.match(front, /function stopWatchingLobbies\(\) \{\n  cancelWatching\(\);\n  \$\('lobbyList'\)\.inert = true;/);
  assert.match(front, /\$\('lobbies'\)\.replaceChildren\(Object\.assign\(document\.createElement\('li'\), \{ textContent: 'The list of open lobbies is not available right now\.'/);
  assert.match(front, /not available right now\.' \}\)\);\n    \$\('lobbyList'\)\.inert = false;/);
  assert.doesNotMatch(front.slice(front.indexOf('async function refreshLobbies'), front.indexOf('function startWatchingLobbies')), /lobbyList'\)\.hidden = true/);
});

test('a failed start of the page says so', () => {
  assert.match(read('front.js'), /catch \(e\) \{\n  say\('The page did not start properly\. Reload it to try again\.'\);\n  \$\('frontMsg'\)\.className = 'problem';\n  throw e;/);
});

test('the open lobbies area is there from the start, with a line saying it is looking', () => {
  assert.doesNotMatch(body, /<div id="lobbyList" hidden/);
  assert.match(body, /<ul id="lobbies"><li>Looking for open lobbies/);
});

// ---- a press

test('the lobbies list keeps room for the two-line empty message, so the answer from the server moves nothing', () => {
  assert.match(rule('#lobbies'), /min-height:\s*2\.8em/);
});

test('hover is drawn only where a pointer can hover, so a tap leaves nothing stuck', () => {
  assert.match(css, /@media \(hover: hover\)\s*\{[^@]*button:hover:not\(:disabled\)/);
  assert.doesNotMatch(css.replace(/@media \(hover: hover\)\s*\{[^@]*?\n  \}/, ''), /button(\.main)?:hover/);
});

test('the status line starts as Loading and keeps room for two lines', () => {
  assert.match(html, /<p id="frontMsg" role="status">Loading…<\/p>/);
  assert.match(rule('#frontMsg'), /min-height:\s*2\.8em/);
  assert.match(read('front.js'), /function init\(\) \{\n  say\(''\);/);
});

test('buttons answer a press at once: no double-tap delay, a pressed state, a focus ring', () => {
  assert.match(rule('button'), /touch-action:\s*manipulation/);
  assert.match(css, /button:active:not\(:disabled\)\s*\{[^}]*transform:\s*scale\(/);
  assert.match(css, /:focus-visible\s*\{[^}]*outline:/);
});

test('status messages on the panel are announced', () => {
  assert.match(html, /<p id="frontMsg" role="status"/);
});

// ---- fields and touch devices

test('field borders read at 3:1 against the panel and against the field fill', () => {
  const field = rule('input, select, textarea');
  const border = field.match(/border:\s*1px solid var\(--([\w-]+)\)/)[1];
  const fill = rgb(field.match(/background:\s*(#[0-9a-f]{6})/i)[1]);
  for (const [name, under] of [['panel', rgb(vars.panel)], ['field fill', fill]])
    assert.ok(ratio(rgb(vars[border]), under) >= 3, `--${border} on ${name} is ${ratio(rgb(vars[border]), under).toFixed(2)}:1`);
});

test('every touch capable device gets the 44px target size, not only those whose main pointer is coarse', () => {
  assert.match(css, /@media \(pointer: coarse\), \(any-pointer: coarse\)\s*\{\s*:root\s*\{\s*--target:\s*44px/);
});

test('the touch layout panel scrolls inside the screen instead of running past its bottom on a short phone', () => {
  const panel = readFileSync(new URL('./touch.js', import.meta.url), 'utf8').match(/#tpPanel \{([^}]*)\}/)[1];
  assert.match(panel, /max-height:\s*calc\(100% - /);
  assert.match(panel, /overflow-y:\s*auto/);
});

// ---- follow-ups from a fresh review of the first round

test('a page whose scripts fail to load says so instead of staying on Loading', () => {
  // a capturing listener sees a script that cannot be fetched, a syntax error and an error thrown while the modules run
  const head = html.slice(0, html.indexOf('</head>'));
  assert.match(head, /<script>[^<]*window\.addEventListener\('error', [^<]*, true\)[^<]*<\/script>/);
  assert.match(head, /textContent === 'Loading…'/);
  assert.match(head, /The page could not load\. Reload it; if it keeps happening, try Chrome, Firefox or Edge\./);
  assert.match(head, /t !== window && t\.tagName !== 'SCRIPT'/);   // a failed icon or preload link is not a failed page
  assert.match(read('front.js'), /const say = \(text\) => \{ \$\('frontMsg'\)\.textContent = text; \$\('frontMsg'\)\.className = ''; \};/);
  assert.doesNotMatch(html, /onerror=/);
});

test('Enter in the join code cannot start a second connection while one is under way or the buttons are off', () => {
  const front = read('front.js');
  assert.match(front, /\$\('joinCode'\)\.addEventListener\('keydown', \(e\) => \{ if \(isSubmitEnter\(e\) && !\$\('join'\)\.disabled\) hostOrJoin\(true\); \}\);/);
});

test('watching the lobbies again makes the list usable at once, even in a hidden tab', () => {
  assert.match(read('front.js'), /function startWatchingLobbies\(\) \{\n  cancelWatching\(\);\n  \$\('lobbyList'\)\.inert = false;/);
});

test('Refresh is off until the handlers are attached, like the main buttons', () => {
  assert.match(html, /<button id="refreshLobbies" disabled>/);
  assert.match(read('front.js'), /\n  \$\('refreshLobbies'\)\.disabled = false;\n\}/);
});
