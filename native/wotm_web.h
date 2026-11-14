// ABOUTME: Browser build only: the calls the runtime makes into the web layer (wotm_webfs.cpp): the file system, each finished frame,
// ABOUTME: whether the tab is hidden, and a long network wait the page offers to leave.
#pragma once

// /disc streams the disc from the page's data/ folder (HTTP range requests, fetched as the game reads), /save keeps settings and saves
// in the browser's private file system (OPFS) and becomes the current folder, where the native layer writes them
void wotmWebMount();

// waits for the browser's next animation frame, so the page stays responsive and the frame reaches the screen
void wotmWebPresent();

// the page's tab is hidden (boot.js passes visibilitychange to wotm_web_set_hidden); any thread
bool wotmWebHidden();

// the online match has been waiting `ms` for the others (0: not waiting), with the on-screen text; after a long wait the page offers to
// keep waiting or leave the match (web/stall.js)
void wotmWebNetStall(double ms, const char *text);
