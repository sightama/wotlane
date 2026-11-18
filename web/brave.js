// ABOUTME: Whether this browser is Brave, and the notice a Brave player is shown. Brave's Shields (and its masked graphics) slowed online
// ABOUTME: matches to a few frames a second on some computers; the notice says which browsers work and what to turn off.

export const BRAVE_NOTICE = "Brave's Shields can slow this game a lot. For the best experience use Chrome, Edge or Firefox, or turn Shields off for playwotm.com and enable graphics acceleration.";

// Brave answers navigator.brave.isBrave() (a promise); no other browser has navigator.brave. Anything else, or a failure, is not Brave
export async function detectBrave(nav) {
  try {
    return (await nav?.brave?.isBrave?.()) === true;
  } catch {
    return false;
  }
}
