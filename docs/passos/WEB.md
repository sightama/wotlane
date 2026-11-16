# The browser version

The whole port (the recompiled game, the PS2Recomp runtime and `native/`) built with Emscripten into one WebAssembly module, plus a
page in `web/`. It runs in desktop Chrome and Edge. The disc is never part of the build or the repo: the page reads it from a `data/`
folder its server provides.

## Build
`sh tools/build_web.sh` configures `<runtime>/build_web` once (the same game flags as `build_dev`, raylib's web platform on WebGL2, no
FFmpeg), builds `wotm` and assembles `<runtime>/web_dist`:
- `index.html` and the `web/*.js` scripts;
- `wotm.js` and `wotm.wasm` (about 40 MB, about 4 MB brotli);
- `version.json`: the wasm's SHA-256 and the build id (`tools/build_id.py`, the same id as `build_dev` for the same sources);
- `mods/`: the repo's enabled mods and `mods/manifest.json` (file list and hash), from `tools/web_mods.py`.

It needs Emscripten (`EMSDK`, default `~/emsdk`) and the translated game (`sh tools/setup_runtime.sh`). The link options (JSPI, WASMFS,
pthreads, WebGL2, `web/data_retry.js` as a `--pre-js`) live in the PS2Recomp seam (`tools/runtime_patch/wotm-runtime.patch`).

## Run locally
`python3 tools/serve_web.py --site <runtime>/web_dist --data <folder with SCUS_971.97 and game.iso>` serves the page with the
cross-origin isolation headers (COOP `same-origin`, COEP `require-corp`) that SharedArrayBuffer needs, and byte ranges for the disc.
For online play start `wotm_server --ws-port <port>`; a page opened from `localhost` or `127.0.0.1` takes `?server=ws://127.0.0.1:<port>`
(a public page always uses its own host). Use one browser profile per player: tabs of the same profile share the saves.

## In the page
- **Disc:** `/disc` is a WASMFS fetch backend over `data/`: the game's reads become 1 MiB range requests, each fetched once
  (`native/wotm_webfs.cpp`). `web/data_retry.js` retries network and 5xx failures in every thread and reports each chunk, which
  `web/loading.js` shows as "Downloading game data 34%" over the game.
- **Saves and settings:** `/save` is the browser's private file system (OPFS) and the game's working folder (`WOTM_SAVEDIR=/save`), so
  `settings.ini`, the memory card and `mods/` survive a reload.
- **Mods:** before the game starts, `web/builtin_mods.js` mirrors the dist's `mods/` into `<OPFS root>/mods/<mod>/`, as
  `tools/sync_mods.sh` does natively; other folders (the player's creations, `web/creations.js`) are left alone. With the same files the
  mods digest equals a native install's.
- **Frames:** each presented frame awaits one `requestAnimationFrame` through JSPI. The game itself runs on its own thread and does not
  wait for presentation, so it keeps its 60 frames per second in a hidden tab.
- **Graphics:** the native layer's GL calls go to WebGL2 (`native/wotm_glad_web.cpp`); its GLSL 330 shaders become GLSL ES 3.00
  (`native/wotm_glsl.hpp`). Lens flares are off (`WOTM_NOFLARE`: WebGL2 can't read the depth buffer back). The canvas fills the page and
  the game letterboxes its own frame.
- **Address switches:** only `WOTM_MATCH`, `WOTM_AIVSAI`, `PS2X_STATS` and `WOTM_MAPEDIT` (the front panel's "Map editor" under Your creations opens the page with it, on a stock stage) are read from the page address; the disc, save and renderer
  settings always win.
- **Browser check:** `web/support.js` gives Firefox, Safari and phones a plain message and disables the play buttons.

## Online
The front panel (`web/front.js`, `web/lobby.js`) speaks the lobby protocol over a WebSocket at `/lobby` (one line per text frame):
Host creates a lobby and shows its code, Join takes one, the owner sets stage, wins and modifiers and starts. A `MATCH` line becomes the
game's environment with the same mapping as `tools/net_lobby.py` `match_env`, with `WOTM_NET` the WebSocket at `/match`, and the build id
from `version.json` in `WOTM_BUILDID`.

`wotm_server --ws-port <port> [--ws-bind <address>]` (default bind `127.0.0.1`, behind a web server that terminates HTTPS) takes these
connections: `/lobby` behaves as a TCP lobby connection, `/match` carries binary frames of one channel byte and the same datagram as
ENet, and the server measures browser round trips with WebSocket pings. Browser, Mac and Windows players can share a match.

During a match:
- a hidden tab sends `Away` (and its return); the others' "Waiting for" message adds "(their game tab is in the background)";
- after 60 s waiting for a player the page offers Keep waiting (asks again 60 s later) or Leave the match (`web/stall.js`);
- a match that stops (refused, desync, connection lost) shows the reason with a way back to the front panel, the lobby code filled in.

## Limits
- **Firefox and Safari:** the game's main thread waits on reads the WASMFS fetch and OPFS backends hand to workers. Chrome services those
  worker reads meanwhile; Firefox doesn't finish a fetch body and WebKit doesn't run the worker's fetch or OPFS calls until the main thread
  yields, so the page freezes at boot. Safari before 27 also lacks JSPI.
- **Movies:** skipped (no FFmpeg in the browser build).
- **Audio:** raylib's web audio uses a ScriptProcessorNode, which Chrome reports as deprecated.
- **Loading counter:** two game tabs of the same site on one computer count each other's chunks.

## Tests
- `node --test web/` (the page scripts, with a fake WebSocket and an in-memory file system);
- `sh tools/test_web.sh` (the native tests built with Emscripten and run under node, the browser-only sources with `-Werror`, then the
  page scripts);
- `python3 tools/test_serve_web.py` and `python3 tools/test_web_mods.py`;
- the WebSocket listener and transport: `sh tools/test_net.sh` (`ws_*` tests).
