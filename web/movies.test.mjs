// ABOUTME: Tests for web/movies.js with a fake video, overlay and window: modes, the manifest, skipping by key, tap and gamepad, what an
// ABOUTME: original (unskippable) movie ignores, failures, and the audio hold. Run: node --test web/*.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { normalizeMode, isSkipKey, parseManifest, newGamepadPress, installAudioHold, holdGamepads, playStartupMovies } from './movies.js';

const MANIFEST = { version: 1, codec: 'vp9+opus', movies: [{ name: 'intro_m', file: 'intro_m.webm' }, { name: 'int_main', file: 'int_main.webm' }] };
const ok = (body) => async () => ({ ok: true, json: async () => body });

class Video extends EventTarget {
  constructor({ refuse = 0 } = {}) { super(); this.srcs = []; this.refuse = refuse; this.muted = false; this.plays = 0; }
  set src(v) { this.srcs.push(v); }
  get src() { return this.srcs.at(-1); }
  removeAttribute() { this.srcs.push(null); }
  load() {}
  async play() { this.plays++; if (this.refuse-- > 0) throw new DOMException('no', 'NotAllowedError'); }
  fire(name) { this.dispatchEvent(new Event(name)); }
}
const makeEnv = (videoOptions) => {
  const win = Object.assign(new EventTarget(), { frames: [], requestAnimationFrame(f) { this.frames.push(f); } });
  const overlay = Object.assign(new EventTarget(), { hidden: true });
  const hint = { hidden: true };
  const video = new Video(videoOptions);
  const nav = { getGamepads: () => [] };
  const held = [];
  return { win, nav, overlay, hint, video, held, audio: { hold: (on) => held.push(on) } };
};
const key = (type, k, extra = {}) => Object.assign(new Event(type, { cancelable: true }), { key: k, ...extra });
const tick = () => new Promise((r) => setTimeout(r, 0));
const start = (env, mode, extra = {}) => playStartupMovies({
  mode, fetchImpl: ok(MANIFEST), overlay: env.overlay, video: env.video, hint: env.hint, win: env.win, audio: env.audio, nav: env.nav, ...extra,
});

test('modes: anything unknown is the default, skippable', () => {
  assert.equal(normalizeMode('original'), 'original');
  assert.equal(normalizeMode('off'), 'off');
  assert.equal(normalizeMode('nope'), 'skippable');
  assert.equal(normalizeMode(null), 'skippable');
});

test('Esc, Space and Enter skip; a held key and other keys do not', () => {
  assert.ok(isSkipKey(key('keydown', 'Escape')));
  assert.ok(isSkipKey(key('keydown', ' ')));
  assert.ok(isSkipKey(key('keydown', 'Enter')));
  assert.ok(!isSkipKey(key('keydown', 'Enter', { repeat: true })));
  assert.ok(!isSkipKey(key('keyup', 'Enter')));
  assert.ok(!isSkipKey(key('keydown', 'a')));
});

test('the manifest lists plain .webm files only', () => {
  assert.deepEqual(parseManifest(MANIFEST).map((m) => m.file), ['intro_m.webm', 'int_main.webm']);
  for (const bad of [null, {}, { version: 2, movies: [] }, { version: 1, movies: [] }, { version: 1, movies: [{ file: '../x.webm' }] },
    { version: 1, movies: [{ file: 'a/b.webm' }] }, { version: 1, movies: [{ file: 'x.mp4' }] }, { version: 1, movies: [{}] }])
    assert.throws(() => parseManifest(bad), /movie/);
});

test('a gamepad press is a button that was not down at the last look', () => {
  const pad = (down) => ({ index: 0, buttons: down.map((pressed) => ({ pressed })) });
  const seen = new Map();
  assert.equal(newGamepadPress([pad([false, false])], seen), false);
  assert.equal(newGamepadPress([pad([true, false])], seen), true);
  assert.equal(newGamepadPress([pad([true, false])], seen), false);   // still held
  assert.equal(newGamepadPress([pad([true, true])], seen), true);
  assert.equal(newGamepadPress([null, undefined], seen), false);
  assert.equal(newGamepadPress([pad([true, true])], seen), true);     // unplugged and back: a new pad
});

test('off fetches nothing and shows nothing', async () => {
  const env = makeEnv();
  let fetched = 0;
  const r = await start(env, 'off', { fetchImpl: async () => { fetched++; return { ok: true }; } });
  assert.equal(r, 'off');
  assert.equal(fetched, 0);
  assert.equal(env.overlay.hidden, true);
  assert.deepEqual(env.held, []);
});

test('no movie files on the server is not an error and shows nothing', async () => {
  const env = makeEnv();
  const logs = [];
  const r = await start(env, 'skippable', { fetchImpl: async () => ({ ok: false, status: 404 }), log: (t) => logs.push(t) });
  assert.equal(r, 'unavailable');
  assert.equal(env.overlay.hidden, true);
  assert.match(logs[0], /404/);
  assert.equal(await start(makeEnv(), 'skippable', { fetchImpl: async () => { throw new Error('offline'); } }), 'unavailable');
  assert.equal(await start(makeEnv(), 'skippable', { fetchImpl: ok({ version: 9 }) }), 'unavailable');
});

test('both movies play in order, the overlay shows meanwhile and the audio is held', async () => {
  const env = makeEnv();
  const run = start(env, 'skippable');
  await tick();
  assert.equal(env.overlay.hidden, false);
  assert.equal(env.hint.hidden, false);
  assert.deepEqual(env.video.srcs, ['data/movies/intro_m.webm']);
  env.video.fire('ended');
  await tick();
  assert.equal(env.video.src, 'data/movies/int_main.webm');
  env.video.fire('ended');
  assert.deepEqual(await run, ['ended', 'ended']);
  assert.equal(env.overlay.hidden, true);
  assert.equal(env.video.src, null);
  assert.deepEqual(env.held, [true, false]);
});

test('in skippable mode Esc skips one movie at a time, and the keys never reach the game', async () => {
  const env = makeEnv();
  const run = start(env, 'skippable');
  await tick();
  let leaked = 0;
  env.win.addEventListener('keydown', () => leaked++);   // the game's handler, behind the page's capture
  const a = key('keydown', 'a');
  env.win.dispatchEvent(a);
  assert.equal(a.defaultPrevented, true);
  assert.equal(leaked, 0);
  assert.equal(env.video.srcs.length, 1);   // 'a' does not skip
  env.win.dispatchEvent(key('keydown', 'Escape'));
  await tick();
  assert.equal(env.video.srcs.length, 2);
  env.win.dispatchEvent(key('keydown', 'Enter'));
  assert.deepEqual(await run, ['skipped', 'skipped']);
  assert.equal(env.overlay.hidden, true);
  assert.equal(leaked, 0);   // not the skipping keys either
});

test('a tap on the overlay skips', async () => {
  const env = makeEnv();
  const run = start(env, 'skippable');
  await tick();
  env.overlay.dispatchEvent(new Event('pointerdown'));
  await tick();
  env.overlay.dispatchEvent(new Event('pointerdown'));
  assert.deepEqual(await run, ['skipped', 'skipped']);
});

test('a new gamepad button press skips; one held since before the movie does not', async () => {
  const env = makeEnv();
  let down = true;
  const pads = () => [{ index: 0, buttons: [{ pressed: down }] }];
  env.nav.getGamepads = pads;
  const run = start(env, 'skippable');
  await tick();
  env.win.frames.shift()();   // still held: nothing
  assert.equal(env.video.srcs.length, 1);
  down = false;
  env.win.frames.shift()();
  down = true;
  env.win.frames.shift()();   // pressed again: skips the first
  await tick();
  assert.equal(env.video.srcs.length, 2);
  down = false;
  env.win.frames.shift()();
  down = true;
  env.win.frames.shift()();
  assert.deepEqual(await run, ['skipped', 'skipped']);
});

test('original cannot be skipped by a key, a tap or a gamepad, and shows no hint', async () => {
  const env = makeEnv();
  let down = false;
  env.nav.getGamepads = () => [{ index: 0, buttons: [{ pressed: down }] }];
  const run = start(env, 'original');
  await tick();
  assert.equal(env.hint.hidden, true);
  for (const k of ['Escape', ' ', 'Enter']) {
    const e = key('keydown', k);
    env.win.dispatchEvent(e);
    assert.equal(e.defaultPrevented, true);   // swallowed, not skipped
  }
  env.overlay.dispatchEvent(new Event('pointerdown'));
  down = true;
  env.win.frames.shift()();
  await tick();
  assert.equal(env.video.srcs.length, 1);
  env.video.fire('ended');
  await tick();
  env.video.fire('ended');
  assert.deepEqual(await run, ['ended', 'ended']);
});

test('the game sees no gamepads while a movie plays, and the real function is back after', async () => {
  const env = makeEnv();
  const real = () => [{ index: 0, buttons: [{ pressed: false }] }];
  env.nav.getGamepads = real;
  const run = start(env, 'original');
  await tick();
  assert.deepEqual(env.nav.getGamepads(), []);
  env.video.fire('ended');
  await tick();
  env.video.fire('ended');
  await run;
  assert.equal(env.nav.getGamepads, real);
});

test('a navigator without the gamepad API, or one that throws, does not stop the movie', async () => {
  assert.deepEqual(holdGamepads({}).read(), []);
  const env = makeEnv();
  env.nav = {};
  const run = start(env, 'skippable');
  await tick();
  env.video.fire('ended');
  await tick();
  env.video.fire('ended');
  assert.deepEqual(await run, ['ended', 'ended']);
});

test('a refused play() of a movie already skipped does not mute or disturb the next one', async () => {
  const env = makeEnv();
  let rejectFirst;
  env.video.play = function () {
    this.plays++;
    return this.plays === 1 ? new Promise((_, reject) => { rejectFirst = reject; }) : Promise.resolve();
  };
  const run = start(env, 'skippable');
  await tick();
  env.win.dispatchEvent(key('keydown', 'Escape'));   // skipped while the first play() is still pending
  await tick();
  assert.equal(env.video.src, 'data/movies/int_main.webm');
  rejectFirst(new DOMException('aborted', 'AbortError'));
  await tick();
  assert.equal(env.video.muted, false);
  assert.equal(env.video.plays, 2);
  env.win.dispatchEvent(key('keydown', 'Enter'));   // the second movie can still be skipped
  assert.deepEqual(await run, ['skipped', 'skipped']);
});

test('a browser that refuses sound plays the picture muted; one that refuses everything skips the movie', async () => {
  const env = makeEnv({ refuse: 1 });
  const run = start(env, 'skippable');
  await tick();
  assert.equal(env.video.muted, true);
  assert.equal(env.video.plays, 2);
  env.video.fire('ended');
  await tick();
  env.video.fire('ended');
  assert.deepEqual(await run, ['ended', 'ended']);
  assert.deepEqual(await start(makeEnv({ refuse: 99 }), 'skippable'), ['failed', 'failed']);
});

test('a movie that errors, or never starts, is passed over', async () => {
  const env = makeEnv();
  const run = start(env, 'original');
  await tick();
  env.video.fire('error');
  await tick();
  env.video.fire('playing');
  env.video.fire('ended');
  assert.deepEqual(await run, ['failed', 'ended']);
  assert.deepEqual(await start(makeEnv(), 'original', { startTimeoutMs: 5 }), ['failed', 'failed']);
});

test('a movie that starts and then stops for good is passed over', async () => {
  const env = makeEnv();
  const run = start(env, 'original', { startTimeoutMs: 1000, stallTimeoutMs: 5 });
  await tick();
  env.video.fire('playing');
  env.video.fire('timeupdate');
  await new Promise((r) => setTimeout(r, 40));   // silence for longer than the stall limit
  env.video.fire('ended');
  assert.deepEqual(await run, ['failed', 'ended']);
});

test('the key capture is removed when the movies end', async () => {
  const env = makeEnv();
  const run = start(env, 'skippable');
  await tick();
  env.video.fire('ended');
  await tick();
  env.video.fire('ended');
  await run;
  const e = key('keydown', 'Escape');
  env.win.dispatchEvent(e);
  assert.equal(e.defaultPrevented, false);
});

test('the audio hold suspends a context made before or during a movie and releases it after', async () => {
  const calls = [];
  class FakeContext {
    suspend() { calls.push(['suspend', this.id]); return Promise.resolve(); }
    resume() { calls.push(['resume', this.id]); return Promise.resolve(); }
    constructor() { this.id = FakeContext.n = (FakeContext.n ?? 0) + 1; }
  }
  const win = { AudioContext: FakeContext };
  const hold = installAudioHold(win);
  const early = new win.AudioContext();
  hold.hold(true);
  const late = new win.AudioContext();   // made while held
  await early.resume();                  // the game's own resume() does nothing while held
  await late.resume();
  assert.deepEqual(calls, [['suspend', 1], ['suspend', 2]]);
  hold.hold(false);
  assert.deepEqual(calls.slice(2), [['resume', 1], ['resume', 2]]);
  await early.resume();
  assert.deepEqual(calls.at(-1), ['resume', 1]);
});

test('without audio contexts the hold does nothing', () => {
  installAudioHold({}).hold(true);
});
