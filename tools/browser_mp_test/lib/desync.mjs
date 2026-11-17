// ABOUTME: One desync trial: a lobby of N browser players plays a scripted trigger (personal pause by different players, refresh, leave,
// ABOUTME: Away, a held network) to the win screen, then every tab's state hashes and the server's DESYNC lines are compared.
import { writeFileSync } from 'node:fs';
import os from 'node:os';
import { join } from 'node:path';
import { sleep } from './player.mjs';
import { formLobby, startMatch, waitAll, stopped } from './session.mjs';
import { hashes, compareHashes, desyncs, gameFps, lastWaits, serverLobbyLines, netStart } from './logs.mjs';

const count = (text, re) => [...text.matchAll(re)].length;
const frames = (p) => hashes(p.logText).size;
const CRASH = /\[CRASH\]|\[watchdog\] FROZE/;

// Waits until every given tab has printed more hash lines than before (the match is moving), or the timeout; returns whether it moved
async function moved(players, timeoutMs) {
  const before = players.map(frames);
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    await Promise.all(players.map((p) => p.pollLog()));
    if (players.every((p, i) => frames(p) > before[i])) return true;
    await sleep(1000);
  }
  return false;
}

// everyone plays for `ms`, except `who`, who runs `action` meanwhile
async function playWhile(players, who, ms, action) {
  await Promise.all(players.map((p) => (p === who ? action() : p.play(ms))));
}

// Start opens the pauser's own menu, closed again with circle; the match runs on and this player sends neutral input meanwhile
async function personalMenu(p) {
  await p.tap('start');
  await sleep(1500);
  await p.tap('circle');
}

// Start, then PAUSE MATCH FOR EVERYONE (the list's third line: down, down, X): the game's own shared pause opens on every tab
async function pauseEveryone(p) {
  await p.tap('start');
  await sleep(600);
  await p.tap('down');
  await p.tap('down');
  await p.tap('cross');
}

// Another player presses Start in the shared pause, then the player who paused picks CONTINUE (X). The pause loop counts frames too, so
// hash lines advancing do not show that the fight runs: the caller's win dialog does.
async function resume(players, pauser, other) {
  await other.tap('start');
  await sleep(2000);
  await pauser.tap('cross');
  return `${other.name} pressed Start, ${pauser.name} chose CONTINUE`;
}

// Each script gets {players, n, note}: note(text) records what it did. Scripts run after the warm-up.
export const SCRIPTS = {
  baseline: async () => {},
  // the #35 suspect: different players pause for everyone in turn, another player resumes, with personal menus in between
  pause: async ({ players, n, note }) => {
    for (let k = 0; k < 3; k++) {
      const pauser = players[(1 + k) % n], other = players[(2 + k) % n];
      await playWhile(players, pauser, 6000, () => personalMenu(pauser));
      await playWhile(players, pauser, 3000, () => pauseEveryone(pauser));
      await sleep(3000);
      note(`pause ${k + 1}: ${pauser.name} paused for everyone; resumed by ${await resume(players, pauser, other)}`);
      await Promise.all(players.map((p) => p.play(4000)));
    }
  },
  // personal menus only, several players, some overlapping
  personal: async ({ players, n, note }) => {
    for (let k = 0; k < 4; k++) {
      const a = players[k % n], b = players[(k + 1) % n];
      await Promise.all(players.map((p) => (p === a ? personalMenu(a) : p === b ? (sleep(700).then(() => personalMenu(b))) : p.play(5000))));
    }
    note('personal menus opened and closed by every player in turn');
  },
  // a pause for everyone and, while it is open, another player's tab goes hidden (Away) and comes back
  pause_away: async ({ players, n, note }) => {
    const pauser = players[1 % n], away = players[n - 1];
    await playWhile(players, pauser, 3000, () => pauseEveryone(pauser));
    await away.hide();
    await sleep(8000);
    await away.show();
    note(`${pauser.name} paused for everyone; ${away.name} hidden for 8 s inside it; resumed by ${await resume(players, pauser, players[0])}`);
  },
  away: async ({ players, n, note }) => {
    const v = players[n - 1];
    const how = await v.hide();
    await sleep(12000);
    await v.show();
    note(`${v.name} hidden 12 s (${how})`);
    await Promise.all(players.map((p) => p.play(5000)));
  },
  // a held network for a few seconds, then everything delivered in order
  stall: async ({ players, n, note }) => {
    const v = players[1 % n];
    await v.freezeNetwork();
    await sleep(6000);
    await v.unfreezeNetwork();
    note(`${v.name}'s network held 6 s`);
    await Promise.all(players.map((p) => p.play(5000)));
  },
  refresh: async ({ players, n, note, gone }) => {
    const v = players[n - 1];
    await Promise.all(players.map((p) => (p === v ? sleep(1) : p.play(5000))));
    await v.reload();
    gone.add(v.name);
    note(`${v.name} reloaded its tab mid-match`);
  },
  leave: async ({ players, n, note, gone }) => {
    const v = players[n - 1];
    await Promise.all(players.map((p) => (p === v ? sleep(1) : p.play(5000))));
    await v.close();
    gone.add(v.name);
    note(`${v.name} closed its tab mid-match`);
  },
  // everything in one match: a pause, an Away, a stall, then a player leaves
  mix: async (ctx) => {
    await SCRIPTS.pause({ ...ctx, note: ctx.note });
    await SCRIPTS.away(ctx);
    await SCRIPTS.stall(ctx);
    if (ctx.n > 3) await SCRIPTS.leave(ctx);
  },
};

// One trial. `tag` keeps concurrent lobbies' files apart. Returns the result row.
export async function desyncTrial(run, { n, scenario, rep, tag = '', stage, mods = [], env = {}, winSecs = 70, winTimeoutMs = 360000 }) {
  const check = `${scenario}-r${rep}${tag}`;
  const row = { n, scenario, rep, tag, stage: stage ?? '', mods: mods.join('+'), code: '', seed: '', status: 'fail', note: '', first: '', fps: '', loadStart: os.loadavg()[0].toFixed(1) };
  const players = [];
  const notes = [];
  const gone = new Set();
  try {
    players.push(...await run.openPlayers(n, check, { env: { WOTM_CHEATS: `vencer=${winSecs}`, ...env } }));
    const lobby = await formLobby(players);
    row.code = lobby.code;
    if (!lobby.ok) { row.note = `lobby: ${lobby.detail}`; return row; }
    if (stage === undefined) {   // a stock stage chosen from the rep so repeats differ
      const all = await players[0].page.$$eval('#lobbyStage option', (o) => o.map((x) => x.value).filter((v) => /^\d+$/.test(v)));
      stage = all[(rep * 3 + n) % all.length];
    }
    row.stage = stage;
    await players[0].setSettings({ stage, wins: 1, mods });
    await sleep(1000);
    const started = await startMatch(players);
    if (!started.ok) { row.note = `no start on ${started.missing.join(',')}: ${stopped(players.find((p) => started.missing.includes(p.name))) ?? 'timeout'}`; return row; }
    const seedLine = serverLobbyLines(run.target.serverLog(), lobby.code).find((l) => /seed \d+/.test(l));
    row.seed = /seed (\d+)/.exec(seedLine ?? '')?.[1] ?? 'unknown (live server log not readable)';
    await Promise.all(players.map((p) => p.focusGame()));
    await sleep(run.opts.warmupSecs * 1000);
    await SCRIPTS[scenario]({ players, n, gone, note: (t) => notes.push(t) });

    const alive = players.filter((p) => !gone.has(p.name));
    const won = await waitAll(alive, (p) => count(p.logText, /\[dialog\] 5 opened/g) >= 1, { timeoutMs: winTimeoutMs });
    await sleep(2000);
    await Promise.all(players.map((p) => p.pollLog()));

    // the tabs that stayed are compared on every frame they share; a tab that left only on the frames it saw (any mismatch counts)
    const cmpAlive = compareHashes(alive.map((p) => hashes(p.logText)));
    const cmpAll = compareHashes(players.map((p) => hashes(p.logText)));
    const cmp = { common: cmpAlive.common, mismatches: [...cmpAll.mismatches, ...cmpAlive.mismatches].sort((a, b) => a.frame - b.frame) };
    const named = cmpAll.mismatches.length ? players : alive;
    const serverDesync = desyncs(run.target.serverLog()).filter((l) => l.includes(`lobby ${lobby.code}:`));
    const crash = players.filter((p) => CRASH.test(p.logText));
    const stops = alive.filter((p) => stopped(p));
    const fps = alive.map((p) => gameFps(p.logText)).filter((f) => f !== null);
    const minFps = fps.length ? Math.min(...fps) : null;
    row.fps = minFps === null ? '?' : minFps.toFixed(1);
    const waitMs = Math.max(0, ...alive.map((p) => lastWaits(p.logText)?.ms ?? 0));
    const bad = [];
    if (cmp.mismatches.length) {
      const m = cmp.mismatches[0];
      row.first = `hash of frame ${m.frame}: ${m.hashes.map((h, i) => `${(m.hashes.length === named.length ? named : players)[i].name}=${h.slice(0, 8)}`).join(' ')}`;
      bad.push(`hashes differ on frame ${m.frame}`);
    }
    if (serverDesync.length) { row.first += (row.first ? ' | ' : '') + serverDesync[0]; bad.push('server DESYNC'); }
    if (crash.length) bad.push(`crash on ${crash.map((p) => p.name)}`);
    if (stops.length) bad.push(`stopped: ${stops.map((p) => `${p.name} ${stopped(p)}`)}`);
    if (!won.ok) bad.push(`no win dialog on ${won.missing.join(',')} within ${winTimeoutMs / 1000} s`);
    if (cmp.common < 2) bad.push(`only ${cmp.common} frames compared`);
    const expected = n >= 3 ? 30 : 60;
    const cpuBound = minFps !== null && minFps < expected * 0.85 && os.loadavg()[0] > os.cpus().length;
    row.desync = cmp.mismatches.length > 0 || serverDesync.length > 0;
    // no state difference, but the host load (start or end) was over twice the cores: the watchdog and the timeouts measure the machine
    const starved = !row.desync && bad.length > 0 && Math.max(Number(row.loadStart), os.loadavg()[0]) > 2 * os.cpus().length;
    row.status = starved ? 'overload' : bad.length ? 'fail' : 'pass';
    row.note = `${cmp.common} frames compared on ${alive.length} tabs; ${notes.join('; ')}${bad.length ? `; ${starved ? 'INCONCLUSIVE (host overloaded)' : 'PROBLEM'}: ` + bad.join('; ') : ''}; waits ${waitMs} ms${cpuBound ? '; machine overloaded (load above cores, fps below 85%)' : ''}`;
    return row;
  } catch (e) {
    row.note = `${e.stack?.split('\n').slice(0, 2).join(' ') ?? e}${notes.length ? '; ' + notes.join('; ') : ''}`;
    return row;
  } finally {
    row.loadEnd = os.loadavg()[0].toFixed(1);
    const d = run.dir(n, check);
    for (const p of players) {
      await p.pollLog().catch(() => {});
      writeFileSync(join(d, `${p.name}.log`), p.logText);
    }
    if (run.target.kind === 'local' && row.code) writeFileSync(join(d, 'server.log'), serverLobbyLines(run.target.serverLog(), row.code).join('\n') + '\n');
    row.evidence = run.rel(d);
    row.start = netStart(players[0]?.logText ?? '') ?? null;
    await run.closeAll(players);
  }
}
