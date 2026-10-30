// wotm_prompts.inc: 2D button icons (Kenney "Input Prompts", CC0) replacing McButtons' 3D objects (the cross, circle, square and
// triangle in the menu button legends and on the memory card pages). Included by wotm_live.inc, after wotm_pad.inc and
// wotm_native.hpp. The native renderer (Scene::drawButtonSprites) finds where the game placed each icon and calls icon() with its
// center and size.
//
// Which sprite: the icon's PS2 button is replaced by whatever the player actually presses for it, according to the key/button map
// (wotm_settings.hpp, CONTROLS tab) and the menu confirm-button swap (MenuConfirm, wotm_pad.inc). Style (Prompts option): automatic
// = follow the last device used (keyboard: the key; gamepad: chosen by its name, Xbox, PlayStation or Nintendo), or a fixed style;
// "original" keeps the game's 3D icons.
// The atlas (native/assets/prompts.png, built by tools/prompts_atlas.py) is embedded in the executables (WOTM_PROMPTS resource in
// native/wotm.rc).
// PlayStation-style face buttons (also used for "original" wherever a 2D icon is drawn) come from the game itself instead of
// Kenney's: texture 163 of SHELL/SHELL.TEX on the disc (T8 64x64, palette at tbp 519 in SHELL.RTX; circle, square, triangle and
// cross in 32x32 quadrants).
#include "../wotm_prompts.h"
#include "wotm_model.hpp"   // decodeT8, unzipIe

#ifdef _WIN32
// No windows.h (its names clash with raylib's): declare only what is needed to read a resource from our own executable
extern "C" __declspec(dllimport) void *__stdcall GetModuleHandleA(const char *);
extern "C" __declspec(dllimport) void *__stdcall FindResourceA(void *module, const char *name, const char *type);
extern "C" __declspec(dllimport) void *__stdcall LoadResource(void *module, void *res);
extern "C" __declspec(dllimport) void *__stdcall LockResource(void *data);
extern "C" __declspec(dllimport) unsigned long __stdcall SizeofResource(void *module, void *res);
#else
#include "../wotm_embedded.hpp"   // the same resources as byte arrays (wotmEmbeddedResource)
#endif

namespace wotm_prompts
{
namespace st = wotm::settings;
namespace pr = wotm::prompts;

static Texture2D g_atlas{};
static bool g_tried = false;

static bool loadAtlas()
{
    if (g_tried) return g_atlas.id != 0;
    g_tried = true;
#ifdef _WIN32
    void *mod = GetModuleHandleA(nullptr);
    void *res = FindResourceA(mod, "WOTM_PROMPTS", reinterpret_cast<const char *>(static_cast<uintptr_t>(10)));   // RT_RCDATA
    void *h = res ? LoadResource(mod, res) : nullptr;
    const unsigned char *data = h ? static_cast<const unsigned char *>(LockResource(h)) : nullptr;
    const int size = res ? static_cast<int>(SizeofResource(mod, res)) : 0;
#else
    int size = 0;
    const unsigned char *data = wotmEmbeddedResource("WOTM_PROMPTS", size);
#endif
    if (!data || size <= 0)
    {
        std::fprintf(stderr, "[prompts] atlas WOTM_PROMPTS is not in the executable: keeping the game's icons\n");
        return false;
    }
    Image img = LoadImageFromMemory(".png", data, size);
    if (!img.data)
    {
        std::fprintf(stderr, "[prompts] atlas WOTM_PROMPTS could not be decoded: keeping the game's icons\n");
        return false;
    }
    g_atlas = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&g_atlas);
    SetTextureFilter(g_atlas, TEXTURE_FILTER_TRILINEAR);
    std::fprintf(stderr, "[prompts] atlas %dx%d\n", g_atlas.width, g_atlas.height);
    return true;
}

// The game's own face buttons (window thread). Virtual cells kFaceBase + 0..3 (triangle, circle, cross, square); each one's
// quadrant in the texture as stored on the disc (rows run bottom to top, so the source rect is flipped when drawing).
constexpr int kFaceBase = 1000;
static Texture2D g_face{};
static bool g_faceTried = false;
static constexpr int kFaceQuad[4][2] = {{0, 32}, {0, 0}, {32, 32}, {32, 0}};
static bool loadFace()
{
    if (g_faceTried) return g_face.id != 0;
    g_faceTried = true;
    const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    std::vector<uint8_t> tex, rtx;
    wotm::model::TexImage im;
    if (!wotm::disc::readFile(io.cdRoot, io.cdImage.string(), "SHELL/SHELL.TEX", tex) ||
        !wotm::disc::readFile(io.cdRoot, io.cdImage.string(), "SHELL/SHELL.RTX", rtx) || !wotm::model::unzipIe(tex) ||
        !wotm::model::unzipIe(rtx) || !wotm::model::decodeT8(tex, rtx, 163, 519, im) || im.w != 64 || im.h != 64)
    {
        std::fprintf(stderr, "[prompts] could not read the game's face buttons (SHELL.TEX 163): using Kenney's\n");
        return false;
    }
    Image img{im.rgba.data(), im.w, im.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    g_face = LoadTextureFromImage(img);
    SetTextureFilter(g_face, TEXTURE_FILTER_BILINEAR);
    std::fprintf(stderr, "[prompts] loaded the game's face buttons (SHELL.TEX 163)\n");
    return true;
}
// Source rect of face button `cell` (kFaceBase + i) in the texture, for DrawTexturePro: the negative height flips the image
// vertically (the rect itself stays in the same place)
static Rectangle faceSrc(int cell)
{
    const int *q = kFaceQuad[cell - kFaceBase];
    return Rectangle{float(q[0]), float(q[1]), 32.f, -32.f};
}

// Last device used (window thread, once per frame): the keyboard or gamepad n
static int g_lastPad = -1;   // -1 = keyboard
static double g_lastPoll = -1.0;
static void pollDevice()
{
    const double now = GetTime();
    if (now == g_lastPoll) return;
    g_lastPoll = now;
    for (const st::KeyName &k : st::kKeyNames)
        if (wotm_pad::inputDown(k.code)) { g_lastPad = -1; return; }   // keys and mouse buttons
    for (int p = 0; p < 4; ++p)
    {
        if (!IsGamepadAvailable(p)) continue;
        bool any = false;
        for (int b = 1; b < st::kPadButtons && !any; ++b) any = IsGamepadButtonDown(p, b);
        for (int a = 0; a < 4 && !any; ++a) any = std::fabs(GetGamepadAxisMovement(p, a)) > 0.5f;
        if (any) { g_lastPad = p; return; }
    }
    // nothing used yet: default to the first connected gamepad, if any
    static bool first = true;
    if (first)
    {
        first = false;
        for (int p = 0; p < 4; ++p)
            if (IsGamepadAvailable(p)) { g_lastPad = p; break; }
    }
}

static int styleOfPad(int pad)
{
    std::string n = pad >= 0 && IsGamepadAvailable(pad) ? GetGamepadName(pad) : "";
    for (char &c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto has = [&](const char *s) { return n.find(s) != std::string::npos; };
    if (has("playstation") || has("dualshock") || has("dualsense") || has("ps3") || has("ps4") || has("ps5") || has("sony") ||
        has("wireless controller"))
        return st::PromptPs;
    if (has("nintendo") || has("switch") || has("pro controller") || has("joy-con"))
        return st::PromptNintendo;
    return st::PromptXbox;
}

// McButtons icon -> PS2 button. Index: 0x1F51.. maps directly; 0x240E.. goes through initButton's table. The index order is the
// order of the characters in drawButtonsForString's s_charMap (' ^ ` $), which matches the scePadRead bit order (triangle 0x1000,
// circle 0x2000, cross 0x4000, square 0x8000). Verified: "press the ' button to cancel" / "press the ` button to continue" on the
// memory card; in the Shell, 0x240F (SELECT) comes out as cross and 0x240E (BACK) as triangle.
static constexpr st::Bind kIndexBind[4] = {st::BTriangle, st::BCircle, st::BCross, st::BSquare};
static int buttonIndex(uint16_t id)
{
    if (id >= 0x1F51 && id <= 0x1F54) return id - 0x1F51;
    static constexpr int k240E[4] = {0, 2, 3, 1};
    if (id >= 0x240E && id <= 0x2411) return k240E[id - 0x240E];
    return -1;
}

// Current icon style: automatic is resolved from the last device used; "original" (the game's 3D icons) becomes the PS2 style for
// the 2D icons the game has no equivalent for (the bottom legend, wotm_legend.inc)
static int currentStyle()
{
    pollDevice();
    const int s = st::get(st::Prompts);
    if (s == st::PromptAuto) return g_lastPad < 0 ? st::PromptKeyboard : styleOfPad(g_lastPad);
    return s == st::PromptOriginal ? st::PromptPs : s;
}
static int keyCell(const char *name)
{
    if (name)
        for (const pr::KeyCell &k : pr::kKeys)
            if (!std::strcmp(k.name, name)) return k.cell;
    return -1;
}
static int padCell(int phys, int style)
{
    if (phys <= 0 || phys >= pr::kPadCells) return -1;   // 18 and 19: left and right analog sticks (legend only, kStickL/kStickR)
    // face buttons (raylib 5..8: up, right, down, left) in PS2 style: use the game's own
    if (style == st::PromptPs && phys >= 5 && phys <= 8 && loadFace())
    {
        static constexpr int kFace[4] = {0, 1, 2, 3};   // triangle, circle, cross, square
        return kFaceBase + kFace[phys - 5];
    }
    const int *table = style == st::PromptNintendo ? pr::kNintendo : style == st::PromptXbox ? pr::kXbox : pr::kPs;
    return table[phys];
}

// Atlas cell for PS2 button b in the current style (gamepad: via the button map of the player on the last gamepad used)
static int cellFor(st::Bind b)
{
    const int style = currentStyle();
    if (style == st::PromptKeyboard)
    {
        const char *name = st::keyName(int(st::kb(b) & 0xFFFF));
        // middle mouse button and wheel: the scroll-wheel icon; L1/R1 with no key bound (layout 2): Q/E, the panel's tab keys
        if (name && (!std::strcmp(name, "MOUSE_MIDDLE") || !std::strncmp(name, "WHEEL", 5))) name = "MOUSE_SCROLL";
        if (!name && (b == st::BL1 || b == st::BR1)) name = b == st::BL1 ? "Q" : "E";
        const int c = keyCell(name);
        if (c >= 0) return c;   // no key (or a key with no icon): fall back to the PS2 symbol
    }
    return padCell(st::pad(wotm_pad::sourceBind(b), wotm_pad::playerOfPad(g_lastPad)), style == st::PromptKeyboard ? st::PromptPs : style);
}

static void icon(uint16_t id, const float mid[3], const float right[3], const float up[3], float size, float alpha)
{
    const int idx = buttonIndex(id);
    static uint32_t logged = 0;
    if (idx >= 0 && !(logged & (1u << idx)) && std::getenv("WOTM_MENULOG"))
    {
        logged |= 1u << idx;
        std::fprintf(stderr, "[prompts] icon id=0x%X index=%d -> %s\n", unsigned(id), idx, st::kBindNames[kIndexBind[idx]]);
    }
    const int cell = idx >= 0 ? cellFor(kIndexBind[idx]) : -1;
    if (cell < 0 || !loadAtlas()) return;
    const bool face = cell >= kFaceBase;
    // Kenney's art has padding inside its cell (the circle fills ~3/4 of it), so its quad is drawn larger than the game's icon;
    // the game's own face buttons fill their whole quadrant
    const float h = size * 0.5f * (face ? 1.f : 128.f / 96.f);
    float u0, v0, u1, v1;
    if (face)
    {
        const Rectangle r = faceSrc(cell);
        u0 = r.x / 64.f; u1 = (r.x + r.width) / 64.f; v0 = (r.y + 32.f) / 64.f; v1 = r.y / 64.f;   // flipped: the bottom row on top
    }
    else
    {
        u0 = float(cell % pr::kCols) / float(pr::kCols); v0 = float(cell / pr::kCols) / float(pr::kRows);
        u1 = u0 + 1.f / float(pr::kCols); v1 = v0 + 1.f / float(pr::kRows);
    }
    // the legend text starts right after the game's icon and the 2D art would touch it, so the icon is shifted a little to the left
    const float shift = -size * 0.16f;
    auto corner = [&](float sr, float su, float out[3]) {
        for (int k = 0; k < 3; ++k) out[k] = mid[k] + right[k] * (sr * h + shift) + up[k] * su * h;
    };
    float a[3], b[3], c[3], d[3];
    corner(-1, 1, a); corner(-1, -1, b); corner(1, -1, c); corner(1, 1, d);
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlSetTexture(face ? g_face.id : g_atlas.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, static_cast<unsigned char>(std::clamp(alpha, 0.f, 1.f) * 255.f));
    rlTexCoord2f(u0, v0); rlVertex3f(a[0], a[1], a[2]);
    rlTexCoord2f(u0, v1); rlVertex3f(b[0], b[1], b[2]);
    rlTexCoord2f(u1, v1); rlVertex3f(c[0], c[1], c[2]);
    rlTexCoord2f(u1, v0); rlVertex3f(d[0], d[1], d[2]);
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

static bool on() { return st::get(st::Prompts) != st::PromptOriginal && loadAtlas(); }

// Draws an atlas cell on screen (to the current render target) as a square of side `size` centered on (cx, cy); the art has
// padding inside its cell, as in icon()
static bool drawCell2d(int cell, float cx, float cy, float size, float alpha)
{
    if (cell < 0 || !loadAtlas()) return false;
    if (cell >= kFaceBase)
    {
        DrawTexturePro(g_face, faceSrc(cell), Rectangle{cx - size * 0.5f, cy - size * 0.5f, size, size}, Vector2{0.f, 0.f}, 0.f,
                       Color{255, 255, 255, static_cast<unsigned char>(std::clamp(alpha, 0.f, 1.f) * 255.f)});
        return true;
    }
    const float h = size * (128.f / 96.f), k = float(pr::kCell);
    DrawTexturePro(g_atlas, Rectangle{float(cell % pr::kCols) * k, float(cell / pr::kCols) * k, k, k}, Rectangle{cx - h * 0.5f, cy - h * 0.5f, h, h},
                   Vector2{0.f, 0.f}, 0.f, Color{255, 255, 255, static_cast<unsigned char>(std::clamp(alpha, 0.f, 1.f) * 255.f)});
    return true;
}

static const bool s_installed = (wotm::g_buttonIconFn = &icon, wotm::g_buttonIconsOn = &on, true);
} // namespace wotm_prompts

// Bottom legend (in the black bar, replacing the game's own) and a large hint over the scene: whoever wants them (the controller
// selection in wotm_padsel.inc, the panel in wotm_mainmenu.inc) posts them every frame from its renderUi; wotm_legend.inc draws them
// for that frame and then clears them. All on the window thread.
namespace wotm_legend
{
namespace st = wotm::settings;
// One icon: the raylib button (gamepad, 0 = none) and the key (a name from wotm_settings.hpp; if it has no atlas cell, the name is
// drawn inside a frame); key2 = the second key bound to the same button (the keyboard map has two per button), drawn next to the first
struct Icon { int pad = 0; const char *key = nullptr; const char *key2 = nullptr; };
struct Item { Icon a, b; const char *text = ""; };
constexpr int kStickL = 18, kStickR = 19;   // Icon::pad values for the analog sticks (extra atlas cells, wotm_prompts.h)
constexpr int kMax = 10;
static Item g_items[kMax];
static int g_n = 0;
static float g_alpha = 0.f;
static float g_barAlpha = 1.f;   // black bar opacity (photo mode makes it translucent over the scene)
static std::string g_hint;
static float g_hintAlpha = 0.f;

static void post(std::initializer_list<Item> items, float alpha, float barAlpha = 1.f)
{
    g_n = 0;
    for (const Item &i : items)
        if (g_n < kMax) g_items[g_n++] = i;
    g_alpha = alpha;
    g_barAlpha = barAlpha;
}
static void hint(const char *s, float alpha)
{
    g_hint = s ? s : "";
    g_hintAlpha = alpha;
}
// PS2 button b as the game reads it: the key and button from the map (CONTROLS tab), with the menu confirm-button swap applied
static Icon bind(st::Bind b)
{
    const uint32_t k = st::kb(b);
    const char *k1 = st::keyName(int(k & 0xFFFF)), *k2 = st::keyName(int(k >> 16));
    if (!k1) { k1 = k2; k2 = nullptr; }
    return Icon{st::pad(wotm_pad::sourceBind(b), wotm_pad::playerOfPad(wotm_prompts::g_lastPad)), k1, k2};
}
} // namespace wotm_legend
