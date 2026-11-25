// ABOUTME: Shared setup for the page tools (page_check.mjs, page_shots.mjs): serves a web/ folder with the isolation headers the live
// ABOUTME: site sends, runs a local wotm_server for real lobbies, and launches a muted Chrome, Brave or Firefox, or WebKit (which has no mute switch).
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { existsSync, mkdirSync, openSync, readFileSync } from 'node:fs';
import { extname, join, resolve } from 'node:path';
import { REPO, loadPlaywright, freePort, chromiumOptions, firefoxOptions } from './browser_mp_test/lib/infra.mjs';

export const BROWSERS = ['chrome', 'brave', 'chromium', 'firefox', 'webkit'];
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// the page refuses to start without cross-origin isolation (web/support.js), so the files go out with the two headers the live site sends
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json', '.png': 'image/png', '.webmanifest': 'application/manifest+json' };

// Serves `site` and a lobby server; resolves to { pageUrl, out, stop }. `out` is created.
export async function startPageEnv({ site, out }) {
  site = resolve(site ?? join(REPO, 'web'));
  const server = join(REPO, 'native/net/build/wotm_server');
  for (const [what, p] of [['site', join(site, 'index.html')], ['wotm_server (sh tools/test_net.sh notest)', server]])
    if (!existsSync(p)) throw new Error(`${what} not found at ${p}`);
  mkdirSync(out, { recursive: true });
  const [wsPort, tcpPort, webPort] = [await freePort(), await freePort(), await freePort()];
  const log = openSync(join(out, 'server.log'), 'w');
  const lobbyServer = spawn(server, ['--port', String(tcpPort), '--ws-port', String(wsPort), '--lobbies', '8'], { stdio: ['ignore', log, log] });
  const files = createServer((req, res) => {
    const path = join(site, new URL(req.url, 'http://x').pathname.replace(/\/$/, '/index.html'));
    if (!path.startsWith(site) || !existsSync(path)) { res.writeHead(404).end(); return; }
    res.writeHead(200, { 'Content-Type': TYPES[extname(path)] ?? 'application/octet-stream', 'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp' });
    res.end(readFileSync(path));
  }).listen(webPort, '127.0.0.1');
  return {
    pageUrl: `http://127.0.0.1:${webPort}/?server=ws://127.0.0.1:${wsPort}`,
    out,
    stop: () => { lobbyServer.kill('SIGTERM'); files.close(); },
  };
}

// WebKit has no switch for muting. Nothing here plays sound: the game is never started and the page has no audio of its own.
export async function launchBrowser(name) {
  const pw = await loadPlaywright();
  if (name === 'firefox') return pw.firefox.launch(firefoxOptions({}));
  if (name === 'webkit') return pw.webkit.launch({ headless: true });
  return pw.chromium.launch(chromiumOptions({ browser: name }));
}

export async function webkitDevices() {
  return (await loadPlaywright()).devices;
}

// Puts the loading screen over the page in one of its states, drawn from a LoadingProgress as web/boot.js does (the game is never
// started): 'start' (nothing arrived yet), 'download' (9 of 27 MB at 2 MB/s) or 'cache' (a hit from the disc cache)
export async function showLoadingScreen(page, kind) {
  await page.evaluate((kind) => {
    document.getElementById('front').hidden = true;
    document.getElementById('stage').inert = false;
    const el = document.getElementById('loading');
    el.hidden = false;
    const screen = wotmLoadingScreen(el, { random: () => 0.3 });
    const progress = new LoadingProgress(27 << 20);
    if (kind === 'download') for (let i = 0; i <= 8; i++) progress.add(1 << 20, 1000 + i * 500);
    if (kind === 'cache') progress.addCacheHit(1 << 20, 1000);
    screen.render(progress.view(kind === 'download' ? 5000 : 1000));
  }, kind);
}

export function argv(usage) {
  const arg = (name, fallback) => {
    const i = process.argv.indexOf(`--${name}`);
    return i < 0 ? fallback : process.argv[i + 1];
  };
  if (process.argv.includes('--help') || process.argv.includes('-h')) { console.log(usage); process.exit(0); }
  return arg;
}
