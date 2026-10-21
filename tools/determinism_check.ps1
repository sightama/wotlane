# ABOUTME: Teste de determinismo para o netcode: roda a mesma luta duas vezes (a segunda com quadros lentos, WOTM_LOGICDELAY) no modo
# ABOUTME: deterministico (WOTM_FIXEDSTEP + WOTM_SEED) e compara o hash do estado de cada quadro (tools\statediff.py). Sai com 0 se iguais.
#
# Uso: powershell -File tools\determinism_check.ps1 [-Match 3,1,1] [-Players 2] [-Secs 60] [-Delay 40] [-Step 1]
#   -Match/-Players: a luta (WOTM_MATCH/WOTM_PLAYERS; IA no controle de todos). -Delay: atraso maximo em ms da segunda execucao.
#   Os hashes ficam em <runtime>\det_A.hash e det_B.hash; rode tools\statediff.py com --dumps para ver o que diverge.
#   -Replay: os jogadores ficam no pad (sem IA): a execucao A joga com um roteiro PS2X_PAD (por tempo) e grava a entrada de cada quadro
#     (WOTM_INPUTREC); a B, com quadros lentos, repete a gravacao (WOTM_INPUTPLAY). Testa a entrada por quadro do netcode.
param(
    [string]$Match = '3,1,1',
    [int]$Players = 2,
    [int]$Secs = 60,
    [int]$Delay = 40,
    [int]$Step = 1,
    [switch]$Replay
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Pasta do runtime nao encontrada: rode run_live.bat uma vez ou defina WOTM_RECOMP_DIR." }

$base = "WOTM_MATCH=$Match;WOTM_PLAYERS=$Players;WOTM_NOINPUT=1;WOTM_FIXEDSTEP=$Step;WOTM_SEED=1234"
if ($Replay) {
    $inputs = Join-Path $runtime 'det_inputs.bin'
    # andar, socar, pular e agarrar, para os jogadores 1 e 2 (segundos desde a abertura da janela; a luta comeca por volta de 15 s)
    $script = '18:6:up;20:1:square;22:1:cross;24:3:left;25:1:triangle;27:4:up@2;28:1:square@2;30:2:right@2;31:1:circle@2;33:5:down;34:1:square;36:1:triangle@2;38:4:left@2'
    $runs = @(   # o roteiro tem ';' e nao cabe no -Env: vai pela variavel de ambiente (Pad)
        @{ Name = 'A'; Pad = $script; Extra = "WOTM_LOGICDELAY=;WOTM_AIVSAI=;WOTM_INPUTREC=$inputs;WOTM_INPUTPLAY=" },
        @{ Name = 'B'; Pad = ''; Extra = "WOTM_LOGICDELAY=$Delay;WOTM_AIVSAI=;WOTM_INPUTREC=;WOTM_INPUTPLAY=$inputs" }
    )
} else {
    $runs = @(
        @{ Name = 'A'; Extra = 'WOTM_LOGICDELAY=;WOTM_AIVSAI=1' },
        @{ Name = 'B'; Extra = "WOTM_LOGICDELAY=$Delay;WOTM_AIVSAI=1" }
    )
}
foreach ($r in $runs) {
    $hash = Join-Path $runtime "det_$($r.Name).hash"
    if (Test-Path $hash) { Remove-Item $hash -Force }
    $env:PS2X_PAD = if ($r.Pad) { $r.Pad } else { $null }
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'live_check.ps1') -Env "$base;$($r.Extra);WOTM_STATEHASH=$hash" -Secs $Secs -Expect '\[sync\] semente 1234' | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "execucao $($r.Name) falhou (veja $runtime\live_check.log)" -ForegroundColor Red; exit 1 }
}
& python (Join-Path $PSScriptRoot 'statediff.py') (Join-Path $runtime 'det_A.hash') (Join-Path $runtime 'det_B.hash')
if ($LASTEXITCODE -ne 0) { Write-Host 'FALHOU: o estado diverge' -ForegroundColor Red; exit 1 }
Write-Host 'PASSOU: estado identico em todos os quadros comparados'
exit 0
