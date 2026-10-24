// Monstros custom (MODS.md, "Monstros custom"): monstros que o jogo nao oferece na selecao, jogados numa luta comum. O
// primeiro e o Goliath Prime (tipo 13, "assboss", MON/ASSBOSS1.*), chefe da aventura em canyon2. Incluido pelo wotm_overrides.cpp,
// depois do wotm_skins.inc. Investigacao: git show 18bb1d2:docs/investigacoes/2026-10-23/goliath_codigo.md (e goliath_selecao.md).
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

    // Som: o SoundManager::loadMonsterBank (e o de baixa resolucao) nao tem caso para o 0x1A0; os sons do Goliath sao do banco do
    // chefe (gSoundBanks[BANK_MECH], Assemb_s.bnk), que o retail so carrega na cena do chefe em canyon2 (canyon2BossIntro). Fora da
    // aventura, o pedido do banco do 0x1A0 vira o SoundManager::loadMechBossSoundBank (mesmo this), como um salto.
    constexpr uint32_t kLoadBank = wotm::fn::loadMonsterBank__12SoundManagerii, kLoadLoBank = wotm::fn::loadLoResMonsterBank__12SoundManagerii,
                       kLoadMech = wotm::fn::loadMechBossSoundBank__12SoundManager;
    constexpr uint32_t kGameMode = 0x1203C8;
    PS2Runtime::RecompiledFunction g_origLoadBank = nullptr, g_origLoadLoBank = nullptr, g_loadMech = nullptr;

    void loadBankCommon(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t entry, PS2Runtime::RecompiledFunction orig)
    {
        if (ctx->pc == entry && GPR_U32(ctx, 5) == kGoliath && READ32(kGameObj + kGameMode) != 1u && g_loadMech)
        {
            std::fprintf(stderr, "[custom] banco de som do Goliath (Assemb_s.bnk, loadMechBossSoundBank)\n");
            // nome na tela ("%s CRUSHED", "%s WINS"): a entrada 13 das 5 copias do MonsterNewNames[17][12] diz "assboss"; fica o
            // "name" do monster.json (ate 11 letras, minusculas como as do jogo) ou "goliath"
            std::string shown = "goliath";
            if (const wotm::mods::Custom *cu = wotm::mods::Registry::get().customByType(13))
            {
                std::string n = cu->name;
                if (n.size() > 11)   // "GOLIATH PRIME" -> "goliath": corta na ultima palavra que cabe
                {
                    const size_t sp = n.rfind(' ', 11);
                    n = n.substr(0, sp != std::string::npos && sp > 0 ? sp : 11);
                }
                shown.clear();
                for (char ch : n)
                    shown += char(std::tolower(static_cast<unsigned char>(ch)));
            }
            for (const uint32_t names : {wotm::sym::MonsterNewNames_006E8958, wotm::sym::MonsterNewNames_006E8D90, wotm::sym::MonsterNewNames_006EAA78,
                                         wotm::sym::MonsterNewNames_006ED318, wotm::sym::MonsterNewNames_006EFEC8})
            {
                char *e = reinterpret_cast<char *>(rdram + ((names + 13 * 12) & 0x01FFFFFFu));
                std::memset(e, 0, 12);
                std::memcpy(e, shown.data(), shown.size());
            }
            ctx->pc = kLoadMech;
            g_loadMech(rdram, ctx, runtime);
            return;
        }
        orig(rdram, ctx, runtime);
    }
    void loadBank(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { loadBankCommon(rdram, ctx, runtime, kLoadBank, g_origLoadBank); }
    void loadLoBank(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { loadBankCommon(rdram, ctx, runtime, kLoadLoBank, g_origLoadLoBank); }

    // Camera: o RST/ASSBOSS.CHR traz parametros de camera que o retail nunca usou (no POV 0 a camera fica na frente e baixa:
    // m_00 = (-70, 336, 90) contra (0, -255, 150) do Togera). No Monster::initAfterDbLoad -> MonsterTweaks::setActiveMonster(m, true),
    // que le o RST/<MonsterLongNames[tipo]>.CHR (charreset::setCharacter monta o nome), o Goliath le antes o .CHR do monstro base da
    // camera, guarda a tabela de camera (+0x6CF0: 10 POVs de 0xA0, mais 10 da tela dividida), le o proprio .CHR de novo e fica com
    // a tabela guardada, com as tres distancias de cada POV (m_00, m_10, m_20) escaladas.
    constexpr uint32_t kSetActive = wotm::fn::setActiveMonster__13MonsterTweaksP7Monsterb;
    constexpr uint32_t kLongNames = wotm::sym::MonsterLongNames_006E8BC8;   // [17][8], a copia do charreset
    constexpr uint32_t kCamTable = 0x6CF0, kCamPovs = 20, kCamPov = 0xA0;
    // Sem monster.json (WOTM_MATCH=13): a do Togera em dobro. Com ele, a do "camera" do mod ({"from", "scale"}).
    constexpr const char *kCamBaseDefault = "lizard";
    constexpr float kCamScaleDefault = 2.0f;
    PS2Runtime::RecompiledFunction g_origSetActive = nullptr;

    void setActive(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        const uint32_t type = ctx->pc == kSetActive ? READ32(m + kTypeBits) >> 5 : 0;
        const wotm::mods::Custom *cu = type ? wotm::mods::Registry::get().customByType(int(type)) : nullptr;
        const char *kCamBase = cu && !cu->cameraFrom.empty() ? cu->cameraFrom.c_str() : type == 13 && !cu ? kCamBaseDefault : nullptr;
        const float kCamScale = cu && !cu->cameraFrom.empty() ? cu->cameraScale : kCamScaleDefault;
        if (!kCamBase || type >= 17)
        {
            g_origSetActive(rdram, ctx, runtime);
            return;
        }
        const R5900Context entry = *ctx;
        char *name = reinterpret_cast<char *>(rdram + ((kLongNames + type * 8) & 0x01FFFFFFu));
        char own[8];
        std::memcpy(own, name, 8);
        std::memset(name, 0, 8);
        std::memcpy(name, kCamBase, std::min<size_t>(7, std::strlen(kCamBase)));
        g_origSetActive(rdram, ctx, runtime);
        std::vector<uint8_t> cam(kCamPovs * kCamPov);
        std::memcpy(cam.data(), rdram + ((m + kCamTable) & 0x01FFFFFFu), cam.size());
        std::memcpy(name, own, 8);
        *ctx = entry;
        g_origSetActive(rdram, ctx, runtime);
        for (uint32_t p = 0; p < kCamPovs; ++p)
            for (uint32_t v = 0; v < 3; ++v)
                for (uint32_t c = 0; c < 3; ++c)
                {
                    float f;
                    std::memcpy(&f, &cam[p * kCamPov + v * 16 + c * 4], 4);
                    f *= kCamScale;
                    std::memcpy(&cam[p * kCamPov + v * 16 + c * 4], &f, 4);
                }
        std::memcpy(rdram + ((m + kCamTable) & 0x01FFFFFFu), cam.data(), cam.size());
        std::fprintf(stderr, "[custom] %s: camera do %s.CHR, distancias x%.2f\n", cu ? cu->id.c_str() : own, kCamBase, kCamScale);
    }
    // Dardo (StateJavelin): o Weapon::DetonateWeapon chama o Monster::attachPickupImpaler na vitima, que entra no StateImpaled pelo
    // enterNewState sem o transitionOK (o que barraria quem nao tem as anims 0x8E..0x90). O StateImpaled::update so sai com a anim
    // 0x90 no fim; sem ela (ASSBOSS0/1) a vitima travava para sempre. Sem essas anims, o gancho faz o que o retail faz com pickup
    // invalido: killPickup(it, 1) e volta. O golpe do objeto (dispatchHit) chega como um comum. Investigacao: 18bb1d2:docs/investigacoes/2026-10-23/goliath_impale.md.
    constexpr uint32_t kImpale = wotm::fn::attachPickupImpaler__7MonsterGQ2t10LinkedList1ZP6Pickup8IteratorP8_fvector,
                       kKillPickup = wotm::fn::killPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori;
    constexpr uint32_t kAnims = 0x1CF0, kAnimStride = 0x10;   // Monster::m_anims[id].ci
    PS2Runtime::RecompiledFunction g_origImpale = nullptr, g_killPickup = nullptr;

    void impale(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        bool has = true;
        if (ctx->pc == kImpale && g_killPickup)
            for (uint32_t id = 0x8E; id <= 0x90; ++id)
                has = has && READ32(m + kAnims + id * kAnimStride) != 0;
        if (has)
        {
            g_origImpale(rdram, ctx, runtime);
            return;
        }
        std::fprintf(stderr, "[custom] dardo: monstro tipo %u sem as anims de empalado; vira golpe comum\n", READ32(m + kTypeBits) >> 5);
        ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(GPR_U32(ctx, 5))));
        ctx->r[5] = _mm_set_epi64x(0, 1);
        ctx->pc = kKillPickup;
        g_killPickup(rdram, ctx, runtime);
    }
    // Giro (StateTopSpin, Monster+0x10EBC; investigacao 18bb1d2:docs/investigacoes/2026-10-23/goliath_topspin.md). Nos monstros comuns X+Quadrado (PadFlags +0x38) e o
    // StateCrowdControl (+0xF3F0), que no Goliath nunca comeca (pede a anim de efeito do tipo, que ele nao tem). So a IA do chefe
    // entrava no TopSpin, pelo setAssBossState(12). Com o Goliath jogador:
    //  - StateCrowdControl::transitionOK: X+Quadrado no chao (Idle ou Run), com ataques ligados, sem objeto grande e o
    //    transitionOK do TopSpin aceito;
    //  - Monster::enterNewState: o CrowdControl vira o TopSpin;
    //  - StateTopSpin::update: o golpe cheio (dano, empurrao, som) so acerta m_monsters[0], que e ele mesmo; durante a chamada vale a
    //    IA viva mais perto;
    //  - Ai::setAssBossState (chamado pelo update a cada batida): so faz algo para o chefe da aventura (o dono e o assBoss).
    constexpr uint32_t kCrowdOK = wotm::fn::transitionOK__17StateCrowdControl, kEnterState = wotm::fn::enterNewState__7MonsterP12MonsterState,
                       kSpinUpdate = wotm::fn::update__12StateTopSpin, kSpinOK = wotm::fn::transitionOK__12StateTopSpin,
                       kSetBossState = wotm::fn::setAssBossState__2Aii, kHoldingLarge = wotm::fn::isHoldingLarge__7Monster,
                       kPadFrame = wotm::fn::x__vc__8PadFlagsi;
    constexpr uint32_t kStateOwner = 0xC, kCurState = 0x34, kIdle = 0x7984, kRun = 0x79AC, kCrowd = 0xF3F0, kSpin = 0x10EBC;
    constexpr uint32_t kAttacksOn = 0x4A, kPad = 0x5040, kPadSpin = 0x38, kDead = 0xE8, kNumMonsters = 0x1203D8, kTrans = 0x10;
    PS2Runtime::RecompiledFunction g_origCrowdOK = nullptr, g_origEnterState = nullptr, g_origSpinUpdate = nullptr, g_origSetBossState = nullptr;

    bool goliathPlayer(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t m)
    {
        return m && READ32(m + kTypeBits) == kGoliath && isPlayerSlot(rdram, ctx, runtime, m);
    }
    void ret(R5900Context *ctx, uint32_t v0)
    {
        ctx->r[2] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(v0)));
        ctx->pc = GPR_U32(ctx, 31);
    }

    void crowdOK(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = ctx->pc == kCrowdOK ? READ32(GPR_U32(ctx, 4) + kStateOwner) : 0;
        if (!goliathPlayer(rdram, ctx, runtime, m))
        {
            g_origCrowdOK(rdram, ctx, runtime);
            return;
        }
        using wotm_modes4::call;
        const uint32_t st = READ32(m + kCurState);
        bool ok = READ8(m + kAttacksOn) != 0 && (st == m + kIdle || st == m + kRun);
        ok = ok && READ16(call(rdram, ctx, runtime, kPadFrame, {m + kPad, 0u}) + kPadSpin) != 0;
        ok = ok && call(rdram, ctx, runtime, kHoldingLarge, {m}) == 0 && call(rdram, ctx, runtime, kSpinOK, {m + kSpin}) != 0;
        ret(ctx, ok ? 1u : 0u);
    }

    void enterState(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        if (ctx->pc == kEnterState && GPR_U32(ctx, 5) == m + kCrowd && goliathPlayer(rdram, ctx, runtime, m))
        {
            ctx->r[5] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(m + kSpin)));
            std::fprintf(stderr, "[custom] Goliath: giro (X+Quadrado)\n");
        }
        g_origEnterState(rdram, ctx, runtime);
    }

    void spinUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = ctx->pc == kSpinUpdate ? READ32(GPR_U32(ctx, 4) + kStateOwner) : 0;
        uint32_t near = 0;
        if (goliathPlayer(rdram, ctx, runtime, m))
        {
            const uint32_t cs = READ32(m + kCs), n = std::min(8u, READ32(kGameObj + kNumMonsters));
            float best = 1e30f, p[3], q[3];
            std::memcpy(p, rdram + ((cs + kTrans) & 0x01FFFFFFu), 12);
            for (uint32_t i = 0; i < n && cs; ++i)
            {
                const uint32_t o = READ32(kGameObj + kMonsters + 4 * i), ocs = o ? READ32(o + kCs) : 0;
                if (!ocs || o == m || READ32(o + kPlayerNum) != 2u || READ8(o + kDead))
                    continue;
                std::memcpy(q, rdram + ((ocs + kTrans) & 0x01FFFFFFu), 12);
                const float d = (q[0] - p[0]) * (q[0] - p[0]) + (q[1] - p[1]) * (q[1] - p[1]) + (q[2] - p[2]) * (q[2] - p[2]);
                if (d < best)
                    best = d, near = o;
            }
        }
        const uint32_t p1 = near ? READ32(kGameObj + kMonsters) : 0;
        if (near)
            WRITE32(kGameObj + kMonsters, near);
        g_origSpinUpdate(rdram, ctx, runtime);
        if (near)
            WRITE32(kGameObj + kMonsters, p1);
    }

    void setBossState(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kSetBossState)
        {
            const uint32_t boss = READ32(wotm::sym::assBoss);
            if (!boss || READ32(GPR_U32(ctx, 4)) != boss)
            {
                ctx->pc = GPR_U32(ctx, 31);   // void: so o chefe da aventura (ou o WOTM_AIVSAI, que roda o initAssBoss)
                return;
            }
        }
        g_origSetBossState(rdram, ctx, runtime);
    }
    // Olhar (18bb1d2:docs/investigacoes/2026-10-23/goliath_lockon.md): o Monster::updateLookAt vira a cabeca para o inimigo mais perto
    // (getLookAtTarget) pelas anims 154/155. No ASSBOSS elas giram a torre inteira (28 das 30 pecas) em ate 90 graus, o que parece
    // um lock-on permanente. No Goliath jogador, sem alvo de olhar: a torre fica no centro; com o lock (os dois gatilhos) ela mira
    // pelo getTarget, como num monstro comum. O chefe da aventura e o Goliath sob IA seguem o retail.
    constexpr uint32_t kLookAt = wotm::fn::getLookAtTarget__7Monster;
    PS2Runtime::RecompiledFunction g_origLookAt = nullptr;

    void lookAt(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4);
        if (ctx->pc == kLookAt && goliathPlayer(rdram, ctx, runtime, m) && READ32(m + kPlayerNum) != 2u)
        {
            ret(ctx, 0);
            return;
        }
        g_origLookAt(rdram, ctx, runtime);
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
    if ((g_origSetActive = runtime.lookupFunction(kSetActive)))
        runtime.registerFunction(kSetActive, &setActive);
    g_loadMech = runtime.lookupFunction(kLoadMech);
    g_killPickup = runtime.lookupFunction(kKillPickup);
    if ((g_origImpale = runtime.lookupFunction(kImpale)))
        runtime.registerFunction(kImpale, &impale);
    if ((g_origLookAt = runtime.lookupFunction(kLookAt)))
        runtime.registerFunction(kLookAt, &lookAt);
    if ((g_origCrowdOK = runtime.lookupFunction(kCrowdOK)))
        runtime.registerFunction(kCrowdOK, &crowdOK);
    if ((g_origEnterState = runtime.lookupFunction(kEnterState)))
        runtime.registerFunction(kEnterState, &enterState);
    if ((g_origSpinUpdate = runtime.lookupFunction(kSpinUpdate)))
        runtime.registerFunction(kSpinUpdate, &spinUpdate);
    if ((g_origSetBossState = runtime.lookupFunction(kSetBossState)))
        runtime.registerFunction(kSetBossState, &setBossState);
    if ((g_origLoadBank = runtime.lookupFunction(kLoadBank)))
        runtime.registerFunction(kLoadBank, &loadBank);
    if ((g_origLoadLoBank = runtime.lookupFunction(kLoadLoBank)))
        runtime.registerFunction(kLoadLoBank, &loadLoBank);
    std::fprintf(stderr, "[custom] Goliath Prime jogavel: %s, %s\n", g_origSetAI ? "visivel com IAs" : "SEM gancho do SetAIMonster",
                 g_origTakeHit ? "toma dano das IAs" : "SEM gancho do takeHit");
}
