// ABOUTME: The loading text shown over the game while the disc streams in: the percent of the expected first download, until no
// ABOUTME: chunk has arrived for a while (then the game is running, or its data came from the browser's cache).
'use strict';

class LoadingProgress {
  // `expectedBytes`: what a first visit downloads before the game is on screen (measured: about 27 MB to the title, 57 MB to a fight)
  // `rateWindowMs`: how far back the download rate looks, for the time left
  constructor(expectedBytes, { quietMs = 1500, initial = 'Starting...', rateWindowMs = 5000 } = {}) {
    this.expectedBytes = expectedBytes;
    this.quietMs = quietMs;
    this.initial = initial;
    this.rateWindowMs = rateWindowMs;
    this.bytes = 0;
    this.cacheBytes = 0;
    this.lastChunkAt = null;
    this.done = false;
    this.samples = [];   // [time, bytes so far] after each network chunk, the last rateWindowMs only
  }

  add(bytes, now) {
    this.bytes += bytes;
    this.lastChunkAt = now;
    this.samples.push([now, this.bytes]);
    while (this.samples.length > 1 && now - this.samples[0][0] > this.rateWindowMs) this.samples.shift();
  }

  addCacheHit(bytes, now) {
    this.cacheBytes += bytes;
    this.lastChunkAt = now;
  }

  finish() {
    this.done = true;
  }

  // seconds until the expected download is in at the recent rate (a stall since the last chunk lowers it), or null: under a second of
  // samples, nothing left, or no network bytes
  secondsLeft(now) {
    if (this.samples.length < 2 || this.bytes >= this.expectedBytes) return null;
    const [t0, b0] = this.samples[0];
    const [t1, b1] = this.samples[this.samples.length - 1];
    if (t1 - t0 < 1000 || b1 <= b0) return null;
    return (this.expectedBytes - this.bytes) / ((b1 - b0) / ((Math.max(t1, now) - t0) / 1000));
  }

  // what the loading screen draws at `now`, or null once the downloads went quiet (and from then on): `percent` is null until there is
  // one to show; `amount`, `eta` and `note` are '' when they have nothing to say
  view(now) {
    if (this.lastChunkAt !== null && now - this.lastChunkAt > this.quietMs) this.done = true;
    if (this.done) return null;
    if (this.bytes === 0) {
      if (this.cacheBytes > 0) return { percent: null, headline: 'Loading from cache...', amount: '', eta: '', note: 'Later visits load faster: this is already on your device.' };
      return { percent: null, headline: this.initial, amount: '', eta: '', note: '' };
    }
    const MB = 1 << 20;
    const raw = Math.floor((this.bytes / this.expectedBytes) * 100);
    const total = Math.floor(this.expectedBytes / MB);
    const mb = Math.min(total, Math.floor(this.bytes / MB));
    const left = this.secondsLeft(now);
    return {
      percent: Math.min(99, raw),
      headline: 'Downloading game data',
      amount: `${mb} of ${total} MB`,
      eta: left === null ? '' : formatEta(left),
      note: raw < 100 ? 'Only the first visit downloads this much.' : '',
    };
  }

  // the view as one line (the status text the page showed before the loading screen)
  text(now) {
    const v = this.view(now);
    if (!v) return null;
    if (v.percent === null) return v.headline;
    return `${v.headline} ${v.percent}% (${v.amount}).${v.note ? ' ' + v.note : ''}`;
  }
}

// "about 10 s left": seconds in fives up to a minute, then whole minutes
function formatEta(seconds) {
  if (seconds <= 5) return 'almost done';
  if (seconds <= 60) return `about ${Math.ceil(seconds / 5) * 5} s left`;
  return `about ${Math.ceil(seconds / 60)} min left`;
}

// Tips for the loading screen, from the game's own controls table so they cannot drift from it. `controls` is controls.js CONTROLS and
// `label` its keyLabel; a tip about an action the table lacks is left out. The rest is about what the page itself does.
function loadingTips(controls, label) {
  const keys = (bind) => controls.find((c) => c.bind === bind)?.keys.map(label).join(' or ');
  const tips = [];
  const control = (bind, make) => { const k = keys(bind); if (k) tips.push(make(k)); };
  control('BLOCK', (k) => `Hold ${k} to block. Blocking is the best answer to a heavy hitter.`);
  control('LOCK', (k) => `Press ${k} to lock on to an enemy, then strafe around them.`);
  control('CIRCLE', (k) => `Press ${k} to grab. A grab gets through a block.`);
  control('SELECT', (k) => `Press ${k} to taunt. It does nothing but it feels great.`);
  control('START', (k) => `Press ${k} to pause and open the options.`);
  tips.push(
    'Jump, Fast and Turbo are rules the host switches on in the lobby.',
    'Game night: the host copies the invite link and friends join with one click.',
    'A lobby holds up to 8 players, so bring everyone.',
    'After a match the lobby stays open for a rematch, so keep the crew together.',
    'Back up your memory card saves from Settings, as a zip.',
  );
  return tips;
}

// After boot: a stage or monster that isn't in the cache yet streams in while the game waits on it, on a black screen. A burst of
// chunks that goes on for `showAfterMs` gets a note, until no chunk has arrived for `quietMs` (it spans the game's own work between the
// files of a load); its MB count goes on through the load's pauses and starts over after `resetMs` without a chunk (a new load)
class StreamActivity {
  constructor({ showAfterMs = 1000, quietMs = 3500, resetMs = 10000 } = {}) {
    this.showAfterMs = showAfterMs;
    this.quietMs = quietMs;
    this.resetMs = resetMs;
    this.burstStart = null;
    this.lastChunkAt = null;
    this.bytes = 0;
  }

  add(bytes, now) {
    const quiet = this.lastChunkAt === null ? Infinity : now - this.lastChunkAt;
    if (quiet > this.quietMs) this.burstStart = now;
    if (quiet > this.resetMs) this.bytes = 0;
    this.bytes += bytes;
    this.lastChunkAt = now;
  }

  // the note at `now`, or null (no burst, a short one, or quiet again)
  text(now) {
    if (this.lastChunkAt === null || now - this.lastChunkAt > this.quietMs || now - this.burstStart < this.showAfterMs) return null;
    const mb = Math.floor(this.bytes / (1 << 20));
    return mb ? `Loading game data (${mb} MB)` : 'Loading game data';
  }
}

// Monotonic status-bar state for game boot and online match startup.
// Phases: 0 starting, 1 emscripten text, 2 connecting, 3 waiting for peers, 4 loading stage, 5 running, 6 terminal error/abort.
class BootStatus {
  constructor({ online = false, otherNames = [] } = {}) {
    this.online = online;
    this.otherNames = otherNames.filter(Boolean);
    this.phase = 0;
    this.text = online ? 'Starting match...' : 'Starting...';
  }

  setEmscriptenStatus(text) {
    if (this.phase >= 5) return null;
    if (text) {
      if (this.phase > 1) return null;
      this.phase = 1;
      this.text = text;
      return this.text;
    }
    if (!this.online) {
      this.phase = 5;
      this.text = 'Running';
      return this.text;
    }
    if (this.phase < 2) {
      this.phase = 2;
      this.text = 'Connecting to match...';
      return this.text;
    }
    return null;
  }

  onLog(line, errorStatus) {
    if (errorStatus) {
      this.phase = 6;
      this.text = errorStatus;
      return this.text;
    }
    if (!this.online || this.phase >= 5) return null;
    if (this.phase < 3 && /^\[net\] connecting to /.test(line)) {
      this.phase = 3;
      this.text = this.otherNames.length ? `Waiting for ${this.otherNames.join(', ')}...` : 'Waiting for other players...';
      return this.text;
    }
    if (this.phase < 4 && /^\[net\] start: /.test(line)) {
      this.phase = 4;
      this.text = 'Loading stage...';
      return this.text;
    }
    return null;
  }

  onFrame(frame) {
    if (this.phase >= 5 || !(frame > 0)) return null;
    this.phase = 5;
    this.text = 'Running';
    return this.text;
  }

  onAbort(what) {
    this.phase = 6;
    this.text = 'Stopped: ' + what;
    return this.text;
  }
}

if (typeof module !== 'undefined' && module.exports) module.exports = { LoadingProgress, StreamActivity, BootStatus, formatEta, loadingTips };
else {
  globalThis.LoadingProgress = LoadingProgress;
  globalThis.StreamActivity = StreamActivity;
  globalThis.BootStatus = BootStatus;
  globalThis.formatEta = formatEta;
  globalThis.loadingTips = loadingTips;
}
