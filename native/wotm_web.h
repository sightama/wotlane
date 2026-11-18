// ABOUTME: Browser build only: the calls the runtime makes into the web layer (wotm_webfs.cpp): the file system, each finished frame,
// ABOUTME: whether the tab is hidden, and a long network wait the page offers to leave.
#pragma once

#include <cstdint>

// /disc streams the disc from the page's data/ folder (HTTP range requests, fetched as the game reads), /save keeps settings and saves
// in the browser's private file system (OPFS) and becomes the current folder, where the native layer writes them
void wotmWebMount();

// /disc read in byte ranges without blocking the main thread (wotm_webdisc.cpp): `names` under the page's data/, with their sizes
bool wotmWebMountRangeDisc(const char *dir, const char *const *names, const uint64_t *sizes, int count);

// the save folder in memory, loaded from OPFS without blocking the main thread (wotm_websave.cpp); wotmWebSaveSync writes what changed
// in it since the last call back to OPFS, in the background (main thread; a no-op when the folder isn't this one)
bool wotmWebMountSaveMemory(const char *dir);
void wotmWebSaveSync();

// waits for the browser's next animation frame, so the page stays responsive and the frame reaches the screen
void wotmWebPresent();

// the page's tab is hidden (boot.js passes visibilitychange to wotm_web_set_hidden); any thread
bool wotmWebHidden();

// the online match has been waiting `ms` for the others (0: not waiting; negative: the match stopped), with the on-screen text; after a
// long wait the page offers to keep waiting or leave the match, and a stopped match gets a way back to the front page (web/stall.js)
void wotmWebNetStall(double ms, const char *text);

// the online match's counters, for the page's diagnostics report (web/diag.js reads them through wotm_web_net_stat; read only, they
// never reach the game): the game frame, how many times the game waited for the other players and the ms it waited in all, the input
// delay in frames and the ping in ms; any thread
void wotmWebNetStats(uint32_t frame, uint32_t waits, double waitMs, uint32_t delay, uint32_t pingMs);

// the game quit (its menu's quit game): the page goes back to its front panel (boot.js wotmQuit); any thread
void wotmWebQuit();

// the page's size in device pixels (window.innerWidth and innerHeight times devicePixelRatio, web/display.js); any thread
void wotmWebPageSize(int &w, int &h);

// device pixels per CSS pixel (window.devicePixelRatio, 1 when the page has none); any thread
double wotmWebPixelRatio();
