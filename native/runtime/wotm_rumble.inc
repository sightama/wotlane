// wotm_rumble.inc: vibracao dos gamepads (opcao VIBRATION da aba CONTROLS do "Recompiled options", WOTM_VIBRATION, ligada por
// padrao). Incluido por wotm_live.inc antes do wotm_mainmenu.inc (que chama o install).
//  - O jogo manda os motores pelo scePadSetActDirect(porta, slot, dados[6]), com o alinhamento que o inputUpdateState pede: dados[0]
//    e o motor pequeno (0/1), dados[1] o grande (0..255). No runtime ele e um stub vazio; o gancho manda os dois motores ao controle do
//    jogador por XInput (o GLFW do raylib nao tem vibracao no Windows).
//  - A chave de vibracao do jogo por pad (inputUseActuator, que o VIBRATION ON/OFF da pausa e da tela de opcoes do retail mudavam; o
//    port tirou as duas linhas) fica sempre ligada: quem decide e a opcao. Desligar a opcao para os motores na hora.
//  - Jogador: porta + 2 no slot 1 (jogadores 3 e 4, como no scePadRead; wotm_pad.inc). Controles dele: os do wotm_pad::devicesOf.
//  - Controle do raylib -> indice do XInput: entre os gamepads do raylib com nome de XInput (o GLFW os chama "Xbox Controller",
//    "Wireless Xbox Controller" ou "XInput ..."), o k-esimo e o k-esimo indice do XInput conectado (o GLFW os abre nessa ordem).
//    Controle que o Windows le por DirectInput (DualShock/DualSense sem o Steam Input) nao vibra.
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
// Sem windows.h (briga com os nomes do raylib): so o que carrega o XInput
extern "C" __declspec(dllimport) void *__stdcall LoadLibraryA(const char *name);
extern "C" __declspec(dllimport) void *__stdcall GetProcAddress(void *module, const char *name);
#endif

namespace wotm_rumble
{
struct XState { uint32_t packet; uint16_t buttons; uint8_t lt, rt; int16_t lx, ly, rx, ry; };   // XINPUT_STATE
struct XVib { uint16_t left, right; };                                                            // XINPUT_VIBRATION
#ifdef _WIN32
using GetStateFn = unsigned long(__stdcall *)(unsigned long, XState *);
using SetStateFn = unsigned long(__stdcall *)(unsigned long, XVib *);
#else
using GetStateFn = unsigned long (*)(unsigned long, XState *);
using SetStateFn = unsigned long (*)(unsigned long, XVib *);
#endif
static GetStateFn g_get = nullptr;
static SetStateFn g_set = nullptr;
static PS2Runtime::RecompiledFunction g_origUse = nullptr, g_origAct = nullptr;
static uint32_t g_last[4] = {~0u, ~0u, ~0u, ~0u};   // ultimo (esquerdo << 16 | direito) mandado a cada indice do XInput
static uint8_t g_want[4][2] = {};                   // o que o jogo pediu por jogador (motor pequeno, grande)
static bool g_wasOn = true;
static std::atomic<unsigned> g_pulseGen{0};   // pulso de aviso em curso (o mais novo vale)

static bool loadXInput()
{
#ifdef _WIN32
    for (const char *dll : {"xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll"})
        if (void *m = LoadLibraryA(dll))
        {
            g_get = reinterpret_cast<GetStateFn>(GetProcAddress(m, "XInputGetState"));
            g_set = reinterpret_cast<SetStateFn>(GetProcAddress(m, "XInputSetState"));
            if (g_get && g_set) return true;
        }
#endif
    g_get = nullptr;
    g_set = nullptr;
    return false;
}

static bool isXInputName(const char *n)
{
    return n && (!std::strncmp(n, "Xbox", 4) || !std::strncmp(n, "Wireless Xbox", 13) || !std::strncmp(n, "XInput", 6));
}

// Indice do XInput do gamepad `pad` do raylib (-1: nao e XInput ou nao achado)
static int xinputIndexOf(int pad)
{
    int k = -1, seen = 0;
    for (int i = 0; i < PS2_PAD_MAX_GAMEPADS; ++i)
        if (IsGamepadAvailable(i) && isXInputName(GetGamepadName(i)))
        {
            if (i == pad) { k = seen; break; }
            ++seen;
        }
    if (k < 0) return -1;
    int n = 0;
    for (unsigned long x = 0; x < 4; ++x)
    {
        XState s{};
        if (g_get(x, &s) != 0) continue;   // ERROR_SUCCESS = 0
        if (n == k) return int(x);
        ++n;
    }
    return -1;
}

static void send(int x, uint16_t left, uint16_t right)
{
    const uint32_t v = uint32_t(left) << 16 | right;
    if (x < 0 || x > 3 || g_last[x] == v) return;
    g_last[x] = v;
    XVib vib{left, right};
    g_set(static_cast<unsigned long>(x), &vib);
}

// Manda a cada controle o pedido do jogador que ele alimenta (o mais forte, se o controle alimentar mais de um)
static void apply()
{
    const bool on = wotm::settings::on(wotm::settings::Vibration);
    uint16_t left[4] = {}, right[4] = {};
    if (on)
        for (int p = 0; p < 4; ++p)
        {
            if (!g_want[p][0] && !g_want[p][1]) continue;
            const uint32_t devs = wotm_pad::devicesOf(p);
            for (int pad = 0; pad < PS2_PAD_MAX_GAMEPADS; ++pad)
                if (devs & (1u << pad))
                    if (const int x = xinputIndexOf(pad); x >= 0)
                    {
                        left[x] = std::max<uint16_t>(left[x], uint16_t(g_want[p][1] * 257u));
                        right[x] = std::max<uint16_t>(right[x], g_want[p][0] ? 0xFFFF : 0);
                    }
        }
    for (int x = 0; x < 4; ++x) send(x, left[x], right[x]);
}

// scePadSetActDirect(porta, slot, dados): o stub do runtime devolvia 1 sem fazer nada
static void actHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::scePadSetActDirect)
    {
        g_origAct(rdram, ctx, runtime);
        return;
    }
    const uint32_t port = GPR_U32(ctx, 4), slot = GPR_U32(ctx, 5), data = GPR_U32(ctx, 6) & 0x1FFFFFFu;
    const int player = int(port) + (slot ? 2 : 0);
    if (player >= 0 && player < 4 && data + 2 <= PS2_RAM_SIZE)
    {
        const bool was = g_want[player][0] || g_want[player][1];
        g_want[player][0] = rdram[data] ? 1 : 0;
        g_want[player][1] = rdram[data + 1];
        static int s_logs = 0;   // as primeiras no log, para conferir sem o controle na mao
        if (!was && (g_want[player][0] || g_want[player][1]) && s_logs < 3)
        {
            ++s_logs;
            std::fprintf(stderr, "[rumble] jogador %d: motor pequeno %d, grande %d (opcao %s)\n", player + 1, g_want[player][0],
                         g_want[player][1], wotm::settings::on(wotm::settings::Vibration) ? "ligada" : "desligada");
        }
        apply();
    }
    ctx->r[2] = _mm_set_epi64x(0, 1);
    ctx->pc = GPR_U32(ctx, 31);
}

// inputUseActuator(pad, usar): sempre ligado (a opcao decide no actHook)
static void useHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::inputUseActuator__Fib) ctx->r[5] = _mm_set_epi64x(0, 1);
    g_origUse(rdram, ctx, runtime);
}

// Ligar a opcao no painel: um tremor curto em todos os controles XInput conectados, para o jogador sentir que ligou. Depois de
// 0,25 s volta ao que o jogo pede (apply). O pulso passa por cima do cache do send.
static void pulse()
{
    const unsigned gen = ++g_pulseGen;
    for (unsigned long x = 0; x < 4; ++x)
    {
        XState st{};
        if (g_get(x, &st) != 0) continue;
        XVib vib{0x6000, 0x6000};
        g_set(x, &vib);
        g_last[x] = ~0u;
    }
    std::thread([gen] {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (g_pulseGen.load() != gen) return;
        for (int x = 0; x < 4; ++x) g_last[x] = ~0u;
        apply();
    }).detach();
}

// Depois de mudar a opcao no painel (wotm_mainmenu.inc): desligar para os motores na hora; ligar da o pulso de aviso
static void poll()
{
    if (!g_set) return;
    const bool on = wotm::settings::on(wotm::settings::Vibration);
    if (on != g_wasOn)
    {
        g_wasOn = on;
        if (on)
            pulse();
        else
            apply();
    }
}

static void stopAll()
{
    if (!g_set) return;
    for (int x = 0; x < 4; ++x)
    {
        XVib vib{0, 0};
        g_set(static_cast<unsigned long>(x), &vib);
    }
}

static void install(PS2Runtime &runtime)
{
    if (!loadXInput())
    {
        std::fprintf(stderr, "[rumble] XInput indisponivel: sem vibracao\n");
        return;
    }
    g_wasOn = wotm::settings::on(wotm::settings::Vibration);
    if ((g_origAct = runtime.lookupFunction(wotm::fn::scePadSetActDirect)))
        runtime.registerFunction(wotm::fn::scePadSetActDirect, &actHook);
    if ((g_origUse = runtime.lookupFunction(wotm::fn::inputUseActuator__Fib)))
        runtime.registerFunction(wotm::fn::inputUseActuator__Fib, &useHook);
    std::atexit(stopAll);   // controle nao fica vibrando depois de fechar o jogo
    std::fprintf(stderr, "[rumble] vibracao por XInput (%s; VIBRATION na aba CONTROLS, WOTM_VIBRATION)\n", g_wasOn ? "ligada" : "desligada");
}
} // namespace wotm_rumble
