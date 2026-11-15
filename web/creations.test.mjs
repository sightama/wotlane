// ABOUTME: Tests for web/creations.js on an in-memory stand-in for the browser's private file system (the directory and file
// ABOUTME: handle calls creations.js uses): listing, export, import with its path checks, and delete. Run: node --test web/*.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { parseCreationPath, listCreations, exportCreation, importZip, deleteCreation, lobbyChoices } from './creations.js';
import { zip, unzip } from './zip.js';

const text = (s) => new TextEncoder().encode(s);
const str = (b) => new TextDecoder().decode(b);

// The subset of FileSystemDirectoryHandle / FileSystemFileHandle that creations.js calls
class Dir {
  constructor() { this.kind = 'directory'; this.items = new Map(); }
  async getDirectoryHandle(name, { create = false } = {}) {
    let h = this.items.get(name);
    if (!h && create) this.items.set(name, h = new Dir());
    if (!h || h.kind !== 'directory') throw new DOMException(name, 'NotFoundError');
    return h;
  }
  async getFileHandle(name, { create = false } = {}) {
    let h = this.items.get(name);
    if (!h && create) this.items.set(name, h = new File());
    if (!h || h.kind !== 'file') throw new DOMException(name, 'NotFoundError');
    return h;
  }
  async removeEntry(name) {
    if (!this.items.delete(name)) throw new DOMException(name, 'NotFoundError');
  }
  async *entries() { yield* [...this.items.entries()]; }
}
class File {
  constructor() { this.kind = 'file'; this.bytes = new Uint8Array(0); }
  async getFile() { const b = this.bytes; return { arrayBuffer: async () => b.slice().buffer }; }
  async createWritable() {
    const parts = [];
    return { write: async (d) => { parts.push(new Uint8Array(d)); }, close: async () => { this.bytes = new Uint8Array(await new Blob(parts).arrayBuffer()); } };
  }
}

async function put(root, path, data) {
  const parts = path.split('/');
  let d = root;
  for (const p of parts.slice(0, -1)) d = await d.getDirectoryHandle(p, { create: true });
  const w = await (await d.getFileHandle(parts.at(-1), { create: true })).createWritable();
  await w.write(typeof data === 'string' ? text(data) : data);
  await w.close();
}
async function get(root, path) {
  const parts = path.split('/');
  let d = root;
  for (const p of parts.slice(0, -1)) d = await d.getDirectoryHandle(p);
  return (await d.getFileHandle(parts.at(-1))).bytes;
}

async function sample() {
  const mods = new Dir();
  await put(mods, 'Creator/skins/green/skin.json', '{"monster": "togera", "id": "green", "name": "GREEN", "base": 1, "ai": false}');
  await put(mods, 'Creator/skins/green/t0004.png', new Uint8Array([137, 80, 78, 71]));
  await put(mods, 'Maps/stages/t1/stage.json', '{"name": "DOWNTOWN MESS", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": []}');
  await put(mods, 'Goliath Prime/MON/x.png', new Uint8Array([1]));   // a mod that is not a creation
  return mods;
}

test('creation paths: mod/skins/<id>/<file> and mod/stages/<id>/<file> only', () => {
  assert.deepEqual(parseCreationPath('Creator/skins/green/t0004.png'), { mod: 'Creator', kind: 'skins', id: 'green', file: 't0004.png' });
  assert.deepEqual(parseCreationPath('My Maps/stages/t1/stage.json'), { mod: 'My Maps', kind: 'stages', id: 't1', file: 'stage.json' });
  for (const bad of ['../skins/green/skin.json', 'Creator/skins/../x/skin.json', '/Creator/skins/green/skin.json',
    'Creator\\skins\\green\\skin.json', 'Creator/skins/green/sub/skin.json', 'Creator/skins/Green/skin.json',
    'Creator/stages/9lives/stage.json', 'Creator/MON/LIZARD1.TEX', '_work/skins/green/skin.json', '.hidden/skins/a/skin.json',
    'Creator/skins/green/.skin.json', 'Creator/skins/green/', 'Creator/skins/green'])
    assert.equal(parseCreationPath(bad), null, bad);
});

test('the list has every skin and map with its display name, and nothing else', async () => {
  assert.deepEqual(await listCreations(await sample()), [
    { mod: 'Creator', kind: 'skins', id: 'green', name: 'GREEN', monster: 'togera' },
    { mod: 'Maps', kind: 'stages', id: 't1', name: 'DOWNTOWN MESS' },
  ]);
  assert.deepEqual(await listCreations(new Dir()), []);
});

test('an exported creation imports into another browser as the same files', async () => {
  const { fileName, bytes } = await exportCreation(await sample(), { mod: 'Creator', kind: 'skins', id: 'green' });
  assert.equal(fileName, 'green.zip');
  assert.deepEqual((await unzip(bytes)).map((e) => e.name), ['Creator/skins/green/skin.json', 'Creator/skins/green/t0004.png']);
  const other = new Dir();
  assert.deepEqual(await importZip(other, bytes), [{ mod: 'Creator', kind: 'skins', id: 'green', name: 'GREEN', monster: 'togera' }]);
  assert.deepEqual(await get(other, 'Creator/skins/green/t0004.png'), new Uint8Array([137, 80, 78, 71]));
});

test('importing a creation that exists replaces its folder (no stale files)', async () => {
  const mods = await sample();
  await put(mods, 'Creator/skins/green/t0009.png', new Uint8Array([9]));
  const z = await zip([{ name: 'Creator/skins/green/skin.json', data: text('{"monster": "togera", "id": "green", "name": "NEW"}') }]);
  await importZip(mods, z);
  const files = [...(await (await (await mods.getDirectoryHandle('Creator')).getDirectoryHandle('skins')).getDirectoryHandle('green')).items.keys()];
  assert.deepEqual(files, ['skin.json']);
  assert.match(str(await get(mods, 'Creator/skins/green/skin.json')), /NEW/);
});

test('a zip with any entry outside the creation layout, or a creation without its json, writes nothing', async () => {
  const bad = await zip([
    { name: 'Creator/skins/blue/skin.json', data: text('{"monster": "togera"}') },
    { name: 'Creator/../../evil.txt', data: text('x') },
  ]);
  const mods = new Dir();
  await assert.rejects(importZip(mods, bad), /Creator\/\.\.\/\.\.\/evil\.txt/);
  assert.equal(mods.items.size, 0);
  const noJson = await zip([{ name: 'Creator/skins/blue/t0004.png', data: new Uint8Array([1]) }]);
  await assert.rejects(importZip(mods, noJson), /skin\.json/);
  assert.equal(mods.items.size, 0);
  await assert.rejects(importZip(mods, text('not a zip')), /not a zip/);
});

test('a zip aimed at a built-in mod folder (any letter case) is refused: the next built-in update would wipe it', async () => {
  const mods = new Dir();
  const z = await zip([{ name: 'Goliath Prime/skins/x/skin.json', data: text('{"monster": "togera"}') }]);
  await assert.rejects(importZip(mods, z, { builtin: ['Goliath Prime', 'Cerebulon'] }), /Goliath Prime.*built-in/);
  const lower = await zip([{ name: 'cerebulon/skins/x/skin.json', data: text('{"monster": "togera"}') }]);
  await assert.rejects(importZip(mods, lower, { builtin: ['Goliath Prime', 'Cerebulon'] }), /built-in/);
  assert.equal(mods.items.size, 0);
  const ok = await zip([{ name: 'Creator/skins/x/skin.json', data: text('{"monster": "togera"}') }]);
  assert.equal((await importZip(mods, ok, { builtin: ['Goliath Prime'] })).length, 1);
});

test('lobby choices: a skin is "<monster number>:<id>", a map is its id; what the lobby cannot carry is left out', () => {
  const list = [
    { mod: 'Creator', kind: 'skins', id: 'green', name: 'GREEN', monster: 'togera' },
    { mod: 'Creator', kind: 'skins', id: 'chrome', name: 'CHROME', monster: 'Robo-47' },
    { mod: 'Creator', kind: 'skins', id: '2fast', name: 'FAST', monster: 'togera' },   // a leading digit reads as a disc costume
    { mod: 'Creator', kind: 'skins', id: 'odd', name: 'ODD', monster: 'nobody' },      // not a playable monster
    { mod: 'Maps', kind: 'stages', id: 't1', name: 'DOWNTOWN MESS' },
  ];
  assert.deepEqual(lobbyChoices(list), {
    skins: [{ value: '3:green', monster: 3, label: 'GREEN (skin)' }, { value: '2:chrome', monster: 2, label: 'CHROME (skin)' }],
    maps: [{ value: 't1', label: 'DOWNTOWN MESS (map)' }],
  });
});

test('delete removes only that creation', async () => {
  const mods = await sample();
  await deleteCreation(mods, { mod: 'Creator', kind: 'skins', id: 'green' });
  assert.deepEqual((await listCreations(mods)).map((c) => c.id), ['t1']);
  assert.ok(mods.items.has('Goliath Prime'));
});
