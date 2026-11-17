// ABOUTME: The lock a tab holds while its game runs (web/boot.js takes it), so another tab of the same profile can tell that a game is
// ABOUTME: running: the settings and the saves are not changed under a running game, which writes them again on its own.

export const GAME_LOCK = 'wotlane-game';

// `locks`: navigator.locks (or a stand-in); false where the browser has none
export async function gameRunningElsewhere(locks) {
  try {
    return Boolean((await locks?.query?.())?.held?.some((l) => l.name === GAME_LOCK));
  } catch {
    return false;
  }
}
