// ABOUTME: The note a player's own front panel shows about their browser: drawing without the graphics card (WebGL in software: very
// ABOUTME: slow, and in an online match everyone slows to it), or Brave (brave.js). Nothing for a browser with neither.
import { BRAVE_NOTICE } from './brave.js';

export const SOFTWARE_GL_NOTICE = 'This browser is drawing the game without your graphics card, so it may run very slowly (and slow down online matches for everyone). Turn on hardware or graphics acceleration in the browser\'s settings and restart it.';

// `softwareGl`: diag.js readSoftwareGl (1, 0 or undefined); `brave`: brave.js detectBrave
export function frontNotice({ brave, softwareGl }) {
  if (softwareGl === 1) return SOFTWARE_GL_NOTICE;
  return brave ? BRAVE_NOTICE : '';
}
