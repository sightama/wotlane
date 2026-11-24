// ABOUTME: Reloads the page once the work before it (the save folder, an online match's last report) is done, or after a deadline.
// ABOUTME: The deadline is checked from posted tasks, never a timer: a throttled page may not run timers, and QUIT must still return.
'use strict';

// Reloads once `work` has settled (failed or not) or `ms` have passed, whichever comes first, and never twice. `now()` in ms;
// `post(cb)` runs cb in a later task. The default is a MessageChannel message, which a hidden or timer-starved page still gets
function reloadWhenDone(work, { ms = 2000, now = () => performance.now(), post = postTask, reload = () => location.reload() } = {}) {
  let reloaded = false;
  const go = () => { if (!reloaded) { reloaded = true; reload(); } };
  const start = now();
  const check = () => { if (reloaded) return; if (now() - start >= ms) go(); else post(check); };
  Promise.resolve(work).then(go, go);
  post(check);
}

function postTask(cb) {
  const channel = new MessageChannel();
  channel.port1.onmessage = () => { channel.port1.close(); cb(); };
  channel.port2.postMessage(0);
}

if (typeof module !== 'undefined' && module.exports) module.exports = { reloadWhenDone };
else window.wotmReloadWhenDone = reloadWhenDone;
