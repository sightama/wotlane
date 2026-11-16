// ABOUTME: The browser version's front panel: single player, or an online lobby on the game server (Host shows a 4-letter code,
// ABOUTME: Join takes one). When the owner starts, every member's game loads with the match environment (boot.js wotmStart).
import { LobbyClient, inviteLink, lastLobbyNotice, lobbyRow, lobbySocketUrl, matchEnv } from './lobby.js';
import { initCreations, addLobbyChoices } from './creations_ui.js';

const MONSTERS = [[1, 'Congar'], [2, 'Robo-47'], [3, 'Togera'], [4, 'Ultra V'], [5, 'Preytor'], [7, 'Kineticlops'], [8, 'Magmo'],
  [9, 'Raptros'], [10, 'Agamo'], [11, 'Zorgulon']];
const STAGES = [[1, 'Midtown Park'], [2, 'Gamblers Gulch'], [3, 'Rosdale Canyon'], [4, 'Metro City'], [5, 'Century Airfield'],
  [6, 'Atomic Island'], [7, 'Baytown'], [8, 'Club Caldera'], [9, 'Tsunopolis'], [10, 'UFO'], [11, 'Capitol'], [13, 'Lake Powell'],
  [14, 'Mini Baytown'], [15, 'Volcano'], [16, 'Tokyo Deathmatch']];
const MODIFIERS = ['jump', 'fast', 'turbo'];

const $ = (id) => document.getElementById(id);
const front = $('front');
const say = (text) => { $('frontMsg').textContent = text; };

function remember(key, value) { try { localStorage.setItem('wotlane.' + key, value); } catch { /* private window: not kept */ } }
function recall(key, fallback) { try { return localStorage.getItem('wotlane.' + key) ?? fallback; } catch { return fallback; } }

function fill(select, items, chosen) {
  for (const [id, label] of items) select.add(new Option(label, String(id), false, String(id) === String(chosen)));
}

// the game server's WebSocket: the page's own host; on a page served from this computer, ?server=ws://host:port can point elsewhere
// (local tests). A link someone sends can't move a public page's players to another server
function serverUrl(path) {
  const local = ['localhost', '127.0.0.1'].includes(location.hostname);
  const override = local && new URLSearchParams(location.search).get('server');
  return override ? override.replace(/\/$/, '') + path : lobbySocketUrl(location, path);
}

async function buildId() {
  try {
    const r = await fetch('version.json', { cache: 'no-store' });
    return (await r.json()).build || 'sem-buildid';
  } catch {
    return 'sem-buildid';   // the sentinel netBuildId uses when it has none (wotm_sync.inc)
  }
}

// names travel in lobby lines: letters, digits, - and _ (the server's cleanName), up to 16
const cleanName = (s) => s.replace(/[^A-Za-z0-9_-]/g, '').slice(0, 16);

let lobby = null;
let owner = false;
let myName = '';
let myMonster = 0;   // the monster the server last accepted

// the owner picks the stage, wins and modifiers and starts; a guest sees them. The server hands the lobby on when the owner leaves
function setOwner(isOwner) {
  owner = isOwner;
  $('ownerOnly').hidden = !isOwner;
  $('guestSettings').hidden = isOwner;
}

function showLobby(code, isOwner) {
  setOwner(isOwner);
  $('choose').hidden = true;
  $('inLobby').hidden = false;
  $('code').textContent = code;
  say(isOwner ? 'Send the code to your friends. Start when everyone is in.' : 'Waiting for the owner to start.');
}

function showMembers(m) {
  if (lobby && !$('inLobby').hidden && m.owner === myName && !owner) {
    const s = lobby.settings;   // the controls start from what the lobby has
    if (s.stage !== undefined) $('lobbyStage').value = s.stage;
    if (s.wins !== undefined) $('wins').value = s.wins;
    for (const mod of MODIFIERS) $('mod_' + mod).checked = (s.mods || '').split(',').includes(mod);
    setOwner(true);
    say('The owner left: you are the owner now. Start when everyone is in.');
  }
  const list = $('members');
  list.replaceChildren(...m.members.map((p) => {
    const li = document.createElement('li');
    const monster = MONSTERS.find(([id]) => id === p.monster);
    const skin = p.skin ? `, skin ${p.skin}` : '';
    li.textContent = `${p.name} (${monster ? monster[1] : 'monster ' + p.monster}${skin})${p.name === m.owner ? ', owner' : ''}`;
    return li;
  }));
}

async function connect(build) {
  const ws = new WebSocket(serverUrl('/lobby'));
  const client = new LobbyClient(ws, build);
  client.onMembers = showMembers;
  client.onSettings = (s) => {
    // a saved map shows by its name when this browser has it (Your creations), else by its id
    const stage = [...$('lobbyStage').options].find((o) => o.value === s.stage);
    $('guestSettings').textContent = `Stage: ${stage ? stage.text : s.stage}, wins: ${s.wins === '0' ? 'endless' : s.wins}` +
      (s.mods ? `, modifiers: ${s.mods}` : '');
  };
  client.onError = (text) => say(friendlyError(text));
  client.onClose = () => { if (!front.hidden) say('Lost the connection to the game server. Reload the page to try again.'); };
  client.onMatch = (match) => {
    try { sessionStorage.setItem('wotlane.lobby', client.code); } catch { /* private window: not kept */ }
    front.hidden = true;
    window.wotmStart(matchEnv(match, serverUrl('/match')), build);
  };
  await client.ready;
  return client;
}

// the open lobbies on the front panel: one connection that asks the server's LIST every few seconds while the panel is showing the
// Host/Join choice (not in a lobby, not while the tab is hidden)
const LIST_EVERY_MS = 5000;
let watcher = null;
let watchTimer = 0;

function showLobbyList(lobbies) {
  $('lobbyList').hidden = false;
  const rows = lobbies.map((l) => {
    const { text, joinable } = lobbyRow(l);
    const li = document.createElement('li');
    const label = document.createElement('span');
    label.textContent = text;
    const button = document.createElement('button');
    button.textContent = 'Join';
    button.disabled = !joinable || $('host').disabled;
    button.addEventListener('click', () => { $('joinCode').value = l.code; hostOrJoin(true); });
    li.append(label, button);
    return li;
  });
  if (!rows.length) rows.push(Object.assign(document.createElement('li'), { textContent: 'No open lobbies. Host one and send the invite link.' }));
  $('lobbies').replaceChildren(...rows);
}

async function refreshLobbies() {
  try {
    if (!watcher) {
      const client = new LobbyClient(new WebSocket(serverUrl('/lobby')), await buildId());
      client.onClose = () => { if (watcher === client) watcher = null; };
      await client.ready;
      if (!watchTimer) return client.close();   // the panel moved on while it connected
      watcher = client;
    }
    const lobbies = await watcher.list();
    if (watcher) showLobbyList(lobbies);
  } catch {   // the server is out of reach or refused this build: the list stays hidden, Host and Join still show why
    watcher?.close();
    watcher = null;
    $('lobbyList').hidden = true;
  }
}

function startWatchingLobbies() {
  stopWatchingLobbies();
  const tick = () => { if (!document.hidden) refreshLobbies(); };
  tick();
  watchTimer = setInterval(tick, LIST_EVERY_MS);
}

function stopWatchingLobbies() {
  clearInterval(watchTimer);
  watchTimer = 0;
  watcher?.close();
  watcher = null;
}

async function hostOrJoin(join) {
  const name = cleanName($('name').value);
  if (!name) return say('Type a name first (letters, digits, - and _).');
  const monster = $('monster').value;   // "<monster>" or "<monster>:<skin id>"
  remember('name', name);
  myName = name;
  myMonster = monster;
  remember('monster', monster);
  const code = $('joinCode').value.trim().toUpperCase();
  if (join && !/^[A-Z2-9]{4}$/.test(code)) return say('The code has 4 characters (letters and digits).');
  say(join ? 'Joining' : 'Creating a lobby');
  stopWatchingLobbies();
  const buttons = ['single', 'host', 'join'].map($);
  for (const b of buttons) b.disabled = true;   // one connection per click
  try {
    const build = await buildId();
    lobby = await connect(build);
    showLobby(join ? await lobby.join(code, name, monster) : await lobby.create(name, monster), !join);
    if (!join) await sendSettings();
  } catch (e) {
    say(friendlyError(e.message));
    lobby?.close();
    lobby = null;
    for (const b of buttons) b.disabled = false;
    startWatchingLobbies();
  }
}

async function sendSettings() {
  const mods = MODIFIERS.filter((m) => $('mod_' + m).checked).join(',');
  remember('stage', $('lobbyStage').value);
  remember('wins', $('wins').value);
  try {
    await lobby.set({ stage: $('lobbyStage').value, wins: $('wins').value, mods });
  } catch (e) {
    say(friendlyError(e.message));
  }
}

// back from a match (Leave the match reloads the page): the code is offered again only while the server still holds that lobby
// (a finished match empties it and the server closes it)
async function offerLastLobby() {
  let code = null;
  try { code = sessionStorage.getItem('wotlane.lobby'); } catch { /* private window */ }
  if (!code) return;
  let probe;
  try {
    const build = await buildId();
    probe = new LobbyClient(new WebSocket(serverUrl('/lobby')), build);
    await probe.ready;
    const notice = lastLobbyNotice(code, await probe.list());
    // the player may have started typing or clicked meanwhile: their message and code stay
    if (notice && !$('frontMsg').textContent && !$('joinCode').value) {
      $('joinCode').value = code;
      say(notice);
    }
  } catch { /* server out of reach: nothing to offer */ }
  probe?.close();
}

// an invite link (/?join=CODE): the code is filled in and, for a player whose name this browser remembers, joined at once
function acceptInvite(code) {
  if (front.hidden || lobby || $('host').disabled) return;   // the player already chose something else
  $('joinCode').value = code;
  if ($('name').value) return hostOrJoin(true);
  say(`You are invited to lobby ${code}. Type a name, then press Join.`);
  $('name').focus();
}

// the link goes to the clipboard; a page that may not use it (or a refusal) shows the link selected to copy by hand
async function copyInvite() {
  const link = inviteLink(location, $('code').textContent);
  try {
    await navigator.clipboard.writeText(link);
    say('Invite link copied. Send it to your friends.');
  } catch {
    const box = $('inviteLink');
    box.value = link;
    box.hidden = false;
    box.select();
    say('Copy the invite link below.');
  }
}

function init() {
  // a browser that can't run the game gets a plain message instead of a page that freezes
  const problem = window.wotmSupportProblem();
  if (problem) {
    say(problem);
    for (const id of ['single', 'host', 'join']) $(id).disabled = true;
    return;
  }
  initCreations(STAGES);
  $('controlsTable').replaceChildren(...controlRows().map((cells) => {
    const tr = document.createElement('tr');
    tr.replaceChildren(...cells.map((text) => Object.assign(document.createElement('td'), { textContent: text })));
    return tr;
  }));
  if (window.wotmDirect) {
    front.hidden = true;
    buildId().then((b) => window.wotmStart(null, b));
    return;
  }
  $('name').value = recall('name', '');
  const invited = window.inviteCode(new URLSearchParams(location.search));
  if (invited) {   // a reload doesn't join again; the address keeps its other parts
    const rest = new URLSearchParams(location.search);
    rest.delete('join');
    history.replaceState(null, '', location.pathname + (rest.size ? '?' + rest : '') + location.hash);
  }
  else offerLastLobby();
  fill($('monster'), MONSTERS, recall('monster', 5));
  fill($('lobbyStage'), STAGES, recall('stage', 1));
  fill($('wins'), [[0, 'Endless'], ...[1, 2, 3, 4, 5].map((n) => [n, String(n)])], recall('wins', 2));
  addLobbyChoices($('monster'), $('lobbyStage')).then(() => {   // a remembered skin or map is picked again once it is listed
    $('monster').value = recall('monster', 5);
    if (!$('monster').value) $('monster').value = '5';
    $('lobbyStage').value = recall('stage', 1);
    if (!$('lobbyStage').value) $('lobbyStage').value = '1';
  }).catch(() => {}).then(() => { if (invited) acceptInvite(invited); });
  $('single').addEventListener('click', async () => {
    stopWatchingLobbies();
    front.hidden = true;
    window.wotmStart(null, await buildId());
  });
  $('host').addEventListener('click', () => hostOrJoin(false));
  $('join').addEventListener('click', () => hostOrJoin(true));
  $('joinCode').addEventListener('keydown', (e) => { if (e.key === 'Enter') hostOrJoin(true); });
  for (const id of ['lobbyStage', 'wins', ...MODIFIERS.map((m) => 'mod_' + m)])
    $(id).addEventListener('change', () => lobby && owner && sendSettings());
  $('monster').addEventListener('change', () => {
    const picked = $('monster').value;
    remember('monster', picked);
    if (!lobby) return;
    lobby.monster(picked).then(() => { myMonster = picked; }, (e) => {
      say(friendlyError(e.message));
      $('monster').value = String(myMonster);   // the server kept the previous one
    });
  });
  $('copyInvite').addEventListener('click', copyInvite);
  $('refreshLobbies').addEventListener('click', refreshLobbies);
  startWatchingLobbies();
  $('start').addEventListener('click', () => lobby?.start().catch((e) => say(friendlyError(e.message))));
  $('leave').addEventListener('click', async () => {
    const leaving = lobby;
    lobby = null;   // a second click has nothing to leave
    // LEFT only comes when the server still has the player in a lobby: a second of waiting is enough
    try { await Promise.race([leaving?.leave(), new Promise((r) => setTimeout(r, 1000))]); } catch { /* the connection is closed next either way */ }
    leaving?.close();
    $('inLobby').hidden = true;
    $('inviteLink').hidden = true;
    $('choose').hidden = false;
    for (const id of ['single', 'host', 'join']) $(id).disabled = false;
    say('');
    startWatchingLobbies();
  });
}

init();
