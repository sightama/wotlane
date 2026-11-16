// ABOUTME: Browser test for the touch controls and gamepads: a phone-sized Chromium (touch, multi-touch through the DevTools protocol) and a
// ABOUTME: mocked Gamepad API drive the page and a real fight, and the game's own pad log says what it read. See README.md.
import { spawn } from 'node:child_process';
import { createServer } from 'node:net';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { loadPlaywright, REPO } from '../browser_mp_test/lib/infra.mjs';

const args = process.argv.slice(2);
const opt = (name, fallback) => { const i = args.indexOf(`--${name}`); return i >= 0 ? args[i + 1] : fallback; };
if (args.includes('--help')) {
  console.log('node tools/browser_touch_test/run.mjs --site <web_dist> --data <folder with game.iso and SCUS_971.97> [--out <dir>] [--only <scenario,...>] [--browser chromium|chrome]');
  process.exit(0);
}
const runtime = join(REPO, 'runtime');
const site = resolve(opt('site', join(runtime, 'web_dist')));
const data = resolve(opt('data', join(runtime, 'web_data')));
const out = resolve(opt('out', join(runtime, 'browser_touch_test', new Date().toISOString().replace(/[:.]/g, '-'))));
const only = opt('only', '')?.split(',').filter(Boolean);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const ANDROID = 'Mozilla/5.0 (Linux; Android 16; Pixel 9 Pro XL) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Mobile Safari/537.36';
const IPHONE = 'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1';
const PHONE = { viewport: { width: 998, height: 448 }, deviceScaleFactor: 2.25, isMobile: true, hasTouch: true, userAgent: ANDROID };   // a Pixel 9 Pro XL, landscape
const FIGHT = '?WOTM_MATCH=3,1,1';   // the player against one AI, on the first stage

function freePort() {
  return new Promise((res, rej) => {
    const s = createServer();
    s.listen(0, '127.0.0.1', () => { const { port } = s.address(); s.close(() => res(port)); });
    s.on('error', rej);
  });
}

async function startServer() {
  for (const [what, p] of [['site', join(site, 'index.html')], ['data', join(data, 'game.iso')]])
    if (!existsSync(p)) throw new Error(`${what} not found at ${p} (see --help)`);
  const port = await freePort();
  const child = spawn('python3', [join(REPO, 'tools/serve_web.py'), '--port', String(port), '--site', site, '--data', data], { stdio: 'ignore' });
  for (let i = 0; i < 75; i++) {
    try { if ((await fetch(`http://127.0.0.1:${port}/version.json`)).ok) break; } catch { /* not up yet */ }
    await sleep(200);
  }
  return { url: `http://127.0.0.1:${port}/`, stop: () => child.kill('SIGTERM') };
}

// ---- in-page helpers (init scripts) ----

// test-only game environment, added after boot.js's own (nothing in web/ changes)
function envInit(env) {
  let module;
  Object.defineProperty(window, 'Module', {
    configurable: true,
    get() { return module; },
    set(v) {
      module = v;
      if (v && Array.isArray(v.preRun)) v.preRun.push(() => Object.assign(v.ENV, env));
    },
  });
}

// a Gamepad API the test controls: navigator.getGamepads answers window.__pads (raw pads as a browser would give them)
function mockGamepads() {
  window.__pads = [];
  navigator.getGamepads = () => window.__pads.slice();
  window.__setPad = (index, { id = 'Mock Pad', mapping = 'standard', axes = [0, 0, 0, 0], down = {}, nbuttons = 17 } = {}) => {
    const buttons = Array.from({ length: nbuttons }, (_, i) => ({ pressed: (down[i] ?? 0) > 0, value: down[i] ?? 0, touched: (down[i] ?? 0) > 0 }));
    window.__pads[index] = { index, id, mapping, connected: true, timestamp: performance.now(), axes, buttons };
  };
  window.__unplug = (index) => { window.__pads[index] = null; };
}

// ---- one phone ----

class Phone {
  static async open(browser, { url, query = '', env = {}, pads = false, ...contextOptions }) {
    const context = await browser.newContext({ ...PHONE, ...contextOptions });
    if (Object.keys(env).length) await context.addInitScript(envInit, env);
    if (pads) await context.addInitScript(mockGamepads);
    const page = await context.newPage();
    const p = new Phone(context, page);
    page.on('pageerror', (e) => p.errors.push(e.message));
    p.cdp = await context.newCDPSession(page);
    await page.goto(url + query, { waitUntil: 'domcontentloaded' });
    return p;
  }

  constructor(context, page) {
    this.context = context;
    this.page = page;
    this.errors = [];
  }

  log() { return this.page.evaluate(() => window.wotmLog ?? []); }
  async logSince(n) { return (await this.log()).slice(n); }
  async logCount() { return (await this.log()).length; }

  // fingers: [{id, x, y}] for every finger down at this moment
  touch(type, fingers) { return this.cdp.send('Input.dispatchTouchEvent', { type, touchPoints: fingers }); }

  centre(id) {
    return this.page.evaluate((id) => { const r = document.querySelector(`[data-id=${id}]`).getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; }, id);
  }

  // what the game sees in slot `slot`
  pad(slot = 0) {
    return this.page.evaluate((slot) => {
      const p = navigator.getGamepads()[slot];
      return p && { id: p.id, axes: Array.from(p.axes, (a) => +a.toFixed(2)), down: p.buttons.flatMap((b, i) => (b.pressed ? [i] : [])) };
    }, slot);
  }

  overlayVisible() { return this.page.evaluate(() => !!document.getElementById('touchpad') && !document.getElementById('touchpad').hidden); }
  toastText() { return this.page.evaluate(() => (document.getElementById('toast').classList.contains('show') ? document.getElementById('toast').textContent : '')); }

  // the game is running and the touch controls are up
  async waitForFight() {
    await this.page.waitForFunction(() => window.wotTouch && !document.getElementById('touchpad').hidden && (window.wotmLog ?? []).some((l) => /\[turn\]/.test(l)), null, { timeout: 120000 });
    await sleep(2500);
  }

  async close() { await this.context.close(); }
}

// ---- scenarios: each returns a list of [check, ok, detail] ----

const check = (rows, name, ok, detail = '') => rows.push([name, !!ok, ok ? '' : String(detail).slice(0, 300)]);
const near = (a, b, tol = 0.15) => Math.abs(a - b) <= tol;

const scenarios = {
  // the front panel on a phone: playable (not refused), touch rows listed; an iPhone gets a plain reason
  async front_phone({ browser, url }) {
    const rows = [];
    let p = await Phone.open(browser, { url });
    await p.page.waitForFunction(() => document.getElementById('monster')?.options.length > 0);
    check(rows, 'Single player is enabled on an Android phone', !(await p.page.isDisabled('#single')));
    check(rows, 'no refusal message', !/computer|phone/i.test(await p.page.textContent('#frontMsg')), await p.page.textContent('#frontMsg'));
    await p.page.click('#controls summary');
    check(rows, 'the Controls card lists the touch controls', (await p.page.locator('#touchTable tr').count()) >= 14);
    check(rows, 'no touch controls over the front panel', !(await p.overlayVisible()));
    check(rows, 'no page errors', p.errors.length === 0, p.errors.join('; '));
    await p.close();
    p = await Phone.open(browser, { url, userAgent: IPHONE });
    await p.page.waitForFunction(() => document.getElementById('frontMsg').textContent !== '');
    check(rows, 'an iPhone is told why not', /iPhone/.test(await p.page.textContent('#frontMsg')), await p.page.textContent('#frontMsg'));
    check(rows, 'and the play buttons are off', await p.page.isDisabled('#single'));
    await p.close();
    return rows;
  },

  // two thumbs at once on the real overlay: the stick, buttons and chords reach the game's pad, taps are not lost, the look zone turns the camera
  async multitouch({ browser, url }) {
    const rows = [];
    const p = await Phone.open(browser, { url, query: FIGHT, env: { PS2X_PADLOG: '1' } });
    await p.waitForFight();
    check(rows, 'the controls are up and the game sees a pad named for the touch screen', (await p.overlayVisible()) && (await p.pad(0))?.id === 'Touch screen controls', JSON.stringify(await p.pad(0)));
    const logIndex = await p.logCount();
    await p.touch('touchStart', [{ id: 1, x: 150, y: 330 }]);
    for (let i = 1; i <= 6; i++) { await p.touch('touchMove', [{ id: 1, x: 150 + i * 14, y: 330 }]); await sleep(40); }
    const cross = await p.centre('cross');
    await p.touch('touchStart', [{ id: 1, x: 234, y: 330 }, { id: 2, x: cross.x, y: cross.y }]);
    await sleep(200);
    const both = await p.pad(0);
    check(rows, 'a stick push and Cross at the same time', both.axes[0] > 0.9 && both.down.includes(0), JSON.stringify(both));
    const sp1 = await p.centre('special1');
    await p.touch('touchStart', [{ id: 1, x: 234, y: 330 }, { id: 2, x: cross.x, y: cross.y }, { id: 3, x: sp1.x, y: sp1.y }]);
    await sleep(200);
    const chord = await p.pad(0);
    check(rows, 'three fingers: the special chord holds triangle and circle', chord.down.includes(3) && chord.down.includes(1) && chord.down.includes(0) && chord.axes[0] > 0.9, JSON.stringify(chord));
    await p.touch('touchEnd', []);
    await sleep(300);
    const after = await p.pad(0);
    check(rows, 'lifting every finger releases everything', after.down.length === 0 && after.axes.every((a) => a === 0), JSON.stringify(after));
    const padLines = (await p.logSince(logIndex)).filter((l) => /\[pad\] t=.*gamepad 0 left=/.test(l));
    check(rows, 'the game\'s own pad log saw the stick ramp up and return', padLines.some((l) => /left=\+0\.9/.test(l)) && /left=\+0\.00,\+0\.00/.test(padLines.at(-1) ?? ''), padLines.slice(-3).join(' | '));

    // a tap shorter than a frame is still read: sample the pad every 8 ms while a 20 ms tap happens
    await p.page.evaluate(() => { window.__seen = false; window.__sampler = setInterval(() => { if (navigator.getGamepads()[0]?.buttons[2].pressed) window.__seen = true; }, 8); });
    const square = await p.centre('square');
    await p.touch('touchStart', [{ id: 1, x: square.x, y: square.y }]);
    await sleep(20);
    await p.touch('touchEnd', []);
    await sleep(200);
    check(rows, 'a 20 ms tap on Square is seen by the pad', await p.page.evaluate(() => { clearInterval(window.__sampler); return window.__seen; }));

    // the look zone is the right stick
    await p.touch('touchStart', [{ id: 1, x: 560, y: 200 }]);
    for (let i = 1; i <= 6; i++) { await p.touch('touchMove', [{ id: 1, x: 560 + i * 15, y: 200 }]); await sleep(40); }
    const look = await p.pad(0);
    check(rows, 'dragging in the right half turns the camera (right stick)', look.axes[2] > 0.7 && near(look.axes[0], 0, 0.01), JSON.stringify(look));
    await p.touch('touchEnd', []);
    await sleep(200);
    check(rows, 'the game never holds the pointer while the controls are up', await p.page.evaluate(() => document.pointerLockElement === null));
    check(rows, 'no page errors', p.errors.length === 0, p.errors.join('; '));
    await p.page.screenshot({ path: join(out, 'phone_landscape_fight.png') });
    await p.close();
    return rows;
  },

  // a real fight against the AI: the player's monster stands still until the touch stick moves it
  async fight_control({ browser, url }) {
    const rows = [];
    const p = await Phone.open(browser, { url, query: FIGHT, env: { PS2X_PADLOG: '1', WOTM_TURNLOG: '1' } });
    await p.waitForFight();
    const turns = async (from) => (await p.logSince(from)).filter((l) => /\[turn\]/.test(l)).map((l) => ({ mon: /mon=(\w+)/.exec(l)[1], vel: /-> ([+-][\d.]+),([+-][\d.]+),([+-][\d.]+)/.exec(l).slice(1).map(Number), ang: Number(/ang=[^,]*,[^,]*,([+-][\d.]+)/.exec(l)[1]) }));
    const speed = (t) => Math.hypot(...t.vel);
    let from = await p.logCount();
    await sleep(2000);
    const idle = await turns(from);
    const peak = (list) => Object.fromEntries([...new Set(list.map((t) => t.mon))].map((m) => [m, Math.max(...list.filter((t) => t.mon === m).map(speed))]));
    const idlePeak = peak(idle);
    // the player's monster is the one standing still while the AI one moves; it is the one we expect to react
    const [mine] = Object.entries(idlePeak).sort((a, b) => a[1] - b[1])[0];
    check(rows, 'two monsters are in the fight and one stands still without input', Object.keys(idlePeak).length === 2 && idlePeak[mine] < 0.05, JSON.stringify(idlePeak));
    const angBefore = idle.filter((t) => t.mon === mine).at(-1).ang;
    from = await p.logCount();
    await p.touch('touchStart', [{ id: 1, x: 150, y: 330 }]);
    for (let i = 1; i <= 6; i++) { await p.touch('touchMove', [{ id: 1, x: 150 + i * 14, y: 330 }]); await sleep(40); }
    await sleep(2500);
    const moving = await turns(from);
    const mineMoving = moving.filter((t) => t.mon === mine);
    check(rows, 'the player\'s monster moves while the stick is held', Math.max(...mineMoving.map(speed)) > 0.5, `peak ${Math.max(...mineMoving.map(speed))}`);
    check(rows, 'and turns to face the way it was pushed', Math.abs(mineMoving.at(-1).ang - angBefore) > 0.3, `${angBefore} -> ${mineMoving.at(-1).ang}`);
    await p.touch('touchEnd', []);
    await sleep(2500);
    const settled = (await turns(await p.logCount() - 6)).filter((t) => t.mon === mine);
    check(rows, 'it stops when the thumb lifts', settled.length > 0 && Math.max(...settled.map(speed)) < 0.3, JSON.stringify(settled.slice(-1)));
    check(rows, 'no page errors', p.errors.length === 0, p.errors.join('; '));
    await p.close();
    return rows;
  },

  // the keyboard hides the controls, a touch brings them back, the settings panel changes and keeps their look
  async overlay_behaviour({ browser, url }) {
    const rows = [];
    const p = await Phone.open(browser, { url, query: FIGHT });
    await p.waitForFight();
    await p.page.keyboard.press('KeyW');
    await sleep(500);
    check(rows, 'a key press hides the controls and frees the pad slot', !(await p.overlayVisible()) && (await p.pad(0)) === null);
    await p.touch('touchStart', [{ id: 1, x: 150, y: 330 }]);
    await p.touch('touchEnd', []);
    await sleep(500);
    check(rows, 'a touch brings them back', await p.overlayVisible());
    const gear = await p.centre('gear');
    await p.touch('touchStart', [{ id: 1, x: gear.x, y: gear.y }]);
    await p.touch('touchEnd', []);
    await sleep(200);
    check(rows, 'the gear opens the settings panel', await p.page.isVisible('#tpPanel'));
    await p.page.evaluate(() => { const r = document.querySelector('#tpPanel input[type=range]'); r.value = '1.2'; r.dispatchEvent(new Event('input')); });
    const [u1, u0] = await p.page.evaluate(() => [parseFloat(getComputedStyle(document.getElementById('touchpad')).getPropertyValue('--u')), wotTouch.overlay.u]);
    check(rows, 'the size slider resizes the controls and the choice is remembered', near(u1, u0, 0.01) && u1 > 58 && (await p.page.evaluate(() => JSON.parse(localStorage.getItem('wotlane.touch')).scale)) === 1.2, `${u1} ${u0}`);
    const wide = await p.page.evaluate(() => document.querySelector('[data-id=cross]').getBoundingClientRect().width);
    check(rows, 'the cross button grew with it', wide > 90, wide);
    await p.page.screenshot({ path: join(out, 'phone_landscape_settings.png') });
    await p.close();
    return rows;
  },

  // gamepads through a mocked Gamepad API: standard and non-standard pads, hot-plug, the toast, players 1 and 2
  async gamepads({ browser, url }) {
    const rows = [];
    const p = await Phone.open(browser, { url, query: FIGHT, pads: true, env: { PS2X_PADLOG: '1' } });
    await p.waitForFight();
    check(rows, 'before any pad the touch controls are up', await p.overlayVisible());
    await p.page.evaluate(() => __setPad(0, { id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)' }));
    await sleep(1500);
    check(rows, 'a standard pad plugged in: a toast names it and player 1', /Controller connected: Xbox Wireless Controller \(player 1\)/.test(await p.toastText()), await p.toastText());
    check(rows, 'the touch controls step aside', !(await p.overlayVisible()));
    check(rows, 'the game\'s pad 0 is now the real pad', /Xbox/.test((await p.pad(0))?.id ?? ''), JSON.stringify(await p.pad(0)));
    let log = (await p.log()).filter((l) => /\[pad\] player 1/.test(l)).at(-1) ?? '';
    check(rows, 'the game says player 1 is that pad', /player 1: pad 0 \(Xbox Wireless Controller/.test(log), log);
    const from = await p.logCount();
    await p.page.evaluate(() => __setPad(0, { id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)', axes: [1, 0, 0, 0], down: { 0: 1 } }));
    await sleep(600);
    const read = (await p.logSince(from)).filter((l) => /gamepad 0 left=\+1\.00/.test(l));
    check(rows, 'the game reads that pad\'s stick', read.length > 0, (await p.logSince(from)).slice(-3).join(' | '));
    // a second pad that reports no standard mapping (hat on axis 9)
    await p.page.evaluate(() => __setPad(1, { id: 'USB Gamepad', mapping: '', axes: [0, -1, 0, 0, 0, 0, 0, 0, 0, 1.285714], down: { 0: 1 }, nbuttons: 12 }));
    await sleep(1500);
    check(rows, 'a second pad: toast says player 2', /Controller connected: USB Gamepad \(player 2\)/.test(await p.toastText()), await p.toastText());
    log = (await p.log()).filter((l) => /\[pad\] player 2/.test(l)).at(-1) ?? '';
    check(rows, 'the game gives it to player 2', /player 2: pad 1 \(USB Gamepad\)/.test(log), log);
    const second = await p.pad(1);
    check(rows, 'its non-standard layout reaches the game as a standard one (stick up, Cross down)', second && second.axes[1] === -1 && second.down.includes(0), JSON.stringify(second));
    const from2 = await p.logCount();
    await p.page.evaluate(() => __setPad(1, { id: 'USB Gamepad', mapping: '', axes: [0, 0, 0, 0, 0, 0, 0, 0, 0, 1.285714], nbuttons: 12 }));
    await p.page.evaluate(() => __setPad(1, { id: 'USB Gamepad', mapping: '', axes: [0, 0, 0, 0, 0, 0, 0, 0, 0, -1], nbuttons: 12 }));
    await sleep(300);
    check(rows, 'its hat is the d-pad', (await p.pad(1)).down.includes(12), JSON.stringify(await p.pad(1)));
    await p.page.screenshot({ path: join(out, 'phone_landscape_gamepad_toast.png') });
    // unplug both: toasts, then the touch controls come back
    await p.page.evaluate(() => __unplug(1));
    await sleep(1500);
    check(rows, 'unplugging a pad shows a toast', /Controller disconnected: USB Gamepad/.test(await p.toastText()), await p.toastText());
    await p.page.evaluate(() => __unplug(0));
    await sleep(1500);
    check(rows, 'with no pad left the touch controls return', (await p.overlayVisible()) && (await p.pad(0))?.id === 'Touch screen controls', JSON.stringify(await p.pad(0)));
    check(rows, 'no page errors', p.errors.length === 0, p.errors.join('; '));
    void from2;
    await p.close();
    return rows;
  },
};

// ---- run ----

mkdirSync(out, { recursive: true });
const pw = await loadPlaywright();
const server = await startServer();
const browser = await (opt('browser', 'chromium') === 'chrome'
  ? pw.chromium.launch({ channel: 'chrome', headless: true })
  : pw.chromium.launch({ channel: 'chromium', headless: true, args: ['--use-angle=metal', '--enable-gpu', '--ignore-gpu-blocklist'] }));
const results = [];
let failed = false;
try {
  for (const [name, fn] of Object.entries(scenarios)) {
    if (only.length && !only.includes(name)) continue;
    process.stdout.write(`${name} ... `);
    let rows;
    try { rows = await fn({ browser, url: server.url }); } catch (e) { rows = [['scenario ran', false, e.message.split('\n')[0].slice(0, 300)]]; }
    const bad = rows.filter((r) => !r[1]);
    console.log(bad.length ? `FAIL (${bad.length} of ${rows.length})` : `ok (${rows.length} checks)`);
    for (const [n, ok, detail] of rows) if (!ok) console.log(`    FAIL ${n}${detail ? `: ${detail}` : ''}`);
    failed ||= bad.length > 0;
    results.push({ name, rows });
  }
} finally {
  await browser.close();
  server.stop();
}
const table = results.flatMap((s) => s.rows.map(([n, ok, d]) => `| ${s.name} | ${n} | ${ok ? 'pass' : 'FAIL ' + d} |`));
writeFileSync(join(out, 'results.md'), `| scenario | check | result |\n|---|---|---|\n${table.join('\n')}\n`);
writeFileSync(join(out, 'results.json'), JSON.stringify(results, null, 1));
console.log(`results in ${out}`);
process.exit(failed ? 1 : 0);
