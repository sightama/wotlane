# ABOUTME: Teste do pacote para amigos (tools\package_friends.ps1): monta o pacote, confere que nao tem arquivo do disco, roda o setup
# ABOUTME: do amigo com a ISO deste PC numa copia e joga uma partida em rede de 2 com o wotm.exe do pacote contra o do repo.
#
# Uso: powershell -File tools\package_check.ps1 [-Iso <iso>] [-SkipNet]. Sai com 0 se tudo passar.
param([string]$Iso = '', [switch]$SkipNet)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$fail = 0
function Check([bool]$ok, [string]$what) {
    if ($ok) { Write-Host "ok      $what" } else { Write-Host "FALHOU  $what" -ForegroundColor Red; $script:fail++ }
}
if (-not $Iso) { $Iso = (Get-ChildItem (Join-Path $repo 'ISO') -Filter *.iso | Select-Object -First 1).FullName }

$pkg = & (Join-Path $PSScriptRoot 'package_friends.ps1') | Select-Object -Last 1
Check ($pkg -and (Test-Path $pkg)) "pacote montado ($pkg)"
$zip = "$pkg.zip"
Check (Test-Path $zip) 'zip ao lado'
$id = (Get-Content (Join-Path $repo '.wotm_recomp_dir') -TotalCount 1).Trim() | ForEach-Object { (Get-Content (Join-Path $_ 'build_dev\ps2xRuntime\wotm.buildid')).Trim() }
foreach ($f in 'WotM_Launcher.exe', 'Setup.bat', 'setup.ps1', 'README.txt', 'native\net\build\wotm_server.exe', 'tools\sync_mods.ps1',
               'runtime\run_live.template.ps1', 'runtime\build_dev\ps2xRuntime\wotm.exe', 'runtime\build_dev\ps2xRuntime\SDL2.dll',
               'runtime\build_dev\ps2xRuntime\vcruntime140.dll', 'runtime\build_dev\ps2xRuntime\msvcp140.dll') {
    Check (Test-Path (Join-Path $pkg $f)) "tem $f"
}
Check ((Get-Content (Join-Path $pkg 'runtime\build_dev\ps2xRuntime\wotm.buildid')).Trim() -eq $id) "buildid $id"
$disco = @(Get-ChildItem $pkg -Recurse -File | Where-Object { $_.Name -match '^SCUS|\.(iso|elf|bin|rom|pdb|map)$' })
Check ($disco.Count -eq 0) "nenhum arquivo do disco nem simbolo ($($disco.Name -join ', '))"
foreach ($m in Get-ChildItem (Join-Path $repo 'mods') -Directory | Where-Object { -not $_.Name.StartsWith('_') -and -not $_.Name.StartsWith('.') }) {
    Check (Test-Path (Join-Path $pkg "mods\$($m.Name)")) "mod $($m.Name)"
}

# setup do amigo numa copia (o pacote fica limpo para mandar)
$try = Join-Path $env:TEMP 'wotlane_pacote_teste'
if (Test-Path $try) { Remove-Item $try -Recurse -Force }
Copy-Item $pkg $try -Recurse
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $try 'setup.ps1') -Iso $Iso -NoPause | Out-Host
Check ($LASTEXITCODE -eq 0) 'setup saiu com 0'
$elf = Join-Path $try 'runtime\discroot\SCUS_971.97'
Check ((Test-Path $elf) -and (Get-FileHash $elf -Algorithm SHA1).Hash -eq '1ED7F8457436A9170D8F85386E267369C7FCDD4F') 'ELF tirado da ISO'
Check ((Get-Content (Join-Path $try 'runtime\run_live.ps1') -Raw).Contains("'$($Iso.Replace("'", "''"))'")) 'run_live.ps1 aponta para a ISO'
Check ((Get-Content (Join-Path $try '.wotm_recomp_dir') -TotalCount 1).Trim() -eq (Join-Path $try 'runtime')) '.wotm_recomp_dir'

if (-not $SkipNet) {
    $exes = (Join-Path ((Get-Content (Join-Path $repo '.wotm_recomp_dir') -TotalCount 1).Trim()) 'build_dev\ps2xRuntime\wotm.exe') + ',' +
            (Join-Path $try 'runtime\build_dev\ps2xRuntime\wotm.exe')
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'net_check.ps1') -Players 2 -Secs 60 -Exe $exes | Out-Host
    Check ($LASTEXITCODE -eq 0) 'partida em rede: wotm.exe do pacote em lockstep com o do repo'
}
Remove-Item $try -Recurse -Force -ErrorAction SilentlyContinue
if ($fail) { Write-Host "$fail falha(s)" -ForegroundColor Red; exit 1 }
Write-Host 'pacote ok'
exit 0
