// ABOUTME: Tests for web/saves.js on the fake private file system: which files are the memory-card saves, the summary the Saves tab
// ABOUTME: shows, backup and restore through a zip (what a restore refuses, rollback), delete, automatic backups and the game lock.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { Dir } from './fakefs.mjs';
import { zip } from './zip.js';
import {
  readSaves, summarize, backupZip, readBackup, replaceSaves, clearSaves, keepAutoBackup, listAutoBackups, readAutoBackup,
  stamp, AUTO_BACKUPS, prettyName,
} from './saves.js';

const enc = (s) => new TextEncoder().encode(s);
const dec = (b) => new TextDecoder().decode(b);

async function put(root, path, data) {
  const parts = path.split('/');
  let d = root;
  for (const p of parts.slice(0, -1)) d = await d.getDirectoryHandle(p, { create: true });
  const w = await (await d.getFileHandle(parts.at(-1), { create: true })).createWritable();
  await w.write(typeof data === 'string' ? enc(data) : data);
  await w.close();
}
async function get(root, path) {
  let d = root;
  const parts = path.split('/');
  for (const p of parts.slice(0, -1)) d = await d.getDirectoryHandle(p);
  return dec(new Uint8Array(await (await (await d.getFileHandle(parts.at(-1))).getFile()).arrayBuffer()));
}
async function exists(root, path) {
  try { await get(root, path); return true; } catch { return false; }
}
const names = (entries) => entries.map((e) => e.name).sort();

const SAVE = JSON.stringify({
  options: { difficulty: 1 }, battleTokens: 120, unlocks: { levels: { ufo: true } },
  saves: [{ level: 'ufo', monster: 'congar', variant: 0, lives: 3, score: 4500, costume: 'default' }, null,
    { level: 'capitol', monster: 'robo-47', variant: 1, lives: 1, score: 90, costume: 'default' }],
});

async function fullRoot() {
  const root = new Dir();
  await put(root, 'save.json', SAVE);
  await put(root, 'mc0/BASCUS-97197/BASCUS-97197', new Uint8Array([1, 2, 3]));
  await put(root, 'mc0/BASCUS-97197/savegame.dat', new Uint8Array([4, 5]));
  await put(root, 'mc1/BASCUS-97197/savegame.dat', new Uint8Array([6]));
  await put(root, 'settings.ini', 'WOTM_RES=720\n');
  await put(root, 'save.json.bad', 'broken');
  await put(root, 'mods/Creator/skins/x/skin.json', '{}');
  return root;
}

test('readSaves lists the save, both cards and nothing else', async () => {
  const { entries, modified } = await readSaves(await fullRoot());
  assert.deepEqual(names(entries), ['mc0/BASCUS-97197/BASCUS-97197', 'mc0/BASCUS-97197/savegame.dat', 'mc1/BASCUS-97197/savegame.dat', 'save.json']);
  assert.equal(dec(entries.find((e) => e.name === 'save.json').data), SAVE);
  assert.ok(modified > 0);
});

test('readSaves of a profile with no saves is empty', async () => {
  const root = new Dir();
  await put(root, 'settings.ini', 'WOTM_RES=720\n');
  assert.deepEqual(await readSaves(root), { entries: [], modified: null });
});

test('summarize reads tokens and adventure slots from save.json', () => {
  const s = summarize([{ name: 'save.json', data: enc(SAVE) }, { name: 'mc0/a/b', data: new Uint8Array(10) }]);
  assert.equal(s.tokens, 120);
  assert.equal(s.files, 2);
  assert.equal(s.bytes, SAVE.length + 10);
  assert.deepEqual(s.slots, [
    { slot: 1, monster: 'congar', level: 'ufo', lives: 3, score: 4500 },
    { slot: 3, monster: 'robo-47', level: 'capitol', lives: 1, score: 90 },
  ]);
  assert.equal(s.jsonError, null);
});

test('summarize says so when save.json does not read', () => {
  const s = summarize([{ name: 'save.json', data: enc('{oops') }]);
  assert.equal(s.slots.length, 0);
  assert.match(s.jsonError, /not readable|does not read/);
  assert.equal(summarize([]).files, 0);
});

test('a backup zip restores to the same entries', async () => {
  const { entries } = await readSaves(await fullRoot());
  const back = await readBackup(await backupZip(entries));
  assert.deepEqual(names(back.entries), names(entries));
  assert.equal(dec(back.entries.find((e) => e.name === 'save.json').data), SAVE);
  assert.deepEqual(back.ignored, []);
});

test('files outside the saves are ignored and reported, not restored', async () => {
  const bytes = await zip([{ name: 'save.json', data: enc(SAVE) }, { name: 'settings.ini', data: enc('x') }, { name: 'mods/a/b.txt', data: enc('y') }]);
  const back = await readBackup(bytes);
  assert.deepEqual(names(back.entries), ['save.json']);
  assert.deepEqual(back.ignored.sort(), ['mods/a/b.txt', 'settings.ini']);
});

test('a zip of the save folder inside one folder still restores', async () => {
  const bytes = await zip([{ name: 'my saves/save.json', data: enc(SAVE) }, { name: 'my saves/mc0/BASCUS-97197/savegame.dat', data: enc('z') }]);
  assert.deepEqual(names((await readBackup(bytes)).entries), ['mc0/BASCUS-97197/savegame.dat', 'save.json']);
});

test('a restore refuses what is not a save', async () => {
  const refuse = (bytes, pattern) => assert.rejects(readBackup(bytes), pattern);
  await refuse(enc('not a zip'), /not a zip/);
  await refuse(await zip([{ name: 'readme.txt', data: enc('hi') }]), /no saves/i);
  await refuse(await zip([{ name: 'save.json', data: enc('{oops') }]), /not readable|does not read/);
  await refuse(await zip([{ name: 'save.json', data: enc('[]') }]), /not a War of the Monsters save/);
  await refuse(await zip([{ name: 'save.json', data: enc('{"hello": 1}') }]), /not a War of the Monsters save/);
  await refuse(await zip([{ name: 'save.json', data: enc('{"battleTokens": "many"}') }]), /not a War of the Monsters save/);
  await refuse(await zip([{ name: 'save.json', data: enc('{"saves": {}}') }]), /not a War of the Monsters save/);
});

test('a restore refuses paths that could leave the save folder', async () => {
  for (const name of ['mc0/../evil', 'mc0//x', 'mc0/./x', 'mc0\\x', 'mc0/a b\u0000', '/mc0/x', 'mc0/x/']) {
    const bytes = await zip([{ name: 'save.json', data: enc(SAVE) }, { name, data: enc('x') }]);
    const back = await readBackup(bytes).catch((e) => e);
    // a name that is not a clean path inside mc0/mc1 is either refused or ignored; it never becomes an entry
    const bad = back instanceof Error ? true : !names(back.entries).includes(name.replace(/\/$/, ''));
    assert.ok(bad, name);
    if (!(back instanceof Error)) assert.deepEqual(names(back.entries), ['save.json']);
  }
});

test('a restore refuses too many files and too large a save.json', async () => {
  const many = Array.from({ length: 60 }, (_, i) => ({ name: `mc0/BASCUS-97197/f${i}`, data: enc('x') }));
  await assert.rejects(readBackup(await zip([{ name: 'save.json', data: enc(SAVE) }, ...many])), /too many/i);
  const big = JSON.stringify({ options: {}, pad: 'x'.repeat(2 << 20) });
  await assert.rejects(readBackup(await zip([{ name: 'save.json', data: enc(big) }])), /too large/i);
});

test('replaceSaves swaps the saves and leaves settings and mods alone', async () => {
  const root = await fullRoot();
  await replaceSaves(root, [{ name: 'save.json', data: enc('{"battleTokens": 5}') }, { name: 'mc0/BASCUS-97197/savegame.dat', data: enc('new') }]);
  assert.equal(await get(root, 'save.json'), '{"battleTokens": 5}');
  assert.equal(await get(root, 'mc0/BASCUS-97197/savegame.dat'), 'new');
  assert.equal(await exists(root, 'mc0/BASCUS-97197/BASCUS-97197'), false);
  assert.equal(await exists(root, 'mc1/BASCUS-97197/savegame.dat'), false);
  assert.equal(await get(root, 'settings.ini'), 'WOTM_RES=720\n');
  assert.equal(await get(root, 'mods/Creator/skins/x/skin.json'), '{}');
  assert.equal(await get(root, 'save.json.bad'), 'broken');
});

test('a restore that fails half way puts the old saves back', async () => {
  const root = await fullRoot();
  const before = names((await readSaves(root)).entries);
  const real = root.getDirectoryHandle.bind(root);
  let failed = false;
  root.getDirectoryHandle = async (name, opts) => {
    if (name === 'mc0' && opts?.create && !failed) { failed = true; throw new DOMException('quota', 'QuotaExceededError'); }
    return real(name, opts);
  };
  await assert.rejects(replaceSaves(root, [{ name: 'save.json', data: enc('{"battleTokens": 5}') }, { name: 'mc0/BASCUS-97197/savegame.dat', data: enc('new') }]), /quota|Quota/);
  const after = await readSaves(root);
  assert.deepEqual(names(after.entries), before);
  assert.equal(dec(after.entries.find((e) => e.name === 'save.json').data), SAVE);
});

test('clearSaves removes the save and the cards only', async () => {
  const root = await fullRoot();
  await clearSaves(root);
  assert.deepEqual((await readSaves(root)).entries, []);
  assert.equal(await get(root, 'settings.ini'), 'WOTM_RES=720\n');
  await clearSaves(root);   // nothing left: still fine
});

test('automatic backups keep the newest few, newest first', async () => {
  const root = new Dir();
  for (let i = 1; i <= AUTO_BACKUPS + 2; i++) await keepAutoBackup(root, enc(`zip${i}`), `2026010${i}-000000`);
  const list = await listAutoBackups(root);
  assert.equal(list.length, AUTO_BACKUPS);
  assert.equal(list[0].name, `saves-2026010${AUTO_BACKUPS + 2}-000000.zip`);
  assert.equal(dec(await readAutoBackup(root, list[0].name)), `zip${AUTO_BACKUPS + 2}`);
  assert.equal(list.at(-1).name, 'saves-20260103-000000.zip');
  assert.deepEqual(await listAutoBackups(new Dir()), []);
});

test('readAutoBackup refuses a name that is not one of the backups', async () => {
  await assert.rejects(readAutoBackup(new Dir(), '../save.json'), /not an automatic backup/);
});

test('stamp is the date and time as digits', () => {
  assert.equal(stamp(new Date(2026, 10, 16, 21, 4, 9)), '20261116-210409');
});

test('prettyName shows the names the save keeps as words', () => {
  assert.equal(prettyName('midtown_park'), 'Midtown Park');
  assert.equal(prettyName('robo-47'), 'Robo-47');
  assert.equal(prettyName('ultra-v'), 'Ultra-V');
  assert.equal(prettyName('undefined'), 'Undefined');
});
