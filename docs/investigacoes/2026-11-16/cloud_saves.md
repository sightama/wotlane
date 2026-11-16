# Cloud saves for the browser version: a design note

Not built. The question: a player on another browser or computer should be able to carry their saves over without handing a zip around.
The model to learn from is the passphrase vault of th0mk/nfsu2-web (`web/src/app/vault.ts`, MIT, nfsu2.xyz). Backup and Restore through a zip
already exist (Settings, Saves tab), so this is a convenience on top, not a missing safety net.

## How nfsu2.xyz does it (read from `vault.ts`)
- The player gets a generated passphrase (a fixed prefix and 8 words from a 1296-word list, about 2^83). The passphrase is the only secret.
- The browser derives 64 bytes with PBKDF2-SHA-256 (100,000 rounds, fixed salt). The first 32 bytes are an AES-256-GCM key, the last 32 a
  token. The server keeps the box under the token's hash and never sees the key.
- What is sent: the save files as a zip, then sealed (version byte, 12-byte IV, ciphertext and tag). The server takes at most 256 KB and keeps
  a copy of an earlier day, and drops a box unused for two years.
- Sync rules do the real work: send after a save and on close if the files changed; take when the page opens if another browser sent a newer
  version and this one did not change; both changed means a conflict and the player picks; never replace a populated profile with an empty
  one, nor with an older version.

## What it would take here
- **What goes up:** `save.json` and `mc0/` `mc1/` (what Backup writes). Card files are small except `ICON.ICO` (61,120 bytes per card, which
  zips well), so a box should stay under 128 KB. A 256 KB cap is generous. `settings.ini`, creations and `backups/` stay local (a creation
  is a mod and online play compares mods, so syncing them changes the match hash).
- **Server:** `wotm_server` speaks a lobby protocol over WebSocket and has no HTTP storage. Options: a small `PUT/GET /vault/<token>` handler
  in the server with a file per token and a size cap, or a route on the web server in front of it. Either needs a quota (boxes per IP,
  total disk) because anyone can create a token.
- **Page:** a Cloud tab (or a section of Saves): create a passphrase (shown once, to be written down), enter one on another browser, Send
  now, Take now, and the conflict choice. `web/saves.js` already gives the file set, the zip, validation and the automatic backup before a
  replace, so Take is "Restore" with the downloaded box. Web Crypto needs no library.
- **Game lock:** sync only runs on the front panel, with no game in any tab (`web/gamelock.js`), so it never races the game's own writes.

## Decisions for Lane
1. Is carrying saves between browsers wanted enough to run storage on the server? Without it, Backup and Restore cover the need.
2. Passphrase only (nfsu2.xyz's choice, no accounts), or tie it to the lobby name? Recommended: passphrase only. No account system exists and
   a name is not a secret.
3. Retention and cap: 128 KB per box, one earlier copy kept, deleted after a year unused, a fixed number of boxes per IP.
4. Scope of sync: manual Send and Take buttons first (simple, no conflict logic beyond "this replaces that, with an automatic backup"),
   automatic sync later if players ask.

Estimated work with manual Send and Take: the server handler and its tests, `web/vault.js` (key derivation, seal, open; testable in node
with Web Crypto), a tab, a `browser_mp_test` session. About the size of the Saves tab.
