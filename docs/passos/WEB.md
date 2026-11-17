# The browser version

The whole port (the recompiled game, the PS2Recomp runtime and `native/`) built with Emscripten into one WebAssembly module, plus a
page in `web/`. It runs in Chrome, Edge and Firefox on a computer and in Chrome on an Android phone. The disc is never part of the build or the repo: the page reads it from a `data/`
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
cross-origin isolation headers (COOP `same-origin`, COEP `require-corp`) that SharedArrayBuffer needs, and the disc both as numbered chunk
files (below; the hashes of each file are computed on first use, a few seconds for the ISO) and as whole files with byte ranges.
`--chunks only` serves no whole files, as the site does once split; `--chunks off` has no chunk files, as a site that is not split yet.
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
- **Disc chunks:** a split site serves the disc as 1 MiB files, so that a CDN caches all of it (Cloudflare Free caches no object over
  512 MB, and the ISO is 1.2 GiB): `/data/index.json` (`{chunkBytes, files: {name: {digest, size}}}`, the digest being the first 16 hex
  digits of the file's SHA-256), `/data/<digest>/manifest.json` (`name`, `size`, `chunkBytes`, `chunks`, `sha256[]`) and
  `/data/<digest>/<n>.bin` (a plain 200, the last chunk shorter, cacheable for good because a new disc is a new digest). The game, the
  fetch backend and `native/wotm_webdisc.cpp` know nothing of this: they still send HEAD and Range requests for `data/<file>`, and
  `web/disc_chunks.js` (a script tag in the page; the workers import it from `disc_cache.js`; like it, it is outside the `--pre-js` list, so
  the build id stays) answers them: a HEAD from the index (the size, `Accept-Ranges` and an ETag of `"<digest>"`), a range GET from the
  chunk files that cover it, fetched in parallel (12 at a time in a thread) and each checked against `sha256[n]` before use. A chunk that
  fails its check, or comes the wrong length, is asked for again from `<n>.bin?retry=1` (another URL, so a bad copy kept by the edge or
  the HTTP cache is not served twice), three tries in all, and then the read fails. The Cache API layer above keeps its caches by the
  digest instead of the ETag. A 404 for `index.json` means the server is not split: every request goes to the whole-file URL as it does
  without this layer. An index that fails any other way is an error, not a fallback, so that a broken split is not hidden. Chunks are 1 MiB
  because the game reads the disc in 1 MiB aligned ranges (`kDiscChunkBytes`), the cache keeps 1 MiB blocks and `boot_chunks.json` lists
  them: a boot to the title is 27 requests of 1 MiB. Firefox's synchronous XHR path asks the helper first and reads the chunk files
  itself, one after another with synchronous XHR, when the helper doesn't have the range; it checks them with a JavaScript SHA-256
  (`sha256Sync`, because SubtleCrypto can't be awaited there) and hands the helper the bytes to keep. The loading counter counts each
  chunk that arrives, as it counts a range.
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
- **High frame rate:** the game logic always runs at 60, online and offline; only the drawing follows the display. Before the game starts
  the page measures the display's refresh rate from 60 animation frames (`web/display.js` `measureRefresh`: the median of 4-frame spans,
  snapped to a common rate within 4%; a hidden tab gives up after 2 s and the game assumes 60) and passes it as `WOTM_DISPLAY_HZ`, which
  stands for the monitor rate natively (`initPresentRate`, `wotm_live.inc`). Above 60 Hz every animation frame draws the blend of the game's
  last two frames (frame interpolation, `docs/passos/NATIVO.md`, "Presentation rate"), one game frame behind; at 60 the scene is redrawn
  once per game frame. The redraw's real cost is in the browser's GPU process, which runs the WebGL calls after this thread has moved on, so the native
  guard (the redraw time against the period) misses it; in the browser blending is judged by the frame interval it reaches
  (`interp::PaceGuard`, `wotm_interp.hpp`): a moving average over 1.4 periods suspends it (the scene goes back to one redraw per game
  frame, presented at the display rate), and it is tried again after 10 s, twice as long after each failed try (up to 160 s), with an
  `[interp]` line in the log each time. The Settings page's Frame rate (`WOTM_HZ`: Display rate or 60) caps the rate the drawing assumes (an unmeasured display counts as 60), so 60 turns
  interpolation off; Vsync has no effect in the browser. Online matches stay in step because nothing the drawing does reaches the game.
  The rate is measured once: a window moved to a screen with another rate keeps the first one until the next Play. Measuring the rate
  from frame times follows nfsu2-web (`web/src/app/display.ts`, https://github.com/th0mk/nfsu2-web, MIT License, Copyright (c) th0mk).
- **Graphics:** the native layer's GL calls go to WebGL2 (`native/wotm_glad_web.cpp`); its GLSL 330 shaders become GLSL ES 3.00
  (`native/wotm_glsl.hpp`). Lens flares are off (`WOTM_NOFLARE`: WebGL2 can't read the depth buffer back). The canvas fills the page in
  device pixels (CSS size times `devicePixelRatio`, `fitCanvas` in `wotm_live.inc`: raylib sizes it in CSS pixels at start and on each page
  resize, so each present puts it back), so a high-density screen gets its full sharpness, and the game letterboxes its own frame in it.
  The overlays drawn in window pixels (the online ping and "Waiting for" message, the F3 counter, the F1 menu) grow by the pixel ratio.
  The picture options are the native ones (`docs/passos/NATIVO.md`, "PC graphics" and "Widescreen"), set on the Settings page's Display
  tab: resolution (Window size is the canvas in device pixels), render scale, aspect ratio (Hor+: the width grows, the vertical field of
  view stays the game's, so models are never stretched), anti-aliasing, texture filter, frame interpolation, frame rate and Wide HUD
  (`WOTM_HUDWIDE`: the bars at the screen's edges, or off, in the 4:3 middle where the game puts them).
- **Frame rate check:** `node tools/web_fps.mjs <chrome|firefox> <page url> [secs]` opens a direct AI fight in a visible browser and prints
  the presented frames per second, the frame times (median, p95, p99, late frames), the game's redraws and interpolated redraws per second,
  and saves a screenshot.
- **Address switches** (`web/pageenv.js`, tested in `pageenv.test.mjs`): only `WOTM_MATCH`, `WOTM_AIVSAI`, `PS2X_STATS` and `WOTM_MAPEDIT` (the Settings page's Mods tab, Your creations, has a Map editor row that opens the page with it, on a stock stage) are read from the page address; the disc, save and renderer
  settings always win.
- **Browser check:** `web/support.js` gives Safari, browsers without JSPI and iPhones and iPads (every browser there is WebKit) a plain message and disables the play buttons; the front panel says Chrome, Firefox and Edge on a computer or an Android phone work.
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
  from the game. The Mods tab holds Your creations and the Map editor row; the Controls tab lists the default keyboard layout and the touch controls
  (`web/controls.js`; `controls.test.mjs` fails when a default key is missing from it). `browser_mp_test`'s `single` session sets two
  options through the page and checks the game's log for them (`settings_page`).
  A tab whose game runs holds a shared Web Lock (`wotlane-game`, taken in `boot.js`, read by `web/gamelock.js`); another tab's Settings
  turns its options off while it is held, because a running game writes `settings.ini` and the saves again from its own memory.
- **Startup movies:** Single player plays the two boot movies (`INTRO_M`, then `INT_MAIN`) in an overlay over the canvas (`web/movies.js`)
  while the game boots behind it, so the movie hides the loading instead of adding to it; the game never sees them, so the wasm and
  determinism are untouched, and an online match boots straight into the fight with none. The Game tab's Startup movies is
  **Skippable** (default: Esc, Space, Enter, a tap on the picture or a new gamepad button press skips the movie being shown, the next one
  starts), **Original** (the movie plays through; keys, taps and pads do nothing) or **Off** (nothing is fetched, not even `movies.json`). The
  choice is the page's own (`localStorage` `wotlane.movies`), not a `settings.ini` key: the game rewrites that file without keys it does not know.
  While the overlay is up its keys never reach the game (capture-phase listeners) and the page's audio contexts are held suspended, so the
  game's title music does not play under the movie (`installAudioHold`), and the game's own gamepad polling (`navigator.getGamepads`) sees no
  pads until the movies end (`holdGamepads`); the page reads them itself to skip. A
  movie that fails, or does not start within 15 s, is passed over. The files are made from the disc and never committed:
  `python3 tools/web_movies.py --iso <game.iso> --out <data>/movies` (FFmpeg needed) writes `intro_m.webm`, `int_main.webm` (VP9 picture, Opus
  audio, constrained quality: CRF 32, at most 500 kbit/s) and `movies.json`, about 6 MB in all against 85 MB of disc movies. FFmpeg reads the
  picture of a `.PSS` but not its audio, so the tool takes the audio apart itself (`docs/FORMATOS.md`). The page fetches `data/movies/` through
  the same data path as the disc (`serve_web.py` serves it from the data folder), and a server without the files plays no movie.
  `movies.test.mjs` and `tools/test_web_movies.py` cover the page and the tool; `browser_mp_test`'s `movies` session plays all three modes.
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

**Invite link:** in a lobby, Copy invite link puts `<page>/?join=CODE` on the clipboard (a page that may not use the clipboard shows the link selected instead). A page opened with `?join=CODE` (`web/pageenv.js` `inviteCode`: only a valid 4-character lobby code counts, and it never reaches the game's environment) fills the code in and removes it from the address; a browser that remembers a name joins at once, any other asks for a name first. The site has no password prompt (https://playwotm.com), so the link opens the page directly.

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
Each player has their own input delay, re-picked by the server every 500 ms from the round trips (`adviseDelay` in
`native/net/wotm_net_proto.hpp`, `docs/ONLINE.md`): half their round trip plus half the slowest other player's, at the high end of the
last 4 s, plus 3 ms. With everyone at a steady 20, 60 and 120 ms that is 2, 4 and 8 frames in a 1 vs 1 (60 Hz) and 1, 2 and 4 frames
with 3 to 8 players (30 Hz); jitter adds to it. A match starts at the largest player's delay, and at least 2.
A Mac or Windows build that plays the site must be built from `wip/browser`: its build id (a hash of `native/`) differs from `wotlane/mac`'s, and the server refuses a different build.

During a match:
- a hidden tab sends `Away` (and its return); the others' "Waiting for" message adds "(their game tab is in the background)";
- after 60 s waiting for a player the page offers Keep waiting (asks again 60 s later) or Leave the match (`web/stall.js`);
- a match that stops (refused, desync, connection lost) shows the reason with a way back to the front panel, the lobby code filled in.

- **Safari:** not supported, and not a goal (Safari before 27 lacks JSPI). The page says so in a plain message; the front panel names Chrome, Firefox and Edge on a computer as the supported browsers.
- **Firefox:** the game takes more CPU per frame than in Chrome (it keeps 60 frames per second). The save folder in memory is also synced
  when the tab is hidden and on `pagehide`, and the game's own quit waits for the writes; a tab closed right after a save can lose it.
- **Movies:** the game side skips every movie (no FFmpeg in the browser build, `PS2X_SKIP_MOVIES`); the page plays the two boot movies itself (Startup movies). The endings, the act intros, the credits and the menu's big-screen loop are skipped.
- **Audio:** raylib's web audio uses a ScriptProcessorNode, which Chrome reports as deprecated.
- **Loading counter:** two game tabs of the same site on one computer count each other's chunks.


## Phones, touch and gamepads
The game reads pads through raylib's web platform: `navigator.getGamepads()` in the standard layout, four slots, player n is the n-th
connected pad. Everything that feeds it lives in the page (`web/pads.js`, `web/touch.js`), so none of it changes the wasm or the build id.

- **Pad hub (`web/pads.js`):** `installPads` replaces `navigator.getGamepads` with the hub's answer: four slots, each a pad in the standard
  layout. A real pad keeps its slot while it stays; a fifth pad is ignored. The browser's own `gamepadconnected` and `gamepaddisconnected`
  events (whose indexes are not the slots) are swallowed by a capture listener and replaced by the hub's, with the slot as the index. The hub
  also announces every pad again each second, so a game that starts listening after a pad was plugged in still hears of it. A standard-mapped pad
  passes through. Any other pad is mapped by its shape (`fallbackLayout`): buttons 0 to 5 as the face buttons and shoulders, then
  triggers, select, start and sticks; a hat on axis 9 or a hat pair on axes 6 and 7 as the d-pad; with 8 axes the triggers on axes 2 and 5
  at rest -1. Those layouts are guesses checked only with synthetic pads (`pads.test.mjs`).
- **Toast:** a connected or removed real pad shows "Controller connected: <name> (player N)" over the page, front panel included; the
  vendor and product text of the browser's name is cut (`padLabel`).
- **Several pads:** up to four pads are four slots; the game's own multiplayer screens give them to players.
- **Touch controls (`web/touch.js`):** shown while the game runs (the front panel is hidden) on a device whose primary pointer is coarse, and on
  any other device from its first touch. They feed a virtual pad named "Touch screen controls" that takes slot 0 while it is shown, so the
  game sees one more gamepad. Layout (`LAYOUT`, positions in thumb units): a floating left stick anywhere on the left 46% of the screen
  (below the top row), a look zone on the right (a drag is the right stick, which turns the camera), the four face buttons as a diamond at the
  bottom right with jump, light and heavy attack bigger than grab, two chord buttons (SP1 is triangle and circle, SP2 is cross and square, the
  game's specials) and one for lock-on (L1 and R1), L2 and L1 at the top left and R1 and R2 (block) at the top right, Select, a gear and Start
  at the top middle. Several fingers work at once; a press stays down at least 80 ms, so a tap shorter than the game's pad reads is still
  read. A quick tap (under 220 ms, under 12 px) on either zone is also a mouse click on the game: menu lines are picked by pointer, and in a
  fight a click is the light attack. The stick moves the menu selection, Cross chooses and Triangle goes back, as on a pad.
  Vibration (`navigator.vibrate`) answers a press and the game's own rumble. The gear opens size (0.7 to 1.2), opacity and vibration,
  kept in `localStorage` (`wotlane.touch`).
- **Steps aside:** a key press, a mouse press or a real pad hides the controls and removes the virtual pad (a real pad would otherwise be player
  2); a touch brings them back unless a real pad is connected. While they are shown the page refuses the game's pointer lock (the mouse
  camera) and leaves any lock, which would zero every touch's coordinates.
- **Fullscreen and landscape:** the first tap on a button on a touch device asks for fullscreen on the whole page and a landscape orientation
  lock (the browser may refuse either); the gear panel has a Fullscreen button. In portrait the controls are replaced by "Turn your phone
  sideways to play". The screen is kept awake while the controls are up (Wake Lock, where the browser has it).
- **Phones:** the same game runs in Chrome on Android; there is no phone-specific build. Frame size is the game's 720 lines; the in-game
  VIDEO options (resolution, render scale) are the lever on a slow phone. Online, a phone's lobby `HELLO` carries `hz=30` and its matches
  run in the game's 30 Hz mode (`docs/ONLINE.md`, "30 Hz with a phone"): at 60 Hz a phone that computes fewer game frames plays
  everyone's match in slow motion, with broken sound. iPhones and iPads cannot run it (no JSPI before Safari 27).
- **Tests:** `node --test web/` (`pads.test.mjs`: layouts, slots, hot-plug, events; `touch.test.mjs`: every PS2 button reachable, nothing
  overlapping or off screen on five phone-sized screens at three control sizes, settings); `node tools/browser_touch_test/run.mjs`
  (`tools/browser_touch_test/README.md`): a phone-sized Chromium with multi-touch through the DevTools protocol and a mocked Gamepad API
  against the real game, including a fight against the AI.

## Startup time
Measured from the page's navigation to the game log's `[boot] ... ready:` line, in a new browser profile (cold: nothing cached) and
again in the same profile (warm), to the title (the front panel's Single player) and to a direct fight
(`?WOTM_MATCH=3,1,1&WOTM_AIVSAI=1`), on the real network and throttled (Chrome's DevTools network emulation), counting the bytes of
each kind of request (page, mods, wasm, disc). The `[boot] disc: LSN ...` lines give each disc read's time. A load's 512 KB reads are about 19 ms apart: the stage unzip runs in
host code (`native/wotm_zip.hpp`), so the game does not decompress between reads, in the browser and natively alike.

## The public site
The live site is https://playwotm.com/ (Cloudflare in front of Caddy on the GCP VM; the old https://35-185-58-174.sslip.io/ redirects to it with a 301).
Everything that serves and deploys it lives in the private coord repo (`scripts/deploy_web.sh`, `web/Caddyfile`, notes in its README).
The page carries the not-affiliated disclaimer and the rights-holder address (`legal@playwotm.com`) in its footer, a description,
OpenGraph and Twitter tags and JSON-LD (`web/index.html`, tested by `web/legal.test.mjs`). `robots.txt`, `sitemap.xml`, `llms.txt`,
`manifest.webmanifest`, the icons and `og.png` are added to the dist by the coord deploy, not by `tools/build_web.sh`.

## Tests
- `node --test web/` (the page scripts, with a fake WebSocket and an in-memory file system);
- `sh tools/test_web.sh` (the native tests built with Emscripten and run under node, the browser-only sources with `-Werror`, then the
  page scripts);
- `python3 tools/test_serve_web.py`, `python3 tools/test_web_mods.py`, `python3 tools/test_web_boot_chunks.py` and `python3 tools/test_web_movies.py`;
- the WebSocket listener and transport: `sh tools/test_net.sh` (`ws_*` tests).
- online play in real browsers, 1 to 8 players against a local server or the live site: `node tools/browser_mp_test/run.mjs`
  (`tools/browser_mp_test/README.md`; its log readers: `node --test tools/browser_mp_test/test/`).
