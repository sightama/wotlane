// ABOUTME: Checks the page's game input handling in a real Chromium and Firefox (web/gameinput.js): Tab does not move focus once the
// ABOUTME: game runs, and the mouse cursor over the game hides after 2 s idle and shows on the next move. No game is loaded.
// Run: node tools/browser_input_test/run.mjs [--browser chromium|firefox|all]   (Playwright: see tools/browser_mp_test/README.md)
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { readFileSync, existsSync, statSync } from 'node:fs';
import { dirname, join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repo = join(here, '..', '..');
const require = createRequire(import.meta.url);
let playwright;
try { playwright = require('playwright'); } catch { playwright = require(join(execFileSync('npm', ['root', '-g']).toString().trim(), 'playwright')); }

const arg = process.argv.indexOf('--browser');
const which = arg > 0 ? process.argv[arg + 1] : 'chromium';
const browsers = which === 'all' ? ['chromium', 'firefox'] : [which];

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json' };
const site = createServer((req, res) => {
  const path = new URL(req.url, 'http://x').pathname;
  const file = join(repo, 'web', path === '/' ? 'index.html' : path);
  const ok = file.startsWith(join(repo, 'web')) && existsSync(file) && statSync(file).isFile();
  res.writeHead(ok ? 200 : 404, { 'content-type': MIME[extname(file)] ?? 'application/octet-stream', 'cache-control': 'no-store' });
  res.end(ok ? readFileSync(file) : 'not found');
});
await new Promise((resolve) => site.listen(0, '127.0.0.1', resolve));

let failures = 0;
const check = (name, ok, detail = '') => { console.log(`${ok ? 'ok     ' : 'FAILED '} ${name}${ok ? '' : ': ' + detail}`); if (!ok) ++failures; };

async function run(name) {
  const browser = await playwright[name].launch();
  try {
    const page = await (await browser.newContext({ viewport: { width: 900, height: 600 } })).newPage();
    await page.goto(`http://127.0.0.1:${site.address().port}/`);
    check(`${name}: the page loads the game input script`, await page.evaluate(() => typeof window.wotmInstallGameInput === 'function'));
    // the front panel is up: Tab moves between its controls
    await page.focus('#name');
    await page.keyboard.press('Tab');
    check(`${name}: on the front panel Tab moves focus`, await page.evaluate(() => document.activeElement.id !== 'name'));
    // the game runs: the panel is gone and Tab is the game's
    await page.evaluate(() => {
      document.getElementById('front').hidden = true;
      const canvas = document.getElementById('canvas');
      window.tabEvents = [];
      window.addEventListener('keydown', (e) => { if (e.key === 'Tab') window.tabEvents.push(e.defaultPrevented); });   // after gameinput's capture listener
      window.wotmStart(null, 'test');   // the page's own start (boot.js); the game's files are not there, which does not matter here
      canvas.tabIndex = 0;
      canvas.focus();
    });
    await page.keyboard.press('Tab');
    check(`${name}: with the game running Tab does not move focus`, await page.evaluate(() => document.activeElement.id === 'canvas'));
    check(`${name}: and the page saw it prevented`, await page.evaluate(() => window.tabEvents.at(-1) === true));
    // the cursor: hides after 2 s without a move
    const box = await page.locator('#canvas').boundingBox();
    const cursor = () => page.evaluate(() => document.getElementById('canvas').style.cursor);
    await page.mouse.move(box.x + 50, box.y + 50);
    await page.mouse.move(box.x + 60, box.y + 60);
    check(`${name}: the cursor shows while the mouse moves`, (await cursor()) === '');
    await page.waitForTimeout(2400);
    check(`${name}: the cursor hides after 2 s idle`, (await cursor()) === 'none');
    await page.mouse.move(box.x + 80, box.y + 80);
    check(`${name}: and shows again on a move`, (await cursor()) === '');
  } finally {
    await browser.close();
  }
}

try {
  for (const b of browsers) await run(b);
} catch (e) {
  check('the run', false, e.stack);
} finally {
  site.close();
}
console.log(failures ? `${failures} FAILED` : 'all ok');
process.exit(failures ? 1 : 0);
