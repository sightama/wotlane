// wotm_menufont.inc: the menu list's words (hand-drawn white lowercase with a black outline) built from the letters in the game's
// own images (included by wotm_live.inc, before wotm_mainmenu.inc, which draws with it).
//
// The game stores each menu item as a ready-made word in T4 textures 81, 142 and 162 of SHELL/SHELL.TEX (palettes tbp 239, 480 and
// 518 of SHELL.RTX). The words the game has ("options", "load game", "unlocks", "adventure", "free-for-all", "minigames") are used
// whole; ours ("single player", "multiplayer", "recompiled options", "quit game", and "endurance"/"elimination", which only exist
// in grey in the game) are built from pieces of those words: at each step, the longest piece of any word that matches what is
// still missing (the phrases "sudden death" and "unlimited play", which the game squeezed, only when the letter exists nowhere
// else). The font has no b, j, q, x or z; q is a mirrored p.
//
// kLines: each word (or pair of words) in the textures, already flipped for reading: texture, y range, baseline, x-height and the
// x of each letter (white part only: the black outline touches the neighboring letter). A cut extends to the middle of the gap on
// each side (at most 4 px past the white), so cuts placed together keep the game's spacing. Measured on the exported textures (no
// game pixels in the repo).
namespace wotm_menufont
{
struct Line
{
    int tex;           // index into kTexIds
    int y0, y1;        // the line's y range (including the outline)
    float base, xh;    // baseline and x-height (median of the letters without ascenders/descenders)
    const char *text;  // the letters, in order (a space separates words and does not count as a letter)
    int seg[14][2];    // x range of each letter's white part
    bool narrow = false;   // phrase squeezed by the game (narrower letters)
};
constexpr uint16_t kTexIds[3] = {81, 142, 162}, kPalTbp[3] = {239, 480, 518};
constexpr Line kLines[] = {
    {0, 5, 54, 50.0f, 29.0f, "free-for-all", {{6, 20}, {27, 41}, {48, 65}, {73, 90}, {97, 112}, {118, 132}, {138, 155}, {163, 177}, {182, 197}, {204, 221}, {229, 236}, {244, 251}}},
    {0, 61, 119, 105.0f, 31.0f, "load game", {{18, 25}, {32, 49}, {56, 73}, {81, 100}, {130, 149}, {157, 174}, {182, 215}, {222, 239}}},
    {0, 121, 175, 160.0f, 30.0f, "minigames", {{16, 49}, {57, 63}, {71, 91}, {97, 103}, {111, 130}, {138, 155}, {163, 196}, {203, 220}, {228, 243}}},
    {1, 122, 179, 164.0f, 29.0f, "options", {{13, 31}, {38, 57}, {63, 80}, {88, 94}, {101, 119}, {125, 146}, {152, 168}}},
    {1, 189, 248, 231.0f, 29.0f, "player", {{14, 33}, {40, 48}, {54, 72}, {78, 103}, {109, 128}, {135, 150}}},
    {2, 3, 51, 47.0f, 29.0f, "adventure", {{16, 33}, {41, 60}, {67, 89}, {95, 112}, {121, 141}, {147, 163}, {171, 192}, {200, 214}, {221, 238}}},
    {2, 67, 115, 111.0f, 29.0f, "sudden death", {{7, 20}, {26, 44}, {50, 66}, {71, 87}, {94, 109}, {114, 131}, {150, 166}, {173, 187}, {192, 207}, {212, 226}, {231, 247}}, true},
    {2, 132, 192, 176.0f, 29.0f, "unlimited play", {{7, 23}, {29, 44}, {49, 55}, {61, 65}, {71, 96}, {102, 107}, {112, 125}, {130, 144}, {149, 164}, {182, 197}, {202, 208}, {213, 226}, {231, 250}}, true},
    {2, 195, 243, 238.0f, 29.0f, "unlocks", {{13, 36}, {42, 63}, {69, 77}, {84, 102}, {108, 125}, {131, 153}, {160, 176}}},
};
constexpr float kRefXh = 30.f;   // piece units: x-height = kRefXh
constexpr float kWordGap = 14.f;

static Texture2D g_tex[3] = {};
static bool g_tried = false, g_ok = false;

// Window thread
static bool load()
{
    if (g_tried) return g_ok;
    g_tried = true;
    const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    std::vector<uint8_t> tex, rtx;
    if (!wotm::disc::readFile(io.cdRoot, io.cdImage.string(), "SHELL/SHELL.TEX", tex) ||
        !wotm::disc::readFile(io.cdRoot, io.cdImage.string(), "SHELL/SHELL.RTX", rtx) || !wotm::model::unzipIe(tex) || !wotm::model::unzipIe(rtx))
    {
        std::fprintf(stderr, "[menufont] SHELL.TEX/RTX not read: the list stays in the uppercase font\n");
        return false;
    }
    for (int i = 0; i < 3; ++i)
    {
        wotm::model::TexImage im;
        if (!wotm::model::decodeT4(tex, rtx, kTexIds[i], kPalTbp[i], im) || im.w != 256 || im.h != 256)
        {
            std::fprintf(stderr, "[menufont] SHELL.TEX texture %d not read: the list stays in the uppercase font\n", kTexIds[i]);
            for (int j = 0; j < i; ++j) UnloadTexture(g_tex[j]);
            return false;
        }
        // stored bottom-up: the table is in reading order (raylib's ImageFlipVertical would free the vector's buffer)
        for (int y = 0; y < im.h / 2; ++y)
            std::swap_ranges(im.rgba.begin() + y * im.w, im.rgba.begin() + (y + 1) * im.w, im.rgba.begin() + (im.h - 1 - y) * im.w);
        Image img{im.rgba.data(), im.w, im.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        g_tex[i] = LoadTextureFromImage(img);
        SetTextureFilter(g_tex[i], TEXTURE_FILTER_BILINEAR);
    }
    std::fprintf(stderr, "[menufont] menu words use the game's letters (SHELL.TEX 81/142/162)\n");
    return g_ok = true;
}

struct Piece
{
    int tex;
    Rectangle src;   // in the texture (negative width: mirrored)
    float x, top, w, h;   // in kRefXh units; top is relative to the baseline
};

static int letters(const Line &l) { int n = 0; for (const char *c = l.text; *c; ++c) n += *c != ' '; return n; }

// Cut of letters a..b (indices not counting spaces) from a line
static Rectangle cut(const Line &l, int a, int b)
{
    const int n = letters(l);
    const float x0 = a > 0 ? std::max((l.seg[a - 1][1] + l.seg[a][0]) * 0.5f, l.seg[a][0] - 4.f) : l.seg[a][0] - 4.f;
    const float x1 = b + 1 < n ? std::min((l.seg[b][1] + l.seg[b + 1][0]) * 0.5f, l.seg[b][1] + 4.f) : l.seg[b][1] + 4.f;
    return Rectangle{x0, float(l.y0), x1 - x0, float(l.y1 - l.y0)};
}

// Builds `s` (lowercase, '-' and space); false if some letter does not exist
static bool compose(const char *s, std::vector<Piece> &out, float &width)
{
    out.clear();
    float x = 0.f;
    for (size_t k = 0; s[k];)
    {
        if (s[k] == ' ') { x += kWordGap; ++k; continue; }
        int bestL = -1, bestA = 0, bestN = 0;
        // normal-width lines first; "sudden death" and "unlimited play" (phrases the game squeezed: their e is 14 px, 17 in the
        // others) only for a letter that does not exist in those
        for (int pass = 0; pass < 2 && bestL < 0; ++pass)
        for (int li = 0; li < int(std::size(kLines)); ++li)
        {
            if (kLines[li].narrow != (pass == 1)) continue;
            // pieces from a single word of the line; a = letter index not counting spaces
            const char *t = kLines[li].text;
            int idx = 0;
            for (size_t i = 0; t[i]; ++i)
            {
                if (t[i] == ' ') continue;
                int n = 0;
                while (t[i + n] && t[i + n] != ' ' && s[k + n] && s[k + n] == t[i + n]) ++n;
                if (n > bestN) { bestL = li; bestA = idx; bestN = n; }
                ++idx;
            }
        }
        bool mirror = false;
        if (bestL < 0 && s[k] == 'q')   // q: the p of "options", mirrored
        {
            bestL = 3; bestA = 1; bestN = 1; mirror = true;
        }
        if (bestL < 0) return false;
        const Line &l = kLines[bestL];
        Rectangle r = cut(l, bestA, bestA + bestN - 1);
        const float sc = kRefXh / l.xh;
        out.push_back(Piece{l.tex, r, x, (float(l.y0) - l.base) * sc, r.width * sc, r.height * sc});
        if (mirror) out.back().src.width = -r.width;
        x += r.width * sc;
        k += size_t(bestN);
    }
    width = x;
    return !out.empty();
}

// Width of `s` (in kRefXh units); < 0 if it cannot be built
static float width(const char *s)
{
    std::vector<Piece> p;
    float w = 0.f;
    return load() && compose(s, p, w) ? w : -1.f;
}

// Draws `s` centered on cx, baseline at `base`, x-height `xh` (target pixels); `c` already in raylib RGB
static bool draw(const char *s, float cx, float base, float xh, Color c)
{
    std::vector<Piece> p;
    float w = 0.f;
    if (!load() || !compose(s, p, w)) return false;
    const float sc = xh / kRefXh, x0 = cx - w * sc * 0.5f;
    for (const Piece &pc : p)
        DrawTexturePro(g_tex[pc.tex], pc.src, Rectangle{x0 + pc.x * sc, base + pc.top * sc, pc.w * sc, pc.h * sc}, Vector2{0, 0}, 0.f, c);
    return true;
}
} // namespace wotm_menufont
