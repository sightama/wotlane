// ABOUTME: Quick rematch, the page's side: the lobby code and rejoin token (the server's TOKEN line) this tab keeps across the reload
// ABOUTME: after a match, given back once while the server still holds the place, and cleared when the player leaves the lobby.

export const LINGER_MS = 120000;   // native/net/wotm_net_server.hpp ServerConfig::lingerMs: how long the server keeps the place
const KEY = 'wotlane.rejoin';

// `storage`: sessionStorage (one tab's, so two tabs of one browser keep their own places); `now` in ms (Date.now)
export function storeRejoin(storage, code, token, now) {
  try { storage?.setItem(KEY, JSON.stringify({ code, token, at: now })); } catch { /* storage refused: no rematch after the reload */ }
}

// the place to rejoin, once ({ code, token }), or null: none kept, unreadable, or older than the server keeps it
export function takeRejoin(storage, now) {
  let kept = null;
  try {
    kept = JSON.parse(storage?.getItem(KEY) ?? 'null');
    storage?.removeItem(KEY);   // a token works once: the server sends a new one after the rejoin
  } catch {
    return null;
  }
  const fresh = kept && typeof kept.code === 'string' && typeof kept.token === 'string' && now - kept.at < LINGER_MS;
  return fresh ? { code: kept.code, token: kept.token } : null;
}

export function clearRejoin(storage) {
  try { storage?.removeItem(KEY); } catch { /* storage refused: nothing was kept */ }
}
