// ABOUTME: Whether this browser can run the game, as a plain message for the front panel (or null when it can). Only Chromium-based
// ABOUTME: desktop browsers can: Firefox and Safari freeze while the game reads the disc, and phones lack the memory and keyboard.
'use strict';

// what the page can see: chromium (Chrome, Edge, Opera...), jspi (WebAssembly.Suspending), isolated (crossOriginIsolated), mobile
function supportProblem({ chromium, jspi, isolated, mobile }) {
  if (mobile) return 'This game needs a computer (Windows, Mac or Linux) with Chrome or Edge. Phones and tablets can\'t run it.';
  if (!chromium) return 'This game runs in Chrome or Edge. Firefox and Safari can\'t run it yet: open this page in Chrome or Edge.';
  if (!jspi) return 'Your browser is too old for this game: update Chrome or Edge, then reload the page.';
  if (!isolated) return 'The page loaded without the settings the game needs. Reload the page; if it keeps happening, tell Lane.';
  return null;
}

// the facts above, read from this browser
function browserFacts() {
  const brands = navigator.userAgentData?.brands ?? [];
  return {
    chromium: brands.some((b) => b.brand === 'Chromium'),
    jspi: typeof WebAssembly.Suspending === 'function',
    isolated: globalThis.crossOriginIsolated === true,
    mobile: navigator.userAgentData?.mobile ?? /Mobi|Android|iPad|iPhone/.test(navigator.userAgent),
  };
}

if (typeof module !== 'undefined' && module.exports) module.exports = { supportProblem };
else globalThis.wotmSupportProblem = () => supportProblem(browserFacts());
