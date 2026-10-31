// wotm_pausemenu.inc: the fight's pause menu (pauseLevelOverlay), included by wotm_live.inc after wotm_mainmenu.inc.
//  - Screen rows: CONTINUE, RESTART, RECOMPILED OPTIONS, QUIT. The game has 5 items (CONTINUE, RESTART, VIBRATION, CONTROL SCHEME,
//    QUIT): getCurrentMenuSelection counts the screen rows, the cursor (gCurrentMenuSelection) moves through them and the game gets
//    the logical item (logicalItem). Each list text is drawn as setMenuFont(item) + fontSpritePrintCenteredXY: setMenuFont gets the
//    row (it highlights by the cursor's row), the print lands at the row's y and VIBRATION's is not drawn (vibration is the
//    VIBRATION option in the panel's CONTROLS tab, wotm_rumble.inc). The list stays centered where the game's list was.
//  - The CONTROL SCHEME row (CONTROL TYPE A/B/C) became RECOMPILED OPTIONS: X on it (or a click) opens the port's options panel
//    (wotm_mainmenu.inc) mid-fight. The scheme no longer changes (left/right on that row do not reach the game): the keyboard,
//    toggle lock-on and target switching assume type A. Monster::playerInit reads the scheme from m_ctlrCfg[player], which only
//    has 2 slots: players 3 and 4 read m_vibration (0 = scheme -1). Its hook sets type A on everyone (playerInitHook).
//  - With the panel open pauseLevelOverlay does not run: its hook runs the panel (wotm_mainmenu::pauseFrame) and returns 0 ("stay
//    paused"). The game neither reads the pad nor draws the list; the fight stays drawn behind the panel (2D, in the window). After
//    closing, the game's pause input stays blocked until whatever closed the panel is released.
//  - With training mode on (wotm_training.h), TRAINING OPTIONS and MOVE LIST come before QUIT (logical items 5 and 6, which retail
//    ignores), drawn here on the overlay's exit with the game's font functions. X on them opens the training panel
//    (wotm_mainmenu.inc, kTrainDefs) or the move list. RESTART becomes RESET POSITIONS and answers the ARE YOU SURE by itself (the
//    areYouSure "yes", redone here): rtMain exits, and TheGame::ResetLevel and rtMain(0) put monsters and stage back as at the
//    start, without reloading.
//  - Mouse (camera step 9): hovering an item selects it, left click confirms (X), right click and wheel go through wotm_pad.inc.
//    The same applies to displayDialog's dialogs (ARE YOU SURE?, end of fight, minigames), through the items setMenuFont marks.
// The pause text is the game's (fontSpritePrintCenteredXY, PS2 screen units: 640 x 224, x 0x140 at the center).
namespace wotm_pausemenu
{
using wotm_padsel::rd32;
using wotm_padsel::wr32;
constexpr int kItems = 5;   // the game's items: CONTINUE, RESTART, VIBRATION, CONTROL SCHEME (RECOMPILED OPTIONS), QUIT
constexpr int kRestart = 1, kVibration = 2, kOptions = 3, kQuit = 4, kTrainItem = 5, kMoveItem = 6;   // logical items
constexpr int kOptRow = 2, kTrainRow = 3, kMoveRow = 4;   // screen rows (QUIT is the last one: quitRow)
constexpr int kLine0 = 0x55, kLineStep = 9;               // y of the first row of the game's 5-item list, and the step
constexpr uint32_t kTrainText = PS2_RAM_SIZE - 0x380;   // "TRAINING OPTIONS" for fontSpritePrintCenteredXY (the other slots at the top: -0x800..-0x3E0)
constexpr uint32_t kResetText = PS2_RAM_SIZE - 0x360;   // "RESET POSITIONS" in place of RESTART
constexpr uint32_t kMoveText = PS2_RAM_SIZE - 0x3A0;    // "MOVE LIST" (between the _fvector at -0x3E0 and TRAINING OPTIONS)
static bool training() { return wotm_training::g_on.load(std::memory_order_relaxed); }
static int items() { return training() ? 6 : 4; }
static int quitRow() { return items() - 1; }
// screen row -> game item
static int logicalItem(int row)
{
    if (row == kOptRow) return kOptions;
    if (row == quitRow()) return kQuit;
    if (training() && row == kTrainRow) return kTrainItem;
    if (training() && row == kMoveRow) return kMoveItem;
    return row;
}
// game item -> screen row (-1: VIBRATION, not in the list)
static int rowOf(int item)
{
    if (item == kVibration) return -1;
    if (item == kOptions) return kOptRow;
    if (item == kQuit) return quitRow();
    return item;
}
// y of the row in a list of n, centered on the game's 5-item list
static int lineY(int row, int n) { return kLine0 + kLineStep * row + (kItems - n) * kLineStep / 2; }
static int lineY(int row) { return lineY(row, items()); }
static bool g_ownDraw = false, g_autoYes = false;
static int g_pauseItem = -1;   // the list's next text is this game item (setMenuFont inside the overlay)
constexpr uint32_t kCross = 0x4000, kStart = 0x08, kLeft = 0x80, kRight = 0x20;   // inputGetInput masks
static PS2Runtime::RecompiledFunction g_origOverlay = nullptr, g_origPrint = nullptr, g_origSel = nullptr, g_origDialog = nullptr,
                                      g_origMenuFont = nullptr, g_origTitleFont = nullptr, g_origDefSize = nullptr, g_origInput = nullptr,
                                      g_origExit = nullptr;
static uint32_t g_overlayRa = 0, g_selRa = 0, g_dialogRa = 0;
static bool g_inOverlay = false, g_selPending = false, g_inDialog = false;
static int g_selPrev = 0, g_selN = 0;
// Inside the pause's checkInputForExit: what the X and Start reads returned (its "was Start" bool lives on the stack, in the scratchpad)
static bool g_inExit = false, g_sawCross = false, g_sawStart = false;

// displayDialog's dialogs (ARE YOU SURE?, end of fight, minigames): each item is drawn as setMenuFont(i) and then
// fontSpritePrintCenteredXY(0, x, y, text); the table keeps where each item was written in the last drawn frame
constexpr int kMaxDlgItems = 8;
struct DlgItem { bool ok; int x, y, chars; };
static DlgItem g_dlg[kMaxDlgItems] = {};
static int g_mark = -1;   // the next text is item g_mark (-1: title or other text)

// Pause list box drawn natively, sized to the list (4 or 6 items): the game's box (shell->m_cs610, a 3D model placed in front of the
// camera by the overlay) has a fixed height for 5 rows and was too narrow for RECOMPILED OPTIONS (~267 PS2 screen units against
// ~213). In displayDialog's loop the hier draws before the overlay: hidden on the overlay's exit (hierSetCsDrawMe 0), it disappears
// from the next frame (the ARE YOU SURE turns its own back on). The window draws the native box under the text while the overlay
// runs (g_boxMs) and the pause text is in the frame. The game's box measured on a 720p screenshot: x 168..472, y from the first
// row's top - 3.5 to the last row's top + 10.5.
static std::atomic<int64_t> g_boxMs{-100000};
static std::atomic<int> g_boxRows{kItems};
static void drawBox2d(int rw, int rh, bool hasText)
{
    if (!hasText || wotm_training::nowMs() - g_boxMs.load(std::memory_order_relaxed) > 150)
        return;
    const int n = g_boxRows.load(std::memory_order_relaxed);
    const float top = float(lineY(0, n)) - 3.5f, bot = float(lineY(n - 1, n)) + 10.5f;
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
        wotm_training::g_dialogMs = wotm_training::nowMs();   // every pause frame: the training windows disappear
        wotm::mouse::gameMenu();   // pause: the mouse does not go through the key map (wotm_pad.inc)
        if (wotm_mainmenu::pauseFrame(rdram, ctx, runtime))
        {
            // panel open: the original does not run (no pad read, list hidden); 0 = displayDialog stays paused
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
        {   // the training rows, in the menu font (setMenuFont highlights the selected row), still inside the overlay (they do not
            // become dialog items); g_ownDraw: the font and text hooks leave them alone
            static constexpr char kText[] = "TRAINING OPTIONS", kMove[] = "MOVE LIST";
            std::memcpy(rdram + kTrainText, kText, sizeof kText);
            std::memcpy(rdram + kMoveText, kMove, sizeof kMove);
            const uint32_t ret = GPR_U32(ctx, 2);
            g_ownDraw = true;
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::setMenuFont__Fi, uint32_t(kTrainRow));
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::fontSpritePrintCenteredXY__FiiiPc, 0u, 0x140u, uint32_t(lineY(kTrainRow)),
                                   kTrainText);
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::setMenuFont__Fi, uint32_t(kMoveRow));
            wotm_padsel::callGuest(rdram, ctx, runtime, wotm::fn::fontSpritePrintCenteredXY__FiiiPc, 0u, 0x140u, uint32_t(lineY(kMoveRow)),
                                   kMoveText);
            g_ownDraw = false;
            ctx->r[2] = _mm_set_epi64x(0, static_cast<int32_t>(ret));
        }
        g_pauseItem = -1;
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

// fontSpritePrintCenteredXY(font, x, y, text) inside pauseLevelOverlay: the scheme row is drawn as RECOMPILED OPTIONS. Its text is a
// 24-byte rodata buffer that the game rewrites with sprintf every frame ("CONTROL TYPE A"): writing into that same buffer (19 bytes
// with the NUL) means we don't need its address.
static void printHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::fontSpritePrintCenteredXY__FiiiPc && g_inOverlay && startsWith(rdram, GPR_U32(ctx, 7), "CONTROL"))
    {
        static constexpr char kText[] = "RECOMPILED OPTIONS";
        static_assert(sizeof kText <= 24);
        const uint32_t raw = GPR_U32(ctx, 7), a = raw & 0x1FFFFFFu;
        if ((raw >> 28) != 7u && a + sizeof kText <= PS2_RAM_SIZE)   // never in the scratchpad (0x70000000, outside rdram)
            std::memcpy(rdram + a, kText, sizeof kText);
    }
    if (ctx->pc == wotm::fn::fontSpritePrintCenteredXY__FiiiPc && g_inOverlay && !g_ownDraw && g_pauseItem >= 0)
    {
        const int row = rowOf(g_pauseItem);
        g_pauseItem = -1;
        if (row < 0)   // VIBRATION: not in the list
        {
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if (training() && startsWith(rdram, GPR_U32(ctx, 7), "RESTART"))
        {
            static constexpr char kText[] = "RESET POSITIONS";
            std::memcpy(rdram + kResetText, kText, sizeof kText);
            ctx->r[7] = _mm_set_epi64x(0, kResetText);
        }
        ctx->r[6] = _mm_set_epi64x(0, lineY(row));
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
        wotm_training::g_dialogMs = wotm_training::nowMs();   // pause (or end dialog): the training windows disappear
        g_dialogRa = GPR_U32(ctx, 31);
        g_inDialog = true;
        std::fprintf(stderr, "[dialog] %u opened\n", GPR_U32(ctx, 4));
    }
    g_origDialog(rdram, ctx, runtime);
    if (g_inDialog && ctx->pc == g_dialogRa)
    {
        g_inDialog = false;
        wotm_mainmenu::pauseDialogEnd();   // the pause panel (if it was left open) goes away with it
    }
}
// setMenuFont(i): the next text is item i; setTitleFont and fontSetDefaultSize: the next text is a title
static void menuFontHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::setMenuFont__Fi && g_inOverlay && !g_ownDraw)
    {   // an item of the game's list: highlight by screen row (the cursor moves through rows); printHook puts it on its row
        g_pauseItem = static_cast<int>(GPR_U32(ctx, 4));
        if (const int row = rowOf(g_pauseItem); row >= 0) ctx->r[4] = _mm_set_epi64x(0, row);
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

// Dialog item under the mouse (-1: none): the closest row top above the cursor, within the text's width
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

// getCurrentMenuSelection(n) in a dialog outside the pause: hover selects the item, click confirms (X for two pad reads). On a
// dialog's first frame (different n, or after a pause) pending clicks and X are dropped: the click on QUIT does not confirm "ARE YOU SURE?"
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

// Pause item under the mouse (-1: none). Half-width of 130: RECOMPILED OPTIONS has 18 letters
static int hoverItem()
{
    float gx, gy;
    if (!wotm::mouse::gsPointer(gx, gy) || std::fabs(gx - 320.f) > 130.f)
        return -1;
    for (int i = 0; i < items(); ++i)
        if (gy >= float(lineY(i)) - 1.f && gy < float(lineY(i)) + 8.f) return i;   // the text's y is the row's top (step of 9)
    return -1;
}

// The pause's getCurrentMenuSelection(5, player): the mouse selects the item under it. Right after the panel closes, the cursor does not move
static void selHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    // the screen list has 4 rows (6 with training): the cursor moves through them and wraps around
    if (ctx->pc == wotm::fn::getCurrentMenuSelection__Fii && g_inOverlay && GPR_U32(ctx, 4) == uint32_t(kItems))
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
    // network match: the item comes only from the pad (the mouse belongs to this PC, outside the input exchanged over the network)
    const bool net = wotm::settings::g_netProfile.load(std::memory_order_relaxed);
    if (g_selN > 0 && net)
        return;
    if (g_selN > 0)   // dialog
    {
        const int d = dialogSel(rdram, ctx, runtime, g_selN, sel);
        if (d != sel && d >= 0 && d < g_selN)
        {
            wr32(rdram, wotm::sym::gCurrentMenuSelection, static_cast<uint32_t>(d));
            ctx->r[2] = _mm_set_epi64x(0, d);
        }
        return;
    }
    // mouse: only with the list on screen (gDrawOverlay; QUIT's "ARE YOU SURE?" is a different box)
    static uint32_t s_seq = 0;
    const uint32_t seq = wotm::mouse::g_moveSeq.load(std::memory_order_relaxed);
    const bool moved = seq != s_seq;
    s_seq = seq;
    if (rd32(rdram, wotm::sym::gDrawOverlay) && !net)
    {
        const int h = hoverItem();
        if (h >= 0)
        {
            wotm::mouse::claim();   // the left click belongs to this item (not a stray X)
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
    wr32(rdram, wotm::sym::gCurrentMenuSelection, static_cast<uint32_t>(sel));   // the screen row (setMenuFont highlights by it)
    ctx->r[2] = _mm_set_epi64x(0, logicalItem(sel));                            // the item the game acts on
}

// inputGetInput(mask, player) inside pauseLevelOverlay:
//  - left/right on RECOMPILED OPTIONS return 0 (the game would change m_ctlrCfg and Monster+0x672C); only those masks:
//    checkInputForExit reads X (0x4000) and Start (0x08) through the same function;
//  - right after the panel closes, everything returns 0;
//  - inside checkInputForExit, records whether X and Start came back true (g_sawCross, g_sawStart).
static void inputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::inputGetInput__Fii || !g_inOverlay)
    {
        g_origInput(rdram, ctx, runtime);
        return;
    }
    const uint32_t mask = GPR_U32(ctx, 4), ra = GPR_U32(ctx, 31);
    const bool lr = (mask == kLeft || mask == kRight) && rd32(rdram, wotm::sym::gCurrentMenuSelection) == uint32_t(kOptRow);
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

// checkInputForExit(player, &start) inside pauseLevelOverlay: X (not Start) on RECOMPILED OPTIONS opens the panel and returns 0
// (in retail X on that row did nothing). Start still unpauses, as in retail. Right after the panel closes, 0.
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
    // network match: the panel reads this PC's keyboard and mouse (outside the input exchanged over the network) and each PC would
    // close it on a different frame; the row does nothing and the pause stays the same on every PC
    if (cross && sel == uint32_t(kOptRow) && wotm::settings::g_netProfile.load(std::memory_order_relaxed))
    {
        std::fprintf(stderr, "[pause] panel unavailable in a network match\n");
        ctx->r[2] = _mm_set_epi64x(0, 0);
    }
    else if (cross && (sel == uint32_t(kOptRow) || ((sel == uint32_t(kTrainRow) || sel == uint32_t(kMoveRow)) && training())))
    {
        wotm_mainmenu::openPausePanel(rdram, ctx, runtime, sel == uint32_t(kTrainRow), sel == uint32_t(kMoveRow));
        ctx->r[2] = _mm_set_epi64x(0, 0);
    }
    else if (cross && sel == uint32_t(kRestart) && training())
        g_autoYes = true;   // RESET POSITIONS: the game moves on to ARE YOU SURE, which sureHook answers
}

// areYouSure (the ARE YOU SURE of RESTART and QUIT):
//  - after RESET POSITIONS: draws nothing and does not read the pad; does what "yes" does (asm game/ShellFinished, areYouSure,
//    s2 == 0) and returns 1: rtMain exits with the m_restart the overlay already set;
//  - "no" puts the cursor back on the saved item (gLastMenuSelection, the logical one): QUIT (item 4) is on the last row (quitRow);
//  - any "yes" hides the training windows immediately (wotm_training::g_inFight).
static PS2Runtime::RecompiledFunction g_origSure = nullptr;

// Lake Powell (stage 13, CANYON1, which retail does not offer): its .NGP carries an old version of the dialog box (shell->m_cs610,
// node 770: GROUP -> SWITCH). Each child of the switch is a GROUP with the game's logo (TRANSLATE -> 2-triangle OBJECT, already at
// depth 34.85) and the box (TRANSLATE (0, 35, z) -> OBJECT); the ARE YOU SURE one (child 1) sits at z -4.23..7.46, below the title,
// which areYouSure writes at y 0x42. On the other stages child 1 is only the box, at z -1.59..10.46. Here the logo is removed
// (TRANSLATE with no children) and child 1's box moves up to the same center (local z -2.71..8.98: translation 1.30). Absolute
// writes: repeating them is harmless. The title font is the stage's (smaller and without the '?') and stays.
static void lakePowellDialogBox(uint8_t *rdram)
{
    constexpr uint32_t sh = wotm::sym::shellInfo;
    constexpr float kBoxZ = 1.30f;
    auto op = [&](uint32_t n) { return rd32(rdram, n) & 0x3Fu; };
    auto ok = [](uint32_t a) { return a >= 0x100000u && a + 0x40u < PS2_GUEST_RAM_SIZE; };
    if (rd32(rdram, sh + 0x2BB0) != 13u)
        return;
    const uint32_t cs = rd32(rdram, sh + 0x610) & 0x1FFFFFFFu;
    const uint32_t ep = ok(cs) ? rd32(rdram, cs) & 0x1FFFFFFFu : 0u;
    if (!ok(ep) || op(ep) != 1u || (FAST_READ16(ep + 8) & 0xFFFFu) < 1u)
        return;
    const uint32_t sw = rd32(rdram, ep + 0x20) & 0x1FFFFFFFu;
    if (!ok(sw) || op(sw) != 6u)
        return;
    const uint32_t n = FAST_READ8(sw + 0xB);
    for (uint32_t k = 0; k < n && k < 8; ++k)
    {
        const uint32_t g = rd32(rdram, sw + 0xC + 4 * k) & 0x1FFFFFFFu;
        if (!ok(g) || op(g) != 1u || FAST_READ16(g + 8) != 2u)
            continue;
        const uint32_t logo = rd32(rdram, g + 0x20) & 0x1FFFFFFFu, box = rd32(rdram, g + 0x24) & 0x1FFFFFFFu;
        if (!ok(logo) || !ok(box) || op(logo) != 3u || op(box) != 3u)
            continue;
        float ly, by;
        std::memcpy(&ly, rdram + logo + 0x14, 4);
        std::memcpy(&by, rdram + box + 0x14, 4);
        if (ly != 0.f || by != 35.f)   // the logo carries its depth in the object; the box, in the translation
            continue;
        wr32(rdram, logo + 8, 0);
        if (k == 1)
            std::memcpy(rdram + box + 0x18, &kBoxZ, 4);
    }
}

static void sureHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::areYouSure__FRi)
    {
        g_origSure(rdram, ctx, runtime);
        return;
    }
    const uint32_t ra = GPR_U32(ctx, 31);
    lakePowellDialogBox(rdram);
    if (rd32(rdram, wotm::sym::gLastMenuSelection) == uint32_t(kQuit))
        wr32(rdram, wotm::sym::gLastMenuSelection, uint32_t(quitRow()));
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
        std::fprintf(stderr, "[training] reset positions (fight restart without the ARE YOU SURE)\n");
        ctx->r[2] = _mm_set_epi64x(0, 1);
        ctx->pc = ra;
        return;
    }
    g_origSure(rdram, ctx, runtime);
    if (ctx->pc == ra && GPR_U32(ctx, 2))
    {
        wotm_training::g_inFight = false;
        wotm_training::restoreDifficulty(rdram, wotm::sym::shellGame + wotm_training::kDifficulty);   // QUIT: the game's difficulty comes back (the save never sees ours)
    }
}

// Monster::playerInit: the control scheme (Monster+0x672C) comes from m_ctlrCfg[m_index] - 1, which for players 3 and 4 lands on
// m_vibration (vibration off: -1, no scheme). The port pins type A (0) on everyone.
static PS2Runtime::RecompiledFunction g_origPlayerInit = nullptr;
constexpr uint32_t kCtlScheme = 0x672C;
static void playerInitHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::playerInit__7Monster)
    {
        g_origPlayerInit(rdram, ctx, runtime);
        return;
    }
    const uint32_t m = GPR_U32(ctx, 4), ra = GPR_U32(ctx, 31);
    g_origPlayerInit(rdram, ctx, runtime);
    if (ctx->pc == ra && m)
    {
        if (const uint32_t was = rd32(rdram, m + kCtlScheme))
            std::fprintf(stderr, "[pause] playerInit for player %u: game control scheme %d -> 0 (type A)\n", rd32(rdram, m + 0x24) + 1,
                         int32_t(was));
        wr32(rdram, m + kCtlScheme, 0);
    }
}

// TheGame::SetPlayerMonster rewrites the scheme of slots 0 and 1 (m_ctlrCfg[i] - 1) after playerInit: type A again on
// m_monsters[0..3] (a save with type B/C, or m_ctlrCfg 0 = scheme -1, which turns off wotm_overrides.cpp's toggle lock-on)
static PS2Runtime::RecompiledFunction g_origSetPlayer = nullptr;
static void setPlayerHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != wotm::fn::SetPlayerMonster__7TheGameiiiii)
    {
        g_origSetPlayer(rdram, ctx, runtime);
        return;
    }
    const uint32_t ra = GPR_U32(ctx, 31);
    g_origSetPlayer(rdram, ctx, runtime);
    if (ctx->pc != ra) return;
    for (uint32_t i = 0; i < 4; ++i)
    {
        const uint32_t m = rd32(rdram, wotm::sym::shellGame + 0x120380u + 4u * i) & 0x1FFFFFFFu;
        if (m < 0x1000u || m + kCtlScheme + 4u > PS2_RAM_SIZE) continue;
        if (const uint32_t was = rd32(rdram, m + kCtlScheme))
            std::fprintf(stderr, "[pause] SetPlayerMonster: control scheme %d of monster %u -> 0 (type A)\n", int32_t(was), i);
        wr32(rdram, m + kCtlScheme, 0);
    }
}

static void install(PS2Runtime &runtime)
{
    struct H { uint32_t addr; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; };
    const H hooks[] = {
        {wotm::fn::playerInit__7Monster, &g_origPlayerInit, &playerInitHook},
        {wotm::fn::SetPlayerMonster__7TheGameiiiii, &g_origSetPlayer, &setPlayerHook},
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
    std::fprintf(stderr, "[pause] pause menu without VIBRATION and with RECOMPILED OPTIONS in place of CONTROL SCHEME (type A pinned for "
                         "all players); mouse in the pause and in dialogs\n");
}
} // namespace wotm_pausemenu
