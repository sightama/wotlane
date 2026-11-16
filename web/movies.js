// ABOUTME: The startup movies, played by the page: the two boot movies as WebM files (tools/web_movies.py makes them from the disc) in an
// ABOUTME: overlay over the game while it loads. Skippable, original (not skippable) or off; off fetches nothing. The game never sees them.

export const MOVIE_MODES = ['skippable', 'original', 'off'];
export const DEFAULT_MODE = 'skippable';

export const normalizeMode = (v) => (MOVIE_MODES.includes(v) ? v : DEFAULT_MODE);

// the keys that skip a movie (Esc, Space, Enter); a key held down does not skip the next one
export const isSkipKey = (e) => e.type === 'keydown' && !e.repeat && ['Escape', ' ', 'Enter'].includes(e.key);

// movies.json (tools/web_movies.py) as [{name, file}]: file names are plain, so a manifest cannot point outside the movies folder
export function parseManifest(json) {
  if (json?.version !== 1 || !Array.isArray(json.movies) || !json.movies.length) throw new Error('movies.json is not a movie list');
  return json.movies.map((m) => {
    if (typeof m?.file !== 'string' || !/^[a-z0-9_]+\.webm$/.test(m.file)) throw new Error('movies.json names a file that is not a movie');
    return { name: String(m.name), file: m.file };
  });
}

// whether any gamepad has a button down that was not down at the last look; `previous` is the Map this keeps between looks
export function newGamepadPress(pads, previous) {
  let pressed = false;
  const seen = new Set();
  for (const pad of pads ?? []) {
    if (!pad) continue;
    seen.add(pad.index);
    const before = previous.get(pad.index) ?? [];
    const now = pad.buttons.map((b) => Boolean(b.pressed));
    if (now.some((down, i) => down && !before[i])) pressed = true;
    previous.set(pad.index, now);
  }
  for (const index of [...previous.keys()]) if (!seen.has(index)) previous.delete(index);
  return pressed;
}

// While a movie plays the game boots behind it, and its title music would play under the movie: the page's audio contexts are held
// (suspended, and resume() does nothing) until the movie is over. Install this before the game creates its context.
export function installAudioHold(win) {
  const contexts = [];
  let held = false;
  const Native = win.AudioContext ?? win.webkitAudioContext;
  if (!Native) return { hold() {} };
  const proto = Native.prototype;
  const Held = class extends Native {
    constructor(...args) {
      super(...args);
      contexts.push(this);
      this.resume = (...a) => (held ? Promise.resolve() : proto.resume.apply(this, a));
      if (held) proto.suspend.call(this);
    }
  };
  win.AudioContext = Held;
  return {
    hold(on) {
      held = on;
      for (const c of contexts) (on ? proto.suspend : proto.resume).call(c);
    },
  };
}

// Plays the startup movies in `overlay`'s `video`, one after the other. Resolves to what happened to each ('ended', 'skipped',
// 'failed'), or to 'off' (nothing was fetched) or 'unavailable' (no movie files on the server). Never rejects.
export async function playStartupMovies({
  mode, base = 'data/movies/', fetchImpl = (...a) => fetch(...a), overlay, video, hint, win = window,
  pads = () => navigator.getGamepads?.() ?? [], audio, log = () => {}, startTimeoutMs = 15000, stallTimeoutMs = 20000,
}) {
  mode = normalizeMode(mode);
  if (mode === 'off') return 'off';
  const hold = audio ?? installAudioHold(win);
  let list;
  try {
    const r = await fetchImpl(`${base}movies.json`, { cache: 'no-store' });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    list = parseManifest(await r.json());
  } catch (e) {
    log(`[movies] none played: ${e.message}`);
    return 'unavailable';
  }
  const skippable = mode === 'skippable';
  let skip = () => {};
  const swallow = (e) => {
    e.preventDefault();
    e.stopImmediatePropagation();   // the game's own key handlers never see a key meant for the movie
    if (skippable && isSkipKey(e)) skip('skipped');
  };
  const keys = ['keydown', 'keyup', 'keypress'];
  const onPointer = () => { if (skippable) skip('skipped'); };
  const seenPads = new Map();
  let watching = true;
  const watchPads = () => {
    if (!watching) return;
    if (skippable && newGamepadPress(pads(), seenPads)) skip('skipped');
    win.requestAnimationFrame?.(watchPads);
  };

  overlay.hidden = false;
  if (hint) hint.hidden = !skippable;
  hold.hold(true);
  for (const k of keys) win.addEventListener(k, swallow, true);
  overlay.addEventListener('pointerdown', onPointer);
  newGamepadPress(pads(), seenPads);   // a button already down is not a press
  watchPads();

  const outcomes = [];
  try {
    for (const movie of list) {
      outcomes.push(await new Promise((resolve) => {
        let timer;
        const finish = (how) => {
          clearTimeout(timer);
          for (const ev of Object.keys(on)) video.removeEventListener(ev, on[ev]);
          skip = () => {};
          resolve(how);
        };
        const on = {
          ended: () => finish('ended'),
          error: () => { log(`[movies] ${movie.file}: the browser could not play it`); finish('failed'); },
          playing: () => arm(stallTimeoutMs, 'stopped'),
          timeupdate: () => arm(stallTimeoutMs, 'stopped'),
        };
        // a movie that never starts, or stops for good (the file stalls), must not cover the game for ever, least of all when it cannot be skipped
        const arm = (ms, what) => {
          clearTimeout(timer);
          timer = setTimeout(() => { log(`[movies] ${movie.file}: ${what === 'stopped' ? 'stopped' : 'did not start'}`); finish('failed'); }, ms);
        };
        for (const ev of Object.keys(on)) video.addEventListener(ev, on[ev]);
        skip = finish;
        arm(startTimeoutMs, 'start');
        video.src = `${base}${movie.file}`;
        // a browser that refuses sound without a gesture still plays the picture
        Promise.resolve(video.play()).catch(() => { video.muted = true; return video.play(); }).catch(() => finish('failed'));
      }));
    }
  } finally {
    watching = false;
    for (const k of keys) win.removeEventListener(k, swallow, true);
    overlay.removeEventListener('pointerdown', onPointer);
    overlay.hidden = true;
    video.removeAttribute('src');
    video.load?.();
    hold.hold(false);
  }
  return outcomes;
}
