# Playing online (wotlane)

Two to four friends, each on their own PC and screen, play one War of the Monsters fight over the internet. Each PC runs the whole game
in deterministic mode and only the controller inputs travel (lockstep through a small server, `wotm_server`). Each friend owns the disc
and builds the game from their own ISO: **never share `wotm.exe`, `recomp/`, `disc/` or the ISO**. The server program has no game code
and can be shared freely.

## 1. Everyone: build the same game

1. Install what `tools\setup_runtime.ps1` needs: git, Python 3, CMake 3.24+, and Visual Studio 2022 Build Tools with the C++ workload
   **plus the component "MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.44-17.14)"** (Visual Studio Installer, Modify, Individual
   components). Online play requires everyone to compile with the same compiler version, so the setup insists on 14.44.
2. Clone the fork on the same commit as your friends:
   `git clone -b wotlane/4p https://github.com/sightama/wotlane.git` (agree on the commit: `git log -1 --format=%h`).
3. Put your own ISO in `ISO\` and run `powershell -ExecutionPolicy Bypass -File tools\setup_runtime.ps1` (about 10 minutes the first
   time). Later updates: `git pull`, then `build_runtime.bat`.
4. Compare build ids in your chat: `python tools\build_id.py`. Everyone must print the same `wl-...` id. The server refuses players with
   a different build (different commit, compiler or flags), because the simulation would drift apart.

## 2. The host: run the server

1. Build it once: `tools\build_net.bat` (it also runs the network tests). The program is `native\net\build\wotm_server.exe`.
2. Start it: in the launcher, section **Online**, press **Host server here**, or run `native\net\build\wotm_server.exe --port 7777`.
   It keeps **2 lobbies** open (`--lobbies N` for more, `--players 2..4` per lobby) and reopens a lobby when its match ends. Leave its
   window open while you play; close it to stop.
3. The first time, Windows Firewall asks about `wotm_server.exe`: allow it on private networks (and public if your network is marked
   public).
4. Let your friends reach it, one of:
   - **Port forwarding (no extra software):** find your PC's local address with `ipconfig` ("IPv4 Address", for example 192.168.1.50).
     In your router's admin page (usually http://192.168.1.1 or http://192.168.0.1), add a port forwarding rule: external port **7777**,
     internal IP **your PC's IPv4 address**, internal port **7777**, protocol **TCP and UDP** (or "Both"). Friends then connect to your
     public IP (search "what is my ip"). If your provider uses CGNAT (your router's WAN address differs from the public IP), forwarding
     will not work: use the next option.
   - **Free virtual LAN (Tailscale or ZeroTier):** everyone installs the same one and joins your network; friends use your virtual
     address (for example 100.x.y.z with Tailscale). No router changes.
   - **Dedicated server:** any always-on PC can run `wotm_server` the same way (the source builds with CMake on Linux too). Friends use
     that machine's address.

## 3. Everyone: join and play

1. Open the launcher (`tools\launcher\build.bat` builds `WotM_Launcher.exe` in the repo root).
2. In **Match**, pick **Your monster** and the **Stage** (the stage only matters for whoever starts the match).
3. In **Online**: Server = `<host address>:7777`, type your name, pick a lobby, press **Join**. The line under the buttons lists who is
   in the lobby. **Refresh** shows both lobbies.
4. When everyone is in, anyone presses **Start match**. The game opens on every PC, loads the stage and waits until everyone has
   arrived; the server measures the ping and picks the input delay.
5. In the game: your controller (or keyboard) drives your monster; the screen shows only your view. Top right: ping and input delay in
   frames. A "Waiting for <name>" message means someone's input is late (everyone pauses together, nobody desyncs).

Command line alternative to the launcher: `python tools\net_lobby.py --server <host>:7777 --name Ana --monster 3` prints the environment
for `wotm.exe` once someone starts the match (`--start <stage>` starts it); `--list` shows the lobbies.

## What is different online

- Endless free-for-all: knocked-out monsters get back up, no end-of-match screen. Close the window to leave.
- Everyone plays with the game's default options (difficulty, volumes, controller layout): local saves are neither read nor written
  online, because they live in game memory and would make the PCs drift apart.
- Debug keys that change the game or pause it are off: F1 (cheats), F5 (free camera), F6/F7/F8 (pause, step, photo).
- Input delay: 2 frames on a LAN, about 4 to 6 over the internet (each frame is 16.7 ms; 33 ms with 3 or 4 players, where the game runs
  its own 30 Hz mode). It is fixed for the whole match.
- If a player leaves, their monster stands still for the rest of the match (same frame on every PC).
- If the server sees two PCs with different game state ("desync"), the match stops on every PC with a message. Please report it with
  everyone's `runtime\live.err.log`.

## Troubleshooting

- "build diferente": someone is on another commit or compiler. Compare `python tools\build_id.py`; `git pull` and rebuild.
- "configuracao do jogo diferente": someone launched with different game options (mods such as the lock-on toggle or enemy bars, cheats,
  `WOTM_RATEMIN`). Use the same launcher checkboxes in the Mods group.
- "sem resposta do servidor": the server is not running, the port is not forwarded (TCP and UDP), or a firewall blocks it.
- Test everything on one PC: `powershell -File tools\net_check.ps1` starts a server and two games and checks that both stay identical.
