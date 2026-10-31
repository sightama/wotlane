# ABOUTME: Network match checks (fixed network profile, lockstep dialogs): each runs tools\net_check.ps1 with 2 players and checks
# ABOUTME: identical state on every frame plus the log. Usage: powershell -File tools\net_checks.ps1 [-Only <name>]; exits 1 if any fail.
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'net_check.ps1'

$checks = [ordered]@{
    # mouse on one PC only (WOTM_ORBITMOUSE: mouse-like movement from orbit frame 300 on): the modern camera applied it to player 1's
    # game camera on that PC only and the match desynced (block 0x800000, the camera's _cs). On the network: classic camera
    'camera_mouse'  = @{ PlayerEnv = '2:WOTM_ORBITMOUSE=300:600:4:0'; Secs = 50
                         Expect = 'network match: fixed profile' }
    # different game options on each PC (classic camera on one, modern on the other: with modern the pad is screen-relative; toggled
    # lock-on, camera sensitivity and inversion): the fixed profile applies on both
    'opcoes_pc'     = @{ PlayerEnv = '1:WOTM_CAMERA=0;WOTM_CAMSENS=10;WOTM_CAMINVERT=1|||2:WOTM_CAMERA=1;WOTM_LOCKTOGGLE=1'; Secs = 50 }
    # photo mode on one PC only (WOTM_PHOTO: turns itself on 25 s after the first scene): it paused that PC and freed the camera; on the
    # network it stays off
    'foto'          = @{ PlayerEnv = '2:WOTM_PHOTO=25'; Secs = 50
                         Reject = '\[photo\] on|||\[photo\] automatic test' }
    # pause by player 1, X on RECOMPILED OPTIONS: the panel reads each PC's keyboard and mouse (outside the network's control) and would
    # close on different frames; on the network that line does not open the panel and the pause stays in lockstep (Start again resumes
    # the fight) (short taps: when held, the menu repeats and skips past the line)
    'painel_pausa'  = @{ PlayerEnv = '1:PS2X_PAD=24:0.3:start;26:0.2:down;27:0.2:down;28:0.3:cross;31:0.3:start;33:3:up;37:1:square'; Secs = 50
                         Reject = '\[pause\] panel opened' }
    # 1-win target: the "vencer" cheat (timed by g_frame, the same on both) knocks out player 2, dialog 5 opens on both PCs and player 1's
    # X picks PLAY AGAIN (the cursor starts there; the dialog accepts any player's d-pad, so player 2 stays still): round 2 starts and
    # ends the same on both. The dialog loop runs in lockstep (without that the frame index stalled and the menu did not move)
    'meta_rede'     = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 60
                         Expect = '\[dialog\] 5 opened|||round 2 ended' }
    # REBUILD LEVEL in dialog 5 (down, X): ResetLevel rebuilds the stage without reloading; round 2 plays and ends the same on both
    'rebuild_rede'  = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.2:down;35:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 60
                         Expect = '\[dialog\] 5 opened|||round 2 ended' }
    # QUIT in dialog 5 (down, down, X; ARE YOU SURE has NO selected: up, X): off the network the game returns to userintMain and the
    # direct match would restart (a second stage load, which desyncs); on the network the match ends and the game closes on every PC
    # (the launcher goes back to the lobby)
    'sair_rede'     = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.2:down;35:0.2:down;36:0.3:cross;38:0.2:up;39:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 55
                         Expect = '\[net\] match ended by QUIT'; Reject = 'second level load' }
    # different mods (player 2 without the repo's mods): the mods digest is part of the configuration hash and the server rejects it
    'mods_diferentes' = @{ PlayerEnv = '2:WOTM_MODS=0'; Secs = 30; ExpectRefused = 'game configuration differs' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    $a = @('-NoProfile', '-File', $check, '-Players', '2', '-Secs', $c.Secs)
    foreach ($p in 'PlayerEnv', 'ExtraEnv', 'Expect', 'Reject', 'ExpectRefused', 'Wins') { if ($c[$p]) { $a += @("-$p", $c[$p]) } }
    & powershell @a
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}
if ($failed) { Write-Host "FAILED: $($failed -join ', ')"; exit 1 }
Write-Host 'all passed'
exit 0
