// ABOUTME: Measures the browser game's frame rate in an AI vs AI fight: presented frames per second, frame times (median, p95, p99, late
// ABOUTME: frames), the game's redraws and interpolated redraws, and a screenshot. Usage: node tools/web_fps.mjs <chrome|firefox> <page url> [secs]
import { writeFileSync } from 'node:fs';
import { loadavg } from 'node:os';
import { loadPlaywright } from './browser_mp_test/lib/infra.mjs';

const pw = await loadPlaywright();

const [which = 'chrome', url, secs = '20'] = process.argv.slice(2);
if (!url) { console.error('usage: node tools/web_fps.mjs <chrome|firefox> <page url> [secs]'); process.exit(2); }
const fight = new URL(url);
fight.searchParams.set('WOTM_MATCH', '3,1,1');   // a stage-3 fight, monsters 1 and 1, at once
fight.searchParams.set('WOTM_AIVSAI', '1');
fight.searchParams.set('PS2X_STATS', '1');

const browser = which === 'firefox' ? await pw.firefox.launch({ headless: false })
  : await pw.chromium.launch({ channel: 'chrome', headless: false, args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await (await browser.newContext({ viewport: { width: 1280, height: 720 } })).newPage();
await page.goto(fight.href);
const log = () => page.evaluate(() => window.wotmLog || []);
const t0 = Date.now();
while (!(await log()).some((l) => /\[native\] frame \d+ redraws=/.test(l))) {
  if (Date.now() - t0 > 180000) { console.error('no fight after 180 s; the log ends:\n' + (await log()).slice(-15).join('\n')); process.exit(1); }
  await page.waitForTimeout(1000);
}
await page.waitForTimeout(5000);   // past the fight's intro

// the page's own animation frames over the window (the game presents once per animation frame) and the game's counters
const stats = (lines) => {
  const s = lines.filter((l) => /\[native\] frame \d+ redraws=/.test(l)).at(-1);
  const m = s && /redraws=(\d+) blended=(\d+) t=([\d.]+)/.exec(s);
  return m ? { redraws: +m[1], blended: +m[2], t: +m[3] } : null;
};
const before = { native: stats(await log()), frames: await page.evaluate(() => ({ ...globalThis.wotmFrames })) };
const times = await page.evaluate((ms) => new Promise((resolve) => {
  const t = [];
  const end = performance.now() + ms;
  const step = (now) => { t.push(now); if (now < end) requestAnimationFrame(step); else resolve(t); };
  requestAnimationFrame(step);
}), Number(secs) * 1000);
const after = { native: stats(await log()), frames: await page.evaluate(() => ({ ...globalThis.wotmFrames })) };

const d = times.slice(1).map((t, i) => t - times[i]).sort((a, b) => a - b);
const q = (p) => d[Math.min(d.length - 1, Math.floor(p * d.length))];
const med = q(0.5);
const presented = after.frames.frames - before.frames.frames;
const workMs = (after.frames.workMs - before.frames.workMs) / presented;
const wall = (times.at(-1) - times[0]) / 1000;
const nat = before.native && after.native && after.native.t > before.native.t ? after.native : null;
const per = (k) => (nat ? ((after.native[k] - before.native[k]) / (after.native.t - before.native.t)).toFixed(1) : '?');
const shot = `web_fps_${which}_${Date.now()}.png`;
await page.screenshot({ path: shot });
const lines = await log();
const pick = (re) => lines.filter((l) => re.test(l)).map((l) => l.trim());
const report = [
  `${which} ${browser.version()} ${fight.href}; load average ${loadavg().map((x) => x.toFixed(1)).join(' ')}`,
  `display: ${pick(/\[web\] display|\[native\] present:|\[native\] canvas/).join(' | ')}`,
  `presented ${(presented / wall).toFixed(1)} fps; frame time median ${med.toFixed(2)} ms, p95 ${q(0.95).toFixed(2)}, p99 ${q(0.99).toFixed(2)}, ` +
    `late (over 1.5x the median) ${d.filter((x) => x > 1.5 * med).length} of ${d.length}; game loop work ${workMs.toFixed(2)} ms per frame`,
  `game: redraws ${per('redraws')}/s, interpolated ${per('blended')}/s; ${pick(/\[interp\]/).slice(-2).join(' | ')}`,
  `screenshot ${shot}`,
];
console.log(report.join('\n'));
if (process.env.DUMP) writeFileSync(process.env.DUMP, lines.join('\n'));
await browser.close();
