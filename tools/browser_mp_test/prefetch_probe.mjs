// ABOUTME: Measures what a whole-game prefetch into the disc cache would cost and give, per browser: the storage quota and persist()
// ABOUTME: answers, the time to fetch every match chunk (web/match_chunks.json), and a first fight's load time cold vs after that fetch.
import { mkdirSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { loadavg } from 'node:os';
import { loadPlaywright, startLocal, startLive, chromiumOptions, firefoxOptions, REPO } from './lib/infra.mjs';
import { sleep } from './lib/player.mjs';

const HELP = `Usage: node tools/browser_mp_test/prefetch_probe.mjs [options]
  --browser chrome|brave|chromium|firefox   (default chrome)
  --mbps N        Chromium family only: throttle the page's network to N Mbit/s down (DevTools network emulation; 0 = none)
  --site <dir>    the web dist (default <repo>/runtime/web_dist)   --data <dir>  the disc (default <repo>/runtime/web_data)
  --fight M,M,S   the direct fight measured (WOTM_MATCH, default 5,7,8: neither the boot's chunks nor a lobby's)
  --live <url>    measure on a public site instead (e.g. https://playwotm.com): its caching headers are the real ones; the local
                  server sends the page with no-store, so there every visit downloads the wasm again
  --flow probe|page  probe (default): this script fetches the whole-game set itself, as a page could; page: the page's own
                  background prefetch (web/game_prefetch.js): a fight opened straight away in a fresh profile (is the first load any
                  slower?), then a fresh profile left on the front panel until the page logs how its prefetch ended, and a fight after it
  --timeline      after each fight, the disc channel per 5 s: game downloads, cache hits, game reads started, background downloads
  --headed        a visible window (headless Chromium reports another quota than a real profile)
Runs on a local server (tools/serve_web.py and wotm_server) or --live, each visit in a fresh browser profile of its own (a disk HTTP
cache like a real one: a throwaway context's cache lives in memory and drops the wasm after the prefetch), muted. Prints:
  storage   quota, usage, persisted() before, and what persist() answered (or 'no answer' after 5 s: a prompt)
  prefetch  MiB and seconds to fetch every chunk of match_chunks.json plus the boot's (the whole-game set), and the usage after
  fight     seconds from opening the fight's page to the fight running ([native] frame lines of PS2X_STATS), with the disc MiB that
            page downloaded: in a fresh context that only saw the front panel first (cold), the same fight again in it (again), and
            in another that also did the prefetch (prefetched)`;

const args = process.argv.slice(2);
if (args.includes('-h') || args.includes('--help')) { console.log(HELP); process.exit(0); }
const opt = (name, dflt) => { const i = args.indexOf(`--${name}`); return i >= 0 ? args[i + 1] : dflt; };
const browserName = opt('browser', 'chrome');
const mbps = Number(opt('mbps', '0'));
const site = opt('site', join(REPO, 'runtime/web_dist'));
const data = opt('data', join(REPO, 'runtime/web_data'));
const fightSpec = opt('fight', '5,7,8');
const headed = args.includes('--headed');
const live = opt('live', '');
const flow = opt('flow', 'probe');
const timeline = args.includes('--timeline');
if (mbps && browserName === 'firefox') { console.error('error: --mbps needs a Chromium-family browser (Firefox has no network emulation)'); process.exit(2); }

const pw = await loadPlaywright();
const outDir = join(REPO, 'runtime/prefetch_probe', new Date().toISOString().replace(/[:.]/g, '-'));
mkdirSync(outDir, { recursive: true });
const target = live ? await startLive({ url: live }) : await startLocal({ site, data, server: join(REPO, 'native/net/build/wotm_server'), outDir });
const engine = browserName === 'firefox' ? pw.firefox : pw.chromium;
const launchOpts = browserName === 'firefox' ? firefoxOptions({ headed }) : chromiumOptions({ browser: browserName, headed });
const profiles = [];
let version = '';

// counts the disc bytes the page and its workers downloaded (numbers on the 'wotlane-disc' channel; cache hits come as {hit})
function discCounter() {
  window.__disc = { bytes: 0, background: 0, events: [] };
  new BroadcastChannel('wotlane-disc').onmessage = (e) => {
    const t = performance.now();
    if (typeof e.data === 'number') { window.__disc.bytes += e.data; window.__disc.events.push([t, 'net', e.data]); }
    else if (typeof e.data?.prefetch === 'number') { window.__disc.background += e.data.prefetch; window.__disc.events.push([t, 'bg', e.data.prefetch]); }
    else if (typeof e.data?.hit === 'number') window.__disc.events.push([t, 'hit', e.data.hit]);
    else if (e.data?.want) window.__disc.events.push([t, 'want', 0]);
  };
}

async function newPage() {
  const dir = mkdtempSync(join(tmpdir(), 'wotm-prefetch-'));
  profiles.push(dir);
  const context = await engine.launchPersistentContext(dir, { ...launchOpts, viewport: { width: 960, height: 600 } });
  version ||= context.browser()?.version() ?? '';
  await context.addInitScript(discCounter);
  const page = await context.newPage();
  // Chromium: what each of the page's requests moved over the wire (compressed), for the per-file lines of a fight
  page.transfers = [];
  page.dataRequests = 0;
  context.on('request', (r) => { if (/\/data\/[0-9a-f]{16}\/\d+\.bin/.test(r.url())) page.dataRequests++; });
  if (browserName === 'firefox') return { context, page };
  const cdp = await context.newCDPSession(page);
  await cdp.send('Network.enable');
  const seen = new Map();   // requestId -> {url, status}
  cdp.on('Network.responseReceived', (e) => seen.set(e.requestId, { url: e.response.url, status: e.response.status, cache: e.response.fromDiskCache }));
  cdp.on('Network.loadingFinished', (e) => {
    const r = seen.get(e.requestId);
    seen.delete(e.requestId);
    if (r) page.transfers.push({ ...r, bytes: e.encodedDataLength });
  });
  if (mbps) {
    await cdp.send('Network.emulateNetworkConditions', { offline: false, latency: 20, downloadThroughput: (mbps * 1e6) / 8, uploadThroughput: (mbps * 1e6) / 8 });
  }
  return { context, page };
}

// the front panel of a first visit, left until the page's own downloads (the wasm, the boot's disc chunks) have settled
async function frontPanel(page) {
  await page.goto(target.pageUrl);
  await page.waitForFunction(() => window.wotmDiscCache && document.getElementById('monster')?.options.length > 0);
  await sleep(mbps ? 60000 : 8000);
}

const MiB = (n) => (n / (1 << 20)).toFixed(0);

async function fight(page) {
  const url = new URL(target.pageUrl);
  url.searchParams.set('WOTM_MATCH', fightSpec);
  url.searchParams.set('WOTM_AIVSAI', '1');
  url.searchParams.set('PS2X_STATS', '1');
  const t0 = Date.now();
  page.transfers.length = 0;
  page.dataRequests = 0;
  await page.goto(url.href);
  while (!(await page.evaluate(() => (window.wotmLog || []).some((l) => /\[native\] frame \d+ redraws=/.test(l))))) {
    if (Date.now() - t0 > 300000) throw new Error('no fight after 300 s');
    await sleep(250);
  }
  // the page's own files over 100 KB on the wire (disc chunks are counted apart)
  const big = page.transfers.filter((x) => x.bytes > 100000 && !/\/data\//.test(x.url))
    .map((x) => `${x.url.replace(/\?.*/, '').replace(/^.*\//, '')} ${x.status}${x.cache ? ' (disk cache)' : ''} ${MiB(x.bytes)} MiB`);
  if (timeline) {
    const { events, log } = await page.evaluate(() => ({ events: window.__disc.events, log: (window.wotmLog || []).filter((l) => /\[boot\]|\[match\]|\[native\] frame|whole-game/.test(l)).slice(0, 12) }));
    const buckets = new Map();
    for (const [at, kind, n] of events) {
      const k = Math.floor(at / 5000) * 5;
      const b = buckets.get(k) ?? { net: 0, hit: 0, want: 0, bg: 0 };
      b[kind] += kind === 'want' ? 1 : n;
      buckets.set(k, b);
    }
    for (const [k, b] of [...buckets].sort((a, c) => a[0] - c[0]))
      lines.push(`  t=${String(k).padStart(4)} s  game net ${MiB(b.net).padStart(3)} MiB, hits ${MiB(b.hit).padStart(3)} MiB, reads started ${String(b.want).padStart(4)}, background ${MiB(b.bg).padStart(3)} MiB`);
    lines.push(...log.map((l) => `  log ${l}`));
  }
  return { secs: (Date.now() - t0) / 1000, bytes: await page.evaluate(() => window.__disc.bytes), big, requests: page.dataRequests };
}

const lines = [`${browserName} on ${live || 'local'}, flow ${flow}${mbps ? `, ${mbps} Mbit/s` : ''}${headed ? ', headed' : ''}; fight ${fightSpec}; load ${loadavg().map((x) => x.toFixed(1)).join(' ')}`];
try {
  if (flow === 'page') await pageFlow();
  else await probeFlow();
} catch (e) {
  lines.push(`error: ${e.message}`);
  process.exitCode = 1;
} finally {
  for (const d of profiles) rmSync(d, { recursive: true, force: true });
  lines[0] += version ? ` (${version})` : '';
  await target.stop();
}
console.log(lines.join('\n'));

async function pageFlow() {
  {
    const { context, page } = await newPage();
    const r = await fight(page);
    lines.push(`fight direct      ${r.secs.toFixed(1)} s, ${MiB(r.bytes)} MiB downloaded by the game (${r.requests} chunk file requests seen)`);
    await context.close();
  }
  const { context, page } = await newPage();
  const t0 = Date.now();
  await page.goto(target.pageUrl);
  if (!(await page.evaluate(() => Boolean(window.wotmGamePrefetch)))) {
    lines.push('page prefetch     none in this build');
    return context.close();
  }
  let how = null;
  while (!(how = await page.evaluate(() => (window.wotmLog || []).map((l) => /whole-game prefetch:? (.*)$/.exec(l)?.[1]).find(Boolean) ?? null))) {
    if (Date.now() - t0 > 900000) throw new Error('the page did not finish its prefetch in 900 s');
    await sleep(500);
  }
  const d = await page.evaluate(() => window.__disc);
  lines.push(`page prefetch     ${how} after ${((Date.now() - t0) / 1000).toFixed(1)} s on the front panel: ${MiB(d.background)} MiB in the background, ${MiB(d.bytes)} MiB other downloads`);
  const r = await fight(page);
  lines.push(`fight after it    ${r.secs.toFixed(1)} s, ${MiB(r.bytes)} MiB downloaded by the game (${r.requests} chunk file requests seen)`);
  await context.close();
}

async function probeFlow() {
  // cold: a fresh context opens the front panel (the wasm and the boot's chunks come down, as on any first visit) and then the fight
  {
    const { context, page } = await newPage();
    await frontPanel(page);
    const r = await fight(page);
    lines.push(`fight cold        ${r.secs.toFixed(1)} s, ${MiB(r.bytes)} MiB downloaded; page files: ${r.big.join(', ') || 'none over 100 KB'}`);
    // the same fight again in that profile: what a second visit gets from the disc cache today
    const again = await fight(page);
    lines.push(`fight again       ${again.secs.toFixed(1)} s, ${MiB(again.bytes)} MiB downloaded; page files: ${again.big.join(', ') || 'none over 100 KB'}`);
    await context.close();
  }
  // prefetched: another fresh context fetches the whole-game set from the front panel, then opens the same fight
  const { context, page } = await newPage();
  await frontPanel(page);
  const storage = await page.evaluate(async () => {
    const s = navigator.storage;
    const est = s?.estimate ? await s.estimate() : {};
    const before = s?.persisted ? await s.persisted() : null;
    const answer = s?.persist ? await Promise.race([s.persist(), new Promise((r) => setTimeout(() => r('no answer'), 5000))]) : null;
    return { quota: est.quota ?? 0, usage: est.usage ?? 0, before, answer };
  });
  lines.push(`storage           quota ${MiB(storage.quota)} MiB, usage ${MiB(storage.usage)} MiB, persisted ${storage.before}, persist() ${storage.answer}`);
  const pre = await page.evaluate(async () => {
    const table = await (await fetch('match_chunks.json')).json();
    const boot = await (await fetch('boot_chunks.json')).json();
    const iso = new Set(boot['game.iso']);
    for (const l of Object.values(table.stages)) l.forEach((c) => iso.add(c));
    for (const m of Object.values(table.monsters)) { m.all.forEach((c) => iso.add(c)); Object.values(m.costumes).forEach((l) => l.forEach((c) => iso.add(c))); }
    const t0 = performance.now();
    const before = window.__disc.bytes;
    await Promise.all([
      window.wotmDiscCache.prefetch(fetch, 'data/game.iso', [...iso], { maxRun: 8, concurrency: 4 }),
      window.wotmDiscCache.prefetch(fetch, 'data/SCUS_971.97', boot['SCUS_971.97'], { maxRun: 8, concurrency: 1 }),
    ]);
    const est = navigator.storage?.estimate ? await navigator.storage.estimate() : {};
    return { chunks: iso.size + boot['SCUS_971.97'].length, secs: (performance.now() - t0) / 1000, bytes: window.__disc.bytes - before, usage: est.usage ?? 0 };
  });
  lines.push(`prefetch          ${pre.chunks} chunks in ${pre.secs.toFixed(1)} s, ${MiB(pre.bytes)} MiB downloaded (${((pre.bytes * 8) / 1e6 / pre.secs).toFixed(0)} Mbit/s), usage after ${MiB(pre.usage)} MiB`);
  const r = await fight(page);
  lines.push(`fight prefetched  ${r.secs.toFixed(1)} s, ${MiB(r.bytes)} MiB downloaded; page files: ${r.big.join(', ') || 'none over 100 KB'}`);
  await context.close();
}
