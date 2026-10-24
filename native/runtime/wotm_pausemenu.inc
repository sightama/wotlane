// wotm_pausemenu.inc: menu de pausa da luta (pauseLevelOverlay), incluido por wotm_live.inc depois do wotm_mainmenu.inc.
//  - A linha do CONTROL SCHEME (CONTROL TYPE A/B/C) virou RECOMPILED OPTIONS: o X nela (ou o clique) abre o painel de opcoes do port
//    (wotm_mainmenu.inc) no meio da luta. O esquema nao muda mais (esquerda/direita nessa linha nao chegam ao jogo): o teclado, o
//    lock-on alternado e a troca de alvo supoem o tipo A, e com 3 ou 4 jogadores a escolha gravava fora do m_ctlrCfg[2] (no
//    m_vibration).
//  - Com o painel aberto o pauseLevelOverlay nao roda: o gancho dele roda o painel (wotm_mainmenu::pauseFrame) e devolve 0 ("continua
//    na pausa"). O jogo nao le o pad nem desenha a lista; a luta continua desenhada atras do painel (2D, na janela). Depois de fechar,
//    a entrada do jogo na pausa fica bloqueada ate soltarem o que fechou.
//  - Com o modo treino ligado (wotm_training.h), um sexto item, TRAINING OPTIONS, antes do QUIT: o getCurrentMenuSelection passa a
//    contar 6 e o cursor (gCurrentMenuSelection) anda pelas linhas da tela; o jogo recebe o item logico (linha 4 = item 5, que o
//    retail ignora; linha 5 = item 4, o QUIT). O QUIT do jogo (setMenuFont(4)) sai na linha 5 e o TRAINING OPTIONS na 4, pelas
//    funcoes de fonte do jogo na saida do overlay; a lista sobe meia linha e a caixa fica 18% mais alta. O X nele abre o painel do
//    treino (wotm_mainmenu.inc, kTrainDefs). O RESTART vira RESET POSITIONS e confirma sozinho o ARE YOU SURE (o "sim" do
//    areYouSure, refeito aqui): o rtMain sai, o TheGame::ResetLevel e o rtMain(0) poem monstros e fase como no comeco, sem recarregar.
//  - Mouse (etapa 9 da camera): passar por cima de um item escolhe, clique esquerdo confirma (X), direito e rodinha pelo wotm_pad.inc.
//    O mesmo nos dialogos do displayDialog (ARE YOU SURE?, fins de luta, minigames), pelos itens que o setMenuFont marca.
// O texto da pausa e do jogo (fontSpritePrintCenteredXY, unidades da tela do PS2: 640 x 224, x 0x140 no centro).
namespace wotm_pausemenu
{
using wotm_padsel::rd32;
using wotm_padsel::wr32;
constexpr int kItems = 5;                       // CONTINUE, RESTART, VIBRATION, RECOMPILED OPTIONS (o CONTROL SCHEME), QUIT
constexpr int kOptions = 3;
constexpr int kLineY[kItems + 1] = {0x55, 0x5E, 0x67, 0x70, 0x79, 0x82};   // y de cada linha no pauseLevelOverlay (+ a do treino)
constexpr uint32_t kTrainText = PS2_RAM_SIZE - 0x380;   // "TRAINING OPTIONS" para o fontSpritePrintCenteredXY (as outras vagas do topo: -0x800..-0x3E0)
constexpr uint32_t kResetText = PS2_RAM_SIZE - 0x360;   // "RESET POSITIONS" no lugar do RESTART
static int items() { return wotm_training::g_on.load(std::memory_order_relaxed) ? kItems + 1 : kItems; }
static bool training() { return items() > kItems; }
constexpr int kRestart = 1, kQuit = 4, kTrainRow = 4, kQuitRow = 5;   // com o treino: linha 4 = TRAINING OPTIONS, 5 = QUIT
constexpr int kTrainShift = 4;   // a lista de 6 sobe meia linha (o centro da caixa nao muda)
static int logicalItem(int row) { return training() && (row == kTrainRow || row == kQuitRow) ? kTrainRow + kQuitRow - row : row; }
static int lineY(int row) { return kLineY[row] - (training() ? kTrainShift : 0); }
static bool g_ownDraw = false, g_quitNext = false, g_autoYes = false;
constexpr uint32_t kCross = 0x4000, kStart = 0x08, kLeft = 0x80, kRight = 0x20;   // mascaras do inputGetInput
static PS2Runtime::RecompiledFunction g_origOverlay = nullptr, g_origPrint = nullptr, g_origSel = nullptr, g_origDialog = nullptr,
                                      g_origMenuFont = nullptr, g_origTitleFont = nullptr, g_origDefSize = nullptr, g_origInput = nullptr,
                                      g_origExit = nullptr;
static uint32_t g_overlayRa = 0, g_selRa = 0, g_dialogRa = 0;
static bool g_inOverlay = false, g_selPending = false, g_inDialog = false;
static int g_selPrev = 0, g_selN = 0;
// Dentro do checkInputForExit da pausa: o que as leituras do X e do Start deram (o bool "foi Start" dele fica na pilha, no scratchpad)
static bool g_inExit = false, g_sawCross = false, g_sawStart = false;

// Diálogos do displayDialog (ARE YOU SURE?, fins de luta, minigames): cada item sai como setMenuFont(i) e depois
// fontSpritePrintCenteredXY(0, x, y, texto); a tabela guarda onde cada item foi escrito no ultimo quadro desenhado
constexpr int kMaxDlgItems = 8;
struct DlgItem { bool ok; int x, y, chars; };
static DlgItem g_dlg[kMaxDlgItems] = {};
static int g_mark = -1;   // o proximo texto e o item g_mark (-1: titulo ou outro texto)

// Caixa da lista da pausa desenhada pelo nativo, do tamanho da lista (5 ou 6 itens): a do jogo (shell->m_cs610, um modelo 3D posto
// na frente da camera pelo overlay) tem altura fixa para 5 linhas e era estreita para o RECOMPILED OPTIONS (~267 unidades da tela do
// PS2 contra ~213). No laco do displayDialog o hier desenha antes do overlay: escondida na saida do overlay (hierSetCsDrawMe 0), ela
// some do quadro seguinte (o ARE YOU SURE religa a dele). A janela desenha a nativa sob o texto enquanto o overlay roda (g_boxMs) e o
// texto da pausa esta no quadro. Medidas da do jogo num print 720p: x 168..472, y do topo da 1a linha - 3,5 ao topo da ultima + 10,5.
static std::atomic<int64_t> g_boxMs{-100000};
static std::atomic<int> g_boxRows{kItems};
static void drawBox2d(int rw, int rh, bool hasText)
{
    if (!hasText || wotm_training::nowMs() - g_boxMs.load(std::memory_order_relaxed) > 150)
        return;
    const int n = g_boxRows.load(std::memory_order_relaxed);
    const int shift = n > kItems ? kTrainShift : 0;
    const float top = float(kLineY[0] - shift) - 3.5f, bot = float(kLineY[n - 1] - shift) + 10.5f;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const float sx = gr.w / 640.f, sy = float(rh) / 224.f;
    const Rectangle r{gr.x0 + 168.f * sx, top * sy, 304.f * sx, (bot - top) * sy};
    const float rad = float(rh) / 80.f, round = std::min(1.f, 2.f * rad / std::min(r.width, r.height));
    DrawRectangleRounded(r, round, 8, Color{0, 0, 0, 120});
    DrawRectangleRoundedLinesEx(r, round, 8, std::max(2.f, float(rh) / 360.f), Color{175, 175, 175, 215});
}

static void overlayHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::pauseLevelOverlay__FRi)
    {
        g_overlayRa = GPR_U32(ctx, 31);
        g_inOverlay = true;
        wotm_training::g_dialogMs = wotm_training::nowMs();   // a cada quadro da pausa: as janelas do treino somem
        wotm::mouse::gameMenu();   // pausa: o mouse nao passa pelo mapa de teclas (wotm_pad.inc)
        if (wotm_mainmenu::pauseFrame(rdram, ctx, runtime))
        {
            // painel aberto: a original nao roda (nenhuma leitura do pad, lista escondida); 0 = o displayDialog continua na pausa
            g_inOverlay = false;
            ctx->r[2] = _mm_set_epi64x(0, 0);
            ctx->pc = g_overlayRa;
            return;
        }
    }
    g_origOverlay(rdram, ctx, runtime);
    if (g_inOverlay && ctx->pc == g_overlayRa)
    {
        if (training() && rd32(rdram, wotm::sym::gDrawOverlay))
        {   // o sexto item na linha 4, com a fonte do menu (setMenuFont acende a linha escolhida), ainda dentro do overlay (nao vira
            // item de dialogo); o printHook sobe a linha como as outras
            static constexpr char kText[] = "TRAINING OPTIONS";
            std::memcpy(rdram + kTrainText, kText, sizeof kText);
            const uint32_t ret = GPR_U32(ctx, 2);
            g_ownDraw = true;
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::setMenuFont__Fi, uint32_t(kTrainRow));
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::fontSpritePrintCenteredXY__FiiiPc, 0u, 0x140u, uint32_t(kLineY[kTrainRow]),
                                   kTrainText);
            g_ownDraw = false;
            ctx->r[2] = _mm_set_epi64x(0, static_cast<int32_t>(ret));
        }
        g_quitNext = false;
        g_inOverlay = false;
        if (rd32(rdram, wotm::sym::gDrawOverlay) && ctx->pc == g_overlayRa)
        {
            const uint32_t ret = GPR_U32(ctx, 2);
            if (const uint32_t cs = rd32(rdram, wotm::sym::shellInfo + 0x610))
                wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::hierSetCsDrawMe__FP3_csUc, cs, 0u);
            ctx->r[2] = _mm_set_epi64x(0, static_cast<int32_t>(ret));
            g_boxRows = items();
            g_boxMs = wotm_training::nowMs();
        }
    }
}

static bool startsWith(const uint8_t *rdram, uint32_t a, const char *s)
{
    a &= 0x1FFFFFFu;
    for (; *s; ++s, ++a)
        if (a >= PS2_RAM_SIZE || rdram[a] != static_cast<uint8_t>(*s)) return false;
    return true;
}

// fontSpritePrintCenteredXY(fonte, x, y, texto) dentro do pauseLevelOverlay: a linha do esquema sai como RECOMPILED OPTIONS. O texto
// dela e um buffer de 24 bytes do rodata que o jogo reescreve com sprintf a cada quadro ("CONTROL TYPE A"): escrever no proprio buffer
// (19 bytes com o NUL) dispensa o endereco dele.
static void printHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::fontSpritePrintCenteredXY__FiiiPc && g_inOverlay && startsWith(rdram, GPR_U32(ctx, 7), "CONTROL"))
    {
        static constexpr char kText[] = "RECOMPILED OPTIONS";
        static_assert(sizeof kText <= 24);
        const uint32_t raw = GPR_U32(ctx, 7), a = raw & 0x1FFFFFFu;
        if ((raw >> 28) != 7u && a + sizeof kText <= PS2_RAM_SIZE)   // nunca no scratchpad (0x70000000, fora do rdram)
            std::memcpy(rdram + a, kText, sizeof kText);
    }
    if (ctx->pc == wotm::fn::fontSpritePrintCenteredXY__FiiiPc && g_inOverlay && training())
    {
        if (startsWith(rdram, GPR_U32(ctx, 7), "RESTART"))
        {
            static constexpr char kText[] = "RESET POSITIONS";
            std::memcpy(rdram + kResetText, kText, sizeof kText);
            ctx->r[7] = _mm_set_epi64x(0, kResetText);
        }
        int y = static_cast<int>(GPR_U32(ctx, 6));
        if (g_quitNext)   // o QUIT do jogo desce para a linha 5
        {
            y = kLineY[kQuitRow];
            g_quitNext = false;
        }
        ctx->r[6] = _mm_set_epi64x(0, y - kTrainShift);
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
        wotm_training::g_dialogMs = wotm_training::nowMs();   // pausa (ou dialogo de fim): as janelas do treino somem
        g_dialogRa = GPR_U32(ctx, 31);
        g_inDialog = true;
    }
    g_origDialog(rdram, ctx, runtime);
    if (g_inDialog && ctx->pc == g_dialogRa)
    {
        g_inDialog = false;
        wotm_mainmenu::pauseDialogEnd();   // o painel da pausa (se ficou aberto) some com ela
    }
}
// setMenuFont(i): o proximo texto e o item i; setTitleFont e fontSetDefaultSize: o proximo texto e titulo
static void menuFontHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::setMenuFont__Fi && g_inOverlay && !g_ownDraw && training() && GPR_U32(ctx, 4) == uint32_t(kQuit))
    {   // o QUIT do jogo acende na linha 5 (o cursor anda pelas linhas da tela)
        ctx->r[4] = _mm_set_epi64x(0, kQuitRow);
        g_quitNext = true;
    }
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
        wotm::mouse::requestCross(2);
    }
    return sel;
}

// Item da pausa sob o mouse (-1: nenhum). Meia largura de 130: o RECOMPILED OPTIONS tem 18 letras
static int hoverItem()
{
    float gx, gy;
    if (!wotm::mouse::gsPointer(gx, gy) || std::fabs(gx - 320.f) > 130.f)
        return -1;
    for (int i = 0; i < items(); ++i)
        if (gy >= float(lineY(i)) - 1.f && gy < float(lineY(i)) + 8.f) return i;   // y do texto e o topo da linha (passo de 9)
    return -1;
}

// getCurrentMenuSelection(5, jogador) da pausa: o mouse escolhe o item sob ele. Logo depois de fechar o painel, o cursor nao anda
static void selHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    // com o treino, a lista da pausa tem 6 itens (o cursor anda e da a volta ate o TRAINING OPTIONS)
    if (ctx->pc == wotm::fn::getCurrentMenuSelection__Fii && g_inOverlay && GPR_U32(ctx, 4) == uint32_t(kItems) && items() > kItems)
        ctx->r[4] = _mm_set_epi64x(0, items());
    if (ctx->pc == wotm::fn::getCurrentMenuSelection__Fii && g_inOverlay && GPR_U32(ctx, 4) == uint32_t(items()) && wotm_mainmenu::pauseBlocked())
    {
        ctx->r[2] = _mm_set_epi64x(0, logicalItem(static_cast<int>(rd32(rdram, wotm::sym::gCurrentMenuSelection))));
        ctx->pc = GPR_U32(ctx, 31);
        return;
    }
    if (ctx->pc == wotm::fn::getCurrentMenuSelection__Fii &&
        ((g_inOverlay && GPR_U32(ctx, 4) == uint32_t(items())) || (g_inDialog && !g_inOverlay && GPR_U32(ctx, 4) - 1u < uint32_t(kMaxDlgItems))))
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
                wotm::mouse::requestCross(2);
            }
        }
    }
    wr32(rdram, wotm::sym::gCurrentMenuSelection, static_cast<uint32_t>(sel));   // a linha da tela (o setMenuFont acende por ela)
    ctx->r[2] = _mm_set_epi64x(0, logicalItem(sel));                            // o item que o jogo decide
}

// inputGetInput(mascara, jogador) dentro do pauseLevelOverlay:
//  - esquerda/direita no RECOMPILED OPTIONS dao 0 (o jogo trocaria o m_ctlrCfg e o Monster+0x672C); so essas mascaras: o
//    checkInputForExit le o X (0x4000) e o Start (0x08) pela mesma funcao;
//  - logo depois de fechar o painel, tudo da 0;
//  - dentro do checkInputForExit, anota se o X e o Start deram verdade (g_sawCross, g_sawStart).
static void inputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::inputGetInput__Fii || !g_inOverlay)
    {
        g_origInput(rdram, ctx, runtime);
        return;
    }
    const uint32_t mask = GPR_U32(ctx, 4), ra = GPR_U32(ctx, 31);
    const bool lr = (mask == kLeft || mask == kRight) && rd32(rdram, wotm::sym::gCurrentMenuSelection) == uint32_t(kOptions);
    if (lr || wotm_mainmenu::pauseBlocked())
    {
        ctx->r[2] = _mm_set_epi64x(0, 0);
        ctx->pc = ra;
        return;
    }
    const bool track = g_inExit && (mask == kCross || mask == kStart);
    g_origInput(rdram, ctx, runtime);
    if (track && ctx->pc == ra && GPR_U32(ctx, 2))
        (mask == kCross ? g_sawCross : g_sawStart) = true;
}

// checkInputForExit(jogador, &start) dentro do pauseLevelOverlay: o X (nao o Start) no RECOMPILED OPTIONS abre o painel e devolve 0
// (no retail o X nessa linha nao fazia nada). O Start continua tirando da pausa, como no retail. Logo depois de fechar o painel, 0.
static void exitHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::checkInputForExit__FiPb || !g_inOverlay)
    {
        g_origExit(rdram, ctx, runtime);
        return;
    }
    const uint32_t ra = GPR_U32(ctx, 31);
    if (wotm_mainmenu::pauseBlocked())
    {
        ctx->r[2] = _mm_set_epi64x(0, 0);
        ctx->pc = ra;
        return;
    }
    g_inExit = true;
    g_sawCross = g_sawStart = false;
    g_origExit(rdram, ctx, runtime);
    g_inExit = false;
    const uint32_t sel = rd32(rdram, wotm::sym::gCurrentMenuSelection);
    const bool cross = ctx->pc == ra && GPR_U32(ctx, 2) && g_sawCross && !g_sawStart;
    if (cross && (sel == uint32_t(kOptions) || (sel == uint32_t(kTrainRow) && training())))
    {
        wotm_mainmenu::openPausePanel(rdram, ctx, runtime, sel == uint32_t(kTrainRow));
        ctx->r[2] = _mm_set_epi64x(0, 0);
    }
    else if (cross && sel == uint32_t(kRestart) && training())
        g_autoYes = true;   // RESET POSITIONS: o jogo segue para o ARE YOU SURE, que o sureHook responde
}

// areYouSure (o ARE YOU SURE do RESTART e do QUIT):
//  - depois do RESET POSITIONS: nao desenha nem le o pad; faz o que o "sim" faz (asm game/ShellFinished, areYouSure, s2 == 0) e
//    devolve 1: o rtMain sai com o m_restart que o overlay ja pos;
//  - o "nao" volta o cursor ao item guardado (gLastMenuSelection, o logico): com o treino, o QUIT (item 4) esta na linha 5;
//  - qualquer "sim" esconde as janelas do treino na hora (wotm_training::g_inFight).
static PS2Runtime::RecompiledFunction g_origSure = nullptr;
static void sureHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::areYouSure__FRi)
    {
        g_origSure(rdram, ctx, runtime);
        return;
    }
    const uint32_t ra = GPR_U32(ctx, 31);
    if (training() && rd32(rdram, wotm::sym::gLastMenuSelection) == uint32_t(kQuit))
        wr32(rdram, wotm::sym::gLastMenuSelection, kQuitRow);
    if (g_autoYes)
    {
        g_autoYes = false;
        constexpr uint32_t sh = wotm::sym::shellInfo;
        wr32(rdram, sh + 0x2BA4, 0);
        const uint32_t cont = rd32(rdram, sh + 0x2B48);
        wr32(rdram, sh + 0x2B44, cont);
        wr32(rdram, sh + 0x2B40, cont);
        if (!rd32(rdram, wotm::sym::shellGame + 0x1204C0 + 0x109C))
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::snd_StopAllSounds, 0u);
        wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::playPauseMenuSound__10ShellSoundi, wotm_padsel::kShellSound, 0x4Fu);
        wr32(rdram, wotm::sym::gExitDialog, 0);
        wotm_training::g_inFight = false;
        std::fprintf(stderr, "[treino] reset positions (restart da luta sem o ARE YOU SURE)\n");
        ctx->r[2] = _mm_set_epi64x(0, 1);
        ctx->pc = ra;
        return;
    }
    g_origSure(rdram, ctx, runtime);
    if (ctx->pc == ra && GPR_U32(ctx, 2))
    {
        wotm_training::g_inFight = false;
        wotm_training::restoreDifficulty(rdram, wotm::sym::shellGame + wotm_training::kDifficulty);   // QUIT: a do jogo volta (o save nao a ve)
    }
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
        {wotm::fn::inputGetInput__Fii, &g_origInput, &inputHook},
        {wotm::fn::checkInputForExit__FiPb, &g_origExit, &exitHook},
        {wotm::fn::areYouSure__FRi, &g_origSure, &sureHook},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
    std::fprintf(stderr, "[pausa] menu de pausa com RECOMPILED OPTIONS no lugar do CONTROL SCHEME (tipo A fixo); mouse na pausa e nos dialogos\n");
}
} // namespace wotm_pausemenu
