# The browser version

The whole port (the recompiled game, the PS2Recomp runtime and `native/`) built with Emscripten into one WebAssembly module, plus a
page in `web/`. It runs in Chrome, Edge, Brave and Firefox on a computer and in Chrome on an Android phone. The disc is never part of the build or the repo: the page reads it from a `data/`
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
- **Front panel loading:** Single player, Host online and Join are `disabled` in the HTML and `front.js` enables them in the same step that attaches
  their handlers, so no press lands on a dead button. `index.html` preloads every module `front.js` imports, directly or not
  (`<link rel="modulepreload">`; `web/page_startup.test.mjs` fails when the list and the import graph differ). The card is anchored to the top of
  the screen, the device note and the Firefox tip sit below the actions, and the open lobbies area is there from the start with
  "Looking for open lobbies…" and room for the two-line empty message; it stays on the page, greyed while a join is under way and with a
  message in its place when the server cannot give the list, so no control above it moves after the first paint (the controls below it move when several lobbies arrive). A button shows a pressed
  state at once (`:active`, `touch-action: manipulation`), and Host and Join say what they are doing in the status line. Controls are 44px on
  any device with a touch screen (`pointer: coarse` or `any-pointer: coarse`) and 24px otherwise; field borders are 3:1 against the panel.
- **Front panel start:** the status line says "Loading…" (`data-pending`) until `init()` runs. A script of `front.js`'s module graph that cannot be
  fetched, has a syntax error or throws as it runs replaces it with "The page could not load…" (a head script decides from the page's module
  scripts and `modulepreload` links; a failed icon, stylesheet, preload link or unrelated script does not). After 6 s without `init()` it says
  "Still loading…". A broken classic script that `init()` needs ends in `init()`'s own message.
- **Front panel layout:** the panel is the page's `<main>`. Home has Name and Monster, one filled button (Single player) and a "Play online"
  region (Host online, join with a code, open lobbies). The lobby shows its code large next to Copy invite link, the members as rows with
  their ping and device chips, Stage and Wins side by side, the Rules group, then Start (the owner only) beside Leave, then the chat. A
  browser that cannot run the game (Safari, an iPhone) sees only the reason, as a note, and no controls. The status bar under the panel is
  not drawn while the panel covers the stage. Colour scheme is dark (`color-scheme`), so selects, scrollbars and checkboxes match.
- **Disc:** `/disc` is a WASMFS fetch backend over `data/`: the game's reads become 1 MiB range requests, each fetched once
  (`native/wotm_webfs.cpp`). `web/data_retry.js` retries network and 5xx failures in every thread and reports each chunk, which
  `web/loading.js` shows as "Downloading game data 34% (9 of 27 MB). Only the first visit downloads this much." over the game.
  After boot, a stage or monster not cached yet makes the game wait on its download on a black screen: while such a download goes on
  (over a second, gaps up to 3.5 s counted as the same load) a note in the middle of the screen says "Loading game data (N MB)"
  (`StreamActivity`; the count starts over after 10 s without a chunk). The chunks come from the disc channel: the fetch path's and, in
  Firefox, the game worker's synchronous reads (`syncChunkReader`'s `onBytes`). Two game tabs of one site share the channel, so a
  download in one shows the note in the other.
- **Disc cache:** `web/disc_cache.js` (loaded by the page with a script tag and, in the game's workers, by `data_retry.js`; it is not in the `--pre-js` list, so a change to it leaves the build id alone) keeps the disc's
  1 MiB chunks in the browser's Cache API, one cache per disc file and version (its ETag), each chunk stored with its SHA-256 and checked
  before use; a chunk missing or failing the check comes from the network. Nothing is stored when the Cache API is missing, the HEAD
  gave no ETag or storage is 90% full. From the page's load, while the player is on the front panel, `web/boot.js` drops the caches
  of other versions and fetches in parallel the chunks every boot reads (`web/boot_chunks.json`, written by
  `python3 tools/web_boot_chunks.py <boot log> <SCUS_971.97>` from a boot to the title); the game starts once they are in. For an online match
  `web/match_prefetch.js` fetches the chunks of the lobby's stage and of each member's monster and costume (`web/match_chunks.json`)
  once the lobby's choices have held still for 1 s, four at a time, a new choice stopping the fetch of the one it replaces; the
  game's boot asks again at once for what is still missing. A fight's load reads the stage, its two music tracks whole and each
  monster one file after another, so on a slow line a match waits at its first frames for the slowest player's download. `match_chunks.json` comes from fight logs: `node tools/web_match_logs.mjs` (one direct fight per monster with that monster
  in both seats on stage 1, one per stage) then `python3 tools/web_match_chunks.py <data>/game.iso runtime/match_logs`: a file read in
  every monster's fight is the stage's, the rest of a monster's fight is the monster's, and its costumes follow the disc's file names
  (`MON/<NAME><costume>.*`, `RST/<NAME><costume>.AIC`). A map of the player's own (a map id as the stage) prefetches its monsters
  only. Once the boot's chunks and the wasm are in, `web/game_prefetch.js` fetches the rest of the game in the background: every chunk
  of `match_chunks.json` (every stage, every monster with all its costumes) and the boot's, about 270 MiB, two 2 MiB runs at a time,
  skipping what the cache holds (`cachedChunks` reads the cache's keys only), so no later fight or menu waits on the network. It
  makes way for the game: from the game's start it holds until the runtime is up (the wasm download is no disc read), and any game
  read pauses it until the disc has been quiet for 5 s; a read says so as it starts (`{want}` on the `wotlane-disc` channel, from
  `cachingFetch`'s `onRead` and the Firefox XHR path), before the cache is asked, so a load the cache serves pauses it too (it wants
  the page's thread); a download goes on the channel as it ends (a number) and a cache hit as `{hit}`; the prefetch's own downloads
  go as `{prefetch: bytes}`, which the loading text, the streaming note and the diag leave out. It stops for good when an online
  match starts (the bandwidth is the match's), does nothing with Save-Data on or without a version (ETag) for a disc file, asks
  before every pass for room under 90% of the quota (the HTTP cache's copies of the chunk files count there too), and ends when a
  pass that asked for chunks left as many missing (the network keeps failing them, or the cache keeps nothing). In a fight it runs
  once the disc is quiet; its cost is the SHA-256 and the store of each chunk on the page's thread: in Chrome at 20 Mbit/s the
  seconds with background downloads present frames as the seconds without (median 8.3 ms, 1.7% over 25 ms against 1.9%); in
  Firefox on a local server, about 200 MiB in 2 to 4 s, 0 and 3.4% of those seconds' frames went over 25 ms (0.75% without); a real
  line spreads the same work over a minute or more. It never calls `navigator.storage.persist()`: Chrome and Brave answer false to
  a new visitor and Firefox would ask with a prompt; the quota (2 GiB in Brave, 10 GiB in Chrome and Firefox on a desktop) holds the
  set. `node tools/browser_mp_test/prefetch_probe.mjs` measures it. In Firefox
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
  interpolation off; Vsync has no effect in the browser. Online matches stay in step because nothing the drawing does reaches the game,
  and they never blend (`interp::wanted`): the newest game frame goes to the screen as soon as it is captured, since showing it a frame late
  is input lag.
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
- **Main thread profile:** `node tools/web_profile.mjs <chrome|brave> <page url> [secs]` opens a direct AI fight, prints the presented frames
  per second, the page's work per present, what the page sees (cores, WebGL renderer, timer resolution) and the functions with the most
  self time in a CPU profile. `WOTM_BRAVE` names Brave's binary, `WOTM_BROWSER_ARGS` adds browser flags (`--disable-gpu`: software WebGL).
- **Address switches** (`web/pageenv.js`, tested in `pageenv.test.mjs`): only `WOTM_MATCH`, `WOTM_AIVSAI`, `PS2X_STATS`, `WOTM_MAPEDIT` (the Settings page's Mods tab, Your creations, has a Map editor row that opens the page with it, on a stock stage) and `WOTM_VBLANK_SPIN` (a lone `0`: the vblank clock without its spin, for A/B runs; any other value keeps the spin) are read from the page address; the disc, save and renderer
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
  from the game. The Mods tab holds Your creations and the Map editor row. The Controls tab rebinds the keyboard (`web/keybinds.js`,
  `web/settings_ui.js`): each game button shows its keys (up to two, as the game takes), a click waits for a key, a mouse button or
  the wheel (Esc cancels; F1 to F12 stay with the game's runtime) and saves at once as `KB_<BUTTON>=KEY[,KEY]` lines plus `KB_LAYOUT` (the game
  reads the keys only for the layout number it knows, `kKbLayout`), through the same queue as the options. A key another button
  already has swaps (the other button gets the key the slot held); the card says which button got what. Letters show as the keyboard prints them where
  the browser has the Keyboard Map API (`navigator.keyboard.getLayoutMap()`, Chromium browsers only). Presets (WASD, arrow keys move) and Reset
  keys rewrite every row. Buttons the card does not list keep their lines (gamepad buttons are changed in the game's panel). The
  touch controls are listed beside it (`web/controls.js`; `controls.test.mjs` fails when a default key is missing from it, and
  `keybinds.test.mjs` reads `native/wotm_settings.hpp` for the key names and the layout number). `browser_mp_test`'s `single` session
  sets two options through the page and checks the game's log for them (`settings_page`); its `remap` session rebinds keys and holds
  one in a match (`remap_card`, `remap_mouse`, `remap_conflict`, `remap_file`, `remap_game`).
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

**Keyboard and mouse in the game:** a browser takes Esc to leave fullscreen and the pointer lock, and a page cannot prevent it, so the pause (which has the options) opens on Tab and Enter (`kDefaultKb`, `docs/passos/RUNTIME.md`) and Esc only closes it. While the game runs `web/gameinput.js` stops the browser moving focus on Tab (Ctrl, Alt and Cmd with Tab are left to the browser and the system). On a computer the mouse cursor over the game hides after 2 s without movement and shows on the next move; a touch screen is left alone. A settings file from the previous key layout (3) keeps the player's own keys; only Start and next target, when still on the old defaults, move to the new ones (`migrateKb`, `KB_LAYOUT` 4).

**Pings:** while a lobby waits, the server pings each member's `/lobby` WebSocket every second and sends the lobby a `PINGS <code> <name:ms,...>` line with the round trips (`-` for a native member on TCP, which the server can't measure). Each member row shows the ping as a chip: green under 80 ms, yellow under 150, red from 150 (`web/lobby_status.js`). A member with a red ping gets a warning under the list ("so everyone's input will feel laggy"). `PINGS` is an extra line: a client that doesn't know it skips it, so the protocol number stays 5.

**Device health:** a second chip says how the member's browser ran its last online match, so a device that can't keep up shows before the match starts. `web/rate.js` (`RateMemory`, fed every diag window) keeps the median of the last minute of the device's own frames a second (`ownRate`: frames over the time not spent waiting for the network), its ms of work per frame and the match's Hz in localStorage (`wotlane.deviceHealth`), once a match has run 20 s past its loading seconds. On joining a lobby the page sends `HEALTH <own fps> <work ms> <hz>` (numbers in range, else an ERROR), with ` sw` at the end when its browser draws WebGL in software (`gl_sw`), or `HEALTH sw` alone before it has run a match; once a second the server sends the lobby `HEALTH <code> <name:fps:work_ms:hz[:sw],...>` for the members that sent one (`-` for numbers not measured yet). A member drawing in software is red for everyone with "software graphics (hardware acceleration off)" and a warning, whatever the numbers say. The page judges the numbers against the match's rate (`deviceLevel`): green from 92% of it, red under 75%, and work per frame over 72% of the frame time is yellow, over 95% red (a 30 Hz phone is judged against 30). A red device gets a warning for everyone; a member who has never played online shows "device not measured yet". The numbers come from the member's own page, so they are a hint, not a check. `HEALTH` is an extra line for older clients to skip; a server from before `sw` refuses those forms with an ERROR that the page ignores.

**Brave and software drawing:** Brave is supported: with the graphics card it draws the game like Chrome (same frame rate and main-thread profile, Shields at their defaults, microsecond timers), and the page treats it like any other browser. A browser drawing WebGL in software (graphics acceleration off, or the GPU blocked) blocks its main thread in GL calls, and an online match slows to it for everyone. Whether the caveat probe catches Windows' WARP renderer in Chrome, Edge and Brave, Linux's llvmpipe or Safari is untested. Brave masks the renderer string (the diag's `gpu=Brave`), so the diag's `gl_sw` asks for a context that fails on a major performance caveat instead. `web/device_notice.js` puts a note on that player's own front panel (`#deviceNote`): "This browser is drawing the game without your graphics card ..." with how to turn acceleration on (Brave and Chrome: Settings, System, "Use graphics acceleration when available"; Edge: the same under System and performance; Firefox: Settings, General, Performance, "Use hardware acceleration when available"; then restart the browser); the lobby shows it to the others through `HEALTH ... sw` (device health above).

**Firefox tip:** a Firefox player's front panel (`#firefoxTip`, `web/firefox_tip.js`) suggests setting `javascript.options.wasm_lazy_tiering` to false in about:config and restarting Firefox, so the game's code is fully optimized as it loads instead of in the background (a slower first load); it says the setting applies to every website and how to undo it, and on Android that about:config is generally only in Firefox Beta or Nightly. "Got it" hides it for good in that browser (`localStorage`); it is never shown during a match.

**Owner controls:** while the lobby waits, the owner has Make owner and Remove on every other member's row (each asks "Sure?" once). `OWNER <name>` makes that member the owner: the server answers everyone with `MEMBERS`, and the page of the old owner drops the stage, wins and Start controls. `KICK <name>` removes a member: the server sends them `KICKED <code> <seconds>`, takes them out of the lobby (the others get `MEMBERS`) and keeps their connection open, so they land on the Host/Join choice with the reason; a `JOIN` of that lobby under the same name is refused ("you were removed from this lobby; try again in N s") for `ServerConfig::kickBanMs` (60 s); the ban is the lobby's and goes when it closes. The ban is by name and by address: the server keeps the removed member's address in memory only (never logged or sent) and refuses a join from it under any name. The address is the TCP peer's, except when that peer is this machine (the Caddy in front of the server): then it is the client address Caddy passes on, Cloudflare's `CF-Connecting-IP`, else the first `X-Forwarded-For` hop (what Caddy sets for a client that reaches it without Cloudflare); a `CF-Connecting-IP` that is present but not an address gives no address (the `X-Forwarded-For` beside it would be Cloudflare's own edge); with neither header the ban is by name alone. Addresses are compared as lower case text, so IPv6 spellings and IPv4-mapped forms count as different addresses. Members behind one shared address (a household) are banned together for the minute. The server cannot tell whether a request came through Cloudflare, and Caddy passes `CF-Connecting-IP` on unchanged, so a client that reaches Caddy without Cloudflare (the origin address with the site's Host header) can put any address in it: it can dodge its own ban, or, from inside a lobby it gets kicked from, ban an address it knows for a minute in that lobby. Closing that is a Caddy change (accept the header only from Cloudflare's ranges), not a server one. Both commands are owner only and refused once the match has started. `KICK`, `OWNER`, `HEALTH` and `KICKED` are extra lines, so the protocol number stays 5.

**Quick rematch (server):** a client that says `HELLO <protocol> <build> rejoin=1` gets `TOKEN <32 hex>` after it joins or creates a lobby. If its lobby connection drops (a page reloads after a match), it becomes a ghost for `ServerConfig::lingerMs` (120 s): its place, name, monster and token are kept, it is left out of `MEMBERS`, its name stays taken and its seat stays held (a `JOIN` counts members and ghosts against the lobby's size). An owner's ghost keeps the lobby's ownership (`MEMBERS` names no owner meanwhile); when it expires the lobby passes to the first member, as when an owner leaves. `REJOIN <code> <token>` brings the member back with the same name and monster and a new token (a token works once), after the same checks as a `JOIN` (the owner's removals, a match on, a full lobby counting members only, another build, the monster's costumes); it is refused for the lobby the connection is already in, and when the place's old connection is still open (a phone that switched networks) that connection gives the place up first. `KICK` removes a ghost too, with the same ban. `LEAVE` gives the token up. A lobby left with only ghosts takes no `JOIN` ("its players are coming back"), isn't listed, and stays open until they expire; at most `ServerConfig::maxLingering` (4) lobbies do at once, and past that a lobby whose last member drops or leaves closes as before. Clients without `rejoin=1` (the native launcher) behave as before. `TOKEN` and `REJOIN` are extra lines, so the protocol number stays 5.

**Chat:** a text box in the lobby sends `CHAT <text>` (up to 200 bytes of printable text; the server drops control characters and trims the ends, and allows 5 lines per 10 s per member, then answers "chatting too fast"). The server sends `CHAT <code> <name> <text>` to everyone in the lobby, the sender too, and only while the lobby waits (`ERROR chat is only open before the match` after the start). The page shows the last 50 lines as plain text (`textContent`) and clears them when it joins a lobby. `CHAT` is an extra line for older clients to skip, so the protocol number stays 5.

**Diagnostics:** the page reports how an online match runs to the server (`web/diag.js`, written up in `docs/ONLINE.md`,
"Diagnostics"). It sends `DIAG` lines over the lobby connection, which stays open during the match: one on joining a lobby, one every 5
s of the match and a last one when it ends (`wotmDiagFinish`, called by `wotmQuit` and Leave the match in `boot.js` before the page
reloads, with a 300 ms wait for the socket to empty; `pagehide` also sends it). The reload itself (`web/reload.js`) happens when the saves and
that report are done or the report has had 2 s (the saves get 10 s), and the deadline is checked from MessageChannel tasks, never a timer, so a
page whose timers are starved still returns to the front panel. `DiagReporter` samples every 500 ms (the ping, for its
jitter) and builds each report from the difference of two samples. Its sources only read: the game loop's presented-frame counter
(`globalThis.wotmFrames`), the match counters the game publishes (`wotmWebNetStats` in `native/wotm_web.h`, set every game frame in
`netPace`, read through the exported `wotm_web_net_stat(i)`: frame, waits, wait ms, delay, ping, the game thread's idle ms), the input
latency window (`latencySource`: `wotm_web_latency_take` then `wotm_web_latency(i)`, once per report; the page feeds the probe each
keydown time in `boot.js` and each pad button press in `touch.js` through `wotm_web_input_at`, which only stores a time), `FrameWatch`
(animation frame intervals), `LongTaskWatch` (a `PerformanceObserver` for `longtask`; Firefox has none, so those fields are left out),
and `installAudioWatch`, which adds an `audioprocess` listener to each `ScriptProcessorNode` the game's audio makes (installed when the
page loads; a callback later than 1.5 buffers counts as a dropped buffer; a page whose audio doesn't use script processors reports no
`au_` fields). Nothing here writes to the game, so determinism is untouched; the wasm change adds counters and an export, and changes
the build id like any change under `native/`. `LobbyClient.diag()` sends a line and stops sending once the server answers `ERROR
unknown command: DIAG`; `sendDiag()` resolves once a `LIST` sent after the line is answered, which is how the page knows the server
read it.

**Send report** (`web/report_ui.js`): the card the front panel's Send report button (shown after a match; the page keeps the last report in
`sessionStorage` under `wotlane.lastdiag`) and Settings, Online tab, open. It sends the last match's numbers, this device and a note of up
to 200 characters (`why=report`). From a lobby the player is still in it uses that connection; otherwise a short-lived one (`deliverReport`,
which gives up after 6 s) whose report the server files as a claim. The card says what is sent: the name, browser and device type, the
match numbers and the note, and no chat, files or address.

A refusal from the server (name taken, no such lobby, lobby full, lobby in a match, build differs) stays on the message line, as a plain sentence (`web/messages.js` maps every ERROR the lobby sends; a test reads them from `wotm_net_server.cpp`, and an unknown text is shown as it is). The live server runs with `--lobbies 4`: a fifth lobby creator reads "All 4 lobbies are busy. Try again soon." Leave sends `LEAVE` and returns to Host/Join without a reload (`LobbyClient.leave()`). The page's own
`LobbyClient.close()` silences `onClose`, which only reports a connection that dropped. The panel is a flex column that scrolls
(`#front` has `overflow-y: auto`, its card `margin: auto`), so a card taller than the window (8 members, a zoomed page) keeps Start
reachable. After a match (or Leave the match, which reloads the page) the page goes back to its lobby by itself (quick rematch,
`web/rematch.js`): every lobby connection says `rejoin=1`, the page keeps the `TOKEN`, the lobby code and this player's name and monster pick in the tab's `sessionStorage`
(so two tabs keep their own places; a duplicated tab copies them and takes the original's), and on load it sends `REJOIN` with them, once and only within the server's 120 s, counted
like the server's from the moment the page went away (`pagehide`). It waits for the members list to know whether it owns the lobby
again, then shows the lobby with the owner's Start button reading Rematch and the owner's controls showing the lobby's stage, wins
and modifiers. Leave, a removal by the owner and an invite link drop the token. When the rejoin is refused (the lobby is gone, the place
expired, a match is on), the panel shows the server's reason and the Host/Join choice. Without a token the panel asks the server for its
lobbies (`LIST`, over a short-lived `LobbyClient`) and offers the last lobby's code only while that lobby is listed. A lobby still in a
match is named as such; one that is gone gets no note and no prefilled code.

`wotm_server --ws-port <port> [--ws-bind <address>]` (default bind `127.0.0.1`, behind a web server that terminates HTTPS) takes these
connections: `/lobby` behaves as a TCP lobby connection, `/match` carries binary frames of one channel byte and the same datagram as
ENet, and the server measures browser round trips with WebSocket pings. Browser, Mac and Windows players can share a match. A WebSocket
is reliable and ordered, so the server sends a browser each input once (an ENet player gets what it lacks again every 16 ms): a browser
whose network stops is sent only pings, not a growing pile of repeats. A browser that comes back sends everything it queued at once; the
server reads a connection 64 KB at a time, so any burst is fine (only a lobby line over 64 KB closes the connection).
Each player has their own input delay, re-picked by the server every 500 ms from the round trips (`adviseDelay` in
`native/net/wotm_net_proto.hpp`, `docs/ONLINE.md`): half their round trip plus half the slowest other player's, at the level each round
trip stays at or under 4/5 of the last 8 s (`RttWindow` riseLevel: up to 3 high samples of 16 are left out, so a short spike costs a
wait instead of raising everyone's delay; the delay falls only for fallLevel, which leaves out one), plus 3 ms. With everyone at a
steady 20, 60 and 120 ms that is 2, 4 and 8 frames in a 1 vs 1 (60 Hz) and 1, 2 and 4 frames with 3 to 8 players (30 Hz); jitter adds
to it. A match starts at the largest player's delay, and at least 2.
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
  bottom right (1.35 thumb units, grab 1.15), drawn as SVG line icons so every symbol has the same weight whatever fonts the phone has, two chord buttons (SP1 is triangle and circle, SP2 is cross and square, the
  game's specials) and one for lock-on (L1 and R1), L2 and L1 at the top left and R1 and R2 (block) at the top right, Select, a gear and Start
  at the top middle. Several fingers work at once; a press stays down at least 80 ms, so a tap shorter than the game's pad reads is still
  read. A quick tap (under 220 ms, under 12 px) on either zone is also a mouse click on the game: menu lines are picked by pointer, and in a
  fight a click is the light attack. The stick moves the menu selection, Cross chooses and Triangle goes back, as on a pad.
  Vibration (`navigator.vibrate`) answers a press and the game's own rumble. The gear opens size (0.7 to 1.2), opacity, vibration, Left handed
  and Move buttons, kept in `localStorage` (`wotlane.touch`). Left handed mirrors every control to the other side (the middle ones swap
  places) and swaps the stick and look zones; the layout is `layoutFor`, the zones `zonesFor`. Move buttons turns the controls into
  things to drag (a dashed outline, nothing is pressed): a drop puts the button's centre at the finger, whole on screen (a place that no longer fits a smaller screen or a larger size is pulled back in), saved as `place` (units
  from its anchor); Reset positions clears them, and so does switching hands. The gear itself does not move. `browser_touch_test`'s
  `touch_layout` scenario drives it.
- **Steps aside:** a key press, a mouse press or a real pad hides the controls and removes the virtual pad (a real pad would otherwise be player
  2); a touch brings them back unless a real pad is connected. While they are shown the page refuses the game's pointer lock (the mouse
  camera) and leaves any lock, which would zero every touch's coordinates.
- **Fullscreen and landscape:** the first tap on a button on a touch device asks for fullscreen on the whole page and a landscape orientation
  lock (the browser may refuse either); the gear panel has a Fullscreen button. In portrait the controls are replaced by "Turn your phone
  sideways to play". The screen is kept awake while the controls are up (Wake Lock, where the browser has it).
- **Phones:** the same game runs in Chrome on Android; there is no phone-specific build. Frame size is the game's 720 lines; the in-game
  VIDEO options (resolution, render scale) are the lever on a slow phone. Online, a phone's lobby `HELLO` carries `hz=30` and its matches
  run in the game's 30 Hz mode (`docs/ONLINE.md`, "30 Hz with a phone or a slow computer"): at 60 Hz a phone that computes fewer game frames plays
  everyone's match in slow motion, with broken sound. The server also recognizes a phone by its lobby WebSocket's User-Agent, so a stale
  page or a pad as the input still gets 30 Hz. In a match a phone, or a computer measured too slow for its matches (`web/rate.js`),
  also draws at 60 (no in-between frames) and at 75% render scale
  (`web/settings.js` `lightMatchDrawing`), leaving its fast cores to the game; a player who set Frame rate or Render scale keeps theirs.
  iPhones and iPads cannot run it (no JSPI before Safari 27).
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
- `python3 tools/test_serve_web.py`, `python3 tools/test_web_mods.py`, `python3 tools/test_web_boot_chunks.py`, `python3 tools/test_web_match_chunks.py` and `python3 tools/test_web_movies.py`;
- the WebSocket listener and transport: `sh tools/test_net.sh` (`ws_*` tests).
- online play in real browsers, 1 to 8 players against a local server or the live site: `node tools/browser_mp_test/run.mjs`
  (`tools/browser_mp_test/README.md`; its log readers: `node --test tools/browser_mp_test/test/`).
- `node tools/browser_input_test/run.mjs --browser all`: a real Chromium and Firefox on the page with no game: Tab does not move focus once the game
  runs, and the cursor over the game hides after 2 s idle and shows on a move.
- `node tools/page_check.mjs --browser chrome|brave|chromium|firefox|webkit` (the shared setup is `tools/page_env.mjs`): the front panel's accessibility and layout in a real, muted browser against a
  local `wotm_server` (no game is loaded): every rule checkbox has its own label, the first Tab stop is in the panel and the canvas is not a tab
  stop, an Enter that confirms an IME conversion does not send chat; computed text contrast of at least 4.5:1; control sizes of 44px on a
  touch phone and 24px with a mouse on the home, lobby and settings views (the Controls tab must not overflow the screen); the status bar at
  full opacity, readable and sized on desktop and touch; the touch layout panel fits a short phone and keeps its own button sizes; nothing on the panel moves after first paint, and a press shows
  at once. `--site <folder>` checks a copy of an older `web/`. Needs
  `sh tools/test_net.sh notest` first and Playwright.
- `node tools/page_shots.mjs --out <folder> --browser firefox|chrome|brave|webkit [--site <web folder>] [--label <name>]`: the same set of
  screenshots of any copy of `web/` (home, lobby with two players, settings, controls, the touch layout panel; phone and desktop), for before
  and after comparisons.
- `node tools/browser_diag_test/run.mjs`: a real Chromium against a local `wotm_server --diag-dir` (no game is loaded): the join,
  periodic and last reports and a Send report from the front page reach the server's files, with no chat text and no user agent string in
  them. Needs `sh tools/test_net.sh notest` first and Playwright (see `tools/browser_mp_test/README.md`).
- `node tools/browser_lobby_test/run.mjs --browser all`: three players in a real Chromium, a real Firefox and the installed Brave
  (skipped where it is not installed) against a local `wotm_server` (no game is loaded): the ping and device health chips, a fourth player
  drawing in software (their notice, the others' red chip and warning), Remove with its ban, and Make owner. Needs `sh tools/test_net.sh notest`
  first and Playwright.
