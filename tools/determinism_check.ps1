# ABOUTME: Teste de determinismo para o netcode: roda a mesma luta duas vezes (a segunda com quadros lentos, WOTM_LOGICDELAY) no modo
# ABOUTME: deterministico (WOTM_FIXEDSTEP + WOTM_SEED) e compara o hash do estado de cada quadro (tools\statediff.py). Sai com 0 se iguais.
#
# Uso: powershell -File tools\determinism_check.ps1 [-Match 3,1,1] [-Players 2] [-Secs 60] [-Delay 40] [-Step 1]
#   -Match/-Players: a luta (WOTM_MATCH/WOTM_PLAYERS; IA no controle de todos). -Delay: atraso maximo em ms da segunda execucao.
#   Os hashes ficam em <runtime>\det_A.hash e det_B.hash; rode tools\statediff.py com --dumps para ver o que diverge.
param(
    [string]$Match = '3,1,1',
    [int]$Players = 2,
    [int]$Secs = 60,
    [int]$Delay = 40,
    [int]$Step = 1
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Pasta do runtime nao encontrada: rode run_live.bat uma vez ou defina WOTM_RECOMP_DIR." }

$base = "WOTM_MATCH=$Match;WOTM_PLAYERS=$Players;WOTM_AIVSAI=1;WOTM_NOINPUT=1;WOTM_FIXEDSTEP=$Step;WOTM_SEED=1234"
$runs = @(
    @{ Name = 'A'; Extra = 'WOTM_LOGICDELAY=' },
    @{ Name = 'B'; Extra = "WOTM_LOGICDELAY=$Delay" }
)
foreach ($r in $runs) {
    $hash = Join-Path $runtime "det_$($r.Name).hash"
    if (Test-Path $hash) { Remove-Item $hash -Force }
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'live_check.ps1') -Env "$base;$($r.Extra);WOTM_STATEHASH=$hash" -Secs $Secs -Expect '\[sync\] semente 1234' | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "execucao $($r.Name) falhou (veja $runtime\live_check.log)" -ForegroundColor Red; exit 1 }
}
& python (Join-Path $PSScriptRoot 'statediff.py') (Join-Path $runtime 'det_A.hash') (Join-Path $runtime 'det_B.hash')
if ($LASTEXITCODE -ne 0) { Write-Host 'FALHOU: o estado diverge' -ForegroundColor Red; exit 1 }
Write-Host 'PASSOU: estado identico em todos os quadros comparados'
exit 0
