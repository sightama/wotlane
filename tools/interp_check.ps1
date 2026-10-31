# ABOUTME: Frame interpolation check: an AI fight on a display above 60 Hz with interpolation on, measuring image delay jitter from
# ABOUTME: the log's [interp] lines (WOTM_INTERPLOG) with tools\interp_smooth.py. Exits 0 on pass.
#
# Usage: powershell -File tools\interp_check.ps1 [-Secs 40] [-Match 3,1,1] [-Players 1] [-Hz 144]
# Interpolation only turns on when presenting above 60 Hz (wotm_live.inc): run on a monitor faster than 60 Hz (WOTM_MONITOR).
param([int]$Secs = 40, [string]$Match = '3,1,1', [int]$Players = 1, [int]$Hz = 144)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
$check = Join-Path $PSScriptRoot 'live_check.ps1'
$smooth = Join-Path $PSScriptRoot 'interp_smooth.py'
# with vsync (as people play): each present goes to a monitor vblank; WOTM_HZ below the monitor rate uses the timed limiter
$base = "WOTM_MATCH=$Match;WOTM_AIVSAI=1;WOTM_INTERPLOG=1;WOTM_HZ=$Hz;WOTM_VSYNC=;PS2X_STATS=1;WOTM_INTERP=1"
if ($Players -ge 2) { $base += ";WOTM_PLAYERS=$Players;WOTM_LOCALPLAYER=1" }

& powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env $base -Secs $Secs -Expect '\[interp\] t=[\d.]+ g=\d+ fA='
if ($LASTEXITCODE -ne 0) { Write-Host 'FAILED: no [interp] lines (interpolation did not turn on: 60 Hz monitor?)'; exit 1 }
Copy-Item (Join-Path $runtime 'live_check.log') (Join-Path $runtime 'interp_on.log') -Force
& python $smooth (Join-Path $runtime 'interp_on.log') --max-ms 8
if ($LASTEXITCODE -ne 0) { Write-Host 'FAILED: image delay varies too much with interpolation' -ForegroundColor Red; exit 1 }
Write-Host 'PASSED'
exit 0
