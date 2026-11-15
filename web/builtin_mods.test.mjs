// ABOUTME: Tests for web/builtin_mods.js: the repo's mods served with the page are mirrored into <OPFS root>/mods/<mod>/ like
// ABOUTME: tools/sync_mods.sh does natively (other mods left alone), and only when they changed. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { syncBuiltinMods } = createRequire(import.meta.url)('./builtin_mods.js');

// an in-memory stand-in for the parts of the OPFS directory API the module uses
class Dir {
  constructor() { this.dirs = new Map(); this.files = new Map(); }
  async getDirectoryHandle(name, { create = false } = {}) {
    if (!this.dirs.has(name)) {
      if (!create) throw Object.assign(new Error('not found'), { name: 'NotFoundError' });
      this.dirs.set(name, new Dir());
    }
    return this.dirs.get(name);
  }
  async getFileHandle(name, { create = false } = {}) {
    if (!this.files.has(name) && !create) throw Object.assign(new Error('not found'), { name: 'NotFoundError' });
    const dir = this;
    return {
      async getFile() { return { text: async () => new TextDecoder().decode(dir.files.get(name)) }; },
      async createWritable() {
        const parts = [];
        return { write: async (d) => parts.push(typeof d === 'string' ? new TextEncoder().encode(d) : new Uint8Array(d)),
          close: async () => dir.files.set(name, Buffer.concat(parts)) };
      },
    };
  }
  async removeEntry(name) {
    if (!this.dirs.delete(name) && !this.files.delete(name)) throw Object.assign(new Error('not found'), { name: 'NotFoundError' });
  }
  // every file under this folder as "a/b/c" -> text
  list(prefix = '') {
    const out = {};
    for (const [n, d] of this.dirs) Object.assign(out, d.list(prefix + n + '/'));
    for (const [n, b] of this.files) out[prefix + n] = new TextDecoder().decode(b);
    return out;
  }
}

const manifest = (hash, files) => ({ hash, files: Object.keys(files).map((path) => ({ path })) });
const server = (files) => {
  const fetched = [];
  const fetchBytes = async (path) => { fetched.push(path); return new TextEncoder().encode(files[path]); };
  return { fetched, fetchBytes };
};
const FILES = { 'Goliath Prime/monsters/goliath/monster.json': '{"g":1}', 'Cerebulon/monsters/jelly/monster.json': '{"j":1}' };

test('copies every built-in mod file into mods/, keeping the folders', async () => {
  const root = new Dir();
  const { fetchBytes } = server(FILES);
  assert.equal(await syncBuiltinMods(root, manifest('h1', FILES), fetchBytes), 2);
  const mods = (await root.getDirectoryHandle('mods')).list();
  assert.deepEqual(mods, FILES);
});

test('does nothing when the same built-in mods are already there', async () => {
  const root = new Dir();
  await syncBuiltinMods(root, manifest('h1', FILES), server(FILES).fetchBytes);
  const again = server(FILES);
  assert.equal(await syncBuiltinMods(root, manifest('h1', FILES), again.fetchBytes), 0);
  assert.deepEqual(again.fetched, []);
});

test('mirrors a changed mod: files gone from the repo go away too, the player\'s own mods stay', async () => {
  const root = new Dir();
  await syncBuiltinMods(root, manifest('h1', FILES), server(FILES).fetchBytes);
  const creator = await (await root.getDirectoryHandle('mods')).getDirectoryHandle('Creator', { create: true });
  await (await (await creator.getFileHandle('a.json', { create: true })).createWritable()).close();
  const next = { 'Goliath Prime/monsters/goliath/monster.json': '{"g":2}' };   // Cerebulon left the repo
  await syncBuiltinMods(root, manifest('h2', next), server(next).fetchBytes);
  assert.deepEqual((await root.getDirectoryHandle('mods')).list(), { ...next, 'Creator/a.json': '' });
});

test('the record of what was copied lives outside mods/ (it must not change the mods digest)', async () => {
  const root = new Dir();
  await syncBuiltinMods(root, manifest('h1', FILES), server(FILES).fetchBytes);
  assert.deepEqual([...root.files.keys()], ['builtin_mods.json']);
});
