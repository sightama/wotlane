// ABOUTME: The game's environment as the page builds it: fixed values, a short allowlist of test switches from the page address, the
// ABOUTME: match's environment and the build id. The fixed values always win, so a link someone sends can't repoint the game.
'use strict';

// harmless switches for tests and direct matches (?WOTM_MATCH=5,1,9&WOTM_AIVSAI=1); nothing that names a file, a server or a cheat
const FROM_ADDRESS = ['WOTM_MATCH', 'WOTM_AIVSAI', 'PS2X_STATS', 'WOTM_MAPEDIT', 'WOTM_SNAPBENCH', 'WOTM_FIXEDSTEP', 'WOTM_SEED'];

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

if (typeof module !== 'undefined' && module.exports) module.exports = { pageEnv, FROM_ADDRESS };
else globalThis.pageEnv = pageEnv;
