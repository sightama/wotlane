# Stale page scripts after a deploy: Cloudflare's Origin-keyed copies (2026-11-25)

After the deploy of wip/browser 6a37752e (#218, the simulation id; #219, wire levels), the muted live smoke failed twice: the Firefox
tab was refused with "That lobby runs a different version of the game". Short answer: **Cloudflare kept a second cached copy of a
page script for requests that carry an `Origin` header, and the deploy's purge by URL did not clear it.** A browser sends an `Origin`
when it fetches a module script (the page loads `front.js` as `<script type="module">`), so a browser can get the previous deploy's
scripts for up to the edge's cache time (one day: `s-maxage=86400` in the coord repo's `web/Caddyfile`). How many browsers hit that
copy was not measured: in this smoke Chrome did and Firefox's tab got the current file.

## What was measured

- Both tabs' join lines on the server: Chrome sent `build=wl-5cc9a7c890df08fd` (the full id) and no `full=` field, which is what the
  `front.js` from before #218 sends when it reads the new `version.json` (it took `build`; the new one takes `sim` and adds `full=`);
  Firefox, with the new `front.js`, sent the simulation id, and the server refused the difference.
- A fresh headless Chrome (Playwright, no browser cache) got `front.js` with `cf-cache-status: HIT`, `age: 32645` and the ETag
  `dn3uia4qpbsz-oea-gzip`: a copy cached about 9 hours earlier, around the deploy of b8d64391 (the time is inferred from the age).
  curl got the new file (`dn460mm2al7z-osp-gzip`) for the same URL and encoding.
- Replaying Chrome's request headers with curl, one at a time: with `Origin: https://playwotm.com` the stale copy, without it the new
  one; the User-Agent, `Accept-Encoding`, the `Sec-Fetch-*` headers, IPv4 or IPv6 and HTTP/1.1 or HTTP/2 made no difference.
- The coord repo's `scripts/check_edge_fresh.sh`, which proves the edge fresh after every deploy, asked each file as identity, gzip
  and br but never with an `Origin`, so it passed (159 answers).

## Why

Cloudflare's default cache key includes "Origin header sent by client (for CORS support)" (developers.cloudflare.com/cache/how-to/
cache-keys). A purge by URL reaches a copy cached with request headers when the purge names them: the purge API (`POST
/zones/{zone_id}/purge_cache`) takes `files` as objects with `url` and `headers` for that (developers.cloudflare.com/api/resources/
cache/methods/purge). The coord repo's `cloudflare_site.sh purge-files` sent plain URLs, which is consistent with the stale copy
surviving the deploy's purge.

## What was done

- On the day: a Purge Everything (`cloudflare_site.sh purge`, `deploy_web.sh`'s own fallback). The `Origin` copies were fresh after
  it, the smoke passed 6 of 6, and both tabs' join lines show the simulation id with `full=` next to it.
- Coord PR #13: `purge-files` also purges each URL as `{url, headers: {Origin: <site>}}`, and `check_edge_fresh.sh` asks every file
  without and with an `Origin` (318 answers for the current web root, all fresh); a stale `Origin` copy then fails the deploy check,
  which falls back to Purge Everything.

## Next

- #187 (versioned builds) puts each build's page scripts under `/b/<build id>/`: a new build's scripts are then at new addresses, so no
  cached copy of an older one can be served for it, and only the small entry page has to be fresh. Not built yet.
