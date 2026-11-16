// ABOUTME: Tests for web/disc_chunks.js: a disc read mapped onto the server's numbered chunk files, each checked against the manifest's
// ABOUTME: SHA-256, in the async fetch path and the synchronous XHR path, with the plain Range path when there is no index. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { chunkedFetch, chunkSpan, discBase, discEtag, sha256Sync, syncChunkReader } = require('./disc_chunks.js');
const { cachingFetch, syncDiscXHR } = require('./disc_cache.js');

const CHUNK = 8;   // bytes per chunk in these tests
const SIZE = 20;   // the fake file: bytes 0..19 (chunks 0, 1 and a short 2)
const FILE = Uint8Array.from({ length: SIZE }, (_, i) => i);
const DIGEST = '0123456789abcdef';
const hex = (bytes) => createHash('sha256').update(bytes).digest('hex');
const CHUNK_URL = new RegExp(`/${DIGEST}/\\d+\\.bin`);

// the server: /data/index.json, /data/<digest>/manifest.json and /data/<digest>/<n>.bin (plain 200, whole chunk), and the whole-file
// Range answer of a server that was not split; `chunked: false` has no index (404). `bad(n, attempt)` returns the bytes to send for
// chunk n instead (attempt 0 is the first request, then ?retry=1, 2, ...). `answer` is synchronous; fakeServer wraps it as a fetch.
function server({ chunked = true, bad = () => null, size = SIZE, file = FILE } = {}) {
  const calls = [];
  const chunkCount = Math.ceil(size / CHUNK);
  const manifest = { name: 'game.iso', size, chunkBytes: CHUNK, chunks: chunkCount, sha256: Array.from({ length: chunkCount }, (_, n) => hex(file.slice(n * CHUNK, (n + 1) * CHUNK))) };
  const index = { chunkBytes: CHUNK, files: { 'game.iso': { digest: DIGEST, size } } };
  const json = (o) => ({ status: 200, body: new TextEncoder().encode(JSON.stringify(o)) });
  function answer(url, init = {}) {
    const u = String(url);
    calls.push({ url: u, range: init.headers?.Range ?? null, method: init.method ?? 'GET' });
    let m;
    if (/data\/+index\.json$/.test(u)) return chunked ? json(index) : { status: 404, body: new Uint8Array() };
    if (new RegExp(`(^|/)data/${DIGEST}/manifest\\.json$`).test(u)) return json(manifest);
    if ((m = new RegExp(`(^|/)data/${DIGEST}/(\\d+)\\.bin(\\?retry=(\\d+))?$`).exec(u))) {
      const n = Number(m[2]);
      return { status: 200, body: bad(n, Number(m[4] ?? 0)) ?? file.slice(n * CHUNK, (n + 1) * CHUNK) };
    }
    if (/data\/+game\.iso$/.test(u)) {   // the whole-file path
      if (init.method === 'HEAD') return { status: 200, body: null, headers: { etag: '"raw"', 'content-length': String(size) } };
      const [, a, b] = /bytes=(\d+)-(\d+)/.exec(init.headers?.Range ?? 'bytes=0-0');
      const body = file.slice(Number(a), Math.min(Number(b) + 1, size));
      return { status: 206, body, headers: { 'content-length': String(body.length) } };
    }
    return { status: 200, body: 'page' };
  }
  return { answer, calls };
}

function fakeServer(opts) {
  const s = server(opts);
  const fn = async (url, init) => {
    const r = s.answer(url, init);
    return new Response(r.body, { status: r.status, headers: r.headers });
  };
  fn.calls = s.calls;
  fn.chunkCalls = () => s.calls.filter((c) => CHUNK_URL.test(c.url));
  fn.fetchedChunks = () => fn.chunkCalls().map((c) => Number(/(\d+)\.bin/.exec(c.url)[1]));
  return fn;
}

const make = (net, extra = {}) => chunkedFetch(net, { subtle: crypto.subtle, ...extra });
const range = (a, b) => ({ headers: { Range: `bytes=${a}-${b}` } });
const bytesOf = async (r) => [...new Uint8Array(await r.arrayBuffer())];

// ---- which chunks a read needs ----

test('chunkSpan maps an inclusive byte range onto chunk numbers and the slice of each', () => {
  assert.deepEqual(chunkSpan(0, 7, CHUNK, SIZE), [{ n: 0, from: 0, to: 8 }]);
  assert.deepEqual(chunkSpan(6, 9, CHUNK, SIZE), [{ n: 0, from: 6, to: 8 }, { n: 1, from: 0, to: 2 }]);
  assert.deepEqual(chunkSpan(8, 15, CHUNK, SIZE), [{ n: 1, from: 0, to: 8 }]);   // ends exactly on a chunk boundary: no chunk 2
  assert.deepEqual(chunkSpan(0, 19, CHUNK, SIZE).map((p) => p.n), [0, 1, 2]);
  assert.deepEqual(chunkSpan(16, 19, CHUNK, SIZE), [{ n: 2, from: 0, to: 4 }]);   // the short last chunk
  assert.deepEqual(chunkSpan(16, 99, CHUNK, SIZE), [{ n: 2, from: 0, to: 4 }]);   // an end past the file is clamped
});

test('discBase and discEtag name a disc file\'s folder and version', () => {
  assert.equal(discBase('http://x/data/game.iso'), 'http://x/data/');
  assert.equal(discBase('data//game.iso'), 'data/');
  assert.equal(discBase('http://x/page.html'), null);
  assert.equal(discEtag({ digest: DIGEST }), `"${DIGEST}"`);
});

// ---- the fetch path ----

test('a HEAD of a disc file is answered from the index: size, version and range support, no chunk fetched', async () => {
  const net = fakeServer();
  const f = make(net);
  const r = await f('http://x/data/game.iso', { method: 'HEAD', headers: { Range: 'bytes=0-' } });
  assert.equal(r.status, 200);
  assert.equal(r.headers.get('content-length'), String(SIZE));
  assert.equal(r.headers.get('etag'), `"${DIGEST}"`);
  assert.equal(r.headers.get('accept-ranges'), 'bytes');
  assert.equal(net.chunkCalls().length, 0);
});

test('a range over two chunks is built from exactly those chunks', async () => {
  const net = fakeServer();
  const f = make(net);
  const r = await f('http://x/data/game.iso', range(6, 9));
  assert.equal(r.status, 206);
  assert.equal(r.headers.get('content-range'), `bytes 6-9/${SIZE}`);
  assert.equal(r.headers.get('content-length'), '4');
  assert.deepEqual(await bytesOf(r), [6, 7, 8, 9]);
  assert.deepEqual(net.fetchedChunks().sort(), [0, 1]);
});

test('an aligned read of a whole chunk is one chunk file; the last short chunk and an end past the file work', async () => {
  const net = fakeServer();
  const f = make(net);
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(8, 15))), [...FILE.slice(8, 16)]);
  assert.deepEqual(net.fetchedChunks(), [1]);
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(16, 1015))), [...FILE.slice(16)]);
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(0, 19))), [...FILE]);
});

test('a read that starts past the end of the file is a 416; an open-ended range reads to the end', async () => {
  const f = make(fakeServer());
  const past = await f('http://x/data/game.iso', range(SIZE, SIZE + 7));
  assert.equal(past.status, 416);
  const open = await f('http://x/data/game.iso', { headers: { Range: 'bytes=12-' } });
  assert.equal(open.status, 206);
  assert.deepEqual(await bytesOf(open), [...FILE.slice(12)]);
});

test('the index and each manifest are fetched once, however many reads follow', async () => {
  const net = fakeServer();
  const f = make(net);
  await f('http://x/data/game.iso', { method: 'HEAD' });
  await Promise.all([f('http://x/data/game.iso', range(0, 7)), f('http://x/data//game.iso', range(8, 15))]);
  assert.equal(net.calls.filter((c) => /index\.json/.test(c.url)).length, 1);
  assert.equal(net.calls.filter((c) => /manifest\.json/.test(c.url)).length, 1);
});

test('requests that are not for a listed disc file pass through untouched', async () => {
  const net = fakeServer();
  const f = make(net);
  assert.equal(await (await f('http://x/page.html')).text(), 'page');
  assert.equal(await (await f('http://x/data/other.bin', range(0, 3))).text(), 'page');
  assert.equal(await (await f('http://x/data/constructor', range(0, 3))).text(), 'page');   // not a property of the index object
  assert.equal(net.chunkCalls().length, 0);
});

// ---- integrity ----

const WRONG = Uint8Array.from([9, 9, 9, 9, 9, 9, 9, 9]);

test('a chunk that fails its hash is fetched again from a new URL and never returned wrong', async () => {
  const net = fakeServer({ bad: (n, attempt) => (n === 1 && attempt === 0 ? WRONG : null) });
  const f = make(net, { retryDelayMs: 0 });
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(8, 15))), [...FILE.slice(8, 16)]);
  const calls = net.chunkCalls();
  assert.equal(calls.length, 2);
  assert.match(calls[1].url, /1\.bin\?retry=1$/);   // another URL, so a bad copy at the edge is not served again
});

test('a chunk that keeps failing, or arrives the wrong length, is an error and nothing is returned', async () => {
  const f = make(fakeServer({ bad: (n) => (n === 0 ? WRONG : null) }), { retryDelayMs: 0 });
  await assert.rejects(f('http://x/data/game.iso', range(0, 7)), /chunk 0/);
  const short = make(fakeServer({ bad: (n) => (n === 1 ? FILE.slice(8, 12) : null) }), { retryDelayMs: 0 });
  await assert.rejects(short('http://x/data/game.iso', range(8, 15)), /chunk 1/);
});

test('a chunk the server does not have is an error, not a fallback to the whole file', async () => {
  const net = fakeServer();
  const missing = async (url, init) => (/2\.bin/.test(String(url)) ? new Response('gone', { status: 404 }) : net(url, init));
  const f = make(missing, { retryDelayMs: 0 });
  await assert.rejects(f('http://x/data/game.iso', range(16, 19)), /chunk 2.*404/);
  assert.equal(net.calls.filter((c) => c.method === 'GET' && c.range).length, 0);
});

// ---- no index: the whole-file Range path ----

test('with no index (404) every request goes to the whole-file URL untouched, and the index is asked for once', async () => {
  const net = fakeServer({ chunked: false });
  const f = make(net);
  const head = await f('http://x/data/game.iso', { method: 'HEAD' });
  assert.equal(head.headers.get('etag'), '"raw"');
  const r = await f('http://x/data/game.iso', range(0, 7));
  assert.equal(r.status, 206);
  assert.deepEqual(await bytesOf(r), [...FILE.slice(0, 8)]);
  assert.equal(net.calls.filter((c) => /index\.json/.test(c.url)).length, 1);
  assert.equal(net.chunkCalls().length, 0);
});

test('an index that fails some other way (500, 401) is an error, not a fallback, and is asked for again next time', async () => {
  let status = 500;
  const net = fakeServer();
  const flaky = async (url, init) => (/index\.json/.test(String(url)) && status !== 200 ? new Response('no', { status }) : net(url, init));
  const f = make(flaky);
  await assert.rejects(f('http://x/data/game.iso', range(0, 7)), /index.*500/);
  status = 401;
  await assert.rejects(f('http://x/data/game.iso', range(0, 7)), /index.*401/);
  status = 200;
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(0, 7))), [...FILE.slice(0, 8)]);
});

// ---- load and progress ----

test('no more than maxInflight chunk files are in flight at once, and the bytes that arrived are reported', async () => {
  const net = fakeServer();
  let now = 0, peak = 0;
  const slow = async (url, init) => {
    if (!/\.bin/.test(String(url))) return net(url, init);
    peak = Math.max(peak, ++now);
    await new Promise((r) => setTimeout(r, 5));
    now--;
    return net(url, init);
  };
  const reported = [];
  const f = make(slow, { maxInflight: 2, onBytes: (n) => reported.push(n) });
  await f('http://x/data/game.iso', range(0, 19));
  assert.equal(peak, 2);
  assert.equal(reported.reduce((a, b) => a + b, 0), SIZE);
});

// ---- with the browser cache on top ----

function fakeCaches() {
  const store = new Map();
  return {
    async open(name) {
      if (!store.has(name)) store.set(name, new Map());
      const m = store.get(name);
      return { async match(k) { const r = m.get(String(k)); return r ? r.clone() : undefined; }, async put(k, r) { m.set(String(k), r.clone()); } };
    },
    async keys() { return [...store.keys()]; },
    async delete(name) { return store.delete(name); },
    store,
  };
}

test('under the Cache API a read is fetched as chunk files once, then served from the cache, which is named by the digest', async () => {
  const net = fakeServer();
  const caches = fakeCaches();
  const f = cachingFetch(make(net), { caches, subtle: crypto.subtle, estimate: async () => ({ usage: 0, quota: 1e9 }), chunkBytes: CHUNK });
  await f('http://x/data/game.iso', { method: 'HEAD' });
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(0, 15))), [...FILE.slice(0, 16)]);
  assert.deepEqual(await bytesOf(await f('http://x/data/game.iso', range(0, 15))), [...FILE.slice(0, 16)]);
  assert.deepEqual(net.fetchedChunks().sort(), [0, 1]);
  assert.deepEqual([...caches.store.keys()], [`wotlane-disc:game.iso:"${DIGEST}"`]);
});

// ---- SHA-256 without SubtleCrypto (the synchronous path cannot await it) ----

test('sha256Sync agrees with the platform for every length around the block edges and for a large chunk', () => {
  for (const n of [0, 1, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128, 129, 1000]) {
    const bytes = Uint8Array.from({ length: n }, (_, i) => (i * 7 + 3) & 255);
    assert.equal(sha256Sync(bytes), hex(bytes), `length ${n}`);
  }
  const big = Uint8Array.from({ length: 1 << 20 }, (_, i) => (i * 31 + (i >> 8)) & 255);
  assert.equal(sha256Sync(big), hex(big));
  assert.equal(sha256Sync(big.subarray(5, 70)), hex(big.subarray(5, 70)));   // a view that doesn't start at its buffer's start
});

// ---- the synchronous XHR path ----

// a synchronous XMLHttpRequest class over the fake server: send() answers at once from `answer`
function fakeXhr(s) {
  return class {
    open(method, url, async) { this.method = method; this.url = url; this.async = async; this.headers = {}; }
    setRequestHeader(n, v) { this.headers[n] = v; }
    getResponseHeader() { return null; }
    send() {
      assert.equal(this.async, false, 'a synchronous read');
      const r = s.answer(this.url, { method: this.method, headers: this.headers });
      this.status = r.status;
      const b = typeof r.body === 'string' ? new TextEncoder().encode(r.body) : r.body ?? new Uint8Array();
      this.response = b.buffer.slice(b.byteOffset, b.byteOffset + b.length);
    }
  };
}

test('the sync reader gives the bytes of a range [start, end) from chunk files and the version to keep them under', () => {
  const s = server();
  const reader = syncChunkReader(fakeXhr(s));
  const got = reader.read('http://x/data/game.iso', 6, 10);
  assert.deepEqual([...got.bytes], [6, 7, 8, 9]);
  assert.equal(got.etag, `"${DIGEST}"`);
  assert.deepEqual(s.calls.filter((c) => /\.bin/.test(c.url)).map((c) => c.url.slice(-5)).sort(), ['0.bin', '1.bin']);
  reader.read('http://x/data/game.iso', 16, 20);
  assert.equal(s.calls.filter((c) => /index\.json|manifest/.test(c.url)).length, 2, 'the index and the manifest, once');
});

test('the sync reader returns null when there is no index, and throws when the index fails another way', () => {
  assert.equal(syncChunkReader(fakeXhr(server({ chunked: false }))).read('http://x/data/game.iso', 0, 8), null);
  const failing = server();
  const X = fakeXhr({ answer: (u, i) => (/index/.test(u) ? { status: 503, body: null } : failing.answer(u, i)), calls: [] });
  assert.throws(() => syncChunkReader(X).read('http://x/data/game.iso', 0, 8), /index.*503/);
});

test('the sync reader checks every chunk, tries again from a new URL, and throws when it stays wrong', () => {
  const s = server({ bad: (n, attempt) => (n === 0 && attempt === 0 ? WRONG : null) });
  assert.deepEqual([...syncChunkReader(fakeXhr(s)).read('http://x/data/game.iso', 0, 8).bytes], [...FILE.slice(0, 8)]);
  assert.match(s.calls.at(-1).url, /0\.bin\?retry=1$/);
  const stuck = server({ bad: (n) => (n === 0 ? WRONG : null) });
  assert.throws(() => syncChunkReader(fakeXhr(stuck)).read('http://x/data/game.iso', 0, 8), /chunk 0/);
});

// the worker's XMLHttpRequest: a base class the wrapper subclasses, which must not be used for the chunk requests
class RangeXHR {
  constructor() { this.sent = 0; this.headers = {}; }
  open(method, url, async) { this.method = method; this.url = url; this.async = async; }
  setRequestHeader(n, v) { this.headers[n] = v; }
  getResponseHeader() { return '"raw"'; }
  send() { this.sent++; this.status = 206; this.response = FILE.slice(0, 8).buffer; }
}

function wrapped(chunkReader) {
  const kept = [];
  const X = syncDiscXHR(RangeXHR, { ask: () => null, remember: (u, s, b, etag) => kept.push([s, [...b], etag]) }, chunkReader);
  const x = new X();
  x.open('GET', 'http://x/data/game.iso', false);
  x.responseType = 'arraybuffer';
  x.setRequestHeader('Range', 'bytes=8-15');
  return { x, kept };
}

test('a worker\'s synchronous disc read that the helper cannot answer is read from chunk files and handed to the helper to keep', () => {
  const { x, kept } = wrapped(syncChunkReader(fakeXhr(server())));
  x.send();
  assert.equal(x.sent, 0, 'the whole-file URL is not used');
  assert.equal(x.status, 206);
  assert.deepEqual([...new Uint8Array(x.response)], [...FILE.slice(8, 16)]);
  assert.deepEqual(kept, [[8, [...FILE.slice(8, 16)], `"${DIGEST}"`]]);
});

test('without an index the worker\'s synchronous read goes to the whole-file URL as it always did', () => {
  const { x, kept } = wrapped(syncChunkReader(fakeXhr(server({ chunked: false }))));
  x.send();
  assert.equal(x.sent, 1);
  assert.deepEqual(kept.map(([s, , etag]) => [s, etag]), [[8, '"raw"']]);
});
