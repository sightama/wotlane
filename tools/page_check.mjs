// ABOUTME: Checks the front panel's accessibility and layout in a real browser (Chrome, Brave or Firefox, muted): serves a web/ folder,
// ABOUTME: runs a local wotm_server for a real lobby, prints a table of checks and saves screenshots. Exit 1 when a check fails.
import { join } from 'node:path';
import { REPO, loadPlaywright } from './browser_mp_test/lib/infra.mjs';
import { BROWSERS, argv, launchBrowser, showLoadingScreen, sleep, startPageEnv } from './page_env.mjs';

const USAGE = `usage: node tools/page_check.mjs [--browser chrome|brave|chromium|firefox|webkit] [--site <web folder>] [--out <folder>] [--label <name>]
  --site   the page to check (default: web/ in this repo); point it at a copy of an older web/ to see what the checks say before a fix
  --out    where screenshots go (default: a new folder under the OS temp dir); files are <label>-<browser>-<view>.png
  --label  prefix of the screenshot names (default: after)
  Needs wotm_server (sh tools/test_net.sh notest) and playwright (tools/browser_mp_test/node_modules or the global npm root).
  Chrome, Brave and Firefox start muted; WebKit has no mute switch, and nothing here makes a sound: the game is never started.`;

const arg = argv(USAGE);
const browserName = arg('browser', 'chrome');
if (!BROWSERS.includes(browserName)) { console.error(`unknown browser ${browserName}\n${USAGE}`); process.exit(2); }
const label = arg('label', 'after');
const out = arg('out', join(process.env.TMPDIR || '/tmp', `page_check_${Date.now()}`));
const env = await startPageEnv({ site: arg('site', join(REPO, 'web')), out }).catch((e) => { console.error(`${e.message}\n${USAGE}`); process.exit(2); });
const { pageUrl } = env;
const site = arg('site', join(REPO, 'web'));
const browser = await launchBrowser(browserName);

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

async function finish() {
  await browser.close();
  env.stop();

  const bad = rows.filter((r) => !r.ok);
  console.log(`\n${browserName}  (${site})`);
  for (const r of rows) console.log(`${r.ok ? 'PASS' : 'FAIL'}  ${r.name}${r.detail ? `  [${r.detail}]` : ''}`);
  console.log(`\n${rows.length - bad.length}/${rows.length} passed; screenshots in ${out}`);
  process.exit(bad.length ? 1 : 0);
}

// `refuseFullscreen`: the browser turns down requestFullscreen, as a phone may; the first tap on a button asks for it (web/touch.js), and
// where it is granted Firefox's window becomes as tall as the screen, which hides a panel that runs past a short phone
async function openPage(viewport, opts = {}, { refuseFullscreen = false } = {}) {
  const ctx = await browser.newContext({ viewport, ...opts });
  const page = await ctx.newPage();
  if (refuseFullscreen) await page.addInitScript(() => { Element.prototype.requestFullscreen = () => Promise.reject(new TypeError('fullscreen refused')); });
  await page.goto(pageUrl, { waitUntil: 'load' });
  await page.waitForSelector('#home:not([hidden])');
  return { ctx, page };
}

// ---- Safari (WebKit): the game cannot run there, so the page must say so and stay tidy
if (browserName === 'webkit') {
  const pw = await loadPlaywright();
  for (const [view, ctxOpts, wanted] of [['Safari on a Mac', { viewport: { width: 1280, height: 800 } }, /Chrome, Firefox or Edge/], ['Safari on an iPhone', { ...pw.devices['iPhone 13'] }, /iPhone or iPad/]]) {
    const ctx = await browser.newContext(ctxOpts);
    const page = await ctx.newPage();
    await page.goto(pageUrl, { waitUntil: 'load' });
    await page.waitForSelector('#home:not([hidden])');
    await sleep(800);
    await check(`[${view}] the page says why the game cannot run`, async () => {
      const msg = await page.locator('#frontMsg').innerText();
      expect(wanted.test(msg), `message: ${msg}`);
      return msg.slice(0, 60);
    });
    await check(`[${view}] nothing is offered that cannot work (no name, monster, play or settings controls)`, async () => {
      const shown = await page.evaluate(() => ['name', 'monster', 'single', 'host', 'join', 'openSettings'].filter((id) => document.getElementById(id).offsetParent !== null));
      expect(shown.length === 0, `still shown: ${shown}`);
    });
    await check(`[${view}] one main landmark, and nothing runs off the screen sideways`, async () => {
      expect(await page.getByRole('main').count() === 1, 'no single main landmark');
      const w = await page.evaluate(() => [document.documentElement.scrollWidth, innerWidth]);
      expect(w[0] <= w[1], `page is ${w[0]}px wide in a ${w[1]}px screen`);
    });
    await check(`[${view}] the message is readable (4.5:1)`, async () => {
      const r = await page.evaluate(() => {
        const parse = (c) => c.match(/[\d.]+/g).slice(0, 3).map(Number);
        const lum = (rgb) => { const [r, g, b] = rgb.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
        const [hi, lo] = [lum(parse(getComputedStyle(document.getElementById('frontMsg')).color)), lum(parse(getComputedStyle(document.getElementById('home')).backgroundColor))].sort((a, b) => b - a);
        return (hi + 0.05) / (lo + 0.05);
      });
      expect(r >= 4.5, `${r.toFixed(2)}:1`);
    });
    await page.screenshot({ path: join(out, `${label}-webkit-${view.includes('iPhone') ? 'phone' : 'desktop'}-home.png`), fullPage: true });
    await ctx.close();
  }
  await finish();
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

// ---- colour contrast, as the browser computes it
{
  const { ctx, page } = await openPage({ width: 1280, height: 800 });
  const ratios = () => page.evaluate(() => {
    const parse = (c) => c.match(/[\d.]+/g).slice(0, 3).map(Number);
    const lum = (rgb) => { const [r, g, b] = rgb.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
    const ratio = (a, b) => { const [hi, lo] = [lum(a), lum(b)].sort((x, y) => y - x); return (hi + 0.05) / (lo + 0.05); };
    const css = (el, prop, pseudo) => getComputedStyle(el, pseudo)[prop];
    const card = css(document.getElementById('home'), 'backgroundColor');
    const field = document.getElementById('joinCode');
    return {
      'label text on the panel': ratio(parse(css(document.querySelector('#home label'), 'color')), parse(card)),
      'footer text on the panel': ratio(parse(css(document.getElementById('legal'), 'color')), parse(card)),
      'code placeholder on its field': ratio(parse(css(field, 'color', '::placeholder')), parse(css(field, 'backgroundColor'))),
      'white on the Single player button': ratio(parse(css(document.getElementById('single'), 'color')), parse(css(document.getElementById('single'), 'backgroundColor'))),
    };
  });
  await check('label, footer, placeholder and main button text are at least 4.5:1', async () => {
    const low = Object.entries(await ratios()).filter(([, r]) => r < 4.5).map(([k, r]) => `${k} ${r.toFixed(2)}`);
    expect(low.length === 0, low.join('; '));
  });
  await ctx.close();
}

// ---- target sizes: a phone (coarse pointer) gets 44px, a mouse at least 24px, on the panel's own views
const targets = (page, min) => page.evaluate((m) => {
  const sel = '#front button, #front select, #front input:not([type=checkbox]), #rules label';
  return [...document.querySelectorAll(sel)].filter((e) => e.offsetParent !== null && !e.disabled && e.type !== 'hidden')
    .map((e) => [e.id || e.textContent.trim().slice(0, 14) || e.tagName, e.getBoundingClientRect().height, e.tagName === 'BUTTON' ? e.getBoundingClientRect().width : m])
    .filter(([, h, w]) => h < m - 0.5 || w < m - 0.5).map(([n, h, w]) => `${n} ${Math.round(w)}x${Math.round(h)}px`);
}, min);
for (const [view, viewport, opts, min] of [['phone', { width: 390, height: 844 }, { isMobile: true, hasTouch: true }, 44], ['desktop', { width: 1280, height: 800 }, {}, 24]]) {
  const { ctx, page } = await openPage(viewport, opts);
  await check(`[${view}] home controls are at least ${min}px`, async () => {
    const small = await targets(page, min);
    expect(small.length === 0, small.join(', '));
  });
  await page.fill('#name', 'Checker');
  await page.click('#host');
  await page.waitForSelector('#inLobby:not([hidden]) #code:not(:empty)', { timeout: 10000 });
  await check(`[${view}] lobby controls are at least ${min}px`, async () => {
    const small = await targets(page, min);
    expect(small.length === 0, small.join(', '));
  });
  await page.click('#leave');
  await page.click('#openSettings');
  await page.waitForSelector('#settings:not([hidden])');
  await check(`[${view}] settings controls are at least ${min}px tall`, async () => {
    const small = await page.evaluate((m) => [...document.querySelectorAll('#settings button, #settings select, #settings input:not([type=checkbox]):not([type=range])')]
      .filter((e) => e.offsetParent !== null && !e.disabled).map((e) => [e.id || e.textContent.trim().slice(0, 14), e.getBoundingClientRect().height])
      .filter(([, h]) => h < m - 0.5).map(([n, h]) => `${n} ${Math.round(h)}px`), min);
    expect(small.length === 0, small.join(', '));
  });
  await check(`[${view}] the Controls tab does not overflow the screen`, async () => {
    await page.click('#tab-controls');
    const w = await page.evaluate(() => { const f = document.getElementById('front'); return [f.scrollWidth, f.clientWidth]; });
    expect(w[0] <= w[1], `front panel is ${w[0]}px wide in a ${w[1]}px screen`);
  });
  await shot(page, `settings-${view}`);
  await ctx.close();
}

// ---- loading: nothing moves after first paint, nothing is pressable before it works, a press shows at once
const unreachable = pageUrl.replace(/server=.*$/, 'server=ws://127.0.0.1:9');   // nothing listens there: the list of lobbies fails
for (const [view, viewport, opts, url, net] of [['desktop', { width: 1280, height: 800 }, {}, pageUrl, ''], ['phone', { width: 390, height: 844 }, { isMobile: true, hasTouch: true }, pageUrl, ''],
  ['desktop', { width: 1280, height: 800 }, {}, unreachable, ', lobby server unreachable'], ['phone', { width: 390, height: 844 }, { isMobile: true, hasTouch: true }, unreachable, ', lobby server unreachable']]) {
  const ctx = await browser.newContext({ viewport, ...opts });
  const page = await ctx.newPage();
  await page.addInitScript(() => {
    window.__first = null;
    window.__moves = [];
    window.__readyAt = null;
    const t0 = performance.now();
    const ids = ['name', 'monster', 'single', 'host', 'join', 'openSettings'];
    const tick = () => {
      const r = Object.fromEntries(ids.map((id) => { const e = document.getElementById(id); const b = e?.getBoundingClientRect(); return [id, b ? [Math.round(b.x), Math.round(b.y), Math.round(b.width), Math.round(b.height)] : null]; }));
      const here = JSON.stringify(r);
      if (window.__first === null && ids.every((id) => r[id])) window.__first = r;   // the whole panel is parsed (a streaming parser shows it in pieces)
      else if (window.__first) for (const id of ids) if (r[id] && JSON.stringify(r[id]) !== JSON.stringify(window.__first[id])) window.__moves.push(`${id} ${window.__first[id]} to ${r[id]} at ${Math.round(performance.now() - t0)}ms`);
      const s = document.getElementById('single');
      if (window.__readyAt === null && s && !s.disabled) window.__readyAt = Math.round(performance.now() - t0);
      if (performance.now() - t0 < 3500) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  });
  await page.goto(url, { waitUntil: 'commit' });
  await page.waitForSelector('#home:not([hidden])');
  await sleep(3800);
  const r = await page.evaluate(() => ({ moves: [...new Set(window.__moves)], readyAt: window.__readyAt, panelSeen: window.__first && true }));
  await check(`[${view}${net}] no control moves after first paint`, async () => {
    expect(r.panelSeen, 'the whole panel never appeared, so nothing was compared');
    expect(r.moves.length === 0, r.moves.slice(0, 3).join('; '));
    return `Single player usable at ${r.readyAt}ms`;
  });
  if (!net) await check(`[${view}] a press shows at once (pressed state)`, async () => {
    const box = await page.locator('#single').boundingBox();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.down();
    const t = await page.evaluate(() => getComputedStyle(document.getElementById('single')).transform);
    await page.mouse.move(2, 2);   // let go away from the button: a click would start the game
    await page.mouse.up();
    expect(t !== 'none', 'no transform while pressed');
  });
  await ctx.close();
}
await check('field borders are at least 3:1 against the panel', async () => {
  const { ctx, page } = await openPage({ width: 1280, height: 800 });
  const r = await page.evaluate(() => {
    const parse = (c) => c.match(/[\d.]+/g).slice(0, 3).map(Number);
    const lum = (rgb) => { const [r, g, b] = rgb.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
    const ratio = (a, b) => { const [hi, lo] = [lum(a), lum(b)].sort((x, y) => y - x); return (hi + 0.05) / (lo + 0.05); };
    const panel = parse(getComputedStyle(document.getElementById('home')).backgroundColor);
    return ['name', 'monster', 'joinCode'].map((id) => [id, ratio(parse(getComputedStyle(document.getElementById(id)).borderTopColor), panel)]).filter(([, x]) => x < 3).map(([id, x]) => `${id} ${x.toFixed(2)}:1`);
  });
  await ctx.close();
  expect(r.length === 0, r.join(', '));
});
// ---- a page whose scripts do not load says so
for (const [what, serve] of [['cannot be fetched', (route) => route.abort()], ['has a syntax error', (route) => route.fulfill({ contentType: 'text/javascript', body: 'export {{{ not javascript' })]]) {
  await check(`a page whose front.js ${what} says so and offers nothing`, async () => {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
    const page = await ctx.newPage();
    await page.route('**/front.js', serve);
    await page.goto(pageUrl, { waitUntil: 'load' });
    await page.waitForFunction(() => /could not load/i.test(document.getElementById('frontMsg').textContent), null, { timeout: 5000 });
    const on = await page.evaluate(() => ['single', 'host', 'join', 'refreshLobbies'].filter((id) => !document.getElementById(id).disabled));
    await ctx.close();
    expect(on.length === 0, `enabled: ${on}`);
  });
}

await check('a classic script of the page that cannot be fetched stops the panel with a message', async () => {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const page = await ctx.newPage();
  await page.route('**/support.js', (route) => route.abort());
  await page.goto(pageUrl, { waitUntil: 'load' });
  await sleep(1000);
  const r = await page.evaluate(() => ({ text: document.getElementById('frontMsg').textContent, cls: document.getElementById('frontMsg').className, on: ['single', 'host', 'join', 'refreshLobbies'].filter((id) => !document.getElementById(id).disabled) }));
  await ctx.close();
  expect(r.text !== '' && r.cls === 'problem', `message '${r.text}' class '${r.cls}'`);
  expect(r.on.length === 0, `enabled: ${r.on}`);
});

await check('a page that is slow to start says so, and recovers when it does', async () => {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const page = await ctx.newPage();
  await page.route('**/front.js', async (route) => { await sleep(7500); await route.continue(); });
  await page.goto(pageUrl, { waitUntil: 'commit' });
  await page.waitForFunction(() => /Still loading/.test(document.getElementById('frontMsg').textContent), null, { timeout: 9000 });
  await page.waitForFunction(() => !document.getElementById('single').disabled, null, { timeout: 15000 });
  const after = await page.evaluate(() => ({ text: document.getElementById('frontMsg').textContent, cls: document.getElementById('frontMsg').className }));
  await ctx.close();
  expect(after.text === '' && after.cls === '', `after start: '${after.text}' class '${after.cls}'`);
});

// ---- a failed icon or stylesheet before the page is ready is not a failed page
await check('a failed icon, stylesheet or unrelated script while front.js is still loading does not raise an alarm', async () => {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const page = await ctx.newPage();
  await page.route('**/front.js', async (route) => { await sleep(1500); await route.continue(); });
  await page.goto(pageUrl, { waitUntil: 'commit' });
  await page.waitForSelector('#frontMsg', { state: 'attached' });
  await page.route('**/stray.js', (route) => route.fulfill({ contentType: 'text/javascript', body: "throw new Error('an unrelated script of this origin');" }));
  await page.evaluate(() => {
    const l = document.createElement('link'); l.rel = 'stylesheet'; l.href = '/no-such-file.css'; document.head.append(l);
    for (const src of ['/stray.js', '/missing.js']) { const s = document.createElement('script'); s.src = src; document.head.append(s); }
  });
  await sleep(500);
  const during = await page.evaluate(() => document.getElementById('frontMsg').textContent);
  await page.waitForFunction(() => !document.getElementById('single').disabled, null, { timeout: 10000 });
  const after = await page.evaluate(() => ({ text: document.getElementById('frontMsg').textContent, cls: document.getElementById('frontMsg').className }));
  await ctx.close();
  expect(during === 'Loading…', `the message became: ${during}`);
  expect(after.text === '' && after.cls === '', `after start: '${after.text}' class '${after.cls}'`);
});

// ---- nothing can be pressed until front.js has run, then everything can
await check('Single player, Host, Join and Refresh are off until front.js runs, then on', async () => {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const page = await ctx.newPage();
  await page.route('**/front.js', async (route) => { await sleep(1500); await route.continue(); });
  await page.goto(pageUrl, { waitUntil: 'commit' });
  await page.waitForSelector('#refreshLobbies', { state: 'attached' });
  const ids = ['single', 'host', 'join', 'refreshLobbies'];
  const early = await page.evaluate((list) => list.filter((id) => !document.getElementById(id).disabled), ids);
  await page.waitForFunction((list) => list.every((id) => !document.getElementById(id).disabled), ids, { timeout: 10000 });
  await ctx.close();
  expect(early.length === 0, `pressable before front.js ran: ${early}`);
});

// ---- back from a lobby the list works at once
await check('after Leave the open lobbies list is not greyed', async () => {
  const { ctx, page } = await openPage({ width: 1280, height: 800 });
  await page.fill('#name', 'Leaver');
  await page.click('#host');
  await page.waitForSelector('#inLobby:not([hidden]) #code:not(:empty)', { timeout: 10000 });
  await page.click('#leave');
  await page.waitForSelector('#choose:not([hidden])');
  const inert = await page.evaluate(() => document.getElementById('lobbyList').inert);
  await ctx.close();
  expect(!inert, 'the list is inert after leaving a lobby');
});

// ---- Enter in the join code, pressed twice, joins once
await check('Enter pressed twice in the join code opens one connection', async () => {
  const host = await openPage({ width: 1280, height: 800 });
  await host.page.fill('#name', 'Owner');
  await host.page.click('#host');
  await host.page.waitForSelector('#inLobby:not([hidden]) #code:not(:empty)', { timeout: 10000 });
  const code = (await host.page.locator('#code').innerText()).trim();
  const g = await openPage({ width: 1280, height: 800 });
  await g.page.fill('#name', 'Twice');
  await g.page.fill('#joinCode', code);
  await sleep(600);   // the list watcher's own connection is settled
  let opened = 0;
  g.page.on('websocket', () => { opened++; });
  // both key presses in one task: the second must find the Join button already off
  await g.page.evaluate(() => { const i = document.getElementById('joinCode'); for (let k = 0; k < 2; k++) i.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true })); });
  await g.page.waitForSelector('#inLobby:not([hidden])', { timeout: 10000 });
  await sleep(500);
  await g.ctx.close();
  await host.ctx.close();
  expect(opened === 1, `${opened} connections opened`);
});

// ---- the Settings tab keeps its own look (it builds elements with the classes .online and .hint)
await check('Settings Online tab notes carry no border or extra spacing from the home panel', async () => {
  const { ctx, page } = await openPage({ width: 1280, height: 800 });
  await page.click('#openSettings');
  await page.click('#tab-online');
  const r = await page.evaluate(() => [...document.querySelectorAll('#settings .online')].map((e) => [getComputedStyle(e).borderTopWidth, getComputedStyle(e).paddingTop]));
  const h = await page.evaluate(() => [...document.querySelectorAll('#settings .hint')].map((e) => getComputedStyle(e).marginTop));
  await ctx.close();
  expect(r.length > 0 && r.every(([b, p]) => b === '0px' && p === '0px'), `.online: ${JSON.stringify(r)}`);
  expect(h.every((m) => m === '0px'), `.hint margins: ${h}`);
});

// ---- structure: landmarks, the online group, Start beside Leave, what a guest sees
{
  const { ctx, page } = await openPage({ width: 1280, height: 800 });
  await check('the panel is the one main landmark, with a Play online region', async () => {
    expect(await page.getByRole('main').count() === 1, 'no single main landmark');
    expect(await page.getByRole('region', { name: 'Play online' }).count() === 1, 'no region named Play online');
  });
  await check('the status bar is not drawn under the panel', async () => {
    const v = await page.evaluate(() => getComputedStyle(document.getElementById('bar')).visibility);
    expect(v === 'hidden', `visibility ${v}`);
  });
  await check('desktop buttons and fields are at least 32px tall', async () => {
    const small = await page.evaluate(() => [...document.querySelectorAll('#home button, #home input, #home select')].filter((e) => e.offsetParent && !e.disabled).map((e) => [e.id, e.getBoundingClientRect().height]).filter(([, h]) => h < 31.5).map(([id, h]) => `${id} ${Math.round(h)}px`));
    expect(small.length === 0, small.join(', '));
  });
  await page.fill('#name', 'Checker');
  await page.click('#host');
  await page.waitForSelector('#inLobby:not([hidden]) #code:not(:empty)', { timeout: 10000 });
  await check('Start and Leave sit on one row, Start the wider', async () => {
    const [s, l] = await Promise.all([page.locator('#start').boundingBox(), page.locator('#leave').boundingBox()]);
    expect(Math.abs(s.y - l.y) < 2 && s.width > l.width, `start ${JSON.stringify(s)} leave ${JSON.stringify(l)}`);
  });
  await check('the lobby code is the largest text on the lobby', async () => {
    const [c, h] = await page.evaluate(() => [parseFloat(getComputedStyle(document.getElementById('code')).fontSize), parseFloat(getComputedStyle(document.querySelector('#home h1')).fontSize)]);
    expect(c > h, `code ${c}px, heading ${h}px`);
  });
  const code = (await page.locator('#code').innerText()).trim();
  const guest = await openPage({ width: 390, height: 844 }, { isMobile: true, hasTouch: true });
  await guest.page.fill('#name', 'Guest');
  await guest.page.fill('#joinCode', code);
  await guest.page.click('#join');
  await guest.page.waitForSelector('#inLobby:not([hidden])', { timeout: 10000 });
  await check('a guest has Leave and no Start or rule controls', async () => {
    expect(await guest.page.locator('#leave').isVisible() && !(await guest.page.locator('#start').isVisible()) && !(await guest.page.locator('#rules').isVisible()), 'guest controls wrong');
  });
  await check('the owner sees the guest join and Start stays', async () => {
    await page.waitForFunction(() => document.querySelectorAll('#members li').length === 2, null, { timeout: 8000 });
    expect(await page.locator('#start').isVisible(), 'Start hidden for the owner');
  });
  await guest.ctx.close();
  await ctx.close();
}

// ---- the status bar over the game: not faded, readable, sized
for (const [view, viewport, opts, min] of [['desktop', { width: 1280, height: 800 }, {}, 24], ['touch', { width: 844, height: 390 }, { hasTouch: true }, 44]]) {
  const { ctx, page } = await openPage(viewport, opts);
  await (opts.hasTouch ? page.locator('#single').tap() : page.click('#single'));   // a mouse click would turn the touch controls off (web/touch.js)
  await page.waitForFunction(() => document.getElementById('front').hidden);
  await page.evaluate(() => { document.getElementById('bar').hidden = false; });   // the touch controls would hide it; this reads the bar itself
  await check(`[${view}] status bar is at full opacity`, async () => {
    const o = await page.evaluate(() => getComputedStyle(document.getElementById('bar')).opacity);
    expect(o === '1', `opacity ${o}`);
  });
  await check(`[${view}] status bar buttons are at least ${min}x${min}px`, async () => {
    const sizes = await page.evaluate(() => [...document.querySelectorAll('#bar button')].map((b) => { const r = b.getBoundingClientRect(); return [Math.round(r.width), Math.round(r.height)]; }));
    expect(sizes.length > 0 && sizes.every(([w, h]) => w >= min && h >= min), `sizes ${sizes.map(([w, h]) => `${w}x${h}`)}`);
  });
  await check(`[${view}] status bar text is at least 4.5:1 over a white frame`, async () => {
    const r = await page.evaluate(() => {
      const bar = document.getElementById('bar');
      const [fr, fg, fb] = getComputedStyle(bar).color.match(/[\d.]+/g).map(Number);
      const [br, bg, bb, ba = 1] = getComputedStyle(bar).backgroundColor.match(/[\d.]+/g).map(Number);
      const mix = [br, bg, bb].map((v) => v * ba + 255 * (1 - ba));
      const lum = (rgb) => { const [r, g, b] = rgb.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
      const [hi, lo] = [lum([fr, fg, fb]), lum(mix)].sort((a, b) => b - a);
      return (hi + 0.05) / (lo + 0.05);
    });
    expect(r >= 4.5, `${r.toFixed(2)}:1`);
  });
  await shot(page, `bar-${view}`);
  await ctx.close();
}

// ---- the touch layout panel (gear) still fits a phone held sideways and still edits
for (const [w, h] of [[844, 390], [568, 320]]) {
  const { ctx, page } = await openPage({ width: w, height: h }, { isMobile: true, hasTouch: true }, { refuseFullscreen: true });
  await page.locator('#single').tap();
  await page.waitForSelector('#touchpad.on', { timeout: 5000 });
  await page.locator('#touchpad [data-id=gear]').tap();
  await page.waitForSelector('#tpPanel:not([hidden])');
  await check(`touch panel fits the screen (${w}x${h} phone)`, async () => {
    const inner = await page.evaluate(() => innerHeight);
    expect(inner === h, `the window is ${inner}px tall, not ${h}: the measurement would mean nothing`);
    const box = await page.locator('#tpPanel').boundingBox();
    expect(box.y >= 0 && box.y + box.height <= h, `panel spans ${Math.round(box.y)} to ${Math.round(box.y + box.height)} of ${h}`);
  });
  // the panel's own size is the touch layout's (web/touch.js); the page's target size must not reach it, and its span is reported
  await check(`touch panel keeps its own button sizes (${w}x${h})`, async () => {
    const mins = await page.evaluate(() => [...document.querySelectorAll('#tpPanel button')].map((b) => getComputedStyle(b).minHeight));
    expect(mins.length > 0 && mins.every((m) => m === '0px' || m === 'auto'), `min-height of its buttons: ${mins}`);
    const box = await page.locator('#tpPanel').boundingBox();
    return `panel spans ${Math.round(box.y)} to ${Math.round(box.y + box.height)} of ${await page.evaluate(() => innerHeight)}`;
  });
  await check(`touch panel: Move buttons starts editing (${w}x${h})`, async () => {
    await page.getByRole('button', { name: 'Move buttons' }).tap();
    expect(await page.evaluate(() => document.getElementById('touchpad').classList.contains('editing')), 'no editing class');
  });
  await shot(page, `touchpanel-${w}x${h}`);
  await ctx.close();
}
// ---- the loading screen: the parts, contrast over its art, the button, and motion only where the player allows it
for (const [view, viewport, opts, min] of [['phone', { width: 844, height: 390 }, { isMobile: true, hasTouch: true }, 44], ['small phone', { width: 568, height: 320 }, { isMobile: true, hasTouch: true }, 44], ['desktop', { width: 1280, height: 800 }, {}, 24]]) {
  const { ctx, page } = await openPage(viewport, opts, { refuseFullscreen: true });   // Firefox's window grows to the screen in fullscreen
  await showLoadingScreen(page, 'download');
  await check(`[${view}] the loading screen shows progress, bytes, time left, a note and a tip`, async () => {
    const t = await page.evaluate(() => Object.fromEntries(['loadHeadline', 'loadAmount', 'loadEta', 'loadNote', 'loadTip'].map((id) => [id, document.getElementById(id).textContent])));
    expect(t.loadHeadline === 'Downloading game data' && t.loadAmount === '9 of 27 MB' && /left$/.test(t.loadEta) && /first visit/.test(t.loadNote) && t.loadTip.length > 20, JSON.stringify(t));
    const bar = await page.evaluate(() => { const b = document.getElementById('loadBar'); return [b.value, b.max, b.getAttribute('aria-valuetext')]; });
    expect(bar[0] === 33 && bar[1] === 100 && /9 of 27 MB/.test(bar[2]), JSON.stringify(bar));
  });
  await check(`[${view}] loading screen text is at least 4.5:1 on its card, over the brightest part of its backdrop`, async () => {
    const low = await page.evaluate(() => {
      const parse = (c) => c.match(/[\d.]+/g).map(Number);
      const lum = (rgb) => { const [r, g, b] = rgb.map((v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
      const ratio = (a, b) => { const [hi, lo] = [lum(a), lum(b)].sort((x, y) => y - x); return (hi + 0.05) / (lo + 0.05); };
      const [r, g, b, a = 1] = parse(getComputedStyle(document.querySelector('.loadcard')).backgroundColor);
      const worst = [122, 42, 14];   // the brightest colour of the backdrop's gradient
      const card = [r, g, b].map((v, i) => v * a + worst[i] * (1 - a));
      return ['loadHeadline', 'loadAmount', 'loadEta', 'loadNote', 'loadTip', 'loadNextTip'].map((id) => [id, ratio(parse(getComputedStyle(document.getElementById(id)).color).slice(0, 3), card)]).filter(([, q]) => q < 4.5).map(([id, q]) => `${id} ${q.toFixed(2)}`);
    });
    expect(low.length === 0, low.join(', '));
  });
  await check(`[${view}] Another tip is at least ${min}px, reachable by keyboard, and changes the tip`, async () => {
    const box = await page.locator('#loadNextTip').boundingBox();
    expect(box.height >= min - 0.5 && box.width >= min - 0.5, `${Math.round(box.width)}x${Math.round(box.height)}px`);
    const before = await page.locator('#loadTip').innerText();
    await page.focus('#loadNextTip');
    await page.keyboard.press('Enter');
    expect(await page.locator('#loadTip').innerText() !== before, 'the tip stayed');
  });
  if (view === 'phone') await check('[phone] no touch button covers Another tip', async () => {
    await page.waitForSelector('#touchpad .tp-ctl', { state: 'attached' });
    const covered = await page.evaluate(() => {
      const b = document.getElementById('loadNextTip').getBoundingClientRect();
      return [...document.querySelectorAll('#touchpad .tp-ctl')].filter((e) => e.offsetParent !== null).map((e) => [e.dataset.id, e.getBoundingClientRect()])
        .filter(([, r]) => r.left < b.right && r.right > b.left && r.top < b.bottom && r.bottom > b.top).map(([id]) => id);
    });
    expect(covered.length === 0, `covered by ${covered}`);
  });
  await check(`[${view}] the screen's box stays inside the window`, async () => {
    const [w, h] = await page.evaluate(() => [innerWidth, innerHeight]);
    expect(w === viewport.width && h === viewport.height, `the window is ${w}x${h}, not ${viewport.width}x${viewport.height}`);
    const box = await page.locator('.loadcard').boundingBox();
    expect(box.x >= 0 && box.y >= 0 && box.x + box.width <= viewport.width && box.y + box.height <= viewport.height, JSON.stringify(box));
  });
  await check(`[${view}] the card rises in only for players who have not asked for less motion`, async () => {
    const name = () => page.evaluate(() => getComputedStyle(document.querySelector('.loadcard')).animationName);
    await page.emulateMedia({ reducedMotion: 'reduce' });
    const reduced = await name();
    await page.emulateMedia({ reducedMotion: 'no-preference' });
    const normal = await name();
    expect(reduced === 'none' && normal === 'rise', `reduce: ${reduced}, no-preference: ${normal}`);
  });
  await ctx.close();
}
// ---- the result card: the button, the frame copy of a real WebGL canvas, and the PNG that comes out
for (const [view, viewport, opts, min] of [['phone', { width: 844, height: 390 }, { isMobile: true, hasTouch: true }, 44], ['desktop', { width: 1280, height: 800 }, {}, 24]]) {
  const { ctx, page } = await openPage(viewport, opts, { refuseFullscreen: true });
  // the game's score cells: a Preytor 3, Congar 1 match to 3 wins that Preytor has taken
  const cells = [1, 2, 3, 4, 0, 5, 3, 1, 1];
  await page.evaluate((cells) => {
    document.getElementById('front').hidden = true;
    document.getElementById('stage').inert = false;
    window.cellsNow = cells;
    wotmResultUi.watchResult({ button: document.getElementById('resultSave'), cell: (i) => window.cellsNow[i] ?? 0, setTimer: setInterval, clearTimer: clearInterval });
  }, cells);
  await page.waitForSelector('#resultSave:not([hidden])', { timeout: 3000 });
  await check(`[${view}] the Save button shows with a result, is at least ${min}px, and stays inside the window`, async () => {
    const box = await page.locator('#resultSave').boundingBox();
    expect(box.height >= min - 0.5 && box.width >= min - 0.5, `${Math.round(box.width)}x${Math.round(box.height)}px`);
    expect(box.x >= 0 && box.x + box.width <= viewport.width && box.y >= 0 && box.y + box.height <= viewport.height, JSON.stringify(box));
    const bar = await page.locator('#bar').boundingBox();   // null where the bar is not drawn
    expect(!bar || box.x + box.width <= bar.x || bar.x + bar.width <= box.x || box.y + box.height <= bar.y || bar.y + bar.height <= box.y, 'it overlaps the status bar');
  });
  await check(`[${view}] the button hides again when the game is back in the menus`, async () => {
    await page.evaluate(() => { window.cellsNow = [0]; });
    await page.waitForSelector('#resultSave[hidden]', { state: 'attached', timeout: 3000 });
    await page.evaluate((cells) => { window.cellsNow = cells; }, cells);
    await page.waitForSelector('#resultSave:not([hidden])', { timeout: 3000 });
  });
  await check(`[${view}] the frame copy of a WebGL canvas in the page holds the frame drawn just before it`, async () => {
    const r = await page.evaluate(async () => {
      const gl = document.createElement('canvas');
      gl.width = 64; gl.height = 36;
      document.body.appendChild(gl);   // in the page, so its frames are presented like the game's
      const g = gl.getContext('webgl2');
      const raf = () => new Promise((resolve) => requestAnimationFrame(resolve));
      const make = () => document.createElement('canvas');
      let stop = false;
      const loop = () => {   // the game's frame loop: draws in its animation frame callback, then asks for the next
        g.clearColor(1, 0, 0, 1);
        g.clear(g.COLOR_BUFFER_BIT);
        if (!stop) requestAnimationFrame(loop);
      };
      loop();
      await raf(); await raf();
      // the Save press: a callback asked for between frames runs after the loop's, in the same frame, before the buffer is presented
      const same = await wotmResultUi.captureFrame(gl, { raf: (cb) => requestAnimationFrame(cb), makeCanvas: make });
      // a copy a task later reads the buffer after it was presented
      stop = true;
      await raf();
      await new Promise((resolve) => setTimeout(resolve, 100));
      const taskLater = await new Promise((resolve) => setTimeout(() => resolve(wotmResultUi.captureFrame(gl, { raf: (cb) => cb(), makeCanvas: make })), 0));
      gl.remove();
      return [same !== null, taskLater === null];
    });
    expect(r[0], 'the copy of the frame drawn just before it was blank');
    return `a copy outside the frame callback reads ${r[1] ? 'blank (so the copy is made inside it)' : 'pixels'}`;
  });
  await check(`[${view}] the card is a 1200x630 PNG, saved as a download, with the picture`, async () => {
    const [download] = await Promise.all([
      page.waitForEvent('download', { timeout: 8000 }),
      page.evaluate(async () => {
        const frame = document.createElement('canvas');
        frame.width = 640; frame.height = 360;
        const f = frame.getContext('2d');
        f.fillStyle = '#2a6'; f.fillRect(0, 0, 640, 360);
        const R = wotmResult;
        const canvas = document.createElement('canvas');
        await wotmResultUi.saveCard({
          cell: (i) => window.cellsNow[i] ?? 0, makeCanvas: () => canvas, draw: R.drawCard, capture: async () => frame,
          deliver: (blob) => wotmResultUi.deliverPng(blob, { nav: {}, makeFile: (b, n) => b, download: (b, name) => {
            const a = Object.assign(document.createElement('a'), { href: URL.createObjectURL(b), download: name });
            a.click();
          } }),
        });
      }),
    ]);
    expect(download.suggestedFilename() === 'war-of-the-monsters-result.png', download.suggestedFilename());
    const path = join(out, `card-${browserName}-${view}.png`);
    await download.saveAs(path);
    const bytes = (await import('node:fs')).readFileSync(path);
    const [w, h] = [bytes.readUInt32BE(16), bytes.readUInt32BE(20)];
    expect(bytes.subarray(1, 4).toString() === 'PNG' && w === 1200 && h === 630, `${w}x${h}`);
    return `${bytes.length} bytes`;
  });
  await shot(page, `result-button-${view}`);
  await ctx.close();
}
await finish();
