// ABOUTME: Whether this device keeps up with an online match, measured from its own diag windows (web/diag.js) and remembered in this
// ABOUTME: browser: a device that can't asks for the game's 30 Hz mode (HELLO hz=30) and draws lighter in its next matches.

// An online match counts every game frame as 1/60 s of game time and runs at the pace of its slowest device, so a device that computes
// fewer than 60 frames a second slows the whole match. Its own rate is the frames it ran over the time it did not spend waiting for
// the network: a slow device waits little and computes ~30; a fast one held back by it waits half the time and computes ~60.
export const HOLD_SHARE = 55 / 60;   // keeping up: at least this share of the match's rate (55 of 60, 27.5 of 30)
export const SLOW_KEY = 'wotlane.slowDevice';   // '1': it fell short; '0': it held 60 in its last 60 Hz match
export const HEALTH_KEY = 'wotlane.deviceHealth';   // JSON { fps, workMs, hz }: how this device ran its last match (shown to the lobby)
const SKIP_SECONDS = 20;   // the match's first seconds (loading the stage, the intro) don't count
const MIN_WINDOWS = 6;     // 30 s of play before a verdict
const MIN_HEALTH_WINDOWS = 4;   // 20 s of play before the health
const HEALTH_WINDOWS = 12;      // the health is the median of the last minute

// frames per second of this device's own time; undefined with under a second of its own in the window
export function ownRate({ frames, seconds, waitMs }) {
  const own = seconds - waitMs / 1000;
  return own >= 1 ? frames / own : undefined;
}

const median = (xs) => {
  const s = [...xs].sort((a, b) => a - b);
  return s[s.length >> 1];
};

// Fed every diag window of a match (add(fields), the [key, value] pairs of windowFields plus hz); once enough windows count, writes the
// device's health and the verdict after each one. `store`: { get(key), set(key, value) } (localStorage)
export class RateMemory {
  constructor(store) {
    this.store = store;
    this.rates = [];
    this.works = [];   // ms of work per frame, one per rate
    this.prevUp = undefined;
  }

  add(fields) {
    const f = Object.fromEntries(fields);
    const up = Number(f.up_s);
    const dt = this.prevUp === undefined ? undefined : up - this.prevUp;
    this.prevUp = up;
    const hz = Number(f.hz);
    if ((hz !== 60 && hz !== 30) || f.hid_s !== undefined || !(up - (dt ?? 0) >= SKIP_SECONDS) || !(dt > 0)) return;
    const rate = ownRate({ frames: Number(f.gfps) * dt, seconds: dt, waitMs: Number(f.wait_ms) || 0 });
    if (rate === undefined) return;
    this.rates.push(rate);
    this.works.push(Number(f.work_ms));
    this.rememberHealth(hz);
    if (this.rates.length < MIN_WINDOWS) return;
    // a 30 Hz match that it kept up with says nothing about 60, so only falling short there is written
    const slow = median(this.rates) < hz * HOLD_SHARE;
    if (hz === 60 || slow) {
      try { this.store.set(SLOW_KEY, slow ? '1' : '0'); } catch { /* storage may be off */ }
    }
  }

  // the median of the last minute's windows: how this device runs a match, for the lobby's member list
  rememberHealth(hz) {
    const rates = this.rates.slice(-HEALTH_WINDOWS);
    const works = this.works.slice(-HEALTH_WINDOWS).filter(Number.isFinite);
    if (rates.length < MIN_HEALTH_WINDOWS || !works.length) return;
    try { this.store.set(HEALTH_KEY, JSON.stringify({ fps: median(rates), workMs: median(works), hz })); } catch { /* storage may be off */ }
  }
}

// `storage`: localStorage (or undefined); how this device ran its last match, or null
export function rememberedHealth(storage) {
  try {
    const h = JSON.parse(storage?.getItem(HEALTH_KEY));
    const inRange = (n, max) => typeof n === 'number' && n >= 0 && n <= max;   // what the server's HEALTH accepts
    return inRange(h?.fps, 1000) && inRange(h?.workMs, 1000) && Number.isInteger(h?.hz) && h.hz >= 1 && h.hz <= 240 ? { fps: h.fps, workMs: h.workMs, hz: h.hz } : null;
  } catch { return null; }
}

// the HEALTH command that tells a lobby how this device runs a match
export const healthLine = ({ fps, workMs, hz }) => `HEALTH ${fps.toFixed(1)} ${workMs.toFixed(1)} ${hz}`;

// `storage`: localStorage (or undefined); true when this device was measured unable to hold 60
export function rememberedSlow(storage) {
  try { return storage?.getItem(SLOW_KEY) === '1'; } catch { return false; }
}
