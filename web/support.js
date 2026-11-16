// ABOUTME: Whether this browser can run the game, as a plain message for the front panel (or null when it can), and how it reads the
// ABOUTME: disc. Chromium-based browsers and Firefox (with JSPI) can; Safari freezes while the game reads the disc, and phones are out.
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

// the disc in byte ranges read without blocking the main thread (native/wotm_webdisc.cpp) everywhere but Chromium, whose worker
// fetches go on while the main thread waits (the WASMFS fetch backend, the proven path)
function rangeDisc({ chromium }) {
  return !chromium;
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

if (typeof module !== 'undefined' && module.exports) module.exports = { supportProblem, rangeDisc };
else {
  globalThis.wotmSupportProblem = () => supportProblem(browserFacts());
  globalThis.wotmRangeDisc = () => rangeDisc(browserFacts());
}
