// Monstros custom (MODS.md, "Monstros custom"): monstros que o jogo nao oferece na selecao, jogados numa luta comum. Os chefes
// que sao Monster: o Goliath Prime (tipo 13, "assboss", MON/ASSBOSS1.*), chefe da aventura em canyon2, e as duas ultimas fases do
// Cerebulon na fase final (14 "jelly", MON/JELLY0.*, e 15 "final", MON/FINAL0.*; a primeira, o Mecha, e peca da fase e nao
// Monster). Incluido pelo wotm_overrides.cpp, depois do wotm_skins.inc. Investigacao: git show
// 18bb1d2:docs/investigacoes/2026-10-23/goliath_codigo.md (e goliath_selecao.md).
//
// - TheGame::SetAIMonster: a cada IA que entra, o retail desliga todo slot dos tipos 0x1A0, 0x1C0 e 0x1E0 (disableMonster:
//   m_playerNum 0 e drawMe 0 no m_cs, na sombra e nos _cs de +0x71F8/+0x71FC; na aventura e o que esconde o chefe ate a cena).
//   Com um chefe como jogador ele ficava invisivel e parado. Aqui o que o disableMonster zera nos jogadores (m_monsters[0..3])
//   volta depois da chamada; o chefe da aventura e uma IA (m_monsters[4..]) e continua como no retail. O retail tambem conta todo
//   slot 0x1C0/0x1E0 em m_numAIs (os dois esperam escondidos na fase final): um jogador desses tipos sai da conta, senao os lacos
//   por m_monsters[4 + i] leriam uma IA a mais (nula).
// - Monster::takeHit: no 0x1A0 o dano so passa com o attackState do chefe em 4 ou 10 ou vindo do jogador 1 (attackerId ==
//   m_monsters[0]->m_id). Com o Goliath jogador, as IAs nao o machucavam. Durante a chamada, o m_id do jogador 1 vale o do
//   atacante (a unica leitura de m_monsters[0] no takeHit e esse teste); o resto do takeHit segue o caminho normal (bloqueio,
//   recuos). So quando o Goliath atingido e um jogador.

#include "wotm_disc.hpp"   // .CHR do monstro base da camera (camTable)

namespace wotm_custom
{
    constexpr uint32_t kSetAI = wotm::fn::SetAIMonster__7TheGameiiii, kTakeHit = wotm::fn::takeHit__7MonsterP8_fvectorfi;
    constexpr uint32_t kGameObj = wotm::sym::shellGame;
    constexpr uint32_t kMonsters = 0x120380;          // TheGame::m_monsters[8]
    constexpr uint32_t kTypeBits = 0x14, kPlayerNum = 0x18, kId = 0x20, kCs = 0xC, kShadow = 0x1A3C, kCsA = 0x71F8, kCsB = 0x71FC;
    constexpr uint32_t kDrawMe = 0xC;                 // _cs::drawMe (byte)
    constexpr uint32_t kNumAIs = 0x1203E0, kGameMode = 0x1203C8;   // TheGame::m_numAIs, m_gameMode
    constexpr uint32_t kSlots = 0xB80, kSlotStride = 0x11190, kNumSlots = 0x1203D4;   // TheGame::m_slots[16], m_numSlots
    constexpr uint32_t kGoliath = 13u << 5, kJelly = 14u << 5, kFinal = 15u << 5;
    // Chefe que e Monster (a selecao nao oferece; o SetAIMonster o esconde)
    bool bossType(uint32_t bits) { return bits == kGoliath || bits == kJelly || bits == kFinal; }
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
        // fora da aventura, todos os slots de chefe (um chefe CPU tambem fica visivel); na aventura, so os dos jogadores
        const bool adventure = READ32(kGameObj + kGameMode) == 1u;
        const uint32_t slots = std::min(16u, READ32(kGameObj + kNumSlots));
        Saved saved[16];
        int n = 0;
        for (uint32_t i = 0; i < slots; ++i)
        {
            const uint32_t m = kGameObj + kSlots + i * kSlotStride;
            if (!bossType(READ32(m + kTypeBits)) || (adventure && !isPlayerSlot(rdram, ctx, runtime, m)))
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
        int32_t ais = static_cast<int32_t>(READ32(kGameObj + kNumAIs));
        bool players = false;
        for (int j = 0; j < n; ++j)
        {
            const Saved &s = saved[j];
            // IA desta luta (m_monsters[4..]): o aiInit lhe deu m_playerNum 2, que o disableMonster zerou. So o Jelly e o Cerebulon:
            // a IA do Goliath (Ai::updateAssBoss) precisa dos pontos da fase dele
            bool cpu = false;
            const uint32_t bits = READ32(s.m + kTypeBits);
            for (uint32_t i = 0; i < 12 && !cpu && (bits == kJelly || bits == kFinal); ++i)
                cpu = READ32(kGameObj + kMonsters + 4 * (4 + i)) == s.m;
            const bool player = isPlayerSlot(rdram, ctx, runtime, s.m);
            if (!player && !cpu)
                continue;   // carregado e ainda sem IA: o retail o esconde ate o SetAIMonster dele
            WRITE32(s.m + kPlayerNum, player ? s.playerNum : 2u);
            // o jogador volta como estava; a CPU fica com o corpo e a sombra ligados, como uma IA comum depois do SetAIMonster
            // (antes dele os quatro estao desligados), e os dois _cs extras desligados, como os do jogador
            for (int k = 0; k < 4; ++k)
                if (s.cs[k])
                    WRITE8(s.cs[k] + kDrawMe, player ? s.draw[k] : uint8_t(k < 2));
            if (!player)
                continue;
            // contado pelo retail (m_playerNum 2 antes do disableMonster, ou tipo 0x1C0/0x1E0); o setAIMonsterCounted do
            // WOTM_AIVSAI nao o desconta, porque ja o le zerado
            const uint32_t type = READ32(s.m + kTypeBits);
            if (s.playerNum == 2u || type == kJelly || type == kFinal)
                --ais;
            players = true;
        }
        if (players)
            WRITE32(kGameObj + kNumAIs, static_cast<uint32_t>(ais < 0 ? 0 : ais));
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

    // Som: o SoundManager::loadMonsterBank (e o de baixa resolucao) nao tem caso para os chefes. Os sons do Goliath sao do banco do
    // chefe (gSoundBanks[BANK_MECH], Assemb_s.bnk), que o retail so carrega na cena do chefe em canyon2 (canyon2BossIntro); os do
    // Jelly e da forma final, do Final_s.bnk (gSoundBanks[BANK_FINAL]), que o loadRTSoundBanks so carrega na fase final da
    // aventura. Fora da aventura, o pedido do banco do 0x1A0 vira o SoundManager::loadMechBossSoundBank (mesmo this), como um
    // salto, e o do 0x1C0/0x1E0 vira o m_banks.loadSoundBank(&gSoundBanks[BANK_FINAL], "Final_s.bnk", autoUnload) (que nao faz nada
    // com o banco ja carregado: os dois tipos na mesma luta dividem um).
    constexpr uint32_t kLoadBank = wotm::fn::loadMonsterBank__12SoundManagerii, kLoadLoBank = wotm::fn::loadLoResMonsterBank__12SoundManagerii,
                       kLoadMech = wotm::fn::loadMechBossSoundBank__12SoundManager,
                       kLoadSoundBank = wotm::fn::loadSoundBank__16SoundBankManagerPPvPci;
    constexpr uint32_t kSndBanks = 0x210, kBankFinal = 24;   // SoundManager::m_banks, BANK_FINAL
    PS2Runtime::RecompiledFunction g_origLoadBank = nullptr, g_origLoadLoBank = nullptr, g_loadMech = nullptr;

    // Nome na tela ("%s CRUSHED", "%s WINS"): a entrada do tipo nas 5 copias do MonsterNewNames[17][12] ("assboss", "jelly",
    // "final"); fica o "name" do monster.json (ate 11 letras, minusculas como as do jogo) ou `fallback`
    void setShownName(uint8_t *rdram, uint32_t type, const char *fallback)
    {
        std::string shown = fallback;
        if (const wotm::mods::Custom *cu = wotm::mods::Registry::get().customByType(int(type)))
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
            char *e = reinterpret_cast<char *>(rdram + ((names + type * 12) & 0x01FFFFFFu));
            std::memset(e, 0, 12);
            std::memcpy(e, shown.data(), shown.size());
        }
    }

    void loadBankCommon(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t entry, PS2Runtime::RecompiledFunction orig)
    {
        const uint32_t bits = ctx->pc == entry ? GPR_U32(ctx, 5) : 0;
        if ((bits == kJelly || bits == kFinal) && READ32(kGameObj + kGameMode) != 1u)
        {
            const uint32_t self = GPR_U32(ctx, 4), autoUnload = GPR_U32(ctx, 6);
            std::fprintf(stderr, "[custom] banco de som do %s (Final_s.bnk)\n", bits == kJelly ? "Jelly" : "Cerebulon");
            setShownName(rdram, bits >> 5, bits == kJelly ? "jelly" : "cerebulon");
            wotm_modes4::call(rdram, ctx, runtime, kLoadSoundBank,
                              {self + kSndBanks, wotm::sym::gSoundBanks + 4 * kBankFinal, wotm_modes4::scratchText(rdram, "Final_s.bnk"), autoUnload});
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if (bits == kGoliath && READ32(kGameObj + kGameMode) != 1u && g_loadMech)
        {
            std::fprintf(stderr, "[custom] banco de som do Goliath (Assemb_s.bnk, loadMechBossSoundBank)\n");
            setShownName(rdram, 13, "goliath");
            ctx->pc = kLoadMech;
            g_loadMech(rdram, ctx, runtime);
            return;
        }
        orig(rdram, ctx, runtime);
    }
    void loadBank(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { loadBankCommon(rdram, ctx, runtime, kLoadBank, g_origLoadBank); }
    void loadLoBank(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { loadBankCommon(rdram, ctx, runtime, kLoadLoBank, g_origLoadLoBank); }

    // Camera: o RST/ASSBOSS.CHR traz parametros de camera que o retail nunca usou (no POV 0 a camera fica na frente e baixa:
    // m_00 = (-70, 336, 90) contra (0, -255, 150) do Togera), e a do RST/JELLY.CHR fica nas pernas do Jelly. A tabela de camera do
    // monstro (+0x6CF0: 10 POVs de 0xA0, mais 10 da tela dividida) vem do .CHR dele (MonsterTweaks::setActiveMonster) e o
    // Cameras::SetCameraPOV copia o POV pedido para a camera. Um custom com "camera" fica com a tabela do .CHR do monstro base,
    // com as tres distancias de cada POV (m_00, m_10, m_20) escaladas: na entrada do SetCameraPOV de uma camera que segue o custom,
    // a tabela calculada vai para o monstro antes da copia.
    // A tabela do base sai do .CHR lido no host (mods/ ou disco), da chave de camera como o charreset::readKey a le (tipo 0xFC6:
    // depois de 0x10 bytes, 2 telas de 9 POVs de 0xA0, que vao para os POVs 0..8 de cada tela; o resto da tabela fica o do
    // monstro). Nada de ler o .CHR pelo jogo dentro de um gancho: o setActiveMonster chamado duas vezes deixava as vezes a
    // tabela do proprio .CHR por cima da emprestada.
    constexpr uint32_t kSetPov = wotm::fn::SetCameraPOV__7CamerasiQ26Camera9CameraPOV;
    constexpr uint32_t kCamTable = 0x6CF0, kCamPovs = 20, kCamPov = 0xA0, kCamSplit = 0x640;
    constexpr uint32_t kCameras = wotm::sym::x_7Cameras_m_cameras, kCamStride = 0xEB0, kCamMonster = 0xCF4;
    constexpr uint32_t kChrCameraKey = 0xFC6, kChrKeyData = 0x10, kChrPovs = 9, kChrSplit = kChrPovs * kCamPov, kChrHeader = 12;
    // Sem monster.json (WOTM_MATCH=13): a do Togera em dobro. Com ele, a do "camera" do mod ({"from", "scale"}).
    constexpr const char *kCamBaseDefault = "lizard";
    constexpr float kCamScaleDefault = 2.0f;
    PS2Runtime::RecompiledFunction g_origSetPov = nullptr;
    std::map<std::string, std::vector<uint8_t>> g_camTables;   // "<base>*<escala>" -> tabela pronta (vazia = falhou)

    // Chave de camera do .CHR -> tabela (sobre `cam`, a do monstro); false sem a chave
    bool chrCamera(const std::vector<uint8_t> &file, std::vector<uint8_t> &cam)
    {
        auto rd = [&](size_t o) { uint32_t v = 0; if (o + 4 <= file.size()) std::memcpy(&v, &file[o], 4); return v; };
        for (size_t o = kChrHeader; o + 8 <= file.size();)
        {
            const uint32_t type = rd(o), size = rd(o + 4);
            if (type == 1 || size < 8)
                break;
            if (type == kChrCameraKey && o + kChrKeyData + 2 * kChrSplit <= file.size())
            {
                for (uint32_t s = 0; s < 2; ++s)
                    for (uint32_t p = 0; p < kChrPovs; ++p)
                    {
                        const uint8_t *src = &file[o + kChrKeyData + s * kChrSplit + p * kCamPov];
                        uint8_t *dst = &cam[s * kCamSplit + p * kCamPov];
                        for (uint32_t v = 0; v < 3; ++v)   // m_00, m_10, m_20 (sem o w)
                            std::memcpy(dst + v * 16, src + v * 16, 12);
                        std::memcpy(dst + 0x30, src + 0x30, 0x94 - 0x30);   // 3 PitchChange e os escalares
                        std::memcpy(dst + 0x94, src + 0x98, 4);             // 0x94 e 0x98 trocados no readKey
                        std::memcpy(dst + 0x98, src + 0x94, 4);
                        std::memcpy(dst + 0x9C, src + 0x9C, 4);
                    }
                return true;
            }
            o += size;
        }
        return false;
    }

    const std::vector<uint8_t> &camTable(uint8_t *rdram, uint32_t m, const std::string &base, float scale, const char *who)
    {
        const std::string key = base + "*" + std::to_string(scale);
        if (const auto it = g_camTables.find(key); it != g_camTables.end())
            return it->second;
        std::vector<uint8_t> &cam = g_camTables[key];
        std::string up = base;
        for (char &ch : up)
            ch = char(std::toupper(static_cast<unsigned char>(ch)));
        const std::string rel = "RST/" + up + ".CHR";
        std::vector<uint8_t> file;
        const std::string host = wotm::mods::Registry::get().hostPath(rel);
        if (!host.empty())
        {
            std::ifstream in(std::filesystem::u8path(host), std::ios::binary);
            file.assign(std::istreambuf_iterator<char>(in), {});
        }
        else
        {
            const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
            wotm::disc::readFile(io.cdRoot, io.cdImage.string(), rel, file);
        }
        const uint8_t *const mon = rdram + ((m + kCamTable) & 0x01FFFFFFu);
        std::vector<uint8_t> table(mon, mon + kCamPovs * kCamPov);
        if (!chrCamera(file, table))
        {
            std::fprintf(stderr, "[custom] %s: camera do %s nao carregou (%s); fica a do proprio .CHR\n", who, rel.c_str(),
                         file.empty() ? "arquivo nao achado" : "sem a chave de camera");
            return cam;
        }
        cam = std::move(table);
        for (uint32_t p = 0; p < kCamPovs; ++p)
            for (uint32_t v = 0; v < 3; ++v)
                for (uint32_t c = 0; c < 3; ++c)
                {
                    float f;
                    std::memcpy(&f, &cam[p * kCamPov + v * 16 + c * 4], 4);
                    f *= scale;
                    std::memcpy(&cam[p * kCamPov + v * 16 + c * 4], &f, 4);
                }
        float d10[3];
        std::memcpy(d10, cam.data(), 12);
        std::fprintf(stderr, "[custom] %s: camera do %s, distancias x%.2f (POV 0: %.0f, %.0f, %.0f)\n", who, rel.c_str(), scale, d10[0], d10[1], d10[2]);
        return cam;
    }

    void setPov(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t view = GPR_U32(ctx, 4);
        const uint32_t m = ctx->pc == kSetPov && view < 4 ? READ32(kCameras + view * kCamStride + kCamMonster) : 0;
        const uint32_t type = m ? READ32(m + kTypeBits) >> 5 : 0;
        const wotm::mods::Custom *cu = type ? wotm::mods::Registry::get().customByType(int(type)) : nullptr;
        const char *base = cu && !cu->cameraFrom.empty() ? cu->cameraFrom.c_str() : type == 13 && !cu ? kCamBaseDefault : nullptr;
        if (base)
        {
            const float scale = cu && !cu->cameraFrom.empty() ? cu->cameraScale : kCamScaleDefault;
            const std::vector<uint8_t> &cam = camTable(rdram, m, base, scale, cu ? cu->id.c_str() : "assboss");
            if (!cam.empty())
                std::memcpy(rdram + ((m + kCamTable) & 0x01FFFFFFu), cam.data(), cam.size());
        }
        g_origSetPov(rdram, ctx, runtime);
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
    if ((g_origSetPov = runtime.lookupFunction(kSetPov)))
        runtime.registerFunction(kSetPov, &setPov);
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
    std::fprintf(stderr, "[custom] chefes jogaveis (Goliath, Jelly, Cerebulon): %s, %s\n", g_origSetAI ? "visivel com IAs" : "SEM gancho do SetAIMonster",
                 g_origTakeHit ? "toma dano das IAs" : "SEM gancho do takeHit");
}
