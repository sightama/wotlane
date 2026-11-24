// ABOUTME: Shared steps of the checks: opening N players, forming a lobby, starting the match and waiting for log lines on every tab,
// ABOUTME: and saving each tab's log and screenshot as evidence. Each check in checks.mjs is built from these.
import { appendFileSync, mkdirSync, writeFileSync } from 'node:fs';
import { join, relative } from 'node:path';
import { randomBytes } from 'node:crypto';
import { Player, sleep } from './player.mjs';
import { diagRecord } from './diaglog.mjs';
import { netStart, netStopped, reloaded, serverLobbyLines, sinceReload } from './logs.mjs';

// the stock monsters in the front panel's order (web/front.js MONSTERS): one each for up to 8 players
export const MONSTER_IDS = [3, 1, 2, 5, 4, 7, 8, 9];

// Each run's players carry a short tag (P1-ab12). On the live site lobbies from earlier runs stay listed, and every run's host used
// to be called P1, so a lobby list row of an earlier run matched this run's host.
export const newRunTag = () => randomBytes(2).toString('hex');
export const playerName = (tag, index) => `P${index + 1}-${tag}`;

export class Run {
  constructor({ pw, target, browsers, opts, report, outDir, firefox = null }) {
    Object.assign(this, { pw, target, browsers, opts, report, outDir, firefox });
    this.tag = newRunTag();
    this.refusals = [];   // refused joins seen by a check, judged by flushRefusals in checks.mjs
  }

  dir(n, check) {
    const d = join(this.outDir, `n${n}`, check);
    mkdirSync(d, { recursive: true });
    return d;
  }

  rel(path) { return relative(this.outDir, path); }

  async openPlayer(n, index, { env = {}, name, monster, video, check, invite, remembered, initScripts }) {
    const firefox = this.firefox && index < n && index >= n - this.opts.firefoxCount;   // the last K players of a lobby (not the spare)
    const browser = firefox ? this.firefox : this.browsers[this.opts.isolation === 'browser' ? index % this.browsers.length : 0];
    return Player.open(browser, {
      name: name ?? playerName(this.tag, index),
      index,
      monster: monster ?? MONSTER_IDS[index % MONSTER_IDS.length],
      url: invite ? `${this.target.pageUrl}${this.target.pageUrl.includes('?') ? '&' : '?'}join=${invite}` : this.target.pageUrl,
      invited: Boolean(invite),
      remembered,
      initScripts,
      onDiag: this.opts.diagOut ? (who, line) => appendFileSync(this.opts.diagOut, diagRecord(new Date(), who, line)) : undefined,
      env,
      cookies: this.target.cookies,
      viewport: { width: this.opts.width, height: this.opts.lobbyHeight },
      gameViewport: { width: this.opts.width, height: this.opts.height },
      videoDir: video ? this.dir(n, check) : undefined,
    });
  }

  // N players, opened one after another (each context downloads the game's data on first use)
  async openPlayers(n, check, { env = {}, video = this.opts.video } = {}) {
    const players = [];
    for (let i = 0; i < n; i++) players.push(await this.openPlayer(n, i, { env, video, check }));
    return players;
  }

  // The tab logs, a screenshot per tab and the server's lines for the lobby, under n<N>/<check>/. Returns the evidence paths.
  async saveEvidence(n, check, players, code) {
    const d = this.dir(n, check);
    const files = [];
    for (const p of players) {
      await p.pollLog();
      writeFileSync(join(d, `${p.name}.log`), p.logText);
      files.push(this.rel(join(d, `${p.name}.log`)));
      if (!p.closed) {
        await p.shot(join(d, `${p.name}.png`));
        files.push(this.rel(join(d, `${p.name}.png`)));
      }
    }
    if (code && this.target.kind === 'local') {
      const lines = serverLobbyLines(this.target.serverLog(), code);
      writeFileSync(join(d, 'server.log'), lines.join('\n') + '\n');
      files.push(this.rel(join(d, 'server.log')));
    }
    return files;
  }

  async closeAll(players) {
    for (const p of players) await p.close();
  }
}

// polls the logs of the players until test(player) is true for every one; resolves {ok, missing: [names]}
export async function waitAll(players, test, { timeoutMs = 120000, everyMs = 1500 } = {}) {
  const end = Date.now() + timeoutMs;
  let missing = players;
  while (Date.now() < end) {
    await Promise.all(missing.map((p) => p.pollLog()));
    missing = missing.filter((p) => !test(p));
    if (!missing.length) return { ok: true, missing: [] };
    await sleep(everyMs);
  }
  return { ok: false, missing: missing.map((p) => p.name) };
}

// this page's match: a page that loaded again (quick rematch) plays a new one
// `afterReload`: only a start line of a page that loaded again counts (a rematch after QUIT: the old match's line is still in the log)
export const hasStart = (p, { afterReload = false } = {}) => (!afterReload || reloaded(p.logText)) && netStart(sinceReload(p.logText)) !== null;

// run in the page (Playwright's waitForFunction): the front panel shows the lobby `code`. #inLobby sits inside #front, so a game
// still covering a hidden front panel must not pass for it. `doc` is the page's document; a test passes its own
export const backInLobbyScript = (code, doc = document) =>
  !doc.getElementById('front').hidden && !doc.getElementById('inLobby').hidden && doc.getElementById('code').textContent.trim() === code;
export const stopped = (p) => netStopped(sinceReload(p.logText));

// Host and join: P1 hosts, the others join one by one. Returns {code, ok, detail}; every tab must list every member
export async function formLobby(players) {
  const [host, ...guests] = players;
  const made = await host.host();
  if (!made.joined) return { ok: false, detail: `host: ${made.msg}` };
  if (!/^[A-Z2-9]{4}$/.test(made.code)) return { ok: false, detail: `bad code ${JSON.stringify(made.code)}` };
  for (const [i, g] of guests.entries()) {
    const r = await g.join(made.code);
    if (!r.joined) return { ok: false, code: made.code, detail: `${g.name} could not join: ${r.msg}` };
    await Promise.all(players.slice(0, i + 2).map((p) => p.waitMembers(i + 2)));
  }
  const want = players.map((p) => p.name).sort();
  for (const p of players) {
    const names = (await p.members()).map((m) => m.split(' ')[0]).sort();
    if (JSON.stringify(names) !== JSON.stringify(want)) return { ok: false, code: made.code, detail: `${p.name} lists ${names.join(',')}` };
  }
  const owner = (await host.members()).filter((m) => m.endsWith(', owner'));
  if (owner.length !== 1 || !owner[0].startsWith(`${host.name} `)) return { ok: false, code: made.code, detail: `owner line: ${owner.join('|')}` };
  return { ok: true, code: made.code, detail: `${players.length} tabs list ${players.length} members, code ${made.code}` };
}

// the owner starts the match and every tab must report its start line; returns the same shape as waitAll
// `afterReload`: a rematch, where the start line must come from the page that loaded again
export async function startMatch(players, { timeoutMs = 180000, afterReload = false } = {}) {
  await players[0].start();
  await Promise.all(players.map((p) => p.toGameViewport()));
  return waitAll(players, (p) => hasStart(p, { afterReload }) || stopped(p), { timeoutMs });
}
