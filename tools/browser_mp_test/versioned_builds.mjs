#!/usr/bin/env node
// ABOUTME: Checks the versioned-build layout (#187) against a local site laid out as the live one: / sends to /b/<build>/, the disc is
// ABOUTME: read from /data/, settings and saves carry across builds, and a page refused by a lobby of another build opens that build.
// Usage: node tools/browser_mp_test/versioned_builds.mjs [--site <web_dist>] [--data <dir>] [--server <wotm_server>] [--out <dir>]
// The site is <web_dist> (tools/build_web.sh) twice under b/: as itself and as a second build with another simulation id, with its
// entry.html as / and a b/index.json naming both. Muted (headless Chromium, --mute-audio). Exit 0 when every check passes.
import { cpSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { REPO, chromiumOptions, loadPlaywright, startLocal } from './lib/infra.mjs';
import { Player, sleep } from './lib/player.mjs';

const arg = (name, def) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : def; };
const dist = arg('--site', join(REPO, 'runtime/web_dist'));
const data = arg('--data', join(REPO, 'runtime/web_data'));
const server = arg('--server', join(REPO, 'native/net/build/wotm_server'));
const out = arg('--out', mkdtempSync(join(tmpdir(), 'versioned_builds_')));

let fails = 0;
const check = (what, ok, detail = '') => { console.log(`${ok ? 'ok    ' : 'FAILED'} ${what}${ok || !detail ? '' : `: ${detail}`}`); if (!ok) fails++; };

// the site: build A is the dist as built, build B the same files with another full and simulation id (a later deploy)
const root = join(out, 'site');
const versionA = JSON.parse(readFileSync(join(dist, 'version.json'), 'utf8'));
const a = versionA.build, b = 'wl-bbbbbbbbbbbbbbbb';
const simB = 'wl-5bbbbbbbbbbbbbbb';
mkdirSync(join(root, 'b'), { recursive: true });
cpSync(dist, join(root, 'b', a), { recursive: true });
cpSync(dist, join(root, 'b', b), { recursive: true });
writeFileSync(join(root, 'b', b, 'version.json'), JSON.stringify({ ...versionA, build: b, sim: simB }) + '\n');
cpSync(join(dist, 'entry.html'), join(root, 'index.html'));
cpSync(join(dist, 'version.json'), join(root, 'version.json'));   // the live root keeps the current build's (the deploy checks read it)
writeFileSync(join(root, 'b', 'index.json'), JSON.stringify({ builds: [
  { full: a, sim: versionA.sim, time: '2026-11-25T10:00:00Z' },
  { full: b, sim: simB, time: '2026-11-25T11:00:00Z' },
] }) + '\n');

const target = await startLocal({ site: root, data, server, outDir: out });
const pw = await loadPlaywright();
const browser = await pw.chromium.launch(chromiumOptions({}));
try {
  const query = new URL(target.pageUrl).search;   // ?server=ws://... (the local lobby)
  const base = `http://127.0.0.1:${target.webPort}`;

  // --measure N [--rtt ms]: N cold loads each way, through / and straight to the build, with Chrome's network emulation adding the
  // round trip to every request; the median time until the panel is ready (its monster list filled)
  const runs = Number(arg('--measure', 0));
  if (runs > 0) {
    const rtt = Number(arg('--rtt', 40));
    const load = async (url) => {
      const c = await browser.newContext();
      const p = await c.newPage();
      const cdp = await c.newCDPSession(p);
      await cdp.send('Network.enable');
      await cdp.send('Network.emulateNetworkConditions', { offline: false, latency: rtt, downloadThroughput: -1, uploadThroughput: -1 });
      const t0 = Date.now();
      await p.goto(url, { waitUntil: 'domcontentloaded' });
      await p.waitForFunction(() => document.getElementById('monster')?.options.length > 0, null, { timeout: 60000 });
      const ms = Date.now() - t0;
      await c.close();
      return ms;
    };
    const median = (v) => [...v].sort((x, y) => x - y)[Math.floor(v.length / 2)];
    const direct = [], entry = [];
    for (let i = 0; i < runs; i++) {   // alternated, so a slower moment of this machine falls on both
      direct.push(await load(`${base}/b/${a}/${query}`));
      entry.push(await load(`${base}/${query}`));
    }
    console.log(`first load to the panel, ${runs} runs each, round trip ${rtt} ms: straight to the build median ${median(direct)} ms ` +
      `(${Math.min(...direct)}-${Math.max(...direct)}), through / median ${median(entry)} ms (${Math.min(...entry)}-${Math.max(...entry)})`);
  }

  // 1. the entry page sends the browser to the build, query kept, and the build's disc reads go to /data/
  const ctx = await browser.newContext();
  const page = await ctx.newPage();
  const urls = [];
  page.on('request', (r) => urls.push(new URL(r.url()).pathname));
  await page.goto(`${base}/${query}#x`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(() => document.getElementById('monster')?.options.length > 0, null, { timeout: 30000 });
  const at = new URL(page.url());
  check('/ opens the build at /b/<build>/', at.pathname === `/b/${a}/`, at.pathname);
  check('with the query and the hash', at.search === query && at.hash === '#x', `${at.search}${at.hash}`);
  await sleep(3000);   // the boot's prefetch of the disc's first chunks
  check('the disc is read from /data/', urls.some((u) => u.startsWith('/data/')), 'no /data/ request');
  check('never from under the build', !urls.some((u) => u.startsWith('/b/') && u.includes('/data/')), urls.filter((u) => u.includes('/data/'))[0]);

  // 2. what the page keeps (localStorage, the private file system, the disc chunks in the Cache API) is the origin's: the next build
  // has it (disc_cache.js names its caches by file and version, not by the page's address)
  const cachesA = await page.evaluate(async () => (await caches.keys()).sort());
  check('the disc cache has chunks after a build\'s first load', cachesA.length > 0, JSON.stringify(cachesA));
  await page.evaluate(async () => {
    localStorage.setItem('wotlane.name', 'Keeper');
    const dir = await navigator.storage.getDirectory();
    const w = await (await dir.getFileHandle('versioned_builds_check.ini', { create: true })).createWritable();
    await w.write('kept=1\n');
    await w.close();
  });
  const pageB = await ctx.newPage();
  await pageB.goto(`${base}/b/${b}/${query}`, { waitUntil: 'domcontentloaded' });
  await pageB.waitForFunction(() => document.getElementById('monster')?.options.length > 0, null, { timeout: 30000 });
  const kept = await pageB.evaluate(async () => {
    const dir = await navigator.storage.getDirectory();
    const file = await (await dir.getFileHandle('versioned_builds_check.ini')).getFile();
    return { name: localStorage.getItem('wotlane.name'), field: document.getElementById('name').value, ini: await file.text() };
  });
  check('localStorage carries across builds', kept.name === 'Keeper' && kept.field === 'Keeper', JSON.stringify(kept));
  check('the private file system carries across builds', kept.ini === 'kept=1\n', JSON.stringify(kept.ini));
  const cachesB = await pageB.evaluate(async () => (await caches.keys()).sort());
  check('and the disc cache: the next build finds the same caches', cachesA.every((k) => cachesB.includes(k)), JSON.stringify(cachesB));
  await ctx.close();

  // 3. a player on build B joining a lobby of build A is refused by the server and opens build A with the code
  const host = await Player.open(browser, { name: 'Ana', index: 0, url: `${base}/b/${a}/${query}` });
  const lobby = await host.host();
  check('the host on build A has a lobby', lobby.joined, lobby.msg);
  const guest = await Player.open(browser, { name: 'Bia', index: 1, url: `${base}/b/${b}/${query}` });
  await guest.page.fill('#joinCode', lobby.code);
  await guest.page.click('#join');
  await guest.page.waitForURL((u) => u.pathname === `/b/${a}/`, { timeout: 20000 }).catch(() => {});
  const moved = new URL(guest.page.url());
  check('the guest moves to build A', moved.pathname === `/b/${a}/`, moved.pathname);
  // the page takes the code from ?join= into the join box and drops it from the address (a reload doesn't join again)
  await guest.page.waitForFunction(() => document.getElementById('joinCode').value !== '', null, { timeout: 20000 }).catch(() => {});
  const code = await guest.page.inputValue('#joinCode');
  check('with the lobby code', code === lobby.code, code);
  // the page remembered the name it tried with, so the invite joins by itself (front.js acceptInvite)
  const joined = await guest.page.waitForFunction(() => !document.getElementById('inLobby').hidden, null, { timeout: 20000 })
    .then(() => true, () => false);
  check('and joins the lobby there by itself', joined, await guest.frontMsg());
  if (joined) await host.waitMembers(2).then(() => check('the host sees two members', true), (e) => check('the host sees two members', false, e.message));
  const link = await host.page.evaluate(() => import('./lobby.js').then((m) => m.inviteLink(location, document.getElementById('code').textContent)));
  check('the invite link names the site root', link === `${base}/?join=${lobby.code}`, link);
} finally {
  await browser.close();
  await target.stop();
}
console.log(fails === 0 ? 'PASSED' : `FAILED (${fails})`);
process.exit(fails === 0 ? 0 : 1);
