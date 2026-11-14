// ABOUTME: The browser version's front panel: single player, or an online lobby on the game server (Host shows a 4-letter code,
// ABOUTME: Join takes one). When the owner starts, every member's game loads with the match environment (boot.js wotmStart).
import { LobbyClient, lobbySocketUrl, matchEnv } from './lobby.js';

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

function showLobby(code, isOwner) {
  owner = isOwner;
  $('choose').hidden = true;
  $('inLobby').hidden = false;
  $('code').textContent = code;
  $('ownerOnly').hidden = !isOwner;
  say(isOwner ? 'Send the code to your friends. Start when everyone is in.' : 'Waiting for the owner to start.');
}

function showMembers(m) {
  const list = $('members');
  list.replaceChildren(...m.members.map((p) => {
    const li = document.createElement('li');
    const monster = MONSTERS.find(([id]) => id === p.monster);
    li.textContent = `${p.name} (${monster ? monster[1] : 'monster ' + p.monster})${p.name === m.owner ? ', owner' : ''}`;
    return li;
  }));
}

async function connect(build) {
  const ws = new WebSocket(serverUrl('/lobby'));
  const client = new LobbyClient(ws, build);
  client.onMembers = showMembers;
  client.onSettings = (s) => {
    if (owner) return;
    const stage = STAGES.find(([id]) => String(id) === s.stage);
    $('guestSettings').textContent = `Stage: ${stage ? stage[1] : s.stage}, wins: ${s.wins === '0' ? 'endless' : s.wins}` +
      (s.mods ? `, modifiers: ${s.mods}` : '');
  };
  client.onError = say;
  client.onClose = () => { if (!front.hidden) say('Lost the connection to the game server. Reload the page to try again.'); };
  client.onMatch = (match) => {
    front.hidden = true;
    window.wotmStart(matchEnv(match, serverUrl('/match')), build);
  };
  await client.ready;
  return client;
}

async function hostOrJoin(join) {
  const name = cleanName($('name').value);
  if (!name) return say('Type a name first (letters, digits, - and _).');
  const monster = Number($('monster').value);
  remember('name', name);
  remember('monster', monster);
  const code = $('joinCode').value.trim().toUpperCase();
  if (join && !/^[A-Z2-9]{4}$/.test(code)) return say('The code has 4 characters (letters and digits).');
  say(join ? 'Joining' : 'Creating a lobby');
  try {
    const build = await buildId();
    lobby = await connect(build);
    showLobby(join ? await lobby.join(code, name, monster) : await lobby.create(name, monster), !join);
    if (!join) await sendSettings();
  } catch (e) {
    say(e.message);
    lobby?.close();
    lobby = null;
  }
}

async function sendSettings() {
  const mods = MODIFIERS.filter((m) => $('mod_' + m).checked).join(',');
  remember('stage', $('lobbyStage').value);
  remember('wins', $('wins').value);
  try {
    await lobby.set({ stage: $('lobbyStage').value, wins: $('wins').value, mods });
  } catch (e) {
    say(e.message);
  }
}

function init() {
  if (window.wotmDirect) {
    front.hidden = true;
    buildId().then((b) => window.wotmStart(null, b));
    return;
  }
  $('name').value = recall('name', '');
  fill($('monster'), MONSTERS, recall('monster', 5));
  fill($('lobbyStage'), STAGES, recall('stage', 1));
  fill($('wins'), [[0, 'Endless'], ...[1, 2, 3, 4, 5].map((n) => [n, String(n)])], recall('wins', 2));
  $('single').addEventListener('click', async () => {
    front.hidden = true;
    window.wotmStart(null, await buildId());
  });
  $('host').addEventListener('click', () => hostOrJoin(false));
  $('join').addEventListener('click', () => hostOrJoin(true));
  $('joinCode').addEventListener('keydown', (e) => { if (e.key === 'Enter') hostOrJoin(true); });
  for (const id of ['lobbyStage', 'wins', ...MODIFIERS.map((m) => 'mod_' + m)])
    $(id).addEventListener('change', () => lobby && owner && sendSettings());
  $('monster').addEventListener('change', () => {
    remember('monster', $('monster').value);
    lobby?.monster(Number($('monster').value));
  });
  $('start').addEventListener('click', () => lobby?.start());
  $('leave').addEventListener('click', () => location.reload());
}

init();
