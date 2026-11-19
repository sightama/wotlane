// ABOUTME: Records a Chrome trace with V8's CPU profiler (every thread, the game's worker included) of one offline AI vs AI direct
// ABOUTME: fight, for tools/chrome_trace_top.py to say where the game thread's time goes. Needs a web_dist built with wasm names.
import { mkdirSync, existsSync, statSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { spawn } from 'node:child_process';
import { REPO, freePort, loadPlaywright, launchBrowsers } from './lib/infra.mjs';

const USAGE = `usage: node tools/browser_mp_test/profile_chrome.mjs [options]
  --browser chrome|brave|chromium   default chrome (the installed one, its GPU)
  --match SPEC    direct fight (WOTM_MATCH, default 3,1,1; 8 monsters: 3,1,2,4,5,7,8,9,1)
  --step 1|2      fixed step in fields (WOTM_FIXEDSTEP, default 1 = 60 Hz; 2 = 30 Hz)
  --warmup S      seconds of fight before the recording starts (default 30: the stage is loaded and the code warm)
  --seconds S     seconds recorded (default 20; a trace grows by ~10 MB a second)
  --site DIR      built page with wasm names (default <runtime>/web_dist_prof)   --data DIR   (default <runtime>/web_data)
  --env K=V       more game environment (repeatable)
  --out FILE      the trace (default <runtime>/chrome_profile/<browser>_<tag>.json)   --headed
Then: python3 tools/chrome_trace_top.py <trace>. Build the page with names as for profile_firefox.mjs (--profiling-funcs).`;

function args() {
  const o = { browser: 'chrome', match: '3,1,1', step: 1, warmup: 30, seconds: 20, site: '', data: '', env: {}, out: '', headed: false };
  const a = process.argv.slice(2);
  for (let i = 0; i < a.length; i++) {
    const k = a[i], v = () => { if (i + 1 >= a.length) { console.error(USAGE); process.exit(2); } return a[++i]; };
    if (k === '-h' || k === '--help') { console.log(USAGE); process.exit(0); }
    else if (k === '--browser') o.browser = v();
    else if (k === '--match') o.match = v();
    else if (k === '--step') o.step = Number(v());
    else if (k === '--warmup') o.warmup = Number(v());
    else if (k === '--seconds') o.seconds = Number(v());
    else if (k === '--site') o.site = v();
    else if (k === '--data') o.data = v();
    else if (k === '--out') o.out = v();
    else if (k === '--headed') o.headed = true;
    else if (k === '--env') { const [n, ...rest] = v().split('='); o.env[n] = rest.join('='); }
    else { console.error(USAGE); process.exit(2); }
  }
  const rt = join(REPO, 'runtime');
  o.site = resolve(o.site || join(rt, 'web_dist_prof'));
  o.data = resolve(o.data || join(rt, 'web_data'));
  o.out = resolve(o.out || join(rt, 'chrome_profile', `${o.browser}_${o.step === 2 ? '30hz' : '60hz'}_${o.match.split(',').length - 1}m.json`));
  if (!existsSync(join(o.site, 'wotm.wasm'))) { console.error(`no wotm.wasm in ${o.site}\n${USAGE}`); process.exit(2); }
  return o;
}

// the game's environment before boot.js runs it (as in synctest.mjs)
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
  mkdirSync(join(o.out, '..'), { recursive: true });
  const port = await freePort();
  const web = spawn('python3', [join(REPO, 'tools/serve_web.py'), '--port', String(port), '--site', o.site, '--data', o.data], { stdio: 'ignore' });
  const pw = await loadPlaywright();
  const [browser] = await launchBrowsers(pw, { n: 1, browser: o.browser, headed: o.headed, unlockFrames: true });
  let lastLog = [];
  try {
    const context = await browser.newContext({ viewport: { width: 960, height: 540 } });
    await context.addInitScript(envInit, { WOTM_FIXEDSTEP: String(o.step), WOTM_SEED: '7', WOTM_MUTE: '1', ...o.env });
    const page = await context.newPage();
    for (let t = 0; t < 60; t++) {   // the page server takes a moment
      try { await page.goto(`http://127.0.0.1:${port}/?WOTM_MATCH=${o.match}&WOTM_AIVSAI=1`); break; } catch { await sleep(500); }
    }
    console.log(`profiling ${o.browser}: match ${o.match}, step ${o.step}, ${o.warmup} s warm-up then ${o.seconds} s; trace ${o.out}`);
    await sleep(o.warmup * 1000);
    await browser.startTracing(page, { path: o.out, categories: ['disabled-by-default-v8.cpu_profiler', 'v8', '__metadata'] });
    await sleep(o.seconds * 1000);
    await browser.stopTracing();
    lastLog = await page.evaluate(() => (window.wotmLog || []).slice(-3)).catch(() => []);
  } finally {
    await browser.close().catch(() => {});
    web.kill();
  }
  for (const l of lastLog) console.log(l.slice(0, 300));
  if (!existsSync(o.out)) { console.error(`no trace written at ${o.out}`); process.exit(1); }
  console.log(`trace: ${o.out} (${(statSync(o.out).size / 1e6).toFixed(1)} MB)`);
}

main().catch((e) => { console.error(e); process.exit(1); });
