// ABOUTME: Vista por jogador de 2 jogadores com WOTM_LOCALPLAYER (as duas vistas no viewport 0) e jogadores 5..8 em IAs guiadas pelos
// ABOUTME: controles 4..7 (WOTM_PLAYERS=5..8). Os quadrantes de 3 e 4 jogadores sao do wotm_split4.inc. Incluido por wotm_overrides.cpp.
//
// 2 jogadores com WOTM_LOCALPLAYER (cada um no seu PC ve a sua vista em tela cheia): TheGame::gameReestablishViews no modo 3 vira
// viewSetNumViews(2) e viewCreate(0, v) para os dois, como o modo 8 do retail faz com duas vistas no viewport 0; a tela cheia tem a
// proporcao 4:3, entao projecao, corte, HUD e cameras do jogo continuam certos, sem a metade 2:3 da tela dividida do retail.
namespace wotm_multi
{
    constexpr uint32_t kReestablishViews = wotm::fn::gameReestablishViews__7TheGame;
    constexpr uint32_t kGameMode = kGame + 0x1203C8, kMonsters = kGame + 0x120380;
    constexpr uint32_t kCameras = wotm::sym::x_7Cameras_m_cameras, kCameraStride = 0xEB0, kNumCameras = wotm::sym::x_7Cameras_m_numCameras;

    PS2Runtime::RecompiledFunction g_origReestablish = nullptr;

    // Uma vista no viewport 0 por jogador e sem vista unificada: 2 jogadores com WOTM_LOCALPLAYER
    bool perPlayerViews()
    {
        static const bool local = [] { const char *e = std::getenv("WOTM_LOCALPLAYER"); return e && std::atoi(e) >= 1; }();
        return local && g_matchPlayers == 2;
    }

    void reestablishViewsPerPlayer(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc != kReestablishViews || !perPlayerViews() || READ32(kGameMode) != 3u)
        {
            g_origReestablish(rdram, ctx, runtime);
            return;
        }
        const uint32_t n = static_cast<uint32_t>(g_matchPlayers);
        wotm_save::guestCall(rdram, ctx, runtime, wotm::fn::viewSetNumViews__Fi, {n});
        for (uint32_t v = 0; v < n; ++v)
            wotm_save::guestCall(rdram, ctx, runtime, wotm::fn::viewCreate__F10_viewportsi, {0u, v});
        WRITE32(kNumCameras, n);
        for (uint32_t i = 0; i < n; ++i)
            wotm_save::guestCall(rdram, ctx, runtime, wotm::fn::SetCameraPOV__7CamerasiQ26Camera9CameraPOV,
                                 {i, READ32(kCameras + i * kCameraStride + 0xCF8)});
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            std::fprintf(stderr, "[multi] %u vistas no viewport 0, %u cameras\n", n, n);
        }
        ctx->pc = GPR_U32(ctx, 31);
    }

    // ---- Jogadores 5..8 (WOTM_PLAYERS=5..8, g_padAIs): monstros nas IAs m_monsters[4..7] guiados pelos controles 4..7 ----
    // Humano e IA enchem o mesmo GamePad do monstro (+0x5024, 0x1C bytes) e o PadFlags::interpretInputs le dos dois igual; o
    // Ai::updateInputs dessas IAs vira o GamePad::loadPadInputs(pad, 4 + m_index) do proprio jogo (o modo de debug Ai::s_mode 3 do
    // retail faz o mesmo com o controle 0). Continuam marcadas como IA (m_playerNum 2): como jogador elas cairiam em tabelas de 4 e
    // de 2 (PlayerDat, eventos de som, tokens, HUD e camera). O que so a IA tem e corrigido: o aiInit zera a arma especial (fica a
    // que o Monster::init deu, como no playerInit), o fator de dano do Ai (+0x152C) fica 1 e o updateAirMotion nao dobra a
    // aceleracao no ar (roda com m_playerNum 1 so durante a chamada). O movimento do free for all nao depende de camera (mapa CR do
    // PadFlags), entao eles nao precisam de uma. Investigacao: docs/plans/8p-investigation.md (local).
    constexpr uint32_t kAiUpdateInputs = wotm::fn::updateInputs__2Ai, kLoadPadInputs = wotm::fn::loadPadInputs__7GamePadi,
                       kMonsterInit = wotm::fn::init__7Monster, kAirMotion = wotm::fn::updateAirMotion__15MonsterDynamicsff;
    constexpr uint32_t kMonPlayerNum = 0x18, kMonIndex = 0x24, kMonCs = 0xC, kMonSpecial = 0xF3, kMonAi = 0x4E0, kAiPad = 0x44,
                       kAiDamageFactor = 0x152C, kMonDynamics = 0x100;
    PS2Runtime::RecompiledFunction g_origAiUpdateInputs = nullptr, g_loadPadInputs = nullptr, g_origMonsterInit = nullptr,
                                   g_origAirMotion = nullptr, g_origAiInit = nullptr;
    uint32_t g_padInitMonster = 0;   // aiInit em curso de uma IA de jogador: o Monster::init guarda a arma especial que deu
    uint8_t g_padInitSpecial = 0;

    // indice (0..g_padAIs-1) da IA de jogador que e este monstro, ou -1
    int padAI(uint8_t *rdram, uint32_t m)
    {
        if (!m || g_padAIs <= 0)
            return -1;
        const uint32_t k = FAST_READ32(m + kMonIndex);
        return k < static_cast<uint32_t>(g_padAIs) && FAST_READ32(kMonsters + 4 * (4 + k)) == m ? static_cast<int>(k) : -1;
    }

    // Prova de que o controle chega ao monstro (tools/net_check.ps1): da primeira entrada nao neutra de cada jogador 5..8 a 2 s de
    // jogo (g_frame) depois, quanto o monstro andou
    struct PadMove
    {
        uint32_t startFrame = 0;
        float start[3] = {};
        bool started = false, done = false;
    };
    PadMove g_padMove[4];

    void logPadMove(uint8_t *rdram, int k, uint32_t m, uint32_t pad)
    {
        PadMove &pm = g_padMove[k];
        if (pm.done)
            return;
        const uint32_t cs = FAST_READ32(m + kMonCs), g = FAST_READ32(wotm::sym::g_frame);
        float pos[3];
        for (int i = 0; i < 3; ++i)
        {
            const uint32_t bits = FAST_READ32(cs + 0x10 + 4 * i);
            std::memcpy(&pos[i], &bits, 4);
        }
        if (!pm.started)
        {
            bool any = false;
            for (uint32_t b = 0; b < 0x14; ++b)
                any = any || FAST_READ8(pad + b) != 0;
            if (!any)
                return;
            pm.started = true;
            pm.startFrame = g;
            std::memcpy(pm.start, pos, sizeof pos);
            return;
        }
        if (g - pm.startFrame < 120u)
            return;
        pm.done = true;
        const float dx = pos[0] - pm.start[0], dy = pos[1] - pm.start[1], dz = pos[2] - pm.start[2];
        std::fprintf(stderr, "[multi] jogador %d andou %.0f unidades em %u quadros desde a primeira entrada do controle %d\n", 5 + k,
                     std::sqrt(dx * dx + dy * dy + dz * dz), g - pm.startFrame, 5 + k);
    }

    void aiUpdateInputsFromPad(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kAiUpdateInputs)
        {
            const uint32_t ai = GPR_U32(ctx, 4), m = FAST_READ32(ai);
            if (const int k = padAI(rdram, m); k >= 0)
            {   // o loadPadInputs no lugar do corpo do updateInputs: volta direto para o Monster::update
                const uint32_t pad = FAST_READ32(ai + kAiPad), ra = GPR_U32(ctx, 31);
                SET_GPR_U32(ctx, 4, pad);
                SET_GPR_U32(ctx, 5, 4u + static_cast<uint32_t>(k));
                ctx->pc = kLoadPadInputs;
                g_loadPadInputs(rdram, ctx, runtime);
                if (ctx->pc == ra)
                    logPadMove(rdram, k, m, pad);
                return;
            }
        }
        g_origAiUpdateInputs(rdram, ctx, runtime);
    }

    void monsterInitKeepsSpecial(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4), ra = GPR_U32(ctx, 31);
        const bool watch = ctx->pc == kMonsterInit && m == g_padInitMonster;
        g_origMonsterInit(rdram, ctx, runtime);
        if (watch && ctx->pc == ra)
            g_padInitSpecial = FAST_READ8(m + kMonSpecial);
    }

    void airMotionAsPlayer(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4) - kMonDynamics, ra = GPR_U32(ctx, 31);
        if (ctx->pc != kAirMotion || padAI(rdram, m) < 0)
            return g_origAirMotion(rdram, ctx, runtime);
        FAST_WRITE32(m + kMonPlayerNum, 1u);
        g_origAirMotion(rdram, ctx, runtime);
        if (ctx->pc == ra)
            FAST_WRITE32(m + kMonPlayerNum, 2u);
    }

    // Monster::aiInit de uma IA de jogador (largada e renascimento): guarda a arma especial e o fator de dano 1
    void aiInitKeepsPadPlayers(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        if (ctx->pc != kAiInit || padAI(rdram, m) < 0)
            return g_origAiInit(rdram, ctx, runtime);
        const uint32_t ra = GPR_U32(ctx, 31);
        g_padInitMonster = m;
        g_padInitSpecial = FAST_READ8(m + kMonSpecial);
        g_origAiInit(rdram, ctx, runtime);
        g_padInitMonster = 0;
        if (ctx->pc == ra)
        {
            FAST_WRITE8(m + kMonSpecial, g_padInitSpecial);
            const float one = 1.f;
            uint32_t bits;
            std::memcpy(&bits, &one, 4);
            FAST_WRITE32(m + kMonAi + kAiDamageFactor, bits);
        }
    }

    // Combate de perto: Cameras::InitCCCamera/InitCCCameraFromThrow ligam gUseUnifiedView e poem a camera 2 no POV 1; com a vista
    // unificada o viewGetNumViews devolve 1 e o outro jogador fica sem tela. Com uma vista por jogador nao fazem nada.
    // As cinemas (doCinema/endCinema) alternam a tela dividida (viewToggleSplitScreen), que com 2 vistas volta a uma ou as metades 2:3;
    // com uma vista por jogador ficam as vistas do gameReestablishViews.
    constexpr uint32_t kInitCC = wotm::fn::InitCCCamera__7CamerasP7MonsterT1, kInitCCThrow = wotm::fn::InitCCCameraFromThrow__7CamerasP7MonsterT1,
                       kToggleSplit = wotm::fn::viewToggleSplitScreen__FbT0;
    PS2Runtime::RecompiledFunction g_origInitCC = nullptr, g_origInitCCThrow = nullptr, g_origToggleSplit = nullptr;

    template <uint32_t Entry, PS2Runtime::RecompiledFunction *Orig> void skipWithPlayerViews(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == Entry && perPlayerViews())
        {
            SET_GPR_S32(ctx, 2, 0);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        (*Orig)(rdram, ctx, runtime);
    }

    void install(PS2Runtime &runtime)
    {
        if (g_padAIs > 0 && (g_origAiUpdateInputs = runtime.lookupFunction(kAiUpdateInputs)) &&
            (g_loadPadInputs = runtime.lookupFunction(kLoadPadInputs)) && (g_origMonsterInit = runtime.lookupFunction(kMonsterInit)) &&
            (g_origAirMotion = runtime.lookupFunction(kAirMotion)) && (g_origAiInit = runtime.lookupFunction(kAiInit)))
        {
            runtime.registerFunction(kAiUpdateInputs, &aiUpdateInputsFromPad);
            runtime.registerFunction(kMonsterInit, &monsterInitKeepsSpecial);
            runtime.registerFunction(kAirMotion, &airMotionAsPlayer);
            runtime.registerFunction(kAiInit, &aiInitKeepsPadPlayers);
            std::fprintf(stderr, "[multi] jogadores 5 a %d nas IAs 1 a %d, pelos controles 5 a %d\n", 4 + g_padAIs, g_padAIs, 4 + g_padAIs);
        }
        if (!perPlayerViews())
            return;
        if ((g_origReestablish = runtime.lookupFunction(kReestablishViews)) && (g_origInitCC = runtime.lookupFunction(kInitCC)) &&
            (g_origInitCCThrow = runtime.lookupFunction(kInitCCThrow)) && (g_origToggleSplit = runtime.lookupFunction(kToggleSplit)))
        {
            runtime.registerFunction(kReestablishViews, &reestablishViewsPerPlayer);
            runtime.registerFunction(kInitCC, &skipWithPlayerViews<kInitCC, &g_origInitCC>);
            runtime.registerFunction(kInitCCThrow, &skipWithPlayerViews<kInitCCThrow, &g_origInitCCThrow>);
            runtime.registerFunction(kToggleSplit, &skipWithPlayerViews<kToggleSplit, &g_origToggleSplit>);
            std::fprintf(stderr, "[multi] 2 jogadores, uma vista por jogador no viewport 0 (WOTM_LOCALPLAYER)\n");
        }
        else
            std::fprintf(stderr, "[multi] funcoes do jogo faltando no runtime: vista por jogador indisponivel\n");
    }
}
