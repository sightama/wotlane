// ABOUTME: Modo deterministico e partida em rede: passo fixo e relogio virtual (WOTM_FIXEDSTEP), semente fixa (WOTM_SEED), entrada por
// ABOUTME: quadro (atraso, gravar/repetir, rede WOTM_NET), hash do estado e copia da RAM. Incluido por wotm_overrides.cpp; roda na fronteira do quadro.
//
// Fronteira do quadro: entrada do timerWaitForMinUpdateRate (waitMinRateByVblanks), com o quadro do jogo pronto e o seguinte sem
// comecar. O indice do quadro recomeca na entrada do Shell::LoadLevelFiles (levelLoad), entao duas execucoes com a mesma WOTM_MATCH
// comparam o mesmo quadro da luta mesmo com boots de duracao diferente.
//   WOTM_FIXEDSTEP=<n>: cada quadro vale exatamente n fields (updateRate = 60 / n, dt = n / 60), em vez dos vblanks medidos pelo relogio
//     do host. Sem ela o dt depende de quanto o quadro demorou nesta maquina.
//   WOTM_STATEHASH=<arquivo>: por quadro grava {indice, g_frame, hash total, 256 hashes de blocos de 128 KB} dos 32 MB que o jogo ve
//     (tools/statediff.py compara duas execucoes e diz o primeiro quadro e os blocos que diferem).
//   WOTM_STATEDUMP=<indice>: grava a RAM do jogo (32 MB) desse quadro em state_<indice>.bin na pasta atual (o discroot).
//   WOTM_NET=<host>:<porta>, WOTM_NETTOKEN=<ficha>, WOTM_NETNAME=<nome>: partida em rede pelo wotm_server (veja "Partida em rede" abaixo).
namespace wotm_sync
{
    constexpr uint32_t kGuestRam = 32u * 1024u * 1024u, kBlocks = 256u, kBlockSize = kGuestRam / kBlocks;

    struct FrameHash
    {
        uint32_t index = 0, gFrame = 0;
        uint64_t total = 0;
        uint64_t blocks[kBlocks] = {};
    };

    uint64_t hashBytes(const uint8_t *p, size_t n)
    {
        uint64_t h = 0xcbf29ce484222325ull;   // FNV-1a em palavras de 64 bits
        const uint64_t *w = reinterpret_cast<const uint64_t *>(p);
        for (size_t i = 0; i < n / 8u; ++i)
        {
            h ^= w[i];
            h *= 0x100000001b3ull;
        }
        return h;
    }

    // WOTM_SEED=<n>: semente fixa das duas sortes do jogo (srand e gRand, Mersenne Twister). O mathfRandInit(0) a tira do relogio do
    // CD (segundo + minuto * 60), entao duas execucoes (ou dois PCs) comecavam a luta com sortes diferentes.
    constexpr uint32_t kRandInit = wotm::fn::mathfRandInit__Fii;
    PS2Runtime::RecompiledFunction g_origRandInit = nullptr;
    long long g_seed = -1;

    void randInitSeeded(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kRandInit && g_seed >= 0)
        {
            std::fprintf(stderr, "[sync] semente %lld (o jogo pediu %d)\n", g_seed, static_cast<int32_t>(GPR_U32(ctx, 4)));
            ctx->r[4] = _mm_set_epi64x(0, g_seed);
        }
        g_origRandInit(rdram, ctx, runtime);
    }

    // Entrada por quadro (base do netcode): o scePadRead(porta, slot, dados) devolve 32 bytes por controle (controle = porta + 2 * slot,
    // jogadores 1..4) e o jogo le cada controle duas vezes por quadro. Com o passo fixo, o estado de cada controle (botoes e analogicos,
    // wotm_net::PadState) vem de uma linha do tempo por quadro (native/net/wotm_lockstep.hpp): os dispositivos locais sao amostrados na
    // fronteira do quadro e valem WOTM_INPUTDELAY quadros depois (o atraso do lockstep; padrao 0), e as duas leituras do quadro veem o
    // mesmo estado. O runtime o formata como um controle de verdade (setPadOverrideState: modo e pressao vem do estado da porta).
    //   WOTM_INPUTREC=<arquivo>: grava {quadro, controle, estado} da entrada aplicada em cada quadro desde a carga da fase.
    //   WOTM_INPUTPLAY=<arquivo>: o jogo ve a entrada gravada (pelo indice do quadro) em vez da dos dispositivos.
    constexpr uint32_t kPadRead = wotm::fn::scePadRead;
    constexpr int kControllers = 4;
    struct InputRecord
    {
        uint32_t index;
        uint32_t controller;
        wotm_net::PadState state;
    };
    PS2Runtime *g_runtime = nullptr;
    PS2Runtime::RecompiledFunction g_origPadRead = nullptr;
    uint32_t g_inputDelay = 0;
    wotm_net::InputTimeline g_inputs(kControllers, 0);
    FILE *g_inputRec = nullptr;
    std::vector<InputRecord> g_inputPlay;
    bool g_levelLoaded = false;   // ja houve carga de fase (a gravacao e a repeticao contam os quadros dela)
    uint32_t g_index = 0;         // quadro desde a carga da fase (frame)
    bool g_loggedLocal = false, g_loggedApplied = false;

    // Partida em rede (WOTM_NET=<host>:<porta> e WOTM_NETTOKEN=<ficha>, que o lobby da: launcher ou tools/net_lobby.py). Cada PC roda a
    // simulacao inteira no modo deterministico e so as entradas viajam (native/net). O controle local (teclado e primeiro controle, os do
    // jogador 1 sem rede) vale para o jogador que o servidor deu a este PC; os outros vem do servidor. Antes da carga da fase a entrada e
    // neutra (o boot fica igual em todos), e um quadro so comeca com as entradas de todos para ele (netWaitFrame).
    std::unique_ptr<wotm_net::MatchClient> g_net;

    wotm_net::PadState inputFor(int controller, uint32_t frame)
    {
        if (g_net)
            return g_levelLoaded && controller < g_net->players() ? g_net->input(controller, frame) : wotm_net::PadState{};
        return g_inputs.get(controller, frame);
    }

    wotm_net::PadState sampleLocal(int controller)
    {
        uint8_t d[32];
        wotm_net::PadState s;
        if (g_runtime && g_runtime->padBackend().readState(controller & 1, controller >> 1, d, sizeof d))
        {
            s.buttons = static_cast<uint16_t>(d[2] | (d[3] << 8));
            s.rx = d[4];
            s.ry = d[5];
            s.lx = d[6];
            s.ly = d[7];
        }
        return s;
    }

    // comeco do quadro `index`: amostra os controles locais (valem em index + atraso)
    void sampleInputs(uint32_t index)
    {
        if (!g_inputPlay.empty())
            return;
        if (g_net)
        {
            if (!g_levelLoaded || g_net->status() != wotm_net::MatchClient::Status::Running)
                return;
            const wotm_net::PadState s = sampleLocal(0);
            g_net->pushLocal(index + g_inputDelay, s);
            if (!g_loggedLocal && s != wotm_net::PadState{})
            {
                g_loggedLocal = true;
                std::fprintf(stderr, "[sync] primeira entrada local: amostra do quadro %u vale no quadro %u\n", index, index + g_inputDelay);
            }
            return;
        }
        for (int c = 0; c < kControllers; ++c)
        {
            const wotm_net::PadState s = sampleLocal(c);
            const uint32_t at = g_inputs.scheduleLocal(c, index, s);
            if (g_levelLoaded && !g_loggedLocal && s != wotm_net::PadState{})
            {
                g_loggedLocal = true;
                std::fprintf(stderr, "[sync] primeira entrada local: amostra do quadro %u vale no quadro %u\n", index, at);
            }
        }
        g_inputs.discardBefore(index > 64u ? index - 64u : 0u);
    }

    void padReadLatched(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t port = GPR_U32(ctx, 4), slot = GPR_U32(ctx, 5);
        const bool ours = ctx->pc == kPadRead && port <= 1u && slot <= 1u;
        const wotm_net::PadState s = ours ? inputFor(static_cast<int>(port + 2u * slot), g_index) : wotm_net::PadState{};
        if (ours)
            ps2_stubs::setPadOverrideState(s.buttons, s.lx, s.ly, s.rx, s.ry);
        g_origPadRead(rdram, ctx, runtime);
        if (!ours)
            return;
        ps2_stubs::clearPadOverrideState();
        if (g_levelLoaded && !g_loggedApplied && s != wotm_net::PadState{})
        {
            g_loggedApplied = true;
            std::fprintf(stderr, "[sync] primeira entrada nao neutra aplicada no quadro %u\n", g_index);
        }
    }

    int g_fixedStep = 0;
    uint32_t g_virtualTics = 0;   // relogio do jogo no passo fixo (ticks do timer 1: 9619 por field), avancado em frame()
    uint32_t g_ticCalls = 0;      // timerGetCurTics do jogo neste quadro
    // timerGetCurTics no passo fixo: o relogio virtual do quadro mais um quantum por chamada (64 ticks, ~0,1 ms). Assim os lacos que
    // esperam o relogio andar (fadeOutMusicBlocking: 101 ms) terminam e o tempo que o jogo mede so depende do quadro e de quantas
    // vezes ele perguntou, nunca de quanto o host demorou.
    uint32_t callTics() { return g_virtualTics + 64u * g_ticCalls++; }

    // sceGsSyncV devolve a paridade do campo (o jogo a guarda na RAM); no passo fixo ela sai de uma contagem das chamadas
    constexpr uint32_t kSyncV = wotm::fn::sceGsSyncV;
    PS2Runtime::RecompiledFunction g_origSyncV = nullptr;
    uint32_t g_syncCalls = 0;

    void syncVParity(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kSyncV;
        const uint32_t ra = GPR_U32(ctx, 31);
        g_origSyncV(rdram, ctx, runtime);
        if (entry && ctx->pc == ra)
            SET_GPR_U32(ctx, 2, g_syncCalls++ & 1u);
    }

    // sceCdReadClock(sceCdCLOCK *) no passo fixo: data fixa (00:00:00 de 01/01/03, BCD) em vez do relogio do PC; o jogo a usa no
    // globalTimeString e, com semente 0, para semear a sorte
    constexpr uint32_t kReadClock = wotm::fn::sceCdReadClock;

    void readClockFixed(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t p = GPR_U32(ctx, 4) & 0x1FFFFFFFu;
        const uint8_t clock[8] = {0, 0x00, 0x00, 0x00, 0, 0x01, 0x01, 0x03};   // stat, segundo, minuto, hora, -, dia, mes, ano
        if (p + sizeof clock <= kGuestRam)
            std::memcpy(rdram + p, clock, sizeof clock);
        SET_GPR_U32(ctx, 2, 1u);
        ctx->pc = GPR_U32(ctx, 31);
    }
    FILE *g_hashFile = nullptr;
    long g_dumpAt = -1;

    void loadInputPlay(const char *path)
    {
        if (FILE *f = std::fopen(path, "rb"))
        {
            InputRecord r;
            while (std::fread(&r, sizeof r, 1, f) == 1)
                g_inputPlay.push_back(r);
            std::fclose(f);
        }
        std::fprintf(stderr, "[sync] entrada gravada: %zu registros de %s\n", g_inputPlay.size(), path);
    }

    // ---- partida em rede ----
    bool stopRequested() { return g_runtime && g_runtime->isStopRequested(); }

    // a partida nao pode seguir (recusa, dessincronia, conexao caida): a mensagem fica na tela e o jogo para ate a janela fechar
    void netFail(const std::string &why)
    {
        std::fprintf(stderr, "[net] parado: %s\n", why.c_str());
        wotm_netstatus::setMessage(why + "  (feche a janela)");
        wotm_netstatus::g_waiting = true;
        while (!stopRequested())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::string netNames(const std::vector<int> &players)
    {
        const std::vector<std::string> names = g_net->names();
        std::string s;
        for (int p : players)
            s += (s.empty() ? "" : ", ") + (p < static_cast<int>(names.size()) ? names[static_cast<size_t>(p)] : "jogador " + std::to_string(p + 1));
        return s;
    }

    // Entrada do Shell::LoadLevelFiles: espera todos chegarem a partida (o servidor mede o ping e decide o atraso)
    void netStart()
    {
        using Status = wotm_net::MatchClient::Status;
        wotm_netstatus::g_waiting = true;
        while (!g_net->waitStarted(100) && !stopRequested())
        {
            const Status s = g_net->status();
            if (s != Status::Connecting && s != Status::Waiting)
                return netFail(g_net->message());
            wotm_netstatus::setMessage(s == Status::Connecting ? g_net->message() : "Esperando os outros jogadores chegarem a partida");
        }
        wotm_netstatus::g_waiting = false;
        wotm_netstatus::setMessage("");
        g_inputDelay = g_net->delay();
        wotm_netstatus::g_delay = g_inputDelay;
        std::fprintf(stderr, "[net] inicio: jogador %d de %d, atraso %u quadros, ping %u ms\n", g_net->localPlayer() + 1, g_net->players(),
                     g_inputDelay, g_net->rttMs());
    }

    // Fim do quadro: o seguinte so comeca com as entradas de todos para ele. Parado mais de 250 ms, a tela diz quem falta; o tempo
    // parado sai dos prazos de vsync do EE (sem isso, na volta, os vsyncs vencidos viravam uma rajada de quadros).
    uint32_t g_netStalls = 0;
    double g_netStallMs = 0.0;

    void netWaitFrame(uint32_t frame)
    {
        wotm_netstatus::g_pingMs = g_net->rttMs();
        if (g_net->waitFrame(frame, 0))
            return;
        const auto t0 = std::chrono::steady_clock::now();
        wotm_netstatus::g_waiting = true;
        bool shown = false;
        while (!g_net->waitFrame(frame, 50) && !stopRequested())
        {
            if (g_net->status() != wotm_net::MatchClient::Status::Running)
                return netFail(g_net->message());
            if (std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(250))
            {
                wotm_netstatus::setMessage("Esperando " + netNames(g_net->missing(frame)));
                shown = true;
            }
        }
        if (shown)
            wotm_netstatus::setMessage("");
        wotm_netstatus::g_waiting = false;
        const auto waited = std::chrono::steady_clock::now() - t0;
        g_runtime->eeScheduler().shiftHostDeadlines(waited);
        ++g_netStalls;
        g_netStallMs += std::chrono::duration<double, std::milli>(waited).count();
        if ((g_netStalls & 63u) == 1u)
            std::fprintf(stderr, "[net] quadro %u: %u esperas pela rede, %.0f ms no total\n", frame, g_netStalls, g_netStallMs);
    }

    // Hash da RAM do jogo para o servidor comparar entre os PCs: 1 MB por quadro, 32 quadros por hash (o custo fica espalhado)
    constexpr uint32_t kNetHashFrames = 32u, kNetHashSlice = kGuestRam / kNetHashFrames;
    uint64_t g_netHash = 0;
    uint32_t g_netHashes = 0;

    void netHash(const uint8_t *rdram, uint32_t index)
    {
        const uint32_t k = index % kNetHashFrames;
        if (k == 0u)
            g_netHash = 0xcbf29ce484222325ull;
        g_netHash = (g_netHash ^ hashBytes(rdram + k * kNetHashSlice, kNetHashSlice)) * 0x100000001b3ull;
        if (k != kNetHashFrames - 1u)
            return;
        g_net->sendHash(index, g_netHash);
        if ((g_netHashes++ % 16u) == 0u)
            std::fprintf(stderr, "[net] hash do quadro %u: %016llx\n", index, static_cast<unsigned long long>(g_netHash));
    }

    // Identidade do build (todos os PCs precisam do mesmo exe): wotm.buildid ao lado do exe, gravado pelo tools\build_runtime.ps1
    std::string netBuildId()
    {
        char path[1024] = {};
        const unsigned long n = GetModuleFileNameA(nullptr, path, sizeof path);
        std::string dir(path, n);
        dir = dir.substr(0, dir.find_last_of("\\/") + 1);
        std::string id;
        if (FILE *f = std::fopen((dir + "wotm.buildid").c_str(), "rb"))
        {
            char buf[128] = {};
            const size_t r = std::fread(buf, 1, sizeof buf - 1, f);
            std::fclose(f);
            id.assign(buf, r);
            while (!id.empty() && (id.back() == '\n' || id.back() == '\r' || id.back() == ' '))
                id.pop_back();
        }
        return id.empty() ? "sem-buildid" : id;
    }

    // Configuracao que muda o estado do jogo (docs/plans/determinism-audit.md 1.7): igual em todos os PCs ou o servidor recusa
    std::string netConfigHash()
    {
        static const char *const kVars[] = {"WOTM_MATCH", "WOTM_PLAYERS", "WOTM_AIVSAI", "WOTM_RATEMIN", "WOTM_FIXEDSTEP", "WOTM_SEED",
                                            "WOTM_HIGHRAM", "WOTM_LOCKTOGGLE", "WOTM_ENEMYBARS", "WOTM_CHEATS", "WOTM_CLOAK", "WOTM_SAVEJSON",
                                            "WOTM_SAVEAUTO", "WOTM_FREECAM", "PS2X_FMV", "PS2X_SKIP_MOVIES", "PS2X_SKIP_CMOVIE", "PS2X_NOOP"};
        std::string all;
        for (const char *v : kVars)
        {
            const char *e = std::getenv(v);
            all += std::string(v) + "=" + (e ? e : "") + ";";
        }
        all.resize((all.size() + 7u) & ~size_t(7), '\0');   // hashBytes le palavras de 8 bytes
        char out[17];
        std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(hashBytes(reinterpret_cast<const uint8_t *>(all.data()), all.size())));
        return out;
    }

    void netInit(const char *addr)
    {
        std::string a(addr);
        const size_t colon = a.rfind(':');
        wotm_net::ClientConfig c;
        c.host = colon == std::string::npos ? a : a.substr(0, colon);
        c.port = colon == std::string::npos ? wotm_net::proto::kDefaultPort : static_cast<uint16_t>(std::atoi(a.c_str() + colon + 1));
        const char *token = std::getenv("WOTM_NETTOKEN"), *name = std::getenv("WOTM_NETNAME");
        c.token = token ? token : "";
        c.name = name && *name ? name : "jogador";
        c.buildId = netBuildId();
        c.configHash = netConfigHash();
        g_net = std::make_unique<wotm_net::MatchClient>();
        std::string err;
        if (!g_net->start(c, err))
        {
            std::fprintf(stderr, "[net] nao consegui iniciar a rede: %s\n", err.c_str());
            g_net.reset();
            return;
        }
        wotm_netstatus::g_active = true;
        wotm_netstatus::setMessage("Conectando a " + c.host + ":" + std::to_string(c.port));
        std::fprintf(stderr, "[net] conectando a %s:%u como %s (build %s, configuracao %s)\n", c.host.c_str(), c.port, c.name.c_str(),
                     c.buildId.c_str(), c.configHash.c_str());
    }

    void init(PS2Runtime &runtime)
    {
        g_runtime = &runtime;
        if (const char *e = std::getenv("WOTM_SEED"); e && *e && (g_origRandInit = runtime.lookupFunction(kRandInit)))
        {
            g_seed = std::clamp(std::atoll(e), 1LL, 0x7FFFFFFFLL);   // 0 faria o jogo ler o relogio
            runtime.registerFunction(kRandInit, &randInitSeeded);
        }
        if (const char *e = std::getenv("WOTM_FIXEDSTEP"); e && *e)
        {
            g_fixedStep = std::clamp(std::atoi(e), 1, 4);
            if ((g_origSyncV = runtime.lookupFunction(kSyncV)))
                runtime.registerFunction(kSyncV, &syncVParity);
            runtime.registerFunction(kReadClock, &readClockFixed);
            if ((g_origPadRead = runtime.lookupFunction(kPadRead)))
                runtime.registerFunction(kPadRead, &padReadLatched);
            if (const char *r = std::getenv("WOTM_INPUTREC"); r && *r)
                g_inputRec = std::fopen(r, "wb");
            if (const char *p = std::getenv("WOTM_INPUTPLAY"); p && *p)
                loadInputPlay(p);
            if (const char *d = std::getenv("WOTM_INPUTDELAY"); d && *d)
                g_inputDelay = static_cast<uint32_t>(std::clamp(std::atoi(d), 0, 30));
            g_inputs = wotm_net::InputTimeline(kControllers, g_inputDelay);
            sampleInputs(0);
            std::fprintf(stderr, "[sync] atraso de entrada: %u quadro(s)\n", g_inputDelay);
        }
        if (const char *e = std::getenv("WOTM_NET"); e && *e)
        {
            if (g_fixedStep)
                netInit(e);
            else
                std::fprintf(stderr, "[net] WOTM_NET precisa de WOTM_FIXEDSTEP (o launcher e o tools/net_lobby.py a poem): rede desligada\n");
        }
        if (const char *e = std::getenv("WOTM_STATEHASH"); e && *e)
            g_hashFile = std::fopen(e, "wb");
        if (const char *e = std::getenv("WOTM_STATEDUMP"); e && *e)
            g_dumpAt = std::atol(e);
        if (g_fixedStep || g_hashFile || g_dumpAt >= 0)
            std::fprintf(stderr, "[sync] passo fixo %d field(s), hash %s, copia no quadro %ld\n", g_fixedStep, g_hashFile ? "ligado" : "desligado", g_dumpAt);
    }

    // Entrada do Shell::LoadLevelFiles (loadLevelFilesTapped): o indice do quadro recomeca ja aqui, antes do primeiro quadro da luta
    // (as leituras do pad desse quadro usam o indice 0 nas duas execucoes, qualquer que tenha sido a duracao do boot)
    void levelLoad()
    {
        if (g_net && g_levelLoaded)
            std::fprintf(stderr, "[net] AVISO: segunda carga de fase na partida em rede (o indice do quadro recomeca; vai dessincronizar)\n");
        g_index = 0;
        g_levelLoaded = true;
        if (!g_fixedStep)
            return;
        if (g_net)
            netStart();
        g_inputs = wotm_net::InputTimeline(kControllers, g_inputDelay);
        for (const InputRecord &r : g_inputPlay)
            g_inputs.set(static_cast<int>(r.controller), r.index, r.state);
        sampleInputs(0);
    }

    // fieldCounterAtStart/fieldCounter: D_006F8DC0/D_006F8DBC; fieldsForAverage: D_006F8DE4 (fields desde o timerSetFrameStart)
    void frame(uint8_t *rdram, uint32_t fieldCounter, uint32_t fieldCounterAtStart, uint32_t fieldsForAverage)
    {
        if (g_fixedStep)
        {
            const uint32_t step = static_cast<uint32_t>(g_fixedStep);
            Ps2FastWrite32(rdram, fieldCounter, Ps2FastRead32(rdram, fieldCounterAtStart) + step);
            Ps2FastWrite32(rdram, fieldsForAverage, Ps2FastRead32(rdram, fieldsForAverage) + step);
            g_virtualTics += step * 9619u;
            g_ticCalls = 0;
            wotmSoundFrame(step);   // o som anda o mesmo tanto, na thread do jogo (o fim de cada som vira funcao dos quadros)
            if (g_inputRec && g_levelLoaded)
            {
                for (int c = 0; c < kControllers; ++c)
                {
                    const InputRecord r{g_index, static_cast<uint32_t>(c), inputFor(c, g_index)};
                    std::fwrite(&r, sizeof r, 1, g_inputRec);
                }
                if ((g_index & 63u) == 0u)
                    std::fflush(g_inputRec);
            }
            sampleInputs(g_index + 1u);
        }
        const uint32_t index = g_index++;
        if (g_hashFile)
        {
            FrameHash fh;
            fh.index = index;
            fh.gFrame = Ps2FastRead32(rdram, wotm::sym::g_frame);
            for (uint32_t b = 0; b < kBlocks; ++b)
                fh.blocks[b] = hashBytes(rdram + b * kBlockSize, kBlockSize);
            fh.total = hashBytes(reinterpret_cast<const uint8_t *>(fh.blocks), sizeof fh.blocks);
            std::fwrite(&fh, sizeof fh, 1, g_hashFile);
            if ((index & 63u) == 0u)
                std::fflush(g_hashFile);
        }
        if (g_dumpAt >= 0 && index == static_cast<uint32_t>(g_dumpAt))
        {
            const std::string name = "state_" + std::to_string(index) + ".bin";
            if (FILE *f = std::fopen(name.c_str(), "wb"))
            {
                std::fwrite(rdram, 1, kGuestRam, f);
                std::fclose(f);
                std::fprintf(stderr, "[sync] RAM do quadro %u em %s\n", index, name.c_str());
            }
        }
        if (g_net && g_levelLoaded && g_fixedStep)
        {
            netHash(rdram, index);
            netWaitFrame(g_index);
        }
    }
}
