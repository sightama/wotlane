// ABOUTME: Gamepads for the page: any pad (standard or not) and the touch pad's virtual pad become the standard-layout pads raylib reads
// ABOUTME: through navigator.getGamepads (slots, hot-plug, events, controller names). Loaded before the game; nothing here needs the DOM.
'use strict';

// the Gamepad API's standard button layout, which raylib's web platform reads by index (the PS2 names on its right side)
const STD = {
  CROSS: 0, CIRCLE: 1, SQUARE: 2, TRIANGLE: 3, L1: 4, R1: 5, L2: 6, R2: 7, SELECT: 8, START: 9, L3: 10, R3: 11,
  UP: 12, DOWN: 13, LEFT: 14, RIGHT: 15, HOME: 16,
};
const STD_BUTTONS = 17;
const MAX_PADS = 4;   // raylib's MAX_GAMEPADS, and the game's players
const VIRTUAL_ID = 'Touch screen controls';

const press = (pressed, value = pressed ? 1 : 0) => ({ pressed, value, touched: pressed });
const quiet = () => Array.from({ length: STD_BUTTONS }, () => press(false));
const num = (v) => (Number.isFinite(v) ? v : 0);

// a hat axis (Chrome on Linux, generic pads) reports eight directions in steps of 2/7 from -1 (up) clockwise, and 9/7 at rest
function hatDirections(v) {
  const step = Math.round((num(v) + 1) * 3.5);   // 0 up, 1 up-right, 2 right, ... 7 up-left, 8 rest
  const up = [0, 1, 7].includes(step), right = [1, 2, 3].includes(step), down = [3, 4, 5].includes(step), left = [5, 6, 7].includes(step);
  return { up, down, left, right };
}

// a trigger that rests at -1 and goes to 1, as a button value 0..1
const triggerValue = (v) => Math.min(1, Math.max(0, (num(v) + 1) / 2));

// Pads the browser doesn't map itself (mapping ''): the layout is guessed from the shape of what they report. Verified with synthetic
// pads only; a pad this guesses wrong still works through the browser's own mapping when it has one.
function fallbackLayout(raw) {
  const axes = Array.from(raw.axes ?? [], num);
  const src = Array.from(raw.buttons ?? [], (b) => ({ pressed: !!b?.pressed, value: num(b?.value ?? (b?.pressed ? 1 : 0)) }));
  const btn = (i) => src[i] ?? { pressed: false, value: 0 };
  const out = quiet();
  const set = (i, b) => { out[i] = press(b.pressed || b.value > 0.5, b.value); };
  const flag = (i, on) => { out[i] = press(on); };
  const xboxAxes = axes.length === 8 && src.length <= 12;   // triggers and hat among the axes (Linux xpad)
  [STD.CROSS, STD.CIRCLE, STD.SQUARE, STD.TRIANGLE, STD.L1, STD.R1].forEach((to, i) => set(to, btn(i)));
  let stick = [axes[0] ?? 0, axes[1] ?? 0, axes[2] ?? 0, axes[3] ?? 0];
  let hat = null;
  if (xboxAxes) {
    stick = [axes[0], axes[1], axes[3], axes[4]];
    out[STD.L2] = press(triggerValue(axes[2]) > 0.5, triggerValue(axes[2]));
    out[STD.R2] = press(triggerValue(axes[5]) > 0.5, triggerValue(axes[5]));
    [STD.SELECT, STD.START, STD.HOME, STD.L3, STD.R3].forEach((to, i) => set(to, btn(6 + i)));
    hat = { up: axes[7] < -0.5, down: axes[7] > 0.5, left: axes[6] < -0.5, right: axes[6] > 0.5 };
  } else {
    [STD.L2, STD.R2, STD.SELECT, STD.START, STD.L3, STD.R3].forEach((to, i) => set(to, btn(6 + i)));
    if (src.length >= 16) [STD.UP, STD.DOWN, STD.LEFT, STD.RIGHT].forEach((to, i) => set(to, btn(12 + i)));
    else if (axes.length >= 10) hat = hatDirections(axes[9]);
  }
  if (hat) { flag(STD.UP, hat.up); flag(STD.DOWN, hat.down); flag(STD.LEFT, hat.left); flag(STD.RIGHT, hat.right); }
  return { axes: stick, buttons: out };
}

// `raw`: a Gamepad from the browser (or shaped like one). A standard-mapped pad passes through; any other is mapped by its shape.
// What comes out is the plain object raylib's emscripten reads (index is the hub's slot, set by the caller).
function normalizeGamepad(raw) {
  let axes, buttons;
  if (raw.mapping === 'standard') {
    axes = Array.from({ length: 4 }, (_, i) => num(raw.axes?.[i]));
    buttons = Array.from({ length: STD_BUTTONS }, (_, i) => {
      const b = raw.buttons?.[i];
      return b ? press(!!b.pressed, num(b.value ?? (b.pressed ? 1 : 0))) : press(false);
    });
  } else {
    ({ axes, buttons } = fallbackLayout(raw));
    axes = Array.from({ length: 4 }, (_, i) => num(axes[i]));
  }
  return {
    id: raw.id ?? '', index: raw.index ?? 0, connected: true, mapping: 'standard', timestamp: raw.timestamp ?? 0, axes, buttons,
    vibrationActuator: raw.vibrationActuator, hapticActuators: raw.hapticActuators,
  };
}

// "Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)" -> "Xbox Wireless Controller"; Firefox's
// "054c-09cc-Wireless Controller" -> "Wireless Controller"
function padLabel(id) {
  const label = String(id ?? '').replace(/\s*\([^)]*\)\s*$/, '').replace(/^[0-9a-f]{4}-[0-9a-f]{4}-/i, '').trim();
  return label || 'Controller';
}

// the text of the toast an event earns, or null (a touch pad coming and going, or a repeated announcement, says nothing)
function toastText(event) {
  if (event.kind !== 'real') return null;
  if (event.type === 'connected') return `Controller connected: ${padLabel(event.label)} (player ${event.player})`;
  if (event.type === 'disconnected') return `Controller disconnected: ${padLabel(event.label)}`;
  return null;
}

// A finger's offset from where the stick started -> the stick's x, y (-1..1): nothing inside the dead zone, then a ramp that starts
// at zero, a full push at `radius`
function stickVector(dx, dy, radius, deadZone) {
  const len = Math.hypot(dx, dy) / radius;
  if (len <= deadZone) return [0, 0];
  const strength = Math.min(1, (len - deadZone) / (1 - deadZone));
  return [dx / radius / len * strength, dy / radius / len * strength];
}

// The touch screen as a pad. The overlay presses and releases buttons by standard index and sets the two sticks; a press stays down
// for at least minHoldMs, so a tap shorter than the game's pad reads is still read.
class VirtualPad {
  constructor({ minHoldMs = 80 } = {}) {
    this.minHoldMs = minHoldMs;
    this.down = new Array(STD_BUTTONS).fill(0);       // touches currently holding each button
    this.holdUntil = new Array(STD_BUTTONS).fill(-1);
    this.sticks = { left: [0, 0], right: [0, 0] };
  }

  press(index, now) {
    this.down[index]++;
    this.holdUntil[index] = now + this.minHoldMs;
  }

  release(index) {
    this.down[index] = Math.max(0, this.down[index] - 1);
  }

  setStick(which, x, y) {
    this.sticks[which] = [x, y];
  }

  snapshot(now) {
    const l = this.sticks.left, r = this.sticks.right;
    return {
      id: VIRTUAL_ID, index: 0, connected: true, mapping: 'standard', timestamp: now, axes: [l[0], l[1], r[0], r[1]],
      buttons: this.down.map((n, i) => press(n > 0 || now < this.holdUntil[i])),
      vibrationActuator: this.vibrationActuator, hapticActuators: undefined,
    };
  }
}

// Which pad is in which of the game's four slots. Real pads keep their slot while they stay; the touch pad takes one only while it is
// wanted and no real pad is there (a real pad would otherwise be player 2, behind a screen nobody is touching).
// `getRaw`: () => the browser's gamepad list; `onEvent`: gets {type: 'connected' | 'disconnected' | 'announce', slot, kind: 'real' |
// 'virtual', label, player, pad}; `virtual`: a VirtualPad; `virtualWanted`: () => boolean.
class PadHub {
  constructor({ getRaw, onEvent = () => {}, virtual = null, virtualWanted = () => false }) {
    this.getRaw = getRaw;
    this.onEvent = onEvent;
    this.virtual = virtual;
    this.virtualWanted = virtualWanted;
    this.slots = new Array(MAX_PADS).fill(null);   // {kind, key, label} or null
  }

  playerOf(slot) {
    return this.slots.slice(0, slot + 1).filter(Boolean).length;
  }

  freeSlot() {
    return this.slots.findIndex((s) => !s);
  }

  emit(type, slot, entry, pad) {
    this.onEvent({ type, slot, kind: entry.kind, label: entry.label, player: this.playerOf(slot), pad });
  }

  drop(slot) {
    const entry = this.slots[slot];
    this.slots[slot] = null;
    this.onEvent({ type: 'disconnected', slot, kind: entry.kind, label: entry.label, player: 0, pad: null });
  }

  // reads the browser's list, updates the slots and announces the changes; returns the raw pads by browser index
  poll(now) {
    const raws = new Map();
    for (const raw of Array.from(this.getRaw() ?? [])) if (raw && raw.connected !== false) raws.set(raw.index, raw);
    const keyOf = (raw) => `${raw.index}:${raw.id}`;
    const present = new Set([...raws.values()].map(keyOf));
    this.slots.forEach((s, i) => { if (s && s.kind === 'real' && !present.has(s.key)) this.drop(i); });
    const wantVirtual = !!this.virtual && this.virtualWanted() && raws.size === 0;
    this.slots.forEach((s, i) => { if (s && s.kind === 'virtual' && !wantVirtual) this.drop(i); });
    for (const raw of raws.values()) {
      if (this.slots.some((s) => s && s.key === keyOf(raw))) continue;
      const slot = this.freeSlot();
      if (slot < 0) break;
      this.slots[slot] = { kind: 'real', key: keyOf(raw), label: raw.id };
      this.emit('connected', slot, this.slots[slot], this.padAt(slot, raws, now));
    }
    if (wantVirtual && !this.slots.some((s) => s && s.kind === 'virtual')) {
      const slot = this.freeSlot();
      if (slot >= 0) {
        this.slots[slot] = { kind: 'virtual', key: 'virtual', label: VIRTUAL_ID };
        this.emit('connected', slot, this.slots[slot], this.padAt(slot, raws, now));
      }
    }
    return raws;
  }

  padAt(slot, raws, now) {
    const entry = this.slots[slot];
    if (!entry) return null;
    if (entry.kind === 'virtual') return { ...this.virtual.snapshot(now), index: slot };
    const raw = [...raws.values()].find((r) => `${r.index}:${r.id}` === entry.key);
    return raw ? { ...normalizeGamepad(raw), index: slot } : null;
  }

  // what navigator.getGamepads() answers: one entry per slot
  snapshot(now) {
    const raws = this.poll(now);
    return this.slots.map((_, i) => this.padAt(i, raws, now));
  }

  // every pad in its slot again (a game that starts listening for connections after the pad was plugged in still hears of it)
  announce(now) {
    const raws = this.poll(now);
    this.slots.forEach((entry, i) => { if (entry) this.emit('announce', i, entry, this.padAt(i, raws, now)); });
  }
}

// Puts the hub between the page and the game: navigator.getGamepads answers from the hub, and the browser's own connect and
// disconnect events (whose indexes are not the hub's slots) are replaced by the hub's. `toast(text)` shows a message.
function installPads({ window, navigator, virtual, virtualWanted, toast = () => {}, now = () => performance.now() }) {
  const nativeGetGamepads = navigator.getGamepads ? navigator.getGamepads.bind(navigator) : () => [];
  const emitToWindow = (type, pad) => {
    const event = new window.Event(type);
    event.gamepad = pad;
    event.fromHub = true;
    window.dispatchEvent(event);
  };
  const hub = new PadHub({
    getRaw: () => { try { return nativeGetGamepads(); } catch { return []; } },
    virtual,
    virtualWanted,
    onEvent: (e) => {
      const text = toastText(e);
      if (text) toast(text);
      if (e.type === 'disconnected') emitToWindow('gamepaddisconnected', { index: e.slot, id: e.label, connected: false, mapping: 'standard', timestamp: now(), axes: [], buttons: [] });
      else emitToWindow('gamepadconnected', e.pad);
    },
  });
  for (const type of ['gamepadconnected', 'gamepaddisconnected'])
    window.addEventListener(type, (e) => {
      if (e.fromHub) return;
      e.stopImmediatePropagation();
      hub.snapshot(now());   // the real event is a reason to look at the list now
    }, true);
  navigator.getGamepads = () => hub.snapshot(now());
  const timer = window.setInterval(() => hub.announce(now()), 1000);
  return { hub, stop: () => window.clearInterval(timer) };
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { STD, STD_BUTTONS, MAX_PADS, VIRTUAL_ID, normalizeGamepad, padLabel, toastText, stickVector, VirtualPad, PadHub, installPads };
} else Object.assign(globalThis, { WotPads: { STD, STD_BUTTONS, MAX_PADS, VIRTUAL_ID, normalizeGamepad, padLabel, toastText, stickVector, VirtualPad, PadHub, installPads } });
