// ABOUTME: The front panel's "Your creations" section: lists the skins and maps saved in this browser, exports one as a zip to share,
// ABOUTME: imports a friend's zip and deletes. Runs before the game starts (the game scans mods/ once, at boot).
import { listCreations, exportCreation, importZip, deleteCreation } from './creations.js';

const $ = (id) => document.getElementById(id);
const KIND_LABEL = { skins: 'skin', stages: 'map' };

// mods/ at the root of the browser's private file system: the game's /save/mods (wotm_webfs.cpp mounts that root at /save)
async function modsFolder() {
  return (await navigator.storage.getDirectory()).getDirectoryHandle('mods', { create: true });
}

function download(fileName, bytes) {
  const url = URL.createObjectURL(new Blob([bytes], { type: 'application/zip' }));
  const a = Object.assign(document.createElement('a'), { href: url, download: fileName });
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

export function initCreations() {
  const say = (text) => { $('creationsMsg').textContent = text; };

  async function refresh() {
    let list;
    try {
      list = await listCreations(await modsFolder());
    } catch (e) {
      say(`Could not read this browser's saved creations: ${e.message}`);
      return;
    }
    $('creationsCount').textContent = list.length ? `(${list.length})` : '';
    $('creationList').replaceChildren(...list.map((c) => {
      const li = document.createElement('li');
      li.className = 'row';
      const label = document.createElement('span');
      label.textContent = `${c.name} (${KIND_LABEL[c.kind]}, ${c.mod})`;
      const exp = Object.assign(document.createElement('button'), { textContent: 'Export' });
      exp.addEventListener('click', async () => {
        try {
          const { fileName, bytes } = await exportCreation(await modsFolder(), c);
          download(fileName, bytes);
          say(`Saved ${fileName}. Send it to your friends: they import it here before joining your lobby.`);
        } catch (e) { say(`Export failed: ${e.message}`); }
      });
      const del = Object.assign(document.createElement('button'), { textContent: 'Delete' });
      del.addEventListener('click', async () => {
        if (del.textContent !== 'Sure?') { del.textContent = 'Sure?'; return; }   // a second click deletes
        try {
          await deleteCreation(await modsFolder(), c);
          say(`Deleted ${c.name}.`);
        } catch (e) { say(`Delete failed: ${e.message}`); }
        refresh();
      });
      li.append(label, exp, del);
      return li;
    }));
    if (!list.length) say('Nothing yet. Make a skin with CREATE in Single player, or import a friend\'s zip.');
  }

  $('importFile').addEventListener('change', async (ev) => {
    const file = ev.target.files[0];
    ev.target.value = '';
    if (!file) return;
    try {
      const added = await importZip(await modsFolder(), new Uint8Array(await file.arrayBuffer()));
      say(`Imported ${added.map((c) => c.name).join(', ')}.`);
    } catch (e) {
      say(`Could not import ${file.name}: ${e.message}`);
    }
    refresh();
  });
  $('importCreation').addEventListener('click', () => $('importFile').click());
  refresh();
}
