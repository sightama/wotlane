// ABOUTME: Tests for web/data_retry.js: disc requests (data/) are retried on network errors and server errors, other requests are
// ABOUTME: left alone, and a disc request that keeps failing gives up after the last try. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { retryingFetch } = createRequire(import.meta.url)('./data_retry.js');

function fakeFetch(results) {
  const calls = [];
  const fn = async (url, init) => {
    calls.push(url);
    const r = results[Math.min(calls.length - 1, results.length - 1)];
    if (r instanceof Error) throw r;
    return { ok: r >= 200 && r < 300, status: r };
  };
  fn.calls = calls;
  return fn;
}

const fast = { tries: 4, delayMs: 1 };

test('a disc request that fails twice then works is retried', async () => {
  const f = fakeFetch([new TypeError('network'), 503, 206]);
  const r = await retryingFetch(f, fast)('data//game.iso', {});
  assert.equal(r.status, 206);
  assert.equal(f.calls.length, 3);
});

test('a disc request that keeps failing gives up after the last try', async () => {
  const f = fakeFetch([new TypeError('network')]);
  await assert.rejects(retryingFetch(f, fast)('data//game.iso', {}), TypeError);
  assert.equal(f.calls.length, 4);
});

test('a missing file (404) is not retried', async () => {
  const f = fakeFetch([404]);
  const r = await retryingFetch(f, fast)('data//nothing.bin', {});
  assert.equal(r.status, 404);
  assert.equal(f.calls.length, 1);
});

test('requests outside data/ are left alone', async () => {
  const f = fakeFetch([new TypeError('network')]);
  await assert.rejects(retryingFetch(f, fast)('wotm.wasm', {}), TypeError);
  assert.equal(f.calls.length, 1);
});

test('each disc chunk that arrives reports its size, other requests do not', async () => {
  const sizes = [];
  const f = async (url) => ({ ok: true, status: 206, headers: new Map([['content-length', '1048576']]) });
  const fetchDisc = retryingFetch(f, { ...fast, onBytes: (n) => sizes.push(n) });
  await fetchDisc('data//game.iso', {});
  await fetchDisc('wotm.wasm', {});
  assert.deepEqual(sizes, [1048576]);
});

test('a size probe (HEAD with an open range: a 206 with the whole file length) is not counted as downloaded', async () => {
  const sizes = [];
  const f = async () => ({ ok: true, status: 206, headers: new Map([['content-length', '1258815488']]) });
  await retryingFetch(f, { ...fast, onBytes: (n) => sizes.push(n) })('data//game.iso', { method: 'HEAD', headers: { Range: 'bytes=0-' } });
  assert.deepEqual(sizes, []);
});
