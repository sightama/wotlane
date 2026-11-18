// ABOUTME: Tests for web/match_prefetch.js: the disc chunks a lobby's match loads (stage, each member's monster and costume) and the
// ABOUTME: warmer that fetches them once the lobby's choices hold still, dropping a superseded choice. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { costumesFor, matchChunkList, matchOfEnv, createMatchWarmer, canKeepChunks } = createRequire(import.meta.url)('./match_prefetch.js');

const TABLE = {
  stages: { 1: [10, 11], 4: [12, 13] },
  monsters: {
    1: { all: [20], costumes: [[21], [22], [23], [24]] },
    13: { all: [30], costumes: [[31], [32]] },
  },
};

test('costumes go as the game hands them out: asked ones first, then the lowest free one of the same monster', () => {
  const m = (monster, skin = '') => ({ monster, skin });
  assert.deepEqual(costumesFor([m(1), m(1, '0'), m(1)]), [1, 0, 2]);
  assert.deepEqual(costumesFor([m(1, '2'), m(1, '2')]), [2, 0]);   // a costume someone already asked for: automatic
  assert.deepEqual(costumesFor([m(13), m(13)]), [1, 2]);   // Goliath starts at costume 1
  assert.deepEqual(costumesFor([m(1), m(1), m(1), m(1), m(1)]), [0, 1, 2, 3, 0]);   // past four, the costumes repeat
  assert.deepEqual(costumesFor([m(1, 'myskin')]), [null]);   // a creation's skin: no disc costume
});

test('a match needs its stage, each monster\'s shared files and each member\'s costume', () => {
  assert.deepEqual(matchChunkList(TABLE, '1', [{ monster: 1, skin: '' }, { monster: 1, skin: '3' }]), [10, 11, 20, 21, 24]);
  assert.deepEqual(matchChunkList(TABLE, 'mymap', [{ monster: 13, skin: '' }]), [30, 32]);   // a map of the player's own: monsters only
  assert.deepEqual(matchChunkList(TABLE, '4', [{ monster: 99, skin: '' }]), [12, 13]);   // a monster the table lacks: nothing for it
});

test('a match environment\'s WOTM_MATCH names the members and the stage', () => {
  assert.deepEqual(matchOfEnv('3:1,1,5,9'), { stage: '9', members: [{ monster: 3, skin: '1' }, { monster: 1, skin: '' }, { monster: 5, skin: '' }] });
  assert.equal(matchOfEnv(''), null);
});

function harness() {
  const timers = [];   // every timer ever set; a cleared or fired one has no fn
  const calls = [];
  const warmer = createMatchWarmer({
    table: async () => TABLE,
    prefetch: (chunks, opts) => { calls.push({ chunks, opts }); return new Promise(() => {}); },
    delayMs: 1000,
    setTimer: (fn, ms) => { timers.push({ fn, ms }); return timers.length; },
    clearTimer: (id) => { if (id) timers[id - 1].fn = null; },
  });
  const fire = async () => { for (const t of timers) { const fn = t.fn; t.fn = null; fn?.(); } await new Promise((r) => setImmediate(r)); };
  return { warmer, calls, fire, timers };
}

test('the warmer waits for the choices to hold still, then fetches a few chunks at a time', async () => {
  const { warmer, calls, fire, timers } = harness();
  warmer.want('1', [{ monster: 1, skin: '' }]);
  warmer.want('4', [{ monster: 1, skin: '' }]);   // the owner changed the stage at once: only the last choice is fetched
  assert.equal(timers.filter((t) => t.fn).length, 1);
  assert.equal(timers.at(-1).ms, 1000);
  await fire();
  assert.equal(calls.length, 1);
  assert.deepEqual(calls[0].chunks, [12, 13, 20, 21]);
  assert.deepEqual([calls[0].opts.concurrency, calls[0].opts.maxRun], [4, 1]);
});

test('a new choice stops the fetch of the one it replaces, and stop() ends it all', async () => {
  const { warmer, calls, fire } = harness();
  warmer.want('1', []);
  await fire();
  const first = calls[0].opts.stop;
  assert.equal(first(), false);
  warmer.want('4', []);
  assert.equal(first(), true);
  await fire();
  assert.equal(calls[1].opts.stop(), false);
  warmer.stop();
  assert.equal(calls[1].opts.stop(), true);
  warmer.want('1', []);   // after stop() nothing more is fetched
  await fire();
  assert.equal(calls.length, 2);
});

test('the game\'s boot asks with no wait', async () => {
  const { warmer, timers } = harness();
  warmer.want('1', [], 0);
  assert.equal(timers.at(-1).ms, 0);
});

test('the same choice again does not start over', async () => {
  const { warmer, calls, fire } = harness();
  warmer.want('1', [{ monster: 1, skin: '' }]);
  await fire();
  warmer.want('1', [{ monster: 1, skin: '' }]);
  await fire();
  assert.equal(calls.length, 1);
  assert.equal(calls[0].opts.stop(), false);
});

test('nothing is prefetched where the chunks could not be kept: no Cache API, one that throws, or storage that would pass 90%', async () => {
  const caches = { keys: async () => [] };
  const roomy = async () => ({ usage: 0, quota: 1 << 30 });
  assert.equal(await canKeepChunks({ caches, estimate: roomy }, 40), true);
  assert.equal(await canKeepChunks({ caches: undefined, estimate: roomy }, 40), false);
  assert.equal(await canKeepChunks({ caches: { keys: async () => { throw new Error('private window'); } }, estimate: roomy }, 40), false);
  assert.equal(await canKeepChunks({ caches, estimate: async () => ({ usage: 900 << 20, quota: 1 << 30 }) }, 40), false);
  assert.equal(await canKeepChunks({ caches, estimate: null }, 40), true);   // no estimate: the cache's own check decides
});
