// ABOUTME: What the lobby shows beside each member: the colour level of their ping and of their device's health (how it ran its
// ABOUTME: last match), the words for them, and the warnings about a member who would spoil the next match.

import { HOLD_SHARE } from './rate.js';

export const PING_GOOD_MS = 80;    // under this a ping is green
export const PING_BAD_MS = 150;    // from this it is red: the input delay grows by several frames
const DEVICE_GOOD = HOLD_SHARE;        // a device holding at least this share of the match's rate is green (the share rate.js calls keeping up)
const DEVICE_BAD = 0.75;           // under this share it is red (45 of 60)
const WORK_WARN = 0.72;            // work per frame over this share of the frame time is a warning (12 ms of 16.7)
const WORK_BAD = 0.95;             // ... and red when it fills the frame (16 ms of 16.7)

const ORDER = ['good', 'warn', 'bad'];
const worst = (a, b) => (ORDER.indexOf(a) >= ORDER.indexOf(b) ? a : b);

export function pingLevel(ms) {
  if (typeof ms !== 'number') return 'none';
  return ms < PING_GOOD_MS ? 'good' : ms < PING_BAD_MS ? 'warn' : 'bad';
}

// `health`: { fps (frames a second of the device's own time), workMs (per frame), hz (the match's rate) } or undefined
export function deviceLevel(health) {
  if (!health) return 'none';
  const share = health.fps / health.hz;
  const frameMs = 1000 / health.hz;
  const byRate = share >= DEVICE_GOOD ? 'good' : share >= DEVICE_BAD ? 'warn' : 'bad';
  const byWork = health.workMs >= WORK_BAD * frameMs ? 'bad' : health.workMs >= WORK_WARN * frameMs ? 'warn' : 'good';
  return worst(byRate, byWork);
}

const rate = (h) => `${Math.round(h.fps)} fps of ${h.hz}`;

// `pings`: name to ms (null: not measured); `healths`: name to health
export function memberStatus(name, pings, healths) {
  const ms = pings[name];
  const ping = pingLevel(ms);
  const h = healths[name];
  const level = deviceLevel(h);
  const text = { none: 'device not measured yet', good: 'device ok', warn: `device borderline: ${h && rate(h)}`, bad: `device too slow: ${h && rate(h)}` }[level];
  return { ping: { level: ping, text: ping === 'none' ? '' : `${ms} ms` }, device: { level, text } };
}

export function lobbyWarnings(names, pings, healths) {
  const out = [];
  for (const name of names) {
    if (pingLevel(pings[name]) === 'bad') out.push(`${name}: ping ${pings[name]} ms is high, so everyone's input will feel laggy.`);
    if (deviceLevel(healths[name]) === 'bad') out.push(`${name}: this device runs ${rate(healths[name])} and would slow the match for everyone.`);
  }
  return out;
}
