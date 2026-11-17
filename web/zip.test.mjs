// ABOUTME: Tests for web/zip.js: zips written here read back, zips from a common zip tool (stored and deflated entries, folder
// ABOUTME: entries) read, and damaged or oversized archives are refused. Run: node --test web/*.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { crc32, zip, unzip } from './zip.js';

const text = (s) => new TextEncoder().encode(s);
const str = (b) => new TextDecoder().decode(b);

// made with Info-ZIP's `zip -r f.zip Creator`: three folder entries, skin.json and t0004.png (3000 'A's) deflated
const TOOL_ZIP = 'UEsDBAoAAAAAAG2Xbl0AAAAAAAAAAAAAAAAIABwAQ3JlYXRvci9VVAkAA971+Gre9fhqdXgLAAEE9QEAAAQAAAAAUEsDBAoAAAAAAG2Xbl0AAAAAAAAAAAAAAAAOABwAQ3JlYXRvci9za2lucy9VVAkAA971+Gre9fhqdXgLAAEE9QEAAAQAAAAAUEsDBAoAAAAAAG2Xbl0AAAAAAAAAAAAAAAAUABwAQ3JlYXRvci9za2lucy9ncmVlbi9VVAkAA971+Gre9fhqdXgLAAEE9QEAAAQAAAAAUEsDBBQAAAAIAG2Xbl2uQI4aJAAAACUAAAAdABwAQ3JlYXRvci9za2lucy9ncmVlbi9za2luLmpzb25VVAkAA971+Gre9fhqdXgLAAEE9QEAAAQAAAAAq1bKzc8rLkktUrJSUCrJT08tSlTSUVDKTAHx04tSU/OUarkAUEsDBBQAAAAIAG2Xbl1p9qqTFQAAALgLAAAdABwAQ3JlYXRvci9za2lucy9ncmVlbi90MDAwNC5wbmdVVAkAA971+Gre9fhqdXgLAAEE9QEAAAQAAAAA7cExAQAAAMKgbOtfyhoeQAEAAO8GUEsBAh4DCgAAAAAAbZduXQAAAAAAAAAAAAAAAAgAGAAAAAAAAAAQAO1BAAAAAENyZWF0b3IvVVQFAAPe9fhqdXgLAAEE9QEAAAQAAAAAUEsBAh4DCgAAAAAAbZduXQAAAAAAAAAAAAAAAA4AGAAAAAAAAAAQAO1BQgAAAENyZWF0b3Ivc2tpbnMvVVQFAAPe9fhqdXgLAAEE9QEAAAQAAAAAUEsBAh4DCgAAAAAAbZduXQAAAAAAAAAAAAAAABQAGAAAAAAAAAAQAO1BigAAAENyZWF0b3Ivc2tpbnMvZ3JlZW4vVVQFAAPe9fhqdXgLAAEE9QEAAAQAAAAAUEsBAh4DFAAAAAgAbZduXa5AjhokAAAAJQAAAB0AGAAAAAAAAQAAAKSB2AAAAENyZWF0b3Ivc2tpbnMvZ3JlZW4vc2tpbi5qc29uVVQFAAPe9fhqdXgLAAEE9QEAAAQAAAAAUEsBAh4DFAAAAAgAbZduXWn2qpMVAAAAuAsAAB0AGAAAAAAAAQAAAKSBUwEAAENyZWF0b3Ivc2tpbnMvZ3JlZW4vdDAwMDQucG5nVVQFAAPe9fhqdXgLAAEE9QEAAAQAAAAAUEsFBgAAAAAFAAUAwgEAAL8BAAAAAA==';

test('crc32 matches the standard check value', () => {
  assert.equal(crc32(text('123456789')), 0xcbf43926);
  assert.equal(crc32(new Uint8Array(0)), 0);
});

test('a zip written here reads back with the same names and bytes', async () => {
  const big = new Uint8Array(5000).fill(65);   // compresses: stored deflated
  const noise = Uint8Array.from({ length: 300 }, (_, i) => (i * 7919 + 13) & 0xff);
  const bytes = await zip([
    { name: 'Creator/skins/green/skin.json', data: text('{"monster": "togera"}\n') },
    { name: 'Creator/skins/green/t0004.png', data: big },
    { name: 'Creator/skins/green/t0005.png', data: noise },
  ]);
  const back = await unzip(bytes);
  assert.deepEqual(back.map((e) => e.name),
    ['Creator/skins/green/skin.json', 'Creator/skins/green/t0004.png', 'Creator/skins/green/t0005.png']);
  assert.equal(str(back[0].data), '{"monster": "togera"}\n');
  assert.deepEqual(back[1].data, big);
  assert.deepEqual(back[2].data, noise);
  assert.ok(bytes.length < 5000, 'the repetitive file is deflated');
});

test('a zip from a common zip tool reads, folder entries left out', async () => {
  const back = await unzip(Buffer.from(TOOL_ZIP, 'base64'));
  assert.deepEqual(back.map((e) => e.name), ['Creator/skins/green/skin.json', 'Creator/skins/green/t0004.png']);
  assert.equal(str(back[0].data), '{"monster": "togera", "id": "green"}\n');
  assert.deepEqual(back[1].data, new Uint8Array(3000).fill(65));
});

test('damaged archives are refused', async () => {
  const good = await zip([{ name: 'a/b.json', data: text('{}') }]);
  await assert.rejects(unzip(good.slice(0, good.length - 10)), /not a zip/);   // end record cut off
  await assert.rejects(unzip(text('hello')), /not a zip/);
  const bad = good.slice();
  bad[30 + 'a/b.json'.length] ^= 0xff;   // first data byte (stored: the file itself)
  await assert.rejects(unzip(bad), /damaged/);
});

test('archives past the limits are refused before anything is inflated', async () => {
  const two = await zip([{ name: 'a', data: text('1') }, { name: 'b', data: text('2') }]);
  await assert.rejects(unzip(two, { maxEntries: 1 }), /too many files/);
  const big = await zip([{ name: 'big', data: new Uint8Array(2000) }]);
  await assert.rejects(unzip(big, { maxBytes: 1000 }), /too large/);
});

test('an entry that unpacks to more than its directory claims is refused', async () => {
  const bytes = await zip([{ name: 'bomb', data: new Uint8Array(100000) }]);   // deflates to a few hundred bytes
  const view = new DataView(bytes.buffer);
  const dir = bytes.length - 22 - 46 - 'bomb'.length;
  view.setUint32(dir + 24, 10, true);   // the directory claims 10 bytes
  await assert.rejects(unzip(bytes), /damaged/);
});
