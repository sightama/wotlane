// wotm_mainmenu.inc: menus de itens do telao em texto (incluido por wotm_live.inc, depois do wotm_padsel.inc, de quem usa o desenho
// no telao e a fonte do jogo). Telas:
//   1 menu principal: 1 player, multiplayer (o "2 player"), options, recompiled options (nosso), load game, unlocks;
//   2 modos de 1 jogador: adventure, free-for-all, endurance;
//   5 modos do multiplayer (depois da escolha de controles do wotm_padsel.inc): free-for-all, elimination, minigames.
//
// Os itens do jogo (shell->m_menuItemSw[tela][1..n]) sao quadros com a palavra em imagem. Aqui todos os objetos dos switches ficam com
// alfa 0 a cada quadro da tela (e no fade de saida dela) e a lista e desenhada com a fonte do jogo (a mesma da lista de saves) numa
// textura propria, posta no plano dos itens, do topo do primeiro ao pe do ultimo.
// O cursor continua sendo o currentSelection[tela] do jogo: o gancho do screenGetInput consome cima/baixo e move o currentSelection;
// o X e o triangulo seguem para a funcao da tela, que faz o que o item faz, como no retail. Na tela 1 a linha "recompiled options"
// fica fora do currentSelection (g_onOptions); o X nela abre o painel de opcoes do port no lugar da lista, sem trocar de tela, e o
// triangulo (ou O) fecha. Com o painel aberto (e 20 quadros depois de fechar) o jogo nao ve entrada nenhuma: o painel le os
// dispositivos direto (wotm_padsel::readDevice), na thread do EE, dentro do gancho do screenGetInput.
// Painel: abas VIDEO / FEATURES / CONTROLS (L1/R1, Q/E ou Tab; Shift+Tab volta), cima/baixo escolhem a linha, esquerda/direita/X
// trocam o valor, que vale na hora e e gravado no settings.ini (wotm_settings.hpp; o video e aplicado na thread da janela pelo
// applyVideoSettings do wotm_live.inc). Desenhado numa textura propria no telao inteiro (a faixa visivel abaixo da tarja do menu).
// Logo do jogo no telao (os dois objetos irmaos do m_movieScreen): alfa 1 so na tela de "press start" (20), 0 nas outras, seguindo o
// targetAlpha nos fades de/para a 20 (logoFrame, a cada quadro antes da coleta da cena). Sem ele a lista fica centrada no telao.
// Tela 5: enquanto a escolha de controles esta no telao a lista fica escondida, e aparece junto com o fade que a escolha da aos itens.
// WOTM_TEXTMENU=0: itens do retail (e o rotulo "multiplayer" do wotm_padsel.inc).
namespace wotm_mainmenu
{
using wotm_padsel::rd32;
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr int kMaxRows = 6;
constexpr int kOptionsRow = -1;   // item que nao e do jogo

struct Row { int item; const char *label; };
struct ScreenDef
{
    int screen;
    uint32_t numSel;   // numSelectables da tela (os itens 1..n do jogo); 0 = todos
    int rows;
    Row row[kMaxRows];
};
constexpr ScreenDef kScreens[] = {
    {1, wotm::sym::numSelectables_2784, 6,
     {{1, "1 PLAYER"}, {2, "MULTIPLAYER"}, {3, "OPTIONS"}, {kOptionsRow, "RECOMPILED OPTIONS"}, {4, "LOAD GAME"}, {5, "UNLOCKS"}}},
    {2, 0, 3, {{1, "ADVENTURE"}, {2, "FREE-FOR-ALL"}, {3, "ENDURANCE"}}},
    {5, wotm::sym::numSelectables_2857, 3, {{1, "FREE-FOR-ALL"}, {2, "ELIMINATION"}, {3, "MINIGAMES"}}},
};
static const ScreenDef *screenDef(uint32_t s)
{
    for (const ScreenDef &d : kScreens)
        if (uint32_t(d.screen) == s) return &d;
    return nullptr;
}

struct View
{
    bool on = false;
    int screen = 0;
    float alpha = 0.f;     // da tela (fades de entrada e saida)
    float panel = 0.f;     // 0 lista, 1 painel aberto
    int sel = 0;
    int rows = 0;
    Row row[kMaxRows] = {};
    uint32_t itemObj[kMaxRows][4] = {};   // objetos do filho em uso de cada item do jogo, na ordem dos itens; posicao da lista
    int items = 0;
    int tab = 0, opt = 0;                 // painel: aba e linha
    uint32_t screenObj = 0;               // telao (objeto do filme, shell->m_movieScreen): onde o painel e desenhado
};
static std::mutex g_m;
static View g_view;

// Thread do EE
static const bool g_enabled = [] { const char *e = std::getenv("WOTM_TEXTMENU"); return !(e && e[0] == '0'); }();
static bool g_onOptions = false;   // tela 1: cursor no "recompiled options"
static bool g_open = false;        // painel "recompiled options" aberto
static float g_panel = 0.f;
static int g_suppress = 0;         // quadros sem entrada do jogo depois de fechar o painel (o triangulo ainda apertado)

// ------------------------------------------------------------------ painel "recompiled options": abas e opcoes
// Cada opcao e uma chave do wotm_settings.hpp; mudar grava o settings.ini e vale na hora (video na thread da janela, mods e
// teclado na proxima leitura deles).
namespace st = wotm::settings;
enum Tab { TabVideo, TabFeatures, TabControls, kTabs };
constexpr const char *kTabNames[kTabs] = {"VIDEO", "FEATURES", "CONTROLS"};
constexpr int kMaxOpts = 5;
struct Opt { st::Key key; const char *label; const char *hint; };
struct TabDef { int n; Opt opt[kMaxOpts]; };
constexpr TabDef kTabDefs[kTabs] = {
    {5,
     {{st::Res, "RESOLUTION", "HEIGHT OF THE RENDERED FRAME"},
      {st::Aspect, "ASPECT RATIO", "DYNAMIC FOLLOWS THE WINDOW"},
      {st::Display, "DISPLAY MODE", "F11 SWITCHES BETWEEN WINDOW AND FULLSCREEN"},
      {st::Rate, "REFRESH RATE", "GAME LOGIC STAYS AT 60"},
      {st::Vsync, "VSYNC", "SYNC WITH THE MONITOR - NO TEARING"}}},
    {3,
     {{st::LockToggle, "LOCK-ON TOGGLE", "R1 LOCKS AND UNLOCKS - D-PAD SWITCHES TARGET"},
      {st::EnemyBars, "ENEMY BARS", "HEALTH AND ENERGY OVER EVERY ENEMY"},
      {st::DamagePop, "DAMAGE NUMBERS", "A NUMBER ON EVERY HIT"}}},
    {1, {{st::KbSeparate, "KEYBOARD", "PLAY ALONG WITH PAD 1 OR AS ITS OWN PLAYER"}}},
};
static int g_tab = TabVideo;
static int g_optSel[kTabs] = {};

// Valores que a opcao percorre, em ordem crescente
static int choices(st::Key k, int out[8])
{
    switch (k)
    {
    case st::Res: std::copy(std::begin(st::kResolutions), std::end(st::kResolutions), out); return int(std::size(st::kResolutions));
    case st::Rate: std::copy(std::begin(st::kRates), std::end(st::kRates), out); return int(std::size(st::kRates));
    case st::Aspect: for (int i = 0; i < st::kAspects; ++i) out[i] = i; return st::kAspects;
    case st::Display: for (int i = 0; i < 3; ++i) out[i] = i; return 3;
    default: out[0] = 0; out[1] = 1; return 2;
    }
}
// Proximo valor no sentido dir (+1/-1); dois valores dao a volta, listas param nas pontas (wrap forca a volta). Um valor fora da
// lista (variavel de ambiente) vai ao vizinho da lista naquele sentido.
static int stepValue(st::Key k, int cur, int dir, bool wrap)
{
    int c[8];
    const int n = choices(k, c);
    int i = -1;
    for (int j = 0; j < n; ++j)
        if (c[j] == cur) i = j;
    if (i < 0)
    {
        if (dir > 0) { for (int j = 0; j < n; ++j) if (c[j] > cur) return c[j]; return c[n - 1]; }
        for (int j = n - 1; j >= 0; --j) if (c[j] < cur) return c[j];
        return c[0];
    }
    i += dir;
    if (n == 2 || wrap) i = (i + n) % n;
    return c[std::clamp(i, 0, n - 1)];
}
static void valueText(st::Key k, int v, char *buf, size_t sz)
{
    static const char *const kDisplay[3] = {"WINDOWED", "BORDERLESS", "EXCLUSIVE"};
    static const char *const kAspectUi[st::kAspects] = {"DYNAMIC", "4:3", "16:9", "16:10", "21:9", "32:9"};
    switch (k)
    {
    case st::Res: std::snprintf(buf, sz, "%dP", v); break;
    case st::Aspect: std::snprintf(buf, sz, "%s", kAspectUi[v > 0 && v < st::kAspects ? v : 0]); break;
    case st::Display: std::snprintf(buf, sz, "%s", kDisplay[std::clamp(v, 0, 2)]); break;
    case st::Rate: if (v > 0) std::snprintf(buf, sz, "%d HZ", v); else std::snprintf(buf, sz, "MONITOR"); break;
    case st::KbSeparate: std::snprintf(buf, sz, "%s", v ? "OWN PLAYER" : "SAME AS PAD 1"); break;
    default: std::snprintf(buf, sz, "%s", v ? "ON" : "OFF"); break;
    }
}

// Entrada do painel: direto dos dispositivos (todos), como a escolha de controles do wotm_padsel.inc, com as teclas do jogo no
// teclado (ps2_pad.cpp: setas/WASD, X/Espaco = X, V/C/Esc = triangulo/bola, Q/E = L1/R1) e o Tab (Shift+Tab volta). O roteiro de
// teste WOTM_PADSEL_SCRIPT vale aqui tambem (acoes l1 e r1 para as abas).
enum PanelAct { PUp, PDown, PLeft, PRight, POk, PBack, PPrev, PNext, kPanelActs };
static bool g_panelPrev[kPanelActs] = {};
static void readPanel(bool out[kPanelActs])
{
    using namespace wotm_padsel;
    std::fill(out, out + kPanelActs, false);
    for (int d = 0; d < kDevs; ++d)
    {
        if (!present(d)) continue;
        bool a[kActions] = {};
        bool back2 = false, next2 = false, prev2 = false;
        if (d == kKb)
        {
            a[Up] = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
            a[Down] = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
            a[Left] = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
            a[Right] = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
            a[Ok] = IsKeyDown(KEY_X) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER);
            a[Back] = IsKeyDown(KEY_V) || IsKeyDown(KEY_KP_1) || IsKeyDown(KEY_C) || IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_BACKSPACE);
            a[L1] = IsKeyDown(KEY_Q);
            a[R1] = IsKeyDown(KEY_E);
            const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            next2 = IsKeyDown(KEY_TAB) && !shift;
            prev2 = IsKeyDown(KEY_TAB) && shift;
            const double now = GetTime();
            for (const ScriptStep &s : g_script)
                if (s.dev == kKb && now >= s.t0 && now < s.t0 + s.dur) a[s.act] = true;
        }
        else
        {
            readDevice(d, a);
            back2 = IsGamepadButtonDown(d, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);   // bola tambem volta
        }
        out[PUp] |= a[Up];
        out[PDown] |= a[Down];
        out[PLeft] |= a[Left];
        out[PRight] |= a[Right];
        out[POk] |= a[Ok];
        out[PBack] |= a[Back] || back2;
        out[PPrev] |= a[L1] || prev2;
        out[PNext] |= a[R1] || next2;
    }
}
static void openPanel()
{
    g_open = true;
    readPanel(g_panelPrev);   // o X que abriu ainda esta apertado: so conta o proximo
}
// Um quadro do painel aberto (no screenGetInput da tela 1, que o jogo chama a cada quadro dela)
static void panelInput(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    bool now[kPanelActs], e[kPanelActs];
    readPanel(now);
    for (int i = 0; i < kPanelActs; ++i)
        e[i] = now[i] && !g_panelPrev[i];
    std::copy(now, now + kPanelActs, g_panelPrev);
    if (e[PBack])
    {
        g_open = false;
        g_suppress = 20;
        wotm_padsel::sfx(rdram, ctx, runtime, 6);
        return;
    }
    if (e[PPrev] || e[PNext])
    {
        g_tab = (g_tab + (e[PNext] ? 1 : kTabs - 1)) % kTabs;
        wotm_padsel::sfx(rdram, ctx, runtime, e[PNext] ? 4 : 2);
        return;
    }
    const TabDef &t = kTabDefs[g_tab];
    int &sel = g_optSel[g_tab];
    if (e[PUp] || e[PDown])
    {
        sel = (sel + (e[PDown] ? 1 : t.n - 1)) % t.n;
        wotm_padsel::sfx(rdram, ctx, runtime, e[PDown] ? 4 : 2);
        return;
    }
    if (e[PLeft] || e[PRight] || e[POk])
    {
        const st::Key k = t.opt[sel].key;
        const int cur = st::get(k), v = stepValue(k, cur, e[PLeft] ? -1 : 1, e[POk]);
        if (v == cur)
            return;
        st::set(k, v);
        char buf[32];
        valueText(k, v, buf, sizeof buf);
        std::fprintf(stderr, "[settings] %s = %s (%s=%s)\n", t.opt[sel].label, buf, st::kEnv[k], st::format(k, v).c_str());
        wotm_padsel::sfx(rdram, ctx, runtime, e[POk] ? 8 : (e[PLeft] ? 2 : 4));
    }
}
static PS2Runtime::RecompiledFunction g_origMain = nullptr, g_origModes1P = nullptr, g_origModes2P = nullptr, g_origTransition = nullptr,
                                      g_origGetInput = nullptr;
static uint32_t g_mainRa = 0, g_modes1Ra = 0, g_modes2Ra = 0, g_transRa = 0, g_inRa = 0;
static bool g_mainPending = false, g_modes1Pending = false, g_modes2Pending = false, g_transPending = false, g_inPending = false;

static uint32_t itemSwitch(uint8_t *rdram, int screen, int item) { return rd32(rdram, kShell + 0x25C + uint32_t(screen * 10 + item) * 4); }
static uint32_t curSelAddr(int screen) { return wotm::sym::currentSelection + 4u * uint32_t(screen); }

// Linhas com os itens que o jogo oferece na tela (os itens 1..numSelectables)
static int buildRows(uint8_t *rdram, const ScreenDef &d, Row out[kMaxRows])
{
    int n = d.numSel ? static_cast<int>(rd32(rdram, d.numSel)) : d.rows;
    if (n < 1 || n > d.rows) n = d.rows;
    int rows = 0;
    for (int i = 0; i < d.rows; ++i)
        if (d.row[i].item == kOptionsRow || d.row[i].item <= n)
            out[rows++] = d.row[i];
    return rows;
}

// Linha do cursor: a do item em currentSelection[tela] (ou a do "recompiled options")
static int cursorRow(uint8_t *rdram, int screen, const Row *row, int rows)
{
    const int cs = static_cast<int>(rd32(rdram, curSelAddr(screen)));
    for (int r = 0; r < rows; ++r)
        if (screen == 1 && g_onOptions ? row[r].item == kOptionsRow : row[r].item == cs)
            return r;
    return 0;
}

// Tela cujos itens estao a mostra e o alfa deles: parada numa tela, ela com 1; nas transicoes, o targetAlpha do fade de entrada (a
// tela nova) ou de saida (a anterior)
static float screenAlpha(uint8_t *rdram, uint32_t &screen)
{
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen), last = rd32(rdram, wotm::sym::lastScreen);
    screen = cur;
    if (!rd32(rdram, wotm::sym::betweenScreens))
        return 1.f;
    float t;
    std::memcpy(&t, rdram + wotm::sym::targetAlpha, 4);
    t = std::clamp(t, 0.f, 1.f);
    if (rd32(rdram, wotm::sym::fadingIn_2979)) return t;
    if (rd32(rdram, wotm::sym::fadingOut_2980) && last != cur) { screen = last; return t; }
    return 0.f;
}

// Tela 5: o que a escolha de controles deixa dos itens (escondidos durante ela, o fade dela depois)
static float padSelFactor()
{
    using namespace wotm_padsel;
    if (g_phase == Config || g_phase == Leaving) return 0.f;
    if (g_phase == Reveal) return std::clamp(g_reveal, 0.f, 1.f);
    return 1.f;
}

// ------------------------------------------------------------------ logo do telao so no "press start"
// O logo "War of the Monsters" do telao sao dois objetos (as duas metades), cada um num grupo de um filho; os dois grupos sao irmaos
// do objeto do filme (shell->m_movieScreen) num grupo de 3 filhos, alocado logo depois deles. Fora do "press start" (tela 20) o logo
// fica com alfa 0 e o telao inteiro e da lista e do painel; nas transicoes de/para a tela 20 ele acompanha o fade.
constexpr uint32_t kPressStart = 20;
static uint32_t g_logoTelao = 0;   // telao do ultimo achado (outro: procura de novo)
static uint32_t g_logoObj[4] = {};
static int g_logoN = 0;

static uint16_t rd16(const uint8_t *rdram, uint32_t a) { uint16_t v; std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 2); return v; }

static void findLogo(uint8_t *rdram, uint32_t telao)
{
    g_logoTelao = telao;
    g_logoN = 0;
    for (uint32_t a = telao + 0x10; a < telao + 0x4000 && a + 0x40 < PS2_RAM_SIZE; a += 0x10)
    {
        if ((rdram[a] & 0x3F) != 1)   // GROUP_NODE: numKids (u16) +8, filhos +0x20
            continue;
        const uint32_t n = rd16(rdram, a + 8);
        if (n < 2 || n > 8)
            continue;
        bool has = false;
        for (uint32_t k = 0; k < n; ++k)
            has = has || (rd32(rdram, a + 0x20 + 4 * k) & 0x1FFFFFFFu) == telao;
        if (!has)
            continue;
        for (uint32_t k = 0; k < n; ++k)
        {
            const uint32_t c = rd32(rdram, a + 0x20 + 4 * k) & 0x1FFFFFFFu;
            if (c == telao || c < 0x1000u || c + 0x40 >= PS2_RAM_SIZE)
                continue;
            if ((rdram[c] & 0x3F) == 0 && g_logoN < 4)
                g_logoObj[g_logoN++] = c;
            else if ((rdram[c] & 0x3F) == 1)
                for (uint32_t g = 0, m = std::min<uint32_t>(rd16(rdram, c + 8), 4u); g < m; ++g)
                    if (const uint32_t o = rd32(rdram, c + 0x20 + 4 * g) & 0x1FFFFFFFu; o >= 0x1000u && (rdram[o] & 0x3F) == 0 && g_logoN < 4)
                        g_logoObj[g_logoN++] = o;
        }
        break;
    }
    std::fprintf(stderr, "[mainmenu] logo do telao: %d objetos%s\n", g_logoN, g_logoN ? " (some fora do press start)" : ", nao achado");
}

// A cada quadro do jogo (wotmCaptureFrame, antes da coleta da cena)
static void logoFrame(uint8_t *rdram)
{
    if (!g_enabled)
        return;
    const uint32_t telao = rd32(rdram, wotm_padsel::kMovieScreen) & 0x1FFFFFFFu;
    if (telao < 0x1000u || telao + 0x40 >= PS2_RAM_SIZE)
        return;
    if (telao != g_logoTelao)
        findLogo(rdram, telao);
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen), last = rd32(rdram, wotm::sym::lastScreen);
    float a = cur == kPressStart ? 1.f : 0.f;
    if (rd32(rdram, wotm::sym::betweenScreens))
    {
        float t;
        std::memcpy(&t, rdram + wotm::sym::targetAlpha, 4);
        t = std::clamp(t, 0.f, 1.f);
        if (rd32(rdram, wotm::sym::fadingIn_2979))
            a = cur == kPressStart ? t : 0.f;
        else if (rd32(rdram, wotm::sym::fadingOut_2980))
            a = last == kPressStart && last != cur ? t : 0.f;
    }
    for (int i = 0; i < g_logoN; ++i)
        std::memcpy(rdram + g_logoObj[i] + 0x4C, &a, 4);
}

// A cada quadro (saida das funcoes das telas e do screenTransition): esconde os itens do jogo e publica a lista
static void frame(uint8_t *rdram)
{
    uint32_t screen = 0;
    float alpha = screenAlpha(rdram, screen);
    const ScreenDef *d = screenDef(screen);
    View v;
    if (d && screen == 5) alpha *= padSelFactor();
    if (d)
    {
        v.screen = d->screen;
        v.rows = buildRows(rdram, *d, v.row);
        v.sel = cursorRow(rdram, d->screen, v.row, v.rows);
        for (int item = 1; item <= d->rows; ++item)
        {
            // posicao: os objetos do filho em uso (so ele esta na cena)
            int k = 0;
            wotm_padsel::forSwitchObjects(rdram, itemSwitch(rdram, d->screen, item), [&](uint32_t o, bool used) {
                const float zero = 0.f;
                std::memcpy(rdram + o + 0x4C, &zero, 4);
                if (used && k < 4 && v.items < kMaxRows) v.itemObj[v.items][k++] = o;
            });
            if (k) ++v.items;
        }
    }
    v.on = d && alpha > 0.f;
    if (rd32(rdram, wotm::sym::currScreen) != 1)
        g_open = false;
    g_panel = std::clamp(g_panel + (g_open ? 0.1f : -0.1f), 0.f, 1.f);
    v.alpha = alpha;
    v.panel = v.screen == 1 ? g_panel : 0.f;
    v.tab = g_tab;
    v.opt = g_optSel[g_tab];
    v.screenObj = rd32(rdram, wotm_padsel::kMovieScreen) & 0x1FFFFFFFu;
    std::lock_guard<std::mutex> lk(g_m);
    g_view = v;
}

// Saida de uma funcao do jogo -> frame (mesmo padrao para as telas e o screenTransition)
#define WOTM_MM_EXIT_HOOK(name, fn, orig, ra, pending)                                \
    static void name(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)          \
    {                                                                                 \
        if (ctx->pc == (fn))                                                          \
        {                                                                             \
            ra = GPR_U32(ctx, 31);                                                    \
            pending = true;                                                           \
        }                                                                             \
        orig(rdram, ctx, runtime);                                                    \
        if (!pending || ctx->pc != ra)                                                \
            return;                                                                   \
        pending = false;                                                              \
        frame(rdram);                                                                 \
    }
WOTM_MM_EXIT_HOOK(mainHook, wotm::fn::screenMain__Fv, g_origMain, g_mainRa, g_mainPending)
WOTM_MM_EXIT_HOOK(modes1Hook, wotm::fn::screenGameModes1P__Fv, g_origModes1P, g_modes1Ra, g_modes1Pending)
WOTM_MM_EXIT_HOOK(modes2Hook, wotm::fn::screenGameModes2P__Fv, g_origModes2P, g_modes2Ra, g_modes2Pending)
WOTM_MM_EXIT_HOOK(transitionHook, wotm::fn::screenTransition__Fb, g_origTransition, g_transRa, g_transPending)
#undef WOTM_MM_EXIT_HOOK

// Telas da lista: cima/baixo movem o cursor (o currentSelection da tela, ou o "recompiled options" da 1); X e triangulo seguem para o
// jogo. No "recompiled options" o X abre o painel; com ele aberto, so o triangulo (fecha) conta.
static void getInputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen);
    if (ctx->pc == wotm::fn::screenGetInput__Fi && screenDef(cur))
    {
        g_inRa = GPR_U32(ctx, 31);
        g_inPending = true;
    }
    g_origGetInput(rdram, ctx, runtime);
    if (!g_inPending || ctx->pc != g_inRa)
        return;
    g_inPending = false;
    const ScreenDef &d = *screenDef(cur);
    Row row[kMaxRows];
    const int rows = buildRows(rdram, d, row);
    if (rows == 0)
        return;
    const uint32_t raw = GPR_U32(ctx, 2), code = raw & 0xFFFFu;
    uint32_t out = raw;
    if (cur == 1 && (g_open || g_suppress > 0))
    {
        // painel aberto: o jogo nao ve nada (a entrada do painel vem dos dispositivos); fechado ha pouco, o triangulo que fechou
        // ainda nao pode virar o "voltar" do menu principal
        out = 0;
        if (g_open)
            panelInput(rdram, ctx, runtime);
        else
            --g_suppress;
    }
    else if (code == 1 || code == 2)
    {
        const int r = (cursorRow(rdram, d.screen, row, rows) + (code == 1 ? rows - 1 : 1)) % rows;
        g_onOptions = d.screen == 1 && row[r].item == kOptionsRow;
        if (row[r].item != kOptionsRow)
            wotm_padsel::wr32(rdram, curSelAddr(d.screen), static_cast<uint32_t>(row[r].item));
        wotm_padsel::sfx(rdram, ctx, runtime, code == 1 ? 2 : 4);
        out = 0;
    }
    else if (code == 6 && cur == 1 && g_onOptions)
    {
        openPanel();
        wotm_padsel::sfx(rdram, ctx, runtime, 8);
        out = 0;
    }
    ctx->r[2] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(out)));
}


// ------------------------------------------------------------------ desenho (janela)

constexpr int kW = 1200, kH = 900;   // textura da lista: kMaxRows linhas de kH / kMaxRows
static RenderTexture2D g_rt{};
static Font g_font{};
static bool g_fontTried = false;

static Font &fallbackFont()
{
    if (!g_fontTried)
    {
        g_fontTried = true;
        for (const char *path : {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/arialbd.ttf"})
            if (FileExists(path) && (g_font = LoadFontEx(path, 96, nullptr, 0)).texture.id)
                break;
        if (!g_font.texture.id) g_font = GetFontDefault();
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
    }
    return g_font;
}
static Color gsToRgb(Color c)   // cor na escala do GS (0x80 = 1,0) -> a do raylib
{
    auto c8 = [](unsigned char v) { return static_cast<unsigned char>(std::min(255, v * 2)); };
    return Color{c8(c.r), c8(c.g), c8(c.b), c8(c.a)};
}
// Passo de uma letra da fonte do jogo, em pixels de uma textura rw x rh (wotm_padsel::text: 1,6 x a altura em unidades da tela do
// jogo, mais 2 de espaco)
static float glyphAdvance(int rw, int rh, float h)
{
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const float sx = 640.f / gr.w, sy = 224.f / float(rh);
    return (1.6f * h * sy + 2.f) / sx;
}
static float textWidth(uint64_t tex0, int rw, int rh, const char *s, float h)
{
    if (tex0)
        return glyphAdvance(rw, rh, h) * float(std::strlen(s));
    return MeasureTextEx(fallbackFont(), s, h, 1.f).x;
}

// Texto com a fonte do jogo (atlas `tex0`) centrado em (cx, cy), numa textura rw x rh; sem atlas, Bahnschrift do Windows (ou a do
// raylib). `c` na escala do GS (0x80 = 1,0). A fonte do jogo nao tem ':' (o "16:9"): sai como dois pontos desenhados.
static void label(std::vector<wotm::TextQuad> &quads, const wotm::Ram &ram, uint64_t tex0, const char *s, float cx, float cy, float h, Color c,
                  int rw = kW, int rh = kH)
{
    if (tex0)
    {
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s", s);
        const size_t n = std::strlen(buf);
        const float adv = glyphAdvance(rw, rh, h), x0 = cx - adv * float(n) * 0.5f;
        for (size_t i = 0; i < n; ++i)
            if (buf[i] == ':')
            {
                buf[i] = ' ';
                const float d = h * 0.16f, x = x0 + adv * (float(i) + 0.5f) - d * 0.5f;
                DrawRectangleRec(Rectangle{x, cy - h * 0.28f, d, d}, gsToRgb(c));
                DrawRectangleRec(Rectangle{x, cy + h * 0.12f, d, d}, gsToRgb(c));
            }
        wotm_padsel::text(quads, ram, tex0, rw, rh, buf, cx, cy, h, c);
        return;
    }
    const Vector2 sz = MeasureTextEx(fallbackFont(), s, h, 1.f);
    DrawTextEx(fallbackFont(), s, Vector2{cx - sz.x * 0.5f, cy - sz.y * 0.5f}, h, 1.f, gsToRgb(c));
}
// O mesmo, alinhado a esquerda em x0
static void labelLeft(std::vector<wotm::TextQuad> &quads, const wotm::Ram &ram, uint64_t tex0, const char *s, float x0, float cy, float h, Color c,
                      int rw, int rh)
{
    label(quads, ram, tex0, s, x0 + textWidth(tex0, rw, rh, s, h) * 0.5f, cy, h, c, rw, rh);
}

// ------------------------------------------------------------------ painel (textura propria, no telao inteiro)
constexpr int kPW = 1800, kPH = 1100;   // ~1,64:1, a proporcao do telao (204 x 124)
static RenderTexture2D g_prt{};
constexpr Color kLit{121, 118, 111, 0x80}, kDim{88, 56, 34, 0x80}, kHintC{0x62, 0x5C, 0x50, 0x80};   // escala do GS

static void renderPanel(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0, const View &v)
{
    if (g_prt.id == 0)
    {
        g_prt = LoadRenderTexture(kPW, kPH);
        SetTextureFilter(g_prt.texture, TEXTURE_FILTER_BILINEAR);
    }
    const float W = float(kPW), H = float(kPH), lineH = H / 8.f;
    BeginTextureMode(g_prt);
    ClearBackground(Color{0, 0, 0, 0});
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    std::vector<wotm::TextQuad> q;
    DrawRectangleRounded(Rectangle{4.f, 4.f, W - 8.f, H - 8.f}, 0.06f, 12, Color{0, 0, 0, 175});

    // abas: L1  VIDEO  FEATURES  CONTROLS  R1
    const float tabY = lineH * 0.7f, tabH = 56.f;
    const float tabX[kTabs] = {W * 0.28f, W * 0.50f, W * 0.72f};
    for (int t = 0; t < kTabs; ++t)
    {
        const bool on = t == v.tab;
        label(q, ram, tex0, kTabNames[t], tabX[t], tabY, tabH, on ? kLit : kDim, kPW, kPH);
        if (on)
        {
            const float w = textWidth(tex0, kPW, kPH, kTabNames[t], tabH);
            DrawRectangleRec(Rectangle{tabX[t] - w * 0.5f, tabY + tabH * 0.75f, w, 5.f}, gsToRgb(kLit));
        }
    }
    auto badge = [&](const char *s, float cx) {
        const float bw = 130.f, bh = 76.f;
        DrawRectangleRoundedLinesEx(Rectangle{cx - bw * 0.5f, tabY - bh * 0.5f, bw, bh}, 0.4f, 8, 3.f, gsToRgb(kDim));
        label(q, ram, tex0, s, cx, tabY, 42.f, kDim, kPW, kPH);
    };
    badge("L1", W * 0.08f);
    badge("R1", W * 0.92f);
    DrawRectangleRec(Rectangle{W * 0.04f, lineH * 1.4f, W * 0.92f, 3.f}, Color{90, 80, 66, 120});

    // opcoes da aba: nome a esquerda, valor a direita (setas na selecionada)
    const TabDef &t = kTabDefs[std::clamp(v.tab, 0, kTabs - 1)];
    const float rowH = 54.f, valX = W * 0.66f, labX = W * 0.10f;   // o telao e inclinado: valor perto do nome
    for (int i = 0; i < t.n; ++i)
    {
        const float cy = lineH * (2.2f + float(i));
        const bool on = i == v.opt;
        if (on)
            DrawRectangleRounded(Rectangle{W * 0.03f, cy - lineH * 0.42f, W * 0.94f, lineH * 0.84f}, 0.5f, 8, Color{60, 50, 40, 130});
        labelLeft(q, ram, tex0, t.opt[i].label, labX, cy, rowH, on ? kLit : kDim, kPW, kPH);
        // fio do nome ao valor: com a inclinacao do telao, guia o olho pela linha
        const float lx1 = labX + textWidth(tex0, kPW, kPH, t.opt[i].label, rowH) + 24.f, lx2 = valX - W * 0.19f - 36.f;
        if (lx2 > lx1)
            DrawRectangleRec(Rectangle{lx1, cy - 1.f, lx2 - lx1, 2.f}, Color{110, 96, 78, static_cast<unsigned char>(on ? 140 : 70)});
        char buf[32];
        valueText(t.opt[i].key, st::get(t.opt[i].key), buf, sizeof buf);
        label(q, ram, tex0, buf, valX, cy, rowH, on ? kLit : kDim, kPW, kPH);
        if (on)
        {
            const Color a = gsToRgb(kLit);
            const float s = 20.f, dx = W * 0.19f;
            DrawTriangle(Vector2{valX - dx - s, cy}, Vector2{valX - dx + s * 0.7f, cy + s}, Vector2{valX - dx + s * 0.7f, cy - s}, a);
            DrawTriangle(Vector2{valX + dx + s, cy}, Vector2{valX + dx - s * 0.7f, cy - s}, Vector2{valX + dx - s * 0.7f, cy + s}, a);
        }
    }
    // dica da opcao selecionada e os botoes
    if (v.opt >= 0 && v.opt < t.n)
        label(q, ram, tex0, t.opt[v.opt].hint, W * 0.5f, lineH * 6.95f, 38.f, kHintC, kPW, kPH);
    label(q, ram, tex0, "L1 R1 - TABS      X - CHANGE      TRIANGLE - BACK", W * 0.5f, lineH * 7.6f, 34.f, kHintC, kPW, kPH);
    scene.drawQuads(q, W, H);
    EndBlendMode();
    EndTextureMode();
}

static void renderUi(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    if (!v.on || v.alpha <= 0.f || v.rows == 0)
        return;
    if (g_rt.id == 0)
    {
        g_rt = LoadRenderTexture(kW, kH);
        SetTextureFilter(g_rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    constexpr float rowH = float(kH) / kMaxRows;
    // cores dos itens do telao: aceso creme, apagado laranja terroso (escala do GS)
    constexpr Color lit{121, 118, 111, 0x80}, dim{88, 56, 34, 0x80};
    BeginTextureMode(g_rt);
    ClearBackground(Color{0, 0, 0, 0});
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    std::vector<wotm::TextQuad> quads;
    const float list = 1.f - v.panel;
    // altura unica das letras: a maior em que a linha mais longa cabe (cada letra da fonte do jogo tem ~0,75 x a altura de largura,
    // nesta textura, mais o espaco)
    size_t longest = 1;
    for (int r = 0; r < v.rows; ++r)
        longest = std::max(longest, std::strlen(v.row[r].label));
    const float textH = std::min(rowH * 0.5f, kW * 0.9f / (0.8f * float(longest)));
    if (list > 0.f)
    {
        const float top = (kH - rowH * float(v.rows)) * 0.5f;
        for (int r = 0; r < v.rows; ++r)
        {
            const float cy = top + rowH * (float(r) + 0.5f);
            const bool on = r == v.sel;
            if (on)
                DrawRectangleRounded(Rectangle{kW * 0.04f, cy - rowH * 0.42f, kW * 0.92f, rowH * 0.84f}, 0.5f, 8,
                                     Color{0, 0, 0, static_cast<unsigned char>(110 * list)});
            Color c = on ? lit : dim;
            c.a = static_cast<unsigned char>(0x80 * list);
            const char *s = v.row[r].label;
            label(quads, ram, tex0, s, kW * 0.5f, cy, textH, c);
        }
    }
    scene.drawQuads(quads, float(kW), float(kH));
    EndBlendMode();
    EndTextureMode();
    if (v.panel > 0.f)
        renderPanel(scene, ram, tex0, v);
}

// Plano do texto de um item (de varios objetos, o de menor altura: o texto, nao o brilho)
static bool itemPlane(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam, const uint32_t objs[4], wotm_padsel::Plane &out)
{
    bool got = false;
    wotm_padsel::Plane p;
    for (int i = 0; i < 4 && objs[i]; ++i)
        if (wotm_padsel::objectPlane(scene, ram, cam, objs[i], p) && (!got || p.y1 - p.y0 < out.y1 - out.y0))
        {
            out = p;
            got = true;
        }
    return got;
}

// O painel no plano do telao, no telao inteiro (o logo so aparece no "press start"), centrado
static void drawPanelOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam, const View &v)
{
    wotm_padsel::Plane pl;
    if (!v.on || v.panel <= 0.f || v.alpha <= 0.f || g_prt.id == 0 || !v.screenObj || !wotm_padsel::objectPlane(scene, ram, cam, v.screenObj, pl))
        return;
    const float fyTop = 0.15f, fyBottom = 0.97f;   // o canto de cima do telao fica atras da faixa preta do menu
    const float hy = (pl.y1 - pl.y0) * (fyBottom - fyTop), wx = std::min(hy * float(kPW) / float(kPH), (pl.x1 - pl.x0) * 0.94f);
    const float h = wx * float(kPH) / float(kPW);   // largura limitada: a altura acompanha (proporcao da textura)
    wotm_padsel::drawOnPlane(pl, g_prt.texture, (pl.x0 + pl.x1) * 0.5f - wx * 0.5f, pl.y1 - (pl.y1 - pl.y0) * fyTop - (hy - h) * 0.5f, wx, h,
                             v.alpha * v.panel, 0.003f * (pl.x1 - pl.x0));
}

// A lista no plano dos itens do jogo: do topo do primeiro ao pe do ultimo, centrada na coluna deles. Dentro do BeginMode3D do quadro.
static void drawListOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam, const View &v)
{
    if (!v.on || v.alpha <= 0.f || v.items == 0 || g_rt.id == 0)
        return;
    wotm_padsel::Plane base, p;
    if (!itemPlane(scene, ram, cam, v.itemObj[0], base))
        return;
    float top = -1e30f, bottom = 1e30f, cx = 0.f, itemH = 0.f;
    int n = 0;
    for (int i = 0; i < v.items; ++i)
    {
        if (!itemPlane(scene, ram, cam, v.itemObj[i], p))
            continue;
        const Vector3 d = wotm_padsel::vsub(p.c, base.c);
        const float y = wotm_padsel::vdot(d, base.up), h = p.y1 - p.y0;
        top = std::max(top, y + h * 0.5f);
        bottom = std::min(bottom, y - h * 0.5f);
        cx += wotm_padsel::vdot(d, base.right);
        itemH = std::max(itemH, h);
        ++n;
    }
    if (!n || top <= bottom)
        return;
    cx /= float(n);
    // cada uma das kMaxRows vagas da textura tem a altura do passo dos itens do jogo (o vao dividido por eles); as linhas ficam
    // centradas no vao
    const float span = top - bottom, hy = span * float(kMaxRows) / float(n), wx = hy * float(kW) / float(kH);
    float mid = (top + bottom) * 0.5f;
    // sem o logo (logoFrame), a lista vai para o centro do telao
    if (wotm_padsel::Plane tl; g_logoN && v.screenObj && wotm_padsel::objectPlane(scene, ram, cam, v.screenObj, tl))
    {
        const Vector3 d = wotm_padsel::vsub(tl.c, base.c);
        mid = wotm_padsel::vdot(d, base.up) - (tl.y1 - tl.y0) * 0.06f;   // centro da parte visivel (o topo some atras da faixa preta)
        cx = wotm_padsel::vdot(d, base.right);
    }
    static int logged = 0;
    if (!(logged & (1 << v.screen)))
    {
        logged |= 1 << v.screen;
        std::fprintf(stderr, "[mainmenu] tela %d em texto: %d itens do jogo, vao %.1f, linha %.1f\n", v.screen, n, span, itemH);
    }
    wotm_padsel::drawOnPlane(base, g_rt.texture, cx - wx * 0.5f, mid + hy * 0.5f, wx, hy, v.alpha, 0.004f * (base.x1 - base.x0));
}

static void drawOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    drawListOnTelao(scene, ram, cam, v);
    drawPanelOnTelao(scene, ram, cam, v);
}
} // namespace wotm_mainmenu

bool wotmTextMenuOn() { return wotm_mainmenu::g_enabled; }

void wotmMainMenuInstall(PS2Runtime &runtime)
{
    using namespace wotm_mainmenu;
    if (!g_enabled)
        return;
    struct H { uint32_t addr; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; };
    const H hooks[] = {
        {wotm::fn::screenMain__Fv, &g_origMain, &mainHook},
        {wotm::fn::screenGameModes1P__Fv, &g_origModes1P, &modes1Hook},
        {wotm::fn::screenGameModes2P__Fv, &g_origModes2P, &modes2Hook},
        {wotm::fn::screenTransition__Fb, &g_origTransition, &transitionHook},
        {wotm::fn::screenGetInput__Fi, &g_origGetInput, &getInputHook},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
    std::fprintf(stderr, "[mainmenu] menus de itens em texto: principal, modos de 1 jogador e do multiplayer (WOTM_TEXTMENU=0 volta aos do jogo)\n");
}

void wotmMainMenuLogoFrame(uint8_t *rdram) { wotm_mainmenu::logoFrame(rdram); }
