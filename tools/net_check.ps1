# ABOUTME: Network match test on a single PC: starts wotm_server, forms the match through the lobby (tools\net_lobby.py), opens one
# ABOUTME: wotm.exe per player (each with its own pad script) and checks that all have the same state on every frame (tools\statediff.py).
#
# Usage: powershell -File tools\net_check.ps1 [-Players 2] [-Monsters 3,1] [-Stage 1] [-Secs 70] [-Port 7811] [-Exe <wotm.exe>]...
#   -Exe: one wotm.exe per player (comma-separated; the last one applies to any remaining players), to prove that two builds in
#     different folders play together. Default: the runtime's build_dev.
#   -Wins <n>: wins needed to end the match (the lobby owner picks them; they become WOTM_KILLTARGET for everyone). 0 = endless.
#   -Drop <s>: closes the last player after s seconds and checks that the others carry on (that player turns neutral from the agreed frame).
#   -ExtraEnv "A=1;B=2": extra variables for all players (the same for everyone, otherwise the server rejects the configuration).
#   -PlayerEnv "2:A=1;B=2|||3:C=1": variables for player N only (what differs on just one PC: mouse, this PC's options).
#   -Expect "re1|||re2": regexes that must appear in every player's log. -Reject: ones that must not appear in any.
#   -ExpectRefused "<regex>": the server must reject the match with this reason (configuration, build) and nobody may start.
#   Logs and hashes go to <runtime>\net\ (servidor.log, jogador<N>.err.log, jogador<N>.hash). Exits with 0 if everything matches.
param(
    [int]$Players = 2,
    [string]$Monsters = '3,1,2,5,4,7,8,9',   # one per player, up to 8 (valid ones: native/net/wotm_monsters.hpp)
    [int]$Stage = 1,
    [int]$Wins = 0,
    [int]$Secs = 70,
    [int]$Port = 7811,
    [string]$Exe = '',
    [int]$Drop = 0,
    [string]$ExtraEnv = '',
    [string]$PlayerEnv = '',
    [string]$Expect = '',
    [string]$Reject = '',
    [string]$ExpectRefused = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Runtime folder not found: run run_live.bat once or set WOTM_RECOMP_DIR." }
$server = Join-Path $repo 'native\net\build\wotm_server.exe'
if (-not (Test-Path $server)) { & cmd /c "`"$repo\tools\build_net.bat`" notest" | Out-Null }
if (-not (Test-Path $server)) { throw "wotm_server.exe did not build (tools\build_net.bat)" }
$exes = @(if ($Exe) { $Exe -split ',' } else { Join-Path $runtime 'build_dev\ps2xRuntime\wotm.exe' })
$disc = Join-Path $runtime 'discroot'
$elf = Join-Path $disc 'SCUS_971.97'
$out = Join-Path $runtime 'net'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out | Remove-Item -Force -Recurse

# the same environment as run_live.ps1 (no sound, small window, no PC input devices: only each player's script)
if (-not $env:PS2X_CD_IMAGE) {
    $iso = Get-ChildItem (Join-Path $repo 'ISO') -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $iso) { throw "No .iso in $repo\ISO (set PS2X_CD_IMAGE)" }
    $env:PS2X_CD_IMAGE = $iso.FullName
}
$env:PS2X_NO_VU1 = '1'; $env:PS2X_NO_VIF1 = '1'; $env:PS2X_NO_GS = '1'; $env:PS2X_SCENE = '1'; $env:PS2X_FMV = '1'
$env:WOTM_MUTE = '1'; $env:WOTM_FULLSCREEN = '0'; $env:WOTM_RES = '360'; if (-not $env:WOTM_MONITOR) { $env:WOTM_MONITOR = '1' }; $env:WOTM_NOINPUT = '1'; $env:WOTM_REPO = $repo; $env:WOTM_SETTINGS = '0'
foreach ($pair in ($ExtraEnv -split ';' | Where-Object { $_.Trim() })) {
    $k = $pair.IndexOf('=')
    [Environment]::SetEnvironmentVariable($pair.Substring(0, $k).Trim(), $pair.Substring($k + 1))
}

$procs = @()
$srv = Start-Process -FilePath $server -ArgumentList "--port $Port" -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $out 'servidor.log') -RedirectStandardError (Join-Path $out 'servidor.err.log')
try {
    Start-Sleep -Milliseconds 500
    $names = (1..$Players | ForEach-Object { "J$_" }) -join ','
    $mons = ($Monsters -split ',')[0..($Players - 1)] -join ','
    & python (Join-Path $PSScriptRoot 'net_lobby.py') --server "127.0.0.1:$Port" --group $names --monsters $mons --stage $Stage --wins $Wins --env-dir $out
    if ($LASTEXITCODE -ne 0) { throw 'the lobby did not form the match (tools\net_lobby.py)' }

    # scripts: each player walks, hits and jumps at different times (seconds since launch; the fight starts at about 15 s)
    $scripts = @(
        '18:6:up;20:1:square;22:1:cross;24:3:left;25:1:triangle;33:5:down;34:1:square;45:2:right;46:1:circle',
        '19:5:up;21:1:triangle;23:2:right;26:1:square;31:1:circle;36:4:left;40:1:cross;47:1:square',
        '18:3:down;21:1:square;24:1:cross;28:4:right;30:1:triangle;38:2:up;43:1:circle',
        '20:4:left;22:1:circle;25:1:square;29:3:up;32:1:cross;41:2:down;44:1:triangle'
    )
    for ($i = 1; $i -le $Players; $i++) {
        foreach ($line in Get-Content (Join-Path $out "env_$i.txt")) {
            $k = $line.IndexOf('=')
            [Environment]::SetEnvironmentVariable($line.Substring(0, $k), $line.Substring($k + 1))
        }
        $env:WOTM_STATEHASH = Join-Path $out "jogador$i.hash"
        $env:PS2X_PAD = $scripts[($i - 1) % $scripts.Count]
        $own = @()   # the -PlayerEnv ones come last (they may replace this player's pad script) and are cleared after that player launches
        foreach ($spec in ($PlayerEnv -split '\|\|\|' | Where-Object { $_ -match "^$i`:" })) {
            # NAME=value separated by ';'; a piece without '=' continues the previous value (the PS2X_PAD script uses ';')
            $pairs = @()
            foreach ($part in ($spec.Substring($spec.IndexOf(':') + 1) -split ';')) {
                if ($part -match '^\s*[A-Z0-9_]+=' -or $pairs.Count -eq 0) { $pairs += $part } else { $pairs[-1] += ";$part" }
            }
            foreach ($pair in ($pairs | Where-Object { $_.Trim() })) {
                $k = $pair.IndexOf('=')
                [Environment]::SetEnvironmentVariable($pair.Substring(0, $k).Trim(), $pair.Substring($k + 1))
                $own += $pair.Substring(0, $k).Trim()
            }
        }
        $exePath = $exes[[Math]::Min($i - 1, $exes.Count - 1)]
        # each player in their own folder, as on different PCs: their own ELF, memory card (mc0) and save.json; the rest of the disc comes
        # from the ISO. From the second player on, the save has different options (difficulty, volumes, pad, tokens): the network
        # match must not read it.
        $pdir = Join-Path $out "pc$i"
        New-Item -ItemType Directory -Force $pdir | Out-Null
        Copy-Item $elf $pdir -Force
        if ($i -ge 2) {
            Set-Content (Join-Path $pdir 'save.json') ('{"options": {"difficulty": 2, "sfxVolume": ' + (40 + $i) + ', "musicVolume": 30, "mono": 1, ' +
                '"controllerConfig": [1, 1], "vibration": [0, 0]}, "battleTokens": 1234}') -Encoding ascii
        }
        $procs += Start-Process -FilePath $exePath -ArgumentList "`"$(Join-Path $pdir 'SCUS_971.97')`"" -WorkingDirectory $pdir -PassThru -WindowStyle Minimized `
            -RedirectStandardError (Join-Path $out "jogador$i.err.log") -RedirectStandardOutput (Join-Path $out "jogador$i.out.log")
        foreach ($k in $own) { [Environment]::SetEnvironmentVariable($k, $null) }
        $env:PS2X_PAD = $null
    }
    if ($Drop -gt 0) {
        Start-Sleep -Seconds $Drop
        $last = $procs[-1]
        $last.CloseMainWindow() | Out-Null
        Start-Sleep -Seconds 2
        if (-not $last.HasExited) { $last.Kill() }
        Start-Sleep -Seconds ([Math]::Max(1, $Secs - $Drop))
    } else {
        Start-Sleep -Seconds $Secs
    }
} finally {
    foreach ($p in $procs) { if (-not $p.HasExited) { $p.CloseMainWindow() | Out-Null } }
    Start-Sleep -Seconds 3
    foreach ($p in $procs) { if (-not $p.HasExited) { $p.Kill() } }
    if (-not $srv.HasExited) { $srv.Kill() }
}

$failed = $false
function Fail([string]$why) { Write-Host "FAILED  $why" -ForegroundColor Red; $script:failed = $true }
$srvLog = Get-Content (Join-Path $out 'servidor.log') -Raw
if ($ExpectRefused) {
    # the match must not start: some player gets the server's rejection with this reason, and none reaches the start
    $refused = $false
    for ($i = 1; $i -le $Players; $i++) {
        $log = Get-Content (Join-Path $out "jogador$i.err.log") -Raw
        if ($log -match "\[net\] stopped: [^\r\n]*$ExpectRefused[^\r\n]*") { $refused = $true; Write-Host "ok      player ${i}: $($Matches[0])" }
        if ($log -match "\[net\] start: player") { Fail "player $i started the match" }
    }
    if (-not $refused) { Fail "nobody was rejected for '$ExpectRefused'" }
    if ($failed) { Write-Host 'FAILED'; exit 1 }
    Write-Host 'PASSED'
    exit 0
}
if ($srvLog -match 'DESYNC[^\r\n]*') { Fail "server: $($Matches[0])" }
if ($srvLog -notmatch 'start, delay (\d+)') { Fail 'server never started the match' } else { Write-Host "ok      server started the match (delay $($Matches[1]))" }
$alive = if ($Drop -gt 0) { $Players - 1 } else { $Players }
for ($i = 1; $i -le $Players; $i++) {
    $log = Get-Content (Join-Path $out "jogador$i.err.log") -Raw
    if ($log -notmatch "\[net\] start: player $i of $Players") { Fail "player $i did not start the match" } else { Write-Host "ok      player $i started" }
    if ($log -match '\[net\] stopped: [^\r\n]*') { if (-not ($Drop -gt 0 -and $i -eq $Players)) { Fail "player ${i}: $($Matches[0])" } }
    if ($log -match '\[CRASH\]|\[watchdog\] FROZE') { Fail "player ${i}: $($Matches[0])" }
    if ($log -notmatch '\[sync\] first non-neutral input applied at frame') { Fail "player ${i}: no input reached the game" }
    foreach ($re in ($Reject -split '\|\|\|' | Where-Object { $_ })) {
        if ($log -match $re) { Fail "player ${i}: found $($Matches[0])" }
    }
    foreach ($re in ($Expect -split '\|\|\|' | Where-Object { $_ })) {
        if ($log -notmatch $re) { Fail "player ${i}: missing $re" }
    }
}
if ($Drop -gt 0 -and $srvLog -notmatch "J$Players left the match \(neutral from frame \d+\)") { Fail "server did not log player $Players leaving" }
for ($i = 2; $i -le $alive; $i++) {
    $r = & python (Join-Path $PSScriptRoot 'statediff.py') (Join-Path $out 'jogador1.hash') (Join-Path $out "jogador$i.hash")
    $r | ForEach-Object { Write-Host "        $_" }
    if ($LASTEXITCODE -ne 0) { Fail "player $i state differs from player 1" }
    elseif (($r -join ' ') -match 'identical for (\d+) frames' -and [int]$Matches[1] -lt 1200) { Fail "only $($Matches[1]) frames compared with player $i" }
}
if ($failed) { Write-Host 'FAILED'; exit 1 }
Write-Host 'PASSED'
exit 0
