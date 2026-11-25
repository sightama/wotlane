// ABOUTME: The loading screen's page side: draws a LoadingProgress view into the screen's parts and picks the tip (one at a time, the
// ABOUTME: player asks for the next: nothing here moves or rotates by itself). Needs loading.js and controls.js, loaded before it.
'use strict';

// `root`: the #loading element. `random` picks the first tip. Returns { render(view), nextTip() }.
function wotmLoadingScreen(root, { random = Math.random } = {}) {
  const $ = (id) => root.querySelector('#' + id);
  const parts = { headline: $('loadHeadline'), amount: $('loadAmount'), eta: $('loadEta'), note: $('loadNote'), tip: $('loadTip'), bar: $('loadBar') };
  const tips = loadingTips(CONTROLS, keyLabel);
  let tipIndex = Math.floor(random() * tips.length);
  const set = (el, text) => { if (el.textContent !== text) el.textContent = text; };
  const showTip = () => set(parts.tip, tips[tipIndex]);
  const nextTip = () => { tipIndex = (tipIndex + 1) % tips.length; showTip(); };
  $('loadNextTip').addEventListener('click', nextTip);
  showTip();
  const render = (view) => {
    set(parts.headline, view.headline);
    set(parts.amount, view.amount);
    set(parts.eta, view.eta);
    set(parts.note, view.note);
    // no value: the bar is indeterminate until a percent is known
    if (view.percent === null) parts.bar.removeAttribute('value');
    else parts.bar.value = view.percent;
    parts.bar.setAttribute('aria-valuetext', [view.headline, view.amount, view.eta].filter(Boolean).join(', '));
  };
  return { render, nextTip };
}

if (typeof module !== 'undefined' && module.exports) module.exports = { wotmLoadingScreen };
else globalThis.wotmLoadingScreen = wotmLoadingScreen;
