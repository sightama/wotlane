// ABOUTME: The front panel's performance tip for Firefox players: a pref that has Firefox optimize the game's code fully right away instead
// ABOUTME: of in the background (less work per frame); on Android, the caveat that about:config opens only in Beta or Nightly. Dismissible.

export const FIREFOX_TIP = 'Faster in Firefox: open about:config (accept the warning), search for javascript.options.wasm_lazy_tiering, ' +
  'set it to false, then restart Firefox. The game\'s code is then fully optimized as it loads: about 14% less work per frame in big ' +
  'fights (8 monsters), little difference in 1 vs 1, for a slower first load. The setting applies to every website; to undo it, ' +
  'click the reset arrow beside it.';
export const ANDROID_CAVEAT = 'On Android, about:config is generally only available in Firefox Beta or Nightly.';
const DISMISSED_KEY = 'wotlane.firefoxTipDismissed';

function dismissed(storage) {
  try { return storage?.getItem(DISMISSED_KEY) === '1'; } catch { return false; }   // storage refused: show it again
}

// the tip for this browser ('' when there is none): `firefox` and `android` from the user agent, `storage` a Storage (localStorage)
export function firefoxTip({ firefox, android, storage }) {
  if (!firefox || dismissed(storage)) return '';
  return android ? FIREFOX_TIP + ' ' + ANDROID_CAVEAT : FIREFOX_TIP;
}

export function dismissFirefoxTip(storage) {
  try { storage?.setItem(DISMISSED_KEY, '1'); } catch { /* storage refused (a private window): it shows again next time */ }
}
