// ABOUTME: Tests for web/pads.js: standard and non-standard gamepads as one standard layout, the slots the game sees (hot-plug, the
// ABOUTME: touch pad that steps aside for a real pad), the virtual pad's held presses and stick math, controller names. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { STD, normalizeGamepad, padLabel, toastText, stickVector, VirtualPad, PadHub } = createRequire(import.meta.url)('./pads.js');

const button = (pressed, value = pressed ? 1 : 0) => ({ pressed, value, touched: pressed });
const buttons = (n, down = {}) => Array.from({ length: n }, (_, i) => button(!!down[i], down[i] ?? 0));
const standardPad = (over = {}) => ({
  index: 0, id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)', connected: true, mapping: 'standard', timestamp: 1,
  axes: [0, 0, 0, 0], buttons: buttons(17), ...over,
});

test('a standard pad comes out as it went in: four axes, seventeen buttons, same values', () => {
  const pad = normalizeGamepad(standardPad({ axes: [0.5, -1, 0.25, 0], buttons: buttons(17, { [STD.CROSS]: 1, [STD.R2]: 0.5 }) }));
  assert.equal(pad.mapping, 'standard');
  assert.deepEqual(pad.axes, [0.5, -1, 0.25, 0]);
  assert.equal(pad.buttons.length, 17);
  assert.equal(pad.buttons[STD.CROSS].pressed, true);
  assert.equal(pad.buttons[STD.R2].value, 0.5);
  assert.equal(pad.buttons[STD.CIRCLE].pressed, false);
});

test('a standard pad with fewer buttons (no home button) is padded, never read past its end', () => {
  const pad = normalizeGamepad(standardPad({ buttons: buttons(16) }));
  assert.equal(pad.buttons.length, 17);
  assert.equal(pad.buttons[STD.HOME].pressed, false);
});

test('a non-standard generic pad (hat on axis 9, buttons in the usual order) becomes a standard one', () => {
  const raw = {
    index: 1, id: 'USB Gamepad', connected: true, mapping: '', timestamp: 1,
    axes: [0.5, -0.5, 0.1, 0.2, 0, 0, 0, 0, 0, -1],
    buttons: buttons(12, { 0: 1, 4: 1, 7: 1, 9: 1 }),
  };
  const pad = normalizeGamepad(raw);
  assert.equal(pad.mapping, 'standard');
  assert.deepEqual(pad.axes, [0.5, -0.5, 0.1, 0.2]);
  assert.equal(pad.buttons[STD.CROSS].pressed, true);   // button 0 is the bottom face button
  assert.equal(pad.buttons[STD.L1].pressed, true);
  assert.equal(pad.buttons[STD.R2].pressed, true);
  assert.equal(pad.buttons[STD.START].pressed, true);
  assert.equal(pad.buttons[STD.UP].pressed, true);      // hat value -1: up
  assert.equal(pad.buttons[STD.DOWN].pressed, false);
});

test('a hat axis reads all eight directions and rest (the neutral value is past +1)', () => {
  const at = (hat) => normalizeGamepad({ index: 0, id: 'USB Gamepad', mapping: '', connected: true, axes: [0, 0, 0, 0, 0, 0, 0, 0, 0, hat], buttons: buttons(12) });
  const dirs = (p) => ['UP', 'DOWN', 'LEFT', 'RIGHT'].filter((d) => p.buttons[STD[d]].pressed).join('+');
  assert.equal(dirs(at(-1)), 'UP');
  assert.equal(dirs(at(-0.428571)), 'RIGHT');
  assert.equal(dirs(at(0.142857)), 'DOWN');
  assert.equal(dirs(at(0.714286)), 'LEFT');
  assert.equal(dirs(at(1)), 'UP+LEFT');
  assert.equal(dirs(at(-0.714286)), 'UP+RIGHT');
  assert.equal(dirs(at(-0.142857)), 'DOWN+RIGHT');
  assert.equal(dirs(at(0.428571)), 'DOWN+LEFT');
  assert.equal(dirs(at(1.285714)), '');
});

test('a non-standard Xbox-style pad (triggers and hat on axes) becomes a standard one', () => {
  const raw = {
    index: 0, id: 'Microsoft X-Box 360 pad', connected: true, mapping: '', timestamp: 1,
    // lx, ly, left trigger (rests at -1), rx, ry, right trigger (rests at -1), hat x, hat y
    axes: [0.1, 0.2, 1, 0.3, 0.4, -1, -1, 1],
    buttons: buttons(11, { 0: 1, 6: 1, 8: 1 }),
  };
  const pad = normalizeGamepad(raw);
  assert.deepEqual(pad.axes, [0.1, 0.2, 0.3, 0.4]);
  assert.equal(pad.buttons[STD.L2].pressed, true);
  assert.equal(pad.buttons[STD.L2].value, 1);
  assert.equal(pad.buttons[STD.R2].pressed, false);
  assert.equal(pad.buttons[STD.R2].value, 0);
  assert.equal(pad.buttons[STD.LEFT].pressed, true);
  assert.equal(pad.buttons[STD.DOWN].pressed, true);
  assert.equal(pad.buttons[STD.CROSS].pressed, true);
  assert.equal(pad.buttons[STD.SELECT].pressed, true);   // Back
  assert.equal(pad.buttons[STD.HOME].pressed, true);
});

test('a pad that reports nothing usable still comes out as a quiet standard pad', () => {
  const pad = normalizeGamepad({ index: 0, id: 'Strange', mapping: '', connected: true, axes: [], buttons: [] });
  assert.equal(pad.buttons.length, 17);
  assert.deepEqual(pad.axes, [0, 0, 0, 0]);
  assert.ok(pad.buttons.every((b) => !b.pressed));
});

test('controller names lose their vendor and product noise', () => {
  assert.equal(padLabel('Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)'), 'Xbox Wireless Controller');
  assert.equal(padLabel('Xbox 360 Controller (XInput STANDARD GAMEPAD)'), 'Xbox 360 Controller');
  assert.equal(padLabel('054c-09cc-Wireless Controller'), 'Wireless Controller');
  assert.equal(padLabel('USB Gamepad '), 'USB Gamepad');
  assert.equal(padLabel(''), 'Controller');
});

test('the virtual stick: a dead zone, then a smooth ramp to the rim, never past 1', () => {
  assert.deepEqual(stickVector(3, 0, 50, 0.1), [0, 0]);
  const [x] = stickVector(25, 0, 50, 0.1);
  assert.ok(x > 0.4 && x < 0.5, `half way is just under half: ${x}`);   // (0.5 - 0.1) / 0.9
  assert.deepEqual(stickVector(50, 0, 50, 0.1), [1, 0]);
  const [bx, by] = stickVector(300, 400, 50, 0.1);
  assert.ok(Math.abs(Math.hypot(bx, by) - 1) < 1e-9, 'far past the rim is clamped to a full push');
  assert.ok(bx > 0 && by > 0 && by > bx, 'direction is kept');
});

test('the virtual pad: a tap shorter than a frame is still held long enough to be read', () => {
  const v = new VirtualPad({ minHoldMs: 80 });
  v.press(STD.CROSS, 0);
  v.release(STD.CROSS, 10);
  assert.equal(v.snapshot(20).buttons[STD.CROSS].pressed, true);
  assert.equal(v.snapshot(79).buttons[STD.CROSS].pressed, true);
  assert.equal(v.snapshot(81).buttons[STD.CROSS].pressed, false);
});

test('the virtual pad: two touches on one button (a chord overlapping a face button) keep it down until both lift', () => {
  const v = new VirtualPad({ minHoldMs: 0 });
  v.press(STD.CIRCLE, 0);
  v.press(STD.CIRCLE, 1);
  v.release(STD.CIRCLE, 2);
  assert.equal(v.snapshot(100).buttons[STD.CIRCLE].pressed, true);
  v.release(STD.CIRCLE, 3);
  assert.equal(v.snapshot(100).buttons[STD.CIRCLE].pressed, false);
});

test('the virtual pad is a standard pad with its own name and the sticks the overlay set', () => {
  const v = new VirtualPad();
  v.setStick('left', 0.5, -0.5);
  v.setStick('right', 1, 0);
  const pad = v.snapshot(0);
  assert.equal(pad.mapping, 'standard');
  assert.equal(pad.connected, true);
  assert.equal(pad.id, 'Touch screen controls');
  assert.deepEqual(pad.axes, [0.5, -0.5, 1, 0]);
  assert.equal(pad.buttons.length, 17);
});

// a hub over a list of real pads the test edits
function hubWith({ virtualWanted = false } = {}) {
  const raws = [];
  const events = [];
  const virtual = new VirtualPad({ minHoldMs: 0 });
  const state = { virtualWanted };
  const hub = new PadHub({ getRaw: () => raws, onEvent: (e) => events.push(e), virtual, virtualWanted: () => state.virtualWanted });
  return { hub, raws, events, virtual, state };
}

test('a pad that appears takes the first slot and is announced once; the others see nothing there', () => {
  const { hub, raws, events } = hubWith();
  assert.deepEqual(hub.snapshot(0), [null, null, null, null]);
  raws[0] = standardPad({ index: 0 });
  const snap = hub.snapshot(1);
  assert.equal(snap[0].index, 0);
  assert.equal(snap[1], null);
  hub.snapshot(2);
  assert.deepEqual(events.filter((e) => e.type === 'connected').map((e) => [e.slot, e.kind, e.player]), [[0, 'real', 1]]);
});

test('a second pad takes the second slot (player 2); unplugging the first frees its slot and tells who left', () => {
  const { hub, raws, events } = hubWith();
  raws[0] = standardPad({ index: 0, id: 'Pad A' });
  raws[1] = standardPad({ index: 1, id: 'Pad B' });
  hub.snapshot(0);
  assert.deepEqual(events.filter((e) => e.type === 'connected').map((e) => [e.slot, e.player]), [[0, 1], [1, 2]]);
  raws[0] = null;
  const snap = hub.snapshot(1);
  assert.equal(snap[0], null);
  assert.equal(snap[1].id, 'Pad B');
  assert.equal(snap[1].index, 1, 'a pad keeps its slot, the game closes the gap itself');
  const gone = events.find((e) => e.type === 'disconnected');
  assert.equal(gone.slot, 0);
  assert.equal(gone.label, 'Pad A');
});

test('a pad plugged into the same browser index again after a gap is a new connection', () => {
  const { hub, raws, events } = hubWith();
  raws[0] = standardPad({ index: 0, id: 'Pad A' });
  hub.snapshot(0);
  raws[0] = null;
  hub.snapshot(1);
  raws[0] = standardPad({ index: 0, id: 'Pad A' });
  hub.snapshot(2);
  assert.deepEqual(events.map((e) => e.type), ['connected', 'disconnected', 'connected']);
});

test('a fifth pad is ignored: the game takes four', () => {
  const { hub, raws } = hubWith();
  for (let i = 0; i < 5; i++) raws[i] = standardPad({ index: i, id: `Pad ${i}` });
  const snap = hub.snapshot(0);
  assert.equal(snap.length, 4);
  assert.ok(snap.every((p) => p));
});

test('the touch pad takes slot 0 when wanted and no real pad is there, and says nothing worth a toast', () => {
  const { hub, events, state } = hubWith({ virtualWanted: true });
  const snap = hub.snapshot(0);
  assert.equal(snap[0].id, 'Touch screen controls');
  assert.equal(events[0].kind, 'virtual');
  assert.equal(toastText(events[0]), null);
  state.virtualWanted = false;
  assert.equal(hub.snapshot(1)[0], null);
  assert.equal(events[1].type, 'disconnected');
});

test('a real pad pushes the touch pad out of slot 0 (it would otherwise be player 1), and the touch pad returns when the pad leaves', () => {
  const { hub, raws, events } = hubWith({ virtualWanted: true });
  hub.snapshot(0);
  raws[0] = standardPad({ index: 0, id: 'Pad A' });
  const snap = hub.snapshot(1);
  assert.equal(snap[0].id, 'Pad A');
  assert.equal(snap[1], null);
  assert.deepEqual(events.map((e) => `${e.type}:${e.kind}:${e.slot}`), ['connected:virtual:0', 'disconnected:virtual:0', 'connected:real:0']);
  raws[0] = null;
  assert.equal(hub.snapshot(2)[0].id, 'Touch screen controls');
});

test('announce repeats every pad in its slot for a game that started listening late', () => {
  const { hub, raws, events } = hubWith();
  raws[0] = standardPad({ index: 0 });
  hub.snapshot(0);
  events.length = 0;
  hub.announce(1);
  assert.deepEqual(events.map((e) => [e.type, e.slot]), [['announce', 0]]);
  assert.equal(events[0].pad.index, 0);
});

test('toasts name the controller and the player; an announce or a virtual pad has none', () => {
  const { hub, raws, events } = hubWith();
  raws[0] = standardPad({ index: 0, id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)' });
  hub.snapshot(0);
  assert.equal(toastText(events[0]), 'Controller connected: Xbox Wireless Controller (player 1)');
  raws[0] = null;
  hub.snapshot(1);
  assert.equal(toastText(events[1]), 'Controller disconnected: Xbox Wireless Controller');
  assert.equal(toastText({ type: 'announce', kind: 'real' }), null);
});

test('real pad values reach the game fresh on every snapshot', () => {
  const { hub, raws } = hubWith();
  raws[0] = standardPad({ index: 0, axes: [0, 0, 0, 0] });
  assert.equal(hub.snapshot(0)[0].axes[0], 0);
  raws[0] = standardPad({ index: 0, axes: [1, 0, 0, 0], buttons: buttons(17, { [STD.CROSS]: 1 }) });
  const pad = hub.snapshot(1)[0];
  assert.equal(pad.axes[0], 1);
  assert.equal(pad.buttons[STD.CROSS].pressed, true);
});

// the browser-facing part, over a bare EventTarget for the window and a navigator the test controls
function installed({ virtualWanted = false } = {}) {
  const window = Object.assign(new EventTarget(), { Event, setInterval: () => 0, clearInterval: () => {} });
  const raws = [];
  const navigator = { getGamepads: () => raws };
  const toasts = [];
  const virtual = new VirtualPad({ minHoldMs: 0 });
  const handle = installPads({ window, navigator, virtual, virtualWanted: () => virtualWanted, toast: (t) => toasts.push(t), now: () => 0 });
  const heard = [];   // the game's listeners come after the page's, as in the browser
  for (const type of ['gamepadconnected', 'gamepaddisconnected']) window.addEventListener(type, (e) => heard.push([type, e.gamepad.index, e.gamepad.id]));
  return { window, raws, navigator, toasts, heard, virtual, handle };
}
const { installPads } = createRequire(import.meta.url)('./pads.js');

test('after install navigator.getGamepads answers with the game\'s four slots', () => {
  const { navigator, raws } = installed();
  assert.deepEqual(navigator.getGamepads(), [null, null, null, null]);
  raws[2] = standardPad({ index: 2, id: 'Pad C' });
  const snap = navigator.getGamepads();
  assert.equal(snap[0].id, 'Pad C');
  assert.equal(snap[0].index, 0, 'the game sees it as pad 0, whatever index the browser gave');
});

test('the game hears the hub\'s connect events (slot index), not the browser\'s own', () => {
  const { navigator, raws, window, heard, toasts } = installed();
  raws[3] = standardPad({ index: 3, id: 'Pad D' });
  const realEvent = new Event('gamepadconnected');
  realEvent.gamepad = raws[3];
  window.dispatchEvent(realEvent);   // the browser's event: swallowed, but it makes the hub look at the list
  assert.deepEqual(heard, [['gamepadconnected', 0, 'Pad D']]);
  assert.deepEqual(toasts, ['Controller connected: Pad D (player 1)']);
  raws[3] = null;
  navigator.getGamepads();
  assert.deepEqual(heard.at(-1), ['gamepaddisconnected', 0, 'Pad D']);
});

test('the touch pad connects as pad 0 without a toast and the game hears it', () => {
  const { navigator, heard, toasts } = installed({ virtualWanted: true });
  navigator.getGamepads();
  assert.deepEqual(heard, [['gamepadconnected', 0, 'Touch screen controls']]);
  assert.deepEqual(toasts, []);
});
