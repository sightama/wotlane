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
constexpr uint32_t kMeters = 1u << 5;        // stamina cheia e especial liberado em todos
constexpr uint32_t kShowInputs = 1u << 6;    // historico dos comandos (P1 e o boneco)
constexpr uint32_t kDefault = kShowAttacks | kShowHulls | kRefill | kPanel | kMeters | kShowInputs;

// Boneco (as IAs): parado (pad desligado), CPU (a IA do jogo), sempre bloqueia, bloqueia depois do 1o acerto (ate se recuperar e
// ficar 1 s sem apanhar). Nos dois de bloqueio a IA nao roda: o gancho do Ai::updateInputs segura o R2 do GamePad.
enum Dummy : int { DStand, DCpu, DBlock, DBlockAfter, DCount };

inline std::atomic<bool> g_on{false};                 // WOTM_TRAINING=1 ou F2
inline std::atomic<uint32_t> g_opts{kDefault};
inline std::atomic<int> g_dummy{DStand};
inline std::atomic<int> g_dummyHealth{100};           // % da vida maxima a que a recarga leva o boneco
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

// Dados de quadro medidos (g_frame): um golpe do atacante (um estado de ataque, ou um passo do combo do StatePunch)
struct MoveRec
{
    int8_t owner;
    const char *name;     // nome do estado (tabela do game.h)
    int8_t chain, step;   // StatePunch/BatSwipe: cadeia e passo (configs do .CHR); -1 nos outros
    uint32_t start, end;  // quadros do estado (end exclusivo)
    int startup;          // quadro do primeiro teste de golpe, contando o primeiro do estado como 1 (0 = nao testou)
    int active;           // quadros com teste de golpe
    int8_t hitVictim;     // slot da primeira vitima com dano (-1 nenhuma)
};
// Uma reacao da vitima: quadros sem agir (recuo, atordoado, arremessado...)
struct StunRec
{
    int8_t victim;
    const char *name;
    uint32_t start, end;
};

// Combo: os golpes que uma vitima levou sem se recuperar (cada um chegou com ela ainda numa reacao, ou ate 3 quadros depois do outro)
struct Combo
{
    int8_t victim = -1, attacker = -1;
    int hits = 0;
    float damage = 0.f;
    uint32_t start = 0, last = 0;
};

// Comandos: uma entrada do historico (direcao e botoes iguais por `frames` quadros)
constexpr uint16_t kBtnL1 = 1, kBtnL2 = 2, kBtnR1 = 4, kBtnR2 = 8, kBtnX = 16, kBtnCircle = 32, kBtnTriangle = 64, kBtnSquare = 128;
struct PadRec
{
    uint8_t dir;      // notacao do teclado numerico, relativa ao monstro: 8 frente, 2 tras, 4 esquerda, 6 direita, 5 neutro
    uint16_t btn;     // kBtn*
    uint32_t frames;
};

struct Shared
{
    std::mutex m;
    std::vector<Sweep> sweeps;   // acumuladas; a janela descarta as velhas
    std::vector<Hit> hits;       // idem
    std::vector<MoveRec> moves;  // ultimos golpes terminados
    std::vector<StunRec> stuns;  // ultimas reacoes terminadas
    std::vector<Hull> hulls;     // do ultimo quadro capturado
    std::vector<Capsule> caps;
    uint32_t frame = 0;          // g_frame da ultima captura
    float health[16] = {}, maxHealth[16] = {};
    int8_t players = 1;          // m_numPlayers (slots 0..players-1 sao jogadores)
    Combo combo;                 // o ultimo (ainda aberto ou terminado)
    float bestCombo = 0.f;       // maior dano de combo desde o ultimo "limpar"
    int bestHits = 0;
    std::vector<PadRec> pad[2];  // historico dos comandos: [0] P1, [1] o primeiro boneco (CPU1); o mais novo no fim
    float stamina[16] = {}, staminaMax[16] = {};
};
inline Shared &shared()
{
    static Shared s;
    return s;
}
}
