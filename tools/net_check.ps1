# ABOUTME: Teste da partida em rede num PC so: sobe o wotm_server, forma a partida pelo lobby (tools\net_lobby.py), abre um wotm.exe
# ABOUTME: por jogador (cada um com seu roteiro de pad) e confere que todos tem o mesmo estado em todo quadro (tools\statediff.py).
#
# Uso: powershell -File tools\net_check.ps1 [-Players 2] [-Monsters 3,1] [-Stage 1] [-Secs 70] [-Port 7811] [-Exe <wotm.exe>]...
#   -Exe: um wotm.exe por jogador (separados por virgula; o ultimo vale para os que sobram), para provar que dois builds em pastas
#     diferentes jogam juntos. Padrao: o build_dev do runtime.
#   -Drop <s>: fecha o ultimo jogador depois de s segundos e confere que os outros seguem (ele vira neutro no quadro combinado).
#   Logs e hashes ficam em <runtime>\net\ (servidor.log, jogador<N>.err.log, jogador<N>.hash). Sai com 0 se tudo bater.
param(
    [int]$Players = 2,
    [string]$Monsters = '3,1,2,5',
    [int]$Stage = 1,
    [int]$Secs = 70,
    [int]$Port = 7811,
    [string]$Exe = '',
    [int]$Drop = 0
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Pasta do runtime nao encontrada: rode run_live.bat uma vez ou defina WOTM_RECOMP_DIR." }
$server = Join-Path $repo 'native\net\build\wotm_server.exe'
if (-not (Test-Path $server)) { & cmd /c "`"$repo\tools\build_net.bat`" notest" | Out-Null }
if (-not (Test-Path $server)) { throw "wotm_server.exe nao compilou (tools\build_net.bat)" }
$exes = @(if ($Exe) { $Exe -split ',' } else { Join-Path $runtime 'build_dev\ps2xRuntime\wotm.exe' })
$disc = Join-Path $runtime 'discroot'
$elf = Join-Path $disc 'SCUS_971.97'
$out = Join-Path $runtime 'net'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out | Remove-Item -Force -Recurse

# o mesmo ambiente do run_live.ps1 (sem som, janela pequena, sem dispositivos do PC: so o roteiro de cada um)
if (-not $env:PS2X_CD_IMAGE) {
    $iso = Get-ChildItem (Join-Path $repo 'ISO') -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $iso) { throw "Nenhuma .iso em $repo\ISO (defina PS2X_CD_IMAGE)" }
    $env:PS2X_CD_IMAGE = $iso.FullName
}
$env:PS2X_NO_VU1 = '1'; $env:PS2X_NO_VIF1 = '1'; $env:PS2X_NO_GS = '1'; $env:PS2X_SCENE = '1'; $env:PS2X_FMV = '1'
$env:WOTM_MUTE = '1'; $env:WOTM_FULLSCREEN = '0'; $env:WOTM_RES = '360'; $env:WOTM_NOINPUT = '1'; $env:WOTM_REPO = $repo

$procs = @()
$srv = Start-Process -FilePath $server -ArgumentList "--port $Port" -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $out 'servidor.log') -RedirectStandardError (Join-Path $out 'servidor.err.log')
try {
    Start-Sleep -Milliseconds 500
    $names = (1..$Players | ForEach-Object { "J$_" }) -join ','
    $mons = ($Monsters -split ',')[0..($Players - 1)] -join ','
    & python (Join-Path $PSScriptRoot 'net_lobby.py') --server "127.0.0.1:$Port" --group $names --monsters $mons --stage $Stage --env-dir $out
    if ($LASTEXITCODE -ne 0) { throw 'o lobby nao formou a partida (tools\net_lobby.py)' }

    # roteiros: cada jogador anda, bate e pula em tempos diferentes (segundos desde a abertura; a luta comeca por volta de 15 s)
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
        $exePath = $exes[[Math]::Min($i - 1, $exes.Count - 1)]
        # cada jogador na sua pasta, como em PCs diferentes: o ELF, o cartao (mc0) e o save.json dele; o resto do disco vem da ISO.
        # Do segundo em diante o save tem outras opcoes (dificuldade, volumes, controle, tokens): a partida em rede nao pode le-lo.
        $pdir = Join-Path $out "pc$i"
        New-Item -ItemType Directory -Force $pdir | Out-Null
        Copy-Item $elf $pdir -Force
        if ($i -ge 2) {
            Set-Content (Join-Path $pdir 'save.json') ('{"options": {"difficulty": 2, "sfxVolume": ' + (40 + $i) + ', "musicVolume": 30, "mono": 1, ' +
                '"controllerConfig": [1, 1], "vibration": [0, 0]}, "battleTokens": 1234}') -Encoding ascii
        }
        $procs += Start-Process -FilePath $exePath -ArgumentList "`"$(Join-Path $pdir 'SCUS_971.97')`"" -WorkingDirectory $pdir -PassThru -WindowStyle Minimized `
            -RedirectStandardError (Join-Path $out "jogador$i.err.log") -RedirectStandardOutput (Join-Path $out "jogador$i.out.log")
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
function Fail([string]$why) { Write-Host "FALHOU  $why" -ForegroundColor Red; $script:failed = $true }
$srvLog = Get-Content (Join-Path $out 'servidor.log') -Raw
if ($srvLog -match 'DESSINCRONIA[^\r\n]*') { Fail "servidor: $($Matches[0])" }
if ($srvLog -notmatch 'inicio, atraso (\d+)') { Fail 'servidor nunca comecou a partida' } else { Write-Host "ok      servidor comecou a partida (atraso $($Matches[1]))" }
$alive = if ($Drop -gt 0) { $Players - 1 } else { $Players }
for ($i = 1; $i -le $Players; $i++) {
    $log = Get-Content (Join-Path $out "jogador$i.err.log") -Raw
    if ($log -notmatch "\[net\] inicio: jogador $i de $Players") { Fail "jogador $i nao comecou a partida" } else { Write-Host "ok      jogador $i comecou" }
    if ($log -match '\[net\] parado: [^\r\n]*') { if (-not ($Drop -gt 0 -and $i -eq $Players)) { Fail "jogador ${i}: $($Matches[0])" } }
    if ($log -match '\[CRASH\]|\[vigia\] CONGELOU') { Fail "jogador ${i}: $($Matches[0])" }
    if ($log -notmatch '\[sync\] primeira entrada nao neutra aplicada no quadro') { Fail "jogador ${i}: nenhuma entrada chegou ao jogo" }
}
if ($Drop -gt 0 -and $srvLog -notmatch "J$Players saiu da partida \(neutro desde o quadro \d+\)") { Fail "servidor nao registrou a saida do jogador $Players" }
for ($i = 2; $i -le $alive; $i++) {
    $r = & python (Join-Path $PSScriptRoot 'statediff.py') (Join-Path $out 'jogador1.hash') (Join-Path $out "jogador$i.hash")
    $r | ForEach-Object { Write-Host "        $_" }
    if ($LASTEXITCODE -ne 0) { Fail "estado do jogador $i difere do jogador 1" }
    elseif (($r -join ' ') -match 'identical for (\d+) frames' -and [int]$Matches[1] -lt 1200) { Fail "so $($Matches[1]) quadros comparados com o jogador $i" }
}
if ($failed) { Write-Host 'FALHOU'; exit 1 }
Write-Host 'PASSOU'
exit 0
