// ABOUTME: The note a player's own front panel shows when their browser draws without the graphics card (WebGL in software: very slow,
// ABOUTME: and in an online match everyone slows to it). Nothing otherwise.

export const SOFTWARE_GL_NOTICE = 'This browser is drawing the game without your graphics card, so it may run very slowly (and slow down online matches for everyone). Turn on hardware or graphics acceleration in the browser\'s settings and restart it.';

// `softwareGl`: diag.js readSoftwareGl (1, 0 or undefined)
export function frontNotice({ softwareGl }) {
  return softwareGl === 1 ? SOFTWARE_GL_NOTICE : '';
}
