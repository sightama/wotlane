// wotm_pausemenu.inc: menu de pausa da luta (pauseLevelOverlay), incluido por wotm_live.inc depois do wotm_mainmenu.inc.
//  - Sem o CONTROL SCHEME (CONTROL TYPE A/B/C): o teclado, o lock-on alternado e a troca de alvo supoem o tipo A, e com 3 ou 4
//    jogadores a escolha gravava fora do m_ctlrCfg[2] (no m_vibration). A linha some (o texto dela nao e desenhado), o QUIT sobe para
//    o lugar dela e o cursor a pula; o m_ctlrCfg fica no tipo A (wotm_mainmenu.inc, frame).
//  - Mouse (etapa 9 da camera): passar por cima de um item escolhe, clique esquerdo confirma (X), direito e rodinha pelo wotm_pad.inc.
//    O mesmo nos dialogos do displayDialog (ARE YOU SURE?, fins de luta, minigames), pelos itens que o setMenuFont marca.
// O texto da pausa e do jogo (fontSpritePrintCenteredXY, unidades da tela do PS2: 640 x 224, x 0x140 no centro).
namespace wotm_pausemenu
{
using wotm_padsel::rd32;
using wotm_padsel::wr32;
constexpr int kItems = 5;                       // CONTINUE, RESTART, VIBRATION, CONTROL SCHEME, QUIT
constexpr int kScheme = 3, kQuit = 4;
constexpr int kLineY[kItems] = {0x55, 0x5E, 0x67, 0x70, 0x79};   // y de cada linha no pauseLevelOverlay
static PS2Runtime::RecompiledFunction g_origOverlay = nullptr, g_origPrint = nullptr, g_origSel = nullptr, g_origDialog = nullptr,
                                      g_origMenuFont = nullptr, g_origTitleFont = nullptr, g_origDefSize = nullptr;
static uint32_t g_overlayRa = 0, g_selRa = 0, g_dialogRa = 0;
static bool g_inOverlay = false, g_selPending = false, g_inDialog = false;
static int g_selPrev = 0, g_selN = 0;

// Diálogos do displayDialog (ARE YOU SURE?, fins de luta, minigames): cada item sai como setMenuFont(i) e depois
// fontSpritePrintCenteredXY(0, x, y, texto); a tabela guarda onde cada item foi escrito no ultimo quadro desenhado
constexpr int kMaxDlgItems = 8;
struct DlgItem { bool ok; int x, y, chars; };
static DlgItem g_dlg[kMaxDlgItems] = {};
static int g_mark = -1;   // o proximo texto e o item g_mark (-1: titulo ou outro texto)

static void overlayHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::pauseLevelOverlay__FRi)
    {
        g_overlayRa = GPR_U32(ctx, 31);
        g_inOverlay = true;
        wotm::mouse::gameMenu();   // pausa: o mouse nao passa pelo mapa de teclas (wotm_pad.inc)
    }
    g_origOverlay(rdram, ctx, runtime);
    if (g_inOverlay && ctx->pc == g_overlayRa)
        g_inOverlay = false;
}

static bool startsWith(const uint8_t *rdram, uint32_t a, const char *s)
{
    a &= 0x1FFFFFFu;
    for (; *s; ++s, ++a)
        if (a >= PS2_RAM_SIZE || rdram[a] != static_cast<uint8_t>(*s)) return false;
    return true;
}

// fontSpritePrintCenteredXY(fonte, x, y, texto) dentro do pauseLevelOverlay: a linha do esquema nao sai, o QUIT sobe para ela
static void printHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::fontSpritePrintCenteredXY__FiiiPc && g_inOverlay)
    {
        if (startsWith(rdram, GPR_U32(ctx, 7), "CONTROL"))
        {
            ctx->pc = GPR_U32(ctx, 31);   // nao desenha
            return;
        }
        if (static_cast<int>(GPR_U32(ctx, 6)) == kLineY[kQuit])
            ctx->r[6] = _mm_set_epi64x(0, kLineY[kScheme]);
    }
    if (ctx->pc == wotm::fn::fontSpritePrintCenteredXY__FiiiPc && g_inDialog && !g_inOverlay && g_mark >= 0 && g_mark < kMaxDlgItems)
    {
        int n = 0;
        for (uint32_t a = GPR_U32(ctx, 7) & 0x1FFFFFFu; n < 64 && a < PS2_RAM_SIZE && rdram[a]; ++a) ++n;
        g_dlg[g_mark] = DlgItem{true, static_cast<int>(GPR_U32(ctx, 5)), static_cast<int>(GPR_U32(ctx, 6)), n};
        g_mark = -1;
    }
    g_origPrint(rdram, ctx, runtime);
}

static void dialogHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::displayDialog__Fi)
    {
        g_dialogRa = GPR_U32(ctx, 31);
        g_inDialog = true;
    }
    g_origDialog(rdram, ctx, runtime);
    if (g_inDialog && ctx->pc == g_dialogRa)
        g_inDialog = false;
}
// setMenuFont(i): o proximo texto e o item i; setTitleFont e fontSetDefaultSize: o proximo texto e titulo
static void menuFontHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::setMenuFont__Fi) g_mark = static_cast<int>(GPR_U32(ctx, 4));
    g_origMenuFont(rdram, ctx, runtime);
}
static void titleFontHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::setTitleFont__Fv) g_mark = -1;
    g_origTitleFont(rdram, ctx, runtime);
}
static void defSizeHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::fontSetDefaultSize__Fi) g_mark = -1;
    g_origDefSize(rdram, ctx, runtime);
}

// Item do dialogo sob o mouse (-1: nenhum): a linha de topo mais proxima acima do cursor, dentro da largura do texto
static int dialogItem(int n)
{
    float gx, gy;
    if (!wotm::mouse::gsPointer(gx, gy))
        return -1;
    int best = -1;
    for (int i = 0; i < n && i < kMaxDlgItems; ++i)
    {
        const DlgItem &d = g_dlg[i];
        const float hw = std::max(40.f, float(d.chars) * 6.f + 10.f);
        if (d.ok && std::fabs(gx - float(d.x)) <= hw && gy >= float(d.y) - 2.f && gy < float(d.y) + 11.f && (best < 0 || d.y > g_dlg[best].y))
            best = i;
    }
    return best;
}

// getCurrentMenuSelection(n) num dialogo fora da pausa: hover escolhe o item, clique confirma (X por duas leituras do pad). No primeiro
// quadro de um dialogo (outro n, ou depois de uma pausa) o clique e o X pendentes caem: o clique no QUIT nao confirma o "ARE YOU SURE?"
static int dialogSel(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, int n, int sel)
{
    static int64_t s_last = 0;
    static int s_n = -1;
    static uint32_t s_seq = 0;
    const int64_t now = wotm::mouse::nowMs();
    const bool fresh = n != s_n || now - s_last > 300;
    s_n = n;
    s_last = now;
    wotm::mouse::gameMenu();
    const uint32_t seq = wotm::mouse::g_moveSeq.load(std::memory_order_relaxed);
    const bool moved = seq != s_seq;
    s_seq = seq;
    if (fresh)
    {
        wotm::mouse::takeClick(wotm::mouse::g_clickL);
        wotm::mouse::g_crossReq.store(0, std::memory_order_relaxed);
        for (DlgItem &d : g_dlg) d = DlgItem{};
        return sel;
    }
    const int h = rd32(rdram, wotm::sym::gDrawOverlay) ? dialogItem(n) : -1;
    if (h < 0)
        return sel;
    wotm::mouse::claim();
    if (moved && h != sel)
    {
        sel = h;
        wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::playPauseMenuSound__10ShellSoundi, wotm_padsel::kShellSound, 0x50);
    }
    if (wotm::mouse::takeClick(wotm::mouse::g_clickL))
    {
        sel = h;
        wotm::mouse::g_crossReq.store(2, std::memory_order_relaxed);
    }
    return sel;
}

// Item da pausa sob o mouse (-1: nenhum), pelas linhas que ficaram (o QUIT na altura do esquema)
static int hoverItem()
{
    float gx, gy;
    if (!wotm::mouse::gsPointer(gx, gy) || std::fabs(gx - 320.f) > 100.f)
        return -1;
    for (int i = 0; i < kItems; ++i)
    {
        if (i == kScheme) continue;
        const int ly = i == kQuit ? kLineY[kScheme] : kLineY[i];
        if (gy >= float(ly) - 1.f && gy < float(ly) + 8.f) return i;   // y do texto e o topo da linha (passo de 9)
    }
    return -1;
}

// getCurrentMenuSelection(5, jogador) da pausa: o cursor pula o esquema (descendo vai ao QUIT, subindo a VIBRATION) e o mouse
// escolhe o item sob ele
static void selHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::getCurrentMenuSelection__Fii &&
        ((g_inOverlay && GPR_U32(ctx, 4) == uint32_t(kItems)) || (g_inDialog && !g_inOverlay && GPR_U32(ctx, 4) - 1u < uint32_t(kMaxDlgItems))))
    {
        g_selRa = GPR_U32(ctx, 31);
        g_selPending = true;
        g_selN = g_inOverlay ? 0 : static_cast<int>(GPR_U32(ctx, 4));
        g_selPrev = static_cast<int>(rd32(rdram, wotm::sym::gCurrentMenuSelection));
    }
    g_origSel(rdram, ctx, runtime);
    if (!g_selPending || ctx->pc != g_selRa)
        return;
    g_selPending = false;
    int sel = static_cast<int>(GPR_U32(ctx, 2));
    if (g_selN > 0)   // dialogo
    {
        const int d = dialogSel(rdram, ctx, runtime, g_selN, sel);
        if (d != sel && d >= 0 && d < g_selN)
        {
            wr32(rdram, wotm::sym::gCurrentMenuSelection, static_cast<uint32_t>(d));
            ctx->r[2] = _mm_set_epi64x(0, d);
        }
        return;
    }
    if (sel == kScheme)
        sel = g_selPrev == kQuit ? kScheme - 1 : kQuit;
    // mouse: so com a lista na tela (gDrawOverlay; o "ARE YOU SURE?" do QUIT e outra caixa)
    static uint32_t s_seq = 0;
    const uint32_t seq = wotm::mouse::g_moveSeq.load(std::memory_order_relaxed);
    const bool moved = seq != s_seq;
    s_seq = seq;
    if (rd32(rdram, wotm::sym::gDrawOverlay))
    {
        const int h = hoverItem();
        if (h >= 0)
        {
            wotm::mouse::claim();   // o clique esquerdo e deste item (nao um X solto)
            if (moved && h != sel)
            {
                sel = h;
                wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::playPauseMenuSound__10ShellSoundi, wotm_padsel::kShellSound, 0x50);
            }
            if (wotm::mouse::takeClick(wotm::mouse::g_clickL))
            {
                sel = h;
                wotm::mouse::g_crossReq.store(2, std::memory_order_relaxed);
            }
        }
    }
    wr32(rdram, wotm::sym::gCurrentMenuSelection, static_cast<uint32_t>(sel));
    ctx->r[2] = _mm_set_epi64x(0, sel);
}

static void install(PS2Runtime &runtime)
{
    struct H { uint32_t addr; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; };
    const H hooks[] = {
        {wotm::fn::pauseLevelOverlay__FRi, &g_origOverlay, &overlayHook},
        {wotm::fn::fontSpritePrintCenteredXY__FiiiPc, &g_origPrint, &printHook},
        {wotm::fn::getCurrentMenuSelection__Fii, &g_origSel, &selHook},
        {wotm::fn::displayDialog__Fi, &g_origDialog, &dialogHook},
        {wotm::fn::setMenuFont__Fi, &g_origMenuFont, &menuFontHook},
        {wotm::fn::setTitleFont__Fv, &g_origTitleFont, &titleFontHook},
        {wotm::fn::fontSetDefaultSize__Fi, &g_origDefSize, &defSizeHook},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
    std::fprintf(stderr, "[pausa] menu de pausa sem o CONTROL SCHEME (tipo A fixo); mouse na pausa e nos dialogos\n");
}
} // namespace wotm_pausemenu
