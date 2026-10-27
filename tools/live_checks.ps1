# ABOUTME: Checagens do jogo ao vivo (multiplayer de ate 4, tela, regressoes): cada uma roda o jogo com tools\live_check.ps1 e
# ABOUTME: confere o log. Uso: powershell -File tools\live_checks.ps1 [-Only <nome>]; sai com 1 se alguma falhar.
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
    # jogadores derrubados voltam: a cada 8 s de jogo (16 s reais: com 3+ jogadores o jogo roda a 30 quadros/s) o "vencer" derruba os
    # jogadores 2..4 (e as IAs); se todos voltaram, a segunda vez derruba todos de novo (3 sem IA; antes, so o jogador 2 voltava). Com IA, o m_playerNum (+0x18) dos jogadores 3/4 (monstros em 0x2D6440 e 0x2E75D0, slots 2 e 3)
    # continua 1 (humano), nunca 2 (IA)
    'volta_4'       = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_CHEATS=vencer=8'; Secs = 50
                         Expect = 'vencer a luta: 3 [^\n]*\n(.|\n)*vencer a luta: 3 ' }
    'volta_4_ia'    = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=4;WOTM_CHEATS=vencer=8;PS2X_STATS=1;PS2X_PEEK=2D6458,2E75E8'; Secs = 50
                         Expect = 'vencer a luta: 4 [^\n]*\n(.|\n)*vencer a luta: 4 |||2d6458=00000001|||2e75e8=00000001'
                         Reject = '2d6458=00000002|||2e75e8=00000002|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # largada de 4 jogadores (wotm_split4.inc, startPoint): os 4 nascem em 4 lugares diferentes (a lista 1 do StartPoints tem 2 pontos
    # no Centro; sem a correcao, 3 nasciam no mesmo ponto). Sem IA e sem controle: ninguem morre, so as largadas aparecem
    'largada_4'     = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_SPLITLOG=1'; Secs = 30
                         Expect = 'largada: m_monsters\[0\]|||largada: m_monsters\[1\]|||largada: m_monsters\[2\]|||largada: m_monsters\[3\]'
                         Reject = 'largada: m_monsters\[[0-3]\] \(playerNum \d\) em (\S+)\r?\n[\s\S]*?largada: m_monsters\[[0-3]\] \(playerNum \d\) em \1\r?\n|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # motion blur com 4 vistas: o MotionBlurAA::init recebe as 4 (vistas em 0x6F8578; o bss dele tem lugar para 4, POS_FRONTEIRA.md item 11)
    'borrao_4'      = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=6F8578'; Secs = 35
                         Expect = '6f8578=00000004'
                         Reject = '6f8578=00000002|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # regressao: o 2 jogadores continua desenhando a tela dividida (redesenhos com 2 vistas) e nunca a tela em 4
    'tela_2'        = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1;PS2X_STATS=1'; Secs = 40
                         Expect = '2 vistas [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '3-4 vistas [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # tela em 4: a camada nativa captura as 4 vistas (linhas [split] da vista 3) e redesenha com 4 vistas (linha [native] redesenho);
    # a foto shot_30.png (discroot) fica para conferir os quadrantes a olho
    'tela_4'        = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;WOTM_SPLITLOG=1;WOTM_SHOTS=30'; Secs = 40
                         Expect = '\[split\] g=\d+ vista 3:|||4 vistas [\d.]+ ms \(CPU [\d.]+\) x [1-9]' }
    # vista local (netcode, cada um no seu PC): com WOTM_LOCALPLAYER so a vista desse jogador, em tela cheia (redesenhos com 1 vista,
    # nunca os quadrantes); foto shot_30.png com a vista do jogador 3 ampliada do quadrante dele, HUD e texto do jogo incluidos
    'local_4'       = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;WOTM_LOCALPLAYER=3;PS2X_STATS=1;WOTM_SHOTS=30'; Secs = 40
                         Expect = 'redesenho: 1 vista [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '3-4 vistas [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # 2 jogadores com vista local: as duas vistas no viewport 0 (viewInfo[1].viewport em 0x6E191C = 0), sem a metade 2:3
    'local_2'       = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1;WOTM_LOCALPLAYER=2;PS2X_STATS=1;PS2X_PEEK=6F8AE0,6E191C;WOTM_SHOTS=30'; Secs = 40
                         Expect = '6f8ae0=00000002 6e191c=00000000|||redesenho: 1 vista [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '2 vistas [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # jogador 5 (numa IA guiada pelo controle 5) no seu PC: camera de perseguicao nativa atras do monstro dele, com a cena coletada
    # sem o corte das 4 vistas do jogo (centenas de objetos), e uma vista na tela; foto shot_30.png para conferir
    'local_5'       = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=5;WOTM_LOCALPLAYER=5;PS2X_STATS=1;WOTM_SHOTS=30'; Secs = 45
                         Expect = '\[chase\] jogador 5: camera a [2-4]\d\d unidades do monstro, \d{3,} objetos|||redesenho: 1 vista [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '3-4 vistas [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[vigia\] CONGELOU' }
    # padrao sem WOTM_FULLSCREEN/WOTM_RES/WOTM_SSAA: tela cheia sem borda, quadro no dobro da resolucao do monitor (superamostragem 2x;
    # esta maquina: 2560x1440) e reduzido na apresentacao; WOTM_SSAA=1 = quadro na resolucao do monitor
    'tela_nativa'   = @{ Env = 'WOTM_FULLSCREEN=;WOTM_RES=;WOTM_SSAA=;WOTM_MATCH=3,1,1'; Secs = 25
                         Expect = '\[native\] quadro 5120x2880, janela 2560x1440 \(tela cheia' }
    'tela_sem_ssaa' = @{ Env = 'WOTM_FULLSCREEN=;WOTM_RES=;WOTM_SSAA=1;WOTM_MATCH=3,1,1'; Secs = 25
                         Expect = '\[native\] quadro 2560x1440, janela 2560x1440 \(tela cheia' }
    # a vista unificada (todos numa tela so no combate de perto, gUseUnifiedView 0x6F8A8C) nunca liga com 3+ jogadores
    'unificada_4'   = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=6F8A8C'; Secs = 70
                         Expect = '6f8a8c=00000000'
                         Reject = '6f8a8c=00000001|||\[CRASH\]|||\[vigia\] CONGELOU' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    $reject = if ($c.Reject) { $c.Reject } else { '\[CRASH\]|||\[vigia\] CONGELOU' }
    & powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env $c.Env -Secs $c.Secs -Expect $c.Expect -Reject $reject
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}
if ($failed.Count) { Write-Host "Falharam: $($failed -join ', ')" -ForegroundColor Red; exit 1 }
Write-Host 'Todas passaram'
exit 0
