// ABOUTME: Records a Firefox profile (Gecko profiler, every thread's JS and wasm stacks) of one offline AI vs AI direct fight, for
// ABOUTME: tools/gecko_profile_top.py to say where the game thread's time goes. Needs a web_dist built with wasm names (--profiling-funcs).
import { mkdirSync, existsSync, statSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { spawn } from 'node:child_process';
import { REPO, freePort, loadPlaywright, firefoxPref } from './lib/infra.mjs';

const USAGE = `usage: node tools/browser_mp_test/profile_firefox.mjs [options]
  --match SPEC    direct fight (WOTM_MATCH, default 3,1,1; 8 monsters: 3,1,2,4,5,7,8,9,1)
  --step 1|2      fixed step in fields (WOTM_FIXEDSTEP, default 1 = 60 Hz; 2 = 30 Hz)
  --seconds S     how long the fight runs before the browser closes (default 60); the profile keeps the last part that fits
  --interval MS   sampling interval (default 1)
  --site DIR      built page with wasm names (default <runtime>/web_dist_prof)   --data DIR   (default <runtime>/web_data)
  --env K=V       more game environment (repeatable: WOTM_SYNCTEST=2, WOTM_FRAMESPIN=8)
  --firefox-pref NAME=VALUE   an about:config preference (repeatable: javascript.options.wasm_lazy_tiering=false)
  --out FILE      the profile (default <runtime>/firefox_profile/<tag>.json)   --headed
Then: python3 tools/gecko_profile_top.py <profile>. Build the page with names: in <runtime>/build_web set
CMAKE_EXE_LINKER_FLAGS=--profiling-funcs, run tools/build_web.sh, copy web_dist aside, set the flag back.`;

function args() {
  const o = { match: '3,1,1', step: 1, seconds: 60, interval: 1, site: '', data: '', env: {}, prefs: {}, out: '', headed: false };
  const a = process.argv.slice(2);
  for (let i = 0; i < a.length; i++) {
    const k = a[i], v = () => { if (i + 1 >= a.length) { console.error(USAGE); process.exit(2); } return a[++i]; };
    if (k === '-h' || k === '--help') { console.log(USAGE); process.exit(0); }
    else if (k === '--match') o.match = v();
    else if (k === '--step') o.step = Number(v());
    else if (k === '--seconds') o.seconds = Number(v());
    else if (k === '--interval') o.interval = Number(v());
    else if (k === '--site') o.site = v();
    else if (k === '--data') o.data = v();
    else if (k === '--out') o.out = v();
    else if (k === '--headed') o.headed = true;
    else if (k === '--env') { const [n, ...rest] = v().split('='); o.env[n] = rest.join('='); }
    else if (k === '--firefox-pref') { const [n, val] = firefoxPref(v()); o.prefs[n] = val; }
    else { console.error(USAGE); process.exit(2); }
  }
  const rt = join(REPO, 'runtime');
  o.site = resolve(o.site || join(rt, 'web_dist_prof'));
  o.data = resolve(o.data || join(rt, 'web_data'));
  const extra = Object.entries(o.env).map(([k, v]) => `_${k.replace(/^WOTM_/, '').toLowerCase()}${v}`).join('').replace(/[^A-Za-z0-9_.-]/g, '-');
  o.out = resolve(o.out || join(rt, 'firefox_profile', `${o.step === 2 ? '30hz' : '60hz'}_${o.match.split(',').length - 1}m${extra}.json`));
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
  // the startup profiler writes the profile when the browser shuts down
  const browser = await pw.firefox.launch({
    ...firefoxOptions({ headed: o.headed, prefs: o.prefs }),
    env: {
      ...process.env,
      MOZ_PROFILER_STARTUP: '1',
      MOZ_PROFILER_STARTUP_INTERVAL: String(o.interval),
      MOZ_PROFILER_STARTUP_ENTRIES: '16000000',
      MOZ_PROFILER_STARTUP_FEATURES: 'js,stackwalk,cpu',
      MOZ_PROFILER_STARTUP_FILTERS: 'GeckoMain,DOM Worker',
      MOZ_PROFILER_SHUTDOWN: o.out,
    },
  });
  let lastLog = [];
  try {
    const context = await browser.newContext({ viewport: { width: 960, height: 540 } });
    const env = { WOTM_FIXEDSTEP: String(o.step), WOTM_SEED: '7', WOTM_MUTE: '1', ...o.env };
    await context.addInitScript(envInit, env);
    const page = await context.newPage();
    for (let t = 0; t < 60; t++) {   // the page server takes a moment
      try { await page.goto(`http://127.0.0.1:${port}/?WOTM_MATCH=${o.match}&WOTM_AIVSAI=1`); break; } catch { await sleep(500); }
    }
    console.log(`profiling Firefox: match ${o.match}, step ${o.step}, ${o.seconds} s; profile ${o.out}`);
    await sleep(o.seconds * 1000);
    lastLog = await page.evaluate(() => (window.wotmLog || []).slice(-5)).catch(() => []);
  } finally {
    await browser.close().catch(() => {});
    web.kill();
  }
  for (const l of lastLog) console.log(l.slice(0, 300));
  if (!existsSync(o.out)) { console.error(`no profile written at ${o.out}`); process.exit(1); }
  console.log(`profile: ${o.out} (${(statSync(o.out).size / 1e6).toFixed(1)} MB)`);
}

main().catch((e) => { console.error(e); process.exit(1); });
