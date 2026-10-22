// wotm_orbit.inc: camera moderna (orbital), item 7 do POS_FRONTEIRA.md; plano e etapas em
// docs/investigacoes/2026-10-22/camera_orbital_plano.md. Incluido no wotm_overrides.cpp, que chama daqui os ganchos que ja tem:
//   - Cameras::Update (o embrulho da camera livre): orbitPre antes do original, orbitPost depois;
//   - PadFlags::interpretInputs (o do lock-on): orbitInputEnter na entrada, orbitInputLeave quando a funcao de fato voltou;
//   - Camera::shakeCamera: guarda o tremor do quadro, que a orbita soma a posicao dela;
//   - TheGame::SetOkToUnify: CloseCam = 0 zera o m_camUnify de todos (nunca entra na vista unificada), como o wotm_split4.inc.
//
// Etapa 1 (orbita basica). Vale para a camera com POV 0 (normalUpdate) que segue o monstro de um jogador humano na vista dele, fora
// da vista unificada, do modo 9 (Crush), da camera livre e do WOTM_AIVSAI (lotes e corpus ficam com a camera do jogo). O jogo
// continua rodando o normalUpdate por baixo (inclinacao, balanco de rampa/escalada/mira, tremor, contorno); depois dele a orbita
// refaz a pose:
//   - posicao: o deslocamento do jogo (D10 mais o balanco, sqrt(|D40|/90 graus) * D50 ou D60) girado pelo nosso angulo em volta do
//     monstro (az = rumo do monstro para a camera) em vez do rumo das costas dele; o analogico direito gira az (X) e a elevacao (Y);
//     o pivo segue o monstro com atraso curto e limitado;
//   - guinada: rot.z = rumo da camera para o monstro; inclinacao: a do jogo menos o que a elevacao subiu (o monstro fica no mesmo
//     lugar da tela); a inclinacao pura do jogo volta ao _cs antes do quadro seguinte (senao a suavizacao dele derivaria);
//   - recentralizar (CamRecenter): ~1,5 s sem mexer no analogico direito, com o monstro andando, az volta devagar para as costas;
//   - o jogo e neutralizado so no que briga com a orbita: trava do setor traseiro (E60 = 1), volta as costas (DA4 = DF4 = 0).
// Ligando (primeiro quadro, volta de outro POV, da vista unificada ou da camera livre) az sai da posicao que o outro modo deixou e a
// pose desliza da do jogo para a da orbita em ~20 quadros: sem salto. Corte do jogo (D98) com a orbita ja ligada (reinicio de
// rodada, renascer) recomeca az das costas.
//
// Controle: com a orbita dirigindo a vista do jogador, o interpretInputs roda com PadFlags +0x16E4/+0x16E8 = 1 (configuracao
// relativa a tela, updateInput*ConfigSR, que le o _cs desta camera) e os valores do jogo voltam na saida; os bytes do analogico
// direito (GamePad 0x10..0x13: esquerda, direita, cima, baixo) vao para a orbita e chegam zerados ao jogo (na CR girariam o monstro,
// na SR tambem, quando o esquerdo esta parado). Liga so num quadro com as direcoes paradas, como a troca do proprio jogo.
namespace
{
    namespace orbit
    {
        constexpr uint32_t kCameras = wotm::sym::x_7Cameras_m_cameras, kNumCameras = wotm::sym::x_7Cameras_m_numCameras, kCamStride = 0xEB0u,
                           kUnified = wotm::sym::gUseUnifiedView, kGameMode = kGame + 0x1203C8u, kSlots = kGame + 0xB80u,
                           kSlotStride = 0x11190u, kNumSlots = kGame + 0x1203D4u;
        // Camera (docs/investigacoes/2026-10-22/camera_normalupdate.md)
        constexpr uint32_t kCamCs = 0xCF0u, kCamMonster = 0xCF4u, kCamPov = 0xCF8u, kCamOffset = 0xD10u, kCamSwayUp = 0xD50u,
                           kCamSwayDown = 0xD60u, kCamPitchExtra = 0xD40u, kCamIdle = 0xDA4u, kCamDrag = 0xDF4u, kCamCut = 0xD98u,
                           kCamSectorLock = 0xE60u;
        // Monster
        constexpr uint32_t kMonCs = 0xCu, kMonDead = 0xE8u, kMonPinTarget = 0x6C04u, kMonCameraView = 0x6CD8u, kMonCamUnify = 0x7970u;
        // PadFlags
        constexpr uint32_t kPfOwner = 0x1810u, kPfConfigActive = 0x16E4u, kPfConfigWanted = 0x16E8u;
        constexpr int kMaxCams = 4;
        constexpr float kPi = 3.14159265f;

        struct Cam
        {
            bool active = false;     // a orbita escreveu a pose no quadro anterior
            uint32_t mon = 0;        // monstro seguido quando ligou
            float az = 0.f;          // rumo do monstro para a camera
            float elev = 0.f;        // elevacao somada pelo analogico (radianos)
            float pivot[3] = {};     // ponto em volta do qual gira (segue o monstro com atraso)
            float lastMon[3] = {};
            float gamePitch = 0.f;   // rot.x do jogo (sem a nossa correcao), devolvido ao _cs antes do quadro seguinte
            float blend = 0.f;       // 1 = pose do jogo, 0 = da orbita (entrada suave)
            int still = 0;           // quadros sem mexer no analogico direito
            bool cutSeen = false;    // D98 ligado na entrada deste quadro
            float shake[3] = {};     // deslocamento do shakeCamera neste quadro
        } g_cam[kMaxCams];

        struct Input
        {
            bool driving = false;    // a orbita dirige a vista deste jogador (orbitPost)
            bool forced = false;     // configuracao SR imposta no interpretInputs em curso
            bool pending = false;    // forcado, mas a funcao nao voltou ainda (fatia de tempo): restaurar na proxima entrada
            uint32_t pf = 0, saved4 = 0, saved8 = 0;
            float stick[2] = {};     // analogico direito do ultimo quadro (x direita, y cima), -1..1
        } g_in[4];

        float rdF(uint8_t *rdram, uint32_t a) { float v; std::memcpy(&v, rdram + a, 4); return v; }
        void wrF(uint8_t *rdram, uint32_t a, float v) { std::memcpy(rdram + a, &v, 4); }
        uint32_t camAddr(int v) { return kCameras + kCamStride * uint32_t(v); }
        bool okAddr(uint32_t a, uint32_t n) { return a >= 0x1000u && a + n <= PS2_RAM_SIZE; }
        float wrapPi(float a)
        {
            while (a > kPi) a -= 2.f * kPi;
            while (a < -kPi) a += 2.f * kPi;
            return a;
        }
        float headingTo(float dx, float dy) { return std::atan2(dx, dy); }   // 0 = +Y, positivo para +X

        bool enabled() { return wotm::settings::on(wotm::settings::Camera) && !g_aiVsAi && !g_wotmFreeCam.load(std::memory_order_relaxed); }

        // Camera v elegivel neste quadro; mon = monstro, slot = jogador
        bool eligible(uint8_t *rdram, int v, uint32_t &mon, int &slot)
        {
            if (v >= kMaxCams || !enabled() || FAST_READ32(kUnified) || FAST_READ32(kGameMode) == 9u)
                return false;
            const uint32_t c = camAddr(v);
            if (FAST_READ32(c + kCamPov) != 0u)
                return false;
            mon = FAST_READ32(c + kCamMonster) & 0x1FFFFFFFu;
            const uint32_t cs = FAST_READ32(c + kCamCs) & 0x1FFFFFFFu;
            if (!okAddr(mon, kMonCamUnify + 4u) || !okAddr(cs, 0x70u) || !isPlayerMonster(rdram, FAST_READ32(c + kCamMonster), &slot))
                return false;
            if (static_cast<int32_t>(FAST_READ32(mon + kMonCameraView)) != v)
                return false;   // camera 2 seguindo quem agarra no 1P etc.: a vista do jogador e outra
            const uint32_t mcs = FAST_READ32(mon + kMonCs) & 0x1FFFFFFFu;
            return okAddr(mcs, 0x70u);
        }

        // Teste sem controle: WOTM_ORBITSTICK="quadro:quadros:x:y;..." poe o analogico direito em (x, y) (-1..1, y para cima) do
        // quadro dado (contado nas chamadas do Cameras::Update) por tantos quadros
        void testStick(unsigned frame, float &sx, float &sy)
        {
            struct Step { unsigned f0, n; float x, y; };
            static std::vector<Step> steps;
            static bool parsed = false;
            if (!parsed)
            {
                parsed = true;
                if (const char *e = std::getenv("WOTM_ORBITSTICK"))
                    for (const char *p = e; *p;)
                    {
                        Step s{};
                        if (std::sscanf(p, "%u:%u:%f:%f", &s.f0, &s.n, &s.x, &s.y) == 4)
                            steps.push_back(s);
                        const char *semi = std::strchr(p, ';');
                        if (!semi)
                            break;
                        p = semi + 1;
                    }
            }
            for (const Step &s : steps)
                if (frame >= s.f0 && frame < s.f0 + s.n)
                {
                    sx = s.x;
                    sy = s.y;
                }
        }

        int numCams(uint8_t *rdram) { return std::clamp(static_cast<int32_t>(FAST_READ32(kNumCameras)), 0, kMaxCams); }

        void pre(uint8_t *rdram)
        {
            const int n = numCams(rdram);
            for (int v = 0; v < n; ++v)
            {
                Cam &s = g_cam[v];
                uint32_t mon = 0;
                int slot = 0;
                s.cutSeen = false;
                std::memset(s.shake, 0, sizeof s.shake);
                if (!eligible(rdram, v, mon, slot))
                    continue;
                const uint32_t c = camAddr(v), cs = FAST_READ32(c + kCamCs) & 0x1FFFFFFFu;
                s.cutSeen = FAST_READ32(c + kCamCut) != 0u;
                if (s.active && s.mon == mon)
                    wrF(rdram, cs + 0x60, s.gamePitch);   // a inclinacao do jogo segue suavizando da dela
                FAST_WRITE32(c + kCamSectorLock, 1);       // sem a trava do setor traseiro
                FAST_WRITE32(c + kCamIdle, 0);             // sem a volta as costas
                FAST_WRITE32(c + kCamDrag, 0);
            }
        }

        void post(uint8_t *rdram)
        {
            static const bool log = std::getenv("WOTM_ORBITLOG") != nullptr;
            static unsigned frames = 0;
            ++frames;
            const int n = numCams(rdram);
            bool driving[4] = {};
            for (int v = 0; v < kMaxCams; ++v)
            {
                Cam &s = g_cam[v];
                uint32_t mon = 0;
                int slot = 0;
                if (v >= n || !eligible(rdram, v, mon, slot))
                {
                    if (s.active && log)
                        std::fprintf(stderr, "[orbit] camera %d: desligada (pov %d, unificada %d)\n", v,
                                     v < n ? static_cast<int32_t>(FAST_READ32(camAddr(v) + kCamPov)) : -1, FAST_READ32(kUnified));
                    s.active = false;
                    continue;
                }
                const uint32_t c = camAddr(v), cs = FAST_READ32(c + kCamCs) & 0x1FFFFFFFu, mcs = FAST_READ32(mon + kMonCs) & 0x1FFFFFFFu;
                float mt[3], game[3];
                for (int i = 0; i < 3; ++i)
                {
                    mt[i] = rdF(rdram, mcs + 0x10 + 4 * i);
                    game[i] = rdF(rdram, cs + 0x10 + 4 * i);
                }
                const float monHeading = rdF(rdram, mcs + 0x68);
                Input &in = g_in[slot];
                const bool restart = !s.active || s.mon != mon;
                if (restart || s.cutSeen)
                {
                    // de onde o jogo (ou o corte dele) deixou a camera
                    const float dx = game[0] - mt[0], dy = game[1] - mt[1];
                    s.az = dx * dx + dy * dy > 1.f ? headingTo(dx, dy) : wrapPi(monHeading + kPi);
                    s.elev = 0.f;
                    std::memcpy(s.pivot, mt, sizeof mt);
                    std::memcpy(s.lastMon, mt, sizeof mt);
                    s.blend = restart && !s.cutSeen ? 1.f : 0.f;
                    s.still = 0;
                    s.mon = mon;
                    in.stick[0] = in.stick[1] = 0.f;
                    if (log)
                        std::fprintf(stderr, "[orbit] camera %d: ligada (jogador %d, az %.1f, rumo do monstro %.1f%s)\n", v, slot + 1,
                                     s.az * 57.2958f, monHeading * 57.2958f, s.cutSeen ? ", corte do jogo" : "");
                }
                s.active = true;
                driving[slot] = true;

                // analogico direito: ~170 graus/s no maximo, a 60 quadros/s
                float sx = in.stick[0], sy = in.stick[1];
                testStick(frames, sx, sy);
                const bool moved = std::fabs(sx) > 0.05f || std::fabs(sy) > 0.05f;
                s.az = wrapPi(s.az + sx * 0.05f);
                s.elev = std::clamp(s.elev - sy * 0.03f, -0.6f, 0.9f);
                s.still = moved ? 0 : s.still + 1;

                // monstro andando: deslocamento horizontal do quadro
                const float vx = mt[0] - s.lastMon[0], vy = mt[1] - s.lastMon[1];
                std::memcpy(s.lastMon, mt, sizeof mt);
                if (wotm::settings::on(wotm::settings::CamRecenter) && s.still > 90 && vx * vx + vy * vy > 1.f)
                {
                    const float back = wrapPi(monHeading + kPi);
                    s.az = wrapPi(s.az + wrapPi(back - s.az) * 0.025f);
                    s.elev *= 0.975f;
                }

                // pivo: segue o monstro com atraso curto, limitado (altura mais devagar: o pulo nao sacode a camera)
                for (int i = 0; i < 3; ++i)
                {
                    const float rate = i < 2 ? 0.3f : 0.12f, lim = i < 2 ? 40.f : 90.f;
                    s.pivot[i] += (mt[i] - s.pivot[i]) * rate;
                    s.pivot[i] = std::clamp(s.pivot[i], mt[i] - lim, mt[i] + lim);
                }

                // deslocamento do jogo no referencial do monstro (x direita, y frente, z cima): D10 mais o balanco
                const float pitchExtra = rdF(rdram, c + kCamPitchExtra);
                const float k = std::sqrt(std::fabs(pitchExtra) / (kPi * 0.5f));
                const uint32_t sway = pitchExtra < 0.f ? kCamSwayDown : kCamSwayUp;
                const float offY = rdF(rdram, c + kCamOffset + 4) + (pitchExtra != 0.f ? k * rdF(rdram, c + sway + 4) : 0.f);
                const float offZ = rdF(rdram, c + kCamOffset + 8) + (pitchExtra != 0.f ? k * rdF(rdram, c + sway + 8) : 0.f);
                const float back = -offY, len = std::sqrt(back * back + offZ * offZ);
                const float e0 = std::atan2(offZ, back), e = std::clamp(e0 + s.elev, -0.25f, 1.45f);
                const float horiz = len * std::cos(e), height = len * std::sin(e);

                float pos[3] = {s.pivot[0] + std::sin(s.az) * horiz, s.pivot[1] + std::cos(s.az) * horiz, s.pivot[2] + height};
                if (s.blend > 0.001f)
                {
                    for (int i = 0; i < 3; ++i)
                        pos[i] += (game[i] - pos[i]) * s.blend;
                    s.blend *= 0.85f;
                }
                else
                    s.blend = 0.f;
                for (int i = 0; i < 3; ++i)
                    pos[i] += s.shake[i];

                WotmFreeCamPose p;
                std::memcpy(p.pos, pos, sizeof pos);
                p.heading = headingTo(mt[0] - pos[0], mt[1] - pos[1]);
                s.gamePitch = rdF(rdram, cs + 0x60);
                p.pitch = std::clamp(s.gamePitch - (e - e0), -1.55f, 1.55f);
                float ax[3][3];
                wotmFreeCamAxes(p, ax[0], ax[1], ax[2]);
                for (int i = 0; i < 3; ++i)
                {
                    wrF(rdram, cs + 0x10 + 4 * i, p.pos[i]);
                    for (int r = 0; r < 3; ++r)
                        wrF(rdram, cs + 0x20 + 0x10 * r + 4 * i, ax[r][i]);
                }
                for (int r = 0; r < 3; ++r)
                    wrF(rdram, cs + 0x20 + 0x10 * r + 0xC, 0.f);
                wrF(rdram, cs + 0x60, p.pitch);
                wrF(rdram, cs + 0x64, 0.f);
                wrF(rdram, cs + 0x68, p.heading);
                if (log && frames % 60 == 0)
                    std::fprintf(stderr, "[orbit] camera %d: az %.1f (costas %.1f) elev %.1f dist %.0f alt %.0f analogico %.2f,%.2f parado %d\n", v,
                                 s.az * 57.2958f, wrapPi(monHeading + kPi) * 57.2958f, s.elev * 57.2958f, horiz, height, sx, sy, s.still);
            }
            for (int i = 0; i < 4; ++i)
                g_in[i].driving = driving[i];
        }

        // Camera::shakeCamera (a0 = camera): o tremor do quadro e o que ele somou ao cs->trans
        PS2Runtime::RecompiledFunction g_origShake = nullptr;
        void shakeHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t c = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
            int v = -1;
            uint32_t cs = 0;
            if (c >= kCameras && c < kCameras + kCamStride * kMaxCams && (c - kCameras) % kCamStride == 0u)
            {
                v = static_cast<int>((c - kCameras) / kCamStride);
                cs = FAST_READ32(c + kCamCs) & 0x1FFFFFFFu;
                if (!okAddr(cs, 0x20u))
                    v = -1;
            }
            float before[3] = {};
            if (v >= 0)
                for (int i = 0; i < 3; ++i)
                    before[i] = rdF(rdram, cs + 0x10 + 4 * i);
            const uint32_t ra = GPR_U32(ctx, 31);
            g_origShake(rdram, ctx, runtime);
            if (v >= 0 && ctx->pc == ra)
                for (int i = 0; i < 3; ++i)
                    g_cam[v].shake[i] = rdF(rdram, cs + 0x10 + 4 * i) - before[i];
        }

        // Entrada do interpretInputs (a0 = PadFlags, a1 = GamePad). Devolve o jogador forcado (ou -1)
        int inputEnter(uint8_t *rdram, uint32_t pf, uint32_t gp)
        {
            for (Input &in : g_in)
                if (in.pending && in.pf == pf)
                {
                    // a chamada anterior nao voltou por este gancho: devolve os valores do jogo se ninguem os trocou
                    if (FAST_READ32(pf + kPfConfigActive) == 1u && FAST_READ32(pf + kPfConfigWanted) == 1u)
                    {
                        FAST_WRITE32(pf + kPfConfigActive, in.saved4);
                        FAST_WRITE32(pf + kPfConfigWanted, in.saved8);
                    }
                    in.pending = in.forced = false;
                }
            if (!okAddr(pf, kPfOwner + 4u) || !okAddr(gp, 0x14u))
                return -1;
            int slot = -1;
            const uint32_t owner = FAST_READ32(pf + kPfOwner);
            if (!isPlayerMonster(rdram, owner, &slot) || slot < 0 || slot > 3)
                return -1;
            Input &in = g_in[slot];
            uint8_t *b = rdram + gp;
            if (!in.driving || !enabled())
            {
                in.forced = false;
                in.stick[0] = in.stick[1] = 0.f;
                return -1;
            }
            // analogico direito para a orbita, zerado para o jogo
            in.stick[0] = (float(b[0x11]) - float(b[0x10])) / 255.f;
            in.stick[1] = (float(b[0x12]) - float(b[0x13])) / 255.f;
            b[0x10] = b[0x11] = b[0x12] = b[0x13] = 0;
            const uint32_t owner2 = owner & 0x1FFFFFFFu;
            const uint32_t e4 = FAST_READ32(pf + kPfConfigActive), e8 = FAST_READ32(pf + kPfConfigWanted);
            const bool dead = okAddr(owner2, kMonDead + 1u) && rdram[owner2 + kMonDead] != 0;
            if (dead || e4 != 0u || e8 != 0u)
            {
                in.forced = false;   // o jogo ja pediu a SR (Crush, fim do planeio) ou esta trocando: fica com ele
                return -1;
            }
            const bool still = !b[0xC] && !b[0xD] && !b[0xE] && !b[0xF];
            if (!in.forced && !still)
                return -1;
            in.forced = true;
            in.pending = true;
            in.pf = pf;
            in.saved4 = e4;
            in.saved8 = e8;
            FAST_WRITE32(pf + kPfConfigActive, 1);
            FAST_WRITE32(pf + kPfConfigWanted, 1);
            return slot;
        }

        // Saida do interpretInputs (so quando a funcao voltou): devolve a configuracao do jogo
        void inputLeave(uint8_t *rdram, int slot)
        {
            if (slot < 0)
                return;
            Input &in = g_in[slot];
            if (!in.pending)
                return;
            if (FAST_READ32(in.pf + kPfConfigActive) == 1u && FAST_READ32(in.pf + kPfConfigWanted) == 1u)
            {
                FAST_WRITE32(in.pf + kPfConfigActive, in.saved4);
                FAST_WRITE32(in.pf + kPfConfigWanted, in.saved8);
            }
            else
                in.forced = false;   // o jogo mudou a configuracao no meio (agarrao, morte): vale a dele
            in.pending = false;
        }

        // TheGame::SetOkToUnify (comeco de cada quadro): CloseCam = 0 -> ninguem unifica
        PS2Runtime::RecompiledFunction g_origOkToUnify = nullptr;
        void okToUnifyHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ra = GPR_U32(ctx, 31);
            g_origOkToUnify(rdram, ctx, runtime);
            if (ctx->pc != ra || wotm::settings::on(wotm::settings::CloseCam))
                return;
            const uint32_t slots = std::min<uint32_t>(FAST_READ32(kNumSlots), 16u);
            for (uint32_t i = 0; i < slots; ++i)
                FAST_WRITE32(kSlots + kSlotStride * i + kMonCamUnify, 0);
        }

        void install(PS2Runtime &runtime)
        {
            constexpr uint32_t kShake = wotm::fn::shakeCamera__6Camera, kOkToUnify = wotm::fn::SetOkToUnify__7TheGame;
            if ((g_origShake = runtime.lookupFunction(kShake)))
                runtime.registerFunction(kShake, &shakeHook);
            if ((g_origOkToUnify = runtime.lookupFunction(kOkToUnify)))
                runtime.registerFunction(kOkToUnify, &okToUnifyHook);
            std::fprintf(stderr, "[wotm] camera %s (WOTM_CAMERA), recentralizar %s, camera de perto %s (WOTM_CLOSECAM)%s\n",
                         wotm::settings::on(wotm::settings::Camera) ? "moderna" : "classica",
                         wotm::settings::on(wotm::settings::CamRecenter) ? "ligado" : "desligado",
                         wotm::settings::on(wotm::settings::CloseCam) ? "ligada" : "desligada",
                         g_aiVsAi ? "; com WOTM_AIVSAI fica a classica" : "");
        }
    }
}
