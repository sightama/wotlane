// ABOUTME: Prefetches the disc chunks an online match will load (its stage, each member's monster and costume: web/match_chunks.json) into
// ABOUTME: the disc cache while the lobby waits and as the game boots, so no player's fight load waits on the network. Page script.

// The costume each member gets, as the game hands them out (native/net/wotm_monsters.hpp fits): costumes asked for first (the first to ask
// gets it), then each automatic one takes the lowest free costume of the same monster from its first one (Goliath's 0 is a prototype),
// repeating past four. A creation's skin (not a number) has no disc costume: null
function costumesFor(members) {
  const asked = members.map((m) => (/^\d+$/.test(m.skin ?? '') ? Number(m.skin) : -1));
  const given = members.map(() => -1);
  const taken = (i, c) => members.some((m, j) => j !== i && given[j] === c && m.monster === members[i].monster);
  members.forEach((m, i) => { if (asked[i] >= 0 && !taken(i, asked[i])) given[i] = asked[i]; });
  members.forEach((m, i) => {
    for (let c = m.monster === 13 ? 1 : 0; given[i] < 0; ++c)
      if (c > 3 || !taken(i, c)) given[i] = c & 3;
  });
  return members.map((m, i) => (asked[i] < 0 && m.skin ? null : given[i]));
}

// The chunks of game.iso a match on `stage` (a stage number, or a map id: no stage list) with `members` ({monster, skin}) loads, sorted
function matchChunkList(table, stage, members) {
  const out = new Set(table.stages?.[stage] ?? []);
  costumesFor(members).forEach((costume, i) => {
    const m = table.monsters?.[members[i].monster];
    if (!m) return;
    for (const c of m.all) out.add(c);
    for (const c of (costume === null ? [] : m.costumes[costume] ?? [])) out.add(c);
  });
  return [...out].sort((a, b) => a - b);
}

// WOTM_MATCH ("<monster>[:<skin>],...,<stage>", web/lobby.js matchEnv) as {stage, members}; null when there is none
function matchOfEnv(value) {
  const parts = String(value ?? '').split(',').filter(Boolean);
  if (parts.length < 2) return null;
  const stage = parts.pop();
  return { stage, members: parts.map((p) => { const [monster, skin = ''] = p.split(':'); return { monster: Number(monster), skin }; }) };
}

// Fetches a lobby's match chunks once its choices have held still for `delayMs` (or the `waitMs` a call gives), `CONCURRENCY` chunks
// at a time (the lobby's WebSocket is never starved); a new choice stops the fetch of the one it replaces (chunks already in stay
// cached), stop() ends it all.
// `prefetch(chunks, opts)` is disc_cache.js prefetch on game.iso; `table()` gives match_chunks.json (or throws: nothing is fetched)
const CONCURRENCY = 4;
function createMatchWarmer({ prefetch, table, delayMs = 1000, setTimer = setTimeout, clearTimer = clearTimeout }) {
  let gen = 0, timer = 0, key = '', stopped = false;
  return {
    want(stage, members, waitMs = delayMs) {
      const next = JSON.stringify([stage, members.map((m) => [m.monster, m.skin ?? ''])]);
      if (stopped || next === key) return;
      key = next;
      const mine = ++gen;
      clearTimer(timer);
      timer = setTimer(async () => {
        let list;
        try { list = matchChunkList(await table(), stage, members); } catch { return; }
        if (mine === gen && !stopped && list.length) prefetch(list, { maxRun: 1, concurrency: CONCURRENCY, stop: () => stopped || mine !== gen });
      }, waitMs);
    },
    stop() {
      stopped = true;
      clearTimer(timer);
    },
  };
}

// Whether `chunks` 1 MiB chunks fetched now would be kept: a Cache API that works (a private window's can throw) and room under 90% of
// the storage quota (disc_cache.js stores nothing past it). Otherwise a prefetch would only download what the game then downloads again
async function canKeepChunks({ caches, estimate }, chunks) {
  try {
    if (!caches) return false;
    await caches.keys();
    if (!estimate) return true;
    const { usage = 0, quota = 0 } = await estimate();
    return quota > 0 && usage + chunks * (1 << 20) <= quota * 0.9;
  } catch {
    return false;
  }
}

if (typeof module !== 'undefined' && module.exports) module.exports = { costumesFor, matchChunkList, matchOfEnv, createMatchWarmer, canKeepChunks };
if (typeof window !== 'undefined') {
  // the page's one warmer: the lobby (front.js) tells it each choice, the game's boot (boot.js wotmStart) the match it starts
  let table = null;
  const warmer = createMatchWarmer({
    table: () => (table ??= fetch('match_chunks.json').then((r) => { if (!r.ok) throw new Error(`HTTP ${r.status}`); return r.json(); }).catch((e) => { table = null; throw e; })),
    prefetch: async (chunks, opts) => {
      const storage = navigator.storage;
      if (await canKeepChunks({ caches: window.caches, estimate: storage?.estimate ? () => storage.estimate() : null }, chunks.length))
        await window.wotmDiscCache?.prefetch(fetch, 'data/game.iso', chunks, opts);
    },
  });
  window.wotmMatchPrefetch = { warmer, matchOfEnv };
}
