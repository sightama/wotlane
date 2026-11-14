// ABOUTME: Retries the streamed disc's requests (data/) when the network or the server fails for a moment, and tells the page how much
// ABOUTME: arrived. Linked as an Emscripten --pre-js: it runs in the page and in the worker that fetches the disc.
'use strict';

// `onBytes` hears the size of each disc chunk that arrives (the page shows the loading progress)
function retryingFetch(baseFetch, { tries = 5, delayMs = 500, onBytes = null } = {}) {
  const isDisc = (url) => /(^|\/)data\//.test(typeof url === 'string' ? url : url.url);
  return async function (url, init) {
    if (!isDisc(url)) return baseFetch(url, init);
    for (let attempt = 1; ; attempt++) {
      try {
        const r = await baseFetch(url, init);
        // 5xx and 429 are worth another try; anything else (including 404) is the answer
        if (r.status < 500 && r.status !== 429) {
          if (r.status === 206 && init?.method !== 'HEAD') onBytes?.(Number(r.headers?.get('content-length')) || 0);   // a chunk, not a size probe
          return r;
        }
        if (attempt >= tries) return r;
      } catch (e) {
        if (attempt >= tries) throw e;
      }
      await new Promise((resolve) => setTimeout(resolve, delayMs * attempt));
    }
  };
}

if (typeof module !== 'undefined' && module.exports) module.exports = { retryingFetch };
else if (typeof globalThis.fetch === 'function' && !globalThis.wotmFetchRetrying) {
  globalThis.wotmFetchRetrying = true;
  // the disc is fetched in a worker; the page listens on this channel (boot.js)
  const progress = typeof BroadcastChannel === 'function' ? new BroadcastChannel('wotlane-disc') : null;
  globalThis.fetch = retryingFetch(globalThis.fetch.bind(globalThis), { onBytes: (n) => progress?.postMessage(n) });
}
