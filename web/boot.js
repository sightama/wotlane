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
  // runtime and game variables from the address (?WOTM_MATCH=5,1,9...), for tests and direct matches
  for (const [key, value] of new URLSearchParams(location.search))
    if (/^(WOTM|PS2X)_[A-Z0-9_]+$/.test(key)) env[key] = value;
  Object.assign(env, window.wotmMatchEnv || {});

  window.Module = {
    canvas,
    arguments: ['/disc/SCUS_971.97'],
    print: out,
    printErr: out,
    setStatus: (text) => { statusEl.textContent = text || 'Running'; },
    preRun: [() => Object.assign(window.Module.ENV, env)],
    onAbort: (what) => { statusEl.textContent = 'Stopped: ' + what; logEl.classList.add('open'); },
  };
})();
