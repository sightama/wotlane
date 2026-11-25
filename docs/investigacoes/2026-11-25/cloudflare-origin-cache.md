# Stale page scripts after a deploy: Cloudflare's Origin-keyed copies (2026-11-25)

After the deploy of wip/browser 6a37752e (#218, the simulation id; #219, wire levels), the muted live smoke failed twice: the Firefox
tab was refused with "That lobby runs a different version of the game". Short answer: **Cloudflare kept a second cached copy of each
file for requests that carry an `Origin` header, a purge by URL did not clear it, and every module script a page loads sends an
`Origin`.** Real browsers have most likely been getting the previous deploy's module scripts for up to the edge's cache time (one
day, `s-maxage=86400`) after every deploy since the page became ES modules; it went unnoticed while old and new scripts were
compatible.

## What was measured

- Both tabs' join lines on the server: Chrome sent `build=wl-5cc9a7c890df08fd` (the full id) and no `full=` field, which is the
  `front.js` from before #218 reading the new `version.json`; Firefox, with the new `front.js`, sent the simulation id, and the server
  refused the difference.
- A fresh headless Chrome (Playwright, no browser cache) got `front.js` with `cf-cache-status: HIT`, `age: 32645` and the ETag
  `dn3uia4qpbsz-oea-gzip`: a copy cached about 9 hours earlier, at the deploy of b8d64391. curl got the new file (`dn460mm2al7z-osp-gzip`)
  for the same URL and encoding.
- Replaying Chrome's request headers with curl, one at a time: with `Origin: https://playwotm.com` the stale copy, without it the new
  one; the User-Agent, `Accept-Encoding`, the `Sec-Fetch-*` headers, IPv4 or IPv6 and HTTP/1.1 or HTTP/2 made no difference.
- `scripts/check_edge_fresh.sh` (coord repo), which proves the edge fresh after every deploy, asks each file as identity, gzip and br
  but never with an `Origin`, so it passed (159 answers).

## Why

Cloudflare's default cache key includes "Origin header sent by client (for CORS support)" (developers.cloudflare.com/cache/how-to/
cache-keys). A purge by URL reaches a copy cached with request headers only when the purge names them: `purge_cache` takes `files` as
objects with `url` and `headers` for that (developers.cloudflare.com/api/resources/cache/methods/purge). The deploy purged plain URLs.

## What was done

- On the day: a Purge Everything (`cloudflare_site.sh purge`, `deploy_web.sh`'s own fallback). The `Origin` copies were fresh after
  it, the smoke passed 6 of 6, and both tabs' join lines show the simulation id with `full=` next to it.
- Coord PR #13: `purge-files` also purges each URL as `{url, headers: {Origin: <site>}}`, and `check_edge_fresh.sh` asks every file
  without and with an `Origin` (318 answers for the current web root); a stale `Origin` copy then fails the deploy check, which falls
  back to Purge Everything.
- #187's `/b/<build id>/` paths take the page's module scripts out of the purge path altogether: each build's scripts live at a new
  address, so no cached copy of an older one can be served for it, and only the small entry page has to be fresh.
