#!/usr/bin/env node
// ABOUTME: Plays one AI-vs-AI fight in a muted browser on the built web_dist and checks the match result card against the real game:
// ABOUTME: wotm_web_result is exported, the score reads as described, the Save button shows with a result, and the saved card is a PNG.
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { parseArgs } from 'node:util';
import { loadPlaywright, startLocal, launchBrowsers, launchFirefox, REPO } from './lib/infra.mjs';
import { Report } from './lib/report.mjs';
import { Run, formLobby, startMatch } from './lib/session.mjs';
import { sleep } from './lib/player.mjs';

const USAGE = `Usage: node tools/browser_mp_test/result_card.mjs [--browser chromium|chrome|brave|firefox] [--ai] [--max-secs 240] [--out <dir>]
  Starts a local site and server and plays a 2-player match to a wins target of 1 (round-ending cheat), always muted, then reads the
  score the game publishes (wotm_web_result) on both tabs once the Save result card button shows, saves the card on both and checks the PNGs.
  Exit code 1 when a row fails. Needs sh tools/build_web.sh first.`;
const { values: v } = parseArgs({ options: { browser: { type: 'string', default: 'chromium' }, 'max-secs': { type: 'string', default: '240' }, ai: { type: 'boolean', default: false }, out: { type: 'string' }, help: { type: 'boolean', default: false } } });
if (v.help) { console.log(USAGE); process.exit(0); }

const runtime = process.env.WOTM_RECOMP_DIR ?? join(REPO, 'runtime');
const outDir = resolve(v.out ?? join(runtime, 'result_card', `${v.browser}-${new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19)}`));
mkdirSync(outDir, { recursive: true });
const firefox = v.browser === 'firefox';

const target = await startLocal({ site: join(runtime, 'web_dist'), data: join(runtime, 'web_data'), server: join(REPO, 'native/net/build/wotm_server'), outDir });
// --ai: no round-ending cheat; the game's AI plays every seat and the fight runs to its own end (the rounds played counter moves)
if (v.ai) target.pageUrl += (target.pageUrl.includes('?') ? '&' : '?') + 'WOTM_AIVSAI=1';
const pw = await loadPlaywright();
const browsers = firefox ? [] : await launchBrowsers(pw, { n: 2, browser: v.browser });
const ff = firefox ? await launchFirefox(pw, {}) : null;
const report = new Report({ target: `local ${target.pageUrl.split('?')[0]}`, build: target.buildId, browser: v.browser, isolation: 'context', host: '', started: new Date().toISOString(), players: '2' });
const run = new Run({
  pw, target, browsers, firefox: ff, report, outDir,
  opts: { maxPlayers: 8, isolation: 'context', video: false, width: 640, height: 400, lobbyHeight: 900, firefoxCount: firefox ? 2 : 0, playSecs: 45, warmupSecs: 20, padlogFrames: 480, disc: '' },
});
const row = (name, ok, detail) => report.add(2, name, ok ? 'pass' : 'fail', detail);
const players = [];
const cellsOf = (p) => p.page.evaluate(() => {
  const f = window.Module?._wotm_web_result;
  return f ? Array.from({ length: 21 }, (_, i) => f(i)) : null;
});
// Save result card shares the PNG where the browser can share files and downloads it where it cannot: the page's navigator.share is
// replaced by a recorder (no share sheet opens), and either outcome must carry a 1200x630 PNG named without any player
const savedCard = async (p, name) => {
  await p.page.evaluate(() => {
    window.__shared = null;
    if (navigator.share) {
      navigator.canShare = () => true;
      navigator.share = async (d) => { window.__shared = await Promise.all((d.files ?? []).map(async (f) => ({ name: f.name, base64: btoa(String.fromCharCode(...new Uint8Array(await f.arrayBuffer()))) }))); };
    }
  });
  const download = p.page.waitForEvent('download', { timeout: 15000 }).catch(() => null);
  await p.page.click('#resultSave');
  const got = await Promise.race([download.then((d) => d && ['download', d]), p.page.waitForFunction(() => window.__shared, null, { timeout: 15000 }).then(() => ['share', null]).catch(() => null)]);
  let how = got?.[0], filename, bytes;
  if (how === 'download') { filename = got[1].suggestedFilename(); const file = join(outDir, `${name}-${filename}`); await got[1].saveAs(file); bytes = readFileSync(file); }
  else if (how === 'share') { const f = (await p.page.evaluate(() => window.__shared))[0]; filename = f.name; bytes = Buffer.from(f.base64, 'base64'); writeFileSync(join(outDir, `${name}-${filename}`), bytes); }
  else return { ok: false, detail: `${name}: neither a download nor a share after Save result card` };
  const png = bytes.subarray(0, 8).toString('hex') === '89504e470d0a1a0a';
  const w = bytes.readUInt32BE(16), h = bytes.readUInt32BE(20);
  return { ok: png && w === 1200 && h === 630 && filename === 'war-of-the-monsters-result.png', detail: `${name}: ${how} of ${filename} ${w}x${h}, ${bytes.length} bytes` };
};
try {
  // two players with the round-ending cheat (as roundSession): the first round won ends the match at a wins target of 1
  players.push(...await run.openPlayers(2, 'result', { env: v.ai ? {} : { WOTM_CHEATS: 'vencer=8' } }));
  const lobby = await formLobby(players);
  if (!lobby.ok) throw new Error(lobby.detail);
  await players[0].setSettings({ wins: 1 });
  await sleep(800);
  const started = await startMatch(players);
  if (!started.ok) throw new Error(`no start on ${started.missing.join(',')}`);
  await Promise.all(players.map((p) => p.focusGame()));
  // the score is read in the same poll that sees the button: the game moves on to its menus soon after the win
  const seen = [null, null];
  const deadline = Date.now() + Number(v['max-secs']) * 1000;
  while (Date.now() < deadline && !seen.every(Boolean)) {
    await sleep(1500);
    for (const [i, p] of players.entries()) {
      if (seen[i] || !await p.page.evaluate(() => !document.getElementById('resultSave').hidden)) continue;
      seen[i] = await cellsOf(p);
    }
  }
  const ended = { ok: seen.every(Boolean), missing: players.filter((_, i) => !seen[i]).map((p) => p.name) };
  row('export', seen.every(Boolean), 'wotm_web_result is exported and answers on every tab');
  const [a, b] = seen;
  row('score_cells', Boolean(a) && a[0] === 1 && a[1] === 2 && a[2] === 1 && a[4] >= 0, `session ${a?.[0]}, ${a?.[1]} fighters, wins target ${a?.[2]}, rounds played ${a?.[3]}, winner cell ${a?.[4]}, wins ${a?.[6]},${a?.[8]}`);
  row('same_score', JSON.stringify(a) === JSON.stringify(b), `tab 1 ${a}; tab 2 ${b}`);
  row('button_shows', ended.ok, ended.ok ? 'Save result card shows on both tabs' : `not on ${ended.missing.join(',')}`);
  if (ended.ok) {
    await players[0].shot(join(outDir, 'before-save.png'));
    for (const [i, p] of players.entries()) {
      const r = await savedCard(p, `tab${i + 1}`);
      row(`card_png_${i + 1}`, r.ok, r.detail);
    }
  }
  await run.saveEvidence(2, 'result', players, lobby.code).catch(() => {});
} catch (e) {
  row('error', false, e.stack?.split('\n').slice(0, 3).join(' ') ?? String(e));
} finally {
  await run.closeAll(players);
  report.write(outDir);
  for (const b of browsers) await b.close().catch(() => {});
  await ff?.close().catch(() => {});
  await target.stop();
}
console.log(`${report.failures.length ? 'FAILED' : 'PASSED'}: ${report.rows.length} rows. ${join(outDir, 'results.md')}`);
process.exit(report.failures.length ? 1 : 0);
