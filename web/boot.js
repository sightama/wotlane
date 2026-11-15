// ABOUTME: Sets up the Emscripten module for the browser version: the ELF and disc paths inside the streamed /disc, the runtime's
// ABOUTME: environment, the page log and status line, and the focus the keyboard needs. Loaded before wotm.js.
'use strict';

(function () {
  const t0 = performance.now();
  const logEl = document.getElementById('log');
  const statusEl = document.getElementById('status');
  const canvas = document.getElementById('canvas');
  const lines = [];
  window.wotmLog = lines;

  function out(text) {
    const line = ((performance.now() - t0) / 1000).toFixed(2) + 's ' + text;
    lines.push(line);
    logEl.textContent += line + '\n';
    // a disc read that still failed after data_retry.js's tries: the game waits for it, so tell the player
    if (/\(ISO\).* FAILED/.test(text))
      statusEl.textContent = 'Lost the connection to the game server. Check your connection and reload the page.';
    // the server compares one hash of the match settings and the mods (creations included), so it can't say which part differs
    if (/rejected by the server: game configuration differs/.test(text))
      statusEl.textContent = "Your game setup differs from the host's (creations or settings). Make sure you both have the same " +
        'creations imported (Your creations, on the front panel), then join again.';
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
  // a few harmless switches from the address, for tests and direct matches (?WOTM_MATCH=5,1,9&WOTM_AIVSAI=1). Nothing that names a
  // file, a server or a cheat: a link someone sends must not be able to point the game elsewhere. The fixed values above always win.
  const fromAddress = ['WOTM_MATCH', 'WOTM_AIVSAI', 'PS2X_STATS'];
  const params = new URLSearchParams(location.search);
  const allowed = {};
  for (const key of fromAddress)
    if (params.has(key)) allowed[key] = params.get(key);
  const fixed = { ...env };

  // the text over the game while the disc streams in (data_retry.js reports each chunk from the worker that fetches it)
  function showLoading(expectedBytes) {
    const loadingEl = document.getElementById('loading');
    const progress = new LoadingProgress(expectedBytes);
    const channel = new BroadcastChannel('wotlane-disc');
    channel.onmessage = (e) => progress.add(e.data, performance.now());
    loadingEl.hidden = false;
    const timer = setInterval(() => {
      const text = progress.text(performance.now());
      if (text) return void (loadingEl.textContent = text);
      loadingEl.hidden = true;
      channel.close();
      clearInterval(timer);
    }, 250);
  }

  // the game quit from its own menu (wotm_webfs.cpp wotmWebQuit): back to the front panel
  window.wotmQuit = () => location.reload();

  // the game hears when the tab is hidden: an online match keeps running and tells the others why it may lag (wotm_webfs.cpp)
  function tellHidden() {
    window.Module._wotm_web_set_hidden?.(document.hidden ? 1 : 0);
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
    document.getElementById('leaveMatch').addEventListener('click', () => location.reload());
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

  // loads the game once (front.js calls it for single player, or with a lobby's match environment); `build` is version.json's
  // build id, which the server compares with the lobby's
  let started = false;
  window.wotmStart = function (matchEnv, build) {
    if (started) return;
    started = true;
    Object.assign(env, allowed, matchEnv || {}, build ? { WOTM_BUILDID: build } : {}, fixed);
    window.Module = {
      canvas,
      arguments: ['/disc/SCUS_971.97'],
      print: out,
      printErr: out,
      setStatus: (text) => { statusEl.textContent = text || 'Running'; },
      preRun: [() => Object.assign(window.Module.ENV, env)],
      onAbort: (what) => { statusEl.textContent = 'Stopped: ' + what; logEl.classList.add('open'); },
      onRuntimeInitialized: tellHidden,
    };
    showLoading(matchEnv ? 57 << 20 : 27 << 20);   // a first visit's download to a fight, or to the title
    document.addEventListener('visibilitychange', tellHidden);
    if (matchEnv) watchStalls();
    // the game reads its mods at start, so the repo's built-in mods are in place first
    syncMods().finally(() => {
      const script = document.createElement('script');
      script.src = 'wotm.js';
      document.body.appendChild(script);
    });
    canvas.focus();
  };

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
