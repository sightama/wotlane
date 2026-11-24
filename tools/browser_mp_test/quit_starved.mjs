// ABOUTME: Checks in real browsers that web/reload.js (QUIT, Leave the match) reloads a page whose timers never fire: setTimeout,
// ABOUTME: setInterval and requestAnimationFrame are disabled before the page loads, and the work before the reload never ends.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { REPO, launchBrowsers, launchFirefox, loadPlaywright } from './lib/infra.mjs';

const USAGE = `Usage: node tools/browser_mp_test/quit_starved.mjs [--browser chromium|chrome|firefox] [--hidden]
  Opens a blank page with reload.js, starves its timers and animation frames, and calls wotmReloadWhenDone(a promise that never
  settles). Passes when the page loads a second time within 5 s. --hidden also puts the tab behind another one first.
  Muted like every test run. Exit 1 on failure.`;

const argv = process.argv.slice(2);
if (argv.includes('-h') || argv.includes('--help')) { console.log(USAGE); process.exit(0); }
const arg = (name, fallback) => (argv.includes(name) ? argv[argv.indexOf(name) + 1] : fallback);
const browserName = arg('--browser', 'chromium');
const hidden = argv.includes('--hidden');

const STARVE = `
  window.setTimeout = () => 0;
  window.setInterval = () => 0;
  window.requestAnimationFrame = () => 0;
  sessionStorage.loads = String(Number(sessionStorage.loads || 0) + 1);
`;

const pw = await loadPlaywright();
const browser = browserName === 'firefox' ? await launchFirefox(pw, {}) : (await launchBrowsers(pw, { n: 1, browser: browserName }))[0];
let ok = false;
try {
  const context = await browser.newContext();
  await context.addInitScript(STARVE);
  await context.route('http://quit.test/**', (route) => route.fulfill({ contentType: 'text/html', body: '<!doctype html><title>quit</title><p>page' }));
  const page = await context.newPage();
  await page.goto('http://quit.test/');
  if (hidden) await (await context.newPage()).goto('http://quit.test/');
  await page.addScriptTag({ content: readFileSync(join(REPO, 'web/reload.js'), 'utf8') });
  const disabled = await page.evaluate(() => window.setTimeout(() => {}, 0) === 0 && window.requestAnimationFrame(() => {}) === 0);
  const loadsBefore = Number(await page.evaluate(() => sessionStorage.loads));
  await page.evaluate(() => { window.wotmReloadWhenDone(new Promise(() => {})); });
  const start = Date.now();
  let loads = loadsBefore;
  while (Date.now() - start < 5000 && loads === loadsBefore) {
    await new Promise((r) => setImmediate(r));
    loads = Number(await page.evaluate(() => sessionStorage.loads).catch(() => loadsBefore));
  }
  ok = loads > loadsBefore;
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${browserName}${hidden ? ' (tab behind another)' : ''}: timers ${disabled ? 'disabled' : 'NOT disabled'}, ` +
    `${ok ? `reloaded after ${Date.now() - start} ms` : 'no reload within 5 s'}`);
} finally {
  await browser.close();
}
process.exit(ok ? 0 : 1);
