// ABOUTME: Tests for the public page's head and footer in web/index.html: the not-affiliated disclaimer, the rights-holder contact,
// ABOUTME: the description, social preview tags, JSON-LD and manifest link. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const html = readFileSync(new URL('./index.html', import.meta.url), 'utf8');
const squash = (s) => s.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ');

test('the footer says the site is not affiliated with the rights holders', () => {
  const text = squash(html);
  assert.match(text, /Not affiliated with or endorsed by Sony Interactive Entertainment or Incognito Entertainment\./);
  assert.match(text, /War of the Monsters is a trademark of its respective owners\./);
  assert.match(text, /This site hosts a fan-made browser port for preservation and play\./);
});

test('the footer gives rights holders a contact address', () => {
  assert.match(squash(html), /Rights holders: legal@playwotm\.com/);
  assert.match(html, /href="mailto:legal@playwotm\.com"/);
});

test('the footer sits inside the front panel, so it shows before the game starts', () => {
  const front = html.slice(html.indexOf('<main id="front">'), html.indexOf('<pre id="log">'));
  assert.match(front, /<footer id="legal">/);
});

test('the head has a description, social tags and a manifest', () => {
  assert.match(html, /<meta name="description" content="[^"]{60,200}">/);
  assert.match(html, /<meta property="og:title" content="War of the Monsters">/);
  assert.match(html, /<meta property="og:image" content="https:\/\/playwotm\.com\/og\.png">/);
  assert.match(html, /<meta name="twitter:card" content="summary_large_image">/);
  assert.match(html, /<meta name="twitter:image" content="https:\/\/playwotm\.com\/og\.png">/);
  assert.match(html, /<link rel="canonical" href="https:\/\/playwotm\.com\/">/);
  assert.match(html, /<link rel="manifest" href="\/manifest\.webmanifest">/);
  assert.match(html, /<link rel="apple-touch-icon" href="\/apple-touch-icon\.png">/);
});

test('the JSON-LD describes a free VideoGame and parses', () => {
  const m = /<script type="application\/ld\+json">([\s\S]*?)<\/script>/.exec(html);
  assert.ok(m, 'a JSON-LD block');
  const ld = JSON.parse(m[1]);
  assert.equal(ld['@type'], 'VideoGame');
  assert.equal(ld.name, 'War of the Monsters');
  assert.equal(ld.url, 'https://playwotm.com/');
  assert.equal(ld.gamePlatform, 'Web browser');
  assert.equal(ld.offers.price, '0');
});
