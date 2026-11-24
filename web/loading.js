// ABOUTME: The loading text shown over the game while the disc streams in: the percent of the expected first download, until no
// ABOUTME: chunk has arrived for a while (then the game is running, or its data came from the browser's cache).
'use strict';

class LoadingProgress {
  // `expectedBytes`: what a first visit downloads before the game is on screen (measured: about 27 MB to the title, 57 MB to a fight)
  constructor(expectedBytes, { quietMs = 1500, initial = 'Starting...' } = {}) {
    this.expectedBytes = expectedBytes;
    this.quietMs = quietMs;
    this.initial = initial;
    this.bytes = 0;
    this.cacheBytes = 0;
    this.lastChunkAt = null;
    this.done = false;
  }

  add(bytes, now) {
    this.bytes += bytes;
    this.lastChunkAt = now;
  }

  addCacheHit(bytes, now) {
    this.cacheBytes += bytes;
    this.lastChunkAt = now;
  }

  finish() {
    this.done = true;
  }

  // the text to show at `now`, or null once the downloads went quiet (and from then on)
  text(now) {
    if (this.lastChunkAt !== null && now - this.lastChunkAt > this.quietMs) this.done = true;
    if (this.done) return null;
    if (this.bytes === 0) return this.cacheBytes > 0 ? 'Loading from cache...' : this.initial;
    const MB = 1 << 20;
    const raw = Math.floor((this.bytes / this.expectedBytes) * 100);
    const total = Math.floor(this.expectedBytes / MB);
    const mb = Math.min(total, Math.floor(this.bytes / MB));
    const firstVisit = raw < 100 ? ' Only the first visit downloads this much.' : '';
    return `Downloading game data ${Math.min(99, raw)}% (${mb} of ${total} MB).${firstVisit}`;
  }
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
// Phases: 0 starting, 1 emscripten/connecting, 2 waiting for peers, 3 loading stage, 4 running, 5 terminal error/abort.
class BootStatus {
  constructor({ online = false, otherNames = [] } = {}) {
    this.online = online;
    this.otherNames = otherNames.filter(Boolean);
    this.phase = 0;
    this.text = online ? 'Starting match...' : 'Starting...';
  }

  setEmscriptenStatus(text) {
    if (this.phase >= 4) return null;
    if (text) {
      if (this.phase > 1) return null;
      this.phase = 1;
      this.text = text;
      return this.text;
    }
    if (!this.online) {
      this.phase = 4;
      this.text = 'Running';
      return this.text;
    }
    if (this.phase <= 1) {
      this.phase = 1;
      this.text = 'Connecting to match...';
      return this.text;
    }
    return null;
  }

  onLog(line, errorStatus) {
    if (errorStatus) {
      this.phase = 5;
      this.text = errorStatus;
      return this.text;
    }
    if (!this.online || this.phase >= 4) return null;
    if (this.phase < 2 && /^\[net\] connecting to /.test(line)) {
      this.phase = 2;
      this.text = this.otherNames.length ? `Waiting for ${this.otherNames.join(', ')}...` : 'Waiting for other players...';
      return this.text;
    }
    if (this.phase < 3 && /^\[net\] start: /.test(line)) {
      this.phase = 3;
      this.text = 'Loading stage...';
      return this.text;
    }
    return null;
  }

  onFrame(frame) {
    if (this.phase >= 4 || !(frame > 0)) return null;
    this.phase = 4;
    this.text = 'Running';
    return this.text;
  }

  onAbort(what) {
    this.phase = 5;
    this.text = 'Stopped: ' + what;
    return this.text;
  }
}

if (typeof module !== 'undefined' && module.exports) module.exports = { LoadingProgress, StreamActivity, BootStatus };
else {
  globalThis.LoadingProgress = LoadingProgress;
  globalThis.StreamActivity = StreamActivity;
  globalThis.BootStatus = BootStatus;
}
