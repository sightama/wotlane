// ABOUTME: The checks, grouped in sessions that each form a lobby of N browser players: match (lobby, picks, start, inputs, sync, frame
// ABOUTME: rate, late join, leaving), round (round end, win screen, rematch, quit), stall (frozen player, Away note), creations, single.
import { Player, sleep } from './player.mjs';
import { Run, formLobby, startMatch, waitAll, hasStart, stopped, MONSTER_IDS } from './session.mjs';
import { CpuMeter } from './infra.mjs';
import { hashes, compareHashes, gameFps, lastWaits, desyncs, padButtons, hasLine, roundsEnded, netStart } from './logs.mjs';
import { makeCreations } from './creations.mjs';

// Per player, a key held for the input check and the pad buttons it must show (default keyboard layout, native/wotm_settings.hpp: Space is
// X, E circle, 1 triangle+circle, 2 X+square, G select, the arrows the d-pad)
const INPUT_KEYS = [['Space', 0x4000], ['e', 0x2000], ['1', 0x3000], ['2', 0xc000], ['g', 0x0001], ['ArrowUp', 0x0010], ['ArrowLeft', 0x0080],
  ['ArrowRight', 0x0020]];
const CRASH = /\[CRASH\]|\[watchdog\] FROZE/;
const POINTER_LOCK = /pointer lock/;   // headless browsers refuse the game's pointer lock request: not a game problem

const count = (text, re) => [...text.matchAll(re)].length;

// every tab: no crash lines, no unexpected stop, no page errors; returns the problems
function health(players, { allowStop = false } = {}) {
  const problems = [];
  for (const p of players) {
    if (CRASH.test(p.logText)) problems.push(`${p.name}: ${CRASH.exec(p.logText)[0]}`);
    const why = stopped(p);
    if (why && !allowStop) problems.push(`${p.name}: match stopped: ${why}`);
    const errors = p.errors.filter((e) => !POINTER_LOCK.test(e));
    if (errors.length) problems.push(`${p.name}: page error: ${errors[0]}`);
  }
  return problems;
}

// state hashes equal on every frame the tabs share (and the server saw no DESYNC)
function syncVerdict(run, players, code, { minCommon = 2 } = {}) {
  const maps = players.map((p) => hashes(p.logText));
  const r = compareHashes(maps);
  const serverDesync = run.target.kind === 'local' ? desyncs(run.target.serverLog()).filter((l) => l.includes(`lobby ${code}:`)) : [];
  const fails = [];
  if (r.mismatches.length) fails.push(`hashes differ on frame ${r.mismatches[0].frame}`);
  if (serverDesync.length) fails.push(serverDesync[0]);
  if (r.common < minCommon) fails.push(`only ${r.common} frames compared (the match ran too short or too slowly)`);
  return { ok: !fails.length, detail: fails.length ? fails.join('; ') : `${r.common} frames compared, all equal on ${players.length} tabs` };
}

// A refused join: the server's reason reaches the page (`first`), and it is still what the player reads a moment later (`msg`)
function noteRefusal(run, what, r) {
  run.refusals.push({ what, first: r.first, final: r.msg });
}

function flushRefusals(run, n) {
  const all = run.refusals.splice(0);
  if (!all.length) return;
  const gone = all.filter((r) => r.first && r.first !== r.final);
  run.report.add(n, 'refusal_message_stays', gone.length ? 'fail' : 'pass',
    gone.length ? gone.map((r) => `${r.what}: the server said ${JSON.stringify(r.first)}, the panel then showed ${JSON.stringify(r.final)}`).join('; ')
      : 'every refusal stayed on screen');
}

async function lobbyErrors(run, n, spare, code) {
  const R = run.report;
  const dup = await spare.join(code);   // the spare is called like P1
  // a full lobby (N=8) refuses for being full before it looks at the name
  const dupOk = !dup.joined && (n === run.opts.maxPlayers ? /full/ : /already has that name/).test(dup.first);
  const wrong = await spare.join(code === 'ZZZZ' ? 'YYYY' : 'ZZZZ');
  const wrongOk = !wrong.joined && /No lobby has that code/.test(wrong.first);
  noteRefusal(run, 'duplicate name', dup);
  noteRefusal(run, 'wrong code', wrong);
  await spare.page.fill('#joinCode', 'AB');
  await spare.page.click('#join');
  const short = await spare.frontMsg();
  const shortOk = /4 characters/.test(short);
  R.add(n, 'lobby_errors', dupOk && wrongOk && shortOk ? 'pass' : 'fail',
    `duplicate name: ${JSON.stringify(dup.msg)}; wrong code: ${JSON.stringify(wrong.msg)}; short code: ${JSON.stringify(short)}`);
}

// Leave takes a guest out of the lobby without a reload: the Host/Join choice is back, the others see the member go
async function lobbyLeave(run, n, spare, players, code) {
  const count = (k) => players[0].page.waitForFunction((want) => document.querySelectorAll('#members li').length === want, k, { timeout: 10000 }).then(() => true, () => false);
  await spare.page.fill('#name', 'Spare');
  const joined = await spare.join(code);
  const inOk = joined.joined && await count(n + 1);
  await spare.page.evaluate(() => { window.__sameDocument = true; });
  await spare.page.click('#leave');
  const back = await spare.page.waitForSelector('#choose', { state: 'visible', timeout: 10000 }).then(() => true, () => false);
  const sameDocument = await spare.page.evaluate(() => window.__sameDocument === true);
  const outOk = await count(n);
  const reset = await spare.page.evaluate(() => document.getElementById('inLobby').hidden && ['single', 'host', 'join'].every((id) => !document.getElementById(id).disabled));
  run.report.add(n, 'lobby_leave', inOk && back && sameDocument && outOk && reset ? 'pass' : 'fail',
    `joined: ${inOk}; the choice is back: ${back}; no reload: ${sameDocument}; the others saw it go: ${outOk}; buttons usable again: ${reset}`);
}

// "Copy invite link" gives this page's address with the lobby's code; a friend who opens it finds the code filled in and joins (a new
// browser profile types a name first, one that remembers a name joins by itself), and the address no longer carries the code
async function inviteLink(run, n, players, code) {
  const host = players[0];
  const count = (k) => host.page.waitForFunction((want) => document.querySelectorAll('#members li').length === want, k, { timeout: 15000 }).then(() => true, () => false);
  const guests = [];
  try {
    await host.context.grantPermissions(['clipboard-read', 'clipboard-write']).catch(() => {});
    await host.page.click('#copyInvite');
    const link = await host.page.evaluate(async () => {
      const box = document.getElementById('inviteLink');
      return box.hidden ? navigator.clipboard.readText() : box.value;
    }).catch(() => '');
    const base = new URL(run.target.pageUrl);
    const linkOk = link === `${base.origin}/?join=${code}`;

    const fresh = await run.openPlayer(n, n + 1, { name: 'Invitee', monster: 5, check: 'match', invite: code });
    guests.push(fresh);
    const prefilled = (await fresh.page.inputValue('#joinCode')) === code && /invited/.test(await fresh.frontMsg());
    const urlClean = await fresh.page.evaluate(() => !location.search.includes('join='));
    await fresh.page.fill('#name', 'Invitee');
    await fresh.page.click('#join');
    const freshJoin = await fresh.settle();
    const freshIn = freshJoin.joined && await count(n + 1);

    const known = await run.openPlayer(n, n + 2, { name: 'Remember', monster: 5, check: 'match', invite: code, remembered: true });
    guests.push(known);
    const knownIn = await known.page.waitForFunction(() => !document.getElementById('inLobby').hidden, null, { timeout: 15000 }).then(() => true, () => false) && await count(n + 2);

    run.report.add(n, 'invite_link', linkOk && prefilled && urlClean && freshIn && knownIn ? 'pass' : 'fail',
      `link ${JSON.stringify(link)} right: ${linkOk}; code filled in with a message: ${prefilled}; address cleaned: ${urlClean}; new profile joined after typing a name: ${freshIn} (${JSON.stringify(freshJoin.msg)}); profile with a name joined by itself: ${knownIn}`);
  } finally {
    for (const g of guests) await g.page.click('#leave').catch(() => {});
    await run.closeAll(guests);
    await count(n);
  }
}

// a ninth player at a full lobby: the front panel says so and the others are untouched
async function capCheck(run, n, spare, code, players) {
  await spare.page.fill('#name', 'P9');
  const r = await spare.join(code);
  const ok = !r.joined && /full/.test(r.first);
  noteRefusal(run, 'ninth player', r);
  const intact = (await Promise.all(players.map((p) => p.members()))).every((m) => m.length === n);
  run.report.add(n, 'cap_refused', ok && intact ? 'pass' : 'fail',
    `9th join: ${JSON.stringify(r.msg)}; other tabs still list ${n} members: ${intact}`);
  await spare.page.fill('#name', 'P1');
}

async function picks(run, n, players) {
  const R = run.report;
  if (n < 2) return R.add(n, 'picks', 'skip', 'needs a guest');
  const [host, guest] = players;
  await guest.pickMonster(10);   // Agamo: not one of the default picks
  await host.setSettings({ stage: 2, wins: 3 });
  await sleep(1200);
  const lists = await Promise.all(players.map((p) => p.members()));
  const monsterSeen = lists.every((l) => l.some((m) => m.startsWith(`${guest.name} (Agamo`)));
  const settings = await Promise.all(players.slice(1).map((p) => p.guestSettings()));
  const settingsSeen = settings.every((s) => /Gamblers Gulch/.test(s) && /wins: 3/.test(s));
  await host.setSettings({ stage: 1, wins: 2 });
  await sleep(500);
  R.add(n, 'picks', monsterSeen && settingsSeen ? 'pass' : 'fail',
    `monster change on all tabs: ${monsterSeen}; guests see stage and wins: ${settingsSeen} (${settings[0] ?? ''})`);
}

// every tab shows the player's pad read with a button held: input from each player reaches every tab
async function inputsCheck(run, n, players, evidence) {
  const plan = players.map((p, i) => INPUT_KEYS[i % INPUT_KEYS.length]);
  await Promise.all(players.map((p, i) => p.hold(plan[i][0], 10000)));
  await sleep(3000);
  await Promise.all(players.map((p) => p.pollLog()));
  const missing = [];
  for (const p of players) {
    const seen = padButtons(p.logText);
    const lack = plan.flatMap(([, mask], i) => (((seen.get(i) ?? 0) & mask) === mask ? [] : [i + 1]));
    if (lack.length) missing.push(`${p.name} lacks the held button of players ${lack.join(',')}`);
  }
  const applied = players.filter((p) => hasLine(p.logText, /\[sync\] first non-neutral input applied/)).length;
  run.report.add(n, 'inputs', !missing.length && applied === n ? 'pass' : 'fail',
    missing.length ? missing.join('; ') : `every tab saw each of the ${n} players' own held button (first input applied on ${applied}/${n} tabs)`, evidence);
}

function perfRow(run, n, players, cpu, evidence) {
  const fps = players.map((p) => gameFps(p.logText)).filter((f) => f !== null);
  const waits = players.map((p) => lastWaits(p.logText)?.ms ?? 0);
  const min = Math.min(...fps), avg = fps.reduce((a, b) => a + b, 0) / (fps.length || 1);
  const cores = cpu.hostCores;
  const busy = cpu.totalCores / cores;
  const expected = n >= 3 ? 30 : 60;   // 3 or more players: the game's own 30 Hz mode (the server's step 2)
  const bound = fps.length && min < expected * 0.85 && busy > 0.75;
  const per = cpu.perRenderer.map((c) => c.toFixed(2)).join(',');
  const others = Object.entries(cpu.byType).filter(([t]) => t !== 'renderer').map(([t, c]) => `${t} ${c.toFixed(2)}`).join(', ');
  run.report.add(n, 'perf', 'info',
    `game fps (full speed ${expected}) min ${fps.length ? min.toFixed(1) : '?'} avg ${fps.length ? avg.toFixed(1) : '?'} (lockstep: all tabs run at the slowest pace); ` +
    `browser CPU ${cpu.totalCores.toFixed(2)} of ${cores} cores (renderers ${per}; other processes ${others}); host load ${cpu.loadAvg[0].toFixed(1)}; ` +
    `network waits ${Math.max(...waits)} ms max per tab${bound ? '; LIKELY CPU-BOUND on this machine, not a game bug' : ''}`, evidence);
}

// ---------------------------------------------------------------------------------------------------------------------------------

// Lobby, picks, start, inputs, sync, frame rate, late join, a player leaving and another refreshing
export async function matchSession(run, n) {
  const R = run.report;
  const check = 'match';
  const players = [];
  let spare = null, code = '', evidence = [];
  try {
    players.push(...await run.openPlayers(n, check, { env: { WOTM_PADLOG: String(run.opts.padlogFrames) } }));
    const lobby = await formLobby(players);
    code = lobby.code;
    R.add(n, 'lobby', lobby.ok ? 'pass' : 'fail', lobby.detail);
    if (!lobby.ok) return;
    spare = await run.openPlayer(n, n, { name: 'P1', monster: 5, check });
    spare.name = 'Spare';   // it types P1 (a duplicate name) but its logs and screenshots must not overwrite P1's
    await lobbyErrors(run, n, spare, code);
    if (n === run.opts.maxPlayers) await capCheck(run, n, spare, code, players);
    else await lobbyLeave(run, n, spare, players, code);
    if (n + 2 <= run.opts.maxPlayers) await inviteLink(run, n, players, code);
    await picks(run, n, players);

    const started = await startMatch(players);
    const bad = players.filter((p, i) => netStart(p.logText)?.player !== i + 1 || netStart(p.logText)?.of !== n);
    R.add(n, 'match_start', started.ok && !bad.length ? 'pass' : 'fail',
      started.ok ? (bad.length ? `wrong player numbers on ${bad.map((p) => p.name)}` : `all ${n} tabs started (delay ${netStart(players[0].logText).delay} frames)`)
        : `no start on ${started.missing.join(',')} (${stopped(players.find((p) => started.missing.includes(p.name))) ?? 'timeout'})`);
    if (!started.ok) return;
    for (const p of players) await p.focusGame();

    await inputsCheck(run, n, players, []);
    await sleep(run.opts.warmupSecs * 1000);
    const cpu = new CpuMeter(run.browsers);
    await cpu.start();
    await Promise.all(players.map((p) => p.play(run.opts.playSecs * 1000)));
    const usage = await cpu.stop();
    await Promise.all(players.map((p) => p.pollLog()));
    evidence = await run.saveEvidence(n, check, players, code);
    const sync = syncVerdict(run, players, code);
    R.add(n, 'sync', sync.ok ? 'pass' : 'fail', sync.detail, evidence);
    const problems = health(players);
    R.add(n, 'health', problems.length ? 'fail' : 'pass', problems.join('; ') || 'no crash, stop or page error on any tab');
    perfRow(run, n, players, usage, evidence);

    // late join: the lobby is in a match
    await spare.page.fill('#name', 'Late');
    const late = await spare.join(code);
    noteRefusal(run, 'late join', late);
    R.add(n, 'late_join', !late.joined && /in a match/.test(late.first) ? 'pass' : 'fail', `late join: joined=${late.joined} message ${JSON.stringify(late.msg)}`);

    await leaving(run, n, players, code);
  } catch (e) {
    R.add(n, 'match_error', 'fail', e.stack?.split('\n').slice(0, 2).join(' ') ?? String(e), evidence);
  } finally {
    flushRefusals(run, n);
    await run.saveEvidence(n, check, [...players, ...(spare ? [spare] : [])], code).catch(() => {});
    await run.closeAll([...players, ...(spare ? [spare] : [])]);
  }
}

// the last player closes the tab, the one before refreshes: the others keep playing, the refresher sees the front panel and cannot join
// a lobby that is in a match
async function leaving(run, n, players, code) {
  const R = run.report;
  if (n < 2) return;
  const leaver = players[n - 1];
  const others = players.slice(0, n - 1);
  const before = Math.min(...others.map((p) => hashes(p.logText).size));
  await leaver.close();
  const refresher = n >= 3 ? players[n - 2] : null;
  const stayers = others.filter((p) => p !== refresher);
  if (refresher) await refresher.reload();
  await sleep(20000);
  await Promise.all(stayers.map((p) => p.pollLog()));
  const moved = Math.min(...stayers.map((p) => hashes(p.logText).size)) > before;
  const stops = stayers.filter((p) => stopped(p)).map((p) => `${p.name}: ${stopped(p)}`);
  const stalls = await Promise.all(stayers.map((p) => p.stall()));
  const stuck = stalls.filter((s) => s.ms !== 0);
  const serverSaw = run.target.kind !== 'local' || new RegExp(`lobby ${code}: ${leaver.name} left the match`).test(run.target.serverLog());
  R.add(n, 'leave', moved && !stops.length && !stuck.length && serverSaw ? 'pass' : 'fail',
    `after ${leaver.name} closed its tab: others advanced=${moved}, stopped=[${stops}], waiting=${stuck.map((s) => s.text)}, server logged the leave=${serverSaw}`);
  if (refresher) {
    const front = await refresher.frontVisible();
    const msg = await refresher.frontMsg();
    const again = front ? await refresher.join(code) : { joined: false, msg: '' };
    noteRefusal(run, 'rejoin after refresh', again);
    R.add(n, 'refresh', front && !again.joined && /in a match/.test(again.first) ? 'pass' : 'fail',
      `after reload: front panel ${front}, message ${JSON.stringify(msg)}; rejoin mid-match: joined=${again.joined} ${JSON.stringify(again.first)} (shown after: ${JSON.stringify(again.msg)})`);
  }
}

// ---------------------------------------------------------------------------------------------------------------------------------

// wins=1 and the vencer cheat: round end, the win dialog, PLAY AGAIN (round 2), then QUIT
export async function roundSession(run, n) {
  const R = run.report;
  const check = 'round';
  if (n < 2) return R.add(n, 'round', 'skip', 'the cheat that ends a round knocks out the other players: needs another one');
  const players = [];
  let code = '';
  try {
    players.push(...await run.openPlayers(n, check, { env: { WOTM_CHEATS: 'vencer=8' } }));
    const lobby = await formLobby(players);
    code = lobby.code;
    if (!lobby.ok) return R.add(n, 'round_lobby', 'fail', lobby.detail);
    await players[0].setSettings({ wins: 1 });
    await sleep(800);
    const started = await startMatch(players);
    if (!started.ok) return R.add(n, 'round_start', 'fail', `no start on ${started.missing.join(',')}`);
    await Promise.all(players.map((p) => p.focusGame()));

    const dialogs = (p) => count(p.logText, /\[dialog\] 5 opened/g);
    const first = await waitAll(players, (p) => dialogs(p) >= 1, { timeoutMs: 240000 });
    const ended = players.filter((p) => roundsEnded(p.logText) >= 1).length;
    R.add(n, 'round_end', ended === n ? 'pass' : 'fail', `round 1 ended on ${ended}/${n} tabs`);
    R.add(n, 'win_screen', first.ok ? 'pass' : 'fail', first.ok ? `the win dialog opened on all ${n} tabs` : `no win dialog on ${first.missing.join(',')}`);
    if (!first.ok) return;

    await sleep(1500);
    // PLAY AGAIN is the dialog's first choice: cross once, then again only if round 2 has not ended 30 s later (a second press on round 2's dialog would start round 3)
    let second = { ok: false, missing: [] };
    for (let i = 0; i < 5 && !second.ok; i++) {
      await players[0].hold('cross', 300);
      second = await waitAll(players, (p) => dialogs(p) >= 2, { timeoutMs: 30000 });
    }
    const sync = syncVerdict(run, players, code, { minCommon: 1 });
    R.add(n, 'rematch', second.ok && sync.ok ? 'pass' : 'fail',
      second.ok ? `round 2 ended and its win dialog opened on all tabs; ${sync.detail}` : `no second win dialog on ${second.missing.join(',')}; ${sync.detail}`);
    if (!second.ok) return;

    await sleep(1500);
    for (const k of ['down', 'down', 'cross']) await players[0].tap(k);   // QUIT, then confirm
    await sleep(1200);
    for (const k of ['up', 'cross']) await players[0].tap(k);
    const gone = await waitAll(players, () => false, { timeoutMs: 1 });
    const end = Date.now() + 60000;
    let front = [];
    while (Date.now() < end) {
      front = await Promise.all(players.map((p) => p.frontVisible().catch(() => false)));
      if (front.every(Boolean)) break;
      await sleep(1500);
    }
    R.add(n, 'quit', front.every(Boolean) ? 'pass' : 'fail',
      front.every(Boolean) ? 'QUIT took every tab back to the front panel' : `still in the game: ${players.filter((_, i) => !front[i]).map((p) => p.name)}`);
    await rejoinAfterMatch(run, n, players, code);
    void gone;
  } catch (e) {
    R.add(n, 'round_error', 'fail', e.stack?.split('\n').slice(0, 2).join(' ') ?? String(e));
  } finally {
    await run.saveEvidence(n, check, players, code).catch(() => {});
    await run.closeAll(players);
  }
}

// after the match every tab is back on the front panel; a note naming the old code is a promise, and Join must keep it
async function rejoinAfterMatch(run, n, players, code) {
  const msgs = await Promise.all(players.map((p) => p.frontMsg().catch(() => '')));
  const promised = msgs.every((m) => m.includes(code));
  const r = await players[0].join(code).catch(async (e) => {
    // the panel never settled: say what it shows
    const state = await players[0].page.evaluate(() => ({ msgs: window.__frontMsgs, hostDisabled: document.getElementById('host').disabled,
      joinDisabled: document.getElementById('join').disabled, inLobby: !document.getElementById('inLobby').hidden, text: document.getElementById('frontMsg').textContent })).catch(() => ({}));
    return { joined: false, msg: `${e.name}: the panel did not settle in 15 s; ${JSON.stringify(state)}`, first: '' };
  });
  // every player left the lobby when the game quit, so the server closed it: the panel's promise holds only if the lobby survived
  run.report.add(n, 'rejoin_after_match', !r.joined && (promised || !r.first) ? 'fail' : 'pass',
    `front panels say ${JSON.stringify(msgs[0])}; Join on the old code: joined=${r.joined} ${JSON.stringify(r.first || r.msg)} (shown after: ${JSON.stringify(r.msg)})`);
}

// ---------------------------------------------------------------------------------------------------------------------------------

// A frozen player (its network held, its tab hidden): the others wait and say why, after 60 s the page offers to leave; the player
// comes back and the match goes on in sync; then its connection drops and it gets the way back
export async function stallSession(run, n) {
  const R = run.report;
  const check = 'stall';
  if (n < 2) return R.add(n, 'stall', 'skip', 'needs another player');
  const players = [];
  let code = '';
  try {
    players.push(...await run.openPlayers(n, check));
    const lobby = await formLobby(players);
    code = lobby.code;
    if (!lobby.ok) return R.add(n, 'stall_lobby', 'fail', lobby.detail);
    const started = await startMatch(players);
    if (!started.ok) return R.add(n, 'stall_start', 'fail', `no start on ${started.missing.join(',')}`);
    await Promise.all(players.map((p) => p.focusGame()));
    await sleep(15000);
    const victim = players[n - 1];
    const others = players.slice(0, n - 1);
    const hidden = await victim.hide();
    await sleep(2000);
    await victim.freezeNetwork();

    let waiting = null;
    const end = Date.now() + 40000;
    while (Date.now() < end && !waiting) {
      for (const p of others) {
        const s = await p.stall();
        // other waits are ordinary on a machine that cannot keep up: only a wait for the frozen player counts
        if (s.ms > 0 && s.text.includes(victim.name)) { waiting = { p, ...s }; break; }
      }
      if (!waiting) await sleep(1000);
    }
    R.add(n, 'stall_detected', waiting && waiting.text.includes(victim.name) ? 'pass' : 'fail',
      waiting ? `${waiting.p.name} says ${JSON.stringify(waiting.text)} after ${Math.round(waiting.ms)} ms` : 'no other tab noticed the missing player');
    R.add(n, 'away_note', waiting && /background/.test(waiting.text) ? 'pass' : 'fail',
      waiting ? `wait text ${JSON.stringify(waiting.text)} (tab hidden: ${hidden})` : 'no wait text');

    const promptEnd = Date.now() + 90000;
    let prompt = null;
    while (Date.now() < promptEnd) {
      prompt = await others[0].prompt();
      if (prompt.visible) break;
      await sleep(2000);
    }
    const promptOk = prompt?.visible && prompt.keepWaiting && /Leave the match/.test(prompt.leave);
    R.add(n, 'stall_prompt', promptOk ? 'pass' : 'fail',
      prompt?.visible ? `${JSON.stringify(prompt.text)} with Keep waiting and ${JSON.stringify(prompt.leave)}` : 'no prompt after 90 s of waiting');
    if (promptOk) {
      await others[0].page.click('#keepWaiting');
      await sleep(4000);
      const hidden = !(await others[0].prompt()).visible;
      R.add(n, 'keep_waiting', hidden ? 'pass' : 'fail', hidden ? 'the prompt went away and stayed away' : 'the prompt came back at once');
    }

    await victim.unfreezeNetwork();
    await victim.show();
    // recovered: every tab has printed two new state hashes since (the match is moving on and agreeing), none is in a long wait. A
    // wait of a moment is normal on a machine that cannot keep up, so only a wait over 5 s counts as still stuck.
    const seen = players.map((p) => hashes(p.logText).size);
    const recoveryEnd = Date.now() + 150000;
    let moved = false;
    while (Date.now() < recoveryEnd && !moved) {
      await sleep(3000);
      await Promise.all(players.map((p) => p.pollLog()));
      moved = players.every((p, i) => hashes(p.logText).size >= seen[i] + 2);
    }
    const after = await Promise.all(players.map((p) => p.stall()));
    const stuckTabs = players.filter((p, i) => hashes(p.logText).size < seen[i] + 2).map((p) => p.name);
    const resumed = moved && after.every((s) => s.ms < 5000);
    const sync = syncVerdict(run, players, code);
    R.add(n, 'stall_recovery', resumed && sync.ok ? 'pass' : 'fail',
      `every tab moved on by two hashes: ${moved}${stuckTabs.length ? ' (silent: ' + stuckTabs.join(',') + ')' : ''}; longest wait now ${Math.max(...after.map((s) => s.ms))} ms; ${sync.detail}`);

    await victim.dropConnection();
    let p2 = { visible: false, text: '', leave: '' };
    for (let i = 0; i < 20 && !(p2.visible && /stopped/.test(p2.text)); i++) {
      await sleep(2000);
      p2 = await victim.prompt();
    }
    await victim.pollLog();
    R.add(n, 'connection_lost', p2.visible && /stopped/.test(p2.text) && /Back to the front/.test(p2.leave) ? 'pass' : 'fail',
      `after the socket dropped: prompt ${JSON.stringify(p2.text)}, button ${JSON.stringify(p2.leave)}`);
    await victim.page.click('#leaveMatch');
    await victim.page.waitForFunction(() => !document.getElementById('front').hidden, null, { timeout: 15000 });
    // the last-lobby note comes from a LIST to the server after the panel shows: Join before it lands would take the note for the
    // server's answer to the Join
    await victim.page.waitForFunction(() => document.getElementById('frontMsg').textContent !== '', null, { timeout: 10000 }).catch(() => {});
    const msg = await victim.frontMsg();
    const rejoin = await victim.join(code);
    noteRefusal(run, 'rejoin after dropped connection', rejoin);
    R.add(n, 'reconnect', !rejoin.joined && /in a match/.test(rejoin.first) ? 'pass' : 'fail',
      `way back: front panel says ${JSON.stringify(msg)}; Join mid-match: joined=${rejoin.joined} ${JSON.stringify(rejoin.msg)}`);
  } catch (e) {
    R.add(n, 'stall_error', 'fail', e.stack?.split('\n').slice(0, 2).join(' ') ?? String(e));
  } finally {
    flushRefusals(run, n);
    await run.saveEvidence(n, check, players, code).catch(() => {});
    await run.closeAll(players);
  }
}

// ---------------------------------------------------------------------------------------------------------------------------------

// A created map and skin imported in every tab, picked in the lobby and played; then one tab without them is refused
export async function creationsSession(run, n) {
  const R = run.report;
  const check = 'creations';
  if (!run.opts.disc) return R.add(n, 'creations', 'skip', 'no --disc folder to make a map from (stage_map.py needs it)');
  const files = await makeCreations(run.dir(n, check), run.opts.disc);
  const players = [];
  let code = '';
  try {
    players.push(...await run.openPlayers(n, check, { env: {} }));
    for (const p of players) {
      await p.importZip(files.zip);
      await sleep(800);
      await p.reload();
      await p.page.fill('#name', p.name);
    }
    const skinPick = '3:green';
    const options = await players[0].page.$$eval('#monster option', (o) => o.map((x) => x.value));
    const maps = await players[0].page.$$eval('#lobbyStage option', (o) => o.map((x) => x.value));
    if (!options.includes(skinPick) || !maps.includes(files.mapId)) {
      return R.add(n, 'creations', 'fail', `imported creations are not offered in the lobby pickers (skin ${options.includes(skinPick)}, map ${maps.includes(files.mapId)})`);
    }
    for (const [i, p] of players.entries()) await p.page.selectOption('#monster', i === 0 ? skinPick : String(MONSTER_IDS[i % MONSTER_IDS.length]));
    const lobby = await formLobby(players);
    code = lobby.code;
    if (!lobby.ok) return R.add(n, 'creations', 'fail', lobby.detail);
    const listed = (await players[1 % n].members()).some((m) => /skin green/.test(m));
    await players[0].setSettings({ stage: files.mapId });
    await sleep(1000);
    const started = await startMatch(players);
    if (!started.ok) return R.add(n, 'creations', 'fail', `no start on ${started.missing.join(',')}: ${stopped(players.find((p) => started.missing.includes(p.name))) ?? 'timeout'}`);
    await Promise.all(players.map((p) => p.focusGame()));
    await sleep(45000);
    await Promise.all(players.map((p) => p.pollLog()));
    const lines = players.map((p) => hasLine(p.logText, /\[mods\] skin togera\/green \(costume \d+\) in the fight/) && hasLine(p.logText, /\[stages\] t1: applied \d+ edits/));
    const sync = syncVerdict(run, players, code);
    R.add(n, 'creations', lines.every(Boolean) && sync.ok && listed ? 'pass' : 'fail',
      `skin shown in the member list: ${listed}; skin and map applied on ${lines.filter(Boolean).length}/${n} tabs; ${sync.detail}`);
  } catch (e) {
    R.add(n, 'creations_error', 'fail', e.stack?.split('\n').slice(0, 2).join(' ') ?? String(e));
  } finally {
    await run.saveEvidence(n, check, players, code).catch(() => {});
    await run.closeAll(players);
  }
  if (n === 2) await creationsMissing(run, files);
}

// two players, the second without the creations: the server compares the mods and refuses the match with a reason the page explains
async function creationsMissing(run, files) {
  const R = run.report;
  const check = 'creations_missing';
  const players = [];
  let code = '';
  try {
    players.push(...await run.openPlayers(2, check, { env: {} }));
    await players[0].importZip(files.zip);
    await sleep(800);
    await players[0].reload();
    await players[0].page.fill('#name', players[0].name);
    const lobby = await formLobby(players);
    code = lobby.code;
    if (!lobby.ok) return R.add(2, 'creations_missing', 'fail', lobby.detail);
    await players[0].setSettings({ stage: files.mapId });
    await sleep(1000);
    await players[0].start();
    const done = await waitAll(players, () => false, { timeoutMs: 1 });
    for (let i = 0; i < 80 && !players.some((p) => stopped(p) || hasStart(p)); i++) {
      await sleep(1500);
      await Promise.all(players.map((p) => p.pollLog()));
    }
    done.ok = players.some((p) => stopped(p));
    const reasons = players.map((p) => stopped(p) ?? '');
    const refused = reasons.some((r) => /game configuration differs/.test(r)) && !players.some(hasStart);
    const text = await Promise.all(players.map((p) => p.status()));
    const explained = text.some((s) => /setup differs/.test(s));
    R.add(2, 'creations_missing', done.ok && refused ? (explained ? 'pass' : 'fail') : 'fail',
      `reasons ${JSON.stringify(reasons)}; the page says ${JSON.stringify(text.find((s) => /setup differs/.test(s)) ?? text[1])}`);
  } catch (e) {
    R.add(2, 'creations_missing_error', 'fail', e.stack?.split('\n').slice(0, 2).join(' ') ?? String(e));
  } finally {
    await run.saveEvidence(2, check, players, code).catch(() => {});
    await run.closeAll(players);
  }
}

// ---------------------------------------------------------------------------------------------------------------------------------

// the front panel's single player button: the game boots and runs without a crash
export async function singleSession(run) {
  const R = run.report;
  const check = 'single';
  const players = [];
  try {
    players.push(...await run.openPlayers(1, check));
    await players[0].page.click('#single');
    await sleep(45000);
    await players[0].pollLog();
    const p = players[0];
    const status = await p.status();
    const problems = health(players);
    const booted = hasLine(p.logText, /\[boot\]/) && /Running/.test(status);
    R.add(1, 'single_player', booted && !problems.length ? 'pass' : 'fail', `status ${JSON.stringify(status)}; ${problems.join('; ') || 'no crash or page error'}`);
  } catch (e) {
    R.add(1, 'single_error', 'fail', e.stack?.split('\n').slice(0, 2).join(' ') ?? String(e));
  } finally {
    await run.saveEvidence(1, check, players, '').catch(() => {});
    await run.closeAll(players);
  }
}

export { Run, Player };
