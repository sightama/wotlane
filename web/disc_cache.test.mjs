// ABOUTME: Tests for web/disc_cache.js with an in-memory Cache API and a fake network: disc chunks kept per file version and verified
// ABOUTME: by hash, cache misses and failures falling back to the network, old versions evicted, and the boot prefetch. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { cachingFetch, evictOld, prefetch, syncReadHelper, syncReadClient, syncDiscXHR } = createRequire(import.meta.url)('./disc_cache.js');

const CHUNK = 8;   // bytes per chunk in these tests
const SIZE = 20;   // the fake file: bytes 0..19 (chunks 0, 1 and a short 2)
const FILE = Uint8Array.from({ length: SIZE }, (_, i) => i);

// the network: HEAD gives the size and version, a Range GET the bytes (206), anything else 404
function fakeNetwork(etag = '"v1"') {
  const calls = [];
  const fn = async (url, init = {}) => {
    calls.push({ url, range: init.headers?.Range ?? null, method: init.method ?? 'GET' });
    if (!/data\/+game\.iso$/.test(url)) return new Response('page', { status: 200 });
    if (init.method === 'HEAD') return new Response(null, { status: 200, headers: { etag, 'content-length': String(SIZE) } });
    if (!init.headers?.Range) return new Response(FILE, { status: 200 });
    const [, a, b] = /bytes=(\d+)-(\d+)/.exec(init.headers.Range);
    const bytes = FILE.slice(Number(a), Math.min(Number(b) + 1, SIZE));
    return new Response(bytes, { status: 206, headers: { 'content-length': String(bytes.length) } });
  };
  fn.calls = calls;
  fn.gets = () => calls.filter((c) => c.method === 'GET' && c.range);
  return fn;
}

// the Cache API in memory: named caches of key -> Response
function fakeCaches({ putFails = false } = {}) {
  const store = new Map();
  return {
    store,
    async open(name) {
      if (!store.has(name)) store.set(name, new Map());
      const m = store.get(name);
      return {
        async match(key) { const r = m.get(String(key)); return r ? r.clone() : undefined; },
        async put(key, r) { if (putFails) throw new DOMException('full', 'QuotaExceededError'); m.set(String(key), r.clone()); },
      };
    },
    async keys() { return [...store.keys()]; },
    async delete(name) { return store.delete(name); },
  };
}

const roomy = async () => ({ usage: 0, quota: 1e9 });
const make = (net, caches, extra = {}) =>
  cachingFetch(net, { caches, subtle: crypto.subtle, estimate: roomy, chunkBytes: CHUNK, ...extra });
const bytesOf = async (r) => [...new Uint8Array(await r.arrayBuffer())];
const range = (a, b) => ({ headers: { Range: `bytes=${a}-${b}` } });

test('a disc range is fetched once, then served from the cache with the same bytes', async () => {
  const net = fakeNetwork();
  const f = make(net, fakeCaches());
  await f('data/game.iso', { method: 'HEAD' });
  const first = await f('data/game.iso', range(0, 15));
  assert.equal(first.status, 206);
  assert.deepEqual(await bytesOf(first), [...FILE.slice(0, 16)]);
  const again = await f('data/game.iso', range(0, 15));
  assert.equal(again.status, 206);
  assert.deepEqual(await bytesOf(again), [...FILE.slice(0, 16)]);
  assert.equal(net.gets().length, 1);
});

test('chunks are kept one by one: a later request over cached chunks needs no network, even the short last one', async () => {
  const net = fakeNetwork();
  const f = make(net, fakeCaches());
  await f('data/game.iso', { method: 'HEAD' });
  await f('data/game.iso', range(0, 23));            // all three chunks, the last one short
  assert.deepEqual(await bytesOf(await f('data/game.iso', range(8, 15))), [...FILE.slice(8, 16)]);
  assert.deepEqual(await bytesOf(await f('data/game.iso', range(16, 19))), [...FILE.slice(16, 20)]);   // end clamped to the file
  assert.equal(net.gets().length, 1);
});

test('a range with one chunk missing goes to the network whole', async () => {
  const net = fakeNetwork();
  const f = make(net, fakeCaches());
  await f('data/game.iso', { method: 'HEAD' });
  await f('data/game.iso', range(0, 7));
  assert.deepEqual(await bytesOf(await f('data/game.iso', range(0, 15))), [...FILE.slice(0, 16)]);
  assert.deepEqual(net.gets().map((c) => c.range), ['bytes=0-7', 'bytes=0-15']);
});

test('a cached chunk that fails its hash is never served: the network answers and the chunk is replaced', async () => {
  const net = fakeNetwork();
  const caches = fakeCaches();
  const f = make(net, caches);
  await f('data/game.iso', { method: 'HEAD' });
  await f('data/game.iso', range(0, 7));
  const [name] = caches.store.keys();
  const [key, good] = [...caches.store.get(name)][0];
  caches.store.get(name).set(key, new Response(new Uint8Array(8).fill(9), { headers: good.headers }));   // same hash header, other bytes
  assert.deepEqual(await bytesOf(await f('data/game.iso', range(0, 7))), [...FILE.slice(0, 8)]);
  assert.deepEqual(await bytesOf(await f('data/game.iso', range(0, 7))), [...FILE.slice(0, 8)]);
  assert.equal(net.gets().length, 2);   // the bad copy cost one fetch, its replacement served the next
});

test('without a known version (no HEAD seen yet) nothing is cached', async () => {
  const net = fakeNetwork();
  const caches = fakeCaches();
  const f = make(net, caches);
  await f('data/game.iso', range(0, 7));
  await f('data/game.iso', range(0, 7));
  assert.equal(net.gets().length, 2);
  assert.equal(caches.store.size, 0);
});

test('no Cache API, a full quota or a failing put: the bytes still come from the network', async () => {
  for (const extra of [{ caches: undefined }, { estimate: async () => ({ usage: 95, quota: 100 }) }, { caches: fakeCaches({ putFails: true }) }]) {
    const net = fakeNetwork();
    const f = make(net, fakeCaches(), extra);
    await f('data/game.iso', { method: 'HEAD' });
    assert.deepEqual(await bytesOf(await f('data/game.iso', range(0, 7))), [...FILE.slice(0, 8)]);
    assert.deepEqual(await bytesOf(await f('data/game.iso', range(0, 7))), [...FILE.slice(0, 8)]);
    assert.equal(net.gets().length, 2);
  }
});

test('requests that are not disc ranges pass through untouched', async () => {
  const net = fakeNetwork();
  const f = make(net, fakeCaches());
  assert.equal(await (await f('wotm.js', {})).text(), 'page');
  await f('data/game.iso', { method: 'HEAD' });
  await f('data/game.iso', {});   // a whole-file GET
  assert.equal(net.calls.length, 3);
});

test('each chunk served from the cache is reported, as a download is', async () => {
  const net = fakeNetwork();
  const heard = [];
  const f = make(net, fakeCaches(), { onBytes: (n) => heard.push(n) });
  await f('data/game.iso', { method: 'HEAD' });
  await f('data/game.iso', range(0, 15));
  await f('data/game.iso', range(0, 15));
  assert.deepEqual(heard, [16]);   // the network's own reports come from data_retry.js; the cache hit reports here
});

test('a new version of a disc file uses a new cache; evictOld drops the other versions and nothing else', async () => {
  const caches = fakeCaches();
  const f1 = make(fakeNetwork('"v1"'), caches);
  await f1('data/game.iso', { method: 'HEAD' });
  await f1('data/game.iso', range(0, 7));
  const f2 = make(fakeNetwork('"v2"'), caches);
  await f2('data/game.iso', { method: 'HEAD' });
  await f2('data/game.iso', range(0, 7));
  await caches.open('something-else');
  assert.equal(caches.store.size, 3);
  await evictOld(caches, { 'game.iso': '"v2"' });
  const left = [...caches.store.keys()];
  assert.equal(left.length, 2);
  assert.ok(left.includes('something-else'));
  assert.ok(left.some((n) => n.includes('"v2"')));
});

test('prefetch asks for the listed chunks in runs of neighbours, a few requests at a time', async () => {
  const net = fakeNetwork();
  await prefetch(net, 'data/game.iso', [0, 1, 4, 5, 6, 9], { chunkBytes: CHUNK, maxRun: 2, concurrency: 2 });
  assert.deepEqual(net.gets().map((c) => c.range).sort(), ['bytes=0-15', 'bytes=32-47', 'bytes=48-55', 'bytes=72-79'].sort());
});

test('the fetch backend\'s double slash (data//game.iso) names the same file and cache', async () => {
  const net = fakeNetwork();
  const caches = fakeCaches();
  const f = make(net, caches);
  await f('data//game.iso', { method: 'HEAD' });
  await f('data//game.iso', range(0, 7));
  assert.deepEqual(await bytesOf(await f('data/game.iso', range(0, 7))), [...FILE.slice(0, 8)]);
  assert.equal(net.gets().length, 1);
});

// ---- synchronous reads of a game worker (Firefox): the helper worker, the worker's client and the XHR that uses them ----

const helperOpts = (caches) => ({ caches, subtle: crypto.subtle, estimate: roomy, chunkBytes: CHUNK });
const get = async (serve, start, end) => {
  const sab = new SharedArrayBuffer(8 + (end - start));
  await serve({ op: 'get', url: 'http://x/data/game.iso', start, end, sab });
  const ctl = new Int32Array(sab, 0, 2);
  return ctl[0] === 1 ? [...new Uint8Array(sab, 8, ctl[1])] : null;
};

test('the helper answers a ping, misses what is not cached without touching the network, then serves what a worker put', async () => {
  const net = fakeNetwork();
  const serve = syncReadHelper(net, helperOpts(fakeCaches()));
  const ping = new SharedArrayBuffer(4);
  await serve({ op: 'ping', sab: ping });
  assert.equal(new Int32Array(ping)[0], 1);

  assert.equal(await get(serve, 0, 16), null);
  assert.deepEqual(net.gets(), []);

  await serve({ op: 'put', url: 'http://x/data/game.iso', start: 0, bytes: FILE.slice(0, 16) });
  assert.deepEqual(await get(serve, 0, 16), [...FILE.slice(0, 16)]);
  assert.deepEqual(await get(serve, 8, 16), [...FILE.slice(8, 16)]);
  assert.equal(await get(serve, 0, 24), null, 'one chunk of the range is missing');
  assert.deepEqual(net.gets(), [], 'a put or a get never reads the disc from the network');
});

test('the helper ignores what is not a disc file and keeps the file\'s last short chunk', async () => {
  const serve = syncReadHelper(fakeNetwork(), helperOpts(fakeCaches()));
  const sab = new SharedArrayBuffer(8 + 8);
  await serve({ op: 'get', url: 'http://x/page.html', start: 0, end: 8, sab });
  assert.equal(new Int32Array(sab)[0], 0, 'no answer for a page: the worker never asks for one');
  await serve({ op: 'put', url: 'http://x/data/game.iso', start: 16, bytes: FILE.slice(16, 20) });
  assert.deepEqual(await get(serve, 16, 20), [...FILE.slice(16, 20)]);
});

test('the client treats a helper that never answers as absent: one short wait, then no more asks for a while', () => {
  let t = 1000;
  const posted = [];
  const client = syncReadClient((m) => posted.push(m.op), { waitMs: 20, pingMs: 20, pauseMs: 500, now: () => t });
  assert.equal(client.ask('http://x/data/game.iso', 0, 8), null);
  assert.deepEqual(posted, ['ping']);
  assert.equal(client.ask('http://x/data/game.iso', 0, 8), null);
  assert.deepEqual(posted, ['ping'], 'paused: nothing is posted');
  t += 600;
  assert.equal(client.ask('http://x/data/game.iso', 0, 8), null);
  assert.deepEqual(posted, ['ping', 'ping'], 'after the pause it tries again');
});

test('the client reads the bytes a helper wrote into the reply buffer, and a wrong length is a miss', () => {
  const reply = (status, bytes) => (m) => {
    if (m.op === 'ping') { Atomics.store(new Int32Array(m.sab), 0, 1); return; }
    const ctl = new Int32Array(m.sab, 0, 2);
    new Uint8Array(m.sab, 8).set(bytes);
    ctl[1] = bytes.length;
    Atomics.store(ctl, 0, status);
  };
  const url = 'http://x/data/game.iso';
  assert.deepEqual([...syncReadClient(reply(1, [1, 2, 3, 4]), { waitMs: 50 }).ask(url, 0, 4)], [1, 2, 3, 4]);
  assert.equal(syncReadClient(reply(2, [1, 2, 3, 4]), { waitMs: 50 }).ask(url, 0, 4), null);
  assert.equal(syncReadClient(reply(1, [1, 2, 3]), { waitMs: 50 }).ask(url, 0, 4), null);
});

class FakeXHR {
  constructor() { FakeXHR.made.push(this); this.sent = 0; this.headers = {}; }
  open(method, url, async) { this.method = method; this.url = url; this.async = async; }
  setRequestHeader(n, v) { this.headers[n] = v; }
  send() { this.sent++; this.status = 206; this.response = FILE.slice(0, 8).buffer; }
}
FakeXHR.made = [];

function xhrOf(client, url = 'http://x/data/game.iso', async = false, method = 'GET') {
  const X = syncDiscXHR(FakeXHR, client);
  const x = new X();
  x.open(method, url, async);
  x.responseType = 'arraybuffer';
  x.setRequestHeader('Range', 'bytes=0-7');
  return x;
}

test('a synchronous disc range GET is answered from the client\'s bytes without a request', () => {
  const asked = [];
  const x = xhrOf({ ask: (...a) => (asked.push(a), Uint8Array.from([9, 8, 7, 6, 5, 4, 3, 2])), remember: () => assert.fail('a hit is already cached') });
  x.send();
  assert.equal(x.sent, 0);
  assert.equal(x.status, 206);
  assert.deepEqual([...new Uint8Array(x.response)], [9, 8, 7, 6, 5, 4, 3, 2]);
  assert.deepEqual(asked.map(([u, a, b]) => [u.endsWith('/data/game.iso'), a, b]), [[true, 0, 8]]);
});

test('a miss reads the network as before and hands the client the bytes to keep', () => {
  const kept = [];
  const x = xhrOf({ ask: () => null, remember: (u, s, b) => kept.push([u, s, [...b]]) });
  x.send();
  assert.equal(x.sent, 1);
  assert.equal(x.status, 206);
  assert.deepEqual(kept.map(([u, s, b]) => [u.endsWith('/data/game.iso'), s, b]), [[true, 0, [0, 1, 2, 3, 4, 5, 6, 7]]]);
});

test('requests that are not synchronous disc range GETs never reach the client', () => {
  const never = { ask: () => assert.fail('asked'), remember: () => assert.fail('kept') };
  xhrOf(never, undefined, true).send();
  xhrOf(never, 'page.html').send();
  xhrOf(never, undefined, false, 'HEAD').send();
  const X = syncDiscXHR(FakeXHR, never);
  const x = new X();
  x.open('GET', 'http://x/data/game.iso', false);
  x.send();
  assert.equal(x.sent, 1);
});

test('the client hands the helper a copy of the bytes a worker fetched', () => {
  const posted = [];
  const client = syncReadClient((m) => posted.push(m));
  const bytes = FILE.slice(16, 20);
  client.remember('http://x/data/game.iso', 16, bytes);
  bytes.fill(0);
  assert.deepEqual(posted.map((m) => [m.op, m.start, [...m.bytes]]), [['put', 16, [...FILE.slice(16, 20)]]]);
});
