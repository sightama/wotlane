// ABOUTME: Keyboard remapping for the Controls card: browser key events to the game's key names, the KB_<BUTTON> lines of settings.ini,
// ABOUTME: conflicts between buttons and presets. The names and the layout number are the native ones (native/wotm_settings.hpp).
import { setIni, parseIni } from './settings.js';

// kKbLayout: the game applies the KB_ lines of a file only when it says this layout
export const KB_LAYOUT = 4;
// the most keys one button takes (kbPair)
export const MAX_KEYS = 2;

// KeyboardEvent.code (the key's place on the keyboard, not the letter it types) -> the native key name (kKeyNames). F1 to F12 are not
// here: the runtime keeps them (debug menu, free camera, screenshots, fullscreen). Escape cancels the capture
const KEY_NAMES = {
  Space: 'SPACE', Quote: 'APOSTROPHE', Comma: 'COMMA', Minus: 'MINUS', Period: 'PERIOD', Slash: 'SLASH',
  Semicolon: 'SEMICOLON', Equal: 'EQUAL', BracketLeft: 'LBRACKET', Backslash: 'BACKSLASH', BracketRight: 'RBRACKET', Backquote: 'GRAVE',
  Enter: 'ENTER', Tab: 'TAB', Backspace: 'BACKSPACE', Insert: 'INSERT', Delete: 'DELETE',
  ArrowRight: 'RIGHT', ArrowLeft: 'LEFT', ArrowDown: 'DOWN', ArrowUp: 'UP', PageUp: 'PGUP', PageDown: 'PGDN', Home: 'HOME', End: 'END',
  CapsLock: 'CAPSLOCK', NumpadDecimal: 'KPDECIMAL', NumpadDivide: 'KPDIVIDE', NumpadMultiply: 'KPMULTIPLY', NumpadSubtract: 'KPMINUS',
  NumpadAdd: 'KPPLUS', NumpadEnter: 'KPENTER',
  ShiftLeft: 'LSHIFT', ControlLeft: 'LCTRL', AltLeft: 'LALT', ShiftRight: 'RSHIFT', ControlRight: 'RCTRL', AltRight: 'RALT',
};
for (let c = 65; c <= 90; c++) KEY_NAMES['Key' + String.fromCharCode(c)] = String.fromCharCode(c);
for (let d = 0; d <= 9; d++) {
  KEY_NAMES['Digit' + d] = String(d);
  KEY_NAMES['Numpad' + d] = 'KP' + d;
}

// native name -> KeyboardEvent.code, for the names a keyboard has
const CODE_OF = Object.fromEntries(Object.entries(KEY_NAMES).map(([code, name]) => [name, code]));

export const keyNameOfCode = (code) => KEY_NAMES[code] ?? null;
export const allKeyNames = () => Object.values(KEY_NAMES);

// MouseEvent.button (0 left, 1 middle, 2 right, 3 back, 4 forward) -> native name
const MOUSE_NAMES = ['MOUSE_LEFT', 'MOUSE_MIDDLE', 'MOUSE_RIGHT', 'MOUSE_SIDE', 'MOUSE_EXTRA'];
export const mouseNameOfButton = (button) => MOUSE_NAMES[button] ?? null;
export const wheelNameOfDelta = (deltaY) => (deltaY < 0 ? 'WHEEL_UP' : deltaY > 0 ? 'WHEEL_DOWN' : null);

// What a key reads as on this keyboard: a letter or sign from the Keyboard Map API (`layoutMap`, navigator.keyboard.getLayoutMap(),
// so an AZERTY player sees their own letters), else `baseLabel`'s words ("Left Shift")
export function labelOfKey(name, layoutMap, baseLabel) {
  const typed = layoutMap?.get?.(CODE_OF[name]);
  if (typed && /^\S$/u.test(typed)) return typed.toUpperCase();
  return baseLabel(name);
}

// The keys of every row in `controls` (web/controls.js CONTROLS) as a Map of native bind name -> key names, from settings.ini text:
// the defaults without KB_ lines, and also when the file is of another layout, which the game ignores too
export function readBinds(text, controls) {
  const file = parseIni(text);
  const used = file.get('KB_LAYOUT') === String(KB_LAYOUT);
  const binds = new Map();
  for (const c of controls.filter((x) => x.bind)) {
    const line = used ? file.get('KB_' + c.bind) : undefined;
    binds.set(c.bind, line === undefined ? [...c.keys] : line.split(',').map((k) => k.trim()).filter(Boolean).slice(0, MAX_KEYS));
  }
  return binds;
}

// `text` with the keys of every bind in `binds` written as KB_ lines (and the layout line the game needs to read them); other lines stay
export function writeBinds(text, binds) {
  let next = setIni(text, 'KB_LAYOUT', String(KB_LAYOUT));
  for (const [bind, keys] of binds) next = setIni(next, 'KB_' + bind, keys.join(','));
  return next;
}

// `key` on `bind` in place `slot` (0 or 1; a slot past the last key adds one). A button that had the key swaps it for the key this slot
// held (the game's own panel does the same), or loses it when the slot was empty; those buttons are returned as
// `swapped`: [{ bind, gets }] so the card can say so. Returns { binds, swapped }
export function assignKey(binds, bind, slot, key) {
  const given = binds.get(bind)[slot] ?? null;
  const next = new Map([...binds].map(([b, keys]) => [b, [...keys]]));
  const swapped = [];
  for (const [other, keys] of next) {
    if (other === bind || !keys.includes(key)) continue;
    const gets = given && !keys.includes(given) ? given : null;
    next.set(other, gets ? keys.map((k) => (k === key ? gets : k)) : keys.filter((k) => k !== key));
    swapped.push({ bind: other, gets });
  }
  const rest = binds.get(bind).filter((k, i) => i !== slot && k !== key);   // this button's other key, unless it is `key` itself
  next.set(bind, (slot === 0 ? [key, ...rest] : [...rest, key]).slice(0, MAX_KEYS));
  return { binds: next, swapped };
}

// the key in place `slot` of `bind` taken away
export function clearKey(binds, bind, slot) {
  const next = new Map(binds);
  next.set(bind, binds.get(bind).filter((_, i) => i !== slot));
  return next;
}

// Presets: what changes from the game's defaults (the rows' own keys). Movement and the menu's D-pad swap places in Arrows
export const PRESETS = [
  { id: 'wasd', label: 'WASD and mouse (default)', keys: {} },
  { id: 'arrows', label: 'Arrow keys move', keys: {
    STICK_UP: ['UP'], STICK_LEFT: ['LEFT'], STICK_DOWN: ['DOWN'], STICK_RIGHT: ['RIGHT'],
    UP: ['W'], LEFT: ['A'], DOWN: ['S'], RIGHT: ['D'],
  } },
];

// the binds of `controls` with the preset's keys over the defaults
export function presetBinds(controls, preset) {
  return new Map(controls.filter((c) => c.bind).map((c) => [c.bind, [...(preset.keys[c.bind] ?? c.keys)]]));
}
