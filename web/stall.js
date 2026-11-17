// ABOUTME: When an online match has waited a long time for another player, the page offers to keep waiting or leave the match;
// ABOUTME: keeping waiting asks again once the wait grows by the same time.
'use strict';

class StallPrompt {
  constructor(limitMs) {
    this.limitMs = limitMs;
    this.askAtMs = limitMs;
  }

  // the match can't go on (refused, desync, connection lost): the game reports a negative wait
  stopped(waitMs) {
    return waitMs < 0;
  }

  // waitMs: how long the game has been waiting now (0 when it isn't, negative once the match stopped)
  show(waitMs) {
    if (this.stopped(waitMs)) return true;
    if (waitMs === 0) this.askAtMs = this.limitMs;
    return waitMs >= this.askAtMs;
  }

  keepWaiting(waitMs) {
    this.askAtMs = waitMs + this.limitMs;
  }
}

if (typeof module !== 'undefined' && module.exports) module.exports = { StallPrompt };
else globalThis.StallPrompt = StallPrompt;
