# ABOUTME: Copia os mods do repo (mods\<mod>\) para a pasta que o jogo le (<runtime>\discroot\mods\<mod>\), espelhando cada
# ABOUTME: pasta do repo; mods que so existem no discroot (os seus, _work) ficam como estao. Chamado por run_live.ps1 e setup_runtime.ps1.
param([string]$Runtime = '', [string]$Repo = '')
$ErrorActionPreference = 'Stop'
if (-not $Repo) { $Repo = Split-Path $PSScriptRoot -Parent }
if (-not $Runtime) {
    $Runtime = $env:WOTM_RECOMP_DIR
    if (-not $Runtime -and (Test-Path (Join-Path $Repo '.wotm_recomp_dir'))) { $Runtime = (Get-Content (Join-Path $Repo '.wotm_recomp_dir') -Raw).Trim() }
}
if (-not $Runtime -or -not (Test-Path (Join-Path $Runtime 'discroot'))) { throw "Pasta do runtime (com discroot) nao encontrada: $Runtime" }
$src = Join-Path $Repo 'mods'
$dst = Join-Path $Runtime 'discroot\mods'
if (-not (Test-Path $src)) { exit 0 }
New-Item -ItemType Directory -Force $dst | Out-Null
foreach ($mod in Get-ChildItem $src -Directory) {
    if ($mod.Name.StartsWith('_') -or $mod.Name.StartsWith('.')) { continue }
    $out = & robocopy $mod.FullName (Join-Path $dst $mod.Name) /MIR /NJH /NJS /NDL /NP /NS /NC 2>&1
    if ($LASTEXITCODE -ge 8) { throw "robocopy falhou em $($mod.Name): $out" }
    $changed = @($out | Where-Object { $_ -match '\S' })
    if ($changed.Count) { Write-Host "[mods] $($mod.Name): $($changed.Count) arquivo(s) atualizado(s)" }
}
exit 0
