// ABOUTME: The browser version's front panel: single player, or an online lobby on the game server (Host shows a 4-letter code,
// ABOUTME: Join takes one). When the owner starts, every member's game loads with the match environment (boot.js wotmStart).
import { LobbyClient, halfRateDevice, phoneDevice, inviteLink, lastLobbyNotice, lobbyRow, lobbySocketUrl, matchEnv } from './lobby.js';
import { initCreations, addLobbyChoices } from './creations_ui.js';
import { initSettings } from './settings_ui.js';
import { phoneMatchDrawing, readSettings, settingsSaved } from './settings.js';
import { playStartupMovies } from './movies.js';
import { DiagReporter, FrameWatch, LongTaskWatch, TaskLagWatch, deviceFields, diagLine, flushSocket, installAudioWatch, joinFields, readGame, reportFields } from './diag.js';
import { deliverReport, initReport } from './report_ui.js';
import { RateMemory, healthLine, rememberedHealth } from './rate.js';
import { lobbyWarnings, memberStatus } from './lobby_status.js';
import { BRAVE_NOTICE, detectBrave } from './brave.js';

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
let members = { owner: '', members: [] };
let pings = {};
let healths = {};   // each member's device health (HEALTH), by name
let braves = new Set();   // the members on Brave (BRAVE)
let onBrave = false;   // this page's browser is Brave
let myMonster = 0;   // the monster the server last accepted

// the owner picks the stage, wins and modifiers and starts; a guest sees them. The server hands the lobby on when the owner leaves
function setOwner(isOwner) {
  owner = isOwner;
  $('ownerOnly').hidden = !isOwner;
  $('guestSettings').hidden = isOwner;
  showMemberList();   // the owner's buttons come and go with the role
}

function showLobby(code, isOwner) {
  setOwner(isOwner);
  $('choose').hidden = true;
  $('inLobby').hidden = false;
  $('code').textContent = code;
  say(isOwner ? 'Send the code to your friends. Start when everyone is in.' : 'Waiting for the owner to start.');
}

function showMembers(m) {
  if (lobby && !$('inLobby').hidden && m.owner !== myName && owner) {
    setOwner(false);
    say(`${m.owner} is the owner now.`);
  }
  if (lobby && !$('inLobby').hidden && m.owner === myName && !owner) {
    const s = lobby.settings;   // the controls start from what the lobby has
    if (s.stage !== undefined) $('lobbyStage').value = s.stage;
    if (s.wins !== undefined) $('wins').value = s.wins;
    for (const mod of MODIFIERS) $('mod_' + mod).checked = (s.mods || '').split(',').includes(mod);
    setOwner(true);
    say('You are the owner now. Start when everyone is in.');
  }
  members = m;
  braves = new Set([...braves].filter((name) => m.members.some((p) => p.name === name)));   // a member who left is not Brave under a name someone else takes
  showMemberList();
}

// the lobby's chat: the last CHAT_LINES lines, the newest in view
const CHAT_LINES = 50;
function showChat({ name, text }) {
  const log = $('chatLog');
  const li = document.createElement('li');
  li.textContent = `${name}: ${text}`;
  log.append(li);
  while (log.children.length > CHAT_LINES) log.firstChild.remove();
  log.scrollTop = log.scrollHeight;
}

function sendChat() {
  const text = $('chatText').value.trim();
  if (!text || !lobby) return;
  lobby.chat(text);
  $('chatText').value = '';
}

// A button that asks once more before it acts: the first click turns it into "Sure?" for a few seconds
// (the member rows are rebuilt when the list changes, so the armed button is kept by `key`, not by the element)
let armed = { key: '', timer: 0 };
function confirmButton(key, label, act) {
  const button = document.createElement('button');
  const show = () => { button.textContent = armed.key === key ? 'Sure?' : label; };
  show();
  button.addEventListener('click', () => {
    clearTimeout(armed.timer);
    if (armed.key === key) {
      armed = { key: '', timer: 0 };
      show();
      return act();
    }
    armed = { key, timer: setTimeout(() => { armed = { key: '', timer: 0 }; for (const b of document.querySelectorAll('#members button')) b.dispatchEvent(new Event('disarm')); }, 3000) };
    show();
  });
  button.addEventListener('disarm', show);
  return button;
}

// the owner's controls on another member's row: hand the lobby over, or remove them (they can't join it again for a minute)
function ownerControls(name) {
  const handOver = confirmButton('owner ' + name, 'Make owner', () => lobby?.handOver(name).catch((e) => say(friendlyError(e.message))));
  const remove = confirmButton('kick ' + name, 'Remove', () => lobby?.kick(name).catch((e) => say(friendlyError(e.message))));
  return [handOver, remove];
}

// The member rows are built when the members change; each member's ping (PINGS, every second while the lobby waits) and device health
// (HEALTH) then change the chips in place, so a button is never replaced under a click
function showMemberList() {
  $('members').replaceChildren(...members.members.map((p) => {
    const li = document.createElement('li');
    li.dataset.name = p.name;
    const monster = MONSTERS.find(([id]) => id === p.monster);
    const skin = p.skin ? `, skin ${p.skin}` : '';
    const who = Object.assign(document.createElement('span'), { className: 'who' });
    who.textContent = `${p.name} (${monster ? monster[1] : 'monster ' + p.monster}${skin})${p.name === members.owner ? ', owner' : ''}`;
    const chips = ['ping', 'device', 'browser'].map((kind) => Object.assign(document.createElement('span'), { className: `chip ${kind}` }));
    li.append(who, ...chips, ...(owner && p.name !== myName ? ownerControls(p.name) : []));
    return li;
  }));
  showMemberStatus();
}

function showMemberStatus() {
  const names = members.members.map((p) => p.name);
  for (const li of $('members').children) {
    const status = memberStatus(li.dataset.name, pings, healths, braves);
    for (const kind of ['ping', 'device', 'browser']) {
      const chip = li.querySelector('.' + kind);
      const shown = kind !== 'browser' || (owner && li.dataset.name !== myName);   // the Brave hint is for the owner, about the others
      chip.textContent = shown ? status[kind].text : '';
      chip.title = shown ? (status[kind].title ?? '') : '';
      chip.className = `chip ${kind} ${shown ? status[kind].level : 'none'}`;
    }
  }
  $('lobbyWarnings').replaceChildren(...lobbyWarnings(names, pings, healths).map((text) => Object.assign(document.createElement('li'), { textContent: text })));
}

// Diagnostics: while a match runs, the lobby connection (which stays open) carries a DIAG report every 5 s and a last one when the match
// ends (web/diag.js). It reads counters; it never touches the game. The audio watch is installed at load, before the game makes its
// audio context
const audioWatch = installAudioWatch(window);
const LAST_DIAG = 'wotlane.lastdiag';
let diagReporter = null;

// runs fn as a new task on this thread (a MessageChannel message: no timer clamping), for the diag's task lag
const taskChannel = new MessageChannel();
const taskQueue = [];
taskChannel.port1.onmessage = () => taskQueue.shift()?.();
function postTask(fn) {
  taskQueue.push(fn);
  taskChannel.port2.postMessage(0);
}

function startDiag(client, socket, match, build) {
  // each window also tells this browser whether its device holds 60 game frames a second (web/rate.js: if not, its next matches ask
  // for 30 Hz)
  const rate = new RateMemory({ get: (k) => localStorage.getItem(k), set: (k, v) => localStorage.setItem(k, v) });
  diagReporter = new DiagReporter({
    send: (line) => { try { client.diag(line); } catch { /* the connection is gone: the report is lost, the match goes on */ } },
    base: [['build', build], ['lobby', match.lobby], ['name', myName], ['hz', 60 / Number(match.step)], ['players', match.players]],
    now: () => performance.now(),
    setTimer: (fn, ms) => setInterval(fn, ms),
    clearTimer: (id) => clearInterval(id),
    store: { set: (v) => {
      rate.add(v.fields);
      try { sessionStorage.setItem(LAST_DIAG, JSON.stringify(v)); } catch { /* private window: not kept */ }
    } },
    frames: new FrameWatch({ raf: (cb) => requestAnimationFrame(cb), doc: document }),
    longTasks: new LongTaskWatch(window.PerformanceObserver),
    audio: audioWatch,
    game: () => readGame(window),
    device: () => deviceFields(window),
    isHidden: () => document.hidden,
    hasFocus: () => document.hasFocus(),
    lag: new TaskLagWatch({ now: () => performance.now(), post: postTask }),
  });
  diagReporter.begin();
  // the last report goes out as the match ends: boot.js calls this before it reloads the page (the game quit, or Leave the match)
  window.wotmDiagFinish = async () => {
    try {
      diagReporter?.finish();
      await flushSocket(socket);
    } catch { /* a report that can't be sent is not worth holding the page */ }
  };
  window.addEventListener('pagehide', () => diagReporter?.finish());
}

function lastDiag() {
  try { return JSON.parse(sessionStorage.getItem(LAST_DIAG)); } catch { return null; }
}

// the hand-sent report: the last match's numbers, this device, the lobby and the name, and the player's note
async function sendReport(note) {
  const build = await buildId();
  const line = diagLine(reportFields({ saved: lastDiag(), device: deviceFields(window), lobby: lobby?.code ?? '', name: cleanName($('name').value), build }), note);
  return deliverReport({
    line, lobby,
    connect: async () => {
      const client = new LobbyClient(new WebSocket(serverUrl('/lobby')), build);
      await client.ready;
      return client;
    },
  });
}

// a phone's drawing settings for an online match (settings.js phoneMatchDrawing); nothing on a computer, or when the file can't be read
async function phoneDrawing() {
  if (!phoneDevice(window)) return {};
  try {
    return phoneMatchDrawing(await readSettings(await navigator.storage.getDirectory()));
  } catch {
    return {};
  }
}

async function connect(build) {
  const ws = new WebSocket(serverUrl('/lobby'));
  const client = new LobbyClient(ws, build, { halfRate: halfRateDevice(window) });
  client.onMembers = showMembers;
  client.onChat = showChat;
  client.onPings = (p) => { pings = p; showMemberStatus(); };
  client.onHealth = (h) => { healths = h; showMemberStatus(); };
  client.onBrave = (names) => { braves = new Set(names); showMemberStatus(); };
  client.onKicked = ({ seconds }) => backToChoice(`The owner removed you from the lobby. You can join it again in ${seconds} seconds.`);
  client.onSettings = (s) => {
    // a saved map shows by its name when this browser has it (Your creations), else by its id
    const stage = [...$('lobbyStage').options].find((o) => o.value === s.stage);
    $('guestSettings').textContent = `Stage: ${stage ? stage.text : s.stage}, wins: ${s.wins === '0' ? 'endless' : s.wins}` +
      (s.mods ? `, modifiers: ${s.mods}` : '');
  };
  client.onError = (text) => say(friendlyError(text));
  client.onClose = () => { if (!front.hidden) say('Lost the connection to the game server. Reload the page to try again.'); };
  client.onMatch = async (match) => {
    try { sessionStorage.setItem('wotlane.lobby', client.code); } catch { /* private window: not kept */ }
    front.hidden = true;
    await settingsSaved();   // a change made a moment ago is in the file before the game reads it
    startDiag(client, ws, match, build);
    window.wotmStart({ ...matchEnv(match, serverUrl('/match')), ...(await phoneDrawing()) }, build);
  };
  await client.ready;
  return client;
}

// the open lobbies on the front panel: one connection that asks the server's LIST every few seconds while the panel is showing the
// Host/Join choice (not in a lobby, not while the tab is hidden)
const LIST_EVERY_MS = 5000;
let watcher = null;
let watchTimer = 0;
let watching = 0;
let refreshingFor = 0;

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

// `watching` counts the starts and stops of the watching: a refresh that began before the latest stop or start does nothing with its answer
async function refreshLobbies() {
  const mine = watching;
  if (refreshingFor === mine) return;   // one LIST at a time
  refreshingFor = mine;
  let client = null;
  try {
    if (!watcher) {
      client = new LobbyClient(new WebSocket(serverUrl('/lobby')), await buildId());
      client.onClose = () => { if (watcher === client) watcher = null; };
      await client.ready;
      if (mine !== watching) return client.close();   // the panel moved on while it connected
      watcher = client;
    }
    const lobbies = await watcher.list();
    if (mine === watching) showLobbyList(lobbies);
  } catch {   // the server is out of reach or refused this build: the list stays hidden, Host and Join still show why
    if (mine !== watching) return;
    (client ?? watcher)?.close();
    watcher = null;
    $('lobbyList').hidden = true;
  } finally {
    if (refreshingFor === mine) refreshingFor = 0;
  }
}

function startWatchingLobbies() {
  stopWatchingLobbies();
  watching++;
  const tick = () => { if (!document.hidden) refreshLobbies(); };
  tick();
  watchTimer = setInterval(tick, LIST_EVERY_MS);
}

function stopWatchingLobbies() {
  watching++;
  $('lobbyList').hidden = true;   // no Join while a join is under way
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
  pings = {};
  healths = {};
  braves = new Set();
  $('chatLog').replaceChildren();
  say(join ? 'Joining' : 'Creating a lobby');
  stopWatchingLobbies();
  const buttons = ['single', 'host', 'join'].map($);
  for (const b of buttons) b.disabled = true;   // one connection per click
  try {
    const build = await buildId();
    lobby = await connect(build);
    showLobby(join ? await lobby.join(code, name, monster) : await lobby.create(name, monster), !join);
    const health = rememberedHealth(localStorage);
    if (onBrave && lobby.code) lobby.brave();   // the owner sees which member's browser is Brave
    if (health) lobby.health(healthLine(health));   // what the last match showed of this device, for the other members
    lobby.diag(diagLine(joinFields([['build', build], ['lobby', lobby.code], ['name', name]], deviceFields(window))));
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

// the Host/Join choice again, with `text` on the panel: after Leave, or when the owner removed this player (the server has taken them
// out of the lobby already)
function backToChoice(text) {
  lobby?.close();
  lobby = null;
  $('inLobby').hidden = true;
  $('inviteLink').hidden = true;
  $('choose').hidden = false;
  for (const id of ['single', 'host', 'join']) $(id).disabled = false;
  say(text);
  startWatchingLobbies();
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
    for (const id of ['single', 'host', 'join', 'openSettings']) $(id).disabled = true;
    return;
  }
  initCreations(STAGES);
  initSettings({ remember, recall });
  initReport({ send: sendReport }).available(!!lastDiag());   // after a match the page has reloaded: its numbers are what the report holds
  if (window.wotmDirect) {
    front.hidden = true;
    buildId().then((b) => window.wotmStart(null, b));
    return;
  }
  $('name').value = recall('name', '');
  detectBrave(navigator).then((brave) => {   // this player's own screen: the front panel carries the notice until they leave it
    onBrave = brave;
    $('braveNote').hidden = !brave;
    $('braveNote').textContent = brave ? BRAVE_NOTICE : '';
    if (brave && lobby?.code) lobby.brave();   // the lobby was joined before the answer came
  });
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
    await settingsSaved();
    // the boot movies play over the game while it loads (not awaited); in a match they never play
    playStartupMovies({ mode: recall('movies', 'skippable'), overlay: $('movie'), video: $('movieVideo'), hint: $('movieHint'), log: (t) => window.wotmLog?.push(t) });
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
  $('chatSend').addEventListener('click', sendChat);
  $('chatText').addEventListener('keydown', (e) => { if (e.key === 'Enter') sendChat(); });
  $('refreshLobbies').addEventListener('click', refreshLobbies);
  startWatchingLobbies();
  $('start').addEventListener('click', () => lobby?.start().catch((e) => say(friendlyError(e.message))));
  $('leave').addEventListener('click', async () => {
    const leaving = lobby;
    lobby = null;   // a second click has nothing to leave
    // LEFT only comes when the server still has the player in a lobby: a second of waiting is enough
    try { await Promise.race([leaving?.leave(), new Promise((r) => setTimeout(r, 1000))]); } catch { /* the connection is closed next either way */ }
    leaving?.close();
    backToChoice('');
  });
}

init();
