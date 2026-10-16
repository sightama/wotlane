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

    // WOTM_CLOAK=1 (teste do CLOAK na checagem VU1): na entrada de Monster::update (0x15B488) do jogador (m_monsters[0]) sem CLOAK
    // (m_cloaked, +0xEF), chama Monster::setCloakOn (0x162CD0), como o power-up; quando o tempo acaba, liga de novo no quadro seguinte.
    constexpr uint32_t kMonsterUpdate = wotm::fn::update__7Monster, kSetCloakOn = wotm::fn::setCloakOn__7Monster;
    PS2Runtime::RecompiledFunction g_origMonsterUpdate = nullptr, g_setCloakOn = nullptr;

    void monsterUpdateCloaked(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        static uint32_t calls = 0;
        if (++calls <= 2) std::fprintf(stderr, "[wotm] Monster::update pc %x monstro %x jogador %x\n", ctx->pc, m, READ32(kGame + 0x120380));
        if (ctx->pc == kMonsterUpdate && m && m == READ32(kGame + 0x120380) && !(READ32(m + 0xEC) & 0xFF000000u))
        {
            const R5900Context saved = *ctx;
            ctx->pc = kSetCloakOn;
            g_setCloakOn(rdram, ctx, runtime);
            static uint32_t n = 0;
            if (++n <= 3) std::fprintf(stderr, "[wotm] CLOAK ligado no monstro %x (m_cloaked %u)\n", m, READ32(m + 0xEC) >> 24);
            *ctx = saved;
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
    void syncVLogged(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (g_syncVCalls < 4)
            g_syncVRa[g_syncVCalls] = GPR_U32(ctx, 31);
        ++g_syncVCalls;
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
            if (g_wotmFreeSpinPc != 0u && g_frameStartValid)
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
        if ((g_origSyncV = runtime.lookupFunction(kSceGsSyncV)))
            runtime.registerFunction(kSceGsSyncV, &syncVLogged);
        std::fprintf(stderr, "[wotm] log de fields por quadro ligado (WOTM_FIELDLOG)\n");
    }
    if (g_minRate > 0 || g_fieldLog)
        runtime.registerFunction(kTimerGetUpdateRate, &timerGetUpdateRate);
    if ((g_origWaitMinRate = runtime.lookupFunction(kTimerWaitForMinUpdateRate)) &&
        (g_origGetCurTics = runtime.lookupFunction(kTimerGetCurTics)))
    {
        runtime.registerFunction(kTimerWaitForMinUpdateRate, &waitMinRateByVblanks);
        runtime.registerFunction(kTimerGetCurTics, &getCurTicsMarked);
    }
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
    if (std::getenv("WOTM_CLOAK") && (g_origMonsterUpdate = runtime.lookupFunction(kMonsterUpdate)) &&
        (g_setCloakOn = runtime.lookupFunction(kSetCloakOn)))
    {
        runtime.registerFunction(kMonsterUpdate, &monsterUpdateCloaked);
        std::fprintf(stderr, "[wotm] jogador sempre com CLOAK (WOTM_CLOAK)\n");
    }
    extern void wotmVu1CheckInstall(PS2Runtime &);   // wotm_vu1check.inc (WOTM_VU1CHECK: VU1 x nativo por objeto)
    wotmVu1CheckInstall(runtime);
    if (g_minRate > 0)
        std::fprintf(stderr, "[wotm] updateRate >= %d (dt <= 1/%d) para o jogo inteiro (WOTM_RATEMIN)\n", g_minRate, g_minRate);
    else
        std::fprintf(stderr, "[wotm] updateRate sem limite (WOTM_RATEMIN=0)\n");
}
