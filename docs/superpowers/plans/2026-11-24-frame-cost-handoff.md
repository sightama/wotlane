# Frame cost, VU0 round to nearest and zero waits: handoff (2026-11-24)

The worker on task_9a24b1cf5a16 (worktree `~/orca/workspaces/wotlane/wotlane-rollback`, the wotlane deploy owner) stopped here on the
coordinator's order. Nothing of this worker runs in the background, nothing is uncommitted, and no deploy is half done.

## Live now

| what | commit | build id | checks |
|---|---|---|---|
| server (`wotm_server` on the VM) | wip/browser `0b331cf6` (quick rematch #177/#178) | `wl-8fe2d8e0c133bbdb` at deploy | probe PASS |
| web (playwotm.com) | wip/browser `f1dfb377` (#184, #185, #190, #191 on top) | `wl-49d9dd9d3239c091` | `check_web.sh` PASSED; live smokes by their workers: rematch 6/6, remap 5/5, page UX 30/30 |

The server only compares players within a lobby, so a web build id that differs from the server's does not matter; native players
need a build from the same commit as the browsers to share a lobby with them.

Merged and **not deployed**: #196 (rematch REJOIN race + 120 s lockout, wip/browser `45386f4d`; touches `web/` and `wotm_server`).
Its worker (structworker_1596f9a6) waits for the deploy to run its prod log check and live smoke.

## Open PR: #193, VU0 microprograms round to nearest

Branch `sightama/vu0-round-nearest`, head `39a735cd` (that commit is a merge of wip/browser `45386f4d`, so it carries #196 too).
Lane approved the change and the deletion of the round-toward-zero code.

- **What it does:** VU0 microprograms round to nearest on every build (no MXCSR/FPCR write per call; the browser uses the same SSE
  path as native); `native/wotm_rtz*.hpp` and the `rtz_*` tests are deleted; a madd's float product and sum saturate to the largest
  float (no infinity); a guard (`ensureIeeeDefaults`) sets the thread back to round to nearest with subnormals and logs `[vu0]`, on
  each new VU0 code generation and every 256 lookups; `native/tests/wotm_vu0_test.cpp` (second unit-test executable, with
  `native/tests/vu0_stub` for PS2Recomp's `VU1State`) checks the madd edge cases, the guard, and a digest of every helper on an
  edge-value grid (`0dd7492b4fc44e33`: Mac ARM, wasm, x86 AVX2 under Rosetta, and in CI Linux x86 and Windows MSVC all agree);
  `tools/vu0bench.sh` checks native == wasm on recorded calls (`--save`/`--expect`, `--keep` to save them for another build).
- **Measured (M1 Max, muted):** game thread per frame (`ee_busy_ms`, `latency.mjs --players 2 --browser chrome --firefox 1`, two
  alternating rounds against live): Chrome 7.10/6.70 -> 6.20/6.00 ms (-10 to -13%), Firefox 11.25/10.40 -> 9.70/9.15 ms (-12 to -14%).
  VU0 call in wasm under node 516/505/515 -> 393/330/337 ns. Details: `docs/investigacoes/2026-11-19/performance-audit.md`, last section.
- **Reviews:** 7 fresh-reviewer rounds. Round 1 found a madd infinity (fixed); rounds 2 to 4 the guard's gaps (fixed); round 5 moved the
  edge cases into CI; round 6 found a tie case that reaches infinity without an overflow flag (fixed with a test); round 7 found no bug
  and asked for the edge-value digest (done, `d6d59794`). **Round 8 was started on `39a735cd` and stopped before it reported:** the next
  step is a new fresh reviewer.
- **CI:** green on `d6d59794` (linux, web, windows; the digest line checked in the Linux and Windows logs). On `39a735cd` it was still
  running when this worker stopped.
- **Desync matrix:** passed 9/9 with 0 desyncs on an earlier build (`wl-c81594b81bdbdc4d`, before the review fixes); later runs on the
  host were inconclusive (OVERLOAD at load 30 to 170 from other workers, frames compared were equal). **It has not passed on the final
  code.** Run it when `uptime` shows a load under ~20.
- **Native checks on the final code:** `sh tools/test_native.sh` and `sh tools/test_web.sh` pass; `vu0bench` passed on `63d02ed8`
  (native vs wasm 0 of 840 differ); the native synctest passed with round to nearest (1 vs 1 8 400 frames, 8 monsters 3 000 frames).

### To finish #193 (paste-ready brief for a fresh worker)

> wotlane, PR #193 (branch `sightama/vu0-round-nearest`, head `39a735cd` or later). Read `docs/superpowers/plans/2026-11-24-frame-cost-handoff.md`
> (branch `sightama/handoff-frame-cost`) and the PR. 1) One fresh code review (superpowers:requesting-code-review, a reviewer that sees
> only the diff, `gh pr diff 193`, and the PR text); fix and repeat until a round approves with no bugs. 2) CI green. 3) Build:
> `sh tools/build_runtime.sh`, `sh tools/build_web.sh`, copy `runtime/web_dist` to its own folder; `sh tools/vu0bench.sh` (needs
> Emscripten; must end with 0 differ from the expected build); desync matrix muted with a quiet machine:
> `caffeinate -dis node tools/browser_mp_test/desync.mjs --cases 2,3,4 --scenarios baseline,pause,stall --firefox 1 --site <copy>`
> (0 desync and no OVERLOAD, else rerun the overloaded cases). 4) Merge #193, then deploy server and web together from wip/browser
> (the build id changes; this also ships #196): check no live match first, `sh scripts/deploy_server.sh <commit> <worktree>/native/net/build/wotm_net_probe`
> then `WOTLANE_DATA_MODE=open WOTLANE_DATA_RAW=off sh scripts/deploy_web.sh <dist>` from `~/projects/wotlane-coord`, then
> `CHECK_CHUNKS=1 CHECK_WWW=https://www.playwotm.com CHECK_OLD=https://35-185-58-174.sslip.io CHECK_HTTP=http://playwotm.com sh scripts/check_web.sh https://playwotm.com`.
> 5) Prod log check (VM `journalctl -u wotlane-server` since the deploy, `/var/log/wotlane`, `sh scripts/match_diag.sh <code>` of a
> live match) and a muted live smoke (`node tools/browser_mp_test/run.mjs --target live --players 2 --checks round --browser chrome --firefox 1`);
> tell #196's worker so it runs its own smoke. Rules: CLAUDE.md, no em or en dashes, tests muted, two-reviewer bar, Lane decides
> anything beyond this.

## Remaining work, in priority order

1. **Finish and ship #193 with #196** (brief above).
2. **Zero waits on playwotm.com** (Lane's mandate: the fastest, most responsive browser game a machine can give, even serving the whole
   game all the time). Plan, each item measured before it is built and shipped only for a real gain:
   1. *Whole-game prefetch into the persistent disc cache:* after the first boot, fetch every chunk of `web/match_chunks.json` (262 MiB
      covers every stage, monster and costume; with the boot chunks and the 8 MiB ELF about 270 MiB) into the Cache API store of
      `web/disc_cache.js`, a few at a time, paused while a fight loads; `navigator.storage.persist()`. Measure time to complete at 20 and
      100 Mbps, first fight cold vs prefetched, quota and persist answers per browser. Page only.
   2. *No wasm download or compile after the first visit:* measure what Chrome and Firefox cache today (compile ms per visit, cold and
      warm); if a browser compiles every visit, keep the module in a service worker's Cache Storage and measure again; fetch the next
      build's wasm in the background when the server announces it. The online audit (`docs/investigacoes/2026-11-23/online-experience.md`)
      measured cold starts at 6.1 to 14.6 s to frame 1 with 27 to 67 MB of disc chunks plus 40 MB of wasm.
   3. *Firefox tiering warm-up* (Gemini review item 1, `docs/investigacoes/2026-11-23/gemini-neon-review.md`): call the hottest functions
      at load behind an early return so the optimizing tier has them; compare the first two minutes' `ee_busy_ms` with the default and with
      `wasm_lazy_tiering=false`; if it closes the gap the Firefox tip goes. The audit measured Firefox at 12.9 to 17.7 ms per frame live.
   4. *Input-to-photon floor,* including plan task 5 (present in phase with the display, `lat_pr_ms` 21.4 to 26.9 ms live at 60 Hz). This
      worker claimed task 5 with the online-audit worker (structworker_1596f9a6); the claim is released with this handoff.
   5. *Gemini codegen items* (`__restrict__`, the per-instruction case labels in the 46 biggest functions), A/B in real pages, bit-identical.
      Item B was already measured as noise in the browser; audit row J needs a correction (`lookupFunction` is a flat table).
3. **VU0 on Windows x86 against recorded calls** (`TODO.md` gap): `sh tools/vu0bench.sh --keep <dir>` on the Mac, then `vu0bench.cpp`
   built with MSVC as the runtime builds it with `--expect <dir>/native.results`.
4. **Rollback hybrid** stays paused (issue #30; synctest GO on correctness, payoff limited by frame cost). The issue still carries this
   worker's claim comment for `sightama/rollback-synctest`; it can be removed.

## Decisions

- Lane: VU0 rounds to nearest on every build (not bit-identical to the PS2, identical across clients); the round-toward-zero code is
  deleted; input recordings replay only on a build with the same VU0 code (`docs/ONLINE.md`).
- Lane: every PR goes through fresh-reviewer rounds until one approves with no bugs; every deploy is followed by a prod log check and a
  live smoke; deploys never during a live match; tests and game runs always muted.
- This worker: deploy #193 and #196 together (one build id change instead of two).

## Gotchas

- **The desync matrix needs a quiet machine.** Other workers' browser tests drive the load to 30 to 170; trials then end OVERLOAD
  (inconclusive), and `desync.mjs` still prints "PASSED" because OVERLOAD is not a failure. Read the per-trial lines.
- **Build ids:** any change under `native/` changes the build id, so a "web-only" deploy after a native merge still changes the id
  (it did for #184/#185 because of #182). The server does not care; native friends' builds do.
- **Reviewers share the worktree:** tell them to read with `gh pr diff` or `git show` and to build in their own `git worktree add`; one
  reviewer once edited a file in the shared tree while a web build compiled it, and the build had to be redone.
- **Shell:** the Bash tool runs zsh (no word splitting of `$var`), the repo's hook rejects inline Python and even commands that mix a
  heredoc with `python3` (write a script file and run it), and `sleep` before a command is blocked (use an `until` loop).
- **Native game windows on the Mac** stop making progress when the display sleeps: `vu0bench.sh` and `synctest.sh` wrap the game in
  `caffeinate`; the browser tests run under `caffeinate -dis`.
- **Checking for live matches:** `gcloud compute ssh wotlane-server --zone us-east1-b --project heylane-c6002 --tunnel-through-iap --command "sudo journalctl -u wotlane-server --since '-20 min' --no-pager | grep -c lobby"`.
