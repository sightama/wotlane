// Monstros custom (POS_FRONTEIRA.md, item 8, parte 2): monstros que o jogo nao oferece na selecao, jogados numa luta comum. O
// primeiro e o Goliath Prime (tipo 13, "assboss", MON/ASSBOSS1.*), chefe da aventura em canyon2. Incluido pelo wotm_overrides.cpp,
// depois do wotm_skins.inc. Investigacao: docs/investigacoes/2026-10-23/goliath_codigo.md.
//
// - TheGame::SetAIMonster: a cada IA que entra, o retail desliga todo slot do tipo 0x1A0 (disableMonster: m_playerNum 0 e drawMe 0
//   no m_cs, na sombra e nos _cs de +0x71F8/+0x71FC; na aventura e o que esconde o chefe ate a cena). Com o Goliath como jogador
//   ele ficava invisivel e parado. Aqui o que o disableMonster zera nos jogadores (m_monsters[0..3]) volta depois da chamada; o
//   chefe da aventura e uma IA (m_monsters[4..]) e continua como no retail.
// - Monster::takeHit: no 0x1A0 o dano so passa com o attackState do chefe em 4 ou 10 ou vindo do jogador 1 (attackerId ==
//   m_monsters[0]->m_id). Com o Goliath jogador, as IAs nao o machucavam. Durante a chamada, o m_id do jogador 1 vale o do
//   atacante (a unica leitura de m_monsters[0] no takeHit e esse teste); o resto do takeHit segue o caminho normal (bloqueio,
//   recuos). So quando o Goliath atingido e um jogador.

namespace wotm_custom
{
    constexpr uint32_t kSetAI = wotm::fn::SetAIMonster__7TheGameiiii, kTakeHit = wotm::fn::takeHit__7MonsterP8_fvectorfi;
    constexpr uint32_t kGameObj = wotm::sym::shellGame;
    constexpr uint32_t kMonsters = 0x120380;          // TheGame::m_monsters[8]
    constexpr uint32_t kTypeBits = 0x14, kPlayerNum = 0x18, kId = 0x20, kCs = 0xC, kShadow = 0x1A3C, kCsA = 0x71F8, kCsB = 0x71FC;
    constexpr uint32_t kDrawMe = 0xC;                 // _cs::drawMe (byte)
    constexpr uint32_t kGoliath = 13u << 5;
    PS2Runtime::RecompiledFunction g_origSetAI = nullptr, g_origTakeHit = nullptr;

    bool isPlayerSlot(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t m)
    {
        for (uint32_t i = 0; i < 4; ++i)
            if (READ32(kGameObj + kMonsters + 4 * i) == m)
                return true;
        return false;
    }

    struct Saved
    {
        uint32_t m = 0, playerNum = 0;
        uint32_t cs[4] = {};
        uint8_t draw[4] = {};
    };

    void setAI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc != kSetAI)
        {
            g_origSetAI(rdram, ctx, runtime);
            return;
        }
        Saved saved[4];
        int n = 0;
        for (uint32_t i = 0; i < 4; ++i)
        {
            const uint32_t m = READ32(kGameObj + kMonsters + 4 * i);
            if (!m || READ32(m + kTypeBits) != kGoliath)
                continue;
            Saved &s = saved[n++];
            s.m = m;
            s.playerNum = READ32(m + kPlayerNum);
            const uint32_t at[4] = {kCs, kShadow, kCsA, kCsB};
            for (int k = 0; k < 4; ++k)
                if ((s.cs[k] = READ32(m + at[k])))
                    s.draw[k] = READ8(s.cs[k] + kDrawMe);
        }
        g_origSetAI(rdram, ctx, runtime);
        for (int j = 0; j < n; ++j)
        {
            const Saved &s = saved[j];
            WRITE32(s.m + kPlayerNum, s.playerNum);
            for (int k = 0; k < 4; ++k)
                if (s.cs[k])
                    WRITE8(s.cs[k] + kDrawMe, s.draw[k]);
        }
    }

    void takeHit(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t self = GPR_U32(ctx, 4), attacker = GPR_U32(ctx, 6);
        const uint32_t p1 = ctx->pc == kTakeHit && READ32(self + kTypeBits) == kGoliath && isPlayerSlot(rdram, ctx, runtime, self)
                                ? READ32(kGameObj + kMonsters) : 0;
        const uint32_t oldId = p1 ? READ32(p1 + kId) : 0;
        if (p1)
            WRITE32(p1 + kId, attacker);
        g_origTakeHit(rdram, ctx, runtime);
        if (p1)
            WRITE32(p1 + kId, oldId);
    }
} // namespace wotm_custom

// Depois do wotmSkinsInstall: embrulha os ganchos do SetAIMonster que ja estiverem registrados (WOTM_AIVSAI, skins).
void wotmCustomInstall(PS2Runtime &runtime)
{
    using namespace wotm_custom;
    if ((g_origSetAI = runtime.lookupFunction(kSetAI)))
        runtime.registerFunction(kSetAI, &setAI);
    if ((g_origTakeHit = runtime.lookupFunction(kTakeHit)))
        runtime.registerFunction(kTakeHit, &takeHit);
    std::fprintf(stderr, "[custom] Goliath Prime jogavel: %s, %s\n", g_origSetAI ? "visivel com IAs" : "SEM gancho do SetAIMonster",
                 g_origTakeHit ? "toma dano das IAs" : "SEM gancho do takeHit");
}
