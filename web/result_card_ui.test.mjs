// ABOUTME: Tests for web/result_card_ui.js: capturing the game's frame for the card (a blank WebGL canvas gives none), handing the PNG to
// ABOUTME: the share sheet or a download, and the button that appears once a result is worth saving. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { captureFrame, deliverPng, watchResult, saveCard, FILE_NAME } = createRequire(import.meta.url)('./result_card_ui.js');

const blankData = { data: new Uint8ClampedArray(4 * 4) };
const litData = { data: new Uint8ClampedArray([0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255]) };

// a scratch canvas whose pixels read back as `data`
const scratch = (data) => ({ width: 0, height: 0, getContext: () => ({ drawImage() {}, getImageData: () => data }) });

test('captureFrame copies the game canvas in the next animation frame and returns it when it holds pixels', async () => {
  let frameCallback = null;
  const copies = [];
  const source = { width: 1280, height: 720 };
  const target = scratch(litData);
  target.getContext = () => ({ drawImage: (...a) => copies.push(a), getImageData: () => litData });
  const pending = captureFrame(source, { raf: (cb) => { frameCallback = cb; }, makeCanvas: () => target });
  assert.equal(copies.length, 0, 'nothing is read between frames: the buffer would be cleared');
  frameCallback();
  assert.equal(await pending, target);
  assert.equal(copies.length, 1);
  assert.equal(copies[0][0], source);
  assert.deepEqual([target.width, target.height], [1280, 720]);
});

test('captureFrame gives null for a blank canvas, so the card is drawn without a picture', async () => {
  const pending = captureFrame({ width: 4, height: 4 }, { raf: (cb) => cb(), makeCanvas: () => scratch(blankData) });
  assert.equal(await pending, null);
});

test('captureFrame gives null for a canvas with no size, or one that refuses to be read', async () => {
  assert.equal(await captureFrame({ width: 0, height: 0 }, { raf: (cb) => cb(), makeCanvas: () => scratch(litData) }), null);
  const refuses = { width: 4, height: 4, getContext: () => ({ drawImage() {}, getImageData() { throw new Error('tainted'); } }) };
  assert.equal(await captureFrame({ width: 4, height: 4 }, { raf: (cb) => cb(), makeCanvas: () => refuses }), null);
});

const png = { type: 'image/png' };

test('deliverPng shares the file where the browser can share files', async () => {
  const shared = [];
  const nav = { canShare: (d) => d.files.length === 1, share: async (d) => { shared.push(d); } };
  const how = await deliverPng(png, { nav, makeFile: (blob, name) => ({ blob, name }), download: () => assert.fail('no download') });
  assert.equal(how, 'shared');
  assert.deepEqual(shared[0].files, [{ blob: png, name: FILE_NAME }]);
  assert.deepEqual(Object.keys(shared[0]), ['files'], 'no title, text or address goes with it');
});

test('deliverPng downloads where sharing files is not possible', async () => {
  const saved = [];
  const how = await deliverPng(png, { nav: {}, makeFile: (b, n) => ({ b, n }), download: (blob, name) => saved.push([blob, name]) });
  assert.equal(how, 'saved');
  assert.deepEqual(saved, [[png, FILE_NAME]]);
});

test('deliverPng falls back to a download when the share sheet fails, but not when the player closes it', async () => {
  const saved = [];
  const failing = { canShare: () => true, share: async () => { throw new Error('boom'); } };
  assert.equal(await deliverPng(png, { nav: failing, makeFile: () => ({}), download: (b) => saved.push(b) }), 'saved');
  assert.equal(saved.length, 1);
  const dismissed = { canShare: () => true, share: async () => { throw Object.assign(new Error('closed'), { name: 'AbortError' }); } };
  assert.equal(await deliverPng(png, { nav: dismissed, makeFile: () => ({}), download: (b) => saved.push(b) }), 'closed');
  assert.equal(saved.length, 1, 'a closed sheet is a choice, not a failure');
});

test('the file name has no player name or address in it', () => {
  assert.equal(FILE_NAME, 'war-of-the-monsters-result.png');
});

test('watchResult shows the button while a result is worth saving and hides it otherwise', () => {
  const button = { hidden: true };
  let cellsNow = [0, 0, 0, 0, -1];
  const timers = [];
  const stop = watchResult({ button, cell: (i) => cellsNow[i] ?? 0, setTimer: (fn) => { timers.push(fn); return 1; }, clearTimer: () => {} });
  timers[0]();
  assert.equal(button.hidden, true);
  cellsNow = [1, 2, 3, 1, -1, 5, 1, 1, 0];
  timers[0]();
  assert.equal(button.hidden, false);
  cellsNow = [0];
  timers[0]();
  assert.equal(button.hidden, true);
  assert.equal(typeof stop, 'function');
});

test('saveCard reads the score, captures the frame, draws the card at its size and delivers the PNG', async () => {
  const events = [];
  const cardCanvas = { width: 0, height: 0, getContext: () => ({ tag: 'ctx' }), toBlob: (cb, type) => { events.push(['toBlob', type]); cb(png); } };
  const cells = [1, 2, 3, 4, 0, 5, 3, 1, 1];
  const how = await saveCard({
    cell: (i) => cells[i] ?? 0,
    capture: async () => { events.push(['capture']); return { width: 10, height: 10 }; },
    makeCanvas: () => cardCanvas,
    draw: (ctx, layout, frame) => events.push(['draw', ctx.tag, layout.title, frame.width]),
    deliver: async (blob) => { events.push(['deliver', blob]); return 'saved'; },
  });
  assert.equal(how, 'saved');
  assert.deepEqual(events, [['capture'], ['draw', 'ctx', 'Preytor wins the match', 10], ['toBlob', 'image/png'], ['deliver', png]]);
  assert.deepEqual([cardCanvas.width, cardCanvas.height], [1200, 630]);
});

test('saveCard still makes a card when the frame could not be captured', async () => {
  const cardCanvas = { getContext: () => ({}), toBlob: (cb) => cb(png) };
  const frames = [];
  await saveCard({ cell: (i) => [1, 2, 3, 4, 0, 5, 3, 1, 1][i] ?? 0, capture: async () => null, makeCanvas: () => cardCanvas,
    draw: (ctx, layout, frame) => frames.push(frame), deliver: async () => 'saved' });
  assert.deepEqual(frames, [null]);
});

test('saveCard does nothing when there is no result to save', async () => {
  const how = await saveCard({ cell: () => 0, capture: async () => assert.fail('no capture'), makeCanvas: () => assert.fail('no canvas'), draw() {}, deliver: async () => assert.fail('nothing to deliver') });
  assert.equal(how, 'none');
});
