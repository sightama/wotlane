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
    const percent = Math.min(99, Math.floor((this.bytes / this.expectedBytes) * 100));
    return `Downloading game data ${percent}%`;
  }
}

if (typeof module !== 'undefined' && module.exports) module.exports = { LoadingProgress };
else globalThis.LoadingProgress = LoadingProgress;
