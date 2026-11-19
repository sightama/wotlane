// ABOUTME: Rollback's synctest in the browser: serves a built web_dist, opens an AI vs AI direct fight in Chrome, Chromium or Firefox with
// ABOUTME: WOTM_SYNCTEST (every fight frame goes n frames back and is redone) and prints the game's [synctest] cost and mismatch lines.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { spawn } from 'node:child_process';
import { REPO, freePort, loadPlaywright, launchBrowsers, launchFirefox, firefoxPref } from './lib/infra.mjs';

const USAGE = `usage: node tools/browser_mp_test/synctest.mjs [options]
  --browser chrome|chromium|firefox   default chrome (the real one, its GPU)
  --back N        frames back at every frame (WOTM_SYNCTEST, 1 to 15, default 2)
  --match SPEC    direct fight (WOTM_MATCH, default 3,1,1; 8 monsters: 3,1,2,4,5,7,8,9,1)
  --step 1|2      fixed step in fields (WOTM_FIXEDSTEP, default 1 = 60 Hz; 2 = 30 Hz)
  --minutes M     how long to run (default 3)
  --site DIR      built page (default <runtime>/web_dist)   --data DIR   game.iso and SCUS_971.97 (default <runtime>/web_data)
  --env K=V       more game environment (repeatable: WOTM_SYNCTEST_SKIP=sound, WOTM_FRAMESPIN=8)
  --firefox-pref NAME=VALUE   an about:config preference for Firefox (repeatable: javascript.options.wasm_baselinejit=false runs
                  the wasm only in the optimizing tier)
  --out DIR       where the game's log goes (default <runtime>/browser_synctest)   --headed
Exit status 1 when the game logged a mismatch or no check ran.`;

function args() {
  const o = { browser: 'chrome', back: 2, match: '3,1,1', step: 1, minutes: 3, site: '', data: '', env: {}, prefs: {}, out: '', headed: false };
  const a = process.argv.slice(2);
  for (let i = 0; i < a.length; i++) {
    const k = a[i], v = () => a[++i];
    if (k === '-h' || k === '--help') { console.log(USAGE); process.exit(0); }
    else if (k === '--browser') o.browser = v();
    else if (k === '--back') o.back = Number(v());
    else if (k === '--match') o.match = v();
    else if (k === '--step') o.step = Number(v());
    else if (k === '--minutes') o.minutes = Number(v());
    else if (k === '--site') o.site = v();
    else if (k === '--data') o.data = v();
    else if (k === '--out') o.out = v();
    else if (k === '--headed') o.headed = true;
    else if (k === '--env') { const [n, ...rest] = v().split('='); o.env[n] = rest.join('='); }
    else if (k === '--firefox-pref') { const [n, val] = firefoxPref(v()); o.prefs[n] = val; }
    else { console.error(USAGE); process.exit(2); }
  }
  const rt = join(REPO, 'runtime');
  o.site = resolve(o.site || join(rt, 'web_dist'));
  o.data = resolve(o.data || join(rt, 'web_data'));
  o.out = resolve(o.out || join(rt, 'browser_synctest'));
  if (!(Number.isInteger(o.back) && o.back >= 1 && o.back <= 15)) { console.error('--back takes 1 to 15'); process.exit(2); }
  return o;
}

// the game's environment before boot.js runs it (the harness's Player does the same)
function envInit(env) {
  let module;
  Object.defineProperty(window, 'Module', {
    configurable: true,
    get() { return module; },
    set(v) {
      module = v;
      if (v && Array.isArray(v.preRun)) v.preRun.push(() => Object.assign(v.ENV, env));
    },
  });
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function main() {
  const o = args();
  mkdirSync(o.out, { recursive: true });
  const port = await freePort();
  const web = spawn('python3', [join(REPO, 'tools/serve_web.py'), '--port', String(port), '--site', o.site, '--data', o.data], { stdio: 'ignore' });
  const pw = await loadPlaywright();
  const browser = o.browser === 'firefox' ? await launchFirefox(pw, { headed: o.headed, prefs: o.prefs })
    : (await launchBrowsers(pw, { n: 1, browser: o.browser, headed: o.headed, unlockFrames: true }))[0];
  const extra = Object.entries(o.env).map(([k, v]) => `_${k.replace(/^WOTM_/, '').toLowerCase()}${v}`).join('').replace(/[^A-Za-z0-9_.-]/g, '-');
  const tag = `${o.browser}_${o.step === 2 ? '30hz' : '60hz'}_back${o.back}_${o.match.split(',').length - 1}m${extra}`;
  const logPath = join(o.out, `${tag}.log`);
  const lines = [];
  let mismatches = 0, checks = 0;
  try {
    const context = await browser.newContext({ viewport: { width: 960, height: 540 } });
    const env = { WOTM_FIXEDSTEP: String(o.step), WOTM_SEED: '7', WOTM_SYNCTEST: String(o.back), WOTM_MUTE: '1', ...o.env };
    await context.addInitScript(envInit, env);
    const page = await context.newPage();
    page.on('pageerror', (e) => lines.push(`[page error] ${e.message}`));
    for (let t = 0; t < 60; t++) {   // the page server takes a moment
      try { await page.goto(`http://127.0.0.1:${port}/?WOTM_MATCH=${o.match}&WOTM_AIVSAI=1`); break; } catch { await sleep(500); }
    }
    console.log(`synctest in ${o.browser}: ${o.back} back, match ${o.match}, step ${o.step}, ${o.minutes} min; log ${logPath}`);
    let seen = 0;
    const end = Date.now() + o.minutes * 60000;
    while (Date.now() < end) {
      await sleep(2000);
      const fresh = await page.evaluate((from) => (window.wotmLog || []).slice(from), seen).catch(() => []);
      seen += fresh.length;
      for (const l of fresh) {
        lines.push(l);
        const at = l.indexOf('[synctest]');   // the page prefixes its seconds
        if (at < 0 || l.startsWith('[synctest]   ', at)) continue;
        if (l.startsWith('[synctest] MISMATCH', at)) ++mismatches;
        const m = / (\d+) checks, /.exec(l);
        if (m) checks += Number(m[1]);
        console.log(l.slice(0, 400));
      }
      writeFileSync(logPath, lines.join('\n') + '\n');
    }
  } finally {
    await browser.close().catch(() => {});
    web.kill();
  }
  console.log(`summary: ${checks} checks in the cost lines, ${mismatches} mismatch line(s)`);
  process.exit(checks > 0 && mismatches === 0 ? 0 : 1);
}

main().catch((e) => { console.error(e); process.exit(1); });
