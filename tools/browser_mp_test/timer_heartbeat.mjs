// ABOUTME: Measures how late setTimeout fires on each of several tabs in one Firefox or Chrome (the harness's layout: one context per
// ABOUTME: player, all driven at once), with a game-like load on every tab, to see which tabs get their timers throttled.
import { launchBrowsers, launchFirefox, loadPlaywright } from './lib/infra.mjs';

const USAGE = `Usage: node tools/browser_mp_test/timer_heartbeat.mjs [--browser chromium|chrome|firefox] [--tabs 2] [--secs 40] [--burn 10] [--focus-first] [--headed]
  Each tab runs a 100 ms setTimeout heartbeat next to a requestAnimationFrame loop that burns --burn ms a frame (default 10). Prints, per tab, the
  heartbeat's mean and max lateness and whether the tab was hidden. --focus-first brings tab 1 to the front when tab 2 opens.
  Muted like every test run.`;
const argv = process.argv.slice(2);
if (argv.includes('-h') || argv.includes('--help')) { console.log(USAGE); process.exit(0); }
const arg = (name, fallback) => (argv.includes(name) ? argv[argv.indexOf(name) + 1] : fallback);
const browserName = arg('--browser', 'firefox');
const tabs = Number(arg('--tabs', 2));
const secs = Number(arg('--secs', 40));
const burn = Number(arg('--burn', 10));

const PAGE = `<!doctype html><title>t</title><script>const BURN = ${burn};
  const late = []; let last = performance.now();
  (function beat() { setTimeout(() => { const n = performance.now(); late.push(n - last - 100); last = n; beat(); }, 100); })();
  (function burn() { requestAnimationFrame(() => { const t = performance.now(); while (performance.now() - t < BURN); burn(); }); })();
  window.report = () => ({ n: late.length, mean: late.reduce((a, b) => a + b, 0) / Math.max(1, late.length), max: Math.max(0, ...late), hidden: document.hidden });
</script>`;

const pw = await loadPlaywright();
const browser = browserName === 'firefox' ? await launchFirefox(pw, { headed: argv.includes('--headed') }) : (await launchBrowsers(pw, { n: 1, browser: browserName, headed: argv.includes('--headed') }))[0];
try {
  const pages = [];
  for (let i = 0; i < tabs; i++) {
    const context = await browser.newContext();
    await context.route('http://beat.test/**', (route) => route.fulfill({ contentType: 'text/html', body: PAGE }));
    const page = await context.newPage();
    await page.goto('http://beat.test/');
    if (argv.includes('--focus-first') && i === 1) await pages[0].bringToFront();
    pages.push(page);
  }
  await new Promise((r) => setTimeout(r, secs * 1000));
  for (const [i, page] of pages.entries()) {
    const r = await page.evaluate(() => window.report());
    console.log(`${browserName} tab ${i + 1}: ${r.n} beats, mean late ${r.mean.toFixed(1)} ms, max late ${r.max.toFixed(0)} ms, hidden ${r.hidden}`);
  }
} finally {
  await browser.close();
}
