// ABOUTME: Profiles the browser game's main thread in a direct AI fight: presented fps and the page's work per frame, then a CPU profile
// ABOUTME: of a few seconds with the functions that took the most self time. Usage: node tools/web_profile.mjs <chrome|brave> <page url> [secs]
import { loadPlaywright } from './browser_mp_test/lib/infra.mjs';

const [which = 'chrome', url, secs = '8'] = process.argv.slice(2);
if (!url) { console.error('usage: node tools/web_profile.mjs <chrome|brave> <page url> [secs]\n  brave: WOTM_BRAVE names its binary (default the macOS app); WOTM_BROWSER_ARGS: more browser flags (e.g. --disable-gpu)'); process.exit(2); }
const pw = await loadPlaywright();
const opts = { headless: false, args: ['--autoplay-policy=no-user-gesture-required', ...(process.env.WOTM_BROWSER_ARGS ?? '').split(' ').filter(Boolean)] };
if (which === 'brave') opts.executablePath = process.env.WOTM_BRAVE || '/Applications/Brave Browser.app/Contents/MacOS/Brave Browser';
else opts.channel = 'chrome';
const browser = await pw.chromium.launch(opts);
const page = await (await browser.newContext({ viewport: { width: 1280, height: 720 } })).newPage();
const fight = new URL(url);
fight.searchParams.set('WOTM_MATCH', '3,1,1');
fight.searchParams.set('WOTM_AIVSAI', '1');
await page.goto(fight.href);
const log = () => page.evaluate(() => window.wotmLog || []);
const t0 = Date.now();
while (!(await log()).some((l) => l.includes('ready: stage on screen'))) {
  if (Date.now() - t0 > 120000) { console.error('no stage after 120 s'); await browser.close(); process.exit(1); }
  await page.waitForTimeout(500);
}
await page.waitForTimeout(5000);
const counters = () => page.evaluate(() => ({ t: performance.now(), frames: window.wotmFrames?.frames ?? 0, workMs: window.wotmFrames?.workMs ?? 0 }));
const report = (a, b, what) => console.log(`${what}: presented ${((b.frames - a.frames) / ((b.t - a.t) / 1000)).toFixed(1)} fps, page work ${((b.workMs - a.workMs) / Math.max(1, b.frames - a.frames)).toFixed(2)} ms per present`);
const a = await counters();
await page.waitForTimeout(Number(secs) * 1000);
const b = await counters();
report(a, b, `${which} ${await page.evaluate(() => navigator.userAgent.match(/(Chrome|Firefox)\/\d+/)?.[0] ?? 'unknown')}`);
console.log('page sees:', JSON.stringify(await page.evaluate(() => { const gl = document.createElement('canvas').getContext('webgl2'); const ext = gl?.getExtension('WEBGL_debug_renderer_info'); const renderer = ext ? gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) : gl?.getParameter(gl.RENDERER); gl?.getExtension('WEBGL_lose_context')?.loseContext(); return { cores: navigator.hardwareConcurrency, renderer, timerRes: (() => { let m = 1e9, p = performance.now(); for (let i = 0; i < 1e5; i++) { const q = performance.now(); if (q > p) { m = Math.min(m, q - p); p = q; } } return m; })() }; })));

const cdp = await page.context().newCDPSession(page);
await cdp.send('Profiler.enable');
await cdp.send('Profiler.start');
await page.waitForTimeout(Number(secs) * 1000);
const { profile } = await cdp.send('Profiler.stop');
const self = new Map();
const dt = profile.timeDeltas;
const byId = new Map(profile.nodes.map((n) => [n.id, n]));
profile.samples.forEach((id, i) => {
  const f = byId.get(id).callFrame;
  const key = `${f.functionName || '(anonymous)'} ${f.url.split('/').pop()}:${f.lineNumber + 1}`;
  self.set(key, (self.get(key) ?? 0) + (dt[i] ?? 0) / 1000);
});
const total = [...self.values()].reduce((s, x) => s + x, 0);
console.log(`profile ${(total / 1000).toFixed(1)} s; top self time:`);
for (const [k, ms] of [...self].sort((x, y) => y[1] - x[1]).slice(0, 15)) console.log(`  ${ms.toFixed(0).padStart(7)} ms ${(100 * ms / total).toFixed(1).padStart(5)}%  ${k}`);
await browser.close();
