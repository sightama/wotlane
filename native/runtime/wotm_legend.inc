// wotm_legend.inc: desenho da legenda de baixo e da dica grande que o wotm_padsel.inc e o wotm_mainmenu.inc publicam (wotm_legend::post
// e hint, no wotm_prompts.inc). Incluido por wotm_live.inc depois do wotm_optscreen.inc e chamado no quadro, antes dos
// fades do jogo, em pixels do quadro (rw x rh).
//   - Legenda: uma faixa preta por cima da do jogo (a legenda dele, "X SELECT  TRIANGULO BACK", some atras) com icones 2D do
//     atlas do Kenney e o texto na fonte do jogo, centrada como a dele.
//   - Dica: o texto da opcao selecionada do painel, na fonte do jogo, reto na tela, sobre os carros abaixo do telao (a camera do painel
//     deixa essa faixa livre: wotm_mainmenu::frameCamera).
namespace wotm_legend
{
constexpr float kBarTop = 0.893f;   // a faixa preta de baixo do menu (48 de 448 linhas do PS2), em fracao da altura
constexpr float kBarMid = 0.929f;   // onde o jogo centra os icones dela

static void drawLegend(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0, float rw, float rh)
{
    const int n = g_n;
    g_n = 0;
    const float a = std::clamp(g_alpha, 0.f, 1.f);
    if (!n || a <= 0.f)
        return;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(rw, rh);
    DrawRectangleRec(Rectangle{0.f, rh * kBarTop, rw, rh * (1.f - kBarTop)}, Color{0, 0, 0, static_cast<unsigned char>(255.f * a)});
    if (!tex0)
        return;
    const int style = wotm_prompts::currentStyle();
    const float S = rh * 0.05f, th = rh * 0.022f, cy = rh * kBarMid;
    const float iconGap = S * 0.12f, textGap = S * 0.35f, itemGap = S * 1.2f;
    const int irw = int(rw), irh = int(rh);
    // celula de um icone no estilo de agora (-1: sem celula; o nome da tecla vai numa moldura)
    auto cellOf = [&](const Icon &ic) {
        if (style == st::PromptKeyboard && ic.key)
            return wotm_prompts::keyCell(ic.key);
        const int c = wotm_prompts::padCell(ic.pad, style == st::PromptKeyboard ? st::PromptPs : style);
        return c >= 0 ? c : wotm_prompts::keyCell(ic.key);
    };
    auto boxed = [&](const Icon &ic) { return cellOf(ic) < 0 && ic.key && (style == st::PromptKeyboard || ic.pad <= 0); };
    auto iconW = [&](const Icon &ic) {
        if (boxed(ic)) return wotm_mainmenu::textWidth(tex0, irw, irh, ic.key, th) + S * 0.5f;
        return cellOf(ic) >= 0 ? S : 0.f;
    };
    float w[kMax], total = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const Item &it = g_items[i];
        const float ia = iconW(it.a), ib = iconW(it.b);
        w[i] = ia + (ib > 0.f ? iconGap + ib : 0.f);
        if (it.text && *it.text) w[i] += (w[i] > 0.f ? textGap : 0.f) + wotm_mainmenu::textWidth(tex0, irw, irh, it.text, th);
        total += w[i] + (i ? itemGap : 0.f);
    }
    // texto na cor da legenda do jogo (cinza claro), um pouco mais claro para ler
    const Color tc{0x5C, 0x5A, 0x54, static_cast<unsigned char>(0x80 * a)};
    std::vector<wotm::TextQuad> q;
    auto drawIcon = [&](const Icon &ic, float x) {
        const float iw = iconW(ic);
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
    float x = gr.x0 + gr.w * 0.5f - total * 0.5f;
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

// Quebra `s` em ate duas linhas de no maximo `maxChars` letras (a fonte do jogo e monoespacada), no espaco mais perto do meio
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

// A dica com a fonte do jogo (a mesma da lista do menu), centrada na faixa sobre os carros, com uma faixa escura atras
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
    if (wmax > maxW)   // linha longa demais mesmo quebrada: letras menores
    {
        th *= maxW / wmax;
        wmax = maxW;
    }
    const float lineH = th * 1.9f, cy = rh * 0.815f, top = cy - lineH * float(n) * 0.5f;
    const float padX = th * 1.6f, padY = th * 0.5f;
    DrawRectangleRounded(Rectangle{gr.x0 + gr.w * 0.5f - wmax * 0.5f - padX, top - padY, wmax + padX * 2.f, lineH * float(n) + padY * 2.f}, 0.35f, 8,
                         Color{0, 0, 0, static_cast<unsigned char>(160.f * a)});
    std::vector<wotm::TextQuad> q;
    const Color shadow{0, 0, 0, static_cast<unsigned char>(0x70 * a)}, fill{121, 118, 111, static_cast<unsigned char>(0x80 * a)};   // escala do GS
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
