// wotm_mouse.hpp: mouse na camera moderna (etapa 7 da camera orbital, 75f5be1:docs/investigacoes/2026-10-22/camera_orbital_plano.md). Dois lados:
//  - janela (wotm_live.inc, a cada apresentacao): diz qual jogador o teclado alimenta, prende o cursor enquanto a orbita desse
//    jogador esta dirigindo (e a janela tem o foco) e acumula o deslocamento do mouse;
//  - thread do EE (wotm_orbit.inc, depois do Cameras::Update): consome o acumulado na orbita do jogador do teclado e avisa, a cada
//    quadro em que ela dirige fora da pausa, que o cursor pode ficar preso. Sem aviso por ~150 ms (menus, pausa, modo foto, F7),
//    a janela solta o cursor.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

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

// O ponteiro para a thread do EE (as telas do jogo sabem onde escrevem o texto, em unidades da tela do PS2): x, y no quadro nativo
// (bits de float empacotados) e o tamanho dele; 0 = fora do quadro ou cursor preso
namespace wotm::mouse
{
inline std::atomic<uint64_t> g_ptrXY{0};
inline std::atomic<uint32_t> g_ptrWH{0};   // rw << 16 | rh
inline void publishPointer(bool in, float x, float y, int rw, int rh)
{
    uint32_t a, b;
    std::memcpy(&a, &x, 4);
    std::memcpy(&b, &y, 4);
    g_ptrXY.store(in ? (uint64_t(a) << 32 | b) : 0, std::memory_order_relaxed);
    g_ptrWH.store(uint32_t(rw) << 16 | uint32_t(rh & 0xFFFF), std::memory_order_relaxed);
}
inline bool pointer(float &x, float &y, int &rw, int &rh)
{
    const uint64_t v = g_ptrXY.load(std::memory_order_relaxed);
    const uint32_t wh = g_ptrWH.load(std::memory_order_relaxed);
    if (!v || !wh) return false;
    const uint32_t a = uint32_t(v >> 32), b = uint32_t(v);
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    rw = int(wh >> 16);
    rh = int(wh & 0xFFFF);
    return rw > 0 && rh > 0;
}
// Menu do jogo com itens sob o mouse (pausa e outras telas do jogo): o EE pede um X por algumas leituras do pad (clique num item)
inline std::atomic<int> g_crossReq{0};
// O pedido vale por pouco tempo: o jogo costuma agir ja na primeira leitura (o QUIT da pausa sai da fase), e a segunda, sem menu com
// mouse durante a carga, ficava pendurada ate o menu principal, onde virava um X no item sob o cursor
inline std::atomic<int64_t> g_crossReqUntil{0};
inline void requestCross(int reads)
{
    g_crossReqUntil.store(nowMs() + 200, std::memory_order_relaxed);
    g_crossReq.store(reads, std::memory_order_relaxed);
}
inline std::atomic<int64_t> g_gameMenuUntil{0};   // ate quando (ms) a tela do jogo com mouse esta aberta (a pausa nao e menu do shell)
inline void gameMenu() { g_gameMenuUntil.store(nowMs() + 150, std::memory_order_relaxed); }
inline bool inGameMenu() { return nowMs() < g_gameMenuUntil.load(std::memory_order_relaxed); }

// Telas do jogo que andam para os lados (fase, numero de CPUs, minigame, unlocks): o EE avisa e o teclado (wotm_pad.inc) manda Q/E como
// esquerda/direita; com `wheel`, a rodinha tambem (senao ela continua cima/baixo)
inline std::atomic<int64_t> g_sideUntil{0};
inline std::atomic<bool> g_sideWheel{false};
inline void sideScreen(bool wheel)
{
    g_sideWheel.store(wheel, std::memory_order_relaxed);
    g_sideUntil.store(nowMs() + 150, std::memory_order_relaxed);
}
inline bool sideActive() { return nowMs() < g_sideUntil.load(std::memory_order_relaxed); }
// Lista de saves aberta (EE, wotm_save.inc): 1 carregar, 2 gravar, 3 confirmar a troca; a janela poe a legenda de baixo
inline std::atomic<int64_t> g_saveListUntil{0};
inline std::atomic<int> g_saveListMode{0};
inline void saveList(int mode)
{
    g_saveListMode.store(mode, std::memory_order_relaxed);
    g_saveListUntil.store(nowMs() + 150, std::memory_order_relaxed);
}
inline int saveListMode() { return nowMs() < g_saveListUntil.load(std::memory_order_relaxed) ? g_saveListMode.load(std::memory_order_relaxed) : 0; }
// Nome do botao de confirmar no dispositivo de agora ("ENTER", "A", "CROSS"...), para o texto do jogo (a janela escreve)
inline std::mutex g_lblM;
inline std::string g_confirmLabel = "X";
inline std::string confirmLabel()
{
    std::lock_guard<std::mutex> lk(g_lblM);
    return g_confirmLabel;
}
} // namespace wotm::mouse

// O ponteiro em unidades da tela do PS2 (640 x 224, as do texto do jogo), publicado pela janela junto da apresentacao
namespace wotm::mouse
{
inline std::atomic<uint64_t> g_gsXY{0};
inline std::atomic<bool> g_gsIn{false};
inline void publishGs(bool in, float gx, float gy)
{
    uint32_t a, b;
    std::memcpy(&a, &gx, 4);
    std::memcpy(&b, &gy, 4);
    g_gsXY.store(uint64_t(a) << 32 | b, std::memory_order_relaxed);
    g_gsIn.store(in, std::memory_order_relaxed);
}
inline bool gsPointer(float &gx, float &gy)
{
    if (!g_gsIn.load(std::memory_order_relaxed)) return false;
    const uint64_t v = g_gsXY.load(std::memory_order_relaxed);
    const uint32_t a = uint32_t(v >> 32), b = uint32_t(v);
    std::memcpy(&gx, &a, 4);
    std::memcpy(&gy, &b, 4);
    return true;
}
} // namespace wotm::mouse
