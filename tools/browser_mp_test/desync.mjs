#!/usr/bin/env node
// ABOUTME: Desync matrix for browser lobbies: for each case (one lobby of N players, or several lobbies at once, "5+5") and each trigger
// ABOUTME: script, plays full matches to the win screen and compares every tab's state hashes. Writes results.md and results.json.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import os from 'node:os';
import { parseArgs } from 'node:util';
import { loadPlaywright, startLocal, startLive, launchBrowsers, launchFirefox, REPO } from './lib/infra.mjs';
import { Report } from './lib/report.mjs';
import { Run } from './lib/session.mjs';
import { desyncTrial, SCRIPTS } from './lib/desync.mjs';

const USAGE = `Usage: node tools/browser_mp_test/desync.mjs [options]
  --cases 3,4,5,8+2,5+5    lobbies to run: "N" is one lobby of N players, "A+B" runs lobbies of A and B at the same time (default 3)
  --scenarios a,b,...      ${Object.keys(SCRIPTS).join(', ')} (default baseline,pause)
  --reps K                 trials per case and scenario (default 1); the stage differs per rep
  --mods jump,fast,turbo   modifiers the owner turns on          --firefox K  the last K players of each lobby in Firefox
  --target local|live  --url <url>  --password <pw>  --site <dir>  --data <dir>  --server <file>  --browser chromium|chrome  --headed
  --win-secs 70 (game seconds to the vencer win)  --warmup-secs 15  --width 640  --height 400  --lobby-height 900  --out <dir>
Exit code 0 when no trial failed.
`;
const { values: v } = parseArgs({
  options: {
    cases: { type: 'string', default: '3' }, scenarios: { type: 'string', default: 'baseline,pause' }, reps: { type: 'string', default: '1' },
    mods: { type: 'string', default: '' }, firefox: { type: 'string', default: '0' },
    target: { type: 'string', default: 'local' }, url: { type: 'string', default: 'https://playwotm.com/' },
    password: { type: 'string', default: process.env.WOTM_SITE_PASSWORD ?? '' },
    site: { type: 'string' }, data: { type: 'string' }, server: { type: 'string' }, browser: { type: 'string', default: 'chromium' },
    headed: { type: 'boolean', default: false }, 'win-secs': { type: 'string', default: '70' }, 'warmup-secs': { type: 'string', default: '15' },
    width: { type: 'string', default: '640' }, height: { type: 'string', default: '400' }, 'lobby-height': { type: 'string', default: '900' },
    out: { type: 'string' }, help: { type: 'boolean', default: false },
  },
});
if (v.help) { console.log(USAGE); process.exit(0); }

const runtime = process.env.WOTM_RECOMP_DIR ?? join(REPO, 'runtime');
const stamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
const outDir = resolve(v.out ?? join(runtime, 'desync_matrix', stamp));
mkdirSync(outDir, { recursive: true });
const cases = v.cases.split(',').map((c) => c.split('+').map(Number));
const scenarios = v.scenarios.split(',');
for (const s of scenarios) if (!SCRIPTS[s]) { console.error(`unknown scenario ${s}`); process.exit(2); }
const mods = v.mods.split(',').filter(Boolean);
const opts = {
  maxPlayers: 8, isolation: 'context', video: false, width: Number(v.width), height: Number(v.height), warmupSecs: Number(v['warmup-secs']),
  lobbyHeight: Number(v['lobby-height']), firefoxCount: Number(v.firefox), playSecs: 0, padlogFrames: 0, disc: '',
};

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
const browsers = await launchBrowsers(pw, { n: 1, browser: v.browser, headed: v.headed, isolation: 'context' });
const firefox = opts.firefoxCount ? await launchFirefox(pw, { headed: v.headed }) : null;
const report = new Report({ target: `${target.kind} ${target.pageUrl.split('?')[0]}`, build: target.buildId, browser: `${v.browser} ${browsers[0].version()}${firefox ? `, Firefox ${firefox.version()} for the last ${opts.firefoxCount} of each lobby` : ''}`,
  host: `${os.cpus().length} cores`, started: new Date().toISOString(), cases: v.cases, scenarios: v.scenarios, reps: v.reps, mods: v.mods || 'none' });
const run = new Run({ pw, target, browsers, opts, report, outDir, firefox });
console.log(`Desync matrix: ${report.meta.target} (build ${target.buildId}), results in ${outDir}`);

const rows = [];
try {
  for (const lobbies of cases) {
    for (const scenario of scenarios) {
      for (let rep = 1; rep <= Number(v.reps); rep++) {
        const label = `${lobbies.join('+')} ${scenario} #${rep}`;
        console.log(`== ${label} (load ${os.loadavg()[0].toFixed(1)})`);
        const trials = await Promise.all(lobbies.map((n, i) => desyncTrial(run, {
          n, scenario, rep, tag: lobbies.length > 1 ? `-${'ab'[i]}` : '', mods, winSecs: Number(v['win-secs']),
          // concurrent lobbies need different stages so a seed or stage coincidence is not mistaken for sharing
        }).then((r) => ({ ...r, cases: lobbies.join('+') }))));
        for (const r of trials) {
          rows.push(r);
          console.log(`  ${r.status.toUpperCase()} ${r.cases} lobby ${r.n}${r.tag} stage ${r.stage} seed ${r.seed} fps ${r.fps}: ${r.note}`);
          writeFileSync(join(outDir, 'results.json'), JSON.stringify({ meta: report.meta, rows }, null, 2));
        }
      }
    }
  }
} finally {
  const md = [`# Desync matrix`, '', ...Object.entries(report.meta).map(([k, x]) => `- ${k}: ${x}`), '',
    '| case | lobby | scenario | rep | stage | mods | seed | code | fps | load start/end | result | first difference |', '|---|---|---|---|---|---|---|---|---|---|---|---|',
    ...rows.map((r) => `| ${r.cases} | ${r.n}${r.tag} | ${r.scenario} | ${r.rep} | ${r.stage} | ${r.mods || '-'} | ${r.seed} | ${r.code} | ${r.fps} | ${r.loadStart}/${r.loadEnd} | ${r.status.toUpperCase()} | ${r.first || '-'} |`),
    '', '## Details', '', ...rows.map((r) => `- **${r.cases} lobby ${r.n}${r.tag} ${r.scenario} #${r.rep}** ${r.status.toUpperCase()}: ${r.note} (${r.evidence})`)];
  writeFileSync(join(outDir, 'results.md'), md.join('\n') + '\n');
  writeFileSync(join(outDir, 'results.json'), JSON.stringify({ meta: report.meta, rows }, null, 2));
  for (const b of browsers) await b.close().catch(() => {});
  await firefox?.close().catch(() => {});
  await target.stop();
}
const failed = rows.filter((r) => r.status === 'fail');
console.log(`\n${failed.length ? 'FAILED' : 'PASSED'}: ${rows.length} trials, ${failed.length} failed. ${join(outDir, 'results.md')}`);
process.exit(failed.length ? 1 : 0);
