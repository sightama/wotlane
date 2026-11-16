// ABOUTME: Tests for web/controls.js: the Controls card lists exactly the keys the native layer maps by default (read from
// ABOUTME: native/wotm_settings.hpp), so a key added or changed there fails here. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';

const { CONTROLS, keyLabel, controlRows } = createRequire(import.meta.url)('./controls.js');
const settings = readFileSync(new URL('../native/wotm_settings.hpp', import.meta.url), 'utf8');

// raylib key code -> the native key name (kKeyNames)
const keyNames = new Map([...settings.matchAll(/\{(\d+), "([A-Z0-9_]+)"\}/g)].map((m) => [Number(m[1]), m[2]]));
// the PS2 bind names, in the order of the Bind enum
const bindNames = [...settings.match(/kBindNames\[kBinds\] = \{([^}]*)\}/)[1].matchAll(/"([A-Z0-9_]+)"/g)].map((m) => m[1]);
// the default keys of each bind: kbPair(a, b) or 0, in the same order
const defaults = (() => {
  const body = settings.match(/kDefaultKb\[kBinds\] = \{([^;]*)\};/)[1].replace(/\/\/[^\n]*/g, '');
  return [...body.matchAll(/kbPair\((\d+)(?:,\s*(\d+))?\)|(?<![\w(])0(?![\w)])/g)].map((m) => (m[1] ? [m[1], m[2]].filter(Boolean).map(Number) : []));
})();

test('the native bind names and default keys line up', () => {
  assert.equal(defaults.length, bindNames.length);
  assert.ok(bindNames.length >= 30);
});

test('every PS2 button the keyboard drives by default has a row with exactly its keys', () => {
  for (const [i, name] of bindNames.entries()) {
    const native = defaults[i].map((code) => keyNames.get(code) ?? (code === 1000 ? 'MOUSE_LEFT' : `code ${code}`));
    const row = CONTROLS.find((c) => c.bind === name);
    if (!native.length) {
      assert.equal(row, undefined, `${name} has no default key but is listed`);
      continue;
    }
    assert.ok(row, `${name} (${native.join(', ')}) is missing from the Controls card`);
    assert.deepEqual([...row.keys].sort(), [...native].sort(), `${name}`);
  }
});

test('each bind is listed once and every row says what it does', () => {
  const binds = CONTROLS.map((c) => c.bind).filter(Boolean);
  assert.equal(new Set(binds).size, binds.length);
  for (const c of CONTROLS) assert.ok(c.does.length > 3 && c.keys.length, JSON.stringify(c));
});

test('keys read as words and rows are [what it does, keys]', () => {
  assert.equal(keyLabel('SPACE'), 'Space');
  assert.equal(keyLabel('W'), 'W');
  assert.equal(keyLabel('MOUSE_MIDDLE'), 'Middle mouse button');
  assert.deepEqual(controlRows().find((r) => r[0] === 'Jump (in menus: choose)'), ['Jump (in menus: choose)', 'Space']);
  assert.deepEqual(controlRows().find((r) => r[0] === 'Grab'), ['Grab', 'E, F']);
});
