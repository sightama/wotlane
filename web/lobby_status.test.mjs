// ABOUTME: Tests for web/lobby_status.js: the colour level of a member's ping and device health, the words beside them and the
// ABOUTME: warnings the lobby shows before a match (a high ping or a device too slow to hold the match's rate).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { pingLevel, deviceLevel, memberStatus, lobbyWarnings } from './lobby_status.js';

test('pingLevel: green under 80 ms, yellow under 150, red from 150, none without a measurement', () => {
  assert.equal(pingLevel(0), 'good');
  assert.equal(pingLevel(79), 'good');
  assert.equal(pingLevel(80), 'warn');
  assert.equal(pingLevel(149), 'warn');
  assert.equal(pingLevel(150), 'bad');
  assert.equal(pingLevel(288), 'bad');
  assert.equal(pingLevel(null), 'none');
  assert.equal(pingLevel(undefined), 'none');
});

test('deviceLevel: a device that holds its match rate is green, one at 60% of it red (a browser drawing in software)', () => {
  assert.equal(deviceLevel({ fps: 60, workMs: 6, hz: 60 }), 'good');
  assert.equal(deviceLevel({ fps: 52, workMs: 8, hz: 60 }), 'warn');
  assert.equal(deviceLevel({ fps: 27, workMs: 13, hz: 60 }), 'bad');
  assert.equal(deviceLevel(undefined), 'none');
});

test('deviceLevel: a phone in the 30 Hz mode is judged against 30, not 60', () => {
  assert.equal(deviceLevel({ fps: 30, workMs: 14, hz: 30 }), 'good');
  assert.equal(deviceLevel({ fps: 20, workMs: 25, hz: 30 }), 'bad');
});

test('deviceLevel: heavy work per frame is a warning even when the frame rate held, and red when it fills the whole frame', () => {
  assert.equal(deviceLevel({ fps: 60, workMs: 13, hz: 60 }), 'warn');
  assert.equal(deviceLevel({ fps: 60, workMs: 16.5, hz: 60 }), 'bad');
});

test('memberStatus words the ping and the device for one member', () => {
  const health = { Bia: { fps: 27.4, workMs: 13.2, hz: 60 } };
  assert.deepEqual(memberStatus('Bia', { Bia: 42 }, health), {
    ping: { level: 'good', text: '42 ms' },
    device: { level: 'bad', text: 'device too slow: 27 fps of 60' },
  });
  assert.deepEqual(memberStatus('Ana', { Ana: null }, health), {
    ping: { level: 'none', text: '' },
    device: { level: 'none', text: 'device not measured yet' },
  });
  assert.equal(memberStatus('Cy', { Cy: 100 }, { Cy: { fps: 53, workMs: 9, hz: 60 } }).device.text, 'device borderline: 53 fps of 60');
  assert.equal(memberStatus('Cy', {}, { Cy: { fps: 60, workMs: 5, hz: 60 } }).device.text, 'device ok');
});

test('lobbyWarnings names whoever would hurt the match: a ping from 150 ms, a device far under the match rate', () => {
  const warnings = lobbyWarnings(['Ana', 'Bia', 'Cy'], { Ana: 30, Bia: 288, Cy: 100 }, { Cy: { fps: 22, workMs: 14, hz: 60 }, Ana: { fps: 60, workMs: 5, hz: 60 } });
  assert.deepEqual(warnings, [
    'Bia: ping 288 ms is high, so everyone\'s input will feel laggy.',
    'Cy: this device runs 22 fps of 60 and would slow the match for everyone.',
  ]);
  assert.deepEqual(lobbyWarnings(['Ana'], { Ana: 12 }, {}), []);
});

test('a member named like an Object property has no ping and no health of their own', () => {
  for (const name of ['constructor', 'toString', 'hasOwnProperty']) {
    assert.equal(memberStatus(name, {}, {}).device.level, 'none');
    assert.deepEqual(lobbyWarnings([name], {}, {}), []);
  }
});

test('a member drawing in software is red with the reason, measured or not, and named in the warnings', () => {
  const health = { Bia: { softwareGl: true }, Cy: { fps: 60, workMs: 5, hz: 60, softwareGl: true } };
  assert.equal(deviceLevel(health.Bia), 'bad');
  assert.deepEqual(memberStatus('Bia', {}, health).device, { level: 'bad', text: 'software graphics (hardware acceleration off)' });
  assert.deepEqual(memberStatus('Cy', {}, health).device, { level: 'bad', text: 'software graphics (hardware acceleration off)' });
  assert.deepEqual(lobbyWarnings(['Bia'], {}, health), [
    'Bia: this browser draws without the graphics card (hardware acceleration off) and would slow the match for everyone.',
  ]);
});
