// ABOUTME: Modo deterministico para o netcode: passo fixo e relogio virtual (WOTM_FIXEDSTEP), semente fixa (WOTM_SEED), entrada por
// ABOUTME: quadro (gravar/repetir), hash do estado por quadro e copia da RAM. Incluido por wotm_overrides.cpp; roda na fronteira do quadro.
//
// Fronteira do quadro: entrada do timerWaitForMinUpdateRate (waitMinRateByVblanks), com o quadro do jogo pronto e o seguinte sem
// comecar. O indice do quadro recomeca na entrada do Shell::LoadLevelFiles (levelLoad), entao duas execucoes com a mesma WOTM_MATCH
// comparam o mesmo quadro da luta mesmo com boots de duracao diferente.
//   WOTM_FIXEDSTEP=<n>: cada quadro vale exatamente n fields (updateRate = 60 / n, dt = n / 60), em vez dos vblanks medidos pelo relogio
//     do host. Sem ela o dt depende de quanto o quadro demorou nesta maquina.
//   WOTM_STATEHASH=<arquivo>: por quadro grava {indice, g_frame, hash total, 256 hashes de blocos de 128 KB} dos 32 MB que o jogo ve
//     (tools/statediff.py compara duas execucoes e diz o primeiro quadro e os blocos que diferem).
//   WOTM_STATEDUMP=<indice>: grava a RAM do jogo (32 MB) desse quadro em state_<indice>.bin na pasta atual (o discroot).
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
    // jogadores 1..4) e o jogo le cada controle duas vezes por quadro. Com o passo fixo, a primeira leitura do quadro e guardada e a
    // segunda devolve a mesma (o que o jogo ve num quadro nao muda no meio dele).
    //   WOTM_INPUTREC=<arquivo>: grava {quadro, controle, 32 bytes} da entrada guardada de cada quadro.
    //   WOTM_INPUTPLAY=<arquivo>: o jogo ve a entrada gravada (pelo indice do quadro) em vez da dos dispositivos.
    constexpr uint32_t kPadRead = wotm::fn::scePadRead;
    constexpr int kControllers = 4;
    struct InputRecord
    {
        uint32_t index;
        uint32_t controller;
        uint8_t data[32];
    };
    PS2Runtime::RecompiledFunction g_origPadRead = nullptr;
    uint8_t g_latch[kControllers][32] = {};
    bool g_latched[kControllers] = {};
    FILE *g_inputRec = nullptr;
    std::vector<InputRecord> g_inputPlay;
    size_t g_playPos = 0;
    uint32_t g_index = 0;   // quadro desde a carga da fase (frame)

    const uint8_t *playedInput(uint32_t index, uint32_t controller)
    {
        while (g_playPos < g_inputPlay.size() && g_inputPlay[g_playPos].index < index)
            ++g_playPos;
        for (size_t i = g_playPos; i < g_inputPlay.size() && g_inputPlay[i].index == index; ++i)
            if (g_inputPlay[i].controller == controller)
                return g_inputPlay[i].data;
        return nullptr;
    }

    void padReadLatched(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool entry = ctx->pc == kPadRead;
        const uint32_t port = GPR_U32(ctx, 4), slot = GPR_U32(ctx, 5), data = GPR_U32(ctx, 6) & 0x1FFFFFFFu, ra = GPR_U32(ctx, 31);
        g_origPadRead(rdram, ctx, runtime);
        const uint32_t c = port + 2u * slot;
        if (!entry || ctx->pc != ra || port > 1u || slot > 1u || GPR_U32(ctx, 2) == 0u || data + 32u > kGuestRam)
            return;
        if (!g_latched[c])
        {
            if (const uint8_t *p = g_inputPlay.empty() ? nullptr : playedInput(g_index, c))
                std::memcpy(g_latch[c], p, 32);
            else
                std::memcpy(g_latch[c], rdram + data, 32);
            g_latched[c] = true;
            if (g_inputRec)
            {
                InputRecord r{g_index, c, {}};
                std::memcpy(r.data, g_latch[c], 32);
                std::fwrite(&r, sizeof r, 1, g_inputRec);
            }
        }
        std::memcpy(rdram + data, g_latch[c], 32);
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
        std::stable_sort(g_inputPlay.begin(), g_inputPlay.end(), [](const InputRecord &a, const InputRecord &b) { return a.index < b.index; });
        std::fprintf(stderr, "[sync] entrada gravada: %zu registros de %s\n", g_inputPlay.size(), path);
    }

    void init(PS2Runtime &runtime)
    {
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
        g_index = 0;
        g_playPos = 0;
        std::fill(std::begin(g_latched), std::end(g_latched), false);
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
            std::fill(std::begin(g_latched), std::end(g_latched), false);
            if (g_inputRec && (g_index & 63u) == 0u)
                std::fflush(g_inputRec);
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
    }
}
