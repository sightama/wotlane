// ABOUTME: Tests for web/settings.js: the option list is checked against native/wotm_settings.hpp (keys, defaults, accepted values,
// ABOUTME: what an online match fixes), the settings.ini editing keeps lines it doesn't own, and the file round trip on a fake OPFS.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { Dir } from './fakefs.mjs';
import {
  OPTIONS, TABS, parseIni, setIni, setOption, choiceFor, readSettings, updateSetting, settingsSaved, optionsOf,
} from './settings.js';

const hpp = readFileSync(new URL('../native/wotm_settings.hpp', import.meta.url), 'utf8');

// the native tables: `inline constexpr <type> name[...] = {...};` -> the items as written
function nativeList(name) {
  const m = hpp.match(new RegExp(`${name}\\[[^\\]]*\\]\\s*=\\s*\\{([^}]*)\\}`));
  assert.ok(m, `${name} not found in wotm_settings.hpp`);
  return m[1].split(',').map((s) => s.trim().replace(/^"|"$/g, '')).filter(Boolean);
}
const kEnv = nativeList('kEnv');
const kDefault = nativeList('kDefault');

test('every option is a native key, with the native default', () => {
  for (const o of OPTIONS) {
    const i = kEnv.indexOf(o.key);
    assert.ok(i >= 0, `${o.key} is not in kEnv`);
    // the native default is a number; the file's text for it is the option's own spelling of it
    const native = kDefault[i];
    const asText = { ASPECT: { 0: 'dynamic' }, TEXFILTER: { 0: 'bilinear' }, PROMPTS: { 0: 'auto' }, MENUCONFIRM: { 0: 'bottom' } }[o.key.slice(5)]?.[native] ?? native;
    assert.equal(o.def, asText, `${o.key}: default ${o.def}, native ${native}`);
  }
});

test('every option offers its default and no value twice', () => {
  for (const o of OPTIONS) {
    const values = o.choices.map(([v]) => v);
    assert.ok(values.includes(o.def), `${o.key} does not offer its default`);
    assert.equal(new Set(values).size, values.length, `${o.key} offers a value twice`);
  }
});

test('the values offered are ones the native parser accepts', () => {
  const choices = (key) => OPTIONS.find((o) => o.key === key).choices.map(([v]) => v);
  const numeric = (key) => choices(key).filter((v) => /^\d+$/.test(v)).map(Number);
  const subset = (items, list) => items.every((v) => list.includes(v));
  assert.ok(subset(numeric('WOTM_RES'), nativeList('kResolutions').map(Number)));
  assert.ok(choices('WOTM_RES').includes('window'));
  assert.ok(subset(numeric('WOTM_RENDERSCALE'), nativeList('kRenderScales').map(Number)));
  assert.ok(subset(numeric('WOTM_MSAA'), nativeList('kMsaaLevels').map(Number)));
  assert.ok(subset(choices('WOTM_ASPECT'), nativeList('kAspectNames')));
  assert.ok(subset(choices('WOTM_PROMPTS'), nativeList('kPromptNames')));
  assert.ok(subset(choices('WOTM_MENUCONFIRM'), nativeList('kConfirmNames')));
  const filters = nativeList('kTexFilters').map(Number);
  assert.ok(subset(choices('WOTM_TEXFILTER').map((v) => (v === 'bilinear' ? 0 : v === 'trilinear' ? 1 : parseInt(v))), filters));
  assert.ok(choices('WOTM_CAMSENS').every((v) => v >= 1 && v <= 10));
  for (const k of ['WOTM_VOL_SFX', 'WOTM_VOL_MUSIC']) assert.ok(choices(k).every((v) => v >= 0 && v <= 100));
});

test('what an online match fixes matches the native profile', () => {
  const keyNames = nativeList('kNetKeys');   // LockToggle, CloseCam (enum names)
  const values = nativeList('kNetValues');
  const enumToEnv = { LockToggle: 'WOTM_LOCKTOGGLE', CloseCam: 'WOTM_CLOSECAM' };
  const fixed = OPTIONS.filter((o) => o.online?.fixed !== undefined);
  assert.deepEqual(fixed.map((o) => o.key).sort(), keyNames.map((k) => enumToEnv[k]).sort());
  keyNames.forEach((k, i) => assert.equal(OPTIONS.find((o) => o.key === enumToEnv[k]).online.fixed, values[i]));
  const lobby = OPTIONS.filter((o) => o.online?.lobby).map((o) => o.key).sort();
  assert.deepEqual(lobby, ['WOTM_FASTMONSTERS', 'WOTM_SUPERJUMP', 'WOTM_TURBO']);
});

test('tabs list their options in order and every option has a tab', () => {
  const ids = TABS.map((t) => t.id);
  for (const o of OPTIONS) assert.ok(ids.includes(o.tab), `${o.key}: tab ${o.tab}`);
  assert.deepEqual(optionsOf('display').map((o) => o.key.slice(5)), ['RES', 'RENDERSCALE', 'ASPECT', 'MSAA', 'TEXFILTER', 'INTERP']);
});

test('parseIni reads KEY=value lines, last one wins, comments and CRLF ignored', () => {
  const ini = parseIni('# comment\r\nWOTM_RES=1080\r\n\r\nnot a line\r\nWOTM_RES=1440\r\nKB_UP=UP,W\r\n');
  assert.equal(ini.get('WOTM_RES'), '1440');
  assert.equal(ini.get('KB_UP'), 'UP,W');
  assert.equal(ini.size, 2);
});

test('parseIni skips the spaces before a value, as the game does', () => {
  assert.equal(parseIni('WOTM_RES= 1080\n').get('WOTM_RES'), '1080');
  assert.deepEqual(choiceFor(OPTIONS.find((o) => o.key === 'WOTM_RES'), 'WOTM_RES=  1080\n'), { value: '1080', custom: false });
});

test('settingsSaved waits for the changes made so far', async () => {
  const root = new Dir();
  updateSetting(root, 'WOTM_MSAA', '4');
  updateSetting(root, 'WOTM_RES', '1080');
  await settingsSaved();
  const ini = parseIni(await readSettings(root));
  assert.equal(ini.get('WOTM_MSAA'), '4');
  assert.equal(ini.get('WOTM_RES'), '1080');
});

test('setIni changes the line in place and keeps every other line', () => {
  const before = '# header\nWOTM_RES=720\nKB_LAYOUT=3\nKB_UP=UP\nWOTM_MSAA=0\n';
  assert.equal(setIni(before, 'WOTM_RES', '1080'), '# header\nWOTM_RES=1080\nKB_LAYOUT=3\nKB_UP=UP\nWOTM_MSAA=0\n');
});

test('setIni appends a missing key, with a newline before it when the file lacks one', () => {
  assert.equal(setIni('WOTM_RES=720', 'WOTM_MSAA', '4'), 'WOTM_RES=720\nWOTM_MSAA=4\n');
  assert.equal(setIni('WOTM_RES=720\n', 'WOTM_MSAA', '4'), 'WOTM_RES=720\nWOTM_MSAA=4\n');
});

test('setIni starts an empty file with the header the game writes', () => {
  assert.equal(setIni('', 'WOTM_MSAA', '4'),
    '# Recompiled options (main menu); same keys as the environment variables\nWOTM_MSAA=4\n');
});

test('setIni changes every line of a repeated key, and keeps CRLF endings', () => {
  assert.equal(setIni('WOTM_RES=1\r\nWOTM_RES=2\r\n', 'WOTM_RES', '720'), 'WOTM_RES=720\r\nWOTM_RES=720\r\n');
});

test('setOption refuses a key or a value the page does not offer', () => {
  assert.throws(() => setOption('', 'WOTM_NOPE', '1'), /unknown setting/);
  assert.throws(() => setOption('', 'WOTM_MSAA', '3'), /not a choice/);
  assert.equal(setOption('', 'WOTM_MSAA', '4').endsWith('WOTM_MSAA=4\n'), true);
});

test('choiceFor gives the default when the file has no line, and flags a value the page does not offer', () => {
  const msaa = OPTIONS.find((o) => o.key === 'WOTM_MSAA');
  assert.deepEqual(choiceFor(msaa, ''), { value: '0', custom: false });
  assert.deepEqual(choiceFor(msaa, 'WOTM_MSAA=4\n'), { value: '4', custom: false });
  assert.deepEqual(choiceFor(msaa, 'WOTM_MSAA=3\n'), { value: '3', custom: true });
});

test('readSettings gives an empty text when there is no file, and updateSetting writes it', async () => {
  const root = new Dir();
  assert.equal(await readSettings(root), '');
  await updateSetting(root, 'WOTM_MSAA', '4');
  assert.equal(await readSettings(root), '# Recompiled options (main menu); same keys as the environment variables\nWOTM_MSAA=4\n');
});

test('updateSetting keeps the other lines of a file the game wrote', async () => {
  const root = new Dir();
  const w = await (await root.getFileHandle('settings.ini', { create: true })).createWritable();
  await w.write('WOTM_RES=720\nKB_LAYOUT=3\nKB_UP=UP\n');
  await w.close();
  await updateSetting(root, 'WOTM_RES', '1080');
  assert.equal(await readSettings(root), 'WOTM_RES=1080\nKB_LAYOUT=3\nKB_UP=UP\n');
});

test('changes made at once all reach the file', async () => {
  const root = new Dir();
  await Promise.all([updateSetting(root, 'WOTM_MSAA', '4'), updateSetting(root, 'WOTM_RES', '1080'), updateSetting(root, 'WOTM_INTERP', '0')]);
  const ini = parseIni(await readSettings(root));
  assert.equal(ini.get('WOTM_MSAA'), '4');
  assert.equal(ini.get('WOTM_RES'), '1080');
  assert.equal(ini.get('WOTM_INTERP'), '0');
});

test('a file that cannot be read is an error, not an empty file', async () => {
  const root = { getFileHandle: async () => { throw new DOMException('x', 'UnknownError'); } };
  await assert.rejects(readSettings(root), /UnknownError|x/);
});
