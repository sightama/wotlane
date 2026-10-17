// Camera livre de debug (F5). Dois lados:
//  - janela (wotm_freecam.inc, thread da apresentacao): teclas e mouse movem a pose, a cada desenho, com o tempo real;
//  - thread do EE (wotm_overrides.cpp): a pose vai para o _cs da vista 0 depois de Cameras::Update e na entrada de viewUpdate,
//    entao o proprio jogo enxerga a camera nova (matrizes, recorte, LOD, particulas, som, controle relativo a camera).
// A pose usa a convencao do jogo (mathfRotationFromVector): heading em torno de Z, 0 = +Y, positivo para +X; pitch para cima;
// sem roll. As linhas da matriz do _cs sao direita, frente e cima.
#pragma once
#include <atomic>
#include <cmath>
#include <mutex>

struct WotmFreeCamPose {
    float pos[3] = {0, 0, 0};
    float heading = 0.f;   // rot.z do _cs
    float pitch = 0.f;     // rot.x do _cs
};

// Eixos da pose (linhas da matriz do _cs, como o mathfRotMatrixRPH com roll 0)
inline void wotmFreeCamAxes(const WotmFreeCamPose &p, float right[3], float fwd[3], float up[3]) {
    const float sh = std::sin(p.heading), ch = std::cos(p.heading), sp = std::sin(p.pitch), cp = std::cos(p.pitch);
    right[0] = ch; right[1] = -sh; right[2] = 0.f;
    fwd[0] = cp * sh; fwd[1] = cp * ch; fwd[2] = sp;
    up[0] = -sp * sh; up[1] = -sp * ch; up[2] = cp;
}

extern std::atomic<bool> g_wotmFreeCam;      // camera livre ligada
extern std::atomic<bool> g_wotmFreeCamPad;   // com ela ligada: o pad volta para o jogo (Tab) e a camera fica parada no teclado
extern std::mutex g_wotmFreeCamM;
extern WotmFreeCamPose g_wotmFreeCamPose;    // protegida por g_wotmFreeCamM
