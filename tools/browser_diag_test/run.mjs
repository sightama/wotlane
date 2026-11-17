// ABOUTME: Checks the page's diagnostics end to end in a real Chromium: a lobby on a local wotm_server, a match start (no game is loaded),
// ABOUTME: and what reaches the server's diagnostics files: the join, periodic and last reports, and a Send report from the front page.
// Run: node tools/browser_diag_test/run.mjs   (needs sh tools/test_net.sh notest first, and Playwright: see tools/browser_mp_test/README.md)
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, readdirSync, existsSync, statSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repo = join(here, '..', '..');
const require = createRequire(import.meta.url);
let playwright;
try { playwright = require('playwright'); } catch { playwright = require(join(execFileSync('npm', ['root', '-g']).toString().trim(), 'playwright')); }

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

const diagDir = mkdtempSync(join(tmpdir(), 'wotm_diag_'));
const ports = { tcp: 20000 + Math.floor(Math.random() * 2000), ws: 22000 + Math.floor(Math.random() * 2000) };
const srv = spawn(server, ['--port', String(ports.tcp), '--ws-port', String(ports.ws), '--diag-dir', diagDir, '--lobbies', '2'], { stdio: 'ignore' });
await new Promise((resolve) => setTimeout(resolve, 600));

const read = () => readdirSync(diagDir).sort().map((f) => `# ${f}\n${readFileSync(join(diagDir, f), 'utf8')}`).join('\n');
const until = async (text, ms = 15000) => {
  for (let t = 0; t < ms; t += 100) {
    if (read().includes(text)) return;
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`never found "${text}" in the diagnostics files:\n${read()}`);
};
let failures = 0;
const check = (name, ok, detail = '') => { console.log(`${ok ? 'ok     ' : 'FAILED '} ${name}${ok ? '' : ': ' + detail}`); if (!ok) ++failures; };

const browser = await playwright.chromium.launch();
try {
  const page = await (await browser.newContext()).newPage();
  const url = `http://127.0.0.1:${sitePort}/?server=ws://127.0.0.1:${ports.ws}`;
  await page.goto(url);
  await page.fill('#name', 'Tester');
  await page.click('#host');
  await page.waitForSelector('#inLobby:not([hidden])');
  await page.fill('#chatText', 'a secret plan');
  await page.click('#chatSend');
  await until('why=join');
  check('a join report with the device', /src=client lobby=\w{4} player=Tester v=1 why=join build=\S+ .*ua=\w*Chrome-\d+ /.test(read()), read());
  await page.click('#start');
  await until('event=match_start');
  await until('why=periodic seq=2', 20000);
  const periodic = read().split('\n').filter((l) => l.includes('why=periodic'));
  check('periodic reports every 5 s with the match numbers', periodic.length >= 2 && /seq=1 build=\S+ hz=60 players=1 up_s=5 /.test(periodic[0]), periodic[0]);
  check('the page frame timing is in them', /raf_p95_ms=\d/.test(periodic[0]) && /late=\d/.test(periodic[0]), periodic[0]);
  await page.evaluate(() => window.wotmDiagFinish());
  await until('why=end');
  const end = read().split('\n').find((l) => l.includes('why=end'));
  check('a last report with the device', /why=end seq=3 .*ua=\w*Chrome-\d+/.test(end), end);
  check('no chat text in any file', !read().includes('secret'), read());
  check('no user agent string', !read().includes('Mozilla'), read());

  await page.reload();
  await page.waitForSelector('#openReport:not([hidden])');
  await page.click('#openReport');
  await page.fill('#reportNote', 'it stuttered after 30 s');
  await page.click('#sendReport');
  await page.waitForFunction(() => document.getElementById('reportMsg').textContent === 'Report sent. Thank you.');
  const files = readdirSync(diagDir);
  const loose = files.find((f) => f.startsWith('reports-'));
  const looseText = loose ? readFileSync(join(diagDir, loose), 'utf8') : '';
  check('a hand-sent report from the front page is filed apart as a claim, with the last match numbers and the note',
    /src=client_outside claimed_lobby=\w{4} claimed_player=Tester v=1 why=report build=\S+ hz=60 players=1 up_s=\d+.* ua=\w*Chrome-\d+.* note="it stuttered after 30 s"/.test(looseText), looseText);
  await page.screenshot({ path: join(diagDir, 'report.png') });
} finally {
  await browser.close();
  srv.kill();
  site.close();
}
console.log(failures ? `${failures} failure(s); files in ${diagDir}` : `all ok (files in ${diagDir})`);
process.exit(failures ? 1 : 0);
