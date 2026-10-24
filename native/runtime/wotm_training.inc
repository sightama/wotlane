// Modo treino (POS_FRONTEIRA.md, item 9; WOTM_TRAINING=1 ou F2 na luta): ganchos na thread do EE que gravam o que o jogo testa nos
// golpes e o dano de cada um. Incluido pelo wotm_overrides.cpp; estado compartilhado em native/wotm_training.h; os volumes do corpo
// sao lidos no wotmCaptureFrame (wotm_live.inc) e o desenho e o menu F2 estao no wotm_live.inc. Investigacoes:
// docs/investigacoes/2026-10-23/treino_hitboxes.md e treino_dano.md.
//
// - Golpes: o jogo nao tem caixa de golpe. Cada teste e uma esfera varrida (hdSphereTest, do ponto do osso no quadro anterior ate o
//   atual) contra os cascos de colisao. Os ganchos marcam o "teste de golpe em curso" (soco/combo: collisTestCloseRangeAttack; golpe
//   com objeto longo: Pickup::collisTestBatAttack; ataque ao levantar: StateGetupAttack::update; projetil: Weapon::WeaponHD; onda
//   do butt slam: ShockWave::update) e, so com a marca, o hdSphereTest (varrido e parado) grava a esfera e se ela pegou algo. Fora
//   da marca o hdSphereTest e da camera e da fisica e fica de fora.
// - Dano: Monster::takeHit guarda o dano pedido e o source do HitEvent; Monster::takeDamage mede a vida antes e depois (o valor exato,
//   com bloqueio, furia e dificuldade). Fogo, choque e batida no cenario chegam so pelo takeDamage.
// - Boneco: as IAs (slots >= m_numPlayers) com o pad desligado (Monster +0xF9, o setPadEnabled) ficam paradas mas recuam, caem e sao
//   agarradas. Recarga: vida cheia depois de 1,5 s sem dano, e um golpe que mataria enche a vida antes (ninguem morre no treino).

#include "wotm_training.h"

namespace wotm_train
{
    using namespace wotm_training;
    constexpr uint32_t kGameObj = wotm::sym::shellGame, kShellObj = wotm::sym::shellInfo;
    constexpr uint32_t kSlots = 0x120380u;             // TheGame::m_monsters (lugar para 16)
    constexpr uint32_t kMaxHealth = 0x448u, kHealth = 0x44Cu, kPadEnabled = 0xF9u, kGetupState = 0x10C40u;
    constexpr uint32_t kTakeHitInfo = wotm::sym::x_8HitEvent_s_takeHitInfo;   // pos* +0, source +0x10

    constexpr uint32_t kMonsterUpdate = wotm::fn::update__7Monster, kTakeHit = wotm::fn::takeHit__7MonsterP8_fvectorfi,
                       kTakeDamage = wotm::fn::takeDamage__7MonsterfbP7Monster,
                       kPunchTest = wotm::fn::collisTestCloseRangeAttack__20PunchSwipeConfigBaseiP7MonsterR10HitHistoryi,
                       kBatTest = wotm::fn::collisTestBatAttack__6PickupfUi11MonsterAnimR10HitHistoryfff,
                       kGetupUpdate = wotm::fn::update__16StateGetupAttack, kWeaponHD = wotm::fn::WeaponHD__6Weapon,
                       kShockUpdate = wotm::fn::update__9ShockWave, kSphereSwept = wotm::fn::hdSphereTest__FP8_fvectorT0fP9_hdResultUii,
                       kSphereStatic = wotm::fn::hdSphereTest__FP8_fvectorfP9_hdResultUii;

    PS2Runtime::RecompiledFunction g_origUpdate = nullptr, g_origTakeHit = nullptr, g_origTakeDamage = nullptr, g_origPunch = nullptr,
                                   g_origBat = nullptr, g_origGetup = nullptr, g_origWeapon = nullptr, g_origShock = nullptr,
                                   g_origSwept = nullptr, g_origStatic = nullptr;

    // estado da thread do EE
    int g_ctxKind = -1;         // teste de golpe em curso (Kind) ou -1
    int g_ctxOwner = -1;
    uint32_t g_curMonster = 0;  // Monster::update em curso
    bool g_inStatic = false;
    struct HitCtx
    {
        uint32_t victim = 0;
        float base = 0.f;
        int source = -1;
    } g_hitCtx;
    uint32_t g_lastDamage[16] = {};   // g_frame do ultimo dano por slot
    uint32_t g_padOff = 0;            // slots com o pad desligado por aqui

    inline bool ok(uint32_t a, uint32_t n) { return a >= 0x1000u && a + n <= PS2_RAM_SIZE; }
    inline uint32_t rd32(const uint8_t *rdram, uint32_t a) { uint32_t v; std::memcpy(&v, rdram + a, 4); return v; }
    inline float rdf(const uint8_t *rdram, uint32_t a) { float v; std::memcpy(&v, rdram + a, 4); return v; }
    inline void wrf(uint8_t *rdram, uint32_t a, float v) { std::memcpy(rdram + a, &v, 4); }
    inline uint32_t frameNow(const uint8_t *rdram) { return rd32(rdram, wotm::sym::g_frame); }

    int slotOf(const uint8_t *rdram, uint32_t m)
    {
        m &= 0x1FFFFFFFu;
        if (!m)
            return -1;
        for (uint32_t i = 0; i < 16; ++i)
            if ((rd32(rdram, kGameObj + kSlots + 4 * i) & 0x1FFFFFFFu) == m)
                return int(i);
        return -1;
    }

    int numPlayers(const uint8_t *rdram)
    {
        const int32_t p = static_cast<int32_t>(rd32(rdram, kShellObj + 0x2BB4));   // m_numPlayers
        return p >= 1 && p <= 4 ? p : 1;
    }

    // a funcao terminou (nao foi interrompida pelo escalonador no meio): o pc voltou ao ra da entrada
    inline bool returned(R5900Context *ctx, uint32_t ra) { return ctx->pc == ra; }

    // ---- golpes

    void pushSweep(const uint8_t *rdram, const float a[3], const float b[3], float r, uint8_t hit)
    {
        if (!std::isfinite(a[0] + a[1] + a[2] + b[0] + b[1] + b[2] + r) || r < 0.f || r > 5000.f)
            return;
        Sweep s{};
        std::memcpy(s.a, a, sizeof s.a);
        std::memcpy(s.b, b, sizeof s.b);
        s.r = r;
        s.frame = frameNow(rdram);
        s.kind = uint8_t(g_ctxKind);
        s.owner = int8_t(g_ctxOwner);
        s.hit = hit;
        Shared &sh = shared();
        std::lock_guard<std::mutex> lk(sh.m);
        if (sh.sweeps.size() < 4096)
            sh.sweeps.push_back(s);
    }

    uint8_t classify(const uint8_t *rdram, uint32_t res, int n)
    {
        uint8_t h = 0;
        res &= 0x1FFFFFFFu;
        for (int i = 0; i < n && i < 100 && ok(res + 0x30u * uint32_t(i), 0x30u); ++i)
        {
            const uint32_t r = res + 0x30u * uint32_t(i);
            if (!rdram[r])   // _hdResult::hit
                continue;
            h = std::max<uint8_t>(h, (rd32(rdram, r + 4) & 0x7FFu) ? 2 : 1);
        }
        return h;
    }

    void readVec(const uint8_t *rdram, uint32_t p, float out[3])
    {
        p &= 0x1FFFFFFFu;
        if (!ok(p, 12)) { out[0] = out[1] = out[2] = NAN; return; }
        for (int i = 0; i < 3; ++i)
            out[i] = rdf(rdram, p + 4 * i);
    }

    // hdSphereTest(from, to, r, res, skipId, max): esfera varrida
    void sphereSwept(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc != kSphereSwept || g_ctxKind < 0 || g_inStatic || !g_on.load(std::memory_order_relaxed))
        {
            g_origSwept(rdram, ctx, runtime);
            return;
        }
        float a[3], b[3];
        readVec(rdram, GPR_U32(ctx, 4), a);
        readVec(rdram, GPR_U32(ctx, 5), b);
        const float r = ctx->f[12];
        const uint32_t res = GPR_U32(ctx, 6), ra = GPR_U32(ctx, 31);
        g_origSwept(rdram, ctx, runtime);
        const int n = returned(ctx, ra) ? int(GPR_S32(ctx, 2)) : 0;
        pushSweep(rdram, a, b, r, classify(rdram, res, n));
    }

    // hdSphereTest(pos, r, res, skipId, max): esfera parada
    void sphereStatic(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc != kSphereStatic || g_ctxKind < 0 || !g_on.load(std::memory_order_relaxed))
        {
            g_origStatic(rdram, ctx, runtime);
            return;
        }
        float a[3];
        readVec(rdram, GPR_U32(ctx, 4), a);
        const float r = ctx->f[12];
        const uint32_t res = GPR_U32(ctx, 5), ra = GPR_U32(ctx, 31);
        g_inStatic = true;
        g_origStatic(rdram, ctx, runtime);
        g_inStatic = false;
        const int n = returned(ctx, ra) ? int(GPR_S32(ctx, 2)) : 0;
        pushSweep(rdram, a, a, r, classify(rdram, res, n));
    }

    // marca o teste de golpe em curso durante a chamada original
    void withCtx(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, PS2Runtime::RecompiledFunction orig, int kind, int owner)
    {
        const int savedKind = g_ctxKind, savedOwner = g_ctxOwner;
        g_ctxKind = kind;
        g_ctxOwner = owner;
        orig(rdram, ctx, runtime);
        g_ctxKind = savedKind;
        g_ctxOwner = savedOwner;
    }

    void punchTest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)   // a2 = Monster atacante
    {
        if (ctx->pc != kPunchTest || !g_on.load(std::memory_order_relaxed)) { g_origPunch(rdram, ctx, runtime); return; }
        withCtx(rdram, ctx, runtime, g_origPunch, KPunch, slotOf(rdram, GPR_U32(ctx, 6)));
    }
    void batTest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)     // no Monster::update de quem segura
    {
        if (ctx->pc != kBatTest || !g_on.load(std::memory_order_relaxed)) { g_origBat(rdram, ctx, runtime); return; }
        withCtx(rdram, ctx, runtime, g_origBat, KBat, slotOf(rdram, g_curMonster));
    }
    void getupUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)  // a0 = o estado, dentro do Monster
    {
        if (ctx->pc != kGetupUpdate || !g_on.load(std::memory_order_relaxed)) { g_origGetup(rdram, ctx, runtime); return; }
        withCtx(rdram, ctx, runtime, g_origGetup, KGetup, slotOf(rdram, GPR_U32(ctx, 4) - kGetupState));
    }
    void weaponHD(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc != kWeaponHD || !g_on.load(std::memory_order_relaxed)) { g_origWeapon(rdram, ctx, runtime); return; }
        withCtx(rdram, ctx, runtime, g_origWeapon, KWeapon, -1);
    }
    void shockUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)  // +0x20: o dono (Monster), quando e um dos slots
    {
        if (ctx->pc != kShockUpdate || !g_on.load(std::memory_order_relaxed)) { g_origShock(rdram, ctx, runtime); return; }
        const uint32_t self = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        withCtx(rdram, ctx, runtime, g_origShock, KShock, ok(self, 0x24) ? slotOf(rdram, rd32(rdram, self + 0x20)) : -1);
    }

    // ---- dano e boneco

    void takeHit(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc != kTakeHit || !g_on.load(std::memory_order_relaxed)) { g_origTakeHit(rdram, ctx, runtime); return; }
        const HitCtx saved = g_hitCtx;
        g_hitCtx.victim = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        g_hitCtx.base = ctx->f[12];
        g_hitCtx.source = static_cast<int32_t>(rd32(rdram, kTakeHitInfo + 0x10u));
        g_origTakeHit(rdram, ctx, runtime);
        g_hitCtx = saved;
    }

    void takeDamage(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)   // a0 vitima, f12 dano, a1 force, a2 fonte (Monster*)
    {
        const uint32_t m = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        const int slot = ctx->pc == kTakeDamage && g_on.load(std::memory_order_relaxed) && ok(m, 0x450u) ? slotOf(rdram, m) : -1;
        if (slot < 0)
        {
            g_origTakeDamage(rdram, ctx, runtime);
            return;
        }
        const float dmg = ctx->f[12], maxH = rdf(rdram, m + kMaxHealth);
        float before = rdf(rdram, m + kHealth);
        // recarga: um golpe que mataria enche a vida antes (o "vencer a luta" do F1, 100000, passa); 1,35 cobre a dificuldade do difícil
        if ((g_opts.load(std::memory_order_relaxed) & kRefill) && dmg < 10000.f && maxH > 0.f && before <= dmg * 1.35f + 0.5f)
        {
            wrf(rdram, m + kHealth, maxH);
            before = maxH;
        }
        const uint32_t ra = GPR_U32(ctx, 31), src = GPR_U32(ctx, 6);
        g_origTakeDamage(rdram, ctx, runtime);
        if (!returned(ctx, ra))
            return;
        const float after = rdf(rdram, m + kHealth);
        if (!std::isfinite(before) || !std::isfinite(after) || before - after < 0.001f)
            return;
        Hit h{};
        h.frame = frameNow(rdram);
        h.victim = int8_t(slot);
        h.attacker = int8_t(slotOf(rdram, src));
        const bool inHit = g_hitCtx.victim == m;
        h.base = inHit ? g_hitCtx.base : -1.f;
        h.source = inHit ? g_hitCtx.source : -1;
        h.applied = before - after;
        h.before = before;
        h.max = maxH;
        g_lastDamage[slot] = h.frame;
        static uint32_t logged = 0;
        if (++logged <= 30 || logged % 100 == 0)
            std::fprintf(stderr, "[treino] dano %.2f em slot %d (de %d; pedido %.2f, source %d), vida %.1f -> %.1f de %.0f (%u)\n", h.applied,
                         slot, h.attacker, h.base, h.source, before, after, maxH, logged);
        Shared &sh = shared();
        std::lock_guard<std::mutex> lk(sh.m);
        // dano continuo (fogo, choque: um pouco por quadro, sem takeHit) vira um registro so enquanto continua
        if (!inHit && !sh.hits.empty())
        {
            Hit &p = sh.hits.back();
            if (p.source < 0 && p.victim == h.victim && p.attacker == h.attacker && h.frame - p.frame <= 3u)
            {
                p.applied += h.applied;
                p.frame = h.frame;
                return;
            }
        }
        if (sh.hits.size() < 1024)
            sh.hits.push_back(h);
    }

    void monsterUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        if (ctx->pc != kMonsterUpdate || !ok(m, 0x450u))
        {
            g_origUpdate(rdram, ctx, runtime);
            return;
        }
        const int slot = slotOf(rdram, m);
        const bool on = g_on.load(std::memory_order_relaxed);
        if (slot >= 0)
        {
            const uint32_t bit = 1u << slot;
            const bool dummy = on && slot >= numPlayers(rdram) && g_dummy.load(std::memory_order_relaxed) == 0;
            if (dummy)
            {
                rdram[m + kPadEnabled] = 0;   // sem entradas: a IA nao age; os estados (recuo, queda, agarrao) seguem
                g_padOff |= bit;
            }
            else if (g_padOff & bit)
            {
                rdram[m + kPadEnabled] = 1;
                g_padOff &= ~bit;
            }
            if (on && (g_opts.load(std::memory_order_relaxed) & kRefill))
            {
                const float h = rdf(rdram, m + kHealth), mx = rdf(rdram, m + kMaxHealth);
                if (std::isfinite(h) && mx > 0.f && h < mx && h > 0.f && frameNow(rdram) - g_lastDamage[slot] > 90u)
                    wrf(rdram, m + kHealth, mx);
            }
        }
        const uint32_t saved = g_curMonster;
        g_curMonster = m;
        g_origUpdate(rdram, ctx, runtime);
        g_curMonster = saved;
    }
}

void wotmTrainingInstall(PS2Runtime &runtime)
{
    using namespace wotm_train;
    if (const char *e = std::getenv("WOTM_TRAINING"); e && *e && *e != '0')
        g_on = true;
    if (const char *e = std::getenv("WOTM_TRAINING_DUMMY"); e && *e)
        g_dummy = std::atoi(e) ? 1 : 0;
    struct H { uint32_t at; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; const char *name; };
    const H hooks[] = {
        {kMonsterUpdate, &g_origUpdate, &monsterUpdate, "Monster::update"},
        {kTakeHit, &g_origTakeHit, &takeHit, "Monster::takeHit"},
        {kTakeDamage, &g_origTakeDamage, &takeDamage, "Monster::takeDamage"},
        {kPunchTest, &g_origPunch, &punchTest, "collisTestCloseRangeAttack"},
        {kBatTest, &g_origBat, &batTest, "Pickup::collisTestBatAttack"},
        {kGetupUpdate, &g_origGetup, &getupUpdate, "StateGetupAttack::update"},
        {kWeaponHD, &g_origWeapon, &weaponHD, "Weapon::WeaponHD"},
        {kShockUpdate, &g_origShock, &shockUpdate, "ShockWave::update"},
        {kSphereSwept, &g_origSwept, &sphereSwept, "hdSphereTest (varrido)"},
        {kSphereStatic, &g_origStatic, &sphereStatic, "hdSphereTest (parado)"},
    };
    int n = 0;
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.at)))
        {
            runtime.registerFunction(h.at, h.fn);
            ++n;
        }
        else
            std::fprintf(stderr, "[treino] sem gancho: %s\n", h.name);
    std::fprintf(stderr, "[treino] %d de %zu ganchos; modo treino %s (WOTM_TRAINING ou F2 na luta)\n", n, sizeof hooks / sizeof hooks[0],
                 g_on.load() ? "LIGADO" : "desligado");
}
