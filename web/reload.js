// ABOUTME: Reloads the page once the work before it (the save folder, an online match's last report) is done, or after a deadline.
// ABOUTME: The deadline is checked from posted tasks, never a timer: a throttled page may not run timers, and QUIT must still return.
'use strict';

// Settles once `work` has settled (failed or not) or `ms` have passed, whichever comes first. `now()` in ms; `post(cb)` runs cb in a
// later task. The default is a MessageChannel message, which a hidden or timer-starved page still gets
function within(work, ms, { now = () => performance.now(), post = postTask } = {}) {
  return new Promise((resolve) => {
    const start = now();
    let settled = false;
    const finish = () => { settled = true; resolve(); };
    const check = () => { if (settled) return; if (now() - start >= ms) finish(); else post(check); };
    Promise.resolve(work).then(finish, finish);
    post(check);
  });
}

// Reloads once `work` is done or `ms` have passed
function reloadWhenDone(work, { ms = 2000, now, post, reload = () => location.reload() } = {}) {
  return within(work, ms, { now, post }).then(reload);
}

function postTask(cb) {
  const channel = new MessageChannel();
  channel.port1.onmessage = () => { channel.port1.close(); cb(); };
  channel.port2.postMessage(0);
}

if (typeof module !== 'undefined' && module.exports) module.exports = { reloadWhenDone, within };
else Object.assign(window, { wotmReloadWhenDone: reloadWhenDone, wotmWithin: within });
