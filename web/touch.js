// ABOUTME: The touch screen's controls for the game: a floating left stick, a look zone, face, shoulder and chord buttons laid out for two
// ABOUTME: thumbs, a size/opacity panel, haptics and fullscreen. It feeds a virtual pad (pads.js), so the game sees a gamepad.
'use strict';

const Pads = typeof module !== 'undefined' && module.exports ? require('./pads.js') : globalThis.WotPads;

// Where the two thumbs go. Positions are in units `u` (about a thumb's width: unitFor) from an anchor on the screen: br, bl, tr, tl are
// corners and tc the middle of the top edge (x is then from the middle). x and y are the control's centre, w and h its size.
// `press` are the standard pad buttons it holds down together; the chords are the game's own combinations (native/wotm_settings.hpp
// kBindMask: BTriCircle, BCrossSquare, BL1R1).
const LAYOUT = [
  { id: 'cross', label: '✕', does: 'Jump (in menus: choose)', press: [Pads.STD.CROSS], anchor: 'br', x: 3.3, y: 1.7, w: 1.35, h: 1.35, cls: 'cross', icon: 'cross' },
  { id: 'square', label: '□', does: 'Light attack', press: [Pads.STD.SQUARE], anchor: 'br', x: 4.55, y: 2.95, w: 1.35, h: 1.35, cls: 'square', icon: 'square' },
  { id: 'triangle', label: '△', does: 'Heavy attack (in menus: back)', press: [Pads.STD.TRIANGLE], anchor: 'br', x: 3.3, y: 4.2, w: 1.35, h: 1.35, cls: 'triangle', icon: 'triangle' },
  { id: 'circle', label: '○', does: 'Grab', press: [Pads.STD.CIRCLE], anchor: 'br', x: 2.05, y: 2.95, w: 1.15, h: 1.15, cls: 'circle', icon: 'circle' },
  { id: 'special1', label: 'SP1', does: 'Special attack 1 (△ and ○ together)', press: [Pads.STD.TRIANGLE, Pads.STD.CIRCLE], anchor: 'br', x: 0.85, y: 4.7, w: 0.95, h: 0.95, cls: 'chord' },
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
// The face buttons' symbols, drawn the same way (one line weight, one box) so they match whatever fonts the phone has
const ICONS = {
  cross: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="M5 5L19 19M19 5L5 19"/></svg>',
  circle: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><circle cx="12" cy="12" r="7.5"/></svg>',
  square: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linejoin="round"><rect x="5" y="5" width="14" height="14"/></svg>',
  triangle: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linejoin="round"><path d="M12 4.5L20 18.5H4Z"/></svg>',
};
const ZONE = { split: 0.46, top: 0.18 };   // the left stick takes the screen left of `split` (a fraction of the width), the look zone the right; both start `top` down
const SCALE_RANGE = { min: 0.7, max: 1.2 };
const DEFAULTS = { scale: 1, opacity: 0.6, haptics: true, mirror: false, place: {} };
const PLACE_LIMIT = 40;   // how far from its anchor a placed control may be, in units
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

// Left handed puts every control on the other side of the screen (the middle ones swap places); a placed control keeps its own x and y
const MIRROR = { br: 'bl', bl: 'br', tr: 'tl', tl: 'tr', tc: 'tc' };
function layoutFor(settings) {
  return LAYOUT.map((b) => {
    const side = settings.mirror ? { ...b, anchor: MIRROR[b.anchor], x: b.anchor === 'tc' ? -b.x : b.x } : b;
    const p = settings.place[b.id];
    return p ? { ...side, x: p.x, y: p.y } : side;
  });
}

// The x and y (units from the anchor, to 0.05) that put the centre of `item` at the point (cx, cy), kept whole on screen
function placeFromPoint(item, vw, vh, u, cx, cy) {
  const x = Math.min(vw - item.w * u / 2, Math.max(item.w * u / 2, cx)), y = Math.min(vh - item.h * u / 2, Math.max(item.h * u / 2, cy));
  const dx = { br: vw - x, tr: vw - x, bl: x, tl: x, tc: x - vw / 2 }[item.anchor];
  const dy = item.anchor[0] === 'b' ? vh - y : y;
  return { x: Math.round(dx / u / 0.05) * 0.05, y: Math.round(dy / u / 0.05) * 0.05 };
}

// `item` (placed or not) moved the least that keeps it whole on screen: a place made on a bigger screen or at a smaller size may not fit now
function fitOnScreen(item, vw, vh, u) {
  const r = rectOf(item, vw, vh, u);
  if (r.left >= 0 && r.top >= 0 && r.right <= vw && r.bottom <= vh) return item;
  return { ...item, ...placeFromPoint(item, vw, vh, u, (r.left + r.right) / 2, (r.top + r.bottom) / 2) };
}

// where the movement stick and the camera look zone are: the left and the right of the screen, swapped for a left handed player
function zonesFor(mirror, vw, vh) {
  const top = ZONE.top * vh, h = vh - top, split = ZONE.split * vw;
  const first = { left: 0, top, width: split, height: h }, second = { left: split, top, width: vw - split, height: h };
  return mirror ? { stick: { ...first, left: vw - split }, look: { ...second, left: 0, width: vw - split } } : { stick: first, look: second };
}

// a touch's pointer already follows the element it started on; asking again can be refused for a pointer the browser has finished with
function capture(el, pointerId) {
  try { el.setPointerCapture(pointerId); } catch { /* the touch is already tracked */ }
}

const between = (v, lo, hi, fallback) => (typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : fallback);
// the placed controls that can be placed (the gear stays where it is, so the panel can always be opened) with numbers in range
function cleanPlace(place) {
  const out = {};
  for (const b of LAYOUT) {
    const p = place && typeof place === 'object' ? place[b.id] : null;
    if (b.id === 'gear' || !p || !Number.isFinite(p.x) || !Number.isFinite(p.y) || Math.abs(p.x) > PLACE_LIMIT || Math.abs(p.y) > PLACE_LIMIT) continue;
    out[b.id] = { x: p.x, y: p.y };
  }
  return out;
}
function clampSettings(s) {
  const o = s && typeof s === 'object' ? s : {};
  return {
    scale: between(o.scale, SCALE_RANGE.min, SCALE_RANGE.max, DEFAULTS.scale),
    opacity: between(o.opacity, 0.2, 1, DEFAULTS.opacity),
    haptics: o.haptics !== false,
    mirror: o.mirror === true,
    place: cleanPlace(o.place),
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
#touchpad .tp-ctl svg { width: 46%; height: 46%; display: block; }
#touchpad .tp-ctl.cross { color: #8ab4ff; } #touchpad .tp-ctl.circle { color: #ff8a8a; }
#touchpad .tp-ctl.square { color: #f0a8e0; } #touchpad .tp-ctl.triangle { color: #8af0b4; }
#touchpad.editing .tp-ctl:not([data-id=gear]) { outline: 2px dashed #ffd34d; opacity: 1; }
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
    this.editing = false;
    this.drag = null;
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
    if (item.icon) el.innerHTML = ICONS[item.icon];   // fixed markup above, no page text
    else el.textContent = item.label;
    el.dataset.id = item.id;
    el.setAttribute('role', 'button');
    el.setAttribute('aria-label', item.does);
    const control = { item, el };
    if (item.id === 'gear') {
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        this.panel.hidden = !this.panel.hidden;
        if (this.panel.hidden) this.setEditing(false);
      });
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
      if (this.editing) { this.drag = { id: item.id, pointerId: e.pointerId }; return; }
      this.held.set(e.pointerId, { el, release: () => this.releaseControl(e.pointerId, item, el) });
      el.classList.add('down');
      for (const i of item.press) this.virtual.press(i, this.now());
      this.vibrate(12);
    });
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) el.addEventListener(type, lift);
    el.addEventListener('pointermove', (e) => {
      if (this.drag?.id !== item.id || this.drag.pointerId !== e.pointerId) return;
      const place = placeFromPoint(control.item, this.window.innerWidth, this.window.innerHeight, this.u, e.clientX, e.clientY);
      this.settings = clampSettings({ ...this.settings, place: { ...this.settings.place, [item.id]: place } });
      this.apply();
    });
    const drop = (e) => {
      if (this.drag?.pointerId !== e.pointerId) return;
      this.drag = null;
      saveSettings(this.storage, this.settings);
    };
    for (const type of ['pointerup', 'pointercancel']) el.addEventListener(type, drop);
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
    const rect = this.rect;
    Object.assign(s.el.style, { width: `${2 * r}px`, height: `${2 * r}px`, left: `${cx - rect.left - r}px`, top: `${cy - rect.top - r}px` });
    Object.assign(s.knob.style, { width: `${r * 0.8}px`, height: `${r * 0.8}px`, transform: `translate(${ox}px, ${oy}px)` });
  }

  // a stick not being touched sits faintly where a thumb would rest, as a hint
  restStick(which) {
    const rect = this.rect;
    if (which === 'left') this.placeStick('left', this.settings.mirror ? rect.right - 2.6 * this.u : rect.left + 2.6 * this.u, rect.bottom - 2.6 * this.u, 0, 0);
    else {   // the look zone has no hint: nothing is drawn until it is touched
      const s = this.sticks.right;
      s.el.style.width = s.el.style.height = s.knob.style.width = s.knob.style.height = '0px';
    }
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
    const mirror = Object.assign(d.createElement('input'), { type: 'checkbox', checked: this.settings.mirror });
    mirror.addEventListener('change', () => {   // the places belong to one side: they go when the side changes
      this.settings = clampSettings({ ...this.settings, mirror: mirror.checked, place: {} });
      this.apply();
      saveSettings(this.storage, this.settings);
    });
    const ml = d.createElement('label');
    ml.append('Left handed', mirror);
    const move = Object.assign(d.createElement('button'), { textContent: 'Move buttons' });
    this.moveButton = move;
    move.addEventListener('click', () => this.setEditing(!this.editing));
    const reset = Object.assign(d.createElement('button'), { textContent: 'Reset positions' });
    reset.addEventListener('click', () => {
      this.settings = clampSettings({ ...this.settings, place: {} });
      this.apply();
      saveSettings(this.storage, this.settings);
    });
    const fullscreen = Object.assign(d.createElement('button'), { textContent: 'Fullscreen' });
    fullscreen.addEventListener('click', () => this.enterFullscreen());
    const close = Object.assign(d.createElement('button'), { textContent: 'Close' });
    close.addEventListener('click', () => { panel.hidden = true; this.setEditing(false); });
    panel.append(range('Size', 'scale', SCALE_RANGE.min, SCALE_RANGE.max, 0.05), range('Opacity', 'opacity', 0.2, 1, 0.05), hl, ml, move, reset, fullscreen, close);
    return panel;
  }

  // while editing, a touch on a control drags it instead of pressing it
  setEditing(on) {
    if (this.drag) saveSettings(this.storage, this.settings);   // a drag cut short keeps where it got to
    this.editing = on;
    this.drag = null;
    this.releaseAll();
    this.root.classList.toggle('editing', on);
    this.moveButton.textContent = on ? 'Done moving' : 'Move buttons';
  }

  // the controls follow the settings and the screen
  apply() {
    this.rect = this.root.getBoundingClientRect();   // read once here; the sticks are placed on every touch move
    // in portrait the controls are hidden by CSS, which may cancel no touch: nothing stays held
    if (this.window.matchMedia?.('(orientation: portrait)').matches) this.releaseAll();
    this.u = unitFor(this.window.innerHeight, this.settings.scale);
    const vw = this.window.innerWidth, vh = this.window.innerHeight;
    this.root.style.setProperty('--u', `${this.u}px`);
    this.root.style.setProperty('--tp-opacity', String(this.settings.opacity));
    const items = layoutFor(this.settings);
    for (const control of this.controls) {
      const { el } = control;
      const found = items.find((b) => b.id === control.item.id);
      const item = control.item = this.settings.place[found.id] ? fitOnScreen(found, vw, vh, this.u) : found;
      const r = rectOf(item, vw, vh, this.u);
      Object.assign(el.style, { left: `${r.left}px`, top: `${r.top}px`, width: `${r.right - r.left}px`, height: `${r.bottom - r.top}px` });
    }
    const zones = zonesFor(this.settings.mirror, vw, vh);
    for (const [el, z] of [[this.zoneLeft, zones.stick], [this.zoneRight, zones.look]])
      Object.assign(el.style, { left: `${z.left}px`, top: `${z.top}px`, width: `${z.width}px`, height: `${z.height}px` });
    if (this.sticks.left.pointer === null) this.restStick('left');
    if (this.sticks.right.pointer === null) this.restStick('right');
  }

  // shows or hides the controls as the situation changes; lifts every held control when they go
  update() {
    const show = this.visible;
    if (show === !this.root.hidden) return;
    this.root.hidden = !show;
    this.root.classList.toggle('on', show);
    const bar = this.document.getElementById('bar');   // the page's status bar would sit under the buttons
    if (bar) bar.hidden = show;
    if (show) {
      this.apply();
      this.document.exitPointerLock?.();   // a locked pointer would zero every touch's coordinates
    } else {
      this.setEditing(false);
      this.releaseAll();
    }
  }

  releaseAll() {
    for (const h of [...this.held.values()]) h.release();
    for (const which of ['left', 'right']) {
      this.sticks[which].pointer = null;
      this.virtual.setStick(which, 0, 0);
      this.sticks[which].el.classList.remove('active');
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
    if (!this.visible || this.wakeLock || this.wakeAsked || !this.navigator.wakeLock || this.now() < (this.wakeRetryAt ?? 0)) return;
    this.wakeAsked = true;
    try {
      const lock = await this.navigator.wakeLock.request('screen');
      this.wakeLock = lock;
      lock.addEventListener('release', () => { if (this.wakeLock === lock) this.wakeLock = null; });
    } catch { this.wakeRetryAt = this.now() + 10000; }   // refused (hidden tab, no support): not again for a while
    this.wakeAsked = false;
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
  // a button going down on any pad (the touch pad too) is the input latency probe's press time (wotm_web_input_at, wotm_webfs.cpp)
  const onPress = (t) => window.Module?._wotm_web_input_at?.(performance.timeOrigin + t);
  const pads = Pads.installPads({ window, navigator, virtual, virtualWanted: () => overlay.wanted, toast: (t) => overlay.toast(t), onPress });
  overlay.hub = pads.hub;
  // the game's mouse camera asks for the pointer on a screen the player is touching; there is no mouse to turn the camera with
  const canvas = document.getElementById('canvas');
  const requestLock = canvas?.requestPointerLock;
  overlay.lockRefusals = 0;
  if (requestLock) canvas.requestPointerLock = function (...args) {
    if (!overlay.visible) return requestLock.apply(this, args);
    overlay.lockRefusals++;
    return undefined;
  };
  window.wotTouch = { overlay, hub: pads.hub, virtual };
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { fitOnScreen, layoutFor, placeFromPoint, zonesFor, LAYOUT, ZONE, SCALE_RANGE, unitFor, rectOf, clampSettings, loadSettings, saveSettings, touchRows, isTap, ICONS, TouchOverlay };
} else {
  Object.assign(globalThis, { wotTouchRows: touchRows });
  start();
}
