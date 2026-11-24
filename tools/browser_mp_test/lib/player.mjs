// ABOUTME: One simulated player: a browser context of its own (so its own OPFS: saves, mods and creations) on the game's page. Drives
// ABOUTME: the front panel (host, join, picks, start), keeps the game's page log, plays with the keyboard and can freeze its network.
import { diagLineOf } from './diaglog.mjs';
import { RELOADED } from './logs.mjs';

// the game's default keyboard layout (native/wotm_settings.hpp kDefaultKb): arrows d-pad (they pick menu lines), WASD the left stick, Space X, E circle
export const KEYS = { up: 'ArrowUp', down: 'ArrowDown', left: 'ArrowLeft', right: 'ArrowRight', cross: 'Space', circle: 'e', special: '1', start: 'Enter' };

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
export { sleep };

// Test-only environment for the game (PS2X_*, WOTM_*): a Module setter adds a preRun step after boot.js's own, so these reach the
// runtime's ENV the way the page's variables do. Nothing in web/ changes.
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

// Keeps every text the front panel's message line has shown, so a refusal that is replaced a moment later is still on record
function frontMessageHistory() {
  window.__frontMsgs = [];
  document.addEventListener('DOMContentLoaded', () => {
    const el = document.getElementById('frontMsg');
    if (!el) return;   // the init script also runs in the blank tab that hides a player's tab
    new MutationObserver(() => window.__frontMsgs.push(el.textContent)).observe(el, { childList: true, characterData: true, subtree: true });
  });
}

export class Player {
  constructor({ name, index, context, page, monster }) {
    this.name = name;
    this.index = index;   // 0-based: the player number in the match is index + 1 (the lobby order)
    this.context = context;
    this.page = page;
    this.monster = monster;
    this.logText = '';
    this.logLines = 0;
    this.errors = [];     // uncaught page errors
    this.closed = false;
  }

  // `target.url(path)` is the page address, `env` the test-only game environment, `video` a folder for a recording or undefined
  static async open(browser, { name, index, monster, url, env = {}, viewport = { width: 640, height: 900 }, gameViewport = { width: 640, height: 400 }, videoDir, cookies, invited = false, remembered = false, initScripts = [], onDiag }) {
    const context = await browser.newContext({ viewport, ...(videoDir ? { recordVideo: { dir: videoDir, size: viewport } } : {}) });
    if (remembered) await context.addInitScript((n) => { try { localStorage.setItem('wotlane.name', n); } catch { /* not kept */ } }, name);
    if (cookies) await context.addCookies(cookies);
    await context.addInitScript(frontMessageHistory);
    for (const script of initScripts) await context.addInitScript(script);
    if (Object.keys(env).length) await context.addInitScript(envInit, env);
    const page = await context.newPage();
    const p = new Player({ name, index, context, page, monster });
    p.gameViewport = gameViewport;
    // every DIAG report the page sends on its lobby socket (the server's diagnostics files are out of reach on the live site)
    if (onDiag) page.on('websocket', (ws) => ws.on('framesent', ({ payload }) => { const line = diagLineOf(payload); if (line) onDiag(p.name, line); }));
    page.on('pageerror', (e) => p.errors.push(`${e.message}${e.stack ? ' @ ' + e.stack.split('\n').slice(0, 3).join(' | ') : ''}`));
    await page.goto(url, { waitUntil: 'domcontentloaded' });
    await page.waitForFunction(() => document.getElementById('monster')?.options.length > 0);
    if (invited) {
      await page.waitForFunction(() => document.getElementById('joinCode').value !== '' || document.getElementById('frontMsg').textContent !== '', null, { timeout: 15000 });
      return p;
    }   // an invite link opened the page: the name and monster are the panel's to ask for
    await page.fill('#name', name);
    if (monster !== undefined) await page.selectOption('#monster', String(monster));
    return p;
  }

  // ---- front panel ----

  async frontMsg() { return (await this.page.textContent('#frontMsg')).trim(); }

  // Waits for the panel to show a lobby or an error. Returns {joined, code, msg (what the panel shows a moment later), first (the first
  // message after the request that was not "Joining" or "Creating a lobby": the server's reason when it refused)}
  async settle() {
    await this.page.waitForFunction(() => {
      const inLobby = !document.getElementById('inLobby').hidden;
      const idle = !document.getElementById('host').disabled;
      const msg = document.getElementById('frontMsg').textContent;
      return inLobby || (idle && msg !== 'Joining' && msg !== 'Creating a lobby' && msg !== '');
    }, null, { timeout: 15000 });
    const joined = (await this.page.getAttribute('#inLobby', 'hidden')) === null;
    if (!joined) await sleep(1500);   // a later message (the dropped connection's) may replace the refusal
    const history = await this.page.evaluate(() => window.__frontMsgs);
    const first = history.find((m) => m && m !== 'Joining' && m !== 'Creating a lobby') ?? '';
    return { joined, code: joined ? (await this.page.textContent('#code')).trim() : '', msg: await this.frontMsg(), first };
  }

  async host() {
    await this.page.waitForFunction(() => !document.getElementById('host').disabled, null, { timeout: 15000 });
    await this.page.evaluate(() => { window.__frontMsgs = []; });
    await this.page.click('#host');
    return this.settle();
  }

  async join(code) {
    await this.page.waitForFunction(() => !document.getElementById('join').disabled, null, { timeout: 15000 });
    await this.page.evaluate(() => { window.__frontMsgs = []; });
    await this.page.fill('#joinCode', code);
    await this.page.click('#join');
    return this.settle();
  }

  // the "<name> (<Monster>[, skin x])[, owner]" lines of the member list
  async members() {
    return this.page.$$eval('#members li', (items) => items.map((li) => li.querySelector('.who').textContent));
  }

  async waitMembers(count, timeout = 15000) {
    await this.page.waitForFunction((n) => document.getElementById('members').children.length === n, count, { timeout });
  }

  async guestSettings() { return (await this.page.textContent('#guestSettings')).trim(); }

  async pickMonster(value) {
    await this.page.selectOption('#monster', String(value));
    this.monster = value;
  }

  async setSettings({ stage, wins, mods }) {
    if (stage !== undefined) await this.page.selectOption('#lobbyStage', String(stage));
    if (wins !== undefined) await this.page.selectOption('#wins', String(wins));
    if (mods !== undefined)
      for (const m of ['jump', 'fast', 'turbo']) await this.page.setChecked(`#mod_${m}`, mods.includes(m));
  }

  // the lobby card is taller than a game-sized window with 8 members (its Start button would be out of reach), so the lobby gets a tall
  // window and the game its own size once the match starts
  async start() { await this.page.click('#start'); }

  async toGameViewport() { await this.page.setViewportSize(this.gameViewport).catch(() => {}); }

  async importZip(file) { await this.page.setInputFiles('#importFile', file); }

  async creationsMsg() { return (await this.page.textContent('#creationsMsg')).trim(); }

  // ---- the game ----

  // lines the game has printed since the last call, kept in logText; a page that loaded again (a reload, or the page's own after a
  // match) starts its log over, and RELOADED marks the place
  async pollLog() {
    if (this.closed) return this.logText;
    try {
      const { origin, fresh } = await this.page.evaluate(([from, seen]) => ({
        origin: performance.timeOrigin,
        fresh: (window.wotmLog || []).slice(performance.timeOrigin === seen ? from : 0),
      }), [this.logLines, this.pageOrigin]);
      if (this.pageOrigin !== undefined && origin !== this.pageOrigin) {
        this.logText += RELOADED + '\n';
        this.logLines = 0;
      }
      this.pageOrigin = origin;
      if (fresh.length) {
        this.logLines += fresh.length;
        this.logText += fresh.join('\n') + '\n';
      }
    } catch { /* the page navigated or closed */ }
    return this.logText;
  }

  async status() { return (await this.page.textContent('#status')).trim(); }

  // the game's own wait text ("Waiting for X"), or '' when it is not waiting; and how long it has waited (ms, negative: stopped)
  async stall() {
    return this.page.evaluate(() => {
      const m = window.Module;
      if (!m || !m._wotm_web_net_stall_ms) return { ms: 0, text: '' };
      const ms = m._wotm_web_net_stall_ms();
      return { ms, text: ms ? m.UTF8ToString(m._wotm_web_net_stall_text()) : '' };
    });
  }

  // the page's own stall prompt (web/stall.js): visible, its text and its buttons
  async prompt() {
    return this.page.evaluate(() => {
      const el = document.getElementById('stall');
      return { visible: !el.hidden, text: document.getElementById('stallText').textContent,
        keepWaiting: !document.getElementById('keepWaiting').hidden, leave: document.getElementById('leaveMatch').textContent };
    });
  }

  async frontVisible() { return this.page.evaluate(() => !document.getElementById('front').hidden); }

  async focusGame() { await this.page.focus('#canvas').catch(() => {}); }

  async hold(key, ms) {
    await this.page.keyboard.down(KEYS[key] ?? key);
    await sleep(ms);
    await this.page.keyboard.up(KEYS[key] ?? key);
  }

  // a press the game's menus see: held for several frames, then a pause so two taps are two presses
  async tap(key) {
    await this.hold(key, 250);
    await sleep(450);
  }

  // a few seconds of play that differs per player: walk one way, hit, jump; the same sequence for a given player number
  async play(ms) {
    const walks = ['w', 'a', 's', 'd'];
    const end = Date.now() + ms;
    let k = this.index;
    while (Date.now() < end && !this.closed) {
      await this.hold(walks[k % 4], 700);
      await this.tap(k % 2 ? 'special' : 'cross');
      await this.tap('circle');
      k++;
    }
  }

  // ---- tab state and the network ----

  // another tab in the same context takes the front: this game tab is hidden (the game sends Away)
  // Returns how the tab was hidden: 'real' (another tab took the front and the page reports hidden), or 'emulated' when a headless
  // browser keeps both tabs visible: document.hidden is then forced true and visibilitychange fired, the only thing boot.js reads
  async hide() {
    this.cover = await this.context.newPage();
    await this.cover.bringToFront();
    await sleep(500);
    if (await this.page.evaluate(() => document.hidden)) return 'real';
    await this.page.evaluate(() => {
      Object.defineProperty(document, 'hidden', { configurable: true, get: () => true });
      document.dispatchEvent(new Event('visibilitychange'));
    });
    return 'emulated';
  }

  async show() {
    await this.page.evaluate(() => {
      delete document.hidden;
      document.dispatchEvent(new Event('visibilitychange'));
    });
    await this.page.bringToFront();
    await this.cover?.close();
    this.cover = null;
  }

  // Holds the match WebSocket's traffic both ways: the others wait for this player's inputs and this player for theirs. unfreeze()
  // delivers what was held, in order.
  async freezeNetwork() {
    await this.page.evaluate(() => {
      const f = window.__freeze = { held: [], out: [], sockets: [] };
      for (const ws of Object.values(globalThis.wotmSockets || {})) {
        const send = ws.send.bind(ws), onmessage = ws.onmessage;
        f.sockets.push({ ws, send, onmessage });
        ws.send = (d) => f.out.push(d);
        ws.onmessage = (e) => f.held.push(e);
      }
    });
  }

  async unfreezeNetwork() {
    await this.page.evaluate(() => {
      const f = window.__freeze;
      if (!f) return;
      for (const { ws, send, onmessage } of f.sockets) {
        ws.send = send;
        ws.onmessage = onmessage;
      }
      for (const e of f.held) f.sockets[0]?.onmessage?.(e);
      for (const d of f.out) f.sockets[0]?.send(d);
      window.__freeze = null;
    });
  }

  // closes the match WebSocket from the page's side (the connection drops)
  async dropConnection() {
    await this.page.evaluate(() => { for (const ws of Object.values(globalThis.wotmSockets || {})) ws.close(); });
  }

  async reload() {
    await this.pollLog();
    await this.page.reload({ waitUntil: 'domcontentloaded' });
    await this.page.waitForFunction(() => document.getElementById('monster')?.options.length > 0);
  }

  async shot(path) {
    try { await this.page.screenshot({ path }); } catch { /* the page is gone */ }
  }

  async close() {
    if (this.closed) return;
    await this.pollLog();
    this.closed = true;
    await this.context.close().catch(() => {});
  }
}
