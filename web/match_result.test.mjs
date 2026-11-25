// ABOUTME: Tests for web/match_result.js: reading the match score the game publishes (wotm_web_result cells), when a result card is
// ABOUTME: offered, the card's layout data and its drawing (no names, no address), and a blank frame. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { CELLS, readResult, resultAvailable, cardLayout, drawCard, isBlankFrame, monsterName, CARD } = createRequire(import.meta.url)('./match_result.js');

// cells as native/wotm_matchresult.hpp publishes them: session, fighters, target, rounds, winner, then type and wins per fighter
const cells = (session, count, target, rounds, winner, fighters) => {
  const c = new Array(CELLS).fill(0);
  Object.assign(c, [session, count, target, rounds, winner]);
  fighters.forEach(([type, wins], i) => { c[5 + 2 * i] = type; c[6 + 2 * i] = wins; });
  return c;
};
const read = (c) => readResult((i) => c[i]);

test('readResult turns the cells into the score', () => {
  const r = read(cells(1, 2, 3, 4, 0, [[5, 3], [1, 1]]));
  assert.deepEqual(r, { session: true, target: 3, rounds: 4, winner: 0, fighters: [{ type: 5, wins: 3 }, { type: 1, wins: 1 }] });
});

test('readResult in menus says no session and no fighters', () => {
  const r = read(cells(0, 0, 0, 0, -1, []));
  assert.equal(r.session, false);
  assert.deepEqual(r.fighters, []);
});

test('readResult takes whatever the export returns for a missing function as no session', () => {
  assert.equal(readResult(undefined).session, false);
  assert.equal(readResult(() => NaN).session, false);
});

test('a card is offered once two fighters are in a session and a round has been won', () => {
  assert.equal(resultAvailable(read(cells(1, 2, 3, 0, -1, [[5, 0], [1, 0]]))), false);   // nobody has won a round
  assert.equal(resultAvailable(read(cells(1, 2, 3, 1, -1, [[5, 1], [1, 0]]))), true);
  assert.equal(resultAvailable(read(cells(1, 1, 3, 1, -1, [[5, 1]]))), false);           // alone: no match
  assert.equal(resultAvailable(read(cells(0, 2, 3, 1, -1, [[5, 1], [1, 0]]))), false);   // menus
});

test('the title names the winner of the match', () => {
  const l = cardLayout(read(cells(1, 2, 3, 4, 0, [[5, 3], [1, 1]])));
  assert.equal(l.title, 'Preytor wins the match');
  assert.equal(l.subtitle, 'First to 3 wins, 4 rounds played');
});

test('before the target is reached the card says how the match stands', () => {
  assert.equal(cardLayout(read(cells(1, 2, 3, 2, -1, [[5, 2], [1, 0]]))).title, 'First to 3 wins');
  assert.equal(cardLayout(read(cells(1, 2, 0, 2, -1, [[5, 2], [1, 0]]))).title, 'The match so far');   // endless
  assert.equal(cardLayout(read(cells(1, 2, 3, 6, -2, [[5, 3], [1, 3]]))).title, 'Tied at 3 wins');
});

test('rows go by wins, then by slot, and mark the leader', () => {
  const l = cardLayout(read(cells(1, 3, 3, 4, -1, [[1, 1], [5, 2], [3, 1]])));
  assert.deepEqual(l.rows.map((r) => [r.name, r.wins, r.lead]), [['Preytor', 2, true], ['Congar', 1, false], ['Togera', 1, false]]);
});

test('two fighters of one monster are told apart by slot', () => {
  const l = cardLayout(read(cells(1, 2, 3, 2, -1, [[5, 1], [5, 1]])));
  assert.deepEqual(l.rows.map((r) => r.name), ['Preytor (1)', 'Preytor (2)']);
  assert.deepEqual(l.rows.map((r) => r.lead), [true, true]);   // a tie leads together
});

test('every client reading the same score lays out the same card', () => {
  const a = cardLayout(read(cells(1, 4, 5, 9, -1, [[1, 2], [2, 4], [3, 0], [4, 4]])));
  const b = cardLayout(read(cells(1, 4, 5, 9, -1, [[1, 2], [2, 4], [3, 0], [4, 4]])));
  assert.deepEqual(a, b);
});

test('monster names come from the lobby\'s list, and an unknown type gets a plain name', () => {
  assert.equal(monsterName(1), 'Congar');
  assert.equal(monsterName(11), 'Zorgulon');
  assert.equal(monsterName(6), 'Spider');
  assert.equal(monsterName(0), 'Monster');
  assert.equal(monsterName(99), 'Monster');
});

// a 2D context that records its calls: text is 10 px a character, gradients take stops, everything else is noted
function fakeCtx(calls) {
  return new Proxy({}, {
    get: (_, name) => {
      if (name === 'measureText') return (text) => ({ width: String(text).length * 10 });
      if (name === 'createLinearGradient' || name === 'createRadialGradient') return () => ({ addColorStop() {} });
      return (...args) => { calls.push([name, args]); };
    },
    set: () => true,
  });
}

test('the card holds no address, user name or other text than the layout\'s', () => {
  const l = cardLayout(read(cells(1, 2, 3, 4, 0, [[5, 3], [1, 1]])));
  const calls = [];
  drawCard(fakeCtx(calls), l, null);
  const texts = calls.filter(([name]) => name === 'fillText').map(([, args]) => args[0]);
  for (const t of [l.title, l.subtitle, ...l.rows.map((r) => r.name)]) assert.ok(texts.includes(t), `draws ${t}`);
  assert.doesNotMatch(texts.join('|'), /https?:|\.com|\d+\.\d+\.\d+\.\d+|playwotm/i);
  assert.equal(texts.length, 3 + l.rows.length * 2, 'the kicker, title, subtitle, and a name and a score per row');
});

test('drawCard lays the final frame under the panel, covering the card, when there is one', () => {
  const calls = [];
  drawCard(fakeCtx(calls), cardLayout(read(cells(1, 2, 3, 4, 0, [[5, 3], [1, 1]]))), { width: 1280, height: 720 });
  const draw = calls.find(([name]) => name === 'drawImage');
  assert.ok(draw, 'the frame is drawn');
  const [, dx, dy, dw, dh] = draw[1];
  assert.ok(dx <= 0 && dy <= 0 && dx + dw >= CARD.width && dy + dh >= CARD.height, 'cover: it fills the whole card');
  assert.ok(Math.abs(dw / dh - 1280 / 720) < 1e-6, 'and keeps its shape');
});

test('drawCard without a frame still draws a card', () => {
  const calls = [];
  drawCard(fakeCtx(calls), cardLayout(read(cells(1, 2, 3, 4, 0, [[5, 3], [1, 1]]))), null);
  assert.ok(!calls.some(([name]) => name === 'drawImage'));
  assert.ok(calls.some(([name]) => name === 'fillText'));
});

test('a frame whose pixels are all transparent is blank (a WebGL canvas read after it was cleared)', () => {
  assert.equal(isBlankFrame({ data: new Uint8ClampedArray(16 * 4) }), true);
  assert.equal(isBlankFrame({ data: new Uint8ClampedArray([0, 0, 0, 255, 0, 0, 0, 0]) }), false);   // an opaque black pixel is a real frame
});

test('each fighter gets a bar as long as its share of the wins target, the leader\'s in the accent colour', () => {
  const l = cardLayout(read(cells(1, 2, 4, 5, -1, [[5, 3], [1, 1]])));
  const fills = [];
  const proxy = {
    fillStyle: '',
    fillRect(...a) { fills.push([this.fillStyle, ...a]); },
    measureText: (text) => ({ width: String(text).length * 10 }),
    createLinearGradient: () => ({ addColorStop() {} }),
    createRadialGradient: () => ({ addColorStop() {} }),
    fillText() {},
  };
  drawCard(proxy, l, null);
  const bars = fills.filter(([, , , , h]) => h === 12);
  assert.equal(bars.length, 4, 'a track and a fill for each of the two fighters');
  const [leadTrack, leadFill, otherTrack, otherFill] = bars;
  assert.equal(leadTrack[3], otherTrack[3]);
  assert.ok(Math.abs(leadFill[3] / leadTrack[3] - 3 / 4) < 1e-9 && Math.abs(otherFill[3] / otherTrack[3] - 1 / 4) < 1e-9);
  assert.equal(leadFill[0], '#e0592a');
  assert.notEqual(otherFill[0], '#e0592a');
});
