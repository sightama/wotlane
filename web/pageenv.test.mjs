// ABOUTME: Tests for web/pageenv.js: the page address can set only a short allowlist of switches, the fixed values win over the address
// ABOUTME: and over the match environment, and WOTM_BUILDID comes only from the build id. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { pageEnv, FROM_ADDRESS } = createRequire(import.meta.url)('./pageenv.js');
const fixed = { PS2X_CD_IMAGE: '/disc/game.iso', WOTM_SAVEDIR: '/save', PS2X_SKIP_MOVIES: '1' };
const params = (q) => new URLSearchParams(q);

test('the allowed test switches pass from the address', () => {
  const env = pageEnv(params('WOTM_MATCH=5,1,9&WOTM_AIVSAI=1&PS2X_STATS=1&WOTM_MAPEDIT=1'), fixed, null, null);
  assert.equal(env.WOTM_MATCH, '5,1,9');
  assert.equal(env.WOTM_AIVSAI, '1');
  assert.equal(env.PS2X_STATS, '1');
  assert.equal(env.WOTM_MAPEDIT, '1');
});

test('any other WOTM_ or PS2X_ key in the address is ignored', () => {
  const env = pageEnv(params('WOTM_NETSERVER=evil:1&PS2X_ELF=/x&WOTM_CHEATS=1&WOTM_PADLOG=1&WOTM_BUILDID=wl-x&other=1'), fixed, null, null);
  assert.deepEqual(env, fixed);
});

test('the fixed values win over the address', () => {
  const env = pageEnv(params('PS2X_CD_IMAGE=/etc/passwd&WOTM_SAVEDIR=/x'), fixed, null, null);
  assert.equal(env.PS2X_CD_IMAGE, '/disc/game.iso');
  assert.equal(env.WOTM_SAVEDIR, '/save');
});

test('the fixed values win over the match environment, which adds its own keys', () => {
  const env = pageEnv(params(''), fixed, { WOTM_SAVEDIR: '/elsewhere', WOTM_NETTOKEN: 't' }, null);
  assert.equal(env.WOTM_SAVEDIR, '/save');
  assert.equal(env.WOTM_NETTOKEN, 't');
});

test('the match environment wins over the address for the keys they share', () => {
  const env = pageEnv(params('WOTM_MATCH=9,9,9'), fixed, { WOTM_MATCH: '1,2,3' }, null);
  assert.equal(env.WOTM_MATCH, '1,2,3');
});

test('WOTM_BUILDID comes only from the build id', () => {
  assert.equal(pageEnv(params('WOTM_BUILDID=wl-address'), fixed, null, 'wl-build').WOTM_BUILDID, 'wl-build');
  assert.equal(pageEnv(params(''), fixed, { WOTM_BUILDID: 'wl-match' }, 'wl-build').WOTM_BUILDID, 'wl-build');
  assert.equal('WOTM_BUILDID' in pageEnv(params('WOTM_BUILDID=wl-address'), fixed, { WOTM_BUILDID: 'wl-match' }, null), false);
});

test('the allowlist is the four test switches', () => {
  assert.deepEqual([...FROM_ADDRESS].sort(), ['PS2X_STATS', 'WOTM_AIVSAI', 'WOTM_MAPEDIT', 'WOTM_MATCH']);
});
