// ABOUTME: The result card's page side: a button that shows while the match has a result worth saving, and what it does: copy the game's
// ABOUTME: frame, draw the card (web/match_result.js) and hand the PNG to the share sheet or a download. Reads game state, never writes it.
'use strict';

const R = typeof module !== 'undefined' && module.exports ? require('./match_result.js') : globalThis.wotmResult;
const FILE_NAME = 'war-of-the-monsters-result.png';
const POLL_MS = 500;

// Copies `source` (the game's canvas) in the next animation frame, while its buffer still holds the frame, into a scratch canvas.
// Resolves to that canvas, or null when it is blank, has no size or cannot be read (the card is then drawn without a picture).
function captureFrame(source, { raf, makeCanvas }) {
  return new Promise((resolve) => {
    raf(() => {
      try {
        if (!(source.width > 0 && source.height > 0)) return resolve(null);
        const copy = makeCanvas();
        copy.width = source.width;
        copy.height = source.height;
        const ctx = copy.getContext('2d');
        ctx.drawImage(source, 0, 0);
        resolve(R.isBlankFrame(ctx.getImageData(0, 0, copy.width, copy.height)) ? null : copy);
      } catch {
        resolve(null);
      }
    });
  });
}

// Shares the PNG as a file where the browser can, otherwise downloads it. Only the file goes to the share sheet: no title, text or
// address. A closed share sheet is the player's choice ('closed'); any other failure falls back to the download.
async function deliverPng(blob, { nav, makeFile, download }) {
  const file = makeFile(blob, FILE_NAME);
  if (nav?.canShare?.({ files: [file] })) {
    try {
      await nav.share({ files: [file] });
      return 'shared';
    } catch (e) {
      if (e?.name === 'AbortError') return 'closed';
    }
  }
  download(blob, FILE_NAME);
  return 'saved';
}

// Shows `button` while the game's score has a result worth saving; `cell` is wotm_web_result. Returns a function that stops it.
function watchResult({ button, cell, setTimer, clearTimer }) {
  const id = setTimer(() => { button.hidden = !R.resultAvailable(R.readResult(cell)); }, POLL_MS);
  return () => clearTimer(id);
}

// Makes and delivers the card for the score now: 'none' when there is nothing to save, else deliver's answer
async function saveCard({ cell, capture, makeCanvas, draw, deliver }) {
  const result = R.readResult(cell);
  if (!R.resultAvailable(result)) return 'none';
  const frame = await capture();
  const canvas = makeCanvas();
  canvas.width = R.CARD.width;
  canvas.height = R.CARD.height;
  draw(canvas.getContext('2d'), R.cardLayout(result), frame);
  const blob = await new Promise((resolve) => canvas.toBlob(resolve, 'image/png'));
  return blob ? deliver(blob) : 'none';
}

if (typeof module !== 'undefined' && module.exports) module.exports = { FILE_NAME, captureFrame, deliverPng, watchResult, saveCard };
else Object.assign(globalThis, { wotmResultUi: { FILE_NAME, captureFrame, deliverPng, watchResult, saveCard } });
