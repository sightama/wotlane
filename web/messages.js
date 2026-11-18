// ABOUTME: Turns the game server's ERROR text into the sentence the front panel shows; a text it doesn't know is shown as it is.
// ABOUTME: Every ERROR the lobby sends (native/net/wotm_net_server.cpp, search sendLine("ERROR) has its line here.
'use strict';

// [pattern of the server's text, the sentence]; a function gets the match. The first pattern that matches wins
const MESSAGES = [
  [/^different protocol/, 'This page is out of date for the game server. Reload the page and try again.'],
  [/^(send HELLO first|join a lobby first|unknown command)/, 'Something went wrong talking to the game server. Reload the page and try again.'],
  [/^only the lobby owner can start/, 'Only the person who made the lobby can start the match.'],
  [/^the server already has (\d+) lobbies open/, (m) => `All ${m[1]} lobbies are busy. Try again soon.`],
  [/^empty name/, 'Type a name first (letters, digits, - and _).'],
  [/^no such lobby/, 'No lobby has that code. Check the code and try again.'],
  [/^you are already in this lobby/, 'You are already in this lobby.'],
  [/^wrong password/, 'That lobby needs a password, and this page can\'t send one.'],
  [/^lobby is in a match; wait/, 'That lobby is in a match. Wait for it to end, then join.'],
  [/^lobby is full/, 'That lobby is full.'],
  [/^build differs from the lobby's/, 'That lobby runs a different version of the game. Reload the page. If that doesn\'t help, ask the host to reload too.'],
  [/^name already taken/, 'Someone in that lobby already has that name. Pick another.'],
  [/^only the lobby owner can change/, 'Only the person who made the lobby can change the stage, wins and modifiers.'],
  [/^unknown modifier/, 'That modifier isn\'t available.'],
  [/^bad stage/, 'That stage isn\'t available.'],
  [/^lobby is in a match; change monster/, 'You can pick another monster once the match is over.'],
  [/^bad monster|^invalid monster/, 'That monster isn\'t available.'],
  [/^that skin is already taken/, 'Someone in the lobby already has that skin. Pick another skin or costume.'],
  [/^chat is only open before the match/, 'Chat closes when the match starts.'],
  [/^chat text is not valid text/, 'That message could not be sent.'],
  [/^chat too fast/, 'You are chatting too fast. Wait a few seconds.'],
  [/^only the lobby owner can remove/, 'Only the owner of the lobby can remove someone.'],
  [/^only the lobby owner can hand/, 'Only the owner of the lobby can hand it to someone else.'],
  [/^you can't remove yourself/, 'To go, press Leave.'],
  [/^you are the owner already/, 'You are the owner already.'],
  [/^the match has started/, 'The match has started. That works again once it ends.'],
  [/^you were removed from this lobby; try again in (\d+) s/, (m) => `The owner removed you, or someone on your connection, from that lobby. You can join it again in ${m[1]} seconds.`],
  [/^HEALTH /, 'Something went wrong talking to the game server. Reload the page and try again.'],
  [/ is not in this lobby$/, 'That player is not in the lobby any more.'],
  [/^monster has no free costume/, 'Every costume of that monster is taken in this lobby. Pick another monster.'],
];

function friendlyError(text) {
  for (const [pattern, sentence] of MESSAGES) {
    const m = pattern.exec(text);
    if (m) return typeof sentence === 'function' ? sentence(m) : sentence;
  }
  return text;
}

if (typeof module !== 'undefined' && module.exports) module.exports = { friendlyError };
else globalThis.friendlyError = friendlyError;
