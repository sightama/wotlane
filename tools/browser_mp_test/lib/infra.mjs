// ABOUTME: Where the checks run: a local wotm_server plus tools/serve_web.py on free ports (heavy runs), or the live site behind its
// ABOUTME: password. Also launches the browser(s) and samples the browsers' CPU through the DevTools SystemInfo domain.
import { spawn } from 'node:child_process';
import { createServer } from 'node:net';
import { openSync, existsSync, readFileSync, mkdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import os from 'node:os';
import { createRequire } from 'node:module';

export const REPO = resolve(new URL('../../..', import.meta.url).pathname);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// playwright from this folder's node_modules, else the global npm root (`npm root -g`)
export async function loadPlaywright() {
  try {
    return await import('playwright');
  } catch { /* not installed here */ }
  const root = process.env.NODE_PATH || '/opt/homebrew/lib/node_modules';
  return createRequire(join(root, 'x.js'))('playwright');
}

export function freePort() {
  return new Promise((res, rej) => {
    const s = createServer();
    s.listen(0, '127.0.0.1', () => { const { port } = s.address(); s.close(() => res(port)); });
    s.on('error', rej);
  });
}

async function waitHttp(url, ms = 15000) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    try { if ((await fetch(url)).ok) return; } catch { /* not up yet */ }
    await sleep(200);
  }
  throw new Error(`${url} did not come up`);
}

// Local target. `site` is a built web_dist folder, `data` a folder with game.iso and SCUS_971.97, `server` the wotm_server binary
// (native/net/build/wotm_server, built by `sh tools/test_net.sh notest`); `diagDir` makes the server keep the players' diagnostics reports
// there (--diag-dir, one file per match).
export async function startLocal({ site, data, server, outDir, maxPlayers = 8, diagDir }) {
  for (const [what, p] of [['site', join(site, 'index.html')], ['data', join(data, 'game.iso')], ['server', server]])
    if (!existsSync(p)) throw new Error(`local target: ${what} not found at ${p}`);
  mkdirSync(outDir, { recursive: true });
  const [tcp, ws, web] = [await freePort(), await freePort(), await freePort()];
  const serverLogPath = join(outDir, 'server.log');
  const serverLog = openSync(serverLogPath, 'w');
  const children = [];
  const run = (cmd, args, log) => {
    const c = spawn(cmd, args, { stdio: ['ignore', log, log] });
    children.push(c);
    return c;
  };
  run(server, ['--port', String(tcp), '--ws-port', String(ws), '--players', String(maxPlayers), '--lobbies', '64', ...(diagDir ? ['--diag-dir', diagDir] : [])], serverLog);
  run('python3', [join(REPO, 'tools/serve_web.py'), '--port', String(web), '--site', site, '--data', data], 'ignore');
  await waitHttp(`http://127.0.0.1:${web}/version.json`);
  const pageUrl = `http://127.0.0.1:${web}/?server=ws://127.0.0.1:${ws}`;
  return {
    kind: 'local',
    pageUrl,
    webPort: web,
    wsPort: ws,
    serverLogPath,
    serverLog: () => (existsSync(serverLogPath) ? readFileSync(serverLogPath, 'utf8') : ''),
    buildId: simId(JSON.parse(readFileSync(join(site, 'version.json'), 'utf8'))),
    async stop() {
      for (const c of children) c.kill('SIGTERM');
      await sleep(300);
    },
  };
}

// Live target: a public site needs nothing; a site with a password gate (password given) sets a cookie at its login form that every
// player's context gets. The server's log is not reachable from here, so the client logs are the only witness.
export async function startLive({ url, password = '' }) {
  const base = url.replace(/\/$/, '');
  const cookies = password ? await loginCookies(base, password) : [];
  const version = await (await fetch(`${base}/version.json`, { headers: { cookie: cookies.map((c) => `${c.name}=${c.value}`).join('; ') } })).json();
  return { kind: 'live', pageUrl: `${base}/`, cookies, serverLog: () => '', buildId: simId(version), async stop() {} };
}

// the id the page sends and the server logs (version.json's sim, tools/build_id.py); an older version.json has only its full id
const simId = (version) => version.sim || version.build;

async function loginCookies(base, password) {
  const login = await fetch(`${base}/login`, {
    method: 'POST',
    headers: { 'content-type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ password, confirm: 'on' }),
    redirect: 'manual',
  });
  const raw = login.headers.getSetCookie?.() ?? [];
  if (!raw.length) throw new Error(`live target: login at ${base}/login gave HTTP ${login.status} and no cookie (wrong password?)`);
  const host = new URL(base).hostname;
  return raw.map((c) => {
    const [pair, ...attrs] = c.split(';').map((s) => s.trim());
    const eq = pair.indexOf('=');
    return { name: pair.slice(0, eq), value: pair.slice(eq + 1), domain: host, path: '/', secure: base.startsWith('https'),
      httpOnly: attrs.some((a) => a.toLowerCase() === 'httponly') };
  });
}

// No check makes a sound (Lane uses the machine's audio meanwhile): Chromium-family browsers start with these arguments and Firefox
// with these preferences. The game still plays and mixes its sound, so the audio checks read it from the page.
export const MUTE_ARGS = ['--mute-audio'];
export const MUTE_PREFS = { 'media.volume_scale': '0.0' };

// `opts` for playwright[engine].launch(), muted: chromium (Chrome and Brave included) and firefox; webkit has no switch for it
export function muted(engine, opts = {}) {
  if (engine === 'firefox') return { ...opts, firefoxUserPrefs: { ...(opts.firefoxUserPrefs ?? {}), ...MUTE_PREFS } };
  if (engine === 'chromium') return { ...opts, args: [...(opts.args ?? []), ...MUTE_ARGS] };
  return opts;
}

// launch options of a Chromium-family browser: Playwright's Chromium (software GL), the installed Chrome or Brave (the real GPU)
export function chromiumOptions({ browser = 'chromium', headed = false, unlockFrames = false }) {
  const opts = { headless: !headed, args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader', '--ignore-gpu-blocklist',
    '--autoplay-policy=no-user-gesture-required'] };
  if (browser === 'chrome') {
    opts.channel = 'chrome';
    opts.args = ['--autoplay-policy=no-user-gesture-required'];   // the real GPU
  } else if (browser === 'brave') {   // Brave (WOTM_BRAVE: its binary), a new profile: Shields at their defaults
    opts.executablePath = process.env.WOTM_BRAVE || '/Applications/Brave Browser.app/Contents/MacOS/Brave Browser';
    opts.args = ['--autoplay-policy=no-user-gesture-required'];
  }
  // animation frames at full rate in every window, covered or not (a covered window otherwise gets 30 a second), and in headless
  // Chromium as fast as it can draw: the page reads a display above 60 Hz and the game draws interpolated frames
  if (unlockFrames) {
    opts.args.push('--disable-backgrounding-occluded-windows', '--disable-renderer-backgrounding', '--disable-background-timer-throttling');
    if (browser === 'chromium') opts.args.push('--disable-frame-rate-limit', '--disable-gpu-vsync');
  }
  return muted('chromium', opts);
}

// Browsers: one shared by all players (separate contexts, so separate storage) or one per player (`isolation: 'browser'`).
export async function launchBrowsers(pw, { n, browser = 'chromium', headed = false, isolation = 'context', unlockFrames = false }) {
  const opts = chromiumOptions({ browser, headed, unlockFrames });
  const count = isolation === 'browser' ? n : 1;
  const list = [];
  for (let i = 0; i < count; i++) list.push(await pw.chromium.launch(opts));
  return list;
}

// CPU of every browser process (renderers, GPU, network...) from SystemInfo.getProcessInfo: cumulative seconds, so a sample is the
// difference of two reads over the time between them
export class CpuMeter {
  constructor(browsers) { this.browsers = browsers; this.sessions = []; }

  async start() {
    for (const b of this.browsers) this.sessions.push(await b.newBrowserCDPSession());
    this.t0 = Date.now();
    this.base = await this.read();
  }

  async read() {
    const out = [];
    for (const s of this.sessions) {
      const { processInfo } = await s.send('SystemInfo.getProcessInfo');
      out.push(...processInfo);
    }
    return out;
  }

  // {seconds, byType: {renderer: cores, gpu: cores, ...}, totalCores, perRenderer: [cores...]} since start()
  async stop() {
    const end = await this.read();
    const seconds = (Date.now() - this.t0) / 1000;
    const before = new Map(this.base.map((p) => [p.id, p.cpuTime]));
    const byType = {};
    const perRenderer = [];
    let total = 0;
    for (const p of end) {
      const cores = (p.cpuTime - (before.get(p.id) ?? 0)) / seconds;
      byType[p.type] = (byType[p.type] ?? 0) + cores;
      if (p.type === 'renderer') perRenderer.push(cores);
      total += cores;
    }
    return { seconds, byType, totalCores: total, perRenderer, hostCores: os.cpus().length, loadAvg: os.loadavg() };
  }
}

// Firefox's launch options, muted; `prefs` are more about:config preferences (e.g. javascript.options.wasm_baselinejit: false, to run
// the wasm only in the optimizing tier)
export function firefoxOptions({ headed = false, prefs = {} }) {
  return muted('firefox', { headless: !headed, firefoxUserPrefs: prefs });
}

// Firefox for the mixed lobbies (`--firefox K`: the last K players of every lobby): Playwright's own Firefox build
export async function launchFirefox(pw, { headed = false, prefs = {} }) {
  return pw.firefox.launch(firefoxOptions({ headed, prefs }));
}

// one `NAME=VALUE` about:config preference from the command line: true/false and numbers become those types
export function firefoxPref(spec) {
  const at = spec.indexOf('=');
  if (at <= 0) throw new Error(`a Firefox preference is NAME=VALUE (got '${spec}')`);
  const name = spec.slice(0, at), raw = spec.slice(at + 1);
  const value = raw === 'true' ? true : raw === 'false' ? false : raw !== '' && !Number.isNaN(Number(raw)) ? Number(raw) : raw;
  return [name, value];
}
