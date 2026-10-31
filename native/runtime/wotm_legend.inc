// wotm_legend.inc: draws the bottom legend and the big hint that wotm_padsel.inc and wotm_mainmenu.inc publish (wotm_legend::post
// and hint, in wotm_prompts.inc). Included by wotm_live.inc after wotm_optscreen.inc and called each frame, before the game's
// fades, in frame pixels (rw x rh).
//   - Legend: a black strip over the game's own (its legend, "X SELECT  TRIANGULO BACK", disappears behind it) with 2D icons from the
//     Kenney atlas and the text in the game font, centered like the game's.
//   - Hint: the text of the selected panel option, in the game font, flat on the screen, over the cars below the big screen (the panel camera
//     leaves that strip clear: wotm_mainmenu::frameCamera).
namespace wotm_legend
{
constexpr float kBarTop = 0.893f;   // the menu's bottom black strip (48 of the PS2's 448 lines), as a fraction of the height
constexpr float kBarMid = 0.929f;   // where the game centers its icons

static void drawLegend(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0, float rw, float rh)
{
    const int n = g_n;
    g_n = 0;
    const float a = std::clamp(g_alpha, 0.f, 1.f);
    if (!n || a <= 0.f)
        return;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(rw, rh);
    DrawRectangleRec(Rectangle{0.f, rh * kBarTop, rw, rh * (1.f - kBarTop)},
                     Color{0, 0, 0, static_cast<unsigned char>(255.f * a * std::clamp(g_barAlpha, 0.f, 1.f))});
    if (!tex0)
        return;
    const int style = wotm_prompts::currentStyle();
    float S = rh * 0.05f, th = rh * 0.022f;
    const float cy = rh * kBarMid;
    float iconGap = S * 0.12f, textGap = S * 0.35f, itemGap = S * 1.2f;
    const int irw = int(rw), irh = int(rh);
    // cell of an icon in the current style (-1: no cell; the key name goes in a frame)
    auto cellOf = [&](const Icon &ic) {
        if (style == st::PromptKeyboard && ic.key)
            return wotm_prompts::keyCell(ic.key);
        const int c = wotm_prompts::padCell(ic.pad, style == st::PromptKeyboard ? st::PromptPs : style);
        return c >= 0 ? c : wotm_prompts::keyCell(ic.key);
    };
    auto boxed = [&](const Icon &ic) { return cellOf(ic) < 0 && ic.key && (style == st::PromptKeyboard || ic.pad <= 0); };
    auto iconW1 = [&](const Icon &ic) {
        if (boxed(ic)) return wotm_mainmenu::textWidth(tex0, irw, irh, ic.key, th) + S * 0.5f;
        return cellOf(ic) >= 0 ? S : 0.f;
    };
    // the button's second key (only when the legend shows keys): side by side with the first
    auto second = [&](const Icon &ic) { return ic.key2 && (style == st::PromptKeyboard || ic.pad <= 0) ? Icon{0, ic.key2} : Icon{}; };
    auto iconW = [&](const Icon &ic) {
        const float w1 = iconW1(ic), w2 = iconW1(second(ic));
        return w2 > 0.f ? w1 + (w1 > 0.f ? iconGap : 0.f) + w2 : w1;
    };
    float w[kMax], total = 0.f;
    auto measure = [&] {
        total = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const Item &it = g_items[i];
            const float ia = iconW(it.a), ib = iconW(it.b);
            w[i] = ia + (ib > 0.f ? iconGap + ib : 0.f);
            if (it.text && *it.text) w[i] += (w[i] > 0.f ? textGap : 0.f) + wotm_mainmenu::textWidth(tex0, irw, irh, it.text, th);
            total += w[i] + (i ? itemGap : 0.f);
        }
    };
    measure();
    // many items (photo mode): shrink everything until it fits in the window, with some margin at the ends
    if (const float maxW = rw * 0.96f; total > maxW)
    {
        const float k = maxW / total;
        S *= k;
        th *= k;
        iconGap *= k;
        textGap *= k;
        itemGap *= k;
        measure();
    }
    // text in the game legend's color (light gray), a little lighter for readability
    const Color tc{0x5C, 0x5A, 0x54, static_cast<unsigned char>(0x80 * a)};
    std::vector<wotm::TextQuad> q;
    auto drawIcon1 = [&](const Icon &ic, float x) {
        const float iw = iconW1(ic);
        if (iw <= 0.f) return x;
        if (boxed(ic))
        {
            const Color line = wotm_mainmenu::gsToRgb(tc);
            DrawRectangleRoundedLinesEx(Rectangle{x, cy - S * 0.42f, iw, S * 0.84f}, 0.35f, 6, std::max(1.5f, S * 0.06f), line);
            wotm_padsel::text(q, ram, tex0, irw, irh, ic.key, x + iw * 0.5f, cy, th, tc);
        }
        else
            wotm_prompts::drawCell2d(cellOf(ic), x + S * 0.5f, cy, S, a);
        return x + iw;
    };
    auto drawIcon = [&](const Icon &ic, float x) {
        const float x0 = x;
        x = drawIcon1(ic, x);
        const Icon k = second(ic);
        if (iconW1(k) > 0.f) x = drawIcon1(k, x > x0 ? x + iconGap : x);
        return x;
    };
    float x = std::max(rw * 0.02f, gr.x0 + gr.w * 0.5f - total * 0.5f);
    for (int i = 0; i < n; ++i)
    {
        const Item &it = g_items[i];
        const float x0 = x;
        x = drawIcon(it.a, x);
        if (iconW(it.b) > 0.f) x = drawIcon(it.b, x + iconGap);
        if (it.text && *it.text)
        {
            if (x > x0) x += textGap;
            const float tw = wotm_mainmenu::textWidth(tex0, irw, irh, it.text, th);
            wotm_padsel::text(q, ram, tex0, irw, irh, it.text, x + tw * 0.5f, cy, th, tc);
            x += tw;
        }
        x = x0 + w[i] + itemGap;
    }
    scene.drawQuads(q, rw, rh);
}

// Wraps `s` into at most two lines of at most `maxChars` characters (the game font is monospaced), at the space closest to the middle
static int wrap(const std::string &s, size_t maxChars, std::string out[2])
{
    if (s.size() <= maxChars)
    {
        out[0] = s;
        return 1;
    }
    size_t best = std::string::npos;
    for (size_t i = s.find(' '); i != std::string::npos; i = s.find(' ', i + 1))
        if (best == std::string::npos || std::abs(int(i) - int(s.size() / 2)) < std::abs(int(best) - int(s.size() / 2)))
            best = i;
    if (best == std::string::npos)
    {
        out[0] = s;
        return 1;
    }
    out[0] = s.substr(0, best);
    out[1] = s.substr(best + 1);
    return 2;
}

// The hint in the game font (the same as the menu list), centered on the strip over the cars, with a dark strip behind it
static void drawHint(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0, float rw, float rh)
{
    const float a = std::clamp(g_hintAlpha, 0.f, 1.f);
    const std::string s = g_hint;
    g_hint.clear();
    g_hintAlpha = 0.f;
    if (s.empty() || a <= 0.f || !tex0)
        return;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(rw, rh);
    const int irw = int(rw), irh = int(rh);
    const float maxW = gr.w * 0.86f;
    float th = rh * 0.03f;
    const float adv = wotm_mainmenu::glyphAdvance(irw, irh, th);
    std::string line[2];
    const int n = wrap(s, size_t(std::max(8.f, maxW / adv)), line);
    float wmax = 0.f;
    for (int i = 0; i < n; ++i) wmax = std::max(wmax, wotm_mainmenu::textWidth(tex0, irw, irh, line[i].c_str(), th));
    if (wmax > maxW)   // line too long even when wrapped: smaller letters
    {
        th *= maxW / wmax;
        wmax = maxW;
    }
    const float lineH = th * 1.9f, cy = rh * 0.815f, top = cy - lineH * float(n) * 0.5f;
    const float padX = th * 1.6f, padY = th * 0.5f;
    DrawRectangleRounded(Rectangle{gr.x0 + gr.w * 0.5f - wmax * 0.5f - padX, top - padY, wmax + padX * 2.f, lineH * float(n) + padY * 2.f}, 0.35f, 8,
                         Color{0, 0, 0, static_cast<unsigned char>(160.f * a)});
    std::vector<wotm::TextQuad> q;
    const Color shadow{0, 0, 0, static_cast<unsigned char>(0x70 * a)}, fill{121, 118, 111, static_cast<unsigned char>(0x80 * a)};   // GS scale
    for (int i = 0; i < n; ++i)
    {
        const float x = gr.x0 + gr.w * 0.5f, y = top + lineH * (float(i) + 0.5f);
        wotm_padsel::text(q, ram, tex0, irw, irh, line[i].c_str(), x + th * 0.08f, y + th * 0.1f, th, shadow);
        wotm_padsel::text(q, ram, tex0, irw, irh, line[i].c_str(), x, y, th, fill);
    }
    scene.drawQuads(q, rw, rh);
}

static void draw(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0, int rw, int rh)
{
    drawHint(scene, ram, tex0, float(rw), float(rh));
    drawLegend(scene, ram, tex0, float(rw), float(rh));
}
} // namespace wotm_legend
