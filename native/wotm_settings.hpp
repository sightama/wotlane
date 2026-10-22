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
    kKeys
};

inline constexpr const char *kEnv[kKeys] = {"WOTM_RES",        "WOTM_ASPECT",    "WOTM_FULLSCREEN", "WOTM_HZ",        "WOTM_VSYNC",
                                            "WOTM_LOCKTOGGLE", "WOTM_ENEMYBARS", "WOTM_DAMAGEPOP",  "WOTM_KBSEPARATE"};
inline constexpr int kDefault[kKeys] = {720, 0, 0, 0, 1, 0, 0, 0, 0};

// Escolhas que o painel percorre
inline constexpr int kResolutions[] = {448, 720, 1080, 1440, 2160};
inline constexpr int kRates[] = {0, 60, 120, 144, 165, 180, 240};
inline constexpr const char *kAspectNames[] = {"dynamic", "4:3", "16:9", "16:10", "21:9", "32:9"};
inline constexpr float kAspectRatio[] = {0.f, 4.f / 3.f, 16.f / 9.f, 16.f / 10.f, 21.f / 9.f, 32.f / 9.f};
inline constexpr int kAspects = 6;

inline std::atomic<int> g_val[kKeys] = {720, 0, 0, 0, 1, 0, 0, 0, 0};
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
    default: out = (*s && std::strcmp(s, "0") != 0) ? 1 : 0; return true;
    }
}

inline std::string format(Key k, int v)
{
    if (k == Aspect) return kAspectNames[v > 0 && v < kAspects ? v : 0];
    return std::to_string(v);
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
            }
        }
        fromEnv = applyEnv();
        std::fprintf(stderr, "[settings] %s: %d do arquivo, %d do ambiente\n", useFile ? g_path.string().c_str() : "arquivo ignorado (WOTM_SETTINGS=0)",
                     fromFile, fromEnv);
    });
}
} // namespace wotm::settings
