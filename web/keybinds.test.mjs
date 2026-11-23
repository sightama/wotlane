// ABOUTME: Tests for web/keybinds.js: the key names and the layout number are checked against native/wotm_settings.hpp, the KB_ lines
// ABOUTME: read and written as the game reads them, conflicts and presets. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import {
  KB_LAYOUT, allKeyNames, keyNameOfCode, mouseNameOfButton, wheelNameOfDelta, labelOfKey, readBinds, writeBinds, assignKey, clearKey,
  PRESETS, presetBinds,
} from './keybinds.js';
import { parseIni, readSettings, updateSetting, editSettings } from './settings.js';
import { Dir } from './fakefs.mjs';

const { CONTROLS, keyLabel } = createRequire(import.meta.url)('./controls.js');
const hpp = readFileSync(new URL('../native/wotm_settings.hpp', import.meta.url), 'utf8');
const nativeKeys = new Set([...hpp.matchAll(/\{(\d+), "([A-Z0-9_]+)"\}/g)].map((m) => m[2]));
const defaults = () => readBinds('', CONTROLS);

test('the layout number is the one the game reads', () => {
  assert.equal(KB_LAYOUT, Number(hpp.match(/kKbLayout = (\d+)/)[1]));
});

test('every key the page names is a native key, and every key a keyboard has can be picked', () => {
  for (const name of allKeyNames()) assert.ok(nativeKeys.has(name), `${name} is not in kKeyNames`);
  const reachable = new Set(allKeyNames());
  for (const name of nativeKeys) {
    if (/^(MOUSE_|WHEEL_)/.test(name) || name === 'ESC') continue;   // the mouse has its own events; Esc cancels the capture
    assert.ok(reachable.has(name), `${name} cannot be picked on the page`);
  }
  for (const name of ['MOUSE_LEFT', 'MOUSE_MIDDLE', 'MOUSE_RIGHT', 'MOUSE_SIDE', 'MOUSE_EXTRA', 'WHEEL_UP', 'WHEEL_DOWN']) assert.ok(nativeKeys.has(name));
});

test('browser events become native names; the runtime keys, Esc and unknown keys are not pickable', () => {
  assert.equal(keyNameOfCode('KeyW'), 'W');
  assert.equal(keyNameOfCode('Digit1'), '1');
  assert.equal(keyNameOfCode('Numpad7'), 'KP7');
  assert.equal(keyNameOfCode('ShiftLeft'), 'LSHIFT');
  assert.equal(keyNameOfCode('ArrowUp'), 'UP');
  for (const code of ['F1', 'F11', 'F12', 'Escape', 'MetaLeft', 'ContextMenu', 'IntlBackslash', undefined]) assert.equal(keyNameOfCode(code), null, String(code));
  assert.deepEqual([0, 1, 2, 3, 4, 5].map(mouseNameOfButton), ['MOUSE_LEFT', 'MOUSE_MIDDLE', 'MOUSE_RIGHT', 'MOUSE_SIDE', 'MOUSE_EXTRA', null]);
  assert.deepEqual([-100, 0, 100].map(wheelNameOfDelta), ['WHEEL_UP', null, 'WHEEL_DOWN']);
});

test('the keys without a file are the Controls card\'s defaults; the file wins, but only in the layout the game reads', () => {
  assert.deepEqual(defaults().get('CIRCLE'), ['E', 'F']);
  assert.equal(defaults().has('L1'), false, 'a button with no default key is not on the card');
  const mine = 'KB_LAYOUT=4\nKB_CIRCLE=K\nKB_BLOCK=\n';
  assert.deepEqual(readBinds(mine, CONTROLS).get('CIRCLE'), ['K']);
  assert.deepEqual(readBinds(mine, CONTROLS).get('BLOCK'), [], 'an empty line is a button with no key');
  assert.deepEqual(readBinds(mine, CONTROLS).get('CROSS'), ['SPACE']);
  assert.deepEqual(readBinds('KB_LAYOUT=2\nKB_CIRCLE=K\n', CONTROLS).get('CIRCLE'), ['E', 'F']);
  assert.deepEqual(readBinds('KB_CIRCLE=K\n', CONTROLS).get('CIRCLE'), ['E', 'F'], 'no layout line: the game ignores the keys');
  assert.deepEqual(readBinds('KB_LAYOUT=4\nKB_CIRCLE=K,L\n', CONTROLS).get('CIRCLE'), ['K', 'L']);
});

test('writing sets the layout line and every KB_ line, and keeps what the page does not own', () => {
  const binds = assignKey(defaults(), 'CIRCLE', 0, 'K').binds;
  const out = writeBinds('# mine\nWOTM_RES=1080\nKB_L1=Z\n', binds);
  const file = parseIni(out);
  assert.equal(file.get('KB_LAYOUT'), String(KB_LAYOUT));
  assert.equal(file.get('KB_CIRCLE'), 'K,F');
  assert.equal(file.get('KB_STICK_UP'), 'W');
  assert.equal(file.get('WOTM_RES'), '1080');
  assert.equal(file.get('KB_L1'), 'Z');
  assert.ok(out.startsWith('# mine\n'));
  assert.deepEqual(readBinds(out, CONTROLS), binds, 'what is written reads back');
  assert.equal(parseIni(writeBinds('', clearKey(defaults(), 'BLOCK', 0))).get('KB_BLOCK'), '');
});

test('a key given to a button that another has swaps: the other gets the key this slot held, as in the game\'s own panel', () => {
  const start = defaults();
  const a = assignKey(start, 'CROSS', 0, 'LSHIFT');
  assert.deepEqual(a.binds.get('CROSS'), ['LSHIFT']);
  assert.deepEqual(a.binds.get('BLOCK'), ['SPACE'], 'Block got what Jump had');
  assert.deepEqual(a.swapped, [{ bind: 'BLOCK', gets: 'SPACE' }]);
  const empty = assignKey(clearKey(start, 'CROSS', 0), 'CROSS', 0, 'LSHIFT');
  assert.deepEqual(empty.binds.get('BLOCK'), [], 'a slot with no key has nothing to give: the other button just loses the key');
  assert.deepEqual(empty.swapped, [{ bind: 'BLOCK', gets: null }]);
  const both = assignKey(start, 'CIRCLE', 1, 'SPACE');   // Grab already has E and F; Space is Jump's
  assert.deepEqual(both.binds.get('CIRCLE'), ['E', 'SPACE']);
  assert.deepEqual(both.binds.get('CROSS'), ['F'], 'Jump got F, the key Grab\'s second slot held');
  const second = assignKey(start, 'CIRCLE', 1, 'X');
  assert.deepEqual(second.binds.get('CIRCLE'), ['E', 'X'], 'the second slot replaces the second key');
  assert.deepEqual(second.swapped, []);
  assert.deepEqual(assignKey(start, 'CIRCLE', 0, 'X').binds.get('CIRCLE'), ['X', 'F']);
  assert.deepEqual(assignKey(start, 'CIRCLE', 0, 'F').binds.get('CIRCLE'), ['F'], 'a key already on the button in the other slot is not doubled');
  assert.deepEqual(assignKey(start, 'BLOCK', 1, 'Y').binds.get('BLOCK'), ['LSHIFT', 'Y'], 'a second slot is added');
  assert.deepEqual(start.get('CROSS'), ['SPACE'], 'the map it started from is not changed');
  const keys = [...a.binds.values()].flat();
  assert.equal(new Set(keys).size, keys.length, 'no key on two buttons after a swap');
});

test('clearing a slot leaves the other key', () => {
  assert.deepEqual(clearKey(defaults(), 'CIRCLE', 0).get('CIRCLE'), ['F']);
});

test('presets use only buttons and keys that exist, never give a key to two buttons, and the default one is the defaults', () => {
  const binds = new Set(CONTROLS.map((c) => c.bind).filter(Boolean));
  for (const p of PRESETS) {
    for (const [bind, keys] of Object.entries(p.keys)) {
      assert.ok(binds.has(bind), `${p.id}: ${bind}`);
      for (const k of keys) assert.ok(nativeKeys.has(k), `${p.id}: ${k}`);
    }
    const seen = new Map();
    for (const [bind, keys] of presetBinds(CONTROLS, p)) {
      for (const k of keys) {
        assert.equal(seen.get(k), undefined, `${p.id}: ${k} on ${seen.get(k)} and ${bind}`);
        seen.set(k, bind);
      }
    }
  }
});

test('a letter reads as the keyboard prints it (AZERTY), the rest in words', () => {
  const azerty = new Map([['KeyW', 'z'], ['KeyA', 'q'], ['Space', ' ']]);
  assert.equal(labelOfKey('W', azerty, keyLabel), 'Z');
  assert.equal(labelOfKey('A', azerty, keyLabel), 'Q');
  assert.equal(labelOfKey('SPACE', azerty, keyLabel), 'Space', 'a blank is not a label');
  assert.equal(labelOfKey('LSHIFT', azerty, keyLabel), 'Left Shift');
  assert.equal(labelOfKey('W', undefined, keyLabel), 'W', 'no Keyboard Map API: the native name');
  assert.equal(labelOfKey('MOUSE_LEFT', azerty, keyLabel), 'Left mouse button');
});

test('keys are saved to the settings file through the same queue as the options, one change after another', async () => {
  const root = new Dir();
  const binds = assignKey(defaults(), 'CIRCLE', 0, 'K').binds;
  await Promise.all([updateSetting(root, 'WOTM_RES', '1080'), editSettings(root, (text) => writeBinds(text, binds)), updateSetting(root, 'WOTM_MSAA', '4')]);
  const text = await readSettings(root);
  assert.deepEqual(readBinds(text, CONTROLS), binds);
  assert.equal(parseIni(text).get('WOTM_RES'), '1080');
  assert.equal(parseIni(text).get('WOTM_MSAA'), '4');
});
test('odd files read as the game reads them: a spaced or too long list, unknown names, layout 3 and a layout number with spaces', () => {
  const read = (ini) => readBinds(ini, CONTROLS);
  assert.deepEqual(read('KB_LAYOUT=4\nKB_CIRCLE=K,L,M\n').get('CIRCLE'), ['K'], 'the game takes "L,M" as one unknown name');
  assert.deepEqual(read('KB_LAYOUT=4\nKB_CIRCLE=K, L\n').get('CIRCLE'), ['K'], 'a space makes the second name unknown');
  assert.deepEqual(read('KB_LAYOUT=4\nKB_CIRCLE=NOPE,L\n').get('CIRCLE'), ['L'], 'an unknown first name leaves the second alone');
  assert.deepEqual(read('KB_LAYOUT=4\nKB_CIRCLE=K,K\n').get('CIRCLE'), ['K'], 'the same key twice is once');
  assert.deepEqual(read('KB_LAYOUT=04\nKB_CIRCLE=K\n').get('CIRCLE'), ['K'], 'the layout number is read as a number');
  const old = read('KB_LAYOUT=3\nKB_CIRCLE=K\nKB_START=ENTER,ESC\nKB_TARGET_NEXT=TAB,WHEEL_DOWN\n');
  assert.deepEqual(old.get('CIRCLE'), ['K'], 'layout 3 keeps what the player changed');
  assert.deepEqual(old.get('START'), ['TAB', 'ENTER'], 'and moves its two default pairs to the current defaults');
  assert.deepEqual(old.get('TARGET_NEXT'), ['T', 'WHEEL_DOWN']);
  assert.deepEqual(read('KB_LAYOUT=2\nKB_CIRCLE=K\n').get('CIRCLE'), ['E', 'F']);
});

test('a key two buttons already share (a hand-edited file) goes to one of them in a swap', () => {
  const shared = new Map([['A', ['X']], ['B', ['X']], ['C', ['Y']]]);
  const { binds } = assignKey(shared, 'C', 0, 'X');
  const keys = [...binds.values()].flat();
  assert.equal(keys.filter((k) => k === 'Y').length, 1);
});
