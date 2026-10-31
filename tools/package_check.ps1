# ABOUTME: Test of the friends package (tools\package_friends.ps1): builds it, checks it holds no disc file, runs the friend's setup
# ABOUTME: on a copy with this PC's ISO and plays a 2-player network match, the package's wotm.exe against the repo's.
#
# Usage: powershell -File tools\package_check.ps1 [-Iso <iso>] [-SkipNet]. Exits with 0 if everything passes.
param([string]$Iso = '', [switch]$SkipNet)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$fail = 0
function Check([bool]$ok, [string]$what) {
    if ($ok) { Write-Host "ok      $what" } else { Write-Host "FAILED  $what" -ForegroundColor Red; $script:fail++ }
}
if (-not $Iso) { $Iso = (Get-ChildItem (Join-Path $repo 'ISO') -Filter *.iso | Select-Object -First 1).FullName }

$pkg = & (Join-Path $PSScriptRoot 'package_friends.ps1') | Select-Object -Last 1
Check ($pkg -and (Test-Path $pkg)) "package built ($pkg)"
$zip = "$pkg.zip"
Check (Test-Path $zip) 'zip next to it'
$id = (Get-Content (Join-Path $repo '.wotm_recomp_dir') -TotalCount 1).Trim() | ForEach-Object { (Get-Content (Join-Path $_ 'build_dev\ps2xRuntime\wotm.buildid')).Trim() }
foreach ($f in 'WotM_Launcher.exe', 'Setup.bat', 'setup.ps1', 'README.txt', 'native\net\build\wotm_server.exe', 'tools\sync_mods.ps1',
               'runtime\run_live.template.ps1', 'runtime\build_dev\ps2xRuntime\wotm.exe', 'runtime\build_dev\ps2xRuntime\SDL2.dll',
               'runtime\build_dev\ps2xRuntime\vcruntime140.dll', 'runtime\build_dev\ps2xRuntime\msvcp140.dll') {
    Check (Test-Path (Join-Path $pkg $f)) "has $f"
}
Check ((Get-Content (Join-Path $pkg 'runtime\build_dev\ps2xRuntime\wotm.buildid')).Trim() -eq $id) "buildid $id"
$disco = @(Get-ChildItem $pkg -Recurse -File | Where-Object { $_.Name -match '^SCUS|\.(iso|elf|bin|rom|pdb|map)$' })
Check ($disco.Count -eq 0) "no disc file or symbol file ($($disco.Name -join ', '))"
foreach ($m in Get-ChildItem (Join-Path $repo 'mods') -Directory | Where-Object { -not $_.Name.StartsWith('_') -and -not $_.Name.StartsWith('.') }) {
    Check (Test-Path (Join-Path $pkg "mods\$($m.Name)")) "mod $($m.Name)"
}

# friend's setup on a copy (the package stays clean for sending)
$try = Join-Path $env:TEMP 'wotlane_pacote_teste'
if (Test-Path $try) { Remove-Item $try -Recurse -Force }
Copy-Item $pkg $try -Recurse
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $try 'setup.ps1') -Iso $Iso -NoPause | Out-Host
Check ($LASTEXITCODE -eq 0) 'setup exited with 0'
$elf = Join-Path $try 'runtime\discroot\SCUS_971.97'
Check ((Test-Path $elf) -and (Get-FileHash $elf -Algorithm SHA1).Hash -eq '1ED7F8457436A9170D8F85386E267369C7FCDD4F') 'ELF extracted from the ISO'
Check ((Get-Content (Join-Path $try 'runtime\run_live.ps1') -Raw).Contains("'$($Iso.Replace("'", "''"))'")) 'run_live.ps1 points at the ISO'
Check ((Get-Content (Join-Path $try '.wotm_recomp_dir') -TotalCount 1).Trim() -eq (Join-Path $try 'runtime')) '.wotm_recomp_dir'

if (-not $SkipNet) {
    $exes = (Join-Path ((Get-Content (Join-Path $repo '.wotm_recomp_dir') -TotalCount 1).Trim()) 'build_dev\ps2xRuntime\wotm.exe') + ',' +
            (Join-Path $try 'runtime\build_dev\ps2xRuntime\wotm.exe')
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'net_check.ps1') -Players 2 -Secs 60 -Exe $exes | Out-Host
    Check ($LASTEXITCODE -eq 0) 'network match: package wotm.exe in lockstep with the repo one'
}
Remove-Item $try -Recurse -Force -ErrorAction SilentlyContinue
if ($fail) { Write-Host "$fail failure(s)" -ForegroundColor Red; exit 1 }
Write-Host 'package ok'
exit 0
