// ABOUTME: Takes the same set of screenshots of the front panel for any copy of web/ (Firefox, Chrome, Brave or WebKit, muted): home,
// ABOUTME: lobby with two players, settings, controls and the touch layout panel, on a phone and a desktop. Used for before and after.
import { join } from 'node:path';
import { REPO } from './browser_mp_test/lib/infra.mjs';
import { BROWSERS, argv, launchBrowser, sleep, startPageEnv } from './page_env.mjs';

const USAGE = `usage: node tools/page_shots.mjs --out <folder> [--browser firefox|chrome|brave|chromium|webkit] [--site <web folder>] [--label <name>]
  --out      where the PNGs go (created); files are <label>-<browser>-<device>-<view>.png
  --site     the page to shoot (default: web/ in this repo); pass an older copy of web/ for the "before" set
  --label    name prefix (default: shot)
  Views: home, lobby (host with one guest), settings, controls (settings tab), and on the phone held sideways the touch layout panel
  and its Move buttons mode. WebKit shoots the home view only, and the game's "can't run here" message on an iPhone.
  Needs wotm_server (sh tools/test_net.sh notest) and playwright. Chrome, Brave and Firefox start muted; WebKit has no mute switch and
  nothing here makes a sound: the game is never started.`;

const arg = argv(USAGE);
const out = arg('out');
if (!out) { console.error(USAGE); process.exit(2); }
const browserName = arg('browser', 'firefox');
if (!BROWSERS.includes(browserName)) { console.error(`unknown browser ${browserName}\n${USAGE}`); process.exit(2); }
const label = arg('label', 'shot');
const env = await startPageEnv({ site: arg('site', join(REPO, 'web')), out: join(out, '.server') }).catch((e) => { console.error(`${e.message}\n${USAGE}`); process.exit(2); });
const browser = await launchBrowser(browserName);
const pw = await (await import('./browser_mp_test/lib/infra.mjs')).loadPlaywright();

const DEVICES = {
  phone: { viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: browserName !== 'firefox', hasTouch: true },
  desktop: { viewport: { width: 1280, height: 800 }, deviceScaleFactor: 1 },
};
const file = (device, view) => join(out, `${label}-${browserName}-${device}-${view}.png`);
const shot = async (page, device, view, opts = {}) => { await sleep(300); await page.screenshot({ path: file(device, view), ...opts }); };

async function open(ctxOpts, name) {
  const ctx = await browser.newContext(ctxOpts);
  const page = await ctx.newPage();
  await page.goto(env.pageUrl, { waitUntil: 'load' });
  await page.waitForSelector('#home:not([hidden])');
  if (browserName !== 'webkit') await page.waitForFunction(() => !document.getElementById('single').disabled);   // WebKit's page says the game cannot run, so the buttons stay off
  if (name) await page.fill('#name', name);
  return { ctx, page };
}

if (browserName === 'webkit') {
  // Safari desktop, then an iPhone: the page and the message that says the game cannot run here
  const d = await open(DEVICES.desktop);
  await shot(d.page, 'desktop', 'home');
  await d.ctx.close();
  const iphone = pw.devices['iPhone 13'];
  const ctx = await browser.newContext({ ...iphone });
  const page = await ctx.newPage();
  await page.goto(env.pageUrl, { waitUntil: 'load' });
  await page.waitForSelector('#home:not([hidden])');
  await shot(page, 'phone', 'home');
  console.log(`iPhone message: ${await page.locator('#frontMsg').innerText()}`);
  await ctx.close();
} else {
  for (const device of ['phone', 'desktop']) {
    const { ctx, page } = await open(DEVICES[device]);
    await shot(page, device, 'home');
    await page.fill('#name', 'Lane');
    await page.click('#host');
    await page.waitForSelector('#inLobby:not([hidden]) #code:not(:empty)', { timeout: 10000 });
    const code = (await page.locator('#code').innerText()).trim();
    const guest = await open({ viewport: { width: 1280, height: 800 } }, 'Guest2');
    await guest.page.fill('#joinCode', code);
    await guest.page.click('#join');
    await guest.page.waitForSelector('#inLobby:not([hidden])', { timeout: 10000 });
    await page.waitForFunction(() => document.querySelectorAll('#members li').length >= 2, null, { timeout: 10000 });
    await page.locator('#mod_fast').check();
    await sleep(500);
    await shot(page, device, 'lobby', { fullPage: true });
    await guest.ctx.close();
    await page.click('#leave');
    await page.click('#openSettings');
    await page.waitForSelector('#settings:not([hidden])');
    await shot(page, device, 'settings', { fullPage: true });
    await page.click('#tab-controls');
    await shot(page, device, 'controls', { fullPage: true });
    await ctx.close();
  }
  // the touch layout: a phone held sideways, the gear panel, then the Move buttons mode (taps only: a mouse click turns the controls off)
  const { ctx, page } = await open({ ...DEVICES.phone, viewport: { width: 844, height: 390 } });
  await page.locator('#single').tap();
  await page.waitForSelector('#touchpad.on', { timeout: 5000 });
  await shot(page, 'phone', 'touch-controls');
  await page.locator('#touchpad [data-id=gear]').tap();
  await page.waitForSelector('#tpPanel:not([hidden])');
  await shot(page, 'phone', 'touch-layout');
  await page.getByRole('button', { name: 'Move buttons' }).tap();
  await shot(page, 'phone', 'touch-layout-moving');
  await ctx.close();
}

await browser.close();
env.stop();
console.log(`screenshots in ${out}`);
