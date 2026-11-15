# ABOUTME: Network match checks (fixed network profile, lockstep dialogs): each runs tools\net_check.ps1 with 2 players (3 where it
# ABOUTME: says so) and checks identical state on every frame plus the log. Usage: powershell -File tools\net_checks.ps1 [-Only <name>]; exits 1 if any fail.
# Entry keys: the net_check.ps1 parameters (Players, PlayerEnv, ExtraEnv, Expect, Reject, ExpectRefused, Wins, Modifiers, Stage, Secs)
# and Setup (optional: a scriptblock run before the match; a failure fails the entry).
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'net_check.ps1'
# map creator: map t1 (stage_map.py, CENTRAL: one removed, three moved, one turned) in a folder every PC uses, and an empty mods
# folder for the PC that lacks it
$mapMods = "$env:TEMP\wotm_map_net"
$noMap = "$env:TEMP\wotm_map_net_none"
$makeMap = {
    foreach ($d in $mapMods, $noMap) { if (Test-Path $d) { Remove-Item -Recurse -Force $d }; New-Item -ItemType Directory -Force $d | Out-Null }
    & python (Join-Path $PSScriptRoot 'stage_map.py') make --base CENTRAL --id t1 --name 'TEST MAP' --out "$mapMods\Maps\stages" --remove 0x18a410 `
        --move '0x165a70=-185.7,475.8,0' --move '0x2d85a0=768.1,499.6,74.1' --turn '0x4da6b0=90' --move '0x5c95a0=400,-100,11.9'
    if ($LASTEXITCODE -ne 0) { throw 'stage_map.py make failed' }
}

$checks = [ordered]@{
    # mouse on one PC only (WOTM_ORBITMOUSE: mouse-like movement from input frame 300 on), modern camera on both: the motion travels in
    # player 2's input with player 2's own sensitivity and inversion, so both PCs turn that camera alike (when it stayed on one PC, the
    # match desynced at block 0x800000, the camera's _cs)
    'camera_mouse'  = @{ PlayerEnv = '1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1;WOTM_CAMSENS=8;WOTM_CAMINVERT=1;WOTM_ORBITMOUSE=300:600:4:1'; Secs = 50
                         Expect = 'network match: fixed profile' }
    # different game options on each PC (classic camera on one, modern on the other: with modern the pad is screen-relative; camera
    # sensitivity and inversion; toggled lock-on): the camera options are each player's own and travel with their input, lock-on is the
    # fixed profile's on both
    'pc_options'    = @{ PlayerEnv = '1:WOTM_CAMERA=0;WOTM_CAMSENS=10;WOTM_CAMINVERT=1|||2:WOTM_CAMERA=1;WOTM_LOCKTOGGLE=1'; Secs = 50 }
    # photo mode on one PC only (WOTM_PHOTO: turns itself on 25 s after the first scene): it paused that PC and freed the camera; on the
    # network it stays off
    'photo'         = @{ PlayerEnv = '2:WOTM_PHOTO=25'; Secs = 50
                         Reject = '\[photo\] on|||\[photo\] automatic test' }
    # pause by player 1, X on RECOMPILED OPTIONS: the panel reads each PC's keyboard and mouse (outside the network's control) and would
    # close on different frames; on the network that line does not open the panel and the pause stays in lockstep (Start again resumes
    # the fight) (short taps: when held, the menu repeats and skips past the line)
    'pause_panel'   = @{ PlayerEnv = '1:PS2X_PAD=24:0.3:start;26:0.2:down;27:0.2:down;28:0.3:cross;31:0.3:start;33:3:up;37:1:square'; Secs = 50
                         Reject = '\[pause\] panel opened' }
    # pause by player 1, screen while paused: the pause's viewGrappleConfig (gUseUnifiedView on) copied view 2's _cs into view 0, and with
    # one view per player there is no view 2: view 0 had no camera, every paused frame was dropped and the screen went black on every PC.
    # A WOTM_SHOTS photo is written only when the window draws a frame, so both PCs must write the one taken while paused
    'pause_screen'  = @{ ExtraEnv = 'WOTM_SHOTS=30'; PlayerEnv = '1:PS2X_PAD=24:0.3:start'; Secs = 40
                         Expect = '\[dialog\] 0 opened|||\[shot\] shot_30\.png' }
    # 1-win target: the "vencer" cheat (timed by g_frame, the same on both) knocks out player 2, dialog 5 opens on both PCs and player 1's
    # X picks PLAY AGAIN (the cursor starts there; the dialog accepts any player's d-pad, so player 2 stays still): round 2 starts and
    # ends the same on both. The dialog loop runs in lockstep (without that the frame index stalled and the menu did not move)
    'wins_target'   = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 60
                         Expect = '\[dialog\] 5 opened|||round 2 ended' }
    # REBUILD LEVEL in dialog 5 (down, X): ResetLevel rebuilds the stage without reloading; round 2 plays and ends the same on both
    'rebuild_level' = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.2:down;35:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 60
                         Expect = '\[dialog\] 5 opened|||round 2 ended' }
    # QUIT in dialog 5 (down, down, X; ARE YOU SURE has NO selected: up, X): off the network the game returns to userintMain and the
    # direct match would restart (a second stage load, which desyncs); on the network the match ends and the game closes on every PC
    # (the launcher goes back to the lobby)
    'quit_match'    = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.2:down;35:0.2:down;36:0.3:cross;38:0.2:up;39:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 55
                         Expect = '\[net\] match ended by QUIT'; Reject = 'second level load' }
    # different mods (player 2 without the repo's mods): the mods digest is part of the configuration hash and the server rejects it
    'different_mods' = @{ PlayerEnv = '2:WOTM_MODS=0'; Secs = 30; ExpectRefused = 'game configuration differs' }
    # training on one PC only: it is outside the configuration hash and changes the game state (the pause menu gets two more rows, the
    # dummy, the refills), so online it is ignored; player 1 pauses to prove the menu stays equal
    'training_online' = @{ PlayerEnv = '1:WOTM_TRAINING=1;PS2X_PAD=24:0.3:start'; Secs = 40
                           Expect = '1:\[training\] WOTM_TRAINING ignored|||\[dialog\] 0 opened' }
    # 3 players (the game's 30 Hz mode: fixed step 2, quadrant views, wotm_split4's grapple hook), modern camera on everyone, player 2
    # pauses (the pause menu, the FIELD-bit fix of rtReturnToShell and the paused HUD all in lockstep)
    'pause_3p'      = @{ Players = 3; PlayerEnv = '1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1;PS2X_PAD=24:0.3:start|||3:WOTM_CAMERA=1'; Secs = 50
                         Expect = '\[dialog\] 0 opened' }
    # 3 players, player 3 (camera 2, the one the orbit code special-cases in the unified view) with the mouse orbit, its own sensitivity
    # and inversion, and the pause: the motion travels in its input, so every PC turns that camera alike across the pause
    'pause_3p_mouse' = @{ Players = 3; PlayerEnv = '1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1|||3:WOTM_CAMERA=1;WOTM_CAMSENS=8;WOTM_CAMINVERT=1;WOTM_ORBITMOUSE=300:900:4:1;PS2X_PAD=24:0.3:start'
                          Secs = 50; Expect = '\[dialog\] 0 opened' }
    # 3 players with Turbo (2 fields per frame on top of the 30 Hz mode) and a pause by player 1
    'turbo_3p'      = @{ Players = 3; Modifiers = 'turbo'; PlayerEnv = '1:PS2X_PAD=24:0.3:start'; Secs = 45
                         Expect = '\[dialog\] 0 opened' }
    # a skin made by the character creator in every PC's mods folder (the same folder) does not break the mods digest or the
    # lockstep; the skin itself is not picked online (the lobby carries disc costumes only, #14). Every log must have loaded it.
    # Run the WOTM_CREATOR live check first (tools\live_checks.ps1 -Only creator_script): it creates the folder
    'creator_skin_folder' = @{ ExtraEnv = "WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 60
                         Expect = '\[mods\] skin togera/green .* = costume 4' }
    # a saved map online (WOTM_MATCH's stage field is the map id on both PCs, the same mods folder): applied on both, same state
    'map_online'    = @{ Setup = $makeMap; ExtraEnv = "WOTM_MODS=$mapMods"; Stage = 't1'; Secs = 60
                         Expect = '\[stages\] map t1 |||\[stages\] t1: applied 13 edits'
                         Reject = 'does not match|||nothing applied|||not found in mods' }
    # the second PC without the map's folder: the mods digest differs and the server refuses the match (as different_mods)
    'map_online_missing' = @{ Setup = $makeMap; ExtraEnv = "WOTM_MODS=$mapMods"; PlayerEnv = "2:WOTM_MODS=$noMap"; Stage = 't1'; Secs = 30
                         ExpectRefused = 'game configuration differs' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    if ($c.Setup) {
        try { & $c.Setup | ForEach-Object { Write-Host "        $_" } } catch { Write-Host "FAILED  setup: $_"; $failed += $name; continue }
    }
    $players = if ($c.Players) { $c.Players } else { 2 }
    $a = @('-NoProfile', '-File', $check, '-Players', $players, '-Secs', $c.Secs)
    foreach ($p in 'PlayerEnv', 'ExtraEnv', 'Expect', 'Reject', 'ExpectRefused', 'Wins', 'Modifiers', 'Stage') { if ($c[$p]) { $a += @("-$p", $c[$p]) } }
    & powershell @a
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}
if ($failed) { Write-Host "FAILED: $($failed -join ', ')"; exit 1 }
Write-Host 'all passed'
exit 0
