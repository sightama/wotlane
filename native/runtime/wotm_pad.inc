// wotm_pad.inc: que controles e o teclado alimentam cada porta do jogo (porta 0 = jogador 1, porta 1 = jogador 2).
// Incluido por wotm_live.inc (escopo global de ps2_runtime.cpp). O runtime (ps2_pad.cpp) consulta g_ps2PadDeviceMap a cada leitura.
// Os controles conectados entram na ordem do raylib (o primeiro conectado e o jogador 1, o segundo o jogador 2), sem buracos:
// um controle desplugado passa a vez ao seguinte.
//   Padrao: o teclado e o mesmo jogador do primeiro controle (jogador 1).
//   WOTM_KBSEPARATE=1: o teclado e um dispositivo a parte, depois dos controles (sem controle, jogador 1; com um, jogador 2;
//   com dois, sobra).
//   Escolha de lados do 2 jogadores (wotm_padsel.inc): enquanto g_manual, a porta 0 e a 1 recebem so os dispositivos que os
//   jogadores puseram de cada lado (g_manualDevs, mesmos bits do mapa); os que ficaram no meio nao jogam.
namespace wotm_pad
{
static std::atomic<bool> g_manual{false};
static std::atomic<uint32_t> g_manualDevs[2] = {0u, 0u};

static uint32_t deviceMap(int port)
{
    static const bool kbSeparate = [] { const char *e = std::getenv("WOTM_KBSEPARATE"); return e && e[0] == '1'; }();
    int pads[PS2_PAD_MAX_GAMEPADS];
    int n = 0;
    for (int i = 0; i < PS2_PAD_MAX_GAMEPADS; ++i)
        if (IsGamepadAvailable(i))
            pads[n++] = i;

    uint32_t devs = 0;
    if (g_manual.load(std::memory_order_relaxed))
        devs = port >= 0 && port < 2 ? g_manualDevs[port].load(std::memory_order_relaxed) : 0u;
    else
    {
        if (port >= 0 && port < n)
            devs = 1u << pads[port];
        if (kbSeparate ? port == n : port == 0)
            devs |= PS2_PAD_DEV_KEYBOARD;
    }

    // uma linha por porta quando o que a alimenta muda (inicio, controle plugado ou desplugado)
    static uint32_t last[2] = {~0u, ~0u};
    if (port >= 0 && port < 2 && devs != last[port])
    {
        last[port] = devs;
        std::string what;
        for (int i = 0; i < PS2_PAD_MAX_GAMEPADS; ++i)
            if (devs & (1u << i))
                what += "controle " + std::to_string(i) + " (" + GetGamepadName(i) + ")";
        if (devs & PS2_PAD_DEV_KEYBOARD)
            what += what.empty() ? "teclado" : " + teclado";
        std::fprintf(stderr, "[pad] jogador %d: %s\n", port + 1, what.empty() ? "nenhum dispositivo" : what.c_str());
    }
    return devs;
}

static const bool s_installed = (g_ps2PadDeviceMap = &deviceMap, true);
}
