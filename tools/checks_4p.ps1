# ABOUTME: Checagens ao vivo do multiplayer de ate 4 jogadores (controles 3/4, partida com 4, tela em 4): cada uma roda o jogo
# ABOUTME: com tools\live_check.ps1 e confere o log. Uso: powershell -File tools\checks_4p.ps1 [-Only <nome>]; sai com 1 se alguma falhar.
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'live_check.ps1'

$checks = [ordered]@{
    # os controles 3 e 4 (slot 1 do multitap nas portas 0 e 1) sao abertos e lidos pelo jogo; o 2 jogadores continua
    'controles_3_4' = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1'; Secs = 25
                         Expect = '\[pad\] jogador 1: |||\[pad\] jogador 3: |||\[pad\] jogador 4: |||\[partida\]' }
    # partida com 4 jogadores (IA no controle): 4 modelos carregados, TheGame::m_numMonsters (0x3D3978) 4, m_monsters[0..3]
    # (0x3D3920..0x3D392C) preenchidos, 4 vistas (viewNumViews 0x6F8AE0) e 4 cameras (Cameras::m_numCameras 0x6F8E30)
    'partida_4'     = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=3D3978,3D3920,3D3924,3D3928,3D392C,6F8AE0,6F8E30'; Secs = 35
                         Expect = '\[native\] recursos: CENTRAL \S+ \S+ \S+ \S+\r?\n|||3d3978=00000004|||3d3928=(?!00000000)[0-9a-f]{8}|||3d392c=(?!00000000)[0-9a-f]{8}|||6f8ae0=00000004|||6f8e30=00000004' }
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
