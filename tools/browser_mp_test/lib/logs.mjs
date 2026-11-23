// ABOUTME: Readers of the game's page log (window.wotmLog: "<seconds>s <line>") and of wotm_server's log, for the browser multiplayer
// ABOUTME: checks: match start and stop, per-frame state hashes, frame rate, waits, round ends, pad input and DESYNC lines.

const NUMBER = (s) => Number(s);

// the line Player.pollLog puts in a player's log where its page loaded again (a reload, or the page's own after a match)
export const RELOADED = '--- page reloaded ---';

// the log since the page last loaded: the readers below then see this page's match, not an earlier one
export function sinceReload(log) {
  const at = log.lastIndexOf(RELOADED + '\n');
  return at < 0 ? log : log.slice(at + RELOADED.length + 1);
}

// {player, of, delay} from "[net] start: player 2 of 4, delay 2 frames", or null before the match starts
export function netStart(log) {
  const m = /\[net\] start: player (\d+) of (\d+), delay (\d+) frames/.exec(log);
  return m ? { player: NUMBER(m[1]), of: NUMBER(m[2]), delay: NUMBER(m[3]) } : null;
}

// the reason from "[net] stopped: <reason>", or null while the match runs
export function netStopped(log) {
  const m = /\[net\] stopped: ([^\r\n]*)/.exec(log);
  return m ? m[1] : null;
}

// Map frame -> hash from the "[net] hash of frame N: <hex>" lines (the game prints one in 16 of its state hashes)
export function hashes(log) {
  const out = new Map();
  for (const m of log.matchAll(/\[net\] hash of frame (\d+): ([0-9a-f]+)/g)) out.set(NUMBER(m[1]), m[2]);
  return out;
}

// {common, mismatches}: how many frames every map has, and for each of them where the hashes differ
export function compareHashes(maps) {
  const common = [...maps[0].keys()].filter((f) => maps.every((m) => m.has(f))).sort((a, b) => a - b);
  const mismatches = [];
  for (const frame of common) {
    const all = maps.map((m) => m.get(frame));
    if (new Set(all).size > 1) mismatches.push({ frame, hashes: all });
  }
  return { common: common.length, mismatches };
}

// the game's frames per second: the slope of the hash lines' frame index against their time stamps, null with fewer than two lines
export function gameFps(log) {
  const points = [...log.matchAll(/(\d+(?:\.\d+)?)s \[net\] hash of frame (\d+):/g)].map((m) => [NUMBER(m[1]), NUMBER(m[2])]);
  if (points.length < 2) return null;
  const [t0, f0] = points[0];
  const [t1, f1] = points[points.length - 1];
  return t1 > t0 ? (f1 - f0) / (t1 - t0) : null;
}

// {frame, waits, ms} from the last "[net] frame N: W waits for the network, M ms in total", or null
export function lastWaits(log) {
  const all = [...log.matchAll(/\[net\] frame (\d+): (\d+) waits for the network, (\d+) ms in total/g)];
  if (!all.length) return null;
  const m = all[all.length - 1];
  return { frame: NUMBER(m[1]), waits: NUMBER(m[2]), ms: NUMBER(m[3]) };
}

// every "[match] round N ended" line counts one round
export function roundsEnded(log) {
  return [...log.matchAll(/\[match\] round \d+ ended/g)].length;
}

export function hasLine(log, re) {
  return re.test(log);
}

// the server's DESYNC lines
export function desyncs(serverLog) {
  return serverLog.split('\n').filter((l) => l.includes('DESYNC'));
}

// the server's lines about one lobby code
export function serverLobbyLines(serverLog, code) {
  return serverLog.split('\n').filter((l) => l.includes(`lobby ${code}:`));
}

// Map player (port + 2 * slot, from 0) -> the buttons (a scePadRead bit mask, 1 = held) it showed held at any time in a WOTM_PADLOG.
// The first data word is little endian: status and pad mode in its low half, the buttons (0 = held) in its high half, so
// "ffff7300" is no button held. Players that never held one are left out.
export function padButtons(log) {
  const held = new Map();
  for (const m of log.matchAll(/scePadRead\(port (\d+), slot (\d+),[^\n]*-> -?\d+ data ([0-9a-f]{8})/g)) {
    const buttons = ~(parseInt(m[3], 16) >>> 16) & 0xffff;
    if (!buttons) continue;
    const player = NUMBER(m[1]) + 2 * NUMBER(m[2]);
    held.set(player, (held.get(player) ?? 0) | buttons);
  }
  return held;
}
