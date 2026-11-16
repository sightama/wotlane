#!/usr/bin/env node
// ABOUTME: Browser multiplayer test: drives N desktop-browser players (1 to 8, plus a ninth that must be refused) through the front
// ABOUTME: panel's lobby and the match on a local wotm_server or the live site, and writes a results table. Exit code 1 on any failure.
import { mkdirSync, existsSync } from 'node:fs';
import { join, resolve } from 'node:path';
import os from 'node:os';
import { parseArgs } from 'node:util';
import { loadPlaywright, startLocal, startLive, launchBrowsers, launchFirefox, REPO } from './lib/infra.mjs';
import { Report } from './lib/report.mjs';
import { Run } from './lib/session.mjs';
import { matchSession, roundSession, stallSession, creationsSession, singleSession, savesSession } from './lib/checks.mjs';

const USAGE = `Usage: node tools/browser_mp_test/run.mjs [options]
  --target local|live      local: start wotm_server and tools/serve_web.py here (default); live: the site at --url
  --players 1,2,...,8      player counts to test (default 1-8; the ninth-player refusal runs with 8)
  --checks a,b,...         sessions: match, round, stall, creations, single, saves (default all; stall and creations only at --deep-n)
  --deep-n 2,4,8           player counts that also run stall (and 2,4 creations) (default 2,4,8)
  --site <dir>             local: a built web_dist (default <runtime>/web_dist)
  --data <dir>             local: a folder with game.iso and SCUS_971.97 (default <runtime>/web_data)
  --server <file>          local: the wotm_server binary (default native/net/build/wotm_server)
  --disc <dir>            extracted disc folder, for the created map (default <repo>/disc if present)
  --url <url>              live: the site (default https://35-185-58-174.sslip.io/)   --password <pw>  live: its password
  --browser chromium|chrome  chromium: headless shell with software GL (default); chrome: the installed Chrome (real GPU)
  --headed                 show the browser windows            --isolation context|browser  one shared browser or one per player
  --firefox K              the last K players of each lobby use Playwright's Firefox (mixed lobbies; needs a build with Firefox support)
  --play-secs 45  --warmup-secs 20  --padlog-frames 480  --width 640  --height 400  --lobby-height 900 (the lobby window; the card scrolls when it is taller)
  --video                  record a video per tab (webm)       --out <dir>  results folder (default <repo>/runtime/browser_mp_test/<time>)
Exit code 0 when no row failed.
`;

const { values: v } = parseArgs({
  options: {
    target: { type: 'string', default: 'local' }, players: { type: 'string', default: '1,2,3,4,5,6,7,8' },
    checks: { type: 'string', default: 'match,round,stall,creations,single,saves' }, 'deep-n': { type: 'string', default: '2,4,8' },
    site: { type: 'string' }, data: { type: 'string' }, server: { type: 'string' }, disc: { type: 'string' },
    url: { type: 'string', default: 'https://35-185-58-174.sslip.io/' }, password: { type: 'string', default: process.env.WOTM_SITE_PASSWORD ?? '' },
    browser: { type: 'string', default: 'chromium' }, headed: { type: 'boolean', default: false }, isolation: { type: 'string', default: 'context' },
    'play-secs': { type: 'string', default: '45' }, 'warmup-secs': { type: 'string', default: '20' }, 'padlog-frames': { type: 'string', default: '480' },
    width: { type: 'string', default: '640' }, height: { type: 'string', default: '400' }, 'lobby-height': { type: 'string', default: '900' }, firefox: { type: 'string', default: '0' }, video: { type: 'boolean', default: false },
    out: { type: 'string' }, help: { type: 'boolean', default: false },
  },
});
if (v.help) { console.log(USAGE); process.exit(0); }

const nums = (s) => s.split(',').filter(Boolean).map(Number);
const stamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
const runtime = process.env.WOTM_RECOMP_DIR ?? join(REPO, 'runtime');
const outDir = resolve(v.out ?? join(runtime, 'browser_mp_test', stamp));
mkdirSync(outDir, { recursive: true });

const opts = {
  maxPlayers: 8, isolation: v.isolation, video: v.video, width: Number(v.width), height: Number(v.height),
  playSecs: Number(v['play-secs']), warmupSecs: Number(v['warmup-secs']), padlogFrames: Number(v['padlog-frames']),
  lobbyHeight: Number(v['lobby-height']), firefoxCount: Number(v.firefox), disc: v.disc ? resolve(v.disc) : (existsSync(join(REPO, 'disc')) ? join(REPO, 'disc') : ''),
};
const counts = nums(v.players);
const sessions = new Set(v.checks.split(','));
const deep = new Set(nums(v['deep-n']));

let target;
if (v.target === 'live') {
  if (!v.password) { console.error('live target: pass --password or set WOTM_SITE_PASSWORD'); process.exit(2); }
  target = await startLive({ url: v.url, password: v.password });
} else {
  target = await startLocal({
    site: resolve(v.site ?? join(runtime, 'web_dist')), data: resolve(v.data ?? join(runtime, 'web_data')),
    server: resolve(v.server ?? join(REPO, 'native/net/build/wotm_server')), outDir,
  });
}

const pw = await loadPlaywright();
const browsers = await launchBrowsers(pw, { n: Math.max(...counts), browser: v.browser, headed: v.headed, isolation: v.isolation });
const firefox = opts.firefoxCount ? await launchFirefox(pw, { headed: v.headed }) : null;
const report = new Report({
  target: `${target.kind} ${target.pageUrl.split('?')[0]}`, build: target.buildId, browser: `${v.browser}${v.headed ? ' headed' : ' headless'} ${browsers[0].version()}${firefox ? `, plus Firefox ${firefox.version()} for the last ${opts.firefoxCount} player(s) of each lobby` : ''}`,
  isolation: v.isolation, host: `${os.cpus().length} cores, load ${os.loadavg()[0].toFixed(1)}`, started: new Date().toISOString(), players: counts.join(','),
});
const run = new Run({ pw, target, browsers, opts, report, outDir, firefox });
console.log(`Browser multiplayer test: ${report.meta.target} (build ${target.buildId}), results in ${outDir}`);

try {
  if (sessions.has('single') && counts.includes(1)) { console.log('== single player'); await singleSession(run); }
  if (sessions.has('saves') && counts.includes(1)) { console.log('== saves tab'); await savesSession(run); }
  for (const n of counts) {
    console.log(`== N=${n}`);
    if (sessions.has('match')) await matchSession(run, n);
    if (sessions.has('round')) await roundSession(run, n);
    if (sessions.has('stall') && deep.has(n)) await stallSession(run, n);
    if (sessions.has('creations') && [2, 4].includes(n) && deep.has(n)) await creationsSession(run, n);
  }
} finally {
  report.write(outDir);
  for (const b of browsers) await b.close().catch(() => {});
  await firefox?.close().catch(() => {});
  await target.stop();
}
const failed = report.failures;
console.log(`\n${failed.length ? 'FAILED' : 'PASSED'}: ${report.rows.length} rows, ${failed.length} failed. ${join(outDir, 'results.md')}`);
process.exit(failed.length ? 1 : 0);
