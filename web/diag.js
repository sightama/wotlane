// ABOUTME: The page's diagnostics for online matches: samples the game's counters, the display's frame timing, long tasks and audio
// ABOUTME: callbacks, and writes them as DIAG lines for the lobby WebSocket. It only reads; nothing here touches the game's state.

export const SCHEMA = 1;
export const SAMPLE_EVERY_MS = 500;
export const REPORT_EVERY_MS = 5000;
const LATE_FRAME_MS = 25;   // a 60 Hz frame is 16.7 ms: past 25 ms one was missed
const MAX_NOTE = 200;       // the server keeps this much of a note (native/net/wotm_net_diag.hpp kMaxNoteBytes)
const MIB = 1 << 20;

// a value as native/net/wotm_net_diag.hpp accepts it: A-Z a-z 0-9 . _ : , / + - and at most 48 characters
export function diagValue(v) {
  const text = String(v).replace(/[^A-Za-z0-9._:,/+-]/g, '-').slice(0, 48);
  return text || '-';
}

// `fields`: [key, value] pairs in order; one that is missing (undefined, null, NaN, '') is left out. `note`: the player's own words
export function diagLine(fields, note) {
  const words = ['DIAG'];
  for (const [k, v] of fields) {
    if (v === undefined || v === null || v === '' || (typeof v === 'number' && !Number.isFinite(v))) continue;
    words.push(`${k}=${diagValue(v)}`);
  }
  const text = String(note ?? '').replace(/\s+/g, ' ').trim().slice(0, MAX_NOTE);
  if (text) words.push('note=' + encodeURIComponent(text));
  return words.join(' ');
}

// the pairs that have a value
const present = (pairs) => pairs.filter(([, v]) => v !== undefined && v !== null && !(typeof v === 'number' && !Number.isFinite(v)));
const fix = (n, digits = 1) => (Number.isFinite(n) ? n.toFixed(digits) : undefined);
const whole = (n) => (Number.isFinite(n) ? Math.round(n) : undefined);

// "Chrome-131": the browser's name and major version, from its brands or (Firefox, older browsers) its user agent; never the whole string
export function browserName(nav) {
  const brand = (nav.userAgentData?.brands ?? []).find((b) => !/not.?a.?brand|chromium/i.test(b.brand));
  if (brand) return `${brand.brand.replace(/^(Google|Microsoft) /, '').replace(/\s+/g, '')}-${brand.version}`;
  const ua = nav.userAgent ?? '';
  for (const [pattern, name] of [[/Edg\/(\d+)/, 'Edge'], [/OPR\/(\d+)/, 'Opera'], [/Firefox\/(\d+)/, 'Firefox'], [/Chrome\/(\d+)/, 'Chrome'],
    [/Version\/(\d+).*Safari/, 'Safari']]) {
    const m = pattern.exec(ua);
    if (m) return `${name}-${m[1]}`;
  }
  return 'unknown';
}

function osName(nav) {
  const platform = nav.userAgentData?.platform;
  if (platform) return platform.replace(/\s+/g, '');
  const ua = nav.userAgent ?? '';
  for (const [pattern, name] of [[/Android/, 'Android'], [/iPhone|iPad/, 'iOS'], [/CrOS/, 'ChromeOS'], [/Windows/, 'Windows'], [/Mac OS X/, 'macOS'],
    [/Linux/, 'Linux']]) {
    if (pattern.test(ua)) return name;
  }
  return undefined;
}

// the class of device: touch, phone or not, system, browser, cores, memory, pixel ratio, window size and the connection's own estimate.
// Nothing that identifies a person or a machine: no user agent string, address or language
export function deviceFields(win) {
  const nav = win.navigator ?? {};
  const conn = nav.connection ?? {};
  const mobile = nav.userAgentData?.mobile ?? /Mobi|Android|iPhone|iPad/.test(nav.userAgent ?? '');
  return present([
    ['touch', win.matchMedia?.('(pointer: coarse)').matches ? 1 : 0],
    ['mobile', mobile ? 1 : 0],
    ['os', osName(nav)],
    ['ua', browserName(nav)],
    ['cores', nav.hardwareConcurrency],
    ['mem_gb', nav.deviceMemory],
    ['dpr', win.devicePixelRatio > 0 ? win.devicePixelRatio.toFixed(2) : undefined],
    ['scr', win.innerWidth > 0 ? `${win.innerWidth}x${win.innerHeight}` : undefined],
    ['net', conn.effectiveType],
    ['dl_mbps', conn.downlink],
    ['est_rtt_ms', conn.rtt],
    ['gpu', readGpu(win) ?? undefined],
    ['gl_sw', readSoftwareGl(win)],
  ]);
}

// the intervals between animation frames: how long the slowest 5% took, the longest, how many missed a 60 Hz frame. The interval that
// spans a tab being hidden or shown is dropped (the browser sends no frames to a hidden tab)
export class FrameWatch {
  constructor({ raf, doc }) {
    this.raf = raf;
    this.running = false;
    this.last = null;
    this.intervals = [];
    doc?.addEventListener('visibilitychange', () => { this.last = null; });
  }

  start() {
    this.running = true;
    const step = (t) => {
      if (!this.running) return;
      if (this.last !== null) this.intervals.push(t - this.last);
      this.last = t;
      this.raf(step);
    };
    this.raf(step);
  }

  stop() { this.running = false; }

  take() {
    const v = this.intervals.sort((a, b) => a - b);
    this.intervals = [];
    if (!v.length) return null;
    return { n: v.length, med: v[v.length >> 1], p95: v[Math.min(v.length - 1, Math.floor(v.length * 0.95))], max: v[v.length - 1],
      late: v.filter((x) => x > LATE_FRAME_MS).length };
  }
}

// The disc bytes the game and the boot's and the lobby's prefetches got (the whole-game prefetch's are {prefetch}, left out): each chunk
// that came over the network is a number on the 'wotlane-disc' channel
// (data_retry.js, disc_chunks.js), each read the disc cache served is {hit: bytes} (disc_cache.js). `channel`: that BroadcastChannel, or
// null (no source)
export class DiscWatch {
  constructor(channel) {
    this.channel = channel;
    this.net = 0;
    this.hit = 0;
    if (channel) channel.onmessage = (e) => {
      if (typeof e.data === 'number') this.net += e.data;
      else if (typeof e.data?.hit === 'number') this.hit += e.data.hit;
    };
  }

  take() {
    if (!this.channel) return null;
    const out = { net: this.net, hit: this.hit };
    this.net = 0;
    this.hit = 0;
    return out;
  }
}

// How late a task posted to the page runs: the match's WebSocket messages, sent and received on this thread, wait in the same queue
// behind the drawing. `post(fn)` runs fn as a new task (a MessageChannel message); `now()` in ms
export class TaskLagWatch {
  constructor({ now, post }) {
    Object.assign(this, { now, post });
    this.n = 0;
    this.sum = 0;
    this.max = 0;
  }

  probe() {
    const t = this.now();
    this.post(() => {
      const d = this.now() - t;
      ++this.n;
      this.sum += d;
      this.max = Math.max(this.max, d);
    });
  }

  take() {
    if (!this.n) return null;
    const out = { n: this.n, avg: this.sum / this.n, max: this.max };
    this.n = 0;
    this.sum = 0;
    this.max = 0;
    return out;
  }
}

// The GPU from WebGL's unmasked renderer string: the model inside ANGLE's "ANGLE (vendor, model, backend)", and "software ..." for a
// renderer without a GPU (SwiftShader, llvmpipe, Microsoft Basic Render Driver: hardware acceleration off or the GPU blocked)
export function gpuName(renderer) {
  if (!renderer) return undefined;
  const inner = /^ANGLE \((.*)\)$/.exec(renderer)?.[1];
  const parts = inner ? inner.split(', ') : [renderer];
  const model = parts.length >= 3 ? parts.slice(1, -1).join(', ') : parts[parts.length > 1 ? 1 : 0];
  return /SwiftShader|llvmpipe|softpipe|Basic Render|Software/i.test(renderer) ? `software ${model}` : model;
}

// the renderer string of a WebGL context made for the purpose, once per page (undefined where WebGL or the debug extension is missing)
export function readGpu(win) {
  if (win.wotmGpu !== undefined) return win.wotmGpu;
  let name;
  try {
    const gl = win.document?.createElement('canvas').getContext('webgl');
    const ext = gl?.getExtension('WEBGL_debug_renderer_info');
    name = gpuName(ext ? gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) : gl?.getParameter(gl.RENDERER));
    gl?.getExtension('WEBGL_lose_context')?.loseContext();
  } catch { /* no WebGL here */ }
  win.wotmGpu = name ?? null;
  return win.wotmGpu;
}

// Whether WebGL2 (the game's) draws in software: a context asked to fail on a major performance caveat is refused while a plain one is
// given (1), or the caveat context is given (0: none detected; a browser may not flag every software renderer). Undefined without WebGL
// or on an error, and then asked again next time. The caveat context is asked first and alone, so a refusal for lack of room (too many
// contexts, a lost GPU) is not read as software. Unlike the renderer string, a browser that masks it (Brave reports "Brave") answers this
export function readSoftwareGl(win) {
  if (win.wotmSoftwareGl !== undefined) return win.wotmSoftwareGl;
  const make = (opts) => win.document?.createElement('canvas').getContext('webgl2', opts) ?? null;
  const drop = (gl) => gl?.getExtension?.('WEBGL_lose_context')?.loseContext();
  let sw;
  try {
    const fast = make({ failIfMajorPerformanceCaveat: true });
    drop(fast);
    if (fast) sw = 0;
    else {
      const plain = make({});
      drop(plain);
      if (plain) sw = 1;
    }
  } catch { /* no WebGL here */ }
  if (sw !== undefined) win.wotmSoftwareGl = sw;
  return sw;
}

// main-thread tasks over 50 ms, where the browser reports them (Chrome and Edge; Firefox has no such entries: take() gives null)
export class LongTaskWatch {
  constructor(Observer) {
    this.n = 0;
    this.ms = 0;
    this.max = 0;
    this.supported = false;
    try {
      this.observer = new Observer((list) => {
        for (const e of list.getEntries()) {
          ++this.n;
          this.ms += e.duration;
          this.max = Math.max(this.max, e.duration);
        }
      });
      this.observer.observe({ entryTypes: ['longtask'] });
      this.supported = true;
    } catch { /* no long task entries in this browser */ }
  }

  take() {
    if (!this.supported) return null;
    const out = { n: this.n, ms: this.ms, max: this.max };
    this.n = 0;
    this.ms = 0;
    this.max = 0;
    return out;
  }
}

// The game's audio comes from a ScriptProcessorNode (raylib's web audio), whose callback is due once per buffer. A callback that comes
// later than 1.5 buffers means the main thread kept the audio waiting: a dropped buffer, heard as a crackle. Install before the game
// creates its audio context. take() gives null when no script processor was ever made.
export function installAudioWatch(win) {
  const proto = (win.AudioContext ?? win.webkitAudioContext)?.prototype;
  const original = proto?.createScriptProcessor;
  const seen = { made: false, ctx: null, cb: 0, gaps: 0, maxGapMs: 0 };
  if (typeof original !== 'function') return { take: () => null };
  proto.createScriptProcessor = function (...args) {
    const node = original.apply(this, args);
    const periodMs = ((node.bufferSize || args[0] || 4096) / (this.sampleRate || 48000)) * 1000;
    let last = null;
    seen.bufferMs = periodMs;
    seen.made = true;
    seen.ctx = this;
    node.addEventListener('audioprocess', () => {
      const now = win.performance.now();
      ++seen.cb;
      if (last !== null) {
        const gap = now - last;
        seen.maxGapMs = Math.max(seen.maxGapMs, gap);
        if (gap > periodMs * 1.5) ++seen.gaps;
      }
      last = now;
    });
    return node;
  };
  return {
    take() {
      if (!seen.made) return null;
      const out = { cb: seen.cb, gaps: seen.gaps, maxGapMs: seen.maxGapMs, state: seen.ctx?.state, bufferMs: seen.bufferMs };
      seen.cb = 0;
      seen.gaps = 0;
      seen.maxGapMs = 0;
      return out;
    },
  };
}

// The game's counters since it started: frames presented and the time its loop worked (wotm_webfs.cpp wotmFrames), and from the wasm
// module (wotm_web_net_stat, native/wotm_webfs.cpp): the game frame, the waits for the other players and their total ms, the input
// delay in frames, the ping, the ms the game thread spent idle (waiting for a vblank or an event), its frames by length in frame periods
// (up to 1.25, 1.75, 2.25, longer: `periods`) and the vblank clock's wakes (`vblankWakes`), the ms they woke late in all (`vblankLateMs`) and
// how many were over half a vblank late (`vblankSlow`): how coarse this browser's timers are. Only reads.
export const NET_STAT = { frame: 0, waits: 1, waitMs: 2, delay: 3, pingMs: 4, idleMs: 5, periods: 6, vblankLateMs: 10, vblankWakes: 11, vblankSlow: 12,
  rbOn: 13, rbWindow: 14, rollbacks: 15, redone: 16, redoMs: 17, saves: 18, saveMs: 19 };
export function readGame(win) {
  const m = win.Module;
  const f = win.wotmFrames;
  const stat = typeof m?._wotm_web_net_stat === 'function' ? (i) => m._wotm_web_net_stat(i) : () => undefined;
  return {
    presented: f?.frames, workMs: f?.workMs,
    frame: stat(NET_STAT.frame), waits: stat(NET_STAT.waits), waitMs: stat(NET_STAT.waitMs), delay: stat(NET_STAT.delay), pingMs: stat(NET_STAT.pingMs),
    idleMs: stat(NET_STAT.idleMs),
    periods: typeof m?._wotm_web_net_stat === 'function' ? [0, 1, 2, 3].map((k) => stat(NET_STAT.periods + k)) : undefined,
    vblankLateMs: stat(NET_STAT.vblankLateMs), vblankWakes: stat(NET_STAT.vblankWakes), vblankSlow: stat(NET_STAT.vblankSlow),
    rbOn: stat(NET_STAT.rbOn), rbWindow: stat(NET_STAT.rbWindow), rollbacks: stat(NET_STAT.rollbacks), redone: stat(NET_STAT.redone),
    redoMs: stat(NET_STAT.redoMs), saves: stat(NET_STAT.saves), saveMs: stat(NET_STAT.saveMs),
  };
}

// The input latency of the presses since the last take (native/wotm_latency.hpp; wotm_web_latency_take and wotm_web_latency in
// native/wotm_webfs.cpp), ms: total (press to the first present that draws it: median, p95, max) and the stages' medians. Taking
// starts a new window, so the reporter takes it once per report. Only reads the game's state.
export const LATENCY = { n: 0, total: 1, p95: 2, max: 3, sample: 4, delay: 5, stall: 6, sim: 7, present: 8, unknown: 9, blend: 10 };
export function latencySource(win) {
  return {
    take() {
      const m = win.Module;
      if (typeof m?._wotm_web_latency_take !== 'function') return null;
      m._wotm_web_latency_take();
      return Object.fromEntries(Object.entries(LATENCY).map(([key, i]) => [key, m._wotm_web_latency(i)]));
    },
  };
}

// the mean difference between consecutive samples (the way RFC 3550 reads jitter)
function jitter(values) {
  if (values.length < 2) return undefined;
  let sum = 0;
  for (let i = 1; i < values.length; ++i) sum += Math.abs(values[i] - values[i - 1]);
  return sum / (values.length - 1);
}

// The report's numbers for the time between two samples. `prev` and `cur` hold the cumulative counters (readGame) and `t`, ms since the
// match began; `cur` also holds what was gathered over the window: `pings`, `raf`, `lt`, `audio`, `hiddenMs`, `lat`. Counters start at 0 when the
// game starts, so a missing earlier value reads as 0. A source that has nothing leaves its fields out.
export function windowFields(prev, cur) {
  const dt = (cur.t - prev.t) / 1000;
  const since = (key) => (Number.isFinite(cur[key]) ? cur[key] - (prev[key] ?? 0) : undefined);
  const perSecond = (n) => (dt > 0 ? n / dt : undefined);
  const presented = since('presented');
  const frames = since('frame');
  const pings = (cur.pings ?? []).filter(Number.isFinite);
  const { raf, lt, audio } = cur;
  const lat = cur.lat?.n > 0 ? cur.lat : undefined;
  // the game thread's busy time: the window less its idle waits and its waits for the network (a wait that began in the last window and
  // is counted in this one can leave less than nothing: no figure then)
  const busyMs = frames > 0 && Number.isFinite(cur.idleMs) ? cur.t - prev.t - since('idleMs') - (since('waitMs') ?? 0) : -1;
  const timed = lat && lat.unknown < lat.n ? lat : undefined;   // the total and the sample stage need presses with a page time
  // rollback (docs/ONLINE.md "Rollback"): the window this PC runs with, and the window's rollbacks, frames redone and their costs
  const rb = cur.rbOn === 1;
  const redone = rb ? since('redone') : undefined, saves = rb ? since('saves') : undefined;
  return present([
    ['up_s', whole(cur.t / 1000)],
    ['fps', fix(perSecond(presented))],
    ['gfps', fix(perSecond(frames))],
    ['work_ms', presented > 0 && Number.isFinite(cur.workMs) ? fix((cur.workMs - (prev.workMs ?? 0)) / presented) : undefined],
    ['ee_busy_ms', busyMs >= 0 ? fix(busyMs / frames) : undefined],
    ['ee_idle_ms', frames > 0 && Number.isFinite(cur.idleMs) ? fix(since('idleMs') / frames) : undefined],
    ['fr_len', Array.isArray(cur.periods) ? cur.periods.map((n, k) => n - (prev.periods?.[k] ?? 0)).join(',') : undefined],
    ['vbl_late_ms', since('vblankWakes') > 0 ? (since('vblankLateMs') / since('vblankWakes')).toFixed(2) : undefined],
    ['vbl_slow', since('vblankWakes') > 0 ? since('vblankSlow') : undefined],
    ['frame', cur.frame],
    ['delay', cur.delay],
    ['ping_ms', cur.pingMs],
    ['ping_max_ms', pings.length ? Math.max(...pings) : undefined],
    ['jitter_ms', fix(jitter(pings))],
    ['waits', since('waits')],
    ['wait_ms', whole(since('waitMs'))],
    ['waits_tot', cur.waits],
    ['wait_tot_ms', whole(cur.waitMs)],
    ['raf_med_ms', fix(raf?.med)],
    ['raf_p95_ms', fix(raf?.p95)],
    ['raf_max_ms', fix(raf?.max)],
    ['late', raf?.late],
    ['lt_n', lt?.n],
    ['lt_ms', whole(lt?.ms)],
    ['lt_max_ms', whole(lt?.max)],
    ['au_cb', audio?.cb],
    ['au_gaps', audio?.gaps],
    ['au_max_ms', whole(audio?.maxGapMs)],
    ['au_buf_ms', fix(audio?.bufferMs)],
    ['au_state', audio?.state],
    ['hid_s', cur.hiddenMs > 0 ? whole(cur.hiddenMs / 1000) : undefined],
    ['focus_pct', whole(cur.focusPct)],
    ['lag_ms', fix(cur.taskLag?.avg)],
    ['lag_max_ms', whole(cur.taskLag?.max)],
    ['tmr_ms', fix(cur.timerLag?.avg)],
    ['tmr_max_ms', whole(cur.timerLag?.max)],
    ['lat_n', lat?.n],
    ['lat_ms', fix(timed?.total)],
    ['lat_p95_ms', fix(timed?.p95)],
    ['lat_max_ms', fix(timed?.max)],
    ['lat_in_ms', fix(timed?.sample)],
    ['lat_dl_ms', fix(lat?.delay)],
    ['lat_st_ms', fix(lat?.stall)],
    ['lat_sim_ms', fix(lat?.sim)],
    ['lat_pr_ms', fix(lat?.present)],
    ['lat_bl_ms', fix(lat?.blend)],
    ['lat_unk', lat?.unknown > 0 ? lat.unknown : undefined],
    ['rb_win', rb ? cur.rbWindow : undefined],
    ['rb_n', rb ? since('rollbacks') : undefined],
    ['rb_frames', redone],
    ['rb_redo_ms', redone > 0 ? fix(since('redoMs') / redone) : undefined],
    ['rb_save_ms', saves > 0 ? fix(since('saveMs') / saves) : undefined],
    ['disc_net_mb', cur.disc?.net > 0 ? fix(cur.disc.net / MIB) : undefined],
    ['disc_hit_mb', cur.disc?.hit > 0 ? fix(cur.disc.hit / MIB) : undefined],
  ]);
}

// waits until the socket has put what it holds on the wire (a page that reloads right after a send would otherwise lose it), for at most
// `ms`. `wait(n)` sleeps n ms
export async function flushSocket(socket, { ms = 300, wait = (n) => new Promise((resolve) => setTimeout(resolve, n)) } = {}) {
  const step = 20;
  for (let waited = 0; socket.bufferedAmount > 0 && waited < ms; waited += step) await wait(step);
}

// the report sent once on joining a lobby: the build, the lobby, this device
export const joinFields = (base, device) => [['v', SCHEMA], ['why', 'join'], ...base, ...device];

// The report a player sends by hand (Send report): the last report of the match they just played (kept by the reporter), this
// device and the lobby and name it was under. `saved`: what the reporter stored, or null
export function reportFields({ saved, device, lobby, name, build, full }) {
  const own = new Set([...device.map(([k]) => k), 'v', 'why', 'seq', 'build', 'full', 'lobby', 'name']);
  return [['v', SCHEMA], ['why', 'report'], ['build', build], ['full', full], ['lobby', saved?.lobby || lobby], ['name', name],
    ...(saved?.fields ?? []).filter(([k]) => !own.has(k)), ...device];
}

// Every SAMPLE_EVERY_MS reads the game's counters (the ping is sampled this often, for its jitter) and every REPORT_EVERY_MS sends the
// window as a periodic report; finish() sends the last one (why=end) with the device. `base`: [key, value] pairs on every report (build,
// lobby, name, hz, players). `send(line)` puts a line on the lobby socket; `store.set` keeps the latest report for a hand-sent one;
// `hasFocus()`, `lag` and `timerLag` (TaskLagWatch, the second one posting through setTimeout) are read at every sample, `latency` (latencySource) once per report
// after the page reloads. A source that throws is left out, never reported as a failure of the page.
export class DiagReporter {
  constructor({ send, base, now, setTimer, clearTimer, store, frames, longTasks, audio, game, device, isHidden, hasFocus, lag, timerLag, latency, disc }) {
    Object.assign(this, { send, base, now, setTimer, clearTimer, store, frames, longTasks, audio, game, device, isHidden, hasFocus, lag, timerLag, latency, disc });
    this.seq = 0;
    this.started = false;
    this.pings = [];
    this.hiddenMs = 0;
    this.samples = 0;
    this.focused = 0;
  }

  begin() {
    this.start = this.now();
    this.prev = { t: 0, ...this.read() };
    this.lastReportAt = this.start;
    this.started = true;
    this.frames.start();
    this.timer = this.setTimer(() => this.sample(), SAMPLE_EVERY_MS);
  }

  read() {
    try { return this.game() ?? {}; } catch { return {}; }
  }

  sample() {
    if (!this.started) return;
    const game = this.read();
    if (Number.isFinite(game.pingMs)) this.pings.push(game.pingMs);
    if (this.isHidden()) this.hiddenMs += SAMPLE_EVERY_MS;
    ++this.samples;
    try { if (this.hasFocus?.()) ++this.focused; } catch { /* no focus reading */ }
    try { this.lag?.probe(); } catch { /* no task probe */ }
    try { this.timerLag?.probe(); } catch { /* no timer probe */ }
    this.latest = game;
    if (this.now() - this.lastReportAt >= REPORT_EVERY_MS) this.report('periodic', game);
  }

  take(source) {
    try { return source?.take() ?? undefined; } catch { return undefined; }
  }

  report(why, game) {
    const now = this.now();
    const cur = { t: now - this.start, ...game, pings: this.pings, raf: this.take(this.frames), lt: this.take(this.longTasks),
      audio: this.take(this.audio), hiddenMs: this.hiddenMs, taskLag: this.take(this.lag), timerLag: this.take(this.timerLag), lat: this.take(this.latency), disc: this.take(this.disc),
      focusPct: this.hasFocus && this.samples ? (100 * this.focused) / this.samples : undefined };
    const fields = [['v', SCHEMA], ['why', why], ['seq', ++this.seq], ...this.base, ...windowFields(this.prev, cur)];
    if (why === 'end') fields.push(...this.device());
    this.send(diagLine(fields));
    const get = (key) => this.base.find(([k]) => k === key)?.[1];
    try { this.store?.set({ lobby: get('lobby'), name: get('name'), fields }); } catch { /* storage may be off */ }
    this.prev = { ...cur, t: cur.t };
    this.pings = [];
    this.hiddenMs = 0;
    this.samples = 0;
    this.focused = 0;
    this.lastReportAt = now;
  }

  // the last report, once; stops the sampling
  finish() {
    if (!this.started) return;
    this.report('end', this.read());
    this.started = false;
    this.frames.stop();
    this.clearTimer(this.timer);
  }
}
