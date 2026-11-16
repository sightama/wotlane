// ABOUTME: The touch screen's controls for the game: a floating left stick, a look zone, face, shoulder and chord buttons laid out for two
// ABOUTME: thumbs, a size/opacity panel, haptics and fullscreen. It feeds a virtual pad (pads.js), so the game sees a gamepad.
'use strict';

const Pads = typeof module !== 'undefined' && module.exports ? require('./pads.js') : globalThis.WotPads;

// Where the two thumbs go. Positions are in units `u` (about a thumb's width: unitFor) from an anchor on the screen: br, bl, tr, tl are
// corners and tc the middle of the top edge (x is then from the middle). x and y are the control's centre, w and h its size.
// `press` are the standard pad buttons it holds down together; the chords are the game's own combinations (native/wotm_settings.hpp
// kBindMask: BTriCircle, BCrossSquare, BL1R1).
const LAYOUT = [
  { id: 'cross', label: '✕', does: 'Jump (in menus: choose)', press: [Pads.STD.CROSS], anchor: 'br', x: 3.4, y: 1.75, w: 1.6, h: 1.6, cls: 'cross' },
  { id: 'square', label: '□', does: 'Light attack', press: [Pads.STD.SQUARE], anchor: 'br', x: 4.75, y: 3.1, w: 1.6, h: 1.6, cls: 'square' },
  { id: 'triangle', label: '△', does: 'Heavy attack (in menus: back)', press: [Pads.STD.TRIANGLE], anchor: 'br', x: 3.4, y: 4.45, w: 1.6, h: 1.6, cls: 'triangle' },
  { id: 'circle', label: '○', does: 'Grab', press: [Pads.STD.CIRCLE], anchor: 'br', x: 2.0, y: 3.1, w: 1.3, h: 1.3, cls: 'circle' },
  { id: 'special1', label: 'SP1', does: 'Special attack 1 (△ and ○ together)', press: [Pads.STD.TRIANGLE, Pads.STD.CIRCLE], anchor: 'br', x: 0.85, y: 5.0, w: 0.95, h: 0.95, cls: 'chord' },
  { id: 'special2', label: 'SP2', does: 'Special attack 2 (✕ and □ together)', press: [Pads.STD.CROSS, Pads.STD.SQUARE], anchor: 'br', x: 0.85, y: 1.2, w: 0.95, h: 0.95, cls: 'chord' },
  { id: 'l2', label: 'L2', does: 'Left shoulder 2', press: [Pads.STD.L2], anchor: 'tl', x: 1.3, y: 0.75, w: 1.9, h: 1.0, cls: 'shoulder' },
  { id: 'l1', label: 'L1', does: 'Left shoulder 1 (strafe left)', press: [Pads.STD.L1], anchor: 'tl', x: 3.4, y: 0.75, w: 1.9, h: 1.0, cls: 'shoulder' },
  { id: 'r1', label: 'R1', does: 'Right shoulder 1 (strafe right)', press: [Pads.STD.R1], anchor: 'tr', x: 3.4, y: 0.75, w: 1.9, h: 1.0, cls: 'shoulder' },
  { id: 'r2', label: 'R2', does: 'Block (hold)', press: [Pads.STD.R2], anchor: 'tr', x: 1.3, y: 0.75, w: 1.9, h: 1.0, cls: 'shoulder' },
  { id: 'lock', label: 'LOCK', does: 'Lock on to an enemy (L1 and R1 together)', press: [Pads.STD.L1, Pads.STD.R1], anchor: 'tl', x: 2.3, y: 2.3, w: 1.1, h: 1.1, cls: 'chord' },
  { id: 'select', label: 'SEL', does: 'Taunt', press: [Pads.STD.SELECT], anchor: 'tc', x: -1.55, y: 0.5, w: 1.3, h: 0.75, cls: 'small' },
  { id: 'start', label: 'START', does: 'Pause (in menus: start)', press: [Pads.STD.START], anchor: 'tc', x: 1.55, y: 0.5, w: 1.3, h: 0.75, cls: 'small' },
  { id: 'gear', label: '⚙', does: 'Size, opacity and vibration of these controls', press: [], anchor: 'tc', x: 0, y: 0.5, w: 0.8, h: 0.75, cls: 'small' },
];
const ZONE = { split: 0.46, top: 0.18 };   // the left stick takes the screen left of `split` (a fraction of the width), the look zone the right; both start `top` down
const SCALE_RANGE = { min: 0.7, max: 1.2 };
const DEFAULTS = { scale: 1, opacity: 0.6, haptics: true };
const STORAGE_KEY = 'wotlane.touch';
const STICK_RADIUS = 1.5, LOOK_RADIUS = 2.0, STICK_DEAD = 0.1, LOOK_DEAD = 0.08;   // in units, and fractions of the radius
const TAP = { ms: 220, distance: 12 };   // a touch this quick that moved this little (pixels) is a tap

// a touch of `ms` milliseconds that travelled `distance` pixels is a tap, not a held or dragged thumb
const isTap = ({ ms, distance }) => ms <= TAP.ms && distance <= TAP.distance;

// a thumb's width in pixels on a screen `vh` tall
const unitFor = (vh, scale) => Math.min(76, Math.max(38, 0.11 * vh)) * scale;

function rectOf(item, vw, vh, u) {
  const dx = item.x * u, dy = item.y * u;
  const cx = { br: vw - dx, tr: vw - dx, bl: dx, tl: dx, tc: vw / 2 + dx }[item.anchor];
  const cy = item.anchor[0] === 'b' ? vh - dy : dy;
  return { left: cx - item.w * u / 2, right: cx + item.w * u / 2, top: cy - item.h * u / 2, bottom: cy + item.h * u / 2 };
}

// a touch's pointer already follows the element it started on; asking again can be refused for a pointer the browser has finished with
function capture(el, pointerId) {
  try { el.setPointerCapture(pointerId); } catch { /* the touch is already tracked */ }
}

const between = (v, lo, hi, fallback) => (typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : fallback);
function clampSettings(s) {
  const o = s && typeof s === 'object' ? s : {};
  return {
    scale: between(o.scale, SCALE_RANGE.min, SCALE_RANGE.max, DEFAULTS.scale),
    opacity: between(o.opacity, 0.2, 1, DEFAULTS.opacity),
    haptics: o.haptics !== false,
  };
}
function loadSettings(storage) {
  try { return clampSettings(JSON.parse(storage.getItem(STORAGE_KEY))); } catch { return clampSettings(null); }
}
function saveSettings(storage, settings) {
  try { storage.setItem(STORAGE_KEY, JSON.stringify(settings)); } catch { /* storage blocked: the settings last until the page closes */ }
}

// the Controls card's rows for a touch screen: [what it does, where it is]
function touchRows() {
  return [
    ['Move', 'Drag a thumb anywhere on the left half'],
    ['Turn the camera', 'Drag a thumb on the right half, away from the buttons'],
    ...LAYOUT.map((b) => [b.does, b.id === 'gear' ? `${b.label} at the top` : `${b.label} button`]),
  ];
}

const CSS = `
#touchpad { position: absolute; inset: 0; pointer-events: none; z-index: 5; --u: 49px; --tp-opacity: .6;
  -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; touch-action: none; }
#touchpad .tp-zone { position: absolute; pointer-events: auto; touch-action: none; }
#touchpad .tp-ctl { position: absolute; pointer-events: auto; touch-action: none; display: grid; place-items: center; color: #fff;
  font: 700 calc(var(--u) * .5) system-ui, sans-serif; border: 2px solid rgba(255,255,255,.7); background: rgba(30,30,36,.75);
  opacity: var(--tp-opacity); border-radius: 50%; -webkit-tap-highlight-color: transparent; }
#touchpad .tp-ctl.shoulder, #touchpad .tp-ctl.small { border-radius: calc(var(--u) * .3); font-size: calc(var(--u) * .36); }
#touchpad .tp-ctl.chord { font-size: calc(var(--u) * .3); }
#touchpad .tp-ctl.cross { color: #8ab4ff; } #touchpad .tp-ctl.circle { color: #ff8a8a; }
#touchpad .tp-ctl.square { color: #f0a8e0; } #touchpad .tp-ctl.triangle { color: #8af0b4; }
#touchpad .tp-ctl.down { background: rgba(224,89,42,.9); opacity: 1; }
#touchpad .tp-stick { position: absolute; pointer-events: none; border-radius: 50%; border: 2px solid rgba(255,255,255,.6);
  background: rgba(255,255,255,.12); opacity: calc(var(--tp-opacity) * .6); display: grid; place-items: center; }
#touchpad .tp-stick.active { opacity: var(--tp-opacity); }
#touchpad .tp-stick i { display: block; border-radius: 50%; background: rgba(255,255,255,.75); }
#tpPanel { position: absolute; left: 50%; top: calc(var(--u) * 1.3); transform: translateX(-50%); pointer-events: auto; z-index: 6;
  display: grid; gap: 8px; padding: 12px 16px; background: rgba(22,22,26,.96); border: 1px solid #2a2a30; border-radius: 8px; color: #d8d8dc; font-size: 14px; }
#tpPanel label { display: grid; grid-template-columns: 6em 1fr; gap: 8px; align-items: center; }
#tpRotate { position: absolute; inset: 0; display: none; place-content: center; text-align: center; background: rgba(11,11,13,.85); color: #d8d8dc;
  font: 600 18px system-ui, sans-serif; pointer-events: none; padding: 16px; }
@media (orientation: portrait) { #touchpad.on .tp-zone, #touchpad.on .tp-ctl, #touchpad.on .tp-stick { display: none; } #touchpad.on #tpRotate { display: grid; } }
#toast { position: fixed; left: 50%; top: calc(8px + env(safe-area-inset-top, 0px)); transform: translateX(-50%); z-index: 20; max-width: 90vw;
  padding: 8px 14px; border-radius: 8px; background: rgba(22,22,26,.95); border: 1px solid #e0592a; color: #fff; font: 14px system-ui, sans-serif;
  pointer-events: none; opacity: 0; transition: opacity .2s; }
#toast.show { opacity: 1; }
`;

class TouchOverlay {
  // `virtual`: the VirtualPad the controls drive; `stage`: the element the controls go over (the game's canvas fills it)
  constructor({ document, window, navigator, stage, virtual, storage, now = () => performance.now() }) {
    this.document = document;
    this.window = window;
    this.navigator = navigator;
    this.virtual = virtual;
    this.now = now;
    this.storage = storage;
    this.hub = null;
    this.settings = loadSettings(storage);
    this.touchSeen = !!window.matchMedia?.('(pointer: coarse)').matches;   // a phone or tablet; a touch laptop shows them at its first touch
    this.keyboardUsed = false;
    this.fullscreenTried = false;
    this.held = new Map();      // pointerId -> {release()} for each touch holding a control
    this.sticks = {};
    this.build(stage);
    this.listen();
    virtual.vibrationActuator = { playEffect: (type, p) => Promise.resolve(this.vibrate(p?.duration ?? 100, true)).then(() => 'complete') };
    this.apply();
  }

  // the game runs when the front panel is gone
  get gameRunning() {
    const front = this.document.getElementById('front');
    return !front || front.hidden;
  }

  // a real pad takes the touch pad's place (its slot would make the real pad player 2)
  get realPad() {
    return !!this.hub?.slots.some((s) => s && s.kind === 'real');
  }

  get wanted() {
    return this.touchSeen && !this.keyboardUsed && this.gameRunning;
  }

  get visible() {
    return this.wanted && !this.realPad;
  }

  build(stage) {
    const d = this.document;
    const style = d.createElement('style');
    style.textContent = CSS;
    d.head.appendChild(style);
    const root = this.root = d.createElement('div');
    root.id = 'touchpad';
    root.hidden = true;
    root.addEventListener('contextmenu', (e) => e.preventDefault());
    this.zoneLeft = this.zone('tp-zone');
    this.zoneRight = this.zone('tp-zone');
    this.sticks.left = this.stickEl();
    this.sticks.right = this.stickEl();
    this.controls = LAYOUT.map((b) => this.control(b));
    this.panel = this.buildPanel();
    this.rotate = Object.assign(d.createElement('div'), { id: 'tpRotate', textContent: 'Turn your phone sideways to play' });
    root.append(this.zoneLeft, this.zoneRight, this.sticks.left.el, this.sticks.right.el, ...this.controls.map((c) => c.el), this.panel, this.rotate);
    stage.appendChild(root);
    this.toastEl = Object.assign(d.createElement('div'), { id: 'toast' });
    this.toastEl.setAttribute('role', 'status');
    d.body.appendChild(this.toastEl);
    this.bindStick(this.zoneLeft, 'left', STICK_RADIUS, STICK_DEAD);
    this.bindStick(this.zoneRight, 'right', LOOK_RADIUS, LOOK_DEAD);
  }

  zone(cls) {
    return Object.assign(this.document.createElement('div'), { className: cls });
  }

  stickEl() {
    const el = this.document.createElement('div');
    el.className = 'tp-stick';
    const knob = this.document.createElement('i');
    el.appendChild(knob);
    return { el, knob, pointer: null, cx: 0, cy: 0 };
  }

  control(item) {
    const el = this.document.createElement('div');
    el.className = `tp-ctl ${item.cls}`;
    el.textContent = item.label;
    el.dataset.id = item.id;
    el.setAttribute('role', 'button');
    el.setAttribute('aria-label', item.does);
    const control = { item, el };
    if (item.id === 'gear') {
      el.addEventListener('pointerdown', (e) => { e.preventDefault(); this.panel.hidden = !this.panel.hidden; });
      return control;
    }
    const lift = (e) => {
      const held = this.held.get(e.pointerId);
      if (held?.el !== el) return;
      held.release();
    };
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      capture(el, e.pointerId);
      this.held.set(e.pointerId, { el, release: () => this.releaseControl(e.pointerId, item, el) });
      el.classList.add('down');
      for (const i of item.press) this.virtual.press(i, this.now());
      this.vibrate(12);
    });
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) el.addEventListener(type, lift);
    return control;
  }

  releaseControl(pointerId, item, el) {
    if (!this.held.delete(pointerId)) return;
    for (const i of item.press) this.virtual.release(i, this.now());
    if (![...this.held.values()].some((h) => h.el === el)) el.classList.remove('down');
  }

  bindStick(zone, which, radiusUnits, deadZone) {
    const stick = this.sticks[which];
    zone.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      if (stick.pointer !== null) return;
      capture(zone, e.pointerId);
      stick.pointer = e.pointerId;
      stick.cx = e.clientX;
      stick.cy = e.clientY;
      stick.t0 = this.now();
      stick.travel = 0;
      stick.el.classList.add('active');
      this.placeStick(which, e.clientX, e.clientY, 0, 0);
    });
    zone.addEventListener('pointermove', (e) => {
      if (e.pointerId !== stick.pointer) return;
      const r = radiusUnits * this.u;
      stick.travel = Math.max(stick.travel, Math.hypot(e.clientX - stick.cx, e.clientY - stick.cy));
      const [x, y] = Pads.stickVector(e.clientX - stick.cx, e.clientY - stick.cy, r, deadZone);
      this.virtual.setStick(which, x, y);
      const len = Math.hypot(e.clientX - stick.cx, e.clientY - stick.cy) || 1;
      const k = Math.min(1, len / r);
      this.placeStick(which, stick.cx, stick.cy, (e.clientX - stick.cx) / len * k * r, (e.clientY - stick.cy) / len * k * r);
    });
    const end = (e) => {
      if (e.pointerId !== stick.pointer) return;
      stick.pointer = null;
      if (e.type === 'pointerup' && isTap({ ms: this.now() - stick.t0, distance: stick.travel })) this.tap(e.clientX, e.clientY);
      this.virtual.setStick(which, 0, 0);
      stick.el.classList.remove('active');
      this.restStick(which);
    };
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) zone.addEventListener(type, end);
  }

  // A tap on the screen is a mouse click on the game (its menus pick lines by pointer; in a fight a click is the light attack)
  tap(x, y) {
    const canvas = this.document.getElementById('canvas');
    if (!canvas) return;
    const init = { bubbles: true, cancelable: true, clientX: x, clientY: y, button: 0, view: this.window };
    canvas.dispatchEvent(new this.window.MouseEvent('mousemove', init));
    canvas.dispatchEvent(new this.window.MouseEvent('mousedown', { ...init, buttons: 1 }));
    this.window.setTimeout(() => canvas.dispatchEvent(new this.window.MouseEvent('mouseup', init)), 80);
  }

  placeStick(which, cx, cy, ox, oy) {
    const s = this.sticks[which];
    const r = (which === 'left' ? STICK_RADIUS : LOOK_RADIUS) * this.u;
    const rect = this.root.getBoundingClientRect();
    Object.assign(s.el.style, { width: `${2 * r}px`, height: `${2 * r}px`, left: `${cx - rect.left - r}px`, top: `${cy - rect.top - r}px` });
    Object.assign(s.knob.style, { width: `${r * 0.8}px`, height: `${r * 0.8}px`, transform: `translate(${ox}px, ${oy}px)` });
  }

  // a stick not being touched sits faintly where a thumb would rest, as a hint
  restStick(which) {
    const rect = this.root.getBoundingClientRect();
    if (which === 'left') this.placeStick('left', rect.left + 2.6 * this.u, rect.bottom - 2.6 * this.u, 0, 0);
    else this.sticks.right.el.style.width = this.sticks.right.el.style.height = '0px';
  }

  buildPanel() {
    const d = this.document;
    const panel = d.createElement('div');
    panel.id = 'tpPanel';
    panel.hidden = true;
    const range = (label, key, min, max, step) => {
      const input = Object.assign(d.createElement('input'), { type: 'range', min, max, step, value: this.settings[key] });
      input.addEventListener('input', () => { this.settings = clampSettings({ ...this.settings, [key]: Number(input.value) }); this.apply(); saveSettings(this.storage, this.settings); });
      const l = d.createElement('label');
      l.append(label, input);
      return l;
    };
    const haptics = Object.assign(d.createElement('input'), { type: 'checkbox', checked: this.settings.haptics });
    haptics.addEventListener('change', () => { this.settings = clampSettings({ ...this.settings, haptics: haptics.checked }); saveSettings(this.storage, this.settings); });
    const hl = d.createElement('label');
    hl.append('Vibration', haptics);
    const fullscreen = Object.assign(d.createElement('button'), { textContent: 'Fullscreen' });
    fullscreen.addEventListener('click', () => this.enterFullscreen());
    const close = Object.assign(d.createElement('button'), { textContent: 'Close' });
    close.addEventListener('click', () => { panel.hidden = true; });
    panel.append(range('Size', 'scale', SCALE_RANGE.min, SCALE_RANGE.max, 0.05), range('Opacity', 'opacity', 0.2, 1, 0.05), hl, fullscreen, close);
    return panel;
  }

  // the controls follow the settings and the screen
  apply() {
    this.u = unitFor(this.window.innerHeight, this.settings.scale);
    const vw = this.window.innerWidth, vh = this.window.innerHeight;
    this.root.style.setProperty('--u', `${this.u}px`);
    this.root.style.setProperty('--tp-opacity', String(this.settings.opacity));
    for (const { item, el } of this.controls) {
      const r = rectOf(item, vw, vh, this.u);
      Object.assign(el.style, { left: `${r.left}px`, top: `${r.top}px`, width: `${r.right - r.left}px`, height: `${r.bottom - r.top}px` });
    }
    const top = ZONE.top * vh;
    Object.assign(this.zoneLeft.style, { left: '0px', top: `${top}px`, width: `${ZONE.split * vw}px`, height: `${vh - top}px` });
    Object.assign(this.zoneRight.style, { left: `${ZONE.split * vw}px`, top: `${top}px`, width: `${(1 - ZONE.split) * vw}px`, height: `${vh - top}px` });
    if (!this.sticks.left.pointer) this.restStick('left');
    if (!this.sticks.right.pointer) this.restStick('right');
  }

  // shows or hides the controls as the situation changes; lifts every held control when they go
  update() {
    const show = this.visible;
    if (show === !this.root.hidden) return;
    this.root.hidden = !show;
    this.root.classList.toggle('on', show);
    if (show) {
      this.apply();
      this.document.exitPointerLock?.();   // a locked pointer would zero every touch's coordinates
    } else this.releaseAll();
  }

  releaseAll() {
    for (const h of [...this.held.values()]) h.release();
    for (const which of ['left', 'right']) {
      this.sticks[which].pointer = null;
      this.virtual.setStick(which, 0, 0);
    }
  }

  listen() {
    const w = this.window, d = this.document;
    d.addEventListener('keydown', (e) => { if (e.isTrusted && this.touchSeen) { this.keyboardUsed = true; this.update(); } }, true);
    d.addEventListener('pointerdown', (e) => {
      if (e.pointerType === 'touch') { this.touchSeen = true; this.keyboardUsed = false; }
      else if (e.pointerType === 'mouse' && e.isTrusted) this.touchSeen = false;
      this.update();
    }, true);
    d.addEventListener('click', (e) => {
      if (this.fullscreenTried || !this.touchSeen || !e.target.closest?.('button')) return;
      this.fullscreenTried = true;
      this.enterFullscreen();
    }, true);
    w.addEventListener('resize', () => this.apply());
    w.addEventListener('orientationchange', () => this.apply());
    w.setInterval(() => { this.update(); this.keepAwake(); }, 250);
  }

  // fullscreen and landscape where the browser allows them (both need a tap; neither is available everywhere)
  async enterFullscreen() {
    try {
      if (!this.document.fullscreenElement) await this.document.documentElement.requestFullscreen({ navigationUI: 'hide' });
      await this.window.screen?.orientation?.lock?.('landscape');
    } catch { /* refused: the game plays in the browser's window */ }
  }

  // the screen stays on while the controls are up
  async keepAwake() {
    if (!this.visible || this.wakeLock || !this.navigator.wakeLock) return;
    try {
      this.wakeLock = await this.navigator.wakeLock.request('screen');
      this.wakeLock.addEventListener('release', () => { this.wakeLock = null; });
    } catch { this.wakeLock = null; }
  }

  vibrate(ms, fromGame = false) {
    if (!this.navigator.vibrate || (!this.settings.haptics && !fromGame) || !this.visible) return false;
    return this.navigator.vibrate(ms);
  }

  toast(text) {
    this.toastEl.textContent = text;
    this.toastEl.classList.add('show');
    this.window.clearTimeout(this.toastTimer);
    this.toastTimer = this.window.setTimeout(() => this.toastEl.classList.remove('show'), 3500);
  }
}

function start() {
  const stage = document.getElementById('stage');
  if (!stage) return;
  const virtual = new Pads.VirtualPad();
  let storage = null;
  try { storage = window.localStorage; } catch { /* blocked */ }
  const overlay = new TouchOverlay({ document, window, navigator, stage, virtual, storage });
  const pads = Pads.installPads({ window, navigator, virtual, virtualWanted: () => overlay.wanted, toast: (t) => overlay.toast(t) });
  overlay.hub = pads.hub;
  // the game's mouse camera asks for the pointer on a screen the player is touching; there is no mouse to turn the camera with
  const canvas = document.getElementById('canvas');
  const requestLock = canvas?.requestPointerLock;
  if (requestLock) canvas.requestPointerLock = function (...args) { return overlay.visible ? undefined : requestLock.apply(this, args); };
  window.wotTouch = { overlay, hub: pads.hub, virtual };
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { LAYOUT, ZONE, SCALE_RANGE, unitFor, rectOf, clampSettings, loadSettings, saveSettings, touchRows, isTap, TouchOverlay };
} else {
  Object.assign(globalThis, { wotTouchRows: touchRows });
  start();
}
