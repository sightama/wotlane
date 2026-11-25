// ABOUTME: Versioned builds (/b/<build id>/, #187): the newest retained build of a simulation id from /b/index.json, and the address a
// ABOUTME: page moves to when a lobby runs another build, so a deploy mid-session doesn't turn its players away. Page module.

const BUILD_ID = /^wl-[0-9a-f]{16}$/;

// the newest build in the index (`{builds: [{full, sim, time}]}`, written by the deploy) whose simulation id is `sim`, or null
export function buildForSim(index, sim) {
  const builds = Array.isArray(index?.builds) ? index.builds : [];
  const same = builds.filter((b) => b && b.sim === sim && BUILD_ID.test(b.full ?? ''));
  same.sort((a, b) => String(b.time).localeCompare(String(a.time)));
  return same[0]?.full ?? null;
}

// the site's own root above a build's address: /b/<id>/ is /, /play/b/<id>/ is /play/
export function siteRoot(pathname) {
  return pathname.replace(/b\/[^/]+\/$/, '');
}

// The server refuses a member of another build with "build differs from the lobby's (<simulation id>)". When the index keeps a build
// with that id, the address of it with the lobby code (`?join=`) and the page's other parameters; null when there is nothing to
// follow, or the page is already there.
export function lobbyBuildUrl(error, code, index, location) {
  const m = /^build differs from the lobby's \((wl-[0-9a-f]{16})\)/.exec(error ?? '');
  if (!m || !code) return null;
  const full = buildForSim(index, m[1]);
  if (!full) return null;
  const root = siteRoot(location.pathname);
  const path = `${root}b/${full}/`;
  if (location.pathname === path) return null;
  const params = new URLSearchParams(location.search ?? '');
  params.set('join', code);
  return `${location.origin}${path}?${params}`;
}
