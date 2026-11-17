// wotm_optscreen.inc: the game's options screen (screen 10) with its text redrawn. Included by wotm_live.inc after
// wotm_mainmenu.inc, whose hooks, screenAlpha and on-plane drawing it uses. Only active with the text menu (WOTM_TEXTMENU, the
// default).
//
// The game draws this screen on a wooden board (title "OPTIONS", photo on the left) with each row as an image: difficulty,
// player 1 and 2 controls, audio, effects and music volume, controller 1 and 2 vibration, "load settings" and "accept and save
// settings". In the port, controls and vibration live in "Recompiled options" and saving is automatic, so only difficulty,
// audio, the two volumes and "save settings and return" remain. What this file does:
//   - every frame on this screen (on exit from screenOptions and screenTransition) it sets alpha to 0 (+0x4C) on the game's row
//     objects: each item's switches (the dimmed label and the lit one with the glow), the value switches (difficulty,
//     stereo/mono, vibration), the slider cursors (drawMe of shell->m_sliderCs) and the loose objects. The loose objects are
//     found at fixed offsets from the stereo switch (the shell's bank is reallocated as a whole: "press [] for more", the icon on
//     the control rows, the bars), and each one is checked to really be an object before it is hidden;
//   - the list is drawn into its own texture with the Oswald font (SIL OFL; resource FONT_OSWALD in native/wotm.rc), placed on
//     the board's plane to the right of the photo, using values read from RAM (TheGame+0x1203CC,
//     shell->m_mono/m_sfxVolume/m_musicVolume);
//   - the screenGetInput(1) hook makes up/down skip the removed rows: before screenOptions handles the code, currentSelection[10]
//     is moved to the neighbor of the destination row, so the game's own +1/-1 lands on it (sound, sliders and everything else
//     stay with the game). X confirms as in retail: on value rows it leaves without saving; on "save settings and return" it
//     saves and leaves.
// If the font isn't embedded in the executable, the game's own screen is left as is.
namespace wotm_optscreen
{
using wotm_padsel::rd32;
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr int kKeep[] = {0, 3, 4, 5, 9};   // game items that remain, in list order
constexpr int kRows = int(std::size(kKeep));
constexpr uint32_t kStereoSw = kShell + 0x5F8;
constexpr int32_t kLoose[] = {0x20, 0x200, 0x490, 0x680, 0xD050};   // offsets from the stereo switch
constexpr int32_t kBoard = -0x40A0;                                   // the board (object)

struct View
{
    bool on = false;
    float alpha = 0.f;
    int sel = 0;          // row (0..kRows-1)
    int difficulty = 0;   // 0..2
    bool mono = false;
    float sfx = 0.f, music = 0.f;
    uint32_t board = 0;
};
static std::mutex g_m;
static View g_view;
// Mouse (step 9): the row under the cursor and where along the texture width (0..1); the window writes it, input reads it (g_m)
struct Hover { int row = -1; float x = 0.f; };
static Hover g_hover;
static std::atomic<bool> g_ready{false};   // font loaded (window): only then is the game's screen hidden

static uint16_t rd16(const uint8_t *rdram, uint32_t a) { uint16_t v; std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 2); return v; }

// Set alpha 0 on every object under n (object, group, translation, rotation, scale, switch: all children)
static void hideTree(uint8_t *rdram, uint32_t n, int depth)
{
    n &= 0x1FFFFFFFu;
    if (n < 0x1000u || n + 0x60 >= PS2_RAM_SIZE || depth > 8)
        return;
    auto kids = [&](uint32_t base, uint32_t cnt) {
        for (uint32_t k = 0; k < cnt && k < 64; ++k) hideTree(rdram, rd32(rdram, base + 4 * k), depth + 1);
    };
    switch (rdram[n] & 0x3F)
    {
    case 0: { const float z = 0.f; std::memcpy(rdram + n + 0x4C, &z, 4); break; }
    case 1: kids(n + 0x20, rd16(rdram, n + 8)); break;
    case 3: kids(n + 0x1C, rd32(rdram, n + 8)); break;
    case 4: kids(n + 0x50, rd32(rdram, n + 8)); break;
    case 30: kids(n + 0x2C, rd32(rdram, n + 8)); break;
    case 6: kids(n + 0xC, rdram[n + 0xB]); break;
    default: break;
    }
}

static int rowOfItem(int item)
{
    for (int r = 0; r < kRows; ++r)
        if (kKeep[r] == item) return r;
    return -1;
}

// Every frame (on exit from screenOptions and screenTransition)
static void frame(uint8_t *rdram)
{
    uint32_t screen = 0;
    const float alpha = wotm_mainmenu::screenAlpha(rdram, screen);
    View v;
    if (screen == kScreen && g_ready.load(std::memory_order_relaxed))
    {
        for (int i = 0; i < 10; ++i) hideTree(rdram, rd32(rdram, kShell + 0x25C + (kScreen * 10 + uint32_t(i)) * 4), 0);
        for (uint32_t off = 0x5DC; off < 0x600; off += 4) hideTree(rdram, rd32(rdram, kShell + off), 0);   // difficulty, vibration, stereo, mono
        for (int i = 0; i < 2; ++i)
            if (const uint32_t cs = rd32(rdram, kShell + 0x604 + 4u * uint32_t(i)) & 0x1FFFFFFFu; cs >= 0x1000u && cs + 4 < PS2_RAM_SIZE)
                rdram[cs + 0xC] = 0;   // slider cursor: drawMe of the _cs (only shown on this screen)
        const uint32_t stereo = rd32(rdram, kStereoSw) & 0x1FFFFFFFu;
        static bool warned = false;
        for (int32_t off : kLoose)
        {
            const uint32_t o = stereo + uint32_t(off);
            if (stereo >= 0x1000u && o + 0x60 < PS2_RAM_SIZE && (rdram[o] & 0x3F) == 0)
                hideTree(rdram, o, 0);
            else if (!warned)
            {
                warned = true;
                std::fprintf(stderr, "[options] loose object at stereo switch %+d is not an object: keeping the game's\n", off);
            }
        }
        v.on = alpha > 0.f;
        v.alpha = alpha;
        const int r = rowOfItem(static_cast<int>(rd32(rdram, wotm::sym::currentSelection + 4 * kScreen)));
        v.sel = r < 0 ? 0 : r;
        const uint32_t game = rd32(rdram, wotm::sym::game) & 0x1FFFFFFFu;
        v.difficulty = game >= 0x1000u ? std::clamp(static_cast<int>(rd32(rdram, game + 0x1203CC)), 0, 2) : 0;
        v.mono = rd32(rdram, kShell + 0x2C20) != 0;
        v.sfx = std::clamp(static_cast<float>(static_cast<int32_t>(rd32(rdram, kShell + 0x2A30))) / 1024.f, 0.f, 1.f);
        v.music = std::clamp(static_cast<float>(static_cast<int32_t>(rd32(rdram, kShell + 0x2A34))) / 830.f, 0.f, 1.f);
        v.board = stereo >= 0x1000u ? stereo + uint32_t(kBoard) : 0;
    }
    std::lock_guard<std::mutex> lk(g_m);
    g_view = v;
}

// renderUi layout measurements the mouse uses: difficulty and audio columns, volume bars
constexpr float kDiffX[3] = {0.49f, 0.69f, 0.89f}, kStereoX = 0.56f, kMonoX = 0.80f, kBarX0 = 0.40f, kBarX1 = 0.96f;

// Mouse (step 9): hovering selects the row; clicking a difficulty or audio value steps toward it (one left/right step per frame);
// holding the button on a volume bar drags it to the cursor; clicking "save settings and return" confirms. Returns the input code
// that screenOptions will handle.
static uint32_t mouseInput(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t code)
{
    static uint32_t s_seq = 0;
    static int s_drag = -1, s_lastDir = 0;        // bar being held (row) and the last step taken on it
    static int s_target = -1, s_targetRow = -1;   // value requested by clicking difficulty (0..2) or audio (0 stereo, 1 mono)
    const uint32_t seq = wotm::mouse::g_moveSeq.load(std::memory_order_relaxed);
    const bool moved = seq != s_seq;
    s_seq = seq;
    Hover hv;
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        hv = g_hover;
        v = g_view;
    }
    const bool click = wotm::mouse::takeClick(wotm::mouse::g_clickL);
    const bool held = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !wotm::mouse::g_captured.load(std::memory_order_relaxed);
    const uint32_t selAddr = wotm::sym::currentSelection + 4 * kScreen;
    if (code != 0)
    {
        s_target = s_drag = -1;   // keyboard or controller input takes priority
        return code;
    }
    if (hv.row >= 0 && (moved || click) && hv.row != v.sel && s_drag < 0)
    {
        wotm_padsel::wr32(rdram, selAddr, static_cast<uint32_t>(kKeep[hv.row]));
        if (!click) wotm_padsel::sfx(rdram, ctx, runtime, 4);
        if (!click) return 0;
        v.sel = hv.row;
    }
    if (click && hv.row == kRows - 1) return 6;
    if (click && hv.row == 0)
    {
        int best = 0;
        for (int i = 1; i < 3; ++i)
            if (std::fabs(hv.x - kDiffX[i]) < std::fabs(hv.x - kDiffX[best])) best = i;
        s_target = best;
        s_targetRow = 0;
    }
    if (click && hv.row == 1)
    {
        s_target = std::fabs(hv.x - kMonoX) < std::fabs(hv.x - kStereoX) ? 1 : 0;
        s_targetRow = 1;
    }
    if (click && (hv.row == 2 || hv.row == 3))
    {
        s_drag = hv.row;
        s_lastDir = 0;
    }
    if (s_target >= 0)
    {
        if (v.sel != s_targetRow) { s_target = -1; return 0; }
        const int cur = s_targetRow == 0 ? v.difficulty : int(v.mono);
        if (cur == s_target) { s_target = -1; return 0; }
        return s_target > cur ? 4 : 3;
    }
    if (s_drag >= 0)
    {
        if (!held || v.sel != s_drag) { s_drag = -1; return 0; }
        const float want = std::clamp((hv.x - kBarX0) / (kBarX1 - kBarX0), 0.f, 1.f), cur = s_drag == 2 ? v.sfx : v.music;
        const int dir = want > cur + 0.02f ? 1 : want < cur - 0.02f ? -1 : 0;
        if (dir == 0 || (s_lastDir && dir != s_lastDir && !moved)) { s_lastDir = 0; return 0; }   // arrived (or overshot: wait for mouse)
        s_lastDir = dir;
        return dir > 0 ? 4 : 3;
    }
    return 0;
}

// screenGetInput(1) for screen 10, after the game reads the input code and before screenOptions handles it: up/down skip the
// removed rows
static uint32_t input(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t code)
{
    if (!g_ready.load(std::memory_order_relaxed))
        return code;
    code = mouseInput(rdram, ctx, runtime, code);
    if (code != 1 && code != 2)
        return code;
    const uint32_t selAddr = wotm::sym::currentSelection + 4 * kScreen;
    const int numSel = static_cast<int>(rd32(rdram, wotm::sym::numSelectables_2938));   // last item (the game's items run from 0 to it)
    int r = rowOfItem(static_cast<int>(rd32(rdram, selAddr)));
    if (r < 0) r = 0;
    const int target = kKeep[(r + (code == 2 ? 1 : kRows - 1)) % kRows];
    // the game does +1 (wrapping from numSel to 0) or -1 (wrapping from 0 to numSel): start from the destination's neighbor
    int from = code == 2 ? target - 1 : target + 1;
    if (code == 2 && target == 0) from = numSel;
    if (code == 1 && target == numSel) from = 0;
    wotm_padsel::wr32(rdram, selAddr, static_cast<uint32_t>(from));
    return code;
}

// ------------------------------------------------------------------ drawing (window)
static Font g_font{};
static bool g_fontTried = false;
static const Font *font()
{
    if (!g_fontTried)
    {
        g_fontTried = true;
#ifdef _WIN32
        void *mod = GetModuleHandleA(nullptr);
        void *res = FindResourceA(mod, "FONT_OSWALD", reinterpret_cast<const char *>(static_cast<uintptr_t>(10)));   // RT_RCDATA
        void *h = res ? LoadResource(mod, res) : nullptr;
        const unsigned char *data = h ? static_cast<const unsigned char *>(LockResource(h)) : nullptr;
        const int size = res ? static_cast<int>(SizeofResource(mod, res)) : 0;
#else
        int size = 0;
        const unsigned char *data = wotmEmbeddedResource("FONT_OSWALD", size);
#endif
        if (data && size > 0)
            g_font = LoadFontFromMemory(".ttf", data, size, 128, nullptr, 0);
        if (g_font.texture.id)
        {
            GenTextureMipmaps(&g_font.texture);
            SetTextureFilter(g_font.texture, TEXTURE_FILTER_TRILINEAR);
            g_ready.store(true, std::memory_order_relaxed);
        }
        else
            std::fprintf(stderr, "[options] font FONT_OSWALD is not in the executable: keeping the game's options screen\n");
    }
    return g_font.texture.id ? &g_font : nullptr;
}

constexpr int kW = 1600, kH = 1000;   // texture: the part of the board right of the photo, below the title
constexpr float kRowY[kRows] = {0.10f, 0.27f, 0.44f, 0.61f, 0.87f};
constexpr float kFont = 104.f;
static RenderTexture2D g_rt{};

static void text(const Font &f, const char *s, float x, float cy, float size, Color c, bool center = false)
{
    const Vector2 sz = MeasureTextEx(f, s, size, 2.f);
    const Vector2 p{center ? x - sz.x * 0.5f : x, cy - sz.y * 0.5f};
    // engraved look: a light copy half a pixel below, like the letters painted on the board
    DrawTextEx(f, s, Vector2{p.x, p.y + 3.f}, size, 2.f, Color{255, 236, 200, static_cast<unsigned char>(c.a * 0.4f)});
    DrawTextEx(f, s, p, size, 2.f, c);
}

static void renderUi()
{
    const Font *f = font();
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    if (!f || !v.on)
        return;
    if (g_rt.id == 0)
    {
        g_rt = LoadRenderTexture(kW, kH);
        SetTextureFilter(g_rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    const Color ink{48, 30, 18, 255}, red{168, 30, 22, 255}, dim{72, 52, 36, 150}, selInk{132, 22, 14, 255};
    BeginTextureMode(g_rt);
    ClearBackground(Color{0, 0, 0, 0});
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    const float W = float(kW), H = float(kH);
    // selected row: a light band, like the game's glow behind the lit item
    {
        const float cy = kRowY[std::clamp(v.sel, 0, kRows - 1)] * H;
        DrawRectangleRounded(Rectangle{8.f, cy - H * 0.075f, W - 16.f, H * 0.15f}, 0.5f, 12, Color{255, 222, 160, 120});
    }
    static const char *const kLabels[kRows] = {"DIFFICULTY", "AUDIO", "FX VOLUME", "MUSIC VOLUME", "SAVE SETTINGS AND RETURN"};
    const float labX = 34.f;
    for (int r = 0; r < kRows - 1; ++r)
        text(*f, kLabels[r], labX, kRowY[r] * H, kFont, r == v.sel ? selInk : ink);
    text(*f, kLabels[kRows - 1], W * 0.5f, kRowY[kRows - 1] * H, kFont, kRows - 1 == v.sel ? selInk : ink, true);
    // values: the current one in red, the others dimmed
    static const char *const kDiff[3] = {"EASY", "MEDIUM", "HARD"};
    const float diffX[3] = {W * kDiffX[0], W * kDiffX[1], W * kDiffX[2]};
    for (int i = 0; i < 3; ++i) text(*f, kDiff[i], diffX[i], kRowY[0] * H, kFont, i == v.difficulty ? red : dim, true);
    text(*f, "STEREO", W * kStereoX, kRowY[1] * H, kFont, v.mono ? dim : red, true);
    text(*f, "MONO", W * kMonoX, kRowY[1] * H, kFont, v.mono ? red : dim, true);
    // bars: dark track, red filled part and the cursor
    const float bx0 = W * kBarX0, bx1 = W * kBarX1, bh = H * 0.05f;
    for (int i = 0; i < 2; ++i)
    {
        const float cy = kRowY[2 + i] * H, val = i == 0 ? v.sfx : v.music;
        DrawRectangleRounded(Rectangle{bx0, cy - bh * 0.5f, bx1 - bx0, bh}, 0.4f, 8, Color{40, 24, 14, 150});
        DrawRectangleRounded(Rectangle{bx0 + 3.f, cy - bh * 0.5f + 3.f, (bx1 - bx0 - 6.f) * val, bh - 6.f}, 0.4f, 8, red);
        const float kx = bx0 + (bx1 - bx0) * val;
        DrawRectangleRounded(Rectangle{kx - 11.f, cy - bh * 1.1f, 22.f, bh * 2.2f}, 0.3f, 6, Color{20, 14, 10, 255});
    }
    EndBlendMode();
    EndTextureMode();
}

// On the board's plane (inside the board's BeginMode3D), right of the photo and below the title
static void drawOnBoard(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    wotm_padsel::Plane pl;
    if (!v.on || g_rt.id == 0 || !v.board || !wotm_padsel::objectPlane(scene, ram, cam, v.board, pl))
        return;
    const float bw = pl.x1 - pl.x0, bh = pl.y1 - pl.y0;
    const float wx = bw * 0.72f, hy = wx * float(kH) / float(kW);
    wotm_padsel::drawOnPlane(pl, g_rt.texture, pl.x0 + bw * 0.265f, pl.y1 - bh * 0.215f, wx, hy, v.alpha, 0.004f * bw);
    // mouse: the row under it (bands around kRowY) and its x on the texture
    Hover hv;
    float u, w;
    wotm::mouse::claim();
    if (v.alpha >= 1.f && wotm_padsel::hitOnPlane(cam, pl, pl.x0 + bw * 0.265f, pl.y1 - bh * 0.215f, wx, hy, u, w))
        for (int r = 0; r < kRows; ++r)
            if (std::fabs(w - kRowY[r]) < 0.085f) { hv.row = r; hv.x = u; }
    static const bool log = std::getenv("WOTM_MENUMOUSELOG") != nullptr;
    static int lastRow = -2;
    if (log && hv.row != lastRow)
    {
        std::fprintf(stderr, "[menumouse] t=%.2f options row %d x %.2f\n", GetTime(), hv.row, hv.x);
        lastRow = hv.row;
    }
    std::lock_guard<std::mutex> lk(g_m);
    g_hover = hv;
}
} // namespace wotm_optscreen
