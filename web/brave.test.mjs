// ABOUTME: Tests for web/brave.js: whether this browser is Brave (navigator.brave.isBrave()), and the notice a Brave player is shown.
// ABOUTME: Brave's Shields (and its masked graphics) slowed online matches to a crawl; the notice says what to do. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { detectBrave, BRAVE_NOTICE } from './brave.js';

test('detectBrave: true only when navigator.brave.isBrave() resolves true', async () => {
  assert.equal(await detectBrave({ brave: { isBrave: async () => true } }), true);
  assert.equal(await detectBrave({ brave: { isBrave: async () => false } }), false);
  assert.equal(await detectBrave({}), false);          // Chrome, Edge, Firefox, Safari have no navigator.brave
  assert.equal(await detectBrave(undefined), false);
});

test('detectBrave: a broken or throwing isBrave is not Brave, and never throws', async () => {
  assert.equal(await detectBrave({ brave: { isBrave: () => { throw new Error('denied'); } } }), false);
  assert.equal(await detectBrave({ brave: { isBrave: async () => { throw new Error('denied'); } } }), false);
  assert.equal(await detectBrave({ brave: {} }), false);
  assert.equal(await detectBrave({ brave: { isBrave: async () => 'yes' } }), false);   // only a real true counts
});

test('the notice names Shields, the browsers that work and what to turn off', () => {
  assert.equal(BRAVE_NOTICE, "Brave's Shields can slow this game a lot. For the best experience use Chrome, Edge or Firefox, or turn Shields off for playwotm.com and enable graphics acceleration.");
});
