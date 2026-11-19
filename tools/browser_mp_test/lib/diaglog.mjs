// ABOUTME: The page's DIAG reports as the harness saves them (--diag-out): which frames a page sends on its lobby socket are DIAG
// ABOUTME: lines (web/diag.js), and the line written for each, with the time and the player name.

// The DIAG line in a frame the page sent (text, or a Buffer for a binary frame), or null for any other lobby traffic
export function diagLineOf(payload) {
  const text = String(payload).replace(/\r?\n$/, '');
  return /^DIAG( |$)/.test(text) ? text : null;
}

// One line of the --diag-out file
export const diagRecord = (time, name, line) => `${time.toISOString()} ${name} ${line}\n`;
