// ABOUTME: Tests for web/disc_cache.js with an in-memory Cache API and a fake network: disc chunks kept per file version and verified
// ABOUTME: by hash, cache misses and failures falling back to the network, old versions evicted, and the boot prefetch. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { cachingFetch, evictOld, prefetch } = createRequire(import.meta.url)('./disc_cache.js');

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
