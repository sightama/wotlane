# ABOUTME: Checagens da partida em rede (perfil fixo da rede, dialogos em lockstep): cada uma roda tools\net_check.ps1 com 2 jogadores
# ABOUTME: e confere estado igual em todo quadro e o log. Uso: powershell -File tools\net_checks.ps1 [-Only <nome>]; sai com 1 se alguma falhar.
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot 'net_check.ps1'

$checks = [ordered]@{
    # mouse so num PC (WOTM_ORBITMOUSE: deslocamento como o do mouse, a partir do quadro 300 da orbita): a camera moderna o levava a
    # camera do jogo do jogador 1 so naquele PC e a partida dessincronizava (bloco 0x800000, o _cs da camera). Na rede: camera classica
    'camera_mouse'  = @{ PlayerEnv = '2:WOTM_ORBITMOUSE=300:600:4:0'; Secs = 50
                         Expect = 'partida em rede: perfil fixo' }
    # opcoes de jogo diferentes em cada PC (camera classica num, moderna no outro: com a moderna o controle e relativo a tela; lock-on
    # alternado, sensibilidade e inversao da camera): o perfil fixo vale nos dois
    'opcoes_pc'     = @{ PlayerEnv = '1:WOTM_CAMERA=0;WOTM_CAMSENS=10;WOTM_CAMINVERT=1|||2:WOTM_CAMERA=1;WOTM_LOCKTOGGLE=1'; Secs = 50 }
    # modo foto num PC so (WOTM_PHOTO: liga sozinho 25 s depois da primeira cena): pausava aquele PC e soltava a camera; na rede nao liga
    'foto'          = @{ PlayerEnv = '2:WOTM_PHOTO=25'; Secs = 50
                         Reject = '\[photo\] ligado|||\[photo\] teste automatico' }
    # pausa pelo jogador 1, X no RECOMPILED OPTIONS: o painel le o teclado e o mouse de cada PC (fora do controle da rede) e fecharia
    # em quadros diferentes; na rede a linha nao abre o painel e a pausa segue em lockstep (o Start de novo continua a luta)
    # (toques curtos: segurado, o menu repete e passa da linha)
    'painel_pausa'  = @{ PlayerEnv = '1:PS2X_PAD=24:0.3:start;26:0.2:down;27:0.2:down;28:0.3:cross;31:0.3:start;33:3:up;37:1:square'; Secs = 50
                         Reject = '\[pausa\] painel aberto' }
    # meta de 1 vitoria: o "vencer" (pelo g_frame, igual nos dois) derruba o jogador 2, o dialogo 5 abre nos dois PCs e o X do jogador 1
    # escolhe PLAY AGAIN (o cursor comeca nele; o dialogo aceita o direcional de qualquer jogador, entao o 2 fica quieto): a rodada 2
    # comeca e termina igual nos dois. O laco do dialogo anda em lockstep (sem isso o indice do quadro parava e o menu nao andava)
    'meta_rede'     = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 60
                         Expect = '\[dialogo\] 5 aberto|||rodada 2 terminou' }
    # REBUILD LEVEL no dialogo 5 (baixo, X): o ResetLevel refaz a fase sem recarregar; a rodada 2 joga e termina igual nos dois
    'rebuild_rede'  = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.2:down;35:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 60
                         Expect = '\[dialogo\] 5 aberto|||rodada 2 terminou' }
    # QUIT no dialogo 5 (baixo, baixo, X; no ARE YOU SURE o NO vem marcado: cima, X): fora da rede o jogo volta ao userintMain e a
    # partida direta recomecaria (segunda carga da fase, que dessincroniza); na rede a partida acaba e o jogo fecha em todos os PCs
    # (o launcher volta ao lobby)
    'sair_rede'     = @{ Wins = 1; ExtraEnv = 'WOTM_CHEATS=vencer=8'; PlayerEnv = '1:PS2X_PAD=34:0.2:down;35:0.2:down;36:0.3:cross;38:0.2:up;39:0.3:cross|||2:PS2X_PAD=12:0.3:square'; Secs = 55
                         Expect = '\[net\] partida encerrada pelo QUIT'; Reject = 'segunda carga de fase' }
    # mods diferentes (o jogador 2 sem os mods do repo): o digest dos mods entra no hash da configuracao e o servidor recusa
    'mods_diferentes' = @{ PlayerEnv = '2:WOTM_MODS=0'; Secs = 30; ExpectRefused = 'configuracao do jogo diferente' }
}

$failed = @()
foreach ($name in $checks.Keys) {
    if ($Only -and $name -ne $Only) { continue }
    $c = $checks[$name]
    Write-Host "== $name"
    $a = @('-NoProfile', '-File', $check, '-Players', '2', '-Secs', $c.Secs)
    foreach ($p in 'PlayerEnv', 'ExtraEnv', 'Expect', 'Reject', 'ExpectRefused', 'Wins') { if ($c[$p]) { $a += @("-$p", $c[$p]) } }
    & powershell @a
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}
if ($failed) { Write-Host "FALHARAM: $($failed -join ', ')"; exit 1 }
Write-Host 'todas passaram'
exit 0
