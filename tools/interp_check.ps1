# ABOUTME: Checagem da interpolacao de quadros: luta com IA numa tela acima de 60 Hz com a interpolacao ligada, medindo pelas linhas
# ABOUTME: [interp] do log (WOTM_INTERPLOG) a variacao do atraso da imagem (tools\interp_smooth.py). Sai com 0 se passar.
#
# Uso: powershell -File tools\interp_check.ps1 [-Secs 40] [-Match 3,1,1] [-Players 1] [-Hz 144]
# A interpolacao so liga com a apresentacao acima de 60 Hz (wotm_live.inc): rode num monitor de mais de 60 Hz (WOTM_MONITOR).
param([int]$Secs = 40, [string]$Match = '3,1,1', [int]$Players = 1, [int]$Hz = 144)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
$check = Join-Path $PSScriptRoot 'live_check.ps1'
$smooth = Join-Path $PSScriptRoot 'interp_smooth.py'
# com vsync (como se joga): cada apresentacao vai para um vblank do monitor; WOTM_HZ abaixo do monitor usa o limite por tempo
$base = "WOTM_MATCH=$Match;WOTM_AIVSAI=1;WOTM_INTERPLOG=1;WOTM_HZ=$Hz;WOTM_VSYNC=;PS2X_STATS=1;WOTM_INTERP=1"
if ($Players -ge 2) { $base += ";WOTM_PLAYERS=$Players;WOTM_LOCALPLAYER=1" }

& powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env $base -Secs $Secs -Expect '\[interp\] t=[\d.]+ g=\d+ fA='
if ($LASTEXITCODE -ne 0) { Write-Host 'FALHOU: sem linhas [interp] (a interpolacao nao ligou: monitor de 60 Hz?)'; exit 1 }
Copy-Item (Join-Path $runtime 'live_check.log') (Join-Path $runtime 'interp_on.log') -Force
& python $smooth (Join-Path $runtime 'interp_on.log') --max-ms 8
if ($LASTEXITCODE -ne 0) { Write-Host 'FALHOU: atraso da imagem varia demais com a interpolacao' -ForegroundColor Red; exit 1 }
Write-Host 'PASSOU'
exit 0
