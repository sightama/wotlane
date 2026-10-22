// wotm_optscreen.inc: a tela de opcoes do jogo (10) com o texto redesenhado (incluido por wotm_live.inc depois do wotm_mainmenu.inc,
// de quem usa os ganchos, o screenAlpha e o desenho no plano). So com o menu em texto (WOTM_TEXTMENU, o padrao).
//
// O jogo desenha a tela num quadro de madeira (titulo "OPTIONS", foto a esquerda) com as linhas em imagem: dificuldade, controles do
// jogador 1 e 2, audio, volume dos efeitos e da musica, vibracao do controle 1 e 2, "load settings" e "accept and save settings".
// No port, controles e vibracao sao do "Recompiled options" e o save e automatico; ficam dificuldade, audio, os dois volumes e
// "save settings and return". Aqui:
//   - a cada quadro da tela (saida do screenOptions e do screenTransition) os objetos das linhas do jogo ficam com alfa 0 (+0x4C):
//     os switches de cada item (rotulo apagado e o aceso com o brilho), os dos valores (dificuldade, estereo/mono, vibracao), os
//     cursores das barras (drawMe do shell->m_sliderCs) e os objetos soltos, achados a distancia fixa do switch de estereo (o banco do shell
//     e realocado inteiro: "press [] for more" e o icone das linhas de controle, as barras), conferindo que sao objetos;
//   - a lista e desenhada numa textura propria com a fonte Oswald (SIL OFL; recurso FONT_OSWALD do native/wotm.rc), no plano
//     do quadro, do lado direito da foto, com os valores lidos da RAM (TheGame+0x1203CC, shell->m_mono/m_sfxVolume/m_musicVolume);
//   - o gancho do screenGetInput(1) faz cima/baixo pular as linhas que sairam: antes de o screenOptions tratar o codigo, o
//     currentSelection[10] vai para a vizinha da linha de destino, e o +1/-1 do jogo cai nela (som, barra e o resto ficam com o jogo).
//     X confirma como no retail: nas linhas de valor sai sem gravar; no "save settings and return" grava e sai.
// Sem a fonte no executavel, fica a tela do jogo.
namespace wotm_optscreen
{
using wotm_padsel::rd32;
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr int kKeep[] = {0, 3, 4, 5, 9};   // itens do jogo que ficam, na ordem da lista
constexpr int kRows = int(std::size(kKeep));
constexpr uint32_t kStereoSw = kShell + 0x5F8;
constexpr int32_t kLoose[] = {0x20, 0x200, 0x490, 0x680, 0xD050};   // a partir do switch de estereo
constexpr int32_t kBoard = -0x40A0;                                   // o quadro (objeto)

struct View
{
    bool on = false;
    float alpha = 0.f;
    int sel = 0;          // linha (0..kRows-1)
    int difficulty = 0;   // 0..2
    bool mono = false;
    float sfx = 0.f, music = 0.f;
    uint32_t board = 0;
};
static std::mutex g_m;
static View g_view;
static std::atomic<bool> g_ready{false};   // fonte carregada (janela): so entao a tela do jogo e escondida

static uint16_t rd16(const uint8_t *rdram, uint32_t a) { uint16_t v; std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 2); return v; }

// Alfa 0 em todos os objetos abaixo de n (objeto, grupo, translacao, rotacao, escala, switch: todos os filhos)
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

// A cada quadro (saida do screenOptions e do screenTransition)
static void frame(uint8_t *rdram)
{
    uint32_t screen = 0;
    const float alpha = wotm_mainmenu::screenAlpha(rdram, screen);
    View v;
    if (screen == kScreen && g_ready.load(std::memory_order_relaxed))
    {
        for (int i = 0; i < 10; ++i) hideTree(rdram, rd32(rdram, kShell + 0x25C + (kScreen * 10 + uint32_t(i)) * 4), 0);
        for (uint32_t off = 0x5DC; off < 0x600; off += 4) hideTree(rdram, rd32(rdram, kShell + off), 0);   // dificuldade, vibracao, estereo, mono
        for (int i = 0; i < 2; ++i)
            if (const uint32_t cs = rd32(rdram, kShell + 0x604 + 4u * uint32_t(i)) & 0x1FFFFFFFu; cs >= 0x1000u && cs + 4 < PS2_RAM_SIZE)
                rdram[cs + 0xC] = 0;   // cursor da barra: drawMe do _cs (so aparece nesta tela)
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
                std::fprintf(stderr, "[options] objeto solto em switch de estereo %+d nao e objeto: fica o do jogo\n", off);
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

// screenGetInput(1) da tela 10, depois do jogo ler o codigo e antes do screenOptions trata-lo: cima/baixo pulam as linhas que sairam
static void input(uint8_t *rdram, uint32_t code)
{
    if (!g_ready.load(std::memory_order_relaxed) || (code != 1 && code != 2))
        return;
    const uint32_t selAddr = wotm::sym::currentSelection + 4 * kScreen;
    const int numSel = static_cast<int>(rd32(rdram, wotm::sym::numSelectables_2938));   // ultimo item (o jogo vai de 0 a ele)
    int r = rowOfItem(static_cast<int>(rd32(rdram, selAddr)));
    if (r < 0) r = 0;
    const int target = kKeep[(r + (code == 2 ? 1 : kRows - 1)) % kRows];
    // o jogo faz +1 (com volta de numSel para 0) ou -1 (com volta de 0 para numSel): parte da vizinha do destino
    int from = code == 2 ? target - 1 : target + 1;
    if (code == 2 && target == 0) from = numSel;
    if (code == 1 && target == numSel) from = 0;
    wotm_padsel::wr32(rdram, selAddr, static_cast<uint32_t>(from));
}

// ------------------------------------------------------------------ desenho (janela)
static Font g_font{};
static bool g_fontTried = false;
static const Font *font()
{
    if (!g_fontTried)
    {
        g_fontTried = true;
        void *mod = GetModuleHandleA(nullptr);
        void *res = FindResourceA(mod, "FONT_OSWALD", reinterpret_cast<const char *>(static_cast<uintptr_t>(10)));   // RT_RCDATA
        void *h = res ? LoadResource(mod, res) : nullptr;
        const unsigned char *data = h ? static_cast<const unsigned char *>(LockResource(h)) : nullptr;
        const int size = res ? static_cast<int>(SizeofResource(mod, res)) : 0;
        if (data && size > 0)
            g_font = LoadFontFromMemory(".ttf", data, size, 128, nullptr, 0);
        if (g_font.texture.id)
        {
            GenTextureMipmaps(&g_font.texture);
            SetTextureFilter(g_font.texture, TEXTURE_FILTER_TRILINEAR);
            g_ready.store(true, std::memory_order_relaxed);
        }
        else
            std::fprintf(stderr, "[options] fonte FONT_OSWALD nao esta no executavel: fica a tela de opcoes do jogo\n");
    }
    return g_font.texture.id ? &g_font : nullptr;
}

constexpr int kW = 1600, kH = 1000;   // textura: a parte do quadro a direita da foto, abaixo do titulo
constexpr float kRowY[kRows] = {0.10f, 0.27f, 0.44f, 0.61f, 0.87f};
constexpr float kFont = 104.f;
static RenderTexture2D g_rt{};

static void text(const Font &f, const char *s, float x, float cy, float size, Color c, bool center = false)
{
    const Vector2 sz = MeasureTextEx(f, s, size, 2.f);
    const Vector2 p{center ? x - sz.x * 0.5f : x, cy - sz.y * 0.5f};
    // entalhe: um claro meio pixel abaixo, como as letras pintadas no quadro
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
    // linha escolhida: faixa clara, como o brilho do jogo atras do item aceso
    {
        const float cy = kRowY[std::clamp(v.sel, 0, kRows - 1)] * H;
        DrawRectangleRounded(Rectangle{8.f, cy - H * 0.075f, W - 16.f, H * 0.15f}, 0.5f, 12, Color{255, 222, 160, 120});
    }
    static const char *const kLabels[kRows] = {"DIFFICULTY", "AUDIO", "FX VOLUME", "MUSIC VOLUME", "SAVE SETTINGS AND RETURN"};
    const float labX = 34.f;
    for (int r = 0; r < kRows - 1; ++r)
        text(*f, kLabels[r], labX, kRowY[r] * H, kFont, r == v.sel ? selInk : ink);
    text(*f, kLabels[kRows - 1], W * 0.5f, kRowY[kRows - 1] * H, kFont, kRows - 1 == v.sel ? selInk : ink, true);
    // valores: o atual em vermelho, os outros apagados
    static const char *const kDiff[3] = {"EASY", "MEDIUM", "HARD"};
    const float diffX[3] = {W * 0.49f, W * 0.69f, W * 0.89f};
    for (int i = 0; i < 3; ++i) text(*f, kDiff[i], diffX[i], kRowY[0] * H, kFont, i == v.difficulty ? red : dim, true);
    text(*f, "STEREO", W * 0.56f, kRowY[1] * H, kFont, v.mono ? dim : red, true);
    text(*f, "MONO", W * 0.80f, kRowY[1] * H, kFont, v.mono ? red : dim, true);
    // barras: trilho escuro, parte cheia vermelha e o cursor
    const float bx0 = W * 0.40f, bx1 = W * 0.96f, bh = H * 0.05f;
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

// No plano do quadro (dentro do BeginMode3D do quadro), a direita da foto e abaixo do titulo
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
}
} // namespace wotm_optscreen
