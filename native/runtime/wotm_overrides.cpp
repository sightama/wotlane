// Correcoes especificas de War of the Monsters (SCUS_971.97) aplicadas por substituicao de funcoes do codigo recompilado.
//
// Limite do updateRate: `timerGetUpdateRate` (0x2236D8) devolve D_006F8DB8, quantos quadros por segundo o ultimo quadro
// do jogo "valeu" (60 = 1 vsync). A fisica integra por Euler com `dt = 1 / rate` (mola do pelo em BoneSpring::update,
// giro dos monstros em MonsterDynamics::updatePosition, veiculos, detritos...). No retail o rate e 60 ou 30; neste
// runtime o jogo roda a ~20 quadros logicos/s e o rate oscila quadro a quadro entre 60 e 8, ou seja, dt ate 7x maior
// que o previsto. Com esse dt os integradores ficam instaveis: o pelo do Congar estica pela cena toda e o controle de giro
// (amortecimento ~ -40.5 * velRot, estavel so com dt < 2/40.5, rate >= 21) inverte e amplifica a velocidade a cada quadro
// (monstro girando sozinho, a camera atras dele gira junto).
// A correcao devolve no minimo WOTM_RATEMIN (padrao 30; 0 desliga) para o jogo inteiro: nos quadros lentos o jogo anda
// um pouco mais devagar em vez de pular, e todo integrador fica na margem para a qual foi feito. Nada muda quando o rate
// ja e >= 30 (retail). Substitui os limites pontuais anteriores (WOTM_SPRINGMIN na mola, WOTM_TURNMIN no giro).
#include "ps2_runtime.h"
#include "wotm_syms.h"   // wotm::sym / wotm::fn: enderecos do ELF por nome (tools/native_syms.py --gen)
#include "ps2_runtime_macros.h"
#include "runtime/ee_scheduler.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern std::atomic<unsigned long long> g_wotmIdleUs, g_wotmPaceUs;
extern bool g_wotmCycleProf;
extern uint64_t g_wotmEeCycle, g_wotmIdleCycles;
extern uint32_t g_wotmProfPc[4096];
extern uint64_t g_wotmProfCyc[4096];
extern uint32_t g_wotmFreeSpinPc;
void wotmCaptureFrame(const uint8_t *rdram);   // wotm_live.inc: copia o quadro pronto para o desenho nativo
void wotmCaptureScene(const uint8_t *rdram);   // wotm_live.inc: cena (objetos, paletas, camera) na saida do hier(0, 0)
void wotmScreenRect(const uint8_t *rdram, uint32_t pkt, bool queued);   // wotm_live.inc: fade do quadro, lido de um pacote GIF recem-montado
void wotmLetterbox(const uint8_t *rdram);   // wotm_live.inc: faixas pretas do menu, lidas do pacote do letterboxDmaData
void wotmGsPktQueueSent();   // wotm_live.inc: a fila do gsPkt foi despachada (DmaGsPktQueue)
void wotmFontSent();         // wotm_live.inc: o texto do quadro foi despachado (fontDmaFontData)
void wotmShellFade(int start, int step, bool accumulate);
void wotmLoadFreeze(bool on);   // wotm_live.inc: carga da fase (LoadLevelFiles ate o rtMain), a tela repete o ultimo quadro
bool wotmPauseBlock(PS2Runtime *rt);   // wotm_foto.inc: F7/F6, segura a thread do EE enquanto pausado
bool wotmPauseTookFrame();             // wotm_foto.inc: o quadro que acabou teve parada (pausa ou foto)
void wotmPickupHighlights(const uint8_t *rdram);   // wotm_live.inc: colorQuad dos pick-ups destacados, na saida do enableHighlights
void wotmScreenPolys(const uint8_t *rdram);   // wotm_live.inc: HUD do ScreenPolys, lido na entrada do dmaData   // wotm_live.inc: Shell::FadeScreen rodando (a apresentacao le o pacote dele ao vivo)

// Trapacas de debug: a janela (menu F1, wotm_live.inc) escreve, a thread do EE (monsterUpdateCheats) le. Bits em wotm_cheats.h.
#include "wotm_cheats.h"
std::atomic<uint32_t> g_wotmCheats{0};
std::atomic<bool> g_wotmCheatWin{false};

// Camera livre de debug (F5): a janela (wotm_freecam.inc) move a pose, a thread do EE a poe no _cs da vista 0 (freeCamWrite).
#include "wotm_freecam.h"
std::atomic<bool> g_wotmFreeCam{false};
std::atomic<bool> g_wotmFreeCamPad{false};
std::mutex g_wotmFreeCamM;
WotmFreeCamPose g_wotmFreeCamPose;

namespace
{
    constexpr uint32_t kTimerGetUpdateRate = wotm::fn::timerGetUpdateRate__Fv;
    constexpr uint32_t kUpdateRateVar = 0x6F8DB8u;   // D_006F8DB8, lido pelo gp

    int g_minRate = 30;
    unsigned long long g_clamped = 0;

    // Fields do quadro = vblanks que passaram desde o inicio dele (relogio do WotM, g_wotmFreeSpinPc != 0).
    // O jogo mede o quadro pelo timer 1: timer1Handler soma ceil(T1 / 9619) ao contador de fields (D_006F8DBC) e espera o
    // vsync; timerWaitForMinUpdateRate tira fields = D_006F8DBC - D_006F8DC0 e updateRate = 60 / fields. No PS2 o quadro
    // comeca logo apos um vblank, entao "a logica cruzou n vblanks" e "T1 >= n fields" sao a mesma coisa. Aqui o timer anda
    // pelo relogio virtual e o quadro comeca alguns ms (de host) depois do vblank: a logica cruza um vblank com T1 < 1 field,
    // o jogo conta 1 field num quadro de 2 vsyncs e anda mais devagar que o relogio (18% dos quadros, ~11% de lentidao
    // na luta). A correcao: na entrada de timerWaitForMinUpdateRate o contador passa a ser no minimo os vblanks desde o
    // inicio do quadro (marcado pelo timerGetCurTics no fim de timerWaitForMinUpdateRate / em timerSetFrameStart).
    constexpr uint32_t kTimerWaitForMinUpdateRate = wotm::fn::timerWaitForMinUpdateRate__FPUi;
    constexpr uint32_t kTimerGetCurTics = wotm::fn::timerGetCurTics__Fv;
    constexpr uint32_t kFrameStartRa[2] = {0x22369Cu, 0x223480u};   // retorno do jal em timerWaitForMinUpdateRate / timerSetFrameStart
    constexpr uint32_t kFieldCounter = 0x6F8DBCu, kFieldCounterAtStart = 0x6F8DC0u;
    PS2Runtime::RecompiledFunction g_origWaitMinRate = nullptr, g_origGetCurTics = nullptr;
    uint64_t g_frameStartVsync = 0;
    bool g_frameStartValid = false;

    void getCurTicsMarked(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = GPR_U32(ctx, 31);
        g_origGetCurTics(rdram, ctx, runtime);
        if (ra == kFrameStartRa[0] || ra == kFrameStartRa[1])
        {
            g_frameStartVsync = runtime->eeScheduler().currentVSyncTick();
            g_frameStartValid = true;
        }
    }


    // Partida direta, WOTM_MATCH="<monstro do jogador>,<IA>[,<IA>[,<IA>]],<fase>" (indices de MonsterLongNames e GameLevelNames;
    // "3,1,1" = lizard contra monkey em central; "3,1,2,5,1" = lizard contra monkey, robot e mantis): userintMain (0x1D9868) nao abre
    // o menu e devolve 0 (comecar a sessao) com o shell como o menu o deixa em "1 jogador contra a CPU" (medido com dumps no fim do
    // menu do roteiro -Auto): desafio (m_mode 2), 1 jogador + 1 a 3 IAs (m_monsterSel[4 + i], m_costume[2 + i]), 3 vidas; monstro
    // repetido ganha o proximo figurino (o menu nao deixa dois iguais no mesmo figurino). A diferenca e m_killTarget 0 (o menu poe 1): sem
    // meta de vitorias, EvaluateOnePlayerChallengeStatus so reergue o monstro derrubado e a luta segue, sem o dialogo "PLAY AGAIN"
    // que esperaria o pad. O filme de abertura (uiMain, 0x1D93D8) vira no-op. Cada fim de rodada (entrada de
    // Shell::EvaluateGameStatus, 0x1A7960) sai no log como [partida] com a vida e as vitorias dos dois monstros.
    constexpr uint32_t kUserintMain = wotm::fn::userintMain__Fv, kUiMain = wotm::fn::uiMain__Fi, kEvaluateGameStatus = wotm::fn::EvaluateGameStatus__5Shelli;
    PS2Runtime::RecompiledFunction g_origEvaluate = nullptr;
    unsigned g_rounds = 0;
    constexpr uint32_t kShell = wotm::sym::shellInfo, kGame = wotm::sym::shellGame;   // shellInfo, shellGame
    int g_matchMonsters[4] = {-1, -1, -1, -1};   // jogador, depois as IAs
    int g_matchAIs = 0, g_matchLevel = -1;
    unsigned g_matchCount = 0;
    bool g_aiVsAi = false;

    void userintMainDirect(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        for (uint32_t i = 0; i < 8; ++i)
        {
            WRITE32(kShell + 0x2938 + 4 * i, 0);                         // m_monsterSel
            WRITE32(kShell + 0x2BE0 + 4 * i, 0);                         // m_costume
        }
        for (int i = 0; i <= g_matchAIs; ++i)
        {
            uint32_t costume = 0;
            for (int j = 0; j < i; ++j)
                costume += g_matchMonsters[j] == g_matchMonsters[i];
            const uint32_t slot = i ? 4u + (i - 1) : 0u, costumeSlot = i ? 2u + (i - 1) : 0u;
            WRITE32(kShell + 0x2938 + 4 * slot, static_cast<uint32_t>(g_matchMonsters[i] << 5));   // (monstro << 5) | variante
            WRITE32(kShell + 0x2BE0 + 4 * costumeSlot, costume & 3u);
        }
        WRITE32(kShell + 0x2A44, 0);                                      // m_killTarget
        WRITE32(kShell + 0x2BB0, static_cast<uint32_t>(g_matchLevel));   // m_levelNum
        WRITE32(kShell + 0x2BB4, 1);                                      // m_numPlayers
        WRITE32(kShell + 0x2BB8, static_cast<uint32_t>(g_matchAIs));     // m_numAIs
        for (uint32_t i = 0; i < 4; ++i)
            WRITE32(kShell + 0x2BBC + 4 * i, 3);                         // m_lives
        WRITE32(kShell + 0x2C10, 2);                                      // m_mode: free for all
        WRITE32(kGame + 0x1203C8, 2);                                     // TheGame::m_gameMode
        std::fprintf(stderr, "[partida] %u: monstro %d (jogador%s) contra", ++g_matchCount, g_matchMonsters[0], g_aiVsAi ? " sob IA" : "");
        for (int i = 1; i <= g_matchAIs; ++i)
            std::fprintf(stderr, " %d", g_matchMonsters[i]);
        std::fprintf(stderr, " (IA), fase %d\n", g_matchLevel);
        SET_GPR_S32(ctx, 2, 0);
        ctx->pc = GPR_U32(ctx, 31);
    }

    void uiMainSkipped(uint8_t *, R5900Context *ctx, PS2Runtime *) { ctx->pc = GPR_U32(ctx, 31); }

    void evaluateGameStatusLogged(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kEvaluateGameStatus)
        {
            std::fprintf(stderr, "[partida] rodada %u terminou (rtMain devolveu %d):", ++g_rounds, static_cast<int32_t>(GPR_U32(ctx, 5)));
            for (uint32_t slot : {0u, 4u, 5u, 6u})   // m_monsters[0] (jogador) e m_monsters[4..] (IAs)
            {
                const uint32_t m = READ32(kGame + 0x120380 + 4 * slot);
                if (!m)
                    continue;
                const uint32_t hw = READ32(m + 0x44C);   // m_health
                float health;
                std::memcpy(&health, &hw, 4);
                std::fprintf(stderr, " monstro %u vida %.0f vitorias %d;", READ32(m + 0x14) >> 5, health, static_cast<int32_t>(READ32(m + 0x3C)));
            }
            std::fprintf(stderr, "\n");
        }
        g_origEvaluate(rdram, ctx, runtime);
    }

    // WOTM_AIVSAI=1: o jogador tambem e controlado pela IA do jogo. Monster::playerInit (0x165F10) salta para Monster::aiInit
    // (0x166038) com os mesmos registradores, como um `j`: m_playerNum fica 2 e Monster::update le as entradas de Ai::updateInputs.
    // Camera e HUD seguem no jogador (SetPlayerMonster). Retomada no meio cai direto no aiInit (o salto nao volta para ca).
    // TheGame::SetAIMonster (0x137B10) recalcula m_numAIs (0x1203E0) contando os slots com m_playerNum 2, o que agora inclui o
    // jogador; m_monsters[4 + i] seria lido alem da ultima IA (bancos de som, pickups). Depois dele, tira da conta os jogadores.
    constexpr uint32_t kPlayerInit = wotm::fn::playerInit__7Monster, kAiInit = wotm::fn::aiInit__7Monster, kSetAIMonster = wotm::fn::SetAIMonster__7TheGameiiii;
    PS2Runtime::RecompiledFunction g_aiInit = nullptr, g_origSetAIMonster = nullptr;

    void playerInitAsAi(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        ctx->pc = kAiInit;
        g_aiInit(rdram, ctx, runtime);
    }

    void setAIMonsterCounted(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kSetAIMonster;
        g_origSetAIMonster(rdram, ctx, runtime);
        if (!entry)
            return;
        const int32_t players = static_cast<int32_t>(READ32(kShell + 0x2BB4));
        int32_t n = static_cast<int32_t>(READ32(kGame + 0x1203E0));
        for (int32_t i = 0; i < players && i < 4; ++i)
        {
            const uint32_t m = READ32(kGame + 0x120380 + 4 * i);   // m_monsters[i]
            if (m && READ32(m + 0x18) == 2u)                       // m_playerNum
                --n;
        }
        WRITE32(kGame + 0x1203E0, static_cast<uint32_t>(n < 0 ? 0 : n));
    }

    // Fora do controle, o jogo ainda trata os slots de jogador como humanos onde o m_playerNum decide algo que nao e entrada:
    // TheGame::gameGetStartPoint (0x138D68; com m_playerNum 2 o jogador nascia num ponto da IA e a IA no do jogador) e
    // Cameras::Update (0x11C770; so chama Camera::checkCollision, que monta os pontos de visada dos predios "fantasma", para
    // camera de monstro com m_playerNum 1). Durante essas duas funcoes os jogadores voltam a 1. Nenhuma das duas espera.
    constexpr uint32_t kGameGetStartPoint = wotm::fn::gameGetStartPoint__7TheGameP7Monster, kCamerasUpdate = wotm::fn::Update__7Cameras;
    PS2Runtime::RecompiledFunction g_origStartPoint = nullptr, g_origCamerasUpdate = nullptr;

    void callAsHumans(PS2Runtime::RecompiledFunction orig, uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        uint32_t flipped[4];
        int n = 0;
        const int32_t players = static_cast<int32_t>(READ32(kShell + 0x2BB4));
        for (int32_t i = 0; i < players && i < 4; ++i)
        {
            const uint32_t m = READ32(kGame + 0x120380 + 4 * i);   // m_monsters[i]
            if (m && READ32(m + 0x18) == 2u)
            {
                WRITE32(m + 0x18, 1u);
                flipped[n++] = m;
            }
        }
        orig(rdram, ctx, runtime);
        while (n > 0)
            WRITE32(flipped[--n] + 0x18, 2u);
    }

    void startPointAsHumans(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { callAsHumans(g_origStartPoint, rdram, ctx, runtime); }
    void camerasUpdateAsHumans(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { callAsHumans(g_origCamerasUpdate, rdram, ctx, runtime); }

    // Free for all de 1 jogador sem IA (mod, sempre ligado): screenFreeForAllOptions1P (tela 12) anda o cursor so por 1..3 IAs, mas
    // o retail ja trata o item 0 na confirmacao (m_numAIs 0, pula o "first to" e vai direto a escolha do monstro, tela 3, que sem IA
    // segue para a fase; na luta EvaluateOnePlayerChallengeStatus so reergue o jogador). Aqui o cursor passa a dar a volta por 0..3:
    // depois da original, cima de 1 (que ia para 3) e baixo de 3 (que ia para 1) param no 0. O switch do menu (shell +0x43C) recebe
    // o mesmo valor, como o hierSetSwitch faria (whichChild, se o switch tiver esse filho).
    constexpr uint32_t kFfaOptions1P = wotm::fn::screenFreeForAllOptions1P__Fv;
    constexpr uint32_t kFfaCursor = wotm::sym::currentSelection + 12 * 4;   // currentSelection[12]
    PS2Runtime::RecompiledFunction g_origFfaOptions1P = nullptr;
    uint32_t g_ffaRa = 0, g_ffaBefore = 0;
    bool g_ffaPending = false;

    void ffaOptions1PWithZero(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kFfaOptions1P)
        {
            g_ffaRa = GPR_U32(ctx, 31);
            g_ffaBefore = READ32(kFfaCursor);
            g_ffaPending = true;
        }
        g_origFfaOptions1P(rdram, ctx, runtime);
        if (!g_ffaPending || ctx->pc != g_ffaRa)
            return;   // parou no meio (retomada cai aqui de novo e termina o servico)
        g_ffaPending = false;
        const uint32_t after = READ32(kFfaCursor);
        if (!((g_ffaBefore == 1 && after == 3) || (g_ffaBefore == 3 && after == 1)))
            return;
        WRITE32(kFfaCursor, 0);
        const uint32_t sw = READ32(kShell + 0x43C);
        if (sw && (READ8(sw) & 0x3F) == 6 && READ8(sw + 0xB) > 0)   // SWITCH_NODE, numKids
            WRITE8(sw + 8, 0);                                        // whichChild
    }

    // Trapacas de debug (menu F1 do wotm_live.inc, ou WOTM_CHEATS no inicio): aplicadas na thread do EE, na entrada de Monster::update
    // (0x15B488) de cada jogador (m_monsters[0 .. m_numPlayers-1]), antes da logica do quadro dele. A janela so escreve g_wotmCheats.
    //   invencivel: Monster::takeDamage (0x15FE40) volta na entrada quando a vitima e um jogador (nem o dano forcado passa).
    //   stamina:    1 = cheia (cur = max 100, sem exaustao); 2 = no nivel de furia (cur = maxLevel, 130).
    //   especial:   m_specialWeapon (+0xF3) = 1, a liberacao que o drainSpecial tira e o isSpecialAvailable le.
    //   cloak:      sem CLOAK (m_cloaked, +0xEF), chama Monster::setCloakOn (0x162CD0), como o power-up; acabou o tempo, liga de novo.
    //   deus:       m_godMode (+0xE9), o modo do proprio jogo: sem dano, vida, stamina e especial cheios a cada quadro.
    //   vencer:     acao de uma vez: takeDamage(100000, force, jogador) em cada IA viva (m_monsters[4 + i], i < m_numAIs).
    // WOTM_CLOAK=1 (teste do CLOAK na checagem VU1) e o mesmo que WOTM_CHEATS=cloak.
    constexpr uint32_t kMonsterUpdate = wotm::fn::update__7Monster, kSetCloakOn = wotm::fn::setCloakOn__7Monster,
                       kTakeDamage = wotm::fn::takeDamage__7MonsterfbP7Monster;
    PS2Runtime::RecompiledFunction g_origMonsterUpdate = nullptr, g_setCloakOn = nullptr, g_origTakeDamage = nullptr;
    uint32_t g_godApplied = 0;
    int g_autoWinSecs = 0;   // WOTM_CHEATS=vencer=N   // bits dos slots de jogador em que o m_godMode foi ligado por aqui (para desligar so esses)

    bool isPlayerMonster(uint8_t *rdram, uint32_t m, int *slot = nullptr)
    {
        int32_t players = static_cast<int32_t>(FAST_READ32(kShell + 0x2BB4));   // m_numPlayers
        if (players < 1 || players > 4)
            players = 1;
        for (int32_t i = 0; i < players; ++i)
            if (m && FAST_READ32(kGame + 0x120380 + 4 * i) == m)
            {
                if (slot)
                    *slot = i;
                return true;
            }
        return false;
    }

    void callTakeDamage(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t victim, float damage, uint32_t source)
    {
        const R5900Context saved = *ctx;
        ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(victim)));
        ctx->r[5] = _mm_set_epi64x(0, 1);   // force
        ctx->r[6] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(source)));
        ctx->r[31] = _mm_set_epi64x(0, 0);
        ctx->f[12] = damage;
        ctx->pc = kTakeDamage;
        g_origTakeDamage(rdram, ctx, runtime);
        *ctx = saved;
    }

    void takeDamageCheat(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kTakeDamage && (g_wotmCheats.load(std::memory_order_relaxed) & kCheatInvincible) && isPlayerMonster(rdram, GPR_U32(ctx, 4)))
        {
            static uint32_t blocked = 0;
            if (++blocked <= 3 || blocked % 100 == 0)
                std::fprintf(stderr, "[debug] invencivel: dano bloqueado no jogador (%u)\n", blocked);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origTakeDamage(rdram, ctx, runtime);
    }

    // Modo deus sem reacao de golpe: no jogador com kCheatGod, Monster::takeHit (0x15F048, a entrada virtual de todo golpe),
    // knockBack (0x1601C8) e takeAdditiveRecoil (0x160058) voltam na entrada, e o enterNewState (0x1621C8) recusa os estados de
    // reacao, como o proprio jogo faz no modo 7 (empurroes de explosao e colisao chamam knockBack ou entram no estado por fora).
    constexpr uint32_t kTakeHit = wotm::fn::takeHit__7MonsterP8_fvectorfi, kKnockBack = wotm::fn::knockBack__7MonsterR8_fvectorff,
                       kAdditiveRecoil = wotm::fn::takeAdditiveRecoil__7MonsterR8_fvectorf, kEnterNewState = wotm::fn::enterNewState__7MonsterP12MonsterState;
    PS2Runtime::RecompiledFunction g_origTakeHit = nullptr, g_origKnockBack = nullptr, g_origAdditiveRecoil = nullptr, g_origEnterNewState = nullptr;
    // offsets dos estados de reacao dentro do Monster (game.h): Recoil, KnockBack, Grappled, BeingSlammed, BeingThrown, Impaled,
    // Stunned, Shocked, BigTakeHit, Countered
    constexpr uint32_t kHitStates[] = {0x7E30, 0x7EA0, 0xDDCC, 0xDE30, 0xE3E0, 0x106E0, 0x10714, 0x10BA0, 0x11100, 0x11114};

    bool godPlayer(uint8_t *rdram, uint32_t m)
    {
        return (g_wotmCheats.load(std::memory_order_relaxed) & kCheatGod) && isPlayerMonster(rdram, m);
    }

    void godBlocked(const char *what)
    {
        static uint32_t n = 0;
        if (++n <= 3 || n % 100 == 0)
            std::fprintf(stderr, "[debug] deus: %s recusado no jogador (%u)\n", what, n);
    }

    void takeHitCheat(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kTakeHit && godPlayer(rdram, GPR_U32(ctx, 4)))
        {
            godBlocked("takeHit");
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origTakeHit(rdram, ctx, runtime);
    }

    void knockBackCheat(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kKnockBack && godPlayer(rdram, GPR_U32(ctx, 4)))
        {
            godBlocked("knockBack");
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origKnockBack(rdram, ctx, runtime);
    }

    void additiveRecoilCheat(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kAdditiveRecoil && godPlayer(rdram, GPR_U32(ctx, 4)))
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origAdditiveRecoil(rdram, ctx, runtime);
    }

    void enterNewStateCheat(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        if (ctx->pc == kEnterNewState && godPlayer(rdram, m))
        {
            const uint32_t off = GPR_U32(ctx, 5) - m;
            for (uint32_t s : kHitStates)
                if (off == s)
                {
                    char what[48];
                    std::snprintf(what, sizeof what, "estado +0x%X", s);
                    godBlocked(what);
                    ctx->pc = GPR_U32(ctx, 31);
                    return;
                }
        }
        g_origEnterNewState(rdram, ctx, runtime);
    }

    void monsterUpdateCheats(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        int slot = 0;
        if (ctx->pc == kMonsterUpdate && isPlayerMonster(rdram, m, &slot))
        {
            const uint32_t c = g_wotmCheats.load(std::memory_order_relaxed);
            const uint32_t st = (c & kCheatStaminaMask) / kCheatStamina1;
            if (st == 1u)
            {
                WRITE32(m + 0x470, READ32(m + 0x460));   // StaminaMeter (+0x460): cur = max
                WRITE32(m + 0x474, 0);                    // recoverPool
                WRITE32(m + 0x478, 0);                    // exhaustTimer
                WRITE32(m + 0x480, 0);                    // exhausted
            }
            else if (st == 2u)
            {
                WRITE32(m + 0x470, READ32(m + 0x468));   // cur = maxLevel (furia)
                WRITE32(m + 0x474, 0);
                WRITE32(m + 0x478, 0);
                WRITE32(m + 0x480, 0);
            }
            if (c & kCheatSpecial)
                WRITE8(m + 0xF3, 1);   // m_specialWeapon
            const uint32_t bit = 1u << slot;
            if (c & kCheatGod)
            {
                WRITE8(m + 0xE9, 1);   // m_godMode
                g_godApplied |= bit;
            }
            else if (g_godApplied & bit)
            {
                WRITE8(m + 0xE9, 0);
                g_godApplied &= ~bit;
            }
            if ((c & kCheatCloak) && g_setCloakOn && !READ8(m + 0xEF))   // m_cloaked
            {
                const R5900Context saved = *ctx;
                ctx->pc = kSetCloakOn;
                g_setCloakOn(rdram, ctx, runtime);
                *ctx = saved;
            }
            if (slot == 0 && g_autoWinSecs > 0)
            {   // WOTM_CHEATS=vencer=N: a cada N segundos de jogo (60 quadros do g_frame por segundo), o mesmo pedido da tecla 6
                static uint32_t last = 0;
                const uint32_t f = READ32(wotm::sym::g_frame);
                if (last == 0 || f < last)
                    last = f;
                else if (f - last >= static_cast<uint32_t>(g_autoWinSecs) * 60u)
                {
                    last = f;
                    g_wotmCheatWin = true;
                }
            }
            if (slot == 0 && g_wotmCheatWin.exchange(false))
            {
                const int32_t ais = static_cast<int32_t>(READ32(kGame + 0x1203E0));   // m_numAIs
                int killed = 0;
                for (int32_t i = 0; i < ais && i < 4; ++i)
                {
                    const uint32_t ai = READ32(kGame + 0x120390 + 4 * i);   // m_monsters[4 + i]
                    if (ai && !READ8(ai + 0xE8))                           // m_dead
                    {
                        callTakeDamage(rdram, ctx, runtime, ai, 100000.0f, m);
                        ++killed;
                    }
                }
                std::fprintf(stderr, "[debug] vencer a luta: %d IA(s) derrubada(s) de %d\n", killed, ais);
            }
        }
        g_origMonsterUpdate(rdram, ctx, runtime);
    }

    // WOTM_TURNLOG: uma linha [turn] por MonsterDynamics::updatePosition (cada monstro, cada quadro do jogo):
    // quadro (g_frame), monstro, rate, acelRot (+0x110, zerada pela funcao), velRot (+0x100) antes/depois e os angulos
    // (Monster+0xC -> +0x60, em radianos) depois da integracao. O rate mostrado e o da variavel (sem o limite).
    constexpr uint32_t kMonsterDynamicsUpdatePosition = wotm::fn::updatePosition__15MonsterDynamics;
    PS2Runtime::RecompiledFunction g_origUpdatePosition = nullptr;

    void updatePositionLogged(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        auto readF = [&](uint8_t *, uint32_t addr)
        {
            const uint32_t w = READ32(addr);
            float f;
            std::memcpy(&f, &w, 4);
            return f;
        };
        const uint32_t dyn = GPR_U32(ctx, 4);
        float acc[3], v0[3];
        for (int i = 0; i < 3; ++i)
        {
            acc[i] = readF(rdram, dyn + 0x110 + 4 * i);
            v0[i] = readF(rdram, dyn + 0x100 + 4 * i);
        }
        const int32_t rate = static_cast<int32_t>(READ32(kUpdateRateVar));
        g_origUpdatePosition(rdram, ctx, runtime);
        const uint32_t mon = READ32(dyn);
        const uint32_t cs = READ32(mon + 0xC);
        std::fprintf(stderr, "[turn] g=%u mon=%06x rate=%d acc=%+.3f,%+.3f,%+.3f vel=%+.3f,%+.3f,%+.3f -> %+.3f,%+.3f,%+.3f ang=%+.3f,%+.3f,%+.3f\n",
                     READ32(wotm::sym::g_frame), mon, rate, acc[0], acc[1], acc[2], v0[0], v0[1], v0[2], readF(rdram, dyn + 0x100), readF(rdram, dyn + 0x104),
                     readF(rdram, dyn + 0x108), readF(rdram, cs + 0x60), readF(rdram, cs + 0x64), readF(rdram, cs + 0x68));
    }

    // WOTM_FIELDLOG: uma linha [field] por quadro do jogo, detectado quando timerWaitForMinUpdateRate atualiza
    // D_006F8DC0 (contagem de fields no inicio do quadro). Mostra os fields que o jogo mediu pelo timer 1 (D_006F8DB4),
    // os ciclos virtuais do EE gastos (em fields: total, saltados no ocio e os 3 pcs de checkpoint que mais gastaram) e o
    // tempo real (total, ocioso esperando evento e esperando o relogio de parede para soltar um prazo).
    bool g_fieldLog = false;
    // Chamadas a sceGsSyncV no quadro (cada uma espera 1 vsync) e o $ra das 4 primeiras: mostra quem gasta os fields.
    constexpr uint32_t kSceGsSyncV = wotm::fn::sceGsSyncV;
    PS2Runtime::RecompiledFunction g_origSyncV = nullptr;
    uint32_t g_syncVCalls = 0, g_syncVRa[4];
    // Laco do displayDialog (menu de pause, areYouSure, fim de luta do multiplayer...): monta o quadro sozinho (hier, particleDraw,
    // Hud::print, a funcao do dialogo, fontDmaFontData) e espera o vsync no sceGsSyncV do fim do laco, sem passar pelo
    // timerWaitForMinUpdateRate. Sem captura ali, a tela repetia o ultimo quadro de antes do dialogo, e a moldura do pause (o _cs
    // shell->m_cs610, que o pauseLevelOverlay liga) nunca aparecia. $ra desse jal: displayDialog + 0x7C4 (jal em 0x1ACB9C).
    // Na ultima volta ($s1 != 0, o beqz logo depois volta ao laco com 0) o Shell::FadeScreen acabou de escurecer a tela: capturar
    // ali desligaria o fade (wotmCaptureFrame) e mostraria a cena clara ate o proximo quadro. Fica sem captura, como antes.
    constexpr uint32_t kDialogLoopSyncVRa = wotm::fn::displayDialog__Fi + 0x7C4u;
    void syncVTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kSceGsSyncV)
        {
            const uint32_t ra = GPR_U32(ctx, 31);
            if (g_fieldLog)
            {
                if (g_syncVCalls < 4)
                    g_syncVRa[g_syncVCalls] = ra;
                ++g_syncVCalls;
            }
            if (ra == kDialogLoopSyncVRa && GPR_U32(ctx, 17) == 0u)
                wotmCaptureFrame(rdram);
        }
        g_origSyncV(rdram, ctx, runtime);
    }
    uint32_t g_lastFieldStart = 0;
    uint64_t g_lastCycle = 0, g_lastIdleUs = 0, g_lastPaceUs = 0;
    std::chrono::steady_clock::time_point g_lastHost;

    void fieldLogFrame(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t start = READ32(0x6F8DC0u);
        if (start == g_lastFieldStart)
            return;
        g_lastFieldStart = start;
        const auto now = std::chrono::steady_clock::now();
        const uint64_t idleUs = g_wotmIdleUs.load(std::memory_order_relaxed), paceUs = g_wotmPaceUs.load(std::memory_order_relaxed);
        int top[3] = {-1, -1, -1};
        for (int i = 0; i < 4096; ++i)
        {
            if (!g_wotmProfCyc[i])
                continue;
            for (int k = 0; k < 3; ++k)
            {
                if (top[k] < 0 || g_wotmProfCyc[i] > g_wotmProfCyc[top[k]])
                {
                    for (int m = 2; m > k; --m)
                        top[m] = top[m - 1];
                    top[k] = i;
                    break;
                }
            }
        }
        const double field = 294912000.0 / 59.94;
        static uint32_t lastTics = 0;   // D_006F8DC4: ticks do timer 1 que o jogo acumulou (9619 por field)
        const uint32_t tics = READ32(0x6F8DC4u);
        std::fprintf(stderr, "[field] g=%u fields=%d rate=%d tics=%.2f ee=%.2f idle=%.2f host=%.1fms ocioso=%.1fms ritmo=%.1fms top",
                     READ32(wotm::sym::g_frame), static_cast<int32_t>(READ32(0x6F8DB4u)), static_cast<int32_t>(READ32(kUpdateRateVar)),
                     (tics - lastTics) / 9619.0,
                     (g_wotmEeCycle - g_lastCycle) / field, g_wotmIdleCycles / field,
                     std::chrono::duration<double, std::milli>(now - g_lastHost).count(), (idleUs - g_lastIdleUs) / 1000.0,
                     (paceUs - g_lastPaceUs) / 1000.0);
        for (int k = 0; k < 3; ++k)
            if (top[k] >= 0)
                std::fprintf(stderr, " %06x=%.2f", g_wotmProfPc[top[k]], g_wotmProfCyc[top[k]] / field);
        std::fprintf(stderr, " syncv=%u", g_syncVCalls);
        for (uint32_t k = 0; k < g_syncVCalls && k < 4; ++k)
            std::fprintf(stderr, "%c%06x", k ? ',' : ':', g_syncVRa[k]);
        g_syncVCalls = 0;
        std::fprintf(stderr, "\n");
        std::memset(g_wotmProfPc, 0, sizeof(g_wotmProfPc));
        std::memset(g_wotmProfCyc, 0, sizeof(g_wotmProfCyc));
        g_wotmIdleCycles = 0;
        g_lastCycle = g_wotmEeCycle;
        g_lastHost = now;
        g_lastIdleUs = idleUs;
        g_lastPaceUs = paceUs;
        lastTics = tics;
    }

    void waitMinRateByVblanks(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        // so na entrada: retomada no meio (depois de um sceGsSyncV do laco) ja tem D_006F8DC0 atualizado
        if (ctx->pc == kTimerWaitForMinUpdateRate)
        {
            // [field] do quadro anterior: aqui roda em todo quadro (timerGetUpdateRate nao: no menu e no inicio da fase
            // ha quadros sem chamada, e a linha somava dois quadros)
            if (g_fieldLog)
                fieldLogFrame(rdram, ctx, runtime);
            // quadro pronto (update, hier, particleDraw) e o seguinte ainda nao comecou: o desenho nativo copia a cena aqui
            wotmCaptureFrame(rdram);
            // pausa (F7/F6/F8, wotm_foto.inc): o quadro pronto fica na tela e o seguinte nao comeca. O quadro com parada conta 1 field:
            // o tempo parado nao vira dt (sem isso o updateRate caia ao limite e o jogo dava um passo de 1/30 na volta)
            wotmPauseBlock(runtime);
            if (wotmPauseTookFrame())
                WRITE32(kFieldCounter, READ32(kFieldCounterAtStart) + 1u);
            else if (g_wotmFreeSpinPc != 0u && g_frameStartValid)
            {
                const uint64_t vblanks = runtime->eeScheduler().currentVSyncTick() - g_frameStartVsync;
                const int32_t start = static_cast<int32_t>(READ32(kFieldCounterAtStart));
                const int32_t measured = static_cast<int32_t>(READ32(kFieldCounter)) - start;
                if (vblanks > 0u && vblanks < 16u && static_cast<int32_t>(vblanks) > measured)
                    WRITE32(kFieldCounter, static_cast<uint32_t>(start + static_cast<int32_t>(vblanks)));
            }
        }
        g_origWaitMinRate(rdram, ctx, runtime);
    }

    // Fades para o desenho nativo (wotm_live.inc, Scene::drawScreenRects): o jogo escurece a tela com um SPRITE do GS montado na hora.
    // rtFadeScreen (entrada da luta) e TheGame::updateFadeOut/updateFadeOutAndIn (fim de rodada, cinemas) o poem na fila do gsPkt
    // (AddCallToQueue, a1 = pacote): o tap le cada pacote enfileirado, e o leitor so aceita o formato do fade. O uiFadeScreen (menus)
    // monta na pilha (0xB0 abaixo do sp de entrada) e manda direto: lido depois de voltar. O Shell::FadeScreen e um laco que espera
    // vsyncs: marca na entrada; o pacote fica em D_00731540. Argumentos: a2 = b (redesenha a cena a cada passo), e na pilha de entrada
    // alfa inicial (+0), final (+8) e passo (+0x10), bytes.
    constexpr uint32_t kAddCallToQueue = wotm::fn::AddCallToQueue__5GsPktP6QwData, kUiFadeScreen = wotm::fn::uiFadeScreen__FUi,
                       kShellFadeScreen = wotm::fn::FadeScreen__5ShellibRUiT3UcUcUcUcUcUc;
    PS2Runtime::RecompiledFunction g_origAddCall = nullptr, g_origUiFade = nullptr, g_origShellFade = nullptr;

    void addCallToQueueTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kAddCallToQueue)
            wotmScreenRect(rdram, GPR_U32(ctx, 5), true);
        g_origAddCall(rdram, ctx, runtime);
    }

    // Ordem fila x texto: o rtMain manda o texto (fontDmaFontData) e depois a fila do gsPkt (DmaGsPktQueue), e o fade cobre o texto;
    // o ShellRtLoop::endField (laco dos dialogos de fim de fase, wonLevelOverlay e cia.) manda a fila antes do texto, e o dialogo de
    // tokens sai sobre a tela escurecida pelo TheGame::updateFadeOut. Marcado na entrada de cada um (ScreenRect::underText).
    constexpr uint32_t kDmaGsPktQueue = wotm::fn::DmaGsPktQueue__5GsPkt, kFontDmaFontData = wotm::fn::fontDmaFontData__Fv;
    PS2Runtime::RecompiledFunction g_origDmaGsPktQueue = nullptr, g_origFontDma = nullptr;

    void dmaGsPktQueueTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kDmaGsPktQueue)
            wotmGsPktQueueSent();
        g_origDmaGsPktQueue(rdram, ctx, runtime);
    }

    void fontDmaTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kFontDmaFontData)
            wotmFontSent();
        g_origFontDma(rdram, ctx, runtime);
    }

    void uiFadeScreenTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        static uint32_t ra = 0, sp = 0;
        if (ctx->pc == kUiFadeScreen)
        {
            ra = GPR_U32(ctx, 31);
            sp = GPR_U32(ctx, 29);
        }
        g_origUiFade(rdram, ctx, runtime);
        if (ra && ctx->pc == ra && GPR_U32(ctx, 29) == sp)   // voltou (nao so uma pausa no meio)
        {
            wotmScreenRect(rdram, sp - 0xB0u, false);
            ra = 0;
        }
    }

    // Faixas pretas do menu: o letterboxDmaData(alfa) monta o pacote em D_00731850 e o manda direto; lido depois de voltar.
    constexpr uint32_t kLetterboxDmaData = wotm::fn::letterboxDmaData__Ff;
    PS2Runtime::RecompiledFunction g_origLetterbox = nullptr;

    void letterboxTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        static uint32_t ra = 0, sp = 0;
        if (ctx->pc == kLetterboxDmaData)
        {
            ra = GPR_U32(ctx, 31);
            sp = GPR_U32(ctx, 29);
        }
        g_origLetterbox(rdram, ctx, runtime);
        if (ra && ctx->pc == ra && GPR_U32(ctx, 29) == sp)   // voltou (nao so uma pausa no meio)
        {
            wotmLetterbox(rdram);
            ra = 0;
        }
    }

    void shellFadeScreenTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kShellFadeScreen)
        {
            const uint32_t sp = GPR_U32(ctx, 29);
            const int start = READ8(sp), end = READ8(sp + 8), step = READ8(sp + 0x10);
            wotmShellFade(start, step, (GPR_U32(ctx, 6) & 0xFF) == 0 && start < end);   // sem redesenho, subindo: acumula
        }
        g_origShellFade(rdram, ctx, runtime);
    }

    // Carga da fase: da entrada do Shell::LoadLevelFiles (o nivel novo comeca a sobrescrever a RAM; o cartao LOAD continua na
    // hierarquia) ate a entrada do rtMain, a tela nativa fica no ultimo quadro, so com o Shell::FadeScreen por cima (wotmLoadFreeze).
    constexpr uint32_t kLoadLevelFiles = wotm::fn::LoadLevelFiles__5Shell, kRtMain = wotm::fn::rtMain__Fb;
    PS2Runtime::RecompiledFunction g_origLoadLevelFiles = nullptr, g_origRtMain = nullptr;

    void loadLevelFilesTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kLoadLevelFiles)
            wotmLoadFreeze(true);
        g_origLoadLevelFiles(rdram, ctx, runtime);
    }
    void rtMainTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kRtMain)
            wotmLoadFreeze(false);
        g_origRtMain(rdram, ctx, runtime);
    }

    // HUD do ScreenPolys (icone de vida do Hud::drawHealthIcon): o dmaData manda os pacotes de s_polyBuffer e zera numPolys; na
    // entrada os quadrados do quadro ainda estao contados (wotm::readScreenPolys).
    constexpr uint32_t kScreenPolysDmaData = wotm::fn::dmaData__11ScreenPolysv;
    PS2Runtime::RecompiledFunction g_origPolysDma = nullptr;

    void screenPolysDmaTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kScreenPolysDmaData)
            wotmScreenPolys(rdram);
        g_origPolysDma(rdram, ctx, runtime);
    }

    // Saida do hier(0, 0): o desenho nativo copia a cena aqui (wotmCaptureScene), com o estado que a travessia acabou de mandar ao
    // VU1; o gameUpdate e o animationRunGlobal do rtMain, logo depois, ja escrevem o quadro seguinte.
    constexpr uint32_t kHier = wotm::fn::hier__Fii;
    PS2Runtime::RecompiledFunction g_origHier = nullptr;

    // A funcao pode voltar no meio (troca de thread) e ser retomada depois: so captura quando o pc chega ao $ra da chamada.
    uint32_t g_hierSceneRa = 0;   // $ra do hier(0, 0) em andamento (0 = nenhum)
    void hierTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kHier && GPR_U32(ctx, 4) == 0u && GPR_U32(ctx, 5) == 0u)
            g_hierSceneRa = GPR_U32(ctx, 31);
        g_origHier(rdram, ctx, runtime);
        if (g_hierSceneRa && ctx->pc == g_hierSceneRa)
        {
            g_hierSceneRa = 0;
            wotmCaptureScene(rdram);
        }
    }

    // Tom verde do pick-up que o monstro pode pegar: o enableHighlights o grava no colorQuad do _cs antes do hier(v,0), e o
    // computeHighlight (Monster::update, depois do hier) o volta a 1 antes da captura. Na saida, wotmPickupHighlights anota o valor
    // (Scene::csColorAtDraw).
    constexpr uint32_t kEnableHighlights = wotm::fn::enableHighlights__12LevelPickupsi;
    PS2Runtime::RecompiledFunction g_origEnableHighlights = nullptr;

    void enableHighlightsTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kEnableHighlights;
        g_origEnableHighlights(rdram, ctx, runtime);
        if (entry)
            wotmPickupHighlights(rdram);
    }

    // Mesmo comportamento da original (`jr $ra; lw $v0, gp_rel(D_006F8DB8)`), exceto o limite acima.
    void timerGetUpdateRate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        int32_t rate = static_cast<int32_t>(READ32(kUpdateRateVar));
        if (rate > 0 && rate < g_minRate)
        {
            rate = g_minRate;
            if ((++g_clamped % 20000ull) == 0 && std::getenv("PS2X_STATS"))
                std::fprintf(stderr, "[wotm] updateRate limitado a %d em %llu chamadas\n", g_minRate, g_clamped);
        }
        SET_GPR_S32(ctx, 2, rate);
        ctx->pc = GPR_U32(ctx, 31);
    }
}

namespace
{
    // Camera livre (F5, wotm_freecam.h). O _cs da vista 0 (viewInfo[0], o mesmo que a camera do jogo move) recebe a pose em dois
    // pontos: depois de Cameras::Update (o resto da logica do quadro, como os scripts da fase, o som e a IA, ja ve a camera nova) e na
    // entrada de viewUpdate(0), que monta worldToScreenMat e worldCtx[0] a partir de cs->trans e cs->mat (ganha de quem mexer no
    // _cs entre os dois, como a camera da introducao da luta). A camera do jogo continua rodando por baixo: ao desligar, ela sai da
    // pose livre e volta sozinha para o monstro. Com ela ligada (e sem o Tab), o scePadRead devolve o pad em repouso.
    constexpr uint32_t kViewUpdate = wotm::fn::viewUpdate__Fi, kScePadRead = wotm::fn::scePadRead;
    constexpr uint32_t kViewInfo = wotm::sym::viewInfo;   // 5 x 0x14: [0] = _cs* da vista
    PS2Runtime::RecompiledFunction g_origViewUpdate = nullptr, g_origCamerasUpdateFree = nullptr, g_origPadRead = nullptr;

    void putF(uint8_t *rdram, uint32_t a, float v) { std::memcpy(rdram + a, &v, 4); }

    void freeCamWrite(uint8_t *rdram)
    {
        if (!g_wotmFreeCam.load(std::memory_order_relaxed))
            return;
        uint32_t cs = 0;
        std::memcpy(&cs, rdram + kViewInfo, 4);
        cs &= 0x1FFFFFFFu;
        if (cs < 0x1000u || cs + 0x70u > PS2_RAM_SIZE)
            return;
        WotmFreeCamPose p;
        {
            std::lock_guard<std::mutex> lk(g_wotmFreeCamM);
            p = g_wotmFreeCamPose;
        }
        float ax[3][3];
        wotmFreeCamAxes(p, ax[0], ax[1], ax[2]);
        for (int i = 0; i < 3; ++i)
        {
            putF(rdram, cs + 0x10 + 4 * i, p.pos[i]);   // trans
            for (int r = 0; r < 3; ++r)
                putF(rdram, cs + 0x20 + 0x10 * r + 4 * i, ax[r][i]);   // mat: direita, frente, cima
        }
        for (int r = 0; r < 3; ++r)
            putF(rdram, cs + 0x20 + 0x10 * r + 0xC, 0.f);
        putF(rdram, cs + 0x60, p.pitch);   // rot (RPH): a camera do jogo parte daqui ao desligar
        putF(rdram, cs + 0x64, 0.f);
        putF(rdram, cs + 0x68, p.heading);
    }

    void camerasUpdateFree(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        g_origCamerasUpdateFree(rdram, ctx, runtime);
        freeCamWrite(rdram);
    }

    void viewUpdateFree(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (GPR_U32(ctx, 4) == 0u)
            freeCamWrite(rdram);
        g_origViewUpdate(rdram, ctx, runtime);
    }

    void padReadFree(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t data = GPR_U32(ctx, 6) & 0x1FFFFFFFu;   // scePadRead(port, slot, rdata)
        g_origPadRead(rdram, ctx, runtime);
        if (!g_wotmFreeCam.load(std::memory_order_relaxed) || g_wotmFreeCamPad.load(std::memory_order_relaxed))
            return;
        if (GPR_U32(ctx, 2) == 0u || data < 0x1000u || data + 32u > PS2_RAM_SIZE)
            return;
        uint8_t *d = rdram + data;
        d[2] = d[3] = 0xFF;                // botoes (ativos em 0)
        std::memset(d + 4, 0x80, 4);       // analogicos no centro
        std::memset(d + 8, 0, 12);         // pressao dos botoes
    }
}

void wotmInstallOverrides(PS2Runtime &runtime, const std::string &elfPath)
{
    const size_t slash = elfPath.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? elfPath : elfPath.substr(slash + 1);
    if (name.find("SCUS_971.97") == std::string::npos)
        return;

    if (const char *e = std::getenv("WOTM_RATEMIN"))
        g_minRate = std::atoi(e);
    if (std::getenv("WOTM_TURNLOG") && (g_origUpdatePosition = runtime.lookupFunction(kMonsterDynamicsUpdatePosition)))
    {
        runtime.registerFunction(kMonsterDynamicsUpdatePosition, &updatePositionLogged);
        std::fprintf(stderr, "[wotm] log da rotacao dos monstros ligado (WOTM_TURNLOG)\n");
    }
    if (std::getenv("WOTM_FIELDLOG"))
    {
        g_fieldLog = g_wotmCycleProf = true;
        g_lastHost = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[wotm] log de fields por quadro ligado (WOTM_FIELDLOG)\n");
    }
    if (g_minRate > 0 || g_fieldLog)
        runtime.registerFunction(kTimerGetUpdateRate, &timerGetUpdateRate);
    if ((g_origSyncV = runtime.lookupFunction(kSceGsSyncV)))
        runtime.registerFunction(kSceGsSyncV, &syncVTapped);
    if ((g_origWaitMinRate = runtime.lookupFunction(kTimerWaitForMinUpdateRate)) &&
        (g_origGetCurTics = runtime.lookupFunction(kTimerGetCurTics)))
    {
        runtime.registerFunction(kTimerWaitForMinUpdateRate, &waitMinRateByVblanks);
        runtime.registerFunction(kTimerGetCurTics, &getCurTicsMarked);
    }
    if ((g_origAddCall = runtime.lookupFunction(kAddCallToQueue)))
        runtime.registerFunction(kAddCallToQueue, &addCallToQueueTapped);
    if ((g_origDmaGsPktQueue = runtime.lookupFunction(kDmaGsPktQueue)))
        runtime.registerFunction(kDmaGsPktQueue, &dmaGsPktQueueTapped);
    if ((g_origFontDma = runtime.lookupFunction(kFontDmaFontData)))
        runtime.registerFunction(kFontDmaFontData, &fontDmaTapped);
    if ((g_origUiFade = runtime.lookupFunction(kUiFadeScreen)))
        runtime.registerFunction(kUiFadeScreen, &uiFadeScreenTapped);
    if ((g_origLetterbox = runtime.lookupFunction(kLetterboxDmaData)))
        runtime.registerFunction(kLetterboxDmaData, &letterboxTapped);
    if ((g_origShellFade = runtime.lookupFunction(kShellFadeScreen)))
        runtime.registerFunction(kShellFadeScreen, &shellFadeScreenTapped);
    if ((g_origLoadLevelFiles = runtime.lookupFunction(kLoadLevelFiles)) && (g_origRtMain = runtime.lookupFunction(kRtMain)))
    {
        runtime.registerFunction(kLoadLevelFiles, &loadLevelFilesTapped);
        runtime.registerFunction(kRtMain, &rtMainTapped);
    }
    if ((g_origPolysDma = runtime.lookupFunction(kScreenPolysDmaData)))
        runtime.registerFunction(kScreenPolysDmaData, &screenPolysDmaTapped);
    if ((g_origHier = runtime.lookupFunction(kHier)))
        runtime.registerFunction(kHier, &hierTapped);
    if ((g_origEnableHighlights = runtime.lookupFunction(kEnableHighlights)))
        runtime.registerFunction(kEnableHighlights, &enableHighlightsTapped);
    if (const char *e = std::getenv("WOTM_MATCH"))
    {
        int v[5], n = 0;
        const char *p = e;
        for (char *end; n < 5; p = end + 1)
        {
            v[n++] = static_cast<int>(std::strtol(p, &end, 10));
            if (end == p || *end != ',')
            {
                p = end == p ? nullptr : end;
                break;
            }
        }
        if (p && *p == '\0' && n >= 3)
        {
            g_matchAIs = n - 2;
            for (int i = 0; i <= g_matchAIs; ++i)
                g_matchMonsters[i] = v[i];
            g_matchLevel = v[n - 1];
        }
        if (g_matchAIs > 0)
        {
            runtime.registerFunction(kUserintMain, &userintMainDirect);
            runtime.registerFunction(kUiMain, &uiMainSkipped);
            if ((g_origEvaluate = runtime.lookupFunction(kEvaluateGameStatus)))
                runtime.registerFunction(kEvaluateGameStatus, &evaluateGameStatusLogged);
            std::fprintf(stderr, "[wotm] partida direta: monstro %d contra %d IA(s), fase %d (WOTM_MATCH)\n", g_matchMonsters[0], g_matchAIs, g_matchLevel);
        }
        else
            std::fprintf(stderr, "[wotm] WOTM_MATCH invalido (\"%s\"; use <monstro>,<IA>[,<IA>[,<IA>]],<fase>)\n", e);
    }
    if (std::getenv("WOTM_AIVSAI") && (g_aiInit = runtime.lookupFunction(kAiInit)) &&
        (g_origSetAIMonster = runtime.lookupFunction(kSetAIMonster)))
    {
        g_aiVsAi = true;
        runtime.registerFunction(kPlayerInit, &playerInitAsAi);
        runtime.registerFunction(kSetAIMonster, &setAIMonsterCounted);
        if ((g_origStartPoint = runtime.lookupFunction(kGameGetStartPoint)))
            runtime.registerFunction(kGameGetStartPoint, &startPointAsHumans);
        if ((g_origCamerasUpdate = runtime.lookupFunction(kCamerasUpdate)))
            runtime.registerFunction(kCamerasUpdate, &camerasUpdateAsHumans);
        std::fprintf(stderr, "[wotm] jogador controlado pela IA (WOTM_AIVSAI)\n");
    }
    if ((g_origFfaOptions1P = runtime.lookupFunction(kFfaOptions1P)))
        runtime.registerFunction(kFfaOptions1P, &ffaOptions1PWithZero);
    // Trapacas de debug (menu F1): sempre instaladas; WOTM_CHEATS=inv,stam,furia,esp,cloak,deus liga desde o inicio
    if ((g_origMonsterUpdate = runtime.lookupFunction(kMonsterUpdate)) && (g_origTakeDamage = runtime.lookupFunction(kTakeDamage)))
    {
        g_setCloakOn = runtime.lookupFunction(kSetCloakOn);
        runtime.registerFunction(kMonsterUpdate, &monsterUpdateCheats);
        runtime.registerFunction(kTakeDamage, &takeDamageCheat);
        if ((g_origTakeHit = runtime.lookupFunction(kTakeHit)))
            runtime.registerFunction(kTakeHit, &takeHitCheat);
        if ((g_origKnockBack = runtime.lookupFunction(kKnockBack)))
            runtime.registerFunction(kKnockBack, &knockBackCheat);
        if ((g_origAdditiveRecoil = runtime.lookupFunction(kAdditiveRecoil)))
            runtime.registerFunction(kAdditiveRecoil, &additiveRecoilCheat);
        if ((g_origEnterNewState = runtime.lookupFunction(kEnterNewState)))
            runtime.registerFunction(kEnterNewState, &enterNewStateCheat);        uint32_t c = std::getenv("WOTM_CLOAK") ? kCheatCloak : 0u;
        if (const char *e = std::getenv("WOTM_CHEATS"))
        {
            const std::string s = std::string(",") + e + ",";
            if (s.find(",inv,") != std::string::npos) c |= kCheatInvincible;
            if (s.find(",stam,") != std::string::npos) c = (c & ~kCheatStaminaMask) | kCheatStamina1;
            if (s.find(",furia,") != std::string::npos) c = (c & ~kCheatStaminaMask) | 2u * kCheatStamina1;
            if (s.find(",esp,") != std::string::npos) c |= kCheatSpecial;
            if (s.find(",cloak,") != std::string::npos) c |= kCheatCloak;
            if (s.find(",deus,") != std::string::npos) c |= kCheatGod;
            if (const size_t w = s.find(",vencer="); w != std::string::npos)
                g_autoWinSecs = std::atoi(s.c_str() + w + 8);
        }
        g_wotmCheats = c;
        if (c || g_autoWinSecs > 0)
            std::fprintf(stderr, "[wotm] trapacas de debug ligadas desde o inicio: 0x%x, vencer a cada %d s (WOTM_CHEATS/WOTM_CLOAK)\n", c, g_autoWinSecs);
    }
    // Camera livre (F5): depois dos outros hooks, para o de Cameras::Update embrulhar o do WOTM_AIVSAI quando ele existe
    if ((g_origCamerasUpdateFree = runtime.lookupFunction(kCamerasUpdate)) && (g_origViewUpdate = runtime.lookupFunction(kViewUpdate)) &&
        (g_origPadRead = runtime.lookupFunction(kScePadRead)))
    {
        runtime.registerFunction(kCamerasUpdate, &camerasUpdateFree);
        runtime.registerFunction(kViewUpdate, &viewUpdateFree);
        runtime.registerFunction(kScePadRead, &padReadFree);
    }
    else
        std::fprintf(stderr, "[wotm] camera livre indisponivel: Cameras::Update, viewUpdate ou scePadRead sem funcao no runtime\n");
    extern void wotmVu1CheckInstall(PS2Runtime &);   // wotm_vu1check.inc (WOTM_VU1CHECK: VU1 x nativo por objeto)
    wotmVu1CheckInstall(runtime);
    if (g_minRate > 0)
        std::fprintf(stderr, "[wotm] updateRate >= %d (dt <= 1/%d) para o jogo inteiro (WOTM_RATEMIN)\n", g_minRate, g_minRate);
    else
        std::fprintf(stderr, "[wotm] updateRate sem limite (WOTM_RATEMIN=0)\n");
}
