// ABOUTME: Keeps the streamed disc's chunks (data/) in the browser's Cache API across visits, per file version (its ETag), each chunk
// ABOUTME: checked against its SHA-256 before use; and prefetches the chunks the boot needs. Loaded by the page and, in workers, by data_retry.js.
'use strict';

// function and var declarations: loading the file twice in one global scope is harmless (the guard at the end keeps one wrapper)
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

// Firefox reads the disc from the game's worker threads with synchronous XHR (native/wotm_webdisc.cpp), which the Cache API can't serve.
// A helper worker (the page starts it, `HELPER_NAME`) owns the cache for them: a worker asks it over a BroadcastChannel, blocks on a
// shared buffer until it answers, and after a read that went to the network hands it the bytes to keep. Messages:
//   { op: 'ping', sab }                  answered (sab[0] = 1) as soon as the helper is listening
//   { op: 'get', url, start, end, sab }  sab[0] = 1 and the bytes from byte 8 on when all of [start, end) is cached, else sab[0] = 2
//   { op: 'put', url, start, bytes }     keeps the chunks of a range the worker fetched itself
var HELPER_NAME = 'wotlane-disc-cache';
var HELPER_CHANNEL = 'wotlane-disc-sync';
var HELPER_HEADER = 8;   // bytes before the data in a reply buffer: Int32 status, Int32 length

// the helper's side: `realFetch` only answers the HEAD that gives a file's version; ranges come from the cache or, for a put, from the bytes
function syncReadHelper(realFetch, opts) {
  const base = async (url, init = {}) => {
    if (init.method === 'HEAD') return realFetch(url, init);
    if (init.fromBytes) return new Response(init.fromBytes, { status: 206, headers: { 'content-length': String(init.fromBytes.length) } });
    return new Response(null, { status: 504 });
  };
  const cached = cachingFetch(base, opts);
  const heads = new Map();   // file -> the HEAD in flight or done
  const known = (url) => {
    const file = discFile(url);
    if (file && !heads.has(file)) heads.set(file, cached(url, { method: 'HEAD' }).catch(() => null));
    return heads.get(file);
  };
  return async function (msg) {
    if (msg.op === 'ping') {
      Atomics.store(new Int32Array(msg.sab), 0, 1);
      Atomics.notify(new Int32Array(msg.sab), 0);
      return;
    }
    if (!discFile(msg.url)) return;
    await known(msg.url);
    if (msg.op === 'put') {
      await cached(msg.url, { headers: { Range: `bytes=${msg.start}-${msg.start + msg.bytes.length - 1}` }, fromBytes: msg.bytes }).catch(() => null);
      return;
    }
    const ctl = new Int32Array(msg.sab, 0, 2);
    let status = 2;
    try {
      const r = await cached(msg.url, { headers: { Range: `bytes=${msg.start}-${msg.end - 1}` } });
      const bytes = r.status === 206 ? new Uint8Array(await r.arrayBuffer()) : null;
      if (bytes && bytes.length === msg.end - msg.start && msg.sab.byteLength >= HELPER_HEADER + bytes.length) {
        new Uint8Array(msg.sab, HELPER_HEADER).set(bytes);
        ctl[1] = bytes.length;
        status = 1;
      }
    } catch (e) {
      // a miss: the worker reads it from the network
    }
    Atomics.store(ctl, 0, status);
    Atomics.notify(ctl, 0);
  };
}

// the game worker's side: `ask(url, start, end)` is the cached bytes of a range or null (it blocks until the helper answers), and
// `remember(url, start, bytes)` hands the helper the bytes of a range that came from the network. Never throws; a helper that doesn't
// answer within `waitMs` is left alone for `pauseMs` (it may still be starting).
function syncReadClient(post, { waitMs = 5000, pingMs = 300, pauseMs = 2000, now = Date.now } = {}) {
  let sab = null, pausedUntil = 0, alive = false;
  function wait(ctl, ms) {
    return Atomics.wait(ctl, 0, 0, ms) !== 'timed-out';
  }
  function ask(url, start, end) {
    if (now() < pausedUntil) return null;
    if (!alive) {
      const ping = new SharedArrayBuffer(4);
      post({ op: 'ping', sab: ping });
      if (!wait(new Int32Array(ping), pingMs)) {
        pausedUntil = now() + pauseMs;
        return null;
      }
      alive = true;
    }
    const need = HELPER_HEADER + (end - start);
    if (!sab || sab.byteLength < need) sab = new SharedArrayBuffer(Math.max(need, HELPER_HEADER + (1 << 20)));
    const ctl = new Int32Array(sab, 0, 2);
    Atomics.store(ctl, 0, 0);
    post({ op: 'get', url, start, end, sab });
    if (!wait(ctl, waitMs)) {
      // the late answer may still write into this buffer: it is abandoned
      sab = null;
      alive = false;
      pausedUntil = now() + pauseMs;
      return null;
    }
    if (ctl[0] !== 1 || ctl[1] !== end - start) return null;
    return new Uint8Array(sab, HELPER_HEADER, end - start).slice();
  }
  const remember = (url, start, bytes) => post({ op: 'put', url, start, bytes: new Uint8Array(bytes.slice(0)) });
  return { ask, remember };
}

// A XMLHttpRequest whose synchronous disc range GETs are answered by `client` when it has the bytes; everything else is `Base`'s
function syncDiscXHR(Base, client) {
  return class extends Base {
    open(method, url, async, ...rest) {
      this.wotm = { method, url: String(url), sync: async === false, range: null };
      return super.open(method, url, async, ...rest);
    }

    setRequestHeader(name, value) {
      if (this.wotm && /^range$/i.test(name)) this.wotm.range = value;
      return super.setRequestHeader(name, value);
    }

    send(body) {
      const w = this.wotm;
      const m = w && w.sync && w.method === 'GET' && discFile(w.url) && /^bytes=(\d+)-(\d+)$/.exec(w.range ?? '');
      if (!m) return super.send(body);
      const url = new URL(w.url, globalThis.location?.href).href;
      const start = Number(m[1]), end = Number(m[2]) + 1;
      const hit = client.ask(url, start, end);
      if (hit) {
        Object.defineProperty(this, 'status', { value: 206 });
        Object.defineProperty(this, 'response', { value: hit.buffer });
        return undefined;
      }
      super.send(body);
      if (this.status === 206 && this.response?.byteLength === end - start) client.remember(url, start, new Uint8Array(this.response));
      return undefined;
    }
  };
}

if (typeof module !== 'undefined' && module.exports) module.exports = { cachingFetch, evictOld, prefetch, syncReadHelper, syncReadClient, syncDiscXHR };
else if (typeof document === 'undefined' && globalThis.name === HELPER_NAME) {
  // this is the helper worker (a page of Firefox starts it below)
  const serve = syncReadHelper(globalThis.fetch.bind(globalThis), { caches: globalThis.caches, subtle: globalThis.crypto?.subtle, estimate: globalThis.navigator?.storage?.estimate ? () => navigator.storage.estimate() : null });
  new BroadcastChannel(HELPER_CHANNEL).onmessage = (e) => { serve(e.data).catch(() => null); };
} else if (typeof globalThis.fetch === 'function' && !globalThis.wotmDiscCaching) {
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
  const canBlock = typeof SharedArrayBuffer === 'function' && typeof BroadcastChannel === 'function';
  if (typeof document !== 'undefined' && document.currentScript?.src && canBlock && typeof Worker === 'function' && globalThis.wotmMainThreadFiles?.()) {
    globalThis.wotmDiscSyncHelper = new Worker(document.currentScript.src, { name: HELPER_NAME });
  } else if (typeof document === 'undefined' && canBlock && typeof XMLHttpRequest === 'function') {
    const channel = new BroadcastChannel(HELPER_CHANNEL);
    globalThis.XMLHttpRequest = syncDiscXHR(XMLHttpRequest, syncReadClient((m) => channel.postMessage(m)));
  }
}
