// wotm_modifiers.inc: gameplay modifiers (net/wotm_modifiers.hpp) inside the runtime. Included by wotm_overrides.cpp.
//
// The active set is latched once per fight (wotm_sync::g_mods, set by wotm_sync::levelLoad at Shell::LoadLevelFiles) and cleared on
// entry of userintMain, where the game goes back to its menus after a session (a pause, the end-of-fight dialogs and RESTART stay in
// the session: rtReturnToShell is not the end of a fight). So a change in the options panel only reaches the next fight and the menus
// run at normal speed. In a network match the latch happens at the same frame index on every PC and the set is in the config hash; a
// network match never goes back to the menus (QUIT closes the game).
//  - Super Jump: TheGame::m_gravity x0.45. Its only writer is TheGame::InitBeforeDbLoad (one swc1), so the scale goes on entry of
//    InitAfterDbLoad, which follows it. Global gravity keeps the game's own jump-height predictions (StateJump/StateFly::
//    getJumpHeightGain) and throw aims consistent with the physics; per-monster m_gravityScale would not.
//  - Fast Monsters: walk/run take their target speed and acceleration in MonsterDynamics::updateFootMotion($f12, $f13): speed
//    x kFastSpeed, acceleration x kFastAccel (full speed almost at once). Dash and ram attack compare against
//    MonsterDynamics::getSpeedModifier: x kFastSpeed. Every animation clip of a fight monster (TheGame::m_monsters, its character
//    instance named by Monster::m_anims) plays at x kFastAnim, so attacks, their hit windows and anything timed off the clip follow.
//    A clip's time is startTime + fields * deltaTime (_animControlNode), so its speed is deltaTime: animationStart,
//    animationStartReverse and animationTransitionInto scale it on entry (rebased, no jump in time) unless already scaled;
//    animationSetSpeed (with animationResetSpeed) and AnimBlend::setSpeed scale the new speed on the way in. Field-counted timers
//    (stun, cooldowns, launch), blend durations, the physics step, the camera, menus and every other character run at normal speed.
//  - Turbo: fixed step -> wotm_sync::frame and the frame wait (wotm_overrides.cpp) use fieldsPerFrame/vsyncsPerFrame; offline ->
//    the host clock the game sees runs 2x (setGameClockScale).
//  - WOTM_MODSTATS=1: player 1's highest rise (cs trans z from where its vertical velocity turned positive to where it stopped
//    rising: a jump's height, not the stage's) and peak ground speed (|vel.xy|) since the fight start, every 300 frames; and per
//    rise its height and the gravity it felt (vertical velocity lost per second over frames 1-6 after takeoff: releasing the jump
//    button damps the rise later on, so the height depends on the input but this does not). For scripts/modcheck.sh (coord).
namespace wotm_modifiers
{
    namespace md = wotm::modifiers;
    constexpr uint32_t kInitAfterDbLoad = wotm::fn::InitAfterDbLoad__7TheGame, kUserintMain = wotm::fn::userintMain__Fv;
    constexpr uint32_t kFootMotion = wotm::fn::updateFootMotion__15MonsterDynamicsfff;
    constexpr uint32_t kSpeedModifier = wotm::fn::getSpeedModifier__15MonsterDynamics;
    constexpr uint32_t kGameGravity = 0x1203C4u, kGameMonsters = 0x120380u;   // TheGame::m_gravity, m_monsters[0] (include/game/game.h)
    constexpr uint32_t kMonsterCs = 0xCu, kCsTrans = 0x10u, kMonsterVel = 0x1E0u;   // Monster::m_cs, _cs::trans, m_dynamics.m_body.vel
    constexpr uint32_t kAnimStart = wotm::fn::animationStart__FG11_animHandleb, kAnimStartReverse = wotm::fn::animationStartReverse__FG11_animHandleb,
                       kAnimTransition = wotm::fn::animationTransitionInto__FR11_animHandlefii,
                       kAnimSetSpeed = wotm::fn::animationSetSpeed__FG11_animHandlef, kBlendSetSpeed = wotm::fn::setSpeed__9AnimBlendf;
    constexpr uint32_t kMaxMonsters = 8u, kMonsterAnims = 0x1CF0u, kNumMonsterAnims = 0x12Cu, kHandleSize = 0x10u;   // TheGame::m_monsters[8], Monster::m_anims
    // _animControlNode (include/hieri_types.h): the clip's clock and whether it runs
    constexpr uint32_t kClipStartField = 0x14u, kClipStartTime = 0x18u, kClipDeltaTime = 0x1Cu, kClipActive = 0x28u;
    PS2Runtime::RecompiledFunction g_origInitAfterDbLoad = nullptr, g_origUserintMain = nullptr, g_origFootMotion = nullptr,
                                   g_origSpeedModifier = nullptr, g_origAnimStart = nullptr, g_origAnimStartReverse = nullptr,
                                   g_origAnimTransition = nullptr, g_origAnimSetSpeed = nullptr, g_origBlendSetSpeed = nullptr;
    uint32_t g_gravityWritten = 0;   // bits of the last gravity written: a second InitAfterDbLoad without a new base never scales twice
    bool g_stats = false;
    float g_takeoffZ = 0.f, g_maxRise = 0.f, g_maxSpeed = 0.f;
    bool g_rising = false;
    uint32_t g_riseFrames = 0;   // frames since takeoff
    float g_vz1 = 0.f, g_riseAccel = 0.f;

    bool on(int bit) { return (wotm_sync::g_mods & bit) != 0; }

    // Fast Monsters: |deltaTime| as the animation hooks last wrote it per control node: any other value means the game (or a reload of
    // the monster's data) set a new base speed since. animationSetDirection only flips the sign, so magnitudes are compared. It decides
    // whether a clip gets scaled, so it is simulation state: emptied at every fight start, so a node address reused from an earlier
    // fight (or a fight only one PC played) can't make two PCs disagree.
    std::unordered_map<uint32_t, float> g_fastClips;

    // Shell::LoadLevelFiles entry (wotm_sync::levelLoad): latch the panel's (or the lobby's) set for this fight; an offline replay
    // (WOTM_INPUTPLAY) plays the set its recording was made with
    void fightStart()
    {
        const bool recorded = !wotm_sync::g_net && wotm_sync::g_inputPlayMods >= 0;
        wotm_sync::g_mods = recorded ? wotm_sync::g_inputPlayMods : wotm::settings::modifiers();
        g_rising = false, g_maxRise = 0.f, g_maxSpeed = 0.f;
        g_fastClips.clear();
        if (!wotm_sync::g_fixedStep)
            setGameClockScale(on(md::Turbo) ? 2u : 1u);
        std::fprintf(stderr, "[modifiers] fight: \"%s\"\n", md::format(wotm_sync::g_mods).c_str());
    }

    void userintMainModsOff(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kUserintMain && wotm_sync::g_mods)
        {
            wotm_sync::g_mods = 0;
            if (!wotm_sync::g_fixedStep)
                setGameClockScale(1u);
            std::fprintf(stderr, "[modifiers] off (back to the menus)\n");
        }
        g_origUserintMain(rdram, ctx, runtime);
    }

    void initAfterDbLoadGravity(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kInitAfterDbLoad && on(md::Jump))
        {
            const uint32_t game = READ32(wotm::sym::game) & 0x1FFFFFFFu;
            if (ramOk(game + kGameGravity, 4u))
            {
                uint32_t bits = READ32(game + kGameGravity);
                if (bits != g_gravityWritten)
                {
                    float g;
                    std::memcpy(&g, &bits, 4);
                    const float moon = g * md::kJumpGravity;
                    std::memcpy(&g_gravityWritten, &moon, 4);
                    WRITE32(game + kGameGravity, g_gravityWritten);
                    std::fprintf(stderr, "[modifiers] super jump: gravity %.1f -> %.1f\n", g, moon);
                }
            }
        }
        g_origInitAfterDbLoad(rdram, ctx, runtime);
    }

    void footMotionFast(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kFootMotion && on(md::Fast))
        {
            ctx->f[12] *= md::kFastSpeed;   // target speed
            ctx->f[13] *= md::kFastAccel;   // acceleration: the faster speed comes almost at once
        }
        g_origFootMotion(rdram, ctx, runtime);
    }

    // Is `ci` (an _animCharInstance) the character of one of the fight's monsters? A monster's handles (m_anims) all name its
    // character instance; the first valid one tells which.
    bool monsterInstance(uint8_t *rdram, uint32_t ci)
    {
        const uint32_t game = FAST_READ32(wotm::sym::game) & 0x1FFFFFFFu;
        if (!ci || !ramOk(game + kGameMonsters, 4u * kMaxMonsters))
            return false;
        for (uint32_t i = 0; i < kMaxMonsters; ++i)
        {
            const uint32_t mon = FAST_READ32(game + kGameMonsters + 4u * i) & 0x1FFFFFFFu;
            if (!mon || !ramOk(mon + kMonsterAnims, kNumMonsterAnims * kHandleSize))
                continue;
            for (uint32_t a = 0; a < kNumMonsterAnims; ++a)
                if (const uint32_t own = FAST_READ32(mon + kMonsterAnims + a * kHandleSize))
                {
                    if (own == ci)
                        return true;
                    break;
                }
        }
        return false;
    }

    // Keeps a clip's speed at kFastAnim x the game's own while Fast is on and the clip is a monster's, and gives it back when not.
    void rescaleClip(uint8_t *rdram, uint32_t ctl, float factor)
    {
        md::ClipClock c{FAST_READ32(ctl + kClipStartField), rdF(rdram, ctl + kClipStartTime), rdF(rdram, ctl + kClipDeltaTime)};
        md::rescaleClip(c, rdram[ctl + kClipActive] != 0, FAST_READ32(kFieldCounter), factor);
        uint32_t t, d;
        std::memcpy(&t, &c.startTime, 4), std::memcpy(&d, &c.deltaTime, 4);
        Ps2FastWrite32(rdram, ctl + kClipStartField, c.startField), Ps2FastWrite32(rdram, ctl + kClipStartTime, t), Ps2FastWrite32(rdram, ctl + kClipDeltaTime, d);
    }

    // `handle`: address of an _animHandle (ci, control node)
    void fastClip(uint8_t *rdram, uint32_t handle)
    {
        handle &= 0x1FFFFFFFu;
        if (!ramOk(handle, 8u))
            return;
        const uint32_t ci = FAST_READ32(handle), ctl = FAST_READ32(handle + 4u) & 0x1FFFFFFFu;
        if (!ctl || !ramOk(ctl, kClipActive + 1u))
            return;
        const bool want = on(md::Fast) && monsterInstance(rdram, ci & 0x1FFFFFFFu);
        if (!want && g_fastClips.empty())
            return;
        const auto rec = g_fastClips.find(ctl);
        const bool scaled = rec != g_fastClips.end() && rec->second == std::fabs(rdF(rdram, ctl + kClipDeltaTime));
        if (want && !scaled)
        {
            rescaleClip(rdram, ctl, md::kFastAnim);
            g_fastClips[ctl] = std::fabs(rdF(rdram, ctl + kClipDeltaTime));
        }
        else if (!want && rec != g_fastClips.end())
        {
            if (scaled)
                rescaleClip(rdram, ctl, 1.f / md::kFastAnim);
            g_fastClips.erase(rec);
        }
    }

    // Before the clip starts or blends in, so the start time and the blend's copy of the clock see the scaled speed
    template <const uint32_t &kEntry, PS2Runtime::RecompiledFunction &orig>
    void animStartFast(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kEntry)
            fastClip(rdram, GPR_U32(ctx, 4));
        orig(rdram, ctx, runtime);
    }

    // A call about to write $f12 as the new base speed of `handle`'s clip: scale it on the way in
    void fastSpeedArg(uint8_t *rdram, R5900Context *ctx, uint32_t handle)
    {
        handle &= 0x1FFFFFFFu;
        const uint32_t ctl = handle && ramOk(handle, 8u) ? FAST_READ32(handle + 4u) & 0x1FFFFFFFu : 0u;
        if (!ctl)
            return;
        g_fastClips.erase(ctl);
        if (on(md::Fast) && monsterInstance(rdram, FAST_READ32(handle) & 0x1FFFFFFFu))
        {
            ctx->f[12] *= md::kFastAnim;
            g_fastClips[ctl] = std::fabs(ctx->f[12]);
        }
    }

    // animationSetSpeed (and animationResetSpeed, which calls it with 1) rebases the clip and writes the speed
    void animSpeedFast(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kAnimSetSpeed)
            fastSpeedArg(rdram, ctx, GPR_U32(ctx, 4));
        g_origAnimSetSpeed(rdram, ctx, runtime);
    }

    // AnimBlend::setSpeed writes the speed straight into its layer's clip and blend node (AnimBlend::m_handle at +0x4)
    void blendSpeedFast(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t blend = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        if (ctx->pc == kBlendSetSpeed && ramOk(blend, 8u))
            fastSpeedArg(rdram, ctx, FAST_READ32(blend + 4u));
        g_origBlendSetSpeed(rdram, ctx, runtime);
    }

    // a leaf function (no waits): it always returns here, so its $f0 can be scaled after the call
    void speedModifierFast(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kSpeedModifier;
        const uint32_t ra = GPR_U32(ctx, 31);
        g_origSpeedModifier(rdram, ctx, runtime);
        if (entry && ctx->pc == ra && on(md::Fast))
            ctx->f[0] *= md::kFastSpeed;
    }

    // WOTM_MODSTATS: once per frame (waitMinRateByVblanks)
    void statsFrame(uint8_t *rdram)
    {
        if (!g_stats || !wotm_sync::g_levelLoaded)
            return;
        const uint32_t game = FAST_READ32(wotm::sym::game) & 0x1FFFFFFFu;
        if (!ramOk(game + kGameMonsters, 4u))
            return;
        const uint32_t mon = FAST_READ32(game + kGameMonsters) & 0x1FFFFFFFu;
        if (!ramOk(mon, kMonsterVel + 12u))
            return;
        const uint32_t cs = FAST_READ32(mon + kMonsterCs) & 0x1FFFFFFFu;
        if (!ramOk(cs + kCsTrans, 12u))
            return;
        const float z = rdF(rdram, cs + kCsTrans + 8u), vx = rdF(rdram, mon + kMonsterVel), vy = rdF(rdram, mon + kMonsterVel + 4u),
                    vz = rdF(rdram, mon + kMonsterVel + 8u);
        if (vz > 0.f && !g_rising)
            g_takeoffZ = z, g_rising = true, g_riseFrames = 0, g_riseAccel = 0.f;
        else if (vz <= 0.f && g_rising)
        {
            g_rising = false;
            if (z - g_takeoffZ > 20.f)   // each real rise (not the ground's jitter)
                std::fprintf(stderr, "[modstats] rise %.1f gravity %.1f at frame %u\n", z - g_takeoffZ, g_riseAccel, wotm_sync::g_index);
        }
        if (g_rising)
        {
            ++g_riseFrames;
            if (g_riseFrames == 1u)
                g_vz1 = vz;
            else if (g_riseFrames == 6u)
                g_riseAccel = (vz - g_vz1) / 5.f * 60.f / static_cast<float>(wotm::modifiers::fieldsPerFrame(std::max(wotm_sync::g_fixedStep, 1), wotm_sync::g_mods));
        }
        if (g_rising)
            g_maxRise = std::max(g_maxRise, z - g_takeoffZ);
        g_maxSpeed = std::max(g_maxSpeed, std::sqrt(vx * vx + vy * vy));
        if (wotm_sync::g_index % 300u == 0u)
            std::fprintf(stderr, "[modstats] frame %u peak_height %.1f peak_speed %.1f\n", wotm_sync::g_index, g_maxRise, g_maxSpeed);
    }

    void install(PS2Runtime &runtime)
    {
        g_stats = std::getenv("WOTM_MODSTATS") && std::getenv("WOTM_MODSTATS")[0] == '1';
        if ((g_origInitAfterDbLoad = runtime.lookupFunction(kInitAfterDbLoad)))
            runtime.registerFunction(kInitAfterDbLoad, &initAfterDbLoadGravity);
        if ((g_origUserintMain = runtime.lookupFunction(kUserintMain)))   // after the direct match's userintMain (WOTM_MATCH), which it chains
            runtime.registerFunction(kUserintMain, &userintMainModsOff);
        if ((g_origFootMotion = runtime.lookupFunction(kFootMotion)))
            runtime.registerFunction(kFootMotion, &footMotionFast);
        if ((g_origSpeedModifier = runtime.lookupFunction(kSpeedModifier)))
            runtime.registerFunction(kSpeedModifier, &speedModifierFast);
        if ((g_origAnimStart = runtime.lookupFunction(kAnimStart)))
            runtime.registerFunction(kAnimStart, &animStartFast<kAnimStart, g_origAnimStart>);
        if ((g_origAnimStartReverse = runtime.lookupFunction(kAnimStartReverse)))
            runtime.registerFunction(kAnimStartReverse, &animStartFast<kAnimStartReverse, g_origAnimStartReverse>);
        if ((g_origAnimTransition = runtime.lookupFunction(kAnimTransition)))
            runtime.registerFunction(kAnimTransition, &animStartFast<kAnimTransition, g_origAnimTransition>);
        if ((g_origAnimSetSpeed = runtime.lookupFunction(kAnimSetSpeed)))
            runtime.registerFunction(kAnimSetSpeed, &animSpeedFast);
        if ((g_origBlendSetSpeed = runtime.lookupFunction(kBlendSetSpeed)))
            runtime.registerFunction(kBlendSetSpeed, &blendSpeedFast);
    }
}
