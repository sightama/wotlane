// ABOUTME: The match result card: reads the score the game publishes (wotm_web_result, native/wotm_matchresult.hpp), says when a card
// ABOUTME: is worth offering, lays it out as data and draws it on a canvas over the final frame. No player names, no address.
'use strict';

const CELLS = 21;   // native/wotm_matchresult.hpp kCells
const CARD = { width: 1200, height: 630 };

// monster type -> the name the lobby shows (web/front.js MONSTERS), plus the one the game has that the lobby does not list
const MONSTER_NAMES = { 1: 'Congar', 2: 'Robo-47', 3: 'Togera', 4: 'Ultra V', 5: 'Preytor', 6: 'Spider', 7: 'Kineticlops', 8: 'Magmo',
  9: 'Raptros', 10: 'Agamo', 11: 'Zorgulon' };
const monsterName = (type) => MONSTER_NAMES[type] ?? 'Monster';

// `cell(i)`: wotm_web_result (undefined where the game has not started). The score as { session, target, rounds, winner, fighters }
function readResult(cell) {
  const n = (i) => { const v = Number(cell?.(i)); return Number.isFinite(v) ? v : 0; };
  const session = n(0) === 1;
  const count = session ? Math.max(0, Math.min(8, n(1))) : 0;
  return {
    session,
    target: n(2),
    rounds: n(3),
    winner: n(4),
    fighters: Array.from({ length: count }, (_, i) => ({ type: n(5 + 2 * i), wins: n(6 + 2 * i) })),
  };
}

// a card is worth offering once two or more fight in a session and a round has been won
function resultAvailable(r) {
  return r.session && r.fighters.length >= 2 && r.fighters.reduce((sum, f) => sum + f.wins, 0) >= 1;
}

const plural = (n, word) => `${n} ${word}${n === 1 ? '' : 's'}`;

// The card's words as data: { title, subtitle, rows: [{ name, wins, lead }] }. Rows go by wins, then slot; fighters of one monster are
// numbered by slot. Everything comes from the score, so two clients reading one score lay out one card.
function cardLayout(r) {
  const top = Math.max(0, ...r.fighters.map((f) => f.wins));
  const sameMonster = (type) => r.fighters.filter((f) => f.type === type).length > 1;
  const rows = r.fighters.map((f, slot) => ({ name: monsterName(f.type) + (sameMonster(f.type) ? ` (${slot + 1})` : ''), wins: f.wins, lead: f.wins === top, slot }))
    .sort((a, b) => b.wins - a.wins || a.slot - b.slot)
    .map(({ slot, ...row }) => row);
  let title;
  if (r.winner >= 0 && r.fighters[r.winner]) title = `${monsterName(r.fighters[r.winner].type)} wins the match`;
  else if (r.winner === -2) title = `Tied at ${plural(r.target, 'win')}`;
  else title = r.target > 0 ? `First to ${plural(r.target, 'win')}` : 'The match so far';
  const parts = [];
  if (r.target > 0 && !title.startsWith('First to')) parts.push(`First to ${plural(r.target, 'win')}`);
  if (r.rounds > 0) parts.push(`${plural(r.rounds, 'round')} played`);
  return { title, subtitle: parts.join(', '), rows };
}

// Draws the card on `ctx` (a CARD.width x CARD.height 2D context). `frame`: anything drawImage takes with width and height (the
// final frame), or null for a card without one
function drawCard(ctx, layout, frame) {
  const { width: W, height: H } = CARD;
  const font = (px, weight = 400) => { ctx.font = `${weight} ${px}px system-ui, sans-serif`; };
  const fit = (text, px, weight, maxWidth) => {
    font(px, weight);
    while (px > 24 && ctx.measureText(text).width > maxWidth) font(px -= 2, weight);
    return px;
  };
  ctx.fillStyle = '#0b0b0d';
  ctx.fillRect(0, 0, W, H);
  if (frame) {
    const scale = Math.max(W / frame.width, H / frame.height);
    const w = frame.width * scale, h = frame.height * scale;
    ctx.drawImage(frame, (W - w) / 2, (H - h) / 2, w, h);
  } else {
    const g = ctx.createRadialGradient(W * 0.7, H * 1.2, 40, W * 0.7, H * 1.2, W * 0.9);
    g.addColorStop(0, '#7a2a0e');
    g.addColorStop(1, '#0b0b0d');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, W, H);
  }
  // the panel the words sit on: dark, so they read over any frame
  const shade = ctx.createLinearGradient(0, 0, W * 0.72, 0);
  shade.addColorStop(0, 'rgba(11, 11, 13, 0.94)');
  shade.addColorStop(1, 'rgba(11, 11, 13, 0.55)');
  ctx.fillStyle = shade;
  ctx.fillRect(0, 0, W, H);
  ctx.fillStyle = '#e0592a';
  ctx.fillRect(0, 0, 10, H);

  const x = 70, textWidth = W * 0.62;
  ctx.textBaseline = 'alphabetic';
  ctx.textAlign = 'left';
  ctx.fillStyle = '#e0592a';
  font(26, 700);
  ctx.fillText('WAR OF THE MONSTERS', x, 90);
  ctx.fillStyle = '#ffffff';
  fit(layout.title, 72, 700, textWidth);
  ctx.fillText(layout.title, x, 180);
  ctx.fillStyle = '#b4b4bc';
  font(30, 400);
  if (layout.subtitle) ctx.fillText(layout.subtitle, x, 232);
  let y = 320;
  const rowH = Math.min(70, Math.floor(280 / Math.max(1, layout.rows.length)));
  for (const row of layout.rows) {
    ctx.fillStyle = row.lead ? '#ffffff' : '#b4b4bc';
    font(Math.round(rowH * 0.56), row.lead ? 700 : 400);
    ctx.textAlign = 'left';
    ctx.fillText(row.name, x, y);
    ctx.textAlign = 'right';
    ctx.fillStyle = row.lead ? '#e0592a' : '#b4b4bc';
    ctx.fillText(String(row.wins), x + textWidth * 0.8, y);
    y += rowH;
  }
}

// whether a frame read back from a canvas is blank: every pixel transparent, which is what a WebGL canvas gives once its buffer was
// presented (an opaque black pixel is a real black frame)
function isBlankFrame(imageData) {
  const d = imageData.data;
  for (let i = 3; i < d.length; i += 4) if (d[i] !== 0) return false;
  return true;
}

if (typeof module !== 'undefined' && module.exports) module.exports = { CELLS, CARD, monsterName, readResult, resultAvailable, cardLayout, drawCard, isBlankFrame };
else Object.assign(globalThis, { wotmResult: { CELLS, CARD, monsterName, readResult, resultAvailable, cardLayout, drawCard, isBlankFrame } });
