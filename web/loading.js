// ABOUTME: The loading text shown over the game while the disc streams in: the percent of the expected first download, until no
// ABOUTME: chunk has arrived for a while (then the game is running, or its data came from the browser's cache).
'use strict';

class LoadingProgress {
  // `expectedBytes`: what a first visit downloads before the game is on screen (measured: about 27 MB to the title, 57 MB to a fight)
  constructor(expectedBytes, { quietMs = 1500 } = {}) {
    this.expectedBytes = expectedBytes;
    this.quietMs = quietMs;
    this.bytes = 0;
    this.lastChunkAt = null;
    this.done = false;
  }

  add(bytes, now) {
    this.bytes += bytes;
    this.lastChunkAt = now;
  }

  // the text to show at `now`, or null once the downloads went quiet (and from then on)
  text(now) {
    if (this.lastChunkAt !== null && now - this.lastChunkAt > this.quietMs) this.done = true;
    if (this.done) return null;
    const MB = 1 << 20;
    const raw = Math.floor((this.bytes / this.expectedBytes) * 100);
    const total = Math.floor(this.expectedBytes / MB);
    const mb = Math.min(total, Math.floor(this.bytes / MB));
    const firstVisit = raw < 100 ? ' Only the first visit downloads this much.' : '';
    return `Downloading game data ${Math.min(99, raw)}% (${mb} of ${total} MB).${firstVisit}`;
  }
}

// After boot: a stage or monster that isn't in the cache yet streams in while the game waits on it, on a black screen. A burst of
// chunks that goes on for `showAfterMs` gets a note with its MB, until no chunk has arrived for `quietMs`; the next burst counts anew.
// `quietMs` spans the game's own work between the files of one load (measured: gaps of 2 to 6 s while a stage loads at 9 Mbps)
class StreamActivity {
  constructor({ showAfterMs = 1000, quietMs = 3500 } = {}) {
    this.showAfterMs = showAfterMs;
    this.quietMs = quietMs;
    this.burstStart = null;
    this.lastChunkAt = null;
    this.bytes = 0;
  }

  add(bytes, now) {
    if (this.lastChunkAt === null || now - this.lastChunkAt > this.quietMs) {
      this.burstStart = now;
      this.bytes = 0;
    }
    this.bytes += bytes;
    this.lastChunkAt = now;
  }

  // the note at `now`, or null (no burst, a short one, or quiet again)
  text(now) {
    if (this.lastChunkAt === null || now - this.lastChunkAt > this.quietMs || now - this.burstStart < this.showAfterMs) return null;
    const mb = Math.floor(this.bytes / (1 << 20));
    return `Loading game data (${mb ? mb + ' MB' : 'under 1 MB'})`;
  }
}

if (typeof module !== 'undefined' && module.exports) module.exports = { LoadingProgress, StreamActivity };
else {
  globalThis.LoadingProgress = LoadingProgress;
  globalThis.StreamActivity = StreamActivity;
}
