// ABOUTME: Quick rematch, the page's side: the lobby code and rejoin token (the server's TOKEN line) this tab keeps across the reload
// ABOUTME: after a match, given back once while the server still holds the place, and cleared when the player leaves the lobby.

export const LINGER_MS = 120000;   // native/net/wotm_net_server.hpp ServerConfig::lingerMs: how long the server keeps the place
export const REJOIN_RETRY_MS = 350;
export const REJOIN_WAIT_MS = 3500;
const KEY = 'wotlane.rejoin';

// `storage`: sessionStorage (one tab's, so two tabs of one browser keep their own places); `now` in ms (Date.now); `who`: this tab's
// player ({ name, monster }), because the browser's other tabs may have changed what localStorage remembers
export function storeRejoin(storage, code, token, now, who) {
  try { storage?.setItem(KEY, JSON.stringify({ code, token, at: now, name: who.name, monster: who.monster })); } catch { /* storage refused: no rematch after the reload */ }
}

function parseRejoin(kept, now) {
  // the server's forms (a lobby code, 128 bits in hex, a player name, a monster pick): anything else in this tab's storage is not sent
  const fresh = kept && /^[A-Z2-9]{4}$/.test(kept.code) && /^[0-9a-f]{32}$/.test(kept.token) && now - kept.at < LINGER_MS
    && /^[A-Za-z0-9_-]{1,16}$/.test(kept.name) && typeof kept.monster === 'string' && /^[0-9]+(:[a-z0-9_]+)?$/.test(kept.monster);
  return fresh ? { code: kept.code, token: kept.token, name: kept.name, monster: kept.monster } : null;
}

// reads the stored place without consuming it, or null if none, unreadable, or expired
export function peekRejoin(storage, now) {
  let kept = null;
  try {
    kept = JSON.parse(storage?.getItem(KEY) ?? 'null');
  } catch {
    return null;
  }
  return parseRejoin(kept, now);
}

// the place to rejoin, once ({ code, token, name, monster }), or null: none kept, unreadable, or older than the server keeps it
export function takeRejoin(storage, now) {
  let kept = null;
  try {
    kept = JSON.parse(storage?.getItem(KEY) ?? 'null');
    storage?.removeItem(KEY);   // a token works once: the server sends a new one after the rejoin
  } catch {
    return null;
  }
  return parseRejoin(kept, now);
}

export function isInMatchError(message) {
  return typeof message === 'string' && /lobby is in a match/i.test(message);
}

export function isRejoinTokenError(message) {
  return typeof message === 'string' && /can't rejoin/i.test(message);
}

// retries REJOIN while peers' match sockets are still closing; stops immediately on terminal errors or after waitMs
export async function retryRejoin(attempt, {
  waitMs = REJOIN_WAIT_MS,
  stepMs = REJOIN_RETRY_MS,
  sleep = (ms) => new Promise((r) => setTimeout(r, ms)),
  now = () => Date.now(),
  onWait = () => {},
} = {}) {
  const deadline = now() + waitMs;
  let waited = false;
  for (;;) {
    try {
      return await attempt();
    } catch (e) {
      if (!isInMatchError(e?.message) || now() >= deadline) throw e;
      if (!waited) {
        waited = true;
        onWait();
      }
      const left = deadline - now();
      await sleep(Math.min(stepMs, Math.max(0, left)));
    }
  }
}

// the page is going away (pagehide): the server keeps the place for LINGER_MS from the moment the lobby connection drops, which is now;
// `who` is the player as it is now (the monster changes in the lobby)
export function leavingPage(storage, now, who) {
  try {
    const kept = JSON.parse(storage?.getItem(KEY) ?? 'null');
    if (kept) storage.setItem(KEY, JSON.stringify({ ...kept, at: now, name: who.name, monster: who.monster }));
  } catch { /* storage refused or unreadable: nothing to restart */ }
}

export function clearRejoin(storage) {
  try { storage?.removeItem(KEY); } catch { /* storage refused: nothing was kept */ }
}
