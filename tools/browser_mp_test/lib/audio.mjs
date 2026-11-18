// ABOUTME: Measures what the game's audio puts out: a probe in the page wraps the ScriptProcessor callback raylib's web audio uses and
// ABOUTME: accumulates the loudness (RMS and peak) of every buffer, which a check reads over a window of time.
import { sleep } from './player.mjs';

// Installed before the game makes its audio context (a context init script). Counts the samples the game's own handler wrote into
// each output buffer; the page's `window.__audio` holds the running sums.
export function audioProbe() {
  window.__audio = { n: 0, sumSquares: 0, peak: 0, buffers: 0 };
  const Context = window.AudioContext || window.webkitAudioContext;
  if (!Context) return;
  const make = Context.prototype.createScriptProcessor;
  Context.prototype.createScriptProcessor = function (...args) {
    const node = make.apply(this, args);
    let handler = null;
    Object.defineProperty(node, 'onaudioprocess', { configurable: true, get: () => handler, set: (f) => { handler = f; } });
    node.addEventListener('audioprocess', (e) => {
      if (handler) handler.call(node, e);
      const a = window.__audio;
      ++a.buffers;
      for (let c = 0; c < e.outputBuffer.numberOfChannels; c++) {
        const d = e.outputBuffer.getChannelData(c);
        for (let i = 0; i < d.length; i++) {
          a.sumSquares += d[i] * d[i];
          ++a.n;
          const m = Math.abs(d[i]);
          if (m > a.peak) a.peak = m;
        }
      }
    });
    return node;
  };
}

// The loudness of the game's output over the next `ms`: { rms, peak, buffers } (rms -1 when no buffer came)
export async function listen(page, ms) {
  await page.evaluate(() => { window.__audio = { ...window.__audio, n: 0, sumSquares: 0, peak: 0, buffers: 0 }; });
  await sleep(ms);
  const a = await page.evaluate(() => window.__audio);
  return { rms: a.n ? Math.sqrt(a.sumSquares / a.n) : -1, peak: a.peak, buffers: a.buffers };
}
