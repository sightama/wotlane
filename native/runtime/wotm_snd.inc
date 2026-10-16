// wotm_snd.inc: som nativo (incluido por wotm_live.inc, no escopo global de ps2_runtime.cpp). Ligado com PS2X_SND=1.
// Troca as funcoes snd_* da lib989snd do EE (que mandam comandos por RPC ao 989snd no IOP) pelo motor de native/wotm_snd.hpp
// (repo WoTM): bancos lidos do disco, sons e streams mixados no host. Com isso os IRX de som (LIBSD, 989SND, 989ERR) nao
// sao carregados no IOP emulado. Formatos e API em docs/FORMATOS.md ("Som").
// PS2X_SND_LOG=1: uma linha por chamada (menos as por quadro: volume/pan, "ainda tocando").
#if __has_include("wotm_snd.hpp")
#include "raylib.h"   // antes do motor: liga Engine::startOutput (AudioStream)
#include "wotm_snd.hpp"
#include <cctype>
#include <cstring>
#include "Kernel/Stubs/CD.h"
#include <fstream>
#define WOTM_HAVE_SND 1

namespace wotm_snd_hooks
{
using wotm::snd::Engine;

static bool g_log = false;
static PS2Runtime::RecompiledFunction g_origLoadModule = nullptr;

inline uint32_t arg(R5900Context *ctx, int i) { return getRegU32(ctx, 4 + i); }   // a0..a3, t0..t3 (EABI: 8 em registradores)
inline int32_t sarg(R5900Context *ctx, int i) { return static_cast<int32_t>(getRegU32(ctx, 4 + i)); }
inline uint32_t stackArg(uint8_t *rdram, R5900Context *ctx, uint32_t off)
{
    uint32_t v = 0;
    std::memcpy(&v, rdram + ((getRegU32(ctx, 29) + off) & PS2_RAM_MASK), 4);
    return v;
}

// Callback do jogo (quase sempre soundPlayCB: *user = valor). O 989snd chama no snd_FlushSoundCommands seguinte; aqui e na hora.
inline void callGuest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, uint32_t fn, uint32_t a0, uint32_t a1)
{
    if (!fn)
        return;
    PS2Runtime::RecompiledFunction f = rt->lookupFunction(fn);
    if (!f)
        return;
    const R5900Context saved = *ctx;
    ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a0)));
    ctx->r[5] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a1)));
    ctx->r[31] = _mm_set_epi64x(0, 0);
    ctx->pc = fn;
    f(rdram, ctx, rt);
    *ctx = saved;
}

// LSN -> bytes: arquivos soltos (cdRoot, LBNs do runtime) ou a ISO de PS2X_CD_IMAGE
inline bool readLsnRaw(uint32_t lsn, uint32_t sectors, uint8_t *dst, const char *&from)
{
    from = "discroot";
    const size_t bytes = size_t(sectors) * 2048;
    std::memset(dst, 0, bytes);
    const ps2_stubs::CdDebugSnapshot snap = ps2_stubs::getCdDebugSnapshot();
    std::filesystem::path path;
    uint64_t offset = 0;
    for (const auto &f : snap.files)
        if (lsn >= f.baseLbn && lsn < f.baseLbn + f.sectors)
        {
            path = f.hostPath;
            offset = uint64_t(lsn - f.baseLbn) * 2048;
            break;
        }
    if (path.empty())
    {
        if (snap.cdImage.empty())
            return false;
        path = snap.cdImage;
        offset = uint64_t(lsn) * 2048;
        from = "ISO";
    }
    std::ifstream in(path, std::ios::binary);
    if (!in.seekg(static_cast<std::streamoff>(offset)))
        return false;
    in.read(reinterpret_cast<char *>(dst), static_cast<std::streamsize>(bytes));
    return in.gcount() > 0;
}
inline bool readLsn(uint32_t lsn, uint32_t sectors, uint8_t *dst)
{
    const char *from = "?";
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = readLsnRaw(lsn, sectors, dst, from);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    wotmBoot("som: LSN %u, %u setores (%s) em %.1f ms%s", lsn, sectors, from, ms, ok ? "" : " FALHOU");
    return ok;
}

#define SND_LOG(...) do { if (g_log) std::fprintf(stderr, "[snd] " __VA_ARGS__); } while (0)

// --- sistema e bancos
void startSoundSystem(uint8_t *, R5900Context *ctx, PS2Runtime *)
{
    const bool ok = Engine::get().startOutput();
    std::fprintf(stderr, "[snd] snd_StartSoundSystem: som nativo %s\n", ok ? "ligado (48 kHz)" : "SEM saida de audio (InitAudioDevice falhou?)");
    setReturnS32(ctx, 1);
}
void stopSoundSystem(uint8_t *, R5900Context *, PS2Runtime *) { Engine::get().stopAll(); }
void ret0(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnS32(ctx, 0); }   // FlushSoundCommands, ResolveBankXREFS...
void getTick(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnU32(ctx, Engine::get().tickCount()); }
void bankLoadByLoc(uint8_t *, R5900Context *ctx, PS2Runtime *)
{
    const uint32_t h = Engine::get().loadBank(arg(ctx, 0), arg(ctx, 1));
    setReturnU32(ctx, h);
}
void bankLoadByLocCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t h = Engine::get().loadBank(arg(ctx, 0), arg(ctx, 1));
    callGuest(rdram, ctx, rt, arg(ctx, 2), h, arg(ctx, 3));
}
void unloadBank(uint8_t *, R5900Context *ctx, PS2Runtime *) { SND_LOG("UnloadBank %#x\n", arg(ctx, 0)); Engine::get().unloadBank(arg(ctx, 0)); }
void unloadBankCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    SND_LOG("UnloadBank_CB %#x\n", arg(ctx, 0));
    Engine::get().unloadBank(arg(ctx, 0));
    callGuest(rdram, ctx, rt, arg(ctx, 1), 0, arg(ctx, 2));
}
void sramFree(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnS32(ctx, Engine::get().sramFree()); }

// --- volume mestre e modos
void setMasterVolume(uint8_t *, R5900Context *ctx, PS2Runtime *)
{
    SND_LOG("SetMasterVolume grupo %d = %#x\n", sarg(ctx, 0), sarg(ctx, 1));
    Engine::get().setMasterVolume(sarg(ctx, 0), sarg(ctx, 1));
}
void getMasterVolume(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnS32(ctx, Engine::get().masterVolume(sarg(ctx, 0))); }
void setPlaybackMode(uint8_t *, R5900Context *ctx, PS2Runtime *) { Engine::get().setMono(sarg(ctx, 0) == 1); }

// --- efeitos: snd_PlaySoundVolPanPMPB(banco, som, vol, pan, pitchMod, pitchBend) [, cb, user]
inline uint32_t playFromArgs(R5900Context *ctx)
{
    const uint32_t h = Engine::get().play(arg(ctx, 0), sarg(ctx, 1), sarg(ctx, 2), sarg(ctx, 3), sarg(ctx, 4), sarg(ctx, 5));
    SND_LOG("Play banco %#x som %d vol %#x pan %d pm %d pb %d -> %u\n", arg(ctx, 0), sarg(ctx, 1), sarg(ctx, 2), sarg(ctx, 3),
            sarg(ctx, 4), sarg(ctx, 5), h);
    return h;
}
void playSound(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnU32(ctx, playFromArgs(ctx)); }
void playSoundCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t cb = arg(ctx, 6), user = arg(ctx, 7);
    const uint32_t h = playFromArgs(ctx);
    callGuest(rdram, ctx, rt, cb, h, user);
}
void stopSound(uint8_t *, R5900Context *ctx, PS2Runtime *) { SND_LOG("Stop %u\n", arg(ctx, 0)); Engine::get().stop(arg(ctx, 0)); }
void pauseSound(uint8_t *, R5900Context *ctx, PS2Runtime *) { Engine::get().pause(arg(ctx, 0), true); }
void continueSound(uint8_t *, R5900Context *ctx, PS2Runtime *) { Engine::get().pause(arg(ctx, 0), false); }
void stopAllSounds(uint8_t *, R5900Context *, PS2Runtime *) { SND_LOG("StopAllSounds\n"); Engine::get().stopAll(); }
void pauseGroups(uint8_t *, R5900Context *ctx, PS2Runtime *) { SND_LOG("PauseAllSoundsInGroup %#x\n", arg(ctx, 0)); Engine::get().pauseGroups(arg(ctx, 0), true); }
void continueGroups(uint8_t *, R5900Context *ctx, PS2Runtime *) { SND_LOG("ContinueAllSoundsInGroup %#x\n", arg(ctx, 0)); Engine::get().pauseGroups(arg(ctx, 0), false); }
// "ainda tocando": devolve o proprio handle, ou 0 quando acabou
void isPlaying(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnU32(ctx, Engine::get().playing(arg(ctx, 0)) ? arg(ctx, 0) : 0u); }
void isPlayingCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t h = arg(ctx, 0);
    callGuest(rdram, ctx, rt, arg(ctx, 1), Engine::get().playing(h) ? h : 0u, arg(ctx, 2));
}
void setVolPan(uint8_t *, R5900Context *ctx, PS2Runtime *) { Engine::get().setVolPan(arg(ctx, 0), sarg(ctx, 1), sarg(ctx, 2)); }
void setPitchBend(uint8_t *, R5900Context *ctx, PS2Runtime *) { Engine::get().setPBend(arg(ctx, 0), sarg(ctx, 1)); }
void setPitchMod(uint8_t *, R5900Context *ctx, PS2Runtime *) { Engine::get().setPMod(arg(ctx, 0), sarg(ctx, 1)); }
// snd_SetSoundParams(handle, mascara, vol, pan, pitchMod, pitchBend) [, cb, user]
void setParams(uint8_t *, R5900Context *ctx, PS2Runtime *)
{
    Engine::get().setParams(arg(ctx, 0), arg(ctx, 1), sarg(ctx, 2), sarg(ctx, 3), sarg(ctx, 4), sarg(ctx, 5));
}
void setParamsCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    setParams(rdram, ctx, rt);
    callGuest(rdram, ctx, rt, arg(ctx, 6), 0, arg(ctx, 7));
}

// --- streams .VPK/.VAG: snd_PlayVAGStreamByLoc(lsn, lsn2, off1, off2, vol, pan, grupo, fila, [pilha] flags) [, cb, user na pilha]
inline uint32_t playStreamFromArgs(uint8_t *rdram, R5900Context *ctx)
{
    const uint32_t h = Engine::get().playStream(arg(ctx, 0), sarg(ctx, 4), sarg(ctx, 5), sarg(ctx, 6));
    if (arg(ctx, 1) || arg(ctx, 2) || arg(ctx, 3) || arg(ctx, 7))
        std::fprintf(stderr, "[snd] PlayVAGStream com parametros nao tratados: lsn2 %u off %u/%u fila %u\n", arg(ctx, 1), arg(ctx, 2),
                     arg(ctx, 3), arg(ctx, 7));
    SND_LOG("PlayVAGStream lsn %u vol %#x pan %d grupo %d flags %#x -> %u\n", arg(ctx, 0), sarg(ctx, 4), sarg(ctx, 5), sarg(ctx, 6),
            stackArg(rdram, ctx, 0), h);
    return h;
}
void playStream(uint8_t *rdram, R5900Context *ctx, PS2Runtime *) { setReturnU32(ctx, playStreamFromArgs(rdram, ctx)); }
void playStreamCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t cb = stackArg(rdram, ctx, 8), user = stackArg(rdram, ctx, 0x10);
    const uint32_t h = playStreamFromArgs(rdram, ctx);
    callGuest(rdram, ctx, rt, cb, h, user);
}
void stopAllStreams(uint8_t *, R5900Context *, PS2Runtime *) { SND_LOG("StopAllStreams\n"); Engine::get().stopAllStreams(); }
// Inicializacao do streaming: nao liga gStreamingInited da lib, entao snd_StreamSafeCd* continuam indo direto ao sceCdRead
void initVagStreaming(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnS32(ctx, 1); }
void isStreamBufferedCB(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt) { callGuest(rdram, ctx, rt, arg(ctx, 1), 1, arg(ctx, 2)); }
void ret1(uint8_t *, R5900Context *ctx, PS2Runtime *) { setReturnS32(ctx, 1); }

// Funil de todos os comandos ao IOP: so chega aqui o que nenhum hook acima cobriu
void unhandledCommand(uint8_t *, R5900Context *ctx, PS2Runtime *)
{
    static uint64_t seen[2] = {};
    const uint32_t cmd = arg(ctx, 0);
    if (cmd < 128 && !(seen[cmd >> 6] & (1ull << (cmd & 63))))
    {
        seen[cmd >> 6] |= 1ull << (cmd & 63);
        std::fprintf(stderr, "[snd] comando 0x%02x do 989snd sem hook (ignorado)\n", cmd);
    }
    setReturnS32(ctx, 0);
}

// sceSifLoadModule: com o som nativo, os IRX de som nao sobem para o IOP emulado
void loadModule(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const char *path = reinterpret_cast<const char *>(rdram + (arg(ctx, 0) & PS2_RAM_MASK));
    std::string p(path, strnlen(path, 128));
    for (char &c : p) c = char(std::toupper(static_cast<unsigned char>(c)));
    if (p.find("989SND") != std::string::npos || p.find("LIBSD") != std::string::npos || p.find("989ERR") != std::string::npos)
    {
        std::fprintf(stderr, "[snd] %s nao carregado (som nativo)\n", path);
        setReturnS32(ctx, 0x100);   // id de modulo positivo = sucesso
        return;
    }
    g_origLoadModule(rdram, ctx, rt);
}
#undef SND_LOG
} // namespace wotm_snd_hooks

void wotmSndInstall(PS2Runtime &runtime)
{
    using namespace wotm_snd_hooks;
    g_log = std::getenv("PS2X_SND_LOG") != nullptr;
    Engine::get().setReader(&readLsn);
    struct H { uint32_t addr; PS2Runtime::RecompiledFunction fn; };
    // enderecos de config/status.csv (lib989snd/snd)
    const H hooks[] = {
        {0x1eaa38, hostLeaf<startSoundSystem>}, {0x1eac78, hostLeaf<ret0> /* FlushSoundCommands */}, {0x1eaea8, hostLeaf<stopSoundSystem>}, {0x1eaed8, hostLeaf<getTick>},
        {0x1eb1d8, hostLeaf<bankLoadByLoc>}, {0x1eb310, hostLeaf<bankLoadByLocCB>}, {0x1eb8b8, hostLeaf<ret0> /* ResolveBankXREFS */},
        {0x1eb8e8, hostLeaf<unloadBank>}, {0x1eb918, hostLeaf<unloadBankCB>},
        {0x1eb9a8, hostLeaf<setMasterVolume>}, {0x1eb9e0, hostLeaf<getMasterVolume>}, {0x1eba38, hostLeaf<setPlaybackMode>}, {0x1ebb28, hostLeaf<ret0> /* SetGroupVoiceRange */},
        {0x1ebb68, hostLeaf<playSound>}, {0x1ebbe8, hostLeaf<playSound> /* _A */}, {0x1ebc80, hostLeaf<playSoundCB>},
        {0x1ebda0, hostLeaf<stopSound>}, {0x1ebe58, hostLeaf<pauseSound>}, {0x1ebe88, hostLeaf<continueSound>}, {0x1ebeb8, hostLeaf<stopAllSounds>},
        {0x1ebee8, hostLeaf<pauseGroups>}, {0x1ebf18, hostLeaf<continueGroups>}, {0x1ebf48, hostLeaf<isPlaying>}, {0x1ebf70, hostLeaf<isPlayingCB>},
        {0x1ec008, hostLeaf<setVolPan>}, {0x1ec140, hostLeaf<setPitchBend>}, {0x1ec178, hostLeaf<setPitchMod>},
        {0x1ec1b0, hostLeaf<setParams>}, {0x1ec1f0, hostLeaf<setParams> /* _A */}, {0x1ec210, hostLeaf<setParamsCB>},
        {0x1ec4e0, hostLeaf<unhandledCommand> /* SendIOPCommandAndWait */}, {0x1ec680, hostLeaf<unhandledCommand> /* SendIOPCommandNoWait */},
        {0x1ecb08, hostLeaf<initVagStreaming>}, {0x1ecbc8, hostLeaf<stopSound> /* StopVAGStream */}, {0x1ecbf8, hostLeaf<stopAllStreams>},
        {0x1ecc28, hostLeaf<stopAllStreams> /* CloseVAGStreaming */}, {0x1ecca0, hostLeaf<playStream>}, {0x1ecd00, hostLeaf<playStream> /* _A */},
        {0x1ecd28, hostLeaf<playStreamCB>}, {0x1ecd90, hostLeaf<pauseSound> /* PauseVAGStream */}, {0x1ecdc0, hostLeaf<continueSound> /* ContinueVAGStream */},
        {0x1ecf50, hostLeaf<ret1> /* IsVAGStreamBuffered */}, {0x1ecf78, hostLeaf<isStreamBufferedCB>},
        {0x1ed228, hostLeaf<ret0> /* SetReverbType */}, {0x1ed260, hostLeaf<ret0> /* SetReverbDepth */}, {0x1ed2a0, hostLeaf<ret0> /* SetReverbEx */},
        {0x1ed4a0, hostLeaf<sramFree>},
        {0x1ed550, hostLeaf<ret0> /* InitMovieSound (os filmes tem audio proprio em wotm_fmv.hpp) */}, {0x1ed588, hostLeaf<ret0>}, {0x1ed5b0, hostLeaf<ret0>},
        {0x1ed5d8, hostLeaf<ret0>}, {0x1ed600, hostLeaf<ret0>}, {0x1ed638, hostLeaf<ret0> /* GetTransStatus */},
    };
    for (const H &h : hooks)
        runtime.registerFunction(h.addr, h.fn);
    g_origLoadModule = runtime.lookupFunction(0x24deb0);   // sceSifLoadModule (stub do runtime)
    if (g_origLoadModule)
        runtime.registerFunction(0x24deb0, hostLeaf<loadModule>);
    std::fprintf(stderr, "[snd] som nativo: %zu funcoes do 989snd trocadas\n", sizeof hooks / sizeof hooks[0]);
}
#else
void wotmSndInstall(PS2Runtime &)
{
    std::fprintf(stderr, "[snd] PS2X_SND ignorado: falta native/wotm_snd.hpp (WOTM_NATIVE_DIR)\n");
}
#endif
