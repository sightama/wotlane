// ABOUTME: Measures the input latency of an online match on this computer: N browser tabs on a local wotm_server, each behind its own
// ABOUTME: latency proxy link (round trip, jitter, spikes), pressing a button now and then; prints each player's latency stages.
import { mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { spawn } from 'node:child_process';
import { join } from 'node:path';
import { REPO, freePort, launchBrowsers, launchFirefox, loadPlaywright, startLocal } from './lib/infra.mjs';
import { Player, sleep } from './lib/player.mjs';
import { formLobby, startMatch, MONSTER_IDS } from './lib/session.mjs';
import { latencySummary } from './lib/latency.mjs';

const USAGE = `Usage: node tools/browser_mp_test/latency.mjs [options]
  --players 4              tabs in the match (2 to 8; 3 or more play at 30 Hz)
  --rtt 60,60,60,70        each player's round trip to the server, ms (one value: everyone)
  --jitter 15              jitter of every link, ms (spread evenly around the round trip)
  --spike 4:6:160:400      player 4 (1-based) gets +160 ms for 400 ms every 6 s (repeatable)
  --secs 90                seconds of play once every tab has started
  --press-ms 700           mean time between one tab's presses (each a 120 ms tap of Space, the cross button)
  --browser chromium|chrome|brave  --firefox K (the last K tabs use Playwright's Firefox)  --headed  --unlock-frames
  --site <dir>  --data <dir>  --server <file>   as in run.mjs (defaults: <repo>/runtime/web_dist, <repo>/runtime/web_data,
                                                native/net/build/wotm_server)
  --env NAME=VALUE         a game environment variable for every tab (repeatable; test only, as run.mjs adds WOTM_PADLOG)
  --http-mbps M            limit each Chromium tab's downloads to M megabits/s once its page has loaded (the disc as a slow line gets it)
  --lobby-secs S           wait S seconds in the formed lobby before the start (players idle there while the others join)
  --out <dir>              results (default <repo>/runtime/latency/<time>): summary.json, the diag files, each tab's log (P1.log...),
                           server.log, proxy.log
The game's AI plays every seat (WOTM_AIVSAI=1), so the match runs on its own; the presses only feed the latency probe (they reach
the game like any press). Run under caffeinate -dis on a Mac (display sleep freezes tabs).`;

function parseArgs(argv) {
  const o = { players: 4, rtt: [60], jitter: 15, spikes: [], secs: 90, pressMs: 700, browser: 'chromium', firefox: 0, headed: false,
    unlockFrames: false, env: {}, httpMbps: 0, lobbySecs: 0, site: join(REPO, 'runtime/web_dist'), data: join(REPO, 'runtime/web_data'), server: join(REPO, 'native/net/build/wotm_server'),
    out: join(REPO, 'runtime/latency', new Date().toISOString().replace(/[:.]/g, '-')) };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i], v = () => argv[++i];
    if (a === '--help' || a === '-h') { console.log(USAGE); process.exit(0); }
    else if (a === '--players') o.players = Number(v());
    else if (a === '--rtt') o.rtt = v().split(',').map(Number);
    else if (a === '--jitter') o.jitter = Number(v());
    else if (a === '--spike') o.spikes.push(v());
    else if (a === '--secs') o.secs = Number(v());
    else if (a === '--press-ms') o.pressMs = Number(v());
    else if (a === '--browser') o.browser = v();
    else if (a === '--firefox') o.firefox = Number(v());
    else if (a === '--headed') o.headed = true;
    else if (a === '--unlock-frames') o.unlockFrames = true;
    else if (a === '--site') o.site = v();
    else if (a === '--data') o.data = v();
    else if (a === '--server') o.server = v();
    else if (a === '--out') o.out = v();
    else if (a === '--http-mbps') o.httpMbps = Number(v());
    else if (a === '--lobby-secs') o.lobbySecs = Number(v());
    else if (a === '--env') { const [k, ...rest] = v().split('='); o.env[k] = rest.join('='); }
    else { console.error(`unknown option ${a}\n${USAGE}`); process.exit(2); }
  }
  if (!(o.players >= 2 && o.players <= 8)) { console.error('--players: 2 to 8'); process.exit(2); }
  return o;
}

// one latency proxy for every tab (tools/net_lagproxy.py --tcp): link i listens on ports[i]
async function startProxy(o, wsPort, logPath) {
  const ports = [];
  for (let i = 0; i < o.players; i++) ports.push(await freePort());
  const args = [join(REPO, 'tools/net_lagproxy.py'), '--tcp', '--to', `127.0.0.1:${wsPort}`];
  ports.forEach((port, i) => args.push('--link', `${port}:${o.rtt[Math.min(i, o.rtt.length - 1)]}:${o.jitter}`));
  for (const s of o.spikes) {
    const [player, ...rest] = s.split(':');
    args.push('--spike', `${ports[Number(player) - 1]}:${rest.join(':')}`);
  }
  const proc = spawn('python3', args, { stdio: ['ignore', 'pipe', 'pipe'] });
  let log = '';
  proc.stdout.on('data', (d) => { log += d; });
  proc.stderr.on('data', (d) => { log += d; });
  const end = Date.now() + 10000;
  while ((log.match(/listening/g) ?? []).length < o.players) {
    if (Date.now() > end) throw new Error(`latency proxy did not start:\n${log}`);
    await sleep(100);
  }
  return { ports, stop: () => { writeFileSync(logPath, log); proc.kill('SIGTERM'); } };
}

// limits a Chromium page's downloads to `mbps` (DevTools network emulation; no added latency: the proxy links model the round trip)
async function throttle(page, mbps) {
  const cdp = await page.context().newCDPSession(page);
  await cdp.send('Network.enable');
  await cdp.send('Network.emulateNetworkConditions', { offline: false, latency: 0, downloadThroughput: (mbps * 1e6) / 8, uploadThroughput: -1 });
}

// taps Space at random intervals around `meanMs` until `until`
async function pressLoop(player, meanMs, until) {
  await player.focusGame();
  while (Date.now() < until && !player.closed) {
    await player.page.keyboard.down('Space').catch(() => {});
    await sleep(120);
    await player.page.keyboard.up('Space').catch(() => {});
    await sleep(meanMs * (0.5 + Math.random()));
  }
}

const fmt = (x) => (x === undefined ? '-' : x.toFixed(1));

async function main() {
  const o = parseArgs(process.argv);
  mkdirSync(o.out, { recursive: true });
  const diagDir = join(o.out, 'diag');
  const pw = await loadPlaywright();
  const target = await startLocal({ site: o.site, data: o.data, server: o.server, outDir: o.out, diagDir });
  const proxy = await startProxy(o, target.wsPort, join(o.out, 'proxy.log'));
  const [chromium] = await launchBrowsers(pw, { n: o.players, browser: o.browser, headed: o.headed, unlockFrames: o.unlockFrames });
  const firefox = o.firefox > 0 ? await launchFirefox(pw, { headed: o.headed }) : null;
  const players = [];
  try {
    for (let i = 0; i < o.players; i++) {
      const url = `http://127.0.0.1:${target.webPort}/?server=ws://127.0.0.1:${proxy.ports[i]}&WOTM_AIVSAI=1`;
      players.push(await Player.open(i >= o.players - o.firefox ? firefox : chromium,
        { name: `P${i + 1}`, index: i, monster: MONSTER_IDS[i], url, env: o.env, viewport: { width: 640, height: 900 }, gameViewport: { width: 640, height: 400 } }));
    }
    if (o.httpMbps > 0) await Promise.all(players.filter((p, i) => i < o.players - o.firefox).map((p) => throttle(p.page, o.httpMbps)));
    const lobby = await formLobby(players);
    if (!lobby.ok) throw new Error(`lobby: ${lobby.detail}`);
    if (o.lobbySecs > 0) await sleep(o.lobbySecs * 1000);
    const started = await startMatch(players);
    if (!started.ok) throw new Error(`match did not start on ${started.missing.join(', ')}`);
    console.log(`match ${lobby.code} started; ${o.secs} s of presses`);
    const until = Date.now() + o.secs * 1000;
    await Promise.all(players.map((p) => pressLoop(p, o.pressMs, until)));
    await Promise.all(players.map((p) => p.page.evaluate(() => window.wotmDiagFinish?.()).catch(() => {})));
    await sleep(1000);
  } finally {
    for (const p of players) {
      await p.pollLog();
      writeFileSync(join(o.out, `${p.name}.log`), p.logText);
      await p.close().catch(() => {});
    }
    await chromium?.close().catch(() => {});
    await firefox?.close().catch(() => {});
    proxy.stop();
    await target.stop();
  }
  const files = readdirSync(diagDir, { recursive: true }).map(String).filter((f) => f.endsWith('-match.log'));
  const text = files.map((f) => readFileSync(join(diagDir, f), 'utf8')).join('\n');
  const summary = { options: o, ...latencySummary(text) };
  writeFileSync(join(o.out, 'summary.json'), JSON.stringify(summary, null, 2));
  console.log(`start delay ${summary.startDelay}, ${summary.delayChanges.length} delay changes`);
  console.log("each stage: the mean of the windows' medians weighted by presses; p95 and max: the worst window's");
  console.log('player  presses  total  p95  max | sample delay stall sim present | blend | delays  ping ms  gfps');
  for (const [name, p] of Object.entries(summary.players))
    console.log(`${name.padEnd(7)} ${String(p.presses).padStart(7)}  ${fmt(p.total)}  ${fmt(p.p95)}  ${fmt(p.max)} | ${fmt(p.sample)} ${fmt(p.delay)} ${fmt(p.stall)} ` +
      `${fmt(p.sim)} ${fmt(p.present)} | ${fmt(p.blend)} | ${p.delays.join(',')}  ${p.pingMs.join('-')}  ${p.gfps.join('-')}`);
  console.log(`results: ${o.out}`);
}

main().catch((e) => { console.error(e.stack || e.message); process.exit(1); });
