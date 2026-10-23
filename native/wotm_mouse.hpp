// wotm_mouse.hpp: mouse na camera moderna (etapa 7 da camera orbital, investigacoes/2026-10-22/camera_orbital_plano.md). Dois lados:
//  - janela (wotm_live.inc, a cada apresentacao): diz qual jogador o teclado alimenta, prende o cursor enquanto a orbita desse
//    jogador esta dirigindo (e a janela tem o foco) e acumula o deslocamento do mouse;
//  - thread do EE (wotm_orbit.inc, depois do Cameras::Update): consome o acumulado na orbita do jogador do teclado e avisa, a cada
//    quadro em que ela dirige fora da pausa, que o cursor pode ficar preso. Sem aviso por ~150 ms (menus, pausa, modo foto, F7),
//    a janela solta o cursor.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace wotm::mouse
{
inline std::atomic<int> g_kbPlayer{0};        // jogador (0..3) que o teclado alimenta; -1 = nenhum (janela escreve)
inline std::atomic<int64_t> g_wantUntil{0};   // ms do relogio monotono ate quando o cursor pode ficar preso (EE escreve)
inline std::atomic<bool> g_captured{false};   // cursor preso agora (janela escreve)
inline std::atomic<int> g_wheel{0};          // passos da rodinha ainda nao usados (+ cima, - baixo; janela soma, teclado consome)
// Troca de alvo pedida pelo teclado (etapa 8: Tab, R, rodinha), por jogador: +1 proximo, -1 anterior. O teclado (wotm_pad.inc) soma
// na borda da tecla, o gancho do interpretInputs (wotm_overrides.cpp) consome com o alvo travado, qualquer que seja o lock-on
inline std::atomic<int> g_targetReq[4] = {};
inline std::mutex g_m;
inline float g_dx = 0.f, g_dy = 0.f;          // pixels acumulados desde o ultimo consumo (protegidos por g_m)

inline int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// EE: a orbita do jogador do teclado dirige neste quadro
inline void want() { g_wantUntil.store(nowMs() + 150, std::memory_order_relaxed); }
inline bool wanted() { return nowMs() < g_wantUntil.load(std::memory_order_relaxed); }
// Janela: deslocamento do quadro (so com o cursor preso)
inline void add(float dx, float dy)
{
    std::lock_guard<std::mutex> lk(g_m);
    g_dx += dx;
    g_dy += dy;
}
// EE: o acumulado (zera)
inline void take(float &dx, float &dy)
{
    std::lock_guard<std::mutex> lk(g_m);
    dx = g_dx;
    dy = g_dy;
    g_dx = g_dy = 0.f;
}
inline void clear()
{
    std::lock_guard<std::mutex> lk(g_m);
    g_dx = g_dy = 0.f;
}
} // namespace wotm::mouse

// Mouse nos menus (etapa 9): com o cursor solto, a janela (wotm_pad.inc, pointerFrame) guarda onde ele esta no quadro nativo e conta
// os cliques; quem desenha um menu nativo no telao (wotm_mainmenu.inc) acha o item sob ele e o gancho da tela, na thread do EE,
// consome os cliques. Menu nativo com itens sob o mouse avisa (claim) que o clique esquerdo e dele; sem aviso, o clique esquerdo vale X
// (confirma o item escolhido) nas telas do jogo.
namespace wotm::mouse
{
struct Pointer
{
    float x = -1.f, y = -1.f;   // no quadro nativo (RW x RH), so com o cursor solto e dentro dele
    int rw = 0, rh = 0;
    bool in = false;
};
inline Pointer g_ptr;                          // so a thread da janela
inline std::atomic<uint32_t> g_moveSeq{0};     // sobe a cada quadro em que o cursor solto andou (o EE so escolhe pelo mouse depois disso)
inline std::atomic<int64_t> g_clickL{0}, g_clickR{0};   // hora (ms) do ultimo clique nao consumido (cursor solto), 0 = nenhum
inline std::atomic<int64_t> g_claimUntil{0};
inline void claim() { g_claimUntil.store(nowMs() + 150, std::memory_order_relaxed); }
inline bool claimed() { return nowMs() < g_claimUntil.load(std::memory_order_relaxed); }
inline void click(std::atomic<int64_t> &c) { c.store(nowMs(), std::memory_order_relaxed); }
// Clique de ate 300 ms atras (um antigo, de fora dos menus, nao vale)
inline bool takeClick(std::atomic<int64_t> &c)
{
    const int64_t t = c.exchange(0, std::memory_order_relaxed);
    return t != 0 && nowMs() - t < 300;
}
} // namespace wotm::mouse
