// Mod skins in the fight (MODS.md, "Skins"). Included by wotm_overrides.cpp, after wotm_mods.inc.
//
// - Base costume sounds: the costume only picks files and becomes the Monster's m_skinNum (+0x30), which only the sound code reads,
//   by equality (Preytor costume 1, the ants; Agamo costume 3, the Mecha Sweet Tooth: Sound.cpp). A skin arrives with its own index
//   (4+) and would fall back to the default sound; here TheGame::SetPlayerMonster / SetAIMonster receives the skin's `base` instead
//   of the index. The files have already been loaded by then (m_costume stays as it is).
// - Randomly drawn AIs (endurance, Shell::RandomlySelectAI): the retail game draws only the monster and keeps whatever costume is in
//   m_costume[2 + i]. For a monster with mod skins marked "ai": true, the costume is drawn from the retail one plus those skins.
// With no mod skins, no hook is installed.

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

    // game type (m_typeBits, monster << 5) + costume -> the costume the sound code should see
    uint32_t soundSkin(uint32_t type, uint32_t skin)
    {
        if (skin < 4u)
            return skin;
        if (const Skin *k = Registry::get().skinByIndex(static_cast<int>(type >> 5), static_cast<int>(skin)))
        {
            std::fprintf(stderr, "[mods] skin %s/%s (costume %u) in the fight with the sounds of costume %d\n", k->monsterName.c_str(), k->id.c_str(), skin, k->base);
            return static_cast<uint32_t>(k->base);
        }
        return skin;
    }

    // SetPlayerMonster(this, player, type, dup, view, costume): costume in t1
    void setPlayer(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kSetPlayer)
        {
            const uint32_t type = GPR_U32(ctx, 6), skin = GPR_U32(ctx, 9);
            ctx->r[9] = _mm_set_epi64x(0, static_cast<int64_t>(soundSkin(type, skin)));
        }
        g_origSetPlayer(rdram, ctx, runtime);
    }
    // SetAIMonster(this, ai, type, dup, costume): costume in t0
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
        // with WOTM_SEED (network match, determinism tests) the draw comes from the seed, the same on every PC
        static std::mt19937 rng{[] {
            const char *e = std::getenv("WOTM_SEED");
            return e && *e ? static_cast<uint32_t>(std::atoll(e)) : std::random_device{}();
        }()};
        const int32_t n = static_cast<int32_t>(READ32(kShellObj + kShellNumAIs));
        for (int32_t i = 0; i < n && i < 6; ++i)
        {
            const uint32_t mon = READ32(kShellObj + kShellMonsterSel + (4 + i) * 4) >> 5;
            std::vector<const Skin *> pool;
            for (const Skin &k : Registry::get().skins())
                if (k.ai && k.custom.empty() && k.monster == static_cast<int>(mon))
                    pool.push_back(&k);
            if (pool.empty())
                continue;
            // the retail costume counts as one more option (without mods, the same fight as always)
            const size_t pick = std::uniform_int_distribution<size_t>(0, pool.size())(rng);
            if (pick == pool.size())
                continue;
            WRITE32(kShellObj + kShellCostume + (2 + i) * 4, static_cast<uint32_t>(pool[pick]->index));
            std::fprintf(stderr, "[mods] AI %d drew skin %s/%s (costume %d)\n", i + 1, pool[pick]->monsterName.c_str(),
                         pool[pick]->id.c_str(), pool[pick]->index);
        }
    }
} // namespace wotm_skins

// After every other hook on these functions (wotm_split4.inc, WOTM_AIVSAI): this one wraps whatever is registered.
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
    std::fprintf(stderr, "[mods] skins in the fight: base costume sounds%s\n", anyAi ? ", random AIs drawn with skins" : "");
}
