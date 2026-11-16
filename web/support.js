// ABOUTME: Whether this browser can run the game, as a plain message for the front panel (or null when it can), and whether it reads
// ABOUTME: the disc and saves on the main thread. Chromium-based browsers and Firefox (with JSPI) can run it; Safari and phones can't yet.
'use strict';

// what the page can see: chromium (Chrome, Edge, Opera...), firefox, jspi (WebAssembly.Suspending), isolated (crossOriginIsolated), mobile
function supportProblem({ chromium, firefox, jspi, isolated, mobile }) {
  if (mobile) return 'This game needs a computer (Windows, Mac or Linux) with Chrome, Edge or Firefox. Phones and tablets can\'t run it.';
  if (!chromium && !firefox) return 'This game runs in Chrome, Edge or Firefox. Safari can\'t run it yet: open this page in one of those.';
  if (!jspi) return firefox ? 'Your Firefox is too old for this game: update Firefox, then reload the page.'
    : 'Your browser is too old for this game: update Chrome or Edge, then reload the page.';
  if (!isolated) return 'The page loaded without the settings the game needs. Reload the page; if it keeps happening, tell Lane.';
  return null;
}

// the disc and the save folder read without blocking the main thread (native/wotm_webdisc.cpp, wotm_websave.cpp) everywhere but
// Chromium, whose workers' fetches and OPFS calls go on while the main thread waits (WASMFS's fetch and OPFS backends, the proven path)
function mainThreadFiles({ chromium }) {
  return !chromium;
}

// a disc file's size (for mainThreadFiles reads) from a HEAD reply's content-length, or else from the total in a range probe's
// content-range ("bytes 0-0/<total>"); null when neither says
function fileSize(contentLength, contentRange) {
  const n = Number(contentLength);
  if (contentLength && Number.isInteger(n) && n > 0) return n;
  const total = /\/(\d+)$/.exec(contentRange ?? '');
  return total ? Number(total[1]) : null;
}

// A tab that isn't focused can't take the pointer: Firefox throws from requestPointerLock and Chrome rejects its promise. The game asks
// again later, so the refusal is dropped instead of surfacing as an uncaught error
function guardPointerLock(el) {
  const request = el.requestPointerLock;
  if (typeof request !== 'function') return;
  el.requestPointerLock = function (...args) {
    try {
      const result = request.apply(this, args);
      return typeof result?.catch === 'function' ? result.catch(() => {}) : result;
    } catch (e) {
      return undefined;
    }
  };
}

// the facts above, read from this browser
function browserFacts() {
  const brands = navigator.userAgentData?.brands ?? [];
  return {
    chromium: brands.some((b) => b.brand === 'Chromium'),
    firefox: /Firefox\//.test(navigator.userAgent),
    jspi: typeof WebAssembly.Suspending === 'function',
    isolated: globalThis.crossOriginIsolated === true,
    mobile: navigator.userAgentData?.mobile ?? /Mobi|Android|iPad|iPhone/.test(navigator.userAgent),
  };
}

if (typeof module !== 'undefined' && module.exports) module.exports = { supportProblem, mainThreadFiles, fileSize, guardPointerLock };
else {
  globalThis.wotmSupportProblem = () => supportProblem(browserFacts());
  globalThis.wotmMainThreadFiles = () => mainThreadFiles(browserFacts());
  globalThis.wotmFileSize = fileSize;
  globalThis.wotmGuardPointerLock = guardPointerLock;
}
