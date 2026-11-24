// ABOUTME: Sets up the Emscripten module for the browser version: the ELF and disc paths inside the streamed /disc, the runtime's
// ABOUTME: environment, the page log and status line, and the focus the keyboard needs. Loaded before wotm.js.
'use strict';

(function () {
  const t0 = performance.now();
  const logEl = document.getElementById('log');
  const statusEl = document.getElementById('status');
  const canvas = document.getElementById('canvas');
  wotmGuardPointerLock(canvas);
  const lines = [];
  window.wotmLog = lines;

  let bootStatus = null;

  function tabStorage() {
    try { return sessionStorage; } catch { return null; }
  }

  function out(text) {
    const line = ((performance.now() - t0) / 1000).toFixed(2) + 's ' + text;
    lines.push(line);
    logEl.textContent += line + '\n';
    // a disc read that failed for good, or a match refused for a setup that differs (log_status.js)
    const status = window.wotmLogStatus(text);
    const next = bootStatus ? bootStatus.onLog(text, status) : status;
    if (next)
      statusEl.textContent = next;
  }

  document.getElementById('logToggle').addEventListener('click', () => logEl.classList.toggle('open'));
  // the canvas and its bar go fullscreen together (Esc leaves); the canvas keeps the game's shape
  document.getElementById('fullscreen').addEventListener('click', () => {
    document.getElementById('stage').requestFullscreen?.().catch(() => {});
    canvas.focus();
  });
  canvas.addEventListener('click', () => canvas.focus());

  // the runtime's environment: the disc comes from /disc and saves go to /save (wotm_webfs.cpp); movies are skipped (no FFmpeg in
  // the browser)
  const env = {
    PS2X_CD_IMAGE: '/disc/game.iso',
    PS2X_SKIP_MOVIES: '1',
    PS2X_SKIP_CMOVIE: '1',
    PS2X_SCENE: '1',   // the native renderer draws the game (as runtime/run_live.sh sets it)
    // lens flares test occlusion by reading the depth buffer, which WebGL2 can't read back (and each read stalls on the GPU)
    WOTM_NOFLARE: '1',
    WOTM_SAVEDIR: '/save',
  };
  const params = new URLSearchParams(location.search);
  const fixed = { ...env };   // pageEnv() keeps these over anything from the address or an online match

  // the text over the game while the disc streams in (data_retry.js reports each chunk from the worker that fetches it)
  function showLoading(expectedBytes, initial) {
    const loadingEl = document.getElementById('loading');
    const progress = new LoadingProgress(expectedBytes, { initial });
    const channel = new BroadcastChannel('wotlane-disc');
    channel.onmessage = (e) => {
      if (typeof e.data === 'number') progress.add(e.data, performance.now());
      else if (e.data?.hit) progress.addCacheHit(e.data.hit, performance.now());
    };
    loadingEl.textContent = progress.text(performance.now());
    loadingEl.hidden = false;
    let finished = false;
    const finish = () => {
      if (finished) return;
      finished = true;
      progress.finish();
      loadingEl.hidden = true;
      clearInterval(timer);
      showStreaming(channel);
    };
    const timer = setInterval(() => {
      const text = progress.text(performance.now());
      if (text) return void (loadingEl.textContent = text);
      finish();
    }, 250);
    return finish;
  }

  // after boot, a stage or monster not cached yet makes the game wait on its download, on a black screen (issue #112): a small note
  // says data is loading while a burst of network chunks goes on (cache hits don't count: nobody waits for them)
  function showStreaming(channel) {
    const noteEl = document.getElementById('streaming');
    const activity = new StreamActivity();
    channel.onmessage = (e) => { if (typeof e.data === 'number') activity.add(e.data, performance.now()); };
    setInterval(() => {
      const text = activity.text(performance.now());
      noteEl.hidden = !text;
      if (text) noteEl.textContent = text;
    }, 250);
  }

  // the game quit from its own menu (wotm_webfs.cpp wotmWebQuit): back to the front panel
  // after the save folder's last changes reach the browser's storage (a save folder in memory: wotm_websave.cpp)
  window.wotmQuit = () => {
    syncSaves();
    Promise.all([globalThis.wotmSaveQueue, finishDiag()]).finally(() => location.reload());
  };

  // an online match's last diagnostics report goes out before the page reloads (web/front.js sets wotmDiagFinish for a match)
  function finishDiag() {
    try { return Promise.resolve(window.wotmDiagFinish?.()); } catch { return Promise.resolve(); }
  }

  // writes what changed in a save folder kept in memory to the browser's storage now (a no-op on the OPFS backend)
  function syncSaves() {
    try { window.Module?._wotm_web_save_sync?.(); } catch (e) { out('[web] saving failed: ' + e.message); }
  }

  // the game hears when the tab is hidden: an online match keeps running and tells the others why it may lag (wotm_webfs.cpp)
  function tellHidden() {
    window.Module._wotm_web_set_hidden?.(document.hidden ? 1 : 0);
    if (document.hidden) syncSaves();   // a hidden tab gets no animation frames, which is where the game's saves are synced
  }

  // after a long wait for another player, the page offers to keep waiting or leave the match (back to the front panel)
  function watchStalls() {
    const prompt = new StallPrompt(60000);
    const el = document.getElementById('stall');
    const waitMs = () => window.Module._wotm_web_net_stall_ms?.() ?? 0;
    document.getElementById('keepWaiting').addEventListener('click', () => {
      prompt.keepWaiting(waitMs());
      el.hidden = true;
      canvas.focus();
    });
    document.getElementById('leaveMatch').addEventListener('click', () => finishDiag().finally(() => location.reload()));
    setInterval(() => {
      const wait = waitMs();
      const show = prompt.show(wait);
      if (show && el.hidden) {
        const text = window.Module.UTF8ToString(window.Module._wotm_web_net_stall_text());
        const stopped = prompt.stopped(wait);   // refused, desync or connection lost: only the way back is left
        document.getElementById('stallText').textContent = stopped ? `The match stopped: ${text}.` : text + '.';
        document.getElementById('stallNote').hidden = stopped;
        document.getElementById('keepWaiting').hidden = stopped;
        document.getElementById('leaveMatch').textContent = stopped ? 'Back to the front page' : 'Leave the match';
      }
      el.hidden = !show;
    }, 500);
  }

  // the display's refresh rate, measured while the front panel is idle and cached in sessionStorage: above 60 Hz single player draws
  // in-between frames (WOTM_DISPLAY_HZ, native/runtime/wotm_live.inc). A hidden tab gets no animation frames, so after 2 s the rate is
  // unknown and the game draws at 60. Online matches disable frame interpolation, so they do not wait for this measurement.
  const cachedHz = window.wotmReadCachedRefresh?.(tabStorage()) || 0;
  const displayRate = cachedHz
    ? (env.WOTM_DISPLAY_HZ = String(cachedHz), out(`[web] display ${cachedHz} Hz (cached)`), Promise.resolve())
    : Promise.race([
        wotmMeasureRefresh(60),
        new Promise((resolve) => setTimeout(() => resolve({ hz: 0, raw: 0 }), 2000)),
      ]).then((r) => {
        if (r.hz) {
          env.WOTM_DISPLAY_HZ = String(r.hz);
          window.wotmWriteCachedRefresh?.(tabStorage(), r.hz);
          out(`[web] display ${r.raw.toFixed(1)} Hz, read as ${r.hz}`);
        }
      });

  // also from the page's load: the game's wasm into the browser's HTTP cache, so that wotm.js's own request is only a revalidation
  // (the server sends it with an ETag and no-cache). The game starts once this is done; a failure only means wotm.js downloads it
  const wasmWarm = fetch('wotm.wasm').then((r) => r.arrayBuffer()).catch(() => {});

  // from the page's load on, while the player is on the front panel: the disc chunks every boot reads (boot_chunks.json, from
  // tools/web_boot_chunks.py) come down in parallel into the disc cache (disc_cache.js), whose other versions are dropped first.
  // The game starts once this is done; a failure only means the game reads those chunks itself
  const discWarm = (async () => {
    const dc = window.wotmDiscCache;
    if (!dc) return;
    try {
      const files = ['SCUS_971.97', 'game.iso'];
      const heads = await Promise.all(files.map((f) => fetch('data/' + f, { method: 'HEAD' })));
      if (window.caches) await dc.evictOld(window.caches, Object.fromEntries(files.map((f, i) => [f, heads[i].headers.get('etag')])));
      const list = await (await fetch('boot_chunks.json')).json();
      await Promise.all(files.map((f) => dc.prefetch(fetch, 'data/' + f, list[f] || [])));
    } catch (e) {
      out('[web] disc prefetch skipped: ' + e.message);
    }
  })();

  // loads the game once (front.js calls it for single player, or with a lobby's match environment); `build` is version.json's
  // build id, which the server compares with the lobby's
  let started = false;
  window.wotmStart = function (matchEnv, build, { otherNames = [] } = {}) {
    if (started) return;
    started = true;
    wotmInstallGameInput({ win: window, canvas, running: () => started });   // Tab is the game's; the idle mouse cursor hides
    // held for as long as this tab lives: another tab's Settings and Saves see that a game is running (web/gamelock.js)
    navigator.locks?.request('wotlane-game', { mode: 'shared' }, () => new Promise(() => {}));
    Object.assign(env, pageEnv(params, fixed, matchEnv, build));
    const isOnline = !!(matchEnv || env.WOTM_NET);
    bootStatus = new BootStatus({ online: isOnline, otherNames });
    statusEl.textContent = bootStatus.text;
    window.Module = {
      canvas,
      arguments: ['/disc/SCUS_971.97'],
      print: out,
      printErr: out,
      setStatus: (text) => {
        const next = bootStatus.setEmscriptenStatus(text);
        if (next) statusEl.textContent = next;
      },
      preRun: [() => Object.assign(window.Module.ENV, env)],
      onAbort: (what) => { statusEl.textContent = bootStatus.onAbort(what); logEl.classList.add('open'); },
      onRuntimeInitialized: tellHidden,
    };
    const finishLoading = showLoading(isOnline ? 57 << 20 : 27 << 20, bootStatus.text);   // a first visit's download to a fight, or to the title
    // a match's stage and monsters: what the lobby has not fetched yet comes down while the game boots (web/match_prefetch.js)
    const match = matchEnv && window.wotmMatchPrefetch?.matchOfEnv(matchEnv.WOTM_MATCH);
    if (match) window.wotmMatchPrefetch.warmer.want(match.stage, match.members, 0);
    if (isOnline) {
      // once frame 1 is reached, the fight is on screen: stop match prefetch, clear the loading overlay, and mark Running
      const loaded = setInterval(() => {
        const frame = window.Module?._wotm_web_net_stat?.(0) ?? 0;
        if (!(frame > 0)) return;
        const next = bootStatus.onFrame(frame);
        if (next) statusEl.textContent = next;
        finishLoading();
        if (match) window.wotmMatchPrefetch.warmer.stop();
        clearInterval(loaded);
      }, 200);
    }
    document.addEventListener('visibilitychange', tellHidden);
    // a key going down is the input latency probe's press time (wotm_web_input_at, native/wotm_webfs.cpp; pads report theirs in
    // touch.js); a key held down repeats its keydown, which is no new press
    window.addEventListener('keydown', (e) => {
      if (!e.repeat) window.Module?._wotm_web_input_at?.(performance.timeOrigin + e.timeStamp);
    }, true);
    window.addEventListener('pagehide', syncSaves);
    if (isOnline) watchStalls();
    // the game reads its mods at start, so the repo's built-in mods are in place first
    const ready = [syncMods(), mainThreadFiles(), discWarm, wasmWarm];
    if (!isOnline) ready.push(displayRate);
    Promise.all(ready).finally(() => {
      const script = document.createElement('script');
      script.src = 'wotm.js';
      document.body.appendChild(script);
    });
    canvas.focus();
  };

  // where a worker's fetch and OPFS calls stall while the main thread waits (support.js mainThreadFiles: Firefox), the game keeps
  // its save folder in memory (native/wotm_websave.cpp) and reads the disc in ranges itself (native/wotm_webdisc.cpp), which needs
  // each disc file's size first
  async function mainThreadFiles() {
    if (!window.wotmMainThreadFiles()) return;
    env.WOTM_SAVE_MEMORY = '1';
    try {
      const sizes = await Promise.all(['SCUS_971.97', 'game.iso'].map(async (name) => {
        const r = await fetch('data/' + name, { method: 'HEAD' });
        if (!r.ok) throw new Error(`data/${name}: HTTP ${r.status}`);
        let size = window.wotmFileSize(r.headers.get('content-length'), null);
        if (size === null) {   // a server that sends no length on HEAD: one byte's range carries the total
          const p = await fetch('data/' + name, { headers: { Range: 'bytes=0-0' } });
          size = window.wotmFileSize(null, p.headers.get('content-range'));
        }
        if (size === null) throw new Error(`data/${name}: no size`);
        return `${name}:${size}`;
      }));
      env.WOTM_DISC_SIZES = sizes.join(',');   // copied into the game's environment when it loads (preRun)
    } catch (e) {
      out('[web] could not read the disc sizes: ' + e.message);
    }
  }

  // mirrors the built-in mods served with the page into the browser's mods folder (web/builtin_mods.js); a failure is logged and
  // the game starts anyway (with whatever mods are there)
  async function syncMods() {
    try {
      const r = await fetch('mods/manifest.json', { cache: 'no-store' });
      if (!r.ok) return;
      const fetchBytes = async (path) => {
        const f = await fetch('mods/' + path.split('/').map(encodeURIComponent).join('/'));
        if (!f.ok) throw new Error(`mods/${path}: HTTP ${f.status}`);
        return new Uint8Array(await f.arrayBuffer());
      };
      const copied = await wotmSyncBuiltinMods(await navigator.storage.getDirectory(), await r.json(), fetchBytes);
      if (copied) out(`[mods] built-in mods updated (${copied} files)`);
    } catch (e) {
      out('[mods] built-in mods not updated: ' + e.message);
    }
  }
  // a test link with a direct match skips the front panel
  window.wotmDirect = params.has('WOTM_MATCH');
})();
