# ABOUTME: Live game checks (up-to-4 multiplayer, screen, regressions): each runs the game with tools\live_check.ps1 and checks
# ABOUTME: the log. Usage: powershell -File tools\live_checks.ps1 [-Only <name>]; exits 1 if any fail.
# Entry keys: Env, Secs, Expect, Reject (as in live_check.ps1) and, optional: Clean (a folder removed before that entry runs),
# Setup (a scriptblock run after Clean, before the game; a failure fails the entry) and After (a scriptblock run once the log
# passed, with the runtime's discroot folder as its argument; it returns $true when its own checks pass).
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'live_check.ps1'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
$stageMap = Join-Path $PSScriptRoot 'stage_map.py'

# Map creator checks (docs/superpowers/specs/2026-11-09-map-creator-design.md): maps made by stage_map.py in a temp mods folder.
# t1 on CENTRAL: a destructible removed (0x18a410), the Apcomm tower (0x165a70, where player 1 starts, on its roof at height
# 580) moved across the map, a low building (0x2d85a0, roof at 148) moved onto that start point, a TRANSLATE-only building
# (0x4da6b0) turned 90 degrees (a ROTATE in the reserve) and a level pickup (0x5c95a0, a static truck) moved into the park.
$mapMods = "$env:TEMP\wotm_map_mods"
$makeT1 = {
    param($out)
    Clear-MapDumps
    & python $stageMap make --base CENTRAL --id t1 --name 'TEST MAP' --out $out --remove 0x18a410 `
        --move '0x165a70=-185.7,475.8,0' --move '0x2d85a0=768.1,499.6,74.1' --move '0x4da6b0=-1153.1,4.1,0.3,90' `
        --move '0x5c95a0=400,-100,11.9'
    if ($LASTEXITCODE -ne 0) { throw 'stage_map.py make failed' }
}
# no stale dumps from an earlier run: the stage dumps (WOTM_STAGE_DUMP) and the RAM of frame 2400 (PS2X_DUMP_RAM)
function Clear-MapDumps {
    Remove-Item "$env:TEMP\wotm_map_*.bin*" -ErrorAction SilentlyContinue
    if ($runtime) { Remove-Item (Join-Path $runtime 'discroot\ram_2400.bin'), (Join-Path $runtime 'discroot\vram_2400.bin') -ErrorAction SilentlyContinue }
}
# the RAM of the runtime's frame 2400 (PS2X_DUMP_RAM's count, about 30 s in: the fight has started, the start-point drop is over),
# read by stage_map.py ram
function Test-MapRam([string]$disc, [string[]]$expect) {
    $ram = Join-Path $disc 'ram_2400.bin'
    if (-not (Test-Path $ram)) { Write-Host "MISSING $ram (PS2X_DUMP_RAM)" -ForegroundColor Red; return $false }
    & python $stageMap ram $ram @expect | ForEach-Object { Write-Host "        $_" }
    $ok = $LASTEXITCODE -eq 0
    Remove-Item (Join-Path $disc 'ram_2400.bin'), (Join-Path $disc 'vram_2400.bin') -ErrorAction SilentlyContinue
    return $ok
}

$checks = [ordered]@{
    # pads 3 and 4 (multitap slot 1 on ports 0 and 1) are opened and read by the game; 2-player mode still works
    'controles_3_4' = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1'; Secs = 25
                         Expect = '\[pad\] player 1: |||\[pad\] player 3: |||\[pad\] player 4: |||\[match\]' }
    # 4-player match (AI on the pads): 4 models loaded, TheGame::m_numMonsters (0x3D3978) 4, m_monsters[0..3]
    # (0x3D3920..0x3D392C) filled, 4 views (viewNumViews 0x6F8AE0) and 4 cameras (Cameras::m_numCameras 0x6F8E30). The monster
    # introduction before the fight uses a single view (wotm_split4.inc, cinemaViews) until game frame ~520: 60 s so the sample gets past it
    'partida_4'     = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=3D3978,3D3920,3D3924,3D3928,3D392C,6F8AE0,6F8E30'; Secs = 60
                         Expect = '\[native\] resources: CENTRAL \S+ \S+ \S+ \S+\r?\n|||3d3978=00000004|||3d3928=(?!00000000)[0-9a-f]{8}|||3d392c=(?!00000000)[0-9a-f]{8}|||6f8ae0=00000004|||6f8e30=00000004' }
    # knocked-out players come back: every 8 s of play the "vencer" cheat knocks out
    # players 2..4 (and the AIs); if they all came back, the second time knocks them all out again (3 without AI; before, only player 2 came back). With AI, m_playerNum (+0x18) of players 3/4 (monsters at 0x2D6440 and 0x2E75D0, slots 2 and 3)
    # stays 1 (human), never 2 (AI)
    'volta_4'       = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_CHEATS=vencer=8'; Secs = 50
                         Expect = 'win the fight: 3 [^\n]*\n(.|\n)*win the fight: 3 ' }
    'volta_4_ia'    = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=4;WOTM_CHEATS=vencer=8;PS2X_STATS=1;PS2X_PEEK=2D6458,2E75E8'; Secs = 50
                         Expect = 'win the fight: 4 [^\n]*\n(.|\n)*win the fight: 4 |||2d6458=00000001|||2e75e8=00000001'
                         Reject = '2d6458=00000002|||2e75e8=00000002|||\[CRASH\]|||\[watchdog\] FROZE' }
    # 4-player spawn (wotm_split4.inc, startPoint): the 4 spawn in 4 different places (StartPoints list 1 has 2 points in
    # Centro; without the fix, 3 spawned on the same point). No AI and no pad input: nobody dies, only the spawns show up
    'largada_4'     = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_SPLITLOG=1'; Secs = 30
                         Expect = 'spawn: m_monsters\[0\]|||spawn: m_monsters\[1\]|||spawn: m_monsters\[2\]|||spawn: m_monsters\[3\]'
                         Reject = 'spawn: m_monsters\[[0-3]\] \(playerNum \d\) at (\S+)\r?\n[\s\S]*?spawn: m_monsters\[[0-3]\] \(playerNum \d\) at \1\r?\n|||\[CRASH\]|||\[watchdog\] FROZE' }
    # motion blur with 4 views: MotionBlurAA::init receives all 4 (views at 0x6F8578; its bss has room for 4, NATIVO.md, "3 and 4 player split screen")
    'borrao_4'      = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=6F8578'; Secs = 35
                         Expect = '6f8578=00000004'
                         Reject = '6f8578=00000002|||\[CRASH\]|||\[watchdog\] FROZE' }
    # win target for the direct match (WOTM_KILLTARGET): the "vencer" cheat knocks out player 2 at 8 s, player 1 reaches 1 win and
    # Evaluate opens dialog 5 (winner, PLAY AGAIN / REBUILD LEVEL / QUIT). Without the target (m_killTarget 0) the loser just gets up
    'meta_vitorias' = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_KILLTARGET=1;WOTM_CHEATS=vencer=8'; Secs = 40
                         Expect = 'win the fight: |||\[dialog\] 5 opened'
                         Reject = '\[CRASH\]|||\[watchdog\] FROZE' }
    # monster with no file on the disc (6 = spider, cut): WOTM_MATCH is rejected with a warning instead of hanging the stage load
    # (native/net/wotm_monsters.hpp)
    'monstro_sem_arquivo' = @{ Env = 'WOTM_MATCH=3,6,7,1;WOTM_AIVSAI=1'; Secs = 25
                         Expect = 'WOTM_MATCH rejected: monster 6 \(list item 2\) has no file on the disc' }
    # regression: 2-player mode still draws the split screen (redraws with 2 views) and never the 4-way screen
    'tela_2'        = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1;PS2X_STATS=1'; Secs = 40
                         Expect = '2 views [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '[34] views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # 4-way screen: the native layer captures the 4 views ([split] lines for view 3) and redraws with 4 views ([native] redraw line);
    # the shot_30.png screenshot (discroot) is kept for checking the quadrants by eye
    'tela_4'        = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;WOTM_SPLITLOG=1;WOTM_SHOTS=30'; Secs = 40
                         Expect = '\[split\] g=\d+ view 3:|||4 views [\d.]+ ms \(CPU [\d.]+\) x [1-9]' }
    # local view (netcode, each on their own PC): with WOTM_LOCALPLAYER only that player's view, full screen (redraws with 1 view,
    # never the quadrants); shot_30.png screenshot with player 3's view enlarged from its quadrant, game HUD and text included
    'local_4'       = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;WOTM_LOCALPLAYER=3;PS2X_STATS=1;WOTM_SHOTS=30'; Secs = 40
                         Expect = 'redraw: 1 view [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '[34] views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # 2 players with local view: both views on viewport 0 (viewInfo[1].viewport at 0x6E191C = 0), without the 2:3 half
    'local_2'       = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1;WOTM_LOCALPLAYER=2;PS2X_STATS=1;PS2X_PEEK=6F8AE0,6E191C;WOTM_SHOTS=30'; Secs = 40
                         Expect = '6f8ae0=00000002 6e191c=00000000|||redraw: 1 view [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '2 views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # player 5 (on an AI driven by pad 5) on their own PC: native chase camera behind their monster, with the scene collected
    # without the game's 4-view culling (hundreds of objects), and one view on screen; shot_30.png screenshot to check
    'local_5'       = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=5;WOTM_LOCALPLAYER=5;PS2X_STATS=1;WOTM_SHOTS=30'; Secs = 45
                         Expect = '\[chase\] player 5: camera [2-4]\d\d units from the monster, \d{3,} objects|||redraw: 1 view [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '[34] views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # default without WOTM_FULLSCREEN/WOTM_RES/WOTM_RENDERSCALE (panel options; settings.ini ignored): a 720-line 16:9 window and a
    # frame of the same size; WOTM_RENDERSCALE=200 = frame at double size (supersampling), scaled down on present
    'tela_padrao'   = @{ Env = 'WOTM_FULLSCREEN=;WOTM_RES=;WOTM_RENDERSCALE=;WOTM_MATCH=3,1,1'; Secs = 25
                         Expect = '\[native\] frame 1280x720, window 1280x720 \(windowed' }
    'tela_escala'   = @{ Env = 'WOTM_FULLSCREEN=;WOTM_RES=;WOTM_RENDERSCALE=200;WOTM_MATCH=3,1,1'; Secs = 25
                         Expect = '\[native\] frame 2560x1440, window 1280x720 \(windowed' }
    # the unified view (everyone on a single screen in close combat, gUseUnifiedView 0x6F8A8C) never turns on with 3+ players
    'unificada_4'   = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=6F8A8C'; Secs = 70
                         Expect = '6f8a8c=00000000'
                         Reject = '6f8a8c=00000001|||\[CRASH\]|||\[watchdog\] FROZE' }
    # character creator (NATIVO.md): WOTM_CREATOR saves a recolor of Togera's costume 1 into a fresh mods folder and registers it live;
    # creator_reload boots on that folder without WOTM_CREATOR and needs creator_script first (they run in order: keep them adjacent).
    # The skin's quotes are \x22 in the regex: powershell -File drops a bare " on the way to live_check.ps1
    'creator_script' = @{ Env = "WOTM_CREATOR=togera:1:green:120;WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 25
                         Clean = "$env:TEMP\wotm_creator_mods"
                         Expect = '\[creator\] saved mods/Creator/skins/green \(\d+ png\)|||\[mods\] skin togera/green \(\x22GREEN\x22, Creator\) = costume 4'
                         Reject = '\[creator\] (cannot write|not registered|WOTM_CREATOR ignored)|||\[CRASH\]|||\[watchdog\] FROZE' }
    'creator_reload' = @{ Env = "WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 25
                         Expect = '\[mods\] skin togera/green .* = costume 4 \(LIZARD4\); base 1, \d+ png'
                         Reject = '\[mods\] skin Creator/skins/green ignored|||\[CRASH\]|||\[watchdog\] FROZE' }
    # map t1 in a CPU fight for 60 s: applied, the stage image right after the edits keeps the grid rule (stage_map.py check on
    # the WOTM_STAGE_DUMP), and the moved pickup was placed at its new spot and none at its old one (createPickup's position,
    # from the fight's RAM: dbsTraverse's TRANSLATE case gives the edited translation)
    'map_load'      = @{ Env = "WOTM_MATCH=3,1,t1;WOTM_AIVSAI=1;WOTM_MODS=$mapMods;WOTM_STAGE_DUMP=$env:TEMP\wotm_map_t1.bin;PS2X_DUMP_RAM=2400"; Secs = 60
                         Clean = $mapMods; Setup = { & $makeT1 "$mapMods\Maps\stages" }
                         Expect = '\[stages\] map t1 .*|||\[stages\] t1: applied 5 edits|||\[match\]'
                         Reject = '\[CRASH\]|||\[watchdog\] FROZE|||does not match|||refused|||nothing applied'
                         After = {
                             param($disc)
                             $out = & python $stageMap check "$env:TEMP\wotm_map_t1.bin"
                             $out | ForEach-Object { Write-Host "        $_" }
                             $grid = $LASTEXITCODE -eq 0 -and ($out -join ' ') -match ', OK'
                             if (-not $grid) { Write-Host 'FAILED  stage_map.py check on the dump' -ForegroundColor Red }
                             (Test-MapRam $disc @('--pickup', '400,-100,11.9', '--no-pickup', '1213.1,536,11.9')) -and $grid
                         } }
    # collision follows the moves: with nobody pressing anything, player 1 drops (hdHatTest) from its start point onto what is
    # under it and stands there. Stock: the tower's roof (580). t1: the low building's roof (148), so the moved building is solid
    # in its new place and the tower is not in its old one
    'map_solid'     = @{ Env = "WOTM_MATCH=3,1,t1;WOTM_PLAYERS=2;WOTM_MODS=$mapMods;PS2X_DUMP_RAM=2400"; Secs = 60
                         Clean = $mapMods; Setup = { & $makeT1 "$mapMods\Maps\stages" }
                         Expect = '\[stages\] t1: applied 5 edits|||\[match\]'
                         After = { param($disc) Test-MapRam $disc @('--monster', '0:140:156') } }
    # a removed building is gone for collision: t2 removes the tower, and player 1 drops to the ground
    'map_remove'    = @{ Env = "WOTM_MATCH=3,1,t2;WOTM_PLAYERS=2;WOTM_MODS=$mapMods;PS2X_DUMP_RAM=2400"; Secs = 60
                         Clean = $mapMods
                         Setup = { Clear-MapDumps
                                   & python $stageMap make --base CENTRAL --id t2 --name 'NO TOWER' --out "$mapMods\Maps\stages" --remove 0x165a70
                                   if ($LASTEXITCODE -ne 0) { throw 'stage_map.py make failed' } }
                         Expect = '\[stages\] t2: applied 1 edits|||\[match\]'
                         After = { param($disc) Test-MapRam $disc @('--monster', '0:-5:5') } }
    # a piece under a CONTROL (UFO's see-through switch 0x2d9680) moved across cells: the loader says the CONTROL stays, the grid
    # rule holds and a CPU fight runs for 60 s
    'map_control'   = @{ Env = "WOTM_MATCH=3,1,u1;WOTM_AIVSAI=1;WOTM_MODS=$mapMods;WOTM_STAGE_DUMP=$env:TEMP\wotm_map_u1.bin"; Secs = 60
                         Clean = $mapMods
                         Setup = { Clear-MapDumps
                                   & python $stageMap make --base UFO --id u1 --name 'UFO TEST' --out "$mapMods\Maps\stages" --move '0x2c3f90=122.1,-877.3,32.1'
                                   if ($LASTEXITCODE -ne 0) { throw 'stage_map.py make failed' } }
                         Expect = '\[stages\] u1: applied 1 edits|||\[stages\] u1: node 0x2c3f90: CONTROL 0x2d9680 above it keeps|||\[match\]'
                         Reject = '\[CRASH\]|||\[watchdog\] FROZE|||does not match|||refused|||nothing applied'
                         After = {
                             $out = & python $stageMap check "$env:TEMP\wotm_map_u1.bin"
                             $out | ForEach-Object { Write-Host "        $_" }
                             $LASTEXITCODE -eq 0 -and ($out -join ' ') -match ', OK'
                         } }
    # t1 with another baseHash (as if made from another disc): refused with a log line, and the stock stage still loads and fights
    'map_bad_hash'  = @{ Env = "WOTM_MATCH=3,1,t1;WOTM_AIVSAI=1;WOTM_MODS=$mapMods"; Secs = 40
                         Clean = $mapMods
                         Setup = {
                             & $makeT1 "$mapMods\Maps\stages"
                             $f = "$mapMods\Maps\stages\t1\stage.json"
                             (Get-Content $f -Raw) -replace '"baseHash": "[0-9a-f]+"', '"baseHash": "0123456789abcdef"' | Set-Content $f -Encoding ascii -NoNewline
                         }
                         Expect = '\[stages\] t1: base CENTRAL does not match this disc|||\[match\] 1: .*stage 1|||\[native\] resources: CENTRAL '
                         Reject = '\[CRASH\]|||\[watchdog\] FROZE|||\[stages\] t1: applied' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    $reject = if ($c.Reject) { $c.Reject } else { '\[CRASH\]|||\[watchdog\] FROZE' }
    if ($c.Clean -and (Test-Path $c.Clean)) { Remove-Item -Recurse -Force $c.Clean }
    if ($c.Setup) {
        try { & $c.Setup | ForEach-Object { Write-Host "        $_" } } catch { Write-Host "FAILED  setup: $_" -ForegroundColor Red; $failed += $name; continue }
    }
    & powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env $c.Env -Secs $c.Secs -Expect $c.Expect -Reject $reject
    if ($LASTEXITCODE -ne 0) { $failed += $name; continue }
    if ($c.After) {
        if (-not $runtime) { Write-Host "FAILED  $name (after the run): no runtime folder (set WOTM_RECOMP_DIR or .wotm_recomp_dir)" -ForegroundColor Red; $failed += $name; continue }
        if (-not (& $c.After (Join-Path $runtime 'discroot'))) { Write-Host "FAILED  $name (after the run)" -ForegroundColor Red; $failed += $name }
    }
}
if ($failed.Count) { Write-Host "Failed: $($failed -join ', ')" -ForegroundColor Red; exit 1 }
Write-Host 'All passed'
exit 0
