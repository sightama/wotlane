// ABOUTME: Tests for web/diag.js: the diagnostics report an online match sends (what is measured, how it is written as a DIAG line, what
// ABOUTME: the device description keeps and leaves out) with fake timers, frames and audio. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  SCHEMA, diagValue, diagLine, browserName, deviceFields, FrameWatch, LongTaskWatch, installAudioWatch, readGame, latencySource, windowFields, gpuName,
  TaskLagWatch,
  DiagReporter, joinFields, reportFields,
} from './diag.js';

const fieldsOf = (line) => Object.fromEntries(line.split(' ').slice(1).map((w) => w.split('=')).map(([k, ...v]) => [k, v.join('=')]));

test('a value keeps only the characters the server accepts, at most 48', () => {
  assert.equal(diagValue('Chrome 131 (x)'), 'Chrome-131--x-');
  assert.equal(diagValue('a'.repeat(60)).length, 48);
  assert.equal(diagValue(''), '-');
  assert.equal(diagValue(12.5), '12.5');
});

test('a DIAG line has key=value words, skips what is missing, and carries the note percent-encoded on one line', () => {
  const line = diagLine([['v', 1], ['why', 'report'], ['fps', undefined], ['gfps', NaN], ['late', 0], ['x', null], ['name', '']], 'it lagged\nbadly 100% "sure"');
  assert.equal(line, 'DIAG v=1 why=report late=0 note=it%20lagged%20badly%20100%25%20%22sure%22');
  assert.ok(!/[\s]/.test(line.slice(5).replace(/ /g, '')));
  assert.equal(diagLine([['v', 1]]), 'DIAG v=1');
  assert.ok(diagLine([['v', 1]], 'x'.repeat(500)).length < 5 + 4 + 6 + 200 * 3);   // the note is cut before it is encoded
});

const CHROME_ANDROID = {
  userAgentData: { brands: [{ brand: 'Not A(Brand', version: '8' }, { brand: 'Chromium', version: '131' }, { brand: 'Google Chrome', version: '131' }],
    mobile: true, platform: 'Android' },
  userAgent: 'Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 Chrome/131.0.0.0 Mobile Safari/537.36 PRIVATE-DETAIL',
  hardwareConcurrency: 8, deviceMemory: 4, connection: { effectiveType: '4g', downlink: 9.6, rtt: 100 },
};
const win = (nav, extra = {}) => ({ navigator: nav, devicePixelRatio: 2.625, innerWidth: 915, innerHeight: 412, matchMedia: () => ({ matches: true }), ...extra });

test('the browser is named from its brands, a computer or a phone, never from the full user agent', () => {
  assert.equal(browserName(CHROME_ANDROID), 'Chrome-131');
  assert.equal(browserName({ userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:132.0) Gecko/20100101 Firefox/132.0' }), 'Firefox-132');
  assert.equal(browserName({ userAgent: 'Mozilla/5.0 (Windows NT 10.0) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36 Edg/130.0.0.0' }), 'Edge-130');
  assert.equal(browserName({}), 'unknown');
});

test('the device description is the class of device: touch, cores, memory, pixel ratio, screen, connection, browser and system', () => {
  const f = Object.fromEntries(deviceFields(win(CHROME_ANDROID)));
  assert.deepEqual(f, { touch: 1, mobile: 1, os: 'Android', ua: 'Chrome-131', cores: 8, mem_gb: 4, dpr: '2.63', scr: '915x412', net: '4g', dl_mbps: 9.6, est_rtt_ms: 100 });
  assert.ok(!JSON.stringify(deviceFields(win(CHROME_ANDROID))).includes('PRIVATE-DETAIL'));
  const bare = Object.fromEntries(deviceFields({ navigator: { userAgent: 'Mozilla/5.0 (X11; Linux x86_64; rv:132.0) Gecko/20100101 Firefox/132.0' }, matchMedia: () => ({ matches: false }) }));
  assert.deepEqual(bare, { touch: 0, mobile: 0, os: 'Linux', ua: 'Firefox-132' });   // what the browser doesn't have is left out
});

test('frames: the 95th percentile and longest interval, and the late ones (over 25 ms); an interval across a visibility change is dropped', () => {
  const callbacks = [];
  const listeners = {};
  const watch = new FrameWatch({ raf: (cb) => callbacks.push(cb), doc: { addEventListener: (type, fn) => { listeners[type] = fn; } } });
  watch.start();
  let t = 1000;
  const frame = (dt) => { t += dt; callbacks.shift()(t); };
  frame(0);                                    // the first frame has no interval
  for (let i = 0; i < 18; ++i) frame(16.7);
  frame(40);
  frame(30);
  listeners.visibilitychange();
  frame(5000);                                 // the tab was hidden: not a late frame
  frame(16.7);
  const w = watch.take();
  assert.equal(w.n, 21);
  assert.equal(w.late, 2);
  assert.equal(w.max, 40);
  assert.ok(w.p95 >= 30 && w.p95 <= 40);
  assert.equal(watch.take(), null);            // nothing since the last take
  watch.stop();
  frame(16.7);
  assert.equal(callbacks.length, 0);           // stopped: no frame is asked for again
});

test('long tasks are counted where the browser reports them, and absent where it does not', () => {
  let observer;
  class FakeObserver {
    constructor(cb) { this.cb = cb; observer = this; }
    observe(options) { this.options = options; }
    disconnect() {}
  }
  const watch = new LongTaskWatch(FakeObserver);
  assert.deepEqual(observer.options, { entryTypes: ['longtask'] });
  observer.cb({ getEntries: () => [{ duration: 80 }, { duration: 120.5 }] });
  assert.deepEqual(watch.take(), { n: 2, ms: 200.5, max: 120.5 });
  assert.deepEqual(watch.take(), { n: 0, ms: 0, max: 0 });
  assert.equal(new LongTaskWatch(class { constructor() { throw new Error('unsupported entry type'); } }).take(), null);
  assert.equal(new LongTaskWatch(undefined).take(), null);
});

test('audio: callbacks later than 1.5 buffers are underruns; a page without script processors reports nothing', () => {
  let clock = 0;
  const nodes = [];
  class FakeContext {
    constructor() { this.state = 'running'; this.sampleRate = 48000; }
    createScriptProcessor(size) {
      const node = { bufferSize: size || 4096, handlers: [], addEventListener(type, fn) { if (type === 'audioprocess') this.handlers.push(fn); } };
      nodes.push(node);
      return node;
    }
  }
  const w = { AudioContext: FakeContext, performance: { now: () => clock } };
  const watch = installAudioWatch(w);
  const ctx = new w.AudioContext();
  const node = ctx.createScriptProcessor(2048);   // 42.67 ms per buffer at 48 kHz
  const fire = (afterMs) => { clock += afterMs; for (const h of node.handlers) h({}); };
  fire(0);
  fire(42.7);
  fire(43);
  fire(90);   // 2.1 buffers: an underrun
  fire(42);
  const a = watch.take();
  assert.equal(a.cb, 5);
  assert.equal(a.gaps, 1);
  assert.equal(Math.round(a.maxGapMs), 90);
  assert.equal(a.state, 'running');
  assert.equal(watch.take().cb, 0);
  assert.equal(installAudioWatch({ AudioContext: class {}, performance: { now: () => 0 } }).take(), null);
  assert.equal(installAudioWatch({}).take(), null);
});

test('the game counters are read from the wasm module and the presented-frame counter, and a game that is not there yet gives nothing', () => {
  const calls = [];
  const Module = { _wotm_web_net_stat: (i) => { calls.push(i); return [600, 3, 120.5, 4, 55][i]; } };
  assert.deepEqual(readGame({ Module, wotmFrames: { frames: 590, workMs: 2950.5 } }), { presented: 590, workMs: 2950.5, frame: 600, waits: 3, waitMs: 120.5, delay: 4, pingMs: 55 });
  assert.deepEqual(calls, [0, 1, 2, 3, 4]);   // reads only
  assert.deepEqual(readGame({}), { presented: undefined, workMs: undefined, frame: undefined, waits: undefined, waitMs: undefined, delay: undefined, pingMs: undefined });
});

test('a window is the difference of two samples: rates, the cost of a frame, waits for the network, ping and its jitter', () => {
  const prev = { t: 5000, presented: 300, workMs: 1500, frame: 300, waits: 1, waitMs: 40 };
  const cur = { t: 10000, presented: 590, workMs: 2950, frame: 575, waits: 4, waitMs: 340.4, delay: 4, pingMs: 70,
    pings: [50, 60, 50, 70], raf: { n: 290, p95: 22.4, max: 61.2, late: 7 }, lt: { n: 2, ms: 200.4, max: 120.4 },
    audio: { cb: 100, gaps: 2, maxGapMs: 99.6, state: 'running' }, hiddenMs: 1500 };
  const f = Object.fromEntries(windowFields(prev, cur));
  assert.deepEqual(f, {
    up_s: 10, fps: '58.0', gfps: '55.0', work_ms: '5.0', frame: 575, delay: 4, ping_ms: 70, ping_max_ms: 70, jitter_ms: '13.3',
    waits: 3, wait_ms: 300, waits_tot: 4, wait_tot_ms: 340, raf_p95_ms: '22.4', raf_max_ms: '61.2', late: 7, lt_n: 2, lt_ms: 200, lt_max_ms: 120,
    au_cb: 100, au_gaps: 2, au_max_ms: 100, au_state: 'running', hid_s: 2,
  });
});

test('a window missing its sources leaves their fields out; counters that start at zero need no earlier sample', () => {
  const f = Object.fromEntries(windowFields({ t: 0 }, { t: 5000, presented: 295, workMs: 1000, frame: 300 }));
  assert.deepEqual(f, { up_s: 5, fps: '59.0', gfps: '60.0', work_ms: '3.4', frame: 300 });
});

test('the input latency is read once per report from the wasm module (a take), and a game without it gives nothing', () => {
  const calls = [];
  const Module = {
    _wotm_web_latency_take: () => calls.push('take'),
    _wotm_web_latency: (i) => { calls.push(i); return [12, 151.4, 210, 260, 6.2, 99.5, 4, 3.1, 9.4, 1, 30.2][i]; },
  };
  assert.deepEqual(latencySource({ Module }).take(),
    { n: 12, total: 151.4, p95: 210, max: 260, sample: 6.2, delay: 99.5, stall: 4, sim: 3.1, present: 9.4, unknown: 1, blend: 30.2 });
  assert.deepEqual(calls, ['take', 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  assert.equal(latencySource({}).take(), null);
});

test('a window carries the input latency of the presses it saw, and none when there were none', () => {
  const lat = { n: 12, total: 151.44, p95: 210, max: 260, sample: 6.24, delay: 99.5, stall: 4, sim: 3.1, present: 9.4, unknown: 0, blend: 30.2 };
  const f = Object.fromEntries(windowFields({ t: 0 }, { t: 5000, lat }));
  assert.deepEqual(f, { up_s: 5, lat_n: 12, lat_ms: '151.4', lat_p95_ms: '210.0', lat_max_ms: '260.0', lat_in_ms: '6.2', lat_dl_ms: '99.5',
    lat_st_ms: '4.0', lat_sim_ms: '3.1', lat_pr_ms: '9.4', lat_bl_ms: '30.2' });
  assert.equal(Object.fromEntries(windowFields({ t: 0 }, { t: 5000, lat: { ...lat, unknown: 3 } })).lat_unk, 3);
  assert.deepEqual(Object.fromEntries(windowFields({ t: 0 }, { t: 5000, lat: { ...lat, n: 0 } })), { up_s: 5 });
});

test('a periodic report carries the latency taken for its window', () => {
  const r = reporterFor({ extra: { latency: { take: () => ({ n: 3, total: 120, p95: 130, max: 130, sample: 5, delay: 100, stall: 0, sim: 3, present: 12, unknown: 0, blend: 0 }) } } });
  r.reporter.begin();
  for (let i = 0; i < 10; ++i) { r.advance(500); r.t.tick(); }
  const first = fieldsOf(r.sent[0]);
  assert.deepEqual([first.lat_n, first.lat_ms, first.lat_dl_ms], ['3', '120.0', '100.0']);
});

function fakeTimers() {
  const timers = [];
  return {
    setTimer: (fn, ms) => { timers.push({ fn, ms }); return timers.length; },
    clearTimer: (id) => { timers[id - 1] = null; },
    tick: (n = 1) => { for (let i = 0; i < n; ++i) for (const t of timers) t?.fn(); },
    timers,
  };
}

function reporterFor({ game, extra = {} } = {}) {
  const sent = [];
  const stored = [];
  let clock = 0;
  const t = fakeTimers();
  const samples = game ?? [];
  const reporter = new DiagReporter({
    send: (line) => sent.push(line),
    base: [['build', 'wl-abc123'], ['lobby', 'QZXW'], ['name', 'Ana'], ['hz', 30], ['players', 2]],
    now: () => clock,
    setTimer: t.setTimer,
    clearTimer: t.clearTimer,
    store: { set: (v) => stored.push(v) },
    frames: { start() {}, stop() {}, take: () => ({ n: 150, p95: 17, max: 30, late: 1 }) },
    longTasks: { take: () => ({ n: 1, ms: 60, max: 60 }) },
    audio: { take: () => ({ cb: 50, gaps: 0, maxGapMs: 43, state: 'running' }) },
    game: () => samples.shift() ?? { presented: 0, frame: 0 },
    device: () => [['ua', 'Chrome-131'], ['cores', 8]],
    isHidden: () => false,
    ...extra,
  });
  return { reporter, sent, stored, t, advance: (ms) => { clock += ms; }, samples };
}

test('the reporter samples every 500 ms, reports every 5 s as periodic, and the last report at the end carries the device', () => {
  const r = reporterFor();
  r.reporter.begin();
  assert.equal(r.t.timers[0].ms, 500);
  for (let i = 0; i < 9; ++i) { r.advance(500); r.t.tick(); }
  assert.equal(r.sent.length, 0);                       // 4.5 s: not yet
  r.advance(500);
  r.samples.push({ presented: 290, workMs: 1450, frame: 290, waits: 2, waitMs: 80, delay: 3, pingMs: 60 });
  r.t.tick();
  assert.equal(r.sent.length, 1);
  const first = fieldsOf(r.sent[0]);
  assert.equal(first.v, String(SCHEMA));
  assert.equal(first.why, 'periodic');
  assert.equal(first.seq, '1');
  assert.deepEqual([first.build, first.lobby, first.name, first.hz, first.players], ['wl-abc123', 'QZXW', 'Ana', '30', '2']);
  assert.equal(first.fps, '58.0');
  assert.equal(first.waits, '2');
  assert.equal(first.ua, undefined);                   // the device is described at join and at the end, not every 5 s
  r.advance(1200);
  r.samples.push({ presented: 340, workMs: 1700, frame: 340, waits: 2, waitMs: 80, delay: 3, pingMs: 61 });
  r.reporter.finish();
  assert.equal(r.sent.length, 2);
  const last = fieldsOf(r.sent[1]);
  assert.deepEqual([last.why, last.seq, last.ua, last.cores, last.wait_tot_ms], ['end', '2', 'Chrome-131', '8', '80']);
  r.reporter.finish();
  r.advance(10000);
  r.t.tick();
  assert.equal(r.sent.length, 2);                       // finished once: nothing more, nothing twice
  assert.equal(r.t.timers[0], null);
});

test('the last report is kept for a report sent from the front page after the match', () => {
  const r = reporterFor();
  r.reporter.begin();
  r.advance(5000);
  r.samples.push({ presented: 290, workMs: 1450, frame: 290, waits: 0, waitMs: 0, delay: 3, pingMs: 60 });
  r.t.tick();
  assert.equal(r.stored.length, 1);
  assert.equal(r.stored[0].lobby, 'QZXW');
  assert.equal(r.stored[0].name, 'Ana');
  assert.equal(Object.fromEntries(r.stored[0].fields).fps, '58.0');
});

test('a hidden tab is counted in the report, and a missing game or a throwing source never stops the reports', () => {
  let hidden = true;
  const r = reporterFor({ extra: { isHidden: () => hidden, audio: { take() { throw new Error('boom'); } } } });
  r.reporter.begin();
  for (let i = 0; i < 4; ++i) { r.advance(500); r.t.tick(); }   // 2 s hidden
  hidden = false;
  for (let i = 0; i < 6; ++i) { r.advance(500); r.t.tick(); }
  assert.equal(r.sent.length, 1);
  assert.equal(fieldsOf(r.sent[0]).hid_s, '2');
  assert.equal(fieldsOf(r.sent[0]).au_cb, undefined);
});

test('a join report describes the device and the build and carries no counters', () => {
  const line = diagLine(joinFields([['build', 'wl-abc123'], ['lobby', 'QZXW'], ['name', 'Ana']], [['ua', 'Chrome-131'], ['touch', 1]]));
  assert.equal(line, 'DIAG v=1 why=join build=wl-abc123 lobby=QZXW name=Ana ua=Chrome-131 touch=1');
});

test('a report from the front page: the last report of the match, this device, the lobby and the name, and the note', () => {
  const saved = { lobby: 'QZXW', name: 'Ana', fields: [['v', 1], ['why', 'end'], ['seq', 9], ['lobby', 'QZXW'], ['fps', '41.0'], ['wait_ms', 900], ['ua', 'old']] };
  const line = diagLine(reportFields({ saved, device: [['ua', 'Chrome-131'], ['touch', 1]], lobby: '', name: 'Ana', build: 'wl-abc123' }), 'laggy');
  assert.equal(line, 'DIAG v=1 why=report build=wl-abc123 lobby=QZXW name=Ana fps=41.0 wait_ms=900 ua=Chrome-131 touch=1 note=laggy');
  const none = diagLine(reportFields({ saved: null, device: [['ua', 'Chrome-131']], lobby: '', name: 'Ana', build: 'wl-abc123' }), 'x');
  assert.equal(none, 'DIAG v=1 why=report build=wl-abc123 name=Ana ua=Chrome-131 note=x');
});

test('frames: the median interval tells the display rate the page really gets (a 30 fps power-saving cap shows as ~33 ms)', () => {
  const callbacks = [];
  const watch = new FrameWatch({ raf: (cb) => callbacks.push(cb) });
  watch.start();
  let t = 0;
  for (let i = 0; i < 31; ++i) { t += 33.3; callbacks.shift()(t); }
  assert.ok(Math.abs(watch.take().med - 33.3) < 0.01);
});

test('gpuName: the GPU from the WebGL renderer string, without the ANGLE wrapper; a software renderer reads as software', () => {
  const nvidia = 'ANGLE (NVIDIA, NVIDIA GeForce RTX 3080 (0x00002206) Direct3D11 vs_5_0 ps_5_0, D3D11)';
  assert.equal(gpuName(nvidia), 'NVIDIA GeForce RTX 3080 (0x00002206) Direct3D11 vs_5_0 ps_5_0');
  assert.ok(gpuName('ANGLE (Google, Vulkan 1.3.0 (SwiftShader Device (Subzero) (0x0000C0DE)), SwiftShader driver)').startsWith('software'));
  assert.ok(gpuName('ANGLE (Microsoft, Microsoft Basic Render Driver Direct3D11 vs_5_0 ps_5_0, D3D11)').startsWith('software'));
  assert.equal(gpuName('Apple M2'), 'Apple M2');
  assert.equal(gpuName(undefined), undefined);
});

test('the device description carries the GPU when the page can read it', () => {
  const f = Object.fromEntries(deviceFields({ navigator: {}, matchMedia: () => ({ matches: false }), wotmGpu: 'software SwiftShader' }));
  assert.equal(f.gpu, 'software SwiftShader');
});

test('task lag: how late a task posted to the page runs (the WebSocket hops of a match wait in the same queue)', () => {
  let now = 0;
  const posted = [];
  const lag = new TaskLagWatch({ now: () => now, post: (fn) => posted.push(fn) });
  lag.probe(); now += 4; posted.shift()();
  lag.probe(); now += 30; posted.shift()();
  assert.deepEqual(lag.take(), { n: 2, avg: 17, max: 30 });
  assert.equal(lag.take(), null);
});

test('a window carries the median frame interval, the share of samples with the page focused and the task lag', () => {
  const r = reporterFor({ extra: {
    frames: { start() {}, stop() {}, take: () => ({ n: 150, p95: 17, max: 30, late: 1, med: 16.7 }) },
    hasFocus: (() => { let i = 0; return () => (i++ % 2 === 0); })(),
    lag: { probe() {}, take: () => ({ n: 10, avg: 3.25, max: 12 }) },
  } });
  r.reporter.begin();
  for (let i = 0; i < 10; ++i) { r.advance(500); r.t.tick(); }
  const f = fieldsOf(r.sent[0]);
  assert.equal(f.raf_med_ms, '16.7');
  assert.equal(f.focus_pct, '50');
  assert.equal(f.lag_ms, '3.3');
  assert.equal(f.lag_max_ms, '12');
});
