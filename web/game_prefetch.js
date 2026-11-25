// ABOUTME: Fetches the whole game's disc chunks (every stage, monster and costume: web/match_chunks.json, and the boot's) into the disc
// ABOUTME: cache in the background, so no later fight or menu waits on the network; it pauses while the game reads the disc. Page script.

// The chunks of every disc file a game can read in a fight or on its way there: per file, sorted, each once
function wholeGameChunks(table, boot) {
  const iso = new Set(boot?.['game.iso'] ?? []);
  for (const list of Object.values(table?.stages ?? {})) list.forEach((c) => iso.add(c));
  for (const m of Object.values(table?.monsters ?? {})) {
    m.all.forEach((c) => iso.add(c));
    m.costumes.forEach((list) => list.forEach((c) => iso.add(c)));
  }
  const sorted = (s) => [...s].sort((a, b) => a - b);
  return { 'game.iso': sorted(iso), 'SCUS_971.97': sorted(new Set(boot?.['SCUS_971.97'] ?? [])) };
}

// Fetches what the cache lacks of `files()` ({file: chunks}), file by file, through `prefetch(file, chunks, {stop})` (which returns how
// many requests it made). The game's own disc reads (`activity()`) come first: a fetch stops asking for chunks while the disc has not
// been quiet for `quietMs`, and goes on with what is still missing (`missing(file, chunks)`) once it is; hold() pauses it until
// release() (while the game's wasm downloads, which is no disc read), and the quiet wait starts over from the release. A pass that asked for chunks and left as many missing as before
// ends it (the network keeps failing them, or the cache keeps nothing: fetching again would only download them again).
// `canKeep(count)` is asked before every pass: false, no room to keep them, nothing more is fetched. start() resolves to how it ended:
// 'done', 'stopped' (stop(): an online match, where the bandwidth is the match's), 'no room', 'gave up' or 'no table (<why>)'
function createGameWarmer({ files, missing, canKeep, prefetch, now, sleep, quietMs = 5000 }) {
  let stopped = false, started = false, held = false, lastActivity = -Infinity;
  const busy = () => held || now() - lastActivity < quietMs;

  async function waitQuiet() {
    while (!stopped && busy()) await sleep(held ? quietMs : Math.max(100, lastActivity + quietMs - now()));
  }

  async function run() {
    let want;
    try { want = await files(); } catch (e) { return `no table (${e.message})`; }
    let lastLeft = Infinity, lastAsked = 0;
    while (!stopped) {
      await waitQuiet();
      if (stopped) break;
      const left = [];
      for (const [file, chunks] of Object.entries(want)) {
        const lacking = await missing(file, chunks);
        if (lacking.length) left.push([file, lacking]);
      }
      if (!left.length) return 'done';
      const count = left.reduce((n, [, l]) => n + l.length, 0);
      if (lastAsked > 0 && count >= lastLeft) return 'gave up';
      if (!(await canKeep(count))) return 'no room';
      lastLeft = count;
      let interrupted = false;
      const stop = () => {
        if (stopped) return true;
        if (busy()) interrupted = true;
        return interrupted;
      };
      lastAsked = 0;
      for (const [file, lacking] of left) {
        lastAsked += await prefetch(file, lacking, { stop });
        if (stopped || interrupted) break;
      }
    }
    return 'stopped';
  }

  return {
    start() {
      if (stopped) return Promise.resolve('stopped');
      if (started) return Promise.resolve('started already');
      started = true;
      return run();
    },
    activity() { lastActivity = now(); },
    hold() { held = true; },
    release() {
      held = false;
      lastActivity = now();
    },
    stop() { stopped = true; },
  };
}

// Whether the whole game may come down on this device now, for a phone's data plan: 'no' with Save-Data or on a mobile link
// (navigator.connection.type cellular or wimax, which Chrome on Android fills in), 'later' on a touch-first device that does not say
// what link it is on (no type, as in Firefox and Safari, or unknown, other, mixed, bluetooth: maybe a phone on a data plan) until
// its player has started a game, else 'yes'
const FIXED_LINKS = ['wifi', 'ethernet', 'none'];
function prefetchAllowed({ connection, coarse, gameStarted }) {
  if (connection?.saveData || ['cellular', 'wimax'].includes(connection?.type)) return 'no';
  if (FIXED_LINKS.includes(connection?.type) || !coarse || gameStarted) return 'yes';
  return 'later';
}

if (typeof module !== 'undefined' && module.exports) module.exports = { wholeGameChunks, createGameWarmer, prefetchAllowed };
if (typeof window !== 'undefined') {
  // the page's one whole-game warmer: boot.js starts it once the boot's own downloads are done (and again when a single-player game
  // starts, for a device prefetchAllowed says 'later' to), holds it from the game's start until its runtime is up and stops it when an
  // online match starts
  let gameWarmer = null, gameWarmerStopped = false, gameWarmerHeld = false, gameWarmerRefused = false;
  window.wotmGamePrefetch = {
    start(log, { gameStarted = false } = {}) {
      const dc = window.wotmDiscCache;
      if (gameWarmer || gameWarmerStopped || !dc?.cachedChunks || !window.caches) return;
      const allowed = prefetchAllowed({ connection: navigator.connection, coarse: window.matchMedia?.('(pointer: coarse)').matches, gameStarted });
      if (allowed !== 'yes') {
        if (allowed === 'no' && !gameWarmerRefused) log('[web] whole-game prefetch: off (Save-Data or a mobile link)');
        gameWarmerRefused ||= allowed === 'no';
        return;
      }
      const etags = {};
      const json = async (name) => {
        const r = await fetch(name);
        if (!r.ok) throw new Error(`${name}: HTTP ${r.status}`);
        return r.json();
      };
      const storage = navigator.storage;
      gameWarmer = createGameWarmer({
        files: async () => {
          const all = wholeGameChunks(await json('match_chunks.json'), await json('boot_chunks.json'));
          // the version each file's chunks are kept under (disc_cache.js names a cache by it, and keeps nothing without one)
          for (const file of Object.keys(all)) {
            etags[file] = (await fetch('/data/' + file, { method: 'HEAD' })).headers.get('etag');
            if (!etags[file]) throw new Error(`/data/${file} has no version (ETag): the cache would keep nothing`);
          }
          return all;
        },
        missing: async (file, chunks) => {
          const have = await dc.cachedChunks(window.caches, file, etags[file]);
          return chunks.filter((c) => !have.has(c));
        },
        // match_prefetch.js (loaded before this script) knows whether the cache can keep that many chunks
        canKeep: (count) => canKeepChunks({ caches: window.caches, estimate: storage?.estimate ? () => storage.estimate() : null }, count),
        // two 2 MiB runs at a time: a game read that starts meanwhile waits behind at most four chunk files. Where the game runs on the
        // page's thread (support.js mainThreadFiles: Firefox), one 1 MiB chunk at a time, so each chunk's hash and store is spread out
        prefetch: (file, chunks, { stop }) => dc.prefetch(fetch, '/data/' + file, chunks, { ...(window.wotmMainThreadFiles?.() ? { maxRun: 1, concurrency: 1 } : { maxRun: 2, concurrency: 2 }), stop, background: true }),
        now: () => performance.now(),
        sleep: (ms) => new Promise((resolve) => setTimeout(resolve, ms)),
      });
      if (gameWarmerHeld) gameWarmer.hold();
      // the game's reads ({want} as one starts, a number: a download, {hit}: the cache) and the boot's and the lobby's prefetches
      // pause it; its own downloads come as {prefetch} (disc_cache.js)
      new BroadcastChannel('wotlane-disc').onmessage = (e) => {
        if (typeof e.data === 'number' || typeof e.data?.hit === 'number' || e.data?.want) gameWarmer.activity();
      };
      gameWarmer.start().then((how) => log(`[web] whole-game prefetch: ${how}`), (e) => log(`[web] whole-game prefetch failed: ${e.message}`));
    },
    hold() {
      gameWarmerHeld = true;
      gameWarmer?.hold();
    },
    release() {
      gameWarmerHeld = false;
      gameWarmer?.release();
    },
    stop() {
      gameWarmerStopped = true;
      gameWarmer?.stop();
    },
  };
}
