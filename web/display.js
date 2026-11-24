// ABOUTME: The display the page is on: its refresh rate, measured from animation frames before the game starts (the game draws
// ABOUTME: in-between frames above 60 Hz), and the canvas size in device pixels (sharp on high-density screens).
'use strict';

// Measuring the rate from frame times follows nfsu2-web's measureRefresh (web/src/app/display.ts, https://github.com/th0mk/nfsu2-web,
// MIT License, Copyright (c) th0mk).
const COMMON_HZ = [30, 48, 50, 60, 72, 75, 85, 90, 100, 120, 144, 165, 170, 175, 180, 200, 240, 280, 360];

// times: requestAnimationFrame timestamps in ms. The median of 4-frame spans, so a few late frames don't count; within 4% of a common
// rate it reads as that rate. hz 0: too few frames to tell.
function refreshFromTimes(times) {
  if (times.length < 9) return { hz: 0, raw: 0 };
  const spans = times.slice(4).map((t, i) => (t - times[i]) / 4).sort((a, b) => a - b);
  const raw = 1000 / spans[spans.length >> 1];
  const snap = COMMON_HZ.reduce((a, b) => (Math.abs(b - raw) < Math.abs(a - raw) ? b : a));
  return { hz: Math.abs(snap - raw) / snap < 0.04 ? snap : Math.round(raw), raw };
}

// the rate over the next `frames` animation frames (the page must be idle meanwhile: a busy page's late frames read as a slower display)
function measureRefresh(frames = 60) {
  return new Promise((resolve) => {
    const times = [];
    const step = (now) => {
      times.push(now);
      if (times.length <= frames) requestAnimationFrame(step);
      else resolve(refreshFromTimes(times));
    };
    requestAnimationFrame(step);
  });
}

const DISPLAY_HZ_KEY = 'wotlane.displayHz';

function validRefreshHz(hz) {
  return Number.isInteger(hz) && hz >= 30 && hz <= 1000;
}

// cached display refresh rate for this tab (avoids re-running measureRefresh across reloads)
function readCachedRefresh(storage) {
  try {
    const raw = storage?.getItem(DISPLAY_HZ_KEY);
    if (!raw || !/^\d+$/.test(raw)) return 0;
    const hz = Number(raw);
    return validRefreshHz(hz) ? hz : 0;
  } catch {
    return 0;
  }
}

function writeCachedRefresh(storage, hz) {
  if (!validRefreshHz(hz)) return;
  try { storage?.setItem(DISPLAY_HZ_KEY, String(hz)); } catch { /* storage refused */ }
}

// a CSS size in device pixels at the page's pixel ratio
function devicePixels(cssW, cssH, ratio) {
  const r = ratio > 0 ? ratio : 1;
  return { w: Math.round(cssW * r), h: Math.round(cssH * r) };
}

if (typeof module !== 'undefined' && module.exports)
  module.exports = { refreshFromTimes, measureRefresh, devicePixels, readCachedRefresh, writeCachedRefresh };
else Object.assign(globalThis, {
  wotmRefreshFromTimes: refreshFromTimes,
  wotmMeasureRefresh: measureRefresh,
  wotmDevicePixels: devicePixels,
  wotmReadCachedRefresh: readCachedRefresh,
  wotmWriteCachedRefresh: writeCachedRefresh,
});
