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
