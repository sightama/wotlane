# ABOUTME: Checagem automatica do jogo ao vivo: roda o runtime com as variaveis dadas por alguns segundos e confere o live.err.log.
# ABOUTME: Sai com 0 quando todos os padroes -Expect aparecem e nenhum -Reject aparece; usado como teste que falha antes da mudanca.
#
# Uso: powershell -File tools\live_check.ps1 -Env "WOTM_MATCH=3,1,1;WOTM_AIVSAI=1" -Secs 30 -Expect "regex1|||regex2" [-Reject "regex"]
#   -Env: pares NOME=valor separados por ';' (os valores podem ter virgula). -Expect: regexes separadas por '|||', todas obrigatorias.
#   -Reject: regexes separadas por '|||' que nao podem aparecer (padrao: [CRASH] e [vigia] CONGELOU).
#   -Exe test roda o ps2EntryRunner.exe. O log fica em <runtime>\live.err.log e uma copia em <runtime>\live_check.log.
param(
    [string]$Env = '',
    [int]$Secs = 30,
    [string]$Expect = '',
    [string]$Reject = '\[CRASH\]|||\[vigia\] CONGELOU',
    [string]$Exe = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Pasta do runtime nao encontrada: rode run_live.bat uma vez ou defina WOTM_RECOMP_DIR." }

# padroes da checagem (antes de -Env, que pode troca-los; "NOME=" apaga): sem som, janela de 720 linhas que nao cobre a tela
if (-not $env:WOTM_MUTE) { $env:WOTM_MUTE = '1' }
if (-not $env:WOTM_FULLSCREEN) { $env:WOTM_FULLSCREEN = '0' }
if (-not $env:WOTM_RES) { $env:WOTM_RES = '720' }
foreach ($pair in ($Env -split ';' | Where-Object { $_.Trim() })) {
    $i = $pair.IndexOf('=')
    if ($i -lt 1) { throw "Par invalido em -Env: $pair" }
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
    if ($text -match $re) { Write-Host "ok      $re" } else { Write-Host "FALTOU  $re" -ForegroundColor Red; $failed = $true }
}
foreach ($re in ($Reject -split '\|\|\|' | Where-Object { $_ })) {
    if ($text -match $re) { Write-Host "PROIBIDO $re : $($Matches[0])" -ForegroundColor Red; $failed = $true }
}
if ($failed) { Write-Host 'FALHOU'; exit 1 }
Write-Host 'PASSOU'
exit 0
