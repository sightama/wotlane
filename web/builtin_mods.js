// ABOUTME: Mirrors the repo's mods, served with the page (mods/manifest.json from tools/build_web.sh), into <OPFS root>/mods/<mod>/
// ABOUTME: before the game starts, as tools/sync_mods.sh does natively: the same mods digest as a native install, other mods left alone.
'use strict';

const RECORD = 'builtin_mods.json';   // at the OPFS root, outside mods/: a file inside would change the mods digest

async function readRecord(root) {
  try {
    return JSON.parse(await (await (await root.getFileHandle(RECORD)).getFile()).text());
  } catch {
    return { hash: '', mods: [] };
  }
}

async function writeFile(dir, name, bytes) {
  const w = await (await dir.getFileHandle(name, { create: true })).createWritable();
  await w.write(bytes);
  await w.close();
}

async function remove(dir, name) {
  try {
    await dir.removeEntry(name, { recursive: true });
  } catch (e) {
    if (e.name !== 'NotFoundError') throw e;
  }
}

// root: the OPFS root directory; manifest: { hash, files: [{ path: '<mod>/<...>' }] }; fetchBytes(path) gives a file's bytes.
// Returns how many files were copied (0 when the same built-in mods are already there)
async function syncBuiltinMods(root, manifest, fetchBytes) {
  const record = await readRecord(root);
  if (record.hash === manifest.hash) return 0;
  const modsDir = await root.getDirectoryHandle('mods', { create: true });
  const names = [...new Set(manifest.files.map((f) => f.path.split('/')[0]))];
  for (const name of new Set([...record.mods, ...names])) await remove(modsDir, name);   // mirrored whole, like rsync --delete
  for (const { path } of manifest.files) {
    const parts = path.split('/');
    let dir = modsDir;
    for (const part of parts.slice(0, -1)) dir = await dir.getDirectoryHandle(part, { create: true });
    await writeFile(dir, parts[parts.length - 1], await fetchBytes(path));
  }
  await writeFile(root, RECORD, JSON.stringify({ hash: manifest.hash, mods: names }));
  return manifest.files.length;
}

if (typeof module !== 'undefined' && module.exports) module.exports = { syncBuiltinMods };
else globalThis.wotmSyncBuiltinMods = syncBuiltinMods;
