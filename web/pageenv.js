// ABOUTME: The game's environment as the page builds it: fixed values, a short allowlist of test switches from the page address, the
// ABOUTME: match's environment and the build id. The fixed values always win, so a link someone sends can't repoint the game.
'use strict';

// harmless switches for tests and direct matches (?WOTM_MATCH=5,1,9&WOTM_AIVSAI=1); nothing that names a file, a server or a cheat.
// WOTM_VBLANK_SPIN=0 is pacing only (the vblank timer thread sleeps instead of spinning), so it can differ between the players of a match
const FROM_ADDRESS = ['WOTM_MATCH', 'WOTM_AIVSAI', 'PS2X_STATS', 'WOTM_MAPEDIT', 'WOTM_VBLANK_SPIN'];

// `searchParams`: the address's URLSearchParams; `fixed`: values nothing may override; `matchEnv`: what an online match adds (or
// null); `build`: the build id, the only source of WOTM_BUILDID
function pageEnv(searchParams, fixed, matchEnv, build) {
  const env = {};
  for (const key of FROM_ADDRESS)
    if (searchParams.has(key)) env[key] = searchParams.get(key);
  Object.assign(env, matchEnv || {});
  delete env.WOTM_BUILDID;
  if (build) env.WOTM_BUILDID = build;
  return { ...env, ...fixed };
}

// the lobby an invite link (/?join=CODE) names: the server's 4-character code (native/net/wotm_net_proto.hpp kCodeLetters), or '' when
// the address has none or something else; it only prefills the front panel, never the game's environment
function inviteCode(searchParams) {
  const code = (searchParams.get('join') || '').toUpperCase();
  return /^[A-HJ-NP-Z2-9]{4}$/.test(code) ? code : '';
}

if (typeof module !== 'undefined' && module.exports) module.exports = { pageEnv, FROM_ADDRESS, inviteCode };
else Object.assign(globalThis, { pageEnv, inviteCode });
