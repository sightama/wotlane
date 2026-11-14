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
    };
    showLoading(matchEnv ? 57 << 20 : 27 << 20);   // a first visit's download to a fight, or to the title
    const script = document.createElement('script');
    script.src = 'wotm.js';
    document.body.appendChild(script);
    canvas.focus();
  };
  // a test link with a direct match skips the front panel
  window.wotmDirect = params.has('WOTM_MATCH');
})();
