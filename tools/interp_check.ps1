# ABOUTME: Checagem da interpolacao de quadros: luta com IA numa tela de 144 Hz, uma vez ligada (autoteste alfa 1 = B e camera suave)
# ABOUTME: e uma desligada (camera aos saltos), medindo o passo da camera por apresentacao com tools\interp_smooth.py. Sai com 0 se passar.
#
# Uso: powershell -File tools\interp_check.ps1 [-Secs 40] [-Match 3,1,1] [-Players 1] [-Hz 144]
param([int]$Secs = 40, [string]$Match = '3,1,1', [int]$Players = 1, [int]$Hz = 144)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
$check = Join-Path $PSScriptRoot 'live_check.ps1'
$smooth = Join-Path $PSScriptRoot 'interp_smooth.py'
# com vsync (como se joga): cada apresentacao vai para um vblank do monitor; WOTM_HZ abaixo do monitor usa o limite por tempo
$base = "WOTM_MATCH=$Match;WOTM_AIVSAI=1;WOTM_INTERPLOG=1;WOTM_HZ=$Hz;WOTM_VSYNC=;PS2X_STATS=1"
if ($Players -ge 2) { $base += ";WOTM_PLAYERS=$Players;WOTM_LOCALPLAYER=1" }
$failed = $false

Write-Host '== ligada'
& powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env "$base;WOTM_INTERP=1;WOTM_INTERP_SELFTEST=25" -Secs $Secs `
    -Expect '\[interp\] ligada|||\[interp\] autoteste [^\r\n]*jogo parado\): B x B: 0; alfa 1 x B: 0 pixels diferentes'
if ($LASTEXITCODE -ne 0) { $failed = $true }
Copy-Item (Join-Path $runtime 'live_check.log') (Join-Path $runtime 'interp_on.log') -Force
& python $smooth (Join-Path $runtime 'interp_on.log') --max-ms 8
if ($LASTEXITCODE -ne 0) { Write-Host 'FALHOU: atraso da imagem varia demais com a interpolacao' -ForegroundColor Red; $failed = $true }

Write-Host '== desligada (controle: a medida tem que ver os saltos)'
& powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env "$base;WOTM_INTERP=0;WOTM_INTERP_SELFTEST=" -Secs $Secs -Expect '\[interp\] t='
if ($LASTEXITCODE -ne 0) { $failed = $true }
Copy-Item (Join-Path $runtime 'live_check.log') (Join-Path $runtime 'interp_off.log') -Force
& python $smooth (Join-Path $runtime 'interp_off.log') --min-ms 12
if ($LASTEXITCODE -ne 0) { Write-Host 'FALHOU: sem interpolacao a medida devia ver o dente de serra' -ForegroundColor Red; $failed = $true }

if ($failed) { Write-Host 'FALHOU'; exit 1 }
Write-Host 'PASSOU'
exit 0
