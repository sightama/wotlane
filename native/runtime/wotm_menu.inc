// wotm_menu.inc: the main menu's "press start" (screen 20, screenWaitForStart). Included by wotm_overrides.cpp.
//
// In retail the screen only accepts Start, and only after the transition that opens it ends: on entry (16 -> 20) the scene fades
// in from black while the camera pulls back from the big screen (animation 1, 120 frames), and screenGetInput returns 0 while
// betweenScreens != 0, i.e. ~2 s of ignored buttons. Here:
//  - X counts as Start: on screen 20, screenGetInput's code 6 (X) becomes 0x10 (Start);
//  - Start/X already work during the transition to screen 20: screenWaitForStart itself runs right away (the first-pass setup,
//    the changeScreen(20 -> 1) and the sound), with Start injected. Animation 1 keeps going (the camera finishes its move) and
//    the 20 -> 1 transition uses animation 0, as on the normal path, so the options appear with the usual fade (40 frames).
//    The press only counts once the button has been released inside the transition (the X that skipped the movie does not
//    enter the menu) and only acts after the previous screen's fade-out (20 frames); until then it is held.
// WOTM_MENULOG=1: logs the screens, the transition and the presses.
namespace wotm_menu
{
constexpr uint32_t kGetInput = wotm::fn::screenGetInput__Fi;
constexpr uint32_t kTransition = wotm::fn::screenTransition__Fb;
constexpr uint32_t kWaitStart = wotm::fn::screenWaitForStart__Fv;
constexpr uint32_t kPressStart = 20;   // currScreen of the "press start"
constexpr uint32_t kCodeStart = 0x10, kCodeX = 6;   // screenGetInput action codes

static PS2Runtime::RecompiledFunction g_origGetInput = nullptr, g_origTransition = nullptr;
static uint32_t g_inRa = 0;
static bool g_inPending = false;
static bool g_force = false;     // the next screenGetInput returns Start (skip via screenWaitForStart)
static bool g_probing = false;   // pad read done by the transition hook
static bool g_armed = false;     // the button has already been released inside the transition to screen 20
static bool g_queued = false;    // press held until the previous screen's fade-out ends
static bool g_log = false, g_readyLogged = false;
static uint32_t g_realignUntil = 0;   // g_frame up to which the legend icons follow the camera (early Start)

static uint32_t rd(const uint8_t *rdram, uint32_t a)
{
    uint32_t v;
    std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 4);
    return v;
}
static void wr(uint8_t *rdram, uint32_t a, uint32_t v) { std::memcpy(rdram + (a & 0x1FFFFFFu), &v, 4); }

// Calls the game function at `fn` (the current hook chain) and returns $v0; the context is restored as it was
static uint32_t callGuest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, uint32_t fn, uint32_t a0)
{
    const PS2Runtime::RecompiledFunction f = rt->lookupFunction(fn);
    if (!f) return 0;
    const R5900Context saved = *ctx;
    ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a0)));
    ctx->r[31] = _mm_set_epi64x(0, 0);
    ctx->pc = fn;
    f(rdram, ctx, rt);
    const uint32_t v0 = GPR_U32(ctx, 2);
    *ctx = saved;
    return v0;
}

// showButtons aligns the legend icons to the camera only during the fade frames. With the early Start the camera (animation 1)
// is still moving after the fade ends, and the icons would stay stuck where it was: here they are realigned every frame until
// the animation ends, with the same values showButtons uses (val1_x for m_buttonCs[3], val2_x for [1]).
static void realignButtons(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    if (rd(rdram, wotm::sym::g_frame) > g_realignUntil)
        return;
    const PS2Runtime::RecompiledFunction f = rt->lookupFunction(wotm::fn::uiAlignItemWithCamera__FP3_csfff);
    const uint32_t sh = rd(rdram, wotm::sym::shell);
    if (!f || !sh)
        return;
    const uint32_t cs[2] = {rd(rdram, sh + 0x60 + 3 * 4), rd(rdram, sh + 0x60 + 1 * 4)};
    const uint32_t xs[2] = {wotm::sym::val1_x_3061, wotm::sym::val2_x_3064};
    for (int i = 0; i < 2; i++)
    {
        if (!cs[i] || !rdram[(cs[i] + 0xC) & 0x1FFFFFFu])   // drawMe
            continue;
        const R5900Context saved = *ctx;
        ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(cs[i])));
        std::memcpy(&ctx->f[12], rdram + wotm::sym::val_z_3063, 4);
        std::memcpy(&ctx->f[13], rdram + xs[i], 4);
        std::memcpy(&ctx->f[14], rdram + wotm::sym::val_y_3062, 4);
        ctx->r[31] = _mm_set_epi64x(0, 0);
        ctx->pc = wotm::fn::uiAlignItemWithCamera__FP3_csfff;
        f(rdram, ctx, rt);
        *ctx = saved;
    }
}

static void getInputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kGetInput)
    {
        if (!g_probing && !g_force)
            realignButtons(rdram, ctx, runtime);
        if (g_force)
        {
            g_force = false;
            ctx->r[2] = _mm_set_epi64x(0, kCodeStart);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if (rd(rdram, wotm::sym::currScreen) == kPressStart)
        {
            g_inRa = GPR_U32(ctx, 31);
            g_inPending = true;
            if (g_log && !g_probing && !g_readyLogged && rd(rdram, wotm::sym::betweenScreens) == 0)
            {
                g_readyLogged = true;
                std::fprintf(stderr, "[menu] g_frame %u: press start accepts input\n", rd(rdram, wotm::sym::g_frame));
            }
        }
    }
    g_origGetInput(rdram, ctx, runtime);
    if (!g_inPending || ctx->pc != g_inRa)
        return;
    g_inPending = false;
    const uint32_t v = GPR_U32(ctx, 2);
    if (g_log && !g_probing && (v & 0xFFFFu))
        std::fprintf(stderr, "[menu] g_frame %u: press start, code %u\n", rd(rdram, wotm::sym::g_frame), v & 0xFFFFu);
    if ((v & 0xFFFFu) == kCodeX)
        ctx->r[2] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>((v & ~0xFFFFu) | kCodeStart)));
}

// Start/X during the transition to screen 20. Returns true if the "press start" was handled (this frame's transition does not run).
static bool pressDuringTransition(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (rd(rdram, wotm::sym::screenFirstPass))   // first pass: a button still held (from the movie, for example) does not count
    {
        g_armed = false;
        g_queued = false;
    }
    if (!g_queued)
    {
        // screenGetInput only reads the pad outside the transition (betweenScreens == 0)
        g_probing = true;
        wr(rdram, wotm::sym::betweenScreens, 0);
        const uint32_t code = callGuest(rdram, ctx, runtime, kGetInput, 1) & 0xFFFFu;
        wr(rdram, wotm::sym::betweenScreens, 1);
        g_probing = false;
        if (code == 0)
            g_armed = true;
        else if (g_armed && code == kCodeStart)   // X already arrives as Start through getInputHook
            g_queued = true;
        if (g_queued && g_log)
            std::fprintf(stderr, "[menu] g_frame %u: Start/X during the transition to press start\n", rd(rdram, wotm::sym::g_frame));
    }
    if (!g_queued || rd(rdram, wotm::sym::fadingOut_2980))
        return false;
    g_queued = false;
    // screenWaitForStart as at the end of the transition: first pass (selections, pads, hides m_menuCs), big-screen movie,
    // and the injected Start does the changeScreen(20 -> 1) and the sound
    wr(rdram, wotm::sym::betweenScreens, 0);
    wr(rdram, wotm::sym::screenFirstPass, 1);
    g_force = true;
    callGuest(rdram, ctx, runtime, kWaitStart, 0);
    g_force = false;
    // 20 -> 1 transition with animation 0, as when the previous one finishes; animation 1 (camera) continues on its own
    wr(rdram, wotm::sym::currAnimationIndex, 0);
    g_realignUntil = rd(rdram, wotm::sym::g_frame) + 150;   // margin over animation 1's 120 frames
    if (g_log)
        std::fprintf(stderr, "[menu] g_frame %u: early press start, screen %u\n", rd(rdram, wotm::sym::g_frame), rd(rdram, wotm::sym::currScreen));
    return true;
}

static void transitionHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kTransition)
    {
        if (g_log)
        {
            static uint32_t lastKey = ~0u;
            const uint32_t cur = rd(rdram, wotm::sym::currScreen), last = rd(rdram, wotm::sym::lastScreen);
            const uint32_t fo = rd(rdram, wotm::sym::fadingOut_2980), fi = rd(rdram, wotm::sym::fadingIn_2979);
            const uint32_t key = cur << 16 | last << 8 | fo << 1 | fi;
            if (key != lastKey)
            {
                lastKey = key;
                g_readyLogged = false;
                float a;
                std::memcpy(&a, rdram + wotm::sym::targetAlpha, 4);
                std::fprintf(stderr, "[menu] g_frame %u: transition %u -> %u, anim %u, fadeOut %u fadeIn %u, alpha %.2f\n", rd(rdram, wotm::sym::g_frame), last,
                             cur, rd(rdram, wotm::sym::currAnimationIndex), fo, fi, a);
            }
        }
        if (rd(rdram, wotm::sym::currScreen) == kPressStart && rd(rdram, wotm::sym::betweenScreens) &&
            pressDuringTransition(rdram, ctx, runtime))
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
    }
    g_origTransition(rdram, ctx, runtime);
    realignButtons(rdram, ctx, runtime);
}

static void install(PS2Runtime &runtime)
{
    g_log = std::getenv("WOTM_MENULOG") != nullptr;
    if ((g_origGetInput = runtime.lookupFunction(kGetInput)))
        runtime.registerFunction(kGetInput, &getInputHook);
    if ((g_origTransition = runtime.lookupFunction(kTransition)) && runtime.lookupFunction(kWaitStart))
        runtime.registerFunction(kTransition, &transitionHook);
}
} // namespace wotm_menu
