// ABOUTME: Per-player views for 2 players with WOTM_LOCALPLAYER (both views in viewport 0), and players 5..8 as AIs driven by
// ABOUTME: pads 4..7 (WOTM_PLAYERS=5..8). The 3- and 4-player quadrants are in wotm_split4.inc. Included by wotm_overrides.cpp.
//
// 2 players with WOTM_LOCALPLAYER (each on their own PC sees their own view fullscreen): TheGame::gameReestablishViews in mode 3 becomes
// viewSetNumViews(2) and viewCreate(0, v) for both, like retail mode 8 does with two views in viewport 0; the fullscreen view keeps the
// 4:3 aspect, so the game's projection, clipping, HUD and cameras stay correct, without the 2:3 halves of retail split screen.
namespace wotm_multi
{
    constexpr uint32_t kReestablishViews = wotm::fn::gameReestablishViews__7TheGame;
    constexpr uint32_t kGameMode = kGame + 0x1203C8, kMonsters = kGame + 0x120380;
    constexpr uint32_t kCameras = wotm::sym::x_7Cameras_m_cameras, kCameraStride = 0xEB0, kNumCameras = wotm::sym::x_7Cameras_m_numCameras;

    PS2Runtime::RecompiledFunction g_origReestablish = nullptr;

    // One view in viewport 0 per player and no unified view: 2 players with WOTM_LOCALPLAYER
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
            std::fprintf(stderr, "[multi] %u views in viewport 0, %u cameras\n", n, n);
        }
        ctx->pc = GPR_U32(ctx, 31);
    }

    // ---- Players 5..8 (WOTM_PLAYERS=5..8, g_padAIs): monsters in AIs m_monsters[4..7] driven by pads 4..7 ----
    // Human and AI fill the same monster GamePad (+0x5024, 0x1C bytes) and PadFlags::interpretInputs reads both the same way; the
    // Ai::updateInputs of these AIs becomes the game's own GamePad::loadPadInputs(pad, 4 + m_index) (retail debug mode Ai::s_mode 3
    // does the same with pad 0). They stay marked as AI (m_playerNum 2): as players they would fall into tables sized for 4 and
    // for 2 (PlayerDat, sound events, tokens, HUD and camera). What only the AI has is corrected: aiInit clears the special weapon (we keep
    // the one Monster::init gave, as in playerInit), the Ai damage factor (+0x152C) becomes 1, and updateAirMotion doesn't double the
    // air acceleration (it runs with m_playerNum 1 only during the call). Free-for-all movement doesn't depend on a camera (CR map in
    // PadFlags), so they don't need one. Investigation: docs/plans/8p-investigation.md (local).
    constexpr uint32_t kAiUpdateInputs = wotm::fn::updateInputs__2Ai, kLoadPadInputs = wotm::fn::loadPadInputs__7GamePadi,
                       kMonsterInit = wotm::fn::init__7Monster, kAirMotion = wotm::fn::updateAirMotion__15MonsterDynamicsff;
    constexpr uint32_t kMonPlayerNum = 0x18, kMonIndex = 0x24, kMonCs = 0xC, kMonSpecial = 0xF3, kMonAi = 0x4E0, kAiPad = 0x44,
                       kAiDamageFactor = 0x152C, kMonDynamics = 0x100;
    PS2Runtime::RecompiledFunction g_origAiUpdateInputs = nullptr, g_loadPadInputs = nullptr, g_origMonsterInit = nullptr,
                                   g_origAirMotion = nullptr, g_origAiInit = nullptr;
    uint32_t g_padInitMonster = 0;   // aiInit in progress for a player AI: Monster::init stores the special weapon it gave
    uint8_t g_padInitSpecial = 0;

    // index (0..g_padAIs-1) of the player AI that this monster is, or -1
    int padAI(uint8_t *rdram, uint32_t m)
    {
        if (!m || g_padAIs <= 0)
            return -1;
        const uint32_t k = FAST_READ32(m + kMonIndex);
        return k < static_cast<uint32_t>(g_padAIs) && FAST_READ32(kMonsters + 4 * (4 + k)) == m ? static_cast<int>(k) : -1;
    }

    // Proof that the pad reaches the monster (tools/net_check.ps1): from the first non-neutral input of each player 5..8, how far the
    // monster moved 2 s of game time (g_frame) later
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
        std::fprintf(stderr, "[multi] player %d moved %.0f units in %u frames since the first input on pad %d\n", 5 + k,
                     std::sqrt(dx * dx + dy * dy + dz * dz), g - pm.startFrame, 5 + k);
    }

    void aiUpdateInputsFromPad(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kAiUpdateInputs)
        {
            const uint32_t ai = GPR_U32(ctx, 4), m = FAST_READ32(ai);
            if (const int k = padAI(rdram, m); k >= 0)
            {   // loadPadInputs in place of the body of updateInputs: returns straight to Monster::update
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

    // Monster::aiInit of a player AI (spawn and respawn): keeps the special weapon and a damage factor of 1
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

    // Close combat: Cameras::InitCCCamera/InitCCCameraFromThrow turn on gUseUnifiedView and put camera 2 on POV 1; with the unified
    // view viewGetNumViews returns 1 and the other player has no screen. With one view per player they do nothing.
    // Cinematics (doCinema/endCinema) toggle split screen (viewToggleSplitScreen), which with 2 views goes back to one or to the 2:3 halves;
    // with one view per player the gameReestablishViews views stay.
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
            std::fprintf(stderr, "[multi] players 5 to %d on AIs 1 to %d, via pads 5 to %d\n", 4 + g_padAIs, g_padAIs, 4 + g_padAIs);
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
            std::fprintf(stderr, "[multi] 2 players, one view per player in viewport 0 (WOTM_LOCALPLAYER)\n");
        }
        else
            std::fprintf(stderr, "[multi] game functions missing from the runtime: per-player view unavailable\n");
    }
}
