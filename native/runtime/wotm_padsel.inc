// wotm_padsel.inc: escolha de lados do modo 2 jogadores, no telao do menu (incluido por wotm_live.inc, com o desenho nativo).
//
// Ao entrar no menu de 2 jogadores (tela 5, screenGameModes2P) vindo do menu principal, o telao mostra primeiro os controles
// conectados e o teclado, um por linha, no meio. Cada um anda para a esquerda (jogador 1) ou para a direita (jogador 2) com o
// proprio direcional; com os dois lados ocupados, X (Espaco/Enter no teclado) confirma, e so entao "free-for-all / elimination /
// minigames" aparecem (fade dos objetos da tela, como o screenTransition faz). Triangulo (X/Backspace no teclado) volta ao centro
// ou, de quem ja esta no centro, ao menu principal (o proprio jogo trata: o screenGetInput devolve o codigo 5).
// Depois de confirmado, o mapa porta -> dispositivos do wotm_pad.inc fica manual (wotm_pad::g_manual): a porta 0 le os dispositivos
// da esquerda e a 1 os da direita, ate o jogo voltar ao menu principal ou ao "press start". Varios dispositivos do mesmo lado sao o
// mesmo jogador (o teclado junto do controle 1, por exemplo); quem fica no meio nao joga.
//
// Thread do EE: ganchos de screenTransition (esconde os itens da tela 5 durante a escolha e mede o fade), screenGameModes2P (le os
// dispositivos, anda a escolha, revela os itens), screenGetInput (sem entrada do jogo durante a escolha; injeta o "voltar") e
// screenMain/screenWaitForStart (desliga o mapa manual). Janela: renderUi desenha a escolha numa textura propria (fonte do jogo,
// icones desenhados) e drawOnTelao a projeta no plano do objeto do filme do telao (shell->m_movieScreen), com a camera do quadro.
// Teste sem tela: WOTM_PADSEL_SCRIPT="seg:dur:disp:acao;..." (disp 0..3 = controle, kb = teclado; acao left, right, ok, back).
namespace wotm_padsel
{
constexpr int kKb = PS2_PAD_MAX_GAMEPADS;   // indice do teclado na lista de dispositivos
constexpr int kDevs = PS2_PAD_MAX_GAMEPADS + 1;
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr uint32_t kMenuObj5 = kShell + 0x6DC + 5 * 90 * 4;   // shell->m_menuObj[5][0..89]
constexpr uint32_t kMovieScreen = kShell + 0x160;              // shell->m_movieScreen
constexpr uint32_t kShellSound = kShell + 0x2918;
enum Phase { Off, Config, Reveal, Leaving };

// Estado que a janela desenha (copiado sob g_m)
struct View
{
    bool on = false;
    float alpha = 0.f;   // da escolha inteira (fade de entrada e saida)
    bool present[kDevs] = {};
    int side[kDevs] = {};   // -1 jogador 1, 0 meio, +1 jogador 2
    bool ready = false;     // os dois lados com alguem
    int count = 0;          // dispositivos presentes
    uint32_t screenObj = 0;
};
static std::mutex g_m;
static View g_view;

// Thread do EE
static Phase g_phase = Off;
static float g_uiAlpha = 0.f, g_reveal = 0.f;
static int g_side[kDevs] = {};
static bool g_prev[kDevs][4] = {};   // left, right, ok, back do quadro anterior (borda)
static bool g_primed = false;
static int g_inject = 0;   // codigo para o proximo screenGetInput (5 = voltar)
static PS2Runtime::RecompiledFunction g_origTransition = nullptr, g_origModes2P = nullptr, g_origGetInput = nullptr, g_origMain = nullptr,
                                      g_origWaitStart = nullptr;
static uint32_t g_transRa = 0, g_modesRa = 0;
static bool g_transPending = false, g_modesPending = false;

struct ScriptStep { double t0, dur; int dev, act; };
static std::vector<ScriptStep> g_script;

static void parseScript()
{
    const char *e = std::getenv("WOTM_PADSEL_SCRIPT");
    if (!e) return;
    std::string s(e);
    for (size_t pos = 0; pos < s.size();)
    {
        size_t end = s.find(';', pos);
        if (end == std::string::npos) end = s.size();
        const std::string item = s.substr(pos, end - pos);
        pos = end + 1;
        double t0 = 0, dur = 0;
        char dev[8] = {}, act[8] = {};
        if (std::sscanf(item.c_str(), "%lf:%lf:%7[^:]:%7s", &t0, &dur, dev, act) != 4) continue;
        const int d = std::strcmp(dev, "kb") == 0 ? kKb : std::atoi(dev);
        const int a = !std::strcmp(act, "left") ? 0 : !std::strcmp(act, "right") ? 1 : !std::strcmp(act, "ok") ? 2 : !std::strcmp(act, "back") ? 3 : -1;
        if (d >= 0 && d < kDevs && a >= 0) g_script.push_back({t0, dur, d, a});
    }
}

static bool present(int d) { return d == kKb || IsGamepadAvailable(d); }

// left, right, ok, back do dispositivo agora
static void readDevice(int d, bool out[4])
{
    if (d == kKb)
    {
        out[0] = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_J);
        out[1] = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_L);
        out[2] = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER);
        out[3] = IsKeyDown(KEY_X) || IsKeyDown(KEY_BACKSPACE);
    }
    else
    {
        const float lx = GetGamepadAxisMovement(d, GAMEPAD_AXIS_LEFT_X);
        out[0] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || lx < -0.6f;
        out[1] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || lx > 0.6f;
        out[2] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsGamepadButtonDown(d, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        out[3] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_RIGHT_FACE_UP);
    }
    const double now = GetTime();
    for (const ScriptStep &st : g_script)
        if (st.dev == d && now >= st.t0 && now < st.t0 + st.dur)
            out[st.act] = true;
}

static void callGuest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, uint32_t fn, uint32_t a0, uint32_t a1)
{
    const PS2Runtime::RecompiledFunction f = rt->lookupFunction(fn);
    if (!f) return;
    const R5900Context saved = *ctx;
    ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a0)));
    ctx->r[5] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a1)));
    ctx->r[31] = _mm_set_epi64x(0, 0);
    ctx->pc = fn;
    f(rdram, ctx, rt);
    *ctx = saved;
}
static void sfx(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, int sound)   // ShellSound::playShellSFXSound (2/4 cursor, 6 voltar, 8 ok)
{
    callGuest(rdram, ctx, rt, wotm::fn::playShellSFXSound__10ShellSoundi, kShellSound, static_cast<uint32_t>(sound));
}

static uint32_t rd32(const uint8_t *rdram, uint32_t a)
{
    uint32_t v;
    std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 4);
    return v;
}

static void setItemsAlpha(uint8_t *rdram, float a)
{
    for (uint32_t i = 0; i < 90; ++i)
        if (const uint32_t o = rd32(rdram, kMenuObj5 + 4 * i) & 0x1FFFFFFFu)
            std::memcpy(rdram + o + 0x4C, &a, 4);
}
static float itemsAlpha(uint8_t *rdram)
{
    for (uint32_t i = 0; i < 90; ++i)
        if (const uint32_t o = rd32(rdram, kMenuObj5 + 4 * i) & 0x1FFFFFFFu)
        {
            float a;
            std::memcpy(&a, rdram + o + 0x4C, 4);
            return a;
        }
    return 0.f;
}

static void publish(uint8_t *rdram)
{
    View v;
    v.on = g_phase != Off;
    v.alpha = std::min(1.f, std::max(0.f, g_uiAlpha));
    int n[3] = {0, 0, 0};
    for (int d = 0; d < kDevs; ++d)
    {
        v.present[d] = present(d);
        v.side[d] = g_side[d];
        if (v.present[d]) { ++v.count; ++n[g_side[d] + 1]; }
    }
    v.ready = n[0] > 0 && n[2] > 0;
    v.screenObj = rd32(rdram, kMovieScreen) & 0x1FFFFFFFu;
    std::lock_guard<std::mutex> lk(g_m);
    g_view = v;
}

static void setManual(bool on)
{
    if (on)
    {
        uint32_t devs[2] = {0, 0};
        for (int d = 0; d < kDevs; ++d)
            if (g_side[d])
                devs[g_side[d] > 0] |= d == kKb ? PS2_PAD_DEV_KEYBOARD : 1u << d;
        wotm_pad::g_manualDevs[0] = devs[0];
        wotm_pad::g_manualDevs[1] = devs[1];
    }
    if (wotm_pad::g_manual.exchange(on) != on)
        std::fprintf(stderr, "[padsel] lados %s\n", on ? "escolhidos: mapa manual" : "desligados: mapa padrao");
}

// screenTransition: entrada vinda do menu principal comeca a escolha; enquanto ela vale, os itens da tela 5 ficam com alfa 0 e o
// alfa que a transicao lhes daria vira o da escolha.
static void transitionHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenTransition__Fb)
    {
        const int cur = static_cast<int>(rd32(rdram, wotm::sym::currScreen)), last = static_cast<int>(rd32(rdram, wotm::sym::lastScreen));
        if (rd32(rdram, wotm::sym::screenFirstPass) && cur == 5 && last == 1)
        {
            g_phase = Config;
            g_primed = false;
            g_uiAlpha = 0.f;
            for (int d = 0; d < kDevs; ++d)
                if (!present(d)) g_side[d] = 0;
            std::fprintf(stderr, "[padsel] escolha de lados\n");
        }
        g_transRa = GPR_U32(ctx, 31);
        g_transPending = true;
    }
    g_origTransition(rdram, ctx, runtime);
    if (!g_transPending || ctx->pc != g_transRa)
        return;
    g_transPending = false;
    if (g_phase == Config || g_phase == Leaving)
    {
        // entrando: so na fase de fade-in (antes disso o alfa e o resto da tela anterior); saindo: o fade-out da tela 5
        const bool fading = g_phase == Config ? rd32(rdram, wotm::sym::fadingIn_2979) != 0 : rd32(rdram, wotm::sym::fadingOut_2980) != 0;
        const bool ended = rd32(rdram, wotm::sym::betweenScreens) == 0;
        g_uiAlpha = fading ? std::min(1.f, std::max(0.f, itemsAlpha(rdram))) : (g_phase == Config && ended ? 1.f : 0.f);
        setItemsAlpha(rdram, 0.f);
        if (g_phase == Leaving && rd32(rdram, wotm::sym::betweenScreens) == 0)
            g_phase = Off;
    }
    publish(rdram);
}

static void configStep(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    bool now[kDevs][4] = {};
    for (int d = 0; d < kDevs; ++d)
        if (present(d)) readDevice(d, now[d]);
    if (!g_primed)   // o X que escolheu "2 player" ainda pode estar apertado: borda so a partir de agora
    {
        std::memcpy(g_prev, now, sizeof g_prev);
        g_primed = true;
        return;
    }
    int moved = 0;
    bool ok = false, back = false;
    for (int d = 0; d < kDevs; ++d)
    {
        if (!present(d)) { g_side[d] = 0; continue; }
        const bool e[4] = {now[d][0] && !g_prev[d][0], now[d][1] && !g_prev[d][1], now[d][2] && !g_prev[d][2], now[d][3] && !g_prev[d][3]};
        const int before = g_side[d];
        if (e[0] && g_side[d] > -1) { --g_side[d]; moved = 2; }
        if (e[1] && g_side[d] < 1) { ++g_side[d]; moved = 4; }
        if (e[2] && g_side[d] != 0) ok = true;
        if (e[3])
        {
            if (g_side[d] != 0) { g_side[d] = 0; moved = 2; }
            else back = true;
        }
        if (g_side[d] != before)
            std::fprintf(stderr, "[padsel] %s %d: lado %d\n", d == kKb ? "teclado" : "controle", d == kKb ? 0 : d, g_side[d]);
    }
    std::memcpy(g_prev, now, sizeof g_prev);
    bool l = false, r = false;
    for (int d = 0; d < kDevs; ++d)
        if (present(d)) { l |= g_side[d] < 0; r |= g_side[d] > 0; }
    if (back)
    {
        g_phase = Leaving;
        g_inject = 5;   // o screenGameModes2P faz o resto (som, m_numPlayers 0, changeScreen de volta)
        std::fprintf(stderr, "[padsel] voltar ao menu principal\n");
        return;
    }
    if (ok && l && r)
    {
        g_phase = Reveal;
        g_reveal = 0.f;
        setManual(true);
        sfx(rdram, ctx, runtime, 8);
        for (int d = 0; d < kDevs; ++d)
            if (present(d) && g_side[d])
                std::fprintf(stderr, "[padsel] jogador %d: %s %d\n", g_side[d] < 0 ? 1 : 2, d == kKb ? "teclado" : "controle", d == kKb ? 0 : d);
        return;
    }
    if (moved)
        sfx(rdram, ctx, runtime, moved);
}

static void modes2PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenGameModes2P__Fv)
    {
        if (g_phase == Config)
        {
            g_uiAlpha = 1.f;
            configStep(rdram, ctx, runtime);
        }
        else if (g_phase == Reveal)
        {
            g_reveal = std::min(1.f, g_reveal + 0.05f);
            g_uiAlpha = 1.f - g_reveal;
            if (g_reveal >= 1.f) g_phase = Off;
        }
        g_modesRa = GPR_U32(ctx, 31);
        g_modesPending = true;
    }
    g_origModes2P(rdram, ctx, runtime);
    if (!g_modesPending || ctx->pc != g_modesRa)
        return;
    g_modesPending = false;
    if (g_phase == Config)
        setItemsAlpha(rdram, 0.f);
    else if (g_phase == Reveal)
        setItemsAlpha(rdram, g_reveal);
    else if (g_reveal > 0.f && g_reveal < 2.f)
    {
        setItemsAlpha(rdram, 1.f);
        g_reveal = 2.f;
    }
    publish(rdram);
}

static void getInputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenGetInput__Fi && rd32(rdram, wotm::sym::currScreen) == 5)
    {
        if (g_inject)
        {
            ctx->r[2] = _mm_set_epi64x(0, g_inject);
            g_inject = 0;
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if (g_phase == Config || g_phase == Reveal)
        {
            ctx->r[2] = _mm_set_epi64x(0, 0);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
    }
    g_origGetInput(rdram, ctx, runtime);
}

// Menu principal e "press start": a partida de 2 jogadores acabou de vez, os controles voltam ao mapa padrao
template <PS2Runtime::RecompiledFunction *Orig, uint32_t Entry>
static void resetHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == Entry && wotm_pad::g_manual.load(std::memory_order_relaxed))
        setManual(false);
    (*Orig)(rdram, ctx, runtime);
}

// ------------------------------------------------------------------ desenho (janela)

static Vector3 vadd(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vector3 vsub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vector3 vscale(Vector3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
static Vector3 vneg(Vector3 a) { return {-a.x, -a.y, -a.z}; }
static float vdot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float vlen(Vector3 a) { return std::sqrt(vdot(a, a)); }
static Vector3 vnorm(Vector3 a) { const float l = vlen(a); return l > 0.f ? vscale(a, 1.f / l) : a; }
static Vector3 vcross(Vector3 a, Vector3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

constexpr int kW = 1200, kH = 500;   // textura da escolha (2,4:1: cabe abaixo do logo do telao)
static RenderTexture2D g_rt{};
static float g_pos[kDevs] = {};   // posicao desenhada de cada dispositivo (-1..1, anda ate o lado)
static bool g_posInit = false;

// Texto com a fonte 0 do jogo (atlas do quadro, `tex0`), como o fontBuildPrim: glifos de tamanho fixo, `h` = altura em pixels da
// textura; cada letra tem 1,6 x a altura em unidades da tela do jogo (640 x 224), ou seja 0,8 x a altura na tela.
static void text(std::vector<wotm::TextQuad> &out, const wotm::Ram &ram, uint64_t tex0, int rw, int rh, const char *s, float cx, float cy, float h,
                 Color c, float spacing = 2.f)
{
    if (!tex0) return;
    const uint32_t f = wotm::FontText::kFonts;
    const float cellU = ram.u16(f) / 16.f, cellV = ram.u16(f + 2) / 16.f, gw = ram.u16(f + 4) / 16.f, gh = ram.u16(f + 6) / 16.f;
    if (gw <= 0.f || gh <= 0.f) return;
    const bool rtx = ram.u32(wotm::FontText::kAtlasMode) != 0;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const float sx = 640.f / gr.w, sy = 224.f / float(rh);   // pixel da textura -> unidade da tela do jogo
    const float hv = h * sy, wv = 1.6f * hv, adv = wv + spacing;
    const size_t n = std::strlen(s);
    float x = (cx - gr.x0) * sx - adv * float(n) * 0.5f;
    const float y = cy * sy - hv * 0.5f;
    for (size_t i = 0; i < n; ++i, x += adv)
    {
        int ch = static_cast<unsigned char>(s[i]);
        if (ch >= 'a' && ch <= 'z') ch -= 0x20;
        if (ch <= ' ') continue;
        int row, col;
        float base;
        if (rtx) { row = ch >> 4; col = ch & 15; base = 1.f; }
        else { const int a = ch - 0x21; row = a >> 3; col = a & 7; base = 0.5f; }
        wotm::TextQuad t{};
        t.u0 = cellU * col + base; t.v0 = cellV * row + base;
        t.u1 = t.u0 + gw; t.v1 = t.v0 + gh;
        t.x0 = x; t.y0 = y; t.x1 = x + wv; t.y1 = y + hv;
        t.r = c.r; t.g = c.g; t.b = c.b; t.a = c.a;
        t.tex0 = tex0;
        out.push_back(t);
    }
}

static void gamepadIcon(float cx, float cy, float s, Color body, Color line, Color dark)
{
    const float o = s * 0.07f;
    DrawCircleV(Vector2{cx - 0.62f * s, cy + 0.28f * s}, 0.42f * s + o, line);
    DrawCircleV(Vector2{cx + 0.62f * s, cy + 0.28f * s}, 0.42f * s + o, line);
    DrawRectangleRounded(Rectangle{cx - 1.f * s - o, cy - 0.45f * s - o, 2.f * s + 2 * o, 0.9f * s + 2 * o}, 0.6f, 12, line);
    DrawCircleV(Vector2{cx - 0.62f * s, cy + 0.28f * s}, 0.42f * s, body);
    DrawCircleV(Vector2{cx + 0.62f * s, cy + 0.28f * s}, 0.42f * s, body);
    DrawRectangleRounded(Rectangle{cx - 1.f * s, cy - 0.45f * s, 2.f * s, 0.9f * s}, 0.6f, 12, body);
    const float dx = cx - 0.58f * s, dy = cy - 0.05f * s, k = 0.11f * s, L = 0.36f * s;
    DrawRectangleRec(Rectangle{dx - L / 2, dy - k / 2, L, k}, dark);
    DrawRectangleRec(Rectangle{dx - k / 2, dy - L / 2, k, L}, dark);
    const float bx = cx + 0.58f * s, by = cy - 0.05f * s, r = 0.075f * s, d = 0.14f * s;
    DrawCircleV(Vector2{bx, by - d}, r, dark);
    DrawCircleV(Vector2{bx, by + d}, r, dark);
    DrawCircleV(Vector2{bx - d, by}, r, dark);
    DrawCircleV(Vector2{bx + d, by}, r, dark);
    DrawCircleV(Vector2{cx - 0.25f * s, cy + 0.25f * s}, 0.13f * s, dark);
    DrawCircleV(Vector2{cx + 0.25f * s, cy + 0.25f * s}, 0.13f * s, dark);
}

static void keyboardIcon(float cx, float cy, float s, Color body, Color line, Color dark)
{
    const float o = s * 0.07f, W = 2.3f * s, H = 0.95f * s;
    DrawRectangleRounded(Rectangle{cx - W / 2 - o, cy - H / 2 - o, W + 2 * o, H + 2 * o}, 0.25f, 8, line);
    DrawRectangleRounded(Rectangle{cx - W / 2, cy - H / 2, W, H}, 0.25f, 8, body);
    const float k = 0.2f * s, g = 0.06f * s;
    for (int row = 0; row < 3; ++row)
    {
        const int keys = 9 - (row == 2);
        const float rowW = keys * k + (keys - 1) * g, x0 = cx - rowW / 2, y = cy - H / 2 + 0.1f * s + row * (k + g);
        for (int i = 0; i < keys; ++i)
            DrawRectangleRec(Rectangle{x0 + i * (k + g), y, k, k}, dark);
    }
    DrawRectangleRec(Rectangle{cx - 0.65f * s, cy + H / 2 - 0.1f * s - k, 1.3f * s, k}, dark);
}

// Escolha desenhada na textura propria (antes do BeginTextureMode do quadro). `tex0` = atlas da fonte do quadro anterior.
static void renderUi(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    if (!v.on || v.alpha <= 0.f)
    {
        g_posInit = false;
        return;
    }
    constexpr float W = kW, H = kH;
    if (g_rt.id == 0)
    {
        g_rt = LoadRenderTexture(kW, kH);
        SetTextureFilter(g_rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    const float dt = std::min(0.1f, GetFrameTime());
    for (int d = 0; d < kDevs; ++d)
    {
        const float target = float(v.side[d]);
        g_pos[d] = g_posInit ? g_pos[d] + (target - g_pos[d]) * std::min(1.f, dt * 14.f) : target;
    }
    g_posInit = true;

    BeginTextureMode(g_rt);
    ClearBackground(Color{0, 0, 0, 0});
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    const float colX[3] = {W * 0.19f, W * 0.5f, W * 0.81f};
    const Color p1{230, 120, 50, 255}, p2{70, 140, 235, 255}, cream{240, 222, 180, 255}, line{25, 18, 12, 255}, dark{60, 48, 38, 255};
    // colunas dos jogadores
    const float boxTop = H * 0.02f, boxBottom = H * 0.82f;
    for (int s = 0; s < 2; ++s)
    {
        const Color c = s ? p2 : p1;
        const Rectangle box{colX[s * 2] - W * 0.15f, boxTop, W * 0.30f, boxBottom - boxTop};
        DrawRectangleRounded(box, 0.08f, 8, Color{0, 0, 0, 120});
        DrawRectangleRoundedLinesEx(box, 0.08f, 8, 5.f, Color{c.r, c.g, c.b, 220});
    }
    // dispositivos: uma linha cada, na ordem (controles, teclado), repartindo a altura das caixas abaixo do titulo
    std::vector<wotm::TextQuad> quads;
    const float listTop = H * 0.2f, listH = boxBottom - H * 0.03f - listTop;
    const float rowH = std::min(H * 0.3f, listH / float(std::max(1, v.count)));
    const float s = std::min(rowH * 0.36f, H * 0.1f);   // escala do icone
    int row = 0;
    for (int d = 0; d < kDevs; ++d)
    {
        if (!v.present[d]) continue;
        const float x = colX[1] + g_pos[d] * (colX[2] - colX[1]), y = listTop + rowH * (row + 0.42f);
        const Color body = v.side[d] < 0 ? p1 : v.side[d] > 0 ? p2 : cream;
        if (d == kKb) keyboardIcon(x, y, s * 0.92f, body, line, dark);
        else gamepadIcon(x, y, s, body, line, dark);
        char label[24];
        if (d == kKb) std::snprintf(label, sizeof label, "KEYBOARD");
        else std::snprintf(label, sizeof label, "CONTROLLER %d", d + 1);
        text(quads, ram, tex0, kW, kH, label, x, y + s * 1.05f, s * 0.55f, Color{0x80, 0x7A, 0x6C, 0x80}, 1.f);
        ++row;
    }
    text(quads, ram, tex0, kW, kH, "PLAYER 1", colX[0], H * 0.1f, H * 0.075f, Color{0x80, 0x50, 0x28, 0x80});
    text(quads, ram, tex0, kW, kH, "PLAYER 2", colX[2], H * 0.1f, H * 0.075f, Color{0x38, 0x68, 0x80, 0x80});
    const char *hint = v.count < 2 ? "CONNECT A CONTROLLER" : v.ready ? "PRESS X TO CONTINUE" : "LEFT OR RIGHT TO PICK A SIDE";
    const bool blink = v.ready && std::fmod(GetTime(), 1.0) > 0.6;
    if (!blink)
        text(quads, ram, tex0, kW, kH, hint, colX[1], H * 0.92f, H * 0.07f, Color{0x80, 0x7A, 0x60, 0x80});
    scene.drawQuads(quads, W, H);
    EndBlendMode();
    EndTextureMode();
}

// A escolha no plano do telao: retangulo do objeto do filme (shell->m_movieScreen) no mundo, abaixo do logo. Dentro do BeginMode3D
// do quadro, depois da cena.
static void drawOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    if (!v.on || v.alpha <= 0.f || g_rt.id == 0 || !v.screenObj)
        return;
    const wotm::DrawItem *it = nullptr;
    for (const wotm::DrawItem &i : scene.items)
        if ((i.node & 0x1FFFFFFFu) == v.screenObj) { it = &i; break; }
    const wotm::MeshData *md = it ? scene.mesh(ram, it->node) : nullptr;
    if (!md || md->tris.size() < 3)
        return;
    std::vector<Vector3> p(md->tris.size());
    for (size_t i = 0; i < p.size(); ++i)
    {
        float w[3];
        wotm::xform(it->m, md->tris[i].x, md->tris[i].y, md->tris[i].z, w);
        p[i] = Vector3{w[0], w[1], w[2]};
    }
    // normal do maior triangulo; "cima" = Z do mundo no plano; "direita" no sentido da direita da camera
    Vector3 n{0, 0, 0};
    float best = 0.f;
    for (size_t i = 0; i + 2 < p.size(); i += 3)
    {
        const Vector3 c = vcross(vsub(p[i + 1], p[i]), vsub(p[i + 2], p[i]));
        const float l = vlen(c);
        if (l > best) { best = l; n = vscale(c, 1.f / l); }
    }
    if (best <= 0.f) return;
    Vector3 c{0, 0, 0};
    for (const Vector3 &q : p) c = vadd(c, q);
    c = vscale(c, 1.f / float(p.size()));
    const Vector3 toCam = vsub(cam.position, c);
    if (vdot(n, toCam) < 0.f) n = vneg(n);
    Vector3 up = vsub(Vector3{0, 0, 1}, vscale(n, n.z));
    if (vlen(up) < 0.1f) return;
    up = vnorm(up);
    const Vector3 fwd = vnorm(vsub(cam.target, cam.position));
    const Vector3 camRight = vcross(fwd, cam.up);
    Vector3 right = vnorm(vcross(up, n));
    if (vdot(right, camRight) < 0.f) right = vneg(right);
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
    for (const Vector3 &q : p)
    {
        const Vector3 d = vsub(q, c);
        const float x = vdot(d, right), y = vdot(d, up);
        x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
    }
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        std::fprintf(stderr, "[padsel] telao: no %x, %zu vertices, %.1f x %.1f, centro %.1f %.1f %.1f\n", v.screenObj, p.size(), x1 - x0, y1 - y0, c.x, c.y,
                     c.z);
    }
    // area abaixo do logo (de 44% a 96% da altura), centrada, com a proporcao da textura
    const float fyTop = 0.44f, fyBottom = 0.96f;
    const float hy = (y1 - y0) * (fyBottom - fyTop), wx = std::min(hy * float(kW) / float(kH), (x1 - x0) * 0.92f);
    const float left = (x0 + x1) * 0.5f - wx * 0.5f, top = y1 - (y1 - y0) * fyTop;
    const Vector3 lift = vscale(n, 0.002f * (x1 - x0));
    auto at = [&](float x, float y) { return vadd(vadd(vadd(c, vscale(right, x)), vscale(up, y)), lift); };
    const Vector3 tl = at(left, top), tr = at(left + wx, top), bl = at(left, top - hy), br = at(left + wx, top - hy);
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    // sem profundidade: os itens da tela 5 com alfa 0 continuam gravando Z na frente do telao (e nada fica entre a area e a camera)
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(g_rt.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, static_cast<unsigned char>(v.alpha * 255.f));
    // textura de render: V invertido
    rlTexCoord2f(0, 1); rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(0, 0); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(1, 0); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(1, 1); rlVertex3f(tr.x, tr.y, tr.z);
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableDepthTest();
    EndBlendMode();
}
} // namespace wotm_padsel

void wotmPadSelInstall(PS2Runtime &runtime)
{
    using namespace wotm_padsel;
    parseScript();
    struct H { uint32_t addr; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; };
    const H hooks[] = {
        {wotm::fn::screenTransition__Fb, &g_origTransition, &transitionHook},
        {wotm::fn::screenGameModes2P__Fv, &g_origModes2P, &modes2PHook},
        {wotm::fn::screenGetInput__Fi, &g_origGetInput, &getInputHook},
        {wotm::fn::screenMain__Fv, &g_origMain, &resetHook<&g_origMain, wotm::fn::screenMain__Fv>},
        {wotm::fn::screenWaitForStart__Fv, &g_origWaitStart, &resetHook<&g_origWaitStart, wotm::fn::screenWaitForStart__Fv>},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
}
