# ABOUTME: Network match checks (fixed network profile, lockstep dialogs): each runs tools\net_check.ps1 with 2 players and checks
# ABOUTME: identical state on every frame plus the log. Usage: powershell -File tools\net_checks.ps1 [-Only <name>]; exits 1 if any fail.
# Entry keys: the net_check.ps1 parameters (PlayerEnv, ExtraEnv, Expect, Reject, ExpectRefused, Wins, Stage, Secs) and Setup (optional:
# a scriptblock run before the match; a failure fails the entry) and After (optional: a scriptblock run after a passing match; a
# false result or an error fails the entry).
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
# map copies (docs/superpowers/specs/2026-11-09-map-copy-design.md): live_checks.ps1's c1 (the Apcomm tower 0x165a70 removed, the
# low building 0x2d85a0 copied onto player 1's start point and 0x18a410 into the park, with their riders and power-ups 5 and 15)
# plus 50 copies of 0x2d85a0 without riders on a 10 x 5 lattice from (-1300, -1100), 260 and 440 apart, at its disc z 74.1.
# make's --no-riders applies to a whole map, so the lattice is made as a second map and its copies are spliced into c1's edits
# after c1's own copies (the loader's file order: piece edits, copies, power-up moves, power-up additions).
$copyDumps = "$env:TEMP\wotm_net_c1"
$makeCopyMap = {
    Remove-Item "${copyDumps}_*.bin*" -ErrorAction SilentlyContinue   # no stale dumps from an earlier run
    $lattice = "$env:TEMP\wotm_map_net_lattice"
    foreach ($d in $mapMods, $lattice) { if (Test-Path $d) { Remove-Item -Recurse -Force $d }; New-Item -ItemType Directory -Force $d | Out-Null }
    $stageMap = Join-Path $PSScriptRoot 'stage_map.py'
    & python $stageMap make --base CENTRAL --id c1 --name 'COPY TEST' --out "$mapMods\Maps\stages" --remove 0x165a70 `
        --copy '0x2d85a0=768.1,499.6,74.1' --copy '0x18a410=400,-100,-0.8'
    if ($LASTEXITCODE -ne 0) { throw 'stage_map.py make failed (c1)' }
    $copies = for ($i = 0; $i -lt 50; $i++) { '--copy'; '0x2d85a0={0},{1},74.1' -f (-1300 + 260 * ($i % 10)), (-1100 + 440 * [math]::Floor($i / 10)) }
    & python $stageMap make --base CENTRAL --id lattice --name 'LATTICE' --out $lattice --no-riders @copies | Where-Object { $_ -notmatch '^  copy 0x' }
    if ($LASTEXITCODE -ne 0) { throw 'stage_map.py make failed (lattice)' }
    $file = "$mapMods\Maps\stages\c1\stage.json"
    $doc = Get-Content $file -Raw | ConvertFrom-Json
    $extra = @((Get-Content "$lattice\lattice\stage.json" -Raw | ConvertFrom-Json).edits)
    $edits = @($doc.edits)
    $last = -1
    for ($i = 0; $i -lt $edits.Count; $i++) { if ($null -ne $edits[$i].copy) { $last = $i } }
    if ($last -lt 0 -or $extra.Count -ne 50) { throw "unexpected maps: c1 copies end at $last, lattice has $($extra.Count) edits" }
    $doc.edits = @($edits[0..$last]) + $extra + @(if ($last + 1 -lt $edits.Count) { $edits[($last + 1)..($edits.Count - 1)] })
    [IO.File]::WriteAllText($file, ($doc | ConvertTo-Json -Depth 6), (New-Object Text.UTF8Encoding $false))
    Remove-Item -Recurse -Force $lattice
    "c1: $($doc.edits.Count) edits ($($extra.Count) lattice copies after edit $last)"
}

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
    # a skin made by the character creator in every PC's mods folder (the same folder) does not break the mods digest or the
    # lockstep; the skin itself is not picked online (the lobby carries disc costumes only, #14). Every log must have loaded it.
    # Run the WOTM_CREATOR live check first (tools\live_checks.ps1 -Only creator_script): it creates the folder
    'creator_skin_folder' = @{ ExtraEnv = "WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 60
                         Expect = '\[mods\] skin togera/green .* = costume 4' }
    # a saved map online (WOTM_MATCH's stage field is the map id on both PCs, the same mods folder): applied on both, same state
    'map_online'    = @{ Setup = $makeMap; ExtraEnv = "WOTM_MODS=$mapMods"; Stage = 't1'; Secs = 60
                         Expect = '\[stages\] map t1 |||\[stages\] t1: applied 13 edits'
                         Reject = 'does not match|||nothing applied|||not found in mods' }
    # the second PC without the map's folder: the mods digest differs and the server refuses the match (as mods_diferentes)
    'map_online_missing' = @{ Setup = $makeMap; ExtraEnv = "WOTM_MODS=$mapMods"; PlayerEnv = "2:WOTM_MODS=$noMap"; Stage = 't1'; Secs = 30
                         ExpectRefused = 'game configuration differs' }
    # map copies online: c1 plus 50 lattice copies applied on both PCs, same state on every frame. The per-frame state hash covers
    # only the low 32 MB of guest RAM and the copies' nodes live in the map reserve above it (plan decision 16), so each PC also
    # dumps its stage image and reserve (WOTM_STAGE_DUMP) and both pairs must match byte for byte
    'map_copy_online' = @{ Setup = $makeCopyMap; ExtraEnv = "WOTM_MODS=$mapMods"; Stage = 'c1'; Secs = 60
                         PlayerEnv = "1:WOTM_STAGE_DUMP=${copyDumps}_1.bin|||2:WOTM_STAGE_DUMP=${copyDumps}_2.bin"
                         Expect = '\[stages\] c1: copies: 58 applied, 0 refused'
                         Reject = 'does not match|||nothing applied|||not found in mods'
                         After = {
                             $same = $true
                             foreach ($ext in '.bin', '.bin.reserve') {
                                 $a = Get-Item "${copyDumps}_1$ext"; $b = Get-Item "${copyDumps}_2$ext"
                                 $ha = (Get-FileHash $a.FullName).Hash; $hb = (Get-FileHash $b.FullName).Hash
                                 Write-Host "        $ext : $($a.Length) / $($b.Length) bytes, $(if ($ha -eq $hb) { "same SHA256 $ha" } else { 'DIFFERENT' })"
                                 $same = $same -and $a.Length -gt 0 -and $ha -eq $hb
                             }
                             $same
                         } }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    if ($c.Setup) {
        try { & $c.Setup | ForEach-Object { Write-Host "        $_" } } catch { Write-Host "FAILED  setup: $_"; $failed += $name; continue }
    }
    $a = @('-NoProfile', '-File', $check, '-Players', '2', '-Secs', $c.Secs)
    foreach ($p in 'PlayerEnv', 'ExtraEnv', 'Expect', 'Reject', 'ExpectRefused', 'Wins', 'Stage') { if ($c[$p]) { $a += @("-$p", $c[$p]) } }
    & powershell @a
    if ($LASTEXITCODE -ne 0) { $failed += $name; continue }
    if ($c.After) {
        $ok = try { & $c.After } catch { Write-Host "FAILED  after the match: $_"; $false }
        if (-not $ok) { Write-Host "FAILED  $name (after the match)"; $failed += $name }
    }
}
if ($failed) { Write-Host "FAILED: $($failed -join ', ')"; exit 1 }
Write-Host 'all passed'
exit 0
