# The browser version

The whole port (the recompiled game, the PS2Recomp runtime and `native/`) built with Emscripten into one WebAssembly module, plus a
page in `web/`. It runs in desktop Chrome, Edge and Firefox. The disc is never part of the build or the repo: the page reads it from a `data/`
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
- **Disc cache:** `web/disc_cache.js` (loaded by the page with a script tag and, in the game's workers, by `data_retry.js`; it is not in the `--pre-js` list, so a change to it leaves the build id alone) keeps the disc's
  1 MiB chunks in the browser's Cache API, one cache per disc file and version (its ETag), each chunk stored with its SHA-256 and checked
  before use; a chunk missing or failing the check comes from the network. Nothing is stored when the Cache API is missing, the HEAD
  gave no ETag or storage is 90% full. From the page's load, while the player is on the front panel, `web/boot.js` drops the caches
  of other versions and fetches in parallel the chunks every boot reads (`web/boot_chunks.json`, written by
  `python3 tools/web_boot_chunks.py <boot log> <SCUS_971.97>` from a boot to the title); the game starts once they are in. In Firefox
  the game's own reads from worker threads are synchronous XHR (below), which the Cache API can't serve, so the page starts a helper
  worker (`disc_cache.js` itself, named `wotlane-disc-cache`) that owns the cache for them: a game worker (its `XMLHttpRequest` is wrapped
  by `data_retry.js`'s `importScripts` of `disc_cache.js`) asks it over a BroadcastChannel, blocks on a SharedArrayBuffer until it
  answers, and after a read that went to the network hands it the bytes to keep. A helper that doesn't answer within 300 ms is left
  alone for 2 s and the read goes to the network, as it did before the helper. The page also fetches `wotm.wasm` from its load on, into the HTTP cache (served with an
  ETag and `no-cache`), so `wotm.js`'s own request only revalidates it.
- **Saves and settings:** `/save` is the browser's private file system (OPFS) and the game's working folder (`WOTM_SAVEDIR=/save`), so
  `settings.ini`, the memory card and `mods/` survive a reload.
- **Disc and saves outside Chromium:** the WASMFS fetch and OPFS backends hand each read to a worker while the game's main thread waits;
  Chrome services those workers meanwhile, Firefox doesn't. So everywhere but Chromium (`web/support.js` `mainThreadFiles`) the page
  passes the disc files' sizes (`WOTM_DISC_SIZES`) and `WOTM_SAVE_MEMORY=1`: `/disc` fetches its 1 MiB chunks itself, awaiting the fetch
  through JSPI on the main thread (`native/wotm_webdisc.cpp`), and `/save` lives in memory, loaded from OPFS at boot and compared with
  OPFS every 60 frames, which writes back new, changed and removed files in the background (`native/wotm_websave.cpp`).
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
- **Browser check:** `web/support.js` gives Safari, browsers without JSPI and phones a plain message and disables the play buttons.

## Online
The front panel (`web/front.js`, `web/lobby.js`) speaks the lobby protocol over a WebSocket at `/lobby` (one line per text frame):
Host creates a lobby and shows its code, Join takes one, the owner sets stage, wins and modifiers and starts. A `MATCH` line becomes the
game's environment with the same mapping as `tools/net_lobby.py` `match_env`, with `WOTM_NET` the WebSocket at `/match`, and the build id
from `version.json` in `WOTM_BUILDID`.

A refusal from the server (name taken, no such lobby, lobby full, lobby in a match, build differs) stays on the message line: the page's own
`LobbyClient.close()` silences `onClose`, which only reports a connection that dropped. The panel is a flex column that scrolls
(`#front` has `overflow-y: auto`, its card `margin: auto`), so a card taller than the window (8 members, a zoomed page) keeps Start
reachable. After a match (or Leave the match, which reloads the page) the panel asks the server for its lobbies (`LIST`, over a short-lived
`LobbyClient`) and offers the last lobby's code only while that lobby is listed: a finished match empties it and the server closes it.
A lobby still in a match is named as such; one that is gone gets no note and no prefilled code.

`wotm_server --ws-port <port> [--ws-bind <address>]` (default bind `127.0.0.1`, behind a web server that terminates HTTPS) takes these
connections: `/lobby` behaves as a TCP lobby connection, `/match` carries binary frames of one channel byte and the same datagram as
ENet, and the server measures browser round trips with WebSocket pings. Browser, Mac and Windows players can share a match. A WebSocket
is reliable and ordered, so the server sends a browser each input once (an ENet player gets what it lacks again every 16 ms): a browser
whose network stops is sent only pings, not a growing pile of repeats. A browser that comes back sends everything it queued at once; the
server reads a connection 64 KB at a time, so any burst is fine (only a lobby line over 64 KB closes the connection).
A Mac or Windows build that plays the site must be built from `wip/browser`: its build id (a hash of `native/`) differs from `wotlane/mac`'s, and the server refuses a different build.

During a match:
- a hidden tab sends `Away` (and its return); the others' "Waiting for" message adds "(their game tab is in the background)";
- after 60 s waiting for a player the page offers Keep waiting (asks again 60 s later) or Leave the match (`web/stall.js`);
- a match that stops (refused, desync, connection lost) shows the reason with a way back to the front panel, the lobby code filled in.

- **Safari:** not supported yet. Safari before 27 lacks JSPI; Safari 27 is untested: the main-thread disc and saves (above) are the path to try there.
- **Safari:** not supported yet. Safari before 27 lacks JSPI; with it, the main-thread disc and saves (above) are the path to try.
- **Firefox:** the game takes more CPU per frame than in Chrome (it keeps 60 frames per second). The save folder in memory is also synced
  when the tab is hidden and on `pagehide`, and the game's own quit waits for the writes; a tab closed right after a save can lose it.
- **Movies:** skipped (no FFmpeg in the browser build).
- **Audio:** raylib's web audio uses a ScriptProcessorNode, which Chrome reports as deprecated.
- **Loading counter:** two game tabs of the same site on one computer count each other's chunks.

## Startup time
Measured from the page's navigation to the game log's `[boot] ... ready:` line, in a new browser profile (cold: nothing cached) and
again in the same profile (warm), to the title (the front panel's Single player) and to a direct fight
(`?WOTM_MATCH=3,1,1&WOTM_AIVSAI=1`), on the real network and throttled (Chrome's DevTools network emulation), counting the bytes of
each kind of request (page, mods, wasm, disc). The `[boot] disc: LSN ...` lines give each disc read's time. A load's 512 KB reads are about 19 ms apart: the stage unzip runs in
host code (`native/wotm_zip.hpp`), so the game does not decompress between reads, in the browser and natively alike.

## Tests
- `node --test web/` (the page scripts, with a fake WebSocket and an in-memory file system);
- `sh tools/test_web.sh` (the native tests built with Emscripten and run under node, the browser-only sources with `-Werror`, then the
  page scripts);
- `python3 tools/test_serve_web.py`, `python3 tools/test_web_mods.py` and `python3 tools/test_web_boot_chunks.py`;
- the WebSocket listener and transport: `sh tools/test_net.sh` (`ws_*` tests).
- online play in real browsers, 1 to 8 players against a local server or the live site: `node tools/browser_mp_test/run.mjs`
  (`tools/browser_mp_test/README.md`; its log readers: `node --test tools/browser_mp_test/test/`).
