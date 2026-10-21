// Multijogador de 3 a 8 (POS_FRONTEIRA.md, item 11). Incluido pelo wotm_overrides.cpp. Vale quando o Shell tem 3 ou mais jogadores
// (m_numPlayers, posto pelo menu multijogador do wotm_padsel.inc ou pela WOTM_MATCH com WOTM_PLAYERS=3..8); com 1 ou 2 os ganchos
// deixam o jogo como no retail. Com 3 e 4: tela em quadrantes (abaixo). Com 5 a 8: tela cheia com a camera compartilhada (fim do
// comentario).
//
// O motor ja tem quase tudo para 4 vistas: viewInfo, worldCtx e viewDb com 5 entradas (a 4 e a da sombra), 4 cameras
// (Cameras::m_cameras), 4 HUDs, a viewPrevMat das particulas e os pontos de visada com 5, e o viewToggleSplitScreen dos cinemas
// nao mexe com 3 ou mais vistas. O que falta, por gancho:
//   - Vistas (saida do TheGame::gameReestablishViews nos modos 3, 6 e 7 com 3 ou 4 jogadores): quadrantes de 320 x 112 do GS, a
//     vista v na coluna v & 1 e na linha v >> 1 (com 3 jogadores o quadrante de baixo a direita fica preto). Os viewDef do retail sao
//     8 (cheio, metades, menus, unificada, sombra, carga); os dos quadrantes moram no fim da RAM de cima (kQuadDefs), copiados da
//     metade do mesmo lado com altura, origem e centro da linha e a distancia de tela da tela cheia pela metade (o FOV dos 640 x 224).
//     O viewSetFov refaz planos e matrizes; a tesoura do viewDb vai junto (so o GS emulado a usa). viewInfo[v].viewport fica 1 ou 2:
//     o viewGetScreenDisplay continua dizendo "tela dividida" (as cameras usam a tabela de POV dela). m_numCameras = jogadores.
//   - viewGrappleConfig (fim do rtMain e troca da vista unificada) devolve a vista 0 ao viewDef[viewport]; na saida o quadrante volta.
//   - Sem vista unificada: o SetOkToUnify do comeco de cada quadro zera o m_camUnify de todos (o InitCCCamera* volta sem unificar; a
//     camera 2 e a do jogador 3). A pose de vitoria (StateVictory) ainda liga gUseUnifiedView com a camera 2 no vencedor; quando a
//     tela volta a dividir, as cameras 2 e 3 voltam aos jogadores 3 e 4 no POV 0. O ResetLevel poe a camera 2 no POV 3 (a unificada
//     do retail); aqui ela fica no POV que tinha.
//   - 4 jogadores: o SetAIMonster poe cada IA na camera 3 (gameInitCamera(3, ia), camera sem vista no retail). Aqui a camera 3 e a do
//     jogador 4: a IA so recebe m_cameraView 3, sem a camera (m_cameraFollows fica 0, o HUD 3 nao recebe as mensagens dela).
//   - Carga: o Shell::LoadMonstersDB monta o caminho dos jogadores 3 e 4 com o monstro do jogador 2, sem traje (peculiaridade do
//     retail). Do LoadLevelFiles ate a entrada do InitPlayers eles passam pelo caminho das IAs (m_monsterSel[4..], m_costume[2..],
//     o mesmo arquivo) e voltam a jogadores no InitPlayers; a ordem de carga e a da contagem de duplicatas dele (jogadores, depois IAs).
//     Os trajes dos jogadores 3 e 4 nao tem lugar no Shell: ficam em g_extraCostume, e o SetPlayerMonster os recebe no lugar do
//     m_costume[1] que o InitPlayers manda a todo jogador depois do 1.
//   - Motion blur: o buffer de pacotes do MotionBlurAA so tem lugar para 2 vistas; o init fica com 2 e as vistas 2 e 3 sem borrao.
//   - HUD: o Hud::initAfter e o Hud::update saem logo com indice >= 2. O HUD 2/3 vira uma copia do HUD da mesma coluna e roda o
//     update como ele, com monstro, camera e barra da propria vista (hudInitAfter, hudUpdate).
//   - Camera no POV 3 (estacionada): as cameras 2 e 3 nao tinham posicao de estacionar e iam para a origem (no inicio e na morte).
//   - Fim de rodada do modo 3: o retail so renasce com 1 ou 0 vivos (2 jogadores); aqui cada morto renasce como o que e.
//   - Controles: o inputInit abre 2 portas x 4 slots (controle = porta + 2 * slot: o jogador 3 e o slot 1 da porta 0, o 4 o da porta
//     1), mas o stub do runtime so tem o slot 0. As scePad* com (porta, slot) recebem o slot 1 como 0 (estado, modo e vibracao sao
//     os da porta), o scePadPortOpen/Close do slot 1 so devolve sucesso, e o scePadRead dele le os dispositivos do jogador porta + 3
//     (wotmPadSetPortBase, wotm_pad.inc). Decidido na instalacao: o inputInit roda no boot. Os slots 2 e 3 sao os jogadores 5 a 8
//     (o inputInit poe inputPlayerPad[i] = i: o jogador i le o controle i, porta i & 1, slot i >> 1).
//
// 5 a 8 jogadores (sem IA): o motor so tem 4 cameras, 4 HUDs, 5 vistas (a 4 e a da sombra) e vetores de som de 4 por jogador
// (SoundManager::m_events, os vols[4] na pilha dos sons 3D, laços por TheGame::m_numMonsters). Por isso:
//   - Os jogadores 5 a 8 moram em m_monsters[4..7] (o lugar das IAs; sem IA nesse caso, m_numAIs 0) com m_index 4..7, e o
//     m_numMonsters do TheGame fica em 4 (saida do Shell::InitRTState e do InitPlayers): os lacos por ele so veem os 4 primeiros.
//     O m_numPlayers do Shell continua com todos (carga, InitPlayers, fim de rodada).
//   - O m_playerInfo do SetPlayerMonster e game + 0x112480 + 4 * jogador: do 5 em diante cai no Weapons (0x112490). Os 16 bytes
//     voltam na saida e o m_playerInfo deles passa para a RAM de cima (kPlayerDat). O playerInit le m_ctlrCfg[m_index], que so tem 2:
//     do jogador 3 em diante a configuracao de botoes vira a do jogador 1. O setSoundVolumeInfo de m_index >= 4 nao grava (m_events[4]).
//   - Uma vista, tela cheia (viewDef[0], viewNumViews 1, m_numCameras 1), refeita a cada quadro (o viewToggleSplitScreen dos cinemas
//     e do botao de vista divide a tela de novo). Camera compartilhada: na entrada do viewUpdate(0) o _cs da vista recebe uma camera
//     que enquadra os jogadores vivos (sharedCamera); fora dos cinemas e da pose de vitoria (gUseUnifiedView), que ficam com a do
//     jogo. As cameras 1 a 7 nao existem: o gameInitCamera dos jogadores 2 a 8 so poe m_cameraView 0 (mensagens no HUD 0). O andar
//     do monstro e relativo a ele mesmo (PadFlags::computeMotionVec), nao a camera.
//   - HUD: o Hud 0 mostra o jogador 1 como no retail; a vida e as vitorias de todos vem do placar nativo (wotm_live.inc,
//     wotmSharedHud).
//   - Som: o loadRTSoundBanks carrega os bancos de m_monsters[0 .. m_numMonsters) e [4 .. 4 + m_numAIs): durante ele o m_numAIs vale
//     os jogadores alem do 4.

void wotmPadSetPortBase(int base);   // wotm_pad.inc: o scePadRead em curso le os dispositivos da porta + base

namespace wotm_split4
{
    static_assert(PS2_RAM_SIZE > PS2_GUEST_RAM_SIZE, "os viewDef dos quadrantes moram acima da RAM do jogo");

    constexpr uint32_t kShell = wotm::sym::shellInfo, kGame = wotm::sym::shellGame;
    constexpr uint32_t kMonsterSel = kShell + 0x2938, kNumPlayers = kShell + 0x2BB4, kNumAIs = kShell + 0x2BB8, kCostume = kShell + 0x2BE0;
    constexpr uint32_t kSlots = kGame + 0xB80, kSlotStride = 0x11190, kMonsters = kGame + 0x120380, kGameMode = kGame + 0x1203C8,
                       kNumSlots = kGame + 0x1203D4;
    constexpr uint32_t kPlayerNum = 0x18, kCameraView = 0x6CD8, kCamUnify = 0x7970;   // Monster
    constexpr uint32_t kViewInfo = wotm::sym::viewInfo, kViewDef = wotm::sym::viewDef, kViewDb = wotm::sym::viewDb,
                       kNumViews = wotm::sym::viewNumViews, kUnified = wotm::sym::gUseUnifiedView;
    constexpr uint32_t kCameras = wotm::sym::x_7Cameras_m_cameras, kNumCameras = wotm::sym::x_7Cameras_m_numCameras, kCamStride = 0xEB0,
                       kCamCs = 0xCF0, kCamMonster = 0xCF4, kCamPov = 0xCF8;
    constexpr uint32_t kDefSize = 0xC0, kDbSize = 0x360;
    constexpr uint32_t kDbScissor[4] = {0xC0, 0x140, 0x230, 0x2B0};
    constexpr uint32_t kQuadDefs = PS2_RAM_SIZE - 0x1000;   // 4 ViewDef dos quadrantes
    constexpr uint32_t kPlayerDat = kQuadDefs + 0x400;      // m_playerInfo dos jogadores 5 a 8 (4 bytes por jogador, indice = jogador)
    constexpr int kMaxPlayers = 8;

    constexpr uint32_t kReestablish = wotm::fn::gameReestablishViews__7TheGame, kGrapple = wotm::fn::viewGrappleConfig__Fv,
                       kOkToUnify = wotm::fn::SetOkToUnify__7TheGame, kResetLevel = wotm::fn::ResetLevel__7TheGame,
                       kInitCamera = wotm::fn::gameInitCamera__7TheGameii, kLoadLevelFiles = wotm::fn::LoadLevelFiles__5Shell,
                       kInitPlayers = wotm::fn::InitPlayers__5Shell, kViewCreate = wotm::fn::viewCreate__F10_viewportsi,
                       kViewSetFov = wotm::fn::viewSetFov__Fiff, kFollow = wotm::fn::SetCameraToFollowMonster__7CamerasiP7Monster,
                       kSetPov = wotm::fn::SetCameraPOV__7CamerasiQ26Camera9CameraPOV;
    PS2Runtime::RecompiledFunction g_origReestablish = nullptr, g_origGrapple = nullptr, g_origResetLevel = nullptr, g_origInitCamera = nullptr,
                                   g_origLoadLevelFiles = nullptr, g_origInitPlayers = nullptr, g_viewCreate = nullptr, g_viewSetFov = nullptr,
                                   g_follow = nullptr, g_setPov = nullptr;

    int g_players = 0;              // teto de jogadores com os ganchos instalados (8); 0 = desligado
    int g_views = 0;                // vistas em quadrante montadas (0 = nenhuma: tela cheia ou metades do retail)
    bool g_shared = false;          // 5 a 8 jogadores: tela cheia com a camera compartilhada montada
    bool g_wasUnified = false;      // a vista unificada (vitoria) estava ligada no quadro anterior
    bool g_cinemaFull = false;      // cinema em tela cheia (cinemaViews)
    unsigned g_hudCalls[4] = {};    // diagnostico (WOTM_SPLITLOG): Hud::update de cada HUD desde a ultima linha
    int g_loadExtra = 0;            // jogadores 3 a 8 no caminho das IAs (do LoadLevelFiles ao InitPlayers)
    uint32_t g_extraCostume[kMaxPlayers - 2] = {};   // trajes dos jogadores 3 a 8 (o Shell nao tem lugar para eles)

    float rdF(uint8_t *rdram, uint32_t a) { float v; std::memcpy(&v, rdram + a, 4); return v; }
    void wrF(uint8_t *rdram, uint32_t a, float v) { std::memcpy(rdram + a, &v, 4); }
    uint32_t sel(int i) { return kMonsterSel + 4u * uint32_t(i); }
    uint32_t costume(int i) { return kCostume + 4u * uint32_t(i); }
    uint32_t camera(int v) { return kCameras + kCamStride * uint32_t(v); }

    // Chamada de uma funcao do jogo com o contexto salvo (como o callTakeDamage): a0, a1 inteiros, f12 e f13 float
    void callGuest(PS2Runtime::RecompiledFunction fn, uint32_t addr, uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t a0,
                   uint32_t a1 = 0, float f12 = 0.f, float f13 = 0.f)
    {
        if (!fn)
            return;
        const R5900Context saved = *ctx;
        ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a0)));
        ctx->r[5] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a1)));
        ctx->r[31] = _mm_set_epi64x(0, 0);
        ctx->f[12] = f12;
        ctx->f[13] = f13;
        ctx->pc = addr;
        fn(rdram, ctx, runtime);
        *ctx = saved;
    }

    // Jogadores do Shell (m_numPlayers) quando os ganchos valem (3 a 8), sem olhar o modo; 0 = nao vale
    int shellPlayers(uint8_t *rdram)
    {
        const int32_t n = static_cast<int32_t>(FAST_READ32(kNumPlayers));
        return g_players && n >= 3 && n <= g_players ? n : 0;
    }
    // Jogadores numa luta multijogador agora (0 = nao vale): 3 a 8 no Shell, num dos modos com tela dividida (3, 6 e 7)
    int multiPlayers(uint8_t *rdram)
    {
        const int32_t mode = static_cast<int32_t>(FAST_READ32(kGameMode));
        return mode == 3 || mode == 6 || mode == 7 ? shellPlayers(rdram) : 0;
    }
    // ... com a tela em quadrantes (3 ou 4)
    int splitPlayers(uint8_t *rdram)
    {
        const int n = multiPlayers(rdram);
        return n <= 4 ? n : 0;
    }
    // ... com a tela cheia e a camera compartilhada (5 a 8)
    int sharedPlayers(uint8_t *rdram)
    {
        const int n = multiPlayers(rdram);
        return n >= 5 ? n : 0;
    }

    void writeQuadDefs(uint8_t *rdram)
    {
        const float fov = rdF(rdram, kViewDef) * 0.5f;   // viewDef[0]: 384 em 640 de largura; o quadrante tem 320
        for (uint32_t v = 0; v < 4; ++v)
        {
            const uint32_t src = kViewDef + kDefSize * (1u + (v & 1u)), dst = kQuadDefs + kDefSize * v;   // metade esquerda ou direita
            std::memcpy(rdram + dst, rdram + src, kDefSize);
            const int32_t h = FAST_READ16(src + 6) / 2, sign = (v >> 1) ? 1 : -1;
            FAST_WRITE16(dst + 6, static_cast<uint16_t>(h));                          // h
            FAST_WRITE16(dst + 0xE, static_cast<uint16_t>((v >> 1) * uint32_t(h)));   // y da tesoura
            wrF(rdram, dst + 0x14, rdF(rdram, src + 0x14) + float(sign * h / 2));    // offY: 2048 -/+ 56
            FAST_WRITE32(dst + 0x1C, static_cast<uint32_t>(static_cast<int32_t>(FAST_READ32(src + 0x1C)) + sign * h / 2));   // cy
            wrF(rdram, dst, fov);
        }
    }

    // Vista v com o viewDef `def`: planos e matrizes (viewSetFov) e a tesoura do viewDb
    void applyDef(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t v, uint32_t def)
    {
        FAST_WRITE32(kViewInfo + 0x14u * v + 4, def);
        callGuest(g_viewSetFov, kViewSetFov, rdram, ctx, runtime, v, 0, rdF(rdram, def), rdF(rdram, def + 0x20));
        const uint64_t x = FAST_READ16(def + 0xC), y = FAST_READ16(def + 0xE), w = FAST_READ16(def + 4), h = FAST_READ16(def + 6);
        const uint64_t s = x | ((x + w - 1) << 16) | (y << 32) | ((y + h - 1) << 48);
        for (uint32_t off : kDbScissor)
            FAST_WRITE64(kViewDb + kDbSize * v + off, s);
    }
    // Vista v no seu quadrante
    void applyQuad(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t v)
    {
        applyDef(rdram, ctx, runtime, v, kQuadDefs + kDefSize * v);
    }

    // Cameras 2 e 3 de volta aos jogadores 3 e 4, no POV 0 (a vitoria as deixou no vencedor, POV 7)
    void refollow(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        for (int v = 2; v < g_views; ++v)
        {
            const uint32_t m = FAST_READ32(kMonsters + 4u * uint32_t(v));
            if (!m || FAST_READ32(camera(v) + kCamMonster) == m)
                continue;
            FAST_WRITE32(camera(v) + kCamPov, 0);   // o SetCameraToFollowMonster recarrega o POV corrente
            callGuest(g_follow, kFollow, rdram, ctx, runtime, uint32_t(v), m);
        }
    }

    // 5 a 8: uma vista em tela cheia (viewDef[0]; o viewDef[5] da vista unificada da pose de vitoria tambem e tela cheia)
    void sharedViews(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        FAST_WRITE32(kNumViews, 1u);
        FAST_WRITE32(kNumCameras, 1u);
        const uint32_t def = FAST_READ32(kViewInfo + 4);
        if (def != kViewDef && def != kViewDef + kDefSize * 5u)
        {
            FAST_WRITE32(kViewInfo + 8, 0u);   // viewport 0: tela cheia (as cameras usam a tabela de POV dela)
            applyDef(rdram, ctx, runtime, 0u, kViewDef);
        }
    }

    // Camera compartilhada (5 a 8 jogadores), escrita no _cs da vista 0 na entrada do viewUpdate(0): olha o centro dos jogadores vivos
    // de cima (kPitch), num rumo fixo na luta (o da camera do jogo no primeiro quadro), afastada o bastante para o circulo que os contem
    // caber na altura da tela, com suavizacao. Escala pela distancia da camera do jogo ao jogador 1 no primeiro quadro (d0). Linhas do
    // mat do _cs: direita (+0x20), frente (+0x30) e cima (+0x40) no mundo (lockCameraAxes), Z para cima; trans em +0x10.
    // WOTM_SHAREDCAM=<pitch em graus>,<distancia maxima em d0> (padrao 50,3).
    constexpr uint32_t kMonsterCs = 0xC;
    struct SharedCam
    {
        bool init = false;
        float t[3] = {}, dist = 0.f, yaw = 0.f, d0 = 0.f;
    } g_cam;

    void sharedCamera(uint8_t *rdram, int n)
    {
        static const float kPitch = [] { const char *e = std::getenv("WOTM_SHAREDCAM"); return (e ? float(std::atof(e)) : 50.f) * 0.0174533f; }();
        static const float kMaxD = [] {
            const char *e = std::getenv("WOTM_SHAREDCAM");
            const char *c = e ? std::strchr(e, ',') : nullptr;
            return c ? float(std::atof(c + 1)) : 3.f;
        }();
        const uint32_t cs = FAST_READ32(kViewInfo) & 0x1FFFFFFFu;
        if (cs < 0x1000u || cs + 0x60u > PS2_RAM_SIZE)
            return;
        float pos[kMaxPlayers][3];
        bool alive[kMaxPlayers] = {};
        int got = 0, living = 0;
        for (int i = 0; i < n; ++i)
        {
            const uint32_t m = FAST_READ32(kMonsters + 4u * uint32_t(i)) & 0x1FFFFFFFu;
            const uint32_t mcs = m >= 0x1000u ? FAST_READ32(m + kMonsterCs) & 0x1FFFFFFFu : 0u;
            if (mcs < 0x1000u || mcs + 0x20u > PS2_RAM_SIZE)
                continue;
            for (int k = 0; k < 3; ++k)
                pos[got][k] = rdF(rdram, mcs + 0x10u + 4u * uint32_t(k));
            if (!std::isfinite(pos[got][0] + pos[got][1] + pos[got][2]))
                continue;
            alive[got] = rdF(rdram, m + 0x44C) > 0.f && !FAST_READ8(m + 0xE8);   // m_health, m_dead
            living += alive[got];
            ++got;
        }
        if (!got)
            return;
        float c[3] = {0.f, 0.f, 0.f};
        int used = 0;
        for (int i = 0; i < got; ++i)
            if (alive[i] || !living)
            {
                for (int k = 0; k < 3; ++k)
                    c[k] += pos[i][k];
                ++used;
            }
        for (float &v : c)
            v /= float(used);
        float r = 0.f;
        for (int i = 0; i < got; ++i)
            if (alive[i] || !living)
                r = std::max(r, std::hypot(pos[i][0] - c[0], pos[i][1] - c[1]));
        if (!g_cam.init)
        {
            // rumo e escala da camera do jogo (que segue o jogador 1)
            const float fx = rdF(rdram, cs + 0x30), fy = rdF(rdram, cs + 0x34);
            const float d0 = std::sqrt((rdF(rdram, cs + 0x10) - pos[0][0]) * (rdF(rdram, cs + 0x10) - pos[0][0]) +
                                       (rdF(rdram, cs + 0x14) - pos[0][1]) * (rdF(rdram, cs + 0x14) - pos[0][1]) +
                                       (rdF(rdram, cs + 0x18) - pos[0][2]) * (rdF(rdram, cs + 0x18) - pos[0][2]));
            if (!std::isfinite(fx + fy + d0) || fx * fx + fy * fy < 0.01f || d0 < 10.f || d0 > 20000.f)
                return;   // camera do jogo ainda nao posta: espera
            g_cam.yaw = std::atan2(fy, fx);
            g_cam.d0 = d0;
            std::copy(c, c + 3, g_cam.t);
            g_cam.dist = 0.f;
            g_cam.init = true;
            std::fprintf(stderr, "[split] camera compartilhada: rumo %.0f graus, d0 %.0f, inclinacao %.0f graus\n", g_cam.yaw * 57.2958f, d0,
                         kPitch * 57.2958f);
        }
        const float d0 = g_cam.d0;
        // o circulo de raio r + margem cabe na altura da tela: meia abertura vertical ~28 graus (fov 384 em 640 x 224)
        const float want = std::clamp((r + 0.8f * d0) * 1.9f, 1.5f * d0, kMaxD * d0);
        const float target[3] = {c[0], c[1], c[2] + 0.25f * d0};
        const float a = g_cam.dist > 0.f ? 0.08f : 1.f;
        for (int k = 0; k < 3; ++k)
            g_cam.t[k] += (target[k] - g_cam.t[k]) * a;
        g_cam.dist += (want - g_cam.dist) * a;
        const float cp = std::cos(kPitch), sp = std::sin(kPitch);
        const float f[3] = {cp * std::cos(g_cam.yaw), cp * std::sin(g_cam.yaw), -sp};
        float rt[3] = {f[1], -f[0], 0.f};   // frente x Z
        const float rl = std::hypot(rt[0], rt[1]);
        rt[0] /= rl;
        rt[1] /= rl;
        const float up[3] = {rt[1] * f[2] - rt[2] * f[1], rt[2] * f[0] - rt[0] * f[2], rt[0] * f[1] - rt[1] * f[0]};   // direita x frente
        for (int k = 0; k < 3; ++k)
        {
            wrF(rdram, cs + 0x10u + 4u * uint32_t(k), g_cam.t[k] - f[k] * g_cam.dist);
            wrF(rdram, cs + 0x20u + 4u * uint32_t(k), rt[k]);
            wrF(rdram, cs + 0x30u + 4u * uint32_t(k), f[k]);
            wrF(rdram, cs + 0x40u + 4u * uint32_t(k), up[k]);
        }
    }

    // viewUpdate(v): na vista 0 com 5 a 8 jogadores, a camera compartilhada antes das matrizes; o cinema e a pose de vitoria ficam com
    // a camera do jogo
    constexpr uint32_t kViewUpdate = wotm::fn::viewUpdate__Fi;
    PS2Runtime::RecompiledFunction g_origViewUpdate = nullptr;
    std::atomic<int64_t> g_sharedStamp{0};   // ultimo viewUpdate(0) da luta de 5 a 8 (ms do relogio estavel): placar nativo ligado
    std::atomic<int> g_sharedN{0};
    int64_t nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void viewUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kViewUpdate && GPR_U32(ctx, 4) == 0u && g_shared)
            if (const int n = sharedPlayers(rdram))
            {
                if (!FAST_READ32(wotm::sym::moviePlaying) && !FAST_READ32(kUnified))
                    sharedCamera(rdram, n);
                g_sharedN.store(n, std::memory_order_relaxed);
                g_sharedStamp.store(nowMs(), std::memory_order_relaxed);
            }
        g_origViewUpdate(rdram, ctx, runtime);
    }

    // viewToggleSplitScreen (cinemas e o botao de vista do PadFlags::updateViewChanges): com 5 a 8 a tela fica numa vista
    constexpr uint32_t kViewToggle = wotm::fn::viewToggleSplitScreen__FbT0;
    PS2Runtime::RecompiledFunction g_origViewToggle = nullptr;
    void viewToggle(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kViewToggle && g_shared && sharedPlayers(rdram))
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origViewToggle(rdram, ctx, runtime);
    }

    void reestablishViews(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = ctx->pc == kReestablish ? GPR_U32(ctx, 31) : 0u;
        g_origReestablish(rdram, ctx, runtime);
        if (!ra || ctx->pc != ra)
            return;
        if (const int s = sharedPlayers(rdram))
        {
            g_views = 0;
            sharedViews(rdram, ctx, runtime);
            if (!g_shared)
                std::fprintf(stderr, "[split] %d jogadores: tela cheia, camera compartilhada\n", s);
            g_shared = true;
            g_cinemaFull = false;
            g_wasUnified = false;
            return;
        }
        g_shared = false;
        const int n = splitPlayers(rdram);
        if (!n)
        {
            g_views = 0;
            return;
        }
        writeQuadDefs(rdram);
        FAST_WRITE32(kNumViews, 4u);   // o viewCreate pinta o fundo de viewNumViews viewDb
        callGuest(g_viewCreate, kViewCreate, rdram, ctx, runtime, 1u, 2u);   // vista 2: o retail a deixa na viewport 5 (unificada)
        if (n == 4)
            callGuest(g_viewCreate, kViewCreate, rdram, ctx, runtime, 2u, 3u);   // vista 3: _cs e viewDb (o retail nao a cria)
        for (int v = 0; v < n; ++v)
            applyQuad(rdram, ctx, runtime, uint32_t(v));
        FAST_WRITE32(kNumViews, uint32_t(n));
        FAST_WRITE32(kNumCameras, uint32_t(n));
        if (n == 4)
            callGuest(g_setPov, kSetPov, rdram, ctx, runtime, 3u, FAST_READ32(camera(3) + kCamPov));   // o laco do original foi ate 3
        if (g_views != n)
            std::fprintf(stderr, "[split] %d jogadores: vistas em quadrantes de 320 x 112, %d cameras\n", n, n);
        g_views = n;
        g_cinemaFull = false;
        g_wasUnified = false;   // as cameras dos jogadores ja vem do InitPlayers (gameInitCamera(i, slot))
        // a camera 2 sai do InitBeforeDbLoad no POV 3 (repouso da unificada); com jogador para seguir, vai ao POV 0 como as outras
        for (int v = 2; v < n; ++v)
            if (FAST_READ32(camera(v) + kCamPov) == 3u && FAST_READ32(camera(v) + kCamMonster))
                callGuest(g_setPov, kSetPov, rdram, ctx, runtime, uint32_t(v), 0u);
    }

    // Cameras::SetCameraPOV(vista, pov): o POV 3 e a camera estacionada em +0xD20 (DetachCamera; TogglePOV alterna 0 e 3). As cameras
    // 2 e 3 do retail nunca recebem essa posicao (a 2 e a unificada, que o InitBeforeDbLoad e o ResetLevel poem no POV 3 so como
    // repouso): no POV 3 elas iam para a origem do mapa (quadrante so de neblina, no inicio e na morte do jogador). Com os quadrantes,
    // o POV 3 numa delas estaciona onde ela esta (_cs->trans), como o DetachCamera, que tambem poe a posicao no _cs antes.
    constexpr uint32_t kCamPark = 0xD20;
    PS2Runtime::RecompiledFunction g_origSetPov = nullptr;
    void setCameraPov(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t v = GPR_U32(ctx, 4);
        if (ctx->pc == kSetPov && GPR_U32(ctx, 5) == 3u && v >= 2u && v < 4u && g_views > int(v))
        {
            const uint32_t c = camera(int(v)), cs = FAST_READ32(c + kCamCs) & 0x1FFFFFFFu;
            if (cs >= 0x1000u && cs + 0x20u <= PS2_RAM_SIZE)
                std::memcpy(rdram + c + kCamPark, rdram + cs + 0x10, 16);   // _cs->trans
        }
        g_origSetPov(rdram, ctx, runtime);
    }

    // Cinema (ActionDispatch::doCinema, moviePlaying; a apresentacao dos monstros antes da luta): com 2 jogadores o doCinema chama o
    // viewToggleSplitScreen e a cena sai em tela cheia; com 3 ou mais vistas o toggle nao faz nada e so a vista 0 seguia o cinema, no
    // quadrante dela. Aqui, no comeco de cada quadro (okToUnify), o cinema ligado poe a tela numa vista so (viewNumViews 1, vista 0 com o
    // viewDef[0], 640 x 224) e o cinema desligado volta aos quadrantes; o rtMain le o numero de vistas no quadro seguinte. As cameras
    // das outras vistas seguem os monstros por baixo.
    constexpr uint32_t kMoviePlaying = wotm::sym::moviePlaying;
    void cinemaViews(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool movie = FAST_READ32(kMoviePlaying) != 0 && !FAST_READ32(kUnified);
        if (movie == g_cinemaFull)
            return;
        g_cinemaFull = movie;
        if (movie)
        {
            FAST_WRITE32(kNumViews, 1u);
            applyDef(rdram, ctx, runtime, 0u, kViewDef);
        }
        else
        {
            // o endCinema chama o viewToggleSplitScreen, que com 1 vista recria a 0 e a 1 como metades (viewCreate): todas de volta
            FAST_WRITE32(kNumViews, uint32_t(g_views));
            for (int v = 0; v < g_views; ++v)
            {
                FAST_WRITE32(kViewInfo + 0x14u * uint32_t(v) + 8, 1u + (uint32_t(v) & 1u));   // viewport: metade do mesmo lado
                applyQuad(rdram, ctx, runtime, uint32_t(v));
            }
        }
    }

    void grappleConfig(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = ctx->pc == kGrapple ? GPR_U32(ctx, 31) : 0u;
        g_origGrapple(rdram, ctx, runtime);
        if (ra && ctx->pc == ra && g_views && !FAST_READ32(kUnified) && splitPlayers(rdram))
        {
            if (g_cinemaFull)
                applyDef(rdram, ctx, runtime, 0u, kViewDef);
            else
                applyQuad(rdram, ctx, runtime, 0u);
        }
    }

    // Comeco de cada quadro do rtMain (gameSetOkToUnify): sem unificar; a camera volta aos jogadores quando a vitoria termina
    PS2Runtime::RecompiledFunction g_origOkToUnify = nullptr;
    void okToUnify(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kOkToUnify && g_shared && sharedPlayers(rdram))
        {
            // 5 a 8: sem camera de combate (a unificada); a tela volta a uma vista se algo a dividiu
            sharedViews(rdram, ctx, runtime);
            const uint32_t slots = std::min<uint32_t>(FAST_READ32(kNumSlots), 16u);
            for (uint32_t i = 0; i < slots; ++i)
                FAST_WRITE32(kSlots + kSlotStride * i + kCamUnify, 0);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if (ctx->pc != kOkToUnify || !g_views || !splitPlayers(rdram))
        {
            g_origOkToUnify(rdram, ctx, runtime);
            return;
        }
        cinemaViews(rdram, ctx, runtime);
        static const bool log = std::getenv("WOTM_SPLITLOG") != nullptr;   // uma linha por segundo: camera de cada vista
        static unsigned frames = 0;
        if (log && ++frames % 60 == 0)
        {
            std::fprintf(stderr, "[split] cameras:");
            for (int v = 0; v < g_views; ++v)
            {
                const uint32_t cs = FAST_READ32(camera(v) + kCamCs), vcs = FAST_READ32(kViewInfo + 0x14u * uint32_t(v));
                std::fprintf(stderr, " %d: monstro %x (jogador %x) pov %d cs %x vista %x em %.0f,%.0f,%.0f;", v, FAST_READ32(camera(v) + kCamMonster),
                             FAST_READ32(kMonsters + 4u * uint32_t(v)), static_cast<int32_t>(FAST_READ32(camera(v) + kCamPov)), cs, vcs,
                             rdF(rdram, (vcs & 0x1FFFFFFFu) + 0x10), rdF(rdram, (vcs & 0x1FFFFFFFu) + 0x14), rdF(rdram, (vcs & 0x1FFFFFFFu) + 0x18));
            }
            std::fprintf(stderr, " hud.update %u/%u/%u/%u\n", g_hudCalls[0], g_hudCalls[1], g_hudCalls[2], g_hudCalls[3]);
            for (unsigned &n : g_hudCalls)
                n = 0;
        }
        const uint32_t slots = std::min<uint32_t>(FAST_READ32(kNumSlots), 16u);
        for (uint32_t i = 0; i < slots; ++i)
            FAST_WRITE32(kSlots + kSlotStride * i + kCamUnify, 0);
        if (FAST_READ32(kUnified))
            g_wasUnified = true;
        else if (g_wasUnified)
        {
            g_wasUnified = false;
            refollow(rdram, ctx, runtime);
        }
        ctx->pc = GPR_U32(ctx, 31);
    }

    void resetLevel(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool keep = ctx->pc == kResetLevel && g_views && splitPlayers(rdram);
        const uint32_t ra = GPR_U32(ctx, 31), pov = FAST_READ32(camera(2) + kCamPov);
        g_origResetLevel(rdram, ctx, runtime);
        if (keep && ctx->pc == ra && FAST_READ32(camera(2) + kCamPov) != pov)
            callGuest(g_setPov, kSetPov, rdram, ctx, runtime, 2u, pov);
    }

    // TheGame::gameInitCamera(view, slot): com 4 jogadores a camera 3 fica com o jogador 4 (a IA nao a leva). Com 5 a 8 so a camera 0
    // existe: os jogadores 2 a 8 (SetPlayerMonster(i, ..., vista i)) so recebem m_cameraView 0 (sem m_viewSlot[i], que passa do fim
    // para o m_slotInteractive, nem camera i). Olha so o Shell: no InitPlayers o modo do TheGame ainda pode nao estar posto.
    void initCamera(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kInitCamera && GPR_U32(ctx, 5) >= 1u && shellPlayers(rdram) >= 5)
        {
            const uint32_t slot = GPR_U32(ctx, 6);
            if (slot < 16u)
                FAST_WRITE32(kSlots + kSlotStride * slot + kCameraView, 0u);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if (ctx->pc == kInitCamera && GPR_U32(ctx, 5) == 3u && splitPlayers(rdram) == 4)
        {
            // jogador = um dos m_monsters[0..3] (o SetPlayerMonster ja o gravou); o m_playerNum nao serve: com WOTM_AIVSAI o jogador
            // tambem tem 2
            const uint32_t slot = GPR_U32(ctx, 6);
            const uint32_t m = kSlots + kSlotStride * slot;
            bool player = false;
            for (uint32_t i = 0; i < 4; ++i)
                player = player || FAST_READ32(kMonsters + 4u * i) == m;
            if (slot < 16u && !player)
            {
                FAST_WRITE32(m + kCameraView, 3u);
                ctx->pc = GPR_U32(ctx, 31);
                return;
            }
        }
        g_origInitCamera(rdram, ctx, runtime);
    }

    // Jogadores 3 a 8 para o caminho das IAs (carga) e de volta (InitPlayers). As IAs andam extra posicoes; m_costume[2 + j] vai ate
    // o indice 11 (0x2C0C, antes do m_mode): no maximo 10 entre jogadores extras e IAs. Com 5 a 8 jogadores nao ha IA: os jogadores
    // 5 a 8 ficam em m_monsterSel[4..7] no lugar delas, e o deslocamento de sel(2 + k) para sel(4 + k) se sobrepoe (copia de tras
    // para a frente).
    void toLoadLayout(uint8_t *rdram)
    {
        const int n = static_cast<int32_t>(FAST_READ32(kNumPlayers)), extra = n - 2;
        int ais = n > 4 ? 0 : std::clamp(static_cast<int32_t>(FAST_READ32(kNumAIs)), 0, 10 - extra);
        for (int j = ais - 1; j >= 0; --j)
        {
            FAST_WRITE32(sel(4 + j + extra), FAST_READ32(sel(4 + j)));
            FAST_WRITE32(costume(2 + j + extra), FAST_READ32(costume(2 + j)));
        }
        for (int k = extra - 1; k >= 0; --k)
        {
            FAST_WRITE32(sel(4 + k), FAST_READ32(sel(2 + k)));
            FAST_WRITE32(costume(2 + k), g_extraCostume[k]);
        }
        FAST_WRITE32(kNumPlayers, 2u);
        FAST_WRITE32(kNumAIs, uint32_t(ais + extra));
        g_loadExtra = extra;
    }

    void toPlayLayout(uint8_t *rdram)
    {
        const int extra = g_loadExtra, ais = std::max(0, static_cast<int32_t>(FAST_READ32(kNumAIs)) - extra), n = 2 + extra;
        for (int k = 0; k < extra; ++k)
            FAST_WRITE32(sel(2 + k), FAST_READ32(sel(4 + k)));
        for (int j = 0; j < ais; ++j)
        {
            FAST_WRITE32(sel(4 + j), FAST_READ32(sel(4 + j + extra)));
            FAST_WRITE32(costume(2 + j), FAST_READ32(costume(2 + j + extra)));
        }
        // o que sobrou do caminho de carga: depois das IAs (3 e 4 jogadores) ou dos jogadores (5 a 8)
        for (int i = std::max(n, 4 + ais); i < 4 + ais + extra; ++i)
            FAST_WRITE32(sel(i), 0u);
        for (int k = 0; k < extra; ++k)
            FAST_WRITE32(costume(2 + ais + k), 0u);
        FAST_WRITE32(kNumPlayers, uint32_t(n));
        FAST_WRITE32(kNumAIs, uint32_t(ais));
        g_loadExtra = 0;
    }

    void loadLevelFiles(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kLoadLevelFiles)
        {
            if (g_loadExtra)   // a carga anterior nao chegou ao InitPlayers
                toPlayLayout(rdram);
            if (shellPlayers(rdram) && FAST_READ32(kShell + 0x2C10) != 6u)   // m_mode 6 carrega os 10 monstros por conta propria
                toLoadLayout(rdram);
            g_cam.init = false;   // fase nova: a camera compartilhada tira rumo e escala da camera do jogo de novo
        }
        g_origLoadLevelFiles(rdram, ctx, runtime);
    }

    // m_numMonsters do TheGame (os lacos por ele so cabem 4 jogadores): no maximo 4, na saida de quem o grava com o m_numPlayers
    void clampNumMonsters(uint8_t *rdram)
    {
        if (static_cast<int32_t>(FAST_READ32(kGame + 0x1203D8)) > 4 && shellPlayers(rdram) >= 5)
            FAST_WRITE32(kGame + 0x1203D8, 4u);
    }

    void initPlayers(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = ctx->pc == kInitPlayers ? GPR_U32(ctx, 31) : 0u;
        if (ra && g_loadExtra)
            toPlayLayout(rdram);
        g_origInitPlayers(rdram, ctx, runtime);
        if (ra && ctx->pc == ra)
            clampNumMonsters(rdram);
    }

    constexpr uint32_t kInitRTState = wotm::fn::InitRTState__5Shell;
    PS2Runtime::RecompiledFunction g_origInitRTState = nullptr;
    void initRTState(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = ctx->pc == kInitRTState ? GPR_U32(ctx, 31) : 0u;
        g_origInitRTState(rdram, ctx, runtime);
        if (ra && ctx->pc == ra)
            clampNumMonsters(rdram);
    }

    // SoundManager::loadRTSoundBanks: bancos de m_monsters[0 .. m_numMonsters) e [4 .. 4 + m_numAIs). Com 5 a 8 jogadores, durante ele
    // o m_numAIs vale os jogadores alem do 4 (os bancos deles entram como os das IAs).
    constexpr uint32_t kLoadBanks = wotm::fn::loadRTSoundBanks__12SoundManager, kGameNumAIs = kGame + 0x1203E0;
    PS2Runtime::RecompiledFunction g_origLoadBanks = nullptr;
    void loadBanks(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = ctx->pc == kLoadBanks ? GPR_U32(ctx, 31) : 0u;
        const int n = ra ? shellPlayers(rdram) : 0;
        const uint32_t ais = FAST_READ32(kGameNumAIs);
        if (n >= 5)
            FAST_WRITE32(kGameNumAIs, uint32_t(n - 4));
        g_origLoadBanks(rdram, ctx, runtime);
        if (n >= 5 && ctx->pc == ra)
            FAST_WRITE32(kGameNumAIs, ais);
    }

    // SoundManager::setSoundVolumeInfo(m, ...): m_events[m_index] so tem 4 jogadores
    constexpr uint32_t kSoundInfo = wotm::fn::setSoundVolumeInfo__12SoundManagerP7MonsteriP8_fvector;
    PS2Runtime::RecompiledFunction g_origSoundInfo = nullptr;
    void soundInfo(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 5) & 0x1FFFFFFFu;
        if (ctx->pc == kSoundInfo && m >= 0x1000u && m + 0x30u <= PS2_RAM_SIZE && FAST_READ32(m + 0x24) >= 4u)   // m_index
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origSoundInfo(rdram, ctx, runtime);
    }

    // TheGame::SetPlayerMonster(jogador, tipo, dup, vista, traje): o InitPlayers manda m_costume[1] a todo jogador depois do 1; os
    // jogadores 3 a 8 recebem o deles (o do arquivo que a carga leu). Do jogador 5 em diante o m_playerInfo (game + 0x112480 +
    // 4 * jogador, onde o playerInit grava o numero do controle) cai no Weapons: os bytes voltam e o m_playerInfo vai para kPlayerDat.
    constexpr uint32_t kSetPlayer = wotm::fn::SetPlayerMonster__7TheGameiiiii, kPlayerInfoBase = kGame + 0x112480, kMonsterPlayerInfo = 0x4D8;
    PS2Runtime::RecompiledFunction g_origSetPlayer = nullptr;
    void setPlayerMonster(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t player = GPR_U32(ctx, 5), ra = ctx->pc == kSetPlayer ? GPR_U32(ctx, 31) : 0u;
        const bool extra = ra && player >= 2u && player < uint32_t(kMaxPlayers) && shellPlayers(rdram);
        if (extra)
            ctx->r[9] = _mm_set_epi64x(0, static_cast<int64_t>(g_extraCostume[player - 2u]));   // t1
        const bool relocate = extra && player >= 4u;
        uint8_t saved[4 * (kMaxPlayers - 4)];
        if (relocate)
            std::memcpy(saved, rdram + kPlayerInfoBase + 16u, sizeof saved);
        g_origSetPlayer(rdram, ctx, runtime);
        if (!relocate || ctx->pc != ra)
            return;
        const uint32_t pad = FAST_READ32(kPlayerInfoBase + 4u * player);
        std::memcpy(rdram + kPlayerInfoBase + 16u, saved, sizeof saved);
        FAST_WRITE32(kPlayerDat + 4u * player, pad);
        const uint32_t m = FAST_READ32(kMonsters + 4u * player) & 0x1FFFFFFFu;
        if (m >= 0x1000u && FAST_READ32(m + kMonsterPlayerInfo) == kPlayerInfoBase + 4u * player)
            FAST_WRITE32(m + kMonsterPlayerInfo, kPlayerDat + 4u * player);
    }

    // Monster::playerInit: m_ctlrCfg[m_index] so tem os jogadores 1 e 2 (do 3 em diante o playerInit lia m_vibration, m_selFirst...):
    // a configuracao de botoes (+0x672C) dos outros e a do jogador 1. Chamado tambem no renascimento (evaluateBattle).
    constexpr uint32_t kPlayerInitFn = wotm::fn::playerInit__7Monster, kCtlrCfg = kShell + 0x2B8C, kMonsterCtlrCfg = 0x672C;
    PS2Runtime::RecompiledFunction g_origPlayerInit = nullptr;
    void playerInit(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t m = GPR_U32(ctx, 4) & 0x1FFFFFFFu, ra = ctx->pc == kPlayerInitFn ? GPR_U32(ctx, 31) : 0u;
        g_origPlayerInit(rdram, ctx, runtime);
        if (ra && ctx->pc == ra && m >= 0x1000u && m + 0x6730u <= PS2_RAM_SIZE && FAST_READ32(m + 0x24) >= 2u && shellPlayers(rdram))
            FAST_WRITE32(m + kMonsterCtlrCfg, FAST_READ32(kCtlrCfg) - 1u);
    }

    // Hud::update(hud): o retail sai logo no comeco com indice (+0) >= 2. O resto usa o indice para o monstro (m_viewSlot[indice]), a
    // camera (m_cameras[indice], _cs em +0xCF0), o lado do texto (0 a esquerda, os outros a direita) e o drawMe das barras de vida (os
    // _cs em +0xF4 dos HUDs 0 e 1: acende a do proprio indice e apaga a do outro; a do proprio HUD e posta na frente da camera). Com os
    // quadrantes, o HUD v = 2 ou 3 roda como o HUD p = v & 1 (mesma coluna): durante a chamada o indice dele vira p e o m_viewSlot[p],
    // o _cs da camera p e a barra do HUD p passam a ser os da vista v; tudo volta na saida. Depois, so a barra deste HUD fica acesa (o
    // rtMain chama o update de cada vista logo antes do hier dela). Com m_numMonsters != 2 a posicao das barras e a da tela cheia
    // (+0x100 no HUD), que e a forma do quadrante.
    constexpr uint32_t kHudUpdate = wotm::fn::update__3Hud, kHudStride = 0x2E0, kHudBarCs = 0xF4, kViewSlot = kGame + 0x1203E8;
    PS2Runtime::RecompiledFunction g_origHudUpdate = nullptr;
    uint32_t hudBar(uint8_t *rdram, uint32_t i)
    {
        const uint32_t cs = FAST_READ32(kGame + kHudStride * i + kHudBarCs) & 0x1FFFFFFFu;
        return cs >= 0x1000u && cs + 0x10u <= PS2_RAM_SIZE ? cs : 0u;
    }
    void hudUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t self = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        const bool split = ctx->pc == kHudUpdate && g_views && splitPlayers(rdram) && self >= kGame && (self - kGame) % kHudStride == 0 &&
                           (self - kGame) / kHudStride < 4u;
        if (!split)
        {
            g_origHudUpdate(rdram, ctx, runtime);
            return;
        }
        const uint32_t me = (self - kGame) / kHudStride, p = me & 1u, hudP = kGame + kHudStride * p;
        ++g_hudCalls[me];
        uint32_t saved[4] = {};
        if (me >= 2u)
        {
            saved[0] = FAST_READ32(self);
            saved[1] = FAST_READ32(kViewSlot + 4u * p);
            saved[2] = FAST_READ32(camera(int(p)) + kCamCs);
            saved[3] = FAST_READ32(hudP + kHudBarCs);
            FAST_WRITE32(self, p);
            FAST_WRITE32(kViewSlot + 4u * p, FAST_READ32(kViewSlot + 4u * me));
            FAST_WRITE32(camera(int(p)) + kCamCs, FAST_READ32(camera(int(me)) + kCamCs));
            FAST_WRITE32(hudP + kHudBarCs, FAST_READ32(self + kHudBarCs));
        }
        g_origHudUpdate(rdram, ctx, runtime);
        if (me >= 2u)
        {
            FAST_WRITE32(self, saved[0]);
            FAST_WRITE32(kViewSlot + 4u * p, saved[1]);
            FAST_WRITE32(camera(int(p)) + kCamCs, saved[2]);
            FAST_WRITE32(hudP + kHudBarCs, saved[3]);
        }
        // a barra deste HUD fica como o jogo a deixou; as outras (por _cs: a do HUD 2 e 3 e a mesma do 0 e 1) apagam
        const uint32_t own = hudBar(rdram, me);
        const uint8_t on = own ? FAST_READ8(own + 0xC) : 0;
        for (uint32_t i = 0; i < 4u; ++i)
            if (const uint32_t b = hudBar(rdram, i); b && b != own)
                FAST_WRITE8(b + 0xC, 0);
        if (own)
            FAST_WRITE8(own + 0xC, on);
    }

    // Hud::initAfter(i) tambem nao faz nada com i >= 2 (sem _cs da barra nem animacoes; o no do HUD em +0xE4 so existe nos HUDs 0 e
    // 1). Na saida do initAfter do HUD 2 ou 3 ele vira uma copia do HUD da mesma coluna (i & 1), ja inicializado (o main os chama de
    // 0 a 3), com o indice dele de volta: os dois dividem o _cs e as animacoes da barra, que cada vista refaz no seu Hud::update
    // logo antes do hier dela.
    constexpr uint32_t kHudInitAfter = wotm::fn::initAfter__3Hudi;
    PS2Runtime::RecompiledFunction g_origHudInitAfter = nullptr;
    void hudInitAfter(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t self = GPR_U32(ctx, 4) & 0x1FFFFFFFu, i = GPR_U32(ctx, 5);
        const uint32_t ra = ctx->pc == kHudInitAfter ? GPR_U32(ctx, 31) : 0u;
        g_origHudInitAfter(rdram, ctx, runtime);
        if (!ra || ctx->pc != ra || i < 2u || i >= 4u || self != kGame + kHudStride * i || !g_players)
            return;
        const int32_t n = static_cast<int32_t>(FAST_READ32(kNumPlayers));
        if (n < 3 || n > 4 || static_cast<int32_t>(i) >= n)   // com 5 a 8 so o HUD 0 aparece
            return;
        std::memcpy(rdram + self, rdram + kGame + kHudStride * (i & 1u), kHudStride);
        FAST_WRITE32(self, i);
    }

    // Fim de rodada do modo 3 (Shell::EvaluateGameStatus -> EvaluateMultiPlayerBattleStatusNoAI ou ...AI, com r = 1 quando alguem
    // morre, 3 quando saem). O NoAI do retail so trata "sobrou 1 vivo" e "nenhum vivo": com 3 ou 4 jogadores uma morte deixa 2 ou 3
    // vivos, ninguem renasce e a rodada seguinte termina logo (o morto continua morto). O AI renasce os slots 0 e 1 como jogador e os
    // outros como IA (os jogadores 3 e 4 virariam IA). Aqui, com os quadrantes, os dois viram a regra do AI por m_monsters: com meta
    // de vitorias (m_killTarget) e um matador vivo que chegou nela, dialogo 5 (vencedor em m_won[0]); senao cada morto renasce como o
    // que e (jogadores 0..3 pelo playerInit, IAs pelo aiInit) e a sessao continua.
    constexpr uint32_t kEvalNoAI = wotm::fn::EvaluateMultiPlayerBattleStatusNoAI__5Shelli,
                       kEvalAI = wotm::fn::EvaluateMultiPlayerBattleStatusAI__5Shelli, kPlayerInit = wotm::fn::playerInit__7Monster,
                       kAiInit = wotm::fn::aiInit__7Monster, kDisplayDialog = wotm::fn::displayDialog__Fi;
    constexpr uint32_t kHealth = 0x44C, kDead = 0xE8, kKiller = 0x846C, kMonsterNum = 0x28, kWins = 0x3C;   // Monster
    constexpr uint32_t kInSession = kShell + 0x2BCC, kInMenus = kShell + 0x2BD0;
    PS2Runtime::RecompiledFunction g_origEvalNoAI = nullptr, g_origEvalAI = nullptr;
    void evaluateBattle(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kEvalNoAI || ctx->pc == kEvalAI;
        if (!entry || !(g_views || g_shared) || !multiPlayers(rdram) || FAST_READ32(kGameMode) != 3u)
        {
            (ctx->pc == kEvalAI ? g_origEvalAI : g_origEvalNoAI)(rdram, ctx, runtime);
            return;
        }
        const int32_t r = static_cast<int32_t>(GPR_U32(ctx, 5));
        const uint32_t ra = GPR_U32(ctx, 31);
        ctx->pc = ra;
        if (r == 3)
        {
            FAST_WRITE32(kInMenus, 0u);
            FAST_WRITE32(kInSession, 0u);
            return;
        }
        int32_t killer = -1;
        const uint32_t slots = std::min<uint32_t>(FAST_READ32(kNumSlots), 16u);
        for (uint32_t i = 0; i < slots && killer < 0; ++i)
        {
            const uint32_t s = kSlots + kSlotStride * i;
            const uint32_t by = FAST_READ32(s + kKiller) & 0x1FFFFFFFu;
            if (FAST_READ8(s + kDead) && by && !FAST_READ8(by + kDead))
                killer = static_cast<int32_t>(FAST_READ32(by + kMonsterNum));
        }
        const int32_t target = static_cast<int32_t>(FAST_READ32(kShell + 0x2A44));   // m_killTarget
        if (target > 0 && killer >= 0 && killer < 16 &&
            static_cast<int32_t>(FAST_READ32(kSlots + kSlotStride * uint32_t(killer) + kWins)) >= target)
        {
            FAST_WRITE32(kGame + 0x12043C, uint32_t(killer));
            callGuest(runtime->lookupFunction(kDisplayDialog), kDisplayDialog, rdram, ctx, runtime, 5u);
            return;
        }
        // jogadores em m_monsters[0 .. jogadores) (de 5 a 8 eles ocupam o lugar das IAs, que nao existem), IAs em [4 .. 4 + m_numAIs)
        const int players = multiPlayers(rdram), ais = static_cast<int32_t>(FAST_READ32(kGame + 0x1203E0));
        for (int i = 0; i < std::max(players, 4 + ais); ++i)
        {
            if (i >= players && i < 4)
                continue;
            const uint32_t m = FAST_READ32(kMonsters + 4u * uint32_t(i)) & 0x1FFFFFFFu;
            if (!m || !(rdF(rdram, m + kHealth) <= 0.f))
                continue;
            const uint32_t fn = i < players ? kPlayerInit : kAiInit;   // o do WOTM_AIVSAI (playerInit -> aiInit) vale: busca a cada vez
            callGuest(runtime->lookupFunction(fn), fn, rdram, ctx, runtime, m);
        }
        FAST_WRITE32(kInMenus, 0u);
        FAST_WRITE32(kInSession, 1u);
    }

    // TheGame::gameGetStartPoint(m): nos modos 2 a 6, o primeiro nascimento de um humano (m_playerNum 1) usa a lista 1 do StartPoints
    // e o de uma IA a lista 2, em rodizio (getNextPoint), recusando ponto com algo a menos de 120; os renascimentos usam a lista 3. A
    // lista 1 tem os pontos dos 2 jogadores do retail (no Centro: 2; a 2 e a 3 tem 4, um em cada canto): os jogadores 3 e 4 pegavam o
    // ponto do 1 (ocupado) e caiam no do 2, os tres no mesmo lugar. Com os quadrantes, o jogador alem do numero de pontos da lista 1
    // nasce como uma IA (m_playerNum 2 durante a chamada): lista 2. Diagnostico com WOTM_SPLITLOG (as listas so uma vez).
    constexpr uint32_t kStartPoint = wotm::fn::gameGetStartPoint__7TheGameP7Monster, kPointTypes = wotm::sym::x_11StartPoints_s_pointTypes;
    PS2Runtime::RecompiledFunction g_origStartPoint = nullptr;
    void logPointLists(uint8_t *rdram)
    {
        for (uint32_t t = 0; t < 4; ++t)
        {
            const uint32_t pts = FAST_READ32(kPointTypes + 16u * t) & 0x1FFFFFFFu, n = FAST_READ32(kPointTypes + 16u * t + 4);
            std::fprintf(stderr, "[split] pontos tipo %u: %u (proximo %u):", t, n, FAST_READ32(kPointTypes + 16u * t + 12));
            for (uint32_t i = 0; i < n && i < 32 && pts >= 0x1000u; ++i)
                std::fprintf(stderr, " (%.0f,%.0f,%.0f)", rdF(rdram, pts + 0x20u * i), rdF(rdram, pts + 0x20u * i + 4), rdF(rdram, pts + 0x20u * i + 8));
            std::fprintf(stderr, "\n");
        }
    }
    void startPoint(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        static const bool log = std::getenv("WOTM_SPLITLOG") != nullptr;
        const uint32_t m = GPR_U32(ctx, 5) & 0x1FFFFFFFu, ra = ctx->pc == kStartPoint ? GPR_U32(ctx, 31) : 0u;
        static bool listed = false;
        if (log && ra && !listed)
        {
            listed = true;
            logPointLists(rdram);
        }
        int who = -1;
        for (int i = 0; i < 12 && who < 0; ++i)
            if ((FAST_READ32(kMonsters + 4u * uint32_t(i)) & 0x1FFFFFFFu) == m)
                who = i;
        const int32_t humanPoints = static_cast<int32_t>(FAST_READ32(kPointTypes + 16u + 4u));   // lista 1
        const bool asAi = ra && who >= humanPoints && who < multiPlayers(rdram) && FAST_READ32(m + kPlayerNum) == 1u;
        if (asAi)
            FAST_WRITE32(m + kPlayerNum, 2u);
        g_origStartPoint(rdram, ctx, runtime);
        if (asAi)
            FAST_WRITE32(m + kPlayerNum, 1u);
        if (log && ra && ctx->pc == ra)
        {
            const uint32_t cs = FAST_READ32(m + 0xC) & 0x1FFFFFFFu;
            std::fprintf(stderr, "[split] largada: m_monsters[%d] (playerNum %d) em %.0f,%.0f,%.0f\n", who,
                         static_cast<int32_t>(FAST_READ32(m + kPlayerNum)), rdF(rdram, cs + 0x10), rdF(rdram, cs + 0x14), rdF(rdram, cs + 0x18));
        }
    }

    // MotionBlurAA::init(vistas, ...) monta um pacote de 0x100 bytes por vista a partir de D_007319F0 + 0x8D0, num bloco de 0xB30: so
    // cabem 2 (a terceira passaria do fim do bss). Com os quadrantes o init fica com 2 vistas e o drawOverlay(campo, impar, vista) das
    // vistas 2 e 3 nao manda nada (o nativo nao desenha o borrao; so o GS emulado o veria).
    constexpr uint32_t kBlurInit = wotm::fn::init__12MotionBlurAAiff, kBlurDraw = wotm::fn::drawOverlay__12MotionBlurAAiii;
    PS2Runtime::RecompiledFunction g_origBlurInit = nullptr, g_origBlurDraw = nullptr;
    void blurInit(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kBlurInit && static_cast<int32_t>(GPR_U32(ctx, 4)) > 2 && splitPlayers(rdram))
            ctx->r[4] = _mm_set_epi64x(0, 2);
        g_origBlurInit(rdram, ctx, runtime);
    }
    void blurDraw(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kBlurDraw && GPR_U32(ctx, 6) >= 2u && splitPlayers(rdram))
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_origBlurDraw(rdram, ctx, runtime);
    }

    // scePad* com (porta, slot): os slots 1 a 3 viram o 0 da mesma porta; o scePadRead deles le os dispositivos do jogador
    // porta + 2 * slot
    constexpr uint32_t kPadSlotFns[] = {wotm::fn::scePadRead,          wotm::fn::scePadGetState,      wotm::fn::scePadGetReqState,
                                        wotm::fn::scePadSetReqState,   wotm::fn::scePadInfoAct,       wotm::fn::scePadInfoMode,
                                        wotm::fn::scePadSetMainMode,   wotm::fn::scePadSetActDirect,  wotm::fn::scePadSetActAlign,
                                        wotm::fn::scePadGetButtonMask, wotm::fn::scePadSetButtonInfo, wotm::fn::scePadInfoPressMode,
                                        wotm::fn::scePadEnterPressMode, wotm::fn::scePadExitPressMode};
    constexpr size_t kPadSlotN = sizeof(kPadSlotFns) / sizeof(kPadSlotFns[0]);
    PS2Runtime::RecompiledFunction g_padOrig[kPadSlotN] = {}, g_origPortOpen = nullptr, g_origPortClose = nullptr;

    void padSlotCall(size_t i, uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t slot = GPR_U32(ctx, 5);
        if (slot == 0u || slot > 3u)
        {
            g_padOrig[i](rdram, ctx, runtime);
            return;
        }
        const bool read = kPadSlotFns[i] == wotm::fn::scePadRead;
        ctx->r[5] = _mm_set_epi64x(0, 0);
        if (read)
            wotmPadSetPortBase(2 * static_cast<int>(slot));
        g_padOrig[i](rdram, ctx, runtime);
        if (read)
            wotmPadSetPortBase(0);
        ctx->r[5] = _mm_set_epi64x(0, static_cast<int64_t>(slot));
    }
    template <size_t I>
    void padSlotMirror(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { padSlotCall(I, rdram, ctx, runtime); }
    constexpr PS2Runtime::RecompiledFunction kPadSlotHooks[kPadSlotN] = {
        &padSlotMirror<0>, &padSlotMirror<1>, &padSlotMirror<2>, &padSlotMirror<3>,   &padSlotMirror<4>,   &padSlotMirror<5>,   &padSlotMirror<6>,
        &padSlotMirror<7>, &padSlotMirror<8>, &padSlotMirror<9>, &padSlotMirror<10>, &padSlotMirror<11>, &padSlotMirror<12>, &padSlotMirror<13>};

    void portOpen(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (GPR_U32(ctx, 5) == 0u || GPR_U32(ctx, 5) > 3u)
        {
            g_origPortOpen(rdram, ctx, runtime);
            return;
        }
        const uint32_t buf = GPR_U32(ctx, 6) & 0x1FFFFFFFu;   // scePadPortOpen(porta, slot, buffer de 32 bytes): o stub o zera
        if (buf >= 0x1000u && buf + 32u <= PS2_RAM_SIZE)
            std::memset(rdram + buf, 0, 32);
        SET_GPR_S32(ctx, 2, 1);
        ctx->pc = GPR_U32(ctx, 31);
    }

    void portClose(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (GPR_U32(ctx, 5) == 0u || GPR_U32(ctx, 5) > 3u)
        {
            g_origPortClose(rdram, ctx, runtime);
            return;
        }
        SET_GPR_S32(ctx, 2, 1);
        ctx->pc = GPR_U32(ctx, 31);
    }

    // Antes dos ganchos do scePadRead da camera livre e do lock-on alternado: eles embrulham este e veem o slot de verdade. Sempre
    // instalado: os ganchos so agem com 3 ou mais jogadores no Shell (o menu multijogador ou a WOTM_MATCH com WOTM_PLAYERS), e os
    // slots 1 a 3 das portas respondem como controles sem botao enquanto ninguem os alimenta.
    void install(PS2Runtime &runtime)
    {
        struct H { uint32_t addr; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; };
        const H hooks[] = {
            {kReestablish, &g_origReestablish, &reestablishViews}, {kGrapple, &g_origGrapple, &grappleConfig},
            {kSetPov, &g_origSetPov, &setCameraPov},               {kOkToUnify, &g_origOkToUnify, &okToUnify},
            {kResetLevel, &g_origResetLevel, &resetLevel},         {kInitCamera, &g_origInitCamera, &initCamera},
            {kLoadLevelFiles, &g_origLoadLevelFiles, &loadLevelFiles}, {kInitPlayers, &g_origInitPlayers, &initPlayers},
            {kInitRTState, &g_origInitRTState, &initRTState},      {kSetPlayer, &g_origSetPlayer, &setPlayerMonster},
            {kPlayerInitFn, &g_origPlayerInit, &playerInit},       {kLoadBanks, &g_origLoadBanks, &loadBanks},
            {kSoundInfo, &g_origSoundInfo, &soundInfo},            {kHudUpdate, &g_origHudUpdate, &hudUpdate},
            {kStartPoint, &g_origStartPoint, &startPoint},         {kHudInitAfter, &g_origHudInitAfter, &hudInitAfter},
            {kEvalNoAI, &g_origEvalNoAI, &evaluateBattle},         {kEvalAI, &g_origEvalAI, &evaluateBattle},
            {kBlurInit, &g_origBlurInit, &blurInit},               {kBlurDraw, &g_origBlurDraw, &blurDraw},
            {kViewUpdate, &g_origViewUpdate, &viewUpdate},         {kViewToggle, &g_origViewToggle, &viewToggle},
            {wotm::fn::scePadPortOpen, &g_origPortOpen, &portOpen}, {wotm::fn::scePadPortClose, &g_origPortClose, &portClose},
        };
        g_viewCreate = runtime.lookupFunction(kViewCreate);
        g_viewSetFov = runtime.lookupFunction(kViewSetFov);
        g_follow = runtime.lookupFunction(kFollow);
        g_setPov = runtime.lookupFunction(kSetPov);
        bool ok = g_viewCreate && g_viewSetFov && g_follow && g_setPov;
        for (const H &h : hooks)
            ok = ok && (*h.orig = runtime.lookupFunction(h.addr)) != nullptr;
        for (size_t i = 0; i < kPadSlotN; ++i)
            ok = ok && (g_padOrig[i] = runtime.lookupFunction(kPadSlotFns[i])) != nullptr;
        if (!ok)
        {
            std::fprintf(stderr, "[split] multijogador desligado: funcao do jogo ou scePad* sem funcao no runtime\n");
            return;
        }
        g_players = kMaxPlayers;
        for (const H &h : hooks)
            runtime.registerFunction(h.addr, h.fn);
        for (size_t i = 0; i < kPadSlotN; ++i)
            runtime.registerFunction(kPadSlotFns[i], kPadSlotHooks[i]);
    }
}

// Placar nativo (wotm_live.inc, wotmSharedHud): jogadores da luta de 5 a 8 em curso, ou 0 fora dela (sem viewUpdate da luta ha
// mais de 0,3 s: menu, carga)
int wotmSharedHudPlayers()
{
    using namespace wotm_split4;
    return nowMs() - g_sharedStamp.load(std::memory_order_relaxed) < 300 ? g_sharedN.load(std::memory_order_relaxed) : 0;
}
