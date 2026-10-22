// wotm_mainmenu.inc: menu principal (tela 1) em texto, com o item "Recompiled options" (incluido por wotm_live.inc, depois do
// wotm_padsel.inc, de quem usa o desenho no telao e a fonte do jogo).
//
// Os itens do jogo (shell->m_menuItemSw[1][1..5]: 1 player, 2 player, options, load game, unlocks) sao quadros com a palavra em
// imagem. Aqui todos os objetos dos switches ficam com alfa 0 a cada quadro da tela 1 (e no fade de saida dela) e a lista e desenhada
// com a fonte do jogo (a mesma da lista de saves) numa textura propria, posta no plano dos itens, do topo do primeiro ao pe do
// ultimo. O "2 player" vira "multiplayer" e, depois de "options", entra "recompiled options".
// O cursor e nosso: o gancho do screenGetInput (tela 1) consome cima/baixo e, no X, poe o currentSelection[1] no item do jogo
// escolhido e deixa o codigo 6 seguir (o screenMain faz o que o item faz, como no retail). No "recompiled options" o X abre o
// painel (por enquanto vazio) sobre a lista, sem trocar de tela; triangulo (codigo 5) fecha. Com o painel aberto o jogo nao ve
// entrada nenhuma.
// WOTM_TEXTMENU=0: itens do retail (e o rotulo "multiplayer" do wotm_padsel.inc).
namespace wotm_mainmenu
{
using wotm_padsel::rd32;
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr int kMaxRows = 6;
constexpr int kOptionsRow = -1;   // item que nao e do jogo

struct Row { int item; const char *label; };
// Ordem da lista: os itens do jogo (1..5) e o nosso depois de "options"
constexpr Row kAllRows[kMaxRows] = {{1, "1 PLAYER"}, {2, "MULTIPLAYER"},  {3, "OPTIONS"},
                                    {kOptionsRow, "RECOMPILED OPTIONS"}, {4, "LOAD GAME"}, {5, "UNLOCKS"}};

struct View
{
    bool on = false;
    float alpha = 0.f;     // da tela 1 (fades de entrada e saida)
    float panel = 0.f;     // 0 lista, 1 painel aberto
    int sel = 0;
    int rows = 0;
    Row row[kMaxRows] = {};
    uint32_t itemObj[kMaxRows][4] = {};   // objetos do filho 0 (apagado) de cada item do jogo, na ordem dos itens; posicao da lista
    int items = 0;
};
static std::mutex g_m;
static View g_view;

// Thread do EE
static const bool g_enabled = [] { const char *e = std::getenv("WOTM_TEXTMENU"); return !(e && e[0] == '0'); }();
static int g_sel = -1;           // linha do cursor (-1: tirar do currentSelection[1] na primeira vez)
static bool g_open = false;      // painel "recompiled options" aberto
static float g_panel = 0.f;
static int g_rows = 0;
static Row g_row[kMaxRows] = {};
static PS2Runtime::RecompiledFunction g_origMain = nullptr, g_origTransition = nullptr, g_origGetInput = nullptr;
static uint32_t g_mainRa = 0, g_transRa = 0, g_inRa = 0;
static bool g_mainPending = false, g_transPending = false, g_inPending = false;

static uint32_t itemSwitch(uint8_t *rdram, int item) { return rd32(rdram, kShell + 0x25C + uint32_t(10 + item) * 4); }

// Linhas com os itens que o jogo oferece (numSelectables da tela 1: os itens 1..n)
static void buildRows(uint8_t *rdram)
{
    int n = static_cast<int>(rd32(rdram, wotm::sym::numSelectables_2784));
    if (n < 1 || n > 5) n = 5;
    g_rows = 0;
    for (const Row &r : kAllRows)
        if (r.item == kOptionsRow || r.item <= n)
            g_row[g_rows++] = r;
    if (g_sel >= g_rows) g_sel = g_rows - 1;
}

// Alfa da tela 1: 1 parada nela; nas transicoes, o targetAlpha do fade de entrada (para a 1) ou de saida (vindo dela)
static float screenAlpha(uint8_t *rdram)
{
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen), last = rd32(rdram, wotm::sym::lastScreen);
    if (!rd32(rdram, wotm::sym::betweenScreens))
        return cur == 1 ? 1.f : 0.f;
    float t;
    std::memcpy(&t, rdram + wotm::sym::targetAlpha, 4);
    t = std::clamp(t, 0.f, 1.f);
    if (cur == 1 && rd32(rdram, wotm::sym::fadingIn_2979)) return t;
    if (last == 1 && cur != 1 && rd32(rdram, wotm::sym::fadingOut_2980)) return t;
    return 0.f;
}

// A cada quadro (saida do screenMain e do screenTransition): esconde os itens do jogo e publica a lista
static void frame(uint8_t *rdram)
{
    const float alpha = screenAlpha(rdram);
    View v;
    v.on = alpha > 0.f;
    if (v.on)
    {
        buildRows(rdram);
        if (g_sel < 0)
        {
            const int cs = static_cast<int>(rd32(rdram, wotm::sym::currentSelection + 4));   // currentSelection[1]
            g_sel = 0;
            for (int r = 0; r < g_rows; ++r)
                if (g_row[r].item == cs) g_sel = r;
        }
        for (int item = 1; item <= 5; ++item)
        {
            // posicao: os objetos do filho em uso (so ele esta na cena)
            int k = 0;
            wotm_padsel::forSwitchObjects(rdram, itemSwitch(rdram, item), [&](uint32_t o, bool used) {
                const float zero = 0.f;
                std::memcpy(rdram + o + 0x4C, &zero, 4);
                if (used && k < 4) v.itemObj[v.items][k++] = o;
            });
            if (k) ++v.items;
        }
    }
    else if (rd32(rdram, wotm::sym::currScreen) != 1)
        g_open = false;
    g_panel = std::clamp(g_panel + (g_open ? 0.1f : -0.1f), 0.f, 1.f);
    v.alpha = alpha;
    v.panel = g_panel;
    v.sel = std::max(0, g_sel);
    v.rows = g_rows;
    std::copy(g_row, g_row + kMaxRows, v.row);
    std::lock_guard<std::mutex> lk(g_m);
    g_view = v;
}

static void mainHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenMain__Fv)
    {
        g_mainRa = GPR_U32(ctx, 31);
        g_mainPending = true;
    }
    g_origMain(rdram, ctx, runtime);
    if (!g_mainPending || ctx->pc != g_mainRa)
        return;
    g_mainPending = false;
    frame(rdram);
}

static void transitionHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenTransition__Fb)
    {
        g_transRa = GPR_U32(ctx, 31);
        g_transPending = true;
    }
    g_origTransition(rdram, ctx, runtime);
    if (!g_transPending || ctx->pc != g_transRa)
        return;
    g_transPending = false;
    frame(rdram);
}

// Tela 1: cima/baixo movem o nosso cursor; X num item do jogo segue como o X do retail com o currentSelection[1] dele; X no nosso
// abre o painel. Com o painel aberto, so o triangulo (fecha) conta.
static void getInputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenGetInput__Fi && rd32(rdram, wotm::sym::currScreen) == 1 && g_rows > 0)
    {
        g_inRa = GPR_U32(ctx, 31);
        g_inPending = true;
    }
    g_origGetInput(rdram, ctx, runtime);
    if (!g_inPending || ctx->pc != g_inRa)
        return;
    g_inPending = false;
    const uint32_t raw = GPR_U32(ctx, 2), code = raw & 0xFFFFu;
    uint32_t out = raw;
    if (g_open)
    {
        out = 0;
        if (code == 5)
        {
            g_open = false;
            wotm_padsel::sfx(rdram, ctx, runtime, 6);
        }
    }
    else if (code == 1 || code == 2)
    {
        g_sel = (std::max(0, g_sel) + (code == 1 ? g_rows - 1 : 1)) % g_rows;
        wotm_padsel::sfx(rdram, ctx, runtime, code == 1 ? 2 : 4);
        out = 0;
    }
    else if (code == 6)
    {
        const Row &r = g_row[std::clamp(g_sel, 0, g_rows - 1)];
        if (r.item == kOptionsRow)
        {
            g_open = true;
            wotm_padsel::sfx(rdram, ctx, runtime, 8);
            out = 0;
        }
        else
            wotm_padsel::wr32(rdram, wotm::sym::currentSelection + 4, static_cast<uint32_t>(r.item));
    }
    ctx->r[2] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(out)));
}

// ------------------------------------------------------------------ desenho (janela)

constexpr int kW = 1200, kH = 900;   // textura da lista: kMaxRows linhas de kH / kMaxRows
static RenderTexture2D g_rt{};
static Font g_font{};
static bool g_fontTried = false;

// Texto com a fonte do jogo (atlas `tex0`); sem atlas, Bahnschrift do Windows (ou a do raylib). `c` na escala do GS (0x80 = 1,0).
static void label(std::vector<wotm::TextQuad> &quads, const wotm::Ram &ram, uint64_t tex0, const char *s, float cx, float cy, float h, Color c)
{
    if (tex0)
    {
        wotm_padsel::text(quads, ram, tex0, kW, kH, s, cx, cy, h, c);
        return;
    }
    if (!g_fontTried)
    {
        g_fontTried = true;
        for (const char *path : {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/arialbd.ttf"})
            if (FileExists(path) && (g_font = LoadFontEx(path, 96, nullptr, 0)).texture.id)
                break;
        if (!g_font.texture.id) g_font = GetFontDefault();
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
    }
    auto c8 = [](unsigned char v) { return static_cast<unsigned char>(std::min(255, v * 2)); };
    const Vector2 sz = MeasureTextEx(g_font, s, h, 1.f);
    DrawTextEx(g_font, s, Vector2{cx - sz.x * 0.5f, cy - sz.y * 0.5f}, h, 1.f, Color{c8(c.r), c8(c.g), c8(c.b), c8(c.a)});
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
    if (v.panel > 0.f)
    {
        DrawRectangleRounded(Rectangle{kW * 0.02f, kH * 0.04f, kW * 0.96f, kH * 0.92f}, 0.08f, 8,
                             Color{0, 0, 0, static_cast<unsigned char>(150 * v.panel)});
        const unsigned char a = static_cast<unsigned char>(0x80 * v.panel);
        label(quads, ram, tex0, "RECOMPILED OPTIONS", kW * 0.5f, kH * 0.16f, textH, Color{lit.r, lit.g, lit.b, a});
        label(quads, ram, tex0, "COMING SOON", kW * 0.5f, kH * 0.5f, rowH * 0.4f, Color{dim.r, dim.g, dim.b, a});
        label(quads, ram, tex0, "TRIANGLE - BACK", kW * 0.5f, kH * 0.86f, rowH * 0.3f, Color{0x70, 0x68, 0x58, a});
    }
    scene.drawQuads(quads, float(kW), float(kH));
    EndBlendMode();
    EndTextureMode();
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

// A lista no plano dos itens do jogo: do topo do primeiro ao pe do ultimo, centrada na coluna deles. Dentro do BeginMode3D do quadro.
static void drawOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
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
    // a lista inteira (kMaxRows linhas) cobre o vao dos itens com uma linha de folga em cima e embaixo quando ha menos itens
    const float span = top - bottom, rowsSpan = span * float(kMaxRows) / float(std::max(1, n));
    const float hy = std::max(span, std::min(rowsSpan, span + 2.f * itemH)), wx = hy * float(kW) / float(kH);
    const float mid = (top + bottom) * 0.5f;
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        std::fprintf(stderr, "[mainmenu] lista em texto: %d itens do jogo, vao %.1f, linha %.1f\n", n, span, itemH);
    }
    wotm_padsel::drawOnPlane(base, g_rt.texture, cx - wx * 0.5f, mid + hy * 0.5f, wx, hy, v.alpha, 0.004f * (base.x1 - base.x0));
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
        {wotm::fn::screenTransition__Fb, &g_origTransition, &transitionHook},
        {wotm::fn::screenGetInput__Fi, &g_origGetInput, &getInputHook},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
    std::fprintf(stderr, "[mainmenu] menu principal em texto (WOTM_TEXTMENU=0 volta aos itens do jogo)\n");
}
