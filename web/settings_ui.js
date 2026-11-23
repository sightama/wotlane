// ABOUTME: The front panel's Settings card: tabs of options (web/settings.js) that are saved to settings.ini the moment they change
// ABOUTME: and apply the next time the game starts, plus the Controls card, your creations and the player's name.
import { TABS, OPTIONS, optionsOf, choiceFor, readSettings, updateSetting, editSettings } from './settings.js';
import {
  MAX_KEYS, PRESETS, assignKey, clearKey, keyNameOfCode, labelOfKey, mouseNameOfButton, presetBinds, readBinds, wheelNameOfDelta, writeBinds,
} from './keybinds.js';
import { gameRunningElsewhere } from './gamelock.js';
import { initSaves } from './saves_ui.js';

const $ = (id) => document.getElementById(id);
const SAVED = 'Saved as you change them. Changes apply the next time you press Play.';

const root = () => navigator.storage.getDirectory();

// the value a page option keeps in the browser's own storage, under its lowercase key
export const pageKey = (option) => option.key.toLowerCase();

function optionRow(option, text, onChange, recall) {
  const id = 'opt_' + option.key;
  const { value, custom } = option.page ? { value: recall(pageKey(option), option.def), custom: false } : choiceFor(option, text);
  const select = Object.assign(document.createElement('select'), { id });
  if (option.page) select.dataset.page = '1';   // kept by the page itself: it never clashes with a running game's files
  for (const [v, label] of option.choices) select.add(new Option(label, v, false, v === value));
  if (custom) select.add(new Option(`Custom (${value})`, value, true, true));
  select.addEventListener('change', () => onChange(option, select));
  const label = Object.assign(document.createElement('label'), { htmlFor: id, textContent: option.label });
  const hint = Object.assign(document.createElement('span'), { className: 'hint', textContent: [option.hint, onlineNote(option)].filter(Boolean).join(' ') });
  const row = document.createElement('div');
  row.className = 'option';
  row.dataset.group = option.group;
  row.append(label, select, hint);
  return row;
}

// what an online match does with the option
function onlineNote(option) {
  if (option.online?.fixed !== undefined) {
    const shown = option.choices.find(([v]) => v === option.online.fixed)[1];
    return `In an online match it is ${shown} for everyone.`;
  }
  return option.online?.lobby ? "In an online match the lobby's owner decides." : '';
}

// the Online tab says which options a match fixes, from the same list the other tabs show
function onlineNotes() {
  const fixed = OPTIONS.filter((o) => o.online?.fixed !== undefined).map((o) => {
    const shown = o.choices.find(([v]) => v === o.online.fixed)[1];
    return `${o.label}: ${shown}`;
  });
  const lobby = OPTIONS.filter((o) => o.online?.lobby).map((o) => o.label);
  const p = (text) => Object.assign(document.createElement('p'), { className: 'online', textContent: text });
  return [
    p(`In an online match these are the same for everyone: ${fixed.join('; ')}.`),
    p(`The lobby's owner sets ${lobby.join(', ')} and the stage. Everything else here is your own choice.`),
  ];
}

function groupedRows(rows) {
  const out = [];
  let group = null;
  for (const row of rows) {
    if (row.dataset.group !== group) {
      group = row.dataset.group;
      out.push(Object.assign(document.createElement('h2'), { textContent: group }));
    }
    out.push(row);
  }
  return out;
}

export function initSettings({ remember, recall }) {
  const home = $('home'), card = $('settings');
  const say = (text) => { $('settingsMsg').textContent = text; };
  const saves = initSaves();
  let text = '';
  let editable = false;

  async function save(option, select) {
    if (option.page) {
      remember(pageKey(option), select.value);
      return say(SAVED);
    }
    const before = choiceFor(option, text).value;
    if (select.value === before) return;   // the file's own value, picked again
    try {
      text = await updateSetting(await root(), option.key, select.value);
      say(SAVED);
    } catch (e) {
      select.value = before;
      say(`Could not save ${option.label}: ${e.message}`);
    }
  }

  function fill() {
    for (const tab of TABS) {
      const holder = $('panel-' + tab.id).querySelector('.options');
      if (!holder) continue;
      holder.replaceChildren(...groupedRows(optionsOf(tab.id).map((o) => optionRow(o, text, save, recall))));
    }
    for (const select of card.querySelectorAll('.options select')) select.disabled = !editable && !select.dataset.page;
    binds = null;   // read again from the file just read
    renderKeys();
  }

  // the Keyboard table of the Controls tab: one button per key a game button takes; pressing it waits for a key, a mouse button or the
  // wheel (Esc cancels) and saves at once, like the options
  const keyLayout = navigator.keyboard?.getLayoutMap?.().catch(() => undefined);   // letters as this keyboard prints them, where the browser has it
  let layoutMap;
  keyLayout?.then((m) => { layoutMap = m; if (!capturing) renderKeys(); });
  let binds = null;
  let stopCapture = () => {};
  let capturing = false;

  const keyWords = (name) => labelOfKey(name, layoutMap, globalThis.keyLabel);
  const nameOf = (bind) => globalThis.CONTROLS.find((c) => c.bind === bind).does;

  async function saveKeys(next, notice) {
    const before = binds;
    binds = next;
    renderKeys();
    try {
      text = await editSettings(await root(), (t) => writeBinds(t, next));
      say(notice ?? SAVED);
    } catch (e) {
      binds = before;
      renderKeys();
      say(`Could not save the keys: ${e.message}`);
    }
  }

  // the events a mouse press goes on to make (click, auxclick, context menu) are dropped for a moment
  function swallowAfterMouse() {
    const drop = (e) => { e.preventDefault(); e.stopPropagation(); };
    const types = ['click', 'auxclick', 'contextmenu'];
    for (const t of types) window.addEventListener(t, drop, true);
    setTimeout(() => { for (const t of types) window.removeEventListener(t, drop, true); }, 400);
  }

  function chooseKey(bind, slot, button) {
    stopCapture();
    const was = button.textContent;
    button.textContent = 'Press a key…';
    capturing = true;
    say('Press a key, click a mouse button or turn the wheel. Esc cancels.');
    const finish = () => {
      for (const [type, fn, opts] of listeners) window.removeEventListener(type, fn, opts);
      stopCapture = () => {};
      capturing = false;
    };
    const cancel = () => { finish(); button.textContent = was; say(SAVED); };
    const take = (name) => {
      finish();
      const { binds: next, swapped } = assignKey(binds, bind, slot, name);
      const others = swapped.map((s) => `${nameOf(s.bind)} ${s.gets ? 'got ' + keyWords(s.gets) : 'lost it'}`);
      saveKeys(next, swapped.length ? `${keyWords(name)} now does ${nameOf(bind)}; ${others.join(' and ')}.` : SAVED);
    };
    const onKey = (e) => {
      e.preventDefault();
      if (e.repeat) return;   // the Enter or Space that pressed the button is still going down
      e.stopPropagation();   // Esc must not also close the card
      if (e.code === 'Escape') return cancel();
      const name = keyNameOfCode(e.code);
      if (!name) return say('The game cannot use that key (F1 to F12 belong to the runtime, and the game has no name for some keys). Press another.');
      take(name);
    };
    const onMouse = (e) => {
      e.preventDefault();
      swallowAfterMouse();   // the click or context menu that follows is not for the page
      if (e.button === 0 && e.target === button) return cancel();   // clicking the waiting button again gives up
      take(mouseNameOfButton(e.button));
    };
    const onWheel = (e) => { e.preventDefault(); if (wheelNameOfDelta(e.deltaY)) take(wheelNameOfDelta(e.deltaY)); };
    const listeners = [['keydown', onKey, true], ['mousedown', onMouse, true], ['wheel', onWheel, { capture: true, passive: false }]];
    for (const [type, fn, opts] of listeners) window.addEventListener(type, fn, opts);
    stopCapture = cancel;
  }

  function renderKeys() {
    if (!binds) binds = readBinds(text, globalThis.CONTROLS);
    const focused = card.contains(document.activeElement) ? document.activeElement.dataset : {};   // the table is rebuilt: focus goes back to the same key
    const rows = globalThis.CONTROLS.map((c) => {
      const tr = document.createElement('tr');
      const does = Object.assign(document.createElement('td'), { textContent: c.does });
      const keys = document.createElement('td');
      if (c.bind) {
        const mine = binds.get(c.bind);
        for (let slot = 0; slot < Math.min(mine.length + 1, MAX_KEYS); slot++) {
          const b = Object.assign(document.createElement('button'), { className: 'key', textContent: mine[slot] ? keyWords(mine[slot]) : '+' });
          b.type = 'button';
          b.setAttribute('aria-label', mine[slot] ? `${c.does}, key ${slot + 1}: ${keyWords(mine[slot])}. Change` : `${c.does}: add key ${slot + 1}`);
          b.dataset.bind = c.bind;
          b.dataset.slot = String(slot);
          b.disabled = !editable;
          b.addEventListener('click', () => chooseKey(c.bind, slot, b));
          if (mine[slot]) {
            const x = Object.assign(document.createElement('button'), { className: 'keyclear', textContent: '×' });
            x.type = 'button';
            x.setAttribute('aria-label', `Take ${keyWords(mine[slot])} off ${c.does}`);
            x.dataset.clear = `${c.bind}:${slot}`;
            x.disabled = !editable;
            x.addEventListener('click', () => saveKeys(clearKey(binds, c.bind, slot)));
            keys.append(b, x);
          } else keys.append(b);
        }
      } else keys.textContent = c.keys.map(keyWords).join(', ');
      tr.append(does, keys);
      return tr;
    });
    $('controlsTable').replaceChildren(...rows);
    const back = focused.clear?.split(':') ?? [focused.bind, focused.slot];
    if (back[0]) $('controlsTable').querySelector(`.key[data-bind="${back[0]}"][data-slot="${back[1]}"]`)?.focus();
    $('keyPreset').disabled = $('keyReset').disabled = !editable;
  }

  async function open() {
    home.hidden = true;
    card.hidden = false;
    $('settingsName').value = $('name').value || recall('name', '');
    $('settingsTabs').querySelector('[aria-selected="true"]').focus();
    try {
      text = await readSettings(await root());
      editable = !(await gameRunningElsewhere(navigator.locks));
      say(editable ? SAVED : 'A game is running in another tab of this browser. Close it to change settings here.');
    } catch (e) {
      text = '';
      editable = false;
      say(`This browser's saved settings could not be read (${e.message}), so they cannot be changed here.`);
    }
    fill();
    saves.refresh();
  }

  function close() {
    card.hidden = true;
    home.hidden = false;
    $('openSettings').focus();
  }

  function showTab(id) {
    for (const t of TABS) {
      $('tab-' + t.id).setAttribute('aria-selected', String(t.id === id));
      $('tab-' + t.id).tabIndex = t.id === id ? 0 : -1;
      $('panel-' + t.id).hidden = t.id !== id;
    }
  }

  $('settingsTabs').replaceChildren(...TABS.map((t) => {
    const b = Object.assign(document.createElement('button'), { id: 'tab-' + t.id, textContent: t.label });
    b.setAttribute('role', 'tab');
    b.setAttribute('aria-controls', 'panel-' + t.id);
    b.addEventListener('click', () => showTab(t.id));
    return b;
  }));
  $('settingsTabs').addEventListener('keydown', (e) => {
    const step = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
    if (!step) return;
    const at = TABS.findIndex((t) => $('tab-' + t.id).getAttribute('aria-selected') === 'true');
    const next = TABS[(at + step + TABS.length) % TABS.length].id;
    showTab(next);
    $('tab-' + next).focus();
  });
  $('onlineNotes').replaceChildren(...onlineNotes());
  const tableRows = (rows) => rows.map((cells) => {
    const tr = document.createElement('tr');
    tr.replaceChildren(...cells.map((t) => Object.assign(document.createElement('td'), { textContent: t })));
    return tr;
  });
  $('keyPreset').replaceChildren(new Option('Use a preset…', ''), ...PRESETS.map((p) => new Option(p.label, p.id)));
  $('keyPreset').addEventListener('change', () => {
    const preset = PRESETS.find((p) => p.id === $('keyPreset').value);
    $('keyPreset').value = '';
    if (preset) saveKeys(presetBinds(globalThis.CONTROLS, preset), `Keys set to: ${preset.label}.`);
  });
  $('keyReset').addEventListener('click', () => saveKeys(presetBinds(globalThis.CONTROLS, PRESETS[0]), 'Keys back to the defaults.'));
  $('touchTable').replaceChildren(...tableRows(globalThis.wotTouchRows()));
  // the name is the front panel's own field, remembered by the same key the lobby buttons use
  $('settingsName').addEventListener('input', () => {
    const name = $('settingsName').value.replace(/[^A-Za-z0-9_-]/g, '').slice(0, 16);
    $('settingsName').value = $('name').value = name;
    remember('name', name);
  });
  showTab(TABS[0].id);
  $('openSettings').addEventListener('click', open);
  $('closeSettings').addEventListener('click', close);
  // Esc also closes an open list or leaves a text field: only an Esc that nothing else used closes the card
  card.addEventListener('keydown', (e) => { if (e.key === 'Escape' && !['SELECT', 'INPUT'].includes(e.target.tagName)) close(); });
}
