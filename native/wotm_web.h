// ABOUTME: Browser build only: the two calls the runtime makes into the web layer (wotm_webfs.cpp).
// ABOUTME: wotmWebMount prepares the file system before the ELF loads; wotmWebPresent hands each finished frame to the browser.
#pragma once

// /disc streams the disc from the page's data/ folder (HTTP range requests, fetched as the game reads), /save keeps settings and saves
// in the browser's private file system (OPFS) and becomes the current folder, where the native layer writes them
void wotmWebMount();

// waits for the browser's next animation frame, so the page stays responsive and the frame reaches the screen
void wotmWebPresent();
