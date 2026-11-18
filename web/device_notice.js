// ABOUTME: The note a player's own front panel shows about their browser: drawing without the graphics card (WebGL in software: a few
// ABOUTME: frames a second, and in an online match everyone slows to it), or Brave (brave.js). Nothing for a browser with neither.
import { BRAVE_NOTICE } from './brave.js';

export const SOFTWARE_GL_NOTICE = 'This browser is drawing the game without your graphics card, so it will run at a few frames a second (and slow down online matches for everyone). Turn on hardware or graphics acceleration in the browser\'s settings and restart it.';

// `softwareGl`: diag.js readSoftwareGl (1, 0 or undefined); `brave`: brave.js detectBrave
export function frontNotice({ brave, softwareGl }) {
  if (softwareGl === 1) return SOFTWARE_GL_NOTICE;
  return brave ? BRAVE_NOTICE : '';
}
