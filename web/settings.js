// ABOUTME: The options the Settings page offers and how they reach the game: one settings.ini in the browser's private file system,
// ABOUTME: which the game reads at boot (native/wotm_settings.hpp). Every option here is one the browser game honors.

const toggle = (key, tab, group, label, hint, def, extra = {}) =>
  ({ key, tab, group, label, hint, def, choices: [['0', 'Off'], ['1', 'On']], ...extra });
const percents = (from, to, step) => {
  const out = [];
  for (let v = from; v <= to; v += step) out.push([String(v), v + '%']);
  return out;
};

export const TABS = [
  { id: 'saves', label: 'Saves' },
  { id: 'online', label: 'Online' },
  { id: 'game', label: 'Game' },
  { id: 'display', label: 'Display' },
  { id: 'mods', label: 'Mods' },
  { id: 'controls', label: 'Controls' },
];

// `online`: how an online match treats the option: { fixed: '<value>' } is the same for everyone (native/wotm_settings.hpp
// kNetKeys), { lobby: true } means the lobby's owner decides. The others stay each player's own.
export const OPTIONS = [
  { key: 'WOTM_RES', tab: 'display', group: 'Picture', label: 'Resolution', def: '720',
    hint: 'Height of the picture in pixels. Window size follows the page.',
    choices: [['window', 'Window size'], ['448', '448p'], ['720', '720p'], ['1080', '1080p'], ['1440', '1440p'], ['2160', '2160p']] },
  { key: 'WOTM_RENDERSCALE', tab: 'display', group: 'Picture', label: 'Render scale', def: '100',
    hint: 'Draws bigger or smaller, then fits the page. Above 100% smooths edges.',
    choices: [['50', '50%'], ['75', '75%'], ['100', '100%'], ['125', '125%'], ['150', '150%'], ['200', '200%']] },
  { key: 'WOTM_ASPECT', tab: 'display', group: 'Picture', label: 'Aspect ratio', def: 'dynamic',
    hint: 'Dynamic matches the shape of the page.',
    choices: [['dynamic', 'Dynamic'], ['4:3', '4:3'], ['16:9', '16:9'], ['16:10', '16:10'], ['21:9', '21:9'], ['32:9', '32:9']] },
  { key: 'WOTM_MSAA', tab: 'display', group: 'Picture', label: 'Anti-aliasing', def: '0',
    hint: 'Smooths the jagged edges of the 3D models.',
    choices: [['0', 'Off'], ['2', '2x'], ['4', '4x'], ['8', '8x']] },
  { key: 'WOTM_TEXFILTER', tab: 'display', group: 'Picture', label: 'Texture filter', def: 'bilinear',
    hint: 'Keeps textures sharp far away and at steep angles.',
    choices: [['bilinear', 'Bilinear'], ['trilinear', 'Trilinear'], ['2x', 'Anisotropic 2x'], ['4x', 'Anisotropic 4x'],
      ['8x', 'Anisotropic 8x'], ['16x', 'Anisotropic 16x']] },
  toggle('WOTM_INTERP', 'display', 'Picture', 'Interpolate frames', 'Smoother motion on screens faster than 60 Hz. Offline only: online it would add a frame of input lag.', '1'),
  { key: 'WOTM_HZ', tab: 'display', group: 'Picture', label: 'Frame rate', def: '0',
    hint: 'The game always runs at 60. On a faster screen, Display rate draws the frames in between; 60 saves power.',
    choices: [['0', 'Display rate'], ['60', '60']] },
  toggle('WOTM_HUDWIDE', 'display', 'Picture', 'Wide HUD', 'Health bars at the edges of a wide screen. Off keeps the original layout.', '1'),

  { key: 'WOTM_VOL_SFX', tab: 'game', group: 'Sound', label: 'Effects volume', def: '100',
    hint: 'Hits, roars, crashes and menu sounds. Only on this computer.', choices: percents(0, 100, 10) },
  { key: 'WOTM_VOL_MUSIC', tab: 'game', group: 'Sound', label: 'Music volume', def: '100',
    hint: 'The music of the menus and the levels. Only on this computer.', choices: percents(0, 100, 10) },
  { key: 'WOTM_ANNOUNCER', tab: 'game', group: 'Sound', label: 'Narrator', def: '1',
    hint: 'A voice calls out the monster and the level you pick, or the monster roars.',
    choices: [['0', 'Off'], ['1', 'Voice'], ['2', 'Monster roar']] },
  // `page`: kept in this browser by the page (localStorage), not in settings.ini, which the game rewrites without keys it does not know
  { key: 'MOVIES', page: true, tab: 'game', group: 'Startup', label: 'Startup movies', def: 'skippable',
    hint: 'The two boot movies play over the game while it loads (about 8 MB, fetched only when they play). Single player only. Off fetches nothing.',
    choices: [['skippable', 'Skippable'], ['original', 'Original (cannot be skipped)'], ['off', 'Off']] },
  toggle('WOTM_ENEMYBARS', 'game', 'Fighting', 'Enemy bars', 'Shows the health and energy of each enemy above their head.', '0'),
  toggle('WOTM_DAMAGEPOP', 'game', 'Fighting', 'Damage numbers', 'Shows how much damage each hit does.', '0'),
  toggle('WOTM_LOCKTOGGLE', 'game', 'Fighting', 'Lock-on toggle', 'Tap the lock-on button to lock on and again to let go.', '0',
    { online: { fixed: '0' } }),
  toggle('WOTM_SUPERJUMP', 'game', 'Modifiers', 'Super jump', 'Moon gravity: jumps go twice as high. From the next fight.', '0', { online: { lobby: true } }),
  toggle('WOTM_FASTMONSTERS', 'game', 'Modifiers', 'Fast monsters', 'Monsters walk, run and dash 1.5 times faster. From the next fight.', '0',
    { online: { lobby: true } }),
  toggle('WOTM_TURBO', 'game', 'Modifiers', 'Turbo', 'The whole fight runs at double speed. From the next fight.', '0', { online: { lobby: true } }),

  { key: 'WOTM_CAMERA', tab: 'controls', group: 'Camera', label: 'Camera style', def: '1',
    hint: 'Modern turns with the mouse or the right stick. Classic is the camera of the original game.',
    choices: [['1', 'Modern'], ['0', 'Classic']] },
  { key: 'WOTM_CAMSENS', tab: 'controls', group: 'Camera', label: 'Sensitivity', def: '5',
    hint: 'How fast the mouse or the right stick turns the camera.', choices: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map((n) => [String(n), String(n)]) },
  toggle('WOTM_CAMINVERT', 'controls', 'Camera', 'Invert Y', 'Flips up and down for the mouse and the right stick.', '0'),
  toggle('WOTM_CAMRECENTER', 'controls', 'Camera', 'Auto center', 'While you walk, the camera slowly swings back behind your monster.', '1'),
  toggle('WOTM_CLOSECAM', 'controls', 'Camera', 'Close-up camera', 'When two monsters fight up close, the game can switch to a shared view.', '1',
    { online: { fixed: '1' } }),
  toggle('WOTM_KBSEPARATE', 'controls', 'Devices', 'Keyboard is its own player', 'Off: the keyboard shares player 1 with the first gamepad.', '0'),
  { key: 'WOTM_PROMPTS', tab: 'controls', group: 'Devices', label: 'Button icons', def: 'auto',
    hint: 'Automatic shows the buttons of whatever you played with last.',
    choices: [['auto', 'Automatic'], ['original', 'Original'], ['keyboard', 'Keyboard'], ['xbox', 'Xbox'], ['nintendo', 'Nintendo']] },
  { key: 'WOTM_MENUCONFIRM', tab: 'controls', group: 'Devices', label: 'Menu confirm', def: 'bottom',
    hint: 'Which gamepad button accepts in the menus.', choices: [['bottom', 'Bottom button'], ['right', 'Right button']] },
];

export const optionsOf = (tab) => OPTIONS.filter((o) => o.tab === tab);

const HEADER = '# Recompiled options (main menu); same keys as the environment variables\n';
const FILE = 'settings.ini';

// The `KEY=value` lines of a settings.ini as a Map (a later line wins, as when the game reads it); comments and lines without `=` are skipped
export function parseIni(text) {
  const out = new Map();
  for (const raw of text.split('\n')) {
    const line = raw.replace(/\r$/, '');
    const eq = line.indexOf('=');
    if (!line || line[0] === '#' || eq < 0) continue;
    out.set(line.slice(0, eq), line.slice(eq + 1).replace(/^ +/, ''));   // the game skips the spaces before a value
  }
  return out;
}

// The text with `key` set: its lines changed in place, or a new line at the end. Every other line stays as it was (the game's own
// keys, the key bindings, comments)
export function setIni(text, key, value) {
  let found = false;
  const lines = text.split('\n').map((raw) => {
    const cr = raw.endsWith('\r') ? '\r' : '';
    if (!raw.startsWith(key + '=')) return raw;
    found = true;
    return `${key}=${value}${cr}`;
  });
  if (found) return lines.join('\n');
  const base = text === '' ? HEADER : text.endsWith('\n') ? text : text + '\n';
  return `${base}${key}=${value}\n`;
}

export function setOption(text, key, value) {
  const option = OPTIONS.find((o) => o.key === key);
  if (!option) throw new Error(`unknown setting ${key}`);
  if (!option.choices.some(([v]) => v === value)) throw new Error(`${value} is not a choice for ${key}`);
  return setIni(text, key, value);
}

// The drawing of a phone, or of a device measured too slow for its matches (web/rate.js), in an online match: the main thread that
// draws shares the device's cores with the game, which must keep up with the match, so such a device draws at 60 (no in-between
// frames) and at 75% scale there. A player who set either option keeps
// theirs; a line with the default value counts as unset (the game writes every key back). `text`: settings.ini. Returns env values
const LIGHT_MATCH_DRAWING = { WOTM_HZ: '60', WOTM_RENDERSCALE: '75' };
export function lightMatchDrawing(text) {
  const file = parseIni(text);
  const out = {};
  for (const [key, value] of Object.entries(LIGHT_MATCH_DRAWING)) {
    const def = OPTIONS.find((o) => o.key === key).def;
    if ((file.get(key) ?? def) === def) out[key] = value;
  }
  return out;
}

// What the page shows for an option: the file's value (the default without a line), and whether the page does not offer that value
// (a file edited by hand, or written by an older game)
export function choiceFor(option, text) {
  const value = parseIni(text).get(option.key) ?? option.def;
  return { value, custom: !option.choices.some(([v]) => v === value) };
}

// The settings file at the root of the browser's private file system (the game's /save); '' when there is none yet. Any other failure
// is thrown, so that nothing is written over a file that did not read
export async function readSettings(root) {
  let handle;
  try {
    handle = await root.getFileHandle(FILE);
  } catch (e) {
    if (e?.name === 'NotFoundError') return '';
    throw e;
  }
  return (await handle.getFile()).text();
}

let queue = Promise.resolve();

// One change, saved at once: the file is read, the option set and the whole file written back (a change made while another is still
// being saved waits its turn). Resolves to the new text
export function updateSetting(root, key, value) {
  const run = async () => {
    const next = setOption(await readSettings(root), key, value);
    const writable = await (await root.getFileHandle(FILE, { create: true })).createWritable();
    await writable.write(next);
    await writable.close();
    return next;
  };
  const result = queue.then(run);
  queue = result.catch(() => {});
  return result;
}

// Resolves once every change made so far is in the file (the game must not start before that)
export const settingsSaved = () => queue;
