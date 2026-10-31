# ABOUTME: Automated live game check: runs the runtime with the given variables for a few seconds and checks live.err.log.
# ABOUTME: Exits 0 when every -Expect pattern appears and no -Reject one does; used as a test that fails before the change.
#
# Usage: powershell -File tools\live_check.ps1 -Env "WOTM_MATCH=3,1,1;WOTM_AIVSAI=1" -Secs 30 -Expect "regex1|||regex2" [-Reject "regex"]
#   -Env: NAME=value pairs separated by ';' (values may contain commas). -Expect: regexes separated by '|||', all required.
#   -Reject: regexes separated by '|||' that must not appear (default: [CRASH] and [watchdog] FROZE).
#   -Exe test runs ps2EntryRunner.exe. The log is in <runtime>\live.err.log, with a copy in <runtime>\live_check.log.
param(
    [string]$Env = '',
    [int]$Secs = 30,
    [string]$Expect = '',
    [string]$Reject = '\[CRASH\]|||\[watchdog\] FROZE',
    [string]$Exe = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Runtime folder not found: run run_live.bat once or set WOTM_RECOMP_DIR." }

# check defaults (set before -Env, which may override them; "NAME=" clears one): no sound, a 720-line window on monitor 1 that
# does not cover the screen
if (-not $env:WOTM_MUTE) { $env:WOTM_MUTE = '1' }
if (-not $env:WOTM_FULLSCREEN) { $env:WOTM_FULLSCREEN = '0' }
if (-not $env:WOTM_MONITOR) { $env:WOTM_MONITOR = '1' }   # off the main monitor (raylib 0); on this PC: 1 = the left one
if (-not $env:WOTM_RES) { $env:WOTM_RES = '720' }
if (-not $env:WOTM_SETTINGS) { $env:WOTM_SETTINGS = '0' }   # this PC's settings.ini (in-game panel) does not affect the check
foreach ($pair in ($Env -split ';' | Where-Object { $_.Trim() })) {
    $i = $pair.IndexOf('=')
    if ($i -lt 1) { throw "Invalid pair in -Env: $pair" }
    [Environment]::SetEnvironmentVariable($pair.Substring(0, $i).Trim(), $pair.Substring($i + 1))
}
$env:WOTM_REPO = $repo

$log = Join-Path $runtime 'live.err.log'
if (Test-Path $log) { Remove-Item $log -Force }
$exeArg = if ($Exe) { " -Exe $Exe" } else { '' }
Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$runtime\run_live.ps1`" -Auto 0$exeArg" -WindowStyle Minimized | Out-Null
Start-Sleep -Seconds $Secs
foreach ($p in @(Get-Process wotm, ps2EntryRunner -ErrorAction SilentlyContinue)) { $p.CloseMainWindow() | Out-Null }
Start-Sleep -Seconds 3
Get-Process wotm, ps2EntryRunner -ErrorAction SilentlyContinue | Stop-Process -Force

$text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
Copy-Item $log (Join-Path $runtime 'live_check.log') -Force -ErrorAction SilentlyContinue
$failed = $false
foreach ($re in ($Expect -split '\|\|\|' | Where-Object { $_ })) {
    if ($text -match $re) { Write-Host "ok      $re" } else { Write-Host "MISSING $re" -ForegroundColor Red; $failed = $true }
}
foreach ($re in ($Reject -split '\|\|\|' | Where-Object { $_ })) {
    if ($text -match $re) { Write-Host "REJECTED $re : $($Matches[0])" -ForegroundColor Red; $failed = $true }
}
if ($failed) { Write-Host 'FAILED'; exit 1 }
Write-Host 'PASSED'
exit 0
