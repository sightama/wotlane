// ABOUTME: The memory-card saves in the browser's private file system (save.json and the mc0/ and mc1/ card folders, the game's /save):
// ABOUTME: listing, a summary, backup and restore through a zip (validated, with rollback), delete, automatic backups and the game lock.
import { zip, unzip } from './zip.js';

const JSON_FILE = 'save.json';
const CARDS = ['mc0', 'mc1'];
const BACKUP_DIR = 'backups';
const BACKUP_NAME = /^saves-\d{8}-\d{6}(-\d+)?\.zip$/;
const SEGMENT = /^[A-Za-z0-9][A-Za-z0-9 _.-]{0,63}$/;
const MAX_FILES = 40;
const MAX_JSON = 1 << 20;
const MAX_SLOTS = 99;   // the game's own limit (docs/passos/NATIVO.md, Save in JSON)
const MAX_UNPACKED = 8 << 20;

export const AUTO_BACKUPS = 5;

// a missing entry, or one of the wrong kind (a file where a folder belongs): either way there is nothing of that name to read
const notFound = (e) => e?.name === 'NotFoundError' || e?.name === 'TypeMismatchError';
const text = new TextDecoder();

async function fileBytes(handle) {
  const file = await handle.getFile();
  return { data: new Uint8Array(await file.arrayBuffer()), modified: file.lastModified };
}

async function walk(dir, prefix, out) {
  for await (const [name, handle] of dir.entries()) {
    if (handle.kind === 'directory') await walk(handle, `${prefix}${name}/`, out);
    else out.push({ name: prefix + name, ...(await fileBytes(handle)) });
  }
}

// The saves as [{name, data}] (save.json, then every file of mc0/ and mc1/, named by path from the save folder), and when the newest of
// them was written (null when there are none)
export async function readSaves(root) {
  const found = [];
  try {
    found.push({ name: JSON_FILE, ...(await fileBytes(await root.getFileHandle(JSON_FILE))) });
  } catch (e) {
    if (!notFound(e)) throw e;
  }
  for (const card of CARDS) {
    try {
      await walk(await root.getDirectoryHandle(card), card + '/', found);
    } catch (e) {
      if (!notFound(e)) throw e;
    }
  }
  const modified = found.length ? Math.max(...found.map((f) => f.modified)) : null;
  return { entries: found.map(({ name, data }) => ({ name, data })), modified };
}

// What the Saves tab shows of a set of entries: how many files and bytes, and from save.json the battle tokens and each adventure slot
// that holds a game
export function summarize(entries) {
  const out = { files: entries.length, bytes: entries.reduce((n, e) => n + e.data.length, 0), hasJson: false, jsonError: null, tokens: null, slots: [] };
  const json = entries.find((e) => e.name === JSON_FILE);
  if (!json) return out;
  out.hasJson = true;
  const save = parseSave(json.data);
  if (!save) {
    out.jsonError = 'save.json is not readable';
    return out;
  }
  if (typeof save.battleTokens === 'number') out.tokens = save.battleTokens;
  (save.saves ?? []).slice(0, MAX_SLOTS).forEach((s, i) => {
    if (plain(s) && typeof s.monster === 'string' && typeof s.level === 'string') out.slots.push({ slot: i + 1, monster: String(s.monster), level: String(s.level), lives: Number(s.lives), score: Number(s.score) });
  });
  return out;
}

const plain = (v) => v !== null && typeof v === 'object' && !Array.isArray(v);

// save.json parsed, or null when it is not JSON or not what the game writes (an object with at least one of options, unlocks,
// battleTokens and saves, each of the right kind)
function parseSave(bytes) {
  let j;
  try { j = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes)); } catch { return null; }
  if (!plain(j) || !['options', 'unlocks', 'battleTokens', 'saves'].some((k) => k in j)) return null;
  if ('battleTokens' in j && typeof j.battleTokens !== 'number') return null;
  if ('saves' in j && !Array.isArray(j.saves)) return null;
  for (const k of ['options', 'unlocks']) if (k in j && !plain(j[k])) return null;
  return j;
}

export function backupZip(entries) {
  return zip(entries);
}

const cleanPath = (name) => !name.startsWith('/') && !/[\\\u0000-\u001f]/.test(name) && name.split('/').every((s) => s && s !== '.' && s !== '..');
const isSavePath = (name) => {
  if (name === JSON_FILE) return true;
  const parts = name.split('/');
  return parts.length >= 2 && parts.length <= 4 && CARDS.includes(parts[0]) && parts.slice(1).every((s) => SEGMENT.test(s));
};

// A backup zip (or a zip of the save folder) as the entries it restores and the names it leaves out. Everything is checked before
// anything is written: names, sizes, and that save.json is a save. Throws an Error with a plain sentence
export async function readBackup(bytes) {
  const unpacked = await unzip(bytes, { maxEntries: 64, maxBytes: MAX_UNPACKED });
  // what Finder and Explorer add to a zipped folder is not part of it
  const junk = (n) => n.startsWith('__MACOSX/') || n.split('/').at(-1) === '.DS_Store' || n.split('/').at(-1) === 'Thumbs.db';
  const all = unpacked.filter((e) => !junk(e.name));
  if (all.some((e) => !cleanPath(e.name))) throw new Error('The zip has file names that could reach outside the saves, so it was not used.');
  // a zip of the folder itself holds everything inside one folder: look inside it
  const tops = new Set(all.map((e) => e.name.split('/')[0]));
  const wrapped = tops.size === 1 && !all.some((e) => isSavePath(e.name)) && all.every((e) => e.name.includes('/'));
  const entries = [], ignored = [];
  for (const e of all) {
    const name = wrapped ? e.name.slice(e.name.indexOf('/') + 1) : e.name;
    if (isSavePath(name)) entries.push({ name, data: e.data });
    else if (CARDS.includes(name.split('/')[0]) || name === JSON_FILE) throw new Error(`${e.name}: not a file name the saves use.`);
    else ignored.push(e.name);
  }
  if (!entries.length) throw new Error('There are no saves in this zip (it needs save.json or the mc0 and mc1 folders).');
  if (new Set(entries.map((e) => e.name)).size !== entries.length) throw new Error('The zip lists the same file twice.');
  if (entries.length > MAX_FILES) throw new Error(`There are too many files for saves in this zip (${entries.length}, at most ${MAX_FILES}).`);
  const json = entries.find((e) => e.name === JSON_FILE);
  if (json) {
    if (json.data.length > MAX_JSON) throw new Error('save.json is too large to be a save.');
    if (!parseSave(json.data)) {
      let readable = true;
      try { JSON.parse(text.decode(json.data)); } catch { readable = false; }
      throw new Error(readable ? 'save.json is not a War of the Monsters save.' : 'save.json in the zip is not readable.');
    }
  }
  return { entries, ignored };
}

async function writeEntries(root, entries) {
  for (const { name, data } of entries) {
    const parts = name.split('/');
    let dir = root;
    for (const p of parts.slice(0, -1)) dir = await dir.getDirectoryHandle(p, { create: true });
    const writable = await (await dir.getFileHandle(parts.at(-1), { create: true })).createWritable();
    await writable.write(data);
    await writable.close();
  }
}

// Every save removed (settings, mods and the rest of the folder stay)
export async function clearSaves(root) {
  for (const name of [JSON_FILE, ...CARDS]) {
    try {
      await root.removeEntry(name, { recursive: true });
    } catch (e) {
      if (!notFound(e)) throw e;
    }
  }
}

// The saves swapped for `entries`: the current ones go first, since a save.json restored over old card files would bring their
// adventures back. A step that fails puts the old saves back and throws; if even that fails the error says so
export async function replaceSaves(root, entries) {
  const { entries: old } = await readSaves(root);
  try {
    await clearSaves(root);
    await writeEntries(root, entries);
  } catch (e) {
    try {
      await clearSaves(root).catch(() => {});   // the old files are written over whatever is left
      await writeEntries(root, old);
    } catch {
      throw new Error(`${e.message}. The old saves could not be put back; they are in the automatic backup.`);
    }
    throw e;
  }
}

export const stamp = (d) => {
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
};

async function backupFolder(root, create) {
  try {
    return await root.getDirectoryHandle(BACKUP_DIR, { create });
  } catch (e) {
    if (notFound(e)) return null;
    throw e;
  }
}

// The newest automatic backups first: [{name, size, modified}]
export async function listAutoBackups(root) {
  const dir = await backupFolder(root, false);
  if (!dir) return [];
  const out = [];
  for await (const [name, handle] of dir.entries()) {
    if (handle.kind !== 'file' || !BACKUP_NAME.test(name)) continue;
    const file = await handle.getFile();
    out.push({ name, size: file.size, modified: file.lastModified });
  }
  return out.sort((a, b) => b.modified - a.modified || (a.name < b.name ? 1 : -1));
}

// A zip of the saves a restore or a delete is about to replace, kept in the browser; returns its name. The newest AUTO_BACKUPS stay (by the
// time they were written, never the one just made), and a second backup in the same second gets its own name
export async function keepAutoBackup(root, bytes, when) {
  const dir = await backupFolder(root, true);
  const taken = new Set((await listAutoBackups(root)).map((b) => b.name));
  let name = `saves-${when}.zip`;
  for (let n = 2; taken.has(name); n++) name = `saves-${when}-${n}.zip`;
  const writable = await (await dir.getFileHandle(name, { create: true })).createWritable();
  await writable.write(bytes);
  await writable.close();
  for (const old of (await listAutoBackups(root)).filter((b) => b.name !== name).slice(AUTO_BACKUPS - 1)) await dir.removeEntry(old.name);
  return name;
}

export async function readAutoBackup(root, name) {
  if (!BACKUP_NAME.test(name)) throw new Error(`${name} is not an automatic backup`);
  const dir = await backupFolder(root, false);
  if (!dir) throw new Error(`${name} is not an automatic backup`);
  return (await fileBytes(await dir.getFileHandle(name))).data;
}


// "midtown_park" -> "Midtown Park", "ultra-v" -> "Ultra-V"
export const prettyName = (name) => String(name).replace(/_/g, ' ').replace(/(^|[ -])([a-z])/g, (m, p, c) => p + c.toUpperCase());
