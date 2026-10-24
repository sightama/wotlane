// Skins de mod na luta (MODS.md, "Skins"). Incluido pelo wotm_overrides.cpp, depois do wotm_mods.inc.
//
// - Sons do traje base: o traje so escolhe arquivo e vira o m_skinNum do Monster (+0x30), que so o som le, por igualdade (Preytor
//   traje 1, as formigas; Agamo traje 3, o Mecha Sweet Tooth: Sound.cpp). Uma skin chega com o indice dela (4+) e cairia no som
//   padrao; aqui o TheGame::SetPlayerMonster / SetAIMonster recebe o `base` da skin no lugar do indice. A carga dos arquivos ja
//   aconteceu (m_costume fica como esta).
// - IAs sorteadas (resistencia, Shell::RandomlySelectAI): o retail sorteia so o monstro e deixa o traje que estiver em
//   m_costume[2 + i]. Para um monstro com skins de mod "ai": true, o traje sai entre o do retail e essas skins.
// Sem skins de mod nenhum gancho e instalado.

#include <random>

namespace wotm_skins
{
    using wotm::mods::Registry;
    using wotm::mods::Skin;

    constexpr uint32_t kSetPlayer = wotm::fn::SetPlayerMonster__7TheGameiiiii, kSetAI = wotm::fn::SetAIMonster__7TheGameiiii,
                       kRandomAI = wotm::fn::RandomlySelectAI__5Shell;
    constexpr uint32_t kShellObj = wotm::sym::shellInfo;
    constexpr uint32_t kShellNumAIs = 0x2BB8, kShellMonsterSel = 0x2938, kShellCostume = 0x2BE0;
    PS2Runtime::RecompiledFunction g_origSetPlayer = nullptr, g_origSetAI = nullptr, g_origRandomAI = nullptr;

    // tipo do jogo (m_typeBits, monstro << 5) + traje -> traje que o som deve ver
    uint32_t soundSkin(uint32_t type, uint32_t skin)
    {
        if (skin < 4u)
            return skin;
        if (const Skin *k = Registry::get().skinByIndex(static_cast<int>(type >> 5), static_cast<int>(skin)))
        {
            std::fprintf(stderr, "[mods] skin %s/%s (traje %u) na luta com os sons do traje %d\n", k->monsterName.c_str(), k->id.c_str(), skin, k->base);
            return static_cast<uint32_t>(k->base);
        }
        return skin;
    }

    // SetPlayerMonster(this, jogador, tipo, dup, vista, traje): traje em t1
    void setPlayer(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kSetPlayer)
        {
            const uint32_t type = GPR_U32(ctx, 6), skin = GPR_U32(ctx, 9);
            ctx->r[9] = _mm_set_epi64x(0, static_cast<int64_t>(soundSkin(type, skin)));
        }
        g_origSetPlayer(rdram, ctx, runtime);
    }
    // SetAIMonster(this, ia, tipo, dup, traje): traje em t0
    void setAI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kSetAI)
        {
            const uint32_t type = GPR_U32(ctx, 6), skin = GPR_U32(ctx, 8);
            ctx->r[8] = _mm_set_epi64x(0, static_cast<int64_t>(soundSkin(type, skin)));
        }
        g_origSetAI(rdram, ctx, runtime);
    }

    void randomAI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kRandomAI;
        g_origRandomAI(rdram, ctx, runtime);
        if (!entry)
            return;
        static std::mt19937 rng{std::random_device{}()};
        const int32_t n = static_cast<int32_t>(READ32(kShellObj + kShellNumAIs));
        for (int32_t i = 0; i < n && i < 6; ++i)
        {
            const uint32_t mon = READ32(kShellObj + kShellMonsterSel + (4 + i) * 4) >> 5;
            std::vector<const Skin *> pool;
            for (const Skin &k : Registry::get().skins())
                if (k.ai && k.monster == static_cast<int>(mon))
                    pool.push_back(&k);
            if (pool.empty())
                continue;
            // o traje do retail conta como mais uma opcao (sem mods, a mesma luta de sempre)
            const size_t pick = std::uniform_int_distribution<size_t>(0, pool.size())(rng);
            if (pick == pool.size())
                continue;
            WRITE32(kShellObj + kShellCostume + (2 + i) * 4, static_cast<uint32_t>(pool[pick]->index));
            std::fprintf(stderr, "[mods] IA %d sorteada com a skin %s/%s (traje %d)\n", i + 1, pool[pick]->monsterName.c_str(),
                         pool[pick]->id.c_str(), pool[pick]->index);
        }
    }
} // namespace wotm_skins

// Depois de todos os outros ganchos destas funcoes (wotm_split4.inc, WOTM_AIVSAI): este embrulha o que estiver registrado.
void wotmSkinsInstall(PS2Runtime &runtime)
{
    using namespace wotm_skins;
    if (Registry::get().skins().empty())
        return;
    if ((g_origSetPlayer = runtime.lookupFunction(kSetPlayer)))
        runtime.registerFunction(kSetPlayer, &setPlayer);
    if ((g_origSetAI = runtime.lookupFunction(kSetAI)))
        runtime.registerFunction(kSetAI, &setAI);
    bool anyAi = false;
    for (const Skin &k : Registry::get().skins())
        anyAi = anyAi || k.ai;
    if (anyAi && (g_origRandomAI = runtime.lookupFunction(kRandomAI)))
        runtime.registerFunction(kRandomAI, &randomAI);
    std::fprintf(stderr, "[mods] skins na luta: sons do traje base%s\n", anyAi ? ", IAs sorteadas com skin" : "");
}
