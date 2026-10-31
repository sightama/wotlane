// wotm_gamescreens.inc: mouse on the menu screens that are still the game's own (camera step 9): monster select (3, 6, 7), stage
// (8), minigame (14), elimination and free-for-all options (11, 12, 13), "first to" (15) and unlocks (21). Included by wotm_live.inc
// before wotm_menufont.inc. Two sides:
//  - window (frame, inside the frame's BeginMode3D): the on-screen rectangle of each of the screen's elements (the drawn objects of
//    its subtree, projected with the frame's camera) and what is under the cursor (View);
//  - EE thread (input, the output of the screen's screenGetInput(1), wotm_mainmenu.inc): the code the screen will handle. The mouse
//    never writes game variables (except on screen 21, text only): it moves through pad codes, one step per frame, and checks the
//    result on the next frame.
// Findings: 75f5be1:docs/investigacoes/2026-10-22/mouse_telas_monstro.md, 75f5be1:docs/investigacoes/2026-10-22/mouse_telas_opcoes.md and 75f5be1:docs/investigacoes/2026-10-22/mouse_telas_save.md.
namespace wotm_gamescreens
{
using wotm_padsel::rd32;
constexpr uint32_t kShell = wotm::sym::shellInfo;
enum : uint32_t { kUp = 1, kDown = 2, kLeft = 3, kRight = 4, kCross = 6 };

// Screens with their own mouse handling or none: setup, main and modes (text), pad configuration (out), options (text), 3 and 4
// players (wotm_padsel.inc), "press start", exit, 17 (unreachable)
static bool handled(uint32_t s)
{
    if ((s == 3 || s == 6 || s == 7) && wotm_padsel::g_solo == int(s))   // native 1- and 2-player select (wotm_padsel.inc): no game boxes
        return true;
    return s == 0 || s == 1 || s == 2 || s == 4 || s == 5 || s == 9 || s == 10 || s == 16 || s == 17 || s >= 18 && s <= 20 || s > 21;
}

// Screen 21 (screenUnlocker): 2D text only, row i with its top at y 82 + 11 i (name at x 230, state at x 450); cursor in s_2990 (and
// its copy currentMenuSelection_2985, made before screenGetInput), items_2992 rows, fade_2991 up to 0x80. Hovering selects, left
// click buys (the game's X, in the same frame). 75f5be1:docs/investigacoes/2026-10-22/mouse_telas_save.md
static uint32_t unlocker(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t code, bool moved, bool click)
{
    if (static_cast<int32_t>(rd32(rdram, wotm::sym::fade_2991)) < 0x80)
        return code;
    wotm::mouse::claim();
    const int n = static_cast<int>(rd32(rdram, wotm::sym::items_2992));
    float gx, gy;
    int hover = -1;
    if (wotm::mouse::gsPointer(gx, gy) && gx >= 90.f && gx < 560.f)
        for (int i = 0; i < n && i < 8; ++i)
            if (const float y = 82.f + 11.f * float(i); gy >= y - 1.f && gy < y + 10.f) hover = i;
    if (code != 0 || hover < 0)
        return code;
    const int cur = static_cast<int>(rd32(rdram, wotm::sym::s_2990));
    if ((moved || click) && hover != cur)
    {
        wotm_padsel::wr32(rdram, wotm::sym::s_2990, static_cast<uint32_t>(hover));
        wotm_padsel::wr32(rdram, wotm::sym::currentMenuSelection_2985, static_cast<uint32_t>(hover));
        if (!click) wotm_padsel::sfx(rdram, ctx, runtime, hover > cur ? 4 : 2);
    }
    return click ? kCross : 0;
}

// ------------------------------------------------------------------ window: rectangles

// What is under the cursor (the window writes, the EE reads; g_m)
struct View
{
    uint32_t screen = ~0u;
    bool any = false;    // the screen has some element on screen (the left click belongs to the mouse, not a stray X)
    int row = -1;        // 15: row 1..3
    int arrow = 0;       // 11..13, 15: -1 left arrow, +1 right arrow
    bool onElem = false; // 8, 11..14: over the picture, the figure or the row
    int third = 0;       // 8, 14: -1 left third, 0 center, +1 right
    int col = -1;        // 3, 6, 7: portrait 1..10 (after resting on it for a moment)
    int costume = -1;    // 3, 6, 7: costume 0..3 of the active box (unlocked ones only)
};
static std::mutex g_m;
static View g_view;

struct Box
{
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    bool ok() const { return x1 > x0 && y1 > y0; }
    void add(const Box &b) { x0 = std::min(x0, b.x0); y0 = std::min(y0, b.y0); x1 = std::max(x1, b.x1); y1 = std::max(y1, b.y1); }
    bool in(float x, float y, float pad = 0.f) const { return ok() && x >= x0 - pad && x <= x1 + pad && y >= y0 - pad && y <= y1 + pad; }
    float cx() const { return (x0 + x1) * 0.5f; }
    float cy() const { return (y0 + y1) * 0.5f; }
};

constexpr uint16_t kTexGlow = 194, kTexArrow = 190;   // highlighted item's glow (larger than the text) and animated arrows

// Objects of a subtree along the active path (SWITCH: only the child in use): f(object)
template <class F>
static void walkActive(const uint8_t *rdram, uint32_t n, F &f, int depth = 0)
{
    n &= 0x1FFFFFFFu;
    if (n < 0x1000u || n + 0x100u > PS2_RAM_SIZE || depth > 12)
        return;
    auto kids = [&](uint32_t first, uint32_t count) {
        for (uint32_t k = 0; k < count && k < 64; ++k) walkActive(rdram, rd32(rdram, first + 4 * k), f, depth + 1);
    };
    switch (rdram[n] & 0x3F)
    {
    case 0: f(n); break;                                                                   // OBJECT
    case 1: kids(n + 0x20, uint32_t(rdram[n + 8] | rdram[n + 9] << 8)); break;             // GROUP
    case 3: kids(n + 0x1C, rd32(rdram, n + 8)); break;                                     // TRANSLATE
    case 4: kids(n + 0x50, rd32(rdram, n + 8)); break;                                     // ROTATE
    case 30: kids(n + 0x2C, rd32(rdram, n + 8)); break;                                    // SCALE
    case 6:                                                                                // SWITCH
        if (rdram[n + 8] < rdram[n + 0xB]) walkActive(rdram, rd32(rdram, n + 0xC + 4u * rdram[n + 8]), f, depth + 1);
        break;
    case 17: kids(n + 0xCC, rd32(rdram, n + 4)); break;                                    // ANIM_XFORM
    case 25: kids(n + 0x2C, rd32(rdram, n + 0x28)); break;                                 // CHAR_INSTANCE
    case 35: kids(n + 0xC, rd32(rdram, n + 4)); break;                                     // ANIM_XFORM container
    default: break;
    }
}

// Projection of one frame: the drawn objects (scene.items) by node
struct Proj
{
    wotm::Scene &scene;
    const wotm::Ram &ram;
    const Camera3D &cam;
    int rw, rh;
    std::unordered_map<uint32_t, const wotm::DrawItem *> items;
    Proj(wotm::Scene &s, const wotm::Ram &r, const Camera3D &c, int w, int h) : scene(s), ram(r), cam(c), rw(w), rh(h)
    {
        for (const wotm::DrawItem &i : scene.items) items.emplace(i.node & 0x1FFFFFFFu, &i);
    }
    // On-screen rectangle of object `o` (false: not drawn this frame); tex = its texture
    bool object(uint32_t o, Box &b, uint16_t &tex)
    {
        const auto f = items.find(o);
        if (f == items.end()) return false;
        const wotm::DrawItem *it = f->second;
        if (it->fade <= 0.f || it->color[3] <= 0.f) return false;
        const wotm::MeshData *md = scene.mesh(ram, it->node);
        if (!md || md->tris.empty()) return false;
        tex = md->texId;
        const Vector3 fwd = wotm_padsel::vsub(cam.target, cam.position);
        for (const wotm::Vtx &v : md->tris)
        {
            float w[3];
            wotm::xform(it->m, v.x, v.y, v.z, w);
            const Vector3 p{w[0], w[1], w[2]};
            if (wotm_padsel::vdot(wotm_padsel::vsub(p, cam.position), fwd) <= 0.f) return false;   // behind the camera
            const Vector2 q = GetWorldToScreenEx(p, cam, rw, rh);
            b.x0 = std::min(b.x0, q.x); b.y0 = std::min(b.y0, q.y); b.x1 = std::max(b.x1, q.x); b.y1 = std::max(b.y1, q.y);
        }
        return b.ok();
    }
    // Union of the subtree's objects, without the glow; the arrows are collected separately (up to 2)
    Box tree(uint32_t root, Box *arrows = nullptr, int *nArrows = nullptr)
    {
        Box all;
        auto f = [&](uint32_t o) {
            Box b;
            uint16_t tex = 0;
            if (!object(o, b, tex) || tex == kTexGlow) return;
            if (tex == kTexArrow && arrows && nArrows)
            {
                if (*nArrows < 2) arrows[(*nArrows)++] = b;
                return;
            }
            all.add(b);
        };
        walkActive(ram.p, root, f);
        return all;
    }
};

static uint32_t menuItemSw(const uint8_t *rdram, uint32_t s, int i) { return rd32(rdram, kShell + 0x25C + (s * 10u + uint32_t(i)) * 4u); }
static uint32_t menuObj(const uint8_t *rdram, uint32_t s, int i) { return rd32(rdram, kShell + 0x6DC + (s * 90u + uint32_t(i)) * 4u); }

static int arrowSide(const Box *arrows, int n, const Box &line, float x, float y)
{
    for (int k = 0; k < n; ++k)
        if (arrows[k].in(x, y, 6.f)) return arrows[k].cx() < line.cx() ? -1 : 1;   // side from the projected X (the order changes per screen)
    return 0;
}

// The portraits group (12 children: plate, columns 1..10, frames). Screens 3 and 6: sibling of the m_selectSw in group P right after
// the m_selectSw[9] node; screen 7: inside child 4 of screen 14's switch
static uint32_t portraitGroup(const uint8_t *rdram, uint32_t s)
{
    auto isGroup = [&](uint32_t g, int n) {
        g &= 0x1FFFFFFFu;
        return g >= 0x1000u && g + 0x80u < PS2_RAM_SIZE && (rdram[g] & 0x3F) == 1 && (rdram[g + 8] | rdram[g + 9] << 8) == n;
    };
    if (s == 7)
    {
        const uint32_t sw = menuItemSw(rdram, 14, 0) & 0x1FFFFFFFu;
        if (sw < 0x1000u || (rdram[sw] & 0x3F) != 6 || rdram[sw + 0xB] < 5) return 0;
        uint32_t n = rd32(rdram, sw + 0xC + 16) & 0x1FFFFFFFu;
        for (int d = 0; d < 3 && n >= 0x1000u; ++d)   // child 4 or a GROUP right below it
        {
            if (isGroup(n, 12)) return n;
            if ((rdram[n] & 0x3F) != 1) return 0;
            uint32_t found = 0;
            for (uint32_t k = 0, c = uint32_t(rdram[n + 8] | rdram[n + 9] << 8); k < c && k < 16 && !found; ++k)
                if (isGroup(rd32(rdram, n + 0x20 + 4 * k), 12)) found = rd32(rdram, n + 0x20 + 4 * k) & 0x1FFFFFFFu;
            if (found) return found;
            n = rd32(rdram, n + 0x20) & 0x1FFFFFFFu;
        }
        return 0;
    }
    const uint32_t sw9 = rd32(rdram, kShell + 0x168 + 9 * 4) & 0x1FFFFFFFu;
    if (sw9 < 0x1000u || (rdram[sw9] & 0x3F) != 6) return 0;
    const uint32_t p = sw9 + 0xC + 4u * rdram[sw9 + 0xB];
    if (!isGroup(p, 11)) return 0;
    for (int k = 1; k <= 10; ++k)
        if ((rd32(rdram, p + 0x20 + 4 * k) & 0x1FFFFFFFu) != (rd32(rdram, kShell + 0x168 + 4 * (k - 1)) & 0x1FFFFFFFu)) return 0;
    const uint32_t g = rd32(rdram, p + 0x20) & 0x1FFFFFFFu;
    return isGroup(g, 12) ? g : 0;
}

// Screen 6 in mode 2, phase B: the player has already chosen; the column and costumes (right-hand box) are the AI's
static bool aiPhase(const uint8_t *rdram)
{
    return rd32(rdram, wotm_padsel::kMode) == 2 && rd32(rdram, wotm::sym::playerOneIsDone_2896) && !rd32(rdram, wotm::sym::AIplayerIsDone_2898);
}
// The screen's active costume box: its switches (4) and its cs
static void costumeBox(const uint8_t *rdram, uint32_t s, uint32_t &sw, uint32_t &cs)
{
    if (s == 7) { sw = kShell + 0xB8; cs = kShell + 0xB0; }
    else if (s == 6 && aiPhase(rdram)) { sw = kShell + 0xA0; cs = kShell + 0x8C; }
    else { sw = kShell + 0x90; cs = kShell + 0x88; }
}

static void frameMonsters(Proj &pj, uint8_t *rdram, uint32_t s, const wotm::mouse::Pointer &p, View &v)
{
    if (s == 6 && wotm_padsel::g_selOn)   // 3 or 4 players: the screen belongs to wotm_padsel.inc
        return;
    if (const uint32_t g = portraitGroup(rdram, s))
        for (int c = 1; c <= 10; ++c)
        {
            const Box b = pj.tree(rd32(rdram, g + 0x20 + 4 * c));
            if (!b.ok()) continue;
            v.any = true;
            if (p.in && b.in(p.x, p.y)) v.col = c;
        }
    uint32_t sw, cs;
    costumeBox(rdram, s, sw, cs);
    const uint32_t csNode = rd32(rdram, cs) & 0x1FFFFFFFu;
    float alpha = 0.f;
    if (csNode >= 0x1000u && csNode + 0x80u < PS2_RAM_SIZE) std::memcpy(&alpha, rdram + csNode + 0x7C, 4);
    if (csNode >= 0x1000u && rd32(rdram, csNode + 0xC) == 1 && alpha > 0.f)
        for (int k = 0; k < 4; ++k)
        {
            const uint32_t n = rd32(rdram, sw + 4 * k) & 0x1FFFFFFFu;
            if (n < 0x1000u || (rdram[n] & 0x3F) != 6 || rdram[n + 8] == 0) continue;   // frame 0: costume locked
            const Box b = pj.tree(n);
            if (!b.ok()) continue;
            v.any = true;
            if (p.in && b.in(p.x, p.y)) v.costume = k;
        }
}

// Window, inside the frame's BeginMode3D (camera `cam`, frame rw x rh)
static void frame(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam, int rw, int rh)
{
    View v;
    uint8_t *rdram = const_cast<uint8_t *>(ram.p);
    if (!rdram || ram.size < PS2_RAM_SIZE)
        return;
    const uint32_t s = rd32(rdram, wotm::sym::currScreen);
    const wotm::mouse::Pointer &p = wotm::mouse::g_ptr;
    static double s_colSince = 0.0;
    static int s_colSeen = -1;
    if (rd32(rdram, wotm::sym::betweenScreens) == 0 && !handled(s) && s != 21)
    {
        v.screen = s;
        Proj pj(scene, ram, cam, rw, rh);
        if (s == 3 || s == 6 || s == 7)
            frameMonsters(pj, rdram, s, p, v);
        else if (s == 15)
        {
            Box rows[4], arrows[2];
            int nA = 0;
            for (int i = 1; i <= 3; ++i) rows[i] = pj.tree(menuItemSw(rdram, 15, i), i == 2 ? arrows : nullptr, i == 2 ? &nA : nullptr);
            float best = 1e30f;
            for (int i = 1; i <= 3; ++i)
            {
                if (!rows[i].ok()) continue;
                v.any = true;
                if (p.in && rows[i].in(p.x, p.y, 6.f) && std::fabs(p.y - rows[i].cy()) < best) { best = std::fabs(p.y - rows[i].cy()); v.row = i; }
            }
            if (p.in && rows[2].ok()) v.arrow = arrowSide(arrows, nA, rows[2], p.x, p.y);
            if (v.arrow) v.row = 2;
        }
        else if (s >= 11 && s <= 13)
        {
            Box arrows[2];
            int nA = 0;
            Box line = pj.tree(menuItemSw(rdram, s, 0), arrows, &nA);
            if (s == 11) line.add(pj.tree(menuItemSw(rdram, s, 1)));
            for (int i = 0; i < 90; ++i)
                if (const uint32_t o = menuObj(rdram, s, i)) line.add(pj.tree(o, arrows, &nA));
            if (line.ok() || nA)
            {
                v.any = true;
                if (p.in) v.arrow = arrowSide(arrows, nA, line, p.x, p.y);
                v.onElem = p.in && !v.arrow && line.in(p.x, p.y);
            }
        }
        else if (s == 8 || s == 14)
        {
            const Box b = pj.tree(s == 8 ? rd32(rdram, kShell + 0x5D0) : menuItemSw(rdram, 14, 0));
            if (b.ok())
            {
                v.any = true;
                v.onElem = p.in && b.in(p.x, p.y);
                const float t = (p.x - b.x0) / (b.x1 - b.x0);
                v.third = t < 1.f / 3.f ? -1 : t > 2.f / 3.f ? 1 : 0;
            }
        }
        // portrait: only after resting on it for ~0.1 s (on the way to the costume boxes the cursor passes over other portraits)
        if (v.col != s_colSeen) { s_colSeen = v.col; s_colSince = GetTime(); }
        if (GetTime() - s_colSince < 0.1) v.col = -1;
        if (v.any) wotm::mouse::claim();
        static const bool log = std::getenv("WOTM_MENUMOUSELOG") != nullptr;
        static View s_last;
        if (log && (v.screen != s_last.screen || v.row != s_last.row || v.arrow != s_last.arrow || v.onElem != s_last.onElem ||
                    v.third != s_last.third || v.any != s_last.any || v.col != s_last.col || v.costume != s_last.costume))
        {
            std::fprintf(stderr, "[menumouse] t=%.2f game screen %u (items %d): row %d arrow %d elem %d third %d portrait %d costume %d\n", GetTime(), s, int(v.any),
                         v.row, v.arrow, int(v.onElem), v.third, v.col, v.costume);
            s_last = v;
        }
    }
    std::lock_guard<std::mutex> lk(g_m);
    g_view = v;
}

// ------------------------------------------------------------------ EE: codes

// Target in progress: moves one step per frame until the value gets there (and confirms, if it was a click); gives up if a step does
// not change the value or after 10 steps
struct Goal
{
    int kind = 0;   // 0 none, 1 column, 2 costume, 3 row on screen 15
    int target = 0, last = -999, steps = 0;
    bool confirm = false;
};

static int colOf(const uint8_t *rdram, uint32_t s) { return static_cast<int>(rd32(rdram, wotm::sym::currentSelection + 4 * s)); }
static int costumeOf(const uint8_t *rdram, uint32_t s)
{
    const uint32_t a = s == 3 ? wotm::sym::curCostume1_2833 : s == 7 ? wotm::sym::curCostume1_2912
                     : aiPhase(rdram) ? wotm::sym::curCostume2_2895 : wotm::sym::curCostume1_2894;
    return static_cast<int>(rd32(rdram, a));
}
static bool columnValid(uint8_t *rdram, uint32_t s, int col)
{
    const int lo = static_cast<int>(rd32(rdram, s == 3 ? wotm::sym::first_2831 : s == 7 ? wotm::sym::beginning_2908 : wotm::sym::beginning_2888));
    const int hi = static_cast<int>(rd32(rdram, s == 3 ? wotm::sym::last_2832 : s == 7 ? wotm::sym::ending_2909 : wotm::sym::ending_2889));
    if (col < lo || col > hi || !wotm_padsel::columnOpen(rdram, col)) return false;
    for (int c = 0; c < 4; ++c)
        if (wotm_padsel::costumeOpen(rdram, col, c)) return true;
    return false;
}
// The player (or the AI, in mode 2's phase B) has already confirmed: the screen ignores the steps
static bool monsterDone(const uint8_t *rdram, uint32_t s)
{
    if (s == 7) return rd32(rdram, wotm::sym::playerOneIsDone_2914) != 0;
    if (s == 6) return aiPhase(rdram) ? false : rd32(rdram, wotm::sym::playerOneIsDone_2896) != 0;
    return false;
}

static uint32_t stepGoal(Goal &g, int cur)
{
    if (g.kind == 0) return 0;
    if (cur == g.target)
    {
        const bool c = g.confirm;
        g = Goal{};
        return c ? kCross : 0;
    }
    if (g.steps >= 10 || cur == g.last) { g = Goal{}; return 0; }
    ++g.steps;
    g.last = cur;
    if (g.kind == 1) return g.target > cur ? kRight : kLeft;
    if (g.kind == 2) return g.target > cur ? kDown : kUp;
    return g.target == (cur == 1 ? 3 : cur - 1) ? kUp : kDown;   // 15: three rows that wrap around, any one is a single step away
}

static uint32_t compute(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t s, uint32_t code)
{
    static Goal s_goal;
    static uint32_t s_screen = ~0u, s_seq = 0;
    const uint32_t seq = wotm::mouse::g_moveSeq.load(std::memory_order_relaxed);
    const bool moved = seq != s_seq;
    s_seq = seq;
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    const bool click = (v.any || s == 21) && wotm::mouse::takeClick(wotm::mouse::g_clickL);
    // screens that move sideways: Q/E (and the wheel, except on unlocks, where it moves through the rows) become left/right on the
    // keyboard (wotm_pad.inc); so do the controller's L1/R1
    const bool side = s == 8 || (s >= 11 && s <= 14) || s == 21;
    if (side)
    {
        wotm::mouse::sideScreen(s != 21);
        if (code == 9 || code == 0xC) code = code == 9 ? kLeft : kRight;
    }
    if (s == 21)
        return unlocker(rdram, ctx, runtime, code, moved, click);
    if (s != s_screen || v.screen != s || handled(s) || rd32(rdram, wotm::sym::betweenScreens))
    {
        s_screen = s;
        s_goal = Goal{};
        return code;
    }
    if (code != 0)   // pad or keyboard: the mouse target is dropped
    {
        s_goal = Goal{};
        return code;
    }
    switch (s)
    {
    case 3: case 6: case 7: {
        if (monsterDone(rdram, s)) { s_goal = Goal{}; return 0; }
        const int col = colOf(rdram, s), cost = costumeOf(rdram, s);
        // the portrait only shows up in the View after the cursor rests on it for a moment (frame): its arrival counts as movement
        static int s_vcol = -1, s_vcost = -1;
        const bool newCol = v.col != s_vcol, newCost = v.costume != s_vcost;
        s_vcol = v.col;
        s_vcost = v.costume;
        if (v.costume >= 0 && (moved || newCost || click) && (v.costume != cost || click))
            s_goal = Goal{2, v.costume, -999, 0, click};
        else if (v.col >= 0 && (moved || newCol || click) && (v.col != col || click) && columnValid(rdram, s, v.col))
            s_goal = Goal{1, v.col, -999, 0, click};
        return stepGoal(s_goal, s_goal.kind == 2 ? cost : col);
    }
    case 15: {
        const int cur = colOf(rdram, s);
        if (click && v.arrow && cur == 2) return v.arrow < 0 ? kLeft : kRight;
        if (v.row >= 1 && (moved || click) && (v.row != cur || click))
            s_goal = Goal{3, v.row, -999, 0, click};
        return stepGoal(s_goal, cur);
    }
    case 11: case 12: case 13:
        if (click && v.arrow) return v.arrow < 0 ? kLeft : kRight;
        return click && v.onElem ? kCross : 0;
    case 8: case 14:
        if (!click || !v.onElem) return 0;
        return v.third < 0 ? kLeft : v.third > 0 ? kRight : kCross;
    default:
        return code;
    }
}

// EE, output of screen `s`'s screenGetInput(1) (code = what the game read): returns the code the screen will handle. Screen 8 reads
// pad 1 twice in the same frame (the first only checks "nothing pressed" to release the movie): the second read, right after,
// gets the same code, as the pad would give
static uint32_t input(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t s, uint32_t code)
{
    static int64_t s_t = 0;
    static uint32_t s_s = ~0u, s_code = 0, s_out = 0;
    const int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (s == 8 && s_s == 8 && code == s_code && now - s_t < 5000)
        return s_out;
    s_t = now;
    s_s = s;
    s_code = code;
    s_out = compute(rdram, ctx, runtime, s, code);
    return s_out;
}
} // namespace wotm_gamescreens
