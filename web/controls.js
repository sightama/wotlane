// ABOUTME: The game's default keyboard layout as data for the front panel's Controls card: what each key does, in plain words.
// ABOUTME: Taken from the native mapping (native/wotm_settings.hpp kDefaultKb); web/controls.test.mjs fails when they drift apart.
'use strict';

// one row per action the keyboard drives: the native bind name (null for the mouse turning the camera), what it does, the keys by
// their native names (wotm_settings.hpp kKeyNames)
const CONTROLS = [
  { bind: 'STICK_UP', does: 'Move forward', keys: ['W'] },
  { bind: 'STICK_LEFT', does: 'Move left', keys: ['A'] },
  { bind: 'STICK_DOWN', does: 'Move back', keys: ['S'] },
  { bind: 'STICK_RIGHT', does: 'Move right', keys: ['D'] },
  { bind: 'WALK', does: 'Walk slowly (hold)', keys: ['LALT'] },
  { bind: null, does: 'Turn the camera (click the game first)', keys: ['MOUSE'] },
  { bind: 'SQUARE', does: 'Light attack', keys: ['MOUSE_LEFT'] },
  { bind: 'TRIANGLE', does: 'Heavy attack (in menus: back)', keys: ['MOUSE_RIGHT'] },
  { bind: 'CROSS', does: 'Jump (in menus: choose)', keys: ['SPACE'] },
  { bind: 'CIRCLE', does: 'Grab', keys: ['E', 'F'] },
  { bind: 'BLOCK', does: 'Block (hold)', keys: ['LSHIFT'] },
  { bind: 'LOCK', does: 'Lock on to an enemy', keys: ['Q', 'MOUSE_MIDDLE'] },
  { bind: 'TARGET_NEXT', does: 'Next target', keys: ['TAB', 'WHEEL_DOWN'] },
  { bind: 'TARGET_PREV', does: 'Previous target', keys: ['R', 'WHEEL_UP'] },
  { bind: 'STRAFE_LEFT', does: 'Strafe left (locked on)', keys: ['Z'] },
  { bind: 'STRAFE_RIGHT', does: 'Strafe right (locked on)', keys: ['C'] },
  { bind: 'TRIANGLE_CIRCLE', does: 'Special attack 1', keys: ['1'] },
  { bind: 'CROSS_SQUARE', does: 'Special attack 2', keys: ['2'] },
  { bind: 'SELECT', does: 'Taunt', keys: ['G', 'LCTRL'] },
  { bind: 'START', does: 'Pause (in menus: Esc goes back)', keys: ['ENTER', 'ESC'] },
  { bind: 'UP', does: 'Menu up', keys: ['UP'] },
  { bind: 'DOWN', does: 'Menu down', keys: ['DOWN'] },
  { bind: 'LEFT', does: 'Menu left', keys: ['LEFT'] },
  { bind: 'RIGHT', does: 'Menu right', keys: ['RIGHT'] },
];

const KEY_LABELS = {
  ESC: 'Esc', ENTER: 'Enter', TAB: 'Tab', SPACE: 'Space', LSHIFT: 'Left Shift', LCTRL: 'Left Ctrl', LALT: 'Left Alt',
  UP: 'Up arrow', DOWN: 'Down arrow', LEFT: 'Left arrow', RIGHT: 'Right arrow',
  MOUSE: 'Mouse', MOUSE_LEFT: 'Left mouse button', MOUSE_RIGHT: 'Right mouse button', MOUSE_MIDDLE: 'Middle mouse button',
  WHEEL_UP: 'Mouse wheel up', WHEEL_DOWN: 'Mouse wheel down',
};

const keyLabel = (name) => KEY_LABELS[name] ?? name;

// the rows of the card: [what it does, "Key, Key"]
const controlRows = () => CONTROLS.map((c) => [c.does, c.keys.map(keyLabel).join(', ')]);

if (typeof module !== 'undefined' && module.exports) module.exports = { CONTROLS, keyLabel, controlRows };
else Object.assign(globalThis, { CONTROLS, keyLabel, controlRows });
