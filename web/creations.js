// ABOUTME: The player's creations in the browser: skins (mods/<mod>/skins/<id>/) and maps (mods/<mod>/stages/<id>/) in the mods folder
// ABOUTME: the game reads from the browser's private file system. Lists, exports one as a zip, imports zips (strict paths) and deletes.
import { zip, unzip } from './zip.js';

const KINDS = { skins: 'skin.json', stages: 'stage.json' };   // each creation folder's own file
// as native/wotm_mods.hpp scans them: a skin id is a-z, 0-9 and _ (up to 32); a map id starts with a letter (up to 16)
const ID = { skins: /^[a-z0-9_]{1,32}$/, stages: /^[a-z][a-z0-9_]{0,15}$/ };
const MOD = /^[A-Za-z0-9][A-Za-z0-9 _-]{0,31}$/;   // the game skips folders starting with '_' or '.'
const FILE = /^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$/;
const LIMITS = { maxEntries: 256, maxBytes: 32 << 20 };

// "Creator/skins/green/t0004.png" -> {mod, kind, id, file}; null for anything else (no '..', no deeper folders, no other folders)
export function parseCreationPath(path) {
  const parts = path.split('/');
  if (parts.length !== 4) return null;
  const [mod, kind, id, file] = parts;
  if (!MOD.test(mod) || !(kind in KINDS) || !ID[kind].test(id) || !FILE.test(file)) return null;
  return { mod, kind, id, file };
}

async function children(dir, kind) {
  const out = [];
  for await (const [name, h] of dir.entries()) if (h.kind === kind) out.push([name, h]);
  return out.sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0));
}
async function readFile(dir, name) {
  return new Uint8Array(await (await (await dir.getFileHandle(name)).getFile()).arrayBuffer());
}
async function folder(mods, { mod, kind, id }, create = false) {
  const m = await mods.getDirectoryHandle(mod, { create });
  return (await m.getDirectoryHandle(kind, { create })).getDirectoryHandle(id, { create });
}

// What the list shows of a creation: the name the game shows ("name" in skin.json or stage.json, else the id) and, for a skin, its
// monster ("monster" in skin.json, lowercase)
async function describe(dir, kind, id) {
  let doc = {};
  try { doc = JSON.parse(new TextDecoder().decode(await readFile(dir, KINDS[kind]))); } catch { /* unreadable: the id alone */ }
  const out = { name: typeof doc.name === 'string' && doc.name ? doc.name : id };
  if (kind === 'skins') out.monster = typeof doc.monster === 'string' ? doc.monster.toLowerCase() : '';
  return out;
}

// [{mod, kind, id, name, monster (skins)}], by mod, then skins before maps, then id
export async function listCreations(mods) {
  const out = [];
  for (const [mod, m] of await children(mods, 'directory')) {
    if (!MOD.test(mod)) continue;
    for (const kind of Object.keys(KINDS)) {
      let k;
      try { k = await m.getDirectoryHandle(kind); } catch { continue; }
      for (const [id, dir] of await children(k, 'directory'))
        if (ID[kind].test(id)) out.push({ mod, kind, id, ...(await describe(dir, kind, id)) });
    }
  }
  return out;
}

// {fileName: "<id>.zip", bytes}: the creation's files under their mods/ paths, so an import puts them back in the same place
export async function exportCreation(mods, c) {
  const dir = await folder(mods, c);
  const entries = [];
  for (const [file] of await children(dir, 'file'))
    if (FILE.test(file)) entries.push({ name: `${c.mod}/${c.kind}/${c.id}/${file}`, data: await readFile(dir, file) });
  return { fileName: `${c.id}.zip`, bytes: await zip(entries) };
}

// Every entry is checked before anything is written: one entry outside the creation layout, one in a built-in mod's folder
// (`builtin`: the names in mods/manifest.json, mirrored whole before each boot, so an import there would be wiped), or a creation
// folder without its own json file, refuses the whole zip. An imported creation replaces a folder of the same path. Returns the
// imported creations.
export async function importZip(mods, bytes, { builtin = [] } = {}) {
  const entries = await unzip(bytes, LIMITS);
  const reserved = new Set(builtin.map((n) => n.toLowerCase()));
  const byFolder = new Map();
  for (const e of entries) {
    const p = parseCreationPath(e.name);
    if (!p) throw new Error(`${e.name}: not part of a creation (expected <mod>/skins/<id>/<file> or <mod>/stages/<id>/<file>)`);
    if (reserved.has(p.mod.toLowerCase()))
      throw new Error(`${e.name}: "${p.mod}" is a built-in mod's folder; put the creation in a folder of its own (e.g. Creator)`);
    const key = `${p.mod}/${p.kind}/${p.id}`;
    if (!byFolder.has(key)) byFolder.set(key, { c: { mod: p.mod, kind: p.kind, id: p.id }, files: [] });
    byFolder.get(key).files.push({ file: p.file, data: e.data });
  }
  if (byFolder.size === 0) throw new Error('the zip has no creation in it');
  for (const [key, { c, files }] of byFolder)
    if (!files.some((f) => f.file === KINDS[c.kind])) throw new Error(`${key}: missing its ${KINDS[c.kind]}`);
  const out = [];
  for (const { c, files } of byFolder.values()) {
    await deleteCreation(mods, c);
    const dir = await folder(mods, c, true);
    for (const { file, data } of files) {
      const w = await (await dir.getFileHandle(file, { create: true })).createWritable();
      await w.write(data);
      await w.close();
    }
    out.push({ ...c, ...(await describe(dir, c.kind, c.id)) });
  }
  return out;
}

// The playable monsters by the names skin.json uses (native/wotm_mods.hpp kMonsters)
const MONSTER_IDS = { congar: 1, 'robo-47': 2, togera: 3, 'ultra-v': 4, preytor: 5, kineticlops: 7, magmo: 8, raptros: 9, agamo: 10,
  zorgulon: 11 };

// What the online lobby can carry (protocol 5): skins as "<monster number>:<id>" and maps by id. A skin of a monster the lobby doesn't
// know, or whose id starts with a digit (the game reads that as a disc costume number), is left out
export function lobbyChoices(list) {
  const skins = [], maps = [];
  for (const c of list) {
    if (c.kind === 'stages') maps.push({ value: c.id, label: `${c.name} (map)` });
    const monster = MONSTER_IDS[(c.monster || '').toLowerCase()];
    if (c.kind === 'skins' && monster && !/^[0-9]/.test(c.id)) skins.push({ value: `${monster}:${c.id}`, monster, label: `${c.name} (skin)` });
  }
  return { skins, maps };
}

// Removes the creation's folder (nothing when there is none)
export async function deleteCreation(mods, { mod, kind, id }) {
  try {
    const k = await (await mods.getDirectoryHandle(mod)).getDirectoryHandle(kind);
    await k.removeEntry(id, { recursive: true });
  } catch (e) {
    if (e?.name !== 'NotFoundError') throw e;
  }
}
