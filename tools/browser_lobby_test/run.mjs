// ABOUTME: Checks the lobby's member list end to end in a real browser with three players on a local wotm_server: each member's ping
// ABOUTME: and device health chips, the warning, and the owner's Remove (with the ban) and Make owner. No game is loaded.
// Run: node tools/browser_lobby_test/run.mjs [--browser chromium|firefox|all]   (needs sh tools/test_net.sh notest first, and Playwright:
// see tools/browser_mp_test/README.md)
import { spawn, execFileSync } from 'node:child_process';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
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

const server = join(repo, 'native/net/build/wotm_server');
if (!existsSync(server)) throw new Error('build the server first: sh tools/test_net.sh notest');

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json' };
const site = createServer((req, res) => {
  const path = new URL(req.url, 'http://x').pathname;
  const file = join(repo, 'web', path === '/' ? 'index.html' : path);
  const ok = file.startsWith(join(repo, 'web')) && existsSync(file) && statSync(file).isFile();
  res.writeHead(ok ? 200 : 404, { 'content-type': MIME[extname(file)] ?? 'application/octet-stream', 'cross-origin-opener-policy': 'same-origin',
    'cross-origin-embedder-policy': 'require-corp', 'cache-control': 'no-store' });
  res.end(ok ? readFileSync(file) : 'not found');
});
await new Promise((resolve) => site.listen(0, '127.0.0.1', resolve));
const sitePort = site.address().port;

const ports = { tcp: 24000 + Math.floor(Math.random() * 2000), ws: 26000 + Math.floor(Math.random() * 2000) };
const srv = spawn(server, ['--port', String(ports.tcp), '--ws-port', String(ports.ws), '--lobbies', '2'], { stdio: 'ignore' });
await new Promise((resolve) => setTimeout(resolve, 600));

let failures = 0;
const check = (name, ok, detail = '') => { console.log(`${ok ? 'ok     ' : 'FAILED '} ${name}${ok ? '' : ': ' + detail}`); if (!ok) ++failures; };

const row = (page, name) => page.locator(`#members li[data-name="${name}"]`);
const chip = (page, name, kind) => row(page, name).locator('.' + kind);
const text = (locator) => locator.textContent({ timeout: 8000 });
const waitFor = async (page, fn, arg, ms = 10000) => { try { await page.waitForFunction(fn, arg, { timeout: ms }); return true; } catch { return false; } };

// `brave`: the page answers navigator.brave.isBrave() with true, as Brave does
async function player(browser, name, health, brave = false) {
  const context = await browser.newContext();
  if (brave) await context.addInitScript(() => Object.defineProperty(navigator, 'brave', { value: { isBrave: async () => true } }));
  if (health) await context.addInitScript((h) => { try { localStorage.setItem('wotlane.deviceHealth', JSON.stringify(h)); } catch { /* none */ } }, health);
  const page = await context.newPage();
  await page.goto(`http://127.0.0.1:${sitePort}/?server=ws://127.0.0.1:${ports.ws}`);
  await page.fill('#name', name);
  return page;
}

async function run(browserName) {
  const tag = (s) => `${browserName}: ${s}`;
  const browser = await playwright[browserName].launch();
  try {
    const ana = await player(browser, 'Ana');
    const bia = await player(browser, 'Bia', { fps: 27, workMs: 13.2, hz: 60 });
    const cai = await player(browser, 'Cai', { fps: 60, workMs: 5, hz: 60 }, true);
    await ana.click('#host');
    await ana.waitForSelector('#inLobby:not([hidden])');
    const code = (await text(ana.locator('#code'))).trim();
    for (const p of [bia, cai]) {
      await p.fill('#joinCode', code);
      await p.click('#join');
      await p.waitForSelector('#inLobby:not([hidden])');
    }
    const all = [ana, bia, cai];

    const listed = await Promise.all(all.map((p) => waitFor(p, () => document.querySelectorAll('#members li').length === 3)));
    check(tag('every tab lists the three members'), listed.every(Boolean));

    const pinged = await Promise.all(all.map((p) => waitFor(p, () => [...document.querySelectorAll('#members .ping')].every((c) => /^\d+ ms$/.test(c.textContent)), null, 12000)));
    check(tag('every tab shows every member\'s ping in ms'), pinged.every(Boolean));
    const green = await Promise.all(all.map(async (p) => (await chip(p, 'Bia', 'ping').getAttribute('class')).includes('good')));
    check(tag('a local ping is green'), green.every(Boolean));

    const healthShown = await Promise.all(all.map((p) => waitFor(p, () => document.querySelector('#members li[data-name="Bia"] .device')?.textContent.startsWith('device too slow'), null, 8000)));
    check(tag('every tab shows Bia\'s device as too slow'), healthShown.every(Boolean));
    check(tag('Bia\'s device chip is red with her numbers'), (await text(chip(cai, 'Bia', 'device'))) === 'device too slow: 27 fps of 60' &&
      (await chip(cai, 'Bia', 'device').getAttribute('class')).includes('bad'));
    check(tag('Cai\'s device chip is green'), (await text(chip(ana, 'Cai', 'device'))) === 'device ok' && (await chip(ana, 'Cai', 'device').getAttribute('class')).includes('good'));
    check(tag('Ana, never measured, says so'), (await text(chip(bia, 'Ana', 'device'))) === 'device not measured yet');
    const warned = await Promise.all(all.map(async (p) => (await text(p.locator('#lobbyWarnings'))).includes('Bia: this device runs 27 fps of 60')));
    check(tag('every tab warns about Bia\'s device'), warned.every(Boolean));

    const buttons = (p, name) => row(p, name).locator('button').allTextContents();
    check(tag('the owner has Make owner and Remove on the others, none on themself'),
      JSON.stringify(await buttons(ana, 'Bia')) === '["Make owner","Remove"]' && (await buttons(ana, 'Ana')).length === 0);
    check(tag('a guest has no owner buttons'), (await buttons(bia, 'Cai')).length === 0 && (await buttons(cai, 'Ana')).length === 0);

    // Brave: Cai's own screen carries the notice, nobody else's does, and the owner (only) sees Cai's browser marked
    const notice = (p) => p.locator('#deviceNote');
    check(tag('a Brave player\'s own screen shows the Shields notice'), await waitFor(cai, () => !document.getElementById('deviceNote').hidden) &&
      (await text(notice(cai))) === "Brave's Shields can slow this game a lot. For the best experience use Chrome, Edge or Firefox, or turn Shields off for playwotm.com and enable graphics acceleration.");
    check(tag('another browser shows no notice'), (await notice(ana).isHidden()) && (await notice(bia).isHidden()));
    check(tag('the owner sees Cai\'s Brave hint'), await waitFor(ana, () => document.querySelector('#members li[data-name="Cai"] .browser')?.textContent === 'Brave', null, 10000));
    check(tag('and no hint on a browser that is not Brave'), (await text(chip(ana, 'Bia', 'browser'))) === '');
    check(tag('a guest sees no Brave hint'), (await text(chip(bia, 'Cai', 'browser'))) === '');

    // Remove asks twice; one click only arms it
    await row(ana, 'Bia').getByRole('button', { name: 'Remove' }).click();
    check(tag('Remove asks again before it acts'), (await buttons(ana, 'Bia')).includes('Sure?') && (await cai.locator('#members li').count()) === 3);
    await row(ana, 'Bia').getByRole('button', { name: 'Sure?' }).first().click();
    check(tag('the removed player is back on the Host/Join choice with the reason'),
      await waitFor(bia, () => !document.getElementById('choose').hidden && /removed you from the lobby. You can join it again in \d+ seconds/.test(document.getElementById('frontMsg').textContent)));
    check(tag('the others see the member go'), (await Promise.all([ana, cai].map((p) => waitFor(p, () => document.querySelectorAll('#members li').length === 2)))).every(Boolean));
    check(tag('the warning about the removed player goes with them'), (await text(ana.locator('#lobbyWarnings'))) === '');

    await bia.fill('#joinCode', code);
    await bia.click('#join');
    check(tag('the removed player can\'t rejoin at once'),
      await waitFor(bia, () => document.getElementById('frontMsg').textContent.includes('from that lobby. You can join it again')) && (await ana.locator('#members li').count()) === 2);

    // Make owner
    await row(ana, 'Cai').getByRole('button', { name: 'Make owner' }).click();
    await row(ana, 'Cai').getByRole('button', { name: 'Sure?' }).first().click();
    check(tag('the new owner gets the lobby controls'), await waitFor(cai, () => !document.getElementById('ownerOnly').hidden));
    check(tag('the old owner loses the controls and the buttons'), await waitFor(ana, () => document.getElementById('ownerOnly').hidden &&
      document.querySelectorAll('#members button').length === 0));
    check(tag('the new owner has the buttons, with Ana now removable'), await waitFor(cai, () => document.querySelectorAll('#members li[data-name="Ana"] button').length === 2));
    check(tag('after the handoff the new owner sees Ana\'s row without a Brave chip and the old owner no chip at all'),
      (await text(chip(cai, 'Ana', 'browser'))) === '' && (await text(chip(ana, 'Cai', 'browser'))) === '');
    check(tag('the old owner is told'), (await text(ana.locator('#frontMsg'))).includes('Cai is the owner now'));
  } finally {
    await browser.close();
  }
}

try {
  for (const b of browsers) await run(b);
} catch (e) {
  check('the run', false, e.stack);
} finally {
  srv.kill();
  site.close();
}
console.log(failures ? `${failures} FAILED` : 'all ok');
process.exit(failures ? 1 : 0);
