// wotm_settings.hpp: opcoes do jogador que valem ao vivo (painel "recompiled options" do menu principal, wotm_mainmenu.inc). O launcher
// nao as tem: so tira as variaveis do ambiente ao abrir o jogo.
//
// Arquivo: settings.ini ao lado do save.json (a pasta acima do mc0/, a do ELF: discroot\ no run_live), uma linha CHAVE=valor por
// opcao, com o nome da variavel de ambiente de cada uma. Ordem na abertura: padrao (o de antes, sem variavel), depois o arquivo,
// depois as variaveis de ambiente que estiverem definidas (testes e lotes); WOTM_SETTINGS=0 ignora o arquivo (nem le nem grava).
// O painel grava o arquivo inteiro com os valores do momento a cada mudanca.
//
// Cada valor e um std::atomic<int>: a thread do EE (ganchos dos mods, painel) e a da janela (video) leem a cada uso.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace wotm::settings
{
enum Key
{
    Res,          // altura do quadro nativo (linhas)
    Aspect,       // 0 = dinamica (segue a janela); 1.. = kAspectNames[i]
    Display,      // 0 janela, 1 tela cheia sem borda, 2 tela cheia exclusiva
    Rate,         // Hz da apresentacao; 0 = a do monitor
    Vsync,        // 0/1
    LockToggle,   // QoL: lock-on alternado no R1
    EnemyBars,    // QoL: barras de vida e energia sobre os inimigos
    DamagePop,    // QoL: numeros de dano
    KbSeparate,   // teclado e um jogador a parte
    Prompts,      // icones de botao: kPromptNames (0 = automatico pelo ultimo dispositivo usado; 1 = os 3D do jogo)
    MenuConfirm,  // nos menus, com gamepad: 0 = o de baixo confirma (o jogo), 1 = o da direita confirma e o de baixo volta
    Camera,       // 0 = classica (a do jogo), 1 = moderna (orbital: analogico direito, controle relativo a tela; wotm_orbit.inc)
    CamRecenter,  // camera moderna: volta devagar para as costas do monstro depois de ~1,5 s sem mexer, com ele andando
    CloseCam,     // 1 = a camera de perto do jogo (vista unificada); 0 = nunca entra nela
    kKeys
};

inline constexpr const char *kEnv[kKeys] = {"WOTM_RES",        "WOTM_ASPECT",    "WOTM_FULLSCREEN", "WOTM_HZ",        "WOTM_VSYNC",
                                            "WOTM_LOCKTOGGLE", "WOTM_ENEMYBARS", "WOTM_DAMAGEPOP",  "WOTM_KBSEPARATE",
                                            "WOTM_PROMPTS",    "WOTM_MENUCONFIRM", "WOTM_CAMERA",    "WOTM_CAMRECENTER", "WOTM_CLOSECAM"};
inline constexpr int kDefault[kKeys] = {720, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1};
inline constexpr const char *kPromptNames[] = {"auto", "original", "keyboard", "xbox", "playstation", "nintendo"};
inline constexpr int kPromptStyles = 6;
enum PromptStyle { PromptAuto, PromptOriginal, PromptKeyboard, PromptXbox, PromptPs, PromptNintendo };
inline constexpr const char *kConfirmNames[] = {"bottom", "right"};

// Escolhas que o painel percorre
inline constexpr int kResolutions[] = {448, 720, 1080, 1440, 2160};
inline constexpr int kRates[] = {0, 60, 120, 144, 165, 180, 240};
inline constexpr const char *kAspectNames[] = {"dynamic", "4:3", "16:9", "16:10", "21:9", "32:9"};
inline constexpr float kAspectRatio[] = {0.f, 4.f / 3.f, 16.f / 9.f, 16.f / 10.f, 21.f / 9.f, 32.f / 9.f};
inline constexpr int kAspects = 6;

inline std::atomic<int> g_val[kKeys] = {720, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1};
inline std::filesystem::path g_path;   // vazio: sem arquivo (WOTM_SETTINGS=0 ou antes do load)
inline std::mutex g_fileMutex;

inline int get(Key k) { return g_val[k].load(std::memory_order_relaxed); }
inline bool on(Key k) { return get(k) != 0; }

// Proporcao fixa da opcao Aspect (0 = dinamica)
inline float aspectRatio()
{
    const int a = get(Aspect);
    return a > 0 && a < kAspects ? kAspectRatio[a] : 0.f;
}

// Texto -> valor, com as regras de cada variavel de antes (mesmo significado no arquivo e no ambiente)
inline bool parse(Key k, const char *s, int &out)
{
    if (!s) return false;
    while (*s == ' ') ++s;
    switch (k)
    {
    case Res:
    {
        const int v = std::atoi(s);
        if (v <= 0) return false;
        out = std::clamp(v, 224, 4320);
        return true;
    }
    case Aspect:
    {
        if (!*s || !std::strcmp(s, "dynamic") || !std::strcmp(s, "0")) { out = 0; return true; }
        float w = 0.f, h = 0.f;
        if (std::sscanf(s, "%f:%f", &w, &h) != 2 || w <= 0.f || h <= 0.f) return false;
        // a da lista mais proxima
        const float r = w / h;
        int best = 1;
        for (int i = 2; i < kAspects; ++i)
            if (std::fabs(kAspectRatio[i] - r) < std::fabs(kAspectRatio[best] - r)) best = i;
        out = best;
        return true;
    }
    case Display: out = *s == '2' ? 2 : (*s && *s != '0') ? 1 : 0; return true;
    case Rate: out = std::max(0, std::atoi(s)); return true;
    case Vsync: out = *s == '0' ? 0 : 1; return true;
    case KbSeparate: out = *s == '1' ? 1 : 0; return true;
    case Prompts:
    case MenuConfirm:
    {
        const int n = k == Prompts ? kPromptStyles : 2;
        for (int i = 0; i < n; ++i)
            if (!std::strcmp(s, k == Prompts ? kPromptNames[i] : kConfirmNames[i]))
            {
                // "playstation" saiu da lista (era igual ao original, com os face buttons do jogo): vira "original"
                out = k == Prompts && i == PromptPs ? int(PromptOriginal) : i;
                return true;
            }
        if (*s < '0' || *s > '9') return false;
        out = std::clamp(std::atoi(s), 0, n - 1);
        return true;
    }
    default: out = (*s && std::strcmp(s, "0") != 0) ? 1 : 0; return true;
    }
}

inline std::string format(Key k, int v)
{
    if (k == Aspect) return kAspectNames[v > 0 && v < kAspects ? v : 0];
    if (k == Prompts) return kPromptNames[std::clamp(v, 0, kPromptStyles - 1)];
    if (k == MenuConfirm) return kConfirmNames[v ? 1 : 0];
    return std::to_string(v);
}

// ------------------------------------------------------------------ mapa de teclas e botoes (aba CONTROLS do painel)
// Cada botao do PS2 recebe ate duas teclas do teclado e um botao de gamepad. Codigos do raylib (KeyboardKey, GamepadButton); 0 = nada.
// Os gamepads tem um mapa por jogador (kPadPlayers): o gamepad usa o do jogador que ele alimenta (wotm_pad::playerOfPad); o teclado
// tem um so. No arquivo: KB_<BOTAO>=TECLA[,TECLA], PAD_<BOTAO>=BOTAO (jogador 1) e PAD2_/PAD3_/PAD4_<BOTAO>=BOTAO, com os nomes de
// kKeyNames/kPadButtonNames. Sem variavel de ambiente.
enum Bind
{
    BUp, BDown, BLeft, BRight,                 // direcional
    BCross, BCircle, BSquare, BTriangle,
    BL1, BR1, BL2, BR2,
    BL1R1,                                     // so teclado: L1 e R1 juntos
    BStart, BSelect, BL3, BR3,
    BStickUp, BStickDown, BStickLeft, BStickRight,   // so teclado: analogico esquerdo
    kBinds
};
inline constexpr const char *kBindNames[kBinds] = {"UP",     "DOWN",   "LEFT",   "RIGHT",    "CROSS",     "CIRCLE",     "SQUARE",
                                                   "TRIANGLE", "L1",   "R1",     "L2",       "R2",        "L1_R1",      "START",
                                                   "SELECT", "L3",     "R3",     "STICK_UP", "STICK_DOWN", "STICK_LEFT", "STICK_RIGHT"};
// Bits do scePadRead (1 = o botao)
inline constexpr uint16_t kBindMask[kBinds] = {0x0010, 0x0040, 0x0080, 0x0020, 0x4000, 0x2000, 0x8000, 0x1000, 0x0400, 0x0800, 0x0100,
                                               0x0200, 0x0C00, 0x0008, 0x0001, 0x0002, 0x0004, 0,      0,      0,      0};
inline constexpr bool bindOnPad(Bind b) { return b != BL1R1 && b < BStickUp; }

struct KeyName { int code; const char *name; };
// Teclas que podem ser escolhidas (Esc cancela a escolha; F1..F12 sao do runtime: menu de debug, camera livre, fotos, tela cheia)
inline constexpr KeyName kKeyNames[] = {
    {32, "SPACE"}, {39, "APOSTROPHE"}, {44, "COMMA"}, {45, "MINUS"}, {46, "PERIOD"}, {47, "SLASH"},
    {48, "0"}, {49, "1"}, {50, "2"}, {51, "3"}, {52, "4"}, {53, "5"}, {54, "6"}, {55, "7"}, {56, "8"}, {57, "9"},
    {59, "SEMICOLON"}, {61, "EQUAL"},
    {65, "A"}, {66, "B"}, {67, "C"}, {68, "D"}, {69, "E"}, {70, "F"}, {71, "G"}, {72, "H"}, {73, "I"}, {74, "J"}, {75, "K"}, {76, "L"},
    {77, "M"}, {78, "N"}, {79, "O"}, {80, "P"}, {81, "Q"}, {82, "R"}, {83, "S"}, {84, "T"}, {85, "U"}, {86, "V"}, {87, "W"}, {88, "X"},
    {89, "Y"}, {90, "Z"},
    {91, "LBRACKET"}, {92, "BACKSLASH"}, {93, "RBRACKET"}, {96, "GRAVE"},
    {257, "ENTER"}, {258, "TAB"}, {259, "BACKSPACE"}, {260, "INSERT"}, {261, "DELETE"}, {262, "RIGHT"}, {263, "LEFT"}, {264, "DOWN"},
    {265, "UP"}, {266, "PGUP"}, {267, "PGDN"}, {268, "HOME"}, {269, "END"}, {280, "CAPSLOCK"},
    {320, "KP0"}, {321, "KP1"}, {322, "KP2"}, {323, "KP3"}, {324, "KP4"}, {325, "KP5"}, {326, "KP6"}, {327, "KP7"}, {328, "KP8"},
    {329, "KP9"}, {330, "KPDECIMAL"}, {331, "KPDIVIDE"}, {332, "KPMULTIPLY"}, {333, "KPMINUS"}, {334, "KPPLUS"}, {335, "KPENTER"},
    {340, "LSHIFT"}, {341, "LCTRL"}, {342, "LALT"}, {344, "RSHIFT"}, {345, "RCTRL"}, {346, "RALT"},
};
// GamepadButton do raylib 1..17, pela posicao no controle (os icones por estilo vem depois)
inline constexpr const char *kPadButtonNames[18] = {"",        "DPAD_UP", "DPAD_RIGHT", "DPAD_DOWN", "DPAD_LEFT", "FACE_UP",
                                                    "FACE_RIGHT", "FACE_DOWN", "FACE_LEFT", "L1",    "L2",        "R1",
                                                    "R2",      "BACK",    "GUIDE",      "START",     "L3",        "R3"};
inline constexpr int kPadButtons = 18;

// Padrao = o mapa fixo de antes (ps2_pad.cpp): I/J/K/L direcional, Espaco X, S bola, Z quadrado, X triangulo, Q L1, W R1,
// Shift L1+R1, A L2, C R2, Enter Start, Backspace Select, setas no analogico
inline constexpr uint32_t kbPair(int a, int b = 0) { return uint32_t(a) | (uint32_t(b) << 16); }
inline constexpr uint32_t kDefaultKb[kBinds] = {kbPair(73), kbPair(75), kbPair(74), kbPair(76), kbPair(32), kbPair(83), kbPair(90),
                                                kbPair(88), kbPair(81), kbPair(87), kbPair(65), kbPair(67), kbPair(340, 344), kbPair(257),
                                                kbPair(259), 0, 0, kbPair(265), kbPair(264), kbPair(263), kbPair(262)};
inline constexpr int kDefaultPad[kBinds] = {1, 3, 4, 2, 7, 6, 8, 5, 9, 11, 10, 12, 0, 15, 13, 16, 17, 0, 0, 0, 0};

inline std::atomic<uint32_t> g_kb[kBinds];
inline constexpr int kPadPlayers = 4;
inline std::atomic<int> g_pad[kPadPlayers][kBinds];
inline const bool g_bindsInit = [] {
    for (int b = 0; b < kBinds; ++b)
    {
        g_kb[b].store(kDefaultKb[b]);
        for (int p = 0; p < kPadPlayers; ++p) g_pad[p][b].store(kDefaultPad[b]);
    }
    return true;
}();
// Prefixo do mapa de gamepad do jogador p (0..3) no arquivo
inline std::string padPrefix(int p) { return p == 0 ? std::string("PAD_") : "PAD" + std::to_string(p + 1) + "_"; }

inline uint32_t kb(Bind b) { return g_kb[b].load(std::memory_order_relaxed); }
inline int pad(Bind b, int player = 0) { return g_pad[std::clamp(player, 0, kPadPlayers - 1)][b].load(std::memory_order_relaxed); }

inline const char *keyName(int code)
{
    for (const KeyName &k : kKeyNames)
        if (k.code == code) return k.name;
    return nullptr;
}
inline int keyCode(const std::string &name)
{
    for (const KeyName &k : kKeyNames)
        if (name == k.name) return k.code;
    return 0;
}
inline std::string formatKb(uint32_t v)
{
    std::string s;
    for (int i = 0; i < 2; ++i)
        if (const char *n = keyName(int((v >> (16 * i)) & 0xFFFF)))
            s += (s.empty() ? "" : ",") + std::string(n);
    return s;
}
inline uint32_t parseKb(const std::string &s)
{
    const size_t c = s.find(',');
    const int a = keyCode(s.substr(0, c)), b = c == std::string::npos ? 0 : keyCode(s.substr(c + 1));
    return a ? kbPair(a, b != a ? b : 0) : kbPair(b);
}
inline int parsePad(const std::string &s)
{
    for (int i = 1; i < kPadButtons; ++i)
        if (s == kPadButtonNames[i]) return i;
    return 0;
}

inline void save();
// Tecla nova para o botao b (a anterior sai); se ela era de outro botao, ele fica com a que b tinha (troca)
inline void bindKey(Bind b, int code)
{
    const uint32_t old = kb(b);
    for (int o = 0; o < kBinds; ++o)
    {
        if (o == b) continue;
        uint32_t v = kb(Bind(o));
        const int k1 = int(v & 0xFFFF), k2 = int(v >> 16);
        if (k1 != code && k2 != code) continue;
        const int give = int(old & 0xFFFF);
        const int n1 = k1 == code ? give : k1, n2 = k2 == code ? (k1 == code ? 0 : give) : k2;
        g_kb[o].store(n1 ? kbPair(n1, n2 != n1 ? n2 : 0) : kbPair(n2));
    }
    g_kb[b].store(kbPair(code));
    save();
}
// Botao novo do gamepad para o botao b no mapa do jogador `player` (troca com quem o tinha, como no teclado)
inline void bindPad(Bind b, int button, int player)
{
    player = std::clamp(player, 0, kPadPlayers - 1);
    const int old = pad(b, player);
    for (int o = 0; o < kBinds; ++o)
        if (o != b && pad(Bind(o), player) == button)
            g_pad[player][o].store(old);
    g_pad[player][b].store(button);
    save();
}
// `map`: -1 = teclado, 0..3 = gamepads do jogador
inline void clearBind(Bind b, int map)
{
    (map < 0 ? (void)g_kb[b].store(0) : (void)g_pad[std::clamp(map, 0, kPadPlayers - 1)][b].store(0));
    save();
}
inline void resetBinds(int map)
{
    for (int b = 0; b < kBinds; ++b)
        map < 0 ? g_kb[b].store(kDefaultKb[b]) : g_pad[std::clamp(map, 0, kPadPlayers - 1)][b].store(kDefaultPad[b]);
    save();
}

inline void save()
{
    std::lock_guard<std::mutex> lk(g_fileMutex);
    if (g_path.empty()) return;
    std::ofstream f(g_path, std::ios::trunc);
    if (!f)
    {
        std::fprintf(stderr, "[settings] nao consegui gravar %s\n", g_path.string().c_str());
        return;
    }
    f << "# Recompiled options (menu principal); mesma chave das variaveis de ambiente\n";
    for (int k = 0; k < kKeys; ++k)
        f << kEnv[k] << '=' << format(Key(k), get(Key(k))) << '\n';
    f << "# Teclas (KB_) e botoes de gamepad de cada botao do PS2 (PAD_ = jogador 1, PAD2_..PAD4_ = jogadores 2 a 4): aba CONTROLS\n";
    for (int b = 0; b < kBinds; ++b)
        f << "KB_" << kBindNames[b] << '=' << formatKb(kb(Bind(b))) << '\n';
    for (int p = 0; p < kPadPlayers; ++p)
        for (int b = 0; b < kBinds; ++b)
            if (bindOnPad(Bind(b)))
                f << padPrefix(p) << kBindNames[b] << '=' << kPadButtonNames[std::clamp(pad(Bind(b), p), 0, kPadButtons - 1)] << '\n';
}

inline void set(Key k, int v, bool write = true)
{
    g_val[k].store(v, std::memory_order_relaxed);
    if (write) save();
}

// Variaveis de ambiente por cima dos valores atuais; quantas valeram
inline int applyEnv()
{
    int n = 0;
    for (int k = 0; k < kKeys; ++k)
        if (const char *e = std::getenv(kEnv[k]))
            if (int v; parse(Key(k), e, v))
            {
                g_val[k].store(v);
                ++n;
            }
    return n;
}
// Ja na inicializacao estatica: quem nao passa pelo load (o replay --vu1cmp, sem os ganchos) fica com o ambiente, como antes
inline const int g_envAtStart = applyEnv();

// Uma vez, na instalacao dos ganchos (antes do primeiro quadro). `dir` = pasta do save.json.
inline void load(const std::filesystem::path &dir)
{
    static std::once_flag once;
    std::call_once(once, [&] {
        const char *off = std::getenv("WOTM_SETTINGS");
        const bool useFile = !(off && off[0] == '0');
        int fromFile = 0, fromEnv = 0;
        if (useFile)
        {
            g_path = dir / "settings.ini";
            std::ifstream f(g_path);
            std::string line;
            while (f && std::getline(f, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                const size_t eq = line.find('=');
                if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
                const std::string key = line.substr(0, eq), val = line.substr(eq + 1);
                for (int k = 0; k < kKeys; ++k)
                    if (key == kEnv[k])
                        if (int v; parse(Key(k), val.c_str(), v))
                        {
                            g_val[k].store(v);
                            ++fromFile;
                        }
                for (int b = 0; b < kBinds; ++b)
                {
                    if (key == std::string("KB_") + kBindNames[b])
                        g_kb[b].store(parseKb(val)), ++fromFile;
                    else if (bindOnPad(Bind(b)))
                        for (int p = 0; p < kPadPlayers; ++p)
                            if (key == padPrefix(p) + kBindNames[b])
                                g_pad[p][b].store(parsePad(val)), ++fromFile;
                }
            }
        }
        fromEnv = applyEnv();
        std::fprintf(stderr, "[settings] %s: %d do arquivo, %d do ambiente\n", useFile ? g_path.string().c_str() : "arquivo ignorado (WOTM_SETTINGS=0)",
                     fromFile, fromEnv);
    });
}
} // namespace wotm::settings
