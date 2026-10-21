// ABOUTME: Partida com 3 ou 4 jogadores (WOTM_MATCH + WOTM_PLAYERS=3/4): carga dos modelos dos jogadores 3/4, figurino, cameras e
// ABOUTME: uma vista por jogador, todas no viewport 0 (a camada nativa compoe em 2x2). Incluido por wotm_overrides.cpp.
//
// O caminho do retail para os jogadores 3/4 nao funciona: o Shell::LoadMonstersDB monta o caminho deles com o monstro do jogador 2
// e o LoadTexture/LoadResTexture sem o digito do figurino (todos os arquivos de \MON tem), e o Shell::InitPlayers da a eles o
// figurino do jogador 2. As IAs carregam certo (formatFilename1 com m_costume[2 + j]), entao na entrada do Shell::LoadLevelFiles o
// shell passa a dizer "2 jogadores + IAs [J3, J4, IAs...]" e na entrada do Shell::InitPlayers volta aos N jogadores: a ordem de
// carga (J1, J2, J3, J4, IAs) e a numeracao dos monstros repetidos sao as mesmas de uma carga de 4 jogadores. So ganchos de
// entrada: as duas funcoes podem bloquear (leitura do disco) e ser retomadas no meio sem passar por aqui.
// Vistas (TheGame::gameReestablishViews, trocado inteiro com 3+ jogadores no modo 3): viewSetNumViews(N) e viewCreate(0, v) para
// cada jogador, como o modo 8 do retail faz com duas vistas no viewport 0; um quarto da tela tem a mesma proporcao 4:3 da tela
// cheia, entao projecao, corte, HUD e cameras do jogo continuam certos. A camera 2 deixa de ser a do combate de perto (POV 3) e
// as cameras 2 e 3 passam a usar o _cs da vista delas. TheGame::SetAIMonster prende a camera 3 numa IA quando ha 2+ monstros:
// o gancho do gameInitCamera recusa camera de jogador para monstro que nao e jogador.
namespace wotm_multi
{
    constexpr uint32_t kLoadLevelFiles = wotm::fn::LoadLevelFiles__5Shell, kInitPlayers = wotm::fn::InitPlayers__5Shell,
                       kSetPlayerMonster = wotm::fn::SetPlayerMonster__7TheGameiiiii, kGameInitCamera = wotm::fn::gameInitCamera__7TheGameii,
                       kReestablishViews = wotm::fn::gameReestablishViews__7TheGame;
    constexpr uint32_t kSel = kShell + 0x2938, kCostume = kShell + 0x2BE0, kNumPlayers = kShell + 0x2BB4, kNumAIs = kShell + 0x2BB8;
    constexpr uint32_t kGameMode = kGame + 0x1203C8, kMonsters = kGame + 0x120380, kSlots = kGame + 0xB80, kSlotStride = 0x11190;
    constexpr uint32_t kCameras = wotm::sym::x_7Cameras_m_cameras, kCameraStride = 0xEB0, kNumCameras = wotm::sym::x_7Cameras_m_numCameras;
    constexpr uint32_t kViewInfo = wotm::sym::viewInfo, kViewInfoStride = 0x14;

    PS2Runtime::RecompiledFunction g_origLoad = nullptr, g_origInitPlayers = nullptr, g_origSetPlayer = nullptr, g_origInitCamera = nullptr,
                                   g_origReestablish = nullptr;
    bool g_loadingAsAIs = false;   // o shell esta dizendo "2 jogadores + IAs" (entre o LoadLevelFiles e o InitPlayers)

    bool active() { return g_matchPlayers > 2; }
    // Uma vista no viewport 0 por jogador e sem vista unificada: 3+ jogadores, ou 2+ com WOTM_LOCALPLAYER (cada um no seu PC ve a sua
    // vista em tela cheia, sem a metade 2:3 da tela dividida do retail)
    bool perPlayerViews()
    {
        static const bool local = [] { const char *e = std::getenv("WOTM_LOCALPLAYER"); return e && std::atoi(e) >= 1; }();
        return active() || (local && g_matchPlayers >= 2);
    }

    void loadLevelFilesAsAIs(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kLoadLevelFiles && active() && !g_loadingAsAIs)
        {
            const int extra = g_matchPlayers - 2, ais = g_matchAIs;
            for (int j = ais - 1; j >= 0; --j)   // IAs andam `extra` posicoes para dar lugar aos jogadores 3/4
            {
                WRITE32(kSel + 4 * (4 + extra + j), READ32(kSel + 4 * (4 + j)));
                WRITE32(kCostume + 4 * (2 + extra + j), READ32(kCostume + 4 * (2 + j)));
            }
            for (int k = 0; k < extra; ++k)
            {
                WRITE32(kSel + 4 * (4 + k), READ32(kSel + 4 * (2 + k)));
                WRITE32(kCostume + 4 * (2 + k), g_matchCostume[2 + k]);
            }
            WRITE32(kNumPlayers, 2u);
            WRITE32(kNumAIs, static_cast<uint32_t>(ais + extra));
            g_loadingAsAIs = true;
            std::fprintf(stderr, "[multi] carga: jogadores 3..%d carregados como as primeiras IAs\n", g_matchPlayers);
        }
        g_origLoad(rdram, ctx, runtime);
    }

    void initPlayersRestored(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kInitPlayers && g_loadingAsAIs)
        {
            const int extra = g_matchPlayers - 2, ais = g_matchAIs;
            for (int j = 0; j < ais; ++j)
            {
                WRITE32(kSel + 4 * (4 + j), READ32(kSel + 4 * (4 + extra + j)));
                WRITE32(kCostume + 4 * (2 + j), READ32(kCostume + 4 * (2 + extra + j)));
            }
            for (int j = ais; j < ais + extra; ++j)
            {
                WRITE32(kSel + 4 * (4 + j), 0u);
                WRITE32(kCostume + 4 * (2 + j), 0u);
            }
            WRITE32(kNumPlayers, static_cast<uint32_t>(g_matchPlayers));
            WRITE32(kNumAIs, static_cast<uint32_t>(ais));
            g_loadingAsAIs = false;
        }
        g_origInitPlayers(rdram, ctx, runtime);
    }

    // TheGame::SetPlayerMonster(i, sel, dup, view, skin): o InitPlayers passa m_costume[i != 0]; jogadores 3/4 recebem o deles (t1)
    void setPlayerMonsterCostume(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t i = GPR_U32(ctx, 5);
        if (ctx->pc == kSetPlayerMonster && active() && i >= 2 && i < static_cast<uint32_t>(g_matchPlayers))
            ctx->r[9] = _mm_set_epi64x(0, static_cast<int64_t>(g_matchCostume[i]));
        g_origSetPlayer(rdram, ctx, runtime);
    }

    bool isPlayerSlot(uint8_t *rdram, uint32_t slot)
    {
        const uint32_t m = kSlots + slot * kSlotStride;
        for (int i = 0; i < g_matchPlayers; ++i)
            if (FAST_READ32(kMonsters + 4 * i) == m)
                return true;
        return false;
    }

    // TheGame::gameInitCamera(view, slot): camera de jogador (2 ou 3) so segue jogador
    void gameInitCameraPlayersOnly(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t view = GPR_U32(ctx, 5), slot = GPR_U32(ctx, 6);
        if (ctx->pc == kGameInitCamera && active() && view >= 2 && view < static_cast<uint32_t>(g_matchPlayers) && !isPlayerSlot(rdram, slot))
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origInitCamera(rdram, ctx, runtime);
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
        for (uint32_t v = 2; v < n; ++v)
        {
            const uint32_t cs = READ32(kViewInfo + v * kViewInfoStride), cam = kCameras + v * kCameraStride;
            WRITE32(cam, cs);
            WRITE32(cam + 0xCF0, cs);
        }
        WRITE32(kNumCameras, n);
        for (uint32_t i = 0; i < n; ++i)
        {
            const uint32_t pov = i < 2 ? READ32(kCameras + i * kCameraStride + 0xCF8) : 0u;
            wotm_save::guestCall(rdram, ctx, runtime, wotm::fn::SetCameraPOV__7CamerasiQ26Camera9CameraPOV, {i, pov});
        }
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            std::fprintf(stderr, "[multi] %u vistas no viewport 0, %u cameras\n", n, n);
        }
        ctx->pc = GPR_U32(ctx, 31);
    }

    // Fim de rodada no modo 3. Com IAs, o Shell::EvaluateMultiPlayerBattleStatusAI reergue os derrubados com `slot < 2 ? playerInit :
    // aiInit`, e os jogadores 3/4 (slots 2 e 3) voltavam como IA: o Monster::aiInit de um jogador vira playerInit (sem WOTM_AIVSAI).
    // Sem IAs, o ...NoAI so reergue alguem quando sobra 1 ou nenhum vivo: com 3+ jogadores a rodada vai para o ...AI, que com
    // m_killTarget 0 (WOTM_MATCH) reergue todos os derrubados.
    constexpr uint32_t kEvalNoAI = wotm::fn::EvaluateMultiPlayerBattleStatusNoAI__5Shelli, kEvalAI = wotm::fn::EvaluateMultiPlayerBattleStatusAI__5Shelli;
    PS2Runtime::RecompiledFunction g_origAiInit = nullptr, g_playerInit = nullptr, g_origEvalNoAI = nullptr, g_evalAI = nullptr;

    bool isPlayer(uint8_t *rdram, uint32_t m)
    {
        for (int i = 0; i < g_matchPlayers; ++i)
            if (m && FAST_READ32(kMonsters + 4 * i) == m)
                return true;
        return false;
    }

    void aiInitKeepsPlayers(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kAiInit && active() && !g_aiVsAi && isPlayer(rdram, GPR_U32(ctx, 4)))
        {
            ctx->pc = kPlayerInit;
            g_playerInit(rdram, ctx, runtime);
            return;
        }
        g_origAiInit(rdram, ctx, runtime);
    }

    void evaluateNoAIRespawnsAll(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kEvalNoAI && active())
        {
            ctx->pc = kEvalAI;
            g_evalAI(rdram, ctx, runtime);
            return;
        }
        g_origEvalNoAI(rdram, ctx, runtime);
    }

    // Combate de perto: Cameras::InitCCCamera/InitCCCameraFromThrow ligam gUseUnifiedView e poem a camera 2 no POV 1; com a vista
    // unificada o viewGetNumViews devolve 1 e 3 dos 4 jogadores ficam sem tela. Com 3+ jogadores nao fazem nada.
    // As cinemas (doCinema/endCinema) alternam a tela dividida (viewToggleSplitScreen), que com 2 vistas volta a uma ou as metades 2:3;
    // com uma vista por jogador ficam as vistas do gameReestablishViews.
    constexpr uint32_t kInitCC = wotm::fn::InitCCCamera__7CamerasP7MonsterT1, kInitCCThrow = wotm::fn::InitCCCameraFromThrow__7CamerasP7MonsterT1,
                       kToggleSplit = wotm::fn::viewToggleSplitScreen__FbT0;
    PS2Runtime::RecompiledFunction g_origInitCC = nullptr, g_origInitCCThrow = nullptr, g_origToggleSplit = nullptr;

    // Mesmo gancho para a alternancia da tela dividida (as cinemas): com uma vista por jogador, ela tambem nao faz nada.
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
        if (!perPlayerViews())
            return;
        if ((g_origAiInit = runtime.lookupFunction(kAiInit)) && (g_playerInit = runtime.lookupFunction(kPlayerInit)) &&
            (g_origEvalNoAI = runtime.lookupFunction(kEvalNoAI)) && (g_evalAI = runtime.lookupFunction(kEvalAI)) &&
            (g_origInitCC = runtime.lookupFunction(kInitCC)) && (g_origInitCCThrow = runtime.lookupFunction(kInitCCThrow)))
        {
            runtime.registerFunction(kAiInit, &aiInitKeepsPlayers);
            runtime.registerFunction(kEvalNoAI, &evaluateNoAIRespawnsAll);
            runtime.registerFunction(kInitCC, &skipWithPlayerViews<kInitCC, &g_origInitCC>);
            runtime.registerFunction(kInitCCThrow, &skipWithPlayerViews<kInitCCThrow, &g_origInitCCThrow>);
        }
        if ((g_origToggleSplit = runtime.lookupFunction(kToggleSplit)))
            runtime.registerFunction(kToggleSplit, &skipWithPlayerViews<kToggleSplit, &g_origToggleSplit>);
        if ((g_origLoad = runtime.lookupFunction(kLoadLevelFiles)) && (g_origInitPlayers = runtime.lookupFunction(kInitPlayers)) &&
            (g_origSetPlayer = runtime.lookupFunction(kSetPlayerMonster)) && (g_origInitCamera = runtime.lookupFunction(kGameInitCamera)) &&
            (g_origReestablish = runtime.lookupFunction(kReestablishViews)))
        {
            runtime.registerFunction(kLoadLevelFiles, &loadLevelFilesAsAIs);
            runtime.registerFunction(kInitPlayers, &initPlayersRestored);
            runtime.registerFunction(kSetPlayerMonster, &setPlayerMonsterCostume);
            runtime.registerFunction(kGameInitCamera, &gameInitCameraPlayersOnly);
            runtime.registerFunction(kReestablishViews, &reestablishViewsPerPlayer);
            std::fprintf(stderr, "[multi] %d jogadores (WOTM_PLAYERS)\n", g_matchPlayers);
        }
        else
            std::fprintf(stderr, "[multi] funcoes do jogo faltando no runtime: %d jogadores indisponivel\n", g_matchPlayers);
    }
}
