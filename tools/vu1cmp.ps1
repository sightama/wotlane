# VU1 vs native check without the game, from snapshots (.vu1snap) written by the runtime with WOTM_VU1CHECK_SNAP=<prefix>.
# Runs `ps2EntryRunner --vu1cmp` (wotm_vu1check.inc) once for all files and prints each one's [vu1check] lines.
#   tools\vu1cmp.ps1 <file.vu1snap | folder> [-Frame <g_frame>] [-Dump <prefix>] [-Full] [-Build build_dev]
#   A file holds the frames of one session (trigger/burst: WOTM_VU1CHECK_WHEN, WOTM_VU1CHECK_BURST); all of them are compared.
#   -Frame <g_frame>: only that frame of the file
#   -Full: every [vu1check] line (default: each frame's summary lines, objects/attributes/particles/sprites, and the whole
#          inventory, "inv" lines: what only the reference or only native draws, per owner; see docs/passos/VU1CHECK.md)
#   -Dump <prefix>: writes each check's triangles to <prefix>_<g_frame>.csv (same as WOTM_VU1CHECK_DUMP)
#   -NoPng: no images (default: <file>_<g_frame>_{ref,nat,diff,trio}.png next to the snapshot; ref = the runtime's software
#           GS, an approximation of the PS2 GS; nat = native layer; diff = differing pixels; trio = all three side by side)
# The runtime folder comes from WOTM_RECOMP_DIR or from .wotm_recomp_dir at the repo root (written by build_runtime.bat).
param(
    [Parameter(Mandatory = $true)][string]$Path,
    [int]$Frame = 0,
    [string]$Dump = '',
    [switch]$Full,
    [switch]$NoPng,
    [string]$Build = 'build_dev'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\$Build\ps2xRuntime\ps2EntryRunner.exe")) {
    Write-Error "Runtime not found ($Build\ps2xRuntime\ps2EntryRunner.exe) in '$runtime': set WOTM_RECOMP_DIR or run build_runtime.bat once."
}
$exe = "$runtime\$Build\ps2xRuntime\ps2EntryRunner.exe"
$files = if (Test-Path $Path -PathType Container) { Get-ChildItem $Path -Filter '*.vu1snap' | Sort-Object { (($_.BaseName -split '_')[-1]) -as [int] }, Name | ForEach-Object { $_.FullName } } else { @((Resolve-Path $Path).Path) }
if (-not $files) { Write-Error "No .vu1snap in $Path" }
$err = Join-Path $env:TEMP 'wotm_vu1cmp.err.log'
$out = Join-Path $env:TEMP 'wotm_vu1cmp.out.log'
$rc = 0
# A single run for all files (launching the runtime cost more than the comparison); each file's output starts at "[vu1cmp] file <path>"
$args = '--vu1cmp ' + (($files | ForEach-Object { "`"$_`"" }) -join ' ')
if ($Frame -gt 0) { $args += " --frame $Frame" }
if ($Dump) { $args += " --dump `"$Dump`"" }
if ($NoPng) { $args += " --nopng" }
# Start-Process redirects without going through PowerShell (PS 5's `2>` writes UTF-16 and breaks long lines)
$p = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory "$runtime\discroot" -NoNewWindow -Wait -PassThru -RedirectStandardError $err -RedirectStandardOutput $out
$all = @(Get-Content $err)
$blocks = [ordered]@{}
foreach ($f in $files) { $blocks[$f] = New-Object System.Collections.Generic.List[string] }
$curBlock = $null
foreach ($l in $all) {
    if ($l -match '^\[vu1cmp\] file (.+)$') { $curBlock = $blocks[$Matches[1]]; continue }
    if ($null -ne $curBlock) { $curBlock.Add($l) }
}
if ($p.ExitCode -ne 0 -and -not ($all -match '^\[vu1cmp\] file ')) {   # the runtime did not even start
    $all | ForEach-Object { Write-Host $_ -ForegroundColor Yellow }
    exit 1
}
foreach ($f in $files) {
    $lines = $blocks[$f]
    Write-Host ("== {0}" -f (Split-Path $f -Leaf)) -ForegroundColor Cyan
    if (-not ($lines -match ': [1-9]\d* frame\(s\) compared$')) {   # the file failed (did not open, other build, requested frame missing)
        $rc = 1
        $lines | Where-Object { $_ -match '\[vu1cmp\]' } | ForEach-Object { Write-Host $_ -ForegroundColor Yellow }
        continue
    }
    # default: each frame's summaries and the whole inventory ("inv" lines: what only one side draws, per owner)
    $sel = if ($Full) { $lines | Where-Object { $_ -match '\[vu1check\]|\[vu1cmp\] (images|no GS frame|the native layer)' } } else { $lines | Where-Object { $_ -match '\[vu1check\] (g=\d+ |inv )|\[vu1cmp\] (images|no GS frame|the native layer)' } }
    $sel | ForEach-Object { $_ -replace '^\[vu1check\] ', '' }
}
exit $rc
