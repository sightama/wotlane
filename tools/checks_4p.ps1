# ABOUTME: Checagens ao vivo do multiplayer de ate 4 jogadores (controles 3/4, partida com 4, tela em 4): cada uma roda o jogo
# ABOUTME: com tools\live_check.ps1 e confere o log. Uso: powershell -File tools\checks_4p.ps1 [-Only <nome>]; sai com 1 se alguma falhar.
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'live_check.ps1'

$checks = [ordered]@{
    # os controles 3 e 4 (slot 1 do multitap nas portas 0 e 1) sao abertos e lidos pelo jogo; o 2 jogadores continua
    'controles_3_4' = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1'; Secs = 25
                         Expect = '\[pad\] jogador 1: |||\[pad\] jogador 3: |||\[pad\] jogador 4: |||\[partida\]' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    & powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env $c.Env -Secs $c.Secs -Expect $c.Expect
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}
if ($failed.Count) { Write-Host "Falharam: $($failed -join ', ')" -ForegroundColor Red; exit 1 }
Write-Host 'Todas passaram'
exit 0
