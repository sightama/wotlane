// ABOUTME: The page's Send report card: a player flags a bad match with a short note; the report (the last match's numbers, this device,
// ABOUTME: the note) goes to the game server's log over the lobby connection, or a short-lived one when the player is not in a lobby.

// Sends `line` (a DIAG line, web/diag.js) and resolves 'sent' once the server has read it, or 'unsupported' for a server that has no
// diagnostics. `lobby`: the lobby connection the player is in (left open), or null: `connect()` then opens one that is closed afterwards.
// Rejects when the server can't be reached or says nothing for `timeoutMs`.
export async function deliverReport({ line, lobby, connect, timeoutMs = 6000 }) {
  let own = null;
  let done = false;
  let timer;
  const limit = new Promise((_, reject) => { timer = setTimeout(() => reject(new Error('the game server did not answer')), timeoutMs); });
  limit.catch(() => {});   // after the work has finished the limit's rejection is nobody's
  try {
    let client = lobby;
    if (!client) {
      const opening = connect();
      opening.then((c) => { if (done) c.close(); else own = c; }, () => {});   // a connection that opens after the time limit is closed too
      client = await Promise.race([opening, limit]);
    }
    return (await Promise.race([client.sendDiag(line), limit])) ? 'sent' : 'unsupported';
  } finally {
    done = true;
    clearTimeout(timer);
    own?.close();
  }
}

// what the card says about the outcome of deliverReport: 'sent', 'unsupported' or the Error it rejected with
export function reportMessage(outcome) {
  if (outcome === 'sent') return 'Report sent. Thank you.';
  if (outcome === 'unsupported') return "The game server can't take reports yet.";
  return `Could not send the report: ${outcome.message}`;
}

const $ = (id) => document.getElementById(id);

// The card replaces the front panel's home card or the Settings card, whichever opened it, and Back returns to that one.
// `send(note)` builds and delivers the report; it resolves with deliverReport's outcome.
export function initReport({ send }) {
  const card = $('report');
  let from = $('home');
  const open = (origin) => {
    from = origin;
    origin.hidden = true;
    card.hidden = false;
    $('reportMsg').textContent = '';
    $('reportNote').focus();
  };
  const close = () => {
    card.hidden = true;
    from.hidden = false;
  };
  $('openReport').addEventListener('click', () => open($('home')));
  $('openReportFromSettings').addEventListener('click', () => open($('settings')));
  $('closeReport').addEventListener('click', close);
  card.addEventListener('keydown', (e) => { if (e.key === 'Escape') close(); });
  $('sendReport').addEventListener('click', async () => {
    $('sendReport').disabled = true;
    $('reportMsg').textContent = 'Sending';
    try {
      $('reportMsg').textContent = reportMessage(await send($('reportNote').value));
      $('reportNote').value = '';
    } catch (e) {
      $('reportMsg').textContent = reportMessage(e);
    }
    $('sendReport').disabled = false;
  });
  return { available(on) { $('openReport').hidden = !on; } };
}
