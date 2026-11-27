// ABOUTME: Runs an AI vs AI fight in a real browser, muted, with the dust texture on the GPU and checked (WOTM_GPUDUST=1 WOTM_DUSTCHECK=1),
// ABOUTME: and summarizes the page's [dustcheck] lines: the GPU texture against the CPU raster of the same sprites, in that browser.

import { writeFileSync } from 'node:fs';
import { chromiumOptions, loadPlaywright, muted } from './lib/infra.mjs';

const USAGE = `Usage: node tools/browser_mp_test/dustcheck.mjs <chrome|firefox|brave> <page url> [secs] [--log <file>]
  page url  a served page (python3 tools/serve_web.py --site <dist> --data <disc folder>), e.g. http://localhost:8080/
  secs      how long to collect after the fight starts (default 40)
  --log     writes the page's whole log there
Prints the regenerations checked, the worst difference in levels and the texels over 2; exit 1 when a texel is over 2, no
[dustcheck] line came (the GPU path did not run) or the fight did not start within 180 s.`;

const args = process.argv.slice(2);
if (args.includes('-h') || args.includes('--help') || args.length < 2) { console.log(USAGE); process.exit(args.length < 2 ? 2 : 0); }
const li = args.indexOf('--log');
const logFile = li >= 0 ? args.splice(li, 2)[1] : null;
const [which, url, secs = '40'] = args;
if (!['chrome', 'firefox', 'brave'].includes(which)) { console.error(USAGE); process.exit(2); }

const fight = new URL(url);
for (const [k, v] of [['WOTM_MATCH', '3,1,1'], ['WOTM_AIVSAI', '1'], ['PS2X_STATS', '1'], ['WOTM_GPUDUST', '1'], ['WOTM_DUSTCHECK', '1']])
  fight.searchParams.set(k, v);

const pw = await loadPlaywright();
const browser = which === 'firefox' ? await pw.firefox.launch(muted('firefox', { headless: false }))
  : await pw.chromium.launch(chromiumOptions({ browser: which, headed: true }));
let status = 0;
try {
  const page = await (await browser.newContext({ viewport: { width: 1280, height: 720 } })).newPage();
  await page.goto(fight.href);
  const log = () => page.evaluate(() => window.wotmLog || []);
  const t0 = Date.now();
  while (!(await log()).some((l) => /\[native\] frame \d+ redraws=/.test(l))) {
    if (Date.now() - t0 > 180000) { console.error('no fight after 180 s; the log ends:\n' + (await log()).slice(-15).join('\n')); process.exit(1); }
    await page.waitForTimeout(1000);
  }
  await page.waitForTimeout(Number(secs) * 1000);
  const lines = await log();
  if (logFile) writeFileSync(logFile, lines.join('\n') + '\n');
  let n = 0, worst = 0, over = 0, failed = 0;
  for (const l of lines) {
    const m = /\[dustcheck\] .*worst (\d+) levels, (\d+) texels over 2( \(read back failed\))?/.exec(l);
    if (!m) continue;
    ++n; worst = Math.max(worst, +m[1]); over += +m[2]; failed += m[3] ? 1 : 0;
  }
  console.log(`${which}: ${n} regenerations checked, worst ${worst} levels, ${over} texels over 2${failed ? `, ${failed} read backs failed` : ''}`);
  if (!n || over || failed) status = 1;
} finally {
  await browser.close();
}
process.exit(status);
