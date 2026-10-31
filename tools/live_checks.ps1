# ABOUTME: Checagens do jogo ao vivo (multiplayer de ate 4, tela, regressoes): cada uma roda o jogo com tools\live_check.ps1 e
# ABOUTME: confere o log. Uso: powershell -File tools\live_checks.ps1 [-Only <nome>]; sai com 1 se alguma falhar.
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'live_check.ps1'

$checks = [ordered]@{
    # os controles 3 e 4 (slot 1 do multitap nas portas 0 e 1) sao abertos e lidos pelo jogo; o 2 jogadores continua
    'controles_3_4' = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1'; Secs = 25
                         Expect = '\[pad\] player 1: |||\[pad\] player 3: |||\[pad\] player 4: |||\[match\]' }
    # partida com 4 jogadores (IA no controle): 4 modelos carregados, TheGame::m_numMonsters (0x3D3978) 4, m_monsters[0..3]
    # (0x3D3920..0x3D392C) preenchidos, 4 vistas (viewNumViews 0x6F8AE0) e 4 cameras (Cameras::m_numCameras 0x6F8E30). A apresentacao dos
    # monstros antes da luta fica numa vista so (wotm_split4.inc, cinemaViews) ate o quadro ~520 do jogo: 60 s para a amostra passar dela
    'partida_4'     = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=3D3978,3D3920,3D3924,3D3928,3D392C,6F8AE0,6F8E30'; Secs = 60
                         Expect = '\[native\] resources: CENTRAL \S+ \S+ \S+ \S+\r?\n|||3d3978=00000004|||3d3928=(?!00000000)[0-9a-f]{8}|||3d392c=(?!00000000)[0-9a-f]{8}|||6f8ae0=00000004|||6f8e30=00000004' }
    # jogadores derrubados voltam: a cada 8 s de jogo o "vencer" derruba os
    # jogadores 2..4 (e as IAs); se todos voltaram, a segunda vez derruba todos de novo (3 sem IA; antes, so o jogador 2 voltava). Com IA, o m_playerNum (+0x18) dos jogadores 3/4 (monstros em 0x2D6440 e 0x2E75D0, slots 2 e 3)
    # continua 1 (humano), nunca 2 (IA)
    'volta_4'       = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_CHEATS=vencer=8'; Secs = 50
                         Expect = 'win the fight: 3 [^\n]*\n(.|\n)*win the fight: 3 ' }
    'volta_4_ia'    = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=4;WOTM_CHEATS=vencer=8;PS2X_STATS=1;PS2X_PEEK=2D6458,2E75E8'; Secs = 50
                         Expect = 'win the fight: 4 [^\n]*\n(.|\n)*win the fight: 4 |||2d6458=00000001|||2e75e8=00000001'
                         Reject = '2d6458=00000002|||2e75e8=00000002|||\[CRASH\]|||\[watchdog\] FROZE' }
    # largada de 4 jogadores (wotm_split4.inc, startPoint): os 4 nascem em 4 lugares diferentes (a lista 1 do StartPoints tem 2 pontos
    # no Centro; sem a correcao, 3 nasciam no mesmo ponto). Sem IA e sem controle: ninguem morre, so as largadas aparecem
    'largada_4'     = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_SPLITLOG=1'; Secs = 30
                         Expect = 'spawn: m_monsters\[0\]|||spawn: m_monsters\[1\]|||spawn: m_monsters\[2\]|||spawn: m_monsters\[3\]'
                         Reject = 'spawn: m_monsters\[[0-3]\] \(playerNum \d\) at (\S+)\r?\n[\s\S]*?spawn: m_monsters\[[0-3]\] \(playerNum \d\) at \1\r?\n|||\[CRASH\]|||\[watchdog\] FROZE' }
    # motion blur com 4 vistas: o MotionBlurAA::init recebe as 4 (vistas em 0x6F8578; o bss dele tem lugar para 4, NATIVO.md, "Tela dividida de 3 e 4 jogadores")
    'borrao_4'      = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=6F8578'; Secs = 35
                         Expect = '6f8578=00000004'
                         Reject = '6f8578=00000002|||\[CRASH\]|||\[watchdog\] FROZE' }
    # meta de vitorias da partida direta (WOTM_KILLTARGET): o "vencer" derruba o jogador 2 aos 8 s, o jogador 1 chega a 1 vitoria e o
    # Evaluate abre o dialogo 5 (vencedor, PLAY AGAIN / REBUILD LEVEL / QUIT). Sem a meta (m_killTarget 0) o derrubado so levanta
    'meta_vitorias' = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_KILLTARGET=1;WOTM_CHEATS=vencer=8'; Secs = 40
                         Expect = 'win the fight: |||\[dialog\] 5 opened'
                         Reject = '\[CRASH\]|||\[watchdog\] FROZE' }
    # monstro sem arquivo no disco (6 = spider, cortado): a WOTM_MATCH e recusada com aviso, em vez de travar a carga da fase
    # (native/net/wotm_monsters.hpp)
    'monstro_sem_arquivo' = @{ Env = 'WOTM_MATCH=3,6,7,1;WOTM_AIVSAI=1'; Secs = 25
                         Expect = 'WOTM_MATCH rejected: monster 6 \(list item 2\) has no file on the disc' }
    # regressao: o 2 jogadores continua desenhando a tela dividida (redesenhos com 2 vistas) e nunca a tela em 4
    'tela_2'        = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1;PS2X_STATS=1'; Secs = 40
                         Expect = '2 views [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '[34] views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # tela em 4: a camada nativa captura as 4 vistas (linhas [split] da vista 3) e redesenha com 4 vistas (linha [native] redesenho);
    # a foto shot_30.png (discroot) fica para conferir os quadrantes a olho
    'tela_4'        = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;WOTM_SPLITLOG=1;WOTM_SHOTS=30'; Secs = 40
                         Expect = '\[split\] g=\d+ view 3:|||4 views [\d.]+ ms \(CPU [\d.]+\) x [1-9]' }
    # vista local (netcode, cada um no seu PC): com WOTM_LOCALPLAYER so a vista desse jogador, em tela cheia (redesenhos com 1 vista,
    # nunca os quadrantes); foto shot_30.png com a vista do jogador 3 ampliada do quadrante dele, HUD e texto do jogo incluidos
    'local_4'       = @{ Env = 'WOTM_MATCH=3,1,2,5,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;WOTM_LOCALPLAYER=3;PS2X_STATS=1;WOTM_SHOTS=30'; Secs = 40
                         Expect = 'redraw: 1 view [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '[34] views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # 2 jogadores com vista local: as duas vistas no viewport 0 (viewInfo[1].viewport em 0x6E191C = 0), sem a metade 2:3
    'local_2'       = @{ Env = 'WOTM_MATCH=3,1,1;WOTM_PLAYERS=2;WOTM_AIVSAI=1;WOTM_LOCALPLAYER=2;PS2X_STATS=1;PS2X_PEEK=6F8AE0,6E191C;WOTM_SHOTS=30'; Secs = 40
                         Expect = '6f8ae0=00000002 6e191c=00000000|||redraw: 1 view [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '2 views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # jogador 5 (numa IA guiada pelo controle 5) no seu PC: camera de perseguicao nativa atras do monstro dele, com a cena coletada
    # sem o corte das 4 vistas do jogo (centenas de objetos), e uma vista na tela; foto shot_30.png para conferir
    'local_5'       = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=5;WOTM_LOCALPLAYER=5;PS2X_STATS=1;WOTM_SHOTS=30'; Secs = 45
                         Expect = '\[chase\] player 5: camera [2-4]\d\d units from the monster, \d{3,} objects|||redraw: 1 view [\d.]+ ms \(CPU [\d.]+\) x [1-9]'
                         Reject = '[34] views [\d.]+ ms \(CPU [\d.]+\) x [1-9]|||\[CRASH\]|||\[watchdog\] FROZE' }
    # padrao sem WOTM_FULLSCREEN/WOTM_RES/WOTM_RENDERSCALE (opcoes do painel; settings.ini ignorado): janela de 720 linhas em 16:9 e o
    # quadro do mesmo tamanho; WOTM_RENDERSCALE=200 = quadro no dobro (supersampling), reduzido na apresentacao
    'tela_padrao'   = @{ Env = 'WOTM_FULLSCREEN=;WOTM_RES=;WOTM_RENDERSCALE=;WOTM_MATCH=3,1,1'; Secs = 25
                         Expect = '\[native\] frame 1280x720, window 1280x720 \(windowed' }
    'tela_escala'   = @{ Env = 'WOTM_FULLSCREEN=;WOTM_RES=;WOTM_RENDERSCALE=200;WOTM_MATCH=3,1,1'; Secs = 25
                         Expect = '\[native\] frame 2560x1440, window 1280x720 \(windowed' }
    # a vista unificada (todos numa tela so no combate de perto, gUseUnifiedView 0x6F8A8C) nunca liga com 3+ jogadores
    'unificada_4'   = @{ Env = 'WOTM_MATCH=3,1,2,5,4,1;WOTM_PLAYERS=4;WOTM_AIVSAI=1;PS2X_STATS=1;PS2X_PEEK=6F8A8C'; Secs = 70
                         Expect = '6f8a8c=00000000'
                         Reject = '6f8a8c=00000001|||\[CRASH\]|||\[watchdog\] FROZE' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    $reject = if ($c.Reject) { $c.Reject } else { '\[CRASH\]|||\[watchdog\] FROZE' }
    & powershell -NoProfile -ExecutionPolicy Bypass -File $check -Env $c.Env -Secs $c.Secs -Expect $c.Expect -Reject $reject
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}
if ($failed.Count) { Write-Host "Falharam: $($failed -join ', ')" -ForegroundColor Red; exit 1 }
Write-Host 'Todas passaram'
exit 0
