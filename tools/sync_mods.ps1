# ABOUTME: Copies the repo's mods (mods\<mod>\) to the folder the game reads (<runtime>\discroot\mods\<mod>\), mirroring each
# ABOUTME: repo folder; mods that only exist in discroot (yours, _work) are left alone. Called by run_live.ps1 and setup_runtime.ps1.
param([string]$Runtime = '', [string]$Repo = '')
$ErrorActionPreference = 'Stop'
if (-not $Repo) { $Repo = Split-Path $PSScriptRoot -Parent }
if (-not $Runtime) {
    $Runtime = $env:WOTM_RECOMP_DIR
    if (-not $Runtime -and (Test-Path (Join-Path $Repo '.wotm_recomp_dir'))) { $Runtime = (Get-Content (Join-Path $Repo '.wotm_recomp_dir') -Raw).Trim() }
}
if (-not $Runtime -or -not (Test-Path (Join-Path $Runtime 'discroot'))) { throw "Runtime folder (with discroot) not found: $Runtime" }
$src = Join-Path $Repo 'mods'
$dst = Join-Path $Runtime 'discroot\mods'
if (-not (Test-Path $src)) { exit 0 }
New-Item -ItemType Directory -Force $dst | Out-Null
foreach ($mod in Get-ChildItem $src -Directory) {
    if ($mod.Name.StartsWith('_') -or $mod.Name.StartsWith('.')) { continue }
    $out = & robocopy $mod.FullName (Join-Path $dst $mod.Name) /MIR /NJH /NJS /NDL /NP /NS /NC 2>&1
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed on $($mod.Name): $out" }
    $changed = @($out | Where-Object { $_ -match '\S' })
    if ($changed.Count) { Write-Host "[mods] $($mod.Name): $($changed.Count) file(s) updated" }
}
exit 0
