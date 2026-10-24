// Modo treino (POS_FRONTEIRA.md, item 9): estado compartilhado entre a thread do EE (ganchos em native/runtime/wotm_training.inc,
// incluido pelo wotm_overrides.cpp; captura dos volumes no wotmCaptureFrame do wotm_live.inc) e a janela (desenho e menu F2 no
// wotm_live.inc). Guia em docs/passos/NATIVO.md, "Modo treino"; investigacoes em docs/investigacoes/2026-10-23/treino_*.md.
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace wotm_training
{
// opcoes (a janela escreve, a thread do EE le)
constexpr uint32_t kShowAttacks = 1u << 0;   // esferas testadas pelos golpes
constexpr uint32_t kShowHulls = 1u << 1;     // cascos de colisao do corpo (o que os golpes acertam)
constexpr uint32_t kShowCapsule = 1u << 2;   // capsula de empurrao monstro x monstro
constexpr uint32_t kRefill = 1u << 3;        // vida volta cheia depois de um tempo sem dano e ninguem morre
constexpr uint32_t kPanel = 1u << 4;         // painel de dano
constexpr uint32_t kDefault = kShowAttacks | kShowHulls | kRefill | kPanel;

inline std::atomic<bool> g_on{false};                 // WOTM_TRAINING=1 ou F2
inline std::atomic<uint32_t> g_opts{kDefault};
inline std::atomic<int> g_dummy{0};                   // boneco (as IAs): 0 parado, 1 CPU (a IA do jogo)
inline std::atomic<bool> g_resetStats{false};

// Tipo do teste de golpe que gravou a esfera
enum Kind : uint8_t { KPunch, KBat, KGetup, KWeapon, KShock, KOther };

// Uma esfera varrida (de a ate b, raio r) que o jogo testou num quadro; a == b na esfera parada
struct Sweep
{
    float a[3], b[3];
    float r;
    uint32_t frame;   // g_frame
    uint8_t kind;
    int8_t owner;     // slot em m_monsters (-1 desconhecido)
    uint8_t hit;      // 0 nada, 1 cenario, 2 objeto ou monstro
};

// Um dano aplicado (Monster::takeDamage): vida antes e depois
struct Hit
{
    uint32_t frame;
    int8_t victim, attacker;   // slots (-1 desconhecido)
    float base;                // dano pedido ao takeHit (antes do bloqueio e dos modificadores); < 0 sem takeHit (fogo, queda...)
    float applied;             // vida antes - depois
    float before, max;
    int source;                // HitEvent::source do takeHit (-1 sem)
};

// Um volume do corpo em mundo: segmentos (pares de pontos) de um casco, ou uma capsula
struct Hull
{
    int8_t owner;
    std::vector<float> seg;   // x0 y0 z0 x1 y1 z1 ...
};
struct Capsule
{
    int8_t owner;
    float a[3], b[3], r;
};

struct Shared
{
    std::mutex m;
    std::vector<Sweep> sweeps;   // acumuladas; a janela descarta as velhas
    std::vector<Hit> hits;       // idem
    std::vector<Hull> hulls;     // do ultimo quadro capturado
    std::vector<Capsule> caps;
    uint32_t frame = 0;          // g_frame da ultima captura
    float health[16] = {}, maxHealth[16] = {};
    int8_t players = 1;          // m_numPlayers (slots 0..players-1 sao jogadores)
};
inline Shared &shared()
{
    static Shared s;
    return s;
}
}
