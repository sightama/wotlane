// ABOUTME: Checks the front panel's accessibility and layout in a real browser (Chrome, Brave or Firefox, muted): serves a web/ folder,
// ABOUTME: runs a local wotm_server for a real lobby, prints a table of checks and saves screenshots. Exit 1 when a check fails.
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { existsSync, mkdirSync, openSync, readFileSync } from 'node:fs';
import { extname, join, resolve } from 'node:path';
import { REPO, loadPlaywright, freePort, chromiumOptions, firefoxOptions } from './browser_mp_test/lib/infra.mjs';

const USAGE = `usage: node tools/page_check.mjs [--browser chrome|brave|firefox|chromium] [--site <web folder>] [--out <folder>] [--label <name>]
  --site   the page to check (default: web/ in this repo); point it at a copy of an older web/ to see what the checks say before a fix
  --out    where screenshots go (default: a new folder under the OS temp dir); files are <label>-<browser>-<view>.png
  --label  prefix of the screenshot names (default: after)
  Needs wotm_server (sh tools/test_net.sh notest) and playwright (tools/browser_mp_test/node_modules or the global npm root).
  Every browser starts muted (no sound from a test run); the game itself is never started.`;

const arg = (name, fallback) => {
  const i = process.argv.indexOf(`--${name}`);
  return i < 0 ? fallback : process.argv[i + 1];
};
if (process.argv.includes('--help') || process.argv.includes('-h')) { console.log(USAGE); process.exit(0); }

const browserName = arg('browser', 'chrome');
const site = resolve(arg('site', join(REPO, 'web')));
const label = arg('label', 'after');
const out = arg('out', join(process.env.TMPDIR || '/tmp', `page_check_${Date.now()}`));
const server = join(REPO, 'native/net/build/wotm_server');
for (const [what, p] of [['site', join(site, 'index.html')], ['wotm_server', server]])
  if (!existsSync(p)) { console.error(`${what} not found at ${p}\n${USAGE}`); process.exit(2); }
mkdirSync(out, { recursive: true });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const children = [];
const [wsPort, tcpPort, webPort] = [await freePort(), await freePort(), await freePort()];
const log = openSync(join(out, 'server.log'), 'w');
children.push(spawn(server, ['--port', String(tcpPort), '--ws-port', String(wsPort), '--lobbies', '8'], { stdio: ['ignore', log, log] }));
// the page refuses to start without cross-origin isolation (web/support.js), so the files go out with the two headers the live site sends
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json', '.png': 'image/png', '.webmanifest': 'application/manifest+json' };
const files = createServer((req, res) => {
  const path = join(site, new URL(req.url, 'http://x').pathname.replace(/\/$/, '/index.html'));
  if (!path.startsWith(site) || !existsSync(path)) { res.writeHead(404).end(); return; }
  res.writeHead(200, { 'Content-Type': TYPES[extname(path)] ?? 'application/octet-stream', 'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp' });
  res.end(readFileSync(path));
}).listen(webPort, '127.0.0.1');
const pageUrl = `http://127.0.0.1:${webPort}/?server=ws://127.0.0.1:${wsPort}`;

const pw = await loadPlaywright();
const browser = await (browserName === 'firefox' ? pw.firefox.launch(firefoxOptions({}))
  : pw.chromium.launch(chromiumOptions({ browser: browserName })));

const rows = [];
const check = async (name, fn) => {
  try {
    const detail = await fn();
    rows.push({ name, ok: true, detail: detail ?? '' });
  } catch (e) {
    rows.push({ name, ok: false, detail: String(e.message).split('\n')[0] });
  }
};
const expect = (cond, msg) => { if (!cond) throw new Error(msg); };
const shot = (page, view) => page.screenshot({ path: join(out, `${label}-${browserName}-${view}.png`) });

async function openPage(viewport, opts = {}) {
  const ctx = await browser.newContext({ viewport, ...opts });
  const page = await ctx.newPage();
  await page.goto(pageUrl, { waitUntil: 'load' });
  await page.waitForSelector('#home:not([hidden])');
  return { ctx, page };
}

// ---- keyboard order and the game canvas
{
  const { ctx, page } = await openPage({ width: 1280, height: 800 });
  await check('canvas is not the first tab stop', async () => {
    await page.keyboard.press('Tab');
    const id = await page.evaluate(() => document.activeElement?.id);
    expect(id !== 'canvas', 'Tab landed on the game canvas first');
  });
  await check('first Tab stop is inside the front panel, not under it', async () => {
    const inFront = await page.evaluate(() => !!document.activeElement?.closest('#front'));
    expect(inFront, `first stop is ${await page.evaluate(() => document.activeElement?.id)}`);
  });
  await check('no positive tabindex anywhere', async () => {
    const n = await page.evaluate(() => [...document.querySelectorAll('[tabindex]')].filter((e) => Number(e.getAttribute('tabindex')) > 0).length);
    expect(n === 0, `${n} elements with a positive tabindex`);
  });
  await shot(page, 'home-desktop');
  await check('leaving the front panel makes the stage usable again', async () => {
    // Single player starts the game load; only the synchronous page change is read (this folder has no game build)
    expect(await page.evaluate(() => document.getElementById('stage').inert), 'the stage is not inert while the panel shows');
    await page.click('#single');
    await page.waitForFunction(() => document.getElementById('front').hidden, null, { timeout: 5000 });
    expect(!(await page.evaluate(() => document.getElementById('stage').inert)), 'the stage stayed inert after the panel closed');
  });
  await check('canvas takes focus from script', async () => {
    const focused = await page.evaluate(() => { const c = document.getElementById('canvas'); c.focus(); return document.activeElement === c; });
    expect(focused, 'canvas.focus() did not focus the canvas');
  });
  await ctx.close();
}

// ---- the lobby: rule checkboxes and chat
for (const [view, viewport, opts] of [['desktop', { width: 1280, height: 800 }, {}], ['phone', { width: 390, height: 844 }, { isMobile: true, hasTouch: true }]]) {
  const { ctx, page } = await openPage(viewport, opts);
  await page.fill('#name', 'Checker');
  await page.click('#host');
  await page.waitForSelector('#inLobby:not([hidden]) #code:not(:empty)', { timeout: 10000 });

  await check(`[${view}] each rule checkbox has exactly one label`, async () => {
    const counts = await page.evaluate(() => ['mod_jump', 'mod_fast', 'mod_turbo'].map((id) => document.getElementById(id).labels.length));
    expect(counts.every((n) => n === 1), `labels per checkbox: ${counts}`);
  });
  await check(`[${view}] checkboxes have the accessible names Jump, Fast, Turbo`, async () => {
    for (const name of ['Jump', 'Fast', 'Turbo'])
      expect(await page.getByRole('checkbox', { name, exact: true }).count() === 1, `no checkbox named ${name}`);
  });
  await check(`[${view}] clicking a rule's word toggles only that rule`, async () => {
    for (const [word, id] of [['Fast', 'mod_fast'], ['Turbo', 'mod_turbo']]) {
      await page.locator('label', { hasText: word }).click();
      const state = await page.evaluate(() => ['mod_jump', 'mod_fast', 'mod_turbo'].map((i) => document.getElementById(i).checked));
      expect(state.filter(Boolean).length === 1 && state[['mod_jump', 'mod_fast', 'mod_turbo'].indexOf(id)], `after clicking ${word}: ${state}`);
      await page.locator('label', { hasText: word }).click();
    }
  });
  await check(`[${view}] the rules form a group named Rules`, async () => {
    expect(await page.getByRole('group', { name: 'Rules' }).count() === 1, 'no group named Rules');
  });

  if (view === 'desktop') {
    const chatLines = () => page.locator('#chatLog li').count();
    const press = (init) => page.evaluate((i) => document.getElementById('chatText').dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true, ...i })), init);
    await check('chat: Enter that confirms an IME conversion does not send', async () => {
      await page.fill('#chatText', 'konnichiwa');
      await press({ isComposing: true, keyCode: 229 });
      await press({ isComposing: false, keyCode: 229 });   // Safari's order of events
      await sleep(400);
      expect(await page.inputValue('#chatText') === 'konnichiwa' && await chatLines() === 0, 'the half-composed line was sent');
    });
    await check('chat: a plain Enter sends', async () => {
      await page.focus('#chatText');
      await page.keyboard.press('Enter');
      await page.waitForFunction(() => document.querySelectorAll('#chatLog li').length === 1, null, { timeout: 5000 });
      expect(await page.inputValue('#chatText') === '', 'the field kept its text');
    });
  }
  await shot(page, `lobby-${view}`);
  await ctx.close();
}

await browser.close();
for (const c of children) c.kill('SIGTERM');
files.close();

const bad = rows.filter((r) => !r.ok);
console.log(`\n${browserName}  (${site})`);
for (const r of rows) console.log(`${r.ok ? 'PASS' : 'FAIL'}  ${r.name}${r.detail ? `  [${r.detail}]` : ''}`);
console.log(`\n${rows.length - bad.length}/${rows.length} passed; screenshots in ${out}`);
process.exit(bad.length ? 1 : 0);
