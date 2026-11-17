// ABOUTME: The browser version's lobby client: wotm_server's lobby lines over a WebSocket (/lobby), and the game environment for a
// ABOUTME: MATCH line (the same mapping as tools/net_lobby.py's match_env, with the match WebSocket as WOTM_NET).

export const PROTOCOL = 5;   // native/net/wotm_net_proto.hpp kProtocol (5: map ids as the stage, <monster>:<skin> members)

// one lobby line: the command, its words, and its key=value words as fields (MATCH, SETTINGS); `rest` is everything after the command
export function parseLine(line) {
  const text = String(line).replace(/\r?\n?$/, '');
  const space = text.indexOf(' ');
  const cmd = space < 0 ? text : text.slice(0, space);
  const rest = space < 0 ? '' : text.slice(space + 1);
  const words = rest ? rest.split(' ') : [];
  const fields = {};
  for (const w of words) {
    const eq = w.indexOf('=');
    if (eq > 0) fields[w.slice(0, eq)] = w.slice(eq + 1);
  }
  return { cmd, rest, words, fields };
}

// MEMBERS <code> <owner or -> <name:monster[:skin],... or -> (names have no ':')
export function parseMembers(parsed) {
  const [code = '', owner = '-', list = '-'] = parsed.words;
  const members = list === '-' ? [] : list.split(',').map((m) => {
    const [name, monster, skin = ''] = m.split(':');
    return { name, monster: Number(monster), skin };
  });
  return { code, owner: owner === '-' ? '' : owner, members };
}

// the game's environment for a MATCH line's fields; `matchUrl` is the match WebSocket (wss://<host>/match)
export function matchEnv(match, matchUrl) {
  const you = Number(match.you);
  return {
    WOTM_NET: matchUrl,
    WOTM_NETTOKEN: match.token,
    WOTM_NETNAME: match.names.split(',')[you],
    WOTM_MATCH: `${match.monsters},${match.stage}`,
    WOTM_PLAYERS: String(Number(match.players)),
    WOTM_FIXEDSTEP: match.step,
    WOTM_SEED: match.seed,
    WOTM_KILLTARGET: match.wins ?? '0',
    WOTM_MODIFIERS: match.mods ?? '',
    WOTM_LOCALPLAYER: String(you + 1),
  };
}

// the server's WebSocket on the page's own host (the web server in front passes /lobby and /match to wotm_server)
export function lobbySocketUrl(location, path) {
  return (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + path;
}

// LOBBY <code> <waiting|playing> <n>/<max> <open|locked> <lobby> <owner> <names,... or ->
function parseLobby(parsed) {
  const [code, state, fill, lock, name, host, names = '-'] = parsed.words;
  const [count, max] = fill.split('/').map(Number);
  return { code, playing: state === 'playing', count, max, locked: lock === 'locked', name, host: host === '-' ? '' : host,
    names: names === '-' ? [] : names.split(',') };
}

// one line of the open-lobby list: what to show, and whether Join can work (this page can't send a password)
export function lobbyRow(l) {
  const state = l.playing ? 'in a match' : l.count >= l.max ? 'full' : 'waiting';
  const text = `${l.host || l.name}: ${l.count}/${l.max} (${l.names.join(', ')}), ${state}${l.locked ? ', needs a password' : ''}`;
  return { text, joinable: !l.playing && l.count < l.max && !l.locked };
}

// PINGS <code> <name:ms|name:->,...: each member's round trip to the server in ms (null: not measured)
export function parsePings(parsed) {
  const pings = {};
  for (const entry of (parsed.words[1] ?? '').split(',')) {
    const [name, ms] = entry.split(':');
    if (name) pings[name] = ms === undefined || ms === '-' ? null : Number(ms);
  }
  return pings;
}

// CHAT <code> <name> <text...>: the text keeps its spaces
export function parseChat(parsed) {
  const [, name] = parsed.words;
  const text = parsed.words.slice(2).join(' ');
  return { name, text };
}

// the address a friend opens to land in the lobby `code`: this page with only the code
export function inviteLink(location, code) {
  return `${location.origin}${location.pathname}?join=${code}`;
}

// A phone or tablet (touch is the primary pointer) can't compute 60 game frames a second, and an online match counts every frame as
// 1/60 s of game time, so at 60 Hz it would play everyone's match in slow motion: it asks for the game's 30 Hz mode (HELLO hz=30).
// `win`: the page's window
export function halfRateDevice(win) {
  return !!win.matchMedia?.('(pointer: coarse)').matches;
}

// One lobby connection. Requests wait for their reply (or the ERROR that refuses them), in order; MEMBERS, SETTINGS and MATCH can
// arrive at any time and go to onMembers, onSettings and onMatch. `socket` is a WebSocket (or anything with send, close and the
// on* handlers). `halfRate`: this device's matches run at 30 Hz (halfRateDevice).
export class LobbyClient {
  constructor(socket, build, { halfRate = false } = {}) {
    this.socket = socket;
    this.settings = {};
    this.code = '';
    this.onMembers = null;
    this.onSettings = null;
    this.onMatch = null;
    this.onPings = null;
    this.onChat = null;
    this.onClose = null;
    this.onError = null;   // an ERROR no request waits for
    // requests waiting for their replies, oldest first: {replies (commands still expected, in order), got (the first reply),
    // resolve, reject}. An ERROR refuses the oldest one
    this.waiting = [];
    socket.onmessage = (e) => this.receive(String(e.data));
    socket.onclose = () => this.closed();
    socket.onerror = () => {};
    this.ready = this.expect(['WELCOME']);
    this.ready.catch(() => this.close());   // a refused HELLO leaves nothing to keep open
    socket.onopen = () => socket.send(`HELLO ${PROTOCOL} ${build}${halfRate ? ' hz=30' : ''}`);
  }

  expect(replies) {
    return new Promise((resolve, reject) => this.waiting.push({ replies: [...replies], got: null, resolve, reject }));
  }

  receive(line) {
    const p = parseLine(line);
    if (p.cmd === 'ERROR') {
      const w = this.waiting.shift();
      if (w) w.reject(new Error(p.rest));
      else this.onError?.(p.rest);
      return;
    }
    if (p.cmd === 'SETTINGS') {
      this.settings = { stage: p.fields.stage, wins: p.fields.wins, mods: p.fields.mods ?? '' };
      this.onSettings?.(this.settings);
    } else if (p.cmd === 'MEMBERS') {
      this.onMembers?.(parseMembers(p));
    } else if (p.cmd === 'PINGS') {
      this.onPings?.(parsePings(p));
    } else if (p.cmd === 'CHAT') {
      this.onChat?.(parseChat(p));
    } else if (p.cmd === 'MATCH') {
      this.onMatch?.(p.fields);
    } else if (p.cmd === 'LOBBY') {
      this.lobbies?.push(parseLobby(p));
    } else if (p.cmd === 'JOINED') {
      this.code = p.words[0];
    }
    const w = this.waiting[0];
    if (w && p.cmd === w.replies[0]) {
      w.replies.shift();
      w.got ??= p;
      if (!w.replies.length) {
        this.waiting.shift();
        w.resolve(w.got);
      }
    }
  }

  closed() {
    for (const w of this.waiting.splice(0)) w.reject(new Error('lost the connection to the game server'));
    this.onClose?.();
  }

  // sends a line and resolves with the first of `replies` once all of them arrived, in order
  request(line, replies) {
    const done = this.expect(replies);
    this.socket.send(line);
    return done;
  }

  // a new lobby named after its owner; resolves with its 4-letter code
  async create(name, monster) {
    const created = await this.request(`CREATE ${name} ${name} ${monster}`, ['CREATED', 'JOINED', 'SETTINGS']);
    return created.words[0];
  }

  // resolves with the lobby code
  async join(code, name, monster) {
    const joined = await this.request(`JOIN ${code} ${name} ${monster}`, ['JOINED', 'SETTINGS']);
    return joined.words[0];
  }

  // owner only: stage, wins and modifiers for the next match; resolves with what the server stored
  async set({ stage, wins, mods }) {
    const parts = [];
    if (stage !== undefined) parts.push(`stage=${stage}`);
    if (wins !== undefined) parts.push(`wins=${wins}`);
    if (mods !== undefined) parts.push(`mods=${mods}`);
    await this.request('SET ' + parts.join(' '), ['SETTINGS']);
    return this.settings;
  }

  // the server answers a monster change with the members list, and START with the match (or an ERROR that refuses either)
  monster(id) { return this.request(`MONSTER ${id}`, ['MEMBERS']); }

  start() { return this.request('START', ['MATCH']); }

  // a line of lobby chat, before the match; the server sends it back to everyone, this member included (or an ERROR: too fast)
  chat(text) { this.socket.send(`CHAT ${text}`); }

  // leaves the lobby and keeps the connection; resolves with the code of the lobby that was left
  async leave() {
    const left = await this.request('LEAVE', ['LEFT']);
    this.code = '';
    return left.words[0];
  }

  // the lobbies the server holds: [{code, playing}]
  async list() {
    this.lobbies = [];
    await this.request('LIST', ['END']);
    return this.lobbies;
  }

  // the page's own close: it is not a lost connection, so onClose stays quiet
  close() {
    this.onClose = null;
    this.socket.close();
  }
}

// what the front panel says about the lobby of the last match; a finished match empties the lobby, so it may be gone
export function lastLobbyNotice(code, lobbies) {
  const found = lobbies.find((l) => l.code === code);
  if (!found) return '';
  return found.playing ? `Your last lobby ${code} is in a match: Join works once it ends.`
    : `Your last lobby was ${code}: Join puts you back in it.`;
}
