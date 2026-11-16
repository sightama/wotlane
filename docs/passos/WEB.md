# The browser version

The whole port (the recompiled game, the PS2Recomp runtime and `native/`) built with Emscripten into one WebAssembly module, plus a
page in `web/`. It runs in desktop Chrome, Edge and Firefox. The disc is never part of the build or the repo: the page reads it from a `data/`
folder its server provides.

**Direction: browser first.** The browser version is the product. Mac-native features, Mac friends zips and browser-versus-Mac cross-play
runs are not release gates. The native build and its tests stay in CI because the browser game is built from the same code. Chrome and
Firefox on a computer are supported (Edge works too); Safari is not a goal.

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
  `web/loading.js` shows as "Downloading game data 34% (9 of 27 MB). Only the first visit downloads this much." over the game.
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
- **Address switches** (`web/pageenv.js`, tested in `pageenv.test.mjs`): only `WOTM_MATCH`, `WOTM_AIVSAI`, `PS2X_STATS` and `WOTM_MAPEDIT` (the Settings page's Mods tab, Your creations, has a Map editor row that opens the page with it, on a stock stage) are read from the page address; the disc, save and renderer
  settings always win.
- **Browser check:** `web/support.js` gives Safari, browsers without JSPI and phones a plain message and disables the play buttons; the front panel says Chrome, Firefox and Edge on a computer work.
- **Settings page:** the front panel's Settings button swaps the panel for a card of tabs (Online, Game, Display, Mods, Controls;
  `web/settings_ui.js`, the list of options in `web/settings.js`). Every option is a `settings.ini` key of `native/wotm_settings.hpp`, so
  the page and the game's own "Recompiled options" panel edit one file: `settings.ini` at the root of the browser's private file system
  (the game's `/save`), which the game reads at boot. A change is written at once (`updateSetting`: read, change the one line, write; the
  other lines, such as the key bindings, stay) and applies the next time the game starts; the card says so. The options are the ones that
  matter in a browser: picture (resolution, render scale, aspect ratio, anti-aliasing, texture filter, frame interpolation), sound
  (volumes, narrator), fighting (enemy bars, damage numbers, lock-on toggle, modifiers), camera, keyboard and button icons. Options that
  need a window or a monitor (display mode, refresh rate, vsync) are not offered. An online match fixes lock-on toggle (off) and the
  close-up camera (on) and takes the modifiers from the lobby, as `native/wotm_settings.hpp` `kNetKeys` says; the Online tab and each
  option's hint say so. `settings.test.mjs` reads `native/wotm_settings.hpp` and fails when an option's key, default or values drift
  from the game. The Mods tab holds Your creations and the Map editor row; the Controls tab lists the default keyboard layout
  (`web/controls.js`; `controls.test.mjs` fails when a default key is missing from it). `browser_mp_test`'s `single` session sets two
  options through the page and checks the game's log for them (`settings_page`).
  A tab whose game runs holds a shared Web Lock (`wotlane-game`, taken in `boot.js`, read by `web/gamelock.js`); another tab's Settings
  turns its options off while it is held, because a running game writes `settings.ini` and the saves again from its own memory.
- **Saves tab:** the Settings card's first tab (`web/saves_ui.js`, logic in `web/saves.js`). The memory-card saves are `save.json` and the
  `mc0/` and `mc1/` card folders at the root of the browser's private file system (`settings.ini`, `mods/` and the rest are not saves). The tab
  lists them (files, size, last saved, and from `save.json` the battle tokens and each adventure slot) and offers:
  - **Back up:** one zip download of exactly those files.
  - **Restore:** reads a zip and checks it before anything is written: file names from an allowlist (`save.json`, plain names under `mc0/` and
    `mc1/`), no path that leaves the folder, at most 40 files and 8 MB unpacked, `save.json` under 1 MB and an object the game reads
    (`options`, `unlocks`, `battleTokens` or `saves` of the right kind). Files outside the saves are left out and named; a zip of the whole
    folder inside one folder works. An inline confirmation shows what the zip holds and what it replaces. The current saves are kept as an
    automatic backup, then replaced (the card folders go too, because the game imports adventures from card files that `save.json`
    lacks); a failed write puts the old saves back.
  - **Delete all saves:** the same confirmation and automatic backup, then `save.json` and both card folders go. One adventure slot can't be
    deleted from the page for the same reason: the game rebuilds the card files from `save.json` and imports a card's slots when the JSON has none.
  - **Automatic backups:** the newest 5 stay in `backups/` of the same file system (`saves-<date>-<time>.zip`), each with a Download.
  Both actions are refused while another tab's game runs (the game lock above). Creations export and import stay in the Mods tab.
  `saves.test.mjs` covers the file logic on a fake file system; `browser_mp_test`'s `saves` session drives the tab in a real browser.

## Online
The front panel (`web/front.js`, `web/lobby.js`) speaks the lobby protocol over a WebSocket at `/lobby` (one line per text frame):
Host creates a lobby and shows its code, Join takes one, the owner sets stage, wins and modifiers and starts. A `MATCH` line becomes the
game's environment with the same mapping as `tools/net_lobby.py` `match_env`, with `WOTM_NET` the WebSocket at `/match`, and the build id
from `version.json` in `WOTM_BUILDID`.

**Invite link:** in a lobby, Copy invite link puts `<page>/?join=CODE` on the clipboard (a page that may not use the clipboard shows the link selected instead). A page opened with `?join=CODE` (`web/pageenv.js` `inviteCode`: only a valid 4-character lobby code counts, and it never reaches the game's environment) fills the code in and removes it from the address; a browser that remembers a name joins at once, any other asks for a name first. The site's password prompt comes before the page, so the link works after it.

**Open lobbies:** on the Host/Join choice the panel keeps one connection that asks `LIST` every 5 s (not while the tab is hidden, and not once the player is in a lobby or has pressed Single player) and lists each lobby as "<owner>: n/8 (<names>), waiting | in a match | full[, needs a password]" with a Join button (`lobbyRow` in `web/lobby.js`). Join is off for a lobby that is in a match, full or locked (the page can't send a password). The server's `LOBBY` line carries no map, so the list doesn't show one. A server that is out of reach hides the list.

**Pings:** while a lobby waits, the server pings each member's `/lobby` WebSocket every second and sends the lobby a `PINGS <code> <name:ms,...>` line with the round trips (`-` for a native member on TCP, which the server can't measure). The member list shows "<name> (<monster>), 23 ms[, owner]". `PINGS` is an extra line: a client that doesn't know it skips it, so the protocol number stays 5.

**Chat:** a text box in the lobby sends `CHAT <text>` (up to 200 bytes of printable text; the server drops control characters and trims the ends, and allows 5 lines per 10 s per member, then answers "chatting too fast"). The server sends `CHAT <code> <name> <text>` to everyone in the lobby, the sender too, and only while the lobby waits (`ERROR chat is only open before the match` after the start). The page shows the last 50 lines as plain text (`textContent`) and clears them when it joins a lobby. `CHAT` is an extra line for older clients to skip, so the protocol number stays 5.

A refusal from the server (name taken, no such lobby, lobby full, lobby in a match, build differs) stays on the message line, as a plain sentence (`web/messages.js` maps every ERROR the lobby sends; a test reads them from `wotm_net_server.cpp`, and an unknown text is shown as it is). The live server runs with `--lobbies 4`: a fifth lobby creator reads "All 4 lobbies are busy. Try again soon." Leave sends `LEAVE` and returns to Host/Join without a reload (`LobbyClient.leave()`). The page's own
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
The input delay is fixed when the match starts, from the two slowest round trips to the server (`pickDelay` in
`native/net/wotm_net_proto.hpp`), and is the same for every player: with everyone at 20, 60 and 120 ms it is 3, 5 and 9 frames in a
1 vs 1 (60 Hz: 50, 83 and 150 ms) and 2, 3 and 5 frames with 3 to 8 players (30 Hz: 67, 100 and 167 ms).
A Mac or Windows build that plays the site must be built from `wip/browser`: its build id (a hash of `native/`) differs from `wotlane/mac`'s, and the server refuses a different build.

During a match:
- a hidden tab sends `Away` (and its return); the others' "Waiting for" message adds "(their game tab is in the background)";
- after 60 s waiting for a player the page offers Keep waiting (asks again 60 s later) or Leave the match (`web/stall.js`);
- a match that stops (refused, desync, connection lost) shows the reason with a way back to the front panel, the lobby code filled in.

- **Safari:** not supported, and not a goal (Safari before 27 lacks JSPI). The page says so in a plain message; the front panel names Chrome, Firefox and Edge on a computer as the supported browsers.
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
