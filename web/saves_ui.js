// ABOUTME: The Settings card's Saves tab: the memory-card saves kept in this browser, Back up (zip download), Restore (zip upload, with a
// ABOUTME: confirmation and an automatic backup of what it replaces), Delete all, and the automatic backups. Runs before the game starts.
import {
  readSaves, summarize, backupZip, readBackup, replaceSaves, clearSaves, keepAutoBackup, listAutoBackups, readAutoBackup, stamp, prettyName, AUTO_BACKUPS,
} from './saves.js';
import { gameRunningElsewhere } from './gamelock.js';
import { download } from './creations_ui.js';

const $ = (id) => document.getElementById(id);
const root = () => navigator.storage.getDirectory();
const MAX_ZIP = 16 << 20;
const kb = (n) => (n < 1024 ? `${n} bytes` : `${Math.round(n / 1024)} KB`);
const when = (ms) => new Date(ms).toLocaleString();

const plural = (n, one, many) => `${n} ${n === 1 ? one : many}`;
function describe(s) {
  if (!s.files) return 'no saves';
  const parts = [plural(s.files, 'file', 'files')];
  if (s.hasJson && !s.jsonError) parts.push(plural(s.slots.length, 'adventure slot', 'adventure slots'), `${s.tokens ?? 0} battle tokens`);
  return parts.join(', ');
}

export function initSaves() {
  const say = (text) => { $('savesMsg').textContent = text; };
  let pending = null;   // { text, ok, run }: the action waiting for a yes

  const actions = ['backupSaves', 'restoreSaves', 'deleteSaves'].map($);

  function confirm(action) {
    pending = action;
    $('savesConfirmText').textContent = action.text;
    $('savesConfirmOk').textContent = action.ok;
    $('savesConfirm').hidden = false;
    $('savesConfirmOk').focus();
  }
  function settle() {
    pending = null;
    $('savesConfirm').hidden = true;
  }

  async function refresh() {
    let current;
    try {
      current = await readSaves(await root());
    } catch (e) {
      $('savesSummary').textContent = `This browser's saves could not be read: ${e.message}`;
      $('savesSlots').replaceChildren();
      for (const b of actions) b.disabled = true;
      return;
    }
    const s = summarize(current.entries);
    $('savesSummary').textContent = s.files
      ? `${describe(s)}, ${kb(s.bytes)}. Last saved ${when(current.modified)}.${s.jsonError ? ` ${s.jsonError}.` : ''}`
      : 'No saves in this browser yet. They appear after the game saves.';
    $('savesSlots').replaceChildren(...s.slots.map((slot) => Object.assign(document.createElement('li'), {
      textContent: `Slot ${slot.slot}: ${prettyName(slot.monster)}, ${prettyName(slot.level)}, ${plural(slot.lives, 'life', 'lives')}, score ${slot.score}`,
    })));
    const busy = await gameRunningElsewhere(navigator.locks);
    for (const b of actions) b.disabled = busy;
    $('backupSaves').disabled = busy || !s.files;
    $('deleteSaves').disabled = busy || !s.files;
    if (busy) say('A game is running in another tab of this browser. Close it to back up or change saves here.');
    await listBackups();
  }

  async function listBackups() {
    const list = await listAutoBackups(await root()).catch(() => []);
    $('savesBackups').replaceChildren(...(list.length ? list : [null]).map((b) => {
      const li = document.createElement('li');
      if (!b) return Object.assign(li, { textContent: 'None yet. One is kept each time a restore or a delete replaces saves.' });
      const label = Object.assign(document.createElement('span'), { textContent: `${when(b.modified)} (${kb(b.size)})` });
      const button = Object.assign(document.createElement('button'), { textContent: 'Download' });
      button.addEventListener('click', async () => {
        try { download(b.name, await readAutoBackup(await root(), b.name)); } catch (e) { say(`Could not read that backup: ${e.message}`); }
      });
      li.append(label, button);
      return li;
    }));
  }

  // the saves as they are now, to keep before they are replaced; the name of the backup, or null when there was nothing to keep
  async function keepCurrent() {
    const { entries } = await readSaves(await root());
    if (!entries.length) return null;
    return keepAutoBackup(await root(), await backupZip(entries), stamp(new Date()));
  }

  // what a failed restore or delete tells: the backup made first is where the saves are
  const withBackup = async (work) => {
    const kept = await keepCurrent();
    try {
      await work();
    } catch (e) {
      throw new Error(`${e.message}${kept ? ` Your previous saves are kept as ${kept}.` : ''}`);
    }
    return kept;
  };

  // a restore or a delete changes files the game writes itself, so it is refused while another tab runs the game
  async function guard() {
    if (!(await gameRunningElsewhere(navigator.locks))) return true;
    say('A game is running in another tab of this browser. Close it first.');
    return false;
  }

  $('backupSaves').addEventListener('click', async () => {
    settle();
    try {
      const { entries } = await readSaves(await root());
      if (!entries.length) return say('There are no saves to back up.');
      download(`wotlane-saves-${stamp(new Date())}.zip`, await backupZip(entries));
      say(`Backed up ${plural(entries.length, 'file', 'files')}.`);
    } catch (e) {
      say(`Could not back up: ${e.message}`);
    }
  });

  $('restoreSaves').addEventListener('click', () => { settle(); $('restoreFile').click(); });
  $('restoreFile').addEventListener('change', async () => {
    const file = $('restoreFile').files[0];
    $('restoreFile').value = '';
    if (!file) return;
    try {
      if (file.size > MAX_ZIP) throw new Error('That file is too large to be a save backup.');
      const incoming = await readBackup(new Uint8Array(await file.arrayBuffer()));
      const now = summarize((await readSaves(await root())).entries);
      const left = incoming.ignored.length ? ` Left out: ${incoming.ignored.join(', ')}.` : '';
      say('');
      confirm({
        text: `Restore ${file.name}? It holds ${describe(summarize(incoming.entries))}. This replaces your current saves (${describe(now)}); ` +
          `those are kept as an automatic backup first.${left}`,
        ok: 'Restore',
        run: async () => {
          if (!(await guard())) return;
          const kept = await withBackup(async () => replaceSaves(await root(), incoming.entries));
          say(`Restored ${plural(incoming.entries.length, 'file', 'files')}.${kept ? ` Your previous saves are kept as ${kept}.` : ''}`);
        },
      });
    } catch (e) {
      settle();
      say(`${file.name}: ${e.message}`);
    }
  });

  $('deleteSaves').addEventListener('click', async () => {
    const now = summarize((await readSaves(await root())).entries);
    confirm({
      text: `Delete all saves in this browser (${describe(now)})? The game starts fresh. They are kept as an automatic backup first ` +
        `(the ${AUTO_BACKUPS} newest are kept).`,
      ok: 'Delete',
      run: async () => {
        if (!(await guard())) return;
        const kept = await withBackup(async () => clearSaves(await root()));
        say(`Deleted.${kept ? ` Kept as ${kept}.` : ''}`);
      },
    });
  });

  $('savesConfirmOk').addEventListener('click', async () => {
    const action = pending;
    settle();
    if (!action) return;
    try {
      // one restore or delete at a time across this browser's Settings tabs
      await (navigator.locks ? navigator.locks.request('wotlane-saves', () => action.run()) : action.run());
    } catch (e) {
      say(`Did not finish: ${e.message}`);
    }
    await refresh();
  });
  $('savesConfirmCancel').addEventListener('click', () => { settle(); say(''); });

  return { refresh };
}
