// ABOUTME: Loads one direct fight per monster (both seats that monster, stage 1) and per stage in the browser game and saves each page's
// ABOUTME: log up to the stage on screen: the input of tools/web_match_chunks.py (web/match_chunks.json). Run under caffeinate -dis on a Mac.
import { mkdirSync, writeFileSync } from 'node:fs';
import { spawn } from 'node:child_process';
import { join } from 'node:path';
import { REPO, freePort, loadPlaywright } from './browser_mp_test/lib/infra.mjs';

const USAGE = `Usage: node tools/web_match_logs.mjs [--site <dir>] [--data <dir>] [--out <dir>] [--only monster-3,stage-4]
  --site  a built page (default <repo>/runtime/web_dist)   --data  the disc folder (default <repo>/runtime/web_data)
  --out   where the logs go (default <repo>/runtime/match_logs): monster-<n>.log and stage-<n>.log
  --only  just these fights (names as the log files, without .log)
Then: python3 tools/web_match_chunks.py <data>/game.iso <out>`;

// the monsters and stages a lobby offers (web/front.js MONSTERS and STAGES)
const MONSTERS = [1, 2, 3, 4, 5, 7, 8, 9, 10, 11];
const STAGES = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16];
const LOADED = 'ready: stage on screen';

const o = { site: join(REPO, 'runtime/web_dist'), data: join(REPO, 'runtime/web_data'), out: join(REPO, 'runtime/match_logs'), only: null };
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i], v = () => process.argv[++i];
  if (a === '--help' || a === '-h') { console.log(USAGE); process.exit(0); }
  else if (a === '--site') o.site = v();
  else if (a === '--data') o.data = v();
  else if (a === '--out') o.out = v();
  else if (a === '--only') o.only = new Set(v().split(','));
  else { console.error(`unknown option ${a}\n${USAGE}`); process.exit(2); }
}

const fights = [...MONSTERS.map((m) => [`monster-${m}`, `${m},${m},1`]), ...STAGES.map((s) => [`stage-${s}`, `1,1,${s}`])]
  .filter(([name]) => !o.only || o.only.has(name));
mkdirSync(o.out, { recursive: true });
const port = await freePort();
const web = spawn('python3', [join(REPO, 'tools/serve_web.py'), '--port', String(port), '--site', o.site, '--data', o.data], { stdio: 'ignore' });
const pw = await loadPlaywright();
const browser = await pw.chromium.launch({ channel: 'chrome', headless: false, args: ['--autoplay-policy=no-user-gesture-required'] });
let failed = 0;
try {
  for (const [name, match] of fights) {
    const context = await browser.newContext({ viewport: { width: 640, height: 400 } });
    const page = await context.newPage();
    const log = () => page.evaluate(() => window.wotmLog || []).catch(() => []);
    for (let tries = 0; ; tries++) {
      try { await page.goto(`http://127.0.0.1:${port}/?WOTM_MATCH=${match}&WOTM_AIVSAI=1`); break; } catch (e) {
        if (tries > 20) throw e;
        await page.waitForTimeout(500);   // the web server is still starting
      }
    }
    const t0 = Date.now();
    let lines = [];
    while (!(lines = await log()).some((l) => l.includes(LOADED)) && Date.now() - t0 < 120000) await page.waitForTimeout(500);
    writeFileSync(join(o.out, `${name}.log`), lines.join('\n') + '\n');
    const ok = lines.some((l) => l.includes(LOADED));
    if (!ok) failed++;
    console.log(`${name} (WOTM_MATCH=${match}): ${ok ? `stage on screen after ${((Date.now() - t0) / 1000).toFixed(1)} s` : 'NO STAGE in 120 s'}`);
    await context.close();
  }
} finally {
  await browser.close();
  web.kill('SIGTERM');
}
console.log(`logs in ${o.out}; next: python3 tools/web_match_chunks.py ${join(o.data, 'game.iso')} ${o.out}`);
process.exit(failed ? 1 : 0);
