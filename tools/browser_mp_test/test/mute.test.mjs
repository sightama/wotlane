// ABOUTME: Tests that every browser the checks start is muted (Lane's rule: no test makes a sound): the Chromium-family launch
// ABOUTME: options and Firefox's preferences from lib/infra.mjs. Run: node --test tools/browser_mp_test/test/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { chromiumOptions, firefoxOptions, muted, MUTE_ARGS, MUTE_PREFS } from '../lib/infra.mjs';

test('every Chromium-family browser starts muted', () => {
  for (const browser of ['chromium', 'chrome', 'brave'])
    for (const unlockFrames of [false, true])
      assert.ok(chromiumOptions({ browser, unlockFrames }).args.includes('--mute-audio'), `${browser} unlockFrames=${unlockFrames}`);
  assert.deepEqual(MUTE_ARGS, ['--mute-audio']);
});

test('Firefox starts with its volume scaled to 0, also when other preferences are asked for', () => {
  assert.equal(firefoxOptions({}).firefoxUserPrefs['media.volume_scale'], '0.0');
  const opts = firefoxOptions({ headed: true, prefs: { 'a.b': 1 } });
  assert.equal(opts.firefoxUserPrefs['media.volume_scale'], '0.0');
  assert.equal(opts.firefoxUserPrefs['a.b'], 1);
  assert.equal(opts.headless, false);
  assert.deepEqual(MUTE_PREFS, { 'media.volume_scale': '0.0' });
});

test('muted() adds the switch to any launch options and keeps the rest', () => {
  assert.deepEqual(muted('chromium', { channel: 'chrome', args: ['--a'] }), { channel: 'chrome', args: ['--a', '--mute-audio'] });
  assert.deepEqual(muted('chromium'), { args: ['--mute-audio'] });
  assert.deepEqual(muted('firefox', { headless: true, firefoxUserPrefs: { 'a.b': 1 } }),
    { headless: true, firefoxUserPrefs: { 'a.b': 1, 'media.volume_scale': '0.0' } });
  assert.deepEqual(muted('webkit', { headless: true }), { headless: true });
});

test('Chrome and Brave keep their own launch options next to the mute', () => {
  const chrome = chromiumOptions({ browser: 'chrome' });
  assert.equal(chrome.channel, 'chrome');
  assert.deepEqual(chrome.args, ['--autoplay-policy=no-user-gesture-required', '--mute-audio']);
  const brave = chromiumOptions({ browser: 'brave', unlockFrames: true });
  assert.ok(brave.executablePath);
  assert.ok(brave.args.includes('--disable-renderer-backgrounding') && brave.args.includes('--mute-audio'));
  assert.ok(!brave.args.includes('--disable-gpu-vsync'));   // only headless Chromium draws unthrottled
});
