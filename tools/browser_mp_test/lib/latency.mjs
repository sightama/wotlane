// ABOUTME: Reads a match's diagnostics file (wotm_server --diag-dir) into the input latency per player: presses, the stages of the
// ABOUTME: page's lat_* fields (web/diag.js) weighted by presses, the delays and pings seen, and the server's delay changes.

const STAGES = { total: 'lat_ms', sample: 'lat_in_ms', delay: 'lat_dl_ms', stall: 'lat_st_ms', sim: 'lat_sim_ms', present: 'lat_pr_ms', blend: 'lat_bl_ms' };
const TIMED = new Set(['total', 'sample']);   // only presses with a page time have these (lat_unk counts the others)

function fields(line) {
  const out = {};
  for (const word of line.split(' ').slice(1)) {
    const eq = word.indexOf('=');
    if (eq > 0) out[word.slice(0, eq)] = word.slice(eq + 1);
  }
  return out;
}

const range = (values) => (values.length ? [Math.min(...values), Math.max(...values)] : []);

// `text`: the file's lines. Returns {startDelay, delayChanges: [{player, from, to}], players: {name: summary}}; a summary's stages are
// means of the windows' medians weighted by each window's presses (total and sample: by its presses with a page time), ms; p95 and
// max the worst window's
export function latencySummary(text) {
  const players = {};
  const delayChanges = [];
  let startDelay = null;
  for (const line of text.split('\n')) {
    const f = fields(line);
    if (f.src === 'server' && f.event === 'start') startDelay = Number(f.delay);
    if (f.src === 'server' && f.event === 'delay') delayChanges.push({ player: f.player, from: Number(f.from), to: Number(f.to) });
    if (f.src !== 'client' || !f.player || (f.why !== 'periodic' && f.why !== 'end')) continue;
    const p = (players[f.player] ??= { reports: 0, presses: 0, sums: {}, weights: {}, p95: 0, max: 0, delays: new Set(), pings: [], gfps: [] });
    ++p.reports;
    if (f.delay !== undefined) p.delays.add(Number(f.delay));
    if (f.ping_ms !== undefined) p.pings.push(Number(f.ping_ms));
    if (f.gfps !== undefined) p.gfps.push(Number(f.gfps));
    const n = Number(f.lat_n ?? 0);
    if (!(n > 0)) continue;
    p.presses += n;
    const timed = n - Number(f.lat_unk ?? 0);
    for (const [stage, key] of Object.entries(STAGES)) {
      const w = TIMED.has(stage) ? timed : n;
      if (!(w > 0) || f[key] === undefined) continue;
      p.sums[stage] = (p.sums[stage] ?? 0) + w * Number(f[key]);
      p.weights[stage] = (p.weights[stage] ?? 0) + w;
    }
    p.p95 = Math.max(p.p95, Number(f.lat_p95_ms ?? 0));
    p.max = Math.max(p.max, Number(f.lat_max_ms ?? 0));
  }
  const out = {};
  for (const [name, p] of Object.entries(players)) {
    out[name] = { reports: p.reports, presses: p.presses, p95: p.p95, max: p.max, delays: [...p.delays].sort((a, b) => a - b),
      pingMs: range(p.pings), gfps: range(p.gfps) };
    for (const stage of Object.keys(STAGES)) if (p.weights[stage] > 0) out[name][stage] = p.sums[stage] / p.weights[stage];
  }
  return { startDelay, delayChanges, players: out };
}
