// ABOUTME: The status line a game log line calls for: a disc read that failed for good, or a match the server refused because a
// ABOUTME: player's setup (creations or settings) differs. A classic script, like boot.js, which asks it for every log line.
'use strict';

// the server compares one hash of the match settings and the mods (creations included), so it can't say which part differs. The
// refused player hears it directly; the players already in hear that the match did not start and who could not join. Null: no change
window.wotmLogStatus = function (text) {
  if (/\(ISO\).* FAILED/.test(text))   // the game waits for that read (data_retry.js already tried again)
    return 'Lost the connection to the game server. Check your connection and reload the page.';
  if (/rejected by the server: game configuration differs/.test(text))
    return "Your game setup differs from the host's (creations or settings). Make sure you both have the same " +
      'creations imported (Your creations, on the front panel), then join again.';
  const other = /rejected by the server: the match did not start: (\S+) could not join \(game configuration differs/.exec(text);
  if (other)
    return `${other[1]}'s game setup differs from yours (creations or settings), so the match did not start. Make sure everyone has ` +
      'the same creations imported (Your creations, on the front panel), then start again.';
  return null;
};
