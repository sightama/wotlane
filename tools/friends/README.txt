War of the Monsters on PC: play online with friends
===================================================

This is the PC version of War of the Monsters (PS2) from the wotlane project. It does not contain the game itself: you need your own
copy of the game as an ISO of the USA disc (SCUS-97197). Please keep this package private; it is for friends who own the game.

Needs: Windows 10 or 11 (64-bit), a PC from roughly 2015 or newer, a controller or the keyboard.


Install (once, about a minute)
------------------------------
1. Unzip this folder somewhere you can write to, for example your Desktop or Documents (not Program Files).
2. Put your War of the Monsters (USA) .iso in the ISO folder. (Or skip this: Setup asks you to pick it.)
3. Double-click Setup.bat.
   It checks your ISO, adds "War of the Monsters online" to your desktop and opens the game's launcher.
   Don't move or rename the ISO afterwards (if you do, double-click Setup.bat again).
4. If Windows says the launcher is from an unknown publisher: press "More info", then "Run anyway".


Join a friend's game
--------------------
1. Open "War of the Monsters online" on your desktop.
2. Type your name and pick your monster.
3. Press "Join a game", paste the invite your friend sent you (it looks like 99.38.90.23:7777/ABCD) and press Join.
4. Wait for your friend to press Start match: the game opens by itself. When the match ends, you're back in the lobby.


Host a game
-----------
1. Open "War of the Monsters online", type your name, pick your monster.
2. Press "Host a game". If Windows asks whether wotm_server may use the network, press Allow.
3. Press "Copy invite" and send it to your friends. Keep the launcher open while you play.
4. Pick the stage, the wins to end the match (0 = endless) and any modifiers. Press "Start match" when everyone is in.
If friends can't reach you: your router has to let port 7777 through (TCP and UDP). The launcher asks the router to open it by
itself; if that doesn't work, forward port 7777 to your PC in the router's settings, or everyone installs Tailscale
(tailscale.com, free) and the host sends the invite from the Tailscale address.


Play alone
----------
Press "Play offline".


If something goes wrong
-----------------------
The launcher says what's wrong in plain words at the bottom. The usual ones:
- "Can't reach the host": check the invite; the host's launcher must be open and hosting.
- "Your game version doesn't match the host's": everyone needs the same package (compare BUILD.txt).
- "Your game files or mods differ from the host's": use the package as it came (don't change the mods folder).
Anything else: send the host runtime\live.err.log (the host also has wotm_server.log next to the launcher).
More options (passwords, public server list, AI opponents): the "Advanced" button in the launcher.
