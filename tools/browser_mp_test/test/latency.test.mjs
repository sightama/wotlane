// ABOUTME: Tests for lib/latency.mjs: the input latency per player from a match's diagnostics file (the DIAG reports wotm_server keeps
// ABOUTME: with --diag-dir) and the delay changes the server logged. Run: node --test tools/browser_mp_test/test/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { latencySummary } from '../lib/latency.mjs';

const lines = [
  '2026-11-18T01:30:48Z src=server lobby=ABCD event=match_start players=2 hz=30',
  '2026-11-18T01:30:52Z src=server lobby=ABCD event=start delay=3 rtt_level_ms=60,70',
  '2026-11-18T01:31:00Z src=client lobby=ABCD player=P1 v=1 why=periodic seq=1 delay=3 ping_ms=61 gfps=30.0 lat_n=10 lat_ms=150.0 lat_p95_ms=180.0 lat_max_ms=190.0 lat_in_ms=8.0 lat_dl_ms=100.0 lat_st_ms=0.0 lat_sim_ms=3.0 lat_pr_ms=12.0 lat_bl_ms=33.0',
  '2026-11-18T01:31:05Z src=client lobby=ABCD player=P1 v=1 why=periodic seq=2 delay=4 ping_ms=63 gfps=29.0 lat_n=30 lat_ms=170.0 lat_p95_ms=200.0 lat_max_ms=260.0 lat_in_ms=10.0 lat_dl_ms=133.0 lat_st_ms=2.0 lat_sim_ms=3.0 lat_pr_ms=14.0 lat_bl_ms=33.0',
  '2026-11-18T01:31:05Z src=client lobby=ABCD player=P2 v=1 why=periodic seq=1 delay=3 ping_ms=70 gfps=29.5',
  '2026-11-18T01:31:02Z src=server lobby=ABCD event=delay player=P1 from=3 to=4 rtt_level_ms=150',
].join('\n');

test('per player: presses, the press-weighted mean of each stage, the worst p95 and max, and the delays seen', () => {
  const s = latencySummary(lines);
  assert.deepEqual(s.players.P1, {
    reports: 2, presses: 40, total: 165, p95: 200, max: 260, sample: 9.5, delay: 124.75, stall: 1.5, sim: 3, present: 13.5, blend: 33,
    delays: [3, 4], pingMs: [61, 63], gfps: [29, 30],
  });
  assert.equal(s.players.P2.presses, 0);
  assert.deepEqual(s.delayChanges, [{ player: 'P1', from: 3, to: 4 }]);
  assert.equal(s.startDelay, 3);
});

test('the total and the sample stage are weighted by the presses that had a page time; a window without any gives them nothing', () => {
  const s = latencySummary([
    '2026-11-18T01:31:00Z src=client lobby=ABCD player=P1 v=1 why=periodic seq=1 lat_n=10 lat_unk=5 lat_ms=150.0 lat_in_ms=10.0 lat_dl_ms=100.0',
    '2026-11-18T01:31:05Z src=client lobby=ABCD player=P1 v=1 why=periodic seq=2 lat_n=4 lat_unk=4 lat_dl_ms=130.0',
  ].join('\n'));
  assert.equal(s.players.P1.presses, 14);
  assert.equal(s.players.P1.total, 150);
  assert.equal(s.players.P1.sample, 10);
  assert.equal(s.players.P1.delay, (100 * 10 + 130 * 4) / 14);
});
