// ABOUTME: Reads the game disc from the server's numbered chunk files (data/index.json, data/<digest>/manifest.json, data/<digest>/<n>.bin):
// ABOUTME: a Range read of data/<file> becomes chunk fetches, each checked against the manifest's SHA-256. Loaded by web/disc_cache.js.
'use strict';

(function () {
  // `data/<file>` of a URL, as the base `.../data/` and the file name; the fetch backend's `data//file` counts too. A chunk's own URL
  // (data/<digest>/<n>.bin) has a slash in the name and is not one.
  function discPath(url) {
    const m = /^(.*?(?:^|\/)data\/)\/*([^/?#]+)$/.exec(typeof url === 'string' ? url : url.url);
    return m ? { base: m[1], file: m[2] } : null;
  }

  // the same folder however the caller spells it (the page asks for data/x, the fetch backend for an absolute URL)
  function absolute(base) {
    try {
      return new URL(base, globalThis.location?.href).href;
    } catch (e) {
      return base;
    }
  }

  const discBase = (url) => discPath(url)?.base ?? null;
  const discEtag = (entry) => `"${entry.digest}"`;   // what a HEAD of the file's version says, and the name its cache goes by

  // The chunks an inclusive byte range [start, end] of a file of `size` bytes touches, each with the slice [from, to) of it that is wanted
  function chunkSpan(start, end, chunkBytes, size) {
    const last = Math.min(end, size - 1);
    const plan = [];
    for (let n = Math.floor(start / chunkBytes); n * chunkBytes <= last; n++) {
      const at = n * chunkBytes;
      plan.push({ n, from: Math.max(start, at) - at, to: Math.min(last + 1, at + chunkBytes) - at });
    }
    return plan;
  }

  const isHex = (s, length) => typeof s === 'string' && s.length === length && /^[0-9a-f]+$/.test(s);

  function parseIndex(json) {
    const files = json && typeof json.files === 'object' && json.files;
    if (!files) throw new Error('disc index is not valid');
    for (const [name, e] of Object.entries(files))
      if (!e || !isHex(e.digest, 16) || !Number.isSafeInteger(e.size) || e.size < 0) throw new Error(`disc index: bad entry for ${name}`);
    return json;
  }

  const entryOf = (index, file) => (index && Object.hasOwn(index.files, file) ? index.files[file] : null);

  // the manifest of a version must say what the index says, and have one hash for each chunk
  function parseManifest(json, entry, name) {
    const ok = json && json.size === entry.size && Number.isSafeInteger(json.chunkBytes) && json.chunkBytes > 0 && Array.isArray(json.sha256) &&
      json.sha256.length === Math.ceil(entry.size / json.chunkBytes) && json.sha256.every((h) => isHex(h, 64));
    if (!ok) throw new Error(`disc manifest of ${name} does not match the index`);
    return json;
  }

  // chunk n's URL; a retry names a URL nothing has been asked for before (`nonce`), so that a bad copy kept by the browser or the
  // edge, under the chunk's own URL or an earlier retry's, is not served again
  const chunkUrl = (base, entry, n, nonce) => `${base}${entry.digest}/${n}.bin${nonce ? `?retry=${nonce}` : ''}`;
  const randomNonce = () => Math.random().toString(36).slice(2, 10);
  const chunkLength = (manifest, n) => Math.min(manifest.chunkBytes, manifest.size - n * manifest.chunkBytes);

  async function sha256Hex(subtle, bytes) {
    const d = new Uint8Array(await subtle.digest('SHA-256', bytes));
    return Array.from(d, (b) => b.toString(16).padStart(2, '0')).join('');
  }

  const K = Uint32Array.from([
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
  ]);

  // SHA-256 as hex without SubtleCrypto, which a synchronous read cannot await
  function sha256Sync(bytes) {
    const h = Uint32Array.from([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);
    const w = new Uint32Array(64);
    const rotr = (x, n) => (x >>> n) | (x << (32 - n));
    function block(view, off) {
      for (let i = 0; i < 16; i++) w[i] = view.getUint32(off + 4 * i);
      for (let i = 16; i < 64; i++) {
        const a = w[i - 15], b = w[i - 2];
        w[i] = w[i - 16] + (rotr(a, 7) ^ rotr(a, 18) ^ (a >>> 3)) + w[i - 7] + (rotr(b, 17) ^ rotr(b, 19) ^ (b >>> 10));
      }
      let [a, b, c, d, e, f, g, hh] = h;
      for (let i = 0; i < 64; i++) {
        const t1 = (hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i]) | 0;
        const t2 = ((rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c))) | 0;
        hh = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
      }
      [a, b, c, d, e, f, g, hh].forEach((x, i) => { h[i] += x; });
    }
    const n = bytes.length;
    const full = n - (n % 64);
    const view = new DataView(bytes.buffer, bytes.byteOffset, n);
    for (let off = 0; off < full; off += 64) block(view, off);
    const tail = new Uint8Array(n % 64 < 56 ? 64 : 128);   // what is left, the 1 bit, zeros and the length in bits
    tail.set(bytes.subarray(full));
    tail[n - full] = 0x80;
    const tailView = new DataView(tail.buffer);
    tailView.setUint32(tail.length - 8, Math.floor(n / 0x20000000));
    tailView.setUint32(tail.length - 4, (n * 8) >>> 0);
    for (let off = 0; off < tail.length; off += 64) block(tailView, off);
    return Array.from(h, (x) => x.toString(16).padStart(8, '0')).join('');
  }

  // Wraps `baseFetch` for the disc files the server's index lists: a HEAD is answered from the index (size, and the digest as the
  // version), a range GET from the chunk files that cover it, each fetched whole, checked against its manifest hash (another URL is tried
  // when it fails) and cut to size. With no index (a 404, the server is not split) and for anything the index does not list, `baseFetch`
  // answers as it is. An index that fails another way is an error: falling back to the whole file would hide a broken server. At most
  // `maxInflight` chunk files are in flight at once. `onBytes` hears the size of each chunk that arrived.
  function chunkedFetch(baseFetch, { subtle = null, maxInflight = 12, tries = 3, retryDelayMs = 300, onBytes = null, nonce = randomNonce, log = console.error } = {}) {
    const indexes = new Map();     // base -> Promise of the index, or null when the server has none
    const manifests = new Map();   // base + digest -> Promise of the manifest
    let active = 0;
    const waiting = [];

    // a failed fetch is not kept: the next read asks again
    function remembered(map, key, load) {
      if (!map.has(key)) {
        const p = load();
        map.set(key, p);
        p.catch(() => map.delete(key));
      }
      return map.get(key);
    }

    const getIndex = (base) => remembered(indexes, base, async () => {
      const r = await baseFetch(base + 'index.json', { cache: 'no-cache' });   // revalidated: a new disc is announced there
      if (r.status === 404) return null;
      if (!r.ok) throw new Error(`disc index: HTTP ${r.status}`);
      return parseIndex(await r.json());
    });

    const getManifest = (base, name, entry) => remembered(manifests, base + entry.digest, async () => {
      const r = await baseFetch(`${base}${entry.digest}/manifest.json`);
      if (!r.ok) throw new Error(`disc manifest of ${name}: HTTP ${r.status}`);
      return parseManifest(await r.json(), entry, name);
    });

    // the slot of a finished job goes straight to the next one waiting
    async function limited(job) {
      if (active >= maxInflight) await new Promise((resolve) => waiting.push(resolve));
      else active++;
      try {
        return await job();
      } finally {
        const next = waiting.shift();
        if (next) next();
        else active--;
      }
    }

    async function getChunk(base, name, entry, manifest, n) {
      for (let attempt = 0; ; attempt++) {
        const bytes = await limited(async () => {
          const r = await baseFetch(chunkUrl(base, entry, n, attempt && nonce()));
          if (r.status !== 200) throw new Error(`disc chunk ${n} of ${name}: HTTP ${r.status}`);
          return new Uint8Array(await r.arrayBuffer());
        });
        const hash = bytes.length === chunkLength(manifest, n) ? await (subtle ? sha256Hex(subtle, bytes) : sha256Sync(bytes)) : null;
        if (hash === manifest.sha256[n]) {
          onBytes?.(bytes.length);
          return bytes;
        }
        if (attempt + 1 >= tries) throw new Error(`disc chunk ${n} of ${name} failed its SHA-256 check`);
        await new Promise((resolve) => setTimeout(resolve, retryDelayMs * (attempt + 1)));
      }
    }

    async function read(url, init) {
      const found = discPath(url);
      if (!found) return baseFetch(url, init);
      const path = { base: absolute(found.base), file: found.file };
      const index = await getIndex(path.base);
      const entry = entryOf(index, path.file);
      if (!entry) return baseFetch(url, init);

      const headers = { 'accept-ranges': 'bytes', etag: discEtag(entry), 'content-type': 'application/octet-stream' };
      if (init.method === 'HEAD') return new Response(null, { status: 200, headers: { ...headers, 'content-length': String(entry.size) } });
      const wanted = init.headers?.Range ?? init.headers?.range;
      const m = /^bytes=(\d+)-(\d*)$/.exec(wanted ?? '');
      const start = m ? Number(m[1]) : 0;
      const end = m && m[2] ? Math.min(Number(m[2]), entry.size - 1) : entry.size - 1;
      if ((wanted && !m) || start > end) return new Response(null, { status: 416, headers: { 'content-range': `bytes */${entry.size}` } });

      const manifest = await getManifest(path.base, path.file, entry);
      const parts = await Promise.all(chunkSpan(start, end, manifest.chunkBytes, entry.size).map(async ({ n, from, to }) =>
        (await getChunk(path.base, path.file, entry, manifest, n)).subarray(from, to)));
      const body = new Uint8Array(end - start + 1);
      let at = 0;
      for (const p of parts) {
        body.set(p, at);
        at += p.length;
      }
      const extra = m ? { 'content-range': `bytes ${start}-${end}/${entry.size}` } : {};
      return new Response(body, { status: m ? 206 : 200, headers: { ...headers, ...extra, 'content-length': String(body.length) } });
    }

    // the game turns a failed read into a bare I/O error, so the reason is logged here
    return async function (url, init = {}) {
      try {
        return await read(url, init);
      } catch (e) {
        log?.(`[disc] ${e.message}`);
        throw e;
      }
    };
  }

  // The same read for a game worker's synchronous XHR (Firefox), which can't await anything: `read(url, start, end)` gives
  // { bytes, etag } for the bytes [start, end) of a disc file, or null when the server has no index (or does not list the file). Chunks
  // are fetched with `Xhr`, one after the other, and checked with sha256Sync; a bad chunk is asked for again from another URL, and any
  // failure throws (the caller, the game's own retry loop, tries the whole read again).
  function syncChunkReader(Xhr, { tries = 3, nonce = randomNonce, log = console.error } = {}) {
    const indexes = new Map();
    const manifests = new Map();

    function get(url) {
      const x = new Xhr();
      x.open('GET', url, false);
      x.responseType = 'arraybuffer';
      x.send();
      return { status: x.status, bytes: new Uint8Array(x.response ?? 0) };
    }

    const json = (r) => JSON.parse(new TextDecoder().decode(r.bytes));

    function getIndex(base) {
      if (!indexes.has(base)) {
        const r = get(base + 'index.json');
        if (r.status !== 404 && r.status !== 200) throw new Error(`disc index: HTTP ${r.status}`);
        indexes.set(base, r.status === 404 ? null : parseIndex(json(r)));
      }
      return indexes.get(base);
    }

    function getManifest(base, name, entry) {
      const key = base + entry.digest;
      if (!manifests.has(key)) {
        const r = get(`${base}${entry.digest}/manifest.json`);
        if (r.status !== 200) throw new Error(`disc manifest of ${name}: HTTP ${r.status}`);
        manifests.set(key, parseManifest(json(r), entry, name));
      }
      return manifests.get(key);
    }

    function getChunk(base, name, entry, manifest, n) {
      for (let attempt = 0; ; attempt++) {
        const r = get(chunkUrl(base, entry, n, attempt && nonce()));
        if (r.status !== 200) throw new Error(`disc chunk ${n} of ${name}: HTTP ${r.status}`);
        if (r.bytes.length === chunkLength(manifest, n) && sha256Sync(r.bytes) === manifest.sha256[n]) return r.bytes;
        if (attempt + 1 >= tries) throw new Error(`disc chunk ${n} of ${name} failed its SHA-256 check`);
      }
    }

    function readRange(url, start, end) {
      const found = discPath(url);
      const path = found && { base: absolute(found.base), file: found.file };
      const index = path && getIndex(path.base);
      const entry = entryOf(index, path?.file);
      if (!entry || start >= Math.min(end, entry.size)) return null;
      const manifest = getManifest(path.base, path.file, entry);
      const last = Math.min(end, entry.size) - 1;
      const bytes = new Uint8Array(last - start + 1);
      let at = 0;
      for (const { n, from, to } of chunkSpan(start, last, manifest.chunkBytes, entry.size)) {
        bytes.set(getChunk(path.base, path.file, entry, manifest, n).subarray(from, to), at);
        at += to - from;
      }
      return { bytes, etag: discEtag(entry) };
    }

    function read(url, start, end) {
      try {
        return readRange(url, start, end);
      } catch (e) {
        log?.(`[disc] ${e.message}`);
        throw e;
      }
    }

    return { read };
  }

  const api = { discPath, discBase, discEtag, chunkSpan, sha256Hex, sha256Sync, chunkedFetch, syncChunkReader };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else globalThis.wotmDiscChunks = api;
})();
