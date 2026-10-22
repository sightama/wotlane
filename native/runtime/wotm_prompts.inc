// wotm_prompts.inc: icones de botao 2D (Kenney "Input Prompts", CC0) no lugar dos objetos 3D do McButtons (X, bola, quadrado e
// triangulo da legenda dos menus e das paginas do memory card). Incluido por wotm_live.inc, depois do wotm_pad.inc e do
// wotm_native.hpp. O desenho nativo (Scene::drawButtonSprites) acha onde o jogo pos cada icone e chama icon() com o centro e o tamanho.
//
// Qual sprite: o botao do PS2 do icone vira o que o jogador aperta para ele, pelo mapa de teclas e botoes (wotm_settings.hpp, aba
// CONTROLS) e pela troca de confirmar dos menus (MenuConfirm, wotm_pad.inc). Estilo (opcao Prompts): automatico = o ultimo dispositivo
// usado (teclado: a tecla; gamepad: pelo nome dele, Xbox, PlayStation ou Nintendo), ou fixo; "original" deixa os 3D do jogo.
// O atlas (native/assets/prompts.png, tools/prompts_atlas.py) vem embutido nos executaveis (recurso WOTM_PROMPTS do native/wotm.rc).
#include "../wotm_prompts.h"

// Sem windows.h (briga com os nomes do raylib): so o que le um recurso do proprio executavel
extern "C" __declspec(dllimport) void *__stdcall GetModuleHandleA(const char *);
extern "C" __declspec(dllimport) void *__stdcall FindResourceA(void *module, const char *name, const char *type);
extern "C" __declspec(dllimport) void *__stdcall LoadResource(void *module, void *res);
extern "C" __declspec(dllimport) void *__stdcall LockResource(void *data);
extern "C" __declspec(dllimport) unsigned long __stdcall SizeofResource(void *module, void *res);

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
    void *mod = GetModuleHandleA(nullptr);
    void *res = FindResourceA(mod, "WOTM_PROMPTS", reinterpret_cast<const char *>(static_cast<uintptr_t>(10)));   // RT_RCDATA
    void *h = res ? LoadResource(mod, res) : nullptr;
    const unsigned char *data = h ? static_cast<const unsigned char *>(LockResource(h)) : nullptr;
    const int size = res ? static_cast<int>(SizeofResource(mod, res)) : 0;
    if (!data || size <= 0)
    {
        std::fprintf(stderr, "[prompts] atlas WOTM_PROMPTS nao esta no executavel: ficam os icones do jogo\n");
        return false;
    }
    Image img = LoadImageFromMemory(".png", data, size);
    if (!img.data)
    {
        std::fprintf(stderr, "[prompts] atlas WOTM_PROMPTS ilegivel: ficam os icones do jogo\n");
        return false;
    }
    g_atlas = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&g_atlas);
    SetTextureFilter(g_atlas, TEXTURE_FILTER_TRILINEAR);
    std::fprintf(stderr, "[prompts] atlas %dx%d\n", g_atlas.width, g_atlas.height);
    return true;
}

// Ultimo dispositivo usado (thread da janela, uma vez por quadro): teclado ou o gamepad n
static int g_lastPad = -1;   // -1 = teclado
static double g_lastPoll = -1.0;
static void pollDevice()
{
    const double now = GetTime();
    if (now == g_lastPoll) return;
    g_lastPoll = now;
    for (const st::KeyName &k : st::kKeyNames)
        if (IsKeyDown(k.code)) { g_lastPad = -1; return; }
    for (int p = 0; p < 4; ++p)
    {
        if (!IsGamepadAvailable(p)) continue;
        bool any = false;
        for (int b = 1; b < st::kPadButtons && !any; ++b) any = IsGamepadButtonDown(p, b);
        for (int a = 0; a < 4 && !any; ++a) any = std::fabs(GetGamepadAxisMovement(p, a)) > 0.5f;
        if (any) { g_lastPad = p; return; }
    }
    // sem uso ainda: o primeiro gamepad conectado, se houver
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

// Icone do McButtons -> botao do PS2. Indice: 0x1F51.. direto; 0x240E.. pela tabela do initButton. Ordem dos indices: a das letras do
// s_charMap do drawButtonsForString (' ^ ` $), que e a dos bits do scePadRead (triangulo 0x1000, bola 0x2000, X 0x4000, quadrado
// 0x8000). Conferido: "press the ' button to cancel" / "press the ` button to continue" no memory card; no Shell o 0x240F (SELECT)
// sai X e o 0x240E (BACK) triangulo.
static constexpr st::Bind kIndexBind[4] = {st::BTriangle, st::BCircle, st::BCross, st::BSquare};
static int buttonIndex(uint16_t id)
{
    if (id >= 0x1F51 && id <= 0x1F54) return id - 0x1F51;
    static constexpr int k240E[4] = {0, 2, 3, 1};
    if (id >= 0x240E && id <= 0x2411) return k240E[id - 0x240E];
    return -1;
}

// Celula do atlas para o botao do PS2 b, no estilo de agora
static int cellFor(st::Bind b)
{
    pollDevice();
    int style = st::get(st::Prompts);
    if (style == st::PromptAuto) style = g_lastPad < 0 ? st::PromptKeyboard : styleOfPad(g_lastPad);
    if (style == st::PromptKeyboard)
    {
        const uint32_t v = st::kb(b);
        if (const char *name = st::keyName(int(v & 0xFFFF)))
            for (const pr::KeyCell &k : pr::kKeys)
                if (!std::strcmp(k.name, name)) return k.cell;
        style = st::PromptPs;   // sem tecla: o simbolo do PS2
    }
    int phys = st::pad(wotm_pad::sourceBind(b));
    if (phys <= 0 || phys >= st::kPadButtons) return -1;
    const int *table = style == st::PromptPs ? pr::kPs : style == st::PromptNintendo ? pr::kNintendo : pr::kXbox;
    return table[phys];
}

static void icon(uint16_t id, const float mid[3], const float right[3], const float up[3], float size, float alpha)
{
    const int idx = buttonIndex(id);
    static uint32_t logged = 0;
    if (idx >= 0 && !(logged & (1u << idx)) && std::getenv("WOTM_MENULOG"))
    {
        logged |= 1u << idx;
        std::fprintf(stderr, "[prompts] icone id=0x%X indice=%d -> %s\n", unsigned(id), idx, st::kBindNames[kIndexBind[idx]]);
    }
    const int cell = idx >= 0 ? cellFor(kIndexBind[idx]) : -1;
    if (cell < 0 || !loadAtlas()) return;
    // a arte do Kenney tem folga na celula (o circulo ocupa ~3/4): o lado do quadrado e maior que o do icone do jogo
    const float h = size * 0.5f * (128.f / 96.f);
    const float u0 = float(cell % pr::kCols) / float(pr::kCols), v0 = float(cell / pr::kCols) / float(pr::kRows);
    const float u1 = u0 + 1.f / float(pr::kCols), v1 = v0 + 1.f / float(pr::kRows);
    auto corner = [&](float sr, float su, float out[3]) {
        for (int k = 0; k < 3; ++k) out[k] = mid[k] + right[k] * sr * h + up[k] * su * h;
    };
    float a[3], b[3], c[3], d[3];
    corner(-1, 1, a); corner(-1, -1, b); corner(1, -1, c); corner(1, 1, d);
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlSetTexture(g_atlas.id);
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

static const bool s_installed = (wotm::g_buttonIconFn = &icon, wotm::g_buttonIconsOn = &on, true);
} // namespace wotm_prompts
