# ABOUTME: Launches the recompiled War of the Monsters (wotm.exe or the ps2EntryRunner.exe test build) from discroot.
# ABOUTME: Called by run_live.bat, the launcher and tools/lote.ps1; stderr goes to live.err.log in this folder.
param([int]$Auto = 1, [int]$Stats = 0, [string]$Build = 'build_dev', [string]$Exe = '')

$ErrorActionPreference = 'Stop'
$rt = $PSScriptRoot
$bin = Join-Path $rt "$Build\ps2xRuntime"
$game = Join-Path $bin 'wotm.exe'
$test = Join-Path $bin 'ps2EntryRunner.exe'

$useTest = ($Exe -eq 'test') -or $env:WOTM_VU1CHECK -or $env:WOTM_FOTOS -or -not (Test-Path $game)
$path = if ($useTest) { $test } else { $game }
if (-not (Test-Path $path)) { throw "Executable not found: $path (run build_runtime.bat)" }

$disc = Join-Path $rt 'discroot'
$elf = Join-Path $disc 'SCUS_971.97'
if ($Build -eq 'build_dev_nm') {
    $nm = Join-Path $env:WOTM_REPO 'recomp\nm\SCUS_971.97_nmall.elf'
    Copy-Item $nm $disc -Force
    $elf = Join-Path $disc 'SCUS_971.97_nmall.elf'
}
if (-not (Test-Path $elf)) { throw "ELF not found: $elf" }

# the game reads raw disc sectors (fileCdSearchFile walks ISO9660 from sector 0x10): it needs the ISO, not just the loose files
$setupIso = '@ISO@'   # the ISO that tools\setup_runtime.ps1 used
if (-not $env:PS2X_CD_IMAGE) {
    if (Test-Path -LiteralPath $setupIso) { $env:PS2X_CD_IMAGE = $setupIso }
    else {
        $repo = if ($env:WOTM_REPO) { $env:WOTM_REPO } else { Split-Path $rt -Parent }
        $iso = Get-ChildItem (Join-Path $repo 'ISO') -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $iso) { throw "No .iso in $repo\ISO (set PS2X_CD_IMAGE)" }
        $env:PS2X_CD_IMAGE = $iso.FullName
    }
}
# the repo's mods (mods\<mod>\) into discroot\mods\ (tools\sync_mods.ps1; docs/passos/MODS.md)
$repoDir = if ($env:WOTM_REPO) { $env:WOTM_REPO } else { Split-Path $rt -Parent | Join-Path -ChildPath 'WoTM' }
$sync = Join-Path $repoDir 'tools\sync_mods.ps1'
if (Test-Path $sync) { & $sync -Runtime $rt -Repo $repoDir }
$env:PS2X_NO_VU1 = '1'; $env:PS2X_NO_VIF1 = '1'; $env:PS2X_NO_GS = '1'
$env:PS2X_SCENE = '1'; $env:PS2X_FMV = '1'
if ($Stats -eq 1) { $env:PS2X_STATS = '1' }
if ($Auto -eq 1) {
    $script = Join-Path $rt 'pad_script.ps1'
    if (Test-Path $script) { . $script; $env:PS2X_PAD = Get-PadScript }
    else { Write-Host 'pad_script.ps1 does not exist: -Auto 1 runs without a script' -ForegroundColor Yellow; $env:PS2X_PAD = $null }
}   # -Auto 0: an inherited PS2X_PAD (checks) still applies

Write-Host "Running $path"
$proc = Start-Process -FilePath $path -ArgumentList "`"$elf`"" -WorkingDirectory $disc -PassThru `
    -RedirectStandardError (Join-Path $rt 'live.err.log') -RedirectStandardOutput (Join-Path $rt 'live.out.log')
$proc.WaitForExit()
exit $proc.ExitCode
