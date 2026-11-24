// ABOUTME: Tests for web/reload.js: the page reloads once the work before it (saves, the last diagnostics report) is done or a deadline
// ABOUTME: passes, and the deadline never waits on a timer, which a throttled page may not run. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';

const { reloadWhenDone } = createRequire(import.meta.url)('./reload.js');

// a clock and a task queue the test steps by hand: no timers anywhere
function fakeLoop() {
  const loop = { time: 0, queue: [], reloads: 0 };
  loop.now = () => loop.time;
  loop.post = (cb) => { loop.queue.push(cb); };
  loop.reload = () => { ++loop.reloads; };
  loop.step = (ms) => { loop.time += ms; const run = loop.queue.splice(0); run.forEach((cb) => cb()); };
  return loop;
}
const flush = () => new Promise((resolve) => setImmediate(resolve));

test('the page reloads as soon as the work is done', async () => {
  const loop = fakeLoop();
  reloadWhenDone(Promise.resolve(), { ...loop, ms: 2000 });
  await flush();
  assert.equal(loop.reloads, 1);
});

test('a failed piece of work still reloads', async () => {
  const loop = fakeLoop();
  reloadWhenDone(Promise.reject(new Error('disk full')), { ...loop, ms: 2000 });
  await flush();
  assert.equal(loop.reloads, 1);
});

test('work that never ends reloads once the deadline passes, driven by posted tasks and not by timers', async () => {
  const loop = fakeLoop();
  reloadWhenDone(new Promise(() => {}), { ...loop, ms: 2000 });
  loop.step(1999);
  assert.equal(loop.reloads, 0);
  loop.step(1);
  await flush();
  assert.equal(loop.reloads, 1);
  loop.step(5000);
  assert.equal(loop.reloads, 1);
});

test('work that ends after the deadline does not reload a second time', async () => {
  const loop = fakeLoop();
  let finish;
  reloadWhenDone(new Promise((resolve) => { finish = resolve; }), { ...loop, ms: 2000 });
  loop.step(2000);
  finish();
  await flush();
  assert.equal(loop.reloads, 1);
});

test('the reload does not use setTimeout or setInterval, so a starved timer cannot hold it', () => {
  const source = readFileSync(new URL('./reload.js', import.meta.url), 'utf8');
  assert.doesNotMatch(source, /setTimeout|setInterval/);
});

test('boot.js reloads through reloadWhenDone for QUIT and for Leave the match, and the page loads reload.js first', () => {
  const boot = readFileSync(new URL('./boot.js', import.meta.url), 'utf8');
  assert.doesNotMatch(boot, /\.finally\(\(\) => location\.reload\(\)\)/);
  assert.equal((boot.match(/wotmReloadWhenDone\(/g) ?? []).length, 2);
  const page = readFileSync(new URL('./index.html', import.meta.url), 'utf8');
  assert.ok(page.indexOf('<script src="reload.js">') >= 0 && page.indexOf('<script src="reload.js">') < page.indexOf('<script src="boot.js">'));
});

test('within gives work its own limit: it settles when the work does or when the limit passes, whichever is first', async () => {
  const { within } = createRequire(import.meta.url)('./reload.js');
  const loop = fakeLoop();
  let done = false;
  within(new Promise(() => {}), 300, loop).then(() => { done = true; });
  loop.step(299);
  await flush();
  assert.equal(done, false);
  loop.step(1);
  await flush();
  assert.equal(done, true);
});

test('the save queue gets longer than the diagnostics report: QUIT reloads at the diag limit only when the saves are done', async () => {
  const { reloadWhenDone, within } = createRequire(import.meta.url)('./reload.js');
  const loop = fakeLoop();
  let saved;
  const saves = new Promise((resolve) => { saved = resolve; });
  reloadWhenDone(Promise.all([saves, within(new Promise(() => {}), 2000, loop)]), { ...loop, ms: 10000 });
  loop.step(2500);
  await flush();
  assert.equal(loop.reloads, 0, 'the saves are still being written');
  saved();
  await flush();
  assert.equal(loop.reloads, 1);
});

test('boot.js limits only the diagnostics report to 2 s and gives the saves 10 s', () => {
  const boot = readFileSync(new URL('./boot.js', import.meta.url), 'utf8');
  assert.match(boot, /wotmReloadWhenDone\(Promise\.all\(\[globalThis\.wotmSaveQueue, window\.wotmWithin\(finishDiag\(\), 2000\)\]\), \{ ms: 10000 \}\)/);
});
