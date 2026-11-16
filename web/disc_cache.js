// ABOUTME: Keeps the streamed disc's chunks (data/) in the browser's Cache API across visits, per file version (its ETag), each chunk
// ABOUTME: checked against its SHA-256 before use; and prefetches the chunks the boot needs. Linked as a --pre-js after data_retry.js.
'use strict';

// declarations that can repeat: the page loads this file and wotm.js carries it again as a --pre-js, in the same global scope
var CACHE_PREFIX = 'wotlane-disc:';
function cacheName(file, etag) { return `${CACHE_PREFIX}${file}:${etag}`; }
function chunkKey(file, i) { return `https://wotlane.invalid/disc/${encodeURIComponent(file)}/${i}`; }
function discFile(url) { return (/(?:^|\/)data\/+([^/?#]+)$/.exec(typeof url === 'string' ? url : url.url) || [])[1] ?? null; }

async function sha256Hex(subtle, bytes) {
  const d = new Uint8Array(await subtle.digest('SHA-256', bytes));
  return Array.from(d, (b) => b.toString(16).padStart(2, '0')).join('');
}

// Wraps `baseFetch`: a HEAD of a disc file records its version and size; a GET of a chunk-aligned range of a known version is served
// from the cache when every chunk in it is there and matches its hash, else from the network, whose chunks are then stored. Anything
// the cache can't do (no Cache API, storage nearly full, a failing call) leaves the network's answer as it is.
function cachingFetch(baseFetch, { caches, subtle, estimate, chunkBytes = 1 << 20, onBytes = null, fullAt = 0.9 } = {}) {
  const versions = new Map();   // file -> { etag, size }

  async function fromCache(cache, file, first, last) {
    const parts = [];
    for (let i = first; i <= last; i++) {
      const r = await cache.match(chunkKey(file, i));
      if (!r) return null;
      const bytes = new Uint8Array(await r.arrayBuffer());
      if (r.headers.get('x-wotm-sha256') !== (await sha256Hex(subtle, bytes))) return null;
      parts.push(bytes);
    }
    return parts;
  }

  async function store(cache, file, first, bytes, start, size) {
    if (estimate) {
      const { usage = 0, quota = 0 } = await estimate();
      if (!quota || usage + bytes.length > quota * fullAt) return;
    }
    for (let at = 0, i = first; at < bytes.length; at += chunkBytes, i++) {
      const chunk = bytes.slice(at, at + chunkBytes);
      if (chunk.length < chunkBytes && start + at + chunk.length !== size) break;   // a short chunk only at the file's end
      const headers = { 'x-wotm-sha256': await sha256Hex(subtle, chunk), 'content-length': String(chunk.length) };
      await cache.put(chunkKey(file, i), new Response(chunk, { status: 200, headers }));
    }
  }

  return async function (url, init = {}) {
    const file = discFile(url);
    if (!file) return baseFetch(url, init);
    if (init.method === 'HEAD') {
      const r = await baseFetch(url, init);
      const etag = r.headers?.get('etag');
      const size = Number(r.headers?.get('content-length'));
      if (r.ok && etag && size > 0) versions.set(file, { etag, size });
      return r;
    }
    const m = /^bytes=(\d+)-(\d+)$/.exec(init.headers?.Range ?? init.headers?.range ?? '');
    const v = versions.get(file);
    if (!m || !v || !caches || !subtle) return baseFetch(url, init);
    const start = Number(m[1]), end = Math.min(Number(m[2]), v.size - 1);
    if (start % chunkBytes !== 0 || start > end) return baseFetch(url, init);
    const first = start / chunkBytes, last = Math.floor(end / chunkBytes);

    let cache = null;
    try {
      cache = await caches.open(cacheName(file, v.etag));
      const parts = await fromCache(cache, file, first, last);
      if (parts) {
        const all = new Uint8Array(end - start + 1);
        let at = 0;
        for (const p of parts) {
          const n = Math.min(p.length, all.length - at);
          all.set(p.subarray(0, n), at);
          at += n;
        }
        onBytes?.(all.length);
        return new Response(all, { status: 206, headers: { 'content-length': String(all.length), 'content-range': `bytes ${start}-${end}/${v.size}` } });
      }
    } catch (e) {
      cache = null;
    }

    const r = await baseFetch(url, init);
    if (r.status !== 206 || !cache) return r;
    const bytes = new Uint8Array(await r.arrayBuffer());
    try {
      await store(cache, file, first, bytes, start, v.size);
    } catch (e) {
      // storage full or unavailable: the game still gets its bytes
    }
    return new Response(bytes, { status: 206, headers: r.headers });
  };
}

// Deletes the caches of the other versions of the files in `current` (file -> ETag); other caches stay
async function evictOld(caches, current) {
  for (const name of await caches.keys()) {
    if (!name.startsWith(CACHE_PREFIX)) continue;
    const rest = name.slice(CACHE_PREFIX.length);
    const file = rest.slice(0, rest.indexOf(':'));
    if (file in current && name !== cacheName(file, current[file])) await caches.delete(name);
  }
}

// Fetches the listed chunks of `url` (runs of neighbours as one range, at most `maxRun` chunks each, `concurrency` at a time) and
// reads them to the end, so that a caching fetch stores them; a failure just leaves that run to the game's own reads
async function prefetch(fetchFn, url, chunks, { chunkBytes = 1 << 20, maxRun = 8, concurrency = 4 } = {}) {
  const sorted = [...new Set(chunks)].sort((a, b) => a - b);
  const runs = [];
  for (const i of sorted) {
    const run = runs[runs.length - 1];
    if (run && i === run[1] + 1 && run[1] - run[0] + 1 < maxRun) run[1] = i;
    else runs.push([i, i]);
  }
  let next = 0;
  async function worker() {
    while (next < runs.length) {
      const [a, b] = runs[next++];
      try {
        const r = await fetchFn(url, { headers: { Range: `bytes=${a * chunkBytes}-${(b + 1) * chunkBytes - 1}` } });
        await r.arrayBuffer();
      } catch (e) {
        // the game reads it itself later
      }
    }
  }
  await Promise.all(Array.from({ length: Math.min(concurrency, runs.length) }, worker));
  return runs.length;
}

if (typeof module !== 'undefined' && module.exports) module.exports = { cachingFetch, evictOld, prefetch };
else if (typeof globalThis.fetch === 'function' && !globalThis.wotmDiscCaching) {
  globalThis.wotmDiscCaching = true;
  const progress = typeof BroadcastChannel === 'function' ? new BroadcastChannel('wotlane-disc') : null;
  const storage = globalThis.navigator?.storage;
  globalThis.fetch = cachingFetch(globalThis.fetch, {
    caches: globalThis.caches,
    subtle: globalThis.crypto?.subtle,
    estimate: storage?.estimate ? () => storage.estimate() : null,
    onBytes: (n) => progress?.postMessage(n),
  });
  globalThis.wotmDiscCache = { evictOld, prefetch };
}
