// wotm_pad.inc: que controles e o teclado alimentam cada porta do jogo (porta 0 = jogador 1, porta 1 = jogador 2).
// Incluido por wotm_live.inc (escopo global de ps2_runtime.cpp). O runtime (ps2_pad.cpp) consulta g_ps2PadDeviceMap a cada leitura.
// Os controles conectados entram na ordem do raylib (o primeiro conectado e o jogador 1, o segundo o jogador 2), sem buracos:
// um controle desplugado passa a vez ao seguinte.
//   Padrao: o teclado e o mesmo jogador do primeiro controle (jogador 1).
//   WOTM_KBSEPARATE=1: o teclado e um dispositivo a parte, depois dos controles (sem controle, jogador 1; com um, jogador 2;
//   com dois, sobra).
//   Escolha de controles do multijogador (wotm_padsel.inc): enquanto g_manual, o jogador n recebe so os dispositivos que entraram
//   na coluna dele (g_manualDevs[n], mesmos bits do mapa); os que ficaram de fora nao jogam.
//   3 e 4 jogadores (WOTM_PLAYERS, wotm_split4.inc): o scePadRead do slot 1 de uma porta (jogadores 3 e 4) chega aqui com a porta
//   do slot 0 e g_portBase 2; o jogador e porta + base, com o controle de mesma ordem.
// Teclas e botoes de cada botao do PS2: o mapa do wotm_settings.hpp (aba CONTROLS do painel), instalado no ps2_pad.cpp do fork
// (g_ps2PadKeyboard, g_ps2PadGamepadButtons); os eixos dos gamepads seguem fixos.
namespace wotm_pad
{
static std::atomic<bool> g_manual{false};
static std::atomic<uint32_t> g_manualDevs[4] = {};
static std::atomic<int> g_portBase{0};

static uint32_t deviceMap(int port)
{
    const bool kbSeparate = wotm::settings::on(wotm::settings::KbSeparate);   // opcao ao vivo (wotm_settings.hpp)
    port += g_portBase.load(std::memory_order_relaxed);
    int pads[PS2_PAD_MAX_GAMEPADS];
    int n = 0;
    for (int i = 0; i < PS2_PAD_MAX_GAMEPADS; ++i)
        if (IsGamepadAvailable(i))
            pads[n++] = i;

    uint32_t devs = 0;
    if (g_manual.load(std::memory_order_relaxed))
        devs = port >= 0 && port < 4 ? g_manualDevs[port].load(std::memory_order_relaxed) : 0u;
    else
    {
        if (port >= 0 && port < n)
            devs = 1u << pads[port];
        if (kbSeparate ? port == n : port == 0)
            devs |= PS2_PAD_DEV_KEYBOARD;
    }

    // uma linha por jogador quando o que o alimenta muda (inicio, controle plugado ou desplugado)
    static uint32_t last[4] = {~0u, ~0u, ~0u, ~0u};
    if (port >= 0 && port < 4 && devs != last[port])
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

// ------------------------------------------------------------------ mapa de teclas e botoes (wotm_settings.hpp, aba CONTROLS)
namespace st = wotm::settings;
inline bool kbDown(st::Bind b)
{
    const uint32_t v = st::kb(b);
    return ((v & 0xFFFF) && IsKeyDown(int(v & 0xFFFF))) || ((v >> 16) && IsKeyDown(int(v >> 16)));
}
// Jogador (0..3) que o gamepad `pad` alimenta, pela regra do deviceMap: na escolha manual, a coluna em que ele entrou; senao, a ordem
// dos conectados. Fora disso (ou sem gamepad), o jogador 1. Escolhe o mapa de botoes dele (wotm_settings.hpp, um por jogador).
inline int playerOfPad(int pad)
{
    if (pad < 0 || pad >= PS2_PAD_MAX_GAMEPADS)
        return 0;
    if (g_manual.load(std::memory_order_relaxed))
    {
        for (int p = 0; p < 4; ++p)
            if (g_manualDevs[p].load(std::memory_order_relaxed) & (1u << pad)) return std::min(p, st::kPadPlayers - 1);
        return 0;
    }
    int n = 0;
    for (int i = 0; i < PS2_PAD_MAX_GAMEPADS; ++i)
        if (IsGamepadAvailable(i))
        {
            if (i == pad) return std::min(n, st::kPadPlayers - 1);
            ++n;
        }
    return 0;
}
inline bool padDown(int pad, st::Bind b, int player)
{
    const int btn = st::pad(b, player);
    return btn > 0 && IsGamepadButtonDown(pad, btn);
}

static void keyboard(uint16_t &btns, uint8_t *data)
{
    for (int b = 0; b < st::kBinds; ++b)
        if (st::kBindMask[b] && kbDown(st::Bind(b)))
            btns &= static_cast<uint16_t>(~st::kBindMask[b]);
    // analogico esquerdo (so com alguma das teclas dele apertada; senao fica o valor do gamepad ou o centro)
    const bool l = kbDown(st::BStickLeft), r = kbDown(st::BStickRight), u = kbDown(st::BStickUp), d = kbDown(st::BStickDown);
    if (l != r) data[6] = l ? 0x00 : 0xFF;
    if (u != d) data[7] = u ? 0x00 : 0xFF;
}
// Nos menus: o gancho do screenGetInput (wotm_mainmenu.inc) marca a hora a cada quadro de uma tela do menu
static std::atomic<double> g_menuTime{-1.0};
inline void markMenuFrame() { g_menuTime.store(GetTime(), std::memory_order_relaxed); }
inline bool inMenus() { return GetTime() - g_menuTime.load(std::memory_order_relaxed) < 0.25; }
// MenuConfirm = 1 nos menus (gamepads): o botao do X passa a dar triangulo (voltar, no jogo), o da bola da X (confirmar) e o do
// triangulo da bola. Com o mapa padrao: o da direita confirma e o de baixo volta, como nos consoles da Nintendo.
inline bool menuSwap() { return st::get(st::MenuConfirm) == 1 && inMenus(); }
// Botao do PS2 que o botao fisico do botao b do mapa produz agora (a troca dos menus)
inline st::Bind effectiveBind(st::Bind b)
{
    if (!menuSwap()) return b;
    return b == st::BCross ? st::BTriangle : b == st::BCircle ? st::BCross : b == st::BTriangle ? st::BCircle : b;
}
// O inverso: qual botao do mapa da o botao do PS2 b agora (os icones mostram esse)
inline st::Bind sourceBind(st::Bind b)
{
    if (!menuSwap()) return b;
    return b == st::BTriangle ? st::BCross : b == st::BCross ? st::BCircle : b == st::BCircle ? st::BTriangle : b;
}
static uint16_t gamepadButtons(int pad)
{
    uint16_t m = 0;
    const int player = playerOfPad(pad);
    for (int b = 0; b < st::kBinds; ++b)
        if (st::bindOnPad(st::Bind(b)) && padDown(pad, st::Bind(b), player))
            m |= st::kBindMask[effectiveBind(st::Bind(b))];
    return m;
}

// Jogador (0..3) que o teclado alimenta, pela regra do deviceMap (-1: nenhum, na escolha manual sem o teclado ou com ele sobrando)
inline int keyboardPlayer()
{
    if (g_manual.load(std::memory_order_relaxed))
    {
        for (int p = 0; p < 4; ++p)
            if (g_manualDevs[p].load(std::memory_order_relaxed) & PS2_PAD_DEV_KEYBOARD) return p;
        return -1;
    }
    if (!st::on(st::KbSeparate)) return 0;
    int n = 0;
    for (int i = 0; i < PS2_PAD_MAX_GAMEPADS; ++i)
        if (IsGamepadAvailable(i)) ++n;
    return n < 4 ? n : -1;
}

// Mouse na camera moderna (wotm_mouse.hpp), a cada apresentacao na thread da janela: cursor preso enquanto a orbita do jogador do
// teclado dirige (aviso da thread do EE) e a janela tem o foco; preso, o deslocamento vai para a orbita. WOTM_MOUSECAM=0 desliga.
static void mouseFrame()
{
    namespace ms = wotm::mouse;
    static const bool off = [] { const char *e = std::getenv("WOTM_MOUSECAM"); return e && e[0] == '0'; }();
    ms::g_kbPlayer.store(keyboardPlayer(), std::memory_order_relaxed);
    const bool want = !off && ms::wanted() && IsWindowFocused();
    static bool captured = false;
    static int skip = 0;
    if (want != captured)
    {
        captured = want;
        if (want)
        {
            DisableCursor();
            skip = 2;   // o DisableCursor recentra o cursor: o primeiro deslocamento e esse salto
        }
        else
            EnableCursor();
        ms::clear();
        ms::g_captured.store(want, std::memory_order_relaxed);
        std::fprintf(stderr, "[mouse] cursor %s (aviso ha %lld ms, foco %d)\n", want ? "preso (camera)" : "solto",
                     static_cast<long long>(ms::nowMs() - (ms::g_wantUntil.load(std::memory_order_relaxed) - 150)), IsWindowFocused() ? 1 : 0);
    }
    if (!captured)
        return;
    const Vector2 d = GetMouseDelta();
    if (skip > 0)
    {
        --skip;
        return;
    }
    if (d.x != 0.f || d.y != 0.f)
        ms::add(d.x, d.y);
}

static const bool s_installed = (g_ps2PadDeviceMap = &deviceMap, g_ps2PadKeyboard = &keyboard, g_ps2PadGamepadButtons = &gamepadButtons, true);
}

void wotmPadSetPortBase(int base) { wotm_pad::g_portBase.store(base, std::memory_order_relaxed); }
