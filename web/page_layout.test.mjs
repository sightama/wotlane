// ABOUTME: Tests for the structure of the front panel in web/index.html and web/front.js: a main landmark, one primary action, the online
// ABOUTME: choices grouped under their own heading, the lobby's code, members, rules and Start/Leave laid out as a unit. Run: node --test web/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (name) => readFileSync(new URL(`./${name}`, import.meta.url), 'utf8');
const html = read('index.html');
const css = html.slice(html.indexOf('<style>'), html.indexOf('</style>'));
const body = html.slice(html.indexOf('<body>'));
const at = (id) => {
  const i = body.indexOf(`id="${id}"`);
  assert.ok(i >= 0, `no element #${id}`);
  return i;
};
const section = (open, close) => body.slice(body.indexOf(open), body.indexOf(close, body.indexOf(open)));

test('the front panel is the page\'s main landmark', () => {
  assert.match(body, /<main id="front">/);
  assert.doesNotMatch(body, /<div id="front">/);
  assert.equal((body.match(/<main\b/g) ?? []).length, 1);
});

test('Single player is the one primary button of the home choices', () => {
  const choose = section('<div id="choose"', '<div id="inLobby"');
  assert.deepEqual([...choose.matchAll(/<button id="(\w+)"[^>]*class="[^"]*\bmain\b/g)].map((m) => m[1]), ['single']);
});

test('the online choices sit together under a Play online heading', () => {
  const online = section('<section class="online"', '</section>');
  assert.match(online, /<section class="online" aria-labelledby="onlineHeading">\s*<h2 id="onlineHeading">Play online<\/h2>/);
  for (const id of ['host', 'joinCode', 'join', 'lobbyList', 'refreshLobbies', 'lobbies']) assert.ok(online.includes(`id="${id}"`), `#${id} is in the group`);
  assert.ok(!online.includes('id="single"'));
});

test('the lobby shows its code with a visible label next to Copy invite link', () => {
  const bar = section('<div class="codebar"', '<input id="inviteLink"');
  assert.match(bar, /id="codeLabel"[^>]*>Lobby code</);
  assert.ok(bar.includes('id="code"') && bar.includes('id="copyInvite"'));
});

test('Start and Leave share one actions row; Start is not inside the owner-only settings', () => {
  const actions = section('<div class="actions"', '<div id="chat"');
  assert.ok(actions.includes('id="start"') && actions.includes('id="leave"'));
  assert.ok(!section('<div id="ownerOnly"', '<div class="actions"').includes('id="start"'));
  assert.match(read('front.js'), /\$\('start'\)\.hidden = !isOwner;/);
});

test('the rules come before the actions and the chat after them', () => {
  assert.ok(at('rules') < at('start') && at('start') < at('chat'));
  assert.ok(at('members') < at('lobbyStage'));
});

test('stage and wins are one row of fields, so their width does not depend on their options', () => {
  assert.match(section('<div id="ownerOnly"', '<fieldset id="rules"'), /<div class="fields">\s*<label>Stage <select id="lobbyStage">/);
  assert.match(css, /\.fields\s*\{[^}]*display:\s*grid/);
});

test('native controls draw in the dark scheme and checkboxes take the page\'s fill colour', () => {
  assert.match(css, /:root\s*\{[^}]*color-scheme:\s*dark/);
  assert.match(css, /#rules input\s*\{[^}]*accent-color:\s*var\(--accent-fill\)/);
});

test('the lobby code field takes the free width of its row', () => {
  assert.match(css, /\.joinrow input\s*\{[^}]*flex:\s*1/);
});

test('the status bar is not drawn under the front panel, where its buttons cannot be used', () => {
  assert.match(css, /#stage\[inert\] #bar\s*\{[^}]*visibility:\s*hidden/);
});

test('a browser that cannot run the game sees the reason as a note, with nothing to press', () => {
  assert.match(html, /<div id="profile" class="fields">/);
  assert.match(css, /#frontMsg\.problem\s*\{[^}]*border:/);
  const init = read('front.js').match(/if \(problem\) \{([^}]*)\}/)[1];
  for (const part of [`say(problem)`, `$('frontMsg').className = 'problem'`, `$('profile').hidden = true`, `$('choose').hidden = true`, `$('openSettings').hidden = true`])
    assert.ok(init.includes(part), `init does not ${part}`);
});

test('the home panel\'s class names are scoped, so the Settings tab that builds .online and .hint elements keeps its look', () => {
  assert.match(css, /\n  #choose \.online \{/);
  assert.match(css, /\n  \.online > \.hint \{/);
  assert.doesNotMatch(css, /\n  \.online \{/);
  assert.doesNotMatch(css, /\n  \.hint \{/);
});

test('the status line sits between Start and Leave and the chat, where a press shows its answer', () => {
  const lobby = section('<div id="inLobby"', '<p id="frontMsg"');
  assert.ok(!lobby.includes('id="chat"'), 'the chat is inside the lobby block, above the status line');
  assert.ok(at('start') < at('frontMsg') && at('frontMsg') < at('chat'));
  const front = read('front.js');
  assert.match(front, /\$\('inLobby'\)\.hidden = false;\n  \$\('chat'\)\.hidden = false;/);
  assert.match(front, /\$\('inLobby'\)\.hidden = true;\n  \$\('chat'\)\.hidden = true;/);
  assert.match(html, /<div id="chat" class="stack" hidden>/);
});

test('the status line and the lobbies list keep one line of room, two on a narrow screen', () => {
  assert.match(css, /#frontMsg \{[^}]*min-height:\s*1\.4em/);
  assert.match(css, /#lobbies \{[^}]*min-height:\s*1\.4em/);
  assert.match(css, /@media \(max-width: 480px\)\s*\{\s*#frontMsg, #lobbies \{ min-height: 2\.8em; \}\s*\}/);
});

test('the roomy button look belongs to the front panel; the game bar, the stall prompt and the touch panel keep the compact buttons', () => {
  assert.match(css, /\n  button \{[^}]*border-radius: 4px; padding: 2px 8px;/);
  assert.match(css, /\n  button\.main \{[^}]*padding: 8px 16px;/);
  assert.match(css, /\n  #front button \{[^}]*border-radius: 8px; padding: 6px 14px;/);
  assert.match(css, /\n  #front button\.main \{[^}]*padding: 10px 18px; font-size: 16px;/);
  assert.match(css, /\n  #controlsTable \.key \{[^}]*padding: 2px 8px;/);
});

test('the lobby keeps two lines for its status line from the moment it opens, so a longer notice moves nothing', () => {
  assert.match(css, /#inLobby:not\(\[hidden\]\) ~ #frontMsg \{ min-height: 2\.8em; \}/);
});
