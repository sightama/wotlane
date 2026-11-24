// ABOUTME: Tests for web/game_prefetch.js: the whole game's disc chunks (every stage, monster and costume, and the boot's) and the
// ABOUTME: background warmer that fetches what the cache lacks, pausing while the game reads the disc. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { wholeGameChunks, createGameWarmer } = createRequire(import.meta.url)('./game_prefetch.js');

const TABLE = {
  stages: { 1: [10, 11], 4: [11, 12] },
  monsters: { 1: { all: [20], costumes: [[21], [22]] }, 13: { all: [30], costumes: [[31], [12]] } },
};
const BOOT = { 'SCUS_971.97': [0, 1], 'game.iso': [0, 23] };

test('the whole game is every stage, every monster with all its costumes and the boot\'s chunks, each once and in order', () => {
  assert.deepEqual(wholeGameChunks(TABLE, BOOT), { 'game.iso': [0, 10, 11, 12, 20, 21, 22, 23, 30, 31], 'SCUS_971.97': [0, 1] });
  assert.deepEqual(wholeGameChunks({}, {}), { 'game.iso': [], 'SCUS_971.97': [] });
});

// A fake disc cache and clock: `prefetch` stores each chunk it is asked for (unless `failing`: true, or a test of the chunk number)
// and checks `stop()` before each one,
// as disc_cache.js prefetch does with its runs; `during(file, chunk)` runs before a chunk is fetched (a test makes the game read there)
function fakeWorld({ cached = {}, failing = false, room = () => true, during = () => {} } = {}) {
  const w = { t: 0, calls: [], kept: Object.fromEntries(Object.entries(cached).map(([f, l]) => [f, new Set(l)])) };
  w.opts = {
    files: async () => wholeGameChunks(TABLE, BOOT),
    missing: async (file, chunks) => chunks.filter((c) => !w.kept[file]?.has(c)),
    canKeep: async (count) => { w.askedRoom = count; return room(count); },
    prefetch: async (file, chunks, { stop }) => {
      const got = [];
      for (const c of chunks) {
        if (stop()) break;
        during(file, c);
        got.push(c);
        if (!(failing === true || (failing && failing(c)))) (w.kept[file] ??= new Set()).add(c);
      }
      w.calls.push([file, got]);
      return got.length;   // as disc_cache.js prefetch returns the runs it asked for
    },
    now: () => w.t,
    sleep: async (ms) => { w.t += ms; w.onSleep?.(); },
    quietMs: 1000,
  };
  return w;
}

test('it fetches only what the cache lacks, file by file, after asking whether there is room for it', async () => {
  const w = fakeWorld({ cached: { 'game.iso': [0, 10, 11, 12, 20, 21, 22] } });
  assert.equal(await createGameWarmer(w.opts).start(), 'done');
  assert.deepEqual(w.calls, [['game.iso', [23, 30, 31]], ['SCUS_971.97', [0, 1]]]);
  assert.equal(w.askedRoom, 5);
  w.calls.length = 0;
  assert.equal(await createGameWarmer(w.opts).start(), 'done', 'all cached: nothing to fetch');
  assert.deepEqual(w.calls, []);
});

test('without room for the missing chunks nothing is fetched', async () => {
  const w = fakeWorld({ room: () => false });
  assert.equal(await createGameWarmer(w.opts).start(), 'no room');
  assert.deepEqual(w.calls, []);
});

test('a game read pauses it until the disc has been quiet for a while, then it goes on with what is still missing', async () => {
  let warmer, readAt = null;
  const w = fakeWorld({
    during: (file, c) => {
      if (file === 'game.iso' && c === 20 && readAt === null) { readAt = w.t; warmer.activity(); }
    },
  });
  warmer = createGameWarmer(w.opts);
  assert.equal(await warmer.start(), 'done');
  assert.deepEqual(w.calls[0], ['game.iso', [0, 10, 11, 12, 20]], 'the chunk being fetched finishes, nothing after it starts');
  assert.deepEqual(w.calls[1], ['game.iso', [21, 22, 23, 30, 31]]);
  assert.ok(w.t - readAt >= 1000, 'it waited for the quiet');
  assert.deepEqual([...w.kept['SCUS_971.97']], [0, 1]);
});

test('hold() pauses it until release(), however quiet the disc is; then the quiet wait applies as after a read', async () => {
  let warmer, heldAt = null;
  const w = fakeWorld({
    during: (file, c) => {
      if (c === 11 && heldAt === null) {
        heldAt = w.t;
        warmer.hold();
        // the hold ends a minute later, as if the game's first load took that long
        w.onSleep = () => { if (w.t - heldAt >= 60000 && w.onSleep) { w.onSleep = null; warmer.release(); } };
      }
    },
  });
  warmer = createGameWarmer(w.opts);
  assert.equal(await warmer.start(), 'done');
  assert.deepEqual(w.calls[0], ['game.iso', [0, 10, 11]]);
  assert.ok(w.t - heldAt >= 61000, `held a minute, then quiet for a second (${w.t - heldAt} ms)`);
});

test('stop() ends it for good: nothing more is fetched', async () => {
  let warmer;
  const w = fakeWorld({ during: (file, c) => { if (c === 11) warmer.stop(); } });
  warmer = createGameWarmer(w.opts);
  assert.equal(await warmer.start(), 'stopped');
  assert.deepEqual(w.calls, [['game.iso', [0, 10, 11]]]);
  assert.equal(await warmer.start(), 'stopped', 'a stopped warmer does not start again');
  assert.equal(w.calls.length, 1);
});

test('a whole pass that kept nothing ends it at once: the set is not downloaded again for nothing', async () => {
  const w = fakeWorld({ failing: true });
  assert.equal(await createGameWarmer(w.opts).start(), 'gave up');
  assert.equal(w.calls.filter(([file]) => file === 'game.iso').length, 1);
});

test('passes the game keeps interrupting that keep nothing end it too', async () => {
  let warmer;
  const w = fakeWorld({ failing: true, during: (file, c) => { if (c === 11) warmer.activity(); } });
  warmer = createGameWarmer(w.opts);
  assert.equal(await warmer.start(), 'gave up');
  assert.equal(w.calls.length, 1, 'the interrupted pass asked for chunks and kept none: no second pass');
});

test('a pass that kept some goes on with the rest, and ends when a pass keeps no more', async () => {
  const w = fakeWorld({ failing: (c) => c === 31 });
  assert.equal(await createGameWarmer(w.opts).start(), 'gave up');
  assert.deepEqual(w.calls.filter(([file]) => file === 'game.iso').map(([, got]) => got), [[0, 10, 11, 12, 20, 21, 22, 23, 30, 31], [31]]);
});

test('room is asked before every pass: storage that filled up meanwhile ends it', async () => {
  let warmer, asked = 0;
  const w = fakeWorld({ room: () => ++asked === 1, during: (file, c) => { if (c === 20 && asked === 1) warmer.activity(); } });
  warmer = createGameWarmer(w.opts);
  assert.equal(await warmer.start(), 'no room');
  assert.equal(asked, 2);
  assert.deepEqual(w.calls, [['game.iso', [0, 10, 11, 12, 20]]]);
});

test('a missing chunk table means no prefetch', async () => {
  const w = fakeWorld();
  w.opts.files = async () => { throw new Error('match_chunks.json: HTTP 404'); };
  assert.equal(await createGameWarmer(w.opts).start(), 'no table (match_chunks.json: HTTP 404)');
  assert.deepEqual(w.calls, []);
});
